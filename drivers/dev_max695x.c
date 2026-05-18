/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_max695x.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include <stdarg.h>
#include <dev_utility.h>
#include <i2c_hub.h>
#include "debug_info.h"
LOG_MODULE_REGISTER(max695x, CONFIG_MAX695_LOG_LEVEL);

uint8_t max695Dev = I2C_2;

#ifndef MAX695X_LOW_I2C_ADDRESS
#define MAX695X_LOW_I2C_ADDRESS   0x38
#endif

#ifndef MAX695X_HIGH_I2C_ADDRESS
#define MAX695X_HIGH_I2C_ADDRESS   0x39
#endif

static bool    _s_forceWriteFlag = false;

/**
 * _max695x_regAccess
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
 *  I2C read and write to max695
 */
static uint32_t _max695x_regAccess(bool isRead, uint8_t slvAddr, uint8_t reg, void * pBuf, uint32_t len)
{
	uint8_t retry = 3;
	uint32_t ret = 0;

	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			ret = i2c_hub_burst_read(max695Dev, slvAddr, reg, pBuf, len);
			if (ret != 0)
				continue;
		} else {
			ret = i2c_hub_burst_write(max695Dev, slvAddr, reg, pBuf, len);
		}
#if (CONFIG_LOG)
		LOG_CLEARBUF;
		for ( uint32_t i = 0; i < len; i++ ) {
			LOG_APPEND(" %02X", *((uint8_t *)pBuf + i));
		}
		LOG_DBG("Access MAX695x %s slv%02X [%02X], ret %d, data(%d)%s", isRead ? "R" : "W", slvAddr, reg, ret, len, LOG_BUF);
#endif
		if (ret == 0)
			return ret;
	}
	if (!retry) {
		info_value_increase(INFO_I2C_MAX695x, 1);
		LOG_ERR("[!!Fatal error!!] on %s MAX695x slv%02X [%02X], ret %d", isRead ? "R" : "W", slvAddr, reg, ret);
		_s_forceWriteFlag = true;
	}
	return ret;
}

/**
 * _max695x_setIntensity
 *
 * @param [in]      slv;     I2C address
 * @param [in]    level;     intensity level
 * 
 * @return 0 If successful.
 * @return -EIO General input / output error.
 */
static void _max695x_setIntensity(uint8_t slv, uint8_t level)
{
	_max695x_regAccess(0, slv, DEV_MAX695X_INTENSITY_REG, (uint8_t *) &level, 1);
}

/**
 * _max695x_setDPs
 *
 * @param [in]   	slv;     I2C address
 * @param [in]      dp;    Digital point.
 *
 * @note
 *  set digital point
 */
static void _max695x_setDPs(uint8_t slv, uint8_t dp)
{
	uint8_t u8tmp = (dp & 0x0F) << 4;
	_max695x_regAccess(0, slv,  DEV_MAX695X_DP_REG, (uint8_t *) &u8tmp, 1);
}

/**
 * _max695x_setData
 *
 * @param [in]   	slv;     I2C address
 * @param [in]     data;     set data
 *
 * @note
 *  set data to max695
 */
static void _max695x_setData(uint8_t slv, uint32_t data)
{
	_max695x_regAccess(0, slv, DEV_MAX695X_P0_REG, (uint8_t *) &data, 4);
}

/**
 * _max695x_turnOn
 *
 * @param [in]   	   slv;     I2C address
 * @param [in]  brightness;     NA
 * @param [in]   	 limit;     NA
 *
 * @note
 *  turn on max695
 */
static void _max695x_turnOn(uint8_t slv, uint8_t brightness, uint8_t limit)
{
	uint8_t u8val = 0x21; // Normal operation + Digit and segment data are cleared
	_max695x_regAccess(0, slv, DEV_MAX695X_CONFIG_REG, (uint8_t *) &u8val, 1);

	u8val = 0x00; // decode mode == off
	_max695x_regAccess(0, slv, DEV_MAX695X_DECODE_REG, (uint8_t *) &u8val, 1);

	/* Set brightness 0~63 (63 max on) */
	_max695x_setIntensity(slv, brightness);

	/* Set all 4 digites on (scan limit = 3) */
	u8val = limit;
	_max695x_regAccess(0, slv, DEV_MAX695X_SCANLIMIT_REG, (uint8_t *) &u8val, 1);
}

/**
 * _max695x_turnOff
 *
 * @param [in]   	   slv;     I2C address
 *
 * @note
 *  turn off the max695x
 */
static void _max695x_turnOff(uint8_t slv)
{
	uint8_t u8val = 0x20; // Shutdown mode + Digit and segment data are cleared
	_max695x_regAccess(0, slv, DEV_MAX695X_CONFIG_REG, (uint8_t *) &u8val, 1);

	u8val = 0;
	_max695x_regAccess(0, slv, DEV_MAX695X_SCANLIMIT_REG, (uint8_t *) &u8val, 1);
}

typedef struct {
	uint8_t  scanLimit;
	uint8_t  brightnessAll;
	uint8_t  brightnessHalf;
	uint8_t  DPs;
	uint32_t rawLow;
	uint32_t rawHigh;
} FRAME_BUF_MAX695x_32BIT;

static FRAME_BUF_MAX695x_32BIT _s_frameUser;
static FRAME_BUF_MAX695x_32BIT _s_frameWorkCopy;
static FRAME_BUF_MAX695x_32BIT _s_frameFlushed;
static bool  _s_enable_max695x = true; /* used to global enable or disable max695x */

/**
 * _max695x_render
 *
 * @note
 *  update max695x state machine
 */
static void _max695x_render (void)
{
	bool forceWrite = false;

	int key = irq_lock();
	memcpy(&_s_frameWorkCopy, &_s_frameUser, sizeof(FRAME_BUF_MAX695x_32BIT));
	if (_s_forceWriteFlag) {
		forceWrite = true;
		_s_forceWriteFlag = false;
	}

	irq_unlock(key);
	uint8_t u8tmp;
	bool lowFlag = false;
	bool highFlag = false;

	if (_s_frameWorkCopy.scanLimit > 8) {
		_s_frameWorkCopy.scanLimit = 8;
	}
	if (_s_frameWorkCopy.scanLimit > 4) {
		lowFlag = highFlag = true;
	} else if (_s_frameWorkCopy.scanLimit > 0) {
		lowFlag = true;
	}

	//
	// On/Off
	//
	if ((_s_frameWorkCopy.scanLimit != _s_frameFlushed.scanLimit) || forceWrite) {
		forceWrite = true;

		if (!lowFlag && !highFlag) {
			_max695x_turnOff(MAX695X_LOW_I2C_ADDRESS);
			_max695x_turnOff(MAX695X_HIGH_I2C_ADDRESS);
 		} else if (lowFlag && !highFlag) {
 			_max695x_turnOff(MAX695X_HIGH_I2C_ADDRESS);

			u8tmp = _s_frameWorkCopy.brightnessHalf ? _s_frameWorkCopy.brightnessHalf : 16;
			_max695x_turnOn(MAX695X_LOW_I2C_ADDRESS, u8tmp, _s_frameWorkCopy.scanLimit - 1);
		} else if (lowFlag && highFlag) {
			u8tmp = _s_frameWorkCopy.brightnessAll ? _s_frameWorkCopy.brightnessAll : 16;

			_max695x_turnOn(MAX695X_LOW_I2C_ADDRESS, u8tmp, 3);
			_max695x_turnOn(MAX695X_HIGH_I2C_ADDRESS, u8tmp, _s_frameWorkCopy.scanLimit - 5);
		}
		LOG_DBG("Set     ScanLimit = %d", _s_frameWorkCopy.scanLimit);
	}

	if (!_s_enable_max695x)
		return;

	//
	// brightness
	//
	if (_s_frameWorkCopy.brightnessAll != _s_frameFlushed.brightnessAll && lowFlag && highFlag) {
		_max695x_setIntensity(MAX695X_LOW_I2C_ADDRESS, _s_frameWorkCopy.brightnessAll);
		_max695x_setIntensity(MAX695X_HIGH_I2C_ADDRESS, _s_frameWorkCopy.brightnessAll);
		LOG_DBG("Set brigtnes All  = %d", _s_frameWorkCopy.brightnessAll);
	} else if (_s_frameWorkCopy.brightnessHalf != _s_frameFlushed.brightnessHalf && lowFlag && !highFlag) {
		_max695x_setIntensity(MAX695X_LOW_I2C_ADDRESS, _s_frameWorkCopy.brightnessHalf);
		LOG_DBG("Set brigtnes Half = %d", _s_frameWorkCopy.brightnessHalf);
	} else if (forceWrite) {
		if (lowFlag && highFlag) {
			_max695x_setIntensity(MAX695X_LOW_I2C_ADDRESS, _s_frameWorkCopy.brightnessAll);
			_max695x_setIntensity(MAX695X_HIGH_I2C_ADDRESS, _s_frameWorkCopy.brightnessAll);
			LOG_DBG("Set brigtnes All  = %d", _s_frameWorkCopy.brightnessAll);
		} else {
			_max695x_setIntensity(MAX695X_LOW_I2C_ADDRESS, _s_frameWorkCopy.brightnessHalf);
			LOG_DBG("Set brigtnes Half = %d", _s_frameWorkCopy.brightnessHalf);
		}
	}

	//
	// DPs
	//
	if (_s_frameWorkCopy.DPs != _s_frameFlushed.DPs) {
		if (lowFlag && ((_s_frameWorkCopy.DPs ^ _s_frameFlushed.DPs) & 0x0F)) {
			_max695x_setDPs(MAX695X_LOW_I2C_ADDRESS, _s_frameWorkCopy.DPs);
			LOG_DBG("Set       Low  DP = 0x%02X", _s_frameWorkCopy.DPs & 0x0F);
		}

		if (highFlag && ((_s_frameWorkCopy.DPs ^ _s_frameFlushed.DPs) & 0xF0)) {
			_max695x_setDPs(MAX695X_HIGH_I2C_ADDRESS, (_s_frameWorkCopy.DPs >> 4));
			LOG_DBG("Set       High DP = 0x%02X", _s_frameWorkCopy.DPs & 0xF0);
		}
	} else if (forceWrite) {
		if (lowFlag) {
			_max695x_setDPs(MAX695X_LOW_I2C_ADDRESS, _s_frameWorkCopy.DPs);
		}
		if (highFlag) {
			_max695x_setDPs(MAX695X_HIGH_I2C_ADDRESS, (_s_frameWorkCopy.DPs >> 4));
		}
		LOG_DBG("Force set      DP = 0x%02X", _s_frameWorkCopy.DPs);
	}

	//
	// data
	//
	if (lowFlag && (_s_frameWorkCopy.rawLow != _s_frameFlushed.rawLow || forceWrite)) {
		_max695x_setData(MAX695X_LOW_I2C_ADDRESS, _s_frameWorkCopy.rawLow);
		LOG_DBG("Set low  data raw = 0x%08X", _s_frameWorkCopy.rawLow);
	}
	if (highFlag && (_s_frameWorkCopy.rawHigh != _s_frameFlushed.rawHigh || forceWrite)) {
		_max695x_setData(MAX695X_HIGH_I2C_ADDRESS, _s_frameWorkCopy.rawHigh);
		LOG_DBG("Set high data raw = 0x%08X", _s_frameWorkCopy.rawHigh);
	}

	memcpy(&_s_frameFlushed, &_s_frameWorkCopy, sizeof(FRAME_BUF_MAX695x_32BIT));
}

#define MAX695X_RENDER_PERIOD_MS 300

static struct k_work max695x_work;

/**
 * _max695x_workHandler
 *
 * @param [in]   w;     work item pointer
 *
 * @note
 *  Work handler for max695x rendering
 */
static void _max695x_workHandler(struct k_work *w)
{
	_max695x_render();
}

/**
 * _max695x_timerCallback
 *
 * @param [in]   timer;     timer pointer
 *
 * @note
 *  Timer callback to submit render work
 */
static void _max695x_timerCallback(struct k_timer *timer)
{
	k_work_submit(&max695x_work);
}

K_TIMER_DEFINE(max695x_timer, _max695x_timerCallback, NULL);

/***************************************
 *
 * MAX695x 32-bit mode
 *
 ***************************************/

/**
 * dev_max695x_Init
 *
 * @param [in]   	   i2cPort;     I2C port
 *
 * @note
 *  init the max695
 */
void dev_max695x_Init(uint8_t i2cPort)
{
	max695Dev = i2cPort;
	memset(&_s_frameUser, 0, sizeof(FRAME_BUF_MAX695x_32BIT));
	memset(&_s_frameWorkCopy, 0, sizeof(FRAME_BUF_MAX695x_32BIT));
	memset(&_s_frameFlushed, 0, sizeof(FRAME_BUF_MAX695x_32BIT));
	_s_frameUser.brightnessAll = 16;
	_s_frameUser.brightnessHalf = 8;
	k_work_init(&max695x_work, _max695x_workHandler);
	k_timer_start(&max695x_timer, K_MSEC(MAX695X_RENDER_PERIOD_MS), K_MSEC(MAX695X_RENDER_PERIOD_MS));
}

/**
 * dev_max695x_32bit_reset
 *
 * @note
 *  reset the max695
 */
void dev_max695x_32bit_reset(void)
{

	int key = irq_lock();
	//
	// Reset 7-Seg LED to "0.0.0.0.0.0.0.0."
	//
	_s_frameUser.scanLimit = 8;
	_s_frameUser.DPs = 0xFF;
	_s_frameUser.rawLow  = 0x7E7E7E7E; // 00000000
	_s_frameUser.rawHigh = 0x7E7E7E7E; // 00000000

	irq_unlock(key);
	k_work_submit(&max695x_work);
}

/**
 * dev_max695x_32bit_DPonly
 *
 * @note
 *  Reset 7-Seg LED to "0.0.0.0.0.0.0.0."
 */
void dev_max695x_32bit_DPonly(void)
{

	int key = irq_lock();
	//
	// Reset 7-Seg LED to "0.0.0.0.0.0.0.0."
	//
	_s_frameUser.scanLimit = 8;
	_s_frameUser.DPs = 0xFF;
	_s_frameUser.rawLow  = 0x0u; // 00000000
	_s_frameUser.rawHigh = 0x0u; // 00000000

	irq_unlock(key);
	k_work_submit(&max695x_work);
}

/**
 * dev_max695x_32bit_turnOn
 *
 * @param [in]   	   brightness;  Postcode led brightness when turn on.
 *
 * @note
 *  turn on the max695
 */
void dev_max695x_32bit_turnOn(uint8_t brightness)
{

	int key = irq_lock();
	_s_frameUser.brightnessAll = brightness;
	_s_frameUser.brightnessHalf = brightness / 2;
	_s_frameUser.scanLimit = 8;
	_s_forceWriteFlag = true;

	irq_unlock(key);
}

/**
 * dev_max695x_32bit_turnOff
 *
 * @note
 *  turn off max695.
 */
void dev_max695x_32bit_turnOff(void)
{
	int key = irq_lock();
	_s_frameUser.scanLimit = 0;
	_s_forceWriteFlag = true;
	irq_unlock(key);
	k_work_submit(&max695x_work);
}

/*
 * Encoding rule:
 *   --a--
 *  |     |
 *  f     b
 *  |     |
 *   --g--
 *  |     |
 *  e     c
 *  |     |
 *   --d--   O <- dp
 *
 * c8bit(0abc, defg)
 */
static uint8_t _s_digMap[16] = {
	c8bit(0111, 1110),   // 0
	c8bit(0011, 0000),   // 1
	c8bit(0110, 1101),   // 2
	c8bit(0111, 1001),   // 3
	c8bit(0011, 0011),   // 4
	c8bit(0101, 1011),   // 5
	c8bit(0101, 1111),   // 6
	c8bit(0111, 0000),   // 7
	c8bit(0111, 1111),   // 8
	c8bit(0111, 1011),   // 9
	c8bit(0111, 0111),   // A
	c8bit(0001, 1111),   // B
	c8bit(0100, 1110),   // C
	c8bit(0011, 1101),   // D
	c8bit(0100, 1111),   // E
	c8bit(0100, 0111)    // F
};

/**
 * dev_max695x_32bit_postcode
 *
 * @param [in]   	   pc;     postcode
 * @param [in]   	   dp;     display
 * @param [in]      limit;     limit
 *
 * @note
 *  show the input postcode number
 */
void dev_max695x_32bit_postcode(uint32_t pc, uint8_t dp, uint8_t limit)
{
	uint64_t rawData = 0;
	for (uint8_t i = 0; i < 8; i++) {
		rawData |= (((uint64_t)_s_digMap[((pc >> (i * 4)) & 0x0F)]) << (i * 8));
	}

	int key = irq_lock();
	_s_frameUser.scanLimit = limit;
	_s_frameUser.rawLow = (rawData & 0xFFFFFFFF);
	_s_frameUser.rawHigh = ((rawData >> 32) & 0xFFFFFFFF);
	_s_frameUser.DPs = dp;

	irq_unlock(key);
	k_work_submit(&max695x_work);
}

/**
 * dev_max695x_32bit_scanLimit
 *
 * @param [in]      limit;     limit
 *
 * @note
 * 32-bit scan limit
 * limit 0~8
 *       0 - turn off all LEDs
 *       1 - only turn on the lowest LED.
 *       ... ...
 *       8 - turn on all LED
 */
void dev_max695x_32bit_scanLimit(uint8_t limit)
{
	_s_frameUser.scanLimit = limit;
	k_work_submit(&max695x_work);
}

/**
 * dev_max695x_32bit_DPs
 *
 * @param [in]      turnOnBits;     bits to turn on
 *
 * @note
 * For example turnOnBits:
 *    c8bit(1100, 0101) -> [X.X.X X X X.X X.]
 *    c8bit(1111, 1111) -> all DPs on
 *    c8bit(0000, 0000) -> all DPs off
 */
void dev_max695x_32bit_DPs(uint8_t turnOnBits)
{
	_s_frameUser.DPs = turnOnBits;

	k_work_submit(&max695x_work);
}

/**
 * dev_max695x_show
 *
 * @param [in]        rawH;     raw data high
 * @param [in]        rawL;     raw data low
 * @param [in]          dp;     display
 * @param [in]       limit;     limit
 * @param [in]  forceWrite;     true is force
 *
 * @note
 *  max695 to show the value
 */
void dev_max695x_show(uint32_t rawH, uint32_t rawL, uint8_t dp, uint8_t limit, bool forceWrite)
{
	int key = irq_lock();
	_s_frameUser.scanLimit = limit;
	_s_frameUser.rawLow = rawL;
	_s_frameUser.rawHigh = rawH;
	_s_frameUser.DPs = dp;
	if (forceWrite)
		_s_forceWriteFlag = true;
	irq_unlock(key);
}

/**
 * dev_max695x_print
 *
 * @param [in]   format;     format string
 * @param [in]      ...;     variable arguments
 *
 * @return true if successful, false otherwise
 *
 * @note
 *  max695 to show the chart
 */
bool dev_max695x_print(const char * format, ...)
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
    encode = 0xFF;
    isDot = false;

    char c = buf[idx];

    if (c >= 'A' && c <= 'F') {
      encode = _s_digMap[c - 'A' + 10];
    } else if (c >= 'a' && c <= 'f') {
      encode = _s_digMap[c - 'a' + 10];
    } else if (c >= '0' && c <= '9') {
      encode = _s_digMap[c - '0'];
    } else {
      switch (c) {
        case '.':  // dot
          if (8 == led) led --;
          dp |= (1ul << led);
          isDot = true;
          break;
        case '-':  // -
          encode = c8bit(0000, 0001); // (0abc, defg)
          break;
        case '_':  // underline
          encode = c8bit(0000, 1000); // (0abc, defg)
          break;
        case ' ':  // space
          encode = c8bit(0000, 0000); // (0abc, defg)
          break;
                   //  _
        case 'g':  // |
        case 'G':  // |_|
          encode = c8bit(0101, 1110); // (0abc, defg)
          break;
                   // |_
        case 'h':  // | |
          encode = c8bit(0001, 0111); // (0abc, defg)
          break;
                   // |_|
        case 'H':  // | |
          encode = c8bit(0011, 0111); // (0abc, defg)
          break;
        case 'i':  //
        case 'I':  // |
          encode = c8bit(0000, 0100); // (0abc, defg)
          break;
        case 'j':  //   |
        case 'J':  // |_|
          encode = c8bit(0011, 1100); // (0abc, defg)
          break;
        case 'k':  // |_
        case 'K':  // |
          // <<< ( ???? )  >>> //
          encode = c8bit(0000, 0111); // (0abc, defg)
          break;
        case 'l':  // |
        case 'L':  // |_|
          encode = c8bit(0001, 1110); // (0abc, defg)
          break;
                   //  _
        case 'm':  //  _
        case 'M':  // | |
          // <<< ( ???? )  >>> //
          encode = c8bit(0101, 0101); // (0abc, defg)
          break;
        case 'n':  //  _
        case 'N':  // | |
          encode = c8bit(0001, 0101); // (0abc, defg)
          break;
        case 'o':  //  _
        case 'O':  // |_|
          encode = c8bit(0001, 1101); // (0abc, defg)
          break;
                   //  _
        case 'p':  // |_|
        case 'P':  // |
          encode = c8bit(0110, 0111); // (0abc, defg)
          break;
                   //     _       _
        case 'q':  // q: |_|  9: |_|
        case 'Q':  //      |      _|
          encode = c8bit(0111, 0011); // (0abc, defg)
          break;
        case 'r':  //  _
        case 'R':  // |
          encode = c8bit(0000, 0101); // (0abc, defg)
          break;
        case 's':  //  _|
        case 'S':  // |
          encode = c8bit(0010, 0101); // (0abc, defg)
          break;
        case 't':  // |_
        case 'T':  // |_
          encode = c8bit(0000, 1111); // (0abc, defg)
          break;
        case 'u':  // | |
        case 'U':  // |_|
          encode = c8bit(0011, 1110); // (0abc, defg)
          break;
        case 'v':  //
        case 'V':  // |_|
          encode = c8bit(0001, 1100); // (0abc, defg)
          break;
        case 'w':  // |_|
        case 'W':  //  _
          // <<< ( ???? )  >>> //
          encode = c8bit(0010, 1011); // (0abc, defg)
          break;
        case 'x':  // | |
        case 'X':  // | |
          // <<< ( none )  >>> //
          encode = c8bit(0011, 0110); // (0abc, defg)
          break;
        case 'y':  // |_|
        case 'Y':  //  _|
          encode = c8bit(0011, 1011); // (0abc, defg)
          break;
        case 'z':  //  _
        case 'Z':  //  _
          encode = c8bit(0000, 1001); // (0abc, defg)
          break;
        default:
          break;
      }
    }

    if (!isDot) {
      led --;
      framebuf |= ((uint64_t) encode) << (8 * led);
    }
  }

  uint32_t rawH = (uint32_t)(framebuf >> 32);
  uint32_t rawL = (uint32_t)(framebuf & 0xFFFFFFFFul);
  dev_max695x_show(rawH, rawL, dp, 8, false);

  return true;
}


/**
 * dev_max695x_set_sts
 *
 * @param [in]   status;     enable or disable status
 *
 * @note
 *  set max695 global enable and disable status
 */
void dev_max695x_set_sts(bool status)
{
	/* this flag only inhibits rander */
	_s_enable_max695x = status;
}

/**
 * dev_max695x_get_sts
 *
 * @return true if enabled, false if disabled
 *
 * @note
 *  get max695 global enable and disable status
 */
bool dev_max695x_get_sts(void)
{
	return _s_enable_max695x;
}