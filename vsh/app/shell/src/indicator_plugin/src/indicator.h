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
#include <cache/cache_manager.h>
#include <widget/core/widget.h>
#include "indicator_interface.h"

/* the page and the bar's widgets (PLACEHOLDER names) */
#define INDICATOR_PAGE			0xFFD7E3F0
#define INDICATOR_ROOT			0x3A375CEF

/*
 * What the bar shows, given to every item when it changes (144 bytes). The values are named by
 * the interface function that sets each until their meaning is read (PLACEHOLDER names).
 */
struct IndicatorStatus {
	bool visible;
	int value4;		/* the interface's function 4 */
	int bluetooth;		/* function 7: Bluetooth on, from its configuration */
	int flightMode;		/* function 5: /CONFIG/SYSTEM/flight_mode */
	int value8;		/* function 8 */
	int flag1;		/* 1 at construction */
	int flag2;		/* 1 at construction */
	int notification;	/* function 11: /CONFIG/SHELL/notification */
	int value6;		/* function 6: the bar's two counters shown or hidden */
	paf::math::v4 barColor;		/* the theme's bar color */
	paf::math::v4 iconColor;	/* the icons' and the texts' colors, by the mode */
	paf::math::v4 textColor;
	paf::string noNoticeFile;	/* the theme's notice images, or nothing */
	paf::string newNoticeFile;
	paf::math::v4 noticeFontColor;	/* the theme's notice colors */
	paf::math::v4 noticeGlowColor;
};

/* one of the bar's items, on its widget; told every change of the status */
class IndicatorItem {
public:
	IndicatorItem(paf::ui::Widget *widget = NULL) : m_widget(widget)
	{
	}

	virtual ~IndicatorItem()
	{
	}

	virtual void OnStatus(IndicatorStatus status)
	{
	}

	/* its widget colored as the status gives (0x8141BAE0) */
	virtual void UpdateColor(IndicatorStatus status);

	paf::ui::Widget *m_widget;	/* the widget colored, NULL for none */
};

PAF_VECTOR_INLINE_DTOR(IndicatorItem *)

/*
 * The bar's two groups of items, each laid out from the left on its widget (28 bytes; the
 * groups' code is the same, PLACEHOLDER names): the left one's (0x8115FDA0) and the right
 * one's (0x8116D398).
 */
class IndicatorLeftGroup : public IndicatorItem {
public:
	IndicatorLeftGroup(paf::Plugin *plugin, paf::ui::Widget *widget);
	virtual ~IndicatorLeftGroup();					/* (0x81160084) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x8116011A) */

	paf::ui::Widget *m_root;
	paf::vector<IndicatorItem *> m_items;
};

class IndicatorRightGroup : public IndicatorItem {
public:
	IndicatorRightGroup(paf::Plugin *plugin, paf::ui::Widget *widget);
	virtual ~IndicatorRightGroup();
	virtual void OnStatus(IndicatorStatus status);

	paf::ui::Widget *m_root;
	paf::vector<IndicatorItem *> m_items;
};

/* the flight mode's icon, opened from its template in its widget (12 bytes; PLACEHOLDER) */
class IndicatorFlightModeIcon : public IndicatorItem {
public:
	IndicatorFlightModeIcon(paf::Plugin *plugin, paf::ui::Widget *widget);	/* (0x81160468) */
	virtual void OnStatus(IndicatorStatus status);			/* (0x8116058A) */

	paf::ui::Widget *m_root;
};

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

/* the new events' item: its textures dropped (0x81165106) and the theme's asked for again
   (0x81164FA4) (indicator_new_event.cpp; PLACEHOLDER names) */
class IndicatorNewEvent;
extern IndicatorNewEvent *g_indicatorNewEvent;
void indicatorNewEventDropTextures(IndicatorNewEvent *item);
void indicatorNewEventReloadTheme(IndicatorNewEvent *item);

/* the indicator page opened and its object made (0x8115E37A), and both destroyed (0x8115E6B2) */
void indicatorCreate(void);
void indicatorDestroy(void);
void indicatorExitReserved(void);

/* the notices' queue emptied (0x81173A82) and their page closed (0x81180152), when the mode
   changes */
void indicatorNoticeQueueClear(void);
void indicatorNoticePageClose(void);

/* telephony_plugin's call state listened to while the bar lives (0x81168D24, 0x81168D86) */
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
