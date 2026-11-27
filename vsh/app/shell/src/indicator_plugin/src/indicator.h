/*
 * indicator.h: indicator_plugin, the information bar at the top of the screen, as the shell
 * builds it into itself. The plugin is one object (Indicator, 208 bytes) holding the bar's items
 * (each an IndicatorItem on one of the bar's widgets) and the status it gives them whenever it
 * changes. The file's and all names here are ours (PLACEHOLDER).
 */
#ifndef INDICATOR_H
#define INDICATOR_H

#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/list>
#include <std/vector>
#include <common/shared_ptr.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <job/job.h>
#include <common/job_slots.h>
#include <cache/cache_manager.h>
#include <cache/cache_image.h>
#include <widget/core/widget.h>
#include <widget/w_text.h>
#include <widget/w_progress_bar.h>
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

/* the cache's option of an icon, its image scaled to the size given (0: as it is) */
static inline void indicatorIconOption(paf::cache::CacheImage::Option *option, int32_t size)
{
	option->auto_delete = true;
	option->retry = true;
	option->retry_count = -1;
	option->reserved = true;
	option->read_size = 0;
	option->read_offset = 0;
	option->max_size_kb = 0;
	option->open_func = NULL;
	option->open_userdata = NULL;
	memset(&option->sub_rect, 0, sizeof(option->sub_rect));
	option->scale = paf::image::ImageExtent(size, size);
	option->resize_type = paf::image::ImageResizeType_NearestNeighbor;
	option->direct_load = false;
	option->image_order = ImageOrder_NONE;
	option->image_order_param = 0;
	option->check_image = NULL;
	option->check_image_userdata = NULL;
	option->surface_flag = false;
}

/* the indicator plugin's string of the ID (0x81182420; PLACEHOLDER name) */
const wchar_t *indicatorGetString(const paf::IDParam &id);

/* an error posted to the error history (0x8118243A) */
int indicatorPostError(int error);

/* the busy dialog with the message opened (0x81182D06) and closed (0x81182E4C), on the main
   thread when asked; the dialog's ID, below 0 when none opened (PLACEHOLDER names) */
int indicatorBusyDialogOpen(const wchar_t *message, bool sync);
int indicatorBusyDialogClose(bool sync);
struct CommonGuiDialogLayout;

/* told the button of a question dialog closed (PLACEHOLDER names) */
typedef void (*IndicatorDialogCB)(int result, int button, void *arg);
/* a dialog of the layout with the message opened, its ID returned (0x81183012; PLACEHOLDER
   name) */
int indicatorDialogOpen(const CommonGuiDialogLayout *layout, const wchar_t *message,
			IndicatorDialogCB callback, void *arg);
/* the dialog of an error opened, its ID returned (0x8117EF86; PLACEHOLDER name) */
int indicatorErrorDialogOpen(int error, const wchar_t *message, IndicatorDialogCB callback,
			     void *arg);
/* the text of the seconds left, in minutes from 120 on (0x81182050; PLACEHOLDER name) */
paf::wstring indicatorRemainingText(int seconds);
/* the date and time of a "YYYY-MM-DD HH:MM:SS" text as the system shows it, empty for none
   (0x8118221C; PLACEHOLDER name) */
paf::wstring indicatorTimeText(const char *time);
/* the error's text, its title set when empty, and its external code and its number in hex
   appended in parentheses as asked; the external code's result (0x811822A4; PLACEHOLDER
   name) */
int indicatorErrorText(paf::wstring *title, paf::wstring *text, int error, bool code, bool hex);

/* what a main-thread call or a dialog's callback is given: a copy of the caller's value, made
   from a copy of it (PLACEHOLDER name) */
template <typename T>
struct IndicatorCallArg {
	IndicatorCallArg(T value) : value(value)
	{
	}

	T value;
};

/* the system task of the new event's rowid: its progress with its status and percentage
   texts (0x811C3F7C), it removed (0x811C3F8C), and its name (0x811C3F94) (PLACEHOLDER
   names) */
int indicatorTaskGetProgress(SceInt64 id, float *progress, paf::string *status,
			     paf::string *percent);
int indicatorTaskRemove(SceInt64 id);
paf::string indicatorTaskGetName(SceInt64 id);

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
	int m_unusedState;			/* not read */
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
/* a notice of the shell's own, its notifications read from the title it runs (PLACEHOLDER
   name) */
#define INDICATOR_PUSH_NOTICE		1

typedef void (*IndicatorPushNotificationCB)(int result, SceInt64 rowid, void *arg);

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

/* an icon's inhale animation: the callback told when it ends, the icon's texture, else the
   file it is loaded from (28 bytes, PLACEHOLDER names), and the animation's page opened
   (0x8118014A; indicator_inhale_anim.cpp) */
struct IndicatorInhaleAnim {
	int animFlags;
	paf::ui::AnimCB callback;
	float animDuration;
	paf::intrusive_ptr<paf::graph::Surface> texture;
	paf::string iconPath;
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

/* the new-event database the shell keeps open while the bar lives (0x81566914; its methods in
   indicator_new_event_db.cpp, 0x8117F184-0x8117F6A4; PLACEHOLDER names) */
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
	/* the event of the key deleted, the accessor's (0x8117F1AE; PLACEHOLDER name) */
	static int Delete(const LsdbNewEventKey *key);
	/* every event deleted, the accessor's (0x8117F1B8) */
	static int DeleteAll(bool all);
	int GetPopupCount();					/* (0x8117F41C) */
	/* the icon data of the event of the row, the accessor's (0x8117F40C) */
	paf::common::SharedPtr<DbBlob> GetIcon(const SceInt64 *rowid, int *result);
	/* the notifications the event shows, the accessor's (0x8117F69A) */
	unsigned int GetNotifyFlags(const LsdbNewEvent *event);

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
void indicatorNoticeDialogsCreate(void);
void indicatorNoticeDialogsDestroy(void);
/* a notice's dialog kept by its ID until it ends, let go by its ID, or all of them let go
   (indicator_notice_dialogs.cpp: 0x8116D9E0, 0x8116DA00, 0x8116DA18; PLACEHOLDER names) */
void indicatorNoticeDialogAdd(SceInt64 id, int dialogId);
void indicatorNoticeDialogClose(SceInt64 id);
void indicatorNoticeDialogCloseAll(void);
/* the popup lists kept for the notices (indicator_notice_popups.cpp: 0x811739D4, 0x81173A08;
   PLACEHOLDER names) */
void indicatorNoticePopupsCreate(void);
void indicatorNoticePopupsDestroy(void);

/*
 * The items of the notices' list (the event window, Job_EventWindowTaskCommand), each made from
 * a new event by IndicatorNewEvent::CreateItem. The classes and their names are ours
 * (PLACEHOLDER); their functions are in the list's file.
 */

/* what the list gives an item with its widget: the list's mode and the item's color
   (0xRRGGBBAA), by value (PLACEHOLDER names) */
struct IndicatorEventItemLook {
	int mode;
	SceUInt32 color;
};

/* an item: the new event copied, its callback and its serial number, the widget it is given
   with the list's mode and its color (320 bytes; indicator_event_item.cpp, 0x81174D48-0x8117587C) */
class IndicatorEventItem {
public:
	IndicatorEventItem(const LsdbNewEvent &event, IndicatorEventItemCB callback);
	virtual ~IndicatorEventItem();
	/* the item given its widget, the list's mode and its color (0xRRGGBBAA): told of the
	   widget's presses and focus (0x81174FFC; PLACEHOLDER name) */
	virtual int Attach(paf::ui::Widget *widget, IndicatorEventItemLook look);
	/* the item's listener on its widget's scene removed (0x81174ECC; PLACEHOLDER name) */
	virtual int Detach();
	/* what the event runs, by its act type (0x81175238; PLACEHOLDER name) */
	virtual int Execute();
	/* the widget's look: its button white, its new or old back and its frame
	   (0x81175302; PLACEHOLDER name) */
	virtual int SetStyle(paf::ui::Widget *widget);
	/* the widget focused and no longer (PLACEHOLDER names; nothing in the base) */
	virtual void OnFocus()
	{
	}

	virtual void OnUnfocus()
	{
	}

	const LsdbNewEvent *GetEvent();			/* (0x81175490) */
	int GetMode();					/* (0x81175520) */
	unsigned int IsNotified();			/* (0x81175802) */
	/* the text's place on the widget by its alignment (4 to 9), from its first or last line
	   (0x81175526; PLACEHOLDER name) */
	int AlignText(paf::graph::TextObj *text, int align, const paf::math::v4 &size,
		      paf::math::v4 *pos);

	LsdbNewEvent m_event;
	IndicatorEventItemCB m_callback;
	SceInt64 m_serial;				/* the items made before it, plus one */
	int m_mode;					/* the list's mode (PLACEHOLDER name) */
	paf::math::v4 m_color;
	paf::ui::Widget *m_widget;
};

/* an icon's information by its title, as the icon information job gives it (PLACEHOLDER
   names) */
typedef paf::map<paf::string, LsdbIconPtr> IndicatorIconInfoMap;
typedef void (*IndicatorIconInfoCB)(IndicatorIconInfoMap *icons, int result, void *arg);

/* a text scrolled along while its item is focused, on the scene of the item's drawer, ticked by
   the scene's draw function: shown a while, scrolled its overflow at 100 a second, faded out and
   in again at its start, then again or stopped (48 bytes; indicator_text_scroll.cpp,
   0x81180848-0x81180FD8; PLACEHOLDER names) */
class IndicatorTextScroll {
public:
	IndicatorTextScroll(paf::ui::Scene *scene);		/* (0x81180848) */
	virtual ~IndicatorTextScroll();				/* (0x8118087C) */
	/* the text scrolled across, the argument unused (0x81180C84), and back in place
	   (0x81180C24) */
	void Start(int unused, paf::graph::TextObj *text);
	void Stop(paf::graph::TextObj *text);
	/* the text's bounds laid out on one line (0x811808F0) */
	int GetTextBounds(paf::math::v2 &bounds);
	/* the scroll begun in the direction, or ended (0: none), again after it when looping
	   (0x81180A58) */
	void Reset(int direction, bool loop);
	/* a frame's tick (0x81180D32) */
	void Update();

	int m_direction;				/* 0 none, 1 across, 2 down */
	paf::math::v2 m_distance;			/* the text's overflow */
	bool m_scrolling;
	int m_phase;					/* INDICATOR_TEXT_SCROLL_* */
	float m_elapsed;				/* in the phase, in milliseconds */
	float m_duration;				/* of the scroll, in milliseconds */
	paf::graph::TextObj *m_text;			/* the text scrolled */
	bool m_ellipsis;				/* the text's, kept while it scrolls */
	bool m_wordWrap;				/* the text's, kept while it scrolls */
	bool m_loop;					/* scrolled again after the fade */
	paf::ui::Scene *m_scene;
};

/* the icons' information of titles read as a job, the callback told (0x811825A0-; 64
   bytes; Sony's name from its job's) */
class JobGetIconInfo : public paf::job::JobItem {
public:
	JobGetIconInfo(paf::vector<paf::string> *titleIds, IndicatorIconInfoCB callback,
		       void *arg);
	virtual ~JobGetIconInfo();
	virtual int Run();
	virtual void Finish(int result);

	paf::vector<paf::string> m_titleIds;
	IndicatorIconInfoMap m_icons;
	IndicatorIconInfoCB m_callback;			/* NULL once the item is let go */
	void *m_arg;
};

/* the icon item's draw objects: its title, the friend's description, its time and its
   frame */
#define INDICATOR_ICON_OBJ_TITLE	0
#define INDICATOR_ICON_OBJ_SUBTEXT	1
#define INDICATOR_ICON_OBJ_TIME		2
#define INDICATOR_ICON_OBJ_FRAME	3

/* a composite drawer's text or plane object of the ID, NULL when it has another one or none */
#define INDICATOR_ICON_TEXT(widget, id)							\
	((widget)->GetDrawObj(id) != NULL &&						\
	 (widget)->GetDrawObj(id)->IsInherit(paf::graph::TextObj::DrawObjType()) ?	\
	 (paf::graph::TextObj *)(widget)->GetDrawObj(id) : NULL)

#define INDICATOR_ICON_PLANE(widget, id)						\
	((widget)->GetDrawObj(id) != NULL &&						\
	 (widget)->GetDrawObj(id)->IsInherit(paf::graph::PlaneObj::DrawObjType()) ?	\
	 (paf::graph::PlaneObj *)(widget)->GetDrawObj(id) : NULL)

/* the item's drawer on its widget, NULL when it has none */
#define INDICATOR_ICON_DRAWER(widget)							\
	((widget)->FindChild(paf::IDParam("composite_item"), 0) != NULL &&		\
	 (widget)->FindChild(paf::IDParam("composite_item"), 0)				\
		 ->IsInherit(paf::ui::CompositeDrawer::TypeName()) ?			\
	 (widget)->FindChild(paf::IDParam("composite_item"), 0) : NULL)

/* an item with an icon loaded through the cache (420 bytes and its derived classes' fields
   after it; indicator_event_icon.cpp, 0x8117B780-0x8117E0BC) */
class IndicatorEventIconItem : public IndicatorEventItem {
public:
	IndicatorEventIconItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventItem(event, callback), m_icon(NULL)
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
	/* (0x8117BAF8, 0x8117BE28, 0x8117D6E4, 0x8117D902) */
	virtual int Attach(paf::ui::Widget *widget, IndicatorEventItemLook look);
	virtual int Detach();
	virtual void OnFocus();
	virtual void OnUnfocus();
	/* the icon shown (0x8117C0A4) and listened to (0x8117C188) (PLACEHOLDER names) */
	virtual void ShowIcon(paf::ui::Widget *icon);
	virtual void ListenIcon(paf::ui::Widget *icon);
	/* the icon of the texture's ID or the file loaded (0x8117C9AA), its frame
	   (0x8117CCFA), the item's text (0x8117CF64) and its time (0x8117D448), and the
	   widget laid out (0x8117DD0C) (PLACEHOLDER names) */
	virtual int SetIcon(paf::ui::Widget *widget, paf::IDParam id, const char *path);
	virtual int SetIconFrame(paf::ui::Widget *widget);
	virtual int SetText(paf::ui::Widget *widget, paf::IDParam id);
	virtual int SetTime(paf::ui::Widget *widget);
	virtual int Layout(paf::ui::Widget *widget, bool animate);

	/* the icon's texture made (0x8117BE38) and not made (0x8117BF7A), its events 0x100005
	   and 0x100006 (PLACEHOLDER names) */
	static void OnIconReady(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				void *data);
	static void OnIconError(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				void *data);

	/* the icon's listeners, the drawer's and the scroll let go and the icon's job cancelled
	   (0x8117B780; PLACEHOLDER name) */
	int Release();
	/* the icon of the title the event runs asked of the icons' job (0x8117C34A), of the
	   file (0x8117C542) or of the event's own icon data (0x8117C7EE) loaded, and the
	   icons' information read (0x8117DFE2) (PLACEHOLDER names) */
	int LoadIconInfo();
	int LoadIconFile(paf::ui::Widget *icon, const paf::string &path);
	int LoadIconData(paf::ui::Widget *icon, const LsdbNewEvent *event);
	void OnIconInfo(IndicatorIconInfoMap *icons, int result);
	/* the path the cache opens the event's icon data by, "newevtdb:<rowid>" (0x8117C6B4;
	   PLACEHOLDER name) */
	paf::string GetIconDataPath(const LsdbNewEvent *event);

	paf::ui::Widget *m_icon;			/* the AppIconSimple made on the drawer */
	IndicatorTextScroll *m_scroll;			/* the title's, while focused */
	paf::string m_iconPath;				/* the file loaded, until it is shown */
	paf::cache::CacheImage::Option m_iconOption;
	paf::common::SharedPtr<paf::job::JobItem> m_iconJob;	/* the icons' job */
};

/* the friend's item: the icon item with the event's description in a second text object
   under its title (indicator_event_friend.cpp, 0x8117E12C-0x8117ED90; 420 bytes) */
class IndicatorEventFriendItem : public IndicatorEventIconItem {
public:
	IndicatorEventFriendItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventIconItem(event, callback)
	{
	}

	virtual ~IndicatorEventFriendItem()
	{
	}

	/* (0x8117E12C, 0x8117E372, 0x8117E928) */
	virtual int Attach(paf::ui::Widget *widget, IndicatorEventItemLook look);
	virtual int SetText(paf::ui::Widget *widget, paf::IDParam id);
	virtual int Layout(paf::ui::Widget *widget, bool animate);
	/* the description's text object (0x8117E518; PLACEHOLDER name) */
	virtual int SetSubText(paf::ui::Widget *widget, paf::IDParam id);
};

/* the trophy's item, from NPXS10008: a notified one looks like the icon item, the others
   like the friend's with a white title (indicator_event_trophy.cpp, 0x8117ED90-0x8117EF12;
   420 bytes) */
class IndicatorEventTrophyItem : public IndicatorEventFriendItem {
public:
	IndicatorEventTrophyItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventFriendItem(event, callback)
	{
	}

	/* (0x8117ED90, 0x8117EDC6, 0x8117EEEA) */
	virtual int Attach(paf::ui::Widget *widget, IndicatorEventItemLook look);
	virtual int SetText(paf::ui::Widget *widget, paf::IDParam id);
	virtual int Layout(paf::ui::Widget *widget, bool animate);
};

/* the task and transfer items' templates, widgets, textures and strings (their IDs' hashes) */
#define INDICATOR_TASK_MENU_TEMPLATE	0x187D55CB
#define INDICATOR_TASK_MENU_BUTTON	0xE1DE6C06
#define INDICATOR_TASK_BAR_TEMPLATE	0x2FD67BCB
#define INDICATOR_TASK_BAR		0x136D89D3
#define INDICATOR_TASK_BACK_NEW		0xFC9E90F4
#define INDICATOR_TASK_BACK		0x21E6334F
#define INDICATOR_TASK_DONE_BACK_NEW	0xB0B3F4AD
#define INDICATOR_TASK_DONE_BACK	0x0D60DC18
#define INDICATOR_TASK_SUBTEXT		0x0531C070
#define INDICATOR_TASK_MENU_CANCEL	0xADCD9ADF

/* the menu's button pressed */
#define INDICATOR_TASK_ON_PRESSED	0x10000008

/* a typed child found by its ID, NULL when there is none or it is of another type */
#define INDICATOR_TASK_FIND(parent, type, id)						\
	((parent)->FindChild(id, 0) != NULL &&						\
	 (parent)->FindChild(id, 0)->IsInherit(type::TypeName()) ?			\
	 (type *)(parent)->FindChild(id, 0) : NULL)

/* a composite drawer's text object of the ID, NULL when it has another one or none */
#define INDICATOR_TASK_TEXT(widget, id)							\
	((widget)->GetDrawObj(id) != NULL &&						\
	 (widget)->GetDrawObj(id)->IsInherit(paf::graph::TextObj::DrawObjType()) ?	\
	 (paf::graph::TextObj *)(widget)->GetDrawObj(id) : NULL)

/* the item of a system task, msgType 0x00-0xFF: a background download, its progress bar
   updated by a job while it runs, and its menu of commands (indicator_event_task.cpp,
   0x81177C44-0x8117B780; 464 bytes) */
class IndicatorEventTaskItem : public IndicatorEventIconItem {
public:
	IndicatorEventTaskItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventIconItem(event, callback), m_bar(NULL), m_subText(NULL), m_text(NULL),
		  m_busy(false)
	{
	}

	/* (0x81177CAE; 0x8117806E, 0x8117889A, 0x81179668, 0x811788AC, 0x81178990, 0x81178B98,
	   0x8117AEEC) */
	virtual ~IndicatorEventTaskItem();
	virtual int Attach(paf::ui::Widget *widget, IndicatorEventItemLook look);
	virtual int Detach();
	virtual int Execute();
	virtual void ShowIcon(paf::ui::Widget *icon);
	virtual void ListenIcon(paf::ui::Widget *icon);
	virtual int SetText(paf::ui::Widget *widget, paf::IDParam id);
	virtual int Layout(paf::ui::Widget *widget, bool animate);
	/* the item's second line (0x81178DFA), its jobs stopped (0x81177C44), and whether it
	   shows a transfer under way (0x8117AE88) (PLACEHOLDER names) */
	virtual int SetSubText(paf::ui::Widget *widget, paf::IDParam id);
	virtual int Stop();
	virtual bool IsTransferring();

	/* the bar's timer set or the bar hidden (0x81177D08), the menu's button put on the item
	   (0x81177E2C), and the progress of a copy (0x811797F2) or of a download (0x81179B2C)
	   shown (PLACEHOLDER names) */
	void UpdateTimer(paf::ui::Widget *bar);
	int AttachMenu(paf::ui::Widget *item);
	int SetCopyProgress(paf::string name, int number, int total, int remaining, bool show);
	int SetDownloadProgress(SceUInt64 total, SceUInt64 current, int remaining, bool show);

	paf::common::JobSlots m_slots;
	paf::ui::ProgressBar *m_bar;
	paf::graph::TextObj *m_subText;			/* the composite drawer's text 1 */
	paf::graph::TextObj *m_text;			/* the composite drawer's text 0 */
	bool m_busy;					/* its icon is the task's state */
};

/* the item of a transfer, msgType 0x40-0x4F and 0x80-0x8F, with its progress bar updated by a
   job while it runs (indicator_event_transfer.cpp, 0x8117587C-0x81177C44; 496 bytes) */
class IndicatorEventTransferItem : public IndicatorEventTaskItem {
public:
	IndicatorEventTransferItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
		: IndicatorEventTaskItem(event, callback), m_bar(NULL), reserved(0)
	{
	}

	virtual int Attach(paf::ui::Widget *widget, IndicatorEventItemLook look);
	virtual int Execute();
	virtual int SetText(paf::ui::Widget *widget, paf::IDParam id);
	virtual int Layout(paf::ui::Widget *widget, bool animate);
	virtual int SetSubText(paf::ui::Widget *widget, paf::IDParam id);
	virtual int Stop();
	virtual bool IsTransferring();

	/* the bar's timer set while the transfer runs, else the bar hidden (0x8117587C), the
	   menu's button on the item (0x811759A0), its menu opened (0x811767B0) and the progress
	   shown (0x81177226) (PLACEHOLDER names) */
	void UpdateTimer(paf::ui::Widget *bar);
	int AttachMenu(paf::ui::Widget *item);
	void OpenMenu(paf::ui::Widget *anchor);
	int SetProgress(float progress, paf::string status, paf::string percent);

	paf::ui::ProgressBar *m_bar;
	int reserved;					/* zero from the constructor; never read */
	paf::common::JobSlots m_updateSlots;		/* the progress's job (PLACEHOLDER name) */
};

/*
 * The notices' list page (0x81544A54, indicator_notice_manager.cpp): the page made from its
 * template on the bar's page, its list of the new events laid out to fit and slid in and out,
 * its commands run as jobs, one slot each (96 bytes, PLACEHOLDER names).
 */
/* the events kept for the icons' callback (PLACEHOLDER name) */
struct IndicatorNoticeIconArg {
	IndicatorNoticeIconArg(paf::vector<LsdbNewEventPtr> *events) : events(events)
	{
	}

	paf::vector<LsdbNewEventPtr> *events;
};

class IndicatorNoticeManager {
public:
	/* (0x811702D8) */
	IndicatorNoticeManager(paf::Plugin *plugin) : m_plugin(plugin), m_page(NULL), m_state(0)
	{
	}

	virtual ~IndicatorNoticeManager();				/* (0x8117360E) */

	/* open while its state is neither 0 nor 3 (0x8116FBD8) */
	bool IsOpen();
	void Layout(paf::ui::Widget *list, bool focus);		/* (0x8116FBE8) */
	IndicatorEventItem *CreateItem(SceUInt32 index);		/* (0x81170218) */
	void Close(int reason, int option);				/* (0x8117034A) */
	void LoadIconInfo(const paf::vector<LsdbNewEventPtr> &events,
			  IndicatorIconInfoCB callback);		/* (0x81170DC0) */
	paf::ui::Widget *MakePage(paf::ui::Widget *parent);		/* (0x81171074) */
	void Open(paf::ui::Widget *page, const paf::vector<LsdbNewEventPtr> &events);
									/* (0x811719BC) */
	void Update(const paf::vector<LsdbNewEventPtr> &events);	/* (0x81171DE2) */
	/* the list and its background shown once the frame is grown (0x81171EFE) */
	void OnFrameResized(paf::ui::Widget *frame, void *data);
	/* the menu of the corner button opened at the widget (0x811724C0) */
	void OpenMenu(paf::ui::Widget *anchor);
	/* the menu's item chosen, by its index (0x811728F6) */
	void OnMenuClosed(int index);
	/* the list's cells made again for the events (0x81172E5E) */
	void Refresh();
	/* the shown events' empty titles and icons filled from the icons read (0x811730A2) */
	void ApplyIcons(IndicatorIconInfoMap *icons);
	/* the events taken, the ones before kept (0x81173132) */
	int SetEvents(const paf::vector<LsdbNewEventPtr> &events, int option);
	/* the icons read: the events kept for them taken and their icons filled (0x811731C8) */
	void OnIconInfo(IndicatorIconInfoMap *icons, int result, IndicatorNoticeIconArg *arg);
	/* the icons read for an open page: its cells changed to the new events (0x81173278) */
	void Sync(IndicatorIconInfoMap *icons, int result, IndicatorNoticeIconArg *arg);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_page;
	/* 0 closed, 1 opening, 2 open, 3 closing */
	int m_state;
	paf::vector<LsdbNewEventPtr> m_events;
	/* the events before the last ones taken */
	paf::vector<LsdbNewEventPtr> m_shownEvents;
	paf::common::JobSlots m_jobs;
	paf::common::SharedPtr<paf::job::JobItem> m_iconJob;
	/* the menu's items' strings */
	paf::vector<paf::IDParam> m_ids;
};

IndicatorNoticeManager *indicatorNoticeManager(void);		/* (0x8116FAAA) */

/*
 * The notices' cache (0x81544A50, indicator_notice_cache.cpp): the notices pushed wait in their
 * order to pop up on the notification page, one at a time, each told how it ended; added and
 * cancelled on the main thread (36 bytes, PLACEHOLDER names).
 */
class IndicatorNotice;


/* the icons' information of the titles read on the indicator's job queue, the callback told
   (0x81182604; PLACEHOLDER name, its file not written yet) */
paf::common::SharedPtr<paf::job::JobItem>
indicatorIconInfoJobStart(paf::vector<paf::string> *titleIds, IndicatorIconInfoCB callback,
			  void *arg);

/* what the bar says of a new event (indicator_event_text.cpp, 0x81173AD0-0x81174D48;
   PLACEHOLDER names): its message's format and text, a system application's notice text, the
   tests of its kind and its download's state, and whether it is told with its sound and icon
   (not the background or log status, of kind 4 or 6) */
bool indicatorEventIsBgcpStatus(const LsdbNewEvent *event);
const wchar_t *indicatorEventMessageFormat(const LsdbNewEvent *event);
paf::wstring indicatorEventMessage(const LsdbNewEvent *event);
paf::wstring indicatorEventText(const LsdbNewEvent *event);
paf::wstring indicatorEventAppText(const paf::string &titleId);
unsigned int indicatorEventIsNotified(const LsdbNewEvent *event);
bool indicatorEventIsStateFive(const LsdbNewEvent *event);
bool indicatorEventRunsPcsi00011(const LsdbNewEvent *event);
bool indicatorEventIsStateSix(const LsdbNewEvent *event);
bool indicatorEventRunsPcsc80018(const LsdbNewEvent *event);
int indicatorEventBgcpState(const LsdbNewEvent *event);
bool indicatorNoticeIsShown(const LsdbNewEvent *event);

class IndicatorNoticeCache {
public:
	IndicatorNoticeCache(paf::Plugin *plugin)
		: m_plugin(plugin), m_page(NULL), m_current(NULL), m_popup(NULL), m_reserved(NULL)
	{
		m_queue.reserve(16);
	}

	void ShowNext();
	void Dismiss(bool animate);
	void Push(IndicatorNotice *notice);
	void OnTimeout();
	void Finish(int result);
	void CancelNow(paf::string *titleId);
	int Add(const IndicatorPushNotificationParam &param);
	int Cancel(const char *titleId);			/* (0x8116EA38) */
	int CancelAll();
	void AttachItem();
	void OnIconInfo(IndicatorIconInfoMap *icons, int result, void *arg);
	void LoadIconInfo();
	void GetBounds(paf::ui::Widget *widget, paf::math::v4 *size, paf::math::v4 *center);
	void Open();

	paf::Plugin *m_plugin;
	paf::ui::Scene *m_page;			/* page_event_notification */
	paf::vector<IndicatorNotice *> m_queue;	/* by their option, the lower first */
	IndicatorNotice *m_current;		/* the one shown */
	paf::ui::Widget *m_popup;		/* the notice's widget on the page */
	void *m_reserved;			/* PLACEHOLDER: only cleared */
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

extern "C" int vshLedSetMode(int led, int mode, const ShellLedParam *param);

/* the indicator page opened and its object made (0x8115E37A), and both destroyed (0x8115E6B2) */
void indicatorCreate(void);
void indicatorDestroy(void);
void indicatorExitReserved(void);

/* the notices' popup lists closed (0x81173A82) and their page closed (0x81180152), when the mode
   changes */
void indicatorNoticePopupCloseAll(void);
/* the popup list of the ID kept, forgotten as it closes (0x81173A52), and closed by its ID
   (0x81173A6A) (PLACEHOLDER names) */
void indicatorNoticePopupAdd(SceInt64 id);
void indicatorNoticePopupClose(SceInt64 id);
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
