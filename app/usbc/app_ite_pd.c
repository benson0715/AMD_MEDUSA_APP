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
#include "app_ite_pd.h"
#include "board_config.h"
#include "app_ucsi_tunnel.h"
#include "system.h"
#include "app_usbc_task.h"

LOG_MODULE_REGISTER(iteapp, CONFIG_ITE_APP_LOG_LEVEL);

/* ************************** *
 *     static valuable        *
 * ************************** */
static pd_do_t _s_xPort0SPdo = {0};
static pd_do_t _s_xPort1SPdo = {0};
static pd_do_t _s_xPort2SPdo = {0};

/* ************************** *
 *     global valuable        *
 * ************************** */
static pd_do_t _s_ite_xSysRdo;

/************************************
 *     ITE PD interrupt handling     *
 ************************************/

/*
 * app_ite_IntTopHalf
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
void app_ite_IntTopHalf(uint8_t pinSt, uint8_t chipID)
{
	/* PD INT# low active */
	/* If there are two TI PD controllers, either interrupt pins may trigger interrput callback function.
     In the callback function, we will loop all the three or four ports. 
	   So in 4cc layer, we think all the ports in one virtual chip */
	if (!pinSt) {
		_s_EventPending[chipID] = 1;
	}
}

/*
 * app_ite_IntPending
 *
 * @return if there is interrupt pending
 *
 * @note
 *  return if there is pending interrupt which is needed to handle
 */
bool app_ite_intPending(void)
{
	if (_s_EventPending[0] || _s_EventPending[1])
		return true;
	else
		return false;
}

/*
 * app_ite_vonderIdentify
 *
 * @return if it is ite PD module
 *
 * @note
 *  return if this is ite PD module based on the I2C address
 */
bool app_ite_vonderIdentify(void)
{
	uint8_t buf[16];
	int ret;
	bool ret0 = false, ret1 = false;
	uint32_t attempts = 0;
	
	do {
		memset (buf, 0x3F, sizeof(buf));
		ret = amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_FW_VER, buf, ITEPD_REG_UCSI_VRESION_LEN, TYPEC_PORT_0_IDX);
		
		if (ret == 0){
			/* ite chip0 detected */
			ret0 = true;
			break;
		}

		k_sleep(K_MSEC(10)); /* sleep "10" ms */
		attempts++;
	} while (attempts < 10);  /* 100ms delay */
	
	attempts = 0;
#if PD_TRIPPORT_ENABLE
	do {
		memset (buf, 0x3F, sizeof(buf));
		ret = amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_FW_VER, buf, ITEPD_REG_UCSI_VRESION_LEN, TYPEC_PORT_2_IDX);
		
		if (ret == 0){
			/* ite chip1 detected */
			ret1 = true;
			break;
		}

		k_sleep(K_MSEC(10)); /* sleep "10" ms */
		attempts++;
	} while (attempts < 10);  /* 100ms delay */
#else
	ret1 = true;
#endif
	return (ret0 & ret1);
}

/*
 * app_ite_Int_bottomHalf
 * 
 * @param [in]    chipID;    chip id
 * @return void
 *
 * @note
 *  handle the interrupt service
 */
void app_ite_IntBottomHalf(uint8_t chipID)
{
	int ret = 0;
	uint8_t loopCount = 0;
	DEV_ITEX_ALERT_STATUS status0;
	DEV_ITEX_ALERT_STATUS status1;

	if ((!_s_EventPending[chipID]))  
		return;
	
	LOG_DBG("ITEPD_INTERRUPT!!!");

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
			ret = amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_ALERT_STATUS, status0.buf, sizeof(status0.buf), TYPEC_PORT_0_IDX);
			if (ret != 0) {
				LOG_ERR("[!!!ERROR!!!]  fail to read TIPD_REG_INT_EVENT P0");
			} else {
				/* process the event p0 */
				app_ite_usbcInterruptProc(chipID, status0);
			}
		}
		/* chip1, 1. read the interrupt event */
		else if (chipID == 1) {
			ret = amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_ALERT_STATUS, status1.buf, sizeof(status1.buf), TYPEC_PORT_2_IDX);
			if (ret != 0) {
				LOG_ERR("[!!!ERROR!!!]  fail to read TIPD_REG_INT_EVENT P0");
			} else {
				/* process the event p0 */
				app_ite_usbcInterruptProc(chipID, status1);
			}
		}

		/* Exit current loop if PD INT asserted more than three handling loop */
		loopCount ++;
		if (loopCount > ITE_REG_ACCESS_TRTRY_TIME) {
			LOG_ERR("Something cannot be handled by current interrupt loop, tried %d times.", loopCount);
			_s_EventPending[chipID] = 1;
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
 * app_ite_getHigherPortSnkPdo
 * 
 * @param void
 * @return pd_do_t
 * @note
 *  Return the highest Rdo from ite PD.
 */
pd_do_t app_ite_getHigherPortSnkPdo(void)
{
	_s_ite_xSysRdo = _s_xPort0SPdo;
#if PD_DUALPORT_ENABLE
	/* Get the higher RDO from two typec ports */
	if ((_s_xPort0SPdo.fixed_snk.op_current * _s_xPort0SPdo.fixed_snk.voltage) < 
	     _s_xPort1SPdo.fixed_snk.op_current * _s_xPort1SPdo.fixed_snk.voltage) {
		_s_ite_xSysRdo = _s_xPort1SPdo;
	}
#endif
#if PD_TRIPPORT_ENABLE
	if ((_s_ite_xSysRdo.fixed_snk.op_current * _s_ite_xSysRdo.fixed_snk.voltage) <
	    _s_xPort2SPdo.fixed_snk.op_current * _s_xPort2SPdo.fixed_snk.voltage) {
		_s_ite_xSysRdo = _s_xPort2SPdo;
	}
#endif
	
	return _s_ite_xSysRdo;
}


/*
 * app_ite_resetParam
 * 
 * @param [in]    port;    port number
 * @return void
 *
 * @note
 *  reset the ITE PD related parameters
 */
void app_ite_resetParam(uint8_t port)
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
}

/*
 * app_ite_usbcInterruptProc
 *
 * @param [in]    chipID;    pd chip id
 * @param [in]    event;     event which is needs to process
 * @return void
 *
 * @note
 *  Process the usbc interrupt triggered by hardware pin
 */
void app_ite_usbcInterruptProc(uint8_t chipID, DEV_ITEX_ALERT_STATUS event)
{
	int ret;
	DEV_ITEX_VCMD_STATUS1 status;
	
	if (event.f.vendorFunctionAlert) {
		/* UCSI trigger event */
		if (chipID == 0) {
			/* check port0 */
			ret = amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_VCMD_STATUS1_PA, status.buf, sizeof(status.buf), TYPEC_PORT_0_IDX);
			
			if (status.f.portConnected) {
				dpm_set_status(TYPEC_PORT_0_IDX)->attach = true;
				dpm_set_status(TYPEC_PORT_0_IDX)->connect = true;
				LOG_DBG("Attached P0");
			} else {
				dpm_set_status(TYPEC_PORT_0_IDX)->attach = false;
				dpm_set_status(TYPEC_PORT_0_IDX)->connect = false;
				app_ite_resetParam(TYPEC_PORT_0_IDX);
			}
			if (status.f.connectionOrientation)
				dpm_set_status(TYPEC_PORT_0_IDX)->polarity = 1;
			else 
				dpm_set_status(TYPEC_PORT_0_IDX)->polarity = 0;
			if (status.f.powerRole)
				dpm_set_status(TYPEC_PORT_0_IDX)->cur_port_role = PRT_ROLE_SOURCE;
			else 
				dpm_set_status(TYPEC_PORT_0_IDX)->cur_port_role = PRT_ROLE_SINK;
			if (status.f.dataRole)
				dpm_set_status(TYPEC_PORT_0_IDX)->cur_port_type = PRT_TYPE_DFP;
			else 
				dpm_set_status(TYPEC_PORT_0_IDX)->cur_port_type = PRT_TYPE_UFP;
			if (status.f.pdNegotiatedDone)
				dpm_set_status(TYPEC_PORT_0_IDX)->contract_exist = true;
			else 
				dpm_set_status(TYPEC_PORT_0_IDX)->contract_exist = false;
			
			if (status.f.pdNegotiatedDone && (status.f.powerRole == PRT_ROLE_SINK)) {
				_s_xPort0SPdo.val = amd_crb_drivers_itex_readSnkRdo(TYPEC_PORT_0_IDX);
			}
			LOG_DBG("Update: P0 P_Polarity:%d P_Role:%d P_Type:%d",  dpm_set_status(TYPEC_PORT_0_IDX)->polarity,
																	 dpm_set_status(TYPEC_PORT_0_IDX)->cur_port_role,
																	 dpm_set_status(TYPEC_PORT_0_IDX)->cur_port_type);

			/* check port1 */
			ret = amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_VCMD_STATUS1_PB, status.buf, sizeof(status.buf), TYPEC_PORT_1_IDX);
			
			if (status.f.portConnected) {
				dpm_set_status(TYPEC_PORT_1_IDX)->attach = true;
				dpm_set_status(TYPEC_PORT_1_IDX)->connect = true;
				LOG_DBG("Attached P1");
			} else {
				dpm_set_status(TYPEC_PORT_1_IDX)->attach = false;
				dpm_set_status(TYPEC_PORT_1_IDX)->connect = false;
				app_ite_resetParam(TYPEC_PORT_1_IDX);
				LOG_DBG("Unattached P1");
			}
			
			if (status.f.connectionOrientation)
				dpm_set_status(TYPEC_PORT_1_IDX)->polarity = 1;
			else 
				dpm_set_status(TYPEC_PORT_1_IDX)->polarity = 0;
			if (status.f.powerRole)
				dpm_set_status(TYPEC_PORT_1_IDX)->cur_port_role = PRT_ROLE_SOURCE;
			else 
				dpm_set_status(TYPEC_PORT_1_IDX)->cur_port_role = PRT_ROLE_SINK;
			if (status.f.dataRole)
				dpm_set_status(TYPEC_PORT_1_IDX)->cur_port_type = PRT_TYPE_DFP;
			else 
				dpm_set_status(TYPEC_PORT_1_IDX)->cur_port_type = PRT_TYPE_UFP;
			if (status.f.pdNegotiatedDone)
				dpm_set_status(TYPEC_PORT_1_IDX)->contract_exist = true;
			else 
				dpm_set_status(TYPEC_PORT_1_IDX)->contract_exist = false;
			
			if (status.f.pdNegotiatedDone && (status.f.powerRole == PRT_ROLE_SINK)) {
				_s_xPort1SPdo.val = amd_crb_drivers_itex_readSnkRdo(TYPEC_PORT_1_IDX);
			}
			LOG_DBG("Update: P1 P_Polarity:%d P_Role:%d P_Type:%d",  dpm_set_status(TYPEC_PORT_1_IDX)->polarity,
																	 dpm_set_status(TYPEC_PORT_1_IDX)->cur_port_role,
																	 dpm_set_status(TYPEC_PORT_1_IDX)->cur_port_type);
		}
		else if (chipID == 1)
		{
			/* check port2 */
			ret = amd_crb_drivers_itex_regAccess(ITE_REG_READ, ITEPD_REG_VCMD_STATUS1_PA, status.buf, sizeof(status.buf), TYPEC_PORT_2_IDX);
			
			if (status.f.portConnected) {
				dpm_set_status(TYPEC_PORT_2_IDX)->attach = true;
				dpm_set_status(TYPEC_PORT_2_IDX)->connect = true;
				LOG_DBG("Attached P2");
			} else {
				dpm_set_status(TYPEC_PORT_2_IDX)->attach = false;
				dpm_set_status(TYPEC_PORT_2_IDX)->connect = false;
				app_ite_resetParam(TYPEC_PORT_2_IDX);
				LOG_DBG("Unattached P2");
			}
			
			if (status.f.connectionOrientation)
				dpm_set_status(TYPEC_PORT_2_IDX)->polarity = 1;
			else 
				dpm_set_status(TYPEC_PORT_2_IDX)->polarity = 0;
			if (status.f.powerRole)
				dpm_set_status(TYPEC_PORT_2_IDX)->cur_port_role = PRT_ROLE_SOURCE;
			else 
				dpm_set_status(TYPEC_PORT_2_IDX)->cur_port_role = PRT_ROLE_SINK;
			if (status.f.dataRole)
				dpm_set_status(TYPEC_PORT_2_IDX)->cur_port_type = PRT_TYPE_DFP;
			else 
				dpm_set_status(TYPEC_PORT_2_IDX)->cur_port_type = PRT_TYPE_UFP;
			if (status.f.pdNegotiatedDone)
				dpm_set_status(TYPEC_PORT_2_IDX)->contract_exist = true;
			else 
				dpm_set_status(TYPEC_PORT_2_IDX)->contract_exist = false;

			if (status.f.pdNegotiatedDone && (status.f.powerRole == PRT_ROLE_SINK)) {
				_s_xPort2SPdo.val = amd_crb_drivers_itex_readSnkRdo(TYPEC_PORT_2_IDX);
			}			
			LOG_DBG("Update: P2 P_Polarity:%d P_Role:%d P_Type:%d",  dpm_set_status(TYPEC_PORT_2_IDX)->polarity,
																	 dpm_set_status(TYPEC_PORT_2_IDX)->cur_port_role,
																	 dpm_set_status(TYPEC_PORT_2_IDX)->cur_port_type);
		}
	}
	if (event.f.ucsiAlert) {
		app_ucsi_tunnel_intHandler(chipID);
	}
	
	/* clear interrupt */
	if (chipID == 0)
		amd_crb_drivers_itex_clearAlterStatus(TYPEC_PORT_0_IDX, event);
	else if (chipID == 1)
		amd_crb_drivers_itex_clearAlterStatus(TYPEC_PORT_2_IDX, event);
}


/*
 * app_ite_loadInitialStatus
 *
 * @param [in]    port;    port number
 * @return void
 *
 * @note
 *  load the PD initial status after system power on
 */
void app_ite_loadInitialStatus(uint8_t port)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);
	DEV_ITEX_VCMD_STATUS1 status;
	int ret = 0;
	uint8_t reg = 0;
	
	if ((port == TYPEC_PORT_0_IDX) || (port == TYPEC_PORT_2_IDX)) {
		reg = ITEPD_REG_VCMD_STATUS1_PA;
	} else if (port == TYPEC_PORT_1_IDX) {
		reg = ITEPD_REG_VCMD_STATUS1_PB;
	} else {
		LOG_ERR("Invalid ITE Port");
		return;
	}
	
	LOG_DBG("Load initial PD status");
	/* read 8+1 bytes for status */
	ret = amd_crb_drivers_itex_regAccess(ITE_REG_READ, reg, status.buf, sizeof(status.buf), port);
	
	if (ret != 0) {
		LOG_ERR("Read status I2C error");
	} else if (status.f.portConnected) {
		LOG_DBG("Attached %d", port);
		dpm_stat->attach = true;
		dpm_stat->connect = true;

		dpm_stat->connect = true;
		dpm_stat->polarity = status.f.connectionOrientation;
		dpm_stat->cur_port_role = (port_role_t)status.f.powerRole;
		dpm_stat->cur_port_type = (port_type_t)status.f.dataRole;
			
		LOG_DBG("Update: P:%d P_Polarity:%d P_Role:%d P_Type:%d", port,
																	dpm_stat->polarity,
																	dpm_stat->cur_port_role,
																	dpm_stat->cur_port_type);
		
		_s_xPort0SPdo.val = amd_crb_drivers_itex_readSnkRdo(TYPEC_PORT_0_IDX);
#if PD_DUALPORT_ENABLE
		_s_xPort1SPdo.val = amd_crb_drivers_itex_readSnkRdo(TYPEC_PORT_1_IDX);
#endif
#if PD_TRIPPORT_ENABLE
		_s_xPort2SPdo.val = amd_crb_drivers_itex_readSnkRdo(TYPEC_PORT_2_IDX);
#endif
		
	} else {
		LOG_DBG("Unattached %d", port);
		dpm_stat->attach = false;
		app_ite_resetParam(port);
	}
}