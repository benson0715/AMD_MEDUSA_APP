/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "espi_hub.h"
#include "espioob_mngr.h"
#include "system.h"
#include "app_pseq.h"
#include "board_config.h"
#include "app_udc.h"
#include "sci.h"
#include "scicodes.h"
#include "errcodes.h"
#include "kbchost.h"
#include "task_handler.h"
#include "softstrap.h"
#include "smchost.h"
#if (CONFIG_USBC_CCGX)
#include "app_ccgx.h"
#endif
#if (CONFIG_USBC_4CC)
#include "app_4cc.h"
#endif
#include "app_usbc_task.h"
#include "app_ucsi_tunnel.h"
#include "app_btn.h"
#include "board_id.h"
#include "f_nv_options.h"
#include "ioexp_hub.h"
#if (CONFIG_VR_CONFIG_TABLE)
#include "board_vrCfg.h"
#endif
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
#include "board_pwrSrc.h"
#endif
#if (CONFIG_BATTERY_MANAGEMENT)
#include "app_smtbty.h"
#endif
#if CONFIG_WIRELESS_MANAGEABILITY
#include "app_manOs.h"
#endif
#if CONFIG_CHARGER_ISL9241
#include "dev_isl9241.h"
#elif CONFIG_CHARGER_MP2764
#include "amd_crb_drivers_mp2764.h"
#endif
#include "app_acpi.h"
#include "board_init.h"
#include "acpi.h"
#include "board_sleep.h"
#include "smchost_extended.h"
#include "amd_crb_drivers_spiFlash.h"
#include "f_bram.h"
#include "dbgLogFifo.h"
#if CONFIG_FAN_RPM_CONTROL
#include "app_fanRpmCtrl.h"
#endif
#if CONFIG_ALS
#include "app_opt4048_thread.h"
#endif
LOG_MODULE_REGISTER(pwrmgmt, CONFIG_PWRMGT_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#define POWER_SEQ_S5_TO_G3_DELAY         (10000)   // delay 10s

/* ************************** *
 *     static valuable        *
 * ************************** */
/* Track power sequencing events */
static bool pwrseq_failure;
/* init espi flag */
static bool init_espi = false;
/* System state machine */
static enum system_power_state current_state;
static enum system_power_state next_state;
/* System stage machine */
static enum system_power_seq_stage next_stage;
/* Status wait counter */
static uint32_t _s_slpS35WaitCnt = 0;
static uint32_t _s_gotoG3WaitCnt = 0;
static uint8_t _s_pwrStHadS3 = 0;

#if CONFIG_VR_CONFIG_TABLE
static bool _s_vrNeedG3 = false;
static uint8_t _s_vrNeedG3_cnt = 0;
#endif

static char *system_power_state_str[] = {
	"SYSTEM_INIT_STATE",
	"SYSTEM_G3_STATE",
	"SYSTEM_S0_R_STATE",
	"SYSTEM_S0_STATE",
	"SYSTEM_S3_STATE",
	"SYSTEM_S4_STATE",
	"SYSTEM_S5_STATE"
};
/* ************************** *
 *     global valuable        *
 * ************************** */
uint32_t g_ui32_pwrBtnMissCnt = 0;
uint32_t g_pwrSeqDelay = 0;
bool g_pwrSeqExpired = false;
/* This global variable is used mostly for pin configuration in board init */
uint8_t boot_mode_maf;
/* power button click to wake up vci mode */
uint8_t g_ui8PwrBtnVciFlag;
/* wwan_wlan D3 code flag */
uint8_t g_auxD3Cold = 0;
#if CONFIG_SFH
uint8_t g_WAIE_RITE = 0;
#endif
uint8_t g_LPCAMM_PWR_EN = 0;

/* task slp ready */
uint8_t *task_pseqSlpReady = NULL;

#if CONFIG_USBC_UCSI_TUNNEL
extern bool g_isUcsiCycleFinished;
#endif
extern uint8_t waie_syc_flag;
extern uint8_t g_S5AlwEnFlag;

#if CONFIG_HOT_BAG_EN
bool g_powerSeq_criticalZone = false;
extern struct k_timer g_pmfHeartBeatTimer;
extern PMF_STATUS g_pmfStatus;
extern void ACPI_pmfHeartBeatRefresh(void);
#endif

/* Handle S5 entry/exit and G3 exit */
static void app_pseq_exitS0(void);
void app_pseq_StateMachine(void);
void app_pseq_nextStep(enum system_power_seq_stage next, uint32_t delay);
void app_pseq_setupPwrCard(void);
void app_pseq_stateMonitor(void);

/* Sequence delay callback*/
static void app_pseq_delayCallback(struct k_timer *timer);
K_TIMER_DEFINE(_s_pseqDelayTimerId, app_pseq_delayCallback, NULL);

/**
 * @brief Get current system power state
 *
 * @retval current system power state
 */
enum system_power_state app_pseq_systemState(void)
{
	return current_state;
}

/**
 * @brief sleep enter handler
 *
 * @param  signal: trigger signal
 * @param  status: power status
 */
static void app_pseq_slp_handler(uint32_t signal, uint32_t status)
{
	/* De-assert always indicates transition to S0 */
	if (status) {
		switch (current_state) {
		case SYSTEM_S5_STATE:
		case SYSTEM_S4_STATE:
		case SYSTEM_G3_STATE:
		case SYSTEM_S3_STATE:
			next_state = SYSTEM_S0_STATE;
			break;
		default:
			LOG_WRN("SLP_SX=1 while at %x", current_state);
			break;
		}
	} else  {
		/* SLPx assertion indicates a specific power system state */
		switch (signal) {
		case ESPI_VWIRE_SIGNAL_SLP_S3:
			LOG_DBG("SLP S3 asserted");
			next_state = SYSTEM_S3_STATE;
			break;
		case ESPI_VWIRE_SIGNAL_SLP_S4:
			LOG_DBG("SLP S4 asserted");
			next_state = SYSTEM_S4_STATE;
			break;
		case ESPI_VWIRE_SIGNAL_SLP_S5:
			LOG_DBG("SLP S5 asserted");
			next_state = SYSTEM_S5_STATE;
			break;
		default:
			break;
		}
	}
}

/**
 * @brief EC boot mode detect
 *
 * @param  boot_mode: SAF/MAF/SHARING
 */
static void handle_spi_sharing(uint8_t boot_mode)
{
	switch (boot_mode) {
	case FLASH_BOOT_MODE_SAF:
		LOG_DBG("Booted in SAF mode");
		/* TODO: set FET HIGH, not possible in MECC card */
		break;
	case FLASH_BOOT_MODE_G3_SHARING:
		LOG_DBG("Booted in G3 mode");
		/* TODO: set FET LOW, not possible in MECC card */
		break;
	case FLASH_BOOT_MODE_MAF:
		boot_mode_maf = 1;
		LOG_DBG("Booted in MAF mode");
		break;
	default:
		LOG_DBG("Boot in %x mode", boot_mode);
		break;
	}
}

/**
 * @brief power sequence error handler
 *
 * @param  error_code: sequence error code
 */
void app_pseq_error(uint8_t error_code)
{
	LOG_ERR("%s: %d", __func__, error_code);

	pwrseq_failure = true;
#if CONFIG_POSTCODE_MANAGEMENT
	update_error(error_code);
#endif
	k_msleep(100);
}

/**
 * @brief power sequence task init
 */
static inline int app_pseq_task_init(void)
{
	int ret = 0;

	LOG_DBG("Power initialization...");

	current_state = SYSTEM_INIT_STATE;
	next_state = SYSTEM_INIT_STATE;

	/* SW strap always takes precedence */
#if CONFIG_POWER_SEQUENCE_DISABLE_TIMEOUTS
	pwrseq_timeout_disabled = true;
	LOG_WRN("SW timeouts disabled: %d", pwrseq_timeout_disabled);
#endif /* CONFIG_POWER_SEQUENCE_DISABLE_TIMEOUTS */

	handle_spi_sharing(espihub_boot_mode());

	espihub_add_state_handler(app_pseq_slp_handler);

	/* Initiate the power sequence status */
	app_pseq_nextStep(POWER_INIT, 1);

	return ret;
}

/**
 * @brief power sequence transition to S0 handler
 */
static bool app_pseq_transitionToS0(void)
{
	bool valid_transition = false;

	switch (current_state) {
	case SYSTEM_G3_STATE: /* G3 -> S0 */
		/* in case the pins were off in G3 */
		board_evalCardCtrlPins();
		valid_transition = true;
		break;
	case SYSTEM_S5_STATE: /* S5 -> S0 */
#if CONFIG_WIRELESS_MANAGEABILITY
		/* call manOs Exit S5 hook */
		app_manOs_ExitS5Hook();
#endif
		valid_transition = true;
		break;
	case SYSTEM_S3_STATE: /* S3 -> S0 */
		valid_transition = true;
#if CONFIG_HOT_BAG_EN
		if (g_ui8MondernStandbySupport && (g_pmfStatus == PMF_SUSPEND)) {
			/* set critical zone if power_seq change from S0i3 to S0 */
			g_powerSeq_criticalZone = true;
			ACPI_pmfHeartBeatRefresh();
		}
#endif
		break;
	case SYSTEM_S0_R_STATE: /* S0R -> S0 */
		valid_transition = true;
		break;
	default:
		/* No action */
		break;
	}

#if CONFIG_FAN_RPM_CONTROL
	app_fan_ctrl_sys_enable();
#endif

	/* sleep is not allowed in S0 */
	task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 0);

	/* Clear the qevent buffer in S0 */
	if (!g_ui8MondernStandbySupport) {
		ACPI_ClearQueuedEvts();
	}
	/* Enable the IBF and OBE interrupt */
	acpi_ibf_intr(0, true); /* enable EC0 IBF interrupt */
	acpi_obe_intr(0, true); /* enable EC0 OBE interrupt */
	acpi_ibf_intr(1, true); /* enable EC1 IBF interrupt */

	/* enable the ACPI and others in S0 */
#if CONFIG_UDC_MANAGEMENT
	app_postcode_turnOn();
#endif
#if CONFIG_WIRELESS_MANAGEABILITY
	/* manOs hook */
	app_manOs_EnterS0Hook();
#endif
	/* turn on CHG_PROCHOTn interrupt */
	gpio_interrupt_configure_pin(CHG_EC_PROCHOT_N, GPIO_INT_EDGE_BOTH);
#if CONFIG_SOC_FAMILY_MEC
	gpio_write_pin(EC_APU_PROCHOT_N, 0);
#else
	gpio_write_pin(EC_APU_PROCHOT_N, 1);
#endif
	/* Enable ACPI notify on S3->S0 if it's MS system; from == SYS_S0_R */
	if (g_ui8MondernStandbySupport && ACPI_isHostNotifyDisable()) {
		ACPI_EnableHostNotify();
	}
	gpio_set_power(APU_THERMTRIP_N, GPIO_CTRL_PWRG_VTR_IO);
#if CONFIG_BATTERY_MANAGEMENT
#if CONFIG_CHARGER_ISL9241
	dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
	amd_crb_drivers_mp2764_setProchot();
#endif
#endif
#if (CONFIG_USBC_CCGX)
	if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx)
		app_ccgx_updatePwrStatus(SYSTEM_S0_STATE);
#endif
#if (CONFIG_USBC_4CC)
	amd_crb_drivers_tps_setSxAppConfig(TYPEC_PORT_0_IDX, SYSTEM_S0_STATE);
	amd_crb_drivers_tps_setSxAppConfig(TYPEC_PORT_2_IDX, SYSTEM_S0_STATE);
#endif
	return valid_transition;
}

/**
 * @brief power sequence transition to S0R handler
 */
static bool app_pseq_transitionToS0R(void)
{
	bool valid_transition = true;

	if (_s_pwrStHadS3) {
		_s_pwrStHadS3 = 0;
		/* call the sleep exit */
		board_init_apuSleepExitHandler();
	}

	switch (current_state) {
	case SYSTEM_S5_STATE: /* S5 -> S0 */
#if CONFIG_WIRELESS_MANAGEABILITY
		/* call manOs Exit S5 hook */
		app_manOs_ExitS5Hook();
#endif
		valid_transition = true;
		break;
	default:
		/* No action */
		break;
	}

#ifdef EC_FPR_OP_BOOT_LOGIN_N
	/* Drive this pin to high when system enter s0i3 state */
	gpio_write_pin(EC_FPR_OP_BOOT_LOGIN_N, 1);
#endif

	/* sleep is not allowed in S0R */
	task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 0);

	return valid_transition;
}

/**
 * @brief power sequence transition to S3 handler
 */
static bool app_pseq_transitionToS3(void)
{
	bool valid_transition = false;

	switch (current_state) {
	case SYSTEM_S0_STATE: /* S0 -> S3 */
		app_pseq_exitS0();
#ifdef EC_FPR_OP_BOOT_LOGIN_N
		/* Drive this pin to low when system enter /S4/S5 state */
		gpio_write_pin(EC_FPR_OP_BOOT_LOGIN_N, 1);
#endif
		valid_transition = true;
#if CONFIG_SFH
		/* clear waie in S3 */
		waie_syc_flag = 0xD1;
#endif
		break;
	case SYSTEM_G3_STATE: /* G3 -> S3 */
		/* in case the pins were off in G3 */
		board_evalCardCtrlPins();
		valid_transition = true;
		break;
	case SYSTEM_S5_STATE: /* S5 -> S3 */
#if CONFIG_WIRELESS_MANAGEABILITY
		/* call manOs Exit S5 hook */
		app_manOs_ExitS5Hook();
#endif
		valid_transition = true;
		break;
	default:
		break;
	}

	/* sleep is allowed in S3 */
	task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 1);

	/* Set the flag after enter S3 */
	_s_pwrStHadS3 = 1;
	/* need to call the APU sleep handle and manos hook */
	board_init_apuSleepEnterHandler();
#if CONFIG_WIRELESS_MANAGEABILITY
	app_manOs_EnterS3Hook();
#endif
#if CONFIG_BATTERY_MANAGEMENT
#if CONFIG_CHARGER_ISL9241
	dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
	amd_crb_drivers_mp2764_setProchot();
#endif
#endif
	if (g_ui8MondernStandbySupport) {
#if CONFIG_SLEEP
		board_slp_enableKSxWakeup();
#endif
		/* need to disable the IBF and OBE interrupt */
		acpi_ibf_intr(0, false); /* disable EC0 IBF interrupt */
		acpi_obe_intr(0, false); /* disable EC0 OBE interrupt */
		acpi_ibf_intr(1, false); /* disable EC1 IBF interrupt */
	}
#if (CONFIG_USBC_CCGX)
	if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx)
		app_ccgx_updatePwrStatus(SYSTEM_S3_STATE);
#endif
#if CONFIG_USBC_UCSI_TUNNEL
	if (g_isUcsiCycleFinished == false) {
		g_isUcsiCycleFinished = true;
		LOG_DBG("Force to clear g_isUcsiCycleFinished");
	}
#endif
	return valid_transition;
}

/**
 * @brief power sequence transition to S4 handler
 */
static bool app_pseq_transitionToS4(void)
{
	/* There is no S4 power status in EC side. So always return true */
	bool valid_transition = true;

	/* sleep is allowed in S4 */
	task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 1);

	switch (current_state) {
		case SYSTEM_S0_STATE: /* S0 -> S4 */
#ifdef EC_FPR_OP_BOOT_LOGIN_N
			/* Drive this pin to low when system enter S0i3/S4/S5 state */
			gpio_write_pin(EC_FPR_OP_BOOT_LOGIN_N, 0);
#endif
			app_pseq_exitS0();
#if CONFIG_SFH
			/* clear waie in S4 */
			waie_syc_flag = 0xD1;
#endif
			break;
		default:
			break;
	}
#if CONFIG_USBC_UCSI_TUNNEL
	if (g_isUcsiCycleFinished == false) {
		g_isUcsiCycleFinished = true;
		LOG_DBG("Force to clear g_isUcsiCycleFinished");
	}
#endif
	return valid_transition;
}

/**
 * @brief power sequence transition to S5 handler
 */
static bool app_pseq_transitionToS5(void)
{
	bool valid_transition = false;

	switch (current_state) {
	case SYSTEM_S3_STATE: /* S3 -> S5 */
		valid_transition = true;
		break;
	case SYSTEM_S0_STATE: /* S0 -> S5 */
		app_pseq_exitS0();
#ifdef EC_FPR_OP_BOOT_LOGIN_N
		/* Drive this pin to low when system enter S0i3/S4/S5 state */
		gpio_write_pin(EC_FPR_OP_BOOT_LOGIN_N, 0);
#endif
		valid_transition = true;
#if CONFIG_SFH
		/* clear waie in S5 */
		waie_syc_flag = 0xD1;
#endif
		break;
	case SYSTEM_G3_STATE: /* G3 -> S5 */
		/* in case the pins were off in G3 */
		board_evalCardCtrlPins();
		valid_transition = true;
		break;
	default:
		/* No action */
		break;
	}

	/* sleep is allowed in S5 */
	task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 1);

	brdId_transitionToS5();

	/* clear WWAN and WLAN D3 cold flag in S5 */
	g_auxD3Cold = 0;

	/* Clear MS flag, it is expected the flag should be set on every normal boot
	 * Also, the QEvt queue can be cleard or not properly base on this flag.
	 */
	g_ui8MondernStandbySupport = 0;

	/* Set g_slideToShutdownEnabled to 0 everytime SoC enters S4/S5
	 * As thus, delay to send PwrBtn to FCH will not happen in BIOS phase.
	 * Expecting BIOS to set this flag on during POST.
	 */
	g_slideToShutdownEnabled = 0;

	/* Ensure postcode LEDs are off at first time entering S5 */
#if CONFIG_UDC_MANAGEMENT
	app_postcode_turnOff();
#endif
#if CONFIG_WIRELESS_MANAGEABILITY
	/* call manOs Enter S5 hook */
	app_manOs_EnterS5Hook();
#endif
#if CONFIG_BATTERY_MANAGEMENT
#if CONFIG_CHARGER_ISL9241
	dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
	amd_crb_drivers_mp2764_setProchot();
#endif
#endif

#if (CONFIG_USBC_CCGX)
	if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx)
		app_ccgx_updatePwrStatus(SYSTEM_S5_STATE);
#endif
#if CONFIG_USBC_UCSI_TUNNEL
	if (g_isUcsiCycleFinished == false) {
		g_isUcsiCycleFinished = true;
		LOG_DBG("Force to clear g_isUcsiCycleFinished");
	}
#endif
	return valid_transition;
}

/**
 * @brief power sequence transition to G3 handler
 */
static bool app_pseq_transitionToG3(void)
{
	bool valid_transition = false;

	switch (current_state) {
	case SYSTEM_S3_STATE: /* S3 -> G3 */
		valid_transition = true;
		break;
	case SYSTEM_S0_STATE: /* S0 -> G3 */
		app_pseq_exitS0();
		valid_transition = true;
#if CONFIG_SFH
		/* clear waie in G3 */
		waie_syc_flag = 0xD1;
#endif
		break;
	case SYSTEM_S5_STATE: /* S5 -> G3 */
#if CONFIG_WIRELESS_MANAGEABILITY
		/* call manOs Exit S5 hook */
		app_manOs_ExitS5Hook();
#endif
		valid_transition = true;
		break;
	case SYSTEM_INIT_STATE: /* Init -> G3 */
		valid_transition = true;
		break;
	default:
		/* No action */
		break;
	}

	/* sleep is allowed in G3 */
	task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 1);

#if CONFIG_VR_CONFIG_TABLE
	/*
	 * VR is powered by S5 rail so in G3 means it will definately exit low pwr mode on next S5 entering
	`*/
	board_vrCfg_exitLowPwrMode();
#endif
#if CONFIG_UDC_MANAGEMENT
	app_postcode_turnOff();
#endif
	LOG_WRN("[VCI]      app_vci_samplePwrBtn: %d", g_ui8PwrBtnVciFlag);
	if (g_ui8PwrBtnVciFlag) {
		g_ui8PwrBtnVciFlag = 0;
		app_pseq_nextStep(FROM_G3_TO_S0, 5);
	}
#if (CONFIG_USBC_CCGX)
	if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx)
		app_ccgx_updatePwrStatus(SYSTEM_G3_STATE);
#endif
#if CONFIG_USBC_UCSI_TUNNEL
	if (g_isUcsiCycleFinished == false) {
		g_isUcsiCycleFinished = true;
		LOG_DBG("Force to clear g_isUcsiCycleFinished");
	}
#endif
#if (CONFIG_USBC_4CC)
	amd_crb_drivers_tps_setSxAppConfig(TYPEC_PORT_0_IDX, SYSTEM_G3_STATE);
	amd_crb_drivers_tps_setSxAppConfig(TYPEC_PORT_2_IDX, SYSTEM_G3_STATE);
#endif
	return valid_transition;
}

/**
 * @brief power sequence need to update system status
 */
static void app_pseq_update(void)
{
	bool valid_sx_transition = false;

	LOG_DBG("PwrStChangeHandler: from %s to %s", system_power_state_str[current_state],
			system_power_state_str[next_state]);

	if (next_state != SYSTEM_S0_STATE && next_state != SYSTEM_S0_R_STATE) {
		_s_pwrStHadS3 = 0;
	}

	switch (next_state) {
	case SYSTEM_S0_STATE:
		valid_sx_transition = app_pseq_transitionToS0();
		break;
	case SYSTEM_S0_R_STATE:
		valid_sx_transition = app_pseq_transitionToS0R();
		break;
	case SYSTEM_S3_STATE:
		valid_sx_transition = app_pseq_transitionToS3();
		break;
	case SYSTEM_S4_STATE:
		valid_sx_transition = app_pseq_transitionToS4();
		break;
	case SYSTEM_S5_STATE:
		valid_sx_transition = app_pseq_transitionToS5();
		break;
	case SYSTEM_G3_STATE:
		valid_sx_transition = app_pseq_transitionToG3();
		break;
	default:
		break;
	}

	if (valid_sx_transition) {
		LOG_INF("System transition %s -> %s", system_power_state_str[current_state],
				system_power_state_str[next_state]);
		current_state = next_state;

		/* update GPIO setting based on power sequence */
		__autoGen_runtimeGpioSwitching(current_state);
	} else {
		LOG_ERR("Unsupported next state: %s", system_power_state_str[next_state]);
		/* Do not transition to invalid state,
		 * stay in current valid role.
		 */
		next_state = current_state;
	}
}

/**
 * @brief Routine that handles eSPI handshake with PMC.
 *
 * This routines also handles system events such as enter/exit S3/S4/S5 and
 * user events related to power/reset sequence.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void app_pseq_thread(void *p1, void *p2, void *p3)
{
	printk("In app_pseq_thread\r\n");
	uint32_t period = *(uint32_t *)p1;
	task_pseqSlpReady = (uint8_t*) p2;
	/* Can enter sleep before first circle */
	task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 1);
	app_pseq_task_init();

	while (true) {
		k_msleep(period);

		/* The main state machine for power sequence */
		if (true == g_pwrSeqExpired) {
			g_pwrSeqDelay = 0;
			g_pwrSeqExpired = false;
			app_pseq_StateMachine();
		}

		/* Run the delay for stage change */
		if (g_pwrSeqDelay) {
			/* cannot sleep during stage change */
			task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 0);
		} else {
			smchost_pwrbtn_post_signal_probe();
			/* monitor power sequence when without transaction */
			app_pseq_stateMonitor();
		}

		app_btn_monitor();
	}
}

/**
 * @brief power sequence exit S0 handler
 */
static void app_pseq_exitS0(void)
{
	/* Start of exit S0 */
#if CONFIG_UDC_MANAGEMENT
	app_postcode_turnOff();
#endif
#if CONFIG_WIRELESS_MANAGEABILITY
	/* manOS hook */
	app_manOs_ExitS0Hook();
#endif
	/* hook eSPI on-reset */
	espihub_onReset();

	/* turn off CHG_PROCHOTn interrupt */
	gpio_interrupt_configure_pin(CHG_EC_PROCHOT_N, GPIO_INT_DISABLE);
#if CONFIG_SOC_FAMILY_MEC
	gpio_write_pin(EC_APU_PROCHOT_N, 0);
#else
	gpio_write_pin(EC_APU_PROCHOT_N, 1);
#endif

#if CONFIG_HOT_BAG_EN
	/* clear critical zone if power_seq change from S0 to Sx*/
	g_powerSeq_criticalZone = false;
	k_timer_stop(&g_pmfHeartBeatTimer);
#endif
	/* End of exit S0 */
}

/**
 * @brief go to power state and call the correspending handler
 *
 * @param  st: system status
 */
void app_pseq_setPwrStatus(enum system_power_state st)
{
	if (next_state != st) {
		LOG_DBG("Changing PwrSt to %s", system_power_state_str[st]);
		dbgLogFifo_printf("P%d\n", st);
		next_state = st;

		/* Update pwrseq state when state change request is present
		 * and no power sequence failure.
		 */
		if ((next_state != current_state) && (!pwrseq_failure)) {
			app_pseq_update();
		}

		if (st == SYSTEM_G3_STATE)
			LOG_DBG("Done of PwrSt SYS_G3");
		else if (st >= SYSTEM_S3_STATE)
			LOG_DBG("Done of PwrSt SYS_S%d", (st - 1));
		else if (st == SYSTEM_S0_STATE)
			LOG_DBG("Done of PwrSt SYS_S0");
		else if (st == SYSTEM_S0_R_STATE)
			LOG_DBG("Done of PwrSt SYS_S0_R");
	}
}

/**
 * @brief return current system status
 *
 * @retval current system status
 */
enum system_power_state app_pseq_getPwrStatus(void)
{
	return current_state;
}

/**
 * @brief switch to next state machine delay callback
 *
 * @param  timer: pointer to timer structure
 */
static void app_pseq_delayCallback(struct k_timer *timer)
{
	g_pwrSeqExpired = true;
}

/**
 * @brief switch to next state machine step
 *
 * @param  next:   new status
 * @param  delay:  ms to delay
 */
void app_pseq_nextStep(enum system_power_seq_stage next, uint32_t delay)
{
	LOG_DBG("State transfer <%d, %d>", next, delay);
	/* cannot sleep during stage change */
	task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 0);
	g_pwrSeqDelay = delay;
	k_timer_start(&_s_pseqDelayTimerId,  K_MSEC(g_pwrSeqDelay), K_NO_WAIT);
	if (STATE_NO_CHANGE != next) {
		next_stage = next;
	}
}

/**
 * @brief if it is during the system status transition
 *
 * @retval True if busy
 */
bool app_pseq_isBusy(void)
{
	return g_pwrSeqDelay ? true : false;
}

/**
 * @brief monitor the EC_IOT_3V3_SENSE_EN signal to control the sb_tsi mux pin
 */
void app_sb_tsi_monitor(void)
{
#ifdef EC_IOT_3V3_SENSE_EN
#ifdef ex_EP_SB_TSI_ESP32n_EC_SEL
	static bool _s_last_pin_level = false;
	int level = gpio_read_pin(EC_IOT_3V3_SENSE_EN);

	if (level != _s_last_pin_level) {
		gpio_write_pin(ex_EP_SB_TSI_ESP32n_EC_SEL, level);
		_s_last_pin_level = level;
	}
#endif
#endif
}

/**
 * @brief Run in the power sequence thread to maintain state machine
 */
void app_pseq_StateMachine(void)
{
	switch (next_stage) {
/*
 * EC power sequence mechanism in thread
 */
		case POWER_INIT:
			app_pseq_nextStep(SETUP_GPIO_DEFAULT, 1);
			break;

		case SETUP_GPIO_DEFAULT:
			app_pseq_nextStep(ENTER_TO_G3, 10);
			break;

		case ENTER_TO_G3:
			app_pseq_setPwrStatus(SYSTEM_G3_STATE);
			break;
/*
 * G3 -> S5
 */
		case FROM_G3_TO_S5:
			gpio_write_pin(EC_S5_PWREN, 1);
			gpio_write_pin(EC_1V8_S5_EN, 1);
#ifdef EC_USB32_RD_RST_N
			gpio_write_pin(EC_USB32_RD_RST_N, 1);
#endif
#ifdef EC_USB32_RD_EN
			gpio_write_pin(EC_USB32_RD_EN, 1);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			ioexp_set(ex_EP_SENSOR_3V3_PWREN, 1);
#endif

			if (gpio_read_pin(SYSTEM_S5_PG)) {
				app_pseq_nextStep(PWRGD_ASSERTED, 20);
			} else {
				app_pseq_nextStep(STATE_NO_CHANGE, 1);
			}
			break;

		case PWRGD_ASSERTED:
			gpio_write_pin(RSMRST_N, 0);
			gpio_write_pin(EC_S5_PWREN, 1);
			gpio_write_pin(EC_1V8_S5_EN, 1);
#ifdef EC_USB32_RD_RST_N
			gpio_write_pin(EC_USB32_RD_RST_N, 1);
#endif
#ifdef EC_USB32_RD_EN
			gpio_write_pin(EC_USB32_RD_EN, 1);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			ioexp_set(ex_EP_SENSOR_3V3_PWREN, 1);
#endif

#if CONFIG_SOC_SERIES_NPCX4
			amd_crb_drivers_spi_reset();
#endif
			/* PD power role adjust when enter S5 */
			brdId_pdPowerRoleS5();
#if CONFIG_IO_EXPANDER
			/*
			 * IO expander initialization
			 * After SYSTEM_ALW_PG goes high and prior to release RSMRST# (by Endy, 2020/12/07)
			 */
			board_ioexp_initIoExp(); /* The only request of TPM_S0I3# is to set it as input before EC is going to drive EC_SLP_S5_L High */
			                         /* As it is OD_1 so this is satisfied, it will be OD_0 until BIOS set it (before hand over to OS) */
#endif
#if CONFIG_ALS
		/* need to init the als sensor because it power rail is S5 */
		/* Initialize on first run */
		if (!app_opt4048_init()) {
			LOG_ERR("OPT4048 initialization failed, thread terminating");
		}
#endif
			app_pseq_nextStep(RELEASE_RSMRST, 1);
			break;

		case RELEASE_RSMRST:
#if CONFIG_VR_CONFIG_TABLE
			/* Do SVI3 config before release RSMRSTn */
			_s_vrNeedG3 = board_vrCfg_vr_init();
#endif
			/* Init eSPI */
			if (!init_espi) {
				LOG_INF("eSPI init!");
				int ret = espihub_init();
				if (ret) {
					LOG_ERR("Failed to init espi %d", ret);
				}
				init_espi = true;
			}
#if CONFIG_VR_CONFIG_TABLE
			if (_s_vrNeedG3 && _s_vrNeedG3_cnt < 3) {
				_s_vrNeedG3 = false;
				_s_vrNeedG3_cnt++;
				LOG_WRN("VR updated, need to enter G3, try %d!", _s_vrNeedG3_cnt);
				app_pseq_nextStep(FROM_S5_TO_G3, 1);
			} else {

				_s_vrNeedG3_cnt = 0;
				LOG_INF("RSMRST_N rise!");
				gpio_write_pin(RSMRST_N, 1);
				app_pseq_setPwrStatus(SYSTEM_S5_STATE);
				/* Requirement from PEO:
				 *   1. needs to auto power on system from G3
				 *   2. stay in S5 if shutdown from S0
				 */
				if (BRDID_isPEO) {
					app_pseq_nextStep(FROM_S5S3_TO_S0, 100);
				}
			}
#else
			LOG_INF("RSMRST_N rise!");
			gpio_write_pin(RSMRST_N, 1);
			app_pseq_setPwrStatus(SYSTEM_S5_STATE);
			/* Requirement from PEO:
			 *   1. needs to auto power on system from G3
			 *   2. stay in S5 if shutdown from S0
			 */
			if (BRDID_isPEO) {
				app_pseq_nextStep(FROM_S5S3_TO_S0, 100);
			}
#endif
			break;
/*
 * G3 -> S0
 */
 		case FROM_G3_TO_S0:
 			gpio_write_pin(RSMRST_N, 0);
 			gpio_write_pin(EC_S5_PWREN, 1);
 			gpio_write_pin(EC_1V8_S5_EN, 1);
#ifdef EC_USB32_RD_RST_N
			gpio_write_pin(EC_USB32_RD_RST_N, 1);
#endif
#ifdef EC_USB32_RD_EN
			gpio_write_pin(EC_USB32_RD_EN, 1);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			ioexp_set(ex_EP_SENSOR_3V3_PWREN, 1);
#endif

			if (gpio_read_pin(SYSTEM_S5_PG)) {
				app_pseq_nextStep(PWRGD_ASSERTED_IN_DC_MODE, 20);
			} else {
				app_pseq_nextStep(STATE_NO_CHANGE, 1);
			}
			break;
		case PWRGD_ASSERTED_IN_DC_MODE:
			gpio_write_pin(EC_S5_PWREN, 1);
			gpio_write_pin(EC_1V8_S5_EN, 1);
#ifdef EC_USB32_RD_RST_N
			gpio_write_pin(EC_USB32_RD_RST_N, 1);
#endif
#ifdef EC_USB32_RD_EN
			gpio_write_pin(EC_USB32_RD_EN, 1);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			ioexp_set(ex_EP_SENSOR_3V3_PWREN, 1);
#endif

#if CONFIG_SOC_SERIES_NPCX4
			amd_crb_drivers_spi_reset();
#endif
			/* PD power role adjust when enter S5 */
			brdId_pdPowerRoleS5();
#if CONFIG_IO_EXPANDER
			/*
			 * IO expander initialization
			 * After SYSTEM_ALW_PG goes high and prior to release RSMRST# (by Endy, 2020/12/07)
			 */
			board_ioexp_initIoExp();
#endif
#if CONFIG_ALS
		/* need to init the als sensor because it power rail is S5 */
		/* Initialize on first run */
		if (!app_opt4048_init()) {
			LOG_ERR("OPT4048 initialization failed, thread terminating");
		}
#endif
			app_pseq_nextStep(RELEASE_RSMRST_IN_DC_MODE, 20);
			break;

		case RELEASE_RSMRST_IN_DC_MODE:
#if CONFIG_VR_CONFIG_TABLE
			/* Do SVI3 config before release RSMRSTn */
			_s_vrNeedG3 = board_vrCfg_vr_init();
#endif
			/* Init eSPI */
			if (!init_espi) {
				LOG_INF("eSPI init!");
				int ret = espihub_init();
				if (ret) {
					LOG_ERR("Failed to init espi %d", ret);
				}
				init_espi = true;
			}
			LOG_INF("RSMRST_N rise!");
#if CONFIG_VR_CONFIG_TABLE
			if (_s_vrNeedG3 && _s_vrNeedG3_cnt < 3) {
				_s_vrNeedG3 = false;
				_s_vrNeedG3_cnt++;
				LOG_WRN("VR updated, need to enter G3, try %d!", _s_vrNeedG3_cnt);
				app_pseq_nextStep(FORCE_RESET, 30);
			} else {
				_s_vrNeedG3_cnt = 0;
				gpio_write_pin(RSMRST_N, 1);
				app_pseq_nextStep(FROM_S5S3_TO_S0, 30); // In DC case, apply 30ms delay after RSMRSTn to 1, so
				// RTCCLK can stable before next step
			}
#else
			gpio_write_pin(RSMRST_N, 1);
			app_pseq_nextStep(FROM_S5S3_TO_S0, 30); // In DC case, apply 30ms delay after RSMRSTn to 1, so
			// RTCCLK can stable before next step
#endif
			break;
			/*
			 * S5 -> G3
			 */
		case FROM_S5_TO_G3:
#ifdef ex_EP_SB_TSI_ESP32n_EC_SEL
			ioexp_set(ex_EP_SB_TSI_ESP32n_EC_SEL, 0);
#endif
			gpio_write_pin(EC_SLP_S5_N, 0);
			gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#ifdef EC_S0_LED
			gpio_write_pin(EC_S0_LED, 0);
#endif
			gpio_write_pin(RSMRST_N, 0);
			app_pseq_nextStep(FROM_S5_TO_G3_Part2, 10);
			break;

		case FROM_S5_TO_G3_Part2:
#ifdef EC_USB32_RD_RST_N
			gpio_write_pin(EC_USB32_RD_RST_N, 0);
#endif
#ifdef EC_USB32_RD_EN
			gpio_write_pin(EC_USB32_RD_EN, 0);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			ioexp_set(ex_EP_SENSOR_3V3_PWREN, 0);
#endif

			gpio_write_pin(EC_1V8_S5_EN, 0);

			app_pseq_nextStep(FROM_S5_TO_G3_Part3, 20);

			break;

		case FROM_S5_TO_G3_Part3:
			gpio_write_pin(EC_S5_PWREN, 0);
			app_pseq_setPwrStatus(SYSTEM_G3_STATE);
			break;
/*
 * S5 -> S0
 */
		/* SoC may be waken up by other reason asynchronously */
		case FROM_S5S3_TO_S0:
			/* if apply a 20ms FCH_PWRBTN here. Hence, use a side function to generate the pulse.
			 * And jump to next status to start to monitor SLP_S3/SLP_S5
			 * */
			app_btn_triggerPwrBtn(20);
			app_pseq_nextStep(FROM_S3_TO_S0, 1);
			break;
/*
 * S3 -> S0
 */
 		case FROM_S3_TO_S0:
 			if (!SLP_S3_ASSERT && !SLP_S5_ASSERT) {
#if CONFIG_BATTERY_MANAGEMENT
				app_smtbty_frozen();
#endif
				board_turnOn_espi();
 				gpio_write_pin(EC_SLP_S5_N, 1);
 				gpio_write_pin(EC_SLP_S3_S0A3_N, 1);
#ifdef ex_EP_SB_TSI_ESP32n_EC_SEL
				ioexp_set(ex_EP_SB_TSI_ESP32n_EC_SEL, 1);
#endif
#ifdef EC_S0_LED
				gpio_write_pin(EC_S0_LED, 1);
#endif
				/* DDR5 PwrCtrl */
				brdId_ctrl_Ddr5SidimmPwrEn(1);
				if (brdId_supportMemPMIC()) {
#ifdef ex_EP_MEM_1V0_PWREN
					ioexp_set(ex_EP_MEM_1V0_PWREN, 1);
#endif
#ifdef ex_EP_MEM_I2C_EN
					ioexp_set(ex_EP_MEM_I2C_EN, 1);
#endif
				}
				app_pseq_setPwrStatus(SYSTEM_S0_R_STATE);
				if (brdId_supportMemPMIC()) {
#ifdef ex_MEM_PMIC_PWR_GOOD
						g_LPCAMM_PWR_EN = 1;
#endif
				}
				app_pseq_nextStep(TRANSFERED_INTO_S0, 100);

				_s_slpS35WaitCnt = 0;

 			} else {
				/* Power sequence thread sleep every 1ms counter step size need to adjust based on it */
 				_s_slpS35WaitCnt += 1;
				app_pseq_nextStep(STATE_NO_CHANGE, 1);

				/* For any reason the PwrBtn pulse is not taking effect,
				 * Jump back to S5 delay 2s
				 */
				if (_s_slpS35WaitCnt > 2000) {
					/* record the power missing message and report in ECRAM for debug purpose */
					g_ui32_pwrBtnMissCnt ++;
					_s_slpS35WaitCnt = 0;

					/* set to 0 before rollback */
					brdId_ctrl_Ddr5SidimmPwrEn(0);
					app_pseq_nextStep(FROM_S0_TO_S3S5, 1);
				}
			}
 			break;

 		case TRANSFERED_INTO_S0:
			/* Set DEPOP_MUTEn to HIGH */
#ifdef ex_EP_HDR_DEPOP_MUTEn
 			ioexp_set(ex_EP_HDR_DEPOP_MUTEn, 1);
#endif
#ifdef ex_EP_CODEC_DEPOP_MUTEn
 			ioexp_set(ex_EP_CODEC_DEPOP_MUTEn, 1);
#endif
#ifdef EC_CODEC_DEPOP_MUTE_N
 			gpio_write_pin(EC_CODEC_DEPOP_MUTE_N, 1);
#endif
			app_pseq_setPwrStatus(SYSTEM_S0_STATE);
 			break;
/*
 * S0 -> S3/S5
 */
		case FROM_S0_TO_S3S5:
#if CONFIG_FAN_RPM_CONTROL
			app_fan_ctrl_sys_disable();
#endif
#ifdef ex_EP_HDR_DEPOP_MUTEn
 			ioexp_set(ex_EP_HDR_DEPOP_MUTEn, 0);
#endif
#ifdef ex_EP_CODEC_DEPOP_MUTEn
 			ioexp_set(ex_EP_CODEC_DEPOP_MUTEn, 0);
#endif
#ifdef EC_CODEC_DEPOP_MUTE_N
 			gpio_write_pin(EC_CODEC_DEPOP_MUTE_N, 0);
#endif
 			if (SLP_S5_ASSERT) {
 				/* S0 -> S5 */
 				app_pseq_nextStep(FROM_S0_TO_S5, 1);
 			} else {
 				 /* S0 -> S3 */
 				gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#ifdef EC_S0_LED
				gpio_write_pin(EC_S0_LED, 0);
#endif
				app_pseq_setPwrStatus(SYSTEM_S3_STATE);
 			}
 			break;

 		case FROM_S0_TO_S5:
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
			/* A workaround to fix an +3V3_PWM/+3V3_EC OVP issue during system shutdown in MDS Dorne,
             * the system can't be recovered unless plug out PD adapter or battery
             */
			gpio_write_pin(EC_SLP_S5_N, 0);
#else
			gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#endif
			if (brdId_supportMemPMIC()) {
#ifdef ex_MEM_PMIC_VR_EN_3V3
				/* needs to assert DDR5 VR_EN more than 1ms then de-assert after when enter S5  */
				ioexp_set(ex_MEM_PMIC_VR_EN_3V3, 0);
#endif /* MEM_PMIC_VR_EN_3V3 */
				app_pseq_nextStep(FROM_S0_TO_S5_MEM_PMIC, 1);
			}
			else {
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
				/* A workaround to fix an +3V3_PWM/+3V3_EC OVP issue during system shutdown in MDS Dorne */
				app_pseq_nextStep(GOING_TO_S5, 20);
#else
				app_pseq_nextStep(GOING_TO_S5, 1);
#endif
			}
			break;

		case FROM_S0_TO_S5_MEM_PMIC:
#ifdef ex_MEM_PMIC_PWR_GOOD
			if (!ioexp_get(ex_MEM_PMIC_PWR_GOOD)) {
				app_pseq_nextStep(GOING_TO_S5, 200);
			} else {
				app_pseq_nextStep(STATE_NO_CHANGE, 1);
			}
#endif
			break;

		case GOING_TO_S5:
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
			/* A workaround to fix an +3V3_PWM/+3V3_EC OVP issue during system shutdown in MDS Dorne */
			gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#else
			gpio_write_pin(EC_SLP_S5_N, 0);
#endif

#ifdef EC_S0_LED
			gpio_write_pin(EC_S0_LED, 0);
#endif

#if CONFIG_VR_CONFIG_TABLE
			/*
			 * Try to setup VDDCORE when enters S5
			 * (After S0 power is off, as thus even SoC bouncing up this will done before EC turn on S0 power for next run)
			 */
			board_vrCfg_setupVddCore();
#endif
			brdId_ctrl_Ddr5SidimmPwrEn(0);
			app_pseq_setPwrStatus(SYSTEM_S5_STATE);
 			break;
/*
 * S0 -> G3 -> S0
 */
 		case FORCE_RESET:
#if CONFIG_FAN_RPM_CONTROL
			 app_fan_ctrl_sys_disable();
#endif
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
			 gpio_write_pin(EC_SLP_S5_N, 0);
			 app_pseq_nextStep(FORCE_RESET_Part1, 20);
#else
			 gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
			 app_pseq_nextStep(FORCE_RESET_Part1, 1);
#endif
			break;

		case FORCE_RESET_Part1:
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
			gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#else
			gpio_write_pin(EC_SLP_S5_N, 0);
#endif

#ifdef ex_EP_SB_TSI_ESP32n_EC_SEL
			ioexp_set(ex_EP_SB_TSI_ESP32n_EC_SEL, 0);
#endif
#ifdef EC_S0_LED
			gpio_write_pin(EC_S0_LED, 0);
#endif
			gpio_write_pin(EC_S5_PWREN, 0);

			app_pseq_nextStep(FORCE_RESET_Part2, 20);
			break;

		case FORCE_RESET_Part2:
#ifdef EC_USB32_RD_EN
			gpio_write_pin(EC_USB32_RD_EN, 0);
#endif
#ifdef EC_USB32_RD_RST_N
			gpio_write_pin(EC_USB32_RD_RST_N, 0);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			ioexp_set(ex_EP_SENSOR_3V3_PWREN, 0);
#endif

			/* Separated as two parts on Lilac/Mayan
			 * to insert 20ms delay before turning off
			 * EC_1_8V_ALW_EN to workaround CPLD glitch.
			 */
			gpio_write_pin(EC_1V8_S5_EN, 0);
			gpio_write_pin(RSMRST_N, 0);
			app_pseq_setPwrStatus(SYSTEM_G3_STATE);
			app_pseq_nextStep(FORCE_RESET_G3_TO_S5, 1000);
			break;

		case FORCE_RESET_G3_TO_S5:
			gpio_write_pin(EC_S5_PWREN, 1);
			gpio_write_pin(EC_1V8_S5_EN, 1);
#ifdef EC_USB32_RD_RST_N
			gpio_write_pin(EC_USB32_RD_RST_N, 1);
#endif
#ifdef EC_USB32_RD_EN
			gpio_write_pin(EC_USB32_RD_EN, 1);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			ioexp_set(ex_EP_SENSOR_3V3_PWREN, 1);
#endif

			if (gpio_read_pin(SYSTEM_S5_PG)) {
				app_pseq_nextStep(FORCE_RESET_PWRGD_ASSERTED, 20);
			} else {
				app_pseq_nextStep(STATE_NO_CHANGE, 1);
			}
			break;

		case FORCE_RESET_PWRGD_ASSERTED:
#if CONFIG_SOC_SERIES_NPCX4
			amd_crb_drivers_spi_reset();
#endif
			/* PD power role adjust when enter S5 */
			brdId_pdPowerRoleS5();
#if CONFIG_IO_EXPANDER
			/*
			 * IO expander initialization
			 * After SYSTEM_ALW_PG goes high and prior to release RSMRST# (by Endy, 2020/12/07)
			 */
			board_ioexp_initIoExp(); /* The only request of TPM_S0I3# is to set it as input before EC is going to drive EC_SLP_S5_L High */
									 /* As it is OD_1 so this is satisfied, it will be OD_0 until BIOS set it (before hand over to OS) */
#endif
			gpio_write_pin(RSMRST_N, 1);
			app_pseq_setPwrStatus(SYSTEM_S5_STATE);
			app_pseq_nextStep(FROM_S5S3_TO_S0, 200);
			break;
/*
 * Sx -> G3 -(if has AC)-> S5
 */
		 case FORCE_G3:
#if CONFIG_FAN_RPM_CONTROL
			 app_fan_ctrl_sys_disable();
#endif
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
			 gpio_write_pin(EC_SLP_S5_N, 0);
			 app_pseq_nextStep(FORCE_G3_Part1, 20);
#else
			 gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
			 app_pseq_nextStep(FORCE_G3_Part1, 1);
#endif
			 break;

		  case FORCE_G3_Part1:
#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
			  gpio_write_pin(EC_SLP_S3_S0A3_N, 0);
#else
			  gpio_write_pin(EC_SLP_S5_N, 0);
#endif

			  gpio_write_pin(RSMRST_N, 0);
#ifdef EC_S0_LED
			  gpio_write_pin(EC_S0_LED, 0);
#endif
#ifdef ex_EP_SB_TSI_ESP32n_EC_SEL
			  ioexp_set(ex_EP_SB_TSI_ESP32n_EC_SEL, 0);
#endif
			  app_pseq_nextStep(FORCE_G3_Part2, 10);
			  break;

		 case FORCE_G3_Part2:
#ifdef EC_USB32_RD_EN
			 gpio_write_pin(EC_USB32_RD_EN, 0);
#endif
#ifdef EC_USB32_RD_RST_N
			 gpio_write_pin(EC_USB32_RD_RST_N, 0);
#endif
#ifdef ex_EP_SENSOR_3V3_PWREN
			 ioexp_set(ex_EP_SENSOR_3V3_PWREN, 0);
#endif

			 gpio_write_pin(EC_1V8_S5_EN, 0);

			 app_pseq_nextStep(FORCE_G3_Part3, 20);

			 break;

		 case FORCE_G3_Part3:
			 gpio_write_pin(EC_S5_PWREN, 0);
			/* Update PwrStatus as G3 */
			 app_pseq_setPwrStatus(SYSTEM_G3_STATE);
			/* Force off 3000ms in G3 */
			 app_pseq_nextStep(ENTER_TO_G3, 3000);
			break;

		default:
			break;
	}
}

/**
 * @brief system status monitor and do the correspending react.
 */
void app_pseq_stateMonitor(void)
{
	enum system_power_state pwrSt = app_pseq_getPwrStatus();

	if (SYSTEM_S4_STATE != pwrSt && SYSTEM_S5_STATE != pwrSt) {
		_s_gotoG3WaitCnt = POWER_SEQ_S5_TO_G3_DELAY;
	}

	switch (pwrSt) {
		case SYSTEM_INIT_STATE:
			break;
		case SYSTEM_G3_STATE:
#if CONFIG_POWER_SOURCE_MANAGEMENT
#if STANDBY_WITH_G3_POWER
			/* do nothing here */
#else
			if (board_pwrSrc_hasAcPowerSource() || g_S5AlwEnFlag) {
				/* AC present */
				app_pseq_nextStep(FROM_G3_TO_S5, 500);
			}

			/* Handle the special cases for cold reboot */
			if (F_BRAM_FLASH_BIOS_WDT_BOOT_SIGNATURE == f_bram_bootReason()) {
				f_bram_bootReasonClear();
				/* boot to S5 after bios flash update by tool */
				app_pseq_nextStep(FROM_G3_TO_S5, 500);
			} else if (F_BRAM_NV_OPTION_UPDATE_SIGNATURE == f_bram_bootReason()) {
				f_bram_bootReasonClear();
				/* boot to S0 after NV option update force reset update */
				app_pseq_nextStep(FROM_G3_TO_S0, 5);
			}
#endif /* STANDBY_WITH_G3_POWER */
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */
			break;
		case SYSTEM_S0_STATE:
			if (SLP_S3_ASSERT) {
				app_pseq_nextStep(FROM_S0_TO_S3S5, 1);
			} else {
				app_sb_tsi_monitor();
			}
			break;
		case SYSTEM_S3_STATE:
			/* S3 -> S0 */
			if (!SLP_S3_ASSERT) {
				app_pseq_nextStep(FROM_S3_TO_S0, 1);
			}
			/* S3 -> S5; In case the SLP_S5n/SLP_S3_S0A3n sent from APU is mis-ordered. */
			if (SLP_S5_ASSERT) {
				app_pseq_nextStep(FROM_S0_TO_S5, 1);
			}
			break;
		case SYSTEM_S4_STATE:
		case SYSTEM_S5_STATE:
 			if (!SLP_S3_ASSERT && !SLP_S5_ASSERT) {
 				app_pseq_nextStep(FROM_S3_TO_S0, 1);
				break;
 			}

#if CONFIG_POWER_SOURCE_MANAGEMENT
#if (!STANDBY_WITH_G3_POWER)
			if (!board_pwrSrc_hasAcPowerSource() && !g_S5AlwEnFlag) {
#endif
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */
				/* sleep is not allowed in S4/5 DC mode */
				task_status_set(task_pseqSlpReady, TASK_STASTUS_SLEEP_READY, 0);

				/*
				 * Apply a S5 to G3 delay for
				 *   1. Keep mosfet Uxx on to allow +12V_RUN power rail discharge
				 *   2. debounce CHG_ACOK.
				 *      For example both AC, PD were connected but powered by AC,
				 *      then a removing of AC can cause system be in G3 for very short period,
				 *      which is not necessary and may cause USBC Alt. mode issue.
				*/
				if (_s_gotoG3WaitCnt > 1) {
					_s_gotoG3WaitCnt -= 1;
					break;
				} else {
					_s_gotoG3WaitCnt = 0;
				}

				app_pseq_nextStep(FROM_S5_TO_G3, 1);

				brdId_pdPowerRoleG3();

#if CONFIG_POWER_SOURCE_MANAGEMENT
#if (!STANDBY_WITH_G3_POWER)
			} else {
				_s_gotoG3WaitCnt = POWER_SEQ_S5_TO_G3_DELAY;
			}
#endif
#endif /* CONFIG_POWER_SOURCE_MANAGEMENT */
			break;
		default:
			break;
	}
}