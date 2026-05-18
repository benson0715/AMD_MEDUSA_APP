/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __APP_ACDCSWITCH_H__
#define __APP_ACDCSWITCH_H__

#include <stdint.h>
#include <stdbool.h>

/* Fake Battery remaining and full charge capacity */
#define FAKE_BATTERY_CAPACITY          3420
#define FAKE_BATTERY_FULL_CHARGE_CAP   (FAKE_BATTERY_CAPACITY - 50)
#define FAKE_BATTERY_VOLTAGE           APP_BATTERY_FULL_CHARGE_VOLTAGE
#define FAKE_BATTERY_CHARGE_CURRENT    0x0230     /*  560mA */
#define FAKE_BATTERY_DISCHARGE_CURRENT 0xFF1A     /* -230mA */

extern volatile uint8_t g_LoadfakeBatteryData;

/* AC DC switch handler */
bool app_acDcSwitchHandler(void);

#endif // __APP_ACDCSWITCH_H__

