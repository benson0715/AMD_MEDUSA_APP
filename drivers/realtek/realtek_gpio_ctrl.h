/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mirror of drivers/nuvoton/nuvoton_gpio_ctrl.h: keeps the MD_GPIO_*
 * encoding the AMD app code uses for GPIO attribute bit-packing.
 *
 * The bit positions intentionally match the NPCX4 layout so the AMD
 * auto-gen tables (board_ioexpTable.h, gpioAutoGen.c) compile against
 * the same numeric literals. Mapping these bits to actual RTS5918
 * pinctrl/GPIO controller register writes is done inside
 * external_lib/script/build/gpio_rts5918.c.
 */

#ifndef __REALTEK_GPIO_CTRL_H__
#define __REALTEK_GPIO_CTRL_H__

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <assert.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/sys_io.h>

#define MD_GPIO_PAD_NONE		0x00ul
#define MD_GPIO_PAD_PULLUP		0x01ul
#define MD_GPIO_PAD_PULLDOWN		0x02ul
#define MD_GPIO_PAD_KEEPER		0x03ul

typedef enum {
	MD_GPIO_EM_PAD__None = 0,
	MD_GPIO_EM_PAD__PullUp = 1,
	MD_GPIO_EM_PAD__PullDown = 2,
	MD_GPIO_EM_PAD__Keeper = 3,
} MD_GPIO_EM_PAD;

#define MD_GPIO_POWER_VTR		(0x00ul << 2)
#define MD_GPIO_POWER_VCC		(0x01ul << 2)
#define MD_GPIO_POWER_UNPOWERED		(0x02ul << 2)

typedef enum {
	MD_GPIO_EM_POWER__VTR = 0,
	MD_GPIO_EM_POWER__VccMain = 1,
	MD_GPIO_EM_POWER__Unpowered = 2,
	MD_GPIO_EM_POWER__Reserved = 3,
} MD_GPIO_EM_POWER;

#define MD_GPIO_INTERRUPT_LOW_LEVEL	((0x00ul + 0x00ul) << 4)
#define MD_GPIO_INTERRUPT_HIGH_LEVEL	((0x00ul + 0x01ul) << 4)
#define MD_GPIO_INTERRUPT_DISABLED	((0x00ul + 0x04ul) << 4)
#define MD_GPIO_INTERRUPT_RISING_EDGE	((0x08ul + 0x05ul) << 4)
#define MD_GPIO_INTERRUPT_FALLING_EDGE	((0x08ul + 0x06ul) << 4)
#define MD_GPIO_INTERRUPT_EITHER_EDGE	((0x08ul + 0x07ul) << 4)

typedef enum {
	MD_GPIO_EM_INTERRUPT__LowLevel = 0,
	MD_GPIO_EM_INTERRUPT__HighLevel = 1,
	MD_GPIO_EM_INTERRUPT__Disabled = 4,
	MD_GPIO_EM_INTERRUPT__RisingEdge = 5,
	MD_GPIO_EM_INTERRUPT__FallingEdge = 6,
	MD_GPIO_EM_INTERRUPT__EitherEdge = 7,
} MD_GPIO_EM_INTERRUPT;

#define MD_GPIO_BUF_TYPE_PP		(0x00ul << 8)
#define MD_GPIO_BUF_TYPE_OD		(0x01ul << 8)
#define MD_GPIO_BUF_TYPE_MASK		(0x01ul << 8)

typedef enum {
	MD_GPIO_EM_BUF_TYPE__PushPull = 0,
	MD_GPIO_EM_BUF_TYPE__OpenDrain = 1,
} MD_GPIO_EM_BUF_TYPE;

#define MD_GPIO_DIRECTION_INPUT		(0x00ul << 9)
#define MD_GPIO_DIRECTION_OUTPUT	(0x01ul << 9)
#define MD_GPIO_DIRECTION_MASK		(0x01ul << 9)

typedef enum {
	MD_GPIO_EM_DIRECTION__Input = 0,
	MD_GPIO_EM_DIRECTION__Output = 1,
} MD_GPIO_EM_DIRECTION;

#define MD_GPIO_OUTPUT_SELECT_PINCTRL16	(0x00ul << 10)
#define MD_GPIO_OUTPUT_SELECT_OUTPUTREG	(0x01ul << 10)

typedef enum {
	MD_GPIO_EM_OUTPUT_SELECT__PinCtrl16 = 0,
	MD_GPIO_EM_OUTPUT_SELECT__OutputReg = 1,
} MD_GPIO_EM_OUTPUT_SELECT;

#define MD_GPIO_MUX_GPIO		(0x00ul << 12)
#define MD_GPIO_MUX_FUN1		(0x01ul << 12)
#define MD_GPIO_MUX_FUN2		(0x02ul << 12)
#define MD_GPIO_MUX_FUN3		(0x03ul << 12)

typedef enum {
	MD_GPIO_EM_MUX__GPIO = 0,
	MD_GPIO_EM_MUX__Fun1 = 1,
	MD_GPIO_EM_MUX__Fun2 = 2,
	MD_GPIO_EM_MUX__Fun3 = 3,
} MD_GPIO_EM_MUX;

#define MD_GPIO_POLARITY_NON_INVERTED	(0x00ul << 11)
#define MD_GPIO_POLARITY_INVERTED	(0x01ul << 11)

#define MD_GPIO_OUTPUT_LOW		(0x00ul << 16)
#define MD_GPIO_OUTPUT_HIGH		(0x01ul << 16)

#define MD_GPIO_INPUT_MASK		(0x01ul << 24)

#endif /* __REALTEK_GPIO_CTRL_H__ */
