/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "u_numbers.h"

/**
 * u_num_temp_flt2fix - Convert floating point temperature to fixed point format
 * @flt: Floating point temperature value
 *
 * Return: Fixed point temperature representation
 */
TtempFix u_num_temp_flt2fix(TtempFlt flt)
{
	TtempFix fix = {0x001F};
	uint32_t u32Tmp;
	int32_t  i32Tmp;

	if (flt < -U_NUM_TEMPERATURE_FIX_POINT_ENCODING_OFFSET)
		return fix;

	if (flt > (255.875 - U_NUM_TEMPERATURE_FIX_POINT_ENCODING_OFFSET)) {
		fix.u16 = 0xFFFF;
		return fix;
	}

	i32Tmp = flt * 1000;
	i32Tmp += (U_NUM_TEMPERATURE_FIX_POINT_ENCODING_OFFSET * 1000);
	u32Tmp = i32Tmp % 1000;

	fix.f.integer = (i32Tmp / 1000);
	fix.f.decimal = u32Tmp / 125;
	fix.f.reserved_4_0 = 0;

	return fix;
}

/**
 * u_num_temp_fix2flt - Convert fixed point temperature to floating point format
 * @fix: Fixed point temperature value
 *
 * Return: Floating point temperature representation
 */
TtempFlt u_num_temp_fix2flt(TtempFix fix)
{
	TtempFlt flt = 0.0;

	flt = ((int32_t)fix.f.integer - U_NUM_TEMPERATURE_FIX_POINT_ENCODING_OFFSET);
	flt += (((TtempFlt)(fix.f.decimal * 125)) / 1000.0f);

	return flt;
}

/**
 * u_num_temp_fltTof16 - Convert floating point temperature to F16 format
 * @flt: Floating point temperature value
 *
 * Return: F16 temperature representation with saturation handling
 */
TtempF16 u_num_temp_fltTof16(TtempFlt flt)
{
	TtempF16 f16;
	if (flt >= 127.99609375f) {
		f16.u16 = 0x7FFF;
		return f16;
	}

	if (flt <= -128.0f) {
		f16.u16 = 0x8000;
		return f16;
	}

	f16.u16 = (uint16_t)((int16_t)(flt / (1.0 / 256)));
	return f16;
}

/**
 * u_num_temp_f16Toflt - Convert F16 temperature to floating point format
 * @f16: F16 temperature value
 *
 * Return: Floating point temperature representation
 */
TtempFlt u_num_temp_f16Toflt(TtempF16 f16)
{
	return (TtempFlt)((1.0 / 256) * (int16_t)f16.u16);
}