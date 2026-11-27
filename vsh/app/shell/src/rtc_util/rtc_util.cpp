/*
 * rtc_util.cpp: timezone table, automatic daylight-saving-time rules,
 * /CONFIG/DATE registry watcher (SceRtcUtilRegistryCallback), and SetConfJob
 * (0x81365414..0x81365BFB).
 */
#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <kernel/threadmgr.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include "../../shell.h"

#define RTC_UTIL_TIMEZONE_COUNT		122U
#define RTC_UTIL_DEFAULT_TIMEZONE	29U
#define RTC_UTIL_DST_EXCEPTION_COUNT	2U
#define RTC_UTIL_DST_SKIP_RULE_COUNT	8U
#define RTC_UTIL_REG_KEY_COUNT		5U

struct RtcDstTransitionSpec {
	SceUInt16 year;		/* 0 = recurring every year */
	SceUInt16 month;
	SceUInt16 day;		/* 0 = nth/last weekday */
	SceUInt16 hour;
	SceUInt16 minute;
	SceUInt16 weekIndex;	/* 0 = last occurrence in month, 1..5 = nth */
	SceUInt16 dayOfWeek;	/* 0 = Sunday .. 6 = Saturday */
};

struct RtcTimeZoneEntry {
	SceInt16 utcOffsetMinutes;
	SceUInt16 hasDst;
	RtcDstTransitionSpec dstStart;
	RtcDstTransitionSpec dstEnd;
};

struct RtcDstExceptionEntry {
	SceInt32 timeZoneId;
	SceUInt16 hasDst;
	RtcDstTransitionSpec dstStart;
	RtcDstTransitionSpec dstEnd;
	SceUInt16 alignmentPad;
};

struct RtcDstSkipRule {
	SceUInt32 timeZoneId;
	RtcDstTransitionSpec skipStart;
	RtcDstTransitionSpec skipEnd;
};

struct RtcDateTimeSystemConf {
	int dateFormat;
	int timeFormat;
	int timeZone;
	int utcOffsetMinutes;
	int summerTime;
};

struct RtcUtilRegKeySpec {
	const char *fullPath;
	const char *keyName;
	int *target;
};


extern "C" {
extern int sceRegMgrGetKeyInt(const char *category, const char *name, int *outVal);
extern int sceRegMgrSetKeyInt(const char *category, const char *name, int val);
extern int sceRegMgrRegisterCallback(SceUID cbId);
extern int sceRegMgrAddRegistryCallback(SceUID cbId, const char *path);
extern int sceRegMgrStartCallback(SceUID cbId);
extern int vshAppMgrSystemParamDateTimeSetConf(const RtcDateTimeSystemConf *conf);
extern int vshRtcSetConf(int dateFormat, int isTwelveHour, int utcOffsetMinutes,
			 int dstOffsetMinutes);
}

/* 0x81547560..0x81547574: cached /CONFIG/DATE registry values and callback UID */
static int s_regDateFormat = -1;
static int s_regTimeFormat = -1;
static int s_regTimeZone = -1;
static int s_regAutoSummerTime = -1;
static int s_regSummerTime = -1;
static SceUID s_regCallbackId = -1;

/* 0x81547578 */
static const RtcUtilRegKeySpec s_rtcRegKeys[RTC_UTIL_REG_KEY_COUNT] = {
	{ "/CONFIG/DATE/date_format",      "date_format",      &s_regDateFormat },
	{ "/CONFIG/DATE/time_format",      "time_format",      &s_regTimeFormat },
	{ "/CONFIG/DATE/time_zone",        "time_zone",        &s_regTimeZone },
	{ "/CONFIG/DATE/auto_summer_time", "auto_summer_time", &s_regAutoSummerTime },
	{ "/CONFIG/DATE/summer_time",      "summer_time",      &s_regSummerTime },
};

/* 0x815475B4: 122 timezone entries (32 bytes each) */
static const RtcTimeZoneEntry s_timeZoneTable[RTC_UTIL_TIMEZONE_COUNT] = {
	{ -660, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -660, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -600, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -540, 1, { 0,  3,  0,  2, 0, 2, 0 }, { 0, 11,  0,  2, 0, 1, 0 } },
	{ -480, 1, { 0,  3,  0,  2, 0, 2, 0 }, { 0, 11,  0,  2, 0, 1, 0 } },
	{ -480, 1, { 0,  3,  0,  2, 0, 2, 0 }, { 0, 11,  0,  2, 0, 1, 0 } },
	{ -420, 1, { 0,  3,  0,  2, 0, 2, 0 }, { 0, 11,  0,  2, 0, 1, 0 } },
	{ -420, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -420, 1, { 0,  4,  0,  2, 0, 1, 0 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{ -360, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -360, 1, { 0,  3,  0,  2, 0, 2, 0 }, { 0, 11,  0,  2, 0, 1, 0 } },
	{ -360, 1, { 0,  4,  0,  2, 0, 1, 0 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{ -360, 1, { 0,  4,  0,  2, 0, 1, 0 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{ -360, 1, { 0,  4,  0,  2, 0, 1, 0 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{ -300, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -300, 1, { 0,  3,  0,  2, 0, 2, 0 }, { 0, 11,  0,  2, 0, 1, 0 } },
	{ -300, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -300, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -300, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -270, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -240, 1, { 0,  9,  0,  0, 0, 1, 0 }, { 0,  4,  0,  0, 0, 0, 0 } },
	{ -240, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -240, 1, { 0,  3,  0,  2, 0, 2, 0 }, { 0, 11,  0,  2, 0, 1, 0 } },
	{ -240, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -240, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -240, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -210, 1, { 0,  3,  0,  2, 0, 2, 0 }, { 0, 11,  0,  2, 0, 1, 0 } },
	{ -180, 1, { 0, 10,  0,  0, 0, 3, 0 }, { 0,  2,  0,  0, 0, 3, 0 } },
	{ -180, 1, { 0, 10,  0,  0, 0, 3, 0 }, { 0,  2,  0,  0, 0, 3, 0 } },
	{ -180, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{ -120, 1, { 0,  4,  0,  2, 0, 1, 0 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{ -120, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  -60, 1, { 0,  3,  0,  0, 0, 0, 0 }, { 0, 10,  0,  1, 0, 0, 0 } },
	{    0, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{    0, 1, { 0,  3,  0,  1, 0, 0, 0 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{    0, 1, { 0,  3,  0,  1, 0, 0, 0 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{    0, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{    0, 1, { 0,  3,  0,  1, 0, 0, 0 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  9,  0,  2, 0, 1, 0 }, { 0,  4,  0,  2, 0, 1, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{   60, 1, { 0,  3,  0,  2, 0, 0, 0 }, { 0, 10,  0,  3, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  0, 0, 0, 5 }, { 0, 10,  0,  1, 0, 0, 5 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  2, 0, 4, 5 }, { 0, 10,  0,  2, 0, 0, 0 } },
	{  120, 1, { 0,  4,  0,  0, 0, 0, 5 }, { 0,  9,  0,  0, 0, 0, 5 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  0, 0, 0, 5 }, { 0, 10,  0,  0, 0, 0, 5 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  0, 0, 0, 0 }, { 0, 10,  0,  0, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  180, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  120, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  120, 1, { 0,  3,  0,  3, 0, 0, 0 }, { 0, 10,  0,  4, 0, 0, 0 } },
	{  180, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  180, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  180, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  180, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  210, 1, { 0,  3, 22,  0, 0, 0, 0 }, { 0,  9, 22,  0, 0, 0, 0 } },
	{  180, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  240, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  240, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  240, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  240, 1, { 0,  3,  0,  4, 0, 0, 0 }, { 0, 10,  0,  5, 0, 0, 0 } },
	{  240, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  270, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  300, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  300, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  330, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  345, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  360, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  360, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  360, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  420, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  480, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  480, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  480, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  480, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  480, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  480, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  540, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  540, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  570, 1, { 0, 10,  0,  2, 0, 1, 0 }, { 0,  4,  0,  3, 0, 1, 0 } },
	{  570, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  600, 1, { 0, 10,  0,  2, 0, 1, 0 }, { 0,  4,  0,  3, 0, 1, 0 } },
	{  600, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  720, 1, { 0,  9,  0,  2, 0, 0, 0 }, { 0,  4,  0,  3, 0, 1, 0 } },
	{  720, 1, { 0, 10,  0,  2, 0, 4, 0 }, { 0,  1,  0,  2, 0, 4, 0 } },
	{  780, 1, { 0,  9,  0,  3, 0, 0, 0 }, { 0,  4,  0,  4, 0, 1, 0 } },
	{ -360, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
	{  660, 0, { 0,  0,  0,  0, 0, 0, 0 }, { 0,  0,  0,  0, 0, 0, 0 } },
};

/* 0x815484F4: year-specific Brazil DST override exceptions */
static const RtcDstExceptionEntry s_dstExceptions[RTC_UTIL_DST_EXCEPTION_COUNT] = {
	{ 27, 1, { 2014, 10, 19, 0, 0, 0, 0 }, { 2015, 2, 22, 0, 0, 0, 0 }, 0 },
	{ 28, 1, { 2014, 10, 19, 0, 0, 0, 0 }, { 2015, 2, 22, 0, 0, 0, 0 }, 0 },
};

/* 0x8154853C: Ramadan DST suspension windows (Morocco tz 33, Egypt tz 72) */
static const RtcDstSkipRule s_dstSkipRules[RTC_UTIL_DST_SKIP_RULE_COUNT] = {
	{ 33U, { 2014, 6, 28, 3, 0, 0, 0 }, { 2014, 8,  2, 2, 0, 0, 0 } },
	{ 33U, { 2015, 6, 13, 3, 0, 0, 0 }, { 2015, 7, 18, 2, 0, 0, 0 } },
	{ 33U, { 2016, 6,  4, 3, 0, 0, 0 }, { 2016, 7,  9, 2, 0, 0, 0 } },
	{ 33U, { 2017, 5, 27, 3, 0, 0, 0 }, { 2017, 7,  1, 2, 0, 0, 0 } },
	{ 72U, { 2014, 6, 27, 0, 0, 0, 0 }, { 2014, 8,  1, 0, 0, 0, 0 } },
	{ 72U, { 2015, 6, 18, 1, 0, 0, 0 }, { 2015, 7, 18, 0, 0, 0, 0 } },
	{ 72U, { 2016, 6,  7, 1, 0, 0, 0 }, { 2016, 7,  7, 0, 0, 0, 0 } },
	{ 72U, { 2017, 5, 27, 1, 0, 0, 0 }, { 2017, 6, 26, 0, 0, 0, 0 } },
};

/* 0x8154863C: registry time_zone value -> s_timeZoneTable index */
static const int s_timeZoneIndexMap[RTC_UTIL_TIMEZONE_COUNT] = {
	  0,   1,   2,   3,   4,   5,   6,   8, 120,  10,  12,  14,  15,  16,  17,  18,
	 19,  20,  22,  24,  25,  26,  27,  29,  32,  33,  34,  35,  36,  37,  38,  39,
	 40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,
	 56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,
	 72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,
	 90,  88,  89,  91,  92,  93,  94,  95,  96,  97,  98, 101,  99, 100, 102, 103,
	104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 115, 117, 118, 119,   7,   9,
	 11,  13,  21,  23,  28,  30,  31, 114, 116, 121
};

/* 0x81365B74 */
static __attribute__((noinline)) const RtcTimeZoneEntry *rtcUtilGetTimeZoneEntry(unsigned int regTimeZone)
{
	if (regTimeZone >= RTC_UTIL_TIMEZONE_COUNT)
		regTimeZone = RTC_UTIL_DEFAULT_TIMEZONE;
	return &s_timeZoneTable[s_timeZoneIndexMap[regTimeZone]];
}

/* 0x81365B98 */
static __attribute__((noinline)) const RtcDstExceptionEntry *rtcUtilGetDstException(unsigned int index, int regTimeZone)
{
	if (index >= RTC_UTIL_DST_EXCEPTION_COUNT)
		index = 0;
	if (s_dstExceptions[index].timeZoneId == s_timeZoneIndexMap[regTimeZone])
		return &s_dstExceptions[index];
	return NULL;
}

/* 0x81365BCC */
static __attribute__((noinline)) const RtcDstSkipRule *rtcUtilGetDstSkipRule(unsigned int index, int regTimeZone)
{
	if (index >= RTC_UTIL_DST_SKIP_RULE_COUNT)
		index = 0;
	if ((int)s_dstSkipRules[index].timeZoneId == s_timeZoneIndexMap[regTimeZone])
		return &s_dstSkipRules[index];
	return NULL;
}

/* 0x81365414: resolve a recurring or fixed transition rule to a concrete SceDateTime */
static int rtcUtilResolveTransitionDate(SceUInt16 targetYear,
					const RtcDstTransitionSpec *spec,
					SceDateTime *outDateTime)
{
	if (spec == NULL || outDateTime == NULL)
		return -1;

	SceUInt16 year = (spec->year != 0) ? spec->year : targetYear;
	outDateTime->year = year;
	outDateTime->month = spec->month;

	if (spec->day != 0) {
		outDateTime->day = spec->day;
	} else {
		int daysInMonth = sceRtcGetDaysInMonth(year, spec->month);
		if (spec->weekIndex != 0) {
			int matchCount = 1;
			for (int day = 1; day <= daysInMonth; ++day) {
				if (sceRtcGetDayOfWeek(year, spec->month, day) == (int)spec->dayOfWeek) {
					if (matchCount == (int)spec->weekIndex) {
						outDateTime->day = (SceUInt16)day;
						break;
					}
					++matchCount;
				}
			}
		} else {
			for (int day = daysInMonth; day > 0; --day) {
				if (sceRtcGetDayOfWeek(year, spec->month, day) == (int)spec->dayOfWeek) {
					outDateTime->day = (SceUInt16)day;
					break;
				}
			}
		}
	}

	outDateTime->hour = spec->hour;
	outDateTime->minute = spec->minute;
	outDateTime->second = 0;
	outDateTime->microsecond = 0;
	return 0;
}

/* 0x813654D4: test whether localTime falls inside [dstStart, dstEnd) with adjustment mode */
static __attribute__((noinline)) int rtcUtilIsInDstRange(const SceDateTime *localTime,
			       const RtcDstTransitionSpec *startSpec,
			       const RtcDstTransitionSpec *endSpec,
			       int adjustMode, SceBool *outInDst)
{
	if (localTime == NULL || startSpec == NULL || endSpec == NULL || outInDst == NULL)
		return -1;

	int yearSpanMode = 0;
	if (startSpec->month > endSpec->month) {
		if (startSpec->month > localTime->month || localTime->month > 12)
			yearSpanMode = 2;
		else
			yearSpanMode = 1;
	}

	SceUInt16 startYear = localTime->year;
	SceUInt16 endYear = localTime->year;
	if (yearSpanMode != 0) {
		if (yearSpanMode == 1)
			endYear = (SceUInt16)(localTime->year + 1);
		else
			startYear = (SceUInt16)(localTime->year - 1);
	}

	SceDateTime startDateTime;
	SceDateTime endDateTime;
	SceRtcTick startTick;
	SceRtcTick endTick;
	SceRtcTick currentTick;

	if (rtcUtilResolveTransitionDate(startYear, startSpec, &startDateTime) != 0)
		return -1;
	if (sceRtcGetTick(&startDateTime, &startTick) != 0)
		return -1;
	if (rtcUtilResolveTransitionDate(endYear, endSpec, &endDateTime) != 0)
		return -1;
	if (sceRtcGetTick(&endDateTime, &endTick) != 0)
		return -1;

	switch (adjustMode) {
	case 1:
		sceRtcTickAddHours(&startTick, &startTick, 1);
		break;
	case 2:
		sceRtcTickAddHours(&endTick, &endTick, -1);
		break;
	case 3:
		sceRtcTickAddHours(&endTick, &endTick, 1);
		break;
	case 4:
		sceRtcTickAddHours(&startTick, &startTick, -1);
		break;
	default:
		break;
	}

	sceRtcSetTick(&startDateTime, &startTick);
	sceRtcSetTick(&endDateTime, &endTick);

	if (sceRtcGetTick(localTime, &currentTick) != 0)
		return -1;

	int cmpStart = sceRtcCompareTick(&startTick, &currentTick);
	int cmpEnd = sceRtcCompareTick(&currentTick, &endTick);
	*outInDst = ((cmpStart == 0 || cmpStart == -1) && cmpEnd == -1) ? SCE_TRUE : SCE_FALSE;
	return 0;
}

/* 0x81365632: check primary DST range and exclude any Ramadan suspension window */
static __attribute__((noinline)) int rtcUtilCheckDstActive(int regTimeZone, const SceDateTime *localTime,
				 const RtcTimeZoneEntry *tzEntry,
				 int adjustMode, SceBool *outInDst)
{
	if (localTime == NULL || tzEntry == NULL || outInDst == NULL)
		return -1;

	if (rtcUtilIsInDstRange(localTime, &tzEntry->dstStart, &tzEntry->dstEnd,
				adjustMode, outInDst) != 0) {
		return -1;
	}

	if (*outInDst) {
		for (unsigned int i = 0; i < RTC_UTIL_DST_SKIP_RULE_COUNT; ++i) {
			const RtcDstSkipRule *skipRule = rtcUtilGetDstSkipRule(i, regTimeZone);
			if (skipRule != NULL) {
				SceBool inSkipWindow = SCE_FALSE;
				int skipAdjust = 0;
				if (adjustMode == 1)
					skipAdjust = 3;
				else if (adjustMode == 2)
					skipAdjust = 4;

				if (rtcUtilIsInDstRange(localTime, &skipRule->skipStart,
							&skipRule->skipEnd,
							skipAdjust, &inSkipWindow) != 0) {
					return -1;
				}
				if (inSkipWindow) {
					*outInDst = SCE_FALSE;
					return 0;
				}
			}
		}
	}
	return 0;
}

/* 0x813656E8: resolve effective timezone entry, applying any year-specific exception */
static __attribute__((noinline)) int rtcUtilResolveTimeZoneRule(int regTimeZone, const SceDateTime *localTime,
				      RtcTimeZoneEntry *outEntry)
{
	const RtcTimeZoneEntry *baseEntry = rtcUtilGetTimeZoneEntry((unsigned int)regTimeZone);
	if (localTime == NULL || baseEntry == NULL)
		return -1;

	int yearSpanMode = 0;
	if (baseEntry->dstStart.month > baseEntry->dstEnd.month) {
		if (baseEntry->dstStart.month > localTime->month || localTime->month > 12)
			yearSpanMode = 2;
		else
			yearSpanMode = 1;
	}

	SceUInt16 startYear = localTime->year;
	SceUInt16 endYear = localTime->year;
	if (yearSpanMode != 0) {
		if (yearSpanMode == 1)
			endYear = (SceUInt16)(localTime->year + 1);
		else
			startYear = (SceUInt16)(localTime->year - 1);
	}

	int matchedException = -1;
	for (unsigned int i = 0; i < RTC_UTIL_DST_EXCEPTION_COUNT; ++i) {
		const RtcDstExceptionEntry *ex = rtcUtilGetDstException(i, regTimeZone);
		if (ex != NULL &&
		    ex->dstStart.year == startYear &&
		    ex->dstEnd.year == endYear) {
			matchedException = (int)i;
		}
	}

	outEntry->utcOffsetMinutes = baseEntry->utcOffsetMinutes;
	if (matchedException == -1) {
		outEntry->hasDst = baseEntry->hasDst;
		outEntry->dstStart = baseEntry->dstStart;
		outEntry->dstEnd = baseEntry->dstEnd;
	} else {
		const RtcDstExceptionEntry *ex =
			rtcUtilGetDstException((unsigned int)matchedException, regTimeZone);
		outEntry->hasDst = ex->hasDst;
		outEntry->dstStart = ex->dstStart;
		outEntry->dstEnd = ex->dstEnd;
	}
	return 0;
}

/* Apply date/time/timezone/DST configuration to AppMgr and VshRtc */
static void rtcUtilApplySystemConf(int dateFormat, int timeFormat,
				   int regTimeZone, int summerTime)
{
	if (sceRegMgrSetKeyInt("/CONFIG/DATE", "summer_time", summerTime) != 0)
		return;

	RtcDateTimeSystemConf conf;
	conf.dateFormat = dateFormat;
	conf.timeFormat = timeFormat;
	conf.timeZone = regTimeZone;
	conf.utcOffsetMinutes = rtcUtilGetTimeZoneEntry((unsigned int)regTimeZone)->utcOffsetMinutes;
	conf.summerTime = summerTime;

	if (vshAppMgrSystemParamDateTimeSetConf(&conf) != 0)
		return;

	int vshDateFormat = 0;
	switch (conf.dateFormat) {
	case 0:
		vshDateFormat = 0;
		break;
	case 1:
		vshDateFormat = 2;
		break;
	case 2:
		vshDateFormat = 1;
		break;
	default:
		break;
	}

	int isTwelveHour = 0;
	switch (conf.timeFormat) {
	case 0:
		isTwelveHour = 1;
		break;
	case 1:
		isTwelveHour = 0;
		break;
	default:
		break;
	}
	vshRtcSetConf(vshDateFormat, isTwelveHour, conf.utcOffsetMinutes, 60 * conf.summerTime);
}

/* 0x81445258..0x81445294: background job updating summer_time and RTC config */
class SetConfJob : public paf::job::JobItem {
public:
	SetConfJob(int dateFormat, int timeFormat, int timeZone, int summerTime)
		: paf::job::JobItem("SetConfJob", NULL),
		  m_dateFormat(dateFormat),
		  m_timeFormat(timeFormat),
		  m_timeZone(timeZone),
		  m_summerTime(summerTime)
	{
	}

	virtual ~SetConfJob() {}

	virtual int32_t Run()
	{
		rtcUtilApplySystemConf(m_dateFormat, m_timeFormat, m_timeZone, m_summerTime);
		return 0;
	}

	virtual void Finish(int32_t result)
	{
		(void)result;
	}

private:
	int m_dateFormat;
	int m_timeFormat;
	int m_timeZone;
	int m_summerTime;
};

/* 0x8136584C: registry watcher callback refreshing cached /CONFIG/DATE keys */
static int rtcUtilRegistryCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)common;
	for (unsigned int i = 0; i < RTC_UTIL_REG_KEY_COUNT; ++i) {
		if (sceRegMgrGetKeyInt("/CONFIG/DATE",
				       s_rtcRegKeys[i].keyName,
				       s_rtcRegKeys[i].target) != 0) {
			break;
		}
	}
	return 0;
}

/* 0x81365888: synchronize RTC summer time and date/time settings with /CONFIG/DATE */
int shellRtcUtilSync(bool fromCallback)
{
	bool firstInit = false;
	bool useBackgroundJob = false;

	if (!fromCallback) {
		if (s_regSummerTime == -1) {
			firstInit = true;
			for (unsigned int i = 0; i < RTC_UTIL_REG_KEY_COUNT; ++i) {
				if (sceRegMgrGetKeyInt("/CONFIG/DATE",
						       s_rtcRegKeys[i].keyName,
						       s_rtcRegKeys[i].target) != 0) {
					break;
				}
			}
		} else {
			useBackgroundJob = true;
		}
	}

	if (s_regCallbackId == -1) {
		s_regCallbackId = sceKernelCreateCallback("SceRtcUtilRegistryCallback", 0,
							  rtcUtilRegistryCallback, NULL);
		if (s_regCallbackId >= 0 && sceRegMgrRegisterCallback(s_regCallbackId) >= 0) {
			int addErr = 0;
			for (unsigned int i = 0; i < RTC_UTIL_REG_KEY_COUNT; ++i) {
				addErr = sceRegMgrAddRegistryCallback(s_regCallbackId,
								      s_rtcRegKeys[i].fullPath);
				if (addErr < 0)
					break;
			}
			if (addErr >= 0)
				sceRegMgrStartCallback(s_regCallbackId);
		}
	}

	int autoSummerTime = s_regAutoSummerTime;
	int summerTime = firstInit ? 0 : s_regSummerTime;
	int dateFormat = s_regDateFormat;
	int timeFormat = s_regTimeFormat;
	int timeZone = s_regTimeZone;
	bool needsUpdate = firstInit;

	SceDateTime localTime;
	if (sceRtcGetCurrentClockLocalTime(&localTime) != 0)
		autoSummerTime = 0;

	RtcTimeZoneEntry resolvedTz;
	rtcUtilResolveTimeZoneRule(timeZone, &localTime, &resolvedTz);

	if (autoSummerTime == 1 && resolvedTz.hasDst == 1) {
		SceBool inDst = SCE_FALSE;
		int adjustMode = (summerTime == 1) ? 1 : 2;
		if (rtcUtilCheckDstActive(timeZone, &localTime, &resolvedTz,
					  adjustMode, &inDst) == 0) {
			if (summerTime == 0 && inDst) {
				summerTime = 1;
				needsUpdate = true;
			} else if (summerTime == 1 && !inDst) {
				summerTime = 0;
				needsUpdate = true;
			}
		}
	}

	if (needsUpdate) {
		if (useBackgroundJob) {
			paf::common::SharedPtr<paf::job::JobItem> job(
				new SetConfJob(dateFormat, timeFormat, timeZone, summerTime));
			paf::job::JobQueue::default_queue->Enqueue(job);
		} else {
			rtcUtilApplySystemConf(dateFormat, timeFormat, timeZone, summerTime);
		}
	}

	return 0;
}
