/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "eeprom.h"

struct args_index {
	uint8_t device;
	uint8_t offset;
	uint8_t length;
	uint8_t data;
	uint8_t pattern;
};

static const struct args_index args_indx = {
	.offset = 1,
	.length = 2,
	.data = 2,
	.pattern = 3,
};

/**
 * @brief Shell command handler to read data from EEPROM
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector containing offset and length
 * @return 0 on success, negative error code on failure
 */
static int cmd_read(const struct shell *shell, size_t argc, char **argv)
{
	size_t addr;
	size_t len;
	size_t pending;
	size_t upto;
	int err;

	addr = strtoul(argv[args_indx.offset], NULL, 0);
	len = strtoul(argv[args_indx.length], NULL, 0);


	shell_print(shell, "Reading %d bytes from EEPROM, offset %d...", len,
		    addr);

	for (upto = 0; upto < len; upto += pending) {
		uint8_t data[SHELL_HEXDUMP_BYTES_IN_LINE];

		pending = MIN(len - upto, SHELL_HEXDUMP_BYTES_IN_LINE);
		err = eeprom_read_block(addr, pending, data);
		if (err) {
			shell_error(shell, "EEPROM read failed (err %d)", err);
			return err;
		}

		shell_hexdump_line(shell, addr, data, pending);
		addr += pending;
	}

	shell_print(shell, "");
	return 0;
}

/**
 * @brief Shell command handler to write data to EEPROM
 *
 * @param shell Pointer to shell instance
 * @param argc Argument count
 * @param argv Argument vector containing offset and data bytes
 * @return 0 on success, negative error code on failure
 */
static int cmd_write(const struct shell *shell, size_t argc, char **argv)
{
	uint8_t wr_buf[CONFIG_AMD_EEPROM_SHELL_BUFFER_SIZE];
	uint8_t rd_buf[CONFIG_AMD_EEPROM_SHELL_BUFFER_SIZE];
	unsigned long byte;
	int offset;
	size_t len;
	int err;
	int i;

	offset = strtoul(argv[args_indx.offset], NULL, 0);
	len = argc - args_indx.data;

	if (len > sizeof(wr_buf)) {
		shell_error(shell, "Write buffer size (%d bytes) exceeded",
			    sizeof(wr_buf));
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		byte = strtoul(argv[args_indx.data + i], NULL, 0);
		if (byte > UINT8_MAX) {
			shell_error(shell, "Error parsing data byte %d", i);
			return -EINVAL;
		}
		wr_buf[i] = byte;
	}

	shell_print(shell, "Writing %d bytes to EEPROM...", len);

	err = eeprom_write_block(offset, len, wr_buf);
	if (err) {
		shell_error(shell, "EEPROM write failed (err %d)", err);
		return err;
	}

	shell_print(shell, "Verifying...");

	err = eeprom_read_block(offset, len, rd_buf);
	if (err) {
		shell_error(shell, "EEPROM read failed (err %d)", err);
		return err;
	}

	if (memcmp(wr_buf, rd_buf, len) != 0) {
		shell_error(shell, "Verify failed");
		return -EIO;
	}

	shell_print(shell, "Verify OK");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(eeprom_cmds,
	SHELL_CMD_ARG(read, NULL, "<offset> <length>", cmd_read, 3, 0),
	SHELL_CMD_ARG(write, NULL,
		      "<offset> [byte0] <byte1> .. <byteN>", cmd_write,
		      3, CONFIG_AMD_EEPROM_SHELL_BUFFER_SIZE - 1),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(eeprom, &eeprom_cmds, "EEPROM shell commands", NULL);