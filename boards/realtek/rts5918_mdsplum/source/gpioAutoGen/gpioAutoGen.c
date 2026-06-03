/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gpioAutoGen.h"
#include <zephyr/sys/printk.h>
#include "gpio_ec.h"


void prim_pwrg_isr(void)
{
	printk("FOR TEST In GPIO ISR\r\n");
}

/***********************************************************************
 *                                                                     *
 * Runtime GPIO settings                                               *
 *                                                                     *
 ***********************************************************************/

struct gpio_ec_config _s_AMDRTS5919_cfg_rtG3[] = {
    GPIO_RUNTIME_SWITCH_G3
};
// struct gpio_ec_config _s_AMDRTS5919_cfg_rtS5S4[] = {
//     GPIO_RUNTIME_SWITCH_S5S4
// };
// struct gpio_ec_config _s_AMDRTS5919_cfg_rtS3[] = {
//     GPIO_RUNTIME_SWITCH_S3
// };
struct gpio_ec_config _s_AMDRTS5919_cfg_rtS0[] = {
    GPIO_RUNTIME_SWITCH_S0
};

void __autoGen_runtimeGpioSwitching (enum system_power_state pwr_state ) {
    switch (pwr_state) {
        case SYSTEM_G3_STATE:   
			gpio_configure_array(_s_AMDRTS5919_cfg_rtG3, ARRAY_SIZE(_s_AMDRTS5919_cfg_rtG3));   
			break;
        case SYSTEM_S4_STATE:
        case SYSTEM_S5_STATE:
			//gpio_configure_array(_s_AMDRTS5919_cfg_rtS5S4, ARRAY_SIZE(_s_AMDRTS5919_cfg_rtS5S4)); 
			break;
        case SYSTEM_S3_STATE:   
			//gpio_configure_array(_s_AMDRTS5919_cfg_rtS3, ARRAY_SIZE(_s_AMDRTS5919_cfg_rtS3));   
			break;
        case SYSTEM_S0_STATE:
			gpio_configure_array(_s_AMDRTS5919_cfg_rtS0, ARRAY_SIZE(_s_AMDRTS5919_cfg_rtS0));
			break;  
        default:                                                   
			break;
    }
}


void __autoGen_initECGPIO(void)
{
	gpio_configure_all_pins();
	gpio_interrupt_configure_all();
}

void Board_Gpio_AcpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t *pui8Data)
{
	ARG_UNUSED(isRead);
	ARG_UNUSED(ui8Idx);
	if (pui8Data) {
		*pui8Data = 0;
	}
	/* TODO(realtek-schematic): once gpioAutoGen.{c,h} are regenerated
	 * from Plum data, replace this stub with the per-pin dispatch
	 * the NPCX4 autogen emits.
	 */
}
