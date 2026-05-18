/*****************************************************************************
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_RRV686XX_H__
#define __DEV_RRV686XX_H__

#include <stdint.h>
#include <stdbool.h>

/* Used to set the register address to use with other DMA commands */
#define DEV_RRV686XX_DMA_ADDRESS          (0xC7) 
/* Used to read from or write to the register selected by the DMA Address command */
#define DEV_RRV686XX_DMA_DATA             (0xC5)
/* Used to read from or write to the register selected by the DMA Address command, then automatically increment the register address by 1. */
#define DEV_RRV686XX_DMA_SEQUENTIAL       (0xC6)
/* Used to apply packet capture changes */
#define DEV_RRV686XX_DMA_APPLY_CHANGE     (0xE7)
/* Used to read the device ID */
#define DEV_RRV686XX_DMA_DEV_ID           (0xAD)
/* Used to read the device revision */
#define DEV_RRV686XX_DMA_DEV_REV          (0xAE)
/* Used to restore configuration */
#define DEV_RRV686XX_DMA_RESTORE_ID       (0xF2)
/* Used to read revision */
#define DEV_RRV686XX_MFR_REVISION       (0x9B)
/* Used to read model */
#define DEV_RRV686XX_MFR_MODEL       (0x9A)

/* Command */
#define DEV_RRV686XX_CMD_DEVICE_DATA      (0xEED0)
#define DEV_RRV686XX_CMD_PROG_STATUS      (0x003A)
#define DEV_RRV686XX_CMD_GET_CRC          (0x00C6)
#define DEV_RRV686XX_CMD_SPS_FAULT        (0x0131)
#define DEV_RRV686XX_CMD_NVM_SPACE        (0x0038)

/* Functions */

/**
 * @brief Check if the RRV686XX device is ready to respond to I2C commands.
 *
 * @param address I2C slave address of the device.
 * @return True if the device is ready, false otherwise.
 */
bool dev_rrv686xx_ready(uint8_t address);

/**
 * @brief Retrieve device data from the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return Device data on success, 0 on failure.
 */
uint32_t dev_rrv686xx_retrieveDeviceData(uint8_t address);

/**
 * @brief Read the device ID from the binary image stored in SPI flash.
 *
 * @param spiOffset Offset in SPI flash where the binary image is stored.
 * @return Device ID on success, 0 on failure.
 */
uint32_t dev_rrv686xx_devId(uint32_t spiOffset);

/**
 * @brief Retrieve the I2C address of the RRV686XX device from the binary image.
 *
 * @param spiOffset Offset in SPI flash where the binary image is stored.
 * @return I2C address on success, 0 on failure.
 */
uint8_t dev_rrv686xx_i2cAddress(uint32_t spiOffset);

/**
 * @brief Check if the device ID matches the expected value.
 *
 * @param address I2C slave address of the device.
 * @param spiOffset Offset in SPI flash where the binary image is stored.
 * @return True if the device ID matches, false otherwise.
 */
bool dev_rrv686xx_checkDevId(uint8_t address, uint32_t spiOffset);

/**
 * @brief Check if the device revision ID matches the expected value.
 *
 * @param address I2C slave address of the device.
 * @param spiOffset Offset in SPI flash where the binary image is stored.
 * @return True if the revision ID matches, false otherwise.
 */
bool dev_rrv686xx_checkRevId(uint8_t address, uint32_t spiOffset);

/**
 * @brief Update the configuration region of the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param spiOffset Offset in SPI flash where the configuration data is stored.
 * @return True on success, false otherwise.
 */
bool dev_rrv686xx_updateConfig(uint8_t address, uint32_t spiOffset);

/**
 * @brief Poll the programming status of the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return True if programming is successful, false otherwise.
 */
bool dev_rrv686xx_pollProgStatus(uint8_t address);

/**
 * @brief Restore a specific configuration in the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param Config_ID Configuration ID to restore.
 * @return True on success, false otherwise.
 */
bool dev_rrv686xx_restoreCfg(uint8_t address, uint8_t Config_ID);

/**
 * @brief Retrieve the CRC value for a specific configuration in the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param Config_ID Configuration ID to retrieve the CRC for.
 * @return CRC value on success, 0 on failure.
 */
uint32_t dev_rrv686xx_retrieveCrc(uint8_t address, uint8_t Config_ID);

/**
 * @brief Retrieve the CRC value from the SPI flash for a specific configuration.
 *
 * @param spiOffset Offset in SPI flash where the configuration data is stored.
 * @param Config_ID Configuration ID to retrieve the CRC for.
 * @return CRC value on success, 0 on failure.
 */
uint32_t dev_rrv686xx_romCrc(uint32_t spiOffset, uint8_t Config_ID);

/**
 * @brief Configure SPS fault settings in the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param value SPS fault configuration value.
 * @return 0 on success, non-zero on failure.
 */
uint32_t dev_rrv686xx_config_sps_fault(uint8_t address, uint32_t value);

/**
 * @brief Write modified data to the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @param dev_data Data to write.
 * @return True on success, false otherwise.
 */
bool dev_rrv686xx_writemodifieddata(uint8_t address, uint32_t dev_data);

/**
 * @brief Apply setting changes to the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return True on success, false otherwise.
 */
bool dev_rrv686xx_apply_setting_changes(uint8_t address);

/**
 * @brief Retrieve the available NVM space in the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return Available NVM space in words.
 */
uint32_t dev_rrv686xx_nvm_space(uint8_t address);

/**
 * @brief Exit low-power mode for the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return 0 on success, non-zero on failure.
 */
uint32_t dev_rrv686xx_exit_low_power_mode(uint8_t address);

/**
 * @brief Enter low-power mode for the RRV686XX device.
 *
 * @param address I2C slave address of the device.
 * @return 0 on success, non-zero on failure.
 */
uint32_t dev_rrv686xx_enter_low_power_mode(uint8_t address);


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
uint8_t dev_rrv686xx_mfr_revision(uint8_t address);

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
uint8_t dev_rrv686xx_mfr_model(uint8_t address);

uint8_t dev_rrv686xx_set_svi3_reset_high(uint8_t address);
uint8_t dev_rrv686xx_set_svi3_reset_low(uint8_t address);
#endif // end of __DEV_RRV686XX_H__