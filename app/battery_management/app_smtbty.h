/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_SMTBTY_H__
#define __APP_SMTBTY_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include "dev_smtbty.h"

#if (MAX_BATTERY_SUPPORTED > 32)
#error "Max battery number 32 !"
#endif

#ifndef APP_SMTBTY_TICK_INTERVAL
#define APP_SMTBTY_TICK_INTERVAL          500  // in (ms); 0.5s
#endif

#ifndef APP_BATTERY_PRESENT_RECHECK_DELAY
#define APP_BATTERY_PRESENT_RECHECK_DELAY 5000 // in (us); 5ms
#endif

#ifndef APP_BATCHG_SELECTOR_I2C_ADDRESS
#define APP_BATCHG_SELECTOR_I2C_ADDRESS  0x0A  // Smart Battery System Manager or Smart Battery Selector
#endif

typedef enum {
    APP_CHG_DISABLE = 0,
    APP_CHG_REACTIVATING,
    APP_CHG_PRE_CHARGE,
    APP_CHG_ENABLED_NORMAL,
    APP_CHG_BATTERY_IS_DEAD
} APP_SMTBTY_CHG_STATUS;

typedef enum {
	APP_BATTERY_NOT_CONNECTED          = 0x0001,
	APP_BATTERY_IDLE                   = 0x0002,

	APP_BATTERY_IS_DEAD                = 0x0004,
	APP_BATTERY_REACTIVATING           = 0x0008,
	APP_BATTERY_PRE_CHARGE             = 0x0010,

	APP_BATTERY_CHARGING               = 0x0020,
	APP_BATTERY_DISCHARGING            = 0x0040,
	APP_BATTERY_FULL_CHARGED           = 0x0080,
	APP_BATTERY_FULL_DISCHARGED        = 0x0100,

	APP_BATTERY_INVALID_SIGNAL         = 0xFFFF,
} APP_SMTBTY_BTY_STATUS;

typedef struct
{
    /**
     * To notify the application that battery status has changed.
     */
    void (*pfnNotify)(uint8_t u8BtyId, uint32_t u32Status);

    /**
     * A callback function to detect if battery is present or not.
     */
    bool (*pfnPresent)(uint8_t u8BtyId);

    /**
     * A callback function to detect if AC is present or not.
     */
    bool (*pfnIsAcPresent)();

    /**
     * A callback function used to allow the battery client to provide
        * additional refresh processing for the battery.  When a battery refresh
        * condition is detected (connect state change, charge state change, or
        * battery polling timeout) the battery cache will be refreshed in the
        * battery module.  If there are additional client operations that should
        * also be performed, they may take place at this time by enabling this
        * callback handle.  If no additional operations are needed, this callback
        * handle may be left NULL.
     */
    void (*pfnRefresh)(uint8_t u8BtyId);

    /**
     * Pre & Post handling
     */
    void (*pfnPreProcessing)(uint8_t u8BtyId);
    void (*pfnPostProcessing)(uint8_t u8BtyId);

    /**
     * A callback for Charger Enable/Disable
     */
    bool (*pfnChgEnble)();
    bool (*pfnChgDisable)();

    /**
        * A callback function used to allow the battery client to provide
        * a custom charging routine.  If the battery module's default charging
        * algorithm is sufficient, this callback handle may be left NULL.  This
        * callback handle must return a value of true if the state of battery
        * charging has changed (that is from not charging to charging).  This
        * will trigger a refresh of cached data and status.
     */
    bool (*pfnCharger)(uint8_t u8BtyId);

    /**
     * A callback function used to allow the battery client to provide
     * battery charge current limit base on the power source.
     * It's called by the default doCharge() function.
     */
    uint16_t (*pfnGetChargeCurrentLimit)(uint8_t u8BtyId);

    /**
        * A callback function used to allow the battery client to provide
        * additional status update for the battery.  If no additional status
        * updates are needed, this callback handle may be left NULL.  This
        * function should return \b true if the battery should be flaged for
        * as needing an alert/notification.
     */
    bool (*pfnStatus)(uint8_t u8BtyId);

    /**
     * A callback function used to allow the platform knowns the battery
     * status changes so as to specialize LED signals
     */
    void (*pfnBatteryLed)(APP_SMTBTY_BTY_STATUS s);

    /**
     * Callback functions used to notify the client that battery is just
     * connected or removed.
     */
    void (*pfnOnBatteryAttach)(uint8_t u8BtyId);
    void (*pfnOnBatteryDetach)(uint8_t u8BtyId);
} APP_SMTBTY_CLIENT_ENTITY;


/* Smart battery spec defined status (0x16) */
/** * * * * * * Alarm Bits * * * * *
 * 0x8000 OVER_CHARGED_ALARM
 * 0x4000 TERMINATE_CHARGE_ALARM
 * 0x2000 Reserved
 * 0x1000 OVER_TEMP_ALARM
 * 0x0800 TERMINATE_DISCHARGE_ALARM
 * 0x0400 Reserved
 * 0x0200 REMAINING_CAPACITY_ALARM
 * 0x0100 REMAINING_TIME_ALARM
   * * * * * * Status Bits * * * * *
 * 0x0080 INITIALIZED
 * 0x0040 DISCHARGING
 * 0x0020 FULLY_CHARGED
 * 0x0010 FULLY_DISCHARGED
   * * * * * * Error Code * * * * *
 * 0x0000-0x000f Reserved for error codes - See Appendix C
 */

typedef enum {

	APP_BATTERY_OK = 0,
	APP_BATTERY_BUSY,
	APP_BATTERY_RESERVED,
	APP_BATTERY_UNSUPPORTED_CMD,
	APP_BATTERY_ACCESS_DENIED,
	APP_BATTERY_OVERFLOW,
	APP_BATTERY_BAD_SIZE,
	APP_BATTERY_UNKNOWN_ERROR,
    
} APP_BATTERY_ERROR_CODE;

typedef union {
    uint16_t u16Status;
    struct {
        APP_BATTERY_ERROR_CODE errCode   : 4;

        uint16_t fullyDischarged         : 1;
        uint16_t fullyCharged            : 1;
        uint16_t discharging             : 1;
        uint16_t initialized             : 1;

        uint16_t remainingTimeAlarm      : 1;
        uint16_t remainingCapacityAlarm  : 1;
        uint16_t                         : 1;
        uint16_t terminateDischargeAlarm : 1;

        uint16_t overTempAlarm           : 1;
        uint16_t                         : 1;
        uint16_t terminateChargeAlarm    : 1;
        uint16_t overChargedAlarm        : 1;
    } f;
} APP_BATTERY_STATUS;

/* Smart battery notification flags */
#define APP_SMTBTY_CONNECTED    (0x00000001)
#define APP_SMTBTY_WARNING      (0x00000002)
#define APP_SMTBTY_LOW          (0x00000004)
#define APP_SMTBTY_CRITICAL     (0x00000008)
#define APP_SMTBTY_IDLE         (0x00000010)
#define APP_SMTBTY_CHARGING     (0x00000020)
#define APP_SMTBTY_DISCHARGING  (0x00000040)
#define APP_SMTBTY_TRIPPED      (0x00000080)
#define APP_SMTBTY_DEAD         (0x00000100)

typedef union {
    uint32_t u32flag;
    struct {
        uint32_t connected   : 1;
        uint32_t warning     : 1;
        uint32_t low         : 1;
        uint32_t critical    : 1;
        uint32_t idle        : 1;
        uint32_t charging    : 1;
        uint32_t discharging : 1;
        uint32_t tripped     : 1;
        uint32_t dead        : 1;
        uint32_t             : 23;
    } f;
} APP_BATTERY_FLAG;

#define APP_SMTBTY_CHG_THROTTLE_DISABLED     (0xFF)

typedef struct {
	APP_BATTERY_FLAG btyFlag;

    /* set by user */
    uint16_t u16ThresholdBtyWarning;               // default 1/5 of remain capacity
    uint16_t u16ThresholdBtyLow;                   // default 1/10 of remain capacity
    uint16_t u16ThresholdBtyCritical;              // default 1/25 of remain capacity
    uint16_t u16ThresholdBtyTripPoint;

    uint8_t u8ChargeThrottle;                      // _DSM F1 charge throttle (0 to 100)

    uint16_t u16LastRemainingCapicity;
    APP_BATTERY_FLAG btyLastFlag;
    uint32_t u32ErrCnt;
} APP_BATTERY_INSTANCE;

#if (APP_SMTBTY_DEBUG_ENABLE)
#ifndef APP_SMTBTY_DEBUG_BASE
#define APP_SMTBTY_DEBUG_BASE   0
#endif

typedef struct {
    uint16_t u16requiredVoltage;
    uint16_t u16appliedVoltage;
    uint16_t u16voltageLowLimit;            // RW
    uint16_t u16voltageHighLimit;           // RW

    uint16_t u16requiredCurrent;
    uint16_t u16appliedCurrent;
    uint16_t u16currentLowLimit;            // RW 0x0C-0x0D low limit of charge rate
    uint16_t u16currentHighLimit;           // RW 0x0E-0x0F high limit of charge rate

    uint32_t u32remainExecuteTimeInSeconds; // RW

    uint16_t u16chargerOption;
    uint16_t u16chargerStatus;

    uint16_t u16batteryStatus;              // 0x16 F_BAT_REG_BatteryStatus
    uint16_t u16relativeStateOfCharge;      // 0x0D F_BAT_REG_RelativeStateOfCharge or Percentage
    uint16_t u16absoluteStateOfCharge;      // 0x0E F_BAT_REG_AbsoluteStateOfCharge
    uint16_t u16batteryMaxChargeCurrent;    // Battey spec define
} APP_SMTBTY_DBG_VECTOR;

typedef union {
    uint8_t u8flgs;
    struct {
        uint8_t isEnabled     : 1;
        uint8_t               : 1;
        uint8_t forceChargeEn : 1;     // no matter there's battery be connected or it's full or not.
    } f;
} APP_SMTBTY_DBG_FLAG;
#endif /* APP_SMTBTY_DEBUG_ENABLE */

/** *********************************************
 * app_smtbty initialization process
 *    01. app_smtbty_init() should be called first
 *        before other smtbty functions can be
 *        called.
 *    02. Register platform special Battery or
 *        Charger table if any.
 *        (Or devices initial functions, for 
 *         example dev_bq24780s_init(). )
 *    03. Set callbacks through the pointer that 
 *        obtains from app_smtbty_getClientEntity().
 *    04. Call to app_smtbty_start() before goto
 *        the main loop.
 *
 *    Note that the register tables should not
 *    change after app_smtbty_start() is called.
 * *********************************************/

/**
 * @brief Allocate memory for some smart battery corresponding structures.
 * 
 * should be called first before other smtbty functions can be called.
 */
void app_smtbty_init(void);

/**
 * @brief Register table for battery.
 *
 * @param btyId Index of the battery.
 * @param pTable battery tabble pointer.
 */
void app_register_platform_battery_table(uint8_t btyId, DEV_SMTBTY_REG_ITEM * pTable);

/**
 * @brief Get board smart battery operation pointer.
 *
 * @retval board smart battery operation pointer.
 */
APP_SMTBTY_CLIENT_ENTITY * app_smtbty_getClientEntity (void);

/**
 * @brief Call to app_smtbty_start() before goto the main loop.
 * 
 * Note that the register tables should not
 * change after app_smtbty_start() is called.
 */
void app_smtbty_start(void);

/**
 * @brief Get board battery number.
 *
 * @retval board battery number.
 */
uint8_t app_battery_countGet(void);

/**
 * @brief Get selected battery index.
 *
 * @retval Selected battery index.
 */
uint8_t app_battery_selectedGet(void);

/**
 * @brief Set selected battery index.
 *
 * @param btyId Index of the battery.
 */
void app_battery_selectedSet(uint8_t btyId);

/**
 * @brief Get the battery warning threshold.
 *
 * @param btyId Index of the battery.
 *
 * @retval Warning threshold read from battery register.
 */
uint16_t app_battery_warningThresholdGet(uint8_t btyId);

/**
 * @brief Set the battery warning threshold.
 *
 * @param btyId Index of the battery.
 * @param u16threshold Target warning threshold.
 */
void app_battery_warningThresholdSet(uint8_t btyId, uint16_t u16threshold);

/**
 * @brief Get the battery low threshold.
 *
 * @param btyId Index of the battery.
 *
 * @retval Battery low threshold read from battery register.
 */
uint16_t app_battery_lowThresholdGet(uint8_t btyId);

/**
 * @brief Set the battery low threshold.
 *
 * @param btyId Index of the battery.
 * @param u16threshold Target battery low threshold.
 */
void app_battery_lowThresholdSet(uint8_t btyId, uint16_t u16threshold);

/**
 * @brief Get the battery critical threshold.
 *
 * @param btyId Index of the battery.
 *
 * @retval Battery critical threshold read from battery register.
 */
uint16_t app_battery_criticalThresholdGet(uint8_t btyId);

/**
 * @brief Set the battery critical threshold.
 *
 * @param btyId Index of the battery.
 * @param u16threshold Target battery critical threshold.
 */
void app_battery_criticalThresholdSet(uint8_t btyId, uint16_t u16threshold);

/**
 * @brief Get the battery trip point.
 *
 * @param btyId Index of the battery.
 *
 * @retval Battery trip point read from battery register.
 */
uint16_t app_battery_tripPointGet(uint8_t btyId);

/**
 * @brief Set the battery trip point.
 *
 * @param btyId Index of the battery.
 * @param u16tripPoint Target battery trip point.
 */
void app_battery_tripPointSet(uint8_t btyId, uint16_t u16tripPoint);

/**
 * @brief Get the battery charge throttle.
 *
 * @param btyId Index of the battery.
 *
 * @retval Battery charge throttle read from battery register.
 */
uint8_t app_battery_ChargeThrottleGet(uint8_t btyId);

/**
 * @brief Set the battery charge throttle.
 *
 * @param btyId Index of the battery.
 * @param u8Throttle Target battery charge throttle.
 */
void app_battery_ChargeThrottleSet(uint8_t btyId, uint8_t u8Throttle);

/**
 * @brief claculate the battery status flag and return.
 *
 * @param btyId Index of the battery.
 *
 * @retval Battery status flag.
 */
APP_BATTERY_FLAG app_battery_flagsCalculate(uint8_t btyId);

/**
 * @brief Get the battery status flag from cache.
 *
 * @param btyId Index of the battery.
 *
 * @retval Battery status flag.
 */
APP_BATTERY_FLAG app_battery_flagsCacheGet(uint8_t btyId);

/**
 * @brief Clear the battery tripped flag.
 *
 * @param btyId Index of the battery.
 */
void app_battery_clearTrippedFlag(uint8_t btyId);

/**
 * @brief Routine that handles smart battery application.
 * 
 * Must call after app_smtbty_taskInit.
 *
 * @param p1 thread run period.
 * @param p2 thread sleep ready.
 * @param p3 reserved.
 */
void batterymgmt_thread(void *p1, void *p2, void *p3);

/**
 * @brief Smart battery main function. Loop in batterymgmt_thread.
 */
void app_smtbty_proc (void);

#if (APP_SMTBTY_DEBUG_ENABLE)
/**
 * @brief Smart battery debug init
 *
 */
void app_smtbty_dbgInit( void );

/**
 * @brief app smart battery debug ACPI handler
 *
 * @param isRead       write permission.
 * @param ui8Idx       acpi offset
 * @param pui8Data     pointer to buffer holding the data.
 */
uint8_t app_smtbty_dbgHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

/**
 * @brief Temporarily disable battery access.
 */
void app_smtbty_frozen(void);

/**
 * @brief Enable battery access.
 */
void app_smtbty_unfrozen(void);

#endif

#endif // end of __APP_SMTBTY_H__

