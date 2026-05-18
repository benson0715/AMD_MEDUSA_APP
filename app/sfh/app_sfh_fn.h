/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_SFH_FN_H__
#define __APP_SFH_FN_H__

#include <zephyr/kernel.h>

/**
* @brief sfh event Enumeration
*/
enum {
	EVT_NOT_USED = 0,
	EVT_STATE_IN_BAG,
	EVT_STATE_OUT_OF_BAG,
};

/**
* @brief Platform OTD status Enumeration
*/
typedef enum {
	SFH_OTD_INVALID_STATE = 0,
	/**< invalid state */
	SFH_ON_TABLE = 1, /**< on
	Table detected */
	SFH_ON_LAP = 2, /**< on
	Lap detected */
	SFH_IN_TRANSIENT = 3,
	/**< In Transient State*/
} SFH_OnTable_OnLap;

/**
* @brief Platform InBag status Enumeration
*/
typedef enum SFH_InBagState {
	/** Current activity is unknown */
	SFH_IOB_INVALID_STATE = 0,
	/** Indicates that the device is in bag */
	SFH_STATE_IN_BAG = 1,
	/** Indicates that the device is out of bag */
	SFH_STATE_OUT_OF_BAG = 2,
	/** Indicates that the device state undefined */
	SFH_INBAG_STATE_UNDEFINED = 0x3,
} SFH_InBagState;

#define SFH_ON_DESK_MSK 0x0F
#define SFH_IN_BAG_MSK  0xF0

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
int app_sfh_notification_handler(volatile void* reg, uint16_t index, uint8_t* u8_data, bool is_read);

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
int app_sfh_on_desk_lap_handler(volatile void* reg, uint16_t index, uint8_t* u8_data, bool is_read);

/*****************************************************************************
 * Function name:   app_sfh_on_io_expander_handler
 * Description:     Handle SFH gpio expander register read/write operations.
 *                  Supports mask and value registers for gpio expander logic.
 * @param           reg     - Pointer to IO expander register array (volatile)
 * @param           index   - Register offset index (0 for mask, 1 for value)
 * @param           u8_data - Pointer to data byte for read/write operation
 * @param           is_read - true for read operation, false for write
 * @return          0 on success, -EINVAL on invalid parameters
 * @note            Manages IO expander mask and value logic for GPIO control
 *****************************************************************************/
int app_sfh_on_gpio_expander_handler(volatile void* reg, uint16_t index, uint8_t* u8_data, bool is_read);

/*****************************************************************************
 * Function name:   app_sfh_fn_init
 * Description:     Initialize SFH function context and polling infrastructure
 * @param           None
 * @return          None (void function)
 * @note            Sets up polling signals, events, and work queue for SFH processing
 *****************************************************************************/
void app_sfh_fn_init(void);

/*****************************************************************************
 * Function name:   app_sfh_in_bag_flag_get
 * Description:     Get current in-bag detection flag status
 * @param           None
 * @return          bool - true if device is detected as in bag, false otherwise
 * @note            Returns the current state of the in_bag_flag from context
 *****************************************************************************/
bool app_sfh_in_bag_flag_get(void);

#endif // end of __APP_SFH_FN_H__

