/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_CHARGER_H__
#define __DEV_CHARGER_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include "board_config.h"

#ifndef MAX_CHARGER_SUPPORTED
#define MAX_CHARGER_SUPPORTED  1
#endif

#ifndef DEV_CHARGER_SWITCH_TO_PORT
#define DEV_CHARGER_SWITCH_TO_PORT(port)         while(0){} // do nothing by default
#endif

#define DEV_CHARGER_REG_INVALID 0x9A                        // exclusive with the registers defined for battery or charger

/*
 * Smart battery
 */
#ifndef CHARGER_I2C_ADDRESS
#define CHARGER_I2C_ADDRESS          0x09  // Smart Battery Charger/Charger Selector or Charger System Manager
#endif

#ifndef DEV_CHARGER_ID_TO_PORT
#define DEV_CHARGER_ID_TO_PORT(chgId)    (0)   // Always map to port 0 by default
#endif

typedef enum {

    DEV_CHARGER_REG_ChargerSpecInfo                     = 0x11, // read the charger's extended status bits.
    DEV_CHARGER_REG_ChargerOption                       = 0x12, // set the various charger modes.
    DEV_CHARGER_REG_ChargerStatus                       = 0x13, // read the charger's status bits
    DEV_CHARGER_REG_ChargerChargingCurrent              = 0x14, // master device sends the desired charging rate (mA).
    DEV_CHARGER_REG_ChargerChargingVoltage              = 0x15, // master device sends the desired charging voltage
                                                          // Or MaxSystemVoltage (Vsys) in NVDC mode
    DEV_CHARGER_REG_AlarmWarning                        = 0x16, // Battery, acting as a bus master device, sends the AlarmWarning() message
    /*--Non-Standard----------------------------*/
    DEV_CHARGER_REG_InputCurrent                        = 0x3F, // 6-Bit Input Current Setting
    DEV_CHARGER_REG_ManufacturerID                      = 0xFE, // Manufacturer ID 0x0040
    DEV_CHARGER_REG_DeviceID                            = 0xFF, // Device ID       0x0008
    /*------------------------------------------*/

} DEV_CHARGER_REG;

//
// Below are RAW REG value of each ACOK_Ref voltage
//
#define ISL9241_VAL_ACOKRef_18912   (0x3140)
#define ISL9241_VAL_ACOKRef_18816   (0x3100)
#define ISL9241_VAL_ACOKRef_17664   (0x2E00)
#define ISL9241_VAL_ACOKRef_17568   (0x2DC0)
#define ISL9241_VAL_ACOKRef_08064   (0x1300)
#define ISL9241_VAL_ACOKRef_04064   (0x980)
#define ISL9241_VAL_ACOKRef_24480   (0x3FC0)
#define ISL9241_VAL_ACOKRef_MAX_RAW (ISL9241_VAL_ACOKRef_24480)

#define ISL9241_VAL_VinVoltage_8192 (0x1000)

//
// Below one is application layer max ACOK_Ref voltage
//
#define ISL9241_VAL_ACOKRef_MAX     (24480)
/* **************************************************************************************************************** *
 *                                                                                                                  *
 * ISL9241 register maps                   Reg,     Bit# | Comments                                     | POR value *
 *                                                                                                                  *
 * **************************************************************************************************************** */
typedef enum {

    DEV_ISL9241_REG_ChargeCurrentLimit    = 0x14, // RW 11 | [12:2]11-bit (0x0000h = disables fast        | 0x0000
                                                //       | charging, trickle charging is 
                                                //       | allowed)
    DEV_ISL9241_REG_MaxSystemVoltage      = 0x15, // RW 12 | [14:3]12-bit, (0x0000h = disables switching) | Set by PROG
                                                //       |                                                8.384V for 2-cell 0x20C0h
                                                //       |                                                12.576V for 3-cell 0x3120h
                                                //       |                                                16.768V for 4-cell 0x4180h                                                
    DEV_ISL9241_REG_Control7              = 0x38, // RW 16 | Configure two-level adapter current limit    | 0x0000
                                                //       | duration, set peak inductor current during   |
                                                //       | supplemental mode, Buck-boost period stretch |
    DEV_ISL9241_REG_Control0              = 0x39, // RW 16 | Configure various charger options            | 0x0000
    DEV_ISL9241_REG_Information1          = 0x3A, // R  16 | Indicate various charger status              | 0x0000
    DEV_ISL9241_REG_AdapterCurrentLimit2  = 0x3B, // RW 11 | [12:2] 11-bit                                | 0x05DC (Max. 6140mA)
    DEV_ISL9241_REG_Control1              = 0x3C, // RW 16 | Configure various charger options            | 0x0103 (Includes value set by PROG pin)
    DEV_ISL9241_REG_Control2              = 0x3D, // RW 16 | Configure various charger options            | 0x6000
    DEV_ISL9241_REG_MinSystemVoltage      = 0x3E, // RW  8 | [13:6]8-bit (0x0000h = disables all battery  | 0x0000 (Max. 16.32V)
                                                //       | charging)                                    |
    DEV_ISL9241_REG_AdapterCurrentLimit1  = 0x3F, // RW 11 | [12:2] 11-bit                                | Set by PROG pin (Max. 6140mA)
    DEV_ISL9241_REG_ACOKReference         = 0x40, // RW  8 | [13:6] x 8-Bit, (0x0000h = disables functionality) | 0x0000 (Max. 24.48V)
    DEV_ISL9241_REG_Control6              = 0x43, // RW 13 | Interrupt trigger level direction            | 0x1FFF
    DEV_ISL9241_REG_ACProchotL            = 0x47, // RW  6 | [12:7] adapter current PROCHOT# threshold    | 0x0C00 (Max. 6.4A)
    DEV_ISL9241_REG_DCProchotL            = 0x48, // RW  6 | [13:8] Battery discharging current PROCHOT# threshold | 0x1000 (Max. 12.8A)
    DEV_ISL9241_REG_OTG_Voltage           = 0x49, // RW 12 | [14:3] 12-bit OTG mode voltage reference     | 0x0D08 (Max. 23.4V)
    DEV_ISL9241_REG_OTG_Current           = 0x4A, // RW  8 | [12:5] 8-bit, OTG mode maximum current lmt.  | 0x0200 (Max. 6112mA)
    DEV_ISL9241_REG_VIN_Voltage           = 0x4B, // RW  6 | [13:6] 8-bit, VIN loop voltage reference     | 0x0C00 (Max. 16.384V)
    DEV_ISL9241_REG_Control3              = 0x4C, // RW 16 | Configure various charger options            | 0x0300
    DEV_ISL9241_REG_Information2          = 0x4D, // R  16 | Indicate various charger status              | 0x0000
    DEV_ISL9241_REG_Control4              = 0x4E, // RW 16 | Configure various charger options            | 0x0000
    DEV_ISL9241_REG_Control5              = 0x4F, // RW 13 | Interrupt mask control enable                | 0x0000
    DEV_ISL9241_REG_NTC_ADC_Results       = 0x80, // R  10 | ADC for NTC/GP measurements                  | 0x??, see ds Table 12
    DEV_ISL9241_REG_VBAT_ADC_Results      = 0x81, // R   8 | ADC for VBAT measurements                    | 0x??, see ds Table 12
    DEV_ISL9241_REG_TJ_ADC_Results        = 0x82, // R   8 | ADC for internal TJ measurements             | 0x??, see ds Table 12
    DEV_ISL9241_REG_IADP_ADC_Results      = 0x83, // R   8 | ADC for adapter current measurements         | 0x??, see ds Table 12
    DEV_ISL9241_REG_DC_ADC_Results        = 0x84, // R   8 | ADC for battery discharge current measurements | 0x??, see ds Table 12
    DEV_ISL9241_REG_CC_ADC_Results        = 0x85, // R   8 | ADC for battery charge current measurements  | 0x??, see ds Table 12
    DEV_ISL9241_REG_VSYS_ADC_Results      = 0x86, // R   8 | ADC for CSOP (VSYS) measurements             | 0x??, see ds Table 12
    DEV_ISL9241_REG_VIN_ADC_Results       = 0x87, // R   8 | ADC for CSIN (VIN) measurements              | 0x??, see ds Table 12
    DEV_ISL9241_REG_Information3          = 0x90, // RW 13 | Interrupt status - multiple events possible  | 0x0000
    DEV_ISL9241_REG_Information4          = 0x91, // RW 13 | Interrupt real time status                   | 0x0000
    DEV_ISL9241_REG_ManufacturerID        = 0xFE, // RO  8 | Manufacturer ID                              | 0x49
    DEV_ISL9241_REG_DeviceID              = 0xFF, // RO  8 | Device ID                                    | 0x0E

} DEV_ISL9241_ADDITIONAL_REG;

typedef union {
	uint16_t u16;
	struct {
		uint16_t ReverseTurboBoost   : 1;
		uint16_t VSYSRegulationOffsetVoltageAdder : 1;
		uint16_t InputVoltageRegulation  : 1;
		uint16_t DCProchotLThresholdBatteryOnlyMode : 2;
	} f;
} DEV_CHARGER_ST_Control0;

typedef struct {
    uint8_t slaveAddr;
    uint32_t continuousErrCount;
    uint32_t maxContinuousErrCount;
} DEV_CHARGER_I2C_INTERFACE_ERROR_COUNT;

#define DEV_CHARGER_RATIO_APP_1_REG_1     (0)   // 1:1
#define DEV_CHARGER_RATIO_APP_1_REG_0P5   (1)   // 1:2
#define DEV_CHARGER_RATIO_APP_0P5_REG_1   (2)   // 2:1
#define DEV_CHARGER_RATIO_APP_1_REG_0P25  (3)   // 1:4

#define DEV_CHARGER_REG_DISABLED     0
#define DEV_CHARGER_REG_READ_ONLY    1
#define DEV_CHARGER_REG_WRITE_ONLY   2
#define DEV_CHARGER_REG_READ_WRITE   3

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
} DEV_CHARGER_REG_DATA;

typedef struct {

    uint8_t reg;

    /* value and accessing lock */
    DEV_CHARGER_REG_DATA data;

    /* Polling interval */
    uint32_t u32refreshReload;
    uint32_t u32refreshCounter;

    /* Control attribute */
    union {
        uint8_t u8Ctrl;
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
} DEV_CHARGER_REG_ITEM;

extern DEV_CHARGER_REG_ITEM *_s_chgBuf[MAX_CHARGER_SUPPORTED];
extern DEV_CHARGER_REG_ITEM *_s_platform_chgBufTable[MAX_CHARGER_SUPPORTED];

/**
 * @brief Find reg from platform and default reg table.
 *
 * @param chgId Index of charger.
 * @param reg reg to find.
 *
 * @retval reg item find in table or NULL when not fonud.
 */
DEV_CHARGER_REG_ITEM *dev_isl9241_findRegItem(uint8_t chgId, uint8_t reg);

/**
 * @brief Register platform charger.
 *
 * @param chgId Index of the charger.
 * @param pTable charger tabble pointer.
 * @param chgBufTable platform charger table.
 */
void dev_isl9241_register_platform_table(uint8_t chgId, DEV_CHARGER_REG_ITEM *pTable, DEV_CHARGER_REG_ITEM **chgBufTable);
/* ****************************************************************
 *
 * Thread-safe functions. Non-reenterable !!
 *
 * bool dev_isl9241_i32CacheGet(uint8_t reg, void * pVal);
 * bool dev_isl9241_i32Read(uint8_t reg, void * pVal);
 * bool dev_isl9241_i32Write(uint8_t reg, int32_t val);
 *
 * ****************************************************************/

/**
 * @brief Get charger register value from cache.
 *
 * @param chgId Index of the charger.
 * @param reg Charger reg.
 * @param pVal Address of the variable to save value read from cache.
 *
 * @retval true is successful.
 */
bool dev_isl9241_i32CacheGet(uint8_t chgId, uint8_t reg, void *pVal);

/**
 * @brief Get charger register value from charger reg.
 *
 * @param chgId Index of the charger.
 * @param reg Charger reg.
 * @param pVal Address of the variable to save value read from cache.
 *
 * @retval true is success.
 */
bool dev_isl9241_i32Read(uint8_t chgId, uint8_t reg, void *pVal);

/**
 * @brief Write value to charger register.
 *
 * @param chgId Index of the charger.
 * @param reg Charger reg.
 * @param pVal Target value.
 *
 * @retval true is success.
 */
bool dev_isl9241_i32Write(uint8_t chgId, uint8_t reg, int32_t val);

/**
 * @brief Refresh charger reg in the charger buffer.
 *
 * @param reg Charger reg.
 * @param pBuf Charger buffer pointer.
 */
void dev_isl9241_reg_refresh(uint8_t *reg, void *pBuf);

/**
 * @brief Read charger reg item from specific i2c port and address.
 *
 * @param pItem Charger reg item.
 * @param port Charger i2c port.
 * @param slaveAddr Charger i2c slave address.
 * 
 * @retval false if fail.
 * @retval dataAvailable flag.
 */
bool dev_isl9241_autoRead(DEV_CHARGER_REG_ITEM *pItem, uint8_t port, uint8_t slaveAddr);

/**
 * @brief Modify charger reg with mask.
 *
 * @param chgId Index of the charger.
 * @param reg Charger reg.
 * @param val Value want to write.
 * @param msk Bit mask.
 * 
 * @retval True if success.
 */
bool dev_isl9241_regRMW(uint8_t chgId, uint8_t reg, uint16_t val, uint16_t msk);

/**
 * @brief Enable charger.
 *
 * Set F_CHG_REG_MinSystemVoltage to non-zero
 * 
 * @param chgId Index of the charger.
 * 
 * @retval True if success.
 */
bool dev_isl9241_chgEnable(uint8_t chgId);

/**
 * @brief Disable charger.
 *
 * Set F_CHG_REG_MinSystemVoltage to zero
 * 
 * @param chgId Index of the charger.
 * 
 * @retval True if success.
 */
bool dev_isl9241_chgDisable(uint8_t chgId);

/**
 * @brief Return charge enable status.
 * 
 * @param chgId Index of the charger.
 * 
 * @retval True if F_CHG_REG_MinSystemVoltage is non-zero.
 */

bool dev_isl9241_isChgEn(uint8_t chgId);

/**
 * @brief Set BGATE status.
 * 
 * @param chgId Index of the charger.
 * @param turnOn Target BGATE status.
 * 
 * @retval True if success.
 */
bool dev_isl9241_setBgate(uint8_t chgId, bool turnOn);

/**
 * @brief Set adapter CurrentLimit1.
 * 
 * @param chgId Index of the charger.
 * @param u16limit Target CurrentLimit1.
 * 
 * @retval True if success.
 */
bool dev_isl9241_AdapterCurrentLimit1(uint8_t chgId, uint16_t u16limit);

/**
 * @brief Set adapter CurrentLimit2.
 * 
 * @param chgId Index of the charger.
 * @param u16limit Target CurrentLimit2.
 * 
 * @retval True if success.
 */
bool dev_isl9241_AdapterCurrentLimit2(uint8_t chgId, uint16_t u16limit);

/**
 * @brief Set adapter ACProchot.
 * 
 * @param chgId Index of the charger.
 * @param u16limit Target ACProchot threshold.
 * 
 * @retval True if success.
 */
bool dev_isl9241_AdapterACProchotL(uint8_t chgId, uint16_t u16limit);

/**
 * @brief Set adapter DCProchot.
 * 
 * @param chgId Index of the charger.
 * @param u16limit Target DCProchot threshold.
 * 
 * @retval True if success.
 */
bool dev_isl9241_AdapterDCProchotL(uint8_t chgId, uint16_t u16limit);

/**
 * @brief Set charger to bypass mode.
 * 
 * @param chgId Index of the charger.
 * 
 * @retval True if success.
 */
bool dev_isl9241_toByPassMode_v1(uint8_t chgId);

/**
 * @brief Set charger to nvdc mode.
 * 
 * @param chgId Index of the charger.
 * 
 * @retval True if success.
 */
bool dev_isl9241_toNvdcMode_v1(uint8_t chgId);

/**
 * @brief Set charger ACOK reference.
 * 
 * convert to register raw value ([13:6] Stepping 96mV, Max. 24.48V))
 * 
 * @param chgId Index of the charger.
 * @param AcRefVoltage Target AcRefVoltage.
 * 
 * @retval True if success.
 */
bool dev_isl9241_ACOKReference(uint8_t chgId, uint32_t AcRefVoltage);

/**
 * @brief Set charger ACOK reference.
 * 
 * dev_isl9241_RAA489110_ACOKReference ([13:6] Stepping 144mV, Max. 36.72V))
 * 
 * @param chgId Index of the charger.
 * @param AcRefVoltage Target AcRefVoltage.
 * 
 * @retval True if success.
 */
bool dev_isl9241_RAA489110_ACOKReference(uint8_t chgId, uint32_t AcRefVoltage);

/**
 * @brief Set charger Vin voltage.
 * 
 * dev_isl9241_VIN_Voltage ([13:6] Stepping 85mV, Max. 16.384V))
 * 
 * @param chgId Index of the charger.
 * @param VinVoltage Target VinVoltage.
 * 
 * @retval True if success.
 */
bool dev_isl9241_VIN_Voltage(uint8_t chgId, uint32_t VinVoltage);

/**
 * @brief Set charger Vin voltage.
 * 
 * dev_isl9241_RAA489110_VIN_Voltage ([13:6] Stepping 128mV, Max. 24.576V))
 * 
 * @param chgId Index of the charger.
 * @param VinVoltage Target VinVoltage.
 * 
 * @retval True if success.
 */
bool dev_isl9241_RAA489110_VIN_Voltage(uint8_t chgId, uint32_t VinVoltage);

typedef enum
{
    DEV_CHARGER_PROCHOT_DEBOUNCE_7us = 0, // 2'b00 (default)
    DEV_CHARGER_PROCHOT_DEBOUNCE_100us,   // 2'b01
    DEV_CHARGER_PROCHOT_DEBOUNCE_500us,   // 2'b10
    DEV_CHARGER_PROCHOT_DEBOUNCE_1ms,     // 2'b11
} EM_DEV_CHARGER_PROCHOT_DEBOUNCE;

/**
 * @brief Set charger prochot debounce.
 * 
 * @param chgId Index of the charger.
 * @param debounce enum of debounce time.
 * 
 * @retval True if success.
 */
bool dev_isl9241_SetProchotDebounce(uint8_t chgId, EM_DEV_CHARGER_PROCHOT_DEBOUNCE debounce);

typedef enum
{
    DEV_CHARGER_PROCHOT_DURATION_10ms = 0, // 3'b000 (default)
    DEV_CHARGER_PROCHOT_DURATION_20ms,     // 3'b001
    DEV_CHARGER_PROCHOT_DURATION_15ms,     // 3'b010
    DEV_CHARGER_PROCHOT_DURATION_5ms,      // 3'b011
    DEV_CHARGER_PROCHOT_DURATION_1ms,      // 3'b100
    DEV_CHARGER_PROCHOT_DURATION_500us,    // 3'b101
    DEV_CHARGER_PROCHOT_DURATION_100us,    // 3'b110
    DEV_CHARGER_PROCHOT_DURATION_0s        // 3'b111
} EM_DEV_CHARGER_PROCHOT_DURATION;

/**
 * @brief Set charger prochot duration.
 * 
 * @param chgId Index of the charger.
 * @param debounce enum of debounce time.
 * 
 * @retval True if success.
 */
bool dev_isl9241_SetProchotDuration(uint8_t chgId, EM_DEV_CHARGER_PROCHOT_DURATION e);

/**
 * @brief Set charger battery-only mode.
 * 
 * @param chgId Index of the charger.
 * @param enable Enable status.
 */
void dev_isl9241_battery_only(uint8_t chgId, bool enable);

typedef void (*pfnSetupProchotGate_t)(void);

/**
 * @brief Register set prochot handler.
 * 
 * @param handler Set prochot handler pointer.
 */
void dev_isl9241_prochot_register(pfnSetupProchotGate_t handler);

/**
 * @brief Trigger set prochot handler.
 */
void dev_isl9241_setProchot(void);
#endif // end of __DEV_CHARGER_H__
