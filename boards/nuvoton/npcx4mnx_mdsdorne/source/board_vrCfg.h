/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __BOARD_VRCFG_H__
#define __BOARD_VRCFG_H__

#include <stdint.h>
#include <stdbool.h>
#include <board_id.h>
#include "dev_mp2815_SVI3.h"

/* special vr config version for debug and it will trigger upgarde every time power on */
#define VR_CFG_DEBUG_VERSION    (0x4583)

/* vr table */
extern DEV_SVI3_CPMPRESSED g_svi3VrCfg[];
/* vr table select based on boar id */
uint32_t board_vrCfg_brdId2TabId(uint8_t portIdx);
/* vr table get register setting */
uint16_t board_vrCfg_getRegSetting(uint32_t tabId, uint8_t pageId, uint8_t reg);
/* vr table get table version */
uint16_t board_vrCfg_getTabVersion(uint32_t tabId);
/* vr table exit low power mode */
void board_vrCfg_exitLowPwrMode(void);
/* setup the vr module VDD core power rail */
void board_vrCfg_setupVddCore(void);
/* vr table init called in the power sequence (vr module is available after S5 power rail ready) */
bool board_vrCfg_vr_init(void);
/* check VR SKU to know CPU type */
BID_MDS_CPU_TYPE board_vrCfg_CpuType(void);
/* return the SKU */
uint16_t board_vrCfg_getTabSku(uint32_t tabId);
#endif // end of __BOARD_VRCFG_H__
