/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "psl.h"
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/gpio.h>
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
#include "board_pwrSrc.h"
#endif
#include "f_bram.h"
/* Soc specific system local functions */
#define PSL_OUT_GPIO_DRIVEN    0
#define PSL_OUT_FW_CTRL_DRIVEN    1
#define PINCTRL_STATE_HIBERNATE 1

#define PSL_NODE DT_INST(0, npcx_hibernate_psl)
PINCTRL_DT_DEFINE(PSL_NODE);

/**
 * @brief Configure PSL (Power Switch Logic) module
 *
 * @return 0 on success
 */
int psl_configure(void)
{
    // const struct pinctrl_dev_config *pcfg =
    //     PINCTRL_DT_DEV_CONFIG_GET(PSL_NODE);
    // return pinctrl_apply_state(pcfg, PINCTRL_STATE_DEFAULT);
    struct gpio_dt_spec enable = GPIO_DT_SPEC_GET(PSL_NODE, enable_gpios);
    gpio_pin_set_dt(&enable, 0);
    return 0;
}

/**
 * @brief Set PSL output to inactive state for hibernate mode
 */
void psl_out_inactive(void)
{
#if (CONFIG_POWER_SOURCE_MANAGEMENT)
    	board_pwrSrc_setACOKReference_vci();
#endif
#if (DT_ENUM_IDX(PSL_NODE, psl_driven_type) == PSL_OUT_GPIO_DRIVEN)
	const struct pinctrl_dev_config *pcfg = PINCTRL_DT_DEV_CONFIG_GET(PSL_NODE);
	pinctrl_apply_state(pcfg, PINCTRL_STATE_DEFAULT);
	f_bram_setSig(F_BRAM_VCI_BOOT_SIGNATURE);
	struct gpio_dt_spec enable = GPIO_DT_SPEC_GET(PSL_NODE, enable_gpios);

	gpio_pin_set_dt(&enable, 1);
#elif (DT_ENUM_IDX(PSL_NODE, psl_driven_type) == PSL_OUT_FW_CTRL_DRIVEN)

    LOG_INF("PINCTRL_STATE_HIBERNATE");
    const struct pinctrl_dev_config *pcfg =
        PINCTRL_DT_DEV_CONFIG_GET(PSL_NODE);

    pinctrl_apply_state(pcfg, PINCTRL_STATE_HIBERNATE);
#endif
}

/**
 * @brief Get PSL input status
 *
 * @return PSL input status (lower 4 bits)
 */
uint8_t psl_get_input(void)
{
	struct glue_reg *inst_glue = (struct glue_reg *)(NPCX_GLUE_REG_ADDR);
	return (uint8_t)(inst_glue->PSL_CTS & 0xF);
}