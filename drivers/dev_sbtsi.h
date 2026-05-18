/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_SBTSI_H__
#define __DEV_SBTSI_H__

#include <stdint.h>
#include <stdbool.h>

typedef uint16_t TSInumber;

/**
 * @brief Init sbi module.
 */
void dev_sbi_init(void);

/**
 * @brief Access sbtsi register.
 *
 * @param isRead True if read mode, false represent write mode.
 * @param reg SBTSI reg to access.
 * @param pData Data pointer.
 *
 * @retval True if success.
 */
uint32_t dev_sbtsi_regAccess(bool isRead, uint8_t reg, uint8_t * pData);

/**
 * @brief Sends a write request packet for slave attached flash.
 *
 * @param isRead True if read mode, false represent write mode.
 * @param slvAddr SBRMI slave address.
 * @param reg SBRMI reg to access.
 * @param pData Data pointer.
 *
 * @retval True if success.
 */
uint32_t dev_sbrmi_regAccess(bool isRead, uint8_t slvAddr, uint8_t reg, uint8_t * pData);

/**
 * @brief Get temperature from sbtsi.
 *
 * @retval Temperature, 60 by default.
 */
TSInumber dev_sbtsi_getTemp16(void);

/**
 * @brief Get temperature integer part from sbtsi.
 *
 * @retval Temperature, 60 by default.
 */
uint8_t dev_sbtsi_getTempInt(void);

/**
 * @brief mail box service
 *
 * @param slv SBI slave address.
 * @param cmd Service command.
 * @param pMsgIn Message in pointer.
 * @param pMsgOut Message out pointer.
 * @param us_timeout timeout.
 *
 * @retval True if success.
 */
uint8_t dev_sbi_mb_service(uint8_t slv, uint8_t cmd, uint32_t * pMsgIn, uint32_t * pMsgOut, uint32_t us_timeout);

/**
 * @brief Write STT register.
 *
 * @param slv SBI slave address.
 * @param sensorId sensor id.
 * @param u16data STT value.
 *
 * @retval True if success.
 */
bool dev_sbi_writeSttSensor(uint8_t slv, uint8_t sensorId, uint16_t u16data);

/**
 * @brief Read pakage power consumption.
 *
 * @param slv SBI slave address.
 *
 * @retval Pakage power consumption.
 */
uint32_t dev_sbi_readPkgPwrConsumption(uint8_t slv);

/**
 * @brief Write P3tLimit to SBI.
 *
 * @param slv SBI slave address.
 * @param u32mW P3tLimit.
 *
 * @retval Message out.
 */
uint32_t dev_sbi_writeP3tLimit(uint8_t slv, uint32_t u32mW);

#endif // end of __DEV_SBTSI_H__
