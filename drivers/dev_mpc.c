/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_mpc.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include <stdarg.h>
#include <dev_utility.h>
#include <i2c_hub.h>
#include <board_id.h>
#include "debug_info.h"
#include "amd_crb_modbusrtu_driver.h"

LOG_MODULE_REGISTER(mpc, CONFIG_MPC_LOG_LEVEL);

#ifndef MPC_I2C_ADDRESS
#define MPC_I2C_ADDRESS      0x37
#endif

#ifndef MPC_SAME_POSTCODE_RETRY_COUNT
#define MPC_SAME_POSTCODE_RETRY_COUNT  (8)
#endif

uint8_t mpcDev = I2C_2;
static bool    _s_forceWriteFlag = false;
static uint32_t _s_tmp_isstring = 0;
static FRAME_BUF_MPC_32BIT _s_frameUser;
static FRAME_BUF_MPC_32BIT _s_frameWorkCopy;
static FRAME_BUF_MPC_32BIT _s_frameFlushed;
static bool  _s_enable_mpc = true; /* used to global enable or disable MPC */
/* mpc thread */
static struct k_sem mpc_sem;
static struct k_thread mpc_thread_data;
static K_THREAD_STACK_DEFINE(mpc_thread_stack, 1024);
extern uint8_t g_WAIE_RITE;
extern uint8_t waie_syc_flag;
static dev_mpc_update_callback_t _s_update_callback = NULL;
static uint8_t _s_digMap[16] = {
	0x00, // 0
	0x01, // 1
	0x02, // 2
	0x03, // 3
	0x04, // 4
	0x05, // 5
	0x06, // 6
	0x07, // 7
	0x08, // 8
	0x09, // 9
	0x0A, // A
	0x0B, // B
	0x0C, // C
	0x0D, // D
	0x0E, // E
	0x0F  // F
};

#if CONFIG_MODBUSRTU

/**
 * _mpc_regAccess_uart
 *
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]  slvAddr;      modbus address
 * @param [in]      reg;     registers  offset
 * @param [in]     pBuf;     data buffer pointer
 * @param [in]      len;     data length/register width
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  uart read and write to the MPC module
 */
static uint32_t _mpc_regAccess_uart(bool isRead, uint8_t slvAddr, uint16_t reg, uint8_t * pBuf, uint32_t len)
{
	int ret = 0;
	if (isRead == false)
		ret = amd_crb_modbusrtu_write(slvAddr, reg, pBuf, len);
	else
		ret = amd_crb_modbusrtu_read(slvAddr, reg, NULL, len);

	return ret;
}
#else
/**
 * _mpc_regAccess
 *
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]  slvAddr;     I2C address
 * @param [in]      reg;     registers  offset
 * @param [in]     pBuf;     data buffer pointer
 * @param [in]      len;     data length/register width
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  I2C read and write to the MPC module
 */
static uint32_t _mpc_regAccess(bool isRead, uint8_t slvAddr, uint16_t reg, void * pBuf, uint32_t len)
{
	uint8_t retry = 1;
	uint32_t ret = 0;

	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			ret = i2c_hub_burst_read(mpcDev, slvAddr, reg, pBuf, len);
			if (ret != 0)
				continue;
		} else {
			ret = i2c_hub_burst_write(mpcDev, slvAddr, reg, pBuf, len);
		}
#if (CONFIG_LOG)
		LOG_CLEARBUF;
		for ( uint32_t i = 0; i < len; i++ ) {
			LOG_APPEND(" %02X", *((uint8_t *)pBuf + i));
		}
		LOG_DBG("Access MPC %s slv%02X [%02X], ret %d, data(%d)%s", isRead ? "R" : "W", slvAddr, reg, ret, len, LOG_BUF);
#endif
		if (ret == 0)
			return ret;
	}
	if (!retry) {
		info_value_increase(INFO_I2C_MPC, 1);
		LOG_ERR("[!!Fatal error!!] on %s MPC slv%02X [%02X], ret %d", isRead ? "R" : "W", slvAddr, reg, ret);
		_s_forceWriteFlag = true;
	}
	return ret;
}

#endif

/**
 * _mpc_setData
 *
 * @param [in]   	slv;     I2C address
 * @param [in]     data;     set data
 * @return 0 If successful.
 * @return non-zero on error.
 *
 * @note
 *  set data to mpc
 */
static int _mpc_setData(uint8_t slv, uint64_t data)
{
	int ret;
#if CONFIG_MODBUSRTU
	ret = _mpc_regAccess_uart(0, slv, DEV_MPC_POSTCODE_REG, (uint8_t *) &data, 8);
#else
	ret = _mpc_regAccess(0, slv, DEV_MPC_POSTCODE_REG, (uint8_t *) &data, 8);
#endif
	return ret;
}

/**
 * _mpc_stringOn
 *
 * @param [in]   	slv;     I2C address
 *
 * @note
 *  enable the string mode for mpc
 */
static void _mpc_stringOn(uint8_t slv)
{
	uint8_t w_data =1;
#if CONFIG_MODBUSRTU
	uint16_t SendBuf;
	SendBuf = w_data;
	 _mpc_regAccess_uart(0, slv, DEV_MPC_DISPLAY_MODE_REG, (uint8_t*)&SendBuf, 2);
#else
	_mpc_regAccess(0, slv, DEV_MPC_DISPLAY_MODE_REG, (uint8_t *) &w_data, 1);
#endif
}

/**
 * _mpc_stringOff
 *
 * @param [in]   	slv;     I2C address
 *
 * @note
 *  disable the string mode for mpc
 */
static void _mpc_stringOff(uint8_t slv)
{
	 uint8_t w_data =0;
#if CONFIG_MODBUSRTU
	uint16_t SendBuf;
	SendBuf = w_data;
	 _mpc_regAccess_uart(0, slv, DEV_MPC_DISPLAY_MODE_REG, (uint8_t*)&SendBuf, 2);
#else
	 _mpc_regAccess(0, slv, DEV_MPC_DISPLAY_MODE_REG, (uint8_t *) &w_data, 1);
#endif
}

/**
 * _mpc_turnOn
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]  brightness;     NA
 * @param [in]   	 limit;     NA
 *
 * @note
 *  turn on the MPC
 */
static void _mpc_turnOn(uint8_t slv, uint8_t brightness, uint8_t limit)
{
	uint8_t u8val = 0; 
#if CONFIG_MODBUSRTU
	uint16_t SendBuf;
	SendBuf = u8val;
	 _mpc_regAccess_uart(0, slv, DEV_MPC_DIGIT_CTRL_REG, (uint8_t*)&SendBuf, 2);
#else
	_mpc_regAccess(0, slv, DEV_MPC_DIGIT_CTRL_REG, (uint8_t *) &u8val, 1);
#endif
}

/**
 * _mpc_turnOff
 *
 * @param [in]   	   slv;     I2C address
 *
 * @note
 *  turn off the MPC
 */
// static void _mpc_turnOff(uint8_t slv)
// {
// #if CONFIG_MODBUSRTU
// 	uint8_t u8val[2] = {0xff, 0xff}; 
// 	 _mpc_regAccess_uart(0, slv, DEV_MPC_DIGIT_CTRL_REG, u8val, 2);
// #else
// 	_mpc_regAccess(0, slv, DEV_MPC_DIGIT_CTRL_REG, (uint8_t *) &u8val, 1);
// #endif
// }

/**
 * _mpc_setlength
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]   	   len;     set the data length
 *
 * @note
 *  set the data length of the MPC
 */
static void _mpc_setlength(uint8_t slv,uint8_t len)
{
	uint8_t u8val = len; 
#if CONFIG_MODBUSRTU
	uint16_t SendBuf;
	SendBuf = u8val;
	_mpc_regAccess_uart(0, slv, DEV_MPC_DATALENGH_REG, (uint8_t*)&SendBuf, 2);
#else
	_mpc_regAccess(0, slv, DEV_MPC_DATALENGH_REG, (uint8_t *) &u8val, 1);
#endif
}

/**
 * _mpc_setboardid
 *
 * @param [in]   	   slv;     I2C address
 *
 * @note
 *  set the board id to the MPC
 */
static void _mpc_setboardid(uint8_t slv)
{
	uint8_t u8val = brdId();
#if CONFIG_MODBUSRTU
	uint16_t SendBuf;
	SendBuf = u8val;
	_mpc_regAccess_uart(0, slv, DEV_MPC_BOARDID_REG, (uint8_t*)&SendBuf, 2);
#else
	_mpc_regAccess(0, slv, DEV_MPC_BOARDID_REG, (uint8_t *) &u8val, 1);
#endif
}

/**
 * _mpc_setpost_counter
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]     counter;     post counter value
 *
 * @note
 *  set the post counter to the MPC
 */
static void _mpc_setpost_counter(uint8_t slv,uint16_t counter)
{
#if CONFIG_MODBUSRTU
	uint16_t SendBuf;
	SendBuf = counter;
	_mpc_regAccess_uart(0, slv, DEV_MPC_POSTCODE_COUNT_REG, (uint8_t*)&SendBuf, 2);
#else
	_mpc_regAccess(0, slv, DEV_MPC_BOARDID_REG, (uint8_t *) &counter, 1);
#endif
}

/**
 * _mpc_setEcVersion
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]     version;     version pointer
 *
 * @note
 *  set the ec version to the MPC
 */
static void _mpc_setEcVersion(uint8_t slv, uint8_t* version)
{
#if CONFIG_MODBUSRTU
	_mpc_regAccess_uart(0, slv, DEV_MPC_EC_VERSION_REG, (uint8_t*) version, 5);
#else
	_mpc_regAccess(0, slv, DEV_MPC_EC_VERSION_REG, (uint8_t *) version, 5);
#endif
}

/**
 * _mpc_setPd0Version
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]     version;     version pointer
 *
 * @note
 *  set the pd0 version to the MPC
 */
static void _mpc_setPd0Version(uint8_t slv, uint8_t* version)
{
#if CONFIG_MODBUSRTU
	_mpc_regAccess_uart(0, slv, DEV_MPC_PD0_VERSION_REG, (uint8_t*) version, 3);
#else
	_mpc_regAccess(0, slv, DEV_MPC_PD0_VERSION_REG, (uint8_t *) version, 3);
#endif

}

/**
 * _mpc_setPd1Version
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]     version;     version pointer
 *
 * @note
 *  set the pd1 version to the MPC
 */
static void _mpc_setPd1Version(uint8_t slv, uint8_t* version)
{
#if CONFIG_MODBUSRTU
	_mpc_regAccess_uart(0, slv, DEV_MPC_PD1_VERSION_REG, (uint8_t*) version, 3);
#else
	_mpc_regAccess(0, slv, DEV_MPC_PD1_VERSION_REG, (uint8_t *) version, 3);
#endif

}

/**
 * _mpc_setMac
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]         mac;     Mac address
 * @param [in]         len;     Mac address length
 *
 * @note
 *  set the mac address version to the MPC
 */
static void _mpc_setMac(uint8_t slv, uint8_t* mac, uint8_t len)
{
#if CONFIG_MODBUSRTU
	_mpc_regAccess_uart(0, slv, DEV_MPC_MAC_REG, (uint8_t*) mac, len);
#else
	_mpc_regAccess(0, slv, DEV_MPC_MAC_REG, (uint8_t *) mac, len);
#endif
}

/**
 * _mpc_setCpuId
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]         cpuid;   cpu id
 * @param [in]         len;     cpu id length
 *
 * @note
 *  set the cpu id to the MPC
 */
static void _mpc_setCpuId(uint8_t slv, uint8_t* cpuid, uint8_t len)
{
#if CONFIG_MODBUSRTU
	_mpc_regAccess_uart(0, slv, DEV_SOCID_EAX_REG, (uint8_t*) cpuid, len);
#else
	_mpc_regAccess(0, slv, DEV_SOCID_EAX_REG, (uint8_t *) cpuid, len);
#endif
}

/**
 * _mpc_setBver
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]         ver;     bios version
 * @param [in]         len;     bios version length
 *
 * @note
 *  set the bios version to the MPC
 */
static void _mpc_setBver(uint8_t slv, uint8_t* ver, uint8_t len)
{
#if CONFIG_MODBUSRTU
	_mpc_regAccess_uart(0, slv, DEV_MPC_BVER_REG, (uint8_t*) ver, len);
#else
	_mpc_regAccess(0, slv, DEV_MPC_BVER_REG, (uint8_t *) ver, len);

#endif
}
 unsigned short count2 =0;

/**
 * _mpc_render
 *
 * @note
 *  update mpc state machine
 */
static void _mpc_render (void)
{
	static uint64_t s_last_rawData = 0;
	bool forceWrite = false;
	bool onOff = false;
	static int ret = 0;
	static int retry = 0;
	int key = irq_lock();
	memcpy(&_s_frameWorkCopy, &_s_frameUser, sizeof(FRAME_BUF_MPC_32BIT));
	if (_s_forceWriteFlag) {
		forceWrite = true;
		_s_forceWriteFlag = false;
	}

	irq_unlock(key);
	uint8_t u8tmp;

	/*
	 * On/Off
	 */
	if (forceWrite) {
		forceWrite = true;
		if(_s_frameUser.OnOrOff) {
			_mpc_turnOn(MPC_I2C_ADDRESS, u8tmp, 3);
		}
		// else {
		// 	_mpc_turnOff(MPC_I2C_ADDRESS);
		// }
		onOff = true;
	}

	/* When MPC is disabled (turned off), keep displaying dashes */
	if (!_s_enable_mpc) {
		/* Ensure dashes are displayed when disabled */
			_s_tmp_isstring = true;
			_mpc_stringOn(MPC_I2C_ADDRESS);
			k_msleep(1);
			uint64_t dash_data = 0x2D2D2D2D2D2D2D2DULL; /* ASCII '-' = 0x2D */
			_mpc_setData(MPC_I2C_ADDRESS, dash_data);
			s_last_rawData = dash_data;
		return;
	}
	
	/*
	 * data
	 */
	uint64_t datahigh = (uint64_t)_s_frameUser.rawHigh << 32;
	uint64_t datalow = _s_frameUser.rawLow;
	uint64_t data = datahigh + datalow;

	if (s_last_rawData == data) {
		if (ret == 0) {
			goto skip_postcode;
		}

		if (++retry > MPC_SAME_POSTCODE_RETRY_COUNT) {
			/* Give up after 3 failed attempts with same data */
			goto skip_postcode;
		}
	} else {
		retry = 0;
	}

	LOG_DBG("_mpc_render rawLow %x", _s_frameUser.rawLow);
	LOG_DBG("_mpc_render rawHigh%x", _s_frameUser.rawHigh);

	if(_s_tmp_isstring !=_s_frameUser.isstring) {
		_s_tmp_isstring = _s_frameUser.isstring;
		if(_s_tmp_isstring) {
			_mpc_stringOn(MPC_I2C_ADDRESS);
		}
		else {
			_mpc_stringOff(MPC_I2C_ADDRESS);
		}
		onOff = true;
	}

	if (onOff) {
		k_msleep(1);
	}
	ret = _mpc_setData(MPC_I2C_ADDRESS, data);
	if (0) {
		_mpc_setpost_counter(MPC_I2C_ADDRESS, count2++);
	}
	s_last_rawData = data;

skip_postcode:

#if CONFIG_SFH
#if CONFIG_MODBUSRTU
	unsigned char pack_i =0;
	if(g_WAIE_RITE)   // only if WAITE is enable then loop read waie
	{
		for(pack_i =0;pack_i<18;pack_i++)
		{
			_mpc_regAccess_uart(1, MPC_I2C_ADDRESS, DEV_WAIE_POWER_REG+pack_i*50, NULL, 50);
			k_msleep(10);
		}
		waie_syc_flag = 1;
	}
#endif
#endif
	memcpy(&_s_frameFlushed, &_s_frameWorkCopy, sizeof(FRAME_BUF_MPC_32BIT));
}

/**
 * _mpc_timerCallback
 *
 * @param [in]   timer;     pointer to timer structure
 *
 * @note
 * mpc timer callback
 */
static void _mpc_timerCallback (struct k_timer *timer)
{
	k_sem_give(&mpc_sem);
}

K_TIMER_DEFINE(mpc_timer, _mpc_timerCallback, NULL);

/**
 * _MPC_threadFunction
 *
 * @param [in]   arg1;     thread argument 1 (unused)
 * @param [in]   arg2;     thread argument 2 (unused)
 * @param [in]   arg3;     thread argument 3 (unused)
 *
 * @note
 *  mpc thread function
 */
static void _MPC_threadFunction(void *arg1, void *arg2, void *arg3)
{
	while (1) {
		k_sem_take(&mpc_sem, K_FOREVER);

		if ((app_pseq_systemState() != SYSTEM_G3_STATE) && (app_pseq_systemState() != SYSTEM_S5_STATE)) {
			_mpc_render();
		}

		/* Call the registered callback if available */
		if (_s_update_callback != NULL) {
			_s_update_callback();
		}
	}
}

/***************************************
 *
 * MPC 32-bit mode
 *
 ***************************************/

/**
 * dev_mpc_init
 *
 * @param [in]   	   i2cPort;     I2C port
 *
 * @note
 *  init the mpc module
 */
void dev_mpc_init(uint8_t i2cPort)
{
	mpcDev = i2cPort;
	memset(&_s_frameUser, 0, sizeof(FRAME_BUF_MPC_32BIT));
	memset(&_s_frameWorkCopy, 0, sizeof(FRAME_BUF_MPC_32BIT));
	memset(&_s_frameFlushed, 0, sizeof(FRAME_BUF_MPC_32BIT));

	_s_frameUser.brightnessAll = 16;
	_s_frameUser.brightnessHalf = 8;

	k_sem_init(&mpc_sem, 0, 1);
	k_thread_create(&mpc_thread_data, mpc_thread_stack,
					K_THREAD_STACK_SIZEOF(mpc_thread_stack),
					_MPC_threadFunction,
					NULL, NULL, NULL,
					K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
	k_thread_name_set(&mpc_thread_data, "mpc_thread");
	k_timer_start(&mpc_timer, K_MSEC(200), K_MSEC(200));
#if CONFIG_MODBUSRTU
	amd_crb_modbusrtu_init();
#endif
	_mpc_setlength(MPC_I2C_ADDRESS, 8);
	_mpc_setboardid(MPC_I2C_ADDRESS);
	uint64_t data =0;
	_mpc_setData(MPC_I2C_ADDRESS, data);
}

/**
 * dev_mpc_setVersion
 *
 * @param [in]   	    ecVer;     EC version
 * @param [in]   	   pd0Ver;     PD0 version
 * @param [in]   	   pd1Ver;     PD1 version
 *
 * @note
 *  report EC and PD version to MPC
 */
void dev_mpc_setEcVersion(uint8_t* ecVer, uint8_t* pd0Ver, uint8_t* pd1Ver)
{
	_mpc_setEcVersion(MPC_I2C_ADDRESS, ecVer);
	_mpc_setPd0Version(MPC_I2C_ADDRESS, pd0Ver);
	_mpc_setPd1Version(MPC_I2C_ADDRESS, pd1Ver);
}

/**
 * dev_mpc_setBiosVersion
 *
 * @param [in]   	    biosVer;     Bios version
 * @param [in]   	     length;     ver length
 *
 * @note
 *  report bios version to MPC
 */
void dev_mpc_setBiosVersion(uint8_t* biosVer, uint8_t length)
{
	_mpc_setBver(MPC_I2C_ADDRESS, biosVer, length);
}

/**
 * dev_mpc_setMacVersion
 *
 * @param [in]   	     macVer;     Mac address version
 * @param [in]   	     length;     ver length
 *
 * @note
 *  report mac address to MPC
 */
void dev_mpc_setMacVersion(uint8_t* macVer, uint8_t length)
{
	_mpc_setMac(MPC_I2C_ADDRESS, macVer, length);
}

/**
 * dev_mpc_setCpuId
 *
 * @param [in]   	     cpuId;      cpu id
 * @param [in]   	     length;     data length
 *
 * @note
 *  report cpu id to MPC
 */
void dev_mpc_setCpuId(uint8_t* cpuId, uint8_t length)
{
	_mpc_setCpuId(MPC_I2C_ADDRESS, cpuId, length);
}

/**
 * dev_mpc_32bit_reset
 *
 * @note
 *  reset the mpc module
 */
void dev_mpc_32bit_reset(void)
{
	
	int key = irq_lock();
	/*
	 * Reset 7-Seg LED to "0.0.0.0.0.0.0.0."
	 */
	_s_frameUser.scanLimit = 8;
	_s_frameUser.DPs = 0xFF;
	_s_frameUser.rawLow  = 0; // 00000000
	_s_frameUser.rawHigh = 0; // 00000000

	irq_unlock(key);
	k_sem_give(&mpc_sem);
}

/**
 * dev_mpc_32bit_turnOn
 *
 * @param [in]   	   brightness;     NA
 *
 * @note
 *  turn on the mpc postcode module
 */
void dev_mpc_32bit_turnOn(uint8_t brightness)
{
	
	int key = irq_lock();
    _s_frameUser.OnOrOff = true;
	_s_forceWriteFlag = true;
	_s_enable_mpc = true;

	irq_unlock(key);
}

/**
 * dev_mpc_32bit_turnOff
 *
 * @note
 *  turn off the mpc postcode module
 */
void dev_mpc_32bit_turnOff(void)
{
	int key = irq_lock();
	 _s_frameUser.OnOrOff = false;
	_s_forceWriteFlag = true;
	_s_enable_mpc = false;

	irq_unlock(key);
	k_sem_give(&mpc_sem);
}

/**
 * dev_mpc_32bit_postcode
 *
 * @param [in]   	   pc;     postcode
 * @param [in]   	   dp;     display
 * @param [in]      limit;     limit
 *
 * @note
 *  show the input postcode number
 */
void dev_mpc_32bit_postcode(uint32_t pc, uint8_t dp, uint8_t limit)
{
	LOG_DBG("dev_mpc_32bit_postcode data %08x", pc);
	uint64_t rawData = 0;
	int key = irq_lock();
	for (uint8_t i = 0; i < 8; i++) {
		rawData |= (((uint64_t)_s_digMap[((pc >> (i * 4)) & 0x0F)]) << (i * 8));
	}

	_s_frameUser.scanLimit = limit;
	_s_frameUser.rawLow = pc;
	_s_frameUser.rawHigh = 0;
	_s_frameUser.DPs = dp;
	_s_frameUser.isstring = 0;

	irq_unlock(key);
}

/**
 * dev_mpc_32bit_scanLimit
 *
 * @param [in]      limit;     limit
 *
 * @note:
 * 32-bit scan limit
 * limit 0~8
 *       0 - turn off all LEDs
 *       1 - only turn on the lowest LED.
 *       ... ...
 *       8 - turn on all LED
 */
void dev_mpc_32bit_scanLimit(uint8_t limit)
{
	_s_frameUser.scanLimit = limit;

	k_sem_give(&mpc_sem);
}

/**
 * dev_mpc_32bit_DPs
 *
 * @param [in]      turnOnBits;     bits to turn on
 *
 * @note:
 * For example turnOnBits:
 *    c8bit(1100, 0101) -> [X.X.X X X X.X X.]
 *    c8bit(1111, 1111) -> all DPs on
 *    c8bit(0000, 0000) -> all DPs off
 */
void dev_mpc_32bit_DPs(uint8_t turnOnBits)
{
	_s_frameUser.DPs = turnOnBits;

	k_sem_give(&mpc_sem);
}

/**
 * dev_mpc_show
 *
 * @param [in]        rawH;     raw data high
 * @param [in]        rawL;     raw data low
 * @param [in]          dp;     display
 * @param [in]       limit;     limit
 * @param [in]  forceWrite;     ture is force
 * @param [in]    isstring;     is string
 *
 * @note
 *  mpc to show the value
 */
void dev_mpc_show(uint32_t rawH, uint32_t rawL, uint8_t dp, uint8_t limit, bool forceWrite,uint8_t isstring)
{
	int key = irq_lock();
	_s_frameUser.scanLimit = limit;
	_s_frameUser.rawLow = rawL;
	_s_frameUser.rawHigh = rawH;
	_s_frameUser.DPs = dp;
	_s_frameUser.isstring = isstring;
	if (forceWrite)
		_s_forceWriteFlag = true;

	irq_unlock(key);
}

/**
 * dev_mpc_print
 *
 * @param [in]      format;     format string
 * @param [in]         ...;     variable arguments
 *
 * @return true if successful, false otherwise
 *
 * @note
 *  mpc to show the chart
 */
bool dev_mpc_print(const char * format, ...)
{
  va_list aptr;
  int strl;
  char buf[20];

	if (NULL == format)
		return false;

  va_start(aptr, format);
  strl = vsnprintk(buf, sizeof(buf), format, aptr);
  va_end(aptr);

  if (strl < 0)
    return false;

  assert (strl < sizeof(buf));

  uint8_t led = 8;
  uint8_t dp = 0;
  uint8_t encode = 0;
  uint64_t framebuf = 0ull;
  bool isDot;
  for (uint8_t idx = 0; idx < strl && 0 != led; idx++) {
    isDot = false;

    char c = buf[idx];
	  encode = c;
		
		led --;
    framebuf |= ((uint64_t) encode) << (8 * led);
  
  }

  uint32_t rawH = (uint32_t)(framebuf >> 32);
  uint32_t rawL = (uint32_t)(framebuf & 0xFFFFFFFFul);
  dev_mpc_show(rawH, rawL, dp, 8, false,1);

  return true;
}


/**
 * dev_mpc_set_sts
 *
 * @param [in]      status;     enable or disable status
 *
 * @note
 *  set MPC global enable and disable status
 */
void dev_mpc_set_sts(bool status)
{
	/* this flag only inhibits rander */
	_s_enable_mpc = status;
}

/**
 * dev_mpc_get_sts
 *
 * @return true if MPC is enabled, false otherwise
 *
 * @note
 *  get MPC global enable and disable status
 */
bool dev_mpc_get_sts(void)
{
	return _s_enable_mpc;
}

/**
 * dev_mpc_setCpuOpn
 *
 * @param [in]      cpuOpn;     CPU OPN data
 * @param [in]      length;     data length
 *
 * @note
 *  set CPU OPN to MPC
 */
void dev_mpc_setCpuOpn(uint8_t* cpuOpn, uint8_t length)
{
#if CONFIG_MODBUSRTU
	_mpc_regAccess_uart(0, MPC_I2C_ADDRESS, DEV_SOCID_OPN, (uint8_t *) cpuOpn, length);
#endif
}

/**
 * dev_mpc_setCpuSN
 *
 * @param [in]       cpuSn;     CPU serial number data
 * @param [in]      length;     data length
 *
 * @note
 *  set CPU serial number to MPC
 */
void dev_mpc_setCpuSN(uint8_t* cpuSn, uint8_t length)
{
#if CONFIG_MODBUSRTU
	_mpc_regAccess_uart(0, MPC_I2C_ADDRESS, DEV_SOCID_SN, (uint8_t *) cpuSn, length);
#endif
}

/**
 * dev_mpc_register_update_callback
 *
 * @param [in]   callback;     callback function pointer
 *
 * @note
 *  Register a callback function to be called when MPC update is requested
 */
void dev_mpc_register_update_callback(dev_mpc_update_callback_t callback)
{
	_s_update_callback = callback;
}