/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_PCA9535_H__
#define __DEV_PCA9535_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>


#define DEV_PCA9535_REG_INPUT1       0x00
#define DEV_PCA9535_REG_INPUT2       0x01
#define DEV_PCA9535_REG_OUTPUT1      0x02
#define DEV_PCA9535_REG_OUTPUT2      0x03
#define DEV_PCA9535_REG_POLARITY1    0x04    // 0 - no inverted
#define DEV_PCA9535_REG_POLARITY2    0x05    // 0 - no inverted
#define DEV_PCA9535_REG_CONFIG1      0x06    // 1 - input; 0 - output
#define DEV_PCA9535_REG_CONFIG2      0x07    // 1 - input; 0 - output
#define DEV_PCA9535_REG_IER1         0x0A    // 1 - enable; 0 - disable
#define DEV_PCA9535_REG_IER2         0x0B    // 1 - enable; 0 - disable

/**
 * @brief Init pca9535 device.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param outputMask Output register mask.
 * @param openDrainMask Open drain register mask.
 * @param outputValue Output value.
 */
void dev_pca9535_init( uint8_t slv, uint8_t i2cDev, uint16_t outputMask, uint16_t openDrainMask, uint16_t outputValue, uint16_t ier );

/**
 * @brief Free device.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 */
void dev_pca9535_free( uint8_t slv , uint8_t i2cDev );

/**
 * @brief Read register by slave address and port.
 *
 * This function returns lastSet for output or OpenDrain pins.
 * It only guarantees the return value is up to date for bits which be set in 'mask'.
 * 
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param mask Bit mask.
 * 
 * @retval Value read from device.
 */
uint16_t dev_pca9535_smartRead(uint8_t slv, uint8_t i2cDev, uint16_t mask);

/**
 * @brief Read register by slave address and port.
 *
 * This function get value from buffer, can't guarantee value is latest.
 * 
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param mask Bit mask.
 * 
 * @retval Value read from device.
 */
uint16_t dev_pca9535_bufferRead(uint8_t slv, uint8_t i2cDev, uint16_t mask);

/**
 * @brief Get input status from input register.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * 
 * @retval Input read from device.
 */
uint16_t dev_pca9535_getInput(uint8_t slv, uint8_t i2cDev);

/**
 * @brief Set pin pull up or open drain .
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param idx Index of device.
 * @param isOutput Output flag.
 * @param isOd Open drain flag.
 */
void dev_pca9535_setPuOd(uint8_t slv, uint8_t i2cDev, uint16_t idx, bool isOutput, bool isOd);

/**
 * @brief Set pin pull up or open drain .
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param idx Index of device.
 * @param isOutput Output flag.
 * @param isOd Open drain flag.
 */
void dev_pca9535_setBit(uint8_t slv, uint8_t i2cDev, uint16_t idx, bool isOutput, bool isOd, bool val);

/**
 * @brief Set output.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param mask Index of device.
 * @param val Output flag.
 */
void dev_pca9535_setOutput(uint8_t slv, uint8_t i2cDev, uint16_t mask, uint16_t val);

/**
 * @brief Set device interrupt.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 */
void dev_pca9535_intEnable(uint8_t slv, uint8_t i2cDev);

#endif // end of __DEV_PCA9535_H__
