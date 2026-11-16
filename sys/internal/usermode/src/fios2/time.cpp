/*
 * time.cpp: FIOS2 times and dates. A SceFiosTime is the process time in
 * microseconds; a SceFiosDate is nanoseconds since the Unix epoch, through the
 * RTC's SceDateTime.
 */
#include "fios2_internal.h"

#undef sceFiosTimeGetCurrent

SceFiosTime sceFiosTimeGetCurrent()
{
	return sceKernelGetProcessTimeWide();
}

int64_t sceFiosTimeIntervalToNanoseconds(SceFiosTimeInterval interval)
{
	return interval * SCE_FIOS_NANOSECONDS_PER_MICROSECOND;
}

SceFiosTimeInterval sceFiosTimeIntervalFromNanoseconds(int64_t ns)
{
	return SCE_FIOS_DIVIDE_ROUNDING_UP(ns, SCE_FIOS_NANOSECONDS_PER_MICROSECOND);
}

SceFiosDate sceFiosDateGetCurrent()
{
	SceUInt64 seconds;
	SceDateTime time;

	if (sceRtcGetCurrentClock(&time, 0) != 0)
		return 0;
	if (sceRtcGetTime64_t(&time, &seconds) != 0)
		return 0;
	return (seconds * 1000000 + time.microsecond) * SCE_FIOS_NANOSECONDS_PER_MICROSECOND;
}

SceFiosDate sceFiosDateFromComponents(const struct tm *pComponents)
{
	SceFiosDate date = 0;
	SceUInt64 seconds;
	SceDateTime time;

	if (pComponents != NULL) {
		sceClibMemset(&time, 0, sizeof(time));
		time.year = pComponents->tm_year + 1900;
		time.month = pComponents->tm_mon + 1;
		time.day = pComponents->tm_mday;
		time.hour = pComponents->tm_hour;
		time.minute = pComponents->tm_min;
		time.second = pComponents->tm_sec;
		seconds = 0;
		if (sceRtcGetTime64_t(&time, &seconds) == 0)
			date = seconds * SCE_FIOS_NANOSECONDS_PER_SECOND;
	}
	return date;
}

struct tm *sceFiosDateToComponents(SceFiosDate date, struct tm *pOutComponents)
{
	SceDateTime time;

	if (pOutComponents != NULL) {
		sceClibMemset(&time, 0, sizeof(time));
		sceRtcSetTime64_t(&time, date / SCE_FIOS_NANOSECONDS_PER_SECOND);
		pOutComponents->tm_sec = time.second;
		pOutComponents->tm_min = time.minute;
		pOutComponents->tm_hour = time.hour;
		pOutComponents->tm_mday = time.day;
		pOutComponents->tm_mon = time.month - 1;
		pOutComponents->tm_year = time.year - 1900;
		pOutComponents->tm_wday = 0;
		pOutComponents->tm_yday = 0;
		pOutComponents->tm_isdst = 0;
	}
	return pOutComponents;
}

SceFiosDate sceFiosDateFromSceDateTime(const SceDateTime *pSceDateTime)
{
	SceFiosDate date = 0;
	SceUInt64 seconds;

	if (pSceDateTime != NULL) {
		seconds = 0;
		if (sceRtcGetTime64_t(pSceDateTime, &seconds) == 0)
			date = seconds * SCE_FIOS_NANOSECONDS_PER_SECOND +
			       (SceFiosDate)pSceDateTime->microsecond * SCE_FIOS_NANOSECONDS_PER_MICROSECOND;
	}
	return date;
}

SceDateTime *sceFiosDateToSceDateTime(SceFiosDate date, SceDateTime *pSceDateTime)
{
	SceUInt64 seconds;

	if (pSceDateTime != NULL) {
		sceClibMemset(pSceDateTime, 0, sizeof(*pSceDateTime));
		seconds = date / SCE_FIOS_NANOSECONDS_PER_SECOND;
		sceRtcSetTime64_t(pSceDateTime, seconds);
		pSceDateTime->microsecond = (date - seconds * SCE_FIOS_NANOSECONDS_PER_SECOND) /
					    SCE_FIOS_NANOSECONDS_PER_MICROSECOND;
	}
	return pSceDateTime;
}
