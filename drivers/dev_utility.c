/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include "dev_utility.h"

char g_logBuf[LOG_BUF_LENGTH];

/**
 * @brief Append formatted string to global log buffer
 *
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 * @return int Number of characters written, or 0 if buffer overflow
 */
int _append2buf(const char *format, ...)
{
    va_list arg;
    int ret;

    size_t len = strlen(g_logBuf);
    if (len >= LOG_BUF_LENGTH - 1) {
        printk("LOG BUFFER OVERFLOWED!!!");
        return 0;
    }

    char * pBuf = &g_logBuf[len];
    size_t rest = LOG_BUF_LENGTH - len;

    va_start(arg, format);
	ret = vsnprintk(pBuf, rest, format, arg);
    va_end(arg);

    // Return the conversion count.
    return(ret);
}