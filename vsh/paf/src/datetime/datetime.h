#ifndef PAF_DATETIME_DATETIME_H
#define PAF_DATETIME_DATETIME_H

#include <stddef.h>
#include <stdint.h>
#include "../rtc/rtc.h"

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
	int16_t m_parse_result;
} DateTime;

typedef char DateTimeSizeCheck[sizeof(DateTime) == 24 ? 1 : -1];
typedef char DateTimeResultOffsetCheck[offsetof(DateTime, m_parse_result) == 22 ? 1 : -1];

#define DateTimeConstruct ScePafMisc_8523A173
#define DateTimeConstructBase ScePafMisc_AF79665E
#define DateTimeDestroy ScePafMisc_2DBDFC13
#define DateTimeDestroyBase ScePafMisc_897EA53D
#define DateTimeUTC ScePafMisc_59979922
#define DateTimeNow scePafGetCurrentClockLocalTime
#define DateTimeNetworkUTC ScePafMisc_DE76F76B
#define DateTimeConvertRtcTickToDateTime ScePafMisc_6EC994D7
#define DateTimeConvertDateTimeToRtcTick ScePafMisc_08943D3E
#define DateTimeDayOfWeek ScePafMisc_8A79BF51
#define DateTimeAdd ScePafMisc_B5184CAF
#define DateTimeLess ScePafMisc_17B87641
#define DateTimeParseRFC3339 ScePafMisc_209DB708
#define DateTimeParseSQLiteDateTime ScePafMisc_900114B8

void DateTimeParseRFC3339(DateTime *result, const char *pszDateTime);
void DateTimeParseSQLiteDateTime(DateTime *result, const char *pszDateTime);
void DateTimeParseSQLiteDateTimeBody(DateTime *result, const char *pszDateTime,
				   uint32_t incomingParseResult);
uint64_t DateTimeStringLengthState(const char *text);

void DateTimeUTC(DateTime *result);
void DateTimeNow(DateTime *result);
void DateTimeNetworkUTC(DateTime *result);
void DateTimeConvertRtcTickToDateTime(DateTime *result, RtcTick *tick);
RtcTick DateTimeConvertDateTimeToRtcTick(DateTime *dt);
int32_t DateTimeDayOfWeek(uint16_t year, uint16_t month, uint16_t day);
void DateTimeAdd(DateTime *result, const DateTime *self, const DateTime *r);
int32_t DateTimeLess(const DateTime *self, const DateTime *r);

DateTime *DateTimeConstruct(DateTime *self, uint16_t year, uint16_t month,
			    uint16_t day, uint16_t hour, uint16_t minute,
			    uint16_t second, uint32_t microsecond);
DateTime *DateTimeConstructBase(DateTime *self, uint16_t year, uint16_t month,
				uint16_t day, uint16_t hour, uint16_t minute,
				uint16_t second, uint32_t microsecond);
DateTime *DateTimeDestroy(DateTime *self);
DateTime *DateTimeDestroyBase(DateTime *self);

#endif
