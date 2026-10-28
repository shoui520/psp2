#include "../compiler.h"
#include "datetime.h"
#include <rtc/rtc.h>

static PafWString date_format;
static PafWString time_format;
static PafWString datetime_format;

static const uint16_t *const day_names[] = {
	L"Sunday", L"Monday", L"Tuesday", L"Wednesday",
	L"Thursday", L"Friday", L"Saturday"
};

static const uint16_t *const abbreviated_day_names[] = {
	L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
};

extern int ScePafStdc_C95732BB(void *object, void (*destroy)(void *), void *dso);
extern void *__dso_handle;

void DateTimeSetDateFormat(const uint16_t *format)
{
	PafWStringAssign(&date_format, format, format ? sce_paf_wcslen(format) : 0);
}

PafWString *DateTimeGetDateFormat(void)
{
	return &date_format;
}

void DateTimeSetTimeFormat(const uint16_t *format)
{
	PafWStringAssign(&time_format, format, format ? sce_paf_wcslen(format) : 0);
}

PafWString *DateTimeGetTimeFormat(void)
{
	return &time_format;
}

void DateTimeSetDateTimeFormat(const uint16_t *format)
{
	PafWStringAssign(&datetime_format, format, format ? sce_paf_wcslen(format) : 0);
}

void DateTimeFormatsInitialize(void)
{
	unsigned char allocator[3];

	PafWStringConstruct(&date_format, &allocator[0]);
	ScePafStdc_C95732BB(&date_format, (void (*)(void *))PafWStringDestruct,
			  &__dso_handle);
	PafWStringConstruct(&time_format, &allocator[1]);
	ScePafStdc_C95732BB(&time_format, (void (*)(void *))PafWStringDestruct,
			  &__dso_handle);
	PafWStringConstruct(&datetime_format, &allocator[2]);
	ScePafStdc_C95732BB(&datetime_format, (void (*)(void *))PafWStringDestruct,
			  &__dso_handle);
}

int32_t DateTimeDayOfWeek(uint16_t year, uint16_t month, uint16_t day)
{
	if (month == 1 || month == 2) {
		year--;
		month += 12;
	}
	return (year + year / 4 - year / 100 + year / 400 +
		(13 * month + 8) / 5 + day) % 7;
}

size_t DateTimeWcsftime(DateTime *self, uint16_t *buf, int32_t maxLen,
		      uint16_t *format, int32_t flags)
{
	uint16_t year = self->m_year, month = self->m_month, day = self->m_day;
	uint16_t hour = self->m_hour, minute = self->m_minute, second = self->m_second;
	uint8_t adjust = flags;
	size_t count = 0, capacity = (uint32_t)maxLen;
	int escaped = 0, hour12, shortYear;
	uint16_t ch;

	if (adjust) {
		RtcDateTime time = {
			.year = year, .month = month, .day = day,
			.hour = hour, .minute = minute, .second = second,
			.microsecond = self->m_microsecond
		};
		RtcTick tick, adjusted;

		RtcConvertDateTimeToTick(&time, &tick);
		RtcTickAddMinutes(&adjusted, &tick, self->m_tz_offset);
		RtcConvertTickToDateTime(&adjusted, &time, NULL);
		year = time.year;
		month = time.month;
		day = time.day;
		hour = time.hour;
		minute = time.minute;
		second = time.second;
	}
	hour12 = (hour + 11) % 12 + 1;
	shortYear = year % 100;
	ch = *format;
	while (ch && count + 1 < capacity) {
		if (escaped) {
			switch (ch) {
			case '%':
				buf[count++] = '%';
				break;
			case 'a':
				count += sce_paf_swprintf(buf + count, capacity - count,
							  L"%s", abbreviated_day_names[
								  DateTimeDayOfWeek(year, month, day)]);
				break;
			case 'A':
				count += sce_paf_swprintf(buf + count, capacity - count,
							  L"%s", day_names[
								  DateTimeDayOfWeek(year, month, day)]);
				break;
			case 'c':
				count += DateTimeWcsftime(self, buf + count, capacity - count,
							 datetime_format.ptr, adjust);
				break;
			case 'd':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"--");
				else
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"%d", day);
				break;
			case 'H':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"--");
				else
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"%d", hour);
				break;
			case 'I':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"--");
				else
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"%d", hour12);
				break;
			case 'm':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"--");
				else
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"%d", month);
				break;
			case 'M':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"--");
				else
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"%02d", minute);
				break;
			case 'p':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"--");
				else
					count += sce_paf_swprintf(buf + count, capacity - count,
								  L"%ls", hour < 12 ? L"AM" : L"PM");
				break;
			case 'S':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"--");
				else
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"%02d", second);
				break;
			case 'x':
				count += DateTimeWcsftime(self, buf + count, capacity - count,
							 date_format.ptr, adjust);
				break;
			case 'X':
				count += DateTimeWcsftime(self, buf + count, capacity - count,
							 time_format.ptr, adjust);
				break;
			case 'y':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"--");
				else
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"%02d", shortYear);
				break;
			case 'Y':
				if (self->m_parse_failed == 1)
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"----");
				else
					count += sce_paf_swprintf(buf + count,
								  capacity - count, L"%d", year);
				break;
			default:
				break;
			}
			escaped = 0;
		} else if (ch == '%') {
			escaped = 1;
		} else {
			buf[count++] = ch;
		}
		ch = *++format;
	}
	if (capacity <= count)
		buf[count - 1] = 0;
	else
		buf[count] = 0;
	return count;
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

void DateTimeParseRFC3339(DateTime *result, const char *pszDateTime)
{
	RtcTick tick;
	RtcDateTime dst;
	uint16_t year, month, day, hour, minute, second;
	uint32_t microsecond, parseResult;
	int32_t timezone = 0;
	int ret = RtcParseRFC3339(&tick, pszDateTime);

	if (ret) {
		result->m_parse_failed = 1;
		result->m_year = 2011;
		result->m_month = 1;
		result->m_day = 1;
		result->m_hour = 0;
		result->m_minute = 0;
		result->m_second = 0;
		result->m_microsecond = 0;
		result->m_tz_offset = 0;
		result->m_parse_result = ret;
		return;
	}
	RtcConvertTickToDateTime(&tick, &dst, NULL);
	year = dst.year;
	month = dst.month;
	day = dst.day;
	hour = dst.hour;
	minute = dst.minute;
	second = dst.second;
	microsecond = dst.microsecond;
	parseResult = month;
	if (pszDateTime) {
		uint64_t state = DateTimeStringLengthState(pszDateTime);
		int32_t length = (uint32_t)state;
		const char *zone, *end;
		int sign, hours, minutes, h0, h1, m0, m1;

		/* The original stores scratch state as well as decoded offsets. */
		parseResult = state >> 32;
		if (length < 19)
			goto finish;
		parseResult = length - 19;
		if (length == 19)
			goto finish;
		end = pszDateTime + length;
		zone = pszDateTime + 19;
		if ((int8_t)*zone == '.') {
			zone++;
			if (zone == end)
				goto finish;
			for (;;) {
				parseResult = (int32_t)(int8_t)*zone;
				if ((int32_t)parseResult < '0' || (int32_t)parseResult > '9')
					break;
				if (++zone == end)
					goto finish;
			}
		}
		sign = (int8_t)*zone;
		parseResult = (uintptr_t)(zone + 1);
		if (sign == 'Z' || sign == 'z') {
			parseResult = 0;
			timezone = 0;
			goto finish;
		}
		if ((sign != '+' && sign != '-') || end - (zone + 1) < 5)
			goto finish;
		h0 = (int8_t)zone[1];
		h1 = (int8_t)zone[2];
		m0 = (int8_t)zone[4];
		m1 = (int8_t)zone[5];
		hours = h0 >= '0' && h0 <= '9' && h1 >= '0' && h1 <= '9' ?
			10 * h0 + h1 - 528 : -1;
		minutes = m0 >= '0' && m0 <= '9' && m1 >= '0' && m1 <= '9' ?
			10 * m0 + m1 - 528 : -1;
		parseResult = minutes;
		if (hours < 0 || (int8_t)zone[3] != ':' || minutes < 0)
			goto finish;
		timezone = 60 * hours + minutes;
		if (sign == '-')
			timezone = -timezone;
		parseResult = timezone;
	}
finish:
	result->m_year = year;
	result->m_month = month;
	result->m_day = day;
	result->m_hour = hour;
	result->m_minute = minute;
	result->m_second = second;
	result->m_microsecond = microsecond;
	result->m_tz_offset = timezone;
	result->m_parse_failed = 0;
	result->m_parse_result = parseResult;
}

/* The entry adapter supplies the caller's unchanged r5 as the third argument. */
void DateTimeParseSQLiteDateTimeBody(DateTime *result, const char *pszDateTime,
				   uint32_t incomingParseResult)
{
	RtcTick tick;
	RtcDateTime dst;

	if (RtcParseSQLiteDateTime(&tick, pszDateTime)) {
		result->m_hour = 0;
		result->m_parse_failed = 1;
		result->m_year = 2011;
		result->m_month = 1;
		result->m_day = 1;
		result->m_minute = 0;
		result->m_second = 0;
		result->m_microsecond = 0;
		result->m_tz_offset = 0;
	} else {
		RtcConvertTickToDateTime(&tick, &dst, NULL);
		result->m_year = dst.year;
		result->m_month = dst.month;
		result->m_day = dst.day;
		result->m_hour = dst.hour;
		result->m_minute = dst.minute;
		result->m_second = dst.second;
		result->m_microsecond = dst.microsecond;
		result->m_tz_offset = 0;
		result->m_parse_failed = 0;
	}
	result->m_parse_result = incomingParseResult;
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
