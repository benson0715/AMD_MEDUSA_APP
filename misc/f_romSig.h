/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __F_ROMSIG_H__
#define __F_ROMSIG_H__

#include <zephyr/kernel.h>

#define F_ROMSIG_MAX_FIELDS  13

typedef union {
	uint8_t buf[48];
	struct {
		uint32_t u32Pd0_Offset;
		uint32_t u32Pd0_Size : 24;
		uint32_t Pd0Checksum : 8;

		uint32_t u32Pd1_Offset;
		uint32_t u32Pd1_Size : 24;
		uint32_t Pd1Checksum : 8;
		
		uint32_t u32Pd2_Offset;
		uint32_t u32Pd2_Size : 24;
		uint32_t Pd2Checksum : 8;
		
		uint32_t u32Pd3_Offset;
		uint32_t u32Pd3_Size : 24;
		uint32_t Pd3Checksum : 8;

		uint32_t u32Pd4_Offset;
		uint32_t u32Pd4_Size : 24;
		uint32_t Pd4Checksum : 8;
		
		uint32_t u32Pd5_Offset;
		uint32_t u32Pd5_Size : 24;
		uint32_t Pd5Checksum : 8;

		uint32_t u32Pd6_Offset;
		uint32_t u32Pd6_Size : 24;
		uint32_t Pd6Checksum : 8;

		uint32_t u32Pd7_Offset;
		uint32_t u32Pd7_Size : 24;
		uint32_t Pd7Checksum : 8;
		
		uint32_t u32Pd8_Offset;
		uint32_t u32Pd8_Size : 24;
		uint32_t Pd8Checksum : 8;
		
		uint32_t u32Pd9_Offset;
		uint32_t u32Pd9_Size : 24;
		uint32_t Pd9Checksum : 8;

		uint32_t u32Pd10_Offset;
		uint32_t u32Pd10_Size : 24;
		uint32_t CyPd4Checksum : 8;
		
		uint32_t u32Pd11_Offset;
		uint32_t u32Pd11_Size : 24;
		uint32_t Pd11Checksum : 8;
		
		uint32_t u32Pd12_Offset;
		uint32_t u32Pd12_Size : 24;
		uint32_t Pd12Checksum : 8;
	} f;
	struct {
		uint32_t offset;
		uint32_t size     : 24;
		uint32_t checksum : 8;
	} sigArr [F_ROMSIG_MAX_FIELDS];
} F_ROMSIG_BLOCK;

#define F_ROMSIG_OFFSET   0x00000010
#define F_ROMSIG_SIZE     sizeof(F_ROMSIG_BLOCK)

void f_romSig_init(void);

bool f_romSig_isValid(uint8_t idx);
uint32_t f_romSig_getOffset(uint8_t idx);
uint32_t f_romSig_getSize(uint8_t idx);

#endif // end of __F_ROMSIG_H__

