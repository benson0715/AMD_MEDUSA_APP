/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include "app_sfh.h"
#include "app_sfh_fn.h"
#include <zephyr/logging/log.h>
#include "u_rrq.h"
#include "board_config.h"
#include "i2c_hub.h"
#include "app_acpi.h"
LOG_MODULE_REGISTER(sfh,  CONFIG_SFH_LOG_LEVEL);

#ifndef SFH_I2C_SLAVE_ADDRESS
#define SFH_I2C_SLAVE_ADDRESS (0x02u)  // Default I2C slave address for SFH
#endif

/* Forward declarations for I2C target callback functions */
static int sfh_write_requested(struct i2c_target_config *config);
static int sfh_write_received(struct i2c_target_config *config, uint8_t val);
static int sfh_read_requested(struct i2c_target_config *config, uint8_t *val);
static int sfh_read_processed(struct i2c_target_config *config, uint8_t *val);
static int sfh_stop(struct i2c_target_config *config);

/* Global SFH register structure - initialized with default values */
struct sfh_reg g_reg_init = {SFH_REG_VAL_NOT_USED};

/* I2C target callback function table for SFH operations */
const struct i2c_target_callbacks sfh_cb_api_init = {
	.write_requested = sfh_write_requested,
	.read_requested = sfh_read_requested,
	.write_received = sfh_write_received,
	.read_processed = sfh_read_processed,
	.stop = sfh_stop,
};

/* SFH register configuration table - defines accessible registers and their properties */
struct sfh_reg_setting sfh_reg_setting_init[] = {
	SFH_REG_SETTING_INIT(notification, SFH_REG_ACCESS_RW, app_sfh_notification_handler),
	SFH_REG_SETTING_INIT(on_desk_lap, SFH_REG_ACCESS_WO, app_sfh_on_desk_lap_handler),
	SFH_REG_SETTING_INIT(gpio_expander, SFH_REG_ACCESS_RW, app_sfh_on_gpio_expander_handler),
	// SFH_REG_SETTING_INIT_N(custom_sensor_flags, 16, SFH_REG_ACCESS_RW, NULL), // sample use

	SFH_REG_SETTING_END(),  // Sentinel to mark end of settings array
};

static struct i2c_sfh_context g_sfh_dev_ctx = {
	.version = SFH_DEV_SPEC_VER,
	.reg = &g_reg_init,
	.setting = &sfh_reg_setting_init[0],
	.config = {
		.address = SFH_I2C_SLAVE_ADDRESS,
		.callbacks = &sfh_cb_api_init,
	},
};

/*****************************************************************************
 * Function name:   sfh_data_context_get
 * Description:     Extract SFH data context from I2C target configuration
 * @param           config - I2C target configuration pointer
 * @return          Pointer to SFH data structure
 * @note            Uses CONTAINER_OF macro to get parent structure
 *****************************************************************************/
static inline struct sfh_data* sfh_data_context_get(struct i2c_target_config *config)
{
	struct i2c_sfh_context *sfh = CONTAINER_OF(config, struct i2c_sfh_context, config);

	return &sfh->data;
}

/*****************************************************************************
 * Function name:   app_sfh_reg_setting_get
 * Description:     Find register setting configuration for given register address
 * @param           sfh - SFH context containing register settings table
 * @param           reg - Register address to look up
 * @return          Pointer to register setting or NULL if not found
 * @note            Searches through settings table for matching address range
 *****************************************************************************/
static struct sfh_reg_setting* app_sfh_reg_setting_get(struct i2c_sfh_context *sfh, uint16_t reg)
{
	struct sfh_reg_setting* setting = sfh->setting;

	/* Iterate through register settings table until sentinel */
	while (setting->address != SFH_REG_INVALID) {
		/* Check if register falls within this setting's address range */
		if ((reg >= setting->address) && (reg < (setting->address + setting->size))) {
			return setting;
		}

		setting++;
	}

	return NULL;  // Register not found in settings table
}

/*****************************************************************************
 * Function name:   app_sfh_data_process
 * Description:     Process SFH register read/write operations with validation
 * @param           sfh  - SFH context containing register settings
 * @param           data - Transaction data containing register and payload
 * @param           value - Operation type
 * @return          0 on success, SFH_ERR on error
 * @note            Handles both read and write operations with access control
 *****************************************************************************/
static int app_sfh_data_process(struct i2c_sfh_context *sfh, struct sfh_data *data, uint8_t value)
{
	uint16_t idx = 0u;
	uint16_t i = 0u;

	struct sfh_reg_setting* setting = app_sfh_reg_setting_get(sfh, data->reg);
	if (!setting) {
		memset(data->cache, SFH_REG_VAL_ERROR, sizeof(data->cache));
		LOG_WRN("SFH: not used register 0x%x", data->reg);
		return 0;
	}

	if (value == 0) {
		if (data->write_index) { // write done operation
			if ((setting->access == SFH_REG_ACCESS_RO) || (setting->access == SFH_REG_ACCESS_RSVD)) {
				LOG_ERR("SFH: register access error, reg=0x%x", data->reg);
				return SFH_ERR;
			}

			if (setting->fn) {
				i = 0;
				idx = data->reg - setting->address;
				while ((i < data->write_index) && (idx < setting->size) && (i < SFH_CACHE_SIZE)) {
					setting->fn(setting->reg_ptr, idx, &data->cache[i], false);
					idx++;
					i++;
				}
			}

			data->write_index = 0u;
			return 0;
		}
	} else if (value == 1) {
		// read request operation
		memset(data->cache, 0x0, sizeof(data->cache));
		if ((setting->access == SFH_REG_ACCESS_WO) || (setting->access == SFH_REG_ACCESS_RSVD)) { // read operation
			LOG_ERR("SFH: register access error, reg=0x%x", data->reg);
			return SFH_ERR;
		}

		if (setting->fn) {
			i = 0;
			idx = data->reg - setting->address;
			while ((idx < setting->size) && (i < SFH_CACHE_SIZE)) {
				setting->fn(setting->reg_ptr, idx, &data->cache[i], true);
				idx++;
				i++;
			}
		}
	} else if (value == 2) {
		// read operation
		// should handle it in read request operation, if no specified use case.
	}

	return 0;
}

/*****************************************************************************
 * Function name:   app_sfh_data_step
 * Description:     Process SFH data operation based on opcode state machine
 * @param           config - I2C target configuration
 * @param           opcode - Operation code to add to current state
 * @return          0 on success, error code on failure
 * @note            Manages state transitions for I2C read/write operations
 *****************************************************************************/
static int app_sfh_data_step(struct i2c_target_config *config, uint8_t opcode)
{
	struct i2c_sfh_context *sfh = CONTAINER_OF(config, struct i2c_sfh_context, config);
	struct sfh_data *data = sfh_data_context_get(config);

	data->opcode |= opcode;
	if (data->opcode == OPCODE_WRITE_DATA_DONE_BITS) {
		return app_sfh_data_process(sfh, data, 0);
	} else if (data->opcode == OPCODE_READ_DATA_REQ_BITS) {
		return app_sfh_data_process(sfh, data, 1);
	} else if (data->opcode == OPCODE_READ_DATA_BITS) {
		return app_sfh_data_process(sfh, data, 2);
	}

	if (data->opcode & OPCODE_STOP) {
		data->opcode = 0;
	}
	
	return 0; // nothing to do
}

/*****************************************************************************
 * Function name:   sfh_write_requested
 * Description:     I2C callback when master initiates write transaction
 * @param           config - I2C target configuration
 * @return          0 on success
 * @note            Initializes transaction state and clears buffers
 *****************************************************************************/
static int sfh_write_requested(struct i2c_target_config *config)
{
	struct sfh_data *data = sfh_data_context_get(config);

	data->reg = SFH_REG_INVALID;
	data->write_index = 0u;
	data->read_index = 0u;
	data->write_cnt = 0u;
	data->opcode = 0;
	memset(data->cache, 0x0, sizeof(data->cache));
	LOG_DBG("SFH: I2C Slave write request");

	return 0;
}

/*****************************************************************************
 * Function name:   sfh_write_received
 * Description:     I2C callback when master sends a data byte
 * @param           config - I2C target configuration
 * @param           val    - Received data byte
 * @return          0 on success, SFH_ERR on error
 * @note            First 1-2 bytes are register address, rest is data payload
 *****************************************************************************/
static int sfh_write_received(struct i2c_target_config *config, uint8_t val)
{
	struct sfh_data *data = sfh_data_context_get(config);
	uint8_t reg_sz = SFH_REG_BITS >> 3u;  // Convert bits to bytes (register address size)

	/* Check for buffer overflow protection */
	if (data->write_index >= SFH_CACHE_SIZE) {
		LOG_ERR("SFH: I2C Slave write error, size=0x%x", data->write_index);
		return SFH_ERR;
	}

	LOG_DBG("SFH: I2C Slave write done, cnt=%d val=0x%x", data->write_cnt, val);
	data->write_cnt++;

	/* Handle register address bytes (first 1-2 bytes) */
	if (data->write_cnt <= reg_sz) {
		if (data->write_cnt == 1u) {
			data->reg = val;  // LSB of register address
		} else if (data->write_cnt == 2u) {
			data->reg |= val << 8;  // MSB of register address (16-bit register)
		} else {
			LOG_ERR("SFH: I2C Slave write error, reg size=%d", data->write_cnt);
			return SFH_ERR;
		}
		
		data->write_index = 0u;
		memset(data->cache, 0x0, sizeof(data->cache));
		if (data->write_cnt == reg_sz) {
			app_sfh_data_step(config, OPCODE_WRITE_REG);
		}
		return 0;
	}

	/* Store data payload bytes in cache */
	data->cache[data->write_index++] = val;
	app_sfh_data_step(config, OPCODE_WRITE_DATA);
	return 0;
}

/*****************************************************************************
 * Function name:   sfh_read_requested
 * Description:     I2C callback when master initiates read transaction
 * @param           config - I2C target configuration
 * @param           val    - Pointer to store first byte to send
 * @return          0 on success
 * @note            Returns first byte from cache, resets counters
 *****************************************************************************/
static int sfh_read_requested(struct i2c_target_config *config, uint8_t *val)
{
	struct sfh_data *data = sfh_data_context_get(config);

	data->write_cnt = 0u;
	data->read_index = 0u;

	app_sfh_data_step(config, OPCODE_READ_DATA_REQ);
	*val = data->cache[data->read_index++];
	LOG_DBG("SFH: I2C Slave read request done, val=0x%x", *val);

	return 0;
}

/*****************************************************************************
 * Function name:   sfh_read_processed
 * Description:     I2C callback for subsequent read bytes after first
 * @param           config - I2C target configuration
 * @param           val    - Pointer to store next byte to send
 * @return          0 on success, SFH_ERR on error
 * @note            Returns subsequent bytes from cache with bounds checking
 *****************************************************************************/
static int sfh_read_processed(struct i2c_target_config *config, uint8_t *val)
{
	struct sfh_data *data = sfh_data_context_get(config);

	data->write_cnt = 0u;
	*val = SFH_REG_VAL_ERROR;  // Default error value

	/* Bounds check to prevent buffer overflow */
	if (data->read_index >= SFH_CACHE_SIZE) {
		LOG_ERR("SFH: I2C Slave read error, size=0x%x", data->read_index);
		return SFH_ERR;
	}

	/* Return next byte from cache and increment index */
	app_sfh_data_step(config, OPCODE_READ_DATA);
	*val = data->cache[data->read_index++];
	LOG_DBG("SFH: I2C Slave read done, val=0x%x", *val);

	return 0;
}

/*****************************************************************************
 * Function name:   sfh_stop
 * Description:     I2C callback when transaction completes (STOP condition)
 * @param           config - I2C target configuration
 * @return          Result of data processing operation
 * @note            Triggers processing of accumulated transaction data
 *****************************************************************************/
static int sfh_stop(struct i2c_target_config *config)
{
	LOG_DBG("SFH: I2C Slave stop");

	return app_sfh_data_step(config, OPCODE_STOP);
}

/*****************************************************************************
 * Function name:   app_sfh_init
 * Description:     Initialize SFH (Smart Fan Hub) I2C slave interface
 * @param           None
 * @return          None
 * @note            Registers SFH as I2C slave device on configured port
 *****************************************************************************/
void app_sfh_init(void)
{
	LOG_INF("SFH feature enable, version = %s", SFH_DEV_SPEC_VER);
	app_sfh_fn_init();
	i2c_hub_slave_register(SFH_I2C_PORT, &g_sfh_dev_ctx.config);
}