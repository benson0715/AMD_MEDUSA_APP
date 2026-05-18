/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <stdint.h>
#include <string.h>
#include "board_smtbty.h"
#include "board_init.h"
#include "amd_crb_drivers_mp2764.h"
#include <zephyr/logging/log.h>
#include "dev_sbtsi.h"
#include "app_pseq.h"
#include "board_id.h"
#include "f_nv_options.h"
#include "board_pwrSrc.h"
#include "board_config.h"
#include "app_AcDcSwitch.h"
#include "app_acpi.h"
#include "app_thermal.h"
#include "periphmgmt.h"
#include "board_sys_config.h"
#include "dbgLogFifo.h"

#if (CONFIG_USBC_RTK)
#include "app_realtek_pd.h"
#endif
LOG_MODULE_REGISTER(app_smtbty, CONFIG_BATTERY_MGMT_LOG_LEVEL);

#define BOARD_SMTBTY_LOCATION_STRING          "Near to AC jack"
#define BOARD_BATTERY_LOW_THRESHOLD         (0x80)

#define CHG_LED_ORG_CONNECT_MSK  (0x1u)

#define CHG_LED_ORG_CONNECTED    (0x1u)
#define CHG_LED_ORG_CHARGING_PRE (0x2u)
#define CHG_LED_ORG_CHARGING     (0x4u)
#define CHG_LED_WHITE_FULL       (0x8u)
#define CHG_LED_ORG_LOW          (0x10u)

#if (APP_SMTBTY_DEBUG_ENABLE)
extern APP_SMTBTY_DBG_VECTOR g_smtbty_dbg_vector;
extern APP_SMTBTY_DBG_FLAG   g_smtbty_dbg_flag;
#endif

extern bool g_pwrSrc_noPdBattery;

/* ************************** *
 *     static valuable        *
 * ************************** */

/*
 * the bit map to hold which battery(ies) caused the latest event changed notification
 */
static volatile uint32_t _s_u32BtyAlartStatus0 = 0;

/* ratio is not used for this charger. So, set the all ratio to 00 */
static DEV_CHARGER_REG_ITEM _s_chg0_isMP2764_regs[] = {  // Charger

	/* Register,                                  value,      Reload,   Counter,   AccessCtrl          */
	/*                                            {width, data}                                        */
	/*                                                               (accessType:2 ratio:2 dataAvail:1 NA:1 autoRefersh:1 NA:1)*/
	{DEV_MP2764_REG_InputMinVoltSetting        ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_InputCurLimitSetting       ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_MinSysVoltSetting          ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ProchotConfig0             ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ProchotConfig1             ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ProchotConfig2             ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },

	{DEV_MP2764_REG_ConfigReg0                 ,  { 2, {0} },  1000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ConfigReg1                 ,  { 2, {0} },  1000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ConfigReg2                 ,  { 2, {0} },  1000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ConfigReg3                 ,  { 2, {0} },  1000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ConfigReg4                 ,  { 2, {0} },  1000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ConfigReg5                 ,  { 2, {0} },  1000  ,  0,  {c8bit(1100, 0000)} },

	{DEV_MP2764_REG_AdcResultInputVolt         ,  { 2, {0} },  0300  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_AdcResultInputCur          ,  { 2, {0} },  0310  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_AdcResultBatVoltPerCell    ,  { 2, {0} },  0320  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_AdcResultSysVolt           ,  { 2, {0} },  0330  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_AdcResultChargeCur         ,  { 2, {0} },  0330  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_AdcResultJunctionTemp      ,  { 2, {0} },  0330  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_AdcResultBatDischargeCur   ,  { 2, {0} },  0330  ,  0,  {c8bit(1100, 0000)} },

	{DEV_MP2764_REG_ChargeCurSetting0          ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_ChargeCurSetting1          ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_TwoLevInputCurLimitSetting ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_BatRegulationVoltSetting   ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_IntMaskCtrl                ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_StatusFaultReg0            ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },
	{DEV_MP2764_REG_StatusFaultReg1            ,  { 2, {0} },  2000  ,  0,  {c8bit(1100, 0000)} },

	/*------------------------------------------*/
	{DEV_CHARGER_REG_INVALID                   ,  { 2, {0} },  1000  ,  0,  {c8bit(1100, 0000)} },  // Ending

};

#pragma pack(push, 1)
static struct {
	uint8_t  reg;
	uint16_t val0;
	uint16_t val1;
} _chgDbg_regDump[] = {
	{DEV_MP2764_REG_ChargeCurSetting0,     	    0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_InputMinVoltSetting,        0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_MinSysVoltSetting,          0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_InputCurLimitSetting,       0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_ConfigReg0,                 0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_ConfigReg1,             	0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_ConfigReg2,             	0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_ConfigReg3,             	0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_ConfigReg4,             	0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_ConfigReg5,         		0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_ChargeCurSetting1,         	0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_TwoLevInputCurLimitSetting, 0xCCCC, 0XCCCC},
	{DEV_MP2764_REG_ProchotConfig0,             0xCCCC, 0xCCCC},
	{DEV_MP2764_REG_ProchotConfig1,             0xCCCC, 0xCCCC},
	{DEV_MP2764_REG_ProchotConfig2,             0xCCCC, 0xCCCC},
	{DEV_MP2764_REG_StatusFaultReg0,            0xCCCC, 0xCCCC},
	{DEV_MP2764_REG_StatusFaultReg1, 		    0xCCCC, 0XCCCC},
	{0xFF, 0xFFFF,0XFFFF},
};
#pragma pack(pop)

static struct k_work _s_chgDbg_work;

/** ************************* *
 *     global valuable        *
 * ************************** */
uint8_t g_batPresentFlag = 0;
uint8_t g_batLedStatus = 0;

uint8_t g_batLedBlinkBreathing = 0;
uint16_t g_batLedBlinkBreathing_cnt = 0;

extern uint8_t g_ui8Virtual_ACDC_STATUS;
static struct acpi_regDump g_acpi_regDump = {0};
static bool g_chgDcProchotAssign = false;

/**
 * @brief charger debug to refresh the setting
 */
static void _chgDbg_refreshSetting (struct k_work *w) 
{
	uint32_t idx = 0;
	while (_chgDbg_regDump[idx].reg != 0xFF) {
		amd_crb_drivers_mp2764_regRefresh((uint8_t *)&_chgDbg_regDump[idx].reg, (uint8_t *)&_chgDbg_regDump[idx].val0);
		idx ++;
	}
}

/**
 * @brief calculate the dc prochot threshold
 */
static EM_DEV_MP2764_PROCHOT_DC_THRESHOLD _chg_dcProchotCalculator(uint16_t DcProchot)
{
	EM_DEV_MP2764_PROCHOT_DC_THRESHOLD ret;
	if (DcProchot < 6000) {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_6A;
	} else if (6000 <= DcProchot && DcProchot < 8000) {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_6A;
	} else if (8000 <= DcProchot && DcProchot < 10000) {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_8A;
	} else if (10000 <= DcProchot && DcProchot < 12000) {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_10A;
	} else if (12000 <= DcProchot && DcProchot < 14000) {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_12A;
	} else if (14000 <= DcProchot && DcProchot < 15000) {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_14A;
	} else if (15000 <= DcProchot && DcProchot < 18000) {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_15A;
	} else if (18000 <= DcProchot && DcProchot < 20000) {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_18A;
	} else {
		ret = DEV_MP2764_PROCHOT_DC_THRESHOLD_20A;
	}
	return ret;
}

/**
 * @brief dump the charger setting
 */
void _chgDbg_dumpChgSetting ()
{
	_chgDbg_refreshSetting(0);
	uint32_t idx = 0;

	LOG_DBG("Charger setting -- AC   -- PD");
	dbgLogFifo_printf("Charger setting -- AC   -- PD\n");
	while (_chgDbg_regDump[idx].reg != 0xFF) {
		LOG_DBG("   Reg 0x%02X    %04X -- %04X %s", _chgDbg_regDump[idx].reg,
			_chgDbg_regDump[idx].val0, _chgDbg_regDump[idx].val1,
			(_chgDbg_regDump[idx].val0 != _chgDbg_regDump[idx].val1) ? "*" : ""	);
		dbgLogFifo_printf("   Reg 0x%02X    %04X -- %04X %s\n", _chgDbg_regDump[idx].reg,
			_chgDbg_regDump[idx].val0, _chgDbg_regDump[idx].val1,
			(_chgDbg_regDump[idx].val0 != _chgDbg_regDump[idx].val1) ? "*" : ""	);
		idx ++;
	}
	LOG_DBG("-----------------------------");
}

void _chgDbg_dumpChgSetting_LogOutput ()
{
	_chgDbg_refreshSetting(0);
	uint32_t idx = 0;

	dbgLog_printf("Charger setting -- AC   -- PD\n");
	while (_chgDbg_regDump[idx].reg != 0xFF) {
		dbgLog_printf("   Reg 0x%02X    %04X -- %04X %s\n", _chgDbg_regDump[idx].reg,
			_chgDbg_regDump[idx].val0, _chgDbg_regDump[idx].val1,
			(_chgDbg_regDump[idx].val0 != _chgDbg_regDump[idx].val1) ? "*" : ""	);
		idx ++;
	}
}

/**
 * @brief smart battery debug ACPI handler
 *
 * @param isRead       write permission.
 * @param ui8Idx       acpi offset
 * @param pui8Data     pointer to buffer holding the data.
 */
uint8_t board_smtbty_dbgHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	/* write none-zero to ECRAMx30 */
	if (0x30 == ui8Idx) {
		if (isRead) {
			*pui8Data = 0;
		} else if (!isRead && *pui8Data) {
			k_work_submit(&_s_chgDbg_work);
		}

	/* read registers dump */
	} else if (isRead && ui8Idx < sizeof (_chgDbg_regDump)) {
		*pui8Data = *(((uint8_t *)&_chgDbg_regDump[0]) + ui8Idx);
	}

	return 1;
}

/**
 * @brief if battery is presented or not
 *
 * @param     u8BtyId:     battery index
 * @retval True if battery presented.
 */
bool board_battery_isPresent(uint8_t u8BtyId)
{
	return g_batPresentFlag;
}

K_TIMER_DEFINE(_btyI2CRefreshTimeStamp,NULL,NULL);
#define __F_BTY_DETACH_DEBOUNCE__  5
#define __F_BTY_PIN_PRESENT_DEBOUNCE__  2
/**
 * @brief check battery attach event by I2C polling address
 *
 * @param     u8BtyId:     battery index
 */
 bool board_battery_pullBtyI2cInterface (uint8_t u8BtyId)
 {
	 static uint8_t _btyDetachDebounce = 0;
	 static uint8_t _btyPinPresentDebounce = 0;
 
	 /* set it to non-zero to debounce I2C fail caused fake battery removeing */
	 if ((k_timer_status_get(&_btyI2CRefreshTimeStamp) == 0)&&(k_timer_remaining_get(&_btyI2CRefreshTimeStamp)!=0)) {
		 /*
		  * The last check (no more than F_BATTERY_PRESENT_RECHECK_DELAY us)
		  * shows the battery is prelent.
		  * Return true reduce SMBus pressure.
		  */
		 return true;
	 } else {
		 uint32_t tmp;
 
		 if (dev_smtbty_i32Read(0, DEV_SMTBTY_REG_ChargingCurrent, &tmp) && dev_smtbty_i32Read(0, DEV_SMTBTY_REG_ChargingVoltage, &tmp)) {
			 k_timer_start(&_btyI2CRefreshTimeStamp,K_MSEC(APP_BATTERY_PRESENT_RECHECK_DELAY),K_NO_WAIT);
			 _btyDetachDebounce = __F_BTY_DETACH_DEBOUNCE__;
			 return true;
		 }
	 }
 
	 if (_btyDetachDebounce) {
		 _btyDetachDebounce --;
		 return true;
	 }
 
	 /* Make sure the BAT_PRSNT_N can indicate battery present status correctly */
	 if (gpio_read_pin(BAT_PRSNT_N)) {
		 _btyPinPresentDebounce = 0;
	 }
 
	 if (_btyPinPresentDebounce >= __F_BTY_PIN_PRESENT_DEBOUNCE__) {
		 LOG_WRN("Battery present status is detected through the BAT_PRSNT_N pin instead of the I2C bus polling");
		 return true;
	 } else {
		 _btyPinPresentDebounce ++;
	 }
 
	 return false;
 }

/**
 * @brief charger status indicated by LED
 */
void board_smtbty_breath_led(void)
{
	static uint32_t s_blink_cnt = 0; // 1: 500ms
	uint8_t org, white = 0;
	
	if (g_batLedStatus & CHG_LED_ORG_CONNECTED) {
		if (g_batLedStatus & CHG_LED_ORG_CHARGING) {
			s_blink_cnt++;
			if (s_blink_cnt > 2) {
				org = 1;
				s_blink_cnt = 0;
			} else {
				org = 0;
			}
		} else if (g_batLedStatus & CHG_LED_ORG_CHARGING_PRE) {
			s_blink_cnt++;
			if (s_blink_cnt > 5) {
				org = 1;
				s_blink_cnt = 0;
			} else {
				org = 0;
			}
		} else if (g_batLedStatus & CHG_LED_ORG_LOW) {
			s_blink_cnt++;
			if (s_blink_cnt > 10) {
				org = 1;
				s_blink_cnt = 0;
			} else {
				org = 0;
			}
		} else {
			org = 0;
			s_blink_cnt = 0;
		}
	} else {
		s_blink_cnt = 0;
		org = 0;
	}
	
	white = g_batLedStatus & CHG_LED_WHITE_FULL;
	
	gpio_write_pin(EC_CHG_WHITE_LED, white);
	
	if (white) {
		gpio_write_pin(EC_CHG_ORG_LED, 0);
	} else {
		gpio_write_pin(EC_CHG_ORG_LED, org);
	}
}

/**
 * @brief calculate the battery charging current limit based on the system status and AC capability
 *
 * @param     u8BtyId:     battery index
 * @retval charging current limit
 */
uint16_t board_smtbty_chargeCurrentLimit (uint8_t u8BtyId)
{
	uint16_t chgCurLimit = 0;

	if (u8BtyId)
		return chgCurLimit;

	uint32_t inputCurLimit = board_pwrSrc_sysAcLimit();
	if (inputCurLimit < 6000) {
		/* PD source */
		if (app_pseq_systemState() != SYSTEM_S0_STATE) {
			/* in S5/S4, S3 */
			if (inputCurLimit > 512)
				chgCurLimit = inputCurLimit - 512;
			else if (inputCurLimit > 0)
				chgCurLimit = 128;
		} else {
			/* in S0 */
			if (inputCurLimit >= 4000) {
				chgCurLimit = 1920;
			} else if (inputCurLimit >= 3000) {
				chgCurLimit = 960;
			} else {
				chgCurLimit = 512;
			}
		}
	} else {
		/* AC jack */
		chgCurLimit = inputCurLimit;
	}

	/** Battery pack spec:
	 *   RSOC<50%,  charge by 1C   (4850 mA)
	 *   RSOC>=50%, charge by 0.5C (2425mAh)
	 *
	 * limit the charge current for safty as per Tanner (2020/11/03)
	 */
	if (chgCurLimit > APP_BATTERY_MAX_CHARGE_CURRENT)
		chgCurLimit = APP_BATTERY_MAX_CHARGE_CURRENT;

	return chgCurLimit;
}

/**
 * @brief return the battery full charge capability
 *
 * @retval charging current limit
 */
uint16_t board_battery_fullChargeCapacity (void)
{
	uint16_t u16val = 0xFFFF;

	if (g_LoadfakeBatteryData) {
		u16val = FAKE_BATTERY_FULL_CHARGE_CAP;		/* Load fake battery data */
	} else {
		if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_FullChargeCapacity, &u16val))
			u16val = 0xFFFF;
	}

	return u16val;
}

/**
 * @brief return the battery remaining capability
 *
 * @retval charging current limit
 */
uint16_t board_battery_remainingCapacity (void)
{
	uint16_t u16val = 0xFFFF;
	uint8_t u8FakeBatteryLevel = 95;

	if (g_LoadfakeBatteryData) {
		GET_NV_OPTIONS(chg_FakeDcLevel, u8FakeBatteryLevel);
		u16val = ((FAKE_BATTERY_FULL_CHARGE_CAP * u8FakeBatteryLevel) / 100);	/* Load fake battery data */
	} else {
		if (! dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_RemainingCapacity, &u16val))
			u16val = 0xFFFF;
	}

	return u16val;
}

/**
 * @brief return the real battery DC prochot calculation based on the remain capabilty
 *
 * @param     level:     remain capabiltiy (0~100%)
 * @retval DC prochot threshold
 */
static uint16_t _realBatAcMap (uint8_t level)  /* level is 0 ~ 100; 0xFF means to read battery info in place */
{
	uint16_t DcProchotArr[] = {7500, 7980, 8200, 8330, 8470, 8750, 9000, 9200, 9660, 10000};

	if (0xFF == level && !g_LoadfakeBatteryData) {
		uint16_t r = board_battery_remainingCapacity();
		uint16_t f = board_battery_fullChargeCapacity();

		uint32_t tmp = r * 100 / f;

		if (tmp > 100) tmp = 100;

		level = tmp;
	}

	level /= 10;
	if (level > 9) level = 9;

	return DcProchotArr[level];
}

/**
 * @brief calculate the prochot level
 *
 * @retval DC prochot threshold
 */
uint16_t board_smtbty_calculateProchotLevel(void)
{
	if (!g_isThrottleApuInDcOnly)
		return 0xFFFF;

	uint16_t DcProchotL = 0, FakeDcLevel = 95;
	GET_NV_OPTIONS(chg_DcProchotLevel, DcProchotL);
	GET_NV_OPTIONS(chg_FakeDcLevel,    FakeDcLevel);

	/** DC Prochot override logic:
     *
     * 1) return MAX. if chg_DcOnlyProchot is 0.
	 *
	 * (chg_DcOnlyProchot is 1)
	 * 2) if chg_DcProchotLevel is 0
	 *      a) Tie DC prochot level to battery level (either real or fake battery level)
	 *      b) if chg_DcProchotLevel is 0x3F
	 *           Set DC prochot to 12A for fake DC, or
	 *           Set DC prochot to 5A for real DC.
	 *      c) if chg_DcProchotLevel is in [1, 0x3E]
	 *           Set DC prochot to (chg_DcProchotLevel << 8)
	 */

	if (0 == DcProchotL) {
		if (board_battery_isPresent(0)) {    /* tie to real battery level if battery is present */
			DcProchotL = _realBatAcMap(0xFF);
		} else {
			/* fix at 12A in if no battery */
			DcProchotL = 12000;
		}
	} else if (0x3F == DcProchotL) {     /* (by default 0x3F, the max DCProcht Value) */
		/* if AcDc Switch is turned on */
		if (g_LoadfakeBatteryData) {
			/* fix DCPrcohot at 12A */
			DcProchotL = 12000;
		} else {                         /* if AcDc Switch is turned off */
			/* fix at 5A */
			DcProchotL = 5000;
		}
	} else {
		DcProchotL <<= 8;
	}

	/* "((DcProchotL >> 1) & 0x3F00) << 1" because the RS2 1:2 */
	/* 0x3F00 as DcProchotL effective bits [13:8] */
	DcProchotL = ((DcProchotL >> 1) & 0x3F00) << 1;  /* DcProchotL effective bits [13:8] */
	return DcProchotL;
}

/**
 * @brief calculate the p3t limit based on system power budget
 *
 * @retval p3t limit
 */
uint32_t board_smtbty_calculateP3tLimit(void)
{
	uint8_t systemConfigP3tLimit = g_p3tConfigLimit; /* make a temp copy */
	uint32_t ret = 0xFFFFFFFF;   /* 0xFFFFFFFF means invalid and */
	                             /* app_thermal_updateP3tSetting() will not apply this setting */

	if (0xFF == systemConfigP3tLimit) {
		return ret;
	}

	uint8_t p3tLimitEn_DC, p3tLimitEn_AC;
	GET_NV_OPTIONS(thm_p3tLimitEn_DC, p3tLimitEn_DC);
	GET_NV_OPTIONS(thm_p3tLimitEn_AC, p3tLimitEn_AC);

	if (!p3tLimitEn_DC && !p3tLimitEn_AC) {          /* if P3T are disabled */
		return ret;                                  /* no more calculation */
	}

	uint32_t acBudget = 0;
	uint32_t dcBudget = 0;
	uint32_t dynamicPowerBudget = 0;
	uint32_t acVoltage = 0;
	uint8_t FakeDcLevel = 95;
	GET_NV_OPTIONS(chg_FakeDcLevel,    FakeDcLevel);

	if (board_pwrSrc_hasAcPowerSource()) {
		/* board_pwrSrc_sysPwrMaxWatt return mA * mV and need to /1000 */
		acBudget = ((board_pwrSrc_sysPwrMaxWatt() / 1000) * 120 / 100);  /* Peak cap = CurLimit (mA) * Voltage (V) * 120% */
	} else {
		acBudget  = 0;
		acVoltage = 0;
	}

	if (!p3tLimitEn_AC && p3tLimitEn_DC && acBudget) { /* if P3T is enabled only for DC case but AC is present */
		/* return 0xFFFFFFFF */
		return ret;
	}

	/**
	 * Three cases:
	 *   p3tLimitEn_AC = 1 && p3tLimitEn_DC = 0
	 *   p3tLimitEn_AC = 0 && p3tLimitEn_DC = 1
	 *   p3tLimitEn_AC = 1 && p3tLimitEn_DC = 1
	 */

	if (board_battery_isPresent(0)) {           /* tie to real battery level if battery is present */
		/* Battery voltage use 8.8V */
		dcBudget = _realBatAcMap (0xFF) * 8800 / 1000;
	}  else {
		dcBudget = 0;
	}

	if (!p3tLimitEn_DC && p3tLimitEn_AC && dcBudget) { /* if P3T is enabled only for AC case but DC is present */
		/* return 0xFFFFFFFF */
		return ret;
	}

	/* no eDP, report the all power budget */
	dynamicPowerBudget = acBudget + dcBudget;

	/* times 1.125 */
	dynamicPowerBudget *= 1125;
	dynamicPowerBudget /= 1000;

	if (dynamicPowerBudget > 1000ul * systemConfigP3tLimit) {
		ret = 1000 * systemConfigP3tLimit;
	} else {
		ret = dynamicPowerBudget;
	}

	LOG_DBG("P3T Limit %dmW = MIN (1.125 x (Ac %dmW + Dc %dmW - 7W - USB 0W), IRM_SysCfg %dW) ", ret, acBudget, dcBudget, systemConfigP3tLimit);

	return ret;
}

/**
 * @brief board battery status change notify
 *
 * @param        u8BtyId:     battery index
 * @param u32ChangedBits:     battery status change bit map
 */
void board_battery_notify(uint8_t u8BtyId, uint32_t u32ChangedBits)
{
	if (u8BtyId) return;

	if (u32ChangedBits) _s_u32BtyAlartStatus0 = 1;

	/* Wakeup system in MS mode if APP_SMTBTY_TRIPPED or APP_SMTBTY_CRITICAL */
	uint32_t u32St = app_battery_flagsCacheGet(u8BtyId).u32flag;
	if ( u32St & APP_SMTBTY_TRIPPED ) {
		ACPI_NotifyHost(ACPI_SCI_BATTERY_TRIP_POINT);
		/* report only one ACPI_SCI_BATTERY in a time */
		return;
	}
	if ( (u32St & (APP_SMTBTY_WARNING | APP_SMTBTY_CRITICAL)) && !board_pwrSrc_hasAcPowerSource() ) {
	 	ACPI_NotifyHost(ACPI_SCI_BATTERY_NOTIFY);
	 	/* report only one ACPI_SCI_BATTERY in a time */
	 	return;
	}

	/**
	 * If there is a non-zero status, notify the host that a battery status
	 * change has occurred.
	 */
	if (u32ChangedBits) {
		 if (!gpio_read_pin(EC_SLP_S3_S0A3_N)) {        /* no other battery Qevent in S0i3 */
		 	LOG_DBG("Notify host ACPI_SCI_BATTERY, ALART (0x%02X", u32ChangedBits);
		 	ACPI_NotifyHost(ACPI_SCI_BATTERY_NOTIFY);
		 }
	}
}

/**
 * @brief smart battery handler in the post stage
 *
 * @param     u8BtyId:     battery index
 */
void board_battery_postHandler(uint8_t u8BtyId)
{
	DEV_MP2764_REG_1A reg_1A;

	if (u8BtyId)
		return;

	/**
	 * Tie DCProchotL setting to real battery level
	 */
	 if (board_battery_isPresent(0) && g_isThrottleApuInDcOnly && !g_chgDcProchotAssign) {
		uint16_t DcProchotL;

		DcProchotL = board_smtbty_calculateProchotLevel();

		if (!amd_crb_drivers_mp2764_i32Read(DEV_CHARGER_CURRENT_PORT, DEV_MP2764_REG_ProchotConfig1, &(reg_1A.data))) {
			return;
		}

		if (DcProchotL != reg_1A.f.ibatt_prochot) {
			reg_1A.f.ibatt_prochot = _chg_dcProchotCalculator(DcProchotL);
			amd_crb_drivers_mp2764_i32Write(DEV_CHARGER_CURRENT_PORT, DEV_MP2764_REG_ProchotConfig1, reg_1A.data);
			LOG_DBG ("Update DCProchotL to %dmA", DcProchotL);
			}
	}

	uint32_t u32P3tLimit;
	u32P3tLimit = board_smtbty_calculateP3tLimit();
	app_thermal_updateP3tSetting(u32P3tLimit);

	uint16_t u16RemainPercentage = 0;
	if (dev_smtbty_i32CacheGet(u8BtyId, DEV_SMTBTY_REG_RelativeStateOfCharge, (void *)&u16RemainPercentage)) {

	}
  #if APP_SMTBTY_CAPACITY_MODE_10MWH
	uint16_t u16Batterytmp = 0;
	if (dev_smtbty_i32CacheGet(u8BtyId, DEV_SMTBTY_REG_BatteryMode, (void *)&u16Batterytmp)) {
		    /**
			 * If bit 15 of BatteryMode is 0, it means smart battery report its capacity in mAh.
			 * EC has to write bit 15 to 1 to let battery report its capacity in 10mWh.
			 */
			if( !(u16Batterytmp & 0x8000) ) {
				u16Batterytmp = u16Batterytmp | 0x8000;			
			  if ( dev_smtbty_i32Write(u8BtyId, DEV_SMTBTY_REG_BatteryMode, (uint32_t) u16Batterytmp) ) {
					/**
					 * Update DesignCapacity, FullChargeCapacity, and RemainingCapacity immediately
					 * after EC modifies Battert Capacity Mode.
					 */
					dev_smtbty_i32Read(0, DEV_SMTBTY_REG_DesignCapacity, &u16Batterytmp);
					dev_smtbty_i32Read(0, DEV_SMTBTY_REG_FullChargeCapacity, &u16Batterytmp);
					dev_smtbty_i32Read(0, DEV_SMTBTY_REG_RemainingCapacity, &u16Batterytmp);
				}
			}
	}
	#endif

	static struct {
		uint8_t regId;
		char *  regName;
		uint16_t oldValue;
	} _s_dbg_BtyDump[] = {
		{ DEV_SMTBTY_REG_DesignCapacity,     "DesignCapacity",     0 },
		{ DEV_SMTBTY_REG_FullChargeCapacity, "FullChargeCapacity", 0 },
		{ DEV_SMTBTY_REG_RemainingCapacity,  "RemainingCapacity",  0 },
		{ DEV_SMTBTY_REG_DesignVoltage,      "DesignVoltage",      0 },
//		{ DEV_SMTBTY_REG_Current,            "Current",            0 },
//		{ DEV_SMTBTY_REG_Voltage,            "Voltage",            0 },
		{ DEV_SMTBTY_REG_BatteryStatus,      "BatteryStatus",      0 },
		{ 0, "", 0 }
	};
	for (uint8_t i = 0; _s_dbg_BtyDump[i].regId != 0; i++) {
		uint16_t u16Val;
		if (dev_smtbty_i32CacheGet(u8BtyId, _s_dbg_BtyDump[i].regId, &u16Val)) {
			if (_s_dbg_BtyDump[i].oldValue != u16Val) {
				switch (_s_dbg_BtyDump[i].regId) {
					case DEV_SMTBTY_REG_Current:
						if ((_s_dbg_BtyDump[i].oldValue > u16Val && _s_dbg_BtyDump[i].oldValue - u16Val > 30) || 
							(_s_dbg_BtyDump[i].oldValue < u16Val && u16Val - _s_dbg_BtyDump[i].oldValue > 30) ) {
							LOG_INF("BTY: %02X %-18s (0x%04X | %5d) <= (0x%04X | %5d)", _s_dbg_BtyDump[i].regId, _s_dbg_BtyDump[i].regName, u16Val, (int16_t)u16Val, _s_dbg_BtyDump[i].oldValue, (int16_t)_s_dbg_BtyDump[i].oldValue);
							_s_dbg_BtyDump[i].oldValue = u16Val;
						}
						break;
					case DEV_SMTBTY_REG_Voltage:
						if ((_s_dbg_BtyDump[i].oldValue > u16Val && _s_dbg_BtyDump[i].oldValue - u16Val > 20) || 
							(_s_dbg_BtyDump[i].oldValue < u16Val && u16Val - _s_dbg_BtyDump[i].oldValue > 20) ) {
							LOG_INF("BTY: %02X %-18s (0x%04X | %5d) <= (0x%04X | %5d)", _s_dbg_BtyDump[i].regId, _s_dbg_BtyDump[i].regName, u16Val, u16Val, _s_dbg_BtyDump[i].oldValue, _s_dbg_BtyDump[i].oldValue);
							_s_dbg_BtyDump[i].oldValue = u16Val;
						}
						break;
					default:
						LOG_INF("BTY: %02X %-18s (0x%04X | %5d) <= (0x%04X | %5d)", _s_dbg_BtyDump[i].regId, _s_dbg_BtyDump[i].regName, u16Val, u16Val, _s_dbg_BtyDump[i].oldValue, _s_dbg_BtyDump[i].oldValue);
						_s_dbg_BtyDump[i].oldValue = u16Val;
						break;
				}
			}
		}
	}

}

/**
 * @brief get and clear the battery alart status
 *
  * @retval alart status
 */
uint32_t board_battery_alartStatusGetAndClear(void)
{
	uint32_t u32Alart = _s_u32BtyAlartStatus0;
	LOG_INF("Report & Clear ALART (0x%02X) flags", u32Alart);
	_s_u32BtyAlartStatus0 = 0;
	return u32Alart;
}

/**
 * @brief callback when detect real battery attached
 *
 * @param     u8BtyId:     battery index
 */
void board_smtbty_onBtyAttach(uint8_t u8BtyId)
{
#if (CONFIG_USBC_4CC)
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(TYPEC_PORT_0_IDX);
#endif

	g_batLedStatus |= CHG_LED_ORG_CONNECTED;

	dev_smtbty_blockRead(u8BtyId, DEV_SMTBTY_REG_DeviceName      , NULL, 0);
	dev_smtbty_blockRead(u8BtyId, DEV_SMTBTY_REG_DeviceChemistry , NULL, 0);
	dev_smtbty_blockRead(u8BtyId, DEV_SMTBTY_REG_ManufacturerName, NULL, 0);
	dev_smtbty_blockRead(u8BtyId, DEV_SMTBTY_REG_ManufactureDate , NULL, 0);
	dev_smtbty_blockRead(u8BtyId, DEV_SMTBTY_REG_ManufacturerData, NULL, 0);
	dev_smtbty_blockRead(u8BtyId, DEV_SMTBTY_REG_SerialNumber    , NULL, 0);

	/* BAT_IN# may not indicate battery presence correctly as bad D127 (PLAT-89591) */
	g_batPresentFlag = 1;
	board_pwrSrcEvent();
	LOG_DBG("Update g_batPresentFlag to %d", g_batPresentFlag);
#if (CONFIG_USBC_4CC)
	if (CHECK_CFG(PD0, TPS66994BF) || CHECK_CFG(PD1, TPS66994BF)) {
		/* clear dead battery flag if battery plugin */
		app_4cc_deadbatteryClear(TYPEC_PORT_0_IDX);
		dpm_stat->dead_bat = false;
	}
#endif

	/* refresh the P3T after battery attached */
	app_thermal_refreshP3tSetting();
}

/**
 * @brief callback when detect real battery detached
 *
 * @param     u8BtyId:     battery index
 */
void board_smtbty_onBtyDetach(uint8_t u8BtyId)
{
	g_batLedStatus &= ~CHG_LED_ORG_CONNECTED;

	/* BAT_IN# may not indicate battery presence correctly as bad D127 (PLAT-89591) */
	g_batPresentFlag = 0;
	board_pwrSrcEvent();
	LOG_DBG("Update g_batPresentFlag to %d", g_batPresentFlag);

	/* refresh the P3T after battery detached */
	app_thermal_refreshP3tSetting();
}

/**
 * @brief callback when battery status change and control BAT LED
 *
 * @param     s:     battery status
 */
void board_smtbty_signals(APP_SMTBTY_BTY_STATUS s)
{
	uint16_t u16Data;
	static uint16_t _s_fullChargedFlag = 0;
	uint8_t led = g_batLedStatus & CHG_LED_ORG_CONNECT_MSK;

	if (s == APP_BATTERY_FULL_CHARGED) {
		/* Turn On Battery LED */
		led |= CHG_LED_WHITE_FULL;
		if (0 == _s_fullChargedFlag) {
			ACPI_NotifyHost(ACPI_SCI_BATTERY_NOTIFY); // Battery charge done
			_s_fullChargedFlag = 1;
		}
	} else {
		_s_fullChargedFlag = 0;
	}

	switch (s) {
		case APP_BATTERY_NOT_CONNECTED:
			led = 0;
			break;
		case APP_BATTERY_IDLE:
			break;
		case APP_BATTERY_CHARGING:
			led |= CHG_LED_ORG_CHARGING;
			break;
		case APP_BATTERY_DISCHARGING:
			if (board_pwrSrc_hasAcPowerSource()) {

			} else if (dev_smtbty_i32CacheGet(0, DEV_SMTBTY_REG_RemainingCapacity, &u16Data)) {
				if (u16Data < BOARD_BATTERY_LOW_THRESHOLD) {
					/* Less than 7% */
					led |= CHG_LED_ORG_LOW;
				}
			}
			break;
		case APP_BATTERY_FULL_DISCHARGED:
			led |= CHG_LED_ORG_LOW;
			break;
		case APP_BATTERY_FULL_CHARGED:
			led |= CHG_LED_WHITE_FULL;
			break;
		case APP_BATTERY_IS_DEAD:
			led |= CHG_LED_ORG_LOW;
			LOG_DBG("APP_BATTERY_IS_DEAD");
			break;
		case APP_BATTERY_REACTIVATING:
			break;
		case APP_BATTERY_PRE_CHARGE:
			led |= CHG_LED_ORG_CHARGING_PRE;
			break;
		default:
			LOG_DBG("board_smtbty_signals ==> unknow case");
			break;
	}

	g_batLedStatus = led;
}

/**
 * @brief set the charger ac limit
 *
 * @param     chgId:        index of charger
 * @param     acLimit1:     limit1 val
 * @param     acLimit2:     limit2 val
 */
bool board_smtbty_setAcLimit(uint8_t chgId, uint32_t acLimit1, uint32_t acLimit2)
{
	bool ret = true;

	if (g_pwrSrc_noPdBattery)
		return false;

	if (chgId != DEV_CHARGER_CURRENT_PORT) {
		/* invalid charge id */
		LOG_ERR("Invalid charge id, %d", chgId);
		return false;
	}

	/* acLimit1 */
	ret &= amd_crb_drivers_mp2764_AcLimit1(chgId, acLimit1);

	/* acLimit2 (0 to disable, none-zero to enable) */
	ret &= amd_crb_drivers_mp2764_AcLimit2(chgId, acLimit2);

	/* IIN_RSENSE_SEL -> 0 (10mohm)
	 * IBATT_RSENSE_SEL -> 0 (10mohm)
	 * 0: 10V/V (10mO) (Default)
	 * 1: 20V/V (5mO)
	 * */
	ret &= amd_crb_drivers_mp2764_regRMW(chgId, DEV_MP2764_REG_ConfigReg5, 0x0000, 0x000C);

	LOG_DBG("   >>> Update acLimit1 %d mA, acLimit2 %d mA and T1/T2 10ms/10ms", acLimit1, acLimit2);
	return ret;
}

/**
 * @brief enable the charger
 *
 * @return false if fail to enable charger
 */
bool board_smtbty_ChgEnable (void)
{
	return amd_crb_drivers_mp2764_chgEnable(DEV_CHARGER_CURRENT_PORT);
}

/**
 * @brief disable the charger
 *
 * @return false if fail to enable charger
 */
bool board_smtbty_ChgDisable (void)
{
	bool ret = true;
	ret &= amd_crb_drivers_mp2764_chgDisable(DEV_CHARGER_CURRENT_PORT);
	return ret;
}

// This function only set current to charger who has external power source
//
static bool _setChargeCurrent(uint16_t current)
{
	BOARD_PWR_SRC_TYPE pwrSource = board_pwrSrc_getCurrentPowerSource ();
	bool ret = false;
	DEV_MP2764_REG_14 reg_14;

	switch (pwrSource) {
		case BOARD_PWR_PD:
			/* read the data from register 0x14 */
			ret = amd_crb_drivers_mp2764_i32Read(DEV_CHARGER_CURRENT_PORT, DEV_MP2764_REG_ChargeCurSetting1, &(reg_14.data));

			/* write Icc to zero to disable the charging, and non zero to enable */
			reg_14.f.icc = (uint8_t) (current * 10 / 625);
			ret &= amd_crb_drivers_mp2764_i32Write(DEV_CHARGER_CURRENT_PORT, DEV_MP2764_REG_ChargeCurSetting1, reg_14.data);

			return ret;
		case BOARD_PWR_BTY:
		default:
			return false;
	}
}

bool board_smtbty_doCharge(uint8_t btyId)
{
	extern APP_SMTBTY_CHG_STATUS _s_smtbty_chgSt;
	static APP_SMTBTY_CHG_STATUS oldChgSt = APP_CHG_DISABLE;
	static uint8_t s_cur_status = 0;
	APP_SMTBTY_CHG_STATUS newChgSt = APP_CHG_ENABLED_NORMAL;
	bool isAcOk = true;
	bool isBtyPresent = false;

	if (btyId >= MAX_BATTERY_SUPPORTED || _s_smtbty_chgSt == APP_CHG_BATTERY_IS_DEAD)
		return false;

	isAcOk = board_pwrSrc_hasAcPowerSource();
	isBtyPresent = board_battery_pullBtyI2cInterface(0) || g_batPresentFlag;

	if (! isBtyPresent ) {

		newChgSt = APP_CHG_DISABLE;

	} else if (isAcOk) {
		APP_BATTERY_STATUS btyStatus;
		bool readStatusSuccess = dev_smtbty_i32Read(btyId, DEV_SMTBTY_REG_BatteryStatus, &btyStatus.u16Status);
		/**
		 * Determine if there is an error/status that indicates that we
		 * should terminate battery charging operations.
		 *
		 * Note:  This read will also update the cache, which may
		 * be referenced later.
		 *
		 * The battery status bits of interested are as follows,
		 * per the SBS data specification (1.1)
		 *
		 * Bit 15 - Over Charged Alarm
		 * Bit 14 - Terminate Charge Alarm
		 * ...
		 * Bit 12 - Over Temp Alarm
		 * ...
		 * Bit 5 - Fully Charged
		 *
		 * Any of these bits set are used to indicate that we should
		 * terminate the battery charging feature.
		 */
		if (readStatusSuccess && (btyStatus.u16Status & 0xD000)) {
			/* Over Charged / Terminate Charge / Over Temp */
			newChgSt = APP_CHG_DISABLE;

			LOG_INF("Charging suspend as status (0x%04X) Over Charged/Terminate Charge/Over Temp", btyStatus.u16Status);
		}

		if (readStatusSuccess && (btyStatus.u16Status & 0x0020)) {
			/* Fully Charged */
			newChgSt = APP_CHG_DISABLE;

			LOG_INF("Battery full charged as status (0x%04X)", btyStatus.u16Status);
		}

		/**
		 * MSDN: acpi-battery-and-power-subsystem-firmware-implementation
		 *       _DSM F1 charge throttle (0 to 100)
		 */
		uint8_t u8ChgThrottle = app_battery_ChargeThrottleGet(btyId);

		if (APP_SMTBTY_CHG_THROTTLE_DISABLED != u8ChgThrottle) {
			uint16_t u16rsoc = APP_SMTBTY_CHG_THROTTLE_DISABLED;

			if (!dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_RelativeStateOfCharge, &u16rsoc)) {
				uint16_t u16rest = 0;
				uint16_t u16full = 0;
				if (dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_RemainingCapacity, &u16rest) &&
					dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_FullChargeCapacity, &u16full) ) {

					u16rsoc = (u16rest * 100) / u16full;
				}
			}

			if (APP_SMTBTY_CHG_THROTTLE_DISABLED != u16rsoc && (uint8_t)u16rsoc <= 100) {
				if (u16rsoc >= u8ChgThrottle) {
					LOG_INF("Charging suspend as Charge Throttle reached (Rem. %d%% >= Thrt. %d%%)", u16rsoc, u8ChgThrottle);
					newChgSt = APP_CHG_DISABLE;
				}
			}
		}

		if (readStatusSuccess && (newChgSt != APP_CHG_DISABLE)) {
			static uint16_t __s_oldCurrent;
			static uint8_t __s_skippedLoops;

			uint16_t u16voltage, u16current;

			board_smtbty_ChgEnable();

			if (dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_ChargingVoltage, &u16voltage) && 
				dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_ChargingCurrent, &u16current)) {

#if (CONFIG_LOG||APP_BATTERY_FULL_CHARGE_VOLTAGE)
				uint16_t u16ReqCurrent = u16current;		
				uint16_t u16ReqVoltage = u16voltage;
#endif
				uint16_t u16ChargeCurrentLimit = board_smtbty_chargeCurrentLimit(0);
				if (u16current > u16ChargeCurrentLimit)
					u16current = u16ChargeCurrentLimit;

				if (__s_oldCurrent != u16current || (__s_skippedLoops ++ > 30)) {
					/* Some of chargers are sensitive on the written order, so we try the both */
					if ( _setChargeCurrent (u16current) ) {

						LOG_WRN("Charging ... %5d mV, %4d mA, Req. %5d mV, %4d mA", u16voltage, u16current, u16ReqVoltage, u16ReqCurrent);
						if (s_cur_status != 3) {
							dbgLogFifo_printf("Charging: %d, %d, %d, %d\n", u16voltage, u16current, u16ReqVoltage, u16ReqCurrent);
							s_cur_status = 3;
						}
					} else {
						LOG_ERR("Charging ... N/A mV, N/A mA, Req. %5d mV, %4d mA; !!!communication error!!!", u16ReqVoltage, u16ReqCurrent);
					}

					__s_oldCurrent = u16current;

					__s_skippedLoops = 0;
				}

				newChgSt = APP_CHG_ENABLED_NORMAL;
			}
		}

	} else {
		/* Only battery is present (no AC) */
		newChgSt = APP_CHG_DISABLE;
	}

	if (newChgSt == APP_CHG_DISABLE ) {
		static uint8_t __s_skippedChgDisLoops; 
		if (oldChgSt != newChgSt || __s_skippedChgDisLoops > 20) {
			board_smtbty_ChgDisable();
			__s_skippedChgDisLoops = 0;
		} else {
			__s_skippedChgDisLoops ++;
		}
		if (s_cur_status != 4) {
			dbgLogFifo_printf("chg disable\n");
			s_cur_status = 4;
		}
	}
	_s_smtbty_chgSt = newChgSt;

	if (oldChgSt != newChgSt) {
		oldChgSt = newChgSt;
		return true;
	}

	return false;
}

/**
 * @brief smart battery handler in the preprocess stage
 *
 * @param     u8BtyId:     battery index
 */
void board_smtbty_preProcessing(uint8_t u8BtyId)
{

}

/**
 * @brief refresh smart battery status
 *
 * @param     u8BtyId:     battery index
 */
void board_smtbty_refresh(uint8_t u8BtyId)
{

}

/**
 * @brief smart battery infomation acpi handler
 *
 * @param isRead write permission.
 * @param ui8Idx acpi offset
 * @param pui8Data pointer to buffer holding the data.
 */
uint8_t board_smtbty_info_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	static uint8_t pgSel = 0;
	uint32_t addr;
	uint8_t s_manufacturer_name[16] = {0};
	uint8_t s_device_name[18] = {0};
	uint8_t s_device_chemistry[10] = {0};
	uint8_t s_manufacturer_info[32] = {0};
	uint16_t s_manufacturer_data = 0;
	uint16_t s_serial_number = 0;

	memset(&s_manufacturer_name[0], 0, sizeof(s_manufacturer_name));
	memset(&s_device_name[0], 0, sizeof(s_device_name));
	memset(&s_device_chemistry[0], 0, sizeof(s_device_chemistry));
	memset(&s_manufacturer_info[0], 0, sizeof(s_manufacturer_info));
	dev_smtbty_blockCacheGet(0, DEV_SMTBTY_REG_ManufacturerName, s_manufacturer_name, sizeof(s_manufacturer_name));
	dev_smtbty_blockCacheGet(0, DEV_SMTBTY_REG_DeviceName, s_device_name, sizeof(s_device_name));
	dev_smtbty_i32CacheGet(0, DEV_SMTBTY_REG_ManufactureDate, &s_manufacturer_data);
	dev_smtbty_i32CacheGet(0, DEV_SMTBTY_REG_SerialNumber, &s_serial_number);
	dev_smtbty_blockCacheGet(0, DEV_SMTBTY_REG_DeviceChemistry, s_device_chemistry, sizeof(s_device_chemistry));
	dev_smtbty_blockCacheGet(0, DEV_SMTBTY_REG_ManufacturerData, s_manufacturer_info, sizeof(s_manufacturer_info));

	if (ui8Idx == 0x30) {

		if (isRead)                *pui8Data = pgSel;
		else if (*pui8Data < 2)    pgSel = *pui8Data;
		return 1;

	} else {

		if (!board_battery_isPresent(0)) {
			*pui8Data = 0;
			return 1;
		}

		addr = pgSel * 0x30 + ui8Idx;
		if (isRead) {
			/* location */
			if (addr <= 0x0F) {
				extern size_t strlen ( const char * str );
				if (addr <= strlen(BOARD_SMTBTY_LOCATION_STRING)) {
					*pui8Data = BOARD_SMTBTY_LOCATION_STRING[addr];
				} else {
					*pui8Data = 0;
				}
			}
			/* manufacturer name */
			if (addr >= 0x10 && addr < 0x1F) {
				*pui8Data = s_manufacturer_name[addr - 0x10];
			} else if (addr == 0x1F) {
				*pui8Data = 0;
			}

			/* device name */
			if (addr >= 0x20 && addr < 0x2F) {
				*pui8Data = s_device_name[addr - 0x20 + 1]; // Invalid data in the battery eeprom.
			} else if (addr == 0x2F) {
				*pui8Data = 0;
			}

			/* Manufacture Date (2-byte) */
			if (addr == 0x31) {
				*pui8Data = (s_manufacturer_data >> 8) & 0xFF;
			} else if (addr == 0x30) {
				*pui8Data = s_manufacturer_data & 0xFF;
			}

			/* Serial Number (2-byte) */
			if (addr == 0x33) {
				*pui8Data = (s_serial_number >> 8) & 0xFF;
			} else if (addr == 0x32) {
				*pui8Data = s_serial_number & 0xFF;
			}

			/* Device Chemistry */
			if (addr >= 0x38 && addr < 0x3F) {
				*pui8Data = s_device_chemistry[addr - 0x38];
			} else if (addr == 0x3F) {
				*pui8Data = 0;
			}

			/* Manufacturer Data */
			if (addr >= 0x40 && addr < 0x5F) {
				*pui8Data = s_manufacturer_info[addr - 0x40];
			} else if (addr == 0x5F) {
				*pui8Data = 0;
			}
		}

		return 1;
	}
}

uint32_t board_smtbty_getAcpiData(uint8_t idx)
{
	if (idx >= ACPI_REG_COUNT) {
		return 0;
	}

	if (g_acpi_regDump.en_bits & (1 << idx)) {

		LOG_DBG("ACPI_REG_IDX_%d: %d", idx, g_acpi_regDump.regs[idx].val);
		return g_acpi_regDump.regs[idx].val;
	}

	return 0;
}

uint8_t board_smtbty_dbg_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint8_t s_last_en_bits = 0;

	if (ui8Idx >= MD_ACPI_HYPERPLANE_SELECTOR_OFFSET)
		return 0;

	uint8_t * ptr = (uint8_t *)&g_acpi_regDump;
	if (ui8Idx >= sizeof(struct acpi_regDump)) {
		return 0;
	}

	if (isRead) {
		*pui8Data = ptr[ui8Idx];
		return 0;
	} else {
		ptr[ui8Idx] = *pui8Data;
	}

	/* Detect bits that changed from 1 to 0 (newly disabled), clear their values */
	uint8_t newly_disabled = s_last_en_bits & ~g_acpi_regDump.en_bits;
	for (uint8_t i = 0; i < ACPI_REG_IDX_COUNT && newly_disabled; i++) {
		if (newly_disabled & (1 << i)) {
			g_acpi_regDump.regs[i].val = 0;
		}
	}

	bool changed = (s_last_en_bits != g_acpi_regDump.en_bits) ? true : false;

	s_last_en_bits = g_acpi_regDump.en_bits;

#ifdef CONFIG_BATTERY_MANAGEMENT
	if (changed) {
		LOG_DBG("changed: %d, g_acpi_regDump.en_bits: %d", changed, g_acpi_regDump.en_bits);
		amd_crb_drivers_mp2764_setProchot();
	}
#endif

	return 0;
}

/**
 * @brief smart battery task init
 *
 * @retval True if successful
 */
bool board_smtbty_taskInit(void)
{
	k_msleep(10);
	/**
	 * Set repDelay to (2ms, 300ms). The 1st delay
	 * should be short enough to ensure ADC is updated
	 * in time on each EC wakeup.
	 */

	app_smtbty_init();
	/**
	 * !! NOTE !!
	 * In order to use the platform specific charger registers table,
	 * replacing dev_charger_init() to below three lines:
	 */
	amd_crb_drivers_mp2764_regPlatTable(DEV_CHARGER_CURRENT_PORT, _s_chg0_isMP2764_regs ,_s_platform_chgBufTable);

	/* Set the prochot registers */
	amd_crb_drivers_mp2764_prochot_register(board_smtbty_setProchotGate);

	/*
	 * register charger function calls
	 */
	APP_SMTBTY_CLIENT_ENTITY * pCfg = app_smtbty_getClientEntity();
	pCfg->pfnPresent = board_battery_pullBtyI2cInterface;
	pCfg->pfnIsAcPresent = board_pwrSrc_hasAcPowerSource;
	pCfg->pfnBatteryLed = board_smtbty_signals;
	pCfg->pfnNotify = board_battery_notify;
	pCfg->pfnOnBatteryAttach = board_smtbty_onBtyAttach;
	pCfg->pfnOnBatteryDetach = board_smtbty_onBtyDetach;
	pCfg->pfnRefresh = board_smtbty_refresh;
	pCfg->pfnPreProcessing = board_smtbty_preProcessing;
	pCfg->pfnPostProcessing = board_battery_postHandler;
	pCfg->pfnGetChargeCurrentLimit = board_smtbty_chargeCurrentLimit;
	pCfg->pfnChgEnble = board_smtbty_ChgEnable;
	pCfg->pfnChgDisable = board_smtbty_ChgDisable;
	pCfg->pfnCharger = board_smtbty_doCharge;
	app_smtbty_start();

	/* reserve 10% of battery capicity by default */
	dev_smtbty_setRsvdBtyCapacity(342);

	/**
	 * For debug purpose to dump critical charger settings
	 */
	// _s_chgRefreshEventId = md_event_aloc(1, false, true, _chgDbg_refreshSetting);
	k_work_init(&_s_chgDbg_work, _chgDbg_refreshSetting);
	md_acpiplanes_register_fun(board_smtbty_dbgHandler, 0x0B);
	md_acpiplanes_register_fun(board_smtbty_dbg_acpiHandler, 0xDE);
#if (APP_SMTBTY_DEBUG_ENABLE)
	md_acpiplanes_register_fun(app_smtbty_dbgHandler, 0xBC);
#endif

	return true;
}

#define CHG_CONST_5000         (5000)
#define CHG_CONST_6000         (6000)
#define PERCENTAGE(v, x)       ((v) * (x) / 100)   // v * x%

static bool _pwrSrc_releaseProchot (uint8_t chgId)
{
	if (chgId != DEV_CHARGER_CURRENT_PORT) {
		/* invalid charge id */
		LOG_ERR("Invalid charge id, %d", chgId);
		return false;
	}

	/* Disable the prochot and re-enable it */
	bool ret = amd_crb_drivers_mp2764_regRMW(chgId, DEV_MP2764_REG_ProchotConfig2, 0x0000, 0x0018);
	LOG_DBG ("  Release-PROCHOT#  [%s]",  ret ? "OK" : "NG");
	ret &= amd_crb_drivers_mp2764_regRMW(chgId, DEV_MP2764_REG_ProchotConfig2, 0x0018, 0x0018);

	return ret;
}

/**
 * @brief check if battery is present, by I2C polling address to make sure the battery is actively connected and can power on the system
 *
 * @retval True if battery is present.	
 */
 bool board_smtbty_hasBattery(void)
 {
	 if (!gpio_read_pin(BAT_PRSNT_N)) { /* in case BAT_PRSNT_N shows the battery is here */
		 uint16_t tmp; /* read by I2C for double confirm */
		 if (dev_smtbty_i32Read(0, DEV_SMTBTY_REG_ChargingVoltage, &tmp) ||
			 dev_smtbty_i32Read(0, DEV_SMTBTY_REG_ChargingCurrent, &tmp)) {
 
			 return true;
		 }
	 }
 
	 return false;
 }

/**
 * @brief set up the prochot threshold based on the power source fake or real AC/DC
 */
void board_smtbty_setProchotGate(void)
{
	uint8_t hasBattery = board_smtbty_hasBattery();
	BOARD_PWR_SRC_TYPE pwrSrc = board_pwrSrc_getCurrentPowerSource();
	uint16_t DcProchotLevel = board_smtbty_calculateProchotLevel();
	uint32_t u32P3tLimit;

	/* Default Charger settings:   AC_Adapter */
	uint32_t acLimit1       = CHG_CONST_5000;      // PD_Adapter default: CHG_CONST_5000
	uint32_t acLimit2       = CHG_CONST_5000;      // PD_Adapter default: CHG_CONST_5000
	uint32_t DcProchot      = DcProchotLevel;

	bool chgAck = true;

	/*
	 * Below settings are following "Two NVDC charger sharing on FP8-0929.docx"
	 * combined with DcProchotLevel
	 *
	 * PROCHOT# can be enabled only in Pure-DC or Pure-AC case, not AC (or PD) + DC case
	 */
	switch (pwrSrc) {
		case BOARD_PWR_BTY:    /* Battery only */
			DcProchot = DcProchotLevel;
			LOG_DBG ("SetChager: Battery only; Update DCProchotL = %d mA", DcProchotLevel);

			_pwrSrc_releaseProchot(DEV_CHARGER_CURRENT_PORT);

			/* set the system min voltage */
			uint16_t u16tmp = 3000;
			chgAck = amd_crb_drivers_mp2764_SysMinVoltage(DEV_CHARGER_CURRENT_PORT, u16tmp);  /* Vin Min voltage 3V */
			if (!chgAck) {
				LOG_DBG("[ERROR] Fail to write sye min voltage to 3000mV!");
			}
			break;

		case BOARD_PWR_PD:
			if (hasBattery) {                              /* PD + Battery */
				acLimit1 = PERCENTAGE(board_pwrSrc_PdCurLimit(), 100);
				acLimit2 = PERCENTAGE(acLimit1, 120);
				LOG_DBG ("SetChager: PD + Battery");

				/* set the system min voltage */
				u16tmp = 3000;
				chgAck = amd_crb_drivers_mp2764_SysMinVoltage(DEV_CHARGER_CURRENT_PORT, u16tmp);  /* Vin Min voltage 3V */
				if (!chgAck) {
					LOG_DBG("[ERROR] Fail to write sye min voltage to 3000mV!");
				}

			} else if (g_isThrottleApuInAcOnly) {          /* PD only + Prochot EN */
				acLimit1 = PERCENTAGE(board_pwrSrc_PdCurLimit(), 100);
				acLimit2 = PERCENTAGE(acLimit1, 120);

				/* set the system min voltage */
				u16tmp = 4300;
				chgAck = amd_crb_drivers_mp2764_SysMinVoltage(DEV_CHARGER_CURRENT_PORT, u16tmp);  /* Vin Min voltage 4.3V */
				if (!chgAck) {
					LOG_DBG("[ERROR] Fail to write sye min voltage to 4300mV!");
				}
			} else {                                       /* PD only + Prochot OFF */
				/* Leave ACLimit 1/2 at MAX. */
			}

			_pwrSrc_releaseProchot(DEV_CHARGER_CURRENT_PORT);
			break;

		default:
			LOG_DBG ("SetChager: unknown power source: %d", pwrSrc);
			break;
	}


	if (_s_BoardSysConfig->f.acLim_Pro_enable) {
		/* Override AcProchot_1t Charger settings from EC config */
		acLimit1 = _s_BoardSysConfig->f.acLimit1_1;
		acLimit2 = _s_BoardSysConfig->f.acLimit2_1;
	}

	uint32_t val = board_smtbty_getAcpiData(ACPI_REG_IDX_AC_LIMIT1_1);

	val = board_smtbty_getAcpiData(ACPI_REG_IDX_AC_LIMIT1_2);
	if (val != 0) {
		acLimit1 = val;
	}

	val = board_smtbty_getAcpiData(ACPI_REG_IDX_AC_LIMIT2_2);
	if (val != 0) {
		acLimit2 = val;
	}

	val = board_smtbty_getAcpiData(ACPI_REG_IDX_DC_PROCHOT_LEVEL);
	if (val != 0) {
		DcProchot = val;
		g_chgDcProchotAssign = true;
	} else {
		g_chgDcProchotAssign = false;
	}

	/*
	 * Apply the settings to charger
	 */
	bool ret = true;
	LOG_DBG ("  Charger:");
	ret &= amd_crb_drivers_mp2764_AdapterACProchotL(DEV_CHARGER_CURRENT_PORT, true, false);  /* AC Limit is 110% but not 150% */
	LOG_DBG ("    - AcProchot : %s" , ret ? "[OK]" : "[FAIL]");
	ret &= amd_crb_drivers_mp2764_AdapterDCProchotL(DEV_CHARGER_CURRENT_PORT, _chg_dcProchotCalculator(DcProchot));
	LOG_DBG ("    - DcProchot %5d : %s", DcProchot, ret ? "[OK]" : "[FAIL]");
	ret &= amd_crb_drivers_mp2764_SetProchotDebounce(DEV_CHARGER_CURRENT_PORT, DEV_MP2764_PROCHOT_DEGL_IIN1_1mS,
			                                                                   DEV_MP2764_PROCHOT_DEGL_IIN2_100uS,
																	           DEV_MP2764_PROCHOT_DC_DEBOUNCE_100us);
	LOG_DBG ("    - Duration  AC1:1ms AC2:100us DC:100us : %s", ret ? "[OK]" : "[FAIL]");
	ret &= amd_crb_drivers_mp2764_SetProchotDuration(DEV_CHARGER_CURRENT_PORT, DEV_MP2764_PROCHOT_DURATION_1ms);
	LOG_DBG ("    - Debounce 1ms : %s", ret ? "[OK]" : "[FAIL]");
	ret &= board_smtbty_setAcLimit(DEV_CHARGER_CURRENT_PORT, (acLimit1), (acLimit2));  /* need to /2 as sensing resistor is 1/2 */
	LOG_DBG ("    - AcLimit1  %5d : %s", (acLimit1), ret ? "[OK]" : "[FAIL]");
	LOG_DBG ("    - AcLimit2  %5d : %s", (acLimit2), ret ? "[OK]" : "[FAIL]");

	/*
	 * apply the P3T limit
	 */
	u32P3tLimit = board_smtbty_calculateP3tLimit();
	LOG_DBG ("  ## Calculated P3T limit 0x%08X (%d)", u32P3tLimit, u32P3tLimit);
	app_thermal_updateP3tSetting(u32P3tLimit);
}

/** **************************************************
 * Charger power saving helper when enter/exit S0i3
 * ***************************************************/
/**
 * @brief charger power saving when enter S0i3
 */
void board_smtbty_chgPwrSavingS0i3Enter(void)
{
	/* no power saving feature for MP2764 */
}

/**
 * @brief charger power saving resume when exit S0i3
 */
void board_smtbty_chgPwrSavingS0i3Exit(void)
{
	/* no power saving feature for MP2764 */
}
