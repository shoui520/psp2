/*
 * SceRtc text forms of a tick: RFC 2822 and RFC 3339 output, and parsing
 * of RFC 3339, RFC 2822 and asctime() dates (rtc/rtc_parse.h, shared with
 * the user library).
 */

#include "rtc_internal.h"

#define sceRtcParseRFC3339	sceRtcParseRFC3339
#define sceRtcParseDateTime	sceRtcParseDateTime

int rtcGetTickUnchecked(const SceDateTime *time, SceRtcTick *tick);

#include <rtc/rtc_parse.h>

static const char s_rtcDayAbbr[] = "SunMonTueWedThuFriSat";
static const char s_rtcMonthAbbr[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

static inline char rtcDigit(unsigned int value)
{
	return value % 10 + '0';
}

int sceRtcFormatRFC2822(char *buf, const SceRtcTick *tick, int offset)
{
	SceRtcTick local;
	SceDateTime time;
	const char *name;
	char *p;
	int wday;

	if (buf == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	if (tick == NULL)
		sceRtcGetCurrentTick(&local);
	else
		local = *tick;
	if ((unsigned int)(offset + 1439) > 2878)
		return SCE_RTC_ERROR_INVALID_VALUE;
	sceRtcTickAddMinutes(&local, &local, offset);
	if ((SceInt64)local.tick < 0)
		return SCE_RTC_ERROR_INVALID_VALUE;
	sceRtcSetTick(&time, &local);
	wday = sceRtcGetDayOfWeek(time.year, time.month, time.day);
	name = &s_rtcDayAbbr[wday * 3];
	buf[0] = name[0];
	buf[1] = name[1];
	buf[2] = name[2];
	buf[3] = ',';
	buf[4] = ' ';
	buf[5] = time.day < 10 ? '0' : time.day / 10 + '0';
	buf[6] = time.day % 10 + '0';
	buf[7] = ' ';
	name = &s_rtcMonthAbbr[(time.month - 1) * 3];
	buf[8] = name[0];
	buf[9] = name[1];
	buf[10] = name[2];
	buf[11] = ' ';
	buf[12] = rtcDigit(time.year / 1000);
	buf[13] = rtcDigit(time.year / 100);
	buf[14] = rtcDigit(time.year / 10);
	buf[15] = rtcDigit(time.year);
	buf[16] = ' ';
	buf[17] = rtcDigit(time.hour / 10);
	buf[18] = rtcDigit(time.hour);
	buf[19] = ':';
	buf[20] = rtcDigit(time.minute / 10);
	buf[21] = rtcDigit(time.minute);
	buf[22] = ':';
	buf[23] = rtcDigit(time.second / 10);
	buf[24] = rtcDigit(time.second);
	buf[25] = ' ';
	if (offset < 0) {
		buf[26] = '-';
		offset = -offset;
	} else {
		buf[26] = '+';
	}
	p = buf + 27;
	p[0] = (offset / 60 / 10) % 10 + '0';
	p[1] = (offset / 60) % 10 + '0';
	p[2] = (offset % 60 / 10) % 10 + '0';
	p[3] = (offset % 60) % 10 + '0';
	p[4] = '\0';
	return 0;
}

int sceRtcFormatRFC2822LocalTime(char *buf, const SceRtcTick *tick)
{
	if (buf == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	return sceRtcFormatRFC2822(buf, tick, g_rtc.timezone + g_rtc.daylightSaving);
}

int sceRtcFormatRFC3339(char *buf, const SceRtcTick *tick, int offset)
{
	SceRtcTick local;
	SceDateTime time;
	char *p;

	if (buf == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	if (tick == NULL)
		sceRtcGetCurrentTick(&local);
	else
		local = *tick;
	if ((unsigned int)(offset + 1439) > 2878)
		return SCE_RTC_ERROR_INVALID_VALUE;
	sceRtcTickAddMinutes(&local, &local, offset);
	if ((SceInt64)local.tick < 0)
		return SCE_RTC_ERROR_INVALID_VALUE;
	sceRtcSetTick(&time, &local);
	buf[0] = rtcDigit(time.year / 1000);
	buf[1] = rtcDigit(time.year / 100);
	buf[2] = rtcDigit(time.year / 10);
	buf[3] = rtcDigit(time.year);
	buf[4] = '-';
	buf[5] = rtcDigit(time.month / 10);
	buf[6] = rtcDigit(time.month);
	buf[7] = '-';
	buf[8] = rtcDigit(time.day / 10);
	buf[9] = rtcDigit(time.day);
	buf[10] = 'T';
	buf[11] = rtcDigit(time.hour / 10);
	buf[12] = rtcDigit(time.hour);
	buf[13] = ':';
	buf[14] = rtcDigit(time.minute / 10);
	buf[15] = rtcDigit(time.minute);
	buf[16] = ':';
	buf[17] = rtcDigit(time.second / 10);
	buf[18] = rtcDigit(time.second);
	buf[19] = '.';
	buf[20] = rtcDigit(time.microsecond / 10000 / 10);
	buf[21] = rtcDigit(time.microsecond / 10000);
	if (offset == 0) {
		buf[22] = 'Z';
		p = buf + 23;
	} else {
		if (offset < 0) {
			buf[22] = '-';
			offset = -offset;
		} else {
			buf[22] = '+';
		}
		buf[23] = (offset / 60 / 10) % 10 + '0';
		buf[24] = (offset / 60) % 10 + '0';
		buf[25] = ':';
		buf[26] = (offset % 60 / 10) % 10 + '0';
		buf[27] = (offset % 60) % 10 + '0';
		p = buf + 28;
	}
	*p = '\0';
	return 0;
}

int sceRtcFormatRFC3339LocalTime(char *buf, const SceRtcTick *tick)
{
	if (buf == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	return sceRtcFormatRFC3339(buf, tick, g_rtc.timezone + g_rtc.daylightSaving);
}
