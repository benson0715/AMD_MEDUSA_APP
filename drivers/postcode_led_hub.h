/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __LED_HUB_H__
#define __LED_HUB_H__

#include <stdint.h>
#include <stdbool.h>

enum _s_ledDevice {
    MAX695 = 0,
    MPC = 1
};

/***************************************
 *
 * LED 32-bit mode
 *
 ***************************************/

/**
 * @brief Init poetcode led device.
 *
 * @param device Poetcode led device.
 * @param i2cPort I2C port connected to poetcode led device.
 */
void postcode_led_hub_Init(uint8_t device, uint8_t i2cPort);

/**
 * @brief get led device.
 */
uint8_t postcode_get_led_device(void);

/**
 * @brief Reset poetcode led device.
 */
void postcode_led_hub_32bit_reset(void);

/**
 * @brief Reset 7-Seg LED to "0.0.0.0.0.0.0.0."
 */
void postcode_led_hub_32bit_DPonly(void);

/**
 * @brief Turn on postcode led.
 * 
 * @param brightness Postcode led brightness when turn on.
 */
void postcode_led_hub_32bit_turnOn(uint8_t brightness);

/**
 * @brief Turn off postcode led.
 */
void postcode_led_hub_32bit_turnOff(void);

/**
 * @brief Show postcode.
 * 
 * @param pc 4-byte postcode.
 * @param dp Digital point.
 * @param limit Scan limit.
 */
void postcode_led_hub_32bit_postcode(uint32_t pc, uint8_t dp, uint8_t limit);

/**
 * @brief Modify scan limit.
 * 
 * limit 0~8
 *       0 - turn off all LEDs
 *       1 - only turn on the lowest LED.
 *       ... ...
 *       8 - turn on all LED.
 * 
 * @param limit Scan limit.
 */
void postcode_led_hub_32bit_scanLimit(uint8_t limit);

/**
 * @brief Modify digital point.
 * 
 *    c8bit(1100, 0101) -> [X.X.X X X X.X X.]
 *    c8bit(1111, 1111) -> all DPs on
 *    c8bit(0000, 0000) -> all DPs off
 * 
 * @param limit Scan limit.
 */
void postcode_led_hub_32bit_DPs(uint8_t turnOnBits);

/**
 * @brief Show content by rawdata.
 *
 * @param rawH High 4-byte.
 * @param rawL Low 4-byte.
 * @param dp Digital point.
 * @param limit Scan limit.
 * @param forceWrite Force wtite when true.
 */
void postcode_led_hub_show(uint32_t rawH, uint32_t rawL, uint8_t dp, uint8_t limit, bool forceWrite);

/**
 * @brief Format print.
 *
 * @param format Variable param. For example, postcode_led_hub_print("%x",value);
 * @retval True if success.
 */
bool postcode_led_hub_print(const char * format, ...);

/**
 * @brief Set postcode led status.
 *
 * @param status True represent enable, false disable.
 * 
 */
void postcode_led_hub_set_sts(bool status);

/**
 * @brief Get postcode led status.
 *
 * @retval True represent enable, false disable.
 */
bool postcode_led_hub_get_sts(void);

#endif // end of __LED_HUB_H__

