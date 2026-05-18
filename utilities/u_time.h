/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#ifndef __U_TIME_H__
#define __U_TIME_H__

#include <zephyr/kernel.h>

#define UNIX_EPOCH_YEAR       1970
#define THE_LEAP_MONTH        2
#define IS_LEAP_YEAR(y)       ( ((((y) & 3) == 0) && ((y) % 100 != 0)) || ((y) % 400 == 0) )

#define DAYS_PER_YEAR         365

#define SECONDS_PER_MIN       60
#define SECONDS_PER_HOUR      (60 * SECONDS_PER_MIN)
#define SECONDS_PER_DAY       (24 * SECONDS_PER_HOUR)
#define SECONDS_PER_YEAR      (DAYS_PER_YEAR * SECONDS_PER_DAY)
#define SECONDS_IN_LEAP_YEAR  (SECONDS_PER_YEAR + SECONDS_PER_DAY)

typedef uint32_t T_UNIX_TIMESTAMP; // The seconds since 1970/1/1 8:0:0 AM
                                   // Overflow at 2106/2/7 14:28:15

typedef struct {
	uint16_t u16year;
	uint8_t  u8mon;
	uint8_t  u8day;
	uint8_t  u8hrs;
	uint8_t  u8min;
	uint8_t  u8sec;
} T_RTC_TIMESTAMP;

T_RTC_TIMESTAMP u_time_unix2rtc(T_UNIX_TIMESTAMP ts);
T_UNIX_TIMESTAMP u_time_rtc2unix(T_RTC_TIMESTAMP * pRtc);
void u_time_unix2str(T_UNIX_TIMESTAMP ts, char * pStr);
void u_time_rtc2str(T_RTC_TIMESTAMP * pRtc, char * pStr);

#endif // __U_TIME_H__

