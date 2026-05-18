/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_MP2815_SVI3_H__
#define __DEV_MP2815_SVI3_H__

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"


#define DEV_SVI3_CMD_PAGE               (0x00)  // Fixed assignment
#define DEV_SVI3_CMD_OPERATION          (0x01)  // Fixed assignment

#define DEV_SVI3_CMD_FAULT_STATUS       (0x10)
#define DEV_SVI3_CMD_STORE_USER_ALL     (0x15)  // Store the configuration into the NVM with I2C command (15h).
#define DEV_SVI3_PRODUCT_DATA_CODE      (0x9B) // Store the version number for different power SKU
#define DEV_SVI3_CMD_CODE_REV           (0xFD) // It is proposed  to use "MFR_REVISION" register 0xFD to track the VR config file

typedef enum {
	DEV_SVI3_PAGE0 = 0,
	DEV_SVI3_PAGE1,
	DEV_SVI3_PAGE2,
	DEV_SVI3_PAGE3,
	DEV_SVI3_PAGE4,
	DEV_SVI3_PAGE_MAX = 5
} DEV_SVI3_PAGE;

//
// VR config
//
typedef struct {
	uint8_t reg;
	uint16_t rail[4];
} DEV_SVI3_REG_ITEM;

#pragma pack(push,1)
typedef struct {
	uint8_t  reg;
	uint16_t table;
	uint8_t page;
	uint16_t val;
} DEV_SVI3_CPMPRESSED;     // 5-byte of each row
#pragma pack(pop)

#define DEV_SVI3_REG_SIZE(reg)   ((((reg) < 0x1B)||(reg == 0x47)) ? 1 : 2)

/**
 * @brief Read data from mp2815.
 *
 * @param offset data offset.
 * @param data data pointer.
 * @param len length of the data.
 */
void dev_svi3_read_bytes(uint8_t offset, uint8_t *data, uint8_t len);

/**
 * @brief Write data to mp2815.
 *
 * @param offset data offset.
 * @param data data pointer.
 * @param len length of the data.
 */
void dev_svi3_write_bytes(uint8_t offset, uint8_t *data, uint8_t len);

/**
 * @brief Get mp2815 ready status.
 *
 * @return ready status.
 */
bool dev_svi3_ready(void);

/**
 * @brief Set mp2815 slave address.
 *
 * @param slvAddr slave address.
 */
void dev_svi3_set_slave_address(uint8_t slvAddr);

/**
 * @brief Read register of mp2815.
 *
 * @param reg reg to read.
 * 
 * @return read value.
 */
uint16_t dev_svi3_readRegister(uint8_t reg);

/**
 * @brief Write register of mp2815.
 *
 * @param reg reg to write.
 * @param val value to write.
 * 
 * @return true if success.
 */
bool dev_svi3_writeRegister(uint8_t reg, uint16_t val);

/**
 * @brief Write register of mp2815.
 *
 * @param pCfg mp2815 reg item pointer.
 */
void dev_svi3_readOneRow(DEV_SVI3_REG_ITEM * pCfg);

/**
 * @brief Get vr table version.
 *
 * @return vr table version.
 */
uint16_t dev_svi3_getCodeRev(void);

/**
 * @brief VR config fail behavior.
 */
void dev_svi3_onFail(void);

/**
 * @brief Switch mp2815 page.
 *
 * @param pageId index of mp2815 page.
 * 
 * @return current page.
 */
uint8_t dev_svi3_switchPage(uint8_t pageId);                             // Page switch is guaranteed

/**
 * @brief Apply one row to mp2815.
 *
 * @param pCfg mp2815 reg item pointer.
 */
void dev_svi3_applyOneRow(DEV_SVI3_REG_ITEM * pCfg);                     // Setting is guaranteed

/**
 * @brief Apply config table to mp2815.
 *
 * @param pCfg mp2815 reg item pointer.
 */
void dev_svi3_applyConfigTable(DEV_SVI3_REG_ITEM * pCfg);                // Setting is guaranteed; pCfg is a FF terminated table

/**
 * @brief Verify mp2815 config table.
 *
 * @param pCfg mp2815 reg item pointer.
 * 
 * @return true if pass, false is fail.
 */
bool dev_svi3_verifyConfigTable(DEV_SVI3_REG_ITEM * pCfg);               // Registers who has value 0xFFFF are skipped; pCfg is a FF terminated table

/**
 * @brief Apply compressed table to mp2815.
 *
 * @param pCfg mp2815 reg item pointer.
 */
void dev_svi3_applyCompressedTable(DEV_SVI3_CPMPRESSED * pVrCfg, uint32_t tableId);

/**
 * @brief Verify mp2815 compressed table.
 *
 * @param pCfg mp2815 reg item pointer.
 * 
 * @return true if pass, false is fail.
 */
bool dev_svi3_verifyCompressedTable(DEV_SVI3_CPMPRESSED * pVrCfg, uint32_t tableId);

/**
 * @brief Flush svi3 config table.
 *
 */
void dev_svi3_flushConfig(void);                                         // SVI3 ready is guaranteed

/**
 * @brief Configure the SVI3 checksum value place.
 *
 * @param page page id.
 * @param reg register value.
 * 
 * @return true if pass, false is fail.
 */

void dev_svi3_checksumConfig(uint8_t page, uint8_t reg);

/**
 * @brief Get svi3 checksum.
 *
 * @return checksum.
 */
uint16_t dev_svi3_checksumGet(void);

/**
 * @brief Flush svi3 config table.
 *
 * @return true if have fault.
 */
bool dev_svi3_crcFaultDetect(void);

/**
 * @brief Dump register of mp2815.
 *
 * @param reg reg to dump.
 * 
 * @return dump value.
 */
void dev_svi3_dumpOneRegister(uint8_t reg); // Debug only

/**
 * @brief Get power sku.
 *
 * @return sku.
 */
uint16_t dev_svi3_getSku(void);
#endif // end of __DEV_MP2815_SVI3_H__

