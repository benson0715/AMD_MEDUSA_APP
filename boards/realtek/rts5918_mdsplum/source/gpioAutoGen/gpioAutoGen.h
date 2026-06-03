/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Empty stub for AMD's Plum_main.csv-driven GPIO autogen output. Once
 * the Plum-on-RTS5918 BGA→pin map exists, regenerate this header (and
 * the matching .c) using a Realtek-specific autogen script. Until then
 * we expose only the header guard so #include "gpioAutoGen.h" parses.
 */

#ifndef __GPIO_AUTOGEN_H__
#define __GPIO_AUTOGEN_H__

#include <stdint.h>
#include "system.h"


/* Runtime GPIO settings */
#define GPIO_RUNTIME_SWITCH_G3                                         \
    {EC_EVAL_BOMACO_EN,  GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_EVAL_SLT_PWREN,  GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_EVAL_CARD_PWREN, GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_SD_AUX_RST_N,    GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_EVAL_AUX_RST_N,  GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_DT_SLT_AUX_RST_N, GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,          0         },    \
    {EC_BLINK_N,         GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_WWAN_AUX_RST_N,  GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_WLAN_AUX_RST_N,  GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {MPM_EVENT_N,        GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_EVAL_19V_PWREN,  GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \
    {EC_LOM_AUX_RST_N,   GPIO_ACTIVE_HIGH | GPIO_INPUT | GPIO_DEFINE,           0         },    \

#if 0
#define GPIO_RUNTIME_SWITCH_S5S4                                       \
    {ESPI_EC_D0,         GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_D1,         GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_D2,         GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_D3,         GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_CS_N,       GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_CLK,        GPIO_INPUT,                                  0         },    \
    {ESPI_EC_ALERT_N,    GPIO_INPUT | GPIO_PULL_UP,                   0         },    \


#define GPIO_RUNTIME_SWITCH_S3                                         \
    {ESPI_EC_D0,         GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_D1,         GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_D2,         GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_D3,         GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_CS_N,       GPIO_INPUT | GPIO_PULL_UP,                   0         },    \
    {ESPI_EC_CLK,        GPIO_INPUT,                                  0         },    \

#endif
#define GPIO_RUNTIME_SWITCH_S0                                         \

/**
 * @brief change GPIO group status based on power sequence
 *
 * @param current power sequence status
 * @retval void
 */
void __autoGen_runtimeGpioSwitching (enum system_power_state pwr_state);

void __autoGen_initECGPIO(void);

/* AMD's app_acpi.c registers this as the ACPI GPIO read/write handler.
 * On NPCX4 Plum the auto-gen output emits this with per-pin dispatch;
 * on the RTS5918 scaffold it's a stub that simply returns zero data.
 *
 * TODO(realtek-schematic): regenerate from Plum GPIO autogen so the
 * handler maps ACPI offsets to the right (bank, pin) at runtime.
 */
extern void Board_Gpio_AcpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);

#endif /* __GPIO_AUTOGEN_H__ */
