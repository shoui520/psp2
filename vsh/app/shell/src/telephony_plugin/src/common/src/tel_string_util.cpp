/*
 * tel_string_util.cpp: plugin heap allocator helpers, formatted string helpers,
 * TelObject, TelEventInfo, TelDateTime, TelImeiInfo, and bit utilities
 * (3.65-CEX: 0x8124E580..0x8124E795, 0x813DBD14, 0x813DD098..0x813DD773).
 */
#include <stddef.h>
#include <stdarg.h>
#include <scetypes.h>
#include <std/ctype.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <plugin.h>
#include <rtc.h>
#include <rtc/rtc.h>
#include <memory/heap_allocator.h>
#include "../tel_common.h"

extern "C" {
extern paf::memory::HeapAllocator *GetGlobalHeapAllocator(void);
extern SceUInt32 sceRtcGetTickResolution(void);
extern int sceRtcConvertUtcToLocalTime(const SceRtcTick *utc, SceRtcTick *localTime);
}

namespace telephony {

namespace {

static paf::memory::HeapAllocator *g_pluginAllocator = NULL;

/* 0x81549478: De Bruijn lookup for lowest set bit in a byte (58 * byte >> 5) */
static const int kLowestBitLookup[8] = { 7, 0, 5, 1, 6, 4, 3, 2 };

} /* namespace */

/* 0x8124E580 */
paf::memory::HeapAllocator *telSetPluginAllocator(paf::Plugin *plugin)
{
	g_pluginAllocator = plugin->m_private_heap;
	return g_pluginAllocator;
}

/* 0x8124E592 */
paf::memory::HeapAllocator *telGetAllocator(void)
{
	if (g_pluginAllocator == NULL)
		g_pluginAllocator = GetGlobalHeapAllocator();
	return g_pluginAllocator;
}

/* 0x8124E5B0 */
void *telMalloc(SceSize size)
{
	return telGetAllocator()->sce_paf_malloc(size);
}

/* 0x8124E5D8 */
void telFree(void *ptr)
{
	if (g_pluginAllocator != NULL)
		g_pluginAllocator->sce_paf_free(ptr);
}

/* 0x8124E5F0 */
paf::string telFormatString(const char *format, ...)
{
	va_list args;
	char measureBuf[4];

	va_start(args, format);
	int needed = sce_paf_vsnprintf(measureBuf, 0, format, args) + 1;
	va_end(args);

	char *formatted = new char[needed];
	if (formatted == NULL)
		return paf::string("");

	sce_paf_memset(formatted, 0, (SceSize)needed);
	va_start(args, format);
	sce_paf_vsnprintf(formatted, (SceSize)needed, format, args);
	va_end(args);

	paf::string result(formatted);
	delete[] formatted;
	return result;
}

/* 0x8124E6C0 */
paf::wstring telFormatWString(const wchar_t *format, ...)
{
	va_list args;
	wchar_t measureBuf[4];

	va_start(args, format);
	int needed = sce_paf_vswprintf(measureBuf, 0, format, args) + 1;
	va_end(args);

	wchar_t *formatted = new wchar_t[needed];
	if (formatted == NULL)
		return paf::wstring(L"");

	sce_paf_memset(formatted, 0, (SceSize)needed * sizeof(wchar_t));
	va_start(args, format);
	sce_paf_vswprintf(formatted, (SceSize)needed, format, args);
	va_end(args);

	paf::wstring result(formatted);
	delete[] formatted;
	return result;
}

/* 0x813DBD14 */
int telFindFirstSetBit(SceUInt32 mask)
{
	if (mask == 0)
		return -1;
	int bitOffset = 0;
	while ((mask & 0xFFU) == 0) {
		mask >>= 8;
		bitOffset += 8;
		if (mask == 0)
			return -1;
	}
	SceUInt8 lowByte = (SceUInt8)(mask & 0xFFU);
	SceUInt8 lowestBit = (SceUInt8)(lowByte & (SceUInt8)(0U - lowByte));
	return kLowestBitLookup[(SceUInt8)(58U * lowestBit) >> 5] + bitOffset;
}

/* TelObject::Clone (0x8142B2C8) */
paf::common::SharedPtr<TelObject> TelObject::Clone() const
{
	return paf::common::SharedPtr<TelObject>(new TelObject());
}

/* TelEventInfo::Clone (0x814539C0) */
paf::common::SharedPtr<TelObject> TelEventInfo::Clone() const
{
	TelEventInfo *copy = new TelEventInfo(m_eventId, m_eventArg, m_eventResult);
	if (copy != NULL)
		copy->SetTimerHandle(m_timerHandle);
	return paf::common::SharedPtr<TelObject>(copy);
}

/* TelDateTime (0x813DD098..0x813DD647) */
TelDateTime::TelDateTime()
	: m_dateTime(0, 0, 0, 0, 0, 0, 0)
{
}

TelDateTime::TelDateTime(SceUInt16 year, SceUInt16 month, SceUInt16 day, SceUInt16 hour,
			 SceUInt16 minute, SceUInt16 second, SceUInt16 millisecond)
	: m_dateTime(year, month, day, hour, minute, second, 0)
{
	m_dateTime.m_microsecond = (SceUInt32)millisecond * 1000U;
}

TelDateTime::~TelDateTime()
{
}

/* 0x813DD15C */
paf::wstring TelDateTime::FormatLocaleString() const
{
	wchar_t formatted[22];
	sce_paf_memset(formatted, 0, sizeof(formatted));
	paf::datetime::DateTime::SetDateTimeFormat(L"%x %X");
	m_dateTime.Format(formatted, 21, L"%c", false);
	return telFormatWString(L"%ls", formatted);
}

/* 0x813DD1C0 */
paf::string TelDateTime::FormatIsoString() const
{
	return telFormatString("%04u-%02u-%02u %02u:%02u:%02u.%03u",
			       (unsigned int)m_dateTime.m_year,
			       (unsigned int)m_dateTime.m_month,
			       (unsigned int)m_dateTime.m_day,
			       (unsigned int)m_dateTime.m_hour,
			       (unsigned int)m_dateTime.m_minute,
			       (unsigned int)m_dateTime.m_second,
			       (unsigned int)(m_dateTime.m_microsecond / 1000U));
}

/* 0x813DD204 */
void TelDateTime::ParseIsoString(const paf::string &isoStr)
{
	m_dateTime = paf::datetime::DateTime(0, 0, 0, 0, 0, 0, 0);
	SceSize len = isoStr.length();
	if (len >= 4)
		m_dateTime.m_year = (SceUInt16)sce_paf_atoi(isoStr.substr(0, 4).c_str());
	if (len >= 7)
		m_dateTime.m_month = (SceUInt16)sce_paf_atoi(isoStr.substr(5, 2).c_str());
	if (len >= 10)
		m_dateTime.m_day = (SceUInt16)sce_paf_atoi(isoStr.substr(8, 2).c_str());
	if (len >= 13)
		m_dateTime.m_hour = (SceUInt16)sce_paf_atoi(isoStr.substr(11, 2).c_str());
	if (len >= 16)
		m_dateTime.m_minute = (SceUInt16)sce_paf_atoi(isoStr.substr(14, 2).c_str());
	if (len >= 19)
		m_dateTime.m_second = (SceUInt16)sce_paf_atoi(isoStr.substr(17, 2).c_str());
	if (len >= 23) {
		int msec = sce_paf_atoi(isoStr.substr(20, 3).c_str());
		m_dateTime.m_microsecond = (SceUInt32)msec * 1000U;
	}
}

/* 0x813DD3D8 */
void TelDateTime::ApplyQuarterHourTimezone(int quarterHoursEast)
{
	if (quarterHoursEast > 0) {
		paf::datetime::DateTime dtCopy = m_dateTime;
		paf::rtc::Tick tick = paf::datetime::DateTime::ConvertDateTimeToRtcTick(dtCopy);
		SceUInt32 resolution = sceRtcGetTickResolution();
		int totalHours = (15 * quarterHoursEast) / 60;
		int remMinutes = (15 * quarterHoursEast) % 60;
		SceUInt64 deltaSeconds = (SceUInt64)totalHours * 3600ULL + (SceUInt64)remMinutes * 60ULL;
		tick -= deltaSeconds * (SceUInt64)resolution;
		m_dateTime = paf::datetime::DateTime::ConvertRtcTickToDateTime(&tick);
	} else if (quarterHoursEast < 0) {
		int absQuarter = sce_paf_abs(quarterHoursEast);
		paf::datetime::DateTime offset((SceUInt16)0, (SceUInt16)0, (SceUInt16)0,
					       (SceUInt16)((15 * absQuarter) / 60),
					       (SceUInt16)((15 * absQuarter) % 60),
					       (SceUInt16)0, 0);
		m_dateTime = m_dateTime + offset;
	}
}

/* 0x813DD584 */
void TelDateTime::ConvertUtcToLocal()
{
	SceRtcTick utcTick;
	SceRtcTick localTick;
	sce_paf_memset(&utcTick, 0, sizeof(utcTick));
	sce_paf_memset(&localTick, 0, sizeof(localTick));

	utcTick.tick = paf::datetime::DateTime::ConvertDateTimeToRtcTick(m_dateTime);
	if (sceRtcConvertUtcToLocalTime(&utcTick, &localTick) != 0) {
		m_dateTime = paf::datetime::DateTime(0, 0, 0, 0, 0, 0, 0);
	} else {
		paf::rtc::Tick tick = localTick.tick;
		m_dateTime = paf::datetime::DateTime::ConvertRtcTickToDateTime(&tick);
	}
}

/* TelImeiInfo (0x813DD678..0x813DD773) */
TelImeiInfo::TelImeiInfo()
	: m_imei("")
{
}

TelImeiInfo::~TelImeiInfo()
{
}

TelephonyResult TelImeiInfo::SetImei(const paf::string &imei)
{
	m_imei = imei;
	return TelephonyResult(TEL_OK);
}

TelephonyResult TelImeiInfo::GetImei(paf::string *outImei) const
{
	*outImei = m_imei;
	return TelephonyResult(TEL_OK);
}

/* 0x813DD740 */
bool TelImeiInfo::IsValid() const
{
	if (m_imei.length() >= 16U)
		return false;
	for (SceSize idx = 0; idx < m_imei.length(); ++idx) {
		char ch = m_imei.c_str()[idx];
		if (ch < '0' || ch > '9')
			return false;
	}
	return true;
}

} /* namespace telephony */
