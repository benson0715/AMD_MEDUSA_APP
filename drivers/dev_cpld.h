/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __DEV_CPLD_H__
#define __DEV_CPLD_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief scan cpld device on i2c bus.
 *
 * @retval count of cpld device.
 */
uint8_t dev_cpld_scan_i2c(void);

/**
 * @brief turn on MemVR.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error.
 */
bool dev_cpld_turnOn_MemVR (void);

/**
 * @brief turn off MemVR.
 * 
 * This function is also used for CPLD version detection.
 * If NACK of 0x61, skip the whole sequence.
 * This is only required by LPDDR5 of Lilac RevB.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error.
 */
bool dev_cpld_turnOff_MemVR (void);

/**
 * @brief scan i2c device on cpld i2c bus.
 *
 * @retval count of device on i2c bus.
 */
uint8_t dev_cpld_scan_i2c_bus(void);
#endif // end of __DEV_CPLD_H__