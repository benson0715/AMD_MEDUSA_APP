/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include "board_id.h"
#include "board_config.h"
#include "smchost.h"
#include "smchost_commands.h"
#include "scicodes.h"
#include "sci.h"
#include "acpi.h"
#include "app_pseq.h"
#include "kbchost.h"
#include "espi_hub.h"
#include "app_btn.h"
#if CONFIG_WIRELESS_MANAGEABILITY
#include "app_manOs.h"
#endif
#include "app_acpi.h"
#include "app_smtbty.h"

LOG_MODULE_DECLARE(smchost, CONFIG_SMCHOST_LOG_LEVEL);

/* ************************** *
 *     static valuable        *
 * ************************** */
static bool pwrbtn_notify;
static uint8_t legacy_wake_status;
static bool cs_low_pwr_mode;
static bool cs_state;
static uint8_t pwrbtn_post_sts[2];
static uint8_t pwrbtn_post_pos;
static uint8_t pwrbtn_post_cnt;

extern uint32_t g_pwrSeqDelay;

/**
 * @brief system cs status return
 *
 * @retval cs status
 */
bool smchost_is_system_in_cs(void)
{
	return cs_state;
}

/**
 * @brief power button press handler
 *
 * @param     pwrbtn_sts: power button status
 */
void smchost_pwrbtn_post_handler(uint8_t pwrbtn_sts)
{
	/* Detect HW Power Button falling edge in S3/S4/S5 */
	if((app_pseq_systemState() >= SYSTEM_S3_STATE) && (pwrbtn_sts == 0)) {
		LOG_DBG("PWR_BTNn falling edge in power state %d", app_pseq_systemState());
	
#if 1
		/* Only check EC FW version when S3/4/5 resume by power button */
		extern bool ec_version_checkEcFwVersion(void);
		if (ec_version_checkEcFwVersion() == false)
		{
			LOG_DBG("EC FW Version not match!!!");
			app_btn_ecReset();
		}
#endif
		app_pseq_nextStep(FROM_S5S3_TO_S0, 5);
	} else if((app_pseq_systemState() == SYSTEM_G3_STATE) && (pwrbtn_sts == 0)) {
		/* Detect HW Power Button falling edge in G3. */
		LOG_DBG("PWR_BTNn falling edge in power state SYS_G3");
		app_pseq_nextStep(FROM_G3_TO_S0, 5);
	} else if((app_pseq_systemState() >= SYSTEM_S4_STATE) && pwrbtn_sts) {
		/* Detect HW Power Button rising edge */
		LOG_DBG("PWR_BTNn rising edge in power state %d", app_pseq_systemState());
		app_btn_deassertFchPwrBtn();
	} else {
		if (!g_slideToShutdownEnabled) {
			/* If the pwrbtn WR is not enabeld, just passing through the PWRBTN signal */
			if(pwrbtn_sts) {
				LOG_DBG("PWR_BTNn rising	edge in power state %d", app_pseq_systemState());
				app_btn_deassertFchPwrBtn();
			} else {
				LOG_DBG("PWR_BTNn falling edge in power state %d", app_pseq_systemState());
				app_btn_assertFchPwrBtn();
			}
		}
	}
}

/**
 * @brief probe and handle the post pwrbtn event.
 */
void smchost_pwrbtn_post_signal_probe(void)
{
	if (pwrbtn_post_cnt) {
		uint8_t pos = pwrbtn_post_pos;

		/* Always restore the last two events */
		if (pwrbtn_post_cnt >= 2) {
			pos = (pwrbtn_post_pos+1) % 2;
			smchost_pwrbtn_post_handler(pwrbtn_post_sts[pos]);
		}
		smchost_pwrbtn_post_handler(pwrbtn_post_sts[pos]);
		pwrbtn_post_cnt = 0;
	}
}

/**
 * @brief power button press handler
 *
 * @param     pwrbtn_sts: power button status
 */
void smchost_pwrbtn_handler(uint8_t pwrbtn_sts)
{
#if defined(FPR_EC_PWRBTN_EVENT_MASK_N) && defined(ex_EP_FPR_PWREN) 
	if (!gpio_read_pin(FPR_EC_PWRBTN_EVENT_MASK_N) && ioexp_get(ex_EP_FPR_PWREN) && app_pseq_systemState() == SYSTEM_S0_STATE) {

		/* EC should ignore the EC Control signal and always respond to the long-press power button event, that user could force shut down system; */
		if (pwrbtn_sts) {
			app_btn_timer_stop_ex();
		} else {
			app_btn_timer_start_ex();
		}

		/* Power button event mask. From FPR module to EC.
		   Fingerprint drives this signal to low to notify EC that fingerprint function is currently being used, 
		   don't response to power button event so as to eliminate power button touch by mistake. */
		return ;
	}
#endif

	if (g_pwrSeqDelay == 0) {
		smchost_pwrbtn_post_handler(pwrbtn_sts);
	} else {
		/* Always store the last two events */
		pwrbtn_post_cnt++;
		pwrbtn_post_pos++;
		pwrbtn_post_pos %= 2;
		pwrbtn_post_sts[pwrbtn_post_pos] = pwrbtn_sts;
		LOG_WRN("PWR_BTNn signal handle post %d", pwrbtn_post_cnt);
	}
	/* change FCH power button */
	if ( pwrbtn_sts) {
		app_btn_deassertCallback();
	} else {
		app_btn_assertCallback();
#if CONFIG_WIRELESS_MANAGEABILITY
		/* app_manOs PwrBtn hook on falling edge */
		app_manOs_PwrBtnHook();
#endif
	}
}

/**
 * @brief Handle SX entry state transition.
 */
static void sx_entry(void)
{

}

/**
 * @brief Handle SX exit state transition.
 */
static void sx_exit(void)
{

}

/**
 * @brief Handle connected standby entry state transition.
 */
static void cs_entry(void)
{
	if (!cs_low_pwr_mode) {
		return;
	}

	cs_state = true;
}

/**
 * @brief Handle connected standby exit state transition.
 */
static void cs_exit(void)
{
	cs_state = false;

#if CONFIG_THERMAL_MANAGEMENT
	thermalmgmt_handle_cs_exit();
#endif
}

/**
 * @brief Get legacy wake status and send to host.
 */
static void get_legacy_wake_sts(void)
{
	send_to_host(&legacy_wake_status, 1);
}

/**
 * @brief Clear legacy wake status.
 */
static void clear_legacy_wake_sts(void)
{
	legacy_wake_status = 0;
}

/**
 * @brief SMC host power management command handler.
 *
 * @param command The command code to process.
 */
void smchost_cmd_pm_handler(uint8_t command)
{
	switch (command) {
	case SMCHOST_ENABLE_PWR_BTN_NOTIFY:
		pwrbtn_notify = true;
		break;
	case SMCHOST_DISABLE_PWR_BTN_NOTIFY:
		pwrbtn_notify = false;
		break;
	case SMCHOST_GET_LEGACY_WAKE_STS:
		get_legacy_wake_sts();
		break;
	case SMCHOST_CLEAR_LEGACY_WAKE_STS:
		clear_legacy_wake_sts();
		break;
	case SMCHOST_SX_ENTRY:
		sx_entry();
		break;
	case SMCHOST_SX_EXIT:
		sx_exit();
		break;
	case SMCHOST_CS_ENTRY:
		cs_entry();
		break;
	case SMCHOST_CS_EXIT:
		cs_exit();
		break;
	default:
		LOG_WRN("%s: command 0x%X without handler", __func__, command);
		break;
	}
}