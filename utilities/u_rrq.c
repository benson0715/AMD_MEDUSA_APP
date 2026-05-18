/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "u_rrq.h"
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <zephyr/kernel.h>

#ifndef U_RRQ_MAX_QUEUE_NUM
#define U_RRQ_MAX_QUEUE_NUM   10
#endif

#ifndef U_RRQ_DEBUG
#define U_RRQ_DEBUG           0
#endif

typedef struct {
	uint8_t * pBuf;
	uint32_t  bufLen;

	bool overrideOnFull;             // 1 - Override the oldest data on full. 
	                                 // 0 - discard the latest data on full. (lock-free in 1p+1c case)

	uint32_t head;                   // point to the space for next char
	uint32_t tail;                   // point to the first-in char if not equal to head.

	uint32_t inputCounter;
	uint32_t outputCounter;

	struct k_mutex sLockProducer; // One producer is allowed in a time
	struct k_mutex sLockConsumer; // One consumer is allowed in a time
} U_RRQ_HANDLER;

static U_RRQ_HANDLER _s_rrqCfg[U_RRQ_MAX_QUEUE_NUM];
static bool _s_moduleInitFlag = false;

#if (U_RRQ_DEBUG)
static void _dumpFifoContent (U_RRQ_HANDLER * pCfg, uint32_t startIdx, uint32_t endIdx, uint8_t * pDesp)
{
	ENTER_CRITICAL_ZONE();

	LOG_CLEARBUF;
	for (uint32_t pos = startIdx; pos != endIdx; pos = (pos + 1) % pCfg->bufLen)
		LOG_APPEND("%02X ", pCfg->pBuf[ pos ]);
	LOG("%s (%d): %s", pDesp ? pDesp : "", (startIdx <= endIdx) ? endIdx - startIdx : endIdx + pCfg->bufLen - startIdx, LOG_BUF);

	EXIT_CRITICAL_ZONE();
}
#endif

/**
 * @brief Initialize the RRQ module
 *
 * Clears all queue configurations and sets the module initialization flag.
 */
void u_rrq_init()
{
	memset(_s_rrqCfg, 0, sizeof(_s_rrqCfg));
	_s_moduleInitFlag = true;
}

/**
 * @brief Setup a new ring queue
 *
 * @param pBuf Pointer to the buffer to use for the queue
 * @param length Length of the buffer in bytes
 * @param overrideOnFull If true, override oldest data when full; if false, discard new data
 * @return Queue ID on success, U_RRQ_INVALID_RRQ_ID on failure
 */
uint8_t u_rrq_setup(void * pBuf, uint32_t length, bool overrideOnFull)
{
	assert (_s_moduleInitFlag);

	uint8_t rrqId;
	U_RRQ_HANDLER * pCfg;

	for (rrqId = 0; rrqId < U_RRQ_MAX_QUEUE_NUM; rrqId ++) {
		pCfg = &_s_rrqCfg[rrqId];

		if (NULL == pCfg->pBuf && 0 == pCfg->bufLen) {
			memset(pCfg, 0, sizeof(U_RRQ_HANDLER));

			pCfg->pBuf = pBuf;
			pCfg->bufLen = length;
			pCfg->overrideOnFull = overrideOnFull;

			k_mutex_init(&pCfg->sLockProducer);
			k_mutex_init(&pCfg->sLockConsumer);

			return rrqId;
		}
	}

	return U_RRQ_INVALID_RRQ_ID;
}

/* Empty the queue */
void u_rrq_reset(uint8_t rrqId)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);
	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];

	/* Lock Producer first (for deadlock avoidance) */
	if (pCfg->overrideOnFull) {
		k_mutex_lock(&pCfg->sLockProducer, K_FOREVER);
	}
	k_mutex_lock(&pCfg->sLockConsumer, K_FOREVER);

#if (U_RRQ_DEBUG)
	if (pCfg->tail != pCfg->head)
		_dumpFifoContent(pCfg, pCfg->tail, pCfg->head, "[INFO] rrq_reset drop"); // to print the discarded data
#endif

	pCfg->tail = pCfg->head;

	pCfg->outputCounter = pCfg->inputCounter;

	k_mutex_unlock(&pCfg->sLockConsumer);
	if (pCfg->overrideOnFull) {
		k_mutex_unlock(&pCfg->sLockProducer);
	}
}

/* 
 * returns how many bytes had inserted to RRQ, update 'head'
 */
uint32_t u_rrq_put(uint8_t rrqId, void * pBuf, uint32_t length)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);
	assert (pBuf != NULL);

	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	uint8_t * pData = (uint8_t *)pBuf;
	if (!k_is_in_isr()) {
		k_mutex_lock(&pCfg->sLockProducer, K_FOREVER);
	}

	// do the loop until putBytes == length or queue is full
	uint32_t putBytes = 0;
	while ( putBytes < length && (((pCfg->head + 1) % pCfg->bufLen) != pCfg->tail) ) {

		/* put one byte in a time */
		pCfg->pBuf[ pCfg->head ] = pData[putBytes];

		/* update head pointer */
		pCfg->head = (pCfg->head + 1) % pCfg->bufLen;

		putBytes ++;
	}

	if (pCfg->overrideOnFull && putBytes != length) {
		
		if (!k_is_in_isr()) {
			k_mutex_lock(&pCfg->sLockConsumer, K_FOREVER);
		}
		while (putBytes < length) {
			/* put one byte in a time */
			pCfg->pBuf[ pCfg->head ] = pData[putBytes];

			/* update head pointer, and tail pointer if the queue is full */
			pCfg->head = (pCfg->head + 1) % pCfg->bufLen;
			if (pCfg->head == pCfg->tail) {
				pCfg->tail = (pCfg->tail + 1) % pCfg->bufLen;
				pCfg->outputCounter ++; // discarded although
			}

			putBytes ++;
		}

		if (!k_is_in_isr()) {
			k_mutex_unlock(&pCfg->sLockConsumer);
		}
	}

	pCfg->inputCounter += putBytes;
	if (!k_is_in_isr()) {
		k_mutex_unlock(&pCfg->sLockProducer);
	}

	return putBytes;
}

/* 
 * returns how many bytes fetched from RRQ, update 'tail'
 */
uint32_t u_rrq_get(uint8_t rrqId, void * pBuf, uint32_t length)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);
	assert (pBuf != NULL);

	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	uint8_t * pData = (uint8_t *)pBuf;

	/* Lock Producer first (for deadlock avoidance) */
	if (pCfg->overrideOnFull) {
		if (!k_is_in_isr()) {
			k_mutex_lock(&pCfg->sLockProducer, K_FOREVER);
		}
	}

	if (!k_is_in_isr()) {
		k_mutex_lock(&pCfg->sLockConsumer, K_FOREVER);
	}
	// do the loop until gutBytes == length or queue is empty
	uint32_t getBytes = 0;
	while (getBytes < length && pCfg->head != pCfg->tail) {

		/* get one byte in a time */
		pData[getBytes] = pCfg->pBuf[ pCfg->tail ];

		/* update tail pointer */
		pCfg->tail = (pCfg->tail + 1) % pCfg->bufLen;

		getBytes ++;
	}

	pCfg->outputCounter += getBytes;
	if (!k_is_in_isr()) {
		k_mutex_unlock(&pCfg->sLockConsumer);
	}
	if (pCfg->overrideOnFull) {
		if (!k_is_in_isr()) {
			k_mutex_unlock(&pCfg->sLockProducer);
		}
	}

	return getBytes;
}

/* 
 * returns how many bytes fetched from RRQ, don't update 'tail'
 */
uint32_t u_rrq_read(uint8_t rrqId, void * pBuf, uint32_t length)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);
	assert (pBuf != NULL);

	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	uint8_t * pData = (uint8_t *)pBuf;

	/* Lock Producer first (for deadlock avoidance) */
	if (pCfg->overrideOnFull) {
		k_mutex_lock(&pCfg->sLockProducer, K_FOREVER);
	}
	k_mutex_lock(&pCfg->sLockConsumer, K_FOREVER);

	// do the loop until gutBytes == length or queue is empty
	uint32_t readBytes = 0;
	uint32_t tail = pCfg->tail;
	while (readBytes < length && pCfg->head != tail) {

		/* read one byte in a time */
		pData[readBytes] = pCfg->pBuf[ tail ];

		/* tail points to the next one */
		tail = (tail + 1) % pCfg->bufLen;

		readBytes ++;
	}

	k_mutex_unlock(&pCfg->sLockConsumer);
	if (pCfg->overrideOnFull) {
		k_mutex_unlock(&pCfg->sLockProducer);
	}

	return readBytes;
}

/**
 * @brief Get the current head index of the queue
 *
 * @param rrqId Queue ID
 * @return Current head index
 */
uint32_t u_rrq_getHeadIdx(uint8_t rrqId)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);

	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	return pCfg->head;
}

/**
 * @brief Get the current tail index of the queue
 *
 * @param rrqId Queue ID
 * @return Current tail index
 */
uint32_t u_rrq_getTailIdx(uint8_t rrqId)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);

	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	return pCfg->tail;
}

/* 
 * returns how many bytes fetched from RRQ, update 'tail'
 */
uint32_t u_rrq_pattern_get(uint8_t rrqId, 
	void * pStartPtn, uint32_t spLen, void * pEndPtn, uint32_t epLen, 
	void * pBuf, uint32_t length)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);
	assert (pBuf != NULL);

	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	uint8_t * pData = (uint8_t *)pBuf;

	/* Lock Producer first (for deadlock avoidance) */
	if (pCfg->overrideOnFull) {
		k_mutex_lock(&pCfg->sLockProducer, K_FOREVER);
	}
	k_mutex_lock(&pCfg->sLockConsumer, K_FOREVER);

	/* Serach for the frame start and end */
	uint32_t frameStart = 0xFFFFFFFF;
	uint32_t frameEndPlus1 = 0xFFFFFFFF;
	uint32_t skippedBytes = 0;

	if (pStartPtn && spLen) {
		uint32_t k = 0; // how many bytes be skipped before the frame
		for (uint32_t i = pCfg->tail; i != pCfg->head; i = (i + 1) % pCfg->bufLen, k++) {

			uint32_t j;
			for (j = 0; 
					j < spLen &&                                        // not reach to the pattern ends
					(i + j) % pCfg->bufLen != pCfg->head &&             // current position in buf is not reach to the head
					pCfg->pBuf[(i + j) % pCfg->bufLen] == ((uint8_t *)pStartPtn)[j]; // char in buf is equal to the char pattern
			j++);

			if (j == spLen) {
				skippedBytes = k;
				frameStart = i;

				break; // for (uint32_t i ...
			}
		}
	} else {
		frameStart = pCfg->tail;
		spLen = 0;
	}

	if (pEndPtn && epLen && frameStart != 0xFFFFFFFF) {
		uint32_t k = 0; // how many bytes in frame body
		for (uint32_t i = (frameStart + spLen) % pCfg->bufLen; i != pCfg->head; i = (i + 1) % pCfg->bufLen, k++) {

			uint32_t j;
			for (j = 0; 
					j < epLen &&                                        // not reach to the pattern ends
					(i + j) % pCfg->bufLen != pCfg->head &&             // current position in buf is not reach to the head
					pCfg->pBuf[(i + j) % pCfg->bufLen] == ((uint8_t *)pEndPtn)[j];   // char in buf is equal to the char pattern
			j++);

			if (j == epLen) {
				// body length = k;
				frameEndPlus1 = (i + epLen) % pCfg->bufLen;

				break; // for (uint32_t i ...
			}
		}
	} else {
		frameEndPlus1 = pCfg->head;
	}

	/* fetch data from queue */
	uint32_t getBytes = 0;
	if (frameStart != frameEndPlus1 && frameStart != 0xFFFFFFFF && frameEndPlus1 != 0xFFFFFFFF) {
#if (U_RRQ_DEBUG)
		if (pCfg->tail != frameStart)
			_dumpFifoContent(pCfg, pCfg->tail, frameStart, "[INFO] rrq_ptn_get drop"); // to print the discarded data
#endif
		/* release the space of skipped bytes */
		pCfg->tail = frameStart;

		while (getBytes < length && pCfg->tail != frameEndPlus1) {

			/* get one byte in a time */
			pData[getBytes] = pCfg->pBuf[ pCfg->tail ];

			/* update tail pointer */
			pCfg->tail = (pCfg->tail + 1) % pCfg->bufLen;

			getBytes ++;
		}

		pCfg->outputCounter += (skippedBytes + getBytes);
	}

	k_mutex_unlock(&pCfg->sLockConsumer);
	if (pCfg->overrideOnFull) {
		k_mutex_unlock(&pCfg->sLockProducer);
	}

	return getBytes;
}

/* 
 * returns how many bytes fetched from RRQ, don't update 'tail'
 */
uint32_t u_rrq_pattern_read(uint8_t rrqId, 
	void * pStartPtn, uint32_t spLen, void * pEndPtn, uint32_t epLen, 
	void * pBuf, uint32_t length)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);
	assert (pBuf != NULL);

	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	uint8_t * pData = (uint8_t *)pBuf;

	/* Lock Producer first (for deadlock avoidance) */
	if (pCfg->overrideOnFull) {
		k_mutex_lock(&pCfg->sLockProducer, K_FOREVER);
	}
	k_mutex_lock(&pCfg->sLockConsumer, K_FOREVER);

	/* Serach for the frame start and end */
	uint32_t frameStart = 0xFFFFFFFF;
	uint32_t frameEndPlus1 = 0xFFFFFFFF;
	uint32_t skippedBytes = 0;

	if (pStartPtn && spLen) {
		uint32_t k = 0; // how many bytes be skipped before the frame
		for (uint32_t i = pCfg->tail; i != pCfg->head; i = (i + 1) % pCfg->bufLen, k++) {

			uint32_t j;
			for (j = 0; 
					j < spLen &&										// not reach to the pattern ends
					(i + j) % pCfg->bufLen != pCfg->head && 			// current position in buf is not reach to the head
					pCfg->pBuf[(i + j) % pCfg->bufLen] == ((uint8_t *)pStartPtn)[j]; // char in buf is equal to the char pattern
			j++);

			if (j == spLen) {
				skippedBytes = k;
				frameStart = i;

				break; // for (uint32_t i ...
			}
		}
	} else {
		frameStart = pCfg->tail;
		spLen = 0;
	}

	if (pEndPtn && epLen && frameStart != 0xFFFFFFFF) {
		uint32_t k = 0; // how many bytes in frame body
		for (uint32_t i = (frameStart + spLen) % pCfg->bufLen; i != pCfg->head; i = (i + 1) % pCfg->bufLen, k++) {

			uint32_t j;
			for (j = 0; 
					j < epLen &&										// not reach to the pattern ends
					(i + j) % pCfg->bufLen != pCfg->head && 			// current position in buf is not reach to the head
					pCfg->pBuf[(i + j) % pCfg->bufLen] == ((uint8_t *)pEndPtn)[j];	// char in buf is equal to the char pattern
			j++);

			if (j == epLen) {
				// body length = k;
				frameEndPlus1 = (i + epLen) % pCfg->bufLen;

				break; // for (uint32_t i ...
			}
		}
	} else {
		frameEndPlus1 = pCfg->head;
	}

	/* read data from queue */
	uint32_t readBytes = 0;
	if (frameStart != frameEndPlus1 && frameStart != 0xFFFFFFFF && frameEndPlus1 != 0xFFFFFFFF) {
#if (U_RRQ_DEBUG)
		if (pCfg->tail != frameStart)
			_dumpFifoContent(pCfg, pCfg->tail, frameStart, "[INFO] rrq_pkg_get skip"); // to print the skipped data
#endif

		uint32_t tail = frameStart;

		while (readBytes < length && tail != frameEndPlus1) {

			/* read one byte in a time */
			pData[readBytes] = pCfg->pBuf[ tail ];

			/* tail points to the next one */
			tail = (tail + 1) % pCfg->bufLen;

			readBytes ++;
		}
	}

	k_mutex_unlock(&pCfg->sLockConsumer);
	if (pCfg->overrideOnFull) {
		k_mutex_unlock(&pCfg->sLockProducer);
	}

	return readBytes;
}

/* 
 * If package is found, and length is not zero, bytes bfore it will be discarded.
 * Nothing will be changed if package is not found.
 */
uint32_t u_rrq_package_get(uint8_t rrqId, U_RRQ_PKG_CRAWLER crawler, void * pBuf, uint32_t length)
{
	assert (rrqId < U_RRQ_MAX_QUEUE_NUM);
	assert (pBuf != NULL);

	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	uint8_t * pData = (uint8_t *)pBuf;

	/* Lock Producer first (for deadlock avoidance) */
	if (pCfg->overrideOnFull) {
		k_mutex_lock(&pCfg->sLockProducer, K_FOREVER);
	}
	k_mutex_lock(&pCfg->sLockConsumer, K_FOREVER);

	uint32_t getBytes = 0;

	/* Serach for the frame start and end */
	uint32_t frameOffset = 0;
	uint32_t posInFrame = 0;

	uint32_t frameStart = pCfg->tail;
	/* Frame traverses the whole queue */
	for (frameStart = pCfg->tail; frameStart != pCfg->head; frameOffset++, frameStart = (frameStart + 1) % pCfg->bufLen) {
		// frameOffset is the byte number be skipped

		posInFrame = 0;

		while (1) {
			// 1. calculate current position
			uint32_t p = (frameStart + posInFrame) % pCfg->bufLen;
			if (p == pCfg->head)
				break; // move to next frame

			U_RRQ_PKG_OPS ops = crawler( pCfg->pBuf[p], posInFrame );
			if (U_RRQ_POS_MOVE_RIGHT == ops) {
				posInFrame ++;
				continue;
			} else if (U_RRQ_MATCH == ops) {
				// posInFrame points to the last char in the package

				if (posInFrame >= length) {
					/* Output buffer is not long enough for the package */
					// LOG("[WARNING] u_rrq_package_get: Output buffer is not long enough for the package");
				}

				if (!length) // Don't discard any characters if nothing will be copied
					goto __rrq_package_get_exit;

				/* Copy the package to output buf */
#if (U_RRQ_DEBUG)
				if (pCfg->tail != frameStart)
					_dumpFifoContent(pCfg, pCfg->tail, frameStart, "[INFO] rrq_pkg_get drop"); // to print the discarded data
#endif
				// 1. Discard the bytes before the frameStart
				pCfg->tail = frameStart;

				// 2. Copy the whole package
				uint32_t frameEndPlus1 = (frameStart + posInFrame + 1) % pCfg->bufLen;
				while (getBytes < length && pCfg->tail != frameEndPlus1) {

					/* get one byte in a time */
					pData[getBytes] = pCfg->pBuf[ pCfg->tail ];

					/* update tail pointer */
					pCfg->tail = (pCfg->tail + 1) % pCfg->bufLen;

					getBytes ++;
				} 
				goto __rrq_package_get_exit;

			} else {
				/* U_RRQ_FRAME_MOVE_RIGHT or others */
				// break the while so as to move to next frame
				break;
			}
		}
	}

__rrq_package_get_exit:

	if (getBytes) {
		pCfg->outputCounter += (frameOffset + getBytes);
	}

	k_mutex_unlock(&pCfg->sLockConsumer);
	if (pCfg->overrideOnFull) {
		k_mutex_unlock(&pCfg->sLockProducer);
	}

	return getBytes;
}

/**
 * @brief Check if the queue is empty
 *
 * @param rrqId Queue ID
 * @return true if queue is empty, false otherwise
 */
bool u_rrq_isEmpty(uint8_t rrqId)
{
	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];

	return (pCfg->head == pCfg->tail) ? true : false;
}

/**
 * @brief Check if the queue is full
 *
 * @param rrqId Queue ID
 * @return true if queue is full, false otherwise
 */
bool u_rrq_isFull(uint8_t rrqId)
{
	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];
	
	return ((pCfg->head + 1) % pCfg->bufLen == pCfg->tail) ? true : false;
}

/**
 * @brief Get the number of bytes currently in the queue
 *
 * @param rrqId Queue ID
 * @return Number of bytes in the queue
 */
uint32_t u_rrq_bytesInQueue(uint8_t rrqId)
{
	U_RRQ_HANDLER * pCfg = &_s_rrqCfg[rrqId];

	uint32_t bytes = pCfg->inputCounter - pCfg->outputCounter;

	if (bytes < pCfg->bufLen)
		return bytes;

	/* outputCounter overflow but inputCounter not */
	k_mutex_lock(&pCfg->sLockProducer, K_FOREVER);
	k_mutex_lock(&pCfg->sLockConsumer, K_FOREVER);

	bytes = (0xFFFFFFFF - pCfg->outputCounter) + pCfg->inputCounter + 1;

	k_mutex_unlock(&pCfg->sLockConsumer);
	k_mutex_unlock(&pCfg->sLockProducer);
	
	return bytes;
}