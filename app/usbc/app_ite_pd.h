/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef APP_ITE_PD_H_
#define APP_ITE_PD_H_

#include "amd_crb_drivers_pd.h"
#include "amd_crb_drivers_ite_pd.h"
#include "system.h"

void app_ite_IntTopHalf(uint8_t pinSt, uint8_t chipID);
void app_ite_IntBottomHalf(uint8_t chipID);
pd_do_t app_ite_getHigherPortSnkPdo(void);
bool app_ite_vonderIdentify(void);
void app_ite_resetParam(uint8_t port);
void app_ite_usbcInterruptProc(uint8_t chipID, DEV_ITEX_ALERT_STATUS event);
bool app_ite_intPending(void);
void app_ite_loadInitialStatus(uint8_t port);

#endif // end of APP_ITE_PD_H_

