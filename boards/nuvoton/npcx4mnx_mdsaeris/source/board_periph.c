/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include "periphmgmt.h"
#include "app_btn.h"
#include "board_config.h"
#include "acpi_region.h"
#include "smchost.h"
#include "app_usbc_task.h"
#include "app_ucsi_tunnel.h"
#if (CONFIG_USBC_RTK)
#include "app_realtek_pd.h"
#endif
#include "app_pseq.h"
#include "dev_pca9535.h"
#include "board_pwrSrc.h"
#include "board_smtbty.h"
#include "app_udc.h"
#include "app_acpi.h"
#include "board_init.h"
#include "task_handler.h"
#include "dbgLogFifo.h"
#if CONFIG_SLEEP
#include "board_sleep.h"
#endif
#include <zephyr/drivers/pwm.h>
#include "board_fanRpmCtrl.h"
#include "f_nv_options.h"

LOG_MODULE_REGISTER(brd_periph, CONFIG_BOARD_PERIPH_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
/* Debouncing is performed in 1 ms intervals.
 * Maximum debounce count is the debounce time specified in milliseconds.
 */
#define BTN_DEBOUNCE_CNT	         CONFIG_PERIPHERAL_BTN_DEBOUNCE_TIME
#define UART_PRSENT_DEBOUNCE_CNT     CONFIG_PERIPHERAL_UART_DONGLE_DEBOUNCE_TIME
#define BAT_IN_DEBOUNCE_CNT          CONFIG_PERIPHERAL_BAT_INT_DEBOUNCE_TIME
#define CHG_OK_DEBOUNCE_CNT          CONFIG_PERIPHERAL_CHG_OK_DEBOUNCE_TIME
#define LID_DEBOUNCE_CNT             CONFIG_PERIPHERAL_LID_DEBOUNCE_TIME
#define DBG_CARD_DEBOUNCE_CNT        CONFIG_PERIPHERAL_DBG_CARD_DEBOUNCE_TIME
#define DEBOUNCE_CNT                 (2)

/* ************************** *
 *     extern valuable        *
 * ************************** */
extern uint32_t g_u32ApmlErrCnt;
extern uint8_t g_u8apmlDelayCnt;
extern uint8_t g_uPepHook;
extern uint8_t g_aslGpFlags;
extern uint8_t g_apuRstS3Flag;
extern uint32_t g_u32ApuRstCnt;
extern uint32_t g_u32ThermalTripCnt;
extern uint32_t g_u32ChgProchotLCnt;
extern uint8_t g_u8EcStateFlags;
/* task slp ready */
extern uint8_t *task_periphSlpReady;

/* ************************** *
 *     static valuable        *
 * ************************** */
static uint8_t _s_InputBufferLidSt = 0;
static uint8_t _s_InputBufferLidLastSend = 0;

static struct k_timer _s_thermtripTimer;
static void _thermtripTimerCallback(struct k_timer *timer);

/** ************************* *
 *     global valuable        *
 * ************************** */
/* All the native interrupt gpio list and debounce xx * 1ms*/
struct inputBuffer_info inputBuffer_lst[] = {
	{HW_PWR_BTN_N,   		false, BTN_DEBOUNCE_CNT,             NULL, PWR_BTN_INIT_POS,
			{{0}, NULL, 0}, "PwrBtn"},
	{LID_CLOSE_N_3V3,  	    false, LID_DEBOUNCE_CNT,     	     NULL, LID_INIT_POS,
			{{0}, NULL, 0}, "LidClose"},
	{BAT_PRSNT_N,         		false, BAT_IN_DEBOUNCE_CNT,      NULL, BAT_IN_INIT_POS,
			{{0}, NULL, 0}, "BatIn"},
	{CHG_ACOK,         		false, CHG_OK_DEBOUNCE_CNT,          NULL, CHG_OK_INIT_POS,
			{{0}, NULL, 0}, "ChgOk"},
	{CHG_EC_PROCHOT_N,         false, 0,                         NULL, CHG_PROCHOT_INIT_POS,
			{{0}, NULL, 0}, "ChgProchot"},
	{APU_EC_RESET_N,             false, 0,                       NULL, APU_RST_INIT_POS,
			{{0}, NULL, 0}, "ApuRst"},
	{APU_PWROK,             false, 0,                            NULL, APU_PWROK_INIT_POS,
			{{0}, NULL, 0}, "ApuPwrOk"},
	{APU_THERMTRIP_N,        false, 0,                           NULL, APU_THERMALTRIP_INIT_POS,
			{{0}, NULL, 0}, "ApuThermalTrip"},
	{APU_EC_ALERT_N,           false, 0,                         NULL, APU_ALERT_INIT_POS,
			{{0}, NULL, 0}, "ApuAlert"},
	{SLP_S3_S0A3_N,           		false, 0,                    NULL, SLP_S3_INIT_POS,
			{{0}, NULL, 0}, "SlpS3"},
	{SLP_S5_N,           		false, 0,                        NULL, SLP_S5_INIT_POS,
			{{0}, NULL, 0}, "SlpS5"},
	{PD0_EC_USBC_INT_N,       false, 0,                          NULL, USB1_INT_INIT_POS,
			{{0}, NULL, 0}, "Usb1Int"},
	{PD1_EC_USBC_INT_N,      	false, 0,                        NULL, USB2_INT_INIT_POS,
			{{0}, NULL, 0}, "Usb2Int"},
	{SYSTEM_S5_PG,      	false, 0,                            NULL, ALW_GD_INIT_POS,
			{{0}, NULL, 0}, "AlwPg"},
	{DBG_CARD_PRSNT_N, false, DBG_CARD_DEBOUNCE_CNT,             NULL, DBG_CARD_INIT_POS,
			{{0}, NULL, 0}, "DbgCard"},
	{WLAN_IO33_N_IO18_SEL, false, 0,                             NULL, 0,
			{{0}, NULL, 0}, "WlanSel"},
	{SFH_EC_INT_N_R, false, 0,                                    NULL, 0,
			{{0}, NULL, 0}, "SfhInt"},
	{ALS_INT_N, false, 0,                                         NULL, 0,
			{{0}, NULL, 0}, "AlsInt"},
	{EC_WLAN_I2C_ALERT_N_1V8, false, 0,                           NULL, 0,
			{{0}, NULL, 0}, "WlanAlert"},
};
uint16_t gpio_cb_list_len = 0, inputBuffer_lst_len = 0;

#if CONFIG_HOT_BAG_EN
bool g_lidCloseEnabled = false;  /* false: LID_OPEN and true: LID_CLOSE */
#endif

/* Functions for inputbuffer handles */
#define LOG_PWRSEQ_PIN_STATUS(s, x)       _dumpPinSt(s, x);

/**
 * @brief Dump pin status for debugging purposes.
 *
 * @param st: status value
 * @param pMsg: message string
 */
void _dumpPinSt(uint8_t st, const char * pMsg)
{
	uint32_t pinSt = 0;
#if (CONFIG_PERIPHERAL_LOG_LEVEL)
	pinSt |= (gpio_read_pin(APU_EC_RESET_N)   ? 0x0001 : 0);
	pinSt |= (gpio_read_pin(APU_PWROK)        ? 0x0002 : 0);
	pinSt |= (gpio_read_pin(SYSTEM_S5_PG)    ? 0x0004 : 0);
	pinSt |= (gpio_read_pin(SLP_S3_S0A3_N)    ? 0x0008 : 0);

	pinSt |= (gpio_read_pin(SLP_S5_N)         ? 0x0010 : 0);
	pinSt |= (gpio_read_pin(HW_PWR_BTN_N)     ? 0x0020 : 0);
	pinSt |= (gpio_read_pin(APU_THERMTRIP_N)   ? 0x0040 : 0);
	pinSt |= (gpio_read_pin(APU_EC_ALERT_N)      ? 0x0080 : 0);

	pinSt |= (gpio_read_pin(RSMRST_N)        ? 0x0100 : 0);
	pinSt |= (gpio_read_pin(EC_S5_PWREN)      ? 0x0200 : 0);
	pinSt |= (gpio_read_pin(EC_1V8_S5_EN)     ? 0x0400 : 0);
	pinSt |= (gpio_read_pin(EC_SLP_S3_S0A3_N) ? 0x0800 : 0);

	pinSt |= (gpio_read_pin(EC_SLP_S5_N)      ? 0x1000 : 0);
	pinSt |= (gpio_read_pin(EC_PWR_BTN_N)     ? 0x2000 : 0);
#else
	return;
#endif
	LOG_DBG("(%x) S3f-%d, Pst-%d in %s", pinSt, g_apuRstS3Flag, st, pMsg ? pMsg : "n\a");
}

/***********************************************************************/
/****** native gpio hardware interrupt handler callback functions ******/
/***********************************************************************/
/**
 * @brief Callback to handle APU_RST change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_apu_rst_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	uint8_t level = gpio_read_pin(APU_EC_RESET_N);
	LOG_PWRSEQ_PIN_STATUS(level, info->name);

	LOG_ERR("APURST: %x", level);

	if (!level) {
		ACPI_DisableHostNotify();

		if (!g_ui8MondernStandbySupport) {
			ACPI_ClearQueuedEvts();
		}

		/* enable apu reset handler timer with "APU_RST_MINIMUM_PULSE" ms delay */
		board_init_apuRstTimerEnable(APU_RST_MINIMUM_PULSE);

		g_apuRstS3Flag = 0;

		g_u32ApmlErrCnt = 0;
		g_u8apmlDelayCnt = 0;
		g_u32ApuRstCnt++;

		if (g_ui8MondernStandbySupport &&
		    (MS_uPEP_HOOK_DISPLAY_OFF == g_uPepHook ||
			 MS_uPEP_HOOK_DRIPs_ENTER == g_uPepHook ||
			 MS_uPEP_HOOK_MS_ENTER == g_uPepHook )) {
			/* non-KBC wake - ENKB + EMBF(100) */
			g_aslGpFlags = 0x42;
		} else {
			g_aslGpFlags = 0x0;
		}
	} else {
		/**
		* Clear g_slyType while APU_RST goes to high
		* Platform should set this variable everytime it prepares to sleep.
		*
		* Assume no APU RST during S4 resume
		*/
		g_slyType = 0;
	}
}

/**
 * @brief Callback to handle APU_PWROK change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_apu_pwrok_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	uint8_t level = gpio_read_pin(APU_PWROK);
	LOG_PWRSEQ_PIN_STATUS(level, info->name);
	dbgLogFifo_printf("apu_pwrok: %d\n", level);
	if (level && !SLP_S3_ASSERT && !SLP_S5_ASSERT ) {
		board_onApuPwrOK();
	}
	LOG_ERR("PWROK: %x", level);
}

/**
 * @brief Timer callback for thermtrip detection timeout.
 *
 * @param timer: timer structure pointer
 */
static void _thermtripTimerCallback(struct k_timer *timer)
{
	/* if thermtrip timer timeout, it means there is no thermtrip event detected */
	k_timer_stop(&_s_thermtripTimer);
}

/**
 * @brief Callback to handle APU_THERMALTRIP change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_apu_thermal_trip_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	uint8_t level = gpio_read_pin(APU_THERMTRIP_N);
	if((SLP_S3_ASSERT)||(SLP_S5_ASSERT)) {
		return; /* only care about thermal trip event in S0 */
	}
	LOG_PWRSEQ_PIN_STATUS(level, info->name);
	/* Nuvoton APU_THERMTRIP_N is low active */
	if (!level) {
		/* THERMTRIP# de-assert: system enters S0 or detects thermtrip event low pulse
		   Enable the timer to filter enter S0 case.
		   Normally, when system enters S0, we cannot see THERMTRIP# de-assert then quickly assert.
		   Even if system enters S0 and force shutdown which also needs 4s. */
		 k_timer_start(&_s_thermtripTimer, K_MSEC(300), K_NO_WAIT);
	} else {
		/* THERMTRIP# assert: system exits S0 or after thermtrip event (after the low pulse)
		   If the filter timer is not timeout, it should be "after thermtrip event" */
		if (k_timer_remaining_ticks(&_s_thermtripTimer) > 0) {
			/* Thermtrip event */
			g_u32ThermalTripCnt ++;
			extern uint8_t g_u8EcStateFlags;
			g_u8EcStateFlags |= 0x01; /* save the event and report to BIOS (PLAT-49047) */

			k_timer_stop(&_s_thermtripTimer);

#if 1       /* disable this WR for RMB unless it is required. */
		    /* Workaround required by PLAT-49048 */
			extern bool g_thermtripForceG3;
			GET_NV_OPTIONS(thermtrip_force_g3, g_thermtripForceG3);
			if (g_thermtripForceG3) {
				LOG_ERR("APU_THERMTRIP assert in S0, trigger FORCE_G3");
				extern void app_pseq_nextStep(enum system_power_seq_stage next,
							      uint32_t delay);
				app_pseq_nextStep(FORCE_G3, 1);
			}
#endif
		}
	}
}

/**
 * @brief Callback to handle APU_ALERT change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_apu_alert_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	uint8_t level = gpio_read_pin(APU_EC_ALERT_N);
	LOG_PWRSEQ_PIN_STATUS(level, info->name);
}

/**
 * @brief Callback to handle ALW_PG change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_alw_pg_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	uint8_t level = gpio_read_pin(SYSTEM_S5_PG);

	dbgLogFifo_printf("SYSTEM_S5_PG: %d\n", level);
	LOG_PWRSEQ_PIN_STATUS(level, info->name);
}

/**
 * @brief Callback to handle SLP_S3 change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_slp_s3_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	uint8_t level = SLP_S3_ASSERT;
	if ((!SLP_S5_ASSERT) && gpio_read_pin(SYSTEM_S5_PG) && level) {
		g_apuRstS3Flag = 1; /* S3/S0i3 case */
	}
	dbgLogFifo_printf("slp_s3: %d\n", level);
	LOG_PWRSEQ_PIN_STATUS(level, info->name);
}

/**
 * @brief Callback to handle SLP_S5 change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_slp_s5_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	uint8_t level = SLP_S5_ASSERT;
	dbgLogFifo_printf("SLP_S5: %d\n", level);
	LOG_PWRSEQ_PIN_STATUS(level, info->name);
}

/**
 * @brief Callback to handle power button change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_pwr_button_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = BTN_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle usbc1 change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_usbc1_int_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
#if (CONFIG_USBC_RTK)
	if (app_usbc_readDevID() == PD_DEVICE_ID_RTKx) {
		app_rtk_IntTopHalf(gpio_read_pin(PD0_EC_USBC_INT_N), 0);  /* Select the interrupt for chip0 */
	}
#endif
#if CONFIG_USBC_POWER_DELIVERY
	/* Signal USBC task */
	app_usbc_giveSem();
#endif
}

/**
 * @brief Callback to handle usbc2 change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_usbc2_int_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
#if (CONFIG_USBC_RTK)
	if (app_usbc_readDevID() == PD_DEVICE_ID_RTKx) {
		app_rtk_IntTopHalf(gpio_read_pin(PD1_EC_USBC_INT_N), 1);  /* Select the interrupt for chip1 */
	}
#endif
#if CONFIG_USBC_POWER_DELIVERY
	/* Signal USBC task */
	app_usbc_giveSem();
#endif
}

/**
 * @brief Callback to handle lid close change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_lid_close_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	_s_InputBufferLidSt = gpio_read_pin(LID_CLOSE_N_3V3);
	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = LID_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle CHG_PROCHOT change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_chg_prochot_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	uint8_t level = gpio_read_pin(CHG_EC_PROCHOT_N);

	if (SYSTEM_S0_STATE != app_pseq_systemState()) {
		/* Release PROCHOT priot to exit */
		gpio_write_pin(EC_APU_PROCHOT_N, 1);
		return;
	}

	g_u32ChgProchotLCnt ++;
	LOG_DBG(" >>> CHG_PROCHOT <<<");
	dbgLogFifo_printf("HOT: %d\n", level);
}

/**
 * @brief Callback to handle bat in change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_bat_in_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = BAT_IN_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle chg ok change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_chg_ok_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = CHG_OK_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle smi int change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_smiInt_turner_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = 5;

	periph_setup_sem();
}

/**
 * @brief Callback to handle debug card present event.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_dbg_card_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = DBG_CARD_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle WLAN_IO33_N_IO18_SEL event.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_wlan_sel_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	/* change wlan select */
}

/**
 * @brief Callback to handle SFH_EC_INTn_R event.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_sfh_ec_int_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	/* SFH interrupt */
}

/**
 * @brief Callback to handle ALS_INTn event.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_als_int_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	/* ALS interrupt */
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	LOG_DBG("%s interrupt triggered", info->name);

#if CONFIG_ALS
	/* Call the OPT4048 interrupt handler */
	extern void app_opt4048_interrupt_callback(void);
	app_opt4048_interrupt_callback();
#endif
}

/**
 * @brief Callback to handle EC_WLAN_I2C_ALERT event.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_ec_wlan_i2c_int_callaback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	/* WLAN I2C alert interrupt */
}

/* order needs to align with "inputBuffer_lst" */
periph_gpio_callback gpio_cb_list[] = {
	board_periph_pwr_button_callback,
	board_periph_lid_close_callback,
	board_periph_bat_in_callback,
	board_periph_chg_ok_callback,
	board_periph_chg_prochot_callback,
	board_periph_apu_rst_callback,
	board_periph_apu_pwrok_callback,
	board_periph_apu_thermal_trip_callback,
	board_periph_apu_alert_callback,
	board_periph_slp_s3_callback,
	board_periph_slp_s5_callback,
	board_periph_usbc1_int_callback,
	board_periph_usbc2_int_callback,
	board_periph_alw_pg_callback,
	board_periph_dbg_card_callback,
	board_periph_wlan_sel_callback,
	board_periph_sfh_ec_int_callback,
	board_periph_als_int_callback,
	board_periph_ec_wlan_i2c_int_callaback
};

/**
 * @brief On board ioexp interrupt handler to process the pin status dispatcher.
 */
void board_periph_IoExp_IntDispatcher(void)
{
	/* not support */
}

/***********************************************************************/
/****** process Native GPIO pins which need to debounce ******/
/***********************************************************************/
/**
 * @brief CHG_ACOK debouncer 5ms.
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_ChgAcOk(uint8_t pin_evt)
{
#if CONFIG_POWER_SOURCE_MANAGEMENT
	/*
	* Notify the host (via ACPI/SCI interrupt) that the AC Power
	* has changed status.
	*/
	ACPI_NotifyHost(ACPI_SCI_AC);

	g_ChgAcOK = pin_evt;

	board_pwrSrcEvent();
	dbgLogFifo_printf("ChgAcOK: %d\n", g_ChgAcOK);
	LOG_DBG("Update g_ChgAcOK to %d", g_ChgAcOK);
#endif
}

/**
 * @brief BAT_IN debouncer 5ms.
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_Bty(uint8_t pin_evt)
{
#if CONFIG_BATTERY_MANAGEMENT
	/* postpone the plug-in case "g_batPresentFlag = 1" to the battery polling method */
	if (pin_evt)
		g_batPresentFlag = 0;
	board_pwrSrcEvent();
	LOG_DBG("Update g_batPresentFlag to %d", g_batPresentFlag);
#endif
}

/**
 * @brief LID_CLOSE debouncer 100ms.
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_LidClose(uint8_t pin_evt)
{
	uint32_t pinVal;

	/* LID_CLOSE# */
	pinVal = pin_evt;
	if ( pinVal && _s_InputBufferLidSt && !_s_InputBufferLidLastSend ) {
		ACPI_NotifyHost(ACPI_SCI_LID_OPEN);
		_s_InputBufferLidLastSend = 1;
#if CONFIG_HOT_BAG_EN
		g_lidCloseEnabled = false; /* LID status as open */
#endif
	}
	else if ( !pinVal && !_s_InputBufferLidSt && _s_InputBufferLidLastSend ) {
		ACPI_NotifyHost(ACPI_SCI_LID_CLOSE);
		_s_InputBufferLidLastSend = 0;
#if CONFIG_HOT_BAG_EN
		g_lidCloseEnabled = true; /* LID status as close */
#endif
	}

	LOG_DBG("Update LID_CLOSE:%d" ,pinVal);
}

/**
 * @brief UDC_PRESENT debouncer 100ms.
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_UdcPresent(uint8_t pin_evt)
{
#if CONFIG_UDC_MANAGEMENT
	g_isUdcPresent = pin_evt ? 0 : 1;
	LOG_DBG("Update g_isUdcPresent:%d" ,g_isUdcPresent);
#endif
}

/**
 * @brief Board peripheral initialization.
 *
 * @return 0 on success, negative error code on failure
 */
int board_periph_init(void)
{
	int ret = 0;
	gpio_cb_list_len = (sizeof(gpio_cb_list) / sizeof(gpio_cb_list[0]));
	inputBuffer_lst_len = (sizeof(inputBuffer_lst) / sizeof(inputBuffer_lst[0]));

	/* Enable the thermtrip detect timer */
	k_timer_init(&_s_thermtripTimer, _thermtripTimerCallback, NULL); /* 300ms */

	/* register all gpio interrupt vector handlers */
	ret = periph_register_gpio_cb_vector();
	if (ret) {
		LOG_ERR("register gpio interrupt vector failed");
	}

	_s_InputBufferLidLastSend  = !!(gpio_read_pin(LID_CLOSE_N_3V3));

	/* register the debouncer handlers */
	periph_register_debounce_handler(LID_CLOSE_N_3V3, periph_debouncer_LidClose);
	periph_register_debounce_handler(BAT_PRSNT_N, periph_debouncer_Bty);
	periph_register_debounce_handler(CHG_ACOK, periph_debouncer_ChgAcOk);
	periph_register_debounce_handler(DBG_CARD_PRSNT_N, periph_debouncer_UdcPresent);

	return ret;
}