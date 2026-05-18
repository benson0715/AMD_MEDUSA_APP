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
#include "app_realtek_pd.h"
#include "board_config.h"
#include "app_ucsi_tunnel.h"
#include "system.h"
#include "app_usbc_task.h"
#include "amd_crb_drivers_pd.h"
#include "amd_crb_drivers_realtek_ucsi.h"

LOG_MODULE_REGISTER(rtkapp, CONFIG_RTK_APP_LOG_LEVEL);

#define APP_RTK_FLASH_PACKET_LEN       (29)

/* ************************** *
 *     static valuable        *
 * ************************** */
static pd_do_t _s_xPort0SPdo = {0};
static pd_do_t _s_xPort1SPdo = {0};
static pd_do_t _s_xPort2SPdo = {0};

/* ************************** *
 *     global valuable        *
 * ************************** */
static pd_do_t _s_rtk_xSysRdo;

/************************************
 *     RealTek PD interrupt handling     *
 ************************************/

/*
 * app_rtk_IntTopHalf
 * 
 * @param [in]    pinSt;     the interrupt input pin status (1 - high, 0 - low)
 * @param [in]    chipID;    the chip id for the interrupt source
 * @return void
 *
 * @note
 *  ITEPD interrupt handling routine. This function is executed 
 *  in the MSP context.
 */
static volatile uint8_t _s_EventPending[2] = {0};
void app_rtk_IntTopHalf(uint8_t pinSt, uint8_t chipID)
{
	/* PD INT# low active */
	/* If there are two realtek PD controllers, either interrupt pins may trigger interrput callback function.
     In the callback function, we will loop all the three or four ports. 
	   So in 4cc layer, we think all the ports in one virtual chip */
	if (!pinSt) {
		_s_EventPending[chipID] = 1;
		app_usbc_giveSem();
	}
}

/*
 * app_rtk_intPending
 *
 * @return if there is interrupt pending
 *
 * @note
 *  return if there is pending interrupt which is needed to handle
 */
bool app_rtk_intPending(void)
{
	if (_s_EventPending[0] || _s_EventPending[1])
		return true;
	else
		return false;
}

/*
 * app_rtk_vonderIdentify
 *
 * @return if it is realtek PD module
 *
 * @note
 *  return if this is realtek PD module based on the I2C address
 */
bool app_rtk_vonderIdentify(void)
{
	DRV_RTK_OUTPUT_GET_IC_STATUS icStatus;
	int ret;
	bool ret0 = false, ret1 = false;
	uint32_t attempts = 0;

	do {
		/* enable the vendor command mode */
		ret = amd_crb_drivers_rtk_writeVendorCmdEn(TYPEC_PORT_0_IDX, true, false);
		k_sleep(K_MSEC(10));

		if (ret != 0) {
			LOG_DBG("enable flash access mode fail");
			return false;
		}

		/* get the ic status for the bank1 or bank0 */
		ret = amd_crb_drivers_rtk_readIcStatus(TYPEC_PORT_0_IDX, 0x1F, &icStatus);

		if (ret == 0){
			/* realtek chip0 detected */
			ret0 = true;
			break;
		} else{
			/* it could be system in rom boot mode (no valid FW at all). It needs to read 20 bytes */
			ret = amd_crb_drivers_rtk_readIcStatus(TYPEC_PORT_0_IDX, 0x14, &icStatus);
			if (ret != 0) {
				LOG_DBG("read PD0 status fail");
				return false;
			}
			ret0 = true;
			break;
		}

		k_sleep(K_MSEC(10)); /* sleep "10" ms */
		attempts++;
	} while (attempts < 10);  /* 100ms delay */
	
	attempts = 0;
	
	do {
		/* enable the vendor command mode */
		ret = amd_crb_drivers_rtk_writeVendorCmdEn(TYPEC_PORT_2_IDX, true, false);
		k_sleep(K_MSEC(10));

		if (ret != 0) {
			LOG_DBG("enable flash access mode fail");
			return false;
		}

		ret = amd_crb_drivers_rtk_readIcStatus(TYPEC_PORT_2_IDX, 0x1F, &icStatus);
		
		if (ret == 0){
			/* realtek chip1 detected */
			ret1 = true;
			break;
		} else{
			/* it could be system in rom boot mode (no valid FW at all). It needs to read 20 bytes */
			ret = amd_crb_drivers_rtk_readIcStatus(TYPEC_PORT_2_IDX, 0x14, &icStatus);
			if (ret != 0) {
				LOG_DBG("read PD1 status fail");
				return false;
			}
			ret1 = true;
			break;
		}

		k_sleep(K_MSEC(10)); /* sleep "10" ms */
		attempts++;
	} while (attempts < 10);  /* 100ms delay */

	return (ret0 & ret1);
}

/*
 * app_rtk_IntBottomHalf
 * 
 * @param [in]    chipID;    chip id
 * @return void
 *
 * @note
 *  handle the interrupt service
 */
void app_rtk_IntBottomHalf(uint8_t chipID)
{
	uint8_t loopCount = 0;
	uint8_t dev_addr = 0;
	uint8_t port = 0;

	if ((!_s_EventPending[chipID]))  
		return;
	
	LOG_DBG("TEKPD_INTERRUPT!!!");

	/*
	 * clear the INT pending indicator
	 */
	_s_EventPending[chipID] = 0;
	/* The INT pending indicator may be set up
	 * again by the following proc running
	 */
	
	while (1) {
		/* chip0, 1. read the interrupt event */
		if (chipID == 0) {
			/* clear the interrupt */
			amd_crb_drivers_rtk_araAccess(&dev_addr, chipID);

			/* break because of wrong device address */
			if (dev_addr == (I2C_REALTEK_ADDRESS_P0 << 1)) {
				port = TYPEC_PORT_0_IDX;
				LOG_DBG("RTK_INT0 right addr: %x P%d", dev_addr, port);
			} else if (dev_addr == (I2C_REALTEK_ADDRESS_P1 << 1)){
				port = TYPEC_PORT_1_IDX;
				LOG_DBG("RTK_INT0 right addr: %x P%d", dev_addr, port);
			} else {
				LOG_ERR("RTK_INT0 wrong addr: %x", dev_addr);
				break;
			}
			/* process the event */
			app_rtk_usbcInterruptProc(port);
		}
#if PD_TRIPPORT_ENABLE
		/* chip1, 1. read the interrupt event */
		else if (chipID == 1) {
			/* clear the interrupt */
			amd_crb_drivers_rtk_araAccess(&dev_addr, chipID);

			/* break because of wrong device address */
			if (dev_addr == (I2C_REALTEK_ADDRESS_P2 << 1)) {
				port = TYPEC_PORT_2_IDX;
				LOG_DBG("RTK_INT1 right addr: %x P%d", dev_addr, port);
			} else {
				LOG_ERR("RTK_INT1 wrong addr: %x", dev_addr);
				break;
			}

			/* process the event p0 */
			app_rtk_usbcInterruptProc(port);
		}
#endif
		/* Exit current loop if PD INT asserted more than three handling loop */
		loopCount ++;
		if (loopCount > RTK_REG_ACCESS_TRTRY_TIME) {
			LOG_ERR("Something cannot be handled by current interrupt loop, tried %d times.", loopCount);
			_s_EventPending[chipID] = 1;
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

/*
 * app_rtk_getHigherPortSnkPdo
 * 
 * @param void
 * @return pd_do_t
 * @note
 *  Return the highest Rdo from ite PD.
 */
pd_do_t app_rtk_getHigherPortSnkPdo(void)
{
	_s_rtk_xSysRdo = _s_xPort0SPdo;
#if PD_DUALPORT_ENABLE
	/* Get the higher RDO from two typec ports */
	if ((_s_xPort0SPdo.fixed_snk.op_current * _s_xPort0SPdo.fixed_snk.voltage) < 
	     _s_xPort1SPdo.fixed_snk.op_current * _s_xPort1SPdo.fixed_snk.voltage) {
		_s_rtk_xSysRdo = _s_xPort1SPdo;
	}
#endif
#if PD_TRIPPORT_ENABLE
	if ((_s_rtk_xSysRdo.fixed_snk.op_current * _s_rtk_xSysRdo.fixed_snk.voltage) <
	    _s_xPort2SPdo.fixed_snk.op_current * _s_xPort2SPdo.fixed_snk.voltage) {
		_s_rtk_xSysRdo = _s_xPort2SPdo;
	}
#endif
	
	return _s_rtk_xSysRdo;
}


/*
 * app_rtk_resetParam
 * 
 * @param [in]    port;    port number
 * @return void
 *
 * @note
 *  reset the ITE PD related parameters
 */
void app_rtk_resetParam(uint8_t port)
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

	if (port == TYPEC_PORT_0_IDX)
		_s_xPort0SPdo.val = 0;
	else if (port == TYPEC_PORT_1_IDX)
		_s_xPort1SPdo.val = 0;
	else if (port == TYPEC_PORT_2_IDX)
		_s_xPort2SPdo.val = 0;
}

/*
 * app_rtk_usbcInterruptProc
 *
 * @param [in]    port;    port number
 * @return void
 *
 * @note
 *  Process the usbc interrupt triggered by hardware pin
 */
void app_rtk_usbcInterruptProc(uint8_t port)
{
	int ret;
	DRV_RTK_INPUT_SET_NOTIFY PD_AMS;
	DRV_RTK_INPUT_SET_NOTIFY PD_AMS_MASK;
	DRV_RTK_OUTPUT_GET_RTK_STATUS event;
	DRV_RTK_OUTPUT_GET_CURR_PARTNER_SRC_PDO pdo;
	pd_do_t tmpPdo;
	app_evt_t evt = APP_EVT_NULL;
	alt_mode_app_evt_t alt_mode_event = AM_NO_EVT;
	
	/* set the event mask */
	PD_AMS_MASK.f.connectChange = 1;
	PD_AMS_MASK.f.externalSupplyChange = 1;
	PD_AMS_MASK.f.negPwrLevelChange = 1;
	PD_AMS_MASK.f.portPartnerChanged = 1;
	PD_AMS_MASK.f.negPwrLevelChange = 1;
	PD_AMS_MASK.f.suppProvdierCapChange = 1;
	PD_AMS_MASK.f.pwrDirchanged = 1;
	PD_AMS_MASK.f.alternateFLowChange = 1;
	PD_AMS_MASK.f.portOpModeChange = 1;

	/******* get the rtk status from Px *******/
	ret = amd_crb_drivers_rtk_writeVendorCmdEn(port, true, false);
	if (ret != 0) {
		LOG_DBG("enable vendor access mode fail P%d", port);
		return;
	}
	ret = amd_crb_drivers_rtk_ucsiGetStatus(port, &event);
	if (ret != 0) {
		LOG_ERR("[!!!ERROR!!!]  fail to read realtek P%d interrupt", port);
		return;
	} else {
		/* copy the event to PD_AMS */
		memcpy(PD_AMS.buf, &(event.buf), 4);
	}

	/* process only with expected mask bits */
	if ((PD_AMS_MASK.buf32 & PD_AMS.buf32) != 0) {
		/* connection */
		if (event.f.connectStatus) {
			dpm_set_status(port)->attach = true;
			dpm_set_status(port)->connect = true;
			LOG_DBG("Attached P%d", port);
			evt = APP_EVT_CONNECT;
		} else {
			dpm_set_status(port)->attach = false;
			dpm_set_status(port)->connect = false;
			app_rtk_resetParam(port);
			LOG_DBG("Unattached P%d", port);
			evt = APP_EVT_DISCONNECT;
		}
		/* polarity */
		if (event.f.plugDirection)
			dpm_set_status(port)->polarity = 1;
		else
			dpm_set_status(port)->polarity = 0;
		/* src/snk */
		if (event.f.pwrDirection)
			dpm_set_status(port)->cur_port_role = PRT_ROLE_SOURCE;
		else
			dpm_set_status(port)->cur_port_role = PRT_ROLE_SINK;
		/*dfp/ufp*/
		if (event.f.portPartnerType == UFP_ATT)
			dpm_set_status(port)->cur_port_type = PRT_TYPE_DFP;
		else if(event.f.portPartnerType == DFP_ATT)
			dpm_set_status(port)->cur_port_type = PRT_TYPE_UFP;

		if (event.f.PD_AMS) {
			LOG_DBG("PD_AMS inProcess P%d", port);
		}

		if (PD_AMS.f.pwrDirchanged)
			evt = APP_EVT_PR_SWAP_COMPLETE;
		if (PD_AMS.f.portPartnerChanged)
			evt = APP_EVT_DR_SWAP_COMPLETE;
		if (PD_AMS.f.pdRstCmplt)
			evt = APP_EVT_HARD_RESET_COMPLETE;

		/* contract exist */
		if (event.f.contractValid)
			dpm_set_status(port)->contract_exist = true;
		else
			dpm_set_status(port)->contract_exist = false;

		/* check alt mode
		 * 0: Discover identity not
		 * 1: Discover identity Done
		 * 2: Discover SVID Done
		 * 3: Discover Mode Done
		 * 4: Enter Mode Done
		 * 5: DP Status update Command Done
		 * 6: DP Configure Command Done
		 * */
		if (event.f.altModeStatus == 4) {
			dpm_set_status(port)->alt_mode_entered = true;
			alt_mode_event = AM_EVT_ALT_MODE_ENTERED;
			evt = APP_EVT_ALT_MODE;
		} else if (event.f.altModeStatus == 3 ) {
			alt_mode_event = AM_EVT_SUPP_ALT_MODE_CHNG;
			evt = APP_EVT_ALT_MODE;
		}

		/* get sink rdo */
		if (event.f.contractValid && (event.f.VBSIN_EN == 3) && (event.f.pwrDirection == PRT_ROLE_SINK)) {
			switch (port)
			{
				case TYPEC_PORT_0_IDX:
					_s_xPort0SPdo.val = amd_crb_drivers_rtk_readSnkRdo(port);
					amd_crb_drivers_rtk_readCurPartnerSrcPdo(port, &pdo);
					tmpPdo.val = pdo.f.PDO1;
					_s_xPort0SPdo.fixed_snk.voltage = tmpPdo.fixed_snk.voltage;
					LOG_DBG("P0 Volt:%x Cur:%x", _s_xPort0SPdo.fixed_snk.voltage, _s_xPort0SPdo.fixed_snk.op_current);
					break;
				case TYPEC_PORT_1_IDX:
					_s_xPort1SPdo.val = amd_crb_drivers_rtk_readSnkRdo(port);
					amd_crb_drivers_rtk_readCurPartnerSrcPdo(port, &pdo);
					tmpPdo.val = pdo.f.PDO1;
					_s_xPort1SPdo.fixed_snk.voltage = tmpPdo.fixed_snk.voltage;
					LOG_DBG("P1 Volt:%x Cur:%x", _s_xPort1SPdo.fixed_snk.voltage, _s_xPort1SPdo.fixed_snk.op_current);
					break;
				case TYPEC_PORT_2_IDX:
					_s_xPort2SPdo.val = amd_crb_drivers_rtk_readSnkRdo(port);
					amd_crb_drivers_rtk_readCurPartnerSrcPdo(port, &pdo);
					tmpPdo.val = pdo.f.PDO1;
					_s_xPort2SPdo.fixed_snk.voltage = tmpPdo.fixed_snk.voltage;
					LOG_DBG("P2 Volt:%x Cur:%x", _s_xPort2SPdo.fixed_snk.voltage, _s_xPort2SPdo.fixed_snk.op_current);
					break;
				default:
					break;
			}
		}

		switch (event.f.portPartnerType) {
			case DFP_ATT:
			case UFP_ATT:
			case PWR_CABLE_UFP_ATT:
				if (event.f.pwrDirection == PRT_ROLE_SINK)
					dpm_set_status(port)->attached_dev = DEV_SNK;
				else
					dpm_set_status(port)->attached_dev = DEV_SRC;
				break;

			case DBG_ACC_ATT:
				dpm_set_status(port)->attached_dev = DEV_DBG_ACC;
				break;

			case AUDIO_ACC_ATT:
				dpm_set_status(port)->attached_dev = DEV_AUD_ACC;
				break;

			default:
				break;
		}

		LOG_DBG("Update: P%d P_Polarity:%d P_Role:%d P_Type:%d ContractExist:%d", port,
																 dpm_set_status(port)->polarity,
																 dpm_set_status(port)->cur_port_role,
																 dpm_set_status(port)->cur_port_type,
																 dpm_set_status(port)->contract_exist);
		/* process the pd event callback */
		if (evt != APP_EVT_NULL) {
			/* Call the pd event handler function to inform the ec ucsi engine */
			amd_crb_drivers_rtk_ucsi_pdEventHandler(port, evt, alt_mode_event);
		}
	}
}


/*
 * app_rtk_loadInitialStatus
 *
 * @param [in]    port;    port number
 * @return void
 *
 * @note
 *  load the PD initial status after system power on
 */
void app_rtk_loadInitialStatus(uint8_t port)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);
	int ret = 0;
	DRV_RTK_OUTPUT_GET_RTK_STATUS event;
	DRV_RTK_OUTPUT_GET_CURR_PARTNER_SRC_PDO pdo;
	pd_do_t tmpPdo;
	
	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return;
	}
	
	LOG_DBG("Load initial PD status P:%d", port);
	ret = amd_crb_drivers_rtk_ucsiGetStatus(port, &event);
	if (ret != 0) {
		LOG_ERR("[!!!ERROR!!!]  fail to read realtek P%d status", port);
		return;
	} else if (event.f.connectStatus) {
		dpm_stat->attach = true;
		dpm_stat->connect = true;
		LOG_DBG("Attached P%d", port);

		/* polarity */
		if (event.f.plugDirection)
			dpm_stat->polarity = 1;
		else
			dpm_stat->polarity = 0;
		/* src/snk */
		if (event.f.pwrDirection)
			dpm_stat->cur_port_role = PRT_ROLE_SOURCE;
		else
			dpm_stat->cur_port_role = PRT_ROLE_SINK;
		/*dfp/ufp*/
		if (event.f.portPartnerType == DFP_ATT)
			dpm_stat->cur_port_type = PRT_TYPE_DFP;
		else if(event.f.portPartnerType == UFP_ATT)
			dpm_stat->cur_port_type = PRT_TYPE_UFP;

		/* contract exist */
		if (event.f.contractValid)
			dpm_stat->contract_exist = true;
		else
			dpm_stat->contract_exist = false;

		/* check alt mode
		 * 0: Discover identity not
		 * 1: Discover identity Done
		 * 2: Discover SVID Done
		 * 3: Discover Mode Done
		 * 4: Enter Mode Done
		 * 5: DP Status update Command Done
		 * 6: DP Configure Command Done
		 * */
		if (event.f.altModeStatus == 4)
			dpm_set_status(port)->alt_mode_entered = true;

		switch (event.f.portPartnerType) {
			case DFP_ATT:
			case UFP_ATT:
			case PWR_CABLE_UFP_ATT:
				if (event.f.pwrDirection == PRT_ROLE_SINK)
					dpm_stat->attached_dev = DEV_SNK;
				else
					dpm_stat->attached_dev = DEV_SRC;
				break;

			case DBG_ACC_ATT:
				dpm_stat->attached_dev = DEV_DBG_ACC;
				break;

			case AUDIO_ACC_ATT:
				dpm_stat->attached_dev = DEV_AUD_ACC;
				break;

			default:
				break;
		}

		LOG_DBG("Update: P%d P_Polarity:%d P_Role:%d P_Type:%d",  dpm_set_status(port)->polarity,
																  dpm_set_status(port)->cur_port_role,
																  dpm_set_status(port)->cur_port_type, port);
		if (port == TYPEC_PORT_0_IDX) {
			_s_xPort0SPdo.val = amd_crb_drivers_rtk_readSnkRdo(port);
			amd_crb_drivers_rtk_readCurPartnerSrcPdo(port, &pdo);
			tmpPdo.val = pdo.f.PDO1;
			_s_xPort0SPdo.fixed_snk.voltage = tmpPdo.fixed_snk.voltage;
		}

#if PD_DUALPORT_ENABLE
		if (port == TYPEC_PORT_1_IDX) {
			_s_xPort1SPdo.val = amd_crb_drivers_rtk_readSnkRdo(port);
			amd_crb_drivers_rtk_readCurPartnerSrcPdo(port, &pdo);
			tmpPdo.val = pdo.f.PDO1;
			_s_xPort1SPdo.fixed_snk.voltage = tmpPdo.fixed_snk.voltage;
		}
#endif
#if PD_TRIPPORT_ENABLE
		if (port == TYPEC_PORT_2_IDX) {
			_s_xPort2SPdo.val = amd_crb_drivers_rtk_readSnkRdo(port);
			amd_crb_drivers_rtk_readCurPartnerSrcPdo(port, &pdo);
			tmpPdo.val = pdo.f.PDO1;
			_s_xPort2SPdo.fixed_snk.voltage = tmpPdo.fixed_snk.voltage;
		}
#endif
		
	} else {
		LOG_DBG("Unattached %d", port);
		dpm_stat->attach = false;
		dpm_stat->connect = false;
		app_rtk_resetParam(port);
	}
}

/**
 * app_rtk_firmwareUpdate
 *
 * @param [in]            port;       realtek pd port number
 * @param [in]          fwOffset;     binary address in spi flash
 * @param [in]            fwSize;     firmware size
 * @param [in]          icStatus;     pointer to IC status structure
 * @return 0 If successful.
 * @return others fail
 *
 * @note
 *  update realtek pd firmare binary
 */
int app_rtk_firmwareUpdate(uint8_t port, uint32_t fwOffset, uint32_t fwSize, DRV_RTK_OUTPUT_GET_IC_STATUS * icStatus)
{
	int ret = 0;
	uint32_t pdFlashAddress = 0;
	uint32_t pdFlashLength = 0;
	uint32_t spiOffset = fwOffset;
	uint32_t spiSize = fwSize;
	uint32_t rowSize = 0;
	uint8_t inputBuf[APP_RTK_FLASH_PACKET_LEN + 1];

	/* LED notify */
	uint8_t LED_status = 0;
	uint8_t LED_cnt = 0;

	if (icStatus->f.romFlash == 0) {
		/* rom code - need to update both banks */
		pdFlashAddress = 0;
		pdFlashLength = 0x40000;
		if (fwSize != (pdFlashLength >> 1)) {
			LOG_DBG("length error1");
			return -1;
		}

		/* need to erase the bank0 and bank1 */
		amd_crb_drivers_rtk_eraseFlash(port);

	} else if (icStatus->f.romFlash == 1) {
		/* flash code - need to update the idle bank */
		if (icStatus->f.FlashBank == 0x00) { /* ?? don't know what is bank0/1 */
			/* running with Bank0 and update Bank1 - 0x20000~0x3FFFF */
			pdFlashAddress = 0x20000;
			pdFlashLength = 0x20000;
			if (fwSize != pdFlashLength) {
				LOG_DBG("length error2");
				return -1;
			}
		} else if (icStatus->f.FlashBank == 0x01){
			/* running with Bank1 and update Bank0 - 0x00000~0x1FFFF */
			pdFlashAddress = 0;
			pdFlashLength = 0x20000;
			if (fwSize != pdFlashLength) {
				LOG_DBG("length error3");
				return -1;
			}
		} else {
			LOG_DBG("wrong bank number");
			return -1;
		}

	} else {
		LOG_DBG("flash or rom wrong data ");
		return -1;
	}

	/* enable the flash access mode */
	ret = amd_crb_drivers_rtk_writeVendorCmdEn(port, true, true);
	if (ret != 0) {
		LOG_DBG("enable flash access mode fail");
		return -1;
	}

	/* write the target flash */
	while ((ret == 0) && (spiSize != 0)){
		rowSize = APP_RTK_FLASH_PACKET_LEN < spiSize ? APP_RTK_FLASH_PACKET_LEN : spiSize;

		/* read the image via spi */
		if (amd_crb_drivers_sFlashRead(0, spiOffset, rowSize, inputBuf)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X, port:%d", spiOffset, port);
			return -1;
		}

		/* write the flash row */
		ret = amd_crb_drivers_rtk_writeFlash(port, pdFlashAddress + spiOffset - fwOffset, inputBuf, rowSize);
		if (ret != 0) {
			LOG_DBG("write flash fail offset:%x size:%x", spiOffset, spiSize);
			return -1;
		}
		/* need to update both bank */
		if (pdFlashLength == 0x40000) {
			ret = amd_crb_drivers_rtk_writeFlash(port, 0x20000 + spiOffset - fwOffset, inputBuf, rowSize);
			if (ret != 0) {
				LOG_DBG("write flash fail offset%x size:%x", 0x20000+spiOffset, spiSize);
				return -1;
			}
		}

		spiSize -= rowSize;
		spiOffset += rowSize;

		LED_cnt++;
		if (LED_cnt >= 20) {
			LED_cnt = 0;
			LED_status = (LED_status > 0) ? 0 : 1;
			EC_DEBUG_LED(LED_status);
		}
	}

	/* disable the flash access mode */
	ret = amd_crb_drivers_rtk_writeVendorCmdEn(port, true, false);
	if (ret != 0) {
		LOG_DBG("disable flash access mode fail");
		return -1;
	}

	if (pdFlashLength == 0x40000) {
		spiOffset = fwOffset;
		spiSize = fwSize;
		rowSize = 0;
		uint8_t verifyBuf[APP_RTK_FLASH_PACKET_LEN + 1];
		/* read flash from bank0/1 and compare with SPI binary */
		while ((ret == 0) && (spiSize != 0)){
			rowSize = APP_RTK_FLASH_PACKET_LEN < spiSize ? APP_RTK_FLASH_PACKET_LEN : spiSize;

			/* read the image via spi */
			if (amd_crb_drivers_sFlashRead(0, spiOffset, rowSize, inputBuf)) {
				LOG_ERR("!!error!! fail to read SPI at 0x%06X, port:%d", spiOffset, port);
				return -1;
			}
			/* read the flash row */
			ret = amd_crb_drivers_rtk_readFlash(port, 0x00000 + spiOffset - fwOffset, verifyBuf, rowSize);
			if (ret != 0) {
				LOG_DBG("read flash fail offset%x size:%x", spiOffset, spiSize);
				return -1;
			}
			for (uint8_t i = 0; i < rowSize; i++){
				if (inputBuf[i] != verifyBuf[i]) {
					LOG_DBG("flash verify fail");
					return -1;
				}
			}
			ret = amd_crb_drivers_rtk_readFlash(port, 0x20000 + spiOffset - fwOffset, inputBuf, rowSize);
			if (ret != 0) {
				LOG_DBG("read flash fail offset%x size:%x", 0x20000+spiOffset, spiSize);
				return -1;
			}
			for (uint8_t i = 0; i < rowSize; i++){
				if (inputBuf[i] != verifyBuf[i]) {
					LOG_DBG("flash verify fail");
					return -1;
				}
			}

			spiSize -= rowSize;
			spiOffset += rowSize;

			LED_cnt++;
			if (LED_cnt >= 20) {
				LED_cnt = 0;
				LED_status = (LED_status > 0) ? 0 : 1;
#ifdef EC_BLINK_N
				gpio_write_pin(EC_BLINK_N, LED_status);
#endif
			}
		}
	} else {
		/* ISP validation */
		ret = amd_crb_drivers_rtk_ispValid(port);
		if (ret != 0) {
			LOG_DBG("ISP validation fail");
			return -1;
		}
		/* reset to flash */
		amd_crb_drivers_rtk_resetFlash(port);
		if (ret != 0) {
			LOG_DBG("reset to flash fail");
			return -1;
		}

		/* need to delay 500ms for a while to wait PD load new FW */
		k_msleep(500);
	}

	return ret;
}

/**
 * app_rtk_init
 *
 * @param [in]            fwOffset;     low region binary spi address
 * @param [in]              fwSize;     low region binary length
 * @param [in]              chipID;     chip index
 * @return 1 If successful.
 * @return 0 if fail
 *
 * @note
 *  Initiate rtk pd controller when system boot-up. Need to download the image to this device
 */
uint8_t app_rtk_init(uint32_t fwOffset, uint32_t fwSize, uint8_t chipID)
{
	DRV_RTK_OUTPUT_GET_IC_STATUS icStatus;
	uint8_t fwVerMajor = 0, fwVerMinor1 = 0, fwVerMinor2 = 0;
	uint8_t romVerMajor = 0, romVerMinor1 = 0, romVerMinor2 = 0;
	int ret = 0;
	uint8_t inputBuf[RTK_PD_INUPUT_DATA_LENGTH + 1];
	DRV_RTK_OUTPUT_GET_PWR_SW_STATUS pwrSwStatus;
	DRV_RTK_INPUT_CLEAR_DB_STATE clrDb;
	DRV_RTK_INPUT_SET_NOTIFY eventMask;
	bool needUpdate = false, icMatch = false; /* assume IC match for now */
	DRV_RTK_OUTPUT_GET_IC_TYPE icType;
	bool romBoot = false;

	if (chipID > 1) {
		return 0;
	}

	/* check if it need to update FW */
	/* read the FW version from chip */
	ret = amd_crb_drivers_rtk_readIcStatus((chipID << 1), 0x1F, &icStatus);
	if (ret != 0) {
		/* it could be system in rom boot mode (no valid FW at all). It needs to read 20 bytes */
		ret = amd_crb_drivers_rtk_readIcStatus((chipID << 1), 0x14, &icStatus);
		if (ret != 0) {
			LOG_DBG("read the IC Status fail");
			g_pdCtrlSt[chipID].isFwLoadSkipped = false;
			g_pdCtrlSt[chipID].isFwLoadSuccess = false;
			return 0;
		}
		/* if 0x1F read fail and 0x14 read pass. it mean the rom boot*/
		romBoot = true;
	}
	fwVerMajor = icStatus.f.fwMainVer;
	fwVerMinor1 = icStatus.f.fwSubVer1;
	fwVerMinor2 = icStatus.f.fwSubVer2;

	g_pdCtrlSt[chipID].RR = fwVerMinor2;
	g_pdCtrlSt[chipID].MM = fwVerMinor1;
	g_pdCtrlSt[chipID].VVVV = fwVerMajor;
	memcpy(g_pdCtrlSt[chipID].mode, "APP ", 4);
	memcpy(g_pdCtrlSt[chipID].type, "RTKP", 4);
	g_pdCtrlSt[chipID].sku[0] = chipID + 1;

	/* check binary project name */
	if (amd_crb_drivers_sFlashRead(0, fwOffset + RTK_PD_VER_OFFSET +8,
				             16, inputBuf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d",
				fwOffset + RTK_PD_VER_OFFSET + 8, chipID);
		g_pdCtrlSt[chipID].isFwLoadSkipped = false;
		g_pdCtrlSt[chipID].isFwLoadSuccess = false;
		return 0;
	}

	/* read the IC type from chip */
	amd_crb_drivers_rtk_readIcType(chipID << 1, &icType);

	/* compare the binary type */
	if (icType.f.icType == RTKPD_IC_TYPE_RTS5453P) {
		if (memcmp(&inputBuf[4],"PRoJECT_NAME", 12) == 0) {
			icMatch = true;
		}
	} else if (icType.f.icType == RTKPD_IC_TYPE_RTS5453P_VB) {
		if (memcmp(&inputBuf[4],"5453PVB_NAME", 12) == 0) {
			icMatch = true;
		}
	} else if (icType.f.icType == RTKPD_IC_TYPE_RTS5453P_28) {
		if (memcmp(&inputBuf[4],"5453P28_NAME", 12) == 0) {
			icMatch = true;
		}
	}

	LOG_DBG("IsMatch %d, icType: %d", icMatch, icType.f.icType);
	if (romBoot) {
		/* bypass the IcMatch for ROM boot because it deosn't support the ic type check */
		icMatch = true;
		LOG_DBG("Bypass IcMatch check");
	}

	/* read the FW version from ROM */
	if (amd_crb_drivers_sFlashRead(0, fwOffset + RTK_PD_VER_OFFSET,
			             8, inputBuf)) {
		LOG_ERR("!!error!! fail to read SPI at 0x%06X, chipID:%d",
				fwOffset + RTK_PD_VER_OFFSET, chipID);
		g_pdCtrlSt[chipID].isFwLoadSkipped = false;
		g_pdCtrlSt[chipID].isFwLoadSuccess = false;
		return 0;
	}

	romVerMajor = inputBuf[1];
	romVerMinor1 = inputBuf[2];
	romVerMinor2 = inputBuf[3];
	LOG_DBG("RTK chip%d Update FW. FW_VER: %x.%x.%x and ROM_VER: %x.%x.%x", chipID, fwVerMajor, fwVerMinor1, fwVerMinor2,
			                                                                romVerMajor, romVerMinor1, romVerMinor2);
	/* check if need force update */
#ifdef CHECK_FOR_FORCE_PD_UPDATE
	needUpdate = CHECK_FOR_FORCE_PD_UPDATE();
#endif

	/* check if need to update */
	if (icMatch && ((romVerMajor != fwVerMajor) || (romVerMinor1 != fwVerMinor1) || needUpdate)) {
		LOG_DBG("Need to update PD chip%d", chipID);
		ret = app_rtk_firmwareUpdate((chipID << 1), fwOffset, fwSize, &icStatus);
		if (ret != 0) {
			LOG_DBG("update fw fail");
			g_pdCtrlSt[chipID].isFwLoadSkipped = false;
			g_pdCtrlSt[chipID].isFwLoadSuccess = false;
			return 0;
		} else {
			g_pdCtrlSt[chipID].isFwLoadSkipped = false;
			g_pdCtrlSt[chipID].isFwLoadSuccess = true;

			/* update the new FW version after successfully upgarde */
			amd_crb_drivers_rtk_readIcStatus((chipID << 1), 0x1F, &icStatus);
			g_pdCtrlSt[chipID].RR = icStatus.f.fwSubVer2;
			g_pdCtrlSt[chipID].MM = icStatus.f.fwSubVer1;
			g_pdCtrlSt[chipID].VVVV = icStatus.f.fwMainVer;
		}
	} else {
		/* skip update because of version match */
		g_pdCtrlSt[chipID].isFwLoadSkipped = true;
		g_pdCtrlSt[chipID].isFwLoadSuccess = false;
	}

	/* load the pd default setting */
	if (chipID == 0) {
		app_rtk_loadInitialStatus(TYPEC_PORT_0_IDX);
		app_rtk_loadInitialStatus(TYPEC_PORT_1_IDX);
	} else if (chipID == 1) {
		app_rtk_loadInitialStatus(TYPEC_PORT_2_IDX);
	}

	/* check the dead battery status */
	amd_crb_drivers_rtk_readPwrSwState((chipID << 1),&pwrSwStatus);
	/* 0: power on from dead battery
	 * 1: not power on from dead battery */
	if (pwrSwStatus.f.DeadBatteryStatus == 0) {
		dpm_set_status(chipID << 1)->dead_bat = true;
		/* Need to clear the dead battery status */
		clrDb.f.Param = 1; /* 1: clear and 0: nothing */
		amd_crb_drivers_rtk_writeClrDeadBatState((chipID << 1), clrDb);
	} else {
		dpm_set_status(chipID << 1)->dead_bat = false;
	}

	/* set the mask */
	eventMask.f.connectChange = 1;
	eventMask.f.externalSupplyChange = 1;
	eventMask.f.negPwrLevelChange = 1;
	eventMask.f.portPartnerChanged = 1;
	eventMask.f.suppProvdierCapChange = 1;
	eventMask.f.pwrDirchanged = 1;
	eventMask.f.alternateFLowChange = 1;
	eventMask.f.portOpModeChange = 1;

	/* set the notification enable */
	if (chipID == 0) {
		amd_crb_drivers_rtk_ucsiSetNotificationEnable(TYPEC_PORT_0_IDX, &eventMask);
		amd_crb_drivers_rtk_ucsiSetNotificationEnable(TYPEC_PORT_1_IDX, &eventMask);
	} else if (chipID == 1) {
		amd_crb_drivers_rtk_ucsiSetNotificationEnable(TYPEC_PORT_2_IDX, &eventMask);
	}

	return 1;
}