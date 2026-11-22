/*
 * indicator_sim.cpp: the SIM card's condition on the information bar, shown in the operator's
 * text as the telephony listener gives it, condensed to fit in its room once laid out
 * (0x8116C804-0x8116D398). The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include "indicator_item.h"

using namespace paf;

/* the operator's text the condition is shown in (indicator_operator.cpp's) */
#define INDICATOR_SIM_TEXT		0xB383ED84

/* the conditions' strings (PLACEHOLDER names) */
#define INDICATOR_SIM_STRING_2		0x8E43E62F
#define INDICATOR_SIM_STRING_3		0xC01F4922
#define INDICATOR_SIM_STRING_5		0xECDD2148

/* the scene's event the item starts on, and the text's layout done (paf's events, PLACEHOLDER
   names) */
#define INDICATOR_SIM_SCENE_EVENT	0x800008
#define INDICATOR_SIM_LAYOUT_EVENT	0x100004

/* the text's shrinking's time, in milliseconds */
#define INDICATOR_SIM_ANIM_TIME		200.0f

/* the condition last given, -1 before the first (0x81544A48) */
static int s_simCondition = -1;
/* the status's value6, as it was told last (0x81544A4C) */
static bool s_simStatusShown = true;
/* the status's flight mode, as it was told last (0x81566978) */
static bool s_simFlightMode;

static void indicatorMobileSimUpdate(int condition, void *arg);
static void indicatorMobileSimOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e,
				       void *data);
static void indicatorMobileSimOnScene(SceInt32 type, ui::Handler *self, ui::Event *e,
				      void *data);

/* the operator's text found in the widget, the item told the SIM card's conditions, and the
   scene's start (0x8116C804) */
IndicatorMobileSim::IndicatorMobileSim(Plugin *plugin, ui::Widget *widget)
	: m_root(widget), m_plugin(plugin)
{
	if (widget != NULL) {
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_SIM_TEXT), 0);
		if (m_widget != NULL) {
			g_indicatorSimConditionCB = indicatorMobileSimUpdate;
			g_indicatorSimConditionArg = this;
			m_widget->m_scene->AddEventListener(INDICATOR_SIM_SCENE_EVENT,
							    new ui::EventCBListener(
								    indicatorMobileSimOnScene, this));
		}
	}
}

/* (0x8116C948) */
IndicatorMobileSim::~IndicatorMobileSim()
{
	g_indicatorSimConditionCB = NULL;
	g_indicatorSimConditionArg = NULL;
}

/* the condition's string: none for 0, 1 and any other (0x8116C96C) */
static const wchar_t *indicatorMobileSimGetString(int condition)
{
	switch (condition) {
	case 0:
	case 1:
		return L"";
	case 2:
		return g_indicatorPlugin->GetString(IDParam(s_empty_name, INDICATOR_SIM_STRING_2));
	case 3:
	case 4:
		return g_indicatorPlugin->GetString(IDParam(s_empty_name, INDICATOR_SIM_STRING_3));
	case 5:
		return g_indicatorPlugin->GetString(IDParam(s_empty_name, INDICATOR_SIM_STRING_5));
	default:
		return L"";
	}
}

/*
 * A condition given (106: only the text hidden): while the status is in the flight mode or not
 * shown, the text is shrunk and hidden; else for a condition with a string the text is hidden,
 * centred, condensed off, emptied and given the string, put in its room by
 * indicatorMobileSimOnLayout once laid out (0x8116CAF4).
 */
static void indicatorMobileSimUpdate(int condition, void *arg)
{
	IndicatorMobileSim *item = (IndicatorMobileSim *)arg;

	if (condition != INDICATOR_SIGNAL_HIDE)
		s_simCondition = condition;
	if (item != NULL && (s_simFlightMode || !s_simStatusShown)) {
		item->m_widget->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
					new Timer(INDICATOR_SIM_ANIM_TIME), ui::Widget::ANIM_SIZE, 0);
		item->m_widget->Hide(0xff, new Timer(INDICATOR_SIM_ANIM_TIME), NULL, 0.0f);
	} else if (condition != INDICATOR_SIGNAL_HIDE && condition != 0 && s_simCondition != -1 &&
		   item != NULL) {
		ui::Widget *widget = item->m_widget;

		if (widget == NULL || !widget->IsInherit(ui::Widget::TypeName()) ||
		    item->m_widget == NULL || !item->m_widget->IsInherit(ui::Text::TypeName()))
			return;

		ui::Text *text = (ui::Text *)item->m_widget;

		if (text == NULL)
			return;
		text->Hide(0xff, NULL, NULL, 0.0f);
		text->SetAdjust(2, text->m_adjust_y, text->m_adjust_z);
		text->SetAdjust(text->m_adjust_x, 2, text->m_adjust_z);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, false);
		text->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
		text->SetString(wstring(indicatorMobileSimGetString(condition)));
		text->AddEventListener(INDICATOR_SIM_LAYOUT_EVENT,
				       new ui::EventCBListener(indicatorMobileSimOnLayout, item), true);
	}
}

/* the text laid out: one wider than its room put at the left, condensed to the room and given
   the string again; one that fits put at the left at its size, condensed, and shown, unless the
   status is in the flight mode or not shown, which shrinks and hides it (0x8116CE40) */
static void indicatorMobileSimOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e,
				       void *data)
{
	IndicatorMobileSim *item = (IndicatorMobileSim *)data;
	math::v4 room(220.0f, 28.0f, 0.0f, 0.0f);
	ui::Text *text = NULL;

	if (self != NULL && ((ui::Widget *)self)->IsInherit(ui::Text::TypeName()))
		text = (ui::Text *)self;

	math::v4 size = *text->GetSize(0);

	if (size.extract_x() == 0.0f)
		return;
	if (size.extract_x() > room.extract_x()) {
		text->SetAdjust(0, text->m_adjust_y, text->m_adjust_z);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, true);
		text->SetSize(room, NULL, ui::Widget::ANIM_SIZE, 0);
		text->SetString(wstring(indicatorMobileSimGetString(s_simCondition)));
	} else {
		if (!s_simFlightMode && s_simStatusShown) {
			text->SetAdjust(0, text->m_adjust_y, text->m_adjust_z);
			text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, true);
			text->SetSize(size, NULL, ui::Widget::ANIM_SIZE, 0);
			text->Show(0xff, NULL, NULL, 0.0f);
		} else {
			item->m_widget->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
						new Timer(INDICATOR_SIM_ANIM_TIME),
						ui::Widget::ANIM_SIZE, 0);
			item->m_widget->Hide(0xff, new Timer(INDICATOR_SIM_ANIM_TIME), NULL, 0.0f);
		}
	}
}

/* the scene started: a condition with a string given already is shown in the text as
   indicatorMobileSimUpdate shows it (0x8116D13E) */
static void indicatorMobileSimOnScene(SceInt32 type, ui::Handler *self, ui::Event *e,
				      void *data)
{
	IndicatorMobileSim *item = (IndicatorMobileSim *)data;

	if (s_simCondition == 1 || s_simCondition == -1)
		return;

	ui::Widget *widget = item->m_widget;

	if (widget == NULL || !widget->IsInherit(ui::Widget::TypeName()) || item->m_widget == NULL)
		return;

	ui::Text *text = NULL;

	if (item->m_widget->IsInherit(ui::Text::TypeName()))
		text = (ui::Text *)item->m_widget;

	text->Hide(0xff, NULL, NULL, 0.0f);
	text->SetAdjust(2, text->m_adjust_y, text->m_adjust_z);
	text->SetAdjust(text->m_adjust_x, 2, text->m_adjust_z);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, false);
	text->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE, 0);
	text->SetString(wstring(indicatorMobileSimGetString(s_simCondition)));
	text->AddEventListener(INDICATOR_SIM_LAYOUT_EVENT,
			       new ui::EventCBListener(indicatorMobileSimOnLayout, item), true);
}

/* the status's flight mode or value6 changed: the text hidden while either keeps it away, and
   the last condition given back once neither does (0x8116D2F8) */
void IndicatorMobileSim::OnStatus(IndicatorStatus status)
{
	if (s_simFlightMode != (status.flightMode == 1) ||
	    s_simStatusShown != (status.value6 == 1)) {
		s_simFlightMode = status.flightMode == 1;
		s_simStatusShown = status.value6 == 1;
		if (s_simFlightMode || !s_simStatusShown)
			indicatorMobileSimUpdate(INDICATOR_SIGNAL_HIDE, this);
		if (!s_simFlightMode || s_simStatusShown)
			indicatorMobileSimUpdate(s_simCondition, this);
	}
}
