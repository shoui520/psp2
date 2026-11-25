/*
 * power_manage_internal.h: internal classes and helpers of power_manage_plugin.
 * 3.65-CEX: 0x81234950 - 0x81241220
 */
#ifndef POWER_MANAGE_INTERNAL_H
#define POWER_MANAGE_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <std/map>
#include <std/memory>
#include <common/shared_ptr.h>
#include <common/job_slots.h>
#include <datetime/datetime.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <widget/w_pageturn.h>
#include <widget/w_supplier.h>
#include <widget/w_text.h>
#include <widget/w_button.h>
#include <widget/w_image_button.h>
#include <widget/w_plane_button.h>
#include "shell.h"
#include "plugins.h"
#include "src/livespace_db/livespace_db_internal.h"

struct CommonGuiDialogLayout;
class CommonGuiLayoutDialogParam;

namespace power_manage {

enum ScreenState {
	SCREEN_STATE_KEYLOCK = 0,
	SCREEN_STATE_PASSCODE = 1,
	SCREEN_STATE_UNLOCKED = 2
};

enum AuthStoreKind {
	AUTH_STORE_PARENTAL_DIALOG = 0,
	AUTH_STORE_PARENTAL_OVERLAY = 1,
	AUTH_STORE_SCREEN_LOCK_DIALOG = 2,
	AUTH_STORE_SCREEN_LOCK_SCREEN = 3
};

enum AuthActionKind {
	AUTH_ACTION_SET = 0,
	AUTH_ACTION_CHANGE = 1,
	AUTH_ACTION_VERIFY = 2
};

enum AuthViewEvent {
	AUTH_VIEW_EVENT_SUBMITTED = 0,
	AUTH_VIEW_EVENT_CANCELED = 1,
	AUTH_VIEW_EVENT_ABORTED = 2,
	AUTH_VIEW_EVENT_TIMEOUT = 3,
	AUTH_VIEW_EVENT_LOCKED_OUT = 4,
	AUTH_VIEW_EVENT_RESET = 5
};

enum AuthError {
	AUTH_OK = 0,
	AUTH_ERROR_FAILED = -1,
	AUTH_ERROR_INVALID = -2,
	AUTH_ERROR_NOT_SET = -3,
	AUTH_ERROR_LOCKED_OUT = -5
};

typedef void (*PowerEventCallback)(int eventCode, const void *eventBuf, SceSize eventSize,
				   void *userData);
typedef void (*PowerKeyCallback)(void *userData);
typedef void (*AuthResultCallback)(int result, void *userData);
typedef void (*AuthDialogOpenCallback)(void *userData);

struct AuthInterface {
	int (*startAuth)(SceInt16 storeKind, SceInt16 actionKind,
			 AuthResultCallback callback, void *userData,
			 AuthDialogOpenCallback openCallback);
	void (*cancelAuth)(void);
	int (*isPasscodeEnabled)(SceInt16 storeKind, bool *outEnabled);
	int (*reloadRegistry)(bool forceReload);
};

struct PowerManageInterface {
	void (*addPowerEventListener)(PowerEventCallback callback, void *userData);
	void (*removePowerEventListener)(PowerEventCallback callback, void *userData);
	void (*pushKeyHandler)(PowerKeyCallback callback, void *userData);
	void (*popKeyHandler)(PowerKeyCallback callback, void *userData);
	void (*reloadWallpaper)(void);
	void (*lockPsButton)(void);
	void (*unlockPsButton)(void);
	void (*lockPowerButton)(void);
	void (*unlockPowerButton)(void);
	void (*holdPowerSwitch)(void);
	void (*releasePowerSwitch)(void);
	void (*reservedHookA)(void);
	void (*reservedHookB)(void);
	void (*powerTickAndSleep)(void);
	void (*requestDisplayOff)(void);
	void (*onBootComplete)(void);
	void (*scheduleControllerCheck)(void);
	void (*unlockAndLaunchApp)(bool markWakeApp, const char *appName, const char *launchArg);
	void (*wakeAndLaunchForegroundApp)(const char *appName, const char *launchArg);
	void (*openPowerMenu)(void);
	void (*closePowerMenu)(void);
	paf::ui::Widget *(*getPage)(void);
	void (*setDolceLockEnabled)(bool enabled);
	int (*lockIfWakeApp)(const char *titleId);
	int (*isWakeApp)(const char *titleId);
	bool (*isSuspended)(void);
	AuthInterface *(*getAuthInterface)(void);
};

class PowerManageScreenBase {
public:
	virtual ~PowerManageScreenBase()
	{
	}

	virtual void OnResume() = 0;
	virtual void Unlock(bool skipPasscode) = 0;
	virtual bool IsDragging()
	{
		return false;
	}
};

struct PowerKeyHandlerEntry {
	PowerKeyCallback callback;
	void *userData;
};

class PowerEventListenerEntry {
public:
	PowerEventListenerEntry(PowerEventCallback cb, void *userData)
		: m_callback(cb), m_userData(userData), m_pendingRemove(false)
	{
	}

	virtual ~PowerEventListenerEntry()
	{
	}

	virtual void Notify(int eventCode, const void *eventBuf, SceSize eventSize)
	{
		if (m_callback != NULL)
			m_callback(eventCode, eventBuf, eventSize, m_userData);
	}

	PowerEventCallback m_callback;
	void *m_userData;
	bool m_pendingRemove;
};

class PowerEventListenerList {
public:
	PowerEventListenerList() : m_dispatching(false)
	{
	}

	virtual ~PowerEventListenerList()
	{
		for (paf::list<PowerEventListenerEntry *>::iterator it = m_entries.begin();
		     it != m_entries.end(); ++it) {
			delete *it;
		}
		m_entries.clear();
	}

	paf::list<PowerEventListenerEntry *> m_entries;
	bool m_dispatching;
};

class PowerManageEventServer {
public:
	PowerManageEventServer();
	virtual ~PowerManageEventServer();

	void AddListener(int eventCode, PowerEventCallback callback, void *userData);
	void RemoveListener(int eventCode, PowerEventCallback callback, void *userData);
	void Dispatch(int eventCode);

	void PushKeyHandler(PowerKeyCallback callback, void *userData);
	void PopKeyHandler(PowerKeyCallback callback, void *userData);

	static void OnDispatchKeyHandler(int eventCode, const void *eventBuf,
					 SceSize eventSize, void *userData);

	paf::map<int, PowerEventListenerList *> m_listeners;
	paf::vector<PowerKeyHandlerEntry> m_keyStack;
};

class KeyLockBlankPage {
public:
	KeyLockBlankPage();

	static void Open();
	static void Close();
	static bool IsOpen();

	paf::ui::Widget *m_page;
};

class KeyLockWallpaperMgr {
public:
	KeyLockWallpaperMgr();
	~KeyLockWallpaperMgr();

	void Reload();
	paf::intrusive_ptr<paf::graph::Surface> GetSurface(int *outWidth, int *outHeight);
	bool IsLoading() const;

	paf::intrusive_ptr<paf::graph::Surface> m_surface;
	int m_texWidth;
	int m_texHeight;
	int m_loadingCount;
};

class PowerMenu {
public:
	PowerMenu();
	~PowerMenu();

	void Open();
	void Close();
	void UpdateLayout();
	void UpdateHintText(SceUInt32 messageHash);

	static void OnPowerOffPressed(int32_t eventType, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userData);
	static void OnCancelPressed(int32_t eventType, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userData);
	static void OnStandbyPressed(int32_t eventType, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userData);
	static void OnPowerOffLayout(int32_t eventType, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userData);
	static void OnKeyCancel(void *userData);

	paf::ui::Widget *m_root;
	paf::ui::ImageButton *m_powerOffButton;
	paf::ui::Button *m_cancelButton;
	paf::ui::Button *m_standbyButton;
	paf::ui::PlaneButton *m_backdropButton;
	paf::ui::Text *m_hintText;
};

class KeyLockScreen : public PowerManageScreenBase {
public:
	KeyLockScreen(paf::Plugin *plugin);
	virtual ~KeyLockScreen();

	virtual void OnResume();
	virtual void Unlock(bool skipPasscode);
	virtual bool IsDragging();

	void ApplyWallpaper();
	void ClearWallpaper();
	void UpdateClock(bool animateFlip);
	void UpdateEventList(const paf::vector<LsdbNewEventPtr> &events);
	int GetEventAt(int index, LsdbNewEvent *outEvent);

	static void OnDragStart(int32_t eventType, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userData);
	static void OnDragCancel(int32_t eventType, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userData);
	static void OnPageTurnCompleted(int32_t eventType, paf::ui::Handler *handler,
					paf::ui::Event *event, void *userData);
	static void OnUnlockTransitionEnd(int32_t eventType, paf::ui::Handler *handler,
					  paf::ui::Event *event, void *userData);
	static void OnCheckPasscodeBeforeUnlock(int32_t eventType, paf::ui::Handler *handler,
						paf::ui::Event *event, void *userData);
	static void OnClockTimer(int32_t eventType, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userData);
	static void OnMinuteFlipEnd(paf::Timer *timer, paf::ui::AnimListener *listener,
				    void *userData, void *digitValue, void *unused);
	static void OnDigitFlipEnd(paf::Timer *timer, paf::ui::AnimListener *listener,
				   void *userData, void *digitValue, void *unused);
	static void OnMainThreadTick(void *userData);
	static void OnEventListLoaded(int result, const paf::vector<LsdbNewEventPtr> *events);
	static void OnShellEvent(int eventCode, const void *eventBuf, SceSize eventSize,
				 void *userData);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_page;
	paf::ui::Widget *m_clockRoot;
	paf::ui::Text *m_dateText;
	paf::ui::Text *m_hourText;
	paf::ui::Text *m_minuteText;
	paf::ui::Widget *m_eventPanel;
	paf::ui::Widget *m_guideWidget;
	paf::ui::PageTurn *m_pageTurn;
	paf::ui::Supplier *m_touchSupplier;
	bool m_wallpaperReady;
	bool m_isDragging;
	bool m_pendingDisplayOn;
	bool m_skipPasscode;
	bool m_themeTestVisible;
	paf::common::SharedPtr<LsdbThemeInfo> m_themeInfo;
	int m_timeFormat;
	paf::vector<LsdbNewEventPtr> m_events;
	paf::datetime::DateTime m_lastDateTime;
	SceUInt64 m_lockStartTick;
};

class PasscodeLockScreen : public PowerManageScreenBase {
public:
	PasscodeLockScreen(paf::Plugin *plugin);
	virtual ~PasscodeLockScreen();

	virtual void OnResume()
	{
	}

	virtual void Unlock(bool skipPasscode);

	static void OnLockoutPluginLoaded(paf::Plugin *plugin);
	static void OnAuthFinished(int result, void *userData);
	static void OnLockoutFinished(int result, void *userData);

	paf::Plugin *m_plugin;
};

class PowerManage {
public:
	PowerManage(paf::Plugin *plugin);
	~PowerManage();

	void SetIduTimerEnabled(bool enabled);
	void SetSuspendedState(bool suspended);
	void ShowImposeNotice(SceUInt32 messageHash, const wchar_t *formatArg);
	void OpenStatusDialog(const CommonGuiDialogLayout &layout, SceUInt32 messageHash,
			      SceUInt32 textureHash, const paf::math::v4 &iconPos);
	void UnlockToTopMenu();

	static void SetScreenState(int state);
	static void SetPowerSaveMode(bool enablePowerSave, bool suspendApps);
	static int IsWakeAppName(const char *titleId);
	static void TurnDisplayOnAndNotify();

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_page;
	int m_currentState;
	int m_targetState;
	PowerManageScreenBase *m_currentScreen;
	PowerManageEventServer m_eventServer;
	KeyLockWallpaperMgr m_wallpaperMgr;
	KeyLockBlankPage m_blankPage;
	int m_psButtonLockCount;
	int m_powerButtonLockCount;
	bool m_isSuspended;
	bool m_iduTimerPaused;
	SceUInt32 m_lowBatteryControllerMask;
	bool m_netConnected;
	bool m_dolceLockEnabled;
	bool m_pendingResumeDisplayOn;
	bool m_powerSaveMode;
	bool m_noControllerNoticeShown;
	SceUInt64 m_lastResumeTime;
	int m_statusDialogId;
	paf::string m_wakeAppName;
	paf::string m_pendingLaunchApp;
	paf::string m_pendingLaunchArg;
	int m_powerSwHoldCount;
	PowerMenu m_powerMenu;
	SceUID m_powerCallbackId;
	SceUID m_usbCallbackId;
	SceUID m_ctrlCallbackId;
	int m_netCtlCallbackId;
};

extern PowerManage *g_powerManage;
extern paf::Plugin *g_powerManagePlugin;
extern KeyLockScreen *g_keyLockScreen;
extern KeyLockWallpaperMgr *g_keyLockWallpaperMgr;
extern KeyLockBlankPage *g_keyLockBlankPage;
extern PasscodeLockScreen *g_passcodeLockScreen;
extern PowerMenu *g_powerMenu;

/* Auth subsystem entrypoints (power_manage_auth.cpp) */
int authServiceCreate();
int authServiceDestroy();
int authServiceReloadRegistry(bool forceReload);
bool authServiceIsRegistryLoaded();
int authServiceIsPasscodeEnabled(SceInt16 storeKind, bool *outEnabled);
int authServiceStart(SceInt16 storeKind, SceInt16 actionKind,
		     AuthResultCallback callback, void *userData,
		     AuthDialogOpenCallback openCallback);
void authServiceCancel();

/* Plugin interface helper wrappers used across power_manage_plugin TUs */
void powerManagePushKeyHandler(PowerKeyCallback callback, void *userData);
void powerManagePopKeyHandler(PowerKeyCallback callback, void *userData);
void powerManageLockPsButton();
void powerManageUnlockPsButton();
void powerManageRequestDisplayOff();

} /* namespace power_manage */

/* Cross-module shell helpers */
extern int shellParentalControlUpdate(bool isActive, bool forceSave);
extern void shellRegMgrLogError(int errorCode, int showErrorDialog);
extern int shellShutdownRequest(int mode, int flags);
extern int shellStateWatcherOpenDialog(CommonGuiLayoutDialogParam *param, bool highPriority,
				       int category);
extern int shellStateWatcherCloseDialog(int dialogId);
extern int shellConsoleInfoRecordSuspendDuration(int eventKind, SceInt64 durationUsec);
extern int shellNpPushResumeNotify(SceUID processId, int flags);
extern int shellExclusiveFuncPasscodeLockoutStart(void (*loadedCb)(paf::Plugin *),
						  void *userData, int flags);
extern int shellExclusiveFuncPasscodeLockoutStop(void);

#endif /* POWER_MANAGE_INTERNAL_H */
