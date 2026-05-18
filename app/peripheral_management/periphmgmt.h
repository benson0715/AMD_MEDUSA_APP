/**
 * Copyright (c) 2019 Intel Corporation
 * Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
 */

#ifndef __PERIPH_MGMT_H__
#define __PERIPH_MGMT_H__

#include "gpio_ec.h"

extern uint8_t g_isThrottleApuInDcOnly;
extern uint8_t g_isThrottleApuInAcOnly;

#define APU_RST_MINIMUM_PULSE   25  /* ms */

/**
 * @brief function pointer for ioexp handler
 *
 */
typedef void (*ioexp_handler_t)(uint8_t pinSt);

/**
 * @brief function pointer for pin debounce handler
 *
 */
typedef void (*pin_handler_t)(uint8_t pin_sts);

/**
 * @brief function pointer for gpio interrupt handler
 *
 */
typedef void (*periph_gpio_callback)(const struct device *dev,
		     	struct gpio_callback *gpio_cb, uint32_t pins);

/**
 * @brief inputbuffer info struct
 *
 */
struct inputBuffer_info {
	uint32_t		     port_pin;
	bool		         debouncing;
	uint8_t		         deb_cnt;
	pin_handler_t	     handler;
	bool		         prev_level;
	struct gpio_callback gpio_cb;
	char		         *name;
};

/* @brief Handles the power button and determine the next power state */
void periph_thread(void *p1, void *p2, void *p3);
/* @brief register the debouncer handler for the pins which need additional debounce */
int periph_register_debounce_handler(uint32_t port_pin, pin_handler_t handler);
/* @brief register the previous native gpio callback to gpio interrupt vector */
int periph_register_gpio_cb_vector(void);
/* @brief trigger the peripheral thread sem */
void periph_setup_sem(void);

#endif /* __PWRBTN_MGMT_H__ */
