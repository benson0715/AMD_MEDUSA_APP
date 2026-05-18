/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include "app_sfh.h"
#include "app_sfh_fn.h"
#include <zephyr/logging/log.h>
#include "u_rrq.h"
#include <zephyr/drivers/i2c.h>
#include "board_config.h"
#include "i2c_hub.h"
#include "app_acpi.h"
#include "board_pwrSrc.h"

LOG_MODULE_REGISTER(sfh_fn, CONFIG_SFH_LOG_LEVEL);

struct sfh_fn_context {
	bool in_bag_flag;

	volatile int event;
	struct k_work work;
};
#if CONFIG_HOT_BAG_EN
extern bool g_lidCloseEnabled; 
#endif
static struct sfh_fn_context sfh_fn_ctx = {0};

/*****************************************************************************
 * Function name:   app_sfh_notification_handler
 * Description:     Handle SFH notification register read/write operations
 * @param           reg     - Pointer to notification register (volatile)
 * @param           index   - Register offset index (unused in this handler)
 * @param           u8_data - Pointer to data byte for read/write operation
 * @param           is_read - true for read operation, false for write
 * @return          0 on success, -EINVAL on invalid parameters
 * @note            Provides bidirectional access to notification status register
 *****************************************************************************/
int app_sfh_notification_handler(volatile void* reg, uint16_t index, uint8_t* u8_data, bool is_read)
{
	/* Parameter validation - ensure pointers are valid */
	if (!reg || !u8_data) {
		LOG_ERR("SFH_FN: Invalid parameters");
		return -EINVAL;
	}
	LOG_DBG("SFH_FN %s: %s index %d, data 0x%x", __FUNCTION__, is_read ? "r":"w", index, *u8_data);

	/* Note: index parameter unused for single-byte register access */
	(void)index;  // Suppress unused parameter warning

	/* Cast volatile void pointer to notification register pointer */
	uint8_t* notify = (uint8_t*)reg;

	if (is_read) {
		/* Read operation: Copy register value to output data */
		*u8_data = *notify;
	} else {
		/* Write operation: Update register with input data */
		*notify = *u8_data;
	}

	return 0;  // Success
}

/*****************************************************************************
 * Function name:   app_sfh_on_desk_lap_handler
 * Description:     Handle SFH on-desk/lap detection register read/write operations
 * @param           reg     - Pointer to on-desk/lap status register (volatile)
 * @param           index   - Register offset index (unused in this handler)
 * @param           u8_data - Pointer to data byte for read/write operation
 * @param           is_read - true for read operation, false for write
 * @return          0 on success, -EINVAL on invalid parameters
 * @note            Manages laptop placement detection status for thermal management
 *****************************************************************************/
int app_sfh_on_desk_lap_handler(volatile void* reg, uint16_t index, uint8_t* u8_data, bool is_read)
{
	if (!reg || !u8_data) {
		LOG_ERR("SFH_FN: Invalid parameters");
		return -EINVAL;
	}
	LOG_DBG("SFH_FN %s: %s index %d, data 0x%x", __FUNCTION__, is_read ? "r":"w", index, *u8_data);

	(void)index;

	uint8_t* on_desk_lap_sts = (uint8_t*)reg;

	if (is_read) {
		*u8_data = *on_desk_lap_sts;
	} else {
		*on_desk_lap_sts = *u8_data;

		uint8_t bag_sts = (*on_desk_lap_sts & SFH_IN_BAG_MSK) >> 4;
		if (bag_sts == SFH_STATE_IN_BAG) {
			sfh_fn_ctx.event = EVT_STATE_IN_BAG;
			k_work_submit(&sfh_fn_ctx.work);
		} else if (bag_sts == SFH_STATE_OUT_OF_BAG) {
			sfh_fn_ctx.event = EVT_STATE_OUT_OF_BAG;
			k_work_submit(&sfh_fn_ctx.work);
		}
	}

	return 0;
}

/*****************************************************************************
 * Function name:   app_sfh_on_gpio_expander_handler
 * Description:     Handle SFH gpio expander register read/write operations.
 *                  Supports mask and value registers for gpio expander logic.
 * @param           reg     - Pointer to IO expander register array (volatile)
 * @param           index   - Register offset index (0 for mask, 1 for value)
 * @param           u8_data - Pointer to data byte for read/write operation
 * @param           is_read - true for read operation, false for write
 * @return          0 on success, -EINVAL on invalid parameters
 * @note            Manages IO expander mask and value logic for GPIO control
 *****************************************************************************/
int app_sfh_on_gpio_expander_handler(volatile void* reg, uint16_t index, uint8_t* u8_data, bool is_read)
{
	/* Parameter validation - ensure pointers are valid */
	if (!reg || !u8_data) {
		LOG_ERR("SFH_FN: Invalid parameters");
		return -EINVAL;
	}

	LOG_DBG("SFH_FN %s: %s index %d, data 0x%x", __FUNCTION__, is_read ? "r":"w", index, *u8_data);
	static uint8_t gpio_native_value = 0; // TODO

	/* 0xA0: mask */
	/* 0xA1: value */
	uint8_t* val_msk = (uint8_t*)reg;

	if (is_read) {
		if (index == 0u) { // mask
			*u8_data = val_msk[0];
		} else if (index == 1u) { // value
			val_msk[1] = val_msk[0] & gpio_native_value;

			*u8_data = val_msk[1];
		}
	} else {
		if (index == 0u) { // mask
			val_msk[0] = *u8_data;
		} else if (index == 1u) { // value
			val_msk[1] = *u8_data;

			// Apply mask to the written value to update native GPIO state
			gpio_native_value = (gpio_native_value & ~val_msk[0]) | (val_msk[1] & val_msk[0]);
		}
	}

	return 0;
}

/*****************************************************************************
 * Function name:   app_sfh_in_bag_probe
 * Description:     Probe and determine if device is in a bag based on system state
 * @param           event - Event type (EVT_STATE_IN_BAG or EVT_STATE_OUT_OF_BAG)
 * @return          None (void function)
 * @note            Sets in_bag_flag based on lid closure, power source, and event
 *****************************************************************************/
static void app_sfh_in_bag_probe(int event)
{
#if CONFIG_HOT_BAG_EN
	extern bool g_hotBagEnabled;
	bool dc_only = board_pwrSrc_hasAcPowerSource() ? false : true;
	
	if (g_lidCloseEnabled && dc_only && event == EVT_STATE_IN_BAG && g_hotBagEnabled) {
		LOG_ERR("Hot bag detected, force G3 from MP2 notification");
		app_pseq_nextStep(FORCE_G3, 1); /* need to force G3 */
		sfh_fn_ctx.in_bag_flag = true;
	} else {
		sfh_fn_ctx.in_bag_flag = false;
		LOG_WRN("Hot bag not detected from MP2 notification");
	}
#else
	sfh_fn_ctx.in_bag_flag = false;
	LOG_WRN("Hot bag detected %d from MP2 notification", event);
#endif
}

/*****************************************************************************
 * Function name:   app_sfh_fn_post
 * Description:     Work handler for processing SFH function events
 * @param           w - Pointer to work structure (unused in implementation)
 * @return          None (void function)
 * @note            Processes in-bag/out-of-bag events and resubmits work polling
 *****************************************************************************/
static void app_sfh_fn_post(struct k_work *w)
{
	int event = sfh_fn_ctx.event;
	LOG_INF("SFH fn post event %d", event);

	switch (event) {
		case EVT_STATE_IN_BAG:
		case EVT_STATE_OUT_OF_BAG: {
			app_sfh_in_bag_probe(event);
			break;
		};
		default: {
			LOG_DBG("SFH_FN: does not support event %d", event);
			break;
		}
	}
}

/*****************************************************************************
 * Function name:   app_sfh_fn_init
 * Description:     Initialize SFH function context and polling infrastructure
 * @param           None
 * @return          None (void function)
 * @note            Sets up polling signals, events, and work queue for SFH processing
 *****************************************************************************/
void app_sfh_fn_init(void)
{
	k_work_init(&sfh_fn_ctx.work, app_sfh_fn_post);
}

/*****************************************************************************
 * Function name:   app_sfh_in_bag_flag_get
 * Description:     Get current in-bag detection flag status
 * @param           None
 * @return          bool - true if device is detected as in bag, false otherwise
 * @note            Returns the current state of the in_bag_flag from context
 *****************************************************************************/
bool app_sfh_in_bag_flag_get(void)
{
	return sfh_fn_ctx.in_bag_flag;
}