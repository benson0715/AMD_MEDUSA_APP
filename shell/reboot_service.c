/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */



#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <init.h>
#include <string.h>
#include <power/reboot.h>

/**
 * @brief Shell command handler for cold reboot
 */
static int cmd_cold(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}

/**
 * @brief Shell command handler for warm reboot
 */
static int cmd_warm(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	sys_reboot(SYS_REBOOT_WARM);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_reboot,
	SHELL_CMD(cold, NULL, NULL, cmd_cold),
	SHELL_CMD(warm, NULL, NULL, cmd_warm),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(reboot, &sub_reboot, "Reboot commands", NULL);