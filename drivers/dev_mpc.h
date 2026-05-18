/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_MPC_H__
#define __DEV_MPC_H__

#include <stdint.h>
#include <stdbool.h>

/* MPC register */
#define DEV_MPC_DATALENGH_REG           0x00  /* data length */
#define DEV_MPC_POSTCODE_COUNT_REG      0x01  /* postcode count */
#define DEV_MPC_POSTCODE_TYPE_REG       0x02  /* postcode type */
#define DEV_MPC_POSTCODE_REG            0x03  /* postcode */
#define DEV_MPC_TIMESTAMP_REG           0x0C  /* timestamp */
#define DEV_MPC_BOARDID_REG             0x10  /* board id */
#define DEV_MPC_CLEAR_SEC_REG           0x11  /* clear sec */
#define DEV_MPC_DIGIT_CTRL_REG          0x12  /* digital control */
#define DEV_MPC_DISPLAY_MODE_REG        0x27  /* display mode */
#define DEV_MPC_EC_VERSION_REG          0x30  /* EC version */
#define DEV_MPC_PD0_VERSION_REG         0x35  /* PD0 version */
#define DEV_MPC_PD1_VERSION_REG         0x38  /* PD1 version */
#define DEV_MPC_MAC_REG                 0x60  /* mac address */
#define DEV_MPC_MPC_STATUS              0xF0  /* mpc status */
#define DEV_MPC_PRODUCT_ID_REG          0xFD  /* product id */
#define DEV_MPC_MANUFACTUER_ID_REG      0xFE  /* manufacturer id */
#define DEV_MPC_REVISION_REG            0xFF  /* revision */
#define DEV_WAIE_POWER_REG              0x500 /* walle rite */
#define DEV_MPC_BVER_REG                0xE00 /* bios version */
#define DEV_SOCID_EAX_REG               0xD00 /* soc id A */
#define DEV_SOCID_EBX_REG               0xD02 /* soc id B */
#define DEV_SOCID_ECX_REG               0xD04 /* soc id C */
#define DEV_SOCID_EDX_REG               0xD06 /* soc id D */
#define DEV_MEMORY_ID_REG               0xD08 /* memory id */
#define DEV_VR_REG                      0xD09 /* VR version */
#define DEV_BOARD_SKU_REG               0xD0D /* board id sku */
#define DEV_SOCID_OPN                   0xDE0 /* soc id OPN */
#define DEV_SOCID_SN                    0xDF0 /* soc id SN */

typedef struct {
	uint8_t  scanLimit;
	uint8_t  brightnessAll;
	uint8_t  brightnessHalf;
	uint8_t  DPs;
	uint32_t rawLow;
	uint32_t rawHigh;
	uint32_t isstring;
	uint32_t OnOrOff;
} FRAME_BUF_MPC_32BIT;

/* mpc device init */
void dev_mpc_init(uint8_t i2cPort);
/* mpc shows postcode with number */
void dev_mpc_32bit_postcode(uint32_t pc, uint8_t dp, uint8_t limit);
/* mpc shows postcode with chart */
bool dev_mpc_print(const char * format, ...);
/* mpc postcode LED turn on */
void dev_mpc_32bit_turnOn(uint8_t brightness);
/* mpc postcode LED turn off */
void dev_mpc_32bit_turnOff(void);
/* mpc set status */
void dev_mpc_set_sts(bool status);
/* mpc get status */
bool dev_mpc_get_sts(void);
/* mpc reset */
void dev_mpc_32bit_reset(void);
/* mpc set EC version */
void dev_mpc_setEcVersion(uint8_t* ecVer, uint8_t* pd0Ver, uint8_t* pd1Ver);
/* mpc set the bios version */
void dev_mpc_setBiosVersion(uint8_t* biosVer, uint8_t length);
/* mpc set the mac address version */
void dev_mpc_setMacVersion(uint8_t* macVer, uint8_t length);
/* mpc set the CPU ID */
void dev_mpc_setCpuId(uint8_t* cpuId, uint8_t length);
/* mpc set the CPU OPN */
void dev_mpc_setCpuOpn(uint8_t* cpuOpn, uint8_t length);
/* mpc set the CPU SN */
void dev_mpc_setCpuSN(uint8_t* cpuSn, uint8_t length);

/* MPC update callback function type */
typedef void (*dev_mpc_update_callback_t)(void);

/* Register MPC update callback */
void dev_mpc_register_update_callback(dev_mpc_update_callback_t callback);

#endif // end of __DEV_MPC_H__

