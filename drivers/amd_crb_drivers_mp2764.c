/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "amd_crb_drivers_mp2764.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include <dev_utility.h>
#include <assert.h>
#include <i2c_hub.h>
#include <board_config.h>
#include "debug_info.h"
LOG_MODULE_REGISTER(mp2764, CONFIG_CHARGER_LOG_LEVEL);

K_MUTEX_DEFINE(_s_chg_port_access_Lock);

/*
 * charger I2C address and port
 */
#ifndef CHARGER_I2C_PORT
#define CHARGER_I2C_PORT         (I2C_3)             /* MP2764 default port */
#endif
#ifndef CHARGER_I2C_ADDRESS
#define CHARGER_I2C_ADDRESS      (0x10)              /* MP2764 default address */
#endif

static pfnSetupProchotGate_t pfnSetupProchotGate;
static struct k_work setProchot_work;
/* charger register buffer */
DEV_CHARGER_REG_ITEM * _s_platform_chgBufTable[MAX_CHARGER_SUPPORTED];
/* charger I2C bus error counter */
static DEV_CHARGER_I2C_INTERFACE_ERROR_COUNT _s_charger_i2c_bus_errCount[5];
/* charger debug registers */
static DEV_CHARGER_REG_ITEM _s_charger0[] = {
	/* Register,                                value,      Reload,   Counter,   AccessCtrl          */
	/*                                          {width, data}                                        */
	/*                                                                         (accessType:2 ratio:2 dataAvail:1 NA:1 autoRefersh:1, NA:1)*/
	{DEV_CHARGER_REG_ChargerChargingCurrent  ,  { 2, {0} }, 500   ,   0,       {c8bit(1100, 0000)} },  // 0x14 - master device sends the desired charging rate (mA).
	{DEV_CHARGER_REG_ChargerChargingVoltage  ,  { 2, {0} }, 1000  ,   0,       {c8bit(1100, 0000)} },  // 0x15 - master device sends the desired charging voltage
	/*------------------------------------------*/
	{DEV_CHARGER_REG_INVALID			     ,  { 0, {0} }, 0     ,   0,       {c8bit(0000, 0000)} },  // Ending
};

/* charger register dump for debug */
DEV_CHARGER_REG_ITEM * _s_chgBuf[MAX_CHARGER_SUPPORTED] = {
	_s_charger0,
//	... ...
};

typedef enum
{
	EM_DEV_MP2764_TRICKLE_CC_0_MA = 0,  /* 4'b0000 */
	EM_DEV_MP2764_TRICKLE_CC_62_5_MA,   /* 4'b0001 */
	EM_DEV_MP2764_TRICKLE_CC_125_MA,    /* 4'b0010 */
	EM_DEV_MP2764_TRICKLE_CC_187_5_MA,  /* 4'b0011 */
	EM_DEV_MP2764_TRICKLE_CC_250_MA,    /* 4'b0100 (default) */
	EM_DEV_MP2764_TRICKLE_CC_312_5_MA,  /* 4'b0101 */
	EM_DEV_MP2764_TRICKLE_CC_375_MA,    /* 4'b0110 */
	EM_DEV_MP2764_TRICKLE_CC_437_5_MA,  /* 4'b0111 */
	EM_DEV_MP2764_TRICKLE_CC_500_MA,    /* 4'b1000 */
	EM_DEV_MP2764_TRICKLE_CC_562_5_MA,  /* 4'b1001 */
	EM_DEV_MP2764_TRICKLE_CC_625_MA,    /* 4'b1010 */
	EM_DEV_MP2764_TRICKLE_CC_687_5_MA,  /* 4'b1011 */
	EM_DEV_MP2764_TRICKLE_CC_750_MA,    /* 4'b1100 */
	EM_DEV_MP2764_TRICKLE_CC_812_5_MA,  /* 4'b1101 */
	EM_DEV_MP2764_TRICKLE_CC_875_MA,    /* 4'b1110 */
	EM_DEV_MP2764_TRICKLE_CC_937_5_MA   /* 4'b1111 */
} EM_DEV_MP2764_TRICKLE_CC;

typedef enum
{
	EM_DEV_MP2764_PRE_CC_0_MA = 0,  /* 4'b0000 */
	EM_DEV_MP2764_PRE_CC_125_MA,    /* 4'b0001 */
	EM_DEV_MP2764_PRE_CC_250_MA,    /* 4'b0010 (default) */
	EM_DEV_MP2764_PRE_CC_375_MA,    /* 4'b0011 */
	EM_DEV_MP2764_PRE_CC_500_MA,    /* 4'b0100 */
	EM_DEV_MP2764_PRE_CC_625_MA,    /* 4'b0101 */
	EM_DEV_MP2764_PRE_CC_750_MA,    /* 4'b0110 */
	EM_DEV_MP2764_PRE_CC_875_MA,    /* 4'b0111 */
	EM_DEV_MP2764_PRE_CC_1000_MA,   /* 4'b1000 */
	EM_DEV_MP2764_PRE_CC_1125_MA,   /* 4'b1001 */
	EM_DEV_MP2764_PRE_CC_1250_MA,   /* 4'b1010 */
	EM_DEV_MP2764_PRE_CC_1375_MA,   /* 4'b1011 */
	EM_DEV_MP2764_PRE_CC_1500_MA,   /* 4'b1100 */
	EM_DEV_MP2764_PRE_CC_1625_MA,   /* 4'b1101 */
	EM_DEV_MP2764_PRE_CC_1750_MA,   /* 4'b1110 */
	EM_DEV_MP2764_PRE_CC_1875_MA    /* 4'b1111 */
} EM_DEV_MP2764_PRE_CC;

typedef enum
{
	EM_DEV_MP2764_TERM_CC_0_MA = 0,  /* 4'b0000 */
	EM_DEV_MP2764_TERM_CC_31_25_MA,  /* 4'b0001 */
	EM_DEV_MP2764_TERM_CC_62_5_MA,   /* 4'b0010 */
	EM_DEV_MP2764_TERM_CC_93_75_MA,  /* 4'b0011 */
	EM_DEV_MP2764_TERM_CC_125_MA,    /* 4'b0100 (default) */
	EM_DEV_MP2764_TERM_CC_156_25_MA, /* 4'b0101 */
	EM_DEV_MP2764_TERM_CC_187_5_MA,  /* 4'b0110 */
	EM_DEV_MP2764_TERM_CC_218_75_MA, /* 4'b0111 */
	EM_DEV_MP2764_TERM_CC_250_MA,    /* 4'b1000 */
	EM_DEV_MP2764_TERM_CC_281_25_MA, /* 4'b1001 */
	EM_DEV_MP2764_TERM_CC_312_5_MA,  /* 4'b1010 */
	EM_DEV_MP2764_TERM_CC_343_75_MA, /* 4'b1011 */
	EM_DEV_MP2764_TERM_CC_375_MA,    /* 4'b1100 */
	EM_DEV_MP2764_TERM_CC_406_25_MA, /* 4'b1101 */
	EM_DEV_MP2764_TERM_CC_437_5_MA,  /* 4'b1110 */
	EM_DEV_MP2764_TERM_CC_468_75_MA  /* 4'b1111 */
} EM_DEV_MP2764_TERM_CC;

/**
 * _dev_charger_updateBusErrorCounter
 *
 * @param [in]   slaveAddr;      I2C address
 * @param [in]   wasSuccess;     pass before or not
 * @return void
 * @note
 *  save the charger I2C bus error counter
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
 * amd_crb_drivers_mp2764_regPlatTable
 *
 * @param [in]   pItem;      charger profile
 * @param [in]   port;       charger index
 * @param [in]   slaveAddr;  I2C address
 * @return void
 * @note
 *  auto read the charger register. (not directly read the register)
 */
void amd_crb_drivers_mp2764_regPlatTable(uint8_t chgId, DEV_CHARGER_REG_ITEM * pTable, DEV_CHARGER_REG_ITEM ** chgBufTable)
{
	if (chgId < MAX_CHARGER_SUPPORTED) {
		chgBufTable[chgId] = NULL;
		/* disable the item from common table */
		for (uint8_t i = 0; pTable[i].reg != DEV_CHARGER_REG_INVALID; i++) {
			DEV_CHARGER_REG_ITEM * pItem = amd_crb_drivers_mp2764_findRegItem(chgId, pTable[i].reg);
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
 * amd_crb_drivers_mp2764_regRefresh
 *
 * @param [in]   reg;      register to read
 * @param [in]   pBuf;     read data
 * @return void
 * @note
 *  refresh charger register data with two bytes
 */
void amd_crb_drivers_mp2764_regRefresh(uint8_t * reg, void * pBuf)
{
	i2c_hub_write_read(CHARGER_I2C_PORT, CHARGER_I2C_ADDRESS, reg, 1, pBuf, 2);
}

/**
 * _dev_charger_regAccess
 *
 * @param [in]   isRead;     true-> read and false->write
 * @param [in]   slaveAddr;  I2C address
 * @param [in]   reg;        register to access
 * @param [in]   pBuf;       data buffer
 * @param [in]   len;        data length
 * @return void
 * @note
 *  i2c read or write to charger device
 */
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
			/* SMBus write block protocol */
			assert(0);
			/* not supported for now */
		}

        if (ret == 0)
        {
            isSuccess = true; /* for block access, the required bytes may not equal to the real numbers can be read. */
			break;
        }
	if (!isSuccess) {
		info_value_increase(INFO_I2C_CHARGER, 1);
	}
    }
	_dev_charger_updateBusErrorCounter(slaveAddr, isSuccess);
	return isSuccess;
}

/**
 * amd_crb_drivers_mp2764_autoRead
 *
 * @param [in]   pItem;      charger profile
 * @param [in]   port;       charger index
 * @param [in]   slaveAddr;  I2C address
 * @return void
 * @note
 *  auto read the charger register. (not directly read the register)
 */
bool amd_crb_drivers_mp2764_autoRead(DEV_CHARGER_REG_ITEM * pItem, uint8_t port, uint8_t slaveAddr)
{
	uint8_t * pData = (uint8_t *)(&pItem->data.value);

	if (!pItem)
		return false;

	if (pItem->access.f.accessType == DEV_CHARGER_REG_WRITE_ONLY ||
		pItem->access.f.accessType == DEV_CHARGER_REG_DISABLED)
		return false;

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
 * amd_crb_drivers_mp2764_i32CacheGet
 *
 * @param [in]   chgId;      charger index
 * @param [in]   reg;        register
 * @param [in]   pVal;       value pointer
 * @return true as read pass
 * @note
 *  get the cached register value. Not directly read.
 */
bool amd_crb_drivers_mp2764_i32CacheGet(uint8_t chgId, uint8_t reg, void * pVal)
{
	uint32_t u32Val = 0;
	uint8_t width = 1;
	bool isCacheAvailable = false;
	DEV_CHARGER_REG_ITEM * pChgReg ;
    pChgReg = amd_crb_drivers_mp2764_findRegItem(chgId, reg);
	if (pChgReg) {
		width = pChgReg->data.width;
		if (pChgReg->access.f.dataAvailable) {
			if (k_mutex_lock(&pChgReg->rwLock,K_NO_WAIT)==0) {
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
 * amd_crb_drivers_mp2764_i32Read
 *
 * @param [in]   chgId;      charger index
 * @param [in]   reg;        register
 * @param [in]   pVal;       value pointer
 * @return true as read pass
 * @note
 *  auto read the register and get cache value from buffer
 */
bool amd_crb_drivers_mp2764_i32Read(uint8_t chgId, uint8_t reg, void * pVal)
{
	bool isSuccess = false;
	DEV_CHARGER_REG_ITEM * pChgReg;
	pChgReg = amd_crb_drivers_mp2764_findRegItem(chgId, reg);
	if(pChgReg)
	if(pChgReg->data.width <= 4 && amd_crb_drivers_mp2764_autoRead(pChgReg, DEV_CHARGER_ID_TO_PORT(chgId), CHARGER_I2C_ADDRESS)) {
		isSuccess = amd_crb_drivers_mp2764_i32CacheGet(chgId, reg, pVal);
	}
	return isSuccess;
}

/**
 * amd_crb_drivers_mp2764_i32Read
 *
 * @param [in]   chgId;      charger index
 * @param [in]   reg;        register
 * @param [in]   val;       value
 * @return true as read pass
 * @note
 *  write date to register
 */
bool amd_crb_drivers_mp2764_i32Write(uint8_t chgId, uint8_t reg, int32_t val)
{
	DEV_CHARGER_REG_ITEM * pChgReg;
	pChgReg = amd_crb_drivers_mp2764_findRegItem(chgId, reg);
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
 * amd_crb_drivers_mp2764_findRegItem
 *
 * @param [in]   chgId;      charger index
 * @param [in]   reg;        register
 * @return true as read pass
 * @note
 *  find the register object based on register offset from the regsiter table
 */
DEV_CHARGER_REG_ITEM * amd_crb_drivers_mp2764_findRegItem(uint8_t chgId, uint8_t reg)
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
 * amd_crb_drivers_mp2764_regRMW
 *
 * @param [in]   chgId;      charger index
 * @param [in]   reg;        register
 * @param [in]   val;        data
 * @param [in]   msk;        bits mask
 * @return true as read pass
 * @note
 *  Read the register data and set/clr bit based on bit mask. Then write data back.
 */
bool amd_crb_drivers_mp2764_regRMW(uint8_t chgId, uint8_t reg, uint16_t val, uint16_t msk)
{
	bool isSuccess;
	uint16_t u16tmp;
	uint8_t retry = 3;

	do {
		isSuccess = false;
		if (amd_crb_drivers_mp2764_i32Read(chgId, reg, &u16tmp)) {
			u16tmp &= ~msk;
			u16tmp |= (val & msk);
			isSuccess = amd_crb_drivers_mp2764_i32Write(chgId, reg, u16tmp);
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
 * amd_crb_drivers_mp2764_chgEnable
 *
 * @param [in]   chgId;      charger index
 * @return true as read pass
 * @note
 *  enable the charging function
 */
bool amd_crb_drivers_mp2764_chgEnable(uint8_t chgId)
{
	bool isSuccess = false;
	uint8_t i;
	DEV_MP2764_REG_14 reg_14;

	/* read the data from register 0x14 */
	isSuccess = amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ChargeCurSetting1, &(reg_14.data));

	/* write Icc to zero to disable the charging, and non zero to enable */
	reg_14.f.icc = (uint8_t) (APP_BATTERY_PRECHARGE_CURRENT * 10 / 625);

	if (isSuccess) {
		for (i = 0, isSuccess = false; i < 3; i++) {
			isSuccess = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ChargeCurSetting1, reg_14.data);
			if (isSuccess)
				break;

			k_usleep(100);
		}

		if (isSuccess) {
			LOG_INF("Charger enabled, DEV_MP2764_REG_ChargeCurSetting1(0x14) = %04X !!!", reg_14.data);
		} else {
			LOG_WRN("!!! Charger enable failed on writing DEV_MP2764_REG_ChargeCurSetting1(0x14) !!!");
		}
	} else {
		LOG_WRN("!!! Charger enable failed on reading DEV_MP2764_REG_ChargeCurSetting1(0x14) !!!");
		return false;
	}

	return true;
}

/**
 * amd_crb_drivers_mp2764_chgDisable
 *
 * @param [in]   chgId;      charger index
 * @return true as read pass
 * @note
 *  disable the charging function
 */
bool amd_crb_drivers_mp2764_chgDisable(uint8_t chgId)
{
	bool isSuccess = false;
	uint8_t i;
	DEV_MP2764_REG_14 reg_14;

	/* read the data from register 0x14 */
	isSuccess = amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ChargeCurSetting1, &(reg_14.data));

	/* write Icc to zero to disable the charging, and non zero to enable */
	reg_14.f.icc = 0;

	if (isSuccess) {
		for (i = 0, isSuccess = false; i < 3; i++) {
			isSuccess = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ChargeCurSetting1, reg_14.data);
			if (isSuccess)
				break;

			k_usleep(100);
		}

		if (isSuccess) {
			LOG_INF("Charger enabled, DEV_MP2764_REG_ChargeCurSetting1(0x14) = %04X !!!", reg_14.data);
		} else {
			LOG_WRN("!!! Charger enable failed on writing DEV_MP2764_REG_ChargeCurSetting1(0x14) !!!");
		}
	} else {
		LOG_WRN("!!! Charger enable failed on reading DEV_MP2764_REG_ChargeCurSetting1(0x14) !!!");
		return false;
	}

	return true;
}

/**
 * amd_crb_drivers_mp2764_chgDisable
 *
 * @param [in]   chgId;      charger index
 * @return true as read pass
 * @note
 *  check if the battery charging is enabled
 */
bool amd_crb_drivers_mp2764_isChgEn(uint8_t chgId)
{
	bool isSuccess;
	uint8_t i;

	/* Set DEV_MP2764_REG_ChargeCurSetting1 to non-zero */
	uint16_t u16tmp = 0;

	for (i = 0, isSuccess = false; i < 3; i++) {
		isSuccess = amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ChargeCurSetting1, &u16tmp);
		if (isSuccess) 
			break;

		k_usleep(100);
	}

	if ( isSuccess && u16tmp != 0 ) {
		return true;
	} else {
		return false;
	}
}

/**
 * amd_crb_drivers_mp2764_setBgate
 *
 * @param [in]   chgId;      charger index
 * @return true as read pass
 * @note
 *  enable or disable the BGATE
 */
bool amd_crb_drivers_mp2764_setBgate(uint8_t chgId, bool turnOn)
{
	bool ret = false;
	DEV_MP2764_REG_11 reg_11;

	ret = amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ConfigReg1, &(reg_11.data));

	if (turnOn) {
		reg_11.f.force_bfet_off = false;
	} else {
		reg_11.f.force_bfet_off = true;
	}

	ret = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ConfigReg1, reg_11.data);

	return ret;
}

/**
 * amd_crb_drivers_mp2764_VinMinVoltage
 *
 * @param [in]   chgId;       charger index
 * @param [in]   VinVoltage;  input min volt
 * @return true as read pass
 * @note
 *  set the input minimum voltage. If input voltage below it, charger will not provide power to system
 */
bool amd_crb_drivers_mp2764_VinMinVoltage(uint8_t chgId, uint32_t VinVoltage)
{
	DEV_MP2764_REG_08 reg_08;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_InputMinVoltSetting, &(reg_08.data))) {
		/* VIN_MIN = DEC(bits[8:0]) x 50 (mV) */
		reg_08.f.vin_min = VinVoltage / 50;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_InputMinVoltSetting, reg_08.data);
		LOG_INF("set DEV_MP2764_REG_InputMinVoltSetting<%02Xh> %d returns %d", DEV_MP2764_REG_InputMinVoltSetting, reg_08.f.vin_min, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_InputMinVoltSetting<%02Xh> read failed !!", DEV_MP2764_REG_InputMinVoltSetting);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_SysMinVoltage
 *
 * @param [in]   chgId;       charger index
 * @param [in]   SysVoltage;  input min volt
 * @return true as read pass
 * @note
 *  Minimum system voltage per cell setting for NVDC power path management control.
 */
bool amd_crb_drivers_mp2764_SysMinVoltage(uint8_t chgId, uint32_t SysVoltage)
{
	DEV_MP2764_REG_09 reg_09;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_MinSysVoltSetting, &(reg_09.data))) {
		/* VSYS_MIN /cell = DEC(bits[5:0]) x 100 (mV) */
		reg_09.f.vsys_min = SysVoltage / 100;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_MinSysVoltSetting, reg_09.data);
		LOG_INF("set DEV_MP2764_REG_MinSysVoltSetting<%02Xh> %d returns %d", DEV_MP2764_REG_MinSysVoltSetting, reg_09.f.vsys_min, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_MinSysVoltSetting<%02Xh> read failed !!", DEV_MP2764_REG_MinSysVoltSetting);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_AcLimit1
 *
 * @param [in]   chgId;       charger index
 * @param [in]   u16limit;    AC limit1
 * @return true as read pass
 * @note
 *  The first level input current limit setting. Easily trigger with longer debounce time. (Range 0~7000mA)
 */
bool amd_crb_drivers_mp2764_AcLimit1(uint8_t chgId, uint16_t u16limit)
{
	DEV_MP2764_REG_0A reg_0A;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_InputCurLimitSetting, &(reg_0A.data))) {
		/* ICCIN_LIM1 = DEC(bits[15:8]) x 31.25 (mA) */
		reg_0A.f.iin_lim1 = u16limit * 100 / 3125;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_InputCurLimitSetting, reg_0A.data);
		LOG_INF("set AdapterCurrentLimit1<%02Xh> %d returns %d", DEV_MP2764_REG_InputCurLimitSetting, reg_0A.f.iin_lim1, st);
	} else {
		LOG_WRN("set AdapterCurrentLimit1<%02Xh> read failed !!", DEV_MP2764_REG_InputCurLimitSetting);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_AcLimit2
 *
 * @param [in]   chgId;       charger index
 * @param [in]   u16limit;    AC limit1
 * @return true as read pass
 * @note
 *  The second level input current limit setting. Hard to trigger with shorter debounce time. (Range 0~7000mA)
 */
bool amd_crb_drivers_mp2764_AcLimit2(uint8_t chgId, uint16_t u16limit)
{
	DEV_MP2764_REG_0A reg_0A;
	DEV_MP2764_REG_12 reg_12;
	DEV_MP2764_REG_18 reg_18;
	bool st = false;

	if (u16limit > 0) {
		if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ConfigReg2, &(reg_12.data))) {
			/* enable the second ac limit */
			reg_12.f.two_inlim_en = 1;
			st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ConfigReg2, reg_12.data);
			LOG_INF("set DEV_MP2764_REG_ConfigReg2<%02Xh> %d returns %d", DEV_MP2764_REG_ConfigReg2, reg_12.f.two_inlim_en, st);
		} else {
			LOG_WRN("set DEV_MP2764_REG_ConfigReg2<%02Xh> read failed !!", DEV_MP2764_REG_ConfigReg2);
		}

		if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_InputCurLimitSetting, &(reg_0A.data))) {
			/* ICCIN_LIM2 = DEC(bits[15:8]) x 31.25 (mA) */
			reg_0A.f.iin_lim2 = u16limit * 100 / 3125;
			st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_InputCurLimitSetting, reg_0A.data);
			LOG_INF("set AdapterCurrentLimit2<%02Xh> %d returns %d", DEV_MP2764_REG_InputCurLimitSetting, reg_0A.f.iin_lim2, st);
		} else {
			LOG_WRN("set AdapterCurrentLimit2<%02Xh> read failed !!", DEV_MP2764_REG_InputCurLimitSetting);
		}

		if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_TwoLevInputCurLimitSetting, &(reg_18.data))) {
			/* TIN_LIM2 = DEC(bits[6:0]) x 0.2 (ms) Range: 0ms to 25.4ms */
			reg_18.f.tin_lim2 = 10;  /* 2ms */
			/* TADP_PERIOD = DEC(bits[14:8]) x 0.2 (ms) Rang: 0ms to 25.4ms. */
			reg_18.f.tadp_period = 100; /* 20ms */
			st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_TwoLevInputCurLimitSetting, reg_18.data);
			LOG_INF("set DEV_MP2764_REG_TwoLevInputCurLimitSetting<%02Xh> %d returns %d", DEV_MP2764_REG_TwoLevInputCurLimitSetting, reg_18.data, st);
		} else {
			LOG_WRN("set DEV_MP2764_REG_TwoLevInputCurLimitSetting<%02Xh> read failed !!", DEV_MP2764_REG_TwoLevInputCurLimitSetting);
		}
	} else {
		/* disable the ac limit2 */
		if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ConfigReg2, &(reg_12.data))) {
			/* disable the second ac limit */
			reg_12.f.two_inlim_en = 0;
			st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ConfigReg2, reg_12.data);
			LOG_INF("set DEV_MP2764_REG_ConfigReg2<%02Xh> %d returns %d", DEV_MP2764_REG_ConfigReg2, reg_12.f.two_inlim_en, st);
		} else {
			LOG_WRN("set DEV_MP2764_REG_ConfigReg2<%02Xh> read failed !!", DEV_MP2764_REG_ConfigReg2);
		}
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_AdapterACProchotL
 *
 * @param [in]   chgId;       charger index
 * @param [in]   enable;      enable prochot or not
 * @param [in]   iin_phot2;   prochot2 threshold 110% or 150% of Iin Limit2
 * @return true as read pass
 * @note
 *  IIN_PHOT1_EN (110% IIN1 limit1) and IIN_PHOT2_EN (110% or 150% IIN Limit2) -> IIN_PHOT2
 */
bool amd_crb_drivers_mp2764_AdapterACProchotL(uint8_t chgId, bool enable, bool iin_phot2)
{
	DEV_MP2764_REG_1A reg_1A;
	DEV_MP2764_REG_1B reg_1B;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ProchotConfig1, &(reg_1A.data))) {
		reg_1A.f.iin_phot2 = (iin_phot2 == true) ? 1 : 0;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ProchotConfig1, reg_1A.data);
		LOG_INF("set DEV_MP2764_REG_ProchotConfig1<%02Xh> %d returns %d", DEV_MP2764_REG_ProchotConfig1, reg_1A.data, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_ProchotConfig1<%02Xh> read failed !!", DEV_MP2764_REG_ProchotConfig1);
	}

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ProchotConfig2, &(reg_1B.data))) {
		reg_1B.f.iin_phot1_en = 1;
		reg_1B.f.iin_phot2_en = 1;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ProchotConfig2, reg_1B.data);
		LOG_INF("set AdapterACProchotL<%02Xh> %d returns %d", DEV_MP2764_REG_ProchotConfig2, reg_1B.data, st);
	} else {
		LOG_WRN("set AdapterACProchotL<%02Xh> read failed !!", DEV_MP2764_REG_ProchotConfig2);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_AdapterDCProchotL
 *
 * @param [in]   chgId;       charger index
 * @param [in]   enable;      enable prochot or not
 * @param [in]   iin_phot2;   prochot2 threshold 110% or 150% of Iin Limit2
 * @return true as read pass
 * @note
 * enable the DC prochot and set the threshold
 */
bool amd_crb_drivers_mp2764_AdapterDCProchotL(uint8_t chgId, EM_DEV_MP2764_PROCHOT_DC_THRESHOLD current)
{
	DEV_MP2764_REG_1A reg_1A;
	DEV_MP2764_REG_1B reg_1B;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ProchotConfig2, &(reg_1B.data))) {
		reg_1B.f.bat_oc_phot_en = 1;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ProchotConfig2, reg_1B.data);
		LOG_INF("set AdapterACProchotL<%02Xh> %d returns %d", DEV_MP2764_REG_ProchotConfig2, reg_1B.data, st);
	} else {
		LOG_WRN("set AdapterACProchotL<%02Xh> read failed !!", DEV_MP2764_REG_ProchotConfig2);
	}

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ProchotConfig1, &(reg_1A.data))) {
		reg_1A.f.ibatt_prochot = current;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ProchotConfig1, reg_1A.data);
		LOG_INF("set DEV_MP2764_REG_ProchotConfig1<%02Xh> %d returns %d", DEV_MP2764_REG_ProchotConfig1, reg_1A.data, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_ProchotConfig1<%02Xh> read failed !!", DEV_MP2764_REG_ProchotConfig1);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_SetProchotDebounce
 *
 * @param [in]   chgId;       charger index
 * @param [in]   enable;      enable prochot or not
 * @return true as read pass
 * @note
 * set the prochot debounce
 */
bool amd_crb_drivers_mp2764_SetProchotDebounce(uint8_t chgId, EM_DEV_MP2764_PROCHOT_DEGL_IIN_PHOT1 debounceAc1,
		                                                      EM_DEV_MP2764_PROCHOT_DEGL_IIN_PHOT2 debounceAc2,
															  EM_DEV_MP2764_PROCHOT_DC_DEBOUNCE debounceDc)
{
	DEV_MP2764_REG_1A reg_1A;
	DEV_MP2764_REG_19 reg_19;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ProchotConfig1, &(reg_1A.data))) {
		reg_1A.f.t_degl_iin_phot1 = debounceAc1;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ProchotConfig1, reg_1A.data);
		LOG_INF("set DEV_MP2764_REG_ProchotConfig1<%02Xh> %d returns %d", DEV_MP2764_REG_ProchotConfig1, reg_1A.data, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_ProchotConfig1<%02Xh> read failed !!", DEV_MP2764_REG_ProchotConfig1);
	}

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ProchotConfig0, &(reg_19.data))) {
		reg_19.f.t_ibatt_oc_degl = debounceDc;
		reg_19.f.t_iin_phot2_degl = debounceAc2;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ProchotConfig0, reg_19.data);
		LOG_INF("set DEV_MP2764_REG_ProchotConfig0<%02Xh> %d returns %d", DEV_MP2764_REG_ProchotConfig0, reg_19.data, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_ProchotConfig0<%02Xh> read failed !!", DEV_MP2764_REG_ProchotConfig0);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_SetProchotDuration
 *
 * @param [in]   chgId;       charger index
 * @param [in]   duration;    prochot duration
 * @return true as read pass
 * @note
 * set the prochot duration for both AC and DC
 */
bool amd_crb_drivers_mp2764_SetProchotDuration(uint8_t chgId, EM_DEV_MP2764_PROCHOT_DURATION duration)
{
	DEV_MP2764_REG_19 reg_19;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ProchotConfig0, &(reg_19.data))) {
		reg_19.f.t_prochot_dur = duration;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ProchotConfig0, reg_19.data);
		LOG_INF("set DEV_MP2764_REG_ProchotConfig0<%02Xh> %d returns %d", DEV_MP2764_REG_ProchotConfig0, reg_19.data, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_ProchotConfig0<%02Xh> read failed !!", DEV_MP2764_REG_ProchotConfig0);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_fullChargeVoltage
 *
 * @param [in]   chgId;        charger index
 * @param [in]   fullVoltage;  full charger voltage mV
 * @return true as read pass
 * @note
 *  Minimum system voltage per cell setting for NVDC power path management control.
 */
bool amd_crb_drivers_mp2764_fullChargeVoltage(uint8_t chgId, uint32_t fullVoltage)
{
	DEV_MP2764_REG_15 reg_15;
	DEV_MP2764_REG_17 reg_17;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_BatRegulationVoltSetting, &(reg_15.data))) {
		/* VBATT_REG = DEC(bits[9:0]) x 5 (mV) */
		reg_15.f.vbtt_reg = fullVoltage / 5;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_BatRegulationVoltSetting, reg_15.data);
		LOG_INF("set DEV_MP2764_REG_BatRegulationVoltSetting<%02Xh> %d returns %d", DEV_MP2764_REG_BatRegulationVoltSetting, reg_15.f.vbtt_reg, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_BatRegulationVoltSetting<%02Xh> read failed !!", DEV_MP2764_REG_BatRegulationVoltSetting);
	}

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ConfigReg5, &(reg_17.data))) {
		/* 2 cell parallel with 2 cell series */
		reg_17.f.bat_num = 2;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ConfigReg5, reg_17.data);
		LOG_INF("set DEV_MP2764_REG_ConfigReg5<%02Xh> %d returns %d", DEV_MP2764_REG_ConfigReg5, reg_17.f.bat_num, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_ConfigReg5<%02Xh> read failed !!", DEV_MP2764_REG_ConfigReg5);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_chargeCurrent
 *
 * @param [in]   chgId;      charger index
 * @param [in]   current;    charging current
 * @return true as read pass
 * @note
 *  set the charging current
 */
bool amd_crb_drivers_mp2764_chargeCurrent(uint8_t chgId, uint16_t current)
{
	DEV_MP2764_REG_14 reg_14;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ChargeCurSetting1, &(reg_14.data))) {
		/* write Icc to zero to disable the charging, and non zero to enable */
		reg_14.f.icc = (uint8_t) (current * 10 / 625);
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ChargeCurSetting1, reg_14.data);
		LOG_INF("set DEV_MP2764_REG_ChargeCurSetting1<%02Xh> %d returns %d", DEV_MP2764_REG_ChargeCurSetting1, reg_14.f.icc, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_ChargeCurSetting1<%02Xh> read failed !!", DEV_MP2764_REG_ChargeCurSetting1);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_interruptMask
 *
 * @param [in]   chgId;      charger index
 * @param [in]   mask;       mask interrupt event
 * @return true as read pass
 * @note
 *  mask the interrupt event or not
 */
bool amd_crb_drivers_mp2764_interruptMask(uint8_t chgId, uint16_t mask)
{
	DEV_MP2764_REG_1C reg_1C;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_IntMaskCtrl, &(reg_1C.data))) {
		/* 0: masked and 1: not masked */
		reg_1C.data = mask;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_IntMaskCtrl, reg_1C.data);
		LOG_INF("set DEV_MP2764_REG_IntMaskCtrl<%02Xh> %d returns %d", DEV_MP2764_REG_IntMaskCtrl, reg_1C.data, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_IntMaskCtrl<%02Xh> read failed !!", DEV_MP2764_REG_IntMaskCtrl);
	}

	return st;
}

/**
 * amd_crb_drivers_mp2764_faultStatus
 *
 * @param [in]   chgId;      charger index
 * @param [in]   status;     pointer for status
 * @return true as read pass
 * @note
 *  get charger fault status
 */
bool amd_crb_drivers_mp2764_faultStatus(uint8_t chgId, uint32_t *status)
{
	DEV_MP2764_REG_4C reg_4C;
	DEV_MP2764_REG_4C reg_4D;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_StatusFaultReg0, &(reg_4C.data))) {
		*status = reg_4C.data;
	} else {
		LOG_WRN("set DEV_MP2764_REG_StatusFaultReg0<%02Xh> read failed !!", DEV_MP2764_REG_StatusFaultReg0);
	}

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_StatusFaultReg1, &(reg_4D.data))) {
		*status += reg_4D.data << 16;
	} else {
		LOG_WRN("set DEV_MP2764_REG_StatusFaultReg1<%02Xh> read failed !!", DEV_MP2764_REG_StatusFaultReg1);
	}

	return st;
}


/**
 * amd_crb_drivers_mp2764_batteryOnly
 *
 * @param [in]   chgId;      charger index
 * @param [in]   enable;     enter low power mode or not
 * @return true as read pass
 * @note
 *  enable low power mode in DC only status
 */
bool amd_crb_drivers_mp2764_batteryOnly(uint8_t chgId, bool enable)
{
	DEV_MP2764_REG_11 reg_11;
	bool st = false;

	if (amd_crb_drivers_mp2764_i32Read(chgId, DEV_MP2764_REG_ConfigReg1, &(reg_11.data))) {
		/* 0: masked and 1: not masked */
		reg_11.f.low_pwr_en = enable;
		st = amd_crb_drivers_mp2764_i32Write(chgId, DEV_MP2764_REG_ConfigReg1, reg_11.data);
		LOG_INF("set DEV_MP2764_REG_ConfigReg1<%02Xh> %d returns %d", DEV_MP2764_REG_ConfigReg1, reg_11.f.low_pwr_en, st);
	} else {
		LOG_WRN("set DEV_MP2764_REG_ConfigReg1<%02Xh> read failed !!", DEV_MP2764_REG_ConfigReg1);
	}

	return st;
}

/**
 * _dev_charger_setProchot_handler
 *
 * @param [in]   w;      work item pointer
 * @return void
 * @note
 *  work handler for setting prochot gate
 */
static void _dev_charger_setProchot_handler (struct k_work *w)
{
	if (pfnSetupProchotGate) {
		pfnSetupProchotGate();
	} else {
		LOG_ERR("No set prochot handler registered!");
	}
}

/**
 * amd_crb_drivers_mp2764_setProchot
 *
 * @param void
 * @return void
 * @note
 *  submit work to set prochot gate
 */
void amd_crb_drivers_mp2764_setProchot (void)
{
	k_work_submit(&setProchot_work);
}

/**
 * amd_crb_drivers_mp2764_prochot_register
 *
 * @param [in]   handler;    callback function for prochot gate setup
 * @return void
 * @note
 *  register the prochot gate setup handler and initialize work item
 */
void amd_crb_drivers_mp2764_prochot_register(pfnSetupProchotGate_t handler)
{
	pfnSetupProchotGate = handler;
	k_work_init(&setProchot_work, _dev_charger_setProchot_handler);
}