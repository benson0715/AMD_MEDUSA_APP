/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_THERMAL_H__
#define __APP_THERMAL_H__

#include <stdint.h>
#include <stdbool.h>

extern uint8_t g_p3tConfigLimit;

/* thermal init */
bool app_thermal_init( void );
/* thermal task */
void app_thermal_task( void );
/* update the P3T budget based on current status */
void app_thermal_updateP3tSetting(uint32_t p3tLimit);
/* refresh the P3T budget based on current status */
void app_thermal_refreshP3tSetting(void);
/* reset the P3T to default value */
void app_thermal_resetP3tSetting (void);
/* ACPI handler for the thermal related data */
void ACPIThermalHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
/* thermal thread */
void thermalmgmt_thread(void *p1, void *p2, void *p3);

#endif // end of __APP_THERMAL_H__

