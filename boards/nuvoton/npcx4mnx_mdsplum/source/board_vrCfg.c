/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "board_vrCfg.h"
#include "postcode_led_hub.h"
#include <assert.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <dev_rrv686xx.h>
#include "amd_crb_drivers_spiFlash.h"
#include "f_romSig.h"
#include "f_nv_options.h"

LOG_MODULE_REGISTER(vrcfg, CONFIG_VR_CONFIG_LOG_LEVEL);
K_TIMER_DEFINE(vrReadyTimer,NULL,NULL);

#define BOARD_VR_DUMP_ENABLE      (0)
#define CPU_TYPE_FLASH_ADDR       (0x5000)   /* Internal flash address for CPU type storage */
#define CPU_TYPE_DATA_SIZE        (4)       /* CPU type data structure size: 1 byte data + 1 byte CRC + 2 bytes reserved */

extern BID_MDS_CPU_TYPE g_ui8CpuType;

static bool _s_isInVrLowPowerMode = false;
VR_HEADER vr_header0;

DEV_SVI3_CPMPRESSED g_svi3VrCfg[] = {

  { .reg = 0x00, .table = 0x03F, .page = 0x1, .val =    0x00 },    // T 0000,0000,0011,1111, P 0,0001, PAGE
  { .reg = 0x00, .table = 0x03F, .page = 0x2, .val =    0x01 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0x00, .table = 0x03F, .page = 0x4, .val =    0x02 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0x00, .table = 0x03F, .page = 0x8, .val =    0x03 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0x00, .table = 0x03F, .page = 0x10, .val =    0x04 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0x01, .table = 0x03F, .page = 0xF, .val =    0x80 },    // T 0000,0000,0011,1111, P 0,1111, OPERATION
  { .reg = 0x02, .table = 0x03F, .page = 0xF, .val =    0x1F },    // T 0000,0000,0011,1111, P 0,1111, ON_OFF_CONFIG
  { .reg = 0x10, .table = 0x03F, .page = 0x1, .val =    0x00 },    // T 0000,0000,0011,1111, P 0,0001, WRITE_PROTECT
  { .reg = 0x1B, .table = 0x03F, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,0001, SMBALERT_MASK
  { .reg = 0x21, .table = 0x03F, .page = 0xF, .val =  0x00A0 },    // T 0000,0000,0011,1111, P 0,1111, VOUT_COMMAND
  { .reg = 0x22, .table = 0x003, .page = 0x5, .val =  0x2222 },    // T 0000,0000,0000,0011, P 0,0101, MFR_VOUT_TRIM
  { .reg = 0x22, .table = 0x024, .page = 0x4, .val =  0x2222 },    // T 0000,0000,0010,0100, P 0,0100, ...
  { .reg = 0x22, .table = 0x027, .page = 0x8, .val =  0x0222 },    // T 0000,0000,0010,0111, P 0,1000, ...
  { .reg = 0x22, .table = 0x03F, .page = 0x2, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0x22, .table = 0x03C, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0011,1100, P 0,0001, ...
  { .reg = 0x22, .table = 0x018, .page = 0xC, .val =  0x0000 },    // T 0000,0000,0001,1000, P 0,1100, ...
  { .reg = 0x23, .table = 0x03F, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1111, VOUT_CAL_OFFSET
  { .reg = 0x24, .table = 0x03F, .page = 0xF, .val =  0x004E },    // T 0000,0000,0011,1111, P 0,1111, VOUT_MAX
  { .reg = 0x25, .table = 0x03F, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1111, VOUT_MARGIN_HIGH
  { .reg = 0x26, .table = 0x03F, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1111, VOUT_MARGIN_LOW
  { .reg = 0x28, .table = 0x03F, .page = 0xF, .val =  0x0028 },    // T 0000,0000,0011,1111, P 0,1111, VOUT_DROOP
  { .reg = 0x2B, .table = 0x03F, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1111, VOUT_MIN
  { .reg = 0x35, .table = 0x03F, .page = 0x1, .val =  0x0026 },    // T 0000,0000,0011,1111, P 0,0001, VIN_ON
  { .reg = 0x35, .table = 0x03F, .page = 0x2, .val =  0x0018 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0x36, .table = 0x03F, .page = 0x1, .val =  0x0022 },    // T 0000,0000,0011,1111, P 0,0001, VIN_OFF
  { .reg = 0x36, .table = 0x03F, .page = 0x2, .val =  0x0014 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0x38, .table = 0x03F, .page = 0xF, .val =  0x00CD },    // T 0000,0000,0011,1111, P 0,1111, IOUT_CAL_GAIN
  { .reg = 0x39, .table = 0x027, .page = 0x1, .val =  0x0004 },    // T 0000,0000,0010,0111, P 0,0001, IOUT_CAL_OFFSET
  { .reg = 0x39, .table = 0x03F, .page = 0xE, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1110, ...
  { .reg = 0x39, .table = 0x018, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0001,1000, P 0,0001, ...
  { .reg = 0x46, .table = 0x001, .page = 0x1, .val =  0x0078 },    // T 0000,0000,0000,0001, P 0,0001, OCP_THRESHOLD
  { .reg = 0x46, .table = 0x00A, .page = 0x1, .val =  0x006E },    // T 0000,0000,0000,1010, P 0,0001, ...
  { .reg = 0x46, .table = 0x014, .page = 0x1, .val =  0x005F },    // T 0000,0000,0001,0100, P 0,0001, ...
  { .reg = 0x46, .table = 0x020, .page = 0x1, .val =  0x0026 },    // T 0000,0000,0010,0000, P 0,0001, ...
  { .reg = 0x46, .table = 0x03F, .page = 0x2, .val =  0x00A0 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0x46, .table = 0x03F, .page = 0x4, .val =  0x00B4 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0x46, .table = 0x03F, .page = 0x8, .val =  0x0050 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0x47, .table = 0x03F, .page = 0xF, .val =    0x02 },    // T 0000,0000,0011,1111, P 0,1111, OCP_FAULT_DELAY
  { .reg = 0x4A, .table = 0x001, .page = 0x1, .val =  0x006E },    // T 0000,0000,0000,0001, P 0,0001, OCWARN_THRESHOLD
  { .reg = 0x4A, .table = 0x00A, .page = 0x1, .val =  0x0064 },    // T 0000,0000,0000,1010, P 0,0001, ...
  { .reg = 0x4A, .table = 0x014, .page = 0x1, .val =  0x0055 },    // T 0000,0000,0001,0100, P 0,0001, ...
  { .reg = 0x4A, .table = 0x020, .page = 0x1, .val =  0x0021 },    // T 0000,0000,0010,0000, P 0,0001, ...
  { .reg = 0x4A, .table = 0x03F, .page = 0x2, .val =  0x0078 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0x4A, .table = 0x03F, .page = 0x4, .val =  0x00A0 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0x4A, .table = 0x03F, .page = 0x8, .val =  0x0048 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0x4F, .table = 0x03F, .page = 0xF, .val =  0x00A5 },    // T 0000,0000,0011,1111, P 0,1111, OTP_THD
  { .reg = 0x51, .table = 0x018, .page = 0xF, .val =  0x0096 },    // T 0000,0000,0001,1000, P 0,1111, VRHOT_THD
  { .reg = 0x51, .table = 0x027, .page = 0xF, .val =  0x009B },    // T 0000,0000,0010,0111, P 0,1111, ...
  { .reg = 0x55, .table = 0x03F, .page = 0x1, .val =  0xC024 },    // T 0000,0000,0011,1111, P 0,0001, VIN_OV_UV_SET
  { .reg = 0x55, .table = 0x03F, .page = 0x2, .val =  0x3C24 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0x60, .table = 0x03F, .page = 0xF, .val =  0x0003 },    // T 0000,0000,0011,1111, P 0,1111, TON_DELAY
  { .reg = 0x64, .table = 0x03F, .page = 0xF, .val =  0x0003 },    // T 0000,0000,0011,1111, P 0,1111, TOFF_DELAY
  { .reg = 0x99, .table = 0x03F, .page = 0x1, .val =  0x0002 },    // T 0000,0000,0011,1111, P 0,0001, MFR_ID
  { .reg = 0x9B, .table = 0x001, .page = 0x1, .val =  0x0011 },    // T 0000,0000,0000,0001, P 0,0001, PRODUCT_DATA_CODE
  { .reg = 0x9B, .table = 0x002, .page = 0x1, .val =  0x0012 },    // T 0000,0000,0000,0010, P 0,0001, ...
  { .reg = 0x9B, .table = 0x004, .page = 0x1, .val =  0x0013 },    // T 0000,0000,0000,0100, P 0,0001, ...
  { .reg = 0x9B, .table = 0x008, .page = 0x1, .val =  0x0014 },    // T 0000,0000,0000,1000, P 0,0001, ...
  { .reg = 0x9B, .table = 0x010, .page = 0x1, .val =  0x0015 },    // T 0000,0000,0001,0000, P 0,0001, ...
  { .reg = 0x9B, .table = 0x020, .page = 0x1, .val =  0x0016 },    // T 0000,0000,0010,0000, P 0,0001, ...
  { .reg = 0xB1, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, 
  { .reg = 0xB2, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xB3, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xB4, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xB5, .table = 0x03F, .page = 0x10, .val =  0x0300 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xB6, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xB7, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xB8, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xB9, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xBA, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xBB, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xBC, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xC4, .table = 0x018, .page = 0x8, .val =  0x0000 },    // T 0000,0000,0001,1000, P 0,1000, MFR_SVI3_VERSION_SCALE
  { .reg = 0xC4, .table = 0x027, .page = 0x8, .val =  0x0505 },    // T 0000,0000,0010,0111, P 0,1000, ...
  { .reg = 0xC4, .table = 0x03F, .page = 0x1, .val =  0x03A4 },    // T 0000,0000,0011,1111, P 0,0001, ...
  { .reg = 0xC4, .table = 0x03F, .page = 0x2, .val =  0x0043 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xC4, .table = 0x03F, .page = 0x4, .val =  0x0037 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xC5, .table = 0x03F, .page = 0x3, .val =  0x1502 },    // T 0000,0000,0011,1111, P 0,0011, MFR_SVI3_ID
  { .reg = 0xC5, .table = 0x03F, .page = 0xC, .val =  0x1500 },    // T 0000,0000,0011,1111, P 0,1100, ...
  { .reg = 0xC6, .table = 0x018, .page = 0x1, .val =  0x42AB },    // T 0000,0000,0001,1000, P 0,0001, MFR_SVI3_VID_SET
  { .reg = 0xC6, .table = 0x018, .page = 0x8, .val =  0x4265 },    // T 0000,0000,0001,1000, P 0,1000, ...
  { .reg = 0xC6, .table = 0x027, .page = 0xD, .val =  0x4200 },    // T 0000,0000,0010,0111, P 0,1101, ...
  { .reg = 0xC6, .table = 0x018, .page = 0x4, .val =  0x4200 },    // T 0000,0000,0001,1000, P 0,0100, ...
  { .reg = 0xC6, .table = 0x03F, .page = 0x2, .val =  0x4283 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xC7, .table = 0x018, .page = 0x1, .val =  0x8A07 },    // T 0000,0000,0001,1000, P 0,0001, MFR_SVI3_SR_DECAY_LL
  { .reg = 0xC7, .table = 0x018, .page = 0xE, .val =  0x2A07 },    // T 0000,0000,0001,1000, P 0,1110, ...
  { .reg = 0xC7, .table = 0x027, .page = 0x1, .val =  0x8A13 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xC7, .table = 0x027, .page = 0xE, .val =  0x2A13 },    // T 0000,0000,0010,0111, P 0,1110, ...
  { .reg = 0xC8, .table = 0x03F, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1111, MFR_SVI3_VOUT_OFFSET_MAX
  { .reg = 0xC9, .table = 0x03F, .page = 0xF, .val =  0x6600 },    // T 0000,0000,0011,1111, P 0,1111, MFR_SVI3_VOUT_MIN_OVUV_THD
  { .reg = 0xCA, .table = 0x018, .page = 0xF, .val =  0x09C7 },    // T 0000,0000,0001,1000, P 0,1111, MFR_SVI3_TELEM
  { .reg = 0xCA, .table = 0x027, .page = 0x1, .val =  0xE9C7 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xCA, .table = 0x027, .page = 0x4, .val =  0xF9C7 },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xCA, .table = 0x027, .page = 0xA, .val =  0x19C7 },    // T 0000,0000,0010,0111, P 0,1010, ...
  { .reg = 0xCB, .table = 0x03F, .page = 0xF, .val =  0x0011 },    // T 0000,0000,0011,1111, P 0,1111, MFR_SVI3_PHASE_SHED_DEBUG
  { .reg = 0xCC, .table = 0x03F, .page = 0xF, .val =  0x8000 },    // T 0000,0000,0011,1111, P 0,1111, MFR_SVI3_DEBUG
  { .reg = 0xCD, .table = 0x03F, .page = 0xF, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1111, MFR_SVI3_IOUT_VOUT_DEBUG
  { .reg = 0xCE, .table = 0x03F, .page = 0x1F, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,1111, MFR_SVI3_GEN_PUR
  { .reg = 0xCF, .table = 0x03F, .page = 0x5, .val =  0x5300 },    // T 0000,0000,0011,1111, P 0,0101, MFR_VOUT_FIL_1CCM_OFFSET
  { .reg = 0xCF, .table = 0x03F, .page = 0xA, .val =  0x4B00 },    // T 0000,0000,0011,1111, P 0,1010, ...
  { .reg = 0xCF, .table = 0x03F, .page = 0x10, .val =  0x0000 },    // T 0000,0000,0011,1111, P 1,0000, ...
  { .reg = 0xD0, .table = 0x018, .page = 0x1, .val =  0x2202 },    // T 0000,0000,0001,1000, P 0,0001, MFR_CB_SATU_PI
  { .reg = 0xD0, .table = 0x018, .page = 0x2, .val =  0x0502 },    // T 0000,0000,0001,1000, P 0,0010, ...
  { .reg = 0xD0, .table = 0x018, .page = 0x4, .val =  0xC002 },    // T 0000,0000,0001,1000, P 0,0100, ...
  { .reg = 0xD0, .table = 0x018, .page = 0x8, .val =  0x0F02 },    // T 0000,0000,0001,1000, P 0,1000, ...
  { .reg = 0xD0, .table = 0x027, .page = 0x1, .val =  0x460A },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xD0, .table = 0x027, .page = 0x2, .val =  0x050A },    // T 0000,0000,0010,0111, P 0,0010, ...
  { .reg = 0xD0, .table = 0x027, .page = 0x4, .val =  0x800A },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xD0, .table = 0x027, .page = 0x8, .val =  0x0F0A },    // T 0000,0000,0010,0111, P 0,1000, ...
  { .reg = 0xD1, .table = 0x018, .page = 0xF, .val =  0x0520 },    // T 0000,0000,0001,1000, P 0,1111, MFR_VCAL_PI
  { .reg = 0xD1, .table = 0x027, .page = 0xF, .val =  0x05A0 },    // T 0000,0000,0010,0111, P 0,1111, ...
  { .reg = 0xD2, .table = 0x03F, .page = 0xF, .val =  0x3214 },    // T 0000,0000,0011,1111, P 0,1111, MFR_TEMP_GAIN_OFFSET
  { .reg = 0xD3, .table = 0x03F, .page = 0xF, .val =  0x8000 },    // T 0000,0000,0011,1111, P 0,1111, OCP_CAL_GAIN_OFFSET
  { .reg = 0xD4, .table = 0x018, .page = 0xF, .val =  0xBD60 },    // T 0000,0000,0001,1000, P 0,1111, MFR_RAIL_CTRL
  { .reg = 0xD4, .table = 0x027, .page = 0x1, .val =  0xBC70 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xD4, .table = 0x027, .page = 0xE, .val =  0xBC60 },    // T 0000,0000,0010,0111, P 0,1110, ...
  { .reg = 0xD5, .table = 0x03F, .page = 0xF, .val =  0x81C0 },    // T 0000,0000,0011,1111, P 0,1111, MFR_RAIL_DVID_CTRL
  { .reg = 0xD6, .table = 0x018, .page = 0xF, .val =  0x0E2C },    // T 0000,0000,0001,1000, P 0,1111, MFR_RAIL_CFG
  { .reg = 0xD6, .table = 0x027, .page = 0xF, .val =  0x062C },    // T 0000,0000,0010,0111, P 0,1111, ...
  { .reg = 0xD7, .table = 0x018, .page = 0xF, .val =  0x0C0C },    // T 0000,0000,0001,1000, P 0,1111, MFR_TRANS_CFG
  { .reg = 0xD7, .table = 0x027, .page = 0xF, .val =  0x0C8C },    // T 0000,0000,0010,0111, P 0,1111, ...
  { .reg = 0xD8, .table = 0x018, .page = 0x1, .val =  0x3620 },    // T 0000,0000,0001,1000, P 0,0001, MFR_INITIAL_ADDR
  { .reg = 0xD8, .table = 0x027, .page = 0x1, .val =  0x3420 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xD8, .table = 0x03F, .page = 0x2, .val =  0x00AF },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xD8, .table = 0x03F, .page = 0xC, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1100, ...
  { .reg = 0xD9, .table = 0x03F, .page = 0x7, .val =  0x1CA7 },    // T 0000,0000,0011,1111, P 0,0111, MFR_SW_PRD_SET
  { .reg = 0xD9, .table = 0x03F, .page = 0x8, .val =  0x14A7 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xDA, .table = 0x03F, .page = 0xF, .val =  0x54AA },    // T 0000,0000,0011,1111, P 0,1111, MFR_FREQ_DET
  { .reg = 0xDB, .table = 0x027, .page = 0x1, .val =  0x2B0F },    // T 0000,0000,0010,0111, P 0,0001, MFR_PWM_MINTIME_SET
  { .reg = 0xDB, .table = 0x03F, .page = 0x2, .val =  0x294F },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xDB, .table = 0x018, .page = 0x1, .val =  0x294F },    // T 0000,0000,0001,1000, P 0,0001, ...
  { .reg = 0xDB, .table = 0x03F, .page = 0x4, .val =  0x2948 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xDB, .table = 0x03F, .page = 0x8, .val =  0x295F },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xDC, .table = 0x001, .page = 0x1, .val =  0x630C },    // T 0000,0000,0000,0001, P 0,0001, MFR_MINOFF_TIME
  { .reg = 0xDC, .table = 0x014, .page = 0x1, .val =  0x430C },    // T 0000,0000,0001,0100, P 0,0001, ...
  { .reg = 0xDC, .table = 0x020, .page = 0x1, .val =  0x230C },    // T 0000,0000,0010,0000, P 0,0001, ...
  { .reg = 0xDC, .table = 0x03F, .page = 0x2, .val =  0x941E },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xDC, .table = 0x03F, .page = 0x4, .val =  0x530C },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xDC, .table = 0x00A, .page = 0x1, .val =  0x530C },    // T 0000,0000,0000,1010, P 0,0001, ...
  { .reg = 0xDC, .table = 0x03F, .page = 0x8, .val =  0x540F },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xDD, .table = 0x018, .page = 0x1, .val =  0xB3FF },    // T 0000,0000,0001,1000, P 0,0001, MFR_VO_COMP_MAX
  { .reg = 0xDD, .table = 0x018, .page = 0x2, .val =  0xB0FF },    // T 0000,0000,0001,1000, P 0,0010, ...
  { .reg = 0xDD, .table = 0x018, .page = 0x4, .val =  0xB1FF },    // T 0000,0000,0001,1000, P 0,0100, ...
  { .reg = 0xDD, .table = 0x018, .page = 0x8, .val =  0xB2FF },    // T 0000,0000,0001,1000, P 0,1000, ...
  { .reg = 0xDD, .table = 0x027, .page = 0x1, .val =  0xB7FF },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xDD, .table = 0x027, .page = 0x2, .val =  0xB4FF },    // T 0000,0000,0010,0111, P 0,0010, ...
  { .reg = 0xDD, .table = 0x027, .page = 0x4, .val =  0xB5FF },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xDD, .table = 0x027, .page = 0x8, .val =  0xB6FF },    // T 0000,0000,0010,0111, P 0,1000, ...
  { .reg = 0xDE, .table = 0x03F, .page = 0xF, .val =  0xD988 },    // T 0000,0000,0011,1111, P 0,1111, UCP_SET
  { .reg = 0xDF, .table = 0x018, .page = 0xF, .val =  0x0A83 },    // T 0000,0000,0001,1000, P 0,1111, MFR_DYNAMIC_CTRL
  { .reg = 0xDF, .table = 0x027, .page = 0xF, .val =  0x0A87 },    // T 0000,0000,0010,0111, P 0,1111, ...
  { .reg = 0xE0, .table = 0x03F, .page = 0xF, .val =  0x1641 },    // T 0000,0000,0011,1111, P 0,1111, UVP_OVP_APS_DELAY
  { .reg = 0xE1, .table = 0x03F, .page = 0x1, .val =  0xAF7E },    // T 0000,0000,0011,1111, P 0,0001, TRANS_DROOP
  { .reg = 0xE1, .table = 0x03F, .page = 0x2, .val =  0x9F7B },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xE1, .table = 0x03F, .page = 0x4, .val =  0xAF7B },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xE1, .table = 0x03F, .page = 0x8, .val =  0xCF7B },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xE2, .table = 0x03F, .page = 0x3, .val =  0x8066 },    // T 0000,0000,0011,1111, P 0,0011, MFR_DROOP_SET
  { .reg = 0xE2, .table = 0x03F, .page = 0xC, .val =  0x8133 },    // T 0000,0000,0011,1111, P 0,1100, ...
  { .reg = 0xE3, .table = 0x03F, .page = 0x1, .val =  0x4104 },    // T 0000,0000,0011,1111, P 0,0001, MFR_DEBUG
  { .reg = 0xE3, .table = 0x03F, .page = 0x6, .val =  0x8137 },    // T 0000,0000,0011,1111, P 0,0110, ...
  { .reg = 0xE3, .table = 0x03F, .page = 0x8, .val =  0x5137 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xE4, .table = 0x018, .page = 0x4, .val =  0x052F },    // T 0000,0000,0001,1000, P 0,0100, MFR_PROTECT_CFG
  { .reg = 0xE4, .table = 0x018, .page = 0xB, .val =  0x050F },    // T 0000,0000,0001,1000, P 0,1011, ...
  { .reg = 0xE4, .table = 0x027, .page = 0x4, .val =  0x002F },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xE4, .table = 0x027, .page = 0xB, .val =  0x000F },    // T 0000,0000,0010,0111, P 0,1011, ...
  { .reg = 0xE5, .table = 0x018, .page = 0x1, .val =  0x100A },    // T 0000,0000,0001,1000, P 0,0001, MFR_APS_HYS
  { .reg = 0xE5, .table = 0x018, .page = 0xE, .val =  0x804D },    // T 0000,0000,0001,1000, P 0,1110, ...
  { .reg = 0xE5, .table = 0x027, .page = 0x1, .val =  0x1010 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xE5, .table = 0x027, .page = 0xE, .val =  0x8080 },    // T 0000,0000,0010,0111, P 0,1110, ...
  { .reg = 0xE6, .table = 0x027, .page = 0x1, .val =  0x069F },    // T 0000,0000,0010,0111, P 0,0001, MFR_PLATFORM_SET
  { .reg = 0xE6, .table = 0x03F, .page = 0xE, .val =  0x0683 },    // T 0000,0000,0011,1111, P 0,1110, ...
  { .reg = 0xE6, .table = 0x018, .page = 0x1, .val =  0x0683 },    // T 0000,0000,0001,1000, P 0,0001, ...
  { .reg = 0xE7, .table = 0x03F, .page = 0x3, .val =  0x4400 },    // T 0000,0000,0011,1111, P 0,0011, MFR_DOWN_PLATFORM
  { .reg = 0xE7, .table = 0x03F, .page = 0xC, .val =  0x0400 },    // T 0000,0000,0011,1111, P 0,1100, ...
  { .reg = 0xE8, .table = 0x03F, .page = 0x1, .val =  0x82BC },    // T 0000,0000,0011,1111, P 0,0001, MFR_DROOP_COMP
  { .reg = 0xE8, .table = 0x03F, .page = 0x2, .val =  0x8274 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xE8, .table = 0x03F, .page = 0x4, .val =  0x82B4 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xE8, .table = 0x03F, .page = 0x8, .val =  0x827C },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xE9, .table = 0x027, .page = 0x3, .val =  0x035A },    // T 0000,0000,0010,0111, P 0,0011, MFR_SLOPE_SET_DCM
  { .reg = 0xE9, .table = 0x03F, .page = 0xC, .val =  0x0355 },    // T 0000,0000,0011,1111, P 0,1100, ...
  { .reg = 0xE9, .table = 0x018, .page = 0x3, .val =  0x0355 },    // T 0000,0000,0001,1000, P 0,0011, ...
  { .reg = 0xEA, .table = 0x03F, .page = 0x1, .val =  0x2040 },    // T 0000,0000,0011,1111, P 0,0001, MFR_SLOPE_EXT_SHD_LEVEL
  { .reg = 0xEA, .table = 0x03F, .page = 0xE, .val =  0x2020 },    // T 0000,0000,0011,1111, P 0,1110, ...
  { .reg = 0xEB, .table = 0x018, .page = 0x2, .val =  0x7446 },    // T 0000,0000,0001,1000, P 0,0010, MFR_PSI_TRIM_SLOPE_TIME
  { .reg = 0xEB, .table = 0x018, .page = 0x4, .val =  0x0446 },    // T 0000,0000,0001,1000, P 0,0100, ...
  { .reg = 0xEB, .table = 0x018, .page = 0x9, .val =  0xF446 },    // T 0000,0000,0001,1000, P 0,1001, ...
  { .reg = 0xEB, .table = 0x027, .page = 0x2, .val =  0x7444 },    // T 0000,0000,0010,0111, P 0,0010, ...
  { .reg = 0xEB, .table = 0x027, .page = 0x4, .val =  0x0444 },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xEB, .table = 0x027, .page = 0x9, .val =  0xF444 },    // T 0000,0000,0010,0111, P 0,1001, ...
  { .reg = 0xEC, .table = 0x03F, .page = 0xF, .val =  0x1016 },    // T 0000,0000,0011,1111, P 0,1111, MFR_ANA_SET
  { .reg = 0xED, .table = 0x003, .page = 0x8, .val =  0x0505 },    // T 0000,0000,0000,0011, P 0,1000, MFR_FIXED_NUM_DVID
  { .reg = 0xED, .table = 0x018, .page = 0x4, .val =  0x4006 },    // T 0000,0000,0001,1000, P 0,0100, ...
  { .reg = 0xED, .table = 0x024, .page = 0x8, .val =  0x0005 },    // T 0000,0000,0010,0100, P 0,1000, ...
  { .reg = 0xED, .table = 0x027, .page = 0x4, .val =  0x4010 },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xED, .table = 0x03F, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,0001, ...
  { .reg = 0xED, .table = 0x018, .page = 0x8, .val =  0x0000 },    // T 0000,0000,0001,1000, P 0,1000, ...
  { .reg = 0xED, .table = 0x03F, .page = 0x2, .val =  0x035A },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xEE, .table = 0x001, .page = 0x8, .val =  0x0005 },    // T 0000,0000,0000,0001, P 0,1000, MFR_CTL_CFG1
  { .reg = 0xEE, .table = 0x03E, .page = 0x8, .val =  0x0000 },    // T 0000,0000,0011,1110, P 0,1000, ...
  { .reg = 0xEE, .table = 0x03F, .page = 0x1, .val =  0x8113 },    // T 0000,0000,0011,1111, P 0,0001, ...
  { .reg = 0xEE, .table = 0x03F, .page = 0x2, .val =  0x0736 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xEE, .table = 0x03F, .page = 0x4, .val =  0x5020 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xEF, .table = 0x018, .page = 0x1, .val =  0x4087 },    // T 0000,0000,0001,1000, P 0,0001, MFR_CTL_CFG2
  { .reg = 0xEF, .table = 0x01F, .page = 0x4, .val =  0x7040 },    // T 0000,0000,0001,1111, P 0,0100, ...
  { .reg = 0xEF, .table = 0x020, .page = 0x4, .val =  0x7000 },    // T 0000,0000,0010,0000, P 0,0100, ...
  { .reg = 0xEF, .table = 0x027, .page = 0x1, .val =  0x4287 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xEF, .table = 0x03F, .page = 0x2, .val =  0x0D24 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xEF, .table = 0x03F, .page = 0x8, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF0, .table = 0x018, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0001,1000, P 0,1000, MFR_I2C_PASSWORD
  { .reg = 0xF0, .table = 0x01F, .page = 0x4, .val =  0x9060 },    // T 0000,0000,0001,1111, P 0,0100, ...
  { .reg = 0xF0, .table = 0x020, .page = 0x4, .val =  0x9000 },    // T 0000,0000,0010,0000, P 0,0100, ...
  { .reg = 0xF0, .table = 0x027, .page = 0x8, .val =  0x80F3 },    // T 0000,0000,0010,0111, P 0,1000, ...
  { .reg = 0xF0, .table = 0x03F, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,0001, ...
  { .reg = 0xF0, .table = 0x03F, .page = 0x2, .val =  0x121B },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xF1, .table = 0x018, .page = 0x1, .val =  0x6666 },    // T 0000,0000,0001,1000, P 0,0001, MFR_PSI_TRIM_1
  { .reg = 0xF1, .table = 0x027, .page = 0x1, .val =  0x4444 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xF1, .table = 0x03F, .page = 0x2, .val =  0x1916 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xF1, .table = 0x03F, .page = 0x4, .val =  0xB080 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xF1, .table = 0x03F, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF2, .table = 0x002, .page = 0x1, .val =  0x4404 },    // T 0000,0000,0000,0010, P 0,0001, MFR_PSI_TRIM_2
  { .reg = 0xF2, .table = 0x018, .page = 0x1, .val =  0x6606 },    // T 0000,0000,0001,1000, P 0,0001, ...
  { .reg = 0xF2, .table = 0x01A, .page = 0x2, .val =  0x0000 },    // T 0000,0000,0001,1010, P 0,0010, ...
  { .reg = 0xF2, .table = 0x025, .page = 0x1, .val =  0x4444 },    // T 0000,0000,0010,0101, P 0,0001, ...
  { .reg = 0xF2, .table = 0x025, .page = 0x2, .val =  0x2112 },    // T 0000,0000,0010,0101, P 0,0010, ...
  { .reg = 0xF2, .table = 0x03F, .page = 0x4, .val =  0xD0A0 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xF2, .table = 0x03F, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF3, .table = 0x018, .page = 0x1, .val =  0x8800 },    // T 0000,0000,0001,1000, P 0,0001, MFR_GATE_CLK
  { .reg = 0xF3, .table = 0x018, .page = 0x4, .val =  0x0033 },    // T 0000,0000,0001,1000, P 0,0100, ...
  { .reg = 0xF3, .table = 0x027, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xF3, .table = 0x027, .page = 0x4, .val =  0x0080 },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xF3, .table = 0x03F, .page = 0x2, .val =  0x035A },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xF3, .table = 0x03F, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF4, .table = 0x003, .page = 0x1, .val =  0x2212 },    // T 0000,0000,0000,0011, P 0,0001, OCP12_SET
  { .reg = 0xF4, .table = 0x008, .page = 0x1, .val =  0x20B3 },    // T 0000,0000,0000,1000, P 0,0001, ...
  { .reg = 0xF4, .table = 0x010, .page = 0x1, .val =  0x2235 },    // T 0000,0000,0001,0000, P 0,0001, ...
  { .reg = 0xF4, .table = 0x018, .page = 0x4, .val =  0x0033 },    // T 0000,0000,0001,1000, P 0,0100, ...
  { .reg = 0xF4, .table = 0x024, .page = 0x1, .val =  0x2417 },    // T 0000,0000,0010,0100, P 0,0001, ...
  { .reg = 0xF4, .table = 0x027, .page = 0x4, .val =  0x0080 },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xF4, .table = 0x03F, .page = 0x2, .val =  0x035A },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xF4, .table = 0x03F, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF5, .table = 0x008, .page = 0x1, .val =  0x20C1 },    // T 0000,0000,0000,1000, P 0,0001, OCP34_SET
  { .reg = 0xF5, .table = 0x013, .page = 0x1, .val =  0x2244 },    // T 0000,0000,0001,0011, P 0,0001, ...
  { .reg = 0xF5, .table = 0x018, .page = 0x2, .val =  0x035A },    // T 0000,0000,0001,1000, P 0,0010, ...
  { .reg = 0xF5, .table = 0x018, .page = 0x4, .val =  0x0033 },    // T 0000,0000,0001,1000, P 0,0100, ...
  { .reg = 0xF5, .table = 0x024, .page = 0x1, .val =  0x2448 },    // T 0000,0000,0010,0100, P 0,0001, ...
  { .reg = 0xF5, .table = 0x027, .page = 0x2, .val =  0x0355 },    // T 0000,0000,0010,0111, P 0,0010, ...
  { .reg = 0xF5, .table = 0x027, .page = 0x4, .val =  0x0080 },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xF5, .table = 0x03F, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF6, .table = 0x001, .page = 0x1, .val =  0x2244 },    // T 0000,0000,0000,0001, P 0,0001, OCP56_SET
  { .reg = 0xF6, .table = 0x008, .page = 0x1, .val =  0x1E41 },    // T 0000,0000,0000,1000, P 0,0001, ...
  { .reg = 0xF6, .table = 0x036, .page = 0x1, .val =  0x1E44 },    // T 0000,0000,0011,0110, P 0,0001, ...
  { .reg = 0xF6, .table = 0x03F, .page = 0x6, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,0110, ...
  { .reg = 0xF6, .table = 0x03F, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF7, .table = 0x007, .page = 0x2, .val =  0x1C64 },    // T 0000,0000,0000,0111, P 0,0010, OCP78_SET
  { .reg = 0xF7, .table = 0x018, .page = 0x1, .val =  0x1AB5 },    // T 0000,0000,0001,1000, P 0,0001, ...
  { .reg = 0xF7, .table = 0x018, .page = 0x2, .val =  0x1C20 },    // T 0000,0000,0001,1000, P 0,0010, ...
  { .reg = 0xF7, .table = 0x020, .page = 0x2, .val =  0x0064 },    // T 0000,0000,0010,0000, P 0,0010, ...
  { .reg = 0xF7, .table = 0x027, .page = 0x1, .val =  0x1E3C },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xF7, .table = 0x03F, .page = 0x4, .val =  0xA800 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xF7, .table = 0x03F, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF8, .table = 0x001, .page = 0x4, .val =  0x540E },    // T 0000,0000,0000,0001, P 0,0100, OCP9_SET_CS_RES
  { .reg = 0xF8, .table = 0x00B, .page = 0x2, .val =  0x1115 },    // T 0000,0000,0000,1011, P 0,0010, ...
  { .reg = 0xF8, .table = 0x014, .page = 0x2, .val =  0x0015 },    // T 0000,0000,0001,0100, P 0,0010, ...
  { .reg = 0xF8, .table = 0x020, .page = 0x2, .val =  0x0000 },    // T 0000,0000,0010,0000, P 0,0010, ...
  { .reg = 0xF8, .table = 0x03E, .page = 0x4, .val =  0x5400 },    // T 0000,0000,0011,1110, P 0,0100, ...
  { .reg = 0xF8, .table = 0x03F, .page = 0x1, .val =  0xFF97 },    // T 0000,0000,0011,1111, P 0,0001, ...
  { .reg = 0xF8, .table = 0x03F, .page = 0x8, .val =  0x40F3 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xF9, .table = 0x018, .page = 0x4, .val =  0x086F },    // T 0000,0000,0001,1000, P 0,0100, MFR_FREQ_DET_PHASE12
  { .reg = 0xF9, .table = 0x027, .page = 0x4, .val =  0x9A60 },    // T 0000,0000,0010,0111, P 0,0100, ...
  { .reg = 0xF9, .table = 0x03F, .page = 0x1, .val =  0x2A54 },    // T 0000,0000,0011,1111, P 0,0001, ...
  { .reg = 0xF9, .table = 0x03F, .page = 0x2, .val =  0x0025 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xF9, .table = 0x03F, .page = 0x8, .val =  0x5454 },    // T 0000,0000,0011,1111, P 0,1000, ...
  { .reg = 0xFA, .table = 0x018, .page = 0x8, .val =  0xA32E },    // T 0000,0000,0001,1000, P 0,1000, MFR_GATE_CLK_ANA_CS_OFFSET1
  { .reg = 0xFA, .table = 0x027, .page = 0x8, .val =  0xA320 },    // T 0000,0000,0010,0111, P 0,1000, ...
  { .reg = 0xFA, .table = 0x03F, .page = 0x1, .val =  0x2000 },    // T 0000,0000,0011,1111, P 0,0001, ...
  { .reg = 0xFA, .table = 0x03F, .page = 0x2, .val =  0xB27D },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xFA, .table = 0x03F, .page = 0x4, .val =  0x003F },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xFB, .table = 0x027, .page = 0x1, .val =  0x0100 },    // T 0000,0000,0010,0111, P 0,0001, MFR_LOAD_TRANS_SET
  { .reg = 0xFB, .table = 0x03F, .page = 0x4, .val =  0x0002 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xFB, .table = 0x03F, .page = 0xA, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1010, ...
  { .reg = 0xFB, .table = 0x018, .page = 0x1, .val =  0x0000 },    // T 0000,0000,0001,1000, P 0,0001, ...
  { .reg = 0xFC, .table = 0x027, .page = 0x8, .val =  0x00C0 },    // T 0000,0000,0010,0111, P 0,1000, MFR_DVID_APS_OFFSET
  { .reg = 0xFC, .table = 0x03F, .page = 0x1, .val =  0x001A },    // T 0000,0000,0011,1111, P 0,0001, ...
  { .reg = 0xFC, .table = 0x03F, .page = 0x2, .val =  0x00D0 },    // T 0000,0000,0011,1111, P 0,0010, ...
  { .reg = 0xFC, .table = 0x03F, .page = 0x4, .val =  0x0040 },    // T 0000,0000,0011,1111, P 0,0100, ...
  { .reg = 0xFC, .table = 0x018, .page = 0x8, .val =  0x0040 },    // T 0000,0000,0001,1000, P 0,1000, ...
  { .reg = 0xFD, .table = 0x018, .page = 0x1, .val =  0x0001 },    // T 0000,0000,0001,1000, P 0,0001, MFR_REVISION
  { .reg = 0xFD, .table = 0x027, .page = 0x1, .val =  0x0008 },    // T 0000,0000,0010,0111, P 0,0001, ...
  { .reg = 0xFD, .table = 0x03F, .page = 0xE, .val =  0x0000 },    // T 0000,0000,0011,1111, P 0,1110, ...

  { .reg = 0xFF, .table = 0x000, .page = 0x0, .val = 0xFFFF }
};

#define VR_CFG_TABLE_0     (0)      // v      MP2815GQKT-0011-rev8_80W.csv
#define VR_CFG_TABLE_1     (1)      // v      MP2815GQKT-0012-rev8_54W.csv
#define VR_CFG_TABLE_2     (2)      // v      MP2815GQKT-0013-rev8_28W.csv
#define VR_CFG_TABLE_3     (3)      // v      MP2815GQKT-0014-rev1_54W.csv
#define VR_CFG_TABLE_4     (4)      // v      MP2815GQKT-0015-rev1_28W.csv
#define VR_CFG_TABLE_5     (5)      // v      MP2815GQKT-0016-rev8_10W.csv

/**
 * @brief check current vr register to detect CPU type
 *
 * @param     void
 * @retval CPU type
 */
BID_MDS_CPU_TYPE board_vrCfg_CpuType(void)
{
	uint16_t cfgSku = dev_svi3_getSku();
	BID_MDS_CPU_TYPE cpuType;

	switch (cfgSku) {
	case 0x0011:  /* 80W */
		cpuType = BID_MDS_CPU_TYPE_80W;
		break;
	case 0x0012:  /* 45W */
		cpuType = BID_MDS_CPU_TYPE_45W;
		break;
	case 0x0013:  /* 28W */
		cpuType = BID_MDS_CPU_TYPE_28W;
		break;
	case 0x0014:  /* 45W */
		cpuType = BID_MDS_CPU_TYPE_45W;
		break;
	case 0x0015:  /* 28W */
		cpuType = BID_MDS_CPU_TYPE_28W;
		break;
	case 0x0016:  /* 10W */
		cpuType = BID_MDS_CPU_TYPE_10W;
		break;
	default:
		cpuType = BID_MDS_CPU_TYPE_80W;
		break;
	}
	return cpuType;
}

//
// This function need to be maintained manually
//
// !!NOTE!! need to ensure the conversion is correct once VR_CFG_TABLE_x macros are updated.
uint32_t board_vrCfg_brdId2TabId(uint8_t portIdx)
{
	uint32_t ret = 0xFFFFFFFF;
	// 0x0011 is ONLY USED for 80W.
	// 0x0012 is ONLY USED for 54W.
	// 0x0013 is ONLY USED for 28W.
	// 0x0014 is ONLY USED for 54W on Plum DAP 20W.
	// 0x0015 is ONLY USED for 28W on Plum DAP 20W.
	switch (g_ui8CpuType) {
		case BID_MDS_CPU_TYPE_28W:
            if (BRDID_isPlumDAP20W) {
                if (board_vrCfg_getTabSku(VR_CFG_TABLE_4) == 0x0015) //table index2 0x12
				    ret = VR_CFG_TABLE_4;
			} else {
			    if (board_vrCfg_getTabSku(VR_CFG_TABLE_2) == 0x0013) //table index1 0x13
				    ret = VR_CFG_TABLE_2;
			}
			break;
		case BID_MDS_CPU_TYPE_45W:
			if (BRDID_isPlumDAP20W) {
                if (board_vrCfg_getTabSku(VR_CFG_TABLE_3) == 0x0014) //table index2 0x12
				    ret = VR_CFG_TABLE_3;
			} else {
			    if (board_vrCfg_getTabSku(VR_CFG_TABLE_1) == 0x0012) //table index2 0x12
				    ret = VR_CFG_TABLE_1;
			}
			break;
		case BID_MDS_CPU_TYPE_80W:
			if (board_vrCfg_getTabSku(VR_CFG_TABLE_0) == 0x0011) //table index0 0x11
				ret = VR_CFG_TABLE_0;
			break;
		case BID_MDS_CPU_TYPE_10W:
			if (board_vrCfg_getTabSku(VR_CFG_TABLE_5) == 0x0016) //table index0 0x16
				ret = VR_CFG_TABLE_5;
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
 * @brief read the vr table sku
 *
 * @param     tabId:           vr table id
 * @retval vr table value.
 */
uint16_t board_vrCfg_getTabSku(uint32_t tabId)
{
	// serach from the last item
	uint32_t i = sizeof(g_svi3VrCfg) / sizeof(DEV_SVI3_CPMPRESSED);
	while (--i) {
		if (g_svi3VrCfg[i].table & (1 << tabId) && g_svi3VrCfg[i].reg == DEV_SVI3_PRODUCT_DATA_CODE && (g_svi3VrCfg[i].page & (1 << 0))) {
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
	k_timer_start(&vrReadyTimer,K_MSEC(800),K_NO_WAIT);
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
	LOG_WRN("SVI3 Port: %d CodeVER: Reg 0xFD Page1 %04X, targetVer %04X ", portIdx, cfgVer, targetVer);
	LOG_WRN("SKU: Reg 0x9B Page1 %04X, targetSKU %04X ", cfgSku, targetSku);

	if ((cfgVer != targetVer ) || (cfgSku != targetSku) ||
#if (SVRTABLE_CRCCHECK_UPDATE_ENABLE)
	        ((checksum != Targetchecksum) && (!crcFault)) ||
#endif
		(VR_CFG_DEBUG_VERSION == targetVer)) {       /* if it is DEBUG VERSION, always flash the table */

		LOG_ERR("SVI3 loading VrCfg table %d", vrCfgTableId);
		postcode_led_hub_32bit_turnOn(16);  // trun on LEDs
		postcode_led_hub_print("load-VRM"); // show warning message

		k_msleep(10);      // 10ms delay

		dev_svi3_applyCompressedTable(g_svi3VrCfg, vrCfgTableId);
        dev_svi3_verifyCompressedTable(g_svi3VrCfg, vrCfgTableId);
		dev_svi3_flushConfig();

		postcode_led_hub_32bit_reset();
		postcode_led_hub_32bit_turnOff();
		needG3 = true;

		cfgVer = dev_svi3_getCodeRev();                     			 /* the taking effect version */
		cfgSku = dev_svi3_getSku();                                      /* the taking effect sku */
        LOG_WRN("SVI3 Update finished Port: %d CodeVER: Reg 0xFD Page0 %04X, targetVer %04X", portIdx, cfgVer, targetVer);
        LOG_WRN("SKU: Reg 0x9B Page1 %04X, targetSKU %04X ", cfgSku, targetSku);
	}

	/* for debug vr cfg */
	board_vrCfg_setupVddCore();
	return needG3;
}

uint32_t g_rrv686xx_romCrcInit[8] = {0}, g_rrv686xx_chipCrcInit[8] = {0};
uint32_t g_rrv686xx_romCrc[8] = {0}, g_rrv686xx_chipCrc[8] = {0};
	
/* check CRC8 with max input buffer 256 bytes */
bool board_vrCfg_checkCrc(uint8_t* buf, uint8_t len, uint8_t targerCrc)
{
	uint8_t result;
	
	LOG_ERR("CRC Check target:%x data:", targerCrc);
#if (0)	
	for (uint8_t i = 0; i < len; i++) {
		printk("%x ",buf[i]);
	}
	printk("\n");
#endif
	/*
	 * ALG - U_CRC8_ALG_CRC8
	 */
    result = crc8(buf, len, 0x07, 0x00, false);
	
	LOG_ERR("CRC Result %x\n", result);

	if (result != targerCrc) {
		return false;
	}
	
	return true;
}

/* Check the VR configuration stored in spi flash. */
/* spiOffset: point to the vr config start */
bool board_vrCfg_cfgPreCheck(uint32_t spiOffset)
{
	uint8_t inputBuf[16];
	uint8_t data_len = 0;
	
	/* Read the first line setting */
	if (amd_crb_drivers_sFlashRead(0,spiOffset, 16, inputBuf)) {
		LOG_WRN("!!error!! fail to read SPI at 0x%06X\n", spiOffset);
		return false;
	}
	
	/***** step 3a ******/
	/* The first line must start with 0x49. Normally first 4 lines are start with 0x49 */
	/* Line1: IC_DEVICE_ID */
	/* Line2: IC_DEVICE_REV */
	/* Should not write those data to VR chip */
	if (inputBuf[0] != 0x49) {
		LOG_ERR("fail with wrong VR record type\n");
		return false;
	}
	
	/* Check the CRC for the first line */
	/* CRC calculation start from byte 2 */
	/* Format: 0x49(header type), 0x07(number of bytes not include 0x00 and 0x07), 
           0xC0(I2C address 8-bit), 0xAD(Command code), 0x49D28100 (Data to write),
           0xFA(CRC8) 
	         CRC8 is calculated with 0xC0AD49D28100*/
	data_len = inputBuf[1]; 
	if (!board_vrCfg_checkCrc(&inputBuf[2], (data_len - 1), inputBuf[data_len + 1])) {
		LOG_ERR("first line CRC error\n");
		return false;		
	}
	
	/* Check the device id code match or not */
	if (inputBuf[3] != 0xAD) { 
		LOG_ERR("device id code should be 0xAD but %x\n", inputBuf[3]);
		return false;		
	}
	
	return true;
}

/**
 * VR Configuration Version History Table (2D Array)
 * Each config ID has its own array of version history
 * Format: [configId][version_index] = {version, crc}
 */

// Config 0 (Base Config) version history
static const vr_version_entry_t vr_config0_versions[] = {
    {0x0001, 0x00F1629D},
    {0xFFFF, 0xFFFFFFFF}  // End marker
};

// Config 1 version history
static const vr_version_entry_t vr_config1_versions[] = {
    {0x0001, 0x008F3FBC},
    {0xFFFF, 0xFFFFFFFF}  // End marker
};

// Config 2 version history
static const vr_version_entry_t vr_config2_versions[] = {
    {0x0001, 0x00EC6AA5},
    {0x0002, 0x0009F72E},
    {0x0003, 0x00F584F7},
    {0x0004, 0x00D35872},
    {0x0008, 0x003BE6DF},
    {0x0009, 0x00A07A92},
    {0xFFFF, 0xFFFFFFFF}  // End marker
};

// Config 3 version history
static const vr_version_entry_t vr_config3_versions[] = {
    {0x0001, 0x00536534},
    {0x0002, 0x00B3C347},
    {0x0003, 0x004FB09E},
    {0x0004, 0x007D79A2},
    {0x0008, 0x00F12308},
    {0x0009, 0x006ABF45},
    {0xFFFF, 0xFFFFFFFF}  // End marker
};

// Config 4 version history
static const vr_version_entry_t vr_config4_versions[] = {
    {0x0001, 0x0085B855},
    {0x0002, 0x00DCF748},
    {0x0003, 0x00208491},
    {0x0004, 0x00FD1270},
    {0x0008, 0x007148DA},
    {0x0009, 0x00EAD497},
    {0xFFFF, 0xFFFFFFFF}  // End marker
};

// Array of pointers to version arrays for each config
static const vr_version_entry_t* vr_version_tables[] = {
    vr_config0_versions,  // Config 0
    vr_config1_versions,  // Config 1  
    vr_config2_versions,  // Config 2
    vr_config3_versions,  // Config 3
    vr_config4_versions   // Config 4
};

/**
 * @brief Get the latest version and CRC for a specific config ID
 *
 * @param configId Configuration ID (0-4)
 * @param latest_version Pointer to store the latest version
 * @param latest_crc Pointer to store the latest CRC
 * @return true if valid config ID and version found, false otherwise
 */
static bool board_vrCfg_getLatestVersion(uint8_t configId, uint16_t *latest_version, uint32_t *latest_crc)
{
    if (configId >= sizeof(vr_version_tables) / sizeof(vr_version_tables[0])) {
        LOG_ERR("Invalid config ID: %d", configId);
        return false;
    }
    
    const vr_version_entry_t *version_table = vr_version_tables[configId];
    uint16_t max_version = 0;
    uint32_t corresponding_crc = 0;
    
    // Find the highest version number (excluding end marker 0xFFFF)
    for (int i = 0; version_table[i].version != 0xFFFF; i++) {
        if (version_table[i].version > max_version) {
            max_version = version_table[i].version;
            corresponding_crc = version_table[i].crc;
        }
    }
    
    if (max_version == 0) {
        LOG_ERR("No valid version found for config ID: %d", configId);
        return false;
    }
    
    *latest_version = max_version;
    *latest_crc = corresponding_crc;
    
    LOG_DBG("Config %d latest version: 0x%04X, CRC: 0x%08X", configId, max_version, corresponding_crc);
    return true;
}

/* Check the VR configuration stored in spi flash. */
/* spiOffset: point to the vr config start */
bool board_vrCfg_cfgUpdate(uint32_t spiOffset, uint8_t configId, bool baseconfig)
{
	uint8_t i2c_address = 0;
	uint32_t resp = 0, crc_chip = 0, crc_rom = 0;
	bool ret = false;
	uint8_t timeout = 0;
	uint16_t mfr_revision = 0;
	uint16_t latest_version = 0;
	uint32_t latest_crc = 0;
	bool needG3 = false;
	/* get I2C address from the target vr config */
	i2c_address = dev_rrv686xx_i2cAddress(spiOffset);

	/* check if VR chip is ready */
	if (!dev_rrv686xx_ready(i2c_address)) {
		LOG_ERR("VR Chip is not Ready");
		return needG3;
	}

	/* Switch config first*/
	if (!baseconfig) {
		dev_rrv686xx_restoreCfg(i2c_address, configId);
		k_msleep(20);
		dev_rrv686xx_apply_setting_changes(i2c_address);
		k_msleep(10);
		dev_rrv686xx_set_svi3_reset_high(i2c_address);
		dev_rrv686xx_set_svi3_reset_low(i2c_address);
		k_msleep(5);
	}

	/* Read MFR revision from VR chip */
	mfr_revision = dev_rrv686xx_mfr_revision(i2c_address);
	LOG_WRN("VR MFR Revision: 0x%04X", mfr_revision);

	/* Get latest version and CRC from config table */
	if (!board_vrCfg_getLatestVersion(configId, &latest_version, &latest_crc)) {
		LOG_ERR("Failed to get latest version for config %d", configId);
		return needG3;
	}

	/* Compare MFR revision with latest version */
	if ((mfr_revision > latest_version) && (!baseconfig)) {
		LOG_DBG("VR already has latest version (0x%04X), no update needed", latest_version);
		return needG3;
	}

	LOG_WRN("VR version mismatch - Current: 0x%04X, Latest: 0x%04X", mfr_revision, latest_version);

	/* Read CRC values for comparison */
	crc_chip = dev_rrv686xx_retrieveCrc(i2c_address, configId);
	crc_rom = dev_rrv686xx_romCrc(spiOffset, configId);

	/* Compare ROM CRC with latest table CRC */
	if (crc_rom != latest_crc) {
		LOG_ERR("ROM CRC (0x%08X) doesn't match latest table CRC (0x%08X), upgrade aborted", 
			crc_rom, latest_crc);
		return needG3;
	}

	LOG_WRN("ROM CRC matches latest table CRC (0x%08X), proceeding with upgrade", latest_crc);

	/* Check if chip already has the correct CRC */
	if (crc_chip == crc_rom) {
		LOG_DBG("Chip CRC already matches ROM CRC, no update needed");
		return needG3;
	} else if (baseconfig && (crc_chip != 0)) {
		LOG_DBG("Don't update base VR");
		return needG3;
	} else {
		LOG_ERR("Chip CRC mismatch, proceeding with VR update");
	}

	/* step 1: Read Available NVM Space */
	dev_rrv686xx_nvm_space(i2c_address);

	/* Pre-check VR config */
	ret = board_vrCfg_cfgPreCheck(spiOffset);
	if (!ret) {
		LOG_DBG("VR cfg pre-check failed");
		return needG3;
	}

	/* step 2c-2h: Compare data to HEX file */
	ret = dev_rrv686xx_checkDevId(i2c_address, spiOffset);
	if (!ret) {
		LOG_DBG("VR device id check failed");
		return needG3;
	}
	ret = dev_rrv686xx_checkRevId(i2c_address, spiOffset);
	if (!ret) {
		LOG_DBG("VR device revision check failed");
		return needG3;
	}

	/* step 3: Exit low power mode*/
	dev_rrv686xx_exit_low_power_mode(i2c_address);

	/* step 4: Parse HEX File and Write to Hardware */
	ret = dev_rrv686xx_updateConfig(i2c_address, spiOffset);
	if (!ret) {
		LOG_DBG("VR update config failed");
		return needG3;
	}

	/* Step 5a : Poll PROGRAMMER_STATUS Register */
	/* Step 5b : Programming Failure */
	while (1) {
		ret = dev_rrv686xx_pollProgStatus(i2c_address);
		if (ret) {
			LOG_ERR("Programmer status pass");
			break;
		}
		/* delay 200ms if it is failed */
		k_msleep(200);
		timeout++;
		if (timeout == 20) {
			/* wait 4s without expected status drop */
			LOG_ERR("Timeout because of failed programmer status\n");
			return needG3;
		}
	}

	/* step 6a: Retrieve Device Data */
	resp = dev_rrv686xx_retrieveDeviceData(i2c_address);

	/* step 6b: Write modified data */
	uint32_t temp = resp;
	temp &= ~0x1F;
	temp += 0x09;
	LOG_ERR("temp 0x%x resp 0x%x", temp, resp);
	dev_rrv686xx_writemodifieddata(i2c_address, temp);

	/* step 7: Re-enter low power mode */
	dev_rrv686xx_enter_low_power_mode(i2c_address);
	
	/* Step 8a : Restore Config into RAM */
	ret = dev_rrv686xx_restoreCfg(i2c_address, configId);
	if (!ret) {
		LOG_ERR("restore cfg fail");
		return needG3;
	}
	k_msleep(20);
	/* Step 8b : Apply Setting Changes */
	dev_rrv686xx_apply_setting_changes(i2c_address);
	k_msleep(10);
	dev_rrv686xx_set_svi3_reset_high(i2c_address);
	dev_rrv686xx_set_svi3_reset_low(i2c_address);
	k_msleep(5);

	/* Step 8c : Read the CRC value */
	crc_chip = dev_rrv686xx_retrieveCrc(i2c_address, configId);
	crc_rom = dev_rrv686xx_romCrc(spiOffset, configId);

	if (crc_chip == crc_rom) {
		LOG_ERR("crc match");
	} else {
		LOG_ERR("crc mismatch");
		return needG3;
	}
	/* Step 9 : Restore Modified Data */
	dev_rrv686xx_writemodifieddata(i2c_address, resp);

	/* Step 10 : Configure SPS Fault Settings */
	ret = dev_rrv686xx_config_sps_fault(i2c_address, 0xF0000000);
	if (ret) {
		LOG_ERR("config sps fault error");
		return needG3;
	}

	LOG_WRN("VR update successful offset: 0x%x ConfigId: 0x%x", spiOffset, configId);
	if (baseconfig) {
		LOG_WRN("Flashed base config, trigger FORCE_G3");
		needG3 = true;
	}

	return needG3;
}

void board_vrCfg_load_header(uint32_t spiOffset);

/**
 * @brief Save CPU type to internal flash with CRC verification
 *
 * @details This function saves the CPU type value to internal flash at address 0x200.
 *          The data structure includes:
 *          - CPU type value (1 byte)
 *          - CRC8 checksum (1 byte)
 *          - Reserved bytes for alignment (2 bytes)
 *          Total: 4 bytes
 *
 * @param [in] cpuType: CPU type value to save
 * @return  0 if successful, negative error code otherwise
 */
int board_vrCfg_saveCpuType(BID_MDS_CPU_TYPE cpuType)
{
	int ret;
	uint8_t data[CPU_TYPE_DATA_SIZE];

	/* Prepare data structure */
	data[0] = (uint8_t)cpuType;
	data[2] = 0xFF;  /* Reserved */
	data[3] = 0xFF;  /* Reserved */

	/* Calculate CRC8 using WCDMA algorithm (poly=0x9B, init=0x00, refIn=true, refOut=true, xorOut=0x00) */
	data[1] = crc8(data, 1, 0x9B, 0x00, true);

	/* Write data to flash using byte write (handles erase automatically if needed) */
	ret = amd_crb_flash_byte_write(CPU_TYPE_FLASH_ADDR, sizeof(data), data);
	if (ret != 0) {
		LOG_ERR("Failed to write CPU type to internal flash at 0x%x: %d", CPU_TYPE_FLASH_ADDR, ret);
		return ret;
	}

	LOG_INF("CPU type 0x%02X saved to internal flash at 0x%x with CRC 0x%02X", 
	        data[0], CPU_TYPE_FLASH_ADDR, data[1]);
	
	return 0;
}

/**
 * @brief Load CPU type from internal flash and verify CRC
 *
 * @details This function reads the CPU type value from internal flash at address 0x200
 *          and verifies the CRC8 checksum.
 *
 * @param [out] cpuType: Pointer to store the CPU type value
 * @return  0 if successful and CRC is valid, negative error code otherwise
 */
int board_vrCfg_loadCpuType(BID_MDS_CPU_TYPE *cpuType)
{
	int ret;
	uint8_t data[CPU_TYPE_DATA_SIZE];
	uint8_t calculated_crc;

	if (cpuType == NULL) {
		LOG_ERR("Invalid CPU type pointer at 0x%x, Set to default 80W", CPU_TYPE_FLASH_ADDR);
		*cpuType = BID_MDS_CPU_TYPE_80W;
		return -EINVAL;
	}

	/* Read data from flash */
	ret = amd_crb_drivers_sFlash_int_read(CPU_TYPE_FLASH_ADDR, sizeof(data), data);
	if (ret != 0) {
		LOG_ERR("Failed to read CPU type from internal flash at 0x%x: %d, Set to default 80W", CPU_TYPE_FLASH_ADDR, ret);
		*cpuType = BID_MDS_CPU_TYPE_80W;
		return -EINVAL;
	}

	/* Calculate CRC8 using WCDMA algorithm (poly=0x9B, init=0x00, refIn=true, refOut=true, xorOut=0x00) */
	calculated_crc = crc8(data, 1, 0x9B, 0x00, true);

	/* Verify CRC */
	if (calculated_crc != data[1]) {
		LOG_ERR("CRC mismatch for CPU type: calculated 0x%02X, stored 0x%02X, Set to default 80W", 
		        calculated_crc, data[1]);
		*cpuType = BID_MDS_CPU_TYPE_80W;
		return -EIO;
	}

	*cpuType = (BID_MDS_CPU_TYPE)data[0];
	LOG_INF("CPU type 0x%02X loaded from internal flash at 0x%x, CRC verified", 
	        data[0], CPU_TYPE_FLASH_ADDR);
	return 0;
}

/**
 * @brief vr module init
 *
 * @return  true if need G3, false no need.
 */
bool board_vrCfg_vr_init(void)
{
	bool needG3 = false;
	uint32_t optionVal;
	/* update the CPU OPEN */
	GET_NV_OPTIONS(cpu_open_type, optionVal);
	if (BRDID_isPlum || BRDID_isJuniper) {
		if (optionVal == BID_MDS_CPU_TYPE_NA){
			/* If it is invalid OPEN type, update based on board id */
			if (brdId_isDM()) {
				g_ui8CpuType = brdId_CpuOpnType();
			} else {
				g_ui8CpuType = board_vrCfg_CpuType();
			}
			SET_NV_OPTIONS(cpu_open_type, g_ui8CpuType);
			LOG_DBG("Init the default OPN %d" ,g_ui8CpuType);
		} else {
			g_ui8CpuType = (0x07 & optionVal);
			LOG_DBG("Load Save OPN: %d" ,g_ui8CpuType);
		}

		needG3 = board_vrCfg_porInit(0, MP2815_I2C_ADDRESS_ID0);

		if (needG3) {
			return needG3;
		}
	}
	if (BRDID_isHickory) {
		if (optionVal == BID_MDS_CPU_TYPE_NA) {
			/* If it is invalid OPEN type, update based on board id */
			if (brdId_isDM()) {
				g_ui8CpuType = brdId_CpuOpnType();
			} else {
				board_vrCfg_loadCpuType(&g_ui8CpuType);
			}
			SET_NV_OPTIONS(cpu_open_type, g_ui8CpuType);
			LOG_DBG("Init the default OPN %d", g_ui8CpuType);
		} else {
			g_ui8CpuType = (0x07 & optionVal);
			BID_MDS_CPU_TYPE savedCpuType;
			int ret = board_vrCfg_loadCpuType(&savedCpuType);
			if ((g_ui8CpuType != savedCpuType)|| ret != 0) {
				board_vrCfg_saveCpuType(g_ui8CpuType);
			}
			LOG_DBG("Load Save OPN: %d", g_ui8CpuType);
		}
		uint32_t offset = f_romSig_getOffset(3);
		board_vrCfg_load_header(offset);
		needG3 = board_vrCfg_cfgUpdate(offset + vr_header0.f.baseConfig, 0, true);
		if (needG3) {
			return needG3;
		}
		// board_vrCfg_cfgUpdate(offset + vr_header0.f.Config[0], 0, false); //Consistent with base config, reserved
		// board_vrCfg_cfgUpdate(offset + vr_header0.f.Config[1], 1, false); //TV FP8
		switch (g_ui8CpuType) {
		case BID_MDS_CPU_TYPE_28W:
			board_vrCfg_cfgUpdate(offset + vr_header0.f.Config[4], 4, false); // 28W FP10
			break;
		case BID_MDS_CPU_TYPE_45W:
			board_vrCfg_cfgUpdate(offset + vr_header0.f.Config[3], 3, false); // 54 or 45W
			break;
		case BID_MDS_CPU_TYPE_80W:
			board_vrCfg_cfgUpdate(offset + vr_header0.f.Config[2], 2, false); // UU 80W FP10
			break;
		default:
			break;
		}
	}
	return needG3;
}


/**
 * @brief load vr binary header
 *
 * @param     slvAddr:          vr slave address
 */
void board_vrCfg_load_header(uint32_t spiOffset)
{
    memset((void *)&vr_header0, 0xFF, sizeof(vr_header0));
    
    /*check data header firstly*/
    amd_crb_drivers_sFlashRead(0, spiOffset, 4, (uint8_t *)&vr_header0.f.dataHeader);
    
    if (vr_header0.f.dataHeader == 0xaa55aa55) {
        amd_crb_drivers_sFlashRead(0, spiOffset + 4, (MAX_VR_CONFIGS + 1) * sizeof(uint32_t),
                       (uint8_t *)&vr_header0.f.baseConfig);
    }
    
    LOG_DBG("dataheader %x baseConfig %x ", 
        vr_header0.f.dataHeader, vr_header0.f.baseConfig);
    
    for (int i = 0; i < MAX_VR_CONFIGS; i++) {
        LOG_DBG("Config[%d] %x", i, vr_header0.f.Config[i]);
    }
}