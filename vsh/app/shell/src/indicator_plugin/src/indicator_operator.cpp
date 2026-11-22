/*
 * indicator_operator.cpp: the mobile network operator's name on the information bar, as the
 * telephony listener gives it, condensed to fit in its room once laid out
 * (0x81168030-0x811685C0). The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_text.h>
#include "../../../plugins.h"
#include "indicator_item.h"

using namespace paf;

/* the operator's template and its text (PLACEHOLDER names) */
#define INDICATOR_OPERATOR_TEMPLATE	0x882CCCE3
#define INDICATOR_OPERATOR_TEXT		0xB383ED84

/* a name this long or longer is shown by its short name */
#define INDICATOR_OPERATOR_NAME_MAX	12

/* the text's layout done (paf's event 0x100004, PLACEHOLDER name) */
#define INDICATOR_OPERATOR_LAYOUT_EVENT	0x100004

/* the text's shrinking's time, in milliseconds */
#define INDICATOR_OPERATOR_ANIM_TIME	200.0f

/* the name shown (0x81566934) */
static wstring s_operatorName;

static void indicatorMobileOperatorUpdate(TelephonyOperatorNames names, void *arg);
static void indicatorMobileOperatorOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e,
					    void *data);

/* the operator's template opened in the widget and its text found in it, the item told the
   operator's names (0x81168030) */
IndicatorMobileOperator::IndicatorMobileOperator(Plugin *plugin, ui::Widget *widget)
	: m_root(widget), m_plugin(plugin)
{
	if (widget != NULL) {
		{
			plugin->TemplateOpen(widget,
					     IDParam(s_empty_name, INDICATOR_OPERATOR_TEMPLATE),
					     Plugin::TemplateOpenParam());
		}
		m_widget = widget->FindChild(IDParam(s_empty_name, INDICATOR_OPERATOR_TEXT), 0);
		if (m_widget != NULL) {
			g_indicatorOperatorNameCB = indicatorMobileOperatorUpdate;
			g_indicatorOperatorNameArg = this;
		}
	}
}

/* (0x81168170) */
IndicatorMobileOperator::~IndicatorMobileOperator()
{
	g_indicatorOperatorNameCB = NULL;
	g_indicatorOperatorNameArg = NULL;
}

/*
 * The operator's names given: without a name, the text shrunk and hidden; else the name, or
 * the short name for a long one, unless it is shown already, centred and condensed off, the
 * text emptied, then given the name, put in its room by indicatorMobileOperatorOnLayout once
 * laid out (0x811681BE).
 */
static void indicatorMobileOperatorUpdate(TelephonyOperatorNames names, void *arg)
{
	IndicatorMobileOperator *item = (IndicatorMobileOperator *)arg;

	if (item == NULL)
		return;

	ui::Widget *widget = item->m_widget;

	if (widget == NULL || !widget->IsInherit(ui::Text::TypeName()))
		return;

	ui::Text *text = (ui::Text *)item->m_widget;

	if (text == NULL)
		return;
	if (names.name.length() == 0) {
		s_operatorName = names.name;
		text->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(INDICATOR_OPERATOR_ANIM_TIME),
			      ui::Widget::ANIM_SIZE, 0);
		text->Hide(0xff, NULL, NULL, 0.0f);
		return;
	}

	if (names.name.length() < INDICATOR_OPERATOR_NAME_MAX) {
		if (s_operatorName == names.name)
			return;
		s_operatorName = names.name;
	} else {
		if (s_operatorName == names.shortName)
			return;
		s_operatorName = names.shortName;
	}
	text->SetAdjust(2, text->m_adjust_y, text->m_adjust_z);
	text->SetAdjust(text->m_adjust_x, 2, text->m_adjust_z);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, false);
	text->AddEventListener(INDICATOR_OPERATOR_LAYOUT_EVENT,
			       new ui::EventCBListener(indicatorMobileOperatorOnLayout, item));
	text->SetString(wstring(L""));
	text->SetString(s_operatorName);
}

/* the text laid out: one wider than its room put at the left, condensed to the room and given
   the name again; one that fits put at the left at its size, condensed, and shown
   (0x81168416) */
static void indicatorMobileOperatorOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e,
					    void *data)
{
	math::v4 room(175.0f, 28.0f, 0.0f, 0.0f);
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
		text->SetString(s_operatorName);
	} else {
		text->DeleteEventCallback(INDICATOR_OPERATOR_LAYOUT_EVENT, NULL, NULL);
		text->SetAdjust(0, text->m_adjust_y, text->m_adjust_z);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, true);
		text->SetSize(size, NULL, ui::Widget::ANIM_SIZE, 0);
		text->Show(0xff, NULL, NULL, 0.0f);
	}
}
