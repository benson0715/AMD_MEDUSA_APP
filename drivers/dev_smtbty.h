/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_SMTBTY_H__
#define __DEV_SMTBTY_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include "board_config.h"

#ifndef MAX_BATTERY_SUPPORTED
#define MAX_BATTERY_SUPPORTED  1
#endif

typedef enum {

    DEV_SMTBTY_REG_ManufacturerAccess                  = 0x00, //
    DEV_SMTBTY_REG_RemainingCapacityAlarm              = 0x01, // Low Capacity alarm threshold value.
    DEV_SMTBTY_REG_RemainingTimeAlarm                  = 0x02, // Remaining Time alarm value.
    DEV_SMTBTY_REG_BatteryMode                         = 0x03, // battery operational modes
    DEV_SMTBTY_REG_AtRate                              = 0x04, //
    DEV_SMTBTY_REG_AtRateTimeToFull                    = 0x05, //
    DEV_SMTBTY_REG_AtRateTimeToEmpty                   = 0x06, //
    DEV_SMTBTY_REG_AtRateOK                            = 0x07, //
    DEV_SMTBTY_REG_Temperature                         = 0x08, // Get cell temperature (word 0.1 degK)
    DEV_SMTBTY_REG_Voltage                             = 0x09, // Get cell pack voltage (word mV)
    DEV_SMTBTY_REG_Current                             = 0x0A, // Get cell pack current (word mA)
    DEV_SMTBTY_REG_AverageCurrent                      = 0x0B, // Get cell avgk current (word mA)
    DEV_SMTBTY_REG_MaxError                            = 0x0C, // Get expected margin of error in charge calculation (%)
    DEV_SMTBTY_REG_RelativeStateOfCharge               = 0x0D, // Get cell relative state of charge
    DEV_SMTBTY_REG_AbsoluteStateOfCharge               = 0x0E, //
    DEV_SMTBTY_REG_RemainingCapacity                   = 0x0F, // Get remaining capacity (word mAh)
    DEV_SMTBTY_REG_FullChargeCapacity                  = 0x10, // Get full charge capacity (word mAh)
    DEV_SMTBTY_REG_RunTimeToEmpty                      = 0x11, // Runtime left (word minutes)
    DEV_SMTBTY_REG_AverageTimeToEmpty                  = 0x12, // Avg time left (word minutes)
    DEV_SMTBTY_REG_AverageTimeToFull                   = 0x13, // Avg time to charge (word minutes)
    DEV_SMTBTY_REG_ChargingCurrent                     = 0x14, // Requested charge current (word mA)
    DEV_SMTBTY_REG_ChargingVoltage                     = 0x15, // Requested charge current (word mV)
    DEV_SMTBTY_REG_BatteryStatus                       = 0x16, // Battery status flags (word)
    DEV_SMTBTY_REG_CycleCount                          = 0x17, // # of cycles the battery has experienced
    DEV_SMTBTY_REG_DesignCapacity                      = 0x18, // Designed capacity (word mAh)
    DEV_SMTBTY_REG_DesignVoltage                       = 0x19, // Designed voltage (word mV)
    DEV_SMTBTY_REG_SpecificationInfo                   = 0x1A, // Designed voltage (word mV)
    DEV_SMTBTY_REG_ManufactureDate                     = 0x1B, // Manufacture date
    DEV_SMTBTY_REG_SerialNumber                        = 0x1C, // Serial number
    DEV_SMTBTY_REG_ManufacturerName                    = 0x20, // Manufacturer name
    DEV_SMTBTY_REG_DeviceName                          = 0x21, // Device name
    DEV_SMTBTY_REG_DeviceChemistry                     = 0x22, // Device chemistry (4 character string)
    DEV_SMTBTY_REG_ManufacturerData                    = 0x23, // Manufacturer proprietary data
    DEV_SMTBTY_REG_OptionalMfgFunction5                = 0x2F, //
    DEV_SMTBTY_REG_OptionalMfgFunction4                = 0x3C, //
    DEV_SMTBTY_REG_OptionalMfgFunction3                = 0x3D, //
    DEV_SMTBTY_REG_OptionalMfgFunction2                = 0x3E, //
    DEV_SMTBTY_REG_OptionalMfgFunction1                = 0x3F, //

} DEV_SMTBTY_REG;

#define DEV_SMTBTY_REG_INVALID 0x9A                        // exclusive with the registers defined for battery or charger

#ifndef BATTERY_I2C_ADDRESS
#define BATTERY_I2C_ADDRESS          0x0B  // Smart Battery
#endif

#ifndef DEV_SMTBTY_ID_TO_PORT
#define DEV_SMTBTY_ID_TO_PORT(btyId)    (0)   // Always map to port 0 by default
#endif

#ifndef DEV_SMTBTY_SWITCH_TO_PORT
#define DEV_SMTBTY_SWITCH_TO_PORT(port)         while(0){} // do nothing by default
#endif

typedef struct {
    uint8_t slaveAddr;
    uint32_t continuousErrCount;
    uint32_t maxContinuousErrCount;
} DEV_SMTBTY_I2C_INTERFACE_ERROR_COUNT;

extern DEV_SMTBTY_I2C_INTERFACE_ERROR_COUNT _s_smtbty_i2c_bus_errCount[5];

typedef enum {
    DEV_SMTBTY_RSV = 0,
    DEV_SMTBTY_RO,
    DEV_SMTBTY_WO,
    DEV_SMTBTY_RW
} DEV_SMTBTY_REG_ACCESS_TYPE;

#define DEV_SMTBTY_REG_DISABLED     0
#define DEV_SMTBTY_REG_READ_ONLY    1
#define DEV_SMTBTY_REG_WRITE_ONLY   2
#define DEV_SMTBTY_REG_READ_WRITE   3

typedef struct {
    uint8_t      width;
    union {
        uint32_t u32;
        uint16_t u16[2];
        uint8_t  u8[4];
        uint8_t* pBuf;
    } value;

    union {
        uint32_t u32;
        uint16_t u16[2];
        uint8_t  u8[4];
    } lastSuccessValue;
} DEV_SMTBTY_REG_DATA;

#define DEV_SMTBTY_RATIO_APP_1_REG_1     (0)   // 1:1
#define DEV_SMTBTY_RATIO_APP_1_REG_0P5   (1)   // 1:2
#define DEV_SMTBTY_RATIO_APP_0P5_REG_1   (2)   // 2:1
#define DEV_SMTBTY_RATIO_APP_1_REG_0P25  (3)   // 1:4

typedef struct {

    uint8_t           reg;

    /* value and accessing lock */
    DEV_SMTBTY_REG_DATA data;

    /* Polling intrval */
    uint32_t          u32refreshReload;
    uint32_t          u32refreshCounter;

    /* Control attribute */
    union {
        uint8_t       u8Ctrl;
        struct {
            uint8_t                : 1;
            uint8_t   autoRefersh  : 1;
            uint8_t   dataAvailable: 1;
            uint8_t   reserved_PEC : 1;       // PEC is not supportted right now
            uint8_t   ratio        : 2;       // ratio Reg:App;
                                              // 2'b00 - 1:1
                                              // 2'b01 - 1:2 or DEV_SMTBTY_RATIO_APP_1_REG_0P5
                                              // 2'b10 - 2:1 or DEV_SMTBTY_RATIO_APP_0P5_REG_1
                                              // 2'b11 - 1:4 or DEV_SMTBTY_RATIO_APP_1_REG_0P25
            uint8_t   accessType   : 2;       // 0 - REV; 1 - RO; 2 - WO; 3 - RW
        } f;
    } access;
    struct k_mutex  rwLock;
} DEV_SMTBTY_REG_ITEM;

extern DEV_SMTBTY_REG_ITEM * _s_platform_btyBufTable[MAX_BATTERY_SUPPORTED];
extern DEV_SMTBTY_REG_ITEM * _s_batBuf[MAX_BATTERY_SUPPORTED];
/* ****************************************************************
 *
 * Thread-safe functions. Non-reenterable !!
 *
 * bool dev_smtbty_i32CacheGet(uint8_t btyId, uint8_t reg, void * pVal);
 * bool dev_smtbty_blockCacheGet(uint8_t btyId, uint8_t reg, void * pBuf, uint8_t size);
 * bool dev_smtbty_i32Read(uint8_t btyId, uint8_t reg, void * pVal);
 * bool dev_smtbty_i32Write(uint8_t btyId, uint8_t reg, int32_t val);
 *
 * ****************************************************************/

/**
 * @brief Get battery register value from cache.
 *
 * @param btyId Index of the battery.
 * @param reg Battery reg.
 * @param pVal Address of the variable to save value read from cache.
 *
 * @retval true is successful.
 */
bool dev_smtbty_i32CacheGet(uint8_t btyId, uint8_t reg, void * pVal);

/**
 * @brief Get battery register value from cache.
 *
 * @param btyId Index of the battery.
 * @param reg Battery reg.
 * @param pBuf Address of the variable to save value read from cache.
 * @param size Block cache size.
 *
 * @retval true is successful.
 */
bool dev_smtbty_blockCacheGet(uint8_t btyId, uint8_t reg, void * pBuf, uint8_t size);

/**
 * @brief Get battery register value from battery reg.
 *
 * @param chgId Index of the battery.
 * @param reg Battery reg.
 * @param pVal Address of the variable to save value read from cache.
 *
 * @retval true is success.
 */
bool dev_smtbty_i32Read(uint8_t btyId, uint8_t reg, void * pVal);

/**
 * @brief Write value to battery register.
 *
 * @param chgId Index of the battery.
 * @param reg Battery reg.
 * @param pVal Target value.
 *
 * @retval true is success.
 */
bool dev_smtbty_i32Write(uint8_t btyId, uint8_t reg, int32_t val);

/**
 * @brief Get block battery register value from register.
 *
 * @param btyId Index of the battery.
 * @param reg Battery reg.
 * @param pBuf Address of the variable to save value read from register.
 * @param size Block size.
 *
 * @retval true is successful.
 */
bool dev_smtbty_blockRead(uint8_t btyId, uint8_t reg, void * pVal, uint8_t size);

/**
 * @brief Read battery reg item from specific i2c port and address.
 *
 * @param pItem Battery reg item.
 * @param port Battery i2c port.
 * @param slaveAddr Battery i2c slave address.
 * 
 * @retval false if fail.
 * @retval dataAvailable flag.
 */
bool dev_smtbty_autoRead(DEV_SMTBTY_REG_ITEM * pItem, uint8_t port, uint8_t slaveAddr);

/**
 * @brief Find reg from platform and default reg table.
 *
 * @param btyId Index of battery.
 * @param reg reg to find.
 *
 * @retval reg item find in table or NULL when not fonud.
 */
DEV_SMTBTY_REG_ITEM * dev_smtbty_findRegItem(uint8_t btyId, uint8_t reg);

/**
 * @brief Set reserved battery capacity.
 *
 * @param u16Rsvd Target reserved battery capacity.
 */
void dev_smtbty_setRsvdBtyCapacity( uint16_t u16Rsvd );

/**
 * @brief Get reserved battery capacity.
 *
 * @retval Reserved battery capacity.
 */
uint16_t dev_smtbty_getRsvdBtyCapacity( void );

/**
 * @brief Update Bus Error Counter.
 *
 * @param slaveAddr Battery address.
 * @param wasSuccess Success status.
 */
void _dev_smtbty_updateBusErrorCounter(uint8_t slaveAddr, bool wasSuccess);

/**
 * @brief Reset Bus Error Counter.
 *
 * @param slaveAddr Battery address.
 */
void dev_smtbty_resetBusErrorCounter(uint8_t slaveAddr);

/**
 * @brief Get Continuous Error Num.
 *
 * @param slaveAddr Battery address.
 * 
 * @retval Continuous Error Num.
 */
uint32_t dev_smtbty_getContinuousErrorNum(uint8_t slaveAddr);
#endif // end of __DEV_SMTBTY_H__

