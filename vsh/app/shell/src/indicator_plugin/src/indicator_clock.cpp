/*
 * indicator_clock.cpp: the bar's clock, updated every second and on the shell's time events,
 * formatting the current time with "%X" and shrinking an AM/PM suffix to 16 pt
 * (0x81183D68-0x81184484). The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/string.h>
#include <timer/timer.h>
#include <math/ngp/v4.h>
#include <datetime/datetime.h>
#include <common/encoded_string.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include "../../../shell.h"
#include "indicator_item.h"

using namespace paf;

/* the clock's template and text widget (PLACEHOLDER names) */
#define INDICATOR_CLOCK_TEMPLATE	0xB4A5EE1C
#define INDICATOR_CLOCK_WIDGET		0x89FFAD08

/* the 1-second interval timer and the shell events that refresh the clock */
#define INDICATOR_CLOCK_TIMER_ID	0x1000
#define INDICATOR_CLOCK_INTERVAL	1000.0f
#define INDICATOR_CLOCK_EVENT_LAYOUT	0x100004
#define INDICATOR_CLOCK_EVENT_PARENT	0x800009
#define INDICATOR_SHELL_EVENT_13	13
#define INDICATOR_SHELL_EVENT_14	14

/* the last time shown, kept across updates (28 bytes; PLACEHOLDER name) */
struct IndicatorClockTime {
	IndicatorClockTime() : dt(0, 0, 0, 0, 0, 0, 0), reserved(-1)
	{
	}

	~IndicatorClockTime()
	{
	}

	datetime::DateTime dt;
	int reserved;
};

/* the active clock item (0x81566BD8) and the last time it drew (0x81566BE0) */
static IndicatorClock *s_indicatorClock;
static IndicatorClockTime s_lastTime;

static void indicatorClockUpdate(ui::Widget *widget, IndicatorClockTime *lastTime);
static void indicatorClockOnShellEvent(int event, const void *data, SceSize size, void *arg);
static void indicatorClockOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void indicatorClockOnEvent(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void indicatorClockOnTimer(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);

/* (0x81183D68) */
IndicatorClock::IndicatorClock(Plugin *plugin, ui::Widget *widget)
	: IndicatorItem(NULL), m_root(widget)
{
	if (widget != NULL) {
		plugin->TemplateOpen(widget, IDParam(s_empty_name, INDICATOR_CLOCK_TEMPLATE),
				     Plugin::TemplateOpenParam());
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_CLOCK_WIDGET), 0);
	}
	s_indicatorClock = this;
}

/* (0x81183E8E) */
IndicatorClock::~IndicatorClock()
{
	if (m_widget != NULL) {
		SceShell_D4C7D2B7(INDICATOR_SHELL_EVENT_13, indicatorClockOnShellEvent, m_widget);
		SceShell_D4C7D2B7(INDICATOR_SHELL_EVENT_14, indicatorClockOnShellEvent, m_widget);
		m_widget->KillIntervalEvent(INDICATOR_CLOCK_TIMER_ID);
	}
	s_indicatorClock = NULL;
}

/* whether the clock widget is present and idle (m_state == 0) (0x81183F48) */
bool indicatorNoticeListCanUpdate(void)
{
	if (s_indicatorClock == NULL)
		return false;
	return s_indicatorClock->m_widget != NULL && s_indicatorClock->m_widget->m_state == 0;
}

/* the text updated when the date or time changed, shrinking the trailing AM/PM to 16 pt
   (0x81183F6C) */
__attribute__((noinline))
static void indicatorClockUpdate(ui::Widget *widget, IndicatorClockTime *lastTime)
{
	ui::Text *text = NULL;

	if (widget != NULL && widget->IsInherit(ui::Text::TypeName()))
		text = (ui::Text *)widget;

	datetime::DateTime now = datetime::DateTime::Now();

	if (now.m_month != lastTime->dt.m_month ||
	    now.m_day != lastTime->dt.m_day ||
	    now.m_hour != lastTime->dt.m_hour ||
	    now.m_minute != lastTime->dt.m_minute ||
	    now.m_second != lastTime->dt.m_second) {
		wchar_t buf[100];

		now.Format(buf, sizeof(buf) / sizeof(buf[0]), L"%X", false);
		text->SetString(wstring(buf), 0);

		common::EncodedString enc;

		ScePafMisc_DCBA2BD4(&enc, buf, common::EncodedString::Encoding_Utf16);
		scePafWidgetSetFontSize(text, 22.0f, graph::TextStyleAttribute_Point, 0, 0);
		if (sce_paf_private_strtok(ScePafMisc_A7C6AF5C(&enc), "M") != NULL ||
		    sce_paf_private_strtok(ScePafMisc_A7C6AF5C(&enc), "m") != NULL)
			scePafWidgetSetFontSize(text, 16.0f, graph::TextStyleAttribute_Point,
						(int32_t)ScePafMisc_183DD5C3(&enc) - 2, 2);
		lastTime->dt = now;
		ScePafMisc_B45A7521(&enc);
	}
}

/* (0x811840C8) */
__attribute__((noinline))
void IndicatorClock::Start(IndicatorClockTime *lastTime)
{
	m_widget->KillIntervalEvent(INDICATOR_CLOCK_TIMER_ID);
	m_widget->SetIntervalEvent(INDICATOR_CLOCK_TIMER_ID, INDICATOR_CLOCK_INTERVAL,
				   indicatorClockOnTimer, lastTime);
	m_widget->m_scene->AddEventListener(INDICATOR_CLOCK_EVENT_PARENT,
					    new ui::EventCBListener(indicatorClockOnEvent, m_widget),
					    true);
	indicatorClockUpdate(m_widget, lastTime);
	SceShell_7FE4E732(INDICATOR_SHELL_EVENT_13, indicatorClockOnShellEvent, m_widget);
	SceShell_7FE4E732(INDICATOR_SHELL_EVENT_14, indicatorClockOnShellEvent, m_widget);
}

/* (0x811841A8) */
void IndicatorClock::Start()
{
	Start(&s_lastTime);
}

/* the text's drop shadow enabled on the Dolce's default look (0x811841B8) */
void IndicatorClock::OnStatus(IndicatorStatus status)
{
	if (m_widget != NULL && m_widget->IsInherit(ui::Text::TypeName())) {
		ui::Text *text = (ui::Text *)m_widget;

		if (text != NULL) {
			bool dolce = indicatorIsDolceDefault();

			text->SetStyleAttribute(graph::TextStyleAttribute_Shadow, 0, 0, dolce);
			if (dolce)
				text->SetStyleAttribute(graph::TextStyleAttribute_ShadowColor,
							0, 0, math::v4(0.0f, 0.0f, 0.0f, 0.2f));
		}
	}
}

/* (0x81184238) */
static void indicatorClockOnShellEvent(int event, const void *data, SceSize size, void *arg)
{
	indicatorClockUpdate((ui::Widget *)arg, &s_lastTime);
}

/* (0x8118424A) */
static void indicatorClockOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ui::Widget *widget = (ui::Widget *)data;

	if (widget != NULL && widget->IsInherit(ui::Text::TypeName())) {
		widget->DeleteEventCallback(INDICATOR_CLOCK_EVENT_LAYOUT, indicatorClockOnLayout,
					    widget);
		widget->Show(0xff, new Timer(100.0f), NULL, 0.0f);
	}
}

/* (0x81184318) */
static void indicatorClockOnEvent(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ui::Widget *widget = (ui::Widget *)data;

	if (widget != NULL && widget->IsInherit(ui::Text::TypeName())) {
		if (widget->m_hold_count <= 0)
			widget->ChangeState(widget->CheckState(), -1);
		if (widget->m_state != 0) {
			widget->Hide(0xff, new Timer(100.0f), NULL, 0.0f);
			widget->AddEventListener(INDICATOR_CLOCK_EVENT_LAYOUT,
						 new ui::EventCBListener(indicatorClockOnLayout,
									 widget),
						 true);
		}
	}
}

/* (0x81184450) */
static void indicatorClockOnTimer(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	indicatorClockUpdate((ui::Widget *)self, (IndicatorClockTime *)data);
}
