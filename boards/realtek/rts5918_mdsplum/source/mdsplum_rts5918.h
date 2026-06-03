/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Per-board symbolic pin map for AMD MDS Plum hardware refreshed with
 * Realtek RTS5918 silicon. The macro NAMES match the original Plum
 * NPCX4 header (mdsplum_npcx4mnx.h) so AMD's app/ code that references
 * EC_PWR_BTN_N, EC_BLINK_N, SLP_S3_S0A3_N, etc. continues to compile.
 *
 * The VALUES are bring-up placeholders. Each macro currently encodes
 * (bank=0, pin=0) via EC_GPIO_PORT_PIN(RTK_GPIO_A, RTK_GPIO_00). Until
 * the Plum-on-RTS5918 BGA->pin map is delivered, every pin token
 * collapses to gpioa pin 0, which is safe for compile-time link but is
 * NOT correct for runtime — gpio_init() in drivers/realtek/
 * realtek_gpio_impl.c skips unmapped banks, and the conservative
 * defconfig leaves the dependent app modules disabled.
 *
 * TODO(realtek-schematic): each entry below tagged `/+TODO+/` must be
 * filled with the real (bank, pin) from the Plum->RTS5918 pinmap CSV
 * before the corresponding app subsystem can be re-enabled.
 */

#ifndef __MDSPLUM_RTS5918_H__
#define __MDSPLUM_RTS5918_H__

#include "gpio_ec.h"
#include "gpioAutoGen.h"  /* exposes Board_Gpio_AcpiHandler() to app/acpi/ */


#define GPIO_PIN_NULL  0xFFFFFFFFu



/* SLP semantic helpers used by app/power_sequencing/ */
#define SLP_S3_ASSERT       (!gpio_read_pin(SLP_S3_S0A3_N))
#define SLP_S5_ASSERT       (!gpio_read_pin(SLP_S5_N))
#define ASSERT_FCH_PWRBTN   gpio_write_pin(EC_PWR_BTN_N, 0)
#define DEASSERT_FCH_PWRBTN gpio_write_pin(EC_PWR_BTN_N, 1)

/* Debug / status LEDs */

#define EC_DEBUG_LED(x)    gpio_write_pin(EC_BLINK_N, !(x))
#define KBC_CAPS_LOCK      EC_CAP_LED_N
#define KBC_SCROLL_LOCK    EC_SCROLL_LED_N
#define KBC_NUM_LOCK       EC_NUM_LED_N




/* AMD-custom eSPI virtual-wire signals that NPCX4 gets via the AMD
 * kernel patch. Defined here (rather than in drivers/realtek/realtek_espi.h)
 * so any app translation unit that picks up board_config.h sees them
 * without an extra include of espi_hub.h. RTS5918 eSPI driver silently
 * drops signals outside its own switch table.
 */
#ifndef ESPI_VWIRE_SIGNAL_CPU_TEMP_READ
#define ESPI_VWIRE_SIGNAL_CPU_TEMP_READ  ((enum espi_vwire_signal)0x80)
#endif
#ifndef ESPI_VWIRE_SIGNAL_RTC_READ
#define ESPI_VWIRE_SIGNAL_RTC_READ       ((enum espi_vwire_signal)0x81)
#endif
/* drivers/espi_hub.c references three more AMD/Nuvoton-specific VW
 * signals (FL_ACK from the flash-access path, SLP_MSC for memory
 * controller sleep, SOC_LP for low-power SoC). Upstream 3.7.1's
 * enum espi_vwire_signal does not declare them.
 */
#ifndef ESPI_VWIRE_SIGNAL_FL_ACK
#define ESPI_VWIRE_SIGNAL_FL_ACK         ((enum espi_vwire_signal)0x82)
#endif
#ifndef ESPI_VWIRE_SIGNAL_SLP_MSC
#define ESPI_VWIRE_SIGNAL_SLP_MSC        ((enum espi_vwire_signal)0x83)
#endif
#ifndef ESPI_VWIRE_SIGNAL_SOC_LP
#define ESPI_VWIRE_SIGNAL_SOC_LP         ((enum espi_vwire_signal)0x84)
#endif


/*******************
 * APU Power OK
 *******************/
#define PIN_LIST_DRIVER_TO_HIGH_AFTER_APU_PWROK \
     EC_WLAN_AUX_RST_N,               EC_WWAN_AUX_RST_N,                 EC_LOM_AUX_RST_N,                   EC_SD_AUX_RST_N,                   \
     EC_EVAL_AUX_RST_N,               GPIO_PIN_NULL

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
#define PIN_LIST_DRIVER_TO_LOW_BEFORE_EC_SLP_S3_L_TO_HIGH \
     EC_EVAL_AUX_RST_N,               EC_EVAL_CARD_PWREN,      EC_EVAL_SLT_PWREN,    EC_EVAL_BOMACO_EN,             \
     EC_EVAL_19V_PWREN,                GPIO_PIN_NULL

/*
 * Device instance handles. Mirror the AMD app's vocabulary so
 * drivers/i2c_hub.c etc. resolve their device pointers via DEVICE_DT_GET.
 * The DT node-labels follow lenovo_p's rts5918_bison layout (i2c_0..i2c_4,
 * adc0, pwm0..pwmN, kbd, espi0, qspi_fiu1, bbram0, sha0); confirm against
 * the Plum DT overlay once the I2C topology is captured.
 *
 * TODO(realtek-schematic): each *_BUS_n or *_CH below maps to one DT
 * node label. If Plum's I2C topology lands controllers on different
 * RTS5918 buses than Bison did, retarget these labels.
 */
/*
 * RTS5918 numbers its I2C controllers from 1 (i2c_1..i2c_11). AMD's
 * Plum source uses the friendlier I2C_BUS_0..I2C_BUS_3 / I2C_0..I2C_3
 * indexing inherited from NPCX4. We map the AMD index to the
 * Realtek hardware controller as: AMD index N -> RTS5918 i2c_(N+1).
 * TODO(realtek-schematic): re-map after the Plum->RTS5918 I2C bus
 * assignment is confirmed (different sensor sets may want different
 * controllers).
 */
/*
 * RTS5918 binds the realtek,rts5918-i2c driver to the SCCON wrapper
 * nodes (i2c_N_wrapper), not to the bare i2c_N bus nodes. AMD app
 * code uses DEVICE_DT_GET against these macros, so they MUST point
 * to nodes the driver is bound to — otherwise the device handle is
 * not generated at compile time.
 *
 * The Bison overlay enables wrappers 1/3/4/5 (and 2 is dead-enabled);
 * make sure the AMD app's I2C_BUS_N indices land on wrappers that the
 * overlay actually `status = "okay"`s.
 */
#define I2C_BUS_0        DT_NODELABEL(i2c_1_wrapper)
#define I2C_BUS_1        DT_NODELABEL(i2c_2_wrapper)
#define I2C_BUS_2        DT_NODELABEL(i2c_3_wrapper)
#define I2C_BUS_3        DT_NODELABEL(i2c_4_wrapper)
#define I2C_BUS_4        DT_NODELABEL(i2c_5_wrapper)

/*
 * `i2c_bus_num` enum gives the per-board integer index that
 * drivers/i2c_hub.c's static `i2c_inst[]` array uses. The DT
 * `I2C_BUS_n` macros above feed DEVICE_DT_GET() at array-init time;
 * the `I2C_n` enum values below are what app/ passes as the
 * `uint8_t instance` argument to i2c_hub_config / write / read /
 * write_read.
 *
 * Keep the enum ordering aligned with i2c_inst[] so I2C_n correctly
 * indexes the corresponding DT_NODELABEL — otherwise an I2C call
 * routes to the wrong physical bus.
 */
enum i2c_bus_num {
	I2C_0,
	I2C_1,
	I2C_2,
	I2C_3,
	I2C_4,
};

#define ADC_CH_BASE      DT_NODELABEL(adc0)
#define KSCAN_MATRIX     DT_NODELABEL(kbd)
#define PWM_0            DT_NODELABEL(pwm0)
#define PWM_2            DT_NODELABEL(pwm2)
#define TACH_1           DT_NODELABEL(tach1)
/* RTS5918 has a single on-chip flash controller exposed as `flash0`.
 * The AMD app distinguishes INT_FLASH / EXT_FLASH (internal vs external
 * SPI flash) for the SAF path; on RTS5918 we collapse both onto the
 * same controller and let SAF-mode firmware decide partition layout.
 */
#define INT_FLASH        DT_NODELABEL(flash0)
#define EXT_FLASH        DT_NODELABEL(flash0)
#define EXT_FLASH1       DT_NODELABEL(flash0)
#define SPI_0            DT_NODELABEL(flash0)
#define NOR_FLASH        DT_NODELABEL(flash0)
#define ESPI_0           DT_NODELABEL(espi0)
#define BBRAM            DT_NODELABEL(bbram0)
#define UART_0           DT_NODELABEL(uart0)
#define UART_1           DT_NODELABEL(uart0)
#define UART_2           DT_NODELABEL(uart0)
/*
 * SHA crypto: NPCX4 has a hardware SHA engine routed through
 * sha_hmac_npcx.h. RTS5918 has no hardware SHA crypto block at this
 * scope. The OOB RPMC HMAC-SHA2 path (drivers/oob_rpmc_hmac_sha2_256.c)
 * is conditionally compiled out for RTS5918 in app/CMakeLists.txt.
 * SHA macro is defined as a sentinel so any compile-only reference
 * still resolves; runtime callers must check device_is_ready before
 * use.
 */
#define SHA              DT_INVALID_NODE

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

#define APP_BATTERY_MAX_CHARGE_CURRENT      (2428)    /* [12:2] 0x097C; 0.5C */
#define APP_BATTERY_MAX_DISCHARGE_CURRENT   (5000)

#define MAX_CHARGER_SUPPORTED           	(1)
#define F_AC_ADAPTER_VIN_VOLTAGE        	(19000)   /* 19000mV */
#define F_AC_ADAPTER_VIN_VOLTAGE_330        (19500)   /* 19000mV */
#define F_AC_ADAPTER_130W_MAX_CURRENT   	(6848)    /* 6848mA for 130W adapter */
#define F_AC_ADAPTER_150W_MAX_CURRENT   	(7872)   /* 7552mA@19V = 143.4W adapter / 7872mA@19V = 149.56W adapter */
#define F_AC_ADAPTER_330W_MAX_CURRENT  		(16900)   /* 16900mA@19V = 321W adapter */
#define APP_SMTBTY_CAPACITY_MODE_10MWH      (1)    /* 1: 10mWh; 0: mAh */
// Charger ID
#define AC_SINK   0
#define PD_SINK   1
#define DEV_CHARGER_ID_TO_PORT(chgId)    (chgId & 0x01)

#define DEV_CHARGER_CURRENT_PORT                 0
#define DEV_CHARGER_SWITCH_TO_PORT(port)         do { } while (0)

#define MD_ACPI_HYPERPLANE_MAX_PLANES   (30)

/* smart prochot for DC mode hotplug PD source */
#define PD_DC_HOTPLUG_PROCHOT           (1)
/* smart prochot for AC/PD switch */
#define PD_AC_HOTPLUG_PROCHOT           (1)

/* enable the PD 48V support */
#define PD_MODULE_48V_EN                (0)
/* AC adapter / battery limits used by power-source management. Bring-up
 * values are deliberately conservative; tune against Plum BOM later.
 */
#define F_AC_ADAPTER_130W_MAX_CURRENT   	(6848)    /* 6848mA for 130W adapter */
#define BOARD_AC_ADAPTER_WATT_MAX_DEFAULT  150u
#define BOARD_BATTERY_CELL_COUNT_DEFAULT   3u

#endif /* __MDSPLUM_RTS5918_H__ */
