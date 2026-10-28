#include "rtc.h"
#include <rtc.h>

extern int32_t sceRtcGetTick(const SceDateTime *time, SceRtcTick *tick);
extern int32_t sceRtcSetTick(SceDateTime *time, const SceRtcTick *tick);
extern int32_t sceRtcGetDayOfWeek(int32_t year, int32_t month, int32_t day);
extern int32_t sceRtcIsLeapYear(int32_t year);
extern int32_t sceRtcGetDaysInMonth(int32_t year, int32_t month);
extern int32_t sceRtcGetCurrentTick(SceRtcTick *tick);
extern int32_t sceRtcGetCurrentNetworkTick(SceRtcTick *tick);
extern int32_t sceRtcConvertUtcToLocalTime(const SceRtcTick *utc, SceRtcTick *local_time);
extern int32_t sceRtcConvertLocalTimeToUtc(const SceRtcTick *local_time, SceRtcTick *utc);
extern int32_t sceRtcFormatRFC3339(char *dt, const SceRtcTick *utc, int32_t timezone_minutes);
extern int32_t sceRtcFormatRFC3339LocalTime(char *dt, const SceRtcTick *utc);
extern int32_t sceRtcParseRFC3339(SceRtcTick *utc, const char *dt);
extern int32_t sceRtcCheckValid(const SceDateTime *time);
extern int32_t sceRegMgrUtilityGetInt(uint32_t id, int32_t *value);
extern void *sce_paf_memset(void *dst, int ch, size_t len);
extern char *sce_paf_strncpy(char *dst, const char *src, size_t len);
extern int sce_paf_strncmp(const char *left, const char *right, size_t len);
extern size_t sce_paf_strlen(const char *str);

int32_t RtcGetDaysInMonth(uint16_t year, uint16_t month)
{
	return sceRtcGetDaysInMonth(year, month);
}

int32_t RtcIsLeapYear(uint16_t year)
{
	return sceRtcIsLeapYear(year);
}

int32_t RtcGetCurrentTick(RtcTick *tick)
{
	SceRtcTick value;
	int32_t ret = sceRtcGetCurrentTick(&value);

	if (tick)
		*tick = value.tick;
	return ret;
}

int32_t RtcConvertUtcToLocalTime(const RtcTick *utc, RtcTick *local_time)
{
	SceRtcTick src, dst;
	int32_t ret;

	src.tick = *utc;
	ret = sceRtcConvertUtcToLocalTime(&src, &dst);
	if (local_time)
		*local_time = dst.tick;
	return ret;
}

int32_t RtcGetCurrentLocalTick(RtcTick *tick)
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

int32_t RtcGetCurrentNetworkTick(RtcTick *tick)
{
	SceRtcTick value;
	int32_t ret = sceRtcGetCurrentNetworkTick(&value);

	if (tick && !ret)
		*tick = value.tick;
	return ret;
}

int32_t RtcConvertLocalTimeToUtc(const RtcTick *local_time, RtcTick *utc)
{
	SceRtcTick src, dst;
	int32_t ret;

	src.tick = *local_time;
	ret = sceRtcConvertLocalTimeToUtc(&src, &dst);
	if (utc)
		*utc = dst.tick;
	return ret;
}

int32_t RtcFormatRFC3339(char *dt, const RtcTick *tick, int32_t timezone_minutes)
{
	SceRtcTick utc, current;
	RtcTick value;
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

int32_t RtcFormatRFC3339LocalTime(char *dt, const RtcTick *tick)
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

int32_t RtcFormatSQLiteDateTime(char *dt, size_t size, const RtcTick *tick, char separator)
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
		year = 2010;
		month = day = 1;
		hour = minute = second = 0;
		microsecond = 0;
	} else {
		year = time.year;
		month = time.month;
		day = time.day;
		hour = time.hour;
		minute = time.minute;
		second = time.second;
		microsecond = time.microsecond;
	}
	text[0] = year / 1000 % 10 + '0';
	text[1] = year / 100 % 10 + '0';
	text[2] = year / 10 % 10 + '0';
	text[3] = year % 10 + '0';
	text[4] = '-';
	text[5] = month / 10 % 10 + '0';
	text[6] = month % 10 + '0';
	text[7] = '-';
	text[8] = day / 10 % 10 + '0';
	text[9] = day % 10 + '0';
	text[10] = separator;
	text[11] = hour / 10 % 10 + '0';
	text[12] = hour % 10 + '0';
	text[13] = ':';
	text[14] = minute / 10 % 10 + '0';
	text[15] = minute % 10 + '0';
	text[16] = ':';
	text[17] = second / 10 % 10 + '0';
	text[18] = second % 10 + '0';
	text[19] = '.';
	millisecond = microsecond / 1000;
	text[20] = millisecond / 100 % 10 + '0';
	text[21] = millisecond / 10 % 10 + '0';
	text[22] = millisecond % 10 + '0';
	text[23] = 0;
	sce_paf_memset(dt, 0, size);
	sce_paf_strncpy(dt, text, size - 1);
	return 0;
}

int32_t RtcParseRFC3339(RtcTick *tick, const char *dt)
{
	SceRtcTick value;
	int32_t ret;

	if (!tick || !dt)
		return SCE_RTC_ERROR_INVALID_POINTER;
	ret = sceRtcParseRFC3339(&value, dt);
	*tick = value.tick;
	return ret;
}

int32_t RtcParseRFC3339LocalTime(RtcTick *tick, const char *dt)
{
	return 0;
}

static inline __attribute__((always_inline))
int RtcSQLiteDigit(int ch)
{
	return ch >= '0' && ch <= '9';
}

static inline __attribute__((always_inline))
int RtcSQLiteReadPair(const char *dt)
{
	int first = (int8_t)dt[0], second = (int8_t)dt[1];

	if (!RtcSQLiteDigit(first) || !RtcSQLiteDigit(second))
		return -1;
	return 10 * first + second - 11 * '0';
}

static inline __attribute__((always_inline))
int32_t RtcCheckPartialSQLiteDateTime(const RtcDateTime *dst,
				    SQLiteDateTimeFormatType *ftype,
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

int32_t RtcParseSQLiteDateTimeCore(RtcDateTime *dst, const char *dt,
				 SQLiteDateTimeFormatType *ftype)
{
	SceDateTime time;
	int32_t length, year, month, day, hour, minute, second;
	int first, next, third, fourth;
	uint32_t microsecond;

	if (ftype)
		*ftype = INVALID;
	if (!dst || !dt)
		return SCE_RTC_ERROR_INVALID_VALUE;
	sce_paf_memset(dst, 0, sizeof(*dst));
	if (ftype) {
		dst->year = 2010;
		dst->month = 1;
		dst->day = 1;
		dst->hour = 0;
		dst->minute = 0;
		dst->second = 0;
		dst->microsecond = 0;
	}
	length = (int32_t)sce_paf_strlen(dt);
	if (length < 10 && !ftype)
		return SCE_RTC_ERROR_INVALID_VALUE;
	first = (int8_t)dt[0];
	next = (int8_t)dt[1];
	third = (int8_t)dt[2];
	fourth = (int8_t)dt[3];
	if (!RtcSQLiteDigit(first) || !RtcSQLiteDigit(next) ||
	    !RtcSQLiteDigit(third) || !RtcSQLiteDigit(fourth))
		return SCE_RTC_ERROR_INVALID_YEAR;
	year = 1000 * first + 100 * next + 10 * third + fourth - 1111 * '0';
	dst->year = year;
	if (length <= 4 && ftype)
		return RtcCheckPartialSQLiteDateTime(dst, ftype, YEAR_ONLY);
	if (dt[4] != '-')
		return SCE_RTC_ERROR_BAD_PARSE;
	month = RtcSQLiteReadPair(dt + 5);
	if (month < 0)
		return SCE_RTC_ERROR_INVALID_MONTH;
	dst->month = month;
	if (length <= 7 && ftype)
		return RtcCheckPartialSQLiteDateTime(dst, ftype, YEAR_MONTH);
	if (dt[7] != '-')
		return SCE_RTC_ERROR_BAD_PARSE;
	day = RtcSQLiteReadPair(dt + 8);
	if (day < 0)
		return SCE_RTC_ERROR_INVALID_DAY;
	dst->day = day;
	if (length <= 10) {
		hour = dst->hour;
		minute = dst->minute;
		second = dst->second;
		if (ftype)
			*ftype = DATE;
		microsecond = dst->microsecond;
		goto validate;
	}
	if (dt[10] != ' ' && dt[10] != 'T')
		return SCE_RTC_ERROR_BAD_PARSE;
	hour = RtcSQLiteReadPair(dt + 11);
	if (hour < 0)
		return SCE_RTC_ERROR_INVALID_HOUR;
	dst->hour = hour;
	if (length <= 13 && ftype)
		return RtcCheckPartialSQLiteDateTime(dst, ftype, DATE_HOUR);
	if (dt[13] != ':')
		return SCE_RTC_ERROR_BAD_PARSE;
	minute = RtcSQLiteReadPair(dt + 14);
	if (minute < 0)
		return SCE_RTC_ERROR_INVALID_MINUTE;
	dst->minute = minute;
	if (length <= 16) {
		second = dst->second;
		if (ftype)
			*ftype = DATE_HOUR_MINUTE;
		microsecond = dst->microsecond;
		goto validate;
	}
	if (dt[16] != ':')
		return SCE_RTC_ERROR_BAD_PARSE;
	second = RtcSQLiteReadPair(dt + 17);
	if (second < 0)
		return SCE_RTC_ERROR_INVALID_SECOND;
	dst->second = second;
	if (length <= 19) {
		if (ftype)
			*ftype = DATE_HOUR_MINUTE_SECOND;
		microsecond = dst->microsecond;
	} else if (dt[19] == '.') {
		const char *cursor = dt + 20;
		const char *end = dt + length;
		uint32_t weight = 100000;

		microsecond = 0;
		while (end - cursor > 0) {
			int ch = (int8_t)*cursor;

			if (!RtcSQLiteDigit(ch))
				break;
			microsecond += (ch - '0') * weight;
			weight /= 10;
			++cursor;
		}
		dst->microsecond = microsecond;
		if (ftype) {
			*ftype = DATE_HOUR_MINUTE_SECOND_MILLISECOND;
			microsecond = dst->microsecond;
		}
	} else {
		/* Non-fraction suffixes are ignored without setting a format. */
		microsecond = 0;
		dst->microsecond = 0;
	}
validate:
	time.year = year;
	time.month = month;
	time.day = day;
	time.hour = hour;
	time.minute = minute;
	time.second = second;
	time.microsecond = microsecond;
	return sceRtcCheckValid(&time);
}

int32_t RtcParseSQLiteDateTime(RtcTick *tick, const char *dt)
{
	SceRtcTick current, value;
	RtcDateTime parsed;
	SceDateTime time;
	int32_t ret;

	if (!tick || !dt)
		return SCE_RTC_ERROR_INVALID_VALUE;
	if (sce_paf_strlen(dt) == 3 && !sce_paf_strncmp(dt, "now", 3)) {
		ret = sceRtcGetCurrentTick(&current);
		*tick = current.tick;
		return ret;
	}
	ret = RtcParseSQLiteDateTimeCore(&parsed, dt, NULL);
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

int32_t RtcParseSQLiteDateTimeWithFormat(RtcTick *tick, SQLiteDateTimeFormatType *ftype,
				       const char *dt)
{
	SceRtcTick current, value;
	RtcDateTime parsed, snapshot;
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
	ret = RtcParseSQLiteDateTimeCore(&parsed, dt, &type);
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

int32_t RtcConvertDateTimeToTick(const RtcDateTime *src, RtcTick *tick)
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

int32_t RtcConvertTickToDateTime(const RtcTick *tick, RtcDateTime *dst,
			       RtcDateTimeEx *exdst)
{
	SceRtcTick value;
	SceDateTime time;
	int32_t ret;

	value.tick = *tick;
	ret = sceRtcSetTick(&time, &value);
	if (ret) {
		dst->year = 2010;
		dst->month = 1;
		dst->day = 1;
		dst->hour = 0;
		dst->minute = 0;
		dst->second = 0;
		dst->microsecond = 0;
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
		int32_t first_setting, second_setting;
		uint16_t first = 0, second = 0;

		sce_paf_memset(exdst, 0, sizeof(*exdst));
		exdst->weekday = sceRtcGetDayOfWeek(dst->year, dst->month, dst->day);
		month = dst->month;
		adjusted_month = month <= 2 ? month + 12 : month;
		year = dst->year;
		day = dst->day;
		yearday = 979 * adjusted_month - 1033;
		yearday = day + ((yearday + ((uint32_t)(yearday >> 5) >> 27)) >> 5) - 1;
		if (month >= 3) {
			int32_t leap = sceRtcIsLeapYear(year);

			if (leap >= 0)
				yearday += leap;
		}
		exdst->yearday = yearday;
		if (sceRegMgrUtilityGetInt(0x00760538, &first_setting) >= 0)
			first = first_setting;
		exdst->registry_values[0] = first;
		if (sceRegMgrUtilityGetInt(0x00668503, &second_setting) >= 0)
			second = second_setting;
		exdst->registry_values[1] = second;
	}
	return ret;
}

int32_t RtcTickAddMinutes(RtcTick *dst, const RtcTick *src, int64_t minutes)
{
	*dst = *src + (uint64_t)minutes * 60000000;
	return 0;
}
