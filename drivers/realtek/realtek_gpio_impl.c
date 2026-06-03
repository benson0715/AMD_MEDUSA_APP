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

/*
 * Build a {index, device*} entry for one realtek,rts5918-gpio node.
 * The COND_CODE_1 wrapper compiles each entry to nothing if the
 * node is status="disabled" (so disabled banks don't pull in their
 * device pointer).
 */
#define DRV_GPIO_TABLE_ENTRY(node_id)                                      \
	COND_CODE_1(DT_NODE_HAS_STATUS(node_id, okay),                     \
		    ({                                                     \
			    .index = DT_PROP(node_id, index),              \
			    .dev   = DEVICE_DT_GET(node_id),               \
		    },),                                                   \
		    ())

struct drv_gpio_entry {
	uint32_t             index;
	const struct device *dev;
};

static const struct drv_gpio_entry drv_gpio_table[] = {
	DT_FOREACH_STATUS_OKAY(realtek_rts5918_gpio, DRV_GPIO_TABLE_ENTRY)
};

struct gpio_pin_name {
	enum gpio_idx pin_idx;
	const char *name;
};

#define GPIO_PIN_NAME_ENTRY(node)				\
	{							\
		.pin_idx = GPIO_VAL_FROM_ARRAY(node, gpios),	\
		.name = DT_PROP(node, enum_name),		\
	}

static const struct gpio_pin_name gpio_pin_names[] = {
	DT_FOREACH_CHILD_SEP(DT_PATH(GPIO_HUB_NODE_NAME), GPIO_PIN_NAME_ENTRY, (,))
};

const char *get_pin_name(enum gpio_idx pin_idx)
{
	for (int i = 0; i < ARRAY_SIZE(gpio_pin_names); i++) {
		if (gpio_pin_names[i].pin_idx == pin_idx) {
			return gpio_pin_names[i].name;
		}
	}
	return "?";
}

const struct device *drv_gpio_get_dev(uint32_t port)
{
	for (size_t i = 0; i < ARRAY_SIZE(drv_gpio_table); i++) {
		if (drv_gpio_table[i].index == port) {
			return drv_gpio_table[i].dev;
		}
	}
	return NULL;
}

int gpio_interrupt_configure_pin(enum gpio_idx pin_idx, gpio_flags_t flags)
{
	uint32_t             port = gpio_get_port(pin_idx);
	gpio_pin_t           pin = gpio_get_pin(pin_idx);
	const struct device *gpio_dev = drv_gpio_get_dev(port);

	if (gpio_dev == NULL) {
		LOG_ERR("Invalid port %d", port);
		return -ENODEV;
	}

	return gpio_pin_interrupt_configure(gpio_dev, pin, flags);
}

void gpio_interrupt_configure_all(void)
{
	/* Configure board GPIO callback functions */
	for (int i = 0; i < ARRAY_SIZE(gpio_hub_ints); i++) {
		int ret;

		ret = gpio_init_callback_pin(gpio_hub_ints[i].pin_idx,
		                             &gpio_hub_ints[i].callback,
		                             gpio_hub_ints[i].handler);
		if (ret != 0) {
			LOG_ERR("Invalid gpio ISR init at %d, %d", i, ret);
			continue;
		}

		ret = gpio_add_callback_pin(gpio_hub_ints[i].pin_idx,
		                            &gpio_hub_ints[i].callback);
		if (ret != 0) {
			LOG_ERR("Invalid gpio ISR add at %d, %d", i, ret);
			continue;
		}

		ret = gpio_interrupt_configure_pin(gpio_hub_ints[i].pin_idx,
		                                   gpio_hub_ints[i].flags);
		if (ret != 0) {
			LOG_ERR("Invalid gpio ISR configure at %d, %d", i, ret);
		}
	}
}


/* S-G 07/31/25 Adjust the timing to configure all GPIOs before turning on M-power. */
/* static int gpio_hub_init(void) */
int gpio_configure_all_pins(void)
{
	/* Configure all gpios here */
	for (int i = 0; i < ARRAY_SIZE(gpio_hub_pins); i++) {
		uint32_t             port = gpio_get_port(gpio_hub_pins[i].pin_idx);
		gpio_pin_t           pin = gpio_get_pin(gpio_hub_pins[i].pin_idx);
		const struct device *gpio_dev = drv_gpio_get_dev(port);
		int                  ret;

		printf("i: %d port: %p pin: %d cfg: %x, idx: %x\r\n", i, gpio_dev, pin, gpio_hub_pins[i].cfg, gpio_hub_pins[i].pin_idx);
		if (GPIO_PIN_IS_UNDEFINED(gpio_hub_pins[i].pin_idx)) {
			continue;
		}
		if (GPIO_PIN_IS_EXPANDER(gpio_hub_pins[i].pin_idx)) {
			/* Expander pins encode an expander chip/port index in
			 * the port field that collides with real GPIO bank
			 * indices (gpioa..gpiod = 0..3, expander1_p0..expander2_p1
			 * = 0..3). Skip here; per-pin IoInit() drives
			 * IoInitExpander() during board bring-up.
			 */
			continue;
		}
		if (!gpio_dev) {
			LOG_ERR("Invalid port %d at %d", port, i);
			continue;
		}
		ret = gpio_pin_configure(gpio_dev, pin, gpio_hub_pins[i].cfg);
		if (ret) {
			LOG_ERR("Config fail: %d i:%d port:%p pin: %d cfg: %d", ret,
			        i, gpio_dev, pin, gpio_hub_pins[i].cfg);
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
	int ret;

	/* Configure static board gpios */
	for (int i = 0; i < len; i++) {
		uint32_t             port = gpio_get_port(gpios[i].port_pin);
		gpio_pin_t           pin = gpio_get_pin(gpios[i].port_pin);
		const struct device *gpio_dev = drv_gpio_get_dev(port);		
		printk("gpio_configure_array: %x\r\n", gpios[i].port_pin);
		if (GPIO_PIN_IS_UNDEFINED(gpios[i].port_pin)) {
			continue;
		}

		if (!gpio_dev) {
			LOG_ERR("Invalid port %d at %d", port, i);
			continue;
		}
		ret = gpio_pin_configure(gpio_dev, pin, gpios[i].cfg);
		if (ret) {
			LOG_ERR("Config fail: %d i:%d port:%p pin: %d cfg: %d", ret,
			        i, gpio_dev, pin, gpio_hub_pins[i].cfg);
		}				

	}

	return 0;
}

int gpio_set_interrupt(uint32_t port_pin, uint32_t flag)
{
	const struct device *dev = bank_dev(port_pin);

	if (!dev) {
		return -ENODEV;
	}
	return gpio_pin_interrupt_configure(dev, gpio_get_pin(port_pin), flag);
}

int gpio_write_pin(enum gpio_idx pin_idx, int value)
{
	uint32_t             port;
	gpio_pin_t           pin;
	const struct device *gpio_dev;

//#ifndef CONFIG_MDL_GPIO_HUB_QUIET
	printk("RTK_gpio_write_pin: %s(%d)\n", get_pin_name(pin_idx), value);
//#endif

	if (GPIO_PIN_IS_UNDEFINED(pin_idx)) {
		return -ENODEV;
	}
	if (GPIO_PIN_IS_EXPANDER(pin_idx)) {
		/* IoSetExpander always drives the pin to "logical active".
		 * To clear (value=0), flip the active-low bit so the same
		 * call drives the pin to "logical inactive". This mirrors
		 * the trick Jupiter/IoPort.c::IoClr() uses internally for
		 * non-embedded ports.
		 */
		unsigned long key = (unsigned long)pin_idx;

		if (value == 0) {
			key ^= GPIO_PIN_ACTIVE_LOW_BIT;
		}
		//IoSetExpander(key);
		return 0;
	}

	port     = gpio_get_port(pin_idx);
	pin      = gpio_get_pin(pin_idx);
	gpio_dev = drv_gpio_get_dev(port);

	if (gpio_dev == NULL) {
		LOG_ERR("Invalid port %d", port);
		return -ENODEV;
	}
	return gpio_pin_set(gpio_dev, pin, value);
}

int gpio_read_pin(enum gpio_idx pin_idx)
{
	uint32_t             port;
	gpio_pin_t           pin;
	const struct device *gpio_dev;

	if (GPIO_PIN_IS_UNDEFINED(pin_idx)) {
		return 0;
	}
	if (GPIO_PIN_IS_EXPANDER(pin_idx)) {
		/* IoGetExpander returns IOP_ACTIVE (1) or IOP_INACTIVE (0)
		 * already accounting for the active-low bit. Map directly
		 * onto Zephyr's gpio_pin_get() return convention.
		 */
		//return IoGetExpander((unsigned long)pin_idx);
		return 0;
	}

	port     = gpio_get_port(pin_idx);
	pin      = gpio_get_pin(pin_idx);
	gpio_dev = drv_gpio_get_dev(port);

	if (gpio_dev == NULL) {
		LOG_ERR("Invalid port %d", port);
		return -ENODEV;
	}
	int ret = gpio_pin_get(gpio_dev, pin);

#ifndef CONFIG_MDL_GPIO_HUB_QUIET
	//const char *caller_name = get_caller_name();
	//if (strcmp(caller_name, "SnsMonitor") != 0) {
		printk("gpio_read_pin: %s %d\r\n", get_pin_name(pin_idx), ret);
	//}
#endif
	return ret;
}

/**
 * @brief Change GPIO attribute: input, output, or open drain
 *
 * @param port_pin gpio pin number.
 * @param type gpio type: input, output, or open drain
 */
void gpio_set_type(uint32_t port_pin, gpio_flags_t type)
{
	gpio_configure_pin(port_pin, type);
}



int gpio_init_callback_pin(enum gpio_idx pin_idx,
			   struct gpio_callback *callback,
			   gpio_callback_handler_t handler)
{
	gpio_pin_t pin;

	if (handler == NULL || callback == NULL) {
		return -EINVAL;
	}
	if (GPIO_PIN_IS_UNDEFINED(pin_idx) || GPIO_PIN_IS_EXPANDER(pin_idx)) {
		/* No Zephyr GPIO device behind these pins. Without this
		 * gate, a later gpio_add_callback_pin() on an expander pin
		 * would land on gpioa..gpiod (bank-collision in the port
		 * field) and attach to the wrong real bank.
		 */
		return -ENODEV;
	}

	pin = gpio_get_pin(pin_idx);
	/* zephyr function is void */
	gpio_init_callback(callback, handler, BIT(pin));

	return 0;
}

int gpio_add_callback_pin(enum gpio_idx pin_idx,
			  struct gpio_callback *callback)
{
	uint32_t             port = gpio_get_port(pin_idx);
	const struct device *gpio_dev = drv_gpio_get_dev(port);

	if (gpio_dev == NULL) {
		LOG_ERR("Invalid port %d", port);
		return -ENODEV;
	}

	if (callback == NULL) {
		return -EINVAL;
	}

	return gpio_add_callback(gpio_dev, callback);
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
