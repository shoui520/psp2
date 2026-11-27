/*
 * livearea_internal.h: shared internal interfaces, structs, and cross-unit declarations
 * for SceShell's livearea_plugin (0x81199B68 - 0x811B7538).
 */
#ifndef LIVEAREA_INTERNAL_H
#define LIVEAREA_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <std/map>
#include <std/string>
#include <std/vector>
#include <plugin.h>
#include <framework.h>
#include <thread/thread.h>
#include <job/job.h>
#include <common/ref_obj.h>
#include <common/shared_ptr.h>
#include <common/job_slots.h>
#include <cache/cache_manager.h>
#include <cache/cache_image.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <widget/w_list_view.h>
#include <widget/w_file_list.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <activity_db/activity_db.h>
#include <activity_db/activity_db_internal.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../livespace_db/livearea.h"
#include "../../livespace_db/livespace_db_internal.h"
#include "../../../plugins.h"
#include "livearea_interface.h"

struct LiveAreaMgr;
struct LiveAreaPluginInterface;
struct LiveAreaPageInitInfo;
struct LiveAreaReloadRequest;
class LiveAreaCacheMgr;
class LiveAreaPage;
class LiveAreaPageInner;
class LiveAreaFeed;
class LiveAreaMoreCtrl;
class LiveAreaStatusDlg;
class LiveAreaFeedItem;
class LiveAreaFeedVisualizer;
class LiveAreaSubItem;
class LiveAreaSubVisualizer;
class LiveAreaUpdatorImpl;
class LiveAreaTaskMgr;
class LiveAreaUpdateTaskBase;
struct LiveAreaUpdaterImpl;
struct LiveAreaUpdater;

/* ------------------------------------------------------------------ */
/* Cross-plugin interface tables                                      */
/* ------------------------------------------------------------------ */

struct LiveAreaTopmenuInterface {
	void *leadingEntries[4];
	paf::ui::Widget *(*getPageWidget)(int pageKey);				/* +0x10 */
	paf::ui::Widget *(*getPageRootWidget)(int pageKey);			/* +0x14 */
	paf::cache::CacheManager *(*getPageCacheManager)(int pageKey);		/* +0x18 */
	paf::graph::SurfacePool *(*getPageSurfacePool)(int pageKey);		/* +0x1C */
	void *pageListEntry;							/* +0x20 */
	int (*getCurrentPageKey)(void);						/* +0x24 */
	int (*getPageIndex)(int pageKey);					/* +0x28 */
	int (*getPageKeyAt)(int index);						/* +0x2C */
	int (*findPageKeyByTitleId)(const char *titleId);			/* +0x30 */
	paf::job::JobQueue *(*getJobQueue)(void);				/* +0x34 */
	void (*onManualIconClick)(int pageKey);					/* +0x38 */
	void (*onPatchIconClick)(int pageKey);					/* +0x3C */
	void (*onImportSavedataIconClick)(int pageKey);				/* +0x40 */
	void (*onOnlineStorageIconClick)(int pageKey);				/* +0x44 */
	void (*addPageEventListener)(int pageKey, void *callback, void *userdata);	/* +0x48 */
	void (*removePageEventListener)(int pageKey, void *callback, void *userdata);	/* +0x4C */
	void *listenerGapEntries[4];
	void (*onContentReset)(int pageKey);					/* +0x60 */
	void (*addPageShowListener)(int pageKey, void *callback, void *userdata);	/* +0x64 */
	void (*addPageHideListener)(int pageKey, void *callback, void *userdata);	/* +0x68 */
	void *holdGapEntries[4];
	void (*beginGameHold)(void);						/* +0x7C */
	void (*endGameHold)(void);						/* +0x80 */
	void (*beginHold)(void);						/* +0x84 */
	void (*endHold)(void);							/* +0x88 */
	void (*beginBusy)(void);						/* +0x8C */
	void (*endBusy)(void);							/* +0x90 */
	void *gateGapEntries[2];
	void (*onGateClick)(int pageKey);					/* +0x9C */
	int (*isPageInteractive)(int pageKey);					/* +0xA0 */
	void *scrollGapEntries[2];
	void (*setPageScrolledDown)(int pageKey, int scrolledDown);		/* +0xAC */
	void (*notifyIntroComplete)(int pageKey);				/* +0xB0 */
	void (*notifyContentUpdated)(const char **args);			/* +0xB4 */
	void *musicGapEntry;
	void (*setPageMusicPlaying)(int pageKey, int playing);			/* +0xBC */
	const wchar_t *(*getPageTitle)(int pageKey);				/* +0xC0 */
	int (*isTopmenuForeground)(void);					/* +0xC4 */
	void *planeGapEntry;
	paf::ui::Widget *(*createPlane)(int layer);				/* +0xCC */
	void *scrollLockGapEntries[2];
	void (*setScrollEnabled)(int enabled);					/* +0xD8 */
	void *homeGapEntries[3];
	int (*isHomePageSelected)(void);					/* +0xE8 */
	void *getRunningTitles;							/* +0xEC */
	void *setApplauncherInterface;						/* +0xF0 */
	void (*setLiveAreaInterface)(LiveAreaPluginInterface *iface);		/* +0xF4 */
	void *statusGapEntries[4];
	void *notifyUpStatus;							/* +0x108 */
	int (*openErrorDialog)(CommonGuiMessageDialogParam *param);		/* +0x10C */
	void *onApplauncherInitDone;						/* +0x110 */
	int (*getAppInfo)(LsdbAppInfoPtr *outInfo, const char *titleId);	/* +0x114 */
	void *openPagesGapEntry;
	void (*getOpenPageTitleIds)(paf::vector<paf::string> *outTitles);	/* +0x11C */
	void (*registerDialogId)(int dialogId);					/* +0x120 */
	void *netStateGapEntries[11];
	int (*getNetworkState)(void);						/* +0x150 */
};

struct LiveAreaGameInterface {
	void *leadingEntry;
	int (*getStorageState)(LsdbAppInfoPtr *appInfo, int *outNeedWait);	/* +0x04 */
	void *middleGapEntries[8];
	void (*setDialogHold)(int hold);					/* +0x28 */
	void *trailingGapEntries[40];
	int (*prepareLiveAreaInstall)(LsdbAppInfoPtr *appInfo, int mode);	/* +0xCC */
	int (*removeLiveAreaContent)(paf::string *titleId, int flags);		/* +0xD0 */
};

struct LiveAreaApplauncherInterface {
	void *leadingEntries[8];
	void (*endIconEffects)(void);						/* +0x20 */
};

struct LiveAreaImposeInterface {
	void *leadingEntries[9];
	int (*isPsMenuOpen)(void);						/* +0x24 */
	void (*setMusicBannerActive)(int active);				/* +0x28 */
};

struct LiveAreaTelephonyModemVtable {
	void *leadingEntries[13];
	int (*getCarrierAppTitleId)(void *self, char **outTitleId);		/* +0x34 */
};

struct LiveAreaTelephonyModem {
	const LiveAreaTelephonyModemVtable *vtable;
};

struct LiveAreaTelephonyInterface {
	void *leadingEntries[2];
	LiveAreaTelephonyModem *(*getModem)(void);				/* +0x08 */
};

/* ------------------------------------------------------------------ */
/* Shared plugin globals                                              */
/* ------------------------------------------------------------------ */

extern LiveAreaPluginInterface *g_liveareaInterface;			/* 0x81566F90 */
extern paf::Plugin *g_liveareaPlugin;					/* 0x81566F94 */
extern LiveAreaCacheMgr *g_liveareaCache;				/* 0x81567080 */
extern LiveAreaMgr *g_liveareaMgr;					/* 0x815670A8 */
extern paf::thread::RMutex g_shellMutex;				/* 0x81467AAC */
extern paf::job::JobQueue *g_activityJobQueue;				/* 0x81488474 + 584 */

/* ------------------------------------------------------------------ */
/* Shared data types                                                  */
/* ------------------------------------------------------------------ */

struct LiveAreaFrameState {
	int currentIndex;					/* +0x00 */
	float flipTimer;					/* +0x04 */
	int pendingLoads;					/* +0x08 */
	bool isSwitching;					/* +0x0C */
	unsigned char switchPadding[3];
	paf::ui::Widget *frameWidget;				/* +0x10 */
	int imageObjIndex;					/* +0x14 */
	int bgImageObjIndex;					/* +0x18 */
	int textObjIndex;					/* +0x1C */
	float textMarginLeft;					/* +0x20 */
	float textMarginTop;					/* +0x24 */
	float textMarginRight;					/* +0x28 */
	float textMarginBottom;					/* +0x2C */
	int scrollMode;						/* +0x30 */
	int scrollPadding;
	float scrollDeltaX;					/* +0x38 */
	float scrollDeltaY;					/* +0x3C */
	bool scrollActive;					/* +0x40 */
	unsigned char scrollActivePadding[3];
	int scrollPhase;					/* +0x44 */
	float scrollElapsed;					/* +0x48 */
	float scrollDuration;					/* +0x4C */
};

struct LiveAreaUpdater {
	LiveAreaUpdaterImpl *m_impl;
};

struct LiveAreaMgr {
	LiveAreaMgr(paf::Plugin *plugin);			/* 0x8119A698 */

	paf::Plugin *m_plugin;					/* +0x00 */
	LiveAreaTopmenuInterface *m_topmenuIf;			/* +0x04 */
	LiveAreaGameInterface *m_gameIf;			/* +0x08 */
	LiveAreaApplauncherInterface *m_applauncherIf;		/* +0x0C */
	int m_netState;						/* +0x10 */
	int m_signedInState;					/* +0x14 */
	float m_pollTimer;					/* +0x18 */
	paf::string m_pendingLaunchTitle;			/* +0x1C */
	int m_currentAppId;					/* +0x28 */
	int m_lsdbCookie;					/* +0x2C */
	LiveAreaUpdater m_updater;				/* +0x30 */
	int m_netCbId;						/* +0x34 */
};

class LiveAreaCacheMgr {
public:
	LiveAreaCacheMgr(paf::Plugin *plugin);			/* 0x8119A408 */
	int CreateCache(const paf::string &path,
			paf::intrusive_ptr<paf::cache::CacheImageInterface> &outRef,
			const paf::cache::CacheImage::Option *option,
			paf::job::JobQueue *queue);		/* 0x8119A51E */

	paf::cache::CacheManager *m_manager;
};

/*
 * Activity feed UI base classes (defined in 0x81348E94 - 0x8135D814).
 */
class ShellListItemBase : public paf::common::RefObj {
public:
	paf::ui::ListItem *m_listItem;				/* +0x08 */
	paf::Plugin *m_plugin;					/* +0x0C */
	paf::graph::SurfacePool *m_surfacePool;			/* +0x10 */
	paf::common::JobSlots m_itemJobSlots;			/* +0x14 (24 bytes -> +0x2C) */
};

struct ActivityRecord : public paf::common::RefObj {
	virtual int GetRecordKind() = 0;			/* slot 2 (+0x08) */
	unsigned char recordHeaderPadding[36];			/* +0x0C..+0x2F */
	SceUInt64 m_updateTimestamp;				/* +0x30 */
	unsigned char recordMiddlePadding[64];			/* +0x38..+0x77 */
	int m_commentCount;					/* +0x78 */
	int m_actionPadding;					/* +0x7C */
	paf::string m_actionUri;				/* +0x80 */
};

struct ActivityFeedCellSource : public paf::common::RefObj {
	unsigned char cellHeaderPadding[68];			/* +0x08..+0x4B */
	int m_totalCount;					/* +0x4C */
	SceUInt64 m_lastReadTime;				/* +0x50 */
	SceUInt64 m_lastUpdateTime;				/* +0x58 */
	SceUInt64 m_latestActivityTime;				/* +0x60 */
};

class ActivityItemBase : public ShellListItemBase {
public:
	paf::common::JobSlots m_feedJobSlots;			/* +0x2C (24 bytes -> +0x44) */
	ActivityRecord *m_record;				/* +0x44 */
	paf::common::SharedPtr<int> m_feedContext;		/* +0x48 */
	SceInt64 m_itemKey;					/* +0x50 */
	bool m_isInitialized;					/* +0x58 */
	unsigned char initPadding[3];				/* +0x59..+0x5B */
	paf::cache::CacheManager *m_cacheManager;		/* +0x5C */
	int m_displayMode;					/* +0x60 */
	int m_statusIndex;					/* +0x64 */
};

class ActivityFeedItemBase : public ActivityItemBase {
public:
	void *m_feedItemExtra;					/* +0x68 */
};

class ActivitySubItemBase : public ActivityItemBase {
public:
	ActivityRecord *m_parentRecord;				/* +0x68 */
};

class ActivityFeedVisualizerBase {
public:
	virtual ~ActivityFeedVisualizerBase() {}

	paf::Plugin *m_plugin;					/* +0x04 */
	paf::cache::CacheManager *m_cacheManager;		/* +0x08 */
};

class ActivitySubVisualizerBase {
public:
	virtual ~ActivitySubVisualizerBase() {}

	paf::Plugin *m_plugin;					/* +0x04 */
	paf::cache::CacheManager *m_cacheManager;		/* +0x08 */
	ActivityRecord *m_parentRecord;				/* +0x0C */
};

class ActivityStatusDlgBase {
public:
	virtual ~ActivityStatusDlgBase() {}

	int m_dialogId;						/* +0x04 */
	paf::Plugin *m_plugin;					/* +0x08 */
	unsigned char statusHelperBytes[20];			/* +0x0C..+0x1F */
	paf::cache::CacheManager *m_cacheManager;		/* +0x20 */
	ActivityRecord *m_record;				/* +0x24 */
	int m_displayMode;					/* +0x28 */
	paf::ui::Widget *m_dialogRoot;				/* +0x2C */
	paf::ui::Widget *m_contentRoot;				/* +0x30 */
	paf::common::JobSlots m_dialogJobSlots;			/* +0x34 (24 bytes -> +0x4C) */
	unsigned char activityClientBytes[116];			/* +0x4C..+0xBF */
};

class LiveAreaMoreCtrl {
public:
	typedef void (*MoreCallback)(void *);

	LiveAreaMoreCtrl(paf::ui::FileList *fileList, int segmentIndex, paf::Plugin *plugin,
			 MoreCallback callback, void *userdata); /* 0x811A1348 */
	virtual ~LiveAreaMoreCtrl();				/* 0x811A13C4 / 0x811A13EE */

	virtual void SetState(int state, paf::ui::Widget *header);	/* 0x811A141E */
	virtual void Enable();						/* 0x811A1AF2 */
	virtual void Disable();						/* 0x811A1B76 */
	virtual void Show();						/* 0x811A1BA2 */
	virtual void Hide();						/* 0x811A1C7C */
	virtual void OnResult(bool hasError);				/* 0x811A1D10 */
	virtual paf::IDParam GetTemplateId();				/* 0x811A1D2A */
	virtual paf::IDParam GetHeaderId();				/* 0x811A1D6E */
	virtual bool IsScrolledToBottom();				/* 0x811A1DB2 */
	virtual void OnScroll();					/* 0x811A1E4E */

	static void OnScrollEvent(int eventId, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userdata); /* 0x811A1E76 */

	paf::ui::FileList *m_fileList;				/* +0x04 */
	int m_segmentIndex;					/* +0x08 */
	paf::Plugin *m_plugin;					/* +0x0C */
	MoreCallback m_callback;				/* +0x10 */
	void *m_userdata;					/* +0x14 */
	bool m_isScrollHooked;					/* +0x18 */
	bool m_hasError;					/* +0x19 */
	unsigned char statePadding[2];
	int m_state;						/* +0x1C */
};

class LiveAreaPageInner {
public:
	LiveAreaPageInner(LiveAreaPage *page, const char *dir, const LiveAreaPageInitInfo *info); /* 0x811A3C72 */
	~LiveAreaPageInner();					/* 0x811A3F4E */

	void Init();						/* 0x811A417E */
	int TryReloadXml();					/* 0x811A44CA */
	void AnimatePopIn(paf::ui::Widget *widget, void *callback); /* 0x811A4684 */
	void OnInitialImagesLoaded();				/* 0x811A58D4 */
	void BuildLayout(int flags);				/* 0x811A985E */
	void OnRcoLoaded();					/* 0x811A9CCC */
	void OnParseFail();					/* 0x811A9CE4 */
	void OnParseComplete(paf::common::SharedPtr<LiTemplate> *parsed, int reloadMask,
			     paf::string *contentDir);		/* 0x811A9D0C */
	void SetGateSurface(paf::graph::Surface **surface);	/* 0x811AA854 */
	void ClearGateSurface();				/* 0x811AA9F2 */
	void LoadDefaultXml();					/* 0x811AAB9E */
	int GetFrameTimestamp(const char *frameId, SceUInt64 *outTimestamp); /* 0x811AACC2 */
	int QueueReload(int flags, const char *dir);		/* 0x811AAD60 */
	int QueueReloadFromReq(const LiveAreaReloadRequest *req); /* 0x811AAE02 */
	int GetGateNormPos(float *outX, float *outY);		/* 0x811AAE14 */
	int SwitchFrame(LiFrame *frameDesc, LiveAreaFrameState *frameState); /* 0x811AAE7C */
	void ResetGateFocus();					/* 0x811AAFB8 */
	void Tick();						/* 0x811AB056 */
	void OnScroll();					/* 0x811AB2DE */
	void StartReloadSpin();					/* 0x811AB38A */
	void StopReloadSpin(int error);				/* 0x811AB5DA */
	int GetReloadCount();					/* 0x811AB8F2 */
	int IsIntroTimerDone();					/* 0x811AB8F8 */
	void ClearGateInitFlag();				/* 0x811AB944 */
	void SetDirtyFlag();					/* 0x811AB94C */
	void UpdateInitInfo(const LiveAreaPageInitInfo *info);	/* 0x811AB954 */
	int GetResetMask();					/* 0x811AB9B8 */
	int HasPatchIcon();					/* 0x811ABA06 */
	int LayoutFrameChild(paf::ui::Widget *widget, bool animate); /* 0x811ABAB4 */
	LiveAreaFrameState *FindFrameState(paf::ui::Widget *widget, LiFrame **outDesc); /* 0x811ABC98 */

	LiveAreaPage *m_page;					/* +0x00 */
	int m_style;						/* +0x04 */
	int m_manualMode;					/* +0x08 */
	int m_upgradeMode;					/* +0x0C */
	paf::cache::CacheImageInterface *m_gateIconCache;	/* +0x10 */
	paf::wstring m_title;					/* +0x14 */
	paf::string m_searchKey;				/* +0x20 */
	paf::string m_url;					/* +0x2C */
	int m_appAttr;						/* +0x38 */
	int m_kind;						/* +0x3C */
	bool m_hasInfo;						/* +0x40 */
	bool m_hasExt;						/* +0x41 */
	bool m_hasFeed;						/* +0x42 */
	bool m_hasOnlineManual;					/* +0x43 */
	int m_rcoRefCount;					/* +0x44 */
	int m_reloadFlags;					/* +0x48 */
	paf::string m_reloadDir;				/* +0x4C */
	paf::string m_baseDir;					/* +0x58 */
	paf::string m_xmlPath;					/* +0x64 */
	paf::string m_helpUri;					/* +0x70 */
	int m_initPhase;					/* +0x7C */
	paf::common::SharedPtr<LiTemplate> m_xmlDoc;		/* +0x80 */
	paf::cache::CacheImageInterface *m_gateCacheRef;	/* +0x88 */
	paf::graph::Surface *m_gateSurface;			/* +0x8C */
	paf::ui::Widget *m_contentZone;				/* +0x90 */
	paf::ui::Widget *m_gateWidget;				/* +0x94 */
	paf::ui::Widget *m_gateBtn;				/* +0x98 */
	bool m_gateHasBg;					/* +0x9C */
	unsigned char gateBgPadding[3];
	int m_reloadCount;					/* +0xA0 */
	int m_reloadError;					/* +0xA4 */
	paf::common::SharedPtr<paf::job::JobItem> m_xmlJob;	/* +0xA8 */
	int m_pendingInitialLoads;				/* +0xB0 */
	bool m_shown;						/* +0xB4 */
	bool m_introDone;					/* +0xB5 */
	bool m_scrolledDown;					/* +0xB6 */
	bool m_hasLiveSurface;					/* +0xB7 */
	bool m_initialLoadNotified;				/* +0xB8 */
	bool m_templateOpened;					/* +0xB9 */
	bool m_gateResetDone;					/* +0xBA */
	bool m_dirty;						/* +0xBB */
	paf::list<LiveAreaFrameState *> m_frames;		/* +0xBC */
};

class LiveAreaPage {
public:
	LiveAreaPage(int pageKey, const char *titleId, const char *contentDir,
		     const LiveAreaPageInitInfo *info);		/* 0x8119B594 */
	~LiveAreaPage();					/* 0x8119B8EC */

	const char *GetTitleId();				/* 0x8119B990 */
	void UpdateLastLaunchTime();				/* 0x8119B9B2 */
	void ScrollToContent();					/* 0x8119BD0E */
	void ResetScroll(bool animate);				/* 0x8119BD32 */
	bool IsAtTop();						/* 0x8119BE16 */
	int CreateImageCache(const paf::string &path,
			     paf::intrusive_ptr<paf::cache::CacheImageInterface> &outRef); /* 0x8119BE9A */
	void EnsureFeed();					/* 0x8119BF26 */
	void ScrollIfPending();					/* 0x8119BF9C */
	void OnFocusComplete();					/* 0x8119C042 */
	void UpdateInitInfo(const LiveAreaPageInitInfo *info);	/* 0x8119C064 */
	int HasPatchIcon();					/* 0x8119C06E */

	int m_pageKey;						/* +0x00 */
	paf::string m_titleId;					/* +0x04 */
	paf::ui::Widget *m_rootWidget;				/* +0x10 */
	paf::cache::CacheManager *m_cacheManager;		/* +0x14 */
	paf::graph::SurfacePool *m_surfacePool;			/* +0x18 */
	paf::string m_templateName;				/* +0x1C */
	paf::ui::FileList *m_fileList;				/* +0x28 */
	bool m_feedVisible;					/* +0x2C */
	bool m_isActive;					/* +0x2D */
	bool m_pendingScroll;					/* +0x2E */
	unsigned char statePadding;
	LiveAreaPageInner m_inner;				/* +0x30 (200 bytes) */
	LiveAreaFeed *m_feed;					/* +0xF8 */
};

struct LiveAreaNewsEntry {
	paf::string ownerId;
	paf::string iconUrl;
};

class LiveAreaFeed {
public:
	LiveAreaFeed(LiveAreaPage *page, paf::cache::CacheImageInterface **iconCacheRef); /* 0x8119D6B8 */
	~LiveAreaFeed();					/* 0x8119D7CA */

	int StartSpinAnim();					/* 0x8119C7A4 */
	int RequestUpdate(int mode);				/* 0x8119C964 */
	void RebuildFeed();					/* 0x8119CA86 */
	int HideDialog(bool checkFirst);			/* 0x8119D7A2 */
	void EnqueueSetLastReadTime();				/* 0x8119D860 */
	void AnimateOutNewsIcon(paf::ui::Widget *widget);	/* 0x8119D9AC */
	void AnimateMoveNewsIcon(paf::ui::Widget *widget, float posX); /* 0x8119DABE */
	void AnimateDropInNewsIcon(paf::ui::Widget *widget);	/* 0x8119DBBE */
	void LayoutNewsIcons(bool hasMore);			/* 0x8119DCD2 */
	void UpdateNewsList(const paf::vector<ActivityDbStatus> &items, bool hasMore); /* 0x8119E48C */
	void SetVisible(bool visible);				/* 0x8119E6B6 */
	void OnNetChanged();					/* 0x8119E788 */
	void SetDialogId(int id);				/* 0x8119E790 */
	void UnlockListView();					/* 0x8119E7A4 */
	void LockListView();					/* 0x8119E7FC */
	void OnRefresh();					/* 0x8119E84E */
	int HandleSysUpdateError(int err);			/* 0x8119E85A */
	void EnqueueGetActivityNews();				/* 0x8119E9E8 */
	int UpdateTimestampText(SceUInt64 utcTick);		/* 0x8119EB54 */
	void UpdateEmptyNotice(bool hasItems);			/* 0x8119ED3C */
	void OnItemCountChanged(int count);			/* 0x8119EF64 */
	void OnUpdateResult(int err, int subErr, int mode, bool showDialog); /* 0x8119F0A6 */
	void StopSpinAnim();					/* 0x8119F204 */
	void FetchMoreActivities();				/* 0x8119F39C */

	int m_headerFlags;					/* +0x00 */
	LiveAreaMoreCtrl m_moreCtrl;				/* +0x04 (32 bytes) */
	LiveAreaPage *m_page;					/* +0x24 */
	paf::cache::CacheImageInterface *m_iconCacheRef;	/* +0x28 */
	paf::common::JobSlots m_jobSlots;			/* +0x2C */
	int m_state;						/* +0x44 */
	int m_updatingCount;					/* +0x48 */
	bool m_isVisible;					/* +0x4C */
	bool m_hasTimestamp;					/* +0x4D */
	unsigned char feedStatePadding[2];
	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0x50 */
	SceUInt64 m_lastUpdateTime;				/* +0x58 */
	SceUInt64 m_latestActivityTime;				/* +0x60 */
	int m_dialogId;						/* +0x68 */
	int m_lockCount;					/* +0x6C */
	paf::vector<LiveAreaNewsEntry> m_news;			/* +0x70 */
};

class LiveAreaStatusDlg : public ActivityStatusDlgBase {
public:
	LiveAreaStatusDlg(int *pageKeyPtr, paf::ui::Widget *parent, ActivityRecord **record,
			  int displayMode, paf::common::SharedPtr<int> *feedContext,
			  paf::common::SharedPtr<SceUInt64> *lastReadTime,
			  paf::cache::CacheImageInterface **iconRef); /* 0x811A078C */
	virtual ~LiveAreaStatusDlg();				/* 0x811A0A14 / 0x811A0B4C */

	void UnlockFeed();					/* 0x811A09E8 */
	void SetupIcon();					/* 0x811A0B5C */
	int LockFeed();						/* 0x811A0E32 */
	void BindContext(paf::common::SharedPtr<int> *ownerContext, const char *titleId); /* 0x811A0E5A */
	bool IsUnread();					/* 0x811A0F4E */
	bool GetShowUnreadBadge();				/* 0x811A0F98 */
	void SetShowUnreadBadge(bool show);			/* 0x811A0F9E */
	void OnStatusUpdated();					/* 0x811A0FA4 */
	void SetForceGameIcon(bool force);			/* 0x811A0FA6 */
	int TriggerSysUpdate();					/* 0x811A0FAC */

	static const paf::IDParam &GetDialogTemplateId();	/* 0x811A0FBE */
	static const paf::IDParam &GetDialogRootId();		/* 0x811A105A */
	static const paf::IDParam &GetContentRootId();		/* 0x811A10F6 */
	static const paf::IDParam &GetSubListId();		/* 0x811A1192 */

	int m_pageKey;						/* +0xC0 */
	bool m_locked;						/* +0xC4 */
	bool m_lockRequested;					/* +0xC5 */
	unsigned char lockStatePadding[2];
	paf::common::SharedPtr<SceUInt64> m_lastReadTime;	/* +0xC8 */
	paf::cache::CacheImageInterface *m_iconRef;		/* +0xD0 */
	paf::common::SharedPtr<int> m_ownerContext;		/* +0xD4 */
	paf::string m_titleId;					/* +0xDC */
	bool m_showUnreadBadge;					/* +0xE8 */
	bool m_forceGameIcon;					/* +0xE9 */
	unsigned char badgeFlagPadding[2];
};

extern void liveareaMoreCtrlSetCount(LiveAreaMoreCtrl *ctrl, int count); /* 0x811A1CA2 */
extern int liveareaFeedHandleSysUpdateErr(int err);			/* 0x8119E85A */
extern LiveAreaSubVisualizer *liveareaSubVisualizerCtor(void *mem, ActivityRecord **record,
							paf::common::SharedPtr<SceUInt64> *lastReadTime); /* 0x811A272C */
extern LiveAreaFeedVisualizer *liveareaFeedVisualizerCtor(void *mem,
							  paf::common::SharedPtr<SceUInt64> *lastReadTime,
							  paf::cache::CacheImageInterface **iconRef); /* 0x811A240E */

struct MusicCoreImposeInfo {
	int volume;						/* +0x00 */
	SceUInt8 playMode;					/* +0x04 */
	SceUInt8 bgmReason;					/* +0x05 */
	SceUInt8 bgmReasonPadding[2];				/* +0x06 */
	int bgmPriority;					/* +0x08 */
	char title[2048];					/* +0x0C */
	char artist[2048];					/* +0x80C */
	char album[2048];					/* +0x100C */
	SceUInt8 canPlay;					/* +0x180C */
	SceUInt8 canPlayPadding[3];				/* +0x180D */
	int durationLo;						/* +0x1810 */
	int durationHi;						/* +0x1814 */
	int posLo;						/* +0x1818 */
	int volStep;						/* +0x181C */
};

struct ShellAppEventPayload {
	int eventType;						/* +0x00 */
	int eventArg;						/* +0x04 */
	const char *titleId;					/* +0x08 */
};

/* ------------------------------------------------------------------ */
/* External shell, NetCtl, Activity UI, and MusicCore functions       */
/* ------------------------------------------------------------------ */

extern void *shellPluginGetInterface(const char *name, int id);		/* 0x810C9D14 */
extern int shellAppEventServerAddListener(void);			/* 0x810E17DE */
extern int shellAppEventServerRemoveListener(int type, int (*callback)(int, const char *),
					     void *userdata);		/* 0x810E17E6 */
extern void shellRecordMetric(int metricId, int value);			/* 0x81376D36 */

/* NetCtlInt network hold / connection request */
extern void netCtlIntAcquire(void);					/* 0x8120EE1E */
extern void netCtlIntRelease(void);					/* 0x8120EE3C */
extern void netCtlIntRequestConnect(void);				/* 0x8120EF44 */

/* NP Manager sign-in state & online ID */
extern int npManagerRegisterStateCallback(void (*callback)(int), void *userdata); /* 0x8132781C */
extern void npManagerUnregisterStateCallback(int callbackId);		/* 0x81327896 */
extern int npManagerGetSignInState(int *outSignedIn);			/* 0x81326F8A */
extern int npManagerGetOnlineId(char *outOnlineId);			/* 0x81326D40 */

/* ARL / graphics shader lifecycle */
extern void arlGraphicsInit(int option);				/* 0x8138D570 */
extern void arlGraphicsTerm(void);					/* 0x8138D77E */
extern paf::common::SharedPtr<paf::graph::Shader> *activityUiGetIconShader(void); /* 0x8139303C */

/* Activity Feed UI framework (0x81348E94 - 0x8135D814) */
extern void activityUiInit(void);					/* 0x8134A248 */
extern void activityUiTerm(void);					/* 0x8134A270 */
extern void activityUiSetNetCallbacks(void (*acquireFn)(void), void (*releaseFn)(void),
				      void (*connectFn)(void));		/* 0x8134A2EA */
extern void activityUiSetDialogPriority(int priority);			/* 0x8134A34C */
extern void activityUiEnqueueCacheInfoJob(void);			/* 0x8134A716 */
extern void activityUiSegmentSetCell(paf::ui::filelist::SegmentSource *source, int segmentIndex,
				     paf::common::RefObj **cellSource,
				     paf::ui::filelist::SegmentListAccessor::OptionParam *option); /* 0x8134A8D2 */
extern void activityUiSegmentRemoveCell(paf::ui::filelist::SegmentSource *source, int segmentIndex); /* 0x8134AAA2 */
extern void activityUiSegmentGetCell(ActivityFeedCellSource **outCellSource,
				     paf::ui::filelist::SegmentSource *source, int segmentIndex); /* 0x8134AB4C */
extern paf::ui::Widget *shellListItemCreateWidget(ShellListItemBase *self, paf::ui::Widget *parent,
						  const paf::IDParam *templateId,
						  const paf::IDParam *itemId); /* 0x8134ADF8 */
extern void shellListItemAttachWidget(ShellListItemBase *self);		/* 0x8134AEC2 */
extern void shellListItemBaseCtor(ShellListItemBase *self, int itemType, paf::Plugin *plugin,
				  const paf::IDParam *templateId, const paf::IDParam *itemId); /* 0x8134AFF6 */
extern paf::ui::Widget *shellListItemGetWidget(ShellListItemBase *self); /* 0x8134B4F6 */
extern void activityUiCellSourceSetCountCallback(ActivityFeedCellSource *cellSource,
						 void (*callback)(int, int), int pageKey); /* 0x8134BF04 */
extern void activityUiRefreshContext(paf::common::SharedPtr<int> *context); /* 0x8134C54C */
extern void activityUiItemBaseCtor(ActivityFeedItemBase *self, int itemType, paf::Plugin *plugin,
				   paf::cache::CacheManager *cacheManager,
				   paf::common::SharedPtr<int> *context); /* 0x8134DA7E */
extern int activityUiGetItemCategory(ActivityRecord **record);		/* 0x81349E1C */
extern int activityUiGetCurrentCategory(void);				/* 0x81349C32 */
extern int activityUiGetItemDisplayMode(ActivityFeedItemBase *self);	/* 0x812FEA62 */
extern void activityUiGetItemContext(paf::common::SharedPtr<int> *outContext,
				     ActivityFeedItemBase *self);	/* 0x8134D860 */
extern void activityUiFeedVisualizerBaseCtor(ActivityFeedVisualizerBase *self, paf::Plugin *plugin,
					     paf::cache::CacheManager *cacheManager); /* 0x813508E8 */
extern void activityUiInitFileList(paf::ui::FileList *fileList);	/* 0x81350B40 */
extern void activityUiStatusDlgBaseCtor(ActivityStatusDlgBase *self, paf::ui::Widget *parent,
					paf::Plugin *plugin, paf::cache::CacheManager *cacheManager,
					ActivityRecord **record, int displayMode,
					paf::common::SharedPtr<int> *feedContext,
					paf::common::SharedPtr<LiveAreaSubVisualizer> *subVisualizer); /* 0x8135286C */
extern void activityUiStatusDlgBaseDtor(ActivityStatusDlgBase *self);	/* 0x81352A64 */
extern void activityUiStatusDlgOpen(ActivityStatusDlgBase *self, paf::ui::ListItem *anchorItem); /* 0x813531DA */
extern void activityUiCloseDialog(void);				/* 0x81353DF2 */
extern void activityUiCloseDialogFast(void);				/* 0x81353E16 */
extern ActivityStatusDlgBase *activityUiFindDialogById(int dialogId);	/* 0x8135B002 */
extern int activityUiIsDialogActive(void);				/* 0x8135B002 */
extern void activityUiCreateStatusContexts(paf::common::SharedPtr<int> *outFeedContext,
					   paf::common::SharedPtr<int> *outOwnerContext,
					   ActivityDbQueryCondition *condition); /* 0x8135B0BA */
extern void activityUiSubItemBaseCtor(ActivitySubItemBase *self, int itemType, paf::Plugin *plugin,
				      paf::cache::CacheManager *cacheManager,
				      paf::common::SharedPtr<int> *context); /* 0x8135B39E */
extern void activityUiSubVisualizerBaseCtor(ActivitySubVisualizerBase *self, paf::Plugin *plugin,
					    paf::cache::CacheManager *cacheManager,
					    ActivityRecord **record);	/* 0x8135D0DC */
extern paf::ui::filelist::CellSource *activityUiCreateSingleCellSource(void); /* 0x8135D814 */

/* MusicCore plugin exports (musiccore_plugin.cpp) */
extern int musicPlayerSeek(int mode, int offset, int flags);		/* 0x811CBD5E */
extern int musicCorePlayerControl(SceUInt8 cmd, int param);		/* 0x811CBDE6 */
extern int musicCoreGetImposeInfo(MusicCoreImposeInfo *outInfo);	/* 0x811CBF10 */
extern int musicCoreGetLastError(int *outErr);				/* 0x811CC17C */
extern int musicCoreSetMute1(SceUInt8 mute);				/* 0x811CCC40 */
extern int musicCoreSetMute2(SceUInt8 mute);				/* 0x811CCD36 */

/* ------------------------------------------------------------------ */
/* livearea_plugin internal cross-TU functions                        */
/* ------------------------------------------------------------------ */

extern void liveareaRegisterInterface(paf::Plugin *plugin);		/* 0x81199D10 */
extern LiveAreaCacheMgr *liveareaCacheCreate(paf::Plugin *plugin);	/* 0x8119A5DE */
extern void liveareaCacheDestroy(void);					/* 0x8119A5F4 */
extern LiveAreaMgr *liveareaMgrCreate(paf::Plugin *plugin);		/* 0x8119A83C */
extern void liveareaMgrDestroy(void);					/* 0x8119A852 */
extern void liveareaMgrStart(void);					/* 0x8119A8C6 */
extern void liveareaMgrStop(void);					/* 0x8119A8D8 */
extern int liveareaMgrGetCachedTemplate(paf::common::SharedPtr<LiTemplate> *outTemplate,
					paf::string *titleId, SceUInt8 isDefault); /* 0x8119A99C */
extern void liveareaMgrUpdatePage(LiveAreaPageInner *pageInner, int mode); /* 0x8119A9BA */
extern void liveareaReplaceOnlineId(paf::string &out, const paf::string &in); /* 0x8119AB34 */
extern void liveareaMgrForcePoll(void);					/* 0x8119AC7A */
extern void liveareaMgrNotifyPage(const char *titleId, int reloadMask);	/* 0x8119AC9C */
extern void liveareaMgrClearPendingLaunch(void);			/* 0x8119ACE4 */
extern bool liveareaMgrIsPendingLaunch(const char *titleId);		/* 0x8119ACFC */
extern int liveareaIfInstallContent(LsdbAppInfoPtr *appInfo, bool force, bool isUpdate); /* 0x8119B222 */

extern int liveareaCronEnqueueJob(paf::common::SharedPtr<paf::job::JobItem> &item); /* 0x81199EC0 */
extern void liveareaCronRegister(void (*callback)(void *), void *arg);	/* 0x81199F26 */
extern void liveareaCronUnregister(void (*callback)(void *));		/* 0x81199FC0 */

extern LiveAreaPage *liveareaPageFindByTemplate(const char *name);	/* 0x8119C078 */
extern LiveAreaPage *liveareaPageFindByAppId(int pageKey);		/* 0x8119C0C6 */
extern LiveAreaPage *liveareaPageFindByListView(paf::ui::ListView *listView); /* 0x8119C0FA */
extern LiveAreaPage *liveareaPageFindByTitleId(const char *titleId);	/* 0x8119C12E */
extern void liveareaPageHideAllFeeds(void);				/* 0x8119C17C */
extern void liveareaPageOnShellPause(void);				/* 0x8119C1BC */
extern void liveareaPageOnShellResume(int pageKey);			/* 0x8119C234 */
extern void liveareaPageTickAll(bool changed);				/* 0x8119C312 */
extern int liveareaPageGetFrameTimestamp(const char *titleId, const char *frameId,
					 SceUInt64 *outTimestamp);	/* 0x8119C37E */
extern void liveareaPageReloadAll(void);				/* 0x8119C406 */
extern void liveareaPageReloadPocketStation(void);			/* 0x8119C4F2 */
extern const char *liveareaPageGetTitleIdRaw(LiveAreaPage *page);	/* 0x8119982C */
extern paf::ui::Widget *liveareaGetNewsContainer(LiveAreaPage *page);	/* 0x811D4E58 */

extern int liveareaIsGamePlayUri(ActivityRecord **recordPtr);		/* 0x8119F9EA */
extern void liveareaLaunchWithArg(const char *titleId, const char *statusParam); /* 0x811A064C */
extern paf::common::SharedPtr<paf::job::JobItem> liveareaEnqueueContentResetJob(LiveAreaPageInner *inner,
										int resetMask); /* 0x811A3426 */
extern int liveareaLoadRco(const paf::string *rcoName, const paf::string *pluginName, int drawPriority); /* 0x811ACF10 */
extern void liveareaUnloadRco(const paf::string *pluginName);		/* 0x811AD0C2 */
extern int liveareaEnqueueContentParserJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
					   LiveAreaPageInner *inner, const char *xmlPath,
					   const char *contentDir, SceUInt8 useLsdb,
					   SceUInt8 isDefault, int reloadMask, SceUInt8 allowDialog); /* 0x811B67AA */
extern paf::ui::filelist::Visualizer *liveareaCreateBgFactory(void);	/* 0x811B7470 */

/* Updater wrapper, implementation, daily task, task manager, and updator */
extern LiveAreaUpdater *liveareaUpdaterCtor(LiveAreaUpdater *updater);	/* 0x811A2910 */
extern LiveAreaUpdater *liveareaUpdaterDtor(LiveAreaUpdater *updater);	/* 0x811A292C */
extern int liveareaUpdaterStart(LiveAreaUpdater *updater);		/* 0x811A2940 */
extern int liveareaUpdaterStop(LiveAreaUpdater *updater);		/* 0x811A294A */
extern int liveareaUpdaterRequestContentUpdate(LiveAreaUpdater *updater, SceUInt64 *outId,
					       const char *titleId, const SceUInt64 *timestamp,
					       int mode, void *callback, int pageKey); /* 0x811A2954 */
extern int liveareaUpdaterRequestActivityRevert(LiveAreaUpdater *updater, SceUInt64 *outId,
						int titleId, int flags, int mode, int option); /* 0x811A2972 */
extern int liveareaUpdaterRequestPatchCheck(LiveAreaUpdater *updater, SceUInt64 *outId,
					    const char *titleId, int mode, int appType,
					    void *callback, int pageKey); /* 0x811A29C0 */
extern int liveareaUpdaterRequestFeedUpdate(LiveAreaUpdater *updater, SceUInt64 *outId,
					    const char *titleId, int mode, void *callback,
					    int pageKey);		/* 0x811A29DA */
extern int liveareaUpdaterRequestClean(LiveAreaUpdater *updater, SceUInt64 *outId,
				       const char *titleId, int mode, void *callback,
				       int pageKey);			/* 0x811A29F0 */

extern LiveAreaUpdaterImpl *liveareaUpdaterImplCtor(LiveAreaUpdaterImpl *self); /* 0x811A2A78 */
extern int liveareaUpdaterImplStop(LiveAreaUpdaterImpl *self);		/* 0x811A2AB0 */
extern LiveAreaUpdaterImpl *liveareaUpdaterImplDtor(LiveAreaUpdaterImpl *self); /* 0x811A2AC4 */
extern int liveareaUpdaterImplStart(LiveAreaUpdaterImpl *self);		/* 0x811A2AF4 */
extern int liveareaUpdaterImplUpdateContent(LiveAreaUpdaterImpl *self, SceUInt64 *outId,
					    const char *titleId, const SceUInt64 *timestamp,
					    int mode, void *callback, int pageKey); /* 0x811A2B1A */
extern int liveareaUpdaterImplCheckPatch(LiveAreaUpdaterImpl *self, SceUInt64 *outId,
					 const char *titleId, int mode, int appType,
					 void *callback, int pageKey);	/* 0x811A2B72 */
extern int liveareaUpdaterImplUpdateFeed(LiveAreaUpdaterImpl *self, SceUInt64 *outId,
					 const char *titleId, int mode, void *callback,
					 int pageKey);			/* 0x811A2BC8 */
extern int liveareaUpdaterImplCleanCache(LiveAreaUpdaterImpl *self, SceUInt64 *outId,
					 const char *titleId, int mode, void *callback,
					 int pageKey);			/* 0x811A2C18 */

extern LiveAreaTaskMgr *liveareaDailyTaskCtor(void *mem, int option);	/* 0x811A2CBC */
extern int liveareaDailyTaskStop(LiveAreaTaskMgr *taskMgr);		/* 0x811A2CC8 */
extern LiveAreaTaskMgr *liveareaDailyTaskDtor(LiveAreaTaskMgr *taskMgr); /* 0x811A2CEC */
extern int liveareaDailyTaskStart(LiveAreaTaskMgr *taskMgr);		/* 0x811A2D1C */

extern LiveAreaTaskMgr *liveareaTaskMgrCtor(void *mem);			/* 0x811B204C */
extern LiveAreaTaskMgr *liveareaTaskMgrDtor(LiveAreaTaskMgr *taskMgr);	/* 0x811B2068 */
extern int liveareaTaskMgrRequestContentUpdate(LiveAreaTaskMgr *taskMgr, SceUInt64 *outId,
					       const char *titleId, const SceUInt64 *timestamp,
					       bool requireNetwork, SceUInt8 checkDevDir,
					       SceUInt8 postNotice, void *callback,
					       void *userdata, const char *localPkg); /* 0x811B207C */
extern int liveareaTaskMgrRequestPatchCheck(LiveAreaTaskMgr *taskMgr, SceUInt64 *outId,
					    const char *titleId, bool requireNetwork,
					    int appType, void *callback, void *userdata); /* 0x811B20AC */
extern int liveareaTaskMgrRequestActivityUpdate(LiveAreaTaskMgr *taskMgr, SceUInt64 *outId,
						const char *titleId, int forceFlag,
						bool requireNetwork, void *callback,
						void *userdata);	/* 0x811B20C8 */
extern int liveareaTaskMgrRequestClean(LiveAreaTaskMgr *taskMgr, SceUInt64 *outId,
				       const char *titleId, bool requireNetwork,
				       void *callback, void *userdata);	/* 0x811B20E4 */

class LiveAreaUpdateTaskBase {
public:
	typedef void (*CompleteCallback)(int result, void *userdata);

	LiveAreaUpdateTaskBase(const char *titleId, CompleteCallback callback, void *userdata)
		: m_titleId(titleId ? titleId : ""),
		  m_callback(callback),
		  m_userdata(userdata),
		  m_canceled(0)
	{}

	virtual ~LiveAreaUpdateTaskBase() {}
	virtual int Run() = 0;
	virtual void OnFinish(int result);			/* 0x811B307C */
	virtual void Cancel();					/* 0x811B308E */

	paf::string m_titleId;					/* +0x04 */
	CompleteCallback m_callback;				/* +0x10 */
	void *m_userdata;					/* +0x14 */
	SceUInt8 m_canceled;					/* +0x18 */
};

extern LiveAreaUpdatorImpl *liveareaUpdatorImplCtor(void *mem);		/* 0x811B216C */
extern LiveAreaUpdatorImpl *liveareaUpdatorImplDtor(LiveAreaUpdatorImpl *self); /* 0x811B21A8 */
extern int liveareaUpdatorImplRequestContentUpdate(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
						   const char *titleId, const SceUInt64 *timestamp,
						   bool requireNetwork, SceUInt8 checkDevDir,
						   SceUInt8 postNotice, void *callback,
						   void *userdata, const char *localPkg); /* 0x811B21C0 */
extern int liveareaUpdatorImplRequestPatchCheck(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
						const char *titleId, bool requireNetwork,
						int appType, void *callback, void *userdata); /* 0x811B2532 */
extern int liveareaUpdatorImplRequestActivityUpdate(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
						    const char *titleId, int forceFlag,
						    bool requireNetwork, void *callback,
						    void *userdata);	/* 0x811B287E */
extern int liveareaUpdatorImplRequestClean(LiveAreaUpdatorImpl *self, SceUInt64 *outId,
					   const char *titleId, bool requireNetwork,
					   void *callback, void *userdata); /* 0x811B2BCE */
extern int liveareaUpdatorCancelById(LiveAreaUpdatorImpl *updator, const SceUInt64 *id); /* 0x811B2FA8 */

extern paf::job::JobItem *liveareaRequestUpdateJobCtor(void *mem, SceUInt64 id,
						       LiveAreaUpdatorImpl *updator,
						       SceUInt8 requireNetwork,
						       const paf::common::SharedPtr<LiveAreaUpdateTaskBase> &task); /* 0x811B3094 */
extern LiveAreaUpdateTaskBase *liveareaContentUpdateTaskCtor(void *mem, const char *titleId,
							     const SceUInt64 *timestamp,
							     SceUInt8 checkDevDir, SceUInt8 postNotice,
							     void *callback, void *userdata,
							     const char *localPkg); /* 0x811B31D2 */
extern LiveAreaUpdateTaskBase *liveareaPatchCheckTaskCtor(void *mem, const char *titleId,
							  int appType, void *callback,
							  void *userdata); /* 0x811B5474 */
extern LiveAreaUpdateTaskBase *liveareaActivityUpdateTaskCtor(void *mem, const char *titleId,
							      int forceFlag, void *callback,
							      void *userdata); /* 0x811B577A */
extern LiveAreaUpdateTaskBase *liveareaCleanTaskCtor(void *mem, const char *titleId,
						     void *callback, void *userdata); /* 0x811B6554 */

#endif
