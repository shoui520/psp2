/*
 * paf::rtc: paf's calendar arithmetic on microsecond ticks, over SceRtc. A tick
 * that does not convert gives 2010-01-01 00:00:00.
 */
#ifndef PAF_RTC_RTC_H
#define PAF_RTC_RTC_H

#include <stddef.h>
#include <stdint.h>

namespace paf {
namespace rtc {

typedef uint64_t Tick;

class DateTime {
public:
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint32_t microsecond;
};

class DateTimeEx {
public:
	uint16_t weekday;
	uint16_t yearday;
	/* the registry's /CONFIG/DATE/summer_time and time_format */
	uint16_t summer_time;
	uint16_t time_format;
};

/* how much of a date and time ParseSQLiteDateTime found */
enum SQLiteDateTimeFormatType {
	INVALID = 0,
	YEAR_ONLY = 1,
	YEAR_MONTH = 2,
	DATE = 3,
	DATE_HOUR = 4,
	DATE_HOUR_MINUTE = 5,
	DATE_HOUR_MINUTE_SECOND = 6,
	DATE_HOUR_MINUTE_SECOND_MILLISECOND = 7
};

int32_t ConvertDateTimeToTick(const DateTime *src, Tick *tick);
int32_t GetDaysInMonth(uint16_t year, uint16_t month);
int32_t IsLeapYear(uint16_t year);
int32_t ConvertTickToDateTime(const Tick *tick, DateTime *dst, DateTimeEx *exdst);
int32_t GetCurrentTick(Tick *tick);
int32_t ConvertUtcToLocalTime(const Tick *utc, Tick *local_time);
int32_t GetCurrentLocalTick(Tick *tick);
int32_t GetCurrentNetworkTick(Tick *tick);
int32_t ConvertLocalTimeToUtc(const Tick *local_time, Tick *utc);
int32_t FormatRFC3339(char *dt, const Tick *tick, int32_t timezone_minutes);
int32_t FormatRFC3339LocalTime(char *dt, const Tick *tick);
int32_t FormatSQLiteDateTime(char *dt, size_t size, const Tick *tick, char separator);
int32_t ParseRFC3339(Tick *tick, const char *dt);
int32_t ParseRFC3339LocalTime(Tick *tick, const char *dt);
int32_t ParseSQLiteDateTime(Tick *tick, const char *dt);
int32_t ParseSQLiteDateTime(Tick *tick, SQLiteDateTimeFormatType *ftype, const char *dt);
int32_t TickAddMicroseconds(Tick *dst, const Tick *src, int64_t microseconds);
int32_t TickAddSeconds(Tick *dst, const Tick *src, int64_t seconds);
int32_t TickAddMinutes(Tick *dst, const Tick *src, int64_t minutes);
int32_t TickAddHours(Tick *dst, const Tick *src, int32_t hours);
int32_t TickAddDays(Tick *dst, const Tick *src, int32_t days);
int32_t TickAddWeeks(Tick *dst, const Tick *src, int32_t weeks);
int32_t TickAddMonths(Tick *dst, const Tick *src, int32_t months);
int32_t TickAddYears(Tick *dst, const Tick *src, int32_t years);
int32_t IsLeapYear(const Tick *tick);
int32_t GetDaysInMonth(const Tick *tick);

}
}

#endif
