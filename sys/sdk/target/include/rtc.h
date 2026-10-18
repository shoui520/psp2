/*
 * rtc.h: the real-time clock's error codes and records, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/rtc.h). The kernel takes these; the user
 * library's functions come with the user-mode SDK headers.
 */
#ifndef _SCE_RTC_RTC_H
#define _SCE_RTC_RTC_H

#include <scetypes.h>

#if !defined(_LANGUAGE_ASSEMBLY)

/* error codes */
#define SCE_RTC_ERROR_INVALID_VALUE		-2145054720	/* 0x80251000 */
#define SCE_RTC_ERROR_INVALID_POINTER		-2145054719	/* 0x80251001 */
#define SCE_RTC_ERROR_NOT_INITIALIZED		-2145054718	/* 0x80251002 */
#define SCE_RTC_ERROR_ALREADY_REGISTERD		-2145054717	/* 0x80251003 */
#define SCE_RTC_ERROR_NOT_FOUND			-2145054716	/* 0x80251004 */
#define SCE_RTC_ERROR_BAD_PARSE			-2145054592	/* 0x80251080 */
#define SCE_RTC_ERROR_INVALID_YEAR		-2145054591	/* 0x80251081 */
#define SCE_RTC_ERROR_INVALID_MONTH		-2145054590	/* 0x80251082 */
#define SCE_RTC_ERROR_INVALID_DAY		-2145054589	/* 0x80251083 */
#define SCE_RTC_ERROR_INVALID_HOUR		-2145054588	/* 0x80251084 */
#define SCE_RTC_ERROR_INVALID_MINUTE		-2145054587	/* 0x80251085 */
#define SCE_RTC_ERROR_INVALID_SECOND		-2145054586	/* 0x80251086 */
#define SCE_RTC_ERROR_INVALID_MICROSECOND	-2145054585	/* 0x80251087 */

/* a point in time: microseconds since 0001-01-01 00:00:00 */
typedef struct SceRtcTick {
	SceUInt64 tick;
} SceRtcTick;

/* sceRtcGetDayOfWeek's result */
#define SCE_RTC_DAYOFWEEK_SUNDAY	0
#define SCE_RTC_DAYOFWEEK_MONDAY	1
#define SCE_RTC_DAYOFWEEK_TUESDAY	2
#define SCE_RTC_DAYOFWEEK_WEDNESDAY	3
#define SCE_RTC_DAYOFWEEK_THURSDAY	4
#define SCE_RTC_DAYOFWEEK_FRIDAY	5
#define SCE_RTC_DAYOFWEEK_SATURDAY	6

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_RTC_RTC_H */
