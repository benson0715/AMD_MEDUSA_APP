/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_sbtsi.h"
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "i2c_hub.h"
LOG_MODULE_REGISTER(sbtsi, CONFIG_TMP432_LOG_LEVEL);

#ifndef SBTSI_I2C_ADDRESS
#define SBTSI_I2C_ADDRESS (0x98 >> 1)
#endif

#ifndef SBTSI_I2C_PORT
#define SBTSI_I2C_PORT I2C_7
#endif

extern bool espihub_socInLP(void);

/* function to access SMU via I2C */
uint32_t dev_sbtsi_regAccess(bool isRead, uint8_t reg, uint8_t *pData)
{
	uint8_t retry = 3;
	uint32_t ret = 0;

	/* return 0 if enter Zx */
	if(espihub_socInLP())
		return 0;

	while (retry) {
		retry--;
		ret = 0;
		if (isRead){
			ret = i2c_hub_reg_read_byte(SBTSI_I2C_PORT, SBTSI_I2C_ADDRESS, reg, pData);
			if (ret != 0)
				continue;
		}
		else{
			ret = i2c_hub_reg_write_byte(SBTSI_I2C_PORT, SBTSI_I2C_ADDRESS, reg, *pData);
		}
		LOG_DBG("Acc SB-TSI  %s [%02X], ret %d", isRead ? "R" : "W", reg, ret);
		if (ret == 0)
			return ret;
	}
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] on %s SB-TSIx[%02X], ret %d", isRead ? "R" : "W", reg, ret);
	}

	return ret;
}

/*
 * This function is intend to accesss the APML registers responed by SMU FW.
 */
uint32_t dev_sbrmi_regAccess(bool isRead, uint8_t slvAddr, uint8_t reg, uint8_t *pData)
{
	uint8_t retry = 3;
	uint32_t ret = 0;

	if(espihub_socInLP())
			return 0;

	while (retry) {
		retry--;
		ret = 0;
		if (isRead) {
			ret = i2c_hub_reg_read_byte(SBTSI_I2C_PORT, slvAddr, reg, pData);
		}
		else {
			ret = i2c_hub_reg_write_byte(SBTSI_I2C_PORT, slvAddr, reg, *pData);
		}
		LOG_DBG("Acc SBRMI<%02X> %s [%02X], data %02X, ret %d", slvAddr, isRead ? "R" : "W", reg, *pData, ret);
		if (ret == 0)
			return ret;
	}
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] on SBRMI<%02X> %s x[%02X], data %02X, ret %d", slvAddr, isRead ? "R" : "W", reg, *pData, ret);
	}

	return ret;
}

/*
 * Get temperature as 16-bit value from SB-TSI sensor.
 */
TSInumber dev_sbtsi_getTemp16(void)
{
	uint8_t Int, Dec;
	TSInumber ret = 0x3C00; // 60.0 degree centigrade

	// By default reading SBTSI_x01 causes the state of SBTSI_x10 to be latched
	if (0 == dev_sbtsi_regAccess(true, 0x01, &Int))
		if (0 == dev_sbtsi_regAccess(true, 0x10, &Dec))
			ret = ((TSInumber)Int << 8) + Dec;
	return ret;
}

/*
 * Get temperature integer part from SB-TSI sensor.
 */
uint8_t dev_sbtsi_getTempInt(void)
{
	TSInumber temp = dev_sbtsi_getTemp16();
	return (temp >> 8);
}

#define F_SBI_MB_RETURN_CODE_SUCCESS 0x0	  // Mailbox message command executed successfully without any error
#define F_SBI_MB_RETURN_CODE_CMD_ABORTED 0x1  // Mailbox message command is aborted
#define F_SBI_MB_RETURN_CODE_UNKNOWN_CMD 0x2  // Unknown/Unsupported mailbox message
#define F_SBI_MB_RETURN_CODE_INVALID_CODE 0x3 // Invalid core is specified in mailbox message parameters

K_MUTEX_DEFINE(_s_mbSerLock);
K_TIMER_DEFINE(sbiTimer, NULL, NULL);

/*
 * SB-RMI mailbox service function for communication with SMU firmware.
 */
uint8_t dev_sbi_mb_service(uint8_t slv, uint8_t cmd, uint32_t *pMsgIn, uint32_t *pMsgOut, uint32_t us_timeout)
{
	/*
	 * This mail box service is not reenterable
	 */
	k_mutex_lock(&_s_mbSerLock, K_FOREVER);

	/*
	 * The sequence is as follows:
	 *
	 *  Firmware = MPFW (MP1/SMU)
	 *  BMC = Embedded Controller
	 *
	 *  1. The initiator (BMC) indicates that command is to be serviced by firmware by writing 0x80 to
	 *     SBRMI::InBndMsg_inst7 (SBRMI_x3F). This register must be set to 0x80 after reset.
	 *  2. The initiator (BMC) writes the command to SBRMI::InBndMsg_inst0 (SBRMI_x38).
	 *  3. For write operations or read operations which require additional addressing information as shown in the table
	 *     above, the initiator (BMC) writes Command Data In[31:0] to SBRMI::InBndMsg_inst[4:1]
	 *     {SBRMI_x3C(MSB):SBRMI_x39(LSB)}.
	 *  4. The initiator (BMC) writes 0x01 to SBRMI::SoftwareInterrupt to notify firmware to perform the requested read or
	 *     write command.
	 *  5. Firmware reads the message and performs the defined action.
	 *  6. Firmware writes the original command to outbound message register SBRMI::OutBndMsg_inst0 (SBRMI_x30).
	 *  7. Firmware will write SBRMI::Status[SwAlertSts]=1 to generate an ALERT (if enabled) to initiator (BMC) to
	 *     indicate completion of the requested command. Firmware must (if applicable) put the message data into the
	 *     message registers SBRMI::OutBndMsg_inst[4:1] {SBRMI_x34(MSB):SBRMI_x31(LSB)}.
	 *  8. For a read operation, the initiator (BMC) reads the firmware response Command Data Out[31:0] from
	 *     SBRMI::OutBndMsg_inst[4:1] {SBRMI_x34(MSB):SBRMI_x31(LSB)}.
	 *  9. Firmware clears the interrupt on SBRMI::SoftwareInterrupt.
	 * 10. BMC must write 1'b1 to SBRMI::Status[SwAlertSts] to clear the ALERT to initiator (BMC). It is recommended to
	 *     clear the ALERT upon completion of the current mailbox command.
	 */
	uint8_t ret = 0xFF;
	uint8_t tmp = 0;

	/* Step 1, writing 0x80 to SBRMI::InBndMsg_inst7 (SBRMI_x3F) to indicate that command is to be serviced */
	tmp = 0x80;
	ret = dev_sbrmi_regAccess(0, slv, 0x3F, &tmp);
	if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
	/*         and to make sure SBRMIx40[Software Interrupt] is cleared */
	tmp = 0;
	ret = dev_sbrmi_regAccess(0, slv, 0x40, &tmp);
	if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}

	/* Step 2, writes the command to SBRMI::InBndMsg_inst0 (SBRMI_x38) */
	ret = dev_sbrmi_regAccess(0, slv, 0x38, &cmd);
	if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}

	/* Step 3, msgIn to {SBRMI_x3C(MSB):SBRMI_x39(LSB)} */
	if (pMsgIn){
		tmp = *pMsgIn & 0xFF;
		ret = dev_sbrmi_regAccess(0, slv, 0x39, &tmp);
		if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
		tmp = (*pMsgIn >> 8) & 0xFF;
		ret = dev_sbrmi_regAccess(0, slv, 0x3A, &tmp);
		if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
		tmp = (*pMsgIn >> 16) & 0xFF;
		ret = dev_sbrmi_regAccess(0, slv, 0x3B, &tmp);
		if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
		tmp = (*pMsgIn >> 24) & 0xFF;
		ret = dev_sbrmi_regAccess(0, slv, 0x3C, &tmp);
		if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
	}

	/* Step 4, writes 0x01 to SBRMIx40[Software Interrupt] to notify firmware to start service */
	tmp = 1;
	ret = dev_sbrmi_regAccess(0, slv, 0x40, &tmp);
	if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}

	/* Step 5, do the service */
	/* Step 6, the original command will be copied to SBRMI::OutBndMsg_inst0 (SBRMI_x30) */

	/* Step 7, wait SBRMIx02[SwAlertSts] to 1 which indicate the completion of a mailbox operation */
	bool isAlerted = false;
	if (us_timeout) {
		k_timer_start(&sbiTimer, K_USEC(us_timeout), K_NO_WAIT);
		do {
			tmp = 0;
			ret = dev_sbrmi_regAccess(1, slv, 0x02, &tmp);
			if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
			if (tmp & 0x02)
			{
				isAlerted = true;
				break;
			}
			k_msleep(5);
		} while (k_timer_status_get(&sbiTimer) == 0);
	}
	else{
		do {
			tmp = 0;
			ret = dev_sbrmi_regAccess(1, slv, 0x02, &tmp);
			if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
			k_msleep(5);
		} while (0 == (tmp & 0x02));

		isAlerted = true;
	}

	if (!isAlerted) {
		k_mutex_unlock(&_s_mbSerLock);
		return ret;
	}

	/* Step 8, read msgOut from {SBRMI_x34(MSB):SBRMI_x31(LSB)} */
	if (pMsgOut) {
		*pMsgOut = 0;
		ret = dev_sbrmi_regAccess(1, slv, 0x31, &tmp);
		if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
		*pMsgOut |= (uint32_t)tmp;
		ret = dev_sbrmi_regAccess(1, slv, 0x32, &tmp);
		if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
		*pMsgOut |= (uint32_t)tmp << 8;
		ret = dev_sbrmi_regAccess(1, slv, 0x33, &tmp);
		if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
		*pMsgOut |= (uint32_t)tmp << 16;
		ret = dev_sbrmi_regAccess(1, slv, 0x34, &tmp);
		if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}
		*pMsgOut |= (uint32_t)tmp << 24;
	}

	/* Step 9, Firmware to clear SBRMIx40[Software Interrupt] */
	/* Step 10, BMC must write 1'b1 to SBRMI::Status[SwAlertSts] to clear the ALERT
	 * to initiator (BMC). It is recommended to clear the ALERT upon completion of
	 * the current mailbox command.
	 */
	// bit1 SwAlertSts is Write-one-to-clear from the SMBus interface
	// other fields are either reserved or read-only.
	tmp = 0x02;
	ret = dev_sbrmi_regAccess(0, slv, 0x02, &tmp);
	if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}

	/* Step 11, read the return code from OutBndMsg_inst7 (SBRMI_x37) */
	ret = dev_sbrmi_regAccess(1, slv, 0x37, &ret);
	if (ret) {k_mutex_unlock(&_s_mbSerLock); return ret;}

	k_mutex_unlock(&_s_mbSerLock);

	return ret;
}

#define WRITE_STT_SENSOR_VALUE 0x3A

/*
 * Write STT sensor value to SMU via SB-RMI mailbox.
 */
bool dev_sbi_writeSttSensor(uint8_t slv, uint8_t sensorId, uint16_t u16data)
{
	uint32_t msgIn = 0;

	// [15:0]: temperature as signed integer with 8 fractional bits.
	// [23:16]: sensor index
	// [31:24]: unused
	msgIn |= (uint32_t)u16data;
	msgIn |= ((uint32_t)sensorId << 16);
	if (F_SBI_MB_RETURN_CODE_SUCCESS == dev_sbi_mb_service(slv, WRITE_STT_SENSOR_VALUE, &msgIn, NULL, 200000)) {
		return true;
	}

	return false;
}

#define READ_PACKAGE_POWER_CONSUMPTION 0x01

/*
 * Read package power consumption from SMU via SB-RMI mailbox.
 */
uint32_t dev_sbi_readPkgPwrConsumption(uint8_t slv)
{
	uint32_t msgOut = 0;

	if (F_SBI_MB_RETURN_CODE_SUCCESS == dev_sbi_mb_service(slv, READ_PACKAGE_POWER_CONSUMPTION, NULL, &msgOut, 20000)) {
		return msgOut;
	}

	return 0xFFFFFFFF;
}

#define WRITE_PEAK_PACKAGE_POWER_LIMIT 0x3C

/*
 * Write peak package power limit (P3T) to SMU via SB-RMI mailbox.
 */
uint32_t dev_sbi_writeP3tLimit(uint8_t slv, uint32_t u32mW)
{
	uint32_t msgIn = 0;
	uint32_t msgOut = 0;

	msgIn = u32mW;
	if (F_SBI_MB_RETURN_CODE_SUCCESS == dev_sbi_mb_service(slv, WRITE_PEAK_PACKAGE_POWER_LIMIT, &msgIn, &msgOut, 20000)) {
		return msgOut;
	}

	return 0xFFFFFFFF;
}