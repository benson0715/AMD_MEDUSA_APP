/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NPCX4 used MIWU GIRQ wake routing; RTS5918 uses its own wake
 * controller. The KSI_GIRQ_INIT_LIST macro is preserved as an empty
 * terminator so AMD's app/peripheral_management code compiles, but
 * the actual wake-source registration must move to RTS5918's wake
 * subsystem once it's wired.
 */

#ifndef __BOARD_SLEEP_H__
#define __BOARD_SLEEP_H__

#include <zephyr/kernel.h>

#define APP_WAIT_BEFORE_EC_LOW_PWR  90

typedef enum {
	SLP_SYSTEM_LIGHT_SLEEP = 0,
	SLP_SYSTEM_HEAVY_SLEEP = 1,
} SLEEP_MODE;

/* Legacy Microchip GIRQ helper retained as a stub so any open-coded
 * reference still parses. _GIRQ_ENCODE/_isGIRQ_IDX/_GIRQ_IDX values
 * stay numerically compatible with the NPCX4 layout.
 */
#define _GIRQ_ENCODE(x)  (0xA0 | ((x) - 8))
#define _isGIRQ_IDX(x)   (0xA0 == (0xA0 & (x)))
#define _GIRQ_IDX(x)     (0x1F & (x))

#define KSI_GIRQ_INIT_LIST  \
	{0, 0}

void board_slp_Sleep(uint8_t mode, uint16_t duration);
bool board_slp_Wakeup(void);
void board_slp_disableKSxWakeup(void);
void board_slp_enableKSxWakeup(void);
void board_slp_wake_up_from_kb(void);
void npcx_power_enter_system_sleep2(int slp_mode, int wk_mode);

#endif /* __BOARD_SLEEP_H__ */
