/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include <zephyr/logging/log.h>
#include "i2c_hub.h"
#include "dev_cpld.h"
#include "board_config.h"
LOG_MODULE_REGISTER(cpld, CONFIG_CPLD_LOG_LEVEL);

#ifndef CPLD_I2C_PORT
#define CPLD_I2C_PORT I2C_2
#endif

#ifndef SCAN_I2C_PORT
#define SCAN_I2C_PORT I2C_2
#endif

#define DEV_CPLD_MEM_VR_TURNON    0x60   // Send this command to turn on CPLD MEM VR 
#define DEV_CPLD_MEM_VR_TURNOFF   0x61   // Send this command to turn off CPLD MEM VR
#define DEV_CPLD_MEM_VR_CHECKING  0x62   // Send this command to know CPLD MEM VR status; ACK = On, NACK = Off

/**
 * @brief Scan CPLD I2C addresses in range 0x5F to 0x63
 * @return Number of devices that responded with ACK
 */
uint8_t dev_cpld_scan_i2c(void)
{
	uint32_t ret;
	uint8_t counter = 0;
	for (uint8_t i = 0x5F; i <= 0x63; i++) {
		ret = i2c_hub_write(CPLD_I2C_PORT, NULL, 0, i);
		if (ret == 0)
		{
			counter++;
			LOG_ERR("[CPLDSCAN]  slv %02X, ret %d", i, ret);
		}
	}

	return counter;
}

/**
 * @brief Turn on CPLD Memory VR
 */
bool dev_cpld_turnOn_MemVR (void)
{
	//
	// This is only required by LPDDR5 of Lilac RevB
	//
	uint32_t ret = 0;
	uint8_t retry = 10;

    while (retry) {
        retry --;
        ret = i2c_hub_write(CPLD_I2C_PORT, NULL, 0, DEV_CPLD_MEM_VR_TURNON);
        if (ret != 0)
            continue;
        if (ret == 0)
            return ret;
    }
    if (!retry) {
		LOG_ERR("[!!Fatal error!!] on CPLD[%02X], ret %d", DEV_CPLD_MEM_VR_TURNON, ret);
	}
	return ret;
}

//
// This function is also used for CPLD version detection
// if NACK of 0x61, skip the whole sequence
//
bool dev_cpld_turnOff_MemVR (void)
{
	//
	// This is only required by LPDDR5 of Lilac RevB
	//
	uint32_t ret = 0;
	uint8_t retry = 10;

    while (retry) {
        retry --;
        ret = i2c_hub_write(CPLD_I2C_PORT, NULL, 0, DEV_CPLD_MEM_VR_TURNOFF);
        if (ret != 0)
            continue;
        if (ret == 0)
            return ret;
    }
    if (!retry) {
		LOG_ERR("[!!Fatal error!!] on CPLD[%02X], ret %d", DEV_CPLD_MEM_VR_TURNOFF, ret);
	}
	return ret;
}

/**
 * @brief Scan entire I2C bus for devices (addresses 0x00 to 0x7F)
 * @return Number of devices that responded with ACK
 */
uint8_t dev_cpld_scan_i2c_bus(void)
{
	uint32_t ret = 0;
	uint8_t counter = 0;
	for (uint8_t i = 0; i <= 0x7F; i++) {
		ret = i2c_hub_write(SCAN_I2C_PORT, NULL, 0, i);
		if (ret == 0)
		{
			counter++;
			LOG_ERR("[I2CSCAN]  slv %02X, ret %d", i, ret);
		}
	}

	return counter;
}