/*
 * indicator.h: indicator_plugin, the information bar at the top of the screen, as the shell
 * builds it into itself. The plugin is one object (Indicator, 208 bytes) holding the bar's items
 * (each an IndicatorItem on one of the bar's widgets) and the status it gives them whenever it
 * changes. The file's and all names here are ours (PLACEHOLDER).
 */
#ifndef INDICATOR_H
#define INDICATOR_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/list>
#include <std/vector>
#include <common/shared_ptr.h>
#include <math/ngp/v4.h>
#include <job/job.h>
#include <common/job_slots.h>
#include <cache/cache_manager.h>
#include <cache/cache_image.h>
#include <widget/core/widget.h>
#include <widget/w_text.h>
#include "shell.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "indicator_interface.h"
#include "indicator_item.h"

/* a registry value the bar reads: asked for by its path and name, given back with the result of
   reading it (40-byte list node; PLACEHOLDER names) */
struct IndicatorRegKey {
	IndicatorRegKey(const paf::string &path, const paf::string &name)
		: path(path), name(name), value(-1)
	{
	}

	paf::string path;
	paf::string name;
	int value;
	int result;		/* 0 when read */
};

/* the keys' list destroyed by its one out-of-line copy (0x8141B7C8), never inline */
namespace paf {
template<> __attribute__((noinline)) inline list<IndicatorRegKey, allocator<IndicatorRegKey> >::~list()
{
	PAF_LIST_DESTROY_ALL
}
}

/* the registry values read, and the Bluetooth configuration read: given to the bar (PLACEHOLDER
   names) */
typedef void (*IndicatorRegInfoCallback)(const paf::list<IndicatorRegKey> &keys, void *data);
typedef void (*IndicatorBtConfigCallback)(int result, int config, void *data);

class Indicator {
public:
	Indicator(paf::ui::Widget *page);				/* (0x8115D882) */
	~Indicator();							/* (0x8115E45E) */

	/* the bar shown in a mode, slid in or at once (0x8115E6CE) */
	int Show(IndicatorMode mode, bool animate);
	bool IsVisible();						/* (0x8115F09C) */

	void SetStatus(IndicatorStatus status);				/* (0x8115B59C) */
	int Hide(bool animate);						/* (0x8115B750) */
	/* the bar laid out for the mode, the items told (0x8115D050) */
	void SetMode(IndicatorMode mode, bool animate);

	/* the parts of SetMode */
	void SetIconColors(paf::ui::Widget *root, IndicatorMode mode);	/* (0x8115C076) */
	void SetBarTexture(paf::ui::Widget *bar, IndicatorMode mode);	/* (0x8115C236) */
	void UpdateCounters(paf::ui::Widget *root, int on);		/* (0x8115C3E8) */
	void UpdateFlightModeIcon(paf::ui::Widget *root, int on, bool animate); /* (0x8115C906) */

	/* the registry values read (0x8115F1D4), and the Bluetooth configuration (0x8115F4A8) */
	void OnRegInfo(const paf::list<IndicatorRegKey> &keys, void *data);
	void OnBtConfig(int result, int config, void *data);

	paf::vector<IndicatorItem *> m_items;
	IndicatorMode m_mode;
	IndicatorStatus m_status;
	paf::common::SharedPtr<paf::job::JobItem> m_regInfoJob;	/* reading the registry values */
	paf::common::SharedPtr<paf::job::JobItem> m_btConfigJob;
	int m_reserved;		/* -1 at construction; not read */
	int m_flightModeIconOn;	/* what UpdateFlightModeIcon last did, -1 for nothing */
	int m_countersOn;	/* what UpdateCounters last did */
};

/* the plugin's object, NULL until it starts */
extern Indicator *g_indicator;

/* the plugin, its private heap and its interface table, and its cache manager
   (indicator_interface.cpp) */
class IndicatorCache;
extern IndicatorCache *g_indicatorCache;
extern paf::memory::HeapAllocator *g_indicatorHeap;
extern IndicatorPluginInterface *g_indicatorInterface;
extern paf::Plugin *g_indicatorPlugin;

/* the registry values read on the plugin's job queue, the callback given them (0x8115FBC6) */
paf::common::SharedPtr<paf::job::JobItem> indicatorRegInfoJobStart(
	const paf::list<IndicatorRegKey> &keys, IndicatorRegInfoCallback callback, void *data);

/* the Bluetooth configuration read, "JobGetBtConfig" (0x8115F9F8) */
paf::common::SharedPtr<paf::job::JobItem> indicatorBtConfigJobStart(IndicatorBtConfigCallback callback,
								     void *data);

/* a widget of a page of the plugin, NULL without the page or the widget (0x8115F9D0) */
paf::ui::Widget *indicatorFindWidget(const paf::IDParam &page, const paf::IDParam &id);

/* the Dolce's own look: no theme and the mode's type 0 (0x8115BF9A) */
bool indicatorIsDolceDefault(void);

/* the plugin's interface given to the shell (0x8115F9A8), and its resources taken (0x8115F98C) */
void indicatorInterfaceRegister(paf::Plugin *plugin);
void indicatorInterfaceInit(void);
void indicatorInterfaceExit(void);

/* the plugin's cache manager, "indicator_plugin_CacheManager" (0x8115F8DA, 0x8115F8F0), and room
   made in its surface pool (0x8115F976) */
void indicatorCacheCreate(paf::Plugin *plugin);
void indicatorCacheDestroy(void);
void indicatorCacheMakeSpace(void);

/* an image of a file (0x8115F924) or of a plugin's texture (0x8115F94E) made in the cache */
int indicatorCacheCreateFile(const paf::string &path,
			     paf::intrusive_ptr<paf::cache::CacheImageInterface> &image,
			     const paf::cache::CacheImage::Option *option, paf::job::JobQueue *queue);
int indicatorCacheCreateTexture(paf::Plugin *plugin, paf::IDParam &id,
				paf::intrusive_ptr<paf::cache::CacheImageInterface> &image,
				const paf::cache::CacheImage::Option *option,
				paf::job::JobQueue *queue);

/* an error posted to the error history (0x8118243A) */
int indicatorPostError(int error);

/*
 * The new events' item: the button on the bar that opens the notices, with the count of the
 * new events and the theme's notice images, its commands run as jobs on its own queue, one slot
 * each (indicator_new_event.cpp; 140 bytes, PLACEHOLDER names).
 */
class IndicatorEventItem;

/* told what came of an item's action, with the list's close argument (PLACEHOLDER names) */
typedef void (*IndicatorEventItemCB)(int result, int arg);

class IndicatorNewEvent : public IndicatorItem {
public:
	IndicatorNewEvent(paf::Plugin *plugin, paf::ui::Widget *root);	/* (0x81162FD0) */
	virtual ~IndicatorNewEvent();					/* (0x811636B0) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x811640C6) */

	paf::intrusive_ptr<paf::cache::CacheImageInterface> GetNoNoticeImage();	/* (0x81163992) */
	paf::intrusive_ptr<paf::cache::CacheImageInterface> GetNewNoticeImage()
	{
		return m_newNoticeImage;
	}

	void SetCount(int count, bool update);				/* (0x811639A6) */
	void UpdateTheme(const IndicatorStatus &status);		/* (0x81163CD2) */
	void OpenNotices();						/* (0x8116419E) */
	int ShowBusy();							/* (0x811642EC) */
	int HideBusy();							/* (0x811644C6) */
	void StartBlink();						/* (0x81164664) */
	int Animate(paf::ui::AnimCB cb, bool pop, float delay);	/* (0x8116472A) */
	int StopScale();						/* (0x811649D0) */
	IndicatorEventItem *CreateItem(LsdbNewEventPtr event, IndicatorEventItemCB callback);
									/* (0x81164B7A) */
	int UpdateBusy(paf::vector<LsdbNewEventPtr> &events);	/* (0x811651B2) */
	int Burst();							/* (0x81165224) */
	int Restore();							/* (0x811657C8) */
	void ShowDimmer();						/* (0x81165C62) */
	void HideDimmer();						/* (0x81165E2A) */

	paf::ui::Widget *m_root;
	paf::ui::Widget *m_badge;		/* the count's back */
	bool m_enabled;
	paf::common::SharedPtr<paf::job::JobItem> m_noticeJob;
	paf::common::SharedPtr<paf::job::JobItem> m_imageJob;
	paf::Plugin *m_plugin;
	int m_count;				/* the new events counted */
	paf::vector<LsdbNewEventPtr> m_events;
	ShellAppActivatedEvent m_activeApp;	/* the application brought to the front last */
	paf::common::JobSlots m_slots;		/* the commands' jobs, by command */
	paf::job::JobQueue *m_queue;		/* SceNewIndicatorJobQueue */
	int m_lockCount;			/* the button locked, by how many */
	int m_reserved108;			/* PLACEHOLDER: not read yet */
	paf::math::v4 m_iconScale;		/* (0.75, 0.75, 1) without new events */
	/* the theme's images shown with new events and without: from the status's first
	   notice file and its second (so the status's names for them look swapped) */
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_newNoticeImage;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_noNoticeImage;
	bool m_themeLoaded;
};

extern IndicatorNewEvent *g_indicatorNewEvent;	/* (0x815449FC) */

/* what a notice pushed to the notification job carries (36 bytes, PLACEHOLDER names), the
   callback told its result with the event's rowid, and the job started on the indicator's
   queue (0x81182A92) */
typedef void (*IndicatorPushNotificationCB)(int result, int reserved, SceInt64 rowid, void *arg);

struct IndicatorPushNotificationParam {
	IndicatorPushNotificationParam(int target, LsdbNewEventPtr event, const char *titleId,
				       int option, IndicatorPushNotificationCB callback, void *arg,
				       int flags)
		: target(target), titleId(titleId), option(option), result(0), event(event),
		  callback(callback), arg(arg), flags(flags)
	{
	}

	int target;
	const char *titleId;
	int option;
	int result;
	LsdbNewEventPtr event;
	IndicatorPushNotificationCB callback;
	void *arg;
	int flags;
};

paf::common::SharedPtr<paf::job::JobItem>
indicatorPushNotificationStart(int type, const IndicatorPushNotificationParam &param);

/* a notice the shell asks the bar to show (60 bytes, PLACEHOLDER names) */
struct IndicatorNoticeRequest {
	paf::string titleId;
	paf::string execTitleId;
	paf::string iconPath;
	paf::string desc;
	int seId;
	float seVolume;
	int flags;
};

/* an icon's inhale animation: the callback told when it ends, the icon's texture and its
   title (28 bytes, PLACEHOLDER names), and the animation's page opened (0x8118014A) */
struct IndicatorInhaleAnim {
	int reserved0;
	paf::ui::AnimCB callback;
	float reserved8;
	paf::intrusive_ptr<paf::graph::Surface> texture;
	paf::string titleId;
};

int indicatorInhaleAnimStart(const IndicatorInhaleAnim *anim);

/* a new event made an item of the list on the caller's widget, the item given back
   (PLACEHOLDER names) */
struct IndicatorPushNewEventParam {
	const LsdbNewEvent *event;
	paf::ui::Widget *widget;
	IndicatorEventItem *item;
	SceUInt32 color;
};

void indicatorPushNotification(int type, LsdbNewEventPtr event, int option, int target,
			       IndicatorPushNotificationCB callback, void *arg, int flags);
void indicatorPushNotice(const IndicatorNoticeRequest *request, int target,
			 IndicatorPushNotificationCB callback, void *arg);	/* (0x81162CB0) */

/* the new-event database the shell keeps open while the bar lives (0x81566914; its
   constructor, destructor, Open and Close at 0x8117F184-0x8117F1AC; PLACEHOLDER names) */
class IndicatorNewEventDb {
public:
	IndicatorNewEventDb();
	~IndicatorNewEventDb();
	int Open();
	int Close();
	/* the accessor's, an event of kind 11's execTitleId taken from its execArg (0x8117F36E,
	   0x8117F424) */
	int GetEvents(paf::vector<LsdbNewEventPtr> *events, int max, int *count, int *newCount,
		      bool clear);
	int GetPopup(LsdbNewEvent *event);
	int ClearPopup(const SceInt64 *rowid);			/* (0x8117F47E) */
	int ClearPopups();					/* (0x8117F486) */
	int UpdateNewFlag(const LsdbNewEvent *event);		/* (0x8117F48E) */
	int ClearNewFlags();					/* (0x8117F692) */
	bool IsChanged();					/* (0x8117F414) */
	int GetPopupCount();					/* (0x8117F41C) */

	NewEventAccessor m_accessor;
};

IndicatorNewEventDb *indicatorNewEventDb(void);			/* (0x81162F74) */

/* the title an event of kind 11 runs, from its execArg: NPXS10003 for a web page, NPXS10002
   for a psns: link, the titleid= of a psgm: link, else empty (0x8117F1C2; PLACEHOLDER name) */
paf::string indicatorNewEventExecTitleId(const paf::string &execArg);

/* the notice list made and not busy (0x81183F48; PLACEHOLDER name) */
bool indicatorNoticeListCanUpdate(void);

/* the bar's notice parts made with the item (0x811702D8, 0x8116EAF0, 0x8116D988, 0x811739D4)
   and destroyed with it (0x81170330, 0x8116EB36, 0x8116D9A8, 0x81173A08) (PLACEHOLDER names) */
void indicatorNoticeManagerCreate(paf::Plugin *plugin);
void indicatorNoticeManagerDestroy(void);
void indicatorNoticeCacheCreate(paf::Plugin *plugin);
void indicatorNoticeCacheDestroy(void);
void indicatorNoticeStateCreate(void);
void indicatorNoticeStateDestroy(void);
void indicatorNoticeListCreate(void);
void indicatorNoticeListDestroy(void);

/*
 * The items of the notices' list (the event window, Job_EventWindowTaskCommand), each made from
 * a new event by IndicatorNewEvent::CreateItem. The classes and their names are ours
 * (PLACEHOLDER); their functions are in the list's file.
 */

/* an item: the new event copied, its callback and its serial number (308 bytes; its
   constructor at 0x81174D48, the base's at 0x81174EC4) */
class IndicatorEventItem {
public:
	IndicatorEventItem(const LsdbNewEvent &event, IndicatorEventItemCB callback);
	virtual ~IndicatorEventItem();
	/* the item given its widget, the list's argument and its color (0xRRGGBBAA)
	   (0x81174FFC; PLACEHOLDER name) */
	virtual int Attach(paf::ui::Widget *widget, int arg, SceUInt32 color);

	LsdbNewEvent m_event;
	IndicatorEventItemCB m_callback;
	SceInt64 m_serial;				/* the items made before it, plus one */
	int m_status;					/* PLACEHOLDER: not read yet */
	SceInt64 m_startTime;				/* PLACEHOLDER: not read yet */
	SceInt64 m_endTime;				/* PLACEHOLDER: not read yet */
	int m_flags;					/* PLACEHOLDER: not read yet */
};

/* an item with an icon loaded through the cache (420 bytes and its derived classes' fields
   after it) */
class IndicatorEventIconItem : public IndicatorEventItem {
public:
	IndicatorEventIconItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventItem(event, callback)
	{
		m_iconOption.auto_delete = true;
		m_iconOption.retry = true;
		m_iconOption.retry_count = -1;
		m_iconOption.reserved = true;
		m_iconOption.read_size = 0;
		m_iconOption.read_offset = 0;
		m_iconOption.max_size_kb = 0;
		m_iconOption.open_func = NULL;
		m_iconOption.open_userdata = NULL;
		m_iconOption.resize_type = paf::image::ImageResizeType_NearestNeighbor;
		m_iconOption.direct_load = false;
		m_iconOption.image_order = ImageOrder_NONE;
		m_iconOption.image_order_param = 0;
		m_iconOption.check_image = NULL;
		m_iconOption.check_image_userdata = NULL;
		m_iconOption.surface_flag = false;
	}

	virtual ~IndicatorEventIconItem();

	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_icon;
	int m_iconType;					/* PLACEHOLDER: not set yet */
	paf::string m_iconPath;
	paf::cache::CacheImage::Option m_iconOption;
	paf::common::SharedPtr<paf::job::JobItem> m_iconJob;
};

/* the item of a message from an application or a user (424 bytes) */
class IndicatorEventMessageItem : public IndicatorEventIconItem {
public:
	IndicatorEventMessageItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventIconItem(event, callback)
	{
	}

	virtual ~IndicatorEventMessageItem();
	virtual int Attach(paf::ui::Widget *widget, int arg, SceUInt32 color);
};

/* a message item with its own application (PLACEHOLDER) */
class IndicatorEventAppItem : public IndicatorEventMessageItem {
public:
	IndicatorEventAppItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventMessageItem(event, callback)
	{
	}

	virtual int Launch();				/* PLACEHOLDER name */
};

/* the item of a friend's event, msgType 0xA00-0xAFF (424 bytes) */
class IndicatorEventFriendItem : public IndicatorEventAppItem {
public:
	IndicatorEventFriendItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventAppItem(event, callback)
	{
	}

	virtual int Attach(paf::ui::Widget *widget, int arg, SceUInt32 color);
};

/* the item of a trophy, from NPXS10008 (424 bytes) */
class IndicatorEventTrophyItem : public IndicatorEventAppItem {
public:
	IndicatorEventTrophyItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventAppItem(event, callback)
	{
	}

	virtual int Attach(paf::ui::Widget *widget, int arg, SceUInt32 color);
};

/* the item of a system task, msgType 0x00-0xFF, its jobs in slots (464 bytes) */
class IndicatorEventTaskItem : public IndicatorEventIconItem {
public:
	IndicatorEventTaskItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventIconItem(event, callback), m_progress(0), m_busy(false)
	{
	}

	virtual ~IndicatorEventTaskItem();
	virtual int Attach(paf::ui::Widget *widget, int arg, SceUInt32 color);

	paf::common::JobSlots m_slots;
	paf::common::SharedPtr<paf::job::JobItem> m_taskJob;
	int m_progress;					/* PLACEHOLDER: not read yet */
	bool m_busy;					/* PLACEHOLDER: not read yet */
};

/* the item of a transfer, msgType 0x40-0x4F and 0x80-0x8F, with its progress
   (Job_TaskControl, Job_UpdateProgress; 496 bytes) */
class IndicatorEventTransferItem : public IndicatorEventTaskItem {
public:
	IndicatorEventTransferItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventTaskItem(event, callback)
	{
	}

	virtual int Attach(paf::ui::Widget *widget, int arg, SceUInt32 color);

	paf::common::SharedPtr<paf::job::JobItem> m_progressJob;
	paf::common::JobSlots m_transferSlots;
};

/* the notices' list page (0x81544A54): open while its state is neither 0 nor 3 (0x8116FBD8),
   closed (0x8117034A), and opened on a page with the new events (0x811719BC) (PLACEHOLDER
   names) */
class IndicatorNoticeManager {
public:
	bool IsOpen();
	void Close(int reason, int option);
	void Open(paf::ui::Widget *page, const paf::vector<LsdbNewEventPtr> &events);
	void Update(const paf::vector<LsdbNewEventPtr> &events);	/* (0x81171DE2) */
};

IndicatorNoticeManager *indicatorNoticeManager(void);		/* (0x8116FAAA) */

/* the notices' cache (0x81544A50): the notices shown cancelled on the main thread
   (0x8116EA8C) (PLACEHOLDER names) */
class IndicatorNoticeCache {
public:
	int CancelAll();
	int Cancel(const char *titleId);			/* (0x8116EA38) */
};

IndicatorNoticeCache *indicatorNoticeCache(void);		/* (0x8116EAE2) */

void indicatorNewEventDropTextures(IndicatorNewEvent *item);
void indicatorNewEventClearNewFlags(IndicatorNewEvent *item);	/* (0x81164A18) */
void indicatorNewEventReloadTheme(IndicatorNewEvent *item);	/* (0x81164FA4) */
void indicatorNewEventBlinkLed(void);				/* (0x8116516E) */

/* the LED's blink (0x81425E58; PLACEHOLDER names, its file not written yet) */
struct ShellLedParam {
	int pattern;
	int duration;
};

int shellLedSetMode(int led, int mode, const ShellLedParam *param);

/* the indicator page opened and its object made (0x8115E37A), and both destroyed (0x8115E6B2) */
void indicatorCreate(void);
void indicatorDestroy(void);
void indicatorExitReserved(void);

/* the notices' queue emptied (0x81173A82) and their page closed (0x81180152), when the mode
   changes */
void indicatorNoticeQueueClear(void);
void indicatorNoticePageClose(void);

/* telephony_plugin's general state listened to while the bar lives (0x81168D24, 0x81168D86;
   indicator_telephony.cpp) */
void indicatorTelephonyListen(void);
void indicatorTelephonyUnlisten(void);

/* the bar's interface functions (0x8115EABC-0x8115F0D8) */
int indicatorShow(IndicatorMode mode, bool animate);
int indicatorHide(bool animate);
int indicatorGetModeType(int *type);
bool indicatorIsVisible(void);
int indicatorSetValue4(int value);
int indicatorSetValue8(int value);
int indicatorSetFlightMode(int on);
int indicatorSetValue6(int value);
int indicatorSetBluetooth(int on);
int indicatorUnsupported(void);
int indicatorSetNotification(int on);
int indicatorGetNotification(int *on);
void indicatorUpdateTheme(void);
bool indicatorIsShownNormal(void);

#endif
