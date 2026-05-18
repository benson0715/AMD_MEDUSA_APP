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
/* Check the VR configuration stored in spi flash. */
bool board_vrCfg_cfgUpdate(uint32_t spiOffset, uint8_t configId, bool baseconfig);
/* check VR SKU to know CPU type */
BID_MDS_CPU_TYPE board_vrCfg_CpuType(void);
/* return the SKU */
uint16_t board_vrCfg_getTabSku(uint32_t tabId);
/* save CPU type to internal flash with CRC verification */
int board_vrCfg_saveCpuType(BID_MDS_CPU_TYPE cpuType);
/* load CPU type from internal flash and verify CRC */
int board_vrCfg_loadCpuType(BID_MDS_CPU_TYPE *cpuType);

#define MAX_VR_CONFIGS 5
typedef struct {
    uint32_t dataHeader;
    uint32_t baseConfig;
    uint32_t Config[MAX_VR_CONFIGS];
} VR_HEADER_FIELDS;

typedef union {
    VR_HEADER_FIELDS f;
    uint8_t raw[64];
} VR_HEADER;

/**
 * @brief VR Configuration Version Entry
 */
typedef struct {
    uint16_t version;       // Version number
    uint32_t crc;           // CRC32 value
} vr_version_entry_t;

#endif // end of __BOARD_VRCFG_H__
