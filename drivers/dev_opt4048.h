/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 *
 * OPT4048 Tristimulus Color Sensor Driver for Zephyr RTOS
 *
 * This driver provides support for the Texas Instruments OPT4048 RGBW color sensor.
 * The OPT4048 is a high-precision tristimulus XYZ sensor with I2C interface that
 * measures color and ambient light with integrated IR rejection.
 *
 * Key Features:
 * - Four-channel RGBW color measurement
 * - CIE 1931 XYZ color space conversion
 * - Lux measurement with high accuracy
 * - Configurable measurement range and integration time
 * - Built-in temperature compensation
 * - Interrupt support with programmable thresholds
 *
 * Based on SparkFun OPT4048 Arduino Library:
 * https://github.com/sparkfun/SparkFun_OPT4048_Arduino_Library
 *
 * Reference: TI OPT4048 Datasheet (SBOS986)
 */

#ifndef __DEV_OPT4048_H__
#define __DEV_OPT4048_H__

#include <stdint.h>
#include <stdbool.h>

/*
 * OPT4048 Tristimulus Color Sensor Register Definitions
 * Based on TI OPT4048 datasheet
 */

/* OPT4048 Register Addresses */
#define OPT4048_REG_EXP_RES_CH0             0x00
#define OPT4048_REG_RES_CNT_CRC_CH0         0x01
#define OPT4048_REG_EXP_RES_CH1             0x02
#define OPT4048_REG_RES_CNT_CRC_CH1         0x03
#define OPT4048_REG_EXP_RES_CH2             0x04
#define OPT4048_REG_RES_CNT_CRC_CH2         0x05
#define OPT4048_REG_EXP_RES_CH3             0x06
#define OPT4048_REG_RES_CNT_CRC_CH3         0x07
#define OPT4048_REG_THRESH_L_EXP_RES        0x08
#define OPT4048_REG_THRESH_H_EXP_RES        0x09
#define OPT4048_REG_CONTROL                 0x0A
#define OPT4048_REG_INT_CONTROL             0x0B
#define OPT4048_REG_FLAGS                   0x0C
#define OPT4048_REG_DEVICE_ID               0x11

/* Device ID */
#define OPT4048_DEVICE_ID                   0x0821

/* Range Settings */
#define OPT4048_RANGE_2KLUX2                0x00
#define OPT4048_RANGE_4KLUX5                0x01
#define OPT4048_RANGE_9LUX                  0x02
#define OPT4048_RANGE_18LUX                 0x03
#define OPT4048_RANGE_36LUX                 0x04
#define OPT4048_RANGE_72LUX                 0x05
#define OPT4048_RANGE_144LUX                0x06
#define OPT4048_RANGE_AUTO                  0x0C

/* Conversion Time Settings */
#define OPT4048_CONVERSION_TIME_600US       0x00
#define OPT4048_CONVERSION_TIME_1MS         0x01
#define OPT4048_CONVERSION_TIME_1MS8        0x02
#define OPT4048_CONVERSION_TIME_3MS4        0x03
#define OPT4048_CONVERSION_TIME_6MS5        0x04
#define OPT4048_CONVERSION_TIME_12MS7       0x05
#define OPT4048_CONVERSION_TIME_25MS        0x06
#define OPT4048_CONVERSION_TIME_50MS        0x07
#define OPT4048_CONVERSION_TIME_100MS       0x08
#define OPT4048_CONVERSION_TIME_200MS       0x09
#define OPT4048_CONVERSION_TIME_400MS       0x0A
#define OPT4048_CONVERSION_TIME_800MS       0x0B

/* Operation Mode */
#define OPT4048_OPERATION_MODE_POWER_DOWN   0x00
#define OPT4048_OPERATION_MODE_AUTO_ONE_SHOT 0x01
#define OPT4048_OPERATION_MODE_ONE_SHOT     0x02
#define OPT4048_OPERATION_MODE_CONTINUOUS   0x03

/* Fault Count */
#define OPT4048_FAULT_COUNT_1               0x00
#define OPT4048_FAULT_COUNT_2               0x01
#define OPT4048_FAULT_COUNT_3               0x02
#define OPT4048_FAULT_COUNT_8               0x03

/* Interrupt Configuration */
#define OPT4048_INT_CFG_SMBUS_ALERT         0x00
#define OPT4048_INT_CFG_DR_NEXT_CHANNEL     0x01
#define OPT4048_INT_CFG_DR_ALL_CHANNELS     0x03

/* Threshold Channel Selection */
#define OPT4048_THRESHOLD_CH_0              0x00
#define OPT4048_THRESHOLD_CH_1              0x01
#define OPT4048_THRESHOLD_CH_2              0x02
#define OPT4048_THRESHOLD_CH_3              0x03

#pragma pack(push, 1)

/* OPT4048_REG_CONTROL -> 0x0A */
typedef union {
	struct {
		uint16_t fault_count        : 2;  /* Fault count field */
		uint16_t int_pol            : 1;  /* Interrupt polarity */
		uint16_t latch              : 1;  /* Latch interrupt */
		uint16_t op_mode            : 2;  /* Operating mode */
		uint16_t conversion_time    : 4;  /* Conversion time */
		uint16_t range              : 4;  /* Range */
		uint16_t reserved           : 1;  /* Reserved */
		uint16_t qwake              : 1;  /* Quick wake */
	} f;
	uint16_t word;
} dev_opt4048_reg_control_t;

/* OPT4048_REG_INT_CONTROL -> 0x0B */
typedef union {
	struct {
		uint16_t i2c_burst          : 1;  /* I2C burst mode */
		uint16_t reserved_two       : 1;  /* Reserved */
		uint16_t int_cfg            : 2;  /* Interrupt configuration */
		uint16_t int_dir            : 1;  /* Interrupt direction */
		uint16_t threshold_ch_sel   : 2;  /* Threshold channel select */
		uint16_t reserved_one       : 9;  /* Reserved */
	} f;
	uint16_t word;
} dev_opt4048_reg_int_control_t;

/* OPT4048_REG_FLAGS -> 0x0C */
typedef union {
	struct {
		uint16_t flag_low           : 1;  /* Low threshold flag */
		uint16_t flag_high          : 1;  /* High threshold flag */
		uint16_t conv_ready_flag    : 1;  /* Conversion ready flag */
		uint16_t overload_flag      : 1;  /* Overload flag */
		uint16_t reserved           : 12; /* Reserved */
	} f;
	uint16_t word;
} dev_opt4048_reg_flags_t;

/* OPT4048_REG_DEVICE_ID -> 0x11 */
typedef union {
	struct {
		uint16_t DIDH               : 12; /* Device ID high bits */
		uint16_t DIDL               : 2;  /* Device ID low bits */
		uint16_t reserved           : 2;  /* Reserved */
	} f;
	uint16_t word;
} dev_opt4048_reg_device_id_t;

/* Channel data registers */
typedef union {
	struct {
		uint16_t result_msb_ch0     : 12; /* MSB result for channel 0 */
		uint8_t exponent_ch0        : 4;  /* Exponent for channel 0 */
	} f;
	uint16_t word;
} dev_opt4048_reg_exp_res_ch0_t;

typedef union {
	struct {
		uint8_t crc_ch0             : 4;  /* CRC for channel 0 */
		uint8_t counter_ch0         : 4;  /* Counter for channel 0 */
		uint8_t result_lsb_ch0      : 8;  /* LSB result for channel 0 */
	} f;
	uint16_t word;
} dev_opt4048_reg_res_cnt_crc_ch0_t;

typedef union {
	struct {
		uint16_t result_msb_ch1     : 12; /* MSB result for channel 1 */
		uint8_t exponent_ch1        : 4;  /* Exponent for channel 1 */
	} f;
	uint16_t word;
} dev_opt4048_reg_exp_res_ch1_t;

typedef union {
	struct {
		uint8_t crc_ch1             : 4;  /* CRC for channel 1 */
		uint8_t counter_ch1         : 4;  /* Counter for channel 1 */
		uint8_t result_lsb_ch1      : 8;  /* LSB result for channel 1 */
	} f;
	uint16_t word;
} dev_opt4048_reg_res_cnt_crc_ch1_t;

typedef union {
	struct {
		uint16_t result_msb_ch2     : 12; /* MSB result for channel 2 */
		uint8_t exponent_ch2        : 4;  /* Exponent for channel 2 */
	} f;
	uint16_t word;
} dev_opt4048_reg_exp_res_ch2_t;

typedef union {
	struct {
		uint8_t crc_ch2             : 4;  /* CRC for channel 2 */
		uint8_t counter_ch2         : 4;  /* Counter for channel 2 */
		uint8_t result_lsb_ch2      : 8;  /* LSB result for channel 2 */
	} f;
	uint16_t word;
} dev_opt4048_reg_res_cnt_crc_ch2_t;

typedef union {
	struct {
		uint16_t result_msb_ch3     : 12; /* MSB result for channel 3 */
		uint8_t exponent_ch3        : 4;  /* Exponent for channel 3 */
	} f;
	uint16_t word;
} dev_opt4048_reg_exp_res_ch3_t;

typedef union {
	struct {
		uint8_t crc_ch3             : 4;  /* CRC for channel 3 */
		uint8_t counter_ch3         : 4;  /* Counter for channel 3 */
		uint8_t result_lsb_ch3      : 8;  /* LSB result for channel 3 */
	} f;
	uint16_t word;
} dev_opt4048_reg_res_cnt_crc_ch3_t;

/* Threshold registers */
typedef union {
	struct {
		uint16_t thresh_exp         : 4;  /* Threshold exponent */
		uint16_t thresh_result      : 12; /* Threshold result */
	} f;
	uint16_t word;
} dev_opt4048_reg_thresh_exp_res_t;

#pragma pack(pop)

/* Color data structure */
typedef struct {
	uint32_t red;        /* Channel 0 - Red */
	uint32_t green;      /* Channel 1 - Green */
	uint32_t blue;       /* Channel 2 - Blue */
	uint32_t white;      /* Channel 3 - White/Clear */
	uint8_t counterR;    /* Counter for red channel */
	uint8_t counterG;    /* Counter for green channel */
	uint8_t counterB;    /* Counter for blue channel */
	uint8_t counterW;    /* Counter for white channel */
	uint8_t CRCR;        /* CRC for red channel */
	uint8_t CRCG;        /* CRC for green channel */
	uint8_t CRCB;        /* CRC for blue channel */
	uint8_t CRCW;        /* CRC for white channel */
} opt4048_color_t;

/* Function prototypes */

/**
 * @brief Initialize OPT4048 sensor
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_init(void);

/**
 * @brief Read/write OPT4048 register
 * @param isRead true for read, false for write
 * @param reg Register address
 * @param pData Pointer to data buffer
 * @param len Number of bytes to read/write
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_reg_access(bool isRead, uint8_t reg, uint8_t *pData, uint16_t len);

/**
 * @brief Get device ID
 * @return Device ID or 0 on error
 */
uint16_t dev_opt4048_get_device_id(void);

/**
 * @brief Set basic sensor configuration
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_basic_setup(void);

/**
 * @brief Set measurement range
 * @param range Range setting (use OPT4048_RANGE_* defines)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_range(uint8_t range);

/**
 * @brief Get measurement range
 * @return Current range setting
 */
uint8_t dev_opt4048_get_range(void);

/**
 * @brief Set conversion time
 * @param time Conversion time setting (use OPT4048_CONVERSION_TIME_* defines)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_conversion_time(uint8_t time);

/**
 * @brief Get conversion time
 * @return Current conversion time setting
 */
uint8_t dev_opt4048_get_conversion_time(void);

/**
 * @brief Set operation mode
 * @param mode Operation mode (use OPT4048_OPERATION_MODE_* defines)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_operation_mode(uint8_t mode);

/**
 * @brief Get operation mode
 * @return Current operation mode
 */
uint8_t dev_opt4048_get_operation_mode(void);

/**
 * @brief Get ADC value for channel 0 (red)
 * @return ADC code for channel 0
 */
uint32_t dev_opt4048_get_adc_ch0(void);

/**
 * @brief Get ADC value for channel 1 (green)
 * @return ADC code for channel 1
 */
uint32_t dev_opt4048_get_adc_ch1(void);

/**
 * @brief Get ADC value for channel 2 (blue)
 * @return ADC code for channel 2
 */
uint32_t dev_opt4048_get_adc_ch2(void);

/**
 * @brief Get ADC value for channel 3 (white)
 * @return ADC code for channel 3
 */
uint32_t dev_opt4048_get_adc_ch3(void);

/**
 * @brief Get all channel data
 * @param color Pointer to color data structure to fill
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_get_all_channels(opt4048_color_t *color);

/**
 * @brief Get lux value
 * @return Lux value
 */
uint32_t dev_opt4048_get_lux(void);

/**
 * @brief Get CIE 1931 x chromaticity coordinate
 * @return CIE x value (0.0 to 1.0)
 */
double dev_opt4048_get_cie_x(void);

/**
 * @brief Get CIE 1931 y chromaticity coordinate
 * @return CIE y value (0.0 to 1.0)
 */
double dev_opt4048_get_cie_y(void);

/**
 * @brief Get Correlated Color Temperature (CCT)
 * @return CCT value in Kelvin
 */
double dev_opt4048_get_cct(void);

/**
 * @brief Get conversion ready flag
 * @return true if conversion ready, false otherwise
 */
bool dev_opt4048_get_conv_ready_flag(void);

/**
 * @brief Get overload flag
 * @return true if overload detected, false otherwise
 */
bool dev_opt4048_get_overload_flag(void);

/**
 * @brief Get too bright flag
 * @return true if above high threshold, false otherwise
 */
bool dev_opt4048_get_too_bright_flag(void);

/**
 * @brief Get too dim flag
 * @return true if below low threshold, false otherwise
 */
bool dev_opt4048_get_too_dim_flag(void);

/**
 * @brief Set low threshold for hysteresis
 * @param threshold_code Threshold code (mantissa)
 * @param exponent Exponent value (0-15)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_threshold_low(uint16_t threshold_code, uint8_t exponent);

/**
 * @brief Set high threshold for hysteresis
 * @param threshold_code Threshold code (mantissa)
 * @param exponent Exponent value (0-15)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_threshold_high(uint16_t threshold_code, uint8_t exponent);

/**
 * @brief Set interrupt configuration
 * @param int_cfg Interrupt mode (use OPT4048_INT_CFG_* defines)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_int_cfg(uint8_t int_cfg);

/**
 * @brief Enable/disable interrupt latching
 * @param enable true to latch interrupt, false for transparent mode
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_latch(bool enable);

/**
 * @brief Set fault count for threshold crossing
 * @param count Fault count (use OPT4048_FAULT_COUNT_* defines)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_fault_count(uint8_t count);

/**
 * @brief Trigger one-shot measurement
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_trigger_one_shot(void);

/**
 * @brief Get all flags
 * @return Flags register value
 */
dev_opt4048_reg_flags_t dev_opt4048_get_all_flags(void);

/**
 * @brief Set fault count
 * @param count Fault count setting (use OPT4048_FAULT_COUNT_* defines)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_fault_count(uint8_t count);

/**
 * @brief Get fault count
 * @return Current fault count setting
 */
uint8_t dev_opt4048_get_fault_count(void);

/**
 * @brief Set threshold channel for interrupt
 * @param channel Channel selection (use OPT4048_THRESHOLD_CH_* defines)
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_threshold_channel(uint8_t channel);

/**
 * @brief Set I2C burst mode
 * @param enable true to enable burst mode, false to disable
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_i2c_burst(bool enable);

/**
 * @brief Set quick wake mode
 * @param enable true to enable quick wake, false to disable
 * @return 0 on success, negative errno code on failure
 */
int dev_opt4048_set_qwake(bool enable);

#endif /* __DEV_OPT4048_H__ */
