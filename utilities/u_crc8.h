/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */


#ifndef __U_CRC8_H__
#define __U_CRC8_H__

#include <zephyr/kernel.h>

typedef enum {
	U_CRC8_ALG_CRC8     = 0,
	U_CRC8_ALG_CDMA2000 = 1,
	U_CRC8_ALG_DARC     = 2,
	U_CRC8_ALG_DVB_S2   = 3,
	U_CRC8_ALG_EBU      = 4,
	U_CRC8_ALG_I_CODE   = 5,
	U_CRC8_ALG_ITU      = 5,
	U_CRC8_ALG_MAXIM    = 7,
	U_CRC8_ALG_ROHC     = 8,
	U_CRC8_ALG_WCDMA    = 9,

	U_CRC8_ALG_MAX_SUPPORT
} U_CRC8_EM_ALGORITHM;

typedef struct {
	U_CRC8_EM_ALGORITHM alg;
	uint8_t *           pCrcTable;
	uint8_t             u8crc;
	uint32_t            u32bytes;
} U_CRC8_CONTEXT;

bool u_crc8_makeTable(U_CRC8_EM_ALGORITHM alg, uint8_t * crc8Table, uint32_t length);
void u_crc8_init(U_CRC8_CONTEXT * pCtx);
void u_crc8_push(U_CRC8_CONTEXT * pCtx, uint8_t byte);
uint8_t u_crc8_finsh(U_CRC8_CONTEXT * pCtx);

#endif // end of __U_CRC8_H__

