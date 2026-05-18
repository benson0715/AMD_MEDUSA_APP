/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RTS5918 PSL shim — see psl.h.
 */

#include "psl.h"

int psl_configure(void)
{
	/* TODO(realtek-schematic): drive Plum PSL latch GPIO inactive.
	 * RTS5918 boots with all pads tri-state so leaving this empty
	 * is safe for first-light bring-up.
	 */
	return 0;
}

void psl_out_inactive(void)
{
	/* TODO(realtek-schematic): same as above; will need the actual
	 * PSL_OUT pin from the schematic.
	 */
}

uint8_t psl_get_input(void)
{
	/* TODO(realtek-schematic): read the PSL wake input pin. Returning
	 * 0 here means "no wake reason" which leaves the power-state
	 * machine in its default cold-boot path.
	 */
	return 0;
}
