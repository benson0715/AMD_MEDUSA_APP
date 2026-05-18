/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <assert.h>
#include <soc.h>
#include "i2c_hub.h"
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "board_config.h"
#include "f_nv_options.h"
#include "board_id.h"
#include "dev_sbtsi.h"
#include "app_acpi.h"
#include "dev_lm95234.h"
#include "board_thermal.h"
#include "espi_hub.h"
LOG_MODULE_REGISTER(board_thermal, CONFIG_BOARD_THERMAL_LOG_LEVEL);

/*
 * APML will be disabled aftr APML_ERROR_NUM_THRESHOLD continues fail.
 * The counter g_u32ApmlErrCnt is reset by APU_RST.
 *
 * APML mailbox requests will be hold for APML_MB_DELAY_SECOND second after APU_RST
 */
uint32_t g_u32ApmlErrCnt = 0;

/* ************************** *
 *     global valuable        *
 * ************************** */
uint16_t _s_u16pcbTmp, _s_u16pcbTmpQ1 /* Q87 */, _s_u16pcbTmpQ2	/* Q47 */;

uint16_t _s_u16dieTmp;
uint16_t _s_u16evalSensor;

/************************************
 *       Eval card sensor           *
 ************************************/
#define CMD_LD_ADDR 0x01
#define CMD_WR_DATA 0x02
#define CMD_RD_DATA 0x03

/**
 * board_thermal_info 
 *
 * @param [in]   BoardTmp;
 *
 * @note
 *  Get board thermal information
 */
void board_thermal_getinfo (board_thermal_info * BoardTmp)
{
	//
	// Read on-board sensors
	//
	BoardTmp->_s_u16pcbTmp = dev_lm95234_read_temp(DEV_LM95234_SENSOR_LOCAL, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQ[0] = dev_lm95234_read_temp(DEV_LM95234_SENSOR_REMOT1, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQ[1] = dev_lm95234_read_temp(DEV_LM95234_SENSOR_REMOT2, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQ2[0] = dev_lm95234_read_temp(DEV_LM95234_SENSOR_REMOT3, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQ2[1] = dev_lm95234_read_temp(DEV_LM95234_SENSOR_REMOT4, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQMax = 2;
	_s_u16pcbTmp = BoardTmp->_s_u16pcbTmp;
	_s_u16pcbTmpQ1 = BoardTmp->_s_u16pcbTmpQ[0];
	_s_u16pcbTmpQ2 = BoardTmp->_s_u16pcbTmpQ[1];

	/* Skip SB-TSI in Zx */
	if (!espihub_socInLP()) {
		_s_u16dieTmp = dev_sbtsi_getTemp16();
	}
	/*
	 * remote1: VR
	 * remote2: APU MEM
	 * remote3: charge
	 * remote4: RT
	 *  */
	LOG_DBG("Die: %2d.%03d, Local %2d.%03d, VR %2d.%03d, ApuMem %2d.%03d, Charge %2d.%03d, RT %2d.%03d",
			_GET_INT_FROM_U16(_s_u16dieTmp), (int)_GET_DEC_FROM_U16(_s_u16dieTmp),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmp), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmp),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ[0]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ[0]),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ[1]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ[1]),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[0]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[0]),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[1]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[1])
			);
}

/**
 * SetThemalResistanceCorrection 
 *
 * @param [in]   BoardTmp;
 *
 * @note
 *  Set Themal Resistance Correction
 */
bool board_thermal_setResistanceCorrection(bool toEnable)
{
	bool ret = 0;
	dev_lm95234_init(LM95234_I2C_PORT,LM95234_I2C_ADDRESS);
	return ret;
}

/**
 * @brief ACPI handler for the thermal related data2
 *
 * @param     isRead:    True is read and false is write
 * @param     ui8Idx:    index
 * @param     pui8Data:  data buffer
 */
void ACPIThermalHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t _b_pcbTmp, _b_pcbTmpQ1 /* Q87 */, _b_pcbTmpQ2	/* Q47 */;
	static uint16_t _b_u16dieTmp;
	static uint16_t _b_u16evalSensor;
	uint8_t ui8Index;
	
	ui8Index = ui8Idx - ACPI_THERMAL;

	/*
	 * Read high byte lock the 16-bit number.
	 *
	 * Bytes
	 *   1/0 - die temp from SB-TSI
	 *   3/2 - TMP432 local sensor
	 *   5/4 - TMP432 remote 1 (Q87)
	 *   7/6 - TMP432 remote 2 (Q47)
	 *   9/8 - dGPU temp
	 */
	if (isRead) {
		switch (ui8Index) {
			case 1:
				_b_u16dieTmp = _s_u16dieTmp;

				*pui8Data = (_b_u16dieTmp >> 8) & 0xFF;
				break;
			case 0:
				*pui8Data = _b_u16dieTmp & 0xFF;
				break;

			case 3:
				_b_pcbTmp = _s_u16pcbTmp;

				*pui8Data = (_b_pcbTmp >> 8) & 0xFF;
				break;
			case 2:
				*pui8Data = _b_pcbTmp & 0xFF;
				break;

			case 5:
				_b_pcbTmpQ1 = _s_u16pcbTmpQ1;

				*pui8Data = (_b_pcbTmpQ1 >> 8) & 0xFF;
				break;
			case 4:
				*pui8Data = _b_pcbTmpQ1 & 0xFF;
				break;

			case 7:
				_b_pcbTmpQ2 = _s_u16pcbTmpQ2;

				*pui8Data = (_b_pcbTmpQ2 >> 8) & 0xFF;
				break;
			case 6:
				*pui8Data = _b_pcbTmpQ2 & 0xFF;
				break;

			case 9:
				_b_u16evalSensor = _s_u16evalSensor;

				*pui8Data = (_b_u16evalSensor >> 8) & 0xFF;
				break;
			case 8:
				*pui8Data = _b_u16evalSensor & 0xFF;
				break;

			default:
				break;
		}
	}
}

/*
 * Runs by PURMAIN thread.
 */
/**
 * @brief ACPI handler for the thermal related data1
 *
 * @param     isRead:    True is read and false is write
 * @param     ui8Idx:    index
 * @param     pui8Data:  data buffer
 */
uint8_t board_thermal_AcpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t _a_pcbTmp, _a_pcbTmpQ1 /* Q87 */, _a_pcbTmpQ2	/* Q47 */;
	static uint16_t _a_u16dieTmp;
	static uint16_t _a_u16evalSensor;

	/*
	 * Read high byte lock the 16-bit number.
	 *
	 * Bytes
	 *   1/0 - die temp from SB-TSI
	 *   3/2 - TMP432 local sensor
	 *   5/4 - TMP432 remote 1 (Q87)
	 *   7/6 - TMP432 remote 2 (Q47)
	 *   9/8 - dGPU temp
	 */
	if (isRead) {
		switch (ui8Idx) {
			case 1:
				_a_u16dieTmp = _s_u16dieTmp;

				*pui8Data = (_a_u16dieTmp >> 8) & 0xFF;
				break;
			case 0:
				*pui8Data = _a_u16dieTmp & 0xFF;
				break;

			case 3:
				_a_pcbTmp = _s_u16pcbTmp;

				*pui8Data = (_a_pcbTmp >> 8) & 0xFF;
				break;
			case 2:
				*pui8Data = _a_pcbTmp & 0xFF;
				break;

			case 5:
				_a_pcbTmpQ1 = _s_u16pcbTmpQ1;

				*pui8Data = (_a_pcbTmpQ1 >> 8) & 0xFF;
				break;
			case 4:
				*pui8Data = _a_pcbTmpQ1 & 0xFF;
				break;

			case 7:
				_a_pcbTmpQ2 = _s_u16pcbTmpQ2;

				*pui8Data = (_a_pcbTmpQ2 >> 8) & 0xFF;
				break;
			case 6:
				*pui8Data = _a_pcbTmpQ2 & 0xFF;
				break;

			case 9:
				_a_u16evalSensor = _s_u16evalSensor;

				*pui8Data = (_a_u16evalSensor >> 8) & 0xFF;
				break;
			case 8:
				*pui8Data = _a_u16evalSensor & 0xFF;
				break;

			default:
				return 0;
		}

		return 1;
	}

	return 0;
}

/**
 * board_thermal_reporter 
 *
 * @param [in]   BoardTmp;
 *
 * @note
 *  Report board thermal information
 */
void board_thermal_reporter(board_thermal_info       BoardTmp)
{
	uint8_t reportOnboardTemp;
	/* SB-RMI address
	 *    (0x78 >> 1) - Socket ID / Package number 0
	 *    (0x70 >> 1) - Socket ID / Package number 1
	 */
	GET_NV_OPTIONS(thm_uploadOnboardTemp, reportOnboardTemp);
	if (reportOnboardTemp && g_u32ApmlErrCnt < APML_ERROR_NUM_THRESHOLD) {
		for(int TmpQn=0;TmpQn<BoardTmp._s_u16pcbTmpQMax;TmpQn++)
		{
			/* Skip SB-TSI in Zx */
			if (!espihub_socInLP()) {
				if (!dev_sbi_writeSttSensor(SBRMI_SLV_ADDRESS_PKG0, TmpQn, BoardTmp._s_u16pcbTmpQ[TmpQn])) {
					g_u32ApmlErrCnt++;
				}
			}
		}
	}
}