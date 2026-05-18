/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gpioAutoGen.h"

void __autoGen_initECGPIO(void)
{
	/* TODO(realtek-schematic): regenerate from Plum-on-RTS5918
	 * BGA→pin map and invoke gpio_configure_array() with the
	 * resulting struct gpio_ec_config table.
	 */
}

void Board_Gpio_AcpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	ARG_UNUSED(isRead);
	ARG_UNUSED(ui8Idx);
	if (pui8Data) {
		*pui8Data = 0;
	}
	/* TODO(realtek-schematic): once gpioAutoGen.{c,h} are regenerated
	 * from Plum data, replace this stub with the per-pin dispatch
	 * the NPCX4 autogen emits.
	 */
}
