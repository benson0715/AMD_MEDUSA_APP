/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 *
 * OPT4048 Tristimulus Color Sensor Driver Implementation
 *
 * This driver implements I2C communication and color measurement functionality
 * for the Texas Instruments OPT4048 RGBW color sensor.
 *
 * The OPT4048 provides:
 * - 4 color channels: Red (Ch0), Green (Ch1), Blue (Ch2), White/Clear (Ch3)
 * - 20-bit ADC resolution with auto-ranging
 * - CIE 1931 XYZ color space calculations
 * - Lux measurement with 23-bit effective dynamic range
 * - Correlated Color Temperature (CCT) calculation
 *
 * Based on SparkFun OPT4048 Arduino Library:
 * https://github.com/sparkfun/SparkFun_OPT4048_Arduino_Library
 *
 * Reference: TI OPT4048 Datasheet (SBOS986)
 */

#include "dev_opt4048.h"
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "i2c_hub.h"

LOG_MODULE_REGISTER(opt4048, CONFIG_ALS_LOG_LEVEL);

/* Default I2C address for OPT4048 */
#ifndef OPT4048_I2C_ADDRESS
#define OPT4048_I2C_ADDRESS     0x44
#endif

/* Default I2C port */
#ifndef OPT4048_I2C_PORT
#define OPT4048_I2C_PORT        I2C_1
#endif

/* CIE 1931 Color Matching Function matrix for XYZ and Lux calculation 
 * From Table in 9.2.4 of OPT4048 Datasheet 
 * Matrix format: [row][col] where rows are X, Y, Z, Lux
 * Columns are coefficients for Ch0(R), Ch1(G), Ch2(B), Lux multiplier
 */
static const double cie_matrix[4][4] = {
	{0.000234892992, -0.0000189652390, 0.0000120811684, 0},        /* X coefficients */
	{0.0000407467441, 0.000198958202, -0.0000158848115, 0.00215},  /* Y coefficients (Lux) */
	{0.0000928619404, -0.0000169739553, 0.000674021520, 0},        /* Z coefficients */
	{0, 0, 0, 0}                                                    /* Reserved */
};

/**
 * @brief Access OPT4048 register via I2C
 * 
 * @param isRead true to read, false to write
 * @param reg Register address
 * @param pData Pointer to data buffer
 * @param len Data length in bytes
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_reg_access(bool isRead, uint8_t reg, uint8_t *pData, uint16_t len)
{
	uint8_t retry = 3;
	int ret = 0;

	while (retry) {
		retry--;
		ret = 0;
		
		if (isRead) {
			ret = i2c_hub_write_read(OPT4048_I2C_PORT, OPT4048_I2C_ADDRESS, 
			                         &reg, 1, pData, len);
		} else {
			ret = i2c_hub_burst_write_multi_cmd(OPT4048_I2C_PORT, OPT4048_I2C_ADDRESS,
			                                    &reg, 1, pData, len);
		}
		
		if (ret == 0) {
			LOG_DBG("OPT4048 %s reg[0x%02X], len=%d, ret=%d", 
			        isRead ? "R" : "W", reg, len, ret);
			return 0;
		}
	}
	
	if (!retry) {
		LOG_ERR("[!!Fatal error!!] on %s OPT4048[0x%02X], ret %d", 
		        isRead ? "R" : "W", reg, ret);
	}

	return ret;
}

/**
 * @brief Get OPT4048 device ID
 * 
 * @return Device ID or 0 on error
 */
uint16_t dev_opt4048_get_device_id(void)
{
	uint8_t buff[2];
	uint16_t uniqueId;
	int ret;
	dev_opt4048_reg_device_id_t idReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_DEVICE_ID, buff, 2);
	if (ret != 0) {
		return 0;
	}

	idReg.word = (buff[0] << 8) | buff[1];
	uniqueId = idReg.f.DIDH;

	LOG_INF("OPT4048 Device ID: 0x%04X", uniqueId);
	
	return uniqueId;
}

/**
 * @brief Initialize OPT4048 sensor
 * 
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_init(void)
{
	uint16_t devId;

	LOG_INF("Initializing OPT4048 sensor");

	/* Verify device ID */
	devId = dev_opt4048_get_device_id();
	if (devId != OPT4048_DEVICE_ID) {
		LOG_ERR("OPT4048 device ID mismatch: expected 0x%04X, got 0x%04X",
		        OPT4048_DEVICE_ID, devId);
		return -ENODEV;
	}

	/* Set basic configuration */
	if (dev_opt4048_set_basic_setup() != 0) {
		LOG_ERR("Failed to configure OPT4048");
		return -EIO;
	}

	LOG_INF("OPT4048 initialized successfully");
	return 0;
}

/**
 * @brief Set basic sensor configuration
 * 
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_basic_setup(void)
{
	int ret;

	/* Set range to 36 lux full scale */
	ret = dev_opt4048_set_range(OPT4048_RANGE_36LUX);
	if (ret != 0) {
		return ret;
	}

	/* Set conversion time to 200ms */
	ret = dev_opt4048_set_conversion_time(OPT4048_CONVERSION_TIME_200MS);
	if (ret != 0) {
		return ret;
	}

	/* Set continuous operation mode */
	ret = dev_opt4048_set_operation_mode(OPT4048_OPERATION_MODE_CONTINUOUS);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

/**
 * @brief Set measurement range
 * 
 * @param range Range setting
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_range(uint8_t range)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_control_t controlReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	controlReg.f.range = range;

	buff[0] = controlReg.word >> 8;
	buff[1] = controlReg.word & 0xFF;

	ret = dev_opt4048_reg_access(false, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	LOG_DBG("OPT4048 range set to: %d", range);
	return 0;
}

/**
 * @brief Get measurement range
 * 
 * @return Current range setting
 */
uint8_t dev_opt4048_get_range(void)
{
	uint8_t buff[2];
	dev_opt4048_reg_control_t controlReg;

	if (dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2) != 0) {
		return 0;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	return controlReg.f.range;
}

/**
 * @brief Set conversion time
 * 
 * @param time Conversion time setting
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_conversion_time(uint8_t time)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_control_t controlReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	controlReg.f.conversion_time = time;

	buff[0] = controlReg.word >> 8;
	buff[1] = controlReg.word & 0xFF;

	ret = dev_opt4048_reg_access(false, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	LOG_DBG("OPT4048 conversion time set to: %d", time);
	return 0;
}

/**
 * @brief Get conversion time
 * 
 * @return Current conversion time setting
 */
uint8_t dev_opt4048_get_conversion_time(void)
{
	uint8_t buff[2];
	dev_opt4048_reg_control_t controlReg;

	if (dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2) != 0) {
		return 0;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	return controlReg.f.conversion_time;
}

/**
 * @brief Set operation mode
 * 
 * @param mode Operation mode
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_operation_mode(uint8_t mode)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_control_t controlReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	controlReg.f.op_mode = mode;

	buff[0] = controlReg.word >> 8;
	buff[1] = controlReg.word & 0xFF;

	ret = dev_opt4048_reg_access(false, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	LOG_DBG("OPT4048 operation mode set to: %d", mode);
	return 0;
}

/**
 * @brief Get operation mode
 * 
 * @return Current operation mode
 */
uint8_t dev_opt4048_get_operation_mode(void)
{
	uint8_t buff[2];
	dev_opt4048_reg_control_t controlReg;

	if (dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2) != 0) {
		return 0;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	return controlReg.f.op_mode;
}

/**
 * @brief Set Quick Wake mode
 * 
 * @param enable true to enable, false to disable
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_qwake(bool enable)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_control_t controlReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	controlReg.f.qwake = enable ? 1 : 0;

	buff[0] = controlReg.word >> 8;
	buff[1] = controlReg.word & 0xFF;

	ret = dev_opt4048_reg_access(false, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

/**
 * @brief Get ADC value for channel 0 (red)
 * 
 * @return ADC code for channel 0
 */
uint32_t dev_opt4048_get_adc_ch0(void)
{
	uint8_t buff[4];
	uint32_t adcCode;
	uint32_t mantissa;
	dev_opt4048_reg_exp_res_ch0_t adcReg;
	dev_opt4048_reg_res_cnt_crc_ch0_t adc1Reg;

	if (dev_opt4048_reg_access(true, OPT4048_REG_EXP_RES_CH0, buff, 4) != 0) {
		return 0;
	}

	adcReg.word = (buff[0] << 8) | buff[1];
	adc1Reg.word = (buff[2] << 8) | buff[3];

	mantissa = ((uint32_t)adcReg.f.result_msb_ch0 << 8) | adc1Reg.f.result_lsb_ch0;
	adcCode = mantissa << adcReg.f.exponent_ch0;

	return adcCode;
}

/**
 * @brief Get ADC value for channel 1 (green)
 * 
 * @return ADC code for channel 1
 */
uint32_t dev_opt4048_get_adc_ch1(void)
{
	uint8_t buff[4];
	uint32_t adcCode;
	uint32_t mantissa;
	dev_opt4048_reg_exp_res_ch1_t adcReg;
	dev_opt4048_reg_res_cnt_crc_ch1_t adc1Reg;

	if (dev_opt4048_reg_access(true, OPT4048_REG_EXP_RES_CH1, buff, 4) != 0) {
		return 0;
	}

	adcReg.word = (buff[0] << 8) | buff[1];
	adc1Reg.word = (buff[2] << 8) | buff[3];

	mantissa = ((uint32_t)adcReg.f.result_msb_ch1 << 8) | adc1Reg.f.result_lsb_ch1;
	adcCode = mantissa << adcReg.f.exponent_ch1;

	return adcCode;
}

/**
 * @brief Get ADC value for channel 2 (blue)
 * 
 * @return ADC code for channel 2
 */
uint32_t dev_opt4048_get_adc_ch2(void)
{
	uint8_t buff[4];
	uint32_t adcCode;
	uint32_t mantissa;
	dev_opt4048_reg_exp_res_ch2_t adcReg;
	dev_opt4048_reg_res_cnt_crc_ch2_t adc1Reg;

	if (dev_opt4048_reg_access(true, OPT4048_REG_EXP_RES_CH2, buff, 4) != 0) {
		return 0;
	}

	adcReg.word = (buff[0] << 8) | buff[1];
	adc1Reg.word = (buff[2] << 8) | buff[3];

	mantissa = ((uint32_t)adcReg.f.result_msb_ch2 << 8) | adc1Reg.f.result_lsb_ch2;
	adcCode = mantissa << adcReg.f.exponent_ch2;

	return adcCode;
}

/**
 * @brief Get ADC value for channel 3 (white)
 * 
 * @return ADC code for channel 3
 */
uint32_t dev_opt4048_get_adc_ch3(void)
{
	uint8_t buff[4];
	uint32_t adcCode;
	uint32_t mantissa;
	dev_opt4048_reg_exp_res_ch3_t adcReg;
	dev_opt4048_reg_res_cnt_crc_ch3_t adc1Reg;

	if (dev_opt4048_reg_access(true, OPT4048_REG_EXP_RES_CH3, buff, 4) != 0) {
		return 0;
	}

	adcReg.word = (buff[0] << 8) | buff[1];
	adc1Reg.word = (buff[2] << 8) | buff[3];

	mantissa = ((uint32_t)adcReg.f.result_msb_ch3 << 8) | adc1Reg.f.result_lsb_ch3;
	adcCode = mantissa << adcReg.f.exponent_ch3;

	return adcCode;
}

/**
 * @brief Get all channel data in a single read
 * 
 * @param color Pointer to color structure to fill
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_get_all_channels(opt4048_color_t *color)
{
	int ret;
	uint8_t buff[16];
	uint32_t mantissaCh0, mantissaCh1, mantissaCh2, mantissaCh3;

	dev_opt4048_reg_exp_res_ch0_t adc0MSB;
	dev_opt4048_reg_res_cnt_crc_ch0_t adc0LSB;
	dev_opt4048_reg_exp_res_ch1_t adc1MSB;
	dev_opt4048_reg_res_cnt_crc_ch1_t adc1LSB;
	dev_opt4048_reg_exp_res_ch2_t adc2MSB;
	dev_opt4048_reg_res_cnt_crc_ch2_t adc2LSB;
	dev_opt4048_reg_exp_res_ch3_t adc3MSB;
	dev_opt4048_reg_res_cnt_crc_ch3_t adc3LSB;

	if (color == NULL) {
		return -EINVAL;
	}

	/* Read all 4 channels in one burst (16 bytes) */
	ret = dev_opt4048_reg_access(true, OPT4048_REG_EXP_RES_CH0, buff, 16);
	if (ret != 0) {
		return ret;
	}

	/* Parse channel 0 (red) */
	adc0MSB.word = (buff[0] << 8) | buff[1];
	adc0LSB.word = (buff[2] << 8) | buff[3];
	mantissaCh0 = ((uint32_t)adc0MSB.f.result_msb_ch0 << 8) | adc0LSB.f.result_lsb_ch0;
	color->red = mantissaCh0 << adc0MSB.f.exponent_ch0;
	color->counterR = adc0LSB.f.counter_ch0;
	color->CRCR = adc0LSB.f.crc_ch0;

	/* Parse channel 1 (green) */
	adc1MSB.word = (buff[4] << 8) | buff[5];
	adc1LSB.word = (buff[6] << 8) | buff[7];
	mantissaCh1 = ((uint32_t)adc1MSB.f.result_msb_ch1 << 8) | adc1LSB.f.result_lsb_ch1;
	color->green = mantissaCh1 << adc1MSB.f.exponent_ch1;
	color->counterG = adc1LSB.f.counter_ch1;
	color->CRCG = adc1LSB.f.crc_ch1;

	/* Parse channel 2 (blue) */
	adc2MSB.word = (buff[8] << 8) | buff[9];
	adc2LSB.word = (buff[10] << 8) | buff[11];
	mantissaCh2 = ((uint32_t)adc2MSB.f.result_msb_ch2 << 8) | adc2LSB.f.result_lsb_ch2;
	color->blue = mantissaCh2 << adc2MSB.f.exponent_ch2;
	color->counterB = adc2LSB.f.counter_ch2;
	color->CRCB = adc2LSB.f.crc_ch2;

	/* Parse channel 3 (white) */
	adc3MSB.word = (buff[12] << 8) | buff[13];
	adc3LSB.word = (buff[14] << 8) | buff[15];
	mantissaCh3 = ((uint32_t)adc3MSB.f.result_msb_ch3 << 8) | adc3LSB.f.result_lsb_ch3;
	color->white = mantissaCh3 << adc3MSB.f.exponent_ch3;
	color->counterW = adc3LSB.f.counter_ch3;
	color->CRCW = adc3LSB.f.crc_ch3;

	LOG_DBG("OPT4048 R:%u G:%u B:%u W:%u", 
	        color->red, color->green, color->blue, color->white);

	return 0;
}

/**
 * @brief Get lux value
 * 
 * @return Lux value (scaled by CIE Y coefficient)
 */
uint32_t dev_opt4048_get_lux(void)
{
	uint32_t adcCh1;
	uint32_t lux;

	adcCh1 = dev_opt4048_get_adc_ch1();
	
	/* Calculate lux using CIE Y coefficient for green channel */
	lux = (uint32_t)(adcCh1 * cie_matrix[1][3]);

	return lux;
}

/**
 * @brief Get CIE 1931 x chromaticity coordinate
 * 
 * @return CIE x value (0.0 to 1.0)
 */
double dev_opt4048_get_cie_x(void)
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
	opt4048_color_t color;

	if (dev_opt4048_get_all_channels(&color) != 0) {
		return 0.0;
	}

	/* Calculate XYZ tristimulus values using CIE matrix */
	x = color.red * cie_matrix[0][0] + color.green * cie_matrix[0][1] + color.blue * cie_matrix[0][2];
	y = color.red * cie_matrix[1][0] + color.green * cie_matrix[1][1] + color.blue * cie_matrix[1][2];
	z = color.red * cie_matrix[2][0] + color.green * cie_matrix[2][1] + color.blue * cie_matrix[2][2];

	double sum = x + y + z;
	if (sum == 0.0) {
		return 0.0;
	}

	return x / sum;
}

/**
 * @brief Get CIE 1931 y chromaticity coordinate
 * 
 * @return CIE y value (0.0 to 1.0)
 */
double dev_opt4048_get_cie_y(void)
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
	opt4048_color_t color;

	if (dev_opt4048_get_all_channels(&color) != 0) {
		return 0.0;
	}

	/* Calculate XYZ tristimulus values using CIE matrix */
	x = color.red * cie_matrix[0][0] + color.green * cie_matrix[0][1] + color.blue * cie_matrix[0][2];
	y = color.red * cie_matrix[1][0] + color.green * cie_matrix[1][1] + color.blue * cie_matrix[1][2];
	z = color.red * cie_matrix[2][0] + color.green * cie_matrix[2][1] + color.blue * cie_matrix[2][2];

	double sum = x + y + z;
	if (sum == 0.0) {
		return 0.0;
	}

	return y / sum;
}

/**
 * @brief Get Correlated Color Temperature (CCT)
 * 
 * @return CCT value in Kelvin
 */
double dev_opt4048_get_cct(void)
{
	double cie_x, cie_y;
	double n;
	double cct;

	cie_x = dev_opt4048_get_cie_x();
	cie_y = dev_opt4048_get_cie_y();

	/* McCamy's approximation formula for CCT calculation */
	n = (cie_x - 0.3320) / (0.1858 - cie_y);

	/* CCT formula from datasheet Section 9.2.4 */
	cct = 437.0 * n * n * n + 3601.0 * n * n + 6861.0 * n + 5517.0;

	return cct;
}

/**
 * @brief Get all flags register
 * 
 * @return Flags register structure
 */
dev_opt4048_reg_flags_t dev_opt4048_get_all_flags(void)
{
	uint8_t buff[2];
	dev_opt4048_reg_flags_t flagReg;

	flagReg.word = 0;
	
	if (dev_opt4048_reg_access(true, OPT4048_REG_FLAGS, buff, 2) == 0) {
		flagReg.word = (buff[0] << 8) | buff[1];
	}

	return flagReg;
}

/**
 * @brief Get conversion ready flag
 * 
 * @return true if conversion is ready, false otherwise
 */
bool dev_opt4048_get_conv_ready_flag(void)
{
	dev_opt4048_reg_flags_t flagReg;
	
	flagReg = dev_opt4048_get_all_flags();
	return (flagReg.f.conv_ready_flag == 1);
}

/**
 * @brief Get overload flag
 * 
 * @return true if overload detected, false otherwise
 */
bool dev_opt4048_get_overload_flag(void)
{
	dev_opt4048_reg_flags_t flagReg;
	
	flagReg = dev_opt4048_get_all_flags();
	return (flagReg.f.overload_flag == 1);
}

/**
 * @brief Get too bright flag
 * 
 * @return true if above high threshold, false otherwise
 */
bool dev_opt4048_get_too_bright_flag(void)
{
	dev_opt4048_reg_flags_t flagReg;
	
	flagReg = dev_opt4048_get_all_flags();
	return (flagReg.f.flag_high == 1);
}

/**
 * @brief Get too dim flag
 * 
 * @return true if below low threshold, false otherwise
 */
bool dev_opt4048_get_too_dim_flag(void)
{
	dev_opt4048_reg_flags_t flagReg;
	
	flagReg = dev_opt4048_get_all_flags();
	return (flagReg.f.flag_low == 1);
}

/**
 * @brief Set fault count
 * 
 * @param count Fault count setting (use OPT4048_FAULT_COUNT_* defines)
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_fault_count(uint8_t count)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_control_t controlReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	controlReg.f.fault_count = count;

	buff[0] = controlReg.word >> 8;
	buff[1] = controlReg.word & 0xFF;

	ret = dev_opt4048_reg_access(false, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

/**
 * @brief Get fault count
 * 
 * @return Current fault count setting
 */
uint8_t dev_opt4048_get_fault_count(void)
{
	uint8_t buff[2];
	dev_opt4048_reg_control_t controlReg;

	if (dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2) != 0) {
		return 0;
	}

	controlReg.word = (buff[0] << 8) | buff[1];
	return controlReg.f.fault_count;
}

/**
 * @brief Set low threshold for hysteresis
 * 
 * @param threshold_code Threshold mantissa (12-bit)
 * @param exponent Exponent value (4-bit)
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_threshold_low(uint16_t threshold_code, uint8_t exponent)
{
	uint8_t buff[2];
	dev_opt4048_reg_thresh_exp_res_t threshReg;

	threshReg.f.thresh_result = threshold_code & 0xFFF;
	threshReg.f.thresh_exp = exponent & 0x0F;

	buff[0] = threshReg.word >> 8;
	buff[1] = threshReg.word & 0xFF;

	return dev_opt4048_reg_access(false, OPT4048_REG_THRESH_L_EXP_RES, buff, 2);
}

/**
 * @brief Set high threshold for hysteresis
 * 
 * @param threshold_code Threshold mantissa (12-bit)
 * @param exponent Exponent value (4-bit)
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_threshold_high(uint16_t threshold_code, uint8_t exponent)
{
	uint8_t buff[2];
	dev_opt4048_reg_thresh_exp_res_t threshReg;

	threshReg.f.thresh_result = threshold_code & 0xFFF;
	threshReg.f.thresh_exp = exponent & 0x0F;

	buff[0] = threshReg.word >> 8;
	buff[1] = threshReg.word & 0xFF;

	return dev_opt4048_reg_access(false, OPT4048_REG_THRESH_H_EXP_RES, buff, 2);
}

/**
 * @brief Set interrupt configuration mode
 * 
 * @param int_cfg Interrupt configuration
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_int_cfg(uint8_t int_cfg)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_int_control_t intReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_INT_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	intReg.word = (buff[0] << 8) | buff[1];
	intReg.f.int_cfg = int_cfg & 0x03;

	buff[0] = intReg.word >> 8;
	buff[1] = intReg.word & 0xFF;

	return dev_opt4048_reg_access(false, OPT4048_REG_INT_CONTROL, buff, 2);
}

/**
 * @brief Enable/disable interrupt latching
 * 
 * @param enable true to latch, false for transparent
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_latch(bool enable)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_control_t ctrlReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	ctrlReg.word = (buff[0] << 8) | buff[1];
	ctrlReg.f.latch = enable ? 1 : 0;

	buff[0] = ctrlReg.word >> 8;
	buff[1] = ctrlReg.word & 0xFF;

	return dev_opt4048_reg_access(false, OPT4048_REG_CONTROL, buff, 2);
}

/**
 * @brief Trigger one-shot measurement
 * 
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_trigger_one_shot(void)
{
	return dev_opt4048_set_operation_mode(OPT4048_OPERATION_MODE_ONE_SHOT);
}

/**
 * @brief Set threshold channel
 * 
 * @param channel Threshold channel (use OPT4048_THRESHOLD_CH_* defines)
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_threshold_channel(uint8_t channel)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_int_control_t intReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_INT_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	intReg.word = (buff[0] << 8) | buff[1];
	intReg.f.threshold_ch_sel = channel;

	buff[0] = intReg.word >> 8;
	buff[1] = intReg.word & 0xFF;

	ret = dev_opt4048_reg_access(false, OPT4048_REG_INT_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

/**
 * @brief Set I2C burst mode
 * 
 * @param enable true to enable burst mode, false to disable
 * @return 0 on success, negative error code on failure
 */
int dev_opt4048_set_i2c_burst(bool enable)
{
	uint8_t buff[2];
	int ret;
	dev_opt4048_reg_int_control_t intReg;

	ret = dev_opt4048_reg_access(true, OPT4048_REG_INT_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	intReg.word = (buff[0] << 8) | buff[1];
	intReg.f.i2c_burst = enable ? 1 : 0;

	buff[0] = intReg.word >> 8;
	buff[1] = intReg.word & 0xFF;

	ret = dev_opt4048_reg_access(false, OPT4048_REG_INT_CONTROL, buff, 2);
	if (ret != 0) {
		return ret;
	}

	return 0;
}