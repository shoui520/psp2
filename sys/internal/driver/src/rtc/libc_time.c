/*
 * SceRtc: the C library time functions processmgr hands to processes.
 */

#include "rtc_internal.h"

/* names unknown: clock(), time(), gettimeofday(), gmtime_r(), localtime_r(), mktime() */
static SceUInt32 rtcLibcClock(void)
{
	SceRtcTick tick;

	SceRtcForDriver_DEC408D4(&tick);
	return tick.tick;
}

static int rtcLibcMktime(ProcessTm *tm, SceKernelTime *t, ProcessTimezone *tz)
{
	SceDateTime time;
	SceRtcTick tick;
	unsigned int sec, min, hour, mon, year;
	int rem, mday, offset;

	if (tm == NULL || t == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	time.microsecond = 0;
	sec = tm->sec;
	if (sec < 60) {
		min = tm->min;
		time.second = sec;
		if (min < 60)
			goto minutes;
	} else {
		min = (int)sec / 60 + tm->min;
		rem = (int)sec % 60;
		if (rem < 0) {
			min--;
			rem += 60;
		}
		tm->min = min;
		time.second = rem;
		if (min < 60) {
minutes:
			hour = tm->hour;
			time.minute = min;
			goto hours;
		}
	}
	hour = (int)min / 60 + tm->hour;
	rem = (int)min % 60;
	if (rem < 0) {
		rem += 60;
		hour--;
	}
	tm->hour = hour;
	time.minute = rem;
hours:
	if (hour > 23) {
		mday = (int)hour / 24 + tm->mday;
		hour = (int)hour % 24;
		if ((int)hour < 0) {
			hour += 24;
			mday--;
		}
		tm->mday = mday;
	}
	mon = tm->mon;
	time.hour = hour;
	if (mon < 12) {
		year = tm->year;
	} else {
		year = tm->year + (int)mon / 12;
		tm->year = year;
		mon = (int)mon % 12;
		if ((int)mon < 0) {
			mon += 12;
			tm->mon = mon;
			tm->mday--;
		} else {
			tm->mon = mon;
		}
	}
	if (year - 1 >= 9999)
		return SCE_RTC_ERROR_INVALID_VALUE;
	if (tm->mday < 1) {
		for (;;) {
			if ((int)mon < 1) {
				year--;
				tm->mon = 11;
				tm->year = year;
				if ((int)year < 1)
					return SCE_RTC_ERROR_INVALID_VALUE;
				mon = 12;
			} else {
				tm->mon = mon - 1;
			}
			mday = tm->mday + SceRtcForDriver_C2DC38D9(year, mon);
			tm->mday = mday;
			if (mday > 0)
				break;
			mon = tm->mon;
			year = tm->year;
		}
	} else {
		while (SceRtcForDriver_C2DC38D9(year, mon + 1) < tm->mday) {
			for (;;) {
				mday = tm->mday - SceRtcForDriver_C2DC38D9(tm->year, tm->mon + 1);
				mon = tm->mon;
				tm->mday = mday;
				if ((int)mon > 10)
					break;
				tm->mon = mon + 1;
				if (mday <= SceRtcForDriver_C2DC38D9(tm->year, mon + 2))
					goto found;
			}
			tm->mon = 0;
			year = tm->year + 1;
			tm->year = year;
			if ((int)year > 9999)
				return SCE_RTC_ERROR_INVALID_VALUE;
			mon = 0;
		}
	}
found:
	time.year = tm->year;
	time.month = tm->mon + 1;
	time.day = tm->mday;
	SceRtcForDriver_F139E359(&time, &tick);
	SceRtcForDriver_42A0DFCB(&time, &tick);
	tm->mon = time.month - 1;
	tm->year = time.year;
	tm->hour = time.hour;
	tm->sec = time.second;
	tm->min = time.minute;
	tm->mday = time.day;
	tm->wday = SceRtcForDriver_0F800878(time.year, time.month, time.day);
	tm->yday = SceRtcForDriver_9097EF0D(time.year, time.month, time.day);
	offset = g_rtc.daylightSaving + g_rtc.timezone;
	tm->isdst = g_rtc.daylightSaving != 0;
	offset = -offset;
	SceRtcForDriver_0F9E02EF(&tick, &tick, offset);
	*t = __aeabi_ldivmod((SceInt64)tick.tick - RTC_TICK_UNIX_EPOCH, 1000000);
	if (tz != NULL) {
		tz->minuteswest = offset;
		tz->dsttime = 0;
	}
	return 0;
}

static inline void rtcFillTm(ProcessTm *tm, const SceDateTime *time)
{
	tm->mon = time->month - 1;
	tm->year = time->year;
	tm->hour = time->hour;
	tm->sec = time->second;
	tm->min = time->minute;
	tm->mday = time->day;
	tm->wday = SceRtcForDriver_0F800878(time->year, time->month, time->day);
	tm->yday = SceRtcForDriver_9097EF0D(time->year, time->month, time->day);
	tm->isdst = g_rtc.daylightSaving != 0;
}

static int rtcLibcLocaltime(const SceKernelTime *t, ProcessTm *tm)
{
	int offset = g_rtc.timezone + g_rtc.daylightSaving;
	SceDateTime time;
	SceRtcTick tick;

	if (t == NULL) {
		SceRtcForDriver_DEC408D4(&tick);
	} else {
		SceRtcForDriver_DE9F4515(&time, *t);
		SceRtcForDriver_F139E359(&time, &tick);
	}
	SceRtcForDriver_0F9E02EF(&tick, &tick, offset);
	SceRtcForDriver_42A0DFCB(&time, &tick);
	if (tm != NULL)
		rtcFillTm(tm, &time);
	return 0;
}

static int rtcLibcGettimeofday(ProcessTimeval *tv, ProcessTimezone *tz)
{
	SceDateTime time;
	SceRtcTick tick;
	unsigned int sec;

	SceRtcForDriver_DEC408D4(&tick);
	SceRtcForDriver_42A0DFCB(&time, &tick);
	SceRtcForDriver_FFD7A04F(&time, &sec);
	if (tv != NULL) {
		tv->sec = sec;
		tv->usec = time.microsecond;
	}
	if (tz != NULL) {
		tz->minuteswest = 0;
		tz->dsttime = 0;
	}
	return 0;
}

static SceKernelTime rtcLibcTime(SceKernelTime *t)
{
	SceDateTime time;
	SceRtcTick tick;
	unsigned int sec;

	SceRtcForDriver_DEC408D4(&tick);
	SceRtcForDriver_42A0DFCB(&time, &tick);
	SceRtcForDriver_FFD7A04F(&time, &sec);
	if (t != NULL)
		*t = sec;
	return sec;
}

static int rtcLibcGmtime(const SceKernelTime *t, ProcessTm *tm)
{
	SceDateTime time;
	SceRtcTick tick;

	if (t == NULL) {
		SceRtcForDriver_DEC408D4(&tick);
	} else {
		SceRtcForDriver_DE9F4515(&time, *t);
		SceRtcForDriver_F139E359(&time, &tick);
	}
	SceRtcForDriver_0F9E02EF(&tick, &tick, 0);
	SceRtcForDriver_42A0DFCB(&time, &tick);
	if (tm != NULL)
		rtcFillTm(tm, &time);
	return 0;
}

static const ProcessmgrLibcOps s_rtcLibcTime = {
	.size = sizeof(ProcessmgrLibcOps),
	.clock = rtcLibcClock,
	.time = rtcLibcTime,
	.gettimeofday = rtcLibcGettimeofday,
	.gmtime_r = rtcLibcGmtime,
	.localtime_r = rtcLibcLocaltime,
	.mktime = rtcLibcMktime,
	.getCurrentTick = SceRtcForDriver_DEC408D4,
	.getCurrentSecureTick = SceRtcForDriver_401C0954,
};

/* name unknown */
void rtcRegisterLibcTime(void)
{
	SceProcessmgrForDriver_4140D633(&s_rtcLibcTime);
}
