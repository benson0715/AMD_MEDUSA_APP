/*****************************************************************************
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 ******************************************************************************
 */

#include "u_time.h"
#include <stdio.h>

static uint8_t _s_days_per_month[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/**
 * u_time_unix2rtc - Convert Unix timestamp to RTC timestamp
 * @ts: Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
 *
 * Converts a Unix timestamp to an RTC timestamp structure with
 * timezone adjustment for UTC+8.
 *
 * Return: RTC timestamp structure
 */
T_RTC_TIMESTAMP u_time_unix2rtc(T_UNIX_TIMESTAMP ts)
{
	T_RTC_TIMESTAMP rtc = {UNIX_EPOCH_YEAR, 1, 1, 0, 0, 0};
	T_UNIX_TIMESTAMP rest = ts;
	rest += (8 * SECONDS_PER_HOUR);

	while (rest != 0) {
		if ( IS_LEAP_YEAR(rtc.u16year) ) {
			if (rest >= SECONDS_IN_LEAP_YEAR) {
				rest -= SECONDS_IN_LEAP_YEAR;
				rtc.u16year ++;
			} else {
				break;
			}
		} else {
			if (rest >= SECONDS_PER_YEAR) {
				rest -= SECONDS_PER_YEAR;
				rtc.u16year ++;
			} else {
				break;
			}
		}
	}

	if (rest >= _s_days_per_month[0] * SECONDS_PER_DAY) {
		rest -= _s_days_per_month[0] * SECONDS_PER_DAY;
		rtc.u8mon ++;
	}

	uint8_t isLeapYear = IS_LEAP_YEAR(rtc.u16year) ? 1 : 0;
	if (rest >= (_s_days_per_month[1] + isLeapYear) * SECONDS_PER_DAY) {
		rest -= (_s_days_per_month[1] + isLeapYear) * SECONDS_PER_DAY;
		rtc.u8mon ++;
	}

	while (rest >= _s_days_per_month[ rtc.u8mon - 1 ] * SECONDS_PER_DAY) {
		rest -= _s_days_per_month[ rtc.u8mon - 1 ] * SECONDS_PER_DAY;
		rtc.u8mon ++;
	}

	rtc.u8day += (rest / SECONDS_PER_DAY);
	rest %= SECONDS_PER_DAY;

	rtc.u8hrs += (rest / SECONDS_PER_HOUR);
	rest %= SECONDS_PER_HOUR;

	rtc.u8min += (rest / SECONDS_PER_MIN);
	rest %= SECONDS_PER_MIN;

	rtc.u8sec += rest;

	return rtc;
}

/**
 * u_time_rtc2unix - Convert RTC timestamp to Unix timestamp
 * @pRtc: Pointer to RTC timestamp structure
 *
 * Converts an RTC timestamp structure to Unix timestamp with
 * timezone adjustment for UTC+8.
 *
 * Return: Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
 */
T_UNIX_TIMESTAMP u_time_rtc2unix(T_RTC_TIMESTAMP * pRtc)
{
	T_UNIX_TIMESTAMP ts = 0;

	for (uint16_t y = UNIX_EPOCH_YEAR; y < pRtc->u16year; y++) {
		if (IS_LEAP_YEAR(y)) {
			ts += SECONDS_IN_LEAP_YEAR;
		} else {
			ts += SECONDS_PER_YEAR;
		}
	}

	if (IS_LEAP_YEAR(pRtc->u16year) && pRtc->u8mon > THE_LEAP_MONTH) {
		ts += SECONDS_PER_DAY;
	}
	for (uint8_t m = 1; m < pRtc->u8mon; m++) {
		ts += (_s_days_per_month[m - 1] * SECONDS_PER_DAY);
	}
	ts += ((pRtc->u8day - 1) * SECONDS_PER_DAY);
	ts += (pRtc->u8hrs * SECONDS_PER_HOUR);
	ts += (pRtc->u8min * SECONDS_PER_MIN);
	ts += pRtc->u8sec;
	ts -= (8 * SECONDS_PER_HOUR);

	return ts;
}

/**
 * u_time_unix2str - Convert Unix timestamp to formatted string
 * @ts: Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
 * @pStr: Output buffer for formatted date-time string (YYYY-MM-DD HH:MM:SS)
 *
 * Converts a Unix timestamp to a human-readable date-time string.
 */
void u_time_unix2str(T_UNIX_TIMESTAMP ts, char * pStr)
{
	T_RTC_TIMESTAMP rtc = u_time_unix2rtc(ts);
	sprintf(pStr, "%04d-%02d-%02d %02d:%02d:%02d", rtc.u16year, rtc.u8mon, rtc.u8day, rtc.u8hrs, rtc.u8min, rtc.u8sec);
}

/**
 * u_time_rtc2str - Convert RTC timestamp to formatted string
 * @pRtc: Pointer to RTC timestamp structure
 * @pStr: Output buffer for formatted date-time string (YYYY-MM-DD HH:MM:SS)
 *
 * Converts an RTC timestamp structure to a human-readable date-time string.
 */
void u_time_rtc2str(T_RTC_TIMESTAMP * pRtc, char * pStr)
{
	sprintf(pStr, "%04d-%02d-%02d %02d:%02d:%02d", pRtc->u16year, pRtc->u8mon, pRtc->u8day, pRtc->u8hrs, pRtc->u8min, pRtc->u8sec);
}