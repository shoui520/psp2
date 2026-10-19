/*
 * SceRtc's ticks: microseconds since 0001-01-01 00:00:00, and the ticks where the other
 * encodings begin (rtc_calendar.h converts between them).
 */

#pragma once

#define RTC_TICKS_PER_DAY	86400000000ULL
#define RTC_TICK_UNIX_EPOCH	62135596800000000ULL	/* 1970-01-01 */
#define RTC_TICK_FILETIME_EPOCH	50491123200000000ULL	/* 1601-01-01 */
#define RTC_TICK_TIME32_MAX	64283080447999999ULL	/* 2038-01-19 03:14:07.999999 */
