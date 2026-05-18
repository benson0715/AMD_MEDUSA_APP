/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_VRCFG_H__
#define __BOARD_VRCFG_H__

#include <stdint.h>
#include <stdbool.h>
#include "board_id.h"
#include "dev_mp2815_SVI3.h"

#define VR_CFG_DEBUG_VERSION  (0x4583)

extern DEV_SVI3_CPMPRESSED g_svi3VrCfg[];

uint32_t board_vrCfg_brdId2TabId(uint8_t portIdx);
uint16_t board_vrCfg_getRegSetting(uint32_t tabId, uint8_t pageId, uint8_t reg);
uint16_t board_vrCfg_getTabVersion(uint32_t tabId);
void     board_vrCfg_exitLowPwrMode(void);
void     board_vrCfg_setupVddCore(void);
bool     board_vrCfg_vr_init(void);
bool     board_vrCfg_cfgUpdate(uint32_t spiOffset, uint8_t configId, bool baseconfig);
BID_MDS_CPU_TYPE board_vrCfg_CpuType(void);
uint16_t board_vrCfg_getTabSku(uint32_t tabId);
int      board_vrCfg_saveCpuType(BID_MDS_CPU_TYPE cpuType);
int      board_vrCfg_loadCpuType(BID_MDS_CPU_TYPE *cpuType);

#define MAX_VR_CONFIGS 5
typedef struct {
	uint32_t dataHeader;
	uint32_t baseConfig;
	uint32_t Config[MAX_VR_CONFIGS];
} VR_HEADER_FIELDS;

#endif /* __BOARD_VRCFG_H__ */
