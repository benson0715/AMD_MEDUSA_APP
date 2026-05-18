/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */


#ifndef __U_NUMBERS_H__
#define __U_NUMBERS_H__

#include <zephyr/kernel.h>

typedef int16_t Tnumber;
typedef float   TtempFlt;

typedef union {
	uint16_t u16;
	uint8_t  u8[2];               // [1] integer; [0] decimal
	struct {
		uint8_t reserved_4_0: 5;
		uint8_t decimal:      3;  // low
		uint8_t integer:      8;  // high
	} f;
} U_NUM_TEMPERATURE_FIX_POINT;

typedef U_NUM_TEMPERATURE_FIX_POINT TtempFix;

/* ***********************************************
 * Set encoding offset to 49 to exactly follow the 
 * format of Extended Range SB-TSI encoding.
 * Reference: PID 40821, v1.63, Table 6
 * ***********************************************/
#ifndef U_NUM_TEMPERATURE_FIX_POINT_ENCODING_OFFSET
#define U_NUM_TEMPERATURE_FIX_POINT_ENCODING_OFFSET          49
#endif
#if (U_NUM_TEMPERATURE_FIX_POINT_ENCODING_OFFSET > 255 || U_NUM_TEMPERATURE_FIX_POINT_ENCODING_OFFSET < 0)
#error ("Incorrect defination of F_DPTC_ENCODING_OFFSET")
#endif

TtempFix u_num_temp_flt2fix(TtempFlt flt);
TtempFlt u_num_temp_fix2flt(TtempFix fix);

typedef union {
	uint32_t u32;
	struct {
		uint32_t frac : 23;
		uint32_t exp  : 8;
		uint32_t sign : 1;
	} f;
} U_NUM_SINGLE_PRECISION;

typedef union {
	U_NUM_SINGLE_PRECISION uNum;
	float f32Num;
} U_NUM_TASK_UNSAFE_SINGLE_PRECISION;

typedef union {
	uint16_t u16;
	uint8_t  u8[2];               // [1] integer; [0] decimal
	struct {
		uint8_t u8Dec:        8;  // low
		int8_t  i8Int:        8;  // high
	} f;
} U_NUM_SMU_FRACTIONAL_16;

typedef U_NUM_SMU_FRACTIONAL_16 TtempF16;

TtempF16 u_num_temp_fltTof16(TtempFlt flt);
TtempFlt u_num_temp_f16Toflt(TtempF16 f16);

#define U_FLT_TO_U32(f32)   (*((uint32_t *)&(f32)))
#define U_U32_TO_FLT(u32)   (*((float    *)&(u32)))

#endif // __U_NUMBERS_H__

