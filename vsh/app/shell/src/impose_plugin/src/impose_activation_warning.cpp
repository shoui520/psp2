/*
 * impose_activation_warning.cpp: impose_plugin's warning on a development or testing kit
 * (0x8114F348-0x8114FD14): a red line when the kit is not activated or its activation has
 * expired, an orange count-down in its last three days, and a notice before that; and the
 * manufacturing mode's line. The file's and the names are ours (PLACEHOLDER), after the
 * notice's name "ActivationWarning".
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/stdio.h>
#include <versatileparam/idparam.h>
#include <rtc/rtc.h>
#include <math/math.h>
#include <widget/core/widget.h>
#include <widget/w_text.h>
#include <graphics/text/text_style.h>
#include <vshbridge/vshbridge.h>
#include <post_ss_mgr/sblpmmgr.h>
#include <post_ss_mgr/sbllicmgr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../plugins.h"
#include "impose.h"

using namespace paf;

/* the warning's page (PLACEHOLDER name) */
#define ACTIVATION_PAGE			0x8DF78D6E

/* what sceSblLicMgrGetUsageTimeLimit returns for a kit not activated, a failed backup battery
   and a clock not set (PLACEHOLDER names) */
#define ACTIVATION_ERROR_NOT_ACTIVATED	((int)0x800F1326)
#define ACTIVATION_ERROR_BATTERY	((int)0x800F1329)
#define ACTIVATION_ERROR_NO_DATE	((int)0x80251002)

/* the time left from which on the count-down is kept: two weeks, in seconds */
#define ACTIVATION_COUNT_TIME		(14 * 24 * 60 * 60)
/* the days left from which on the count-down is shown on the screen */
#define ACTIVATION_SHOW_DAYS		3

#define SECONDS_PER_DAY			(24 * 60 * 60)
#define SECONDS_PER_HOUR		(60 * 60)
#define SECONDS_PER_MINUTE		60
#define TICKS_PER_SECOND		1000000

/* the notice's icon (PLACEHOLDER name) */
#define ACTIVATION_NOTICE_ICON		0x2617A32D

/* (0x81564E08) */
ImposeActivationWarning *g_imposeActivationWarning;

/* PLACEHOLDER names: the lines' background colours, red for an error and orange for the
   count-down (0x81564E10, 0x81564E20) */
static math::v4 s_errorColor(1.0f, 0.0f, 0.0f, 1.0f);
static math::v4 s_countColor(1.0f, 0.65f, 0.0f, 1.0f);

/* (0x8114F348) */
ImposeActivationWarning::ImposeActivationWarning(Plugin *plugin)
	: m_plugin(plugin), m_page(NULL), m_text(NULL), m_counting(false), m_timeLeft(0),
	  m_tick(0), m_timeLimit(0), m_startTick(0), m_isTestingKit(vshSblAimgrIsDEX())
{
	g_imposeActivationWarning = this;
}

/* (0x8114F380) */
ImposeActivationWarning::~ImposeActivationWarning()
{
	g_imposeActivationWarning = NULL;
}

/* the line made the first time, with its background colour (0x8114F390) */
void ImposeActivationWarning::SetMessage(const wchar_t *message, const math::v4 &color)
{
	if (m_text == NULL) {
		m_text = new ui::Text(m_page, NULL);
		m_text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
					  math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		m_text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
		scePafWidgetSetFontSize(m_text, 20.0f, graph::TextStyleAttribute_Point, 0, 0);
		m_text->SetAdjust(2, 2, 0);
		m_text->SetAlign(1, 0, 0, NULL);
		m_text->SetPos(math::v4(-465.0f, -252.0f, 0.0f, 0.0f));
	}
	m_text->SetStyleAttribute(graph::TextStyleAttribute_BackColor, 0, 0, color);
	m_text->SetString(wstring(message));
}

/* the page opened; the manufacturing mode's line, and on a kit its activation's state
   (0x8114F4BE) */
void ImposeActivationWarning::Open()
{
	Plugin::PageOpenParam param;
	SceUInt8 mode;
	SceUInt32 timeLeft;

	param.overwrite_draw_priority = 6;
	m_page = m_plugin->PageOpen(IDParam(ACTIVATION_PAGE), param);
	if (sceSblPmMgrGetCurrentMode(&mode) == 0 && mode == 1) {
		ui::Text *text = new ui::Text(m_page, NULL);

		text->SetString(wstring(L"manufacturing mode"));
		text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
					math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		text->SetStyleAttribute(graph::TextStyleAttribute_BackColor, 0, 0,
					math::v4(1.0f, 0.0f, 0.0f, 1.0f));
		text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
		scePafWidgetSetFontSize(text, 20.0f, graph::TextStyleAttribute_Point, 0, 0);
		text->SetSize(math::v4(200.0f, 30.0f, 0.0f, 0.0f));
		text->SetPos(math::v4(370.0f, -252.0f, 0.0f, 0.0f));
	}
	if (vshSblAimgrIsTool() == 1 || vshSblAimgrIsDEX() == 1) {
		int ret = sceSblLicMgrGetUsageTimeLimit(&timeLeft);
		wstring message;
		math::v4 color = s_errorColor;

		if (ret == ACTIVATION_ERROR_NOT_ACTIVATED) {
			if (m_isTestingKit)
				message = wstring(L"This testing kit is not activated.");
			else
				message = wstring(L"This development kit is not activated.");
		} else if (ret == ACTIVATION_ERROR_BATTERY) {
			message = wstring(L"The backup battery has failed.");
		} else if (ret == ACTIVATION_ERROR_NO_DATE) {
			message = wstring(L"Cannot check expiration date. Please set date via Internet.");
		}
		if (ret == 0) {
			if (timeLeft == 0) {
				if (m_isTestingKit)
					message = wstring(L"This Testing Kit is expired. See DevKit/TestKit "
							  L"Activation User's Guide.");
				else
					message = wstring(L"This Development Kit is expired. See "
							  L"DevKit/TestKit Activation User's Guide.");
			} else {
				m_counting = true;
				m_timeLeft = timeLeft;
				rtc::GetCurrentTick(&m_tick);
				m_timeLimit = m_timeLeft;
				m_startTick = m_tick;
				if (timeLeft < ACTIVATION_COUNT_TIME) {
					SceUInt64 left = timeLeft;
					unsigned int days = left / SECONDS_PER_DAY;
					unsigned int hours;
					unsigned int minutes;
					unsigned int seconds;
					wchar_t buf[1024];

					left -= days * SECONDS_PER_DAY;
					hours = left / SECONDS_PER_HOUR;
					left -= hours * SECONDS_PER_HOUR;
					minutes = left / SECONDS_PER_MINUTE;
					seconds = left - minutes * SECONDS_PER_MINUTE;
					if (days < ACTIVATION_SHOW_DAYS) {
						if (m_isTestingKit)
							sce_paf_swprintf(buf, sizeof(buf),
								L"This testing kit expires in %2d day "
								L"+%02d:%02d:%02d", days, hours,
								minutes, seconds);
						else
							sce_paf_swprintf(buf, sizeof(buf),
								L"This development kit expires in %2d day "
								L"+%02d:%02d:%02d", days, hours,
								minutes, seconds);
						color = s_countColor;
						message = buf;
					} else {
						ImposeNotice notice;

						if (m_isTestingKit)
							sce_paf_swprintf(buf, sizeof(buf),
								L"This testing kit expires in %2d day\n"
								L"+%02d:%02d:%02d.", days, hours,
								minutes, seconds);
						else
							sce_paf_swprintf(buf, sizeof(buf),
								L"This development kit expires in %2d day\n"
								L"+%02d:%02d:%02d.", days, hours,
								minutes, seconds);
						notice.name = "ActivationWarning";
						notice.message = buf;
						notice.iconHash = ACTIVATION_NOTICE_ICON;
						imposeNotificationShowNotice(&notice);
					}
				}
			}
		}
		if (!message.empty())
			SetMessage(message.c_str(), color);
	}
}

/* each frame: the count-down's line, the expired line when the time is up (0x8114FAC4) */
void ImposeActivationWarning::Update()
{
	if (m_counting) {
		rtc::Tick now;
		SceUInt64 timeLeft;

		rtc::GetCurrentTick(&now);
		timeLeft = m_timeLimit - (now - m_startTick) / TICKS_PER_SECOND;
		if (m_timeLeft != timeLeft) {
			SceUInt64 left = timeLeft;
			unsigned int days;
			unsigned int hours;
			unsigned int minutes;
			unsigned int seconds;

			m_tick = now;
			m_timeLeft = timeLeft;
			days = left / SECONDS_PER_DAY;
			left -= days * SECONDS_PER_DAY;
			hours = left / SECONDS_PER_HOUR;
			left -= hours * SECONDS_PER_HOUR;
			minutes = left / SECONDS_PER_MINUTE;
			seconds = left - minutes * SECONDS_PER_MINUTE;
			if (days < ACTIVATION_SHOW_DAYS) {
				wchar_t buf[1024];

				if (m_isTestingKit)
					sce_paf_swprintf(buf, sizeof(buf),
						L"This testing kit expires in %2d day +%02d:%02d:%02d",
						days, hours, minutes, seconds);
				else
					sce_paf_swprintf(buf, sizeof(buf),
						L"This development kit expires in %2d day "
						L"+%02d:%02d:%02d", days, hours, minutes, seconds);
				SetMessage(buf, s_countColor);
			}
		}
		if (m_timeLeft == 0) {
			if (m_isTestingKit)
				SetMessage(L"This testing kit is expired.See Testing Kit Activation "
					   L"Guide.", s_errorColor);
			else
				SetMessage(L"This development kit is expired.See Development Kit "
					   L"Activation Guide.", s_errorColor);
			m_counting = false;
		}
	}
}
