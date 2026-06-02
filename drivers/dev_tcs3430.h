/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_TCS3430_H__
#define __DEV_TCS3430_H__

#include <stdint.h>
#include <stdbool.h>

/*
 * Color and ALS Sensor registers define
 * Reserve regs will not in this list
 */
#define TCS3430_REG_ENABLE                          (0x80)
#define TCS3430_REG_ATIME                           (0x81)
#define TCS3430_REG_WTIME                           (0x83)
#define TCS3430_REG_AILTL                           (0x84)
#define TCS3430_REG_AILTH                           (0x85)
#define TCS3430_REG_AIHTL                           (0x86)
#define TCS3430_REG_AIHTH                           (0x87)
#define TCS3430_REG_PERS                            (0x8C)
#define TCS3430_REG_CFG0                            (0x8D)
#define TCS3430_REG_CFG1                            (0x90)
#define TCS3430_REG_REVID                           (0x91)
#define TCS3430_REG_ID                              (0x92)
#define TCS3430_REG_STATUS                          (0x93)
#define TCS3430_REG_CH0DATAL                        (0x94)
#define TCS3430_REG_CH0DATAH                        (0x95)
#define TCS3430_REG_CH1DATAL                        (0x96)
#define TCS3430_REG_CH1DATAH                        (0x97)
#define TCS3430_REG_CH2DATAL                        (0x98)
#define TCS3430_REG_CH2DATAH                        (0x99)
#define TCS3430_REG_CH3DATAL                        (0x9A)
#define TCS3430_REG_CH3DATAH                        (0x9B)
#define TCS3430_REG_CFG2                            (0x9F)
#define TCS3430_REG_CFG3                            (0xAB)
#define TCS3430_REG_AZ_CONFIG                       (0xD6)
#define TCS3430_REG_INTENAB                         (0xDD)

#define AMUX_SHIFT 3
#define AMS_TCS3430_AMUX_MASK (1 << AMUX_SHIFT)
#define AMS_TCS3430_AMUX_ENABLE (1 << AMUX_SHIFT)
#define AGAIN_SHIFT 0
#define AMS_TCS3430_AGAIN_GAIN_1X (0 << AGAIN_SHIFT)
#define AMS_TCS3430_AGAIN_GAIN_4X (1 << AGAIN_SHIFT)
#define AMS_TCS3430_AGAIN_GAIN_16X (2 << AGAIN_SHIFT)
#define AMS_TCS3430_AGAIN_GAIN_64X (3 << AGAIN_SHIFT)
#define AMS_TCS3430_AGAIN_MASK (3 << AGAIN_SHIFT)

#define AMS_TCS3430_CH0DATAH_SHIFT 8
#define AMS_TCS3430_CH1DATAH_SHIFT 8
#define AMS_TCS3430_CH2DATAH_SHIFT 8
#define AMS_TCS3430_CH3DATAH_SHIFT 8

#define AMS_INSTANCE_ID_MAX 256

#define AMS_TCS3430_ATIME_100_MS	 		0x23
#define AMS_TCS3430_WTIME_1_MS		 		0x00
#define AMS_TCS3430_APERS_INT_MODE			0x05
#define AMS_TCS3430_APERS_POLL_MODE			0x00
#define AMS_TCS3430_RUN_AZ_AUTOMATICALLY	0x7F
#define AMS_TCS3430_DISABLE_128X_AGAIN		0x00
#define AMS_TCS3430_DISABLE_TOGGLE_IR2		0x00

#define MIN_TIME_QUANTUM 		2780
#define NUM_OF_MS_IN_ONE_SEC	1000

#define ID_SHIFT 2
#define ID_MASK (63 << ID_SHIFT)
#define AMS_TCS3430_ID_NUM 0xDC

#define AMS_TCS3430_CFG0_RESET 0x80
#define WLONG_SHIFT 2
#define AMS_TCS3430_WLONG_MASK (1 << WLONG_SHIFT)

#define HGAIN_SHIFT 4
#define AMS_TCS3430_HGAIN_MASK (1 << HGAIN_SHIFT)
#define AMS_TCS3430_HGAIN_ENABLE (1 << HGAIN_SHIFT)

#define INT_READ_CLEAR_SHIFT 7
#define AMS_TCS3430_INT_READ_CLEAR_MASK (1 << INT_READ_CLEAR_SHIFT)
#define AMS_TCS3430_INT_READ_CLEAR_ENABLE (1 << INT_READ_CLEAR_SHIFT)

#define AMS_TCS3430_ASAT_SHIFT 7
#define AMS_TCS3430_ASAT_MASK (1 << AMS_TCS3430_ASAT_SHIFT)
#define AMS_TCS3430_ASAT_ENABLE (1 << AMS_TCS3430_ASAT_SHIFT)
#define AMS_TCS3430_ASAT_DISABLE (0 << AMS_TCS3430_ASAT_SHIFT)
#define AMS_TCS3430_AINT_SHIFT 4
#define AMS_TCS3430_AINT_MASK (1 << AMS_TCS3430_AINT_SHIFT)
#define AMS_TCS3430_AINT_ENABLE (1 << AMS_TCS3430_AINT_SHIFT)

#define AMS_TCS3430_AEN_SHIFT 1
#define AMS_TCS3430_AEN_MASK (1 << AMS_TCS3430_AEN_SHIFT)
#define AMS_TCS3430_AEN_ENABLE (1 << AMS_TCS3430_AEN_SHIFT)
#define AMS_TCS3430_PON_SHIFT 0
#define AMS_TCS3430_PON_MASK (1 << AMS_TCS3430_PON_SHIFT)
#define AMS_TCS3430_PON_ENABLE (1 << AMS_TCS3430_PON_SHIFT)

#define MASK_LUX_VALID    (1<<0)
#define MASK_COLOR_VALID  (1<<1)
#define MAX_RAW_COUNT     60000
#define MIN_RAW_COUNT     5

#if CONFIG_AMS_ALS_XYZ_FLOATING_POINT
#define MIN_CCT           800
#define MAX_CCT           20000
#define MIN_LUX_CCT_CAP   (4500) //CCT cap value when lux <= MIN_LUX_CAP
#define MIN_CCT_CAP       (2800) //CCT min value when CCT cap feature is enabled
#define MAX_CCT_CAP       (7000) //CCT max value when CCT cap feature is enabled
#define MIN_LUX_TH        (4)    //Min LUX thrshold value below which CCT=4.5 when cap feature in ON
#else
#define MIN_CCT           3276800
#define MAX_CCT           81920000
#define MIN_LUX_CCT_CAP   18432000 //CCT cap value when lux <= MIN_LUX_CAP
#define MIN_CCT_CAP       11468800 //CCT min value when CCT cap feature is enabled
#define MAX_CCT_CAP       28672000 //CCT max value when CCT cap feature is enabled
#define MIN_LUX_TH        16384    //Min LUX thrshold value below which CCT=4.5 when cap feature in ON
#endif

// #define likely(x)       __builtin_expect(!!(x), 1)
// #define unlikely(x)     x

#pragma pack(push, 1)
/* TCS3430_REG_ENABLE -> 0x80 */
 typedef union {
		struct {
			uint8_t PON                                 :1; //Power ON
			uint8_t AEN                                 :1; //ALS Enable
			uint8_t RESERVED                            :1;
			uint8_t WEN                                 :1; //Wait Enable
			uint8_t RESERVED1                           :4;
		} f;
		uint8_t buf;
} DEV_TCS3430_REG_ENABLE;

/* TCS3430_REG_ATIME -> 0x81 */
typedef union {
		struct {
			uint8_t ATIME                                :8; //Integration Time
		} f;
		uint8_t buf;
} DEV_TCS3430_REG_ATIME;

/* TCS3430_REG_WTIME -> 0x83 */
typedef union {
		struct {
			uint8_t WTIME                                :8; //ALS Wait Time
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_WTIME;

/* TCS3430_REG_AILTL -> 0x84 */
typedef union {
		struct {
			uint8_t AILTL                                :8; //Low Byte of the Low Threshold
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_AILTL;

/* TCS3430_REG_AILTH -> 0x85 */
typedef union {
		struct {
			uint8_t AILTH                                :8; //High Byte of the Low Threshold
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_AILTH;

/* TCS3430_REG_AIHTL -> 0x86 */
typedef union {
		struct {
			uint8_t AIHTL                                :8; //Low Byte of the High Threshold
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_AIHTL;

/* TCS3430_REG_AIHTH -> 0x87 */
typedef union {
		struct {
			uint8_t AIHTH                                :8; //High Byte of the High Threshold
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_AIHTH;

/* TCS3430_REG_PERS -> 0x8C */
typedef union {
		struct {
			uint8_t APERS                                :4; //Interrupt Generated When...
			uint8_t RESERVED                             :4;
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_PERS;

/* TCS3430_REG_CFG0 -> 0x8D */
typedef union {
		struct {
			uint8_t RESERVED                             :2;
			uint8_t WLONG                                :1; //Wait Long. When asserted, the wait cycle is increased by a factor 12x from that programmed in the WTIME register
			uint8_t RESERVED1                            :4;
			uint8_t RESET                                :1;
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CFG0;

/* TCS3430_REG_CFG1 -> 0x90 */
typedef union {
		struct {
			uint8_t AGAIN                                :2; //ALS Gain Control. Sets the gain of the ALS DAC.
			uint8_t RESERVED                             :1;
			uint8_t AMUX                                 :1; //ALS Multiplexer. Sets the CH3 input. Default = 0 (X Channel). Set to 1 to read IR2.
			uint8_t RESERVED1                            :4; 
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CFG1;

/* TCS3430_REG_REVID -> 0x91 */
typedef union {
		struct {
			uint8_t REV_ID                               :4; //Revision Number Identification
			uint8_t RESERVED                             :4;
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_REVID;

/* TCS3430_REG_ID -> 0x92 */
typedef union {
		struct {
			uint8_t RESERVED                             :2;
			uint8_t ID                                   :6; //Part Number Identification
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_ID;

/* TCS3430_REG_STATUS -> 0x93 */
typedef union {
		struct {
			uint8_t RESERVED                             :3;
			uint8_t AINT                                 :1; //ALS Interrupt. Indicates that the device is asserting an ALS interrupt. writing a 1 will clear this status flag.
			uint8_t RESERVED1                            :3;
			uint8_t ASAT                                 :1; //ALS Saturation. This flag is set for analog saturation writing a 1 will clear this status flag.
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_STATUS;

/* TCS3430_REG_CH0DATAL -> 0x94 */
typedef union {
		struct {
			uint8_t CH0DATAL                             :8; //Low Byte of CH0 ADC data. Contains Z data
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CH0DATAL;

/* TCS3430_REG_CH0DATAH -> 0x95 */
typedef union {
		struct {
			uint8_t CH0DATAH                             :8; //High Byte of CH0 ADC data. Contains Z data.
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CH0DATAH;

/* TCS3430_REG_CH1DATAL -> 0x96 */
typedef union {
		struct {
			uint8_t CH1DATAL                             :8; //Low Byte of CH1 ADC data. Contains Y data.
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CH1DATAL;

/* TCS3430_REG_CH1DATAH -> 0x97 */
typedef union {
		struct {
			uint8_t CH1DATAH                             :8; //High Byte of CH1 ADC data. Contains Y data.
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CH1DATAH;

/* TCS3430_REG_CH2DATAL -> 0x98 */
typedef union {
		struct {
			uint8_t CH2DATAL                             :8; //Low Byte of CH2 ADC data. Contains IR1 data.
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CH2DATAL;

/* TCS3430_REG_CH2DATAH -> 0x99 */
typedef union {
		struct {
			uint8_t CH2DATAH                             :8; //High Byte of CH2 ADC data. Contains IR1 data
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CH2DATAH;

/* TCS3430_REG_CH3DATAL -> 0x9A */
typedef union {
		struct {
			uint8_t CH3DATAL                             :8; //Low Byte of CH3 ADC data.
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CH3DATAL;

/* TCS3430_REG_CH3DATAH -> 0x9A */
typedef union {
		struct {
			uint8_t CH3DATAH                             :8; //High Byte of CH3 ADC data. 
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CH3DATAH;

/* TCS3430_REG_CFG2 -> 0x9F */
typedef union {
		struct {
			uint8_t RESERVED                             :4;
			uint8_t HGAIN                                :1; //High 128x gain.
			uint8_t RESERVED1                            :3;
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CFG2;

/* TCS3430_REG_CFG3 -> 0xAB */
typedef union {
		struct {
			uint8_t RESERVED                             :4;
			uint8_t SAI                                  :1; //Sleep After Interrupt. Power down the device at the end of the ALS cycle if an interrupt has been generated
			uint8_t RESERVED1                            :2;
			uint8_t INT_READ_CLEAR                       :1; //If this bit is set, all flag bits in the STATUS register will be reset whenever the STATUS register is read over I2C
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_CFG3;

/* TCS3430_REG_AZ_CONFIG -> 0xD6 */
typedef union {
		struct {
			uint8_t AZ_NTH_ITERATION                     :7; //Run autozero automatically every nth ALS iteration (0=never, 7Fh=only at first ALS cycle, n=every nth time)
			uint8_t AZ_MODE                              :1; //0: Always start at zero when searching the best offset value 
					                                                   //1: Always start at the previous (offset_c) with the auto-zero mechanism
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_AZ_CONFIG;

/* TCS3430_REG_INTENAB -> 0xDD */
typedef union {
		struct {
				uint8_t RESERVED                             :4;
				uint8_t AIEN                                 :1; //Writing '1' to this bit enables ALS interrupt.
				uint8_t RESERVED1                            :2;
				uint8_t ASIEN                                :1; //Writing '1' to this bit enables ASAT interrupt
		} f;
		uint8_t buf;
}  DEV_TCS3430_REG_INTENAB;

typedef struct tcs3430_display_calibration_data
{
	uint8_t atime        :8;
	uint8_t wtime        :8;
	uint8_t pers         :4;
	uint8_t autozero     :7;
	uint8_t again        :2;
	uint8_t hgain        :1;
	uint8_t toggle_ir2   :1;
	uint8_t autogain     :1;
}  tcs3430_display_calibration_data;

typedef struct tcs3430_registers
{
	union {
		struct {
			DEV_TCS3430_REG_ENABLE     enable;                         //0x80
			DEV_TCS3430_REG_ATIME      atime;                          //0x81
			DEV_TCS3430_REG_WTIME      wtime;                          //0x83
			DEV_TCS3430_REG_AILTL      ailtl;                          //0x84
			DEV_TCS3430_REG_AILTH      ailth;                          //0x85
			DEV_TCS3430_REG_AIHTL      aihtl;                          //0x86
			DEV_TCS3430_REG_AIHTH      aihth;                          //0x87
			DEV_TCS3430_REG_PERS       pers;                           //0x8C
			DEV_TCS3430_REG_CFG0       cfg0;                           //0x8D
			DEV_TCS3430_REG_CFG1       cfg1;                           //0x90
			DEV_TCS3430_REG_REVID      revid;                          //0x91
			DEV_TCS3430_REG_ID         id;                             //0x92
			DEV_TCS3430_REG_STATUS     status;                         //0x93
			DEV_TCS3430_REG_CH0DATAL   ch0datal;                       //0x94
			DEV_TCS3430_REG_CH0DATAH   ch0datah;                       //0x95
			DEV_TCS3430_REG_CH1DATAL   ch1datal;                       //0x96
			DEV_TCS3430_REG_CH1DATAH   ch1datah;                       //0x97
			DEV_TCS3430_REG_CH2DATAL   ch2datal;                       //0x98
			DEV_TCS3430_REG_CH2DATAH   ch2datah;                       //0x99
			DEV_TCS3430_REG_CH3DATAL   ch3datal;                       //0x9A
			DEV_TCS3430_REG_CH3DATAH   ch3datah;                       //0x9B
			DEV_TCS3430_REG_CFG2       cfg2;                           //0x9F
			DEV_TCS3430_REG_CFG3       cfg3;                           //0xAB
			DEV_TCS3430_REG_AZ_CONFIG  azconfig;                       //0xD6
			DEV_TCS3430_REG_INTENAB    intenab;                        //0xDD
		}  f;
		uint8_t buf[25];
	};
} tcs3430_registers;

// *********************
// * STRUCT DEFINITION *
// *********************
typedef struct _ams_tcs3430_display_calibration_data
{
	unsigned char atime:8;
	unsigned char wtime:8;
	unsigned char pers:4;
	unsigned char autozero:7;
	unsigned char again:2;
	unsigned char hgain:1;
	unsigned char toggle_ir2:1;
	unsigned char autogain:1;
} ams_tcs3430_display_calibration_data;

typedef struct tcs3430_display_calibration_private_data
{
	uint8_t instance_id;
	tcs3430_display_calibration_data caldata;
} tcs3430_display_calibration_private_data;

typedef struct _display_calibration_ams_tcs3430_private_data
{
	unsigned char instance_id;
	ams_tcs3430_display_calibration_data caldata;
} display_calibration_ams_tcs3430_private_data;

struct _xyz_sensor_data_u16_t {
	unsigned short x;
	unsigned short y;
	unsigned short z;
	unsigned short ir1;
	unsigned short ir2;
};

#pragma pack(pop)

typedef struct _xyz_sensor_data_u16_t  xyz_sensor_data_u16_t;

/* function to init TCS3430 */
void TCS3430_init(void);

/* TCS3430 I2C write and read */
uint32_t dev_tcs3430_regAccess(bool isRead, uint8_t reg, void * pBuf, uint32_t len);

/* Dump the register map */
void TCS3430_DumpRegister(void);

uint8_t TCS3430_Register_Offset_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);
uint8_t TCS3430_Register_Value_Handler (uint8_t isRead, uint8_t ui8Idx, uint8_t * pui8Data);

#endif // end of __DEV_TCS3430_H__

