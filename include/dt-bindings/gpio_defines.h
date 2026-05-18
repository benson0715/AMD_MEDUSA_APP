/*
 * Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef DT_BINDINGS_GPIO_DEFINES_H_
#define DT_BINDINGS_GPIO_DEFINES_H_

#include <zephyr/dt-bindings/gpio/gpio.h>

/*
 * Pin encoding used by middle/gpio/mdl_gpio_hub.c. The middle layer
 * packs (port, pin) into a 32-bit value via:
 *
 *   value = (port << EC_GPIO_PORT_POS) | (pin << EC_GPIO_PIN_POS)
 *
 * Bit layout (matches Lenovo Port.h IO_NAME_DEF convention):
 *   - bits  0..4   pin  index inside the controller (max 32 pins)
 *   - bits  5..7   reserved for flag overlay (GPIO_ACTIVE_LOW = BIT(5),
 *                  GPIO_OUTPUT_INIT_HIGH = BIT(6), GPIO_OUTPUT = BIT(7))
 *   - bits  8..15  port index of the controller    (max 256 controllers)
 *   - bits 16..31  reserved for further flags (set by mdl_gpio_hub.h
 *                  GPIO_VAL_FROM_ARRAY: GPIO_OPEN_DRAIN = BIT(17),
 *                  GPIO_VWIRE = BIT(20), GPIO_DEFINE = BIT(21),
 *                  GPIO_UNDEFINE_PIN = BIT(30), GPIO_EXPANDER = BIT(31))
 *
 * Phase 9.2: EC_GPIO_PIN_MASK was originally 0xFF but bit 7 is the
 * GPIO_OUTPUT flag in GPIO_VAL_FROM_ARRAY. Any pin in an output-direction
 * entry would extract as `pin | 0x80` and gpio_pin_set() would target
 * a non-existent pin. Shrink to 0x1F (5 bits) to match the legacy
 * Lenovo IoExtractBitNumber convention. RTS5918 GPIO banks have
 * 8 pins each (0-7) and the largest virtual port-byte expander has
 * 8 pins each, both well within 5 bits.
 */
#define EC_GPIO_PIN_POS    0u
#define EC_GPIO_PORT_POS   8u
#define EC_GPIO_PIN_MASK   (0x1Fu << EC_GPIO_PIN_POS)
#define EC_GPIO_PORT_MASK  (0xFFu << EC_GPIO_PORT_POS)

/*
 * The GPIO_INPUT and GPIO_OUTPUT defines are normally not available to
 * the device tree. For GPIOs that are controlled by the platform/ec module, we
 * allow device tree to set the initial state.
 *
 * Note the raw defines (e.g. GPIO_OUTPUT) in this file are copies from
 * <drivers/gpio.h>
 *
 * The combined defined (e.g. GPIO_OUT_LOW) have been renamed to fit with
 * gpio defined in platform/ec codebase.
 */

/** Enables pin as input. */
#define GPIO_INPUT (1U << 16)

/** Enables pin as output, no change to the output state. */
#define GPIO_OUTPUT (1U << 17)

/* Initializes output to a low state. */
#define GPIO_OUTPUT_INIT_LOW (1U << 18)

/* Initializes output to a high state. */
#define GPIO_OUTPUT_INIT_HIGH (1U << 19)

/* Initializes output based on logic level */
#define GPIO_OUTPUT_INIT_LOGICAL (1U << 20)

/* Configures GPIO pin as output and initializes it to a low state. */
#define GPIO_OUTPUT_LOW (GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW)

/* Configures GPIO pin as output and initializes it to a high state. */
#define GPIO_OUTPUT_HIGH (GPIO_OUTPUT | GPIO_OUTPUT_INIT_HIGH)

/* Configures GPIO pin as input with pull-up. */
#define GPIO_INPUT_PULL_UP (GPIO_INPUT | GPIO_PULL_UP)

/* Configures GPIO pin as input with pull-down. */
#define GPIO_INPUT_PULL_DOWN (GPIO_INPUT | GPIO_PULL_DOWN)

/** Configures GPIO pin as ODR output and initializes it to a low state. */
#define GPIO_ODR_LOW (GPIO_OUTPUT_LOW | GPIO_OPEN_DRAIN)

/** Configures GPIO pin as ODR output and initializes it to a high state. */
#define GPIO_ODR_HIGH (GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN)

/*
 * GPIO interrupt flags, taken from <drivers/gpio.h>
 *
 * TODO: Remove these once upstream changes to make these flags
 * available lands.
 */

/** Disables GPIO pin interrupt. */
#define GPIO_INT_DISABLE (1U << 21)

/* Enables GPIO pin interrupt. */
#define GPIO_INT_ENABLE (1U << 22)

/* GPIO interrupt is sensitive to logical levels.
 *
 * This is a component flag that should be combined with other
 * `GPIO_INT_*` flags to produce a meaningful configuration.
 */
#define GPIO_INT_LEVELS_LOGICAL (1U << 23)

/* GPIO interrupt is edge sensitive.
 *
 * Note: by default interrupts are level sensitive.
 *
 * This is a component flag that should be combined with other
 * `GPIO_INT_*` flags to produce a meaningful configuration.
 */
#define GPIO_INT_EDGE (1U << 24)

/* Trigger detection when input state is (or transitions to) physical low or
 * logical 0 level.
 *
 * This is a component flag that should be combined with other
 * `GPIO_INT_*` flags to produce a meaningful configuration.
 */
#define GPIO_INT_LOW_0 (1U << 25)

/* Trigger detection on input state is (or transitions to) physical high or
 * logical 1 level.
 *
 * This is a component flag that should be combined with other
 * `GPIO_INT_*` flags to produce a meaningful configuration.
 */
#define GPIO_INT_HIGH_1 (1U << 26)

// added for expander IO by Vernon >>
/** to mark that the pin is a expander IO port*/
#define GPIO_EXPANDER (1U << 27)

/** to mark that the pin is a GPIO define of chip */
#define GPIO_DEFINE (1U << 28)

/** to mark that the pin is a vitual wire IO pin */
#define GPIO_VWIRE (1U << 29)

/** to mark that the pin is a vitual wire IO pin */
#define GPIO_UNDEFINE_PIN (1U << 30)
// <<

/** Configures GPIO interrupt to be triggered on pin rising edge and enables it.
 */
#define GPIO_INT_EDGE_RISING (GPIO_INT_ENABLE | GPIO_INT_EDGE | GPIO_INT_HIGH_1)

/** Configures GPIO interrupt to be triggered on pin falling edge and enables
 * it.
 */
#define GPIO_INT_EDGE_FALLING (GPIO_INT_ENABLE | GPIO_INT_EDGE | GPIO_INT_LOW_0)

/** Configures GPIO interrupt to be triggered on pin rising or falling edge and
 * enables it.
 */
#define GPIO_INT_EDGE_BOTH \
	(GPIO_INT_ENABLE | GPIO_INT_EDGE | GPIO_INT_LOW_0 | GPIO_INT_HIGH_1)

/** Configures GPIO interrupt to be triggered on pin physical level low and
 * enables it.
 */
#define GPIO_INT_LEVEL_LOW (GPIO_INT_ENABLE | GPIO_INT_LOW_0)

/** Configures GPIO interrupt to be triggered on pin physical level high and
 * enables it.
 */
#define GPIO_INT_LEVEL_HIGH (GPIO_INT_ENABLE | GPIO_INT_HIGH_1)

/** Configures GPIO interrupt to be triggered on pin state change to logical
 * level 0 and enables it.
 */
#define GPIO_INT_EDGE_TO_INACTIVE                                    \
	(GPIO_INT_ENABLE | GPIO_INT_LEVELS_LOGICAL | GPIO_INT_EDGE | \
	 GPIO_INT_LOW_0)

/** Configures GPIO interrupt to be triggered on pin state change to logical
 * level 1 and enables it.
 */
#define GPIO_INT_EDGE_TO_ACTIVE                                      \
	(GPIO_INT_ENABLE | GPIO_INT_LEVELS_LOGICAL | GPIO_INT_EDGE | \
	 GPIO_INT_HIGH_1)

/** Configures GPIO interrupt to be triggered on pin logical level 0 and enables
 * it.
 */
#define GPIO_INT_LEVEL_INACTIVE \
	(GPIO_INT_ENABLE | GPIO_INT_LEVELS_LOGICAL | GPIO_INT_LOW_0)

/** Configures GPIO interrupt to be triggered on pin logical level 1 and enables
 * it.
 */
#define GPIO_INT_LEVEL_ACTIVE \
	(GPIO_INT_ENABLE | GPIO_INT_LEVELS_LOGICAL | GPIO_INT_HIGH_1)

#endif /* DT_BINDINGS_GPIO_DEFINES_H_ */
