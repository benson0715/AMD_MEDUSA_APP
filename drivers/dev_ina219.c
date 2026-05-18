/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_ina219.h"
#include <zephyr/logging/log.h>
#include "board_config.h"
#include <dev_utility.h>
#include "i2c_hub.h"
#include "debug_info.h"
LOG_MODULE_REGISTER(INA219, CONFIG_INA219_LOG_LEVEL);

#ifndef INA219_I2C_PORT
#define INA219_I2C_PORT I2C_2
#endif

#ifndef INA219_I2C_ADDRESS
#define INA219_I2C_ADDRESS   0x40
#endif

#ifndef LOG_DEV_INA219
#define LOG_DEV_INA219       1
#endif

#define _SWAP_16b(x)    ((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))

uint16_t ina219_calValue = 0;
uint32_t ina219_current_LSB_uA = 200;
uint32_t ina219_power_LSB_mW = 4;

/**
 * @brief Access INA219 register via I2C with retry mechanism
 */
uint32_t dev_ina219_regAccess(bool isRead, uint8_t reg, void * pData, uint8_t dataLen)
{
	uint8_t retry = 3;
	uint32_t ret;

	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			ret = i2c_hub_burst_read(INA219_I2C_PORT, INA219_I2C_ADDRESS, reg, pData, dataLen);
			if (ret != 0)
				continue;
		} else {
			ret = i2c_hub_burst_write(INA219_I2C_PORT, INA219_I2C_ADDRESS, reg, pData, dataLen);
		}
#if (CONFIG_LOG)
		switch (dataLen) {
			case 1:
				LOG_DBG("Acc INA219	%s [%02X], a(%d), u8  (0x%02X, %d)", isRead ? "R" : "W", reg, ret, *((uint8_t *)pData), *((uint8_t *)pData));
				break;
			case 2:
				LOG_DBG("Acc INA219	%s [%02X], a(%d), u16 (0x%04X, %d)", isRead ? "R" : "W", reg, ret, *((uint16_t *)pData), *((uint16_t *)pData));
				break;
			case 4:
				LOG_DBG("Acc INA219	%s [%02X], a(%d), u32 (0x%08X, %d)", isRead ? "R" : "W", reg, ret, *((uint32_t *)pData), *((uint32_t *)pData));
				break;
			default:
				LOG_CLEARBUF;
				for ( uint32_t i = 0; i < dataLen; i++ ) {
					LOG_APPEND(" %02X", *((uint8_t *)pData + i));
				}
				LOG_DBG("Acc INA219	%s [%02X], a(%d), len %d, buf %s", isRead ? "R" : "W", reg, ret, dataLen, LOG_BUF);
				break;
		}
#endif
		if (ret == 0)
				return ret;
	}
	if (!retry) {
		info_value_increase(INFO_I2C_INA219, 1);
		LOG_ERR("[!!Fatal error!!] on %s INA219x[%02X], ret %d", isRead ? "R" : "W", reg, ret);
	}

	return ret;
}

/**
 * @brief Calibrate INA219 based on shunt resistor value
 */
void dev_ina219_calibrate(DEV_INA219_EM_RSHUNT rshunt)
{
	uint16_t u16Cfg = 0;

	if (DEV_INA219_10_mOHM == rshunt) {
		ina219_calValue = 0x5000;
		ina219_current_LSB_uA = 200;     // Current LSB = 500uA per bit
		ina219_power_LSB_mW = 4;         // Power LSB = 10mW per bit = 20 * Current LSB
  
		u16Cfg = ( INA219_CFG_BVOLT_RANGE_32V | INA219_CFG_SVOLT_RANGE_40MV | INA219_CFG_BADCRES_12BIT_64S_34MS | INA219_CFG_SADCRES_12BIT_64S_34MS | INA219_CFG_MODE_SANDBVOLT_CONTINUOUS );
	} else if (DEV_INA219_100_mOHM == rshunt) {
		ina219_calValue = 0x333;
		ina219_current_LSB_uA = 500;     // Current LSB = 500uA per bit
		ina219_power_LSB_mW = 10;        // Power LSB = 10mW per bit = 20 * Current LSB
 
		u16Cfg = ( INA219_CFG_BVOLT_RANGE_32V | INA219_CFG_SVOLT_RANGE_320MV | INA219_CFG_BADCRES_12BIT_64S_34MS | INA219_CFG_SADCRES_12BIT_64S_34MS | INA219_CFG_MODE_SANDBVOLT_CONTINUOUS );
	} else {
		return;
	}

	ina219_calValue = _SWAP_16b(ina219_calValue);
	u16Cfg = _SWAP_16b(u16Cfg);
	dev_ina219_regAccess(0, INA219_REG_CALIBRATION, &ina219_calValue, 2);
	dev_ina219_regAccess(0, INA219_REG_CONFIG, &u16Cfg, 2);
}

/**
 * @brief Get bus voltage in millivolts
 */
uint32_t dev_ina219_GetVoltage_mV()
{
	uint16_t tmp = 0;
	uint32_t ret = 0;

	dev_ina219_regAccess(0, INA219_REG_CALIBRATION, &ina219_calValue, 2);
	dev_ina219_regAccess(1, INA219_REG_BUSVOLTAGE, &tmp, 2);

	tmp = _SWAP_16b(tmp);
	// R-shift 3 to drop CNVR and OVF
	ret = (tmp >> 3) << 2;

	return ret;
}

/**
 * @brief Get current in microamperes
 */
uint32_t dev_ina219_GetCurrent_uA()
{
	uint16_t tmp = 0;
	uint32_t ret = 0;

	dev_ina219_regAccess(0, INA219_REG_CALIBRATION, &ina219_calValue, 2);
	dev_ina219_regAccess(1, INA219_REG_CURRENT, &tmp, 2);

	tmp = _SWAP_16b(tmp);
	ret = tmp * ina219_current_LSB_uA;

	return ret;
}

/**
 * @brief Get power in milliwatts
 */
uint32_t dev_ina219_GetPower_mW()
{
	uint16_t tmp = 0;
	uint32_t ret = 0;

	dev_ina219_regAccess(0, INA219_REG_CALIBRATION, &ina219_calValue, 2);
	dev_ina219_regAccess(1, INA219_REG_POWER, &tmp, 2);

	tmp = _SWAP_16b(tmp);
	ret = tmp * ina219_power_LSB_mW;

	return ret;
}