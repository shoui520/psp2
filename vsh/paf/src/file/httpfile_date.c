#include "httpfile.h"
#include "../rtc/rtc.h"
#include <rtc.h>

static const char httpDateMonths[] =
	"JanuaryFebruaryMarchAprilMayJuneJulyAugustSeptemberOctoberNovemberDecember";
static const uint8_t httpDateMonthLengths[] = {
	7, 8, 5, 5, 3, 4, 4, 6, 9, 7, 8, 8
};
static const char httpDateWeekdays[] =
	"SundayMondayTuesdayWednesdayThursdayFridaySaturday";
static const uint8_t httpDateWeekdayLengths[] = { 6, 6, 7, 9, 8, 6, 8 };

typedef struct HttpDateTimeZone {
	const char *name;
	uint8_t length;
	int16_t minutes;
} HttpDateTimeZone;

#define HTTP_TIME_ZONE(name, minutes) { name, sizeof(name) - 1, minutes }

static const HttpDateTimeZone httpDateTimeZones[] = {
	HTTP_TIME_ZONE("GMT", 0),
	HTTP_TIME_ZONE("EST", -300),
	HTTP_TIME_ZONE("EDT", -240),
	HTTP_TIME_ZONE("CST", -360),
	HTTP_TIME_ZONE("CDT", -300),
	HTTP_TIME_ZONE("MST", -420),
	HTTP_TIME_ZONE("MDT", -360),
	HTTP_TIME_ZONE("PST", -480),
	HTTP_TIME_ZONE("PDT", -420),
	HTTP_TIME_ZONE("NZDT", 780),
	HTTP_TIME_ZONE("NZST", 720),
	HTTP_TIME_ZONE("IDLE", 720),
	HTTP_TIME_ZONE("NZT", 720),
	HTTP_TIME_ZONE("AESST", 660),
	HTTP_TIME_ZONE("ACSST", 630),
	HTTP_TIME_ZONE("CADT", 630),
	HTTP_TIME_ZONE("SADT", 630),
	HTTP_TIME_ZONE("AEST", 600),
	HTTP_TIME_ZONE("EAST", 600),
	HTTP_TIME_ZONE("GST", 600),
	HTTP_TIME_ZONE("LIGT", 600),
	HTTP_TIME_ZONE("ACST", 570),
	HTTP_TIME_ZONE("SAST", 570),
	HTTP_TIME_ZONE("CAST", 570),
	HTTP_TIME_ZONE("AWSST", 540),
	HTTP_TIME_ZONE("JST", 540),
	HTTP_TIME_ZONE("KST", 540),
	HTTP_TIME_ZONE("WDT", 540),
	HTTP_TIME_ZONE("MT", 510),
	HTTP_TIME_ZONE("AWST", 480),
	HTTP_TIME_ZONE("CCT", 480),
	HTTP_TIME_ZONE("WADT", 480),
	HTTP_TIME_ZONE("WST", 480),
	HTTP_TIME_ZONE("JT", 450),
	HTTP_TIME_ZONE("WAST", 420),
	HTTP_TIME_ZONE("IT", 210),
	HTTP_TIME_ZONE("BT", 180),
	HTTP_TIME_ZONE("EETDST", 180),
	HTTP_TIME_ZONE("EET", 120),
	HTTP_TIME_ZONE("CETDST", 120),
	HTTP_TIME_ZONE("FWT", 120),
	HTTP_TIME_ZONE("IST", 120),
	HTTP_TIME_ZONE("MEST", 120),
	HTTP_TIME_ZONE("METDST", 120),
	HTTP_TIME_ZONE("SST", 120),
	HTTP_TIME_ZONE("BST", 60),
	HTTP_TIME_ZONE("CET", 60),
	HTTP_TIME_ZONE("DNT", 60),
	HTTP_TIME_ZONE("FST", 60),
	HTTP_TIME_ZONE("MET", 60),
	HTTP_TIME_ZONE("MEWT", 60),
	HTTP_TIME_ZONE("MEZ", 60),
	HTTP_TIME_ZONE("NOR", 60),
	HTTP_TIME_ZONE("SET", 60),
	HTTP_TIME_ZONE("SWT", 60),
	HTTP_TIME_ZONE("WETDST", 60),
	HTTP_TIME_ZONE("WET", 0),
	HTTP_TIME_ZONE("WAT", -60),
	HTTP_TIME_ZONE("NDT", -90),
	HTTP_TIME_ZONE("ADT", -180),
	HTTP_TIME_ZONE("NFT", -150),
	HTTP_TIME_ZONE("NST", -150),
	HTTP_TIME_ZONE("AST", -240),
	HTTP_TIME_ZONE("YDT", -480),
	HTTP_TIME_ZONE("HDT", -540),
	HTTP_TIME_ZONE("YST", -540),
	HTTP_TIME_ZONE("AHST", -600),
	HTTP_TIME_ZONE("CAT", -600),
	HTTP_TIME_ZONE("NT", -660),
	HTTP_TIME_ZONE("IDLW", -720),
};

#undef HTTP_TIME_ZONE

static inline __attribute__((always_inline))
int HttpDateMatch(const char *left, const char *right, int length)
{
	while (length--) {
		if ((*left++ & 0xdf) != (*right++ & 0xdf))
			return 0;
	}
	return 1;
}

const char *HttpFileParseDateMonth(const char *text, int32_t *month)
{
	const char *name = httpDateMonths;
	int i;

	for (i = 0; i < 12; ++i) {
		int length = httpDateMonthLengths[i];

		if (HttpDateMatch(name, text, length)) {
			*month = i + 1;
			return text + length;
		}
		if (HttpDateMatch(name, text, 3)) {
			*month = i + 1;
			return text + 3;
		}
		name += length;
	}
	return NULL;
}

static inline __attribute__((always_inline))
int HttpDateIsDigit(int ch)
{
	return ch >= '0' && ch <= '9';
}

static inline __attribute__((always_inline))
const char *HttpDateReadNumber(const char *text, int32_t *number)
{
	int first = (signed char)text[0];
	int second = (signed char)text[1];

	if (!HttpDateIsDigit(first)) {
		*number = -1;
		return NULL;
	}
	if (HttpDateIsDigit(second)) {
		*number = 10 * first + second - 11 * '0';
		return text + 2;
	}
	*number = first - '0';
	return text + 1;
}

static inline __attribute__((always_inline))
int32_t HttpDateReadYear(const char *text)
{
	int a = (signed char)text[0], b = (signed char)text[1];
	int c = (signed char)text[2], d = (signed char)text[3];

	if (!HttpDateIsDigit(a) || !HttpDateIsDigit(b) ||
	    !HttpDateIsDigit(c) || !HttpDateIsDigit(d))
		return -1;
	return 1000 * a + 100 * b + 10 * c + d - 1111 * '0';
}

int32_t HttpFileParseDateTick(RtcTick *tick, const char *text)
{
	RtcDateTime date;
	const char *name = httpDateWeekdays;
	const char *next = NULL;
	int32_t number, offset = 0;
	int i;

	while (*text == ' ' || *text == '\t')
		++text;
	for (i = 0; i < 7; ++i) {
		int length = httpDateWeekdayLengths[i];

		if (HttpDateMatch(name, text, length)) {
			next = text + length;
			break;
		}
		if (HttpDateMatch(name, text, 3)) {
			next = text + 3;
			break;
		}
		name += length;
	}
	if (!next)
		return SCE_RTC_ERROR_BAD_PARSE;
	if (*next == ',')
		++next;
	while (*next == ' ' || *next == '\t')
		++next;
	text = next;
	next = HttpFileParseDateMonth(text, &number);
	if (next) {
		date.month = number;
		if (*next != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		if (next[1] == ' ') {
			number = (signed char)next[2] - '0';
			if (number > 9)
				return SCE_RTC_ERROR_BAD_PARSE;
			next += 3;
		} else {
			next = HttpDateReadNumber(next + 1, &number);
			if (!next)
				return SCE_RTC_ERROR_BAD_PARSE;
		}
		date.day = number;
		if (*next != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (!next)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.hour = number;
		if (*next != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (!next)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.minute = number;
		if (*next != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (!next)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.second = number;
		if (*next != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		number = HttpDateReadYear(next + 1);
		if (number < 0)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.year = number;
	} else {
		next = HttpDateReadNumber(text, &number);
		if (!next)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.day = number;
		if (*next != ' ' && *next != '-')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpFileParseDateMonth(next + 1, &number);
		if (!next)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.month = number;
		if (*next != ' ' && *next != '-')
			return SCE_RTC_ERROR_BAD_PARSE;
		++next;
		number = HttpDateReadYear(next);
		if (number < 0) {
			int a = (signed char)next[0], b = (signed char)next[1];

			if (!HttpDateIsDigit(a) || !HttpDateIsDigit(b))
				return SCE_RTC_ERROR_BAD_PARSE;
			number = 10 * a + b - 11 * '0';
			number += number < 50 ? 2000 : 1900;
			next += 2;
		} else {
			next += 4;
		}
		date.year = number;
		if (*next != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (!next || number > 25)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.hour = number;
		if (*next != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (!next)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.minute = number;
		if (*next == ' ') {
			date.second = 0;
			++next;
		} else {
			if (*next != ':')
				return SCE_RTC_ERROR_BAD_PARSE;
			next = HttpDateReadNumber(next + 1, &number);
			if (!next)
				return SCE_RTC_ERROR_BAD_PARSE;
			date.second = number;
		}
		if (*next == ' ') {
			int ch = (signed char)next[1];

			if (ch == '+' || ch == '-') {
				number = HttpDateReadYear(next + 2);
				offset = number % 60 + 60 * (number / 100);
				if (ch == '-')
					offset = -offset;
			} else if (ch != 'U' && next[2] != 'T') {
				for (i = 0; i < 70; ++i) {
					const HttpDateTimeZone *zone = &httpDateTimeZones[i];

					if (HttpDateMatch(zone->name, next + 1, zone->length)) {
						offset = zone->minutes;
						break;
					}
				}
				if (i == 70) {
					ch = (signed char)(next[1] & 0xdf);
					if ((ch >= 'A' && ch <= 'I') ||
					    (ch >= 'K' && ch <= 'M'))
						offset = 60 * (ch - 'A');
					else if (ch >= 'N' && ch <= 'Y')
						offset = 60 * ('N' - ch);
					else if (ch != 'Z')
						return SCE_RTC_ERROR_BAD_PARSE;
				}
			}
		}
	}
	date.microsecond = 0;
	RtcConvertDateTimeToTick(&date, tick);
	RtcTickAddMinutes(tick, tick, -offset);
	return 0;
}

void HttpFileParseDate(DateTime *result, const char *text)
{
	RtcTick ignored, tick;
	RtcDateTime date;

	HttpFileParseDateTick(&ignored, text);
	HttpFileParseDateTick(&tick, text);
	RtcConvertTickToDateTime(&tick, &date, NULL);
	DateTimeConstruct(result, date.year, date.month, date.day, date.hour,
			  date.minute, date.second, date.microsecond);
}
