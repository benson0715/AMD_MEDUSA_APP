/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __APP_PSEQ_H__
#define __APP_PSEQ_H__

#include "system.h"

/**
 * @brief Routine that handles eSPI handshake with PMC.
 *
 * This routines also handles system events such as enter/exit S3/S4/S5 and
 * user events related to power/reset sequence.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void app_pseq_thread(void *p1, void *p2, void *p3);

/**
 * @brief Handles power sequencing error.
 *
 * This routines displays error code and stops any power sequencing.
 *
 * @param error_code the power sequencing error.
 */
void app_pseq_error(uint8_t error_code);

/**
 * @brief Indicates current system power state.
 *
 * @retval the current power state.
 */
enum system_power_state app_pseq_systemState(void);

/**
 * @brief Indicates the current boot mode.
 *
 * @retval is the current boot mode.
 */
enum boot_config_mode pwrseq_get_boot_mode(void);

/**
 * @brief API to switch power seq.
 *
 * This is called by other file to trigger power seq change
 *
 */
void app_pseq_nextStep(enum system_power_seq_stage next, uint32_t delay);

/**
 * @brief API return current power status
 *
 * This is called to report power sequence status
 *
 */
enum system_power_state app_pseq_getPwrStatus(void);

/**
 * @brief API return current power is busy or not
 *
 * This is called to report power sequence status
 *
 */
bool app_pseq_isBusy(void);
#endif /* __APP_PSEQ_H__ */
