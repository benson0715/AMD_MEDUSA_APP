/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "system.h"
#include "i2c_hub.h"
#include "dev_utility.h"
#include "amd_crb_drivers_raa489300.h"
#include "debug_info.h"
LOG_MODULE_REGISTER(raa489300, CONFIG_RAA_LOG_LEVEL);

/* I2C port number */
#ifndef TPS_I2C_PORT
#define TPS_I2C_PORT                      I2C_3
#endif

/**
 * amd_crb_drivers_raa489300_access
 *
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]  address;     I2C address
 * @param [in]      reg;     device register
 * @param [in]    pBuf;      data buffer pointer
 * @param [in]      len;     data length
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  Read/write the raa489300 register
 */
int amd_crb_drivers_raa489300_access (bool isRead, uint8_t address, uint8_t reg, uint8_t* pBuf, uint8_t len)
{
	uint8_t retry = 3;
	int ret = 0;

	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			/* I2C read */
			ret = i2c_hub_burst_read(TPS_I2C_PORT, address, reg, pBuf, len);
		} else {
			/* I2C write */
			ret = i2c_hub_burst_write(TPS_I2C_PORT, address, reg, pBuf, len);
		}

		if (ret == 0)
			return ret;
	}
	
	if (!retry) {
#if (CONFIG_ECDBGI_SUPPORT)			
		info_value_increase(INFO_I2C_RAA489300, 1);
#endif		
		LOG_DBG("[!!Fatal error!!] on %s RAA489300[%02X], ret %d", isRead ? "R" : "W", reg, ret);
	}
	
	return ret;
}

/**
 * amd_crb_drivers_raa489300_ready
 *
 * @return true if the raa489300 is ready.
 * @return false if the raa489300 is not ready.
 *
 * @note
 *  check if the raa489300 is ready
 */
bool amd_crb_drivers_raa489300_ready(void)
{
	uint8_t address = (0x94 >> 1);
	uint8_t data[2] = { 0x00, 0x00};
	int ret = 0;
	
	ret = amd_crb_drivers_raa489300_access( 1, address, RAA489300_REG_OUTPUT_CURRENT_LIMIT, &data[0], 2);
	
	if (ret == 0)
		return true;
	else
		return false;
}

/**
 * amd_crb_drivers_raa489300_init
 *
 * @param [in]    tbIdx;     1-> 20~28V table; 0-> 48V table
 *
 * @note
 *  init the raa489300 register
 */
void amd_crb_drivers_raa489300_init(uint8_t tbIdx)
{
	uint8_t data[2] = { 0x00, 0x00};
	uint8_t address = (0x94 >> 1);

	if (tbIdx == 0) {
		/* 0x14 -> 0x2F80 */
		data[0] = 0x80; data[1] = 0x2F;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_OUTPUT_CURRENT_LIMIT, &data[0], 2);
		
		/* 0x15 -> 0x48E0 */
		data[0] = 0xE0; data[1] = 0x48;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_OUTPUT_OUTPUT_VOLTAGE, &data[0], 2);
		
		/* 0x39 -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL0, &data[0], 2);
		
		/* 0x3C -> 0x0004 */
		data[0] = 0x04; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL1, &data[0], 2);
		
		/* 0x3D -> 0x0300 */
		data[0] = 0x00; data[1] = 0x03;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL2, &data[0], 2);
		
		/* 0x3F -> 0x1388 */
		data[0] = 0x88; data[1] = 0x13;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_INPUT_CURRENT_LIMIT, &data[0], 2);
		
		/* 0x40 -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_VIN_OK_REF, &data[0], 2);
		
		/* 0x43 -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL6, &data[0], 2);
		
		/* 0x49 -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_REVERSE_PTM_VOLTAGE, &data[0], 2);
		
		/* 0x4B -> 0x4E00 */
		data[0] = 0x00; data[1] = 0x4E;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_MINI_INPUT_VOLTAGE, &data[0], 2);
		
		/* 0x4C -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL3, &data[0], 2);
		
		/* 0x4E -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL4, &data[0], 2);
		
		/* 0x4F -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL5, &data[0], 2);
	} else if (tbIdx == 1) {
		/* 0x14 -> 0x2EE0 */
		data[0] = 0xE0; data[1] = 0x2E;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_OUTPUT_CURRENT_LIMIT, &data[0], 2);
		
		/* 0x15 -> 0x3600 */
		data[0] = 0x00; data[1] = 0x36;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_OUTPUT_OUTPUT_VOLTAGE, &data[0], 2);
		
		/* 0x39 -> 0x0002 */
		data[0] = 0x02; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL0, &data[0], 2);
		
		/* 0x3C -> 0x0004 */
		data[0] = 0x04; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL1, &data[0], 2);
		
		/* 0x3D -> 0x0300 */
		data[0] = 0x00; data[1] = 0x03;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL2, &data[0], 2);
		
		/* 0x3F -> 0x1388 */
		data[0] = 0x88; data[1] = 0x13;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_INPUT_CURRENT_LIMIT, &data[0], 2);
		
		/* 0x40 -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_VIN_OK_REF, &data[0], 2);
		
		/* 0x43 -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL6, &data[0], 2);
		
		/* 0x49 -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_REVERSE_PTM_VOLTAGE, &data[0], 2);
		
		/* 0x4B -> 0x3E00 */
		data[0] = 0x00; data[1] = 0x3E;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_MINI_INPUT_VOLTAGE, &data[0], 2);
		
		/* 0x4C -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL3, &data[0], 2);
		
		/* 0x4E -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL4, &data[0], 2);
		
		/* 0x4F -> 0x0000 */
		data[0] = 0x00; data[1] = 0x00;
		amd_crb_drivers_raa489300_access( 0, address, RAA489300_REG_CONTROL5, &data[0], 2);
	}
}