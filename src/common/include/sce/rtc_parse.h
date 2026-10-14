/*
 * SceRtc: parsing of RFC 3339, RFC 2822 and asctime() dates into ticks.
 * Sony built the same source into the kernel (os0:kd/rtc.skprx) and the user
 * library (SceRtcUser in os0:us/driver_us.suprx).
 *
 * The day, month and zone names are kept as one string each with a table of
 * lengths (and, for the zones, offsets) beside it.
 *
 * This header holds function definitions: include it from exactly one C file
 * of a module. The functions carry the user library's names; a module whose
 * exports are named otherwise maps them with #define before including (the
 * kernel exports them by NID). The includer declares sceRtcCheckValid(),
 * sceRtcTickAddMinutes() and rtcGetTickUnchecked().
 */

#pragma once

#include <psp2common/kernel/rtc.h>

#define IS_DIGIT(c)	((unsigned int)((c) - '0') < 10)
#define UPPER(c)	((c) & 0xdf)

#define RTC_ZONES	70

typedef struct RtcZone {
	SceUInt8 nameOffset;	/* in s_rtcZoneNames */
	SceUInt8 nameLength;
	SceInt16 minutes;	/* east of UTC */
} RtcZone;

static const char s_rtcZoneNames[] =
	"GMT" "EST" "EDT" "CST" "CDT" "MST" "MDT" "PST" "PDT" "NZDT" "NZST" "IDLE"
	"NZT" "AESST" "ACSST" "CADT" "SADT" "AEST" "EAST" "GST" "LIGT" "ACST" "SAST"
	"CAST" "AWSST" "JST" "KST" "WDT" "MT" "AWST" "CCT" "WADT" "WST" "JT" "WAST"
	"IT" "BT" "EETDST" "EET" "CETDST" "FWT" "IST" "MEST" "METDST" "SST" "BST"
	"CET" "DNT" "FST" "MET" "MEWT" "MEZ" "NOR" "SET" "SWT" "WETDST" "WET" "WAT"
	"NDT" "ADT" "NFT" "NST" "AST" "YDT" "HDT" "YST" "AHST" "CAT" "NT" "IDLW";

static const char s_rtcDayNames[] =
	"Sunday" "Monday" "Tuesday" "Wednesday" "Thursday" "Friday" "Saturday";

static const char s_rtcMonthNames[] =
	"January" "February" "March" "April" "May" "June" "July" "August"
	"September" "October" "November" "December";

static const RtcZone s_rtcZones[RTC_ZONES] = {
	{   0, 3,    0 }, {   3, 3, -300 }, {   6, 3, -240 }, {   9, 3, -360 },
	{  12, 3, -300 }, {  15, 3, -420 }, {  18, 3, -360 }, {  21, 3, -480 },
	{  24, 3, -420 }, {  27, 4,  780 }, {  31, 4,  720 }, {  35, 4,  720 },
	{  39, 3,  720 }, {  42, 5,  660 }, {  47, 5,  630 }, {  52, 4,  630 },
	{  56, 4,  630 }, {  60, 4,  600 }, {  64, 4,  600 }, {  68, 3,  600 },
	{  71, 4,  600 }, {  75, 4,  570 }, {  79, 4,  570 }, {  83, 4,  570 },
	{  87, 5,  540 }, {  92, 3,  540 }, {  95, 3,  540 }, {  98, 3,  540 },
	{ 101, 2,  510 }, { 103, 4,  480 }, { 107, 3,  480 }, { 110, 4,  480 },
	{ 114, 3,  480 }, { 117, 2,  450 }, { 119, 4,  420 }, { 123, 2,  210 },
	{ 125, 2,  180 }, { 127, 6,  180 }, { 133, 3,  120 }, { 136, 6,  120 },
	{ 142, 3,  120 }, { 145, 3,  120 }, { 148, 4,  120 }, { 152, 6,  120 },
	{ 158, 3,  120 }, { 161, 3,   60 }, { 164, 3,   60 }, { 167, 3,   60 },
	{ 170, 3,   60 }, { 173, 3,   60 }, { 176, 4,   60 }, { 180, 3,   60 },
	{ 183, 3,   60 }, { 186, 3,   60 }, { 189, 3,   60 }, { 192, 6,   60 },
	{ 198, 3,    0 }, { 201, 3,  -60 }, { 204, 3,  -90 }, { 207, 3, -180 },
	{ 210, 3, -150 }, { 213, 3, -150 }, { 216, 3, -240 }, { 219, 3, -480 },
	{ 222, 3, -540 }, { 225, 3, -540 }, { 228, 4, -600 }, { 232, 3, -600 },
	{ 235, 2, -660 }, { 237, 4, -720 },
};

static const SceUInt8 s_rtcDayNameLength[8] = { 6, 6, 7, 9, 8, 6, 8 };
static const SceUInt8 s_rtcMonthNameLength[12] = { 7, 8, 5, 5, 3, 4, 4, 6, 9, 7, 8, 8 };

/* name unknown: case-insensitive compare of the first n characters */
static inline int rtcMatch(const char *name, const char *s, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		if (UPPER(name[i]) != UPPER(s[i]))
			return 0;
	}
	return 1;
}

/* name unknown: one or two digits */
static __attribute__((noinline)) const char *rtcParse2Digits(const char *s, int *value)
{
	if (!IS_DIGIT(s[0])) {
		*value = -1;
		return NULL;
	}
	if (IS_DIGIT(s[1])) {
		*value = (s[0] - '0') * 10 + s[1] - '0';
		return s + 2;
	}
	*value = s[0] - '0';
	return s + 1;
}

int sceRtcParseRFC3339(SceRtcTick *tick, const char *str)
{
	SceDateTime time;
	const char *p;
	unsigned int scale;
	int offset, hours, minutes, ret;

	if (tick == NULL || str == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	if (!IS_DIGIT(str[0]) || !IS_DIGIT(str[1]) || !IS_DIGIT(str[2]) || !IS_DIGIT(str[3]))
		return SCE_RTC_ERROR_BAD_PARSE;
	time.year = (str[0] - '0') * 1000 + (str[1] - '0') * 100 + (str[2] - '0') * 10 + str[3] - '0';
	if (str[4] != '-' || !IS_DIGIT(str[5]) || !IS_DIGIT(str[6]))
		return SCE_RTC_ERROR_BAD_PARSE;
	time.month = (str[5] - '0') * 10 + str[6] - '0';
	if (str[7] != '-' || !IS_DIGIT(str[8]) || !IS_DIGIT(str[9]))
		return SCE_RTC_ERROR_BAD_PARSE;
	time.day = (str[8] - '0') * 10 + str[9] - '0';
	if (UPPER(str[10]) != 'T' || !IS_DIGIT(str[11]) || !IS_DIGIT(str[12]))
		return SCE_RTC_ERROR_BAD_PARSE;
	time.hour = (str[11] - '0') * 10 + str[12] - '0';
	if (str[13] != ':' || !IS_DIGIT(str[14]) || !IS_DIGIT(str[15]))
		return SCE_RTC_ERROR_BAD_PARSE;
	time.minute = (str[14] - '0') * 10 + str[15] - '0';
	if (str[16] != ':' || !IS_DIGIT(str[17]) || !IS_DIGIT(str[18]))
		return SCE_RTC_ERROR_BAD_PARSE;
	time.second = (str[17] - '0') * 10 + str[18] - '0';
	p = str + 19;
	time.microsecond = 0;
	if (*p == '.') {
		p++;
		scale = 100000;
		while (IS_DIGIT(*p)) {
			time.microsecond += (*p - '0') * scale;
			scale /= 10;
			p++;
		}
	}
	if (UPPER(*p) == 'Z') {
		offset = 0;
	} else {
		if (*p != '-' && *p != '+')
			return SCE_RTC_ERROR_BAD_PARSE;
		hours = IS_DIGIT(p[1]) && IS_DIGIT(p[2]) ? (p[1] - '0') * 10 + p[2] - '0' : -1;
		minutes = IS_DIGIT(p[4]) && IS_DIGIT(p[5]) ? (p[4] - '0') * 10 + p[5] - '0' : -1;
		if (hours == -1 || p[3] != ':' || minutes == -1)
			return SCE_RTC_ERROR_BAD_PARSE;
		offset = hours * 60 + minutes;
		if (*p == '-')
			offset = -offset;
	}
	ret = sceRtcCheckValid(&time);
	if (ret == SCE_RTC_ERROR_INVALID_SECOND) {
		if (time.second != 60)	/* a leap second is accepted */
			return ret;
	} else if (ret < 0) {
		return ret;
	}
	rtcGetTickUnchecked(&time, tick);
	sceRtcTickAddMinutes(tick, tick, -offset);
	return 0;
}

/* name unknown: a zone name; the end of it, or NULL if none is there */
static inline const char *rtcParseZoneName(const char *s, int *minutes)
{
	int i;

	for (i = 0; i < RTC_ZONES; i++) {
		if (rtcMatch(&s_rtcZoneNames[s_rtcZones[i].nameOffset], s, s_rtcZones[i].nameLength)) {
			*minutes = s_rtcZones[i].minutes;
			return s + s_rtcZones[i].nameLength;
		}
	}
	return NULL;
}

/* name unknown: a day or month name, whole or abbreviated; the end of it, or NULL */
static inline const char *rtcParseName(const char *s, const char *names, const SceUInt8 *length,
				       int count, int *index)
{
	int i;

	for (i = 0; i < count; i++) {
		if (rtcMatch(names, s, length[i])) {
			*index = i;
			return s + length[i];
		}
		if (rtcMatch(names, s, 3)) {
			*index = i;
			return s + 3;
		}
		names += length[i];
	}
	return NULL;
}

int sceRtcParseDateTime(SceRtcTick *tick, const char *str)
{
	const char *p = str;
	const char *end;
	SceDateTime time;
	unsigned int c;
	int value, offset, month;

	if (tick == NULL || str == NULL)
		return SCE_RTC_ERROR_INVALID_POINTER;
	while (*p == ' ' || *p == '\t')
		p++;
	if (IS_DIGIT(p[0]) && IS_DIGIT(p[1]) && IS_DIGIT(p[2]) && IS_DIGIT(p[3]))
		return sceRtcParseRFC3339(tick, p);

	/* day of the week */
	p = rtcParseName(p, s_rtcDayNames, s_rtcDayNameLength, 7, &value);
	if (p == NULL)
		return SCE_RTC_ERROR_BAD_PARSE;
	if (*p == ',')
		p++;
	while (*p == ' ' || *p == '\t')
		p++;

	offset = 0;
	end = rtcParseName(p, s_rtcMonthNames, s_rtcMonthNameLength, 12, &month);
	if (end != NULL) {
		/* asctime(): "Www Mmm dd hh:mm:ss yyyy" */
		p = end;
		time.month = month + 1;
		if (*p != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		if (p[1] == ' ') {
			value = p[2] - '0';
			if (value >= 10)
				return SCE_RTC_ERROR_BAD_PARSE;
			p += 3;
		} else {
			if (!IS_DIGIT(p[1]))
				return SCE_RTC_ERROR_BAD_PARSE;
			if (IS_DIGIT(p[2])) {
				value = (p[1] - '0') * 10 + p[2] - '0';
				p += 3;
			} else {
				value = p[1] - '0';
				p += 2;
			}
		}
		time.day = value;
		if (*p != ' ' || !IS_DIGIT(p[1]))
			return SCE_RTC_ERROR_BAD_PARSE;
		if (IS_DIGIT(p[2])) {
			value = (p[1] - '0') * 10 + p[2] - '0';
			p += 3;
		} else {
			value = p[1] - '0';
			p += 2;
		}
		time.hour = value;
		if (*p != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		p = rtcParse2Digits(p + 1, &value);
		if (p == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		time.minute = value;
		if (*p != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		p = rtcParse2Digits(p + 1, &value);
		if (p == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		time.second = value;
		if (*p != ' ' || !IS_DIGIT(p[1]) || !IS_DIGIT(p[2]) || !IS_DIGIT(p[3]) ||
		    !IS_DIGIT(p[4]))
			return SCE_RTC_ERROR_BAD_PARSE;
		time.year = (p[1] - '0') * 1000 + (p[2] - '0') * 100 + (p[3] - '0') * 10 + p[4] - '0';
		goto done;
	}

	/* RFC 2822: "dd Mmm yyyy hh:mm[:ss] [zone]" */
	if (!IS_DIGIT(*p))
		return SCE_RTC_ERROR_BAD_PARSE;
	if (IS_DIGIT(p[1])) {
		value = (p[0] - '0') * 10 + p[1] - '0';
		p += 2;
	} else {
		value = p[0] - '0';
		p += 1;
	}
	if (*p != '-' && *p != ' ')
		return SCE_RTC_ERROR_BAD_PARSE;
	time.day = value;
	p = rtcParseName(p + 1, s_rtcMonthNames, s_rtcMonthNameLength, 12, &month);
	if (p == NULL)
		return SCE_RTC_ERROR_BAD_PARSE;
	time.month = month + 1;
	if ((*p != '-' && *p != ' ') || !IS_DIGIT(p[1]) || !IS_DIGIT(p[2]))
		return SCE_RTC_ERROR_BAD_PARSE;
	if (IS_DIGIT(p[3]) && IS_DIGIT(p[4])) {
		time.year = (p[1] - '0') * 1000 + (p[2] - '0') * 100 + (p[3] - '0') * 10 + p[4] - '0';
		p += 5;
	} else {
		value = (p[1] - '0') * 10 + p[2] - '0';
		time.year = value < 50 ? value + 2000 : value + 1900;
		p += 3;
	}
	if (*p != ' ')
		return SCE_RTC_ERROR_BAD_PARSE;
	p = rtcParse2Digits(p + 1, &value);
	if (p == NULL || value >= 26)
		return SCE_RTC_ERROR_BAD_PARSE;
	time.hour = value;
	if (*p != ':')
		return SCE_RTC_ERROR_BAD_PARSE;
	p = rtcParse2Digits(p + 1, &value);
	if (p == NULL)
		return SCE_RTC_ERROR_BAD_PARSE;
	time.minute = value;
	if (*p == ' ') {
		time.second = 0;
		p++;
	} else {
		if (*p != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		p = rtcParse2Digits(p + 1, &value);
		if (p == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		time.second = value;
	}
	if (*p != ' ')
		goto done;
	if (p[1] == '+' || p[1] == '-') {
		value = IS_DIGIT(p[2]) && IS_DIGIT(p[3]) ? (p[2] - '0') * 10 + p[3] - '0' : -1;
		if (!IS_DIGIT(p[4]) || !IS_DIGIT(p[5]) || value < 0)
			goto done;
		offset = (p[4] - '0') * 10 + p[5] - '0' + value * 60;
		if (p[1] != '-')
			offset = -offset;
	} else if (p[1] != 'U' && p[2] != 'T') {
		if (rtcParseZoneName(p + 1, &value) != NULL) {
			offset = -value;
		} else {
			/* a military zone letter */
			c = UPPER((unsigned char)p[1]);
			if (c - 'A' < 9 || c - 'K' < 3)
				offset = -(int)(c - 'A') * 60;
			else if (c - 'N' <= 11)
				offset = -(int)('N' - c) * 60;
			else if (c != 'Z')
				return SCE_RTC_ERROR_BAD_PARSE;
		}
	}

done:
	time.microsecond = 0;
	rtcGetTickUnchecked(&time, tick);
	sceRtcTickAddMinutes(tick, tick, offset);
	return 0;
}
