#ifndef PAF_DATETIME_DATETIME_H
#define PAF_DATETIME_DATETIME_H

#include <stddef.h>
#include <stdint.h>

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

DateTime *DateTimeConstruct(DateTime *self, uint16_t year, uint16_t month,
			    uint16_t day, uint16_t hour, uint16_t minute,
			    uint16_t second, uint32_t microsecond);
DateTime *DateTimeConstructBase(DateTime *self, uint16_t year, uint16_t month,
				uint16_t day, uint16_t hour, uint16_t minute,
				uint16_t second, uint32_t microsecond);
DateTime *DateTimeDestroy(DateTime *self);
DateTime *DateTimeDestroyBase(DateTime *self);

#endif
