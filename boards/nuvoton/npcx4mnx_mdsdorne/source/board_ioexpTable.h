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
#define ex_EP_UART0_WLANn_DBG_SEL ( _SN(  0) | IO_EXP_FLG | ex_O_1     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  0 )    // U5100,  0 | EC to select APU UART0 routing, low- WLAN, high - Serdes debug. 
#define ex_EP_CODEC_DEPOP_MUTEn  ( _SN(  1) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  2 )    // U5100,  2 | To mute codec POP noise
#define ex_EP_SMB0_BUFF_EN       ( _SN(  2) | IO_EXP_FLG | ex_OD_1    | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  4 )    // U5100,  4 | EC to enable SMBus0 buffer (IPU), high active
#define ex_EP_APU_PCCn           ( _SN(  3) | IO_EXP_FLG | ex_O_1     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  5 )    // U5100,  5 | Reserved for APU PCC function, low active
#define ex_EP_SSD0_PWREN         ( _SN(  4) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  6 )    // U5100,  6 | EC to enable M.2 SSD0 power, high active
#define ex_EP_HDMI_RD_PD         ( _SN(  5) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  7 )    // U5100,  7 | HDMI RD power down. High active.  L for normal operation

#define ex_EC_ESP32_RF_OFF       ( _SN(  6) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  8 )    // U5100,  8 | ESP32 RF off,  high active.  High-off, Low- on. 
#define ex_EP_MEM_8533_9600n_SEL ( _SN(  7) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) |  9 )    // U5100,  9 | MEM VDD power selection for different speed, high - 8533, low - 9600.  
#define ex_EP_WLAN_1V8_PWREN     ( _SN(  8) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 10 )    // U5100, 10 | EC to enable WLAN 1V8 power separately, high active
#define ex_EP_WLAN_3V3_PWREN     ( _SN(  9) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 11 )    // U5100, 11 | EC to enable WLAN power, high active; 
#define ex_EP_WLAN_RADIO_DIS     ( _SN( 10) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 12 )    // U5100, 12 | To disable WLAN's raido, high active.  High-disable; 
#define ex_EP_WLAN_WIFI_RST      ( _SN( 11) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 13 )    // U5100, 13 | To reset WLAN module, high active
#define ex_EP_BT_RADIO_DIS       ( _SN( 12) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 14 )    // U5100, 14 | To disable Bluetooth radio, high active
#define ex_EP_M2_BT_WAKE         ( _SN( 13) | IO_EXP_FLG | ex_O_0     | (I2C_1 << 21) | (2 << 18) | (0x21 << 7) | 15 )    // U5100, 15 | EC to wake Blue Tooth module. high active

#define ex_EP_USBA_PWREN         ( _SN( 14) | IO_EXP_FLG | ex_O_1     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  0 )    // U5101,  0 | Load Switch EN for all USBA port. Enable=1 (Default); OFF=0.
#define ex_EP_SB_TSI_ESP32n_EC_SEL ( _SN( 15) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  1 )    // U5101,  1 | To select SB-TSI routing. High- EC I2C0; low - ESP32, 0: Default, 1: when EC_IOT_3V3_SENSE_EN=1 
#define ex_EP_FPR_PWREN          ( _SN( 16) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  2 )    // U5101,  2 | EC to enable Fingerprint power, high active
#define ex_EP_TPM_PWREN          ( _SN( 17) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  3 )    // U5101,  3 | EC to enable TPM power, high active
#define ex_EP_SENSOR_1V8_PWREN   ( _SN( 18) | IO_EXP_FLG | ex_O_1     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  4 )    // U5101,  4 | EC to enable 1.8V SFH PWR load switch. High active
#define ex_EP_SENSOR_3V3_PWREN   ( _SN( 19) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  5 )    // U5101,  5 | EC to enable 3.3V SFH PWR load switch. High active
#define ex_EP_TPAD_PWREN         ( _SN( 20) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  6 )    // U5101,  6 | EC to enable TPAD PWR load switch. High active
#define ex_EP_TPNL_PWREN         ( _SN( 21) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  7 )    // U5101,  7 | EC to enable Touch panel power, high active

#define ex_EP_USBCAM_FW_WPn      ( _SN( 22) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  8 )    // U5101,  8 | Camera FW update write protect, low active, low for normal operation
#define ex_EP_USBCAM_PRIV        ( _SN( 23) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) |  9 )    // U5101,  9 | USBCAM Privacy. High active.
#define ex_EP_USBCAM_PWREN       ( _SN( 24) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) | 10 )    // U5101, 10 | EC to enable USB CAM power, high active
#define ex_EP_USBMUX_USB20n_EUSB2V1_SEL ( _SN( 25) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) | 11 )    // U5101, 11 | Drive it low when detect 
#define ex_EP_ESP32_I2C_ENn      ( _SN( 26) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) | 12 )    // U5101, 12 | EC to control E3 on I2C0 buffer enable, low active
#define ex_EP_HDMI_RD_PWREN      ( _SN( 27) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) | 13 )    // U5101, 13 | To enable HDMI RD PWR load switch.  High active
#define ex_EP_DMIC_PWREN         ( _SN( 28) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) | 14 )    // U5101, 14 | To enbale CAM DMIC PWR load switch. High active
#define ex_EP_USBC_RT_SMB_BUFF_EN ( _SN( 29) | IO_EXP_FLG | ex_O_0     | (I2C_3 << 21) | (2 << 18) | (0x22 << 7) | 15 )    // U5101, 15 | EC to enable Re-timer I2C0/SMB1 buffer. Low- disable; high- enable for RT FW Update

/*
 * U5100, Slv_0x21, I2C_1
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  EP_UART0_WLAN#_DBG_SEL     O        1        EC to select APU UART0 routing, low- WLAN, high - Serdes debug. 
 * IO_02  EP_CODEC_DEPOP_MUTE#       O        0        To mute codec POP noise
 * IO_04  EP_SMB0_BUFF_EN            OD       1        EC to enable SMBus0 buffer (IPU), high active
 * IO_05  EP_APU_PCC#                O        1        Reserved for APU PCC function, low active
 * IO_06  EP_SSD0_PWREN              O        0        EC to enable M.2 SSD0 power, high active
 * IO_07  EP_HDMI_RD_PD              O        0        HDMI RD power down. High active.  L for normal operation
 * IO_08  EC_ESP32_RF_OFF            O        0        ESP32 RF off,  high active.  High-off, Low- on. 
 * IO_09  EP_MEM_8533_9600#_SEL      O        0        MEM VDD power selection for different speed, high - 8533, low - 9600.  
 * IO_10  EP_WLAN_1V8_PWREN          O        0        EC to enable WLAN 1V8 power separately, high active
 * IO_11  EP_WLAN_3V3_PWREN          O        0        EC to enable WLAN power, high active; 
 * IO_12  EP_WLAN_RADIO_DIS          O        0        To disable WLAN's raido, high active.  High-disable; 
 * IO_13  EP_WLAN_WIFI_RST           O        0        To reset WLAN module, high active
 * IO_14  EP_BT_RADIO_DIS            O        0        To disable Bluetooth radio, high active
 * IO_15  EP_M2_BT_WAKE              O        0        EC to wake Blue Tooth module. high active
 */
#define IOEXP_MASK_O___U5100_I2C1_0x21             (0x0000FFE5ul)  // 0000,0000,0000,0000,1111,1111,1110,0101
#define IOEXP_MASK_OD__U5100_I2C1_0x21             (0x00000010ul)  // 0000,0000,0000,0000,0000,0000,0001,0000
#define IOEXP_MASK_POR_U5100_I2C1_0x21             (0x00000031ul)  // 0000,0000,0000,0000,0000,0000,0011,0001
#define IOEXP_MASK_INT_U5100_I2C1_0x21             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_I2C_PORT_U5100_I2C1_0x21             (I2C_1)         //    Port: I2C_1
#define IOEXP_I2C_ADDR_U5100_I2C1_0x21             (0x21)          // SlvAddr: (0x21 << 1), 8-bit: 0x42

/*
 * U5101, Slv_0x22, I2C_3
 *
 *        NetName                    BufType  POR_Def  Comment
 * IO_00  EP_USBA_PWREN              O        1        Load Switch EN for all USBA port. Enable=1 (Default); OFF=0.
 * IO_01  EP_SB_TSI_ESP32#_EC_SEL    O        0        To select SB-TSI routing. High- EC I2C0; low - ESP32, 0: Default, 1: when EC_IOT_3V3_SENSE_EN=1 
 * IO_02  EP_FPR_PWREN               O        0        EC to enable Fingerprint power, high active
 * IO_03  EP_TPM_PWREN               O        0        EC to enable TPM power, high active
 * IO_04  EP_SENSOR_1V8_PWREN        O        1        EC to enable 1.8V SFH PWR load switch. High active
 * IO_05  EP_SENSOR_3V3_PWREN        O        0        EC to enable 3.3V SFH PWR load switch. High active
 * IO_06  EP_TPAD_PWREN              O        0        EC to enable TPAD PWR load switch. High active
 * IO_07  EP_TPNL_PWREN              O        0        EC to enable Touch panel power, high active
 * IO_08  EP_USBCAM_FW_WP#           O        0        Camera FW update write protect, low active, low for normal operation
 * IO_09  EP_USBCAM_PRIV             O        0        USBCAM Privacy. High active.
 * IO_10  EP_USBCAM_PWREN            O        0        EC to enable USB CAM power, high active
 * IO_11  EP_USBMUX_USB20#_EUSB2V1_SEL  O        0        Drive it low when detect 
 * IO_12  EP_ESP32_I2C_EN#           O        0        EC to control E3 on I2C0 buffer enable, low active
 * IO_13  EP_HDMI_RD_PWREN           O        0        To enable HDMI RD PWR load switch.  High active
 * IO_14  EP_DMIC_PWREN              O        0        To enbale CAM DMIC PWR load switch. High active
 * IO_15  EP_USBC_RT_SMB_BUFF_EN     O        0        EC to enable Re-timer I2C0/SMB1 buffer. Low- disable; high- enable for RT FW Update
 */
#define IOEXP_MASK_O___U5101_I2C3_0x22             (0x0000FFFFul)  // 0000,0000,0000,0000,1111,1111,1111,1111
#define IOEXP_MASK_OD__U5101_I2C3_0x22             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_MASK_POR_U5101_I2C3_0x22             (0x00000011ul)  // 0000,0000,0000,0000,0000,0000,0001,0001
#define IOEXP_MASK_INT_U5101_I2C3_0x22             (0x00000000ul)  // 0000,0000,0000,0000,0000,0000,0000,0000
#define IOEXP_I2C_PORT_U5101_I2C3_0x22             (I2C_3)         //    Port: I2C_3
#define IOEXP_I2C_ADDR_U5101_I2C3_0x22             (0x22)          // SlvAddr: (0x22 << 1), 8-bit: 0x44


#endif // end of __BOARD_IOEXPTABLE_H__
