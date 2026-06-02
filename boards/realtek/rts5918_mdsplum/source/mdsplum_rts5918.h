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

#define PLATFORM_DATA(x, y) ((x) | ((y) << 8))

#define TODO_PIN  EC_GPIO_PORT_PIN(RTK_GPIO_A, RTK_GPIO_00)
#define GPIO_PIN_NULL  0xFFFFFFFFu

/* Power sequencing / SLP signals */
#define SLP_S3_S0A3_N      TODO_PIN  /* TODO(realtek-schematic) */
#define SLP_S5_N           TODO_PIN  /* TODO(realtek-schematic) */
#define APU_RST_N          TODO_PIN  /* TODO(realtek-schematic) */
#define APU_PWROK          TODO_PIN  /* TODO(realtek-schematic) */
#define EC_PWR_BTN_N       TODO_PIN  /* TODO(realtek-schematic) */

/* SLP semantic helpers used by app/power_sequencing/ */
#define SLP_S3_ASSERT       (!gpio_read_pin(SLP_S3_S0A3_N))
#define SLP_S5_ASSERT       (!gpio_read_pin(SLP_S5_N))
#define ASSERT_FCH_PWRBTN   gpio_write_pin(EC_PWR_BTN_N, 0)
#define DEASSERT_FCH_PWRBTN gpio_write_pin(EC_PWR_BTN_N, 1)

/* Debug / status LEDs */
#define EC_BLINK_N         TODO_PIN  /* TODO(realtek-schematic) */
#define EC_CAP_LED_N       TODO_PIN  /* TODO(realtek-schematic) */
#define EC_SCROLL_LED_N    TODO_PIN  /* TODO(realtek-schematic) */
#define EC_NUM_LED_N       TODO_PIN  /* TODO(realtek-schematic) */
#define EC_DEBUG_LED(x)    gpio_write_pin(EC_BLINK_N, !(x))
#define KBC_CAPS_LOCK      EC_CAP_LED_N
#define KBC_SCROLL_LOCK    EC_SCROLL_LED_N
#define KBC_NUM_LOCK       EC_NUM_LED_N

/* Peripheral auxiliary reset lines */
#define EC_WLAN_AUX_RST_N  TODO_PIN  /* TODO(realtek-schematic) */
#define EC_WWAN_AUX_RST_N  TODO_PIN  /* TODO(realtek-schematic) */
#define EC_LOM_AUX_RST_N   TODO_PIN  /* TODO(realtek-schematic) */
#define EC_SD_AUX_RST_N    TODO_PIN  /* TODO(realtek-schematic) */
#define EC_EVAL_AUX_RST_N  TODO_PIN  /* TODO(realtek-schematic) */
#define EC_EVAL_CARD_PWREN TODO_PIN  /* TODO(realtek-schematic) */
#define EC_EVAL_SLT_PWREN  TODO_PIN  /* TODO(realtek-schematic) */
#define EC_EVAL_BOMACO_EN  TODO_PIN  /* TODO(realtek-schematic) */
#define EC_EVAL_19V_PWREN  TODO_PIN  /* TODO(realtek-schematic) */

/* Charger / power-supply signals */
#define CHG_ACOK           TODO_PIN  /* TODO(realtek-schematic) */
#define CHG_BATT_DETECT    TODO_PIN  /* TODO(realtek-schematic) */
#define CHG_PROCHOT_N      TODO_PIN  /* TODO(realtek-schematic) */

/* Wireless / MPM event signals */
#define MPM_EVENT_N        TODO_PIN  /* TODO(realtek-schematic) */
#define WLAN_PD_N          TODO_PIN  /* TODO(realtek-schematic) */
#define WWAN_PD_N          TODO_PIN  /* TODO(realtek-schematic) */

/* Power-sequencing signals referenced from app/power_sequencing */
#define APU_THERMTRIP_N    TODO_PIN  /* TODO(realtek-schematic) */
#define CHG_EC_PROCHOT_N   TODO_PIN  /* TODO(realtek-schematic) */
#define EC_APU_PROCHOT_N   TODO_PIN  /* TODO(realtek-schematic) */
#define EC_1V8_S5_EN       TODO_PIN  /* TODO(realtek-schematic) */
#define EC_S5_PWREN        TODO_PIN  /* TODO(realtek-schematic) */
#define EC_SLP_S3_S0A3_N   SLP_S3_S0A3_N  /* alias to SLP_S3_S0A3_N */
#define EC_SLP_S5_N        SLP_S5_N       /* alias to SLP_S5_N */
#define RSMRST_N           TODO_PIN  /* TODO(realtek-schematic) */
#define SYSTEM_S5_PG       TODO_PIN  /* TODO(realtek-schematic) */
#define EC_APU_SCI_N       TODO_PIN  /* TODO(realtek-schematic) */

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

/* eSPI signals — these live on the RTS5918 eSPI pinctrl group; the
 * tokens below are kept for any explicit gpio_write_pin reference but
 * must remain in pinctrl mode under normal operation.
 */
#define ESPI_EC_ALERT_N    TODO_PIN  /* TODO(realtek-schematic) */
#define ESPI_EC_CLK        TODO_PIN  /* TODO(realtek-schematic) */
#define ESPI_EC_CS_N       TODO_PIN  /* TODO(realtek-schematic) */
#define ESPI_EC_D0         TODO_PIN  /* TODO(realtek-schematic) */
#define ESPI_EC_D1         TODO_PIN  /* TODO(realtek-schematic) */
#define ESPI_EC_D2         TODO_PIN  /* TODO(realtek-schematic) */
#define ESPI_EC_D3         TODO_PIN  /* TODO(realtek-schematic) */
#define ESPI_EC_RESET_N    TODO_PIN  /* TODO(realtek-schematic) */

/* Power sequencing pin lists — empty placeholders, terminated by
 * GPIO_PIN_NULL so power_sequencing iteration safely no-ops until
 * filled in from the schematic.
 */
#define PIN_LIST_DRIVER_TO_HIGH_AFTER_APU_PWROK \
	GPIO_PIN_NULL
#define PIN_LIST_ESPI_SIGNALES_TO_PWRON \
	ESPI_EC_ALERT_N, ESPI_EC_CLK, ESPI_EC_CS_N, ESPI_EC_D0, \
	ESPI_EC_D1, ESPI_EC_D2, ESPI_EC_D3, ESPI_EC_RESET_N, \
	GPIO_PIN_NULL
#define PIN_LIST_DRIVER_TO_LOW_BEFORE_EC_SLP_S3_L_TO_HIGH \
	GPIO_PIN_NULL

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

/* AC adapter / battery limits used by power-source management. Bring-up
 * values are deliberately conservative; tune against Plum BOM later.
 */
#define BOARD_AC_ADAPTER_WATT_MAX_DEFAULT  150u
#define BOARD_BATTERY_CELL_COUNT_DEFAULT   3u

#endif /* __MDSPLUM_RTS5918_H__ */
