/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <zephyr/logging/log.h>

#include <dev_rrv686xx.h>
#include <i2c_hub.h>

#include "amd_crb_drivers_spiFlash.h"
#ifndef RV686XX_I2C_PORT
#define RV686XX_I2C_PORT I2C_1
#endif
#ifndef RV686XX_I2C_ADDRESS
#define RV686XX_I2C_ADDRESS 0x60
#endif
LOG_MODULE_REGISTER(rrv686xx, CONFIG_SVI3_LOG_LEVEL);

/**
 * @brief Perform register access (read/write) on the RRV686XX device.
 *
 * @param isRead  True for read operation, false for write operation.
 * @param slv     I2C slave address of the device.
 * @param reg     Register address to access.
 * @param pByte   Pointer to the data buffer for read/write.
 * @param len     Length of the data buffer.
 * @return        0 on success, non-zero on failure.
 */
int dev_rrv686xx_regAccess(bool isRead, uint8_t slv, uint8_t reg, uint8_t *pByte, uint8_t len)
{
	uint8_t retry = 3;
	int acks;

	while (retry) {
		retry--;
		acks = 0;
		if (isRead) {
			acks = i2c_hub_burst_read(RV686XX_I2C_PORT, slv, reg, pByte, len);
			if (acks != 0)
				continue;
		} else {
			acks = i2c_hub_burst_write(RV686XX_I2C_PORT, slv, reg, pByte, len);
		}
		if (acks == 0)
			return acks;
	}
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] on rrv686xx @ %02X %s [%02X], acks %d", slv,
			isRead ? "R" : "W", reg, acks);
	}
	return acks;
}

/**
 * @brief Read bytes from the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param offset  Register offset to read from.
 * @param data    Pointer to the buffer to store the read data.
 * @param len     Number of bytes to read.
 */
void dev_rrv686xx_read_bytes(uint8_t address, uint8_t offset, uint8_t *data, uint8_t len)
{
	dev_rrv686xx_regAccess(1, address, offset, data, len);
}

/**
 * @brief Write bytes to the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param offset  Register offset to write to.
 * @param data    Pointer to the buffer containing the data to write.
 * @param len     Number of bytes to write.
 */
void dev_rrv686xx_write_bytes(uint8_t address, uint8_t offset, uint8_t *data, uint8_t len)
{
	dev_rrv686xx_regAccess(0, address, offset, data, len);
}

/**
 * @brief Perform a DMA read operation on the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param cmd     DMA command to execute.
 * @param data    Pointer to store the read data.
 * @return        0 on success, non-zero on failure.
 */
int dev_rrv686xx_dma_read(uint8_t address, uint16_t cmd, uint32_t *data)
{
	int acks;
	uint8_t cmdBuffer[2];
	cmdBuffer[0] = (uint8_t)cmd;
	cmdBuffer[1] = (uint8_t)(cmd >> 8);
	acks = dev_rrv686xx_regAccess(0, address, DEV_RRV686XX_DMA_ADDRESS, cmdBuffer, 2);
	if (acks != 0)
		return acks;

	uint8_t dataBuffer[4];
	acks = dev_rrv686xx_regAccess(1, address, DEV_RRV686XX_DMA_DATA, dataBuffer, 4);
	if (acks == 0) {
		*data = dataBuffer[0] + (dataBuffer[1] << 8) + (dataBuffer[2] << 16) +
			(dataBuffer[3] << 24);
	}
	LOG_DBG("dma_read cmd 0x%x data 0x%x", cmd, *data);
	return acks;
}

/**
 * @brief Perform a DMA write operation on the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param cmd     DMA command to execute.
 * @param data    Data to write.
 * @return        0 on success, non-zero on failure.
 */
int dev_rrv686xx_dma_write(uint8_t address, uint16_t cmd, uint32_t data)
{
	int acks = 0;
	uint8_t cmdBuffer[2];
	uint8_t dataBuffer[4];

	cmdBuffer[0] = (uint8_t)cmd;
	cmdBuffer[1] = (uint8_t)(cmd >> 8);
	acks = dev_rrv686xx_regAccess(0, address, DEV_RRV686XX_DMA_ADDRESS, cmdBuffer, 2);

	dataBuffer[0] = (uint8_t)data;
	dataBuffer[1] = (uint8_t)(data >> 8);
	dataBuffer[2] = (uint8_t)(data >> 16);
	dataBuffer[3] = (uint8_t)(data >> 24);
	acks = dev_rrv686xx_regAccess(0, address, DEV_RRV686XX_DMA_DATA, dataBuffer, 4);
	if (acks != 0)
		return acks;

	return acks;
}

/**
 * @brief Check if the RRV686XX device is ready to respond to I2C commands.
 *
 * @param address I2C slave address of the device.
 * @return        True if the device is ready, false otherwise.
 */
bool dev_rrv686xx_ready(uint8_t address)
{
	uint8_t inputBuffer[8], outputBuffer[8];
	uint32_t acks;

	inputBuffer[0] = (uint8_t)DEV_RRV686XX_CMD_DEVICE_DATA;
	inputBuffer[1] = (uint8_t)(DEV_RRV686XX_CMD_DEVICE_DATA >> 8);
	acks = dev_rrv686xx_regAccess(0, address, DEV_RRV686XX_DMA_ADDRESS, inputBuffer, 2);
	if (acks != 0)
		return false;

	acks = dev_rrv686xx_regAccess(1, address, DEV_RRV686XX_DMA_DATA, outputBuffer, 4);
	if (acks != 0)
		return false;

	if ((outputBuffer[0] != 0xFF) || (outputBuffer[1] != 0xFF)) {
		LOG_DBG("rrv686xx Device Ready");
		return true;
	} else
		return false;
}

/**
 * @brief Retrieve device data from the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return        Device data on success, 0 on failure.
 */
uint32_t dev_rrv686xx_retrieveDeviceData(uint8_t address)
{
	uint32_t acks, device_data = 0;
	acks = dev_rrv686xx_dma_read(address, DEV_RRV686XX_CMD_DEVICE_DATA, &device_data);
	if (acks != 0)
		return false;
	LOG_DBG("RRV686XX device data: 0x%x", device_data);
	return device_data;
}

/**
 * @brief Write modified data to the RRV686XX device.
 *
 * @param address  I2C slave address of the device.
 * @param dev_data Data to write.
 * @return         True on success, false otherwise.
 */
bool dev_rrv686xx_writemodifieddata(uint8_t address, uint32_t dev_data)
{
	uint32_t acks;
	acks = dev_rrv686xx_dma_write(address,DEV_RRV686XX_CMD_DEVICE_DATA,dev_data);
	return acks;
}

/**
 * @brief Read the device ID from the binary image stored in SPI flash.
 *
 * @param spiOffset Offset in SPI flash where the binary image is stored.
 * @return          Device ID on success, 0 on failure.
 */
uint32_t dev_rrv686xx_devId(uint32_t spiOffset)
{
	uint8_t inputBuf[16];
	uint32_t ret = 0;

	/* read the image via spi */
	if (amd_crb_drivers_sFlashRead(0, spiOffset, 16, inputBuf)) {
		LOG_DBG("!!error!! fail to read SPI at 0x%06X", spiOffset);
		return 0;
	}
	if (inputBuf[0] != 0x49) {
		LOG_DBG("!!error!! fail with wrong record type");
		return 0;
	}

	ret = (inputBuf[4] << 24) + (inputBuf[5] << 16) + (inputBuf[6] << 8) + inputBuf[7];
	LOG_DBG("rrv686xx device id hex: 0x%x\n", ret);
	return ret;
}

/**
 * @brief Retrieve the I2C address of the RRV686XX device from the binary image.
 *
 * @param spiOffset Offset in SPI flash where the binary image is stored.
 * @return          I2C address on success, 0 on failure.
 */
uint8_t dev_rrv686xx_i2cAddress(uint32_t spiOffset)
{
	uint8_t inputBuf[16];
	uint32_t ret = 0;

	/* read the image via spi */
	if (amd_crb_drivers_sFlashRead(0, spiOffset, 16, inputBuf)) {
		LOG_DBG("!!error!! fail to read SPI at 0x%06X", spiOffset);
		return 0;
	}
	if (inputBuf[0] != 0x49) {
		LOG_DBG("!!error!! fail with wrong record type");
		return 0;
	}
	ret = inputBuf[2] >> 1;
	LOG_DBG("rrv686xx i2c address: %x", ret);
	return ret;
}

/**
 * @brief Check if the device ID matches the expected value.
 *
 * @param address   I2C slave address of the device.
 * @param spiOffset Offset in SPI flash where the binary image is stored.
 * @return          True if the device ID matches, false otherwise.
 */
bool dev_rrv686xx_checkDevId(uint8_t address, uint32_t spiOffset)
{
	uint8_t outputBuffer[8];
	uint32_t acks, devId_hex = 0, devId_reg;

	acks = dev_rrv686xx_regAccess(1, address, DEV_RRV686XX_DMA_DEV_ID, outputBuffer, 5);
	if (acks != 0)
		return false;

	/* The number of available NVM configuration will be a value from 0 to 24 */
	devId_reg = outputBuffer[1] + (outputBuffer[2] << 8) + (outputBuffer[3] << 16) +
		    (outputBuffer[4] << 24);
	devId_hex = dev_rrv686xx_devId(spiOffset);
	if (devId_hex == devId_reg) {
		LOG_DBG("rrv686xx device id match: 0x%x 0x%x", devId_hex, devId_reg);
		return true;
	} else {
		LOG_WRN("Warnning: rrv686xx device id not match: 0x%x 0x%x", devId_hex, devId_reg);
		/* return to true even if device id not match. Because device id register can be
		 * overwritten */
		return true;
	}
}

/**
 * @brief Check if the device revision ID matches the expected value.
 *
 * @param address   I2C slave address of the device.
 * @param spiOffset Offset in SPI flash where the binary image is stored.
 * @return          True if the revision ID matches, false otherwise.
 */
bool dev_rrv686xx_checkRevId(uint8_t address, uint32_t spiOffset)
{
	uint8_t outputBuffer[8];
	uint32_t acks, RevId_reg;

	acks = dev_rrv686xx_regAccess(1, address, DEV_RRV686XX_DMA_DEV_REV, outputBuffer, 5);
	if (acks != 0) {
		return false;
	}
	/* The number of available NVM configuration will be a value from 0 to 24 */
	RevId_reg = outputBuffer[1] + (outputBuffer[2] << 8) + (outputBuffer[3] << 16) +
		    (outputBuffer[4] << 24);
	LOG_WRN("outputBuffer[1] %x outputBuffer[2] %x", outputBuffer[1], outputBuffer[2]);
	LOG_WRN("Warnning: rrv686xx Rev id %x", RevId_reg);
	// devId_hex = dev_rrv686xx_devId(spiOffset);
	// if (devId_hex == devId_reg) {
	// 	LOG_DBG("rrv686xx device id match: 0x%x 0x%x", devId_hex, devId_reg);
	// 	return true;
	// } else {
	// LOG_WRN("Warnning: rrv686xx device id not match: 0x%x 0x%x", devId_hex, devId_reg);
	/* return to true even if device id not match. Because device id register can be overwritten
	 */
	return true;
	// }
}

/**
 * @brief Update the configuration region of the RRV686XX device.
 *
 * @param address   I2C slave address of the device.
 * @param spiOffset Offset in SPI flash where the configuration data is stored.
 * @return          True on success, false otherwise.
 */
/* Format: 0x00(header type), 0x07(number of bytes not include 0x00 and 0x07),
	   0xC0(I2C address 8-bit), 0xC6(Command code), 0xE1000000(Data to write),
	   0xFC(CRC8)*/
bool dev_rrv686xx_updateConfig(uint8_t address, uint32_t spiOffset)
{
	uint8_t inputBuffer[16], spiBuffer[16], index = 0;
	uint32_t acks;
	uint32_t rowSize = 0;
	bool needBreak = false;

	LOG_WRN("VR update start.....");

	/* loops until all config updated*/
	while (1) {
		/* read the 16 bytes via spi */
		if (amd_crb_drivers_sFlashRead(0, spiOffset + rowSize, 16, spiBuffer)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X", spiOffset);
			return false;
		}
		if (spiBuffer[0] == 0x49) {
			/* Do nothing for header type */
			rowSize += spiBuffer[1] + 2;
		} else if (spiBuffer[0] == 0x00) {
			rowSize += spiBuffer[1] + 2;
			/* write config to VR chip */
			for (index = 0; index < (spiBuffer[1] - 3); index++) {
				inputBuffer[index] = spiBuffer[4 + index];
			}
			/* write registers */
			acks = dev_rrv686xx_regAccess(0, address, spiBuffer[3], inputBuffer,
						      (spiBuffer[1] - 3));
			if (acks != 0) {
				return false;
			}
			// /* Check command 0xE7 and stop next line */
			if (spiBuffer[3] == DEV_RRV686XX_DMA_APPLY_CHANGE) {
				break;
				LOG_DBG("Break for 0xE7");
				needBreak = true;
			}
		} else {
			/* Unsupported type */
			LOG_DBG("overflow the config 0x%x rowSize%d", spiBuffer[0], rowSize);
			return false;
		}
	}

	return true;
}

/**
 * @brief Poll the programming status of the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return        True if programming is successful, false otherwise.
 */
/* 0xWWXXYYZZ:
0xZZ-> pass should be 0x01
->bit0: If this bit is 0, programming has failed.
->bit1: If bit 1 is 1, programming has failed.
->bit2: If bit 2 is 1, the HEX file contains more configurations than are available
->bit3: If bit 3 is 1, a CRC mismatch exists within the configuration data. Programming fails before
OTP banks are consumed.
->bit4: If bit 4 is 1, the CRC check fails on the OTP memory. Programming fails after OTP banks are
consumed.
->bit5: If bit 5 is 1, programming was attempted on a secured part without using a password and has
failed. Programming fails before OTP banks are consumed.
*/
bool dev_rrv686xx_pollProgStatus(uint8_t address)
{
	uint32_t acks, status;
	acks = dev_rrv686xx_dma_read(address, DEV_RRV686XX_CMD_PROG_STATUS, &status);
	if (acks != 0)
		return false;
	LOG_DBG("rrv686xx program status: 0x%x", status);

	if (status & BIT(0))
		return true;
	else {
		return false;
	}
}

/**
 * @brief Restore a specific configuration in the RRV686XX device.
 *
 * @param address   I2C slave address of the device.
 * @param Config_ID Configuration ID to restore.
 * @return          True on success, false otherwise.
 */
bool dev_rrv686xx_restoreCfg(uint8_t address, uint8_t Config_ID)
{
	uint8_t inputBuffer[8];
	uint32_t acks;

	inputBuffer[0] = Config_ID;
	acks = dev_rrv686xx_regAccess(0, address, DEV_RRV686XX_DMA_RESTORE_ID, inputBuffer, 1);
	if (acks != 0)
		return false;

	LOG_DBG("rrv686xx restore CFG: 0x%x", Config_ID);
	return true;
}

/**
 * @brief Retrieve the CRC value for a specific configuration in the RRV686XX device.
 *
 * @param address   I2C slave address of the device.
 * @param Config_ID Configuration ID to retrieve the CRC for.
 * @return          CRC value on success, 0 on failure.
 */
uint32_t dev_rrv686xx_retrieveCrc(uint8_t address, uint8_t Config_ID)
{
	uint32_t acks, crc = 0;
	acks = dev_rrv686xx_dma_read(address, DEV_RRV686XX_CMD_GET_CRC, &crc);
	if (acks != 0)
		return 0;
	LOG_DBG("rrv686xx retrieve CRC: 0x%x", crc);
	return crc;
}

/**
 * @brief Retrieve the CRC value from the SPI flash for a specific configuration.
 *
 * @param spiOffset Offset in SPI flash where the configuration data is stored.
 * @param Config_ID Configuration ID to retrieve the CRC for.
 * @return          CRC value on success, 0 on failure.
 */
uint32_t dev_rrv686xx_romCrc(uint32_t spiOffset, uint8_t Config_ID)
{
	uint8_t spiBuffer[16];
	uint32_t rowSize = 0;
	uint32_t lineCnt = 0, crc = 0;

	/* loops until all config updated*/
	while (1) {
		/* read the 16 bytes via spi */
		if (amd_crb_drivers_sFlashRead(0, spiOffset + rowSize, 16, spiBuffer)) {
			LOG_ERR("!!error!! fail to read SPI at 0x%06X", spiOffset);
			return 0;
		}
		if (spiBuffer[0] == 0x49) {
			/* Do nothing for header type */
			rowSize += spiBuffer[1] + 2;
			lineCnt++;
		} else if (spiBuffer[0] == 0x00) {
			rowSize += spiBuffer[1] + 2;
			lineCnt++;
		} else {
			/* Unsupported type */
			LOG_DBG("overflow to search crc");
			return 0;
		}

		if (lineCnt == 6) {
			crc = (spiBuffer[7] << 24) + (spiBuffer[6] << 16) + (spiBuffer[5] << 8) +
			      spiBuffer[4];
			/* INCREMENTAL ROM CRC in line7*/
			if (amd_crb_drivers_sFlashRead(0, spiOffset + rowSize, 16, spiBuffer)) {
				LOG_ERR("!!error!! fail to read SPI at 0x%06X", spiOffset);
				return 0;
			}
			if (spiBuffer[0] == 0x49) {
				crc = (spiBuffer[7] << 24) + (spiBuffer[6] << 16) +
				      (spiBuffer[5] << 8) + spiBuffer[4];
			}
			LOG_DBG("rrv686xx ROM CRC: 0x%x", crc);
			break;
		}
	}

	return crc;
}

/**
 * @brief Configure SPS fault settings in the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param value   SPS fault configuration value.
 * @return        0 on success, non-zero on failure.
 */
uint32_t dev_rrv686xx_config_sps_fault(uint8_t address, uint32_t value)
{
	uint32_t ret = dev_rrv686xx_dma_write(address, DEV_RRV686XX_CMD_SPS_FAULT, value);
	return ret;
}

/**
 * @brief Apply setting changes to the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return        True on success, false otherwise.
 */
bool dev_rrv686xx_apply_setting_changes(uint8_t address)
{
	uint8_t inputBuffer[8];
	uint32_t acks;

	inputBuffer[0] = 0x09;
	inputBuffer[1] = 0x00;
	acks = dev_rrv686xx_regAccess(0, address, DEV_RRV686XX_DMA_APPLY_CHANGE, inputBuffer, 2);
	if (acks != 0) {
		LOG_ERR("!!error!! fail to apply setting changes");
		return false;
	}

	return acks == 0;
}

/**
 * @brief Retrieve the available NVM space in the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return        Available NVM space in words.
 */
uint32_t dev_rrv686xx_nvm_space(uint8_t address)
{
	uint32_t space = 0;
	dev_rrv686xx_dma_read(address, DEV_RRV686XX_CMD_NVM_SPACE, &space);
	LOG_ERR("NVM space %d words", space);
	return space;
}

/**
 * @brief Exit low-power mode for the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return        0 on success, non-zero on failure.
 */
uint32_t dev_rrv686xx_exit_low_power_mode(uint8_t address)
{
	uint32_t ret = dev_rrv686xx_dma_write(address, 0xED97, 0x0000008D);
	ret = dev_rrv686xx_dma_write(address, 0xEDBC, 0x00000009);
	ret = dev_rrv686xx_dma_write(address, 0xEDBD, 0x00000009);
	ret = dev_rrv686xx_dma_write(address, 0xEDBE, 0x00000009);
	ret = dev_rrv686xx_dma_write(address, 0xEDBF, 0x00000009);
	return ret;
}

/**
 * @brief Enter low-power mode for the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return        0 on success, non-zero on failure.
 */
uint32_t dev_rrv686xx_enter_low_power_mode(uint8_t address)
{
	uint32_t ret = dev_rrv686xx_dma_write(address, 0xED97, 0x0000000D);
	return ret;
}

/**
 * @brief Retrieve the manufacturer revision information from the RRV686XX device.
 *
 * This function reads the manufacturer revision data from the device's specific register
 * and logs the retrieved values for debugging purposes. The last byte of the read data
 * is returned as the manufacturer revision identifier.
 *
 * @param address I2C slave address of the device.
 * @return        Manufacturer revision identifier (last byte of the read data).
 */
uint8_t dev_rrv686xx_mfr_revision(uint8_t address)
{
    uint8_t outputBuffer[8] = {0};
    dev_rrv686xx_read_bytes(address, DEV_RRV686XX_MFR_REVISION, outputBuffer, 5);
    LOG_DBG("MFR Revision: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", outputBuffer[0], outputBuffer[1], outputBuffer[2], outputBuffer[3], outputBuffer[4]);
    // PLAT-185310 Workaround for Renesas VR Version Register Incorrect
    uint8_t mfr_revision = outputBuffer[1] + outputBuffer[4];
    return mfr_revision;
    //    return outputBuffer[1];
}

/**
 * @brief Retrieve the manufacturer model information from the RRV686XX device.
 *
 * This function reads the manufacturer model data from the device's specific register
 * and logs the retrieved values for debugging purposes. The last byte of the read data
 * is returned as the manufacturer model identifier.
 *
 * @param address I2C slave address of the device.
 * @return        Manufacturer model identifier (last byte of the read data).
 */
uint8_t dev_rrv686xx_mfr_model(uint8_t address)
{
    uint8_t outputBuffer[8] = {0};
    dev_rrv686xx_read_bytes(address, DEV_RRV686XX_MFR_MODEL, outputBuffer, 5);
    LOG_DBG("MFR Model: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", outputBuffer[0], outputBuffer[1], outputBuffer[2], outputBuffer[3], outputBuffer[4]);
    return outputBuffer[4];
}

/**
 * @brief Set the SVI3 reset signal high for the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return        True on success, false on failure.
 */
uint8_t dev_rrv686xx_set_svi3_reset_high(uint8_t address)
{
	uint32_t acks;
	acks = dev_rrv686xx_dma_write(address, 0xEABD, 0x00000080);
	if (acks != 0) {
		LOG_ERR("!!error!! fail to set svi3 reset high");
		return false;
	}
	return true;
}

/**
 * @brief Set the SVI3 reset signal low for the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return        True on success, false on failure.
 */
uint8_t dev_rrv686xx_set_svi3_reset_low(uint8_t address)
{
	uint32_t acks;
	acks = dev_rrv686xx_dma_write(address, 0xEABD, 0x00000100);
	if (acks != 0) {
		LOG_ERR("!!error!! fail to set svi3 reset low");
		return false;
	}
	return true;
}