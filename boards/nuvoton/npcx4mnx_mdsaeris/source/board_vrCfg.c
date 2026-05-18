/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "board_vrCfg.h"
#include <zephyr/logging/log.h>
#include <assert.h>
#include "f_nv_options.h"

#if CONFIG_FAN_RPM_CONTROL
#include "app_fanRpmCtrl.h"
#endif

LOG_MODULE_REGISTER(vrcfg, CONFIG_VR_CONFIG_LOG_LEVEL);
K_TIMER_DEFINE(vrReadyTimer, NULL, NULL);

#define BOARD_VR_DUMP_ENABLE      (0)

extern BID_MDS_CPU_TYPE g_ui8CpuType;

static bool _s_isInVrLowPowerMode = false;

DEV_SVI3_CPMPRESSED g_svi3VrCfg[] = {

  { .reg = 0x00, .table = 0x001, .page = 0x1, .val =    0x00 },    // T 0000,0000,0000,0001, P 0,0001, PAGE
  { .reg = 0x00, .table = 0x001, .page = 0x2, .val =    0x01 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0x00, .table = 0x001, .page = 0x4, .val =    0x02 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0x00, .table = 0x001, .page = 0x8, .val =    0x03 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0x00, .table = 0x001, .page = 0x10, .val =    0x04 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0x01, .table = 0x001, .page = 0xF, .val =    0x80 },    // T 0000,0000,0000,0001, P 0,1111, OPERATION
  { .reg = 0x02, .table = 0x001, .page = 0xF, .val =    0x1F },    // T 0000,0000,0000,0001, P 0,1111, ON_OFF_CONFIG
  { .reg = 0x10, .table = 0x001, .page = 0x1, .val =    0x02 },    // T 0000,0000,0000,0001, P 0,0001, WRITE_PROTECT
  { .reg = 0x1B, .table = 0x001, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,0001, SMBALERT_MASK
  { .reg = 0x21, .table = 0x001, .page = 0xF, .val =  0x00C8 },    // T 0000,0000,0000,0001, P 0,1111, VOUT_COMMAND
  { .reg = 0x22, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, MFR_VOUT_TRIM
  { .reg = 0x23, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, VOUT_CAL_OFFSET
  { .reg = 0x24, .table = 0x001, .page = 0xF, .val =  0x004E },    // T 0000,0000,0000,0001, P 0,1111, VOUT_MAX
  { .reg = 0x25, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, VOUT_MARGIN_HIGH
  { .reg = 0x26, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, VOUT_MARGIN_LOW
  { .reg = 0x28, .table = 0x001, .page = 0xF, .val =  0x0028 },    // T 0000,0000,0000,0001, P 0,1111, VOUT_DROOP
  { .reg = 0x2B, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, VOUT_MIN
  { .reg = 0x35, .table = 0x001, .page = 0x1, .val =  0x0026 },    // T 0000,0000,0000,0001, P 0,0001, VIN_ON
  { .reg = 0x35, .table = 0x001, .page = 0x2, .val =  0x0018 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0x36, .table = 0x001, .page = 0x1, .val =  0x0022 },    // T 0000,0000,0000,0001, P 0,0001, VIN_OFF
  { .reg = 0x36, .table = 0x001, .page = 0x2, .val =  0x0014 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0x38, .table = 0x001, .page = 0xF, .val =  0x00CD },    // T 0000,0000,0000,0001, P 0,1111, IOUT_CAL_GAIN
  { .reg = 0x39, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, IOUT_CAL_OFFSET
  { .reg = 0x46, .table = 0x001, .page = 0x1, .val =  0x0036 },    // T 0000,0000,0000,0001, P 0,0001, OCP_THRESHOLD
  { .reg = 0x46, .table = 0x001, .page = 0x2, .val =  0x0090 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0x46, .table = 0x001, .page = 0x4, .val =  0x00B4 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0x46, .table = 0x001, .page = 0x8, .val =  0x00A0 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0x47, .table = 0x001, .page = 0xF, .val =    0x02 },    // T 0000,0000,0000,0001, P 0,1111, OCP_FAULT_DELAY
  { .reg = 0x4A, .table = 0x001, .page = 0x1, .val =  0x0031 },    // T 0000,0000,0000,0001, P 0,0001, OCWARN_THRESHOLD
  { .reg = 0x4A, .table = 0x001, .page = 0x2, .val =  0x0080 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0x4A, .table = 0x001, .page = 0x4, .val =  0x00A8 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0x4A, .table = 0x001, .page = 0x8, .val =  0x0090 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0x4F, .table = 0x001, .page = 0xF, .val =  0x00A5 },    // T 0000,0000,0000,0001, P 0,1111, OTP_THD
  { .reg = 0x51, .table = 0x001, .page = 0xF, .val =  0x008C },    // T 0000,0000,0000,0001, P 0,1111, VRHOT_THD
  { .reg = 0x55, .table = 0x001, .page = 0x1, .val =  0x5024 },    // T 0000,0000,0000,0001, P 0,0001, VIN_OV_UV_SET
  { .reg = 0x55, .table = 0x001, .page = 0x2, .val =  0x5016 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0x60, .table = 0x001, .page = 0xF, .val =  0x0003 },    // T 0000,0000,0000,0001, P 0,1111, TON_DELAY
  { .reg = 0x64, .table = 0x001, .page = 0xF, .val =  0x0003 },    // T 0000,0000,0000,0001, P 0,1111, TOFF_DELAY
  { .reg = 0x99, .table = 0x001, .page = 0x1, .val =  0x0002 },    // T 0000,0000,0000,0001, P 0,0001, MFR_ID
  { .reg = 0x9B, .table = 0x001, .page = 0x1, .val =  0x0021 },    // T 0000,0000,0000,0001, P 0,0001, PRODUCT_DATA_CODE
  { .reg = 0xB1, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000,
  { .reg = 0xB2, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xB3, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xB4, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xB5, .table = 0x001, .page = 0x10, .val =  0x0300 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xB6, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xB7, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xB8, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xB9, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xBA, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xBB, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xBC, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xC4, .table = 0x001, .page = 0x1, .val =  0x03A4 },    // T 0000,0000,0000,0001, P 0,0001, MFR_SVI3_VERSION_SCALE
  { .reg = 0xC4, .table = 0x001, .page = 0x2, .val =  0x0043 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xC4, .table = 0x001, .page = 0x4, .val =  0x0037 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xC4, .table = 0x001, .page = 0x8, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xC5, .table = 0x001, .page = 0xF, .val =  0x1502 },    // T 0000,0000,0000,0001, P 0,1111, MFR_SVI3_ID
  { .reg = 0xC6, .table = 0x001, .page = 0x2, .val =  0x4283 },    // T 0000,0000,0000,0001, P 0,0010, MFR_SVI3_VID_SET
  { .reg = 0xC6, .table = 0x001, .page = 0xD, .val =  0x4200 },    // T 0000,0000,0000,0001, P 0,1101, ...
  { .reg = 0xC7, .table = 0x001, .page = 0x1, .val =  0x8A03 },    // T 0000,0000,0000,0001, P 0,0001, MFR_SVI3_SR_DECAY_LL
  { .reg = 0xC7, .table = 0x001, .page = 0x6, .val =  0x2A03 },    // T 0000,0000,0000,0001, P 0,0110, ...
  { .reg = 0xC7, .table = 0x001, .page = 0x8, .val =  0x0A03 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xC8, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, MFR_SVI3_VOUT_OFFSET_MAX
  { .reg = 0xC9, .table = 0x001, .page = 0xF, .val =  0x6600 },    // T 0000,0000,0000,0001, P 0,1111, MFR_SVI3_VOUT_MIN_OVUV_THD
  { .reg = 0xCA, .table = 0x001, .page = 0xF, .val =  0x0DC7 },    // T 0000,0000,0000,0001, P 0,1111, MFR_SVI3_TELEM
  { .reg = 0xCB, .table = 0x001, .page = 0xF, .val =  0x0011 },    // T 0000,0000,0000,0001, P 0,1111, MFR_SVI3_PHASE_SHED_DEBUG
  { .reg = 0xCC, .table = 0x001, .page = 0xF, .val =  0x8000 },    // T 0000,0000,0000,0001, P 0,1111, MFR_SVI3_DEBUG
  { .reg = 0xCD, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, MFR_SVI3_IOUT_VOUT_DEBUG
  { .reg = 0xCE, .table = 0x001, .page = 0x1F, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,1111, MFR_SVI3_GEN_PUR
  { .reg = 0xCF, .table = 0x001, .page = 0xF, .val =  0x5300 },    // T 0000,0000,0000,0001, P 0,1111, MFR_VOUT_FIL_1CCM_OFFSET
  { .reg = 0xCF, .table = 0x001, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0000,0001, P 1,0000, ...
  { .reg = 0xD0, .table = 0x001, .page = 0x1, .val =  0x2202 },    // T 0000,0000,0000,0001, P 0,0001, MFR_CB_SATU_PI
  { .reg = 0xD0, .table = 0x001, .page = 0x2, .val =  0x0502 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xD0, .table = 0x001, .page = 0x4, .val =  0xC002 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xD0, .table = 0x001, .page = 0x8, .val =  0x0F02 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xD1, .table = 0x001, .page = 0xF, .val =  0x0520 },    // T 0000,0000,0000,0001, P 0,1111, MFR_VCAL_PI
  { .reg = 0xD2, .table = 0x001, .page = 0xF, .val =  0xDD32 },    // T 0000,0000,0000,0001, P 0,1111, MFR_TEMP_GAIN_OFFSET
  { .reg = 0xD3, .table = 0x001, .page = 0xF, .val =  0x8000 },    // T 0000,0000,0000,0001, P 0,1111, OCP_CAL_GAIN_OFFSET
  { .reg = 0xD4, .table = 0x001, .page = 0x1, .val =  0xBD60 },    // T 0000,0000,0000,0001, P 0,0001, MFR_RAIL_CTRL
  { .reg = 0xD4, .table = 0x001, .page = 0xE, .val =  0xBC60 },    // T 0000,0000,0000,0001, P 0,1110, ...
  { .reg = 0xD5, .table = 0x001, .page = 0xF, .val =  0x81C0 },    // T 0000,0000,0000,0001, P 0,1111, MFR_RAIL_DVID_CTRL
  { .reg = 0xD6, .table = 0x001, .page = 0xF, .val =  0x0E2C },    // T 0000,0000,0000,0001, P 0,1111, MFR_RAIL_CFG
  { .reg = 0xD7, .table = 0x001, .page = 0xF, .val =  0x0C0C },    // T 0000,0000,0000,0001, P 0,1111, MFR_TRANS_CFG
  { .reg = 0xD8, .table = 0x001, .page = 0x1, .val =  0x3620 },    // T 0000,0000,0000,0001, P 0,0001, MFR_INITIAL_ADDR
  { .reg = 0xD8, .table = 0x001, .page = 0x2, .val =  0x00AB },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xD8, .table = 0x001, .page = 0xC, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1100, ...
  { .reg = 0xD9, .table = 0x001, .page = 0x2, .val =  0x18A7 },    // T 0000,0000,0000,0001, P 0,0010, MFR_SW_PRD_SET
  { .reg = 0xD9, .table = 0x001, .page = 0x5, .val =  0x1CA7 },    // T 0000,0000,0000,0001, P 0,0101, ...
  { .reg = 0xD9, .table = 0x001, .page = 0x8, .val =  0x14A7 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xDA, .table = 0x001, .page = 0xF, .val =  0x54AA },    // T 0000,0000,0000,0001, P 0,1111, MFR_FREQ_DET
  { .reg = 0xDB, .table = 0x001, .page = 0x3, .val =  0xA94F },    // T 0000,0000,0000,0001, P 0,0011, MFR_PWM_MINTIME_SET
  { .reg = 0xDB, .table = 0x001, .page = 0x4, .val =  0xA948 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xDB, .table = 0x001, .page = 0x8, .val =  0xA95F },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xDC, .table = 0x001, .page = 0x1, .val =  0x3410 },    // T 0000,0000,0000,0001, P 0,0001, MFR_MINOFF_TIME
  { .reg = 0xDC, .table = 0x001, .page = 0x2, .val =  0x9410 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xDC, .table = 0x001, .page = 0xC, .val =  0x5410 },    // T 0000,0000,0000,0001, P 0,1100, ...
  { .reg = 0xDD, .table = 0x001, .page = 0x1, .val =  0xB3FF },    // T 0000,0000,0000,0001, P 0,0001, MFR_VO_COMP_MAX
  { .reg = 0xDD, .table = 0x001, .page = 0x2, .val =  0xB0FF },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xDD, .table = 0x001, .page = 0x4, .val =  0xB1FF },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xDD, .table = 0x001, .page = 0x8, .val =  0xB2FF },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xDE, .table = 0x001, .page = 0xF, .val =  0xD314 },    // T 0000,0000,0000,0001, P 0,1111, UCP_SET
  { .reg = 0xDF, .table = 0x001, .page = 0xF, .val =  0x0A83 },    // T 0000,0000,0000,0001, P 0,1111, MFR_DYNAMIC_CTRL
  { .reg = 0xE0, .table = 0x001, .page = 0xF, .val =  0x1641 },    // T 0000,0000,0000,0001, P 0,1111, UVP_OVP_APS_DELAY
  { .reg = 0xE1, .table = 0x001, .page = 0x2, .val =  0x9F7E },    // T 0000,0000,0000,0001, P 0,0010, TRANS_DROOP
  { .reg = 0xE1, .table = 0x001, .page = 0x5, .val =  0xBF7E },    // T 0000,0000,0000,0001, P 0,0101, ...
  { .reg = 0xE1, .table = 0x001, .page = 0x8, .val =  0xDF7E },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xE2, .table = 0x001, .page = 0xF, .val =  0x8066 },    // T 0000,0000,0000,0001, P 0,1111, MFR_DROOP_SET
  { .reg = 0xE3, .table = 0x001, .page = 0x1, .val =  0x4104 },    // T 0000,0000,0000,0001, P 0,0001, MFR_DEBUG
  { .reg = 0xE3, .table = 0x001, .page = 0x6, .val =  0x8137 },    // T 0000,0000,0000,0001, P 0,0110, ...
  { .reg = 0xE3, .table = 0x001, .page = 0x8, .val =  0x4133 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xE4, .table = 0x001, .page = 0x1, .val =  0x030F },    // T 0000,0000,0000,0001, P 0,0001, MFR_PROTECT_CFG
  { .reg = 0xE4, .table = 0x001, .page = 0x4, .val =  0x002F },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xE4, .table = 0x001, .page = 0xA, .val =  0x000F },    // T 0000,0000,0000,0001, P 0,1010, ...
  { .reg = 0xE5, .table = 0x001, .page = 0x1, .val =  0x1010 },    // T 0000,0000,0000,0001, P 0,0001, MFR_APS_HYS
  { .reg = 0xE5, .table = 0x001, .page = 0x6, .val =  0x8080 },    // T 0000,0000,0000,0001, P 0,0110, ...
  { .reg = 0xE5, .table = 0x001, .page = 0x8, .val =  0x9A9A },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xE6, .table = 0x001, .page = 0xF, .val =  0x1485 },    // T 0000,0000,0000,0001, P 0,1111, MFR_PLATFORM_SET
  { .reg = 0xE7, .table = 0x001, .page = 0x3, .val =  0x2400 },    // T 0000,0000,0000,0001, P 0,0011, MFR_DOWN_PLATFORM
  { .reg = 0xE7, .table = 0x001, .page = 0xC, .val =  0x0400 },    // T 0000,0000,0000,0001, P 0,1100, ...
  { .reg = 0xE8, .table = 0x001, .page = 0x1, .val =  0x82AC },    // T 0000,0000,0000,0001, P 0,0001, MFR_DROOP_COMP
  { .reg = 0xE8, .table = 0x001, .page = 0xE, .val =  0x826C },    // T 0000,0000,0000,0001, P 0,1110, ...
  { .reg = 0xE9, .table = 0x001, .page = 0x1, .val =  0x0355 },    // T 0000,0000,0000,0001, P 0,0001, MFR_SLOPE_SET_DCM
  { .reg = 0xE9, .table = 0x001, .page = 0xE, .val =  0x0455 },    // T 0000,0000,0000,0001, P 0,1110, ...
  { .reg = 0xEA, .table = 0x001, .page = 0x1, .val =  0x2040 },    // T 0000,0000,0000,0001, P 0,0001, MFR_SLOPE_EXT_SHD_LEVEL
  { .reg = 0xEA, .table = 0x001, .page = 0xE, .val =  0x2020 },    // T 0000,0000,0000,0001, P 0,1110, ...
  { .reg = 0xEB, .table = 0x001, .page = 0x2, .val =  0x7336 },    // T 0000,0000,0000,0001, P 0,0010, MFR_PSI_TRIM_SLOPE_TIME
  { .reg = 0xEB, .table = 0x001, .page = 0x4, .val =  0x0336 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xEB, .table = 0x001, .page = 0x9, .val =  0xF336 },    // T 0000,0000,0000,0001, P 0,1001, ...
  { .reg = 0xEC, .table = 0x001, .page = 0xF, .val =  0x1016 },    // T 0000,0000,0000,0001, P 0,1111, MFR_ANA_SET
  { .reg = 0xED, .table = 0x001, .page = 0x2, .val =  0x035A },    // T 0000,0000,0000,0001, P 0,0010, MFR_FIXED_NUM_DVID
  { .reg = 0xED, .table = 0x001, .page = 0x4, .val =  0x4010 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xED, .table = 0x001, .page = 0x9, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1001, ...
  { .reg = 0xEE, .table = 0x001, .page = 0x1, .val =  0xA11B },    // T 0000,0000,0000,0001, P 0,0001, MFR_CTL_CFG1
  { .reg = 0xEE, .table = 0x001, .page = 0x2, .val =  0x0736 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xEE, .table = 0x001, .page = 0x4, .val =  0x5020 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xEE, .table = 0x001, .page = 0x8, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xEF, .table = 0x001, .page = 0x1, .val =  0x4387 },    // T 0000,0000,0000,0001, P 0,0001, MFR_CTL_CFG2
  { .reg = 0xEF, .table = 0x001, .page = 0x2, .val =  0x0D24 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xEF, .table = 0x001, .page = 0x4, .val =  0x7040 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xEF, .table = 0x001, .page = 0x8, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF0, .table = 0x001, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,0001, MFR_I2C_PASSWORD
  { .reg = 0xF0, .table = 0x001, .page = 0x2, .val =  0x121B },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF0, .table = 0x001, .page = 0x4, .val =  0x9000 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF0, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF1, .table = 0x001, .page = 0x1, .val =  0x6666 },    // T 0000,0000,0000,0001, P 0,0001, MFR_PSI_TRIM_1
  { .reg = 0xF1, .table = 0x001, .page = 0x2, .val =  0x1915 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF1, .table = 0x001, .page = 0x4, .val =  0xB080 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF1, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF2, .table = 0x001, .page = 0x1, .val =  0x6666 },    // T 0000,0000,0000,0001, P 0,0001, MFR_PSI_TRIM_2
  { .reg = 0xF2, .table = 0x001, .page = 0x2, .val =  0x2112 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF2, .table = 0x001, .page = 0x4, .val =  0xD0A0 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF2, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF3, .table = 0x001, .page = 0x1, .val =  0x8900 },    // T 0000,0000,0000,0001, P 0,0001, MFR_GATE_CLK
  { .reg = 0xF3, .table = 0x001, .page = 0x2, .val =  0x045A },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF3, .table = 0x001, .page = 0x4, .val =  0x0080 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF3, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF4, .table = 0x001, .page = 0x1, .val =  0x1AB5 },    // T 0000,0000,0000,0001, P 0,0001, OCP12_SET
  { .reg = 0xF4, .table = 0x001, .page = 0x2, .val =  0x045A },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF4, .table = 0x001, .page = 0x4, .val =  0xFF80 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF4, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF5, .table = 0x001, .page = 0x1, .val =  0x1E35 },    // T 0000,0000,0000,0001, P 0,0001, OCP34_SET
  { .reg = 0xF5, .table = 0x001, .page = 0x2, .val =  0x045A },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF5, .table = 0x001, .page = 0x4, .val =  0xFF00 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF5, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF6, .table = 0x001, .page = 0x1, .val =  0x1E3C },    // T 0000,0000,0000,0001, P 0,0001, OCP56_SET
  { .reg = 0xF6, .table = 0x001, .page = 0x2, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF6, .table = 0x001, .page = 0x4, .val =  0x4000 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF6, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF7, .table = 0x001, .page = 0x1, .val =  0xDB23 },    // T 0000,0000,0000,0001, P 0,0001, OCP78_SET
  { .reg = 0xF7, .table = 0x001, .page = 0x2, .val =  0x1C20 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF7, .table = 0x001, .page = 0x4, .val =  0xD800 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF7, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF8, .table = 0x001, .page = 0x1, .val =  0xFF97 },    // T 0000,0000,0000,0001, P 0,0001, OCP9_SET_CS_RES
  { .reg = 0xF8, .table = 0x001, .page = 0x2, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF8, .table = 0x001, .page = 0x4, .val =  0x5400 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF8, .table = 0x001, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xF9, .table = 0x001, .page = 0x1, .val =  0x2A54 },    // T 0000,0000,0000,0001, P 0,0001, MFR_FREQ_DET_PHASE12
  { .reg = 0xF9, .table = 0x001, .page = 0x2, .val =  0x0025 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xF9, .table = 0x001, .page = 0x4, .val =  0x006F },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xF9, .table = 0x001, .page = 0x8, .val =  0x5454 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xFA, .table = 0x001, .page = 0x1, .val =  0x2000 },    // T 0000,0000,0000,0001, P 0,0001, MFR_GATE_CLK_ANA_CS_OFFSET1
  { .reg = 0xFA, .table = 0x001, .page = 0x2, .val =  0xB27D },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xFA, .table = 0x001, .page = 0x4, .val =  0x003F },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xFA, .table = 0x001, .page = 0x8, .val =  0xA32E },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xFB, .table = 0x001, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1111, MFR_LOAD_TRANS_SET
  { .reg = 0xFC, .table = 0x001, .page = 0x1, .val =  0x001D },    // T 0000,0000,0000,0001, P 0,0001, MFR_DVID_APS_OFFSET
  { .reg = 0xFC, .table = 0x001, .page = 0x2, .val =  0x00D0 },    // T 0000,0000,0000,0001, P 0,0010, ...
  { .reg = 0xFC, .table = 0x001, .page = 0x4, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,0100, ...
  { .reg = 0xFC, .table = 0x001, .page = 0x8, .val =  0x0040 },    // T 0000,0000,0000,0001, P 0,1000, ...
  { .reg = 0xFD, .table = 0x001, .page = 0x1, .val =  0x0003 },    // T 0000,0000,0000,0001, P 0,0001, MFR_REVISION
  { .reg = 0xFD, .table = 0x001, .page = 0xE, .val =  0x0000 },    // T 0000,0000,0000,0001, P 0,1110, ...

  { .reg = 0xFF, .table = 0x000, .page = 0x0, .val = 0xFFFF }
};

#define VR_CFG_TABLE_0     (0)      // v      MP2815GQKT-0021-rev3.csv

/**
 * @brief convert board id to vr table id
 *
 * This function need to be maintained manually
 *
 * !!NOTE!! need to ensure the conversion is correct once VR_CFG_TABLE_x macros are updated.
 *
 *  PortIdx 0: 0x20 - 15
 *  PortIdx 1: 0x24 - 14
 *
 *  EC_TRACEDATA3 1: FP8
 *  EC_TRACEDATA3 0: FP11 - 120w or 55w
 *
 * @param     portIdx:          vr port index
 * @retval vr table id.
 */
uint32_t board_vrCfg_brdId2TabId(uint8_t portIdx)
{
	uint32_t ret = 0xFFFFFFFF;
	// 0x0012 is ONLY USED for 45W.
	// 0x0013 is ONLY USED for 28W.

	LOG_DBG("get cfg table: %x", g_ui8CpuType);

	switch (g_ui8CpuType) {
		case BID_MDS_CPU_TYPE_28W:
			ret = VR_CFG_TABLE_0;
			break;
		default:
			/**
			 * don't run further if VR is not detected.
			 */
			LOG_ERR("Cannot detect VR Cfg table.");
			dev_svi3_onFail();

			assert(0);
			break;
	}

	return ret;
}

/**
 * @brief read the vr module register value
 *
 * @param     tabId:           vr table id
 * @param     pageId:          vr table page id
 * @param     reg:             vr register
 * @retval register value.
 */
uint16_t board_vrCfg_getRegSetting(uint32_t tabId, uint8_t pageId, uint8_t reg)
{
	// serach from the last item
	for (uint32_t i = 0; g_svi3VrCfg[i].reg != 0xFF; i++) {
		if (g_svi3VrCfg[i].table & (1 << tabId) && g_svi3VrCfg[i].reg == reg && (g_svi3VrCfg[i].page & (1 << pageId))) {
			return g_svi3VrCfg[i].val;
		}
	}

	return 0xFFFF;
}

/**
 * @brief read the vr table sku
 *
 * @param     tabId:           vr table id
 * @retval vr table value.
 */
uint16_t board_vrCfg_getTabSku(uint32_t tabId)
{
	// search from the last item
	uint32_t i = sizeof(g_svi3VrCfg) / sizeof(DEV_SVI3_CPMPRESSED);
	while (--i) {
		if (g_svi3VrCfg[i].table & (1 << tabId) && g_svi3VrCfg[i].reg == DEV_SVI3_PRODUCT_DATA_CODE && (g_svi3VrCfg[i].page & (1 << 0))) {
			return g_svi3VrCfg[i].val;
		}
	}
	return 0xFFFF;
}

/**
 * @brief read the vr table version
 *
 * @param     tabId:           vr table id
 * @retval vr table value.
 */
uint16_t board_vrCfg_getTabVersion(uint32_t tabId)
{
	// serach from the last item
	uint32_t i = sizeof(g_svi3VrCfg) / sizeof(DEV_SVI3_CPMPRESSED);
	while (--i) {
		if (g_svi3VrCfg[i].table & (1 << tabId) && g_svi3VrCfg[i].reg == DEV_SVI3_CMD_CODE_REV && (g_svi3VrCfg[i].page & (1 << 0))) {
			return g_svi3VrCfg[i].val;
		}
	}
	return 0xFFFF;
}

/**
 * @brief dump the vr module all register value for debug
 *
 */
void board_vrCfg_dumpAllRegisters(void)
{
#if (BOARD_VR_DUMP_ENABLE)
	for (uint8_t reg = 1; reg < 0x90; reg ++) {
		dev_svi3_dumpOneRegister(reg);
	}
#endif
}

/**
 * @brief set up the vr module VDD core power rail
 *
 */
void board_vrCfg_setupVddCore(void)
{
	//
	// The VR I2C interface is off if it had entered low power mode
	//
	if (_s_isInVrLowPowerMode)
		return;
}

/**
 * @brief exit the low power mode
 *
 */
void board_vrCfg_exitLowPwrMode(void)
{
	_s_isInVrLowPowerMode = false;
}

/**
 * @brief vr module init
 *
 * @param     portIdx:          vr table id
 * @param     slvAddr:          vr slave address
 *
 * @return  true if need G3, false no need.
 */
bool board_vrCfg_porInit(uint8_t portIdx, uint8_t slvAddr)
{
    bool needG3 = false;
    dev_svi3_set_slave_address(slvAddr);

	/* Wait for device ready */
	k_timer_start(&vrReadyTimer, K_MSEC(800), K_NO_WAIT);
	do {
		if (dev_svi3_ready())
			break;
	} while (k_timer_status_get(&vrReadyTimer) == 0);

	if (!dev_svi3_ready()) {
		LOG_ERR("SVI3 is not ready.");
		dev_svi3_onFail();
	}

	/* for debug vr cfg */
#if BOARD_VR_DUMP_ENABLE
	board_vrCfg_dumpAllRegisters();
#endif

	/**
	 * collect info
	 */
	uint16_t cfgVer = dev_svi3_getCodeRev();                     			  /* the taking effect version */
	uint16_t cfgSku = dev_svi3_getSku();                                      /* the taking effect sku */
	uint32_t vrCfgTableId = board_vrCfg_brdId2TabId(portIdx);         	      /* calculate which table should be used from the compressed config file */
	uint16_t targetVer = board_vrCfg_getTabVersion(vrCfgTableId);  			  /* get the version in config file */
	uint16_t targetSku = board_vrCfg_getTabSku(vrCfgTableId);  			      /* get the sku in config file */
	uint16_t checksum = dev_svi3_checksumGet();
	
	LOG_WRN("SVI3 Port: %d CodeVER: Reg 0xFD Page1 %04X, targetVer %04X, CRC 0x%04x", portIdx, cfgVer, targetVer, checksum);
	LOG_WRN("SKU: Reg 0x9B Page1 %04X, targetSKU %04X ", cfgSku, targetSku);

	if ((cfgVer != targetVer ) || (cfgSku != targetSku) ||
		(VR_CFG_DEBUG_VERSION == targetVer)) {       /* if it is DEBUG VERSION, always flash the table */

		LOG_ERR("SVI3 loading VrCfg table %d", vrCfgTableId);

		k_msleep(10);      // 10ms delay

		dev_svi3_applyCompressedTable(g_svi3VrCfg, vrCfgTableId);
        dev_svi3_verifyCompressedTable(g_svi3VrCfg, vrCfgTableId);
		dev_svi3_flushConfig();

		needG3 = true;

		checksum = dev_svi3_checksumGet();

		cfgVer = dev_svi3_getCodeRev();                     			 /* the taking effect version */
		cfgSku = dev_svi3_getSku();                                      /* the taking effect sku */
		checksum = dev_svi3_checksumGet();
		LOG_WRN("SVI3 Update finished Port: %d CodeVER: Reg 0x85 Page0 %04X, targetVer %04X, CRC 0x%04x\n", portIdx, cfgVer, targetVer, checksum);
		LOG_WRN("SKU: Reg 0x8B Page1 %04X, targetSKU %04X ", cfgSku, targetSku);
	}

	/* for debug vr cfg */
	board_vrCfg_setupVddCore();
    return needG3;
}

/**
 * @brief vr module initialization entry point
 *
 * @return  true if need G3, false no need.
 */
bool board_vrCfg_vr_init(void)
{
    bool needG3 = false;

	/* update the CPU OPEN */
	g_ui8CpuType = BID_MDS_CPU_TYPE_28W;

    needG3 = board_vrCfg_porInit(0, MP2815_I2C_ADDRESS_ID0);
    return needG3;
}