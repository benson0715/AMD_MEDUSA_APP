/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#include <zephyr/types.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include "board_config.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(i2c_hub, CONFIG_I2C_HUB_LOG_LEVEL);

struct i2c_dev_inst {
	const struct device *device;
	uint8_t speed;
	struct k_mutex mutex;
};

static struct i2c_dev_inst i2c_inst[] = {
	{
		.device = DEVICE_DT_GET(I2C_BUS_0),
		/* Standard speed - 100kHz */
		.speed = I2C_SPEED_STANDARD,
	},
	{
		.device = DEVICE_DT_GET(I2C_BUS_1),
		/* Standard speed - 100kHz */
		.speed = I2C_SPEED_STANDARD,
	},
	{
		.device = DEVICE_DT_GET(I2C_BUS_2),
		/* Standard speed - 100kHz */
		.speed = I2C_SPEED_STANDARD,
	},
	{
		.device = DEVICE_DT_GET(I2C_BUS_3),
		/* Standard speed - 100kHz */
		.speed = I2C_SPEED_STANDARD,
	},
};

#define NUM_OF_I2C_BUS		ARRAY_SIZE(i2c_inst)

/**
 * @brief Configure I2C bus instance
 */
int i2c_hub_config(uint8_t instance)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}

	if (!device_is_ready(i2c_inst[instance].device)) {
		LOG_ERR("i2c%d not ready", instance);
		return -EINVAL;
	}

	ret = i2c_configure(i2c_inst[instance].device,
			    I2C_SPEED_SET(i2c_inst[instance].speed) |
			    I2C_MODE_CONTROLLER);
	if (ret) {
		LOG_ERR("Error:%d failed to configure i2c device", ret);
		return ret;
	}

	k_mutex_init(&i2c_inst[instance].mutex);

	return 0;
}

/**
 * @brief Set I2C bus speed for specified instance
 */
int i2c_hub_set_speed(uint8_t instance, uint8_t speed)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);
	i2c_inst[instance].speed = speed;
	ret = i2c_configure(i2c_inst[instance].device,
			    I2C_SPEED_SET(i2c_inst[instance].speed) |
			    I2C_MODE_CONTROLLER);
	if (ret) {
		LOG_ERR("Error:%d failed to set i2c device speed", ret);
	}

	k_mutex_unlock(&i2c_inst[instance].mutex);
	return ret;
}

/**
 * @brief Write data to I2C device
 */
int i2c_hub_write(uint8_t instance, const uint8_t *buf,
		  uint32_t num_bytes, uint16_t addr)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);
	ret = i2c_write(i2c_inst[instance].device, buf, num_bytes, addr);
	k_mutex_unlock(&i2c_inst[instance].mutex);

	return ret;
}

/**
 * @brief Read data from I2C device
 */
int i2c_hub_read(uint8_t instance, uint8_t *buf,
		  uint32_t num_bytes, uint16_t addr)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);
	ret = i2c_read(i2c_inst[instance].device, buf, num_bytes, addr);
	k_mutex_unlock(&i2c_inst[instance].mutex);

	return ret;
}

/**
 * @brief Write then read data from I2C device
 */
int i2c_hub_write_read(uint8_t instance, uint16_t addr, const void *write_buf,
		       size_t num_write, void *read_buf, size_t num_read)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);
	ret = i2c_write_read(i2c_inst[instance].device, addr, write_buf,
			     num_write, read_buf, num_read);
	k_mutex_unlock(&i2c_inst[instance].mutex);

	return ret;
}

/**
 * @brief Burst read from I2C device register
 */
int i2c_hub_burst_read(uint8_t instance, uint16_t dev_addr,
		  uint8_t reg_addr, uint8_t *value, uint32_t len)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);
	ret = i2c_burst_read(i2c_inst[instance].device, dev_addr,
				reg_addr, value, len);
	k_mutex_unlock(&i2c_inst[instance].mutex);

	return ret;
}

/**
 * @brief Burst write to I2C device register
 */
int i2c_hub_burst_write(uint8_t instance, uint16_t dev_addr,
		  uint8_t reg_addr, uint8_t *value, uint32_t len)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);
	ret = i2c_burst_write(i2c_inst[instance].device, dev_addr,
				reg_addr, value, len);
	k_mutex_unlock(&i2c_inst[instance].mutex);

	return ret;
}

/**
 * @brief Burst write with multiple command bytes to I2C device
 */
int i2c_hub_burst_write_multi_cmd(uint8_t instance, uint16_t dev_addr,
		  uint8_t *cmd, uint32_t clen, uint8_t *value, uint32_t vlen)
{
	int ret;
	struct i2c_msg msg[2];

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);

	msg[0].buf = (uint8_t *)cmd;
	msg[0].len = clen;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = (uint8_t *)value;
	msg[1].len = vlen;
	msg[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	ret = i2c_transfer(i2c_inst[instance].device, msg, 2, dev_addr);

	k_mutex_unlock(&i2c_inst[instance].mutex);

	return ret;
}

/**
 * @brief Read single byte from I2C device register
 */
int i2c_hub_reg_read_byte(uint8_t instance,
				    uint16_t dev_addr,
				    uint8_t reg_addr, uint8_t *value)
{
	return i2c_hub_write_read(instance, dev_addr, &reg_addr,
							  sizeof(reg_addr), value, sizeof(*value));
}

/**
 * @brief Write single byte to I2C device register
 */
int i2c_hub_reg_write_byte(uint8_t instance,
				     uint16_t dev_addr,
				     uint8_t reg_addr, uint8_t value)
{
	uint8_t tx_buf[2] = {reg_addr, value};

	return i2c_hub_write(instance, tx_buf, 2, dev_addr);
}

/**
 * @brief Register I2C target device
 */
int i2c_hub_slave_register(uint8_t instance, struct i2c_target_config *cfg)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);
	ret = i2c_target_register(i2c_inst[instance].device, cfg);
	k_mutex_unlock(&i2c_inst[instance].mutex);

	return ret;
}

/**
 * @brief Unregister I2C target device
 */
int i2c_hub_slave_unregister(uint8_t instance, struct i2c_target_config *cfg)
{
	int ret;

	if (instance >= NUM_OF_I2C_BUS) {
		return -ENODEV;
	}
	if (!device_is_ready(i2c_inst[instance].device)) {
		return -ENODEV;
	}

	k_mutex_lock(&i2c_inst[instance].mutex, K_FOREVER);
	ret = i2c_target_unregister(i2c_inst[instance].device, cfg);
	k_mutex_unlock(&i2c_inst[instance].mutex);

	return ret;
}