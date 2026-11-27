/*
 * topmenu_internal.h: shared internal classes, structures, and cross-unit declarations
 * for SceShell's topmenu_plugin (0x8126F718-0x812A25FF).
 */
#ifndef SHELL_TOPMENU_INTERNAL_H
#define SHELL_TOPMENU_INTERNAL_H

#include "../../../../../paf/include/rtc/rtc.h"
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <std/map>
#include <std/memory>
#include <common/shared_ptr.h>
#include <common/main_thread_call_list.h>
#include <common/string_util.h>
#include <datetime/datetime.h>
#include <file/localfile.h>
#include <framework.h>
#include <plugin.h>
#include <system/system.h>
#include <timer/timer.h>
#include <thread/thread.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/surface/surface_screen.h>
#include <graphics/effects/screen_copy.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/graphmem/graphmem.h>
#include <cache/cache_manager.h>
#include <cache/cache_image.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <widget/w_plane.h>
#include <widget/w_box.h>
#include <widget/w_button.h>
#include <widget/w_supplier.h>
#include <widget/w_busyindicator.h>
#include <widget/w_progress_bar.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_pageturn.h>
#include <widget/w_text.h>
#include <widget/w_rich_text.h>
#include <inputdevice/inputlistener.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <libhttp/libhttp_url.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../livespace_db/livespace_db_internal.h"
#include "../../../shell.h"
#include "../../../boot.h"
#include "../../../plugins.h"
#include "../../../bgdl_api.h"
#include "../../../console_info/console_info.h"
#include "../../nsx/nsx_internal.h"
#include "../../esecron/include/eseutil.hpp"
#include "../../indicator_plugin/src/indicator_interface.h"
#include "../../power_manage_plugin/src/power_manage_internal.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../livearea_plugin/src/livearea_internal.h"
#include "../../../../game/game_plugin/game_plugin.h"
#include "topmenu_plugin.h"

/* Free-trial license info returned by game_plugin slot 58 (72 bytes) */
struct TopmenuFttLicenseInfo {
	char contentId[48];
	SceUInt64 startTick;
	SceUInt64 expireTick;
	SceUInt32 remainingSeconds;
	bool isTrialValid;
};
typedef char TopmenuFttLicenseInfoSizeCheck[sizeof(TopmenuFttLicenseInfo) == 72 ? 1 : -1];

/* Live coredump progress returned by sceAppMgrGetCoredumpStateForShell (1048 bytes) */
struct TopmenuCoredumpState {
	SceUID processId;
	SceInt32 status;
	SceInt32 progress;
	SceInt32 isCompleted;
	SceInt32 saveResult;
	char path[1024];
	SceInt32 isAutoUpload;
};
typedef char TopmenuCoredumpStateSizeCheck[sizeof(TopmenuCoredumpState) == 1048 ? 1 : -1];

/* Savedata info returned by sceAppMgrGetSaveDataInfo (28 bytes) */
struct TopmenuSaveDataInfo {
	SceUInt32 mediaType;
	SceUInt32 mountFlags;
	SceInt32 versionMismatch;
	SceUInt32 requiredAppVer;
	SceUInt32 accountFlags;
	SceUInt32 extraWordA;
	SceUInt32 extraWordB;
};
typedef char TopmenuSaveDataInfoSizeCheck[sizeof(TopmenuSaveDataInfo) == 28 ? 1 : -1];

/* License check result buffer passed to game_plugin slot 24/25 (232 bytes) */
struct TopmenuLicenseCheckResult {
	SceUInt32 words[58];
};
typedef char TopmenuLicenseCheckResultSizeCheck[sizeof(TopmenuLicenseCheckResult) == 232 ? 1 : -1];

/* Application launch attributes copied into TopmenuAppProcess (12 bytes) */
struct TopmenuAppAttrFlags {
	bool hasFlagBitSeven;
	SceInt32 flagBitEight;
	SceInt32 flagBitNine;
};

/* Crash notification event passed to topmenuHandleAppCrash (16 bytes) */
struct TopmenuCrashEvent {
	SceUID processId;
	SceInt32 reservedWord;
	SceInt32 appId;
	SceInt32 hasCoredump;
};

/* External shell / kernel / registry helpers used across topmenu_plugin */
extern "C" {
int sceRegMgrGetKeyInt(const char *category, const char *name, int *outValue);
int sceRegMgrSetKeyInt(const char *category, const char *name, int value);
int sceRegMgrGetKeyBin(const char *category, const char *name, void *buf, SceSize size);
int sceRegMgrRegisterCallback(SceUID cbId);
int sceRegMgrUnregisterCallback(SceUID cbId);
int sceRegMgrAddRegistryCallback(SceUID cbId, const char *path);
int sceRegMgrStartCallback(SceUID cbId);
SceUID sceKernelCreateCallback(const char *name, SceUInt32 attr,
				      int (*func)(int notifyId, int notifyCount, int notifyArg, void *common),
				      void *userData);
int sceKernelDeleteCallback(SceUID cbId);
int sceKernelDeleteSema(SceUID semaId);
int sceCompatGetPspSystemSoftwareVersion(void);
int sceAppMgrSetRecommendedScreenOrientationForShell(int orientation);
int sceAppMgrDestroyAppByName(const char *name);
int sceAppMgrDeactivateApp(int appId);
int sceAppMgrOverwriteLaunchParamForShell(int appId, const char *param);
int sceAppMgrPeekLaunchParamForShell(int appId, char *param, SceSize size);
int sceAppMgrLaunchAppByNameForShell(SceUInt32 flags, const char *name, const char *param);
int sceAppMgrLaunchAppByName2ndStage(int appId, const char *path, SceUInt32 budgetSize,
				     SceUInt32 processAttr, const AppmgrLaunchAppOption *option);
int sceAppMgrLaunchAppCancel(int appId);
int sceAppMgrResumeBgAppByShell(int appId);
int sceAppMgrSuspendBgAppByShell(int appId);
int sceAppMgrReceiveShellEvent(void *eventBuf, SceSize eventSize);
int sceAppMgrGetRunningAppIdListForShell(SceInt32 *appIds, SceInt32 maxCount);
int sceAppMgrReturnLiveAreaOperationResultForShell(int appId, int result);
int sceAppMgrSetBackRenderPortOwner(int appId);
int sceAppMgrGetRawPathOfApp0ByAppIdForShell(int appId, char *outPath);
int sceAppMgrReceiveShellEventNum(void);
int sceAppMgrGetCoredumpStateForShell(TopmenuCoredumpState *state);
int sceAppMgrFinishCoredumpForShell(void);
SceUID sceAppMgrGetProcessIdByAppIdForShell(int appId);
int sceAppMgrDestroyAppByAppId(int appId);
int sceAppMgrNotifyLiveBoardModeForShell(int appId, int mode);
int sceTouchSetProcessPrivilege(unsigned int privilege);
int sceAVConfigHdmiSetResolution(int mode, int apply);
int sceSharedFbGetRenderingInfo(SceUID pid);
SceUInt32 SceThreadmgr_47F6DE49(void);
SceUInt32 vshDisplayRegisterFrameBufCallback(SceUID cbId);
SceUInt32 vshCoredumpDeleteCrashReportCaf(void);
int SceNpManager_BF8921C8(int *outState);
int sceErrorGetExternalString(char *outBuf, int errorCode);
int SceLsdb_DD7D68C0(AppInfoSClient *client, LsdbQueryCondition *conditions,
		     paf::vector<paf::string> *outTitleIds, int option);
int SceLsdb_87EE0526(AppInfoSClient *client, const paf::string *titleId, LsdbAppInfoPtr *outInfo);
int SceLsdb_E2838878(AppInfoSClient *client, const char *uri, paf::string *outTitleId, int option);
const char *SceLsdb_8FDA59EA(const LsdbAppInfoPtr *app);
const char *sceLsdbGetSelfPath(const LsdbAppInfoPtr *app);
const char *sceLsdbGetDiscId(const LsdbAppInfoPtr *app);
SceInt64 sceLsdbGetParentalLevel(const LsdbAppInfoPtr *app);
SceInt64 sceLsdbGetBootable(const LsdbAppInfoPtr *app);
const char *sceLsdbGetAppVer(const LsdbAppInfoPtr *app);
const char *sceLsdbGetGameDataId(const LsdbAppInfoPtr *app);
const char *sceLsdbGetStitle(const LsdbAppInfoPtr *app);
const char *sceLsdbGetTitle(const LsdbAppInfoPtr *app);
const char *sceLsdbGetCategory(const LsdbAppInfoPtr *app);
const char *sceLsdbGetOriginalPath(const LsdbAppInfoPtr *app);
const char *SceLsdb_E26FC9E8(const LsdbAppInfoPtr *app);
const char *SceLsdb_2D538EB9(const LsdbAppInfoPtr *app);
int sceLsdbGetMetaContentsPath(const LsdbAppInfoPtr *app);
SceInt64 SceLsdb_849E8DC6(const LsdbAppInfoPtr *app);
SceInt32 SceLsdb_132FA3C6(const LsdbAppInfoPtr *app);
SceUInt32 SceLsdb_45B38A03(const LsdbAppInfoPtr *app);
SceInt64 SceLsdb_1518F869(const LsdbAppInfoPtr *app);
SceInt64 SceLsdb_43153457(const LsdbAppInfoPtr *app);
SceInt64 SceLsdb_9D27A87D(const LsdbAppInfoPtr *app);
SceInt64 SceLsdb_0B16523B(const LsdbAppInfoPtr *app);
int SceLsdb_DA0D7B0A(const LsdbAppInfoPtr *app);
int SceLsdb_44CD0D07(const LsdbAppInfoPtr *app);
bool SceLsdb_F4325AD0(const LsdbAppInfoPtr *app);
const char *SceLsdb_B7F9E89F(const LsdbAppInfoPtr *app);
int SceLsdb_C80B07C3(const LsdbAppInfoPtr *app);
int SceLsdb_D8DF91EE(paf::rtc::Tick *tick);
const char *SceLsdb_457F369E(const LsdbAppInfoPtr *app);
const char *SceLsdb_1B9A8B45(const LsdbAppInfoPtr *app);
const char *SceActivityDb_E6C23731(void);
int SceActivityDb_6D8000C1(void);
void *ScePafMisc_C11CB5E4(void *buf, const char *str, int mode);
const wchar_t *ScePafMisc_53948F03(void *buf);
void ScePafMisc_B45A7521(void *buf);
}

/* External shell NP sign-in state query (0x81329E20) */
int npCommonServiceIsSignedIn(int *outSignedIn);

/* External shell helpers (console_info / format) */
int consoleInfoFileAddTime(int index, SceUInt64 elapsedUs);		/* 0x81376D44 */
void shellSizeTextKb(paf::wstring *text, SceUInt32 sizeKB);		/* 0x8134367C */

/* Forward declarations */
class TopTabBase;
class TopTab;
class TopTabAppData;
class TopWindowBase;
class TopWindow;
class TopTabCacheManager;
class TopmenuDialogMgr;
class TopmenuHelpMgr;
struct TopmenuSurfacePoolSlot;
struct TopmenuWhatsNewMgr;
struct TopmenuBootScreen;

/* ------------------------------------------------------------------ */
/* Generic event listener list & event emitter (used by TopTabBase &  */
/* TopWindowBase, 0x812845A8 / 0x81284EB4)                            */
/* ------------------------------------------------------------------ */

typedef void (*TopmenuEventCallback)(SceInt32 eventType, void *sender, SceInt32 eventArg,
				     void *userData);

class TopmenuEventListener {
public:
	TopmenuEventListener() : m_callback(NULL), m_userData(NULL), m_pendingDelete(false) {}
	TopmenuEventListener(TopmenuEventCallback cb, void *userData)
		: m_callback(cb), m_userData(userData), m_pendingDelete(false)
	{
	}
	virtual ~TopmenuEventListener() {}
	virtual void Invoke(SceInt32 eventType, void *sender, SceInt32 eventArg)
	{
		if (m_callback != NULL)
			m_callback(eventType, sender, eventArg, m_userData);
	}

	TopmenuEventCallback m_callback;
	void *m_userData;
	bool m_pendingDelete;
};

class TopmenuEventListenerList {
public:
	TopmenuEventListenerList() : m_dispatching(false) {}
	virtual ~TopmenuEventListenerList()
	{
		for (paf::list<TopmenuEventListener *>::iterator it = m_listeners.begin();
		     it != m_listeners.end(); ++it) {
			delete *it;
		}
		m_listeners.clear();
	}

	void AddListener(TopmenuEventListener *listener)
	{
		for (paf::list<TopmenuEventListener *>::iterator it = m_listeners.begin();
		     it != m_listeners.end(); ++it) {
			if (*it == listener)
				return;
		}
		m_listeners.push_back(listener);
	}

	void RemoveListener(TopmenuEventCallback cb, void *userData)
	{
		for (paf::list<TopmenuEventListener *>::iterator it = m_listeners.begin();
		     it != m_listeners.end(); ++it) {
			TopmenuEventListener *listener = *it;
			if (listener != NULL && listener->m_callback == cb &&
			    listener->m_userData == userData) {
				if (m_dispatching) {
					listener->m_pendingDelete = true;
				} else {
					m_listeners.erase(it);
					delete listener;
				}
				return;
			}
		}
	}

	void Dispatch(SceInt32 eventType, void *sender, SceInt32 eventArg)
	{
		m_dispatching = true;
		for (paf::list<TopmenuEventListener *>::iterator it = m_listeners.begin();
		     it != m_listeners.end(); ++it) {
			TopmenuEventListener *listener = *it;
			if (listener != NULL)
				listener->Invoke(eventType, sender, eventArg);
		}
		m_dispatching = false;

		paf::list<TopmenuEventListener *>::iterator it = m_listeners.begin();
		while (it != m_listeners.end()) {
			TopmenuEventListener *listener = *it;
			if (listener == NULL || listener->m_pendingDelete) {
				paf::list<TopmenuEventListener *>::iterator nextIt = it;
				++nextIt;
				m_listeners.erase(it);
				delete listener;
				it = nextIt;
			} else {
				++it;
			}
		}
	}

	paf::list<TopmenuEventListener *> m_listeners;
	bool m_dispatching;
};

class TopmenuEventEmitter {
public:
	TopmenuEventEmitter() {}
	virtual ~TopmenuEventEmitter()
	{
		for (paf::map<SceInt32, TopmenuEventListenerList *>::iterator it = m_eventMap.begin();
		     it != m_eventMap.end(); ++it) {
			delete it->second;
		}
		m_eventMap.clear();
	}

	void AddEventListener(SceInt32 eventType, TopmenuEventCallback cb, void *userData)
	{
		TopmenuEventListener *listener = new TopmenuEventListener(cb, userData);
		paf::map<SceInt32, TopmenuEventListenerList *>::iterator it =
			m_eventMap.find(eventType);
		if (it == m_eventMap.end()) {
			TopmenuEventListenerList *list = new TopmenuEventListenerList();
			list->AddListener(listener);
			m_eventMap.insert(paf::pair<SceInt32, TopmenuEventListenerList *>(
				eventType, list));
		} else if (it->second != NULL) {
			it->second->AddListener(listener);
		}
	}

	void RemoveEventListener(SceInt32 eventType, TopmenuEventCallback cb, void *userData)
	{
		paf::map<SceInt32, TopmenuEventListenerList *>::iterator it =
			m_eventMap.find(eventType);
		if (it != m_eventMap.end() && it->second != NULL)
			it->second->RemoveListener(cb, userData);
	}

	void DispatchEvent(SceInt32 eventType, void *sender, SceInt32 eventArg)
	{
		paf::map<SceInt32, TopmenuEventListenerList *>::iterator it =
			m_eventMap.find(eventType);
		if (it != m_eventMap.end() && it->second != NULL)
			it->second->Dispatch(eventType, sender, eventArg);
	}

	paf::map<SceInt32, TopmenuEventListenerList *> m_eventMap;
};

/* ------------------------------------------------------------------ */
/* TopTabBase (68 bytes, 0x812845A8) and TopWindowBase (60 bytes,     */
/* 0x81284EB4)                                                        */
/* ------------------------------------------------------------------ */

class TopTabBase : public TopmenuEventEmitter {
public:
	TopTabBase(paf::Plugin *plugin, TopWindowBase *window, const paf::IDParam &templateId,
		   const paf::IDParam &rootWidgetId);
	virtual ~TopTabBase();
	virtual void OnEnterViewport(bool fromAbove) {}
	virtual void OnLeaveViewport(bool fromAbove) {}

	bool IsVisibleInViewport(const paf::math::v4 *boundsSize, paf::ui::Widget *overrideWidget);
	void UpdateViewportVisibility();

	paf::ui::Widget *m_widget;		/* +0x10 */
	paf::Plugin *m_plugin;			/* +0x14 */
	paf::ui::ScrollView *m_scrollView;	/* +0x18 */
	SceInt32 m_inViewport;			/* +0x1C */
	SceInt32 m_prevInViewport;		/* +0x20 */
	SceInt32 m_viewportAlignPad;		/* +0x24 */
	float m_lastViewPosX;			/* +0x28 */
	float m_lastViewPosY;			/* +0x2C */
	SceInt32 m_index;			/* +0x30 */
	SceInt32 m_tabKind;			/* +0x34 */
	float m_hitShrinkX;			/* +0x38 */
	float m_hitShrinkY;			/* +0x3C */
	SceInt32 m_pageKey;			/* +0x40 */
};

class TopWindowBase : public TopmenuEventEmitter {
public:
	TopWindowBase(paf::ui::Widget *rootWidget, SceInt32 layoutOrientation,
		      SceUInt32 initialCapacity, SceInt32 tabSpacing);
	virtual ~TopWindowBase();
	virtual void LayoutCustom(int layoutMode) {}
	virtual void OnScrollBegin(paf::ui::Event *event) {}
	virtual void OnScrollEnd(paf::ui::Event *event) {}

	void LayoutVertical(int layoutMode);
	void LayoutHorizontal(int layoutMode);
	void UpdateTabLayout(int layoutMode);
	bool InsertTabInternal(TopTabBase *tab, bool appendEnd, TopTabBase *afterTab);
	void AddTab(TopTabBase *tab, bool appendEnd, TopTabBase *afterTab);
	bool RemoveTabInternal(TopTabBase *tab, TopTabBase ***outNextIter);
	void RemoveTab(TopTabBase ***outNextIter, TopTabBase *tab);
	void MoveTabAfter(TopTabBase *tab, TopTabBase *afterTab);
	void AttachScrollView(paf::ui::ScrollView *scrollView);
	TopTabBase *FindFocusedTab();
	TopTabBase *FindNearestTab();
	TopTabBase *FindTabByPageKey(SceInt32 pageKey);
	TopTabBase *FindTabBySnapId(SceInt32 snapId);
	TopTabBase *GetNextTab(TopTabBase *tab);
	TopTabBase *GetPrevTab(TopTabBase *tab);
	SceInt32 GetTabSnapId(TopTabBase *tab);
	void ScrollToTab(TopTabBase *tab, float durationSec);
	void ScrollToTabAnimated(TopTabBase *tab);
	void RefreshLayoutImmediate();
	void RefreshLayoutDefault();

	paf::ui::Widget *m_rootWidget;		/* +0x10 */
	paf::ui::ScrollView *m_scrollView;	/* +0x14 */
	SceInt32 m_layoutOrientation;		/* +0x18 */
	paf::vector<TopTabBase *> m_tabs;	/* +0x1C (16 bytes) */
	SceInt32 m_nextPageKey;			/* +0x2C */
	SceUInt32 m_initialCapacity;		/* +0x30 */
	SceInt32 m_tabSpacing;			/* +0x34 */
	bool m_emitEvents;			/* +0x38 */
};

typedef char TopTabBaseSizeCheck[sizeof(TopTabBase) == 68 ? 1 : -1];
typedef char TopWindowBaseSizeCheck[sizeof(TopWindowBase) == 60 ? 1 : -1];

/* ------------------------------------------------------------------ */
/* TopTab (132 bytes, 0x81282826)                                     */
/* ------------------------------------------------------------------ */

class TopTab : public TopTabBase {
public:
	TopTab(paf::Plugin *plugin, bool isHomeTab);
	virtual ~TopTab();
	virtual void OnEnterViewport(bool fromAbove);
	virtual void OnLeaveViewport(bool fromAbove);

	bool CanClose() const;
	void SetIconSurface(const paf::intrusive_ptr<paf::graph::Surface> &surface);
	void SetIconCache(const paf::intrusive_ptr<paf::cache::CacheImageInterface> &imageCache);
	void SetTitleText(const paf::wstring &title);
	void HideIcon();
	void RefreshOrderStamp();
	void InitHeaderWidgets(paf::Plugin *plugin);
	void SetActiveTab(bool active);

	bool m_lockedByApp;						/* +0x44 */
	paf::ui::Widget *m_contentRoot;					/* +0x48 */
	paf::ui::Widget *m_headerWidget;				/* +0x4C */
	paf::ui::Text *m_titleText;					/* +0x50 */
	paf::intrusive_ptr<paf::graph::Surface> m_iconSurface;		/* +0x54 */
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_iconCache; /* +0x58 */
	SceInt32 m_tabMode;						/* +0x5C */
	TopTabAppData *m_appData;					/* +0x60 */
	TopmenuSurfacePoolSlot *m_poolSlot;				/* +0x64 */
	paf::wstring m_title;						/* +0x68 */
	bool m_isActiveTab;						/* +0x74 */
	SceInt32 m_orderStamp;						/* +0x78 */
	bool m_isClosing;						/* +0x7C */
	float m_hoverProgress;						/* +0x80 */
};

typedef char TopTabSizeCheck[sizeof(TopTab) == 132 ? 1 : -1];

/* ------------------------------------------------------------------ */
/* TopTabCacheManager (4 bytes, 0x812928A0)                           */
/* ------------------------------------------------------------------ */

class TopTabCacheManager {
public:
	TopTabCacheManager(paf::graph::SurfacePool *surfacePool);
	~TopTabCacheManager();

	int CreateImageCache(const paf::string &path,
			     paf::intrusive_ptr<paf::cache::CacheImageInterface> &outImage,
			     const paf::cache::CacheImage::Option *option,
			     paf::job::JobQueue *jobQueue);

	paf::cache::CacheManager *GetCacheManager() const { return m_cacheManager; }

private:
	paf::cache::CacheManager *m_cacheManager;
};

/* Liveboard surface pool + cache manager pair (6 slots at 0x815AC490) */
struct TopmenuSurfacePoolSlot {
	paf::graph::SurfacePool *surfacePool;
	TopTabCacheManager *cacheManager;
};

/* ------------------------------------------------------------------ */
/* Application process controllers (40B base + 104B derived,          */
/* 0x8129A0E0 / 0x81286FDE)                                           */
/* ------------------------------------------------------------------ */

class TopmenuAppProcess;

class TopmenuProcessBase {
public:
	TopmenuProcessBase(TopmenuAppProcess *owner);
	~TopmenuProcessBase();

	TopmenuAppProcess *m_owner;				/* +0x00 */
	bool m_enabled;						/* +0x04 */
	paf::common::SharedPtr<paf::job::JobItem> m_spawnJob;	/* +0x08 */
	SceInt32 m_pendingAction;				/* +0x10 */
	SceInt32 m_procState;					/* +0x14 */
	bool m_wantActive;					/* +0x18 */
	bool m_isActive;					/* +0x19 */
	bool m_suspended;					/* +0x1A */
	bool m_hasAppId;					/* +0x1B */
	SceInt32 m_appId;					/* +0x1C */
	bool m_liveAreaFocus;					/* +0x20 */
	SceUInt32 m_bootTimeStartUs;				/* +0x24 */
};

class TopmenuAppProcess : public TopmenuProcessBase {
public:
	TopmenuAppProcess(const paf::string &titleId, const LsdbAppInfoPtr &appInfo,
			  TopTabAppData *tabAppData, SceInt32 launchMode,
			  bool autoStart, SceInt32 startArg);
	~TopmenuAppProcess();

	bool StartOrResume(SceInt32 startArg);
	const char *GetBgTitleId() const;
	bool ResumeWithArg(SceInt32 startArg, SceInt32 resumeFlag);
	bool ResumeDefault();
	int SendProcessCommand(SceInt32 command);
	void DestroyOrDefer();
	bool IsProcessRunning() const;
	void SetAttrFlags(const TopmenuAppAttrFlags *flags);
	void SetParentAppId(SceInt32 parentAppId);
	void UpdateAppInfo(const LsdbAppInfoPtr &appInfo);
	void SetHasBgPartner(bool hasPartner);
	void OnEnterViewport();
	void OnLeaveViewport();
	void OnResumeForeground();
	void OnSuspendBackground();
	void StartAppCheck(const char *debugBasePath);
	bool OnCheckJobComplete();
	void ApplyAppAttributes();
	void OnAppCheckSucceeded(const LsdbAppInfoPtr &appInfo);

	paf::string m_titleId;					/* +0x28 */
	TopTabAppData *m_tabAppData;				/* +0x34 */
	SceInt32 m_launchState;					/* +0x38 */
	TopmenuBgLayerOption m_bgLayerOpt;			/* +0x3C */
	SceInt32 m_parentAppId;					/* +0x48 */
	SceInt32 m_launchMode;					/* +0x4C */
	bool m_pendingDelete;					/* +0x50 */
	LsdbAppInfoPtr m_appInfo;				/* +0x54 */
	paf::common::SharedPtr<paf::job::JobItem> m_checkJob;	/* +0x5C */
	bool m_pendingStart;					/* +0x64 */
	bool m_launchedBgPartner;				/* +0x65 */
	bool m_pendingResume;					/* +0x66 */
};

typedef char TopmenuProcessBaseSizeCheck[sizeof(TopmenuProcessBase) == 40 ? 1 : -1];
typedef char TopmenuAppProcessSizeCheck[sizeof(TopmenuAppProcess) == 104 ? 1 : -1];

/* ------------------------------------------------------------------ */
/* TopTabScreenCopy (16B) & TopTabAppData (184B, 0x8129666C..8D23)    */
/* ------------------------------------------------------------------ */

class TopTabScreenCopy {
public:
	TopTabScreenCopy(TopTabAppData *owner, paf::graph::SurfacePool *pool, bool fullSize);
	~TopTabScreenCopy();

	void ClearState();
	void CaptureAndPublish();
	bool IsReady() const;

	TopTabAppData *m_owner;					/* +0x00 */
	paf::intrusive_ptr<paf::graph::Surface> m_surface;	/* +0x04 */
	paf::graph::ScreenCopyObj *m_copyObj;			/* +0x08 */
	bool m_captured;					/* +0x0C */
};

class TopTabAppData {
public:
	TopTabAppData(TopTab *ownerTab, const paf::string &titleId, const LsdbAppInfoPtr &appInfo,
		      SceInt32 launchMode, bool autoStart, SceInt32 startArg);
	~TopTabAppData();

	void HideAnimated(float durationSec);
	void ShowAnimated(float durationSec);
	void SetAppViewState(SceInt32 state);
	void SetSnapshotSurface(const paf::intrusive_ptr<paf::graph::Surface> &surface);
	void Update();
	void SetPageTurnDisabled(bool disabled);
	int ReloadLiveArea(const LiveAreaReloadRequest *req);
	void OnActivateTab();
	void OnDeactivateTab();
	void OnBgLoadStepDone();
	paf::intrusive_ptr<paf::graph::Surface> GetScreenCopySurface() const;
	void UpdateUpStatus(SceInt32 newStatus);
	void FillLiveAreaInitInfo(LiveAreaPageInitInfo *outInfo);
	void RefreshLiveAreaInfo();
	void OnEnterViewport();
	void OnLeaveViewport();
	void ReportLiveAreaOpen();
	void ReportLiveAreaClose();
	void LoadBackgroundImage();
	void SetupContent();
	void RequestCancelLaunch();

	paf::string m_titleId;					/* +0x00 */
	TopTab *m_ownerTab;					/* +0x0C */
	bool m_noScreenCopy;					/* +0x10 */
	bool m_hasSnapshot;					/* +0x11 */
	bool m_shownOnce;					/* +0x12 */
	SceInt32 m_upStatus;					/* +0x14 */
	paf::ui::Widget *m_rootWidget;				/* +0x18 */
	paf::ui::PageTurn *m_pageTurn;				/* +0x1C */
	paf::ui::Widget *m_contentWidget;			/* +0x20 */
	paf::ui::Widget *m_snapshotPlane;			/* +0x24 */
	paf::ui::BusyIndicator *m_busyIndicator;		/* +0x28 */
	paf::ui::Supplier *m_cornerSupplier;			/* +0x2C */
	TopTabScreenCopy *m_screenCopy;				/* +0x30 */
	SceInt32 m_viewKind;					/* +0x34 */
	SceInt32 m_appViewState;				/* +0x38 */
	SceInt32 m_captureCountdown;				/* +0x3C */
	SceInt32 m_bgLoadPendingCount;				/* +0x40 */
	SceInt32 m_dialogId;					/* +0x44 */
	SceInt32 m_mediaAppType;				/* +0x48 */
	SceInt32 m_mediaStorageKind;				/* +0x4C */
	TopmenuAppProcess m_process;				/* +0x50 */
};

typedef char TopTabScreenCopySizeCheck[sizeof(TopTabScreenCopy) == 16 ? 1 : -1];
typedef char TopTabAppDataSizeCheck[sizeof(TopTabAppData) == 184 ? 1 : -1];

/* Promotion error context (68 bytes, 0x81289E86 / 0x8128A1F8 / 0x8128ACC8) */
struct TopmenuInstallError {
	TopmenuInstallError();

	SceInt32 errorCode;					/* +0x00 */
	bool isAddcont;						/* +0x04 */
	paf::string url;					/* +0x08 */
	paf::string path;					/* +0x14 */
	paf::string title;					/* +0x20 */
	paf::string titleId;					/* +0x2C */
	paf::string iconPath;					/* +0x38 */
};

typedef char TopmenuInstallErrorSizeCheck[sizeof(TopmenuInstallError) == 68 ? 1 : -1];

/* Helper structs embedded in TopWindow */
struct TopmenuLaunchSeq {
	SceInt32 state;						/* +0x00 */
	SceInt32 targetPageKey;					/* +0x04 */
	bool autoStart;						/* +0x08 */
	bool skipScroll;					/* +0x09 */
	bool isWebApp;						/* +0x0A */
	bool openPatchChk;					/* +0x0B */
	bool holdBusy;						/* +0x0C */
	SceInt32 closePageKey;					/* +0x10 */
	bool immediateStart;					/* +0x14 */
	bool notifyApplauncher;					/* +0x15 */
};
typedef char TopmenuLaunchSeqSizeCheck[sizeof(TopmenuLaunchSeq) == 24 ? 1 : -1];

struct TopmenuLaunchSeqParam {
	SceInt32 targetPageKey;					/* +0x00 */
	bool autoStart;						/* +0x04 */
	bool holdBusy;						/* +0x05 */
	bool skipScroll;					/* +0x06 */
	SceInt32 closePageKey;					/* +0x08 */
	bool immediateStart;					/* +0x0C */
	bool isWebApp;						/* +0x0D */
	bool openPatchChk;					/* +0x0E */
	bool notifyApplauncher;					/* +0x0F */
};
typedef char TopmenuLaunchSeqParamSizeCheck[sizeof(TopmenuLaunchSeqParam) == 16 ? 1 : -1];

struct TopmenuBgRequest {
	void *owner;						/* +0x00 */
	SceInt32 priority;					/* +0x04 */
	SceInt32 styleKind;					/* +0x08 */
	SceInt32 style;						/* +0x0C */
	SceUInt32 color;					/* +0x10 */
};
typedef char TopmenuBgRequestSizeCheck[sizeof(TopmenuBgRequest) == 20 ? 1 : -1];

struct TopmenuAppFlagEntry {
	SceUInt64 flags;					/* +0x00 */
	SceInt32 appId;						/* +0x08 */
	char appName[32];					/* +0x0C */
	bool registered;					/* +0x2C */
};
typedef char TopmenuAppFlagEntrySizeCheck[sizeof(TopmenuAppFlagEntry) == 48 ? 1 : -1];

/* ------------------------------------------------------------------ */
/* TopWindow (388 bytes, 0x812740FE)                                  */
/* ------------------------------------------------------------------ */

class TopWindow : public TopWindowBase {
public:
	TopWindow(paf::Plugin *plugin);
	virtual ~TopWindow();
	virtual void LayoutCustom(int layoutMode);

	IndicatorPluginInterface *m_indicatorIf;		/* +0x3C */
	ImposePluginInterface *m_imposeIf;			/* +0x40 */
	power_manage::PowerManageInterface *m_powerManageIf;	/* +0x44 */
	GamePluginInterface *m_gameIf;				/* +0x48 */
	ApplauncherInterface *m_applauncherIf;			/* +0x4C */
	LiveAreaPluginInterface *m_liveAreaIf;			/* +0x50 */
	paf::intrusive_ptr<paf::graph::Surface> m_defaultBgTex;	/* +0x54 */
	paf::ui::Widget *m_homeTabWidget;			/* +0x58 */
	paf::ui::Widget *m_overlayBgWidget;			/* +0x5C */
	paf::ui::Widget *m_backPlaneWidget;			/* +0x60 */
	paf::ui::Scene *m_gateEffectScene;			/* +0x64 */
	TopTab *m_currentTab;					/* +0x68 */
	TopmenuAppProcess *m_foregroundApp;			/* +0x6C */
	SceInt32 m_activeSnapId;				/* +0x70 */
	paf::vector<TopmenuSurfacePoolSlot *> m_freePools;	/* +0x74 */
	SceInt32 m_carouselState;				/* +0x84 */
	TopTab *m_hoveredTab;					/* +0x88 */
	TopTab *m_draggedTab;					/* +0x8C */
	SceInt32 m_pendingScrollPageKey;			/* +0x90 */
	bool m_carouselAnimating;				/* +0x94 */
	bool m_carouselDragging;				/* +0x95 */
	paf::ui::Widget *m_focusedNavWidget;			/* +0x98 */
	SceInt32 m_navFocusMode;				/* +0x9C */
	bool m_homeInputBlocked;				/* +0xA0 */
	bool m_closingAppPending;				/* +0xA1 */
	bool m_switchToHomeOnClose;				/* +0xA2 */
	TopmenuAppProcess *m_closingApp;			/* +0xA4 */
	SceInt32 m_shellViewMode;				/* +0xA8 */
	paf::list<TopmenuProcessBase *> m_pendingLaunchQueue;	/* +0xAC */
	SceInt32 m_pendingClosePageKey;				/* +0xB8 */
	bool m_touchActive;					/* +0xBC */
	bool m_touchSuspendRequested;				/* +0xBD */
	bool m_touchForceResume;				/* +0xBE */
	SceInt32 m_busyRefCount;				/* +0xC0 */
	SceInt32 m_overlayPlaneRefCount;			/* +0xC4 */
	SceInt32 m_suspendLockCount;				/* +0xC8 */
	SceInt32 m_gameHoldCount;				/* +0xCC */
	bool m_editModeActive;					/* +0xD0 */
	paf::job::JobQueue *m_jobQueue;				/* +0xD4 */
	SceInt32 m_bgStyleKind;					/* +0xD8 */
	SceInt32 m_bgColorParam;				/* +0xDC */
	SceInt32 m_bgWaveParam;					/* +0xE0 */
	bool m_gateEffectActive;				/* +0xE4 */
	bool m_gateEffectReverse;				/* +0xE5 */
	paf::intrusive_ptr<paf::graph::Surface> m_gateEffectSurface; /* +0xE8 */
	float m_gateEffectOriginX;				/* +0xEC */
	float m_gateEffectOriginY;				/* +0xF0 */
	TopmenuAppProcess *m_gateEffectApp;			/* +0xF4 */
	paf::intrusive_ptr<paf::graph::Surface> m_appCloseSnapshot;  /* +0xF8 */
	TopmenuLaunchSeq m_launchSeq;				/* +0xFC */
	SceInt32 m_pendingKillAppId;				/* +0x114 */
	bool m_closingAllTabs;					/* +0x118 */
	SceInt32 m_deactivateDelayFrames;			/* +0x11C */
	paf::ui::Widget *m_arrowUpWidget;			/* +0x120 */
	paf::ui::Widget *m_arrowDownWidget;			/* +0x124 */
	bool m_arrowUpVisible;					/* +0x128 */
	bool m_arrowDownVisible;				/* +0x129 */
	paf::vector<TopmenuBgRequest> m_bgRequests;		/* +0x12C */
	bool m_openingLiveArea;					/* +0x13C */
	SceInt32 m_openingPageKey;				/* +0x140 */
	bool m_liveAreaReady;					/* +0x144 */
	bool m_skipDestroyCheck;				/* +0x145 */
	TopmenuAppProcess *m_bgLaunchingApp;			/* +0x148 */
	paf::common::SharedPtr<paf::inputdevice::InputListener> m_carouselInputListener; /* +0x14C */
	paf::ui::Widget *m_iduOverlayWidget;			/* +0x154 */
	bool m_navArrowsEnabled;				/* +0x158 */
	SceInt32 m_scrollLockCount;				/* +0x15C */
	bool m_whatsNewNotifyEnabled;				/* +0x160 */
	SceInt32 m_psnSignInState;				/* +0x164 */
	SceInt32 m_powerSuspendMode;				/* +0x168 */
	paf::list<TopmenuAppFlagEntry> m_appFlagList;		/* +0x16C */
	paf::list<SceInt32> m_appTerminateWaiters;		/* +0x178 */
};

typedef char TopWindowSizeCheck[sizeof(TopWindow) == 388 ? 1 : -1];

/* ------------------------------------------------------------------ */
/* Shared globals across topmenu_plugin TUs                           */
/* ------------------------------------------------------------------ */

extern paf::Plugin *g_topmenuPlugin;				/* 0x815AC594 */
extern TopmenuPluginInterface *g_topmenuInterface;		/* 0x815AC590 */
extern paf::memory::HeapAllocator *g_topmenuHeap;		/* 0x815AC568 */
extern TopWindow *g_topWindow;					/* 0x815AC4CC */
extern TopmenuDialogMgr *g_topmenuDialogMgr;			/* 0x815AC778 */
extern TopmenuHelpMgr *g_topmenuHelpMgr;			/* 0x815AC7C8 */
extern TopmenuWhatsNewMgr *g_topmenuWhatsNewMgr;		/* 0x815AC520 */
extern TopmenuBootScreen *g_topmenuBootScreen;			/* 0x815AC6F0 */
extern bool g_topmenuTestCrashEnabled;				/* 0x815AC4C0 */
extern SceGxmRenderTarget *g_topmenuSmallRenderTarget;		/* 0x815AC4C4 */
extern SceGxmRenderTarget *g_topmenuLargeRenderTarget;		/* 0x815AC4C8 */
extern bool g_topmenuHeaderFocusByPad;				/* 0x815AC4F8 */
extern SceInt32 g_topmenuNextTabOrderStamp;			/* 0x8154573C */
extern SceInt32 g_topmenuDisabledPageTurnCount;			/* 0x81545A30 */

/* ------------------------------------------------------------------ */
/* Cross-TU internal APIs                                             */
/* ------------------------------------------------------------------ */

/* topmenu_interface.cpp (0x81284398) */
void topmenuInterfaceInit(paf::Plugin *plugin);
void topmenuInterfaceTerm(void);

/* topmenu_heap.cpp (0x8128430C) */
void topmenuHeapInit(void);
void topmenuHeapTerm(void);

/* topmenu_plane.cpp (0x812844A0) */
paf::ui::Widget *topmenuCreatePlane(paf::ui::Widget *parent);

/* topmenu_lsdb_sync.cpp (0x813C9AC0) */
int topmenuLsdbSyncInit(void);
void topmenuLsdbSyncTerm(void);
void topmenuLsdbSyncStart(void);
void topmenuLsdbSyncStop(void);

/* topmenu_dialog_mgr.cpp (0x81292AF4..0x81292E87) */
void topmenuDialogMgrInit(void);
void topmenuDialogMgrTerm(void);
void topmenuDialogRegister(int dialogId);
void topmenuDialogRegisterDeferred(int dialogId);
void topmenuDialogCloseAll(void);
void topmenuDialogOnPowerHold(int dialogId);
void topmenuDialogOnPowerRelease(int dialogId);
int topmenuOpenErrorDialog(CommonGuiMessageDialogParam *param);
void topmenuDialogRegisterAndHold(int dialogId);
void topmenuDialogPowerKeyCallback(void *userData);

/* topmenu_help.cpp (0x81294B24..0x81296253) */
void topmenuHelpInit(void);
void topmenuHelpTerm(void);
void topmenuHelpPrepareLiveAreaIntro(void);
void topmenuHelpShowLiveAreaIntro(void);
void topmenuHelpCancelLiveAreaIntro(void);
void topmenuHelpPrepareLiveAreaPeel(void);
void topmenuHelpShowLiveAreaPeel(void);
void topmenuHelpShowHomeHelp(void);
void topmenuHelpShowEditHelp(void);

/* import_savedata_launcher.cpp (0x812962C4..0x812965FB) */
int topmenuLaunchImportSavedata(const LsdbAppInfoPtr *appInfo);

/* chk_ux0_free_space_job.cpp (0x81298D24..0x81298FD7) */
void topmenuStartCheckUx0FreeSpace(void);

/* np_comm_id_search_job.cpp (0x81299048..0x812997D9) */
int topmenuStartNpCommIdSearch(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			       const char *npCommId, int launchAppId);

/* online_storage_launcher.cpp (0x8129984C..0x81299A4D) */
int topmenuLaunchOnlineStorage(const LsdbAppInfoPtr *appInfo);

/* topmenu_input_listener.cpp (0x81299AC0..0x81299BDF) */
paf::inputdevice::InputListener *topmenuCreateInputListener(void);

/* patch_chk_launcher.cpp (0x81299C50..0x8129A06D) */
int topmenuLaunchPatchChk(const LsdbAppInfoPtr *appInfo);

/* topmenu_gesture_listener.cpp (0x8129B5E0..0x8129BA01) */
void topmenuMotionInit(void);
void topmenuMotionUpdateFrontApp(void);

/* TopWindow / WhatsNew / BootScreen / Cleanup lifecycle hooks (implemented in later batches) */
void topmenuTopWindowInitConfig(void);
void topmenuTopWindowStart(paf::Plugin *plugin);
void topmenuTopWindowStop(void);
void topmenuTopWindowExit(void);
void topmenuWhatsNewInit(void);
void topmenuWhatsNewTerm(void);
void topmenuWhatsNewShow(void);
void topmenuWhatsNewHide(void);
void topmenuWhatsNewCheckRefresh(void);
void topmenuWhatsNewForceRefresh(void);
void topmenuWhatsNewOnAgeChanged(void);
void topmenuBootScreenStart(void);
void topmenuBootScreenStop(void);
int topmenuCheckCrashReportFile(void);

/* Batch 2 cross-TU APIs */
typedef void (*TopmenuPromoteDoneCallback)(SceInt32 result, void *userData);
bool topmenuStartAppDataPromote(const LsdbAppInfoPtr *appInfo,
				TopmenuPromoteDoneCallback callback,
				void *userData, bool isLaunchPromote);
void topmenuPushPowerKeyHandler(power_manage::PowerKeyCallback callback, void *userData);
void topmenuPopPowerKeyHandler(power_manage::PowerKeyCallback callback, void *userData);
void topmenuAddTabLeaveListener(SceInt32 pageKey, TopmenuEventCallback callback, void *userData);
void topmenuRemoveTabLeaveListener(SceInt32 pageKey, TopmenuEventCallback callback, void *userData);
int topmenuShowAppErrorMessageDialog(SceInt32 errorCode, const LsdbAppInfoPtr *appInfo,
				     CommonGuiDialogHandler *handler,
				     const wchar_t *msgArgA, const wchar_t *msgArgB,
				     const wchar_t *msgArgC, const wchar_t *msgArgD,
				     const wchar_t *msgArgE);
int topmenuShowAppError(SceInt32 errorCode, const LsdbAppInfoPtr *appInfo,
			CommonGuiDialogHandler *handler,
			const wchar_t *msgArgA, const wchar_t *msgArgB,
			const wchar_t *msgArgC, const wchar_t *msgArgD,
			const wchar_t *msgArgE);
void topmenuShowPowerHoldErrorDialog(SceInt32 errorCode);

/* Batch 3-5 cross-TU hooks (real signatures verified in IDA) */
TopmenuSurfacePoolSlot *topmenuAcquireSurfacePool(bool highPriority);		/* 0x81275CF4 */
void topmenuReleaseSurfacePool(TopmenuSurfacePoolSlot *slot);			/* 0x81275D32 */
void topmenuOnTabDestroyed(TopWindow *window, TopTab *tab);			/* 0x81279F06 */
bool topmenuHasRunningAppWithTitleId(TopWindow *window, const char *titleId);	/* 0x8127A9AC */
SceUInt64 topmenuGetAppFlags(TopWindow *window, const char *titleId);		/* 0x81270BD6 */
void topmenuSetHoveredTab(TopTab *tab);						/* 0x8127B864 */
void topmenuFocusHoveredTab(TopTab *tab);					/* 0x8127B89C */
void topmenuSelectHeaderTab(TopTab *tab);					/* 0x812730B8 */
bool topmenuCanPromoteApp(TopWindow *window, int checkMode, bool ignoreActive, bool force); /* 0x8127A688 */
ApplauncherInterface *topmenuGetApplauncherInterface(void);			/* 0x8126F976 */
LiveAreaPluginInterface *topmenuGetLiveAreaInterface(void);			/* 0x81270644 */
void topmenuSetGateEffectSurface(bool reverseEffect,
				 const paf::intrusive_ptr<paf::graph::Surface> *surface,
				 float originX, float originY); /* 0x8127B8DC */
bool topmenuIsLaunchSeqActive(const TopmenuLaunchSeq *seq);			/* 0x81271E3C */
void topmenuSetPageTurnDragging(TopWindow *window, bool dragging);		/* 0x8127A396 */
void topmenuCloseTab(TopTab *tab, bool animate);				/* 0x812701BA */
void topmenuCloseTabWithNext(TopTab *tab, bool animate, TopTabBase ***outNextIter); /* 0x812701BA */
void topmenuSetShellViewMode(TopWindow *window, SceInt32 viewMode);		/* 0x81270968 */
void topmenuSuspendBgPartners(void);						/* 0x8127B420 */
void topmenuResumeOrDestroyBgPartners(SceInt32 parentAppId);			/* 0x8127B472 */
void topmenuNotifyLaunchSeqReady(SceInt32 pageKey);				/* 0x8127C7B6 */
void topmenuNotifyForegroundReady(TopmenuAppProcess *proc);			/* 0x8127C7CE */
SceInt32 topmenuGetPsnSignInState(void);					/* 0x81281424 */
bool topmenuIsPowerSuspendNormal(void);						/* 0x8128138E */
TopTab *topmenuFindDebuggerTab(TopWindow *window);				/* 0x8127A3DA */
TopTab *topmenuFindTabByTitleId(TopWindow *window, const char *titleId);	/* 0x8127813C */
void topmenuTriggerLiveAreaUpdate(SceInt32 pageKey);				/* 0x812738F2 */
void topmenuRegisterSystemAppId(TopWindow *window, SceInt32 appId);		/* 0x812772AE */
void topmenuOnAppInfoSearchDone(const paf::string *titleId, const LsdbAppInfoPtr *appInfo,
				SceInt32 launchMode);				/* 0x8127A9F2 */
void topmenuOnUriSearchDone(SceInt32 result, const char *titleId, SceInt32 launchAppId); /* 0x8127AC1E */
void topmenuCancelLiveAreaOpen(SceInt32 pageKey);				/* 0x812766F6 */
void topmenuDeleteTabByPageKey(SceInt32 pageKey);				/* 0x8127EF96 */
void topmenuSetDefaultBgStyle(int styleKind, const TopmenuBgLayerOption *opt, bool animate); /* 0x81270738 */
TopmenuAppProcess *topmenuFindProcessByAppId(TopWindow *window, SceInt32 appId); /* 0x8127A784 */
void topmenuScrollToTabPageKey(SceInt32 pageKey);				/* 0x812737FE */
SceInt32 topmenuGetFocusedTabPageKey(TopWindow *window);			/* 0x81285A3C */

/* Cross-subsystem shell helpers used by top_window.cpp */
struct TopmenuMusicStatus {
	SceInt32 state;
	SceInt32 eventType;
	SceInt32 errorCode;
	SceUInt8 playMode;
	SceUInt8 padPlayMode[3];
	SceInt64 duration;
	SceInt32 posLo;
	SceInt32 posHi;
	SceInt32 sampleRate;
	SceInt32 bitrate;
	SceInt32 channels;
	SceInt32 codec;
	SceInt32 trackIndex;
	SceInt32 trackTotal;
	SceInt32 artistId;
	SceInt32 albumId;
	SceUInt8 repeatMode;
	SceUInt8 shuffleMode;
	SceUInt8 eqMode;
	SceUInt8 avlsMode;
	SceInt32 mediaType;
};
typedef char TopmenuMusicStatusSizeCheck[sizeof(TopmenuMusicStatus) == 72 ? 1 : -1];

bool shellActivityIsEnabled(void);						/* 0x81326D30 */
int parentalGetRemainingSec(void);						/* 0x812327F8 */
int shellParentalControlPause(void);						/* 0x81232834 */
int shellParentalControlResume(void);						/* 0x8123284E */
int musicCoreBgmLoadUri(const char *uri, const SceUInt8 *opt);			/* 0x811CAE46 */
int musicCoreBgmControl(SceUInt8 cmd, int param);				/* 0x811CB03A */
struct MusicCorePublicStatus;
int musicCoreBgmGetStatus(MusicCorePublicStatus *outStatus);			/* 0x811CB342 */
int musicCorePlayerControl(SceUInt8 cmd, int param);				/* 0x811CBDE6 */
typedef void (*LsdbJobCallback)(void *job, void *userData);
struct LsdbGetAppInfoJobResult : public paf::job::JobItem {
	LsdbJobCallback m_cb;
	void *m_userData;
	int m_result;
	SceUInt8 m_done;
	paf::string m_titleId;
	LsdbAppInfoPtr m_appInfo;
};
int lsdbEnqueueGetAppInfoJob(paf::common::SharedPtr<paf::job::JobItem> *out,
			     paf::job::JobQueue *queue, const paf::string &titleId,
			     LsdbJobCallback cb, void *userData);		/* 0x811C5BA6 */

/* Process base and Batch 4 job helpers */
class TopmenuSpawnConfirmHandler : public CommonGuiDialogHandler {
public:
	TopmenuSpawnConfirmHandler(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
				   SceInt32 existingFlags, SceInt32 extraFlags);
	virtual ~TopmenuSpawnConfirmHandler();
	virtual void operator()(SceInt32 dialogId, SceInt32 button);

	void CancelLaunchAndReturnHome();
	void ResumePendingLaunch();

	LsdbAppInfoPtr m_appInfo;	/* +0x04 */
	SceInt32 m_launchAppId;		/* +0x0C */
	SceInt32 m_existingFlags;	/* +0x10 */
	SceInt32 m_extraFlags;		/* +0x14 */
};

typedef char TopmenuSpawnConfirmHandlerSizeCheck[sizeof(TopmenuSpawnConfirmHandler) == 24 ? 1 : -1];

void topmenuProcessListAdd(TopmenuProcessBase *proc);				/* 0x81281758 */
void topmenuProcessListRemove(TopmenuProcessBase *proc);			/* 0x8128178E */
TopmenuAppProcess *topmenuFindProcessByTitleId(TopWindow *window, const char *titleId); /* 0x8127AB9A */
TopmenuAppProcess *topmenuFindProcessByRunningAppId(TopWindow *window, SceInt32 appId); /* 0x8127A7FC */
void topmenuRecordLaunchedAppFlags(TopWindow *window, SceInt32 appId,
				   const char *appName, SceInt64 attribute);	/* 0x8127A874 */
bool topmenuIsBlockedLaunchTitle(const char *titleId);				/* 0x8127289E */
void topmenuSwitchToProcess(TopWindow *window, TopmenuAppProcess *proc, bool animate); /* 0x81272BAE */
void topmenuReturnToHomeIfActive(TopmenuAppProcess *proc);			/* 0x8127B8B6 */
void topmenuUpdateIduAfterAppFailure(TopWindow *window, TopTab *tab);		/* 0x8127A5EA */
int shellSvcActivityPostSystemSoftwareImpl(const char *titleId);		/* 0x811BD168 */

LsdbAppInfoPtr *topmenuProcessGetAppInfo(TopmenuProcessBase *proc);		/* 0x8129A16C */
void topmenuProcessSetState(TopmenuProcessBase *proc, SceInt32 state);		/* 0x8129A172 */
const char *topmenuProcessGetTitleId(const TopmenuProcessBase *proc);		/* 0x8129A1FA */
void topmenuProcessSetLiveAreaFocus(TopmenuProcessBase *proc, bool focused);	/* 0x8129A222 */
void topmenuProcessActivateLiveArea(TopmenuProcessBase *proc);			/* 0x8129A294 */
TopTabAppData *topmenuProcessGetTabAppData(const TopmenuProcessBase *proc);	/* 0x8129A3D8 */
void topmenuProcessResetAndCancel(TopmenuProcessBase *proc);			/* 0x8129A3E4 */
void topmenuProcessOnEnterViewport(TopmenuProcessBase *proc);			/* 0x8129A476 */
void topmenuProcessOnLeaveViewport(TopmenuProcessBase *proc);			/* 0x8129A510 */
int topmenuProcessCommand(TopmenuProcessBase *proc, SceInt32 command);		/* 0x8129A6FA */
int topmenuProcessFlushPendingAction(TopmenuProcessBase *proc);			/* 0x8129A7A4 */
void topmenuProcessClearSpawnJob(TopmenuProcessBase *proc);			/* 0x8129A7BE */
int topmenuProcessSpawn(TopmenuProcessBase *proc, SceInt32 startArg, SceInt32 resumeFlag); /* 0x8129A828 */
int topmenuProcessLaunchAsync(TopmenuProcessBase *proc, SceInt32 startArg);	/* 0x8129A966 */
int topmenuProcessAttachAppId(TopmenuProcessBase *proc, SceInt32 appId);	/* 0x8129AA7E */
int topmenuProcessPollSharedFb(TopmenuProcessBase *proc);			/* 0x8129AA9A */
bool topmenuProcessIsSpawnedOrRunning(const TopmenuProcessBase *proc);		/* 0x8129AAC8 */
SceInt32 topmenuProcessGetPendingLaunchId(const TopmenuProcessBase *proc);	/* 0x8129AB02 */
SceUID topmenuProcessGetPid(const TopmenuProcessBase *proc);			/* 0x8129A20C */
void topmenuCreateSpawnJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			   TopmenuProcessBase *proc, SceInt32 launchAppId,
			   SceInt32 checkFlags);				/* 0x8129BD9C */
int topmenuStartSpawnJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			 TopmenuProcessBase *proc, SceInt32 launchAppId,
			 SceInt32 checkFlags);					/* 0x8129BDFA */
int topmenuEnqueueExistingSpawnJob(paf::common::SharedPtr<paf::job::JobItem> *job); /* 0x8129BE88 */
SceInt32 topmenuSpawnJobGetLaunchAppId(const paf::job::JobItem *job);		/* 0x8129AB02 helper */
int topmenuStartSpawnBgJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			   const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId); /* 0x8129E818 */
int topmenuSpawnWaitPromoteReady(const paf::string *titleId);			/* 0x8129EC6A */
int topmenuSpawnHandlePromoteFailure(const paf::string *titleId, SceInt32 retryCount); /* 0x8129EAF8 */
void topmenuSetPromoteWaitLock(bool locked, const char *titleId);		/* 0x8129EEF8 */
void topmenuSpawnOnInstallNoticeEvent(int code, const void *data, SceSize size, void *arg); /* 0x8129EF5C */
void topmenuStartPsmUpdateJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			      const paf::string *titleId, SceInt32 pageKey);	/* 0x8129B436 */
void topmenuStartUpdateAddcontLicenseJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
					 const char *titleId, SceInt32 launchAppId,
					 SceInt32 existingFlags, SceInt32 extraFlags); /* 0x8129F682 */
void topmenuStartPsnSignInDialogJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				    TopmenuAppProcess *proc);			/* 0x8129FC14 */
bool topmenuHasPendingFttUpdateTask(const paf::string *titleId);		/* 0x8129FF40 */
void topmenuStartUpdateFttLicenseJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				     TopmenuAppProcess *proc,
				     const LsdbAppInfoPtr *appInfo);		/* 0x812A03AE */
void topmenuOnLicenseDialogTabLeave(SceInt32 eventType, void *sender, SceInt32 eventArg,
				    void *userData);				/* 0x812A053E */
void topmenuStartUpdateLicenseJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				  const LsdbAppInfoPtr *appInfo);		/* 0x812A0AEA */
void topmenuStartRemoveSavedataJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				   const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
				   SceInt32 existingFlags, SceInt32 extraFlags); /* 0x812A122C */
int topmenuShowOnlineStorageDeletePrompt(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
					 SceInt32 existingFlags, SceInt32 extraFlags,
					 const paf::LocalFileStat *saveStat,
					 SceInt32 errorCode);			/* 0x812A1D82 */
bool bgdlResolvePspInstallTitleId(const LsdbAppInfoPtr *appInfo, paf::string *outTitleId); /* 0x81113900 */

/* Batch 3 cross-TU APIs */
int topmenuStartAppInfoSearch(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			      const char *titleId, const char *altTitleIds,
			      SceInt32 launchMode);				/* 0x81286D74 */
const char *topmenuAppProcessGetTitleId(const TopmenuAppProcess *proc);		/* 0x81287190 */
bool topmenuAppProcessIsRunning(const TopmenuAppProcess *proc);			/* 0x8128720C */
void topmenuAppProcessOnEnterViewport(TopmenuAppProcess *proc);			/* 0x812872DC */
void topmenuAppProcessOnLeaveViewport(TopmenuAppProcess *proc);			/* 0x812872E4 */
void topmenuAppProcessCancel(TopmenuAppProcess *proc);				/* 0x812874F2 */
int topmenuStartAppCheckJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			    TopmenuAppProcess *proc);				/* 0x812888A2 */
void topmenuCloseDialogAndDeleteTab(SceInt32 dialogId, SceInt32 unusedFlag, SceInt32 pageKey); /* 0x81289B0A */
void topmenuInstallErrorFill(SceInt32 errorCode, const BgdlTaskInfo *taskInfo,
			     TopmenuInstallError *outError);			/* 0x8128A1F8 */
void topmenuInstallErrorShowDialog(const TopmenuInstallError *errorInfo);	/* 0x8128ACC8 */
void topmenuExecCheckInit(void);						/* 0x8128B44A */
void topmenuExecCheckTerm(void);						/* 0x8128B4D2 */
bool topmenuResolvePromoteTitleId(const LsdbAppInfoPtr *appInfo, paf::string *outTitleId); /* 0x8128B506 */
void topmenuSysupchkCancelPowerKey(void *userData);				/* 0x8128B6A8 */
void topmenuSysupchkCancelTabLeave(SceInt32 eventType, void *sender, SceInt32 eventArg,
				   void *userData);				/* 0x8128B700 */
void topmenuSysupchkDoneCallback(SceInt32 result, void *userData);		/* 0x8128B758 */
int topmenuCheckVitaAppExec(const LsdbAppInfoPtr *appInfo, SceUInt8 checkFlags); /* 0x8128C8EC */
int topmenuCheckPspAppExec(const LsdbAppInfoPtr *appInfo, SceUInt32 checkFlags); /* 0x8128CAF2 */
int topmenuCheckPsmAppExec(const LsdbAppInfoPtr *appInfo);			/* 0x8128CFC2 */
int topmenuCheckPsmRuntimeExec(const LsdbAppInfoPtr *appInfo);			/* 0x8128D30E */
void topmenuEnqueueInstallJob(paf::common::SharedPtr<paf::job::JobItem> *job);	/* 0x8128D510 */
int topmenuStartUriSearch(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			  const char *uri, SceInt32 launchAppId);		/* 0x8128D64C */
int topmenuStartUriSearchOrFastPath(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				    const char *uri, SceInt32 launchAppId);	/* 0x8128E0F6 */
int topmenuParsePsgmUriScheme(const char *uri);					/* 0x8128D932 */
int topmenuParsePsgmTitleId(const char *uri, char *outBuf, SceSize bufSize);	/* 0x8128D984 */
int topmenuParsePsgmNpCommId(const char *uri, char *outBuf, SceSize bufSize);	/* 0x8128D9FC */
int topmenuParsePsgmContent(const char *uri, char *outBuf, SceSize bufSize);	/* 0x8128DA74 */
int topmenuParsePsgmType(const char *uri, char *outBuf, SceSize bufSize);	/* 0x8128DAEC */
int topmenuParsePsgmReqVer(const char *uri, char *outBuf, SceSize bufSize);	/* 0x8128DB64 */
int topmenuParsePsgmTitleName(const char *uri, char *outBuf, SceSize bufSize);	/* 0x8128DBDC */
int topmenuParsePsgmAltTitleIds(const char *uri, char *outBuf, SceSize bufSize); /* 0x8128DCD8 */
int topmenuParsePsgmCombinedTitleIds(const char *uri, char *outBuf, SceSize bufSize); /* 0x8128DDA8 */
int topmenuUriMatchesSupportList(const char *uri, const LsdbAppInfoPtr *appInfo); /* 0x8128DED8 */
void topmenuBootScreenClearImage(void);						/* 0x8128F484 */
void topmenuBootScreenShow(const char *splashPath, TopmenuAppProcess *proc);	/* 0x8128F496 */
void topmenuBootScreenHide(void);						/* 0x8128F4AE */
void topmenuBootScreenCaptureClose(TopmenuAppProcess *proc);			/* 0x8128F4C0 */
paf::intrusive_ptr<paf::graph::Surface> *topmenuBootScreenGetSurfacePtr(void);	/* 0x8128F4D6 */
int topmenuLaunchPendingCrashReport(void);					/* 0x8128FFCC */
int topmenuHandleAppCrash(const TopmenuCrashEvent *crashEvent);			/* 0x81290A60 */
void topmenuCloseCrashDialogForApp(SceInt32 appId);				/* 0x812917C4 */
bool topmenuHasPendingCrashReport(void);					/* 0x8129181A */
int topmenuProcessCrashReportOnBoot(void);					/* 0x81291FE0 */
void topmenuCoredumpInit(void);							/* 0x812921BC */

/* TopWindow hold/busy and app-launch helpers called by Batch 1 TUs */
void topmenuBeginGameHold(void);
void topmenuEndGameHold(void);
void topmenuBeginBusy(void);
void topmenuEndBusy(void);
void topmenuDismissIduOverlay(TopWindow *window);
void topmenuReturnFromIduDemoApp(TopWindow *window);
void topmenuOnNpCommIdFound(const paf::string *titleId, const LsdbAppInfoPtr *appInfo,
			    int launchAppId);
void topmenuCancelPendingAppLaunch(int launchAppId);
int topmenuShowAppErrorDialog(int errorCode, int appId, int optionFlag, int extraFlag);

/* NetCtl active reference count helpers (vsh/app/shell/src/netctl/netctl_api.cpp) */
int netCtlApiIncActiveRef(void);
int netCtlApiDecActiveRef(void);

/* Shell plugin lookup helper (applauncher / shell) */
void *topmenuFindPluginInterface(const char *pluginName, int interfaceId);

/* Livespace DB client lifecycle (0x811C4BF8 / 0x811C4C24) */
int shellLsdbClientCreate(void);
void shellLsdbClientDestroy(void);

/* import_savedata_plugin entry points (0x81393628..0x813936B4) */
void importSavedataPluginSetParam(paf::Plugin::InitParam2 *param);
void importSavedataPluginInit(paf::Plugin *plugin);
void importSavedataPluginStart(paf::Plugin *plugin);
void importSavedataPluginStop(paf::Plugin *plugin);
void importSavedataPluginExit(paf::Plugin *plugin);

/* Storage free-space helper (0x8137E308) */
int gameIoGetDeviceSpace(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize);

#endif
