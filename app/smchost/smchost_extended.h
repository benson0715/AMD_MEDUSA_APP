/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#ifndef __SMCHOST_EXTENDED_H__
#define __SMCHOST_EXTENDED_H__


/**
 * @brief Handle extended SMC commands to retrieve EC information.
 *
 * @param command identifier for the operation requested.
 */
//[AMD]++
//void smchost_cmd_info_handler(uint8_t command);
//[AMD]--

/**
 * @brief Handle extended SMC commands for power management operations.
 *
 * @param command identifier for the operation requested.
 */
void smchost_cmd_pm_handler(uint8_t command);

#if CONFIG_THERMAL_MANAGEMENT
/**
 * @brief Handle extended SMC commands for thermal management.
 *
 * @param command identifier for the operation requested.
 */
void smchost_cmd_thermal_handler(uint8_t command);
#endif

/**
 * @brief Handle power button events.
 *
 * @param pwrbtn_sts the current power button status.
 */
void smchost_pwrbtn_handler(uint8_t pwrbtn_sts);

/**
 * @brief Get connected standby state.
 */
bool smchost_is_system_in_cs(void);

/**
 * @brief Handle Power button press Power Loss Notification (PLN) events to SSD
 *
 * @param pwrbtn_sts the current power button status.
 */
void smchost_pwrbtn_pln_handler(uint8_t pwrbtn_sts);

/**
 * @brief platform reset deassert notification for pln.
 */
void smchost_pln_pltreset_handler(void);

/**
 * @brief probe and handle the post pwrbtn event.
 */
void smchost_pwrbtn_post_signal_probe(void);

#endif /* __SMCHOST_EXTENDED_H__ */
