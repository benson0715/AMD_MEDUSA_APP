/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include "app_sfh_waie.h"
#include "app_sfh_fn.h"
#include <zephyr/logging/log.h>
#include "u_rrq.h"
#include <zephyr/drivers/i2c.h>
#include "board_config.h"
#include "i2c_hub.h"
#include "app_acpi.h"
#include "dev_mpc.h"
#include "amd_crb_modbusrtu_driver.h"
LOG_MODULE_REGISTER(sfh_waie, CONFIG_SFH_WAIE_LOG_LEVEL);

#ifndef SFH_WAIE_I2C_SLAVE_WAIE_ADDRESS
#define SFH_WAIE_I2C_SLAVE_WAIE_ADDRESS (0x58u)  /* Default I2C slave address for SFH */
#endif

/* Forward declarations for I2C target callback functions */
static int sfh_write_requested(struct i2c_target_config *config);
static int sfh_write_received(struct i2c_target_config *config, uint8_t val);
static int sfh_read_requested(struct i2c_target_config *config, uint8_t *val);
static int sfh_read_processed(struct i2c_target_config *config, uint8_t *val);
static int sfh_stop(struct i2c_target_config *config);

static unsigned char page_sel = 0;
uint8_t waie_syc_flag = 0xD1;

/* Global SFH register structure - initialized with default values */
static struct sfh_reg g_reg_init = {SFH_REG_VAL_NOT_USED};

/* I2C target callback function table for SFH operations */
static const struct i2c_target_callbacks sfh_cb_api_init = {
	.write_requested = sfh_write_requested,
	.read_requested = sfh_read_requested,
	.write_received = sfh_write_received,
	.read_processed = sfh_read_processed,
	.stop = sfh_stop,
};

/* SFH register configuration table - defines accessible registers and their properties */
static struct sfh_reg_setting sfh_reg_setting_init[] = {
	SFH_REG_SETTING_INIT(notification, SFH_REG_ACCESS_RW, app_sfh_notification_handler),
	SFH_REG_SETTING_INIT(on_desk_lap, SFH_REG_ACCESS_WO, app_sfh_on_desk_lap_handler),
	SFH_REG_SETTING_INIT(gpio_expander, SFH_REG_ACCESS_RW, app_sfh_on_gpio_expander_handler),
	SFH_REG_SETTING_END(),
};

/* Main SFH device context - contains all configuration and state information */
static struct i2c_sfh_context g_sfh_dev_ctx = {
	.version = SFH_DEV_SPEC_VER,
	.reg = &g_reg_init,
	.setting = &sfh_reg_setting_init[0],
	.config = {
		.address = SFH_WAIE_I2C_SLAVE_WAIE_ADDRESS,
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
static inline struct sfh_data *sfh_data_context_get(struct i2c_target_config *config)
{
	struct i2c_sfh_context *sfh = CONTAINER_OF(config, struct i2c_sfh_context, config);

	return &sfh->data;
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
	*val = SFH_REG_VAL_ERROR;  /* Default error value */

	/* Bounds check to prevent buffer overflow */
	if (data->read_index >= SFH_CACHE_SIZE) {
		LOG_ERR("SFH: I2C Slave read error, size=0x%x", data->read_index);
		return SFH_ERR;
	}
	if ((data->read_index == 0) && (page_sel == 0)) {
		*val = waie_syc_flag;
		data->read_index++;
	} else {
		/* Return next byte from cache and increment index */
		*val = *(unsigned char *)((unsigned char *)(&modbus_dev.registers[DEV_WAIE_POWER_REG]) + page_sel * 256 + data->read_index - 1);
		data->read_index++;
		if (data->read_index >= 3584) {
			data->read_index = 0;
		}
	}
	LOG_DBG("SFH: I2C Slave read done, val=0x%x", *val);

	return 0;
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

	/* Check for buffer overflow protection */
	if (data->write_index >= SFH_CACHE_SIZE) {
		LOG_ERR("SFH: I2C Slave write error, size=0x%x", data->write_index);
		return SFH_ERR;
	}

	LOG_DBG("SFH: I2C Slave write done, cnt=%d val=0x%x", data->write_cnt, val);
	data->write_cnt++;

	/* Handle register address bytes (first 1 bytes) */
	if (data->write_cnt <= 1) {
		if (data->write_cnt == 1u) {
			data->reg = val;  /* LSB of register address */
		} else if (data->write_cnt == 2u) {
			data->reg |= val << 8;  /* MSB of register address (16-bit register) */
		} else {
			LOG_ERR("SFH: I2C Slave write error, reg size=%d", data->write_cnt);
			return SFH_ERR;
		}
		data->write_index = 0u;

		return 0;
	}

	if (data->write_index == 0) {
		if (val == 2) {
			waie_syc_flag = 0;
		} else if (val == 3) {
			waie_syc_flag = 1;
		} else if (val == 1) {
			waie_syc_flag = 1;
		} else {
			waie_syc_flag = 0xD1;
		}
		LOG_DBG("SFH: waie_syc_flag=%d", waie_syc_flag);
	} else if (data->write_index == 1) {
		page_sel = val;
		LOG_DBG("SFH: page_sel=%d", page_sel);
	}
	/* Store data payload bytes in cache */
	data->cache[data->write_index++] = val;

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
	*val = data->cache[data->read_index++];
	LOG_DBG("SFH: I2C Slave read request done, val=0x%x", *val);

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
	LOG_DBG("SFH: stop");
	return 0;
}

/*****************************************************************************
 * Function name:   app_sfh_waie_init
 * Description:     Initialize SFH (MP2 Hub) I2C slave interface
 * @param           None
 * @return          None
 * @note            Registers SFH as I2C slave device on configured port
 *****************************************************************************/
void app_sfh_waie_init(void)
{
	LOG_INF("SFH feature enable, version = %s", SFH_DEV_SPEC_VER);
	app_sfh_fn_init();
	i2c_hub_slave_register(SFH_I2C_PORT, &g_sfh_dev_ctx.config);
}