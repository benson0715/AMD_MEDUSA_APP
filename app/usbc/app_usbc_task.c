/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#if (CONFIG_USBC_4CC)
#include "app_4cc.h"
#endif
#if (CONFIG_USBC_CCGX)
#include "app_ccgx.h"
#endif
#if (CONFIG_USBC_ITE)
#include "app_ite_pd.h"
#endif
#if (CONFIG_USBC_RTK)
#include "app_realtek_pd.h"
#endif
#include "app_ucsi_tunnel.h"
#include "board_config.h"
#include "app_usbc_task.h"
#include "system.h"
#include "app_pseq.h"
#include "amd_crb_drivers_spiFlash.h"
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
#include "board_pwrSrc.h"
#endif
#include "app_acpi.h"
#include "task_handler.h"
#include "dev_isl9241.h"

LOG_MODULE_REGISTER(usbc_task, CONFIG_USBC_TASK_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#ifndef MD_ACPI_HYPERPLANE_SELECTOR_OFFSET
#define MD_ACPI_HYPERPLANE_SELECTOR_OFFSET 0x31
#endif

/* external variable */
extern bool g_PdAdapterConnected;
extern uint8_t gs_eSpiMmioSpace[CONFIG_ESPI_NPCX_PERIPHERAL_ACPI_SHD_MEM_SIZE];
extern bool g_isUcsiCycleFinished;
#if (CONFIG_USBC_4CC)
#if (!PD_TRIPPORT_ENABLE && PD_DUALPORT_ENABLE)
bool g_pd_ppextVbusSwConfig = false;
static bool _s_pd_ppextVbusSwConfigPrev = false;
extern bool g_pd_autoSinkEnable;
#endif
#endif

/* ************************** *
 *     global valuable        *
 * ************************** */
/* crossbar mailbox */
uint8_t g_usbc_CrossbarMailbox_Inputbuf[8] = {0};
uint8_t g_usbc_CrossbarMailbox_Outputbuf[8] = {0};
uint8_t g_usbc_CrossbarMailbox_Command = 0xFF;
uint8_t g_usbc_CrossbarMailbox_Port = 0x00;

/* device id */
uint8_t g_usbc_devID = 0;
/* system boot up with PD power source */
bool g_usbc_pdBootFlag;
/* usbc status */
APP_USBC_STATUS_REG app_usbc_status;
/* register sem for USBC */
K_SEM_DEFINE(usbcSem, 0, 1);

/* ************************** *
 *     static valuable        *
 * ************************** */
static uint8_t *task_usbcSlpReady;

typedef union {
	struct {
		uint8_t address;
		uint8_t offset;
		uint8_t value;
	} field;
	uint8_t ui8Reg[3];
} APP_USBC_RETIMER_REG;

/* retimer register table */
APP_USBC_RETIMER_REG app_usbc_retimerReg[] =
{
	/* Displayport related settings */
	{.field.address = 0x08, .field.offset = 0x76, .field.value = 0x00},   //DP EQ level register setting P0
	{.field.address = 0x0A, .field.offset = 0x07, .field.value = 0x00},   //DP EQ level register setting P2
	{.field.address = 0x0A, .field.offset = 0x10, .field.value = 0x00},   //DP EQ level register setting P2
	{.field.address = 0x0A, .field.offset = 0x11, .field.value = 0x00},   //DP EQ level register setting P2
	{.field.address = 0x0A, .field.offset = 0x12, .field.value = 0x00},   //DP EQ level register setting P2
	{.field.address = 0x0A, .field.offset = 0x13, .field.value = 0x00},   //DP EQ level register setting P2
	{.field.address = 0x0A, .field.offset = 0x14, .field.value = 0x00},   //DP EQ level register setting P2

	{.field.address = 0x09, .field.offset = 0x10, .field.value = 0x00},   //DPCD_100 P1
	{.field.address = 0x09, .field.offset = 0x11, .field.value = 0x00},   //DPCD_101 P1
	{.field.address = 0x09, .field.offset = 0x12, .field.value = 0x00},   //DPCD_103 P1
	{.field.address = 0x09, .field.offset = 0x13, .field.value = 0x00},   //DPCD_104 P1
	{.field.address = 0x09, .field.offset = 0x14, .field.value = 0x00},   //DPCD_105 P1
	{.field.address = 0x09, .field.offset = 0x15, .field.value = 0x00},   //DPCD_106 P1
	{.field.address = 0x09, .field.offset = 0x1E, .field.value = 0x00},   //DPCD_600 P1

	/* USB related settings */
	{.field.address = 0x08, .field.offset = 0x78, .field.value = 0x00},   //USB AP_TX2/1 EQ level register setting 10G P0
	{.field.address = 0x08, .field.offset = 0x7C, .field.value = 0x00},   //USB AP_TX2/1 EQ level register setting 5G P0
	{.field.address = 0x08, .field.offset = 0x7A, .field.value = 0x00},   //USB CRX2/1 EQ level register setting 10G P0
	{.field.address = 0x08, .field.offset = 0x7E, .field.value = 0x00},   //USB CRX2/1 EQ level register setting 5G P0
	{.field.address = 0x08, .field.offset = 0x73, .field.value = 0x00},   //USB DE & PS setting for Type C TX P0
	{.field.address = 0x08, .field.offset = 0x79, .field.value = 0x00},   //USB CTX2/1 DE register setting 10G P0
	{.field.address = 0x08, .field.offset = 0x7D, .field.value = 0x00},   //USB CTX2/1 DE register setting 5G P0
	{.field.address = 0x09, .field.offset = 0xC9, .field.value = 0x00},   //USB Pre-shoot Setting for AP_RX P1
	{.field.address = 0x09, .field.offset = 0xCA, .field.value = 0x00},   //USB DE & PS Setting for AP_RX P1
	{.field.address = 0x08, .field.offset = 0x7B, .field.value = 0x00},   //APRX2/1 DE register setting 10G P0
	{.field.address = 0x08, .field.offset = 0x7F, .field.value = 0x00},   //APRX2/1 DE register setting 5G P0

	/* USB Status indicating */
	{.field.address = 0x0F, .field.offset = 0x32, .field.value = 0x00},   //USB status on AP_TX to Type C P7
	{.field.address = 0x0F, .field.offset = 0x36, .field.value = 0x00}    //USB status on AP_TX to Type C P7
};

/**
 * app_usbc_writeDevID
 *
 * @param [in]   id;     pd vendor id
 *
 * @note
 *  write the usbc pd vendor id TI/IFX and so on
 */
void app_usbc_writeDevID(uint8_t id)
{
	g_usbc_devID = id;
}

/**
 * app_usbc_readDevID
 *
 * @return usbc pd vendor id
 *
 * @note
 *  return the usbc pd vendor id TI/IFX and so on
 */
uint8_t app_usbc_readDevID(void)
{
	return g_usbc_devID;
}

/**
 * app_usbc_getAcLimitOfHighestPdPort
 *
 * @return highest sink RDO
 *
 * @note
 *  return the highest sink RDO from all three typec ports
 */
uint32_t app_usbc_getAcLimitOfHighestPdPort(void)
{
	if (g_usbc_devID == PD_DEVICE_ID_TIx) {
#if (CONFIG_USBC_4CC)
		return app_4cc_getHigherPortSnkPdo().val;
#endif
	} else if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
#if (CONFIG_USBC_CCGX)
		return amd_crb_drivers_hpi_getHigherPortSnkPdo().val;
#endif
	} else if (g_usbc_devID == PD_DEVICE_ID_RTKx) {
#if (CONFIG_USBC_RTK)
		return app_rtk_getHigherPortSnkPdo().val;
#endif
	}
	return 0;
}

static void app_usbc_debouncerUsbc(void)
{
	/* interval by 30ms */

	/* Rdo status change trigger */
	static pd_do_t _s_lastRdo = {0};

	pd_do_t rdo = { .val = app_usbc_getAcLimitOfHighestPdPort() };
#if CONFIG_POWER_SOURCE_MANAGEMENT
	if (rdo.fixed_snk.op_current && (rdo.fixed_snk.voltage > 240)) { /* source pdo > 12V (240 * 50 mV) */
		g_PdAdapterConnected = true;
	} else {
		g_PdAdapterConnected = false;
	}
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */

	if (_s_lastRdo.val != rdo.val) {
		_s_lastRdo = rdo;
#if CONFIG_POWER_SOURCE_MANAGEMENT
		board_pwrSrcEvent();
#endif
	}
}

/**
 * app_usbc_task_init
 *
 * @return 0 init successfully
 *
 * @note
 *  app usbc task init
 */
int app_usbc_task_init(void)
{
	int ret = 0;

	/* PdSig0 for TI PD */
	if (g_usbc_devID == PD_DEVICE_ID_TIx) {
#if (CONFIG_USBC_4CC)
		LOG_DBG("Initiate TI PD controller");
		/**
		 * Make sure pending interrupt will be cleared and
		 * RDO is updated properly
		 */
		app_4cc_intTopHalf(0);

#if CONFIG_USBC_UCSI_TUNNEL
#if CONFIG_EC_UCSI_ENABLE
		/* Init UCSI by TI controller */
		app_ucsi_tunnel_init(g_usbc_devID, (F_UCSI_MAILBOX *)&gs_eSpiMmioSpace[ACPI_UCSI_BASE]);
#endif /* CONFIG_EC_UCSI_ENABLE */
#endif /* CONFIG_USBC_UCSI_TUNNEL */
#endif /* CONFIG_USBC_4CC */
	}
	else if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
#if (CONFIG_USBC_CCGX)
		LOG_DBG("Initiate IFX PD controller");
		app_ccgx_init();
#if CONFIG_USBC_UCSI_TUNNEL
		/* Init UCSI by Cypress controller */
		app_ucsi_tunnel_init(g_usbc_devID, (F_UCSI_MAILBOX *)&gs_eSpiMmioSpace[ACPI_UCSI_BASE]);
#endif /* CONFIG_USBC_UCSI_TUNNEL */
#endif /* CONFIG_USBC_CCGX */
	}
	else if (g_usbc_devID == PD_DEVICE_ID_ITEx) {
#if (CONFIG_USBC_ITE)
		app_ite_loadInitialStatus(TYPEC_PORT_0_IDX);
		app_ite_loadInitialStatus(TYPEC_PORT_1_IDX);
#if PD_TRIPPORT_ENABLE
		app_ite_loadInitialStatus(TYPEC_PORT_2_IDX);
#endif
#if UCSI_TUNNEL_ENABLE
		/* Init UCSI by Cypress controller */
		app_ucsi_tunnel_init(g_usbc_devID, (F_UCSI_MAILBOX *)&gs_eSpiMmioSpace[ACPI_UCSI_BASE]);
#endif /* UCSI_TUNNEL_ENABLE */
#endif
	}
	else if (g_usbc_devID == PD_DEVICE_ID_RTKx) {
#if (CONFIG_USBC_RTK)
		/* force fake interrupt */
		app_rtk_IntTopHalf(0, 0);
		app_rtk_IntTopHalf(0, 1);

		/* Init UCSI by realtek controller */
		app_ucsi_tunnel_init(g_usbc_devID, (F_UCSI_MAILBOX *)&gs_eSpiMmioSpace[ACPI_UCSI_BASE]);

#endif /* CONFIG_USBC_RTK */
	}

	md_acpiplanes_register_fun(app_usbc_retimer0_Handler, 0xF0);
	md_acpiplanes_register_fun(app_usbc_retimer1_Handler, 0xF1);
	md_acpiplanes_register_fun(app_usbc_CrossbarMailbox_Handler, 0xF2);

	return ret;
}

/**
 * app_usbc_pdLowVoltCtrl
 *
 * @note
 *  If PD power supply is too low in dead battery force system to G3
 */
void app_usbc_pdLowVoltCtrl (void)
{
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
	/* Check pd capability and lock low-cap pd adapter in dead battery condition */
	if (g_usbc_pdBootFlag) {
		/* Initial boot with PD power source */
		if (board_pwrSrc_getCurrentPowerSource() == BOARD_PWR_PD) {
			/* Wait the first interrupt from PD and "board_pwrSrc_powerOnReset" is too early to get solid info */
			if (dpm_get_info(TYPEC_PORT_0_IDX)->attach
#if PD_DUALPORT_ENABLE
			 || dpm_get_info(TYPEC_PORT_1_IDX)->attach
#endif
#if PD_TRIPPORT_ENABLE
			 || dpm_get_info(TYPEC_PORT_2_IDX)->attach
#endif
			   ) {
				pd_do_t rdo = { .val = app_usbc_getAcLimitOfHighestPdPort() };
				if (((dpm_get_info(TYPEC_PORT_0_IDX)->contract_exist == false)
#if PD_DUALPORT_ENABLE
				   && (dpm_get_info(TYPEC_PORT_1_IDX)->contract_exist == false)
#endif
#if PD_TRIPPORT_ENABLE
				   && (dpm_get_info(TYPEC_PORT_2_IDX)->contract_exist == false)
#endif
				)|| (rdo.fixed_src.voltage < 300)) /* voltage below 15V */
				{
					/* At least one port attached and both ports no contract */
					LOG_DBG("Low-cap, lock in G3");
					app_pseq_nextStep(FORCE_G3, 1);
				}
				g_usbc_pdBootFlag = false;
			}
		}
		else
			g_usbc_pdBootFlag = false;
	}
#endif
}

/**
 * app_usbc_giveSem
 *
 * @note
 *  signal usbcSem
 */
void app_usbc_giveSem(void)
{
	/* sleep is not allowed after give sem */
	task_status_set(task_usbcSlpReady, TASK_STASTUS_SLEEP_READY, 0);

	/* Signal USBC task */
	k_sem_give(&usbcSem);
}

/**
 * app_usbc_thread
 *
 * @param [in]                P1;     thread input 1
 * @param [in]                P2;     thread input 2
 * @param [in]                P3;     thread input 3
 *
 * @note
 *  app usbc main thread
 */
void app_usbc_thread(void *p1, void *p2, void *p3)
{
	task_usbcSlpReady = (uint8_t *)p2;

	while(1) {
		/* Not allow to enter sleep */
		task_status_set(task_usbcSlpReady, TASK_STASTUS_SLEEP_READY, 0);
			
		/**It is because of Mayan/Lilac EC cannot access PD in G3 (ALW Power Good not ready).
		 * For new platform should not have this issue. */
		//if (gpio_read_pin(SYSTEM_S5_PG)) {
		if (1) {
#if (CONFIG_USBC_4CC)
			if (g_usbc_devID == PD_DEVICE_ID_TIx) {
				/* Only access PD controller when ALW PG */
				app_4cc_intBottomHalf();
#if (!PD_TRIPPORT_ENABLE && PD_DUALPORT_ENABLE)
				/* scan if there are PD adapters */
				if (g_pd_autoSinkEnable == false) {
					g_pd_ppextVbusSwConfig = app_4cc_ppextVbusSwConfig();
					if (_s_pd_ppextVbusSwConfigPrev != g_pd_ppextVbusSwConfig) {
						/* need to trigger power src event */
						dev_isl9241_setProchot();
						LOG_DBG("Trigger ACLimit update");
					}
					_s_pd_ppextVbusSwConfigPrev = g_pd_ppextVbusSwConfig;
				}
#endif
			}
#endif
#if (CONFIG_USBC_CCGX)
			if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
				/* Only access PD controller when ALW PG */
				app_ccgx_eventHandler();
			}
#endif
#if (CONFIG_USBC_ITE)
			if (g_usbc_devID == PD_DEVICE_ID_ITEx) {
				/* Only access PD controller when ALW PG */
				app_ite_IntBottomHalf(0);
#if PD_TRIPPORT_ENABLE
				app_ite_IntBottomHalf(1);
#endif
			}
#endif
#if (CONFIG_USBC_RTK)
			if (g_usbc_devID == PD_DEVICE_ID_RTKx) {
				/* Only access PD controller when ALW PG */
				app_rtk_IntBottomHalf(0);
#if PD_TRIPPORT_ENABLE
				app_rtk_IntBottomHalf(1);
#endif
			}
#endif

#if CONFIG_USBC_UCSI_TUNNEL
			/* process the ucsi command */
			app_ucsi_tunnel_Ppm2Opm();
			app_ucsi_tunnel_Opm2Ppm();
#if CONFIG_EC_UCSI_ENABLE
			app_ucsi_tunnel_task();
#endif /* CONFIG_EC_UCSI_ENABLE */
#endif /* CONFIG_USBC_UCSI_TUNNEL */

			/* update the usbc power source */
			app_usbc_debouncerUsbc();

			if ((g_isUcsiCycleFinished == false)
#if CONFIG_USBC_CCGX
					|| amd_crb_drivers_hpi_intPending()
#endif
#if (CONFIG_USBC_4CC)
					|| app_4cc_intPending()
#endif
#if (CONFIG_USBC_ITE)
					|| app_ite_intPending()
#endif
#if (CONFIG_USBC_RTK)
					|| app_rtk_intPending()
#endif
				) {

				/* Do nothing to wait UCSI done */
				k_usleep(200);
			} else {
				/* Allow enter sleep */
				task_status_set(task_usbcSlpReady, TASK_STASTUS_SLEEP_READY, 1);

				/**
				* wait sem for available jobs
				*/
				k_sem_take(&usbcSem, K_FOREVER);
			}
		} else {
			/* Allow enter sleep */
			task_status_set(task_usbcSlpReady, TASK_STASTUS_SLEEP_READY, 1);
			/* If alw power good not ready, cannot stuck task in case there is pending event */
			k_msleep(100);
		}
	}
}

/**
 * app_usbc_retimer0_Handler
 *
 * @param [in]                isRead;     1: isRead, 0: isWrite
 * @param [in]                ui8Idx;     ECRAM index
 * @param [in]              pui8Data;     data pointer
 *
 * @return true if successful
 *
 * @note
 *  acpi reitmer0 ECRAM handler
 */
uint8_t app_usbc_retimer0_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t index = 0;
	static uint16_t refrash_cnt = 0;

	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	if (isRead) {
		refrash_cnt++;

		/* Load default */
		if (refrash_cnt > 1000) {
			refrash_cnt = 0;
			for (index = 0; index < (sizeof(app_usbc_retimerReg) / 3); index++) {
				if (g_usbc_devID == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					/* TI Read back re-timer register */
					app_4cc_i2cTunnel(1, app_usbc_retimerReg[index].field.address,
					                   app_usbc_retimerReg[index].field.offset,
					                   1,
					                   &app_usbc_retimerReg[index].field.value, 0);
#endif
				}
				else if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
					/* CY Read back re-timer register */
#if CONFIG_USBC_CCGX
					app_ccgx_i2cTunnel(1, TYPEC_PORT_0_IDX,
					                   app_usbc_retimerReg[index].field.address,
					                   app_usbc_retimerReg[index].field.offset,
					                   1,
					                   &app_usbc_retimerReg[index].field.value, 0);
#endif
				}
			}
		}

		if(ui8Idx < (sizeof(app_usbc_retimerReg) / 3))
			*pui8Data = app_usbc_retimerReg[ui8Idx].field.value;
		else
			*pui8Data = 0xFF;
	}
	else if(ui8Idx < (sizeof(app_usbc_retimerReg) / 3)) {
		if (g_usbc_devID == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
			/* TI Write re-timer register */
			app_4cc_i2cTunnel(0, app_usbc_retimerReg[ui8Idx].field.address,
			                   app_usbc_retimerReg[ui8Idx].field.offset,
			                   1, pui8Data, 0);

			/* TI Read back re-timer register */
			app_4cc_i2cTunnel(1, app_usbc_retimerReg[ui8Idx].field.address,
			                   app_usbc_retimerReg[ui8Idx].field.offset,
			                   1,
			                   &app_usbc_retimerReg[ui8Idx].field.value, 0);
#endif
		}
		else if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
			/* CY Write re-timer register */
			app_ccgx_i2cTunnel(0, TYPEC_PORT_0_IDX,
			                app_usbc_retimerReg[ui8Idx].field.address,
			                app_usbc_retimerReg[ui8Idx].field.offset,
			                1, pui8Data, 0);

			/* CY Read back re-timer register */
			app_ccgx_i2cTunnel(1, TYPEC_PORT_0_IDX,
			                app_usbc_retimerReg[ui8Idx].field.address,
			                app_usbc_retimerReg[ui8Idx].field.offset,
			                1,
			                &app_usbc_retimerReg[ui8Idx].field.value, 0);
#endif
		}
	}

	return 1;
}

/**
 * app_usbc_retimer1_Handler
 *
 * @param [in]                isRead;     1: isRead, 0: isWrite
 * @param [in]                ui8Idx;     ECRAM index
 * @param [in]              pui8Data;     data pointer
 *
 * @return true if successful
 *
 * @note
 *  acpi reitmer1 ECRAM handler
 */
uint8_t app_usbc_retimer1_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t index = 0;
	static uint16_t refrash_cnt = 0;

	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	if (isRead) {
		refrash_cnt++;
		/* Load default */
		if (refrash_cnt > 1000) {
			refrash_cnt = 0;

			for (index = 0; index < (sizeof(app_usbc_retimerReg) / 3); index++) {
				if (g_usbc_devID == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					/* TI Read back re-timer register */
					app_4cc_i2cTunnel(1, app_usbc_retimerReg[index].field.address,
					                   app_usbc_retimerReg[index].field.offset,
					                   1,
					                   &app_usbc_retimerReg[index].field.value, 0);
#endif
				}
				else if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					/* CY Read back re-timer register */
					app_ccgx_i2cTunnel(1, TYPEC_PORT_1_IDX,
					                   app_usbc_retimerReg[index].field.address,
					                   app_usbc_retimerReg[index].field.offset,
					                   1,
					                   &app_usbc_retimerReg[index].field.value, 0);
#endif
				}
			}
		}

		if(ui8Idx < (sizeof(app_usbc_retimerReg) / 3))
			*pui8Data = app_usbc_retimerReg[ui8Idx].field.value;
		else
			*pui8Data = 0xFF;
	}
	else if(ui8Idx < (sizeof(app_usbc_retimerReg) / 3)) {
		if (g_usbc_devID == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
			/* TI Write re-timer register */
			app_4cc_i2cTunnel(0, app_usbc_retimerReg[ui8Idx].field.address,
			                   app_usbc_retimerReg[ui8Idx].field.offset,
			                   1, pui8Data, 0);

			/* TI Read back re-timer register */
			app_4cc_i2cTunnel(1, app_usbc_retimerReg[ui8Idx].field.address,
			                   app_usbc_retimerReg[ui8Idx].field.offset,
			                   1,
			                   &app_usbc_retimerReg[ui8Idx].field.value, 0);
#endif
		}
		else if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
			/* CY Write re-timer register */
			app_ccgx_i2cTunnel(0, TYPEC_PORT_1_IDX,
			                app_usbc_retimerReg[ui8Idx].field.address,
			                app_usbc_retimerReg[ui8Idx].field.offset,
			                1, pui8Data, 0);

			/* CY Read back re-timer register */
			app_ccgx_i2cTunnel(1, TYPEC_PORT_1_IDX,
			                app_usbc_retimerReg[ui8Idx].field.address,
			                app_usbc_retimerReg[ui8Idx].field.offset,
			                1,
			                &app_usbc_retimerReg[ui8Idx].field.value, 0);
#endif
		}
	}

	return 1;
}

/**
 * app_usbc_getRetimerType
 *
 * @note
 *  get the retimer type through the EC to PD I2C tunnel
 */
void app_usbc_getRetimerType(void)
{
	if (g_usbc_devID == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
		app_4cc_i2cTunnel(true, 0x08, 0x93, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_4cc_i2cTunnel(true, 0x08, 0x94, 1, &(g_pdCtrlSt[0].retimer[1]), 0);
		app_4cc_i2cTunnel(true, 0x08, 0x95, 1, &(g_pdCtrlSt[0].retimer[2]), 0);
		app_4cc_i2cTunnel(true, 0x08, 0x96, 1, &(g_pdCtrlSt[0].retimer[3]), 0);
		app_4cc_i2cTunnel(true, 0x08, 0x97, 1, &(g_pdCtrlSt[0].retimer[4]), 0);
		app_4cc_i2cTunnel(true, 0x08, 0x98, 1, &(g_pdCtrlSt[0].retimer[5]), 0);

		app_4cc_i2cTunnel(true, 0x18, 0x93, 1, &(g_pdCtrlSt[1].retimer[0]), 0);
		app_4cc_i2cTunnel(true, 0x18, 0x94, 1, &(g_pdCtrlSt[1].retimer[1]), 0);
		app_4cc_i2cTunnel(true, 0x18, 0x95, 1, &(g_pdCtrlSt[1].retimer[2]), 0);
		app_4cc_i2cTunnel(true, 0x18, 0x96, 1, &(g_pdCtrlSt[1].retimer[3]), 0);
		app_4cc_i2cTunnel(true, 0x18, 0x97, 1, &(g_pdCtrlSt[1].retimer[4]), 0);
		app_4cc_i2cTunnel(true, 0x18, 0x98, 1, &(g_pdCtrlSt[1].retimer[5]), 0);
#endif
	}
	else if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x08, 0x93, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x08, 0x94, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x08, 0x95, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x08, 0x96, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x08, 0x97, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x08, 0x98, 1, &(g_pdCtrlSt[0].retimer[0]), 0);

		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x18, 0x93, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x18, 0x94, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x18, 0x95, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x18, 0x96, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x18, 0x97, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
		app_ccgx_i2cTunnel(true, TYPEC_PORT_0_IDX, 0x18, 0x98, 1, &(g_pdCtrlSt[0].retimer[0]), 0);
#endif
	}
}

/**
 * app_usbc_CrossbarMailbox_Handler
 *
 * @param [in]                isRead;     1: isRead, 0: isWrite
 * @param [in]                ui8Idx;     ECRAM index
 * @param [in]              pui8Data;     data pointer
 *
 * @return true if successful
 *
 * @note
 *  acpi pd crossbar mailbox tunnel ECRAM handler
 */
uint8_t app_usbc_CrossbarMailbox_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	if (g_usbc_devID == PD_DEVICE_ID_TIx) {
		if (isRead) {
			switch (ui8Idx) {
				case 0:
					*pui8Data = g_usbc_CrossbarMailbox_Command;
					break;
				case 1:
					*pui8Data = g_usbc_CrossbarMailbox_Port;
					break;
				case 2:
					*pui8Data = g_usbc_CrossbarMailbox_Inputbuf[0];
					break;
				case 3:
					*pui8Data = g_usbc_CrossbarMailbox_Inputbuf[1];
					break;
				case 4:
					*pui8Data = g_usbc_CrossbarMailbox_Inputbuf[2];
					break;
				case 5:
					*pui8Data = g_usbc_CrossbarMailbox_Inputbuf[3];
					break;
				case 6:
					*pui8Data = g_usbc_CrossbarMailbox_Outputbuf[0];
					break;
				case 7:
					*pui8Data = g_usbc_CrossbarMailbox_Outputbuf[1];
					break;
				case 8:
					*pui8Data = g_usbc_CrossbarMailbox_Outputbuf[2];
					break;
				case 9:
					*pui8Data = g_usbc_CrossbarMailbox_Outputbuf[3];
					break;
				default:
					break;
			}
		}
		else {
			switch (ui8Idx) {
				case 0:
					g_usbc_CrossbarMailbox_Command = *pui8Data;
#if CONFIG_USBC_4CC
					/* Command to write inputbuf data to Crossbar mailbox */
					if (g_usbc_CrossbarMailbox_Command == 0x01) {
						app_4cc_crossbarMailBox(false, g_usbc_CrossbarMailbox_Port, g_usbc_CrossbarMailbox_Inputbuf);
					}
					/* Command to read Crossbar mailbox to output buffer*/
					else if (g_usbc_CrossbarMailbox_Command == 0x02) {
						app_4cc_crossbarMailBox(true, g_usbc_CrossbarMailbox_Port, g_usbc_CrossbarMailbox_Outputbuf);
					}
#endif
					break;
				case 1:
					g_usbc_CrossbarMailbox_Port = *pui8Data;
					break;
				case 2:
					g_usbc_CrossbarMailbox_Inputbuf[0] = *pui8Data;
					break;
				case 3:
					g_usbc_CrossbarMailbox_Inputbuf[1] = *pui8Data;
					break;
				case 4:
					g_usbc_CrossbarMailbox_Inputbuf[2] = *pui8Data;
					break;
				case 5:
					g_usbc_CrossbarMailbox_Inputbuf[3] = *pui8Data;
					break;
				default:
					break;
			}
		}
	}
	else if (g_usbc_devID == PD_DEVICE_ID_CCGx) {
		if (isRead) {
			switch (ui8Idx) {
				case 0:
					*pui8Data = g_usbc_CrossbarMailbox_Command;
					break;
				case 1:
					*pui8Data = g_usbc_CrossbarMailbox_Port;
					break;
				case 2:
					*pui8Data = g_usbc_CrossbarMailbox_Inputbuf[0];
					break;
				case 3:
					*pui8Data = g_usbc_CrossbarMailbox_Inputbuf[1];
					break;
				case 4:
					*pui8Data = g_usbc_CrossbarMailbox_Inputbuf[2];
					break;
				case 5:
					*pui8Data = g_usbc_CrossbarMailbox_Inputbuf[3];
					break;
				case 6:
					*pui8Data = g_usbc_CrossbarMailbox_Outputbuf[0];
					break;
				case 7:
					*pui8Data = g_usbc_CrossbarMailbox_Outputbuf[1];
					break;
				case 8:
					*pui8Data = g_usbc_CrossbarMailbox_Outputbuf[2];
					break;
				case 9:
					*pui8Data = g_usbc_CrossbarMailbox_Outputbuf[3];
					break;
				default:
					break;
			}
		}
		else {
			switch (ui8Idx) {
				case 0:
					g_usbc_CrossbarMailbox_Command = *pui8Data;
					/* Command to write inputbuf data to Crossbar mailbox */
					if (g_usbc_CrossbarMailbox_Command == 0x01) {

					}
					/* Command to read Crossbar mailbox to output buffer*/
					else if (g_usbc_CrossbarMailbox_Command == 0x02) {

					}
					break;
				case 1:
					g_usbc_CrossbarMailbox_Port = *pui8Data;
					break;
				case 2:
					g_usbc_CrossbarMailbox_Inputbuf[0] = *pui8Data;
					break;
				case 3:
					g_usbc_CrossbarMailbox_Inputbuf[1] = *pui8Data;
					break;
				case 4:
					g_usbc_CrossbarMailbox_Inputbuf[2] = *pui8Data;
					break;
				case 5:
					g_usbc_CrossbarMailbox_Inputbuf[3] = *pui8Data;
					break;
				default:
					break;
			}
		}
	}

	return 1;
}