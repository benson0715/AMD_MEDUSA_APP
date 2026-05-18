/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef AMD_CRB_DRIVERS_RAA489300_H_
#define AMD_CRB_DRIVERS_RAA489300_H_

#include "system.h"

/* LM95234 registers */
#define RAA489300_REG_OUTPUT_CURRENT_LIMIT          (0x14)
#define RAA489300_REG_OUTPUT_OUTPUT_VOLTAGE			(0x15)
#define RAA489300_REG_CONTROL0          			(0x39)
#define RAA489300_REG_CONTROL1						(0x3C)
#define RAA489300_REG_CONTROL2          			(0x3D)
#define RAA489300_REG_INPUT_CURRENT_LIMIT			(0x3F)
#define RAA489300_REG_VIN_OK_REF           			(0x40)
#define RAA489300_REG_CONTROL6						(0x43)
#define RAA489300_REG_REVERSE_PTM_VOLTAGE           (0x49)
#define RAA489300_REG_MINI_INPUT_VOLTAGE			(0x4B)
#define RAA489300_REG_CONTROL3          			(0x4C)
#define RAA489300_REG_CONTROL4						(0x4E)
#define RAA489300_REG_CONTROL5          			(0x4F)

/* function */
void amd_crb_drivers_raa489300_init(uint8_t tbIdx);
bool amd_crb_drivers_raa489300_ready();

#endif  /*AMD_CRB_DRIVERS_RAA489300_H_*/
