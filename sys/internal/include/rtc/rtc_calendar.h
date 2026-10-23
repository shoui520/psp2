/*
 * SceRtc calendar arithmetic: proleptic Gregorian dates, ticks and the DOS,
 * Windows FILETIME and Unix time encodings. Sony built the same source into
 * the kernel (os0:kd/rtc.skprx) and the user library (SceRtcUser in
 * os0:us/driver_us.suprx).
 *
 * This header holds function definitions: include it from exactly one C file
 * of a module. The functions carry the user library's names; a module whose
 * exports are named otherwise maps them with #define before including (the
 * kernel exports them by NID). The includer declares __aeabi_uldivmod().
 */

#pragma once

#include <rtc.h>
#include <rtc/rtc_tick.h>

static const unsigned char s_rtcMonthDays[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
};

static inline int rtcIsLeap(int year)
{
	return year % 400 == 0 || (year % 100 != 0 && (year & 3) == 0);
}

unsigned int sceRtcGetTickResolution(void)
{
	return 1000000;
}

int sceRtcIsLeapYear(int year)
{
	if (year <= 0)
		return SCE_RTC_ERROR_INVALID_YEAR;
	return rtcIsLeap(year);
}

int sceRtcGetDaysInMonth(int year, int month)
{
	if (year <= 0)
		return SCE_RTC_ERROR_INVALID_YEAR;
	if (month < 1 || month > 12)
		return SCE_RTC_ERROR_INVALID_MONTH;
	if (rtcIsLeap(year))
		return s_rtcMonthDays[1][month - 1];
	return s_rtcMonthDays[0][month - 1];
}

int sceRtcGetDayOfWeek(int year, int month, int day)
{
	int days = sceRtcGetDaysInMonth(year, month);

	if (days < 0)
		return days;
	if (day <= 0 || day > days)
		return SCE_RTC_ERROR_INVALID_DAY;
	if (month <= 2) {
		month += 12;
		year--;
	}
	return (year + year / 4 - year / 100 + year / 400 + (13 * month + 8) / 5 + day) % 7;
}

int sceRtcGetDayOfYear(int year, int month, int day)
{
	int days = sceRtcGetDaysInMonth(year, month);
	int i;

	if (days < 0)
		return days;
	if (day <= 0 || day > days)
		return SCE_RTC_ERROR_INVALID_DAY;
	days = 0;
	for (i = 1; i < month; i++)
		days += rtcIsLeap(year) ? s_rtcMonthDays[1][i - 1] : s_rtcMonthDays[0][i - 1];
	return day - 1 + days;
}

int sceRtcCheckValid(const SceDateTime *time)
{
	int days;

	if (time == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	if ((unsigned short)(time->year - 1) > 9998)
		return SCE_RTC_ERROR_INVALID_YEAR;
	if ((unsigned int)(time->month - 1) > 11)
		return SCE_RTC_ERROR_INVALID_MONTH;
	days = sceRtcGetDaysInMonth(time->year, time->month);
	if (time->day == 0 || time->day > days)
		return SCE_RTC_ERROR_INVALID_DAY;
	if (time->hour > 23)
		return SCE_RTC_ERROR_INVALID_HOUR;
	if (time->minute > 59)
		return SCE_RTC_ERROR_INVALID_MINUTE;
	if (time->second > 59)
		return SCE_RTC_ERROR_INVALID_SECOND;
	if (time->microsecond > 999999)
		return SCE_RTC_ERROR_INVALID_MICROSECOND;
	return 0;
}

int sceRtcSetDosTime(SceDateTime *time, unsigned int dosTime)
{
	unsigned int date = dosTime >> 16;
	unsigned int clock = dosTime & 0xffff;

	if (time == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	time->year = (date >> 9) + 1980;
	time->month = (date >> 5) & 0xf;
	time->day = date & 0x1f;
	time->hour = clock >> 11;
	time->minute = (clock >> 5) & 0x3f;
	time->second = (clock << 1) & 0x3e;
	time->microsecond = 0;
	return 0;
}

int sceRtcGetDosTime(const SceDateTime *time, unsigned int *dosTime)
{
	int ret;

	if (time == NULL || dosTime == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	ret = sceRtcCheckValid(time);
	if (ret < 0)
		return ret;
	if (time->year < 1980) {
		*dosTime = 0;
		return SCE_RTC_ERROR_INVALID_YEAR;
	}
	if (time->year > 2107) {
		*dosTime = 0xff9fbf7d;
		return SCE_RTC_ERROR_INVALID_YEAR;
	}
	*dosTime = ((time->year - 1980) << 9 | time->month << 5 | (time->day & 0x1f)) << 16 |
		   (unsigned short)(time->hour << 11 | time->minute << 5 | ((time->second >> 1) & 0x1f));
	return 0;
}

/* tick to calendar fields */
static inline void rtcTickToDateTime(SceDateTime *time, SceUInt64 tick)
{
	SceUInt64 quotient;
	unsigned int days, seconds, length, month;
	int year, leap;

	quotient = __aeabi_uldivmod(tick, RTC_TICKS_PER_DAY);
	days = quotient;
	tick -= quotient * RTC_TICKS_PER_DAY;
	seconds = __aeabi_uldivmod(tick, 1000000);
	time->microsecond = tick - (SceUInt64)seconds * 1000000;
	time->second = seconds % 60;
	time->minute = seconds / 60 % 60;
	time->hour = seconds / 3600 % 24;
	year = days / 146097 * 400 + 1;
	days %= 146097;
	if (days > 36523) {
		year += days / 36524 * 100;
		days %= 36524;
		if (days == 0) {
			year--;
			days = 365;
		}
	}
	if (days > 1460) {
		year += days / 1461 * 4;
		days %= 1461;
	}
	for (;;) {
		length = sceRtcIsLeapYear(year) ? 366 : 365;
		if (days < length)
			break;
		days -= length;
		year++;
	}
	time->year = year;
	leap = sceRtcIsLeapYear(year);
	for (month = 0; month < 11; month++) {
		if (days < s_rtcMonthDays[leap][month])
			break;
		days -= s_rtcMonthDays[leap][month];
	}
	time->month = month + 1;
	time->day = days + 1;
}

int sceRtcSetWin32FileTime(SceDateTime *time, SceUInt64 fileTime)
{
	if (time == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	rtcTickToDateTime(time, RTC_TICK_FILETIME_EPOCH + __aeabi_uldivmod(fileTime, 10));
	return 0;
}

/*
 * Calendar fields to tick, unchecked. Year 0 makes the leap index Sony's
 * error code, and the month table read then goes wherever that points.
 */
static inline SceUInt64 rtcDateTimeToTick(const SceDateTime *time)
{
	int year = time->year;
	int leap = sceRtcIsLeapYear(year);
	int months = time->month - 1;
	SceInt64 days;
	int i;

	days = 365 * year + (year + 3) / 4 - (year + 99) / 100 + (year + 399) / 400 - 366;
	for (i = 0; i < months; i++)
		days += s_rtcMonthDays[leap][i];
	days += time->day - 1;
	return (((days * 24 + time->hour) * 60 + time->minute) * 60 + time->second) * 1000000 +
	       time->microsecond;
}

int sceRtcGetWin32FileTime(const SceDateTime *time, SceUInt64 *fileTime)
{
	SceUInt64 tick;
	int ret;

	if (time == NULL || fileTime == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	ret = sceRtcCheckValid(time);
	if (ret < 0)
		return ret;
	tick = rtcDateTimeToTick(time);
	if (tick < RTC_TICK_FILETIME_EPOCH) {
		*fileTime = 0;
		return time->year <= 1600 ? SCE_RTC_ERROR_INVALID_YEAR : SCE_RTC_ERROR_INVALID_VALUE;
	}
	*fileTime = (tick - RTC_TICK_FILETIME_EPOCH) * 10;
	return 0;
}

int sceRtcSetTime64_t(SceDateTime *time, SceUInt64 unixTime)
{
	if (time == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	rtcTickToDateTime(time, unixTime * 1000000 + RTC_TICK_UNIX_EPOCH);
	return 0;
}

int sceRtcSetTime_t(SceDateTime *time, unsigned int time32)
{
	if (time == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	rtcTickToDateTime(time, (SceUInt64)time32 * 1000000 + RTC_TICK_UNIX_EPOCH);
	return 0;
}

int sceRtcGetTime64_t(const SceDateTime *time, SceUInt64 *unixTime)
{
	SceUInt64 tick;
	int ret;

	if (time == NULL || unixTime == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	ret = sceRtcCheckValid(time);
	if (ret < 0)
		return ret;
	tick = rtcDateTimeToTick(time);
	if (tick < RTC_TICK_UNIX_EPOCH) {
		*unixTime = 0;
		return time->year <= 1969 ? SCE_RTC_ERROR_INVALID_YEAR : SCE_RTC_ERROR_INVALID_VALUE;
	}
	*unixTime = __aeabi_uldivmod(tick - RTC_TICK_UNIX_EPOCH, 1000000);
	return 0;
}

int sceRtcGetTime_t(const SceDateTime *time, unsigned int *time32)
{
	SceUInt64 tick;
	int ret;

	if (time == NULL || time32 == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	ret = sceRtcCheckValid(time);
	if (ret < 0)
		return ret;
	tick = rtcDateTimeToTick(time);
	if (tick < RTC_TICK_UNIX_EPOCH) {
		*time32 = 0;
		return time->year <= 1969 ? SCE_RTC_ERROR_INVALID_YEAR : SCE_RTC_ERROR_INVALID_VALUE;
	}
	if (tick > RTC_TICK_TIME32_MAX) {
		*time32 = 0x7fffffff;
		if (time->year > 2038)
			return SCE_RTC_ERROR_INVALID_YEAR;
		if (time->month > 1)
			return SCE_RTC_ERROR_INVALID_MONTH;
		if (time->day > 19)
			return SCE_RTC_ERROR_INVALID_DAY;
		if (time->hour > 3)
			return SCE_RTC_ERROR_INVALID_HOUR;
		if (time->minute > 14)
			return SCE_RTC_ERROR_INVALID_MINUTE;
		if (time->second > 7)
			return SCE_RTC_ERROR_INVALID_SECOND;
		return SCE_RTC_ERROR_INVALID_MICROSECOND;
	}
	*time32 = __aeabi_uldivmod(tick - RTC_TICK_UNIX_EPOCH, 1000000);
	return 0;
}

__attribute__((noinline)) int sceRtcSetTick(SceDateTime *time, const SceRtcTick *tick)
{
	if (time == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	rtcTickToDateTime(time, tick->tick);
	return 0;
}

/* name unknown: ticks of a date that is not validated (a leap second may be in it) */
int rtcGetTickUnchecked(const SceDateTime *time, SceRtcTick *tick)
{
	if (time == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	tick->tick = rtcDateTimeToTick(time);
	return 0;
}

int sceRtcGetTick(const SceDateTime *time, SceRtcTick *tick)
{
	int ret;

	if (time == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	ret = sceRtcCheckValid(time);
	if (ret < 0)
		return ret;
	tick->tick = rtcDateTimeToTick(time);
	return 0;
}

int sceRtcTickAddTicks(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n)
{
	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	res->tick = tick->tick + n;
	return 0;
}

int sceRtcTickAddMicroseconds(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n)
{
	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	res->tick = tick->tick + n;
	return 0;
}

int sceRtcTickAddSeconds(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n)
{
	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	res->tick = tick->tick + n * 1000000;
	return 0;
}

int sceRtcTickAddMinutes(SceRtcTick *res, const SceRtcTick *tick, SceInt64 n)
{
	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	res->tick = tick->tick + n * 60000000;
	return 0;
}

int sceRtcTickAddHours(SceRtcTick *res, const SceRtcTick *tick, int n)
{
	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	res->tick = tick->tick + (SceInt64)n * 3600000000LL;
	return 0;
}

int sceRtcTickAddDays(SceRtcTick *res, const SceRtcTick *tick, int n)
{
	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	res->tick = tick->tick + (SceInt64)n * 86400000000LL;
	return 0;
}

int sceRtcTickAddWeeks(SceRtcTick *res, const SceRtcTick *tick, int n)
{
	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	res->tick = tick->tick + (SceInt64)n * 604800000000LL;
	return 0;
}

int sceRtcTickAddMonths(SceRtcTick *res, const SceRtcTick *tick, int n)
{
	SceDateTime time;
	int months, days;

	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	sceRtcSetTick(&time, tick);
	months = time.year * 12 + time.month - 1 + n;
	time.year = months / 12;
	time.month = months % 12 + 1;
	days = sceRtcGetDaysInMonth(time.year, time.month);
	if (time.day > days)
		time.day = days;
	sceRtcGetTick(&time, res);
	return 0;
}

int sceRtcTickAddYears(SceRtcTick *res, const SceRtcTick *tick, int n)
{
	SceDateTime time;
	int days;

	if (res == NULL || tick == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	sceRtcSetTick(&time, tick);
	time.year += n;
	days = sceRtcGetDaysInMonth(time.year, time.month);
	if (time.day > days)
		time.day = days;
	sceRtcGetTick(&time, res);
	return 0;
}

int sceRtcCompareTick(const SceRtcTick *a, const SceRtcTick *b)
{
	if (a == NULL || b == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	if (a->tick < b->tick)
		return -1;
	return a->tick > b->tick;
}
