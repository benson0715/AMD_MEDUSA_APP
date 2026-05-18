/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <version.h>

static const char *app_version = "1.0.0";

/**
 * @brief Shell command to display board information
 */
static int cmd_info_board(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, CONFIG_BOARD);

	return 0;
}

/**
 * @brief Shell command to display Zephyr kernel version
 */
static int cmd_info_version(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Zephyr version %s", KERNEL_VERSION_STRING);

	return 0;
}

/**
 * @brief Shell command to boot application from specified address
 */
static int cmd_boot_app(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	unsigned int address;
	unsigned int addr_p;
	void (*JMP_func_pointer)();
 	addr_p = 0x10070404;
	address = *(int*)addr_p;
	JMP_func_pointer=(void*)(address);
	irq_lock();
	JMP_func_pointer();

	return 0;
}

/**
 * @brief Shell command to display application version
 */
static int cmd_info_app_version(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "App version %s", app_version);

	return 0;
}

/**
 * @brief Shell command to display help information
 */
static int cmd_shell_help(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "show help: %d", argc);
    if(argc == 1){
        shell_help(sh);
        return SHELL_CMD_HELP_PRINTED;
    }

	for(size_t i=0; i< argc; i++){
		shell_print(sh, "check arg %d: %s", i, argv[i]);
	}

	return 0;
}

/**
 * @brief Main entry point for Zephyr bootloader
 */
void main(void)
{
	printk("*** Enter Zephyr Bootloader Version 0.1.0 ***\n");
	unsigned int address;
	unsigned int addr_p;
	void (*JMP_func_pointer)();
 	addr_p = 0x10070404;
	address = *(int*)addr_p;
	JMP_func_pointer=(void*)(address);
	irq_lock();
	JMP_func_pointer();
	while(1)
	{		
		k_msleep(2100);
	}
}

SHELL_STATIC_SUBCMD_SET_CREATE(subinfo,
    SHELL_CMD(board, NULL, "Show board command.", cmd_info_board),
    SHELL_CMD(version, NULL, "Show Zephyr version command.", cmd_info_version),
    SHELL_CMD(app_version, NULL, "Show app version command.", cmd_info_app_version),
    SHELL_CMD(app, NULL, "Show app command.", cmd_boot_app),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(boot, &subinfo, "Demo commands", cmd_shell_help);