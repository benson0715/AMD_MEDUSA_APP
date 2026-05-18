/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include "app_sleep.h"
#include "app_pseq.h"
#if (CONFIG_BATTERY_MANAGEMENT)
#include "board_smtbty.h"
#endif
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
#include "board_pwrSrc.h"
#endif
#include "f_nv_options.h"
#include "board_sleep.h"
#include "board_config.h"
#include "task_handler.h"
#include <zephyr/arch/arm/aarch32/cortex_m/cmsis.h>
#include <zephyr/logging/log.h>
#include <soc.h>
#include <zephyr/pm/pm.h>
#if CONFIG_PSL
#include "psl.h"
#endif
LOG_MODULE_REGISTER(sleep, CONFIG_SLEEP_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#define PWRSAVER_WAIT_CYCLE               (800)
#define PSL_WAIT_CYCLE               	  (600)

/* ************************** *
 *     static valuable        *
 * ************************** */
static unsigned int _s_sleep_wait_count = 0;
#if (!STANDBY_WITH_G3_POWER) 
#if CONFIG_PSL
static unsigned int _s_psl_wait_count = 0;
#endif
#endif
/* ************************** *
 *     global valuable        *
 * ************************** */
extern int OSIdleCtr;
bool _s_rtcWake = false;
uint8_t _s_rtcCycle = 0;

/**
 * @brief sleep power saving mode
 */
void app_sleep_PwrSaver(void)
{
	uint16_t slpDurtion_x1s = 61;

#if CONFIG_POWER_SOURCE_MANAGEMENT
		/* Sleep 3s if has AC but DC */
		if (board_pwrSrc_hasAcPowerSource()) {
			slpDurtion_x1s = 3;
		}
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */
		
		__disable_irq();
		board_slp_Sleep(SLP_SYSTEM_HEAVY_SLEEP, slpDurtion_x1s);
		({
			__asm__ volatile(
				"push {r0-r5}\n"				  /* Save the registers used for delay */
				"wfi\n"							  /* Enter sleep mode after receiving wfi */
				"ldm %0, {r0-r5}\n"				  /* Add a delay before instructions fetching */
				"pop {r0-r5}\n"					  /* Restore the registers used for delay */
				"isb\n"							  /* Flush the cpu pipelines */
				::"r"(CONFIG_SRAM_BASE_ADDRESS)); /* A valid addr used for delay */
		});
		static struct itim32_reg *const evt_tmr = (struct itim32_reg *)
					DT_REG_ADDR_BY_NAME(DT_NODELABEL(itims), evt_itim);
		if (evt_tmr->ITCTS32 & BIT(NPCX_ITCTSXX_TO_STS)) {
			_s_rtcWake = true;
			_s_rtcCycle++;
			if (_s_rtcCycle == 300) {
				_s_rtcWake = false;
				_s_rtcCycle = 0;
			}
		}
		board_slp_Wakeup();
		__NOP(); __NOP();
		__enable_irq();
}


/**
 * @brief task wait for interrupt
 */
void task_wfi(void)
{
		if (all_tasks_sleep_ready()) {
			_s_sleep_wait_count++;  /* check again to ignore ready to false in 6s */
			if (_s_sleep_wait_count>600) {
				__WFI();
				_s_sleep_wait_count = 0;
			} 
		} else {
			LOG_DBG("NOT READY");
			_s_sleep_wait_count = 0;
		}
}

/**
 * @brief task wait for sleep
 */
void task_wait_for_sleep(void)
{
		if((_s_rtcWake == true) && all_tasks_sleep_ready()) {
			_s_rtcWake = false;
			app_sleep_PwrSaver();
		} else if (all_tasks_sleep_ready()) {
			_s_sleep_wait_count ++;  /* check again to ignore ready to false in 6s */
			if (_s_sleep_wait_count > PWRSAVER_WAIT_CYCLE) {
				app_sleep_PwrSaver();
				_s_sleep_wait_count = 0;
			}
			k_msleep(10);
		} else {
			LOG_DBG("NOT READY");
			_s_sleep_wait_count = 0;
		}
}

/**
 * @brief idle thread with lowest priority and can enter only when high priority tasks are all idle
 * @param p1 unused parameter
 * @param p2 unused parameter
 * @param p3 unused parameter
 */
void idle_thread(void *p1, void *p2, void *p3)
{
	k_msleep(2000); //wait for sys init ready

	while(1) {
		enum system_power_state st = app_pseq_getPwrStatus();
		if (SYSTEM_S3_STATE != st && SYSTEM_S5_STATE != st && SYSTEM_G3_STATE != st) {
			__disable_irq();                      /* disable global interrupt */
			EC_DEBUG_LED(0);                      /* turn off DBG_LED */
			OSIdleCtr++;
			__NOP();
			__DSB();    						  /* Data Synchronization Barrier */
			__WFI();    						  /* Wait for interrupt */
			__ISB();    						  /* Instruction Synchronization Barrier */
			__NOP();
			EC_DEBUG_LED(1);                      /* turn on DBG_LED */
			__NOP();
			__NOP();
			__NOP();
			__NOP();
			__NOP();
			__NOP();
			__enable_irq();
		} else if (SYSTEM_G3_STATE == app_pseq_systemState()){ 
#if CONFIG_PSL
#if (!STANDBY_WITH_G3_POWER)  // PSL_OUT low may not power off EC_ALW power rail in S5 case.
			/* PSL_OUT low, so turn off EC VTR power rail */
			uint8_t AcDcSwitchEnableFlag = 0;
			GET_NV_OPTIONS(chg_AcDcSwitch, AcDcSwitchEnableFlag);

#ifdef ex_PSL_MODE_EN
			/* PSL_MODE_EN: high can enter; low cannot enter */
			uint8_t PSL_Status = ioexp_get(ex_PSL_MODE_EN);
			if (all_tasks_sleep_ready() && !AcDcSwitchEnableFlag && PSL_Status) {
#else
			if (all_tasks_sleep_ready() && !AcDcSwitchEnableFlag) {
#endif
				_s_psl_wait_count++;
				if (_s_psl_wait_count > PSL_WAIT_CYCLE) {
					LOG_WRN("ENTER PSL MODE!!!");
					psl_out_inactive();
					_s_psl_wait_count = 0;
				}
				k_msleep(10);
			}
#endif
#endif
		} else { /*  Sleep only enabeld for S3/S5/G3 */
			#ifndef CONFIG_SHELL
			task_wait_for_sleep();
			#endif
		}
	}
}