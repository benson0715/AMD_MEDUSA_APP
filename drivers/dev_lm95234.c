/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include "dev_lm95234.h"
#include "board_config.h"
#include "i2c_hub.h"
LOG_MODULE_REGISTER(lm95234, CONFIG_LM95234_LOG_LEVEL);

/* Macro define */
#define READ_LM95234         true
#define WRITE_LM95234        false

/*****************************************************************************
 * Function name:   dev_lm95234_access
 *                   
 * Description:     Access LM95234 Register
 * @param           isRead, reg, pData
 * @return          return ret
 * @note    
 *****************************************************************************/
uint32_t dev_lm95234_access (bool isRead, uint8_t port, uint8_t address, uint8_t reg, uint8_t *pData)
{
	uint8_t retry = 3;
	int32_t ret;

	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			ret = i2c_hub_reg_read_byte(port, address, reg, pData);
			if (ret != 0)
				continue;
		} else {
			ret = i2c_hub_reg_write_byte (port, address, reg, *pData);
		}

		LOG_DBG("%s LM95234 [%02X], a(%d), u8 (0x%02X, %d)", isRead ? "R" : "W", reg,
		ret, *((uint8_t *)pData), *(pData));

		if (ret==0)
		return ret;
	}
	
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] on %s LM95234[%02X], ret %d", isRead ? "R" : "W", reg, ret);
	}
	
	return ret;
}

/*****************************************************************************
 * Function name:   dev_lm95234_thermaltrip_set
 *                   
 * Description:     Configure thermal trip point for LM95234
 * @param           port - I2C port
 * @param           address - I2C address
 * @param           pin_mask - pin mask b0 for tcrit 1, b1 for tcrit 2, b2 for tcrit 3
 * @param           temp_c - temperature in Celsius
 * @return          NA
 * @note    
 *****************************************************************************/
void dev_lm95234_thermaltrip_set(uint8_t port, uint8_t address, uint8_t pin_mask, uint8_t temp_c)
{
	for (uint8_t pin = 0; pin<3; pin++) {
		uint8_t en_mask = 0;
		
		if (pin_mask & BIT(pin)) {
			en_mask = BIT_MASK(5); // Remote 1,2,3,4 and local channel are enabled
		}

		dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_MSK_TCRIT(pin), &en_mask);

		if (!en_mask) {
			continue;
		}

		switch (pin) {
			case 0:
				dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(1), &temp_c);  // Remote 1 Tcrit-1 Limit (used by TCRIT1 error events)
				dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(2), &temp_c);  // Remote 2 Tcrit-1 Limit (used by TCRIT1 error events)
				break;
			case 1:
				dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(9), &temp_c);  // Remote 1 Tcrit-2 and Tcrit3 Limit (used by TCRIT2 and TCRIT3 error events)
				dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(10), &temp_c); // Remote 2 Tcrit-2 and Tcrit3 Limit (used by TCRIT2 and TCRIT3 error events)
				dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(3), &temp_c);  // Remote 3 Tcrit Limit (used by TCRIT1, TCRIT2 and TCRIT3 error events)
				dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(4), &temp_c);  // Remote 4 Tcrit Limit (used by TCRIT1, TCRIT2 and TCRIT3 error events)
				break;
			case 2:
				dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(9), &temp_c);  // Remote 1 Tcrit-2 and Tcrit3 Limit (used by TCRIT2 and TCRIT3 error events)
				dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(10), &temp_c);  // Remote 2 Tcrit-2 and Tcrit3 Limit (used by TCRIT2 and TCRIT3 error events)
				break;
			default:
				break;
		}
		dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_TCRIT(0), &temp_c);  // Local Tcrit Limit
	}
}

/*****************************************************************************
 * Function name:   dev_lm95234_thermaltrip_status_get
 *                   
 * Description:     Get thermal trip status for LM95234
 * @param           port - I2C port
 * @param           address - I2C address
 * @param           sensor - sensor index b0 for local sensor, b1 for remote 1 sensor, b2 for remote 2 sensor, b3 for remote 3 sensor, b4 for remote 4 sensor
 * @return          pin mask 0 for tcrit pin 1, 1 for tcrit pin 2, 2 for tcrit pin 3
 *****************************************************************************/
uint8_t dev_lm95234_thermaltrip_status_get(uint8_t port, uint8_t address, uint8_t *sensor)
{
	uint8_t pin = 0;
	uint8_t status = 0;
	dev_lm95234_access(READ_LM95234, port, address, LM95234_REG_STS_TCRIT1, &status);
	if (status > 0) {
		pin = BIT(0);
		*sensor = status;
	}
	dev_lm95234_access(READ_LM95234, port, address, LM95234_REG_STS_TCRIT2, &status);
	if (status > 0) {
		pin |= BIT(1);
		*sensor |= status;
	}
	dev_lm95234_access(READ_LM95234, port, address, LM95234_REG_STS_TCRIT3, &status);
	if (status > 0) {
		pin |= BIT(2);
		*sensor |= status;
	}

	return pin;
}

/*****************************************************************************
 * Function name:   dev_lm95234_init
 *                   
 * Description:     config device
 * @param           port - I2C port
 * @param           address - I2C address
 * @return          NA         
 * @note    
 *****************************************************************************/
void dev_lm95234_init(uint8_t port, uint8_t address){

  uint8_t data[] = { 0x0, 0x3, 0x0, 0x1F};
  // selects diode model 2 MMBT3904
  dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_REM_MODEL, &data[0]);

  // disable standby and enable default queue
  dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_CONFIG, &data[1]);

  // convert rate = continuous (30 ms to 143 ms)
  dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_CONVRATE, &data[2]);

  // enable all channel
  dev_lm95234_access(WRITE_LM95234, port, address, LM95234_REG_CHNEL_EN, &data[3]);

  // TODO: LM95234_REG_TCRIT1 LM95234_REG_TCRIT2
  return;

}

/*****************************************************************************
 * Function name:   dev_lm95234_read_temp
 *                   
 * Description:     Read signed tempareture by LM95234
 * @param           sensor - sensor index
 * @param           port - I2C port
 * @param           address - I2C address
 * @return          reture the current tempareture         
 * @note    
 *****************************************************************************/
uint16_t dev_lm95234_read_temp(DEV_LM95234_SENSOR sensor, uint8_t port, uint8_t address){	
	uint8_t data[2] = {0};
	uint16_t temp = 0;
	uint32_t ret[2];

	//check status register LM95234_REG_STATUS == 0? stauts ok: not ok
	
	//reading the MSB register first. The LSB will be locked after the MSB is read
	ret[0] = dev_lm95234_access(READ_LM95234, port, address, LM95234_REG_TEMPH(sensor), &data[0]);
	ret[1] = dev_lm95234_access(READ_LM95234, port, address, LM95234_REG_TEMPL(sensor), &data[1]);

	// check if error happened
	temp = ((ret[0] == 0) && (ret[1]) == 0) ? (data[0] << 8 | data[1]) : 0xFFFF;
	return temp;
}