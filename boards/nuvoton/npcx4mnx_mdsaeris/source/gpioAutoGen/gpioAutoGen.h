/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __GPIOAUGOGEN_H__
#define __GPIOAUGOGEN_H__

#include <zephyr/kernel.h>
#include <system.h>

/***********************************************************************
 *                                                                     *
 * PIN CTRL INDEX definition sort by category                          *
 *                                                                     *
 ***********************************************************************/

/*******************
 * ACPI 
 *******************/
#define EC_APU_SCI_N             EC_GPIO_76 // GPIO76       | J1.23    |            | EC to notify APU a system control interrupt is triggered.

#define POR_INIT_ACPI            \
    EC_APU_SCI_N

/*******************
 * BAT_CHG 
 *******************/
#define BAT_PRSNT_N              EC_GPIO_72 // GPIO72       | J4.15    | GPIO33_13  | Notify EC that battery is present, low active
#define CHG_ACOK                 EC_GPIO_D2 // GPIOD2       | J3.8     |            | To notify EC, AC is present
#define CHG_EC_PROCHOT_N         EC_GPIO_E0 // GPIOE0       | J4.20    | GPIO33_25  | Notify EC that charger is overheating, low active

#define POR_INIT_BAT_CHG         \
    BAT_PRSNT_N,                  CHG_ACOK,                     CHG_EC_PROCHOT_N

/*******************
 * DBG 
 *******************/
#define EC_BLINK_N               EC_GPIO_B6 // GPIOB6       | J6.21    | GPIO33_10  | Storage acitve indication

#define POR_INIT_DBG             \
    EC_BLINK_N

/*******************
 * GPIO 
 *******************/
#define ALS_INT_N                EC_GPIO_A1 // GPIOA1       | .        |            | ALS INT# to EC, low active.
#define BOARD_ID0                EC_GPIO_67 // GPIO67       | J6.25    |            | board id detect gpio0
#define BOARD_ID1                EC_GPIO_70 // GPIO70       | J6.27    |            | board id detect gpio1
#define DBG_CARD_PRSNT_N         EC_GPIO_D6 // GPIOD6       | J4.4     | GPIO33_19  | Reserved.  To notify EC the Debug Card is present,  low active. 
#define EC_APU_PCC_N             EC_GPIO_81 // GPIO81       | .        |            | Reserved. for APU PCC function, low active
#define EC_BT_RADIO_DIS          EC_GPIO_C7 // GPIOC7       | J4.19    |            | To disable Bluetooth radio, high active
#define EC_CHG_ORG_LED           EC_GPIO_41 // GPIO41       | J6.17    | GIO33_8    | To light this LED when BAT is in 
#define EC_CHG_WHITE_LED         EC_GPIO_43 // GPIO43       | J6.11    | GPIO33_5   | To light this LED when BAT is in 
#define EC_CODEC_DEPOP_MUTE_N    EC_GPIO_B0 // GPIOB0       | .        |            | To mute codec POP noise, low active
#define EC_CODEC_MIC_HW_MUTE_N   EC_GPIO_A6 // GPIOA6       | .        |            | To HW mute Codex MIC,  Low active. 
#define EC_DMIC_PWREN            EC_GPIO_F0 // GPIOF0       | J6.15    | GPIO33_7   | To enbale CAM DMIC PWR load switch. High active
#define EC_FN_LED                EC_GPIO_34 // GPIO34       | J6.23    |            | To light FN LED,  high active
#define EC_FPR_PWREN             EC_GPIO_E5 // GPIOE5       | .        |            | EC to enable Fingerprint power, high active
#define EC_M2_BT_WAKE            EC_GPIO_37 // GPIO37       | J6.19    |            | EC to wake Blue Tooth module. high active
#define EC_SENSOR_1V8_PWREN      EC_GPIO_03 // GPIO03       | .        |            | Sensor 1.8V enable
#define EC_SSD0_PWREN            EC_GPIO_B1 // GPIOB1       | J3.20    | GPIO33_17  | EC to enable M.2 SSD0 power, high active
#define EC_TPAD_PWREN            EC_GPIO_C4 // GPIOC4       | J3.18    | GPIO33_16  | EC to enable TPAD PWR load switch. High active
#define EC_TPM_PWREN             EC_GPIO_44 // GPIO44       | J6.9     | GIO33_4    | EC to enable TPM power, high active
#define EC_TPNL_PWREN            EC_GPIO_E1 // GPIOE1       | J3.16    |            | EC to enable Touch panel power, high active
#define EC_USBCAM_FW_WP_N        EC_GPIO_D4 // GPIOD4       | .        |            | Camera FW update write protect, low active, low for normal operation
#define EC_USBCAM_PRIV           EC_GPIO_E2 // GPIOE2       | .        |            | USBCAM Privacy. High active.
#define EC_USBCAM_PWREN          EC_GPIO_64 // GPIO64       | .        |            | EC to enable USB CAM power, high active
#define EC_USBC_RT_SMB_BUFF_EN   EC_GPIO_C6 // GPIOC6       | J4.12    | GPIO33_22  | EC to enable Re-timer I2C0/SMB1 buffer. Low- disable; high- enable for RT FW Update
#define EC_WLAN_I2C_ALERT_N_1V8  EC_GPIO_95 // GPIO95       | .        |            | Reserved. WLAN I2C Alert#, low active. 
#define EC_WLAN_RASIO_DIS        EC_GPIO_C0 // GPIOC0       | J4.2     | GPIO33_18  | To disable WLAN's raido, high active.  High-disable; 
#define EC_WLAN_WIFI_RST         EC_GPIO_60 // GPIO60       | J6.7     | GPIO33_3   | To reset WLAN module, high active
#define LID_CLOSE_N_3V3          EC_GPIO_01 // GPIO01       | .        |            | Notify EC that Lid is closed, low active
#define PD0_EC_USBC_INT_N        EC_GPIO_F3 // GPIOF3       | J4.17    | GPIO33_14  | To notify EC, USB PD controller0 need service from EC
#define PD1_EC_USBC_INT_N        EC_GPIO_F2 // GPIOF2       | J4.18    | GPIO33_24  | To notify EC, USB PD controller0 need service from EC
#define SFH_EC_INT_N_R           EC_GPIO_A3 // GPIOA3       | .        |            | SFH INT# from APU (IPIO0) to EC, low active. 
#define TPNL_EN_CONN             EC_GPIO_62 // GPIO62       | J6.3     | GPIO33_1   | Reserve
#define WLAN_IO33_N_IO18_SEL     EC_GPIO_42 // GPIO42       | J6.13    | GPIO33_6   | Reserved. Notify EC that 3.3V/1.8V WWAN module installed.  High- 1.8V WWAN, Low- 3.3V WWAN.  
#define WLAN_PWREN               EC_GPIO_D5 // GPIOD5       | .        |            | EC to enable WLAN power, high active; 

#define POR_INIT_GPIO            \
    ALS_INT_N,                    BOARD_ID0,                    BOARD_ID1,                    DBG_CARD_PRSNT_N,             \
    EC_APU_PCC_N,                 EC_BT_RADIO_DIS,              EC_CHG_ORG_LED,               EC_CHG_WHITE_LED,             \
    EC_CODEC_DEPOP_MUTE_N,        EC_CODEC_MIC_HW_MUTE_N,       EC_DMIC_PWREN,                EC_FN_LED,                    \
    EC_FPR_PWREN,                 EC_M2_BT_WAKE,                EC_SENSOR_1V8_PWREN,          EC_SSD0_PWREN,                \
    EC_TPAD_PWREN,                EC_TPM_PWREN,                 EC_TPNL_PWREN,                EC_USBCAM_FW_WP_N,            \
    EC_USBCAM_PRIV,               EC_USBCAM_PWREN,              EC_USBC_RT_SMB_BUFF_EN,       EC_WLAN_I2C_ALERT_N_1V8,      \
    EC_WLAN_RASIO_DIS,            EC_WLAN_WIFI_RST,             LID_CLOSE_N_3V3,              PD0_EC_USBC_INT_N,            \
    PD1_EC_USBC_INT_N,            SFH_EC_INT_N_R,               TPNL_EN_CONN,                 WLAN_IO33_N_IO18_SEL,         \
    WLAN_PWREN

/*******************
 * I2C 
 *******************/
#define EC_I2C0_SCL_3V3_S0       EC_GPIO_B5 // I2C0_SCL0    | J6.2     |            | EC I2C0 SCL
#define EC_I2C0_SDA_3V3_S0       EC_GPIO_B4 // I2C0_SDA0    | J6.6     |            | EC I2C0 SDA
#define EC_I2C1_SCL_3V3_S5       EC_GPIO_90 // I2C1_SCL0    | J6.8     |            | EC I2C1 SCL
#define EC_I2C1_SDA_3V3_S5       EC_GPIO_87 // I2C1_SDA0    | J6.12    |            | EC I2C1 SDA
#define EC_I2C2_SCL_1V8_S5       EC_GPIO_92 // I2C2_SCL0    | J6.14    |            | EC I2C2 SCL
#define EC_I2C2_SDA_1V8_S5       EC_GPIO_91 // I2C2_SDA0    | J6.18    |            | EC I2C2 SDA
#define EC_I2C3_SCL_3V3_EC       EC_GPIO_D1 // I2C3_SCL0    | J6.20    |            | EC I2C3 SCL
#define EC_I2C3_SDA_3V3_EC       EC_GPIO_D0 // I2C3_SDA0    | J6.24    |            | EC I2C3 SDA

#define POR_INIT_I2C             \
    EC_I2C0_SCL_3V3_S0,           EC_I2C0_SDA_3V3_S0,           EC_I2C1_SCL_3V3_S5,           EC_I2C1_SDA_3V3_S5,           \
    EC_I2C2_SCL_1V8_S5,           EC_I2C2_SDA_1V8_S5,           EC_I2C3_SCL_3V3_EC,           EC_I2C3_SDA_3V3_EC

/*******************
 * Keyboard 
 *******************/
#define EC_CAP_LED               EC_GPIO_C2 // GPIOC2       | J5.25    |            | To light CAP LED, high active
#define SMSC_KSI_0               EC_GPIO_31 // KSI0         | J3.2     |            | Keyboard Scan Matrix Input
#define SMSC_KSI_1               EC_GPIO_30 // KSI1         | J3.4     |            | Keyboard Scan Matrix Input
#define SMSC_KSI_2               EC_GPIO_27 // KSI2         | J3.6     |            | Keyboard Scan Matrix Input
#define SMSC_KSI_3               EC_GPIO_26 // KSI3         | J3.8     |            | Keyboard Scan Matrix Input
#define SMSC_KSI_4               EC_GPIO_25 // KSI4         | J3.10    |            | Keyboard Scan Matrix Input
#define SMSC_KSI_5               EC_GPIO_24 // KSI5         | J3.12    |            | Keyboard Scan Matrix Input
#define SMSC_KSI_6               EC_GPIO_23 // KSI6         | J3.14    |            | Keyboard Scan Matrix Input
#define SMSC_KSI_7               EC_GPIO_22 // KSI7         | J3.16    |            | Keyboard Scan Matrix output
#define SMSC_KSO_0               EC_GPIO_21 // KSO00        | J3.18    |            | Keyboard Scan Matrix output
#define SMSC_KSO_1               EC_GPIO_20 // KSO01        | J3.20    |            | Keyboard Scan Matrix output
#define SMSC_KSO_10              EC_GPIO_07 // KSO10        | J3.9     |            | Keyboard Scan Matrix output
#define SMSC_KSO_11              EC_GPIO_06 // KSO11        | J3.11    |            | Keyboard Scan Matrix output
#define SMSC_KSO_12              EC_GPIO_05 // KSO12        | J3.13    |            | Keyboard Scan Matrix output
#define SMSC_KSO_13              EC_GPIO_04 // KSO13        | J3.15    |            | Keyboard Scan Matrix output
#define SMSC_KSO_14              EC_GPIO_82 // KSO14        | J3.17    |            | Keyboard Scan Matrix output
#define SMSC_KSO_15              EC_GPIO_83 // KSO15        | J3.19    |            | Keyboard Scan Matrix output
#define SMSC_KSO_2               EC_GPIO_17 // KSO02        | J3.22    |            | Keyboard Scan Matrix output
#define SMSC_KSO_3               EC_GPIO_16 // KSO03        | J3.24    |            | Keyboard Scan Matrix output
#define SMSC_KSO_4               EC_GPIO_15 // KSO04        | J3.26    |            | Keyboard Scan Matrix output
#define SMSC_KSO_5               EC_GPIO_14 // KSO05        | J3.28    |            | Keyboard Scan Matrix output
#define SMSC_KSO_6               EC_GPIO_13 // KSO06        | J3.1     |            | Keyboard Scan Matrix output
#define SMSC_KSO_7               EC_GPIO_12 // KSO07        | J3.3     |            | Keyboard Scan Matrix output
#define SMSC_KSO_8               EC_GPIO_11 // KSO08        | J3.5     |            | Keyboard Scan Matrix output
#define SMSC_KSO_9               EC_GPIO_10 // KSO09        | J3.7     |            | Keyboard Scan Matrix output

#define POR_INIT_Keyboard        \
    EC_CAP_LED,                   SMSC_KSI_0,                   SMSC_KSI_1,                   SMSC_KSI_2,                   \
    SMSC_KSI_3,                   SMSC_KSI_4,                   SMSC_KSI_5,                   SMSC_KSI_6,                   \
    SMSC_KSI_7,                   SMSC_KSO_0,                   SMSC_KSO_1,                   SMSC_KSO_10,                  \
    SMSC_KSO_11,                  SMSC_KSO_12,                  SMSC_KSO_13,                  SMSC_KSO_14,                  \
    SMSC_KSO_15,                  SMSC_KSO_2,                   SMSC_KSO_3,                   SMSC_KSO_4,                   \
    SMSC_KSO_5,                   SMSC_KSO_6,                   SMSC_KSO_7,                   SMSC_KSO_8,                   \
    SMSC_KSO_9


#define KEYBOARD_KSI_LIST    \
    SMSC_KSI_0,                   SMSC_KSI_1,                   SMSC_KSI_2,                   SMSC_KSI_3,                   \
    SMSC_KSI_4,                   SMSC_KSI_5,                   SMSC_KSI_6,                   SMSC_KSI_7

#define KEYBOARD_KSO_LIST    \
    SMSC_KSO_0,                   SMSC_KSO_1,                   SMSC_KSO_2,                   SMSC_KSO_3,                   \
    SMSC_KSO_4,                   SMSC_KSO_5,                   SMSC_KSO_6,                   SMSC_KSO_7,                   \
    SMSC_KSO_8,                   SMSC_KSO_9,                   SMSC_KSO_10,                  SMSC_KSO_11,                  \
    SMSC_KSO_12,                  SMSC_KSO_13,                  SMSC_KSO_14,                  SMSC_KSO_15

/*******************
 * PWRSEQ 
 *******************/
#define APU_EC_RESET_N           EC_GPIO_F4 // GPIOF4       | J3.1     |            | APU to reset eSPI slave, low active
#define APU_PWROK                EC_GPIO_97 // GPIO97       | J3.3     |            | To notify EC, all system power rails are ready.
#define EC_1V8_S5_EN             EC_GPIO_E4 // GPIOE4       | J4.3     |            | To enable system's +1.8V_ALW power rail, high active
#define EC_PWR_BTN_N             EC_GPIO_A5 // GPIOA5       | J3.9     |            | To notify APU, power button is pressed.
#define EC_S0_LED                EC_GPIO_F1 // GPIOF1       | J6.5     | GPIO33_2   | To light S0 power indicator LED. High active. O_0>>1, follow S0 power EN:  EC_SLP_S3_S0A3#
#define EC_S5_PWREN              EC_GPIO_E3 // GPIOE3       | J4.1     |            | To enable system's ALWAYS power rails, high active
#define EC_SLP_S3_S0A3_N         EC_GPIO_74 // GPIO74       | J3.17    |            | EC to enable/disable system S0 power rails.
#define EC_SLP_S5_N              EC_GPIO_61 // GPIO61       | J3.19    |            | EC to enable/disable system S5 power rails.
#define EC_WLAN_AUX_RST_N        EC_GPIO_63 // GPIO63       | J6.1     | GIO33_0    | EC will keep S0 PCIe aux reset status during go into or resume from S0i3, this is to save and restore original device status. PCIe global reset will be used for PCIe trainging sequence (parallel mode) control.
#define HW_PWR_BTN_N             EC_GPIO_00 // GPIO00       | J3.11    |            | To notify EC, power button is pressed, low active
#define KBRST_N                  EC_GPIO_F5 // GPIOF5       | J4.5     |            | EC to Warm reset APU, low active
#define MPM_EVENT_N              EC_GPIO_73 // GPIO73       | J4.8     | GPIO33_20  | Wireless Manageability Processor interrupt, low active
#define RSMRST_N                 EC_GPIO_94 // GPIO94       | J4.7     |            | EC to reset FCH, low active.
#define SLP_S3_S0A3_N            EC_GPIO_B7 // GPIOB7       | J3.13    |            | To notify EC, S3 is asserted, low active
#define SLP_S5_N                 EC_GPIO_B2 // GPIOB2       | J3.15    |            | EC to enable/disable system S3 power rails.
#define SYSTEM_S5_PG             EC_GPIO_C5 // GPIOC5       | J3.12    |            | To notify EC that system's ALWAYS power rails is ready, high active.
#define SYS_RST_N                EC_GPIO_33 // GPIO33       | J4.7     |            | EC to cold reset APU, low active

#define POR_INIT_PWRSEQ          \
    APU_EC_RESET_N,               APU_PWROK,                    EC_1V8_S5_EN,                 EC_PWR_BTN_N,                 \
    EC_S0_LED,                    EC_S5_PWREN,                  EC_SLP_S3_S0A3_N,             EC_SLP_S5_N,                  \
    EC_WLAN_AUX_RST_N,            HW_PWR_BTN_N,                 KBRST_N,                      MPM_EVENT_N,                  \
    RSMRST_N,                     SLP_S3_S0A3_N,                SLP_S5_N,                     SYSTEM_S5_PG,                 \
    SYS_RST_N

/*******************
 * SYS_BUS 
 *******************/
#define ESPI_EC_ALERT_N          EC_GPIO_57 // eSPI_ALERT#  | J1.5     |            | eSPI Alert signal, Low-Active. Exercised only if ALERT# is configured to be pre-sented separately from the IO1 pin
#define ESPI_EC_CLK              EC_GPIO_55 // eSPI_CLK     | J1.2     |            | eSPI Clock 
#define ESPI_EC_CS_N             EC_GPIO_53 // eSPI_CS#     | J1.7     |            | eSPI Chip Select, Low-Active
#define ESPI_EC_D0               EC_GPIO_46 // eSPI_IO0     | J1.6     |            | eSPI Data Bus, bit 0. Input (MOSI) in x1 Bus Mode. Else, it holds the LS data bit. 
#define ESPI_EC_D1               EC_GPIO_47 // eSPI_IO1     | J1.8     |            | eSPI Data Bus, bit 1. Output (MISO) in x1 Bus Mode. Also, by default, presents ALERT# state between frames. 
#define ESPI_EC_D2               EC_GPIO_51 // eSPI_IO2     | J1.12    |            | eSPI Data Bus, bit 2. Used only in x4 mode
#define ESPI_EC_D3               EC_GPIO_52 // eSPI_IO3     | J1.14    |            | eSPI Data Bus, bit 3. Used only in x4 mode, as MS bit
#define ESPI_EC_RESET_N          EC_GPIO_54 // eSPI_RST#    | .        |            | 
#define SPI_APU_EC_ROM_GNT       EC_GPIO_50 // GPIO50       | J1.1     |            | SPI bus grant from APU to EC
#define SPI_EC_APU_ROM_REQ       EC_GPIO_56 // GPIO56       | J1.3     |            | EC to notify APU that EC need to use ROM.
#define SPI_EC_ROM_CLK           EC_GPIO_A2 // FLM_SCLK     | J1.18    |            | Quad SPI Controller Clock, Shared SPI port
#define SPI_EC_ROM_CS0_N         EC_GPIO_A0 // FLM_CSIO#    | J3.7     |            | Quad SPI Controller Chip Select, Shared SPI port. 172x: Boot Source select strap, must use as RSMRST# for MAFS.
#define SPI_EC_ROM_D0            EC_GPIO_A4 // FLM_DI0      | J1.24    |            | Quad SPI Controller Data 0, Shared SPI port
#define SPI_EC_ROM_D1            EC_GPIO_96 // FLM_DI1      | J1.20    |            | Quad SPI Controller Data 1, Shared SPI port

#define POR_INIT_SYS_BUS         \
    ESPI_EC_ALERT_N,              ESPI_EC_CLK,                  ESPI_EC_CS_N,                 ESPI_EC_D0,                   \
    ESPI_EC_D1,                   ESPI_EC_D2,                   ESPI_EC_D3,                   ESPI_EC_RESET_N,              \
    SPI_APU_EC_ROM_GNT,           SPI_EC_APU_ROM_REQ,           SPI_EC_ROM_CLK,               SPI_EC_ROM_CS0_N,             \
    SPI_EC_ROM_D0,                SPI_EC_ROM_D1

/*******************
 * THERMAL 
 *******************/
#define APU_EC_ALERT_N           EC_GPIO_B3 // GPIOB3       | J3.2     |            | To nofity EC, APU SB-TSI need service from EC
#define APU_THERMTRIP_N          EC_GPIO_80 // GPIO80       | J3.5     |            | To notify EC that APU is shutdown
#define EC_APU_PROCHOT_N         EC_GPIO_C1 // GPIOC1       | J3.17    |            | EC to notify APU overheating, low active
#define EC_PWM0                  EC_GPIO_C3 // PWM0         | J5.5     |            | FAN speed control for APU FAN1 

#define POR_INIT_THERMAL         \
    APU_EC_ALERT_N,               APU_THERMTRIP_N,              EC_APU_PROCHOT_N,             EC_PWM0

/*******************
 * UNUSED 
 *******************/
#define EC_ARM                   EC_GPIO_66 // GPIO66       | .        |            | 
#define GPIO02                   EC_GPIO_02 // GPIO02       | .        |            | 
#define GPIO32                   EC_GPIO_32 // GPIO32       | J5.29    |            | 
#define GPIO35                   EC_GPIO_35 // GPIO35       | J5.27    |            | 
#define GPIO36                   EC_GPIO_36 // GPIO36       | J4.13    |            | 
#define GPIO45                   EC_GPIO_45 // GPIO45       | .        |            | Clear CMOS by EC, high active.  Always keep low. 
#define GPIO65                   EC_GPIO_65 // GPIO65       | .        |            | 
#define GPIO93                   EC_GPIO_93 // GPIO93       | J1.30    |            | 
#define GPIOA7                   EC_GPIO_A7 // GPIOA7       | J1.16    |            | 
#define GPIOD3                   EC_GPIO_D3 // GPIOD3       | .        |            | 
#define GPIOD7                   EC_GPIO_D7 // GPIOD7       | .        |            | 

#define POR_INIT_UNUSED          \
    EC_ARM,                       GPIO02,                       GPIO32,                       GPIO35,                       \
    GPIO36,                       GPIO45,                       GPIO65,                       GPIO93,                       \
    GPIOA7,                       GPIOD3,                       GPIOD7

/* Runtime GPIO settings */
#define GPIO_RUNTIME_SWITCH_G3                                         \
    {EC_WLAN_AUX_RST_N,  GPIO_OUTPUT_LOW,                             GPIO_CTRL_PWRG_OFF         },    \
    {MPM_EVENT_N,        GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN,          GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D0,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D1,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D2,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D3,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_CS_N,       GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_CLK,        GPIO_INPUT,                                  GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_ALERT_N,    GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \


#define GPIO_RUNTIME_SWITCH_S5S4                                       \
    {ESPI_EC_D0,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D1,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D2,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D3,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_CS_N,       GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_CLK,        GPIO_INPUT,                                  GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_ALERT_N,    GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \


#define GPIO_RUNTIME_SWITCH_S3                                         \
    {ESPI_EC_D0,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D1,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D2,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_D3,         GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_CS_N,       GPIO_INPUT | GPIO_PULL_UP,                   GPIO_CTRL_PWRG_OFF         },    \
    {ESPI_EC_CLK,        GPIO_INPUT,                                  GPIO_CTRL_PWRG_OFF         },    \


#define GPIO_RUNTIME_SWITCH_S0                                         \



/**
 * @brief change GPIO group status based on power sequence
 *
 * @param current power sequence status
 * @retval void
 */
void __autoGen_runtimeGpioSwitching (enum system_power_state pwr_state);

/**
 * @brief Initialize EC GPIO
 *
 * @retval 0 if successful
 */
int __autoGen_initECGPIO (void );


/***********************************************************************
 *                                                                     *
 * Forward declaration for EC ACPI space handlers                      *
 *                                                                     *
 ***********************************************************************/
extern void Board_Gpio_AcpiHandler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
 
#endif // end of __GPIOAUGOGEN_H__
