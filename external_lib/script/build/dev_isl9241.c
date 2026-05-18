/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_isl9241.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include <dev_utility.h>
#include <assert.h>
#include <i2c_hub.h>
#include <board_config.h>
#include "debug_info.h"
LOG_MODULE_REGISTER(isl9241, CONFIG_CHARGER_LOG_LEVEL);
K_MUTEX_DEFINE(_s_chg_port_access_Lock);

#ifndef CHARGER_I2C_PORT
#define CHARGER_I2C_PORT            I2C_0
#endif

#ifndef F_AC_ADAPTER_VIN_VOLTAGE
#define F_AC_ADAPTER_VIN_VOLTAGE    (19000)
#endif

static pfnSetupProchotGate_t pfnSetupProchotGate;
static struct k_work setProchot_work;

DEV_CHARGER_REG_ITEM * _s_platform_chgBufTable[MAX_CHARGER_SUPPORTED] = {NULL};

static DEV_CHARGER_REG_ITEM _s_charger0[] = {
	/* Register,                           value,    Reload, Counter,   AccessCtrl         */
	/*                                 {width, data}  (MS)                                  */
	{DEV_CHARGER_REG_ChargerSpecInfo         ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0000)} },  // 0x11 - read the charger's extended status bits.
	{DEV_CHARGER_REG_ChargerOption           ,  { 2, {0} },  500   ,       0,  {c8bit(1100, 0000)} },  // 0x12 - set the various charger modes.
	{DEV_CHARGER_REG_ChargerStatus           ,  { 2, {0} },  500   ,       0,  {c8bit(0100, 0000)} },  // 0x13 - read the charger's status bits
	{DEV_CHARGER_REG_ChargerChargingCurrent  ,  { 2, {0} },  500   ,       0,  {c8bit(1100, 0000)} },  // 0x14 - master device sends the desired charging rate (mA).
	{DEV_CHARGER_REG_ChargerChargingVoltage  ,  { 2, {0} },  1000  ,       0,  {c8bit(1100, 0000)} },  // 0x15 - master device sends the desired charging voltage
	{DEV_CHARGER_REG_AlarmWarning            ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0000)} },  // 0x16 - Battery, acting as a bus master device, sends the AlarmWarning() message
	/*--Non-Standard----------------------------*/
	{DEV_CHARGER_REG_InputCurrent            ,  { 2, {0} },  1000  ,       0,  {c8bit(1100, 0000)} },  // 0x3F - 6-Bit Input Current Setting
	{DEV_CHARGER_REG_ManufacturerID          ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0000)} },  // 0xFE - Manufacturer ID 0x0040
	{DEV_CHARGER_REG_DeviceID                ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0000)} },  // 0xFF - Device ID       0x0008
	/*------------------------------------------*/
	{DEV_CHARGER_REG_INVALID			   ,  { 0, {0} },      0 , 	  0,  {c8bit(0000, 0000)} },  // Ending
};

#if (MAX_CHARGER_SUPPORTED > 1)
static DEV_CHARGER_REG_ITEM _s_charger1[] = {
	/* Register,                           value,    Reload, Counter,   AccessCtrl         */
	/*                                          {width, data} (MS)                                  */
	{DEV_CHARGER_REG_ChargerSpecInfo         ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0000)} },  // 0x11 - read the charger's extended status bits.
	{DEV_CHARGER_REG_ChargerOption           ,  { 2, {0} },  500   ,       0,  {c8bit(1100, 0000)} },  // 0x12 - set the various charger modes.
	{DEV_CHARGER_REG_ChargerStatus           ,  { 2, {0} },  500   ,       0,  {c8bit(0100, 0000)} },  // 0x13 - read the charger's status bits
	{DEV_CHARGER_REG_ChargerChargingCurrent  ,  { 2, {0} },  500   ,       0,  {c8bit(1100, 0000)} },  // 0x14 - master device sends the desired charging rate (mA).
	{DEV_CHARGER_REG_ChargerChargingVoltage  ,  { 2, {0} },  1000  ,       0,  {c8bit(1100, 0000)} },  // 0x15 - master device sends the desired charging voltage
	{DEV_CHARGER_REG_AlarmWarning            ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0000)} },  // 0x16 - Battery, acting as a bus master device, sends the AlarmWarning() message
	/*--Non-Standard----------------------------*/
	{DEV_CHARGER_REG_InputCurrent            ,  { 2, {0} },  1000  ,       0,  {c8bit(1100, 0000)} },  // 0x3F - 6-Bit Input Current Setting
	{DEV_CHARGER_REG_ManufacturerID          ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0000)} },  // 0xFE - Manufacturer ID 0x0040
	{DEV_CHARGER_REG_DeviceID                ,  { 2, {0} },  1000  ,       0,  {c8bit(0100, 0000)} },  // 0xFF - Device ID       0x0008
	/*------------------------------------------*/
	{DEV_CHARGER_REG_INVALID			   ,  { 0, {0} },  	0 , 	  0,  {c8bit(0000, 0000)} },  // Ending

};
#endif

DEV_CHARGER_REG_ITEM * _s_chgBuf[MAX_CHARGER_SUPPORTED] = {
	_s_charger0,
#if (MAX_CHARGER_SUPPORTED > 1)
	_s_charger1,
#endif
#if (MAX_CHARGER_SUPPORTED > 2)
	_s_charger2,
#endif
//	... ...
};

static DEV_CHARGER_I2C_INTERFACE_ERROR_COUNT _s_charger_i2c_bus_errCount[5];

/**
 * @brief Update the I2C bus error counter for a specific slave address
 * @param slaveAddr The I2C slave address to track
 * @param wasSuccess True if the last operation was successful, false otherwise
 */
void _dev_charger_updateBusErrorCounter(uint8_t slaveAddr, bool wasSuccess)
{
	if (slaveAddr) {
		for(uint8_t i = 0; i < sizeof(_s_charger_i2c_bus_errCount) / sizeof(DEV_CHARGER_I2C_INTERFACE_ERROR_COUNT); i++) {
			if(0 == _s_charger_i2c_bus_errCount[i].slaveAddr || 
			   slaveAddr == _s_charger_i2c_bus_errCount[i].slaveAddr) {

				_s_charger_i2c_bus_errCount[i].slaveAddr = slaveAddr;
				if (!wasSuccess) {
					if (_s_charger_i2c_bus_errCount[i].continuousErrCount < 0xFFFFFF00)
						_s_charger_i2c_bus_errCount[i].continuousErrCount ++;
					if (_s_charger_i2c_bus_errCount[i].maxContinuousErrCount < _s_charger_i2c_bus_errCount[i].continuousErrCount)
						_s_charger_i2c_bus_errCount[i].maxContinuousErrCount = _s_charger_i2c_bus_errCount[i].continuousErrCount;
				} else {
					_s_charger_i2c_bus_errCount[i].continuousErrCount = 0;
				}

				return;
			}
		}
	}
}

/**
 * @brief Refresh a charger register by reading from I2C
 * @param reg Pointer to the register address
 * @param pBuf Pointer to buffer to store read data
 */
void dev_isl9241_reg_refresh(uint8_t * reg, void * pBuf)
{
	i2c_hub_write_read(CHARGER_I2C_PORT,CHARGER_I2C_ADDRESS,reg, 1, pBuf, 2);
}

static bool _dev_charger_regAccess(bool isRead, uint8_t slaveAddr, uint8_t reg, void * pBuf, uint32_t len)
{
	bool isSuccess = false;
	uint8_t retry = 1;
	uint32_t ret;

	while (retry) {
		retry --;
		ret = 0;
		if (len <= 4) {
			if (isRead) {
                ret = i2c_hub_burst_read(CHARGER_I2C_PORT, slaveAddr, reg, pBuf, len);
			} else {
                ret = i2c_hub_burst_write(CHARGER_I2C_PORT, slaveAddr, reg, pBuf, len);
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
            ret = i2c_hub_burst_read(CHARGER_I2C_PORT, slaveAddr, reg, pBuf, len);
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
	if (!isSuccess) {
		info_value_increase(INFO_I2C_CHARGER, 1);
	}
	_dev_charger_updateBusErrorCounter(slaveAddr, isSuccess);
	return isSuccess;
}

/**
 * @brief Automatically read a charger register item
 * @param pItem Pointer to the register item to read
 * @param port The charger port number
 * @param slaveAddr The I2C slave address
 * @return true if read was successful, false otherwise
 */
bool dev_isl9241_autoRead(DEV_CHARGER_REG_ITEM * pItem, uint8_t port, uint8_t slaveAddr)
{
	if (!pItem)
		return false;

	if (pItem->access.f.accessType == DEV_CHARGER_REG_WRITE_ONLY || pItem->access.f.accessType == DEV_CHARGER_REG_DISABLED)
		return false;

	uint8_t * pData = (uint8_t *)(&pItem->data.value);
	if (pItem->data.width > 4) {
		pData = pItem->data.value.pBuf;
	}

	if (pData) {
		k_mutex_lock(&_s_chg_port_access_Lock,K_FOREVER);

		DEV_CHARGER_SWITCH_TO_PORT(port);
		pItem->access.f.dataAvailable = _dev_charger_regAccess(true, slaveAddr, pItem->reg, pData, pItem->data.width);
		if (pItem->access.f.dataAvailable && pItem->data.width <= 4) {
			if (DEV_CHARGER_RATIO_APP_1_REG_0P5 == pItem->access.f.ratio) {
				LOG_INF("**ratio_APP:REG = 2.0**, Read  REG: %02X, RAW %04X (%d), convert to %04X (%d)", pItem->reg, pItem->data.value.u32, pItem->data.value.u32, pItem->data.value.u32 << 1, pItem->data.value.u32 << 1);
				pItem->data.value.u32 <<= 1;
			} else if (DEV_CHARGER_RATIO_APP_0P5_REG_1 == pItem->access.f.ratio) {
				LOG_INF("**ratio_APP:REG = 0.5**, Read  REG: %02X, RAW %04X (%d), convert to %04X (%d)", pItem->reg, pItem->data.value.u32, pItem->data.value.u32, pItem->data.value.u32 >> 1, pItem->data.value.u32 >> 1);
				pItem->data.value.u32 >>= 1;
			} else if (DEV_CHARGER_RATIO_APP_1_REG_0P25 == pItem->access.f.ratio) {
				LOG_INF("**ratio_APP:REG = 4.0**, Read  REG: %02X, RAW %04X (%d), convert to %04X (%d)", pItem->reg, pItem->data.value.u32, pItem->data.value.u32, pItem->data.value.u32 << 2, pItem->data.value.u32 << 2);
				pItem->data.value.u32 <<= 2;
			}
			pItem->data.lastSuccessValue.u32 = pItem->data.value.u32;
		}

		/* Reload the counter, it means the data has just refershed */
		pItem->u32refreshCounter = pItem->u32refreshReload;

		k_mutex_unlock(&_s_chg_port_access_Lock);
	}

	return pItem->access.f.dataAvailable;
}

/**
 * @brief Register a platform-specific charger table
 * @param chgId The charger ID
 * @param pTable Pointer to the platform-specific register table
 * @param chgBufTable Pointer to the charger buffer table array
 */
void dev_isl9241_register_platform_table(uint8_t chgId, DEV_CHARGER_REG_ITEM * pTable, DEV_CHARGER_REG_ITEM ** chgBufTable)
{
	if (chgId < MAX_CHARGER_SUPPORTED) {
		chgBufTable[chgId] = NULL;
		/* disable the item from common table */
		for (uint8_t i = 0; pTable[i].reg != DEV_CHARGER_REG_INVALID; i++) {
			DEV_CHARGER_REG_ITEM * pItem = dev_isl9241_findRegItem(chgId, pTable[i].reg);
			if (pItem) {
				pItem->access.f.autoRefersh = 0;
				pItem->access.f.accessType = DEV_CHARGER_REG_DISABLED;
			}
		}
		/* register the platform special table */
		chgBufTable[chgId] = pTable;
	}
}


/**
 * @brief Get cached value from charger register
 * @param chgId The charger ID
 * @param reg The register address
 * @param pVal Pointer to store the cached value
 * @return true if cache is available, false otherwise
 */
bool dev_isl9241_i32CacheGet(uint8_t chgId, uint8_t reg, void * pVal)
{
	uint32_t u32Val = 0;
	uint8_t width = 1;
	bool isCacheAvailable = false;
	DEV_CHARGER_REG_ITEM * pChgReg ;
    pChgReg = dev_isl9241_findRegItem(chgId, reg);
	if (pChgReg) {
		width = pChgReg->data.width;
		if (pChgReg->access.f.dataAvailable) {
			if (k_mutex_lock(&pChgReg->rwLock, K_NO_WAIT) == 0) {
				/* use the latest value */
				switch (pChgReg->data.width) {
					case 1:  u32Val = (uint32_t)pChgReg->data.value.u8[0];   break;
					case 2:  u32Val = (uint32_t)pChgReg->data.value.u16[0];  break;
					case 4:  u32Val = pChgReg->data.value.u32;               break;
					default:                                                 break;
				}
				if (pChgReg->access.f.dataAvailable) {
					isCacheAvailable = true;
				} else {
					u32Val = 0;
				}
				k_mutex_unlock(&pChgReg->rwLock);
			}
		}
	}

	if (!isCacheAvailable && pChgReg && width <= 4 && pChgReg->data.lastSuccessValue.u32 != 0xFFFFFFFF) {
		/* use the last value */
		u32Val = pChgReg->data.lastSuccessValue.u32;
		isCacheAvailable = true;
	}

	if (pVal && isCacheAvailable && width <= 4) {
		switch (width) {
			case 1: *(uint8_t *) pVal = (uint8_t) u32Val;  break;
			case 2: *(uint16_t *)pVal = (uint16_t)u32Val;  break;
			case 4: *(uint32_t *)pVal =           u32Val;  break;
			default:									   return false;
		}

		return true;
	}

	return false;
}

/**
 * @brief Read a 32-bit value from charger register
 * @param chgId The charger ID
 * @param reg The register address
 * @param pVal Pointer to store the read value
 * @return true if read was successful, false otherwise
 */
bool dev_isl9241_i32Read(uint8_t chgId, uint8_t reg, void * pVal)
{
	bool isSuccess = false;
	DEV_CHARGER_REG_ITEM * pChgReg;
	pChgReg = dev_isl9241_findRegItem(chgId, reg);
	if(pChgReg) {
		if(pChgReg->data.width <= 4 && dev_isl9241_autoRead(pChgReg, DEV_CHARGER_ID_TO_PORT(chgId), CHARGER_I2C_ADDRESS)) {
			isSuccess = dev_isl9241_i32CacheGet(chgId, reg, pVal);
		}
	}
	return isSuccess;
}

/**
 * @brief Write a 32-bit value to charger register
 * @param chgId The charger ID
 * @param reg The register address
 * @param val The value to write
 * @return true if write was successful, false otherwise
 */
bool dev_isl9241_i32Write(uint8_t chgId, uint8_t reg, int32_t val)
{
	DEV_CHARGER_REG_ITEM * pChgReg;
	pChgReg = dev_isl9241_findRegItem(chgId, reg);
	if (!pChgReg || pChgReg->access.f.accessType == DEV_CHARGER_REG_READ_ONLY || pChgReg->access.f.accessType == DEV_CHARGER_REG_DISABLED || pChgReg->data.width > 4)
		return false;

	bool isSuccess = false;
	k_mutex_lock(&_s_chg_port_access_Lock,K_FOREVER);
	if (DEV_CHARGER_RATIO_APP_1_REG_0P5 == pChgReg->access.f.ratio) {
		LOG_INF("**ratio_APP:REG = 2.0**, WRITE CHG: %02X, RAW %04X (%d), convert to %04X (%d)", reg, val, val, val >> 1, val >> 1);
		val >>= 1;
	} else if (DEV_CHARGER_RATIO_APP_0P5_REG_1 == pChgReg->access.f.ratio) {
		LOG_INF("**ratio_APP:REG = 0.5**, WRITE CHG: %02X, RAW %04X (%d), convert to %04X (%d)", reg, val, val, val << 1, val << 1);
		val <<= 1;
	} else if (DEV_CHARGER_RATIO_APP_1_REG_0P25 == pChgReg->access.f.ratio) {
		LOG_INF("**ratio_APP:REG = 4.0**, WRITE CHG: %02X, RAW %04X (%d), convert to %04X (%d)", reg, val, val, val >> 2, val >> 2);
		val >>= 2;
	}
	DEV_CHARGER_SWITCH_TO_PORT(DEV_CHARGER_ID_TO_PORT(chgId));
	isSuccess = _dev_charger_regAccess(false, CHARGER_I2C_ADDRESS, reg, (void *)&val, pChgReg->data.width);
	pChgReg->access.f.dataAvailable = 0;
	pChgReg->u32refreshCounter = 0;
	k_mutex_unlock(&_s_chg_port_access_Lock);

	return isSuccess;
}

/**
 * @brief Find a register item by charger ID and register address
 * @param chgId The charger ID
 * @param reg The register address to find
 * @return Pointer to the register item, or NULL if not found
 */
DEV_CHARGER_REG_ITEM * dev_isl9241_findRegItem(uint8_t chgId, uint8_t reg)
{
	if (chgId >= MAX_CHARGER_SUPPORTED || reg == DEV_CHARGER_REG_INVALID)
		return NULL;
	if (_s_platform_chgBufTable[chgId]) {
		for (uint8_t i = 0; _s_platform_chgBufTable[chgId][i].reg != DEV_CHARGER_REG_INVALID; i++) {
			if (_s_platform_chgBufTable[chgId][i].reg == reg && _s_platform_chgBufTable[chgId][i].access.f.accessType != DEV_CHARGER_REG_DISABLED)
				return &_s_platform_chgBufTable[chgId][i];
		}
	}
	for (uint8_t i = 0; _s_chgBuf[chgId][i].reg != DEV_CHARGER_REG_INVALID; i++) {
		if (_s_chgBuf[chgId][i].reg == reg && _s_chgBuf[chgId][i].access.f.accessType != DEV_CHARGER_REG_DISABLED)
			return &_s_chgBuf[chgId][i];
	}
	return NULL;
}

/**
 * @brief Read-Modify-Write operation on a charger register
 * @param chgId The charger ID
 * @param reg The register address
 * @param val The value to write (masked)
 * @param msk The bit mask for the operation
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_regRMW(uint8_t chgId, uint8_t reg, uint16_t val, uint16_t msk)
{
	bool isSuccess;
	uint16_t u16tmp;
	uint8_t retry = 3;

	do {
		isSuccess = false;
		if (dev_isl9241_i32Read(chgId, reg, &u16tmp)) {
			u16tmp &= ~msk;
			u16tmp |= (val & msk);
			isSuccess = dev_isl9241_i32Write(chgId, reg, u16tmp);
			if (isSuccess)
				break;
		}

		retry --;
		k_usleep(100);
	} while(retry);

	if (!isSuccess) {
		LOG_WRN("chgId:%d, rRMW 0x%02X v %04X m %04X failed!", chgId, reg, val, msk);
	}

	return isSuccess;
}

/**
 * @brief Enable charging on the specified charger
 * @param chgId The charger ID
 * @return true if charger was enabled successfully, false otherwise
 */
bool dev_isl9241_chgEnable(uint8_t chgId)
{
	bool isSuccess;
	uint8_t i;

	//
	// Set F_CHG_REG_MinSystemVoltage to non-zero
	//
	uint16_t u32tmp = 0;

	for (i = 0, isSuccess = false; i < 3; i++) {
		isSuccess = dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_MinSystemVoltage, &u32tmp);
		if (isSuccess) 
			break;

		k_usleep(100);
	}

	if ( isSuccess ) {
		if (u32tmp != APP_BATTERY_MIN_BATTERY_VOLTAGE) {
			u32tmp = APP_BATTERY_MIN_BATTERY_VOLTAGE;

			for (i = 0, isSuccess = false; i < 3; i++) {
				isSuccess = dev_isl9241_i32Write(chgId, DEV_ISL9241_REG_MinSystemVoltage, u32tmp);
				if (isSuccess) 
					break;

				k_usleep(100);
			}

			if (isSuccess) {
				LOG_INF("Charger enabled, DEV_ISL9241_REG_MinSystemVoltage(0x3E) = %04X !!!", u32tmp);
			} else {
				LOG_WRN("!!! Charger enable failed on writing DEV_ISL9241_REG_MinSystemVoltage(0x3E) !!!");
			}
		}
	} else {
		LOG_WRN("!!! Charger enable failed on reading DEV_ISL9241_REG_MinSystemVoltage(0x3E) !!!");
		return false;
	}

	return true;
}

/**
 * @brief Disable charging on the specified charger
 * @param chgId The charger ID
 * @return true if charger was disabled successfully, false otherwise
 */
bool dev_isl9241_chgDisable(uint8_t chgId)
{
	bool isSuccess;
	uint32_t u32tmp;
	uint8_t i;

	isSuccess = dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_MinSystemVoltage, &u32tmp);
	if ((isSuccess && 0 != u32tmp) || !isSuccess) {
		//
		// Set F_CHG_REG_MinSystemVoltage to zero
		//
		for (i = 0, isSuccess = false; i < 3; i++) {
			isSuccess = dev_isl9241_i32Write(chgId, DEV_ISL9241_REG_MinSystemVoltage, 0);
			if (isSuccess) 
				break;

			k_usleep(100);
		}
	}

	if (isSuccess) {
		LOG_INF("Charger disabled !!!");
		LOG_INF("     set DEV_ISL9241_REG_MinSystemVoltage(0x3E) = 0");
	} else {
		LOG_WRN("!!! Charger disable failed on writing DEV_ISL9241_REG_MinSystemVoltage(0x3E) !!!");
	}

	return isSuccess;
}

/**
 * @brief Check if charging is enabled on the specified charger
 * @param chgId The charger ID
 * @return true if charging is enabled, false otherwise
 */
bool dev_isl9241_isChgEn(uint8_t chgId)
{
	bool isSuccess;
	uint8_t i;

	//
	// Set F_CHG_REG_MinSystemVoltage to non-zero
	//
	uint16_t u32tmp = 0;

	for (i = 0, isSuccess = false; i < 3; i++) {
		isSuccess = dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_MinSystemVoltage, &u32tmp);
		if (isSuccess) 
			break;

		k_usleep(100);
	}

	if ( isSuccess && u32tmp != 0 ) {
		return true;
	} else {
		return false;
	}
}

/**
 * @brief Set the battery gate (BGATE) state
 * @param chgId The charger ID
 * @param turnOn true to turn on BGATE, false to turn off
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_setBgate(uint8_t chgId, bool turnOn)
{
	bool ret = false;
	if (turnOn) {
		ret = dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control1, 0, cbit(6));
	} else {
		ret = dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control1, cbit(6), cbit(6));
	}

	return ret;
}

/**
 * @brief Set the ACOK reference voltage for ISL9241
 * @param chgId The charger ID
 * @param AcRefVoltage The AC reference voltage in mV
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_ACOKReference(uint8_t chgId, uint32_t AcRefVoltage)
{
	//
	// convert to register raw value ([13:6] Stepping 96mV, Max. 24.48V))
	//
	uint32_t regRaw = AcRefVoltage * 2 / 3;

	bool st = dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_ACOKReference, regRaw, 0xFFFF);

	LOG_INF("Update ACOK_Ref to %5d mV (RAW 0x%04X, actual effect value %5d) returns %d", AcRefVoltage, regRaw, (regRaw & ISL9241_VAL_ACOKRef_MAX_RAW) * 3 / 2, st);
	return st;
}

/**
 * @brief Set the ACOK reference voltage for RAA489110
 * @param chgId The charger ID
 * @param AcRefVoltage The AC reference voltage in mV
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_RAA489110_ACOKReference(uint8_t chgId, uint32_t AcRefVoltage)
{
	//
	// dev_isl9241_RAA489110_ACOKReference ([13:6] Stepping 144mV, Max. 36.72V))
	//
	uint32_t regRaw = AcRefVoltage * 4 / 9;

	bool st = dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_ACOKReference, regRaw, 0xFFFF);

	LOG_INF("Update ACOK_Ref to %5d mV (RAW 0x%04X, actual effect value %5d) returns %d", AcRefVoltage, regRaw, (regRaw & ISL9241_VAL_ACOKRef_MAX_RAW) * 3 / 2, st);
	return st;
}

/**
 * @brief Set the VIN voltage for ISL9241
 * @param chgId The charger ID
 * @param VinVoltage The VIN voltage in mV
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_VIN_Voltage(uint8_t chgId, uint32_t VinVoltage)
{
	//
	// dev_isl9241_VIN_Voltage ([13:6] Stepping 85mV, Max. 16.384V))
	//
	uint32_t regRaw = VinVoltage * 64 / 85;

	bool st = dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_ACOKReference, regRaw, 0xFFFF);

	LOG_INF("Update VIN Voltage to %5d mV (RAW 0x%04X) returns %d", VinVoltage, regRaw, st);
	return st;
}

/**
 * @brief Set the VIN voltage for RAA489110
 * @param chgId The charger ID
 * @param VinVoltage The VIN voltage in mV
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_RAA489110_VIN_Voltage(uint8_t chgId, uint32_t VinVoltage)
{
	//
	// dev_isl9241_VIN_Voltage ([13:6] Stepping 128mV, Max. 24.576V))
	//
	uint32_t regRaw = VinVoltage / 2;

	bool st = dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_ACOKReference, regRaw, 0xFFFF);

	LOG_INF("Update VIN Voltage to %5d mV (RAW 0x%04X) returns %d", VinVoltage, regRaw, st);
	return st;
}

/**
 * @brief Set adapter current limit 1
 * @param chgId The charger ID
 * @param u16limit The current limit value
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_AdapterCurrentLimit1(uint8_t chgId, uint16_t u16limit)
{
	uint16_t u16Data;
	bool st = false;

	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_AdapterCurrentLimit1, &u16Data)) {
		u16Data = u16limit;    // Input current
		st = dev_isl9241_i32Write(chgId, DEV_ISL9241_REG_AdapterCurrentLimit1, u16Data);
		LOG_INF("set AdapterCurrentLimit1<%02Xh> %d returns %d", DEV_ISL9241_REG_AdapterCurrentLimit1, u16Data, st);
	} else {
		LOG_WRN("set AdapterCurrentLimit1<%02Xh> read failed !!", DEV_ISL9241_REG_AdapterCurrentLimit1);
	}

	return st;
}

/**
 * @brief Set adapter AC PROCHOT low threshold
 * @param chgId The charger ID
 * @param u16limit The threshold value
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_AdapterACProchotL(uint8_t chgId, uint16_t u16limit)
{
	uint16_t u16Data;
	bool st = false;

	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_ACProchotL, &u16Data)) {
		u16Data = u16limit;
		st = dev_isl9241_i32Write(chgId, DEV_ISL9241_REG_ACProchotL, u16Data);
		LOG_INF("set AdapterACProchotL<%02Xh> %d returns %d", DEV_ISL9241_REG_ACProchotL, u16Data, st);
	} else {
		LOG_WRN("set AdapterACProchotL<%02Xh> read failed !!", DEV_ISL9241_REG_ACProchotL);
	}

	return st;
}

/**
 * @brief Set adapter current limit 2
 * @param chgId The charger ID
 * @param u16limit The current limit value
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_AdapterCurrentLimit2(uint8_t chgId, uint16_t u16limit)
{
	uint16_t u16Data;
	bool st = false;

	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_AdapterCurrentLimit2, &u16Data)) {
		u16Data = u16limit;    // Input current
		st = dev_isl9241_i32Write(chgId, DEV_ISL9241_REG_AdapterCurrentLimit2, u16Data);
		LOG_INF("set AdapterCurrentLimit2<%02Xh> %d returns %d", DEV_ISL9241_REG_AdapterCurrentLimit2, u16Data, st);
	} else {
		LOG_WRN("set AdapterCurrentLimit2<%02Xh> read failed !!", DEV_ISL9241_REG_AdapterCurrentLimit2);
	}

	return st;
}

/**
 * @brief Set adapter DC PROCHOT low threshold
 * @param chgId The charger ID
 * @param u16limit The threshold value
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_AdapterDCProchotL(uint8_t chgId, uint16_t u16limit)
{
	uint16_t u16Data;
	bool st = false;

	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_DCProchotL, &u16Data)) {
		u16Data = u16limit;
		st = dev_isl9241_i32Write(chgId, DEV_ISL9241_REG_DCProchotL, u16Data);
		LOG_INF("set AdapterDCProchotL<%02Xh> %d returns %d", DEV_ISL9241_REG_DCProchotL, u16Data, st);
	} else {
		LOG_WRN("set AdapterDCProchotL<%02Xh> read failed !!", DEV_ISL9241_REG_DCProchotL);
	}

	return st;
}

/**
 * @brief Set PROCHOT debounce time
 * @param chgId The charger ID
 * @param debounce The debounce setting
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_SetProchotDebounce(uint8_t chgId, EM_DEV_CHARGER_PROCHOT_DEBOUNCE debounce)
{
	uint16_t u16Data;
	bool st = false;

	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_Control2, &u16Data)) {
		u16Data &= ~0x0600; // [10:9]
		u16Data |= (((uint32_t)debounce & 0x03) << 9);
		st = dev_isl9241_i32Write(chgId, DEV_ISL9241_REG_Control2, u16Data);
		LOG_INF("set ProchotDebounce<%02Xh> %04X returns %d", DEV_ISL9241_REG_Control2, u16Data, st);
	} else {
		LOG_WRN("set ProchotDebounce<%02Xh> read failed !!", DEV_ISL9241_REG_Control2);
	}

	return st;
}

/**
 * @brief Set PROCHOT duration
 * @param chgId The charger ID
 * @param e The duration setting
 * @return true if operation was successful, false otherwise
 */
bool dev_isl9241_SetProchotDuration(uint8_t chgId, EM_DEV_CHARGER_PROCHOT_DURATION e)
{
	uint16_t u16Data;
	bool st = false;

	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_Control2, &u16Data)) {
		u16Data &= ~0x01C0; // [8:6]
		u16Data |= (((uint32_t)e & 0x07) << 6);
		st = dev_isl9241_i32Write(chgId, DEV_ISL9241_REG_Control2, u16Data);
		LOG_INF("set ProchotDuration<%02Xh> %04X returns %d", DEV_ISL9241_REG_Control2, u16Data, st);
	} else {
		LOG_WRN("set ProchotDuration<%02Xh> read failed !!", DEV_ISL9241_REG_Control2);
	}

	return st;
}


/**
 * @brief Transition charger to bypass mode (version 1)
 * @param chgId The charger ID
 * @return true if transition was successful, false otherwise
 */
bool dev_isl9241_toByPassMode_v1(uint8_t chgId)
{
	LOG_INF("To by pass mode v1 <ENTER>");

	bool isChaEn = dev_isl9241_isChgEn(chgId);
	uint16_t u16tmp;

	/* Preparation steps */
	// 01. Set CC and MinsysV = 0 (transition to NVDC state first)
	// 02. Set ACLIM to appropriate value
	// 03. Reduce system load below ACLIM before start of transition
	// 04. Enable ADC first if using ADC after step 3
	//     Enable ADC - Control3 reg 0x4C bit<0>: 0 = ADC is active only when adapter is plugged in and charging (default)
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control3, 0x0001, 0x0001);

	dev_isl9241_chgDisable(chgId);
	k_usleep(10 * 1000);

	// 1. Turn on VIN/VOUT Comparator so Bypass gate turns off when adapter is removed.
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control0, 0x0020, 0xFFFF);

	// read Vsys as reference
	u16tmp = 0;
	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_VSYS_ADC_Results, &u16tmp)) {
		LOG_INF("Vsys voltage raw 0x%02X (%d mV)", u16tmp, u16tmp * 3 / 2);
	}

	// 2. Set MaxsysV = VADP (from ADC or PD controller) - 2V
	u16tmp = 0;
	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_VIN_ADC_Results, &u16tmp)) {
		LOG_INF("Vin (CSIN) voltage raw 0x%02X (%d mV)", u16tmp, u16tmp * 3 / 2);

		u16tmp = u16tmp * 3 / 2;
	}
	if (u16tmp > 2000)
		u16tmp -= 2000;
	if (u16tmp > APP_BATTERY_FULL_CHARGE_VOLTAGE && u16tmp < F_AC_ADAPTER_VIN_VOLTAGE) {
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_MaxSystemVoltage, u16tmp, 0xFFFF);
		LOG_INF("Set MaxsysV<15h, ChargingVoltage> %d mV", u16tmp);
	} else {
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_MaxSystemVoltage, APP_BATTERY_FULL_CHARGE_VOLTAGE + 384, 0xFFFF);
		LOG_INF("Set MaxsysV<15h, ChargingVoltage> %d mV", APP_BATTERY_FULL_CHARGE_VOLTAGE + 384);
	}

	// Wait until Vsys == MaxsysV
	k_usleep(50 * 1000);

	// read Vsys again as reference
	u16tmp = 0;
	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_VSYS_ADC_Results, &u16tmp)) {
		LOG_INF("Vsys voltage raw 0x%02X (%d mV)", u16tmp, u16tmp * 3 / 2);
	}

	// 3. Set ACOKREF to 1 ~ 2V higher than battery**
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_ACOKReference, ISL9241_VAL_ACOKRef_17664, 0xFFFF);

	// 4. Turn on Bypass Gate, turn off NGATE, set CP to 100%, enable 10mA current source*
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control0, 0x3860, 0xFFFF);

	// 5. Stop Switching after 1ms (wait time for Bypass FET to turn on)
	k_usleep(1000);
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_MaxSystemVoltage, 0, 0xFFFF);

	// 6. Disable 10mA discharge on CSOP after 10ms (wait times can be optimized)
	k_usleep(10 * 1000);
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control0, 0x1860, 0xFFFF);

	// 7. Force Forward Buck/Reverse Boost mode, Disable slew rate control (if enabled)
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control4, 0x0400, 0xFFFF);

	if (isChaEn)
		dev_isl9241_chgEnable(chgId);

	LOG_INF("To by pass mode v1 <EXIT>");
	return true;
}

/**
 * @brief Transition charger to NVDC mode (version 1)
 * @param chgId The charger ID
 * @return true if transition was successful, false otherwise
 */
bool dev_isl9241_toNvdcMode_v1(uint8_t chgId)
{
	LOG_INF("To NVDC mode v1 <ENTER>");

	bool isChaEn = dev_isl9241_isChgEn(chgId);
	uint16_t u16tmp;

	dev_isl9241_chgDisable(chgId);
	k_usleep(100);

	// 0. set Enable ADC = 1
	//    0 - ADC is active only when adapter is plugged in and charging (default)
	//    1 - Enables ADC for all modes
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control3, 0x0001, 0x0001);

	// 1. Turn off Force Forward Buck/Reverse Boost mode
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control4, 0, 0xFFFF);

	// 2. Turn off VIN/VOUT Comparator so Bypass gate turns off when adapter is removed.
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control0, 0x1840, 0xFFFF);
	k_usleep(10 * 1000);

	// read Vsys as reference
	u16tmp = 0;
	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_VSYS_ADC_Results, &u16tmp)) {
		LOG_INF("Vsys voltage raw 0x%02X (%d mV)", u16tmp, u16tmp * 3 / 2);
	}

	// 3. Set MaxsysV = VADP (from ADC or PD controller) - 2V
	u16tmp = 0;
	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_VIN_ADC_Results, &u16tmp)) {
		LOG_INF("Vin (CSIN) voltage raw 0x%02X (%d mV)", u16tmp, u16tmp * 3 / 2);

		u16tmp = u16tmp * 3 / 2;
	}
	if (u16tmp > 2000)
		u16tmp -= 2000;
	if (u16tmp > APP_BATTERY_FULL_CHARGE_VOLTAGE && u16tmp < F_AC_ADAPTER_VIN_VOLTAGE) {
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_MaxSystemVoltage, u16tmp, 0xFFFF);
		LOG_INF("Set MaxsysV<15h, ChargingVoltage> %d mV", u16tmp);
	} else {
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_MaxSystemVoltage, APP_BATTERY_FULL_CHARGE_VOLTAGE + 384, 0xFFFF);
		LOG_INF("Set MaxsysV<15h, ChargingVoltage> %d mV", APP_BATTERY_FULL_CHARGE_VOLTAGE + 384);
	}

	// Wait until Vsys == MaxsysV
	k_usleep(50 * 1000);

	// read Vsys again as reference
	u16tmp = 0;
	if (dev_isl9241_i32Read(chgId, DEV_ISL9241_REG_VSYS_ADC_Results, &u16tmp)) {
		LOG_INF("Vsys voltage raw 0x%02X (%d mV)", u16tmp, u16tmp * 3 / 2);
	}

	// 4. Turn off Bypass Gate, turn on NGATE, set CP to 100%, enable 10mA current source*
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control0, 0x2040, 0xFFFF);

	// 5. Set MaxsysVoltage = Full charge voltage after 1ms (wait time for NGATE to turn on)
	k_usleep(1000);
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_MaxSystemVoltage, APP_BATTERY_FULL_CHARGE_VOLTAGE, 0xFFFF);

	// 6. Disable 10mA discharge on CSOP after 10ms (or until CSOP ramps down to MaxsysV ), Set CP to 2%
	k_usleep(10 * 1000);
	dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control0, 0x0, 0xFFFF);

	// 7. Optional - Set ACOKREF back to 0 to allow detection of low voltage adapters during next plugin 
	//    (or for FRS or to save power in B attery only mode)
	// dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_ACOKReference, 0, 0xFFFF);

	if (isChaEn)
		dev_isl9241_chgEnable(chgId);

	LOG_INF("To NVDC mode v1 <EXIT>");

	return true;
}

/**
 * @brief Configure charger for battery-only or wall power mode
 * @param chgId The charger ID
 * @param enable true for battery-only optimized mode, false for wall power mode
 */
void dev_isl9241_battery_only(uint8_t chgId, bool enable)
{
	LOG_INF("Switch isl9241 to %s ", enable ? "battery-only optimized" : "wall power");

	if (enable) {                  /* Battery only mode */
		/* Assert Disable IMON bit5 */
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control1, cbit(5), cbit(5));

		/* Clear ADC active only if adapter is plugged in and charging enabled bit0 */
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control3, 0, cbit(0));

		/* Assert Disable GP Comparator for battery only mode bit12 */
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control4, cbit(12), cbit(12));
	} else {                       /* Revert to original value */
		/* Clear Disable IMON bit5 */
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control1, 0, cbit(5));

		/* Assert ADC active only if adapter is plugged in and charging enabled bit0 */
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control3, cbit(0), cbit(0));

		/* Clear Disable GP Comparator for battery only mode bit12 */
		dev_isl9241_regRMW(chgId, DEV_ISL9241_REG_Control4, 0, cbit(12));
	}
}

/**
 * @brief Work handler for PROCHOT gate setup
 * @param w Pointer to the work item
 */
static void _dev_charger_setProchot_handler(struct k_work *w)
{
	if (pfnSetupProchotGate) {
		pfnSetupProchotGate();
	} else {
		LOG_ERR("No set prochot handler registered!");
	}
}

/**
 * @brief Submit PROCHOT setup work to the work queue
 */
void dev_isl9241_setProchot(void)
{
	k_work_submit(&setProchot_work);
}

/**
 * @brief Register a PROCHOT gate setup handler
 * @param handler Function pointer to the PROCHOT gate setup handler
 */
void dev_isl9241_prochot_register(pfnSetupProchotGate_t handler)
{
	pfnSetupProchotGate = handler;
	k_work_init(&setProchot_work, _dev_charger_setProchot_handler);
}