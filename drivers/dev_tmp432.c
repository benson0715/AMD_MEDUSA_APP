/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_tmp432.h"
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include <dev_utility.h>
#include "i2c_hub.h"
#include "debug_info.h"
LOG_MODULE_REGISTER(tmp432, CONFIG_TMP432_LOG_LEVEL);

#ifndef TMP432_I2C_PORT
#define TMP432_I2C_PORT I2C_0
#endif

#ifndef TMP432_I2C_ADDRESS
#define TMP432_I2C_ADDRESS (0x9A >> 1)
#endif

static bool g_isExtendedRangeEnabled = false;

/**
 * @brief Access TMP432 register via I2C with retry mechanism
 * @param isRead true for read operation, false for write operation
 * @param reg Register address to access
 * @param pData Pointer to data buffer
 * @param dataLen Length of data to read/write
 * @return 0 on success, non-zero on failure
 */
uint32_t dev_tmp432_regAccess(bool isRead, uint8_t reg, void * pData, uint8_t dataLen)
{
	uint8_t retry = 3;
	int32_t ret;

	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			ret = i2c_hub_burst_read(TMP432_I2C_PORT, TMP432_I2C_ADDRESS, reg, pData, dataLen);
			if (ret != 0)
				continue;
		} else {
			ret = i2c_hub_burst_write(TMP432_I2C_PORT, TMP432_I2C_ADDRESS, reg, pData, dataLen);
		}
		switch (dataLen) {
			case 1:
				LOG_DBG("Acc TMP432  %s [%02X], a(%d), u8  (0x%02X, %d)", isRead ? "R" : "W", reg, ret, *((uint8_t *)pData), *((uint8_t *)pData));
				break;
			case 2:
				LOG_DBG("Acc TMP432  %s [%02X], a(%d), u16 (0x%04X, %d)", isRead ? "R" : "W", reg, ret, *((uint16_t *)pData), *((uint16_t *)pData));
				break;
			case 4:
				LOG_DBG("Acc TMP432  %s [%02X], a(%d), u32 (0x%08X, %d)", isRead ? "R" : "W", reg, ret, *((uint32_t *)pData), *((uint32_t *)pData));
				break;
			default:
				LOG_CLEARBUF;
				for ( uint32_t i = 0; i < dataLen; i++ ) {
					LOG_APPEND(" %02X", *((uint8_t *)pData + i));
				}
				LOG_DBG("Acc TMP432  %s [%02X], a(%d), len %d, buf %s", isRead ? "R" : "W", reg, ret, dataLen, LOG_BUF);
				break;
		}
		if (ret == 0)
				return ret;
	}
	if (!retry) {
#if (CONFIG_ECDBGI_SUPPORT)			
		info_value_increase(INFO_I2C_TMP432, 1);
#endif		
		LOG_ERR("[!!Fatal error!!] on %s TMP432x[%02X], ret %d", isRead ? "R" : "W", reg, ret);
	}

	return ret;
}

/**
 * @brief Get integer part of temperature from TMP432 sensor
 * @param s Sensor selector (local, remote1, or remote2)
 * @return Temperature integer value, or DEV_TMP432_MAGIC_NUM on failure
 */
uint8_t dev_tmp432_getTempInt(DEV_TMP432_SENSOR_SELECTOR s)
{
	uint8_t ret = DEV_TMP432_MAGIC_NUM, Int; // 0xC8: magic number means the read to tmp432 was failed.
	if (0 == dev_tmp432_regAccess(true, ((uint32_t)s >> 8) & 0xFF, &Int, 1))
		ret = Int;

	return ret;
}

/**
 * @brief Get decimal part of temperature from TMP432 sensor
 * @param s Sensor selector (local, remote1, or remote2)
 * @return Temperature decimal value, or DEV_TMP432_MAGIC_NUM on failure
 */
uint8_t dev_tmp432_getTempDec(DEV_TMP432_SENSOR_SELECTOR s)
{
	uint8_t ret = DEV_TMP432_MAGIC_NUM, Dec; // 0xC8: magic number means the read to tmp432 was failed.
	if (0 == dev_tmp432_regAccess(true, (uint32_t)s & 0xFF, &Dec, 1))
		ret = Dec;

	return ret;
}

/**
 * @brief Read full temperature value from TMP432 sensor
 * @param s Sensor selector (local, remote1, or remote2)
 * @return 16-bit temperature value, or 0xFFFF on failure
 */
uint16_t dev_tmp432_readTemp(DEV_TMP432_SENSOR_SELECTOR s)
{
	uint16_t u16buf = 0;

	uint8_t u8idx;
	switch (s) {
		case DEV_TMP432_SENSOR__LOCAL:
		case DEV_TMP432_SENSOR__REMOTE1:
		case DEV_TMP432_SENSOR__REMOTE2:
			u8idx = (((uint32_t)s >> 8) & 0xFF);
			break;
		default:
			return 0xFFFF;
	}

	if (0 == dev_tmp432_regAccess(true, u8idx, &u16buf, 2)) {
		// The high byte is output first from TPM432, followed by the low byte.
		uint16_t ret = 0;
		ret  = ((u16buf & 0xFF) << 8);
		ret |= (u16buf >> 8) & 0xFF;

		return ret;
	}

	return 0xFFFF;
}

/**
 * @brief Enable extended temperature range mode (-55 to 150C)
 * @return true on success, false on failure
 */
bool dev_tmp432_enableExtendedRange()
{
	uint8_t u8val;
	if (0 == dev_tmp432_regAccess(true, 0x03, &u8val, 1)) {
		u8val |= c8bit(0000, 0100);  // bit 2, Temperature range; 0 = 0~127C, 1 = -55~150C
		if (0 == dev_tmp432_regAccess(false, 0x09, &u8val, 1)) {
			g_isExtendedRangeEnabled = true;
			return true;
		}
	}

	return false;
}

/**
 * @brief Disable extended temperature range mode (use 0 to 127C)
 * @return true on success, false on failure
 */
bool dev_tmp432_disableExtendedRange()
{
	uint8_t u8val;
	if (0 == dev_tmp432_regAccess(true, 0x03, &u8val, 1)) {
		u8val &= ~ c8bit(0000, 0100); // bit 2, Temperature range; 0 = 0~127C, 1 = -55~150C
		if (0 == dev_tmp432_regAccess(false, 0x09, &u8val, 1)) {
			g_isExtendedRangeEnabled = false;
			return true;
		}
	}

	return false;
}

/**
 * @brief Set resistance correction mode for TMP432
 * @param toEnable true to enable resistance correction, false to disable
 * @return true on success, false on failure
 */
bool dev_tmp432_setResistanceCorrection(bool toEnable)
{
	uint8_t u8Cfg2 = 0;

	/* !! must be set on every time after back to S0 !!
	 *
	 * Clear the RC bit.  Bit 2 of TMP432x3F.
	 * if RC = 0, resistance correction is disabled. Resistance correction must be enabled for most applications.
	 * However, disabling the resistance correction can yield slightly improved temperature measurement noise
	 * performance, and reduce conversion time by about 50%, which could lower power consumption when conversion
	 * rates of two per second or less are selected.
	 */
	if (0 != dev_tmp432_regAccess(true,	0x3F, &u8Cfg2, 1))
		return false;
	if (toEnable && (u8Cfg2 & 0x04) )
		return true;
	else
		u8Cfg2 |= 0x04;

	if ((!toEnable) && (u8Cfg2 & 0x04) )
		u8Cfg2 &= 0xFB;
	else
		return true;

	if (0 == dev_tmp432_regAccess(false, 0x3F, &u8Cfg2, 1)) {
		/*
		 * delay at least 200 MS to ensure setting is applied.
		 * there will be ~ 2C temp offset on the reading once this setting is changed.
		 */
		k_msleep(200);
		return true;
	}

	return false;
}

/* *******************************************
 * !!! NOTE !!! TtempFlt is float.
 *
 * Below function gets FPU be involved !!!
 *
 * Please be prudent of the execution context
 * so as to avoid stack corruption.
 *
 * *******************************************/
/**
 * @brief Convert TMP432 raw temperature value to float
 * @param u16Tmp432 Raw 16-bit temperature value from TMP432
 * @return Temperature in degrees Celsius as float
 */
float dev_tmp432_convert2Flt(uint16_t u16Tmp432)
{
	float f = 123.456;

	uint8_t high, low;
	high = (u16Tmp432 >> 8) & 0xFF;
	low  = (u16Tmp432 & 0xFF);

	// The resolution of TMP432 is 0.0625 C
	f = 1.0f * high + 0.0625f * ((low >> 4) & 0x0F);

	if (g_isExtendedRangeEnabled) {
		f -= 64.0f;
	}

	return f;
}