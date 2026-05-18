/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include "dev_mp2815_SVI3.h"
#include "board_config.h"
#include "i2c_hub.h"
#include <dev_utility.h>
#include "debug_info.h"
LOG_MODULE_REGISTER(mp2815, CONFIG_SVI3_LOG_LEVEL);

static uint8_t svi3_slv_address = MP2815_I2C_ADDRESS_ID0 ;
static uint8_t svi3_slv_checksum_page = 0x0u; /* default */
static uint8_t svi3_slv_checksum_reg = 0xAEu; /* default */

/**
 * @brief Set the SVI3 slave address
 * @param slvAddr The slave address to set
 */
void dev_svi3_set_slave_address(uint8_t slvAddr)
{
	svi3_slv_address = slvAddr;
}

/**
 * @brief Internal function for SVI3 register access via I2C
 * @param isRead True for read operation, false for write
 * @param port I2C port number
 * @param slv Slave address
 * @param reg Register address
 * @param pByte Pointer to data buffer
 * @param len Length of data
 * @return 0 on success, non-zero on failure
 */
static uint32_t _svi3_regAccess(bool isRead, uint8_t port, uint8_t slv, uint8_t reg, uint8_t * pByte, uint8_t len)
{
	uint8_t retry = 3;
	uint32_t ret = 0;
	
	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			ret = i2c_hub_burst_read(port, slv, reg, pByte, len);
		} else {
			ret = i2c_hub_burst_write(port, slv, reg, pByte, len);
		}
#if (CONFIG_LOG)
		LOG_CLEARBUF;
		for ( uint32_t i = 0; i < len; i++ ) {
			LOG_APPEND(" %02X", *((uint8_t *)pByte + i));
		}
		LOG_DBG("SVI3@ %02X %s [%02X], ret %d, data(%d)%s", slv, isRead ? "R" : "W", reg, ret, 1, LOG_BUF);
#endif
		if (ret == 0)
			return ret;
	}
	if (!retry) {
		info_value_increase(INFO_I2C_SVI3, 1);
		LOG_ERR("[!!Fatal error!!] on SVI3 @ %02X %s [%02X], ret %d", slv, isRead ? "R" : "W", reg, ret);
	}

	return ret;
}

/**
 * @brief Read bytes from SVI3 device
 * @param offset Register offset to read from
 * @param data Pointer to buffer for read data
 * @param len Number of bytes to read
 */
void dev_svi3_read_bytes(uint8_t offset, uint8_t *data, uint8_t len)
{
	_svi3_regAccess(1, MP2815_I2C_PORT, svi3_slv_address, offset, data, len);
}

/**
 * @brief Write bytes to SVI3 device
 * @param offset Register offset to write to
 * @param data Pointer to data to write
 * @param len Number of bytes to write
 */
void dev_svi3_write_bytes(uint8_t offset, uint8_t *data, uint8_t len)
{
	_svi3_regAccess(0, MP2815_I2C_PORT, svi3_slv_address, offset, data, len);
}

/**
 * @brief Handle VR configuration failure with LED indication
 *
 * FW need to ensure the setup of VR is applied properly
 * Do this deadloop in case any fail
 */
void dev_svi3_onFail(void)
{
	uint32_t duty = 0;
	LOG_ERR("[ERROR] VR config failed!");

	while(1) {
		duty = (duty + 1) % 150;

		for (uint32_t p = 0; p < 100; p++) {
			if (p < duty) {
				EC_DEBUG_LED(1);
				k_usleep(10);
			} else {
			    EC_DEBUG_LED(0);
				k_usleep(10);
			}
		}
	}
}

/**
 * @brief Check if SVI3 device is ready
 * @return true if device is ready, false otherwise
 */
bool dev_svi3_ready(void)
{
	uint32_t ret;
	uint8_t data = DEV_SVI3_PAGE0;

	ret = _svi3_regAccess(0, MP2815_I2C_PORT, svi3_slv_address, DEV_SVI3_CMD_PAGE, &data, 1);
	if (ret != 0)
		return false;
	return true;
}

/**
 * @brief Switch to specified page
 * @param pageId Page ID to switch to
 * @return Current page after switch
 */
uint8_t dev_svi3_switchPage(uint8_t pageId)
{
	uint8_t curPage = 0xFF;
	dev_svi3_read_bytes(DEV_SVI3_CMD_PAGE, &curPage, 1);

	if (curPage != pageId) {
		dev_svi3_write_bytes(DEV_SVI3_CMD_PAGE, &pageId, 1);
		dev_svi3_read_bytes (DEV_SVI3_CMD_PAGE, &curPage, 1);

		if (pageId != curPage) {
			LOG_ERR("SVI3: fail to switch page.");
			dev_svi3_onFail();
		}
	}

	return curPage;
}

/**
 * @brief Read a register value
 * @param reg Register address to read
 * @return Register value (8 or 16 bit depending on register)
 */
uint16_t dev_svi3_readRegister(uint8_t reg)
{
	uint16_t u16data = 0;
	dev_svi3_read_bytes(reg, (uint8_t *)&u16data, DEV_SVI3_REG_SIZE(reg));

	if (1 == DEV_SVI3_REG_SIZE(reg)) {
		u16data &= 0x00FF;
	}

	return u16data;
}

/**
 * @brief Write a register value
 * @param reg Register address to write
 * @param val Value to write
 * @return true on success, false on failure
 */
bool dev_svi3_writeRegister(uint8_t reg, uint16_t val)
{
	dev_svi3_write_bytes(reg, (uint8_t *)&val, DEV_SVI3_REG_SIZE(reg));

	if (reg != 0x7C) {  // Set [15:11] of Reg 0x7C to 5'b01111 will bring VR to low power mode
		uint16_t read = dev_svi3_readRegister(reg);
		if (read != val) {
			LOG_ERR("[ERROR] Update reg %02X to %04X failed, read = %04X", reg, val, read);
		}
		return (read == val) ? true : false;
	}

	return true;
}

/**
 * @brief Write a register value only if different from current value
 * @param reg Register address to write
 * @param val Value to write
 * @return true if value was changed, false if unchanged
 */
bool dev_svi3_writeRegOnDiff(uint8_t reg, uint16_t val)
{
	uint16_t orig = dev_svi3_readRegister(reg);

	if (orig != val) {
		dev_svi3_write_bytes(reg, (uint8_t *)&val, DEV_SVI3_REG_SIZE(reg));

		if (reg != 0x7C) {  // Set [15:11] of Reg 0x7C to 5'b01111 will bring VR to low power mode
			uint16_t read = dev_svi3_readRegister(reg);
			if (read != val) {
				LOG_ERR("[ERROR] Update reg %02X to %04X failed, read = %04X", reg, val, read);
				LOG_ERR("SVI3: dev_svi3_writeRegOnDiff() error.");
				dev_svi3_onFail();
			}
			return true;
		}
	}

	return false;
}

/**
 * @brief Get the SKU from product data code register
 *
 * NOTE: This routine can change the active page
 * reg 0x9B page0 for code rev in V2
 *
 * @return SKU value
 */
uint16_t dev_svi3_getSku(void)
{
	dev_svi3_switchPage(DEV_SVI3_PAGE0);
	uint16_t sku = dev_svi3_readRegister(DEV_SVI3_PRODUCT_DATA_CODE);
	return sku;
}

/**
 * @brief Get the code revision
 * @return Code revision value
 */
uint16_t dev_svi3_getCodeRev(void)
{
	dev_svi3_switchPage(DEV_SVI3_PAGE0);
	uint16_t cordRev = dev_svi3_readRegister(DEV_SVI3_CMD_CODE_REV);
	return cordRev;
}

/**
 * @brief Apply one row of configuration
 *
 * NOTE: This routine can change the active page
 * Signal end-user by LED on config fail
 *
 * @param pCfg Pointer to configuration item
 */
void dev_svi3_applyOneRow(DEV_SVI3_REG_ITEM * pCfg)
{
	if (pCfg->reg != 0xFF) {
		for (uint32_t page = 0; page < DEV_SVI3_PAGE_MAX; page ++) {
			if (0xFFFF != pCfg->rail[page]) {
				dev_svi3_switchPage(page);

				if (!dev_svi3_writeRegister(pCfg->reg, pCfg->rail[page])) {
					LOG_ERR("SVI3: Write register error.");
					dev_svi3_onFail();
				} else {
					LOG_DBG("Set page %d, reg %02X, val %04X", page, pCfg->reg, pCfg->rail[page]);
				}
			}
		}
	}
}

/**
 * @brief Read one row of configuration
 *
 * NOTE: This routine can change the active page
 *
 * @param pCfg Pointer to configuration item to fill
 */
void dev_svi3_readOneRow(DEV_SVI3_REG_ITEM * pCfg)
{
	if (pCfg && pCfg->reg != 0xFF) {
		for (uint32_t page = 0; page < DEV_SVI3_PAGE_MAX; page ++) {
			dev_svi3_switchPage(page);

			pCfg->rail[page] = dev_svi3_readRegister(pCfg->reg);
		}
	}
}

/**
 * @brief Apply a configuration table
 *
 * NOTE: This routine can change the active page
 * Signal end-user by LED on config fail
 *
 * @param pCfg Pointer to configuration table
 */
void dev_svi3_applyConfigTable(DEV_SVI3_REG_ITEM * pCfg)
{
	uint32_t i = 0;

	for (uint32_t page = 0; page < DEV_SVI3_PAGE_MAX; page ++) {
		dev_svi3_switchPage(page);

		for (i = 0; pCfg[i].reg != 0xFF; i++) {
			/*
			 * 0xFFFF in table means this setting should be skipped
			 * Also skip the PAGE register
			 */
			if (0xFFFF != pCfg[i].rail[page] && DEV_SVI3_CMD_PAGE != pCfg[i].reg) {
				if (!dev_svi3_writeRegister(pCfg[i].reg, pCfg[i].rail[page])) {
					LOG_ERR("SVI3: Write register error.");
					dev_svi3_onFail();
				}
			}
		}
	}
}

/**
 * @brief Verify a configuration table against device registers
 * @param pCfg Pointer to configuration table
 * @return true if all values match, false otherwise
 */
bool dev_svi3_verifyConfigTable(DEV_SVI3_REG_ITEM * pCfg)
{
	uint32_t i = 0;
	uint16_t val;

	for (uint32_t page = 0; page < DEV_SVI3_PAGE_MAX; page ++) {
		dev_svi3_switchPage(page);

		for (i = 0; pCfg[i].reg != 0xFF; i++) {
			/*
			 * 0xFFFF in table means this setting should be skipped
			 * Also skip the PAGE register
			 */
			if (0xFFFF != pCfg[i].rail[page] && DEV_SVI3_CMD_PAGE != pCfg[i].reg) {
				val = dev_svi3_readRegister(pCfg[i].reg);
				if (val != pCfg[i].rail[page])
					return false;
			}
		}
	}

	return true;
}

/**
 * @brief Apply a compressed configuration table
 *
 * NOTE: This routine can change the active page
 * Signal end-user by LED on config fail
 *
 * @param pVrCfg Pointer to compressed configuration table
 * @param tableId Table ID to apply
 */
void dev_svi3_applyCompressedTable(DEV_SVI3_CPMPRESSED * pVrCfg, uint32_t tableId)
{
	LOG_DBG("Apply VR Cfg of table %d ...", tableId);

	for (uint32_t page = 0; page < DEV_SVI3_PAGE_MAX; page ++) {
		dev_svi3_switchPage(page);

		for (uint32_t i = 0; pVrCfg[i].reg != 0xFF; i++) {
			if (pVrCfg[i].table & (1 << tableId) && pVrCfg[i].page & (1 << page)) {
				if (!dev_svi3_writeRegister(pVrCfg[i].reg, pVrCfg[i].val)) {
					LOG_ERR("SVI3: Write register error.");
					dev_svi3_onFail();
				} else {
					LOG_DBG("Set page %d, reg %02X, val %04X", page, pVrCfg[i].reg, pVrCfg[i].val);
				}
			}
		}
	}

	LOG_DBG("VR Cfg update done ...");
}

/**
 * @brief Verify a compressed configuration table against device registers
 * @param pVrCfg Pointer to compressed configuration table
 * @param tableId Table ID to verify
 * @return true if all values match, false otherwise
 */
bool dev_svi3_verifyCompressedTable(DEV_SVI3_CPMPRESSED * pVrCfg, uint32_t tableId)
{
	for (uint32_t page = 0; page < DEV_SVI3_PAGE_MAX; page ++) {
		dev_svi3_switchPage(page);

		for (uint32_t i = 0; pVrCfg[i].reg != 0xFF; i++) {
			if (pVrCfg[i].table & (1 << tableId) && pVrCfg[i].page & (1 << page)) {
				uint16_t val = dev_svi3_readRegister(pVrCfg[i].reg);
				if (val != pVrCfg[i].val) {
					LOG_ERR("[ERROR] Verify VR Cfg settings failed ...");
					LOG_DBG("        table %d page %d, reg 0x%02X => 0x%04X in Cfg != 0x%04X in Reg", tableId, page, pVrCfg[i].reg, val, pVrCfg[i].val);
					return false;
				}
			}
		}
	}

	return true;
}

K_TIMER_DEFINE(flushTimer, NULL, NULL);

/**
 * @brief Flush configuration from RAM to EEPROM
 */
void dev_svi3_flushConfig (void)
{
	/* Store config from RAM to EEPROM */
	dev_svi3_write_bytes(DEV_SVI3_CMD_STORE_USER_ALL, NULL, 0);

	/* Wait for device ready */
	k_timer_start(&flushTimer,K_MSEC(800),K_NO_WAIT);
	do {
		if (dev_svi3_ready())
			break;
	} while (k_timer_status_get(&flushTimer) == 0);

	if (!dev_svi3_ready()) {
		LOG_ERR("SVI3 is not ready.");
		dev_svi3_onFail();
	}
}

/**
 * @brief Configure checksum location
 * @param page Page where checksum is located
 * @param reg Register where checksum is located
 */
void dev_svi3_checksumConfig(uint8_t page, uint8_t reg)
{
	/* Please confirm the checksum location with the power team if it has */
	svi3_slv_checksum_page = page;
	svi3_slv_checksum_reg = reg;
}

/**
 * @brief Get the checksum value
 * @return Checksum value
 */
uint16_t dev_svi3_checksumGet(void)
{
	dev_svi3_switchPage(svi3_slv_checksum_page);
	return dev_svi3_readRegister(svi3_slv_checksum_reg);
}

/**
 * @brief Detect CRC fault
 * @return true if CRC fault detected, false otherwise
 */
bool dev_svi3_crcFaultDetect(void)
{
    dev_svi3_switchPage(0U);

	return (dev_svi3_readRegister(0xBE) & cbit(2))?(true):(false);
}

/**
 * @brief Dump one register value for debugging
 * @param reg Register address to dump
 */
void dev_svi3_dumpOneRegister(uint8_t reg)
{
#if (CONFIG_LOG)
	DEV_SVI3_REG_ITEM item = {reg};
	memset(item.rail, 0xFF, sizeof(item.rail));
	dev_svi3_readOneRow(&item);

	if (1 == DEV_SVI3_REG_SIZE(reg)) {
		LOG_DBG("  SVI3 DUMP { Reg 0x%02X, {   0x%02X,    0x%02X,    0x%02X,    0x%02X} }",
			item.reg,
			item.rail[0], item.rail[1], item.rail[2], item.rail[3]);
	} else {
		LOG_DBG("  SVI3 DUMP { Reg 0x%02X, { 0x%04X,  0x%04X,  0x%04X,  0x%04X} }",
			item.reg,
			item.rail[0], item.rail[1], item.rail[2], item.rail[3]);
	}
#endif
}