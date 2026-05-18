/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
   
#ifndef __EC_CONFIG_REGION_H__
#define __EC_CONFIG_REGION_H__
   
#include <stdint.h>
#include <stdbool.h>
   
#define EC_CONFIG_REGION_LENGTH     0x100   
   
#define BSYSCOF_BASE_ADDR      0xCF040   
#define BSYSCOF_BUF_SIZE       256   
#pragma pack(push, 1)
   
typedef union {
uint8_t arr[EC_CONFIG_REGION_LENGTH];
  struct {
   
   uint8_t ECLLogEnable;  /* EC DEBUG LOG Enable - Enable:0x01 - Disable:0x00 */  
   uint8_t ECLogLevel;  /* EC debug logs level setting, Error=bit0,Warn=bit1,Info=bit2,Debug=bit3 */  
   uint8_t ECShellEnable;  /* EC Shell Function Enable - Enable:0x01 - Disable:0x00 */  
   uint8_t ECSleepEnable;  /* EC Sleep Function Enable - Enable:0x01 - Disable:0x00 */  
   uint8_t board_sys_config_0;  /* Invalid Instruction 1 */  
   uint16_t board_sys_config_1;  /* Invalid Instruction 2 */  
   uint32_t board_sys_config_2;  /* Invalid Instruction 3 */  
   uint8_t board_sys_config_3;  /* Invalid Instruction 4 */  
   uint16_t board_sys_config_4;  /* Invalid Instruction 5 */  
   uint32_t board_sys_config_5;  /* Invalid Instruction 6 */  
   uint8_t board_sys_config_6;  /* Invalid Instruction 7 */  
   uint8_t acLim_Pro_enable;  /* Invalid Instruction 1 */  
   uint32_t acLimit1_1;  /* Invalid Instruction 2 */  
   uint32_t acLimit1_2;  /* Invalid Instruction 3 */  
   uint32_t acLimit2_1;  /* Invalid Instruction 4 */  
   uint32_t acLimit2_2;  /* Invalid Instruction 5 */  
   uint32_t AcProchot_1;  /* Invalid Instruction 6 */  
   uint32_t AcProchot_2;  /* Invalid Instruction 7 */  
   uint8_t pwr_keepWlanPwrInS3;  /* Invalid Instruction 1 */  
   uint8_t pwr_keepWlanPwrInS4;  /* Invalid Instruction 1 */  
   uint8_t pwr_SSD0D3Cold;  /* Invalid Instruction 3 */  
   uint8_t pwr_SSD1D3Cold;  /* Invalid Instruction 3 */  
   uint8_t chg_UseNvdcMode;  /* Invalid Instruction 4 */  
   uint8_t pwr_WWAND3Cold;  /* Invalid Instruction 5 */  
   uint8_t pwr_WLAND3Cold;  /* Invalid Instruction 6 */  
   uint8_t chg_DcOnlyProchot;  /* Invalid Instruction 7 */  
   uint8_t chg_AcOnlyProchot;  /* Invalid Instruction 1 */  
   uint8_t chg_AcDcSwitch;  /* Invalid Instruction 2 */  
   uint8_t chg_AcTime;  /* Invalid Instruction 3 */  
   uint8_t chg_DcTime;  /* Invalid Instruction 4 */  
   uint8_t chg_FakeDcLevel;  /* Invalid Instruction 5 */  
   uint8_t chg_DcProchotLevel;  /* Invalid Instruction 6 */  
   uint8_t chg_ProchotAcDebounce;  /* Invalid Instruction 7 */  
   uint8_t chg_ProchotDuration;  /* Invalid Instruction 7 */  
   uint8_t chg_ProchotDcDebounce;  /* Invalid Instruction 7 */  
   uint8_t thm_p3tLimitEn_DC;  /* Invalid Instruction 2 */  
   uint8_t thm_p3tLimitEn_AC;  /* Invalid Instruction 3 */  
   uint8_t thm_uploadOnboardTemp;  /* Invalid Instruction 4 */  
   uint8_t thm_uploadEvalCardTemp;  /* Invalid Instruction 5 */  
   uint8_t f_wirelessManagability;  /* Invalid Instruction 6 */  
   uint8_t f_s0i3KbcWake;  /* Invalid Instruction 7 */  
   uint8_t f_TurnOnPostCode;  /* Invalid Instruction 5 */  
   uint8_t spi_ba_Type;  /* Invalid Instruction 6 */  
   uint8_t espi_peripheralSupport;  /* Invalid Instruction 7 */  
   uint8_t espi_virtialWireSupport;  /* Invalid Instruction 2 */  
   uint8_t espi_OobSupport;  /* Invalid Instruction 3 */  
   uint8_t espi_FlashAccSupport;  /* Invalid Instruction 4 */  
   uint8_t espi_modeSupport;  /* Invalid Instruction 5 */  
   uint8_t espi_maxSpeed;  /* Invalid Instruction 6 */  
   uint32_t espi_mmio_ecram_bar;  /* Invalid Instruction 7 */  
   uint32_t espi_mmio_test_bar;  /* Invalid Instruction 6 */  
   uint8_t espi_ResetInS0i3;  /* Invalid Instruction 7 */  
   uint8_t usbc_status;  /* Invalid Instruction 7 */  
   uint8_t vdd_mem_a_value;  /* Invalid Instruction 7 */  
   uint8_t vdd_mem_b_value;  /* Invalid Instruction 7 */  
   uint8_t cpu_open_type;  /* Invalid Instruction 7 */  
   uint8_t waie_b_value;  /* Invalid Instruction 7 */  
   uint8_t hot_bag_enable;  /* Invalid Instruction 7 */  
   uint8_t s5_alw_enable;  /* Invalid Instruction 7 */  
   uint8_t thermtrip_force_g3;  /* Invalid Instruction 7 */  
   uint8_t Resv[162];   /* Reserve region */
   uint16_t crc16;   /* 2 bytes for CRC16 */
   
  } f;
} T_EC_CONFIG_REGION_FIELD;
   
#pragma pack(pop)
   
extern T_EC_CONFIG_REGION_FIELD *_s_BoardSysConfig; 
extern int brdSysConfig_Init(void);   
#endif // end of __EC_CONFIG_REGION_H__