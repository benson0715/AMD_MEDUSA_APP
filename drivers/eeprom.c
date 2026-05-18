/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */


#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "i2c_hub.h"
#include "board_id.h"
#include "board_config.h"
#include "eeprom.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(eeprom, CONFIG_EEPROM_LOG_LEVEL);

/* LAN enable/disable status */
#define EEPROM_LANSTS               0x00
/* EEPROM write delay 5ms */
#define EEPROM_WR_MSDELAY           5

#define EEPROM_DEFAULT_DATA         0xFFu
#define DATA_MAX_LEN                256

#ifndef EEPROM_DRIVER_I2C_ADDR
#define EEPROM_DRIVER_I2C_ADDR      0x50
#endif

#ifndef EEPROM_DRIVER_I2C_PORT
#define EEPROM_DRIVER_I2C_PORT      (I2C_3)
#endif

#define OFS_MSB(word)  (((word & 0xFF00) >> 8))
#define OFS_LSB(word)  (word & 0xFF)

/* EEPROM access for offset greater than 255.
 * Following the 4-bit device type identifier in the bits 3-1 of the device
 * slave address byte are bits A10, A9 and A8 which are the three MSB of the
 * memory array word address
 * i.e. offset 0x0006 correspond to device address 0x50, offset 0x06
 * i.e. offset 0x0106 correspond to device address 0x51, offset 0x00
 */

int eeprom_read_byte(uint16_t offset, uint8_t *data)
{
	/* 24LC128 needs the high byte address presenting first on the bus */
	uint16_t reversedRegister = ((offset & 0xFF) << 8) | ((offset >> 8) & 0xFF);
	uint8_t ret;
	uint8_t buf = OFS_MSB(reversedRegister);

	ret = i2c_hub_write_read(EEPROM_DRIVER_I2C_PORT,
			     EEPROM_DRIVER_I2C_ADDR | OFS_LSB(reversedRegister),
			     &buf, sizeof(buf),
			     data, 1);
	if (ret) {
		LOG_ERR("Fail to read: %d", ret);
		return ret;
	}

	return 0;
}

int eeprom_write_byte(uint16_t offset, uint8_t data)
{
	/* 24LC128 needs the high byte address presenting first on the bus */
	uint16_t reversedRegister = ((offset & 0xFF) << 8) | ((offset >> 8) & 0xFF);
	uint8_t ret;
	uint8_t buf[] = { OFS_LSB(reversedRegister), data };

	ret = i2c_hub_write(EEPROM_DRIVER_I2C_PORT, buf, sizeof(buf),
			EEPROM_DRIVER_I2C_ADDR | OFS_MSB(reversedRegister));
	if (ret) {
		LOG_ERR("Fail to write: %d", ret);
		return ret;
	}

	k_msleep(EEPROM_WR_MSDELAY);

	return 0;
}

int eeprom_read_word(uint16_t offset, uint16_t *data)
{
	/* 24LC128 needs the high byte address presenting first on the bus */
	uint16_t reversedRegister = ((offset & 0xFF) << 8) | ((offset >> 8) & 0xFF);
	uint8_t ret;
	uint8_t buf = { OFS_LSB(reversedRegister) };
	uint8_t rbuf[] = {EEPROM_DEFAULT_DATA, EEPROM_DEFAULT_DATA};

	ret = i2c_hub_write_read(EEPROM_DRIVER_I2C_PORT,
			     EEPROM_DRIVER_I2C_ADDR | OFS_LSB(reversedRegister),
			     &buf, sizeof(buf),
			     rbuf, sizeof(rbuf));
	if (ret) {
		LOG_ERR("Fail to read: %d", ret);
		return ret;
	}

	/* Adjust endianness */
	*data = ((rbuf[0] << 8) | rbuf[1]);

	return 0;
}

int eeprom_write_word(uint16_t offset, uint16_t data)
{
	/* 24LC128 needs the high byte address presenting first on the bus */
	uint16_t reversedRegister = ((offset & 0xFF) << 8) | ((offset >> 8) & 0xFF);
	uint16_t ret;
	uint8_t buf[] = { OFS_LSB(reversedRegister),
		       OFS_MSB(data), OFS_LSB(data) };

	ret = i2c_hub_write(EEPROM_DRIVER_I2C_PORT, buf, sizeof(buf),
			EEPROM_DRIVER_I2C_ADDR | OFS_MSB(reversedRegister));
	if (ret) {
		LOG_ERR("Fail to write: %d", ret);
		return ret;
	}

	/* Delay to wait for write completion */
	k_msleep(EEPROM_WR_MSDELAY);

	return 0;
}

int eeprom_read_block(uint16_t offset, uint8_t len, uint8_t *data)
{
	/* 24LC128 needs the high byte address presenting first on the bus */
	uint16_t reversedRegister = ((offset & 0xFF) << 8) | ((offset >> 8) & 0xFF);
	uint16_t ret;
	uint8_t retry = 5;
	
	while (retry) {
		retry --;
		ret = 0;
		ret  = i2c_hub_write (EEPROM_DRIVER_I2C_PORT, (uint8_t *) &reversedRegister, 2,EEPROM_DRIVER_I2C_ADDR);
		if (ret)
		  continue;
		ret += i2c_hub_read (EEPROM_DRIVER_I2C_PORT, data, len,EEPROM_DRIVER_I2C_ADDR);
		}

	if (len > DATA_MAX_LEN) {
		return -EINVAL;
	}

	if (ret) {
		LOG_ERR("Fail to read: %d", ret);
		return -EIO;
	}

	return ret;
}

int eeprom_write_block(uint16_t offset, uint8_t len, uint8_t *data)
{
	/* 24LC128 needs the high byte address presenting first on the bus */
	uint16_t reversedRegister = ((offset & 0xFF) << 8) | ((offset >> 8) & 0xFF);
	int ret;
	uint8_t buf[DATA_MAX_LEN + sizeof(reversedRegister)];

	if (len > DATA_MAX_LEN) {
		return -EINVAL;
	}

	buf[0] = OFS_LSB(reversedRegister);
	buf[1] = OFS_MSB(reversedRegister);
	memcpy(&buf[2], data, len);

	ret = i2c_hub_write(EEPROM_DRIVER_I2C_PORT, buf, len + sizeof(reversedRegister),
			EEPROM_DRIVER_I2C_ADDR );
	if (ret) {
		LOG_ERR("Fail to write: %d", ret);
		return ret;
	}

	k_msleep(EEPROM_WR_MSDELAY);

	return ret;
}
