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

/* GPIO configuration generation */
/*
 * This structure is used to restore GPIO port/pin configurations for the middle
 * layer driver
 */
struct gpio_pin_cfg {
	enum gpio_idx pin_idx;
	uint32_t cfg;
};

/* define your configurations from board dts file */
#define POC_GPIO_OBJS_GET(node)							\
	{									\
		.pin_idx = GPIO_VAL_FROM_ARRAY(node, gpios),			\
		.cfg = DT_GPIO_FLAGS(node, gpios),				\
	}

/*
 * Create an array of gpio_pin_cfg containing the read-only configuration
 * for the pin configuration.
 */
static const struct gpio_pin_cfg gpio_hub_pins[] = {
	DT_FOREACH_CHILD_SEP(DT_PATH(GPIO_HUB_NODE_NAME), POC_GPIO_OBJS_GET, (,))
};

/* GPIO ISR generation */
/*
 * This function declarations is used for GPIO interrupt routines.
 */
#define GPIO_ISR_FUNC_NAME(entry) void entry(const struct device *,  \
	                                     struct gpio_callback *, \
					     gpio_port_pins_t)
#define GPIO_ISR_FUNC_DEFINE(node) \
	GPIO_ISR_FUNC_NAME(DT_STRING_TOKEN(node, handler));
DT_FOREACH_CHILD_SEP(DT_PATH(GPIO_INT_HUB_NODE_NAME), GPIO_ISR_FUNC_DEFINE, ())

/*
 * Structure contains the read-only configuration data for an
 * interrupt, such as the initial flags and the handler vector.
 * The RW callback data is reseved used later.
 */
struct gpio_int_config {
	const gpio_callback_handler_t handler; /* Handler to call */
	const gpio_flags_t flags; /* Flags */
	const enum gpio_idx pin_idx;
	struct gpio_callback callback;
};

/*
 * Create an instance of a gpio_int_config structure from a DTS node
 */
#define INT_CONFIG_ENTRY(node, irq_pin)                                \
	{                                                              \
		.flags = DT_PROP(node, flags),                         \
		.pin_idx = GPIO_VAL_FROM_ARRAY(irq_pin, gpios),        \
		.handler = DT_STRING_TOKEN(node, handler),             \
	},

#define INT_CONFIG_FROM_NODE(node) INT_CONFIG_ENTRY(node, DT_PROP(node, irq_pin))

/*
 * Create an array of gpio_int_config containing the read-only configuration
 * and read-write structure for the interrupt configuration.
 */
static struct gpio_int_config gpio_hub_ints[] = {
	DT_FOREACH_CHILD_STATUS_OKAY(DT_PATH(GPIO_INT_HUB_NODE_NAME),
				     INT_CONFIG_FROM_NODE)
};

/*
 * Phase 7.1: BIT(30) on the encoded pin_idx means GPIO_UNDEFINE_PIN —
 * the source code references this signal name, but Bison's pin map
 * has no real Realtek GPIO behind it. The backfill in
 * scripts/gen_poc_from_csv.py emits these as
 *     gpios = <&gpioa 0 GPIO_UNDEFINE_PIN>
 * so the encoded enum value carries the sentinel. All hub
 * accessors below silently return -ENODEV for these pins instead of
 * actually poking gpioa pin 0 (which is a real net on Bison).
 */
#define GPIO_PIN_IS_UNDEFINED(idx)  (((uint32_t)(idx)) & BIT(30))

/*
 * Phase 8.3c: BIT(31) on the encoded pin_idx means GPIO_EXPANDER —
 * the pin lives on a Lenovo-style I/O expander (KTS1622 / NCT5635Y)
 * managed by Jupiter/peripheral/Nct5635/nct5635.c, NOT on a real EC
 * GPIO bank. The proper Lenovo way is for source code to use
 * IO_SET_/IO_CLR_/IO_GET_ macros that expand to the IoSet() /
 * IoClr() / IoGet() dispatcher in Jupiter/IoPort.c, which checks
 * BIT(31) and routes to IoSetExpander/IoGetExpander.
 *
 * BUT a few legacy entries in include/Jupiter/Port.h still expand
 * directly to gpio_write_pin() / gpio_read_pin() (instead of
 * IoSet()/IoGet()) — most notably POutSysPwrOkEc and POutLEDMicMute,
 * which Phase 4.7 emitted as direct gpio_write_pin calls before we
 * understood the expander dispatch path. Without the safety net
 * below, those calls would land in drv_gpio_get_dev(port=1..3)
 * which after Phase 9.1 returns GPIO bank gpiob/c/d — completely
 * unrelated real EC pins. Catastrophic if it actually toggled.
 *
 * The fix: detect BIT(31) and forward to IoSetExpander() /
 * IoGetExpander() / IoInitExpander() before touching drv_gpio_*.
 *
 * Lenovo's expander API has no IoClrExpander() — to clear a pin,
 * IoClr() in IoPort.c flips the IO_NAME_DEF_ACTIVE_LOW_BIT_MASK bit
 * (BIT(5)) and calls IoSet() again, so IoSetExpander always sees
 * "logical active". We mirror that trick here for value=0.
 */
#define GPIO_PIN_IS_EXPANDER(idx)        (((uint32_t)(idx)) & BIT(31))
#define GPIO_PIN_ACTIVE_LOW_BIT          BIT(5)   /* IO_NAME_DEF_ACTIVE_LOW_BIT_MASK */

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
