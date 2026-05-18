/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <assert.h>
#include "board_config.h"
#include "app_pseq.h"
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
#include "board_pwrSrc.h"
#endif
#include "f_nv_options.h"
#include "app_acpi.h"
#include "app_manOs.h"

LOG_MODULE_REGISTER(manos, CONFIG_WIRELESS_MANOS_LOG_LEVEL);

#ifndef LOG_MANOS
#define LOG_MANOS   1
#endif

#ifndef DBG_MANOS
#define DBG_MANOS   1
#endif

#if LOG_MANOS
#define LOGMOS(frmt, ...)        LOG_INF(frmt, ##__VA_ARGS__)
#else
#define LOGMOS(frmt, ...)
#endif

#if LOG_MANOS_TRACE
#define _MOS_TRACE(x)            LOG_INF("%s: %d", __FUNCTION__, x)
#else
#define _MOS_TRACE(x)
#endif

#define MOS_MPM_EVENT_DEBOUNCE_MAX   18  /* times 500ms */

/**************************** *
 *      Forward declaration   *
 * ************************** */
uint8_t app_manOs_AcpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

/**************************** *
 *       Module variables     *
 * ************************** */
struct k_timer _s_tidS5;
struct k_timer _s_tidBty;
struct k_timer _s_tidMpmEvtDbuc;

/* used for re-establishing MpmEvent signal */
static uint32_t _s_mpmEvtDbcCnt = 0;

extern unsigned char mpm_pinout_act;


/**************************** *
 *        Inner Functions     *
 * ************************** */

/**
 * @brief assert the MPM_EVENT pin
 */
static void _manOs_assertMpmEvent (void)
{
	gpio_write_pin(MPM_EVENT_N, 0);
	mpm_pinout_act = 0;
	LOGMOS("assert MPM_EVENT#");
}

/**
 * @brief de-assert the MPM_EVENT pin
 */
static void _manOs_deassertMpmEvent (void)
{
	gpio_write_pin(MPM_EVENT_N, 1);
	mpm_pinout_act = 1;
	LOGMOS("de-assert MPM_EVENT#");
}

/**
 * @brief sample the MPM_EVENT pin status
 *
 * @retval MPM_EVENT status
 */
static uint32_t _manOs_sampleMpmEvent (void)
{
	uint8_t i;

	_manOs_deassertMpmEvent();

	for (i = 0; i < 0x30; i++) {
		if (gpio_read_pin(MPM_EVENT_N))
			if (gpio_read_pin(MPM_EVENT_N)) {
				LOGMOS("sample MPM_EVENT# get 1 (loop %d)", i);
				_MOS_TRACE(0);
				return 1;
			}
	}

	if (gpio_read_pin(MPM_EVENT_N)) {
		LOGMOS("sample MPM_EVENT# get 1 (loop %d)", i);
		_MOS_TRACE(1);
		return 1;
	} else {
		LOGMOS("sample MPM_EVENT# get 0 (loop %d)", i);
		_MOS_TRACE(2);
		return 0;
	}
}

/**
 * @brief start the timer to debounce the MPM_EVENT status
 */
static void _manOs_restartMpmEventDebounce (void)
{
	_s_mpmEvtDbcCnt = 0;
	k_timer_start(&_s_tidMpmEvtDbuc, K_MSEC(500), K_MSEC(500));

	LOGMOS("(Re)start MPM_EVENT# debounce ...");
}

/**
 * @brief stop the timer to denounce the MPM_EVENT status
 */
static void _manOs_stopMpmEventDebounce (void)
{
	_s_mpmEvtDbcCnt = 0;
	k_timer_stop(&_s_tidMpmEvtDbuc);

	LOGMOS("Stop MPM_EVENT# debounce ...");
}

/**
 * @brief manOS hook in S5 enter
 */
void app_manOs_EnterS5Hook (void)
{
	if (app_manOs_getEnFlag()) {
		if (_manOs_sampleMpmEvent()) {  // sample the pin also leave it as de-asserted.
			LOGMOS("Enter S5 hook, MPM_EVENT# is 1, enable mOS timer");
			k_timer_start(&_s_tidS5, K_MSEC(6000), K_NO_WAIT);
			_MOS_TRACE(3);
		} else {
			LOGMOS("Enter S5 hook, MPM_EVENT# is 0, do nothing");
			_MOS_TRACE(4);
		}
	} else {
		_MOS_TRACE(5);
	}
}

/**
 * @brief manOS hook in S5 exit
 */
void app_manOs_ExitS5Hook (void)
{
	if (app_manOs_getEnFlag()) {
		k_timer_stop(&_s_tidS5);
		LOGMOS("Exit S5 hook, disable mOS timer");
	}

	_MOS_TRACE(6);
}

/**
 * @brief manOS hook in S3 enter
 */
void app_manOs_EnterS3Hook (void)
{
	_MOS_TRACE(7);
}

/**
 * @brief manOS hook in S0 enter
 */
void app_manOs_EnterS0Hook (void)
{
	/* If the boot up is not for MPM_EVNET# */
	if (app_manOs_getEnFlag()) {
		if (gpio_was_mpm_set(MPM_EVENT_N)) {
			_manOs_restartMpmEventDebounce();
			LOGMOS("Enter S0 hook, MPM_EVENT# was set to 1, restart MPM_EVENT# debounce");
		} else {
			LOGMOS("Enter S0 hook, MPM_EVENT# was set to 0, do nothing");
		}
	}

	_MOS_TRACE(8);
}

/**
 * @brief manOS hook in S0 exit
 */
void app_manOs_ExitS0Hook (void)
{
	_MOS_TRACE(9);
}

/**
 * @brief manOS hook in power button handler
 */
void app_manOs_PwrBtnHook (void)
{
	if (!app_manOs_getEnFlag()) {
		return;
	}

	enum system_power_state st = app_pseq_systemState();
	switch (st) {
		default:
#if (LOG_PRINT_ENUM_STRING)
			LOGMOS("PwrBtn press hook in system state %s, deassert MPM_EVENT#", EM_PWR_STATE_2_str(st));
#else
			LOGMOS("PwrBtn press hook in system state %d, deassert MPM_EVENT#", (uint8_t)(st));
#endif
			_manOs_stopMpmEventDebounce();
			_manOs_deassertMpmEvent();
			_MOS_TRACE(0x0A);
			break;
	}
}

/**
 * @brief System shutdown from S0 to S5, the hooked sequence as below
 *    1) 000ms   app_manOs_ExitS0Hook()
 *    2) +95us   app_manOs_EnterS5Hook()
 *    3) +15ms   app_manOs_ApuRstHook()
 */
void app_manOs_ApuRstHook (void)
{
	uint32_t u32MpmEventSt = 0xFFFFFFFF;
	uint32_t need2keepWlanPwr;

	if (app_manOs_getEnFlag()) {
		LOGMOS("ApuRst hook, MPM_EVENT# St = %d, wasSet = %d", gpio_read_pin(MPM_EVENT_N), gpio_was_mpm_set(MPM_EVENT_N));
		_MOS_TRACE(0x0B);

		u32MpmEventSt = _manOs_sampleMpmEvent(); // sample the pin also leave it as de-asserted.
		LOGMOS("ApuRst hook, de-assert MPM_EVENT# on APU_RST, MPM_EVENT# from SoC is %d", u32MpmEventSt);

		/* No MPM_EVENT# re-establish on APU_RST# event */
		_manOs_stopMpmEventDebounce();
	}

	if (!u32MpmEventSt) {
		return; /* if MPM_EVENT# is asserted by SoC, skip the following */
	}

	/**
	 * Override WLAN power in below cases
	 *   - a) manOS is not enabled
	 *   - b) manOS is enabled but MPM_EVENT# is not asserted by SoC
	 *
	 * Turn off WLAN power if
	 *   - "Sleep Type is S4" and "Option pwr_keepWlanPwrInS4 is 0"
	 */

	/* +++ add back for PLAT-81801 (20210427) +++ */
	GET_NV_OPTIONS(pwr_keepWlanPwrInS4, need2keepWlanPwr);
	/* g_slyType = 2 stands for sleep S4, required by [CZN] PLAT-64301 */
	if (!(2 == g_slyType && need2keepWlanPwr)) {
		// ioexp_set(ex_WLAN_PWREN, 0);          /* IO_12 (Keep if S4 && pwr_keepWlanPwrInS4) */
	}
	/* --- add back for PLAT-81801 (20210427) --- */
}

/**
 * @brief manOS hook in power source change
 */
void app_manOs_PwrSourceChange(void)
{
#if CONFIG_POWER_SOURCE_MANAGEMENT
	if (app_manOs_getEnFlag()) {
		if (BOARD_PWR_BTY == board_pwrSrc_getCurrentPowerSource()) {
			k_timer_start(&_s_tidBty, K_MSEC(2500), K_NO_WAIT);
		}
	}
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */
}

/**
 * @brief manOS auto system power on timer callback
 *
 * @param timer Pointer to the timer structure
 */
static void _manOs_timer_autoPwrOn (struct k_timer *timer)
{
	if (SYSTEM_S5_STATE == app_pseq_systemState() && _manOs_sampleMpmEvent()) {
		LOGMOS("manOs autoPwrOn timer fired -- do mOS boot");
		_MOS_TRACE(0x0D);
		_manOs_assertMpmEvent();
		app_pseq_nextStep(FROM_S5S3_TO_S0, 1);
	}
}

/**
 * @brief manOS check power source timer callback
 *
 * @param timer Pointer to the timer structure
 */
static void _manOs_timer_checkPwrSrc (struct k_timer *timer)
{
	LOGMOS("PwrSrc hook - in timer cb, MPM_EVENT# St = %d, wasSet = %d", gpio_read_pin(MPM_EVENT_N), gpio_was_mpm_set(MPM_EVENT_N));
	_MOS_TRACE(0x0E);

#if CONFIG_POWER_SOURCE_MANAGEMENT
	if (BOARD_PWR_BTY == board_pwrSrc_getCurrentPowerSource()) {
		_MOS_TRACE(0x0F);
		LOGMOS("PwrSrc hook - in timer cb, de-assert MPM_EVENT# on Battery");
		_manOs_stopMpmEventDebounce();
		_manOs_deassertMpmEvent();
	}
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */
}

/**
 * @brief manOS setup MPM_EVENT debounce timer callback
 *
 * @param timer Pointer to the timer structure
 */
static void _manOs_timer_setupMpmEventDebounce (struct k_timer *timer)
{
	static uint8_t pinSt = 0;

	if (!SLP_S5_ASSERT && !SLP_S3_ASSERT &&       // SoC in S0, and
		gpio_was_mpm_set(MPM_EVENT_N) && !gpio_read_pin(MPM_EVENT_N) ) { // MPM_EVENT# is asserted by SoC
		if (pinSt) {                                              // status changed
			pinSt = 0;
			_s_mpmEvtDbcCnt = 0;
		} if (_s_mpmEvtDbcCnt >= MOS_MPM_EVENT_DEBOUNCE_MAX) {    // debounce done
			goto _debounce_done;
		} else {
			_s_mpmEvtDbcCnt ++;
		}
	} else {
		if (!pinSt) {                                             // status changed
			pinSt = 1;
			_s_mpmEvtDbcCnt = 0;
		} if (_s_mpmEvtDbcCnt >= MOS_MPM_EVENT_DEBOUNCE_MAX) {    // debounce done
			goto _debounce_done;
		} else {
			_s_mpmEvtDbcCnt ++;
		}
	}

	LOGMOS("MPM_EVENT# debounce Cnt# %2d - dbceSt = %d, GPIO St = %d, wasSet = %d", _s_mpmEvtDbcCnt, pinSt, gpio_read_pin(MPM_EVENT_N), gpio_was_mpm_set(MPM_EVENT_N));

	return;

_debounce_done:
	_manOs_stopMpmEventDebounce();
	if (!pinSt && !SLP_S5_ASSERT && !SLP_S3_ASSERT) {
		_manOs_assertMpmEvent();
		LOGMOS("MPM_EVENT# debounce finished -- Assert MPM_EVENT# again!");
		_MOS_TRACE(0x11);
	} else {
		LOGMOS("MPM_EVENT# debounce finished -- do nothing");
		_MOS_TRACE(0x12);
	}
}

/**
 * @brief manOs get enable flag
 *
 * @retval enable flag
 */
bool app_manOs_getEnFlag(void)
{
	uint32_t tmp;

	GET_NV_OPTIONS(f_wirelessManagability, tmp);

	return !!tmp;
}

/**
 * @brief manOs set enable flag
 *
 * @param isEn True is enable
 */
void app_manOs_setEnFlag(bool isEn)
{
	SET_NV_OPTIONS(f_wirelessManagability, (!!isEn));
	gpio_set_power(MPM_EVENT_N, isEn ? GPIO_CTRL_PWRG_VTR_IO : GPIO_CTRL_PWRG_OFF);
}

/**
 * @brief manOs init
 */
void app_manOs_init (void)
{
	LOGMOS("%s: Entry", __FUNCTION__);

	k_timer_init(&_s_tidS5, _manOs_timer_autoPwrOn, NULL);
	k_timer_init(&_s_tidBty, _manOs_timer_checkPwrSrc, NULL);
	k_timer_init(&_s_tidMpmEvtDbuc, _manOs_timer_setupMpmEventDebounce, NULL);

#if (DBG_MANOS)
	md_acpiplanes_register_fun(app_manOs_AcpiHandler, MANOS_EC_PLANE_ID);
#endif

	uint32_t isEn;
	GET_NV_OPTIONS(f_wirelessManagability, isEn);
	gpio_set_power(MPM_EVENT_N, isEn ? GPIO_CTRL_PWRG_VTR_IO : GPIO_CTRL_PWRG_OFF);
	LOGMOS("f_wirelessManagability = %d", isEn);

	LOGMOS("%s: Exit", __FUNCTION__);
}

/**
 * @brief manOs ACPI handler
 *        0x00 - Target OS; 0x01 - manOS; others - host OS
 *
 * @param isRead Read flag (1 for read, 0 for write)
 * @param ui8Idx Index of the ACPI register
 * @param pui8Data Pointer to data buffer
 *
 * @retval 1 on success, 0 otherwise
 */
#if (DBG_MANOS)
uint8_t app_manOs_AcpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{

	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	uint32_t tmp;

	if (isRead) {
		switch (ui8Idx) {
			case 0x10:  *pui8Data = 'P'; break;
			case 0x11:  *pui8Data = 'W'; break;
			case 0x12:  *pui8Data = 'R'; break;
			case 0x13:  *pui8Data = ':'; break;
			case 0x14:
				switch(gpio_get_power(MPM_EVENT_N)) {
					case GPIO_CTRL_PWRG_VTR_IO:        *pui8Data = '0'; break;
					// case MD_GPIO_EM_POWER__VccMain:    *pui8Data = '1'; break;
					case GPIO_CTRL_PWRG_OFF:  *pui8Data = '2'; break;
					// case MD_GPIO_EM_POWER__Reserved:   *pui8Data = '3'; break;
					default:                           *pui8Data = '-'; break;
			    }
				break;
			case 0x18:
				GET_NV_OPTIONS(f_wirelessManagability, tmp);
				*pui8Data = tmp ? 1 : 0;
				break;
			case 0x20:  *pui8Data = 'R'; break;
			case 0x21:  *pui8Data = 'E'; break;
			case 0x22:  *pui8Data = 'A'; break;
			case 0x23:  *pui8Data = 'D'; break;
			case 0x24:  *pui8Data = ':'; break;
			case 0x25:  *pui8Data = gpio_read_pin(MPM_EVENT_N) ? '1' : '0';
				break;
			case 0x28:  *pui8Data = 'W'; break;
			case 0x29:  *pui8Data = 'S'; break;
			case 0x2A:  *pui8Data = 'E'; break;
			case 0x2B:  *pui8Data = 'T'; break;
			case 0x2C:  *pui8Data = ':'; break;
			case 0x2D:	*pui8Data = gpio_was_mpm_set(MPM_EVENT_N) ? '1' : '0';
				break;
			default:
				break;
		}
	} else {
		switch (ui8Idx) {
			case 0x18:
				tmp = *pui8Data ? 1 : 0;
				app_manOs_setEnFlag(tmp);
				break;
			default:
				break;
		}
	}

	return 1;
}
#endif