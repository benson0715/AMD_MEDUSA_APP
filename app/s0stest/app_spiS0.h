/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __APP_SPIS0_H__
#define __APP_SPIS0_H__

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <assert.h>

/* APP SPI S0 init */
void app_spiS0_Init(void);
/* APP SPI S0 ACPI handler */
uint8_t app_spiS0_AcpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);

#endif // end of __APP_SPIS0_H__
