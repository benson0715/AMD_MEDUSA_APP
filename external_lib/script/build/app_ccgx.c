/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/flash.h>
#include "amd_crb_drivers_spiFlash.h"
#include "app_ccgx.h"
#include "board_config.h"
#include "app_ucsi_tunnel.h"
#include "system.h"
#include "app_usbc_task.h"

LOG_MODULE_REGISTER(ccgx, CONFIG_CCGX_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#define CYPRESS_ROM_FW_VERSION_OFFSET       0xEE
#define CYPRESS_ROM_FW_VERSION_OFFSET_CCGx  0xF7
#define CYPRESS_ROM_FW_VERSION_OFFSET_CCG8  0x5F8

/* ************************** *
 *     static valuable        *
 * ************************** */
/* FW update related */
static DRV_HPI_FW_ROW_INDEX _s_ccgxMetadataRow[2];
static DRV_HPI_FW_ROW_INDEX_CCG8 _s_ccgxMetadataRowCcg8[2];
static DRV_HPI_FW_ONE_ROW _s_ccgx_oneRowBuf;
static DRV_HPI_FW_ONE_ROW_CCGx _s_ccgx_oneRowBufNew;
static DRV_HPI_FW_ONE_ROW_CCG8 _s_ccgx_oneRowBufNewCcg8;
static DRV_HPI_FW_SIGNATURE  _s_ccgx_fw1Sign;
static DRV_HPI_FW_SIGNATURE  _s_ccgx_fw2Sign;

static bool _s_ccgxPowerOn = true;
static bool _s_ccgx_G3ResumePd0 = false, _s_ccgx_G3ResumePd1 = false;
static struct k_mutex g_ccgxSyncUpLock;

/* ************************** *
 *     global valuable        *
 * ************************** */
DRV_HPI_FW_VERSION g_ccgx_blVer;
DRV_HPI_FW_VERSION g_ccgx_fw1Ver;
DRV_HPI_FW_VERSION g_ccgx_fw2Ver;

/* skip update or not */
bool g_ccgx_skipFw1 = false;
bool g_ccgx_skipFw2 = false;

/* CCGx sync up are protected by spin lock */
volatile uint8_t g_ccgxSyncUpFlag = 1;

/* Update the sink path status */
uint8_t g_ccgxUpdateSinkPath = 0;

extern DRV_HPI_Rx0000_DEVICE_MODE g_hpi_Regx0000[2];

/**
 * app_ccgx_vonderIdentify
 *
 * @return true: detect ccgx PD controller
 *
 * @note
 *  Detect IFX PD controller ccgx serial based on I2C address
 */
bool app_ccgx_vonderIdentify(void)
{
	uint8_t buf[16];
	int ret;
	bool ret0 = false, ret1 = false;
	uint32_t attempts = 0;

	do {
		memset (buf, 0x3F, sizeof(buf));
		ret = amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0000, &g_hpi_Regx0000[0], 1, 0);

		if (ret == 0){
			/* ccgx chip0 detected */
			ret0 = true;
			break;
		}
		k_sleep(K_MSEC(10)); /* sleep "10" ms */
		attempts++;
	} while (attempts < 10);  /* 100ms delay */

	attempts = 0;

	do {
		memset (buf, 0x3F, sizeof(buf));
		ret = amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0000, &g_hpi_Regx0000[1], 1, 1);

		if (ret == 0) {
			/* ccgx chip1 detected */
			ret1 = true;
			break;
		}
		k_sleep(K_MSEC(10)); /* sleep "10" ms */
		attempts++;
	} while (attempts < 10);  /* 100ms delay */

	return (ret0 & ret1);
}

/**
 * app_ccgx_init
 *
 * @note
 *  ccgx device init
 */
void app_ccgx_init(void)
{
	/* Clean pending interrupt before the initialization routine. */
	/* Otherwise Cypress IC may not response to registers accessing. */
	amd_crb_drivers_hpi_intTrigger(0);
	amd_crb_drivers_hpi_intBottomHalf(0);

#if PD_TRIPPORT_ENABLE
	amd_crb_drivers_hpi_intTrigger(1);
	amd_crb_drivers_hpi_intBottomHalf(1);
#endif

	amd_crb_drivers_hpi_init();

	/* Read the 0x1040 initial status */
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0040, (void*)&g_hpi_Regx0040[0], 1, 0); /*  chipID0 retimer */
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0040, (void*)&g_hpi_Regx0040[1], 1, 1); /*  chipID1 no retimer*/

	/* set up mutex */
	k_mutex_init(&g_ccgxSyncUpLock);

	g_pdCtrlSt[0].sku[0] = 1;//g_hpi_Regx0040[0].f.b4FWSKU;
	g_pdCtrlSt[1].sku[0] = 2;//g_hpi_Regx0040[1].f.b4FWSKU;

	g_pdCtrlSt[0].mode[0] = 'A';
	g_pdCtrlSt[0].mode[1] = 'P';
	g_pdCtrlSt[0].mode[2] = 'P';
	g_pdCtrlSt[0].mode[3] = ' ';

	g_pdCtrlSt[1].mode[0] = 'A';
	g_pdCtrlSt[1].mode[1] = 'P';
	g_pdCtrlSt[1].mode[2] = 'P';
	g_pdCtrlSt[1].mode[3] = ' ';

	g_pdCtrlSt[0].type[0] = 'C';
	g_pdCtrlSt[0].type[1] = 'Y';
	g_pdCtrlSt[0].type[2] = 'D';
	g_pdCtrlSt[0].type[3] = 'F';

	g_pdCtrlSt[1].type[0] = 'C';
	g_pdCtrlSt[1].type[1] = 'Y';
	g_pdCtrlSt[1].type[2] = 'S';
	g_pdCtrlSt[1].type[3] = 'F';
}

/**
 * app_ccgx_syncUpCustomizedByte
 *
 * @note
 *  sync up the ccgx amd custimized bytes
 */
void app_ccgx_syncUpCustomizedByte (void)
{
	uint8_t flag = 0;
	uint8_t val1, val2;

	k_mutex_lock(&g_ccgxSyncUpLock, K_FOREVER);

	if (g_ccgxSyncUpFlag) {
		flag = 1;
		val1 = g_hpi_Regx0040[0].ui8Reg;
		val2 = g_hpi_Regx0040[1].ui8Reg;

		g_ccgxSyncUpFlag = 0;
	}
	k_mutex_unlock(&g_ccgxSyncUpLock);

	if (flag) {
		uint8_t ret = amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0040, &val1, 1, 0);
		LOG_DBG("write CCGx1040 0x%02X  (ack:%d)", val1, ret);
		LOG_DBG("chipID:0*************************************************** write CCGx0040 0x%02X  (ack:%d)", val1, ret);

		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, 0);  /* 200ms */

#if PD_TRIPPORT_ENABLE
		ret = amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0040, &val2, 1, 1);
		LOG_DBG("write CCGx1040 0x%02X  (ack:%d)", val2, ret);
		LOG_DBG("chipID:1*************************************************** write CCGx0040 0x%02X  (ack:%d)", val2, ret);

		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, 1);  /* 200ms */
#endif

		/* Need to skip first time power on or power from VCI mode */
		if (_s_ccgxPowerOn == false) {
			/* Only works in G3 mode and S0 mode after G3. */
			if (g_hpi_Regx0040[0].f.b2PwSt == 0 && _s_ccgx_G3ResumePd0 == false) {
				amd_crb_drivers_hpi_systemStatus(0, app_pseq_systemState());
				_s_ccgx_G3ResumePd0 = true;
			} else if (g_hpi_Regx0040[0].f.b2PwSt == 3 && _s_ccgx_G3ResumePd0 == true) {
				amd_crb_drivers_hpi_systemStatus(0, app_pseq_systemState());
				_s_ccgx_G3ResumePd0 = false;
			}

			if (g_hpi_Regx0040[1].f.b2PwSt == 0 && _s_ccgx_G3ResumePd1 == false) {
#if PD_TRIPPORT_ENABLE
				amd_crb_drivers_hpi_systemStatus(1, app_pseq_systemState());
#endif
				_s_ccgx_G3ResumePd1 = true;
			} else if (g_hpi_Regx0040[1].f.b2PwSt == 3 && _s_ccgx_G3ResumePd1 == true) {
#if PD_TRIPPORT_ENABLE
				amd_crb_drivers_hpi_systemStatus(1, app_pseq_systemState());
#endif
				_s_ccgx_G3ResumePd1 = false;
			}
		}
	}
}

/**
 * app_ccgx_systemPowerOn
 *
 * @note
 *  clean the ccgx power on flag in power sequence thread
 */
void app_ccgx_systemPowerOn(void)
{
	_s_ccgxPowerOn = false;
}

/**
 * app_ccgx_eventHandler
 *
 * @note
 *  ccgx event interrupt handler
 */
void app_ccgx_eventHandler(void)
{
	/* 1. HPI interrupt handling */
	app_ccgx_syncUpCustomizedByte();
	amd_crb_drivers_hpi_intBottomHalf(0);
#if PD_TRIPPORT_ENABLE
	amd_crb_drivers_hpi_intBottomHalf(1);
#endif
	/* Process the Sink path enable */
	if (g_ccgxUpdateSinkPath)
	{
		g_ccgxUpdateSinkPath = 0;
		amd_crb_drivers_hpi_sinkPathEnable(amd_crb_drivers_hpi_getHigherPortNumSnk());
	}
}

/**
 * app_ccgx_firmwareUpdate
 *
 * @param [in]   pStatusCallback;     update callback after finish
 * @param [in]          fwOffset;     binary address in spi flash
 * @param [in]            fwSize;     firmware size
 * @param [in]            chipID;     chip index
 * @return true If successful.
 * @return false if fail
 *
 * @note
 *  update ccgx pd firmare binary
 */
bool app_ccgx_firmwareUpdate(DRV_HPI_PROGRESS_INDICATOR pStatusCallback, uint32_t fwOffset, uint32_t fwSize, uint8_t chipID)
{
	uint8_t testBuf[128];

#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif

	/* FW0 */
	_s_ccgxMetadataRow[0].rowIdx[0] = 0x00;
	_s_ccgxMetadataRow[0].rowIdx[1] = 0x01;
	_s_ccgxMetadataRow[0].rowIdx[2] = 0xFF;
	/* FW1 */
	_s_ccgxMetadataRow[1].rowIdx[0] = 0x00;
	_s_ccgxMetadataRow[1].rowIdx[1] = 0x01;
	_s_ccgxMetadataRow[1].rowIdx[2] = 0xFE;

	LOG_DBG("CYPD update FW chipID:%d", chipID);

	if (pStatusCallback) {
		pStatusCallback(DRV_HPI_FW_UPDATE_INIT);
	}

	g_pdCtrlSt[chipID].isFwLoadSkipped = false;
	g_pdCtrlSt[chipID].isFwLoadSuccess = true;
	g_pdCtrlSt[chipID].Custom[0] = 0;
	g_pdCtrlSt[chipID].Custom[1] = 0;
	g_pdCtrlSt[chipID].Custom[2] = 0;
	g_pdCtrlSt[chipID].Custom[3] = 0;

	/* step 00.a, read SPI  */
	memset(&_s_ccgx_fw1Sign, 0, sizeof(DRV_HPI_FW_SIGNATURE));
	memset(&_s_ccgx_fw2Sign, 0, sizeof(DRV_HPI_FW_SIGNATURE));

	/* only update FW2 in ccgx */
	g_ccgx_skipFw1 = true;
	g_ccgx_skipFw2 = false;

	if (fwOffset != 0xFFF && fwOffset != 0x0) {
		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, fwOffset, 32, testBuf);
		memcpy(_s_ccgx_fw2Sign.sign, testBuf, sizeof(_s_ccgx_fw2Sign));
	} else {
		g_ccgx_skipFw2 = true;
	}

	if (g_ccgx_skipFw1 && g_ccgx_skipFw2) {

		if (pStatusCallback) {
			pStatusCallback(DRV_HPI_FW_UPDATE_HAD_SKIPPED);
		}

		LOG_DBG("Sig not match. CCGx update failed");
		return false;
	}

	/* step 01, make sure there's no pending interrupt */
	/** Need to handle Cypress interrupt first, otherwise
	 *  the FW version cannot be read correctly
	 */
	amd_crb_drivers_hpi_intTrigger(chipID);
	amd_crb_drivers_hpi_intBottomHalf(chipID);

	/* step 02, get device mode */
	amd_crb_drivers_hpi_getDeviceMode(chipID);

	DRV_HPI_FW_VERSION romfw1Ver;
	DRV_HPI_FW_VERSION romfw2Ver;
	memset(&romfw1Ver, 0, sizeof(DRV_HPI_FW_VERSION));
	memset(&romfw2Ver, 0, sizeof(DRV_HPI_FW_VERSION));

	/**offset-3: cypress PD1 A0 FW2
	 * offset-4: cypress PD1 B0 FW2
	 * offset-5: cypress PD2 FW2
	 */
	if (!g_ccgx_skipFw2) {
		amd_crb_drivers_sFlashRead(0, fwOffset + CYPRESS_ROM_FW_VERSION_OFFSET_CCGx, 16, testBuf);  /* FW2 location */
		memcpy((uint8_t *)&romfw2Ver, testBuf, sizeof(romfw2Ver));
	}

	/* step 03. check the fw version */
	if (app_ccgx_getCypressFwVersionCcgx(chipID)) {
		/* Do the following check only if the boot loader version is available. */
		if (!app_ccgx_fwVerCompare(&romfw2Ver, &g_ccgx_fw2Ver)) {
			g_ccgx_skipFw2 = true;
			LOG_DBG("ChipID: %d, FW2 version match.", chipID);

			/**
			 * This call out gives an opportunity to force the PD FW update routine
			 * It only influences the Version matched case.
			*/
#ifdef CHECK_FOR_FORCE_PD_UPDATE
			if (CHECK_FOR_FORCE_PD_UPDATE()) {
				g_ccgx_skipFw2 = false;
			}
#endif
		}

		/* Update the PD FW */
		g_pdCtrlSt[chipID].VVVV = g_ccgx_fw2Ver.f.appVer.f.major;
		g_pdCtrlSt[chipID].MM = g_ccgx_fw2Ver.f.appVer.f.minor;
		g_pdCtrlSt[chipID].RR = g_ccgx_fw2Ver.f.appVer.f.exCricuit;
	} else {
		g_ccgx_skipFw1 = true;
		g_ccgx_skipFw2 = true;
		LOG_DBG("FW version failed. CCGx update failed");
	}

	/* If cannot read major and minor skip upgrade */
	if ((g_pdCtrlSt[chipID].VVVV == 0xF) &&
		(g_pdCtrlSt[chipID].MM == 0xF) &&
		(g_ccgx_fw2Ver.f.appVer.f.exCricuit == 0xFF))
		g_ccgx_skipFw2 = true;

	/* chip 0 need to check two ports */
	if (chipID == 0)
	{
		/* make sure there is no sink attached for dead battery case */
		amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_0_IDX, chipID);
		amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_1_IDX, chipID);
		amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_0_IDX, chipID);
		amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_1_IDX, chipID);
		if (((g_hpi_Regx1008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx100C[chipID].f.typeDeviceAttached)) ||
			((g_hpi_Regx2008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx200C[chipID].f.typeDeviceAttached)))
		{
			g_ccgx_skipFw1 = true;
			g_ccgx_skipFw2 = true;
			LOG_DBG("chipID: %d, Source attached. CCGx update failed", chipID);
		}
	}
	/* chip 1 need to check one port */
	else if (chipID == 1)
	{
		/* make sure there is no sink attached for dead battery case */
		amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_0_IDX, chipID);
		amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_0_IDX, chipID);
		if ((g_hpi_Regx1008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx100C[chipID].f.typeDeviceAttached))
		{
			g_ccgx_skipFw1 = true;
			g_ccgx_skipFw2 = true;
			LOG_DBG("chipID: %d, Source attached. CCGx update failed", chipID);
		}
	}

	if (g_ccgx_skipFw1 && g_ccgx_skipFw2) {

		if (pStatusCallback) {
			pStatusCallback(DRV_HPI_FW_UPDATE_HAD_SKIPPED);
		}
		return false;
	}

#define CYPRESS_HPI_UPDATE_BOTH_FW 1

	/* step 04, if it's not boot mode, i.e. either fw1 or fw2 exists */
#if CYPRESS_HPI_UPDATE_BOTH_FW /* if we can update both FWs in a time, set the HPI to work in boot mode. */
	while (g_hpi_Regx0000[chipID].f.firmwareMode != CYPRESS_HPI_FwMODE_BOOT) {
#if CONFIG_EC_UCSI_ENABLE
		/* Stop UCSI */
		app_ucsi_tunnel_end(chipID);
#endif

		/* Disable Port0/1 */
		DRV_HPI_Rx002C_PORT_ENABLE _hpi_Regx002C = {0xFF};
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &_hpi_Regx002C, 1, chipID);
		if (_hpi_Regx002C.f.port0En || _hpi_Regx002C.f.port1En) {
			_hpi_Regx002C.f.port0En = 0;
			_hpi_Regx002C.f.port1En = 0;
			amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &_hpi_Regx002C, 1, chipID);
			amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
			LOG_DBG("chipID:%d CCGx disable port", chipID);
		}

		/* Jump to boot */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0007, "J", 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_EVENT_RESET_COMPLETE, 200, chipID);  /* 200ms */

		/* read the device mode again */
		amd_crb_drivers_hpi_getDeviceMode(chipID);
		LOG_DBG("chipID:%d Enter bootloader mode", chipID);
	}
#endif

	LOG_DBG("chipID:%d CCGx update start....", chipID);

	/* step 05, confirm if the silicon ID is 0x1800 */
	uint16_t siliconId = 0;
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0002, &siliconId, 2, chipID);
	if (g_hpi_Regx0000[chipID].f.firmwareMode == CYPRESS_HPI_FwMODE_FW1 || siliconId != _BYTEORDER16(_s_ccgx_fw1Sign.f.siliconId))
		g_ccgx_skipFw1 = true;
	if (g_hpi_Regx0000[chipID].f.firmwareMode == CYPRESS_HPI_FwMODE_FW2 || siliconId != _BYTEORDER16(_s_ccgx_fw2Sign.f.siliconId))
		g_ccgx_skipFw2 = true;

	if (g_ccgx_skipFw1 && g_ccgx_skipFw2) {
		/* Jump to Alt FW */
#if CYPRESS_HPI_UPDATE_BOTH_FW
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0007, "A", 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
#endif

		if (pStatusCallback) {
			pStatusCallback(DRV_HPI_FW_UPDATE_HAD_SKIPPED);
		}
		return false;
	}

	/* step 06, Update the fw */
	bool ret1 = false, ret2 = false;
	/* Only update FW2 for ccg6df */
	if (!g_ccgx_skipFw1) {
		/* Enter flash mode */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x000A, "P", 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */

		/* TODO: make sure device mode is BL */

		ret1 = app_ccgx_fwxUpdate(0, pStatusCallback, fwOffset, fwSize, chipID);
		LOG_DBG("chipID:%d FW1 is updated %s.", chipID, ret1 ? "successfully" : "but failed");
	}
	if (!g_ccgx_skipFw2) {
		/* Enter flash mode */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x000A, "P", 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */

		/* TODO: make sure device mode is BL */
		gpio_write_pin(EC_BLINK_N, 0);

#ifdef INFORM_USER_FOR_PD_UPDATE
		INFORM_USER_FOR_PD_UPDATE();
#endif
#if CONFIG_SOC_SERIES_NPCX4
		amd_crb_drivers_spi_reset();
#endif
		ret2 = app_ccgx_fwxUpdate(1, pStatusCallback, fwOffset, fwSize, chipID);
		LOG_DBG("chipID:%d FW2 is updated %s.", chipID, ret2 ? "successfully" : "but failed");
		gpio_write_pin(EC_BLINK_N, 1);
	}

	/*   step 07.b Issue reset command */
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0008, "R\1", 2, chipID);
	k_sleep(K_MSEC(200));  /* 200ms */
	amd_crb_drivers_hpi_wait4DevResp(HPI_EVENT_RESET_COMPLETE, 400, chipID);  /* 400ms */

	/* Update the PD FW */
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0020, &g_ccgx_fw2Ver, 8, chipID);
	g_pdCtrlSt[chipID].VVVV = g_ccgx_fw2Ver.f.appVer.f.major;
	g_pdCtrlSt[chipID].MM = g_ccgx_fw2Ver.f.appVer.f.minor;
	g_pdCtrlSt[chipID].RR = g_ccgx_fw2Ver.f.appVer.f.exCricuit;

	if (pStatusCallback) {
		if (ret1 || ret2)
			pStatusCallback(DRV_HPI_FW_UPDATE_SUCCESS);
		else
			pStatusCallback(DRV_HPI_FW_UPDATE_FAILED);
	}
	return ret1 || ret2;
}

/**
 * f_hpi_firmwareUpdateCcg8
 *
 * @param [in]   pStatusCallback;     update callback after finish
 * @param [in]          fwOffset;     binary address in spi flash
 * @param [in]            fwSize;     firmware size
 * @param [in]            chipID;     chip index
 * @return true If successful.
 * @return false if fail
 *
 * @note
 *  update ccg8 pd firmare binary
 */
bool app_ccgx_firmwareUpdateCcg8(DRV_HPI_PROGRESS_INDICATOR pStatusCallback, uint32_t fwOffset, uint32_t fwSize, uint8_t chipID)
{
	bool ret2 = false;
	uint8_t testBuf[128];
#if CCG8_CFP_ENABLE
	/* CCG8 FW0 */
	_s_ccgxMetadataRowCcg8[0].rowIdx[0] = 0x00;
	_s_ccgxMetadataRowCcg8[0].rowIdx[1] = 0x01;
	_s_ccgxMetadataRowCcg8[0].rowIdx[2] = 0xFF;
	_s_ccgxMetadataRowCcg8[0].rowIdx[3] = 0x00;
	/* CCG8 FW1 */
	_s_ccgxMetadataRowCcg8[1].rowIdx[0] = 0x00;
	_s_ccgxMetadataRowCcg8[1].rowIdx[1] = 0x01;
	_s_ccgxMetadataRowCcg8[1].rowIdx[2] = 0xFE;
	_s_ccgxMetadataRowCcg8[1].rowIdx[3] = 0x00;
#else
	/* CCG8 FW0 */
	_s_ccgxMetadataRowCcg8[0].rowIdx[0] = 0x00;
	_s_ccgxMetadataRowCcg8[0].rowIdx[1] = 0x03;
	_s_ccgxMetadataRowCcg8[0].rowIdx[2] = 0xFF;
	_s_ccgxMetadataRowCcg8[0].rowIdx[3] = 0x00;
	/* CCG8 FW1 */
	_s_ccgxMetadataRowCcg8[1].rowIdx[0] = 0x00;
	_s_ccgxMetadataRowCcg8[1].rowIdx[1] = 0x03;
	_s_ccgxMetadataRowCcg8[1].rowIdx[2] = 0xFE;
	_s_ccgxMetadataRowCcg8[1].rowIdx[3] = 0x00;
#endif

#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif

	LOG_DBG("CCG8 update FW chipID:%d", chipID);

	if (pStatusCallback) {
		pStatusCallback(DRV_HPI_FW_UPDATE_INIT);
	}

	g_pdCtrlSt[chipID].isFwLoadSkipped = false;
	g_pdCtrlSt[chipID].isFwLoadSuccess = true;
	g_pdCtrlSt[chipID].Custom[0] = 0;
	g_pdCtrlSt[chipID].Custom[1] = 0;
	g_pdCtrlSt[chipID].Custom[2] = 0;
	g_pdCtrlSt[chipID].Custom[3] = 0;

	/* step 00.a, read SPI  */
	memset(&_s_ccgx_fw1Sign, 0, sizeof(DRV_HPI_FW_SIGNATURE));
	memset(&_s_ccgx_fw2Sign, 0, sizeof(DRV_HPI_FW_SIGNATURE));

	/* update FW2 for ccg8 */
	g_ccgx_skipFw1 = true;
	g_ccgx_skipFw2 = false;

	if (g_ccgx_skipFw1 && g_ccgx_skipFw2) {
		if (pStatusCallback) {
			pStatusCallback(DRV_HPI_FW_UPDATE_HAD_SKIPPED);
		}

		LOG_DBG("Sig not match. CCG8 update failed");
		return false;
	}

	/* step 01, make sure there's no pending interrupt */
	/* Need to handle Cypress interrupt first, otherwise
	 * the FW version cannot be read correctly
	 */
	amd_crb_drivers_hpi_intTrigger(chipID);
	amd_crb_drivers_hpi_intBottomHalf(chipID);

	/* step 02, get device mode */
	amd_crb_drivers_hpi_getDeviceMode(chipID);

	DRV_HPI_FW_VERSION romfw1Ver;
	DRV_HPI_FW_VERSION romfw2Ver;
	memset(&romfw1Ver, 0, sizeof(DRV_HPI_FW_VERSION));
	memset(&romfw2Ver, 0, sizeof(DRV_HPI_FW_VERSION));

	if (!g_ccgx_skipFw2) {
		amd_crb_drivers_sFlashRead(0, fwOffset + CYPRESS_ROM_FW_VERSION_OFFSET_CCG8, 16, testBuf);  /* FW2 location */
		memcpy((uint8_t *)&romfw2Ver, testBuf, sizeof(romfw2Ver));
	}

	/* step 03. check the fw version */
	if (app_ccgx_getCypressFwVersionCcgx(chipID)) {
		/* Do the following check only if the boot loader version is available. */
		if (!app_ccgx_fwVerCompare(&romfw2Ver, &g_ccgx_fw2Ver)) {
			g_ccgx_skipFw2 = true;
			g_ccgx_skipFw1 = true;
			LOG_DBG("ChipID: %d, CCG8 version match.", chipID);

			/*
			 * This call out gives an opportunity to force the PD FW update routine
			 * It only influences the Version matched case.
			 */
#ifdef CHECK_FOR_FORCE_PD_UPDATE
			if (CHECK_FOR_FORCE_PD_UPDATE()) {
				g_ccgx_skipFw2 = false;
				g_ccgx_skipFw1 = false;
			}
#endif
		}

		/* Update the PD FW */
		g_pdCtrlSt[chipID].VVVV = g_ccgx_fw2Ver.f.appVer.f.major;
		g_pdCtrlSt[chipID].MM = g_ccgx_fw2Ver.f.appVer.f.minor;
		g_pdCtrlSt[chipID].RR = g_ccgx_fw2Ver.f.appVer.f.exCricuit;
	} else {
		g_ccgx_skipFw2 = true;
		g_ccgx_skipFw1 = true;
		LOG_DBG("FW version failed. CCG8 update failed");
	}

	/* If cannot read major and minor skip upgrade */
	if ((g_pdCtrlSt[chipID].VVVV == 0xF) &&
		(g_pdCtrlSt[chipID].MM == 0xF) &&
		(g_ccgx_fw2Ver.f.appVer.f.exCricuit == 0xFF)) {
		g_ccgx_skipFw2 = true;
		g_ccgx_skipFw1 = true;
	}

	/* chip 0 need to check two ports */
	if (chipID == 0) {
		/* make sure there is no sink attached for dead battery case */
		amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_0_IDX, chipID);
		amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_1_IDX, chipID);
		amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_0_IDX, chipID);
		amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_1_IDX, chipID);
		if (((g_hpi_Regx1008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx100C[chipID].f.typeDeviceAttached)) ||
			((g_hpi_Regx2008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx200C[chipID].f.typeDeviceAttached))) {
			g_ccgx_skipFw2 = true;
			g_ccgx_skipFw1 = true;
			LOG_DBG("chipID: %d, Source attached. CCG8 update failed", chipID);
		}
	}
	/* chip 1 need to check one port */
	else if (chipID == 1) {
		/* make sure there is no sink attached for dead battery case */
		amd_crb_drivers_hpi_getPdStatus(TYPEC_PORT_0_IDX, chipID);
		amd_crb_drivers_hpi_getTypecStatus(TYPEC_PORT_0_IDX, chipID);
		if ((g_hpi_Regx1008[chipID].f.crtPortPwrRole == PRT_ROLE_SINK) && (g_hpi_Regx100C[chipID].f.typeDeviceAttached)) {
			g_ccgx_skipFw2 = true;
			g_ccgx_skipFw1 = true;
			LOG_DBG("chipID: %d, Source attached. CCG8 update failed", chipID);
		}
	}

	if (g_ccgx_skipFw1 && g_ccgx_skipFw2) {
		if (pStatusCallback) {
			pStatusCallback(DRV_HPI_FW_UPDATE_HAD_SKIPPED);
		}
		return false;
	}

	/* step 04, if it's not boot mode, i.e. either fw1 or fw2 exists */
	while (g_hpi_Regx0000[chipID].f.firmwareMode != CYPRESS_HPI_FwMODE_BOOT) {
		/* Stop UCSI */
		app_ucsi_tunnel_end(chipID);

		/* Disable Port0/1 */
		DRV_HPI_Rx002C_PORT_ENABLE _hpi_Regx002C = {0xFF};
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &_hpi_Regx002C, 1, chipID);
		if (_hpi_Regx002C.f.port0En || _hpi_Regx002C.f.port1En) {
			_hpi_Regx002C.f.port0En = 0;
			_hpi_Regx002C.f.port1En = 0;
			amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &_hpi_Regx002C, 1, chipID);
			amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
			LOG_DBG("chipID:%d CCGx disable port", chipID);
		}

		/* Jump to boot */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0007, "J", 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_EVENT_RESET_COMPLETE, 200, chipID);  /* 200ms */

		/* read the device mode again */
		amd_crb_drivers_hpi_getDeviceMode(chipID);
		LOG_DBG("chipID:%d Enter bootloader mode", chipID);
	}

	LOG_DBG("chipID:%d CCG8 update start....", chipID);

	/* step 05, confirm if the silicon ID is 0x1800 */
	uint16_t siliconId = 0;
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0002, &siliconId, 2, chipID);
	if (g_hpi_Regx0000[chipID].f.firmwareMode == CYPRESS_HPI_FwMODE_FW2)
	{
		g_ccgx_skipFw2 = true;
		g_ccgx_skipFw1 = true;
	}

	if (g_ccgx_skipFw1 && g_ccgx_skipFw2) {
		if (pStatusCallback) {
			pStatusCallback(DRV_HPI_FW_UPDATE_HAD_SKIPPED);
		}
		return false;
	}

	if (!g_ccgx_skipFw2) {
		/* Enter flash mode */
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x000A, "P", 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */

		gpio_write_pin(EC_BLINK_N, 0);

#ifdef INFORM_USER_FOR_PD_UPDATE
		INFORM_USER_FOR_PD_UPDATE();
#endif
#if CONFIG_SOC_SERIES_NPCX4
		amd_crb_drivers_spi_reset();
#endif
		ret2 = app_ccgx_fwxUpdateCcg8(1, pStatusCallback, fwOffset, fwSize, chipID);
		LOG_DBG("chipID:%d FW2 is updated %s len:%x S:%x.", chipID, ret2 ? "successfully" : "but failed", fwOffset, fwSize);

		gpio_write_pin(EC_BLINK_N, 1);
	}

	/*   step 07.b Issue reset command */
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0008, "R\1", 2, chipID);
	k_sleep(K_MSEC(200));  /* 200ms */
	amd_crb_drivers_hpi_wait4DevResp(HPI_EVENT_RESET_COMPLETE, 400, chipID);  /* 400ms */

	/* Update the PD FW */
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0020, &g_ccgx_fw2Ver, 8, chipID);
	g_pdCtrlSt[chipID].VVVV = g_ccgx_fw2Ver.f.appVer.f.major;
	g_pdCtrlSt[chipID].MM = g_ccgx_fw2Ver.f.appVer.f.minor;
	g_pdCtrlSt[chipID].RR = g_ccgx_fw2Ver.f.appVer.f.exCricuit;

	/* Enable Port0/1 */
	DRV_HPI_Rx002C_PORT_ENABLE _hpi_Regx002C = {0xFF};
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x002C, &_hpi_Regx002C, 1, chipID);
	if (!_hpi_Regx002C.f.port0En || !_hpi_Regx002C.f.port1En) {
		_hpi_Regx002C.f.port0En = 1;
		_hpi_Regx002C.f.port1En = 1;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x002C, &_hpi_Regx002C, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		LOG_DBG("chipID:%d CCG8 enable port", chipID);
	}

	if (pStatusCallback) {
		if (ret2)
			pStatusCallback(DRV_HPI_FW_UPDATE_SUCCESS);
		else
			pStatusCallback(DRV_HPI_FW_UPDATE_FAILED);
	}
	return ret2;
}

/**
 * app_ccgx_firmwareUpdate
 *
 * @param [in]   pStatusCallback;     update callback after finish
 * @param [in]          fwOffset;     binary address in spi flash
 * @param [in]            fwSize;     firmware size
 * @param [in]            chipID;     chip index
 * @return true If successful.
 * @return false if fail
 *
 * @note
 *  update ccgx one firmware region.
 */
bool app_ccgx_fwxUpdate (uint8_t fwIdx, DRV_HPI_PROGRESS_INDICATOR pStatusCallback, uint32_t fwOffset, uint32_t fwSize, uint8_t chipID)
{
	uint32_t spiOffset = sizeof(DRV_HPI_FW_SIGNATURE);
	uint8_t LED_status = 0;
	uint8_t LED_cnt = 0;

#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif

	spiOffset += fwOffset;

	/* 01. Clear metadata */
	memset(&_s_ccgx_oneRowBufNew, 0, sizeof(_s_ccgx_oneRowBufNew));
	memcpy(&_s_ccgx_oneRowBufNew.f.idx, &_s_ccgxMetadataRow[fwIdx], sizeof(DRV_HPI_FW_ROW_INDEX));

	amd_crb_drivers_hpi_writePayLoad2DataBuf(_s_ccgx_oneRowBufNew.f.payload, 128, chipID);
	amd_crb_drivers_hpi_triggerRowReadWrite(HPI_WRITE, _s_ccgx_oneRowBufNew.f.idx, chipID);


	memset(&_s_ccgx_oneRowBufNew.f.idx, 0, sizeof(DRV_HPI_FW_ROW_INDEX));
	memset(&_s_ccgx_oneRowBuf.f.idx, 0, sizeof(DRV_HPI_FW_ROW_INDEX));
	uint8_t testBuf[64];
	/* 02. Program each rows */
	while (memcmp(&_s_ccgxMetadataRow[fwIdx], &_s_ccgx_oneRowBufNew.f.idx, sizeof(DRV_HPI_FW_ROW_INDEX))) {
		/* load one row 128 bytes */
		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, spiOffset, 64, testBuf);
		memcpy(&_s_ccgx_oneRowBufNew.thisRow[0], testBuf, 64);

		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, spiOffset + 64 , 64, testBuf);
		memcpy(&_s_ccgx_oneRowBufNew.thisRow[64], testBuf, 64);

		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, spiOffset + 128, 32, testBuf);
		memcpy(&_s_ccgx_oneRowBufNew.thisRow[128], testBuf, sizeof(DRV_HPI_FW_ONE_ROW_CCGx) - 128);
		spiOffset += sizeof(DRV_HPI_FW_ONE_ROW_CCGx);

		/* checksum and : */
		if (!amd_crb_drivers_hpi_rowDataCheckCcgx(&_s_ccgx_oneRowBufNew)) {
			LOG_DBG("FW%d row %X check failed.", fwIdx + 1, _s_ccgx_oneRowBufNew.f.idx.f.index);
			if (pStatusCallback)
				pStatusCallback(DRV_HPI_FW_UPDATE_CHECKSUM_ERROR);
		}
		amd_crb_drivers_hpi_writePayLoad2DataBuf(_s_ccgx_oneRowBufNew.f.payload, 128, chipID);
		amd_crb_drivers_hpi_triggerRowReadWrite(HPI_WRITE, _s_ccgx_oneRowBufNew.f.idx, chipID);

		/* Blink the DBG LED to indicate the progress */
		if (pStatusCallback)
			pStatusCallback(DRV_HPI_FW_UPDATE_ONE_ROW);

		LED_cnt++;
		if (LED_cnt >= 20) {
			LED_cnt = 0;
			LED_status = (LED_status > 0) ? 0 : 1;
			gpio_write_pin(EC_BLINK_N, LED_status);
		}
	}

	/* 03. Ask CCGx to verify the FW */
	uint8_t fwVrf = fwIdx + 1;
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x000B, &fwVrf, 1, chipID);
	k_msleep(100);  /* 100ms */
	if (amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID))  /* 200ms */
		return true;

	return false;
}

/**
 * app_ccgx_fwxUpdateCcg8
 *
 * @param [in]   pStatusCallback;     update callback after finish
 * @param [in]          fwOffset;     binary address in spi flash
 * @param [in]            fwSize;     firmware size
 * @param [in]            chipID;     chip index
 * @return true If successful.
 * @return false if fail
 *
 * @note
 *  update ccg8 one firmware region.
 */
bool app_ccgx_fwxUpdateCcg8 (uint8_t fwIdx, DRV_HPI_PROGRESS_INDICATOR pStatusCallback, uint32_t fwOffset, uint32_t fwSize, uint8_t chipID)
{
	uint32_t spiOffset = 0;
	uint8_t LED_status = 0;
	uint8_t LED_cnt = 0;
	uint8_t temp;

#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif

	spiOffset = fwOffset;

	/* 01. Clear metadata for FW1 or 2 */
	memset(&_s_ccgx_oneRowBufNewCcg8, 0, sizeof(_s_ccgx_oneRowBufNewCcg8));
	memcpy(&_s_ccgx_oneRowBufNewCcg8.f.idx, (const void *)&_s_ccgxMetadataRowCcg8[fwIdx], sizeof(DRV_HPI_FW_ROW_INDEX_CCG8));

	amd_crb_drivers_hpi_writePayLoad2DataBuf(_s_ccgx_oneRowBufNewCcg8.f.payload, 256, chipID);
	amd_crb_drivers_hpi_triggerRowReadWriteCcg8(HPI_WRITE, _s_ccgx_oneRowBufNewCcg8.f.idx, chipID);

	memset(&_s_ccgx_oneRowBufNewCcg8.f.idx, 0, sizeof(DRV_HPI_FW_ROW_INDEX_CCG8));
	uint8_t testBuf[64];
	/* 02. Program each rows */
	while (memcmp(&_s_ccgxMetadataRowCcg8[fwIdx], &_s_ccgx_oneRowBufNewCcg8.f.idx, sizeof(DRV_HPI_FW_ROW_INDEX_CCG8))) {

		/* load one row 256 bytes */
		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, spiOffset, 64, testBuf);
		memcpy(&_s_ccgx_oneRowBufNewCcg8.thisRow[0], testBuf, 64);

		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, spiOffset + 64, 64, testBuf);
		memcpy(&_s_ccgx_oneRowBufNewCcg8.thisRow[64], testBuf, 64);

		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, spiOffset + 128 , 64, testBuf);
		memcpy(&_s_ccgx_oneRowBufNewCcg8.thisRow[128], testBuf, 64);

		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, spiOffset + 192 , 64, testBuf);
		memcpy(&_s_ccgx_oneRowBufNewCcg8.thisRow[192], testBuf, 64);

		memset(testBuf, 0, sizeof(testBuf));
		amd_crb_drivers_sFlashRead(0, spiOffset + 256, 4, testBuf);

		memcpy(&_s_ccgx_oneRowBufNewCcg8.thisRow[256], testBuf, sizeof(DRV_HPI_FW_ONE_ROW_CCG8) - 256);
		spiOffset += sizeof(DRV_HPI_FW_ONE_ROW_CCG8);

		/* sweep the address */
		temp = _s_ccgx_oneRowBufNewCcg8.f.idx.rowIdx[0];
		_s_ccgx_oneRowBufNewCcg8.f.idx.rowIdx[0] = _s_ccgx_oneRowBufNewCcg8.f.idx.rowIdx[3];
		_s_ccgx_oneRowBufNewCcg8.f.idx.rowIdx[3] = temp;
		temp = _s_ccgx_oneRowBufNewCcg8.f.idx.rowIdx[1];
		_s_ccgx_oneRowBufNewCcg8.f.idx.rowIdx[1] = _s_ccgx_oneRowBufNewCcg8.f.idx.rowIdx[2];
		_s_ccgx_oneRowBufNewCcg8.f.idx.rowIdx[2] = temp;

		/* write data */
		amd_crb_drivers_hpi_writePayLoad2DataBuf(_s_ccgx_oneRowBufNewCcg8.f.payload, 256, chipID);
		amd_crb_drivers_hpi_triggerRowReadWriteCcg8(HPI_WRITE, _s_ccgx_oneRowBufNewCcg8.f.idx, chipID);

		/* Blink the DBG LED to indicate the progress */
		if (pStatusCallback)
			pStatusCallback(DRV_HPI_FW_UPDATE_ONE_ROW);

		LED_cnt++;
		if (LED_cnt >= 20)
		{
			LED_cnt = 0;
			LED_status = (LED_status > 0) ? 0 : 1;
		}
	}

	/* 03. Ask CCG8 to verify the FW */
	uint8_t fwVrf = fwIdx + 1;
	amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x000B, &fwVrf, 1, chipID);
	k_msleep(100);  /* 100ms */
	if (amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID)) /* 200ms */
		return true;

	return false;
}

/**
 * app_ccgx_fwVerCompare
 *
 * @param [in]   pVer1;     version 1
 * @param [in]   pVer2;     version 2
 * @return match or not
 *
 * @note
 *  compare two version
 */
bool app_ccgx_fwVerCompare(DRV_HPI_FW_VERSION * pVer1, DRV_HPI_FW_VERSION * pVer2)
{
    /* return true if the first one is newer than the second one */
	if (pVer2->f.appVer.f.appName != 0 && pVer1->f.appVer.f.appName != pVer2->f.appVer.f.appName)
		return false;

	if (pVer1->f.baseVer.f.build != pVer2->f.baseVer.f.build)
		return true;
	if (pVer1->f.baseVer.f.patch != pVer2->f.baseVer.f.patch)
		return true;
	if (pVer1->f.baseVer.f.major != pVer2->f.baseVer.f.major)
		return true;
	if (pVer1->f.baseVer.f.minor != pVer2->f.baseVer.f.minor)
		return true;
	if (pVer1->f.appVer.f.exCricuit != pVer2->f.appVer.f.exCricuit)
		return true;
	return false;
}

/**
 * app_ccgx_getCypressFwVersion
 *
 * @param [in]   chipID;     chip index
 * @return is bootloader version avaliable or not
 *
 * @note
 *  try to read the pd fw version
 */
bool app_ccgx_getCypressFwVersion(uint8_t chipID)
{
	bool isBootLoaderVersionAvailable = false;
	uint32_t attempts = 0;

#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif

	do {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0010, &g_ccgx_blVer, 8, chipID);
		if (g_ccgx_blVer.f.appVer.f.appName == 0x6E62) {
			isBootLoaderVersionAvailable = true;
			break;
		}
		k_sleep(K_MSEC(1)); /* sleep "1" ms */
		attempts++;
	} while (attempts < 300);  /* wait 300ms */

	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0018, &g_ccgx_fw1Ver, 8, chipID);
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0020, &g_ccgx_fw2Ver, 8, chipID);

	return isBootLoaderVersionAvailable;
}

/**
 * app_ccgx_getCypressFwVersionCcgx
 *
 * @param [in]   chipID;     chip index
 * @return is bootloader version avaliable or not
 *
 * @note
 *  try to read the pd fw version for ccgx
 */
bool app_ccgx_getCypressFwVersionCcgx(uint8_t chipID)
{
	bool isBootLoaderVersionAvailable = false;
	uint32_t attempts = 0;

#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif

	do {
		amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0010, &g_ccgx_blVer, 8, chipID);
		if (g_ccgx_blVer.f.appVer.f.appName == 0x6E62) {
			isBootLoaderVersionAvailable = true;
			break;
		}
		k_msleep(1); /* sleep "ACD_HPI_RETRY_PERIOD" ms */
		attempts++;
	} while (attempts < 300);  /* wait 300ms */

	g_ccgx_fw2Ver.f.appVer.f.minor = 0xF;
	g_ccgx_fw2Ver.f.appVer.f.major = 0xF;
	g_ccgx_fw2Ver.f.appVer.f.exCricuit = 0xFF;

	g_ccgx_fw1Ver.f.appVer.f.minor = 0xF;
	g_ccgx_fw1Ver.f.appVer.f.major = 0xF;
	g_ccgx_fw1Ver.f.appVer.f.exCricuit = 0xFF;

	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0020, &g_ccgx_fw2Ver, 8, chipID);
	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0018, &g_ccgx_fw1Ver, 8, chipID);

	LOG_DBG("CYPD ChipID: %d, FW1: %x.%x.%x, FW2: %x.%x.%x", chipID, g_ccgx_fw1Ver.f.appVer.f.major, g_ccgx_fw1Ver.f.appVer.f.minor, g_ccgx_fw1Ver.f.appVer.f.exCricuit
	                                                                 , g_ccgx_fw2Ver.f.appVer.f.major, g_ccgx_fw2Ver.f.appVer.f.minor, g_ccgx_fw2Ver.f.appVer.f.exCricuit);

	return isBootLoaderVersionAvailable;
}

/**
 * app_ccgx_i2cTunnel
 *
 * @param [in]     isRead;     read or write
 * @param [in]       port;     port number
 * @param [in]    address;     i2c address
 * @param [in]        reg;     device register
 * @param [in]    dataLen;     data length
 * @param [in]       data;     data pointer
 * @param [in]     chipID;     chip index
 * @return successful or not
 *
 * @note
 *  i2c pass through tunnel through PD controller i2c master bus
 */
uint8_t app_ccgx_i2cTunnel(bool isRead, uint8_t port, uint8_t address, uint8_t reg, uint8_t dataLen, uint8_t* data, uint8_t chipID)
{
	uint8_t cmd = 0x3C;
	uint8_t i2c_packet[24];

	/* hpi i2c tunnel max buffer is 16. */
	if (dataLen > 16)
		return 0;

	/* load the setting into the write_mem */
	i2c_packet[0] = (address << 1) + isRead;
	i2c_packet[1] = 0;         /* CCGx SCB index */
	i2c_packet[2] = 0;         /* Reserved */
	i2c_packet[3] = 0;         /* Custom command code:
	                              0x00 -> AMD re-timer tunneling command
                                0x01 -> AMD mailbox tunneling command	*/
	i2c_packet[4] = dataLen;   /* Number of bytes */
	i2c_packet[5] = reg;       /* I2C register offset */

	memcpy(&i2c_packet[6], data, dataLen);

	/* Full the write data buf first */
	amd_crb_drivers_hpi_writeData2WDataBuf(port, i2c_packet, (6 + dataLen), false, chipID);

	/* trigger command */
	if (port == TYPEC_PORT_0_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x1006, &cmd, 1, chipID);
	}
	else if (port == TYPEC_PORT_1_IDX) {
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x2006, &cmd, 1, chipID);
	}
	else
		return 0;

	/* Wait response 0x1D */
	amd_crb_drivers_hpi_wait4PortRespReturn(HPI_RESP_I2C_REG, data, port, 20, chipID);  /* 20ms */

	return 1;
}

/**
 * app_ccgx_updatePwrStatus
 *
 * @param [in]     state;     system power status
 *
 * @note
 *  notify the CCGx register based on system power status
 */
void app_ccgx_updatePwrStatus(enum system_power_state state)
{
	k_mutex_lock(&g_ccgxSyncUpLock, K_FOREVER);
	g_ccgxSyncUpFlag = 1;
	switch(state) {
		case SYSTEM_G3_STATE:
			g_hpi_Regx0040[0].f.b2PwSt = 0; // 0: G3 - CCGx
			g_hpi_Regx0040[1].f.b2PwSt = 0; // 0: G3 - CCGx
			break;
		case SYSTEM_S0_STATE:
			g_hpi_Regx0040[0].f.b2PwSt = 3; // 3: S0 - CCGx
			g_hpi_Regx0040[1].f.b2PwSt = 3; // 3: S0 - CCGx
			break;
		case SYSTEM_S3_STATE:
			g_hpi_Regx0040[0].f.b2PwSt = 2; // 2: S3 - CCGx
			g_hpi_Regx0040[1].f.b2PwSt = 2; // 2: S3 - CCGx
			break;
		case SYSTEM_S5_STATE:
			g_hpi_Regx0040[0].f.b2PwSt = 1; // 1: S5 - CCGx
			g_hpi_Regx0040[1].f.b2PwSt = 1; // 1: S5 - CCGx
			break;
		default:
			break;
	}
	
	k_mutex_unlock(&g_ccgxSyncUpLock);
	app_usbc_giveSem();
}

/**
 * app_ccgx_forceRtPower
 *
 * @param [in]       port;     port number
 * @param [in]     chipID;     PD chip id
 * @param [in]         en;     force power on or not
 *
 * @note
 *  force power re-timer power and reset
 */
void app_ccgx_forceRtPower (uint8_t port, uint8_t chipID, bool en)
{
	uint8_t rt_Status;

	/* 0x0050 is a 1 byte register.
	 * If you write 1 to bits 0, 1, 2, 3
	 * - RT_PWR_EN_P0_PORT, RT_PWR_EN_P1_PORT, RT_RESET_N_P0_PORT and RT_RESET_N_P1_PORT will go high.
   * To clear the gpios, please write 0 to the bits accordingly.
	 */

	amd_crb_drivers_hpi_regAccess(HPI_READ, 0x0050, &rt_Status, 1, chipID);

	if (port == TYPEC_PORT_0_IDX) {
		if (en) {
			/* Enable the RT0 PWREN */
			rt_Status |= 0x01;
			/* trigger command */
			amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0050, &rt_Status, 1, chipID);
			amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */

			k_usleep(5000);

			/* Enable the RT0 RST */
			rt_Status |= 0x05;
			/* trigger command */
			amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0050, &rt_Status, 1, chipID);
			amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		} else {
			/* Disable the RT0 PWREN */
			/* Disable the RT0 RST */
			rt_Status &= ~(0x05);
			/* trigger command */
			amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0050, &rt_Status, 1, chipID);
			amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		}
	} else if (port == TYPEC_PORT_1_IDX) {
		if (en) {
			/* Enable the RT1 PWREN */
			rt_Status |= 0x02;
			/* trigger command */
			amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0050, &rt_Status, 1, chipID);
			amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */

			k_usleep(5000);

			/* Enable the RT1 RST */
			rt_Status |= 0x0A;
			/* trigger command */
			amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0050, &rt_Status, 1, chipID);
			amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		} else {
			/* Disable the RT0 PWREN */
			/* Disable the RT0 RST */
			rt_Status &= ~(0x0A);
			/* trigger command */
			amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0050, &rt_Status, 1, chipID);
			amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID);  /* 200ms */
		}
	}
}

/**
 * app_ccgx_forceUsb4Tbt3
 *
 * @param [in]     chipID;     PD chip id
 * @param [in]         en;     force power on or not
 *
 * @note
 *  force power re-timer power and reset
 *  force crossbar to USB4/TBT3 mode
 */
void app_ccgx_forceUsb4Tbt3(uint8_t chipID, bool en)
{
	uint8_t input = 0;
	if (en) {
		/* trigger command */
		input = 0x03;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0050, &input, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID); /* 200ms */
	} else {
		/* trigger command */
		input = 0x05;
		amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0050, &input, 1, chipID);
		amd_crb_drivers_hpi_wait4DevResp(HPI_RESP_SUCCESS, 200, chipID); /* 200ms */
	}
}