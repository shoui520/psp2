/*
 * indicator_label.cpp: a text on the information bar, the plugin's string, made to fit in its
 * room once it is laid out (0x81161548-0x81161A80). The file's and the names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_text.h>
#include "indicator.h"

using namespace paf;

/* the label's template, its text and the string it shows (PLACEHOLDER names) */
#define INDICATOR_LABEL_TEMPLATE	0x3E8F6B7E
#define INDICATOR_LABEL_TEXT		0xFC7100D8
#define INDICATOR_LABEL_STRING		0xA3C092EA

/* the text's layout done (paf's event 0x100004, PLACEHOLDER name) */
#define INDICATOR_LABEL_LAYOUT_EVENT	0x100004

static void indicatorLabelOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);

/* the label's template opened in the widget, and its text found in it (0x81161548) */
IndicatorLabel::IndicatorLabel(Plugin *plugin, ui::Widget *widget)
	: m_root(widget)
{
	if (widget != NULL) {
		bool opened = plugin->TemplateOpen(widget, IDParam(s_empty_name, INDICATOR_LABEL_TEMPLATE),
						   Plugin::TemplateOpenParam()) == 0;

		if (opened)
			m_text = widget->FindChild(IDParam(s_empty_name, INDICATOR_LABEL_TEXT), 0);
	}
}

/* the text hidden, centred and emptied, then given the string, shown again by
   indicatorLabelOnLayout once laid out (0x81161668) */
__attribute__((noinline))
static void indicatorLabelUpdate(int result, int value, IndicatorLabel *label)
{
	if (label == NULL)
		return;

	ui::Widget *widget = label->m_text;

	if (widget == NULL || !widget->IsInherit(ui::Text::TypeName()))
		return;

	ui::Text *text = (ui::Text *)label->m_text;

	if (text == NULL)
		return;
	text->Hide(0xff, NULL, NULL, 0.0f);
	text->SetAdjust(2, text->m_adjust_y, text->m_adjust_z);
	text->SetAdjust(text->m_adjust_x, 2, text->m_adjust_z);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, false);
	text->AddEventListener(INDICATOR_LABEL_LAYOUT_EVENT,
			       new ui::EventCBListener(indicatorLabelOnLayout, label));
	text->SetString(wstring(L""));
	text->SetString(wstring(g_indicatorPlugin->GetString(
		IDParam(s_empty_name, INDICATOR_LABEL_STRING))));
}

/* the text laid out: one wider than its room put at the left, condensed to the room and given
   the string again; one that fits put at the left at its size, condensed, and shown (0x81161870) */
static void indicatorLabelOnLayout(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
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
		text->SetString(wstring(g_indicatorPlugin->GetString(
			IDParam(s_empty_name, INDICATOR_LABEL_STRING))));
	} else {
		text->DeleteEventCallback(INDICATOR_LABEL_LAYOUT_EVENT, NULL, NULL);
		text->SetAdjust(0, text->m_adjust_y, text->m_adjust_z);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_CondenseX, true);
		text->SetSize(size, NULL, ui::Widget::ANIM_SIZE, 0);
		text->Show(0xff, NULL, NULL, 0.0f);
	}
}

/* the label updated while the counters are not shown (0x81161A6E) */
void IndicatorLabel::OnStatus(IndicatorStatus status)
{
	if (status.value6 == 0)
		indicatorLabelUpdate(0, status.bluetooth, this);
}
