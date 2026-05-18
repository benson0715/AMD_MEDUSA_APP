/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __GPIO_DRIVER_H__
#define __GPIO_DRIVER_H__

#include <zephyr/drivers/gpio.h>
#include "nuvoton_gpio_ctrl.h" 

#define  NUVO_GPIO_00  (0U) 
#define  NUVO_GPIO_01  (1U) 
#define  NUVO_GPIO_02  (2U) 
#define  NUVO_GPIO_03  (3U) 
#define  NUVO_GPIO_04  (4U) 
#define  NUVO_GPIO_05  (5U) 
#define  NUVO_GPIO_06  (6U) 
#define  NUVO_GPIO_07  (7U) 
                       
#define  NUVO_GPIO_10  (0U) 
#define  NUVO_GPIO_11  (1U) 
#define  NUVO_GPIO_12  (2U) 
#define  NUVO_GPIO_13  (3U) 
#define  NUVO_GPIO_14  (4U) 
#define  NUVO_GPIO_15  (5U) 
#define  NUVO_GPIO_16  (6U) 
#define  NUVO_GPIO_17  (7U) 
                       
#define  NUVO_GPIO_20  (0U) 
#define  NUVO_GPIO_21  (1U) 
#define  NUVO_GPIO_22  (2U) 
#define  NUVO_GPIO_23  (3U) 
#define  NUVO_GPIO_24  (4U) 
#define  NUVO_GPIO_25  (5U) 
#define  NUVO_GPIO_26  (6U) 
#define  NUVO_GPIO_27  (7U) 
                       
#define  NUVO_GPIO_30  (0U) 
#define  NUVO_GPIO_31  (1U) 
#define  NUVO_GPIO_32  (2U) 
#define  NUVO_GPIO_33  (3U) 
#define  NUVO_GPIO_34  (4U) 
#define  NUVO_GPIO_35  (5U) 
#define  NUVO_GPIO_36  (6U) 
#define  NUVO_GPIO_37  (7U) 
                      
#define  NUVO_GPIO_40  (0U) 
#define  NUVO_GPIO_41  (1U) 
#define  NUVO_GPIO_42  (2U) 
#define  NUVO_GPIO_43  (3U) 
#define  NUVO_GPIO_44  (4U) 
#define  NUVO_GPIO_45  (5U) 
#define  NUVO_GPIO_46  (6U) 
#define  NUVO_GPIO_47  (7U) 
                       
#define  NUVO_GPIO_50  (0U) 
#define  NUVO_GPIO_51  (1U) 
#define  NUVO_GPIO_52  (2U) 
#define  NUVO_GPIO_53  (3U) 
#define  NUVO_GPIO_54  (4U) 
#define  NUVO_GPIO_55  (5U) 
#define  NUVO_GPIO_56  (6U) 
#define  NUVO_GPIO_57  (7U) 
                       
#define  NUVO_GPIO_60  (0U) 
#define  NUVO_GPIO_61  (1U) 
#define  NUVO_GPIO_62  (2U) 
#define  NUVO_GPIO_63  (3U) 
#define  NUVO_GPIO_64  (4U) 
#define  NUVO_GPIO_65  (5U) 
#define  NUVO_GPIO_66  (6U) 
#define  NUVO_GPIO_67  (7U) 
                      
#define  NUVO_GPIO_70  (0U) 
#define  NUVO_GPIO_71  (1U) 
#define  NUVO_GPIO_72  (2U) 
#define  NUVO_GPIO_73  (3U) 
#define  NUVO_GPIO_74  (4U) 
#define  NUVO_GPIO_75  (5U) 
#define  NUVO_GPIO_76  (6U) 
#define  NUVO_GPIO_77  (7U) 
                       
#define  NUVO_GPIO_80  (0U) 
#define  NUVO_GPIO_81  (1U) 
#define  NUVO_GPIO_82  (2U) 
#define  NUVO_GPIO_83  (3U) 
#define  NUVO_GPIO_84  (4U) 
#define  NUVO_GPIO_85  (5U) 
#define  NUVO_GPIO_86  (6U) 
#define  NUVO_GPIO_87  (7U) 
                       
#define  NUVO_GPIO_90  (0U) 
#define  NUVO_GPIO_91  (1U) 
#define  NUVO_GPIO_92  (2U) 
#define  NUVO_GPIO_93  (3U) 
#define  NUVO_GPIO_94  (4U) 
#define  NUVO_GPIO_95  (5U) 
#define  NUVO_GPIO_96  (6U) 
#define  NUVO_GPIO_97  (7U) 
                       
#define  NUVO_GPIO_A0  (0U) 
#define  NUVO_GPIO_A1  (1U) 
#define  NUVO_GPIO_A2  (2U) 
#define  NUVO_GPIO_A3  (3U) 
#define  NUVO_GPIO_A4  (4U) 
#define  NUVO_GPIO_A5  (5U) 
#define  NUVO_GPIO_A6  (6U) 
#define  NUVO_GPIO_A7  (7U) 
                      
#define  NUVO_GPIO_B0  (0U) 
#define  NUVO_GPIO_B1  (1U) 
#define  NUVO_GPIO_B2  (2U) 
#define  NUVO_GPIO_B3  (3U) 
#define  NUVO_GPIO_B4  (4U) 
#define  NUVO_GPIO_B5  (5U) 
#define  NUVO_GPIO_B6  (6U) 
#define  NUVO_GPIO_B7  (7U) 
                       
#define  NUVO_GPIO_C0  (0U) 
#define  NUVO_GPIO_C1  (1U) 
#define  NUVO_GPIO_C2  (2U) 
#define  NUVO_GPIO_C3  (3U) 
#define  NUVO_GPIO_C4  (4U) 
#define  NUVO_GPIO_C5  (5U) 
#define  NUVO_GPIO_C6  (6U) 
#define  NUVO_GPIO_C7  (7U) 
                       
#define  NUVO_GPIO_D0  (0U) 
#define  NUVO_GPIO_D1  (1U) 
#define  NUVO_GPIO_D2  (2U) 
#define  NUVO_GPIO_D3  (3U) 
#define  NUVO_GPIO_D4  (4U) 
#define  NUVO_GPIO_D5  (5U) 
#define  NUVO_GPIO_D6  (6U) 
#define  NUVO_GPIO_D7  (7U) 
                       
#define  NUVO_GPIO_E0  (0U) 
#define  NUVO_GPIO_E1  (1U) 
#define  NUVO_GPIO_E2  (2U) 
#define  NUVO_GPIO_E3  (3U) 
#define  NUVO_GPIO_E4  (4U) 
#define  NUVO_GPIO_E5  (5U) 
#define  NUVO_GPIO_E6  (6U) 
#define  NUVO_GPIO_E7  (7U) 
                      
#define  NUVO_GPIO_F0  (0U) 
#define  NUVO_GPIO_F1  (1U) 
#define  NUVO_GPIO_F2  (2U) 
#define  NUVO_GPIO_F3  (3U) 
#define  NUVO_GPIO_F4  (4U) 
#define  NUVO_GPIO_F5  (5U) 
#define  NUVO_GPIO_F6  (6U) 
#define  NUVO_GPIO_F7  (7U) 

/**
 * @brief GPIO driver wrapper APIS.
 */
#define EC_GPIO_PORT_POS	8U
#define EC_GPIO_PORT_MASK	((uint32_t) 0xfU << EC_GPIO_PORT_POS)
#define EC_GPIO_PIN_POS		0U
#define EC_GPIO_PIN_MASK	((uint32_t)0x1fU << EC_GPIO_PIN_POS)

#define EC_GPIO_PORT_PIN(_port, _pin)                   \
	(((uint32_t)(_port) << EC_GPIO_PORT_POS)   |       \
	((uint32_t)(_pin) << EC_GPIO_PIN_POS))             \

#define HIGH	1
#define LOW	0

#define NUVO_GPIO_0		0x0
#define NUVO_GPIO_1		0x1
#define NUVO_GPIO_2		0x2
#define NUVO_GPIO_3		0x3
#define NUVO_GPIO_4		0x4
#define NUVO_GPIO_5		0x5
#define NUVO_GPIO_6		0x6
#define NUVO_GPIO_7		0x7
#define NUVO_GPIO_8		0x8
#define NUVO_GPIO_9		0x9
#define NUVO_GPIO_A		0xa
#define NUVO_GPIO_B		0xb
#define NUVO_GPIO_C		0xc
#define NUVO_GPIO_D		0xd
#define NUVO_GPIO_E		0xe
#define NUVO_GPIO_F		0xf

#define GPIO_CTRL_PWRG_VTR_IO	0
#define GPIO_CTRL_PWRG_OFF		1

#define GPIO_CTRL_IRQ_ON		0
#define GPIO_CTRL_IRQ_OFF		1


extern uint32_t _s_Gpio_IdxOfUseWasSet[];
/*
 * This structure is used to pass an array of GPIOs and settings to
 * this driver wrapper. It encodes the gpio device and the pin in a
 * single variable.
 */
struct gpio_ec_config {
	uint32_t port_pin;
	uint32_t cfg;
	uint32_t pwr_cfg;
};

static inline uint32_t gpio_get_port(uint16_t pin)
{
	return  ((pin & EC_GPIO_PORT_MASK) >> EC_GPIO_PORT_POS);
}

static inline uint32_t gpio_get_pin(uint16_t pin)
{
	return ((pin & EC_GPIO_PIN_MASK) >> EC_GPIO_PIN_POS);
}

/**
 * @brief Routine to get the absolute gpio number
 *
 * This routine gets the absolute gpio number from the port and the
 * pin number. For e.g. GPIO_227 which is port 4 and pin 23 gets a
 * value of 227.
 *
 * @param p1 a value which combines port and pin values.
 * @retval absolute gpio number.
 */
uint32_t get_absolute_gpio_num(uint32_t port_pin);

/**
 * @brief Initialize the gpio ports.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_init(void);

/**
 * @brief gpio_set_interrupt
 *
 * Wrapper interface to configure a gpio pin using the standard Zephyr flags.
 * The first parameter hides the port number and pin.
 *
 * @param port_pin Encoded device and pin.
 * @param flags true is enable irq false is disable
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int  gpio_set_interrupt(uint32_t port_pin,uint32_t flag);

/**
 * @brief Configure a specific gpio pin.
 *
 * Wrapper interface to configure a gpio pin using the standard Zephyr flags.
 * The first parameter hides the port number and pin.
 *
 * @param port_pin Encoded device and pin.
 * @param flags Standard GPIO driver flags to configure a pin.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_configure_pin(uint32_t port_pin, gpio_flags_t flags);

/**
 * @brief Configure an array of gpios.
 *
 * Wrapper to configure several GPIOs by receiving
 * an array containing the encoded Port/Pin, flags and direction.
 *
 * @param gpios Array containing device and pin, direction.
 * @param len Array size.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_configure_array(struct gpio_ec_config *gpios, uint32_t len);

/**
 * @brief Set the level for a pin.
 *
 * Wrapper interface to configure several GPIOs by receiving
 * an array containing the encoded Port/Pin, flags and direction.
 *
 * @param port_pin Encoded port/pin.
 * @param value Desired logical level.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_write_pin(uint32_t port_pin, int value);

/**
 * @brief Read the level of a pin.
 *
 * Wrapper interface to configure several GPIOs by receiving
 * an array containing the encoded Port/Pin, flags and direction.
 *
 * @param port_pin Encoded port/pin.
 *
 * @retval 1 If pin physical level is high.
 * @retval 0 If pin physical level is low.
 * @retval -ENODEV error when internal device validation failed.
 * @retval Negative errno code on failure.
 */
int gpio_read_pin(uint32_t port_pin);

/**
 * @brief Initialize a gpio_callback struct.
 *
 * This wrapper interface initializes a gpio_callback struct.
 * Note: The underlaying zephyr function is void.
 *
 * @param port_pin Encoded port/pin.
 * @param callback A valid Application's callback structure pointer.
 * @param handler A valid handler function pointer.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_init_callback_pin(uint32_t port_pin,
			   struct gpio_callback *callback,
			   gpio_callback_handler_t handler);
/**
 * @brief Read the power of a pin.
 *
 * Wrapper interface to configure several GPIOs by receiving
 * an array containing the encoded Port/Pin, flags and direction.
 *
 * @param port_pin Encoded port/pin.
 *
 * @retval 2 If pin physical level is power off.
 * @retval 0 If pin physical level is power on.
 * @retval -ENODEV error when internal device validation failed.
 * @retval Negative errno code on failure.
 */
MD_GPIO_EM_POWER gpio_get_power (uint32_t port_pin);
/**
 * @brief Set the power for a pin.
 *
 * Wrapper interface to configure several GPIOs by receiving
 * an array containing the encoded Port/Pin, flags and direction.
 *
 * @param port_pin Encoded port/pin.
 * @param value Desired logical level.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int  gpio_set_power(uint32_t port_pin, uint32_t pwrSt);
/**
 * @brief output select for a pin.
 *
 * Wrapper interface to configure several GPIOs by receiving
 * an array containing the encoded Port/Pin, flags and direction.
 *
 * @param port_pin Encoded port/pin.
 * @param value Desired select.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int  gpio_set_output_select(uint32_t port_pin, MD_GPIO_EM_OUTPUT_SELECT val);

/**
 * @brief Add an application callback.
 *
 * This wrapper interface adds a callback pointer to be triggered in the
 * application context.
 *
 * Note: Callbacks may be added to the device from within a callback
 * handler invocation
 *
 * @param port_pin Encoded port/pin.
 * @param callback A valid Application's callback structure pointer.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_add_callback_pin(uint32_t port_pin,
			  struct gpio_callback *callback);

/**
 * @brief Remove an application callback.
 *
 * This wrapper interface adds a callback pointer to be triggered in the
 * application context.
 *
 * Note: It is explicitly permitted, within a callback handler, to
 * remove the registration for the callback that is running.
 *
 * @param port_pin Encoded port/pin.
 * @param callback A valid Application's callback structure pointer.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_remove_callback_pin(uint32_t port_pin,
			     struct gpio_callback *callback);

/**
 * @brief Configure pin interrupt.
 *
 * This wrapper interface configures interrupt capabilities for a given pin.
 *
 * @param port_pin Encoded port/pin.
 * @param flags Interrupt configuration flags as defined by GPIO_INT_*.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_interrupt_configure_pin(uint32_t port_pin, gpio_flags_t flags);

/**
 * @brief Validate if a specific gpio port was initialized.
 *
 * This function is expected to be called after gpio_init otherwise
 * it will always return -ENODEV.
 *
 * @param port_pin Encoded port/pin.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval true  if successful, false otherwise.
 */
bool gpio_port_enabled(uint32_t port_pin);
/**
 * @brief Allow to force GPIO onfiguration on a specific gpio pin.
 *
 * Note: This is temporary solution until proper solution to SAF DTS
 * configuring pins in alternate function.
 *
 * @param port_pin Encoded device and pin.
 * @param flags Standard GPIO driver flags to configure a pin.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_force_configure_pin(uint32_t port_pin, gpio_flags_t flags);
int gpio_force_configure_pin_npcx( uint8_t alt,uint8_t flag);
/**
 * @brief Allow to force GPIO alt function on a specific gpio pin.
 *
 * Note: This is temporary solution until proper solution to SAF DTS
 * configuring pins in alternate function.
 *
 * @param port_pin Encoded device and pin.
 * @param flags Standard GPIO driver flags to configure a pin.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int gpio_alt_fun_configure_pin(uint32_t port_pin, gpio_flags_t flags);

/**
 * @brief Allow to get the previous write value to gpio.
 *
 * @param port_pin Encoded device and pin.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int  gpio_was_mpm_set(uint32_t port_pin);

/**
 * @brief Change GPIO attribute: input, output, or open drain
 *
 * @param port_pin gpio pin number.
 * @param type gpio type: input, output, or open drain
 */
void gpio_set_type(uint32_t port_pin, gpio_flags_t type);
/**
 * @brief set GPIO select
 *
 * @param port_pin gpio pin number.
 * @param type gpio type: input, output, or open drain
 */
void md_gpio_set_OUTPUT_SELECT(uint32_t * addr, MD_GPIO_EM_OUTPUT_SELECT val);
/**
 * @brief get GPIO select
 *
 * @param port_pin gpio pin number.
 * @param type 
 */
uint8_t md_gpio_get_OUTPUT_SELECT(uint32_t * addr);
/**
 * @brief Change GPIO out control: 1,0
 *
 * @param port_pin gpio pin number.
 * @retval 0  OUTPUT_SELECT__PinCtrl16.
 * @retval 1  OUTPUT_SELECT__OutputReg.
 */
void md_gpio_set_OUTPUT(uint32_t * addr, uint8_t val);
/**
 * @brief gpio regwas_table
 *
 * @retval 0  The table for register gpio.
 */
void gpio_reg_was_table (uint32_t * pTab);


#endif /* __GPIO_DRIVER_H__*/
