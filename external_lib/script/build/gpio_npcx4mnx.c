/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <errno.h>
#if CONFIG_PINCTRL
#include <zephyr/drivers/pinctrl.h>
#else
#include <zephyr/drivers/pinmux.h>
#endif
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "soc_gpio.h"
#include "soc_miwu.h"
#include "soc_pins.h"
#include "reg/reg_def.h"
LOG_MODULE_REGISTER(gpio_ec, CONFIG_GPIO_EC_LOG_LEVEL);

#define DT_DRV_COMPAT   nuvoton_npcx_gpio

#define EC_DUMMY_GPIO_PORT 0xff
#define MAX_PINS_PER_PORT	32
#define OCTAL_BASE		8

unsigned char mpm_pinout_act = 1;

static uint32_t * _s_gpio_uasWasSetTab = NULL;
static uint32_t _s_gpio_WasSetTab_Size = 0;

static const struct device *const miwu_devs[] = {
	DEVICE_DT_GET(DT_NODELABEL(miwu0)),
	DEVICE_DT_GET(DT_NODELABEL(miwu1)),
	DEVICE_DT_GET(DT_NODELABEL(miwu2)),
};

/* Driver config */
struct gpio_npcx_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;
	/* GPIO controller base address */
	uintptr_t base;
	/* IO port */
	int port;
	/* Mapping table between gpio bits and wui */
	struct npcx_wui wui_maps[NPCX_GPIO_PORT_PIN_NUM];
	/* Mapping table between gpio bits and lvol */
	struct npcx_lvol lvol_maps[NPCX_GPIO_PORT_PIN_NUM];
};

/* Driver config */
struct intc_miwu_config {
	/* miwu controller base address */
	uintptr_t base;
	/* index of miwu controller */
	uint8_t index;
};

static const struct device *ports[] = {
	DEVICE_DT_GET(DT_NODELABEL(gpio0)),
	DEVICE_DT_GET(DT_NODELABEL(gpio1)),
	DEVICE_DT_GET(DT_NODELABEL(gpio2)),
	DEVICE_DT_GET(DT_NODELABEL(gpio3)),
	DEVICE_DT_GET(DT_NODELABEL(gpio4)),
	DEVICE_DT_GET(DT_NODELABEL(gpio5)),
	DEVICE_DT_GET(DT_NODELABEL(gpio6)),
	DEVICE_DT_GET(DT_NODELABEL(gpio7)),
	DEVICE_DT_GET(DT_NODELABEL(gpio8)),
	DEVICE_DT_GET(DT_NODELABEL(gpio9)),
	DEVICE_DT_GET(DT_NODELABEL(gpioa)),
	DEVICE_DT_GET(DT_NODELABEL(gpiob)),
	DEVICE_DT_GET(DT_NODELABEL(gpioc)),
	DEVICE_DT_GET(DT_NODELABEL(gpiod)),
	DEVICE_DT_GET(DT_NODELABEL(gpioe)),
	DEVICE_DT_GET(DT_NODELABEL(gpiof)),
	/* Handle 1 or more IO expanders */
#if CONFIG_GPIO_PCA95XX
#if DT_NODE_HAS_STATUS(DT_DRV_INST(0), okay)
	DEVICE_DT_INST_GET(0),
#endif
#if DT_NODE_HAS_STATUS(DT_DRV_INST(1), okay)
	DEVICE_DT_INST_GET(1),
#endif
#endif
};

struct gpio_port_pin {
	const struct device *gpio_dev;
	gpio_pin_t pin;
};

/**
 * @brief Get absolute GPIO number from port and pin
 *
 * @param port_pin Combined port and pin value
 * @return Absolute GPIO number
 */
uint32_t get_absolute_gpio_num(uint32_t port_pin)
{
	return (MAX_PINS_PER_PORT * gpio_get_port(port_pin) + gpio_get_pin(port_pin));
}

/**
 * @brief Validate GPIO device and extract port/pin information
 *
 * @param port_pin Combined port and pin value
 * @param pp Pointer to gpio_port_pin structure to fill
 * @return 0 on success, negative error code on failure
 */
static int validate_device(uint32_t port_pin, struct gpio_port_pin *pp)
{
	uint32_t port_idx;
	gpio_pin_t pin;

	port_idx = gpio_get_port(port_pin);
	pin = gpio_get_pin(port_pin);

	/* Fail gracefully when GPIO table is misconfigured which easily
	 * results in a CPU fault
	 */
	if (port_idx >= ARRAY_SIZE(ports)) {
		return -EINVAL;
	}

	if (!device_is_ready(ports[port_idx])) {
		LOG_ERR("%s gpio dev %d not ready", __func__, port_idx);
		return -ENODEV;
	}

	LOG_DBG("%s: port idx: %d port ptr: %p pin: %d",
		__func__, port_idx, ports[port_idx], pin);

	pp->gpio_dev = ports[port_idx];
	pp->pin = pin;

	return 0;
}

/**
 * @brief Check if GPIO is a dummy GPIO
 *
 * @param port_pin Combined port and pin value
 * @return true if dummy GPIO, false otherwise
 */
static bool is_dummy_gpio(uint32_t port_pin)
{
	uint32_t port_idx;
	port_idx = gpio_get_port(port_pin);
	return (port_idx == EC_DUMMY_GPIO_PORT);
}

/**
 * @brief Read dummy GPIO pin value
 *
 * @param port_pin Combined port and pin value
 * @return Pin value from the port_pin encoding
 */
static bool gpio_read_dummy_pin(uint32_t port_pin)
{
	return gpio_get_pin(port_pin);
}

/**
 * @brief Initialize GPIO subsystem
 *
 * @return 0 on success
 */
int gpio_init(void)
{
	gpio_reg_was_table(_s_Gpio_IdxOfUseWasSet);
	for (int i = 0; i < ARRAY_SIZE(ports); i++) {
		if (!device_is_ready(ports[i])) {
			LOG_ERR("GPIO port %c not ready", (i+0x31));
		}

		LOG_DBG("[Port %c] %p", (i+0x31), ports[i]);
	}
	return 0;
}

/**
 * @brief Configure a GPIO pin with specified flags
 *
 * @param port_pin Combined port and pin value
 * @param flags GPIO configuration flags
 * @return 0 on success, negative error code on failure
 */
int gpio_configure_pin(uint32_t port_pin, gpio_flags_t flags)
{
	int ret;
	struct gpio_port_pin pp;

	ret = validate_device(port_pin, &pp);
	if (ret) {
		LOG_ERR("Invalid pin %d", pp.pin);
		return ret;
	}

	ret = gpio_pin_configure(pp.gpio_dev, pp.pin, (~GPIO_INT_MASK)&flags);

	if (ret) {
		LOG_ERR("Failed to configure pin %d", pp.pin);
		return ret;
	}

	return 0;
}

/**
 * @brief Check if GPIO was set by MPM
 *
 * @param port_pin Combined port and pin value
 * @return MPM pinout activity status
 */
int gpio_was_mpm_set(uint32_t port_pin)
{

	return mpm_pinout_act;

}

/**
 * @brief Flip GPIO pin value
 *
 * @param port_pin Combined port and pin value
 */
void gpio_flp(uint32_t port_pin)
{
	;
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

/**
 * @brief Set GPIO interrupt enable/disable
 *
 * @param port_pin Combined port and pin value
 * @param toEnable true to enable, false to disable
 */
void gpio_set_griq(uint32_t port_pin, bool toEnable)
{
	if (toEnable) {
		gpio_interrupt_configure_pin(port_pin, GPIO_INT_ENABLE);
	} else {
		gpio_interrupt_configure_pin(port_pin, GPIO_INT_DISABLE);
	}
}

/**
 * @brief Enable MIWU IRQ for specified WUI
 *
 * @param wui Pointer to NPCX WUI structure
 */
void gpio_miwu_irq_enable(const struct npcx_wui *wui)
{
	const struct intc_miwu_config *config = miwu_devs[wui->table]->config;
	const uint32_t base = config->base;

	NPCX_WKEN(base, wui->group) |= BIT(wui->bit);
}

/**
 * @brief Disable MIWU IRQ for specified WUI
 *
 * @param wui Pointer to NPCX WUI structure
 */
void gpio_miwu_irq_disable(const struct npcx_wui *wui)
{
	const struct intc_miwu_config *config = miwu_devs[wui->table]->config;
	const uint32_t base = config->base;

	NPCX_WKEN(base, wui->group) &= ~BIT(wui->bit);
}

/**
 * @brief Get MIWU status for specified WUI
 *
 * @param wui Pointer to NPCX WUI structure
 * @return Status bit value
 */
int npcx_miwu_status(const struct npcx_wui *wui)
{
	const struct intc_miwu_config *config = miwu_devs[wui->table]->config;
	const uint32_t base = config->base;
	return IS_BIT_SET(NPCX_WKINEN(base, wui->group), wui->bit);
}

/**
 * @brief Get pad status for a GPIO pin
 *
 * @param dev GPIO device
 * @param pin Pin number
 * @return Pad status or negative error code
 */
int get_pads_Staus(const struct device *dev, int pin)
{
	const struct gpio_npcx_config *const config = dev->config;
	const struct npcx_wui *io_wui = &config->wui_maps[pin];
	if (io_wui->table == NPCX_MIWU_TABLE_NONE) {
		LOG_DBG("Cannot read GPIO(%x, %d) pad", config->port, pin);
		return -ENXIO;
	}
	return npcx_miwu_status(io_wui);
}

/**
 * @brief Disable pad IRQ for a GPIO pin
 *
 * @param dev GPIO device
 * @param pin Pin number
 */
void set_pads_irq_disable(const struct device *dev, int pin)
{
	const struct gpio_npcx_config *const config = dev->config;
	const struct npcx_wui *io_wui = &config->wui_maps[pin];
	if (io_wui->table == NPCX_MIWU_TABLE_NONE) {
		LOG_DBG("Cannot disable GPIO(%x, %d) pad", config->port, pin);
		return;
	}
	gpio_miwu_irq_disable(io_wui);
}

/**
 * @brief Enable pad IRQ for a GPIO pin
 *
 * @param dev GPIO device
 * @param pin Pin number
 */
void set_pads_irq_enable(const struct device *dev, int pin)
{
	const struct gpio_npcx_config *const config = dev->config;
	const struct npcx_wui *io_wui = &config->wui_maps[pin];
	if (io_wui->table == NPCX_MIWU_TABLE_NONE) {
		LOG_DBG("Cannot enable GPIO(%x, %d) pad", config->port, pin);
		return;
	}
	gpio_miwu_irq_enable(io_wui);
}

/**
 * @brief Get GPIO power state
 *
 * @param port_pin Combined port and pin value
 * @return Power state enumeration value
 */
MD_GPIO_EM_POWER gpio_get_power(uint32_t port_pin)
{
	bool ret;
	struct gpio_port_pin pp;
	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}
	ret = get_pads_Staus(pp.gpio_dev, pp.pin);
	if (ret) {
		return GPIO_CTRL_PWRG_VTR_IO;
	} else {
		return GPIO_CTRL_PWRG_OFF;
	}
}

/**
 * @brief Set GPIO interrupt configuration
 *
 * @param port_pin Combined port and pin value
 * @param flag Interrupt flag (GPIO_CTRL_IRQ_ON or GPIO_CTRL_IRQ_OFF)
 * @return 0 on success, negative error code on failure
 */
int gpio_set_interrupt(uint32_t port_pin, uint32_t flag)
{
	int ret;
	struct gpio_port_pin pp;
	if (is_dummy_gpio(port_pin)) {
		return 0;
	}
	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}

	if ((0xff & (gpio_port_pins_t)BIT(pp.pin)) == 0U) {
		LOG_ERR("Pin not support: port_pin= %x", port_pin);
		return 0;
	}
	if (flag == GPIO_CTRL_IRQ_ON) {
		set_pads_irq_enable(pp.gpio_dev, pp.pin);
	} else if (flag == GPIO_CTRL_IRQ_OFF) {
		set_pads_irq_disable(pp.gpio_dev, pp.pin);
	}
	return 0;
}

/**
 * @brief Set GPIO power state
 *
 * @param port_pin Combined port and pin value
 * @param pwrSt Power state to set
 * @return 0 on success, negative error code on failure
 */
int gpio_set_power(uint32_t port_pin, uint32_t pwrSt)
{
	int ret;
	struct gpio_port_pin pp;
	if (is_dummy_gpio(port_pin)) {
		return 0;
	}
	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}

	if ((0xff & (gpio_port_pins_t)BIT(pp.pin)) == 0U) {
		LOG_ERR("Pin not support: port_pin= %x", port_pin);
		return 0;
	}
	if (pwrSt == GPIO_CTRL_PWRG_OFF) {
		npcx_gpio_disable_io_pads(pp.gpio_dev, pp.pin);
	} else if (pwrSt == GPIO_CTRL_PWRG_VTR_IO) {
		npcx_gpio_enable_io_pads(pp.gpio_dev, pp.pin);
	}
	return 0;
}

/**
 * @brief Write value to GPIO pin
 *
 * @param port_pin Combined port and pin value
 * @param value Value to write (0 or 1)
 * @return 0 on success, negative error code on failure
 */
int gpio_write_pin(uint32_t port_pin, int value)
{
	int ret;
	struct gpio_port_pin pp;
	if (is_dummy_gpio(port_pin)) {
		return 0;
	}

	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}

	if ((0xff & (gpio_port_pins_t)BIT(pp.pin)) == 0U) {
		LOG_ERR("Pin not support: port_pin= %x", port_pin);
		return 0;
	}

	return gpio_pin_set_raw(pp.gpio_dev, pp.pin, value);
}

/**
 * @brief Read GPIO pin value
 *
 * @param port_pin Combined port and pin value
 * @return Pin value or negative error code
 */
int gpio_read_pin(uint32_t port_pin)
{
	int ret;
	struct gpio_port_pin pp;

	if (is_dummy_gpio(port_pin)) {
		return gpio_read_dummy_pin(port_pin);
	}

	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}
	for (uint8_t i = 0; _s_gpio_uasWasSetTab && i < _s_gpio_WasSetTab_Size; i++) {
		if (_s_gpio_uasWasSetTab[i] == port_pin) {
			return md_gpio_get_WAS_SET(GPIO_Pin2Addr(port_pin));
		}
	}
	if ((0xff & (gpio_port_pins_t)BIT(pp.pin)) == 0U) {
		LOG_ERR("Pin not support: port_pin= %x", port_pin);
		return 0;
	}
	return gpio_pin_get_raw(pp.gpio_dev, pp.pin);
}

/**
 * @brief Configure an array of GPIO pins
 *
 * @param gpios Pointer to array of GPIO configurations
 * @param len Number of entries in the array
 * @return 0 on success, negative error code on failure
 */
int gpio_configure_array(struct gpio_ec_config *gpios, uint32_t len)
{
	int ret;
	struct gpio_port_pin pp;

	/* Configure static board gpios */
	for (int i = 0; i < len; i++) {
		if (is_dummy_gpio(gpios[i].port_pin)) {
			continue;
		}

		ret = validate_device(gpios[i].port_pin, &pp);
		if (ret) {
			return ret;
		}

		LOG_DBG("i: %d port: %p pin: %d cfg: %x", i, pp.gpio_dev,
			pp.pin,
			gpios[i].cfg);

		if (!pp.gpio_dev) {
			LOG_ERR("Invalid port at %d", i);
			continue;
		}

		ret = gpio_pin_configure(pp.gpio_dev, pp.pin,
					 (~GPIO_INT_MASK)&gpios[i].cfg);

		ret = gpio_set_power(gpios[i].port_pin, gpios[i].pwr_cfg);

		if (ret) {
			LOG_ERR("Config fail: %d i:%d port:%p pin: %d cfg: %d",
				ret, i, pp.gpio_dev, pp.pin,
				gpios[i].cfg);
		}
	}

	return 0;
}

/**
 * @brief Initialize GPIO callback for a pin
 *
 * @param port_pin Combined port and pin value
 * @param callback Pointer to callback structure
 * @param handler Callback handler function
 * @return 0 on success, negative error code on failure
 */
int gpio_init_callback_pin(uint32_t port_pin,
			   struct gpio_callback *callback,
			   gpio_callback_handler_t handler)
{
	int ret;
	struct gpio_port_pin pp;

	if (is_dummy_gpio(port_pin)) {
		return 0;
	}

	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}

	/* zephyr function is void */
	gpio_init_callback(callback, handler, BIT(pp.pin));

	return 0;
}

/**
 * @brief Add GPIO callback for a pin
 *
 * @param port_pin Combined port and pin value
 * @param callback Pointer to callback structure
 * @return 0 on success, negative error code on failure
 */
int gpio_add_callback_pin(uint32_t port_pin,
			  struct gpio_callback *callback)
{
	int ret;
	struct gpio_port_pin pp;

	if (is_dummy_gpio(port_pin)) {
		return 0;
	}

	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}

	return gpio_add_callback(pp.gpio_dev, callback);
}

/**
 * @brief Remove GPIO callback for a pin
 *
 * @param port_pin Combined port and pin value
 * @param callback Pointer to callback structure
 * @return 0 on success, negative error code on failure
 */
int gpio_remove_callback_pin(uint32_t port_pin,
			     struct gpio_callback *callback)
{
	int ret;
	struct gpio_port_pin pp;

	if (is_dummy_gpio(port_pin)) {
		return 0;
	}

	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}

	return gpio_remove_callback(pp.gpio_dev, callback);
}

/**
 * @brief Configure GPIO interrupt for a pin
 *
 * @param port_pin Combined port and pin value
 * @param flags Interrupt configuration flags
 * @return 0 on success, negative error code on failure
 */
int gpio_interrupt_configure_pin(uint32_t port_pin, gpio_flags_t flags)
{
	int ret;
	struct gpio_port_pin pp;

	if (is_dummy_gpio(port_pin)) {
		return 0;
	}

	ret = validate_device(port_pin, &pp);
	if (ret) {
		return ret;
	}

	return gpio_pin_interrupt_configure(pp.gpio_dev, pp.pin, flags);
}

/**
 * @brief Check if GPIO port is enabled
 *
 * @param port_pin Combined port and pin value
 * @return true if enabled, false otherwise
 */
bool gpio_port_enabled(uint32_t port_pin)
{
	int ret;
	struct gpio_port_pin pp;

	if (is_dummy_gpio(port_pin)) {
		return 0;
	}

	ret = validate_device(port_pin, &pp);

	return ret == 0;
}

/**
 * @brief Force configure a GPIO pin
 *
 * @param port_pin Combined port and pin value
 * @param flags GPIO configuration flags
 * @return 0 on success, negative error code on failure
 */
int gpio_force_configure_pin(uint32_t port_pin, gpio_flags_t flags)
{

	int ret = 0;

	uint32_t port_idx;
	struct gpio_port_pin pp;

	ret = validate_device(port_pin, &pp);
	if (ret) {
		LOG_ERR("Invalid pin %d", pp.pin);
		return ret;
	}

	port_idx = gpio_get_port(port_pin);

	ret = gpio_pin_configure(pp.gpio_dev, pp.pin, (~GPIO_INT_MASK)&flags);

	if (ret) {
		LOG_ERR("Failed to configure pin %d", pp.pin);
		return ret;
	}

	return ret;
}

/**
 * @brief Force configure NPCX pin control
 *
 * @param alt Alternate function value
 * @param flag Pin control flag
 * @return 0 on success, negative error code on failure
 */
int gpio_force_configure_pin_npcx(uint8_t alt, uint8_t flag)
{

	int ret = 0;
	pinctrl_soc_pin_t pin;
	pin.cfg.periph.group = (alt & 0xf0) >> 4;
	pin.cfg.periph.bit = (alt & 0xf);
	pin.flags.type = NPCX_PINCTRL_TYPE_PERIPH;
	pin.flags.pinmux_gpio = flag;
	pin.cfg.periph.type = NPCX_PINCTRL_TYPE_PERIPH_PINMUX;

	ret = pinctrl_configure_pins(&pin, 1, PINCTRL_REG_NONE);
	if (ret) {
		LOG_ERR("Failed to configure pin control for GPIO ");
		return ret;
	}

	return ret;
}

/**
 * @brief Register WAS table for GPIO
 *
 * @param pTab Pointer to WAS table
 */
void gpio_reg_was_table(uint32_t *pTab)
{
	_s_gpio_WasSetTab_Size = 0;

	_s_gpio_uasWasSetTab = pTab;
	for (uint8_t i = 0; !GPIO_isNullPin(pTab[i]); i++) {
		_s_gpio_WasSetTab_Size++;
	}
}