/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "fwSt.h"
#include "f_romSig.h"
#include "acpiplanes.h"

/* ************************** *
 *       Static Valuable      *
 * ************************** */
static uint8_t _s_pageSelector = 0;

/* ************************** *
 *       Global Valuable      *
 * ************************** */
#if CONFIG_USBC_DRIVERS
extern PD_VR_CTRL_STATUS g_pdCtrlSt[2];  /* PDFW version for two chips */
#endif
extern bool g_allow_BiosInfo_MacAddr_Report;
/* extern value for error status */
extern uint32_t g_ui32_pwrBtnMissCnt;
extern uint32_t g_u32_vw_SlpMscCounter;
extern uint32_t g_u32_vw_SocLPCounter;
extern uint32_t g_u32_vw_SMIOUTnCounter;
extern uint32_t g_u32_vw_NMIOUTnCounter;
extern uint32_t g_ioExpErrCnt;
#if CONFIG_APP_THERMAL
extern uint32_t g_u32ApmlErrCnt;
#endif
uint8_t g_u8Pd0Slot = 0xFF;
uint8_t g_u8Pd1Slot = 0xFF;

uint32_t g_u32ThermalTripCnt = 0;
uint32_t g_u32ApuRstCnt = 0;
uint32_t g_u32ChgProchotLCnt = 0;

uint8_t g_biosVer[36] = {0};
uint8_t g_nicMacAddr[6] = {0};
uint8_t g_cpuId[16];
uint8_t g_cpuOpn[32];
uint8_t g_cpuSn[8] = {0};
bool biosVer_ready;
bool nicMacAddr_ready;
bool cpuId_ready;
bool cpuOpn_ready;
/* ************************** *
 *       External Handlers    *
 * ************************** */
extern uint8_t board_pwrSrc_fwStHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

/* ************************** *
 *      Forward declaration   *
 * ************************** */
uint8_t fwSt_EcSig_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t fwSt_PdVersion1_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t fwSt_PdVersion2_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t fwSt_Statistic_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t fwSt_MacAddr_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t fwSt_BiosVer_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t fwSt_CpuId_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

/* ************************** *
 *       Module functions     *
 * ************************** */

/**
 * @brief Initialize firmware status module and register ACPI plane handlers
 */
void fwSt_init()
{
	// register planes
	md_acpiplanes_register_fun(fwSt_EcSig_Handler,       0xD0);
	md_acpiplanes_register_fun(fwSt_PdVersion1_Handler,  0xD1);
	md_acpiplanes_register_fun(fwSt_Statistic_Handler,   0xD2);
#if CONFIG_POWER_SOURCE_MANAGEMENT
	md_acpiplanes_register_fun(board_pwrSrc_fwStHandler, 0xD3);
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */
	md_acpiplanes_register_fun(fwSt_PdVersion2_Handler,  0xD4);
	md_acpiplanes_register_fun(fwSt_MacAddr_Handler,     0xD5);
	md_acpiplanes_register_fun(fwSt_BiosVer_Handler,     0xD6);
	md_acpiplanes_register_fun(fwSt_CpuId_Handler,       0xD7);
}

/*
 * 0x00 ~ 0x20
 * {Offset/Size/Checksum} x 4; totally 32 bytes
 * 0x20 - 'V' if rom sig 0 is valid
 * 0x21 - 'V' if rom sig 1 is valid
 * 0x22 - 'V' if rom sig 2 is valid
 * 0x23 - 'V' if rom sig 3 is valid
 */
uint8_t fwSt_EcSig_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{


	extern F_ROMSIG_BLOCK g_xRomSig;
	if (isRead) {
		if ( ui8Idx < sizeof(g_xRomSig) && ui8Idx < 0x20 ) {
			*pui8Data = g_xRomSig.buf[ui8Idx];
		} else {
			switch (ui8Idx) {
				case 0x20:
				case 0x21:
				case 0x22:
				case 0x23:
					*pui8Data = f_romSig_isValid(ui8Idx & 0x03) ? 'V' : 0;
					break;
				default:
					break;
			}
		}
	}

	return 1;
}

/*
 * 4 bytes MODE:    "APP", "BOOT", "PTCH"
 * 4 bytes Version: VV.MM.RR.ii
 * 4 bytes : "SKIP" or "LOAD"
 * 2 bytes to indicate load fw status "OK" or "NG"
 */
uint8_t fwSt_PdVersion1_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
#if CONFIG_USBC_DRIVERS
	if (isRead) {
		switch ( ui8Idx ) {
			/* Mode */
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
				*pui8Data = g_pdCtrlSt[0].mode[ui8Idx];
				break;
			/* FW Version */
			case 0x04:
				*pui8Data = g_pdCtrlSt[0].VVVV;
				break;
			case 0x05:
				*pui8Data = g_pdCtrlSt[0].MM;
				break;
			case 0x06:
				*pui8Data = g_pdCtrlSt[0].RR;
				break;
			/* Customer version */
			case 0x07:
				*pui8Data = g_pdCtrlSt[0].EE;
				break;
			case 0x08:
				*pui8Data = g_pdCtrlSt[0].Custom[2];
				break;
			case 0x09:
				*pui8Data = g_pdCtrlSt[0].Custom[1];
				break;
			case 0x0A:
				*pui8Data = g_pdCtrlSt[0].Custom[0];
				break;
			/* SKU */
			case 0x0B:
				*pui8Data = 'S';
				break;
			case 0x0C:
				*pui8Data = 'K';
				break;
			case 0x0D:
				*pui8Data = 'U';
				break;
			case 0x0E:
				*pui8Data = g_pdCtrlSt[0].sku[0];
				break;
			case 0x0F:
				*pui8Data = g_u8Pd0Slot;
				break;
			/* FW status */
			case 0x10:
				*pui8Data = g_pdCtrlSt[0].isFwLoadSkipped ? 'S' : 'L';
				break;
			case 0x11:
				*pui8Data = g_pdCtrlSt[0].isFwLoadSkipped ? 'K' : 'O';
				break;
			case 0x12:
				*pui8Data = g_pdCtrlSt[0].isFwLoadSkipped ? 'I' : 'A';
				break;
			case 0x13:
				*pui8Data = g_pdCtrlSt[0].isFwLoadSkipped ? 'P' : 'D';
				break;
			case 0x14:
				*pui8Data = g_pdCtrlSt[0].isFwLoadSuccess ? 'O' : 'N';
				break;
			case 0x15:
				*pui8Data = g_pdCtrlSt[0].isFwLoadSuccess ? 'K' : 'G';
				break;
			/* mini-type */
			case 0x16:
			case 0x17:
			case 0x18:
			case 0x19:
				*pui8Data = g_pdCtrlSt[0].type[ui8Idx - 0x16];
				break;
			/* re-timer */
			case 0x1A:
			case 0x1B:
			case 0x1C:
			case 0x1D:
			case 0x1E:
			case 0x1F:
			case 0x20:
			case 0x21:
				*pui8Data = g_pdCtrlSt[0].retimer[ui8Idx - 0x1A];
				break;
			case 0x22:
			case 0x23:
			case 0x24:
			case 0x25:
			case 0x26:
			case 0x27:
			case 0x28:
			case 0x29:
				*pui8Data = g_pdCtrlSt[0].ParadeVer[ui8Idx - 0x22];
				break;
			default: 
				break;
		}
	}
#endif
	return 1;
}

/*
 * 4 bytes MODE:    "APP", "BOOT", "PTCH"
 * 4 bytes Version: VV.MM.RR.ii
 * 4 bytes : "SKIP" or "LOAD"
 * 2 bytes to indicate load fw status "OK" or "NG"
 */
uint8_t fwSt_PdVersion2_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
#if CONFIG_USBC_DRIVERS
	if (isRead) {
		switch ( ui8Idx ) {
			/* Mode */
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
				*pui8Data = g_pdCtrlSt[1].mode[ui8Idx];
				break;
			/* FW Version */
			case 0x04:
				*pui8Data = g_pdCtrlSt[1].VVVV;
				break;
			case 0x05:
				*pui8Data = g_pdCtrlSt[1].MM;
				break;
			case 0x06:
				*pui8Data = g_pdCtrlSt[1].RR;
				break;
			/* Customer version */
			case 0x07:
				*pui8Data = g_pdCtrlSt[1].EE;
				break;
			case 0x08:
				*pui8Data = g_pdCtrlSt[1].Custom[2];
				break;
			case 0x09:
				*pui8Data = g_pdCtrlSt[1].Custom[1];
				break;
			case 0x0A:
				*pui8Data = g_pdCtrlSt[1].Custom[0];
				break;
			/* SKU */
			case 0x0B:
				*pui8Data = 'S';
				break;
			case 0x0C:
				*pui8Data = 'K';
				break;
			case 0x0D:
				*pui8Data = 'U';
				break;
			case 0x0E:
				*pui8Data = g_pdCtrlSt[1].sku[0];
				break;
			case 0x0F:
				*pui8Data = g_u8Pd1Slot;
				break;
			/* FW status */
			case 0x10:
				*pui8Data = g_pdCtrlSt[1].isFwLoadSkipped ? 'S' : 'L';
				break;
			case 0x11:
				*pui8Data = g_pdCtrlSt[1].isFwLoadSkipped ? 'K' : 'O';
				break;
			case 0x12:
				*pui8Data = g_pdCtrlSt[1].isFwLoadSkipped ? 'I' : 'A';
				break;
			case 0x13:
				*pui8Data = g_pdCtrlSt[1].isFwLoadSkipped ? 'P' : 'D';
				break;
			case 0x14:
				*pui8Data = g_pdCtrlSt[1].isFwLoadSuccess ? 'O' : 'N';
				break;
			case 0x15:
				*pui8Data = g_pdCtrlSt[1].isFwLoadSuccess ? 'K' : 'G';
				break;
			/* mini-type */
			case 0x16:
			case 0x17:
			case 0x18:
			case 0x19:
				*pui8Data = g_pdCtrlSt[1].type[ui8Idx - 0x16];
				break;
			/* re-timer */
			case 0x1A:
			case 0x1B:
			case 0x1C:
			case 0x1D:
			case 0x1E:
			case 0x1F:
			case 0x20:
			case 0x21:
				*pui8Data = g_pdCtrlSt[1].retimer[ui8Idx - 0x1A];
				break;
			case 0x22:
			case 0x23:
			case 0x24:
			case 0x25:
			case 0x26:
			case 0x27:
			case 0x28:
			case 0x29:
				*pui8Data = g_pdCtrlSt[1].ParadeVer[ui8Idx - 0x22];
				break;
			default: 
				break;
		}
	}
#endif
	return 1;
}

/**
 * @brief Handler for MAC address ACPI plane read/write operations
 */
uint8_t fwSt_MacAddr_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	/* set the page selector from 0x30. While the date is from 0x00 to 0x2F */
	if (ui8Idx == 0x30) {
		if (isRead) {
			*pui8Data = _s_pageSelector;
		} else {
			_s_pageSelector = *pui8Data;
		}
		return 1;
	} else {
		uint8_t offset = _s_pageSelector * 0x30 + ui8Idx;
		if (offset < sizeof(g_nicMacAddr)) {
			if (isRead) {
				*pui8Data = g_nicMacAddr[offset];
			} else {
				g_nicMacAddr[offset] = *pui8Data;
				if (offset == (sizeof(g_nicMacAddr) - 1)) {
					nicMacAddr_ready = true;
					if (biosVer_ready & nicMacAddr_ready & cpuId_ready & cpuOpn_ready) {
						/* clear the write enable after any write. Make sure
						 * Bios only report one time after system power on
						 */
						g_allow_BiosInfo_MacAddr_Report = false;
						extern void board_init_updateEcInfoEventTrigger(
							void);
						board_init_updateEcInfoEventTrigger();
					}
				}
			}
			return 1;
		} else {
			return 0;
		}
	}
	return 1;
}

/**
 * @brief Handler for BIOS version ACPI plane read/write operations
 */
uint8_t fwSt_BiosVer_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{

	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	/* set the page selector from 0x30. While the date is from 0x00 to 0x2F */
	if (ui8Idx == 0x30) {
		if (isRead) {
			*pui8Data = _s_pageSelector;
		} else {
			_s_pageSelector = *pui8Data;
		}
		return 1;
	} else {
		uint8_t offset = _s_pageSelector * 0x30 + ui8Idx;
		if (offset < sizeof(g_biosVer)) {
			if (isRead) {
				*pui8Data = g_biosVer[offset];
			} else {
				g_biosVer[offset] = *pui8Data;
				if (offset == (sizeof(g_biosVer) - 1)) {
					biosVer_ready = true;
					/* clear the write enable after any write. Make sure Bios
					 * only report one time after system power on */
					if (biosVer_ready & nicMacAddr_ready & cpuId_ready & cpuOpn_ready) {
						g_allow_BiosInfo_MacAddr_Report = false;
						extern void board_init_updateEcInfoEventTrigger(
							void);
						board_init_updateEcInfoEventTrigger();
					}
				}
			}
			return 1;
		} else {
			return 0;
		}
	}
	return 1;
}

/**
 * @brief Handler for CPU ID and related info ACPI plane read/write operations
 */
uint8_t fwSt_CpuId_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	/* set the page selector from 0x30. While the date is from 0x00 to 0x2F */
	if (ui8Idx == 0x30) {
		if (isRead) {
			*pui8Data = _s_pageSelector;
		} else {
			_s_pageSelector = *pui8Data;
		}
		return 1;
	} else {
		uint8_t offset = _s_pageSelector * 0x30 + ui8Idx;
		if (offset < sizeof(g_cpuOpn)) {
			if (isRead) {
				*pui8Data = g_cpuOpn[offset];
			} else {
				g_cpuOpn[offset] = *pui8Data;
				if (offset == (sizeof(g_cpuOpn) - 1)) {
					cpuOpn_ready = true;
					/* clear the write enable after any write. Make sure Bios
					 * only report one time after system power on */
					if (biosVer_ready & nicMacAddr_ready & cpuId_ready & cpuOpn_ready) {
						g_allow_BiosInfo_MacAddr_Report = false;
						extern void board_init_updateEcInfoEventTrigger(
							void);
						board_init_updateEcInfoEventTrigger();
					}
				}
			}
			return 1;
		} else if ((offset >= 0x20) && (offset <= 0x2F)) {
			g_cpuId[offset - 0x20] = *pui8Data;
			return 1;
		} else if ((offset >= 0x30) && (offset <= 0x37)) {
			/* Page 1: bytes 0-7 for Serial Number (SN) */
			if (isRead) {
				*pui8Data = g_cpuSn[offset - 0x30];
			} else {
				g_cpuSn[offset - 0x30] = *pui8Data;
				if (offset == 0x37) {
					cpuId_ready = true;
					if (biosVer_ready & nicMacAddr_ready & cpuId_ready & cpuOpn_ready) {
						/* clear the write enable after any write. Make sure
						 * Bios only report one time after system power on
						 */
						g_allow_BiosInfo_MacAddr_Report = false;
						extern void board_init_updateEcInfoEventTrigger(void);
						board_init_updateEcInfoEventTrigger();
					}
				}
			}
			return 1;
		} else {
			return 0;
		}
	}
	return 1;

}

#if 1
/*
 * 4 bytes little-endian g_u32ApuRstCnt
 * 4 bytes little-endian g_u32ThermalTripCnt
 * 4 bytes little-endian g_ui32PwrBtnMissCnt
 * 4 bytes little-endian g_u32ChgProchotLCnt
 * 4 bytes little-endian g_u32_vw_SlpMscCounter
 * 4 bytes little-endian g_u32_vw_SocLPCounter
 * 4 bytes little-endian 0
 * 4 bytes little-endian 0
 * 4 bytes little-endian g_u32_vw_SMIOUTnCounter 
 * 4 bytes little-endian g_u32_vw_NMIOUTnCounter
 * 4 bytes little-endian g_ioExpErrCnt
 * 4 bytes little-endian g_u32ApmlErrCnt
 */
uint8_t fwSt_Statistic_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (isRead) {
		switch ( ui8Idx ) {
			case 0: case 1: case 2: case 3:
				*pui8Data = 0xFF & (g_u32ApuRstCnt          >> (8 * ui8Idx));
				break;
			case 4: case 5: case 6: case 7:
				*pui8Data = 0xFF & (g_u32ThermalTripCnt     >> (8 * (ui8Idx - 4)));
				break;
			case 8: case 9: case 10: case 11:
				*pui8Data = 0xFF & (g_ui32_pwrBtnMissCnt    >> (8 * (ui8Idx - 8)));
				break;
			case 12: case 13: case 14: case 15:
				*pui8Data = 0xFF & (g_u32ChgProchotLCnt     >> (8 * (ui8Idx - 12)));
				break;
			case 16: case 17:	case 18: case 19:
				*pui8Data = 0xFF & (g_u32_vw_SlpMscCounter  >> (8 * (ui8Idx - 16)));
				break;
			case 20: case 21: case 22: case 23:
				*pui8Data = 0xFF & (g_u32_vw_SocLPCounter   >> (8 * (ui8Idx - 20)));
				break;
			case 24: case 25: case 26: case 27:
				*pui8Data = 0xFF & (0   >> (8 * (ui8Idx - 24)));
				break;
			case 28: case 29: case 30: case 31:
				*pui8Data = 0xFF & (0  >> (8 * (ui8Idx - 28)));
				break;
			case 32: case 33: case 34: case 35:
				*pui8Data = 0xFF & (g_u32_vw_SMIOUTnCounter >> (8 * (ui8Idx - 32)));
				break;
			case 36: case 37: case 38: case 39:
				*pui8Data = 0xFF & (g_u32_vw_NMIOUTnCounter >> (8 * (ui8Idx - 36)));
				break;
			case 40: case 41: case 42: case 43:
				*pui8Data = 0xFF & (g_ioExpErrCnt >> (8 * (ui8Idx - 40)));
				break;
#if CONFIG_APP_THERMAL
			case 44: case 45: case 46: case 47:
				*pui8Data = 0xFF & (g_u32ApmlErrCnt >> (8 * (ui8Idx - 44)));
				break;
#endif
			default:
				break;
		}
	}

	return 1;
}
#endif