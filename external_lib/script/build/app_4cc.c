/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "amd_crb_drivers_spiFlash.h"
#if CONFIG_USBC_TIPD_TPS6599X
#include "amd_crb_drivers_tps6599x.h"
#endif
#if CONFIG_USBC_TIPD_TPS6699X
#include "amd_crb_drivers_tps6699x.h"
#endif
#include "app_4cc.h"
#include "board_config.h"
#include "system.h"
#include "app_usbc_task.h"
#if (CONFIG_BATTERY_MANAGEMENT)
#include "board_smtbty.h"
#endif

LOG_MODULE_REGISTER(4cc, CONFIG_4CC_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#ifndef _SKIP_TI_PATCH_DOWNLOAD_
#define _SKIP_TI_PATCH_DOWNLOAD_      0
#endif
#define _SKIP_TI_EVENT_MASK_CONFIG    0

typedef enum {
	APP_MODE =   1,
	PTCH_MODE =  2,
	VALID_MODE = 3
} APP_4CC_TIPD_MODE;

#define TI4CC_UFP_ONLY                  (1)
#define TI4CC_DFP_ONLY                  (2)
#define TI4CC_DRP                       (3)

#define TI4CC_DYN_SINKONLY              (0)

/* ************************** *
 *     static valuable        *
 * ************************** */
#if CONFIG_USBC_TIPD_TPS6599X
#define APP_4CC_NUM_OF_REGIONS  (2)
static uint32_t _s_4cc_regionPtrStart[APP_4CC_NUM_OF_REGIONS] = {0x0 , 0x400 };
static uint32_t _s_regionAddrPatchbundle[APP_4CC_NUM_OF_REGIONS] = {0x800, 0x4400};
#endif
static pd_do_t _s_xPort0SPdo = {0};
#if PD_DUALPORT_ENABLE
static pd_do_t _s_xPort1SPdo = {0};
#endif
#if PD_TRIPPORT_ENABLE
static pd_do_t _s_xPort2SPdo = {0};
#endif
#if CONFIG_USBC_TIPD_TPS6599X
static PD_APP_CONTEXT _s_4cc_appCtx;
#endif
static bool _s_prSwapRequest[NO_OF_TYPEC_PORTS] = {false};
static bool _s_drSwapRequest[NO_OF_TYPEC_PORTS] = {false};
#if CONFIG_EC_UCSI_ENABLE
extern uint8_t _s_ucsi_retryTimeCnt[NO_OF_TYPEC_PORTS];
#endif
static volatile uint8_t _s_4cc_eventPending = 0;
static struct k_timer g_4cc_retry_TimerId[NO_OF_TYPEC_PORTS];
#if TI4CC_DYN_SINKONLY
static uint8_t _s_4cc_DrpUfpStatusPending[NO_OF_TYPEC_PORTS] = {0};
#endif
static uint8_t LED_cnt = 0, LED_status = 0;

/* ************************** *
 *     global valuable        *
 * ************************** */
pd_do_t g_4cc_xSysRdo = {0};
uint8_t g_4cc_higherPort = 0;               // Valid if g_xSysRdo is not zero
uint8_t g_4cc_tiPdRetimerType[2] = {0};

#if CONFIG_EC_UCSI_ENABLE
uint8_t g_4cc_pendingCmdType[NO_OF_TYPEC_PORTS] = {CMD_TYPE_NULL};
#endif /* CONFIG_EC_UCSI_ENABLE */

/* Pre-define functions */
/* process interrupt */
uint32_t app_4cc_usbcInterruptProc(uint8_t port, DRV_TPS_INT_EVENT event);
/* update external flash */
bool app_4cc_externalFlashUpdate(uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID);
/* pre operation before flash update */
bool app_4cc_preOpsForFlashUpdate(uint8_t chipID);
/* start the flash update */
bool app_4cc_startFlashUpdate(uint8_t inactive_region, uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID);
/* update and verify flash region */
uint8_t app_4cc_updateAndVerifyRegion(uint8_t region_number, uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID);
/* reset pd controller */
uint8_t app_4cc_resetPDController(uint8_t chipID);
/* patch version check */
bool app_4cc_patchVersionCheck(uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID);
/* custom version check */
bool app_4cc_customVersionCheck(uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID);
/* configure gpio status */
uint8_t app_4cc_gpioConfig(uint8_t gpioNum, bool ODorPP, uint8_t port);
/* reset PD data buffer */
void app_4cc_resetDpmState(uint8_t port);

/************************************
 *     TI PD TPS6599x functions     *
 ************************************/

/**
 * app_4cc_vonderIdentify
 *
 * @return true: detect TI PD controller
 *
 * @note
 *  Detect TI PD controller TPS6599x serial based on I2C address
 */
bool app_4cc_vonderIdentify(void)
{
	uint8_t buf[16];
	int ret;
	bool ret0 = false, ret1 = false, ret2 = false;
	uint32_t attempts = 0;

	/* Need to identify the 6599x or 6699x PD module */
	/* 6599x I2C address:  0x20 0x24 0x21 */
	/* 6699x I2C address:  0x20 0x24 0x22 */

	do {
		memset (buf, 0x3F, sizeof(buf));
		ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_MODE, buf, (TIPD_REG_MODE_LEN + 1), TYPEC_PORT_0_IDX);

		if (ret == 0){
			/* TI chip0 detected */
			ret0 = true;
			break;
		}
		k_sleep(K_MSEC(100)); /* sleep "100" ms */
		attempts++;
	} while (attempts < 8);  /* 800ms delay */

	attempts = 0;

	do {
		memset (buf, 0x3F, sizeof(buf));
		ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_MODE, buf, (TIPD_REG_MODE_LEN + 1), TYPEC_PORT_1_IDX);

		if (ret == 0) {
			/* TI chip1 detected */
			ret1 = true;
			break;
		}
		k_sleep(K_MSEC(100)); /* sleep "100" ms */
		attempts++;
	} while (attempts < 8);  /* 800ms delay */

	attempts = 0;
#if PD_TRIPPORT_ENABLE
	do {
		memset (buf, 0x3F, sizeof(buf));
		ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_MODE, buf, (TIPD_REG_MODE_LEN + 1), TYPEC_PORT_2_IDX);

		if (ret == 0) {
			/* TI chip1 detected */
			ret2 = true;
			break;
		}
		k_sleep(K_MSEC(100)); /* sleep "100" ms */
		attempts++;
	} while (attempts < 8);  /* 800ms delay */
#else
	ret2 = true;
#endif
	return (ret0 & ret1 & ret2);
}

/**
 * app_4cc_intTopHalf
 * 
 * @param [in]    pinSt;     the interrupt input pin status (1 - high, 0 - low)
 * @return void
 *
 * @note
 *  TIPD interrupt handling routine. This function is executed 
 *  in the MSP context.
 *
 *  GPIO I2C2_IRQn low indicates interrupt active from TIPD.
 *  The handler is splited to top and bottom half to minimize
 *  EC's turn around time.
 *
 *  The function only records the interrupt had occured but don't
 *  handle it.
 */
void app_4cc_intTopHalf(uint8_t pinSt)
{
	/* PD INT# low active */
	/* If there are two TI PD controllers, either interrupt pins may trigger interrput callback function.
     In the callback function, we will loop all the three or four ports. 
	   So in 4cc layer, we think all the ports in one virtual chip */
	if (!pinSt) {
		_s_4cc_eventPending = 1;
		app_usbc_giveSem();
	}
}

/**
 * app_4cc_intPending
 *
 * @note
 *  check if there is TIPD interrupt pending to process
 */
bool app_4cc_intPending(void)
{
	if (_s_4cc_eventPending)
		return true;
	else
		return false;
}

/**
 * app_4cc_intBottomHalf
 * 
 * @param void
 * @return void
 *
 * @note
 *  This is the bottom half for TIPD interrupt handling.
 */
void app_4cc_intBottomHalf(void)
{
	int ret;
	uint8_t loopCount = 0;

	DRV_TPS_INT_EVENT port0IntEvent;
#if PD_DUALPORT_ENABLE
	DRV_TPS_INT_EVENT port1IntEvent;
#endif
#if PD_TRIPPORT_ENABLE
	DRV_TPS_INT_EVENT port2IntEvent;
#endif

	if ((!_s_4cc_eventPending))
		return;
	
	LOG_DBG("TIPD_INTERRUPT!!!");

	/**
	 * clear the INT pending indicator
	 */
	_s_4cc_eventPending = 0;
	/** The INT pending indicator may be set up
	 *  again by the following proc running
	 */
	
	while (1) {
		memset(&port0IntEvent, 0, sizeof(port0IntEvent));
		port0IntEvent.u8Length = TIPD_REG_INT_EVENT2_LEN;
#if PD_DUALPORT_ENABLE
		memset(&port1IntEvent, 0, sizeof(port1IntEvent));
		port1IntEvent.u8Length = TIPD_REG_INT_EVENT2_LEN;
#endif
#if PD_TRIPPORT_ENABLE
		memset(&port2IntEvent, 0, sizeof(port2IntEvent));
		port1IntEvent.u8Length = TIPD_REG_INT_EVENT2_LEN;
#endif
		ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INT_EVENT1, &port0IntEvent, sizeof(port0IntEvent), TYPEC_PORT_0_IDX);

		if (ret != 0) {
			LOG_ERR("[!!!ERROR!!!]  fail to read TIPD_REG_INT_EVENT P0");
			memset(&port0IntEvent, 0, sizeof(port0IntEvent));
		}
		else {
			/* process the event p0 */
			app_4cc_usbcInterruptProc(TYPEC_PORT_0_IDX, port0IntEvent);
		}

		/* Update the RDO only when sink attached */
		/* Port#0, 2. Handle the events one by one */
		if(port0IntEvent.f.NewContractAsCons)
			_s_xPort0SPdo.val = amd_crb_drivers_tps_readSnkPdo(TYPEC_PORT_0_IDX);

		/* Port#0, 3. Clear the pending bits */
		amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_CLEAR1, &port0IntEvent, sizeof(port0IntEvent), TYPEC_PORT_0_IDX);
	
#if PD_DUALPORT_ENABLE
		/* Port#1, 1. read the interrupt event */
		ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INT_EVENT1, &port1IntEvent, sizeof(port1IntEvent), TYPEC_PORT_1_IDX);

		if (ret != 0) {
			LOG_ERR("[!!!ERROR!!!]  fail to read TIPD_REG_INT_EVENT P1");
			memset(&port1IntEvent, 0, sizeof(port1IntEvent));
		}
		else {
			/* process the event p1 */
			app_4cc_usbcInterruptProc(TYPEC_PORT_1_IDX, port1IntEvent);
		}

		/* Update the RDO only when sink attached */
		/* Port#1, 2. Handle the events one by one */
		if(port1IntEvent.f.NewContractAsCons)
			_s_xPort1SPdo.val = amd_crb_drivers_tps_readSnkPdo(TYPEC_PORT_1_IDX);

		/* Port#1, 3. Clear the pending bits */
		amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_CLEAR1, &port1IntEvent, sizeof(port1IntEvent), TYPEC_PORT_1_IDX);
#endif
		
#if PD_TRIPPORT_ENABLE
		/* Port#2, 1. read the interrupt event */
		ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INT_EVENT1, &port2IntEvent, sizeof(port2IntEvent), TYPEC_PORT_2_IDX);

		if (ret != 0) {
			LOG_ERR("[!!!ERROR!!!]  fail to read TIPD_REG_INT_EVENT P2");
			memset(&port2IntEvent, 0, sizeof(port2IntEvent));
		}
		else {
			/* process the event p2 */
			app_4cc_usbcInterruptProc(TYPEC_PORT_2_IDX, port2IntEvent);
		}

		/* Update the RDO only when sink attached */
		/* Port#2, 2. Handle the events one by one */
		if(port2IntEvent.f.NewContractAsCons)
			_s_xPort2SPdo.val = amd_crb_drivers_tps_readSnkPdo(TYPEC_PORT_2_IDX);

		/* Port#2, 3. Clear the pending bits */
		amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_CLEAR1, &port2IntEvent, sizeof(port2IntEvent), TYPEC_PORT_2_IDX);
#endif

		/* Exit current loop if PD INT asserted more than three handling loop */
		loopCount ++;
		if (loopCount > TI_REG_ACCESS_TRTRY_TIME) {
			LOG_DBG("Something cannot be handled by current interrupt loop, tried %d times.", loopCount);
			_s_4cc_eventPending = 1;
			app_usbc_giveSem();
			break;
		}
		
		/* Exit if PD INT de-asserted */
#ifdef PD1_EC_USBC_INT_N
		if (gpio_read_pin(PD0_EC_USBC_INT_N) && gpio_read_pin(PD1_EC_USBC_INT_N)) {
			break;
		}
#else
		if (gpio_read_pin(PD0_EC_USBC_INT_N)) {
			break;
		}
#endif
	}
}

#define TIPD_UCSI_RESP_LEN         (24)
/**
 * app_4cc_usbcInterruptProc
 *
 * @param [in]     port;     port number
 * @param [in]    event;     interrupt event type
 * @return 0 If successful.
 * @return TIPD_4CC_nCMD General error.
 *
 * @note
 *  TIPD interrupt handler process
 */
uint32_t app_4cc_usbcInterruptProc(uint8_t port, DRV_TPS_INT_EVENT event)
{
	int ret = 0;
	app_evt_t evt = APP_EVT_NULL;
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);
	DRV_TPS_STATUS status;
#if CONFIG_EC_UCSI_ENABLE
	uint8_t tmpBuf[TIPD_UCSI_RESP_LEN + 1];
	uint32_t u32rsp = 0;
	alt_mode_app_evt_t alt_mode_event = AM_NO_EVT;
#endif /* CONFIG_EC_UCSI_ENABLE */

	/* Check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return TIPD_4CC_nCMD;
	}

	/* Process the cmd complete info */
	if (event.f.Cmd1Complete) {
		LOG_DBG("CMD1_CMPLET %d", port);
#if CONFIG_EC_UCSI_ENABLE
		/* wait until CMD changes to 0 or '!CMD' */
		u32rsp = amd_crb_drivers_tps_wait4CmdDone(port, 0);

		if (u32rsp != TIPD_4CC_nCMD) {
			/* read dataout */
			tmpBuf[0] = 0;
			/* read 32 bytes will be enough for UCSI resposne */
			ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_DATA1, tmpBuf, (TIPD_UCSI_RESP_LEN + 1), port);
			if (ret == 0) {
				if (_s_ucsi_retryTimeCnt[port] && (tmpBuf[1] & 0x7F)) { /* response failed + retry within */
					_s_ucsi_retryTimeCnt[port]--;
					k_timer_start(&g_4cc_retry_TimerId[port], K_MSEC(20), K_NO_WAIT);  /* 20ms */
				}
				else {
					if (g_4cc_pendingCmdType[port] == CMD_TYPE_UCSI) {
						LOG_DBG("UCSI End P:%d", port);
						/* Process the UCSI response with success */
						amd_crb_drivers_ec_ucsi_cmdCb(port, &tmpBuf[1]);
						g_4cc_pendingCmdType[port] = CMD_TYPE_NULL;
					}
					else if (g_4cc_pendingCmdType[port] == CMD_TYPE_ALT_MODE) {
						LOG_DBG("ALT MODE End P:%d", port);
						/* Process the UCSI response with success */
						amd_crb_drivers_ec_ucsi_altModeCmdCb(port, &tmpBuf[1]);
						g_4cc_pendingCmdType[port] = CMD_TYPE_NULL;
					}
					else {
						g_4cc_pendingCmdType[port] = CMD_TYPE_NULL;
					}

					_s_ucsi_retryTimeCnt[port] = 0;
				}
			} else {
				LOG_ERR("Resp failed from Data1 Reg");
			}
		}
#endif /* CONFIG_EC_UCSI_ENABLE */
	}
	/* Process the status update */
	if ((event.f.StatusUpdate) ||
		(event.f.DrSwapComplete) ||
	    (event.f.PrSwapComplete)) { /* Read the status update */
		LOG_DBG("EVENT_STATUS_UPDATE %d", port);

		if (_s_prSwapRequest[port]) {
			 _s_prSwapRequest[port] = false;
			 evt = APP_EVT_PR_SWAP_COMPLETE;
		}
		if (_s_drSwapRequest[port]) {
			 _s_drSwapRequest[port] = false;
			 evt = APP_EVT_DR_SWAP_COMPLETE;
		}

		/* Update the Rp */
		amd_crb_drivers_tps_readTypecCurrent(port);

		/* read 8+1 bytes for status */
		ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_STATUS, &status, sizeof(status), port);

		if (ret != 0) {
			LOG_DBG("Read status I2C error");
		}
		else if (status.f.PlugPresent) {
			LOG_DBG("Attached %d", port);
			dpm_stat->attach = true;
			/* if attach then check conn state */
			if (status.f.ConnState >= TIPD_REG_STATUS_CONNSTATUS_AUDIO_CONN) {
				if (dpm_stat->connect == false) {
					evt = APP_EVT_CONNECT;
				}
				dpm_stat->connect = true;
				dpm_stat->polarity = status.f.PlugOrientation;
				dpm_stat->cur_port_role = (port_role_t)status.f.PortRole;
				dpm_stat->cur_port_type = (port_type_t)status.f.DataRole;

				LOG_DBG("Update: P:%d P_Polarity:%d P_Role:%d P_Type:%d", port,
				                                                        dpm_stat->polarity,
				                                                        dpm_stat->cur_port_role,
				                                                        dpm_stat->cur_port_type);
				switch (status.f.ConnState) {
					case TIPD_REG_STATUS_CONNSTATUS_AUDIO_CONN:
						dpm_stat->attached_dev = DEV_AUD_ACC;
						break;
					case TIPD_REG_STATUS_CONNSTATUS_DEBUG_CONN:
						dpm_stat->attached_dev = DEV_DBG_ACC;
						break;
					case TIPD_REG_STATUS_CONNSTATUS_NO_CONN_RA:
						dpm_stat->attached_dev = DEV_UNSUPORTED_ACC;
						break;
					case TIPD_REG_STATUS_CONNSTATUS_CONN_WO_RA:
						/* Clear the ra present */
						dpm_stat->ra_present = 0;
						if (status.f.PortRole == TIPD_REG_STATUS_PORT_ROLE_SNK)
							dpm_stat->attached_dev = DEV_SRC;
						else
							dpm_stat->attached_dev = DEV_SNK;
						break;
					case TIPD_REG_STATUS_CONNSTATUS_CONN_W_RA:
						/* Set the ra present */
						dpm_stat->ra_present = 1;
						if (status.f.PortRole == TIPD_REG_STATUS_PORT_ROLE_SNK)
							dpm_stat->attached_dev = DEV_SRC;
						else
							dpm_stat->attached_dev = DEV_SNK;
						break;
					default:
						break;
				}
			}
			else {
				app_4cc_resetDpmState(port);
			}
		}
		else {
			LOG_DBG("Unattached %d", port);
			dpm_stat->attach = false;
			app_4cc_resetDpmState(port);
		}
	}
	/* Process the host init swap info */
	if (event.f.PrSwapComplete) {
		LOG_DBG("EVENT_PR_CMPLET, %d", port);
		evt = APP_EVT_PR_SWAP_COMPLETE;

		/* Clear the RDO because of the PR swap */
		dpm_stat->snk_rdo.val = 0;
		dpm_stat->src_rdo.val = 0;
	}
	if (event.f.DrSwapComplete) {
		LOG_DBG("EVENT_DR_CMPLET %d", port);
		evt = APP_EVT_DR_SWAP_COMPLETE;
	}
	/* Process the device sent swap info */
	if (event.f.PrSwapRequested) {
		LOG_DBG("EVENT_PR_RECV, %d", port);
		if ((dpm_get_info(port)->swap_response & DPM_PR_SWAP_RESP_MASK) == (APP_RESP_ACCEPT << DPM_PR_SWAP_RESP_POS)) {
			_s_prSwapRequest[port] = true;
		}
	}
	if (event.f.DrSwapRequested) {
		LOG_DBG("EVENT_DR_RECV %d", port);
		if ((dpm_get_info(port)->swap_response & DPM_DR_SWAP_RESP_MASK) == (APP_RESP_ACCEPT << DPM_DR_SWAP_RESP_POS)) {
			_s_drSwapRequest[port] = true;
		}
	}
	/* Process the connect and disconnect info */
	if (event.f.PlugInsertOrRemoval) {
		LOG_DBG("EVENT_PLUGOrREMOVE %d", port);
		if (dpm_stat->attach) {
			evt = APP_EVT_CONNECT;
			LOG_DBG("Connected %d", port);
		}
		else {
			app_4cc_resetDpmState(port);
			evt = APP_EVT_DISCONNECT;
			LOG_DBG("Disconnected %d", port);
		}
	}
	/* Process hard reset info */
	if (event.f.PdHardReset) {
		LOG_DBG("HardReset %d", port);
		evt = APP_EVT_HARD_RESET_COMPLETE;
		dpm_stat->attach = false;
		app_4cc_resetDpmState(port);
	}
	/* Process the contract negotiation complete info */
	if ((event.f.NewContractAsProv) || (event.f.NewContractAsCons)) {
		uint32_t prev_rdo = 0;

		LOG_DBG("NewContract %d", port);
		dpm_stat->pd_connected = true;
		dpm_stat->contract_exist = true;

		if (event.f.NewContractAsCons) {
			prev_rdo = dpm_stat->snk_rdo.val;
			dpm_stat->snk_rdo.val = amd_crb_drivers_tps_readSnkRdo(port);
#if CONFIG_EC_UCSI_ENABLE
			/* If there is no RDO change, should not inform the UCSI driver */
			if (prev_rdo != dpm_stat->snk_rdo.val)
				evt = APP_EVT_PD_CONTRACT_NEGOTIATION_COMPLETE;
			else
				LOG_DBG("IgnoreNewContract2 %d", port);
#endif /* CONFIG_EC_UCSI_ENABLE */
		}
		else {
			prev_rdo = dpm_stat->src_rdo.val;
			dpm_stat->src_rdo.val = amd_crb_drivers_tps_readSrcRdo(port);
#if CONFIG_EC_UCSI_ENABLE
			/* If there is no RDO change, should not inform the UCSI driver */
			if (prev_rdo != dpm_stat->src_rdo.val)
				evt = APP_EVT_PD_CONTRACT_NEGOTIATION_COMPLETE;
			else
				LOG_DBG("IgnoreNewContract3 %d", port);
#endif /* CONFIG_EC_UCSI_ENABLE */
		}
	}
#if CONFIG_EC_UCSI_ENABLE
	/* Only process the alt mode command when in DFP mode */
	if (dpm_stat->cur_port_type == PRT_TYPE_DFP) {
		/* Process ALT MODE info */
		if (event.f.AmEnteredMode) { /* Enter */
			LOG_DBG("AM_Entered %d", port);
			evt = APP_EVT_ALT_MODE;
			alt_mode_event = AM_EVT_ALT_MODE_ENTERED;
			dpm_stat->alt_mode_entered = true;

			/* Check if it's USB4 alt mode */
			dpm_stat->usb4_entered = amd_crb_drivers_tps_isUsb4Enter(port);
			dpm_stat->tbt3_entered = amd_crb_drivers_tps_isTbtEnter(port);
			dpm_stat->dp_entered = amd_crb_drivers_tps_isDpEnter(port);

			alt_mode_event = AM_EVT_SUPP_ALT_MODE_CHNG;
		}
		if (event.f.ExitModeComplete) { /* Exit */
			LOG_DBG("AM_Exited %d", port);
			evt = APP_EVT_ALT_MODE;
			alt_mode_event = AM_EVT_ALT_MODE_EXITED;
			dpm_stat->alt_mode_entered = false;
			set_cur_alt_mode_id(port, MODE_NOT_SUPPORTED);
		}
		if (event.f.DiscoverModesComplete) { /* Disc mode complete need to check if it support?? chem */
			LOG_DBG("AM_DisMode_Cmplet %d", port);
			evt = APP_EVT_ALT_MODE;
			alt_mode_event = AM_EVT_SUPP_ALT_MODE_CHNG;
			amd_crb_drivers_tps_readRxSvidSop(port);
		}
		if (event.f.VdmReceived) {
			LOG_DBG("VDM_Recv %d", port);
			/* Check VDM state */
			amd_crb_drivers_tps_readVdmInfo(port);
		}
	}
	/* process the pd event callback */
	if (evt != APP_EVT_NULL) {
		/* Call the pd event handler function to inform the ec ucsi engine */
		amd_crb_drivers_ec_ucsi_pdEventHandler(port, evt, alt_mode_event);
	}
#endif /* CONFIG_EC_UCSI_ENABLE */
	return 0;
}


/**
 * app_4cc_getHigherPortSnkPdo
 * 
 * @param void
 * @return pd_do_t
 *
 * @note
 *  Return the highest Rdo.
 */
pd_do_t app_4cc_getHigherPortSnkPdo(void)
{
	g_4cc_xSysRdo = _s_xPort0SPdo;
	g_4cc_higherPort = TYPEC_PORT_0_IDX;
#if PD_DUALPORT_ENABLE
	/* Get the higher RDO from two typec ports */
	if ((_s_xPort0SPdo.fixed_snk.op_current * _s_xPort0SPdo.fixed_snk.voltage) <
		 _s_xPort1SPdo.fixed_snk.op_current * _s_xPort1SPdo.fixed_snk.voltage) {
		g_4cc_xSysRdo = _s_xPort1SPdo;
		g_4cc_higherPort = TYPEC_PORT_1_IDX;
	}
#endif
#if PD_TRIPPORT_ENABLE
	if ((g_4cc_xSysRdo.fixed_snk.op_current * g_4cc_xSysRdo.fixed_snk.voltage) <
	    _s_xPort2SPdo.fixed_snk.op_current * _s_xPort2SPdo.fixed_snk.voltage) {
		g_4cc_xSysRdo = _s_xPort2SPdo;
		g_4cc_higherPort = TYPEC_PORT_2_IDX;
	}
#endif
	
	return g_4cc_xSysRdo;
}

/**
 * app_4cc_waitPatchMode
 *
 * @param [in]  delay100Ms;     timeout number per 100ms
 * @param [in]      chipID;     chip index
 * @param [in]        type;     wait TIPD patch mode type
 *
 * @note
 *  wait the TIPD patch mode with timeout
 */
void app_4cc_waitPatchMode(uint16_t delay100Ms, uint8_t chipID, APP_4CC_TIPD_MODE type)
{
	uint8_t buf[16];
	uint32_t attempts = 0;

	do {
		memset (buf, 0x3F, sizeof(buf));
		amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_MODE, buf, (TIPD_REG_MODE_LEN + 1), TYPEC_PORT_0_IDX + (chipID << 1)); /* Just read from port0 and port2*/
		if (type == APP_MODE) {
			/* Wait APP mode */
			if (buf[1] == 'A' && buf[2] == 'P' && buf[3] == 'P')
				break;
		}
		else if (type == PTCH_MODE) {
			/* Wait PTCH mode */
			if (buf[1] == 'P' && buf[2] == 'T' && buf[3] == 'C')
				break;
		}
		else if (type == VALID_MODE) {
			/* Wait valid mode */
			if (buf[1] != 0x3F || buf[2] != 0x3F || buf[3] != 0x3F || buf[4] != 0x3F)
				break;
		}
		else
			break;

		k_sleep(K_MSEC(100)); /* sleep "delayMs" ms */
		attempts++;
	} while (attempts < delay100Ms);
	memcpy ( &g_pdCtrlSt[chipID].mode[0], &buf[1], 4 );
	LOG_DBG("chipID:%d Boot Mode is %c%c%c%c", chipID, buf[1], buf[2], buf[3], buf[4]);
}

/**
 * app_4cc_patchModeF211
 *
 * @param [in]      chipID;     chip index
 *
 * @note
 *  check if patch mode is F211
 */
bool app_4cc_patchModeF211(uint8_t chipID)
{
	uint8_t buf[16];
	memset (buf, 0x3F, sizeof(buf));

	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_MODE, buf, (TIPD_REG_MODE_LEN + 1), TYPEC_PORT_0_IDX + (chipID << 1));
	if (buf[1] == 'F' && buf[2] == '2' && buf[3] == '1' && buf[4] == '1')
		return true;
	else
		return false;
}

/**
 * app_4cc_resetPDController
 *
 * @param [in]     chipID;                  chip index
 * @return 0 if reset pass
 *
 * @note
 *  Reset the PD controller after update the external SPI flash via I2C
 */
uint8_t app_4cc_resetPDController(uint8_t chipID)
{
	uint32_t u32rsp;

	/**
	 * Execute GAID, and wait for reset to complete
	 */
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), NULL, 0, "GAID", NULL, 0);

	/* delay 1000ms */
	k_sleep(K_MSEC(1000));

	/* 800ms wait APP mode */
	app_4cc_waitPatchMode(8, chipID, APP_MODE);

	amd_crb_drivers_tps_bootStatus(TYPEC_PORT_0_IDX + (chipID << 1));
	return 0;
}

/**
 * app_4cc_resetPDControllerSwitch
 *
 * @param [in]         chipID;                  chip index
 * @param [in]     switchBank;                  switch PD FW bank or not
 * @param [in]       copyBank;                  copy PD FW bank or not
 * @return 0 if reset pass
 *
 * @note
 *  Reset the PD controller after update the external SPI flash via I2C
 */
uint8_t app_4cc_resetPDControllerSwitch(uint8_t chipID, bool switchBank, bool copyBank)
{
	uint32_t u32rsp;
	uint8_t inputBuf[2];

	inputBuf[0] = switchBank ? 0xAC : 0;
    inputBuf[1] = copyBank ? 0xAC : 0;

	/**
	 * Execute GAID, and wait for reset to complete
	 */
	u32rsp = amd_crb_drivers_tps_cmdExecutor(1, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 2, "GAID", NULL, 0);

	/* delay 1000ms */
	k_sleep(K_MSEC(1000));

	/* 800ms wait APP mode */
	app_4cc_waitPatchMode(100, chipID, APP_MODE);

	amd_crb_drivers_tps_bootStatus(TYPEC_PORT_0_IDX + (chipID << 1));
	return 0;
}

/**
 * app_4cc_readPatchVersion
 *
 * @param [in]      chipID;     chip index
 *
 * @note
 *  read TIPD patch version
 */
void app_4cc_readPatchVersion(uint8_t chipID)
{
	PD_FW_VERSION pdVersion;

	/* Read the fw version from pd controller */
	memset (&pdVersion, 0, sizeof(pdVersion));
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_VERSION, (uint8_t *)&pdVersion, sizeof(pdVersion), TYPEC_PORT_0_IDX + (chipID << 1)); /* Just read from port0 and port2 */

	g_pdCtrlSt[chipID].EE = amd_crb_drivers_tps_deviceInfo(chipID << 1);
	g_pdCtrlSt[chipID].RR = pdVersion.RR;
	g_pdCtrlSt[chipID].MM = pdVersion.MM;
	g_pdCtrlSt[chipID].VVVV = pdVersion.VVVV;
	LOG_DBG("PD FW is %04X.%02X.%02X.%02X", pdVersion.VVVV, pdVersion.MM, pdVersion.RR, pdVersion.EE);
}

/**
 * app_4cc_chkCurPdVerAndUpdatePdCtrlSt
 *
 * @param [in]      chkPt;      string to output
 * @param [in]      chipID;     chip index
 *
 * @note
 *  callback after PD FW upgarde
 */
static void app_4cc_chkCurPdVerAndUpdatePdCtrlSt(const uint8_t * chkPt, uint8_t chipID)
{
	uint8_t buf[16];
	uint32_t attempts = 0;

#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return;
#endif

	/* 500ms wait valid mode */
	app_4cc_waitPatchMode(5, chipID, VALID_MODE);

	/* load TIPD PATCH version */
	app_4cc_readPatchVersion(chipID);

	do {
		/* read the customer use */
		memset (buf, 0, sizeof(buf));
		amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_CUSTOMER_USER, buf, (TIPD_REG_CUSTOMER_USER_LEN + 1), TYPEC_PORT_0_IDX + (chipID << 1)); /* Just read from port0 and port2 */
		if (buf[1] != 0 || buf[2] != 0 || buf[3] != 0 || buf[4] != 0) {
			break;
		}
		k_sleep(K_MSEC(10)); /* sleep 10 ms */
		attempts++;
	} while (attempts < 50); /* delay 500ms */
	LOG_DBG("%s, PD minor version is %02X %02X %02X %02X", chkPt, buf[4], buf[3], buf[2], buf[1]);
	/* Buf[1]--> 0~3 bits: TIPD SKU */
	/* Buf[1]--> 4~6 bits: re-timer type port0. 0-None; 1-PS8828A; 2-PS8830; 3-KB8002; 4-KB8001 */
	/* Buf[1]--> 7 bits and
	   Buf[2]--> 0~1 bits: re-timer type port1. 0-None; 1-PS8828A; 2-PS8830; 3-KB8002; 4-KB8001 */
	/* Buf[5]--> Customer version for specific SKU */
	g_pdCtrlSt[chipID].Custom[0] = buf[5]; // this byte used for minor version
	g_pdCtrlSt[chipID].Custom[1] = buf[6]; // 'L'
	g_pdCtrlSt[chipID].Custom[2] = buf[7]; // 'E'
	g_pdCtrlSt[chipID].Custom[3] = buf[8]; // 'C'
	g_pdCtrlSt[chipID].sku[0] = buf[1] & 0x0F;

	/* Return the re-timer type */
	g_4cc_tiPdRetimerType[chipID] = (buf[1] >> 4) & 0x07;
}

/**
 * app_4cc_retryCmdP0
 *
 * @param void
 * @return void
 *
 * @note
 *  retry 4CC command for P0 if it is failed
 */
static void app_4cc_retryCmdP0(struct k_timer *timer)
{
	amd_crb_drivers_tps_cmdxRetry(0, TYPEC_PORT_0_IDX);
}

#if PD_DUALPORT_ENABLE
/**
 * app_4cc_retryCmdP1
 *
 * @param void
 * @return void
 *
 * @note
 *  retry 4CC command for P1 if it is failed
 */
static void app_4cc_retryCmdP1(struct k_timer *timer)
{
	amd_crb_drivers_tps_cmdxRetry(0, TYPEC_PORT_1_IDX);
}
#endif

#if PD_TRIPPORT_ENABLE
/**
 * app_4cc_retryCmdP2
 *
 * @param void
 * @return void
 *
 * @note
 *  retry 4CC command for P2 if it is failed
 */
static void app_4cc_retryCmdP2(struct k_timer *timer)
{
	amd_crb_drivers_tps_cmdxRetry(0, TYPEC_PORT_2_IDX);
}
#endif


/************************************************************/
/*                659xx FW update start                     */
/************************************************************/
#if CONFIG_USBC_TIPD_TPS6599X
/**
 * app_4cc_loadPatch
 * 
 * @param [in]   spiOffset;     binary address in spi flash
 * @param [in]    fwLength;     binary length
 * @param [in]      chipID;     chip index
 * @return true If successful.
 * @return false if fail
 *
 * @note
 *  load TIPD patch binary through I2C
 */
bool app_4cc_loadPatch(uint32_t spiOffset, uint32_t fwLength, uint8_t chipID)
{
	/** Config: I2C@400K, SPI@16M
	 * app_4cc_loadPatch() totally takes ~1703ms with EC LOG off
	 *        Only 5ms used for 16KB FW read from SPI
	 *        One row (64B) needs 4ms to write (64B+CMD) and another 4ms to wait PD response
     *
	 * Config: I2C@100K, SPI@16M
	 * app_4cc_loadPatch() totally takes ~3000ms with EC LOG off
	 *        It needs 8ms to write (64B+CMD) and another 4ms to wait PD response for one row
	 */
	bool ret = false;
	uint32_t u32rsp;
	uint8_t outputBuf[10];
	uint8_t inputBuf[TIPD_4CC_PTCd_DATA_LENGTH + 1];
	uint32_t c = 0;
	int acks = 0;
	
	LOG_DBG("TIPD_6599X update Chip ID %d", chipID);

#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return false;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return false;
#endif
	
#if CONFIG_EC_UCSI_ENABLE
	g_4cc_pendingCmdType[TYPEC_PORT_0_IDX + (chipID << 1)] = CMD_TYPE_PROGRAM;
#endif
	/* PBMs to start the image download*/
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	memset(inputBuf, 0, sizeof(inputBuf));
	inputBuf[0] = fwLength;          /* Byte1 of bundle size */
	inputBuf[1] = fwLength >> 8;     /* Byte2 of bundle size */
	inputBuf[2] = fwLength >> 16;    /* Byte3 of bundle size */
	inputBuf[3] = fwLength >> 24;    /* Byte4 of bundle size */
	inputBuf[4] = (0x32 + chipID);   /* Byte5 I2C address (0x40 + chipID) + R/W */
	inputBuf[5] = 0x32;              /* Byte6 timeout reserved */
	
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 6, "PBMs", outputBuf, 4);
	
	if (0 != u32rsp) {
		LOG_ERR("!!error!! PBMs returns 0x%08X", u32rsp);
		goto __APP_4CC_EXIT;
	}
	if (0 != outputBuf[0] || 0 != outputBuf[1] || 0 != outputBuf[2]) {
		LOG_ERR("!!error!! PBMs indicates unexpected status [1] PatchStartStatus %02X, [2] DevicePatchStartStatus %02X, [3] AppConfigStartStatus %02X", outputBuf[0], outputBuf[1], outputBuf[2]);
		goto __APP_4CC_EXIT;
	}

	/* FW body */
	while (0 == u32rsp && 0 != fwLength) {
		uint8_t rowSize = TIPD_4CC_PTCd_DATA_LENGTH < fwLength ? TIPD_4CC_PTCd_DATA_LENGTH : fwLength;
		LOG_DBG("Patch PD FW at 0x%06X, rowSize %d, %d bytes remaining", spiOffset, rowSize, fwLength - rowSize);

		/* read the image via spi */
		if (amd_crb_drivers_sFlashRead(0, spiOffset, rowSize, inputBuf)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X", spiOffset);
			goto __APP_4CC_EXIT;
		}

		/* addr:0x08 for image data download */
		acks = amd_crb_drivers_tps_burst_patch_download((0x32 + chipID), inputBuf, rowSize);
		if (acks) {
			LOG_ERR("!!error!! brust returns 0x%08X", acks);
			goto __APP_4CC_EXIT;
		}

		fwLength -= rowSize;
		spiOffset += rowSize;

		gpio_write_pin(EC_BLINK_N, c++ % 7 < 4);
	}
	
	k_sleep(K_MSEC(1));  /* 1ms */

	/** PBMc to end the image download*/
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), NULL, 0, "PBMc", outputBuf, 4);
	/********************************
	 * PBMc takes 66ms ~ 100ms to finish.
	 *
	 * This command has chance to cause 
	 * power lost in dead battery case.
	 * ******************************/
	if (u32rsp != 0) {
		LOG_ERR("!!error!! PBMc returns 0x%08X", u32rsp);
		goto __APP_4CC_EXIT;
	}
	if (0 != outputBuf[2] || 0 != outputBuf[3]) {
		LOG_ERR("!!error!! PBMc indicates unexpected status [2] DevicePatchCompleteStatus %02X, [3] AppConfigPatchCompleteStatus %02X", outputBuf[2], outputBuf[3]);
		goto __APP_4CC_EXIT;
	}
	
	/* patch load end */
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), NULL, 0, "PBMe", outputBuf, 4);

	ret = true;

__APP_4CC_EXIT:
	LOG_DBG("TIPD FW Loading end!!!");
#if CONFIG_EC_UCSI_ENABLE
	g_4cc_pendingCmdType[TYPEC_PORT_0_IDX + (chipID << 1)] = CMD_TYPE_NULL;
#endif
	return ret;
}

/**
 * app_4cc_recoverExternalFlash
 *
 * @param [in]            fwOffset;     low region binary spi address
 * @param [in]              fwSize;     low region binary length
 * @param [in]    fullregion_array;     full region binary spi address
 * @param [in]   fullregion_length;     full region binary length
 * @param [in]              chipID;     chip index
 * @return 1 If successful.
 * @return 0 if fail
 *
 * @note
 *  load the low region TIPD binary to boot up PD. And make it ready to access external flash.
 *  Then upgrade full region binary to external flash to recover PD.
 */
uint8_t app_4cc_recoverExternalFlash(uint32_t fwOffset, uint32_t fwSize, uint32_t fullregion_array, uint32_t fullregion_length, uint8_t chipID)
{
	uint32_t attempts = 0;
	DRV_TPS_INT_EVENT portIntEvent1;
	uint8_t buf[16];
	
	uint32_t u32rsp, spiOffset;
	uint8_t outputBuf[32];
	uint8_t inputBuf[TIPD_4CC_PTCd_DATA_LENGTH + 1];
	uint32_t patchBundleSize = 0;
	uint8_t rowSize;
	patchBundleSize = fullregion_length;
	spiOffset = fullregion_array;
	uint8_t LED_status = 0;
	uint8_t LED_cnt = 0;
	
#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 1;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 1;
#endif
	
	gpio_write_pin(EC_BLINK_N, 0);
	
#if CONFIG_SOC_SERIES_NPCX4
	amd_crb_drivers_spi_reset();
#endif

	/* 800ms wait app mode */
	app_4cc_waitPatchMode(8, chipID, APP_MODE);

	memset (buf, 0x3F, sizeof(buf));
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_MODE, buf, (TIPD_REG_MODE_LEN + 1), TYPEC_PORT_0_IDX + (chipID << 1)); /* Just read from port0 and port2*/

	if (buf[1] == 'A' && buf[2] == 'P' && buf[3] == 'P') {
		return 0;
	}
	
#ifdef INFORM_USER_FOR_PD_UPDATE
	INFORM_USER_FOR_PD_UPDATE();
#endif
	
	/* not in APP mode check ReadyForPatch */
	attempts = 0;
	do {
		amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INT_EVENT1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
		if (portIntEvent1.f.ReadyForPatch) {
			LOG_DBG("chipID:%d, Ready for patch load", chipID);
			amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_CLEAR1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
			break;
		}
		k_sleep(K_MSEC(10)); /* sleep 10ms */
		attempts++;
	} while (attempts < 50);  /* 500ms */
	
	/* Load the PD FW from EC PBMx */
	app_4cc_loadPatch(fwOffset, fwSize, chipID);
				
	/* Read EVENT1 to check PatchLoaded */
	attempts = 0;
	do {
		amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INT_EVENT1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
		if (portIntEvent1.f.PatchLoaded) {
			LOG_DBG("chipID:%d, Patch loaded", chipID);
			/* Clear the patch loaded flag */
			amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_CLEAR1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
			break;
		}
		k_sleep(K_MSEC(10)); /* sleep 10ms */
		attempts++;
	} while (attempts < 50);  /* 500ms */

	/*
	* Set the start address for the next write
	*/
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	memset(inputBuf, 0, sizeof(inputBuf));
	
	inputBuf[0] = 0;
	inputBuf[1] = 0 >> 8;
	inputBuf[2] = 0 >> 16;
	inputBuf[3] = 0 >> 24;
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 4, "FLad", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! FLad returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}

	/* FW body */
	while (0 == u32rsp && 0 != patchBundleSize) {
		/* Adjust the row size from 64 to 32. As 64 bytes may cause I2C busy error code */
		rowSize = 32 < patchBundleSize ? 32 : patchBundleSize;
		LOG_DBG("Patch PD FW at 0x%06X, rowSize %d, %d bytes remaining, chipID:%d", spiOffset, rowSize, patchBundleSize - rowSize, chipID);

		/* read the image via spi */
		if (amd_crb_drivers_sFlashRead(0, spiOffset, rowSize, inputBuf)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", spiOffset, chipID);
			return 0;
		}

		memset(outputBuf, 0xCC, sizeof(outputBuf));
		/* FLwd for image data download */
		u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, rowSize, "FLwd", outputBuf, 4);
		if (0 != u32rsp) {
			LOG_ERR("!!error!! FLwd returns 0x%08X, chipID:%d", u32rsp, chipID);
			return 0;
		}
		if (0 != outputBuf[0]) {
			LOG_ERR("!!error!! FLwd busy 0x%08X, chipID:%d", outputBuf[0], chipID);
			return 0;
		}

		patchBundleSize -= rowSize;
		spiOffset += rowSize;
		
		LED_cnt++;
		if (LED_cnt >= 20) {
			LED_cnt = 0;
			LED_status = (LED_status > 0) ? 0 : 1;
			gpio_write_pin(EC_BLINK_N, LED_status);
		}
	}
	;
	gpio_write_pin(EC_BLINK_N, 1);
	
	return 1;
}

/**
 * app_4cc_init   TPS6599x
 *
 * @param [in]            fwOffset;     low region binary spi address
 * @param [in]              fwSize;     low region binary length
 * @param [in]        pd_load_mode;     pd load mode: EC I2C/ External flash/ ignore
 * @param [in]              chipID;     chip index
 * @return 1 If successful.
 * @return 0 if fail
 *
 * @note
 *  Initiate ti pd controller when system boot-up. Need to download the image to this device
 */
uint8_t app_4cc_init(uint32_t fwOffset, uint32_t fwSize, uint8_t pd_load_mode, uint8_t chipID)
{
	uint8_t buf[16];
	uint32_t g_4cc_eventMask[3] = {0,0,0};
	DRV_TPS_INT_EVENT portIntEvent1;
	uint32_t attempts = 0;
	
	LOG_DBG("TIPD_6599x initiate chipID:%d", chipID);

#if PD_TRIPPORT_ENABLE	
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 1;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 1;
#endif
	
	/********************************************************/
	/**************** Start Firmware upgrade ****************/
	/********************************************************/

	/* Read PD version before any updates */
	memset (&g_pdCtrlSt[chipID], 0, sizeof(g_pdCtrlSt[chipID]));
	
	/* 500ms wait valid mode */
	app_4cc_waitPatchMode(8, chipID, VALID_MODE);

	/* load TIPD PATCH version */
	app_4cc_readPatchVersion(chipID);

	/* Read EVENT1 to check ReadyForPatch */	
	if (pd_load_mode == APP_4CC_PD_LOAD_MODE_EC) {
		attempts = 0;
		do {
			amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INT_EVENT1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
			if (portIntEvent1.f.ReadyForPatch) {
				LOG_DBG("chipID:%d, Ready for patch load", chipID);
				amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_CLEAR1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
				break;
			}
			k_sleep(K_MSEC(10)); /* sleep 10 ms */
			attempts++;
		} while (attempts < 50);  /* delay 500ms */
	}
	/* Wait the app mode for not load patch from EC case */
	else {
		/* 800ms wait APP mode */
		app_4cc_waitPatchMode(8, chipID, APP_MODE);
		
		/* load TIPD PATCH version */
		app_4cc_readPatchVersion(chipID);
	}
	
	k_sleep(K_MSEC(300)); /* Delay 300ms for customer version ready */

	/* read the customer use */
	memset (buf, 0, sizeof(buf));
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_CUSTOMER_USER, buf, (TIPD_REG_CUSTOMER_USER_LEN + 1), TYPEC_PORT_0_IDX + (chipID << 1)); /* Just read from port0 */
	LOG_DBG("chipID:%d, PD minor version is %02X %02X %02X %02X %02X %02X %02X %02X %02X", chipID, buf[8], buf[7], buf[6], buf[5], buf[4], buf[3], buf[2], buf[1], buf[0]);
	g_pdCtrlSt[chipID].Custom[0] = buf[5]; // this byte used for minor version
	g_pdCtrlSt[chipID].Custom[1] = buf[6]; // 'L'
	g_pdCtrlSt[chipID].Custom[2] = buf[7]; // 'E'
	g_pdCtrlSt[chipID].Custom[3] = buf[8]; // 'C'
	g_pdCtrlSt[chipID].sku[0] = buf[1] & 0x0F;
	
	g_4cc_tiPdRetimerType[chipID] = (buf[1] >> 4) & 0x07;
	
	if ( pd_load_mode == APP_4CC_PD_LOAD_MODE_FLASH_OVERWRITE ) {
		if (app_4cc_patchVersionCheck(fwOffset, fwSize, chipID)) {

			g_pdCtrlSt[chipID].isFwLoadSkipped = false;
			LOG_DBG("chipID:%d, Flash PD from SBIOS ROM @0x%5X, L %d", chipID, fwOffset, fwSize);
			
			gpio_write_pin(EC_BLINK_N, 0);
#ifdef INFORM_USER_FOR_PD_UPDATE
			INFORM_USER_FOR_PD_UPDATE();
#endif
			
#if CONFIG_SOC_SERIES_NPCX4
			amd_crb_drivers_spi_reset();
#endif

			g_pdCtrlSt[chipID].isFwLoadSuccess = app_4cc_externalFlashUpdate(fwOffset, fwSize, chipID);
			
			gpio_write_pin(EC_BLINK_N, 1);

			app_4cc_chkCurPdVerAndUpdatePdCtrlSt("After flash PD SPI & PD reset", chipID);

		} else {

			g_pdCtrlSt[chipID].isFwLoadSkipped = true;
			LOG_DBG("chipID:%d, Skip to flash PD.", chipID);

		}
	} else if (pd_load_mode == APP_4CC_PD_LOAD_MODE_EC) {
		if ((_SKIP_TI_PATCH_DOWNLOAD_) || 
		    (g_pdCtrlSt[chipID].RR == 0 && g_pdCtrlSt[chipID].MM == 0 && g_pdCtrlSt[chipID].VVVV == 0)) {

			LOG_DBG("chipID:%d Skip to load PD patch as FW Version is 0.", chipID);
			g_pdCtrlSt[chipID].isFwLoadSkipped = true;

		} else {
			g_pdCtrlSt[chipID].isFwLoadSkipped = false;
			LOG_DBG("chipID:%d, Load PD patch from SBIOS ROM @0x%5X, L %d", fwOffset, fwSize, chipID);
			g_pdCtrlSt[chipID].isFwLoadSuccess = app_4cc_loadPatch(fwOffset, fwSize, chipID);

			/* Read EVENT1 to check PatchLoaded */
			attempts = 0;
			do {
				amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INT_EVENT1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
				if (portIntEvent1.f.PatchLoaded) {
					LOG_DBG("chipID:%d, Patch loaded", chipID);
					/* Clear the patch loaded flag */
					amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_CLEAR1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
					break;
				}
				k_sleep(K_MSEC(10)); /* sleep 10 ms */
				attempts++;
			} while (attempts < 50);  /* delay 500ms */
			
			app_4cc_chkCurPdVerAndUpdatePdCtrlSt("After load patch", chipID);
		}
	} else {
		g_pdCtrlSt[chipID].isFwLoadSkipped = true;
		g_pdCtrlSt[chipID].isFwLoadSuccess = false;
	}
	/*********************************************************/
	/**************** Finish Firmware Upgrade ****************/
	/*********************************************************/
	
#if (_SKIP_TI_EVENT_MASK_CONFIG == 0)	
	/* Setup the interrupt event */
	LOG_DBG("chipID:%d, Reconfig the event mask bits", chipID);
	/* Close all the interrupt first */
	memset (buf, 0x00, sizeof(buf));
	buf[0] = TIPD_REG_INT_MASK1_LEN;

	if (chipID == 0) { /* chip 0 */
		//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK1_LEN + 1),TYPEC_PORT_0_IDX);  /* Clear p0 */
		//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1),TYPEC_PORT_0_IDX);  /* Clear p0 */
#if PD_DUALPORT_ENABLE		
		//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK1_LEN + 1),TYPEC_PORT_1_IDX);  /* Clear p1 */
		//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1),TYPEC_PORT_1_IDX);  /* Clear p1 */
#endif
	}
#if PD_TRIPPORT_ENABLE		
	else if (chipID == 1) { /* chip 1 */
		//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK1_LEN + 1),TYPEC_PORT_2_IDX);  /* Clear p2 */
		//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1),TYPEC_PORT_2_IDX);  /* Clear p2 */
	}
#endif

	/* Enable the interrupt mask for the interesting bits */
	memset (buf, 0x00, sizeof(buf));
	/* Full the event mask for both ports */
	g_4cc_eventMask[0] = 0;
	g_4cc_eventMask[0] = (TIPD_EVENT0_PLUG_INSERT_OR_REMOVAL |
									TIPD_EVENT0_NEW_CONTRACT_AS_CONS |
									TIPD_EVENT0_NEW_CONTRACT_AS_PROV |
									//TIPD_EVENT0_PWR_STATUS_UPDATE |
									//TIPD_EVENT0_DATA_STATUS_UPDATE |
									TIPD_EVENT0_CMD1_CPLT |
									TIPD_EVENT0_PR_SWAP_CPLT |
									TIPD_EVENT0_DR_SWAP_CPLT |
									TIPD_EVENT0_STATUS_UPDATE |
									TIPD_EVENT0_VDM_RECV |
									TIPD_EVENT0_PR_SWAP_REQ |
									TIPD_EVENT0_DR_SWAP_REQ |
									TIPD_EVENT0_PD_HARD_RESET);

	buf[0] = TIPD_REG_INT_MASK1_LEN;
	/* byte from 1~4 */
	buf[1] = (0x000000FF) & g_4cc_eventMask[0];
	buf[2] = (0x000000FF) & (g_4cc_eventMask[0] >> 8);
	buf[3] = (0x000000FF) & (g_4cc_eventMask[0] >> 16);
	buf[4] = (0x000000FF) & (g_4cc_eventMask[0] >> 24);

	g_4cc_eventMask[1] = 0;
	g_4cc_eventMask[1] = (TIPD_EVENT1_DISC_MODE_CPLT |   /*bit-19*/
									TIPD_EVENT1_EXIT_MODE_CPLT |   /*bit-20*/
					TIPD_EVENT1_AM_ENTERED_MODE);	 /*bit-17*/
	/* byte from 5~8 */
	buf[5] = (0x000000FF) & g_4cc_eventMask[1];
	buf[6] = (0x000000FF) & (g_4cc_eventMask[1] >> 8);
	buf[7] = (0x000000FF) & (g_4cc_eventMask[1] >> 16);
	buf[8] = (0x000000FF) & (g_4cc_eventMask[1] >> 24);

	g_4cc_eventMask[2] = TIPD_EVENT2_AMD_IOMUX_ERROR;

	/* byte from 9~11 */
	buf[9] = (0x000000FF) & g_4cc_eventMask[2];
	buf[10] = (0x000000FF) & (g_4cc_eventMask[2] >> 8);
	buf[11] = (0x000000FF) & (g_4cc_eventMask[2] >> 16);

	if (chipID == 0) { /* chip 0 */
		/* update the p0 mask */
		amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_0_IDX);
		amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_0_IDX);
#if PD_DUALPORT_ENABLE
		/* update the p1 mask */
		amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_1_IDX);
		amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_1_IDX);
#endif
	}
#if PD_TRIPPORT_ENABLE
	else if (chipID == 1) { /* chip 1 */
		/* update the p2 mask */
		amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_2_IDX);
	}
#endif
#endif

	if (chipID == 0) { /* chip 0 */
		/* reset pd status P0 */
		app_4cc_resetDpmState(TYPEC_PORT_0_IDX);
		/* Resst the swap */
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response = 0;
		/* Reset DR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response &= ~DPM_DR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_DR_SWAP_RESP_POS;
		/* Reset PR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response &= ~DPM_PR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_PR_SWAP_RESP_POS;
		
		amd_crb_drivers_tps_portCtrlPrSwapResp(TYPEC_PORT_0_IDX, APP_RESP_ACCEPT);
		amd_crb_drivers_tps_portCtrlDrSwapResp(TYPEC_PORT_0_IDX, APP_RESP_ACCEPT);
		
		k_timer_init(&g_4cc_retry_TimerId[TYPEC_PORT_0_IDX], app_4cc_retryCmdP0, NULL);
		
		dpm_set_status(TYPEC_PORT_0_IDX)->port_disable = false;
		
		/* Read the type-c current */
		amd_crb_drivers_tps_readTypecCurrent(TYPEC_PORT_0_IDX);
		/* Load the default setting */
		app_4cc_loadInitialStatus(TYPEC_PORT_0_IDX);
		/* set sleep config */
		amd_crb_drivers_tps_sleepConfig(TYPEC_PORT_0_IDX, true, 1);
	
#if PD_DUALPORT_ENABLE
		/* reset pd status P1 */
		app_4cc_resetDpmState(TYPEC_PORT_1_IDX);
		/* Resst the swap */
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response = 0;
		/* Reset DR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response &= ~DPM_DR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_DR_SWAP_RESP_POS;
		/* Reset PR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response &= ~DPM_PR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_PR_SWAP_RESP_POS;
		
		amd_crb_drivers_tps_portCtrlPrSwapResp(TYPEC_PORT_1_IDX, APP_RESP_ACCEPT);
		amd_crb_drivers_tps_portCtrlDrSwapResp(TYPEC_PORT_1_IDX, APP_RESP_ACCEPT);
		
		k_timer_init(&g_4cc_retry_TimerId[TYPEC_PORT_1_IDX], app_4cc_retryCmdP1, NULL);
		
		dpm_set_status(TYPEC_PORT_1_IDX)->port_disable = false;
		
		/* Read the type-c current */
		amd_crb_drivers_tps_readTypecCurrent(TYPEC_PORT_1_IDX);
		/* Load the default setting */
		app_4cc_loadInitialStatus(TYPEC_PORT_1_IDX);
#endif
	}
#if PD_TRIPPORT_ENABLE
	else if (chipID == 1) { /* chip 1 */
		/* reset pd status P2 */
		app_4cc_resetDpmState(TYPEC_PORT_2_IDX);
		/* Resst the swap */
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response = 0;
		/* Reset DR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response &= ~DPM_DR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_DR_SWAP_RESP_POS;
		/* Reset PR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response &= ~DPM_PR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_PR_SWAP_RESP_POS;
		
		amd_crb_drivers_tps_portCtrlPrSwapResp(TYPEC_PORT_2_IDX, APP_RESP_ACCEPT);
		amd_crb_drivers_tps_portCtrlDrSwapResp(TYPEC_PORT_2_IDX, APP_RESP_ACCEPT);
		
		k_timer_init(&g_4cc_retry_TimerId[TYPEC_PORT_2_IDX], app_4cc_retryCmdP2, NULL);
		
		dpm_set_status(TYPEC_PORT_2_IDX)->port_disable = false;
		
		/* Read the type-c current */
		amd_crb_drivers_tps_readTypecCurrent(TYPEC_PORT_2_IDX);
		/* Load the default setting */
		app_4cc_loadInitialStatus(TYPEC_PORT_2_IDX);
		
		amd_crb_drivers_tps_sleepConfig(TYPEC_PORT_2_IDX, true, 1);
	}
#endif

	if (chipID == 0) { /* chip 0 */
		/* If chip0 is dead battery, chip1 will not */
		if(amd_crb_drivers_tps_deadBattery(TYPEC_PORT_0_IDX)){
			app_4cc_deadbatteryClear(TYPEC_PORT_0_IDX);
			/* Disable if SINK PDO is 5V */
			if (dpm_set_status(TYPEC_PORT_0_IDX)->attach && 
				dpm_set_status(TYPEC_PORT_0_IDX)->contract_exist &&
			   (dpm_set_status(TYPEC_PORT_0_IDX)->cur_port_role == PRT_ROLE_SINK)) {
				if (_s_xPort0SPdo.fixed_snk.voltage == 100) { //5V
					/* Disable this port */
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_NULL);
					dpm_set_status(TYPEC_PORT_0_IDX)->port_disable = true;
				}
			}
			else if (dpm_set_status(TYPEC_PORT_1_IDX)->attach && 
				     dpm_set_status(TYPEC_PORT_1_IDX)->contract_exist &&
					(dpm_set_status(TYPEC_PORT_1_IDX)->cur_port_role == PRT_ROLE_SINK)) {
				if (_s_xPort1SPdo.fixed_snk.voltage == 100) { //5V
					/* Disable this port */
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_NULL);
					dpm_set_status(TYPEC_PORT_1_IDX)->port_disable = true;
				}
			}
		}

		/* Workaround for PLAT-81048 */
		if ((dpm_get_info(TYPEC_PORT_0_IDX)->attach == 0) &&
			(dpm_get_info(TYPEC_PORT_0_IDX)->contract_exist == 0)) {
				if ((_s_xPort0SPdo.fixed_src.voltage == 100) || //5V
					(_s_xPort0SPdo.fixed_src.voltage == 0)) {
					/* Disable this port */
					if (dpm_set_status(TYPEC_PORT_0_IDX)->port_disable == false)
						amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_NULL);
					dpm_set_status(TYPEC_PORT_0_IDX)->port_disable = true;
				}
		}
		
		if ((dpm_get_info(TYPEC_PORT_1_IDX)->attach == 0) &&
			(dpm_get_info(TYPEC_PORT_1_IDX)->contract_exist == 0)) {
				if ((_s_xPort1SPdo.fixed_src.voltage == 100) || //5V
					(_s_xPort1SPdo.fixed_src.voltage == 0)) {
					/* Disable this port */
					if (dpm_set_status(TYPEC_PORT_1_IDX)->port_disable == false)
						amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_NULL);
					dpm_set_status(TYPEC_PORT_1_IDX)->port_disable = true;
				}
		}
	}
#if PD_TRIPPORT_ENABLE	
	else if (chipID == 1) {
		/* If chip1 is dead battery, chip0 will not */
		if(amd_crb_drivers_tps_deadBattery(TYPEC_PORT_0_IDX + (chipID << 1))) {
			app_4cc_deadbatteryClear(TYPEC_PORT_0_IDX + (chipID << 1));
			/* Disable port if SINK PDO is 5V */
			if (dpm_get_info(TYPEC_PORT_2_IDX)->attach &&
				dpm_get_info(TYPEC_PORT_2_IDX)->contract_exist &&
			   (dpm_get_info(TYPEC_PORT_2_IDX)->cur_port_role == PRT_ROLE_SINK)) {
				if (_s_xPort2SPdo.fixed_snk.voltage == 100) { //5V
					/* Disable this port */
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_2_IDX, PRT_NULL);
					dpm_set_status(TYPEC_PORT_2_IDX)->port_disable = true;
				}
			}
		}
	}
#endif	
	return 0;
}

/**
 * app_4cc_patchVersionCheck (TPS6599x)
 *
 * @param [in]     lowregion_array;              low region binary address
 * @param [in]     lowregion_length;             low region binary length
 * @param [in]     chipID;                       chip index
 * @return true if version check pass
 *
 * @note
 *  Check patch version if need to update
 */
bool app_4cc_patchVersionCheck(uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID)
{
	bool needUpdate = false;
	uint8_t inputBuf[TIPD_4CC_PTCd_DATA_LENGTH + 1];
#if (TIPD_SILICON_VER == TIPD_6599X)
	uint8_t retry = 20;	
	uint8_t outputBuf[16];
#endif
	
	/* read the image via spi */
	if (amd_crb_drivers_sFlashRead(0, lowregion_array, TIPD_4CC_PTCd_DATA_LENGTH, inputBuf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", lowregion_array, chipID);
		return false;
	}

	if ((inputBuf[0x36] == g_pdCtrlSt[chipID].VVVV) &&
		(inputBuf[0x35] == g_pdCtrlSt[chipID].MM) &&
		(inputBuf[0x34] == g_pdCtrlSt[chipID].RR) ) {

		/* Patch version match*/
		LOG_DBG("Patch version match, chipID:%d", chipID);

		while(retry--) {
			/* Check the customer user value */				
			amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_CUSTOMER_USER, outputBuf, (TIPD_REG_CUSTOMER_USER_LEN + 1), TYPEC_PORT_0_IDX + (chipID << 1)); /* Just read from port0 */
			if (outputBuf[1] != 0 || outputBuf[2] != 0 || outputBuf[3] != 0)
				break;

			k_sleep(K_MSEC(50));  /* Delay 50ms */
		}
		/* read the image via spi from base-address(0x21000 or 0x22000) + 0x9E */
		if (amd_crb_drivers_sFlashRead(0, (lowregion_array + 0x9E), 16, inputBuf)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", lowregion_array, chipID);
			return false;
		}		
		
		if ((outputBuf[1] == inputBuf[0]) && 
		    (outputBuf[2] == inputBuf[1]) &&
		    (outputBuf[3] == inputBuf[2]) &&
		    (outputBuf[4] == inputBuf[3]) &&
		    (outputBuf[5] == inputBuf[4]) &&
		    (outputBuf[6] == inputBuf[5]) &&
		    (outputBuf[7] == inputBuf[6]) &&
		    (outputBuf[8] == inputBuf[7])) {
			LOG_DBG("Customer Ver match, chipID:%d", chipID);

			/**
			 * This call out gives an opportunity to force the PD FW update routine
			 * It only influences the Version matched case.
			*/
#ifdef CHECK_FOR_FORCE_PD_UPDATE
			needUpdate = CHECK_FOR_FORCE_PD_UPDATE();
#endif

		}
		else {
			/* Update the patch */
			LOG_DBG("Checksum not match. Update the new patch, chipID:%d", chipID);
			needUpdate = true;
		}

	} else {

		/* Update the patch */
		LOG_DBG("Patch version not match. Update the new patch, chipID:%d", chipID);
		if (g_pdCtrlSt[chipID].RR)
		{
			needUpdate = true;
		}
		else
		{
			needUpdate = false;
			LOG_DBG("Empty Patch do nothing... chipID:%d", chipID);
		
			DRV_TPS_INT_EVENT portIntEvent1;
			while(0)
			{
				amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INT_EVENT1, &portIntEvent1, sizeof(portIntEvent1), TYPEC_PORT_0_IDX + (chipID << 1));
				if (portIntEvent1.f.ReadyForPatch)
					break;
				k_sleep(K_MSEC(10));  /* delay 10ms */
			}
			
			/* Recover PD FW from EC */
			if (app_4cc_loadPatch(lowregion_array, lowregion_length, chipID))
				needUpdate = true;
		}
	}
	
	return needUpdate;
}

/**
 * app_4cc_externalFlashUpdate
 *
 * @param [in]     lowregion_array;         low region binary address
 * @param [in]     lowregion_length;        low region binary length
 * @param [in]     chipID;                  chip index
 * @return true if upgrade pass
 *
 * @note
 *  upgrade the external flash
 */
bool app_4cc_externalFlashUpdate(uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID)
{
	bool ret = false;

	g_pdCtrlSt[chipID].isFwLoadSuccess = false;
	if (app_4cc_preOpsForFlashUpdate(chipID) ) {
		ret = app_4cc_startFlashUpdate(_s_4cc_appCtx.InactiveRegion, lowregion_array, lowregion_length, chipID);
	}
	app_4cc_resetPDController(chipID);

	return ret;
}

/**
 * app_4cc_preOpsForFlashUpdate
 *
 * @param [in]     chipID;                  chip index
 * @return 0 if pre operation pass
 *
 * @note
 *  get the image info before update the flash
 */
bool app_4cc_preOpsForFlashUpdate(uint8_t chipID)
{
	PD_APP_CONTEXT *pCtx = &_s_4cc_appCtx;
	DRV_TPS_BOOT p_bootflags;
	int ret = 0;
	/**
	* Read BootFlags (0x2D) register:
	* - Note #1: Applications shouldn't proceed w/ flash-update if the device's
	* boot didn't succeed
	* - Note #2: Flash-update shall be attempted on the inactive region first
	*/
	ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_BOOT, &p_bootflags, sizeof(p_bootflags), TYPEC_PORT_0_IDX + (chipID << 1));
	
	/**
	* Note #1
	* Error during patch load - Don't attempt flash-update as the device wouldn't
	* be able to process the 4CC commands
	*/
	if(0 != p_bootflags.f.PatchHeaderErr) {
		LOG_ERR("patch header error:0x%08X, chipID:%d", p_bootflags.f.PatchHeaderErr, chipID);
		goto error;
	}
	/*
	* Note #2
	* Region1 = 0 indicates that device didn't attempt 'Region1',
	* which implicitly means that the content at Region0 is valid/active
	*/
	if((1 == p_bootflags.f.Region0Invalid) || (1 == p_bootflags.f.region0eepromerr)) {
		pCtx->ActiveRegion = 1;
		pCtx->InactiveRegion = 0;
	}
	else {
		pCtx->ActiveRegion = 0;
		pCtx->InactiveRegion = 1;
	}
	
	if (chipID == 0) {
		/**
		* Keep the port disabled during the flash-update
		*/
		amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_NULL);
#if PD_DUALPORT_ENABLE
		amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_NULL);
#endif
	}
#if PD_TRIPPORT_ENABLE
	else if (chipID == 1) {
		amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_2_IDX, PRT_NULL);
	}
#endif	

	return true;

error:
	return false;
}

/**
 * app_4cc_startFlashUpdate
 *
 * @param [in]     inactive_region;         inactive region number
 * @param [1n]     lowregion_array;         low region binary address
 * @param [in]     chipID;                  chip index
 * @return true if pass
 *
 * @note
 *  Start flash update 
 */
bool app_4cc_startFlashUpdate(uint8_t inactive_region, uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID)
{
	uint8_t retVal = 0;
	PD_APP_CONTEXT *pCtx = &_s_4cc_appCtx;
	
	if (inactive_region > 1)
		return false;

	LOG_DBG("Active Region is [%d] - Region being updated is [%d], chipID:%d", pCtx->ActiveRegion, pCtx->InactiveRegion, chipID);
	/**
	* Region-0 is currently active, hence update Region-1
	*/
	retVal = app_4cc_updateAndVerifyRegion(pCtx->InactiveRegion, lowregion_array, lowregion_length, chipID);
	if(retVal == 0) {
		LOG_DBG("Region[%d] update failed.! Next boot will happen from Region[%d], chipID:%d", pCtx->InactiveRegion, pCtx->ActiveRegion, chipID);
		goto error;
	}
	
	/**
	* Region-1 is successfully updated.
	* To maintain a redundant copy for a fail-safe flash-update, copy the same
	* content at Region-0
	*/
	retVal = app_4cc_updateAndVerifyRegion(pCtx->ActiveRegion, lowregion_array, lowregion_length, chipID);
	if(retVal == 0) {
		LOG_DBG("Region[%d] update failed.! Next boot will happen from Region[%d], chipID:%d", pCtx->ActiveRegion, pCtx->InactiveRegion, chipID);
		goto error;
	}

	return true;

error:

	return false;
}

/**
 * app_4cc_writeRegionPointer
 *
 * @param [in]     region_number;        region number to write pointer
 * @param [1n]     value;                new pointer value
 * @param [in]     chipID;               chip index
 * @return 1 if pass
 *
 * @note
 *  writer region pointer in the header before or after upgarde process
 */
uint8_t app_4cc_writeRegionPointer(uint8_t region_number, uint32_t value, uint8_t chipID)
{
	uint32_t u32rsp;
	uint8_t outputBuf[32];
	uint8_t inputBuf[TIPD_4CC_PTCd_DATA_LENGTH + 1];
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	memset(inputBuf, 0, sizeof(inputBuf));
	
	/* set the flash address to the pointer */
	inputBuf[0] = _s_4cc_regionPtrStart[region_number];
	inputBuf[1] = _s_4cc_regionPtrStart[region_number] >> 8;
	inputBuf[2] = _s_4cc_regionPtrStart[region_number] >> 16;
	inputBuf[3] = _s_4cc_regionPtrStart[region_number] >> 24;
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 4, "FLad", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! FLad returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	
	/* Over-wirte the pointer number */
	inputBuf[0] = value;
	inputBuf[1] = value >> 8;
	inputBuf[2] = value >> 16;
	inputBuf[3] = value >> 24;
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 4, "FLwd", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! FLwd returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	
	/* Read the pointer number */
	inputBuf[0] = _s_4cc_regionPtrStart[region_number];
	inputBuf[1] = _s_4cc_regionPtrStart[region_number] >> 8;
	inputBuf[2] = _s_4cc_regionPtrStart[region_number] >> 16;
	inputBuf[3] = _s_4cc_regionPtrStart[region_number] >> 24;
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 4, "FLrd", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! FLrd returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	LOG_DBG("Region:%d pointer is %02X %02X %02X %02X, chipID:%d", region_number, outputBuf[3], outputBuf[2], outputBuf[1], outputBuf[0], chipID);
	return 1;
}


/**
 * app_4cc_updateAndVerifyRegion
 *
 * @param [in]     region_number;        region number to write pointer
 * @param [1n]     lowregion_array;      low region binary address
 * @param [1n]     lowregion_length;     low region binary length
 * @param [in]     chipID;               chip index
 * @return 1 if it successful
 *
 * @note
 *  Update and verify the specific flash region 
 */
uint8_t app_4cc_updateAndVerifyRegion(uint8_t region_number, uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID)
{
	uint32_t u32rsp, spiOffset;
	uint8_t outputBuf[32];
	uint8_t inputBuf[TIPD_4CC_PTCd_DATA_LENGTH + 1];
	uint32_t patchBundleSize = 0;
	uint32_t regAddr = _s_regionAddrPatchbundle[region_number];
	uint8_t rowSize;
	patchBundleSize = lowregion_length;
	spiOffset = lowregion_array;
	
	uint8_t LED_status = 0;
	uint8_t LED_cnt = 0;

	memset(outputBuf, 0xCC, sizeof(outputBuf));
	memset(inputBuf, 0, sizeof(inputBuf));
	
	/**
	* Step-1
	* First erases the region-pointer so that if there is an interruption while writing
	* the new Patch Bundle, it is guaranteed the PD controller won't try to load it.
	*/
	app_4cc_writeRegionPointer(region_number, 0, chipID);
	
	/**
	* Step-2
	* Set the start address
	* write the correspending patch into flash
	*/
	inputBuf[0] = regAddr;
	inputBuf[1] = regAddr >> 8;
	inputBuf[2] = regAddr >> 16;
	inputBuf[3] = regAddr >> 24;
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 4, "FLad", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! FLad returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}

	/* running furhter 4k flash sectors */
	LOG_DBG("Updating [%d] 4k chunks starting @ 0x%x chipID:%d", TOTAL_4kBSECTORS_FOR_PATCH, regAddr, chipID);

	/* FW body */
	while (0 == u32rsp && 0 != patchBundleSize) {
		/* Adjust the row size from 64 to 32. As 64 bytes may cause I2C busy error code */
		rowSize = 32 < patchBundleSize ? 32 : patchBundleSize;
		LOG_DBG("Patch PD FW at 0x%06X, rowSize %d, %d bytes remaining, chipID:%d", spiOffset, rowSize, patchBundleSize - rowSize, chipID);

		/* read the image via spi */
		if (amd_crb_drivers_sFlashRead(0, spiOffset, rowSize, inputBuf)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", spiOffset, chipID);
			return 0;
		}

		memset(outputBuf, 0xCC, sizeof(outputBuf));
		/* FLwd for image data download */
		u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, rowSize, "FLwd", outputBuf, 4);
		if (0 != u32rsp) {
			LOG_ERR("!!error!! FLwd returns 0x%08X, chipID:%d", u32rsp, chipID);
			return 0;
		}
		if (0 != outputBuf[0]) {
			LOG_ERR("!!error!! FLwd busy 0x%08X, chipID:%d", outputBuf[0], chipID);
			return 0;
		}

		patchBundleSize -= rowSize;
		spiOffset += rowSize;
		
		LED_cnt++;
		if (LED_cnt >= 20) {
			LED_cnt = 0;
			LED_status = (LED_status > 0) ? 0 : 1;
			gpio_write_pin(EC_BLINK_N, LED_status);
		}
	}
	LOG_DBG("External flash write done region: %d, chipID:%d", region_number, chipID);
	
	/**
	* Write is through. Now verify if the content/copy is valid
	*/
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	memset(inputBuf, 0, sizeof(inputBuf));
	inputBuf[0] = regAddr;
	inputBuf[1] = regAddr >> 8;
	inputBuf[2] = regAddr >> 16;
	inputBuf[3] = regAddr >> 24;
	
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 4, "FLvy", outputBuf, 10);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! FLad returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	
	if(0 != outputBuf[0]) {
		LOG_DBG("Flash Verify Failed.!, chipID:%d", chipID);
		return 0;
	}
	else {
		LOG_DBG("Flash Verify Passed region: %d, chipID:%d", region_number, chipID);
	}
	
	/* Configure the right patch pointer for new region */
	app_4cc_writeRegionPointer(region_number, _s_regionAddrPatchbundle[region_number], chipID);

	return 1;
}

#endif /* TIPD_SILICON_VER == TIPD_6599X */
/************************************************************/
/*                659xx FW update end                       */
/************************************************************/

/************************************************************/
/*                6699x FW update start                     */
/************************************************************/
#if CONFIG_USBC_TIPD_TPS6699X
/*
* @note This global variable will hold all the global
*       variables needed for TFU Process
*/
tTFUHandle gTFUHandle;

/**
 * app_4cc_tfuSendTFUs
 *
 * @param [in]     chipID;               chip index
 * @return 1 if it successful
 *
 * @note
 *  This Command will put the PD Controller from APPx Mode to F211 Mode (ready for FW update)
 */
uint8_t app_4cc_tfuSendTFUs(uint8_t chipID)
{
	uint8_t outputBuf[8];
	uint32_t u32rsp;
	memset(outputBuf, 0xCC, sizeof(outputBuf));

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), NULL, 0, "TFUs", outputBuf, 4);

	if (0 != u32rsp) {
		LOG_ERR("!!error!! TFUs returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}

	return 1;
}

/**
 * app_4cc_tfuSendTFUi
 *
 * @param [in]     chipID;               chip index
 * @param [1n]     appRegionArray;       app region address
 * @return 1 if it successful
 *
 * @note   This command initiates the TFU sequence and prepares the PD Controller to receive the
 *         Header Block with contains the Authenication data of the Firmware Image.
 */
uint8_t app_4cc_tfuSendTFUi(uint8_t chipID, uint32_t appRegionArray)
{
	uint8_t outputBuf[8];
	uint32_t u32rsp;
	tTFUiInput inputBuf;
	memset(outputBuf, 0xCC, sizeof(outputBuf));

	/* read the header metadata via spi */
	if (amd_crb_drivers_sFlashRead(0, appRegionArray + HB_ROOT_HEADER_METADATA_OFFSET,
			             HB_ROOT_HEADER_METADATA_LENGTH, inputBuf.buf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d",
				appRegionArray + HB_ROOT_HEADER_METADATA_OFFSET, chipID);
		return 0;
	}

	/* block number:   0x000A */
	/* block size:     0x0800 */
	/* timeout in Sec: 0x0BB8 */
	/* SlaveAddr:      0x0077 */
	gTFUHandle.numDataBlock = inputBuf.f.NumOfBlock;
    gTFUHandle.streamingI2CAddress = inputBuf.f.SlaveAddr;

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf.buf, 8, "TFUi", outputBuf, 4);

	if (0 != u32rsp) {
		LOG_ERR("!!error!! TFUi returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}

	return 1;
}

/**
 * app_4cc_tfuStreamHeaderBlock
 *
 * @param [in]     chipID;               chip index
 * @param [1n]     appRegionArray;       app region address
 * @return 1 if it successful
 *
 * @note   This function sends the 2K of Header Data to PD Controller in one shot.
 *         It is recommended to send TFUq command after this transmission.
 */
uint8_t app_4cc_tfuStreamHeaderBlock(uint8_t chipID, uint32_t appRegionArray)
{
	uint8_t inputBuf[TIPD_4CC_PTCd_DATA_LENGTH + 1];
	uint32_t patchBundleSize = HB_ROOT_HEADER_LENGTH;
	uint32_t spiOffset = appRegionArray + HB_ROOT_HEADER_HASH_OFFSET;
	uint32_t imageSize = 0;
	uint8_t rowSize;
	int ret;

	/* read the header firmware via spi */
	if (amd_crb_drivers_sFlashRead(0, appRegionArray + HB_ROOT_HEADER_FIRMIMAGE_SIZE_OFFSET,
			             16, inputBuf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d",
				appRegionArray + HB_ROOT_HEADER_FIRMIMAGE_SIZE_OFFSET, chipID);
		return 0;
	}

	imageSize = inputBuf[0];
	imageSize += inputBuf[1] << 8;
	imageSize += inputBuf[2] << 16;
	imageSize	+= inputBuf[3] << 24;

	gTFUHandle.FirmwareImageSize = imageSize;

	/* read the header hash via spi */
	if (amd_crb_drivers_sFlashRead(0, appRegionArray + HB_ROOT_HEADER_HASH_OFFSET, TIPD_4CC_PTCd_DATA_LENGTH, inputBuf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d",
				appRegionArray + HB_ROOT_HEADER_HASH_OFFSET, chipID);
		return 0;
	}

  /* Header body */
	while (0 != patchBundleSize) {
		/* Adjust the row size from 64 to 32. As 64 bytes may cause I2C busy error code */
		rowSize = 32 < patchBundleSize ? 32 : patchBundleSize;
		//LOG_DBG("Patch PD FW at 0x%06X, rowSize %d, %d bytes remaining, chipID:%d", spiOffset, rowSize, patchBundleSize - rowSize, chipID);

		/* read the image via spi */
		if (amd_crb_drivers_sFlashRead(0, spiOffset, rowSize, inputBuf)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", spiOffset, chipID);
			return 0;
		}

		/* Burst data download */
		ret = amd_crb_drivers_tps_burst_patch_download(gTFUHandle.streamingI2CAddress, inputBuf, rowSize);
		if (ret) {
			LOG_ERR("!!error!! brust returns 0x%08X", ret);
			return 0;
		}

		patchBundleSize -= rowSize;
		spiOffset += rowSize;
	}

	return 1;
}

/**
 * app_4cc_tfuSendTFUq
 *
 * @param [in]     chipID;               chip index
 * @return 1 if it successful
 *
 * @note   Used this function to send TFUq command when every
 *         status of the Topgun Firmware Upgrade Sequence
 *         is in process. This function provides the overall
 *         health of the TFU process.
 */
uint8_t app_4cc_tfuSendTFUq(uint8_t chipID)
{
	tTFUqOutput outputBuf;
	tTFUqInput inputBuf;
	uint32_t u32rsp;

	memset(outputBuf.buf, 0xCC, sizeof(outputBuf));

	inputBuf.f.regionNum = TFU_IN_PROGRESS_STATUS;
	inputBuf.f.commandType = TFU_QUERY_TFU_STATUS;

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf.buf, 2, "TFUq", outputBuf.buf, 21);

	if (0 != u32rsp) {
		LOG_ERR("!!error!! TFUq returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}

	return 1;
}

/**
 * app_4cc_tfuSendTFUd
 *
 * @param [in]     chipID;               chip index
 * @param [in]     appRegionArray;       app region address
 * @param [in]     appConfigArray;       app config address
 * @param [in]     blockNum;             flash block number
 * @return 1 if it successful
 *
 * @note   Use this function to send the Firmware Image and
 *         App configuration Image to the PD Device.
 *         This function allows to update the App Configuration
 *         only without changing the base firmware image.
 *         Refer TRM document for this process.
 */
uint8_t app_4cc_tfuSendTFUd(uint8_t chipID, uint32_t appRegionArray, uint32_t appConfigArray, uint8_t blockNum)
{
	tTFUdOutput outputBuf;
	tTFUdInput inputBuf;
	uint32_t offset;
	uint32_t u32rsp;

	if (MAX_DATA_BLOCKS == blockNum) {
		if (gTFUHandle.AppConfigOnly) {
			/* read the config via spi */
			if (amd_crb_drivers_sFlashRead(0, appConfigArray + FW_IMAGE_ID_LENGTH,
					                             DB_APP_CONFIG_METADATA_LENGTH, inputBuf.buf)) {
				LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d",
						 appConfigArray + FW_IMAGE_ID_LENGTH, chipID);
				return 0;
			}
		} else {
			/* The Application Configuration is stored at the following offset
			 * FirmwareImageSize 0x2500 (Which excludes Header and App Config) + 0x800 (Header Block Size)
			 * + (8 (Meta Data for Each Block including Header block) * Number of Data block (0x0A) + 1)
			 * + 4 (File Identifier) = 0x2585C
			 */
			offset = gTFUHandle.FirmwareImageSize + HEADER_BLOCK_SIZE \
							+ (DB_FIRMARE_IMAGE_METADATA_LENGTH * (gTFUHandle.numDataBlock + 1)) \
							+ FW_IMAGE_ID_LENGTH;

			/* read the image via spi */
			if (amd_crb_drivers_sFlashRead(0, appRegionArray + offset,
												DB_APP_CONFIG_METADATA_LENGTH, inputBuf.buf)) {
				LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d",
						 appConfigArray + DB_APP_CONFIG_METADATA_LENGTH, chipID);
				return 0;
			}
		}
	} else {

		/* read the image via spi */
		if (amd_crb_drivers_sFlashRead(0, appRegionArray + DB_FIRMARE_IMAGE_METADATA_OFFSET(blockNum),
											DB_FIRMARE_IMAGE_METADATA_LENGTH, inputBuf.buf)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d",
					 appConfigArray + DB_APP_CONFIG_METADATA_LENGTH, chipID);
			return 0;
		}
	}

	gTFUHandle.streamingI2CAddress = inputBuf.f.SlaveAddr;
	gTFUHandle.CurrentBlockSize = inputBuf.f.BlockSize;

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf.buf, 8, "TFUd", outputBuf.buf, 1);

	if (0 != u32rsp) {
		LOG_ERR("!!error!! TFUd returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}

	return 1;
}

/**
 * app_4cc_tfuStreamDataBlock
 *
 * @param [in]     chipID;               chip index
 * @param [in]     appRegionArray;       app region address
 * @param [in]     appConfigArray;       app config address
 * @param [in]     blockNum;             flash block number
 * @return 1 if it successful
 *
 * @note   This function sends the 16K of Header Data to PD Controller in one shot.
 *         It is optional to send TFUq command after this transmission. To check
 *         if the streaming was successful and data was written to Flash successfully
 *         In case of any failure in transmission or flash write the TFUd sequence
 *         for the failed block can be reattempted at this instant only or after
 *         all the blocks are transmitted
 */
uint8_t app_4cc_tfuStreamDataBlock (uint8_t chipID, uint32_t appRegionArray, uint32_t appConfigArray, uint8_t blockNum)
{
	uint8_t inputBuf[TIPD_4CC_PTCd_DATA_LENGTH + 1];
	uint32_t patchBundleSize = 0, patchBundleSizeHalf = 0;
	uint32_t spiOffset = 0;
	uint8_t rowSize;
	bool ledUpdate = false;
	int ret;

	if (MAX_DATA_BLOCKS == blockNum) {
		if (gTFUHandle.AppConfigOnly) {
			spiOffset = appConfigArray + DB_FIRMARE_IMAGE_METADATA_LENGTH + FW_IMAGE_ID_LENGTH;
			patchBundleSize = gTFUHandle.CurrentBlockSize;

		} else {
			/* The Application Configuration is stored at the following offset
			 * FirmwareImageSize (Which excludes Header and App Config) + 0x800 (Header Block Size)
			 * + (8 (Meta Data for Each Block including Header block) * Number of Data block + 1)
			 * + 4 (File Identifier) + 8 (Application Configuration Metadata)
			 */
			spiOffset = appRegionArray + gTFUHandle.FirmwareImageSize + HEADER_BLOCK_SIZE \
							+ (DB_FIRMARE_IMAGE_METADATA_LENGTH * (gTFUHandle.numDataBlock + 1)) \
							+ FW_IMAGE_ID_LENGTH + DB_APP_CONFIG_METADATA_LENGTH;
			patchBundleSize = gTFUHandle.CurrentBlockSize;
		}
	}
	else
	{
		spiOffset = appRegionArray + DB_FIRMARE_IMAGE_OFFSET(blockNum);
		patchBundleSize = gTFUHandle.CurrentBlockSize;
	}

	patchBundleSizeHalf = patchBundleSize >> 1;

	/* FW body */
	while (0 != patchBundleSize) {
		/* Adjust the row size from 64 to 32. As 64 bytes may cause I2C busy error code */
		rowSize = 32 < patchBundleSize ? 32 : patchBundleSize;
		//LOG_DBG("Patch PD FW at 0x%06X, rowSize %d, %d bytes remaining, chipID:%d", spiOffset, rowSize, patchBundleSize - rowSize, chipID);

		/* read the image via spi */
		if (amd_crb_drivers_sFlashRead(0, spiOffset, rowSize, inputBuf)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", spiOffset, chipID);
			return 0;
		}

		/* Burst data download */
		ret = amd_crb_drivers_tps_burst_patch_download(gTFUHandle.streamingI2CAddress, inputBuf, rowSize);
		if (ret) {
			LOG_ERR("!!error!! brust returns 0x%08X", ret);
			return 0;
		}

		patchBundleSize -= rowSize;
		spiOffset += rowSize;

		if (patchBundleSizeHalf > patchBundleSize) {
			if (ledUpdate == false) {
				ledUpdate = true;
				LED_cnt++;
				LED_status = ((LED_cnt%2) > 0) ? 0 : 1;
				EC_DEBUG_LED(!LED_status);
			}
		}
	}

	return 1;
}

/**
 * app_4cc_tfuSendTFUc
 *
 * @param [in]     chipID;               chip index
 * @param [in]     switchBank;           switch bank or not
 * @param [in]     copyBank;             copy bank or not
 * @param [in]     reset;                reset PDC or not
 * @return 1 if it successful
 *
 * @note   This function can automatically resets (With in few ms) the PD Controller to
 *         boot from flash if the command is successful.
 *         Any failure in TFUi and TFUd command will result in this
 *         command being rejected. Hence it is recommended to send
 *         TFUq command before sending this command.
 */
uint8_t app_4cc_tfuSendTFUc(uint8_t chipID, bool switchBank, bool copyBank, bool reset)
{
	uint8_t outputBuf[8];
	uint32_t u32rsp;
	tTFUcInput inputBuf;

	memset(outputBuf, 0xCC, sizeof(outputBuf));

	inputBuf.f.switchBank = (switchBank) ? TFUC_MAGIC_KEY : 0;
	inputBuf.f.copyActiveToBackup = (copyBank) ? TFUC_MAGIC_KEY : 0;
	inputBuf.f.resetPdc = (reset) ? 0 : TFUC_MAGIC_KEY;

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf.buf, 3, "TFUc", outputBuf, 2);

	if (0 != u32rsp) {
		LOG_ERR("!!error!! TFUc returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}

	return 1;
}

/**
 * app_4cc_tfuSendTFUc
 *
 * @param [in]     chipID;               chip index
 * @param [in]     appRegionArray;       app region address
 * @param [in]     appConfigArray;       app config address
 * @return void
 *
 * @brief  Implements the Topgun Firmware Update Programming Sequence through a State Machine
 *         The states, transition Events and response from the PD Controller is tabulated below
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * |                                                       TFU State Transition Table                                                                                        |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | States                     | Transition Event         | 4CC_CMD_SUCCESS           | 4CC_CMD_FAILURE | STREAM_SUCCESS                                   | STREAM_FAILURE |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | TFU_START                  | Send TFUs                | TFU_INIT                  | TFU_START       | x                                                | x              |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | TFU_INIT                   | Send TFUi                | TFU_DOWNLOAD              | TFU_START       | x                                                | x              |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | TFU_STREAM_HEADERBLOCK     | Stream Header Block      | x                         | x               | TFU_DOWNLOAD                                     | TFU_START      |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | TFU_DOWNLOAD               | Send TFUd                | TFU_STREAM_DATABLOCK      | TFU_START       | x                                                | x              |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | TFU_STREAM_DATABLOCK       | Stream Data Block        | x                         | x               | BLOCK_COUNTER < TOTAL NUM BLOCK -> TFU_DOWNLOAD  | TFU_START      |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * |                            |                          |                           |                 | BLOCK_COUNTER = TOTAL NUM BLOCK -> TFU_APPCONIG  |                |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | TFU_APPCONFIG              | Send TFUd for App Config | TFU_STREAM_APPCONFIGBLOCK |                 |                                                  |                |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | TFU_STREAM_APPCONFIGBLOCK  | Stream App Config        | x                         | x               | TFU_COMPLETE                                     | TFU_START      |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * | TFU_COMPLETE               | Send TFUc                | TFU_START                 | TFU_START       | x                                                | x              |
 * |----------------------------|--------------------------|---------------------------|-----------------|--------------------------------------------------|----------------|
 * @retval None
*/
void app_4cc_tfuStateMachine(uint8_t chipID, uint32_t appRegionArray, uint32_t appConfigArray)
{
	LOG_DBG("Tomcat fw-update seq");
	gTFUHandle.RunTFUStateMachine = true;

	while(gTFUHandle.RunTFUStateMachine)
	{
		switch (gTFUHandle.TFUStateMachine)
		{
			case TFU_START:
			{
				gTFUHandle.blockIncrementer = 0;
				if (app_4cc_tfuSendTFUs(chipID)) {
					/* Wait for 500ms for the device to restart to TFU Mode */
					k_sleep(K_MSEC(500)); /* Delay 500ms */
					if (app_4cc_patchModeF211(chipID)) {
						LOG_DBG("tfu_init");
						gTFUHandle.TFUStateMachine = TFU_INIT;
					}
				}
				break;
			}

			case TFU_INIT:
			{
				gTFUHandle.blockIncrementer = 0;
				if (gTFUHandle.AppConfigOnly == false) {
					if (app_4cc_tfuSendTFUi(chipID, appRegionArray)) {
						LOG_DBG("tfu_stream_handerblock");
						gTFUHandle.TFUStateMachine = TFU_STREAM_HEADERBLOCK;
						gTFUHandle.RunTFUStateMachine = true;
					} else {
						LOG_DBG("tfu_start");
						gTFUHandle.TFUStateMachine = TFU_START;
					}
				} else {
					LOG_DBG("tfu_app_config");
					gTFUHandle.TFUStateMachine = TFU_APPCONFIG;
					gTFUHandle.RunTFUStateMachine = true;
				}
				break;
			}
			case TFU_STREAM_HEADERBLOCK:
			{
				if (app_4cc_tfuStreamHeaderBlock(chipID, appRegionArray)) {
					k_sleep(K_MSEC(200)); /* Delay 200ms */
					app_4cc_tfuSendTFUq(chipID);

					LOG_DBG("tfu_download");
					gTFUHandle.TFUStateMachine = TFU_DOWNLOAD;
					gTFUHandle.RunTFUStateMachine = true;
				} else {
					LOG_DBG("Error during TFU Sequence : 0x%02X",gTFUHandle.TFUStateMachine);
					gTFUHandle.TFUStateMachine = TFU_START;
				}
				break;
			}
			case TFU_DOWNLOAD:
			{
				if(gTFUHandle.blockIncrementer < gTFUHandle.numDataBlock) {
					if (app_4cc_tfuSendTFUd(chipID, appRegionArray, appConfigArray, gTFUHandle.blockIncrementer)) {
						LOG_DBG("tfu_stream_datablock");
						gTFUHandle.TFUStateMachine = TFU_STREAM_DATABLOCK;
						gTFUHandle.RunTFUStateMachine = true;
					} else {
						LOG_DBG("Error during TFU Sequence : 0x%02X",gTFUHandle.TFUStateMachine);
						gTFUHandle.TFUStateMachine = TFU_START;
					}
				} else {
					LOG_DBG("tfu_app_config");
					gTFUHandle.TFUStateMachine = TFU_APPCONFIG;
					gTFUHandle.RunTFUStateMachine = true;
				}

				LED_cnt++;
				LED_status = ((LED_cnt%2) > 0) ? 0 : 1;
				EC_DEBUG_LED(!LED_status);

				break;
			}
			case TFU_STREAM_DATABLOCK:
			{
				if (app_4cc_tfuStreamDataBlock(chipID, appRegionArray, appConfigArray, gTFUHandle.blockIncrementer)) {
					k_sleep(K_MSEC(150)); /* Delay 150ms */
					gTFUHandle.RunTFUStateMachine = true;
					gTFUHandle.blockIncrementer++;
					LOG_DBG("tfu_download");
					gTFUHandle.TFUStateMachine = TFU_DOWNLOAD;
				} else {
					LOG_DBG("Error during TFU Sequence : 0x%02X",gTFUHandle.TFUStateMachine);
					gTFUHandle.TFUStateMachine = TFU_START;
				}
				break;
			}

			case TFU_APPCONFIG:
			{
				/* Send Appconfiguration Block which is Block Number as 12 */
				if (app_4cc_tfuSendTFUd(chipID, appRegionArray, appConfigArray, MAX_DATA_BLOCKS)) {
					LOG_DBG("tfu_stream_appconfig_block");
					gTFUHandle.TFUStateMachine = TFU_STREAM_APPCONFIGBLOCK;
					gTFUHandle.RunTFUStateMachine = true;
					} else {
					LOG_DBG("Error during TFU Sequence : 0x%02X",gTFUHandle.TFUStateMachine);
					gTFUHandle.TFUStateMachine = TFU_START;
				}
				break;
			}
			case TFU_STREAM_APPCONFIGBLOCK:
			{
				if (app_4cc_tfuStreamDataBlock(chipID, appRegionArray, appConfigArray, MAX_DATA_BLOCKS)) {
					k_sleep(K_MSEC(100)); /* Delay 100ms */
					app_4cc_tfuSendTFUq(chipID);
					LOG_DBG("tfu_complete");
					gTFUHandle.TFUStateMachine = \
									//gTFUHandle.AppConfigOnly ? TFU_APPCONFIG_COMPLETE : TFU_COMPLETE;
									TFU_COMPLETE;
					gTFUHandle.RunTFUStateMachine = true;
				} else {
					LOG_DBG("Error during TFU Sequence : 0x%02X",gTFUHandle.TFUStateMachine);
					gTFUHandle.TFUStateMachine = TFU_START;
				}
				break;
			}

			case TFU_COMPLETE:
			{
				if (app_4cc_tfuSendTFUc(chipID, false, true, true)) {
					gTFUHandle.TFUStateMachine = TFU_START;

					k_sleep(K_MSEC(600)); /* Delay 600ms */
					app_4cc_waitPatchMode(100, chipID, APP_MODE);
					LOG_DBG("TFU Image Download Successful");
					return;
				} else {
					LOG_DBG("TFUc failed. Try again");
					k_sleep(K_MSEC(600)); /* Delay 600ms */
					app_4cc_waitPatchMode(100, chipID, APP_MODE);
					gTFUHandle.TFUStateMachine = TFU_START;
					return;
				}
				break;
			}

			case TFU_APPCONFIG_COMPLETE:
			{
				gTFUHandle.TFUStateMachine = TFU_START;
				app_4cc_resetPDControllerSwitch(chipID, true, false);
				k_sleep(K_MSEC(1500)); /* Delay 1500ms */
				app_4cc_resetPDControllerSwitch(chipID, false, true);
				k_sleep(K_MSEC(1500)); /* Delay 1500ms */
				app_4cc_waitPatchMode(100, chipID, APP_MODE);
				LOG_DBG("TFU App Config Only Download Successful");
				return;
			}
			default:
				break;
		}
	}
}

/**
 * app_4cc_init  (TPS6699X - Tomcat)
 *
 * @param [in]            fwOffset;     low region binary spi address
 * @param [in]              fwSize;     low region binary length
 * @param [in]        pd_load_mode;     pd load mode: EC I2C/ External flash/ ignore
 * @param [in]              chipID;     chip index
 * @return 1 If successful.
 * @return 0 if fail
 *
 * @note
 *  Initiate ti pd controller when system boot-up. Need to download the image to this device
 */
uint8_t app_4cc_init(uint32_t fwOffset, uint32_t fwSize, uint8_t pd_load_mode, uint8_t chipID)
{
	uint8_t buf[16];
	uint32_t eventMask[3] = {0,0,0};

	LOG_DBG("TIPD_6699x initiate chipID:%d", chipID);

#if PD_TRIPPORT_ENABLE
	/* Three port only process chipID 0 and 1 update */
	if (chipID > 1)
		return 1;
#else
	/* Dual port only process chipID 0 update */
	if (chipID > 0)
		return 1;
#endif

	/********************************************************/
	/**************** Start Firmware upgrade ****************/
	/********************************************************/

	/* Read PD version before any updates */
	memset (&g_pdCtrlSt[chipID], 0, sizeof(g_pdCtrlSt[chipID]));

	/* 500ms wait valid mode */
	app_4cc_waitPatchMode(8, chipID, VALID_MODE);

	/* load TIPD PATCH version */
	app_4cc_readPatchVersion(chipID);

	/* 800ms wait APP mode */
	app_4cc_waitPatchMode(8, chipID, APP_MODE);

	/* load TIPD PATCH version */
	app_4cc_readPatchVersion(chipID);

	/* read the customer use */
	memset (buf, 0, sizeof(buf));
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_CUSTOMER_USER, buf, (TIPD_REG_CUSTOMER_USER_LEN + 1), TYPEC_PORT_0_IDX + (chipID << 1)); /* Just read from port0 */
	LOG_DBG("chipID:%d, PD minor version is %02X %02X %02X %02X", chipID, buf[4], buf[3], buf[2], buf[1]);
	g_pdCtrlSt[chipID].Custom[0] = buf[5]; // this byte used for minor version
	g_pdCtrlSt[chipID].Custom[1] = buf[6]; // 'L'
	g_pdCtrlSt[chipID].Custom[2] = buf[7]; // 'E'
	g_pdCtrlSt[chipID].Custom[3] = buf[8]; // 'C'
	g_pdCtrlSt[chipID].sku[0] = buf[1] & 0x0F;

	g_4cc_tiPdRetimerType[chipID] = (buf[1] >> 4) & 0x07;

	if (pd_load_mode == APP_4CC_PD_LOAD_MODE_IMAGE) {
		/* only check the patch version and both 66994 and 66993 can share same image */
		if (app_4cc_patchVersionCheck(fwOffset, fwSize, chipID)) {
			g_pdCtrlSt[chipID].isFwLoadSkipped = false;
			LOG_DBG("chipID:%d, Flash PD from SBIOS ROM @0x%5X, L %d", chipID, fwOffset, fwSize);

			EC_DEBUG_LED(0);

#if CONFIG_SOC_SERIES_NPCX4
			amd_crb_drivers_spi_reset();
#endif

			/* exit EPR for deadbattery */
			amd_crb_drivers_tps_deadBattery(chipID << 1);
			if (dpm_get_info(chipID << 1)->dead_bat) {
				if (chipID == 0) {
					app_4cc_eprEnable(TYPEC_PORT_0_IDX, false);
					app_4cc_eprEnable(TYPEC_PORT_1_IDX, false);
				} else if (chipID == 1) {
					app_4cc_eprEnable(TYPEC_PORT_2_IDX, false);
				}
				/* need to wait the EPR exit and SPR settle down */
				k_sleep(K_MSEC(1000));/* delay 1000ms */
			}

			/* update the image */
			app_4cc_tfuStateMachine(chipID, fwOffset, 0);

			/* enable EPR for deadbattery */
			if (dpm_get_info(chipID << 1)->dead_bat) {
				if (chipID == 0) {
					app_4cc_eprEnable(TYPEC_PORT_0_IDX, true);
					app_4cc_eprEnable(TYPEC_PORT_1_IDX, true);
				} else if (chipID == 1) {
					app_4cc_eprEnable(TYPEC_PORT_2_IDX, true);
				}
			}

			EC_DEBUG_LED(1);

			app_4cc_chkCurPdVerAndUpdatePdCtrlSt("After flash PD SPI & PD reset", chipID);

		} else {

			g_pdCtrlSt[chipID].isFwLoadSkipped = true;
			LOG_DBG("chipID:%d, Skip to flash PD.", chipID);

		}
	} else if (pd_load_mode == APP_4CC_PD_LOAD_MODE_CONFIG) {
		if (app_4cc_customVersionCheck(fwOffset, fwSize, chipID)) {
			LOG_DBG("chipID:%d, Flash PD from SBIOS ROM @0x%5X, L %d", chipID, fwOffset, fwSize);

			/* exit EPR for deadbattery */
			amd_crb_drivers_tps_deadBattery(chipID << 1);
			if (dpm_get_info(chipID << 1)->dead_bat) {
				if (chipID == 0) {
					app_4cc_eprEnable(TYPEC_PORT_0_IDX, false);
					app_4cc_eprEnable(TYPEC_PORT_1_IDX, false);
				} else if (chipID == 1) {
					app_4cc_eprEnable(TYPEC_PORT_2_IDX, false);
				}
				/* need to wait the EPR exit and SPR settle down */
				k_sleep(K_MSEC(1000));/* delay 1000ms */
			}

			/* update the config */
			gTFUHandle.AppConfigOnly = true;
			app_4cc_tfuStateMachine(chipID, 0, fwOffset);
			gTFUHandle.AppConfigOnly = false;
			app_4cc_chkCurPdVerAndUpdatePdCtrlSt("After flash PD SPI & PD reset", chipID);

			/* enable EPR for deadbattery */
			if (dpm_get_info(chipID << 1)->dead_bat) {
				if (chipID == 0) {
					app_4cc_eprEnable(TYPEC_PORT_0_IDX, true);
					app_4cc_eprEnable(TYPEC_PORT_1_IDX, true);
				} else if (chipID == 1) {
					app_4cc_eprEnable(TYPEC_PORT_2_IDX, true);
				}
			}

			return 0;
		}
	} else if (pd_load_mode == APP_4CC_PD_LOAD_MODE_FLASH) {
		g_pdCtrlSt[chipID].isFwLoadSkipped = true;
		g_pdCtrlSt[chipID].isFwLoadSuccess = false;
	} else {
		/* Abort the TPS6699x Init */
		return 0;
	}
	/*********************************************************/
	/**************** Finish Firmware Upgrade ****************/
	/*********************************************************/

#if (_SKIP_TI_EVENT_MASK_CONFIG == 0)
	/* Not do the int mask reconfig for Nova */
	if (1) {
		/* Setup the interrupt event */
		LOG_DBG("chipID:%d, Reconfig the event mask bits", chipID);
		/* Close all the interrupt first */
		memset (buf, 0x00, sizeof(buf));
		buf[0] = TIPD_REG_INT_MASK1_LEN;

		if (chipID == 0) /* chip 0 */ {
			//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK1_LEN + 1),TYPEC_PORT_0_IDX);  /* Clear p0 */
			//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1),TYPEC_PORT_0_IDX);  /* Clear p0 */
#if PD_DUALPORT_ENABLE
			//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK1_LEN + 1),TYPEC_PORT_1_IDX);  /* Clear p1 */
			//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1),TYPEC_PORT_1_IDX);  /* Clear p1 */
#endif
		}
#if PD_TRIPPORT_ENABLE
		else if (chipID == 1) /* chip 1 */ {
			//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK1_LEN + 1),TYPEC_PORT_2_IDX);  /* Clear p2 */
			//amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1),TYPEC_PORT_2_IDX);  /* Clear p2 */
		}
#endif

		/* Enable the interrupt mask for the interesting bits */
		memset (buf, 0x00, sizeof(buf));
		/* Full the event mask for both ports */
		eventMask[0] = 0;
		eventMask[0] = (TIPD_EVENT0_PLUG_INSERT_OR_REMOVAL |
										TIPD_EVENT0_NEW_CONTRACT_AS_CONS |
										TIPD_EVENT0_NEW_CONTRACT_AS_PROV |
										//TIPD_EVENT0_PWR_STATUS_UPDATE |
										//TIPD_EVENT0_DATA_STATUS_UPDATE |
										TIPD_EVENT0_CMD1_CPLT |
										TIPD_EVENT0_PR_SWAP_CPLT |
										TIPD_EVENT0_DR_SWAP_CPLT |
										TIPD_EVENT0_STATUS_UPDATE |
										TIPD_EVENT0_VDM_RECV |
										TIPD_EVENT0_PR_SWAP_REQ |
										TIPD_EVENT0_DR_SWAP_REQ |
										TIPD_EVENT0_PD_HARD_RESET);

		buf[0] = TIPD_REG_INT_MASK1_LEN;
		/* byte from 1~4 */
		buf[1] = (0x000000FF) & eventMask[0];
		buf[2] = (0x000000FF) & (eventMask[0] >> 8);
		buf[3] = (0x000000FF) & (eventMask[0] >> 16);
		buf[4] = (0x000000FF) & (eventMask[0] >> 24);

		eventMask[1] = 0;
		eventMask[1] = (TIPD_EVENT1_DISC_MODE_CPLT |   /*bit-19*/
						TIPD_EVENT1_EXIT_MODE_CPLT |   /*bit-20*/
						TIPD_EVENT1_AM_ENTERED_MODE);	 /*bit-17*/
		/* byte from 5~8 */
		buf[5] = (0x000000FF) & eventMask[1];
		buf[6] = (0x000000FF) & (eventMask[1] >> 8);
		buf[7] = (0x000000FF) & (eventMask[1] >> 16);
		buf[8] = (0x000000FF) & (eventMask[1] >> 24);

		eventMask[2] = TIPD_EVENT2_AMD_IOMUX_ERROR;

		/* byte from 9~11 */
		buf[9] = (0x000000FF) & eventMask[2];
		buf[10] = (0x000000FF) & (eventMask[2] >> 8);
		buf[11] = (0x000000FF) & (eventMask[2] >> 16);

		if (chipID == 0) /* chip 0 */ {
			/* update the p0 mask */
			amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_0_IDX);
			amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_0_IDX);
#if PD_DUALPORT_ENABLE
			/* update the p1 mask */
			amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_1_IDX);
			amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_1_IDX);
#endif
		}
#if PD_TRIPPORT_ENABLE
		else if (chipID == 1) /* chip 1 */ {
			/* update the p2 mask */
			amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK1, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_2_IDX);
			amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INT_MASK2, buf, (TIPD_REG_INT_MASK2_LEN + 1), TYPEC_PORT_2_IDX);
		}
#endif
	}
#endif

	if (chipID == 0) /* chip 0 */ {
		/* reset pd status P0 */
		app_4cc_resetDpmState(TYPEC_PORT_0_IDX);
		/* Resst the swap */
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response = 0;
		/* Reset DR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response &= ~DPM_DR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_DR_SWAP_RESP_POS;
		/* Reset PR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response &= ~DPM_PR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_0_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_PR_SWAP_RESP_POS;

		amd_crb_drivers_tps_portCtrlPrSwapResp(TYPEC_PORT_0_IDX, APP_RESP_ACCEPT);
		amd_crb_drivers_tps_portCtrlDrSwapResp(TYPEC_PORT_0_IDX, APP_RESP_ACCEPT);

		k_timer_init(&g_4cc_retry_TimerId[TYPEC_PORT_0_IDX], app_4cc_retryCmdP0, NULL);

		dpm_set_status(TYPEC_PORT_0_IDX)->port_disable = false;

		/* Read the type-c current */
		amd_crb_drivers_tps_readTypecCurrent(TYPEC_PORT_0_IDX);
		/* Load the default setting */
		app_4cc_loadInitialStatus(TYPEC_PORT_0_IDX);

		amd_crb_drivers_tps_sleepConfig(TYPEC_PORT_0_IDX, true, 1);

#if PD_DUALPORT_ENABLE
		/* reset pd status P1 */
		app_4cc_resetDpmState(TYPEC_PORT_1_IDX);
		/* Resst the swap */
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response = 0;
		/* Reset DR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response &= ~DPM_DR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_DR_SWAP_RESP_POS;
		/* Reset PR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response &= ~DPM_PR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_1_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_PR_SWAP_RESP_POS;

		amd_crb_drivers_tps_portCtrlPrSwapResp(TYPEC_PORT_1_IDX, APP_RESP_ACCEPT);
		amd_crb_drivers_tps_portCtrlDrSwapResp(TYPEC_PORT_1_IDX, APP_RESP_ACCEPT);

		k_timer_init(&g_4cc_retry_TimerId[TYPEC_PORT_1_IDX], app_4cc_retryCmdP1, NULL);

		dpm_set_status(TYPEC_PORT_1_IDX)->port_disable = false;

		/* Read the type-c current */
		amd_crb_drivers_tps_readTypecCurrent(TYPEC_PORT_1_IDX);
		/* Load the default setting */
		app_4cc_loadInitialStatus(TYPEC_PORT_1_IDX);
#endif
	}
#if PD_TRIPPORT_ENABLE
	else if (chipID == 1) /* chip 1 */ {
		/* reset pd status P2 */
		app_4cc_resetDpmState(TYPEC_PORT_2_IDX);
		/* Resst the swap */
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response = 0;
		/* Reset DR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response &= ~DPM_DR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_DR_SWAP_RESP_POS;
		/* Reset PR SWAP to accept. */
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response &= ~DPM_PR_SWAP_RESP_MASK;
		dpm_set_status(TYPEC_PORT_2_IDX)->swap_response |= APP_RESP_ACCEPT << DPM_PR_SWAP_RESP_POS;

		amd_crb_drivers_tps_portCtrlPrSwapResp(TYPEC_PORT_2_IDX, APP_RESP_ACCEPT);
		amd_crb_drivers_tps_portCtrlDrSwapResp(TYPEC_PORT_2_IDX, APP_RESP_ACCEPT);

		k_timer_init(&g_4cc_retry_TimerId[TYPEC_PORT_2_IDX], app_4cc_retryCmdP2, NULL);

		dpm_set_status(TYPEC_PORT_2_IDX)->port_disable = false;

		/* Read the type-c current */
		amd_crb_drivers_tps_readTypecCurrent(TYPEC_PORT_2_IDX);
		/* Load the default setting */
		app_4cc_loadInitialStatus(TYPEC_PORT_2_IDX);

		amd_crb_drivers_tps_sleepConfig(TYPEC_PORT_2_IDX, true, 1);
	}
#endif

	if (chipID == 0) /* chip 0 */ {
		/* If chip0 is dead battery, chip1 will not */
		if(amd_crb_drivers_tps_deadBattery(TYPEC_PORT_0_IDX)) {
			app_4cc_deadbatteryClear(TYPEC_PORT_0_IDX);
			/* Disable if SINK PDO is 5V */
			if (dpm_get_info(TYPEC_PORT_0_IDX)->attach &&
				dpm_get_info(TYPEC_PORT_0_IDX)->contract_exist &&
			   (dpm_get_info(TYPEC_PORT_0_IDX)->cur_port_role == PRT_ROLE_SINK)) {
				if (_s_xPort0SPdo.fixed_snk.voltage == 100) { //5V
					/* Disable this port */
					amd_crb_drivers_tps6699x_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_NULL);
					dpm_set_status(TYPEC_PORT_0_IDX)->port_disable = true;
				}
			} else if (dpm_get_info(TYPEC_PORT_1_IDX)->attach &&
					   dpm_get_info(TYPEC_PORT_1_IDX)->contract_exist &&
					  (dpm_get_info(TYPEC_PORT_1_IDX)->cur_port_role == PRT_ROLE_SINK)) {
				if (_s_xPort1SPdo.fixed_snk.voltage == 100) { //5V
					/* Disable this port */
					amd_crb_drivers_tps6699x_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_NULL);
					dpm_set_status(TYPEC_PORT_1_IDX)->port_disable = true;
				}
			}
		}

		/* Workaround for PLAT-81048 */
		if ((dpm_get_info(TYPEC_PORT_0_IDX)->attach == 0) &&
			(dpm_get_info(TYPEC_PORT_0_IDX)->contract_exist == 0)) {
			if ((_s_xPort0SPdo.fixed_src.voltage == 100) || //5V
				(_s_xPort0SPdo.fixed_src.voltage == 0)) {
				/* Disable this port */
				if (dpm_set_status(TYPEC_PORT_0_IDX)->port_disable == false)
				amd_crb_drivers_tps6699x_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_NULL);
				dpm_set_status(TYPEC_PORT_0_IDX)->port_disable = true;
			}
		}

		if ((dpm_get_info(TYPEC_PORT_1_IDX)->attach == 0) &&
			(dpm_get_info(TYPEC_PORT_1_IDX)->contract_exist == 0)) {
			if ((_s_xPort1SPdo.fixed_src.voltage == 100) || //5V
				  (_s_xPort1SPdo.fixed_src.voltage == 0)) {
				/* Disable this port */
				if (dpm_set_status(TYPEC_PORT_1_IDX)->port_disable == false)
				amd_crb_drivers_tps6699x_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_NULL);
				dpm_set_status(TYPEC_PORT_1_IDX)->port_disable = true;
			}
		}
	}
#if PD_TRIPPORT_ENABLE
	else if (chipID == 1) {
		/* If chip1 is dead battery, chip0 will not */
		if(amd_crb_drivers_tps_deadBattery(TYPEC_PORT_0_IDX + (chipID << 1))) {
			app_4cc_deadbatteryClear(TYPEC_PORT_0_IDX + (chipID << 1));
			/* Disable port if SINK PDO is 5V */
			if (dpm_get_info(TYPEC_PORT_2_IDX)->attach &&
				dpm_get_info(TYPEC_PORT_2_IDX)->contract_exist &&
			   (dpm_get_info(TYPEC_PORT_2_IDX)->cur_port_role == PRT_ROLE_SINK)) {
				if (_s_xPort2SPdo.fixed_src.voltage == 100) { //5V
					/* Disable this port */
					amd_crb_drivers_tps6699x_portConfigStateMachine(TYPEC_PORT_2_IDX, PRT_NULL);
					dpm_set_status(TYPEC_PORT_2_IDX)->port_disable = true;
				}
			}
		}
	}
#endif
	return 0;
}

/**
 * app_4cc_patchVersionCheck (TPS6699x)
 *
 * @param [in]     lowregion_array;              low region binary address
 * @param [in]     lowregion_length;             low region binary length
 * @param [in]     chipID;                       chip index
 * @return true if version check pass
 *
 * @note
 *  Check patch version if need to update
 */
bool app_4cc_patchVersionCheck(uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID)
{
	bool needUpdate = false;
	uint8_t inputBuf[32];
	uint8_t rom_vvvv = 0, rom_mm = 0, rom_rr = 0, rom_ee = 0;

	gTFUHandle.AppConfigOnly = false;

	/* read the version via spi */
	if (amd_crb_drivers_sFlashRead(0, lowregion_array + 0x4E0, 32, inputBuf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", lowregion_array, chipID);
		return 0;
	}

	rom_vvvv = inputBuf[0x16];
	rom_mm = inputBuf[0x15];
	rom_rr = inputBuf[0x14];

	/* read the version via spi */
	if (amd_crb_drivers_sFlashRead(0, lowregion_array + 0x4800, 32, inputBuf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", lowregion_array, chipID);
		return 0;
	}

	rom_ee = inputBuf[0x11];

	/* Check if patch version match */
	if ((rom_vvvv == g_pdCtrlSt[chipID].VVVV) &&
		(rom_mm == g_pdCtrlSt[chipID].MM) &&
		(rom_rr == g_pdCtrlSt[chipID].RR) &&
		(rom_ee == g_pdCtrlSt[chipID].EE)) {
		/* Patch version match*/
		LOG_DBG("Patch version match, chipID:%d", chipID);
		needUpdate = false;
#ifdef CHECK_FOR_FORCE_PD_UPDATE
		needUpdate = CHECK_FOR_FORCE_PD_UPDATE();
#endif
	} else {
		/* patch version mismatch and update the patch */
		LOG_DBG("Patch version not match. Update the new patch, chipID:%d", chipID);
		needUpdate = true;
	}

	LOG_DBG("ROM %x %x %x %x | FW %x %x %x %x", rom_vvvv, rom_mm, rom_rr, rom_ee,
			g_pdCtrlSt[chipID].VVVV, g_pdCtrlSt[chipID].MM, g_pdCtrlSt[chipID].RR, g_pdCtrlSt[chipID].EE);

	return needUpdate;
}

/**
 * app_4cc_customVersionCheck (TPS6699x)
 *
 * @param [in]     lowregion_array;              low region binary address
 * @param [in]     lowregion_length;             low region binary length
 * @param [in]     chipID;                       chip index
 * @return true if version check pass
 *
 * @note
 *  Check customer version if need to update
 */
bool app_4cc_customVersionCheck(uint32_t lowregion_array, uint32_t lowregion_length, uint8_t chipID)
{
	bool needUpdate = false;
	uint8_t inputBuf[16];
	uint8_t outputBuf[16];

	/* read the version via spi */
	if (amd_crb_drivers_sFlashRead(0, lowregion_array + 0x4E, 16, inputBuf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d", lowregion_array, chipID);
		return 0;
	}

	/* Check the customer user value */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_CUSTOMER_USER, outputBuf, (TIPD_REG_CUSTOMER_USER_LEN + 1), TYPEC_PORT_0_IDX + (chipID << 1)); /* Just read from port0 */

	if ((outputBuf[1] == inputBuf[0]) &&
		(outputBuf[2] == inputBuf[1]) &&
		(outputBuf[3] == inputBuf[2]) &&
		(outputBuf[4] == inputBuf[3]) &&
		(outputBuf[5] == inputBuf[4]) &&
		(outputBuf[6] == inputBuf[5]) &&
		(outputBuf[7] == inputBuf[6]) &&
		(outputBuf[8] == inputBuf[7])){
		LOG_DBG("Customer Ver match, chipID:%d", chipID);

	} else {
	/* Update the patch */
		LOG_DBG("Customer Ver not match update, chipID:%d", chipID);
		needUpdate = true;
	}

	return needUpdate;
}

#endif /* TIPD_SILICON_VER == TIPD_6699X */
/************************************************************/
/*                 6699x FW update end                      */
/************************************************************/

#if CONFIG_USBC_TIPD_TPS6599X
/**
 * app_4cc_gpioInput
 *
 * @param [1n]     gpioNum;       gpio number to operate
 * @param [in]     chipID;        chip index
 * @return 1 if it successful
 *
 * @note
 *  gpioNum 00h~15h for GPIO0 through GPIO21
 */
uint8_t app_4cc_gpioInput(uint8_t gpioNum, uint8_t chipID)
{
	uint8_t outputBuf[5];
	uint8_t inputBuf = gpioNum;
	uint32_t u32rsp = 0;

	memset(outputBuf, 0xCC, sizeof(outputBuf));

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), &inputBuf, 1, "GPie", outputBuf, 4);

	if (0 != u32rsp) {
		LOG_ERR("!!error!! GPio returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	return 1;
}

/**
 * app_4cc_gpioOutput
 *
 * @param [1n]     gpioNum;       gpio number to operate
 * @param [in]     chipID;        chip index
 * @return 1 if it successful
 *
 * @note
 *  gpioNum 00h~15h for GPIO0 through GPIO21
 */
uint8_t app_4cc_gpioOutput(uint8_t gpioNum, uint8_t chipID)
{
	uint8_t outputBuf[5];
	uint8_t inputBuf = gpioNum;
	uint32_t u32rsp = 0;

	memset(outputBuf, 0xCC, sizeof(outputBuf));

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), &inputBuf, 1, "GPoe", outputBuf, 4);

	if (0 != u32rsp) {
		LOG_ERR("!!error!! GPoe returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	return 1;
}

/**
 * app_4cc_gpioConfig
 *
 * @param [1n]     gpioNum;       gpio number to operate
 * @param [in]     chipID;        chip index
 * @return 1 if it successful
 *
 * @note
 *  gpioNum 00h~15h for GPIO0 through GPIO21
 */
uint8_t app_4cc_gpioConfig(uint8_t gpioNum, bool ODorPP, uint8_t port)
{
	DRV_TPS_GPIO_CONFIG gpioStatus;
	int ret = 0;
	uint8_t index = 0;

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return false;
	}
	else if (gpioNum > 12) {
		/* return failed if gpioNum upto 12 */
		return false;
	}

	ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_GPIO_CONFIG, &gpioStatus, sizeof(gpioStatus), port);

	/* ODorPP->1: output, ODorPP->0: high-z */
	if (ret == 0) {
		index = ((gpioNum > 7) ? 1 : 0);
		gpioStatus.buf [index] &= ~(1 << (gpioNum - (index << 3)));
		gpioStatus.buf [index] |= ODorPP << (gpioNum - (index << 3));
	}

	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_GPIO_CONFIG, &gpioStatus, sizeof(gpioStatus), port);

	return true;
}

#endif

/**
 * app_4cc_recoverPorts
 *
 * @param port
 * @return void
 *
 * @note
 *  recovery disabled ports
 */
void app_4cc_recoverPorts(void)
{
	uint8_t port;
#if TI4CC_DYN_SINKONLY
	for (port = 0; port < NO_OF_TYPEC_PORTS; port++) {
		if (dpm_get_info(port)->port_disable) {
			amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
			dpm_set_status(port)->port_role = PRT_DUAL;
			dpm_set_status(port)->port_disable = false;
			LOG_DBG("Dual recover from disable: %d", port);
		} else if ((dpm_get_info(port)->attach) &&
				   (dpm_get_info(port)->cur_port_role == PRT_ROLE_SINK) &&
				   (dpm_get_info(port)->contract_exist)) {
			if (_s_4cc_DrpUfpStatusPending[port] != 0) {
				/* pending change when detch */
				_s_4cc_DrpUfpStatusPending[port] = TI4CC_DRP;
				LOG_DBG("Pending DRP recover: %d", port);
			} else {
				amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
				dpm_set_status(port)->port_role = PRT_DUAL;
				LOG_DBG("DRP recover: %d", port);
			}
		} else {
			amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
			dpm_set_status(port)->port_role = PRT_DUAL;
			LOG_DBG("DRP recover: %d", port);
		}

	}
#else
	for (port = 0; port < NO_OF_TYPEC_PORTS; port++) {
		if (dpm_get_info(port)->port_disable) {
			amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
			dpm_set_status(port)->port_role = PRT_DUAL;
			dpm_set_status(port)->port_disable = false;
			LOG_DBG("Dual recover from disable: %d", port);
		} else if (dpm_get_info(port)->port_role == PRT_ROLE_SINK) {
			amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
			dpm_set_status(port)->port_role = PRT_DUAL;
			LOG_DBG("Dual recover from sink only: %d", port);
		}
	}
#endif
}

/*
 * app_4cc_sinkOnlyAll
 *
 * @return void
 *
 * @note
 *  set all the type-c port to sink only mode
 */
void app_4cc_sinkOnlyAll(void)
{
	uint8_t port;
#if TI4CC_DYN_SINKONLY
	for (port = 0; port < NO_OF_TYPEC_PORTS; port++) {
		if ((dpm_get_info(port)->attach) &&
			(dpm_get_info(port)->cur_port_role == PRT_ROLE_SINK) &&
			(dpm_get_info(port)->contract_exist)) {
			/* do nothing if port acts as sink with contract exist */
			/* pending change when detch */
			_s_4cc_DrpUfpStatusPending[port] = TI4CC_UFP_ONLY;
		} else {
			amd_crb_drivers_tps_portConfigStateMachine(port, PRT_ROLE_SINK);
			dpm_set_status(port)->port_role = PRT_ROLE_SINK;
		}

	}
#else
	for (port = 0; port < NO_OF_TYPEC_PORTS; port++) {
		if (dpm_get_info(port)->dead_bat == false) {
			amd_crb_drivers_tps_portConfigStateMachine(port, PRT_ROLE_SINK);
			dpm_set_status(port)->port_role = PRT_ROLE_SINK;
			LOG_DBG("sink only: %d", port);
		}
	}
#endif
}

/**
 * app_4cc_resetDpmState
 *
 * @param    port:  port number
 * @return   void
 *
 * @note
 *  reset the TI PD related params
 */
void app_4cc_resetDpmState(uint8_t port)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);

	dpm_stat->connect = false;
	dpm_stat->pd_connected = false;
	dpm_stat->contract_exist = false;
	dpm_stat->alt_mode_entered = false;
	dpm_stat->emca_present = false;
	dpm_stat->cbl_vdo.val = 0;
	dpm_stat->cbl_mode_en = false;
	dpm_stat->modal_op_support = false;
	dpm_stat->pd_disabled = false;
	dpm_stat->ra_present = 0;

	set_cur_alt_mode_id(port, MODE_NOT_SUPPORTED);
	dpm_stat->attached_dev = DEV_UNSUPORTED_ACC;
	dpm_stat->snk_rdo.val = 0;
	dpm_stat->src_rdo.val = 0;

	memset(dpm_stat->partner_snk_pdo, 0x00, sizeof(dpm_stat->partner_snk_pdo));
	memset(dpm_stat->partner_src_pdo, 0x00, sizeof(dpm_stat->partner_src_pdo));
	dpm_stat->partner_snk_pdo_count = 0;
	dpm_stat->partner_src_pdo_count = 0;

	dpm_stat->snk_rdo.val = 0;
	dpm_stat->src_rdo.val = 0;

	dpm_stat->usb4_entered = false;
	dpm_stat->tbt3_entered = false;
	dpm_stat->dp_entered = false;

	dpm_stat->usb4_supp_vid = false;
	dpm_stat->tbt3_supp_svid = false;

	/* Clear the sink pdo info, when it need to reset param */
	if (port == TYPEC_PORT_0_IDX) {
		_s_xPort0SPdo.val = 0;
	}
#if PD_DUALPORT_ENABLE
	else if (port == TYPEC_PORT_1_IDX) {
		_s_xPort1SPdo.val = 0;
	}
#endif
#if PD_TRIPPORT_ENABLE
	else if (port == TYPEC_PORT_2_IDX) {
		_s_xPort2SPdo.val = 0;
	}
#endif
#if TI4CC_DYN_SINKONLY
	/* process the pending DRP/UFP event during the detch */
	if (_s_4cc_DrpUfpStatusPending[port] == TI4CC_UFP_ONLY) {
		_s_4cc_DrpUfpStatusPending[port] = 0;
		amd_crb_drivers_tps_portConfigStateMachine(port, PRT_ROLE_SINK);
		dpm_set_status(port)->port_role = PRT_ROLE_SINK;
	} else if (_s_4cc_DrpUfpStatusPending[port] == TI4CC_DRP) {
		_s_4cc_DrpUfpStatusPending[port] = 0;
		amd_crb_drivers_tps_portConfigStateMachine(port, PRT_DUAL);
		dpm_set_status(port)->port_role = PRT_DUAL;
	}
#endif
}

/**
 * app_4cc_getI2cAddr
 *
 * @param [in]     port;     port number
 * @return I2C address
 *
 * @note
 *  return I2C address for each port
 */
uint8_t app_4cc_getI2cAddr(uint8_t port)
{
	if (port == TYPEC_PORT_0_IDX)
		return 0x20;
#if PD_DUALPORT_ENABLE
	else if (port == TYPEC_PORT_1_IDX)
		return 0x24;
#endif
#if PD_TRIPPORT_ENABLE
	else if (port == TYPEC_PORT_2_IDX)
#if CONFIG_USBC_TIPD_TPS6599X
		return 0x21;
#elif CONFIG_USBC_TIPD_TPS6699X
		return 0x22;
#endif
#endif

	return 0x00;
}

/**
 * app_4cc_retimerForcePower
 *
 * @param [1n]     port;        port number
 * @param [in]     status;      true->poweron false->poweroff
 * @return 1 if it successful
 *
 * @note
 *  force power on and off PS883x re-timer for I2C access
 */
uint8_t app_4cc_retimerForcePower(uint8_t port, bool status)
{
	uint8_t outputBuf[5];
	uint8_t inputBuf[2];
	uint32_t u32rsp = 0;
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	
	inputBuf[0] = status;
	inputBuf[1] = 0x2A; /* Retimer_SoC_OVR_Force_PWR_Event */
	
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, inputBuf, 2, "Trig", outputBuf, 4);
	
	if (0 != u32rsp) {
		LOG_ERR("!!error!! Trig returns 0x%08X", u32rsp);
		return 0;
	}
	return 1;
}

/**
 * app_4cc_gpioOutputHigh
 *
 * @param [1n]     gpioNum;       gpio number to operate
 * @param [in]     chipID;        chip index
 * @return 1 if it successful
 *
 * @note
 *  gpioNum 00h~15h for GPIO0 through GPIO21
 */
uint8_t app_4cc_gpioOutputHigh(uint8_t gpioNum, uint8_t chipID)
{
#if CONFIG_USBC_TIPD_TPS6599X
	uint8_t outputBuf[5];
	uint8_t inputBuf = gpioNum;
	uint32_t u32rsp = 0;
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), &inputBuf, 1, "GPsh", outputBuf, 4);
	
	if (0 != u32rsp) {
		LOG_ERR("!!error!! GPio returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	return 1;
#elif CONFIG_USBC_TIPD_TPS6699X
	/* 6699x has two groups of GPIO. GPIO0 has 15 pins and GPIO1 has 12 pins. Totally 27 pin */
	/* gpioNum input from 0~14 located to GPIO0 and 15~26 located to GPIO1  */
	if (gpioNum < 15) {
		/* GPIO0 group */
		if (chipID == 0)
			amd_crb_drivers_tps_gpio0Output(TYPEC_PORT_0_IDX, (1 << gpioNum), (1 << gpioNum));
#if PD_TRIPPORT_ENABLE
		else if (chipID == 1)
			amd_crb_drivers_tps_gpio0Output(TYPEC_PORT_2_IDX, (1 << gpioNum), (1 << gpioNum));
#endif
	} else if (gpioNum < 27) {
		/* GPIO1 group */
		gpioNum -= 15;
		if (chipID == 0)
			amd_crb_drivers_tps_gpio1Output(TYPEC_PORT_0_IDX, (1 << gpioNum), (1 << gpioNum));
#if PD_TRIPPORT_ENABLE
		else if (chipID == 1)
			amd_crb_drivers_tps_gpio1Output(TYPEC_PORT_2_IDX, (1 << gpioNum), (1 << gpioNum));
#endif
	}
	return 1;
#endif
}

/**
 * app_4cc_gpioOutputLow
 *
 * @param [1n]     gpioNum;       gpio number to operate
 * @param [in]     chipID;        chip index
 * @return 1 if it successful
 *
 * @note
 *  gpioNum 00h~15h for GPIO0 through GPIO21
 */
uint8_t app_4cc_gpioOutputLow(uint8_t gpioNum, uint8_t chipID)
{
#if CONFIG_USBC_TIPD_TPS6599X
	uint8_t outputBuf[5];
	uint8_t inputBuf = gpioNum;
	uint32_t u32rsp = 0;
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), &inputBuf, 1, "GPsl", outputBuf, 4);
	
	if (0 != u32rsp) {
		LOG_ERR("!!error!! GPio returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	return 1;
#elif CONFIG_USBC_TIPD_TPS6699X
	/* 6699x has two groups of GPIO. GPIO0 has 15 pins and GPIO1 has 12 pins. Totally 27 pin */
	/* gpioNum input from 0~14 located to GPIO0 and 15~26 located to GPIO1  */
	if (gpioNum < 15) {
		/* GPIO0 group */
		if (chipID == 0)
			amd_crb_drivers_tps_gpio0Output(TYPEC_PORT_0_IDX, (1 << gpioNum), 0);
#if PD_TRIPPORT_ENABLE
		else if (chipID == 1)
			amd_crb_drivers_tps_gpio0Output(TYPEC_PORT_2_IDX, (1 << gpioNum), 0);
#endif
	} else if (gpioNum < 27) {
		/* GPIO1 group */
		gpioNum -= 15;
		if (chipID == 0)
			amd_crb_drivers_tps_gpio1Output(TYPEC_PORT_0_IDX, (1 << gpioNum), 0);
#if PD_TRIPPORT_ENABLE
		else if (chipID == 1)
			amd_crb_drivers_tps_gpio1Output(TYPEC_PORT_2_IDX, (1 << gpioNum), 0);
#endif
	}
	return 1;
#endif
}
#if CONFIG_BATTERY_MANAGEMENT
extern uint8_t g_batPresentFlag;
#endif
/**
 * app_4cc_deadbatteryClear
 *
 * @param [1n]     port;      port number
 * @return 1 if it successful
 *
 * @note
 *  clear the dead battery flag
 */
uint8_t app_4cc_deadbatteryClear(uint8_t port)
{
	uint8_t outputBuf[5];
	uint32_t u32rsp = 0;
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
#ifdef BRD_MDS_DORNE
	/* only clear the dead battery flag with battery attached */
	if (dpm_stat->dead_bat && g_batPresentFlag) {
#else
	if (dpm_stat->dead_bat) {
#endif
		/* If dead battery flag assert clear it by 4cc command "DBfg" */
		//dpm_stat->dead_bat = false;  /* not used for now */
		
		/* Both chip should in the same status. So only need to read one */
		u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, NULL, 0, "DBfg", outputBuf, 4);
		if (0 != u32rsp) {
			LOG_ERR("!!error!! DBfg returns 0x%08X", u32rsp);
			return 0;
		}
		LOG_DBG("DB Cleared");
		return 1;
	}
	return 0;
}

/**
 * app_4cc_waitAttachEvent
 *
 * @param [1n]     port;      port number
 * @return true if it successful
 *
 * @note
 *  wait attach event report
 */
bool app_4cc_waitAttachEvent(uint8_t port)
{
	DRV_TPS_STATUS status;
	int ret = 0;
	
	ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_STATUS, &status, sizeof(status), port);
	
	if (ret != 0) {
		return false;
	}
	else if (status.f.PlugPresent) {
		return true;
	}
	
	return false;
}

/**
 * app_4cc_loadInitialStatus
 *
 * @param [1n]     port;      port number
 *
 * @note
 *  when pd bootup, align the default pd status
 */
void app_4cc_loadInitialStatus(uint8_t port)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);
	DRV_TPS_STATUS status;
	int ret = 0;
	
	/* Set the default port_role and dflt_port_role to dual */
	dpm_stat->dflt_port_role = PRT_DUAL;
	dpm_stat->port_role = PRT_DUAL;

	LOG_DBG("Load initial PD status port %d", port);
	/* read 8+1 bytes for status */
	ret = amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_STATUS, &status, sizeof(status), port);
	
	if (ret != 0) {
		LOG_ERR("Read status I2C error");
	}
	else if (status.f.PlugPresent) {
		LOG_DBG("Attached %d", port);
		dpm_stat->attach = true;
		/* if attach then check conn state */
		if (status.f.ConnState >= TIPD_REG_STATUS_CONNSTATUS_AUDIO_CONN) {
			dpm_stat->connect = true;
			dpm_stat->polarity = status.f.PlugOrientation;
			dpm_stat->cur_port_role = (port_role_t)status.f.PortRole;
			dpm_stat->cur_port_type = (port_type_t)status.f.DataRole;
			
			LOG_DBG("Update: P:%d P_Polarity:%d P_Role:%d P_Type:%d", port,
			dpm_stat->polarity,
			dpm_stat->cur_port_role,
			dpm_stat->cur_port_type);
			switch (status.f.ConnState) {
				case TIPD_REG_STATUS_CONNSTATUS_AUDIO_CONN:
					dpm_stat->attached_dev = DEV_AUD_ACC;
					break;
				case TIPD_REG_STATUS_CONNSTATUS_DEBUG_CONN:
					dpm_stat->attached_dev = DEV_DBG_ACC;
					break;
				case TIPD_REG_STATUS_CONNSTATUS_NO_CONN_RA:
					dpm_stat->attached_dev = DEV_UNSUPORTED_ACC;
					break;
				case TIPD_REG_STATUS_CONNSTATUS_CONN_WO_RA:
					/* Clear the ra present */
					dpm_stat->ra_present = 0;
					if (status.f.PortRole == TIPD_REG_STATUS_PORT_ROLE_SNK)
						dpm_stat->attached_dev = DEV_SRC;
					else
						dpm_stat->attached_dev = DEV_SNK;
					break;
				case TIPD_REG_STATUS_CONNSTATUS_CONN_W_RA:
					/* Set the ra present */
					dpm_stat->ra_present = 1;
					if (status.f.PortRole == TIPD_REG_STATUS_PORT_ROLE_SNK)
						dpm_stat->attached_dev = DEV_SRC;
					else
						dpm_stat->attached_dev = DEV_SNK;
					break;
				default:
					break;
			}
			_s_xPort0SPdo.val = amd_crb_drivers_tps_readSnkPdo(TYPEC_PORT_0_IDX);
#if PD_DUALPORT_ENABLE
			_s_xPort1SPdo.val = amd_crb_drivers_tps_readSnkPdo(TYPEC_PORT_1_IDX);
#endif
#if PD_TRIPPORT_ENABLE
			_s_xPort2SPdo.val = amd_crb_drivers_tps_readSnkPdo(TYPEC_PORT_2_IDX);
#endif
		}
		else {
			app_4cc_resetDpmState(port);
		}
	}
	else {
		LOG_DBG("Unattached %d", port);
		dpm_stat->attach = false;
		app_4cc_resetDpmState(port);
	}
}

/**
 * app_4cc_deviceInfo
 *
 * @note
 *  get TIPD device info
 */
void app_4cc_deviceInfo(void)
{	
#if CONFIG_USBC_TIPD_TPS6599X
	TIPD_DEVICE_TYPE type = amd_crb_drivers_tps_deviceInfo(TYPEC_PORT_0_IDX);
	if (type == TPS6599X_AC) {
		memcpy(g_pdCtrlSt[0].type, "TIAC", 4);
	}
	else if (type == TPS6599X_AD) {
		memcpy(g_pdCtrlSt[0].type, "TIAD", 4);
	}
	else if (type == TPS6599X_BF) {
		memcpy(g_pdCtrlSt[0].type, "TIBF", 4);
	}
	
#if PD_TRIPPORT_ENABLE
	type = amd_crb_drivers_tps_deviceInfo(TYPEC_PORT_2_IDX);
	if (type == TPS6599X_AC) {
		memcpy(g_pdCtrlSt[1].type, "TIAC", 4);
	}
	else if (type == TPS6599X_AD) {
		memcpy(g_pdCtrlSt[1].type, "TIAD", 4);
	}
	else if (type == TPS6599X_BF) {
		memcpy(g_pdCtrlSt[1].type, "TIBF", 4);
	}
#endif
#elif CONFIG_USBC_TIPD_TPS6699X
	memcpy(g_pdCtrlSt[0].type, "TITC", 4);
#if PD_TRIPPORT_ENABLE
	memcpy(g_pdCtrlSt[1].type, "TITC", 4);
#endif /* PD_TRIPPORT_ENABLE */
#endif
}

/**
 * app_4cc_curPdo
 *
 * @param [1n]     port;      port number
 * @return current PDO
 *
 * @note
 *  when pd bootup, align the default pd status
 */
uint32_t app_4cc_curPdo(uint8_t port)
{

	uint32_t ret = 0;
	
	switch (port) {
		case TYPEC_PORT_0_IDX:
			ret = _s_xPort0SPdo.val;
			break;
		case TYPEC_PORT_1_IDX:
				ret = _s_xPort1SPdo.val;
			break;
#if PD_TRIPPORT_ENABLE
		case TYPEC_PORT_2_IDX:
				ret = _s_xPort2SPdo.val;
			break;
#endif
		default:
			break;
	}
	return ret;
}

/**
 * app_4cc_patchPdController
 *
 * @param [1n]     chipID;      chip index
 * @return 0 if pass
 *
 * @note
 *  set PD controller into patch mode
 */
uint8_t app_4cc_patchPdController(uint8_t chipID)
{
	uint32_t u32rsp;

	/*
	 * Execute GAID, and wait for reset to complete
	 */
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX + (chipID << 1), NULL, 0, "GO2P", NULL, 0);

	/* delay 1000ms */
	k_sleep(K_MSEC(1000));

	/* 500ms wait PTCH mode */
	app_4cc_waitPatchMode(5, chipID, PTCH_MODE);

	return 0;
}

/**
 * app_4cc_dataReset
 *
 * @param [1n]     port;      port number
 * @return 0 if pass
 *
 * @note
 *  trigger TI PD controller to send data-reset message
 */
uint8_t app_4cc_dataReset(uint8_t port)
{
	uint32_t u32rsp;

	/*
	 * Execute GAID, and wait for reset to complete
	 */
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, NULL, 0, "DRST", NULL, 0);

	return 0;
}

/**
 * app_4cc_discoveryProcess
 *
 * @param [1n]     port;      port number
 * @return 0 if pass
 *
 * @note
 *  trigger TI PD controller to send discover id message
 */
uint8_t app_4cc_discoveryProcess(uint8_t port)
{
	uint32_t u32rsp;
	uint8_t temp1[4] = {(0x01 << 5), 0x01, 0xFF};
	uint8_t DISCdelay = 2; /* delay 2s to re-connect */
	
	/* Enter mode but support TBT */
	if (dpm_get_info(port)->tbt3_entered) {
		dpm_set_status(port)->tbt3_entered = false;
		
		u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, &DISCdelay, 1, "DISC", NULL, 0);
		LOG_DBG("ALT-MODE Exit 0x8087");
	}
	else if (dpm_get_info(port)->dp_entered) {
		dpm_set_status(port)->dp_entered = false;

		u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, temp1, 3, "AMEx", NULL, 0);
		LOG_DBG("ALT-MODE Exit 0xFF01");
	}
	else if (dpm_get_info(port)->contract_exist){
		/**
		 * Execute GAID, and wait for reset to complete
		 */
		u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, NULL, 0, "AMDs", NULL, 0);
		LOG_DBG("ALT-MODE DISCOVER");
	}

	return 0;
}

/**
 * app_4cc_i2cTunnel
 *
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]  address;     i2c address
 * @param [in]      reg;     register to write
 * @param [in]  dataLen;     data length
 * @param [in]     data;     data buffer pointer
 * @param [in]   chipID;     chip index
 * @return 1 If successful.
 *
 * @note
 *  use TIPD I2C tunnel to control PD I2C master to access re-timer or apu crossbar
 */
uint8_t app_4cc_i2cTunnel(bool isRead, uint8_t address, uint8_t reg, uint8_t dataLen, uint8_t* data, uint8_t chipID)
{
	uint8_t outputBuf[4] = {0xCC};
	uint8_t inputBuf[16] = {0};
	uint32_t u32rsp = 0;
	
	/* Overflow longest I2C payload for transaction */
	if (dataLen > 10) {
		LOG_ERR("!!error!! Overflow longest I2C payload for transaction, chipID:%d", chipID);
		return 0;
	}
	
	/* I2C3m read */
	if (isRead) {
		inputBuf[0] = address;
		inputBuf[1] = reg;
		inputBuf[2] = dataLen;
		
		u32rsp = amd_crb_drivers_tps_cmdExecutor(1000, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, 3, "I2Cr", outputBuf, (dataLen + 1));
		
		/* Copy output data */
		memcpy (data, &outputBuf[1], dataLen);
		
		if (outputBuf[0] != 0)
			*data = 0;
	}
	/* I2C3m Write */
	else {
		inputBuf[0] = address;
		inputBuf[1] = dataLen + 1;
		inputBuf[2] = reg;
		
		memcpy(&(inputBuf[3]), data, dataLen);
		
		u32rsp = amd_crb_drivers_tps_cmdExecutor(1000, TYPEC_PORT_0_IDX + (chipID << 1), inputBuf, (dataLen + 3), "I2Cw", outputBuf, 4);
	}
	
	if (0 != u32rsp) {
		LOG_ERR("!!error!! I2Cw/r returns 0x%08X, chipID:%d", u32rsp, chipID);
		return 0;
	}
	return 1;
}

/**
 * app_4cc_usb4Tbt3Disable
 *
 * @param [in]   port;     port number
 * @return 1 If successful.
 *
 * @note
 *  USB4 disable and TBT3 disable
 */
uint8_t app_4cc_usb4Tbt3Disable(uint8_t port)
{
	uint8_t outputBuf[5];

	/* Input the MFVDO: DFPVDO/NA/NA */
	/* DFPVDO --> Port number:0 Host capable: USB2/3 DFPVDO Version: 0x1 */
	uint32_t Vdo[4] = {0x81000451, 0x00000451, 0x00000908, 0x23800000};
	DRV_TPS_STATUS status;
	DRV_TPS_PORT_CONTROL port_ctrl;
	DRV_TPS_INTEL_VID_CONFIG intel_vid_config;
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	
	/* Disable the USB4 and set the MF */
	amd_crb_drivers_tps_txVidUpdate(port, 4, false, Vdo);
	
	/* Check ConnState */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_STATUS, &status, sizeof(status), port);
	
	/* disable the TBT ALT MODE */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INTEL_VID_CONFIG, &intel_vid_config, sizeof(intel_vid_config), port);
	intel_vid_config.f.IntelVidEnabled = 0;
	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INTEL_VID_CONFIG, &intel_vid_config, sizeof(intel_vid_config), port);
	
	/* ConnState =< b'101 */
	if (status.f.ConnState <= TIPD_REG_STATUS_CONNSTATUS_RSVD) {
		/* Do nothing */
	}
	/* DataRole == b'0 */
	else if (status.f.DataRole == PRT_TYPE_UFP) {
		/* Do nothing */
	}
	else {

		dpm_set_status(port)->usb4_entered = amd_crb_drivers_tps_isUsb4Enter(port);
		dpm_set_status(port)->tbt3_entered = amd_crb_drivers_tps_isTbtEnter(port);
		dpm_set_status(port)->dp_entered = amd_crb_drivers_tps_isDpEnter(port);

		if (dpm_get_info(port)->usb4_entered)
			app_4cc_dataReset(port);
		else
			app_4cc_discoveryProcess(port);
	}
	
	/* Enable Automatic ID request field in register 0x29 bit 16 */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
	port_ctrl.f.AutomaticIDRequest = 1;
	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
	
	return 1;
}

/**
 * app_4cc_usb4Tbt3Enable
 *
 * @param [in]   port;     port number
 * @return 1 If successful.
 *
 * @note
 *  USB4 enable and TBT3 enable
 */
uint8_t app_4cc_usb4Tbt3Enable(uint8_t port)
{
	uint8_t outputBuf[5];

	/* Input the USB4: UFPVDO/UFPVDO1/DFPVDO2 */
	/* UFPVDO0 -->  */
	/* UFPVDO1 -->  */
	/* DFPVDO2 --> */
	uint32_t Vdo[6] = {0x95000451, 0x00000451, 0x00000908, 0x28800003, 0x00000000, 0x7000000};
	DRV_TPS_STATUS status;
	DRV_TPS_PORT_CONTROL port_ctrl;
	DRV_TPS_INTEL_VID_CONFIG intel_vid_config;
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	
	/* Enable the USB4 and set the USB4 */
	amd_crb_drivers_tps_txVidUpdate(port, 6, true, Vdo);
	
	/* Check ConnState */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_STATUS, &status, sizeof(status), port);
	
	/* enable the TBT ALT MODE */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INTEL_VID_CONFIG, &intel_vid_config, sizeof(intel_vid_config), port);
	intel_vid_config.f.thunderBoltModeEnabled = 1;
	intel_vid_config.f.IntelVidEnabled = 1;
	intel_vid_config.f.ThunderBoltAutoEntryAllowed = 1;
	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INTEL_VID_CONFIG, &intel_vid_config, sizeof(intel_vid_config), port);
	
	/* ConnState =< b'101 */
	if (status.f.ConnState <= TIPD_REG_STATUS_CONNSTATUS_RSVD) {
		/* Do nothing */
	}
	/* DataRole == b'0 */
	else if (status.f.DataRole == PRT_TYPE_UFP) {
		/* Do nothing */
	}
	else {
		dpm_set_status(port)->usb4_entered = amd_crb_drivers_tps_isUsb4Enter(port);
		dpm_set_status(port)->tbt3_entered = amd_crb_drivers_tps_isTbtEnter(port);
		dpm_set_status(port)->dp_entered = amd_crb_drivers_tps_isDpEnter(port);

		if (dpm_get_info(port)->usb4_entered)
			app_4cc_dataReset(port);
		else
			app_4cc_discoveryProcess(port);
	}
	
	/* Enable Automatic ID request field in register 0x29 bit 16 */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
	port_ctrl.f.AutomaticIDRequest = 1;
	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
	
	return 1;
}

/**
 * app_4cc_usb4DisableTbt3Enable
 *
 * @param [in]   port;     port number
 * @return 1 If successful.
 *
 * @note
 *  USB4 disable and TBT3 enable
 */
uint8_t app_4cc_usb4DisableTbt3Enable(uint8_t port)
{
	uint8_t outputBuf[5];

	/* Input the MFVDO: DFPVDO/NA/NA */
	/* DFPVDO --> Port number:0 Host capable: USB2/3 DFPVDO Version: 0x1 */
	uint32_t Vdo[4] = {0x81000451, 0x00000451, 0x00000908, 0x23800000};
	DRV_TPS_STATUS status;
	DRV_TPS_PORT_CONTROL port_ctrl;
	DRV_TPS_INTEL_VID_CONFIG intel_vid_config;
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	
	/* Disable the USB4 and set the MF */
	amd_crb_drivers_tps_txVidUpdate(port, 4, false, Vdo);
	
	/* Check ConnState */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_STATUS, &status, sizeof(status), port);
	
	/* enable the TBT ALT MODE */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INTEL_VID_CONFIG, &intel_vid_config, sizeof(intel_vid_config), port);
	intel_vid_config.f.IntelVidEnabled = 1;
	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INTEL_VID_CONFIG, &intel_vid_config, sizeof(intel_vid_config), port);
	
	/* ConnState =< b'101 */
	if (status.f.ConnState <= TIPD_REG_STATUS_CONNSTATUS_RSVD) {
		/* Do nothing */
	}
	/* DataRole == b'0 */
	else if (status.f.DataRole == PRT_TYPE_UFP) {
		/* Do nothing */
	}
	else {
		dpm_set_status(port)->usb4_entered = amd_crb_drivers_tps_isUsb4Enter(port);
		dpm_set_status(port)->tbt3_entered = amd_crb_drivers_tps_isTbtEnter(port);
		dpm_set_status(port)->dp_entered = amd_crb_drivers_tps_isDpEnter(port);

		if (dpm_get_info(port)->usb4_entered)
			app_4cc_dataReset(port);
		else
			app_4cc_discoveryProcess(port);
	}
	
	/* Enable Automatic ID request field in register 0x29 bit 16 */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
	port_ctrl.f.AutomaticIDRequest = 1;
	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
	
	return 1;
}

/**
 * app_4cc_usb4EnableTbt3Disable
 *
 * @param [in]   port;     port number
 * @return 1 If successful.
 *
 * @note
 *  USB4 enable and TBT3 enable
 */
uint8_t app_4cc_usb4EnableTbt3Disable(uint8_t port)
{
	uint8_t outputBuf[5];

	/* Input the USB4: UFPVDO/UFPVDO1/DFPVDO2 */
	/* UFPVDO0 -->  */
	/* UFPVDO1 -->  */
	/* DFPVDO2 --> */
	uint32_t Vdo[6] = {0x95000451, 0x00000451, 0x00000908, 0x28800003, 0x00000000, 0x7000000};
	DRV_TPS_STATUS status;
	DRV_TPS_PORT_CONTROL port_ctrl;
	DRV_TPS_INTEL_VID_CONFIG intel_vid_config;
	
	memset(outputBuf, 0xCC, sizeof(outputBuf));
	
	/* Enable the USB4 and set the USB4 */
	amd_crb_drivers_tps_txVidUpdate(port, 6, true, Vdo);
	
	/* Check ConnState */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_STATUS, &status, sizeof(status), port);
	
	/* disable the TBT ALT MODE */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_INTEL_VID_CONFIG, &intel_vid_config, sizeof(intel_vid_config), port);
	intel_vid_config.f.IntelVidEnabled = 0;
	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_INTEL_VID_CONFIG, &intel_vid_config, sizeof(intel_vid_config), port);
	
	/* ConnState =< b'101 */
	if (status.f.ConnState <= TIPD_REG_STATUS_CONNSTATUS_RSVD) {
		/* Do nothing */
	}
	/* DataRole == b'0 */
	else if (status.f.DataRole == PRT_TYPE_UFP) {
		/* Do nothing */
	}
	else {
		dpm_set_status(port)->usb4_entered = amd_crb_drivers_tps_isUsb4Enter(port);
		dpm_set_status(port)->tbt3_entered = amd_crb_drivers_tps_isTbtEnter(port);
		dpm_set_status(port)->dp_entered = amd_crb_drivers_tps_isDpEnter(port);

		if (dpm_get_info(port)->usb4_entered)
			app_4cc_dataReset(port);
		else
			app_4cc_discoveryProcess(port);
	}
	
	/* Enable Automatic ID request field in register 0x29 bit 16 */
	amd_crb_drivers_tps_regAccess(TI_REG_READ, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
	port_ctrl.f.AutomaticIDRequest = 1;
	amd_crb_drivers_tps_regAccess(TI_REG_WRITE, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
	
	return 1;
}

/**
 * app_4cc_crossbarMailBox
 *
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]     port;     port number
 * @param [in]  dataBuf;     data buffer pointer
 * @return 1 If successful.
 *
 * @note
 *  use TIPD I2C tunnel to control PD I2C master to send message to APU mailbox
 */
uint8_t app_4cc_crossbarMailBox(bool isRead, uint8_t port, uint8_t* dataBuf)
{
	uint32_t u32rsp;
	uint8_t tmpBuf[8] = {0};

	if (isRead) {
		/**
		 * Crossbar MailBox read
		 */
		u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, NULL, 0, "MBXr", dataBuf, 4);
		
		if (0 != u32rsp) {
			LOG_ERR("!!error!! MBXr returns 0x%08X, port:%d", u32rsp, port);
			return 0;
		}
	}
	else {
		/**
		 * Crossbar MailBox write
		 */
		u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, dataBuf, 4, "MBXw", tmpBuf, 4);
	}
	
	return 0;
}

/**
* app_4cc_setGpioHigh
*
* @param [1n]     port;       port number
* @param [1n]     gpio_port;  gpio port
* @param [1n]     gpio_index; gpio index
* @return 0 if pass
*
* @note
*  force GPIO to high
*/
uint8_t app_4cc_setGpioHigh(uint8_t port, uint8_t gpio_port, uint8_t gpio_index)
{
	uint32_t u32rsp;
	uint8_t inputBuf[2] = {0};
	uint8_t outputBuf[8] = {0x00};

	/*
	 * gpio_port: gpio port number 0 or 1
	 * gpio_index: gpio index 0~13
	 * */
 
	inputBuf[0] = gpio_port;
	inputBuf[1] = gpio_index;
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, inputBuf, 2, "GPsh", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! GPsh returns 0x%08X, port:%d", u32rsp, port);
		return 0;
	}
 
	return 0;
}
 
/**
* app_4cc_setGpioLow
*
* @param [1n]     port;      port number
* @param [1n]     gpio_port;  gpio port
* @param [1n]     gpio_index; gpio index
* @return 0 if pass
*
* @note
*  force gpio to low
*/
uint8_t app_4cc_setGpioLow(uint8_t port, uint8_t gpio_port, uint8_t gpio_index)
{
	uint32_t u32rsp;
	uint8_t inputBuf[2] = {0};
	uint8_t outputBuf[8] = {0x00};
 
	inputBuf[0] = gpio_port;
	inputBuf[1] = gpio_index;
	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, inputBuf, 2, "GPsl", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! GPsl returns 0x%08X, port:%d", u32rsp, port);
		return 0;
	}
 
	return 0;
}

/**
 * app_4cc_forceRtPower
 *
 * @param [in]     port;     port number
 * @param [in]   Enable;     enable the power/reset or not
 *
 * @note
 *  force to enable the re-timer power and reset.
 */
void app_4cc_forceRtPower(uint8_t port, bool Enable)
{
 
#ifdef amd_crb_drivers_portCtrlRetimerUpdate
	//amd_crb_drivers_portCtrlRetimerUpdate(port, Enable);
#endif

	LOG_DBG("%s port: %d, enable: %d attach: %d dead_bat: %d\r\n", __FUNCTION__, port, Enable, dpm_get_info(port)->attach, dpm_get_info(port)->dead_bat);

	if (Enable) {
		/* only force without device attached */
		if ((port == TYPEC_PORT_0_IDX) && (dpm_get_info(TYPEC_PORT_0_IDX)->attach == false)) {
			app_4cc_setGpioHigh(TYPEC_PORT_0_IDX, 0, 2); /* PD0 GPIO0_2 -> USBC0_PD_RT_PWREN */
			app_4cc_setGpioHigh(TYPEC_PORT_0_IDX, 0, 0); /* PD0 GPIO0_0 -> USBC0_PD_RT_RSTn */
			LOG_DBG("P0_RT_ON");
		}
		if ((port == TYPEC_PORT_1_IDX) && (dpm_get_info(TYPEC_PORT_1_IDX)->attach == false)) {
			app_4cc_setGpioHigh(TYPEC_PORT_1_IDX, 0, 13); /* PD0 GPIO0_13 -> USBC1_PD_RT_PWREN */
			app_4cc_setGpioHigh(TYPEC_PORT_1_IDX, 0, 1); /* PD0 GPIO0_1 -> USBC1_PD_RT_RSTn */
			LOG_DBG("P1_RT_ON");
		}
		if ((port == TYPEC_PORT_2_IDX) && (dpm_get_info(TYPEC_PORT_2_IDX)->attach == false)) {
			app_4cc_setGpioHigh(TYPEC_PORT_2_IDX, 0, 2); /* PD1 GPIO0_2 -> USBC2_PD_RT_PWREN */
			app_4cc_setGpioHigh(TYPEC_PORT_2_IDX, 0, 0); /* PD1 GPIO0_0 -> USBC2_PD_RT_RSTn */
			LOG_DBG("P2_RT_ON");
		}
 
	} else {
		if (port == TYPEC_PORT_0_IDX) {
			if (dpm_get_info(TYPEC_PORT_0_IDX)->attach == false) {
				app_4cc_setGpioLow(TYPEC_PORT_0_IDX, 0, 2); /* PD0 GPIO2 -> USBC0_PD_RT_PWREN */
				app_4cc_setGpioLow(TYPEC_PORT_0_IDX, 0, 0); /* PD0 GPIO0 -> USBC0_PD_RT_RSTn */
				LOG_DBG("P0_RT_OFF");
			} else if (dpm_get_info(TYPEC_PORT_0_IDX)->dead_bat == false){
				amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_NULL);
				amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_DUAL);
				LOG_DBG("P0_Toggle");
			}
		}
		if (port == TYPEC_PORT_1_IDX) {
			if (dpm_get_info(TYPEC_PORT_1_IDX)->attach == false) {
				app_4cc_setGpioLow(TYPEC_PORT_1_IDX, 0, 13); /* PD0 GPIO13 -> USBC1_PD_RT_PWREN */
				app_4cc_setGpioLow(TYPEC_PORT_1_IDX, 0, 1); /* PD0 GPIO1 -> USBC1_PD_RT_RSTn */
				LOG_DBG("P1_RT_OFF");
			} else if (dpm_get_info(TYPEC_PORT_0_IDX)->dead_bat == false){
				amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_NULL);
				amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_DUAL);
				LOG_DBG("P1_Toggle");
			}
		}
		if (port == TYPEC_PORT_2_IDX) {
			if (dpm_get_info(TYPEC_PORT_2_IDX)->attach == false) {
				app_4cc_setGpioLow(TYPEC_PORT_2_IDX, 0, 2); /* PD1 GPIO2 -> USBC1_PD_RT_PWREN */
				app_4cc_setGpioLow(TYPEC_PORT_2_IDX, 0, 0); /* PD1 GPIO0 -> USBC1_PD_RT_RSTn */
				LOG_DBG("P2_RT_OFF");
			} else if (dpm_get_info(TYPEC_PORT_2_IDX)->dead_bat == false){
				amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_2_IDX, PRT_NULL);
				amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_2_IDX, PRT_DUAL);
				LOG_DBG("P2_Toggle");
			}
		}
	}
}

/**
 * app_4cc_srdy
 *
 * @param [1n]     port;      port number
 * @return 0 if pass
 *
 * @note
 *  enable the sink port
 */
uint8_t app_4cc_srdy(uint8_t port)
{
	uint32_t u32rsp;
	uint8_t inputBuf = 0;
	uint8_t outputBuf[8] = {0x00};

	if (port == TYPEC_PORT_0_IDX)
		inputBuf = 0x02; /* PP_EXT1 */
	else if (port == TYPEC_PORT_1_IDX)
		inputBuf = 0x03; /* PP_EXT2 */
#if PD_TRIPPORT_ENABLE
	else if (port == TYPEC_PORT_2_IDX)
		inputBuf = 0x02; /* PP_EXT1 */
#endif

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, &inputBuf, 0, "SRDY", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! SRDY returns 0x%08X, port:%d", u32rsp, port);
		return 0;
	}

	return 0;
}

/**
 * app_4cc_sryr
 *
 * @param [1n]     port;      port number
 * @return 0 if pass
 *
 * @note
 *  disable the sink port
 */
uint8_t app_4cc_sryr(uint8_t port)
{
	uint32_t u32rsp;
	uint8_t outputBuf[8] = {0x00};

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, NULL, 0, "SRYR", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! SRYR returns 0x%08X, port:%d", u32rsp, port);
		return 0;
	}

	return 0;
}

/**
 * app_4cc_ppextVbusSwConfig
 * @return true: detect two identical PD source
 * @note
 *  dynamic change the ppext VBUS SW config
 */
bool app_4cc_ppextVbusSwConfig(void)
{
#if (!PD_TRIPPORT_ENABLE && PD_DUALPORT_ENABLE)
	bool portSink0 = false, portSink1 = false;
	static bool ppextenable = false;
	uint8_t outputBuf[8] = {0};

	_s_xPort0SPdo.val = amd_crb_drivers_tps_readSnkPdo(TYPEC_PORT_0_IDX);
	_s_xPort1SPdo.val = amd_crb_drivers_tps_readSnkPdo(TYPEC_PORT_1_IDX);

	LOG_DBG("PD0: %x %x", _s_xPort0SPdo.fixed_snk.op_current,_s_xPort0SPdo.fixed_snk.voltage);
	LOG_DBG("PD1: %x %x", _s_xPort1SPdo.fixed_snk.op_current,_s_xPort1SPdo.fixed_snk.voltage);

	if ((dpm_get_info(TYPEC_PORT_0_IDX)->attach) &&
		(dpm_get_info(TYPEC_PORT_0_IDX)->cur_port_role == PRT_ROLE_SINK) &&
		(dpm_get_info(TYPEC_PORT_0_IDX)->contract_exist)) {
		portSink0 = true;
	}

	if ((dpm_get_info(TYPEC_PORT_1_IDX)->attach) &&
		(dpm_get_info(TYPEC_PORT_1_IDX)->cur_port_role == PRT_ROLE_SINK) &&
		(dpm_get_info(TYPEC_PORT_1_IDX)->contract_exist)) {
		portSink1 = true;
	}

	/* There are two power source attached */
	if ((portSink0 == true) && (portSink1 == true)) {
		if (_s_xPort0SPdo.fixed_src.voltage == 0x190) { /* voltage == 20V */

		} else if (_s_xPort0SPdo.fixed_src.voltage == 0x230) { /* voltage == 28V */

		} else { /* other voltages */
			ppextenable = false;
			return ppextenable;
		}

		/* check if they has same RDO */
		if ((_s_xPort0SPdo.fixed_snk.op_current * _s_xPort0SPdo.fixed_snk.voltage) ==
			(_s_xPort1SPdo.fixed_snk.op_current * _s_xPort1SPdo.fixed_snk.voltage)) {
			/* Need to enable both sink path */
			if (ppextenable == false) {
				/* need to force clear dead battery flag before force ppext on */
				amd_crb_drivers_tps_cmdExecutor(0, TYPEC_PORT_0_IDX, NULL, 0, "DBfg", outputBuf, 4);

				app_4cc_srdy(TYPEC_PORT_0_IDX);
				app_4cc_srdy(TYPEC_PORT_1_IDX);
				ppextenable = true;
			}
		} else {
			ppextenable = false;
		}
	} else if (portSink0 == true) {
		app_4cc_srdy(TYPEC_PORT_0_IDX);
		ppextenable = false;
	} else if (portSink1 == true) {
		app_4cc_srdy(TYPEC_PORT_1_IDX);
		ppextenable = false;
	} else {
		ppextenable = false;
	}

	return ppextenable;
#else
	return false;
#endif
}

/**
 * app_4cc_eprEnable
 *
 * @param [1n]     port;      port number
 * @param [1n]     en;        true means enable EPR
 * @return 0 if pass
 *
 * @note
 *  enable or disable the EPR mode
 */
uint8_t app_4cc_eprEnable(uint8_t port, bool en)
{
	uint32_t u32rsp;
	uint8_t inputBuf = 0;
	uint8_t outputBuf[8] = {0x00};

	if (en)
		inputBuf = 0x01;
	else
		inputBuf = 0x00;

	u32rsp = amd_crb_drivers_tps_cmdExecutor(0, port, &inputBuf, 1, "EPRm", outputBuf, 4);
	if (0 != u32rsp) {
		LOG_ERR("!!error!! EPRm returns 0x%08X, port:%d", u32rsp, port);
		return 0;
	}

	return 0;
}
