/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __ACPIPLANES_H__
#define __ACPIPLANES_H__

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include <assert.h>

#define ACPIPLANES_SUPPORTED 1

#define _LOG_EC_RAM_RANGE_START           0xA0
#define _LOG_EC_RAM_RANGE_END             0xB0

#define LOGECRAM(frmt, ...)       LOG_INF(frmt, ##__VA_ARGS__)
#define LOG(frmt, ...)            LOG_INF(frmt, ##__VA_ARGS__)

typedef struct {
    uint8_t ui8Register;
    void (*pfnHandler)(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
//	uint8_t ui8Default;
} MD_ACPIPLANES_HANDLER_TABLE;

typedef uint8_t (*MD_ACPI_HYPERPLANE_LAUNCHER)(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);


#define MD_ACPI_HYPERPLANE_SELECTOR_OFFSET 0x31
#define MD_ACPI_HYPERPLANE_FIXED_PLANE_ID  0xFF

#define MD_ACPI_HYPERPLANE_REGION_START    0
#define MD_ACPI_HYPERPLANE_REGION_END      0x37

#ifndef MD_ACPI_HYPERPLANE_MAX_PLANES
#define MD_ACPI_HYPERPLANE_MAX_PLANES      25
#endif

extern uint8_t ui8AcpiSelectorUpdate;

void md_acpiplanes_init(void);
bool md_acpiplanes_register_tab(const MD_ACPIPLANES_HANDLER_TABLE * pTab, uint8_t ui8tabSize, uint8_t planeId);
bool md_acpiplanes_register_fun(MD_ACPI_HYPERPLANE_LAUNCHER pFun, uint8_t planeId);
bool md_acpiplanes_deregister(uint8_t planeId);
uint8_t md_acpiplanes_dispatcher(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

#endif // end of __ACPIPLANES_H__

