/*
 * indicator_event_task.cpp: the item of a system task in the notices' list, a background
 * download or copy: its progress bar updated by a job while the task runs, its texts, its icon
 * of the task's state, and its menu, whose commands resume, stop or remove the task
 * (0x81177C44-0x8117B780). The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <std/stdlib.h>
#include <common/shared_ptr.h>
#include <common/job_slots.h>
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
#include <widget/w_appicon_simple.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../plugins.h"
#include "../../../popup_list.h"
#include "../../../bgdl_api.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../download_plugin/src/download_plugin.h"
#include "../../esecron/include/esetask.hpp"
#include "../../livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* the menu's IDs' push_back, out of line as Sony calls it (0x8141CE5C) */
PAF_VECTOR_NOINLINE_PUSH_BACK(IDParam, allocator<IDParam>)

/* the bar's timer that reads the progress, and its interval in seconds */
#define INDICATOR_TASK_TIMER		0x500
#define INDICATOR_TASK_INTERVAL		10.0f

/* the slots of the commands' and the progress's jobs: a command's, the download's progress
   and the copy's */
#define INDICATOR_TASK_CMD_RESET	0	/* bgdlResetInstalledTask */
#define INDICATOR_TASK_CMD_RELOAD	1	/* bgdlReloadTask */
#define INDICATOR_TASK_CMD_CANCEL	2	/* bgdlCancelTask */
#define INDICATOR_TASK_CMD_REMOVE	3	/* download_plugin's and bgdlRemoveTask */
#define INDICATOR_TASK_SLOT_DOWNLOAD	4
#define INDICATOR_TASK_SLOT_COPY	5

/* the task's cancellation the service reports when it was stopped */
#define INDICATOR_TASK_ERROR_CANCELLED	0x80103A1D
/* the event's errors that leave the task to be retried, and the one that ends it */
#define INDICATOR_TASK_ERROR_RETRY1	0x80103A3B
#define INDICATOR_TASK_ERROR_RETRY2	0x80103A4A
#define INDICATOR_TASK_ERROR_DONE	0x80103A3F

/* the questions' buttons */
#define INDICATOR_TASK_BUTTON_OK	2
#define INDICATOR_TASK_BUTTON_YES	4

/* the menu's, the questions', the texts' and the icons' IDs' hashes */
#define INDICATOR_TASK_MENU_RETRY	0xBFC5A69F
#define INDICATOR_TASK_MENU_STOP	0x5E8BF0EA
#define INDICATOR_TASK_ASK_REMOVE_COPY	0xA1F6B06B
#define INDICATOR_TASK_ASK_REMOVE_BGCP	0xAE9CEA4D
#define INDICATOR_TASK_ASK_REMOVE	0x8973C036
#define INDICATOR_TASK_ASK_RETRY	0x79FFD62E
#define INDICATOR_TASK_CANCELLING	0x632B6733
#define INDICATOR_TASK_TEXT_INSTALL	0x36328571
#define INDICATOR_TASK_TEXT_RETRY	0xE9FB03BC
#define INDICATOR_TASK_TEXT_COPY	0x501258E7
#define INDICATOR_TASK_ICON		0xBB2891AC
#define INDICATOR_TASK_ICON_DONE	0xD429557A
#define INDICATOR_TASK_ICON_STATE1	0x04D62B09
#define INDICATOR_TASK_ICON_STATE2	0xCCE7D536
#define INDICATOR_TASK_ICON_STATE3	0x6B71BB32
#define INDICATOR_TASK_ICON_OTHER	0x7FE74D8E

/* the icon's texture made and not made */
#define INDICATOR_TASK_ON_ICON_READY	0x100005
#define INDICATOR_TASK_ON_ICON_ERROR	0x100006

/* what the menu and the questions are given: the event, or its key, that the caller copied */
template <typename T>
struct IndicatorTaskArg {
	IndicatorTaskArg(T *value) : value(value)
	{
	}

	T *value;
	int reserved;
};

/* the menu's items (0x81566A50) and the commands' slots (0x81566A60) */
static vector<IDParam> s_menuIds((allocator<IDParam>()));
static common::JobSlots s_controlSlots;

static void indicatorTaskOnTimer(int32_t type, ui::Handler *self, ui::Event *e, void *data);
static void indicatorTaskOnMenuClosed(int result, void *arg);
static void indicatorTaskOnMenuButton(int32_t type, ui::Handler *self, ui::Event *e, void *data);
static void indicatorTaskOnAnswered(int result, int button, void *arg);

namespace {

/* a command on the task, run on the new events' queue (56 bytes) */
class Job_TaskControl : public common::SlotJobItem {
public:
	Job_TaskControl(SceUInt32 command, SceUInt32 taskId, SceInt64 rowid)
		: common::SlotJobItem("Job_TaskControl", NULL), m_taskId(taskId), m_rowid(rowid),
		  m_command(command), reserved(0)
	{
	}

	virtual ~Job_TaskControl()
	{
	}

	virtual int Run()
	{
		if (IsCanceled())
			return -1;

		int ret = 0;

		switch (m_command) {
		case INDICATOR_TASK_CMD_RESET:
			ret = bgdlResetInstalledTask(m_taskId);
			break;
		case INDICATOR_TASK_CMD_RELOAD:
			ret = bgdlReloadTask(m_taskId);
			break;
		case INDICATOR_TASK_CMD_CANCEL:
			ret = bgdlCancelTask(m_taskId);
			break;
		case INDICATOR_TASK_CMD_REMOVE: {
			indicatorBusyDialogOpen(
				indicatorGetString(IDParam(s_empty_name, INDICATOR_TASK_CANCELLING)),
				true);

			DownloadPluginInterface *download =
				(DownloadPluginInterface *)applauncherPluginGetInterface(
					"download_plugin", 1);

			if (download != NULL)
				download->Bgdl_81123ACA(m_taskId);
			ret = bgdlRemoveTask(m_taskId);
			indicatorBusyDialogClose(true);
			break;
		}
		}
		return ret;
	}

	/* the error shown unless the stop was asked for, the removed task's event deleted */
	virtual void Finish(int result)
	{
		common::SlotJobItem::Finish(result);
		if ((result != (int)INDICATOR_TASK_ERROR_CANCELLED ||
		     m_command != INDICATOR_TASK_CMD_CANCEL) &&
		    result != 0) {
			indicatorNoticeDialogAdd(m_rowid,
						 indicatorErrorDialogOpen(result, NULL, NULL, NULL));
			if (m_command == INDICATOR_TASK_CMD_REMOVE) {
				LsdbNewEventKey key;

				key.rowid = m_rowid;
				indicatorNewEventDb()->Delete(&key);
			}
		}
	}

	SceUInt32 m_taskId;
	SceInt64 m_rowid;
	SceUInt32 m_command;
	int reserved;
};

/* the task's progress read on the new events' queue and shown on its item (152 bytes) */
class Job_UpdateProgress : public common::SlotJobItem {
public:
	Job_UpdateProgress(ui::Widget *bar, SceUInt32 taskId, int kind,
			   IndicatorEventTaskItem *item)
		: common::SlotJobItem("Job_UpdateProgress", NULL), m_copy(),
		  m_bar(bar != NULL && bar->IsInherit(ui::ProgressBar::TypeName()) ?
				(ui::ProgressBar *)bar : NULL),
		  m_taskId(taskId), m_kind(kind), m_item(item)
	{
	}

	virtual ~Job_UpdateProgress()
	{
	}

	virtual int Run()
	{
		if (IsCanceled())
			return -1;
		if (m_kind == INDICATOR_TASK_SLOT_COPY)
			return bgdlGetTaskCopy(m_taskId, &m_copy);
		return bgdlGetTaskStatus(m_taskId, &m_status);
	}

	/* the bar shown with the progress, its signal animation while the task waits */
	virtual void Finish(int result)
	{
		if (!IsCanceled() && result == 0) {
			IndicatorEventTaskItem *item = m_item;

			if (item != NULL && m_bar != NULL && (m_bar->m_hold_count > 0) == false) {
				if (item->GetEvent()->msgType == 4) {
					if (!m_bar->IsSignalAnim())
						m_bar->SetSignalAnim(true);
				} else if (m_bar->IsSignalAnim()) {
					m_bar->SetSignalAnim(false);
				}
				m_bar->Show(0xff, new Timer(20.0f), NULL, 0.0f);
				if (m_kind == INDICATOR_TASK_SLOT_COPY) {
					m_bar->SetMaxValue(100.0f);
					m_bar->SetValue((float)m_copy.copy.progress, false,
							ui::ProgressBar::BAR_FORE);
					item->SetCopyProgress(m_copy.copy.name, m_copy.copy.number,
							      m_copy.copy.total, m_copy.copy.remaining,
							      true);
				} else if (m_status.fileNumber >= 2) {
					m_bar->SetMaxValue((float)m_status.filesTotal);
					m_bar->SetValue((float)m_status.filesCurrent, false,
							ui::ProgressBar::BAR_FORE);
					item->SetDownloadProgress(m_status.filesTotal,
								  m_status.filesCurrent,
								  m_status.filesRemaining, true);
				} else {
					m_bar->SetMaxValue((float)m_status.total);
					m_bar->SetValue((float)m_status.current, false,
							ui::ProgressBar::BAR_FORE);
					item->SetDownloadProgress(m_status.total, m_status.current,
								  m_status.remaining, true);
				}
			}
		}
		common::SlotJobItem::Finish(result);
	}

	BgdlTaskStatus m_status;
	BgdlTaskCopyStatus m_copy;
	ui::ProgressBar *m_bar;
	SceUInt32 m_taskId;
	int m_kind;
	IndicatorEventTaskItem *m_item;
};

}

/* the bar's timer killed and the jobs stopped (0x81177C44) */
int IndicatorEventTaskItem::Stop()
{
	if (m_bar != NULL) {
		if (m_bar->m_map_t_listener.find(ui::Handler::CB_INTERVAL | INDICATOR_TASK_TIMER) !=
		    m_bar->m_map_t_listener.end())
			m_bar->KillIntervalEvent(INDICATOR_TASK_TIMER);
		m_bar = NULL;
	}
	m_slots.CancelAll();
	return 0;
}

/* (0x81177CAE) */
IndicatorEventTaskItem::~IndicatorEventTaskItem()
{
	Stop();
}

/* the bar's timer set while the task runs; else the jobs stopped, the timer killed and the
   bar hidden (0x81177D08) */
void IndicatorEventTaskItem::UpdateTimer(ui::Widget *bar)
{
	if (bar == NULL)
		return;

	if (IsTransferring()) {
		bar->SetIntervalEvent(INDICATOR_TASK_TIMER, INDICATOR_TASK_INTERVAL,
				      indicatorTaskOnTimer, this);
	} else {
		m_slots.CancelAll();
		bar->KillIntervalEvent(INDICATOR_TASK_TIMER);
		bar->Hide(0xff, new Timer(20.0f), NULL, 0.0f);
	}
}

/* the menu's button put on the item in the list, but for a transfer's (0x81177E2C) */
int IndicatorEventTaskItem::AttachMenu(ui::Widget *item)
{
	int ret = -1;

	if (GetMode() == 1) {
		switch (m_event.msgType) {
		case 80:
		case 81:
		case 82:
			break;
		default: {
			g_indicatorPlugin->TemplateOpen(
				item, IDParam(s_empty_name, INDICATOR_TASK_MENU_TEMPLATE),
				Plugin::TemplateOpenParam());

			ui::Widget *button = item->FindChild(
				IDParam(s_empty_name, INDICATOR_TASK_MENU_BUTTON), 0);

			if (button != NULL) {
				button->SetPos(math::v4(339.0f, -35.0f, 0.0f, 0.0f));
				button->SetSize(math::v4(60.0f, 60.0f, 0.0f, 0.0f));
				button->AddEventCallback(INDICATOR_TASK_ON_PRESSED,
							 indicatorTaskOnMenuButton, this);
				button->Show(0xff, NULL, NULL, 0.0f);
			}
			break;
		}
		}
		ret = 0;
	}
	return ret;
}

/* the item given its widget: its button enabled once the task is done, its back, its second
   line, its menu and its progress bar (0x8117806E) */
int IndicatorEventTaskItem::Attach(ui::Widget *widget, IndicatorEventItemLook look)
{
	int ret = IndicatorEventIconItem::Attach(widget, look);

	if (ret != 0)
		return ret;

	if (widget != NULL && widget->IsInherit(ui::Button::TypeName())) {
		IDParam back(s_empty_name,
			     m_event.newFlag ? INDICATOR_TASK_BACK_NEW : INDICATOR_TASK_BACK);
		bool enable = false;

		switch (m_event.msgType) {
		case 80:
		case 81:
			back.m_idhash = m_event.newFlag ? INDICATOR_TASK_DONE_BACK_NEW :
							  INDICATOR_TASK_DONE_BACK;
			enable = true;
			break;
		case 82:
			back.m_idhash = m_event.newFlag ? INDICATOR_TASK_DONE_BACK_NEW :
							  INDICATOR_TASK_DONE_BACK;
			enable = true;
			break;
		case 7:
			if (m_event.actType == 3) {
				if (indicatorEventIsStateSix(&m_event)) {
					back.m_idhash = m_event.newFlag ? INDICATOR_TASK_BACK_NEW :
									  INDICATOR_TASK_BACK;
					enable = true;
				} else {
					back.m_idhash = m_event.newFlag ?
								INDICATOR_TASK_DONE_BACK_NEW :
								INDICATOR_TASK_DONE_BACK;
				}
			} else if (indicatorEventRunsPcsi00011(&m_event)) {
				back.m_idhash = m_event.newFlag ? INDICATOR_TASK_BACK_NEW :
								  INDICATOR_TASK_BACK;
			} else {
				back.m_idhash = m_event.newFlag ? INDICATOR_TASK_DONE_BACK_NEW :
								  INDICATOR_TASK_DONE_BACK;
				enable = true;
			}
			break;
		case 8:
			if (m_event.actType == 3) {
				int error = m_event.msgArg[7].length() != 0 ?
						    sce_paf_atoi(m_event.msgArg[7].c_str()) :
						    0;

				if (error == (int)INDICATOR_TASK_ERROR_DONE) {
					back.m_idhash = m_event.newFlag ?
								INDICATOR_TASK_DONE_BACK_NEW :
								INDICATOR_TASK_DONE_BACK;
					enable = true;
				}
			}
			break;
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
		return ret;

	SetSubText(item, IDParam(s_empty_name, INDICATOR_TASK_SUBTEXT));
	m_subText = INDICATOR_TASK_TEXT(item, 1);
	m_text = INDICATOR_TASK_TEXT(item, 0);
	if (indicatorEventIsBgcpStatus(&m_event) &&
	    (m_event.msgType == 81 || m_event.msgType == 2))
		return ret;

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

/* the jobs stopped (0x8117889A) */
int IndicatorEventTaskItem::Detach()
{
	Stop();
	return IndicatorEventIconItem::Detach();
}

/* the icon of the task's state shown at once (0x811788AC) */
void IndicatorEventTaskItem::ShowIcon(ui::Widget *icon)
{
	if (icon != NULL && icon->IsInherit(ui::AppIconSimple::TypeName()) && m_busy) {
		((ui::AppIconSimple *)icon)->SetAppearance(indicatorEventIsStateFive(&m_event) ? 0 :
											     2);
		icon->Show(0xff, new Timer(200.0f), NULL, 0.0f);
	} else {
		IndicatorEventIconItem::ShowIcon(icon);
	}
}

/* the icon of the task's state made, and the icon item's listeners put on the icon
   (0x81178990) */
void IndicatorEventTaskItem::ListenIcon(ui::Widget *icon)
{
	if (icon == NULL || !icon->IsInherit(ui::AppIconSimple::TypeName()))
		return;

	intrusive_ptr<cache::CacheImageInterface> image;
	IDParam texture(s_empty_name, INDICATOR_TASK_ICON);

	m_busy = true;
	int state = indicatorEventBgcpState(&m_event);

	if (state == 0) {
		if (indicatorEventIsStateFive(&m_event))
			texture.m_idhash = INDICATOR_TASK_ICON_DONE;
	} else {
		switch (state) {
		case 1:
			texture.m_idhash = INDICATOR_TASK_ICON_STATE1;
			break;
		case 2:
			texture.m_idhash = INDICATOR_TASK_ICON_STATE2;
			break;
		case 3:
			texture.m_idhash = INDICATOR_TASK_ICON_STATE3;
			break;
		default:
			texture.m_idhash = INDICATOR_TASK_ICON_OTHER;
			break;
		}
	}
	{
		IDParam id = texture;

		indicatorCacheCreateTexture(g_indicatorPlugin, id, image, NULL, NULL);
	}
	icon->AddEventListener(INDICATOR_TASK_ON_ICON_READY,
			       new ui::EventCBListener(IndicatorEventIconItem::OnIconReady, this),
			       true);
	icon->AddEventListener(INDICATOR_TASK_ON_ICON_ERROR,
			       new ui::EventCBListener(IndicatorEventIconItem::OnIconError, this),
			       true);
	icon->SetDrawObjMaskBit(&image, 0, 0);
}

/* the item's title, placed by the list's mode and whether the task runs (0x81178B98) */
int IndicatorEventTaskItem::SetText(ui::Widget *widget, IDParam id)
{
	int ret = IndicatorEventIconItem::SetText(widget, id);

	if (ret < 0)
		return ret;
	if (indicatorEventIsBgcpStatus(&m_event) &&
	    (m_event.msgType == 81 || m_event.msgType == 2))
		return ret;
	if (widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return ret;

	graph::TextObj *text = INDICATOR_TASK_TEXT(widget, 0);

	if (text == NULL)
		return ret;

	if (m_event.title.length() != 0)
		text->SetString(common::String(m_event.title).GetWString());
	else
		text->SetString(wstring(L""));

	if (GetMode() == 2) {
		math::v3 pos = text->m_pos;

		pos.set_y(-29.0f);
		text->SetPos(pos);
		text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 20.0f);
	} else if (IsTransferring()) {
		math::v3 pos = text->m_pos;

		pos.set_y(-38.0f);
		text->SetPos(pos);
	} else {
		float y = GetMode() == 0 ? -37.0f : -41.0f;
		math::v3 pos = text->m_pos;

		pos.set_y(y);
		text->SetPos(pos);
	}
	text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
	return ret;
}

/* the item's second line: what the task waits for, or the event's message with its error
   (0x81178DFA) */
int IndicatorEventTaskItem::SetSubText(ui::Widget *widget, IDParam id)
{
	if (indicatorEventIsBgcpStatus(&m_event) &&
	    (m_event.msgType == 81 || m_event.msgType == 2))
		return 0;
	if (widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return -1;

	common::String str(L"");
	SceInt64 type = m_event.msgType;

	if (type != 256 && type != 258) {
		switch (type) {
		case 80:
		case 81: {
			string titleId = m_event.execTitleId;

			str = common::String(indicatorEventAppText(titleId));
			break;
		}
		case 82:
			if (indicatorEventRunsPcsi00011(&m_event) ||
			    indicatorEventRunsPcsc80018(&m_event))
				str = common::String(g_indicatorPlugin->GetString(
					IDParam(s_empty_name, INDICATOR_TASK_TEXT_INSTALL)));
			break;
		case 2: {
			int error = m_event.msgArg[7].length() != 0 ?
					    sce_paf_atoi(m_event.msgArg[7].c_str()) :
					    0;

			if (error == (int)INDICATOR_TASK_ERROR_RETRY1 ||
			    error == (int)INDICATOR_TASK_ERROR_RETRY2)
				str = common::String(g_indicatorPlugin->GetString(
					IDParam(s_empty_name, INDICATOR_TASK_TEXT_RETRY)));
			break;
		}
		case 7:
			if (GetMode() != 2 && m_event.actType != 3 &&
			    !indicatorEventIsStateFive(&m_event))
				str = common::String(g_indicatorPlugin->GetString(
					IDParam(s_empty_name, INDICATOR_TASK_TEXT_COPY)));
			break;
		}
		if (str.GetWString().length() == 0) {
			str = common::String(indicatorEventMessage(&m_event));
			if (GetMode() != 2) {
				type = m_event.msgType;
				if (type == 8 || type == 2 || type == 0) {
					wstring title;
					wstring code;
					int error = m_event.msgArg[7].length() != 0 ?
							    sce_paf_atoi(m_event.msgArg[7].c_str()) :
							    0;

					indicatorErrorText(&title, &code, error, true, false);
					str += common::String(code);
				}
			}
		}
	}
	if (str.GetWString().length() == 0)
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
	text->SetString(str.GetWString());
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
		float left = mode == 0 ? 261.0f : 298.0f;

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

/* the download's own page opened for a download's event (0x81179668) */
static inline int indicatorTaskLogDownload(const LsdbNewEvent *event)
{
	DownloadPluginInterface *download =
		(DownloadPluginInterface *)applauncherPluginGetInterface("download_plugin", 1);

	if (download != NULL)
		return download->EventLogDownload(event->rowid);
	return 0;
}

/* what the event runs: a download's page, the base's for a copy and the transfers
   (0x81179668) */
int IndicatorEventTaskItem::Execute()
{
	SceInt64 type = m_event.msgType;

	switch (type) {
	case 7:
	case 8:
	case 80:
	case 81:
	case 82:
		break;
	default:
		return 0;
	}

	switch (m_event.actType) {
	case 3:
		return indicatorTaskLogDownload(&m_event);
	case 4:
		return indicatorTaskLogDownload(&m_event);
	case 5:
		return indicatorTaskLogDownload(&m_event);
	case 6:
		return indicatorTaskLogDownload(&m_event);
	case 9:
		return indicatorTaskLogDownload(&m_event);
	case 10:
		return indicatorTaskLogDownload(&m_event);
	default:
		switch (type) {
		case 7:
		case 80:
		case 81:
			return IndicatorEventItem::Execute();
		}
		return 1;
	}
}

/* the copy's object, its number of the total and the time left shown, or both lines hidden
   (0x811797F2) */
int IndicatorEventTaskItem::SetCopyProgress(string name, int number, int total, int remaining,
					    bool show)
{
	if (m_text == NULL || m_subText == NULL)
		return -1;

	SceInt64 type = m_event.msgType;

	if (type == 256 || type == 258 || type != 4)
		return 0;

	if (!show) {
		m_text->SetVisible(false);
		m_subText->SetVisible(false);
		return 0;
	}

	if (name.length() != 0) {
		common::String text(name);

		m_text->SetString(text.GetWString());
		m_text->SetVisible(true);
	}

	char numberText[16] = "";

	sce_paf_snprintf(numberText, sizeof(numberText) - 1, "%04d", number);

	common::String numberStr(numberText);
	char totalText[16] = "";

	sce_paf_snprintf(totalText, sizeof(totalText) - 1, "%04d", total);

	common::String totalStr(totalText);
	wstring left(L"");

	left = indicatorRemainingText(remaining);

	wstring line = shellFormatText(L"%1   (%2 / %3)", left.c_str(),
				       numberStr.GetWString().c_str(),
				       totalStr.GetWString().c_str());

	m_subText->SetString(line);
	m_subText->SetVisible(true);
	return 0;
}

/* the download's size done of its total and the time left shown, or the line hidden
   (0x81179B2C) */
int IndicatorEventTaskItem::SetDownloadProgress(SceUInt64 total, SceUInt64 current,
						int remaining, bool show)
{
	if (m_subText == NULL)
		return -1;

	SceInt64 type = m_event.msgType;

	if (type == 256 || type == 258 || type != 4)
		return 0;

	if (!show) {
		m_subText->SetVisible(false);
		return 0;
	}

	wstring currentText(L"");
	wstring totalText(L"");
	wstring left(L"");

	shellSizeText(&currentText, current);
	shellSizeText(&totalText, total);
	left = indicatorRemainingText(remaining);

	wstring line = shellFormatText(L"%1   (%2 / %3)", left.c_str(), currentText.c_str(),
				       totalText.c_str());

	m_subText->SetString(line);
	m_subText->SetVisible(true);
	return 0;
}

/* the item's menu opened at its button: resuming, stopping or removing the task
   (0x81179D36) */
static void indicatorTaskOpenMenu(ui::Widget *anchor, const LsdbNewEvent *event)
{
	ShellPopupListParam param(3, g_indicatorPlugin);

	param.flags = 19;
	param.anchor = anchor;
	param.anchorOffset[0] = 42.0f;
	param.SetCloseCallback(indicatorTaskOnMenuClosed,
			       new IndicatorTaskArg<LsdbNewEvent>(new LsdbNewEvent(*event)));
	s_menuIds.clear();
	switch (event->msgType) {
	case 2:
	case 6:
		s_menuIds.push_back(IDParam(s_empty_name, INDICATOR_TASK_MENU_RETRY));
		break;
	}
	switch (event->msgType) {
	case 3:
	case 4:
		s_menuIds.push_back(IDParam(s_empty_name, INDICATOR_TASK_MENU_STOP));
		break;
	}
	switch (event->msgType) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		s_menuIds.push_back(IDParam(s_empty_name, INDICATOR_TASK_MENU_CANCEL));
		break;
	}

	SceUInt32 count = s_menuIds.size();

	param.items.resize(count, ShellPopupListItem());
	for (SceUInt32 i = 0; i < count; i++)
		param.items[i].text = indicatorGetString(s_menuIds[i]);
	shellPopupListOpen(&param, 0, 0);
	indicatorNoticePopupAdd(event->rowid);
}

/* the command queued as a job in its slot unless one is queued there, the job returned */
static inline common::SharedPtr<job::JobItem> indicatorTaskRunControl(int command,
								      SceUInt32 taskId,
								      SceInt64 rowid)
{
	if (s_controlSlots.Contains(command))
		return common::SharedPtr<job::JobItem>();

	common::SharedPtr<job::JobItem> job(
		(job::JobItem *)new Job_TaskControl(command, taskId, rowid));

	if (job.get() != NULL)
		s_controlSlots.Enqueue(command, g_indicatorNewEvent->m_queue, job);
	return job;
}

/* the menu's item chosen: the command run, or the question asked first (0x8117A2FE) */
static __attribute__((noinline)) void indicatorTaskOnMenuChosen(int index,
								 const LsdbNewEvent *event)
{
	if (index < 0)
		return;

	SceUInt32 taskId = sce_paf_atoi(event->itemId.c_str());

	switch (s_menuIds[index].GetIDHash()) {
	case INDICATOR_TASK_MENU_RETRY:
		if (event->msgType == 2) {
			int error = event->msgArg[7].length() != 0 ?
					    sce_paf_atoi(event->msgArg[7].c_str()) :
					    0;

			if (error == (int)INDICATOR_TASK_ERROR_RETRY1 ||
			    error == (int)INDICATOR_TASK_ERROR_RETRY2) {
				int id = indicatorDialogOpen(
					&SceCommonGuiDialog_AF75CA6D,
					indicatorGetString(
						IDParam(s_empty_name, INDICATOR_TASK_ASK_RETRY)),
					indicatorTaskOnAnswered,
					new IndicatorTaskArg<LsdbNewEventKey>(
						new LsdbNewEventKey(*event)));

				indicatorNoticeDialogAdd(event->rowid, id);
			} else {
				indicatorTaskRunControl(INDICATOR_TASK_CMD_RELOAD, taskId,
							event->rowid);
			}
		} else {
			indicatorTaskRunControl(INDICATOR_TASK_CMD_RELOAD, taskId, event->rowid);
		}
		break;
	case INDICATOR_TASK_MENU_STOP:
		indicatorTaskRunControl(INDICATOR_TASK_CMD_CANCEL, taskId, event->rowid);
		break;
	case INDICATOR_TASK_MENU_CANCEL:
		switch (event->msgType) {
		case 7:
		case 8: {
			int id = indicatorDialogOpen(
				&SceCommonGuiDialog_6CD802FC,
				indicatorGetString(
					IDParam(s_empty_name, INDICATOR_TASK_ASK_REMOVE_COPY)),
				indicatorTaskOnAnswered,
				new IndicatorTaskArg<LsdbNewEventKey>(new LsdbNewEventKey(*event)));

			indicatorNoticeDialogAdd(event->rowid, id);
			break;
		}
		case 80:
		case 81:
		case 82:
			break;
		default: {
			IDParam question(s_empty_name, indicatorEventIsBgcpStatus(event) ?
							       INDICATOR_TASK_ASK_REMOVE_BGCP :
							       INDICATOR_TASK_ASK_REMOVE);
			int id = indicatorDialogOpen(
				&SceCommonGuiDialog_6CD802FC, indicatorGetString(question),
				indicatorTaskOnAnswered,
				new IndicatorTaskArg<LsdbNewEventKey>(new LsdbNewEventKey(*event)));

			indicatorNoticeDialogAdd(event->rowid, id);
			break;
		}
		}
		break;
	}
}

/* a question answered: the task resumed for OK, removed for yes (0x8117AB1E) */
static void indicatorTaskOnAnswered(int result, int button, void *arg)
{
	IndicatorTaskArg<LsdbNewEventKey> *call = (IndicatorTaskArg<LsdbNewEventKey> *)arg;
	LsdbNewEventKey *key = call->value;
	SceUInt32 taskId = sce_paf_atoi(key->itemId.c_str());
	SceInt64 rowid = key->rowid;

	delete key;
	switch (button) {
	case INDICATOR_TASK_BUTTON_OK:
		indicatorTaskRunControl(INDICATOR_TASK_CMD_RELOAD, taskId, rowid);
		break;
	case INDICATOR_TASK_BUTTON_YES:
		indicatorTaskRunControl(INDICATOR_TASK_CMD_REMOVE, taskId, rowid);
		break;
	}
	delete call;
}

/* a download or a copy running (0x8117AE88) */
bool IndicatorEventTaskItem::IsTransferring()
{
	const LsdbNewEvent *event = GetEvent();
	SceInt64 type = event->msgType;
	bool ret = false;

	if (type == 6 || type == 4 || type == 3 || type == 2) {
		if (type == 2) {
			if (event->msgArg[7].length() != 0)
				sce_paf_atoi(event->msgArg[7].c_str());
		} else {
			ret = true;
		}
	}
	return ret;
}

/* the item's texts placed by their lines, the icon item's layout for a background copy
   (0x8117AEEC) */
int IndicatorEventTaskItem::Layout(ui::Widget *widget, bool animate)
{
	if (indicatorEventIsBgcpStatus(&m_event) &&
	    (m_event.msgType == 81 || m_event.msgType == 2))
		return IndicatorEventIconItem::Layout(widget, animate);
	if (widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return 1;

	uint32_t lines = 0;
	int mode = GetMode();

	graph::TextObj *sub = INDICATOR_TASK_TEXT(widget, 1);

	if (sub != NULL) {
		if (sub->GetLineCount(lines) < 0)
			lines = 0;

		math::v4 size(sub->m_size, 0.0f);
		math::v4 offset;
		int align = 4;
		float y;

		if (mode == 2) {
			y = -47.0f;
			align = 6;
		} else if (IsTransferring()) {
			y = -61.0f;
			align = 7;
		} else {
			y = (mode == 0 ? -59.0f : -68.0f) + 7.0f;
		}
		AlignText(sub, align, size, &offset);

		math::v3 pos = sub->m_pos;

		pos.set_y(y + offset.extract_y());
		sub->SetPos(pos);
		sub->SetVisible(true);
	}

	graph::TextObj *text = INDICATOR_TASK_TEXT(widget, 0);

	if (text != NULL) {
		float y;

		if (mode == 2)
			y = -26.0f;
		else if (IsTransferring() || lines == 2)
			y = mode == 0 ? -29.0f : -38.0f;
		else
			y = mode == 0 ? -37.0f : -41.0f;

		math::v4 size(text->m_size, 0.0f);
		math::v4 offset;

		AlignText(text, 6, size, &offset);

		math::v3 pos = text->m_pos;

		pos.set_y(y + offset.extract_y());
		text->SetPos(pos);
		text->SetVisible(true);

		graph::PlaneObj *plane = widget->GetDrawObj(3) != NULL &&
						 widget->GetDrawObj(3)->IsInherit(
							 graph::PlaneObj::DrawObjType()) ?
					 (graph::PlaneObj *)widget->GetDrawObj(3) :
					 NULL;

		if (plane != NULL) {
			AlignText(text, 9, size, &offset);

			math::v3 planePos = plane->m_pos;

			planePos.set_y(pos.extract_y() - offset.extract_y());
			plane->SetPos(planePos);
			plane->SetVisible(true);
		}
	}

	graph::TextObj *time = INDICATOR_TASK_TEXT(widget, 2);

	if (time != NULL) {
		math::v4 size(time->m_size, 0.0f);
		math::v4 offset;

		AlignText(time, 5, size, &offset);

		math::v3 pos = time->m_pos;

		pos.set_y(offset.extract_y() + -80.0f);
		time->SetPos(pos);
		time->SetVisible(true);
	}
	return 1;
}

/* the progress's job queued in the item's slot unless one is queued there, the job returned */
static inline common::SharedPtr<job::JobItem> indicatorTaskRunUpdate(IndicatorEventTaskItem *item,
								     ui::Widget *bar,
								     SceUInt32 taskId, int kind)
{
	if (item == NULL || item->m_slots.Contains(kind))
		return common::SharedPtr<job::JobItem>();

	common::SharedPtr<job::JobItem> job(
		(job::JobItem *)new Job_UpdateProgress(bar, taskId, kind, item));

	if (job.get() != NULL)
		item->m_slots.Enqueue(kind, g_indicatorNewEvent->m_queue, job);
	return job;
}

/* the bar's timer: the progress of the copy or the download read (0x8117B360) */
static void indicatorTaskOnTimer(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	IndicatorEventTaskItem *item = (IndicatorEventTaskItem *)data;
	SceUInt32 taskId = sce_paf_atoi(item->GetEvent()->itemId.c_str());
	int kind = INDICATOR_TASK_SLOT_DOWNLOAD;

	if (indicatorEventIsBgcpStatus(item->GetEvent()) == true)
		kind = INDICATOR_TASK_SLOT_COPY;
	indicatorTaskRunUpdate(item, (ui::Widget *)self, taskId, kind);
}

/* the menu closed with the item chosen, the event's copy deleted (0x8117B5AA) */
static void indicatorTaskOnMenuClosed(int result, void *arg)
{
	IndicatorTaskArg<LsdbNewEvent> *call = (IndicatorTaskArg<LsdbNewEvent> *)arg;
	LsdbNewEvent *event = call->value;

	if (event != NULL) {
		indicatorTaskOnMenuChosen(result, event);
		delete event;
		call->value = NULL;
	}
	delete call;
}

/* the menu's button pressed (0x8117B694) */
static void indicatorTaskOnMenuButton(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	if (data != NULL)
		indicatorTaskOpenMenu((ui::Widget *)self,
				      ((IndicatorEventTaskItem *)data)->GetEvent());
}
