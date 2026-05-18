/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_smtbty.h"
#include <assert.h>
#include <string.h>
#include <dev_utility.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "i2c_hub.h"
#include "debug_info.h"
LOG_MODULE_REGISTER(dev_smtbty, CONFIG_EC_LOG_LEVEL);
#ifndef BATTERY_I2C_PORT
#define BATTERY_I2C_PORT I2C_0
#endif
static uint16_t _s_u16ReservedBtyCapacity = 0;

DEV_SMTBTY_I2C_INTERFACE_ERROR_COUNT _s_smtbty_i2c_bus_errCount[5];
DEV_SMTBTY_REG_ITEM * _s_platform_btyBufTable[MAX_BATTERY_SUPPORTED] = {NULL};
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*            Ratio -------------++  +----- 1: Reserved for PEC             */
/*  00 1:1, 01 1:2, 10 2:1       ||  |+---- 1: is data available            */
/*  00  disable,  01 Rd only ---+||  ||+--- 1: auto-refersh                 */
/*  10  Wt only,  11 Rd/Wt   --+|||  |||+-- 1: Reserved                     */
/*                             ||||  ||||                                   */
/*          AccessCtrl = c8bit(xxxx, xxxx)  x = 0, 1 (Binary format)        */
/*                                                                          */
/*--------------------------------------------------------------------------*/

static DEV_SMTBTY_REG_ITEM _s_battery0[] = {

	/* Register,                           value,     Reload, Counter,   AccessCtrl         */
	/*                                 {width, data}    (MS)                                  */
	{DEV_SMTBTY_REG_ManufacturerAccess      ,  { 2, {0} },  8000  ,       0,  {c8bit(0100, 0010)} },  // 0x00 -
	{DEV_SMTBTY_REG_RemainingCapacityAlarm  ,  { 2, {0} },  3000  ,       0,  {c8bit(0100, 0010)} },  // 0x01 - Low Capacity alarm threshold value.
	{DEV_SMTBTY_REG_RemainingTimeAlarm      ,  { 2, {0} },  3000  ,       0,  {c8bit(0100, 0010)} },  // 0x02 - Remaining Time alarm value.
#if APP_SMTBTY_CAPACITY_MODE_10MWH
	// Set Bit 7:6 to 11 to enable Read/Write
	{DEV_SMTBTY_REG_BatteryMode             ,  { 2, {0} },  3000  ,       0,  {c8bit(1100, 0010)} },  // 0x03 - battery operational modes
#else
	{DEV_SMTBTY_REG_BatteryMode             ,  { 2, {0} },  3000  ,       0,  {c8bit(0100, 0010)} },  // 0x03 - battery operational modes
#endif
	{DEV_SMTBTY_REG_AtRate                  ,  { 2, {0} },  3000  ,       0,  {c8bit(0100, 0010)} },  // 0x04 -
	{DEV_SMTBTY_REG_AtRateTimeToFull        ,  { 2, {0} },  3000  ,       0,  {c8bit(0100, 0010)} },  // 0x05 -
	{DEV_SMTBTY_REG_AtRateTimeToEmpty       ,  { 2, {0} },  3000  ,       0,  {c8bit(0100, 0010)} },  // 0x06 -
	{DEV_SMTBTY_REG_AtRateOK                ,  { 2, {0} },  3000  ,       0,  {c8bit(0100, 0010)} },  // 0x07 -
	{DEV_SMTBTY_REG_Temperature             ,  { 2, {0} },  500   ,       0,  {c8bit(0100, 0010)} },  // 0x08 - Get cell temperature (word 0.1 degK)
	{DEV_SMTBTY_REG_Voltage                 ,  { 2, {0} },  500   ,       0,  {c8bit(0100, 0010)} },  // 0x09 - Get cell pack voltage (word mV)
	{DEV_SMTBTY_REG_Current                 ,  { 2, {0} },  500   ,       0,  {c8bit(0100, 0010)} },  // 0x0A - Get cell pack current (word mA)
	{DEV_SMTBTY_REG_AverageCurrent          ,  { 2, {0} },  500   ,       0,  {c8bit(0100, 0010)} },  // 0x0B - Get cell avgk current (word mA)
	{DEV_SMTBTY_REG_MaxError                ,  { 2, {0} },  500   ,       0,  {c8bit(0100, 0010)} },  // 0x0C - Get expected margin of error in charge calculation (%)
	{DEV_SMTBTY_REG_RelativeStateOfCharge   ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0010)} },  // 0x0D - Get cell relative state of charge
	{DEV_SMTBTY_REG_AbsoluteStateOfCharge   ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0010)} },  // 0x0E -
	{DEV_SMTBTY_REG_RemainingCapacity       ,  { 2, {0} },  800   ,       0,  {c8bit(0100, 0010)} },  // 0x0F - Get remaining capacity (word mAh)
	{DEV_SMTBTY_REG_FullChargeCapacity      ,  { 2, {0} },  8000  ,       0,  {c8bit(0100, 0010)} },  // 0x10 - Get full charge capacity (word mAh)
	{DEV_SMTBTY_REG_RunTimeToEmpty          ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0010)} },  // 0x11 - Runtime left (word minutes)
	{DEV_SMTBTY_REG_AverageTimeToEmpty      ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0010)} },  // 0x12 - Avg time left (word minutes)
	{DEV_SMTBTY_REG_AverageTimeToFull       ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0010)} },  // 0x13 - Avg time to charge (word minutes)
	{DEV_SMTBTY_REG_ChargingCurrent         ,  { 2, {0} },  100   ,       0,  {c8bit(0100, 0010)} },  // 0x14 - Requested charge current (word mA)
	{DEV_SMTBTY_REG_ChargingVoltage         ,  { 2, {0} },  100   ,       0,  {c8bit(0100, 0010)} },  // 0x15 - Requested charge current (word mV)
	{DEV_SMTBTY_REG_BatteryStatus           ,  { 2, {0} },  100   ,       0,  {c8bit(0100, 0010)} },  // 0x16 - Battery status flags (word)
	{DEV_SMTBTY_REG_CycleCount              ,  { 2, {0} },  20000 ,       0,  {c8bit(0100, 0010)} },  // 0x17 - # of cycles the battery has experienced
	{DEV_SMTBTY_REG_DesignCapacity          ,  { 2, {0} },  20000 ,       0,  {c8bit(0100, 0010)} },  // 0x18 - Designed capacity (word mAh)
	{DEV_SMTBTY_REG_DesignVoltage           ,  { 2, {0} },  20000 ,       0,  {c8bit(0100, 0010)} },  // 0x19 - Designed voltage (word mV)
	{DEV_SMTBTY_REG_SpecificationInfo       ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0010)} },  // 0x1A - Designed voltage (word mV)
	{DEV_SMTBTY_REG_ManufactureDate         ,  { 2, {0} },  60000 ,       0,  {c8bit(0100, 0000)} },  // 0x1B - Manufacture date
	{DEV_SMTBTY_REG_SerialNumber            ,  { 2, {0} },  60000 ,       0,  {c8bit(0100, 0000)} },  // 0x1C - Serial number
	{DEV_SMTBTY_REG_ManufacturerName        ,  {32, {0} },  3600000,      0,  {c8bit(0100, 0000)} },  // 0x20 - Manufacturer name
	{DEV_SMTBTY_REG_DeviceName              ,  {10, {0} },  3600000,      0,  {c8bit(0100, 0000)} },  // 0x21 - Device name
	{DEV_SMTBTY_REG_DeviceChemistry         ,  {16, {0} },  3600000,      0,  {c8bit(0100, 0000)} },  // 0x22 - Device chemistry (4 character string)
	{DEV_SMTBTY_REG_ManufacturerData        ,  {32, {0} },  3600000,      0,  {c8bit(0100, 0000)} },  // 0x23 - Manufacturer proprietary data
	{DEV_SMTBTY_REG_OptionalMfgFunction5    ,  { 2, {0} },  0     ,       0,  {c8bit(0100, 0000)} },  // 0x2F -
	{DEV_SMTBTY_REG_OptionalMfgFunction4    ,  { 2, {0} },  0     ,       0,  {c8bit(0100, 0000)} },  // 0x3C -
	{DEV_SMTBTY_REG_OptionalMfgFunction3    ,  { 2, {0} },  0     ,       0,  {c8bit(0100, 0000)} },  // 0x3D -
	{DEV_SMTBTY_REG_OptionalMfgFunction2    ,  { 2, {0} },  0     ,       0,  {c8bit(0100, 0000)} },  // 0x3E -
	{DEV_SMTBTY_REG_OptionalMfgFunction1    ,  { 2, {0} },  0     ,       0,  {c8bit(0100, 0000)} },  // 0x3F -
	/*------------------------------------------*/
	{DEV_SMTBTY_REG_INVALID             ,  { 0, {0} },  0     ,       0,  {c8bit(0000, 0000)} },  // Ending

};
DEV_SMTBTY_REG_ITEM * _s_batBuf[MAX_BATTERY_SUPPORTED] = {
	_s_battery0,
#if (MAX_BATTERY_SUPPORTED > 1)
	NULL,
#endif
#if (MAX_BATTERY_SUPPORTED > 2)
	NULL,
#endif
};

/**
 * @brief Perform I2C register access for smart battery communication
 * @param isRead True for read operation, false for write operation
 * @param slaveAddr I2C slave address of the battery
 * @param reg Register address to access
 * @param pBuf Pointer to data buffer
 * @param len Length of data to read/write
 * @return True if operation succeeded, false otherwise
 */
static bool _dev_smtbty_regAccess(bool isRead, uint8_t slaveAddr, uint8_t reg, void * pBuf, uint32_t len)
{
	bool isSuccess = false;
	uint8_t retry = 1;
	uint32_t ret;

	while (retry) {
		retry --;
		ret = 0;
		if (len <= 4) {
			if (isRead) {
                ret = i2c_hub_burst_read(BATTERY_I2C_PORT, slaveAddr, reg, pBuf, len);
			} else {
                ret = i2c_hub_burst_write(BATTERY_I2C_PORT, slaveAddr, reg, pBuf, len);
			}
#if (CONFIG_LOG)
			switch (len) {
				case 1:
					LOG_DBG("%s [%02X %02X], a(%d), dw(%d) 0x%02X | %d", isRead ? "R" : "W", 
						slaveAddr, reg, ret, len, *(uint8_t *)pBuf, *(uint8_t *)pBuf);
					break;
				case 2:
					LOG_DBG("%s [%02X %02X], a(%d), dw(%d) 0x%04X | %d", isRead ? "R" : "W", 
						slaveAddr, reg, ret, len, *(uint16_t *)pBuf, *(uint16_t *)pBuf);
					break;
				case 4:
					LOG_DBG("%s [%02X %02X], a(%d), dw(%d) 0x%08X | %d", isRead ? "R" : "W", 
						slaveAddr, reg, ret, len, *(uint32_t *)pBuf, *(uint32_t *)pBuf);
					break;
				default:
					LOG_CLEARBUF;
					for ( uint32_t i = 0; i < len; i++ ) {
						LOG_APPEND(" %02X", *((uint8_t *)pBuf + i));
					}
					LOG_DBG("%s [%02X %02X], a(%d), dw(%d)%s", isRead ? "R" : "W", slaveAddr, reg, ret, len, LOG_BUF);
					break;
			}
#endif
		} else if (isRead) {
            ret = i2c_hub_burst_read(BATTERY_I2C_PORT, slaveAddr, reg, pBuf, len);
#if (CONFIG_LOG)
			LOG_CLEARBUF;
			for ( uint32_t i = 0; i < len; i++ ) {
				LOG_APPEND(" %02X", *((uint8_t *)pBuf + i));
			}
			LOG_DBG("%s [%02X %02X], a(%d), dw(%d)%s", isRead ? "R" : "W", slaveAddr, reg, ret, len, LOG_BUF);
#endif
		} else {
			//
			// SMBus write block protocol
			//
			assert(0);
			// not supported for now
		}

        if (ret == 0)
        {
            isSuccess = true; // for block access, the required bytes may not equal to the real numbers can be read.
			break;
        }
    }

	_dev_smtbty_updateBusErrorCounter(slaveAddr, isSuccess);
	if (!isSuccess) {
		info_value_increase(INFO_I2C_SMTBAT, 1);
	}

	return isSuccess;
}

/**
 * @brief Override cached battery data with reserved capacity adjustments
 * @param btyId Battery identifier
 * @param reg Register address being accessed
 * @param pVal Pointer to value to be modified
 */
void _dev_smtbty_cacheDataOverride(uint8_t btyId, uint8_t reg, void * pVal)
{
	switch (reg) {
		case DEV_SMTBTY_REG_RemainingCapacity:
		case DEV_SMTBTY_REG_FullChargeCapacity:
		case DEV_SMTBTY_REG_DesignCapacity:
		{
			uint16_t u16Data = *(uint16_t *)pVal;
			u16Data = u16Data > dev_smtbty_getRsvdBtyCapacity() ? u16Data - dev_smtbty_getRsvdBtyCapacity() : 0;

			*(uint16_t *)pVal = u16Data;
			break;
		}
		default:
			break;
	}
}

/**
 * @brief Get cached 32-bit integer value from battery register
 * @param btyId Battery identifier
 * @param reg Register address to read
 * @param pVal Pointer to store the read value
 * @return True if cache data is available, false otherwise
 */
bool dev_smtbty_i32CacheGet(uint8_t btyId, uint8_t reg, void * pVal)
{
	uint32_t u32Val = 0;
	uint8_t width = 1;
	bool isCacheAvailable = false;
 	DEV_SMTBTY_REG_ITEM * pBtyReg = dev_smtbty_findRegItem(btyId, reg);
	if (pBtyReg) {
		width = pBtyReg->data.width;
		if (pBtyReg->access.f.dataAvailable) {
			k_mutex_lock(&pBtyReg->rwLock,K_FOREVER);
			/* use the latest value */
			switch (pBtyReg->data.width) {
				case 1:  u32Val = (uint32_t)pBtyReg->data.value.u8[0];   break;
				case 2:  u32Val = (uint32_t)pBtyReg->data.value.u16[0];  break;
				case 4:  u32Val = pBtyReg->data.value.u32;               break;
				default:                                                 break;
			}
			if (pBtyReg->access.f.dataAvailable) {
				isCacheAvailable = true;
			} else {
				u32Val = 0;
			}
			k_mutex_unlock(&pBtyReg->rwLock);
		}
	}

	if (!isCacheAvailable && pBtyReg && width <= 4 && pBtyReg->data.lastSuccessValue.u32 != 0xFFFFFFFF) {
		/* use the last value */
		u32Val = pBtyReg->data.lastSuccessValue.u32;
		isCacheAvailable = true;
	}

	if (pVal && isCacheAvailable && width <= 4) {
		switch (width) {
			case 1: *(uint8_t *) pVal = (uint8_t) u32Val;  break;
			case 2: *(uint16_t *)pVal = (uint16_t)u32Val;  break;
			case 4: *(uint32_t *)pVal =           u32Val;  break;
			default:                                       return false;
		}

		_dev_smtbty_cacheDataOverride(btyId, reg, pVal);

		return true;
	}

	return false;
}

/**
 * @brief Get cached block data from battery register
 * @param btyId Battery identifier
 * @param reg Register address to read
 * @param pBuf Pointer to buffer to store the data
 * @param size Size of buffer in bytes
 * @return True if cache data is available, false otherwise
 */
bool dev_smtbty_blockCacheGet(uint8_t btyId, uint8_t reg, void * pBuf, uint8_t size)
{
	if (!pBuf)
		return false;

	bool isCacheAvailable = false;
	uint8_t copyLength = 0;
 	DEV_SMTBTY_REG_ITEM * pBtyReg = dev_smtbty_findRegItem(btyId, reg);
	if (pBtyReg && pBtyReg->access.f.dataAvailable) {
		k_mutex_lock(&pBtyReg->rwLock,K_FOREVER);
		if (pBtyReg->access.f.dataAvailable) {
			copyLength = (size < pBtyReg->data.width) ? size : (pBtyReg->data.width);
			if (copyLength)
				memcpy (pBuf, pBtyReg->data.value.pBuf, copyLength);

			isCacheAvailable = true;
		}
		k_mutex_unlock(&pBtyReg->rwLock);
	}

	if (pBuf && copyLength && isCacheAvailable) {
		return true;
	}

	return false;
}

/**
 * @brief Read 32-bit integer value from battery register
 * @param btyId Battery identifier
 * @param reg Register address to read
 * @param pVal Pointer to store the read value
 * @return True if read succeeded, false otherwise
 */
bool dev_smtbty_i32Read(uint8_t btyId, uint8_t reg, void * pVal)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);

	bool isSuccess = false;
	DEV_SMTBTY_REG_ITEM * pBtyReg;
	pBtyReg = dev_smtbty_findRegItem(btyId, reg);
	if(pBtyReg->data.width <= 4 && dev_smtbty_autoRead(pBtyReg, DEV_SMTBTY_ID_TO_PORT(btyId), BATTERY_I2C_ADDRESS)) {
		isSuccess = dev_smtbty_i32CacheGet(btyId, reg, pVal);
	}

	return isSuccess;
}

/**
 * @brief Write 32-bit integer value to battery register
 * @param btyId Battery identifier
 * @param reg Register address to write
 * @param val Value to write
 * @return True if write succeeded, false otherwise
 */
bool dev_smtbty_i32Write(uint8_t btyId, uint8_t reg, int32_t val)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);

	DEV_SMTBTY_REG_ITEM * pBtyReg;
	pBtyReg = dev_smtbty_findRegItem(btyId, reg);
	if (!pBtyReg || pBtyReg->access.f.accessType == DEV_SMTBTY_REG_READ_ONLY || pBtyReg->access.f.accessType == DEV_SMTBTY_REG_DISABLED || pBtyReg->data.width > 4)
		return false;

	bool isSuccess = false;
	k_mutex_lock(&pBtyReg->rwLock,K_FOREVER);
	if (DEV_SMTBTY_RATIO_APP_1_REG_0P5 == pBtyReg->access.f.ratio) {
		LOG_INF("**ratio_APP:REG = 2.0**, WRITE BAT: %02X, RAW %04X (%d), convert to %04X (%d)", reg, val, val, val >> 1, val >> 1);
		val >>= 1;
	} else if (DEV_SMTBTY_RATIO_APP_0P5_REG_1 == pBtyReg->access.f.ratio) {
		LOG_INF("**ratio_APP:REG = 0.5**, WRITE BAT: %02X, RAW %04X (%d), convert to %04X (%d)", reg, val, val, val << 1, val << 1);
		val <<= 1;
	} else if (DEV_SMTBTY_RATIO_APP_1_REG_0P25 == pBtyReg->access.f.ratio) {
		LOG_INF("**ratio_APP:REG = 4.0**, WRITE BAT: %02X, RAW %04X (%d), convert to %04X (%d)", reg, val, val, val >> 2, val >> 2);
		val >>= 2;
	}
	DEV_SMTBTY_SWITCH_TO_PORT(DEV_SMTBTY_ID_TO_PORT(btyId));
	isSuccess = _dev_smtbty_regAccess(false, BATTERY_I2C_ADDRESS, reg, (void *)&val, pBtyReg->data.width);
	pBtyReg->access.f.dataAvailable = 0;
	pBtyReg->u32refreshCounter = 0;
	k_mutex_unlock(&pBtyReg->rwLock);

	return isSuccess;
}

/**
 * @brief Read block data from battery register
 * @param btyId Battery identifier
 * @param reg Register address to read
 * @param pVal Pointer to buffer to store the data
 * @param size Size of buffer in bytes
 * @return True if read succeeded, false otherwise
 */
bool dev_smtbty_blockRead(uint8_t btyId, uint8_t reg, void * pVal, uint8_t size)
{
	assert (btyId < MAX_BATTERY_SUPPORTED);

	bool isSuccess = false;
	DEV_SMTBTY_REG_ITEM * pBtyReg;
	pBtyReg = dev_smtbty_findRegItem(btyId, reg);
	if(dev_smtbty_autoRead(pBtyReg, DEV_SMTBTY_ID_TO_PORT(btyId), BATTERY_I2C_ADDRESS)) {
		if (!pVal && !size)
			isSuccess = true;
		else
			isSuccess = dev_smtbty_blockCacheGet(btyId, reg, pVal, size);
	}

	return isSuccess;
}

/**
 * @brief Automatically read battery register and update cache
 * @param pItem Pointer to register item structure
 * @param port I2C port number
 * @param slaveAddr I2C slave address
 * @return True if data is available, false otherwise
 */
bool dev_smtbty_autoRead(DEV_SMTBTY_REG_ITEM * pItem, uint8_t port, uint8_t slaveAddr)
{
	if (!pItem)
		return false;

	if (pItem->access.f.accessType == DEV_SMTBTY_REG_WRITE_ONLY || pItem->access.f.accessType == DEV_SMTBTY_REG_DISABLED)
		return false;

	uint8_t * pData = (uint8_t *)(&pItem->data.value);
	if (pItem->data.width > 4) {
		pData = pItem->data.value.pBuf;
	}

	if (pData) {
		k_mutex_lock(&pItem->rwLock,K_FOREVER);

		DEV_SMTBTY_SWITCH_TO_PORT(port);
		pItem->access.f.dataAvailable = _dev_smtbty_regAccess(true, slaveAddr, pItem->reg, pData, pItem->data.width);
		if (pItem->access.f.dataAvailable && pItem->data.width <= 4) {
			if (DEV_SMTBTY_RATIO_APP_1_REG_0P5 == pItem->access.f.ratio) {
				LOG_INF("**ratio_APP:REG = 2.0**, Read  REG: %02X, RAW %04X (%d), convert to %04X (%d)", pItem->reg, pItem->data.value.u32, pItem->data.value.u32, pItem->data.value.u32 << 1, pItem->data.value.u32 << 1);
				pItem->data.value.u32 <<= 1;
			} else if (DEV_SMTBTY_RATIO_APP_0P5_REG_1 == pItem->access.f.ratio) {
				LOG_INF("**ratio_APP:REG = 0.5**, Read  REG: %02X, RAW %04X (%d), convert to %04X (%d)", pItem->reg, pItem->data.value.u32, pItem->data.value.u32, pItem->data.value.u32 >> 1, pItem->data.value.u32 >> 1);
				pItem->data.value.u32 >>= 1;
			} else if (DEV_SMTBTY_RATIO_APP_1_REG_0P25 == pItem->access.f.ratio) {
				LOG_INF("**ratio_APP:REG = 4.0**, Read  REG: %02X, RAW %04X (%d), convert to %04X (%d)", pItem->reg, pItem->data.value.u32, pItem->data.value.u32, pItem->data.value.u32 << 2, pItem->data.value.u32 << 2);
				pItem->data.value.u32 <<= 2;
			}
			pItem->data.lastSuccessValue.u32 = pItem->data.value.u32;
		}

		/* Reload the counter, it means the data has just refershed */
		pItem->u32refreshCounter = pItem->u32refreshReload;

		k_mutex_unlock(&pItem->rwLock);
	}

	return pItem->access.f.dataAvailable;
}

/**
 * @brief Update I2C bus error counter for a slave device
 * @param slaveAddr I2C slave address
 * @param wasSuccess True if last operation succeeded, false otherwise
 */
void _dev_smtbty_updateBusErrorCounter(uint8_t slaveAddr, bool wasSuccess)
{
	if (slaveAddr) {
		for(uint8_t i = 0; i < sizeof(_s_smtbty_i2c_bus_errCount) / sizeof(DEV_SMTBTY_I2C_INTERFACE_ERROR_COUNT); i++) {
			if(0 == _s_smtbty_i2c_bus_errCount[i].slaveAddr || 
			   slaveAddr == _s_smtbty_i2c_bus_errCount[i].slaveAddr) {

				_s_smtbty_i2c_bus_errCount[i].slaveAddr = slaveAddr;
				if (!wasSuccess) {
					if (_s_smtbty_i2c_bus_errCount[i].continuousErrCount < 0xFFFFFF00)
						_s_smtbty_i2c_bus_errCount[i].continuousErrCount ++;
					if (_s_smtbty_i2c_bus_errCount[i].maxContinuousErrCount < _s_smtbty_i2c_bus_errCount[i].continuousErrCount)
						_s_smtbty_i2c_bus_errCount[i].maxContinuousErrCount = _s_smtbty_i2c_bus_errCount[i].continuousErrCount;
				} else {
					_s_smtbty_i2c_bus_errCount[i].continuousErrCount = 0;
				}

				return;
			}
		}
	}
}

/**
 * @brief Reset I2C bus error counter for a slave device
 * @param slaveAddr I2C slave address
 */
void dev_smtbty_resetBusErrorCounter(uint8_t slaveAddr)
{
	if (slaveAddr) {
		for(uint8_t i = 0; i < sizeof(_s_smtbty_i2c_bus_errCount) / sizeof(DEV_SMTBTY_I2C_INTERFACE_ERROR_COUNT); i++) {
			if(slaveAddr == _s_smtbty_i2c_bus_errCount[i].slaveAddr) {
				_s_smtbty_i2c_bus_errCount[i].continuousErrCount = 0;
				return;
			}
		}
	}
}

/**
 * @brief Get continuous error count for a slave device
 * @param slaveAddr I2C slave address
 * @return Continuous error count, or 0xFFFFFFFF if device not found
 */
uint32_t dev_smtbty_getContinuousErrorNum(uint8_t slaveAddr)
{
	if (slaveAddr) {
		for(uint8_t i = 0; i < sizeof(_s_smtbty_i2c_bus_errCount) / sizeof(DEV_SMTBTY_I2C_INTERFACE_ERROR_COUNT); i++) {
			if(slaveAddr == _s_smtbty_i2c_bus_errCount[i].slaveAddr) {
				return _s_smtbty_i2c_bus_errCount[i].continuousErrCount;
			}
		}
	}

	return 0xFFFFFFFF;
}

/**
 * @brief Find register item structure for a battery register
 * @param btyId Battery identifier
 * @param reg Register address to find
 * @return Pointer to register item, or NULL if not found
 */
DEV_SMTBTY_REG_ITEM * dev_smtbty_findRegItem(uint8_t btyId, uint8_t reg)
{
	if (btyId >= MAX_BATTERY_SUPPORTED || reg == DEV_SMTBTY_REG_INVALID)
		return NULL;

	if (_s_platform_btyBufTable[btyId]) {
		for (uint8_t i = 0; _s_platform_btyBufTable[btyId][i].reg != DEV_SMTBTY_REG_INVALID; i++) {
			if (_s_platform_btyBufTable[btyId][i].reg == reg && _s_platform_btyBufTable[btyId][i].access.f.accessType != DEV_SMTBTY_REG_DISABLED)
				return &_s_platform_btyBufTable[btyId][i];
		}
	}

	for (uint8_t i = 0; _s_batBuf[btyId][i].reg != DEV_SMTBTY_REG_INVALID; i++) {
		if (_s_batBuf[btyId][i].reg == reg && _s_batBuf[btyId][i].access.f.accessType != DEV_SMTBTY_REG_DISABLED)
			return &_s_batBuf[btyId][i];
	}

	return NULL;
}

/**
 * @brief Set reserved battery capacity value
 * @param u16Rsvd Reserved capacity in mAh
 */
void dev_smtbty_setRsvdBtyCapacity( uint16_t u16Rsvd )
{
	_s_u16ReservedBtyCapacity = u16Rsvd;
}

/**
 * @brief Get reserved battery capacity value
 * @return Reserved capacity in mAh
 */
uint16_t dev_smtbty_getRsvdBtyCapacity( void )
{
	return _s_u16ReservedBtyCapacity;
}