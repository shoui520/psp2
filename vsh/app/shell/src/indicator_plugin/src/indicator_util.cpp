/*
 * indicator_util.cpp: the indicator's texts: the time left in seconds or minutes, a database
 * time as the system shows it, an error's title and its codes, the plugin's strings by ID, and
 * an error posted to the error history (0x81182050-0x811825A0). The file's and all names here
 * are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/string.h>
#include <std/stdio.h>
#include <std/stdio.h>
#include <rtc/rtc.h>
#include <datetime/datetime.h>
#include <common/encoded_string.h>
#include <versatileparam/idparam.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <error/error.h>
#include "../../../plugins.h"
/* Sony built this file without the keys' header, so it has no static initializer */
#define LSDB_KEYS_H
#include "indicator.h"

using namespace paf;

/* the time left's texts, in seconds and in minutes, and an error's title when it has none
   (RCO hashes without labels) */
#define INDICATOR_STRING_SECONDS_LEFT	0x95954687
#define INDICATOR_STRING_MINUTES_LEFT	0x1F2827C8
#define INDICATOR_STRING_ERROR_TITLE	0xC51AA4A4

/* from how many seconds the time left is shown in minutes */
#define INDICATOR_MINUTES_FROM		120

/* the flags of the indicator's errors in the history (PLACEHOLDER) */
#define INDICATOR_ERROR_HISTORY_FLAGS	15

/* an error put in the error history: its code, its record's flags and its title (the message is
   not set; PLACEHOLDER names) */
class IndicatorErrorHistory {
public:
	IndicatorErrorHistory(int code) : m_code(code), m_flags(0), m_reserved(0)
	{
	}

	/* the record of the error and of the application in front posted */
	int Post()
	{
		ErrorRecord record;
		SceAppMgrAppStatus status;

		sce_paf_memset(&record, 0, sizeof(record));
		record.code = m_code;

		common::EncodedString text;

		ScePafMisc_7709D1B2(&text, m_title, common::EncodedString::Encoding_Utf16);
		sce_paf_snprintf(record.text, 256, "%s", ScePafMisc_A7C6AF5C(&text));
		record.id = m_reserved | (m_flags << 24);
		if (sceAppMgrGetStatusByName(NULL, &status) == 0)
			sce_paf_strlcpy((char *)record.key, status.appName, sizeof(record.key));

		int ret = sceErrorHistoryPostError(&record);

		ScePafMisc_B45A7521(&text);
		return ret;
	}

	int m_code;
	SceUInt32 m_flags : 8;
	SceUInt32 m_reserved : 24;
	wstring m_title;
	wstring m_message;
};

/* (0x81182050) */
wstring indicatorRemainingText(int seconds)
{
	wstring text;
	wchar_t number[16] = L"";
	const wchar_t *format;

	if (seconds < INDICATOR_MINUTES_FROM) {
		sce_paf_swprintf(number, 8, L"%d", seconds);
		format = indicatorGetString(IDParam(s_empty_name, INDICATOR_STRING_SECONDS_LEFT));
	} else {
		sce_paf_swprintf(number, 8, L"%d", (seconds + 59) / 60);
		format = indicatorGetString(IDParam(s_empty_name, INDICATOR_STRING_MINUTES_LEFT));
	}
	if (format != NULL)
		text = shellFormatText(format, number);
	return text;
}

/* (0x8118221C) */
wstring indicatorTimeText(const char *time)
{
	wstring text;

	if (time != NULL) {
		datetime::DateTime dt = datetime::DateTime::ParseSQLiteDateTime(time);
		rtc::Tick tick = datetime::DateTime::ConvertDateTimeToRtcTick(dt);

		text = shellTickText(&tick, true);
	}
	return text;
}

/* (0x811822A4) */
int indicatorErrorText(wstring *title, wstring *text, int error, bool code, bool hex)
{
	int ret = 0;

	if (title->length() == 0)
		*title = indicatorGetString(IDParam(s_empty_name, INDICATOR_STRING_ERROR_TITLE));

	int parts = 0;

	if (code)
		parts = 1;
	if (hex)
		parts++;
	if (parts > 0) {
		wchar_t number[9];
		char external[18];

		*text += L"(";
		if (code) {
			common::EncodedString encoded;

			ret = sceErrorGetExternalString(external, error);
			*text += ScePafMisc_53948F03(ScePafMisc_C11CB5E4(&encoded, external, 1));
			ScePafMisc_B45A7521(&encoded);
		}
		if (parts > 1)
			*text += L", ";
		if (hex) {
			sce_paf_swprintf(number, 9, L"%08x", error);
			*text += number;
		}
		*text += L")";
	}
	return ret;
}

/* (0x81182420) */
__attribute__((noinline))
const wchar_t *indicatorGetString(const IDParam &id)
{
	return g_indicatorPlugin->GetString(id);
}

/* the error's title with its codes posted to the error history (0x8118243A) */
int indicatorPostError(int error)
{
	wstring title;

	{
		wstring errorTitle;
		wstring codes;

		indicatorErrorText(&errorTitle, &codes, error, true, true);
		title = errorTitle;
	}

	IndicatorErrorHistory history(error);

	history.m_title = title;
	history.m_flags = INDICATOR_ERROR_HISTORY_FLAGS;
	return history.Post();
}
