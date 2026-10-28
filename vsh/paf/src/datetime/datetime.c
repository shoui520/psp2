#include "../compiler.h"
#include "datetime.h"
#include <rtc/rtc.h>

int32_t DateTimeDayOfWeek(uint16_t year, uint16_t month, uint16_t day)
{
	if (month == 1 || month == 2) {
		year--;
		month += 12;
	}
	return (year + year / 4 - year / 100 + year / 400 +
		(13 * month + 8) / 5 + day) % 7;
}

DateTime *DateTimeConstruct(DateTime *self, uint16_t year, uint16_t month,
			    uint16_t day, uint16_t hour, uint16_t minute,
			    uint16_t second, uint32_t microsecond)
{
	self->m_year = year;
	self->m_month = month;
	self->m_day = day;
	self->m_hour = hour;
	self->m_minute = minute;
	self->m_second = second;
	self->m_parse_failed = 0;
	self->m_tz_offset = 0;
	self->m_microsecond = microsecond;
	return self;
}

void DateTimeUTC(DateTime *result)
{
	SceDateTime time;

	sceRtcGetCurrentClock(&time, 0);
	DateTimeConstruct(result, time.year, time.month, time.day, time.hour,
			  time.minute, time.second, time.microsecond);
}

void DateTimeNow(DateTime *result)
{
	SceDateTime time;

	sceRtcGetCurrentClockLocalTime(&time);
	DateTimeConstruct(result, time.year, time.month, time.day, time.hour,
			  time.minute, time.second, time.microsecond);
}

void DateTimeNetworkUTC(DateTime *result)
{
	SceRtcTick tick;
	SceDateTime time;

	sceRtcGetCurrentNetworkTick(&tick);
	sceRtcSetTick(&time, &tick);
	DateTimeConstruct(result, time.year, time.month, time.day, time.hour,
			  time.minute, time.second, time.microsecond);
}

void DateTimeConvertRtcTickToDateTime(DateTime *result, RtcTick *tick)
{
	RtcDateTime dst;

	RtcConvertTickToDateTime(tick, &dst, NULL);
	DateTimeConstruct(result, dst.year, dst.month, dst.day, dst.hour,
			  dst.minute, dst.second, dst.microsecond);
}

RtcTick DateTimeConvertDateTimeToRtcTick(DateTime *dt)
{
	RtcDateTime src = {
		.year = dt->m_year,
		.month = dt->m_month,
		.day = dt->m_day,
		.hour = dt->m_hour,
		.minute = dt->m_minute,
		.second = dt->m_second,
		.microsecond = dt->m_microsecond
	};
	RtcTick tick;

	RtcConvertDateTimeToTick(&src, &tick);
	return tick;
}

DateTime *DateTimeDestroy(DateTime *self)
{
	return self;
}

void DateTimeAdd(DateTime *result, const DateTime *self, const DateTime *r)
{
	SceDateTime time = {
		.year = self->m_year,
		.month = self->m_month,
		.day = self->m_day,
		.hour = self->m_hour,
		.minute = self->m_minute,
		.second = self->m_second,
		.microsecond = self->m_microsecond
	};
	uint16_t year = r->m_year, month = r->m_month, day = r->m_day;
	uint16_t hour = r->m_hour, minute = r->m_minute, second = r->m_second;
	uint32_t microsecond = r->m_microsecond;
	SceRtcTick tick;
	SceDateTime dst;

	sceRtcGetTick(&time, &tick);
	if (year)
		sceRtcTickAddYears(&tick, &tick, year);
	if (month)
		sceRtcTickAddMonths(&tick, &tick, month);
	if (day)
		sceRtcTickAddDays(&tick, &tick, day);
	if (hour)
		sceRtcTickAddHours(&tick, &tick, hour);
	if (minute)
		sceRtcTickAddMinutes(&tick, &tick, minute);
	if (second)
		sceRtcTickAddSeconds(&tick, &tick, second);
	if (microsecond)
		sceRtcTickAddMicroseconds(&tick, &tick, (uint64_t)microsecond);
	sceRtcSetTick(&dst, &tick);
	DateTimeConstruct(result, dst.year, dst.month, dst.day, dst.hour,
			  dst.minute, dst.second, dst.microsecond);
}

int32_t DateTimeLess(const DateTime *self, const DateTime *r)
{
	SceDateTime left = {
		.year = self->m_year,
		.month = self->m_month,
		.day = self->m_day,
		.hour = self->m_hour,
		.minute = self->m_minute,
		.second = self->m_second,
		.microsecond = self->m_microsecond
	};
	SceDateTime right = {
		.year = r->m_year,
		.month = r->m_month,
		.day = r->m_day,
		.hour = r->m_hour,
		.minute = r->m_minute,
		.second = r->m_second,
		.microsecond = r->m_microsecond
	};
	SceRtcTick leftTick, rightTick;

	sceRtcGetTick(&left, &leftTick);
	sceRtcGetTick(&right, &rightTick);
	return leftTick.tick < rightTick.tick;
}

DateTime *DateTimeConstructBase(DateTime *self, uint16_t year, uint16_t month,
			    uint16_t day, uint16_t hour, uint16_t minute,
			    uint16_t second, uint32_t microsecond)
{
	self->m_year = year;
	self->m_month = month;
	self->m_day = day;
	self->m_hour = hour;
	self->m_minute = minute;
	self->m_second = second;
	self->m_parse_failed = 0;
	self->m_tz_offset = 0;
	self->m_microsecond = microsecond;
	return self;
}

DateTime *DateTimeDestroyBase(DateTime *self)
{
	return self;
}
