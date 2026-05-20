/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Realtek RTS5918 mirror of drivers/nuvoton/gpio_ec.h.
 *
 * Keeps the same uint32_t (port << 8) | pin encoding so app/* code that
 * uses EC_GPIO_xx symbols stays unchanged. Only the underlying bank
 * count and per-port pin count differ from NPCX4.
 *
 * RTS5918 has 17 GPIO banks (A through Q). Each bank exposes 8 pins.
 * Banks are surfaced through the realtek,rts5918-gpio driver in
 * zephyr_base/zephyr_fork/drivers/gpio/gpio_rts5918.c
 *
 * TODO(realtek-schematic): the EC_GPIO_xx macros that resolve to a
 * concrete (bank, pin) pair live in boards/realtek/<board>/source/
 * <board>_rts5918.h and are populated from the schematic. Until then
 * those tokens are all zero and any gpio_read_pin/gpio_write_pin call
 * will operate on bank A pin 0 (harmless because gpio_init() leaves
 * unmapped pins disabled, but functionally a no-op).
 */

#ifndef __REALTEK_GPIO_EC_H__
#define __REALTEK_GPIO_EC_H__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include "realtek_gpio_ctrl.h"
#include <dt-bindings/gpio_defines.h>

/* Same encoding as nuvoton: high byte = port (bank), low byte = pin */
// #define EC_GPIO_PORT_POS	8U
// #define EC_GPIO_PORT_MASK	((uint32_t)0xfU << EC_GPIO_PORT_POS)
// #define EC_GPIO_PIN_POS		0U
// #define EC_GPIO_PIN_MASK	((uint32_t)0x1fU << EC_GPIO_PIN_POS)

#define EC_GPIO_PORT_PIN(_port, _pin)                                 \
	(((uint32_t)(_port) << EC_GPIO_PORT_POS) |                    \
	 ((uint32_t)(_pin)  << EC_GPIO_PIN_POS))

#define HIGH	1
#define LOW	0

/* RTS5918 GPIO bank identifiers (A..Q). Matches the gpioa..gpioq
 * DT node labels in dts/arm/realtek/ec/rts5918.dtsi. Keep the numeric
 * codes so the existing port_pin encoding (1 nibble per field) still
 * fits — banks 0xA..0xF reuse the digit form to keep the encoding
 * width identical to NPCX4 conventions.
 */
#define RTK_GPIO_A		0x0
#define RTK_GPIO_B		0x1
#define RTK_GPIO_C		0x2
#define RTK_GPIO_D		0x3
#define RTK_GPIO_E		0x4
#define RTK_GPIO_F		0x5
#define RTK_GPIO_G		0x6
#define RTK_GPIO_H		0x7
#define RTK_GPIO_I		0x8
#define RTK_GPIO_J		0x9
#define RTK_GPIO_K		0xA
#define RTK_GPIO_L		0xB
#define RTK_GPIO_M		0xC
#define RTK_GPIO_N		0xD
#define RTK_GPIO_O		0xE
#define RTK_GPIO_P		0xF
/*
 * TODO(realtek-schematic): RTS5918 also exposes bank Q. The 4-bit
 * EC_GPIO_PORT_MASK above does NOT cover 16 banks (A=0..P=15). If
 * any Plum signal lands on bank Q we have to widen EC_GPIO_PORT_MASK
 * or move bank Q to overflow into the high nibble. Defer until pin
 * map is in hand.
 */

#define RTK_GPIO_00	0x0
#define RTK_GPIO_01	0x1
#define RTK_GPIO_02	0x2
#define RTK_GPIO_03	0x3
#define RTK_GPIO_04	0x4
#define RTK_GPIO_05	0x5
#define RTK_GPIO_06	0x6
#define RTK_GPIO_07	0x7

/*
 * NUVO_GPIO_n / NUVO_GPIO_nn aliases kept for any AMD app source that
 * still references them by their original token name; they map onto the
 * Realtek banks 1:1. Remove once all call sites are renamed.
 */
#define NUVO_GPIO_0	RTK_GPIO_A
#define NUVO_GPIO_1	RTK_GPIO_B
#define NUVO_GPIO_2	RTK_GPIO_C
#define NUVO_GPIO_3	RTK_GPIO_D
#define NUVO_GPIO_4	RTK_GPIO_E
#define NUVO_GPIO_5	RTK_GPIO_F
#define NUVO_GPIO_6	RTK_GPIO_G
#define NUVO_GPIO_7	RTK_GPIO_H
#define NUVO_GPIO_8	RTK_GPIO_I
#define NUVO_GPIO_9	RTK_GPIO_J
#define NUVO_GPIO_A	RTK_GPIO_K
#define NUVO_GPIO_B	RTK_GPIO_L
#define NUVO_GPIO_C	RTK_GPIO_M
#define NUVO_GPIO_D	RTK_GPIO_N
#define NUVO_GPIO_E	RTK_GPIO_O
#define NUVO_GPIO_F	RTK_GPIO_P

#define NUVO_GPIO_00	RTK_GPIO_00
#define NUVO_GPIO_01	RTK_GPIO_01
#define NUVO_GPIO_02	RTK_GPIO_02
#define NUVO_GPIO_03	RTK_GPIO_03
#define NUVO_GPIO_04	RTK_GPIO_04
#define NUVO_GPIO_05	RTK_GPIO_05
#define NUVO_GPIO_06	RTK_GPIO_06
#define NUVO_GPIO_07	RTK_GPIO_07
#define NUVO_GPIO_10	RTK_GPIO_00
#define NUVO_GPIO_11	RTK_GPIO_01
#define NUVO_GPIO_12	RTK_GPIO_02
#define NUVO_GPIO_13	RTK_GPIO_03
#define NUVO_GPIO_14	RTK_GPIO_04
#define NUVO_GPIO_15	RTK_GPIO_05
#define NUVO_GPIO_16	RTK_GPIO_06
#define NUVO_GPIO_17	RTK_GPIO_07
#define NUVO_GPIO_20	RTK_GPIO_00
#define NUVO_GPIO_21	RTK_GPIO_01
#define NUVO_GPIO_22	RTK_GPIO_02
#define NUVO_GPIO_23	RTK_GPIO_03
#define NUVO_GPIO_24	RTK_GPIO_04
#define NUVO_GPIO_25	RTK_GPIO_05
#define NUVO_GPIO_26	RTK_GPIO_06
#define NUVO_GPIO_27	RTK_GPIO_07
#define NUVO_GPIO_30	RTK_GPIO_00
#define NUVO_GPIO_31	RTK_GPIO_01
#define NUVO_GPIO_32	RTK_GPIO_02
#define NUVO_GPIO_33	RTK_GPIO_03
#define NUVO_GPIO_34	RTK_GPIO_04
#define NUVO_GPIO_35	RTK_GPIO_05
#define NUVO_GPIO_36	RTK_GPIO_06
#define NUVO_GPIO_37	RTK_GPIO_07
#define NUVO_GPIO_40	RTK_GPIO_00
#define NUVO_GPIO_41	RTK_GPIO_01
#define NUVO_GPIO_42	RTK_GPIO_02
#define NUVO_GPIO_43	RTK_GPIO_03
#define NUVO_GPIO_44	RTK_GPIO_04
#define NUVO_GPIO_45	RTK_GPIO_05
#define NUVO_GPIO_46	RTK_GPIO_06
#define NUVO_GPIO_47	RTK_GPIO_07
#define NUVO_GPIO_50	RTK_GPIO_00
#define NUVO_GPIO_51	RTK_GPIO_01
#define NUVO_GPIO_52	RTK_GPIO_02
#define NUVO_GPIO_53	RTK_GPIO_03
#define NUVO_GPIO_54	RTK_GPIO_04
#define NUVO_GPIO_55	RTK_GPIO_05
#define NUVO_GPIO_56	RTK_GPIO_06
#define NUVO_GPIO_57	RTK_GPIO_07
#define NUVO_GPIO_60	RTK_GPIO_00
#define NUVO_GPIO_61	RTK_GPIO_01
#define NUVO_GPIO_62	RTK_GPIO_02
#define NUVO_GPIO_63	RTK_GPIO_03
#define NUVO_GPIO_64	RTK_GPIO_04
#define NUVO_GPIO_65	RTK_GPIO_05
#define NUVO_GPIO_66	RTK_GPIO_06
#define NUVO_GPIO_67	RTK_GPIO_07
#define NUVO_GPIO_70	RTK_GPIO_00
#define NUVO_GPIO_71	RTK_GPIO_01
#define NUVO_GPIO_72	RTK_GPIO_02
#define NUVO_GPIO_73	RTK_GPIO_03
#define NUVO_GPIO_74	RTK_GPIO_04
#define NUVO_GPIO_75	RTK_GPIO_05
#define NUVO_GPIO_76	RTK_GPIO_06
#define NUVO_GPIO_77	RTK_GPIO_07
#define NUVO_GPIO_80	RTK_GPIO_00
#define NUVO_GPIO_81	RTK_GPIO_01
#define NUVO_GPIO_82	RTK_GPIO_02
#define NUVO_GPIO_83	RTK_GPIO_03
#define NUVO_GPIO_84	RTK_GPIO_04
#define NUVO_GPIO_85	RTK_GPIO_05
#define NUVO_GPIO_86	RTK_GPIO_06
#define NUVO_GPIO_87	RTK_GPIO_07
#define NUVO_GPIO_90	RTK_GPIO_00
#define NUVO_GPIO_91	RTK_GPIO_01
#define NUVO_GPIO_92	RTK_GPIO_02
#define NUVO_GPIO_93	RTK_GPIO_03
#define NUVO_GPIO_94	RTK_GPIO_04
#define NUVO_GPIO_95	RTK_GPIO_05
#define NUVO_GPIO_96	RTK_GPIO_06
#define NUVO_GPIO_97	RTK_GPIO_07
#define NUVO_GPIO_A0	RTK_GPIO_00
#define NUVO_GPIO_A1	RTK_GPIO_01
#define NUVO_GPIO_A2	RTK_GPIO_02
#define NUVO_GPIO_A3	RTK_GPIO_03
#define NUVO_GPIO_A4	RTK_GPIO_04
#define NUVO_GPIO_A5	RTK_GPIO_05
#define NUVO_GPIO_A6	RTK_GPIO_06
#define NUVO_GPIO_A7	RTK_GPIO_07
#define NUVO_GPIO_B0	RTK_GPIO_00
#define NUVO_GPIO_B1	RTK_GPIO_01
#define NUVO_GPIO_B2	RTK_GPIO_02
#define NUVO_GPIO_B3	RTK_GPIO_03
#define NUVO_GPIO_B4	RTK_GPIO_04
#define NUVO_GPIO_B5	RTK_GPIO_05
#define NUVO_GPIO_B6	RTK_GPIO_06
#define NUVO_GPIO_B7	RTK_GPIO_07
#define NUVO_GPIO_C0	RTK_GPIO_00
#define NUVO_GPIO_C1	RTK_GPIO_01
#define NUVO_GPIO_C2	RTK_GPIO_02
#define NUVO_GPIO_C3	RTK_GPIO_03
#define NUVO_GPIO_C4	RTK_GPIO_04
#define NUVO_GPIO_C5	RTK_GPIO_05
#define NUVO_GPIO_C6	RTK_GPIO_06
#define NUVO_GPIO_C7	RTK_GPIO_07
#define NUVO_GPIO_D0	RTK_GPIO_00
#define NUVO_GPIO_D1	RTK_GPIO_01
#define NUVO_GPIO_D2	RTK_GPIO_02
#define NUVO_GPIO_D3	RTK_GPIO_03
#define NUVO_GPIO_D4	RTK_GPIO_04
#define NUVO_GPIO_D5	RTK_GPIO_05
#define NUVO_GPIO_D6	RTK_GPIO_06
#define NUVO_GPIO_D7	RTK_GPIO_07
#define NUVO_GPIO_E0	RTK_GPIO_00
#define NUVO_GPIO_E1	RTK_GPIO_01
#define NUVO_GPIO_E2	RTK_GPIO_02
#define NUVO_GPIO_E3	RTK_GPIO_03
#define NUVO_GPIO_E4	RTK_GPIO_04
#define NUVO_GPIO_E5	RTK_GPIO_05
#define NUVO_GPIO_E6	RTK_GPIO_06
#define NUVO_GPIO_E7	RTK_GPIO_07
#define NUVO_GPIO_F0	RTK_GPIO_00
#define NUVO_GPIO_F1	RTK_GPIO_01
#define NUVO_GPIO_F2	RTK_GPIO_02
#define NUVO_GPIO_F3	RTK_GPIO_03
#define NUVO_GPIO_F4	RTK_GPIO_04
#define NUVO_GPIO_F5	RTK_GPIO_05
#define NUVO_GPIO_F6	RTK_GPIO_06
#define NUVO_GPIO_F7	RTK_GPIO_07

#define GPIO_CTRL_PWRG_VTR_IO	0
#define GPIO_CTRL_PWRG_OFF	1

/* DT node name for gpios on POC */
#define GPIO_HUB_NODE_NAME poc_gpios

/* DT node name for gpio interrupt on POC */
#define GPIO_INT_HUB_NODE_NAME poc_gpio_ints

/**
 * @brief Get GPIO port and pin value from `gpios` property

 * @param node Node identifier.
 * @param array Array name contains gpio information
 * @return GPIO port and pin value
 */
#define GPIO_VAL_FROM_NODE(node, array) 					\
	((DT_PROP(DT_GPIO_CTLR(node, array), index) << EC_GPIO_PORT_POS)	\
	 | (DT_GPIO_PIN(node, array) << EC_GPIO_PIN_POS))

#if NPCK /* RTK modify: original N4SHT17W drop targeted nuvoton_npcx_gpio */
#define GPIO_VAL_FROM_UNDEF_NODE(node, array) 					\
	(((DT_PROP(DT_GPIO_CTLR(node, array), index) + 				\
	   DT_NUM_INST_STATUS_OKAY(nuvoton_npcx_gpio)) << EC_GPIO_PORT_POS)	\
	 | (DT_GPIO_PIN(node, array) << EC_GPIO_PIN_POS))
#else
#define GPIO_VAL_FROM_UNDEF_NODE(node, array) 					\
	(((DT_PROP(DT_GPIO_CTLR(node, array), index) + 				\
	   DT_NUM_INST_STATUS_OKAY(realtek_rts5918_gpio)) << EC_GPIO_PORT_POS) \
	 | (DT_GPIO_PIN(node, array) << EC_GPIO_PIN_POS))
#endif

#define GPIO_VAL_FROM_ARRAY(node, array) 					\
	GPIO_VAL_FROM_NODE(node, array) | \
	(DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_EXPANDER ? BIT(31) : 0) | \
	(DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_OUTPUT ? BIT(7) : 0) | \
	(DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_OUTPUT_INIT_HIGH ? BIT(6) : 0) | \
	((DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_OPEN_DRAIN) == GPIO_OPEN_DRAIN ? BIT(17) : 0) | \
	(DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_ACTIVE_LOW ? BIT(5) : 0) | \
	(DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_DEFINE ? BIT(21) : 0) | \
	(DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_VWIRE ? BIT(20) : 0) | \
	(DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_UNDEFINE_PIN ? BIT(30) : 0)
	// (DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_EXPANDER_ACTIVE_LOW ? BIT(5) : 0) | 
	// (DT_PROP(node, gpios_IDX_0_VAL_flags) & GPIO_EXPANDER_OPEN_DRAIN ? BIT(17) : 0)  | 


/**
 * @brief Macro function to construct a list of GPIO pin enumeration

 * @param node Node identifier.
 * @return Enumeration and its value which from GPIO port and pin
 */
#define GPIO_NAME_FROM_ENUM(node) 						\
	DT_STRING_TOKEN(node, enum_name) = GPIO_VAL_FROM_ARRAY(node, gpios)
	// DT_STRING_UPPER_TOKEN(node, enum_name) = GPIO_VAL_FROM_ARRAY(node, gpios)

/* Enumeration of GPIO Pins */
enum gpio_idx {
	DT_FOREACH_CHILD_SEP(DT_PATH(GPIO_HUB_NODE_NAME), GPIO_NAME_FROM_ENUM, (,)),
};

struct gpio_ec_config {
	uint32_t port_pin;
	uint32_t cfg;
	uint32_t pwr_cfg;
};

static inline uint32_t gpio_get_port(uint16_t pin)
{
	return ((pin & EC_GPIO_PORT_MASK) >> EC_GPIO_PORT_POS);
}

static inline uint32_t gpio_get_pin(uint16_t pin)
{
	return ((pin & EC_GPIO_PIN_MASK) >> EC_GPIO_PIN_POS);
}

uint32_t get_absolute_gpio_num(uint32_t port_pin);
int gpio_init(void);
int gpio_set_interrupt(uint32_t port_pin, uint32_t flag);
int gpio_configure_pin(uint32_t port_pin, gpio_flags_t flags);
int gpio_configure_array(struct gpio_ec_config *gpios, uint32_t len);
int gpio_write_pin(uint32_t port_pin, int value);
int gpio_read_pin(uint32_t port_pin);
int gpio_init_callback_pin(uint32_t port_pin,
			   struct gpio_callback *callback,
			   gpio_callback_handler_t handler);
int gpio_add_callback_pin(uint32_t port_pin, struct gpio_callback *callback);
int gpio_remove_callback_pin(uint32_t port_pin, struct gpio_callback *callback);
int gpio_force_configure_pin(uint32_t port_pin, gpio_flags_t flags);
int gpio_get_pending_interrupt_pin(uint32_t port_pin);


/**
 * @brief Configure all GPIO pins to their default states.
 *
 * @retval none.
 */
int gpio_configure_all_pins(void);

#endif /* __REALTEK_GPIO_EC_H__ */
