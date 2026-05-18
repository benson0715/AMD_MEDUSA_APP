/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * EC sys-config region stub for AMD MDS Plum on RTS5918. The block
 * itself is generated at build time by scripts/generate_config.py;
 * here we only expose a default zero-initialised struct so the rest
 * of the app links. Replace the static buffer with a real flash-resident
 * read once the EC config region's physical placement on RTS5918 flash
 * is decided.
 */

#include "board_sys_config.h"

static T_EC_CONFIG_REGION_FIELD s_default_cfg;
T_EC_CONFIG_REGION_FIELD *_s_BoardSysConfig = &s_default_cfg;

int brdSysConfig_Init(void)
{
	return 0;
}
