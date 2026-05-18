/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_pca9557.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include <dev_utility.h>
#include "i2c_hub.h"
LOG_MODULE_REGISTER(pca9557, CONFIG_PCA9557_LOG_LEVEL);

/*
 * PCA9557 inner structure
 */
#define DEV_PCA9557_MAX_CHIP_NUM    8

typedef struct {
	uint8_t slv;
    uint8_t i2cDev;

	/*P1_7, P1_6, P1_5.......P0_2, P0_1, P0_0*/
	uint8_t lastRead;               // DEV_PCA9557_REG_INPUT    0x00
	uint8_t lastSet;                // DEV_PCA9557_REG_OUTPUT   0x01
	uint8_t polarity;               // DEV_PCA9557_REG_POLARITY 0x02
	uint8_t config;                 // DEV_PCA9557_REG_CONFIG   0x03
	uint8_t intMask;                // DEV_PCA9557_REG_INT_MASK 0x45

	/* pin setup */
	uint8_t outputVal;
	uint8_t outputMask;
	uint8_t openDrainMask;

	struct k_mutex opsLock;
} DEV_PCA9557_SLV_PORT_MAP;

/*
 * I2C controller allocation
 */
#define DEV_PCA9557_FORCE_WRITE     0

static DEV_PCA9557_SLV_PORT_MAP _s_slv2port[DEV_PCA9557_MAX_CHIP_NUM] = {
	{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
};

/**
 * @brief Get PCA9557 configuration by slave address and I2C device
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 * @return Pointer to configuration structure, NULL if not found
 */
static DEV_PCA9557_SLV_PORT_MAP* _dev_pca9557_getCfg (uint8_t slv, uint8_t i2cDev)
{
	for (uint8_t i = 0; i < DEV_PCA9557_MAX_CHIP_NUM; i ++) {
		/* Check both I2C port and I2C address */
		if ((_s_slv2port[i].slv == slv) && (_s_slv2port[i].i2cDev == i2cDev)) {
			return &_s_slv2port[i];
		}
	}

	return NULL;
}

/**
 * @brief Calculate config and output register values based on pin masks
 * @param outputMask Push-pull output pin mask
 * @param openDrainMask Open-drain output pin mask
 * @param outputValue Output value for pins
 * @return Combined config (high byte) and output (low byte) values
 */
static uint16_t _dev_pca9557_makeup(uint8_t outputMask, uint8_t openDrainMask, uint8_t outputValue)
{
	if (outputMask & openDrainMask) {
		/* One pin be set as both Push-pull and OD */
		assert (0);
	}

	/* Set Pp and OD_0 as output, OD_1 as input */
	uint8_t config = ~(outputMask | (openDrainMask & ~outputValue));
	uint8_t output = (outputMask & outputValue) & ~(openDrainMask & ~outputValue);

	uint16_t ret = config;
	ret <<= 8;
	ret |= output;
	return ret; // config, output
}

#define DEV_PCA9557_REG_INPUT    0x00
#define DEV_PCA9557_REG_OUTPUT   0x01
#define DEV_PCA9557_REG_POLARITY 0x02    // 0 - no inverted
#define DEV_PCA9557_REG_CONFIG   0x03    // 1 - input; 0 - output
#define DEV_PCA9557_REG_INT_MASK 0x45    // 1 - interrupt is masked; 0 - interrupt is enabled

/**
 * @brief Access PCA9557 register via I2C with retry mechanism
 * @param isRead True for read operation, false for write
 * @param slv I2C slave address
 * @param dev I2C device index
 * @param reg Register address
 * @param pByte Pointer to data byte
 * @return 0 on success, non-zero on failure
 */
static uint32_t _pca9557_access(bool isRead, uint8_t slv, uint8_t dev, uint8_t reg, uint8_t * pByte)
{
	uint8_t retry = 3;
	uint32_t ret;
	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
			ret  = i2c_hub_burst_read(dev, slv, reg, pByte, 1);
			if (ret != 0)
				continue;
		} else {
			ret = i2c_hub_burst_write(dev, slv, reg, pByte, 1);
		}
#if (CONFIG_LOG)
		LOG_CLEARBUF;
		for ( uint32_t i = 0; i < 1; i++ ) {
			LOG_APPEND(" %02X", *((uint8_t *)pByte + i));
		}
		LOG_DBG("Access IoExp @ %02X %s [%02X], ret %d, data(%d)%s", slv, isRead ? "R" : "W", reg, ret, 1, LOG_BUF);
#endif
			if (ret == 0)
			return ret;
	}
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] on IoExp @ %02X %s [%02X], ret %d", slv, isRead ? "R" : "W", reg, ret);
	}
	return ret;
}

/**
 * @brief Initialize PCA9557 IO expander device
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 * @param outputMask Push-pull output pin mask
 * @param openDrainMask Open-drain output pin mask
 * @param outputValue Initial output value
 */
void dev_pca9557_init( uint8_t slv, uint8_t i2cDev, uint8_t outputMask, uint8_t openDrainMask, uint8_t outputValue )
{
	DEV_PCA9557_SLV_PORT_MAP * pCfg = NULL;
	for (uint8_t i = 0; i < DEV_PCA9557_MAX_CHIP_NUM; i ++) {
		/* Check both I2C port and I2C address */
		if ((_s_slv2port[i].slv == slv) && (_s_slv2port[i].i2cDev == i2cDev)) {
			pCfg = &_s_slv2port[i];
			break;
		}
	}

	if (!pCfg) {
		for (uint8_t i = 0; i < DEV_PCA9557_MAX_CHIP_NUM; i ++) {
			/* Check both I2C port and I2C address */
			if ((_s_slv2port[i].slv == 0) && (_s_slv2port[i].i2cDev == 0)) {
				pCfg = &_s_slv2port[i];
				break;
			}
		}
	}

	if (pCfg) {
		k_mutex_init(&pCfg->opsLock);
		k_mutex_lock(&pCfg->opsLock, K_FOREVER);

		pCfg->slv = slv;
		pCfg->i2cDev = i2cDev;
		pCfg->outputVal = outputValue;
		pCfg->outputMask = outputMask;
		pCfg->openDrainMask = openDrainMask;
		pCfg->intMask = 0xFF;

		uint16_t setup = _dev_pca9557_makeup(outputMask, openDrainMask, outputValue);

		pCfg->polarity = 0;
		pCfg->lastSet = setup & 0xFF;
		pCfg->config = (setup >> 8) & 0xFF;

		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_POLARITY, &pCfg->polarity);
		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_OUTPUT, &pCfg->lastSet);
		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_CONFIG, &pCfg->config);

		k_mutex_unlock(&pCfg->opsLock);
		return;
	}

	/* _s_slv2port is full */
	assert (0);
}

/**
 * @brief Free PCA9557 device and reset configuration
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 */
void dev_pca9557_free( uint8_t slv , uint8_t i2cDev )
{
	for (uint8_t i = 0; i < DEV_PCA9557_MAX_CHIP_NUM; i ++) {
		/* Check both I2C port and I2C address */
		if ((_s_slv2port[i].slv == slv) && (_s_slv2port[i].i2cDev == i2cDev)) {
			DEV_PCA9557_SLV_PORT_MAP * pCfg = &_s_slv2port[i];

			pCfg->config = 0xFF;  /* All input */
			/* set all pin as INPUT */
			_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_CONFIG, &pCfg->config);

			pCfg->slv = 0;
			pCfg->i2cDev = 0;
			pCfg->outputVal = 0;
			pCfg->outputMask = 0;
			pCfg->openDrainMask = 0;
			pCfg->lastSet = 0;
			pCfg->polarity = 0;
			pCfg->intMask = 0xFF;

			break;
		}
	}
}

/**
 * @brief Enable interrupt for specified pins
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 * @param mask Pin mask to enable interrupt
 */
void dev_pca9557_intEnable(uint8_t slv, uint8_t i2cDev, uint8_t mask)
{
	DEV_PCA9557_SLV_PORT_MAP* pCfg = _dev_pca9557_getCfg(slv, i2cDev);

	if (!pCfg) return;

	uint8_t tmp = pCfg->intMask;
	tmp &= ~mask;
	if (DEV_PCA9557_FORCE_WRITE || tmp != pCfg->intMask) {
		pCfg->intMask = tmp;
		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_INT_MASK, &tmp);
	}
}

/**
 * @brief Mask interrupt for specified pins
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 * @param mask Pin mask to disable interrupt
 */
void dev_pca9557_intMask(uint8_t slv, uint8_t i2cDev, uint8_t mask)
{
	DEV_PCA9557_SLV_PORT_MAP* pCfg = _dev_pca9557_getCfg(slv, i2cDev);

	if (!pCfg) return;

	uint8_t tmp = pCfg->intMask;
	tmp |= mask;
	if (DEV_PCA9557_FORCE_WRITE || tmp != pCfg->intMask) {
		pCfg->intMask = tmp;
		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_INT_MASK, &tmp);
	}
}

/**
 * @brief Smart read that returns cached values for output pins
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 * @param mask Pin mask to read
 * @return Pin values with output pins from cache
 */
uint8_t dev_pca9557_smartRead(uint8_t slv, uint8_t i2cDev, uint8_t mask)
{
	/*
	 * This function returns lastSet for output or OpenDrain pins.
	 * It only guarantees the return value is up to date for bits which be set in 'mask'.
	 */

	DEV_PCA9557_SLV_PORT_MAP* pCfg = _dev_pca9557_getCfg(slv, i2cDev);

	if (pCfg) {
		k_mutex_lock(&pCfg->opsLock, K_FOREVER);

		uint8_t omsk = pCfg->outputMask | pCfg->openDrainMask;
		if (mask != (omsk & mask)) {
			/* not all pins were set as OUTPUT of this read, thus it has to refresh input value */
			_pca9557_access(1, slv, i2cDev, DEV_PCA9557_REG_INPUT, &pCfg->lastRead);
		}

		/* Use outputVal for OUTPUT pins */
		uint8_t tmp = pCfg->lastRead;
		tmp &= (~omsk);
		tmp |= (pCfg->outputVal & omsk);

		k_mutex_unlock(&pCfg->opsLock);
		return tmp;
	} else {
		assert (0);
		return 0xFF;
	}
}

/**
 * @brief Read input register from PCA9557
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 * @return Input register value
 */
uint8_t dev_pca9557_getInput(uint8_t slv, uint8_t i2cDev)
{
	DEV_PCA9557_SLV_PORT_MAP* pCfg = _dev_pca9557_getCfg(slv, i2cDev);

	if (pCfg) {
		k_mutex_lock(&pCfg->opsLock, K_FOREVER);
		_pca9557_access(1, slv, i2cDev, DEV_PCA9557_REG_INPUT, &pCfg->lastRead);
		k_mutex_unlock(&pCfg->opsLock);

		return pCfg->lastRead;
	} else {
		assert (0);
		return 0xFF;
	}
}

/**
 * @brief Set a single bit configuration and value
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 * @param idx Bit index (0-7)
 * @param isOutput True if push-pull output
 * @param isOd True if open-drain output
 * @param val Output value
 */
void dev_pca9557_setBit(uint8_t slv, uint8_t i2cDev, uint8_t idx, bool isOutput, bool isOd, bool val)
{
	uint8_t bitMask = 1 << idx;
	uint8_t outputMask = isOutput ? bitMask : 0;
	uint8_t openDrainMask = isOd ? bitMask : 0;
	uint8_t outputVal = val ? bitMask : 0;

	DEV_PCA9557_SLV_PORT_MAP* pCfg = _dev_pca9557_getCfg(slv, i2cDev);

	if (!pCfg) return;

	k_mutex_lock(&pCfg->opsLock, K_FOREVER);

	/* update outputVal */
	uint8_t tmp = pCfg->outputVal;
	uint8_t omsk = (outputMask | openDrainMask) & bitMask;
	tmp &= (~omsk);
	tmp |= (outputVal & omsk);
	pCfg->outputVal = tmp;

	uint16_t setup = _dev_pca9557_makeup(outputMask, openDrainMask, outputVal);

	/* set OUTPUT */
	tmp  = pCfg->lastSet;
	tmp &= (~bitMask);
	tmp |= (setup & bitMask);
	if (DEV_PCA9557_FORCE_WRITE || tmp != pCfg->lastSet) {
		pCfg->lastSet = tmp;
		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_OUTPUT, &tmp);
	}

	/* set CONFIG */
	tmp  = pCfg->config;
	tmp &= (~bitMask);
	tmp |= ((setup >> 8) & bitMask);
	if (DEV_PCA9557_FORCE_WRITE || tmp != pCfg->config) {
		pCfg->config = tmp;
		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_CONFIG, &tmp);
	}

	k_mutex_unlock(&pCfg->opsLock);
}

/**
 * @brief Set output values for multiple pins
 * @param slv I2C slave address
 * @param i2cDev I2C device index
 * @param mask Pin mask to modify
 * @param val Output values
 */
void dev_pca9557_setOutput(uint8_t slv, uint8_t i2cDev, uint8_t mask, uint8_t val)
{
	DEV_PCA9557_SLV_PORT_MAP* pCfg = _dev_pca9557_getCfg(slv, i2cDev);

	if (!pCfg) return;

	k_mutex_lock(&pCfg->opsLock, K_FOREVER);

	/* update outputVal */
	uint8_t tmp = pCfg->outputVal;
	uint8_t omsk = (pCfg->outputMask | pCfg->openDrainMask) & mask;
	tmp &= (~omsk);
	tmp |= (val & omsk);
	pCfg->outputVal = tmp;

	uint16_t setup = _dev_pca9557_makeup(pCfg->outputMask, pCfg->openDrainMask, val);

	/* set OUTPUT */
	tmp  = pCfg->lastSet;
	tmp &= (~mask);
	tmp |= (setup & mask);
	if (DEV_PCA9557_FORCE_WRITE || tmp != pCfg->lastSet) {
		pCfg->lastSet = tmp;
		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_OUTPUT, &tmp);
	}

	/* set CONFIG */
	tmp  = pCfg->config;
	tmp &= (~mask);
	tmp |= ((setup >> 8) & mask);
	if (DEV_PCA9557_FORCE_WRITE || tmp != pCfg->config) {
		pCfg->config = tmp;
		_pca9557_access(0, slv, i2cDev, DEV_PCA9557_REG_CONFIG, &tmp);
	}

	k_mutex_unlock(&pCfg->opsLock);
	return;
}