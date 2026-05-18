/*****************************************************************************
 * Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include "board_config.h"
#include "dbgLogFifo.h"
#include "u_rrq.h"

#include <assert.h>
#include <errno.h>

#include <zephyr/logging/log.h>

#include <zephyr/drivers/uart.h>

#define DBG_LOGFIFO_SIZE    (1024 * 8)   // Default size 8K

static uint8_t _s_logFifo[DBG_LOGFIFO_SIZE];
static uint8_t _s_rrqId_logFifo = U_RRQ_INVALID_RRQ_ID;
static K_MUTEX_DEFINE(fifo_mutex);
const struct device *uart_fifo;

/**
 * @brief Initialize the debug log FIFO
 *
 * Sets up the ring queue for debug logging and initializes the UART device.
 */
void dbgLogFifo_init(void)
{
#if (DBG_LOG_FIFO_ENABLE)
	_s_rrqId_logFifo = u_rrq_setup(_s_logFifo, sizeof(_s_logFifo), 1);
	
	uart_fifo = DEVICE_DT_GET(UART_0);

	if (!uart_fifo) {
		assert(uart_fifo);
	};

	assert(_s_rrqId_logFifo != U_RRQ_INVALID_RRQ_ID);
#endif
}

/**
 * @brief Flush the debug log FIFO contents to UART
 *
 * Outputs all buffered log data from the FIFO to the UART device.
 */
void dbgLogFifo_flush2uart (void)
{
#if (DBG_LOG_FIFO_ENABLE)
	k_mutex_lock(&fifo_mutex, K_FOREVER);

	uint32_t head = u_rrq_getHeadIdx(_s_rrqId_logFifo);
	uint32_t tail = u_rrq_getTailIdx(_s_rrqId_logFifo);

	for (uint32_t i = tail; i != head; i = (i + 1) % DBG_LOGFIFO_SIZE) {
		uart_poll_out(uart_fifo, (unsigned char)(_s_logFifo[i]));
	}

	k_mutex_unlock(&fifo_mutex);
#endif
}

/**
 * @brief Print a single character to the debug log FIFO
 *
 * @param c Character to print
 * @return true on success, false if FIFO is not initialized
 */
bool dbgLogFifo_print(uint8_t c)
{
#if (DBG_LOG_FIFO_ENABLE)
	if (_s_rrqId_logFifo == U_RRQ_INVALID_RRQ_ID)
		return 0;

	k_mutex_lock(&fifo_mutex,K_FOREVER);
	uint8_t u8 = (uint8_t)c;
	u_rrq_put(_s_rrqId_logFifo, &u8, 1);
	k_mutex_unlock(&fifo_mutex);
#endif
	return 1;
}

/**
 * @brief Print formatted string to the debug log FIFO
 *
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * @return true on success, false on error
 */
bool dbgLogFifo_printf(const char *format, ...)
{
#if (DBG_LOG_FIFO_ENABLE)

	va_list aptr;
	int strl;
	char buf[100];

	if (NULL == format)
		return false;

	va_start(aptr, format);
	strl = vsnprintk(buf, sizeof(buf), format, aptr);
	va_end(aptr);

	if (strl < 0)
		return false;

	strl = (strl < 100) ? strl : 100;
	if (_s_rrqId_logFifo == U_RRQ_INVALID_RRQ_ID)
		return 0;

	k_mutex_lock(&fifo_mutex, K_FOREVER);
	for (int i = 0; i < strl; i++) {
		uint8_t u8 = (uint8_t)buf[i];
		u_rrq_put(_s_rrqId_logFifo, &u8, 1);
	}
	k_mutex_unlock(&fifo_mutex);
#endif
	return 1;
}

/**
 * @brief Print formatted string directly to UART
 *
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * @return true on success, false on error
 */
bool dbgLog_printf(const char *format, ...)
{
#if (DBG_LOG_FIFO_ENABLE)

	va_list aptr;
	int strl;
	char buf[100];

	if (NULL == format)
		return false;

	va_start(aptr, format);
	strl = vsnprintk(buf, sizeof(buf), format, aptr);
	va_end(aptr);

	if (strl < 0)
		return false;

	strl = (strl < 100) ? strl : 100;
	k_mutex_lock(&fifo_mutex, K_FOREVER);
	for (int i = 0; i < strl; i++) {
		uint8_t u8 = (uint8_t)buf[i];
		uart_poll_out(uart_fifo, u8);
	}
	k_mutex_unlock(&fifo_mutex);
#endif
	return 1;
}