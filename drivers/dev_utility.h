/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __DEV_UTILITY_H__
#define __DEV_UTILITY_H__

#include <stdint.h>
#include <stdbool.h>

#define     c0000               ( 0 )
#define     c0001               ( 1 )
#define     c0010               ( 2 )
#define     c0011               ( 3 )
#define     c0100               ( 4 )
#define     c0101               ( 5 )
#define     c0110               ( 6 )
#define     c0111               ( 7 )
#define     c1000               ( 8 )
#define     c1001               ( 9 )
#define     c1010               ( 10 )
#define     c1011               ( 11 )
#define     c1100               ( 12 )
#define     c1101               ( 13 )
#define     c1110               ( 14 )
#define     c1111               ( 15 )
#define     MASK(bitpos)        ( 1ul << (bitpos)) //  Create mask for selected bit
#define     cbit( x )           ( 1ul << (x) )
#define     c8bit(x,y)          ( (uint8_t) ((c##x <<4 )| c##y ))
#define     c16bit(w,x,y,z)     ( (uint16_t)((c##w <<12)|(c##x <<8)|(c##y <<4)|c##z))

#define LOG_BUF_LENGTH           256
extern char g_logBuf[LOG_BUF_LENGTH];
#define LOG_BUF                  g_logBuf
#define LOG_CLEARBUF             g_logBuf[0]='\0'
extern int _append2buf(const char *format, ...);
#define LOG_APPEND(frmt, ...)    _append2buf(frmt, ##__VA_ARGS__)

#endif // end of __DEV_UTILITY_H__

