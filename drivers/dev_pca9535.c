/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "dev_pca9535.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include <dev_utility.h>
#include "i2c_hub.h"
#include "debug_info.h"
LOG_MODULE_REGISTER(pca9535, CONFIG_PCA9535_LOG_LEVEL);

/*
 * PCA9557 inner structure
 */
#ifndef DEV_PCA9535_MAX_CHIP_NUM
#define DEV_PCA9535_MAX_CHIP_NUM     4
#endif

#ifndef DEV_PCA9535_RETRY_NUM
#define DEV_PCA9535_RETRY_NUM        3
#endif

typedef struct {
	uint8_t slv;
    uint8_t i2cDev;
	
	/*P1_7, P1_6, P1_5.......P0_2, P0_1, P0_0*/
	uint16_t lastRead;               // DEV_PCA9557_REG_INPUT    0x00
	uint16_t lastSet;                // DEV_PCA9557_REG_OUTPUT   0x02
	uint16_t polarity;               // DEV_PCA9557_REG_POLARITY 0x04
	uint16_t config;                 // DEV_PCA9557_REG_CONFIG   0x06
	uint16_t ier;                    // DEV_PCA9557_REG_INT_EN   0x0A

	/* pin setup */
	uint16_t outputVal;
	uint16_t outputMask;
	uint16_t openDrainMask;

	struct k_mutex opsLock;
} DEV_PCA9535_SLV_PORT_MAP;

/*
 * I2C controller allocation
 */
#define DEV_PCA9535_FORCE_WRITE     0

uint32_t g_ioExpErrCnt = 0;

static DEV_PCA9535_SLV_PORT_MAP _s_slv2port[DEV_PCA9535_MAX_CHIP_NUM] = {
	{0, 0}, {0, 0}, {0, 0}, {0, 0}
};

/**
 * @brief Get PCA9535 configuration by slave address and I2C device
 */
static DEV_PCA9535_SLV_PORT_MAP* _dev_pca9535_getCfg (uint8_t slv, uint8_t i2cDev)
{
	for (uint8_t i = 0; i < DEV_PCA9535_MAX_CHIP_NUM; i ++) {
		/* Check both I2C port and I2C address */
		if ((_s_slv2port[i].slv == slv) && (_s_slv2port[i].i2cDev == i2cDev)) {
			return &_s_slv2port[i];
		}
	}

	return NULL;
}

/**
 * @brief Calculate config and output register values from masks
 */
static uint32_t _dev_pca9535_makeup(uint16_t outputMask, uint16_t openDrainMask, uint16_t outputValue)
{
	if (outputMask & openDrainMask) {
		/* One pin be set as both Push-pull and OD */
		assert (0);
	}

	/* Set Pp and OD_0 as output, OD_1 as input */
	uint16_t config = ~(outputMask | (openDrainMask & ~outputValue));
	uint16_t output = (outputMask & outputValue) & ~(openDrainMask & ~outputValue);

	uint32_t ret = config;
	ret <<= 16;
	ret |= output;
	return ret; // config, output
}

/**
 * @brief Access PCA9535 register via I2C read or write
 */
static uint32_t _pca9535_access(bool isRead, uint8_t slv, uint8_t i2cDev, uint8_t reg, uint16_t * pData)
{
	uint8_t retry = 10;
	uint32_t ret = 0;
	uint8_t byte[2] = {0};
	
	while (retry) {
		retry --;
		ret = 0;
		if (isRead) {
				ret  = i2c_hub_burst_read(i2cDev, slv, reg, byte, 2);
				if (ret != 0)
					continue;
				*pData = ((uint16_t)byte[1] << 8) + byte[0];
		} else {
			byte[0] = (*pData) & 0xFF; /* Port0 */
			byte[1] = ((*pData) >> 8) & 0xFF; /* Port1 */
			ret  = i2c_hub_burst_write(i2cDev, slv, reg, byte, 2);
		}
#if (CONFIG_LOG)
		LOG_CLEARBUF;
		for (uint32_t i = 0; i < 2; i++)
		{
			LOG_APPEND(" %02X", *((uint8_t *)pData + i));
		}
		LOG_DBG("Access IoExp @ %02X %s [%02X], ret %d, data(%d)%s", slv, isRead ? "R" : "W", reg, ret, 1, LOG_BUF);
#endif
		if (ret == 0)
			return ret;
	}
	if (!retry) {
		info_value_increase(INFO_I2C_PCA9535, 1);
		LOG_ERR("[!!Fatal error!!] on IoExp @ %02X %s [%02X], ret %d", slv, isRead ? "R" : "W", reg, ret);
	}
	return ret;
}

/**
 * @brief Initialize PCA9535 IO expander device
 */
void dev_pca9535_init( uint8_t slv, uint8_t i2cDev, uint16_t outputMask, uint16_t openDrainMask, uint16_t outputValue, uint16_t ier )
{
	DEV_PCA9535_SLV_PORT_MAP * pCfg = NULL;
	uint32_t ret = 0;

	for (uint8_t i = 0; i < DEV_PCA9535_MAX_CHIP_NUM; i ++) {
		/* Check both I2C port and I2C address */
		if ((_s_slv2port[i].slv == slv) && (_s_slv2port[i].i2cDev == i2cDev)) {
			pCfg = &_s_slv2port[i];
			break;
		}
	}

	if (!pCfg) {
		for (uint8_t i = 0; i < DEV_PCA9535_MAX_CHIP_NUM; i ++) {
			/* Check both I2C port and I2C address */
			if ((_s_slv2port[i].slv == 0) && (_s_slv2port[i].i2cDev == 0)) {
				pCfg = &_s_slv2port[i];
				break;
			}
		}
	}

	if (pCfg) {
		k_mutex_init(&pCfg->opsLock);
		k_mutex_lock(&pCfg->opsLock,K_FOREVER);

		pCfg->slv = slv;
		pCfg->i2cDev = i2cDev;
		pCfg->outputVal = outputValue;
		pCfg->outputMask = outputMask;
		pCfg->openDrainMask = openDrainMask;
		pCfg->ier = ier;

		uint32_t setup = _dev_pca9535_makeup(outputMask, openDrainMask, outputValue);

		pCfg->polarity = 0;
		pCfg->lastSet = setup & 0xFFFF;
		pCfg->config = (setup >> 16) & 0xFFFF;

		for (uint8_t i = 0; i < DEV_PCA9535_RETRY_NUM; i++) {

			ret  = _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_POLARITY1, &pCfg->polarity);
			ret |= _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_OUTPUT1, &pCfg->lastSet);
			ret |= _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_CONFIG1, &pCfg->config);

			if (ret!=0) {
				g_ioExpErrCnt++;
			} else {
				break;
			}

		}

		k_mutex_unlock(&pCfg->opsLock);
		return;
	}

	/* _s_slv2port is full */
	assert (0);
}

/**
 * @brief Enable interrupt for PCA9535 device
 */
void dev_pca9535_intEnable(uint8_t slv, uint8_t i2cDev)
{
	DEV_PCA9535_SLV_PORT_MAP* pCfg = _dev_pca9535_getCfg(slv, i2cDev);
	uint32_t ret = 0;
	
	// pCfg->ier = 0xFFFF;

	if (pCfg) {
		if (!k_is_in_isr()) {
			k_mutex_lock(&pCfg->opsLock, K_FOREVER);
		}

		ret = _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_IER1, &pCfg->ier);

		if (!k_is_in_isr()) {
			k_mutex_unlock(&pCfg->opsLock);
		}
	} 
}

/**
 * @brief Free PCA9535 device and reset configuration
 */
void dev_pca9535_free( uint8_t slv , uint8_t i2cDev )
{
	uint32_t ret = 0;

	for (uint8_t i = 0; i < DEV_PCA9535_MAX_CHIP_NUM; i ++) {
		/* Check both I2C port and I2C address */
		if ((_s_slv2port[i].slv == slv) && (_s_slv2port[i].i2cDev == i2cDev)) {
			DEV_PCA9535_SLV_PORT_MAP * pCfg = &_s_slv2port[i];

			pCfg->config = 0xFFFF;  /* All input */
			/* set all pin as INPUT */
			ret = _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_CONFIG1, &pCfg->config);
			if (ret!=0) {
				g_ioExpErrCnt ++;
			}

			pCfg->slv = 0;
			pCfg->i2cDev = 0;
			pCfg->outputVal = 0;
			pCfg->outputMask = 0;
			pCfg->openDrainMask = 0;
			pCfg->lastSet = 0;
			pCfg->polarity = 0;

			break;
		}
	}
}

/**
 * @brief Smart read from PCA9535 with cached output values
 */
uint16_t dev_pca9535_smartRead(uint8_t slv, uint8_t i2cDev, uint16_t mask)
{
	/*
	 * This function returns lastSet for output or OpenDrain pins.
	 * It only guarantees the return value is up to date for bits which be set in 'mask'.
	 */

	DEV_PCA9535_SLV_PORT_MAP* pCfg = _dev_pca9535_getCfg(slv, i2cDev);
	uint32_t ret = 0;

	if (pCfg) {
		if (!k_is_in_isr()) {
			k_mutex_lock(&pCfg->opsLock,K_FOREVER);
		}

		uint16_t omsk = pCfg->outputMask | pCfg->openDrainMask;
		if (mask != (omsk & mask)) {
			for (uint8_t i = 0; i < DEV_PCA9535_RETRY_NUM; i++) {
				/* not all pins were set as OUTPUT of this read, thus it has to refresh input value */
				ret = _pca9535_access(1, slv, i2cDev, DEV_PCA9535_REG_INPUT1, &pCfg->lastRead);
				if (ret!=0) {
					g_ioExpErrCnt++;
				} else {
					break;
				}
			}
		}

		/* Use outputVal for OUTPUT pins */
		uint16_t tmp = pCfg->lastRead;
		tmp &= (~omsk);
		tmp |= (pCfg->outputVal & omsk);

		if (!k_is_in_isr()) {
			k_mutex_unlock(&pCfg->opsLock);
		}

		return tmp;
	} else {
		assert (0);
		return 0xFFFF;
	}
}

/**
 * @brief Read from buffer without I2C access
 */
uint16_t dev_pca9535_bufferRead(uint8_t slv, uint8_t i2cDev, uint16_t mask)
{
	/*
	 * This function returns lastSet for output or OpenDrain pins.
	 * It only guarantees the return value is up to date for bits which be set in 'mask'.
	 */

	DEV_PCA9535_SLV_PORT_MAP* pCfg = _dev_pca9535_getCfg(slv, i2cDev);

	if (pCfg) {

		uint16_t omsk = pCfg->outputMask | pCfg->openDrainMask;

		/* Use outputVal for OUTPUT pins */
		uint16_t tmp = pCfg->lastRead;
		tmp &= (~omsk);
		tmp |= (pCfg->outputVal & omsk);

		return tmp;
	} else {
		assert (0);
		return 0xFFFF;
	}
}

/**
 * @brief Get input register value from PCA9535
 */
uint16_t dev_pca9535_getInput(uint8_t slv, uint8_t i2cDev)
{
	DEV_PCA9535_SLV_PORT_MAP* pCfg = _dev_pca9535_getCfg(slv, i2cDev);
	uint32_t ret = 0;

	if (pCfg) {
		if (!k_is_in_isr()) {
			k_mutex_lock(&pCfg->opsLock,K_FOREVER);
		}
		for (uint8_t i = 0; i < DEV_PCA9535_RETRY_NUM; i++) {
			ret = _pca9535_access(1, slv, i2cDev, DEV_PCA9535_REG_INPUT1, &pCfg->lastRead);
			if (ret!=0) {
				g_ioExpErrCnt++;
			} else {
				break;
			}
		}
		if (!k_is_in_isr()) {
			k_mutex_unlock(&pCfg->opsLock);
		}
		return pCfg->lastRead;
	} else {
		assert (0);
		return 0xFFFF;
	}
}

/**
 * @brief Set push-pull or open-drain mode for a pin
 */
void dev_pca9535_setPuOd(uint8_t slv, uint8_t i2cDev, uint16_t idx, bool isOutput, bool isOd)
{
	uint16_t bitMask = 1 << idx;
	uint16_t outputMask = isOutput ? bitMask : 0;
	uint16_t openDrainMask = isOd ? bitMask : 0;
	
	DEV_PCA9535_SLV_PORT_MAP* pCfg = _dev_pca9535_getCfg(slv, i2cDev);
	
	/* save the output and openDrain mask */
	pCfg->outputMask |= outputMask;
	pCfg->outputMask &= (~openDrainMask);
	
	pCfg->openDrainMask |= openDrainMask;
	pCfg->openDrainMask &= (~outputMask);
}

/**
 * @brief Set a single bit output value on PCA9535
 */
void dev_pca9535_setBit(uint8_t slv, uint8_t i2cDev, uint16_t idx, bool isOutput, bool isOd, bool val)
{
	uint16_t bitMask = 1 << idx;
	uint16_t outputMask = isOutput ? bitMask : 0;
	uint16_t openDrainMask = isOd ? bitMask : 0;
	uint16_t outputVal = val ? bitMask : 0;
	uint32_t ret = 0;

	DEV_PCA9535_SLV_PORT_MAP* pCfg = _dev_pca9535_getCfg(slv, i2cDev);

	if (!pCfg) return;

	if (!k_is_in_isr()) {
		k_mutex_lock(&pCfg->opsLock,K_FOREVER);
	}
	/* update outputVal */
	uint16_t tmp = pCfg->outputVal;
	uint16_t omsk = (outputMask | openDrainMask) & bitMask;
	tmp &= (~omsk);
	tmp |= (outputVal & omsk);
	pCfg->outputVal = tmp;

	uint32_t setup = _dev_pca9535_makeup(outputMask, openDrainMask, outputVal);

	/* set OUTPUT */
	uint16_t opt  = pCfg->lastSet;
	opt &= (~bitMask);
	opt |= (setup & bitMask);

	/* set CONFIG */
	uint16_t cfg  = pCfg->config;
	cfg &= (~bitMask);
	cfg |= ((setup >> 16) & bitMask);

	if (DEV_PCA9535_FORCE_WRITE || opt != pCfg->lastSet || cfg != pCfg->config) {

		for (uint8_t i = 0; i < DEV_PCA9535_RETRY_NUM; i++) {

			ret  = _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_OUTPUT1, &opt);
			ret |= _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_CONFIG1, &cfg);

			if (ret!=0) {
				g_ioExpErrCnt++;
			} else {
				break;
			}
		}

		pCfg->lastSet = opt;
		pCfg->config = cfg;
	}
	if (!k_is_in_isr()) {
		k_mutex_unlock(&pCfg->opsLock);
	}
}

/**
 * @brief Set output values for multiple pins on PCA9535
 */
void dev_pca9535_setOutput(uint8_t slv, uint8_t i2cDev, uint16_t mask, uint16_t val)
{
	DEV_PCA9535_SLV_PORT_MAP* pCfg = _dev_pca9535_getCfg(slv, i2cDev);
	uint32_t ret = 0;

	if (!pCfg) return;

	if (!k_is_in_isr()) {
		k_mutex_lock(&pCfg->opsLock,K_FOREVER);
	}
	/* update outputVal */
	uint16_t tmp = pCfg->outputVal;
	uint16_t omsk = (pCfg->outputMask | pCfg->openDrainMask) & mask;
	tmp &= (~omsk);
	tmp |= (val & omsk);
	pCfg->outputVal = tmp;

	uint32_t setup = _dev_pca9535_makeup(pCfg->outputMask, pCfg->openDrainMask, val);

	/* set OUTPUT */
	uint16_t opt  = pCfg->lastSet;
	opt &= (~mask);
	opt |= (setup & mask);

	/* set CONFIG */
	uint16_t cfg  = pCfg->config;
	cfg &= (~mask);
	cfg |= ((setup >> 16) & mask);

	if (DEV_PCA9535_FORCE_WRITE || opt != pCfg->lastSet || cfg != pCfg->config) {

		for (uint8_t i = 0; i < DEV_PCA9535_RETRY_NUM; i++) {

			ret  = _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_OUTPUT1, &opt);
			ret |= _pca9535_access(0, slv, i2cDev, DEV_PCA9535_REG_CONFIG1, &cfg);

			if (ret!=0) {
				g_ioExpErrCnt++;
			} else {
				break;
			}
		}

		pCfg->lastSet = opt;
		pCfg->config = cfg;
	}
	if (!k_is_in_isr()) {
		k_mutex_unlock(&pCfg->opsLock);
	}
	return;
}