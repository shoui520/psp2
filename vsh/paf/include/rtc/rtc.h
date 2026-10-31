/*
 * paf::rtc: paf's calendar arithmetic on microsecond ticks. The functions are
 * still C (src/rtc/rtc.c); this is their C++ view, under Sony's names.
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
	uint16_t registry_values[2];
};

int32_t ConvertDateTimeToTick(const DateTime *src, Tick *tick);
int32_t ConvertTickToDateTime(const Tick *tick, DateTime *dst, DateTimeEx *exdst);
int32_t TickAddMinutes(Tick *dst, const Tick *src, int64_t minutes);
int32_t ParseRFC3339(Tick *tick, const char *dt);
int32_t ParseSQLiteDateTime(Tick *tick, const char *dt);

}
}

#endif
