/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>

#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include <stdlib.h>
#include <string.h>
#include <zephyr/drivers/flash.h>

/* Buffer is only needed for bytes that follow command and offset */
#define BUF_ARRAY_CNT (CONFIG_SHELL_ARGC_MAX - 2)
#define TEST_ARR_SIZE 0x1000

/* This only issues compilation error when it would not be possible
 * to extract at least one byte from command line arguments, yet
 * it does not warrant successful writes if BUF_ARRAY_CNT
 * is smaller than flash write alignment.
 */
BUILD_ASSERT(BUF_ARRAY_CNT >= 1);

static const struct device *const zephyr_flash_controller =
	DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_flash_controller));

static uint8_t __aligned(4) test_arr[TEST_ARR_SIZE];

/**
 * @brief Parse flash command arguments to extract device and address
 *
 * @param shell Shell instance
 * @param argc Pointer to argument count
 * @param argv Pointer to argument vector
 * @param flash_dev Pointer to store flash device
 * @param addr Pointer to store address
 * @return 0 on success, negative error code on failure
 */
static int parse_helper(const struct shell *shell, size_t *argc,
		char **argv[], const struct device * *flash_dev,
		uint32_t *addr)
{
	char *endptr;

	*addr = strtoul((*argv)[1], &endptr, 16);

	if (*endptr != '\0') {
		/* flash controller from user input */
		*flash_dev = device_get_binding((*argv)[1]);
		if (!*flash_dev) {
			shell_error(shell, "Given flash device was not found");
			return -ENODEV;
		}
	} else if (zephyr_flash_controller != NULL) {
		/* default to zephyr,flash-controller */
		if (!device_is_ready(zephyr_flash_controller)) {
			shell_error(shell, "Default flash driver not ready");
			return -ENODEV;
		}
		*flash_dev = zephyr_flash_controller;
	} else {
		/* no flash controller given, no default available */
		shell_error(shell, "No flash device specified (required)");
		return -ENODEV;
	}

	if (*endptr == '\0') {
		return 0;
	}
	if (*argc < 3) {
		shell_error(shell, "Missing address.");
		return -EINVAL;
	}
	*addr = strtoul((*argv)[2], &endptr, 16);
	(*argc)--;
	(*argv)++;
	return 0;
}

/**
 * @brief Shell command handler for flash erase operation
 */
static int cmd_erase(const struct shell *shell, size_t argc, char *argv[])
{
	const struct device *flash_dev;
	uint32_t page_addr;
	int result;
	uint32_t size;

	result = parse_helper(shell, &argc, &argv, &flash_dev, &page_addr);
	if (result) {
		return result;
	}
	if (argc > 2) {
		size = strtoul(argv[2], NULL, 16);
	} else {
		struct flash_pages_info info;

		result = flash_get_page_info_by_offs(flash_dev, page_addr,
						     &info);

		if (result != 0) {
			shell_error(shell, "Could not determine page size, "
				    "code %d.", result);
			return -EINVAL;
		}

		size = info.size;
	}

	result = flash_erase(flash_dev, page_addr, size);

	if (result) {
		shell_error(shell, "Erase Failed, code %d.", result);
	} else {
		shell_print(shell, "Erase success.");
	}

	return result;
}

/**
 * @brief Shell command handler for flash write operation
 */
static int cmd_write(const struct shell *shell, size_t argc, char *argv[])
{
	uint32_t __aligned(4) check_array[BUF_ARRAY_CNT];
	uint32_t __aligned(4) buf_array[BUF_ARRAY_CNT];
	const struct device *flash_dev;
	uint32_t w_addr;
	int ret;
	size_t op_size;

	ret = parse_helper(shell, &argc, &argv, &flash_dev, &w_addr);
	if (ret) {
		return ret;
	}

	if (argc <= 2) {
		shell_error(shell, "Missing data to be written.");
		return -EINVAL;
	}

	op_size = 0;

	for (int i = 2; i < argc; i++) {
		int j = i - 2;

		buf_array[j] = strtoul(argv[i], NULL, 16);
		check_array[j] = ~buf_array[j];

		op_size += sizeof(buf_array[0]);
	}

	if (flash_write(flash_dev, w_addr, buf_array, op_size) != 0) {
		shell_error(shell, "Write internal ERROR!");
		return -EIO;
	}

	shell_print(shell, "Write OK.");

	if (flash_read(flash_dev, w_addr, check_array, op_size) < 0) {
		shell_print(shell, "Verification read ERROR!");
		return -EIO;
	}

	if (memcmp(buf_array, check_array, op_size) == 0) {
		shell_print(shell, "Verified.");
	} else {
		shell_error(shell, "Verification ERROR!");
		return -EIO;
	}

	return 0;
}

/**
 * @brief Shell command handler for flash read operation
 */
static int cmd_read(const struct shell *shell, size_t argc, char *argv[])
{
	const struct device *flash_dev;
	uint32_t addr;
	int todo;
	int upto;
	int cnt;
	int ret;

	ret = parse_helper(shell, &argc, &argv, &flash_dev, &addr);
	if (ret) {
		return ret;
	}

	if (argc > 2) {
		cnt = strtoul(argv[2], NULL, 16);
	} else {
		cnt = 1;
	}

	for (upto = 0; upto < cnt; upto += todo) {
		uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

		todo = MIN(cnt - upto, SHELL_HEXDUMP_BYTES_IN_LINE);
		ret = flash_read(flash_dev, addr, data, todo);
		if (ret != 0) {
			shell_error(shell, "Read ERROR!");
			return -EIO;
		}
		shell_hexdump_line(shell, addr, data, todo);
		addr += todo;
	}

	shell_print(shell, "");

	return 0;
}

/**
 * @brief Shell command handler for reading flash JEDEC ID
 */
static int cmd_read_id(const struct shell *shell, size_t argc, char *argv[])
{
	const struct device *flash_dev;
	int ret;
	uint32_t addr;
	uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];
	ret = parse_helper(shell, &argc, &argv, &flash_dev, &addr);
	if (ret) {
		return ret;
	}


	ret = flash_read_jedec_id(flash_dev, data);
	if (ret != 0) {
		shell_error(shell, "Read ERROR!");
			return -EIO;
	}
	shell_print(shell, "id = %x %x %x",data[0],data[1],data[2]);

	shell_print(shell, "");

	return 0;
}

/**
 * @brief Shell command handler for reading flash SFDP data
 */
static int cmd_read_sfdp(const struct shell *shell, size_t argc, char *argv[])
{
	const struct device *flash_dev;
	uint32_t addr;
	int todo;
	int upto;
	int cnt;
	int ret;

	ret = parse_helper(shell, &argc, &argv, &flash_dev, &addr);
	if (ret) {
		return ret;
	}

	if (argc > 2) {
		cnt = strtoul(argv[2], NULL, 16);
	} else {
		cnt = 1;
	}

	for (upto = 0; upto < cnt; upto += todo) {
		uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

		todo = MIN(cnt - upto, SHELL_HEXDUMP_BYTES_IN_LINE);
		ret = flash_sfdp_read(flash_dev, addr, data, todo);
		if (ret != 0) {
			shell_error(shell, "Read ERROR!");
			return -EIO;
		}
		shell_hexdump_line(shell, addr, data, todo);
		addr += todo;
	}

	shell_print(shell, "");

	return 0;
}

/**
 * @brief Shell command handler for flash erase-write test
 */
static int cmd_test(const struct shell *shell, size_t argc, char *argv[])
{
	const struct device *flash_dev;
	uint32_t repeat;
	int result;
	uint32_t addr;
	uint32_t size;

	result = parse_helper(shell, &argc, &argv, &flash_dev, &addr);
	if (result) {
		return result;
	}

	size = strtoul(argv[2], NULL, 16);
	repeat = strtoul(argv[3], NULL, 16);
	if (size > TEST_ARR_SIZE) {
		shell_error(shell, "<size> must be at most 0x%x.",
			    TEST_ARR_SIZE);
		return -EINVAL;
	}

	for (uint32_t i = 0; i < size; i++) {
		test_arr[i] = (uint8_t)i;
	}

	result = 0;

	while (repeat--) {
		result = flash_erase(flash_dev, addr, size);

		if (result) {
			shell_error(shell, "Erase Failed, code %d.", result);
			break;
		}

		shell_print(shell, "Erase OK.");

		result = flash_write(flash_dev, addr, test_arr, size);

		if (result) {
			shell_error(shell, "Write internal ERROR!");
			break;
		}

		shell_print(shell, "Write OK.");
	}

	if (result == 0) {
		shell_print(shell, "Erase-Write test done.");
	}

	return result;
}

/**
 * @brief Dynamic shell command callback for device name completion
 */
static void device_name_get(size_t idx, struct shell_static_entry *entry);

SHELL_DYNAMIC_CMD_CREATE(dsub_device_name, device_name_get);

static void device_name_get(size_t idx, struct shell_static_entry *entry)
{
	const struct device *dev = shell_device_lookup(idx, NULL);

	entry->syntax = (dev != NULL) ? dev->name : NULL;
	entry->handler = NULL;
	entry->help  = NULL;
	entry->subcmd = &dsub_device_name;
}

SHELL_STATIC_SUBCMD_SET_CREATE(flash_cmds,
	SHELL_CMD_ARG(erase, &dsub_device_name,
		"[<device>] <page address> [<size>]",
		cmd_erase, 2, 2),
	SHELL_CMD_ARG(read, &dsub_device_name,
		"[<device>] <address> [<Dword count>]",
		cmd_read, 2, 2),
	SHELL_CMD_ARG(readid, &dsub_device_name,
		"[<device>] <address> [<Dword count>]",
		cmd_read_id, 2, 2),
	SHELL_CMD_ARG(readsf, &dsub_device_name,
		"[<device>] <address> [<Dword count>]",
		cmd_read_sfdp, 2, 2),
	SHELL_CMD_ARG(test, &dsub_device_name,
		"[<device>] <address> <size> <repeat count>",
		cmd_test, 4, 1),
	SHELL_CMD_ARG(write, &dsub_device_name,
		"[<device>] <address> <dword> [<dword>...]",
		cmd_write, 3, BUF_ARRAY_CNT),
	SHELL_SUBCMD_SET_END
);

/**
 * @brief Root flash shell command handler
 */
static int cmd_flash(const struct shell *shell, size_t argc, char **argv)
{
	shell_error(shell, "%s:unknown parameter: %s", argv[0], argv[1]);
	return -EINVAL;
}

SHELL_CMD_ARG_REGISTER(flash, &flash_cmds, "Flash shell commands",
		       cmd_flash, 2, 0);