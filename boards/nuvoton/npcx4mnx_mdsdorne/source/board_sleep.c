/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <soc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "u_misc.h"
#include "scicodes.h"
#include "board_id.h"
#include "board_sleep.h"
//#include "mchp_rtc.h"
#include "app_acpi.h"
#include "f_nv_options.h"
#include "board_config.h"
#include <zephyr/arch/arm/aarch32/cortex_m/cmsis.h>
#include "soc_gpio.h"
#include "soc_host.h"
#include <zephyr/drivers/timer/system_timer.h>
LOG_MODULE_DECLARE(sleep, CONFIG_SLEEP_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#define __KSI_NUM__       8
#define __KSI_DEBOUNCE__  10        /* 10ms */

/*
 * Bit0 - UART0
 * Bit1 - UART1
 */
#define _BLK_ACT_FLAG_UART0        0x000000001
#define _BLK_ACT_FLAG_UART1        0x000000002
#define _BLK_ACT_FLAG_TIMER16_0    0x000000004
#define _BLK_ACT_FLAG_TIMER16_1    0x000000008
#define _BLK_ACT_FLAG_TIMER16_2    0x000000010
#define _BLK_ACT_FLAG_TIMER16_3    0x000000020
#define _BLK_ACT_FLAG_TIMER32_0    0x000000040
#define _BLK_ACT_FLAG_TIMER32_1    0x000000080
#define _BLK_ACT_GIRQ23_TIMER16_0  0x000000100
#define _BLK_ACT_GIRQ23_TIMER16_1  0x000000200
#define _BLK_ACT_GIRQ23_TIMER16_2  0x000000400
#define _BLK_ACT_GIRQ23_TIMER16_3  0x000000800
#define _BLK_ACT_GIRQ23_TIMER32_0  0x000001000
#define _BLK_ACT_GIRQ23_TIMER32_1  0x000002000
#define _BLK_ACT_SYSTICK           0x000004000
#define _BLK_ACT_GIRQ15_UART0      0x000008000
#define _BLK_ACT_GIRQ15_UART1      0x000010000
#define _BLK_ACT_KSCAN             0x000020000

/* ************************** *
 *     static valuable        *
 * ************************** */
// static uintptr_t uart0_regbase = DT_REG_ADDR(DT_NODELABEL(uart0));
// static uintptr_t espi_iom_regsbase = DT_REG_ADDR(DT_NODELABEL(espi0));
// static bool _s_sleepFlag = false; 
static uint64_t _s_gpioOffFlags[4] = {0ull, 0ull, 0ull, 0ull};
// static uint8_t _s_rtosTimer_prld;
/* KBC GPIO for KSI and KSO */
static uint32_t KSI_list[] = {
	KEYBOARD_KSI_LIST
};
static uint32_t KSO_list[] = {
	KEYBOARD_KSO_LIST
};
static uint8_t _s_ksiMonitor = 0;
// static uint32_t _s_blkActFlag = 0;

/** ************************* *
 *     global valuable        *
 * ************************** */
uint8_t g_ui8isKbcWakeEvent = 0;

/**
 * @brief power off GPIO when enter sleep mode. Except GPIO in pinsActiveInSleepMode table
 */
void slp_gpioPwrGate(void)
{
	uint32_t pinsActiveInSleepMode[] = {
		APU_EC_RESET_N,
		EC_PWR_BTN_N,
		HW_PWR_BTN_N,
		PD0_EC_USBC_INT_N,

		SLP_S5_N,
		SLP_S3_S0A3_N,
		EC_SLP_S5_N,
		EC_SLP_S3_S0A3_N,
		EC_S0_LED,

		EC_APU_SCI_N,
		RSMRST_N,

		EC_1V8_S5_EN,
		EC_S5_PWREN,
		EC_USB32_RD_EN,
		EC_USB32_RD_RST_N,
		EC_FPR_OP_BOOT_LOGIN_N,
		EC_FPR_RST_N,
		EC_CODEC_AUX_MODE_EN_N,
		EC_CODEC_MIC_HW_MUTE_N,
		EC_TPAD_I3C_I2C_N_SEL,
		EC_TPNL_I3C_I2C_N_SEL,
		EC_IOT_3V3_SENSE_EN,
		EC_RTCRST_ON,
		TV_EN,
		EC_PWM0,
		EC_PWM1,
		
		CHG_ACOK,

		EC_CAP_LED,

		/* POR_INIT_GPIO */
		BAT_PRSNT_N /*Need for BAT detect*/,
		MPM_EVENT_N,

		POR_INIT_PWRSEQ,

		KEYBOARD_KSI_LIST,
		KEYBOARD_KSO_LIST,
#ifdef EC_BLINK_N
		EC_BLINK_N,
#endif
#ifdef TNR_EC_INT_N
		TNR_EC_INT_N,
#endif
		APU_EC_ALERT_N,
		APU_THERMTRIP_N,
		PD_PRSNT_N,
		LID_CLOSE_N_3V3,
	};
  _s_gpioOffFlags[0] = 0ull;
  _s_gpioOffFlags[1] = 0ull;
  _s_gpioOffFlags[2] = 0ull;
  _s_gpioOffFlags[3] = 0ull;

  for (uint8_t i = 0; !GPIO_isNullPin( _s_autogen_PorInitTable[i] ); i++) {
    uint32_t pin = _s_autogen_PorInitTable[i];
      if (GPIO_CTRL_PWRG_VTR_IO != gpio_get_power(pin))
        continue;

      bool skip = false;
      for(uint8_t j = 0; j < sizeof(pinsActiveInSleepMode) / sizeof(uint32_t); j++) {
        if (pin == pinsActiveInSleepMode[j] ) {
          skip = true;
          break;
        }
      }

      if (!skip) {
        _s_gpioOffFlags[i / 64] |= (1ull << (i % 64));
	gpio_set_interrupt(pin, GPIO_CTRL_IRQ_OFF);
	gpio_set_power(pin, GPIO_CTRL_PWRG_OFF);
      }
  }
}

/**
 * @brief restore the GPIO from power off to on when system resume
 */
void slp_gpioPwrRestore(void)
{
  uint32_t ops;
  for (uint8_t i = 0; i < 4; i++) {
    ops = 0;
    while (ops < 64) {
      ops = u_misc_u64lowestOneBit(_s_gpioOffFlags[i], ops);
      if (ops >= 64)
        break;

      gpio_set_power(_s_autogen_PorInitTable[i * 64 + ops], GPIO_CTRL_PWRG_VTR_IO);
      gpio_set_interrupt(_s_autogen_PorInitTable[i * 64 + ops], GPIO_CTRL_IRQ_ON);
      ops += 1;
    }
  }
}

/**
 * @brief KSI scan monitor with 10ms interval
 */
void board_slp_wake_up_from_kb(void)
{
  uint8_t m_espi_ResetInS0i3;
  GET_NV_OPTIONS(espi_ResetInS0i3 , m_espi_ResetInS0i3);
	if (_s_ksiMonitor) {
					_s_ksiMonitor = 0;

					/**
					 * set g_ui8isKbcWakeEvent so KBC will be re-init as long as VW is re-enabled.
					 * there will be IRQ1 sync up hence SO will treat the wake source as KBC.
					 */
	 if (m_espi_ResetInS0i3) {
					g_ui8isKbcWakeEvent = 1;
					LOG_DBG("set KbcWakeEvt = %d", g_ui8isKbcWakeEvent);
	 }

					/**
					 * Issue QEvt 0 is just trigger the system up
					 */
	LOG_DBG("Wake up from keyboard!");
					ACPI_NotifyHost(ACPI_SCI_KB_WAKEUP);

		}
}

/**
 * @brief enable the KSI and KSO wakeup
 */
void board_slp_enableKSxWakeup(void)
{

	_s_ksiMonitor = 1;
}

/**
 * @brief disable the KSI and KSO wakeup
 */
void board_slp_disableKSxWakeup(void)
{
	_s_ksiMonitor = 0;
}

/**
 * @brief enable the KSI and KSO power
 */
void board_slp_enableKSxPwr(void)
{
	/* 01. KSO PWR_ON+O1 */
	for (uint8_t i = 0; i < sizeof(KSO_list) / sizeof(uint32_t); i++) {
		uint32_t pin = KSO_list[i];
		gpio_set_power(pin, GPIO_CTRL_PWRG_VTR_IO);
		gpio_write_pin(pin, 0);
	}

	/* 02. KSI PWR_ON+LEVEL */
	for (uint8_t i = 0; i < sizeof(KSI_list) / sizeof(uint32_t); i++) {
		uint32_t pin = KSI_list[i];
		gpio_set_power(pin, GPIO_CTRL_PWRG_VTR_IO);
		/* INT_FALLING */
	}
}

/**
 * @brief disable the KSI and KSO power
 */
void board_slp_disableKSxPwr(void)
{
	/* 01. KSO PWR_OFF */
	for (uint8_t i = 0; i < sizeof(KSO_list) / sizeof(uint32_t); i++) {
		uint32_t pin = KSO_list[i];
		gpio_set_power(pin, GPIO_CTRL_PWRG_OFF);
	}

	/* 02. KSI PWR_OFF */
	for (uint8_t i = 0; i < sizeof(KSI_list) / sizeof(uint32_t); i++) {
		uint32_t pin = KSI_list[i];
		gpio_set_power(pin, GPIO_CTRL_PWRG_OFF);
	}
}

/**
 * @brief disable the KSI and KSO power
 *
 * @param         mode:     sleep mode SLP_SYSTEM_LIGHT_SLEEP or SLP_SYSTEM_HEAVY_SLEEP
 * @param     duration:     auto wake up duration when enter sleep mode
 */
void board_slp_Sleep(uint8_t mode, uint16_t duration)
{
	LOG_ERR("Enter system sleep");
	/*
	 * Disable priority mask temporarily to make sure that wake-up events
	 * are visible to the WFI instruction.
	 */
	__set_BASEPRI(0);
#ifdef EC_DEBUG_LED
		EC_DEBUG_LED(0);       /* Make sure DBG LED is turned off */
#endif
	sys_clock_set_timeout(duration*1000,false);
	/* Configure sleep/deep sleep settings in clock control module. */
	npcx_clock_control_turn_on_system_sleep(mode,
					1);

	/*
	 * Disable the connection between io pads that have leakage current and
	 * input buffer to save power consumption.
	 */
	// npcx_power_suspend_leak_io_pads();
	slp_gpioPwrGate();
	/* Turn on eSPI/LPC host access wake-up interrupt. */
	if (IS_ENABLED(CONFIG_ESPI_NPCX)) {
		npcx_host_enable_access_interrupt();
	}

	/* Turn on UART RX wake-up interrupt. */
	if (IS_ENABLED(CONFIG_UART_NPCX)) {
		npcx_uart_enable_access_interrupt();
	}

	/*
	 * Capture the reading of low-freq timer for compensation before ec
	 * enters system sleep mode.
	 */
	npcx_clock_capture_low_freq_timer();
}

/**
 * @brief Wake up from sleep mode.
 *
 * @retval True if successful.
 */
bool board_slp_Wakeup()
{
	bool ret = false;
	/*
	 * Compensate system timer by the elapsed time of low-freq timer during
	 * system sleep mode.
	 */
	sys_clock_set_timeout(1,false);
	npcx_clock_compensate_system_timer();
	// npcx_itim_evt_enable();
	/* Turn off eSPI/LPC host access wake-up interrupt. */
	if (IS_ENABLED(CONFIG_ESPI_NPCX)) {
		npcx_host_disable_access_interrupt();
	}

	/*
	 * Restore the connection between io pads that have leakage current and
	 * input buffer.
	 */
	// npcx_power_restore_leak_io_pads();
	slp_gpioPwrRestore();
	/* Turn off system sleep mode. */
	npcx_clock_control_turn_off_system_sleep();
#ifdef EC_DEBUG_LED
		EC_DEBUG_LED(1);  /* Turn on EC_DBG indicate exiting sleep mode */
#endif
	LOG_ERR("Exit system sleep");
	ret = true;

	return ret;
}