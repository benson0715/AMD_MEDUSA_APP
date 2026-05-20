/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "task_handler.h"
#include "softstrap.h"
#if CONFIG_PSL
#include "psl.h"
#endif
#include "f_bram.h"
extern uint8_t g_ui8PwrBtnVciFlag;
LOG_MODULE_REGISTER(ecfw, CONFIG_EC_LOG_LEVEL);

/**
 * @brief Main entry point for EC firmware application
 *
 * Initializes VCI, PSL, board configuration, and starts all tasks.
 * Runs the main application loop.
 */
void main(void)
{
	int ret;
#if CONFIG_VCI
	app_vci();
	app_vci_samplePwrBtn();
	app_vci_latchRst();
#endif
	printk("In main!!\r\n");
	/* Delayed start for debug */
	k_sleep(K_SECONDS(CONFIG_EC_DELAYED_BOOT));
	printk("In main2!!\r\n");
#if CONFIG_PSL
	if (psl_get_input() & 0x02) {
		g_ui8PwrBtnVciFlag = 1;
	} else {
		g_ui8PwrBtnVciFlag = 0;
	}
	psl_configure();
#endif
	f_bram_onBootUp();
	printk("EC FW Zephyr %p %s\r\n", k_current_get(), CONFIG_BOARD);

	/* The espi block needs to be initialized before the GPIOS. This is
	 * because we must query the boot mode before assigning functions
	 * to certain pins
	 */

	ret = board_init();
	if (ret) {
		LOG_ERR("Failed to init board %d", ret);
		return;
	}
	strap_init();
	start_all_tasks();

	while (true) {
		k_msleep(2100);

	}
}