/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "u_crc8.h"
#include <assert.h>

static struct {
	uint8_t             poly;
	uint8_t             init;
	bool                refIn;
	bool                refOut;
	uint8_t             xorOut;
	uint8_t             check;
} _s_crc8Cfg[] = {
  /* Poly, Init, RefIn, RefOut, XorOut, Check */
	{0x07, 0x00, false, false,  0x00,   0xF4}, // U_CRC8_ALG_CRC8
	{0x9B, 0xFF, false, false,  0x00,   0xDA}, // U_CRC8_ALG_CDMA2000
	{0x9B, 0xFF, false, false,  0x00,   0xDA}, // U_CRC8_ALG_DARC
	{0xD5, 0x00, false, false,  0x00,   0xBC}, // U_CRC8_ALG_DVB_S2
	{0x1D, 0xFF, true,  true,   0x00,   0x97}, // U_CRC8_ALG_EBU
	{0x1D, 0xFD, false, false,  0x00,   0x7E}, // U_CRC8_ALG_I_CODE
 	{0x07, 0x00, false, false,  0x55,   0xA1}, // U_CRC8_ALG_ITU
 	{0x31, 0x00, true,  true,   0x00,   0xA1}, // U_CRC8_ALG_MAXIM
	{0x07, 0xFF, true,  true,   0x00,   0xD0}, // U_CRC8_ALG_ROHC
	{0x9B, 0x00, true,  true,   0x00,   0x25}, // U_CRC8_ALG_WCDMA
};

/**
 * u_crc8_makeTable - Generate CRC8 lookup table for specified algorithm
 * @alg: CRC8 algorithm type
 * @crc8Table: Pointer to table buffer to populate
 * @length: Length of the table buffer
 *
 * Returns true if table was fully generated (length >= 256), false otherwise
 */
bool u_crc8_makeTable(U_CRC8_EM_ALGORITHM alg, uint8_t * crc8Table, uint32_t length)
{
	uint8_t u8crc, tmp;

	if (alg >= U_CRC8_ALG_MAX_SUPPORT)
		return false;

	for (uint32_t i = 0; i < length && i < 256; i++) {
		u8crc = i;
		for (uint32_t j = 0; j < 8; j++) {
			tmp = u8crc & 0x80;
			u8crc <<= 1;
			if (0 != tmp) {
				u8crc ^= _s_crc8Cfg[alg].poly;
			}
		}

		crc8Table[i] = u8crc;
	}

	return length < 256 ? false : true;
}

static uint8_t _s_reverse[] = { 
	0x00, 0x08, 0x04, 0x0C, 0x02, 0x0A, 0x06, 0x0E,
	0x01, 0x09, 0x05, 0x0D, 0x03, 0x0B, 0x07, 0x0F
};

/**
 * _u_crc8_reversByte - Reverse bits in a byte
 * @b: Byte to reverse
 *
 * Returns the bit-reversed byte
 */
uint8_t _u_crc8_reversByte(uint8_t b)
{
	return ((_s_reverse[b & 0x0F] << 4) | _s_reverse[(b >> 4) & 0x0F]);
}

/**
 * u_crc8_init - Initialize CRC8 context for calculation
 * @pCtx: Pointer to CRC8 context structure
 */
void u_crc8_init(U_CRC8_CONTEXT * pCtx)
{
	assert (pCtx->alg < U_CRC8_ALG_MAX_SUPPORT);
	assert (pCtx->pCrcTable != NULL);

	pCtx->u32bytes = 0;
	pCtx->u8crc = _s_crc8Cfg[pCtx->alg].init;
}

/**
 * u_crc8_push - Add a byte to the CRC8 calculation
 * @pCtx: Pointer to CRC8 context structure
 * @byte: Byte to add to CRC calculation
 */
void u_crc8_push(U_CRC8_CONTEXT * pCtx, uint8_t byte)
{
	if (_s_crc8Cfg[pCtx->alg].refIn) {
		byte = _u_crc8_reversByte(byte);
	}

	pCtx->u8crc = pCtx->pCrcTable[ pCtx->u8crc ^ byte ];
}

/**
 * u_crc8_finsh - Finalize CRC8 calculation and return result
 * @pCtx: Pointer to CRC8 context structure
 *
 * Returns the final CRC8 value
 */
uint8_t u_crc8_finsh(U_CRC8_CONTEXT * pCtx)
{
	if (_s_crc8Cfg[pCtx->alg].refOut) {
		pCtx->u8crc = _u_crc8_reversByte(pCtx->u8crc);
	}

	pCtx->u8crc ^= _s_crc8Cfg[pCtx->alg].xorOut;
	
	return pCtx->u8crc;
}