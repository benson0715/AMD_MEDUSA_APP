/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#if CONFIG_PINCTRL
#include <zephyr/drivers/pinctrl.h>
#else
#include <zephyr/drivers/pinmux.h>
#endif
#include "nuvoton_gpio_ctrl.h"

/**
 * @brief Set gpio pad type.
 *
 * @param addr gpio register address.
 * @param pad  gpio pad type.
 */
void md_gpio_set_PAD(uint32_t * addr, MD_GPIO_EM_PAD pad)
{
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).PAD = pad;
}

/**
 * @brief Set gpio power rail.
 *
 * @param addr gpio register address.
 * @param pad  gpio power rail.
 */
void md_gpio_set_POWER(uint32_t * addr, MD_GPIO_EM_POWER pwr)
{
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).POWER = pwr;
}

/**
 * @brief Get gpio power rail.
 *
 * @param addr gpio register address.
 * 
 * @return gpio power.
 */
int md_gpio_get_POWER(uint32_t * addr)
{
	return (*( (volatile MD_GPIO_PIN_CONTROL *) addr)).POWER;
}

/**
 * @brief Set gpio interrupt type.
 *
 * @param addr gpio register address.
 * @param intType interrupt type.
 */
void md_gpio_set_INTERRUPT(uint32_t * addr, MD_GPIO_EM_INTERRUPT intType)
{
	switch (intType) {
		case MD_GPIO_EM_INTERRUPT__LowLevel:
		case MD_GPIO_EM_INTERRUPT__HighLevel:
		case MD_GPIO_EM_INTERRUPT__Disabled:
			(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).EDGE_ENABLE = 0;
			break;
		case MD_GPIO_EM_INTERRUPT__RisingEdge:
		case MD_GPIO_EM_INTERRUPT__FallingEdge:
		case MD_GPIO_EM_INTERRUPT__EitherEdge:
			(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).EDGE_ENABLE = 1;
			break;
		default:
			assert(0);
	}
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).INTERRUPT = intType;
}

/**
 * @brief Set gpio buf type.
 *
 * @param addr gpio register address.
 * @param bufType gpio buf type.
 */
void md_gpio_set_BUF_TYPE(uint32_t * addr, MD_GPIO_EM_BUF_TYPE bufType)
{
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).BUF_TYPE = bufType;
}

/**
 * @brief Set gpio direction.
 *
 * @param addr gpio register address.
 * @param dir  gpio direcioon.
 */
void md_gpio_set_DIRECTION(uint32_t * addr, MD_GPIO_EM_DIRECTION dir)
{
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).DIRECTION = dir;
}

/**
 * @brief Set gpio output mechanism.
 *
 * @param addr gpio register address.
 * @param val  gpio output mechanism.
 */
void md_gpio_set_OUTPUT_SELECT(uint32_t * addr, MD_GPIO_EM_OUTPUT_SELECT val)
{
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).OUTPUT_SELECT = val;
}

/**
 * @brief Set gpio polarity.
 *
 * @param addr gpio register address.
 * @param val  gpio polarity.
 */
void md_gpio_set_POLARITY(uint32_t * addr, bool isInverted)
{
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).POLARITY = isInverted ? 1 : 0;
}

/**
 * @brief Set gpio mux function.
 *
 * @param addr gpio register address.
 * @param mux  mux function.
 */
void md_gpio_set_MUX(uint32_t * addr, MD_GPIO_EM_MUX mux)
{
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).MUX = mux;
}

/**
 * @brief Set gpio output.
 *
 * @param addr gpio register address.
 * @param val  output value.
 */
void md_gpio_set_OUTPUT(uint32_t * addr, uint8_t val)
{
	assert((*( (volatile MD_GPIO_PIN_CONTROL *) addr)).OUTPUT_SELECT == MD_GPIO_EM_OUTPUT_SELECT__PinCtrl16);
	(*( (volatile MD_GPIO_PIN_CONTROL *) addr)).OUTPUT = val ? 1 : 0;
}

/**
 * @brief Get gpio input.
 *
 * @param addr gpio register address.
 * 
 * @return input value.
 */
uint8_t md_gpio_get_INPUT(uint32_t * addr)
{
	return (*( (volatile MD_GPIO_PIN_CONTROL *) addr)).INPUT ? 1 : 0;
}

/**
 * @brief Get output select.
 *
 * @param addr gpio register address.
 * 
 * @return output select.
 */
uint8_t md_gpio_get_OUTPUT_SELECT(uint32_t * addr)
{
	return (*( (volatile MD_GPIO_PIN_CONTROL *) addr)).OUTPUT_SELECT ? 1 : 0;
}

/**
 * @brief Get output set value.
 *
 * @param addr gpio register address.
 * 
 * @return output set value.
 */
uint8_t md_gpio_get_WAS_SET(uint32_t * addr)
{
	assert((*( (volatile MD_GPIO_PIN_CONTROL *) addr)).OUTPUT_SELECT == MD_GPIO_EM_OUTPUT_SELECT__PinCtrl16);
	return (*( (volatile MD_GPIO_PIN_CONTROL *) addr)).OUTPUT ? 1 : 0;
}