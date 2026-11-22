/*
 * indicator_notice_cache.cpp: the notices pushed to the bar, kept in their order and popped up
 * one at a time on the notification page with their sound, each told how it ended: shown,
 * cancelled, or dropped from a full queue (0x8116DA80-0x8116F9D8). The file's and the names are
 * ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <thread/thread.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_box.h>
#include <widget/w_scene.h>
#include <widget/w_composite_drawer.h>
#include "src/livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* the notification page, its notice's template and the notice's widget, in the bar's normal
   mode and in the other */
#define INDICATOR_NOTICE_PAGE			"page_event_notification"
#define INDICATOR_NOTICE_TEMPLATE_NORMAL	0x68F644CA
#define INDICATOR_NOTICE_TEMPLATE		0xC6129FE5
#define INDICATOR_NOTICE_WIDGET_NORMAL		0x6A605CAB
#define INDICATOR_NOTICE_WIDGET			0x8797CE4A

/* the notice's box, and the item's drawer in it */
#define INDICATOR_NOTICE_BOX			0x29AB0B9E
#define INDICATOR_NOTICE_ITEM			"composite_item"

/* the box's event once its children are ready to lay out (paf's event, PLACEHOLDER name) */
#define INDICATOR_NOTICE_BOX_EVENT		0x100008

/* the popup's timeout's ID, and its times in milliseconds: shown, kept, kept longer, and
   moved or scaled in and out */
#define INDICATOR_NOTICE_TIMEOUT		0x100
#define INDICATOR_NOTICE_SHOW_TIME		2200.0f
#define INDICATOR_NOTICE_KEEP_TIME		1000.0f
#define INDICATOR_NOTICE_KEEP_LONG_TIME		8000.0f
#define INDICATOR_NOTICE_ANIM_TIME		200.0f
#define INDICATOR_NOTICE_IN_TIME		400.0f

/* the notices kept waiting at most */
#define INDICATOR_NOTICE_QUEUE_MAX		16

/* how a notice ended, as its callback is told (PLACEHOLDER names) */
#define INDICATOR_NOTICE_SHOWN			0
#define INDICATOR_NOTICE_CANCELLED		2
#define INDICATOR_NOTICE_NOT_SHOWN		3
#define INDICATOR_NOTICE_DROPPED		4

/* the notice's flags (PLACEHOLDER names): kept after the popups waiting, kept longer and slid
   in, not kept */
#define INDICATOR_NOTICE_FLAG_KEEP		0x1
#define INDICATOR_NOTICE_FLAG_SLIDE		0x2
#define INDICATOR_NOTICE_FLAG_SKIP		0x4

/* the new event's notify flags (PLACEHOLDER names): slid in, with its sound, with the LED */
#define INDICATOR_NOTIFY_SLIDE			0x2
#define INDICATOR_NOTIFY_SOUND			0x4
#define INDICATOR_NOTIFY_LED			0x8

/* the sound of a notice that pops up without one, and the one that is silent */
#define INDICATOR_NOTICE_SE_DEFAULT		15
#define INDICATOR_NOTICE_SE_SILENT		-1

/* the child, when it is of the type */
#define INDICATOR_NOTICE_FIND(parent, type, id)						\
	((parent)->FindChild(id, 0) != NULL &&						\
	 (parent)->FindChild(id, 0)->IsInherit(type::TypeName()) ?			\
	 (type *)(parent)->FindChild(id, 0) : NULL)

/* a notice waiting or shown: what was pushed, with the item made of its event and the job
   reading its icon (60 bytes) */
class IndicatorNotice {
public:
	IndicatorNotice(const IndicatorPushNotificationParam &param);
	~IndicatorNotice();
	int Release();

	int m_target;
	string m_titleId;
	int m_option;
	int m_result;
	IndicatorEventItem *m_item;
	LsdbNewEventPtr m_event;
	IndicatorPushNotificationCB m_callback;
	void *m_arg;
	int m_flags;
	common::SharedPtr<job::JobItem> m_iconJob;
	bool m_shown;		/* its time shown over */
};

/* (0x81544A50) */
IndicatorNoticeCache *g_indicatorNoticeCache;

static void indicatorNoticeCacheOnTimeout(SceInt32 type, ui::Handler *self, ui::Event *e,
					  void *data);
static void indicatorNoticeCacheOnEnd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void indicatorNoticeCacheOnIconInfo(IndicatorIconInfoMap *icons, int result, void *arg);
static void indicatorNoticeCacheOnBoxEvent(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data);
static void indicatorNoticeCacheOnHidden(Timer *t, ui::AnimListener *listener, void *data1,
					 void *data2, void *data3);
static void indicatorNoticeCacheOnShown(Timer *t, ui::AnimListener *listener, void *data1,
					void *data2, void *data3);

/* the event's sound: one that pops up plays the default sound for none and is silent for -1;
   one without the sound flag none (0x8116DA80) */
static __attribute__((noinline)) void indicatorNoticeSetSound(LsdbNewEventPtr event)
{
	LsdbNewEvent *e = event.get();
	unsigned int flags = e->notifyFlags;

	if (e->popupNo) {
		switch (e->seId) {
		case 0:
			e->seId = INDICATOR_NOTICE_SE_DEFAULT;
			e->seVolume = 1.0f;
			break;
		case INDICATOR_NOTICE_SE_SILENT:
			e->seVolume = 0.0f;
			break;
		}
	}
	if ((flags & INDICATOR_NOTIFY_SOUND) == 0) {
		e->seId = 0;
		e->seVolume = 0.0f;
	}
}

/* (0x8116DACC) */
IndicatorNotice::IndicatorNotice(const IndicatorPushNotificationParam &param)
	: m_target(param.target), m_titleId(param.titleId), m_option(param.option),
	  m_result(param.result), m_item(NULL), m_event(param.event), m_callback(param.callback),
	  m_arg(param.arg), m_flags(param.flags), m_shown(false)
{
	indicatorNoticeSetSound(m_event);
}

/* the bar's scale stopped, the item deleted, the icon's job cancelled and the event let go
   (0x8116DBD6) */
int IndicatorNotice::Release()
{
	g_indicatorNewEvent->StopScale();
	if (m_item != NULL) {
		delete m_item;
		m_item = NULL;
	}
	if (m_iconJob.get() != NULL) {
		m_iconJob->Cancel();
		m_iconJob.reset();
	}
	m_event.reset();
	return 0;
}

/* (0x8116DCB6) */
IndicatorNotice::~IndicatorNotice()
{
	Release();
}

/*
 * The notice shown done with and the next one taken from the queue, the page opened for it if
 * it is not yet, its widget made from the template of the bar's mode and moved in, the bar told
 * to pop and the LED to blink for an event that asks; with none left the page closed
 * (0x8116DD7C).
 */
void IndicatorNoticeCache::ShowNext()
{
	if (m_current != NULL) {
		delete m_current;
		m_current = NULL;
	}
	if (m_queue.size() != 0) {
		m_current = m_queue[0];
		m_queue.erase(m_queue.begin(), m_queue.begin() + 1);

		m_page = m_plugin->PageRoot(IDParam(INDICATOR_NOTICE_PAGE));
		if (m_page == NULL) {
			Plugin::PageOpenParam param;

			param.overwrite_draw_priority = 32;
			param.option = 1;
			m_page = m_plugin->PageOpen(IDParam(INDICATOR_NOTICE_PAGE), param);
			ScePafWidget_66847A11(m_page, true);
		}

		bool normal = indicatorIsShownNormal();
		IDParam templ(s_empty_name, normal ? INDICATOR_NOTICE_TEMPLATE_NORMAL :
						     INDICATOR_NOTICE_TEMPLATE);
		IDParam id(s_empty_name, normal ? INDICATOR_NOTICE_WIDGET_NORMAL :
						  INDICATOR_NOTICE_WIDGET);
		math::v4 pos = normal ? math::v4(472.0f, 207.0f, 0.0f, 0.0f) :
					math::v4(472.0f, 240.0f, 0.0f, 0.0f);

		m_popup = m_page->FindChild(id, 0);
		if (m_popup == NULL) {
			m_plugin->TemplateOpen(m_page, templ, Plugin::TemplateOpenParam());
			m_popup = m_page->FindChild(id, 0);
			if (m_popup == NULL)
				return;
		}
		m_popup->SetPos(pos, NULL, ui::Widget::ANIM_MOVE, 0);
		m_popup->m_hit_detect_b_mode = 2;
		m_popup->m_hit_detect_mode = 2;
		m_popup->m_hit_find_mode = 1;
		g_indicatorNewEvent->Animate(indicatorNoticeCacheOnShown, true, 0.0f);

		LsdbNewEvent *event = m_current->m_event.get();

		if ((event->notifyFlags & INDICATOR_NOTIFY_LED) != 0 && event->newFlag)
			indicatorNewEventBlinkLed();
	} else {
		m_plugin->PageClose(IDParam(INDICATOR_NOTICE_PAGE), Plugin::PageCloseParam());
	}
}

/* the notice's widget moved out and shrunk (at once unless animated), then destroyed and the
   next notice shown by indicatorNoticeCacheOnHidden; without one the next shown now
   (0x8116E176) */
void IndicatorNoticeCache::Dismiss(bool animate)
{
	if (m_popup != NULL) {
		ui::Widget *box = INDICATOR_NOTICE_FIND(m_popup, ui::Box,
							 IDParam(s_empty_name, INDICATOR_NOTICE_BOX));

		if (box != NULL)
			box->DeleteEventCallback(INDICATOR_NOTICE_BOX_EVENT,
						 indicatorNoticeCacheOnBoxEvent, this);

		float time = 0.0f;

		if (animate)
			time = INDICATOR_NOTICE_ANIM_TIME;
		m_popup->KillTimeoutEvent(INDICATOR_NOTICE_TIMEOUT);

		math::v4 pos = m_popup->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		pos.set_x(403.0f);
		m_popup->SetPos(pos, new Timer(time), ui::Widget::ANIM_MOVE, 0);
		m_popup->SetScale(math::v4(0.0f, 0.0f, 0.0f, 0.0f), new Timer(time),
				  ui::Widget::ANIM_SCALE, 2, 0.0f, indicatorNoticeCacheOnHidden, this);
	} else {
		ShowNext();
	}
}

/*
 * A notice pushed: one with an option of 0 or more waits behind those with an option not above
 * it; one below 0 is shown now, unless the one shown is the same title's and event's, whose
 * time is restarted instead (one below 0 that is put back to wait in front, any other done
 * with). The queue's last notices beyond 16 are dropped, and a notice shown if none is
 * (0x8116E488).
 */
void IndicatorNoticeCache::Push(IndicatorNotice *notice)
{
	if (notice->m_option < 0) {
		if (m_current != NULL) {
			if (m_current->m_option < 0 && m_current->m_titleId == notice->m_titleId &&
			    m_current->m_event->title == notice->m_event->title) {
				m_popup->SetTimeoutEvent(INDICATOR_NOTICE_TIMEOUT,
							 INDICATOR_NOTICE_SHOW_TIME,
							 indicatorNoticeCacheOnTimeout);
				if (notice != NULL)
					delete notice;
				return;
			}
			if (m_current->m_option < 0)
				m_queue.insert(m_queue.begin(), m_current);
			else if (m_current != NULL)
				delete m_current;
			m_current = NULL;
			Dismiss(true);
		}
		m_queue.insert(m_queue.begin(), notice);
	} else {
		vector<IndicatorNotice *>::iterator it = m_queue.begin();

		for (; it != m_queue.end(); ++it) {
			if (notice->m_option < (*it)->m_option)
				break;
		}
		m_queue.insert(it, notice);
	}
	while (m_queue.size() > INDICATOR_NOTICE_QUEUE_MAX) {
		IndicatorNotice *last = m_queue.back();

		if (last->m_callback != NULL)
			last->m_callback(INDICATOR_NOTICE_DROPPED, last->m_event->rowid, last->m_arg);
		m_queue.pop_back();
		if (last != NULL)
			delete last;
	}
	if (m_popup == NULL)
		ShowNext();
}

/* (0x8116E63E) */
static void indicatorNoticeCachePushCall(void *data)
{
	g_indicatorNoticeCache->Push((IndicatorNotice *)data);
}

/* the notice told how it ended, done with, and the next shown (inline in its callers) */
inline void IndicatorNoticeCache::Finish(int result)
{
	IndicatorNotice *notice = m_current;

	if (notice->m_callback != NULL)
		notice->m_callback(result, notice->m_event->rowid, notice->m_arg);
	if (m_current != NULL)
		delete m_current;
	m_current = NULL;
	Dismiss(true);
}

/* the notice's time shown over: it ends now if it is not kept, or other popups wait and it is
   neither kept nor slid in; else it stays a while longer (0x8116E654) */
void IndicatorNoticeCache::OnTimeout()
{
	int popups = indicatorNewEventDb()->GetPopupCount();
	int waiting = 0;
	int flags = m_current->m_flags;

	if (popups != 0)
		waiting = popups - 1;
	m_current->m_shown = true;
	if ((flags & INDICATOR_NOTICE_FLAG_SKIP) != 0 ||
	    (waiting != 0 && (flags & INDICATOR_NOTICE_FLAG_KEEP) == 0 &&
	     (flags & INDICATOR_NOTICE_FLAG_SLIDE) == 0)) {
		Finish(INDICATOR_NOTICE_SHOWN);
	} else if ((flags & INDICATOR_NOTICE_FLAG_SLIDE) != 0) {
		m_popup->SetTimeoutEvent(INDICATOR_NOTICE_TIMEOUT, INDICATOR_NOTICE_KEEP_LONG_TIME,
					 indicatorNoticeCacheOnEnd);
	} else {
		m_popup->SetTimeoutEvent(INDICATOR_NOTICE_TIMEOUT, INDICATOR_NOTICE_KEEP_TIME,
					 indicatorNoticeCacheOnEnd);
	}
}

/* (0x8116E762) */
static void indicatorNoticeCacheOnTimeout(SceInt32 type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	g_indicatorNoticeCache->OnTimeout();
}

/* (0x8116E778) */
static void indicatorNoticeCacheOnEnd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	g_indicatorNoticeCache->Finish(INDICATOR_NOTICE_SHOWN);
}

/*
 * The notices of the title ("all": every one) cancelled: those waiting told and dropped, and
 * the one shown ("current" or "force" too) told and moved out; "force" drops it at once, told
 * it was not shown if its time was not over yet. The title is deleted (0x8116E7BC).
 */
void IndicatorNoticeCache::CancelNow(string *titleId)
{
	vector<IndicatorNotice *>::iterator it = m_queue.begin();

	while (it != m_queue.end()) {
		if (*titleId == "all" || *titleId == (*it)->m_titleId) {
			IndicatorNotice *notice = *it;

			if (notice->m_callback != NULL)
				notice->m_callback(INDICATOR_NOTICE_CANCELLED, notice->m_event->rowid,
						   notice->m_arg);
			if (*it != NULL)
				delete *it;
			it = m_queue.erase(it, it + 1);
		} else {
			++it;
		}
	}
	if (m_current != NULL &&
	    (*titleId == "all" || *titleId == "current" || *titleId == "force" ||
	     *titleId == m_current->m_titleId)) {
		bool force = *titleId == "force";

		if (force) {
			if (m_current->m_callback != NULL) {
				if (m_current->m_shown)
					m_current->m_callback(INDICATOR_NOTICE_CANCELLED,
							      m_current->m_event->rowid,
							      m_current->m_arg);
				else
					m_current->m_callback(INDICATOR_NOTICE_NOT_SHOWN,
							      m_current->m_event->rowid,
							      m_current->m_arg);
			}
			if (m_current != NULL)
				delete m_current;
			m_current = NULL;
			if (m_popup != NULL)
				Dismiss(false);
		} else {
			if (m_current->m_callback != NULL)
				m_current->m_callback(INDICATOR_NOTICE_CANCELLED,
						      m_current->m_event->rowid, m_current->m_arg);
			Dismiss(true);
		}
	}
	if (titleId != NULL)
		delete titleId;
}

/* (0x8116E9E8) */
static void indicatorNoticeCacheCancelCall(void *data)
{
	g_indicatorNoticeCache->CancelNow((string *)data);
}

/* a notice made of what was pushed and pushed on the main thread (0x8116E9FE) */
int IndicatorNoticeCache::Add(const IndicatorPushNotificationParam &param)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("notificationPush",
							 indicatorNoticeCachePushCall,
							 new IndicatorNotice(param));
	return 1;
}

/* the title's notices cancelled on the main thread (0x8116EA38) */
int IndicatorNoticeCache::Cancel(const char *titleId)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("notificationCancel",
							 indicatorNoticeCacheCancelCall,
							 new string(titleId));
	return 1;
}

/* every notice cancelled on the main thread (0x8116EA8C) */
int IndicatorNoticeCache::CancelAll()
{
	thread::CallQueue::main_thread_callqueue.Enqueue("notificationCancel",
							 indicatorNoticeCacheCancelCall,
							 new string("all"));
	return 1;
}

/* (0x8116EAE2) */
IndicatorNoticeCache *indicatorNoticeCache(void)
{
	return g_indicatorNoticeCache;
}

/* (0x8116EAF0) */
void indicatorNoticeCacheCreate(Plugin *plugin)
{
	if (g_indicatorNoticeCache == NULL)
		g_indicatorNoticeCache = new IndicatorNoticeCache(plugin);
}

/* (0x8116EB36) */
void indicatorNoticeCacheDestroy(void)
{
	IndicatorNoticeCache *cache = g_indicatorNoticeCache;

	if (cache != NULL) {
		g_indicatorNoticeCache = NULL;
		delete cache;
	}
}

/* the notice's item made of its event and given the box, which is shown by
   indicatorNoticeCacheOnBoxEvent once its children are ready (0x8116EB68) */
void IndicatorNoticeCache::AttachItem()
{
	m_current->m_item = g_indicatorNewEvent->CreateItem(m_current->m_event, NULL);
	if (m_current->m_item == NULL)
		return;

	ui::Widget *box = INDICATOR_NOTICE_FIND(m_page, ui::Box,
						 IDParam(s_empty_name, INDICATOR_NOTICE_BOX));

	if (box == NULL)
		return;

	/* the same in either mode */
	math::v4 pos(0.0f, indicatorIsShownNormal() ? -3.0f : -3.0f, 0.0f, 0.0f);

	box->SetPos(pos, NULL, ui::Widget::ANIM_MOVE, 0);
	box->AddEventListener(INDICATOR_NOTICE_BOX_EVENT,
			      new ui::EventCBListener(indicatorNoticeCacheOnBoxEvent, this), true);
	box->SetMetaAlpha(0.0f, NULL, ui::Widget::ANIM_META_ALPHA, 0);
	m_current->m_item->Attach(box, 2, 0);
}

/* the icons' information read: an event told with its icon takes its title's name and icon
   where it has none, and its item is made (0x8116EE2A) */
void IndicatorNoticeCache::OnIconInfo(IndicatorIconInfoMap *icons, int result, void *arg)
{
	if (m_current == NULL)
		return;
	if (indicatorNoticeIsShown(m_current->m_event.get())) {
		IndicatorIconInfoMap::iterator it = icons->find(m_current->m_event->execTitleId);

		if (it != icons->end()) {
			if (m_current->m_event->title.size() == 0)
				m_current->m_event->title = it->second->title;
			if (m_current->m_event->iconPath.size() == 0)
				m_current->m_event->iconPath = it->second->iconPath;
		}
	}
	m_current->m_iconJob.reset();
	AttachItem();
}

/* (0x8116EF04) */
static void indicatorNoticeCacheOnIconInfo(IndicatorIconInfoMap *icons, int result, void *arg)
{
	g_indicatorNoticeCache->OnIconInfo(icons, result, arg);
}

/* the icon's information of the title the notice's event runs read (0x8116EF1C) */
void IndicatorNoticeCache::LoadIconInfo()
{
	if (m_current == NULL)
		return;

	vector<string> titleIds;

	titleIds.push_back(string(m_current->m_event->execTitleId.c_str()));
	m_current->m_iconJob = indicatorIconInfoJobStart(&titleIds, indicatorNoticeCacheOnIconInfo,
							 this);
}

/* the drawer's draw objects and its shown children bounded: the bounds' size and centre
   (0x8116F0EC) */
void IndicatorNoticeCache::GetBounds(ui::Widget *widget, math::v4 *size, math::v4 *center)
{
	using namespace sce::Vectormath::Simd::Aos;

	if (widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return;

	int count = widget->m_drawobjs.size();
	math::v4 lo(9.2233720e18f, 9.2233720e18f, 9.2233720e18f, 9.2233720e18f);
	math::v4 hi(-9.2233720e18f, -9.2233720e18f, -9.2233720e18f, -9.2233720e18f);

	for (int i = 0; i < count; i++) {
		graph::DrawObj *obj = widget->GetDrawObj(i);

		if (obj != NULL) {
			math::v4 half = math::v4(obj->m_size, 0.0f) * 0.5f;
			math::v4 pos(obj->m_pos, 0.0f);

			lo = minPerElem((pos - half).getImpl(), lo.getImpl());
			hi = maxPerElem((pos + half).getImpl(), hi.getImpl());
		}
	}
	for (list<ui::Widget *>::iterator it = widget->m_children.begin();
	     it != widget->m_children.end(); ++it) {
		if (!(*it)->m_parent_protected) {
			math::v4 half = math::v4(Vector4((*it)->GetSize(0)->getImpl().getXYZ(), 0.0f)) *
					0.5f;
			math::v4 pos = math::v4(Vector4(
				(*it)->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
					.getImpl()
					.getXYZ(),
				0.0f));

			lo = minPerElem((pos - half).getImpl(), lo.getImpl());
			hi = maxPerElem((pos + half).getImpl(), hi.getImpl());
		}
	}
	*center = (hi + lo) / 2.0f;
	*size = hi - lo;
}

/*
 * The box's children ready: the box sized to the item's drawer (at least 52 high) and shown,
 * the notice's widget sized around it and scaled in, slid in from the right too for an event
 * that asks, its time started and its sound played (0x8116F284).
 */
void IndicatorNoticeCache::Open()
{
	math::v4 boxSize(398.0f, 70.0f, 0.0f, 0.0f);
	ui::Widget *box = INDICATOR_NOTICE_FIND(m_page, ui::Box,
						 IDParam(s_empty_name, INDICATOR_NOTICE_BOX));

	if (box != NULL) {
		ui::Widget *item = INDICATOR_NOTICE_FIND(box, ui::CompositeDrawer,
							  IDParam(INDICATOR_NOTICE_ITEM));

		if (item != NULL) {
			math::v4 size, center;

			GetBounds(item, &size, &center);
			item->GetSize(0);
			item->GetSizeBound(0, NULL);
			item->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			float height = __builtin_fabsf(center.extract_y()) + size.extract_y() * 0.5f;

			if (height < 52.0f)
				height = 52.0f;
			boxSize.set_y(height);
		}
		box->SetSize(boxSize, NULL, ui::Widget::ANIM_SIZE, 0);
		box->SetMetaAlpha(1.0f, NULL, ui::Widget::ANIM_META_ALPHA, 0);
	}
	m_popup->SetSize(math::v4(426.0f, boxSize.extract_y() + 3.0f + 13.0f, 0.0f, 0.0f), NULL,
			 ui::Widget::ANIM_SIZE, 0);
	m_popup->SetScale(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SCALE, 0);
	if ((m_current->m_event->notifyFlags & INDICATOR_NOTIFY_SLIDE) != 0) {
		math::v4 pos = m_popup->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v4 start = pos;

		start.set_x(403.0f);
		m_popup->SetPos(start, NULL, ui::Widget::ANIM_MOVE, 0);
		m_popup->SetPos(pos, new Timer(INDICATOR_NOTICE_IN_TIME, Timer::FUNC_BACK_OUT),
				ui::Widget::ANIM_MOVE, 0);
		m_popup->SetScale(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
				  new Timer(INDICATOR_NOTICE_IN_TIME, Timer::FUNC_BACK_OUT),
				  ui::Widget::ANIM_SCALE, 0);
	}
	m_popup->SetTimeoutEvent(INDICATOR_NOTICE_TIMEOUT, INDICATOR_NOTICE_SHOW_TIME,
				 indicatorNoticeCacheOnTimeout);
	if (m_current->m_event->seId > 0)
		Framework::Instance()->PlayCommonSE(m_current->m_event->seId,
						    m_current->m_event->seVolume, false);
}

/* (0x8116F8F4) */
static void indicatorNoticeCacheOnBoxEvent(SceInt32 type, ui::Handler *self, ui::Event *e,
					   void *data)
{
	if (self == NULL || ((ui::Widget *)self)->m_parent->m_prepare_count > 0)
		return;
	self->DeleteEventCallback(type, indicatorNoticeCacheOnBoxEvent, data);
	((IndicatorNoticeCache *)data)->Open();
}

/* the notice's widget moved out: destroyed, and the next notice shown (0x8116F926) */
static void indicatorNoticeCacheOnHidden(Timer *t, ui::AnimListener *listener, void *data1,
					 void *data2, void *data3)
{
	g_indicatorNoticeCache->m_popup->DestroyWidget();
	g_indicatorNoticeCache->m_popup = NULL;
	g_indicatorNoticeCache->ShowNext();
}

/* the bar popped: it blinks, and the notice's icon is read (0x8116F948) */
static void indicatorNoticeCacheOnShown(Timer *t, ui::AnimListener *listener, void *data1,
					void *data2, void *data3)
{
	g_indicatorNewEvent->StartBlink();
	g_indicatorNoticeCache->LoadIconInfo();
}
