/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __GPIOCTRL_H__
#define __GPIOCTRL_H__
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <assert.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/sys_io.h>
#include <soc.h>


#define MD_GPIO_PAD_NONE				0x00ul
#define MD_GPIO_PAD_PULLUP				0x01ul
#define MD_GPIO_PAD_PULLDOWN			0x02ul
#define MD_GPIO_PAD_KEEPER				0x03ul

typedef enum {
	MD_GPIO_EM_PAD__None = 0,
	MD_GPIO_EM_PAD__PullUp = 1,
	MD_GPIO_EM_PAD__PullDown = 2,
	MD_GPIO_EM_PAD__Keeper = 3,
} MD_GPIO_EM_PAD;

#define MD_GPIO_POWER_VTR				(0x00ul << 2)
#define MD_GPIO_POWER_VCC				(0x01ul << 2)
#define MD_GPIO_POWER_UNPOWERED			(0x02ul << 2)

typedef enum {
	MD_GPIO_EM_POWER__VTR = 0,
	MD_GPIO_EM_POWER__VccMain = 1,
	MD_GPIO_EM_POWER__Unpowered = 2,
	MD_GPIO_EM_POWER__Reserved = 3,
} MD_GPIO_EM_POWER;

#define MD_GPIO_INTERRUPT_LOW_LEVEL		((0x00ul + 0x00ul) << 4)
#define MD_GPIO_INTERRUPT_HIGH_LEVEL	((0x00ul + 0x01ul) << 4)
#define MD_GPIO_INTERRUPT_DISABLED		((0x00ul + 0x04ul) << 4)
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

#define MD_GPIO_BUF_TYPE_PP				(0x00ul << 8)
#define MD_GPIO_BUF_TYPE_OD				(0x01ul << 8)
#define MD_GPIO_BUF_TYPE_MASK			(0x01ul << 8)

typedef enum {
	MD_GPIO_EM_BUF_TYPE__PushPull = 0,
	MD_GPIO_EM_BUF_TYPE__OpenDrain = 1,
} MD_GPIO_EM_BUF_TYPE;

#define MD_GPIO_DIRECTION_INPUT			(0x00ul << 9)
#define MD_GPIO_DIRECTION_OUTPUT		(0x01ul << 9)
#define MD_GPIO_DIRECTION_MASK			(0x01ul << 9)

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

#define MD_GPIO_MUX_GPIO				(0x00ul << 12)
#define MD_GPIO_MUX_FUN1				(0x01ul << 12)
#define MD_GPIO_MUX_FUN2				(0x02ul << 12)
#define MD_GPIO_MUX_FUN3				(0x03ul << 12)

typedef enum {
	MD_GPIO_EM_MUX__GPIO = 0,
	MD_GPIO_EM_MUX__Fun1 = 1,
	MD_GPIO_EM_MUX__Fun2 = 2,
	MD_GPIO_EM_MUX__Fun3 = 3,
} MD_GPIO_EM_MUX;

#define MD_GPIO_POLARITY_NON_INVERTED	(0x00ul << 11)
#define MD_GPIO_POLARITY_INVERTED		(0x01ul << 11)

#define MD_GPIO_OUTPUT_LOW				(0x00ul << 16)
#define MD_GPIO_OUTPUT_HIGH				(0x01ul << 16)

#define MD_GPIO_INPUT_MASK				(0x01ul << 24)

typedef struct {
 	uint32_t PAD			: 2;
 	uint32_t POWER			: 2;
 	uint32_t INTERRUPT		: 3;
 	uint32_t EDGE_ENABLE	: 1;
 	uint32_t BUF_TYPE		: 1;
 	uint32_t DIRECTION		: 1;
 	uint32_t OUTPUT_SELECT	: 1;
 	uint32_t POLARITY		: 1;
 	uint32_t MUX			: 2;
 	uint32_t				: 2;
 	uint32_t OUTPUT			: 1;
 	uint32_t				: 7;
 	uint32_t INPUT			: 1;
} MD_GPIO_PIN_CONTROL;

typedef struct {
	uint32_t SLEW_RATE		: 1;
	uint32_t				: 3;
	uint32_t DRIVE_STRENGTH	: 2;
} MD_GPIO_PIN_CONTROL_2;

/***********************************************************************
 *                                                                     *
 * GPIO miscellaneous definition                                       *
 *                                                                     *
 ***********************************************************************/
#define GPIO_isNativePin(pin)       ((0 << 14) == ((pin) & (0x03ul << 14)))
#define GPIO_isDummyPin(pin)        ((1 << 14) == ((pin) & (0x03ul << 14)))
#define GPIO_isNullPin(pin)         ((2 << 14) == ((pin) & (0x03ul << 14)))
#define GPIO_isIoExpPin(pin)        ((3 << 14) == ((pin) & (0x03ul << 14)))

#define GPIO_COUNT   123
#define IO_EXP_COUNT 81

#define GPIO_MMCR_BASE DT_REG_ADDR(DT_NODELABEL(gpio0))

typedef     volatile uint8_t  *    REG_8b_DPTR;    //  8  Bits DPTR
typedef     volatile uint16_t  *   REG_16b_DPTR;   //  16 Bits DPTR
typedef     volatile uint32_t *    REG_32b_DPTR;   //  32 Bits DPTR
#define     MMCR_8b(  x )       (*(REG_8b_DPTR)((uint32_t)(  x )))
#define     MMCR_16b( x )       (*(REG_16b_DPTR)((uint32_t)( x )))
#define     MMCR_32b( x )       (*(REG_32b_DPTR)((uint32_t)( x )))

//
// Below MACROs taking effect only for EC native GPIOs.
//
#define GPIO_Pin2Addr(pin)          (uint32_t *)( GPIO_MMCR_BASE | ((pin & 0xFF00) >> 1) | (pin & 0xFF)<<2)
#define GPIO_Pin2SerialNumber(pin)  (((pin) >> 25) & 0x7F)
#define GPIO_PinNum(pin)            (((pin) & 0x01FE0000) >> 17)
#define GPIO_PinPort(pin)           (((pin) & 0x01FE0000) >> 22)
#define GPIO_PinPin(pin)            (GPIO_PinNum(pin) & 0x7)
#define GPIO_GetPORSetting(pin)     (uint32_t)( pin & 0x13FFF )
#define GPIO_POR_DEFAULT_SETTING    (0x00040)

/* Set gpio pad type */
void md_gpio_set_PAD(uint32_t * addr, MD_GPIO_EM_PAD pad);
/* Set gpio power rail */
void md_gpio_set_POWER(uint32_t * addr, MD_GPIO_EM_POWER pwr);
/* Set gpio interrupt type */
void md_gpio_set_INTERRUPT(uint32_t * addr, MD_GPIO_EM_INTERRUPT intType);
/* Set gpio buf type */
void md_gpio_set_BUF_TYPE(uint32_t * addr, MD_GPIO_EM_BUF_TYPE bufType);
/* Set gpio direction */
void md_gpio_set_DIRECTION(uint32_t * addr, MD_GPIO_EM_DIRECTION dir);
/* Set gpio output mechanism */
void md_gpio_set_OUTPUT_SELECT(uint32_t * addr, MD_GPIO_EM_OUTPUT_SELECT val);
/* Set gpio polarity */
void md_gpio_set_POLARITY(uint32_t * addr, bool isInverted);
/* Set gpio mux function */
void md_gpio_set_MUX(uint32_t * addr, MD_GPIO_EM_MUX mux);
/* Set gpio output */
void md_gpio_set_OUTPUT(uint32_t * addr, uint8_t val);
/* Get gpio input */
uint8_t md_gpio_get_INPUT(uint32_t * addr);
/* Get output set value */
uint8_t md_gpio_get_WAS_SET(uint32_t * addr);
/* Get gpio power rail */
int  md_gpio_get_POWER(uint32_t * addr);

#endif // end of __GPIOCTRL_H__

