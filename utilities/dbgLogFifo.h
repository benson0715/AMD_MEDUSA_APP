/*****************************************************************************
 * Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#ifndef __DBG_LOGFIFO__
#define __DBG_LOGFIFO__

#include <zephyr/kernel.h>

void dbgLogFifo_init (void);
void dbgLogFifo_flush2uart (void);
bool dbgLogFifo_print(uint8_t c);
bool dbgLogFifo_printf(const char * format, ...);
bool dbgLog_printf(const char *format, ...);


#endif // end of __DBG_LOGFIFO__

