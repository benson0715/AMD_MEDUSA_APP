/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_max695x.h"
#include "dev_mpc.h"
#include "postcode_led_hub.h"
#include <stdint.h>
#include <stdbool.h>

static uint8_t _s_ledDevice;

/**
 * @brief Get the current LED device type
 * @return Current LED device identifier
 */
uint8_t postcode_get_led_device(void)
{
	return _s_ledDevice;
}

/**
 * @brief Initialize the postcode LED hub with specified device and I2C port
 * @param device LED device type (MAX695 or MPC)
 * @param i2cPort I2C port number for communication
 */
void postcode_led_hub_Init(uint8_t device, uint8_t i2cPort)
{
	_s_ledDevice = device;
	if (_s_ledDevice == MAX695) {
		dev_max695x_Init(i2cPort);
	}
	else if (_s_ledDevice == MPC) {
		dev_mpc_init(i2cPort);
	}
	else {
	}
}

/**
 * @brief Reset 32-bit LED display
 */
void postcode_led_hub_32bit_reset(void)
{
	if (_s_ledDevice == MAX695)
	{
		dev_max695x_32bit_reset();
	}
	else if(_s_ledDevice == MPC) {
		dev_mpc_32bit_reset();
	} else {
	}
}

/**
 * @brief Set 32-bit LED display to decimal point only mode
 */
void postcode_led_hub_32bit_DPonly(void)
{
	if (_s_ledDevice == MAX695) {
		dev_max695x_32bit_DPonly();
	}
	else {
	}

}

/**
 * @brief Turn on 32-bit LED display with specified brightness
 * @param brightness Brightness level
 */
void postcode_led_hub_32bit_turnOn(uint8_t brightness)
{
	if (_s_ledDevice == MAX695) {
		dev_max695x_32bit_turnOn(brightness);
	}
	else if (_s_ledDevice == MPC) {
		dev_mpc_32bit_turnOn(brightness);
	}
	else {
	}

}

/**
 * @brief Turn off 32-bit LED display
 */
void postcode_led_hub_32bit_turnOff(void)
{
	if (_s_ledDevice == MAX695) {
		dev_max695x_32bit_turnOff();
	}
	else if (_s_ledDevice == MPC) {
		dev_mpc_32bit_turnOff();
	}
	else {
	}
}

/**
 * @brief Display postcode on 32-bit LED display
 * @param pc Postcode value to display
 * @param dp Decimal point configuration
 * @param limit Scan limit value
 */
void postcode_led_hub_32bit_postcode(uint32_t pc, uint8_t dp, uint8_t limit)
{
	if (_s_ledDevice == MAX695) {
		dev_max695x_32bit_postcode(pc, dp, limit);
	}
	else if (_s_ledDevice == MPC) {
		dev_mpc_32bit_postcode(pc, dp, limit);
	}
	else {
	}
}

/* 32-bit scan limit
 * limit 0~8
 *       0 - turn off all LEDs
 *       1 - only turn on the lowest LED.
 *       ... ...
 *       8 - turn on all LED.
 */
void postcode_led_hub_32bit_scanLimit(uint8_t limit)
{
	if (_s_ledDevice == MAX695) {
		dev_max695x_32bit_scanLimit(limit);
	}
	else {
	}
}

/* For example turnOnBits:
 *    c8bit(1100, 0101) -> [X.X.X X X X.X X.]
 *    c8bit(1111, 1111) -> all DPs on
 *    c8bit(0000, 0000) -> all DPs off
 */
void postcode_led_hub_32bit_DPs(uint8_t turnOnBits)
{
	if (_s_ledDevice == MAX695) {
		dev_max695x_32bit_DPs(turnOnBits);
	}
	else {
	}
}

/**
 * @brief Show raw data on LED display
 * @param rawH High 32-bit raw data
 * @param rawL Low 32-bit raw data
 * @param dp Decimal point configuration
 * @param limit Scan limit value
 * @param forceWrite Force write even if data unchanged
 */
void postcode_led_hub_show(uint32_t rawH, uint32_t rawL, uint8_t dp, uint8_t limit, bool forceWrite)
{
	if (_s_ledDevice == MAX695) {
		dev_max695x_show(rawH, rawL, dp, limit, forceWrite);
	}
	else {
	}
}

/**
 * @brief Print formatted string to LED display
 * @param format Format string
 * @return true if print successful, false otherwise
 */
bool postcode_led_hub_print(const char * format, ...)
{
  bool ret = false;
	if (_s_ledDevice == MAX695) {
		ret = dev_max695x_print(format);
	}
	else if (_s_ledDevice == MPC) {
		ret = dev_mpc_print(format);
	}
	else {

	}
  return ret;
}


/* set LED global enable and disbale status */
void postcode_led_hub_set_sts(bool status)
{
	/* this flag only inhibits rander */
	if (_s_ledDevice == MAX695) {
		dev_max695x_set_sts(status);
	}
	else if (_s_ledDevice == MPC) {
		dev_mpc_set_sts(status);
	}
	else {
	}
}

/* get LED global enable and disbale status */
bool postcode_led_hub_get_sts(void)
{
  bool ret = false;
	if (_s_ledDevice == MAX695) {
		ret = dev_max695x_get_sts();
	}
	else if (_s_ledDevice == MPC) {
		ret = dev_mpc_get_sts();
	}
	else {
	}
  return ret;
}