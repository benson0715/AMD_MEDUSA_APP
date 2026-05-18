/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <assert.h>
#include <soc.h>
#include "i2c_hub.h"
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "board_config.h"
#include "f_nv_options.h"
#include "board_id.h"
#include "dev_sbtsi.h"
#include "app_acpi.h"
#include "dev_tmp432.h"
#include "dev_lm95234.h"
#include "board_thermal.h"
#include "espi_hub.h"
LOG_MODULE_REGISTER(board_thermal, CONFIG_BOARD_THERMAL_LOG_LEVEL);

/*
 * APML will be disabled aftr APML_ERROR_NUM_THRESHOLD continues fail.
 * The counter g_u32ApmlErrCnt is reset by APU_RST.
 *
 * APML mailbox requests will be hold for APML_MB_DELAY_SECOND second after APU_RST
 */
uint32_t g_u32ApmlErrCnt = 0;

/* ************************** *
 *     global valuable        *
 * ************************** */
uint16_t _s_u16pcbTmp, _s_u16pcbTmpQ1 /* Q87 */, _s_u16pcbTmpQ2	/* Q47 */;
uint16_t _s_u16pcbTmp2, _s_u16pcbTmpQ21, _s_u16pcbTmpQ22, _s_u16pcbTmpQ23, _s_u16pcbTmpQ24;

uint16_t _s_u16dieTmp;
uint16_t _s_u16evalSensor;

/************************************
 *       Eval card sensor           *
 ************************************/
#define CMD_LD_ADDR 0x01
#define CMD_WR_DATA 0x02
#define CMD_RD_DATA 0x03

/**
 * board_thermal_info 
 *
 * @param [in]   BoardTmp;
 *
 * @note
 *  Get board thermal information
 */
void board_thermal_getinfo (board_thermal_info * BoardTmp)
{
	//
	// Read on-board sensors
	//
	BoardTmp->_s_u16pcbTmp = dev_tmp432_readTemp(DEV_TMP432_SENSOR__LOCAL);
	BoardTmp->_s_u16pcbTmpQ[0] = dev_tmp432_readTemp(DEV_TMP432_SENSOR__REMOTE1);
	BoardTmp->_s_u16pcbTmpQ[1] = dev_tmp432_readTemp(DEV_TMP432_SENSOR__REMOTE2);
	BoardTmp->_s_u16pcbTmpQMax = 2;
	_s_u16pcbTmp = BoardTmp->_s_u16pcbTmp;
	_s_u16pcbTmpQ1 = BoardTmp->_s_u16pcbTmpQ[0];
	_s_u16pcbTmpQ2 = BoardTmp->_s_u16pcbTmpQ[1];
	//
	// Read on-board sensors
	//
	BoardTmp->_s_u16pcbTmp2 = dev_lm95234_read_temp(DEV_LM95234_SENSOR_LOCAL, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQ2[0] = dev_lm95234_read_temp(DEV_LM95234_SENSOR_REMOT1, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQ2[1] = dev_lm95234_read_temp(DEV_LM95234_SENSOR_REMOT2, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQ2[2] = dev_lm95234_read_temp(DEV_LM95234_SENSOR_REMOT3, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	BoardTmp->_s_u16pcbTmpQ2[3] = dev_lm95234_read_temp(DEV_LM95234_SENSOR_REMOT4, LM95234_I2C_PORT, LM95234_I2C_ADDRESS);
	_s_u16pcbTmp2 = BoardTmp->_s_u16pcbTmp2;
	_s_u16pcbTmpQ21 = BoardTmp->_s_u16pcbTmpQ2[0];
	_s_u16pcbTmpQ22 = BoardTmp->_s_u16pcbTmpQ2[1];
	_s_u16pcbTmpQ23 = BoardTmp->_s_u16pcbTmpQ2[2];
	_s_u16pcbTmpQ24 = BoardTmp->_s_u16pcbTmpQ2[3];
	/* Skip SB-TSI in Zx */
	if (!espihub_socInLP()) {
		_s_u16dieTmp = dev_sbtsi_getTemp16();
	}

	LOG_DBG("Die: %2d.%03d, Charger %2d.%03d, APU %2d.%03d, IO %2d.%03d",
			_GET_INT_FROM_U16(_s_u16dieTmp), (int)_GET_DEC_FROM_U16(_s_u16dieTmp),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmp), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmp),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ[0]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ[0]),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ[1]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ[1]));

	LOG_DBG("Die: %2d.%03d, LP5x %2d.%03d, VR %2d.%03d, Charger %2d.%03d, SSD0 %2d.%03d",
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmp2), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmp2),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[0]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[0]),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[1]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[1]),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[2]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[2]),
			_GET_INT_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[3]), (int)_GET_DEC_FROM_U16(BoardTmp->_s_u16pcbTmpQ2[3]));
}

/**
 * SetThemalResistanceCorrection 
 *
 * @param [in]   BoardTmp;
 *
 * @note
 *  Set Themal Resistance Correction
 */
bool board_thermal_setResistanceCorrection(bool toEnable)
{
	bool ret = 0;
	ret = dev_tmp432_setResistanceCorrection(toEnable);
	dev_lm95234_init(LM95234_I2C_PORT,LM95234_I2C_ADDRESS);
	return ret;
}

/**
 * @brief ACPI handler for the thermal related data2
 *
 * @param     isRead:    True is read and false is write
 * @param     ui8Idx:    index
 * @param     pui8Data:  data buffer
 */
void ACPIThermalHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t _b_pcbTmp, _b_pcbTmpQ1 /* Q87 */, _b_pcbTmpQ2	/* Q47 */;
	static uint16_t _b_u16dieTmp;
	static uint16_t _b_u16evalSensor;
	// static uint16_t _a_u16pcbTmp2;
	// static uint16_t _a_u16pcbTmpQ21, _a_u16pcbTmpQ22, _a_u16pcbTmpQ23, _a_u16pcbTmpQ24;
	uint8_t ui8Index;
	
	ui8Index = ui8Idx - ACPI_THERMAL;

	/*
	 * Read high byte lock the 16-bit number.
	 *
	 * Bytes
	 *   1/0 - die temp from SB-TSI
	 *   3/2 - TMP432 local sensor
	 *   5/4 - TMP432 remote 1 (Q87)
	 *   7/6 - TMP432 remote 2 (Q47)
	 *   9/8 - dGPU temp
	 */
	if (isRead) {
		switch (ui8Index) {
			case 1:
				_b_u16dieTmp = _s_u16dieTmp;

				*pui8Data = (_b_u16dieTmp >> 8) & 0xFF;
				break;
			case 0:
				*pui8Data = _b_u16dieTmp & 0xFF;
				break;

			case 3:
				_b_pcbTmp = _s_u16pcbTmp;

				*pui8Data = (_b_pcbTmp >> 8) & 0xFF;
				break;
			case 2:
				*pui8Data = _b_pcbTmp & 0xFF;
				break;

			case 5:
				_b_pcbTmpQ1 = _s_u16pcbTmpQ1;

				*pui8Data = (_b_pcbTmpQ1 >> 8) & 0xFF;
				break;
			case 4:
				*pui8Data = _b_pcbTmpQ1 & 0xFF;
				break;

			case 7:
				_b_pcbTmpQ2 = _s_u16pcbTmpQ2;

				*pui8Data = (_b_pcbTmpQ2 >> 8) & 0xFF;
				break;
			case 6:
				*pui8Data = _b_pcbTmpQ2 & 0xFF;
				break;

			case 9:
				_b_u16evalSensor = _s_u16evalSensor;

				*pui8Data = (_b_u16evalSensor >> 8) & 0xFF;
				break;
			case 8:
				*pui8Data = _b_u16evalSensor & 0xFF;
				break;

			default:
				break;
		}
	}
}

/*
 * Runs by PURMAIN thread.
 */
/**
 * @brief ACPI handler for the thermal related data1
 *
 * @param     isRead:    True is read and false is write
 * @param     ui8Idx:    index
 * @param     pui8Data:  data buffer
 */
uint8_t board_thermal_AcpiHandler(uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	static uint16_t _a_pcbTmp, _a_pcbTmpQ1 /* Q87 */, _a_pcbTmpQ2	/* Q47 */;
	static uint16_t _a_u16pcbTmp2;
	static uint16_t _a_u16pcbTmpQ21, _a_u16pcbTmpQ22, _a_u16pcbTmpQ23, _a_u16pcbTmpQ24;
	static uint16_t _a_u16dieTmp;
	static uint16_t _a_u16evalSensor;

	/*
	 * Read high byte lock the 16-bit number.
	 *
	 * Bytes
	 *   1/0 - die temp from SB-TSI
	 *   3/2 - TMP432 local sensor
	 *   5/4 - TMP432 remote 1 (Q87)
	 *   7/6 - TMP432 remote 2 (Q47)
	 *   9/8 - dGPU temp
	 */
	if (isRead) {
		switch (ui8Idx) {
			case 1:
				_a_u16dieTmp = _s_u16dieTmp;

				*pui8Data = (_a_u16dieTmp >> 8) & 0xFF;
				break;
			case 0:
				*pui8Data = _a_u16dieTmp & 0xFF;
				break;

			case 3:
				_a_pcbTmp = _s_u16pcbTmp;

				*pui8Data = (_a_pcbTmp >> 8) & 0xFF;
				break;
			case 2:
				*pui8Data = _a_pcbTmp & 0xFF;
				break;

			case 5:
				_a_pcbTmpQ1 = _s_u16pcbTmpQ1;

				*pui8Data = (_a_pcbTmpQ1 >> 8) & 0xFF;
				break;
			case 4:
				*pui8Data = _a_pcbTmpQ1 & 0xFF;
				break;

			case 7:
				_a_pcbTmpQ2 = _s_u16pcbTmpQ2;

				*pui8Data = (_a_pcbTmpQ2 >> 8) & 0xFF;
				break;
			case 6:
				*pui8Data = _a_pcbTmpQ2 & 0xFF;
				break;

			case 9:
				_a_u16evalSensor = _s_u16evalSensor;

				*pui8Data = (_a_u16evalSensor >> 8) & 0xFF;
				break;
			case 8:
				*pui8Data = _a_u16evalSensor & 0xFF;
				break;
			
			case 11:
				_a_u16pcbTmp2 = _s_u16pcbTmp2;

				*pui8Data = (_a_u16pcbTmp2 >> 8) & 0xFF;
				break;
			case 10:
				*pui8Data = _a_u16pcbTmp2 & 0xFF;
				break;

			case 13:
				_a_u16pcbTmpQ21 = _s_u16pcbTmpQ21;

				*pui8Data = (_a_u16pcbTmpQ21 >> 8) & 0xFF;
				break;
			case 12:
				*pui8Data = _a_u16pcbTmpQ21 & 0xFF;
				break;

			case 15:
				_a_u16pcbTmpQ22 = _s_u16pcbTmpQ22;

				*pui8Data = (_a_u16pcbTmpQ22 >> 8) & 0xFF;
				break;
			case 14:
				*pui8Data = _a_u16pcbTmpQ22 & 0xFF;
				break;

			case 17:
				_a_u16pcbTmpQ23 = _s_u16pcbTmpQ23;

				*pui8Data = (_a_u16pcbTmpQ23 >> 8) & 0xFF;
				break;
			case 16:
				*pui8Data = _a_u16pcbTmpQ23 & 0xFF;
				break;

			case 19:
				_a_u16pcbTmpQ24 = _s_u16pcbTmpQ24;

				*pui8Data = (_a_u16pcbTmpQ24 >> 8) & 0xFF;
				break;
			case 18:
				*pui8Data = _a_u16pcbTmpQ24 & 0xFF;
				break;

			default:
				return 0;
		}

		return 1;
	}

	return 0;
}

/**
 * @brief I2C protocal is following NDA 55952 (4.2.1 SMBus Write Cycle/4.2.2 SMBus Read Cycle)
 *
 * @param     isRead:    True is read and false is write
 * @param     slvAddr:   I2C slave address
 * @param     reg:       I2C register
 * @param     pData:     I2C data buffer pointer
 * @retval 0 if successful.
 */
int board_eval_regAccess(bool isRead, uint16_t slvAddr, uint32_t reg, uint32_t * pData)
{
	uint8_t retry = 3;
	int ret;
	uint8_t cmd;
	uint8_t addrBuf[6];
	uint8_t dataBuf[6];

	while (retry) {
		retry --;

		/* Prepare buffer for addr load. */
		/* [Slave Addr]|W; CMD_LD_ADDR; ByteCount 4; { 4'b0000, BE[3:0] }; Addr[25:18]; Addr[17:10];  Addr[9:2] */
		addrBuf[0] = CMD_LD_ADDR;
		addrBuf[1] = 4;                  // Byte count = 4
		addrBuf[2] = 0x0F;               // BE[3:0] = 4'b1111
		addrBuf[3] = 0xFF & (reg >> 18); // Addr[25:18]
		addrBuf[4] = 0xFF & (reg >> 10); // Addr[17:10]
		addrBuf[5] = 0xFF & (reg >> 2);  // Addr[9:2]

		ret = i2c_hub_write(I2C_EVAL_PORT, addrBuf, 6,slvAddr);
		if (ret != 0) {
			continue;
		}

		if (isRead) {
			cmd = CMD_RD_DATA;

			ret = i2c_hub_write_read (I2C_EVAL_PORT, slvAddr, &cmd, 1, dataBuf, 5);
			if (0 == ret && 4 == dataBuf[0]) {
				*pData = ((uint32_t)dataBuf[1] << 24) | ((uint32_t)dataBuf[2] << 16) | ((uint32_t)dataBuf[3] << 8) | ((uint32_t)dataBuf[4]);
			}
		} else {
			dataBuf[0] = CMD_WR_DATA;
			dataBuf[1] = 4;                       /* Byte count = 4 */
			dataBuf[2] = 0xFF & ((*pData) >> 24); /* Data[31:24] */
			dataBuf[3] = 0xFF & ((*pData) >> 16); /* Data[23:16] */
			dataBuf[4] = 0xFF & ((*pData) >> 8 ); /* Data[15:8] */
			dataBuf[5] = 0xFF & ((*pData));       /* Data[7:0] */

			ret = i2c_hub_write (I2C_EVAL_PORT,dataBuf, 6,slvAddr);
		}

		LOG_DBG("Acc EVAL<%02X> %s [%06X/4] %02X_%02X_%02X, data 0x%08X, ret %d", slvAddr, isRead ? "R" : "W", reg, addrBuf[3], addrBuf[4], addrBuf[5], *pData, ret);
	}
	if (!retry) {
		LOG_DBG("[!!Fatal error!!] on EVAL<%02X> %s x[%06X/4] %02X_%02X_%02X, data 0x%08X, ret %d", slvAddr, isRead ? "R" : "W", reg, addrBuf[3], addrBuf[4], addrBuf[5], *pData, ret);
	}

	return ret;
}

/**
 * board_thermal_reporter 
 *
 * @param [in]   BoardTmp;
 *
 * @note
 *  Report board thermal information
 */
void board_thermal_reporter(board_thermal_info       BoardTmp)
{
	uint8_t reportOnboardTemp;
	uint8_t reportEvalCardTemp;
	/* SB-RMI address
	 *    (0x78 >> 1) - Socket ID / Package number 0
	 *    (0x70 >> 1) - Socket ID / Package number 1
	 */
	GET_NV_OPTIONS(thm_uploadOnboardTemp, reportOnboardTemp);
	if (reportOnboardTemp && g_u32ApmlErrCnt < APML_ERROR_NUM_THRESHOLD) {
		for(int TmpQn=0;TmpQn<BoardTmp._s_u16pcbTmpQMax;TmpQn++)
		{
			/* Skip SB-TSI in Zx */
			if (!espihub_socInLP()) {
				if (!dev_sbi_writeSttSensor(SBRMI_SLV_ADDRESS_PKG0, TmpQn, BoardTmp._s_u16pcbTmpQ[TmpQn])) {
					g_u32ApmlErrCnt++;
				}
			}
		}
	}
	GET_NV_OPTIONS(thm_uploadEvalCardTemp, reportEvalCardTemp);
	if (reportEvalCardTemp && g_u32ApmlErrCnt < APML_ERROR_NUM_THRESHOLD) {

		/* P21 data book 4.2.3 requires to
		 *     a. Write 0xC0300014 to 0x3C000238/4
		 *     b. Read 0x3C00023C/4, [17:9] is the tempurature
		 */
		uint32_t u32Data = 0xC0300014;
		board_eval_regAccess(0, 0x40, 0x3C000238, &u32Data);
		board_eval_regAccess(1, 0x40, 0x3C00023C, &u32Data);

		/* [17:9] is the tempurature */
		uint16_t u16tmp = (u32Data & 0x3FE00) >> 9;
		if (u16tmp > 0xFF) {
			_s_u16evalSensor = 0xFF00;
		} else {
			_s_u16evalSensor = u16tmp << 8;
		}

		/* Skip SB-TSI in Zx */
		if (!espihub_socInLP()) {
			if (!dev_sbi_writeSttSensor(SBRMI_SLV_ADDRESS_PKG0, 2, _s_u16evalSensor)) {
				g_u32ApmlErrCnt++;
			}
		}
	}
}