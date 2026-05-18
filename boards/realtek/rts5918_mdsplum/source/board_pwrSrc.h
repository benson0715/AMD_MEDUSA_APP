/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_PWRSRC_H__
#define __BOARD_PWRSRC_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	BOARD_PWR_AC,
	BOARD_PWR_PD,
	BOARD_PWR_BTY,
	BOARD_PWR_AC_AND_PD
} BOARD_PWR_SRC_TYPE;

extern bool g_AcAdapterConnected;
extern bool g_PdAdapterConnected;
extern bool g_ChgAcOK;

void board_pwrSrc_clearAcCap(void);
void board_pwrSrc_clearPdCap(void);
void board_pwrSrc_powerOnReset(void);
BOARD_PWR_SRC_TYPE board_pwrSrc_getCurrentPowerSource(void);
bool board_pwrSrc_hasAcPowerSource(void);
bool board_pwrSrc_isAdapterConnected(void);
uint32_t board_pwrSrc_sysAcLimit(void);
uint32_t board_pwrSrc_PdCurLimit(void);
uint32_t board_pwrSrc_AcCurLimit(void);
uint32_t board_pwrSrc_sysPwrMaxWatt(void);
void board_pwrSrcEvent(void);
void board_pwrSrc_thread(void *p1, void *p2, void *p3);
void board_pwrSrc_thread_delay_callback(void *p1, void *p2, void *p3);
uint8_t board_pwrSrc_fwStHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data);
bool board_smtbty_ChgEnable(void);
bool board_smtbty_ChgDisable(void);
void board_pwrSrc_setACOKReference_vci(void);

#endif /* __BOARD_PWRSRC_H__ */
