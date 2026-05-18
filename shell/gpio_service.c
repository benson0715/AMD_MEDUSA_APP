/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/sys/printk.h>
#include <shell/shell.h>
#include <init.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <stdlib.h>
#include <ctype.h>
#include <zephyr/logging/log.h>
#include "gpio_ec.h"

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL

LOG_MODULE_REGISTER(gpio_shell);

struct args_index {
	uint8_t port;
	uint8_t index;
	uint8_t mode;
	uint8_t value;
};

struct args_number {
	uint8_t conf;
	uint8_t set;
	uint8_t get;
	uint8_t listen;
};

static const struct args_index args_indx = {
	.port = 1,
	.index = 2,
	.mode = 3,
	.value = 3,
};

static const struct args_number args_no = {
	.conf = 4,
	.set = 4,
	.get = 3,
	.listen = 7
};

/**
 * @brief Shell command handler to configure GPIO pin direction
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, negative error code on failure
 */
static int cmd_gpio_conf(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t index = 0U;
        uint8_t port= 0U;
	int type = GPIO_OUTPUT;
	if (argc == args_no.conf &&
	    isdigit((unsigned char)argv[args_indx.index][0]) &&
	    isalpha((unsigned char)argv[args_indx.mode][0])) {
		index = (uint8_t)atoi(argv[args_indx.index]);
		if (!strcmp(argv[args_indx.mode], "in")) {
			type = GPIO_INPUT;
		} else if (!strcmp(argv[args_indx.mode], "out")) {
			type = GPIO_OUTPUT;
                } else if (!strcmp(argv[args_indx.mode], "od")) {
			type = GPIO_OUTPUT_LOW|GPIO_OPEN_DRAIN;
		} else {
			return 0;
		}
	} else {
		shell_error(shell, "Wrong parameters for conf");
		return -ENOTSUP;
	}

	index = (uint8_t)atoi(argv[args_indx.index]);
        port = (uint8_t)atoi(argv[args_indx.port]);
	shell_print(shell, "Configuring %s pin %d type:%x",
			    argv[args_indx.port], index,type);
	gpio_set_type((port<<8)+index,type);


	return 0;
}

/**
 * @brief Shell command handler to get GPIO pin value
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, negative error code on failure
 */
static int cmd_gpio_get(const struct shell *shell,
			size_t argc, char **argv)
{
	uint8_t index = 0U;
        uint8_t port= 0U;
	int rc;

	if (argc == args_no.get && isdigit((unsigned char)argv[args_indx.index][0])) {
		index = (uint8_t)atoi(argv[args_indx.index]);
	} else {
		shell_error(shell, "Wrong parameters for get");
		return -EINVAL;
	}


	index = (uint8_t)atoi(argv[2]);
        port =  (uint8_t)atoi(argv[1]);
	shell_print(shell, "Reading %s pin %d",
			    argv[args_indx.port], index);
	rc = gpio_read_pin((port<<8)+index);
	if (rc >= 0) {
		shell_print(shell, "Value %d", rc);
	} else {
		shell_error(shell, "Error %d reading value", rc);
		return -EIO;
	}

	return 0;
}

/**
 * @brief Shell command handler to set GPIO pin value
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, negative error code on failure
 */
static int cmd_gpio_set(const struct shell *shell,
			size_t argc, char **argv)
{
	uint8_t index = 0U;
        uint8_t port= 0U;
	uint8_t value = 0U;

	if (argc == args_no.set &&
	    isdigit((unsigned char)argv[args_indx.index][0]) &&
	    isdigit((unsigned char)argv[args_indx.value][0])) {
		index = (uint8_t)atoi(argv[args_indx.index]);
		value = (uint8_t)atoi(argv[args_indx.value]);
	} else {
		shell_print(shell, "Wrong parameters for set");
		return -EINVAL;
	}

	index = (uint8_t)atoi(argv[2]);
        port =  (uint8_t)atoi(argv[1]);
	shell_print(shell, "Writing to %s pin %d",
			    argv[args_indx.port], index);
	gpio_write_pin((port<<8)+index, value);

	return 0;
}

/**
 * @brief Shell command handler to set GPIO power domain
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, negative error code on failure
 */
static int cmd_gpio_power_set(const struct shell *shell,
			size_t argc, char **argv)
{
	uint8_t index = 0U;
        uint8_t port= 0U;
	uint8_t value = 0U;

	if (argc == args_no.set &&
	    isdigit((unsigned char)argv[args_indx.index][0]) &&
	    isdigit((unsigned char)argv[args_indx.value][0])) {
		index = (uint8_t)atoi(argv[args_indx.index]);
                port =  (uint8_t)atoi(argv[args_indx.port]);
		value = (uint8_t)atoi(argv[args_indx.value]);
	} else {
		shell_print(shell, "Wrong parameters for set");
		return -EINVAL;
	}

	index = (uint8_t)atoi(argv[2]);
        port =  (uint8_t)atoi(argv[1]);
	shell_print(shell, "Power Writing to %s pin %d",
			    argv[args_indx.port], index);
	gpio_set_power((port<<8)+index, value);

	return 0;
}

/**
 * @brief Shell command handler to get GPIO power domain
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, negative error code on failure
 */
static int cmd_gpio_power_get(const struct shell *shell,
			size_t argc, char **argv)
{
	uint8_t index = 0U;
        uint8_t port= 0U;
	int rc;

	if (argc == args_no.get && isdigit((unsigned char)argv[args_indx.index][0])) {
		index = (uint8_t)atoi(argv[args_indx.index]);
	} else {
		shell_error(shell, "Wrong parameters for get");
		return -EINVAL;
	}


	index = (uint8_t)atoi(argv[2]);
        port =  (uint8_t)atoi(argv[1]);
	shell_print(shell, "Reading %s pin %d",
			    argv[args_indx.port], index);

        rc = gpio_get_power((port<<8)+index);
	if (rc == 0) {
		shell_print(shell, "Get Power is VTR");
	} else if (rc == 2)  {
		shell_print(shell, "GetPower is UNPOWERED");
		return 0;
	}
        else if (rc == 1)  {
		shell_print(shell, "GetPower is VCCMAIN");
		return 0;
	}


	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio,
	SHELL_CMD(conf, NULL, "Configure GPIO arg0:<port> arg1:<pin> arg2<type>", cmd_gpio_conf),
	SHELL_CMD(get, NULL, "Get GPIO  arg0:<port> arg1: <pin>", cmd_gpio_get),
	SHELL_CMD(set, NULL, "Set GPIO  arg0:<port> arg1:<pin> arg2<value>", cmd_gpio_set),
	SHELL_CMD(pwrset, NULL, "Set GPIO power arg0:<port> arg1:<pin> arg2<value> value{0:vtr 1: vccmain 2:unpowered}", cmd_gpio_power_set),
	SHELL_CMD(pwrget, NULL, "Get GPIO Power arg0:<port> arg1: <pin>", cmd_gpio_power_get),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(gpio, &sub_gpio, "GPIO commands", NULL);