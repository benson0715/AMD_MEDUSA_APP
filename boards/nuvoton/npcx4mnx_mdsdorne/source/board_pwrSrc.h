/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __BOARD_PWRSRC_H__
#define __BOARD_PWRSRC_H__

#include <stdint.h>
#include <stdbool.h>

/* board power source type */
typedef enum {
	BOARD_PWR_PD,
	BOARD_PWR_BTY,
} BOARD_PWR_SRC_TYPE;

/* extern AC/PD status */
extern bool g_PdAdapterConnected;
extern bool g_ChgAcOK;

void board_pwrSrc_clearPdCap (void);

/* board power source init during EC power on */
void board_pwrSrc_powerOnReset(void);
/* return current main board power source */
BOARD_PWR_SRC_TYPE board_pwrSrc_getCurrentPowerSource(void);
/* if AC is the main power source */
bool board_pwrSrc_hasAcPowerSource(void);
/* if AC or PD is the main power source */
bool board_pwrSrc_isAdapterConnected (void);
/* return the charger AC limit */
uint32_t board_pwrSrc_sysAcLimit(void);
uint32_t board_pwrSrc_PdCurLimit(void);
/* return the system max power supply capability */
uint32_t board_pwrSrc_sysPwrMaxWatt(void);
/* board power source change event (for example AC plugin battery unplug) */
void board_pwrSrcEvent(void);
/* board power source handler main thread */
void board_pwrSrc_thread(void *p1, void *p2, void *p3);
void board_pwrSrc_thread_delay_callback(void *p1, void *p2, void *p3);
/* board power source status acpi handler */
uint8_t board_pwrSrc_fwStHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
/* board dual charger enable */
bool board_smtbty_ChgEnable (void);
/* board dual charger disable */
bool board_smtbty_ChgDisable (void);
/* set ACOK reference before entering vci */
void board_pwrSrc_setACOKReference_vci(void);
#endif // end of __BOARD_PWRSRC_H__
