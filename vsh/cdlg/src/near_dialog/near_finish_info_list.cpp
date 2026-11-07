/*
 * The check-in's end screen in the near dialog's contents: a title, then lines of a label and its
 * value. The texts stay hidden until each has its size; then the labels' column and the values'
 * column are each centred on their widest text, and the texts fade in. No Sony record names the
 * file, the class or its functions ("SceNearFinishInfoList" is the name the dialog gives the list's
 * widgets; the rest is ours).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <math/math.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_box.h>
#include <widget/w_text_box.h>
#include "near_dialog_plugin.h"

using namespace paf;

/* the list's template and the box it opens in the parent */
#define NEAR_FINISH_INFO_LIST_TEMPLATE		0xFF95C07D
#define NEAR_FINISH_INFO_LIST_BASE_BOX		0x43F18209

/* a line's template, its label, its value and the box that holds them */
#define NEAR_FINISH_INFO_LIST_LINE_TEMPLATE	0xF4A6636F
#define NEAR_FINISH_INFO_LIST_LINE_TITLE	0xF2D1053D
#define NEAR_FINISH_INFO_LIST_LINE_VALUE	0xD13759CC
#define NEAR_FINISH_INFO_LIST_LINE_BOX		0xF06A6B9E

/* the title's template and its text */
#define NEAR_FINISH_INFO_LIST_TITLE_TEMPLATE	0x2A8A77C3
#define NEAR_FINISH_INFO_LIST_TITLE_TEXT	0xC6FC31BE

/* the title's font size */
#define NEAR_FINISH_INFO_LIST_TITLE_FONT_SIZE	28.0f

/* how long the texts fade in (ms) */
#define NEAR_FINISH_INFO_LIST_FADE_TIME		200.0f

/* the widget of the parent with the ID, when it is of the type */
#define NEAR_FINISH_INFO_LIST_FIND(parent, type, id)					\
	((parent)->FindChild(IDParam(s_empty_name, (id)), 0) != NULL &&			\
	 (parent)->FindChild(IDParam(s_empty_name, (id)), 0)->IsInherit(type::TypeName()) ?	\
	 (type *)(parent)->FindChild(IDParam(s_empty_name, (id)), 0) : NULL)

/* names a widget with the list's name and the suffix */
static inline void SetWidgetName(ui::Widget *widget, const char *name, const char *suffix)
{
	string prefix(name);

	widget->m_name = IDParam(prefix + suffix);
}

/* shows and hides a widget over the timer's time (inline in paf's callers) */
static inline void ShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

NearFinishInfoList::NearFinishInfoList(const Param &param)
	: m_plugin(param.plugin), m_parent(param.parent), m_box(NULL), m_type(param.type),
	  m_fontSize(param.fontSize), m_valid(true)
{
	if (param.parent == NULL || param.plugin == NULL)
		return;
	m_widgets.clear();
	m_titles.clear();
	m_lines.clear();
	switch (m_type) {
	case 0:
		if (m_plugin->TemplateOpen(param.parent,
					   IDParam(s_empty_name, NEAR_FINISH_INFO_LIST_TEMPLATE),
					   Plugin::TemplateOpenParam()) != SCE_OK)
			return;
		m_box = NEAR_FINISH_INFO_LIST_FIND(param.parent, ui::Box,
						   NEAR_FINISH_INFO_LIST_BASE_BOX);
		break;
	case 1:
	case 2:
		break;
	default:
		return;
	}
	if (m_box != NULL)
		SetWidgetName(m_box, param.name, "_base_box");
}

/* adds the line of the label and the value in the box, or makes it the box's when the list has
   their names already */
int NearFinishInfoList::AddLine(ui::Widget *box, ui::Widget *title, ui::Widget *value)
{
	SceUInt32 titleId;
	SceUInt32 valueId;

	if (title == NULL || value == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	titleId = title->m_name.m_idhash;
	if (titleId == 0)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	valueId = value->m_name.m_idhash;
	if (valueId == 0)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	for (vector<Line>::iterator it = m_lines.begin(); it != m_lines.end(); ++it) {
		if (it->titleId == titleId && it->valueId == valueId) {
			it->titleWidth = 0.0f;
			it->valueWidth = 0.0f;
			it->title = title;
			it->value = value;
			it->titleSized = 0;
			it->valueSized = 0;
			it->box = box;
			return SCE_OK;
		}
	}

	Line line = { titleId, valueId, 0.0f, 0.0f, title, value, 0, 0, box };

	m_lines.push_back(line);
	return SCE_OK;
}

/* what a text calls once it has its size: when every text of the list has one, both columns are
   centred on their widest text, and the texts fade in */
static void NearFinishInfoListSizeHandler(int32_t type, ui::Handler *self, ui::Event *e,
					  void *userdata)
{
	NearFinishInfoList *list = (NearFinishInfoList *)userdata;
	ui::Widget *widget = (ui::Widget *)self;
	vector<NearFinishInfoList::Line>::iterator it;
	SceUInt32 id;
	int sized;

	if (list == NULL || widget == NULL)
		return;
	id = widget->m_name.m_idhash;
	for (it = list->m_lines.begin(); it != list->m_lines.end(); ++it) {
		if (it->titleId == id) {
			it->titleWidth = it->title->GetSize(0)->extract_x();
			it->titleSized = 1;
			break;
		}
		if (it->valueId == id) {
			it->valueWidth = it->value->GetSize(0)->extract_x();
			it->valueSized = 1;
			break;
		}
	}

	sized = 0;
	for (it = list->m_lines.begin(); it != list->m_lines.end(); ++it) {
		if (it->titleSized != 0 && it->valueSized != 0)
			sized++;
	}
	if (list->m_lines.size() != sized)
		return;

	float titleWidth = 0.0f;
	float valueWidth = 0.0f;

	for (it = list->m_lines.begin(); it != list->m_lines.end(); ++it) {
		if (titleWidth < it->titleWidth)
			titleWidth = it->titleWidth;
		if (valueWidth < it->valueWidth)
			valueWidth = it->valueWidth;
	}
	for (it = list->m_lines.begin(); it != list->m_lines.end(); ++it) {
		if (it->titleWidth < titleWidth) {
			float offset = titleWidth - it->titleWidth;
			math::v4 pos = it->box->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos.set_x(pos.extract_x() + offset * 0.5f);
			it->box->SetPos(pos);
		}
		if (it->valueWidth < valueWidth) {
			float offset = valueWidth - it->valueWidth;
			math::v4 pos = it->box->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos.set_x(pos.extract_x() - offset * 0.5f);
			it->box->SetPos(pos);
		}
		it->title->SetMetaAlpha(0.0f);
		it->title->SetMetaAlpha(1.0f, new Timer(NEAR_FINISH_INFO_LIST_FADE_TIME));
		ShowWidget(it->title, NULL);
		it->value->SetMetaAlpha(0.0f);
		it->value->SetMetaAlpha(1.0f, new Timer(NEAR_FINISH_INFO_LIST_FADE_TIME));
		ShowWidget(it->value, NULL);
	}
	list->m_lines.clear();
}

/* the line of the label and the value, its widgets named with the name */
int NearFinishInfoList::AddItem(const wstring &title, const wstring &value, const char *name)
{
	ui::Widget *titleText;
	ui::Widget *valueText;
	ui::Widget *box;

	if (m_box == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	if (m_plugin->TemplateOpen(m_box, IDParam(s_empty_name, NEAR_FINISH_INFO_LIST_LINE_TEMPLATE),
				   Plugin::TemplateOpenParam()) != SCE_OK)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;

	titleText = NEAR_FINISH_INFO_LIST_FIND(m_box, ui::Text, NEAR_FINISH_INFO_LIST_LINE_TITLE);
	if (titleText == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	titleText->SetString(title);
	SetWidgetName(titleText, name, "_left");
	scePafWidgetSetFontSize(titleText, m_fontSize, graph::TextStyleAttribute_Point, 0, 0);
	titleText->AddEventListener(ui::Handler::CB_STATE_READY,
				    new ui::EventCBListener(NearFinishInfoListSizeHandler, this),
				    true);
	HideWidget(titleText, NULL);

	valueText = NEAR_FINISH_INFO_LIST_FIND(m_box, ui::Text, NEAR_FINISH_INFO_LIST_LINE_VALUE);
	if (valueText == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	valueText->SetString(value);
	SetWidgetName(valueText, name, "_right");
	scePafWidgetSetFontSize(valueText, m_fontSize, graph::TextStyleAttribute_Point, 0, 0);
	valueText->AddEventListener(ui::Handler::CB_STATE_READY,
				    new ui::EventCBListener(NearFinishInfoListSizeHandler, this),
				    true);
	HideWidget(valueText, NULL);

	box = NEAR_FINISH_INFO_LIST_FIND(m_box, ui::Box, NEAR_FINISH_INFO_LIST_LINE_BOX);
	if (box == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	SetWidgetName(box, name, "_box");

	int ret = AddLine(box, titleText, valueText);

	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

/* the list's title, its text named with the name and as wide as the width */
int NearFinishInfoList::AddTitle(const wstring &title, const char *name, float width)
{
	ui::Widget *text;

	if (m_box == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	if (m_plugin->TemplateOpen(m_box, IDParam(s_empty_name, NEAR_FINISH_INFO_LIST_TITLE_TEMPLATE),
				   Plugin::TemplateOpenParam()) != SCE_OK)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;

	text = NEAR_FINISH_INFO_LIST_FIND(m_box, ui::Text, NEAR_FINISH_INFO_LIST_TITLE_TEXT);
	if (text == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	text->SetString(title);
	SetWidgetName(text, name, "_title");
	scePafWidgetSetFontSize(text, NEAR_FINISH_INFO_LIST_TITLE_FONT_SIZE,
				graph::TextStyleAttribute_Point, 0, 0);

	math::v4 size = *text->GetSize(0);

	size.set_x(width);
	text->SetSize(size);
	return SCE_OK;
}
