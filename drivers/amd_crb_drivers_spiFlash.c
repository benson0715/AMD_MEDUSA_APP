/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/flash/npcx_flash_api_ex.h>
#include "amd_crb_drivers_spiFlash.h"
#include "board_config.h"
#include "system.h"
#if CONFIG_PINCTRL
#include "gpio_ec.h"
#include <zephyr/drivers/pinctrl.h>
#endif
#include <zephyr/sys/byteorder.h>
#include <string.h>
#include "espi_hub.h"

#define SECTOR_SIZE 4096  // SPI Flash sector size (4KB)

// Static buffer for sector operations (4-byte aligned for flash requirements)
static uint8_t sector_buffer[SECTOR_SIZE] __aligned(4);

LOG_MODULE_REGISTER(spiFlash, CONFIG_SPI_FLASH_LOG_LEVEL);

#define SPI_FLASH_RESET_DELAY_US             40U
#define SPI_FLASH_MHZ_TO_HZ(x)               ((x) * 1000000U)
#define SPI_FLASH_MAX_RESPONSE               10U

#if CONFIG_SOC_SERIES_NPCX4
#define NPCX_SCFG_REG_ADDR DT_REG_ADDR_BY_NAME(DT_NODELABEL(scfg), scfg)
/* flash device */
static const struct device *extflash_dev;
static const struct device *intflash_dev;


#else
static struct spi_config spi_cfg_quad;
#endif
/* spi device */
static const struct device *spi_dev;
static struct spi_config spi_cfg_single;


#if CONFIG_PINCTRL
enum jesd216_dw15_qer_type {
	/* No QE status required for 1-1-4 or 1-4-4 mode */
	JESD216_DW15_QER_NONE = 0,
	JESD216_DW15_QER_S2B1v1 = 1,
	/* Bit 6 of SR byte must be set to enable 1-1-4 or 1-4-4 mode.
	 * SR is one byte.
	 */
	JESD216_DW15_QER_S1B6 = 2,
	JESD216_DW15_QER_S2B7 = 3,
	JESD216_DW15_QER_S2B1v4 = 4,
	JESD216_DW15_QER_S2B1v5 = 5,
	JESD216_DW15_QER_S2B1v6 = 6,
};

/* Device constant configuration parameters */
struct spi_qmspi_config {
	struct qmspi_regs *regs;
	uint32_t cs1_freq;
	uint32_t cs_timing;
	uint16_t taps_adj;
	uint8_t girq;
	uint8_t girq_pos;
	uint8_t girq_nvic_aggr;
	uint8_t girq_nvic_direct;
	uint8_t irq_pri;
	uint8_t pcr_idx;
	uint8_t pcr_pos;
	uint8_t port_sel;
	uint8_t chip_sel;
	uint8_t width;	/* 0(half) 1(single), 2(dual), 4(quad) */
	uint8_t unused[2];
	const struct pinctrl_dev_config *pcfg;
	void (*irq_config_func)(void);
};

/* QSPI bus configuration for a SPI device */
struct npcx_qspi_cfg {
	/* Type of Quad Enable bit in status register */
	enum jesd216_dw15_qer_type qer_type;
	/* Pinctrl for QSPI bus */
	const struct pinctrl_dev_config *pcfg;
	/* Enter four bytes address mode value */
	uint8_t enter_4ba;
	/* SPI read access type of Direct Read Access mode */
	uint8_t rd_mode;
	/* Configurations for the Quad-SPI peripherals */
	int flags;
};

/* Device config */
struct flash_npcx_nor_config {
	/* QSPI bus device for mutex control and bus configuration */
	const struct device *qspi_bus;
	/* Mapped address for flash read via direct access */
	uintptr_t mapped_addr;
	/* Size of nor device in bytes, from size property */
	uint32_t flash_size;
	/* Maximum chip erase time-out in ms */
	uint32_t max_timeout;
	/* SPI Nor device configuration on QSPI bus */
	struct npcx_qspi_cfg qspi_cfg;
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	struct flash_pages_layout layout;
#endif
};

#endif

/**
 * @brief SPI logical command needed to map to vendor-specific opcodes
 */
enum amd_crb_drivers_command_index {
	SPIF_RD_STS1_CMD_INDEX,
	SPIF_RD_STS2_CMD_INDEX,
	SPIF_EN_RST_CMD_INDEX,
	SPIF_RST_CMD_INDEX,
	SPIF_RD_JEDEC_ID_INDEX,
	SPIF_RD_DATA_INDEX,
	/* Devices of 32MB/256Mbit must also support a command to enter 4-byte
	 * address mode
	 */
	SPIF_FOUR_BYTE_CMD_INDEX,
	SPIF_WRITE_ENABLE_INDEX,
	SPIF_WRITE_NV_REGISTER_INDEX,
	SPIF_READ_NV_REGISTER_INDEX,
	SPIF_READ_SDF_INDEX,
	SPIF_4_BYTE_ADDR_ENTER_OPCODE,
	SPIF_READ_UNIQUE_ID,
	SPIF_CONTINUOUS_MODE_GET_GD,
	SPIF_CONTINUOUS_MODE_OPCODE_GD,
};


/**
 * @brief Describes a SPI command.
 */
static struct amd_crb_drivers_spi_transaction amd_crb_drivers_qspi_read[] = {
	[SPIF_RD_STS1_CMD_INDEX] = {
		.buf = { SPIF_COMMAND_READ_STATUS_REG1 },
		.tx_len = 1,
		.rx_len = 1,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	[SPIF_RD_STS2_CMD_INDEX] = {
		.buf = { SPIF_COMMAND_READ_STATUS_REG2 },
		.tx_len = 1,
		.rx_len = 2,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	[SPIF_EN_RST_CMD_INDEX] = {
		.buf = { SPIF_COMMAND_ENABLE_RST_OPCODE },
		.tx_len = 1,
		.rx_len = 0,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	[SPIF_RST_CMD_INDEX] = {
		.buf = { SPIF_COMMAND_RESET_OPCODE },
		.tx_len = 1,
		.rx_len = 0,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	[SPIF_RD_JEDEC_ID_INDEX] = {
		.buf = { SPIF_COMMAND_JEDEC_ID },
		.tx_len = 1,
		.rx_len = 2,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	[SPIF_RD_DATA_INDEX] = {
		.buf = { SPIF_COMMAND_FAST_READ, 0x00, 0x00, 0x00 },  /* Read data with 24bits address */
		.tx_len = 4,
		.rx_len = 1,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 8,
	},
	[SPIF_FOUR_BYTE_CMD_INDEX] = {
		.buf = { SPIF_COMMAND_4_BYTE_ADDR_ENTER_OPCODE },
		.tx_len = 1,
		.rx_len = 0,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	[SPIF_WRITE_ENABLE_INDEX] = {
		.buf = { SPIF_COMMAND_WRITE_ENABLE },
		.tx_len = 1,
		.rx_len = 0,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	/* Buffer data smaller than tx_len, so driver transmit dummy clocks */
	[SPIF_READ_NV_REGISTER_INDEX] = {
		.buf = { SPIF_COMMAND_READ_VOLATILE_CFG_OPCODE, 0x00, 0x00, 0x06 },
		.tx_len = 5,
		.rx_len = 1,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	[SPIF_READ_SDF_INDEX] = {
		.buf = { SPIF_COMMAND_READ_SFDP_REG, 0x00, 0x00, 0x00 },
		.tx_len = 4,
		.rx_len = 1,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 8,
	},
	[SPIF_4_BYTE_ADDR_ENTER_OPCODE] = {
		.buf = { SPIF_COMMAND_4_BYTE_ADDR_ENTER_OPCODE, 0x00, 0x00, 0x00 },
		.tx_len = 4,
		.rx_len = 1,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 0,
	},
	[SPIF_READ_UNIQUE_ID] = {
		.buf = { SPIF_COMMAND_READ_UNIQUE_ID, 0x00, 0x00, 0x00 },
		.tx_len = 4,
		.rx_len = 1,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 8,
	},
	[SPIF_CONTINUOUS_MODE_GET_GD] = {
		.buf = { SPIF_COMMAND_CONTINUOUS_MODE_GET_GD, 0x00, 0x00, 0x06 },
		.tx_len = 4,
		.rx_len = 1,
		.mode = SPI_LINES_SINGLE,
		.dummyCycle = 8,
	},
	[SPIF_CONTINUOUS_MODE_OPCODE_GD] = {
	.buf = { SPIF_COMMAND_CONTINUOUS_MODE_OPCODE_GD, 0x00, 0x00, 0x06,0xFE },
	.tx_len = 5,
	.rx_len = 1,
	.mode = SPI_LINES_SINGLE,
	.dummyCycle = 0,
	},

};

/**
 * @brief Returns SPI command structure from command table
 *
 * @param [in]   command:   SPI command idx
 * @return spi command structure
 */
static struct amd_crb_drivers_spi_transaction spi_cmd(enum amd_crb_drivers_command_index command)
{
	return amd_crb_drivers_qspi_read[command];
}

/**
 * @brief Check if SPI flash device is ready
 *
 * @return true if spi flash is ready, false otherwise
 */
bool amd_crb_drivers_sFlashReady(void)
{
	if (spi_dev == NULL)
		return false;
	else
		return true;
}

/**
 * @brief Initialize SPI flash device
 *
 * @return 0 if successful, negative error code otherwise
 */
int amd_crb_drivers_sFlashInit(void)
{
#if CONFIG_SOC_SERIES_NPCX4
	extflash_dev =  DEVICE_DT_GET(EXT_FLASH);
	if (!device_is_ready(extflash_dev)) {
		LOG_ERR("FLASH device not ready");
		return -ENODEV;
	};
#if 0
	//flash information dump
	unsigned char buf[20];
	int ret =0;
	*(unsigned char*)(NPCX_SCFG_REG_ADDR) &= ~(BIT(6));
	ret = flash_ex_op(extflash_dev,FLASH_NPCX_EX_OP_SPI_UID,0,buf);
	printk("flash0 unique id =%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14]);

	if (ret) {
		return ret;
	}
	ret = flash_read_jedec_id(extflash_dev,buf);
	printk("flash0 id =%x %x %x\n",buf[0],buf[1],buf[2]);

 	buf[0] =0;
	buf[1] =0;
	buf[2] =0;

	ret = flash_read(extflash_dev,0x00,buf,3);

	printk("flash0 data[0x00-0x02] =%x %x %x\n",buf[0],buf[1],buf[2]);
	static const struct device *extflash_dev1;
	extflash_dev1 =  DEVICE_DT_GET( DT_NODELABEL(ext_flash1));
	if (!device_is_ready(extflash_dev1)) {
		LOG_ERR("FLASH device not ready");
		return -ENODEV;
	};
 	buf[0] =0;
	buf[1] =0;
	buf[2] =0;

	ret = flash_ex_op(extflash_dev1,FLASH_NPCX_EX_OP_SPI_UID,0,buf);
	printk("flash1 unique id =%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14]);

	if (ret) {
		return ret;
	}
	ret = flash_read_jedec_id(extflash_dev1,buf);
	printk("flash1 id =%x %x %x\n",buf[0],buf[1],buf[2]);
	buf[0] =0;
	buf[1] =0;
	buf[2] =0;

	ret = flash_read(extflash_dev1,0x00,buf,3);
	*(unsigned char*)(NPCX_SCFG_REG_ADDR) |= (BIT(6));
	printk("flash1 data[0x00-0x02] =%x %x %x\n",buf[0],buf[1],buf[2]);
#endif
#else
	spi_dev =  DEVICE_DT_GET(SPI_0);
	if (!device_is_ready(spi_dev)) {
		LOG_ERR("SPI device not ready");
		return -ENODEV;
	};

	/* configure spi single mode */
	spi_cfg_single.frequency = SPI_FLASH_MHZ_TO_HZ(8);
	spi_cfg_single.operation = (uint16_t) (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB
			    | SPI_WORD_SET(8) | SPI_MODE_CPHA | SPI_MODE_CPOL | SPI_LINES_SINGLE);
	spi_cfg_single.slave = 0;
	spi_cfg_single.cs = NULL;

	/* configure spi quad mode */
	spi_cfg_quad.frequency = SPI_FLASH_MHZ_TO_HZ(8);
	spi_cfg_quad.operation = (uint16_t) (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB
				| SPI_WORD_SET(8) | SPI_MODE_CPHA | SPI_MODE_CPOL | SPI_LINES_QUAD);
	spi_cfg_quad.slave = 0;
	spi_cfg_quad.cs = NULL;

#if CONFIG_PINCTRL
	/* Workaround the SPI CLK would cause system cannot bootup */
	gpio_force_configure_pin(SPI_EC_ROM_CS0_N, GPIO_INPUT);
	gpio_force_configure_pin(SPI_EC_ROM_CLK, GPIO_INPUT);
	gpio_force_configure_pin(SPI_EC_ROM_D0, GPIO_INPUT);
	gpio_force_configure_pin(SPI_EC_ROM_D1, GPIO_INPUT);
#ifdef SPI_EC_ROM_D2
		gpio_force_configure_pin(SPI_EC_ROM_D2, GPIO_INPUT);
#endif
#ifdef SPI_EC_ROM_D3
		gpio_force_configure_pin(SPI_EC_ROM_D3, GPIO_INPUT);
#endif
	#endif
#endif
	return 0;
}


/**
 * @brief Send SPI flash command and get response
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   cmd:           command code
 * @param [in]   resp:          resp buffer pointer
 * @param [in]   mode:          spi mode
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlashSendCmd(uint8_t slave_index, struct amd_crb_drivers_spi_transaction cmd, uint8_t *resp, uint32_t mode)
{
	int ret;
	uint8_t data[SPI_FLASH_MAX_RESPONSE];
	struct spi_buf rx;
	struct spi_buf tx[2];
	struct spi_buf_set rx_bufs = {
		.buffers = NULL,
		.count = 0,
	};

	struct spi_buf_set tx_bufs = {
		.buffers = &tx[0],
		.count = 1,
	};

#if CONFIG_PINCTRL
	const struct spi_qmspi_config *cfg = spi_dev->config;
#endif

#if CONFIG_PINCTRL
	/**
	 * Workaround the SPI CLK would cause system cannot bootup
	 * Set SPI bus to native function before spi_transceive
	 */
	pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
#endif

	tx[0].buf = cmd.buf;
	tx[0].len = cmd.tx_len;

	if (cmd.dummyCycle) {
		tx_bufs.count = 2;
		tx[1].buf = NULL;
		tx[1].len = cmd.dummyCycle;
	}

	if (resp == NULL)
		rx.buf = data;
	else
		rx.buf = resp;
	rx.len = cmd.rx_len;

	/**Increase SPI driver compatibility, do not indicate RX buffer despite
	 * of the buffers been empty
	 */
	if (cmd.rx_len) {
		rx_bufs.buffers = &rx;
		rx_bufs.count = 1;
	}

	spi_cfg_single.slave = slave_index;

	ret = spi_transceive(spi_dev, &spi_cfg_single, &tx_bufs, &rx_bufs);
	if (ret != 0) {
		LOG_ERR("SPI transceive error: %d", ret);
		return ret;
	}

#if CONFIG_PINCTRL
	/* Workaround the SPI CLK would cause system cannot bootup */
	gpio_force_configure_pin(SPI_EC_ROM_CS0_N, GPIO_INPUT);
	gpio_force_configure_pin(SPI_EC_ROM_CLK, GPIO_INPUT);
	gpio_force_configure_pin(SPI_EC_ROM_D0, GPIO_INPUT);
	gpio_force_configure_pin(SPI_EC_ROM_D1, GPIO_INPUT);
#ifdef SPI_EC_ROM_D2
	gpio_force_configure_pin(SPI_EC_ROM_D2, GPIO_INPUT);
#endif
#ifdef SPI_EC_ROM_D3
	gpio_force_configure_pin(SPI_EC_ROM_D3, GPIO_INPUT);
#endif
	#endif
	return 0;
}

/**
 * @brief Reset SPI flash device
 *
 * @return 0 successful
 */
int amd_crb_drivers_spi_reset(void)
{
	int ret;

	extflash_dev =	DEVICE_DT_GET(EXT_FLASH);
#if CONFIG_PINCTRL
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) &= ~(BIT(6));
	}
#endif
	ret = flash_ex_op(extflash_dev,FLASH_NPCX_EX_OP_SPI_RESET,0,0);
#if CONFIG_PINCTRL
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) |= (BIT(6));
	}
#endif
	return 0;
}


/**
 * @brief Read data from SPI flash
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   offset:        flash offset to read
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlashRead(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp)
{
	int ret;
#if CONFIG_SOC_SERIES_NPCX4
		extflash_dev =	DEVICE_DT_GET(EXT_FLASH);
#if CONFIG_PINCTRL
	//const struct flash_npcx_nor_config *cfg = extflash_dev->config;
#endif

#if CONFIG_PINCTRL
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) &= ~(BIT(6));
	}
#endif
	
	ret = flash_read(extflash_dev,offset,resp,size);
	if (ret != 0) {
		LOG_ERR("SPI read error: %d,offset=%x size=%d", ret,offset,size);
		return ret;
	}
	
#if CONFIG_PINCTRL
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) |= (BIT(6));
	}
#endif

	
#else	
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = { SPIF_RD_DATA_INDEX };

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);

		spi_command.rx_len = size;
		spi_command.buf[1] = (uint8_t)(offset >> 16);
		spi_command.buf[2] = (uint8_t)(offset >> 8);
		spi_command.buf[3] = (uint8_t)offset;

		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);

		if (ret != 0) {
			LOG_ERR("SPI read error: %d", ret);
			return ret;
		}

	}
#endif
	return 0;
}

/**
 * @brief Read data from internal SPI flash
 *
 * @param [in]   offset:        flash offset to read
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlash_int_read(uint32_t offset, uint32_t size, uint8_t *resp)
{
	int ret;
#if CONFIG_SOC_SERIES_NPCX4
	intflash_dev =	DEVICE_DT_GET(INT_FLASH);
	ret = flash_read(intflash_dev,offset,resp,size);
	if (ret != 0) {
		LOG_ERR("SPI read error: %d,offset=%x size=%d", ret,offset,size);
		return ret;
	}
#endif
	return 0;
}

/**
 * @brief Write data to internal SPI flash
 *
 * @param [in]   offset:        flash offset to write
 * @param [in]   size:          data length
 * @param [in]   resp:          source data buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlash_int_write(uint32_t offset, uint32_t size, uint8_t *resp)
{
	int ret;
#if CONFIG_SOC_SERIES_NPCX4
	intflash_dev =	DEVICE_DT_GET(INT_FLASH);
	ret = flash_write(intflash_dev,offset,resp,size);
	if (ret != 0) {
		LOG_ERR("SPI read error: %d,offset=%x size=%d", ret,offset,size);
		return ret;
	}
#endif
	return 0;
}

/**
 * @brief Erase internal SPI flash sector
 *
 * @param [in]   offset:        flash offset to erase
 * @param [in]   size:          erase size
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlash_int_erase(uint32_t offset, uint32_t size)
{
	int ret;
#if CONFIG_SOC_SERIES_NPCX4
	intflash_dev =	DEVICE_DT_GET(INT_FLASH);
	ret = flash_erase(intflash_dev,offset,size);
	if (ret != 0) {
		LOG_ERR("SPI read error: %d,offset=%x size=%d", ret,offset,size);
		return ret;
	}
#endif
	return 0;
}

/**
 * @brief Read unique ID from internal SPI flash
 *
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlash_int_sUniqId(uint32_t size, uint8_t *resp)
{
	int ret;
	uint8_t tmp[16] = {0};

#if CONFIG_SOC_SERIES_NPCX4
	intflash_dev =	DEVICE_DT_GET(INT_FLASH);
	memset(resp, 0x0, size);
	ret = flash_ex_op(intflash_dev, FLASH_NPCX_EX_OP_SPI_UID, 0, tmp);
	if (ret) {
		return ret;
	}
#endif

	/* FIXME: NPCX driver issue!!!                      */
	/* Dummy Bytes 4 + UNIQUE ID Bytes 8 + RSVD Bytes 4 */
	/* Pls follow W25Q80 4BH spec                       */
	memcpy(resp, &tmp[4], 8);
	return 0;
}

/**
 * @brief Read SPI flash status register
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   resp:          resp buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlashQspiReadStatus(uint8_t slave_index, uint8_t *resp)
{
	int ret;
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = {
		SPIF_RD_STS1_CMD_INDEX
	};


	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);
		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);
		if (ret) {
			return ret;
		}
	}
	return 0;
}

/**
 * @brief Send reset command to SPI flash device
 *
 * @param [in]   slave_index:   spi flash number
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlashQspiRestDev(uint8_t slave_index)
{
	int ret;
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = { SPIF_EN_RST_CMD_INDEX, SPIF_RST_CMD_INDEX };

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);
		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, NULL,
				SPI_LINES_QUAD);
		if (ret) {
			return ret;
		}
	}

	k_busy_wait(SPI_FLASH_RESET_DELAY_US);
	return ret;
}

/**
 * @brief Read SPI flash JEDEC ID
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   resp:          resp buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sFlashJedecId(uint8_t slave_index, uint8_t *resp)
{
	int ret;
#if CONFIG_SOC_SERIES_NPCX4
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) &= ~(BIT(6));
	}
	ret = flash_read_jedec_id(extflash_dev,resp);
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char*)(NPCX_SCFG_REG_ADDR) |= (BIT(6));
	}
	if (ret) {
		return ret;
	}
#else
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = { SPIF_RD_JEDEC_ID_INDEX };


	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);
		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);
		if (ret) {
			return ret;
		}

	}
#endif
	return 0;
}


/**
 * @brief Read SFDP table from SPI flash
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   offset:        flash offset to read
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_flashSfdpTableRead(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp)
{
 int ret;
#if CONFIG_SOC_SERIES_NPCX4
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) &= ~(BIT(6));
	}
	ret = flash_sfdp_read(extflash_dev,offset,resp,size);
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) |= (BIT(6));
	}
	if (ret) {
		return ret;
	}
#else
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = { SPIF_READ_SDF_INDEX };

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);

		spi_command.rx_len = size;
		spi_command.buf[1] = (uint8_t)(offset >> 16);
		spi_command.buf[2] = (uint8_t)(offset >> 8);
		spi_command.buf[3] = (uint8_t)offset;

		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);

		if (ret != 0) {
			LOG_ERR("SPI read error: %d", ret);
			return ret;
		}

	}
#endif
	return 0;
}

/**
 * @brief Read unique ID from SPI flash
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   offset:        flash offset to read
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_sUniqId(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp)
{
	int ret;
#if CONFIG_SOC_SERIES_NPCX4
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) &= ~(BIT(6));
	}
	ret = flash_ex_op(extflash_dev,FLASH_NPCX_EX_OP_SPI_UID,0,resp);
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) |= (BIT(6));
	}
	if (ret) {
		return ret;
	}
#else
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = { SPIF_READ_UNIQUE_ID };

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);

		spi_command.rx_len = size;
		spi_command.buf[1] = (uint8_t)(offset >> 16);
		spi_command.buf[2] = (uint8_t)(offset >> 8);
		spi_command.buf[3] = (uint8_t)offset;

		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);

		if (ret != 0) {
			LOG_ERR("SPI read error: %d", ret);
			return ret;
		}

	}
#endif
	return 0;
}

/**
 * @brief Get SPI ROM configuration
 *
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful
 *
 * @note
 *  REQUEST_SPI_ROM_CONFIGURATION 19B Manufacture
 *  ID  resp[0]= 8b
 *  Memory Type resp[1] = 8b
 *  Capacity resp[2] = 8b
 *  Signature &resp[3] = 128b
 */
int amd_crb_drivers_spi_rom_config(uint8_t *resp)
{
	int ret;
	extflash_dev =	DEVICE_DT_GET(EXT_FLASH);
#if CONFIG_PINCTRL
	/**
	 * Workaround the SPI CLK would cause system cannot bootup
	 * Set SPI bus to native function before spi_transceive
	 */
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) &= ~(BIT(6));
	}
#endif
	ret = flash_read_jedec_id(extflash_dev,resp);
	ret = flash_ex_op(extflash_dev,FLASH_NPCX_EX_OP_SPI_UID,0,resp+3);
#if CONFIG_PINCTRL
	if (espihub_get_saf_boot_mode() == false) {
		*(unsigned char *)(NPCX_SCFG_REG_ADDR) |= (BIT(6));
	}
#endif
	return 0;
}

/**
 * @brief Get continuous mode status from SPI flash
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   offset:        flash offset to read
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_GetContinueMode(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp)
{
	int ret;
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = { SPIF_CONTINUOUS_MODE_GET_GD };

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);

		spi_command.rx_len = size;
		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);

		if (ret != 0) {
			LOG_ERR("SPI read error: %d", ret);
			return ret;
		}

	}
	return 0;
}

/**
 * @brief Get status register 2 from SPI flash
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   offset:        flash offset to read
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_GetStatus2(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp)
{
	int ret;
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = { SPIF_RD_STS2_CMD_INDEX };

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);

		spi_command.rx_len = size;
		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);

		if (ret != 0) {
			LOG_ERR("SPI read error: %d", ret);
			return ret;
		}

	}
	return 0;
}

/**
 * @brief Set continuous mode on SPI flash
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   offset:        flash offset to read
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_SetContinueMode(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp)
{
	int ret;
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds1[] = { SPIF_WRITE_ENABLE_INDEX, };
	
		for (int i = 0; i < ARRAY_SIZE(cmds1); i++) {
			spi_command = spi_cmd(cmds1[i]);
			ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, NULL,
					SPI_LINES_QUAD);
			if (ret) {
				return ret;
			}
		}
		
	enum amd_crb_drivers_command_index cmds[] = { SPIF_CONTINUOUS_MODE_OPCODE_GD };

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);

		spi_command.rx_len = size;
		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);

		if (ret != 0) {
			LOG_ERR("SPI read error: %d", ret);
			return ret;
		}

	}
	return 0;
}

/**
 * @brief Set quad mode on SPI flash
 *
 * @param [in]   slave_index:   spi flash number
 * @param [in]   offset:        flash offset to read
 * @param [in]   size:          data length
 * @param [in]   resp:          response buffer pointer
 * @return 0 successful, negative error code on failure
 */
int amd_crb_drivers_SetQuaMode(uint8_t slave_index, uint32_t offset, uint32_t size, uint8_t *resp)
{

	int ret;
	struct amd_crb_drivers_spi_transaction spi_command;

	enum amd_crb_drivers_command_index cmds[] = {
		SPIF_FOUR_BYTE_CMD_INDEX
	};

	for (int i = 0; i < ARRAY_SIZE(cmds); i++) {
		spi_command = spi_cmd(cmds[i]);
		ret = amd_crb_drivers_sFlashSendCmd(slave_index, spi_command, resp,
				 SPI_LINES_SINGLE);
		if (ret) {
			return ret;
		}
	}
	return 0;
}



/**
 * @brief Writes data to flash memory with byte-level granularity
 * 
 * @param offset Starting address for write operation
 * @param size Number of bytes to write
 * @param resp Pointer to source data buffer
 * @return int 0 on success, negative error code on failure
 * 
 * This function handles the read-modify-write cycle required for
 * flash memory which requires full sector erasure before writing.
 */
int amd_crb_flash_byte_write(uint32_t offset, uint32_t size, uint8_t *resp)
{
	int rc;
	off_t sector_start;
	size_t chunk_len;
	uint16_t sector_offset;
	const uint8_t *src = (const uint8_t *)resp;
#if CONFIG_SOC_SERIES_NPCX4	
	intflash_dev =	DEVICE_DT_GET(INT_FLASH);
	// Validate input parameters
	if (resp == NULL || size == 0) {
		return -EINVAL;
	}

	while (size > 0) {
		// Calculate current sector boundaries
		sector_start = offset & ~(SECTOR_SIZE - 1);
		sector_offset = offset - sector_start;

		// Determine maximum writable chunk in current sector
		chunk_len = MIN(size, SECTOR_SIZE - sector_offset);

		// Read entire sector into buffer
		rc = flash_read(intflash_dev, sector_start, sector_buffer, SECTOR_SIZE);
		if (rc != 0) {
			return rc;  // Propagate read error
		}

		// Check if sector erase is required
		bool need_erase = false;
		for (size_t i = 0; i < chunk_len; i++) {
			// Detect if we need to change 0-1 bits (requires erase)
			if ((src[i] & ~sector_buffer[sector_offset + i]) != 0) {
			need_erase = true;
			break;
			}
		}

		if (need_erase) {
			// Update sector buffer with new data
			memcpy(&sector_buffer[sector_offset], src, chunk_len);

		// Perform erase-write cycle
			rc = flash_erase(intflash_dev, sector_start, SECTOR_SIZE);
			if (rc != 0) {
				return rc;  // Propagate erase error
			}

			rc = flash_write(intflash_dev, sector_start, sector_buffer, SECTOR_SIZE);
			if (rc != 0) {
				return rc;  // Propagate write error
			}
		} else {
			// Direct write (only changing 1-0 bits)
			rc = flash_write(intflash_dev, offset, src, chunk_len);
			if (rc != 0) {
				return rc;  // Propagate write error
			}
		}

		// Advance to next chunk
		offset += chunk_len;
		src += chunk_len;
		size -= chunk_len;
	}
#endif
	return 0;  // Success
}