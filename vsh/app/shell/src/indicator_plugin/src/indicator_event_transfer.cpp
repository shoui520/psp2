/*
 * indicator_event_transfer.cpp: the item of a transfer in the notices' list: its progress bar
 * updated by a job while the transfer runs, its texts, and its menu, whose one command cancels
 * the transfer once the question is answered (0x8117587C-0x81177C44). The file's and all names
 * here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <common/job_slots.h>
#include <common/char_encoding.h>
#include <common/string_util.h>
#include <job/job.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_button.h>
#include <widget/w_composite_drawer.h>
#include <widget/w_progress_bar.h>
#include <graphics/drawobj/text_obj.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../popup_list.h"
#include "../../livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* the menu's IDs' push_back, out of line as Sony calls it (0x8141CE5C) */
PAF_VECTOR_NOINLINE_PUSH_BACK(IDParam, allocator<IDParam>)

/* the bar's timer that reads the progress, and its interval in seconds */
#define INDICATOR_TRANSFER_TIMER		0x400
#define INDICATOR_TRANSFER_INTERVAL		10.0f

/* the dialog's button that cancels the transfer */
#define INDICATOR_TRANSFER_BUTTON_YES		4

/* the string shown while the transfer is cancelled (its ID's hash) */
#define INDICATOR_TRANSFER_CANCELLING		0x632B6733

/* the transfer's commands */
#define INDICATOR_TRANSFER_CMD_CANCEL		0

/* the menu's items (0x81566A08) and the transfers' commands' slots (0x81566A18) */
static vector<IDParam> s_menuIds((allocator<IDParam>()));
static common::JobSlots s_controlSlots;

static void indicatorTransferOnTimer(int32_t type, ui::Handler *self, ui::Event *e, void *data);
static void indicatorTransferOnMenuClosed(int result, void *arg);
static void indicatorTransferOnMenuButton(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data);
static void indicatorTransferOnCancelAnswered(int result, int button, void *arg);

/* a command on the transfer of the event, run on the new events' queue (60 bytes) */
class Job_TaskControl : public common::SlotJobItem {
public:
	Job_TaskControl(LsdbNewEventPtr event)
		: common::SlotJobItem("Job_TaskControl", NULL), m_command(INDICATOR_TRANSFER_CMD_CANCEL),
		  m_event(event), reserved(0)
	{
	}

	virtual ~Job_TaskControl()
	{
	}

	/* the transfer removed under the busy dialog */
	virtual int Run()
	{
		if (IsCanceled())
			return -1;
		if (m_command == INDICATOR_TRANSFER_CMD_CANCEL) {
			indicatorBusyDialogOpen(
				indicatorGetString(IDParam(s_empty_name, INDICATOR_TRANSFER_CANCELLING)),
				true);
			indicatorTaskRemove(m_event->rowid);
			indicatorBusyDialogClose(true);
		}
		return 0;
	}

	virtual void Finish(int result)
	{
		m_event.reset();
		common::SlotJobItem::Finish(result);
	}

	string m_param;
	int m_command;
	LsdbNewEventPtr m_event;
	int reserved;
};

/* the transfer's progress read on the new events' queue and shown on its item (80 bytes) */
class Job_UpdateProgress : public common::SlotJobItem {
public:
	Job_UpdateProgress(ui::Widget *widget, LsdbNewEventPtr event,
			   IndicatorEventTransferItem *item)
		: common::SlotJobItem("Job_UpdateProgress", NULL),
		  m_bar(widget != NULL && widget->IsInherit(ui::ProgressBar::TypeName()) ?
				(ui::ProgressBar *)widget : NULL),
		  m_percent(s_empty_name), m_progress(0.0f), m_event(event), reserved(0),
		  m_item(item)
	{
	}

	virtual ~Job_UpdateProgress()
	{
	}

	/* the progress read, the transfer removed when it is gone */
	virtual int Run()
	{
		if (IsCanceled())
			return -1;
		if (indicatorTaskGetProgress(m_event->rowid, &m_progress, &m_status, &m_percent) != 0)
			indicatorTaskRemove(m_event->rowid);
		return 0;
	}

	virtual void Finish(int result)
	{
		if (!IsCanceled() && m_item != NULL)
			m_item->SetProgress(m_progress, m_status, m_percent);
		m_event.reset();
		common::SlotJobItem::Finish(result);
	}

	ui::ProgressBar *m_bar;
	string m_status;
	string m_percent;
	float m_progress;
	LsdbNewEventPtr m_event;
	int reserved;
	IndicatorEventTransferItem *m_item;
};

/* the bar's timer set while the transfer runs; else the progress's job stopped, the timer
   killed and the bar hidden (0x8117587C) */
void IndicatorEventTransferItem::UpdateTimer(ui::Widget *bar)
{
	if (bar == NULL)
		return;

	if (IsTransferring()) {
		bar->SetIntervalEvent(INDICATOR_TRANSFER_TIMER, INDICATOR_TRANSFER_INTERVAL,
				      indicatorTransferOnTimer, this);
	} else {
		m_updateSlots.CancelAll();
		bar->KillIntervalEvent(INDICATOR_TRANSFER_TIMER);
		bar->Hide(0xff, new Timer(20.0f), NULL, 0.0f);
	}
}

/* the menu's button put on the item of a transfer under way in the list (0x811759A0) */
int IndicatorEventTransferItem::AttachMenu(ui::Widget *item)
{
	int ret = -1;

	if (GetMode() == 1) {
		if (IsTransferring()) {
			g_indicatorPlugin->TemplateOpen(
				item, IDParam(s_empty_name, INDICATOR_TASK_MENU_TEMPLATE),
				Plugin::TemplateOpenParam());

			ui::Widget *button = item->FindChild(
				IDParam(s_empty_name, INDICATOR_TASK_MENU_BUTTON), 0);

			if (button != NULL) {
				button->SetPos(math::v4(339.0f, -35.0f, 0.0f, 0.0f));
				button->SetSize(math::v4(60.0f, 60.0f, 0.0f, 0.0f));
				button->AddEventCallback(INDICATOR_TASK_ON_PRESSED,
							 indicatorTransferOnMenuButton, this);
				button->Show(0xff, NULL, NULL, 0.0f);
			}
		}
		ret = 0;
	}
	return ret;
}

/* the item given its widget: its button disabled while the transfer runs, its back, its
   second line, its menu and its progress bar (0x81175BBC) */
int IndicatorEventTransferItem::Attach(ui::Widget *widget, IndicatorEventItemLook look)
{
	int ret = IndicatorEventIconItem::Attach(widget, look);

	if (ret != 0)
		return ret;

	if (widget != NULL && widget->IsInherit(ui::Button::TypeName())) {
		IDParam back(s_empty_name, 0);
		bool enable;

		if (IsTransferring()) {
			back.m_idhash = m_event.newFlag ? INDICATOR_TASK_BACK_NEW :
							  INDICATOR_TASK_BACK;
			enable = false;
		} else {
			back.m_idhash = m_event.newFlag ? INDICATOR_TASK_DONE_BACK_NEW :
							  INDICATOR_TASK_DONE_BACK;
			enable = true;
		}
		if (enable)
			((ui::Button *)widget)->Enable(false);
		else
			((ui::Button *)widget)->Disable(false);
		widget->SetTexture(g_indicatorPlugin->GetTexture(back), 0, 0);
	}

	ui::CompositeDrawer *item = INDICATOR_TASK_FIND(widget, ui::CompositeDrawer,
							   IDParam("composite_item"));

	if (item == NULL)
		return 0;

	SetSubText(item, IDParam(s_empty_name, INDICATOR_TASK_SUBTEXT));
	AttachMenu(item);

	ret = g_indicatorPlugin->TemplateOpen(item,
					      IDParam(s_empty_name, INDICATOR_TASK_BAR_TEMPLATE),
					      Plugin::TemplateOpenParam());
	if (ret != 0)
		return ret;

	m_bar = INDICATOR_TASK_FIND(widget, ui::ProgressBar,
					IDParam(s_empty_name, INDICATOR_TASK_BAR));
	if (m_bar != NULL) {
		m_bar->SetSize(math::v4(606.0f, 12.0f, 0.0f, 0.0f));
		m_bar->SetPos(math::v4(-298.0f, -51.0f, 0.0f, 0.0f));
		m_bar->SetMinValue(0);
		if (IsTransferring())
			m_bar->Show(0xff, NULL, NULL, 0.0f);
		else
			m_bar->Hide(0xff, NULL, NULL, 0.0f);
		UpdateTimer(m_bar);
	}
	return ret;
}

/* the item's title, placed by the list's mode and whether the transfer runs (0x81176200) */
int IndicatorEventTransferItem::SetText(ui::Widget *widget, IDParam id)
{
	int ret = IndicatorEventTaskItem::SetText(widget, id);

	if (ret < 0 || widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return ret;

	graph::TextObj *text = INDICATOR_TASK_TEXT(widget, 0);

	if (text == NULL)
		return ret;

	if (m_event.title.length() != 0)
		text->SetString(common::String(m_event.title).GetWString());

	math::v3 pos = text->m_pos;

	if (GetMode() == 2) {
		math::v3 top = text->m_pos;

		top.set_y(-29.0f);
		text->SetPos(top);
		text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 20.0f);
	} else if (IsTransferring()) {
		pos.set_y(-38.0f);
		text->SetPos(pos);
	} else {
		pos.set_y(GetMode() == 0 ? -37.0f : -41.0f);
		text->SetPos(pos);
	}
	text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
	return ret;
}

/* the item's second line, the event's description, made when the widget has none
   (0x811763E0) */
int IndicatorEventTransferItem::SetSubText(ui::Widget *widget, IDParam id)
{
	if (widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return -1;

	graph::TextObj *text = INDICATOR_TASK_TEXT(widget, 1);

	if (text == NULL) {
		common::SharedPtr<graph::DrawObj> obj(new graph::TextObj(graph::TextObjStyle()));

		((ui::CompositeDrawer *)widget)->InsertDrawObj(obj, 1, false);
		text = obj.get() != NULL && obj->IsInherit(graph::TextObj::DrawObjType()) ?
			       (graph::TextObj *)obj.get() : NULL;
	}
	if (text == NULL)
		return -1;

	text->SetVisible(false);
	{
		wstring desc;

		common::Utf8ToUtf16(m_event.desc, &desc, 0);
		text->SetString(desc);
	}
	text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 16.0f);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, true);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, true);
	text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
	if (IsTransferring())
		text->SetLayoutAttribute(graph::TextLayoutAttribute_MaxLineCount, 1);
	else
		text->SetLayoutAttribute(graph::TextLayoutAttribute_MaxLineCount, 2);

	int mode = GetMode();

	if (mode == 2) {
		text->SetSize(math::v3(337.0f, 0.0f, 0.0f));
		text->SetPos(math::v3(20.5f, -47.0f, 0.0f));
		text->SetLayoutAttribute(graph::TextLayoutAttribute_LinePitch, 16.0f);
		text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
					math::v4(0.43137255f, 0.43137255f, 0.43137255f, 1.0f));
		text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, false);
		text->SetStyleAttribute(graph::TextStyleAttribute_EnlargeFontSize, 0, 0, false);
	} else {
		float left = mode == 0 ? 261.0f : 293.0f;

		text->SetSize(math::v3(590.0f, 0.0f, 0.0f));
		text->SetPos(math::v3(295.0f - left, -59.0f, 0.0f));
		text->SetLayoutAttribute(graph::TextLayoutAttribute_LinePitch, 19.0f);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_EnlargeLinePitch, true);
		text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
		if (mode == 0) {
			text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0, m_color);
			text->SetStyleAttribute(graph::TextStyleAttribute_Shadow, 0, 0, true);
		} else if (m_event.newFlag) {
			text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
						math::v4(0.98039216f, 0.98039216f, 0.98039216f,
							 1.0f));
		} else {
			text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
						math::v4(0.98039216f, 0.98039216f, 0.98039216f,
							 1.0f));
		}
	}
	return 0;
}

/* (0x811767A8) */
int IndicatorEventTransferItem::Execute()
{
	return IndicatorEventItem::Execute();
}

/* the item's menu opened at its button: cancelling the transfer (0x811767B0) */
void IndicatorEventTransferItem::OpenMenu(ui::Widget *anchor)
{
	ShellPopupListParam param(3, g_indicatorPlugin);

	param.flags = 19;
	param.anchor = anchor;
	param.anchorOffset[0] = 42.0f;

	LsdbNewEventPtr event(new LsdbNewEvent(m_event));

	param.SetCloseCallback(indicatorTransferOnMenuClosed,
			       new IndicatorCallArg<LsdbNewEventPtr>(event));
	s_menuIds.clear();
	s_menuIds.push_back(IDParam(s_empty_name, INDICATOR_TASK_MENU_CANCEL));

	SceUInt32 count = s_menuIds.size();

	param.items.resize(count, ShellPopupListItem());
	for (SceUInt32 i = 0; i < count; i++)
		param.items[i].text = indicatorGetString(s_menuIds[i]);
	shellPopupListOpen(&param, 0, 0);
	indicatorNoticePopupAdd(event->rowid);
}

/* the menu's item chosen: the question whether to cancel the transfer of its name
   (0x81176D42) */
static __attribute__((noinline)) void indicatorTransferOnMenuChosen(int index,
								     const LsdbNewEventPtr &arg)
{
	if (index < 0 || s_menuIds[index].GetIDHash() != INDICATOR_TASK_MENU_CANCEL)
		return;

	LsdbNewEventPtr event = arg;
	string name = indicatorTaskGetName(event->rowid);
	int id;

	{
		wstring text;

		common::Utf8ToUtf16(name, &text, 0);
		id = indicatorDialogOpen(&SceCommonGuiDialog_2A68B4A4, text.c_str(),
					 indicatorTransferOnCancelAnswered,
					 new IndicatorCallArg<LsdbNewEventPtr>(event));
	}
	indicatorNoticeDialogAdd(event->rowid, id);
}

/* the cancel command queued as a job in its slot unless one is queued there, the job
   returned */
static inline common::SharedPtr<job::JobItem> indicatorTransferRunControl(LsdbNewEventPtr event)
{
	if (s_controlSlots.Contains(INDICATOR_TRANSFER_CMD_CANCEL))
		return common::SharedPtr<job::JobItem>();

	common::SharedPtr<job::JobItem> job((job::JobItem *)new Job_TaskControl(event));

	if (job.get() != NULL)
		s_controlSlots.Enqueue(INDICATOR_TRANSFER_CMD_CANCEL, g_indicatorNewEvent->m_queue,
				       job);
	return job;
}

/* the question answered: the transfer cancelled for yes (0x81176F28) */
static void indicatorTransferOnCancelAnswered(int result, int button, void *arg)
{
	IndicatorCallArg<LsdbNewEventPtr> *call = (IndicatorCallArg<LsdbNewEventPtr> *)arg;

	if (button == INDICATOR_TRANSFER_BUTTON_YES)
		indicatorTransferRunControl(call->value);
	delete call;
}

/* the progress's percentage and status shown and the bar set (0x81177226) */
int IndicatorEventTransferItem::SetProgress(float progress, string status, string percent)
{
	ui::CompositeDrawer *item = INDICATOR_TASK_FIND(m_widget, ui::CompositeDrawer,
							   IDParam("composite_item"));

	if (item != NULL) {
		graph::TextObj *text;

		if (status.length() != 0 && (text = INDICATOR_TASK_TEXT(item, 0)) != NULL) {
			{
				wstring str;

				common::Utf8ToUtf16(status, &str, 0);
				text->SetString(str);
			}
			text->SetVisible(true);
		}
		if ((text = INDICATOR_TASK_TEXT(item, 1)) != NULL) {
			{
				wstring str;

				common::Utf8ToUtf16(percent, &str, 0);
				text->SetString(str);
			}
			text->SetVisible(true);
		}
	}
	m_bar->SetMaxValue(100.0f);
	m_bar->SetValue(progress, false, ui::ProgressBar::BAR_FORE);
	return 0;
}

/* the bar's timer killed and the progress's job stopped (0x81177584) */
int IndicatorEventTransferItem::Stop()
{
	if (m_bar != NULL) {
		if (m_bar->m_map_t_listener.find(ui::Handler::CB_INTERVAL | INDICATOR_TRANSFER_TIMER) !=
		    m_bar->m_map_t_listener.end())
			m_bar->KillIntervalEvent(INDICATOR_TRANSFER_TIMER);
		m_bar = NULL;
	}
	m_updateSlots.CancelAll();
	return 0;
}

/* a download under way in the list (0x811775EE) */
bool IndicatorEventTransferItem::IsTransferring()
{
	const LsdbNewEvent *event = GetEvent();

	return GetMode() == 1 && event->msgType == 64;
}

/* (0x8117761A) */
int IndicatorEventTransferItem::Layout(ui::Widget *widget, bool animate)
{
	return IndicatorEventTaskItem::Layout(widget, animate);
}

/* the progress's job queued in the item's slot unless one is queued there, the job returned */
static inline common::SharedPtr<job::JobItem> indicatorTransferRunUpdate(
	IndicatorEventTransferItem *item, ui::Widget *bar, LsdbNewEventPtr event)
{
	if (item == NULL || item->m_updateSlots.Contains(0))
		return common::SharedPtr<job::JobItem>();

	common::SharedPtr<job::JobItem> job(
		(job::JobItem *)new Job_UpdateProgress(bar, event, item));

	if (job.get() != NULL)
		item->m_updateSlots.Enqueue(0, g_indicatorNewEvent->m_queue, job);
	return job;
}

/* the bar's timer: the progress read (0x81177624) */
static void indicatorTransferOnTimer(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	IndicatorEventTransferItem *item = (IndicatorEventTransferItem *)data;
	const LsdbNewEvent *current = item->GetEvent();
	LsdbNewEventPtr event(new LsdbNewEvent(*current));

	indicatorTransferRunUpdate(item, (ui::Widget *)self, event);
}

/* the menu closed with the item chosen, its argument deleted (0x81177AFA) */
static void indicatorTransferOnMenuClosed(int result, void *arg)
{
	IndicatorCallArg<LsdbNewEventPtr> *call = (IndicatorCallArg<LsdbNewEventPtr> *)arg;

	if (call != NULL) {
		indicatorTransferOnMenuChosen(result, call->value);
		call->value.reset();
		delete call;
	}
}

/* the menu's button pressed (0x81177B66) */
static void indicatorTransferOnMenuButton(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	((IndicatorEventTransferItem *)data)->OpenMenu((ui::Widget *)self);
}
