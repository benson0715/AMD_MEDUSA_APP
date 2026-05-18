/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *****************************************************************************
 */

#ifndef __EC_DEBUG_H__
#define __EC_DEBUG_H__

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <assert.h>
#include <soc.h>

void espi_validation_init(void);
void app_espi_oob_reset(void);

uint8_t app_espi_cfg_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t app_espi_vw_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t app_espi_vw_aggr_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t app_espi_pc_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t app_espi_oob_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t app_espi_fa_acpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

#endif // end of __EC_DEBUG_H__
