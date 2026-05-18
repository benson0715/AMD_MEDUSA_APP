/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dbgRw.h"
#include <zephyr/sys/reboot.h>
#include "gpio_ec.h"
#include "board_config.h"
#include <zephyr/logging/log.h>
#include "f_bram.h"
#if CONFIG_FAN_RPM_CONTROL
#include "app_fanRpmCtrl.h"
#endif

#define __DBGRW_SWAP_U16(x)  ((((x) & 0xFF) << 8) | (((x) & 0xFF00) >> 8))
#define __DBGRW_SWAP_U32(x)  (__DBGRW_SWAP_U16((((x) >> 16) & 0xFFFF)) | (__DBGRW_SWAP_U16((x) & 0xFFFF)<< 16))

typedef union {
	uint8_t u8Ctrl;
	union {
		struct {
			/* if accArmReg is 1 */			
			uint8_t grpKick     : 6;    // 1 of bit 0 to 5 stands for group 0 to 5 is triggered respectively

			uint8_t isRead      : 1;    // bit 6: This is used only if accArmReg is 1.
			uint8_t accArmReg   : 1;    // bit 7: 1 to use 32-bit address mode so as to access all possible register space
			                            //        0 to use 24-bit address mode but appending 0x40 in the highest bytes, so this is for MEC170x registers
		} ArmReg;
		struct {
			/* if accArmReg is 0 */
			uint8_t trigger     : 1;    // bit 0
			uint8_t EcReset     : 6;
			uint8_t accArmReg   : 1;    // bit 7: See ArmReg.accArmReg
		} EcReg;
	} b;
} DGBRW_CTRL_REG;

typedef struct {
	union {
		struct {
			uint32_t width      : 2;    // 0-4_Bytes; 1-1_Bytes; 2-2_Bytes access
			uint32_t isRead     : 1;
			uint32_t grpEn      : 1;
			uint32_t doneFlag   : 4;

			uint32_t u24a       : 24;   // 24-bit address
		} c;
		uint32_t u32a;                // 32-bit address
	} CtrlAddr;
	uint32_t u32data;
} DBGRW_ONE_ITEM;

static DGBRW_CTRL_REG _s_dbgCtrl = {0};
static DBGRW_ONE_ITEM _s_dbgItems[DBGRW_MAX_ITEM_NUM];

/*****************************************************************
 * Quick reference:
 *
 * EC registers access (24-bit addr mode):
 *       Group 0                 | Group 1
 *  Idx: 00 01 02 03 04 05 06 07 | 08 09 0A 0B 0C 0D 0E 0F
 * Data: 0C 0F 9C 04 XX YY ZZ MM | 09 00 A8 10 xx xx AB CD
 *
 * Set ECRAMx30 = 0x7F triggers all of the 6 group from ECRAMx00 to ECRAMx2F
 * Where Group 0/1 are enabled as below:
 *   Group 0 := 32'b{XX,YY,ZZ,MM} = MMCR_32b (0x40000000 + 0x000F9C04)
 *   Group 1 := MMCR_16b (0x40000000 + 0x0000A810) = 16'b{AB,CD}
 *
 * ARM registers access (32-bit addr mode):
 *       Group 0                 | Group 1
 *  Idx: 00 01 02 03 04 05 06 07 | 08 09 0A 0B 0C 0D 0E 0F
 * Data: E0 00 E0 18 XX YY ZZ MM | .. .. .. .. .. .. .. ..
 *
 * Set ECRAMx30 = 0xC1 triggers Group 0 for read, i.e.
 *
 * Group 0 := 32'b{XX,YY,ZZ,MM} = MMCR_32b (0xE000E018)
 *         >> System Tick current Value (SysTick->VAL) <<
 *****************************************************************/

/**
 * @brief Handle debug read/write control register write operation
 *
 * @param u8Ctrl Control byte value written to the debug control register
 */
void _dbgRw_onCtrlWrite(uint8_t u8Ctrl)
{
	_s_dbgCtrl.u8Ctrl = u8Ctrl;

	if (u8Ctrl & 0x80) {
		/* 32-bit address mode */
		for (uint8_t i = 0; i < DBGRW_MAX_ITEM_NUM; i++) {
			if (u8Ctrl & (0x01 << i)) {
				uint32_t addr = __DBGRW_SWAP_U32(_s_dbgItems[i].CtrlAddr.u32a);
				uint32_t data = 0xFFFFFFFF;
				if (_s_dbgCtrl.b.ArmReg.isRead) {
					data = *((volatile uint32_t *) addr);
					_s_dbgItems[i].u32data = __DBGRW_SWAP_U32(data);
				} else {
					data = __DBGRW_SWAP_U32(_s_dbgItems[i].u32data);
					*((volatile uint32_t *) addr) = data;
				}
				_s_dbgCtrl.u8Ctrl &= (~(0x01 << i));
			}
		}
	} else {
		if (_s_dbgCtrl.b.EcReg.EcReset == 0x22) {
			/* G3 system reset Bios ROM */
#if CONFIG_FAN_RPM_CONTROL
			app_fan_ctrl_sys_disable();
#endif
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
			gpio_write_pin(EC_SLP_S5_N, 0);
			k_busy_wait(20000);
			gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#else
			gpio_write_pin(EC_SLP_S5_N, 0);
			gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#endif

#ifdef EC_S0_LED
			gpio_write_pin(EC_S0_LED, 0);
#endif
#ifdef ex_EP_SB_TSI_ESP32n_EC_SEL
			ioexp_set(ex_EP_SB_TSI_ESP32n_EC_SEL, 0);
#endif
			gpio_write_pin(RSMRST_N, 0);
			k_busy_wait(10000);
#ifdef EC_USB32_RD_EN
			gpio_write_pin(EC_USB32_RD_EN, 0);
#endif
#ifdef EC_USB32_RD_RST_N
			gpio_write_pin(EC_USB32_RD_RST_N, 0);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			ioexp_set(ex_EP_SENSOR_3V3_PWREN, 0);
#endif

			gpio_write_pin(EC_1V8_S5_EN, 0);

			k_busy_wait(20000);

			gpio_write_pin(EC_S5_PWREN, 0);
			f_bram_setSig(F_BRAM_FLASH_BIOS_WDT_BOOT_SIGNATURE);
			k_busy_wait(10000);
			sys_reboot(SYS_REBOOT_COLD);
		}
	
		if (!_s_dbgCtrl.b.EcReg.trigger)
			return;
		/* 24-bit address mode */
		static uint8_t accessDoneFlag = 0;
		uint8_t incFlag = 1;
		for (uint8_t i = 0; i < DBGRW_MAX_ITEM_NUM; i++) {
			uint32_t addr = __DBGRW_SWAP_U32(_s_dbgItems[i].CtrlAddr.c.u24a) >> 8;
			addr &= 0x00FFFFFF;
			addr |= 0x40000000;
			uint32_t data = __DBGRW_SWAP_U32(_s_dbgItems[i].u32data);

			if (_s_dbgItems[i].CtrlAddr.c.grpEn) {
				if (incFlag) {
					incFlag = 0;
					accessDoneFlag ++;
				}
				switch (_s_dbgItems[i].CtrlAddr.c.width) {
					case 1:  // 1-byte
						if (_s_dbgItems[i].CtrlAddr.c.isRead)
							data = *((volatile uint8_t *) addr);
						else
							*((volatile uint8_t *) addr) = data;
						_s_dbgItems[i].CtrlAddr.c.doneFlag = (accessDoneFlag & 0x0F);
						break;
					case 2:  // 2-byte/word
						if (_s_dbgItems[i].CtrlAddr.c.isRead)
							data = *((volatile uint16_t *) addr);
						else
							*((volatile uint16_t *) addr) = data;
						_s_dbgItems[i].CtrlAddr.c.doneFlag = (accessDoneFlag & 0x0F);
						break;
					case 0:  // 4-byte/dword
						if (_s_dbgItems[i].CtrlAddr.c.isRead)
							data = *((volatile uint32_t *) addr);
						else
							*((volatile uint32_t *) addr) = data;
						_s_dbgItems[i].CtrlAddr.c.doneFlag = (accessDoneFlag & 0x0F);
						break;
					case 3:
					default:
						break;
				}

				if (_s_dbgItems[i].CtrlAddr.c.isRead) {
					_s_dbgItems[i].u32data = __DBGRW_SWAP_U32(data);
				}
			}
		}
		_s_dbgCtrl.b.EcReg.trigger = 0;
	}
}

/**
 * @brief ACPI handler for debug read/write operations
 *
 * @param isRead Non-zero for read operation, zero for write operation
 * @param ui8Idx ACPI index offset
 * @param pui8Data Pointer to data byte for read/write
 * @return 1 if handled, 0 if not handled
 */
uint8_t dbgRw_acpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx == DBGRW_ACPI_CTRL_OFFSET) {
		if (isRead) {
			*pui8Data = _s_dbgCtrl.u8Ctrl;
		} else {
			_dbgRw_onCtrlWrite(*pui8Data);
		}

		return 1;
	} else if (ui8Idx >= DBGRW_ACPI_SPACE_BASE && 
	           ui8Idx <  DBGRW_ACPI_SPACE_BASE + (DBGRW_MAX_ITEM_NUM * sizeof(DBGRW_ONE_ITEM))) {

		uint8_t * pByte = (uint8_t *)_s_dbgItems;
		ui8Idx -= DBGRW_ACPI_SPACE_BASE;

		if (isRead) {
			*pui8Data = pByte[ui8Idx];
		} else {
			pByte[ui8Idx] = *pui8Data;
		}

		return 1;
	}

	return 0;
}