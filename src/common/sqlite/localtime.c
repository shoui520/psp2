/* RTC-backed localtime_r shared by Sony's SQLite modules. */

#include <time.h>
#include <psp2/rtc.h>

struct tm *localtime_r(const time_t *timer, struct tm *result)
{
	SceRtcTick tick;
	SceDateTime date;

	sceRtcSetTime_t(&date, *timer);
	sceRtcGetTick(&date, &tick);
	sceRtcConvertUtcToLocalTime(&tick, &tick);
	sceRtcSetTick(&date, &tick);
	result->tm_mon = date.month - 1;
	result->tm_year = date.year - 1900;
	result->tm_hour = date.hour;
	result->tm_mday = date.day;
	result->tm_sec = date.second;
	result->tm_min = date.minute;
	return result;
}
