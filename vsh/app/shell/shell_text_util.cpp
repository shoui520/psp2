/*
 * shell_text_util.cpp: shell text formatting helpers for byte/KB sizes,
 * positional %1..%(11) template strings, relative timestamps, and remaining
 * expiration durations (0x81343594..0x8134405D).
 */
#include "plugins.h"

#include <stddef.h>
#include <scetypes.h>
#include <framework.h>
#include <std/string>
#include <std/string.h>
#include <std/stdio.h>
#include <versatileparam/idparam.h>
#include <rtc/rtc.h>
#include <datetime/datetime.h>

/* Common string resource hashes in ScePaf common resource */
#define SHELL_COMMON_STR_JUST_NOW	0x93D80BE7u
#define SHELL_COMMON_STR_MINS_AGO	0xA6D44F10u
#define SHELL_COMMON_STR_ONE_HOUR_AGO	0x0989E4ABu
#define SHELL_COMMON_STR_HOURS_AGO	0x3A2A7190u
#define SHELL_COMMON_STR_EXPIRED	0x7B505BD4u	/* 2068863956 */
#define SHELL_COMMON_STR_DAYS_REMAINING	0xF3C900E6u

#define SHELL_SIZE_KB_THRESHOLD_BYTES	0x9C3C00ULL	/* 9999 KB in bytes */
#define SHELL_SIZE_MB_THRESHOLD_BYTES	0x270F00000ULL	/* 9999 MB in bytes */
#define SHELL_SIZE_KB_THRESHOLD_KB	9999u
#define SHELL_SIZE_MB_THRESHOLD_KB	0x9C3C00u
#define SHELL_TICK_NEVER_EXPIRES	0x7FFFFFFFFFFFFFFFULL
#define SHELL_USEC_PER_SEC		1000000ULL
#define SHELL_USEC_PER_MIN		60000000ULL

typedef const wchar_t *(*ShellFormatTokenCallback)(wchar_t tokenChar);

/* 0x815C635C */
static ShellFormatTokenCallback s_customFormatCallback = NULL;

/* 0x81343594 */
void shellSizeText(paf::wstring *text, SceUInt64 size)
{
	wchar_t buf[16];
	if (size > SHELL_SIZE_KB_THRESHOLD_BYTES) {
		if (size > SHELL_SIZE_MB_THRESHOLD_BYTES)
			sce_paf_swprintf(buf, 16, L"%llu GB", (size + 0x3FFFFFFFULL) >> 30);
		else
			sce_paf_swprintf(buf, 16, L"%llu MB", (size + 0xFFFFFULL) >> 20);
	} else {
		sce_paf_swprintf(buf, 16, L"%llu KB", (size + 0x3FFULL) >> 10);
	}
	text->assign(buf, sce_paf_wcslen(buf));
}

/* 0x8134367C */
void shellSizeTextKb(paf::wstring *text, SceUInt32 sizeKb)
{
	wchar_t buf[16];
	if (sizeKb > SHELL_SIZE_KB_THRESHOLD_KB) {
		if (sizeKb > SHELL_SIZE_MB_THRESHOLD_KB)
			sce_paf_swprintf(buf, 16, L"%d GB", sizeKb >> 20);
		else
			sce_paf_swprintf(buf, 16, L"%d MB", sizeKb >> 10);
	} else {
		sce_paf_swprintf(buf, 16, L"%d KB", sizeKb);
	}
	text->assign(buf, sce_paf_wcslen(buf));
}

/* 0x81343700 */
paf::wstring shellFormatText(const wchar_t *format, const wchar_t *first,
			     const wchar_t *second, const wchar_t *third,
			     const wchar_t *fourth, const wchar_t *fifth,
			     const wchar_t *sixth, const wchar_t *seventh,
			     const wchar_t *eighth, const wchar_t *ninth,
			     const wchar_t *tenth, const wchar_t *eleventh)
{
	const wchar_t *replacements[11] = {
		first, second, third, fourth, fifth, sixth,
		seventh, eighth, ninth, tenth, eleventh
	};

	paf::wstring result;
	const wchar_t *cursor = format;
	int matchCount = 0;

	for (;;) {
		const wchar_t *percent = sce_paf_wcschr(cursor, L'%');
		if (percent == NULL)
			break;

		result.append(cursor, static_cast<paf::wstring::size_type>(percent - cursor));
		wchar_t nextCh = percent[1];
		if (nextCh == L'%') {
			result.append(1, L'%');
			cursor = percent + 2;
		} else if (nextCh >= L'1' && nextCh <= L'9') {
			const wchar_t *rep = replacements[nextCh - L'1'];
			result.append(rep, sce_paf_wcslen(rep));
			cursor = percent + 2;
		} else if ((nextCh >= L'A' && nextCh <= L'Z') ||
			   (nextCh >= L'a' && nextCh <= L'z')) {
			if (s_customFormatCallback != NULL) {
				const wchar_t *rep = s_customFormatCallback(nextCh);
				result.append(rep, sce_paf_wcslen(rep));
			}
			cursor = percent + 2;
		} else if (nextCh == L'(' && percent[2] == L'1' &&
			   percent[3] >= L'0' && percent[3] <= L'1' &&
			   percent[4] == L')') {
			const wchar_t *rep = replacements[9 + (percent[3] - L'0')];
			result.append(rep, sce_paf_wcslen(rep));
			cursor = percent + 5;
		} else if (nextCh == L'\0') {
			return result;
		} else {
			cursor = percent + 2;
		}
		matchCount++;
	}

	if (matchCount != 0)
		result.append(cursor, sce_paf_wcslen(cursor));
	else if (format != NULL)
		result.assign(format, sce_paf_wcslen(format));

	return result;
}

static paf::wstring formatRelativeCommonString(SceUInt32 commonHash, const wchar_t *numText)
{
	paf::IDParam resId("", commonHash);
	const wchar_t *tmpl = paf::Framework::Instance()->GetCommonString(resId);
	if (tmpl == NULL)
		tmpl = L"";
	return shellFormatText(tmpl, numText);
}

/* 0x8134385E */
paf::wstring shellTickText(const paf::rtc::Tick *tick, bool flag)
{
	(void)flag;
	paf::wstring result;
	wchar_t buf[64];
	sce_paf_memset(buf, 0, sizeof(buf));

	if (*tick == 0)
		return result;

	paf::rtc::Tick nowTick = 0;
	if (paf::rtc::GetCurrentNetworkTick(&nowTick) != 0) {
		paf::rtc::Tick localTick = 0;
		paf::rtc::ConvertUtcToLocalTime(tick, &localTick);
		paf::datetime::DateTime dt =
			paf::datetime::DateTime::ConvertRtcTickToDateTime(&localTick);
		paf::wstring dateFmt = paf::datetime::DateTime::GetDateFormat();
		dateFmt.append(L" %X", sce_paf_wcslen(L" %X"));
		dt.Format(buf, 128, dateFmt.c_str(), false);
		result.assign(buf, sce_paf_wcslen(buf));
		return result;
	}

	if (nowTick < *tick)
		return result;

	SceUInt64 elapsedSecs = (nowTick - *tick) / SHELL_USEC_PER_SEC;
	SceUInt64 elapsedMins = elapsedSecs / 60ULL;
	if (elapsedMins < 2ULL) {
		sce_paf_swprintf(buf, 64, L"");
		return formatRelativeCommonString(SHELL_COMMON_STR_JUST_NOW, buf);
	}
	if (elapsedMins < 60ULL) {
		sce_paf_swprintf(buf, 64, L"%llu", elapsedMins);
		return formatRelativeCommonString(SHELL_COMMON_STR_MINS_AGO, buf);
	}

	SceUInt64 elapsedHours = elapsedMins / 60ULL;
	if (elapsedHours < 2ULL) {
		sce_paf_swprintf(buf, 64, L"");
		return formatRelativeCommonString(SHELL_COMMON_STR_ONE_HOUR_AGO, buf);
	}
	if (elapsedHours < 24ULL) {
		sce_paf_swprintf(buf, 64, L"%llu", elapsedHours);
		return formatRelativeCommonString(SHELL_COMMON_STR_HOURS_AGO, buf);
	}

	paf::rtc::Tick localNow = 0;
	paf::rtc::ConvertUtcToLocalTime(&nowTick, &localNow);
	paf::datetime::DateTime nowDt =
		paf::datetime::DateTime::ConvertRtcTickToDateTime(&localNow);
	(void)nowDt;

	paf::rtc::Tick localTick = 0;
	paf::rtc::ConvertUtcToLocalTime(tick, &localTick);
	paf::datetime::DateTime tickDt =
		paf::datetime::DateTime::ConvertRtcTickToDateTime(&localTick);

	paf::wstring dateFmt = paf::datetime::DateTime::GetDateFormat();
	dateFmt.append(L" %X", sce_paf_wcslen(L" %X"));
	tickDt.Format(buf, 128, dateFmt.c_str(), false);
	result.assign(buf, sce_paf_wcslen(buf));
	return result;
}

/* 0x81343D8E */
paf::wstring shellRemainTimeText(const paf::rtc::Tick *expireTick)
{
	paf::wstring defaultDash(L"-");
	if (*expireTick == SHELL_TICK_NEVER_EXPIRES)
		return defaultDash;

	paf::rtc::Tick nowTick = 0;
	if (paf::rtc::GetCurrentNetworkTick(&nowTick) < 0)
		return defaultDash;

	if (*expireTick <= nowTick) {
		paf::IDParam resId("", SHELL_COMMON_STR_EXPIRED);
		const wchar_t *expiredStr = paf::Framework::Instance()->GetCommonString(resId);
		if (expiredStr == NULL)
			return paf::wstring();
		return paf::wstring(expiredStr, sce_paf_wcslen(expiredStr));
	}

	SceUInt64 totalMins = (*expireTick - nowTick) / SHELL_USEC_PER_MIN;
	SceUInt32 hours = static_cast<SceUInt32>(totalMins / 60ULL);
	SceUInt32 remMins = static_cast<SceUInt32>(totalMins % 60ULL);

	wchar_t buf[64];
	if (hours < 48u) {
		sce_paf_swprintf(buf, 64, L"%u:%02u'", hours, remMins);
		return paf::wstring(buf, sce_paf_wcslen(buf));
	}

	SceUInt32 days = hours / 24u;
	sce_paf_swprintf(buf, 64, L"%u", days);
	return formatRelativeCommonString(SHELL_COMMON_STR_DAYS_REMAINING, buf);
}
