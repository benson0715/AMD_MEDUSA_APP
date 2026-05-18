/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __U_MISC_H__
#define __U_MISC_H__

#include <zephyr/kernel.h>

uint8_t u_misc_u8BitCount(uint8_t d);
uint8_t u_misc_u16BitCount(uint16_t d);
uint8_t u_misc_u32BitCount(uint32_t d);
uint8_t u_misc_u64BitCount(uint64_t d);

//
// Use U_MISC_LOB_isOverflow to check if there's bit1 higher than startPos
//
#define U_MISC_LOB_isOverflow(x)   ((x) & 0x80)

uint8_t u_misc_u8lowestOneBit(uint8_t d, uint32_t startPos);
uint8_t u_misc_u16lowestOneBit(uint16_t d, uint32_t startPos);
uint8_t u_misc_u32lowestOneBit(uint32_t d, uint32_t startPos);
uint8_t u_misc_u64lowestOneBit(uint64_t d, uint32_t startPos);

/*
 * Functions for pseudo-random numbers
 */
void u_misc_pr_srand(uint32_t s);
uint16_t u_misc_pr_rand( void );

uint16_t u_misc_pr_random (uint16_t s);
void u_misc_pr_genRandArray (uint16_t seed, void * pBuf, uint32_t size);
uint32_t u_misc_pr_verifyRandArray (uint16_t seed, void * pBuf, uint32_t size);

#endif // end of __U_MISC_H__

