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
#include "board_id.h"
#include "f_nv_options.h"
LOG_MODULE_REGISTER(brd_periph, CONFIG_BOARD_PERIPH_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
/* Debouncing is performed in 1 ms intervals.
 * Maximum debounce count is the debounce time specified in milliseconds.
 */
#define BTN_DEBOUNCE_CNT	       CONFIG_PERIPHERAL_BTN_DEBOUNCE_TIME
#define AC_PRSENT_DEBOUNCE_CNT	   CONFIG_PERIPHERAL_AC_PRENT_DEBOUNCE_TIME
#define UART_PRSENT_DEBOUNCE_CNT   CONFIG_PERIPHERAL_UART_DONGLE_DEBOUNCE_TIME
#define BAT_IN_DEBOUNCE_CNT        CONFIG_PERIPHERAL_BAT_INT_DEBOUNCE_TIME
#define CHG_OK_DEBOUNCE_CNT        CONFIG_PERIPHERAL_CHG_OK_DEBOUNCE_TIME
#define DOCK_IN_DEBOUNCE_CNT       CONFIG_PERIPHERAL_DOCK_IN_DEBOUNCE_TIME
#define LID_DEBOUNCE_CNT           CONFIG_PERIPHERAL_LID_DEBOUNCE_TIME
#define DBG_CARD_DEBOUNCE_CNT      CONFIG_PERIPHERAL_DBG_CARD_DEBOUNCE_TIME
#define PD_SRC_ON_DEBOUNCE_CNT     CONFIG_PERIPHERAL_PD_SRC_ON_DEBOUNCE_TIME
#define UART0_INTn_DEBOUNCE_CNT    CONFIG_PERIPHERAL_UART0_INT_DEBOUNCE_TIME
#define SMIn_INTn_DEBOUNCE_CNT     CONFIG_PERIPHERAL_SMI_INT_DEBOUNCE_TIME
#define DISP_SW_DEBOUNCE_CNT       CONFIG_PERIPHERAL_DISP_SW_DEBOUNCE_TIME
#define AC_330W_PRSNT_DEBOUNCE_CNT CONFIG_PERIPHERAL_AC_330W_PRSNT_DEBOUNCE_TIME
#define CHG_PROCHOTn1_DEBOUNCE_CNT CONFIG_PERIPHERAL_CHG_PROCHOT1_DEBOUNCE_TIME
#define CHG_PROCHOTn2_DEBOUNCE_CNT CONFIG_PERIPHERAL_CHG_PROCHOT2_DEBOUNCE_TIME
#define MEM_PMIC_PG_DEBOUNCE_CNT   CONFIG_PERIPHERAL_MEM_PMIC_PG_DEBOUNCE_TIME

/* ************************** *
 *     extern valuable        *
 * ************************** */
#if CONFIG_APP_THERMAL
extern uint32_t g_u32ApmlErrCnt;
extern uint8_t g_u8apmlDelayCnt;
#endif
extern uint8_t g_uPepHook;
extern uint8_t g_aslGpFlags;
extern uint8_t g_apuRstS3Flag;
extern uint32_t g_u32ApuRstCnt;
extern uint32_t g_u32ThermalTripCnt;
extern uint32_t g_u32ChgProchotLCnt;
extern uint8_t g_u8EcStateFlags;
extern uint8_t g_ac330Prest;
/* task slp ready */
extern uint8_t *task_periphSlpReady;

/* ************************** *
 *     static valuable        *
 * ************************** */
static uint16_t _s_InputBufferIoExp0_lastVal = 0;
static uint8_t _s_InputBufferDockSt = 0;
static uint8_t _s_InputBufferDockLastSend = 0;
static uint8_t _s_InputBufferLidSt = 0;
static uint8_t _s_InputBufferLidLastSend = 0;

static struct k_timer _s_pcBtnPressTimer;
static void _pcBtnPressTimerCallback(struct k_timer *timer);

static struct k_timer _s_pcBtnClickTimer;
static void _pcBtnClickTimerCallback(struct k_timer *timer);
static uint8_t _pcBtnCounter;

static struct k_timer _s_prochotTimer;
static void _prochotTimerCallback(struct k_timer *timer);

static struct k_timer _s_thermtripTimer;
static void _thermtripTimerCallback(struct k_timer *timer);

/** ************************* *
 *     global valuable        *
 * ************************** */
/* All the native interrupt gpio list and debounce xx * 1ms*/
struct inputBuffer_info inputBuffer_lst[] = {
	{AC_PRSNT_N,   		false, AC_PRSENT_DEBOUNCE_CNT,   NULL, AC_PRSENT_INIT_POS,
			{{0}, NULL, 0}, "AcPrsent"},
	{HW_PWR_BTN_N,   		false, BTN_DEBOUNCE_CNT,         NULL, PWR_BTN_INIT_POS,
			{{0}, NULL, 0}, "PwrBtn"},
	{HDR_DOCK_IN_N,     	false, DOCK_IN_DEBOUNCE_CNT,     NULL, DOCK_IN_INIT_POS,
			{{0}, NULL, 0}, "Dockin"},
	{HW_LID_CLOSE_N,  	    false, LID_DEBOUNCE_CNT,     	 NULL, LID_INIT_POS,
			{{0}, NULL, 0}, "LidClose"},
	{EC_UDC_UART_INT_N, 	false, UART0_INTn_DEBOUNCE_CNT , NULL, UART0_INTn_INIT_POS,
			{{0}, NULL, 0}, "Uart0Int"},
#ifdef TNR_EC_INT_N
	{TNR_EC_INT_N, 	false, SMIn_INTn_DEBOUNCE_CNT,   NULL, SMIn_INTn_INIT_POS,
			{{0}, NULL, 0}, "SmiInt"},
#endif
	{BAT_PRSNT_N,         		false, BAT_IN_DEBOUNCE_CNT,      NULL, BAT_IN_INIT_POS,
			{{0}, NULL, 0}, "BatIn"},
	{CHG_ACOK,         		false, CHG_OK_DEBOUNCE_CNT,      NULL, CHG_OK_INIT_POS,
			{{0}, NULL, 0}, "ChgOk"},
	{PD_PRSNT_N,        false, PD_SRC_ON_DEBOUNCE_CNT,   NULL, PD_SRC_ON_INIT_POS,
			{{0}, NULL, 0}, "PdSrcOn"},
	{CHG_EC_PROCHOT_N,         false, 0,                        NULL, CHG_PROCHOT_INIT_POS,
			{{0}, NULL, 0}, "ChgProchot"},
	{APU_EC_RESET_N,             false, 0,                        NULL, APU_RST_INIT_POS,
			{{0}, NULL, 0}, "ApuRst"},
	{APU_PWROK,             false, 0,                        NULL, APU_PWROK_INIT_POS,
			{{0}, NULL, 0}, "ApuPwrOk"},
	{APU_THERMTRIP_N,        false, 0,                        NULL, APU_THERMALTRIP_INIT_POS,
			{{0}, NULL, 0}, "ApuThermalTrip"},
	{APU_EC_ALERT_N,           false, 0,                        NULL, APU_ALERT_INIT_POS,
			{{0}, NULL, 0}, "ApuAlert"},
	{SLP_S3_S0A3_N,           		false, 0,                        NULL, SLP_S3_INIT_POS,
			{{0}, NULL, 0}, "SlpS3"},
	{SLP_S5_N,           		false, 0,                        NULL, SLP_S5_INIT_POS,
			{{0}, NULL, 0}, "SlpS5"},
	{PD0_EC_USBC_INT_N,       false, 0,                        NULL, USB1_INT_INIT_POS,
			{{0}, NULL, 0}, "Usb1Int"},
	{PD1_EC_USBC_INT_N,      	false, 0,                        NULL, USB2_INT_INIT_POS,
			{{0}, NULL, 0}, "Usb2Int"},
	{EP_EC_INT_N,      	    false, 0,                        NULL, IOEXP0_INIT_POS,
			{{0}, NULL, 0}, "IoExp0"},
	{SYSTEM_S5_PG,      	false, 0,                        NULL, ALW_GD_INIT_POS,
			{{0}, NULL, 0}, "AlwPg"},
	/* U5105, +3.3_EC */
	{ex_EC_UART_DOGL_PRSNTn,false, UART_PRSENT_DEBOUNCE_CNT, NULL, UART_DONGLE_PRSENT_INIT_POS,
			{{0}, NULL, 0}, "UartDongle"},
	/* U5105, +3.3_EC */
	{ex_PD_FORCE_FW_UPn,  	false, DISP_SW_DEBOUNCE_CNT,     NULL, DISP_SW_INIT_POS,
			{{0}, NULL, 0}, "DispSW"},
	/* U5105, +3.3_EC */
	{ex_UDC_EP_DBG_CARD_PRSNTn, false, DBG_CARD_DEBOUNCE_CNT,    NULL, DBG_CARD_INIT_POS,
			{{0}, NULL, 0}, "DbgCard"},
	/* U5105, +3.3_EC */
	{ex_AC_330W_PRSNTn,      false, AC_330W_PRSNT_DEBOUNCE_CNT,NULL, AC_330W_PRSNT_INIT_POS,
			{{0}, NULL, 0}, "AC330"},
	{ex_MEM_PMIC_PWR_GOOD,   false, MEM_PMIC_PG_DEBOUNCE_CNT ,NULL, MEM_PMIC_PWR_GOOD_INIT_POS ,
			{{0}, NULL, 0}, "MEM_PG"},
};
uint16_t gpio_cb_list_len = 0, inputBuffer_lst_len = 0;
extern uint16_t g_InputBufferIoExp0_flag;
bool g_lidCloseEnabled = false;  /* false: LID_OPEN and true: LID_CLOSE */

/* Functions for inputbuffer handles */
#define LOG_PWRSEQ_PIN_STATUS(s, x)       _dumpPinSt(s, x);

/**
 * @brief Dump pin status for power sequence debugging
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


/**
 * @brief return the gpio info for sepcific pin
 *
 * @param pin: GPIO to return
 * @retval gpio info
 */
static struct inputBuffer_info* periph_get_gpio_info(uint32_t pin)
{
	uint8_t index = 0;
	struct inputBuffer_info* info;

	info = NULL;

	for (index = 0; index < (sizeof (inputBuffer_lst) / sizeof (inputBuffer_lst[0])); index++) {
		if (inputBuffer_lst[index].port_pin == pin)
			info = &inputBuffer_lst[index];
	}

	return info;
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
#if CONFIG_APP_THERMAL
		g_u32ApmlErrCnt = 0;
		g_u8apmlDelayCnt = 0;
#endif
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

#if CONFIG_UDC_MANAGEMENT
	app_postcode_OnReset();
#endif
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
 * @brief Callback for thermtrip timer expiration
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
			g_u8EcStateFlags |= 0x01;
			
			k_timer_stop(&_s_thermtripTimer);

			/* Workaround required by PLAT-49048 */
			extern bool g_thermtripForceG3;
			GET_NV_OPTIONS(thermtrip_force_g3, g_thermtripForceG3); 
			if (g_thermtripForceG3) {
				LOG_ERR("APU_THERMTRIP assert in S0, trigger FORCE_G3");
				extern void app_pseq_nextStep(enum system_power_seq_stage next,
							      uint32_t delay);
				app_pseq_nextStep(FORCE_G3, 1);
			}
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
#if CONFIG_UDC_MANAGEMENT
	app_udc_displayWrapper(0x88888888, 8, 0xFF);
#endif
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
 * @brief Callback to handle AC present change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_ac_psrent_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = AC_PRSENT_DEBOUNCE_CNT;

	periph_setup_sem();
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
#if (CONFIG_USBC_4CC)
	if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
		app_4cc_intTopHalf(gpio_read_pin(PD0_EC_USBC_INT_N));  /* Select the interrupt for chip0 */
	}
#endif
#if (CONFIG_USBC_CCGX)
	if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
		amd_crb_drivers_hpi_intTopHalf(gpio_read_pin(PD0_EC_USBC_INT_N), 0);  /* Select the interrupt for chip0 */
	}
#endif
#if (CONFIG_USBC_ITE)
	if (app_usbc_readDevID() == PD_DEVICE_ID_ITEx) {
		app_ite_IntTopHalf(gpio_read_pin(PD0_EC_USBC_INT_N), 0);  /* Select the interrupt for chip0 */
	}
#endif
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
#if (CONFIG_USBC_4CC)
	if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
		app_4cc_intTopHalf(gpio_read_pin(PD1_EC_USBC_INT_N));  /* Select the interrupt for chip1 */
	}
#endif
#if (CONFIG_USBC_CCGX)
	if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
		amd_crb_drivers_hpi_intTopHalf(gpio_read_pin(PD1_EC_USBC_INT_N), 1);  /* Select the interrupt for chip1 */
	}
#endif
#if (CONFIG_USBC_ITE)
	if (app_usbc_readDevID() == PD_DEVICE_ID_ITEx) {
		app_ite_IntTopHalf(gpio_read_pin(PD1_EC_USBC_INT_N), 1);  /* Select the interrupt for chip1 */
	}
#endif
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
 * @brief Callback to handle dock in change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_dock_in_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);
	_s_InputBufferDockSt = gpio_read_pin(HDR_DOCK_IN_N);
	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = DOCK_IN_DEBOUNCE_CNT;

	periph_setup_sem();
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
	_s_InputBufferLidSt = gpio_read_pin(HW_LID_CLOSE_N);
	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = LID_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle PD_SRC_ON change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_pd_src_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	LOG_ERR("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = PD_SRC_ON_DEBOUNCE_CNT;

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

	if (level == 0) {
#if 0
		/* Throttle APU if option is on (for non-KD case) */
		gpio_write_pin(EC_APU_PROCHOT_N, 0);
		k_timer_start(&_s_prochotTimer, K_MSEC(6), K_NO_WAIT);  /* 6ms */
#endif
	}

	/* KD has the PROCHOT# implemented by a different way */
#if CONFIG_UDC_MANAGEMENT
	app_postcode_setWarnMsg(1);
#endif

	g_u32ChgProchotLCnt ++;
	LOG_DBG(" >>> CHG_PROCHOT <<<");
	dbgLogFifo_printf("HOT: %d\n", level);
}

/**
 * @brief Prochot timer callback for non-KD board
 *
 * @param timer: timer structure pointer
 */
void _prochotTimerCallback(struct k_timer *timer)
{
	if (SYSTEM_S0_STATE != app_pseq_systemState()) {
		/* Release PROCHOT priot to exit */
		gpio_write_pin(EC_APU_PROCHOT_N, 1);
		return;
	}

	if (gpio_read_pin(CHG_EC_PROCHOT_N)) {
		/* de-assert APU_PROCHOT if source is de-asserted */
		gpio_write_pin(EC_APU_PROCHOT_N, 1);
	} else {
		/* if APU_PROCHOT had asserted */
		if (!gpio_read_pin(EC_APU_PROCHOT_N)) {
			/* Refresh warning message and keep APU_PROCHOT asserted */
#if CONFIG_UDC_MANAGEMENT
			app_postcode_setWarnMsg(1);
#endif
		} else {
#if CONFIG_UDC_MANAGEMENT
			app_postcode_setWarnMsg(0);
#endif
		}

		/* extend the timer if CHG_PROCHOTn is asserted */
		k_timer_start(&_s_prochotTimer, K_MSEC(6), K_NO_WAIT);  /* 6ms */
		LOG_DBG(" >>> CHG_PROCHOT 2");
		g_u32ChgProchotLCnt ++;
	}
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
 * @brief Callback to handle ioexp0 change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_ioexp0_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	task_status_set(task_periphSlpReady, TASK_STASTUS_BUSY_SHORT, 1);
	task_status_set(task_periphSlpReady, TASK_STASTUS_SLEEP_READY, 0);

	g_InputBufferIoExp0_flag = 1;
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
 * @brief Callback to handle uart0 int change events.
 *
 * @param     dev: device tree
 * @param gpio_cb: callback
 * @param    pins: pin status
 */
void board_periph_uart0_int_callback(const struct device *dev,
		     struct gpio_callback *gpio_cb, uint32_t pins)
{
	struct inputBuffer_info *info = CONTAINER_OF(gpio_cb, struct inputBuffer_info, gpio_cb);

	LOG_ERR("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = 3;

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

/* order needs to align with "inputBuffer_lst" */
periph_gpio_callback gpio_cb_list[] = {
	board_periph_ac_psrent_callback,
	board_periph_pwr_button_callback,
	board_periph_dock_in_callback,
	board_periph_lid_close_callback,
	board_periph_uart0_int_callback,
#ifdef TNR_EC_INT_N
	board_periph_smiInt_turner_callback,
#endif
	board_periph_bat_in_callback,
	board_periph_chg_ok_callback,
	board_periph_pd_src_callback,
	board_periph_chg_prochot_callback,
	board_periph_apu_rst_callback,
	board_periph_apu_pwrok_callback,
	board_periph_apu_thermal_trip_callback,
	board_periph_apu_alert_callback,
	board_periph_slp_s3_callback,
	board_periph_slp_s5_callback,
	board_periph_usbc1_int_callback,
	board_periph_usbc2_int_callback,
	board_periph_ioexp0_callback,
	board_periph_alw_pg_callback
};


/***********************************************************************/
/****** IOEXP gpio hardware interrupt handler callback functions  ******/
/***********************************************************************/
/*
 * U5105, Slv_0x27, I2C_3
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  HW_WWAN_RADIO#             I        x        Notify EC to toggle enable/disable WWAN radio, falling edge active
 * IO_01  HW_WIFI_RADIO#             I        x        Notify EC to toggle enable/disable WIFI radio, falling edge active
 * IO_02  DC_S5_PWRCTL               I        x        High: S5 POWER ON IN DC MODELow: S5 POWER OFF IN DC MODE(default)
 * IO_03  PD_FORCE_FW_UP#            I        x        Notify EC to force update PD FW when triggered in G3', falling edge activeNotify EC to toggle debug post code or Board basic info, falling edge active
 * IO_04  AC_330W_PRSNT#             I        x        Low when using 330W adaptor.
 * IO_05  FCH_DBG_BUS_EN             I        x        Notify EC that FCH debug mode is enabled, high active
 * IO_06  APU_EC_VDDCORE_LEDON       I        x        Reserved for Vcore LED indicator
 * IO_07  APU_EC_VDDCORE_HIGH        I        x        Reserved for Vcore LED indicator
 * IO_08  PSL_MODE_EN                I        x        High: EC PSL (VCI) mode Enable (default);Low: EC PSL (VCI) mode Disable; 
 * IO_09  EXT_TALERT#                I        x        To nofity EC, an overheating event is occurred. low active
 * IO_10  WLAN_IO33#_IO18_SEL        I        x        Notify EC the M.2 WLAN card I/O level. Low --3.3V level, High -- 1.8V level
 * IO_11  UDC_EP_DBG_CARD_PRSNT#     I        x        To notify EC, the UDC debug card is present. low active
 * IO_12  EC_UART_DOGL_PRSNT#        I        x        To notify EC, the UART dongle is present, low active
 * IO_13  WAIIE_EP_SENSOR_EN_STA#    I        x        Reserved to notify EC, the WALLE sensor power enable from system power. 
 * IO_14  EVAL_NEW#_LEGACY           I        x        Low: Plugged EVAL card is a new card ; High: Plugged EVAL card is a legacy card;
 */

/**
 * @brief Callback to handle P0 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P0_HW_WWAN_RADIOn(uint8_t pinSt)
{
	if (pinSt) {
		ACPI_NotifyHost(ACPI_SCI_WWAN_RADIO_SW_PRESSED);
#if CONFIG_UDC_MANAGEMENT
		app_udc_onWwanRadioClick();
#endif
	}
}

/**
 * @brief Callback to handle P1 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P1_HW_WIFI_RADIOn(uint8_t pinSt)
{
	if (pinSt) {
		ACPI_NotifyHost(ACPI_SCI_WIFI_RADIO_SW_PRESSED);
#if CONFIG_UDC_MANAGEMENT
		app_udc_onWlanRadioClick();
#endif
	}
}

/**
 * @brief Callback to handle P2 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P2_DC_S5_PWRCTL(uint8_t pinSt)
{

}

/**
 * @brief Callback to handle P3 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P3_PD_FORCE_FW_UPn(uint8_t pinSt)
{
	struct inputBuffer_info *info;
	info = periph_get_gpio_info(ex_PD_FORCE_FW_UPn);
	if (info == NULL) {
		LOG_ERR("Cannot find pin info");
		return;
	}

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = DISP_SW_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle P4 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P4_AC_330W_PRSNTn(uint8_t pinSt)
{

}


/**
 * @brief Callback to handle P5 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P5_FCHDEBUGBUS_EN(uint8_t pinSt)
{

}

/**
 * @brief Callback to handle P6 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P6_MEM_PMIC_PWR_GOOD(uint8_t pinSt)
{
	struct inputBuffer_info *info;
	info = periph_get_gpio_info(ex_MEM_PMIC_PWR_GOOD);
	if (info == NULL) {
		LOG_ERR("Cannot find pin info");
		return;
	}

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = MEM_PMIC_PG_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle P7 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P7_LPCAMM_EP_GSI_N_3V3(uint8_t pinSt)
{

}

/**
 * @brief Callback to handle P8 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P8_PSL_MODE_EN(uint8_t pinSt)
{

}

/**
 * @brief Callback to handle P9 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P9_EXT_TALERTn(uint8_t pinSt)
{

}

/**
 * @brief Callback to handle P10 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P10_WLAN_IO33n_IO18_SEL(uint8_t pinSt)
{

}

/**
 * @brief Callback to handle P11 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P11_UDC_EP_DBG_CARD_PRSNTn(uint8_t pinSt)
{
	struct inputBuffer_info *info;
	info = periph_get_gpio_info(ex_UDC_EP_DBG_CARD_PRSNTn);
	if (info == NULL) {
		LOG_ERR("Cannot find pin info");
		return;
	}

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = DBG_CARD_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle P12 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P12_EC_UART_DOGL_PRSNTn(uint8_t pinSt)
{
	struct inputBuffer_info *info;
	info = periph_get_gpio_info(ex_EC_UART_DOGL_PRSNTn);
	if (info == NULL) {
		LOG_ERR("Cannot find pin info");
		return;
	}

	LOG_DBG("%s level changed, starting debounce", info->name);
	info->debouncing = true;
	info->deb_cnt = UART_PRSENT_DEBOUNCE_CNT;

	periph_setup_sem();
}

/**
 * @brief Callback to handle P13 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P13_WAIIE_EP_SENSOR_EN_STAn(uint8_t pinSt)
{

}

/**
 * @brief Callback to handle P14 change events.
 *
 * @param pinSt: pin status
 */
void IOExp_P14_EVAL_NEWn_LEGACY(uint8_t pinSt)
{

}

/**
 * @brief Callback to handle btn press callback.
 *
 * @param timer: timer structure pointer
 */
static void _pcBtnPressTimerCallback(struct k_timer *timer)
{
	/* Oneshot timer. It will be disabled automatically on expiration */
	LOG_DBG("-- cb BTN 2s  press");
#if CONFIG_UDC_MANAGEMENT
	app_udc_longPress();
#endif
}

/**
 * @brief Callback to handle btn click callback
 *
 * @param timer: timer structure pointer
 */
static void _pcBtnClickTimerCallback(struct k_timer *timer)
{
	if (1 == _pcBtnCounter) {
		/* single click */
#if CONFIG_UDC_MANAGEMENT
		app_udc_shortClick();
#endif
	}

	if (_pcBtnCounter) {
		LOG_DBG("-- cb BTN click %d", _pcBtnCounter);
		_pcBtnCounter = 0;
	}
}

ioexp_handler_t _s_IoExp0_ISR[] = {
	IOExp_P0_HW_WWAN_RADIOn,
	IOExp_P1_HW_WIFI_RADIOn,
	IOExp_P2_DC_S5_PWRCTL,
	IOExp_P3_PD_FORCE_FW_UPn,
	IOExp_P4_AC_330W_PRSNTn,
	IOExp_P5_FCHDEBUGBUS_EN,
	IOExp_P6_MEM_PMIC_PWR_GOOD,
	IOExp_P7_LPCAMM_EP_GSI_N_3V3,
	IOExp_P8_PSL_MODE_EN,
	IOExp_P9_EXT_TALERTn,
	IOExp_P10_WLAN_IO33n_IO18_SEL,
	IOExp_P11_UDC_EP_DBG_CARD_PRSNTn,
	IOExp_P12_EC_UART_DOGL_PRSNTn,
	IOExp_P13_WAIIE_EP_SENSOR_EN_STAn,
	IOExp_P14_EVAL_NEWn_LEGACY,
	NULL
};

/**
 * @brief on board ioexp interrpt handler to process the pin status dispatcher
 */
void board_periph_IoExp_IntDispatcher(void)
{
	while (g_InputBufferIoExp0_flag)
	{
		uint16_t curVal = dev_pca9535_getInput(IOEXP_I2C_ADDR_U5105_I2C3_0x27, IOEXP_I2C_PORT_U5105_I2C3_0x27);
		uint16_t diff = curVal ^ _s_InputBufferIoExp0_lastVal;
		uint16_t m = 0x0001;

		for (uint8_t i = 0; i < sizeof (diff) * 8; i++, m <<= 1) {
			if ((m & diff) && NULL != _s_IoExp0_ISR[i]) {
				LOG_DBG("IoExp_ISR bit %d, level %d", i, (m & curVal) >> i);

				_s_IoExp0_ISR[i](!!(m & curVal));
			}
		}
		_s_InputBufferIoExp0_lastVal = curVal;
		/* Clear the flag */
		if (gpio_read_pin(EP_EC_INT_N))
			g_InputBufferIoExp0_flag = 0;
	}
}

/***********************************************************************/
/****** process IOEXP and Native GPIO pins which need to debounce ******/
/***********************************************************************/
/**
 * @brief CHG_ACOK debouncer 5ms
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
 * @brief AC_POWER_PRSENT debouncer 5ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_AcPowerPrsent(uint8_t pin_evt)
{
#if CONFIG_POWER_SOURCE_MANAGEMENT
	g_AcAdapterConnected = pin_evt ? 0 : 1;
	board_pwrSrcEvent();
	LOG_DBG("Update g_AcAdapterConnected to %d", g_AcAdapterConnected);
#endif
}

/**
 * @brief BAT_IN debouncer 5ms
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
 * @brief UART_DONGLE debouncer 100ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_UartDongle(uint8_t pin_evt)
{
#if CONFIG_UDC_MANAGEMENT
	g_isUartDonglePresent = pin_evt ? false : true;
	LOG_DBG("Update UARTDongle to %d", g_isUartDonglePresent);
#endif
}

/**
 * @brief LID_CLOSE debouncer 100ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_LidClose(uint8_t pin_evt)
{
	uint32_t pinVal;

	/* IOEXP LID_CLOSE# */
	pinVal = pin_evt;
	if ( pinVal && _s_InputBufferLidSt && !_s_InputBufferLidLastSend ) {
		ACPI_NotifyHost(ACPI_SCI_LID_OPEN);
		_s_InputBufferLidLastSend = 1;

		/* WHEN LID_CLOSED# IS DE-ASSERTED, EC SHOULD DRIVE TPAD_DISABLE# TO HIGH */
		if(app_pseq_getPwrStatus() > SYSTEM_G3_STATE) {
			/* Set TPAD_DISABLE# to HIGH */
			ioexp_set(ex_EP_TPAD_DISn, 1);
		}

	}
	else if ( !pinVal && !_s_InputBufferLidSt && _s_InputBufferLidLastSend ) {
		ACPI_NotifyHost(ACPI_SCI_LID_CLOSE);
		_s_InputBufferLidLastSend = 0;

		/* WHEN LID_CLOSED# IS ASSERTED, EC SHOULD DRIVE TPAD_DISABLE# TO LOW */
		if(app_pseq_getPwrStatus() > SYSTEM_G3_STATE) {
			ioexp_set(ex_EP_TPAD_DISn, 0); /* Set TPAD_DISABLE# to LOW */
		}
	}

	LOG_DBG("Update LID_CLOSE:%d" ,pinVal);
}

/**
 * @brief DOCK_IN debouncer 100ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_DockIn(uint8_t pin_evt)
{
	uint32_t pinVal;

	/* IOEXP DOCK_IN# */
	pinVal = pin_evt;
	if ( pinVal && _s_InputBufferDockSt && !_s_InputBufferDockLastSend ) {
		ACPI_NotifyHost(ACPI_SCI_LBDOCK_OFF);
		_s_InputBufferDockLastSend = 1;
	}
	else if ( !pinVal && !_s_InputBufferDockSt && _s_InputBufferDockLastSend ) {
		ACPI_NotifyHost(ACPI_SCI_LBDOCK_ON);
		_s_InputBufferDockLastSend = 0;
	}
	LOG_DBG("Update DOCK_IN:%d" ,pinVal);
}

/**
 * @brief UDC_PRESENT debouncer 100ms
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
 * PD_SOURCE_ONn works for TI case only
 * Reason is: not like CY PD, the EC has to get involved to turn on MOSEFET of PD Ports
 * before the sinked power reaching to sample point of PD_SOURCE_ONn signal.
 * In TI case, the PD will turn one of the three MOSEFETs on once PD connection is established.
 * as thus PD_SOURCE_ONn can reflac the PD powering status.
 */
/**
 * @brief PD_SOURCE_ON debouncer 100ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_PdSrcOn(uint8_t pin_evt)
{
#if CONFIG_POWER_SOURCE_MANAGEMENT
	board_pwrSrcEvent();
	LOG_DBG("PdSrcOn is %d", (gpio_read_pin(PD_PRSNT_N) ? 0 : 1));
#endif
}

/**
 * @brief Disp SW debouncer 3ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_DispSw(uint8_t pin_evt)
{
	if (SYSTEM_S0_STATE != app_pseq_systemState())
		return;

	if (!pin_evt) {
		k_timer_start(&_s_pcBtnPressTimer, K_MSEC(2000), K_NO_WAIT);  /* 2000ms */

		/* _s_pcBtnClickTimer is not enabled and _s_pcBtnClickTimer is expired */
		if (k_timer_remaining_ticks(&_s_pcBtnClickTimer) == 0) {
			_pcBtnCounter = 0;
			k_timer_start(&_s_pcBtnClickTimer, K_MSEC(800), K_NO_WAIT);  /* 800ms */
		}
	} else {
		/* _s_pcBtnClickTimer is not expired */
		if (k_timer_remaining_ticks(&_s_pcBtnClickTimer) > 0) {
			_pcBtnCounter ++;
		}

		/* _s_pcBtnPressTimer is not expired and _s_pcBtnClickTimer is expired */
		if (k_timer_status_get(&_s_pcBtnPressTimer) == 0 && k_timer_remaining_ticks(&_s_pcBtnClickTimer) == 0) {
			if (0 == _pcBtnCounter) {
				LOG_DBG("-- BTN click (>800ms <2000ms)");
			}
		}

		k_timer_stop(&_s_pcBtnPressTimer);
	}
}

/**
 * @brief SmiInt debouncer 3ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_SmiInt_Turner(uint8_t pin_evt)
{

}

/**
 * @brief UART0 Int debouncer 3ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_UART0IntUdc(uint8_t pin_evt)
{
	
}

/**
 * @brief AC 330W Int debouncer 5ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_AC_330W_PRSNT(uint8_t pin_evt)
{
#if CONFIG_POWER_SOURCE_MANAGEMENT
	g_ac330Prest = pin_evt ? 1 : 0;
	LOG_DBG("Update g_ac330Prest:%d" ,g_ac330Prest);
#endif
}

/**
 * @brief MEM PMIC power good debouncer
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_MEM_PMIC_PWR_GOOD(uint8_t pin_evt)
{
	extern uint8_t g_LPCAMM_PWR_EN;
#ifdef ex_MEM_PMIC_PWR_GOOD
	if (brdId_supportMemPMIC()) {
		if (g_LPCAMM_PWR_EN) {
			if (pin_evt) {
				ioexp_set(ex_MEM_PMIC_VR_EN_3V3, 1);
				g_LPCAMM_PWR_EN = 0;
			}
		}
	}
#endif
}

/**
 * @brief Charger prochot1 Int debouncer 5ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_CHG_PROCHOTn1(uint8_t pin_evt)
{
	LOG_DBG("Update chg prochot 1:%d" ,pin_evt);
}

/**
 * @brief Charger prochot2 Int debouncer 5ms
 *
 * @param pin_evt: pin status
 */
void periph_debouncer_CHG_PROCHOTn2(uint8_t pin_evt)
{
	LOG_DBG("Update chg prochot 2:%d" ,pin_evt);
}

/**
 * @brief baord peripheral init
 *
 * @retval 0 on success, negative error code on failure
 */
int board_periph_init(void)
{
	int ret = 0;

	gpio_cb_list_len = (sizeof (gpio_cb_list) / sizeof (gpio_cb_list[0]));
	inputBuffer_lst_len = (sizeof(inputBuffer_lst) / sizeof(inputBuffer_lst[0]));

	/**
	 * Timer for prochotL handling
	 *   APU_PROCHOT will assert for at least 6ms on each CHG_PROCHOTn assertion
	 */
	k_timer_init(&_s_prochotTimer, _prochotTimerCallback, NULL);  /* 6ms */

	/**
	 * Timer for PC button long press, single/double click
	 *   Long press: >2000ms
	 *   Multi-click threshold: 400ms
	 */
	k_timer_init(&_s_pcBtnPressTimer, _pcBtnPressTimerCallback, NULL);  /* 2000ms */
	k_timer_init(&_s_pcBtnClickTimer, _pcBtnClickTimerCallback, NULL);  /* 800ms */

	/* Enable the thermtrip detect timer */
	k_timer_init(&_s_thermtripTimer, _thermtripTimerCallback, NULL); /* 300ms */

	/* register all gpio interrupt vector handlers */
	ret = periph_register_gpio_cb_vector();
	if (ret) {
		LOG_ERR("register gpio interrupt vector failed");
	}

	/* Read initial status of LID and DOCK */
	_s_InputBufferDockLastSend = !!(gpio_read_pin( HDR_DOCK_IN_N ));
	_s_InputBufferLidLastSend  = !!(gpio_read_pin( HW_LID_CLOSE_N ));

	/* Enable interrupt of I2C_EXP_INT0n */
	g_InputBufferIoExp0_flag = 0;
	_s_InputBufferIoExp0_lastVal = dev_pca9535_getInput(IOEXP_I2C_ADDR_U5105_I2C3_0x27, IOEXP_I2C_PORT_U5105_I2C3_0x27);

	/* Enable the IOEXP0 GIRQ */
	gpio_interrupt_configure_pin(EP_EC_INT_N, GPIO_INT_EDGE_BOTH);

	/* register the debouncer handlers */
	periph_register_debounce_handler(AC_PRSNT_N, periph_debouncer_AcPowerPrsent);
	periph_register_debounce_handler(HDR_DOCK_IN_N, periph_debouncer_DockIn);
	periph_register_debounce_handler(HW_LID_CLOSE_N, periph_debouncer_LidClose);
	periph_register_debounce_handler(BAT_PRSNT_N, periph_debouncer_Bty);
	periph_register_debounce_handler(CHG_ACOK, periph_debouncer_ChgAcOk);
	periph_register_debounce_handler(PD_PRSNT_N, periph_debouncer_PdSrcOn);
#ifdef TNR_EC_INT_N
	periph_register_debounce_handler(TNR_EC_INT_N, periph_debouncer_SmiInt_Turner);
#endif
	periph_register_debounce_handler(EC_UDC_UART_INT_N, periph_debouncer_UART0IntUdc);

	periph_register_debounce_handler(ex_EC_UART_DOGL_PRSNTn, periph_debouncer_UartDongle);
	periph_register_debounce_handler(ex_UDC_EP_DBG_CARD_PRSNTn, periph_debouncer_UdcPresent);
	periph_register_debounce_handler(ex_PD_FORCE_FW_UPn, periph_debouncer_DispSw);
	periph_register_debounce_handler(ex_AC_330W_PRSNTn, periph_debouncer_AC_330W_PRSNT);
	periph_register_debounce_handler(ex_MEM_PMIC_PWR_GOOD, periph_debouncer_MEM_PMIC_PWR_GOOD);

	uint8_t ChgAcOKstatus = gpio_read_pin(CHG_ACOK);
	if (ChgAcOKstatus != g_ChgAcOK) {
		g_ChgAcOK = gpio_read_pin(CHG_ACOK);
		board_pwrSrcEvent();
	}

	return ret;
}