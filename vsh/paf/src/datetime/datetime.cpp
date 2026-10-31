/*
 * paf::datetime::DateTime. Format prints with English day names; FormatMsg
 * takes the month and weekday names from the system's messages (msg_ddmm_* or
 * msg_mmdd_*, by the registry's date format) through the framework's common
 * string hook. The clock reads and the tick arithmetic go to SceRtc.
 */
#include <datetime/datetime.h>
#include <common/char_encoding.h>
#include <common/resource_hooks.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <rtc.h>

extern "C" int sce_paf_swprintf(wchar_t *buffer, size_t bufsz, const wchar_t *format, ...);
extern "C" int sceRegMgrUtilityGetInt(int id, int *value);

/* /CONFIG/DATE/date_format: 0 YYYY/MM/DD, 1 DD/MM/YYYY, 2 MM/DD/YYYY */
#define REGMGR_DATE_FORMAT	0x88776

namespace paf {
namespace datetime {

static const wchar_t *const day_names[] = {
	L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday"
};

static const wchar_t *const abbreviated_day_names[] = {
	L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
};

static const char *const day_of_week_msgs[] = {
	"msg_sunday", "msg_monday", "msg_tuesday", "msg_wednesday", "msg_thursday",
	"msg_friday", "msg_saturday"
};

/* [0]: the day before the month, [1]: the month first */
static const char *const month_msgs[2][12] = {
	{
		"msg_ddmm_january", "msg_ddmm_february", "msg_ddmm_march", "msg_ddmm_april",
		"msg_ddmm_may", "msg_ddmm_june", "msg_ddmm_july", "msg_ddmm_august",
		"msg_ddmm_september", "msg_ddmm_october", "msg_ddmm_november",
		"msg_ddmm_december"
	},
	{
		"msg_mmdd_january", "msg_mmdd_february", "msg_mmdd_march", "msg_mmdd_april",
		"msg_mmdd_may", "msg_mmdd_june", "msg_mmdd_july", "msg_mmdd_august",
		"msg_mmdd_september", "msg_mmdd_october", "msg_mmdd_november",
		"msg_mmdd_december"
	}
};

/* The day of the month, "%1" in the message. */
static const char *const day_msgs[] = {
	"msg_ddmm_dd_variable", "msg_mmdd_dd_variable"
};

static wstring date_format;
static wstring time_format;
static wstring datetime_format;

inline int32_t DateTime::DayOfWeek(uint16_t year, uint16_t month, uint16_t day)
{
	if (month == 1 || month == 2) {
		year--;
		month += 12;
	}
	return (year + year / 4 - year / 100 + year / 400 + (13 * month + 8) / 5 + day) % 7;
}

size_t DateTime::Format(wchar_t *buf, size_t maxLen, const wchar_t *format, bool localtime) const
{
	uint16_t year, month, day, hour, minute, second;
	size_t len = 0;
	bool escaped = false;
	int32_t hour12, year2;
	wchar_t ch;

	if (localtime) {
		rtc::DateTime time;
		rtc::Tick tick, local;

		time.year = m_year;
		time.month = m_month;
		time.day = m_day;
		time.hour = m_hour;
		time.minute = m_minute;
		time.second = m_second;
		time.microsecond = m_microsecond;
		rtc::ConvertDateTimeToTick(&time, &tick);
		rtc::TickAddMinutes(&local, &tick, m_tz_offset);
		rtc::ConvertTickToDateTime(&local, &time, NULL);
		year = time.year;
		month = time.month;
		day = time.day;
		hour = time.hour;
		minute = time.minute;
		second = time.second;
	} else {
		year = m_year;
		month = m_month;
		day = m_day;
		hour = m_hour;
		minute = m_minute;
		second = m_second;
	}
	hour12 = (hour + 11) % 12 + 1;
	year2 = year % 100;
	for (ch = *format; ch != 0 && len + 1 < maxLen; ch = *++format) {
		if (escaped) {
			switch (ch) {
			case 'a':
				len += sce_paf_swprintf(&buf[len], maxLen - len, L"%s",
							abbreviated_day_names[DayOfWeek(year, month, day)]);
				break;
			case 'A':
				len += sce_paf_swprintf(&buf[len], maxLen - len, L"%s",
							day_names[DayOfWeek(year, month, day)]);
				break;
			case 'c':
				len += Format(&buf[len], maxLen - len, datetime_format.c_str(), localtime);
				break;
			case 'd':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"--");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%d", day);
				break;
			case 'H':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"--");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%d", hour);
				break;
			case 'I':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"--");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%d", hour12);
				break;
			case 'm':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"--");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%d", month);
				break;
			case 'M':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"--");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%02d", minute);
				break;
			case 'p':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"--");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%ls",
								hour < 12 ? L"AM" : L"PM");
				break;
			case 'S':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"--");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%02d", second);
				break;
			case 'x':
				len += Format(&buf[len], maxLen - len, date_format.c_str(), localtime);
				break;
			case 'X':
				len += Format(&buf[len], maxLen - len, time_format.c_str(), localtime);
				break;
			case 'y':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"--");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%02d", year2);
				break;
			case 'Y':
				if (m_parse_failed == 1)
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"----");
				else
					len += sce_paf_swprintf(&buf[len], maxLen - len, L"%d", year);
				break;
			case '%':
				buf[len++] = '%';
				break;
			}
			escaped = false;
		} else if (ch == '%') {
			escaped = true;
		} else {
			buf[len++] = ch;
		}
	}
	if (maxLen <= len)
		buf[len - 1] = 0;
	else
		buf[len] = 0;
	return len;
}

/* UTF-8 to UTF-16 and back, through a string of the other kind. */
static inline wstring ToWString(const char *src)
{
	string src8(src);
	wstring dst;

	common::Utf8ToUtf16(src8, &dst, 0);
	return dst;
}

static inline string ToString(const wchar_t *src)
{
	wstring src16(src);
	string dst;

	common::Utf16ToUtf8(src16, &dst);
	return dst;
}

/* The weekday's message, or an empty string without a month and day. */
static wstring GetDayOfWeekString(uint16_t year, int32_t month, int32_t day)
{
	wstring result(L"");

	if (month > 0 && day > 0)
		result = common::GetCommonString(
			IDParam(day_of_week_msgs[DateTime::DayOfWeek(year, month, day)]));
	return result;
}

static wstring GetMonthString(int32_t month, int32_t monthFirst)
{
	wstring result(L"");

	if (month > 0)
		result = common::GetCommonString(IDParam(month_msgs[monthFirst][month - 1]));
	return result;
}

/* The day of the month put into its message for "%1". */
static wstring GetDayString(int32_t day, int32_t monthFirst)
{
	wstring result;
	char num[8];

	sce_paf_snprintf(num, sizeof(num), "%d", day);

	wstring number = ToWString(num);
	const wchar_t *digits = number.c_str();
	const wchar_t *msg = common::GetCommonString(IDParam(day_msgs[monthFirst]));
	string msg8 = ToString(msg);
	const wchar_t *arg = sce_paf_wcschr(msg, '%');

	if (arg != NULL) {
		if (arg[1] == '1') {
			result.append(msg, arg - msg);
			result.append(digits);
			result.append(arg + 2);
		} else {
			result = msg;
		}
	} else {
		result = msg;
	}
	return result;
}

size_t DateTime::FormatMsg(wchar_t *buf, size_t maxLen, const wchar_t *format) const
{
	wstring result(L"");
	int32_t dateFormat;
	int32_t monthFirst;
	size_t len;
	bool escaped;
	wchar_t ch;

	if (sceRegMgrUtilityGetInt(REGMGR_DATE_FORMAT, &dateFormat) < 0) {
		sce_paf_printf_dummy();
		return 0;
	}
	switch (dateFormat) {
	case 0:
	case 2:
		monthFirst = 1;
		break;
	case 1:
		monthFirst = 0;
		break;
	default:
		monthFirst = 0;
		break;
	}
	escaped = false;
	len = 0;
	for (ch = *format; ch != 0 && len + 1 < maxLen; ch = *++format) {
		if (escaped) {
			switch (ch) {
			case 'A':
			case 'a': {
				wstring name = GetDayOfWeekString(m_year, m_month, m_day);

				len += name.length();
				result.append(name);
				break;
			}
			case 'c': {
				wstring monthName = GetMonthString(m_month, monthFirst);
				wstring dayName = GetDayString(m_day, monthFirst);
				wstring weekday = GetDayOfWeekString(m_year, m_month, m_day);

				if (monthFirst)
					result.append(monthName + L" " + dayName + L" (" + weekday + L")");
				else
					result.append(dayName + L" " + monthName + L" (" + weekday + L")");
				len += monthName.length() + dayName.length() + weekday.length() + 4;
				break;
			}
			case 'x': {
				wstring monthName = GetMonthString(m_month, monthFirst);
				wstring dayName = GetDayString(m_day, monthFirst);

				if (monthFirst)
					result.append(monthName + L" " + dayName);
				else
					result.append(dayName + L" " + monthName);
				len += monthName.length() + dayName.length() + 1;
				break;
			}
			case 'y': {
				char num[8];

				sce_paf_snprintf(num, 7, "%02d", m_year % 100);

				wstring year = ToWString(num);

				result.append(year);
				len += year.length();
				break;
			}
			case 'Y': {
				char num[8];

				sce_paf_snprintf(num, 7, "%d", m_year);

				wstring year = ToWString(num);

				result.append(year);
				len += year.length();
				break;
			}
			case '%':
				result.append(L"%");
				len++;
				break;
			}
			escaped = false;
		} else if (ch == '%') {
			escaped = true;
		} else {
			result.append(1, ch);
			len++;
		}
	}
	sce_paf_swprintf(buf, maxLen, L"%ls", result.c_str());
	if (maxLen <= len)
		buf[len - 1] = 0;
	else
		buf[len] = 0;
	return len;
}

DateTime::DateTime(uint16_t year, uint16_t month, uint16_t day, uint16_t hour, uint16_t minute,
		   uint16_t second, uint32_t microsecond)
{
	m_year = year;
	m_month = month;
	m_day = day;
	m_hour = hour;
	m_minute = minute;
	m_second = second;
	m_parse_failed = false;
	m_tz_offset = 0;
	m_microsecond = microsecond;
}

DateTime DateTime::UTC()
{
	SceDateTime time;

	sceRtcGetCurrentClock(&time, 0);
	return DateTime(time.year, time.month, time.day, time.hour, time.minute, time.second,
			time.microsecond);
}

DateTime DateTime::Now()
{
	SceDateTime time;

	sceRtcGetCurrentClockLocalTime(&time);
	return DateTime(time.year, time.month, time.day, time.hour, time.minute, time.second,
			time.microsecond);
}

DateTime DateTime::NetworkUTC()
{
	SceRtcTick tick;
	SceDateTime time;

	sceRtcGetCurrentNetworkTick(&tick);
	sceRtcSetTick(&time, &tick);
	return DateTime(time.year, time.month, time.day, time.hour, time.minute, time.second,
			time.microsecond);
}

void DateTime::SetDateFormat(const wchar_t *format)
{
	date_format = format;
}

wstring &DateTime::GetDateFormat()
{
	return date_format;
}

void DateTime::SetTimeFormat(const wchar_t *format)
{
	time_format = format;
}

wstring &DateTime::GetTimeFormat()
{
	return time_format;
}

void DateTime::SetDateTimeFormat(const wchar_t *format)
{
	datetime_format = format;
}

DateTime DateTime::ConvertRtcTickToDateTime(rtc::Tick *tick)
{
	rtc::DateTime time;

	rtc::ConvertTickToDateTime(tick, &time, NULL);
	return DateTime(time.year, time.month, time.day, time.hour, time.minute, time.second,
			time.microsecond);
}

rtc::Tick DateTime::ConvertDateTimeToRtcTick(DateTime &dt)
{
	rtc::DateTime time;
	rtc::Tick tick;

	time.year = dt.m_year;
	time.month = dt.m_month;
	time.day = dt.m_day;
	time.hour = dt.m_hour;
	time.minute = dt.m_minute;
	time.second = dt.m_second;
	time.microsecond = dt.m_microsecond;
	rtc::ConvertDateTimeToTick(&time, &tick);
	return tick;
}

DateTime::~DateTime()
{
}

/* The date and time, then the offset from the suffix: Z, or +hh:mm or -hh:mm. */
DateTime DateTime::ParseRFC3339(const char *pszDateTime)
{
	DateTime failed;
	rtc::Tick tick;
	rtc::DateTime time;
	int32_t tz;

	if (rtc::ParseRFC3339(&tick, pszDateTime) != 0)
		return failed;
	rtc::ConvertTickToDateTime(&tick, &time, NULL);

	DateTime result(time.year, time.month, time.day, time.hour, time.minute, time.second,
			time.microsecond);

	tz = 0;
	if (pszDateTime != NULL) {
		int32_t len = sce_paf_strlen(pszDateTime);

		if (len >= 19) {
			const char *end = pszDateTime + len;
			const char *p = pszDateTime + 19;

			if (end - p > 0) {
				if (*p == '.') {
					p++;
					while (end - p > 0 && *p >= '0' && *p <= '9')
						p++;
				}
				if (end - p > 0) {
					char sign = *p;

					if (sign == 'Z' || sign == 'z') {
						tz = 0;
					} else if ((sign == '+' || sign == '-') && end - (p + 1) >= 5) {
						char h0 = p[1], h1 = p[2], m0 = p[4], m1 = p[5];
						int32_t hours, minutes;

						if (h0 >= '0' && h0 <= '9' && h1 >= '0' && h1 <= '9')
							hours = (h0 - '0') * 10 + h1 - '0';
						else
							hours = -1;
						if (m0 >= '0' && m0 <= '9' && m1 >= '0' && m1 <= '9')
							minutes = (m0 - '0') * 10 + m1 - '0';
						else
							minutes = -1;
						if (hours >= 0 && p[3] == ':' && minutes >= 0) {
							tz = hours * 60 + minutes;
							if (sign == '-')
								tz = -tz;
						}
					}
				}
			}
		}
	}
	result.m_tz_offset = tz;
	return result;
}

DateTime DateTime::ParseSQLiteDateTime(const char *pszDateTime)
{
	DateTime failed;
	rtc::Tick tick;
	rtc::DateTime time;

	if (rtc::ParseSQLiteDateTime(&tick, pszDateTime) != 0)
		return failed;
	rtc::ConvertTickToDateTime(&tick, &time, NULL);

	DateTime result(time.year, time.month, time.day, time.hour, time.minute, time.second,
			time.microsecond);

	return result;
}

DateTime DateTime::operator+(const DateTime &r) const
{
	SceDateTime time;
	SceRtcTick tick;
	SceDateTime sum;

	time.year = m_year;
	time.month = m_month;
	time.day = m_day;
	time.hour = m_hour;
	time.minute = m_minute;
	time.second = m_second;
	time.microsecond = m_microsecond;

	uint16_t years = r.m_year;
	uint16_t months = r.m_month;
	uint16_t days = r.m_day;
	uint16_t hours = r.m_hour;
	uint16_t minutes = r.m_minute;
	uint16_t seconds = r.m_second;
	uint32_t microseconds = r.m_microsecond;

	sceRtcGetTick(&time, &tick);
	if (years != 0)
		sceRtcTickAddYears(&tick, &tick, years);
	if (months != 0)
		sceRtcTickAddMonths(&tick, &tick, months);
	if (days != 0)
		sceRtcTickAddDays(&tick, &tick, days);
	if (hours != 0)
		sceRtcTickAddHours(&tick, &tick, hours);
	if (minutes != 0)
		sceRtcTickAddMinutes(&tick, &tick, minutes);
	if (seconds != 0)
		sceRtcTickAddSeconds(&tick, &tick, seconds);
	if (microseconds != 0)
		sceRtcTickAddMicroseconds(&tick, &tick, microseconds);
	sceRtcSetTick(&sum, &tick);
	return DateTime(sum.year, sum.month, sum.day, sum.hour, sum.minute, sum.second,
			sum.microsecond);
}

bool DateTime::operator<(const DateTime &r) const
{
	SceDateTime left, right;
	SceRtcTick leftTick, rightTick;

	left.year = m_year;
	left.month = m_month;
	left.day = m_day;
	left.hour = m_hour;
	left.minute = m_minute;
	left.second = m_second;
	left.microsecond = m_microsecond;
	right.year = r.m_year;
	right.month = r.m_month;
	right.day = r.m_day;
	right.hour = r.m_hour;
	right.minute = r.m_minute;
	right.second = r.m_second;
	right.microsecond = r.m_microsecond;
	sceRtcGetTick(&left, &leftTick);
	sceRtcGetTick(&right, &rightTick);
	return leftTick.tick < rightTick.tick;
}

}
}
