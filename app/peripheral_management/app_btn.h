/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __PWRBTN_MGMT_H__
#define __PWRBTN_MGMT_H__

/**
 * @typedef pwrbtn_handler_t
 * @brief callback function for modules registered for power button events.
 * @param pwrbtn_sts current power button status.
 */
typedef void (*pwrbtn_handler_t)(uint8_t pwrbtn_sts);

/**
 * @brief Perform initialization of task that perform power button handling.
 */
int app_btn_init(void);

/**
 * @brief assert the FCH power button
 */
void app_btn_assertFchPwrBtn(void);

/**
 * @brief de-assert the FCH power button
 */
void app_btn_deassertFchPwrBtn(void);

/**
 * @brief slide shutdown handler
 */
void app_btn_slideHandler(void);

/**
 * @brief trigger fake power button click with "hold" ms
 */
void app_btn_triggerPwrBtn(uint8_t hold);

/**
 * @brief Allows to register modules interested in power button events.
 */
void app_btn_registerHandler(pwrbtn_handler_t handler);

/**
 * @brief process the power button deassert event
 */
void app_btn_deassertCallback(void);

/**
 * @brief process the power button assert event
 */
void app_btn_assertCallback(void);

/**
 * @brief reset EC chip and also reset the whole power sequence
 */
void app_btn_ecReset(void);

/**
 * @brief monitor power button related flag in power sequnece
 */
void app_btn_monitor (void);

/**
 *  @brief if fch power button notification is enabled
 */
uint8_t app_btn_isFchPwrBtnNotifyEnabled(void);

/**
 *  @brief Disable power button timer
 */
void app_btn_timer_stop_ex(void);

/**
 *  @brief Enable power button timer
 */
void app_btn_timer_start_ex(void);

#endif /* __PWRBTN_MGMT_H__ */
