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
