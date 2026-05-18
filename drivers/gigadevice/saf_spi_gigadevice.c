/**
* Copyright (c) 2019 Intel Corporation
* Modifications copyright (c) 2023 Advanced Micro Devices, Inc.
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/espi_saf.h>
#include "saf_spi_transaction.h"
#include "saf_spi_gigadevice.h"

#define GD25Q_CAPACITY_BYTES (0x4000000U)

static struct saf_spi_transaction gigadevice_qspi_cmds[] = {
	[RD_STS1_CMD_INDEX] = {
		.buf = { READ_STATUS_1_OPCODE },
		.tx_len = 1,
		.rx_len = 1,
		.mode = SPI_LINES_SINGLE,
	},
	[RD_STS2_CMD_INDEX] = {
		.buf = { READ_STATUS_2_OPCODE },
		.tx_len = 1,
		.rx_len = 2,
		.mode = SPI_LINES_SINGLE,
	},
	[EN_RST_CMD_INDEX] = {
		.buf = { ENABLE_RESET_OPCODE },
		.tx_len = 1,
		.rx_len = 0,
	},
	[RST_CMD_INDEX] = {
		.buf = { RESET_OPCODE },
		.tx_len = 1,
		.rx_len = 0,
	},
	[FOUR_BYTE_CMD_INDEX] = {
		.buf = { FOUR_BYTE_ADDRESS_ENTER_OPCODE },
		.tx_len = 1,
		.rx_len = 0,
	},
	[WRITE_ENABLE_INDEX] = {
		.buf = { WRITE_ENABLE_OPCODE,  },
		.tx_len = 1,
		.rx_len = 0,
	},
	/* XIP register address 0x000006, new value 0xFE */
	[WRITE_NV_REGISTER_INDEX] = {
		.buf = { WRITE_VOLATILE_CFG_OPCODE, 0x00, 0x00, 0x06, 0xFE },
		.tx_len = 5,
		.rx_len = 0,
	},
	/* Buffer data smaller than tx_len, so driver transmit dummy clocks */
	[READ_NV_REGISTER_INDEX] = {
		.buf = { READ_VOLATILE_CFG_OPCODE, 0x00, 0x00, 0x06 },
		.tx_len = 5,
		.rx_len = 1,
	},
};

/**
 * @brief Get GigaDevice QSPI command transaction structure
 *
 * @param command Command index to retrieve
 * @return Pointer to the SPI transaction structure for the specified command
 */
struct saf_spi_transaction *gigadevice_qspi_cmd(enum saf_command_index command)
{
	__ASSERT(command < ARRAY_SIZE(gigadevice_qspi_cmds),
		 "Invalid index");
	return &gigadevice_qspi_cmds[command];
}