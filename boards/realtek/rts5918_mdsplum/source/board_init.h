/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform-init hook declarations for AMD MDS Plum on RTS5918. These
 * are referenced from boards/board_config.h regardless of whether
 * CONFIG_PLATFORM_INIT is enabled, so the prototypes must always be
 * visible. The implementations live in board_init.c which is gated by
 * CONFIG_PLATFORM_INIT in source/CMakeLists.txt.
 */

#ifndef __BOARD_INIT_H__
#define __BOARD_INIT_H__

#include <stdint.h>
#include <stdbool.h>

extern uint16_t g_sysAcLimit1;

void board_init_platformInit(void);
void board_init_apuResetHandler(void);
void board_init_apuSleepEnterHandler(void);
void board_init_apuSleepExitHandler(void);
bool board_init_ifNeedForcePdFwUpdate(void);
void board_init_informUserForPdUpdate(void);
void board_init_pmicTunnelTrigger(void);
void board_init_apuRstTimerEnable(uint32_t ms);
void board_init_kbcRst(void);
void board_init_smartMux_V0(void);
void board_init_smartMux_V1(void);
void board_init_smartMux_V2(void);
void board_init_smartMux_V3(void);
void board_init_updateEcInfoEventTrigger(void);

#endif /* __BOARD_INIT_H__ */
