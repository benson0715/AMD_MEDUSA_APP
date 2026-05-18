/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef APP_REALTEK_PD_H_
#define APP_REALTEK_PD_H_

#include "amd_crb_drivers_pd.h"
#include "amd_crb_drivers_realtek_pd.h"
#include "system.h"

#define RTK_PD_INUPUT_DATA_LENGTH               (64)
#define RTK_PD_VER_OFFSET                       (0x7EF8)

/* assert interrupt event */
void app_rtk_IntTopHalf(uint8_t pinSt, uint8_t chipID);
/* pending interrupt */
bool app_rtk_intPending(void);
/* detect RTK module */
bool app_rtk_vonderIdentify(void);
/* port interrupt handler */
void app_rtk_IntBottomHalf(uint8_t chipID);
/* get the highest RDO */
pd_do_t app_rtk_getHigherPortSnkPdo(void);
/* reset the parameter */
void app_rtk_resetParam(uint8_t port);
/* interrupt handler */
void app_rtk_usbcInterruptProc(uint8_t chipID);
/* TTK chip init */
uint8_t app_rtk_init(uint32_t fwOffset, uint32_t fwSize, uint8_t chipID);

#endif // end of APP_REALTEK_PD_H_

