#ifndef PAF_RTC_RTC_H
#define PAF_RTC_RTC_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t RtcTick;

typedef enum SQLiteDateTimeFormatType {
	INVALID = 0,
	YEAR_ONLY = 1,
	YEAR_MONTH = 2,
	DATE = 3,
	DATE_HOUR = 4,
	DATE_HOUR_MINUTE = 5,
	DATE_HOUR_MINUTE_SECOND = 6,
	DATE_HOUR_MINUTE_SECOND_MILLISECOND = 7
} SQLiteDateTimeFormatType;

typedef struct RtcDateTime {
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint32_t microsecond;
} RtcDateTime;

typedef struct RtcDateTimeEx {
	uint16_t weekday;
	uint16_t yearday;
	uint16_t registry_values[2];
} RtcDateTimeEx;

typedef char RtcDateTimeSizeCheck[sizeof(RtcDateTime) == 16 ? 1 : -1];
typedef char RtcDateTimeExSizeCheck[sizeof(RtcDateTimeEx) == 8 ? 1 : -1];

#define RtcConvertDateTimeToTick ScePafMisc_1B012451
#define RtcConvertTickToDateTime ScePafMisc_EA2AFB9D
#define RtcTickAddMinutes ScePafMisc_7C9BA0C5
#define RtcGetDaysInMonth ScePafMisc_3D21E902
#define RtcIsLeapYear ScePafMisc_A6178A4C
#define RtcGetCurrentTick ScePafMisc_656E8C26
#define RtcConvertUtcToLocalTime ScePafMisc_909913D9
#define RtcGetCurrentLocalTick ScePafMisc_64927F55
#define RtcGetCurrentNetworkTick ScePafMisc_6DB38377
#define RtcConvertLocalTimeToUtc ScePafMisc_F9AE799D
#define RtcFormatRFC3339 ScePafMisc_0CC85E44
#define RtcFormatRFC3339LocalTime ScePafMisc_B94B29AC
#define RtcFormatSQLiteDateTime ScePafMisc_581735E6
#define RtcParseRFC3339 ScePafMisc_E4516D60
#define RtcParseRFC3339LocalTime ScePafMisc_DAAC3337
#define RtcParseSQLiteDateTime ScePafMisc_0853896D
#define RtcParseSQLiteDateTimeWithFormat ScePafMisc_EEF3EE60

int32_t RtcFormatSQLiteDateTime(char *dt, size_t size, const RtcTick *tick, char separator);
int32_t RtcParseRFC3339(RtcTick *tick, const char *dt);
int32_t RtcParseRFC3339LocalTime(RtcTick *tick, const char *dt);
int32_t RtcParseSQLiteDateTimeCore(RtcDateTime *dst, const char *dt,
				 SQLiteDateTimeFormatType *ftype);
int32_t RtcParseSQLiteDateTime(RtcTick *tick, const char *dt);
int32_t RtcParseSQLiteDateTimeWithFormat(RtcTick *tick, SQLiteDateTimeFormatType *ftype,
				       const char *dt);

int32_t RtcGetDaysInMonth(uint16_t year, uint16_t month);
int32_t RtcIsLeapYear(uint16_t year);
int32_t RtcGetCurrentTick(RtcTick *tick);
int32_t RtcConvertUtcToLocalTime(const RtcTick *utc, RtcTick *local_time);
int32_t RtcGetCurrentLocalTick(RtcTick *tick);
int32_t RtcGetCurrentNetworkTick(RtcTick *tick);
int32_t RtcConvertLocalTimeToUtc(const RtcTick *local_time, RtcTick *utc);
int32_t RtcFormatRFC3339(char *dt, const RtcTick *tick, int32_t timezone_minutes);
int32_t RtcFormatRFC3339LocalTime(char *dt, const RtcTick *tick);

int32_t RtcConvertDateTimeToTick(const RtcDateTime *src, RtcTick *tick);
int32_t RtcConvertTickToDateTime(const RtcTick *tick, RtcDateTime *dst,
			       RtcDateTimeEx *exdst);
int32_t RtcTickAddMinutes(RtcTick *dst, const RtcTick *src, int64_t minutes);

#endif
