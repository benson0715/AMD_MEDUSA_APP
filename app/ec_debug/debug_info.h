
/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *****************************************************************************
 */
#ifndef __DEBUG_INFO_H__
#define __DEBUG_INFO_H__
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <assert.h>
#include <soc.h>
enum {
	INFO_I2C_SVI3 = 0,
	INFO_I2C_PCA9535,
	INFO_I2C_MPC,
	INFO_I2C_MAX695x,
	INFO_I2C_TMP432,
	INFO_I2C_SMTBAT,
	INFO_I2C_INA219,
	INFO_I2C_CHARGER,
	INFO_I2C_TPS6699x,
	INFO_I2C_RAA489300,
	
	INFO_NUMBER,
};
#define INFO_RECORD_THREAD_NUMBER 20
extern uint32_t g_info_statistics_buf[INFO_NUMBER];
void info_value_increase(uint8_t id, uint8_t value);
void info_thread_collect(void);
void info_value_collect(void);
#endif // end of __DEBUG_HOOK_H__