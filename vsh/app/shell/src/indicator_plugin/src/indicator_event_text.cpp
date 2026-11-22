/*
 * indicator_event_text.cpp: what the bar says of a new event: its message's format by its
 * kind, the message made from the format and its arguments, the text of the system
 * applications' notices, and the tests of an event's kind and its download's status
 * (0x81173AD0-0x81174D48). The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <common/char_encoding.h>
#include "../../../plugins.h"
#include "../../livespace_db/lsdb_keys.h"
#include "../../esecron/include/esebase.hpp"
#include "indicator.h"

using namespace paf;

/* the strings of the messages (their IDs' hashes) */
#define INDICATOR_EVENT_MSG_INSTALLED		0xB769BD20	/* also kind 82 done (act type 3) */
#define INDICATOR_EVENT_MSG_APP			0xDC0CC888	/* "%1" of a system application */
#define INDICATOR_EVENT_MSG_SETTINGS		0xD4DF4C79	/* NPXS10010 */
#define INDICATOR_EVENT_MSG_PARTY		0xB112FDEA	/* NPXS10004 */
#define INDICATOR_EVENT_MSG_FRIENDS		0x0B40173F	/* NPXS10009 */
#define INDICATOR_EVENT_MSG_ARG_FIRST		0x45285D4B	/* " (%1" of kinds 2309 and 2311 */
#define INDICATOR_EVENT_MSG_ARG_SECOND		0x45118BFD	/* of kinds 2310 and 2311 */

/* no string for the event's kind */
#define INDICATOR_EVENT_NO_MESSAGE		L"No Message ID %1"

/* the kinds that add their arguments' texts after the message */
#define INDICATOR_EVENT_KIND_ARG_FIRST		2309
#define INDICATOR_EVENT_KIND_ARG_SECOND		2310
#define INDICATOR_EVENT_KIND_ARG_BOTH		2311

/* the background download's status (msg_arg8): its state in bits 4 to 7 */
#define INDICATOR_EVENT_STATE(status)		((unsigned char)(status) >> 4)

/* a message's string by the event's kind (24 bytes) */
struct IndicatorEventFormat {
	SceInt64 msgType;
	IDParam id;
};

/* (0x81544A58) */
static IndicatorEventFormat s_eventFormats[] = {
	{ 0x50, IDParam(0xB769BD20) },
	{ 0x0, IDParam(0x77F396A2) },
	{ 0x1, IDParam(0x2A862367) },
	{ 0x51, IDParam(0x431F4D56) },
	{ 0x2, IDParam(0xE9FB03BC) },
	{ 0x3, IDParam(0x3DAA9CBD) },
	{ 0x4, IDParam(0x02139F28) },
	{ 0x6, IDParam(0xB29438D3) },
	{ 0x7, IDParam(0x431F4D56) },
	{ 0x52, IDParam(0x36328571) },
	{ 0x8, IDParam(0x77F396A2) },
	{ 0x400, IDParam(0x806B7BB4) },
	{ 0x401, IDParam(0x185AD731) },
	{ 0x800, IDParam(0x0C1F8D33) },
	{ 0x900, IDParam(0xBB947386) },
	{ 0x901, IDParam(0x1FDD00D9) },
	{ 0x902, IDParam(0xC4466DF5) },
	{ 0x903, IDParam(0xC9DAFF06) },
	{ 0x904, IDParam(0xF6E39738) },
	{ 0x905, IDParam(0xC4466DF5) },
	{ 0x906, IDParam(0xC9DAFF06) },
	{ 0x907, IDParam(0xF6E39738) }
};

/* the background download status's own (0x81544C68) */
static IndicatorEventFormat s_bgcpFormats[] = {
	{ 0x51, IDParam(0x8EA127DB) },
	{ 0x2, IDParam(0x9997ED01) },
	{ 0x3, IDParam(0x2947F738) }
};

/* the background download status's event (0x81173AD0) */
bool indicatorEventIsBgcpStatus(const LsdbNewEvent *event)
{
	return event != NULL && "BGCPSTATUS" == event->titleId;
}

/* the format of the event's message, NULL without one (0x81173AF8) */
const wchar_t *indicatorEventMessageFormat(const LsdbNewEvent *event)
{
	SceInt64 msgType = event->msgType;
	int actType = event->actType;

	if (msgType == 82 && actType == 3)
		return g_indicatorPlugin->GetString(
			IDParam(s_empty_name, INDICATOR_EVENT_MSG_INSTALLED));

	const wchar_t *format = NULL;

	if (indicatorEventIsBgcpStatus(event)) {
		for (unsigned int i = 0; i < sizeof(s_bgcpFormats) / sizeof(s_bgcpFormats[0]); i++) {
			if (msgType == s_bgcpFormats[i].msgType)
				return g_indicatorPlugin->GetString(s_bgcpFormats[i].id);
		}
	}

	IDParam id(s_empty_name, 0xFFFFFFFF);

	for (unsigned int i = 0; i < sizeof(s_eventFormats) / sizeof(s_eventFormats[0]); i++) {
		if (msgType == s_eventFormats[i].msgType) {
			id = s_eventFormats[i].id;
			if (id.GetIDHash() != 0xFFFFFFFF)
				format = g_indicatorPlugin->GetString(id);
			break;
		}
	}
	return format;
}

/* (0x81173CAE) */
wstring indicatorEventMessage(const LsdbNewEvent *event)
{
	return wstring(indicatorEventMessageFormat(event));
}

/* an argument of the event's message in UTF-16 */
static inline wstring indicatorEventArg(const LsdbNewEvent *event, int index)
{
	wstring arg;

	common::Utf8ToUtf16(event->msgArg[index], &arg, 0);
	return arg;
}

/* the event's message: its format with its arguments, and the kinds' own arguments after it
   (0x81173CDE) */
wstring indicatorEventText(const LsdbNewEvent *event)
{
	wstring message;
	const wchar_t *format = indicatorEventMessageFormat(event);

	if (format == NULL)
		return wstring(INDICATOR_EVENT_NO_MESSAGE);

	message = shellFormatText(format, indicatorEventArg(event, 0).c_str(),
				  indicatorEventArg(event, 1).c_str(), indicatorEventArg(event, 2).c_str(),
				  indicatorEventArg(event, 3).c_str(), indicatorEventArg(event, 4).c_str(),
				  indicatorEventArg(event, 5).c_str(), indicatorEventArg(event, 6).c_str(),
				  indicatorEventArg(event, 7).c_str(), indicatorEventArg(event, 8).c_str());

	switch (event->msgType) {
	case INDICATOR_EVENT_KIND_ARG_FIRST:
		message += L" (";
		{
			IDParam id(s_empty_name, INDICATOR_EVENT_MSG_ARG_FIRST);
			const wchar_t *text = g_indicatorPlugin->GetString(id);
			wstring arg;

			common::Utf8ToUtf16(event->msgArg[0], &arg, 0);
			message.append(shellFormatText(text, arg.c_str()));
		}
		message += L")";
		break;
	case INDICATOR_EVENT_KIND_ARG_SECOND:
		message += L" (";
		{
			IDParam id(s_empty_name, INDICATOR_EVENT_MSG_ARG_SECOND);
			const wchar_t *text = g_indicatorPlugin->GetString(id);
			wstring arg;

			common::Utf8ToUtf16(event->msgArg[0], &arg, 0);
			message.append(shellFormatText(text, arg.c_str()));
		}
		message += L")";
		break;
	case INDICATOR_EVENT_KIND_ARG_BOTH:
		message += L" (";
		{
			IDParam id(s_empty_name, INDICATOR_EVENT_MSG_ARG_FIRST);
			const wchar_t *text = g_indicatorPlugin->GetString(id);
			wstring arg;

			common::Utf8ToUtf16(event->msgArg[0], &arg, 0);
			message.append(shellFormatText(text, arg.c_str()));
		}
		message += L"  ";
		{
			IDParam id(s_empty_name, INDICATOR_EVENT_MSG_ARG_SECOND);
			const wchar_t *text = g_indicatorPlugin->GetString(id);
			wstring arg;

			common::Utf8ToUtf16(event->msgArg[1], &arg, 0);
			message.append(shellFormatText(text, arg.c_str()));
		}
		message += L")";
		break;
	default:
		break;
	}
	return message;
}

/* the text of a system application's notice: the application's name in its format; empty for
   the other titles (0x811743FE) */
wstring indicatorEventAppText(const string &titleId)
{
	wstring text((const wchar_t *)NULL, 0);
	const wchar_t *name;

	if (titleId.length() == 0)
		return text;

	if (sce_paf_strcmp(titleId.c_str(), "NPXS10010") == 0)
		name = g_indicatorPlugin->GetString(
			IDParam(s_empty_name, INDICATOR_EVENT_MSG_SETTINGS));
	else if (sce_paf_strcmp(titleId.c_str(), "NPXS10004") == 0)
		name = g_indicatorPlugin->GetString(IDParam(s_empty_name, INDICATOR_EVENT_MSG_PARTY));
	else if (sce_paf_strcmp(titleId.c_str(), "NPXS10009") == 0)
		name = g_indicatorPlugin->GetString(
			IDParam(s_empty_name, INDICATOR_EVENT_MSG_FRIENDS));
	else
		return text;

	if (name != NULL) {
		wstring format(g_indicatorPlugin->GetString(
			IDParam(s_empty_name, INDICATOR_EVENT_MSG_APP)));

		if (format.length() == 0)
			format = INDICATOR_EVENT_NO_MESSAGE;

		wstring args[9] = { name, L"", L"", L"", L"", L"", L"", L"", L"" };

		text = shellFormatText(format.c_str(), args[0].c_str(), args[1].c_str(),
				       args[2].c_str(), args[3].c_str(), args[4].c_str(),
				       args[5].c_str(), args[6].c_str(), args[7].c_str(),
				       args[8].c_str());
	}
	return text;
}

/* the event's notification told, for the kinds that have one: the system's (from 256), the
   installs' and the downloads' (0x811747AC) */
unsigned int indicatorEventIsNotified(const LsdbNewEvent *event)
{
	unsigned int notified = event->notifyFlags & 1;

	if ((event->msgType & ~0xFFu) != 0)
		return notified;

	switch (event->msgType) {
	case 7:
	case 8:
	case 80:
	case 81:
	case 82:
		return notified;
	default:
		return 0;
	}
}

/* the download's state is 5 (0x8117481C) */
bool indicatorEventIsStateFive(const LsdbNewEvent *event)
{
	return INDICATOR_EVENT_STATE(bgdlStringToInt(event->msgArg[8])) == 5;
}

/* the event runs PCSI00011 (0x81174852) */
bool indicatorEventRunsPcsi00011(const LsdbNewEvent *event)
{
	if (event != NULL) {
		string titleId("PCSI00011");

		if (sce_paf_strcmp(event->execTitleId.c_str(), titleId.c_str()) == 0)
			return true;
	}
	return false;
}

/* the download's state is 6 with its kind 2 or 3 (0x811748D4) */
bool indicatorEventIsStateSix(const LsdbNewEvent *event)
{
	if (event != NULL) {
		int status = bgdlStringToInt(event->msgArg[8]);

		if ((status & 0xF0) == 0x60 && ((status & 0x3800000) == 0x1000000 ||
						 (status & 0x3800000) == 0x1800000))
			return true;
	}
	return false;
}

/* the event runs PCSC80018 (0x81174920) */
bool indicatorEventRunsPcsc80018(const LsdbNewEvent *event)
{
	if (event != NULL) {
		string titleId("PCSC80018");

		if (sce_paf_strcmp(event->execTitleId.c_str(), titleId.c_str()) == 0)
			return true;
	}
	return false;
}

/* the background download status's state: 1 to 4 for its states 1, 2, 3 and 6, else 0
   (0x811749A2) */
int indicatorEventBgcpState(const LsdbNewEvent *event)
{
	int result = 0;

	if (indicatorEventIsBgcpStatus(event)) {
		switch (INDICATOR_EVENT_STATE(bgdlStringToInt(event->msgArg[8]))) {
		case 1:
			result = 1;
			break;
		case 2:
			result = 2;
			break;
		case 3:
			result = 3;
			break;
		case 6:
			result = 4;
			break;
		}
	}
	return result;
}

/* (0x81174A12) */
bool indicatorNoticeIsShown(const LsdbNewEvent *event)
{
	if (indicatorEventIsBgcpStatus(event))
		return false;

	bool notLog = !(event->titleId == "LOGSTATUS0");
	bool stateFive = INDICATOR_EVENT_STATE(bgdlStringToInt(event->msgArg[8])) == 5;
	int actType = event->actType;

	if ((notLog | stateFive) || (actType != 4 && actType != 6))
		return true;
	return false;
}
