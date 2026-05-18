/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */


#ifndef __U_RRQ_H__
#define __U_RRQ_H__

#include <stdint.h>
#include <stdbool.h>

#define U_RRQ_INVALID_RRQ_ID     0xFF

void u_rrq_init( void ); // need to be called before any others rrq functions.

uint8_t u_rrq_setup(void * pBuf, uint32_t length, bool overrideOnFull);
void u_rrq_reset(uint8_t rrqId);
uint32_t u_rrq_put(uint8_t rrqId, void * pBuf, uint32_t length);
uint32_t u_rrq_get(uint8_t rrqId, void * pBuf, uint32_t length);
uint32_t u_rrq_read(uint8_t rrqId, void * pBuf, uint32_t length);
uint32_t u_rrq_getHeadIdx(uint8_t rrqId);
uint32_t u_rrq_getTailIdx(uint8_t rrqId);
uint32_t u_rrq_pattern_get(uint8_t rrqId, 
	void * pStartPtn, uint32_t spLen, void * pEndPtn, uint32_t epLen, 
	void * pBuf, uint32_t length);
uint32_t u_rrq_pattern_read(uint8_t rrqId, 
	void * pStartPtn, uint32_t spLen, void * pEndPtn, uint32_t epLen, 
	void * pBuf, uint32_t length);

typedef enum {
	U_RRQ_POS_MOVE_RIGHT,
	U_RRQ_FRAME_MOVE_RIGHT,

	U_RRQ_MATCH
} U_RRQ_PKG_OPS;
typedef U_RRQ_PKG_OPS (* U_RRQ_PKG_CRAWLER) (uint8_t c, uint32_t pos);
uint32_t u_rrq_package_get(uint8_t rrqId, U_RRQ_PKG_CRAWLER crawler, void * pBuf, uint32_t length);

bool u_rrq_isEmpty(uint8_t rrqId);
bool u_rrq_isFull(uint8_t rrqId);
uint32_t u_rrq_bytesInQueue(uint8_t rrqId);

#endif // end of __U_RRQ_H__

