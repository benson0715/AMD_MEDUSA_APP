/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_TMP432_H__
#define __DEV_TMP432_H__

#include <stdint.h>
#include <stdbool.h>

#define DEV_TMP432_REG_LOCAL_TEMP_HIGH     0x00
#define DEV_TMP432_REG_LOCAL_TEMP_LOW      0x29
#define DEV_TMP432_REG_REMOTE_TEMP1_HIGH   0x01
#define DEV_TMP432_REG_REMOTE_TEMP1_LOW    0x10
#define DEV_TMP432_REG_REMOTE_TEMP2_HIGH   0x23
#define DEV_TMP432_REG_REMOTE_TEMP2_LOW    0x24

#define DEV_TEM432_REG_STATUS              0x02

#define DEV_TMP432_MAGIC_NUM               0xC8

typedef enum {
	DEV_TMP432_SENSOR__LOCAL   = 0x0029,
	DEV_TMP432_SENSOR__REMOTE1 = 0x0110,
	DEV_TMP432_SENSOR__REMOTE2 = 0x2324
} DEV_TMP432_SENSOR_SELECTOR;

/**
 * @brief Access tmp432 register.
 *
 * @param isRead Indicate write or read.
 * @param reg from where the data is transferred.
 * @param pData Memory pool from which the data is transferred.
 * @param dataLen dataLen.
 *
 * @retval 0 If successful.
 * @retval -EIO General input / output error.
 */
uint32_t dev_tmp432_regAccess(bool isRead, uint8_t reg, void * pData, uint8_t dataLen);

/*
 * The TMP43x contain circuitry to assure that a low byte register read command returns data from the same ADC
 * conversion as the immediately preceding high byte read command. This assurance remains valid only until
 * another register is read. 
 *
 * For proper operation, the high byte of a temperature register must be read first. The low byte register must
 * be read in the next read command. The low byte register can be left unread if the LSBs are not needed.
 *  
 * Alternatively, the temperature registers can be read as a 16-bit register by using a single two-byte read 
 * command from address 00h for the local channel result, or from address 01h for the remote channel result
 * (23h for the second remote channel result). The high byte is output first, followed by the low byte. Both 
 * bytes of this read operation are from the same ADC conversion.
 */

/**
 * @brief Get temperature integer part from tmp432.
 *
 * @param s Sensor selected.
 *
 * @retval Temperature. Integer part.
 */
uint8_t dev_tmp432_getTempInt(DEV_TMP432_SENSOR_SELECTOR s);

/**
 * @brief Get temperature decimal part from tmp432.
 *
 * @param s Sensor selected.
 *
 * @retval Temperature. Decimal part.
 */
uint8_t dev_tmp432_getTempDec(DEV_TMP432_SENSOR_SELECTOR s);

/**
 * @brief Get temperature from tmp432. 
 *
 * @param s Sensor selected.
 *
 * @retval Temperature.
 */
uint16_t dev_tmp432_readTemp(DEV_TMP432_SENSOR_SELECTOR s);

/**
 * @brief Enable tmp432 extended range. 
 * 
 * Temperature range; 0 = 0~127C, 1 = -55~150C
 *
 * @retval True if success.
 */
bool dev_tmp432_enableExtendedRange(void);

/**
 * @brief Disable tmp432 extended range. 
 * 
 * Temperature range; 0 = 0~127C, 1 = -55~150C
 *
 * @retval True if success.
 */
bool dev_tmp432_disableExtendedRange(void);

/**
 * @brief Set resistance correction. 
 * 
 * @param s Resistance correction enabled.
 *
 * @retval True if success.
 */
bool dev_tmp432_setResistanceCorrection(bool toEnable);

/* *******************************************
 * !!! NOTE !!! TtempFlt is float.
 *
 * Below function gets FPU be involved !!!
 *
 * Please be prudent of the execution context
 * so as to avoid stack corruption.
 *
 * *******************************************/

/**
 * @brief Convert temperature to float format. 
 * 
 * @param u16Tmp432 Raw temperature data.
 *
 * @retval Float temperature.
 */
float dev_tmp432_convert2Flt(uint16_t u16Tmp432);

#endif // end of __DEV_TMP432_H__

