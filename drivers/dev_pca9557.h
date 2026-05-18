/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_PCA9557_H__
#define __DEV_PCA9557_H__

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/device.h>

/**
 * @brief Init pca9557 device.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param outputMask Output register mask.
 * @param openDrainMask Open drain register mask.
 * @param outputValue Output value.
 */
void dev_pca9557_init( uint8_t slv, uint8_t i2cDev, uint8_t outputMask, uint8_t openDrainMask, uint8_t outputValue );

/**
 * @brief Free device.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 */
void dev_pca9557_free( uint8_t slv , uint8_t i2cDev );

/**
 * @brief Set device interrupt.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param mask Bit mask.
 */
void dev_pca9557_intEnable(uint8_t slv, uint8_t i2cDev, uint8_t mask);

/**
 * @brief Set device interrupt mask.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param mask Bit mask.
 */
void dev_pca9557_intMask(uint8_t slv, uint8_t i2cDev, uint8_t mask);

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
uint8_t dev_pca9557_smartRead(uint8_t slv, uint8_t i2cDev, uint8_t mask);

/**
 * @brief Get input status from input register.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * 
 * @retval Input read from device.
 */
uint8_t dev_pca9557_getInput(uint8_t slv, uint8_t i2cDev);

/**
 * @brief Set pin pull up or open drain .
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param idx Index of device.
 * @param isOutput Output flag.
 * @param isOd Open drain flag.
 */
void dev_pca9557_setBit(uint8_t slv, uint8_t i2cDev, uint8_t idx, bool isOutput, bool isOd, bool val);

/**
 * @brief Set output.
 *
 * @param slv Device slave address.
 * @param i2cDev I2C port connected to device.
 * @param mask Index of device.
 * @param val Output flag.
 */
void dev_pca9557_setOutput(uint8_t slv, uint8_t i2cDev, uint8_t mask, uint8_t val);

#endif // end of __DEV_PCA9557_H__
