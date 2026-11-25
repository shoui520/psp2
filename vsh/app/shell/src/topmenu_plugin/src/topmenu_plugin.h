/*
 * topmenu_plugin.h: public plugin entry points and GetInterface(1) table
 * for SceShell's topmenu_plugin (0x8126F718-0x812A25FF).
 */
#ifndef SHELL_TOPMENU_PLUGIN_H
#define SHELL_TOPMENU_PLUGIN_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <std/memory>
#include <graphics/surface/surface.h>
#include <cache/cache_manager.h>
#include <job/job.h>
#include <widget/core/widget.h>
#include "../../livespace_db/livespace_db_internal.h"

class CommonGuiMessageDialogParam;
struct LiveAreaPluginInterface;
struct ApplauncherInterface;

/* Running/open application status entry returned by getRunningApplications (24 bytes) */
struct TopmenuRunningAppInfo {
	char titleId[12];
	SceInt32 launchState;		/* 0: not running, 1: active, 2: suspended */
	SceUID processId;
	SceInt32 appId;
};

/* Background layer appearance requested via pushBackgroundLayer (12 bytes) */
struct TopmenuBgLayerOption {
	bool useCustomColor;
	SceInt32 style;
	SceUInt32 color;
};

typedef void (*TopmenuPageEventCallback)(SceInt32 eventType, SceInt32 pageKey, void *userData);
typedef void (*TopmenuWindowEventCallback)(SceInt32 eventType, void *window, SceInt32 pageKey,
					   void *userData);
typedef void (*TopmenuPromoteCallback)(SceInt32 result, void *userData);

/*
 * topmenu_plugin GetInterface(1) function table (89 slots, 356 = 0x164 bytes at 0x81545740).
 */
struct TopmenuPluginInterface {
	/* 0x000..0x020: page creation, app page opening, surface & cache accessors */
	int (*createCustomPage)(int afterPageKey, paf::intrusive_ptr<paf::graph::Surface> *bgSurface);
	int (*openAppPage)(const paf::string *titleId, int *inOutPageKey,
			   paf::intrusive_ptr<paf::graph::Surface> *iconSurface, int launchAppId);
	void (*setPageSurfaceByTitleId)(const char *const *titleId,
					paf::intrusive_ptr<paf::graph::Surface> *surface);
	void (*closePage)(int pageKey);
	paf::ui::Widget *(*getPageWidget)(int pageKey);
	paf::ui::Widget *(*getPageRootWidget)(int pageKey);
	paf::cache::CacheManager *(*getPageCacheManager)(int pageKey);
	paf::graph::SurfacePool *(*getPageSurfacePool)(int pageKey);
	paf::intrusive_ptr<paf::graph::Surface> *(*getDefaultBgTexture)(void);

	/* 0x024..0x034: page enumeration and job queue */
	int (*getCurrentPageKey)(void);
	int (*getPageIndex)(int pageKey);
	int (*getPageKeyAt)(int index);
	int (*findPageKeyByTitleId)(const char *titleId);
	paf::job::JobQueue *(*getJobQueue)(void);

	/* 0x038..0x044: LiveArea action bar icon click handlers */
	void (*onManualIconClick)(int pageKey);
	void (*onPatchIconClick)(int pageKey);
	void (*onImportSavedataIconClick)(int pageKey);
	void (*onOnlineStorageIconClick)(int pageKey);

	/* 0x048..0x068: page event listeners, focus & intro transitions */
	void (*addPageCloseListener)(int pageKey, TopmenuPageEventCallback callback, void *userData);
	void (*removePageCloseListener)(int pageKey, TopmenuPageEventCallback callback,
					void *userData);
	void (*scrollToPage)(int pageKey);
	void (*showPageContent)(int pageKey, float duration);
	void (*cancelPendingPageTransition)(void);
	void (*completePageTransition)(int pageKey);
	void (*hidePageContent)(int pageKey, float duration);
	void (*addPageShowListener)(int pageKey, TopmenuPageEventCallback callback, void *userData);
	void (*addPageHideListener)(int pageKey, TopmenuPageEventCallback callback, void *userData);

	/* 0x06C..0x098: home navigation, edit mode, hold/busy reference counts */
	int (*requestReturnToHome)(void);
	int (*requestReturnToHomeImmediate)(void);
	void (*enterEditMode)(void);
	void (*leaveEditMode)(void);
	void (*beginGameHold)(void);
	void (*endGameHold)(void);
	void (*beginHold)(void);
	void (*endHold)(void);
	void (*beginBusy)(void);
	void (*endBusy)(void);
	int (*isBusy)(void);
	void (*setPendingLaunchParam)(int param);

	/* 0x09C..0x0CC: gate interaction, LiveArea state, foreground & plane creation */
	void (*onGateClick)(int pageKey);
	int (*isPageInteractive)(int pageKey);
	int (*isPageSelected)(int pageKey);
	paf::ui::Widget *(*getPageTurnWidget)(int pageKey);
	void (*setPageScrolledDown)(int pageKey, bool scrolledDown);
	void (*notifyIntroComplete)(int pageKey);
	int (*notifyContentUpdated)(const char *const *updateArgs);
	const char *(*getCurrentPageTitleId)(void);
	void (*setPageMusicPlaying)(int pageKey, bool playing);
	int (*getPageMusicState)(int pageKey);
	int (*isTopmenuForeground)(void);
	void (*onInitialBootReady)(void);
	paf::ui::Widget *(*createPlane)(paf::ui::Widget *parent);

	/* 0x0D0..0x0EC: background layer stack, scroll lock, Store logo & running apps */
	void (*pushBackgroundLayer)(void *owner, int priority, bool useCustomColor, int style,
				    SceUInt32 color);
	void (*popBackgroundLayer)(void *owner, int priority);
	void (*setScrollEnabled)(bool enabled);
	int (*openPsStoreLogo)(int mode, int arg);
	int (*closePsStoreLogo)(void *arg);
	int (*isAppInForeground)(void);
	int (*isHomePageSelected)(void);
	int (*getRunningApplications)(paf::vector<TopmenuRunningAppInfo> *outApps);

	/* 0x0F0..0x124: cross-plugin registration, app lifecycle, error dialogs */
	void (*setApplauncherInterface)(ApplauncherInterface *iface);
	void (*setLiveAreaInterface)(LiveAreaPluginInterface *iface);
	void (*closePageByTitleId)(const char *titleId);
	void (*closeCurrentAppIfMatches)(const char *titleId);
	void (*reloadWhatsNew)(void);
	int (*startAppDataPromote)(const char *titleId, TopmenuPromoteCallback callback,
				   void *userData);
	void (*notifyUpStatusChanged)(const char *titleId, int upStatus);
	int (*openErrorDialog)(CommonGuiMessageDialogParam *param);
	int (*checkCrashReportFile)(void);
	int (*getAppInfoByTitleId)(LsdbAppInfoPtr *outInfo, const char *titleId);
	const char *(*getAppExecInfoByBgTitleId)(const char *bgTitleId, SceUInt64 *outTimestamp);
	void (*getOpenPageTitleIds)(paf::vector<paf::string> *outTitles);
	void (*registerDialogId)(int dialogId);
	void (*unregisterDialogId)(int dialogId);

	/* 0x128..0x160: app foreground/background, install lock, window listeners & boot */
	void (*enterAppForeground)(void);
	void (*leaveAppForeground)(bool animate, const char *titleId);
	int (*beginAppInstallLock)(const char *titleId);
	int (*endAppInstallLock)(const char *titleId, int relaunchMode);
	void (*addPageAddedListener)(TopmenuWindowEventCallback callback, void *userData);
	void (*removePageAddedListener)(TopmenuWindowEventCallback callback, void *userData);
	void (*addPageRemovedListener)(TopmenuWindowEventCallback callback, void *userData);
	void (*removePageRemovedListener)(TopmenuWindowEventCallback callback, void *userData);
	void (*addPageFocusListener)(TopmenuWindowEventCallback callback, void *userData);
	void (*removePageFocusListener)(TopmenuWindowEventCallback callback, void *userData);
	int (*getNetworkState)(void);
	void (*refreshAppInfoSync)(const char *titleId);
	int (*getOpenAppPageCount)(void);
	void (*reloadSystemBgm)(void);
	void (*initLiveboardSurfacePools)(void);
};

typedef char TopmenuPluginInterfaceSizeCheck[sizeof(TopmenuPluginInterface) == 0x164 ? 1 : -1];

void topmenuPluginSetParam(paf::Plugin::InitParam2 *param);
void topmenuPluginInit(paf::Plugin *plugin);
void topmenuPluginStart(paf::Plugin *plugin);
void topmenuPluginStop(paf::Plugin *plugin);
void topmenuPluginExit(paf::Plugin *plugin);

#endif
