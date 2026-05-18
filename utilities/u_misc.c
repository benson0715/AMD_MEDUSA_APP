/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "u_misc.h"
#include <assert.h>
static uint8_t _s_bitcount[] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

static uint8_t _s_lowbit[] = {
 0x80, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

#define _U_BYTE_BC(byte)  (_s_bitcount[(byte)])
#define _U_BYTE_LB(byte)  (_s_lowbit[(byte)])


uint8_t u_misc_u8BitCount(uint8_t d)
{
	return _U_BYTE_BC(d);
}

uint8_t u_misc_u16BitCount(uint16_t d)
{
	return _U_BYTE_BC(d & 0xFF) + 
	       _U_BYTE_BC((d >> 8) & 0xFF);
}

uint8_t u_misc_u32BitCount(uint32_t d)
{
	return _U_BYTE_BC(d & 0xFF) + 
	       _U_BYTE_BC((d >> 8) & 0xFF) + 
	       _U_BYTE_BC((d >> 16) & 0xFF) + 
	       _U_BYTE_BC((d >> 24) & 0xFF);
}

uint8_t u_misc_u64BitCount(uint64_t d)
{
	uint32_t h = (uint32_t)(d >> 32);
	uint32_t l = (uint32_t)(d);
	return u_misc_u32BitCount(h) + u_misc_u32BitCount(l);
}

uint8_t u_misc_u8lowestOneBit(uint8_t d, uint32_t startPos)
{
	assert(startPos < 8);

	d >>= startPos;  // clear the bits lower than startPos
	return _U_BYTE_LB(d) + startPos;
}

uint8_t u_misc_u16lowestOneBit(uint16_t d, uint32_t startPos)
{
	assert(startPos < 16);
	uint8_t tmp;

	d >>= startPos;  // clear the bits lower than startPos

	tmp = (uint8_t)(d & 0xFF);
	if (tmp) {
		return _U_BYTE_LB(tmp) + startPos;
	}

	tmp = (uint8_t)((d >> 8) & 0xFF);
	return _U_BYTE_LB(tmp) + startPos + 8;
}

uint8_t u_misc_u32lowestOneBit(uint32_t d, uint32_t startPos)
{
	assert(startPos < 32);
	uint8_t tmp;

	d >>= startPos;  // clear the bits lower than startPos

	tmp = (uint8_t)(d & 0xFF);
	if (tmp) {
		return _U_BYTE_LB(tmp) + startPos;
	}

	if (startPos < 24) {
		tmp = (uint8_t)((d >> 8) & 0xFF);
		if (tmp) {
			return _U_BYTE_LB(tmp) + startPos + 8;
		}

		if (startPos < 16) {
			tmp = (uint8_t)((d >> 16) & 0xFF);
			if (tmp) {
				return _U_BYTE_LB(tmp) + startPos + 16;
			}

			if (startPos < 8) {
				tmp = (uint8_t)((d >> 24) & 0xFF);
				if (tmp) {
					return _U_BYTE_LB(tmp) + startPos + 24;
				}
			}
		}
	}

	return 0x80;
}

uint8_t u_misc_u64lowestOneBit(uint64_t d, uint32_t startPos)
{
	assert(startPos < 64);
	uint32_t tmp;

	d >>= startPos;  // clear the bits lower than startPos

	tmp = (uint32_t)(d & 0xFFFFFFFF);
	if (tmp)
		return u_misc_u32lowestOneBit(tmp, 0) + startPos;

	if (startPos < 32) {
		tmp = (uint32_t)((d >> 32) & 0xFFFFFFFF);
		if (tmp) {
			return u_misc_u32lowestOneBit(tmp, 0) + startPos + 32;
		}
	}

	return 0x80;
}

/*
 * Functions for pseudo-random numbers
 */

static uint32_t _s_pr_next;

/**
 * Set seed of pseudo-random numbers array
 */
void u_misc_pr_srand(uint32_t s)
{
	_s_pr_next = s;
}

/**
 * Get one 16-bit pseudo-random number
 */
uint16_t u_misc_pr_rand(void)
{
	_s_pr_next = _s_pr_next * 1103515245ul + 12345;  
	return((uint16_t)(_s_pr_next / 65536) % 32768);
}

uint16_t u_misc_pr_random (uint16_t s)
{
	uint64_t tmp = ((uint64_t)s * 1103515245ull + 12345); 
	return((uint64_t)(tmp / 65536ul) % 32768);
}

typedef union {
    uint16_t  u16;
    struct {
        uint8_t l;
        uint8_t h;
    } byte;
} U_MISC_RAND_VAL_HELPER;

void u_misc_pr_genRandArray (uint16_t seed, void * pBuf, uint32_t size)
{
    U_MISC_RAND_VAL_HELPER val;
    uint8_t * pArr = (uint8_t *)pBuf;
    uint32_t idx = 0;
    for (idx = 0, val.u16 = seed; idx < (size & (~0x01ul)); idx += 2) {
        *pArr++ = val.byte.l;
        *pArr++ = val.byte.h;
        val.u16 = u_misc_pr_random(val.u16);
    }
    if (size % 2) {
        *pArr = val.byte.l;
    }
}

uint32_t u_misc_pr_verifyRandArray (uint16_t seed, void * pBuf, uint32_t size)
{
    U_MISC_RAND_VAL_HELPER val;
    uint8_t * pArr = (uint8_t *)pBuf;
    uint32_t idx = 0;
    for (idx = 0, val.u16 = seed; idx < (size & (~0x01ul)); idx += 2) {
        if (*pArr++ != val.byte.l) {
            return idx;
        }
        if (*pArr++ != val.byte.h) {
            return idx + 1;
        }
        val.u16 = u_misc_pr_random(val.u16);
    }
    if (size & 0x01ul) {
        if (*pArr != val.byte.l) {
            return size - 1;
        }
    }

    return size;        // return size means no error be found.
                        // return other value (less than size) means the offset the error being detected.
}