/*
 * indicator_new_event.cpp: the new events' item on the information bar, the button that opens
 * the notices, with the count of the new events on it, and the notices pushed to the bar
 * (0x81162B04-0x81168030). Its commands run as jobs on its own queue, each in its slot. The
 * file's and the names are ours (PLACEHOLDER), but Sony's job names.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <framework.h>
#include <plugin.h>
#include <std/string>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <cache/cache_image.h>
#include <image/image_types.h>
#include <image/image_region.h>
#include <std/vector>
#include <common/shared_ptr.h>
#include <common/job_slots.h>
#include <common/main_thread_call_list.h>
#include <common/string_util.h>
#include <job/job.h>
#include <thread/thread.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/environment.h>
#include <widget/w_button_base.h>
#include <widget/w_plane.h>
#include <widget/w_busyindicator.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include "../../../shell.h"
#include "../../../boot.h"
#include "indicator.h"

using namespace paf;

/* the item's template, its button and the count's back on the bar, the count's text on the
   button, and the glow shown once the theme's images are in (PLACEHOLDER names) */
#define INDICATOR_NEW_EVENT_TEMPLATE	0x4FB59E9A
#define INDICATOR_NEW_EVENT_BUTTON	0x718CB7FF
#define INDICATOR_NEW_EVENT_BADGE	0x6AE5A623
#define INDICATOR_NEW_EVENT_COUNT	0x6E6CC672
#define INDICATOR_NEW_EVENT_GLOW	0xA39E3CC2
#define INDICATOR_NEW_EVENT_BUSY	0x2D08677D

/* the plugin's images used where the theme has none, with and without new events, and the
   size the theme's are loaded at (PLACEHOLDER names) */
#define INDICATOR_NEW_EVENT_NEW_IMAGE	0xE062C41F
#define INDICATOR_NEW_EVENT_NONE_IMAGE	0x6821404A
#define INDICATOR_NEW_EVENT_IMAGE_W	120
#define INDICATOR_NEW_EVENT_IMAGE_H	110

/* the item's commands, each run by a Job_NewEventTaskCommand in the slot of its number: the
   first reading (popups cleared on a demo unit), a reading again, the new flags cleared, the
   active application's events no longer new, a new event come and the popup shown (PLACEHOLDER
   names) */
#define INDICATOR_NEW_EVENT_CMD_INIT		0
#define INDICATOR_NEW_EVENT_CMD_THEME		1
#define INDICATOR_NEW_EVENT_CMD_CLEAR_NEW	2
#define INDICATOR_NEW_EVENT_CMD_APP_SEEN	3
#define INDICATOR_NEW_EVENT_CMD_NEW_EVENT	4
#define INDICATOR_NEW_EVENT_CMD_POPUP		5

/* the most new events a command reads (PLACEHOLDER name) */
#define INDICATOR_NEW_EVENT_MAX_EVENTS	64

/* the shell's states the item is told of (PLACEHOLDER names, read from indicatorNewEventOnShellEvent) */
#define INDICATOR_SHELL_STATE_MENU	2	/* shell event 6 */
#define INDICATOR_SHELL_STATE_APP	3	/* shell event 7 */
#define INDICATOR_SHELL_STATE_BUSY	4	/* shell event 4, until event 5 */
#define INDICATOR_SHELL_STATE_HOME	5	/* shell event 2, until event 3 */

/* the scene entry hidden while the shell is in INDICATOR_SHELL_STATE_HOME (PLACEHOLDER name) */
#define INDICATOR_NEW_EVENT_SCENE_ENTRY	19

/* the count's back fading out, and how long it takes (PLACEHOLDER names) */
#define INDICATOR_NEW_EVENT_BADGE_TIMER	0x2001000
#define INDICATOR_NEW_EVENT_FADE_TIME	800.0f
#define INDICATOR_NEW_EVENT_BLINK_TIME	100.0f

/* how long each blink started keeps the back blinking, how long it holds itself lit and out
   (PLACEHOLDER names) */
#define INDICATOR_NEW_EVENT_BLINK_SPAN	3.6f
#define INDICATOR_NEW_EVENT_BLINK_LIT	200.0f

/* the button's moves: its place on the bar and off it, how long they take and its scale when
   it pops (PLACEHOLDER names) */
#define INDICATOR_NEW_EVENT_ANIM_TIME	300.0f
#define INDICATOR_NEW_EVENT_POP_SCALE	1.2f

/* the button's burst when the notices open: a wobble, then a swell after the wobble's first
   part, the count and the busy indicator hidden (PLACEHOLDER names) */
#define INDICATOR_NEW_EVENT_WOBBLE_SCALE	1.1f
#define INDICATOR_NEW_EVENT_WOBBLE_TIME		800.0f
#define INDICATOR_NEW_EVENT_SWELL_SCALE		1.6f
#define INDICATOR_NEW_EVENT_SWELL_TIME		350.0f
#define INDICATOR_NEW_EVENT_SWELL_START		250.0f
#define INDICATOR_NEW_EVENT_SWELL_LEAD		100.0f

/* the dimmer's style over the page while the notices are open (PLACEHOLDER name) */
#define INDICATOR_NEW_EVENT_DIMMER_STYLE	0x9E4C88B9

/* how long after the notices close the button moves back, how long its count and busy
   indicator take to fade, and how long after it they come back (PLACEHOLDER names) */
#define INDICATOR_NEW_EVENT_RESTORE_DELAY	500.0f
#define INDICATOR_NEW_EVENT_PARTS_FADE_TIME	600.0f
#define INDICATOR_NEW_EVENT_PARTS_SHOW_DELAY	800.0f

/* the LED's blink for new events (PLACEHOLDER names) */
#define INDICATOR_NEW_EVENT_LED		2
#define INDICATOR_NEW_EVENT_LED_MODE	4

/* the button's hit box, scaled to the button (PLACEHOLDER name) */
#define INDICATOR_NEW_EVENT_HIT_SCALE	0.7f

/* a command of the new events' item run on its queue (72 bytes) */
class Job_NewEventTaskCommand : public common::SlotJobItem {
public:
	Job_NewEventTaskCommand(SceUInt32 command, IndicatorNewEvent *item)
		: common::SlotJobItem("Job_NewEventTaskCommand", NULL), m_command(command),
		  m_item(item)
	{
	}

	virtual ~Job_NewEventTaskCommand()
	{
	}

	virtual int Run();				/* (0x81165FE0) */
	virtual void Finish(int result);		/* (0x811668E4) */

	SceUInt32 m_command;
	IndicatorNewEvent *m_item;
	vector<LsdbNewEventPtr> m_events;		/* the new events read */
	int m_total;					/* the events read (PLACEHOLDER name) */
	int m_count;					/* the new events counted */
	LsdbNewEventPtr m_popup;			/* the event to pop up */
};

IndicatorNewEvent *g_indicatorNewEvent;		/* (0x815449FC) */

/* the shell's state last told (0x815449E8), and whether the shell told it has started
   (shell event 0, 0x815449EC) */
static int s_shellState;
static bool s_shellStarted;

/* the rowid of the notice pushed last, counted down from -1 (0x815449F0) */
static SceInt64 s_lastNoticeRowid;

/* the count's back's blinks started (0x815449F8) */
static float s_blinkCount;

/* the count's back's blink ticks counted, and how long it has held its alpha (0x81544A00,
   0x81544A04) */
static float s_blinkTicks;
static float s_blinkHold;

/* the shell's state before the one shell events 2 and 4 set, given back by events 3 and 5
   (0x81544A08) */
static int s_savedShellState;

void indicatorNewEventOnDecide(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
void indicatorNewEventOnImageReady(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
void indicatorNewEventOnImageFailed(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
void indicatorNewEventOnBlink(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
void indicatorNewEventOnNoticeDone(int result, SceInt64 rowid, void *arg);
void indicatorNewEventOnPopupDone(int result, SceInt64 rowid, void *arg);
void indicatorNewEventOnPopped(Timer *timer, ui::AnimListener *listener, void *arg1, void *arg2,
			       void *arg3);
void indicatorNewEventOnMainThread(void *data);
void indicatorNewEventOnAppActivated(int code, const void *data, SceSize size, void *arg);
void indicatorNewEventOnAppEvent(int code, const void *data, SceSize size, void *arg);
void indicatorNewEventOnShellEvent(int code, const void *data, SceSize size, void *arg);
void indicatorNewEventTaskEvent(const IndicatorTaskEvent *event);
int indicatorStartInhaleAnim(const IndicatorInhaleAnim *anim);
int indicatorPrimaryNotificationPush(const IndicatorNoticeRequest *request);
int indicatorPrimaryNotificationCancel(const char *titleId);
int indicatorPushNewEvent(IndicatorPushNewEventParam *param);
int indicatorDeleteObject(IndicatorEventItem **item);
void indicatorNewEventLockButton(void);
void indicatorNewEventUnlockButton(void);
bool indicatorNewEventUpdateNotices(void);
ui::Widget *indicatorNewEventGetButton(void);

/* the command started on the item's queue, in its slot */
static inline common::SharedPtr<job::JobItem> indicatorNewEventCommand(IndicatorNewEvent *item,
								       int command)
{
	common::SharedPtr<job::JobItem> job(new Job_NewEventTaskCommand(command, item));
	common::JobSlots *slots = &item->m_slots;

	if (slots != NULL)
		slots->Enqueue(command, item->m_queue, job);
	return common::SharedPtr<job::JobItem>(job);
}

/* the command started, when there is an item */
static inline void indicatorNewEventStart(IndicatorNewEvent *item, int command)
{
	if (item != NULL)
		indicatorNewEventCommand(item, command);
}

/* the event pushed to the notification job, the callback told its result (0x81162B04) */
void indicatorPushNotification(int type, LsdbNewEventPtr event, int option, int target,
			       IndicatorPushNotificationCB callback, void *arg, int flags)
{
	indicatorPushNotificationStart(type, IndicatorPushNotificationParam(target, event,
		event->titleId.c_str(), option, callback, arg, flags));
}

/* the shell's notice made a new event with the next rowid below the last one's, and pushed
   (0x81162CB0) */
void indicatorPushNotice(const IndicatorNoticeRequest *request, int target,
			 IndicatorPushNotificationCB callback, void *arg)
{
	if (s_lastNoticeRowid > 0)
		s_lastNoticeRowid = 0;

	LsdbNewEventPtr event(new LsdbNewEvent);
	SceInt64 rowid = s_lastNoticeRowid - 1;

	event->rowid = rowid;
	s_lastNoticeRowid = rowid;
	event->titleId = request->titleId;
	event->execTitleId = request->execTitleId;
	event->desc = request->desc;
	event->iconPath = request->iconPath;
	event->seId = request->seId;
	event->seVolume = request->seVolume;
	indicatorPushNotification(INDICATOR_PUSH_NOTICE, event, -1, target, callback, arg,
				  request->flags);
}

/* (0x81162F74) */
IndicatorNewEventDb *indicatorNewEventDb(void)
{
	static IndicatorNewEventDb s_db;

	return &s_db;
}

/*
 * The item's template opened in the root and its button found, the button told its decide and
 * its glow's texture events, the notice parts made, the new events read on the item's queue,
 * the new-event database opened and the shell's events listened to; then the count's back, and
 * the item's functions given to the interface (0x81162FD0).
 */
IndicatorNewEvent::IndicatorNewEvent(Plugin *plugin, ui::Widget *root)
	: m_root(root), m_badge(NULL), m_enabled(true), m_plugin(plugin), m_activeApp(),
	  m_lockCount(0), m_iconScale(0.75f, 0.75f, 1.0f, 0.0f), m_themeLoaded(false)
{
	job::JobQueue::Option option;

	option.priority = 0x1000010A;
	option.stackSize = 0x2000;
	option.numThreads = 1;
	m_queue = new job::JobQueue("SceNewIndicatorJobQueue", &option);
	if (root == NULL)
		return;
	{
		plugin->TemplateOpen(root, IDParam(s_empty_name, INDICATOR_NEW_EVENT_TEMPLATE),
				     Plugin::TemplateOpenParam());
	}
	m_widget = root->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_BUTTON), 0);
	if (m_widget != NULL) {
		ui::Widget *button = m_widget;

		button->m_hit_detect_mode = 1;
		button->m_hit_detect_b_mode = 1;
		button->SetHitBox(2, math::v4(INDICATOR_NEW_EVENT_HIT_SCALE, INDICATOR_NEW_EVENT_HIT_SCALE,
					   INDICATOR_NEW_EVENT_HIT_SCALE, INDICATOR_NEW_EVENT_HIT_SCALE),
				  math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		button = m_widget;
		if (!button->m_drag_begun)
			m_widget->m_focus_target = false;
		m_widget->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
					   new ui::EventCBListener(indicatorNewEventOnDecide, this),
					   false);
		m_widget->AddEventListener(ui::Handler::CB_STATE_READY_CACHEIMAGE,
					   new ui::EventCBListener(indicatorNewEventOnImageReady,
								   this),
					   true);
		m_widget->AddEventListener(ui::Handler::CB_COMMAND,
					   new ui::EventCBListener(indicatorNewEventOnImageFailed,
								   this),
					   true);
		((ui::ButtonBase *)m_widget)->SetButtonMode(ui::ButtonBase::BTN_MODE_PRESSOVER);
		m_widget->EnableFocusEvent(false);
		indicatorNoticeManagerCreate(m_plugin);
		indicatorNoticeCacheCreate(m_plugin);
		indicatorNoticeDialogsCreate();
		indicatorNoticePopupsCreate();
		indicatorNewEventStart(this, INDICATOR_NEW_EVENT_CMD_INIT);
		indicatorNewEventDb()->Open();
		common::MainThreadCallList::Register(indicatorNewEventOnMainThread, this);
		SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, indicatorNewEventOnAppActivated, this);
		shellAppEventServerAddListener(0, indicatorNewEventOnAppEvent, this);
		shellAppEventServerAddListener(2, indicatorNewEventOnAppEvent, this);
		SceShell_7FE4E732(4, indicatorNewEventOnShellEvent, this);
		SceShell_7FE4E732(5, indicatorNewEventOnShellEvent, this);
		SceShell_7FE4E732(7, indicatorNewEventOnShellEvent, this);
		SceShell_7FE4E732(9, indicatorNewEventOnShellEvent, this);
		SceShell_7FE4E732(6, indicatorNewEventOnShellEvent, this);
		SceShell_7FE4E732(0, indicatorNewEventOnShellEvent, this);
		SceShell_7FE4E732(2, indicatorNewEventOnShellEvent, this);
		SceShell_7FE4E732(3, indicatorNewEventOnShellEvent, this);
	}
	m_badge = root->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_BADGE), 0);
	g_indicatorNewEvent = this;
	g_indicatorInterface->TaskEvent = indicatorNewEventTaskEvent;
	g_indicatorInterface->StartInhaleAnim = indicatorStartInhaleAnim;
	g_indicatorInterface->PrimaryNotificationPush = indicatorPrimaryNotificationPush;
	g_indicatorInterface->PrimaryNotificationCancel = indicatorPrimaryNotificationCancel;
	g_indicatorInterface->PushNewEvent = indicatorPushNewEvent;
	g_indicatorInterface->DeleteObject = indicatorDeleteObject;
	g_indicatorInterface->LockButton = indicatorNewEventLockButton;
	g_indicatorInterface->UnlockButton = indicatorNewEventUnlockButton;
	g_indicatorInterface->UpdateNotices = indicatorNewEventUpdateNotices;
	g_indicatorInterface->GetButton = indicatorNewEventGetButton;
}

/* the queue and the commands stopped, the notice parts destroyed, the database closed and the
   shell's events no longer listened to (0x811636B0) */
IndicatorNewEvent::~IndicatorNewEvent()
{
	if (m_queue != NULL) {
		delete m_queue;
		m_queue = NULL;
	}
	m_slots.CancelAll();
	if (m_noticeJob.get() != NULL) {
		m_noticeJob->Cancel();
		m_noticeJob.reset();
	}
	if (m_imageJob.get() != NULL) {
		m_imageJob->Cancel();
		m_imageJob.reset();
	}
	indicatorNoticeManagerDestroy();
	indicatorNoticeCacheDestroy();
	indicatorNoticeDialogsDestroy();
	indicatorNoticePopupsDestroy();
	common::MainThreadCallList::Unregister(indicatorNewEventOnMainThread, this);
	indicatorNewEventDb()->Close();
	SceShell_D4C7D2B7(0, indicatorNewEventOnShellEvent, this);
	SceShell_D4C7D2B7(6, indicatorNewEventOnShellEvent, this);
	SceShell_D4C7D2B7(7, indicatorNewEventOnShellEvent, this);
	SceShell_D4C7D2B7(9, indicatorNewEventOnShellEvent, this);
	SceShell_D4C7D2B7(2, indicatorNewEventOnShellEvent, this);
	SceShell_D4C7D2B7(3, indicatorNewEventOnShellEvent, this);
	SceShell_D4C7D2B7(4, indicatorNewEventOnShellEvent, this);
	SceShell_D4C7D2B7(5, indicatorNewEventOnShellEvent, this);
	shellAppEventServerRemoveListener(2, indicatorNewEventOnAppEvent, this);
	shellAppEventServerRemoveListener(0, indicatorNewEventOnAppEvent, this);
	SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, indicatorNewEventOnAppActivated, this);
	g_indicatorNewEvent = NULL;
}

/* (0x81163992) */
intrusive_ptr<cache::CacheImageInterface> IndicatorNewEvent::GetNoNoticeImage()
{
	return m_noNoticeImage;
}

/* the count of the new events (none in the idu mode) written on the button with the theme's
   image for it, when asked, and the button's scale for it (0x811639A6) */
void IndicatorNewEvent::SetCount(int count, bool update)
{
	if (shellStartupGetIduMode() == 1)
		count = 0;
	if (update) {
		intrusive_ptr<cache::CacheImageInterface> image;
		char text[64];

		if (count <= 0) {
			sce_paf_snprintf(text, sizeof(text), "");
			image = GetNoNoticeImage();
		} else {
			sce_paf_snprintf(text, sizeof(text), "%d", count);
			image = GetNewNoticeImage();
		}

		ui::Text *countText =
			m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT), 0) !=
				NULL &&
			m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT), 0)
				->IsInherit(ui::Text::TypeName()) ?
			(ui::Text *)m_widget->FindChild(IDParam(s_empty_name,
								INDICATOR_NEW_EVENT_COUNT), 0) :
			NULL;

		if (countText != NULL) {
			{
				string utf8(text);
				wstring str;

				common::Utf8ToUtf16(utf8, &str, 0);
				countText->SetString(str);
			}
			countText->Show(0xff, NULL, NULL, 0.0f);
		}
		m_widget->SetDrawObjMaskBit(&image, 0);
	}

	if (count > 0)
		m_iconScale = math::v4(1.0f, 1.0f, 1.0f, 0.0f);
	else
		m_iconScale = math::v4(0.75f, 0.75f, 1.0f, 0.0f);
}

/*
 * The theme's notice images loaded, or the plugin's where it has none, the count's text and its
 * back given the theme's colors, and the count written again (0x81163CD2).
 */
void IndicatorNewEvent::UpdateTheme(const IndicatorStatus &status)
{
	string newPath = status.noNoticeFile;

	if (newPath.length() != 0) {
		cache::CacheImage::Option option;

		option.auto_delete = true;
		option.retry = true;
		option.retry_count = -1;
		option.reserved = true;
		option.read_size = 0;
		option.read_offset = 0;
		option.max_size_kb = 0;
		option.open_func = NULL;
		option.open_userdata = NULL;
		memset(&option.sub_rect, 0, sizeof(option.sub_rect));
		option.scale = image::ImageExtent(INDICATOR_NEW_EVENT_IMAGE_W,
						  INDICATOR_NEW_EVENT_IMAGE_H);
		option.resize_type = image::ImageResizeType_NearestNeighbor;
		option.direct_load = false;
		option.image_order = ImageOrder_NONE;
		option.image_order_param = 0;
		option.check_image = NULL;
		option.check_image_userdata = NULL;
		option.surface_flag = false;
		indicatorCacheCreateFile(newPath, m_newNoticeImage, &option, NULL);
	} else {
		IDParam id(s_empty_name, INDICATOR_NEW_EVENT_NEW_IMAGE);

		indicatorCacheCreateTexture(g_indicatorPlugin, id, m_newNoticeImage, NULL, NULL);
	}

	string nonePath = status.newNoticeFile;

	if (nonePath.length() != 0) {
		cache::CacheImage::Option option;

		option.auto_delete = true;
		option.retry = true;
		option.retry_count = -1;
		option.reserved = true;
		option.read_size = 0;
		option.read_offset = 0;
		option.max_size_kb = 0;
		option.open_func = NULL;
		option.open_userdata = NULL;
		memset(&option.sub_rect, 0, sizeof(option.sub_rect));
		option.scale = image::ImageExtent(INDICATOR_NEW_EVENT_IMAGE_W,
						  INDICATOR_NEW_EVENT_IMAGE_H);
		option.resize_type = image::ImageResizeType_NearestNeighbor;
		option.direct_load = false;
		option.image_order = ImageOrder_NONE;
		option.image_order_param = 0;
		option.check_image = NULL;
		option.check_image_userdata = NULL;
		option.surface_flag = false;
		indicatorCacheCreateFile(nonePath, m_noNoticeImage, &option, NULL);
	} else {
		IDParam id(s_empty_name, INDICATOR_NEW_EVENT_NONE_IMAGE);

		indicatorCacheCreateTexture(g_indicatorPlugin, id, m_noNoticeImage, NULL, NULL);
	}
	m_themeLoaded = (newPath.length() | nonePath.length()) != 0;

	ui::Text *countText =
		m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT), 0) != NULL &&
		m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT), 0)
			->IsInherit(ui::Text::TypeName()) ?
		(ui::Text *)m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT),
						0) :
		NULL;

	if (countText != NULL)
		countText->SetColor(status.noticeFontColor, NULL, ui::Widget::ANIM_COLOR, 0);
	if (m_badge != NULL)
		m_badge->SetColor(status.noticeGlowColor, NULL, ui::Widget::ANIM_COLOR, 0);
	SetCount(m_count, true);
}

/* the button put back on the bar while it is shown, then the theme taken (0x811640C6) */
void IndicatorNewEvent::OnStatus(IndicatorStatus status)
{
	m_enabled = status.visible;
	if (status.visible && m_root != NULL) {
		m_widget->SetParent(m_root, NULL, 0);
		m_widget->SetPos(math::v4(-23.0f, 2.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_MOVE, 0,
				 0.0f, NULL, NULL);
		m_widget->SetAlign(0, m_widget->m_align_y, m_widget->m_align_z, NULL);
		m_widget->Show(0xff, NULL, NULL, 0.0f);
	}
	UpdateTheme(status);
}

/*
 * The button decided, on a page and while nothing holds it: the count's back stopped blinking
 * and faded out, then the notices' list closed when it is open, or the notices shown cancelled
 * and the list opened on the page with the new events (0x8116419E).
 */
void IndicatorNewEvent::OpenNotices()
{
	ui::Widget *page = m_widget->m_parent;

	if (page == NULL || m_lockCount != 0)
		return;

	ui::Widget *badge = m_badge != NULL && m_badge->IsInherit(ui::Plane::TypeName()) ?
		m_badge : NULL;

	if (badge != NULL &&
	    badge->m_map_t_listener.find(INDICATOR_NEW_EVENT_BADGE_TIMER) !=
		    badge->m_map_t_listener.end()) {
		m_badge->KillTimer(INDICATOR_NEW_EVENT_BADGE_TIMER);
		badge->SetMetaAlpha(0.0f, new Timer(INDICATOR_NEW_EVENT_FADE_TIME));
	}
	if (indicatorNoticeManager()->IsOpen()) {
		indicatorNoticeManager()->Close(2, 0);
	} else {
		indicatorNoticeCache()->CancelAll();
		indicatorNoticeManager()->Open(page, m_events);
	}
}

/* the button's busy indicator, when it has one */
static inline ui::BusyIndicator *indicatorNewEventBusy(IndicatorNewEvent *item)
{
	return item->m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_BUSY), 0) !=
			       NULL &&
		       item->m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_BUSY), 0)
			       ->IsInherit(ui::BusyIndicator::TypeName()) ?
		(ui::BusyIndicator *)item->m_widget->FindChild(IDParam(s_empty_name,
								       INDICATOR_NEW_EVENT_BUSY),
							       0) :
		NULL;
}

/* the busy indicator shown and started, unless it runs; -1 without it (0x811642EC) */
int IndicatorNewEvent::ShowBusy()
{
	ui::BusyIndicator *busy = indicatorNewEventBusy(this);

	if (busy == NULL)
		return -1;
	busy->Show(0xff, NULL, NULL, 0.0f);
	switch (busy->m_anim_state) {
	case ui::BusyIndicator::ANIM_STOPPED:
	case ui::BusyIndicator::ANIM_STOPPING:
		busy->Start();
		break;
	}
	return 0;
}

/* the busy indicator stopped, when it starts or runs; -1 without it (0x811644C6) */
int IndicatorNewEvent::HideBusy()
{
	ui::BusyIndicator *busy = indicatorNewEventBusy(this);

	if (busy == NULL)
		return -1;
	switch (busy->m_anim_state) {
	case ui::BusyIndicator::ANIM_STARTING:
	case ui::BusyIndicator::ANIM_RUNNING:
		busy->Stop();
		break;
	}
	return 0;
}

/* the blinks counted, and the count's back blinked again from now on (0x81164664) */
void IndicatorNewEvent::StartBlink()
{
	if (m_badge == NULL)
		return;
	s_blinkCount += 1.0f;
	if (m_badge->m_map_t_listener.find(INDICATOR_NEW_EVENT_BADGE_TIMER) !=
	    m_badge->m_map_t_listener.end())
		m_badge->KillTimer(INDICATOR_NEW_EVENT_BADGE_TIMER);
	m_badge->SetTimerListener(INDICATOR_NEW_EVENT_BADGE_TIMER, INDICATOR_NEW_EVENT_BLINK_TIME,
				  new ui::EventCBListener(indicatorNewEventOnBlink, NULL));
}

/*
 * The count written again and the button moved onto the bar while it is shown, bouncing, or
 * popped first when asked (cb told when the pop ends), else shown and moved off it; it is
 * scaled back to its scale for the count (0x8116472A).
 */
int IndicatorNewEvent::Animate(ui::AnimCB cb, bool pop, float delay)
{
	SetCount(m_count, true);

	math::v4 scale = m_iconScale;
	ui::Widget *button = m_widget;
	int func;
	math::v4 pos;
	float factor;

	if (m_enabled) {
		func = pop ? Timer::FUNC_ELASTIC_OUT : Timer::FUNC_BOUNCE_OUT;
		pos = math::v4(-23.0f, 2.0f, 0.0f, 0.0f);
		factor = func == Timer::FUNC_ELASTIC_OUT ? 5.0f : 2.0f;
	} else {
		func = Timer::FUNC_EXPO_OUT;
		pos = math::v4(440.0f, 232.0f, 0.0f, 0.0f);
		factor = 1.0f;
		button->Show(0xff, NULL, NULL, 0.0f);
		button = m_widget;
	}
	button->SetPos(pos, NULL, ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
	if (pop)
		m_widget->SetScale(math::v4(INDICATOR_NEW_EVENT_POP_SCALE, INDICATOR_NEW_EVENT_POP_SCALE,
					    1.0f, 0.0f),
				   new Timer(INDICATOR_NEW_EVENT_ANIM_TIME, Timer::FUNC_BACK_OUT),
				   ui::Widget::ANIM_SCALE, 0, delay, cb, this);
	m_widget->SetScale(scale, new Timer(factor * INDICATOR_NEW_EVENT_ANIM_TIME, func),
			   ui::Widget::ANIM_SCALE, 0, pop ? 0.0f : delay, NULL, this);
	m_widget->SetPos(pos + math::v4(0.0f, 0.0f, 0.0f, 0.0f),
			 new Timer(factor * INDICATOR_NEW_EVENT_ANIM_TIME, func),
			 ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
	return 0;
}

/* the button's scale animation stopped, the button at its scale for the count (0x811649D0) */
int IndicatorNewEvent::StopScale()
{
	if (m_widget->FindAnim(ui::Widget::ANIM_SCALE)) {
		m_widget->KillAnim(ui::Widget::ANIM_SCALE);
		m_widget->SetScale(m_iconScale, NULL, ui::Widget::ANIM_SCALE, 0, 0.0f, NULL, NULL);
	}
	return 0;
}

/* the new events' new flags cleared on the item's queue (0x81164A18) */
void indicatorNewEventClearNewFlags(IndicatorNewEvent *item)
{
	indicatorNewEventStart(item, INDICATOR_NEW_EVENT_CMD_CLEAR_NEW);
}

/*
 * The notices' list item for a new event by its msgType: a system task's or a transfer's, a
 * friend's, a trophy's or any other message's; NULL for an event without a title (0x81164B7A).
 */
IndicatorEventItem *IndicatorNewEvent::CreateItem(LsdbNewEventPtr event,
						  IndicatorEventItemCB callback)
{
	const LsdbNewEvent *e = event.get();

	if (e->titleId.length() == 0)
		return NULL;
	switch (e->msgType & 0xFFFFFF00) {
	case 0x0:
		if ((e->msgType & 0xFFFFFFF0) == 0x40 || (e->msgType & 0xFFFFFFF0) == 0x80)
			return new IndicatorEventTransferItem(*e, callback);
		return new IndicatorEventTaskItem(*e, callback);
	case 0xA00:
		return new IndicatorEventFriendItem(*e, callback);
	default:
		if (e->titleId == "NPXS10008")
			return new IndicatorEventTrophyItem(*e, callback);
		return new IndicatorEventIconItem(*e, callback);
	}
}

/* the theme's images taken again on the item's queue (0x81164FA4) */
void indicatorNewEventReloadTheme(IndicatorNewEvent *item)
{
	indicatorNewEventStart(item, INDICATOR_NEW_EVENT_CMD_THEME);
}

/* the button's texture and the theme's images let go (0x81165106) */
void indicatorNewEventDropTextures(IndicatorNewEvent *item)
{
	item->m_widget->SetTexture(intrusive_ptr<graph::Surface>(), 0, 0);
	item->m_newNoticeImage.clear();
	item->m_noNoticeImage.clear();
}

/* the LED blinked for new events, unless the bar is shown as usual or an application is up
   (0x8116516E) */
void indicatorNewEventBlinkLed(void)
{
	if (!indicatorIsShownNormal() && !shellBootHasAppStack()) {
		ShellLedParam param;

		param.pattern = 8;
		param.duration = 33;
		shellLedSetMode(INDICATOR_NEW_EVENT_LED, INDICATOR_NEW_EVENT_LED_MODE, &param);
	}
}

/* the busy indicator shown while a system task of type 1, 4 or 64 is among the events, else
   stopped (0x811651B2) */
__attribute__((noinline)) int IndicatorNewEvent::UpdateBusy(vector<LsdbNewEventPtr> &events)
{
	for (vector<LsdbNewEventPtr>::iterator it = events.begin(); it != events.end();
	     ++it) {
		SceInt64 type = (*it)->msgType;

		if ((type & 0xFFFFFF00) == 0 && (type == 1 || type == 4 || type == 64))
			return ShowBusy();
	}
	return HideBusy();
}

/* the button wobbled and swollen, its count and busy indicator hidden (0x81165224) */
int IndicatorNewEvent::Burst()
{
	m_widget->SetScale(m_iconScale, NULL, ui::Widget::ANIM_SCALE, 0, 0.0f, NULL, NULL);

	float param[4] = { 6.0f, 0.0f, 0.0f, 0.0f };
	ui::Widget *button = m_widget;
	Timer *wobble = new Timer(INDICATOR_NEW_EVENT_WOBBLE_TIME, Timer::FUNC_CUSTOM_ELASTIC_OUT,
				  Timer::REPEAT_NONE, 0, param);

	button->SetScale(math::v4(INDICATOR_NEW_EVENT_WOBBLE_SCALE, INDICATOR_NEW_EVENT_WOBBLE_SCALE,
				  0.0f, 0.0f),
			 wobble, ui::Widget::ANIM_SCALE, 2, 0.0f, NULL, NULL);

	float delay = INDICATOR_NEW_EVENT_SWELL_START - INDICATOR_NEW_EVENT_SWELL_LEAD;

	if (delay < 0.0f)
		delay = 0.0f;
	button = m_widget;

	Timer *swell = new Timer(INDICATOR_NEW_EVENT_SWELL_TIME, Timer::FUNC_BACK_OUT);

	button->SetScale(math::v4(INDICATOR_NEW_EVENT_SWELL_SCALE, INDICATOR_NEW_EVENT_SWELL_SCALE,
				  0.0f, 0.0f),
			 swell, ui::Widget::ANIM_SCALE, 0, delay, NULL, NULL);

	ui::Text *countText =
		m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT), 0) != NULL &&
		m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT), 0)
			->IsInherit(ui::Text::TypeName()) ?
		(ui::Text *)m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT),
						0) :
		NULL;

	if (countText != NULL)
		countText->Hide(0xff, new Timer(INDICATOR_NEW_EVENT_PARTS_FADE_TIME), NULL, 0.0f);

	ui::BusyIndicator *busy = indicatorNewEventBusy(this);

	if (busy != NULL)
		busy->Hide(0xff, new Timer(INDICATOR_NEW_EVENT_PARTS_FADE_TIME), NULL, 0.0f);
	return 0;
}

/* the button moved back after the notices close, its count and busy indicator faded in again
   after it (0x811657C8) */
int IndicatorNewEvent::Restore()
{
	Animate(NULL, false, INDICATOR_NEW_EVENT_RESTORE_DELAY);

	ui::Text *countText =
		m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT), 0) != NULL &&
		m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT), 0)
			->IsInherit(ui::Text::TypeName()) ?
		(ui::Text *)m_widget->FindChild(IDParam(s_empty_name, INDICATOR_NEW_EVENT_COUNT),
						0) :
		NULL;

	if (countText != NULL) {
		countText->Hide(0xff, NULL, NULL, 0.0f);
		countText->Show(0xff, new Timer(INDICATOR_NEW_EVENT_PARTS_FADE_TIME), NULL,
				INDICATOR_NEW_EVENT_PARTS_SHOW_DELAY);
	}

	ui::BusyIndicator *busy = indicatorNewEventBusy(this);

	if (busy != NULL) {
		busy->Hide(0xff, NULL, NULL, 0.0f);
		busy->Show(0xff, new Timer(INDICATOR_NEW_EVENT_PARTS_FADE_TIME), NULL,
			   INDICATOR_NEW_EVENT_PARTS_SHOW_DELAY);
	}
	return 0;
}

/* the page dimmed under the notices, the dimmer made the first time (0x81165C62) */
void IndicatorNewEvent::ShowDimmer()
{
	ui::Scene *page = m_widget->m_scene;
	ui::Widget *dimmer = page->FindChild(IDParam("news_temporary_dimmer"), 0);

	if (dimmer == NULL) {
		dimmer = g_indicatorPlugin->CreateWidget(page, "plane",
							 IDParam("news_temporary_dimmer"),
							 IDParam(s_empty_name,
								 INDICATOR_NEW_EVENT_DIMMER_STYLE));
		dimmer->SetAdjust(4, 4, 0);
		dimmer->SetParent(dimmer->m_parent, NULL, 5);
	}
	ScePafWidget_44B22ACA(dimmer, common::transition::Type_FadeinSlow, 0.0f, NULL, NULL);
}

/* the dimmer faded out and deleted after it (0x81165E2A) */
void IndicatorNewEvent::HideDimmer()
{
	ui::Scene *page = m_widget->m_scene;
	ui::Widget *dimmer = page->FindChild(IDParam("news_temporary_dimmer"), 0);

	if (dimmer != NULL)
		ScePafWidget_5A9793B0(dimmer, ScePafWidget_F1176ACC(dimmer,
					common::transition::Type_FadeinSlow, 0.0f, NULL, NULL));
}

/* the glow's image in: the theme's glow on the button once the theme's images are in, else
   none (0x81165EEE) */
void indicatorNewEventOnImageReady(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	IndicatorNewEvent *item = (IndicatorNewEvent *)data;

	if (self != NULL && ((ui::Widget *)self)->IsInherit(ui::Widget::TypeName())) {
		ui::Widget *button = (ui::Widget *)self;

		if (item->m_themeLoaded)
			button->SetTexture(g_indicatorPlugin->GetTexture(
						   IDParam(s_empty_name, INDICATOR_NEW_EVENT_GLOW)),
					   0, 1);
		else
			button->SetTexture(intrusive_ptr<graph::Surface>(), 0, 1);
	}
}

/* (0x81165FD8) */
void indicatorNewEventOnImageFailed(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	sce_paf_printf_dummy();
}

/*
 * The command's work on the item's queue: the first reading's popups cleared on a demo unit, a
 * new event's popup taken (shown unless its kind waits for home or notices are off), the new
 * flags cleared, the active application's events no longer new, the popup cleared; then the new
 * events read (0x81165FE0).
 */
int Job_NewEventTaskCommand::Run()
{
	int ret = -1;

	if (IsCanceled())
		return ret;

	IndicatorNewEventDb *db = indicatorNewEventDb();

	if (db == NULL)
		return ret;

	IndicatorNewEvent *item = m_item;

	switch (m_command) {
	case INDICATOR_NEW_EVENT_CMD_INIT:
	case INDICATOR_NEW_EVENT_CMD_THEME:
	case INDICATOR_NEW_EVENT_CMD_NEW_EVENT:
		if (m_command == INDICATOR_NEW_EVENT_CMD_INIT) {
			if (shellStartupGetIduMode() == 1) {
				db->ClearPopups();
				db->ClearNewFlags();
			}
			ret = 0;
		} else if (m_command == INDICATOR_NEW_EVENT_CMD_NEW_EVENT) {
			LsdbNewEvent event;

			ret = db->GetPopup(&event);
			if (ret < 0)
				return ret;
			if (shellStartupGetIduMode() == 1)
				db->ClearNewFlags();

			SceAppMgrAppStatus status;

			if (s_shellState == INDICATOR_SHELL_STATE_APP &&
			    sceAppMgrGetStatusByAppId(-4, &status) == 0) {
				bool active = sce_paf_strcmp(event.execTitleId.c_str(),
							     string(status.appName).c_str()) == 0;

				if (active && event.newFlag) {
					event.newFlag = false;
					db->UpdateNewFlag(&event);
				}
			}
			if (indicatorNoticeManager()->IsOpen()) {
				ret = db->ClearPopups();
			} else {
				if ((event.actType == 3 && s_shellState != INDICATOR_SHELL_STATE_APP) ||
				    shellStartupGetIduMode() == 1)
					goto clear;
				m_popup = LsdbNewEventPtr(new LsdbNewEvent(event));

				int on;

				indicatorGetNotification(&on);
				if (((event.notifyFlags & 0x10) != 0 || on == 1) &&
				    s_shellState != INDICATOR_SHELL_STATE_HOME)
					goto done;
clear:
				ret = db->ClearPopup(&event.rowid);
				if (ret >= 0)
					ret = 1;
done:
				;
			}
		} else {
			ret = 0;
		}
		break;
	case INDICATOR_NEW_EVENT_CMD_POPUP: {
		LsdbNewEvent event;

		ret = db->GetPopup(&event);
		if (ret < 0)
			return ret;

		SceInt64 rowid = event.rowid;

		ret = db->ClearPopup(&rowid);
		return ret;
	}
	case INDICATOR_NEW_EVENT_CMD_CLEAR_NEW:
		return db->ClearNewFlags();
	case INDICATOR_NEW_EVENT_CMD_APP_SEEN: {
		char titleId[32];
		SceAppMgrAppStatus status;

		sce_paf_memset(titleId, 0, sizeof(titleId));
		if (sceAppMgrGetStatusByAppId(item->m_activeApp.appId, &status) == 0)
			sce_paf_strncpy(titleId, status.appName, sizeof(titleId));
		ret = db->GetEvents(&m_events, INDICATOR_NEW_EVENT_MAX_EVENTS, &m_total, &m_count,
				    true);
		if (ret >= 0) {
			for (vector<LsdbNewEventPtr>::iterator it = m_events.begin();
			     it != m_events.end(); ++it) {
				if (sce_paf_strcmp((*it)->execTitleId.c_str(),
						   string(titleId).c_str()) == 0) {
					LsdbNewEvent *e = it->get();

					if (e->newFlag) {
						e->newFlag = false;
						ret = db->UpdateNewFlag(e);
					}
				}
			}
		}
		return ret;
	}
	default:
		return ret;
	}
	db->GetEvents(&m_events, INDICATOR_NEW_EVENT_MAX_EVENTS, &m_total, &m_count, true);
	return ret;
}

/* the LED blinked for a popup the user is told of with it, at home with the bar hidden and no
   application stack (inlined in Finish) */
static inline __attribute__((always_inline)) void
indicatorNewEventPopupLed(const LsdbNewEventPtr &event)
{
	int on;

	indicatorGetNotification(&on);

	const LsdbNewEvent *popup = event.get();

	if (popup != NULL && on == 1 && (popup->notifyFlags & 8) != 0 && popup->newFlag &&
	    !indicatorIsShownNormal() && !shellBootHasAppStack()) {
		ShellLedParam param;

		param.pattern = 8;
		param.duration = 33;
		shellLedSetMode(INDICATOR_NEW_EVENT_LED, INDICATOR_NEW_EVENT_LED_MODE, &param);
	}
}

/*
 * The events read given to the item and the notices; then the button moved by the command: in
 * at the first reading, popped (and blinking) for a new event while the notices are open, the
 * popup pushed to the notification job or the LED blinked at home, or out once no new event is
 * left (0x811668E4).
 */
void Job_NewEventTaskCommand::Finish(int result)
{
	IndicatorNewEvent *item = m_item;

	if (item != NULL && result >= 0 && IsCanceled() != true) {
		switch (m_command) {
		case INDICATOR_NEW_EVENT_CMD_INIT:
		case INDICATOR_NEW_EVENT_CMD_THEME:
		case INDICATOR_NEW_EVENT_CMD_NEW_EVENT: {
			int count = item->m_count;

			item->m_events.clear();
			for (vector<LsdbNewEventPtr>::iterator it = m_events.begin();
			     it != m_events.end(); ++it)
				item->m_events.push_back(*it);
			item->m_count = m_count;
			if (m_command == INDICATOR_NEW_EVENT_CMD_INIT) {
				item->Animate(NULL, true, 0.0f);
				break;
			}
			item->UpdateBusy(m_events);
			indicatorNoticeManager()->Update(m_events);

			bool open = indicatorNoticeManager()->IsOpen();

			if (m_command == INDICATOR_NEW_EVENT_CMD_NEW_EVENT) {
				if (open) {
					item->Animate(indicatorNewEventOnPopped, true, 0.0f);
				} else if (result > 0) {
					item->Animate(NULL, false, 0.0f);
					if (s_shellState == INDICATOR_SHELL_STATE_HOME)
						indicatorNewEventPopupLed(m_popup);
				} else {
					const LsdbNewEvent *e = m_popup.get();

					if (e != NULL) {
						s_lastNoticeRowid = e->rowid;

						LsdbNewEventPtr popup = m_popup;

						if (m_item != NULL)
							indicatorPushNotification(0, popup, 0,
								(int)m_item->m_widget->m_parent,
								indicatorNewEventOnPopupDone, m_item, 0);
					}
				}
			} else if (count > m_count) {
				item->Animate(NULL, m_count > 0, 0.0f);
			}
			break;
		}
		case INDICATOR_NEW_EVENT_CMD_POPUP:
			s_lastNoticeRowid = 0;
			break;
		}
	}
	common::SlotJobItem::Finish(result);
}

/* the shell's notice done with: no notice is the last one pushed any more (0x81166BA8) */
void indicatorNewEventOnNoticeDone(int result, SceInt64 rowid, void *arg)
{
	if (rowid == s_lastNoticeRowid)
		s_lastNoticeRowid = 0;
}

/* the popup done with: forgotten when it was cancelled, else cleared from the database on the
   item's queue (0x81166BC8) */
void indicatorNewEventOnPopupDone(int result, SceInt64 rowid, void *arg)
{
	if (rowid == s_lastNoticeRowid) {
		if (result == 3)
			s_lastNoticeRowid = 0;
		else
			indicatorNewEventStart((IndicatorNewEvent *)arg, INDICATOR_NEW_EVENT_CMD_POPUP);
	}
}

/* the button popped for a new event: its count's back blinks (0x81166D50) */
void indicatorNewEventOnPopped(Timer *timer, ui::AnimListener *listener, void *arg1, void *arg2,
			       void *arg3)
{
	g_indicatorNewEvent->StartBlink();
}

/*
 * The count's back's blink timer, every 100 ms: once the blinks started have run their time, the
 * timer stopped and the back faded out; else the back faded out after holding lit, and in after
 * holding out (0x81166D62).
 */
void indicatorNewEventOnBlink(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	if (self == NULL || !((ui::Widget *)self)->IsInherit(ui::Plane::TypeName()))
		return;

	ui::Widget *badge = (ui::Widget *)self;
	float alpha = badge->m_meta_alpha;

	if (s_blinkTicks * INDICATOR_NEW_EVENT_BLINK_TIME >
	    s_blinkCount * INDICATOR_NEW_EVENT_BLINK_SPAN * 1000.0f) {
		s_blinkTicks = 0.0f;
		s_blinkCount = 0.0f;
		badge->KillTimer(INDICATOR_NEW_EVENT_BADGE_TIMER);
		if (alpha > 0.0f) {
			badge->KillAnimType(ui::Widget::ANIM_META_ALPHA);
			badge->SetMetaAlpha(0.0f, new Timer(INDICATOR_NEW_EVENT_FADE_TIME));
		}
		return;
	}
	s_blinkTicks += 1.0f;
	if (alpha == 1.0f) {
		if (s_blinkHold > INDICATOR_NEW_EVENT_BLINK_LIT)
			badge->SetMetaAlpha(0.0f, new Timer(INDICATOR_NEW_EVENT_FADE_TIME));
		else
			s_blinkHold += INDICATOR_NEW_EVENT_BLINK_TIME;
	} else if (alpha == 0.0f) {
		if (s_blinkHold > 0.0f)
			badge->SetMetaAlpha(1.0f, new Timer(INDICATOR_NEW_EVENT_FADE_TIME));
		else
			s_blinkHold += INDICATOR_NEW_EVENT_BLINK_TIME;
	} else if (s_blinkHold > 0.0f) {
		s_blinkHold = 0.0f;
	}
}

/*
 * A popup is waiting to be pushed: the shell has started and has no application stack, no
 * notice of the shell's is out, no new-event or popup command runs and the database holds
 * popups (0x81167018).
 */
static __attribute__((noinline)) bool indicatorNewEventHasPopup(void)
{
	if (!s_shellStarted || shellBootHasAppStack() || s_lastNoticeRowid != 0 ||
	    g_indicatorNewEvent->m_slots.Contains(INDICATOR_NEW_EVENT_CMD_NEW_EVENT) ||
	    g_indicatorNewEvent->m_slots.Contains(INDICATOR_NEW_EVENT_CMD_POPUP))
		return false;
	if (indicatorNewEventDb()->GetPopupCount() > 0)
		return true;
	return false;
}

/* (0x811670DA) */
ui::Widget *indicatorNewEventGetButton(void)
{
	return g_indicatorNewEvent->m_widget;
}

/* on the main thread: the inhale animation started while the bar is shown, then freed
   (0x811670E8) */
static void indicatorNewEventInhaleAnim(void *data)
{
	IndicatorCallArg<IndicatorInhaleAnim> *arg = (IndicatorCallArg<IndicatorInhaleAnim> *)data;

	if (indicatorIsShownNormal() && g_indicatorNewEvent != NULL)
		indicatorInhaleAnimStart(&arg->value);
	delete arg;
}

/* on the main thread: the shell's notice pushed on the button's parent, then freed
   (0x81167132) */
static void indicatorNewEventNoticePush(void *data)
{
	IndicatorCallArg<IndicatorNoticeRequest> *arg =
		(IndicatorCallArg<IndicatorNoticeRequest> *)data;

	if (g_indicatorNewEvent != NULL) {
		ui::Widget *parent = g_indicatorNewEvent->m_widget->m_parent;

		if (parent != NULL)
			indicatorPushNotice(&arg->value, (int)parent,
					    indicatorNewEventOnNoticeDone, NULL);
	}
	delete arg;
}

/* on the main thread: the title's notices cancelled, then the title freed (0x811671CE) */
static void indicatorNewEventNoticeCancel(void *data)
{
	string *titleId = (string *)data;

	indicatorNoticeCache()->Cancel(titleId->c_str());
	delete titleId;
}

/* the button decided: the notices opened, but not on a demo unit (0x81167202) */
void indicatorNewEventOnDecide(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	if (shellStartupGetIduMode() != 1 && data != NULL)
		((IndicatorNewEvent *)data)->OpenNotices();
}

/*
 * Every main-thread turn, unless the events are being read: a waiting popup pushed, or the
 * events read again when the database changed (0x81167218).
 */
void indicatorNewEventOnMainThread(void *data)
{
	IndicatorNewEvent *item = (IndicatorNewEvent *)data;

	if (g_indicatorNewEvent->m_slots.Contains(INDICATOR_NEW_EVENT_CMD_THEME))
		return;
	if (indicatorNewEventHasPopup())
		indicatorNewEventStart(item, INDICATOR_NEW_EVENT_CMD_NEW_EVENT);
	else if (indicatorNewEventDb()->IsChanged() && item != NULL)
		indicatorNewEventStart(item, INDICATOR_NEW_EVENT_CMD_THEME);
}

/* an application brought to the front: kept, and its new events marked seen (0x81167544) */
void indicatorNewEventOnAppActivated(int code, const void *data, SceSize size, void *arg)
{
	IndicatorNewEvent *item = (IndicatorNewEvent *)arg;

	ShellAppActivatedEvent event = *(const ShellAppActivatedEvent *)data;

	item->m_activeApp = event;
	indicatorNewEventStart(item, INDICATOR_NEW_EVENT_CMD_APP_SEEN);
}

/*
 * An application's event: the notifications application's start clears the new flags, and an
 * application's end (other than NPXS19999) stops the LED (0x811676CC).
 */
void indicatorNewEventOnAppEvent(int code, const void *data, SceSize size, void *arg)
{
	IndicatorNewEvent *item = (IndicatorNewEvent *)arg;
	const char *titleId = (const char *)data;

	if (code == 0 && sce_paf_strncmp("NPXS10020", titleId, 10) == 0 && item != NULL)
		indicatorNewEventStart(item, INDICATOR_NEW_EVENT_CMD_CLEAR_NEW);
	if (code == 2 && sce_paf_strncmp("NPXS19999", titleId, 10) != 0)
		shellLedSetMode(INDICATOR_NEW_EVENT_LED, 0, NULL);
}

/* the shell's events: its start, and the states it goes through (0x8116788A) */
void indicatorNewEventOnShellEvent(int code, const void *data, SceSize size, void *arg)
{
	switch (code) {
	case 0:
		s_shellStarted = true;
		break;
	case 2:
		s_savedShellState = s_shellState;
		s_shellState = INDICATOR_SHELL_STATE_HOME;
		Framework::Instance()->GetEnvironmentInstance()->HideSceneEntry(
			INDICATOR_NEW_EVENT_SCENE_ENTRY);
		break;
	case 3:
		s_shellState = s_savedShellState;
		shellLedSetMode(INDICATOR_NEW_EVENT_LED, 0, NULL);
		Framework::Instance()->GetEnvironmentInstance()->ShowSceneEntry(
			INDICATOR_NEW_EVENT_SCENE_ENTRY);
		break;
	case 4:
		s_savedShellState = s_shellState;
		s_shellState = INDICATOR_SHELL_STATE_BUSY;
		if (indicatorNoticeManager()->IsOpen())
			indicatorNoticeManager()->Close(2, 0);
		break;
	case 5:
		s_shellState = s_savedShellState;
		if (indicatorNoticeManager()->IsOpen())
			indicatorNoticeManager()->Close(2, 0);
		break;
	case 6:
		s_shellState = INDICATOR_SHELL_STATE_MENU;
		break;
	case 7:
		s_shellState = INDICATOR_SHELL_STATE_APP;
		if (s_lastNoticeRowid != 0)
			indicatorNoticeCache()->Cancel("force");
		break;
	case 9:
		indicatorNoticeCache()->Cancel("force");
		break;
	}
}

/*
 * A new event made an item of the list on the caller's widget, an event of kind 11 given its
 * execTitleId from its execArg (0x811679C2).
 */
int indicatorPushNewEvent(IndicatorPushNewEventParam *param)
{
	int ret = 0;
	LsdbNewEventPtr event(new LsdbNewEvent(*param->event));

	if (event->actType == 11)
		event->execTitleId = indicatorNewEventExecTitleId(event->execArg);

	IndicatorEventItem *item = g_indicatorNewEvent->CreateItem(event, NULL);

	if (item != NULL) {
		IndicatorEventItemLook look = { 0, param->color };

		item->Attach(param->widget, look);
		param->item = item;
	}
	return ret;
}

/* (0x81167C80) */
int indicatorDeleteObject(IndicatorEventItem **item)
{
	if (*item != NULL)
		delete *item;
	return 0;
}

/* (0x81167C94) */
void indicatorNewEventTaskEvent(const IndicatorTaskEvent *event)
{
	g_indicatorNewEvent->Animate(event->callback, true, event->delay);
}

/* the animation copied and started on the main thread (0x81167CB2) */
int indicatorStartInhaleAnim(const IndicatorInhaleAnim *anim)
{
	IndicatorCallArg<IndicatorInhaleAnim> *arg = new IndicatorCallArg<IndicatorInhaleAnim>(*anim);

	thread::CallQueue::main_thread_callqueue.Enqueue("StartInhaleAnim",
							 indicatorNewEventInhaleAnim, arg);
	return 0;
}

/* the shell's notice copied and pushed on the main thread (0x81167D8A) */
int indicatorPrimaryNotificationPush(const IndicatorNoticeRequest *request)
{
	IndicatorCallArg<IndicatorNoticeRequest> *arg =
		new IndicatorCallArg<IndicatorNoticeRequest>(*request);

	thread::CallQueue::main_thread_callqueue.Enqueue("primaryNotificationPush",
							 indicatorNewEventNoticePush, arg);
	return 0;
}

/* the title's notices cancelled on the main thread (0x81167EF2) */
int indicatorPrimaryNotificationCancel(const char *titleId)
{
	thread::CallQueue::main_thread_callqueue.Enqueue("primaryNotificationCancel",
		indicatorNewEventNoticeCancel, new string(titleId));
	return 0;
}

/* the button locked: the first lock closes the notices and takes the focus from the button
   (0x81167F4C) */
void indicatorNewEventLockButton(void)
{
	IndicatorNewEvent *item = g_indicatorNewEvent;

	if (++item->m_lockCount == 1) {
		if (indicatorNoticeManager()->IsOpen())
			indicatorNoticeManager()->Close(0, 0);
		item->m_widget->EnableFocusEvent(false);
		item->m_widget->ReleaseFocus();
	}
}

/* the button unlocked: the last unlock gives it its focus back (0x81167F8C) */
void indicatorNewEventUnlockButton(void)
{
	IndicatorNewEvent *item = g_indicatorNewEvent;

	if (--item->m_lockCount < 0)
		item->m_lockCount = 0;
	else if (item->m_lockCount != 0)
		return;
	item->m_widget->EnableFocusEvent(true);
}

/* (0x81167FB6) */
bool indicatorNewEventUpdateNotices(void)
{
	return indicatorNoticeListCanUpdate();
}
