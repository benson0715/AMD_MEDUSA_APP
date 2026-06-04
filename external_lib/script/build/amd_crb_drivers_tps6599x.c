/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/logging/log.h>
#include "amd_crb_drivers_tps6599x.h"
#include "board_config.h"
#include "system.h"
#include "i2c_hub.h"
#include "dev_utility.h"
#include "debug_info.h"

LOG_MODULE_REGISTER(tps6599x, CONFIG_TPS_LOG_LEVEL);

/* TI 4CC command retry times */
#define DRV_TPS_RETRY_TIME_MAX            (2U)  /* Set to 0 if need to disable the retry */

/* default TI PD I2C address for each port */
#ifndef I2C_4CC_ADDRESS_P0
#define I2C_4CC_ADDRESS_P0                (0x20)
#endif

#ifndef I2C_4CC_ADDRESS_P1
#define I2C_4CC_ADDRESS_P1                (0x24)
#endif

#ifndef I2C_4CC_ADDRESS_P2
#define I2C_4CC_ADDRESS_P2                (0x21)
#endif

/* Time units in u_sec */
#define DRV_TPS_RETRY_PERIOD	          (1U)

/* I2C port number */
#ifndef TPS_I2C_PORT
#define TPS_I2C_PORT                      I2C_10
#endif

/* save previous 4CC data */
uint8_t g_tps6599x_prev_4CC[NO_OF_TYPEC_PORTS][TIPD_REG_CMD1_LEN] = {0};
uint8_t g_tps6599x_prev_Data[NO_OF_TYPEC_PORTS][TIPD_MAX_PACKAGE_LENGTH] = {0};
uint8_t g_tps6599x_prev_len[NO_OF_TYPEC_PORTS] = {0};

#if CONFIG_EC_UCSI_ENABLE
uint8_t _s_ucsi_retryTimeCnt[NO_OF_TYPEC_PORTS] = {0};
/* Samsung 2K monitor workaround */
uint16_t Samsung_2K_detect[NO_OF_TYPEC_PORTS] = {0};
extern uint8_t g_4cc_pendingCmdType[NO_OF_TYPEC_PORTS];
#if UCSI_SET_PWR_LEVEL_ENABLE 	
static uint32_t calc_power(uint32_t voltage, uint32_t current)
{
    /*
       Voltage is expressed in 50 mV units.
       Current is expressed in 10 mA units.
       Power should be expressed in 250 mW units.
     */
    return div_round_up(voltage * current, 500);
}
#endif /* UCSI_SET_PWR_LEVEL_ENABLE */
#endif /* CONFIG_EC_UCSI_ENABLE */

/**
 * amd_crb_drivers_tps6599x_regAccess
 * 
 * @param [in]   isRead;     0 - to write, 1 - to read
 * @param [in]      reg;     registers' offset (see TI spec.)
 * @param [in]     pBuf;     a buffer pointer for data in/out
 * @param [in]      len;     data length/register width
 * @param [in]     port;     port number
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  Read/write the 4CC spec defined registers
 */
int amd_crb_drivers_tps6599x_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len, uint8_t port)
{
	uint8_t retry = TI_REG_ACCESS_TRTRY_TIME;
	int ret = 0;
	uint8_t i2c_addr = 0;
	
	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return 0;
	}
#if PD_TRIPPORT_ENABLE
	else if (port == TYPEC_PORT_2_IDX) {
		/* select cmd2 for p2 */
		i2c_addr = I2C_4CC_ADDRESS_P2;
	}
#endif
#if PD_DUALPORT_ENABLE
	else if (port == TYPEC_PORT_1_IDX) {
		/* select cmd2 for p2 */
		i2c_addr = I2C_4CC_ADDRESS_P1;
	}
#endif
	else {
		/* select cmd1 for p1 */
		i2c_addr = I2C_4CC_ADDRESS_P0;
	}

	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			/* I2C read */
			ret = i2c_hub_burst_read(TPS_I2C_PORT, i2c_addr, reg, pBuf, len);
		} 
		else {
			/* I2C write */
			ret = i2c_hub_burst_write(TPS_I2C_PORT, i2c_addr, reg, pBuf, len);
		}

		LOG_CLEARBUF;
		for ( uint32_t i = 0; i < len; i++ ) {
			LOG_APPEND(" %02X", *((uint8_t *)pBuf + i));
		}
		LOG_DBG("Acc4CC  %s [%02X], data(%d)%s, port(%d)", isRead ? "R" : "W", reg, len, LOG_BUF, port);

		if (ret == 0)
			return ret;
	}
	if (!retry) {
#if (CONFIG_ECDBGI_SUPPORT)			
		info_value_increase(INFO_I2C_TPS6699x, 1);
#endif		
		LOG_ERR("[!!Fatal error!!] on %s tps6599x[%02X], ret %d, port(%d)", isRead ? "R" : "W", reg, ret, port);
	}
	return ret;
}

/**
 * amd_crb_drivers_tps6599x_burst_patch_download
 * 
 * @param [in]     addr;     slave address specified in DATA1.SlaveAddress
 * @param [in]     pBuf;     a buffer pointer for data in/out
 * @param [in]      len;     data length/register width
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  Read/write the 4CC spec defined registers
 */
int amd_crb_drivers_tps6599x_burst_patch_download(uint8_t addr, void * pBuf, uint32_t len)
{
	uint8_t retry = TI_REG_ACCESS_TRTRY_TIME;
	int ret = 0;
	
	while (retry) {
		retry --;
		ret = 0;

		/* I2C write */
		ret = i2c_hub_write(TPS_I2C_PORT, pBuf, len, addr);

		LOG_CLEARBUF;
		for ( uint32_t i = 0; i < len; i++ ) {
			LOG_APPEND(" %02X", *((uint8_t *)pBuf + i));
		}
		LOG_DBG("Acc4CC [%02X], data(%d)%s", addr, len, LOG_BUF);

		if (ret == 0)
			return ret;
	}
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] burst_patch_download fail");
	}
	return ret;
}

/**
 * amd_crb_drivers_tps6599x_wait4CmdDone
 *
 * @param [in]     port;     port for ti pd
 * @param [in]     us;       max delay us for 4cc command wait
 * @return 0 If successful.
 * @return TIPD_4CC_nCMD If fail.
 *
 * @note
 *  wait the 4cc command to be done with max us delay
 */
uint32_t amd_crb_drivers_tps6599x_wait4CmdDone (uint8_t port, uint32_t ms)
{
	uint8_t cmdBuf[5] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
	uint32_t u32Rsp = 0;
	uint32_t attempts = 0;
	
	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)){
		/* return failed if port num upto 2 */
		return TIPD_4CC_nCMD;
	}
	
	/* if use the time out based on tick */
	if (ms) {
		do {
			amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_CMD1, cmdBuf, (TIPD_REG_CMD1_LEN + 1), port);

			u32Rsp = TIPD_4CCPTR_TO_U32(&cmdBuf[1]);
			if (0 == u32Rsp ||  TIPD_4CC_nCMD == u32Rsp) {
				break;
			}
			k_sleep(K_MSEC(DRV_TPS_RETRY_PERIOD)); /* sleep "DRV_TPS_RETRY_PERIOD" ms */
			attempts++;
		} while (attempts < ms);
	} 
	else 
	{
		while (1) {
			amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_CMD1, cmdBuf, (TIPD_REG_CMD1_LEN + 1), port);

			u32Rsp = TIPD_4CCPTR_TO_U32(&cmdBuf[1]);
			if (0 == u32Rsp ||  TIPD_4CC_nCMD == u32Rsp) {
				break;
			}
			k_sleep(K_MSEC(DRV_TPS_RETRY_PERIOD));
		}
	}
	return u32Rsp;
}

/**
 * amd_crb_drivers_tps6599x_cmdExecutor
 *
 * @param [in]     timeout;    command timeout
 * @param [in]     port;       port for ti pd
 * @param [in]     pDataIn;    input buffer pointer
 * @param [in]     dataInLen;  input buffer length
 * @param [in]     p4CC;       4cc command pointer
 * @param [in]     pDataOut;   output buffer pointer
 * @param [in]     dataOutLen; output buffer length
 * @return 0 If successful.
 * @return TIPD_4CC_nCMD If fail.
 *
 * @note
 *  send 4cc command with input data and get output data
 */
uint32_t amd_crb_drivers_tps6599x_cmdExecutor(uint32_t timeout, uint8_t port, uint8_t * pDataIn, uint8_t dataInLen, uint8_t * p4CC, uint8_t * pDataOut, uint8_t dataOutLen)
{
	/* Check the in and out buffer length */
	if ((dataInLen > TIPD_MAX_PACKAGE_LENGTH) || (dataOutLen > TIPD_MAX_PACKAGE_LENGTH)) {
		LOG_WRN("Length overflow");
		/* length overflow */
		return TIPD_4CC_nCMD;
	}

	uint8_t tmpBuf[TIPD_MAX_PACKAGE_LENGTH + 1]; /* add 1 for length byte */
	uint32_t u32rsp = 0;
	int ret = 0;
	uint8_t index;
	
	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return TIPD_4CC_nCMD;
	}

	/* 1. CMD is 0 or '!CMD' */
	amd_crb_drivers_tps6599x_wait4CmdDone(port, timeout);

	/* 2. write DataIn to DATA if (dataInLen != 0) */
	if ((dataInLen != 0) && (pDataIn != NULL)) {
		memcpy(&tmpBuf[1], pDataIn, dataInLen);
		tmpBuf[0] = dataInLen; /* lenght in first byte */
		
		/* Save the currnet 4CC data */
		g_tps6599x_prev_len[port] = dataInLen;
		for (index = 0; index < dataInLen; index++) {
			g_tps6599x_prev_Data[port][index] = pDataIn[index];
		}

		amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_DATA1, tmpBuf, (dataInLen + 1), port);
	}
	else {
		g_tps6599x_prev_len[port] = 0;
	}

	/* 3. write 4-byte pCmd to CMD */
	if (p4CC) {
		memcpy(&tmpBuf[1], p4CC, TIPD_REG_CMD1_LEN);  /* both ports have some cmd length */
		tmpBuf[0] = TIPD_REG_CMD1_LEN;
		
		/* Save the currnet 4CC command */
		for (index = 0; index < 4; index++) {
			g_tps6599x_prev_4CC[port][index] = p4CC[index];
		}

		amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_CMD1, tmpBuf, (TIPD_REG_CMD1_LEN + 1), port);
	}

	/* 4. wait until CMD changes to 0 or '!CMD' */
	u32rsp = amd_crb_drivers_tps6599x_wait4CmdDone(port, timeout);

	/* 5. return if !CMD */
	if (u32rsp == TIPD_4CC_nCMD)
		return TIPD_4CC_nCMD;

	/* 6. read pDataOut */
	if (pDataOut && dataOutLen) {
		tmpBuf[0] = 0;
		ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_DATA1, tmpBuf, (dataOutLen + 1), port);

		if ((ret == 0) && (tmpBuf[0] >= dataOutLen)) {
			for (uint8_t i  = 0; i < dataOutLen; i++, pDataOut++) {
				*pDataOut = tmpBuf[i + 1];
			}

			LOG_DBG("%s success P:%d", p4CC, port);
			return 0; /* success get the resp */
		}
	}

	LOG_DBG("%s failed !! P:%d", p4CC, port);

	/* no used for now */
	//_s_ucsi_retryTimeCnt[port] = DRV_TPS_RETRY_TIME_MAX;

	return TIPD_4CC_nCMD;
}

/**
 * amd_crb_drivers_tps6599x_readSnkRdo
 *
 * @param [in]     port;       port for ti pd
 * @return 0 If no contract.
 * @return sink rdo from pd controller
 *
 * @note
 *  read the sink rdo from pd controller
 */
uint32_t amd_crb_drivers_tps6599x_readSnkRdo(uint8_t port)
{
	DRV_TPS6599X_ACTIVE_CONTRACT_RDO actRdo;
	memset(&actRdo, 0, sizeof(actRdo));
	actRdo.u8Length = TIPD_REG_ACTIVE_RDO_LEN;

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return 0;
	}
	/* Only do it when attached as sink */
	if ((dpm_get_info(port)->attach) &&
		(dpm_get_info(port)->cur_port_role == PRT_ROLE_SINK) &&
		(dpm_get_info(port)->contract_exist)) {
		int ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_ACTIVE_RDO, &actRdo, sizeof(actRdo), port);
		LOG_DBG("PORT #%d: PDx35 - TIPD_REG_ACTIVE_RDO SINK - 0x%08X", port, actRdo.xDo.val);
		if (ret == 0) {
			return actRdo.xDo.val;
		}
	}
	/* Otherwise return 0 */
	return 0;
}

/**
 * amd_crb_drivers_tps6599x_readSrcRdo
 *
 * @param [in]     port;       port for ti pd
 * @return 0 If no contract.
 * @return source rdo from pd controller
 *
 * @note
 *  read the source rdo from pd controller
 */
uint32_t amd_crb_drivers_tps6599x_readSrcRdo(uint8_t port)
{
	DRV_TPS6599X_ACTIVE_CONTRACT_RDO actRdo;
	memset(&actRdo, 0, sizeof(actRdo));
	actRdo.u8Length = TIPD_REG_ACTIVE_RDO_LEN;

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return 0;
	}
	/* Only do it when attached as sink */
	if ((dpm_get_info(port)->attach) &&
			(dpm_get_info(port)->cur_port_role == PRT_ROLE_SOURCE) &&
			(dpm_get_info(port)->contract_exist)) {
		int ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_ACTIVE_RDO, &actRdo, sizeof(actRdo), port);
		LOG_DBG("PORT #%d: PDx35 - TIPD_REG_ACTIVE_RDO Source - 0x%08X", port, actRdo.xDo.val);
		if (ret == 0) {
			return actRdo.xDo.val;
		}
	}
	/* Otherwise return 0 */
	return 0;
}

/**
 * amd_crb_drivers_tps6599x_readSnkPdo
 *
 * @param [in]     port;       port for ti pd
 * @return 0 If no contract.
 * @return sink pdo from pd controller
 *
 * @note
 *  read the sink pdo from pd controller
 */
uint32_t amd_crb_drivers_tps6599x_readSnkPdo(uint8_t port)
{
	DRV_TPS6599X_ACTIVE_CONTRACT_PDO actPdo;
	memset(&actPdo, 0, sizeof(actPdo));
	actPdo.u8Length = TIPD_REG_ACTIVE_PDO_LEN;

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return 0;
	}
	/* Only do it when attached as sink */
	if ((dpm_get_info(port)->attach) &&
			(dpm_get_info(port)->cur_port_role == PRT_ROLE_SINK) &&
			(dpm_get_info(port)->contract_exist)) {
		int ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_ACTIVE_PDO, &actPdo, sizeof(actPdo), port);
		LOG_DBG("PORT #%d: PDx34 - TIPD_REG_ACTIVE_PDO   - 0x%08X", port, actPdo.xDo.val);
		LOG_DBG("		 cur10mA %5d, vol50mV %5d", actPdo.xDo.fixed_src.max_current * 10, actPdo.xDo.fixed_src.voltage * 50);
		if (ret == 0) {
			return actPdo.xDo.val;
		}
	}
	/* Otherwise return 0 */
	else {
		return 0;
	}

	return 0;
}

/**
 * amd_crb_drivers_tps6599x_deviceInfo
 *
 * @param [in]     port;       port for ti pd
 * @return ti pd device info (tps6599x AC/AD/BF)
 *
 * @note
 *  read ti pd device info to know part number
 */
TIPD_DEVICE_TYPE amd_crb_drivers_tps6599x_deviceInfo(uint8_t port)
{
	DRV_TPS6599X_DEV_INFO status;
	int ret = 0;
	TIPD_DEVICE_TYPE deviceType = TPS6599X_NULL;

	LOG_DBG("DeviceInfo P:%d", port);

	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_DEV_INFO, &status, sizeof(status), port);

    /* BH: 54 50 53 36 35 39 39 34 20 48 57 30 30 42 32 20 46 57 46 39 30 39 2E 31 32 2E 30 30 5F 30 30 30 37 20 5A 41 63 65 47 00 */
    /*     ( TPS65994 HW00B2 FWF909.12.00-0007 ZAceG */

	/* BF: 54 50 53 36 35 39 39 34 20 48 57 30 30 42 30 20 46 57 46 37 30 39 2E 30 38 2E 30 30 5F 30 30 30 35 20 5A 41 63 65 47 00 */
	/*     ( TPS65994 HW00B0 FWF709.08.00-0005 ZAceG */

	/* AD: 54 50 53 36 35 39 39 34 20 48 57 30 30 41 32 20 46 57 46 35 30 39 2E 30 34 2E 30 30 5F 30 30 30 35 20 5A 41 63 65 47 00 */
	/*     ( TPS65994 HW00A2 FWF509.04.00-0005 ZAceG */

	/* AC: 54 50 53 36 35 39 39 34 20 48 57 30 30 41 32 20 46 57 46 33 30 39 2E 30 32 2E 30 30 5F 30 30 31 38 20 5A 41 63 65 47 00 */
	/*     ( TPS65994 HW00A2 FWF309.02.00-0018 ZAceG */
    if ((status.buf[19] == '9') && (status.buf[20] == '0') && (status.buf[21] == '9'))
    {
        deviceType = TPS6599X_BH;
    }
    else if ((status.buf[19] == '7') && (status.buf[20] == '0') && (status.buf[21] == '9')) {
		deviceType = TPS6599X_BF;
	}
	else if ((status.buf[19] == '5') && (status.buf[20] == '0') && (status.buf[21] == '9')) {
		deviceType = TPS6599X_AD;
	}
	else if ((status.buf[19] == '3') && (status.buf[20] == '0') && (status.buf[21] == '9')) {
		deviceType = TPS6599X_AC;
	}
	else {
		deviceType = TPS6599X_AD;
	}

	/* Check device is type */
	return deviceType;
}

/**
 * amd_crb_drivers_tps6599x_gpioConfig
 *
 * @param [in]     port;          port for ti pd
 * @param [in]     pinMask;       pin mask to change
 * @param [in]     outputEnable;  enable pin output
 * @param [in]     outputCtrl;    output high/low
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  config the ti pd gpio status
 */
int amd_crb_drivers_tps6599x_gpioConfig(uint8_t port, uint16_t pinMask, uint16_t outputEnable, uint16_t outputCtrl)
{
	DRV_TPS6599X_GPIO_CONFIG config;
	int ret = 0;

	LOG_DBG("GPIO_Config P:%d", port);

	/* Read the whole gpio status */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_GPIO_CONFIG, &config, sizeof(config), port);

	config.buf[0] &= ~pinMask;
	config.buf[1] &= ~(pinMask >> 8);

	config.buf[0] |= (outputEnable & pinMask);
	config.buf[1] |= (outputEnable & pinMask) >> 8;

	config.buf[8] &= ~pinMask;
	config.buf[9] &= ~(pinMask >> 8);

	config.buf[8] |= (outputCtrl & pinMask);
	config.buf[9] |= (outputCtrl & pinMask) >> 8;

	/* Write the output enable to gpio status */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_GPIO_CONFIG, &config, sizeof(config), port);

	/* Read the whole gpio status */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_GPIO_CONFIG, &config, sizeof(config), port);

	return ret;
}

/**
 * amd_crb_drivers_tps6599x_isUsb4Enter
 *
 * @param [in]     port;          port for ti pd
 * @return true: enter USB4 mode
 *
 * @note
 *  return if ti pd enter USB4 mode
 */
bool amd_crb_drivers_tps6599x_isUsb4Enter(uint8_t port)
{
	DRV_TPS6599X_DATA_STATUS status;
	int ret = 0;

	LOG_DBG("DATA_STATUS P:%d", port);

	/* Read the USB config */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_DATA_STATUS, &status, sizeof(status), port);

	if (status.f.Usb4Connection)
		return true;
	else
		return false;
}

/**
 * amd_crb_drivers_tps6599x_isUsb4Enter
 *
 * @param [in]     port;          port for ti pd
 * @return true: enter TBT mode
 *
 * @note
 *  return if ti pd enter TBT mode
 */
bool amd_crb_drivers_tps6599x_isTbtEnter(uint8_t port)
{
	DRV_TPS6599X_DATA_STATUS status;
	int ret = 0;

	LOG_DBG("DATA_STATUS P:%d", port);

	/* Read the USB config */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_DATA_STATUS, &status, sizeof(status), port);

	if (status.f.TbtConnection)
		return true;
	else
		return false;
}

/**
 * amd_crb_drivers_tps6599x_txVidUpdate
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     numValidVDOs;   vdo number
 * @param [in]     CapAsDev;       capability as device
 * @param [in]     Vdo;            vdo pointer
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  update the tx vid register
 */
int amd_crb_drivers_tps6599x_txVidUpdate(uint8_t port, uint8_t numValidVDOs, bool CapAsDev, uint32_t* Vdo)
{
	DRV_TPS6599X_TX_VID vid;
	int ret = 0;

	LOG_DBG("TIPD_REG_TX_IDENTITY P:%d", port);

	/* Read the TX vid */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_IDENTITY, &vid, sizeof(vid), port);

	/* MF:4 and USB4:6 */
	vid.f.numValidVDOs = numValidVDOs;
	/* MF_CapAsDev:false and USB4_CapAsDev:True */
	vid.f.UsbCommCapableAsDevice = CapAsDev;
	/* MF: DFPVDO/NA/NA and USB4: DFPVDO/UFPVDO1/UFPVDO2 */
	for (uint8_t index = 0; index < numValidVDOs; index++) {
		vid.buf[1 + (index << 2)] = (uint8_t)(Vdo[index] >> 0);
		vid.buf[2 + (index << 2)] = (uint8_t)(Vdo[index] >> 8);
		vid.buf[3 + (index << 2)] = (uint8_t)(Vdo[index] >> 16);
		vid.buf[4 + (index << 2)] = (uint8_t)(Vdo[index] >> 24);
	}

	/* Write TX vid */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_TX_IDENTITY, &vid, sizeof(vid), port);

	return ret;
}

/**
 * amd_crb_drivers_tps6599x_txVidChangeModelOpSupp
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     ModelOpSupp;    model operation support bit
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  change the model operation support bit in tx vid register
 */
int amd_crb_drivers_tps6599x_txVidChangeModelOpSupp(uint8_t port, bool ModelOpSupp)
{
	DRV_TPS6599X_TX_VID vid;
	int ret = 0;

	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_IDENTITY, &vid, DRV_TPS6599X_TX_VID_REGISTER_LENGTH, port);
	/* Change the model operation support bit */
	vid.f.ModalOperationSupported = ModelOpSupp;

	/* Write TX vid */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_TX_IDENTITY, &vid, DRV_TPS6599X_TX_VID_REGISTER_LENGTH, port);
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_IDENTITY, &vid, DRV_TPS6599X_TX_VID_REGISTER_LENGTH, port);
	return ret;
}

/**
 * amd_crb_drivers_tps6599x_txVidOverwrite
 *
 * @param [in]     port;           port for ti pd
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  override the tx vid register (hard code)
 */
int amd_crb_drivers_tps6599x_txVidOverwrite(uint8_t port)
{
	DRV_TPS6599X_TX_VID vid;
	int ret = 0;
	uint8_t vid_usb4[26] = {0x19, 0x06, 0x51, 0x04, 0x00, 0x91, 0x51, 0x04, 0x00, 0x00, 0x04,
	                        0x09, 0x00, 0x00, 0x1b, 0x00, 0x80, 0x28, 0x00, 0x00, 0x00, 0x00,
	                        0x00, 0x00, 0x80, 0x07};
	uint8_t vid_usb3[26] = {0x19, 0x04, 0x51, 0x04, 0x00, 0x91, 0x51, 0x04, 0x00, 0x00, 0x04,
	                        0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x00,
	                        0x00, 0x00, 0x00, 0x00};

	if ((port == TYPEC_PORT_0_IDX) || (port == TYPEC_PORT_1_IDX)) {
		vid.u8Length = vid_usb4[0];
		memcpy(vid.buf, &vid_usb4[1], (sizeof(vid_usb4) - 1));
	}
	else if (port == TYPEC_PORT_2_IDX) {
		vid.u8Length = vid_usb3[0];
		memcpy(vid.buf, &vid_usb3[1], (sizeof(vid_usb3) - 1));
	}

	/* Write TX vid */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_TX_IDENTITY, &vid, DRV_TPS6599X_TX_VID_REGISTER_LENGTH, port);

	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_IDENTITY, &vid, DRV_TPS6599X_TX_VID_REGISTER_LENGTH, port);

	return ret;
}

/**
 * amd_crb_drivers_tps6599x_setSxAppConfig
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     sys_status;     system status (S5/4/3/0 and G3)
 * @return 0 If successful.
 * @return -EIO General input / output error.
 *
 * @note
 *  change the ti pd system register based on real system status
 */
int amd_crb_drivers_tps6599x_setSxAppConfig(uint8_t port, enum system_power_state sys_status)
{
	DRV_TPS6599X_SET_SX_APP_CONFIG config;
	int ret = 0;

	LOG_DBG("TIPD_REG_SET_SX P:%d", port);

	/* Read the TX vid */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_SX_APP_CONFIG, &config, sizeof(config), port);

	switch (sys_status) {
		case SYSTEM_S5_STATE:
			config.f.SleepState = 3;
			break;
		case SYSTEM_S4_STATE:
		case SYSTEM_S3_STATE:
			config.f.SleepState = 1;
			break;
		case SYSTEM_S0_STATE:
			config.f.SleepState = 0;
			break;
		default:
			config.f.SleepState = 3;
			break;
	}

	/* Write TX vid */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_SX_APP_CONFIG, &config, sizeof(config), port);

	return ret;
}

/**
 * amd_crb_drivers_tps6599x_bootStatus
 *
 * @param [in]     port;           port for ti pd
 *
 * @note
 *  read boot status (normal or dead battery)
 */
void amd_crb_drivers_tps6599x_bootStatus(uint8_t port)
{
	int ret = 0;
	DRV_TPS6599X_BOOT_FLAGS status;

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return;
	}

	LOG_DBG("Boot status P:%d", port);

	/* read current port control */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_BOOT, &status, sizeof(status), port);
}

/**
 * amd_crb_drivers_tps6599x_sleepConfig
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     sleep_en;       enable the sleep mode
 * @param [in]     sleep_time;     delay before enter sleep mode 100ms or 1000ms
 *
 * @note
 *  config the sleep status
 */
void amd_crb_drivers_tps6599x_sleepConfig(uint8_t port, bool sleep_en, uint8_t sleep_time)
{
	int ret = 0;
	DRV_TPS6599X_SLEEP_CONFIG sleep_config;

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return;
	}

	LOG_DBG("Sleep config P:%d", port);

	/* read current port control */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_SLEEP_CONFIG, &sleep_config, sizeof(sleep_config), port);

	if (!ret) {
		/* 1: enable sleep; 0: disable sleep */
		sleep_config.f.SleepModeAllowed = sleep_en;
		/* 1: 100ms delay; 2: 1000ms delay */
		sleep_config.f.SleepTime = sleep_time;
	}

	/* write port control back to device */
	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_SLEEP_CONFIG, &sleep_config, sizeof(sleep_config), port);
}

/**
 * amd_crb_drivers_tps6599x_deadBattery
 *
 * @param [in]     port;        port for ti pd
 *
 * @return true->dead battery boot
 *
 * @note
 *  if ti pd boots up with dead battery
 */
bool amd_crb_drivers_tps6599x_deadBattery(uint8_t port)
{
	DRV_TPS6599X_POWER_PATH status;
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	uint32_t ret = 0;

	LOG_DBG("PowerPath P:%d", port);

	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_PWR_PATH_STATUS, &status, sizeof(status), port);

	if (ret == 0) {
		if (status.f.PowerSource == 0x02) {
			dpm_stat->dead_bat = true;
			LOG_DBG("DB-Boot");
			return true;
		}
	}
	dpm_stat->dead_bat = false;
	LOG_DBG("NOR-Boot");
	return false;
}

/**
 * amd_crb_drivers_tps6599x_portConfigPdDisable
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     pd_disable;     disable/enable pd
 *
 * @note
 *  disable or enable pd policy engine
 */
void amd_crb_drivers_tps6599x_portConfigPdDisable(uint8_t port, uint8_t pd_disable)
{
	DRV_TPS6599X_PORT_CONFIG status, status_set, status_clear;

	for (uint8_t index = 0; index < DRV_TPS6599X_PORT_CONFIG_REGISTER_LENGTH; index++) {
		status_set.buf[index] = 0;
		status_clear.buf[index] = 0;
	}

	amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_PORT_CONFIG, &status, sizeof(status), port);

	LOG_DBG("Port Ctrl disable pd:%d P:%d", pd_disable, port);

	if ((pd_disable) && (status.f.DisablePD == 0)) {
			status_set.f.DisablePD = 1; /* 01b for legacy USB source*/
			status_clear.f.DisablePD = 1;
			amd_crb_drivers_tps6599x_portConfig(port, status_set, status_clear);
	}
	else if ((pd_disable == 0) && (status.f.DisablePD)) {
			status_set.f.DisablePD = 0; /* 00b for pd active*/
			status_clear.f.DisablePD = 1;
			amd_crb_drivers_tps6599x_portConfig(port, status_set, status_clear);
	}
}

/**
 * amd_crb_drivers_tps6599x_port_config
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     status_set;     set bit mask
 * @param [in]     status_clear;   clear bit mask
 *
 * @note
 *  set and clear the port config register
 */
void amd_crb_drivers_tps6599x_portConfig(uint8_t port, DRV_TPS6599X_PORT_CONFIG status_set, DRV_TPS6599X_PORT_CONFIG status_clear)
{
	int ret = 0;
	DRV_TPS6599X_PORT_CONFIG status;

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return;
	}

	LOG_DBG("Port Config P:%d", port);

	/* read current port control */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_PORT_CONFIG, &status, sizeof(status), port);

	if (ret == 0) {
		for (uint8_t i  = 0; i < TIPD_REG_PORT_CONFIG_LEN; i++) {
			/* clear status first */
			status.buf[i] &= ~status_clear.buf[i];
			/* set status second */
			status.buf[i] |= status_set.buf[i];
		}
	}

	/* write port control back to device */
	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_PORT_CONFIG, &status, sizeof(status), port);
}

/**
 * amd_crb_drivers_tps6599x_portConfigStateMachine
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     state_machine;  typec state machine
 *
 * @note
 *  set the new typec state machine
 */
void amd_crb_drivers_tps6599x_portConfigStateMachine(uint8_t port, port_role_t state_machine)
{
	DRV_TPS6599X_PORT_CONFIG status, status_set, status_clear;

	LOG_DBG("port config status machine:%d P:%d", state_machine, port);

	for (uint8_t index = 0; index < DRV_TPS6599X_PORT_CONFIG_REGISTER_LENGTH; index++) {
		status_set.buf[index] = 0;
		status_clear.buf[index] = 0;
	}

	amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_PORT_CONFIG, &status, sizeof(status), port);

	switch (state_machine) {
		case PRT_ROLE_SINK:
			status_set.f.TypeCStateMachine = 0; /* 00b for UFP only*/
			status_clear.f.TypeCStateMachine = 3;
			if (status.f.TypeCStateMachine != PRT_ROLE_SINK)
				amd_crb_drivers_tps6599x_portConfig(port, status_set, status_clear);
			break;
		case PRT_ROLE_SOURCE:
			status_set.f.TypeCStateMachine = 1; /* 01b for DFP only*/
			status_clear.f.TypeCStateMachine = 3;
			if (status.f.TypeCStateMachine != PRT_ROLE_SOURCE)
				amd_crb_drivers_tps6599x_portConfig(port, status_set, status_clear);
			break;
		case PRT_DUAL:
			status_set.f.TypeCStateMachine = 2; /* 10b for DRP*/
			status_clear.f.TypeCStateMachine = 3;
			if (status.f.TypeCStateMachine != PRT_DUAL)
				amd_crb_drivers_tps6599x_portConfig(port, status_set, status_clear);
			break;
		case PRT_NULL:
			status_set.f.TypeCStateMachine = 3; /* 11b for no action in CC (no pull-up and pull-down)*/
			status_clear.f.TypeCStateMachine = 3;
			if (status.f.TypeCStateMachine != PRT_NULL)
				amd_crb_drivers_tps6599x_portConfig(port, status_set, status_clear);
			break;
		default:
			break;
	}
}

/**
 * amd_crb_drivers_tps6599x_cmdxRetry
 *
 * @param [in]     timeout;        time out for retry
 * @param [in]     port;           port for ti pd
 * @return 0 If successful.
 * @return TIPD_4CC_nCMD If fail.
 *
 * @note
 *  retry 4cc command
 */
uint32_t amd_crb_drivers_tps6599x_cmdxRetry(uint32_t timeout, uint8_t port)
{
	uint8_t tmpBuf[TIPD_MAX_PACKAGE_LENGTH + 1]; /* add 1 for length byte */

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return TIPD_4CC_nCMD;
	}
	
	LOG_DBG("retry P:%d %d %d %d %d", port, g_tps6599x_prev_4CC[port][0],
			                                  g_tps6599x_prev_4CC[port][1],
											  g_tps6599x_prev_4CC[port][2],
											  g_tps6599x_prev_4CC[port][3]);

	if (g_tps6599x_prev_len[port] != 0) {
		/* 2. write DataIn to DATA if (dataInLen != 0) */
		memcpy(&tmpBuf[1], g_tps6599x_prev_Data[port], g_tps6599x_prev_len[port]);
		tmpBuf[0] = g_tps6599x_prev_len[port]; /* lenght in first byte */

		amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_DATA1, tmpBuf, (g_tps6599x_prev_len[port] + 1), port);
	}

	/* 3. write 4-byte pCmd to CMD */
	memcpy(&tmpBuf[1], g_tps6599x_prev_4CC[port], TIPD_REG_CMD1_LEN);  /* both ports have some cmd length */
	tmpBuf[0] = TIPD_REG_CMD1_LEN;
	
	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_CMD1, tmpBuf, (TIPD_REG_CMD1_LEN + 1), port);
	
	return 0;
}

/**
 * amd_crb_drivers_tps6599x_portCtrlDrSwapResp
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     resp;           dr swap response
 *
 * @note
 *  change dr swap response
 */
void amd_crb_drivers_tps6599x_portCtrlDrSwapResp(uint8_t port, app_swap_resp_t resp)
{
	DRV_TPS6599X_PORT_CONTROL status_set, status_clear;

	for (uint8_t index = 0; index < DRV_TPS6599X_PORT_CONTROL_REGISTER_LENGTH; index++) {
		status_set.buf[index] = 0;
		status_clear.buf[index] = 0;
	}

	LOG_DBG("Port Ctrl dr swap:%d P:%d", resp, port);

	/* Force clear the Initiate swap to DFP */
	//status_clear.f.InitiateSwapToDFP = 1;
	status_set.f.InitiateSwapToDFP = 1;

	if (resp == APP_RESP_ACCEPT) {
		status_set.f.ProcessSwapToDFP = 1;
		//status_set.f.ProcessSwapToUFP = 1;
		amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
	}
	else if (resp == APP_RESP_REJECT) {
		status_clear.f.ProcessSwapToDFP = 1;
		//status_clear.f.ProcessSwapToUFP = 1;
		amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
	}
}

/**
 * amd_crb_drivers_tps6599x_portCtrlPrSwapResp
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     resp;           pr swap response
 *
 * @note
 *  change pr swap response
 */
void amd_crb_drivers_tps6599x_portCtrlPrSwapResp(uint8_t port, app_swap_resp_t resp)
{
	DRV_TPS6599X_PORT_CONTROL status_set, status_clear;

	for (uint8_t index = 0; index < DRV_TPS6599X_PORT_CONTROL_REGISTER_LENGTH; index++) {
		status_set.buf[index] = 0;
		status_clear.buf[index] = 0;
	}

	LOG_DBG("Port Ctrl pr swap:%d P:%d", resp, port);

	if (resp == APP_RESP_ACCEPT) {
		status_set.f.ProcessSwapToSource = 1;
		status_set.f.ProcessSwapToSink = 1;
		amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
	}
	else if (resp == APP_RESP_REJECT) {
		status_clear.f.ProcessSwapToSource = 1;
		status_clear.f.ProcessSwapToSink = 1;
		amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
	}
}

/**
 * amd_crb_drivers_tps6599x_port_ctl
 *
 * @param [in]     port;           port for ti pd
 *
 * @note
 *  read typec current level
 */
void amd_crb_drivers_tps6599x_readTypecCurrent(uint8_t port)
{
	int ret = 0;
	DRV_TPS6599X_PWR_STATUS pwr_status;
	dpm_status_t *dpm_stat = dpm_set_status(port);

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return;
	}

	/* read current port control */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_PWR_STATUS, &pwr_status, sizeof(pwr_status), port);

	if (ret == 0) {
		switch (pwr_status.f.Type_C_Current) {
			case DPM_CMD_SET_RP_DFLT:
				dpm_stat->src_cur_level = DPM_CMD_SET_RP_DFLT;
				dpm_stat->contract_exist = false;
				break;
			case DPM_CMD_SET_RP_1_5A:
				dpm_stat->src_cur_level = DPM_CMD_SET_RP_1_5A;
				dpm_stat->contract_exist = false;
				break;
			case DPM_CMD_SET_RP_3A:
				dpm_stat->src_cur_level = DPM_CMD_SET_RP_3A;
				dpm_stat->contract_exist = false;
				break;
			case DPM_CMD_SET_PD:
				dpm_stat->src_cur_level = DPM_CMD_SET_PD;
				dpm_stat->contract_exist = true;
				break;
			default:
				break;
		}
	}

	LOG_DBG("read type-c current P:%d Le: %d", port, dpm_stat->src_cur_level);
}

/**
 * amd_crb_drivers_tps6599x_port_ctl
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     status_set;     set bit mask
 * @param [in]     status_clear;   clear bit mask
 *
 * @note
 *  set and clear port control register
 */
void amd_crb_drivers_tps6599x_portCtrl(uint8_t port, DRV_TPS6599X_PORT_CONTROL status_set, DRV_TPS6599X_PORT_CONTROL status_clear)
{
	int ret = 0;
	DRV_TPS6599X_PORT_CONTROL port_ctrl;

	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return;
	}

	LOG_DBG("Port Ctrl P:%d", port);

	/* read current port control */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);

	if (ret == 0) {
		for (uint8_t i  = 0; i < TIPD_REG_PORT_CTL_LEN; i++) {
			/* clear status first */
			port_ctrl.buf[i] &= ~status_clear.buf[i];
			/* set status second */
			port_ctrl.buf[i] |= status_set.buf[i];
		}
	}

	/* write port control back to device */
	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_PORT_CTL, &port_ctrl, sizeof(port_ctrl), port);
}

#if CONFIG_EC_UCSI_ENABLE

/**
 * amd_crb_drivers_tps6599x_cmdx
 *
 * @param [in]     timeout;        time out for retry
 * @param [in]     port;           port for ti pd
 * @param [in]     pDataIn;        input buffer pointer
 * @param [in]     dataInLen;      input buffer length
 * @param [in]     p4CC;           4cc command
 * @return 0 If successful.
 * @return TIPD_4CC_nCMD If fail.
 *
 * @note
 *  send the 4cc command without reading response
 */
uint32_t amd_crb_drivers_tps6599x_cmdx(uint32_t timeout, uint8_t port, uint8_t * pDataIn, uint8_t dataInLen, uint8_t * p4CC)
{
	/* Check the in buffer length */
	if (dataInLen > TIPD_MAX_PACKAGE_LENGTH) {
		LOG_WRN("Length overflow");
		/* length overflow */
		return TIPD_4CC_nCMD;
	}

	uint8_t tmpBuf[TIPD_MAX_PACKAGE_LENGTH + 1]; /* add 1 for length byte */
	uint8_t index;
	
	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return TIPD_4CC_nCMD;
	}

	LOG_DBG("%s Start P:%d", p4CC, port);

	/* 1. CMD is 0 or '!CMD' */
	amd_crb_drivers_tps6599x_wait4CmdDone(port, timeout);

	/* 2. write DataIn to DATA if (dataInLen != 0) */
	if ((dataInLen != 0) && (pDataIn != NULL))  {
		memcpy(&tmpBuf[1], pDataIn, dataInLen);
		tmpBuf[0] = dataInLen; /* lenght in first byte */
		
		/* Save the currnet 4CC data */
		g_tps6599x_prev_len[port] = dataInLen;
		for (index = 0; index < dataInLen; index++) {
			g_tps6599x_prev_Data[port][index] = pDataIn[index];
		}

		amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_DATA1, tmpBuf, (dataInLen + 1), port);
	}
	else {
		g_tps6599x_prev_len[port] = 0;
	}

	/* 3. write 4-byte pCmd to CMD */
	if (p4CC) {
		memcpy(&tmpBuf[1], p4CC, TIPD_REG_CMD1_LEN);  /* both ports have some cmd length */
		tmpBuf[0] = TIPD_REG_CMD1_LEN;
		
		/* Save the currnet 4CC command */
		for (index = 0; index < 4; index++) {
			g_tps6599x_prev_4CC[port][index] = p4CC[index];
		}

		amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_CMD1, tmpBuf, (TIPD_REG_CMD1_LEN + 1), port);
	}
	
	LOG_DBG("%s Send P:%d ", p4CC, port);

	_s_ucsi_retryTimeCnt[port] = DRV_TPS_RETRY_TIME_MAX;
	
	return 0;
}

/**
 * amd_crb_drivers_tps6599x_cmdAltMode
 *
 * @param [in]     timeout;        time out for retry
 * @param [in]     port;           port for ti pd
 * @param [in]     svid;           svid pointer
 * @param [in]     ObjPos;         object position
 * @param [in]     p4CC;           4cc command
 *
 * @note
 *  send the alt mode discovery
 */
void amd_crb_drivers_tps6599x_cmdAltMode(uint32_t timeout, uint8_t port, uint8_t* svid, uint8_t ObjPos, uint8_t * p4CC)
{
	uint8_t tmpBuf[TIPD_MAX_PACKAGE_LENGTH + 1]; /* add 1 for length byte */
	uint8_t index;
	
	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return;
	}
	
	LOG_DBG("%s Start P:%d", p4CC, port);

	/* 1. CMD is 0 or '!CMD' */
	//amd_crb_drivers_tps6599x_wait4CmdDone(port, timeout);

	/* 2. write DataIn to DATA if (dataInLen != 0) */
	if (ObjPos == 0xFF) { /* For MuxR */
		tmpBuf[0] = TIPD_CMD_ST_ALT_MODE_LEN; /* lenght in first byte */
		tmpBuf[1] = 0x01;
		tmpBuf[2] = 0x00;
		tmpBuf[3] = 0x00;	
	}
	else {
		tmpBuf[0] = TIPD_CMD_ST_ALT_MODE_LEN; /* lenght in first byte */
		tmpBuf[1] = (ObjPos << 5);
		tmpBuf[2] = svid[0];
		tmpBuf[3] = svid[1];
	}

	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_DATA1, tmpBuf, (TIPD_CMD_ST_ALT_MODE_LEN + 1), port);
	g_tps6599x_prev_len[port] = TIPD_CMD_ST_ALT_MODE_LEN;
	memcpy(&g_tps6599x_prev_Data[port][0], &tmpBuf[1], TIPD_CMD_ST_ALT_MODE_LEN);

	/* 3. write 4-byte pCmd to CMD */
	if (p4CC) {
		memcpy(&tmpBuf[1], p4CC, TIPD_REG_CMD1_LEN);  /* both ports have some cmd length */
		tmpBuf[0] = TIPD_REG_CMD1_LEN;
		
		/* Save the currnet 4CC command */
		for (index = 0; index < 4; index++) {
			g_tps6599x_prev_4CC[port][index] = p4CC[index];
		}

		amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_CMD1, tmpBuf, (TIPD_REG_CMD1_LEN + 1), port);
	}
	g_4cc_pendingCmdType[port] = CMD_TYPE_ALT_MODE;
	
	_s_ucsi_retryTimeCnt[port] = DRV_TPS_RETRY_TIME_MAX;
	
	LOG_DBG("%s Send P:%d ", p4CC, port);
}

/**
 * amd_crb_drivers_tps6599x_cmdUcsi
 *
 * @param [in]     timeout;        time out for retry
 * @param [in]     port;           port for ti pd
 * @param [in]     ucsi_cmd;       ucsi 4cc command
 * @param [in]     ucsi_length;    ucsi length
 * @param [in]     detail;         ucsi detail
 *
 * @note
 *  send the ucsi command
 */
void amd_crb_drivers_tps6599x_cmdUcsi(uint32_t timeout, uint8_t port, uint8_t ucsi_cmd, uint8_t ucsi_length, ucsi_ctrl_details_t *detail)
{
	uint8_t tmpBuf[TIPD_CMD_ST_UCSI_LEN + 1] = {0}; /* add 1 for length byte */
	
	/* check port number */
	if (port > (NO_OF_TYPEC_PORTS - 1)) {
		/* return failed if port num upto 2 */
		return;
	}

	/* 1. CMD is 0 or '!CMD' */
	//amd_crb_drivers_tps6599x_wait4CmdDone(port, timeout);  /* not used for now */

	/* 2. write DataIn to DATA if (dataInLen != 0) */
	if (ucsi_cmd) {
		tmpBuf[0] = TIPD_CMD_ST_UCSI_LEN; /* length in first byte */
		tmpBuf[1] = ucsi_cmd;
		tmpBuf[2] = ucsi_length;
		memcpy(&tmpBuf[3], detail, (TIPD_CMD_ST_UCSI_LEN - 2));
		
		if (ucsi_cmd != UCSI_CMD_GET_ALTERNATE_MODES) { /* Get alt mode connector num is not in those bits */
			/* Force to add the connector number */
			tmpBuf[3] &= ~(0x7F);  /* Clear low 7-bits */
			
			if (port == TYPEC_PORT_2_IDX)
				tmpBuf[3] |= (0 + 1);  /* Force add the connector number. Port2 is relocate to port0 */
			else
				tmpBuf[3] |= (port + 1);  /* Force add the connector number */
		}
		else {
			tmpBuf[4] &= ~(0x7F);  /* Clear low 7-bits */
			
			if (port == TYPEC_PORT_2_IDX)
				tmpBuf[4] |= (0 + 1);  /* Force add the connector number. Port2 is relocate to port0 */
		}
	}
	
	LOG_DBG("UCSI Start P:%d cmd:%d ucsi_len:%d", port, ucsi_cmd,ucsi_length);

	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_DATA1, tmpBuf, (TIPD_CMD_ST_UCSI_LEN + 1), port);
	g_tps6599x_prev_len[port] = tmpBuf[0];
	memcpy(&g_tps6599x_prev_Data[port][0], &tmpBuf[1], TIPD_CMD_ST_UCSI_LEN);

	/* 3. write 4-byte pCmd to CMD */
	memcpy(&tmpBuf[1], TIPD_CMD_ST_UCSI, TIPD_REG_CMD1_LEN);  /* both ports have some cmd length */
	tmpBuf[0] = TIPD_REG_CMD1_LEN;
	
	/* Save the currnet 4CC command */
	memcpy(&g_tps6599x_prev_4CC[port][0], TIPD_CMD_ST_UCSI, TIPD_REG_CMD1_LEN);  /* both ports have some cmd length */

	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_CMD1, tmpBuf, (TIPD_REG_CMD1_LEN + 1), port);
	
	g_4cc_pendingCmdType[port] = CMD_TYPE_UCSI;
	
	_s_ucsi_retryTimeCnt[port] = DRV_TPS_RETRY_TIME_MAX;
	
	LOG_DBG("UCSI Send P:%d", port);
}


/**
 * amd_crb_drivers_tps6599x_portConfigStateMachineMatch
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     state_machine;  typec state machine
 *
 * @note
 *  check if target state machine match with register value
 */
bool amd_crb_drivers_tps6599x_portConfigStateMachineMatch(uint8_t port, port_role_t state_machine)
{
	DRV_TPS6599X_PORT_CONFIG status;
	
	amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_PORT_CONFIG, &status, sizeof(status), port);
	
	if (status.f.TypeCStateMachine != state_machine)
		return false;
	else
		return true;
}

#define TYPEC_CURRENT_DEF              (0)
#define TYPEC_CURRENT_1_5A             (1)
#define TYPEC_CURRENT_3A               (2)
#define TYPEC_CURRENT_RSEV             (3)
#define TYPEC_CURRENT_MASK             (3)

/**
 * amd_crb_drivers_tps6599x_portCtrlTypecCurrent
 *
 * @param [in]     port;           port for ti pd
 * @param [in]     cc_cur;         typec current
 *
 * @note
 *  write the typec current
 */
void amd_crb_drivers_tps6599x_portCtrlTypecCurrent(uint8_t port, dpm_typec_cmd_t cc_cur)
{
	DRV_TPS6599X_PORT_CONTROL status, status_set, status_clear;
	dpm_status_t *dpm_stat = dpm_set_status(port);
	
	for (uint8_t index = 0; index < DRV_TPS6599X_PORT_CONTROL_REGISTER_LENGTH; index++) {
		status_set.buf[index] = 0;
		status_clear.buf[index] = 0;
	}
	
	amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_PORT_CTL, &status, sizeof(status), port);
	
	LOG_DBG("Port Ctrl Cur:%d P:%d", cc_cur, port);
	
	switch (cc_cur) {
		case DPM_CMD_SET_RP_DFLT:
			status_set.f.TypeCCurrent = TYPEC_CURRENT_DEF; /* 00b for 900mA*/
			status_clear.f.TypeCCurrent = TYPEC_CURRENT_MASK; 
		
			amd_crb_drivers_tps6599x_portConfigPdDisable(port, 1); /* disable pd */
		
			if (status.f.TypeCCurrent != TYPEC_CURRENT_DEF) {
				amd_crb_drivers_tps6599x_portConfigStateMachine(port, PRT_NULL);
				amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
				k_sleep(K_MSEC(4));  /* delay 4ms */
				amd_crb_drivers_tps6599x_portConfigStateMachine(port, PRT_DUAL);
				k_sleep(K_MSEC(40));  /* delay 40ms */
			}
			dpm_stat->src_cur_level = UCSI_SRC_CUR_LEVEL_DEF;
			break;
		case DPM_CMD_SET_RP_1_5A:
			status_set.f.TypeCCurrent = TYPEC_CURRENT_1_5A; /* 01b for 1.5A*/
			status_clear.f.TypeCCurrent = TYPEC_CURRENT_MASK; 
		
			amd_crb_drivers_tps6599x_portConfigPdDisable(port, 1); /* disable pd */
		
			if (status.f.TypeCCurrent != DPM_CMD_SET_RP_1_5A) {
				amd_crb_drivers_tps6599x_portConfigStateMachine(port, PRT_NULL);
				amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
				k_sleep(K_MSEC(4));  /* delay 4ms */
				amd_crb_drivers_tps6599x_portConfigStateMachine(port, PRT_DUAL);
				k_sleep(K_MSEC(40));  /* delay 40ms */
			}
			dpm_stat->src_cur_level = UCSI_SRC_CUR_LEVEL_1_5A;
			break;
		case DPM_CMD_SET_RP_3A:
			status_set.f.TypeCCurrent = TYPEC_CURRENT_3A; /* 10b for 3A */
			status_clear.f.TypeCCurrent = TYPEC_CURRENT_MASK; 
		
			amd_crb_drivers_tps6599x_portConfigPdDisable(port, 0); /* enable pd */
		
			if (status.f.TypeCCurrent != TYPEC_CURRENT_3A) {
				amd_crb_drivers_tps6599x_portConfigStateMachine(port, PRT_NULL);
				amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
				k_sleep(K_MSEC(4));  /* delay 4ms */
				amd_crb_drivers_tps6599x_portConfigStateMachine(port, PRT_DUAL);
				k_sleep(K_MSEC(40));  /* delay 4ms */
			}
			dpm_stat->src_cur_level = UCSI_SRC_CUR_LEVEL_3A;
			break;
		default:
			break;
	}
	/* Read back current Rp */
	amd_crb_drivers_tps6599x_readTypecCurrent(port);
}

/**
 * amd_crb_drivers_tps6599x_portCtrlInitDrSwap
 *
 * @param [in]     port;               port for ti pd
 * @param [in]     targer_port_type;   new port type
 *
 * @note
 *  change the port type
 */
void amd_crb_drivers_tps6599x_portCtrlInitDrSwap(uint8_t port, port_type_t targer_port_type)
{
	DRV_TPS6599X_PORT_CONTROL status_set, status_clear;
	
	for (uint8_t index = 0; index < DRV_TPS6599X_PORT_CONTROL_REGISTER_LENGTH; index++) {
		status_set.buf[index] = 0;
		status_clear.buf[index] = 0;
	}
	
	LOG_DBG("Port Ctrl DR swap P:%d T:%d", port, targer_port_type);
	if (targer_port_type == PRT_TYPE_UFP) {
		status_set.f.InitiateSwapToUFP = 1;
	}
	else if (targer_port_type == PRT_TYPE_DFP) {
		status_set.f.InitiateSwapToDFP = 1;
	}

	amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
}

/**
 * amd_crb_drivers_tps6599x_portCtrlInitPrSwap
 *
 * @param [in]     port;               port for ti pd
 * @param [in]     targer_port_role;   new port role
 *
 * @note
 *  change the port role
 */
void amd_crb_drivers_tps6599x_portCtrlInitPrSwap(uint8_t port, port_role_t targer_port_role)
{
	DRV_TPS6599X_PORT_CONTROL status_set, status_clear;
	
	for (uint8_t index = 0; index < DRV_TPS6599X_PORT_CONTROL_REGISTER_LENGTH; index++) {
		status_set.buf[index] = 0;
		status_clear.buf[index] = 0;
	}
	
	LOG_DBG("Port Ctrl PR swap P:%d T:%d", port, targer_port_role);
	if (targer_port_role == PRT_ROLE_SINK) {
		status_set.f.InitiateSwapToSink = 1;
	}
	else if (targer_port_role == PRT_ROLE_SOURCE) {
		status_set.f.InitiateSwapToSource = 1;
	}

	amd_crb_drivers_tps6599x_portCtrl(port, status_set, status_clear);
}

/**
 * amd_crb_drivers_tps6599x_readCableVdo
 *
 * @param [in]     port;               port for ti pd
 *
 * @return 0 If fail.
 * @return cable vdo if successfully
 *
 * @note
 *  read cable vdo
 */
uint32_t amd_crb_drivers_tps6599x_readCableVdo(uint8_t port)
{
	int ret = 0;
	uint32_t cbl_vdo;
	DRV_TPS6599X_RX_VID_SOPP rx_vid_sopp;
	
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_RX_IDENTITY_SOPP, &rx_vid_sopp, sizeof(rx_vid_sopp), port);

	if (ret == 0) {
		/* byte-1 status; VDO1-4bytes-Header; VDO2-4bytes-Cert; VDO3-4bytes-Pro; VDO4-4bytes-Cable;  */
		cbl_vdo = (uint32_t)(rx_vid_sopp.f.RxIdSoppVdo4);
		LOG_DBG("cbl vod:%d P:%d", cbl_vdo, port);
		return cbl_vdo; /* success get the resp */
	}
	else
		return 0;
}

/**
 * amd_crb_drivers_tps6599x_readRxVidSop
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  read rx vid
 */
void amd_crb_drivers_tps6599x_readRxVidSop(uint8_t port)
{
	int ret = 0;
	DRV_TPS6599X_RX_VID_SOP rx_vid_sop;
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);
	pd_do_t ID_Header, Product_VDO, UFP_VDO;
	
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_RX_IDENTITY_SOP, &rx_vid_sop, sizeof(rx_vid_sop), port);

	if (ret == 0) {
		/* byte-1 status; VDO1-4bytes-ID-Header; VDO2-4bytes-Cert; VDO3-4bytes-Pro; VDO4-4bytes-other;  */
		ID_Header.val = (uint32_t)(rx_vid_sop.f.RxIdSoppVdo1);
		Product_VDO.val = (uint32_t)(rx_vid_sop.f.RxIdSoppVdo3);
		UFP_VDO.val = (uint32_t)(rx_vid_sop.f.RxIdSoppVdo4);
		
		if (ID_Header.std_id_hdr.mod_support == true) {
			dpm_stat->modal_op_support = true;
		}
		else {
			dpm_stat->modal_op_support = false;
		}
		
		/* dev_cap bit0~3 -> USB2 USB2_Billboard USB3 and USB4 */
		if (UFP_VDO.std_ufp_vdo.dev_cap & 0x08) {
			/* Check if it support USB4 as device */
			dpm_stat->usb4_supp_vid = true;
		}
		
		/* Detect SAMSUNG 2K monitor */
		if ((ID_Header.std_id_hdr.usb_vid == 0xBDA) && (Product_VDO.std_prod_vdo.bcd_dev == 0x121)) {
			/* Send the pr swap */
			Samsung_2K_detect[port] = 80;
		}
		LOG_DBG("TX_VID sop P:%d modal_op_support:%d", port, dpm_stat->modal_op_support);
	}
}

/**
 * amd_crb_drivers_tps6599x_readRxSvidSop
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  read rx svid
 */
void amd_crb_drivers_tps6599x_readRxSvidSop(uint8_t port)
{
	int ret = 0;
	DRV_TPS6599X_RX_SVID_SOP rx_svid_sop;
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);
	uint32_t svid[2];
	
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_DIS_SVID, &rx_svid_sop, sizeof(rx_svid_sop), port);

	if (ret == 0) {
		/* byte-1 status; VDO1-4bytes-ID-Header; VDO2-4bytes-Cert; VDO3-4bytes-Pro; VDO4-4bytes-other;  */
		svid[0] = (uint32_t)(rx_svid_sop.f.SvidsSop0);
		svid[1] = (uint32_t)(rx_svid_sop.f.SvidsSop1);
		
		if(((uint16_t)svid[0] == INTEL_VID) ||
			 ((svid[0] >> 16) == INTEL_VID)) {
			dpm_stat->tbt3_supp_svid = true;
			LOG_DBG("TBT3 support P:%d", port);
		}
		
		LOG_DBG("TX_SVID sop P%d: %x and %x", port, svid[0], svid[1]);
	}
}

/**
 * amd_crb_drivers_tps6599x_readRxVidSopp
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  read cable rx vid
 */
void amd_crb_drivers_tps6599x_readRxVidSopp(uint8_t port)
{
	int ret = 0;
	DRV_TPS6599X_RX_VID_SOPP rx_vid_sopp;
	pd_do_t ID_Header;
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status(port);
	
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_RX_IDENTITY_SOPP, &rx_vid_sopp, sizeof(rx_vid_sopp), port);

	if (ret == 0) {
		/* byte-1 status; VDO1-4bytes-ID-Header; VDO2-4bytes-Cert; VDO3-4bytes-Pro; VDO4-4bytes-Cable;  */
		dpm_stat->cbl_vdo.val = (uint32_t)(rx_vid_sopp.f.RxIdSoppVdo4);
		ID_Header.val = (uint32_t)(rx_vid_sopp.f.RxIdSoppVdo1);
		if((ID_Header.std_id_hdr.prod_type == PROD_TYPE_PAS_CBL) ||
			 (ID_Header.std_id_hdr.prod_type == PROD_TYPE_ACT_CBL)) {
			dpm_stat->emca_present = true; 
			dpm_stat->cbl_mode_en = ID_Header.std_id_hdr.mod_support;
		}
		else {
			dpm_stat->emca_present = false; 
			dpm_stat->cbl_mode_en = false;
		}
		
		LOG_DBG("TX_VID sopp P:%d", port);
	}
}

/**
 * amd_crb_drivers_tps6599x_readVdmInfo
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  read cable vdm information
 */
void amd_crb_drivers_tps6599x_readVdmInfo(uint8_t port)
{
	int ret = 0;
	DRV_TPS6599X_RX_VDM vdm_info;
	pd_do_t VDM_Header;
	
	/* Only need to read 5 bytes (1 status and 4 vdo) to detect the vdm command id (also need to read the first length so totally 6 types) */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_RX_VDM, &vdm_info, 6, port);
	
	if (ret == 0) {
		/* byte-1 status; VDO1-4bytes-VDM-Header; VDO2-4bytes-ID-Header; VDO3-4bytes-Cert; VDO4-4bytes-Pro; VDO5-4bytes-Cable;  */

		VDM_Header.val = (uint32_t)(vdm_info.f.RxVdmdo1);
		
		/* Only process the structure VMD and with ack*/
		if ((VDM_Header.std_vdm_hdr.vdm_type != VDM_TYPE_STRUCTURED) ||
			  (VDM_Header.std_vdm_hdr.cmd_type != CMD_TYPE_RESP_ACK))
			return;
		
		/* As interrupt mode + loop processing, we may miss the vid vdm event and triggered by further vdm */
		if(VDM_Header.std_vdm_hdr.cmd == VDM_CMD_DSC_IDENTITY) {
			LOG_DBG("Load the vid P:%d", port);
			amd_crb_drivers_tps6599x_readRxVidSop(port);
			amd_crb_drivers_tps6599x_readRxVidSopp(port);
			
		}
		else if (VDM_Header.std_vdm_hdr.cmd == VDM_CMD_DSC_SVIDS) {
			LOG_DBG("Load the svid P:%x", port);
			amd_crb_drivers_tps6599x_readRxSvidSop(port);
		}
		/* Save the alt mode id when get enter mode ack */
		else if (VDM_Header.std_vdm_hdr.cmd == VDM_CMD_ENTER_MODE) {
			LOG_DBG("Load enter mode P:%d svid:%x ", port, VDM_Header.std_vdm_hdr.svid);
			
			set_cur_alt_mode_id(port,VDM_Header.std_vdm_hdr.svid);
			
			LOG_DBG("Load the svid P:%x", port);
			amd_crb_drivers_tps6599x_readRxSvidSop(port);
		}
	}
}

/**
 * amd_crb_drivers_tps6599x_altModeInfo
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  read alt mode information
 */
void amd_crb_drivers_tps6599x_altModeInfo(uint8_t port)
{
	uint8_t altmode_num = 0;
	
	LOG_DBG("Get_ALTMODE_Seq P:%d", port);
	
	/* There is no reg to read the supproted ALT MODE svid.
     Need to check with TI. Hard code for now	*/
		
	/* only add DP svid 0xFF01 and mode 1 into buffer */
	set_alt_mode_info(port, 0, DP_SVID, 1);
	altmode_num++;
	
	/* Save the supported alt mode num */
	set_alt_mode_number(port, altmode_num);
}

/**
 * amd_crb_drivers_tps6599x_intEvent2Clear
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  clear the intevent2 bit
 */
void amd_crb_drivers_tps6599x_intEvent2Clear(uint8_t port)
{
	DRV_TPS6599X_DATA_CTRL data_ctrl;
	int ret = 0;
	
	LOG_DBG("Clear_Event2 P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_DATA_CTL, &data_ctrl, sizeof(data_ctrl), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		data_ctrl.f.InterruptAck = 1; /* Clear all interrupt bits in intEvent2 */
	}
	
	/* Write back the new value */
	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_DATA_CTL, &data_ctrl, sizeof(data_ctrl), port);
}

/**
 * amd_crb_drivers_tps6599x_partnerSrcCapInfo
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  get partner source capability information
 */
void amd_crb_drivers_tps6599x_partnerSrcCapInfo(uint8_t port)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	DRV_TPS6599X_RX_SRC_CAP src_cap;
	int ret = 0;
	
	LOG_DBG("Get_partner_SrcCap P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_RX_SRC_CAP, &src_cap, sizeof(src_cap), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		dpm_stat->partner_src_pdo_count = src_cap.f.RxSourceNumValidPdos; /* it should be bank1 */
		dpm_stat->partner_src_pdo[0].val = src_cap.f.RxSourcePdo1;
		dpm_stat->partner_src_pdo[1].val = src_cap.f.RxSourcePdo2;
		dpm_stat->partner_src_pdo[2].val = src_cap.f.RxSourcePdo3;
		dpm_stat->partner_src_pdo[3].val = src_cap.f.RxSourcePdo4;
		dpm_stat->partner_src_pdo[4].val = src_cap.f.RxSourcePdo5;
		dpm_stat->partner_src_pdo[5].val = src_cap.f.RxSourcePdo6;
		dpm_stat->partner_src_pdo[6].val = src_cap.f.RxSourcePdo7;
	}
	
	LOG_DBG("SrcCapNum:%d P:%d", dpm_stat->partner_src_pdo_count, port);
}

/**
 * amd_crb_drivers_tps6599x_partnerSnkCapInfo
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  get partner sink capability information
 */
void amd_crb_drivers_tps6599x_partnerSnkCapInfo(uint8_t port)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	DRV_TPS6599X_RX_SNK_CAP snk_cap;
	uint32_t ret = 0;
	
	LOG_DBG("Get_partner_SnkCap P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_RX_SINK_CAP, &snk_cap, sizeof(snk_cap), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		dpm_stat->partner_snk_pdo_count = snk_cap.f.numValidPdos; /* it should be bank1 */
		dpm_stat->partner_snk_pdo[0].val = snk_cap.f.RxSinkPdo1;
		dpm_stat->partner_snk_pdo[1].val = snk_cap.f.RxSinkPdo2;
		dpm_stat->partner_snk_pdo[2].val = snk_cap.f.RxSinkPdo3;
		dpm_stat->partner_snk_pdo[3].val = snk_cap.f.RxSinkPdo4;
		dpm_stat->partner_snk_pdo[4].val = snk_cap.f.RxSinkPdo5;
		dpm_stat->partner_snk_pdo[5].val = snk_cap.f.RxSinkPdo6;
		dpm_stat->partner_snk_pdo[6].val = snk_cap.f.RxSinkPdo7;
	}
	
	LOG_DBG("SnkCapNum:%d P:%d", dpm_stat->partner_snk_pdo_count, port);
}

/**
 * amd_crb_drivers_tps6599x_devSrcCapInfo
 *
 * @param [in]     port;               port for ti pd
 * @param [in]     deft;               default source capability
 *
 * @note
 *  get device source capability information
 */
void amd_crb_drivers_tps6599x_devSrcCapInfo(uint8_t port, uint8_t deft)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	DRV_TPS6599X_TX_SRC_CAP src_cap;
	uint32_t ret = 0;
	uint8_t i;
	
	LOG_DBG("Get_dev_SrcCap P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_SRC_CAP, &src_cap, sizeof(src_cap), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		dpm_stat->src_pdo_count = src_cap.f.numValidPdos;
		dpm_stat->src_pdo[0].val = src_cap.f.TXSourcePdo1;
		dpm_stat->src_pdo[1].val = src_cap.f.TXSourcePdo2;
		dpm_stat->src_pdo[2].val = src_cap.f.TXSourcePdo3;
		dpm_stat->src_pdo[3].val = src_cap.f.TXSourcePdo4;
		dpm_stat->src_pdo[4].val = src_cap.f.TXSourcePdo5;
		dpm_stat->src_pdo[5].val = src_cap.f.TXSourcePdo6;
		dpm_stat->src_pdo[6].val = src_cap.f.TXSourcePdo7;
	}
	
	dpm_stat->src_pdo_mask = 0; /* Mask for 7 bits */
	for (i = 0; i < dpm_stat->src_pdo_count; i++) {
		dpm_stat->src_pdo_mask |= (1 << i);
	}
	
	if (deft) {
		for (i = 0; i < MAX_NO_OF_PDO; i++) {
			dpm_stat->src_pdo_deft[i].val = dpm_stat->src_pdo[i].val;
		}
	}
	
	LOG_DBG("SrcCapNum:%d P:%d", dpm_stat->src_pdo_count, port);
}

/**
 * amd_crb_drivers_tps6599x_refreashDevSrcCapCurrent
 *
 * @param [in]     port;               port for ti pd
 *
 * @note
 *  refresh device source capability information
 */
void amd_crb_drivers_tps6599x_refreashDevSrcCapCurrent(uint8_t port)
{
    dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
    uint32_t i, count;

    /* Loop through all PDOs to update current/power as per cable capabilities. */
    for (i = 0, count = 0; i < (dpm_stat->src_pdo_count); i++) {
        /* Check if PDO's mask is enabled for this PDO. */
        if ((dpm_stat->src_pdo_mask & (0x01 << i)) != 0) {
            dpm_stat->cur_src_pdo[count] = dpm_stat->src_pdo[i];
            count++;
        }
    }

    /* Update the fields required for PDO 0. */
    dpm_stat->cur_src_pdo[0].fixed_src.ext_powered = ((dpm_stat->src_pdo_mask) >> PD_EXTERNALLY_POWERED_BIT_POS);


#if PD_REV3_ENABLE
		dpm_stat->cur_src_pdo[0].val &= PD_FIX_SRC_PDO_MASK_REV3;
		/* Always set the unchunked extended message support bit in PD3 connections. */
		dpm_stat->cur_src_pdo[0].fixed_src.unchunk_sup = 1;
#else
		dpm_stat->cur_src_pdo[0].val &= PD_FIX_SRC_PDO_MASK_REV2;
#endif /* PD_REV3_ENABLE */

    dpm_stat->cur_src_pdo_count = count;
}

/**
 * amd_crb_drivers_tps6599x_devSrcCapUpdate
 *
 * @param [in]     port;        port for ti pd
 * @param [in]     mask;        bit-0 for PDO0 ... if set means this PDO will be used. Otherwise bypass it.
 * @param [in]     pdos;        pdos pointer
 *
 * @note
 *  udpate device source capability
 */
void amd_crb_drivers_tps6599x_devSrcCapUpdate(uint8_t port, uint8_t mask, pd_do_t* pdos)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	DRV_TPS6599X_TX_SRC_CAP src_cap;
	int ret = 0;
	uint8_t index = 0, len = 0;
	uint32_t tmpBuf[MAX_NO_OF_PDO] = {0};
	
	LOG_DBG("Update_dev_SrcCap P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_SRC_CAP, &src_cap, sizeof(src_cap), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		for (index = 0, src_cap.f.numValidPdos = 0; index < dpm_stat->src_pdo_count; index++) {
			if ((mask & (0x01 << index)) != 0) {
				tmpBuf[src_cap.f.numValidPdos] = pdos[index].val;
				src_cap.f.numValidPdos++;
			}
		}
		src_cap.f.TXSourcePdo1 = tmpBuf[0];
		src_cap.f.TXSourcePdo2 = tmpBuf[1];
		src_cap.f.TXSourcePdo3 = tmpBuf[2];
		src_cap.f.TXSourcePdo4 = tmpBuf[3];
		src_cap.f.TXSourcePdo5 = tmpBuf[4];
		src_cap.f.TXSourcePdo6 = tmpBuf[5];
		src_cap.f.TXSourcePdo7 = tmpBuf[6];
		
		len = src_cap.f.numValidPdos;
		
		/* update the src cap back  */
		ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_TX_SRC_CAP, &src_cap, sizeof(src_cap), port);
		
		/* update the new mask */
		dpm_stat->src_pdo_mask = mask;
		/* refrash the current src cap based on mask */
		dpm_stat->cur_src_pdo_count = len;
		for (index = 0; index < dpm_stat->cur_src_pdo_count; index++) {
			dpm_stat->cur_src_pdo[index].val = tmpBuf[index];
		}
	}
}

/**
 * amd_crb_drivers_tps6599x_devSrcCapDeft
 *
 * @param [in]     port;        port for ti pd
 *
 * @note
 *  return the default device source capability
 */
void amd_crb_drivers_tps6599x_devSrcCapDeft(uint8_t port)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	DRV_TPS6599X_TX_SRC_CAP src_cap;
	uint32_t ret = 0;
	uint8_t index = 0, len = 0;
	
	LOG_DBG("Update_dev_SrcCap_deft P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_SRC_CAP, &src_cap, sizeof(src_cap), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		src_cap.f.numValidPdos = dpm_stat->src_pdo_count;
		
		src_cap.f.TXSourcePdo1 = dpm_stat->src_pdo_deft[0].val;
		src_cap.f.TXSourcePdo2 = dpm_stat->src_pdo_deft[1].val;
		src_cap.f.TXSourcePdo3 = dpm_stat->src_pdo_deft[2].val;
		src_cap.f.TXSourcePdo4 = dpm_stat->src_pdo_deft[3].val;
		src_cap.f.TXSourcePdo5 = dpm_stat->src_pdo_deft[4].val;
		src_cap.f.TXSourcePdo6 = dpm_stat->src_pdo_deft[5].val;
		src_cap.f.TXSourcePdo7 = dpm_stat->src_pdo_deft[6].val;
		
		len = src_cap.f.numValidPdos;
		
		/* update the src cap back  */
		ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_TX_SRC_CAP, &src_cap, sizeof(src_cap), port);
		
		/* refrash the current src cap based on mask */
		dpm_stat->src_pdo_count = len;
		for (index = 0; index < MAX_NO_OF_PDO; index++) {
			dpm_stat->src_pdo[index].val = dpm_stat->src_pdo_deft[index].val;
		}
		amd_crb_drivers_tps6599x_refreashDevSrcCapCurrent(port);
	}
}

/**
 * amd_crb_drivers_tps6599x_devSnkCapInfo
 *
 * @param [in]     port;        port for ti pd
 * @param [in]     deft;        deft sink cap
 *
 * @note
 *  return to the default sink capability
 */
void amd_crb_drivers_tps6599x_devSnkCapInfo(uint8_t port, uint8_t deft)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	DRV_TPS6599X_TX_SNK_CAP snk_cap;
	int ret = 0;
	uint8_t i;
	
	LOG_DBG("Get_dev_SnkCap P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_SNK_CAP, &snk_cap, sizeof(snk_cap), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		dpm_stat->snk_pdo_count = snk_cap.f.numValidPdos;
		dpm_stat->snk_pdo[0].val = snk_cap.f.TxSinkPdo1;
		dpm_stat->snk_pdo[1].val = snk_cap.f.TxSinkPdo2;
		dpm_stat->snk_pdo[2].val = snk_cap.f.TxSinkPdo3;
		dpm_stat->snk_pdo[3].val = snk_cap.f.TxSinkPdo4;
		dpm_stat->snk_pdo[4].val = snk_cap.f.TxSinkPdo5;
		dpm_stat->snk_pdo[5].val = snk_cap.f.TxSinkPdo6;
		dpm_stat->snk_pdo[6].val = snk_cap.f.TxSinkPdo7;
	}
	
	/* reconfigure the AskForMax */
	snk_cap.f.TxSinkPdo1 = dpm_stat->snk_pdo[0].val;
	snk_cap.f.TxSinkPdo2 = dpm_stat->snk_pdo[1].val;
	
	/* update the snk cap back  */
	amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_TX_SNK_CAP, &snk_cap, sizeof(snk_cap), port);

	dpm_stat->snk_pdo_mask = 0;
	for (i = 0; i < dpm_stat->snk_pdo_count; i++) {
		dpm_stat->snk_pdo_mask |= (1 << i);
	}
	
	if (deft) {
		for (i = 0; i < MAX_NO_OF_PDO; i++) {
			dpm_stat->snk_pdo_deft[i].val = dpm_stat->snk_pdo[i].val;
		}
	}
	
	LOG_DBG("SnkCapNum:%d P:%d", dpm_stat->snk_pdo_count, port);
}

/**
 * amd_crb_drivers_tps6599x_refreashDevSnkCapCurrent
 *
 * @param [in]     port;        port for ti pd
 *
 * @note
 *  refresh the device sink capability
 */
void amd_crb_drivers_tps6599x_refreashDevSnkCapCurrent(uint8_t port)
{
    uint32_t i, count;
    dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);

    /* Loop through all PDOs to update current/power */
    for (i = 0, count = 0; i < (dpm_stat->snk_pdo_count); i++) {
        /* Check if PDOs mask is enabled for this PDO. */
        if ((dpm_stat->snk_pdo_mask & (0x01 << i)) != 0) {
            dpm_stat->cur_snk_pdo[count] = dpm_stat->snk_pdo[i];
            count++;
        }
    }

    /* Update the flags required in PDO 0. */
    dpm_stat->cur_snk_pdo[0].fixed_snk.ext_powered = ((dpm_stat->snk_pdo_mask) >> PD_EXTERNALLY_POWERED_BIT_POS);
    dpm_stat->cur_snk_pdo_count = count;
}

/**
 * amd_crb_drivers_tps6599x_devSnkCapUpdate
 *
 * @param [in]     port;        port for ti pd
 * @param [in]     mask;        mask bit to the updated pdo
 * @param [in]     pdos;        pdos pointer
 *
 * @note
 *  update sink capability pdo
 */
void amd_crb_drivers_tps6599x_devSnkCapUpdate(uint8_t port, uint8_t mask, pd_do_t* pdos)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	DRV_TPS6599X_TX_SNK_CAP snk_cap;
	int ret = 0;
	uint8_t index = 0;
	uint32_t tmpBuf[MAX_NO_OF_PDO] = {0};
	
	LOG_DBG("Update_dev_SnkCap P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_SNK_CAP, &snk_cap, sizeof(snk_cap), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		for (index = 0, snk_cap.f.numValidPdos = 0; index < dpm_stat->snk_pdo_count; index++) {
			if ((mask & (0x01 << index)) != 0) {
				tmpBuf[snk_cap.f.numValidPdos] = pdos[index].val;
				snk_cap.f.numValidPdos++;
			}
		}
		snk_cap.f.TxSinkPdo1 = tmpBuf[0];
		snk_cap.f.TxSinkPdo2 = tmpBuf[1];
		snk_cap.f.TxSinkPdo3 = tmpBuf[2];
		snk_cap.f.TxSinkPdo4 = tmpBuf[3];
		snk_cap.f.TxSinkPdo5 = tmpBuf[4];
		snk_cap.f.TxSinkPdo6 = tmpBuf[5];
		snk_cap.f.TxSinkPdo7 = tmpBuf[6];
		
		/* update the src cap back  */
		ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_TX_SNK_CAP, &snk_cap, sizeof(snk_cap), port);
		
		/* update the new mask */
		dpm_stat->snk_pdo_mask = mask;
		/* refrash the current src cap based on mask */
		dpm_stat->cur_snk_pdo_count = snk_cap.f.numValidPdos;
		for (index = 0; index < dpm_stat->cur_snk_pdo_count; index++) {
			dpm_stat->cur_snk_pdo[index].val = tmpBuf[index];
		}
	}
}

/**
 * amd_crb_drivers_tps6599x_devSnkCapDeft
 *
 * @param [in]     port;        port for ti pd
 *
 * @note
 *  return the default sink capability
 */
void amd_crb_drivers_tps6599x_devSnkCapDeft(uint8_t port)
{
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
	DRV_TPS6599X_TX_SNK_CAP snk_cap;
	int ret = 0;
	uint8_t index = 0;
	
	LOG_DBG("Update_dev_SnkCap_deft P:%d", port);
	
	/* Read the register */
	ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_READ, TIPD_REG_TX_SNK_CAP, &snk_cap, sizeof(snk_cap), port);
		
	/* Max pdo numbers are 7 */
	if (ret == 0) {
		snk_cap.f.numValidPdos = dpm_stat->snk_pdo_count;

		snk_cap.f.TxSinkPdo1 = dpm_stat->snk_pdo_deft[0].val;
		snk_cap.f.TxSinkPdo2 = dpm_stat->snk_pdo_deft[1].val;
		snk_cap.f.TxSinkPdo3 = dpm_stat->snk_pdo_deft[2].val;
		snk_cap.f.TxSinkPdo4 = dpm_stat->snk_pdo_deft[3].val;
		snk_cap.f.TxSinkPdo5 = dpm_stat->snk_pdo_deft[4].val;
		snk_cap.f.TxSinkPdo6 = dpm_stat->snk_pdo_deft[5].val;
		snk_cap.f.TxSinkPdo7 = dpm_stat->snk_pdo_deft[6].val;

		/* update the src cap back  */
		ret = amd_crb_drivers_tps6599x_regAccess(TI_REG_WRITE, TIPD_REG_TX_SNK_CAP, &snk_cap, sizeof(snk_cap), port);
		
		/* refrash the current src cap based on mask */
		dpm_stat->snk_pdo_count = snk_cap.f.numValidPdos;
		for (index = 0; index < MAX_NO_OF_PDO; index++) {
			dpm_stat->snk_pdo[index].val = dpm_stat->snk_pdo_deft[index].val;
		}
		amd_crb_drivers_tps6599x_refreashDevSnkCapCurrent(port);
	}
}

#if UCSI_SET_PWR_LEVEL_ENABLE   
#define PWR_LVL_4P5W                    (9)         /* 4.5W in 0.5W units */
#define PWR_LVL_4P0W                    (8)         /* 4W in 0.5W units */    
#define MIN_CURRENT                     (90)        /* 900mA in 10mA units */

/**
 * @brief Edit the advertised PDOs to match the OS's requirements
 *
 * @param port The port whose PDOs need editing
 * @param power Final power level that the OS wants it to get to
 * @param is_src Set to '1' if the Source Caps need editing. For Sink Caps, its set to '0'
 * @param &out_mask An output variable indicating a bitmask of enabled PDOs
 *
 * @return 
 *    NULL If it should fail the command by setting the Error Indicator
 *    pd_do_t[7] A 7-element PD data object array of edited PDOs
 */
pd_do_t* amd_crb_drivers_ucsi_changePdoPower(uint8_t port, uint8_t power, uint8_t is_src, uint8_t *out_mask)
{
    uint8_t i, pdo_cnt;
    uint32_t cur_power;
    pd_do_t* cur_pdo;
    static pd_do_t pdos[MAX_NO_OF_PDO];
	dpm_status_t *dpm_stat = (dpm_status_t *)dpm_set_status (port);
		
	LOG_DBG("Change_PDO P:%d", port);
    
    /* Always enable 5V */
    *out_mask = 1;

	if(is_src == 1) {
		/* Take the number of PDOs and the list from the default table (copy when pd controller boot up) */
		pdo_cnt = dpm_stat->src_pdo_count;
		cur_pdo = dpm_stat->src_pdo;
		/* get the PDO info */

		for(i = 0; i < pdo_cnt; i++) {
			LOG_DBG("SrPDO1:%x %x", cur_pdo[i].val, dpm_stat->src_pdo[i].val);
			
			pdos[i].val = cur_pdo[i].val;

			/* If the OPM sets PD power level to 0 or 255, we revert to the default configuration */
			if((power == 0) || (power == 0xFF)) {
				dpm_stat->src_pdo[i].val = dpm_stat->src_pdo_deft[i].val;
				*out_mask |= (1 << i);
			}
			else {
				switch(pdos[i].fixed_src.supply_type) {
					case PDO_FIXED_SUPPLY:
						cur_power = calc_power(pdos[i].fixed_src.voltage, pdos[i].fixed_src.max_current);
						/* Force the max current to be within 900mA and 3A */
						pdos[i].fixed_src.max_current = GET_MAX(MIN_CURRENT, GET_MIN((2 * power * pdos[i].fixed_src.max_current) / cur_power, CBL_CAP_3A));

						/* Enable other PDOs only if requested power is >4.5W */
						if(power > PWR_LVL_4P5W)
							*out_mask |= (1 << i);
						break;

					case PDO_VARIABLE_SUPPLY:
						cur_power = calc_power(pdos[i].var_src.max_voltage, pdos[i].var_src.max_current);
						pdos[i].var_src.max_current = GET_MAX(MIN_CURRENT, GET_MIN((2 * power * pdos[i].var_src.max_current) / cur_power, CBL_CAP_3A));
						*out_mask |= (1 << i);
						break;

					case PDO_BATTERY:
						pdos[i].bat_src.max_power = power * 2;
						*out_mask |= (1 << i);
						break;
					}
			}
			/* Copy back the pdo value */
			cur_pdo[i].val = pdos[i].val;

			LOG_DBG("SrPDO2:%x %x", cur_pdo[i].val, dpm_stat->src_pdo[i].val);
		}
		LOG_DBG("NewSrc PDO mask:%d P:%d", *out_mask, port);
	}
	else
	{
		/* Take the number of PDOs and the list from the default table (copy when pd controller boot up) */
		pdo_cnt = dpm_stat->snk_pdo_count;
		cur_pdo = dpm_stat->snk_pdo;
		/* get the PDO info */

		for(i = 0; i < pdo_cnt; i++) {
			pdos[i].val = cur_pdo[i].val;
			LOG_DBG("SnPDO1:%x %x", cur_pdo[i].val, dpm_stat->snk_pdo[i].val);

			/* If the OPM sets PD power level to 0 or 255, we revert to the default configuration */
			if((power == 0) || (power == 0xFF)) {
				dpm_stat->snk_pdo[i].val = dpm_stat->snk_pdo_deft[i].val;
				*out_mask |= (1 << i);
			}
			else {
				switch(pdos[i].fixed_snk.supply_type) {
					case PDO_FIXED_SUPPLY:
						cur_power = calc_power(pdos[i].fixed_snk.voltage, pdos[i].fixed_snk.op_current);
						/* Force the max current to be within 900mA and 3A */
						pdos[i].fixed_snk.op_current = GET_MAX(MIN_CURRENT, GET_MIN((2 * power * pdos[i].fixed_snk.op_current) / cur_power, CBL_CAP_3A));

						/* Enable this pdo only when it below threshold*/
						cur_power = calc_power(pdos[i].fixed_snk.voltage, pdos[i].fixed_snk.op_current);
						if((power * 2) >= cur_power)
							*out_mask |= (1 << i);
						break;

					case PDO_VARIABLE_SUPPLY:
						cur_power = calc_power(pdos[i].var_snk.max_voltage, pdos[i].var_snk.op_current);
						pdos[i].var_snk.op_current = GET_MAX(MIN_CURRENT, GET_MIN((2 * power * pdos[i].var_snk.op_current) / cur_power, CBL_CAP_3A));

						/* Enable this pdo only when it below threshold*/
						cur_power = calc_power(pdos[i].var_snk.max_voltage, pdos[i].fixed_snk.op_current);
						if((power * 2) >= cur_power)
							*out_mask |= (1 << i);
						break;

					case PDO_BATTERY:
						pdos[i].bat_snk.op_power = power * 2;
						*out_mask |= (1 << i);
						break;
					}
				}
				/* Copy back the pdo value */
				cur_pdo[i].val = pdos[i].val;

				LOG_DBG("SnPDO2:%x %x", cur_pdo[i].val, dpm_stat->snk_pdo[i].val);
		}
		LOG_DBG("NewSnk PDO mask:%d P:%d", *out_mask, port);
	}
    return pdos;
}
#endif /*UCSI_SET_PWR_LEVEL_ENABLE*/
#endif /* CONFIG_EC_UCSI_ENABLE */



