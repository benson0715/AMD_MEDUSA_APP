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

#define BRD_MDS_DORNE

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

/*******************
 * SLP S3 high
 *******************/
#define PIN_LIST_DRIVER_TO_LOW_BEFORE_EC_SLP_S3_L_TO_HIGH GPIO_PIN_NULL

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
#define UART_1              DT_NODELABEL(uart1)
#define PWM_0               DT_NODELABEL(pwm0)
#define PWM_2               DT_NODELABEL(pwm2)
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
#define PWR_BTN_INIT_POS		           (1)
#define AC_PRSENT_INIT_POS                 (1)
#define UART_DONGLE_PRSENT_INIT_POS        (1)
#define UART0_INTn_INIT_POS                (1)
#define SMIn_INTn_INIT_POS                 (1)
#define BAT_IN_INIT_POS                    (1)
#define CHG_OK_INIT_POS                    (0)
#define DOCK_IN_INIT_POS                   (1)
#define LID_INIT_POS                       (1)
#define DISP_SW_INIT_POS                   (0)
#define DBG_CARD_INIT_POS                  (1)
#define AC_330W_PRSNT_INIT_POS             (0)
#define CHG_PROCHOTn1_INIT_POS             (1)
#define CHG_PROCHOTn2_INIT_POS             (1)
#define DC_S5_ALW_INIT_POS                 (0)
#define PD_SRC_ON_INIT_POS                 (1)
#define CHG_PROCHOT_INIT_POS               (1)
#define APU_RST_INIT_POS                   (1)
#define APU_PWROK_INIT_POS                 (0)
#define APU_THERMALTRIP_INIT_POS           (0)
#define APU_ALERT_INIT_POS                 (1)
#define SLP_S3_INIT_POS                    (1)
#define SLP_S5_INIT_POS                    (1)
#define USB1_INT_INIT_POS                  (1)
#define USB2_INT_INIT_POS                  (1)
#define IOEXP0_INIT_POS                    (1)
#define ALW_GD_INIT_POS                    (0)
#define CAM_DET_INIT_POS                   (0)
#define EXT_TALERT_INIT_POS                (1)

#define DBG_LOG_FIFO_ENABLE                (1)
#define DISP_SW_DEBUG_ALWAYS               (0)

#define MPC_SAME_POSTCODE_RETRY_COUNT      (1) // Dorne doesn't support mpc postcode

/* RPMC ROM and Capabilities configuration */
/** RPMC ROM version: 0 Debug version, >0 Release version (Note: Debug version and release version changes will erase RPMC ROM data after power on) */
#define RPMC_ROM_VERSION                   (2)

/** RPMC ROM access mode: 0 Stable mode (default), 1 Performance mode */
#define RPMC_ROM_ACCESS_POSTPONE           (0)

/** Number of RPMC ROM monotonic counters: 0-3 Reserved for MC, 4-7 Reserved for FAR2.0 */
#define RPMC_ROM_MC_NUM                    (8)

/**  RPMC ROM base address in internal flash */
#define RPMC_ROM_BASE_ADDR                 (0x80000)

/** RPMC algorithm support bits for ECAIG (Valid options in RPMC driver include: RPMC_SHA256_MC_ALGID_BIT, RPMC_SHA3_384_MC_ALGID_BIT) */
#define RPMC_ECAIG_SUPPORT_BITS            (RPMC_SHA256_MC_ALGID_BIT)

#define SBRMI_SLV_ADDRESS_PKG0             (0x78 >> 1)
#define SYS_CONFIG_MAX_P3T_LIMIT           (216000)      // P3T max. 216W for Phx

/* IOEXP  */
#define DEV_PCA9535_MAX_CHIP_NUM    7
#define IOEXP_SETBIT            	dev_pca9535_setBit
#define IOEXP_SETOUTPUT         	dev_pca9535_setOutput
#define IOEXP_SMARTREAD         	dev_pca9535_smartRead
#define IOEXP_SETMODE             dev_pca9535_setPuOd

/* MP2815-SVI3  */
#define MP2815_I2C_PORT           I2C_1
#define MP2815_I2C_ADDRESS_ID0    (0x20)
#define SVRTABLE_CRCCHECK_UPDATE_ENABLE (0)

/* CPLD -- I2C_1 */
#define CPLD_I2C_PORT           	I2C_1
#define SCAN_I2C_PORT          	    I2C_1

/* INA219 -- I2C_1 */
#define INA219_I2C_PORT         	I2C_1
#define INA219_I2C_ADDRESS      	(0x40)

/* ISL9241-CHARGER -- I2C_3 */
#define CHARGER_I2C_PORT        	I2C_3
#define CHARGER_I2C_ADDRESS     	(0x09)

/* SB-TSI -- I2C_0 */
#define SBTSI_I2C_PORT         	 	I2C_0
#define SBTSI_I2C_ADDRESS       	(0x4C)

/* BATTERY -- I2C_3 */
#define BATTERY_I2C_PORT 			I2C_3
#define BATTERY_I2C_ADDRESS 		(0x0B)

/* TMP432 -- I2C_0 */
#define TMP432_I2C_PORT 			I2C_0
#define TMP432_I2C_ADDRESS 			(0x4D)

/* LM95234 -- I2C_0 */
#define LM95234_I2C_PORT 			I2C_0
#define LM95234_I2C_ADDRESS 		(0x4E)

/* MAX695X-Postcode -- I2C_1 */
#define MAX695X_I2C_PORT 			I2C_1
#define MAX695X_LOW_I2C_ADDRESS 	(0x38)
#define MAX695X_HIGH_I2C_ADDRESS 	(0x39)

/* MPC-Postcode -- I2C_1 */
#define MPC_I2C_PORT 				I2C_1
#define MPC_I2C_ADDRESS 			(0x37)

/* HPI PD -- I2C_3 */
#define CCG8_CFP_ENABLE             (0)
#define HPI_I2C_PORT          		I2C_3
#if CCG8_CFP_ENABLE
#define I2C_HPI_ADDRESS_ID1    		(0x40)
#define I2C_HPI_ADDRESS_ID2    		(0x42)
#else
#define I2C_HPI_ADDRESS_ID1    		(0x08)
#define I2C_HPI_ADDRESS_ID2    		(0x40)
#endif

/* ITE PD -- I2C_3 */
#define ITE_I2C_PORT          		I2C_3
#define I2C_ITE_ADDRESS_P0          (0x4C)
#define I2C_ITE_ADDRESS_P1          (0x4C)
#define I2C_ITE_ADDRESS_P2          (0x26)

/* TI PD -- I2C_3 */
#define TPS_I2C_PORT   				I2C_3
#define I2C_4CC_ADDRESS_P0      	(0x20)
#define I2C_4CC_ADDRESS_P1      	(0x24)
#define I2C_4CC_ADDRESS_P2      	(0x22)

/* SFH SLAVE -- I2C_2 */
#define SFH_I2C_PORT   				I2C_2
#define SFH_I2C_SLAVE_ADDRESS      	(0x02)

/* board id -- I2C_3 */
#define EEPROM_DRIVER_I2C_PORT      (I2C_3)
#define EEPROM_DRIVER_I2C_ADDR      (0x54)

/* DAC -- I2C_3 */
#define I2C_DAC_PWR_PORT            (I2C_3)

/* EVAL CARD -- I2C_1 */
#define I2C_EVAL_PORT            (I2C_1)

/*
 * Smart battery
 */
#define APP_SMTBTY_DEBUG_ENABLE             (1)
#define MAX_BATTERY_SUPPORTED               (1)
#define APP_BATTERY_NO_BATT_CHARGE_VOLTAGE  (12000)   // If there is no battery adjust the charger output voltage to 12V
#define APP_BATTERY_FULL_CHARGE_VOLTAGE     (13200)   /* 3-cell */
#define APP_BATTERY_MIN_BATTERY_VOLTAGE     (10176)   /* [13:6] 0x27C0; 3.4V/cell */

#define APP_BATTERY_REACTIVING_VOLTAGE      (13200)
#define APP_BATTERY_REACTIVING_CURRENT      (256)

#define APP_BATTERY_PRECHARGE_THRESHOLD     (0x30)
#define APP_BATTERY_PRECHARGE_VOLTAGE       (13200)
#define APP_BATTERY_PRECHARGE_CURRENT       (512)

#define APP_BATTERY_MAX_CHARGE_CURRENT      (3248)    /* 0.7C */
#define APP_BATTERY_MAX_DISCHARGE_CURRENT   (5000)

#define MAX_CHARGER_SUPPORTED           	(1)
#define F_AC_ADAPTER_VIN_VOLTAGE        	(19000)   /* 19000mV */
#define F_AC_ADAPTER_VIN_VOLTAGE_330        (19500)   /* 19000mV */
#define F_AC_ADAPTER_130W_MAX_CURRENT   	(6848)    /* 6848mA for 130W adapter */
#define F_AC_ADAPTER_150W_MAX_CURRENT   	(7872)   /* 7552mA@19V = 143.4W adapter / 7872mA@19V = 149.56W adapter */
#define F_AC_ADAPTER_330W_MAX_CURRENT  		(16900)   /* 16900mA@19V = 321W adapter */
#define APP_SMTBTY_CAPACITY_MODE_10MWH      (1)    /* 1: 10mWh; 0: mAh */
// Charger ID
#define PD_SINK   0
#define DEV_CHARGER_ID_TO_PORT(chgId)    (chgId & 0x01)

#define DEV_CHARGER_CURRENT_PORT                 PD_SINK
#define DEV_CHARGER_SWITCH_TO_PORT(port)         do { } while (0)

#define MD_ACPI_HYPERPLANE_MAX_PLANES   (30)

/* smart prochot for DC mode hotplug PD source */
#define PD_DC_HOTPLUG_PROCHOT           (1)

/* eFFP only has two USBC port and one PD controller */
#define PD_PORT_3_ENABLE                (0)

/* GPIO port table */
extern uint32_t _s_autogen_PorInitTable[];

extern void board_uart_disable(void);

extern void board_uart_enable(void);
#define STANDBY_WITH_G3_POWER 0

#endif /* __MDSEFFP_NPCX4_H__ */
