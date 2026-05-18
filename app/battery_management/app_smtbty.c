/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <assert.h>
#include <string.h>
#include <dev_utility.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "app_acpi.h"
#if CONFIG_CHARGER_ISL9241
#include "dev_isl9241.h"
#elif CONFIG_CHARGER_MP2764
#include "amd_crb_drivers_mp2764.h"
#endif
#include "app_pseq.h"
#include "board_smtbty.h"
#include "board_pwrSrc.h"
#include "app_AcDcSwitch.h"
#include "app_smtbty.h"
#include "task_handler.h"

LOG_MODULE_REGISTER(smtbty, CONFIG_BATTERY_MGMT_LOG_LEVEL);

/* ************************** *
 *          Macro             *
 * ************************** */
#define F_SMTBTY_MAX_STRING_BUFFER  (100)
K_TIMER_DEFINE(_lastRefreshTimeStamp,NULL,NULL);
static void app_smtbty_dbgCallback(struct k_timer *timer);
K_TIMER_DEFINE(_s_smtbtyDbgTimerId,app_smtbty_dbgCallback,NULL);

/* ************************** *
 *     static valuable        *
 * ************************** */
static struct k_timer g_batteryReadyDelay_timer;

/* bit[x] = 1 means Bty ID x had tripped. */
static uint32_t _s_u32BtyTrippedBitMap = 0;

static uint8_t _s_u8SmtBtyInitFlag = 0;
static uint8_t _s_u8SelectedBattery = 0;
uint8_t *task_smtbtySlpReady;

static uint8_t _s_smtbty_strBuf[F_SMTBTY_MAX_STRING_BUFFER];

static APP_BATTERY_INSTANCE _s_batIns[MAX_BATTERY_SUPPORTED];
static APP_SMTBTY_CLIENT_ENTITY _s_smtbty_callBacks;
static APP_BATTERY_FLAG _s_cachedBtyFlag[MAX_BATTERY_SUPPORTED];
static bool _s_preBtyPresent = false;
static bool g_batteryIsReadyToGo = true;

/* ************************** *
 *     global valuable        *
 * ************************** */
APP_SMTBTY_CHG_STATUS _s_smtbty_chgSt = APP_CHG_DISABLE;
extern uint8_t gs_eSpiMmioSpace[CONFIG_ESPI_NPCX_PERIPHERAL_ACPI_SHD_MEM_SIZE];

#if (APP_SMTBTY_DEBUG_ENABLE)
APP_SMTBTY_DBG_FLAG   g_smtbty_dbg_flag;
APP_SMTBTY_DBG_VECTOR g_smtbty_dbg_vector;
#endif

/**
 * These two proc will be used only if the ones are not specified by app.
 */
static bool _battery_isPresent(uint8_t btyId);  /* default batter presence checking proc. */
static bool _battery_doCharge(uint8_t btyId);   /* default charging proc, which handles only the single charger case */
static void _battery_resetData(uint8_t btyId);
static void _smtbty_assignStringBuffer(void);

/**
 * @brief assign string buffer to smart battery process
 */
void _smtbty_assignStringBuffer(void)
{
	uint32_t idx = 0;

	/* assign string buffer */
	/* 1. battery */
	for (uint8_t btyId = 0; btyId < MAX_BATTERY_SUPPORTED; btyId ++) {
		if (_s_platform_btyBufTable[btyId]) {
			for (uint8_t i = 0; _s_platform_btyBufTable[btyId][i].reg != DEV_SMTBTY_REG_INVALID; i++) {
				k_mutex_init(&_s_platform_btyBufTable[btyId][i].rwLock);
				
				if (_s_platform_btyBufTable[btyId][i].data.width > 4 && 
					_s_platform_btyBufTable[btyId][i].access.f.accessType != DEV_SMTBTY_REG_DISABLED) {

					_s_platform_btyBufTable[btyId][i].data.value.pBuf = &_s_smtbty_strBuf[idx];
					idx += _s_platform_btyBufTable[btyId][i].data.width;
				}
			}
		}

		for (uint8_t i = 0; _s_batBuf[btyId][i].reg != DEV_SMTBTY_REG_INVALID; i++) {
			k_mutex_init(&_s_batBuf[btyId][i].rwLock);
			
			if (_s_batBuf[btyId][i].data.width > 4 &&
				_s_batBuf[btyId][i].access.f.accessType != DEV_SMTBTY_REG_DISABLED) {

				_s_batBuf[btyId][i].data.value.pBuf = &_s_smtbty_strBuf[idx];
				idx += _s_batBuf[btyId][i].data.width;
			}
		}
	}

	/* 2. charger */
	for (uint8_t chgId = 0; chgId < MAX_CHARGER_SUPPORTED; chgId ++) {
		if (_s_platform_chgBufTable[chgId]) {
			for (uint8_t i = 0; _s_platform_chgBufTable[chgId][i].reg != DEV_CHARGER_REG_INVALID; i++) {
				if (_s_platform_chgBufTable[chgId][i].data.width > 4 && 
					_s_platform_chgBufTable[chgId][i].access.f.accessType != DEV_SMTBTY_REG_DISABLED) {

					_s_platform_chgBufTable[chgId][i].data.value.pBuf = &_s_smtbty_strBuf[idx];
					idx += _s_platform_chgBufTable[chgId][i].data.width;
				}
			}
		}

		for (uint8_t i = 0; _s_chgBuf[chgId][i].reg != DEV_CHARGER_REG_INVALID; i++) {
			if (_s_chgBuf[chgId][i].data.width > 4 && 
				_s_chgBuf[chgId][i].access.f.accessType != DEV_SMTBTY_REG_DISABLED) {

				_s_chgBuf[chgId][i].data.value.pBuf = &_s_smtbty_strBuf[idx];
				idx += _s_chgBuf[chgId][i].data.width;
			}
		}
	}

	/* make sure the buffer space is enough */
	assert (idx <= F_SMTBTY_MAX_STRING_BUFFER);
	LOG_INF("SmtBty string buffer allocated %d/%d", idx, F_SMTBTY_MAX_STRING_BUFFER);
	if (idx > F_SMTBTY_MAX_STRING_BUFFER) while(1);
}

/**
 * @brief Temporarily disable battery access.
 */
void app_smtbty_frozen(void)
{
	k_timer_start(&g_batteryReadyDelay_timer, K_SECONDS(20), K_NO_WAIT);
	g_batteryIsReadyToGo = false;
}

/**
 * @brief Enable battery access.
 */
void app_smtbty_unfrozen(void)
{
	k_timer_stop(&g_batteryReadyDelay_timer);
	g_batteryIsReadyToGo = true;
}

/**
 * _batteryReadyDelayTimerCallback
 *
 * @note
 *  timer callback
 */
static void _batteryReadyDelayTimerCallback(struct k_timer *timer)
{
	app_smtbty_unfrozen();
}

/**
 * @brief app smart battery init
 */
void app_smtbty_init(void)
{
	k_timer_init(&g_batteryReadyDelay_timer, _batteryReadyDelayTimerCallback, NULL);

	memset(_s_smtbty_i2c_bus_errCount, 0, sizeof(_s_smtbty_i2c_bus_errCount));
	memset(_s_smtbty_strBuf, 0, F_SMTBTY_MAX_STRING_BUFFER);
	memset(_s_batIns, 0, sizeof(_s_batIns));
	for (uint8_t i = 0; i < MAX_BATTERY_SUPPORTED; i++) {
		/* A value of 40% indicates the battery should be charged at 40% of maximum rate.
		 * A value of 0% indicates battery charging should be stopped until this method is called again. */
		_s_batIns[i].u8ChargeThrottle = APP_SMTBTY_CHG_THROTTLE_DISABLED;
	}

	memset(&_s_smtbty_callBacks, 0, sizeof(_s_smtbty_callBacks));

	memset(_s_platform_btyBufTable, 0, sizeof(_s_platform_btyBufTable));
	memset(_s_platform_chgBufTable, 0, sizeof(_s_platform_chgBufTable));
}

/**
 * @brief app smart battery get client entity
 */
APP_SMTBTY_CLIENT_ENTITY * app_smtbty_getClientEntity (void)
{
	return &_s_smtbty_callBacks;
}

/**
 * @brief app smart battery start process
 */
void app_smtbty_start(void)
{
	_smtbty_assignStringBuffer();

	/**
	 * f_smtbty provides two default routine for battery detection and do-charge
	 * the default do-charge routine only handles singla charger case
	 */
	if (!_s_smtbty_callBacks.pfnPresent)
		_s_smtbty_callBacks.pfnPresent = _battery_isPresent;
	if (!_s_smtbty_callBacks.pfnCharger)
		_s_smtbty_callBacks.pfnCharger = _battery_doCharge;

	for (uint8_t btyId = 0; btyId < MAX_BATTERY_SUPPORTED; btyId ++) {
		_battery_resetData(btyId);
	}

	_s_u8SmtBtyInitFlag = 1;

#if (APP_SMTBTY_DEBUG_ENABLE)
	app_smtbty_dbgInit();
#endif
}

/**
 * @brief retrun the battery count
 *
 * @retval number of battery.
 */
uint8_t app_battery_countGet(void)
{
	return (MAX_BATTERY_SUPPORTED);
}

/**
 * @brief return the selected battery
 *
 * @retval number of battery.
 */
uint8_t app_battery_selectedGet(void)
{
	return _s_u8SelectedBattery;
}

/**
 * @brief return the selected battery
 *
 * @param  btyId: index of the battery
 */
void app_battery_selectedSet(uint8_t btyId)
{
	if (btyId < MAX_BATTERY_SUPPORTED)
		_s_u8SelectedBattery = btyId;

	LOG_WRN("**** Set the active battery to %d", btyId);
}

/**
 * @brief return the warning threshold of the battery
 *
 * @param  btyId: index of the battery
 * @retval warning threshold
 */
uint16_t app_battery_warningThresholdGet(uint8_t btyId)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	return _s_batIns[btyId].u16ThresholdBtyWarning;
}

/**
 * @brief set the warning threshold of the battery
 *
 * @param  btyId:          index of the battery
 * @param  u16threshold:   threshold
 */
void app_battery_warningThresholdSet(uint8_t btyId, uint16_t u16threshold)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	_s_batIns[btyId].u16ThresholdBtyWarning = u16threshold;
}

/**
 * @brief return the low threshold of the battery
 *
 * @param  btyId: index of the battery
 * @retval low threshold
 */
uint16_t app_battery_lowThresholdGet(uint8_t btyId)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	return _s_batIns[btyId].u16ThresholdBtyLow;
}

/**
 * @brief set the low threshold of the battery
 *
 * @param  btyId:          index of the battery
 * @param  u16threshold:   threshold
 */
void app_battery_lowThresholdSet(uint8_t btyId, uint16_t u16threshold)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	_s_batIns[btyId].u16ThresholdBtyLow = u16threshold;
}

/**
 * @brief return the critical threshold of the battery
 *
 * @param  btyId: index of the battery
 * @retval critical threshold
 */
uint16_t app_battery_criticalThresholdGet(uint8_t btyId)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	return _s_batIns[btyId].u16ThresholdBtyCritical;
}

/**
 * @brief set the critical threshold of the battery
 *
 * @param  btyId:          index of the battery
 * @param  u16threshold:   threshold
 */
void app_battery_criticalThresholdSet(uint8_t btyId, uint16_t u16threshold)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	_s_batIns[btyId].u16ThresholdBtyCritical = u16threshold;
}

/**
 * @brief return the trip point of the battery
 *
 * @param  btyId: index of the battery
 * @retval trip point
 */
uint16_t app_battery_tripPointGet(uint8_t btyId)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	return _s_batIns[btyId].u16ThresholdBtyTripPoint;
}

/**
 * @brief set the trip point of the battery
 *
 * @param  btyId:          index of the battery
 * @param  u16tripPoint:   tripPoint
 */
void app_battery_tripPointSet(uint8_t btyId, uint16_t u16tripPoint)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	_s_batIns[btyId].u16ThresholdBtyTripPoint = u16tripPoint;
	_s_batIns[btyId].u16LastRemainingCapicity = 0;
}

/**
 * @brief return the Charge Throttle of the battery
 *
 * @param  btyId: index of the battery
 * @retval Charge Throttle
 */
uint8_t app_battery_ChargeThrottleGet(uint8_t btyId)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	return _s_batIns[btyId].u8ChargeThrottle;
}

/**
 * @brief set the Charge Throttle of the battery
 *
 * @param  btyId:          index of the battery
 * @param  u8Throttle:     Throttle
 */
void app_battery_ChargeThrottleSet(uint8_t btyId, uint8_t u8Throttle)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);
	_s_batIns[btyId].u8ChargeThrottle = u8Throttle;
}

/**
 * @brief return if the battery is presented
 *
 * @param  btyId: index of the battery
 * @retval True if presented
 */
bool _battery_isPresent(uint8_t btyId)
{
	static uint8_t _btyDetachDebounce = 0;
	/* set it to non-zero is apt to report battery is present */
	#define __F_BTY_DETACH_DEBOUNCE__ 0

	if ((k_timer_status_get(&_lastRefreshTimeStamp) == 0)&&(k_timer_remaining_get(&_lastRefreshTimeStamp)!=0)) {
		/** 
		 * The last check (no more than APP_BATTERY_PRESENT_RECHECK_DELAY us)
		 * shows the battery is prelent.
		 * Return true reduce SMBus pressure.
		 */
		_s_preBtyPresent = g_batPresentFlag;
		g_batPresentFlag = true;
		if (_s_preBtyPresent != g_batPresentFlag)
			board_pwrSrcEvent();
		return true;
	} else {
		DEV_SMTBTY_REG_ITEM * pBtyReg;
		pBtyReg = dev_smtbty_findRegItem(btyId, DEV_SMTBTY_REG_ChargingCurrent);
		if (pBtyReg && dev_smtbty_autoRead(pBtyReg, DEV_SMTBTY_ID_TO_PORT(btyId), BATTERY_I2C_ADDRESS)) {
			pBtyReg = dev_smtbty_findRegItem(btyId, DEV_SMTBTY_REG_ChargingVoltage);
			if (pBtyReg && dev_smtbty_autoRead(pBtyReg, DEV_SMTBTY_ID_TO_PORT(btyId), BATTERY_I2C_ADDRESS)) {
                k_timer_start(&_lastRefreshTimeStamp, K_USEC(APP_BATTERY_PRESENT_RECHECK_DELAY), K_NO_WAIT);
                _btyDetachDebounce = __F_BTY_DETACH_DEBOUNCE__;
				_s_preBtyPresent = g_batPresentFlag;
				g_batPresentFlag = true;
				if (_s_preBtyPresent != g_batPresentFlag)
					board_pwrSrcEvent();
				return true;
			}
		}

		k_msleep(2);
		if (pBtyReg && dev_smtbty_autoRead(pBtyReg, DEV_SMTBTY_ID_TO_PORT(btyId), BATTERY_I2C_ADDRESS)) {
			k_timer_start(&_lastRefreshTimeStamp,K_USEC(APP_BATTERY_PRESENT_RECHECK_DELAY),K_NO_WAIT);
			_btyDetachDebounce = __F_BTY_DETACH_DEBOUNCE__;
			_s_preBtyPresent = g_batPresentFlag;
			g_batPresentFlag = true;
			if (_s_preBtyPresent != g_batPresentFlag)
				board_pwrSrcEvent();
			return true;
		}
	}

	if (_btyDetachDebounce) {
		_btyDetachDebounce --;
		return true;
	}
	_s_preBtyPresent = g_batPresentFlag;
	g_batPresentFlag = false;
	if (_s_preBtyPresent != g_batPresentFlag)
		board_pwrSrcEvent();
	return false;
}

/**
 * @brief battery do charging process
 *
 * @param  btyId: index of the battery
 * @retval True if successful
 */
bool _battery_doCharge(uint8_t btyId)
{
	static APP_SMTBTY_CHG_STATUS oldChgSt = APP_CHG_DISABLE;
	APP_SMTBTY_CHG_STATUS newChgSt = APP_CHG_ENABLED_NORMAL;
	bool isAcOk = true;
	bool isBtyPresent = false;

	if (btyId >= MAX_BATTERY_SUPPORTED || _s_smtbty_chgSt == APP_CHG_BATTERY_IS_DEAD)
		return false;

#if (APP_BATTERY_REACTIVING_VOLTAGE > 0 && APP_BATTERY_REACTIVING_CURRENT > 0)
	static uint32_t _s_smtbty_reactivingCounter;
	static uint32_t _s_smtbty_batteryResponseCounter;

	if (_s_smtbty_chgSt == APP_CHG_REACTIVATING) {
		if (_s_smtbty_reactivingCounter > 1800) { // by 100ms interval, 1800 stands for 3mins; by 500ms interval, it's 15mins
			if (_s_smtbty_callBacks.pfnChgDisable)
				_s_smtbty_callBacks.pfnChgDisable();
			newChgSt = APP_CHG_BATTERY_IS_DEAD;
			goto _endof_doCharge;
		} else if (_s_smtbty_reactivingCounter % 20 == 0) {
			/* battery re-active */

			if (_s_smtbty_callBacks.pfnChgEnble)
				_s_smtbty_callBacks.pfnChgEnble();

			uint16_t current = 0, voltage = 0;
			if ( dev_smtbty_i32Read(btyId, DEV_SMTBTY_REG_ChargingVoltage, &voltage) && 
				 dev_smtbty_i32Read(btyId, DEV_SMTBTY_REG_ChargingCurrent, &current) &&
				 current && voltage ) {

				if (current < APP_BATTERY_REACTIVING_CURRENT)
					current = APP_BATTERY_REACTIVING_CURRENT;

				_s_smtbty_batteryResponseCounter ++;

			} else {
				current = APP_BATTERY_REACTIVING_CURRENT;
				voltage = APP_BATTERY_REACTIVING_VOLTAGE;
				_s_smtbty_batteryResponseCounter = 0;
			}
#if CONFIG_CHARGER_ISL9241
			/* Some of chargers are sensitive on the written order, so we try the both */
			if ( dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingCurrent, current) &&
				 dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingVoltage, voltage) ) {

				LOG_INF("Reactivating ... %5d mV, %4d mA;  resp %5d, active %5d", voltage, current, _s_smtbty_batteryResponseCounter, _s_smtbty_reactivingCounter);

			} else if ( dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingVoltage, voltage) &&
						dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingCurrent, current) ) {

				LOG_INF("Reactivating ... %5d mV, %4d mA;  resp %5d, active %5d", voltage, current, _s_smtbty_batteryResponseCounter, _s_smtbty_reactivingCounter);

			} else {
				LOG_ERR("Reactivating ... N/A mV, N/A mA;  resp %5d, active %5d; !!!communication error!!!", _s_smtbty_batteryResponseCounter, _s_smtbty_reactivingCounter);
			}
#endif
		}

		if (_s_smtbty_batteryResponseCounter > 10) {
			newChgSt = APP_CHG_PRE_CHARGE;
			goto _endof_doCharge;
		}

		_s_smtbty_reactivingCounter ++;
		return false;
	} else {
		 _s_smtbty_reactivingCounter = 0;
		 _s_smtbty_batteryResponseCounter = 0;
	}
#endif

	if (_s_smtbty_callBacks.pfnIsAcPresent) {
		isAcOk = _s_smtbty_callBacks.pfnIsAcPresent();
	}

	if (_s_smtbty_callBacks.pfnPresent) {
		isBtyPresent = _s_smtbty_callBacks.pfnPresent(btyId);
	}

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
			static uint16_t __s_oldVoltage, __s_oldCurrent;
			static uint8_t __s_skippedLoops;

			uint16_t u16voltage, u16current;

			if (_s_smtbty_callBacks.pfnChgEnble)
				_s_smtbty_callBacks.pfnChgEnble();

#if (APP_BATTERY_PRECHARGE_THRESHOLD > 0 && APP_BATTERY_PRECHARGE_VOLTAGE > 0 && APP_BATTERY_PRECHARGE_CURRENT > 0)
			uint16_t u16BatRemainCapacity = 0;

			if ( dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_RemainingCapacity, &u16BatRemainCapacity) &&
				 u16BatRemainCapacity <= APP_BATTERY_PRECHARGE_THRESHOLD) {
				if (__s_skippedLoops ++ > 30) {
#if CONFIG_CHARGER_ISL9241
					/* Some of chargers are sensitive on the written order, so we try the both */
					if ( dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingCurrent, APP_BATTERY_PRECHARGE_CURRENT) &&
						 dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingVoltage, APP_BATTERY_PRECHARGE_VOLTAGE) ) {

						LOG_INF("Pre-charging ... %5d mV, %4d mA", APP_BATTERY_PRECHARGE_VOLTAGE, APP_BATTERY_PRECHARGE_CURRENT);

					} else if ( dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingVoltage, APP_BATTERY_PRECHARGE_VOLTAGE) &&
							    dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingCurrent, APP_BATTERY_PRECHARGE_CURRENT) ) {

						LOG_INF("Pre-charging ... %5d mV, %4d mA", APP_BATTERY_PRECHARGE_VOLTAGE, APP_BATTERY_PRECHARGE_CURRENT);

					} else {
						LOG_ERR("Pre-charging ... N/A mV, N/A mA; !!!communication error!!!");
					}
#endif
					__s_skippedLoops = 0;
				}

				newChgSt = APP_CHG_PRE_CHARGE;

			} else
#endif
			if (dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_ChargingVoltage, &u16voltage) && 
				dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_ChargingCurrent, &u16current)) {
#if CONFIG_CHARGER_ISL9241
				uint16_t u16ReqCurrent = u16current;		
#endif
#if (CONFIG_LOG||APP_BATTERY_FULL_CHARGE_VOLTAGE)
				uint16_t u16ReqVoltage = u16voltage;
#endif
#if (APP_SMTBTY_DEBUG_ENABLE)
				g_smtbty_dbg_vector.u16requiredVoltage = u16voltage;
				g_smtbty_dbg_vector.u16requiredCurrent = u16current;
#endif
				if (_s_smtbty_callBacks.pfnGetChargeCurrentLimit) {
					uint16_t u16ChargeCurrentLimit = _s_smtbty_callBacks.pfnGetChargeCurrentLimit(btyId);
					if (u16current > u16ChargeCurrentLimit)
						u16current = u16ChargeCurrentLimit;
				}

#if (APP_SMTBTY_DEBUG_ENABLE)
				if (g_smtbty_dbg_flag.f.isEnabled && g_smtbty_dbg_vector.u32remainExecuteTimeInSeconds) {
					if (u16voltage < g_smtbty_dbg_vector.u16voltageLowLimit)
						u16voltage = g_smtbty_dbg_vector.u16voltageLowLimit;
					if (u16voltage > g_smtbty_dbg_vector.u16voltageHighLimit)
						u16voltage = g_smtbty_dbg_vector.u16voltageHighLimit;

					if (u16current < g_smtbty_dbg_vector.u16currentLowLimit)
						u16current = g_smtbty_dbg_vector.u16currentLowLimit;
					if (u16current > g_smtbty_dbg_vector.u16currentHighLimit)
						u16current = g_smtbty_dbg_vector.u16currentHighLimit;
				}

				g_smtbty_dbg_vector.u16appliedVoltage = u16voltage;
				g_smtbty_dbg_vector.u16appliedCurrent = u16current;
#endif

#ifdef APP_BATTERY_FULL_CHARGE_VOLTAGE
				if (0 == u16ReqVoltage) {
					/**
					 * Override the charge voltage to battery full charge voltage 
					 * (isl9241 x15 := 0 can cause charger stop powering VIN)
					 */
					u16voltage = APP_BATTERY_FULL_CHARGE_VOLTAGE;
				}
#endif
				if (__s_oldVoltage != u16voltage || __s_oldCurrent != u16current || (__s_skippedLoops ++ > 30)) {
#if CONFIG_CHARGER_ISL9241
					/* Some of chargers are sensitive on the written order, so we try the both */
					if ( dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingCurrent, u16current) &&
						 dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingVoltage, u16voltage) ) {

						LOG_WRN("Charging ... %5d mV, %4d mA, Req. %5d mV, %4d mA", u16voltage, u16current, u16ReqVoltage, u16ReqCurrent);

					} else if ( dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingVoltage, u16voltage) &&
							    dev_isl9241_i32Write(0, DEV_CHARGER_REG_ChargerChargingCurrent, u16current) ) {

						LOG_WRN("Charging ... %5d mV, %4d mA, Req. %5d mV, %4d mA", u16voltage, u16current, u16ReqVoltage, u16ReqCurrent);

					} else {
						LOG_ERR("Charging ... N/A mV, N/A mA, Req. %5d mV, %4d mA; !!!communication error!!!", u16ReqVoltage, u16ReqCurrent);
					}
#endif
					__s_oldVoltage = u16voltage;
					__s_oldCurrent = u16current;

					__s_skippedLoops = 0;
				}

				newChgSt = APP_CHG_ENABLED_NORMAL;
			}
		}

#if (APP_BATTERY_REACTIVING_VOLTAGE > 0 && APP_BATTERY_REACTIVING_CURRENT > 0)
		if (newChgSt != APP_CHG_DISABLE && dev_smtbty_getContinuousErrorNum(BATTERY_I2C_ADDRESS) > 0x120) {
			/* battery re-active */
			newChgSt = APP_CHG_REACTIVATING;
		} 
#endif

	} else {
		/* Only battery is present (no AC) */
		newChgSt = APP_CHG_DISABLE;
	}

	if (newChgSt == APP_CHG_DISABLE && _s_smtbty_callBacks.pfnChgDisable) {
		static uint8_t __s_skippedChgDisLoops; 
		if (oldChgSt != newChgSt || __s_skippedChgDisLoops > 20) {
			_s_smtbty_callBacks.pfnChgDisable();
			__s_skippedChgDisLoops = 0;
		} else {
			__s_skippedChgDisLoops ++;
		}
	}
#if (APP_BATTERY_REACTIVING_VOLTAGE > 0 && APP_BATTERY_REACTIVING_CURRENT > 0)
_endof_doCharge:
#endif
	_s_smtbty_chgSt = newChgSt;

	if (oldChgSt != newChgSt) {
		oldChgSt = newChgSt;
		return true;
	}

	return false;
}

/**
 * @brief battery reset all the data
 *
 * @param  btyId: index of the battery
 */
void _battery_resetData(uint8_t btyId)
{
	/* reset common data buffer */
	for (uint8_t i = 0; _s_batBuf[btyId][i].reg != DEV_SMTBTY_REG_INVALID; i++) {
		DEV_SMTBTY_REG_ITEM * pItem = &_s_batBuf[btyId][i];

		k_mutex_lock(&pItem->rwLock,K_FOREVER);
		pItem->u32refreshCounter = 0;
		pItem->access.f.dataAvailable = 0;
		/* dont't clear data as the field may hold the string buffer pointer */
		// pItem->data.value.u32 = 0;
		pItem->data.lastSuccessValue.u32 = 0xFFFFFFFF;
		k_mutex_unlock(&pItem->rwLock);
	}

	/* reset platform special data */
	if (_s_platform_btyBufTable[btyId]) {
		for (uint8_t i = 0; _s_platform_btyBufTable[btyId][i].reg != DEV_SMTBTY_REG_INVALID; i++) {
			DEV_SMTBTY_REG_ITEM * pItem = &_s_platform_btyBufTable[btyId][i];

			k_mutex_lock(&pItem->rwLock,K_FOREVER);
			pItem->u32refreshCounter = 0;
			pItem->access.f.dataAvailable = 0;
			/* dont't clear data as the field may hold the string buffer pointer */
			// pItem->data.value.u32 = 0;
			pItem->data.lastSuccessValue.u32 = 0xFFFFFFFF;
			k_mutex_unlock(&pItem->rwLock);
		}
	}
}

/**
 * @brief battery calculate the flags
 *
 * @param  btyId: index of the battery
 * @retval app battery flag
 */
APP_BATTERY_FLAG app_battery_flagsCalculate(uint8_t btyId)
{
	APP_BATTERY_FLAG st = {0};

	APP_BATTERY_STATUS btyStatus;                                     // battery status defined by Smart battery data sheet.
	APP_SMTBTY_BTY_STATUS clientBtySignal = APP_BATTERY_INVALID_SIGNAL; // client friendly battery status.
	uint16_t u16BatRemainCapacity = 0, u16BatFullChargeCapacity = 0;
	int16_t i16BatteryCurrent = 0;

	if (btyId >= MAX_BATTERY_SUPPORTED)
		return st;

	APP_BATTERY_INSTANCE * pBat = &_s_batIns[btyId];
	if (!pBat->btyFlag.f.connected)
		goto _endof_flagsCalculate;

	st.f.connected = 1;

	/* trying to read DEV_SMTBTY_REG_BatteryStatus one more time if the 1st read is failed */
	if ( dev_smtbty_i32Read(btyId, DEV_SMTBTY_REG_BatteryStatus, &btyStatus.u16Status) ||
		 dev_smtbty_i32Read(btyId, DEV_SMTBTY_REG_BatteryStatus, &btyStatus.u16Status) ) {
		/* read success */
	} else {
//		st.f.connected = 0;
		goto _endof_flagsCalculate;
	}

	if ( dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_RemainingCapacity, &u16BatRemainCapacity) &&
	     dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_FullChargeCapacity, &u16BatFullChargeCapacity) &&
	     dev_smtbty_i32CacheGet(btyId, DEV_SMTBTY_REG_Current, &i16BatteryCurrent) ) {
		/* read success */
		if (i16BatteryCurrent == 0xFFFF) {
//			st.f.connected = 0;
			goto _endof_flagsCalculate;
		}
	} else {
//		st.f.connected = 0;
		goto _endof_flagsCalculate;
	}

	uint16_t threshold;
	/**
	 * Check to see if a user provided WARNING threshold is non-zero and use it
	 * if it is.  Otherwise, use a defalt threshold of 20% of the full charge
	 * capacity.
	 */
	threshold = u16BatFullChargeCapacity / 5;
	if (pBat->u16ThresholdBtyWarning)
		threshold = pBat->u16ThresholdBtyWarning;
	if (u16BatRemainCapacity < threshold)
		st.f.warning = 1;

	/**
	 * Check to see if a user provided LOW threshold is non-zero and use it
	 * if it is.  Otherwise, use a defalt threshold of 10% of the full charge
	 * capacity.
	 */
	threshold = u16BatFullChargeCapacity / 10;
	if (pBat->u16ThresholdBtyLow)
		threshold = pBat->u16ThresholdBtyLow;
	if (u16BatRemainCapacity < threshold)
		st.f.low = 1;

	/**
	 * Check to see if a user provided CRITICAL threshold is non-zero and use
	 * it if it is.  Otherwise, use a defalt threshold of 4% of the full
	 * charge capacity.
	 */
	threshold = u16BatFullChargeCapacity / 25;
	if (pBat->u16ThresholdBtyCritical)
		threshold = pBat->u16ThresholdBtyCritical;
	if (u16BatRemainCapacity < threshold)
		st.f.critical = 1;

	/* trip point */
	if (0 != pBat->u16ThresholdBtyTripPoint && 0 != pBat->u16LastRemainingCapicity) {
		if ( ( u16BatRemainCapacity > pBat->u16ThresholdBtyTripPoint &&
			   pBat->u16LastRemainingCapicity < pBat->u16ThresholdBtyTripPoint ) ||
			 ( u16BatRemainCapacity < pBat->u16ThresholdBtyTripPoint &&
			   pBat->u16LastRemainingCapicity > pBat->u16ThresholdBtyTripPoint ) ) {

			/* set tripped flag */
			LOG_INF(" >> Set _BTP flag");
			LOG_INF(" >>   u16BatRemainCapacity           %04X", u16BatRemainCapacity);
			LOG_INF(" >>   pBat->u16LastRemainingCapicity %04X", pBat->u16LastRemainingCapicity);
			LOG_INF(" >>   pBat->u16ThresholdBtyTripPoint %04X", pBat->u16ThresholdBtyTripPoint);
			LOG_INF(" >>   _s_u32BtyTrippedBitMap   was   %04X", _s_u32BtyTrippedBitMap);
			_s_u32BtyTrippedBitMap |= (1 << btyId);
		}
	}

	static uint16_t s_dbg_cur_cnt = 0;
	s_dbg_cur_cnt++;
	if(s_dbg_cur_cnt > 10) {
		s_dbg_cur_cnt = 0;
		LOG_DBG(" Battery current is %d", i16BatteryCurrent);
	}
	
	/** 
	 * Battery Trip bit can be cleared only by app_battery_clearTrippedFlag()
	 * so assert the flag in st.f untill it be cleared.
	 */
	if (_s_u32BtyTrippedBitMap & (1 << btyId)) {
		st.f.tripped = 1;
	}

	if (u16BatRemainCapacity != pBat->u16ThresholdBtyTripPoint) {
		pBat->u16LastRemainingCapicity = u16BatRemainCapacity;
	}

	/* status */
	if (btyStatus.f.fullyCharged) {
		LOG_WRN("Battery status fullyCharged");

		/* battery charge done */
		clientBtySignal = APP_BATTERY_FULL_CHARGED;
	} else if (btyStatus.f.fullyDischarged) {
		LOG_WRN("Battery status fully discharged");

		if (i16BatteryCurrent > 0) {
			st.f.charging = 1;

			clientBtySignal = APP_BATTERY_CHARGING;
		} else {
			clientBtySignal = APP_BATTERY_FULL_DISCHARGED;
		}

	} else if (i16BatteryCurrent < 0) {
		st.f.discharging = 1;

		clientBtySignal = APP_BATTERY_DISCHARGING;
	} else if (i16BatteryCurrent > 0) {
		st.f.charging = 1;

		clientBtySignal = APP_BATTERY_CHARGING;
	} else {
		LOG_WRN("Battery status idle as Current is 0.");
		st.f.idle = 1;

		clientBtySignal = APP_BATTERY_IDLE;
	}

	if (btyStatus.f.errCode)
		LOG_WRN("Battery status errCode 0x%02X", btyStatus.f.errCode);

	if (!pBat->btyFlag.f.connected)
		st.u32flag = 0;

_endof_flagsCalculate:
	if (_s_smtbty_callBacks.pfnBatteryLed) {

		/* override the client battery signal by the charger status */
		if (_s_smtbty_chgSt == APP_CHG_BATTERY_IS_DEAD) {
			st.f.dead = 1;
			clientBtySignal = APP_BATTERY_IS_DEAD;
		} else if (_s_smtbty_chgSt == APP_CHG_REACTIVATING) {
			st.f.dead = 1;
			clientBtySignal = APP_BATTERY_REACTIVATING;
		} else if (_s_smtbty_chgSt == APP_CHG_PRE_CHARGE) {
			clientBtySignal = APP_BATTERY_PRE_CHARGE;
		}

		/* report to client */
		if (clientBtySignal != APP_BATTERY_INVALID_SIGNAL)
			_s_smtbty_callBacks.pfnBatteryLed(clientBtySignal);
	}

	_s_cachedBtyFlag[btyId] = st;
	return st;
}

/**
 * @brief battery get cache flags
 *
 * @param  btyId: index of the battery
 * @retval app battery flag
 */
APP_BATTERY_FLAG app_battery_flagsCacheGet(uint8_t btyId)
{
	APP_BATTERY_FLAG st = {0};
	if (btyId >= MAX_BATTERY_SUPPORTED)
		return st;

	return _s_cachedBtyFlag[btyId];
}

/**
 * @brief battery clear the tripped flag
 *
 * @param  btyId: index of the battery
 */
void app_battery_clearTrippedFlag(uint8_t btyId)
{
	if (btyId >= MAX_BATTERY_SUPPORTED)
		return;

	_s_u32BtyTrippedBitMap &= ~(1 << btyId);
}

/**
 * @brief app smart battery process
 */
void app_smtbty_proc (void)
{
	DEV_CHARGER_REG_ITEM * pItem;

	if (!_s_u8SmtBtyInitFlag)
		return;

	for (uint8_t btyId = 0; btyId < MAX_BATTERY_SUPPORTED; btyId ++) {
		/**
		 * TODO:
		 * set Smart Battery Selector 
		 * (APP_BATCHG_SELECTOR_I2C_ADDRESS)
		 * if more than one battery 
		 */

		APP_BATTERY_INSTANCE * pBat = &_s_batIns[btyId];
		bool bat_refresh = false;

		/* pre processing */
		if (_s_smtbty_callBacks.pfnPreProcessing)
			_s_smtbty_callBacks.pfnPreProcessing(btyId);

		/* Check if battery presents */
		if (_s_smtbty_callBacks.pfnPresent(btyId)) {
			if (!pBat->btyFlag.f.connected) {
				LOG_WRN("NEW BATTERY PLUG-IN !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
				_s_smtbty_chgSt = APP_CHG_DISABLE;

				_battery_resetData(btyId);
				dev_smtbty_resetBusErrorCounter(BATTERY_I2C_ADDRESS);

				/* Clear the Battery Tripped bit */
				_s_u32BtyTrippedBitMap &= ~(1 << btyId);

				if (_s_smtbty_callBacks.pfnOnBatteryAttach)
					_s_smtbty_callBacks.pfnOnBatteryAttach(btyId);

				/* reset the battery status */
				pBat->btyFlag.u32flag = 0;
				pBat->btyFlag.f.connected = 1;
				pBat->u32ErrCnt = 0;
				bat_refresh = true;
				
				/* Update the battery connect MMIO */
				gs_eSpiMmioSpace[ACPI_CHARGER_STATUS] |=  ACPI_BATTERY_CONNECTED;

				if (_s_smtbty_callBacks.pfnBatteryLed)
					_s_smtbty_callBacks.pfnBatteryLed(APP_BATTERY_IDLE);
			}
		} else if (pBat->btyFlag.f.connected) {
			/* If was connected */
			LOG_WRN("BATTERY REMOVING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
			_s_smtbty_chgSt = APP_CHG_DISABLE;

			_battery_resetData(btyId);
			dev_smtbty_resetBusErrorCounter(BATTERY_I2C_ADDRESS);

			/* Clear the Battery Tripped bit */
			_s_u32BtyTrippedBitMap &= ~(1 << btyId);

			if (_s_smtbty_callBacks.pfnOnBatteryDetach)
				_s_smtbty_callBacks.pfnOnBatteryDetach(btyId);

			pBat->btyFlag.f.connected = 0;
			bat_refresh = true;
			
			/* Update the battery connect MMIO */
			gs_eSpiMmioSpace[ACPI_CHARGER_STATUS] &= ~ACPI_BATTERY_CONNECTED;

			if (_s_smtbty_callBacks.pfnBatteryLed)
				_s_smtbty_callBacks.pfnBatteryLed(APP_BATTERY_NOT_CONNECTED);

			if (_s_smtbty_callBacks.pfnChgDisable)
				_s_smtbty_callBacks.pfnChgDisable();
		} else {
			static uint8_t u8skippedLoops = 0;
			static bool inited = false;
			if (u8skippedLoops++ > 200) {
				if (_s_smtbty_callBacks.pfnChgDisable)
				_s_smtbty_callBacks.pfnChgDisable();
				u8skippedLoops = 0;
			}

			if (!inited) {
				/* Update the battery connect MMIO */
				gs_eSpiMmioSpace[ACPI_CHARGER_STATUS] &= ~ACPI_BATTERY_CONNECTED;
				pBat->btyFlag.f.connected = 0;
				/* Notify OS for battery status */
				ACPI_NotifyHost(ACPI_SCI_BATTERY);
				inited = true;
			}

			continue;
		}

		/* refresh the battery data */
		/* 01. common data */
		for (uint8_t i = 0; _s_batBuf[btyId][i].reg != DEV_SMTBTY_REG_INVALID; i++) {
			DEV_SMTBTY_REG_ITEM * pItem = &_s_batBuf[btyId][i];

			if (pItem->access.f.autoRefersh && pItem->u32refreshCounter >= APP_SMTBTY_TICK_INTERVAL) {
				pItem->u32refreshCounter -= APP_SMTBTY_TICK_INTERVAL;
			}

			if ( pItem->u32refreshCounter < APP_SMTBTY_TICK_INTERVAL && pItem->access.f.autoRefersh ) {
				if (!dev_smtbty_autoRead(pItem, DEV_SMTBTY_ID_TO_PORT(btyId), BATTERY_I2C_ADDRESS)) {
					pBat->u32ErrCnt ++;
				}
			}
		}
		/* 02. platform special data */
		if (_s_platform_btyBufTable[btyId]) {
			for (uint8_t i = 0; _s_platform_btyBufTable[btyId][i].reg != DEV_SMTBTY_REG_INVALID; i++) {
				DEV_SMTBTY_REG_ITEM * pItem = &_s_platform_btyBufTable[btyId][i];

				if (pItem->access.f.autoRefersh && pItem->u32refreshCounter >= APP_SMTBTY_TICK_INTERVAL) {
					pItem->u32refreshCounter -= APP_SMTBTY_TICK_INTERVAL;
				}

				if ( pItem->u32refreshCounter < APP_SMTBTY_TICK_INTERVAL && pItem->access.f.autoRefersh ) {
					if (!dev_smtbty_autoRead(pItem, DEV_SMTBTY_ID_TO_PORT(btyId), BATTERY_I2C_ADDRESS)) {
						pBat->u32ErrCnt ++;
					}
				}
			}
		}

		/* call special refresh routine if defined */
		if(_s_smtbty_callBacks.pfnRefresh)
			_s_smtbty_callBacks.pfnRefresh(btyId);

		/* refresh the charger data */
		for (uint8_t chgId = 0; chgId < MAX_CHARGER_SUPPORTED; chgId ++) {
			/* 01. common data */
			for (uint8_t i = 0; _s_chgBuf[chgId][i].reg != DEV_CHARGER_REG_INVALID; i ++) {
				pItem = &_s_chgBuf[chgId][i];

				if (pItem->access.f.autoRefersh && pItem->u32refreshCounter >= APP_SMTBTY_TICK_INTERVAL) {
					pItem->u32refreshCounter -= APP_SMTBTY_TICK_INTERVAL;
				}

				if ( pItem->u32refreshCounter < APP_SMTBTY_TICK_INTERVAL && pItem->access.f.autoRefersh ) {
#if CONFIG_CHARGER_ISL9241
					dev_isl9241_autoRead(pItem, DEV_CHARGER_ID_TO_PORT(btyId), CHARGER_I2C_ADDRESS);
#elif CONFIG_CHARGER_MP2764
					amd_crb_drivers_mp2764_autoRead(pItem, DEV_CHARGER_ID_TO_PORT(btyId), CHARGER_I2C_ADDRESS);
#endif
				}
			}
			/* 02. platform special data */
			if (_s_platform_chgBufTable[chgId]) {
				for (uint8_t i = 0; _s_platform_chgBufTable[chgId][i].reg != DEV_CHARGER_REG_INVALID; i++) {
					pItem = &_s_platform_chgBufTable[chgId][i];

					if (pItem->access.f.autoRefersh && pItem->u32refreshCounter >= APP_SMTBTY_TICK_INTERVAL) {
						pItem->u32refreshCounter -= APP_SMTBTY_TICK_INTERVAL;
					}

					if ( pItem->u32refreshCounter < APP_SMTBTY_TICK_INTERVAL && pItem->access.f.autoRefersh ) {
#if CONFIG_CHARGER_ISL9241
						dev_isl9241_autoRead(pItem, DEV_CHARGER_ID_TO_PORT(btyId), CHARGER_I2C_ADDRESS);
#elif CONFIG_CHARGER_MP2764
						amd_crb_drivers_mp2764_autoRead(pItem, DEV_CHARGER_ID_TO_PORT(btyId), CHARGER_I2C_ADDRESS);
#endif
					}
				}
			}
		}

		/* do normal charge */
		bool chgSt = _s_smtbty_callBacks.pfnCharger(btyId);

		APP_BATTERY_FLAG btyFlag = app_battery_flagsCalculate(btyId);
		if (pBat->btyFlag.u32flag != btyFlag.u32flag) {
			LOG_INF("Update btyFlags %d, 0x%08X => 0x%08X", btyId, pBat->btyFlag.u32flag, btyFlag.u32flag);

			pBat->btyFlag.u32flag = btyFlag.u32flag;
		}

		/* call special status update routine if defined */
		bool isBtyStUpdated = false;
		if(_s_smtbty_callBacks.pfnStatus)
			isBtyStUpdated = _s_smtbty_callBacks.pfnStatus(btyId);

		/* notify the client */
		uint32_t alartFlags = pBat->btyLastFlag.u32flag ^ pBat->btyFlag.u32flag;
		if (chgSt || alartFlags || isBtyStUpdated) {
			if(_s_smtbty_callBacks.pfnNotify)
				_s_smtbty_callBacks.pfnNotify(btyId, alartFlags);

			pBat->btyLastFlag.u32flag = pBat->btyFlag.u32flag;
		}

		/* post processing */
		if (_s_smtbty_callBacks.pfnPostProcessing)
			_s_smtbty_callBacks.pfnPostProcessing(btyId);

		if (bat_refresh) {
			bat_refresh = false;
			/* Notify OS for battery status */
			ACPI_NotifyHost(ACPI_SCI_BATTERY);
		}
	}
}

/********************************/
/*        SmtBty debug          */
/********************************/
#if (APP_SMTBTY_DEBUG_ENABLE)

/**
 * @brief Smart battery debug timer callback
 *
 * @param timer pointer to the timer structure
 */
static void app_smtbty_dbgCallback(struct k_timer *timer)
{
	if (g_smtbty_dbg_flag.f.isEnabled) {
		if (g_smtbty_dbg_vector.u32remainExecuteTimeInSeconds)
			g_smtbty_dbg_vector.u32remainExecuteTimeInSeconds --;

		dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_AbsoluteStateOfCharge, &g_smtbty_dbg_vector.u16absoluteStateOfCharge);
		dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_RelativeStateOfCharge, &g_smtbty_dbg_vector.u16relativeStateOfCharge);
		dev_smtbty_i32CacheGet(app_battery_selectedGet(), DEV_SMTBTY_REG_BatteryStatus,         &g_smtbty_dbg_vector.u16batteryStatus);
	}
}

/**
 * @brief Smart battery debug init
 */
void app_smtbty_dbgInit(void)
{
	g_smtbty_dbg_flag.u8flgs = 1;

	memset(&g_smtbty_dbg_vector, 0xFF, sizeof(APP_SMTBTY_DBG_VECTOR));
	g_smtbty_dbg_vector.u16voltageHighLimit = APP_BATTERY_FULL_CHARGE_VOLTAGE;
	g_smtbty_dbg_vector.u16voltageLowLimit  = 0;
	g_smtbty_dbg_vector.u16currentHighLimit = 0xFCCC;
	g_smtbty_dbg_vector.u16currentLowLimit  = 0;
	g_smtbty_dbg_vector.u32remainExecuteTimeInSeconds = 0xFCCC0000;
	g_smtbty_dbg_vector.u16batteryMaxChargeCurrent = APP_BATTERY_MAX_CHARGE_CURRENT;
}

/**
 * @brief app smart battery debug ACPI handler
 *
 * @param isRead       write permission.
 * @param ui8Idx       acpi offset
 * @param pui8Data     pointer to buffer holding the data.
 */
uint8_t app_smtbty_dbgHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx == 0x30) {
		if (isRead) {
			*pui8Data = g_smtbty_dbg_flag.u8flgs;
		} else {
			APP_SMTBTY_DBG_FLAG tmp = { *pui8Data };

			if (!tmp.f.isEnabled) {
				k_timer_stop(&_s_smtbtyDbgTimerId);
			} else if (!g_smtbty_dbg_flag.f.isEnabled) {
				if (g_smtbty_dbg_vector.u16currentLowLimit > g_smtbty_dbg_vector.u16currentHighLimit)
					g_smtbty_dbg_vector.u16currentLowLimit = g_smtbty_dbg_vector.u16currentHighLimit;

				if (g_smtbty_dbg_vector.u16voltageLowLimit > g_smtbty_dbg_vector.u16voltageHighLimit)
					g_smtbty_dbg_vector.u16voltageLowLimit = g_smtbty_dbg_vector.u16voltageHighLimit;

				k_timer_start(&_s_smtbtyDbgTimerId, K_MSEC(1000), K_MSEC(1000));
			}

			g_smtbty_dbg_flag.u8flgs = *pui8Data;
		}

		return 1;
	}

	if (ui8Idx < APP_SMTBTY_DEBUG_BASE || ui8Idx >= APP_SMTBTY_DEBUG_BASE + sizeof(APP_SMTBTY_DBG_VECTOR))
		return 0;

	ui8Idx -= APP_SMTBTY_DEBUG_BASE;
	if (isRead) {
		switch (ui8Idx) {
			default:
				*pui8Data = *(((uint8_t *)&g_smtbty_dbg_vector) + ui8Idx);
				break;
		}
	} else if (!g_smtbty_dbg_flag.f.isEnabled) {
		uint8_t tmp = *pui8Data;
		switch (ui8Idx) {
			/* writeable fields */
			case  4: case  5:                    // u16voltageLowLimit
			case  6: case  7:                    // u16voltageHighLimit
			case 12: case 13:                    // u16currentLowLimit
			case 14: case 15:                    // u16currentHighLimit
			case 16: case 17: case 18: case 19:  // u32remainExecuteTimeInSeconds
				*(((uint8_t *)&g_smtbty_dbg_vector) + ui8Idx) = tmp;
				break;
			default:
				break;
		}
	}

	return 1;
}

#endif  // end of APP_SMTBTY_DEBUG_ENABLE

/**
 * @brief Routine that handles smart battery application.
 *
 * Must call after app_smtbty_taskInit.
 *
 * @param p1 thread run period.
 * @param p2 thread sleep ready.
 * @param p3 reserved.
 */
void batterymgmt_thread(void *p1, void *p2, void *p3)
{
	uint32_t period = *(uint32_t *)p1;

	task_smtbtySlpReady = (uint8_t*) p2;

	while (true) {
		k_msleep(period);

		/* Disable it, prio to the entry of the smart battery services. since it has thread yield use case. */
		task_status_set(task_smtbtySlpReady, TASK_STASTUS_SLEEP_READY, 0);

		if ((app_pseq_systemState() > SYSTEM_G3_STATE) && (g_batteryIsReadyToGo)) {
			app_smtbty_proc();
			board_smtbty_breath_led();
		}
#if CONFIG_ACDCSWITCH
		if (app_acDcSwitchHandler()){
			task_status_set(task_smtbtySlpReady, TASK_STASTUS_SLEEP_READY, 1);
		} else
#endif		 
		{
			task_status_set(task_smtbtySlpReady, TASK_STASTUS_SLEEP_READY, 0);
		}
	}
}