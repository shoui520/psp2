#ifndef PAF_RTC_RTC_H
#define PAF_RTC_RTC_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t RtcTick;

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

int32_t RtcConvertDateTimeToTick(const RtcDateTime *src, RtcTick *tick);
int32_t RtcConvertTickToDateTime(const RtcTick *tick, RtcDateTime *dst,
			       RtcDateTimeEx *exdst);
int32_t RtcTickAddMinutes(RtcTick *dst, const RtcTick *src, int64_t minutes);

#endif
