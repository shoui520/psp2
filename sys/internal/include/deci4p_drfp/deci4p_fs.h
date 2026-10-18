/*
 * File information as the DECI4P file protocols (SceDeci4pDrfp,
 * SceDeci4pRDrfp) exchange it with the host, and the host's times:
 * microseconds since 0001-01-01. Sony compiles these helpers into each
 * module.
 */

#pragma once

#include <scetypes.h>

/* host modes */
#define DECI4P_FS_MODE_IFREG		0x8000
#define DECI4P_FS_MODE_IFDIR		0x4000
#define DECI4P_FS_MODE_IRUSR		0x100
#define DECI4P_FS_MODE_IWUSR		0x80

typedef struct __attribute__((packed, aligned(4))) Deci4pFsStat {
	SceUInt32 mode;				/* DECI4P_FS_MODE_* */
	SceUInt32 dev;
	SceUInt64 fid;				/* 0: none */
	SceUInt64 size;
	SceUInt64 atime;
	SceUInt64 mtime;
	SceUInt64 ctime;
} Deci4pFsStat;

#define DECI4P_FS_USEC_PER_SEC		1000000
#define DECI4P_FS_USEC_PER_DAY		86400000000ULL
#define DECI4P_FS_DAYS_PER_400Y		146097
#define DECI4P_FS_DAYS_PER_100Y		36524
#define DECI4P_FS_DAYS_PER_4Y		1461

static const SceUInt8 deci4pFsMonthDays[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
};

static inline int deci4pFsIsLeapYear(int year)
{
	if (year % 400 == 0)
		return 1;
	if (year % 100 == 0)
		return 0;
	return (year & 3) == 0;
}

/* host time to SceDateTime */
static inline __attribute__((always_inline)) void deci4pFsUsecToDateTime(SceUInt64 usec,
									  SceDateTime *dt)
{
	SceUInt32 days, sec, month;
	SceUInt64 rest;
	int year, leap;

	days = usec / DECI4P_FS_USEC_PER_DAY;
	rest = usec - days * DECI4P_FS_USEC_PER_DAY;
	sec = rest / DECI4P_FS_USEC_PER_SEC;
	dt->microsecond = rest - sec * DECI4P_FS_USEC_PER_SEC;
	dt->second = sec % 60;
	dt->minute = sec / 60 % 60;
	dt->hour = sec / 60 / 60 % 24;

	if (days < DECI4P_FS_DAYS_PER_400Y) {
		year = 1;
	} else {
		year = days / DECI4P_FS_DAYS_PER_400Y * 400 + 1;
		days %= DECI4P_FS_DAYS_PER_400Y;
	}
	if (days >= DECI4P_FS_DAYS_PER_100Y) {
		year += days / DECI4P_FS_DAYS_PER_100Y * 100;
		days %= DECI4P_FS_DAYS_PER_100Y;
		if (days == 0) {
			year--;
			days = 365;
		}
	}
	if (days >= DECI4P_FS_DAYS_PER_4Y) {
		year += days / DECI4P_FS_DAYS_PER_4Y * 4;
		days %= DECI4P_FS_DAYS_PER_4Y;
	}
	for (;;) {
		leap = deci4pFsIsLeapYear(year);
		if (days < (SceUInt32)(leap ? 366 : 365))
			break;
		days -= leap ? 366 : 365;
		year++;
	}
	dt->year = year;
	for (month = 0; month < 11 && days >= deci4pFsMonthDays[leap][month]; month++)
		days -= deci4pFsMonthDays[leap][month];
	dt->month = month + 1;
	dt->day = days + 1;
}

/* SceDateTime to host time */
static inline __attribute__((always_inline)) SceUInt64 deci4pFsDateTimeToUsec(const SceDateTime *dt)
{
	int year = dt->year;
	int leap = deci4pFsIsLeapYear(year);
	SceInt64 days;
	int i;

	days = year * 365 + (year + 3) / 4 - (year + 99) / 100 + (year + 399) / 400 - 366;
	for (i = 0; i < dt->month - 1; i++)
		days += deci4pFsMonthDays[leap][i];
	days += dt->day - 1;
	return (((days * 24 + dt->hour) * 60 + dt->minute) * 60 + dt->second) *
	       DECI4P_FS_USEC_PER_SEC + dt->microsecond;
}
