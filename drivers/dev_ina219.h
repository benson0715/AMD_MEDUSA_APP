/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_INA219_H__
#define __DEV_INA219_H__

#include <stdint.h>
#include <stdbool.h>

// INA219 Registers
#define INA219_REG_CONFIG                       (uint8_t)(0x00)                // CONFIG REGISTER (R/W)
#define INA219_REG_SHUNTVOLTAGE                 (uint8_t)(0x01)                // SHUNT VOLTAGE REGISTER (R)
#define INA219_REG_BUSVOLTAGE                   (uint8_t)(0x02)                // BUS VOLTAGE REGISTER (R)
#define INA219_REG_POWER                        (uint8_t)(0x03)                // POWER REGISTER (R)
#define INA219_REG_CURRENT                      (uint8_t)(0x04)                // CURRENT REGISTER (R)
#define INA219_REG_CALIBRATION                  (uint8_t)(0x05)                // CALIBRATION REGISTER (R/W)

// Macros for assigning config bits
#define INA219_CFGB_RESET(x)                    (uint16_t)((x & 0x01) << 15)    // Reset Bit
#define INA219_CFGB_BUSV_RANGE(x)               (uint16_t)((x & 0x01) << 13)    // Bus Voltage Range
#define INA219_CFGB_PGA_RANGE(x)                (uint16_t)((x & 0x03) << 11)    // Shunt Voltage Range
#define INA219_CFGB_BADC_RES_AVG(x)             (uint16_t)((x & 0x0F) << 7)     // Bus ADC Resolution/Averaging
#define INA219_CFGB_SADC_RES_AVG(x)             (uint16_t)((x & 0x0F) << 3)     // Shunt ADC Resolution/Averaging
#define INA219_CFGB_MODE(x)                     (uint16_t) (x & 0x07)           // Operating Mode

// Configuration Register
#define INA219_CFG_RESET                        INA219_CFGB_RESET(1)            // Reset Bit

#define INA219_CFG_BVOLT_RANGE_MASK             INA219_CFGB_BUSV_RANGE(1)       // Bus Voltage Range Mask
#define INA219_CFG_BVOLT_RANGE_16V              INA219_CFGB_BUSV_RANGE(0)       // 0-16V Range
#define INA219_CFG_BVOLT_RANGE_32V              INA219_CFGB_BUSV_RANGE(1)       // 0-32V Range (default)

#define INA219_CFG_SVOLT_RANGE_MASK             INA219_CFGB_PGA_RANGE(3)        // Shunt Voltage Range Mask
#define INA219_CFG_SVOLT_RANGE_40MV             INA219_CFGB_PGA_RANGE(0)        // Gain 1, 40mV Range
#define INA219_CFG_SVOLT_RANGE_80MV             INA219_CFGB_PGA_RANGE(1)        // Gain 2, 80mV Range
#define INA219_CFG_SVOLT_RANGE_160MV            INA219_CFGB_PGA_RANGE(2)        // Gain 4, 160mV Range
#define INA219_CFG_SVOLT_RANGE_320MV            INA219_CFGB_PGA_RANGE(3)        // Gain 8, 320mV Range (default)

#define INA219_CFG_BADCRES_MASK                 INA219_CFGB_BADC_RES_AVG(15)    // Bus ADC Resolution and Averaging Mask
#define INA219_CFG_BADCRES_9BIT_1S_84US         INA219_CFGB_BADC_RES_AVG(0)     // 1 x 9-bit Bus sample
#define INA219_CFG_BADCRES_10BIT_1S_148US       INA219_CFGB_BADC_RES_AVG(1)     // 1 x 10-bit Bus sample
#define INA219_CFG_BADCRES_11BIT_1S_276US       INA219_CFGB_BADC_RES_AVG(2)     // 1 x 11-bit Bus sample
#define INA219_CFG_BADCRES_12BIT_1S_532US       INA219_CFGB_BADC_RES_AVG(3)     // 1 x 12-bit Bus sample (default)
#define INA219_CFG_BADCRES_12BIT_2S_1MS         INA219_CFGB_BADC_RES_AVG(9)     // 2 x 12-bit Bus samples averaged together
#define INA219_CFG_BADCRES_12BIT_4S_2MS         INA219_CFGB_BADC_RES_AVG(10)    // 4 x 12-bit Bus samples averaged together
#define INA219_CFG_BADCRES_12BIT_8S_4MS         INA219_CFGB_BADC_RES_AVG(11)    // 8 x 12-bit Bus samples averaged together
#define INA219_CFG_BADCRES_12BIT_16S_8MS        INA219_CFGB_BADC_RES_AVG(12)    // 16 x 12-bit Bus samples averaged together
#define INA219_CFG_BADCRES_12BIT_32S_17MS       INA219_CFGB_BADC_RES_AVG(13)    // 32 x 12-bit Bus samples averaged together
#define INA219_CFG_BADCRES_12BIT_64S_34MS       INA219_CFGB_BADC_RES_AVG(14)    // 64 x 12-bit Bus samples averaged together
#define INA219_CFG_BADCRES_12BIT_128S_68MS      INA219_CFGB_BADC_RES_AVG(15)    // 128 x 12-bit Bus samples averaged together

#define INA219_CFG_SADCRES_MASK                 INA219_CFGB_SADC_RES_AVG(15)    // Shunt ADC Resolution and Averaging Mask
#define INA219_CFG_SADCRES_9BIT_1S_84US         INA219_CFGB_SADC_RES_AVG(0)     // 1 x 9-bit Shunt sample
#define INA219_CFG_SADCRES_10BIT_1S_148US       INA219_CFGB_SADC_RES_AVG(1)     // 1 x 10-bit Shunt sample
#define INA219_CFG_SADCRES_11BIT_1S_276US       INA219_CFGB_SADC_RES_AVG(2)     // 1 x 11-bit Shunt sample
#define INA219_CFG_SADCRES_12BIT_1S_532US       INA219_CFGB_SADC_RES_AVG(3)     // 1 x 12-bit Shunt sample (default)
#define INA219_CFG_SADCRES_12BIT_2S_1MS         INA219_CFGB_SADC_RES_AVG(9)     // 2 x 12-bit Shunt samples averaged together
#define INA219_CFG_SADCRES_12BIT_4S_2MS         INA219_CFGB_SADC_RES_AVG(10)    // 4 x 12-bit Shunt samples averaged together
#define INA219_CFG_SADCRES_12BIT_8S_4MS         INA219_CFGB_SADC_RES_AVG(11)    // 8 x 12-bit Shunt samples averaged together
#define INA219_CFG_SADCRES_12BIT_16S_8MS        INA219_CFGB_SADC_RES_AVG(12)    // 16 x 12-bit Shunt samples averaged together
#define INA219_CFG_SADCRES_12BIT_32S_17MS       INA219_CFGB_SADC_RES_AVG(13)    // 32 x 12-bit Shunt samples averaged together
#define INA219_CFG_SADCRES_12BIT_64S_34MS       INA219_CFGB_SADC_RES_AVG(14)    // 64 x 12-bit Shunt samples averaged together
#define INA219_CFG_SADCRES_12BIT_128S_68MS      INA219_CFGB_SADC_RES_AVG(15)    // 128 x 12-bit Shunt samples averaged together

#define INA219_CFG_MODE_MASK                    INA219_CFGB_MODE(7)             // Operating Mode Mask
#define INA219_CFG_MODE_POWERDOWN               INA219_CFGB_MODE(0)             // Power-Down
#define INA219_CFG_MODE_SVOLT_TRIGGERED         INA219_CFGB_MODE(1)             // Shunt Voltage, Triggered
#define INA219_CFG_MODE_BVOLT_TRIGGERED         INA219_CFGB_MODE(2)             // Bus Voltage, Triggered
#define INA219_CFG_MODE_SANDBVOLT_TRIGGERED     INA219_CFGB_MODE(3)             // Shunt and Bus, Triggered
#define INA219_CFG_MODE_ADCOFF                  INA219_CFGB_MODE(4)             // ADC Off (disabled)
#define INA219_CFG_MODE_SVOLT_CONTINUOUS        INA219_CFGB_MODE(5)             // Shunt Voltage, Continuous
#define INA219_CFG_MODE_BVOLT_CONTINUOUS        INA219_CFGB_MODE(6)             // Bus Voltage, Continuous
#define INA219_CFG_MODE_SANDBVOLT_CONTINUOUS    INA219_CFGB_MODE(7)             // Shunt and Bus, Continuous (default)

// Bus Voltage Register
#define INA219_BVOLT_CNVR                       (uint16_t)(0x0002)              // Conversion Ready
#define INA219_BVOLT_OVF                        (uint16_t)(0x0001)              // Math Overflow Flag

typedef enum {
	DEV_INA219_10_mOHM = 0,
	DEV_INA219_100_mOHM
} DEV_INA219_EM_RSHUNT;

/**
 * @brief Access ina219 register.
 *
 * @param isRead Indicate write or read.
 * @param reg from where the data is transferred.
 * @param pData Memory pool from which the data is transferred.
 * @param dataLen dataLen.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error.
 */
uint32_t dev_ina219_regAccess(bool isRead, uint8_t reg, void * pData, uint8_t dataLen);

/**
 * @brief Access ina219 register.
 *
 * @param rshunt Indicate write or read.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error.
 */
void dev_ina219_calibrate(DEV_INA219_EM_RSHUNT rshunt);

/**
 * @brief Get voltage from ina219.
 *
 * @retval Voltage read from ina219(mV).
 */
uint32_t dev_ina219_GetVoltage_mV(void);

/**
 * @brief Get current from ina219.
 *
 * @retval Current read from ina219(uA).
 */
uint32_t dev_ina219_GetCurrent_uA(void);

/**
 * @brief Get power from ina219.
 *
 * @retval Power read from ina219(mW).
 */
uint32_t dev_ina219_GetPower_mW(void);

#endif // end of __DEV_INA219_H__

