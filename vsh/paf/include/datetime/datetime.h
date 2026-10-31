/*
 * paf::datetime::DateTime: a calendar date and time with its UTC offset, read
 * from the clock or parsed, compared and added through rtc ticks, and printed
 * through strftime-like formats: Format with English names, FormatMsg with the
 * system's month and weekday messages in the order the system date format sets.
 */
#ifndef PAF_DATETIME_DATETIME_H
#define PAF_DATETIME_DATETIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus

#include <std/string>
#include "../rtc/rtc.h"

namespace paf {
namespace datetime {

class DateTime {
public:
	DateTime(uint16_t year, uint16_t month, uint16_t day, uint16_t hour, uint16_t minute,
		 uint16_t second, uint32_t microsecond);

	/* What a failed parse gives. */
	DateTime()
	{
		m_year = 2011;
		m_month = 1;
		m_day = 1;
		m_hour = 0;
		m_minute = 0;
		m_second = 0;
		m_microsecond = 0;
		m_tz_offset = 0;
		m_parse_failed = true;
	}

	~DateTime();

	static int32_t DayOfWeek(uint16_t year, uint16_t month, uint16_t day);

	size_t Format(wchar_t *buf, size_t maxLen, const wchar_t *format, bool localtime) const;
	size_t FormatMsg(wchar_t *buf, size_t maxLen, const wchar_t *format) const;

	static DateTime UTC();
	static DateTime Now();
	static DateTime NetworkUTC();

	static void SetDateFormat(const wchar_t *format);
	static wstring &GetDateFormat();
	static void SetTimeFormat(const wchar_t *format);
	static wstring &GetTimeFormat();
	static void SetDateTimeFormat(const wchar_t *format);

	static DateTime ConvertRtcTickToDateTime(rtc::Tick *tick);
	static rtc::Tick ConvertDateTimeToRtcTick(DateTime &dt);

	static DateTime ParseRFC3339(const char *pszDateTime);
	static DateTime ParseSQLiteDateTime(const char *pszDateTime);

	DateTime operator+(const DateTime &r) const;
	bool operator<(const DateTime &r) const;

	uint16_t m_year;
	uint16_t m_month;
	uint16_t m_day;
	uint16_t m_hour;
	uint16_t m_minute;
	uint16_t m_second;
	uint32_t m_microsecond;
	int32_t m_tz_offset;	/* minutes east of UTC */
	bool m_parse_failed;
	int16_t m_reserved;	/* set by no code: copies carry what the stack held */
};

}
}

typedef paf::datetime::DateTime DateTime;

#else

/* The C view, for the files that are still C. */
typedef struct DateTime {
	uint16_t m_year;
	uint16_t m_month;
	uint16_t m_day;
	uint16_t m_hour;
	uint16_t m_minute;
	uint16_t m_second;
	uint32_t m_microsecond;
	int32_t m_tz_offset;
	unsigned char m_parse_failed;
	int16_t m_reserved;
} DateTime;

#define DateTimeConstruct _ZN3paf8datetime8DateTimeC1Ettttttj
#define DateTimeDestroy _ZN3paf8datetime8DateTimeD1Ev

DateTime *DateTimeConstruct(DateTime *self, uint16_t year, uint16_t month, uint16_t day,
			    uint16_t hour, uint16_t minute, uint16_t second,
			    uint32_t microsecond);
DateTime *DateTimeDestroy(DateTime *self);

#endif

#endif
