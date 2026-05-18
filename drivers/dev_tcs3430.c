/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_tcs3430.h"
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>
#include "f_nv_options.h"
#include "board_config.h"
#include <dev_utility.h>
#include "i2c_hub.h"
LOG_MODULE_REGISTER(tcs3430, CONFIG_TCS3430_LOG_LEVEL);

/* SVI3 -- I2C2 */
#define I2C_TCS34303_PORT       10
#define I2C_TCS34303_ADDRESS    0x39

#define TCS3430_REGISTER_CNT                    (25)
	
#define TCS3430_ATIME_100_MS	 	              	(0x23)
#define TCS3430_WTIME_1_MS		 	              	(0x00)
#define TCS3430_APERS_INT_MODE		            	(0x05)
#define TCS3430_APERS_POLL_MODE		            	(0x00)
#define TCS3430_RUN_AZ_AUTOMATICALLY            	(0x7F)
#define TCS3430_DISABLE_128X_AGAIN		            (0x00)
#define TCS3430_DISABLE_TOGGLE_IR2		            (0x00)

#define TCS3430_AGAIN_GAIN_1X           		    (0 << AGAIN_SHIFT)
#define TCS3430_AGAIN_GAIN_4X            		    (1 << AGAIN_SHIFT)
#define TCS3430_AGAIN_GAIN_16X           		    (2 << AGAIN_SHIFT)
#define TCS3430_AGAIN_GAIN_64X           		    (3 << AGAIN_SHIFT)
	
/**
 * Global variables
 */
static tcs3430_display_calibration_private_data private_data[1] = {0};
static tcs3430_registers tcs3430_reg;
static uint8_t tcs_3430_registers_address[TCS3430_REGISTER_CNT] = {
		TCS3430_REG_ENABLE,    //0                    
		TCS3430_REG_ATIME,     //1                     
		TCS3430_REG_WTIME,     //2                     
		TCS3430_REG_AILTL,     //3                   
		TCS3430_REG_AILTH,     //4                    
		TCS3430_REG_AIHTL,     //5                    
		TCS3430_REG_AIHTH,     //6                   
		TCS3430_REG_PERS,      //7                   
		TCS3430_REG_CFG0,      //8                     
		TCS3430_REG_CFG1,      //9                     
		TCS3430_REG_REVID,     //A                     
		TCS3430_REG_ID,        //B                    
		TCS3430_REG_STATUS,    //C                    
		TCS3430_REG_CH0DATAL,  //D                      
		TCS3430_REG_CH0DATAH,  //E                   
		TCS3430_REG_CH1DATAL,  //F                     
		TCS3430_REG_CH1DATAH,  //10                   
		TCS3430_REG_CH2DATAL,  //11                 
		TCS3430_REG_CH2DATAH,  //12                
		TCS3430_REG_CH3DATAL,  //13                 
		TCS3430_REG_CH3DATAH,  //14                
		TCS3430_REG_CFG2,      //15                
		TCS3430_REG_CFG3,      //16                   
		TCS3430_REG_AZ_CONFIG, //17                  
		TCS3430_REG_INTENAB,   //18                      
};

uint32_t g_tcs3430LuxrSum = 0;

/**
 * dev_tcs3430_regAccess - Access TCS3430 registers via I2C
 *
 * @param isRead: 0 - to write, 1 - to read
 * @param reg: registers' offset
 * @param pBuf: a buffer pointer for data in/out
 * @param len: data length/register width
 *
 * @return The return value of I2C communication.
 */
uint32_t dev_tcs3430_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len)
{
	uint8_t retry = 3;
	int ret;

	while (retry) 
	{
		retry --;
		ret = 0;
		if (isRead) {
			ret = i2c_hub_write_read(I2C_TCS34303_PORT, I2C_TCS34303_ADDRESS, (uint8_t *)&reg, 2, pBuf, len);
		} else {
			ret = i2c_hub_burst_write_multi_cmd(I2C_TCS34303_PORT, I2C_TCS34303_ADDRESS, (uint8_t *)&reg, 2, pBuf, len);
		}
			
		for ( uint32_t i = 0; i < len; i++ ) 
		{
		   LOG_DBG(" %02X", *((uint8_t *)pBuf + i));
		}
	
	}
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] on %s tcs3430[%02X], ret %d", isRead ? "R" : "W", reg, ret);
	}

	return ret;
}

/**
 * TCS3430_init - Initialize the TCS3430 sensor
 *
 * Initializes the physical sensor and registers its context into the sensor core FW.
 * This function is responsible for:
 * 1) Create the sensor in the sensor core FW and obtain its context.
 * 2) Initialize the sensor's private data (optional)
 * 3) Validating the sensor presence (optional)
 *    a) validate the sensor i2c address
 *    b) call udriver_validate_id function to test sensor connectivity
 * 4) Register FIFO if supported
 * 5) Register GPIO if supported
 * 6) Setting the sensor to its default configuration
 * 7) Setting the sensor to READY state
 */
void TCS3430_init(void)
{
	uint8_t instance_id = 0;
	uint8_t buf = 0;
	DEV_TCS3430_REG_CFG0 cfg0;
	DEV_TCS3430_REG_CFG1 cfg1;
	DEV_TCS3430_REG_CFG2 cfg2;
	DEV_TCS3430_REG_CFG3 cfg3;
	DEV_TCS3430_REG_STATUS status;
	DEV_TCS3430_REG_ATIME atime;
	DEV_TCS3430_REG_ENABLE enable;
	DEV_TCS3430_REG_INTENAB intenab;
	DEV_TCS3430_REG_PERS pers;

	/* TODO: wtime,pers,autozero logic */
	private_data[instance_id].caldata.atime = TCS3430_ATIME_100_MS;
	private_data[instance_id].caldata.wtime = TCS3430_WTIME_1_MS;
#ifdef ENABLE_PORT_INTERRUPT
	private_data[instance_id].caldata.pers = TCS3430_APERS_INT_MODE;
#else
	private_data[instance_id].caldata.pers = TCS3430_APERS_POLL_MODE;
#endif
	private_data[instance_id].caldata.autozero = TCS3430_RUN_AZ_AUTOMATICALLY;
	private_data[instance_id].caldata.again = TCS3430_AGAIN_GAIN_16X;
	private_data[instance_id].caldata.hgain = TCS3430_DISABLE_128X_AGAIN;
	private_data[instance_id].caldata.toggle_ir2 = TCS3430_DISABLE_TOGGLE_IR2;
	private_data[instance_id].caldata.autogain = 1;

	/* cfg0 */	
	dev_tcs3430_regAccess(1, TCS3430_REG_CFG0, &cfg0.buf, 1);
	cfg0.f.RESET = 1;
	cfg0.f.WLONG = 0;
	dev_tcs3430_regAccess(0, TCS3430_REG_CFG0, &cfg0.buf, 1);

	/* cfg1 */
	dev_tcs3430_regAccess(1, TCS3430_REG_CFG1, &cfg1.buf, 1);
	cfg1.f.AGAIN = private_data[instance_id].caldata.again;
	dev_tcs3430_regAccess(0, TCS3430_REG_CFG1, &cfg1.buf, 1);

	/* During Initialization, clear the STATUS register */
	dev_tcs3430_regAccess(1, TCS3430_REG_STATUS, &status.buf, 1);
	status.f.AINT = 1;
	status.f.ASAT = 1;
	dev_tcs3430_regAccess(0, TCS3430_REG_STATUS, &status.buf, 1);											
											
	/* cfg2 */
	dev_tcs3430_regAccess(1, TCS3430_REG_CFG2, &cfg2.buf, 1);
	cfg2.f.HGAIN = (private_data[instance_id].caldata.hgain) ? 1 : 0;
	dev_tcs3430_regAccess(0, TCS3430_REG_CFG2, &cfg2.buf, 1);

	/* cfg3 */										 
	dev_tcs3430_regAccess(1, TCS3430_REG_CFG3, &cfg3.buf, 1);
	cfg3.f.INT_READ_CLEAR = 1;
	dev_tcs3430_regAccess(0, TCS3430_REG_CFG3, &cfg3.buf, 1);

	/* atime */
	atime.f.ATIME = private_data[instance_id].caldata.atime;
	dev_tcs3430_regAccess(0, TCS3430_REG_ATIME, &atime.buf, 1);

	/* enable */
	enable.buf = 0;
	enable.f.AEN = 1;
	enable.f.PON = 1;
	dev_tcs3430_regAccess(0, TCS3430_REG_ENABLE, &enable.buf, 1);

	/* intenab */
	intenab.buf = 0;
	intenab.f.AIEN = 1;
	intenab.f.ASIEN = 0;
	dev_tcs3430_regAccess(0, TCS3430_REG_INTENAB, &intenab.buf, 1);

	/* enable */
	enable.buf = 0;
	dev_tcs3430_regAccess(0, TCS3430_REG_ENABLE, &enable.buf, 1);
	
	/* apers */
	pers.buf = private_data[instance_id].caldata.pers;
	dev_tcs3430_regAccess(0, TCS3430_REG_PERS, &pers.buf, 1);
	
	/* ailtl */
	buf = 0xFF;
	dev_tcs3430_regAccess(0, TCS3430_REG_AILTL, &buf, 1);

	/* ailth */
	buf = 0xFF;
	dev_tcs3430_regAccess(0, TCS3430_REG_AILTH, &buf, 1);

	/* aihtl */
	buf = 0x00;
	dev_tcs3430_regAccess(0, TCS3430_REG_AIHTL, &buf, 1);

	/* aihth */
	buf = 0x00;
	dev_tcs3430_regAccess(0, TCS3430_REG_AIHTH, &buf, 1);
									
	/* enable */
	enable.buf = 0;
	enable.f.AEN = 1;
	enable.f.PON = 1;
	dev_tcs3430_regAccess(0, TCS3430_REG_ENABLE, &enable.buf, 1);
	
	TCS3430_DumpRegister();
}

/**
 * TCS3430_DumpRegister - Dump the TCS3430 register map
 *
 * Reads all TCS3430 registers and calculates the Luxr sum from channel data.
 */
void TCS3430_DumpRegister(void)
{
	uint8_t index = 0;
	
	for (index = 0; index < TCS3430_REGISTER_CNT; index++)
	{
		dev_tcs3430_regAccess(1, tcs_3430_registers_address[index], &tcs3430_reg.buf[index], 1);
	}
	
	/* Calculate the Luxr sum */
	g_tcs3430LuxrSum = 	tcs3430_reg.buf[13] + (tcs3430_reg.buf[14] << 8) + // TCS3430_REG_CH0
	                    tcs3430_reg.buf[15] + (tcs3430_reg.buf[16] << 8) + // TCS3430_REG_CH1
	                    tcs3430_reg.buf[17] + (tcs3430_reg.buf[18] << 8) + // TCS3430_REG_CH2
	                    tcs3430_reg.buf[19] + (tcs3430_reg.buf[20] << 8);  // TCS3430_REG_CH3
}

/**
 * TCS3430_Register_Offset_Handler - Handle register offset read operations
 *
 * @param isRead: Read flag (1 for read operation)
 * @param ui8Idx: Register index
 * @param pui8Data: Pointer to data buffer
 *
 * @return 1 on success, 0 on failure
 */
uint8_t TCS3430_Register_Offset_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= 0x31)
		return 0;
	
	if (ui8Idx >= TCS3430_REGISTER_CNT)
		return 0;

	if (isRead) {
		
		*pui8Data = tcs_3430_registers_address[ui8Idx];
	}

	return 1;
}

/**
 * TCS3430_Register_Value_Handler - Handle register value read/write operations
 *
 * @param isRead: Read/write flag (1 for read, 0 for write)
 * @param ui8Idx: Register index
 * @param pui8Data: Pointer to data buffer
 *
 * @return 1 on success, 0 on failure
 */
uint8_t TCS3430_Register_Value_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data)
{
	if (ui8Idx >= 0x31)
		return 0;

	if (isRead) {
		if (ui8Idx >= (TCS3430_REGISTER_CNT + 4))
			return 0;
		else if (ui8Idx < TCS3430_REGISTER_CNT)
			*pui8Data = tcs3430_reg.buf[ui8Idx];
		else if (ui8Idx == TCS3430_REGISTER_CNT)        //0x19
			*pui8Data = (uint8_t)g_tcs3430LuxrSum;
		else if (ui8Idx == (TCS3430_REGISTER_CNT + 1))  //0x1A
			*pui8Data = (uint8_t)(g_tcs3430LuxrSum >> 8);
		else if (ui8Idx == (TCS3430_REGISTER_CNT + 2))  //0x1B  
			*pui8Data = (uint8_t)(g_tcs3430LuxrSum >> 16);
		else if (ui8Idx == (TCS3430_REGISTER_CNT + 3))  //0x1C
			*pui8Data = (uint8_t)(g_tcs3430LuxrSum >> 24);
	}
	else {
		if (ui8Idx >= TCS3430_REGISTER_CNT)
			return 0;
		
		tcs3430_reg.buf[ui8Idx] = *pui8Data;
		dev_tcs3430_regAccess(0, tcs_3430_registers_address[ui8Idx], &tcs3430_reg.buf[ui8Idx], 1);
	}

	return 1;
}