/*****************************************************************************
 *
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 ******************************************************************************
 */

#include <soc.h>
#include "npcx4mnx_pin.h"
#include "gpioAutoGen.h"

#ifndef __MDSEFFP_NPCX4_H__
#define __MDSEFFP_NPCX4_H__

#define BRD_MDS_AERIS

#define PLATFORM_DATA(x, y)  ((x) | ((y) << 8))

/* Nuvoton EC Module SLP_S3/S5 is low acctive */
#define SLP_S3_ASSERT !gpio_read_pin(SLP_S3_S0A3_N)
#define SLP_S5_ASSERT !gpio_read_pin(SLP_S5_N)
#define ASSERT_FCH_PWRBTN gpio_write_pin(EC_PWR_BTN_N, 0)
#define DEASSERT_FCH_PWRBTN gpio_write_pin(EC_PWR_BTN_N, 1)
#define EC_DEBUG_LED(x)
#define KBC_CAPS_LOCK       EC_CAP_LED

/*******************
 * APU Power OK
 *******************/
#define PIN_LIST_DRIVER_TO_HIGH_AFTER_APU_PWROK \
     EC_WLAN_AUX_RST_N,          GPIO_PIN_NULL

/*******************
 * ESPI Power on
 *******************/
#define PIN_LIST_ESPI_SIGNALES_TO_PWRON \
     ESPI_EC_ALERT_N,            ESPI_EC_CLK,                  ESPI_EC_CS_N,                  ESPI_EC_D0,                 \
     ESPI_EC_D1,                 ESPI_EC_D2,                   ESPI_EC_D3,                    ESPI_EC_RESET_N,            \
     GPIO_PIN_NULL

/* Device instance names */
#define I2C_BUS_0           DT_NODELABEL(i2c0_0)
#define I2C_BUS_1           DT_NODELABEL(i2c1_0)
#define I2C_BUS_2           DT_NODELABEL(i2c2_0)
#define I2C_BUS_3           DT_NODELABEL(i2c3_0)
#define PS2_KEYBOARD        DT_NODELABEL(ps2_0)
#define PS2_MOUSE           DT_NODELABEL(ps2_1)
#define ESPI_0              DT_NODELABEL(espi0)
#define KSCAN_MATRIX        DT_NODELABEL(kscan0)
#define SPI_0               DT_NODELABEL(qspi_fiu1)
#define NOR_FLASH           DT_NODELABEL(int_flash)
#define EXT_FLASH           DT_NODELABEL(ext_flash0)
#define EXT_FLASH1          DT_NODELABEL(ext_flash1)
#define INT_FLASH           DT_NODELABEL(int_flash)
#define UART_0              DT_NODELABEL(uart2)
//#define UART_1              DT_NODELABEL(uart1)
#define PWM_0               DT_NODELABEL(pwm0)
//#define PWM_2               DT_NODELABEL(pwm2)
#define TACH_n              DT_NODELABEL(tach1)
#define BBRAM               DT_NODELABEL(bbram)
#define SHA                 DT_NODELABEL(sha0)

//#define ADC_CH_BASE       DT_NODELABEL(adc0)

enum i2c_bus_num {
	I2C_0,
	I2C_1,
	I2C_2,
	I2C_3,
};

/* inputBuffer Initial positions */
#define PWR_BTN_INIT_POS		    (1)
#define BAT_IN_INIT_POS             (1)
#define CHG_OK_INIT_POS             (0)
#define LID_INIT_POS                (1)
#define DBG_CARD_INIT_POS           (1)
#define CHG_PROCHOTn1_INIT_POS      (1)
#define CHG_PROCHOTn2_INIT_POS      (1)
#define DC_S5_ALW_INIT_POS          (0)
#define CHG_PROCHOT_INIT_POS        (1)
#define APU_RST_INIT_POS            (1)
#define APU_PWROK_INIT_POS          (0)
#define APU_THERMALTRIP_INIT_POS    (0)
#define APU_ALERT_INIT_POS          (1)
#define SLP_S3_INIT_POS             (1)
#define SLP_S5_INIT_POS             (1)
#define USB1_INT_INIT_POS           (1)
#define USB2_INT_INIT_POS           (1)
#define ALW_GD_INIT_POS             (0)
#define LIGHTING_SENSOR_INIT_POS    (1)  /* OPT4048 ALS interrupt active low */

#define DBG_LOG_FIFO_ENABLE         (1)

/* RPMC ROM and Capabilities configuration */
/** RPMC ROM version: 0 Debug version, >0 Release version (Note: Debug version and release version changes will erase RPMC ROM data after power on) */
#define RPMC_ROM_VERSION            (2)

/** RPMC ROM access mode: 0 Stable mode (default), 1 Performance mode */
#define RPMC_ROM_ACCESS_POSTPONE    (0)

/** Number of RPMC ROM monotonic counters: 0-3 Reserved for MC, 4-7 Reserved for FAR2.0 */
#define RPMC_ROM_MC_NUM             (8)

/**  RPMC ROM base address in internal flash */
#define RPMC_ROM_BASE_ADDR          (0x80000)

/** RPMC algorithm support bits for ECAIG (Valid options in RPMC driver include: RPMC_SHA256_MC_ALGID_BIT, RPMC_SHA3_384_MC_ALGID_BIT) */
#define RPMC_ECAIG_SUPPORT_BITS     (RPMC_SHA256_MC_ALGID_BIT)

#define SBRMI_SLV_ADDRESS_PKG0      (0x78 >> 1)
#define SYS_CONFIG_MAX_P3T_LIMIT    (216000)      // P3T max. 216W for Phx

/* SB-TSI -- I2C_0 */
#define SBTSI_I2C_PORT         	 	I2C_0
#define SBTSI_I2C_ADDRESS       	(0x4C)

/* LM95234 -- I2C_0 */
#define LM95234_I2C_PORT 			I2C_0
#define LM95234_I2C_ADDRESS 		(0x4E)

/* MP2815-SVI3  */
#define MP2815_I2C_PORT             I2C_1
#define MP2815_I2C_ADDRESS_ID0      (0x20)
#define SVRTABLE_CRCCHECK_UPDATE_ENABLE (0)

/* ALS OPT4048 */
#define OPT4048_I2C_PORT            I2C_1
#define OPT4048_I2C_ADDRESS_ID0     (0x44)

/* SFH SLAVE -- I2C_2 */
#define SFH_I2C_PORT   				I2C_2
#define SFH_I2C_SLAVE_ADDRESS      	(0x02)

/* BATTERY -- I2C_3 */
#define BATTERY_I2C_PORT 			I2C_3
#define BATTERY_I2C_ADDRESS 		(0x0B)

/* MP2764-CHARGER -- I2C_3 */
#define CHARGER_I2C_PORT        	I2C_3
#define CHARGER_I2C_ADDRESS     	(0x5C)

/* RTK PD -- I2C_3 */
extern bool g_rtk_romBootMode;
#define REALTEK_I2C_PORT   			I2C_3
#define I2C_REALTEK_ADDRESS_P0      ((g_rtk_romBootMode) ? 0x66 : 0x67) //(0x67)
#define I2C_REALTEK_ADDRESS_P1      (0x66)
#define I2C_REALTEK_ADDRESS_P2      ((g_rtk_romBootMode) ? 0x68 : 0x69) //(0x69)
#define I2C_REALTEK_ADDRESS_P3      (0x68)

/*
 * Smart battery
 */
#define APP_SMTBTY_DEBUG_ENABLE             (1)
#define MAX_BATTERY_SUPPORTED               (1)
#define APP_BATTERY_FULL_CHARGE_VOLTAGE     (9000)   /* 2-cell */
#define APP_BATTERY_MIN_BATTERY_VOLTAGE     (6000)   /* [13:6] 0x27C0; 3.4V/cell */

#define APP_BATTERY_REACTIVING_VOLTAGE      (8800)
#define APP_BATTERY_REACTIVING_CURRENT      (256)

#define APP_BATTERY_PRECHARGE_THRESHOLD     (0x30)
#define APP_BATTERY_PRECHARGE_VOLTAGE       (8800)
#define APP_BATTERY_PRECHARGE_CURRENT       (256)

#define APP_BATTERY_TERMINATION_CURRENT     (128)

#define APP_BATTERY_MAX_CHARGE_CURRENT      (4800)    /* 15~20C */
#define APP_BATTERY_MAX_DISCHARGE_CURRENT   (14400)

#define MAX_CHARGER_SUPPORTED           	(1)
#define APP_SMTBTY_CAPACITY_MODE_10MWH      (1)    /* 1: 10mWh; 0: mAh */
// Charger ID
#define PD_SINK   0
#define DEV_CHARGER_ID_TO_PORT(chgId)       (chgId & 0x01)

#define DEV_CHARGER_CURRENT_PORT            PD_SINK
#define DEV_CHARGER_SWITCH_TO_PORT(port)    do { } while (0)

#define MD_ACPI_HYPERPLANE_MAX_PLANES       (30)

/* smart prochot for DC mode hotplug PD source */
#define PD_DC_HOTPLUG_PROCHOT               (1)

/* able only has two USBC port and one PD controller */
#define PD_PORT_3_ENABLE                    (1)

/* GPIO port table */
extern uint32_t _s_autogen_PorInitTable[];

extern void board_uart_disable(void);
extern void board_uart_enable(void);

#define STANDBY_WITH_G3_POWER 0

#endif /* __MDSEFFP_NPCX4_H__ */
