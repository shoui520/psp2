/*
 * The library's clock: the RTC's ticks (microseconds since 0001-01-01) turned into
 * seconds or minutes since 1970-01-01 and back into a broken-down time with its UTC offset
 * and daylight saving. The file's name is ours.
 */
#include <rtc.h>
#include "mln_internal.h"

#define MLN_TIME_ERROR		1
#define MLN_TIME_ERROR_INVALID	3

/* 1970-01-01 00:00:00 in RTC ticks */
#define MLN_UNIX_EPOCH_TICK	62135596800000000ULL

#define MLN_SECOND_TICKS	1000000
#define MLN_MINUTE_TICKS	60000000
#define MLN_DST_SECONDS		3600

int MlnImpl_8100702C(void)
{
	return MLN_OK;
}

unsigned int MlnTimeNow(void)
{
	SceRtcTick tick;

	sceRtcGetCurrentTick(&tick);
	if (tick.tick < MLN_UNIX_EPOCH_TICK)
		return 0;
	return (tick.tick - MLN_UNIX_EPOCH_TICK) / MLN_SECOND_TICKS;
}

int MlnImpl_81007098(void)
{
	return MLN_OK;
}

int MlnTimeGetTick(unsigned int *tick)
{
	SceRtcTick now;

	sceRtcGetCurrentTick(&now);
	*tick = (unsigned int)now.tick;
	return MLN_OK;
}

int MlnTimeToSeconds(const MlnDateTime *time, int *seconds)
{
	SceRtcTick tick;
	SceDateTime date;
	int value;

	if ((unsigned int)(time->year - 1) > 9998)
		return MLN_TIME_ERROR;
	date.year = time->year;
	if ((unsigned int)(time->month - 1) > 11)
		return MLN_TIME_ERROR;
	date.month = time->month;
	if ((unsigned int)(time->day - 1) > 30)
		return MLN_TIME_ERROR;
	date.day = time->day;
	if ((unsigned int)time->hour > 23)
		return MLN_TIME_ERROR;
	date.hour = time->hour;
	if ((unsigned int)time->minute > 59)
		return MLN_TIME_ERROR;
	date.minute = time->minute;
	if ((unsigned int)time->second > 59)
		return MLN_TIME_ERROR;
	date.second = time->second;
	if ((unsigned int)time->microsecond > 999999)
		return MLN_TIME_ERROR;
	date.microsecond = time->microsecond;
	if (sceRtcGetTick(&date, &tick) != 0 || tick.tick < MLN_UNIX_EPOCH_TICK)
		return MLN_TIME_ERROR;
	value = (unsigned int)((tick.tick - MLN_UNIX_EPOCH_TICK) / MLN_SECOND_TICKS) -
		time->utcOffset;
	if (time->daylightSaving != 0)
		value -= MLN_DST_SECONDS;
	*seconds = value;
	return MLN_OK;
}

int MlnTimeFromSeconds(const unsigned int *seconds, int utcOffset, int daylightSaving,
		       MlnDateTime *time)
{
	SceRtcTick tick;
	SceDateTime date;
	unsigned int local = *seconds + utcOffset;

	if (daylightSaving != 0)
		local += MLN_DST_SECONDS;
	tick.tick = MLN_UNIX_EPOCH_TICK;
	if (sceRtcTickAddSeconds(&tick, &tick, local) != 0 || sceRtcSetTick(&date, &tick) != 0)
		return MLN_TIME_ERROR;
	MlnMemset(time, 0, sizeof(*time));
	time->year = date.year;
	time->month = date.month;
	time->day = date.day;
	time->hour = date.hour;
	time->minute = date.minute;
	time->second = date.second;
	time->microsecond = 0;
	time->utcOffset = utcOffset;
	time->daylightSaving = daylightSaving != 0;
	return MLN_OK;
}

int MlnTimeFromMinutes(const unsigned int *minutes, MlnDateTime *time)
{
	SceRtcTick tick;
	SceDateTime date;

	tick.tick = MLN_UNIX_EPOCH_TICK;
	if (sceRtcTickAddMinutes(&tick, &tick, *minutes) != 0 || sceRtcSetTick(&date, &tick) != 0)
		return MLN_TIME_ERROR;
	MlnMemset(time, 0, sizeof(*time));
	time->year = date.year;
	time->month = date.month;
	time->day = date.day;
	time->hour = date.hour;
	time->minute = date.minute;
	time->second = date.second;
	time->microsecond = 0;
	time->utcOffset = 0;
	time->daylightSaving = 0;
	return MLN_OK;
}

/* Out-of-range fields leave the RTC date's field unset, as Sony's does. */
int MlnTimeToMinutes(const MlnDateTime *time, unsigned int *minutes)
{
	SceRtcTick tick;
	SceDateTime date;
	int utcOffset = time->utcOffset;

	if ((unsigned int)(time->year - 1) <= 9998)
		date.year = time->year;
	if ((unsigned int)(time->month - 1) <= 11)
		date.month = time->month;
	if ((unsigned int)(time->day - 1) <= 30)
		date.day = time->day;
	if ((unsigned int)time->hour <= 23)
		date.hour = time->hour;
	if ((unsigned int)time->minute <= 59)
		date.minute = time->minute;
	if ((unsigned int)time->second <= 59)
		date.second = time->second;
	if ((unsigned int)time->microsecond <= 999999)
		date.microsecond = time->microsecond;
	if (sceRtcGetTick(&date, &tick) != 0)
		return MLN_TIME_ERROR_INVALID;
	sceRtcTickAddSeconds(&tick, &tick, utcOffset);
	if (time->daylightSaving != 0)
		sceRtcTickAddSeconds(&tick, &tick, -MLN_DST_SECONDS);
	if (tick.tick < MLN_UNIX_EPOCH_TICK)
		return MLN_TIME_ERROR_INVALID;
	*minutes = (tick.tick - MLN_UNIX_EPOCH_TICK) / MLN_MINUTE_TICKS;
	return MLN_OK;
}
