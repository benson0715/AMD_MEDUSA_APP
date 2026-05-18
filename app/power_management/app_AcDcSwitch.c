/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "app_AcDcSwitch.h"
#include "app_acpi.h"
#include "board_smtbty.h"
#include "f_nv_options.h"
#include "board_config.h"

/* ************************** *
 *     global valuable        *
 * ************************** */
/**
 * Variables to support the AC/DC switch module connection to the ACPI module.
 */
/* AC Time for AC/DC switch */
volatile uint8_t g_ui8AC_Time_cnt = 0;
/* DC Time for AC/DC switch */
volatile uint8_t g_ui8DC_Time_cnt = 0;
/* Virtual AC/DC present status */
volatile uint8_t g_ui8Virtual_ACDC_STATUS;
volatile uint8_t g_bLastBatteryCharge;               /* 0/false - not charging */
                                                     /* 1/true - charging */
volatile uint8_t g_LoadfakeBatteryData = 0;          /* 1 = load fake battery data */

/* ************************** *
 *     static valuable        *
 * ************************** */
static volatile uint8_t g_bACDC_ACMode;
static volatile uint8_t g_bACDC_ACEvent = 0;
static volatile uint8_t g_bACDC_DCMode = 1;
static volatile uint8_t g_bACDC_DCEvent = 0;


/**
 * app_acDcSwitchHandler
 *
 * @note
 *  Handler to process the AC DC switch called every 1s. Need timer to be accurate.
 */
bool app_acDcSwitchHandler(void)
{
	uint32_t ui32BatteryStatus;
	uint16_t ui16BatteryPercentage;
	uint8_t AC_Time, DC_Time;
	uint8_t AcDcSwitchEnableFlag;
	bool slpAllowed = true;

	GET_NV_OPTIONS(chg_AcTime, AC_Time);
	GET_NV_OPTIONS(chg_DcTime, DC_Time);
	GET_NV_OPTIONS(chg_AcDcSwitch, AcDcSwitchEnableFlag);

	/**
	 * Check if nothing to do.
	 */
	if ((!AcDcSwitchEnableFlag) || (!DC_Time)) {
		return slpAllowed;
	}

	/**
	 * In order for accurate timer (if it has to switch).
	 */
	if (DC_Time && AC_Time && AcDcSwitchEnableFlag) {
		slpAllowed = false;
	}

	ui32BatteryStatus = app_battery_flagsCacheGet(app_battery_selectedGet()).u32flag; // Bit0 is 1 stands for BATTERY is connected.
	if (!dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_RelativeStateOfCharge, &ui16BatteryPercentage))
		ui16BatteryPercentage = 0; // BatteryCacheGet(0, 0x0d);

	/**
	 * if AC DC Switch is enabled by CMOS, then report battery always exist to OS
	 */
	g_ui8Virtual_ACDC_STATUS |= ACPI_BATTERY_CONNECTED;

	if ((g_bACDC_DCMode) && (!g_bACDC_ACMode)) {
		/**
		 * DC mode --> AC Mode
		 */
		if(g_ui8DC_Time_cnt) {
			g_ui8DC_Time_cnt --;
			if(g_ui8DC_Time_cnt) {
				return slpAllowed;
			}
			g_bACDC_DCMode = 0;
			g_bACDC_ACMode = 1;
			g_bACDC_ACEvent = 1;                    /* switch to AC mode */
			g_ui8AC_Time_cnt = AC_Time * 2;
		}
		g_ui8DC_Time_cnt = DC_Time * 2;
	} else if((g_bACDC_ACMode) && (!g_bACDC_DCMode)) {
		/**
		 * AC mode --> DC mode
		 */
		if(g_ui8AC_Time_cnt) {
			g_ui8AC_Time_cnt--;
			if(g_ui8AC_Time_cnt) {
				return slpAllowed;
			}
			g_bACDC_DCMode = 1;
			g_bACDC_ACMode = 0;
			g_bACDC_DCEvent = 1;                    // switch to DC mode
			g_ui8DC_Time_cnt = DC_Time * 2;
		}
		g_ui8AC_Time_cnt = AC_Time * 2;
	}

	/**
	 * Both AC supply and battery are plugged in. AC time and DC time are not zero.
	 * Battery capacity > 10%. The switch will be between AC power and actual Battery.
	 * If battery capacity < 10%, EC will stop the switch, stay in AC mode,
	 * wait battery capacity over 20%, and start the switch again.
	 */
	if(DC_Time && AC_Time && (ui32BatteryStatus & APP_SMTBTY_CONNECTED)) {
		g_LoadfakeBatteryData = 0;       /* use actual battery data */
		if(ui16BatteryPercentage <= 10) {
			g_ui8Virtual_ACDC_STATUS |= ACPI_AC_CONNECTED;
			return slpAllowed;
		} else if(ui16BatteryPercentage > 20) {
			if (g_bACDC_ACMode && g_bACDC_ACEvent) {
				g_ui8Virtual_ACDC_STATUS |= ACPI_AC_CONNECTED;
				gpio_write_pin(CHG_ACOK, 1);       /* Set AC_DC_SW to AC power */
				ACPI_NotifyHost(ACPI_SCI_AC);
				g_bACDC_ACEvent = 0;
			} else if (g_bACDC_DCMode && g_bACDC_DCEvent) {
				g_ui8Virtual_ACDC_STATUS |= ACPI_BATTERY_CONNECTED;
				g_ui8Virtual_ACDC_STATUS &= ~ACPI_AC_CONNECTED;
				gpio_write_pin(CHG_ACOK, 0);       /* Set AC_DC_SW to DC Power */
				ACPI_NotifyHost(ACPI_SCI_BATTERY);
				g_bACDC_DCEvent = 0;
			}
		}
	}

	/**
	 * Only AC power supply exists. AC time and DC time are not zero. The
	 * switch will be between AC power and faked battery, and all use AC power.
	 */
	else if(DC_Time && AC_Time && !(ui32BatteryStatus & APP_SMTBTY_CONNECTED)) {
		g_LoadfakeBatteryData = 1;             /* Load fake battery data */
		if(g_bACDC_ACMode && g_bACDC_ACEvent) {
			g_ui8Virtual_ACDC_STATUS |= ACPI_AC_CONNECTED;
			gpio_write_pin(CHG_ACOK, 1);            /* Set AC_DC_SW to AC power */
			ACPI_NotifyHost(ACPI_SCI_AC);
			g_bACDC_ACEvent = 0;
		} else if(g_bACDC_DCMode && g_bACDC_DCEvent) {
			g_ui8Virtual_ACDC_STATUS |= ACPI_BATTERY_CONNECTED;
			g_ui8Virtual_ACDC_STATUS &= ~ACPI_AC_CONNECTED;
			gpio_write_pin(CHG_ACOK, 0);            /* Set AC_DC_SW to DC Power */
			ACPI_NotifyHost(ACPI_SCI_BATTERY);
			g_bACDC_DCEvent = 0;
		}
	}

	/**
	 * If AC time is set to zero, DC time is not zero; or DC set to MAX =255,
	 * EC always report DC mode with AC power. If AC and Battery all exist,
	 * EC report actual battery information;
	 * if only AC exists, EC report a faked battery, with 100% capacity.
	 */
	else if (!AC_Time && DC_Time) {
		g_ui8Virtual_ACDC_STATUS |= ACPI_BATTERY_CONNECTED;
		g_ui8Virtual_ACDC_STATUS &= ~ACPI_AC_CONNECTED;

		if (!(ui32BatteryStatus & APP_SMTBTY_CONNECTED)) {
			g_LoadfakeBatteryData = 1;
		} else {
			g_LoadfakeBatteryData = 0;
		}
		gpio_write_pin(CHG_ACOK, 0);	        /* Set AC_DC_SW to DC Power although there's no DC exists */
												/* On Gardenia/Jadeite, it was set to AC as we cannot simulate DC mode from HW layer */
		g_bACDC_DCEvent = 0;
		g_bACDC_ACEvent = 0;
	}
	return slpAllowed;
}