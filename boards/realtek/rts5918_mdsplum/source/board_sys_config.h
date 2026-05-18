/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * EC config-region descriptor for AMD MDS Plum on RTS5918. The struct
 * layout below is the wire-format AMD's app code uses to read/write
 * the 256-byte EC config block, so the field names and offsets must
 * match the NPCX4 Plum header byte-for-byte. The block itself is
 * generated at build time by scripts/generate_config.py.
 */

#ifndef __EC_CONFIG_REGION_H__
#define __EC_CONFIG_REGION_H__

#include <stdint.h>
#include <stdbool.h>

#define EC_CONFIG_REGION_LENGTH  0x100

#define BSYSCOF_BASE_ADDR        0xCF040
#define BSYSCOF_BUF_SIZE         256

#pragma pack(push, 1)

typedef union {
	uint8_t arr[EC_CONFIG_REGION_LENGTH];
	struct {
		uint8_t  ECLLogEnable;
		uint8_t  ECLogLevel;
		uint8_t  ECShellEnable;
		uint8_t  ECSleepEnable;
		uint8_t  board_sys_config_0;
		uint16_t board_sys_config_1;
		uint32_t board_sys_config_2;
		uint8_t  board_sys_config_3;
		uint16_t board_sys_config_4;
		uint32_t board_sys_config_5;
		uint8_t  board_sys_config_6;
		uint8_t  acLim_Pro_enable;
		uint32_t acLimit1_1;
		uint32_t acLimit1_2;
		uint32_t acLimit2_1;
		uint32_t acLimit2_2;
		uint32_t AcProchot_1;
		uint32_t AcProchot_2;
		uint8_t  pwr_keepWlanPwrInS3;
		uint8_t  pwr_keepWlanPwrInS4;
		uint8_t  pwr_SSD0D3Cold;
		uint8_t  pwr_SSD1D3Cold;
		uint8_t  chg_UseNvdcMode;
		uint8_t  pwr_WWAND3Cold;
		uint8_t  pwr_WLAND3Cold;
		uint8_t  chg_DcOnlyProchot;
		uint8_t  chg_AcOnlyProchot;
		uint8_t  chg_AcDcSwitch;
		uint8_t  chg_AcTime;
		uint8_t  chg_DcTime;
		uint8_t  chg_FakeDcLevel;
		uint8_t  chg_DcProchotLevel;
		uint8_t  chg_ProchotAcDebounce;
		uint8_t  chg_ProchotDuration;
		uint8_t  chg_ProchotDcDebounce;
		uint8_t  thm_p3tLimitEn_DC;
		uint8_t  thm_p3tLimitEn_AC;
		uint8_t  thm_uploadOnboardTemp;
		uint8_t  thm_uploadEvalCardTemp;
		uint8_t  f_wirelessManagability;
		uint8_t  f_s0i3KbcWake;
		uint8_t  f_TurnOnPostCode;
		uint8_t  spi_ba_Type;
		uint8_t  espi_peripheralSupport;
		uint8_t  espi_virtialWireSupport;
		uint8_t  espi_OobSupport;
		uint8_t  espi_FlashAccSupport;
		uint8_t  espi_modeSupport;
		uint8_t  espi_maxSpeed;
		uint32_t espi_mmio_ecram_bar;
		uint32_t espi_mmio_test_bar;
		uint8_t  espi_ResetInS0i3;
		uint8_t  usbc_status;
		uint8_t  vdd_mem_a_value;
		uint8_t  vdd_mem_b_value;
		uint8_t  cpu_open_type;
		uint8_t  waie_b_value;
		uint8_t  hot_bag_enable;
		uint8_t  thermtrip_force_g3;
		uint8_t  Resv[163];
		uint16_t crc16;
	} f;
} T_EC_CONFIG_REGION_FIELD;

#pragma pack(pop)

extern T_EC_CONFIG_REGION_FIELD *_s_BoardSysConfig;
int brdSysConfig_Init(void);

#endif /* __EC_CONFIG_REGION_H__ */
