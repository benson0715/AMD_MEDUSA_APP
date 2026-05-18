/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RTS5918 implementation of the EC_GPIO_xx wrapper API declared in
 * drivers/realtek/gpio_ec.h. Translates the packed (port, pin) tokens
 * back into Zephyr gpio_dt_spec calls against the realtek,rts5918-gpio
 * banks gpioa..gpioq.
 *
 * Bank lookup is a static dispatch on the high-byte of port_pin. If
 * a token decodes to a bank whose DT node is not enabled, the call
 * silently returns -ENODEV — this is the bring-up safety net so
 * undefined EC_GPIO_xx tokens don't accidentally toggle the wrong pad.
 *
 * TODO(realtek-schematic):
 *   1. gpio_init() currently leaves every pin in its reset state.
 *      Replace with a per-board init table once pin map exists.
 *   2. gpio_set_interrupt() / gpio_init_callback_pin() route to the
 *      stock Zephyr gpio_pin_interrupt_configure_dt + gpio_add_callback
 *      path. If Plum uses any wake-from-G3 paths that depend on
 *      RTS5918's wake controller, extend here.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "gpio_ec.h"

LOG_MODULE_REGISTER(realtek_gpio_ec, CONFIG_GPIO_LOG_LEVEL);

#define DT_DRV_COMPAT realtek_rts5918_gpio

#define BANK_DEV_OR_NULL(node) \
	COND_CODE_1(DT_NODE_HAS_STATUS(node, okay), (DEVICE_DT_GET(node)), (NULL))

static const struct device *const rtk_gpio_banks[] = {
	BANK_DEV_OR_NULL(DT_NODELABEL(gpioa)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpiob)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpioc)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpiod)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpioe)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpiof)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpiog)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpioh)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpioi)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpioj)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpiok)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpiol)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpiom)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpion)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpioo)),
	BANK_DEV_OR_NULL(DT_NODELABEL(gpiop)),
};

#define RTK_GPIO_BANK_COUNT ARRAY_SIZE(rtk_gpio_banks)

static const struct device *bank_dev(uint32_t port_pin)
{
	uint32_t bank = gpio_get_port(port_pin);

	if (bank >= RTK_GPIO_BANK_COUNT) {
		return NULL;
	}
	return rtk_gpio_banks[bank];
}

uint32_t get_absolute_gpio_num(uint32_t port_pin)
{
	/* Same arithmetic as NPCX4 wrapper: 8 pins/bank, decimal layout. */
	return gpio_get_port(port_pin) * 8U + gpio_get_pin(port_pin);
}

int gpio_init(void)
{
	/* TODO(realtek-schematic): per-bank pad init using the board's
	 * gpioAutoGen table. For now we simply confirm every advertised
	 * bank device is ready.
	 */
	for (size_t i = 0; i < RTK_GPIO_BANK_COUNT; i++) {
		if (rtk_gpio_banks[i] && !device_is_ready(rtk_gpio_banks[i])) {
			LOG_WRN("RTS5918 GPIO bank %u not ready", (unsigned int)i);
		}
	}
	return 0;
}

int gpio_configure_pin(uint32_t port_pin, gpio_flags_t flags)
{
	const struct device *dev = bank_dev(port_pin);

	if (!dev) {
		return -ENODEV;
	}
	return gpio_pin_configure(dev, gpio_get_pin(port_pin), flags);
}

int gpio_force_configure_pin(uint32_t port_pin, gpio_flags_t flags)
{
	return gpio_configure_pin(port_pin, flags);
}

int gpio_configure_array(struct gpio_ec_config *gpios, uint32_t len)
{
	int rc = 0;

	if (!gpios) {
		return -EINVAL;
	}
	for (uint32_t i = 0; i < len; i++) {
		int err = gpio_configure_pin(gpios[i].port_pin, gpios[i].cfg);

		if (err && !rc) {
			rc = err;
		}
	}
	return rc;
}

int gpio_set_interrupt(uint32_t port_pin, uint32_t flag)
{
	const struct device *dev = bank_dev(port_pin);

	if (!dev) {
		return -ENODEV;
	}
	return gpio_pin_interrupt_configure(dev, gpio_get_pin(port_pin), flag);
}

int gpio_write_pin(uint32_t port_pin, int value)
{
	const struct device *dev = bank_dev(port_pin);

	if (!dev) {
		return -ENODEV;
	}
	return gpio_pin_set(dev, gpio_get_pin(port_pin), value);
}

int gpio_read_pin(uint32_t port_pin)
{
	const struct device *dev = bank_dev(port_pin);

	if (!dev) {
		return -ENODEV;
	}
	return gpio_pin_get(dev, gpio_get_pin(port_pin));
}

int gpio_init_callback_pin(uint32_t port_pin,
			   struct gpio_callback *callback,
			   gpio_callback_handler_t handler)
{
	const struct device *dev = bank_dev(port_pin);

	if (!dev || !callback) {
		return -ENODEV;
	}
	gpio_init_callback(callback, handler, BIT(gpio_get_pin(port_pin)));
	return 0;
}

int gpio_add_callback_pin(uint32_t port_pin, struct gpio_callback *callback)
{
	const struct device *dev = bank_dev(port_pin);

	if (!dev) {
		return -ENODEV;
	}
	return gpio_add_callback(dev, callback);
}

int gpio_remove_callback_pin(uint32_t port_pin, struct gpio_callback *callback)
{
	const struct device *dev = bank_dev(port_pin);

	if (!dev) {
		return -ENODEV;
	}
	return gpio_remove_callback(dev, callback);
}

int gpio_get_pending_interrupt_pin(uint32_t port_pin)
{
	ARG_UNUSED(port_pin);
	/* RTS5918 GPIO driver clears its IRQ status inside the ISR;
	 * caller-facing pending-bit polling is not used by AMD app code.
	 */
	return 0;
}
