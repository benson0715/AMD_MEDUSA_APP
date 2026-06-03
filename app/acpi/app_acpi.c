/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "app_acpi.h"
#include "board_config.h"
#include "board_id.h"
#include "gpio_ec.h"
#include "f_nv_options.h"
#include "app_thermal.h"
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
#include "board_pwrSrc.h"
#endif
#if CONFIG_CHARGER_ISL9241
#include "dev_isl9241.h"
#elif CONFIG_CHARGER_MP2764
#include "amd_crb_drivers_mp2764.h"
#endif
#include "app_pseq.h"
#include "system.h"
#include "sci.h"
#include "scicodes.h"
#include "smc.h"
#include "board_id.h"
#include "app_AcDcSwitch.h"
#if (CONFIG_BATTERY_MANAGEMENT)
#include "board_smtbty.h"
#else
#include "app_smtbty.h"
#endif
#include "app_thermal.h"
#include "app_ucsi_tunnel.h"
#include "app_usbc_task.h"
#include "postcode_led_hub.h"
#include "amd_crb_drivers_hpi.h"
#include <zephyr/drivers/espi.h>
#include <zephyr/devicetree.h>
#if (CONFIG_USBC_4CC)
#include "app_4cc.h"
#endif
#if (CONFIG_USBC_CCGX)
#include "app_ccgx.h"
#endif
#if (CONFIG_USBC_RTK)
#include "app_realtek_pd.h"
#endif
#include "f_nv_options.h"
#include "i2c_hub.h"
#include <zephyr/drivers/pwm.h>
#include "ec_version.h"
#include "board_sleep.h"
#include "app_btn.h"
#include "periphmgmt.h"
#if CONFIG_ALS
#include "app_opt4048_thread.h"
#endif
#if CONFIG_ECDBGI_SUPPORT
#include "ec_debug.h"
#endif
#if CONFIG_SELECTABLE_DAC_VALUE
#include "board_dac.h"
#endif
#include "app_udc.h"
#include "dbgRw.h"

#if CONFIG_FAN_RPM_CONTROL
#include "app_fanRpmCtrl.h"
#endif

LOG_MODULE_REGISTER(acpi, CONFIG_ACPI_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#ifndef I2C_DAC_PWR_PORT
#define I2C_DAC_PWR_PORT            (I2C_0)
#endif

#ifndef BRDID_isDM
#define BRDID_isDM                  (1)
#endif

#define SMART_MUX_DUTY_CYCLE        (100u)
#define SMART_MUX_PWM_FREQ_MULT     (480u)

/* ************************** *
 *     extern valuable        *
 * ************************** */
extern uint8_t g_ui8AC_Time_cnt;     /* In second, 60 stands for 1 min. */
extern uint8_t g_ui8DC_Time_cnt;     /* In second */
extern uint8_t g_ui8Virtual_ACDC_STATUS;
#if CONFIG_SFH
extern uint8_t g_WAIE_RITE;
#endif
/* wwan and wlan D3 cold flag */
extern uint8_t g_auxD3Cold;
BID_MDS_CPU_TYPE g_ui8CpuType = BID_MDS_CPU_TYPE_NA;
#if CONFIG_USBC_UCSI_TUNNEL
extern bool g_isUcsiCycleFinished;
#endif
/* ************************** *
 *     global valuable        *
 * ************************** */
uint8_t g_slyType = 0;
/* allow Bios to write Bios info and Mac address to EC */
bool g_allow_BiosInfo_MacAddr_Report = true;

/* flag for S0i3 enter */
uint8_t g_ui8MondernStandbySupport = 0;
/* flag for slide shutdown enable/disable */
uint8_t g_slideToShutdownEnabled = 0;
/* flag to reset EC */
bool g_ecResetAfterSetOptions = false;

/* WRITE-1-to-CLEAR */
uint8_t g_u8EcStateFlags = 0;

/* holds the value of macros MS_uPEP_HOOK_XXXX */
uint8_t g_aslGpFlags = 0xFF;
uint8_t g_uPepHook = 0;

/* True: USB4 disable and False: USB4 enable. As USB4 enable by default, so default value is false */
bool g_usb4Disable = false;
bool g_tbt3Disable = false;
bool g_portDisable = false;

/* Force power on re-timer for FW update */
bool g_forceRtPower = false;

/* bios debug log enable/disable flag */
bool g_biosDebugLogEnable = true;

/* ************************** *
 *     static valuable        *
 * ************************** */
/* postcode align with screen flag */
static uint8_t _s_alignPostCodeWithScreenBacklight = 0;

/*
 * Smart Mux 1.0 (Mayan/Lilac PoC)
 *   Implement fixed PWM backlight and Mux setting for Case 1/2
 */
static uint8_t g_smartMuxSetting = 0;
/* smart mux related setting */
static struct k_work smartMux_work;

#if CONFIG_PWM
#ifdef PWM
static const struct device *devPWM;
#endif
#endif

/*
 * ACPI fake notify queue
 *
 *   for EVENT IDs being written to the queue, EC will
 *   notify to OS later as long as ACPI notify is enabled.
 *
 *   This is a simplified version than what in PHX.
 *   It works only for EVENT set before ACPI Notify is enabled.
 */
static uint8_t _s_fakeEventId = 0;

/*
 * VW MMIO tunnel variables
 */
static uint8_t _s_vw_mmio_val = 0;    
static uint8_t _s_vw_mmio_status = ACPI_VW_MMIO_SUCCESS;   
static struct k_work vwMmio_work;

#if CONFIG_HOT_BAG_EN
struct k_timer g_pmfHeartBeatTimer;
extern bool g_powerSeq_criticalZone;
extern uint16_t highestSkinTemp;
PMF_STATUS g_pmfStatus = PMF_NUKNOW;
extern bool g_lidCloseEnabled;
void ACPI_pmfHeartBeatRefresh(void);
bool g_hotBagEnabled = false;
#endif
bool g_thermtripForceG3 = true;
/**
 * @brief ACPI fake notify queue
 *
 * @param isRead Host read or write.
 * @param ui8Idx acpi offset
 * @param pui8Data pointer to buffer holding the data.
 */
static void AcpiFakeNotifyQueueHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (isRead) {
		*pui8Data = _s_fakeEventId;
	} else if (*pui8Data) {
		_s_fakeEventId = *pui8Data;
	}
}

/*
 * PWM channel uses main clock frequency i.e. 48 MHz.
 * For desired frequency of 1 kHz, division should be:
 *
 * division = main clock freq / desired freq.
 *          = 48 MHz / 1 kHz = 48000.
 *
 * PWM Duty cycle = pulse width / period.
 *
 * To calculate duty cycle in percentage, multiplier should be:
 * multiplier = division / 100
 *            = 48000 / 100 =480.
 */
static void smartMux_1P0_SetPwm(bool turnOn)
{
#if CONFIG_PWM
#ifdef PWM
	int ret = 0;

	if (turnOn) {
		ret = pwm_set_cycles(devPWM, 0,
				SMART_MUX_PWM_FREQ_MULT * SMART_MUX_DUTY_CYCLE,
				SMART_MUX_PWM_FREQ_MULT * (SMART_MUX_DUTY_CYCLE / 2), 0);

		if (ret) {
			LOG_WRN("pwm error: %d", ret);
		}
	} else {
		/* set period and dutycycle to 0 to disable PWM module */
		ret = pwm_set_cycles(devPWM, 0, 0, 0, 0);

		if (ret) {
			LOG_WRN("pwm error: %d", ret);
		}
	}
#endif
#endif
}

/**
 * @brief smart mux 1P0 timer callback
 */
static void smartMux_1P0_eventProc (struct k_work *w)
{
	if (0 == g_smartMuxSetting) {
		/*
		 * Restore SmartMux Ctrl PINs to default
		 */
		board_init_smartMux_V0();

		/* turn off PWM */
		smartMux_1P0_SetPwm(0);
	} else if (1 == g_smartMuxSetting) {
		/*
		 * All pins Pp output (SmartMux 1.0 case 1)
		 */
		board_init_smartMux_V1();

		/* turn on PWM */
		smartMux_1P0_SetPwm(1);
	} else if (2 == g_smartMuxSetting) {
		/*
		 * All pins Pp output (SmartMux 1.0 case 2)
		 */
		board_init_smartMux_V2();

		/* turn on PWM */
		smartMux_1P0_SetPwm(1);
	}  else if (3 == g_smartMuxSetting) {
		/* Mayan/Lilac have these pins reused with FPR. Change the drive type
		 * and enable PWM if SmartMux 1.5+ is enabled
		 *
		 * All pins Pp output LOW (for SmartMux 1.5+)
		 */
		board_init_smartMux_V3();

		/* turn on PWM */
		smartMux_1P0_SetPwm(1);
	}

	LOG_DBG("Apply SmartMux 1.0 setting Case %d", g_smartMuxSetting);
}

/**
 * @brief ACPI callback for ECRAM empty
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiGpioHandler_NULL (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint8_t cc = 1;
	if (isRead)
		*pui8Data = cc++;
}

#if CONFIG_APP_THERMAL
/**
 * @brief ACPI callback for prochotsetting00 ECRAM 0xB2
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_ProchotSetting00 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	extern uint8_t g_p3tConfigLimit;

	if (isRead) {
		*pui8Data = g_p3tConfigLimit;
	} else {
		if (*pui8Data != g_p3tConfigLimit) {
			g_p3tConfigLimit = *pui8Data;

			app_thermal_resetP3tSetting();

#if CONFIG_BATTERY_MANAGEMENT
#if CONFIG_CHARGER_ISL9241
			dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
			amd_crb_drivers_mp2764_setProchot();
#endif
#endif
		}
	}
}
#endif
/**
 * @brief ACPI callback for prochotsetting01 ECRAM 0xB3
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_ProchotSetting01 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t u8FakeBatteryLevel; /* [6:0] */
	uint8_t p3tLimitEn_AC;      /* [7] - Enable for AC case */
	GET_NV_OPTIONS(chg_FakeDcLevel,       u8FakeBatteryLevel);
	GET_NV_OPTIONS(thm_p3tLimitEn_AC,     p3tLimitEn_AC);

	if (isRead) {
		*pui8Data = (p3tLimitEn_AC ? 0x80 : 0x00) | (0x7F & u8FakeBatteryLevel);
	} else {
		uint8_t tmp;
		bool changedFlag = false;

		tmp = 0x7F & (*pui8Data);
		if (tmp <= 100 && tmp != u8FakeBatteryLevel) {
			changedFlag = true;
			u8FakeBatteryLevel = tmp;
		}

		tmp = (0x80 & *pui8Data) ? 1 : 0;
		if (tmp != p3tLimitEn_AC) {
			changedFlag = true;
			p3tLimitEn_AC = tmp;
		}

		if (changedFlag) {
			SET_NV_OPTIONS(chg_FakeDcLevel,   u8FakeBatteryLevel);
			SET_NV_OPTIONS(thm_p3tLimitEn_AC, p3tLimitEn_AC);
#if CONFIG_BATTERY_MANAGEMENT
#if CONFIG_CHARGER_ISL9241
			dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
			amd_crb_drivers_mp2764_setProchot();
#endif
#endif
		}
	}
}

/**
 * @brief ACPI callback for prochotsetting02 ECRAM 0xB4
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_ProchotSetting02 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Data = 0;

	uint8_t ProchotAcDebounce; /* [1:0] */
	uint8_t ProchotDuration;   /* [4:2] */
	uint8_t ProchotDcDebounce; /* [6:5] */
	uint8_t p3tLimitEn_DC;     /* [7] - Enable for DC-Only case */
	GET_NV_OPTIONS(chg_ProchotAcDebounce, ProchotAcDebounce);
	GET_NV_OPTIONS(chg_ProchotDuration,   ProchotDuration);
	GET_NV_OPTIONS(chg_ProchotDcDebounce, ProchotDcDebounce);
	GET_NV_OPTIONS(thm_p3tLimitEn_DC,     p3tLimitEn_DC);

	ui8Data = (ProchotAcDebounce & 0x03) | ((ProchotDuration & 0x07) << 2) | ((ProchotDcDebounce & 0x03) << 5) | ((p3tLimitEn_DC & 0x01) << 7);

	if (isRead) {
		*pui8Data = ui8Data;
	} else {
		if (ui8Data != *pui8Data) {
			ui8Data = *pui8Data;

			ProchotAcDebounce = ui8Data & 0x03;
			ProchotDuration = (ui8Data >> 2) & 0x07;
			ProchotDcDebounce = (ui8Data >> 5) & 0x03;
			p3tLimitEn_DC = (ui8Data >> 7) & 0x01;

			SET_NV_OPTIONS(chg_ProchotAcDebounce, ProchotAcDebounce);
			SET_NV_OPTIONS(chg_ProchotDuration,   ProchotDuration);
			SET_NV_OPTIONS(chg_ProchotDcDebounce, ProchotDcDebounce);
			SET_NV_OPTIONS(thm_p3tLimitEn_DC,     p3tLimitEn_DC);

#if CONFIG_BATTERY_MANAGEMENT
#if CONFIG_CHARGER_ISL9241
			dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
			amd_crb_drivers_mp2764_setProchot();
#endif
#endif
		}
	}
}

/**
 * @brief ACPI callback for prochotsetting03 ECRAM 0xB5
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_ProchotSetting03 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t optionVal; /* [5:0] */
	GET_NV_OPTIONS(chg_DcProchotLevel, optionVal);
	if (isRead) {
		*pui8Data = optionVal;
	} else {
		if (optionVal != (*pui8Data & 0x3F)) {
			optionVal = (*pui8Data & 0x3F);
			SET_NV_OPTIONS(chg_DcProchotLevel, optionVal);

#if CONFIG_BATTERY_MANAGEMENT
#if CONFIG_CHARGER_ISL9241
			dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
			amd_crb_drivers_mp2764_setProchot();
#endif
#endif
		}
	}
}

/**
 * @brief ACPI callback for switch01 ECRAM 0xB6
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_SW01 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Data;
	uint32_t optionVal;
#if CONFIG_USBC_UCSI_TUNNEL
	extern bool g_ucsiTunnelEnabled;
#endif
	if (isRead) {
		ui8Data = 0;

		GET_NV_OPTIONS(chg_UseNvdcMode, optionVal);
		if (!optionVal) {
			ui8Data |= 0x01;                        /* Bit[0] - !chg_UseNvdcMode */
		} else {
			ui8Data &= (~0x01);
		}
#if CONFIG_USBC_UCSI_TUNNEL
		if (g_ucsiTunnelEnabled) {
			ui8Data |= 0x02;                        /* Bit[1] - g_ucsiTunnelEnabled */
		} else {
			ui8Data &= (~0x02);
		}
#endif
		GET_NV_OPTIONS(thm_uploadOnboardTemp, optionVal);
		if (optionVal) {
			ui8Data |= 0x04;                        /* Bit[2] - thm_uploadOnboardTemp */
		} else {
			ui8Data &= (~0x04);
		}

		GET_NV_OPTIONS(thm_uploadEvalCardTemp, optionVal);
		if (optionVal) {
			ui8Data |= 0x08;                        /* Bit[3] - thm_uploadEvalCardTemp */
		} else {
			ui8Data &= (~0x08);
		}

		GET_NV_OPTIONS(f_wirelessManagability, optionVal);
		if (optionVal) {
			ui8Data |= 0x10;                        /* Bit[4] - f_wirelessManagability */
		} else {
			ui8Data &= (~0x10);
		}

		GET_NV_OPTIONS(f_s0i3KbcWake, optionVal);
		if (optionVal) {
			ui8Data |= 0x20;                        /* Bit[5] - f_s0i3KbcWake */
		} else {
			ui8Data &= (~0x20);
		}

		GET_NV_OPTIONS(pwr_keepWlanPwrInS3, optionVal);
		if (optionVal) {
			ui8Data |= 0x40;                        /* Bit[6] - pwr_keepWlanPwrInS3 */
		} else {
			ui8Data &= (~0x40);
		}

		GET_NV_OPTIONS(pwr_keepWlanPwrInS4, optionVal);
		if (optionVal) {
			ui8Data |= 0x80;                        /* Bit[7] - pwr_keepWlanPwrInS4 */
		} else {
			ui8Data &= (~0x80);
		}

		*pui8Data = ui8Data;
	} else {
		ui8Data = *pui8Data;

		if (ui8Data & (0x01)) {                     /* Bit[0] - !chg_UseNvdcMode */
			/*
			 * Ensure PCIe slot power are off in bypass mode (PLAT-76765)
			 */
#ifdef EC_EVAL_SLT_PWREN
			gpio_write_pin(EC_EVAL_SLT_PWREN, 0);
#endif
#ifdef X1_SLOT_PWREN
			gpio_write_pin(X1_SLOT_PWREN, 0);
#endif
#ifdef ex_DT_PWREN
			ioexp_set(ex_DT_PWREN, 0);
#endif
			SET_NV_OPTIONS(chg_UseNvdcMode, 0);
		} else {
			SET_NV_OPTIONS(chg_UseNvdcMode, 1);
		}
#if CONFIG_USBC_UCSI_TUNNEL
		if (ui8Data & (0x02)) {                     /* Bit[1] - g_ucsiTunnelEnabled */
			g_ucsiTunnelEnabled = 1;
		} else {
			g_ucsiTunnelEnabled = 0;
		}
#endif
		if (ui8Data & (0x04)) {                     /* Bit[2] - thm_uploadOnboardTemp */
			SET_NV_OPTIONS(thm_uploadOnboardTemp, 1);
		} else {
			SET_NV_OPTIONS(thm_uploadOnboardTemp, 0);
		}

		if (ui8Data & (0x08)) {                     /* Bit[3] - thm_uploadEvalCardTemp */
			SET_NV_OPTIONS(thm_uploadEvalCardTemp, 1);
		} else {
			SET_NV_OPTIONS(thm_uploadEvalCardTemp, 0);
		}

		if (ui8Data & (0x10)) {                     /* Bit[4] - f_wirelessManagability */
			SET_NV_OPTIONS(f_wirelessManagability, 1);
			// gpio_set_power(MPM_EVENT_N, GPIO_CTRL_PWRG_VTR_IO); // TODO_RTK
		} else {
			SET_NV_OPTIONS(f_wirelessManagability, 0);
			// gpio_set_power(MPM_EVENT_N, GPIO_CTRL_PWRG_OFF); // TODO_RTK
		}

		if (ui8Data & (0x20)) {                     /* Bit[5] - f_s0i3KbcWake */
			SET_NV_OPTIONS(f_s0i3KbcWake, 1);
		} else {
			SET_NV_OPTIONS(f_s0i3KbcWake, 0);
		}

		if (ui8Data & (0x40)) {                     /* Bit[6] - pwr_keepWlanPwrInS3 */
			SET_NV_OPTIONS(pwr_keepWlanPwrInS3, 1);
		} else {
			SET_NV_OPTIONS(pwr_keepWlanPwrInS3, 0);
		}

		if (ui8Data & (0x80)) {                     /* Bit[7] - pwr_keepWlanPwrInS4 */
			SET_NV_OPTIONS(pwr_keepWlanPwrInS4, 1);
		} else {
			SET_NV_OPTIONS(pwr_keepWlanPwrInS4, 0);
		}
	}
}

/**
 * @brief ACPI callback for switch02 ECRAM 0xB7
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_SW02 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Data;
	uint32_t optionVal;

	bool isProchotSettingChanged = false;

	if (isRead) {
		ui8Data = 0;

		if (g_slideToShutdownEnabled) {
			ui8Data |= 0x01;                        /* Bit[0] - g_slideToShutdownEnabled */
		} else {
			ui8Data &= (~0x01);
		}

		GET_NV_OPTIONS(espi_ResetInS0i3 , optionVal);
		if (optionVal) {
			ui8Data |= 0x02;                        /* Bit[1] - g_eSPIResetInS0i3 */
		} else {
			ui8Data &= (~0x02);
		}

		if (g_allow_BiosInfo_MacAddr_Report) {
			ui8Data |= 0x04;                        // Bit[2] - bios info mac address report enable/disable
		} else {
			ui8Data &= (~0x04);
		}

		GET_NV_OPTIONS(chg_AcOnlyProchot, g_isThrottleApuInAcOnly);
		if (g_isThrottleApuInAcOnly) {
			ui8Data |= 0x08;                        /* Bit[3] - Charger PROCHOT# to throttle APU in AC only case */
		} else {
			ui8Data &= (~0x08);
		}

		GET_NV_OPTIONS(pwr_SSD0D3Cold, optionVal);
		if (optionVal) {
			ui8Data |= 0x10;                        /* Bit[4] - SSD0 D3-Code enable */
		} else {
			ui8Data &= (~0x10);
		}

		GET_NV_OPTIONS(pwr_SSD1D3Cold, optionVal);
		if (optionVal) {
			ui8Data |= 0x20;                        /* Bit[5] - SSD1 D3-Code enable */
		} else {
			ui8Data &= (~0x20);
		}

		GET_NV_OPTIONS(chg_DcOnlyProchot, g_isThrottleApuInDcOnly);
		if (g_isThrottleApuInDcOnly) {
			ui8Data |= 0x40;                        /* Bit[6] - Charger PROCHOT# to throttle APU in DC only case */
		} else {
			ui8Data &= (~0x40);
		}

		if (g_ui8MondernStandbySupport) {
			ui8Data |= 0x80;                        /* Bit[7] - g_ui8MondernStandbySupport */
		} else {
			ui8Data &= (~0x80);
		}

		*pui8Data = ui8Data;
	} else {
		ui8Data = *pui8Data;

		if (ui8Data & (0x01)) {                     /* Bit[0] - g_slideToShutdownEnabled */
			g_slideToShutdownEnabled = 1;
		} else {
			g_slideToShutdownEnabled = 0;
		}

		if (ui8Data & (0x02)) {                     /* Bit[1] - g_eSPIResetInS0i3 */
			SET_NV_OPTIONS(espi_ResetInS0i3, 1);
		} else {
			SET_NV_OPTIONS(espi_ResetInS0i3, 0);
		}

													/* Bit[2] - bios info mac address report enable/disable */
												    /* read only */

		if (ui8Data & (0x08)) {
			if (!g_isThrottleApuInAcOnly)
				isProchotSettingChanged = true;

			SET_NV_OPTIONS(chg_AcOnlyProchot, 1);   /* Bit[3] - Charger PROCHOT# to throttle APU in AC only case */
			g_isThrottleApuInAcOnly = 1;
		} else {
			if (g_isThrottleApuInAcOnly)
				isProchotSettingChanged = true;

			SET_NV_OPTIONS(chg_AcOnlyProchot, 0);
			g_isThrottleApuInAcOnly = 0;
		}

		if (ui8Data & (0x10)) {
			SET_NV_OPTIONS(pwr_SSD0D3Cold, 1);       /* Bit[4] - SSD0 D3-Code enable */
		} else {
			SET_NV_OPTIONS(pwr_SSD0D3Cold, 0);
		}

		if (ui8Data & (0x20)) {
			SET_NV_OPTIONS(pwr_SSD1D3Cold, 1);       /* Bit[5] - SSD1 D3-Code enable */
		} else {
			SET_NV_OPTIONS(pwr_SSD1D3Cold, 0);
		}

		if (ui8Data & (0x40)) {
			if (!g_isThrottleApuInDcOnly)
				isProchotSettingChanged = true;

			SET_NV_OPTIONS(chg_DcOnlyProchot, 1);    /* Bit[6] - Charger PROCHOT# to throttle APU in DC only case */
			g_isThrottleApuInDcOnly = 1;
		} else {
			if (g_isThrottleApuInDcOnly)
				isProchotSettingChanged = true;

			SET_NV_OPTIONS(chg_DcOnlyProchot, 0);
			g_isThrottleApuInDcOnly = 0;
		}

		if (ui8Data & (0x80)) {                     /* Bit[7] - g_ui8MondernStandbySupport */
			g_ui8MondernStandbySupport = 1;
		} else {
			g_ui8MondernStandbySupport = 0;
		}

#if CONFIG_BATTERY_MANAGEMENT
		/* notify the PROCHOT reconfig event if setting changed. */
		if (isProchotSettingChanged) {
#if CONFIG_CHARGER_ISL9241
			dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
			amd_crb_drivers_mp2764_setProchot();
#endif
		}
#endif
	}
}

/**
 * @brief ACPI callback for switch03 ECRAM 0xCD
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_SW03 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Data;
	uint32_t optionVal;
	bool max695x_status;

	if (isRead) {
		ui8Data = 0;

		GET_NV_OPTIONS(f_TurnOnPostCode, optionVal);
		if (optionVal) {
			ui8Data |= 0x01;                          /* Bit[0] - f_TurnOnPostCode */
		} else {
			ui8Data &= (~0x01);
		}

		if (_s_alignPostCodeWithScreenBacklight) {
			ui8Data |= 0x02;                          /* Bit[1] - _s_alignPostCodeWithScreenBacklight */
		} else {
			ui8Data &= (~0x02);
		}

		ui8Data &= 0xF3;
		if (g_smartMuxSetting) {                     /* Bit[3:2] - g_smartMuxSetting */
			ui8Data |= ((g_smartMuxSetting & 0x03) << 2);
		}

		if (g_biosDebugLogEnable) {                  /* Bit[4] - g_biosDebugLogEnable */
			ui8Data |= 0x10;
		} else {
			ui8Data &= (~0x10);
		}

		if (g_auxD3Cold & 0x20) {
			ui8Data |= 0x20;                         /* Bit[5] - WWAN D3-Code enable */
		} else {
			ui8Data &= (~0x20);
		}

		if (g_auxD3Cold & 0x40) {
			ui8Data |= 0x40;                         /* Bit[6] - WLAN D3-Code enable */
		} else {
			ui8Data &= (~0x40);
		}

		if (g_auxD3Cold & 0x80) {
			ui8Data |= 0x80;                        /* Bit[7] - LOM D3-Code enable */
		} else {
			ui8Data &= (~0x80);
		}

		*pui8Data = ui8Data;
	} else {
		ui8Data = *pui8Data;

		/* BIOS would read the ECRAM to compare this value. If it is the same as BIOS setting, BIOS would not write value
		 * It will have problem. So, comment out write this setting to EC NV ram.
		 * Let BIOS modify this setting every time if it is disabled.
		 */
		GET_NV_OPTIONS(f_TurnOnPostCode, optionVal);
		if (ui8Data & 0x01) {
			SET_NV_OPTIONS(f_TurnOnPostCode, 1);      /* Bit[0] - f_TurnOnPostCode */
		} else {
			SET_NV_OPTIONS(f_TurnOnPostCode, 0);
		}
		max695x_status = postcode_led_hub_get_sts();       /* refresh MAX695x setting if needed. */
		if ((ui8Data & 0x01) && !max695x_status) {
			postcode_led_hub_set_sts(true);
			postcode_led_hub_32bit_turnOn(16);
		} else if (!(ui8Data & 0x01) && max695x_status) {
			postcode_led_hub_32bit_turnOff();
			postcode_led_hub_set_sts(false);
		}

		if (ui8Data & 0x02) {
			_s_alignPostCodeWithScreenBacklight = 1;  /* Bit[1] - _s_alignPostCodeWithScreenBacklight */
		} else {
			_s_alignPostCodeWithScreenBacklight = 0;
		}

		if (g_smartMuxSetting != ((ui8Data & 0x0C) >> 2)) { /* Bit[3:2] - g_smartMuxSetting */
			g_smartMuxSetting = ((ui8Data & 0x0C) >> 2);
			k_work_submit(&smartMux_work);
		}

		if (ui8Data & 0x10) {                         /* Bit[4] - g_biosDebugLogEnable */
			g_biosDebugLogEnable = true;
		} else {
			g_biosDebugLogEnable = false;
			app_udc_refreshUdcStatus();
		}

        if (ui8Data & (0x20)) {
            g_auxD3Cold |= 0x20;               // Bit[5] - WWAN D3-Code enable
        } else {
        	g_auxD3Cold &= (~0x20);
        }

        if (ui8Data & (0x40)) {
        	g_auxD3Cold |= 0x40;               // Bit[6] - WLAN D3-Code enable
        } else {
        	g_auxD3Cold &= (~0x40);
        }

		if (ui8Data & (0x80)) {
			g_auxD3Cold |= 0x80;               // Bit[7] - LOM D3-Code enable
		} else {
			g_auxD3Cold &= (~0x80);
		}
	}
}

/**
 * @brief ACPI callback for switch04 ECRAM 0xCE
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_SW04 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Data;

	uint8_t ui8CpuTypeTmp = 0;

	if (isRead) {
		ui8Data = 0;

		if (g_auxD3Cold & 0x01) {
			ui8Data |= 0x01;                        // Bit[0] - SD D3-Code enable
		} else {
			ui8Data &= (~0x01);
		}

		// Bit[3:1] - MDS CPU OPEN type
		ui8Data &= (~0x0E);
		ui8Data |= (uint8_t)((g_ui8CpuType & 0x07) << 1);

#if 0 // Use cpu_open_type instead of fan speed table select in MDS board (reserved for future use)
#if CONFIG_FAN_RPM_CONTROL
		uint8_t select = 0;
		app_fan_ctrl_sys_speed_table_select(&select, 0);
		ui8Data &= ~(0x07 << 4);                    // Bit[6:4] - Fan speed table select
		ui8Data |= ((select & 0x07) << 4);
#endif
#endif
		if (g_auxD3Cold & 0x02) {
			ui8Data |= 0x80;                         /* Bit[7] - dGPU D3-Code enable */
		} else {
			ui8Data &= (~0x80);
		}

		*pui8Data = ui8Data;
	} else {
		ui8Data = *pui8Data;

		if (ui8Data & (0x01)) {
			g_auxD3Cold |= 0x01;                    // Bit[0] - SD D3-Code enable
		} else {
			g_auxD3Cold &= (~0x01);
		}

		// Bit[3:1] - MDS CPU OPEN type
		if (!BRDID_isDM) {
			/* only not DM board can change the CPU OPN type */
			ui8CpuTypeTmp = (BID_MDS_CPU_TYPE)((ui8Data & 0x0E) >> 1);
			if (g_ui8CpuType != ui8CpuTypeTmp) {
				g_ui8CpuType = ui8CpuTypeTmp;
				SET_NV_OPTIONS(cpu_open_type, g_ui8CpuType);
				/* Need to reboot system after NV option saved */
				g_ecResetAfterSetOptions = true;
			}
		} else {
			/* do nothing for DM board */

		}

#if 0 // Use cpu_open_type instead of fan speed table select in MDS board (reserved for future use)
#if CONFIG_FAN_RPM_CONTROL
		uint8_t select = ((ui8Data >> 4) & 0x07);  // Bit[6:4] - Fan speed table select
		app_fan_ctrl_sys_speed_table_select(&select, 1);
#endif
#endif
		if (ui8Data & (0x80)) {
			g_auxD3Cold |= 0x02;                    // Bit[7] - dGPU D3-Code enable
		} else {
			g_auxD3Cold &= (~0x02);
		}
	}
}


/**
 * @brief ACPI callback for switch05 ECRAM 0xFF
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiSwitchHandler_SW05 (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Data;
	if (isRead) {
		ui8Data = 0;
#if CONFIG_SFH
		GET_NV_OPTIONS(waie_b_value, g_WAIE_RITE);
		if (g_WAIE_RITE) {
			ui8Data |= 0x80;                        // // Bit[7] - WAIIE RITE enable
		} else {
			ui8Data &= (~0x80);
		}
#endif
#if CONFIG_HOT_BAG_EN
		/* PMF heartbeat bit 0:
		 Heartbeat -> 1
		*/
		/* PMF status bit 1~3:
	     Boot -> 1
	     Unload -> 2
	     Suspend -> 3
	     Resume -> 4
		*/
		uint8_t temp = (uint8_t)g_pmfStatus;
		ui8Data |= (temp << 1) & 0x0E;

		/* PMF status bit 4:
		   Hot bag enable
		*/
		GET_NV_OPTIONS(hot_bag_enable, g_hotBagEnabled);
		if (g_hotBagEnabled) {
			ui8Data |= 0x10;
		} else {
			ui8Data &= (~0x10);
		}
#endif

#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
		extern uint8_t g_S5AlwEnFlag;

		GET_NV_OPTIONS(s5_alw_enable, g_S5AlwEnFlag);  // Bit[5] - Force S5 Always
		if (g_S5AlwEnFlag) {
			ui8Data |= 0x20;
		} else {
			ui8Data &= (~0x20);
		}
#endif

		GET_NV_OPTIONS(thermtrip_force_g3, g_thermtripForceG3);  // Bit[6] - Thermtrip Force G3
		if (g_thermtripForceG3) {
			ui8Data |= 0x40;
		} else {
			ui8Data &= (~0x40);
		}

		*pui8Data = ui8Data;
	} else {
		ui8Data = *pui8Data;
#if CONFIG_SFH
		if (ui8Data & (0x80)) {
			g_WAIE_RITE = 0x01;                    // Bit[7] - WAIIE RITE enable
			SET_NV_OPTIONS(waie_b_value, 1);
		} else {
			g_WAIE_RITE = 0;
			SET_NV_OPTIONS(waie_b_value, 0);
		}
#endif
#if CONFIG_HOT_BAG_EN
		/* bit-0 assert: heartbeat message per 1min */
		if ((ui8Data & 0x01) == 0x01)
		{
			/* refresh timer */
			ACPI_pmfHeartBeatRefresh();
		}
		/* bit-1~3: PMF status update */
		else if ((ui8Data & 0x0E) > 0)
		{
			g_pmfStatus = (PMF_STATUS)((ui8Data & 0x0E) >> 1);
			switch (g_pmfStatus) {
				case PMF_BOOT: 
					/* start the heartbeat monitor */
					ACPI_pmfHeartBeatRefresh();
					break;
				case PMF_UNLOAD:
					/* end the heartbeat monitor */
					k_timer_stop(&g_pmfHeartBeatTimer);
					break;
				case PMF_SUSPEND:
					/* enter critical zone */
					g_powerSeq_criticalZone = true;
					ACPI_pmfHeartBeatRefresh();
					break;
				case PMF_RESUME:
					/* exit the critical zone */
					g_powerSeq_criticalZone = false;
					ACPI_pmfHeartBeatRefresh();
					break;
				default:
					break;
			}
		}
		if ((ui8Data & 0x10) == 0x10) {
			/* PMF status bit 4:
		     Hot bag enable
			*/
			g_hotBagEnabled = true;
			SET_NV_OPTIONS(hot_bag_enable, 1);
		} else {
			g_hotBagEnabled = false;
			SET_NV_OPTIONS(hot_bag_enable, 0);
		}
#endif

#if defined(BRD_MDS_DORNE) || defined(BRD_MDS_AERIS)
		extern uint8_t g_S5AlwEnFlag;

		if ((ui8Data & 0x20) == 0x20) {      // Bit[5] - Force S5 Always
			g_S5AlwEnFlag = true;
			SET_NV_OPTIONS(s5_alw_enable, 1);
		} else {
			g_S5AlwEnFlag = false;
			SET_NV_OPTIONS(s5_alw_enable, 0);
		}
#endif

		if ((ui8Data & 0x40) == 0x40) {      // Bit[6] - Thermtrip Force G3
			g_thermtripForceG3 = true;
			SET_NV_OPTIONS(thermtrip_force_g3, 1);
		} else {
			g_thermtripForceG3 = false;
			SET_NV_OPTIONS(thermtrip_force_g3, 0);
		}
	}
}

/**
 * @brief DC switch handlers ECRAM 0xC7
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIDCTimeHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (isRead) {
		GET_NV_OPTIONS(chg_DcTime, *pui8Data);
	} else {
		SET_NV_OPTIONS(chg_DcTime, *pui8Data);
	}
}

/**
 * @brief AC switch handlers ECRAM 0xC8
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIACTimeHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (isRead) {
		GET_NV_OPTIONS(chg_AcTime, *pui8Data);
	} else {
		SET_NV_OPTIONS(chg_AcTime, *pui8Data);
	}
}

/**
 * @brief ACPI EC Event handler1 ECRAM 0xCB
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIEcEventFlagsHandler1(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	/* Write-1-to-clear */
	if (isRead) {
		*pui8Data = g_u8EcStateFlags;
	} else {
		g_u8EcStateFlags &= ~(*pui8Data);
	}
}

/**
 * @brief ACPI EC Event handler2 ECRAM 0xCC
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIEcEventFlagsHandler2(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t retVal = 0;
	/* Read-only */
	if (isRead) {
		retVal |= (app_btn_isFchPwrBtnNotifyEnabled() ? 0x01 : 0x00); /* BIT[0] -  1: Notify PwrBtn; 0: Don't notify PwrBtn */

		*pui8Data = retVal;
	}
}

/**
 * @brief ACPI EC version handler ECRAM 0xB8~BC
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiEcVersionHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Length, ui8Index;

	// if (isRead) {
	// 	uint8_t ui8Buff[10];
	// 	ui8Length = ec_version_stringGet(3, ui8Buff, sizeof(ui8Buff));
	// 	ui8Index = ui8Idx - ACPI_EC_FW_VERSION;

	// 	if (!ui8Length || ui8Index > ui8Length) {
	// 		return;
	// 	}
	// 	*pui8Data = ui8Buff[ui8Index];
	// }
	*pui8Data = 0x11; // TODO_RTK
	return;
}

/**
 * @brief ACPI EC build date handler ECRAM 0xBD~C0
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiEcBuildDateHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Length, ui8Index;
	static uint8_t strBuf[4] = {0, 0, 0, 0};

	// if (isRead) {
	// 	if (0 == strBuf[0]) {
	// 		uint8_t ui8Buff[11];
	// 		ui8Length = ec_version_stringGet(2, ui8Buff, sizeof(ui8Buff));
	// 		if (!ui8Length) return;
	// 		uint16_t yyyy = (ui8Buff[0] - '0') * 1000 +
	// 						(ui8Buff[1] - '0') * 100 +
	// 						(ui8Buff[2] - '0') * 10 +
	// 						(ui8Buff[3] - '0');
	// 		uint8_t mm =	(ui8Buff[5] - '0') * 10 +
	// 						(ui8Buff[6] - '0');

	// 		strBuf[0] = (uint8_t)((yyyy - 2010) >= 10 ? ('A' + (yyyy - 2010)) : ('0' + (yyyy - 2010)));
	// 		strBuf[1] = (uint8_t)(mm >= 10 ? ('A' + (mm - 10)) : ('0' + mm));
	// 		strBuf[2] = ui8Buff[8];
	// 		strBuf[3] = ui8Buff[9];
	// 	}

	// 	ui8Index = ui8Idx - ACPI_EC_FW_BUILD_DATE;
	// 	if (0 == strBuf[0] || ui8Index >= 4) {
	// 		return;
	// 	}
	// 	*pui8Data = strBuf[ui8Index];
	// }
	*pui8Data = 0x11; // TODO_RTK
	return;
}

/**
 * @brief ACPI EC build time handler ECRAM 0xC1~C6
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void AcpiEcBuildTimeHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Length, ui8Index;

	// if (isRead) {  // TODO_RTK
	// 	uint8_t ui8Buff[10];
	// 	ui8Length = ec_version_stringGet(6, ui8Buff, sizeof(ui8Buff));
	// 	ui8Index = ui8Idx - ACPI_EC_FW_BUILD_TIME;

	// 	if (!ui8Length || ui8Index > ui8Length) {
	// 		return;
	// 	}
	// 	*pui8Data = ui8Buff[ui8Index];
	// }
	*pui8Data = 0x11;
	return;
}


/**
 * @brief Miscellaneous status and control in acpi ec namespace
 *
 * @param isRead Host read or write.
 * @param ui8Idx acpi offset
 * @param pui8Data pointer to buffer holding the data.
 */
void ACPIMiscStatusControlHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
#if CONFIG_ACDCSWITCH
	uint8_t AcDcSwitchEnableFlag;
	GET_NV_OPTIONS(chg_AcDcSwitch, AcDcSwitchEnableFlag);
#endif
	/* Bit[1] - 1, BIOS enable AC/DC switch
	 *			0, BIOS disable AC/DC switch
	 * Bit[2] - 1, BIOS enable ACPI mode
	 *			0, BIOS disable ACPI mode
	 * Bit[7:6] - target sleep mode
	 *			0, N\A, reserved for S0i3
	 *			1, S3
	 *			2, S4
	 *			3, S5
	 */
	if (isRead) {
#if CONFIG_ACDCSWITCH
		if (AcDcSwitchEnableFlag) {
			*pui8Data |=  AMD_AC_DC_SWITCH_ENABLE;
		} else {
			*pui8Data &= ~AMD_AC_DC_SWITCH_ENABLE;
		}
#endif
		if (!ACPI_isHostNotifyDisable()) {
			*pui8Data |=  AMD_ACPI_MODE_ENABLE;
		} else {
			*pui8Data &= ~AMD_ACPI_MODE_ENABLE;
		}

		*pui8Data &= 0x3F; /* clear field of g_slyType. APU always read as 0. */
	} else {
#if CONFIG_ACDCSWITCH
		if (*pui8Data & AMD_AC_DC_SWITCH_ENABLE) {
			gpio_set_type(CHG_ACOK, GPIO_OPEN_DRAIN | GPIO_OUTPUT_HIGH);

			SET_NV_OPTIONS(chg_AcDcSwitch, 1);
#if CONFIG_BATTERY_MANAGEMENT
			if (!AcDcSwitchEnableFlag) {
#if CONFIG_CHARGER_ISL9241
				dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
				amd_crb_drivers_mp2764_setProchot();
#endif
			}
#endif
		} else {
			gpio_set_type(CHG_ACOK, GPIO_INPUT);

			g_LoadfakeBatteryData = 0;
			g_ui8AC_Time_cnt = 0;
			g_ui8DC_Time_cnt = 0;
			SET_NV_OPTIONS(chg_AcDcSwitch, 0);

#if CONFIG_BATTERY_MANAGEMENT
			if (AcDcSwitchEnableFlag) {
#if CONFIG_CHARGER_ISL9241
				dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
				amd_crb_drivers_mp2764_setProchot();
#endif
			}
#endif
		}
#endif
		g_slyType = (*pui8Data >> 6) & 0x03; /* set g_slyType */

		if (*pui8Data & AMD_ACPI_MODE_ENABLE) {
			ACPI_EnableHostNotify();
		} else {
			ACPI_DisableHostNotify();
		}
	}
}

#if CONFIG_BATTERY_MANAGEMENT
/**
 * @brief ACPI battery charge throttle ECRAM 0xEA
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryChargeThrottle (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	/* _DSM F1 charge throttle (0 to 100)
	 *
	 * A value of 40 percent indicates the battery should be
	 * charged at 40 percent of maximum rate.
	 * A value of 0 percent indicates battery charging should
	 * be stopped until this method is called again.
	 *
	 * Read charge throttle
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_CHARGE_THROTTLE)) {
		*pui8Data = app_battery_ChargeThrottleGet(app_battery_selectedGet());
	}

	/*
	 * Set charge throttle
	 */
	else if(!isRead && (ui8Idx == ACPI_BATTERY_CHARGE_THROTTLE)) {
		app_battery_ChargeThrottleSet(app_battery_selectedGet(), *pui8Data);
	}
}

/**
 * @brief ACPI battery low handler ECRAM 0xD2~D3
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryLowHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t u16Temp;
	static uint16_t _s_u16Value;

	/*
	 * Return LSB of previously saved battery low threshold.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_LOW)) {
		*pui8Data = (uint8_t)(_s_u16Value & 0xff);
	}

	/*
	 * Save current battery low threshold, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_LOW + 1)) {
		_s_u16Value = app_battery_lowThresholdGet(app_battery_selectedGet());
		*pui8Data = (uint8_t)((_s_u16Value >> 8) & 0xff);
	}

	/*
	 * Save LSB of battery low threshold
	 */
	else if(!isRead && (ui8Idx == ACPI_BATTERY_LOW)) {
		_s_u16Value = (uint16_t)(*pui8Data);
	}

	/*
	 * Save MSB of battery low threshold, then set the battery low threshold.
	 */
	else if(!isRead && (ui8Idx == ACPI_BATTERY_LOW + 1)) {
		u16Temp = (uint16_t)(*pui8Data);
		_s_u16Value &= 0xFF;
		_s_u16Value |= ((u16Temp << 8) & 0xFF00);
		app_battery_lowThresholdSet(app_battery_selectedGet(), _s_u16Value);
	}
}

/**
 * @brief ACPI battery critical handler ECRAM 0xD4~D5
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryCriticalHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t u16Temp;
	static uint16_t _s_u16Value;

	/*
	 * Return LSB of previously saved battery critical threshold.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_CRITICAL)) {
		*pui8Data = (uint8_t)(_s_u16Value & 0xff);
	}

	/*
	 * Save current battery critical threshold, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_CRITICAL + 1)) {
		_s_u16Value = app_battery_criticalThresholdGet(app_battery_selectedGet());
		*pui8Data = (uint8_t)((_s_u16Value >> 8) & 0xff);
	}

	/*
	 * Save LSB of battery critical threshold
	 */
	else if(!isRead && (ui8Idx == ACPI_BATTERY_CRITICAL)) {
		_s_u16Value = (uint16_t)(*pui8Data);
	}

	/*
	 * Save MSB of battery critical threshold, then set the of battery critical
	 * threshold.
	 */
	else if(!isRead && (ui8Idx == ACPI_BATTERY_CRITICAL + 1)) {
		u16Temp = (uint16_t)(*pui8Data);
		_s_u16Value &= 0xFF;
		_s_u16Value |= ((u16Temp << 8) & 0xFF00);
		app_battery_criticalThresholdSet(app_battery_selectedGet(), _s_u16Value);
	}
}

/**
 * @brief ACPI battery trip point handler ECRAM 0xD6~D7
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryTripPointHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint16_t u16Temp;
	static uint16_t _s_u16Value;

	/*
	 * Return LSB of previously saved battery Trip Point.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_TRIP_POINT)) {
		*pui8Data = (uint8_t)(_s_u16Value & 0xff);
	}

	/*
	 * Save current battery Trip Point, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_TRIP_POINT + 1)) {
		_s_u16Value = app_battery_tripPointGet(app_battery_selectedGet());
		*pui8Data = (uint8_t)((_s_u16Value >> 8) & 0xff);
	}

	/*
	 * Save LSB of battery Trip Point
	 */
	else if(!isRead && (ui8Idx == ACPI_BATTERY_TRIP_POINT)) {
		_s_u16Value = (uint16_t)(*pui8Data);
	}

	/*
	 * Save MSB of battery Trip Point, then set the battery Trip Point.
	 */
	else if(!isRead && (ui8Idx == ACPI_BATTERY_TRIP_POINT + 1)) {
		u16Temp = (uint16_t)(*pui8Data);
		_s_u16Value &= 0xFF;
		_s_u16Value |= ((u16Temp << 8) & 0xFF00);

		app_battery_clearTrippedFlag(app_battery_selectedGet());
		app_battery_tripPointSet(app_battery_selectedGet(), _s_u16Value);
	}
}

/**
 * @brief ACPI battery temperature handler ECRAM 0xD8~D9
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryTemperatureHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t ui16Data;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_TEMPERATURE)) {
		*pui8Data = (uint8_t)(ui16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_TEMPERATURE + 1)) {
		if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_Temperature, &ui16Data))
			ui16Data = 0xFFFF;
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
	}
}

/**
 * @brief ACPI battery max error handler ECRAM 0xDA~DB
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryMaxErrorHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t ui16Data;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_MAX_ERROR)) {
		*pui8Data = (uint8_t)(ui16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_MAX_ERROR + 1)) {
		if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_MaxError, &ui16Data))
			ui16Data = 0xFFFF;
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
	}
}

/**
 * @brief ACPI battery cycle count handler ECRAM 0xDC~DD
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryCycleCountHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t ui16Data;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_CYCLE_COUNT)) {
		*pui8Data = (uint8_t)(ui16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_CYCLE_COUNT + 1)) {
		if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_CycleCount, &ui16Data))
			ui16Data = 0xFFFF;
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
	}
}

/**
 * @brief ACPI battery current handler ECRAM 0xDE~DF
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryCurrentHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static int16_t i16Data;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_CURRENT)) {
		*pui8Data = (uint8_t)(i16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_CURRENT + 1)) {
#if CONFIG_ACDCSWITCH
		if (g_LoadfakeBatteryData) {
			if (g_ui8Virtual_ACDC_STATUS & ACPI_BATTERY_CONNECTED) {
				if (g_ui8Virtual_ACDC_STATUS & ACPI_AC_CONNECTED) {
					/* AC+DC so report as charging */
					i16Data = FAKE_BATTERY_CHARGE_CURRENT;
				} else {
					/* DC only so discharging */
					i16Data = FAKE_BATTERY_DISCHARGE_CURRENT;
				}
			} else {
				i16Data = 0;
			}
		}else 
#endif		
		{
			if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_Current, &i16Data))
				i16Data = 0x0;
		}
		*pui8Data = (uint8_t)((i16Data >> 8) & 0xff);
	}
}

/**
 * @brief ACPI battery voltage handler ECRAM 0xDA~DB
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryVoltageHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t ui16Data;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_VOLTAGE)) {
		*pui8Data = (uint8_t)(ui16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_VOLTAGE + 1)) {
#if CONFIG_ACDCSWITCH
		if (!g_LoadfakeBatteryData) {
			if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_Voltage, &ui16Data))
				ui16Data = 0xFFFF;
		} else {
			ui16Data = FAKE_BATTERY_VOLTAGE;
		}
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
#else
		if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_Voltage, &ui16Data))
			ui16Data = 0xFFFF;
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
#endif
	}
}

/**
 * @brief ACPI battery design voltage handler ECRAM 0xDC~DD
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryDesignVoltageHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t ui16Data;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_DESIGN_VOLTAGE)) {
		*pui8Data = (uint8_t)(ui16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_DESIGN_VOLTAGE + 1)) {
		if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_DesignVoltage, &ui16Data))
			ui16Data = 0xFFFF;
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
	}
}

/**
 * @brief ACPI battery remaining capacity handler ECRAM 0xDE~DF
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryRemainingCapacityHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t ui16Data;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if( isRead && (ui8Idx == ACPI_BATTERY_REMAINING_CAPACITY) ) {
		*pui8Data = (uint8_t)(ui16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if( isRead && (ui8Idx == ACPI_BATTERY_REMAINING_CAPACITY + 1) ) {
		ui16Data = board_battery_remainingCapacity();    /* lock the data on high byte read */
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
	}
}

/**
 * @brief ACPI battery full charge capacity handler ECRAM 0xE0~E1
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryFullChargeCapacityHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t ui16Data;
	static uint16_t ui16Rsvd;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_FULL_CHARGE_CAPACITY)) {
		*pui8Data = (uint8_t)(ui16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_FULL_CHARGE_CAPACITY + 1)) {
		ui16Data = board_battery_fullChargeCapacity();    /* lock the data on high byte read */
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
	}

	/*
	 * On write, to set the reserved battery capacity
	 */
	else if((!isRead) && (ui8Idx == ACPI_BATTERY_FULL_CHARGE_CAPACITY)) {
		ui16Rsvd = (0x00FF & (uint16_t)*pui8Data);
	}

	else if((!isRead) && (ui8Idx == ACPI_BATTERY_FULL_CHARGE_CAPACITY + 1)) {
		ui16Rsvd |= (((uint16_t)*pui8Data) << 8);
		dev_smtbty_setRsvdBtyCapacity(ui16Rsvd);
	}
}

/**
 * @brief ACPI battery design capacity handler ECRAM 0xE1~E2
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryDesignCapacityHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t ui16Data;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_DESIGN_CAPACITY)) {
		*pui8Data = (uint8_t)(ui16Data & 0xff);
	}

	/*
	 * Save current battery data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_DESIGN_CAPACITY + 1)) {
#if CONFIG_ACDCSWITCH
		if (!g_LoadfakeBatteryData) {
			if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_DesignCapacity, &ui16Data))
				ui16Data = 0xFFFF;
		} else {
			ui16Data = FAKE_BATTERY_CAPACITY;		// Load fake battery data
		}
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
#else
		if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_DesignCapacity, &ui16Data))
			ui16Data = 0xFFFF;
		*pui8Data = (uint8_t)((ui16Data >> 8) & 0xff);
#endif		
	}
}

/**
 * @brief ACPI battery measurement accuracy handler ECRAM 0xD0~D1
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryMeasurementAccuracyHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t _s_u16Value;

	/*
	 * Return LSB of previously saved battery data.
	 */
	if(isRead && (ui8Idx == ACPI_BATTERY_MEASUREACCURACY)) {
		*pui8Data = (uint8_t)(_s_u16Value & 0xff);
	}

	/*
	 * Save current measurement accuracy data, return MSB
	 */
	else if(isRead && (ui8Idx == ACPI_BATTERY_MEASUREACCURACY + 1)) {
//		_s_u16Value = app_battery_lowThresholdGet(app_battery_selectedGet());
		_s_u16Value = 96000 >> 8; // Fix to 96% for now
		*pui8Data = (uint8_t)((_s_u16Value >> 8) & 0xff);
	}
}

/**
 * @brief ACPI battery count handler ECRAM 0xEB
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryCountHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	/*
	 * Return the current alert status.
	 */
	if(isRead)
	{
		*pui8Data = (uint8_t)app_battery_countGet();
	}
}

/**
 * @brief ACPI battery select handler ECRAM 0xEC
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatterySelectHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	/*
	 * Return the currently selected battery.
	 */
	if(isRead) {
		*pui8Data = app_battery_selectedGet();
	}

	/*
	 * Select a new battery.
	 */
	else {
		app_battery_selectedSet(*pui8Data);
	}
}

/**
 * @brief ACPI battery alert handler ECRAM 0xEF
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryAlertHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	/*
	 * Return the current alert status.
	 */
	if(isRead)
	{
		*pui8Data = (uint8_t)(board_battery_alartStatusGetAndClear() & 0xff);
	}
}
#endif

/**
 * @brief ACPI asl Gp Flag handler ECRAM 0xF0
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPI_aslGpFlag (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if(isRead) {
		uint8_t supportS0i3KbcWake;
		uint8_t m_espi_ResetInS0i3;
		GET_NV_OPTIONS(f_s0i3KbcWake, supportS0i3KbcWake);
		GET_NV_OPTIONS(espi_ResetInS0i3 , m_espi_ResetInS0i3);
		/**
		 ASL GP Flags used to control KBC restore sequence doing by ASL upon S0i3 exit
		 It covers the KBC wake case and KBC restore case (other wake source than KBC)

		 As thus, a meaningful flag should be returned only if
			a) KBC Wake of S0i3 is enabled
			b) eSPI bus is in reset status in S0i3 */

		if (supportS0i3KbcWake&& m_espi_ResetInS0i3) {
			if ( MS_uPEP_HOOK_DISPLAY_OFF == g_uPepHook ||    /* Display off */
				   MS_uPEP_HOOK_DRIPs_ENTER == g_uPepHook ||  /* DRIPs_ENTER (20H2) */
				   MS_uPEP_HOOK_MS_ENTER == g_uPepHook ) {    /* MS_ENTER (20H2) */
				g_aslGpFlags = 0xFF;
			}
			if (MS_uPEP_HOOK_Q_EVT == g_uPepHook) {           /* _Q70 */
				g_aslGpFlags = 0x22;                          /* ENKB + WKB4 */
			}

			*pui8Data = g_aslGpFlags;
			g_aslGpFlags = 0;

		} else {
			*pui8Data = 0xFF;
		}
	}
}

/**
 * @brief ACPI uPep Flag handler ECRAM 0xF1
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPI_uPepHook (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	extern uint8_t g_ui8isKbcWakeEvent;
	uint8_t optionVal;

	if(isRead) {
		*pui8Data = g_uPepHook;
	} else {
		g_uPepHook = *pui8Data;
		LOG_DBG("[S0i3]     uPEP hook Fun: %d; KBC Wake: %d", g_uPepHook, g_ui8isKbcWakeEvent);
		if (MS_uPEP_HOOK_DISPLAY_OFF == g_uPepHook) {
			if (postcode_led_hub_get_sts()) {
				/*
				 * if postcode is currently enabled
				 * and _s_alignPostCodeWithScreenBacklight is true
				 */
				if (_s_alignPostCodeWithScreenBacklight) {
					postcode_led_hub_32bit_turnOff();
					postcode_led_hub_set_sts(false);
				}
			}
		} else if (MS_uPEP_HOOK_DISPLAY_ON == g_uPepHook) {     /* Display on */
#if CONFIG_SLEEP
			board_slp_disableKSxWakeup();
#endif
			if (!postcode_led_hub_get_sts()) {
				GET_NV_OPTIONS(f_TurnOnPostCode, optionVal);
				/*
				 * if postcode is not force off for PPO testing (f_TurnOnPostCode)
				 * and _s_alignPostCodeWithScreenBacklight is true
				 */
				if (optionVal && _s_alignPostCodeWithScreenBacklight) {
					postcode_led_hub_set_sts(true);
					postcode_led_hub_32bit_turnOn(16);
				}
			}
		} else if (MS_uPEP_HOOK_OS_ENTER == g_uPepHook) {
#if CONFIG_BATTERY_MANAGEMENT
			app_smtbty_unfrozen();
#endif
		}
	}
}

#if CONFIG_BATTERY_MANAGEMENT
/**
 * @brief ACPI uPep Flag handler ECRAM 0xF1
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIChargerStatusHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Status = 0;

	if (isRead) {
#if CONFIG_ACDCSWITCH
		uint8_t AcDcSwitchEnableFlag;
		GET_NV_OPTIONS(chg_AcDcSwitch, AcDcSwitchEnableFlag);

		if (AcDcSwitchEnableFlag) {
			/* AC/DC switch is on and in DC mode */
			ui8Status = g_ui8Virtual_ACDC_STATUS;
		} else 
#endif		
		{
			if ( ( app_battery_flagsCacheGet(app_battery_selectedGet()).u32flag & (APP_SMTBTY_CONNECTED | APP_SMTBTY_DEAD) ) == APP_SMTBTY_CONNECTED ) {
				ui8Status |=  ACPI_BATTERY_CONNECTED;
			} else {
				ui8Status &= ~ACPI_BATTERY_CONNECTED;
			}

			if (board_pwrSrc_hasAcPowerSource()) {
				ui8Status |=  ACPI_AC_CONNECTED;
			} else {
				ui8Status &= ~ACPI_AC_CONNECTED;
			}
		}
		*pui8Data = ui8Status;
	}
}

/**
 * @brief ACPI status handler ECRAM 0xED
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPIBatteryStatusHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Status = 0;

	if(isRead) {

		uint32_t ui32BatSt;
		uint16_t u16BatRemainCapacity = 0, u16BatFullChargeCapacity = 0;
		int16_t i16BatteryCurrent = 0;
		ui32BatSt = (uint32_t)app_battery_flagsCacheGet(app_battery_selectedGet()).u32flag;
		dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_RemainingCapacity, &u16BatRemainCapacity);
		dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_FullChargeCapacity, &u16BatFullChargeCapacity);
		dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_Current, &i16BatteryCurrent);

		if( (board_pwrSrc_hasAcPowerSource()) &&
			(u16BatRemainCapacity < u16BatFullChargeCapacity) &&
			(i16BatteryCurrent > 0) &&
			(ui32BatSt & APP_SMTBTY_CHARGING) ) {
			ui8Status |= AMD_BATTERY_CHARGING;
		} else {
			ui8Status &= ~AMD_BATTERY_CHARGING;
		}

		if ( ui32BatSt & APP_SMTBTY_TRIPPED ) {
			ui8Status |= AMD_BATTERY_TRIPPED;
		} else {
			ui8Status &= ~AMD_BATTERY_TRIPPED;
		}

		*pui8Data = ui8Status;
	} else {
		uint8_t u8tmp = *pui8Data;

		if (u8tmp & AMD_BATTERY_TRIPPED) {
			app_battery_clearTrippedFlag(app_battery_selectedGet());
		}
	}
}
#endif

#if CONFIG_ACPI_DAC_PWR
/**
 * PMIC/VR adjustment
 */

#define ACPI_PMIC_TUNNEL_SUCCESS        (0xAC)
#define ACPI_PMIC_TUNNEL_ERROR          (0xE2)
#define ACPI_PMIC_TUNNEL_ONGOING        (0xCC)

static uint8_t _s_pmic_addr = 0;        // ECRAMx92
static uint8_t _s_pmic_idx = 0;         // ECRAMx90
static uint8_t _s_pmic_val = 0;         // ECRAMx91
static uint8_t _s_pmic_status = 0xe2;   // ECRAMx92

/**
 * @brief ACPI PMIC handler ECRAM 0x90
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
void ACPI_PMIC_TunnelHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (isRead) {
		switch (ui8Idx) {
			case ACPI_PMIC_TUNNEL_BASE:       *pui8Data = _s_pmic_idx; break;
			case ACPI_PMIC_TUNNEL_BASE + 1:   *pui8Data = _s_pmic_val; break;
			case ACPI_PMIC_TUNNEL_BASE + 2:   *pui8Data = _s_pmic_status; break;
			default:
				return;
		}
	} else {
		_s_pmic_status = ACPI_PMIC_TUNNEL_ONGOING;
		switch (ui8Idx) {
			case ACPI_PMIC_TUNNEL_BASE:        _s_pmic_idx = *pui8Data;	break;
			case ACPI_PMIC_TUNNEL_BASE + 1:    _s_pmic_val = *pui8Data;	break;
			case ACPI_PMIC_TUNNEL_BASE + 2:    _s_pmic_addr = *pui8Data; board_init_pmicTunnelTrigger(); break;
			default:
				return;
		}
	}
}

/**
 * @brief ACPI PMIC I2C tunnel process
 */
void ACPI_PMIC_TunnelProc (void)
{
	int ret = 0;
	if (_s_pmic_addr & 0x01) {  /* I2C read */
		ret = i2c_hub_write_read(I2C_DAC_PWR_PORT, (_s_pmic_addr >> 1), &_s_pmic_idx, 1, &_s_pmic_val, 1);

		if (ret == 0) {
			_s_pmic_status = ACPI_PMIC_TUNNEL_SUCCESS;
		} else {
			_s_pmic_status = ACPI_PMIC_TUNNEL_ERROR;
		}
		LOG_DBG("Read  PMIC/VR on I2C [%d] Addr: %02X, Idx: %02X, Val: %02X; ret = %d",
			I2C_DAC_PWR_PORT, _s_pmic_addr, _s_pmic_idx, _s_pmic_val, ret);
	} else {                    /* I2C write */
		ret = i2c_hub_burst_write_multi_cmd(I2C_DAC_PWR_PORT, (_s_pmic_addr >> 1), &_s_pmic_idx, 1, &_s_pmic_val, 1);
		if (ret == 0) {
			_s_pmic_status = ACPI_PMIC_TUNNEL_SUCCESS;
		} else {
			_s_pmic_status = ACPI_PMIC_TUNNEL_ERROR;
		}
		LOG_DBG("Write PMIC/VR on I2C [%d] Addr: %02X, Idx: %02X, Val: %02X; ret = %d",
			I2C_DAC_PWR_PORT, _s_pmic_addr, _s_pmic_idx, _s_pmic_val, ret);
	}
}
#endif /* CONFIG_ACPI_DAC_PWR */

/**
 * @brief ACPI USB status handler ECRAM 0xFE
 *
 * @param     isRead:     True is read and false is write
 * @param     ui8Idx:     ECRAM index
 * @param     pui8Data:   Read and write data buffer
 */
#if CONFIG_USBC_POWER_DELIVERY
void ACPI_UsbStatus (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	uint8_t ui8Status = 0;

	if (isRead) {
		*pui8Data = 0;

		if (g_forceRtPower)
			*pui8Data |= 0x80;

		if (g_usb4Disable)
			*pui8Data |= 0x10;

		if (g_tbt3Disable)
			*pui8Data |= 0x20;

		if (g_portDisable)
			*pui8Data |= 0x40;
	} else {
		ui8Status = *pui8Data;

		/* bit-4/5 assert, it means disable USB4 TBT3 both */
		if ((ui8Status & 0x30) == 0x30) {
			if (!((g_usb4Disable == true) && (g_tbt3Disable == true))) {
				LOG_DBG("Disable USB4 and TBT3");
				g_usb4Disable = true;
				g_tbt3Disable = true;
				if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					app_4cc_usb4Tbt3Disable(TYPEC_PORT_0_IDX);
					app_4cc_usb4Tbt3Disable(TYPEC_PORT_1_IDX);
#if PD_TRIPPORT_ENABLE
					app_4cc_usb4Tbt3Disable(TYPEC_PORT_2_IDX);
#endif
#endif /* CONFIG_USBC_4CC */
				} else if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					amd_crb_drivers_hpi_usb4Tbt3Disable(TYPEC_PORT_0_IDX, 0);
					amd_crb_drivers_hpi_usb4Tbt3Disable(TYPEC_PORT_1_IDX, 0);
#if PD_TRIPPORT_ENABLE
					amd_crb_drivers_hpi_usb4Tbt3Disable(TYPEC_PORT_2_IDX, 0);
#endif
#endif /* CONFIG_USBC_CCGX */
				}
			}
		/* bit-4/5 de-assert, it means enable USB4 TBT3 both */
		} else if ((ui8Status & 0x30) == 0x00) {
			if (!((g_usb4Disable == false) && (g_tbt3Disable == false))) {
				LOG_DBG("Enable USB4 and TBT3");
				g_usb4Disable = false;
				g_tbt3Disable = false;
				if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					app_4cc_usb4Tbt3Enable(TYPEC_PORT_0_IDX);
					app_4cc_usb4Tbt3Enable(TYPEC_PORT_1_IDX);
#if PD_TRIPPORT_ENABLE
					app_4cc_usb4Tbt3Enable(TYPEC_PORT_2_IDX);
#endif
#endif /* CONFIG_USBC_4CC */
				} else if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					amd_crb_drivers_hpi_usb4Tbt3Enable(TYPEC_PORT_0_IDX, 0);
					amd_crb_drivers_hpi_usb4Tbt3Enable(TYPEC_PORT_1_IDX, 0);
#if PD_TRIPPORT_ENABLE
					amd_crb_drivers_hpi_usb4Tbt3Enable(TYPEC_PORT_2_IDX, 0);
#endif
#endif /* CONFIG_USBC_CCGX */
				}
			}
		/* bit-4 assert, it means disable USB4 only */
		} else if ((ui8Status & 0x30) == 0x10) {
			if (!((g_usb4Disable == true) && (g_tbt3Disable == false))) {
				LOG_DBG("Disable USB4 and Enable TBT3");
				g_usb4Disable = true;
				g_tbt3Disable = false;
				if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					app_4cc_usb4DisableTbt3Enable(TYPEC_PORT_0_IDX);
					app_4cc_usb4DisableTbt3Enable(TYPEC_PORT_1_IDX);
#if PD_TRIPPORT_ENABLE
					app_4cc_usb4DisableTbt3Enable(TYPEC_PORT_2_IDX);
#endif
#endif /* CONFIG_USBC_4CC */
				} else if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					amd_crb_drivers_hpi_usb4DisableTbt3Enable(TYPEC_PORT_0_IDX, 0);
					amd_crb_drivers_hpi_usb4DisableTbt3Enable(TYPEC_PORT_1_IDX, 0);
#if PD_TRIPPORT_ENABLE
					amd_crb_drivers_hpi_usb4DisableTbt3Enable(TYPEC_PORT_2_IDX, 0);
#endif
#endif /* CONFIG_USBC_CCGX */
				}
			}
		/* bit-5 assert, it means disable TBT3 only */
		} else if ((ui8Status & 0x30) == 0x20) {
			if (!((g_usb4Disable == false) && (g_tbt3Disable == true))) {
				LOG_DBG("Enable USB4 and disable TBT3 ");
				g_usb4Disable = false;
				g_tbt3Disable = true;
				if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					app_4cc_usb4EnableTbt3Disable(TYPEC_PORT_0_IDX);
					app_4cc_usb4EnableTbt3Disable(TYPEC_PORT_1_IDX);
#if PD_TRIPPORT_ENABLE
					app_4cc_usb4EnableTbt3Disable(TYPEC_PORT_2_IDX);
#endif
#endif /* CONFIG_USBC_4CC */
				} else if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					amd_crb_drivers_hpi_usb4EnableTbt3Disable(TYPEC_PORT_0_IDX, 0);
					amd_crb_drivers_hpi_usb4EnableTbt3Disable(TYPEC_PORT_1_IDX, 0);
#if PD_TRIPPORT_ENABLE
					amd_crb_drivers_hpi_usb4EnableTbt3Disable(TYPEC_PORT_2_IDX, 0);
#endif
#endif /* CONFIG_USBC_CCGX */
				}
			}
		}

		if (ui8Status & 0x40)
		{
			/* Disable port */
			if (g_portDisable == false) {
				if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_NULL);
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_NULL);
#if PD_TRIPPORT_ENABLE
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_2_IDX, PRT_NULL);
#endif /* PD_TRIPPORT_ENABLE */
#endif /* CONFIG_USBC_4CC */
				} else if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					amd_crb_drivers_hpi_portDisable(TYPEC_PORT_0_IDX, 0);
					amd_crb_drivers_hpi_portDisable(TYPEC_PORT_1_IDX, 0);
#if PD_TRIPPORT_ENABLE
					amd_crb_drivers_hpi_portDisable(TYPEC_PORT_0_IDX, 1);
#endif
#endif /* CONFIG_USBC_CCGX */
				}
				g_portDisable = true;
			}
		} else {
			/* Enable port */
			if (g_portDisable == true) {
				if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_0_IDX, PRT_DUAL);
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_1_IDX, PRT_DUAL);
#if PD_TRIPPORT_ENABLE
					amd_crb_drivers_tps_portConfigStateMachine(TYPEC_PORT_2_IDX, PRT_DUAL);
#endif /* PD_TRIPPORT_ENABLE */
#endif /* CONFIG_USBC_4CC */
				} else if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					amd_crb_drivers_hpi_resetChip(0);
#if PD_TRIPPORT_ENABLE
					amd_crb_drivers_hpi_resetChip(1);
#endif
#endif /* CONFIG_USBC_CCGX */
				}
				g_portDisable = false;
			}
		}
		/* process the RT force power on/off for FW upgrade */
		if (ui8Status & 0x80) {
			if (g_forceRtPower == false) {
				g_forceRtPower = true;
				if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					app_4cc_forceRtPower(TYPEC_PORT_0_IDX, true);  /* Enable TR0 */
					app_4cc_forceRtPower(TYPEC_PORT_1_IDX, true);  /* Enable TR1 */
#if PD_TRIPPORT_ENABLE
					app_4cc_forceRtPower(TYPEC_PORT_2_IDX, true);  /* Enable TR2 */
#endif
#endif /* CONFIG_USBC_4CC */
				} else if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					app_ccgx_forceUsb4Tbt3(0, true);  /* Enable TR0/1 */
#if PD_TRIPPORT_ENABLE
					app_ccgx_forceUsb4Tbt3(1, true);  /* Enable TR2 */
#endif
#endif /* CONFIG_USBC_CCGX */
				}
			}
		} else {
			if (g_forceRtPower == true) {
				g_forceRtPower = false;
				if (app_usbc_readDevID() == PD_DEVICE_ID_TIx) {
#if CONFIG_USBC_4CC
					app_4cc_forceRtPower(TYPEC_PORT_0_IDX, false);  /* disable TR0 */
					app_4cc_forceRtPower(TYPEC_PORT_1_IDX, false);  /* disable TR1 */
#if PD_TRIPPORT_ENABLE
					app_4cc_forceRtPower(TYPEC_PORT_2_IDX, false);  /* disable TR2 */
#endif
#endif /* CONFIG_USBC_4CC */
				} else if (app_usbc_readDevID() == PD_DEVICE_ID_CCGx) {
#if CONFIG_USBC_CCGX
					app_ccgx_forceUsb4Tbt3(0, false);  /* Disable TR0/1 */
#if PD_TRIPPORT_ENABLE
					app_ccgx_forceUsb4Tbt3(1, false);  /* Disable TR2 */
#endif
#endif /* CONFIG_USBC_CCGX */
				}
			}
		}
	}
}
#endif
//*****************************************************************************
//
// VW MMIO tunnel implementation
//
//*****************************************************************************

/**
 * @brief VW MMIO tunnel work handler
 */
static void vwMmio_eventProc(struct k_work *work)
{
	LOG_DBG("VW MMIO tunnel work triggered");
	const struct device *espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));
	int ret = 0;
	
	/* 0: idle, 1: TEMP VW, 2: RTC VW */
	if (_s_vw_mmio_val & 0x01) {
		/* CPU temperature VW trigger */
		LOG_DBG("CPU TEMP VW trigger requested");
		
		if (device_is_ready(espi_dev)) {
			/* Send VW pulse: HIGH -> LOW -> HIGH */
			ret = espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_CPU_TEMP_READ, 1);
			if (ret == 0) {
				k_msleep(10); /* 10ms delay */
				ret = espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_CPU_TEMP_READ, 0);
				if (ret == 0) {
					k_msleep(10); /* 10ms delay */
					ret = espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_CPU_TEMP_READ, 1);
				}
			}
			if (ret != 0) {
				LOG_ERR("Failed to send CPU_TEMP VW signal: %d", ret);
				_s_vw_mmio_status = ACPI_VW_MMIO_ERROR;
				return;
			}
		} else {
			LOG_ERR("eSPI device not ready");
			_s_vw_mmio_status = ACPI_VW_MMIO_ERROR;
			return;
		}
	}
	
	if (_s_vw_mmio_val & 0x02) {
		/* RTC VW trigger */
		LOG_DBG("RTC VW trigger requested");
		
		if (device_is_ready(espi_dev)) {
			/* Send VW pulse: HIGH -> LOW -> HIGH */
			ret = espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_RTC_READ, 1);
			if (ret == 0) {
				k_msleep(10); /* 10ms delay */
				ret = espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_RTC_READ, 0);
				if (ret == 0) {
					k_msleep(10); /* 10ms delay */
					ret = espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_RTC_READ, 1);
				}
			}
			if (ret != 0) {
				LOG_ERR("Failed to send RTC_READ VW signal: %d", ret);
				_s_vw_mmio_status = ACPI_VW_MMIO_ERROR;
				return;
			}
		} else {
			LOG_ERR("eSPI device not ready");
			_s_vw_mmio_status = ACPI_VW_MMIO_ERROR;
			return;
		}
	}
	
	_s_vw_mmio_status = ACPI_VW_MMIO_SUCCESS;
}

/**
 * @brief VW MMIO ACPI handler
 *
 * @param isRead Host read or write.
 * @param ui8Idx acpi offset
 * @param pui8Data pointer to buffer holding the data.
 */
uint8_t ACPI_VW_MMIO_Handler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	extern uint8_t gs_eSpiMmioSpace[CONFIG_ESPI_NPCX_PERIPHERAL_ACPI_SHD_MEM_SIZE];

	if (ui8Idx >= 0x31) {
		return 0;
	}
	if (ui8Idx == 0x30) {

		return 1;

	} else if (ui8Idx < 0x30) {
		if (isRead) {
			switch (ui8Idx) {
			case 0:
				/* 0: idle, 1: TEMP VW, 2: RTC VW */
				*pui8Data = _s_vw_mmio_val;
				break;

			case 1:
				/* response of the event */
				*pui8Data = _s_vw_mmio_status;
				break;

			case 2:
			case 3:
			case 4:
			case 5:
				/* read the CPU TEMP */
				*pui8Data = gs_eSpiMmioSpace[0x00 + ui8Idx - 2];
				break;

			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
				/* read the RTC */
				*pui8Data = gs_eSpiMmioSpace[0x50 + ui8Idx - 6];
				break;

			default:
				*pui8Data = 0;
				break;
			}
		} else {
			switch (ui8Idx) {
			case 0:
				_s_vw_mmio_val = *pui8Data;
				/* 0: idle, 1: TEMP VW, 2: RTC VW */
				if (_s_vw_mmio_val == 0) {
					_s_vw_mmio_status = ACPI_VW_MMIO_SUCCESS;
				} else if ((_s_vw_mmio_val == 1) || (_s_vw_mmio_val == 2) ||
					   (_s_vw_mmio_val == 3)) {
					_s_vw_mmio_status = ACPI_VW_MMIO_ONGOING;
					/* trigger VW and MMIO event */
					k_work_submit(&vwMmio_work);
				} else {
					/* response error if write other value except 0,1,2 */
					_s_vw_mmio_status = ACPI_VW_MMIO_ERROR;
				}
				break;

			case 1:
				// Do not support write to status register
				break;

			default:
				break;
			}
		}
	}
	return 1;
}


#if CONFIG_ALS
/**
 * @brief ACPI hyperplane handler for OPT4048 ALS (registered at plane 0xB2)
 *
 * ACPI register map (plane 0xB2):
 *   Offset 0-1:  ALS lux value (uint16_t)
 *   Offset 2-3:  Red channel (uint16_t)
 *   Offset 4-5:  Green channel (uint16_t)
 *   Offset 6-7:  Blue channel (uint16_t)
 *   Offset 8-9:  White/IR channel (uint16_t)
 *   Offset 10-11: CCT in Kelvin (uint16_t)
 *
 * @param isRead    1 for read, 0 for write
 * @param ui8Idx    Register offset (0-11)
 * @param pui8Data  Pointer to data byte
 * @return          0 on success (required by MD_ACPI_HYPERPLANE_LAUNCHER)
 */
uint8_t app_opt4048_acpi_handler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	static uint16_t snapshot_lux, snapshot_red, snapshot_green;
	static uint16_t snapshot_blue, snapshot_white, snapshot_cct;
	uint16_t acpi_als_lux, acpi_als_red, acpi_als_green;
	uint16_t acpi_als_blue, acpi_als_white, acpi_als_cct;
	/* Get current ALS data from app thread */
	app_opt4048_get_acpi_data(&acpi_als_lux, &acpi_als_red, &acpi_als_green,
	                          &acpi_als_blue, &acpi_als_white, &acpi_als_cct);
	if (isRead) {
		switch (ui8Idx) {
			/* ALS Lux value */
			case 1:
				snapshot_lux = acpi_als_lux;
				*pui8Data = (snapshot_lux >> 8) & 0xFF;
				break;
			case 0:
				*pui8Data = snapshot_lux & 0xFF;
				break;
			case ACPI_LIGHT_SENSOR_LUX + 1:
				snapshot_lux = acpi_als_lux;
				*pui8Data = (snapshot_lux >> 8) & 0xFF;
				break;
			case ACPI_LIGHT_SENSOR_LUX:
				*pui8Data = snapshot_lux & 0xFF;
				break;
			/* Red channel */
			case 3:
				snapshot_red = acpi_als_red;
				*pui8Data = (snapshot_red >> 8) & 0xFF;
				break;
			case 2:
				*pui8Data = snapshot_red & 0xFF;
				break;
			/* Green channel */
			case 5:
				snapshot_green = acpi_als_green;
				*pui8Data = (snapshot_green >> 8) & 0xFF;
				break;
			case 4:
				*pui8Data = snapshot_green & 0xFF;
				break;
			/* Blue channel */
			case 7:
				snapshot_blue = acpi_als_blue;
				*pui8Data = (snapshot_blue >> 8) & 0xFF;
				break;
			case 6:
				*pui8Data = snapshot_blue & 0xFF;
				break;
			/* White/IR channel */
			case 9:
				snapshot_white = acpi_als_white;
				*pui8Data = (snapshot_white >> 8) & 0xFF;
				break;
			case 8:
				*pui8Data = snapshot_white & 0xFF;
				break;
			/* CCT (Correlated Color Temperature) */
			case 11:
				snapshot_cct = acpi_als_cct;
				*pui8Data = (snapshot_cct >> 8) & 0xFF;
				break;
			case 10:
				*pui8Data = snapshot_cct & 0xFF;
				break;
			default:
				*pui8Data = 0;
				break;
		}
	}
	/* Write operations not supported for ALS data */
	return 0;
}
/**
 * @brief ACPI handler wrapper (void return for handler table)
 */
void app_opt4048_acpi_wrapper(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	(void)app_opt4048_acpi_handler(isRead, ui8Idx, pui8Data);
}
#endif /* CONFIG_ALS */

/**
 * Table of ACPI handlers to be supported in this application.
 */
const MD_ACPIPLANES_HANDLER_TABLE g_sAPCIHandlerTable[] =
{

	{ 0x5F,                                     AcpiFakeNotifyQueueHandler},
#if 0 /*TODO*/
	/* 0x60 - 0x8F UCSI */
	{ ACPI_UCSI_BASE,                           f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  1,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  2,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  3,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  4,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  5,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  6,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  7,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  8,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE +  9,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 10,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 11,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 12,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 13,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 14,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 15,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 16,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 17,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 18,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 19,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 20,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 21,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 22,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 23,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 24,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 25,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 26,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 27,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 28,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 29,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 30,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 31,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 32,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 33,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 34,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 35,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 36,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 37,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 38,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 39,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 40,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 41,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 42,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 43,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 44,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 45,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 46,                      f_ucsi_tunnel_AcpiSpaceHandler },
	{ ACPI_UCSI_BASE + 47,                      f_ucsi_tunnel_AcpiSpaceHandler },
#endif /*TODO*/
#if CONFIG_ACPI_DAC_PWR
	/* 0x90 */
	{ ACPI_PMIC_TUNNEL_BASE,                    ACPI_PMIC_TunnelHandler },
	{ ACPI_PMIC_TUNNEL_BASE + 1,                ACPI_PMIC_TunnelHandler },
	{ ACPI_PMIC_TUNNEL_BASE + 2,                ACPI_PMIC_TunnelHandler },
#endif /* CONFIG_ACPI_DAC_PWR */
	/* 0x93 */
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE,          acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 1,      acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 2,      acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 3,      acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 4,      acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 5,      acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 6,      acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 7,      acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 8,      acpi_brdIdExpField_Handler },
	{ ACPI_BOARD_ID_EXPORT_FIELD_BASE + 9,      acpi_brdIdExpField_Handler },

	/* 0x9D */
	{ ACPI_GPIO_BASE,                           Board_Gpio_AcpiHandler },
	{ ACPI_GPIO_BASE + 1,                       Board_Gpio_AcpiHandler },
	{ ACPI_GPIO_BASE + 2,                       Board_Gpio_AcpiHandler },
	{ ACPI_GPIO_BASE + 3,                       Board_Gpio_AcpiHandler },
	{ ACPI_GPIO_BASE + 4,                       Board_Gpio_AcpiHandler },
	{ ACPI_GPIO_BASE + 5,                       Board_Gpio_AcpiHandler },
	{ ACPI_GPIO_BASE + 6,                       Board_Gpio_AcpiHandler },
	{ ACPI_GPIO_BASE + 7,                       Board_Gpio_AcpiHandler },
	/* 0xA5 */
	{ ACPI_GPIO_BASE + 8,                       Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 9,                       Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 10,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 11,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 12,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 13,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 14,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 15,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 16,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 17,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 18,                      Board_Gpio_AcpiHandler }, // RW
	{ ACPI_GPIO_BASE + 19,                      Board_Gpio_AcpiHandler }, // RW

	// 0xB2 : DC Prochot setting
#if CONFIG_APP_THERMAL
	{ ACPI_SWITCH_BASE - 4,                     AcpiSwitchHandler_ProchotSetting00 },
#endif
	{ ACPI_SWITCH_BASE - 3,                     AcpiSwitchHandler_ProchotSetting01 },
	{ ACPI_SWITCH_BASE - 2,                     AcpiSwitchHandler_ProchotSetting02 },
	{ ACPI_SWITCH_BASE - 1,                     AcpiSwitchHandler_ProchotSetting03 },

	/* 0xB6 : switches group 1 */
	{ ACPI_SWITCH_BASE,                         AcpiSwitchHandler_SW01 },
	/* 0xB7 : switches group 2 */
	{ ACPI_SWITCH_BASE + 1,                     AcpiSwitchHandler_SW02 },

	/* 0xB8 firmware version string (5 + 4 + 6) */
	{ ACPI_EC_FW_VERSION,                       AcpiEcVersionHandler },
	{ ACPI_EC_FW_VERSION + 1,                   AcpiEcVersionHandler },
	{ ACPI_EC_FW_VERSION + 2,                   AcpiEcVersionHandler },
	{ ACPI_EC_FW_VERSION + 3,                   AcpiEcVersionHandler },
	{ ACPI_EC_FW_VERSION + 4,                   AcpiEcVersionHandler },

	/* 0xBD */
	{ ACPI_EC_FW_BUILD_DATE,                    AcpiEcBuildDateHandler },
	{ ACPI_EC_FW_BUILD_DATE + 1,                AcpiEcBuildDateHandler },
	{ ACPI_EC_FW_BUILD_DATE + 2,                AcpiEcBuildDateHandler },
	{ ACPI_EC_FW_BUILD_DATE + 3,                AcpiEcBuildDateHandler },

	/* 0xC1 */
	{ ACPI_EC_FW_BUILD_TIME,                    AcpiEcBuildTimeHandler },
	{ ACPI_EC_FW_BUILD_TIME + 1,                AcpiEcBuildTimeHandler },
	{ ACPI_EC_FW_BUILD_TIME + 2,                AcpiEcBuildTimeHandler },
	{ ACPI_EC_FW_BUILD_TIME + 3,                AcpiEcBuildTimeHandler },
	{ ACPI_EC_FW_BUILD_TIME + 4,                AcpiEcBuildTimeHandler },
	{ ACPI_EC_FW_BUILD_TIME + 5,                AcpiEcBuildTimeHandler },

	/* 0xC7 ACPI_ACDC_SWITCH_BASE */
	{ ACPI_DC_TIME,                             ACPIDCTimeHandler },
	/* 0xC8 ACPI_ACDC_SWITCH_BASE + 1 */
	{ ACPI_AC_TIME,                             ACPIACTimeHandler },

	/* DPTC margin */
	//Offset(0xC9),
	//{F_DPTC_ACPI_SPACE_BASE,                    f_dptc_acpiHandler},
	//{F_DPTC_ACPI_SPACE_BASE + 1,                f_dptc_acpiHandler},

	/* 0xCB */
	{ ACPI_SYS_EVENT_FLAGS,                     ACPIEcEventFlagsHandler1 },

	/* 0xCC */
	{ ACPI_SYS_EVENT_FLAGS + 1,                 ACPIEcEventFlagsHandler2 },

	/* 0xCD */
	{ ACPI_SYS_EVENT_FLAGS + 2,                 AcpiSwitchHandler_SW03 },

	/* 0xCE */
	{ ACPI_SYS_EVENT_FLAGS + 3,                 AcpiSwitchHandler_SW04 },

	/* 0xCF */
	{ ACPI_MISC_STATUS_CONTROL,                 ACPIMiscStatusControlHandler },

#if CONFIG_BATTERY_MANAGEMENT
	/* 0xD0 ACPI_BATTERY_BASE */
	{ ACPI_BATTERY_MEASUREACCURACY,             ACPIBatteryMeasurementAccuracyHandler },
	{ ACPI_BATTERY_MEASUREACCURACY + 1,         ACPIBatteryMeasurementAccuracyHandler },
	/* ACPI_BATTERY_BASE + 0x02/0x03 */
	{ ACPI_BATTERY_LOW,                         ACPIBatteryLowHandler },
	{ ACPI_BATTERY_LOW + 1,                     ACPIBatteryLowHandler },
	/* ACPI_BATTERY_BASE + 0x04/0x05 */
	{ ACPI_BATTERY_CRITICAL,                    ACPIBatteryCriticalHandler },
	{ ACPI_BATTERY_CRITICAL + 1,                ACPIBatteryCriticalHandler },
	/* ACPI_BATTERY_BASE + 0x06/0x07 */
	{ ACPI_BATTERY_TRIP_POINT,                  ACPIBatteryTripPointHandler },
	{ ACPI_BATTERY_TRIP_POINT + 1,              ACPIBatteryTripPointHandler },
	/* ACPI_BATTERY_BASE + 0x08/0x09 */
	{ ACPI_BATTERY_TEMPERATURE,                 ACPIBatteryTemperatureHandler },
	{ ACPI_BATTERY_TEMPERATURE + 1,             ACPIBatteryTemperatureHandler },
	/* ACPI_BATTERY_BASE + 0x0A/0x0B */
	{ ACPI_BATTERY_MAX_ERROR,                   ACPIBatteryMaxErrorHandler },
	{ ACPI_BATTERY_MAX_ERROR + 1,               ACPIBatteryMaxErrorHandler },
	/* ACPI_BATTERY_BASE + 0x0C/0x0D */
	{ ACPI_BATTERY_CYCLE_COUNT,                 ACPIBatteryCycleCountHandler },
	{ ACPI_BATTERY_CYCLE_COUNT + 1,             ACPIBatteryCycleCountHandler },
	/* ACPI_BATTERY_CURRENT + 0x0E/0x0F */
	{ ACPI_BATTERY_CURRENT,                     ACPIBatteryCurrentHandler },
	{ ACPI_BATTERY_CURRENT + 1,                 ACPIBatteryCurrentHandler },
	/* ACPI_BATTERY_VOLTAGE + 0x10/0x11 */
	{ ACPI_BATTERY_VOLTAGE,                     ACPIBatteryVoltageHandler },
	{ ACPI_BATTERY_VOLTAGE + 1,                 ACPIBatteryVoltageHandler },
	/* ACPI_BATTERY_VOLTAGE + 0x12/0x13 */
	{ ACPI_BATTERY_DESIGN_VOLTAGE,              ACPIBatteryDesignVoltageHandler },
	{ ACPI_BATTERY_DESIGN_VOLTAGE + 1,          ACPIBatteryDesignVoltageHandler },
	/* ACPI_BATTERY_VOLTAGE + 0x14/0x15 */
	{ ACPI_BATTERY_REMAINING_CAPACITY,          ACPIBatteryRemainingCapacityHandler },
	{ ACPI_BATTERY_REMAINING_CAPACITY + 1,      ACPIBatteryRemainingCapacityHandler },
	/* ACPI_BATTERY_VOLTAGE + 0x16/0x17 */
	{ ACPI_BATTERY_FULL_CHARGE_CAPACITY,        ACPIBatteryFullChargeCapacityHandler },
	{ ACPI_BATTERY_FULL_CHARGE_CAPACITY + 1,    ACPIBatteryFullChargeCapacityHandler },
	/* ACPI_BATTERY_VOLTAGE + 0x18/0x19 */
	{ ACPI_BATTERY_DESIGN_CAPACITY,             ACPIBatteryDesignCapacityHandler },
	{ ACPI_BATTERY_DESIGN_CAPACITY + 1,         ACPIBatteryDesignCapacityHandler },
	/* ACPI_BATTERY_CHARGE_THROTTLE + 0x1A */
	{ ACPI_BATTERY_CHARGE_THROTTLE,             ACPIBatteryChargeThrottle },
	/* ACPI_BATTERY_VOLTAGE + 0x1B/0x1C */
	{ ACPI_BATTERY_COUNT,                       ACPIBatteryCountHandler },
	{ ACPI_BATTERY_SELECT,                      ACPIBatterySelectHandler },
	/* ACPI_BATTERY_VOLTAGE + 0x1D */
	{ ACPI_BATTERY_STATUS,                      ACPIBatteryStatusHandler },
	/* ACPI_BATTERY_VOLTAGE + 0x1E */
	{ ACPI_CHARGER_STATUS,                      ACPIChargerStatusHandler },

	/* ACPI_BATTERY_VOLTAGE + 0x1F */
	{ ACPI_BATTERY_ALERT,                       ACPIBatteryAlertHandler },
#endif

	/* 0xF0 Bios hook */
	{ ACPI_BIOS_HOOK_SPACE,                     ACPI_aslGpFlag },
	{ ACPI_BIOS_HOOK_SPACE + 1,                 ACPI_uPepHook },
#if CONFIG_APP_THERMAL
	{ ACPI_THERMAL,                             ACPIThermalHandler },
	{ ACPI_THERMAL + 1,                         ACPIThermalHandler },
	{ ACPI_THERMAL + 2,                         ACPIThermalHandler },
	{ ACPI_THERMAL + 3,                         ACPIThermalHandler },
	{ ACPI_THERMAL + 4,                         ACPIThermalHandler },
	{ ACPI_THERMAL + 5,                         ACPIThermalHandler },
	{ ACPI_THERMAL + 6,                         ACPIThermalHandler },
	{ ACPI_THERMAL + 7,                         ACPIThermalHandler },
	{ ACPI_THERMAL + 8,                         ACPIThermalHandler },
	{ ACPI_THERMAL + 9,                         ACPIThermalHandler },
#endif
#if CONFIG_ALS
	/* 0xFC - 0xFD: Ambient Light Sensor lux only. Full 12-byte RGBW data available via hyperplane 0xB2 */
	{ ACPI_LIGHT_SENSOR_LUX,                    app_opt4048_acpi_wrapper },  /* 0xFC: Lux low byte */
	{ ACPI_LIGHT_SENSOR_LUX + 1,                app_opt4048_acpi_wrapper },  /* 0xFD: Lux high byte */
#endif
#if CONFIG_USBC_POWER_DELIVERY
	/* 0xFE */
	{ ACPI_USB_HOST_STATUS,                     ACPI_UsbStatus },
#endif
	{ ACPI_SWITCH_BASE5,                        AcpiSwitchHandler_SW05 },
};

/**
 * ACPI functions
 */
bool ACPI_isModernStandbyWakeupEvent(uint8_t ui8QEvt)
{
	switch (ui8QEvt) {
		case ACPI_SCI_KB_WAKEUP:
		case ACPI_SCI_LBDOCK_ON:
		case ACPI_SCI_LBDOCK_OFF:
		case ACPI_SCI_LID_OPEN:
		case ACPI_SCI_LID_CLOSE:
		case ACPI_SCI_TOUCH_PAD_STATUS_CHANGED:
		case ACPI_SCI_AC:
		case ACPI_SCI_BATTERY:
		case ACPI_SCI_BATTERY_TRIP_POINT:
		case ACPI_SCI_BATTERY_NOTIFY:
			return true;
		default:
			return false;
	}
}

#if CONFIG_HOT_BAG_EN
/**
 * @brief handler for the polling timer
 */
static void ACPI_pmfHeartBeatHandler(struct k_timer *timer)
{
	k_timer_stop(&g_pmfHeartBeatTimer);
	GET_NV_OPTIONS(hot_bag_enable, g_hotBagEnabled);
	if (g_hotBagEnabled && g_lidCloseEnabled && !board_pwrSrc_hasAcPowerSource() && !g_LoadfakeBatteryData) {
		if (g_powerSeq_criticalZone) {
			/* In critrical zone: suspend->SLP_S3 and SLP_S3->resume
				 if it take more than 300s with temp above 50 degrees trigger force g3*/
			if (highestSkinTemp > 50) {
				/* need to force G3 */
				LOG_ERR("Hot bag detected, force G3 from hot bag battery acpi monitor");
				app_pseq_nextStep(FORCE_G3, 1);
			}
		} else {
			LOG_ERR("Hot bag detected, force G3 from hot bag battery acpi monitor");
			app_pseq_nextStep(FORCE_G3, 1); /* need to force G3 */
		}
	}
}

/**
 * @brief stop the polling timer
 */
void ACPI_pmfHeartBeatStop(void)
{
	k_timer_stop(&g_pmfHeartBeatTimer);
}

/*	
 * @brief refresh the polling timer if needed
 *
 */
void ACPI_pmfHeartBeatRefresh(void)
{
	/* It will kill the timer if it is already running, and enable a new count down */
	k_timer_start(&g_pmfHeartBeatTimer,  K_MINUTES(5), K_NO_WAIT);
}
#endif

/**
 * @brief send qevent code to buffer
 *
 * @param     ui8QEvt:     qevent code
 */
void ACPI_NotifyHost(uint8_t ui8QEvt)
{
	if ( (!ACPI_isHostNotifyDisable()) ||
		(g_ui8MondernStandbySupport && SYSTEM_S3_STATE == app_pseq_systemState()  && ACPI_isModernStandbyWakeupEvent(ui8QEvt))) {

		enqueue_sci(ui8QEvt);

	} else {
		LOG_DBG("QEvt is disabled, skip to send 0x%02X.", ui8QEvt);
	}
}

/**
 * @brief clear qevent buffer
 */
void ACPI_ClearQueuedEvts (void)
{
	sci_queue_flush();
}

/**
 * @brief enable qevent report
 */
void ACPI_EnableHostNotify(void)
{
#if CONFIG_BATTERY_MANAGEMENT
	if (!g_acpi_state_flags.acpi_mode) {
#if CONFIG_CHARGER_ISL9241
		dev_isl9241_setProchot();
#elif CONFIG_CHARGER_MP2764
		amd_crb_drivers_mp2764_setProchot();
#endif
	}
#endif

	g_acpi_state_flags.acpi_mode = 1;
	LOG_DBG("Enable ACPI host notify.");

	if (_s_fakeEventId) {
		enqueue_sci(_s_fakeEventId);
		_s_fakeEventId = 0;
	}

}

/**
 * @brief disable qevent report
 */
void ACPI_DisableHostNotify(void)
{
	g_acpi_state_flags.acpi_mode = 0;
	LOG_DBG("Disable ACPI host notify.");

#if CONFIG_USBC_UCSI_TUNNEL
	if (g_isUcsiCycleFinished == false) {
		g_isUcsiCycleFinished = true;
		LOG_DBG("Force to clear g_isUcsiCycleFinished");
	}
#endif

}

/**
 * @brief return if qevent report is disabled
 *
 * @retval 0 if disable.
 */
uint8_t ACPI_isHostNotifyDisable(void)
{
	return (g_acpi_state_flags.acpi_mode ? 0 : 1);
}

/**
 * @brief ACPI handler init
 */
void ACPI_planeInit(void)
{
	LOG_INF("%s: Entry", __FUNCTION__);

	md_acpiplanes_register_tab(g_sAPCIHandlerTable, sizeof(g_sAPCIHandlerTable) / sizeof(MD_ACPIPLANES_HANDLER_TABLE), MD_ACPI_HYPERPLANE_FIXED_PLANE_ID);
	md_acpiplanes_register_fun(acpi_brdId_Handler, 0);

	md_acpiplanes_register_fun(dbgRw_acpiHandler, 0xDB);

	/* Register ACPI planes for eSPI validation */
#if CONFIG_ECDBGI_ESPI_SLAVE_CAPABILITIES
	md_acpiplanes_register_fun(app_espi_cfg_acpiHandler, 0xE0);
#endif

#if CONFIG_ECDBGI_ESPI_PC_BUS_MASTER_TEST
	md_acpiplanes_register_fun(app_espi_pc_acpiHandler,  0xE1);
#endif

#if CONFIG_ECDBGI_ESPI_VW_INFORMATION
	md_acpiplanes_register_fun(app_espi_vw_acpiHandler,  0xE2);
	md_acpiplanes_register_fun(app_espi_vw_aggr_acpiHandler, 0xA2);
#endif

#if CONFIG_ECDBGI_ESPI_OOB_TEST
	md_acpiplanes_register_fun(app_espi_oob_acpiHandler, 0xE3);
#endif

#if CONFIG_ECDBGI_ESPI_FA_TEST
	md_acpiplanes_register_fun(app_espi_fa_acpiHandler, 0xE5);
#endif

#if CONFIG_SELECTABLE_DAC_VALUE
    md_acpiplanes_register_fun(board_dac_Tunnel_acpiHandler, 0xF3);
#endif

    md_acpiplanes_register_fun(ACPI_VW_MMIO_Handler, 0xF4);

	/* acpi planes for battery strings */
#if CONFIG_BATTERY_MANAGEMENT
	md_acpiplanes_register_fun(board_smtbty_info_acpiHandler, 0x89);
#endif

	/* acpi plane for ambient light sensor (ALS) */
#if CONFIG_ALS
	md_acpiplanes_register_fun(app_opt4048_acpi_handler, 0xB2);
#endif

	/* Setup event for Smart Mux 1.0 */
	k_work_init(&smartMux_work, smartMux_1P0_eventProc);

	/* Setup work queue for VW MMIO tunnel */
	k_work_init(&vwMmio_work, vwMmio_eventProc);

#if CONFIG_PWM
#ifdef PWM
		/* Init the PWM */
		devPWM = DEVICE_DT_GET(PWM);
		if (!device_is_ready(devPWM)) {
			LOG_ERR(" PWM Device not ready");
		};
#endif
#endif


#if CONFIG_HOT_BAG_EN
	k_timer_init(&g_pmfHeartBeatTimer, ACPI_pmfHeartBeatHandler, NULL);
#endif

}

