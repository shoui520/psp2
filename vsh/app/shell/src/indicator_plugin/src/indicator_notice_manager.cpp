/*
 * indicator_notice_manager.cpp: the notices' list page, opened from the bar with the new events,
 * its list sized to them and slid in and out, its events' icons read and their commands run as
 * jobs (0x8116F9DA-0x81173878). The file's and the names are ours (PLACEHOLDER), but the job's,
 * Job_EventWindowTaskCommand.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <common/shared_ptr.h>
#include <common/job_slots.h>
#include <job/job.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <timer/timerqueue.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/animlistener.h>
#include <widget/w_plane.h>
#include <widget/w_group.h>
#include <widget/w_corner_button.h>
#include <widget/w_list_view.h>
#include <widget/w_list_item.h>
#include <widget/w_button.h>
#include <widget/w_text.h>
#include <graphics/drawobj/plane_obj.h>
#include <common/property_list.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../popup_list.h"
#include "../../livespace_db/lsdb_keys.h"
#include "../../esecron/include/esetask.hpp"
#include "indicator.h"

using namespace paf;

/* the events' copy and the menu's IDs' push_back, out of line as Sony calls them
   (0x8141CC88, 0x8141CE5C) */
PAF_VECTOR_NOINLINE_COPY(LsdbNewEventPtr)
PAF_VECTOR_NOINLINE_PUSH_BACK(IDParam, allocator<IDParam>)

/* the page's template and widgets */
#define INDICATOR_NOTICE_PAGE_TEMPLATE	0x7B4F51C1
#define INDICATOR_NOTICE_PAGE_BACK	0x4607A687
#define INDICATOR_NOTICE_PAGE_FRAME	0x80A00D66
#define INDICATOR_NOTICE_PAGE_LIST	0xBCAE5EE5
#define INDICATOR_NOTICE_PAGE_LIST_BACK	0x1C7BBEE9
#define INDICATOR_NOTICE_PAGE_BUTTON	0x8BF9D3C5
#define INDICATOR_NOTICE_PAGE_GROUP	0x33001E2B

/* the message shown while the notices are deleted, and the question before */
#define INDICATOR_NOTICE_MSG_DELETING	0x632B6733
#define INDICATOR_NOTICE_MSG_DELETE_ALL	0xAF9B2409

/* the menu's items: the downloads cancelled, read again, and every notice deleted */
#define INDICATOR_NOTICE_MENU_CANCEL	0xA006B75E
#define INDICATOR_NOTICE_MENU_RELOAD	0x0104A04B
#define INDICATOR_NOTICE_MENU_DELETE	0x8E351533

/* the list's items' template, an item's widgets, and the delete-all cell's template, text and
   label */
#define INDICATOR_NOTICE_ITEM_TEMPLATE	0x7678DE85
#define INDICATOR_NOTICE_ITEM		0xA64F1E67
#define INDICATOR_NOTICE_ITEM_BUTTON	0x9AA68BAC
#define INDICATOR_NOTICE_ITEM_TEXT	0xE1DE6C06
#define INDICATOR_NOTICE_DELETE_TEMPLATE	0xEF0F356C
#define INDICATOR_NOTICE_DELETE_TEXT	0x4C345FE5
#define INDICATOR_NOTICE_MSG_DELETE	0x187DA0F4

/* the list's cells' default sizes: an event's, and the delete-all cell's */
#define INDICATOR_NOTICE_CELL_WIDTH	766.0f
#define INDICATOR_NOTICE_DELETE_WIDTH	398.0f
#define INDICATOR_NOTICE_DELETE_HEIGHT	70.0f

/* the property an item's widget keeps its event item in */
#define INDICATOR_NOTICE_ITEM_PARAM	"event_value_item_param"

/* the first segment's cells counted at most, and the list's height for them */
#define INDICATOR_NOTICE_LIST_CELLS	7
#define INDICATOR_NOTICE_CELL_HEIGHT	90.0f

/* the list's colors' and size's animation times, and its frame's */
#define INDICATOR_NOTICE_LIST_TIME	200.0f
#define INDICATOR_NOTICE_FRAME_TIME	400.0f

/* the corner button's event */
#define INDICATOR_NOTICE_BUTTON_EVENT	0x10000008

/* the commands of the job */
#define INDICATOR_NOTICE_CMD_DELETE_ALL	0
#define INDICATOR_NOTICE_CMD_CANCEL	1
#define INDICATOR_NOTICE_CMD_RELOAD	2

/* a typed child found by its ID, NULL when there is none or it is of another type */
#define INDICATOR_NOTICE_FIND(parent, type, id)						\
	((parent)->FindChild(id, 0) != NULL &&						\
	 (parent)->FindChild(id, 0)->IsInherit(type::TypeName()) ?			\
	 (type *)(parent)->FindChild(id, 0) : NULL)

/* a command of the notices' page run on its queue (56 bytes) */
class Job_EventWindowTaskCommand : public common::SlotJobItem {
public:
	Job_EventWindowTaskCommand(SceUInt32 command)
		: common::SlotJobItem("Job_EventWindowTaskCommand", NULL), m_command(command),
		  reserved2(0)
	{
	}

	virtual ~Job_EventWindowTaskCommand()
	{
	}

	virtual int Run();

	SceUInt32 m_command;
	string m_param;
	int reserved;
	int reserved2;
};

/* (0x81544A54) */
static IndicatorNoticeManager *s_noticeManager;

static void indicatorNoticeManagerOnItemDone(int result, int option);
static void indicatorNoticeManagerOnButton(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data);
static void indicatorNoticeManagerOnResized(Timer *t, ui::AnimListener *listener, void *data1,
					    void *data2, void *data3);
static void indicatorNoticeManagerOnClosed(Timer *t, ui::AnimListener *listener, void *data1,
					   void *data2, void *data3);
static void indicatorNoticeManagerOnGroupEvent(SceInt32 type, ui::Handler *self, ui::Event *e,
					       void *data);
static void indicatorNoticeManagerOnMenuClosed(int result, void *arg);
static void indicatorNoticeManagerOnDeleteAll(int result, int button, void *arg);
static void indicatorNoticeManagerOnOpenIcons(IndicatorIconInfoMap *icons, int result, void *arg);
static void indicatorNoticeManagerOnSyncIcons(IndicatorIconInfoMap *icons, int result, void *arg);
static void indicatorNoticeManagerOnIcons(IndicatorIconInfoMap *icons, int result, void *arg);

/* an event item kept in its widget's property (4 bytes; PLACEHOLDER name) */
struct IndicatorNoticeItemHolder {
	IndicatorNoticeItemHolder(IndicatorEventItem *item) : item(item)
	{
	}

	IndicatorEventItem *item;
};

/* the same event: by the rowid both have, else by the title and item IDs (PLACEHOLDER name) */
static inline bool indicatorNoticeIsSame(const LsdbNewEvent *event, const LsdbNewEvent *other)
{
	if (event->rowid != 0 && other->rowid != 0 && event->rowid == other->rowid)
		return true;
	if (event->titleId.length() == 0 || event->itemId.length() == 0)
		return false;

	bool same = false;

	if (event->titleId == other->titleId && event->itemId == other->itemId)
		same = true;
	return same;
}

/*
 * The list's items: an event's cell made from the item template, its event item attached to
 * its button, the first and last with a rounded focus; and the delete-all cell, its label
 * sized to its text (vtable 0x81531228; the class's names are ours, PLACEHOLDER).
 */
class IndicatorNoticeItemFactory : public ui::listview::ItemFactory {
public:
	IndicatorNoticeItemFactory(Plugin *plugin) : m_plugin(plugin)
	{
	}

	/* (0x8141D034) */
	virtual ~IndicatorNoticeItemFactory()
	{
	}

	/* the delete-all cell sized to its label's text (0x8141D070) */
	static void OnTextLayout(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
	{
		math::v2 bounds(0.0f, 0.0f);
		math::v4 size(INDICATOR_NOTICE_DELETE_WIDTH, INDICATOR_NOTICE_DELETE_HEIGHT, 0.0f,
			      0.0f);

		if (self != NULL && ((ui::Widget *)self)->IsInherit(ui::Text::TypeName())) {
			ui::Text *text = (ui::Text *)self;

			text->GetBounds(bounds);

			wstring str;

			text->GetString(str);
			if (str.length() == 0)
				return;
			if (bounds.extract_x() + 40.0f > INDICATOR_NOTICE_DELETE_WIDTH)
				size.set_x(bounds.extract_x() + 40.0f);
			if (bounds.extract_y() > INDICATOR_NOTICE_DELETE_HEIGHT)
				size.set_y(bounds.extract_y());
		}
		if (data != NULL && ((ui::Widget *)data)->IsInherit(ui::ListItem::TypeName())) {
			ui::ListItem *item = (ui::ListItem *)data;
			ui::Widget *button = INDICATOR_NOTICE_FIND(
				item, ui::Button, IDParam(s_empty_name, INDICATOR_NOTICE_ITEM_BUTTON));

			if (button != NULL)
				button->SetSize(size, NULL, ui::Widget::ANIM_SIZE, 0);

			ui::ListView *list = item->GetListView();

			if (list != NULL) {
				list->SetCellSizeDefault(1, size);

				IndicatorNoticeManager *manager = s_noticeManager;

				if (list->IsInherit(ui::ListView::TypeName()))
					manager->Layout(list, true);
			}
			item->SetSize(math::v4(bounds.extract_x(), bounds.extract_y(), 0.0f, 0.0f),
				      NULL, ui::Widget::ANIM_SIZE, 0);
		}
	}

	/* (0x8141D39C) */
	virtual ui::ListItem *Create(CreateParam &param)
	{
		ui::ListItem *item = NULL;
		bool opened = false;

		if (m_plugin->TemplateOpen(param.m_parent,
					   IDParam(s_empty_name, INDICATOR_NOTICE_ITEM_TEMPLATE),
					   Plugin::TemplateOpenParam()) == 0)
			opened = true;
		if (!opened)
			return item;
		item = (ui::ListItem *)param.m_parent->FindChild(
			IDParam(s_empty_name, INDICATOR_NOTICE_ITEM), 0);
		if (item == NULL)
			return item;
		if (s_empty_name != NULL)
			item->m_name.SetID(string(s_empty_name));
		else
			item->m_name.m_idhash = 0;
		item->SetSize(param.m_list_view->GetCellSizeDefault(param.m_segment), NULL,
			      ui::Widget::ANIM_SIZE, 0);
		if (param.m_segment <= 0)
			CreateEvent(param, item);
		else
			CreateDeleteAll(item);
		return item;
	}

	/* an event's cell (inlined into Create) */
	void CreateEvent(CreateParam &param, ui::ListItem *item)
	{
		ui::Widget *button =
			item->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ITEM_BUTTON), 0);

		if (button == NULL)
			return;
		button->SetSize(math::v4(INDICATOR_NOTICE_CELL_WIDTH, INDICATOR_NOTICE_CELL_HEIGHT,
					 0.0f, 0.0f),
				NULL, ui::Widget::ANIM_SIZE, 0);

		SceUInt32 cell = param.m_cell;

		if (param.m_list_view->GetCellNum(0) > 1) {
			if (cell != 0 && cell != param.m_list_view->GetCellNum(0) - 1) {
				ui::FocusShape shape;

				shape.m_shape = 2;
				ScePafWidget_13B246E0(button, &shape);
			} else {
				ui::FocusShape shape;

				shape.m_shape = 6;

				math::v4 size = *button->GetSize(0);

				shape.m_size = math::v2(size.extract_y(), size.extract_x());
				if (param.m_cell != 0)
					shape.m_rot = math::v3(0.0f, 0.0f, 1.5707964f);
				else
					shape.m_rot = math::v3(0.0f, 0.0f, -1.5707964f);
				shape.m_changed = true;
				button->m_focus_parts.push_back(shape);
			}
			cell = param.m_cell;
		}

		IndicatorEventItem *event = s_noticeManager->CreateItem(cell);

		if (event == NULL)
			return;

		IndicatorEventItemLook look = { 1, 0 };

		event->Attach(button, look);

		common::property_list::PropertyValue value;

		value.m_pointer = new IndicatorNoticeItemHolder(event);
		item->m_property_list.SetValue(IDParam(INDICATOR_NOTICE_ITEM_PARAM), value);

		ui::Widget *corner = s_noticeManager->m_page->FindChild(
			IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_BUTTON), 0);
		ui::Widget *text =
			button->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_ITEM_TEXT), 0);

		if (text != NULL) {
			text->m_not_hit_target = true;
			button->SetFocusDirectionWidget(3, text);
			text->SetFocusDirectionWidget(2, button);
			if (corner != NULL)
				text->SetFocusDirectionWidget(3, corner);
		} else if (corner != NULL) {
			button->SetFocusDirectionWidget(3, corner);
		}
	}

	/* the delete-all cell (inlined into Create) */
	void CreateDeleteAll(ui::ListItem *item)
	{
		ui::ButtonBase *button = (ui::ButtonBase *)INDICATOR_NOTICE_FIND(
			item, ui::Button, IDParam(s_empty_name, INDICATOR_NOTICE_ITEM_BUTTON));

		if (button == NULL)
			return;
		button->Disable(false);
		if (button->GetDrawObj(0) != NULL &&
		    button->GetDrawObj(0)->IsInherit(graph::PlaneObj::DrawObjType())) {
			graph::DrawObj *obj = button->GetDrawObj(0);

			if (obj != NULL)
				obj->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		}
		if (button->GetDrawObj(68) != NULL &&
		    button->GetDrawObj(68)->IsInherit(graph::PlaneObj::DrawObjType())) {
			graph::DrawObj *obj = button->GetDrawObj(68);

			if (obj != NULL)
				obj->m_draw_style.blend = 1;
		}
		if (m_plugin->TemplateOpen(item, IDParam(s_empty_name, INDICATOR_NOTICE_DELETE_TEMPLATE),
					   Plugin::TemplateOpenParam()) != 0)
			return;

		ui::Text *text = INDICATOR_NOTICE_FIND(
			item, ui::Text, IDParam(s_empty_name, INDICATOR_NOTICE_DELETE_TEXT));

		if (text == NULL)
			return;
		text->AddEventListener(0x100004, new ui::EventCBListener(OnTextLayout, item), true);
		text->SetStyleAttribute(graph::TextStyleAttribute_Emboss, 0, 0, true);
		text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
		scePafWidgetSetFontSize(text, 28.0f, graph::TextStyleAttribute_Point, 0, 0);
		text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
					math::v4(0.43137255f, 0.43137255f, 0.43137255f, 1.0f));
		text->SetString(wstring(g_indicatorPlugin->GetString(
			IDParam(s_empty_name, INDICATOR_NOTICE_MSG_DELETE))));
	}

	/* the event item kept in the cell's widget deleted, and the page's jobs cancelled
	   (0x8141DEC0) */
	virtual void Stop(StopParam &param)
	{
		IndicatorNoticeItemHolder *holder =
			(IndicatorNoticeItemHolder *)param.m_item->m_property_list
				.GetValue(IDParam(INDICATOR_NOTICE_ITEM_PARAM))
				.m_pointer;

		if (holder != NULL) {
			IndicatorEventItem *event = holder->item;

			if (event != NULL) {
				event->Detach();
				delete event;
				holder->item = NULL;
			}
			delete holder;
		}
		s_noticeManager->m_jobs.CancelAll();
		ItemFactory::Stop(param);
	}

	Plugin *m_plugin;
};

/* every notice deleted behind the busy dialog, the waiting downloads cancelled, or the
   downloads read again (0x8116F9DA) */
int Job_EventWindowTaskCommand::Run()
{
	int ret = 0;

	if (IsCanceled())
		return -1;
	switch (m_command) {
	case INDICATOR_NOTICE_CMD_DELETE_ALL:
		indicatorBusyDialogOpen(
			indicatorGetString(IDParam(s_empty_name, INDICATOR_NOTICE_MSG_DELETING)), true);
		ret = indicatorNewEventDb()->DeleteAll(true);
		indicatorBusyDialogClose(true);
		break;
	case INDICATOR_NOTICE_CMD_CANCEL:
		return bgdlCancelWaitingTasks();
	case INDICATOR_NOTICE_CMD_RELOAD:
		return bgdlReloadTasks();
	}
	return ret;
}

/* (0x8116FAAA) */
IndicatorNoticeManager *indicatorNoticeManager(void)
{
	return s_noticeManager;
}

/* the list's size: as wide as its widest cell and as high as its cells, the first segment's
   first seven only, at most 446 (0x8116FAB6) */
static __attribute__((noinline)) math::v4 indicatorNoticeListSize(ui::Widget *widget)
{
	using namespace sce::Vectormath::Simd::Aos;
	math::v4 size(0.0f, 0.0f, 0.0f, 0.0f);

	if (widget != NULL && widget->IsInherit(ui::ListView::TypeName())) {
		ui::ListView *list = (ui::ListView *)widget;
		SceUInt32 segments = list->GetSegmentNum();

		for (SceUInt32 segment = 0; segment < segments; segment++) {
			SceUInt32 cells = list->GetCellNum(segment);

			if (segment == 0 && cells > INDICATOR_NOTICE_LIST_CELLS)
				cells = INDICATOR_NOTICE_LIST_CELLS;
			for (SceUInt32 cell = 0; cell < cells; cell++) {
				math::v4 width(0.0f, 0.0f, 0.0f, 0.0f);

				width.set_x(list->GetCellSize(segment, cell).extract_x());
				size = maxPerElem(width.getImpl(), size.getImpl());

				math::v4 height(0.0f, 0.0f, 0.0f, 0.0f);

				height.set_y(list->GetCellSize(segment, cell).extract_y());

				math::v4 sum = size + height;
				math::v4 limit = sum;

				limit.set_y(INDICATOR_NOTICE_CELL_HEIGHT * 5.0f - 4.0f);
				size = minPerElem(limit.getImpl(), sum.getImpl());
			}
		}
	}
	return size;
}

/* (0x8116FBD8) */
bool IndicatorNoticeManager::IsOpen()
{
	return m_state != 3 && m_state != 0;
}

/*
 * The list sized to its cells, dimmed when it has some (shown white without), its background
 * sized around it and its frame grown to it in the time left of its growing; a list with cells
 * takes the focus from the corner button (0x8116FBE8).
 */
void IndicatorNoticeManager::Layout(ui::Widget *widget, bool focus)
{
	if (!IsOpen() || widget == NULL || !widget->IsInherit(ui::ListView::TypeName()) ||
	    widget->m_hold_count > 0)
		return;

	ui::ListView *list = (ui::ListView *)widget;
	ui::Widget *frame = list->m_parent;
	math::v4 size = indicatorNoticeListSize(list);
	SceUInt32 cells = list->GetCellNum(0);
	math::v4 color;

	if (cells != 0)
		color = math::v4(163.0f / 255.0f, 163.0f / 255.0f, 163.0f / 255.0f, 1.0f);
	else
		color = math::v4(1.0f, 1.0f, 1.0f, 0.0f);
	list->SetColor(color, new Timer(INDICATOR_NOTICE_LIST_TIME), ui::Widget::ANIM_COLOR, 0);
	list->SetSize(size, NULL, ui::Widget::ANIM_SIZE, 0);

	ui::Widget *back = frame->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST_BACK), 0);

	if (back != NULL) {
		math::v4 backColor;

		if (cells != 0)
			backColor = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
		else
			backColor = math::v4(1.0f, 1.0f, 1.0f, 0.0f);
		back->SetColor(backColor, new Timer(INDICATOR_NOTICE_LIST_TIME),
			       ui::Widget::ANIM_COLOR, 0);
		back->SetSize(size + math::v4(0.0f, 2.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SIZE,
			      0);
	}

	math::v4 margin(62.0f, 32.0f, 0.0f, 0.0f);
	math::v4 frameSize = size + margin;
	ui::AnimQueue *anim = frame->FindAnim(ui::Widget::ANIM_SCALE);
	float delay = 0.0f;

	if (anim != NULL)
		delay = anim->m_queue[0] != NULL ? anim->m_queue[0]->GetRestTime() : 0.0f;

	float overshoot = 0.1f;

	frame->SetSize(frameSize,
		       new Timer(INDICATOR_NOTICE_FRAME_TIME, Timer::FUNC_CUSTOM_BACK_OUT,
				 Timer::REPEAT_NONE, 0, &overshoot),
		       ui::Widget::ANIM_SIZE, 2, delay, indicatorNoticeManagerOnResized, this);

	if (list->GetCellNum(0) != 0) {
		ui::Widget *button = INDICATOR_NOTICE_FIND(
			m_page, ui::CornerButton, IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_BUTTON));

		if (button != NULL) {
			if (focus)
				ScePafWidget_21125870(button, 64);
			button->AddEventListener(INDICATOR_NOTICE_BUTTON_EVENT,
						 new ui::EventCBListener(indicatorNoticeManagerOnButton,
									 NULL),
						 true);
			button->SetFocusDirection(2,
						  IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST));
			button->SetFocusDirection(0,
						  IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST));
		}
	}
}

/* the item of the event at the index, told to the page when it is done (0x81170218) */
IndicatorEventItem *IndicatorNoticeManager::CreateItem(SceUInt32 index)
{
	IndicatorEventItem *item = NULL;

	if (index < m_events.size())
		item = g_indicatorNewEvent->CreateItem(m_events[index],
						       indicatorNoticeManagerOnItemDone);
	return item;
}

/* (0x811702D8) */
void indicatorNoticeManagerCreate(Plugin *plugin)
{
	if (s_noticeManager == NULL)
		s_noticeManager = new IndicatorNoticeManager(plugin);
}

/* (0x81170330) */
void indicatorNoticeManagerDestroy(void)
{
	if (s_noticeManager != NULL)
		delete s_noticeManager;
}

/*
 * The page closed: the icons' job cancelled, and an open page's back and corner button hidden,
 * the notices' dialogs and queue let go, and its frame slid out and shrunk to nothing; the new
 * flags cleared when asked (0x8117034A).
 */
void IndicatorNoticeManager::Close(int reason, int option)
{
	if (m_iconJob.get() != NULL) {
		m_iconJob->Cancel();
		m_iconJob.reset();
	}
	if (!IsOpen())
		return;

	ui::Widget *page = m_page;

	if (page == NULL || page->m_hold_count > 0)
		return;

	ui::Widget *back = INDICATOR_NOTICE_FIND(page, ui::Plane,
						 IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_BACK));

	if (back != NULL)
		ScePafWidget_F1176ACC(back, common::transition::Type_FadeinSlow, INDICATOR_NOTICE_LIST_TIME,
				      NULL, NULL);

	ui::Widget *button = INDICATOR_NOTICE_FIND(
		page, ui::CornerButton, IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_BUTTON));

	if (button != NULL)
		ScePafWidget_F1176ACC(button, (common::transition::Type)0xff, 0.0f, NULL, NULL);
	indicatorNoticeDialogCloseAll();
	indicatorNoticePopupCloseAll();

	ui::Widget *frame = INDICATOR_NOTICE_FIND(page, ui::Widget,
						  IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_FRAME));

	if (frame == NULL)
		return;
	m_state = 3;

	ui::Widget *list = INDICATOR_NOTICE_FIND(frame, ui::ListView,
						 IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST));

	if (list != NULL)
		list->Hide(INDICATOR_NOTICE_LIST_TIME, 0.0f);

	ui::Widget *listBack = INDICATOR_NOTICE_FIND(
		frame, ui::Plane, IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST_BACK));

	if (listBack != NULL)
		listBack->Hide(INDICATOR_NOTICE_LIST_TIME, 0.0f);
	frame->SetPos(math::v4(-50.0f, -22.0f, 0.0f, 0.0f) + math::v4(0.0f, 0.0f, 0.0f, 0.0f),
		      new Timer(INDICATOR_NOTICE_LIST_TIME), ui::Widget::ANIM_MOVE, 2,
		      INDICATOR_NOTICE_LIST_TIME);
	frame->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(INDICATOR_NOTICE_LIST_TIME),
		       ui::Widget::ANIM_SIZE, 2, INDICATOR_NOTICE_LIST_TIME,
		       indicatorNoticeManagerOnClosed, this);
	if (reason != 0)
		indicatorNewEventClearNewFlags(g_indicatorNewEvent);
}

/* an item done with: the page closed (0x81170DA8) */
static void indicatorNoticeManagerOnItemDone(int result, int option)
{
	if (result == 0)
		s_noticeManager->Close(2, option);
}

/* the events' icons read by their titles, each title once, the events kept for the callback
   (0x81170DC0) */
void IndicatorNoticeManager::LoadIconInfo(const vector<LsdbNewEventPtr> &events,
					  IndicatorIconInfoCB callback)
{
	vector<string> titleIds;
	vector<LsdbNewEventPtr> &list = const_cast<vector<LsdbNewEventPtr> &>(events);

	for (vector<LsdbNewEventPtr>::iterator it = list.begin(); it != list.end(); ++it) {
		vector<string>::iterator title = titleIds.begin();
		vector<string>::iterator end = titleIds.end();
		LsdbNewEvent *event = (*it).get();

		for (; title != end; ++title) {
			if (*title == event->execTitleId)
				break;
		}
		if (end == title)
			titleIds.push_back((*it)->execTitleId);
	}
	if (m_iconJob.get() != NULL) {
		m_iconJob->Cancel();
		m_iconJob.reset();
	}
	vector<LsdbNewEventPtr> *copy = new vector<LsdbNewEventPtr>(events);

	m_iconJob = indicatorIconInfoJobStart(&titleIds, callback, new IndicatorNoticeIconArg(copy));
}

/*
 * The page made from its template in the parent: its back faded in over the parent's top, its
 * list emptied and given the item factory, an event segment and the delete-all segment, and the
 * list, its background and the corner button hidden until the frame is grown; NULL, the page
 * destroyed, when a widget is missing (0x81171074).
 */
ui::Widget *IndicatorNoticeManager::MakePage(ui::Widget *parent)
{
	if (parent == NULL)
		return NULL;
	m_plugin->TemplateOpen(parent, IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_TEMPLATE),
			       Plugin::TemplateOpenParam());

	ui::Widget *page = INDICATOR_NOTICE_FIND(parent, ui::Plane,
						 IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_BACK));

	if (page == NULL)
		return NULL;
	ScePafWidget_A1193AAB(page, true, false);
	page->m_hit_detect_b_mode = 2;
	page->m_hit_detect_mode = 2;
	page->SetParent(page->m_parent, NULL, 5);
	ScePafWidget_44B22ACA(page, common::transition::Type_FadeinSlow, 0.0f, NULL, NULL);

	ui::Widget *frame = INDICATOR_NOTICE_FIND(page, ui::Plane,
						  IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_FRAME));

	if (frame != NULL) {
		ui::ListView *list = INDICATOR_NOTICE_FIND(
			parent, ui::ListView, IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST));

		if (list != NULL) {
			ui::Widget *listBack = INDICATOR_NOTICE_FIND(
				parent, ui::Plane,
				IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST_BACK));

			if (listBack != NULL) {
				ui::Widget *button = INDICATOR_NOTICE_FIND(
					parent, ui::CornerButton,
					IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_BUTTON));

				if (button != NULL) {
					list->Clear();
					list->SetVerticalBarMargin(
						math::v4(8.0f, 8.0f, 9.0f, 0.0f));
					list->SetHorizontalScrollBar(NULL);
					list->SetScrollType(ui::ListView::SCROLL_TYPE_VERTICAL);
					list->SetItemFactory(new IndicatorNoticeItemFactory(m_plugin));
					list->InsertSegment(0, 1);
					list->InsertSegment(1, 1);
					list->SetCellSizeDefault(
						0, math::v4(INDICATOR_NOTICE_CELL_WIDTH,
							    INDICATOR_NOTICE_CELL_HEIGHT, 0.0f, 0.0f));
					list->SetCellSizeDefault(
						1, math::v4(INDICATOR_NOTICE_DELETE_WIDTH,
							    INDICATOR_NOTICE_DELETE_HEIGHT, 0.0f,
							    0.0f));
					listBack->m_hit_detect_mode = 2;
					listBack->m_hit_detect_b_mode = 2;
					list->Hide(0xff, NULL, NULL, 0.0f);
					list->SetPos(math::v4(-16.0f, -11.0f, 0.0f, 0.0f), NULL,
						     ui::Widget::ANIM_MOVE, 0);
					listBack->Hide(0xff, NULL, NULL, 0.0f);
					listBack->SetPos(math::v4(-16.0f, -10.0f, 0.0f, 0.0f), NULL,
							 ui::Widget::ANIM_MOVE, 0);
					button->Hide(0xff, NULL, NULL, 0.0f);
					return page;
				}
			}
		}
	}
	page->DestroyWidget();
	return NULL;
}

/*
 * The page opened in the parent with the events, made again when there is none or it is held:
 * a touch on its group closes it, and its frame is slid in and grown to the delete-all cell
 * while the events' icons are read (0x811719BC).
 */
void IndicatorNoticeManager::Open(ui::Widget *parent, const vector<LsdbNewEventPtr> &events)
{
	if (parent == NULL || IsOpen())
		return;
	if (m_page == NULL || m_page->m_hold_count > 0) {
		m_page = NULL;
		m_page = MakePage(parent);
	}
	if (m_page == NULL)
		return;

	ui::Widget *group = INDICATOR_NOTICE_FIND(m_page, ui::Group,
						  IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_GROUP));

	if (group != NULL) {
		group->m_keycode_capture = true;
		ScePafWidget_42C5A4D4(group, 0, 0);

		ui::Widget *page = m_page;

		group->AddEventListener(0x2001A,
					new ui::EventCBListener(indicatorNoticeManagerOnGroupEvent,
								page),
					true);
	}

	math::v4 frameSize =
		math::v4(INDICATOR_NOTICE_DELETE_WIDTH, INDICATOR_NOTICE_DELETE_HEIGHT, 0.0f, 0.0f) +
		math::v4(62.0f, 32.0f, 0.0f, 0.0f);
	ui::Widget *frame = m_page->FindChild(IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_FRAME), 0);

	if (frame == NULL)
		return;
	m_state = 1;

	math::v4 pos(-50.0f, -22.0f, 0.0f, 0.0f);

	frame->SetPos(pos + math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_MOVE, 0);
	frame->SetPos(pos, new Timer(INDICATOR_NOTICE_FRAME_TIME, Timer::FUNC_BACK_OUT),
		      ui::Widget::ANIM_MOVE, 2);
	frame->SetSize(frameSize, new Timer(INDICATOR_NOTICE_FRAME_TIME, Timer::FUNC_BACK_OUT),
		       ui::Widget::ANIM_SIZE, 2, 0.0f, NULL, this);
	LoadIconInfo(events, indicatorNoticeManagerOnOpenIcons);
}

/* the events' icons read again unless they are the events shown, the page's cells changed to
   them when it is open (0x81171DE2) */
void IndicatorNoticeManager::Update(const vector<LsdbNewEventPtr> &events)
{
	vector<LsdbNewEventPtr> &list = const_cast<vector<LsdbNewEventPtr> &>(events);

	if (list.size() == m_events.size()) {
		vector<LsdbNewEventPtr>::iterator it = list.begin();
		vector<LsdbNewEventPtr>::iterator shown = m_events.begin();

		for (; it != list.end(); ++it, ++shown) {
			if (!indicatorNoticeIsSame((*it).get(), (*shown).get()))
				break;
			if ((*it)->hash != (*shown)->hash)
				break;
		}
		if (it == list.end())
			return;
	}
	if (IsOpen())
		LoadIconInfo(events, indicatorNoticeManagerOnSyncIcons);
	else
		LoadIconInfo(events, indicatorNoticeManagerOnIcons);
}

/* the frame grown: the list and its background shown, the corner button shown when the list
   has events, and the page open (0x81171EFE) */
void IndicatorNoticeManager::OnFrameResized(ui::Widget *frame, void *data)
{
	if (frame == NULL || !frame->IsInherit(ui::Widget::TypeName()))
		return;

	ui::ListView *list = INDICATOR_NOTICE_FIND(frame, ui::ListView,
						   IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST));

	if (list == NULL)
		return;

	ui::Widget *listBack = INDICATOR_NOTICE_FIND(
		frame, ui::Plane, IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST_BACK));

	if (listBack == NULL)
		return;
	list->Show(0xff, new Timer(INDICATOR_NOTICE_LIST_TIME), NULL, 0.0f);
	listBack->Show(0xff, new Timer(INDICATOR_NOTICE_LIST_TIME), NULL, 0.0f);

	SceUInt32 cells = list->GetCellNum(0);
	ui::Widget *button = INDICATOR_NOTICE_FIND(
		m_page, ui::CornerButton, IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_BUTTON));

	if (button != NULL) {
		if (cells != 0)
			ScePafWidget_44B22ACA(button, (common::transition::Type)0xff, 0.0f, NULL,
					      NULL);
		else
			ScePafWidget_F1176ACC(button, (common::transition::Type)0xff, 0.0f, NULL,
					      NULL);
	}
	m_state = 2;
}

/* the corner button's menu opened at the widget: the downloads' commands when an event is a
   download's, and the deletion of every notice (0x811724C0) */
void IndicatorNoticeManager::OpenMenu(ui::Widget *anchor)
{
	ShellPopupListParam param(3, g_indicatorPlugin);

	param.flags = 19;
	param.anchor = anchor;
	param.anchorOffset[0] = 42.0f;
	param.SetCloseCallback(indicatorNoticeManagerOnMenuClosed, this);
	m_ids.clear();

	/* the menu's items only when there are events: reloading for an event that can be
	   downloaded again, cancelling for one downloading, and deleting them all */
	if (m_events.begin() != m_events.end()) {
		bool reload = false;
		bool cancel = false;

		for (vector<LsdbNewEventPtr>::iterator it = m_events.begin(); it != m_events.end();
		     ++it) {
			switch ((*it)->msgType) {
			case 2:
			case 6:
				reload = true;
				break;
			case 3:
			case 4:
				cancel = true;
				break;
			}
		}
		if (reload)
			m_ids.push_back(IDParam(s_empty_name, INDICATOR_NOTICE_MENU_RELOAD));
		if (cancel)
			m_ids.push_back(IDParam(s_empty_name, INDICATOR_NOTICE_MENU_CANCEL));
		m_ids.push_back(IDParam(s_empty_name, INDICATOR_NOTICE_MENU_DELETE));
	}

	SceUInt32 count = m_ids.size();

	param.items.resize(count, ShellPopupListItem());
	for (SceUInt32 i = 0; i < count; i++)
		param.items[i].text = indicatorGetString(m_ids[i]);
	shellPopupListOpen(&param, 0, 0);
	indicatorNoticePopupAdd(0);
}

/* the command queued as a job in its slot unless one is queued there, the job returned */
static inline common::SharedPtr<job::JobItem> indicatorNoticeManagerRunCommand(SceUInt32 command)
{
	common::JobSlots *jobs = &s_noticeManager->m_jobs;

	if (jobs->Contains(command))
		return common::SharedPtr<job::JobItem>();

	job::JobItem *item = new Job_EventWindowTaskCommand(command);
	common::SharedPtr<job::JobItem> job(item);

	if (job.get() != NULL)
		jobs->Enqueue(command, g_indicatorNewEvent->m_queue, job);
	return job;
}

/* the menu's item chosen: the downloads read again or cancelled, or every notice deleted once
   the question is answered (0x811728F6) */
void IndicatorNoticeManager::OnMenuClosed(int index)
{
	if (index < 0)
		return;
	switch (m_ids[index].GetIDHash()) {
	case INDICATOR_NOTICE_MENU_RELOAD:
		indicatorNoticeManagerRunCommand(INDICATOR_NOTICE_CMD_RELOAD);
		break;
	case INDICATOR_NOTICE_MENU_DELETE: {
		int dialogId = indicatorDialogOpen(
			&SceCommonGuiDialog_92752DE2,
			indicatorGetString(IDParam(s_empty_name, INDICATOR_NOTICE_MSG_DELETE_ALL)),
			indicatorNoticeManagerOnDeleteAll, this);

		if (dialogId >= 0)
			indicatorNoticeDialogAdd(0, dialogId);
		break;
	}
	case INDICATOR_NOTICE_MENU_CANCEL:
		indicatorNoticeManagerRunCommand(INDICATOR_NOTICE_CMD_CANCEL);
		break;
	}
}

/* the question answered: every notice deleted on OK (0x81172CD4) */
static void indicatorNoticeManagerOnDeleteAll(int result, int button, void *arg)
{
	if (button != 2)
		return;
	indicatorNoticeManagerRunCommand(INDICATOR_NOTICE_CMD_DELETE_ALL);
}

/* the list's cells made again: an event's for each event, else the delete-all cell alone
   (0x81172E5E) */
void IndicatorNoticeManager::Refresh()
{
	if (m_page == NULL)
		return;

	ui::ListView *list = INDICATOR_NOTICE_FIND(m_page, ui::ListView,
						   IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST));

	if (list == NULL)
		return;
	list->SetCellSizeDefault(0, math::v4(INDICATOR_NOTICE_CELL_WIDTH,
					     INDICATOR_NOTICE_CELL_HEIGHT, 0.0f, 0.0f));
	list->SetCellSizeDefault(1, math::v4(INDICATOR_NOTICE_DELETE_WIDTH,
					     INDICATOR_NOTICE_DELETE_HEIGHT, 0.0f, 0.0f));
	if (m_events.size() != 0) {
		list->DeleteCell(1, 0, 1);

		SceUInt32 count = m_events.size();

		list->DeleteCell(0, 0, list->GetCellNum(0));
		list->InsertCell(0, 0, count);
	} else {
		list->DeleteCell(1, 0, 1);
		list->InsertCell(1, 0, 1);
		list->DeleteCell(0, 0, list->GetCellNum(0));
	}
	if (list->IsInherit(ui::ListView::TypeName()))
		Layout(list, true);
}

/* the shown events' empty titles and icon paths filled from their applications' icons
   (0x811730A2) */
void IndicatorNoticeManager::ApplyIcons(IndicatorIconInfoMap *icons)
{
	for (vector<LsdbNewEventPtr>::iterator it = m_events.begin(); it != m_events.end(); ++it) {
		if (!indicatorNoticeIsShown((*it).get()))
			continue;

		IndicatorIconInfoMap::iterator icon = icons->find((*it)->execTitleId);

		if (icon == icons->end())
			continue;
		if ((*it)->title.length() == 0)
			(*it)->title = icon->second->title;
		if ((*it)->iconPath.length() == 0)
			(*it)->iconPath = icon->second->iconPath;
	}
}

/* (0x81173132) */
int IndicatorNoticeManager::SetEvents(const vector<LsdbNewEventPtr> &events, int option)
{
	m_shownEvents.clear();
	m_shownEvents = m_events;
	m_events.clear();
	m_events = events;
	return 0;
}

/* (0x811731C8) */
void IndicatorNoticeManager::OnIconInfo(IndicatorIconInfoMap *icons, int result,
					IndicatorNoticeIconArg *arg)
{
	m_iconJob.reset();
	if (arg != NULL) {
		vector<LsdbNewEventPtr> *events = arg->events;

		if (events != NULL) {
			SetEvents(*events, 0);
			delete events;
			arg->events = NULL;
		}
		delete arg;
	}
	ApplyIcons(icons);
}

/* (0x81173258) */
static void indicatorNoticeManagerOnOpenIcons(IndicatorIconInfoMap *icons, int result, void *arg)
{
	IndicatorNoticeManager *manager = s_noticeManager;

	manager->OnIconInfo(icons, result, (IndicatorNoticeIconArg *)arg);
	manager->Refresh();
}

/*
 * The icons read for an open page: each event shown before kept in its cell, its cell made
 * again when it changed (a changed event moved to the top, its popup and dialog closed) and
 * removed when it is gone; then the new events' cells added and the list laid out
 * (0x81173278).
 */
void IndicatorNoticeManager::Sync(IndicatorIconInfoMap *icons, int result,
				  IndicatorNoticeIconArg *arg)
{
	OnIconInfo(icons, result, arg);
	if (m_page == NULL)
		return;

	ui::ListView *list = INDICATOR_NOTICE_FIND(m_page, ui::ListView,
						   IDParam(s_empty_name, INDICATOR_NOTICE_PAGE_LIST));

	if (list == NULL)
		return;
	list->BlockSync();

	int cell = 0;
	SceUInt32 count = m_events.size();

	for (vector<LsdbNewEventPtr>::iterator shown = m_shownEvents.begin();
	     shown != m_shownEvents.end(); ++shown, cell++) {
		vector<LsdbNewEventPtr>::iterator it;

		for (it = m_events.begin(); it != m_events.end(); ++it) {
			if (indicatorNoticeIsSame((*shown).get(), (*it).get()))
				break;
		}
		if (it == m_events.end()) {
			list->DeleteCell(0, cell, 1);
			cell--;
		} else if ((*shown).get() != (*it).get()) {
			if ((*shown)->timeUpdated != (*it)->timeUpdated) {
				list->DeleteCell(0, cell, 1);
				list->InsertCell(0, 0, 1);
				indicatorNoticePopupClose((*shown)->rowid);
				indicatorNoticeDialogClose((*shown)->rowid);
			} else {
				list->DeleteCell(0, cell, 1);
				list->InsertCell(0, cell, 1);
			}
		}
		count = m_events.size();
	}

	SceUInt32 cells = list->GetCellNum(0);
	SceUInt32 deleteCells = list->GetCellNum(1);

	if (count != 0) {
		if (deleteCells != 0)
			list->DeleteCell(1, 0, deleteCells);
		if (count != cells)
			list->InsertCell(0, 0, count - cells);
	} else if (deleteCells == 0) {
		list->InsertCell(1, 0, 1);
	}
	list->UnblockSync();
	Layout(list, false);
}

/* (0x811735DC) */
static void indicatorNoticeManagerOnSyncIcons(IndicatorIconInfoMap *icons, int result, void *arg)
{
	s_noticeManager->Sync(icons, result, (IndicatorNoticeIconArg *)arg);
}

/* (0x811735F4) */
static void indicatorNoticeManagerOnIcons(IndicatorIconInfoMap *icons, int result, void *arg)
{
	s_noticeManager->OnIconInfo(icons, result, (IndicatorNoticeIconArg *)arg);
}

/* the page let go, its jobs cancelled and the icons' job with them (0x8117360E) */
IndicatorNoticeManager::~IndicatorNoticeManager()
{
	m_page = NULL;
	m_state = 0;
	s_noticeManager = NULL;
	m_jobs.CancelAll();
	if (m_iconJob.get() != NULL) {
		m_iconJob->Cancel();
		m_iconJob.reset();
	}
}

/* a touch on the page's group: the page closed (0x8117379A) */
static void indicatorNoticeManagerOnGroupEvent(SceInt32 type, ui::Handler *self, ui::Event *e,
					       void *data)
{
	ScePafWidget_42C5A4D4((ui::Widget *)self, 2, 0);
	s_noticeManager->Close(2, 0);
}

/* (0x811737BA) */
static void indicatorNoticeManagerOnMenuClosed(int result, void *arg)
{
	s_noticeManager->OnMenuClosed(result);
}

/* (0x811737D0) */
static void indicatorNoticeManagerOnButton(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data)
{
	s_noticeManager->OpenMenu((ui::Widget *)self);
}

/* (0x811737E2) */
static void indicatorNoticeManagerOnResized(Timer *t, ui::AnimListener *listener, void *data1,
					    void *data2, void *data3)
{
	((IndicatorNoticeManager *)data2)->OnFrameResized((ui::Widget *)data1, data2);
}

/* the frame shrunk: the page destroyed and closed (0x811737F0) */
static void indicatorNoticeManagerOnClosed(Timer *t, ui::AnimListener *listener, void *data1,
					   void *data2, void *data3)
{
	IndicatorNoticeManager *manager = (IndicatorNoticeManager *)data2;

	if (manager->m_page != NULL)
		manager->m_page->DestroyWidget();
	manager->m_page = NULL;
	manager->m_state = 0;
}
