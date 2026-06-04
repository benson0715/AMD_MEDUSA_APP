/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Per-board hook implementations for AMD MDS Plum on RTS5918. The
 * matching Nuvoton file (mdsplum_npcx4mnx.c) ran POR init tables that
 * walked an autogen array; for RTS5918 the init flow is reduced to
 * the bare minimum needed for first-light bring-up. Real hooks
 * follow as schematic data is filled in.
 */

#include "board_config.h"
#include "espi_hub.h"



/* `boot_mode_maf` is owned by app/power_sequencing/app_pseq.c (defined
 * unconditionally at line 112). Keep only the extern declaration via
 * board_config.h — defining it here too produces a link-time multiple
 * definition error.
 */

int board_init(void)
{
	int ret;

	/**
	 * Get Sys Config from rom 
	 */
	ret = brdSysConfig_Init();
	if (ret) {
		//LOG_ERR("Failed to get Sys Config from rom: %d", ret);
		return ret;
	}

	/**
	 * Initialize EC GPIO
	 */
	__autoGen_initECGPIO();


	/**
	 * Initialize SPI Flash
	 */
	//ret = amd_crb_drivers_sFlashInit();
	if (ret) {
		return ret;
	}

	/**
	 * Detect boot mode
	 */
	detect_boot_mode();

	/**
	 * Initialize the platform
	 */
	board_init_platformInit();

	k_msleep(100);

	return 0;	
}

int board_suspend(void)
{
	return 0;
}

int board_resume(void)
{
	return 0;
}

void board_onApuPwrOK(void)
{
}

/* Runtime GPIO settings */
#define DRIVER_TO_LOW_BEFORE_EC_SLP_S3_L_TO_HIGH                                         \
    {EC_EVAL_AUX_RST_N,  GPIO_ACTIVE_HIGH | GPIO_OUTPUT_LOW | GPIO_DEFINE,           0         },    \
    {EC_EVAL_CARD_PWREN,  GPIO_ACTIVE_HIGH | GPIO_OUTPUT_LOW | GPIO_DEFINE,           0         },    \
    {EC_EVAL_SLT_PWREN, GPIO_ACTIVE_HIGH | GPIO_OUTPUT_LOW | GPIO_DEFINE,           0         },    \
    {EC_EVAL_BOMACO_EN,    GPIO_ACTIVE_HIGH | GPIO_OUTPUT_LOW | GPIO_DEFINE,           0         },    \
    {EC_EVAL_19V_PWREN,  GPIO_ACTIVE_HIGH | GPIO_OUTPUT_LOW | GPIO_DEFINE,           0         },    \

struct gpio_ec_config evalCardCtrlPins[] = {
    DRIVER_TO_LOW_BEFORE_EC_SLP_S3_L_TO_HIGH
};

/**
 * @brief Initialize eval card control pins to low state
 *
 * PLAT-84470
 * Init all of the pin status to 0 on APU_RST# but keep them untouched across S3/S0i3
 *
 * @param void
 * @return void
 */
void board_evalCardCtrlPins(void)
{
	// uint32_t pinList[] = {
	// 	PIN_LIST_DRIVER_TO_LOW_BEFORE_EC_SLP_S3_L_TO_HIGH,
	// };

	gpio_configure_array(evalCardCtrlPins, ARRAY_SIZE(evalCardCtrlPins));   

	// for (uint8_t i = 0; pinList[i] != GPIO_PIN_NULL; i++) {
	// 	gpio_write_pin(pinList[i], 0);
		//gpio_set_power(pinList[i], GPIO_CTRL_PWRG_VTR_IO);
	//}
}

void board_turnOffJtagInterface(void)
{
}

bool board_ifNeedForcePdFwUpdate(void)
{
	return false;
}

void board_turnOn_espi(void)
{
}
