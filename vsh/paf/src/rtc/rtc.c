#include "rtc.h"
#include <rtc.h>

extern int32_t sceRtcGetTick(const SceDateTime *time, SceRtcTick *tick);
extern int32_t sceRtcSetTick(SceDateTime *time, const SceRtcTick *tick);
extern int32_t sceRtcGetDayOfWeek(int32_t year, int32_t month, int32_t day);
extern int32_t sceRtcIsLeapYear(int32_t year);
extern int32_t sceRegMgrUtilityGetInt(uint32_t id, int32_t *value);
extern void *sce_paf_memset(void *dst, int ch, size_t len);

int32_t RtcConvertDateTimeToTick(const RtcDateTime *src, RtcTick *tick)
{
	SceDateTime time;
	SceRtcTick value;
	int32_t ret;

	time.year = src->year;
	time.month = src->month;
	time.day = src->day;
	time.hour = src->hour;
	time.minute = src->minute;
	time.second = src->second;
	time.microsecond = src->microsecond;
	ret = sceRtcGetTick(&time, &value);
	if (tick)
		*tick = value.tick;
	return ret;
}

int32_t RtcConvertTickToDateTime(const RtcTick *tick, RtcDateTime *dst,
			       RtcDateTimeEx *exdst)
{
	SceRtcTick value;
	SceDateTime time;
	int32_t ret;

	value.tick = *tick;
	ret = sceRtcSetTick(&time, &value);
	if (ret) {
		dst->year = 2010;
		dst->month = 1;
		dst->day = 1;
		dst->hour = 0;
		dst->minute = 0;
		dst->second = 0;
		dst->microsecond = 0;
	} else {
		dst->year = time.year;
		dst->month = time.month;
		dst->day = time.day;
		dst->hour = time.hour;
		dst->minute = time.minute;
		dst->second = time.second;
		dst->microsecond = time.microsecond;
	}
	if (exdst && !ret) {
		int32_t month, adjusted_month, year, day, yearday;
		int32_t first_setting, second_setting;
		uint16_t first = 0, second = 0;

		sce_paf_memset(exdst, 0, sizeof(*exdst));
		exdst->weekday = sceRtcGetDayOfWeek(dst->year, dst->month, dst->day);
		month = dst->month;
		adjusted_month = month <= 2 ? month + 12 : month;
		year = dst->year;
		day = dst->day;
		yearday = 979 * adjusted_month - 1033;
		yearday = day + ((yearday + ((uint32_t)(yearday >> 5) >> 27)) >> 5) - 1;
		if (month >= 3) {
			int32_t leap = sceRtcIsLeapYear(year);

			if (leap >= 0)
				yearday += leap;
		}
		exdst->yearday = yearday;
		if (sceRegMgrUtilityGetInt(0x00760538, &first_setting) >= 0)
			first = first_setting;
		exdst->registry_values[0] = first;
		if (sceRegMgrUtilityGetInt(0x00668503, &second_setting) >= 0)
			second = second_setting;
		exdst->registry_values[1] = second;
	}
	return ret;
}

int32_t RtcTickAddMinutes(RtcTick *dst, const RtcTick *src, int64_t minutes)
{
	*dst = *src + (uint64_t)minutes * 60000000;
	return 0;
}
