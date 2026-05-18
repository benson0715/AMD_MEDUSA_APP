/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "app_acpi.h"
#include "app_btn.h"
#include "periphmgmt.h"
#include "board_config.h"
#include "app_pseq.h"
#include "f_nv_options.h"
#include "dbgLogFifo.h"
#include "kbs_matrix.h"
#if CONFIG_WIRELESS_MANAGEABILITY
#include "app_manOs.h"
#endif

LOG_MODULE_REGISTER(periph, CONFIG_PERIPHERAL_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
/* Delay to start power on sequencing - 300 ms*/
#define PWRBTN_POWERON_DELAY               (300)
#define MAX_PWRBTN_HANDLERS                (4)

#define EC_WDT_RESET_COUNT_ON_PWRBTN_LOW   (16 * 1000) /* 16s */
#define EC_DELAY_TO_SEND_PWRBTN            (6 * 1000)  /* 6s */
#define EC_SLIDE_TO_SHUTDOWN               (2 * 1000)  /* 2s */
#define EC_DELAY_FORCE_SHUTDOWN_EX         (10 * 1000)  /* 10s */

/* ************************** *
 *     static valuable        *
 * ************************** */
/* flag to send slide shutdown event */
static bool _s_hadsentSlideToShutdown = false;
/* power button trigger ec force reset counter */
static uint32_t _s_u32ecRstCount = 0;
/* debounce for _s_hasPwrStOtherThanS0 */
static uint32_t _s_u32inS0Counter = 0;
/* flag for power status not in S0 */
static bool _s_hasPwrStOtherThanS0 = true;
/* for control method Power Button */
static uint8_t _s_notifyPwrBtn = 1;
/* trigger slide shutdown */
static bool _s_triggerSlideShutDown = false;
static bool _s_triggerForceShutDown_ex = false;

#if defined(BRD_MDS_DORNE)
/* flag for kbc led for power status indicator */
static bool _s_kbcLedIndicator_dis = false;
#endif

struct pwrbtn_handler {
	sys_snode_t	node;
	pwrbtn_handler_t handler;
};

/* This is just a pool */
static struct pwrbtn_handler pwrbtn_handlers[MAX_PWRBTN_HANDLERS];
static int pwrbtn_handler_index;
static bool pwr_btn_out_sts = true;

/* fake fch button timer */
static struct k_timer _s_fakeFchBtnClick;
static void _fakeFchBtnClickTimerCallback(struct k_timer *timer);
/* press power button timer */
static struct k_timer _s_btnEcReset, _s_btnSlidShutDown, _s_btnForceShutDown, _s_btnForceShutDown_ex;
static void _btnEcResetCallback(struct k_timer *timer);
static void _btnSlidShutDownCallback(struct k_timer *timer);
static void _btnForceShutDownCallback(struct k_timer *timer);
static void _btnForceShutDownCallback_ex(struct k_timer *timer);

/**
 * app_btn_maskFchPwrBtn
 *
 * @note
 *  set fch power button mask
 */
void app_btn_maskFchPwrBtn(void)
{
	_s_notifyPwrBtn = 0;
}

/**
 * app_btn_unmaskFchPwrBtn
 *
 * @note
 *  clear fch power button mask
 */
void app_btn_unmaskFchPwrBtn(void)
{
	_s_notifyPwrBtn = 1;
}

/**
 * app_btn_isFchPwrBtnNotifyEnabled
 *
 * @return true: fch power button notification enabled
 *
 * @note
 *  if fch power button notification is enabled
 */
uint8_t app_btn_isFchPwrBtnNotifyEnabled(void)
{
	return _s_notifyPwrBtn;
}

/**
 * app_btn_assertFchPwrBtn
 *
 * @note
 *  assert fch power button gpio
 */
void app_btn_assertFchPwrBtn(void)
{
	/* Do the unmask just before FchPwrBtn assertion
	 * Hence to ensure it would not be masked unexpectedly.
	 * SBIOS set it as rising edge active.
	 */
	app_btn_unmaskFchPwrBtn();
	ASSERT_FCH_PWRBTN;
	dbgLogFifo_printf("FBTN: ASSERT\n");
}

/**
 * app_btn_deassertFchPwrBtn
 *
 * @note
 *  deassert fch power button gpio
 */
void app_btn_deassertFchPwrBtn(void)
{
	DEASSERT_FCH_PWRBTN;
	dbgLogFifo_printf("FBTN: DEASSERT\n");
}

/**
 * app_btn_slideHandler
 *
 * @note
 *  send the fake slide shutdown event by mimic kcode
 */
void app_btn_slideHandler(void)
{
	LOG_DBG("Slid Shutdown");

	/* mask FchPwrBtn before send hot keys.*/
	app_btn_maskFchPwrBtn();
	_s_triggerSlideShutDown = true;
}

/**
 * app_btn_triggerPwrBtn
 *
 * @param [in]    hold;     ms to hold the fake power button pulse
 * @return void
 *
 * @note
 *  trigger a fake power button pulse by "hold" ms
 */
void app_btn_triggerPwrBtn(uint8_t hold)
{
	if (hold) {
		LOG_DBG("Trigger fake PWR_BTNn");
		app_btn_assertFchPwrBtn();
		k_timer_start(&_s_fakeFchBtnClick, K_MSEC(hold), K_NO_WAIT);  /* hold ms */
	}
}

/**
 * app_btn_debouncerPwrBtn
 *
 * @param [in]    pwrbtn_evt;    power button status
 * @return void
 *
 * @note
 *  power button status change handler callback (from notify_btn_handlers )
 */
void app_btn_debouncerPwrBtn(uint8_t pwrbtn_evt)
{
	dbgLogFifo_printf("BTN: %d\n", pwrbtn_evt);
	LOG_DBG("sys_evt=%d, sys_pwr_btn_sts=%d", pwrbtn_evt, pwr_btn_out_sts);

	/* Current status should be different from last
	 * executed status.
	 */
	if (pwrbtn_evt != pwr_btn_out_sts) {

		pwr_btn_out_sts = pwrbtn_evt;

		LOG_DBG("power button event %d is executing ", pwr_btn_out_sts);

		for (int i = 0; i < MAX_PWRBTN_HANDLERS; i++) {
			if (pwrbtn_handlers[i].handler) {
				LOG_DBG("Calling handler %s", __func__);
				pwrbtn_handlers[i].handler(pwr_btn_out_sts);
			}
		}
	}
}

/**
 * app_btn_registerHandler
 *
 * @param [in]    handler;    power button handler pointer
 * @return void
 *
 * @note
 *  power button status change handler pointer (to smchost_pwrbtn_handler )
 */
void app_btn_registerHandler(pwrbtn_handler_t handler)
{
	if (pwrbtn_handler_index < MAX_PWRBTN_HANDLERS) {
		pwrbtn_handlers[pwrbtn_handler_index].handler = handler;
		pwrbtn_handler_index++;
	}
}

/**
 * app_btn_init
 *
 * @return status < 0 is error
 *
 * @note
 *  app button initiate
 */
int app_btn_init(void)
{
	LOG_INF("%s", __func__);

	k_timer_init(&_s_fakeFchBtnClick, _fakeFchBtnClickTimerCallback, NULL);
	k_timer_init(&_s_btnEcReset, _btnEcResetCallback, NULL);
	k_timer_init(&_s_btnSlidShutDown, _btnSlidShutDownCallback, NULL);
	k_timer_init(&_s_btnForceShutDown, _btnForceShutDownCallback, NULL);
	k_timer_init(&_s_btnForceShutDown_ex, _btnForceShutDownCallback_ex, NULL);

	/* Register power button for debouncing */
	return periph_register_debounce_handler(HW_PWR_BTN_N, app_btn_debouncerPwrBtn);
}

/**
 * app_btn_timer_stop_ex
 *
 * @note
 *  Disable power button timer
 */
void app_btn_timer_stop_ex(void)
{
	if (_s_triggerForceShutDown_ex) {
		/* make sure FCH GPIO0 is HIGH when EC_PWRBTN is HIGH for all case */
		app_btn_deassertFchPwrBtn();

		/* clear the long press status */
		_s_u32ecRstCount = 0;

		/* if no slide-to-shutdown, no pwrst change and the PWRBTN WR is enabeld, send a click */
		if (!_s_hadsentSlideToShutdown && !_s_hasPwrStOtherThanS0 && g_slideToShutdownEnabled) {
			app_btn_triggerPwrBtn(30);
		}
		_s_hadsentSlideToShutdown = false;
		_s_triggerForceShutDown_ex = false;
	}

	k_timer_stop(&_s_btnEcReset);
	k_timer_stop(&_s_btnForceShutDown_ex);
}

/**
 * app_btn_timer_start_ex
 *
 * @note
 *  Enable power button timer
 */
void app_btn_timer_start_ex(void)
{
	k_timer_start(&_s_btnEcReset, K_MSEC(EC_WDT_RESET_COUNT_ON_PWRBTN_LOW), K_NO_WAIT);
	k_timer_start(&_s_btnForceShutDown_ex, K_MSEC(EC_DELAY_FORCE_SHUTDOWN_EX), K_NO_WAIT);
}

/**
 * app_btn_deassertCallback
 *
 * @note
 *  callback for power button deassert
 */
void app_btn_deassertCallback(void)
{
	LOG_DBG("PWR_BTNn_HW goes to HIGH ...");

	/* make sure FCH GPIO0 is HIGH when EC_PWRBTN is HIGH for all case */
	app_btn_deassertFchPwrBtn();

	/* clear the long press status */
	_s_u32ecRstCount = 0;

	/* if no slide-to-shutdown, no pwrst change and the PWRBTN WR is enabeld, send a click */
	if (!_s_hadsentSlideToShutdown && !_s_hasPwrStOtherThanS0 && g_slideToShutdownEnabled) {
		app_btn_triggerPwrBtn(30);
	}
	_s_hadsentSlideToShutdown = false;

	k_timer_stop(&_s_btnEcReset);
	k_timer_stop(&_s_btnSlidShutDown);
	k_timer_stop(&_s_btnForceShutDown);
}

/**
 * app_btn_assertCallback
 *
 * @note
 *  callback for power button assert
 */
void app_btn_assertCallback(void)
{
	LOG_DBG("PWR_BTNn_HW goes to LOW ...");
	k_timer_start(&_s_btnEcReset, K_MSEC(EC_WDT_RESET_COUNT_ON_PWRBTN_LOW), K_NO_WAIT);
	k_timer_start(&_s_btnSlidShutDown, K_MSEC(EC_SLIDE_TO_SHUTDOWN), K_NO_WAIT);
	k_timer_start(&_s_btnForceShutDown, K_MSEC(EC_DELAY_TO_SEND_PWRBTN), K_NO_WAIT);
}

/**
 * app_btn_monitor
 *
 * @note
 *  app button monitor power sequence status in loop
 */
void app_btn_monitor (void)
{
	uint8_t state = app_pseq_systemState();

	/* Set the "_s_hasPwrStOtherThanS0" based on power state */
	if (state != SYSTEM_S0_STATE) {
#if defined(BRD_MDS_DORNE)
		if (state == SYSTEM_S3_STATE) {
			_s_kbcLedIndicator_dis = true;
		} else if (_s_kbcLedIndicator_dis && state != SYSTEM_S0_R_STATE) {
			_s_kbcLedIndicator_dis = false;
		}
#endif
		_s_hasPwrStOtherThanS0 = true;
		_s_u32inS0Counter = 0;
	}
	if (_s_hasPwrStOtherThanS0 && state == SYSTEM_S0_STATE) {
		_s_u32inS0Counter ++;

#if defined(BRD_MDS_DORNE)
		if (!_s_kbcLedIndicator_dis) {
			if (_s_u32inS0Counter > 1000) {
				gpio_write_pin(EC_CAP_LED, 0);
				gpio_write_pin(EC_F4_LED, 0);
				_s_kbcLedIndicator_dis = true;
			} else {
				gpio_write_pin(EC_CAP_LED, 1);
				gpio_write_pin(EC_F4_LED, 1);
			}
		}
#endif

		if (_s_u32inS0Counter > 1900) {
			_s_hasPwrStOtherThanS0 = false;
		}
	}

	if (_s_triggerSlideShutDown) {
		_s_triggerSlideShutDown = false;
#if CONFIG_KSCAN
		/* Trigger the report for slid function win+F16 */
		kbs_generate_key_win_slide();
#endif
	}
}

/**
 * app_btn_ecReset
 *
 * @note
 *  force reset EC by power button press 16s
 */
void app_btn_ecReset(void)
{
	/* Disable i.8V ALW to reset Bios ROM */
	gpio_write_pin(EC_1V8_S5_EN, 0);
	k_busy_wait(1000 * USEC_PER_MSEC);
	sys_reboot(SYS_REBOOT_COLD);
}

/**
 * _fakeFchBtnClickTimerCallback
 *
 * @note
 *  timer callback
 */
static void _fakeFchBtnClickTimerCallback(struct k_timer *timer)
{
	app_btn_deassertFchPwrBtn();
}

/**
 * _btnEcResetCallback
 *
 * @note
 *  timer callback
 */
static void _btnEcResetCallback(struct k_timer *timer)
{
	app_btn_ecReset();
}

/**
 * _btnSlidShutDownCallback
 *
 * @note
 *  timer callback
 */
static void _btnSlidShutDownCallback(struct k_timer *timer)
{
	if (g_slideToShutdownEnabled && false == _s_hadsentSlideToShutdown) {
		_s_hadsentSlideToShutdown = true;
		app_btn_slideHandler();
	}
}

/**
 * _btnForceShutDownCallback
 *
 * @note
 *  timer callback
 */
static void _btnForceShutDownCallback(struct k_timer *timer)
{
	if (g_slideToShutdownEnabled) {
		app_btn_assertFchPwrBtn();
		LOG_DBG("Passing through ... Set FCH PWR_BTNn_EC to 0");
	}
}

/**
 * _btnForceShutDownCallback_ex
 *
 * @note
 *  timer callback
 */
static void _btnForceShutDownCallback_ex(struct k_timer *timer)
{
#if CONFIG_WIRELESS_MANAGEABILITY
	/* app_manOs PwrBtn hook on falling edge */
	app_manOs_PwrBtnHook();
#endif
	
	if (g_slideToShutdownEnabled && false == _s_hadsentSlideToShutdown) {
		_s_hadsentSlideToShutdown = true;
		app_btn_slideHandler();
	}
	
	if (g_slideToShutdownEnabled) {
		app_btn_assertFchPwrBtn();
		LOG_DBG("Passing through ... Set FCH PWR_BTNn_EC to 0");
	}

	_s_triggerForceShutDown_ex = true;
}