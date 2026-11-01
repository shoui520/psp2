/*
 * paf::rtc: SceRtc's ticks and dates in paf's own types. SQLite dates
 * ("YYYY-MM-DD HH:MM:SS.SSS", any prefix of it) are parsed here.
 */
#include <rtc/rtc.h>
#include <rtc.h>
#include <std/string.h>

extern "C" int sceRegMgrUtilityGetInt(int id, int *value);

/* /CONFIG/DATE/summer_time and /CONFIG/DATE/time_format */
#define REGMGR_SUMMER_TIME	0x760538
#define REGMGR_TIME_FORMAT	0x668503

namespace paf {
namespace rtc {

/* what a tick that does not convert, and an SQLite date's missing parts, are */
static const DateTime s_default_date_time = { 2010, 1, 1, 0, 0, 0, 0 };

int32_t ConvertDateTimeToTick(const DateTime *src, Tick *tick)
{
	SceDateTime time;
	SceRtcTick value;
	int32_t ret;

	time.year = src->year;
	time.month = src->month;
	time.day = src->day;
	time.hour = src->hour;
	time.minute = src->minute;
	time.second = src->second;
	time.microsecond = src->microsecond;
	ret = sceRtcGetTick(&time, &value);
	if (tick)
		*tick = value.tick;
	return ret;
}

int32_t GetDaysInMonth(uint16_t year, uint16_t month)
{
	return sceRtcGetDaysInMonth(year, month);
}

int32_t IsLeapYear(uint16_t year)
{
	return sceRtcIsLeapYear(year);
}

int32_t ConvertTickToDateTime(const Tick *tick, DateTime *dst,
			       DateTimeEx *exdst)
{
	SceRtcTick value;
	SceDateTime time;
	int32_t ret;

	value.tick = *tick;
	ret = sceRtcSetTick(&time, &value);
	if (ret) {
		dst->year = s_default_date_time.year;
		dst->month = s_default_date_time.month;
		dst->day = s_default_date_time.day;
		dst->hour = s_default_date_time.hour;
		dst->minute = s_default_date_time.minute;
		dst->second = s_default_date_time.second;
		dst->microsecond = s_default_date_time.microsecond;
	} else {
		dst->year = time.year;
		dst->month = time.month;
		dst->day = time.day;
		dst->hour = time.hour;
		dst->minute = time.minute;
		dst->second = time.second;
		dst->microsecond = time.microsecond;
	}
	if (exdst && !ret) {
		int32_t month, adjusted_month, year, day, yearday;
		int32_t time_format_value, summer_time_value;
		uint16_t summer_time = 0, time_format = 0;

		sce_paf_memset(exdst, 0, sizeof(*exdst));
		exdst->weekday = sceRtcGetDayOfWeek(dst->year, dst->month, dst->day);
		month = dst->month;
		adjusted_month = month <= 2 ? month + 12 : month;
		year = dst->year;
		day = dst->day;
		yearday = day + (979 * adjusted_month - 1033) / 32 - 1;
		if (month >= 3) {
			int32_t leap = sceRtcIsLeapYear(year);

			if (leap >= 0)
				yearday += leap;
		}
		exdst->yearday = yearday;
		if (sceRegMgrUtilityGetInt(REGMGR_SUMMER_TIME, &summer_time_value) >= 0)
			summer_time = summer_time_value;
		exdst->summer_time = summer_time;
		if (sceRegMgrUtilityGetInt(REGMGR_TIME_FORMAT, &time_format_value) >= 0)
			time_format = time_format_value;
		exdst->time_format = time_format;
	}
	return ret;
}

int32_t GetCurrentTick(Tick *tick)
{
	SceRtcTick value;
	int32_t ret = sceRtcGetCurrentTick(&value);

	if (tick)
		*tick = value.tick;
	return ret;
}

int32_t ConvertUtcToLocalTime(const Tick *utc, Tick *local_time)
{
	SceRtcTick src, dst;
	int32_t ret;

	src.tick = *utc;
	ret = sceRtcConvertUtcToLocalTime(&src, &dst);
	if (local_time)
		*local_time = dst.tick;
	return ret;
}

int32_t GetCurrentLocalTick(Tick *tick)
{
	SceRtcTick value, utc, local_time;
	int32_t ret = sceRtcGetCurrentTick(&value);

	if (!ret) {
		utc.tick = value.tick;
		ret = sceRtcConvertUtcToLocalTime(&utc, &local_time);
		if (tick)
			*tick = local_time.tick;
	}
	return ret;
}

int32_t GetCurrentNetworkTick(Tick *tick)
{
	SceRtcTick value;
	int32_t ret = sceRtcGetCurrentNetworkTick(&value);

	if (tick && !ret)
		*tick = value.tick;
	return ret;
}

int32_t ConvertLocalTimeToUtc(const Tick *local_time, Tick *utc)
{
	SceRtcTick dst, src;
	int32_t ret;

	src.tick = *local_time;
	ret = sceRtcConvertLocalTimeToUtc(&src, &dst);
	if (utc)
		*utc = dst.tick;
	return ret;
}

int32_t FormatRFC3339(char *dt, const Tick *tick, int32_t timezone_minutes)
{
	SceRtcTick utc, current;
	Tick value;
	int32_t ret;

	if (tick) {
		value = *tick;
	} else {
		ret = sceRtcGetCurrentTick(&current);
		if (ret)
			return ret;
		value = current.tick;
	}
	utc.tick = value;
	return sceRtcFormatRFC3339(dt, &utc, timezone_minutes);
}

int32_t FormatRFC3339LocalTime(char *dt, const Tick *tick)
{
	SceRtcTick value, current, utc, local_time;
	int32_t ret;

	if (tick) {
		value.tick = *tick;
	} else {
		ret = sceRtcGetCurrentTick(&current);
		if (ret)
			return ret;
		utc.tick = current.tick;
		ret = sceRtcConvertUtcToLocalTime(&utc, &local_time);
		if (ret)
			return ret;
		value.tick = local_time.tick;
	}
	return sceRtcFormatRFC3339LocalTime(dt, &value);
}

int32_t FormatSQLiteDateTime(char *dt, size_t size, const Tick *tick, char separator)
{
	SceRtcTick current, value;
	SceDateTime time;
	char text[64];
	int32_t year, month, day, hour, minute, second, millisecond, ret;
	uint32_t microsecond;

	if (!dt)
		return SCE_RTC_ERROR_INVALID_VALUE;
	if (tick) {
		value.tick = *tick;
	} else {
		ret = sceRtcGetCurrentTick(&current);
		if (ret)
			return ret;
		value.tick = current.tick;
	}
	if (sceRtcSetTick(&time, &value)) {
		year = s_default_date_time.year;
		month = s_default_date_time.month;
		day = s_default_date_time.day;
		hour = s_default_date_time.hour;
		minute = s_default_date_time.minute;
		second = s_default_date_time.second;
		microsecond = s_default_date_time.microsecond;
	} else {
		year = time.year;
		month = time.month;
		day = time.day;
		hour = time.hour;
		minute = time.minute;
		second = time.second;
		microsecond = time.microsecond;
	}
	millisecond = microsecond / 1000;
	text[3] = year % 10 + '0';
	year /= 10;
	text[2] = year % 10 + '0';
	year /= 10;
	text[1] = year % 10 + '0';
	year /= 10;
	text[0] = year % 10 + '0';
	text[4] = '-';
	text[6] = month % 10 + '0';
	text[5] = month / 10 % 10 + '0';
	text[7] = '-';
	text[9] = day % 10 + '0';
	text[8] = day / 10 % 10 + '0';
	text[10] = separator;
	text[12] = hour % 10 + '0';
	text[11] = hour / 10 % 10 + '0';
	text[13] = ':';
	text[15] = minute % 10 + '0';
	text[14] = minute / 10 % 10 + '0';
	text[16] = ':';
	text[18] = second % 10 + '0';
	text[17] = second / 10 % 10 + '0';
	text[19] = '.';
	text[22] = millisecond % 10 + '0';
	millisecond /= 10;
	text[21] = millisecond % 10 + '0';
	millisecond /= 10;
	text[20] = millisecond % 10 + '0';
	text[23] = '\0';
	sce_paf_memset(dt, 0, size);
	sce_paf_strncpy(dt, text, size - 1);
	return 0;
}

int32_t ParseRFC3339(Tick *tick, const char *dt)
{
	SceRtcTick value;
	int32_t ret;

	if (!tick || !dt)
		return SCE_RTC_ERROR_INVALID_POINTER;
	ret = sceRtcParseRFC3339(&value, dt);
	*tick = value.tick;
	return ret;
}

int32_t ParseRFC3339LocalTime(Tick *tick, const char *dt)
{
	return 0;
}

static inline __attribute__((always_inline))
int IsSQLiteDigit(int ch)
{
	return ch >= '0' && ch <= '9';
}

/* Two digits, or -1. */
static inline __attribute__((always_inline))
int ReadSQLitePair(const char *dt)
{
	int first = (int8_t)dt[0], second = (int8_t)dt[1];

	if (!IsSQLiteDigit(first) || !IsSQLiteDigit(second))
		return -1;
	return 10 * first + second - 11 * '0';
}

/* Four digits, or -1. */
static inline __attribute__((always_inline))
int ReadSQLiteYear(const char *dt)
{
	int first = (int8_t)dt[0], second = (int8_t)dt[1];
	int third = (int8_t)dt[2], fourth = (int8_t)dt[3];

	if (!IsSQLiteDigit(first) || !IsSQLiteDigit(second) ||
	    !IsSQLiteDigit(third) || !IsSQLiteDigit(fourth))
		return -1;
	return 1000 * first + 100 * second + 10 * third + fourth - 1111 * '0';
}

static inline __attribute__((always_inline))
int32_t CheckSQLiteDateTime(uint16_t year, uint16_t month, uint16_t day, uint16_t hour,
			    uint16_t minute, uint16_t second, uint32_t microsecond)
{
	SceDateTime time;

	time.year = year;
	time.month = month;
	time.day = day;
	time.hour = hour;
	time.minute = minute;
	time.second = second;
	time.microsecond = microsecond;
	return sceRtcCheckValid(&time);
}

/* A date cut short where ftype allows it: checked as far as it was read. */
static inline __attribute__((always_inline))
int32_t CheckPartialSQLiteDateTime(const DateTime *dst, SQLiteDateTimeFormatType *ftype,
				   SQLiteDateTimeFormatType type)
{
	SceDateTime time;

	time.year = dst->year;
	*ftype = type;
	time.month = dst->month;
	time.day = dst->day;
	time.hour = dst->hour;
	time.minute = dst->minute;
	time.second = dst->second;
	time.microsecond = dst->microsecond;
	return sceRtcCheckValid(&time);
}

/*
 * Without ftype the date is needed whole; with it any prefix is taken, the
 * rest coming from the default date, and ftype says how much was read.
 */
static int32_t ParseSQLiteDateTimeCore(DateTime *dst, const char *dt,
				       SQLiteDateTimeFormatType *ftype)
{
	int32_t length, year, month, day, hour, minute, second;

	if (ftype != NULL)
		*ftype = INVALID;
	if (dst == NULL || dt == NULL)
		return SCE_RTC_ERROR_INVALID_VALUE;
	sce_paf_memset(dst, 0, sizeof(*dst));
	if (ftype != NULL)
		*dst = s_default_date_time;
	length = sce_paf_strlen(dt);
	if (length < 10 && ftype == NULL)
		return SCE_RTC_ERROR_INVALID_VALUE;

	const char *end = dt + length;

	year = ReadSQLiteYear(dt);
	if (year < 0)
		return SCE_RTC_ERROR_INVALID_YEAR;
	dst->year = year;
	if (length <= 4 && ftype != NULL)
		return CheckPartialSQLiteDateTime(dst, ftype, YEAR_ONLY);
	if (dt[4] != '-')
		return SCE_RTC_ERROR_BAD_PARSE;
	month = ReadSQLitePair(dt + 5);
	if (month < 0)
		return SCE_RTC_ERROR_INVALID_MONTH;
	dst->month = month;
	if (length <= 7 && ftype != NULL)
		return CheckPartialSQLiteDateTime(dst, ftype, YEAR_MONTH);
	if (dt[7] != '-')
		return SCE_RTC_ERROR_BAD_PARSE;
	day = ReadSQLitePair(dt + 8);
	if (day < 0)
		return SCE_RTC_ERROR_INVALID_DAY;
	dst->day = day;
	if (length <= 10) {
		hour = dst->hour;
		minute = dst->minute;
		second = dst->second;
		if (ftype != NULL)
			*ftype = DATE;
		return CheckSQLiteDateTime(year, month, day, hour, minute, second, dst->microsecond);
	}
	if (dt[10] != ' ' && dt[10] != 'T')
		return SCE_RTC_ERROR_BAD_PARSE;
	hour = ReadSQLitePair(dt + 11);
	if (hour < 0)
		return SCE_RTC_ERROR_INVALID_HOUR;
	dst->hour = hour;
	if (length <= 13 && ftype != NULL)
		return CheckPartialSQLiteDateTime(dst, ftype, DATE_HOUR);
	if (dt[13] != ':')
		return SCE_RTC_ERROR_BAD_PARSE;
	minute = ReadSQLitePair(dt + 14);
	if (minute < 0)
		return SCE_RTC_ERROR_INVALID_MINUTE;
	dst->minute = minute;
	if (length <= 16) {
		second = dst->second;
		if (ftype != NULL)
			*ftype = DATE_HOUR_MINUTE;
		return CheckSQLiteDateTime(year, month, day, hour, minute, second, dst->microsecond);
	}
	if (dt[16] != ':')
		return SCE_RTC_ERROR_BAD_PARSE;
	second = ReadSQLitePair(dt + 17);
	if (second < 0)
		return SCE_RTC_ERROR_INVALID_SECOND;
	dst->second = second;
	if (length <= 19) {
		if (ftype != NULL)
			*ftype = DATE_HOUR_MINUTE_SECOND;
		return CheckSQLiteDateTime(year, month, day, hour, minute, second, dst->microsecond);
	}

	uint32_t microsecond;

	if (dt[19] == '.') {
		const char *cursor = dt + 20;
		uint32_t weight = 100000;

		microsecond = 0;
		while (end - cursor > 0) {
			int ch = (int8_t)*cursor;

			if (!IsSQLiteDigit(ch))
				break;
			microsecond += (ch - '0') * weight;
			if (weight == 100000)
				weight = 10000;
			else if (weight == 10000)
				weight = 1000;
			else if (weight == 1000)
				weight = 100;
			else if (weight == 100)
				weight = 10;
			else
				weight = weight == 10;
			++cursor;
		}
		dst->microsecond = microsecond;
		if (ftype != NULL) {
			*ftype = DATE_HOUR_MINUTE_SECOND_MILLISECOND;
			microsecond = dst->microsecond;
		}
	} else {
		/* anything else after the seconds is ignored */
		microsecond = 0;
		dst->microsecond = 0;
	}
	return CheckSQLiteDateTime(year, month, day, hour, minute, second, microsecond);
}

int32_t ParseSQLiteDateTime(Tick *tick, const char *dt)
{
	SceRtcTick current, value;
	DateTime parsed;
	SceDateTime time;
	int32_t ret;

	if (!tick || !dt)
		return SCE_RTC_ERROR_INVALID_VALUE;
	if (sce_paf_strlen(dt) == 3 && !sce_paf_strncmp(dt, "now", 3)) {
		ret = sceRtcGetCurrentTick(&current);
		*tick = current.tick;
		return ret;
	}
	ret = ParseSQLiteDateTimeCore(&parsed, dt, NULL);
	if (!ret) {
		time.year = parsed.year;
		time.month = parsed.month;
		time.day = parsed.day;
		time.hour = parsed.hour;
		time.minute = parsed.minute;
		time.second = parsed.second;
		time.microsecond = parsed.microsecond;
		sceRtcGetTick(&time, &value);
		*tick = value.tick;
	}
	return ret;
}

int32_t ParseSQLiteDateTime(Tick *tick, SQLiteDateTimeFormatType *ftype,
				       const char *dt)
{
	SceRtcTick current, value;
	DateTime parsed, snapshot;
	SceDateTime time;
	SQLiteDateTimeFormatType type = INVALID;
	int32_t ret;

	if (!tick || !dt)
		return SCE_RTC_ERROR_INVALID_VALUE;
	if (sce_paf_strlen(dt) == 3 && !sce_paf_strncmp(dt, "now", 3)) {
		ret = sceRtcGetCurrentTick(&current);
		*tick = current.tick;
		return ret;
	}
	ret = ParseSQLiteDateTimeCore(&parsed, dt, &type);
	snapshot = parsed;
	if (ftype)
		*ftype = type;
	if (!ret) {
		time.year = snapshot.year;
		time.month = snapshot.month;
		time.day = snapshot.day;
		time.hour = snapshot.hour;
		time.minute = snapshot.minute;
		time.second = snapshot.second;
		time.microsecond = snapshot.microsecond;
		sceRtcGetTick(&time, &value);
		*tick = value.tick;
	}
	return ret;
}

int32_t TickAddMicroseconds(Tick *dst, const Tick *src, int64_t microseconds)
{
	*dst = *src + (uint64_t)microseconds;
	return 0;
}

int32_t TickAddSeconds(Tick *dst, const Tick *src, int64_t seconds)
{
	*dst = *src + (uint64_t)seconds * 1000000;
	return 0;
}

int32_t TickAddMinutes(Tick *dst, const Tick *src, int64_t minutes)
{
	*dst = *src + (uint64_t)minutes * 60000000;
	return 0;
}

int32_t TickAddHours(Tick *dst, const Tick *src, int32_t hours)
{
	*dst = *src + (uint64_t)hours * 3600000000ULL;
	return 0;
}

int32_t TickAddDays(Tick *dst, const Tick *src, int32_t days)
{
	*dst = *src + (uint64_t)days * 86400000000ULL;
	return 0;
}

int32_t TickAddWeeks(Tick *dst, const Tick *src, int32_t weeks)
{
	*dst = *src + (uint64_t)weeks * 604800000000ULL;
	return 0;
}

static inline __attribute__((always_inline))
void ClampDayAndGetTick(DateTime *date, Tick *dst)
{
	SceDateTime time;
	SceRtcTick value;
	int32_t days = sceRtcGetDaysInMonth(date->year, date->month);

	if (date->day > days)
		date->day = days;
	time.year = date->year;
	time.month = date->month;
	time.day = date->day;
	time.hour = date->hour;
	time.minute = date->minute;
	time.second = date->second;
	time.microsecond = date->microsecond;
	sceRtcGetTick(&time, &value);
	if (dst)
		*dst = value.tick;
}

int32_t TickAddMonths(Tick *dst, const Tick *src, int32_t months)
{
	DateTime date;
	int32_t total;

	ConvertTickToDateTime(src, &date, NULL);
	/* The original wraps this intermediate before signed division. */
	total = (int32_t)((uint32_t)months + date.month + 12u * date.year - 1u);
	date.year = total / 12;
	date.month = total % 12 + 1;
	ClampDayAndGetTick(&date, dst);
	return 0;
}

int32_t TickAddYears(Tick *dst, const Tick *src, int32_t years)
{
	DateTime date;

	ConvertTickToDateTime(src, &date, NULL);
	date.year = (uint32_t)date.year + (uint32_t)years;
	ClampDayAndGetTick(&date, dst);
	return 0;
}

int32_t IsLeapYear(const Tick *tick)
{
	DateTime date;

	ConvertTickToDateTime(tick, &date, NULL);
	return sceRtcIsLeapYear(date.year);
}

int32_t GetDaysInMonth(const Tick *tick)
{
	DateTime date;

	ConvertTickToDateTime(tick, &date, NULL);
	return sceRtcGetDaysInMonth(date.year, date.month);
}

}
}
