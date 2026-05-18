/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "amd_crb_drivers_ucsi.h"
#include "amd_crb_drivers_hpi.h"
#if CONFIG_USBC_TIPD_TPS6599X
#include "amd_crb_drivers_tps6599x.h"
#endif
#if CONFIG_USBC_TIPD_TPS6699X
#include "amd_crb_drivers_tps6699x.h"
#endif
#if CONFIG_USBC_CCGX
#include "app_ccgx.h"
#endif
#if CONFIG_USBC_ITE
#include "app_ite_pd.h"
#endif
#if CONFIG_USBC_RTK
#include "app_realtek_pd.h"
#include "amd_crb_drivers_realtek_ucsi.h"
#endif
#include "board_config.h"
#include "app_ucsi_tunnel.h"
#include "system.h"
#include "app_usbc_task.h"
#include "app_acpi.h"
#include "amd_crb_drivers_ucsi_internal.h"

LOG_MODULE_REGISTER(ucsi_tu, CONFIG_UCSI_TUNNEL_LOG_LEVEL);

/* ************************** *
 *     static valuable        *
 * ************************** */
/* Marco for calculation */
#define ACPI_IDX_TO_OFFSET(x)             (x - ACPI_UCSI_BASE)
#define UCSI_MAILBOX_GET_BYTE(s, x)       (*(((uint8_t *)&s) + x))
#define UCSI_MAILBOX_SET_BYTE(s, x, v)    (*(((uint8_t *)&s) + x) = v)
#define UCSI_MAILBOX_OFFSET(f)            (uint8_t)((uint32_t)&(((F_UCSI_MAILBOX *)0)->f))

/* ************************** *
 *     global valuable        *
 * ************************** */
bool g_isUcsiCycleFinished = true;
bool g_isUcsiSupported = false;
bool g_ucsiTunnelEnabled = true;
/* There are two buffer g_xUcsiMailBoxPtr exchange with ACPI and 
g_xUcsiMailBox_ds exchange with UCSI handler (could be pd controller or ec ucsi engine) */
F_UCSI_MAILBOX * g_xUcsiMailBoxPtr = NULL;  /* upstream   OPM (HOST <-> EC) */
volatile F_UCSI_MAILBOX g_xUcsiMailBox_ds;  /* downstream PPM (EC <-> device) */
#if (CONFIG_USBC_CCGX && PD_TRIPPORT_ENABLE)
uint8_t ucsi_PrevCommPort = TYPEC_PORT_0_IDX;
uint8_t ucsi_resp_cnt = 0;
F_UCSI_MAILBOX g_ucsi_error_status[2];
bool g_ucsi_error_flag[2];
#endif

/* ************************** *
 *     static valuable        *
 * ************************** */
/* flags to detect events */
volatile static bool _s_ucsi_Opm2PpmFlag = false;
volatile static bool _s_ucsi_Ppm2OpmFlag = false;
volatile static bool _s_ucsi_Ppm2OpmTimeout = false;

/* device id */
volatile static uint8_t _s_ucsi_tunnel_devId = PD_DEVICE_ID_TIx;

void app_ucsi_force_NumConnectors (uint8_t number);
void app_ucsi_force_ConnectorChangeIndicator (uint8_t chipID);

/**
 * app_ucsi_tunnel_isSupported
 *
 * @note
 *  is ucsi tunnel supported
 */
bool app_ucsi_tunnel_isSupported(void)
{
	return g_isUcsiSupported;
}

/**
 * app_ucsi_tunnel_writeDevID
 *
 * @note
 *  set ucsi tunnel device id
 */
void app_ucsi_tunnel_writeDevID(uint8_t id)
{
	_s_ucsi_tunnel_devId = id;
}

/**
 * app_ucsi_tunnel_readDevID
 *
 * @note
 *  get ucsi tunnel device id
 */
uint8_t app_ucsi_tunnel_readDevID(void)
{
	return _s_ucsi_tunnel_devId;
}

/***************************************************************************/
/**                         IFX PD Module UCSI                            **/
/***************************************************************************/
#if CONFIG_USBC_CCGX
/**
 * app_ucsi_writeHpiMailbox
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * CCGx hpi write to mailbox for each chip
 */
void app_ucsi_writeHpiMailbox (uint8_t chipID)
{
	LOG_DBG("      >>>>  g_xUcsiMailBox_ds --> CCGx");
	/* Note that CONTROL data has always to be written after MESSAGE_OUT data is written */
	amd_crb_drivers_hpi_ucsiRegAccess(HPI_WRITE, UCSI_MAILBOX_OFFSET(msgOut), (void *)&g_xUcsiMailBox_ds.msgOut, sizeof(APP_UCSI_MESSAGE), chipID);
	amd_crb_drivers_hpi_ucsiRegAccess(HPI_WRITE, UCSI_MAILBOX_OFFSET(ctrl),   (void *)&g_xUcsiMailBox_ds.ctrl,   sizeof(APP_UCSI_CONTROL), chipID);
	
	/* Write to ctrl means OPM data is available for CCGx */
}

/**
 * app_ucsi_writeHpiTunnel
 *
 * @note
 * CCGx hpi write to whole mailbox
 */
void app_ucsi_writeHpiTunnel(void)
{
	uint8_t portNum;

	/* identify command type: common, single or error */
	portNum = amd_crb_drivers_ec_ucsi_cypdSelector(&(g_xUcsiMailBox_ds.ctrl.u8Ctrl[0]), ucsi_PrevCommPort);
	switch (portNum) {
		case TYPEC_PORT_0_IDX:
			app_ucsi_writeHpiMailbox(0);
			ucsi_resp_cnt = 1;
			break;
		case TYPEC_PORT_1_IDX:
			app_ucsi_writeHpiMailbox(0);
			ucsi_resp_cnt = 1;
			break;
		case TYPEC_PORT_2_IDX:
			/* Adjust the port number */
			amd_crb_drivers_ec_ucsi_cypdAdjustPort(&(g_xUcsiMailBox_ds.ctrl.u8Ctrl[0]));
			app_ucsi_writeHpiMailbox(1);
			ucsi_resp_cnt = 1;
			break;
		case 0xCC: /* common cmd */
			app_ucsi_writeHpiMailbox(1);
			app_ucsi_writeHpiMailbox(0);
			ucsi_resp_cnt = 2;
			break;
		case 0xEE: /* GET_ERROR_STATUS */
			g_ucsi_error_flag[0] = false;
			g_ucsi_error_flag[1] = false;
			app_ucsi_writeHpiMailbox(1);
			app_ucsi_writeHpiMailbox(0);
			ucsi_resp_cnt = 2;
			break;
		default:
			break;
	}
	ucsi_PrevCommPort = portNum;
}

/**
 * app_ucsi_readHpiMailbox
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * CCGx hpi read to mailbox for each chip
 */
void app_ucsi_readHpiMailbox (uint8_t chipID)
{
	/* CCGx --> g_xUcsiMailBox_ds */
	LOG_DBG("      <<<<  CCGx --> g_xUcsiMailBox_ds");
	amd_crb_drivers_hpi_ucsiRegAccess(HPI_READ, UCSI_MAILBOX_OFFSET(msgIn),   (void *)&g_xUcsiMailBox_ds.msgIn,  sizeof(APP_UCSI_MESSAGE), chipID);
	amd_crb_drivers_hpi_ucsiRegAccess(HPI_READ, UCSI_MAILBOX_OFFSET(CCI),     (void *)&g_xUcsiMailBox_ds.CCI,    sizeof(APP_UCSI_CCI), chipID);
	
#if PD_TRIPPORT_ENABLE	
	/* Force connector number to 3 in Get_Connector_Caap */
	app_ucsi_force_NumConnectors(3);
	/* chipID1: Change connector change indicator to 3 */
	app_ucsi_force_ConnectorChangeIndicator(chipID);
#endif
}

/**
 * app_ucsi_readHpiTunnel
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * CCGx hpi read to whole mailbox
 */
void app_ucsi_readHpiTunnel(uint8_t chipID)
{
	if (g_isUcsiCycleFinished == false) {
		uint8_t portNum = amd_crb_drivers_ec_ucsi_cypdSelector(&(g_xUcsiMailBox_ds.ctrl.u8Ctrl[0]), ucsi_PrevCommPort);

		/* previous common command for both CCG6 */
		if (portNum == 0xCC) {
			if (chipID == 0) {
				app_ucsi_readHpiMailbox(chipID);
			}
			else {
				/* dummy read chip1 response for common message */
				app_ucsi_dummy_readHpiMailbox(chipID);
			}
		}
		/* previous get_error command */
		else if (portNum == 0xEE) {
			g_ucsi_error_flag[chipID] = true;
			app_ucsi_error_readHpiMailbox(chipID);
			if ((g_ucsi_error_flag[0] == true) && (g_ucsi_error_flag[1] == true)) {
				g_ucsi_error_status[0].CCI |= g_ucsi_error_status[1].CCI;
				for (uint8_t index = 0; index < (sizeof(g_ucsi_error_status[0].msgIn.u32Buf) >> 2); index++) {
					g_ucsi_error_status[0].msgIn.u32Buf[index] |= g_ucsi_error_status[1].msgIn.u32Buf[index];
				}
				memcpy((void *)&g_xUcsiMailBox_ds.msgIn, (const void *)&g_ucsi_error_status[0].msgIn,  sizeof(APP_UCSI_MESSAGE));
				memcpy((void *)&g_xUcsiMailBox_ds.CCI,   (const void *)&g_ucsi_error_status[0].CCI,    sizeof(APP_UCSI_CCI));
			}
		}
		/* Single CCG6 command, only read one */
		else {
			app_ucsi_readHpiMailbox(chipID);
		}
	}
	else {
		/* Initial interrupt, not response */
		app_ucsi_readHpiMailbox(chipID);
	}
	/* Wait both PD controller get resposne */
	if (ucsi_resp_cnt)
		ucsi_resp_cnt--;

	/* All resposne read from CYPD */
	if (ucsi_resp_cnt == 0) {
		_s_ucsi_Ppm2OpmFlag = true;
	}
}
#if PD_TRIPPORT_ENABLE
/**
 * app_ucsi_dummy_readHpiMailbox
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * CCGx hpi dummy read from mailbox
 */
void app_ucsi_dummy_readHpiMailbox (uint8_t chipID)
{
	F_UCSI_MAILBOX dummy;

	/* Dummy read the CCG6SF to maintain sequence but not response to OS. */
	LOG_DBG("      <<<<  CCGx --> dummy");
	amd_crb_drivers_hpi_ucsiRegAccess(HPI_READ, UCSI_MAILBOX_OFFSET(msgIn),   (void *)&dummy.msgIn,  sizeof(APP_UCSI_MESSAGE), chipID);
	amd_crb_drivers_hpi_ucsiRegAccess(HPI_READ, UCSI_MAILBOX_OFFSET(CCI),     (void *)&dummy.CCI,    sizeof(APP_UCSI_CCI), chipID);
}

/**
 * app_ucsi_error_readHpiMailbox
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * CCGx hpi error dummy read from mailbox
 */
void app_ucsi_error_readHpiMailbox (uint8_t chipID)
{
	/* Dummy read both CCG6DF and SSG6SF error info. Then combine them together to response to OS. */
	LOG_DBG("      <<<<  CCGx --> dummy error");
	amd_crb_drivers_hpi_ucsiRegAccess(HPI_READ, UCSI_MAILBOX_OFFSET(msgIn),   (void *)&g_ucsi_error_status[chipID].msgIn,  sizeof(APP_UCSI_MESSAGE), chipID);
	amd_crb_drivers_hpi_ucsiRegAccess(HPI_READ, UCSI_MAILBOX_OFFSET(CCI),     (void *)&g_ucsi_error_status[chipID].CCI,    sizeof(APP_UCSI_CCI), chipID);
}
#endif /* PD_TRIPPORT_ENABLE */
#endif /* CONFIG_USBC_CCGX */

/***************************************************************************/
/**                         ITE PD Module UCSI                            **/
/***************************************************************************/
#if CONFIG_USBC_ITE
/**
 * app_ucsi_writeIteMailbox
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * ITE write to mailbox for each chip
 */
void app_ucsi_writeIteMailbox (uint8_t chipID)
{
	LOG_DBG("      >>>>  g_xUcsiMailBox_ds --> ITEx");
	/* Note that CONTROL data has always to be written after MESSAGE_OUT data is written */
	amd_crb_drivers_itex_regAccess(ITE_REG_WRITE, ITEPD_REG_UCSI_MSGOUT, (void *)&g_xUcsiMailBox_ds.msgOut, sizeof(APP_UCSI_MESSAGE), chipID << 1);
	amd_crb_drivers_itex_regAccess(ITE_REG_WRITE, ITEPD_REG_UCSI_CTRL,   (void *)&g_xUcsiMailBox_ds.ctrl,   sizeof(APP_UCSI_CONTROL), chipID << 1);

	/* Write to ctrl means OPM data is available for CCGx */
}

/**
 * app_ucsi_readIteMailbox
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * Ite read to mailbox for each chip
 */
void app_ucsi_readIteMailbox (uint8_t chipID)
{
	LOG_DBG("      <<<<  ITEx --> g_xUcsiMailBox_ds");
	amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_UCSI_MSGIN,   (void *)&g_xUcsiMailBox_ds.msgIn,  sizeof(APP_UCSI_MESSAGE), chipID << 1);
	amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_UCSI_CCI,     (void *)&g_xUcsiMailBox_ds.CCI,    sizeof(APP_UCSI_CCI),     chipID << 1);

	/* Force connector number to 3 in Get_Connector_Caap */
	app_ucsi_force_NumConnectors(3);
	/* chipID1: Change connector change indicator to 3 */
	app_ucsi_force_ConnectorChangeIndicator(chipID);
}

/**
 * app_ucsi_readIteTunnel
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * ITE read to whole mailbox
 */
void app_ucsi_readIteTunnel(uint8_t chipID)
{
	if (g_isUcsiCycleFinished == false)
	{
		uint8_t portNum = amd_crb_drivers_ec_ucsi_cypdSelector(&(g_xUcsiMailBox_ds.ctrl.u8Ctrl[0]), ucsi_PrevCommPort);

		/* previous common command for both ITEs */
		if (portNum == 0xCC)
		{
			if (chipID == 0)
			{
				app_ucsi_readIteMailbox(chipID);
			}
			else
			{
				/* dummy read chip1 response for common message */
				app_ucsi_dummy_readIteMailbox(chipID);
			}
		}
		/* previous get_error command */
		else if (portNum == 0xEE)
		{
			g_ucsi_error_flag[chipID] = true;
			app_ucsi_error_readIteMailbox(chipID);
			if ((g_ucsi_error_flag[0] == true) && (g_ucsi_error_flag[1] == true))
			{
				g_ucsi_error_status[0].CCI |= g_ucsi_error_status[1].CCI;
				for (uint8_t index = 0; index < (sizeof(g_ucsi_error_status[0].msgIn.u32Buf) >> 2); index++)
				{
					g_ucsi_error_status[0].msgIn.u32Buf[index] |= g_ucsi_error_status[1].msgIn.u32Buf[index];
				}
				memcpy((void *)&g_xUcsiMailBox_ds.msgIn, (const void *)&g_ucsi_error_status[0].msgIn,  sizeof(APP_UCSI_MESSAGE));
				memcpy((void *)&g_xUcsiMailBox_ds.CCI,   (const void *)&g_ucsi_error_status[0].CCI,    sizeof(APP_UCSI_CCI));
			}
		}
		/* Single ITEs command, only read one */
		else
		{
			app_ucsi_readIteMailbox(chipID);
		}
	}
	else
	{
		/* Initial interrupt, not response */
		app_ucsi_readIteMailbox(chipID);
	}
	/* Wait both PD controller get resposne */
	if (ucsi_resp_cnt)
		ucsi_resp_cnt--;

	/* All resposne read from CYPD */
	if (ucsi_resp_cnt == 0)
	{
		_s_ucsi_Ppm2OpmFlag = true;
	}
}

/**
 * app_ucsi_writeIteTunnel
 *
 * @note
 * ITE write to whole mailbox
 */
void app_ucsi_writeIteTunnel(void)
{
	uint8_t portNum;

	/* identify command type: common, single or error */
	portNum = amd_crb_drivers_ec_ucsi_cypdSelector(&(g_xUcsiMailBox_ds.ctrl.u8Ctrl[0]), ucsi_PrevCommPort);
	switch (portNum)
	{
		case TYPEC_PORT_0_IDX:
			app_ucsi_writeIteMailbox(0);
			ucsi_resp_cnt = 1;
			break;
		case TYPEC_PORT_1_IDX:
			app_ucsi_writeIteMailbox(0);
			ucsi_resp_cnt = 1;
			break;
		case TYPEC_PORT_2_IDX:
			/* Adjust the port number */
			amd_crb_drivers_ec_ucsi_cypdAdjustPort(&(g_xUcsiMailBox_ds.ctrl.u8Ctrl[0]));
			app_ucsi_writeIteMailbox(1);
			ucsi_resp_cnt = 1;
			break;
		case 0xCC: /* common cmd */
			app_ucsi_writeIteMailbox(1);
			app_ucsi_writeIteMailbox(0);
			ucsi_resp_cnt = 2;
			break;
		case 0xEE: /* GET_ERROR_STATUS */
			g_ucsi_error_flag[0] = false;
			g_ucsi_error_flag[1] = false;
			app_ucsi_writeIteMailbox(1);
			app_ucsi_writeIteMailbox(0);
			ucsi_resp_cnt = 2;
			break;
		default:
			break;
	}
	ucsi_PrevCommPort = portNum;
}
#if PD_TRIPPORT_ENABLE
/**
 * app_ucsi_dummy_readIteMailbox
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * ITEx dummy read from mailbox
 */
void app_ucsi_dummy_readIteMailbox (uint8_t chipID)
{
	F_UCSI_MAILBOX dummy;

	/* Dummy read the ITE to maintain sequence but not response to OS. */
	LOG_DBG("      <<<<  ITEx --> dummy");
	amd_crb_drivers_itex_regAccess(HPI_READ, ITEPD_REG_UCSI_MSGIN,   (void *)&dummy.msgIn,  sizeof(APP_UCSI_MESSAGE), chipID << 1);
	amd_crb_drivers_itex_regAccess(HPI_READ, ITEPD_REG_UCSI_CCI,     (void *)&dummy.CCI,    sizeof(APP_UCSI_CCI),     chipID << 1);
}

/**
 * app_ucsi_error_readIteMailbox
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * ITEx dummy read from mailbox for error status
 */
void app_ucsi_error_readIteMailbox (uint8_t chipID)
{
	/* Dummy read both ITE PD error info. Then combine them together to response to OS. */
	LOG_DBG("      <<<<  ITEx --> dummy error");
	amd_crb_drivers_itex_regAccess(HPI_READ, ITEPD_REG_UCSI_MSGIN,   (void *)&g_ucsi_error_status[chipID].msgIn,  sizeof(APP_UCSI_MESSAGE), chipID << 1);
	amd_crb_drivers_itex_regAccess(HPI_READ, ITEPD_REG_UCSI_CCI,     (void *)&g_ucsi_error_status[chipID].CCI,    sizeof(APP_UCSI_CCI),     chipID << 1);
}
#endif /* PD_TRIPPORT_ENABLE */
#endif /* CONFIG_USBC_ITE */

/***************************************************************************/
/**                         TI PD Module UCSI                             **/
/***************************************************************************/
#if CONFIG_EC_UCSI_ENABLE
/**
 * app_ucsi_tunnel_writeECMailbox
 *
 * @note
 * write EC UCSI regs
 */
void app_ucsi_tunnel_writeECMailbox (void)
{
	LOG_DBG("      >>>>  g_xUcsiMailBox_ds --> EC");
	/* Note that CONTROL data has always to be written after MESSAGE_OUT data is written */
	amd_crb_drivers_ec_ucsi_regAccess(TI_REG_WRITE, UCSI_MAILBOX_OFFSET(msgOut), (void *)&g_xUcsiMailBox_ds.msgOut, sizeof(APP_UCSI_MESSAGE));
	amd_crb_drivers_ec_ucsi_regAccess(TI_REG_WRITE, UCSI_MAILBOX_OFFSET(ctrl),   (void *)&g_xUcsiMailBox_ds.ctrl,   sizeof(APP_UCSI_CONTROL));
}

/**
 * app_ucsi_tunnel_readECMailbox
 *
 * @note
 * read EC UCSI regs
 */
void app_ucsi_tunnel_readECMailbox (void)
{
	LOG_DBG("      <<<<  EC --> g_xUcsiMailBox_ds");
	amd_crb_drivers_ec_ucsi_regAccess(TI_REG_READ, UCSI_MAILBOX_OFFSET(msgIn),   (void *)&g_xUcsiMailBox_ds.msgIn,  sizeof(APP_UCSI_MESSAGE));
	amd_crb_drivers_ec_ucsi_regAccess(TI_REG_READ, UCSI_MAILBOX_OFFSET(CCI),     (void *)&g_xUcsiMailBox_ds.CCI,    sizeof(APP_UCSI_CCI));
}
#endif /* CONFIG_EC_UCSI_ENABLE */

/***************************************************************************/
/**                        RTK PD Module UCSI                             **/
/***************************************************************************/
#if CONFIG_USBC_RTK
/**
 * app_ucsi_tunnel_writeRtkMailbox
 *
 * @note
 * write EC UCSI regs
 */
void app_ucsi_tunnel_writeRtkMailbox (void)
{
	LOG_DBG("      >>>>  g_xUcsiMailBox_ds --> RTK");
	/* Note that CONTROL data has always to be written after MESSAGE_OUT data is written */
	amd_crb_drivers_rtk_ucsi_regAccess(RTK_REG_WRITE, UCSI_MAILBOX_OFFSET(msgOut), (void *)&g_xUcsiMailBox_ds.msgOut, sizeof(APP_UCSI_MESSAGE));
	amd_crb_drivers_rtk_ucsi_regAccess(RTK_REG_WRITE, UCSI_MAILBOX_OFFSET(ctrl),   (void *)&g_xUcsiMailBox_ds.ctrl,   sizeof(APP_UCSI_CONTROL));
}

/**
 * app_ucsi_tunnel_readRtkMailbox
 *
 * @note
 * read EC UCSI regs
 */
void app_ucsi_tunnel_readRtkMailbox (void)
{
	LOG_DBG("      <<<<  RTK --> g_xUcsiMailBox_ds");
	amd_crb_drivers_rtk_ucsi_regAccess(RTK_REG_READ, UCSI_MAILBOX_OFFSET(msgIn),   (void *)&g_xUcsiMailBox_ds.msgIn,  sizeof(APP_UCSI_MESSAGE));
	amd_crb_drivers_rtk_ucsi_regAccess(RTK_REG_READ, UCSI_MAILBOX_OFFSET(CCI),     (void *)&g_xUcsiMailBox_ds.CCI,    sizeof(APP_UCSI_CCI));
}
#endif

/***********************************************************************************/
/**                                 UCSI handler functions                        **/
/***********************************************************************************/

/**
 * app_ucsi_tunnel_init
 *
 * @param [in]         deviceID;     pd device id
 * @param [in]  pUcsiMailBoxBuf;     ucsi sharing buffer pointer
 *
 * @note
 *  Initiate ucsi feature
 */
void app_ucsi_tunnel_init (uint8_t deviceID, F_UCSI_MAILBOX * pUcsiMailBoxBuf)
{
	g_xUcsiMailBoxPtr = pUcsiMailBoxBuf; // upstream	OPM (HOST <-> EC)

	/* Empty ucsi tunnel mailbox */
	memset((void *)g_xUcsiMailBoxPtr, 0,  sizeof(F_UCSI_MAILBOX));
	memset((void *)&g_xUcsiMailBox_ds, 0, sizeof(g_xUcsiMailBox_ds));
	LOG_DBG("Size of F_UCSI_MAILBOX is %d", sizeof(F_UCSI_MAILBOX));
#if CONFIG_USBC_CCGX
	ucsi_PrevCommPort = TYPEC_PORT_0_IDX;
#endif
	if (deviceID != 0) {
		LOG_DBG("UCSI ID %d", deviceID);
		app_ucsi_tunnel_writeDevID(deviceID);
	}
	else {
		LOG_DBG("UCSI ID default %d", PD_DEVICE_ID_TIx);
	}

#if CONFIG_USBC_CCGX
	/* Init ucsi for CCGx pd controller */
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_CCGx) {
		bool isPortEnabled;

#if PD_TRIPPORT_ENABLE
		isPortEnabled = (app_ucsi_tunnel_start(0) & app_ucsi_tunnel_start(1));
#else
		isPortEnabled = app_ucsi_tunnel_start(0);
#endif

		if (isPortEnabled) {
			/* read VERSION and CCI to both g_xUcsiMailBoxPtr and g_xUcsiMailBox_ds */
			app_ucsi_tunnel_ReadVersion();
		}

		if (g_xUcsiMailBox_ds.version == 0x0000 &&
			g_xUcsiMailBox_ds.CCI == 0x00000000) {
			g_isUcsiSupported = false;
		} else {
			g_isUcsiSupported = true;
		}

		LOG_DBG("UCSI supported flag: %s", g_isUcsiSupported ? "Yes" : "No");
	}
#endif

#if CONFIG_EC_UCSI_ENABLE
	/* Init ucsi for TI pd controller */
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_TIx) {
		/* Init the ec ucsi feature */
		amd_crb_drivers_ec_ucsi_init();
		/* Read the supported alt mode info */
		amd_crb_drivers_tps_altModeInfo(TYPEC_PORT_0_IDX);
		amd_crb_drivers_tps_devSrcCapInfo(TYPEC_PORT_0_IDX, 1);
		/* Refrash the current src and snk cap */
		amd_crb_drivers_tps_refreashDevSnkCapCurrent(TYPEC_PORT_0_IDX);
		amd_crb_drivers_tps_refreashDevSrcCapCurrent(TYPEC_PORT_0_IDX);
#if PD_DUALPORT_ENABLE
		/* Read the supported alt mode info */
		amd_crb_drivers_tps_altModeInfo(TYPEC_PORT_1_IDX);
		amd_crb_drivers_tps_devSrcCapInfo(TYPEC_PORT_1_IDX, 1);
		/* Refrash the current src and snk cap */
		amd_crb_drivers_tps_refreashDevSnkCapCurrent(TYPEC_PORT_1_IDX);
		amd_crb_drivers_tps_refreashDevSrcCapCurrent(TYPEC_PORT_1_IDX);
#endif
#if PD_TRIPPORT_ENABLE
		/* Read the supported alt mode info */
		amd_crb_drivers_tps_altModeInfo(TYPEC_PORT_2_IDX);
		amd_crb_drivers_tps_devSrcCapInfo(TYPEC_PORT_2_IDX, 1);
		/* Refrash the current src and snk cap */
		amd_crb_drivers_tps_refreashDevSnkCapCurrent(TYPEC_PORT_2_IDX);
		amd_crb_drivers_tps_refreashDevSrcCapCurrent(TYPEC_PORT_2_IDX);
#endif

		/* Read default src and snk cap */
		amd_crb_drivers_tps_devSnkCapInfo(TYPEC_PORT_0_IDX, 1);
#if PD_DUALPORT_ENABLE
		amd_crb_drivers_tps_devSnkCapInfo(TYPEC_PORT_1_IDX, 1);
#endif
#if PD_TRIPPORT_ENABLE
		amd_crb_drivers_tps_devSnkCapInfo(TYPEC_PORT_2_IDX, 1);
#endif
		app_ucsi_tunnel_ReadVersion();
		g_isUcsiSupported = true;
		LOG_DBG("UCSI supported flag: %s", g_isUcsiSupported ? "Yes" : "No");
	}
#endif /* CONFIG_EC_UCSI_ENABLE */

#if CONFIG_USBC_ITE
	/* Init ucsi for ITEx pd controller */
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_ITEx) {
		/* read VERSION and CCI to both g_xUcsiMailBoxPtr and g_xUcsiMailBox_ds */
		app_ucsi_tunnel_ReadVersion();
		g_isUcsiSupported = true;

		LOG_DBG("UCSI supported flag: %s", g_isUcsiSupported ? "Yes" : "No");
	}
#endif

#if CONFIG_USBC_RTK
	/* Init ucsi for RTK pd controller */
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_RTKx) {

		/* init rtk ucsi */
		amd_crb_drivers_rtk_ucsi_init();

		/* read VERSION and CCI to both g_xUcsiMailBoxPtr and g_xUcsiMailBox_ds */
		app_ucsi_tunnel_ReadVersion();

		g_isUcsiSupported = true;
		LOG_DBG("UCSI supported flag: %s", g_isUcsiSupported ? "Yes" : "No");
	}
#endif
}

/**
 * app_ucsi_force_NumConnectors
 *
 * @param [in]         number;    update the connector number
 *
 * @note
 * update connector number as the original port number are two in max
 */
void app_ucsi_force_NumConnectors (uint8_t number)
{
	/* over write get_capability bNumConnectors */
	if (g_xUcsiMailBox_ds.ctrl.u8Ctrl[0] == UCSI_CMD_GET_CAPABILITY) {
		g_xUcsiMailBox_ds.msgIn.u8Buf[4] = number;
		LOG_DBG("Force bNumConnectors: %x, %x, %x, %x", g_xUcsiMailBox_ds.msgIn.u32Buf[0],
														g_xUcsiMailBox_ds.msgIn.u32Buf[1],
														g_xUcsiMailBox_ds.msgIn.u32Buf[2],
													  	g_xUcsiMailBox_ds.msgIn.u32Buf[3]);
	}
}

/**
 * app_ucsi_force_ConnectorChangeIndicator
 *
 * @param [in]         chipID;    chip index
 *
 * @note
 * change the connector indicate for chipID 1
 */
void app_ucsi_force_ConnectorChangeIndicator (uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE
	if ((chipID == 1) && (g_xUcsiMailBox_ds.CCI & 0xFE)) {
		/* Change connector change indicator to 3 */
		g_xUcsiMailBox_ds.CCI &= ~0xFE;
		g_xUcsiMailBox_ds.CCI |= (0x06); /* 0x02--P0, 0x04--P1, 0x06--P2 */
		LOG_DBG("Force CCI:%x", g_xUcsiMailBox_ds.CCI);
	}
#endif
}

/**
 * app_ucsi_tunnel_start
 *
 * @param [in]         chipID;     chip index
 * @return true If successful.
 *
 * @note
 *  ucsi tunnel start
 */
bool app_ucsi_tunnel_start(uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
#if CONFIG_USBC_CCGX
	ucsi_PrevCommPort = TYPEC_PORT_0_IDX;

	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_CCGx) {
		uint8_t returnCodes[] = {HPI_RESP_SUCCESS, HPI_RESP_PD_COMMAND_FAILED, HPI_RESP_UCSI_IS_ALREADY_STARTED};
		uint32_t attempts = 0;

		/**
		 * According to CCGx UCSI interface guide, EC should retry
		 * every few 10s of milliseconds up to a MAX TIME DELAY of
		 * 800 milliseconds.
		 */
		do {
			/* 0x01 = Start UCSI */
			if (0 == amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0039, "\x01", 1, chipID)) {
				uint8_t resp = amd_crb_drivers_hpi_wait4DevRespArray(returnCodes, sizeof(returnCodes) / sizeof(uint8_t), 2000, chipID);
				switch (resp) {
					case HPI_RESP_SUCCESS:
						LOG_DBG("chipID:%d, UCSI START: Port 0/1 are enabled successfully.", chipID);
						return true;
					case HPI_RESP_UCSI_IS_ALREADY_STARTED:
						LOG_DBG("chipID:%d, UCSI START: Port 0/1 are already enabled.", chipID);
						return true;
					case HPI_RESP_PD_COMMAND_FAILED:
						//k_sleep(K_USEC(5));
					default:
						break;
				}
			}
			attempts++;
		} while (attempts < 3);  /* 2 cycles retry */

		LOG_DBG("chipID:%d, UCSI START: Port 0/1 are *NOT* enabled after several trials.",chipID);
		return false;
	}
	else
#endif
	{
		return true;
	}
}

/**
 * app_ucsi_tunnel_end
 *
 * @param [in]         chipID;     chip index
 * @return true If successful.
 *
 * @note
 *  ucsi tunnel end
 */
bool app_ucsi_tunnel_end(uint8_t chipID)
{
#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
#if CONFIG_USBC_CCGX
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_CCGx) {
		uint8_t returnCodes[] = {HPI_RESP_SUCCESS, HPI_RESP_PD_COMMAND_FAILED};
		uint32_t attempts = 0;

		/**
		 * According to CCGx UCSI interface guide, EC should retry
		 * every few 10s of milliseconds up to a MAX TIME DELAY of
		 * 800 milliseconds.
		 */
		do {
			/* 0x02 = Stop UCSI */
			if (0 == amd_crb_drivers_hpi_regAccess(HPI_WRITE, 0x0039, "\x02", 1, chipID)) {
				uint8_t resp = amd_crb_drivers_hpi_wait4DevRespArray(returnCodes, sizeof(returnCodes) / sizeof(uint8_t), 2000, chipID);
				switch (resp) {
					case HPI_RESP_SUCCESS:
						LOG_DBG("UCSI END: Port 0/1 are disabled successfully.");
						return true;
					case HPI_RESP_PD_COMMAND_FAILED:
						//k_sleep(K_USEC(5));
					default:
						break;
				}
			}
			attempts++;
		} while (attempts < 3);  /* 2 cycle retry */

		LOG_DBG("UCSI END: Port 0/1 are *NOT* disabled after several trials.");
		return false;
	}
	else
#endif
	{
		return true;
	}
}

/**
 * app_ucsi_tunnel_AcpiSpaceHandler
 *
 * @param [in]         isRead;     read or write
 * @param [in]         ui8Idx;     ECRAM offset
 * @param [in]       pui8Data;     ECRAM value
 *
 * @note
 * Below function is running in PUREMAIN thread.
 * Handle acpi interface for ucsi packet sending/receiving
 */
void app_ucsi_tunnel_AcpiSpaceHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (!g_ucsiTunnelEnabled || !g_xUcsiMailBoxPtr)
		return;

	if (ui8Idx < ACPI_UCSI_BASE || ui8Idx >= ACPI_UCSI_BASE + F_UCSI_SIZEOF_MAILBOX)
		return;

	if (isRead) {
		if (pui8Data) *pui8Data = *(((uint8_t *)g_xUcsiMailBoxPtr) + ACPI_IDX_TO_OFFSET(ui8Idx));

	} else if (!pui8Data){
		return;
	} else {
		*(((uint8_t *)g_xUcsiMailBoxPtr) + ACPI_IDX_TO_OFFSET(ui8Idx)) = *pui8Data;
	}
	/* Signal USBC task */
	app_usbc_giveSem();
}

/**
 * app_ucsi_tunnel_syncPpm2Opm
 *
 * @note
 * Copy the PPM buffer to OPM buffer
 */
void app_ucsi_tunnel_syncPpm2Opm (void)
{
	/*              desc                                    src                        size                 */
	memcpy((void *)&g_xUcsiMailBoxPtr->msgIn, (const void *)&g_xUcsiMailBox_ds.msgIn,  sizeof(APP_UCSI_MESSAGE));
	memcpy((void *)&g_xUcsiMailBoxPtr->CCI,   (const void *)&g_xUcsiMailBox_ds.CCI,    sizeof(APP_UCSI_CCI));
#if 0
	printk("OP_CCI: %x %x %x %x\n",
			(uint8_t)(g_xUcsiMailBox_ds.CCI), (uint8_t)(g_xUcsiMailBox_ds.CCI >> 8) ,
			(uint8_t)(g_xUcsiMailBox_ds.CCI >> 16), (uint8_t)(g_xUcsiMailBox_ds.CCI >> 24) );

	printk("OP_MIN: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			g_xUcsiMailBox_ds.msgIn.u8Buf[0], g_xUcsiMailBox_ds.msgIn.u8Buf[1] ,
			g_xUcsiMailBox_ds.msgIn.u8Buf[2], g_xUcsiMailBox_ds.msgIn.u8Buf[3] ,
            g_xUcsiMailBox_ds.msgIn.u8Buf[4], g_xUcsiMailBox_ds.msgIn.u8Buf[5] ,
			g_xUcsiMailBox_ds.msgIn.u8Buf[6], g_xUcsiMailBox_ds.msgIn.u8Buf[7] ,
		    g_xUcsiMailBox_ds.msgIn.u8Buf[8], g_xUcsiMailBox_ds.msgIn.u8Buf[9] ,
		    g_xUcsiMailBox_ds.msgIn.u8Buf[10], g_xUcsiMailBox_ds.msgIn.u8Buf[11] ,
		    g_xUcsiMailBox_ds.msgIn.u8Buf[12], g_xUcsiMailBox_ds.msgIn.u8Buf[13] ,
		    g_xUcsiMailBox_ds.msgIn.u8Buf[14], g_xUcsiMailBox_ds.msgIn.u8Buf[15]);
#endif
}

/**
 * app_ucsi_tunnel_syncOpm2Ppm
 *
 * @note
 * Copy the OPM buffer to PPM buffer
 */
void app_ucsi_tunnel_syncOpm2Ppm (void)
{
	/*              desc                                    src                        size                  */
	memcpy((void *)&g_xUcsiMailBox_ds.ctrl,   (const void *)&g_xUcsiMailBoxPtr->ctrl,  sizeof(APP_UCSI_CONTROL));
	memcpy((void *)&g_xUcsiMailBox_ds.msgOut, (const void *)&g_xUcsiMailBoxPtr->msgOut,sizeof(APP_UCSI_MESSAGE));
#if 0
	printk("OP_CTRL: %x %x %x %x %x %x %x %x\n", g_xUcsiMailBox_ds.ctrl.u8Ctrl[0], g_xUcsiMailBox_ds.ctrl.u8Ctrl[1]
								               , g_xUcsiMailBox_ds.ctrl.u8Ctrl[2], g_xUcsiMailBox_ds.ctrl.u8Ctrl[3]
											   , g_xUcsiMailBox_ds.ctrl.u8Ctrl[4], g_xUcsiMailBox_ds.ctrl.u8Ctrl[5]
											   , g_xUcsiMailBox_ds.ctrl.u8Ctrl[6], g_xUcsiMailBox_ds.ctrl.u8Ctrl[7]);

	printk("OP_MOUT: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			g_xUcsiMailBox_ds.msgOut.u8Buf[0], g_xUcsiMailBox_ds.msgOut.u8Buf[1] ,
			g_xUcsiMailBox_ds.msgOut.u8Buf[2], g_xUcsiMailBox_ds.msgOut.u8Buf[3] ,
            g_xUcsiMailBox_ds.msgOut.u8Buf[4], g_xUcsiMailBox_ds.msgOut.u8Buf[5] ,
			g_xUcsiMailBox_ds.msgOut.u8Buf[6], g_xUcsiMailBox_ds.msgOut.u8Buf[7] ,
		    g_xUcsiMailBox_ds.msgOut.u8Buf[8], g_xUcsiMailBox_ds.msgOut.u8Buf[9] ,
		    g_xUcsiMailBox_ds.msgOut.u8Buf[10], g_xUcsiMailBox_ds.msgOut.u8Buf[11] ,
		    g_xUcsiMailBox_ds.msgOut.u8Buf[12], g_xUcsiMailBox_ds.msgOut.u8Buf[13] ,
		    g_xUcsiMailBox_ds.msgOut.u8Buf[14], g_xUcsiMailBox_ds.msgOut.u8Buf[15]);
#endif
}

/**
 * app_ucsi_tunnel_ReadVersion
 *
 * @note
 * Read ucsi version and cci from hpi reg
 */
void app_ucsi_tunnel_ReadVersion(void)
{
	LOG_DBG("load version");
#if CONFIG_USBC_CCGX
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_CCGx) {
		LOG_DBG("Read CCGx the UCSI Version");
		/** Read VERSION and CCI to both g_xUcsiMailBoxPtr-> and g_xUcsiMailBox_ds,
		 * Note that VERSION only be read in the first time
		 */
		/* If there are two cypress PD contrllers, the UCSI version should be same. So only read the chip 0 */
		amd_crb_drivers_hpi_ucsiRegAccess(HPI_READ, UCSI_MAILBOX_OFFSET(version), (void *)&g_xUcsiMailBox_ds.version, sizeof(APP_UCSI_VERSION), 0);
		amd_crb_drivers_hpi_ucsiRegAccess(HPI_READ, UCSI_MAILBOX_OFFSET(CCI),     (void *)&g_xUcsiMailBox_ds.CCI,     sizeof(APP_UCSI_CCI), 0);
	}
#endif

#if CONFIG_EC_UCSI_ENABLE
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_TIx) {
		LOG_DBG("Read EC the UCSI Version");
		/** Read VERSION and CCI to both g_xUcsiMailBoxPtr-> and g_xUcsiMailBox_ds,
		 * Note that VERSION only be read in the first time
		 */
		amd_crb_drivers_ec_ucsi_regAccess(TI_REG_READ, UCSI_MAILBOX_OFFSET(version), (void *)&g_xUcsiMailBox_ds.version, sizeof(APP_UCSI_VERSION));
		amd_crb_drivers_ec_ucsi_regAccess(TI_REG_READ, UCSI_MAILBOX_OFFSET(CCI),     (void *)&g_xUcsiMailBox_ds.CCI,     sizeof(APP_UCSI_CCI));
	}
#endif

#if CONFIG_USBC_ITE
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_ITEx) {
		LOG_DBG("Read ITEx the UCSI Version");
		/* Read VERSION and CCI to both g_xUcsiMailBoxPtr-> and g_xUcsiMailBox_ds,
		 * Note that VERSION only be read in the first time
		 */
		/* If there are two cypress PD contrllers, the UCSI version should be same. So only read the chip 0 */
		amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_UCSI_VRESION, (void *)&g_xUcsiMailBox_ds.version, sizeof(APP_UCSI_VERSION), 0);
		amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_UCSI_CCI,     (void *)&g_xUcsiMailBox_ds.CCI,     sizeof(APP_UCSI_CCI),     0);

		/* force version to 2.1.0 0x0210 */
		g_xUcsiMailBox_ds.version = 0x0210;
	}
#endif

#if CONFIG_USBC_RTK
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_RTKx) {
		LOG_DBG("Read RTK the UCSI Version");
		/** Read VERSION and CCI to both g_xUcsiMailBoxPtr-> and g_xUcsiMailBox_ds,
		 * Note that VERSION only be read in the first time
		 */
		amd_crb_drivers_rtk_ucsi_regAccess(RTK_REG_READ, UCSI_MAILBOX_OFFSET(version), (void *)&g_xUcsiMailBox_ds.version, sizeof(APP_UCSI_VERSION));
		amd_crb_drivers_rtk_ucsi_regAccess(RTK_REG_READ, UCSI_MAILBOX_OFFSET(CCI),     (void *)&g_xUcsiMailBox_ds.CCI,     sizeof(APP_UCSI_CCI));
	}
#endif
	/*              desc                                       src                        size                 */
	memcpy((void *)&g_xUcsiMailBoxPtr->version, (const void *)&g_xUcsiMailBox_ds.version, sizeof(APP_UCSI_VERSION));
	memcpy((void *)&g_xUcsiMailBoxPtr->CCI,     (const void *)&g_xUcsiMailBox_ds.CCI,     sizeof(APP_UCSI_CCI));

	ACPI_NotifyHost(ACPI_SCI_UCSI);
}

/**
 * app_ucsi_tunnel_intHandler
 *
 * @param [in]         chipID;    chip index
 * @return true If successful.
 *
 * @note
 * EC UCSI interrupt handler
 */
bool app_ucsi_tunnel_intHandler (uint8_t chipID)
{
	/* PPM -> OPM */
#if CONFIG_USBC_CCGX
	/* update g_xUcsiMailBox_ds */
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_CCGx) {
#if PD_TRIPPORT_ENABLE
		app_ucsi_readHpiTunnel(chipID);
#else
		app_ucsi_readHpiMailbox(0);
		_s_ucsi_Ppm2OpmFlag = true;
#endif
	}
#endif

#if CONFIG_EC_UCSI_ENABLE
	/* TI PD not process chipID */
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_TIx) {
		app_ucsi_tunnel_readECMailbox();
		_s_ucsi_Ppm2OpmFlag = true;
	}
#endif

#if CONFIG_USBC_ITE
	/* update g_xUcsiMailBox_ds */
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_ITEx) {
#if PD_TRIPPORT_ENABLE
		app_ucsi_readIteTunnel(chipID);
#else
		app_ucsi_readHpiMailbox(0);
		_s_ucsi_Ppm2OpmFlag = true;
#endif
	}
#endif

#if CONFIG_USBC_RTK
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_RTKx) {
		app_ucsi_tunnel_readRtkMailbox();
		_s_ucsi_Ppm2OpmFlag = true;
	}
#endif

	/* Signal USBC task */
	app_usbc_giveSem();

     _s_ucsi_Ppm2OpmTimeout = true;
	
	/**
	 * return true to cause the UCSI INT bit
	 * be cleared at the end of current main
	 * interrupt handler loop.
	 */
	return true;
}

/**
 * app_ucsi_tunnel_Ppm2Opm
 *
 * @note
 * process the ppm to opm command
 */
void app_ucsi_tunnel_Ppm2Opm (void)
{
	if (_s_ucsi_Ppm2OpmFlag) {
#if CONFIG_USBC_CCGX
		if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_CCGx) {
#if (CONFIG_EC_UCSI_ENABLE && PD_TRIPPORT_ENABLE)
			_s_ucsi_Ppm2OpmFlag = false;
#else
			_s_ucsi_Ppm2OpmFlag = false;
#endif
		}
#endif

#if CONFIG_EC_UCSI_ENABLE
		if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_TIx) {
			_s_ucsi_Ppm2OpmFlag = false;
		}
#endif

#if CONFIG_USBC_ITE
		if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_ITEx)
		{
#if (CONFIG_EC_UCSI_ENABLE && PD_TRIPPORT_ENABLE)
			_s_ucsi_Ppm2OpmFlag = false;
#else
			_s_ucsi_Ppm2OpmFlag = false;
#endif
		}
#endif

#if CONFIG_USBC_RTK
		if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_RTKx) {
			_s_ucsi_Ppm2OpmFlag = false;
		}
#endif
		app_ucsi_tunnel_syncPpm2Opm();

		/* trigger SCI */
		if (g_ucsiTunnelEnabled) {
			ACPI_NotifyHost(ACPI_SCI_UCSI);
			LOG_DBG(">>>>>>>>>ACPI_UCSI");
		}
	}
}

/**
 * app_ucsi_tunnel_Opm2Ppm
 *
 * @note
 * process the opm to ppm command
 */
void app_ucsi_tunnel_Opm2Ppm (void)
{
	if (_s_ucsi_Opm2PpmFlag) {
		_s_ucsi_Opm2PpmFlag = false;
#if CONFIG_USBC_CCGX
		if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_CCGx) {
#if PD_TRIPPORT_ENABLE
			app_ucsi_writeHpiTunnel();
#else
			app_ucsi_writeHpiMailbox(0);
#endif
		}
#endif

#if CONFIG_EC_UCSI_ENABLE
		if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_TIx) {
			app_ucsi_tunnel_writeECMailbox();
		}
#endif

#if CONFIG_USBC_ITE
		if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_ITEx) {
#if PD_TRIPPORT_ENABLE
			app_ucsi_writeIteTunnel();
#else
			app_ucsi_writeIteMailbox(0);
#endif
		}
#endif
#if CONFIG_USBC_RTK
		if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_RTKx) {
			app_ucsi_tunnel_writeRtkMailbox();
		}
#endif
	}
}

/**
 * app_ucsi_tunnel_task
 *
 * @note
 * EC ucsi main task in loop
 */
void app_ucsi_tunnel_task(void)
{
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_EC_UCSI_ENABLE
		amd_crb_drivers_ec_ucsi_task();
#endif
	}
#if CONFIG_USBC_CCGX
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_CCGx) {
		/* No task need for CCGx */
	}
#endif

#if CONFIG_USBC_ITE
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_ITEx) {

	}
#endif
#if CONFIG_USBC_RTK
	if (app_ucsi_tunnel_readDevID() == PD_DEVICE_ID_RTKx) {
		amd_crb_drivers_rtk_ucsi_task();
	}
#endif
}

/**
 * app_ucsi_tunnel_hostWriteDone
 *
 * @note
 * Below functions are running in main thread.
 * It will detect the write and read done event from Bios
 */
void app_ucsi_tunnel_hostWriteDone ()
{
	if (!g_ucsiTunnelEnabled)
		return;
	
	LOG_DBG("Receive 0xE0 (WRITE_DONE) command.");
	app_ucsi_tunnel_syncOpm2Ppm();
	_s_ucsi_Opm2PpmFlag = true;
	
	if (g_isUcsiCycleFinished == false || _s_ucsi_Ppm2OpmTimeout == true) {
		/* Workaroud for missing read-done: force read done */
#if CONFIG_EC_UCSI_ENABLE
		amd_crb_drivers_ec_ucsi_acpiReadDone();
#endif
#if CONFIG_USBC_RTK
		amd_crb_drivers_rtk_ucsi_acpiReadDone();
#endif
		_s_ucsi_Ppm2OpmTimeout = false;
	}

	g_isUcsiCycleFinished = false;

	/* Signal USBC task */
	app_usbc_giveSem();
}

/**
 * app_ucsi_tunnel_hostReadDone
 *
 * @note
 * Below functions are running in main thread.
 * It will detect the write and read done event from Bios
 */
void app_ucsi_tunnel_hostReadDone ()
{
	if (!g_ucsiTunnelEnabled)
		return;

	LOG_DBG("Receive 0xE1 (READ_DONE) command.");
#if CONFIG_EC_UCSI_ENABLE
	amd_crb_drivers_ec_ucsi_acpiReadDone();
#endif
#if CONFIG_USBC_RTK
	amd_crb_drivers_rtk_ucsi_acpiReadDone();
#endif
	
	g_isUcsiCycleFinished = true;
	_s_ucsi_Ppm2OpmTimeout = false;

	/* Signal USBC task */
	app_usbc_giveSem();
}