/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Per-board hook implementations for AMD MDS Plum on RTS5918. The
 * matching Nuvoton file (mdsplum_npcx4mnx.c) ran POR init tables that
 * walked an autogen array; for RTS5918 the init flow is reduced to
 * the bare minimum needed for first-light bring-up. Real hooks
 * follow as schematic data is filled in.
 */

#include "board_config.h"

/* Mirror of `g_sysAcLimit1` declared in board_init.h. Definition has
 * to live somewhere; place it here so it's part of the per-board
 * compilation unit.
 */
uint16_t g_sysAcLimit1 = BOARD_AC_ADAPTER_WATT_MAX_DEFAULT;

/* `boot_mode_maf` is owned by app/power_sequencing/app_pseq.c (defined
 * unconditionally at line 112). Keep only the extern declaration via
 * board_config.h — defining it here too produces a link-time multiple
 * definition error.
 */

int board_init(void)
{
	/* TODO(realtek-schematic): replicate the POR init sequence from
	 * boards/nuvoton/npcx4mnx_mdsplum/source/mdsplum_npcx4mnx.c once
	 * the Plum-on-RTS5918 GPIO autogen table is in place. For now
	 * we return success so app/main.c proceeds to thread startup.
	 */
	return 0;
}

int board_suspend(void)
{
	return 0;
}

int board_resume(void)
{
	return 0;
}

void board_onApuPwrOK(void)
{
}

void board_evalCardCtrlPins(void)
{
}

void board_turnOffJtagInterface(void)
{
}

bool board_ifNeedForcePdFwUpdate(void)
{
	return false;
}

void board_turnOn_espi(void)
{
}
