/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef _DEV_LM95234_H
#define _DEV_LM95234_H
#include <stdint.h>
#include <stdbool.h>
/* LM95234 registers */
#define LM95234_REG_MAN_ID          (0xFE)
#define LM95234_REG_CHIP_ID			(0xFF)

#define LM95234_REG_CONFIG		    (0x03) /* config register */
#define LM95234_REG_CONVRATE		(0x04) /* convert rate */
#define LM95234_REG_CHNEL_EN		(0x05) /* channel enable */
#define LM95234_REG_FILTER		    (0x06) /* filter setting */

#define LM95234_REG_STATUS		    (0x02) /* common status register */
#define LM95234_REG_STS_FAULT		(0x07) /* Status fault bits for open or shorted diode */
#define LM95234_REG_STS_TCRIT1		(0x08) /* Status bits for TCRIT1 */
#define LM95234_REG_STS_TCRIT2		(0x09) /* Status bits for TCRIT2 */
#define LM95234_REG_STS_TCRIT3		(0x0A) /* Status bits for TCRIT3 */
#define LM95234_RE_STS_REM_MODEL	(0x38) /* Diode Model Status (TruTherm on and 3904 connected) */

#define LM95234_REG_MSK_TCRIT1	    (0xC)  /* TCRIT1 Mask Registers */
#define LM95234_REG_MSK_TCRIT2	    (0xD)  /* TCRIT2 Mask Registers */
#define LM95234_REG_MSK_TCRIT3	    (0xE)  /* TCRIT3 Mask Registers */
#define LM95234_REG_MSK_TCRIT(x)	((x) + 0x0C)  /* TCRITx Mask Registers */
#define LM95234_REG_1SHOT		    (0xF)  /*  write only */

#define LM95234_REG_TEMPH(x)		((x) + 0x10) /* signed tempareture high byte */
#define LM95234_REG_TEMPL(x)		((x) + 0x20) /* signed tempareture low byte */
#define LM95234_REG_UTEMPH(x)		((x) + 0x19) /* unsigned tempareture high byte */
#define LM95234_REG_UTEMPL(x)		((x) + 0x29) /* unsigned tempareture low byte */
#define LM95234_REG_REM_MODEL		(0x30)

#define LM95234_REG_OFFSET(x)		((x) + 0x31) /* Remote only, compensation temperature */
#define LM95234_REG_TCRIT1(x)		((x) + 0x40) /* Limit Registers */
#define LM95234_REG_TCRIT2(x)		((x) + 0x49) /* Remote channel 1,2 */
#define LM95234_REG_TCRIT(x)		((x) + 0x40) /* Remote channel x */
#define LM95234_REG_TCRIT_HYST		(0x5a)

#define NATSEMI_MAN_ID			    (0x01)
#define LM95234_CHIP_ID			    (0x79)

/* register value*/
//#define LM95234_CONVRATE_CONTINU  0x0
//#define LM95234_CONVRATE_346MS    0x1
//#define LM95234_CONVRATE_1S       0x2
//#define LM95234_CONVRATE_2S       0x3


/* structure define */
typedef enum {
	DEV_LM95234_SENSOR_LOCAL  = 0x0,
	DEV_LM95234_SENSOR_REMOT1 = 0x1,
	DEV_LM95234_SENSOR_REMOT2 = 0x2,
	DEV_LM95234_SENSOR_REMOT3 = 0x3,
	DEV_LM95234_SENSOR_REMOT4 = 0x4	
}DEV_LM95234_SENSOR;

/* function */
/**
 * @brief Read temp from LM95234.
 *
 * @param sensor Index of the sensor.
 * @param port LM95234 device port.
 * @param address Sensor slave address.
 * 
 * @retval reture the current tempareture.
 */
uint16_t dev_lm95234_read_temp(DEV_LM95234_SENSOR sensor, uint8_t port, uint8_t address);

/**
 * @brief Init LM95234 device.
 *
 * @param port LM95234 device port.
 * @param address Sensor slave address.
 */
void dev_lm95234_init(uint8_t port, uint8_t address);

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
 void dev_lm95234_thermaltrip_set(uint8_t port, uint8_t address, uint8_t pin_mask, uint8_t temp_c);

 /*****************************************************************************
 * Function name:   dev_lm95234_thermaltrip_status_get
 *                   
 * Description:     Get thermal trip status for LM95234
 * @param           port - I2C port
 * @param           address - I2C address
 * @param           sensor - sensor index b0 for local sensor, b1 for remote 1 sensor, b2 for remote 2 sensor, b3 for remote 3 sensor, b4 for remote 4 sensor
 * @return          pin mask 0 for tcrit pin 1, 1 for tcrit pin 2, 2 for tcrit pin 3
 *****************************************************************************/
uint8_t dev_lm95234_thermaltrip_status_get(uint8_t port, uint8_t address, uint8_t *sensor);

#endif
