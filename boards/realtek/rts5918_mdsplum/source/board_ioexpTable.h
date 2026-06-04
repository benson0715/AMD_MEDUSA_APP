/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __BOARD_IOEXPTABLE_H__
#define __BOARD_IOEXPTABLE_H__

#include "i2c_hub.h"

#define _SN(x)        (((x) & 0x7Ful) << 25)
#define PSN(p)        (((p) >> 25) & 0x7F)
#define ex_OD_1    ((1 << 16) | (3 << 5))  // [6:5]   2'b11 Open Drain
#define ex_OD_0    ((0 << 16) | (3 << 5))  // [16]    POR setting  (HIGH/LOW)
#define ex_O_1     ((1 << 16) | (2 << 5))  // [6:5]   2'b10 Output
#define ex_O_0     ((0 << 16) | (2 << 5))  // [16]    POR setting  (HIGH/LOW)
#define IO_EXP_FLG    (3ul << 14)            // 0 - Native; 2 - Null; 3 - IO_EXP
#define ex_IN_INTR              (1 << 5)   // [6:5]   2'b01 Input+INT_EN
#define ex_INPUT                (0 << 5)   // [6:5]   2'b00 Input

/*      ex NET NAME                    SEQ  |            | BUF_TYPE   |  I2C PORT  | I2C SPEED | SLAVE ADDR  | idx */
#define ex_HW_WWAN_RADIOn        ( _SN(  0) | IO_EXP_FLG | ex_IN_INTR | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  0 )    // U5105,  0 | Notify EC to toggle enable/disable WWAN radio, falling edge active
#define ex_HW_WIFI_RADIOn        ( _SN(  1) | IO_EXP_FLG | ex_IN_INTR | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  1 )    // U5105,  1 | Notify EC to toggle enable/disable WIFI radio, falling edge active
#define ex_DC_S5_PWRCTL          ( _SN(  2) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  2 )    // U5105,  2 | High: S5 POWER ON IN DC MODELow: S5 POWER OFF IN DC MODE(default)
#define ex_PD_FORCE_FW_UPn       ( _SN(  3) | IO_EXP_FLG | ex_IN_INTR | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  3 )    // U5105,  3 | Notify EC to force update PD FW when triggered in G3', falling edge activeNotify EC to toggle debug post code or Board basic info, falling edge active
#define ex_AC_330W_PRSNTn        ( _SN(  4) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  4 )    // U5105,  4 | Low when using 330W adaptor.
#define ex_FCH_DBG_BUS_EN        ( _SN(  5) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  5 )    // U5105,  5 | Notify EC that FCH debug mode is enabled, high active
#define ex_MEM_PMIC_PWR_GOOD     ( _SN(  6) | IO_EXP_FLG | ex_IN_INTR | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  6 )    // U5105,  6 | Notify EC that SODIMM/LPCAMM PMIC power ok, then can enable VR_EN pin
#define ex_LPCAMM_EP_GSIn_3V3    ( _SN(  7) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  7 )    // U5105,  7 | Reserved. Notity EC/BIOS that the LPCAMM GSI_L pin status. 

#define ex_PSL_MODE_EN           ( _SN(  8) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  8 )    // U5105,  8 | High: EC PSL (VCI) mode Enable (default);Low: EC PSL (VCI) mode Disable; 
#define ex_EXT_TALERTn           ( _SN(  9) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) |  9 )    // U5105,  9 | To nofity EC, an overheating event is occurred. low active
#define ex_WLAN_IO33n_IO18_SEL   ( _SN( 10) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) | 10 )    // U5105, 10 | Notify EC the M.2 WLAN card I/O level. Low --3.3V level, High -- 1.8V level
#define ex_UDC_EP_DBG_CARD_PRSNTn ( _SN( 11) | IO_EXP_FLG | ex_IN_INTR | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) | 11 )    // U5105, 11 | To notify EC, the UDC debug card is present. low active
#define ex_EC_UART_DOGL_PRSNTn   ( _SN( 12) | IO_EXP_FLG | ex_IN_INTR | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) | 12 )    // U5105, 12 | To notify EC, the UART dongle is present, low active
#define ex_WAIIE_EP_SENSOR_EN_STAn ( _SN( 13) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) | 13 )    // U5105, 13 | Reserved to notify EC, the WALLE sensor power enable from system power. 
#define ex_EVAL_NEWn_LEGACY      ( _SN( 14) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) | 14 )    // U5105, 14 | Low: Plugged EVAL card is a new card ; High: Plugged EVAL card is a legacy card;
#define ex_EC_SHARE_SAFn_MODE    ( _SN( 15) | IO_EXP_FLG | ex_INPUT   | (I2C_3 << 21) | (2 << 18) | (0x27 << 7) | 15 )    // U5105, 15 | Notify EC that APU is in SHARE/SAFS mode. High- SHARE, Low - SAFA

#define ex_EP_TPM_PWREN          ( _SN( 16) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  0 )    // U5100,  0 | EC to enable TPM power, high active
#define ex_EP_USBA_PWREN         ( _SN( 17) | IO_EXP_FLG | ex_O_1     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  1 )    // U5100,  1 | Load Switch EN for all USBA port. Enable=1 (Default); OFF=0.
#define ex_EP_CODEC_DEPOP_MUTEn  ( _SN( 18) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  2 )    // U5100,  2 | To mute codec POP noise
#define ex_EP_HDR_DEPOP_MUTEn    ( _SN( 19) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  3 )    // U5100,  3 | To mute plugged codec module POP noise
#define ex_EP_SD_AUX_PWREN       ( _SN( 20) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  4 )    // U5100,  4 | EC to enable SD controller power, high active
#define ex_EP_SD_MAIN_PWREN      ( _SN( 21) | IO_EXP_FLG | ex_O_1     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  5 )    // U5100,  5 | EC to control SD S0 domain power enable
#define ex_EP_SSD0_PWREN         ( _SN( 22) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  6 )    // U5100,  6 | EC to enable M.2 SSD0 power, high active
#define ex_EP_SSD1_PWREN         ( _SN( 23) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  7 )    // U5100,  7 | EC to enable M.2 SSD0 power, high active

#define ex_EP_RTL_ISOLATEn       ( _SN( 24) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  8 )    // U5100,  8 | Reserved for PCIE link ISOLATE.  Low active.
#define ex_EP_LOM_S5_PWREN       ( _SN( 25) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  9 )    // U5100,  9 | EC to enable LOM power, high active
#define ex_EP_WLAN_1V8_PWREN     ( _SN( 26) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 10 )    // U5100, 10 | Reserved to enable WLAN 1V8 power separately, high active
#define ex_EP_WLAN_3V3_PWREN     ( _SN( 27) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 11 )    // U5100, 11 | EC to enable WLAN power, high active; 
#define ex_EP_WLAN_RADIO_DIS     ( _SN( 28) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 12 )    // U5100, 12 | To disable WLAN's raido, high active.  High-disable; 
#define ex_EP_WLAN_WIFI_RST      ( _SN( 29) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 13 )    // U5100, 13 | To reset WLAN module, high active
#define ex_EP_BT_RADIO_DIS       ( _SN( 30) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 14 )    // U5100, 14 | To disable Bluetooth radio, high active
#define ex_EP_M2_BT_WAKE         ( _SN( 31) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 15 )    // U5100, 15 | EC to wake Blue Tooth module. high active

#define ex_EP_FPR_LOCKn          ( _SN( 32) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  0 )    // U5101,  0 | Low- Finger print enters low power state, High - for normal operation
#define ex_EP_FPR_OFFn           ( _SN( 33) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  1 )    // U5101,  1 | High to enable wake up function, low to disable wake up function
#define ex_EP_FPR_PWREN          ( _SN( 34) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  2 )    // U5101,  2 | EC to enable Fingerprint power, high active
#define ex_EP_FPR_GPIO           ( _SN( 35) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  3 )    // U5101,  3 | Reserved for Fingerprint side band control
#define ex_EP_TPAD_DISn          ( _SN( 36) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  4 )    // U5101,  4 | To disable touch pad, low active
#define ex_EP_TPAD_FW_SECn       ( _SN( 37) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  5 )    // U5101,  5 | for reprogramming the FW image on the touchpad,default low, high active
#define ex_EP_TPNL_EN            ( _SN( 38) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  6 )    // U5101,  6 | EC to enable Touch panel function, high active
#define ex_EP_TPNL_PWREN         ( _SN( 39) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  7 )    // U5101,  7 | EC to enable Touch panel power, high active

#define ex_EP_USBCAM_FW_WPn      ( _SN( 40) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  8 )    // U5101,  8 | Camera FW update write protect, low active, low for normal operation
#define ex_EP_USBCAM_PRIV        ( _SN( 41) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) |  9 )    // U5101,  9 | Reserved for USBCAM. 
#define ex_EP_USBCAM_PWREN       ( _SN( 42) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) | 10 )    // U5101, 10 | EC to enable USB CAM power, high active
#define ex_EP_WWAN_PWREN         ( _SN( 43) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) | 11 )    // U5101, 11 | EC to enable WWAN power, high active
#define ex_EP_WWAN_CARD_PWROFFn  ( _SN( 44) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) | 12 )    // U5101, 12 | To control WWAN power on/off;  High - pwr on ; Low - pwr off
#define ex_EP_WWAN_MODULE_RSTn   ( _SN( 45) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) | 13 )    // U5101, 13 | To reset WWAN module, low active
#define ex_EP_GNSS_RADIO_DISn    ( _SN( 46) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) | 14 )    // U5101, 14 | To disable WWAN's GNSS raido, low active
#define ex_EP_MAIN_RADIO_DISn    ( _SN( 47) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x22 << 7) | 15 )    // U5101, 15 | To disable WWAN's radio, low active

#define ex_EP_USBMUX_BTn_TPNL_SEL ( _SN( 48) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  0 )    // U5102,  0 | EC to select USB6 routing, low-BT, high--TPNL
#define ex_EP_USBMUX_FPRn_GBE_SEL ( _SN( 49) | IO_EXP_FLG | ex_O_1     | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  1 )    // U5102,  1 | EC to select USB5 routing, low-Fingerprint, high--GbE
#define ex_EP_WLANn_SD_SEL       ( _SN( 50) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  2 )    // U5102,  2 | EC to select PCIE routing, low- WLAN, high - SD.
#define ex_EP_WWANn_LOM_SEL      ( _SN( 51) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  3 )    // U5102,  3 | EC to select PCIE routing, low- WWAN, high - LOM
#define ex_EP_UART0_WLANn_HDR_SEL ( _SN( 52) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  4 )    // U5102,  4 | EC to select APU UART0 routing, low- WLAN, hihg - Header
#define ex_EP_AUD_SWn_HDR_SEL    ( _SN( 53) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  5 )    // U5102,  5 | EC to select Audio routing, low - SW Codec, high - Header
#define ex_EP_EVAL_SMB_APUn_EC_SEL ( _SN( 54) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  6 )    // U5102,  6 | EC to select Eval SMBus routing, low - APU SMB1, high - EC I2C1
#define ex_EP_WAIIEn_MP2_SEL     ( _SN( 55) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  7 )    // U5102,  7 | To control I2C for PWR sensor switch, low--WALLE, devices, high--APU MP2

#define ex_EP_SMB0_BUFF_EN       ( _SN( 56) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  8 )    // U5102,  8 | EC to enable SMBus0 buffer, high active
#define ex_EP_SMB1_BUFF_EN       ( _SN( 57) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) |  9 )    // U5102,  9 | EC to enable SMBus1 buffer, high active
#define ex_EP_USBC_RT_SMB_BUFF_EN ( _SN( 58) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) | 10 )    // U5102, 10 | EC to enable Re-timer I2C0/SMB1 buffer. Low- disable; high- enable for RT FW Update
#define ex_EP_TPAD_I2C_BUFF_EN   ( _SN( 59) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) | 11 )    // U5102, 11 | EC to control TPAD I2C buffer enable, high active
#define ex_EP_TPNL_I2C_BUFF_EN   ( _SN( 60) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) | 12 )    // U5102, 12 | EC to control TPNL I2C buffer enable, high active
#define ex_EP_NFC_I2C_BUFF_EN    ( _SN( 61) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) | 13 )    // U5102, 13 | EC to control NFC I2C buffer enable, high active
#define ex_EP_SFH_I2C_BUFF_GATEn ( _SN( 62) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) | 14 )    // U5102, 14 | EC control to gate SFH 3V3 route path, low active
#define ex_EP_ESP32_I2C_ENn      ( _SN( 63) | IO_EXP_FLG | ex_O_1     | (I2C_1 << 21) | (2 << 18) | (0x23 << 7) | 15 )    // U5102, 15 | EC to control E3 on I2C0 buffer enable, low active

#define ex_EP_BAT_LEDn           ( _SN( 64) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  0 )    // U5103,  0 | Battery LED
#define ex_EP_KBC_LOW_BATn       ( _SN( 65) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  1 )    // U5103,  1 | Reserved to notify APU, to enter low battary mode, low active
#define ex_EP_ADAPTER_OFF        ( _SN( 66) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  2 )    // U5103,  2 | To gate adapter power source, high active
#define ex_APU_EC_VDDCORE_HIGH   ( _SN( 67) | IO_EXP_FLG | ex_INPUT   | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  3 )    // U5103,  3 | 
#define ex_EP_SB_TSI_SEL         ( _SN( 68) | IO_EXP_FLG | ex_O_1     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  4 )    // U5103,  4 | To select SB-TSI routing. High- EC I2C0; low - ESP32
#define ex_EP_APU_PCCn           ( _SN( 69) | IO_EXP_FLG | ex_O_1     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  5 )    // U5103,  5 | Reserved for APU PCC function, low active
#define ex_EP_DP0_HPD            ( _SN( 70) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  6 )    // U5103,  6 | Reserved to drive this signal High/Low to mimic Plug-in/ Plug-out
#define ex_EP_DP1_HPD            ( _SN( 71) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  7 )    // U5103,  7 | Reserved to drive this signal High/Low to mimic Plug-in/ Plug-out

#define ex_EP_EDP_SMUX_PWM_EN    ( _SN( 72) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  8 )    // U5103,  8 | Smart MUX PWM control
#define ex_EP_EDP_SMUX_BLON      ( _SN( 73) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) |  9 )    // U5103,  9 | Smart MUX back light on
#define ex_EP_EDP_SMUX_PNL_RSTn  ( _SN( 74) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) | 10 )    // U5103, 10 | Smart MUX panel reset
#define ex_EP_EDP_SMUX_PWMMUX_SEL ( _SN( 75) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) | 11 )    // U5103, 11 | Smart MUX PWM mux control
#define ex_APU_EC_VDDCORE_LEDON  ( _SN( 76) | IO_EXP_FLG | ex_INPUT   | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) | 12 )    // U5103, 12 | Reserved for Vcore LED indicator
#define ex_EP_CODEC_AUX_MODE_EN  ( _SN( 77) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) | 13 )    // U5103, 13 | To enable Codec AUX Mode. High - AUX mode enable; Low- AUX mode disable. 
#define ex_EP_CODEC_MIC_HW_MUTE  ( _SN( 78) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) | 14 )    // U5103, 14 | To enable Codex MIC Mute.  High - MIC Mute , Low - MIC Unmute
#define ex_EP_JTAG_SELn          ( _SN( 79) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x25 << 7) | 15 )    // U5103, 15 | EC to control APU JTAG path to UDC, low active

#define ex_EP_MEM_SEL            ( _SN( 80) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  0 )    // U5104,  0 | MEM VDD power selection for different speed, high - 8533, low - 9600.  
#define ex_EP_AUD_DMICn_HDR_SEL  ( _SN( 81) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  1 )    // U5104,  1 | EC to select Audio DMIC routing, low - DMIC header,  high - I2S header
#define ex_DIS_DASHn             ( _SN( 82) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  2 )    // U5104,  2 | Reserved for DASH enable or disable,  low - Dash disable  ; high - Dash enable
#define ex_EC_EFFP_MODULE_RESET_N ( _SN( 83) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  3 )    // U5104,  3 | Reserved for eFFP module debug only, TBD
#define ex_MEM_PMIC_VIN_BULK_PWREN ( _SN( 84) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  5 )    // U5104,  5 | Reserved to enable SODIMM/LPCAMM VIN BULK power, high active. 
#define ex_MEM_PMIC_VR_EN_3V3    ( _SN( 85) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  6 )    // U5104,  6 | VR EN pin control, high active. I2C command mode when power on, VR EN pin mode in power off. 
#define ex_TPM_OBn_HDR_SEL       ( _SN( 86) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  7 )    // U5104,  7 | EC to select TPM SPI routing , low- onboard TPM,  high- TPM header

#define ex_EP_LPCAMM_VDDQ_PWREN  ( _SN( 87) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  8 )    // U5104,  8 | Reserved to enbale VDDQ power for LPCAMM, high active.
#define ex_EP_LPCAMM_VDDQ_DIS_3V3 ( _SN( 88) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) |  9 )    // U5104,  9 | Reserved to control LPCAMM VDDQ_DISABLE pin. High - disable, Low- Enable. 
#define ex_EP_LPCAMM_UNLOCK_3V3  ( _SN( 89) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) | 10 )    // U5104, 10 | EC to control LPCAMM UNLOCK pin.  High- Unlock, Low-Lock
#define ex_EP_LPCAMM_DVFSQn_3V3  ( _SN( 90) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) | 11 )    // U5104, 11 | Reserved to control LPCAMM DVFSQ_L pin.  Low active
#define ex_EP_MEM_1V0_PWREN      ( _SN( 91) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) | 12 )    // U5104, 12 | EC to enable MEM I3C buffer 1.0V power. High active
#define ex_EP_MEM_I2C_EN         ( _SN( 92) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) | 13 )    // U5104, 13 | EC to enable MEM LS buffer.  High active
#define ex_EP_WWAN_1V8_PWREN     ( _SN( 93) | IO_EXP_FLG | ex_OD_0    | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) | 14 )    // U5104, 14 | Reserved to Enable WWAN 1.8V power. High active. 
#define ex_WWAN_IO33n_IO18_SEL   ( _SN( 94) | IO_EXP_FLG | ex_INPUT   | (I2C_1 << 21) | (2 << 18) | (0x26 << 7) | 15 )    // U5104, 15 | Reserved. Notify EC that 3.3V or 1.8V WWAN module installed.  High- 1.8V WWAN, Low- 3.3V WWAN.  

/*
 * U5100, Slv_0x21, I2C_1
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  EP_TPM_PWREN               OD       0        EC to enable TPM power, high active
 * IO_01  EP_USBA_PWREN              O        1        Load Switch EN for all USBA port. Enable=1 (Default); OFF=0.
 * IO_02  EP_CODEC_DEPOP_MUTE#       O        0        To mute codec POP noise
 * IO_03  EP_HDR_DEPOP_MUTE#         O        0        To mute plugged codec module POP noise
 * IO_04  EP_SD_AUX_PWREN            OD       0        EC to enable SD controller power, high active
 * IO_05  EP_SD_MAIN_PWREN           O        1        EC to control SD S0 domain power enable
 * IO_06  EP_SSD0_PWREN              OD       0        EC to enable M.2 SSD0 power, high active
 * IO_07  EP_SSD1_PWREN              OD       0        EC to enable M.2 SSD0 power, high active
 * IO_08  EP_RTL_ISOLATE#            OD       1        Reserved for PCIE link ISOLATE.  Low active.
 * IO_09  EP_LOM_S5_PWREN            OD       0        EC to enable LOM power, high active
 * IO_10  EP_WLAN_1V8_PWREN          OD       1        Reserved to enable WLAN 1V8 power separately, high active
 * IO_11  EP_WLAN_3V3_PWREN          OD       0        EC to enable WLAN power, high active; 
 * IO_12  EP_WLAN_RADIO_DIS          O        0        To disable WLAN's raido, high active.  High-disable; 
 * IO_13  EP_WLAN_WIFI_RST           O        0        To reset WLAN module, high active
 * IO_14  EP_BT_RADIO_DIS            O        0        To disable Bluetooth radio, high active
 * IO_15  EP_M2_BT_WAKE              O        0        EC to wake Blue Tooth module. high active
 */
#define IOEXP_MASK_O___U5100_I2C1_0x21             (0x0000F02Eul)  // 0000,0000,0000,0000,1111,0000,0010,1110
#define IOEXP_MASK_OD__U5100_I2C1_0x21             (0x00000FD1ul)  // 0000,0000,0000,0000,0000,1111,1101,0001
#define IOEXP_MASK_POR_U5100_I2C1_0x21             (0x00000522ul)  // 0000,0000,0000,0000,0000,0101,0010,0010
#define IOEXP_MASK_INT_U5100_I2C1_0x21             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_I2C_PORT_U5100_I2C1_0x21             (I2C_1)         //    Port: I2C_1
#define IOEXP_I2C_ADDR_U5100_I2C1_0x21             (0x21)          // SlvAddr: (0x21 << 1), 8-bit: 0x42

/*
 * U5101, Slv_0x22, I2C_1
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  EP_FPR_LOCK#               OD       1        Low- Finger print enters low power state, High - for normal operation
 * IO_01  EP_FPR_OFF#                OD       1        High to enable wake up function, low to disable wake up function
 * IO_02  EP_FPR_PWREN               OD       0        EC to enable Fingerprint power, high active
 * IO_03  EP_FPR_GPIO                OD       1        Reserved for Fingerprint side band control
 * IO_04  EP_TPAD_DIS#               OD       1        To disable touch pad, low active
 * IO_05  EP_TPAD_FW_SEC#            O        0        for reprogramming the FW image on the touchpad,default low, high active
 * IO_06  EP_TPNL_EN                 OD       0        EC to enable Touch panel function, high active
 * IO_07  EP_TPNL_PWREN              OD       0        EC to enable Touch panel power, high active
 * IO_08  EP_USBCAM_FW_WP#           O        0        Camera FW update write protect, low active, low for normal operation
 * IO_09  EP_USBCAM_PRIV             OD       1        Reserved for USBCAM. 
 * IO_10  EP_USBCAM_PWREN            O        0        EC to enable USB CAM power, high active
 * IO_11  EP_WWAN_PWREN              OD       0        EC to enable WWAN power, high active
 * IO_12  EP_WWAN_CARD_PWROFF#       OD       0        To control WWAN power on/off;  High - pwr on ; Low - pwr off
 * IO_13  EP_WWAN_MODULE_RST#        OD       0        To reset WWAN module, low active
 * IO_14  EP_GNSS_RADIO_DIS#         OD       1        To disable WWAN's GNSS raido, low active
 * IO_15  EP_MAIN_RADIO_DIS#         OD       1        To disable WWAN's radio, low active
 */
#define IOEXP_MASK_O___U5101_I2C1_0x22             (0x00000520ul)  // 0000,0000,0000,0000,0000,0101,0010,0000
#define IOEXP_MASK_OD__U5101_I2C1_0x22             (0x0000FADFul)  // 0000,0000,0000,0000,1111,1010,1101,1111
#define IOEXP_MASK_POR_U5101_I2C1_0x22             (0x0000C21Bul)  // 0000,0000,0000,0000,1100,0010,0001,1011
#define IOEXP_MASK_INT_U5101_I2C1_0x22             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_I2C_PORT_U5101_I2C1_0x22             (I2C_1)         //    Port: I2C_1
#define IOEXP_I2C_ADDR_U5101_I2C1_0x22             (0x22)          // SlvAddr: (0x22 << 1), 8-bit: 0x44

/*
 * U5102, Slv_0x23, I2C_1
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  EP_USBMUX_BT#_TPNL_SEL     O        0        EC to select USB6 routing, low-BT, high--TPNL
 * IO_01  EP_USBMUX_FPR#_GBE_SEL     O        1        EC to select USB5 routing, low-Fingerprint, high--GbE
 * IO_02  EP_WLAN#_SD_SEL            OD       1        EC to select PCIE routing, low- WLAN, high - SD.
 * IO_03  EP_WWAN#_LOM_SEL           OD       1        EC to select PCIE routing, low- WWAN, high - LOM
 * IO_04  EP_UART0_WLAN#_HDR_SEL     OD       1        EC to select APU UART0 routing, low- WLAN, hihg - Header
 * IO_05  EP_AUD_SW#_HDR_SEL         O        0        EC to select Audio routing, low - SW Codec, high - Header
 * IO_06  EP_EVAL_SMB_APU#_EC_SEL    OD       0        EC to select Eval SMBus routing, low - APU SMB1, high - EC I2C1
 * IO_07  EP_WAIIE#_MP2_SEL          O        0        To control I2C for PWR sensor switch, low--WALLE, devices, high--APU MP2
 * IO_08  EP_SMB0_BUFF_EN            OD       1        EC to enable SMBus0 buffer, high active
 * IO_09  EP_SMB1_BUFF_EN            OD       1        EC to enable SMBus1 buffer, high active
 * IO_10  EP_USBC_RT_SMB_BUFF_EN     OD       1        EC to enable Re-timer I2C0/SMB1 buffer. Low- disable; high- enable for RT FW Update
 * IO_11  EP_TPAD_I2C_BUFF_EN        OD       0        EC to control TPAD I2C buffer enable, high active
 * IO_12  EP_TPNL_I2C_BUFF_EN        OD       1        EC to control TPNL I2C buffer enable, high active
 * IO_13  EP_NFC_I2C_BUFF_EN         OD       1        EC to control NFC I2C buffer enable, high active
 * IO_14  EP_SFH_I2C_BUFF_GATE#      OD       1        EC control to gate SFH 3V3 route path, low active
 * IO_15  EP_ESP32_I2C_EN#           O        1        EC to control E3 on I2C0 buffer enable, low active
 */
#define IOEXP_MASK_O___U5102_I2C1_0x23             (0x000080A3ul)  // 0000,0000,0000,0000,1000,0000,1010,0011
#define IOEXP_MASK_OD__U5102_I2C1_0x23             (0x00007F5Cul)  // 0000,0000,0000,0000,0111,1111,0101,1100
#define IOEXP_MASK_POR_U5102_I2C1_0x23             (0x0000F71Eul)  // 0000,0000,0000,0000,1111,0111,0001,1110
#define IOEXP_MASK_INT_U5102_I2C1_0x23             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_I2C_PORT_U5102_I2C1_0x23             (I2C_1)         //    Port: I2C_1
#define IOEXP_I2C_ADDR_U5102_I2C1_0x23             (0x23)          // SlvAddr: (0x23 << 1), 8-bit: 0x46

/*
 * U5103, Slv_0x25, I2C_1
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  EP_BAT_LED#                OD       1        Battery LED
 * IO_01  EP_KBC_LOW_BAT#            OD       1        Reserved to notify APU, to enter low battary mode, low active
 * IO_02  EP_ADAPTER_OFF             O        0        To gate adapter power source, high active
 * IO_03  APU_EC_VDDCORE_HIGH        I        x        
 * IO_04  EP_SB_TSI_SEL              O        1        To select SB-TSI routing. High- EC I2C0; low - ESP32
 * IO_05  EP_APU_PCC#                O        1        Reserved for APU PCC function, low active
 * IO_06  EP_DP0_HPD                 OD       1        Reserved to drive this signal High/Low to mimic Plug-in/ Plug-out
 * IO_07  EP_DP1_HPD                 OD       1        Reserved to drive this signal High/Low to mimic Plug-in/ Plug-out
 * IO_08  EP_EDP_SMUX_PWM_EN         O        0        Smart MUX PWM control
 * IO_09  EP_EDP_SMUX_BLON           O        0        Smart MUX back light on
 * IO_10  EP_EDP_SMUX_PNL_RST#       O        0        Smart MUX panel reset
 * IO_11  EP_EDP_SMUX_PWMMUX_SEL     O        0        Smart MUX PWM mux control
 * IO_12  APU_EC_VDDCORE_LEDON       I        x        Reserved for Vcore LED indicator
 * IO_13  EP_CODEC_AUX_MODE_EN       O        0        To enable Codec AUX Mode. High - AUX mode enable; Low- AUX mode disable. 
 * IO_14  EP_CODEC_MIC_HW_MUTE       O        0        To enable Codex MIC Mute.  High - MIC Mute , Low - MIC Unmute
 * IO_15  EP_JTAG_SEL#               OD       1        EC to control APU JTAG path to UDC, low active
 */
#define IOEXP_MASK_O___U5103_I2C1_0x25             (0x00006F34ul)  // 0000,0000,0000,0000,0110,1111,0011,0100
#define IOEXP_MASK_OD__U5103_I2C1_0x25             (0x000080C3ul)  // 0000,0000,0000,0000,1000,0000,1100,0011
#define IOEXP_MASK_POR_U5103_I2C1_0x25             (0x000080F3ul)  // 0000,0000,0000,0000,1000,0000,1111,0011
#define IOEXP_MASK_INT_U5103_I2C1_0x25             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_I2C_PORT_U5103_I2C1_0x25             (I2C_1)         //    Port: I2C_1
#define IOEXP_I2C_ADDR_U5103_I2C1_0x25             (0x25)          // SlvAddr: (0x25 << 1), 8-bit: 0x4A

/*
 * U5104, Slv_0x26, I2C_1
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  EP_MEM_SEL                 O        0        MEM VDD power selection for different speed, high - 8533, low - 9600.  
 * IO_01  EP_AUD_DMIC#_HDR_SEL       O        0        EC to select Audio DMIC routing, low - DMIC header,  high - I2S header
 * IO_02  DIS_DASH#                  OD       1        Reserved for DASH enable or disable,  low - Dash disable  ; high - Dash enable
 * IO_03  EC_EFFP_MODULE_RESET_N     OD       1        Reserved for eFFP module debug only, TBD
 * IO_05  MEM_PMIC_VIN_BULK_PWREN    O        0        Reserved to enable SODIMM/LPCAMM VIN BULK power, high active. 
 * IO_06  MEM_PMIC_VR_EN_3V3         O        0        VR EN pin control, high active. I2C command mode when power on, VR EN pin mode in power off. 
 * IO_07  TPM_OB#_HDR_SEL            O        0        EC to select TPM SPI routing , low- onboard TPM,  high- TPM header
 * IO_08  EP_LPCAMM_VDDQ_PWREN       O        0        Reserved to enbale VDDQ power for LPCAMM, high active.
 * IO_09  EP_LPCAMM_VDDQ_DIS_3V3     O        0        Reserved to control LPCAMM VDDQ_DISABLE pin. High - disable, Low- Enable. 
 * IO_10  EP_LPCAMM_UNLOCK_3V3       OD       1        EC to control LPCAMM UNLOCK pin.  High- Unlock, Low-Lock
 * IO_11  EP_LPCAMM_DVFSQ#_3V3       OD       1        Reserved to control LPCAMM DVFSQ_L pin.  Low active
 * IO_12  EP_MEM_1V0_PWREN           O        0        EC to enable MEM I3C buffer 1.0V power. High active
 * IO_13  EP_MEM_I2C_EN              O        0        EC to enable MEM LS buffer.  High active
 * IO_14  EP_WWAN_1V8_PWREN          OD       0        Reserved to Enable WWAN 1.8V power. High active. 
 * IO_15  WWAN_IO33#_IO18_SEL        I        x        Reserved. Notify EC that 3.3V or 1.8V WWAN module installed.  High- 1.8V WWAN, Low- 3.3V WWAN.  
 */
#define IOEXP_MASK_O___U5104_I2C1_0x26             (0x000033E3ul)  // 0000,0000,0000,0000,0011,0011,1110,0011
#define IOEXP_MASK_OD__U5104_I2C1_0x26             (0x00004C0Cul)  // 0000,0000,0000,0000,0100,1100,0000,1100
#define IOEXP_MASK_POR_U5104_I2C1_0x26             (0x00000C0Cul)  // 0000,0000,0000,0000,0000,1100,0000,1100
#define IOEXP_MASK_INT_U5104_I2C1_0x26             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_I2C_PORT_U5104_I2C1_0x26             (I2C_1)         //    Port: I2C_1
#define IOEXP_I2C_ADDR_U5104_I2C1_0x26             (0x26)          // SlvAddr: (0x26 << 1), 8-bit: 0x4C

/*
 * U5105, Slv_0x27, I2C_3
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  HW_WWAN_RADIO#             I        x        Notify EC to toggle enable/disable WWAN radio, falling edge active
 * IO_01  HW_WIFI_RADIO#             I        x        Notify EC to toggle enable/disable WIFI radio, falling edge active
 * IO_02  DC_S5_PWRCTL               I        x        High: S5 POWER ON IN DC MODELow: S5 POWER OFF IN DC MODE(default)
 * IO_03  PD_FORCE_FW_UP#            I        x        Notify EC to force update PD FW when triggered in G3', falling edge activeNotify EC to toggle debug post code or Board basic info, falling edge active
 * IO_04  AC_330W_PRSNT#             I        x        Low when using 330W adaptor.
 * IO_05  FCH_DBG_BUS_EN             I        x        Notify EC that FCH debug mode is enabled, high active
 * IO_06  MEM_PMIC_PWR_GOOD          I        x        Notify EC that SODIMM/LPCAMM PMIC power ok, then can enable VR_EN pin
 * IO_07  LPCAMM_EP_GSI#_3V3         I        x        Reserved. Notity EC/BIOS that the LPCAMM GSI_L pin status. 
 * IO_08  PSL_MODE_EN                I        x        High: EC PSL (VCI) mode Enable (default);Low: EC PSL (VCI) mode Disable; 
 * IO_09  EXT_TALERT#                I        x        To nofity EC, an overheating event is occurred. low active
 * IO_10  WLAN_IO33#_IO18_SEL        I        x        Notify EC the M.2 WLAN card I/O level. Low --3.3V level, High -- 1.8V level
 * IO_11  UDC_EP_DBG_CARD_PRSNT#     I        x        To notify EC, the UDC debug card is present. low active
 * IO_12  EC_UART_DOGL_PRSNT#        I        x        To notify EC, the UART dongle is present, low active
 * IO_13  WAIIE_EP_SENSOR_EN_STA#    I        x        Reserved to notify EC, the WALLE sensor power enable from system power. 
 * IO_14  EVAL_NEW#_LEGACY           I        x        Low: Plugged EVAL card is a new card ; High: Plugged EVAL card is a legacy card;
 * IO_15  EC_SHARE_SAF#_MODE         I        x        Notify EC that APU is in SHARE/SAFS mode. High- SHARE, Low - SAFA
 */
#define IOEXP_MASK_O___U5105_I2C3_0x27             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_MASK_OD__U5105_I2C3_0x27             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_MASK_POR_U5105_I2C3_0x27             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_MASK_INT_U5105_I2C3_0x27             (0x0000184Bul)  // 0000,0000,0000,0000,0001,1000,0100,1011
#define IOEXP_I2C_PORT_U5105_I2C3_0x27             (I2C_3)         //    Port: I2C_3
#define IOEXP_I2C_ADDR_U5105_I2C3_0x27             (0x27)          // SlvAddr: (0x27 << 1), 8-bit: 0x4E


#endif // end of __BOARD_IOEXPTABLE_H__
