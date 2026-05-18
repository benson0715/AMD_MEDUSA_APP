/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "ioexp_hub.h"

/**
 * @brief Initialize IO expander pin to power-on reset state
 * @param pin The IO expander pin number
 */
void ioexp_por_init(uint32_t pin)
{
	if (!GPIO_isIoExpPin(pin)) {
		__ASSERT_PRINT("GPIO is not ioexp pin: %d", pin);
	}

	IOEXP_SETBIT(IOEXP_I2cSlvAddr(pin), IOEXP_I2cPort(pin), IOEXP_IdxOfGroup(pin), 
			IOEXP_IsPushPullPin(pin) ? 1 : 0, 
			IOEXP_IsOpenDrainPin(pin) ? 1 : 0, 
			IOEXP_PORSeting(pin));
}

/**
 * @brief Reset IO expander pin to default state
 * @param pin The IO expander pin number
 */
void ioexp_reset(uint32_t pin)
{
	if (!GPIO_isIoExpPin(pin)) {
		__ASSERT_PRINT("GPIO is not ioexp pin: %d", pin);
	}

	IOEXP_SETBIT(IOEXP_I2cSlvAddr(pin), IOEXP_I2cPort(pin), IOEXP_IdxOfGroup(pin), 
			IOEXP_IsPushPullPin(pin) ? 1 : 0, 
			IOEXP_IsOpenDrainPin(pin) ? 1 : 0, 
			IOEXP_PORSeting(pin));
}

/**
 * @brief Get the current value of an IO expander pin
 * @param pin The IO expander pin number
 * @return 1 if pin is high, 0 if pin is low
 */
uint32_t ioexp_get(uint32_t pin)
{
	if (!GPIO_isIoExpPin(pin)) {
		__ASSERT_PRINT("GPIO is not ioexp pin: %d", pin);
	}

	return ((0x01ul << IOEXP_IdxOfGroup(pin)) & (IOEXP_SMARTREAD(IOEXP_I2cSlvAddr(pin), IOEXP_I2cPort(pin), 0x01ul << IOEXP_IdxOfGroup(pin)))) ? 1 : 0;
}

/**
 * @brief Set the output value of an IO expander pin
 * @param pin The IO expander pin number
 * @param val The value to set (0 or 1)
 */
void ioexp_set(uint32_t pin, uint32_t val)
{
	if (!GPIO_isIoExpPin(pin)) {
		__ASSERT_PRINT("GPIO is not ioexp pin: %d", pin);
	}

	IOEXP_SETOUTPUT(IOEXP_I2cSlvAddr(pin), IOEXP_I2cPort(pin), 0x01ul << IOEXP_IdxOfGroup(pin), val ? (0x01ul << IOEXP_IdxOfGroup(pin)) : 0);
}

/**
 * @brief Toggle the output value of an IO expander pin
 * @param pin The IO expander pin number
 */
void ioexp_flip(uint32_t pin)
{
	if (!GPIO_isIoExpPin(pin)) {
		__ASSERT_PRINT("GPIO is not ioexp pin: %d", pin);
	}

	if ((0x01ul << IOEXP_IdxOfGroup(pin)) & (IOEXP_SMARTREAD(IOEXP_I2cSlvAddr(pin), IOEXP_I2cPort(pin), 0x01ul << IOEXP_IdxOfGroup(pin)))) {
		IOEXP_SETOUTPUT(IOEXP_I2cSlvAddr(pin), IOEXP_I2cPort(pin), 0x01ul << IOEXP_IdxOfGroup(pin), 0);
	} else {
		IOEXP_SETOUTPUT(IOEXP_I2cSlvAddr(pin), IOEXP_I2cPort(pin), 0x01ul << IOEXP_IdxOfGroup(pin), 0x01ul << IOEXP_IdxOfGroup(pin));
	}
}

/**
 * @brief Set the GPIO type of an IO expander pin
 * @param pin The IO expander pin number
 * @param type The GPIO type (input, output, or open-drain)
 */
void ioexp_set_type(uint32_t pin, IOEXP_GPIO_TYPE type)
{
	if (!GPIO_isIoExpPin(pin)) {
		__ASSERT_PRINT("GPIO is not ioexp pin: %d", pin);
	}

	bool isOutput, isOd;

	switch (type) {
		case IOEXP_GPIO_OUTPUT:
			isOutput = true;
			isOd = false;
			break;
		case IOEXP_GPIO_OPEN_DRAIN:
			isOutput = false;
			isOd = true;
			break;
		case IOEXP_GPIO_INPUT:
		default:
			isOutput = isOd = false;
			break;
	}
	IOEXP_SETBIT(IOEXP_I2cSlvAddr(pin), IOEXP_I2cPort(pin), IOEXP_IdxOfGroup(pin), isOutput, isOd, IOEXP_PORSeting(pin));
}