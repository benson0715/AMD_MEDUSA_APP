/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <errno.h>
#include <assert.h>
#include <zephyr/logging/log.h>
#include "app_thermal.h"
#include "app_pseq.h"
#include "f_nv_options.h"
#include "fan.h"
#include "task_handler.h"
#include "board_config.h"
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
#include "board_pwrSrc.h"
#endif
#include "dev_sbtsi.h"
#include "app_acpi.h"
#include "i2c_hub.h"
#include "dev_tmp432.h"
#include "board_thermal.h"
#include "espi_hub.h"
#include "task_handler.h"
#include "app_pseq.h"
#include "app_AcDcSwitch.h"

LOG_MODULE_REGISTER(thermal, CONFIG_THERMAL_MGMT_LOG_LEVEL);

uint8_t *task_thermalSlpReady;
static uint32_t _u32AppliedP3tLimit = 0;
static uint32_t _u32RequiredP3tLimit = 0;

static struct k_work thermal_work;

/* ************************** *
 *     global valuable        *
 * ************************** */
uint8_t g_p3tConfigLimit = 0xFF;

uint32_t g_u8apmlDelayCnt = 0;

/**
 * @brief apply the P3T budget based on current status
 */
static void _applyP3tSetting (void)
{
	/*
	 * do nothing in pre-boot phase
	 */
	if (ACPI_isHostNotifyDisable())
		return;

	uint8_t p3tLimitEn_DC, p3tLimitEn_AC;
	GET_NV_OPTIONS(thm_p3tLimitEn_DC, p3tLimitEn_DC);
	GET_NV_OPTIONS(thm_p3tLimitEn_AC, p3tLimitEn_AC);

	/*
	 * never use P3T command if options is off
	 */
	if (!p3tLimitEn_DC && !p3tLimitEn_AC) {
		return;
	}

	/*
	 * Apply P3T limit in S0 after PMFW ready
	 */
	if (_u32RequiredP3tLimit > SYS_CONFIG_MAX_P3T_LIMIT) {
		LOG_DBG("[WARNING] Required P3T Limit %d > Max. %d, fix it at MAX.", _u32RequiredP3tLimit, SYS_CONFIG_MAX_P3T_LIMIT);
		_u32RequiredP3tLimit = SYS_CONFIG_MAX_P3T_LIMIT;
	}
	if (_u32AppliedP3tLimit != _u32RequiredP3tLimit) {
		uint8_t retry = 2;
		uint32_t retVal;

		while (retry) {
			if (!espihub_socInLP()) {
				retVal = dev_sbi_writeP3tLimit(SBRMI_SLV_ADDRESS_PKG0,
							       _u32RequiredP3tLimit);
				if (retVal == _u32RequiredP3tLimit) {
					LOG_DBG("Set P3T limit to %d (%08X) success.", retVal,
						retVal);
					_u32AppliedP3tLimit = retVal;
					break;
				}
			}
			retry--;
		}
	}
}

/**
 * @brief update the P3T budget based on current status
 */
void app_thermal_updateP3tSetting(uint32_t p3tLimit)
{
	if (0xFFFFFFFF != p3tLimit) {
		_u32RequiredP3tLimit = p3tLimit;
		k_work_submit(&thermal_work);
	} else if (0xFF != g_p3tConfigLimit) {
		_u32RequiredP3tLimit = 1000ul * g_p3tConfigLimit;
		k_work_submit(&thermal_work);
	}
}

/**
 * @brief refresh the P3T budget based on current status
 */
void app_thermal_refreshP3tSetting(void)
{
	k_work_submit(&thermal_work);
}

/**
 * @brief reset the P3T to default value
 */
void app_thermal_resetP3tSetting (void)
{
	_u32AppliedP3tLimit = 0;
	_u32RequiredP3tLimit = 0;
}

/**
 * @brief event callback for the P3T events
 */
static void _THL_eventCallback (struct k_work *w)
{
	_applyP3tSetting();
}

#if CONFIG_HOT_BAG_EN

/**
 * @brief check if lid closed and DC mode
 */
static bool app_thermal_hotBagCheck(void)
{
	extern bool g_lidCloseEnabled;
	extern bool g_hotBagEnabled;

	if (!g_hotBagEnabled) {
		return false;
	}

	if (!g_lidCloseEnabled) {
		return false;
	}

	if (board_pwrSrc_hasAcPowerSource()) {
		return false;
	}

	if (g_LoadfakeBatteryData) {
		return false;
	}

	return true;
}

/**
 * @brief monitor the thermal status and trigger the force shut down if needed
 */
uint16_t highestSkinTemp = 0;
static void app_thermal_hotBagMonitorTemp(board_thermal_info info)
{
	uint16_t overTempCnt = 0;

	if (!app_thermal_hotBagCheck()) {
		return;
	}

	/* get the highest skin temp */
	highestSkinTemp = info._s_u16pcbTmp;
	if (highestSkinTemp < info._s_u16pcbTmpQ[0])
		highestSkinTemp = info._s_u16pcbTmpQ[0];
	if (highestSkinTemp < info._s_u16pcbTmpQ[1])
		highestSkinTemp = info._s_u16pcbTmpQ[1];

	/* If skin temp above 75 degrees */
	if (highestSkinTemp > 75) {
		overTempCnt++; /* thermal task interval is 800ms */
	} else {
		overTempCnt = 0;
	}

	/* trigger OTP if over temp more than 60s */
	if (overTempCnt > 75) {
		overTempCnt = 0;
		highestSkinTemp = 0;

		LOG_ERR("Hot bag detected, force G3 from hot bag thermal monitor");
		app_pseq_nextStep(FORCE_G3, 1); /* need to force G3 */
	}
	extern bool g_powerSeq_criticalZone;
	extern struct k_timer g_pmfHeartBeatTimer;
	if ((g_powerSeq_criticalZone == true) && (highestSkinTemp > 50) &&
	    (k_timer_status_get(&g_pmfHeartBeatTimer) > 0)) {
		/* Timer has expired at least once */
		app_pseq_nextStep(FORCE_G3, 1); /* need to force G3 */
	}
}

/**
 * @brief monitor the battery status and start the polling timer if needed
 */
static void app_thermal_hotBagMonitorBat(void)
{
	extern bool g_powerSeq_criticalZone;
	static bool is_polling_started = false;

	if (!app_thermal_hotBagCheck()) {
		ACPI_pmfHeartBeatStop();
		is_polling_started = false;
		return;
	}

	/* add the critrical check
	 * S0 -> disable ACPI notification -> critrical zone -> Sx
	 * S0i3 -> critrical zone -> enable ACPI notification
	 */
	if (ACPI_isHostNotifyDisable() && !g_powerSeq_criticalZone) {
		ACPI_pmfHeartBeatStop();
		is_polling_started = false;
		return;
	}

	if (!is_polling_started) {
		ACPI_pmfHeartBeatRefresh();
		is_polling_started = true;
	}
}
#endif

/**
 * @brief thermal init
 *
 * @retval True if successful.
 */
bool app_thermal_init( void )
{
	k_work_init(&thermal_work, _THL_eventCallback);

	md_acpiplanes_register_fun(board_thermal_AcpiHandler, 0xB1);

	return true;
}

/**
 * @brief Routine that handles thermal application.
 *
 * thermalmgmt_thread must run after system enter S0 and APU boot up SMU ready
 * happen.
 *
 * @param p1 thread run period.
 * @param p2 thread sleep ready.
 * @param p3 reserved.
 */
void thermalmgmt_thread(void *p1, void *p2, void *p3)
{
	uint32_t period = *(uint32_t *)p1;
	task_thermalSlpReady = (uint8_t*) p2;
	static enum system_power_state lastPwrSt = SYSTEM_INIT_STATE;
	enum system_power_state pwrSt;
	board_thermal_info Borad_Thermal_Info;
	app_thermal_init();
	while (1) {
		k_msleep(period);
		task_status_set(task_thermalSlpReady, TASK_STASTUS_SLEEP_READY, 0);
		pwrSt = app_pseq_systemState();

		if (lastPwrSt != pwrSt) {
			if (SYSTEM_S0_STATE == pwrSt) {
				/* Set every time back to S0  */
				board_thermal_setResistanceCorrection(false);
			}

			lastPwrSt = pwrSt;
		}
		/*
		 * FIXME/TODO: EC should stop sensor polling while SoC in Z-Status.
		 * EC should resume the polling no sooner than 5ms after exit from low power state.
		 */
		if (SYSTEM_S0_STATE == pwrSt) {
			if (g_u8apmlDelayCnt < APML_MB_DELAY_MS / period) {
				g_u8apmlDelayCnt ++;
			} else {
				k_work_submit(&thermal_work);
				board_thermal_getinfo(&Borad_Thermal_Info);
				board_thermal_reporter(Borad_Thermal_Info);
#if CONFIG_HOT_BAG_EN
				app_thermal_hotBagMonitorTemp(Borad_Thermal_Info);
#endif
			}
		} else {
			g_u8apmlDelayCnt = 0;
		}

#if CONFIG_HOT_BAG_EN
		app_thermal_hotBagMonitorBat();
#endif
		task_status_set(task_thermalSlpReady, TASK_STASTUS_SLEEP_READY, 1);
	}
}