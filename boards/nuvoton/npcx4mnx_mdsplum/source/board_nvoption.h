/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __BOARD_NVOPTION_H__
#define __BOARD_NVOPTION_H__
#include "board_sys_config.h"
//
// NV Options declaration
//
//                          field name              , bitwidth
#define    NV_OPTIONS_DECLARATION                                       \
           DEF_NV_OPTIONS ( pwr_keepWlanPwrInS3     ,  1 );             \
           DEF_NV_OPTIONS ( pwr_keepWlanPwrInS4     ,  1 );             \
           DEF_NV_OPTIONS ( pwr_SSD0D3Cold          ,  1 );             \
           DEF_NV_OPTIONS ( pwr_SSD1D3Cold          ,  1 );             \
           DEF_NV_OPTIONS ( chg_UseNvdcMode         ,  1 );             \
           DEF_NV_OPTIONS ( pwr_WWAND3Cold          ,  1 );             \
           DEF_NV_OPTIONS ( pwr_WLAND3Cold          ,  1 );             \
           DEF_NV_OPTIONS ( chg_DcOnlyProchot       ,  1 );             \
           DEF_NV_OPTIONS ( chg_AcOnlyProchot       ,  1 );             \
           DEF_NV_OPTIONS ( chg_AcDcSwitch          ,  1 );             \
           DEF_NV_OPTIONS ( chg_AcTime              ,  8 );             \
           DEF_NV_OPTIONS ( chg_DcTime              ,  8 );             \
           DEF_NV_OPTIONS ( chg_FakeDcLevel         ,  8 );             \
           DEF_NV_OPTIONS ( chg_DcProchotLevel      ,  6 );             \
           DEF_NV_OPTIONS ( chg_ProchotAcDebounce   ,  2 );             \
           DEF_NV_OPTIONS ( chg_ProchotDuration     ,  3 );             \
           DEF_NV_OPTIONS ( chg_ProchotDcDebounce   ,  2 );             \
                                                                        \
           DEF_NV_OPTIONS ( thm_p3tLimitEn_DC       ,  1 );						  \
           DEF_NV_OPTIONS ( thm_p3tLimitEn_AC       ,  1 );						  \
           DEF_NV_OPTIONS ( thm_uploadOnboardTemp   ,  1 );             \
           DEF_NV_OPTIONS ( thm_uploadEvalCardTemp  ,  1 );             \
                                                                        \
           DEF_NV_OPTIONS ( f_wirelessManagability  ,  1 );             \
           DEF_NV_OPTIONS ( f_s0i3KbcWake           ,  1 );             \
           DEF_NV_OPTIONS ( f_TurnOnPostCode        ,  1 );             \
                                                                        \
           DEF_NV_OPTIONS ( spi_ba_Type             ,  2 );             \
                                                                        \
           DEF_NV_OPTIONS ( espi_peripheralSupport  ,  1 );             \
           DEF_NV_OPTIONS ( espi_virtialWireSupport ,  1 );             \
           DEF_NV_OPTIONS ( espi_OobSupport         ,  1 );             \
           DEF_NV_OPTIONS ( espi_FlashAccSupport    ,  1 );             \
           DEF_NV_OPTIONS ( espi_modeSupport        ,  2 );             \
           DEF_NV_OPTIONS ( espi_maxSpeed           ,  3 );             \
           DEF_NV_OPTIONS ( espi_mmio_ecram_bar     , 32 );             \
           DEF_NV_OPTIONS ( espi_mmio_test_bar      , 32 );             \
           DEF_NV_OPTIONS ( espi_ResetInS0i3        ,  1 );             \
           DEF_NV_OPTIONS ( usbc_status             ,  8 );             \
           DEF_NV_OPTIONS ( vdd_mem_a_value         ,  8 );             \
           DEF_NV_OPTIONS ( vdd_mem_b_value         ,  8 );             \
           DEF_NV_OPTIONS ( cpu_open_type           ,  8 );             \
           DEF_NV_OPTIONS ( waie_b_value            ,  1 );             \
           DEF_NV_OPTIONS ( hot_bag_enable          ,  1 );             \
           DEF_NV_OPTIONS ( thermtrip_force_g3      ,  1 )
//
// NV Options default setting
//
//                          field name              , default
#define    SET_NV_OPTIONS_TO_DEFAULT                                    \
           SET_NV_OPTIONS ( pwr_keepWlanPwrInS3     ,  _s_BoardSysConfig->f.pwr_keepWlanPwrInS3 );             \
           SET_NV_OPTIONS ( pwr_keepWlanPwrInS4     ,  _s_BoardSysConfig->f.pwr_keepWlanPwrInS4 );             \
           SET_NV_OPTIONS ( pwr_SSD0D3Cold          ,  _s_BoardSysConfig->f.pwr_SSD0D3Cold );             \
           SET_NV_OPTIONS ( pwr_SSD1D3Cold          ,  _s_BoardSysConfig->f.pwr_SSD1D3Cold );             \
           SET_NV_OPTIONS ( chg_UseNvdcMode         ,  _s_BoardSysConfig->f.chg_UseNvdcMode );             \
           SET_NV_OPTIONS ( pwr_WWAND3Cold          ,  _s_BoardSysConfig->f.pwr_WWAND3Cold );             \
           SET_NV_OPTIONS ( pwr_WLAND3Cold          ,  _s_BoardSysConfig->f.pwr_WLAND3Cold );             \
           SET_NV_OPTIONS ( chg_DcOnlyProchot       ,  _s_BoardSysConfig->f.chg_DcOnlyProchot  );  /* Need EC reset on changing */   \
           SET_NV_OPTIONS ( chg_AcOnlyProchot       ,  _s_BoardSysConfig->f.chg_AcOnlyProchot  );  /* Need EC reset on changing */   \
           SET_NV_OPTIONS ( chg_AcDcSwitch          ,  _s_BoardSysConfig->f.chg_AcDcSwitch  );             \
           SET_NV_OPTIONS ( chg_AcTime              ,  _s_BoardSysConfig->f.chg_AcTime );             \
           SET_NV_OPTIONS ( chg_DcTime              ,  _s_BoardSysConfig->f.chg_DcTime  );             \
           SET_NV_OPTIONS ( chg_FakeDcLevel         ,  _s_BoardSysConfig->f.chg_FakeDcLevel  );  /* Remain capacity 95% */ \
           SET_NV_OPTIONS ( chg_DcProchotLevel      ,  _s_BoardSysConfig->f.chg_DcProchotLevel  );  /* 0 - tie to battery level; 0x3F - fixed at 5A; 1~0x3E - high byte of the level */ \
           SET_NV_OPTIONS ( chg_ProchotAcDebounce   ,  _s_BoardSysConfig->f.chg_ProchotAcDebounce  );  /* 1ms ->0:7us / 1:100us / 2:500us / 3:1ms */ \
           SET_NV_OPTIONS ( chg_ProchotDuration     ,  _s_BoardSysConfig->f.chg_ProchotDuration );  /* 1ms */ \
           SET_NV_OPTIONS ( chg_ProchotDcDebounce   ,  _s_BoardSysConfig->f.chg_ProchotDcDebounce );  /* 100us ->0:7us / 1:100us / 2:500us / 3:1ms */  \
                                                                        \
           SET_NV_OPTIONS ( thm_p3tLimitEn_DC       ,  _s_BoardSysConfig->f.thm_p3tLimitEn_DC );						  \
           SET_NV_OPTIONS ( thm_p3tLimitEn_AC       ,  _s_BoardSysConfig->f.thm_p3tLimitEn_AC );						  \
           SET_NV_OPTIONS ( thm_uploadOnboardTemp   ,  _s_BoardSysConfig->f.thm_uploadOnboardTemp );             \
           SET_NV_OPTIONS ( thm_uploadEvalCardTemp  ,  _s_BoardSysConfig->f.thm_uploadEvalCardTemp );             \
                                                                        \
           SET_NV_OPTIONS ( f_wirelessManagability  ,  _s_BoardSysConfig->f.f_wirelessManagability  );             \
           SET_NV_OPTIONS ( f_s0i3KbcWake           ,  _s_BoardSysConfig->f.f_s0i3KbcWake  );             \
           SET_NV_OPTIONS ( f_TurnOnPostCode        ,  _s_BoardSysConfig->f.f_TurnOnPostCode  );             \
                                                                        \
           SET_NV_OPTIONS ( spi_ba_Type             ,  _s_BoardSysConfig->f.spi_ba_Type );             \
                                                                        \
           SET_NV_OPTIONS ( espi_peripheralSupport  ,  _s_BoardSysConfig->f.espi_peripheralSupport  );             \
           SET_NV_OPTIONS ( espi_virtialWireSupport ,  _s_BoardSysConfig->f.espi_virtialWireSupport );             \
           SET_NV_OPTIONS ( espi_OobSupport         ,  _s_BoardSysConfig->f.espi_OobSupport  );             \
           SET_NV_OPTIONS ( espi_FlashAccSupport    ,  _s_BoardSysConfig->f.espi_FlashAccSupport );             \
           SET_NV_OPTIONS ( espi_modeSupport        ,  _s_BoardSysConfig->f.espi_modeSupport );  /* 0-Single; 1-single/dual; 2-Single/Qual; 3-Single/dual/quad */ \
           SET_NV_OPTIONS ( espi_maxSpeed           ,  _s_BoardSysConfig->f.espi_maxSpeed  );  /* 0-20, 1-25, 2-33, 3-50, 4-66 */ \
           SET_NV_OPTIONS ( espi_mmio_ecram_bar     ,  _s_BoardSysConfig->f.espi_mmio_ecram_bar );    \
           SET_NV_OPTIONS ( espi_mmio_test_bar      ,  _s_BoardSysConfig->f.espi_mmio_test_bar );    \
           SET_NV_OPTIONS ( espi_ResetInS0i3        ,  _s_BoardSysConfig->f.espi_ResetInS0i3 );    \
           SET_NV_OPTIONS ( usbc_status             ,  _s_BoardSysConfig->f.usbc_status );  /* bit5~7: resv*/ \
					                                           /* bit2~4: 000 (not initiate), 001 (PS8828A), 010 (PS8830), 011 (KB8002), 100 (KB8001) */ \
					                                           /* bit0~1: 00 (not initiate), 01 (TV), 10 (A0 Silicon), 11 (B0 SIlicon)*/ \
                                                                        \
	       SET_NV_OPTIONS ( vdd_mem_a_value             ,  _s_BoardSysConfig->f.vdd_mem_a_value ); \
           SET_NV_OPTIONS ( vdd_mem_b_value             ,  _s_BoardSysConfig->f.vdd_mem_b_value ); \
           SET_NV_OPTIONS ( cpu_open_type               ,  _s_BoardSysConfig->f.cpu_open_type );   \
           SET_NV_OPTIONS ( waie_b_value                ,  _s_BoardSysConfig->f.waie_b_value );    \
           SET_NV_OPTIONS ( hot_bag_enable              ,  _s_BoardSysConfig->f.hot_bag_enable );  \
           SET_NV_OPTIONS ( thermtrip_force_g3          ,  _s_BoardSysConfig->f.thermtrip_force_g3 )
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////

#endif
