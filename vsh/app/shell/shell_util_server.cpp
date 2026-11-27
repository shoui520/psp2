/*
 * shell_util_server.cpp: SceShell's ShellUtilServerMgr & IPMI 0x000C0000..0x000C001E dispatch
 * (0x812499F6-0x8124D61B).
 *
 * Handles synchronous and asynchronous SceShellUtil requests: system locks, locale and date/time
 * format changes, app launching (by name and by URI with PS Store fallback dialog), indicator
 * toggles, standby/cold-reset shutdown orchestration, error dialogs, theme change/delete,
 * parental remaining time, and console usage counters.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/list>
#include <common/shared_ptr.h>
#include <framework.h>
#include <plugin.h>
#include <job/job.h>
#include <thread/thread.h>
#include <datetime/datetime.h>
#include <inputdevice/inputdevice.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>
#include <versatileparam/idparam.h>
#include <kernel/processmgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <ipmi/session.h>

#include "shell.h"
#include "plugins.h"
#include "theme.h"
#include "bgdl_api.h"
#include "shell_util_server.h"
#include "src/power_manage_plugin/src/power_manage_internal.h"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

extern "C" {
extern int scePowerRequestStandby(void);
extern int scePowerCancelRequest(void);
extern int vshPowerRequestColdReset(void);
extern int SceLsdb_C6678B98(int language);
extern int SceLsdb_6AFF7E71(void);
extern int SceLsdb_E2838878(AppInfoSClient *client, const char *uri,
	paf::string *outTitleId, unsigned int flags);
extern int SceLsdb_87EE0526(AppInfoSClient *client, const paf::string *titleId,
	LsdbAppInfoPtr *outAppInfo);
extern bool SceLsdb_41019D89(const LsdbAppInfoPtr *appInfo);
}

/* External shell helpers in other TUs */
extern bool shellBootArePluginsLoaded(void);
static inline bool shellHasMediaInfoFeature(void)
{
	return shellBootArePluginsLoaded();
}
extern void shellReloadAccessibilityState(void);
extern void shellNetCtlTerm(void);
extern int musicCoreSetSystemPause(SceUInt8 pause);
extern int shellParentalControlUpdate(bool isActive, bool forceSave);
extern int parentalGetRemainingSec(void);
extern int shellTriggerMgrSuspend(void (*callback)(void *), void *userData);

namespace {

/* IPMI method IDs handled by ShellUtilServerMgr */
enum {
	SHELL_UTIL_METHOD_LOCK				= 0x000C0000,
	SHELL_UTIL_METHOD_UNLOCK			= 0x000C0001,
	SHELL_UTIL_METHOD_UNLOCK_ALL			= 0x000C0002,
	SHELL_UTIL_METHOD_SET_LOCALE			= 0x000C0003,
	SHELL_UTIL_METHOD_SET_DATETIME_FORMAT		= 0x000C0004,
	SHELL_UTIL_METHOD_LAUNCH_APP_BY_NAME		= 0x000C0005,
	SHELL_UTIL_METHOD_SET_CLOCK_VISIBLE		= 0x000C0006,
	SHELL_UTIL_METHOD_SET_STATUSBAR_VISIBLE		= 0x000C0007,
	SHELL_UTIL_METHOD_SET_MUSIC_PAUSE		= 0x000C0008,
	SHELL_UTIL_METHOD_REFRESH_LSDB			= 0x000C0009,
	SHELL_UTIL_METHOD_RELOAD_ACCESSIBILITY		= 0x000C000A,
	SHELL_UTIL_METHOD_REQUEST_STANDBY		= 0x000C000B,
	SHELL_UTIL_METHOD_REQUEST_COLD_RESET		= 0x000C000C,
	SHELL_UTIL_METHOD_SHOW_ERROR_DIALOG		= 0x000C000D,
	SHELL_UTIL_METHOD_NOTIFY_TOPMENU_UPDATE		= 0x000C000E,
	SHELL_UTIL_METHOD_RELOAD_WALLPAPER		= 0x000C000F,
	SHELL_UTIL_METHOD_RELOAD_KEYLOCK_WALLPAPER	= 0x000C0010,
	SHELL_UTIL_METHOD_SET_BATTERY_PERCENT		= 0x000C0011,
	SHELL_UTIL_METHOD_ACTIVATE_TITLE		= 0x000C0012,
	SHELL_UTIL_METHOD_SET_NOTIFICATION_ENABLED	= 0x000C0013,
	SHELL_UTIL_METHOD_SET_KEY_NAVIGATION		= 0x000C0014,
	SHELL_UTIL_METHOD_RELOAD_LIVEAREA		= 0x000C0015,
	SHELL_UTIL_METHOD_LAUNCH_APP_BY_URI		= 0x000C0016,
	SHELL_UTIL_METHOD_GET_PARENTAL_REMAINING	= 0x000C0017,
	SHELL_UTIL_METHOD_REFRESH_CALLER_APP		= 0x000C0018,
	SHELL_UTIL_METHOD_CHANGE_THEME			= 0x000C0019,
	SHELL_UTIL_METHOD_DELETE_THEME			= 0x000C001A,
	SHELL_UTIL_METHOD_CANCEL_TOPMENU_GUIDE		= 0x000C001B,
	SHELL_UTIL_METHOD_RESET_IMPOSE_STATUS		= 0x000C001C,
	SHELL_UTIL_METHOD_ADD_USAGE_COUNTER		= 0x000C001D,
	SHELL_UTIL_METHOD_ASYNC_RESULT			= 0x000C001E,
};

/* Internal request type discriminator (matches original virtual GetType() 1..29) */
enum ShellUtilRequestKind {
	SHELL_UTIL_REQ_LOCK				= 1,
	SHELL_UTIL_REQ_UNLOCK				= 2,
	SHELL_UTIL_REQ_UNLOCK_ALL			= 3,
	SHELL_UTIL_REQ_SET_LOCALE			= 4,
	SHELL_UTIL_REQ_SET_DATETIME_FORMAT		= 5,
	SHELL_UTIL_REQ_LAUNCH_APP_BY_NAME		= 6,
	SHELL_UTIL_REQ_SET_CLOCK_VISIBLE		= 7,
	SHELL_UTIL_REQ_SET_STATUSBAR_VISIBLE		= 8,
	SHELL_UTIL_REQ_SET_MUSIC_PAUSE			= 9,
	SHELL_UTIL_REQ_REFRESH_LSDB			= 10,
	SHELL_UTIL_REQ_RELOAD_ACCESSIBILITY		= 11,
	SHELL_UTIL_REQ_REQUEST_STANDBY			= 12,
	SHELL_UTIL_REQ_REQUEST_COLD_RESET		= 13,
	SHELL_UTIL_REQ_SHOW_ERROR_DIALOG		= 14,
	SHELL_UTIL_REQ_NOTIFY_TOPMENU_UPDATE		= 15,
	SHELL_UTIL_REQ_RELOAD_WALLPAPER			= 16,
	SHELL_UTIL_REQ_RELOAD_KEYLOCK_WALLPAPER		= 17,
	SHELL_UTIL_REQ_SET_BATTERY_PERCENT		= 18,
	SHELL_UTIL_REQ_ACTIVATE_TITLE			= 19,
	SHELL_UTIL_REQ_SET_NOTIFICATION_ENABLED		= 20,
	SHELL_UTIL_REQ_SET_KEY_NAVIGATION		= 21,
	SHELL_UTIL_REQ_RELOAD_LIVEAREA			= 22,
	SHELL_UTIL_REQ_LAUNCH_APP_BY_URI		= 23,
	SHELL_UTIL_REQ_REFRESH_CALLER_APP		= 24,
	SHELL_UTIL_REQ_CHANGE_THEME			= 25,
	SHELL_UTIL_REQ_DELETE_THEME			= 26,
	SHELL_UTIL_REQ_CANCEL_TOPMENU_GUIDE		= 27,
	SHELL_UTIL_REQ_RESET_IMPOSE_STATUS		= 28,
	SHELL_UTIL_REQ_ADD_USAGE_COUNTER		= 29,
};

/* Plugin interface slot indices (byte offset / 4) for plugins without full headers */
enum {
	TOPMENU_SLOT_NOTIFY_UPDATE			= 27,	/* +108 */
	TOPMENU_SLOT_CANCEL_GUIDE			= 28,	/* +112 */
	TOPMENU_SLOT_ACTIVATE_TITLE			= 62,	/* +248 */
	TOPMENU_SLOT_REFRESH_APP_STATUS			= 63,	/* +252 */
	TOPMENU_SLOT_RELOAD_LIVEAREA			= 86,	/* +344 */
	TOPMENU_SLOT_RELOAD_THEME			= 87,	/* +348 */

	APPLAUNCHER_SLOT_LAUNCH_BY_NAME			= 4,	/* +16 */
	APPLAUNCHER_SLOT_RELOAD_WALLPAPER		= 14,	/* +56 */
	APPLAUNCHER_SLOT_PREPARE_THEME			= 24,	/* +96 */
	APPLAUNCHER_SLOT_APPLY_THEME			= 25,	/* +100 */

	INDICATOR_SLOT_SET_CLOCK_VISIBLE		= 5,	/* +20 */
	INDICATOR_SLOT_SET_BATTERY_PERCENT		= 6,	/* +24 */
	INDICATOR_SLOT_SET_STATUSBAR_VISIBLE		= 7,	/* +28 */
	INDICATOR_SLOT_SET_NOTIFICATION_ENABLED		= 11,	/* +44 */
	INDICATOR_SLOT_RELOAD_WALLPAPER			= 26,	/* +104 */
	INDICATOR_SLOT_RELOAD_THEME			= 31,	/* +124 */

	IMPOSE_SLOT_RESET_STATUS			= 7,	/* +28 */
	IMPOSE_SLOT_RELOAD_THEME			= 24,	/* +96 */
	IMPOSE_SLOT_START_SHUTDOWN_FADE			= 26,	/* +104 */
};

/* Constants matching original ShellUtil behavior */
enum {
	SHELL_UTIL_MAX_LOCALE_COUNT			= 20,
	SHELL_UTIL_LAUNCH_MODE_BY_URI			= 0x00820000,
	SHELL_UTIL_LSDB_ERR_INVALID_URI			= -2146430975,	/* 0x80101001 */
	SHELL_UTIL_LSDB_ERR_URI_NOT_FOUND		= -2146430971,	/* 0x80101005 */
	SHELL_UTIL_INPUT_LOCK_ALL			= 0x7F,
	SHELL_UTIL_INPUT_OWNERSHIP_DIALOG		= 0x6F,
	SHELL_UTIL_URI_DIALOG_SOUND_ID			= -682994459,	/* 0xD74A3625 */
	SHELL_UTIL_URI_DIALOG_PRIORITY			= 35,
	SHELL_UTIL_MSG_URI_NOT_FOUND			= 890947911,
	SHELL_UTIL_MSG_PSCLOUD_STORE_PROMPT		= 259486978,
	SHELL_UTIL_MSG_PSCLOUD_BTN_STORE		= 735295558,
	SHELL_UTIL_MSG_PSCLOUD_BTN_CANCEL		= -1379034401,
	SHELL_UTIL_DIALOG_PART_CONFIRM_BTN		= 12,
	SHELL_UTIL_DIALOG_PART_CANCEL_BTN		= 13,
	SHELL_UTIL_COUNTER_BASE_ID			= 280,
	SHELL_UTIL_MAX_COUNTER_KIND			= 3,
	SHELL_UTIL_IPMI_ERR_UNSUPPORTED			= -2147351103,	/* 0x800205C1 */
};

static wstring s_defaultErrorMessage;

/* Base request object */
class ShellUtilRequest {
public:
	ShellUtilRequest() : m_session(NULL), m_requestId(-1) {}
	virtual ~ShellUtilRequest() {}
	virtual int GetType() const = 0;

	IPMI::Session *m_session;
	int m_requestId;
};

/* Base request carrying a single integer payload (16 bytes, matching Sony's subclasses) */
class ShellUtilIntRequest : public ShellUtilRequest {
public:
	ShellUtilIntRequest() : m_value(0) {}
	int m_value;
};

/* Base request carrying a single string payload (24 bytes, matching Sony's subclasses) */
class ShellUtilStringRequest : public ShellUtilRequest {
public:
	ShellUtilStringRequest() : m_text("") {}
	string m_text;
};

class ShellUtilLockRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_LOCK; }
};

class ShellUtilUnlockRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_UNLOCK; }
};

class ShellUtilUnlockAllRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_UNLOCK_ALL; }
};

class ShellUtilSetLocaleRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_SET_LOCALE; }
};

/* Type 5: SetDateTimeFormat */
class ShellUtilDateTimeFormatRequest : public ShellUtilRequest {
public:
	ShellUtilDateTimeFormatRequest() : m_dateFormat(L""), m_timeFormat(L"") {}
	virtual int GetType() const { return SHELL_UTIL_REQ_SET_DATETIME_FORMAT; }

	wstring m_dateFormat;
	wstring m_timeFormat;
};

class ShellUtilLaunchAppByNameRequest : public ShellUtilStringRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_LAUNCH_APP_BY_NAME; }
};

class ShellUtilSetClockVisibleRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_SET_CLOCK_VISIBLE; }
};

class ShellUtilSetStatusbarVisibleRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_SET_STATUSBAR_VISIBLE; }
};

class ShellUtilSetMusicPauseRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_SET_MUSIC_PAUSE; }
};

class ShellUtilRefreshLsdbRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_REFRESH_LSDB; }
};

class ShellUtilReloadAccessibilityRequest : public ShellUtilRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_RELOAD_ACCESSIBILITY; }
};

class ShellUtilRequestStandbyRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_REQUEST_STANDBY; }
};

class ShellUtilRequestColdResetRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_REQUEST_COLD_RESET; }
};

/* Type 14: ShowErrorDialog */
class ShellUtilErrorDialogRequest : public ShellUtilRequest {
public:
	ShellUtilErrorDialogRequest() : m_errorType(0), m_errorCode(0), m_flags(0) {}
	virtual int GetType() const { return SHELL_UTIL_REQ_SHOW_ERROR_DIALOG; }

	int m_errorType;
	int m_errorCode;
	int m_flags;
};

class ShellUtilNotifyTopmenuUpdateRequest : public ShellUtilRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_NOTIFY_TOPMENU_UPDATE; }
};

class ShellUtilReloadWallpaperRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_RELOAD_WALLPAPER; }
};

class ShellUtilReloadKeylockWallpaperRequest : public ShellUtilRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_RELOAD_KEYLOCK_WALLPAPER; }
};

class ShellUtilSetBatteryPercentRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_SET_BATTERY_PERCENT; }
};

class ShellUtilActivateTitleRequest : public ShellUtilStringRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_ACTIVATE_TITLE; }
};

class ShellUtilSetNotificationEnabledRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_SET_NOTIFICATION_ENABLED; }
};

class ShellUtilSetKeyNavigationRequest : public ShellUtilIntRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_SET_KEY_NAVIGATION; }
};

class ShellUtilReloadLiveareaRequest : public ShellUtilRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_RELOAD_LIVEAREA; }
};

/* Type 23: LaunchAppByUri */
class ShellUtilLaunchUriRequest : public ShellUtilRequest {
public:
	ShellUtilLaunchUriRequest() : m_uri(""), m_callerAppId(0), m_result(SHELL_UTIL_ERR_INVALID_PARAM) {}
	virtual int GetType() const { return SHELL_UTIL_REQ_LAUNCH_APP_BY_URI; }

	string m_uri;
	int m_callerAppId;
	int m_result;
};

class ShellUtilRefreshCallerAppRequest : public ShellUtilRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_REFRESH_CALLER_APP; }
};

/* Type 25 / 26: ThemeChange / ThemeDelete */
class ShellUtilThemeRequest : public ShellUtilRequest {
public:
	ShellUtilThemeRequest()
		: m_themePath(""), m_result(SHELL_UTIL_ERR_INVALID_PARAM) {}

	string m_themePath;
	int m_result;
};

class ShellUtilChangeThemeRequest : public ShellUtilThemeRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_CHANGE_THEME; }
};

class ShellUtilDeleteThemeRequest : public ShellUtilThemeRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_DELETE_THEME; }
};

class ShellUtilCancelTopmenuGuideRequest : public ShellUtilRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_CANCEL_TOPMENU_GUIDE; }
};

class ShellUtilResetImposeStatusRequest : public ShellUtilRequest {
public:
	virtual int GetType() const { return SHELL_UTIL_REQ_RESET_IMPOSE_STATUS; }
};

/* Type 29: AddUsageCounter */
class ShellUtilUsageCounterRequest : public ShellUtilRequest {
public:
	ShellUtilUsageCounterRequest()
		: m_counterKind(0), m_increment(0), m_result(SHELL_UTIL_ERR_INVALID_PARAM) {}
	virtual int GetType() const { return SHELL_UTIL_REQ_ADD_USAGE_COUNTER; }

	int m_counterKind;
	unsigned int m_increment;
	int m_result;
};

class ShellUtilServerMgr;
static ShellUtilServerMgr *s_utilServerMgr = NULL;

static void mainThreadExecuteTrampoline(void *requestPtr);
static void shutdownFadeDoneTrampoline(void *requestPtr);
static void onPowerEventCloseUriDialog(int eventCode, const void *eventBuf,
	SceSize eventSize, void *userData);

/* Background JobItem executing heavy ShellUtil requests off the main thread */
class ShellUtilWorkerJob : public job::JobItem {
public:
	explicit ShellUtilWorkerJob(ShellUtilRequest *request)
		: job::JobItem(NULL, NULL), m_request(request) {}
	virtual ~ShellUtilWorkerJob() {}

	virtual int Run();
	virtual void Finish(int result);

	ShellUtilRequest *m_request;
};

/* CommonGuiDialogHandler for URI-not-found / PS Store redirect prompt (0x8124D290) */
class ShellUtilUriDialogHandler : public CommonGuiDialogHandler {
public:
	explicit ShellUtilUriDialogHandler(ShellUtilRequest *request) : m_request(request) {}
	virtual ~ShellUtilUriDialogHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button);

	ShellUtilRequest *m_request;
};

/* Main server manager singleton (0x8124BB02) */
class ShellUtilServerMgr {
public:
	ShellUtilServerMgr()
		: m_mutex("SceShellUtilServerMgrMutex", 0),
		  m_shuttingDown(false),
		  m_activeDialogId(-1) {}
	virtual ~ShellUtilServerMgr() {}

	int EnqueueRequest(const common::SharedPtr<ShellUtilRequest> &request);
	int EnqueueJobRequest(const common::SharedPtr<ShellUtilRequest> &request);
	int PrepareJobRequest(ShellUtilRequest *request);
	int OnSessionDisconnected(IPMI::Session *session);

	int ExecuteBackgroundWork(ShellUtilRequest *request);
	int ExecuteMainThreadWork(ShellUtilRequest *request);
	int FinishRequest(ShellUtilRequest *request);

	int CloseActiveUriDialog();
	void ClearActiveUriDialogId() { m_activeDialogId = -1; }
	int GetActiveUriDialogId() const { return m_activeDialogId; }

private:
	int HandleUsageCounterWork(ShellUtilUsageCounterRequest *request);
	int HandleLaunchUriWork(ShellUtilLaunchUriRequest *request);
	int HandleThemeMainThread(ShellUtilThemeRequest *request);
	int OpenUriFallbackDialog(ShellUtilRequest *request, const CommonGuiDialogLayout &layout,
		const wchar_t *messageText, const wchar_t *confirmLabel, const wchar_t *cancelLabel);
	int HandleLaunchUriMainThread(ShellUtilLaunchUriRequest *request);
	int HandleErrorDialogMainThread(ShellUtilErrorDialogRequest *request);
	int HandleDateTimeFormatMainThread(ShellUtilDateTimeFormatRequest *request);
	int HandleLocaleMainThread(ShellUtilIntRequest *request);
	void StopServicesForPowerTransition();

	list<common::SharedPtr<ShellUtilRequest> > m_pendingRequests;
	thread::RMutex m_mutex;
	bool m_shuttingDown;
	int m_activeDialogId;
};

/* 0x8124D25E */
int ShellUtilWorkerJob::Run()
{
	if (s_utilServerMgr != NULL)
		return s_utilServerMgr->ExecuteBackgroundWork(m_request);
	return 0;
}

/* 0x8124D276 */
void ShellUtilWorkerJob::Finish(int result)
{
	(void)result;
	if (s_utilServerMgr != NULL)
		s_utilServerMgr->ExecuteMainThreadWork(m_request);
}

/* 0x8124D290 */
void ShellUtilUriDialogHandler::operator()(SceInt32 dialogId, SceInt32 button)
{
	(void)dialogId;
	if (s_utilServerMgr == NULL)
		return;

	switch (button) {
	case 0: {
		s_utilServerMgr->ClearActiveUriDialogId();
		Plugin *pm = Plugin::Find("power_manage_plugin");
		if (pm != NULL) {
			power_manage::PowerManageInterface *iface =
				(power_manage::PowerManageInterface *)pm->GetInterface(1);
			if (iface != NULL)
				iface->removePowerEventListener(onPowerEventCloseUriDialog, NULL);
		}
		inputdevice::ReleaseOwnership(SHELL_UTIL_INPUT_OWNERSHIP_DIALOG);
		s_utilServerMgr->FinishRequest(m_request);
		break;
	}
	case 4: {
		if (m_request->GetType() == SHELL_UTIL_REQ_LAUNCH_APP_BY_URI) {
			ShellUtilLaunchUriRequest *uriReq = (ShellUtilLaunchUriRequest *)m_request;
			AppmgrLaunchAppByNameOption opt;
			sce_paf_memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.launchMode = SHELL_UTIL_LAUNCH_MODE_BY_URI;
			opt.creatorAppId = uriReq->m_callerAppId;
			sceAppMgrLaunchAppByName2("NPXS10002", uriReq->m_uri.c_str(), &opt);
		}
		s_utilServerMgr->CloseActiveUriDialog();
		break;
	}
	case 2:
	case 5:
		s_utilServerMgr->CloseActiveUriDialog();
		break;
	default:
		break;
	}
}

/* 0x8124D1DE */
static void mainThreadExecuteTrampoline(void *requestPtr)
{
	if (s_utilServerMgr != NULL)
		s_utilServerMgr->ExecuteMainThreadWork((ShellUtilRequest *)requestPtr);
}

/* 0x8124D1F4 */
static void shutdownFadeDoneTrampoline(void *requestPtr)
{
	if (s_utilServerMgr != NULL)
		s_utilServerMgr->FinishRequest((ShellUtilRequest *)requestPtr);
}

/* 0x8124D20A */
static void onPowerEventCloseUriDialog(int eventCode, const void *eventBuf,
	SceSize eventSize, void *userData)
{
	(void)eventCode;
	(void)eventBuf;
	(void)eventSize;
	(void)userData;
	if (s_utilServerMgr != NULL)
		s_utilServerMgr->CloseActiveUriDialog();
}

int ShellUtilServerMgr::CloseActiveUriDialog()
{
	if (m_activeDialogId >= 0)
		SceCommonGuiDialog_BC311DC0(m_activeDialogId);
	return m_activeDialogId;
}

/* 0x8124BBE4 */
int ShellUtilServerMgr::PrepareJobRequest(ShellUtilRequest *request)
{
	switch (request->GetType()) {
	case SHELL_UTIL_REQ_REQUEST_STANDBY:
		inputdevice::Lock(SHELL_UTIL_INPUT_LOCK_ALL);
		sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
		scePowerRequestStandby();
		break;
	case SHELL_UTIL_REQ_REQUEST_COLD_RESET:
		inputdevice::Lock(SHELL_UTIL_INPUT_LOCK_ALL);
		sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
		vshPowerRequestColdReset();
		break;
	case SHELL_UTIL_REQ_LAUNCH_APP_BY_URI: {
		ShellUtilLaunchUriRequest *uriReq = (ShellUtilLaunchUriRequest *)request;
		if (uriReq->m_session != NULL)
			uriReq->m_callerAppId = uriReq->m_session->getAppId();
		break;
	}
	default:
		break;
	}
	return 0;
}

/* 0x8124BC36 */
int ShellUtilServerMgr::EnqueueJobRequest(const common::SharedPtr<ShellUtilRequest> &request)
{
	common::SharedPtr<job::JobItem> workerJob(new ShellUtilWorkerJob(request.get()));
	if (workerJob.get() == NULL)
		return SHELL_UTIL_ERR_NO_MEMORY;

	m_mutex.Lock();
	m_pendingRequests.push_back(request);
	m_mutex.Unlock();

	int ret = job::JobQueue::default_queue->Enqueue(workerJob);
	if (ret == 0)
		PrepareJobRequest(request.get());
	return ret;
}

/* 0x8124BD58 */
int ShellUtilServerMgr::EnqueueRequest(const common::SharedPtr<ShellUtilRequest> &request)
{
	if (m_shuttingDown)
		return SHELL_UTIL_ERR_NOT_READY;

	switch (request->GetType()) {
	case SHELL_UTIL_REQ_REQUEST_STANDBY:
	case SHELL_UTIL_REQ_REQUEST_COLD_RESET:
		m_shuttingDown = true;
		return EnqueueJobRequest(request);
	case SHELL_UTIL_REQ_SET_LOCALE:
	case SHELL_UTIL_REQ_REFRESH_LSDB:
	case SHELL_UTIL_REQ_LAUNCH_APP_BY_URI:
	case SHELL_UTIL_REQ_CHANGE_THEME:
	case SHELL_UTIL_REQ_DELETE_THEME:
	case SHELL_UTIL_REQ_ADD_USAGE_COUNTER:
		return EnqueueJobRequest(request);
	default:
		m_mutex.Lock();
		m_pendingRequests.push_back(request);
		m_mutex.Unlock();
		thread::CallQueue::main_thread_callqueue.Enqueue(NULL,
			mainThreadExecuteTrampoline, request.get());
		return 0;
	}
}

/* 0x8124BDDE */
int ShellUtilServerMgr::OnSessionDisconnected(IPMI::Session *session)
{
	ShellUtilUnlockAllRequest *unlockReq = new ShellUtilUnlockAllRequest();
	if (unlockReq == NULL)
		return SHELL_UTIL_ERR_NO_MEMORY;

	m_mutex.Lock();
	for (list<common::SharedPtr<ShellUtilRequest> >::iterator it = m_pendingRequests.begin();
	     it != m_pendingRequests.end(); ++it) {
		if ((*it)->m_session == session) {
			(*it)->m_session = NULL;
			(*it)->m_requestId = -1;
		}
	}

	unlockReq->m_session = session;
	unlockReq->m_value = (int)shellHandlerTableGetAllMask();
	common::SharedPtr<ShellUtilRequest> reqPtr(unlockReq);
	int ret = EnqueueRequest(reqPtr);
	m_mutex.Unlock();
	return ret;
}

/* 0x8124BF1A */
int ShellUtilServerMgr::HandleUsageCounterWork(ShellUtilUsageCounterRequest *request)
{
	if (request->m_counterKind >= 0 && request->m_counterKind <= SHELL_UTIL_MAX_COUNTER_KIND) {
		request->m_result = shellUsageCounterAdd(
			SHELL_UTIL_COUNTER_BASE_ID + request->m_counterKind,
			request->m_increment, 0);
	}
	return 0;
}

/* 0x8124BF5E */
int ShellUtilServerMgr::HandleLaunchUriWork(ShellUtilLaunchUriRequest *request)
{
	if (request->m_uri.empty())
		return 0;

	string targetTitleId("");
	int ret = SceLsdb_E2838878(g_shellLsdbClient->client, request->m_uri.c_str(),
		&targetTitleId, 0);
	request->m_result = ret;
	if (ret != 0)
		return 0;

	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	if (appInfo.get() == NULL) {
		request->m_result = SHELL_UTIL_ERR_NO_MEMORY;
		return 0;
	}

	ret = SceLsdb_87EE0526(g_shellLsdbClient->client, &targetTitleId, &appInfo);
	request->m_result = ret;
	if (ret != 0)
		return 0;

	if (!SceLsdb_41019D89(&appInfo)) {
		string launchPrefix("type=LAUNCH_APP_BY_URI&uri=");
		request->m_uri.insert(0, launchPrefix.c_str(), launchPrefix.size());
	}

	AppmgrLaunchAppByNameOption opt;
	sce_paf_memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.launchMode = SHELL_UTIL_LAUNCH_MODE_BY_URI;
	opt.creatorAppId = request->m_callerAppId;
	request->m_result = sceAppMgrLaunchAppByName2(
		targetTitleId.c_str(), request->m_uri.c_str(), &opt);
	return 0;
}

void ShellUtilServerMgr::StopServicesForPowerTransition()
{
	shellTriggerMgrSuspend(NULL, NULL);
	shellCronStop();
	vshRtcSetAlarmTick(NULL, 1);
	shellBgdlSuspend(true);
	if (shellHasMediaInfoFeature()) {
		ShellMediaInfoRequest mediaReq;
		sce_paf_memset(&mediaReq, 0, sizeof(mediaReq));
		mediaReq.type = 2;
		shellMediaInfoWrite(&mediaReq, true);
	}
	shellNetCtlTerm();
}

/* 0x8124C152 */
int ShellUtilServerMgr::ExecuteBackgroundWork(ShellUtilRequest *request)
{
	switch (request->GetType()) {
	case SHELL_UTIL_REQ_SET_LOCALE: {
		ShellUtilIntRequest *localeReq = (ShellUtilIntRequest *)request;
		if ((unsigned int)localeReq->m_value < SHELL_UTIL_MAX_LOCALE_COUNT)
			SceLsdb_C6678B98(localeReq->m_value);
		break;
	}
	case SHELL_UTIL_REQ_REFRESH_LSDB:
		SceLsdb_6AFF7E71();
		break;
	case SHELL_UTIL_REQ_REQUEST_STANDBY:
	case SHELL_UTIL_REQ_REQUEST_COLD_RESET:
		StopServicesForPowerTransition();
		break;
	case SHELL_UTIL_REQ_LAUNCH_APP_BY_URI:
		HandleLaunchUriWork((ShellUtilLaunchUriRequest *)request);
		break;
	case SHELL_UTIL_REQ_CHANGE_THEME:
	case SHELL_UTIL_REQ_DELETE_THEME: {
		ShellUtilThemeRequest *themeReq = (ShellUtilThemeRequest *)request;
		if (request->GetType() == SHELL_UTIL_REQ_CHANGE_THEME)
			themeReq->m_result = shellThemeChange(themeReq->m_themePath.c_str());
		else
			themeReq->m_result = shellThemeDelete(themeReq->m_themePath.c_str());

		Plugin *launcher = Plugin::Find("applauncher_plugin");
		if (launcher != NULL) {
			ShellPluginInterface *iface =
				(ShellPluginInterface *)launcher->GetInterface(1);
			if (iface != NULL)
				iface->func[APPLAUNCHER_SLOT_PREPARE_THEME]();
		}
		break;
	}
	case SHELL_UTIL_REQ_ADD_USAGE_COUNTER:
		HandleUsageCounterWork((ShellUtilUsageCounterRequest *)request);
		break;
	default:
		break;
	}
	return 0;
}

/* 0x8124C2B8 */
int ShellUtilServerMgr::FinishRequest(ShellUtilRequest *request)
{
	m_mutex.Lock();

	switch (request->GetType()) {
	case SHELL_UTIL_REQ_REQUEST_STANDBY:
	case SHELL_UTIL_REQ_REQUEST_COLD_RESET:
		scePowerCancelRequest();
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	case SHELL_UTIL_REQ_LAUNCH_APP_BY_URI: {
		ShellUtilLaunchUriRequest *uriReq = (ShellUtilLaunchUriRequest *)request;
		if (uriReq->m_session != NULL && uriReq->m_requestId != -1)
			shellUtilSendAsyncResponse(uriReq->m_session, uriReq->m_requestId, uriReq->m_result);
		break;
	}
	default:
		break;
	}

	for (list<common::SharedPtr<ShellUtilRequest> >::iterator it = m_pendingRequests.begin();
	     it != m_pendingRequests.end(); ++it) {
		if (it->get() == request) {
			m_pendingRequests.erase(it);
			break;
		}
	}

	m_mutex.Unlock();
	return 0;
}

/* 0x8124C384 */
int ShellUtilServerMgr::HandleThemeMainThread(ShellUtilThemeRequest *request)
{
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->reloadWallpaper();
	}

	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
		if (iface != NULL)
			iface->func[TOPMENU_SLOT_RELOAD_THEME]();
	}

	Plugin *launcher = Plugin::Find("applauncher_plugin");
	if (launcher != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)launcher->GetInterface(1);
		if (iface != NULL)
			iface->func[APPLAUNCHER_SLOT_APPLY_THEME]();
	}

	Plugin *indicator = Plugin::Find("indicator_plugin");
	if (indicator != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)indicator->GetInterface(1);
		if (iface != NULL)
			iface->func[INDICATOR_SLOT_RELOAD_THEME]();
	}

	Plugin *impose = Plugin::Find("impose_plugin");
	if (impose != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)impose->GetInterface(1);
		if (iface != NULL)
			iface->func[IMPOSE_SLOT_RELOAD_THEME]();
	}

	if (request->m_session != NULL && request->m_requestId != -1)
		shellUtilSendAsyncResponse(request->m_session, request->m_requestId, request->m_result);
	return 0;
}

/* 0x8124C438 */
int ShellUtilServerMgr::OpenUriFallbackDialog(ShellUtilRequest *request,
	const CommonGuiDialogLayout &layout, const wchar_t *messageText,
	const wchar_t *confirmLabel, const wchar_t *cancelLabel)
{
	if (m_activeDialogId >= 0)
		SceCommonGuiDialog_BC311DC0(m_activeDialogId);

	CommonGuiLayoutDialogParam dialogParam;
	dialogParam.soundId = (SceUInt32)SHELL_UTIL_URI_DIALOG_SOUND_ID;
	dialogParam.priority = SHELL_UTIL_URI_DIALOG_PRIORITY;
	dialogParam.notifyClose = true;
	dialogParam.layout = layout;
	if (messageText != NULL)
		dialogParam.text = messageText;

	ShellUtilUriDialogHandler *handler = new ShellUtilUriDialogHandler(request);
	if (handler == NULL)
		return SHELL_ERROR_NO_MEMORY;

	dialogParam.handler = handler;
	int dialogId = SceCommonGuiDialog_CCFCD9A6(&dialogParam);
	if (dialogId >= 0) {
		if (confirmLabel != NULL) {
			ui::Widget *confirmBtn = SceCommonGuiDialog_8947FC23(dialogId,
				SHELL_UTIL_DIALOG_PART_CONFIRM_BTN);
			if (confirmBtn != NULL) {
				wstring label(confirmLabel);
				confirmBtn->SetString(label);
			}
		}
		if (cancelLabel != NULL) {
			ui::Widget *cancelBtn = SceCommonGuiDialog_8947FC23(dialogId,
				SHELL_UTIL_DIALOG_PART_CANCEL_BTN);
			if (cancelBtn != NULL) {
				wstring label(cancelLabel);
				cancelBtn->SetString(label);
			}
		}
		inputdevice::GetOwnership(SHELL_UTIL_INPUT_OWNERSHIP_DIALOG);
		Plugin *pm = Plugin::Find("power_manage_plugin");
		if (pm != NULL) {
			power_manage::PowerManageInterface *iface =
				(power_manage::PowerManageInterface *)pm->GetInterface(1);
			if (iface != NULL)
				iface->addPowerEventListener(onPowerEventCloseUriDialog, NULL);
		}
		m_activeDialogId = dialogId;
	}
	return dialogId;
}

/* 0x8124C66A */
int ShellUtilServerMgr::HandleLaunchUriMainThread(ShellUtilLaunchUriRequest *request)
{
	if (request->m_result == SHELL_UTIL_LSDB_ERR_INVALID_URI) {
		request->m_result = SHELL_UTIL_ERR_INVALID_PARAM;
		return 0;
	}

	if (request->m_result != SHELL_UTIL_LSDB_ERR_URI_NOT_FOUND)
		return 0;

	request->m_result = SHELL_UTIL_ERR_URI_NOT_FOUND;
	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu == NULL)
		return 0;

	CommonGuiDialogLayout layout = SceCommonGuiDialog_AF75CA6D;
	IDParam msgId(SHELL_UTIL_MSG_URI_NOT_FOUND);
	const wchar_t *messageText = topmenu->GetString(msgId);
	const wchar_t *confirmLabel = NULL;
	const wchar_t *cancelLabel = NULL;

	if (sce_paf_strncmp(request->m_uri.c_str(), "pscloud:", 8) == 0) {
		request->m_uri = "psns:browse?product&title=PCSI00013";
		layout = SceCommonGuiDialog_22B17F2B;
		IDParam storePromptId(SHELL_UTIL_MSG_PSCLOUD_STORE_PROMPT);
		IDParam storeBtnId(SHELL_UTIL_MSG_PSCLOUD_BTN_STORE);
		IDParam cancelBtnId((SceUInt32)SHELL_UTIL_MSG_PSCLOUD_BTN_CANCEL);
		messageText = topmenu->GetString(storePromptId);
		confirmLabel = topmenu->GetString(storeBtnId);
		cancelLabel = topmenu->GetString(cancelBtnId);
	}

	int dialogId = OpenUriFallbackDialog(request, layout, messageText, confirmLabel, cancelLabel);
	if (dialogId >= 0)
		return SHELL_UTIL_ERR_DEFERRED;
	return dialogId;
}

/* 0x8124C8EE */
int ShellUtilServerMgr::HandleErrorDialogMainThread(ShellUtilErrorDialogRequest *request)
{
	CommonGuiMessageDialogParam dialogParam;
	dialogParam.type = request->m_errorCode;
	if (request->m_session != NULL)
		dialogParam.buttonType = request->m_session->getAppId();
	if (request->m_errorCode == 0)
		dialogParam.defaultButton = 0;
	dialogParam.title.format = s_defaultErrorMessage;

	shellErrorDialogOpen(request->m_errorType, &dialogParam, false);

	if (request->m_session != NULL && request->m_requestId != -1)
		shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
	return 0;
}

/* 0x8124CA20 */
int ShellUtilServerMgr::HandleDateTimeFormatMainThread(ShellUtilDateTimeFormatRequest *request)
{
	int result = SHELL_UTIL_ERR_INVALID_PARAM;

	if (!request->m_dateFormat.empty() || !request->m_timeFormat.empty()) {
		if (!request->m_dateFormat.empty())
			datetime::DateTime::SetDateFormat(request->m_dateFormat.c_str());
		if (!request->m_timeFormat.empty())
			datetime::DateTime::SetTimeFormat(request->m_timeFormat.c_str());

		ui::Event formatEvent(0x2003, 0x200000, 0x40000, 0, 0, 0, 0, 0);
		Framework::Instance()->GetEnvironmentInstance()->m_ev_queue.PushBack(formatEvent);
		result = 0;
	}

	if (request->m_session != NULL && request->m_requestId != -1)
		shellUtilSendAsyncResponse(request->m_session, request->m_requestId, result);
	return 0;
}

/* 0x8124CB02 */
int ShellUtilServerMgr::HandleLocaleMainThread(ShellUtilIntRequest *request)
{
	int result = SHELL_UTIL_ERR_INVALID_PARAM;

	if ((unsigned int)request->m_value < SHELL_UTIL_MAX_LOCALE_COUNT) {
		Framework::Instance()->SetLocale((Locale)request->m_value);
		Plugin::SetLocaleAll((Locale)request->m_value);
		shellHomeUpdateRequest(1);

		ui::Event localeEvent(0x2002, 0x200000, 0x40000, 0, 0, 0, 0, 0);
		Framework::Instance()->GetEnvironmentInstance()->m_ev_queue.PushBack(localeEvent);
		result = 0;
	}

	if (request->m_session != NULL && request->m_requestId != -1)
		shellUtilSendAsyncResponse(request->m_session, request->m_requestId, result);
	return 0;
}

/* 0x8124CBA2 */
int ShellUtilServerMgr::ExecuteMainThreadWork(ShellUtilRequest *request)
{
	m_mutex.Lock();

	bool keepPending = false;

	switch (request->GetType()) {
	case SHELL_UTIL_REQ_LOCK: {
		ShellUtilIntRequest *lockReq = (ShellUtilIntRequest *)request;
		if (lockReq->m_session != NULL) {
			ShellLockManager *lockMgr = shellHandlerTableGetInstance();
			if (lockMgr != NULL)
				lockMgr->Lock((SceUInt32)lockReq->m_value,
					lockReq->m_session, lockReq->m_requestId);
		}
		break;
	}
	case SHELL_UTIL_REQ_UNLOCK: {
		ShellUtilIntRequest *unlockReq = (ShellUtilIntRequest *)request;
		if (unlockReq->m_session != NULL) {
			ShellLockManager *lockMgr = shellHandlerTableGetInstance();
			if (lockMgr != NULL)
				lockMgr->Unlock((SceUInt32)unlockReq->m_value,
					unlockReq->m_session, unlockReq->m_requestId);
		}
		break;
	}
	case SHELL_UTIL_REQ_UNLOCK_ALL: {
		ShellUtilIntRequest *unlockReq = (ShellUtilIntRequest *)request;
		if (unlockReq->m_session != NULL) {
			ShellLockManager *lockMgr = shellHandlerTableGetInstance();
			if (lockMgr != NULL)
				lockMgr->UnlockAll((SceUInt32)unlockReq->m_value,
					unlockReq->m_session, unlockReq->m_requestId);
		}
		break;
	}
	case SHELL_UTIL_REQ_SET_LOCALE:
		keepPending = (HandleLocaleMainThread((ShellUtilIntRequest *)request) == SHELL_UTIL_ERR_DEFERRED);
		break;
	case SHELL_UTIL_REQ_SET_DATETIME_FORMAT:
		keepPending = (HandleDateTimeFormatMainThread((ShellUtilDateTimeFormatRequest *)request) == SHELL_UTIL_ERR_DEFERRED);
		break;
	case SHELL_UTIL_REQ_LAUNCH_APP_BY_NAME: {
		ShellUtilStringRequest *strReq = (ShellUtilStringRequest *)request;
		int result = SHELL_UTIL_ERR_INVALID_PARAM;
		if (!strReq->m_text.empty()) {
			Plugin *launcher = Plugin::Find("applauncher_plugin");
			if (launcher != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)launcher->GetInterface(1);
				if (iface != NULL) {
					void (*launchFn)(const string *, int) =
						(void (*)(const string *, int))iface->func[APPLAUNCHER_SLOT_LAUNCH_BY_NAME];
					launchFn(&strReq->m_text, 1);
				}
			}
			result = 0;
		}
		if (strReq->m_session != NULL && strReq->m_requestId != -1)
			shellUtilSendAsyncResponse(strReq->m_session, strReq->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_SET_CLOCK_VISIBLE: {
		ShellUtilIntRequest *intReq = (ShellUtilIntRequest *)request;
		int result = SHELL_UTIL_ERR_INVALID_PARAM;
		if ((unsigned int)intReq->m_value <= 1u) {
			Plugin *indicator = Plugin::Find("indicator_plugin");
			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);
				if (iface != NULL) {
					void (*setFn)(bool) =
						(void (*)(bool))iface->func[INDICATOR_SLOT_SET_CLOCK_VISIBLE];
					setFn(intReq->m_value == 1);
				}
			}
			result = 0;
		}
		if (intReq->m_session != NULL && intReq->m_requestId != -1)
			shellUtilSendAsyncResponse(intReq->m_session, intReq->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_SET_STATUSBAR_VISIBLE: {
		ShellUtilIntRequest *intReq = (ShellUtilIntRequest *)request;
		int result = SHELL_UTIL_ERR_INVALID_PARAM;
		if ((unsigned int)intReq->m_value <= 1u) {
			Plugin *indicator = Plugin::Find("indicator_plugin");
			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);
				if (iface != NULL) {
					void (*setFn)(bool) =
						(void (*)(bool))iface->func[INDICATOR_SLOT_SET_STATUSBAR_VISIBLE];
					setFn(intReq->m_value == 1);
				}
			}
			result = 0;
		}
		if (intReq->m_session != NULL && intReq->m_requestId != -1)
			shellUtilSendAsyncResponse(intReq->m_session, intReq->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_SET_MUSIC_PAUSE: {
		ShellUtilIntRequest *intReq = (ShellUtilIntRequest *)request;
		int result = SHELL_UTIL_ERR_INVALID_PARAM;
		if ((unsigned int)intReq->m_value <= 1u) {
			musicCoreSetSystemPause(intReq->m_value != 1 ? 1 : 0);
			result = 0;
		}
		if (intReq->m_session != NULL && intReq->m_requestId != -1)
			shellUtilSendAsyncResponse(intReq->m_session, intReq->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_REFRESH_LSDB:
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	case SHELL_UTIL_REQ_RELOAD_ACCESSIBILITY:
		shellReloadAccessibilityState();
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	case SHELL_UTIL_REQ_REQUEST_STANDBY:
	case SHELL_UTIL_REQ_REQUEST_COLD_RESET: {
		shellParentalControlUpdate(true, true);
		Plugin *impose = Plugin::Find("impose_plugin");
		if (impose != NULL) {
			ShellPluginInterface *iface =
				(ShellPluginInterface *)impose->GetInterface(1);
			if (iface != NULL) {
				void (*fadeFn)(void (*)(void *), void *) =
					(void (*)(void (*)(void *), void *))iface->func[IMPOSE_SLOT_START_SHUTDOWN_FADE];
				fadeFn(shutdownFadeDoneTrampoline, request);
				keepPending = true;
			}
		}
		break;
	}
	case SHELL_UTIL_REQ_SHOW_ERROR_DIALOG:
		keepPending = (HandleErrorDialogMainThread((ShellUtilErrorDialogRequest *)request) == SHELL_UTIL_ERR_DEFERRED);
		break;
	case SHELL_UTIL_REQ_NOTIFY_TOPMENU_UPDATE: {
		Plugin *topmenu = Plugin::Find("topmenu_plugin");
		if (topmenu != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
			if (iface != NULL)
				iface->func[TOPMENU_SLOT_NOTIFY_UPDATE]();
		}
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	}
	case SHELL_UTIL_REQ_RELOAD_WALLPAPER: {
		Plugin *launcher = Plugin::Find("applauncher_plugin");
		if (launcher != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)launcher->GetInterface(1);
			if (iface != NULL)
				iface->func[APPLAUNCHER_SLOT_RELOAD_WALLPAPER]();
		}
		Plugin *indicator = Plugin::Find("indicator_plugin");
		if (indicator != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)indicator->GetInterface(1);
			if (iface != NULL)
				iface->func[INDICATOR_SLOT_RELOAD_WALLPAPER]();
		}
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	}
	case SHELL_UTIL_REQ_RELOAD_KEYLOCK_WALLPAPER: {
		Plugin *pm = Plugin::Find("power_manage_plugin");
		if (pm != NULL) {
			power_manage::PowerManageInterface *iface =
				(power_manage::PowerManageInterface *)pm->GetInterface(1);
			if (iface != NULL)
				iface->reloadWallpaper();
		}
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	}
	case SHELL_UTIL_REQ_SET_BATTERY_PERCENT: {
		ShellUtilIntRequest *intReq = (ShellUtilIntRequest *)request;
		int result = SHELL_UTIL_ERR_INVALID_PARAM;
		if ((unsigned int)intReq->m_value <= 1u) {
			Plugin *indicator = Plugin::Find("indicator_plugin");
			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);
				if (iface != NULL) {
					void (*setFn)(bool) =
						(void (*)(bool))iface->func[INDICATOR_SLOT_SET_BATTERY_PERCENT];
					setFn(intReq->m_value == 1);
				}
			}
			result = 0;
		}
		if (intReq->m_session != NULL && intReq->m_requestId != -1)
			shellUtilSendAsyncResponse(intReq->m_session, intReq->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_ACTIVATE_TITLE: {
		ShellUtilStringRequest *strReq = (ShellUtilStringRequest *)request;
		int result = SHELL_UTIL_ERR_INVALID_PARAM;
		if (!strReq->m_text.empty()) {
			Plugin *topmenu = Plugin::Find("topmenu_plugin");
			if (topmenu != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)topmenu->GetInterface(1);
				if (iface != NULL) {
					void (*activateFn)(const char *) =
						(void (*)(const char *))iface->func[TOPMENU_SLOT_ACTIVATE_TITLE];
					activateFn(strReq->m_text.c_str());
				}
			}
			result = 0;
		}
		if (strReq->m_session != NULL && strReq->m_requestId != -1)
			shellUtilSendAsyncResponse(strReq->m_session, strReq->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_SET_NOTIFICATION_ENABLED: {
		ShellUtilIntRequest *intReq = (ShellUtilIntRequest *)request;
		int result = SHELL_UTIL_ERR_INVALID_PARAM;
		if ((unsigned int)intReq->m_value <= 1u) {
			Plugin *indicator = Plugin::Find("indicator_plugin");
			if (indicator != NULL) {
				ShellPluginInterface *iface =
					(ShellPluginInterface *)indicator->GetInterface(1);
				if (iface != NULL) {
					void (*setFn)(bool) =
						(void (*)(bool))iface->func[INDICATOR_SLOT_SET_NOTIFICATION_ENABLED];
					setFn(intReq->m_value == 1);
				}
			}
			result = 0;
		}
		if (intReq->m_session != NULL && intReq->m_requestId != -1)
			shellUtilSendAsyncResponse(intReq->m_session, intReq->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_SET_KEY_NAVIGATION: {
		ShellUtilIntRequest *intReq = (ShellUtilIntRequest *)request;
		int result = SHELL_UTIL_ERR_INVALID_PARAM;
		if ((unsigned int)intReq->m_value <= 1u) {
			ui::Environment *env = Framework::Instance()->GetEnvironmentInstance();
			ScePafWidget_75FB31F1(env, intReq->m_value == 1);
			result = 0;
		}
		if (intReq->m_session != NULL && intReq->m_requestId != -1)
			shellUtilSendAsyncResponse(intReq->m_session, intReq->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_RELOAD_LIVEAREA: {
		int result = 0;
		Plugin *topmenu = Plugin::Find("topmenu_plugin");
		if (topmenu != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
			if (iface != NULL) {
				int (*reloadFn)(void) =
					(int (*)(void))iface->func[TOPMENU_SLOT_RELOAD_LIVEAREA];
				result = reloadFn();
			}
		}
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, result);
		break;
	}
	case SHELL_UTIL_REQ_LAUNCH_APP_BY_URI:
		keepPending = (HandleLaunchUriMainThread((ShellUtilLaunchUriRequest *)request) == SHELL_UTIL_ERR_DEFERRED);
		break;
	case SHELL_UTIL_REQ_REFRESH_CALLER_APP: {
		if (request->m_session != NULL) {
			int appId = request->m_session->getAppId();
			SceAppMgrAppStatus appStatus;
			if (sceAppMgrGetStatusByAppId(appId, &appStatus) == 0) {
				Plugin *topmenu = Plugin::Find("topmenu_plugin");
				if (topmenu != NULL) {
					ShellPluginInterface *iface =
						(ShellPluginInterface *)topmenu->GetInterface(1);
					if (iface != NULL) {
						void (*refreshFn)(const char *) =
							(void (*)(const char *))iface->func[TOPMENU_SLOT_REFRESH_APP_STATUS];
						refreshFn(appStatus.appName);
					}
				}
			}
		}
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	}
	case SHELL_UTIL_REQ_CHANGE_THEME:
	case SHELL_UTIL_REQ_DELETE_THEME:
		keepPending = (HandleThemeMainThread((ShellUtilThemeRequest *)request) == SHELL_UTIL_ERR_DEFERRED);
		break;
	case SHELL_UTIL_REQ_CANCEL_TOPMENU_GUIDE: {
		Plugin *topmenu = Plugin::Find("topmenu_plugin");
		if (topmenu != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
			if (iface != NULL)
				iface->func[TOPMENU_SLOT_CANCEL_GUIDE]();
		}
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	}
	case SHELL_UTIL_REQ_RESET_IMPOSE_STATUS: {
		Plugin *impose = Plugin::Find("impose_plugin");
		if (impose != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)impose->GetInterface(1);
			if (iface != NULL)
				iface->func[IMPOSE_SLOT_RESET_STATUS]();
		}
		if (request->m_session != NULL && request->m_requestId != -1)
			shellUtilSendAsyncResponse(request->m_session, request->m_requestId, 0);
		break;
	}
	case SHELL_UTIL_REQ_ADD_USAGE_COUNTER: {
		ShellUtilUsageCounterRequest *counterReq = (ShellUtilUsageCounterRequest *)request;
		if (counterReq->m_session != NULL && counterReq->m_requestId != -1)
			shellUtilSendAsyncResponse(counterReq->m_session, counterReq->m_requestId,
				counterReq->m_result);
		break;
	}
	default:
		break;
	}

	if (!keepPending)
		FinishRequest(request);

	m_mutex.Unlock();
	return 0;
}

/* Inline enqueue body shared by Sony's per-method async submission functions (0x81249A68..0x8124B9A8) */
static inline int enqueueAsyncRequestInline(IPMI::Session *session, int requestId,
					    ShellUtilRequest *rawReq)
{
	rawReq->m_session = session;
	rawReq->m_requestId = requestId;
	common::SharedPtr<ShellUtilRequest> reqPtr(rawReq);
	int ret = s_utilServerMgr->EnqueueRequest(reqPtr);
	if (ret < 0)
		return shellUtilSendAsyncResponse(session, requestId, ret);
	return ret;
}

/* 0x81249A68 */
static int submitLockRequest(IPMI::Session *session, int requestId, int lockMask)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilLockRequest *req = new ShellUtilLockRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = lockMask;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x81249B76 */
static int submitUnlockRequest(IPMI::Session *session, int requestId, int lockMask)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilUnlockRequest *req = new ShellUtilUnlockRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = lockMask;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x81249C84 */
static int submitUnlockAllRequest(IPMI::Session *session, int requestId, int lockMask)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilUnlockAllRequest *req = new ShellUtilUnlockAllRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = lockMask;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x81249D92 */
static int submitSetLocaleRequest(IPMI::Session *session, int requestId, int locale)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilSetLocaleRequest *req = new ShellUtilSetLocaleRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = locale;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x81249EA0 */
static int submitDateTimeFormatRequest(IPMI::Session *session, int requestId,
	const wchar_t *dateFormat, const wchar_t *timeFormat)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilDateTimeFormatRequest *req = new ShellUtilDateTimeFormatRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	if (dateFormat != NULL)
		req->m_dateFormat = dateFormat;
	if (timeFormat != NULL)
		req->m_timeFormat = timeFormat;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124A052 */
static int submitLaunchAppByNameRequest(IPMI::Session *session, int requestId, const char *appName)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilLaunchAppByNameRequest *req = new ShellUtilLaunchAppByNameRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	if (appName != NULL)
		req->m_text = appName;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124A1A8 */
static int submitSetClockVisibleRequest(IPMI::Session *session, int requestId, int visible)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilSetClockVisibleRequest *req = new ShellUtilSetClockVisibleRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = visible;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124A2B6 */
static int submitSetStatusbarVisibleRequest(IPMI::Session *session, int requestId, int visible)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilSetStatusbarVisibleRequest *req = new ShellUtilSetStatusbarVisibleRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = visible;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124A3C4 */
static int submitSetMusicPauseRequest(IPMI::Session *session, int requestId, int pauseState)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilSetMusicPauseRequest *req = new ShellUtilSetMusicPauseRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = pauseState;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124A4D2 */
static int submitRefreshLsdbRequest(IPMI::Session *session, int requestId, int refreshFlags)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilRefreshLsdbRequest *req = new ShellUtilRefreshLsdbRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = refreshFlags;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124A5E0 */
static int submitReloadAccessibilityRequest(IPMI::Session *session, int requestId)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilReloadAccessibilityRequest *req = new ShellUtilReloadAccessibilityRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124A9D4 */
static int submitNotifyTopmenuUpdateRequest(IPMI::Session *session, int requestId)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilNotifyTopmenuUpdateRequest *req = new ShellUtilNotifyTopmenuUpdateRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124AAD6 */
static int submitReloadWallpaperRequest(IPMI::Session *session, int requestId, int wallpaperKind)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilReloadWallpaperRequest *req = new ShellUtilReloadWallpaperRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = wallpaperKind;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124ABE4 */
static int submitReloadKeylockWallpaperRequest(IPMI::Session *session, int requestId)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilReloadKeylockWallpaperRequest *req = new ShellUtilReloadKeylockWallpaperRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124ACE6 */
static int submitSetBatteryPercentRequest(IPMI::Session *session, int requestId, int showPercent)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilSetBatteryPercentRequest *req = new ShellUtilSetBatteryPercentRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = showPercent;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124ADF4 */
static int submitActivateTitleRequest(IPMI::Session *session, int requestId, const char *titleId)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilActivateTitleRequest *req = new ShellUtilActivateTitleRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	if (titleId != NULL)
		req->m_text = titleId;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124AF4A */
static int submitSetNotificationEnabledRequest(IPMI::Session *session, int requestId, int enabled)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilSetNotificationEnabledRequest *req = new ShellUtilSetNotificationEnabledRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = enabled;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124B058 */
static int submitSetKeyNavigationRequest(IPMI::Session *session, int requestId, int enabled)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilSetKeyNavigationRequest *req = new ShellUtilSetKeyNavigationRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_value = enabled;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124B166 */
static int submitReloadLiveareaRequest(IPMI::Session *session, int requestId)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilReloadLiveareaRequest *req = new ShellUtilReloadLiveareaRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124B268 */
static int submitLaunchUriRequest(IPMI::Session *session, int requestId, const char *uri)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilLaunchUriRequest *req = new ShellUtilLaunchUriRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	if (uri != NULL)
		req->m_uri = uri;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124B3D8 */
static int submitRefreshCallerAppRequest(IPMI::Session *session, int requestId)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilRefreshCallerAppRequest *req = new ShellUtilRefreshCallerAppRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124B63A */
static int submitDeleteThemeRequest(IPMI::Session *session, int requestId, const char *themePath)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilDeleteThemeRequest *req = new ShellUtilDeleteThemeRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	if (themePath != NULL)
		req->m_themePath = themePath;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124B79A */
static int submitCancelTopmenuGuideRequest(IPMI::Session *session, int requestId)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilCancelTopmenuGuideRequest *req = new ShellUtilCancelTopmenuGuideRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124B8A6 */
static int submitResetImposeStatusRequest(IPMI::Session *session, int requestId)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilResetImposeStatusRequest *req = new ShellUtilResetImposeStatusRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124B9A8 */
static int submitUsageCounterRequest(IPMI::Session *session, int requestId,
	int counterKind, unsigned int increment)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilUsageCounterRequest *req = new ShellUtilUsageCounterRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	req->m_counterKind = counterKind;
	req->m_increment = increment;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124D440 */
static int dispatchSyncMethod(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inList, SceUInt32 inCount,
	IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	int result;

	switch (methodId) {
	case SHELL_UTIL_METHOD_REQUEST_STANDBY: {
		const int *inWords = (const int *)inList[0].pData;
		result = shellShutdownRequest(session, inWords[0]);
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}
	case SHELL_UTIL_METHOD_REQUEST_COLD_RESET: {
		const int *inWords = (const int *)inList[0].pData;
		result = shellColdResetRequest(session, inWords[0]);
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}
	case SHELL_UTIL_METHOD_SHOW_ERROR_DIALOG: {
		const int *inWords = (const int *)inList[0].pData;
		result = shellUtilShowErrorDialog(session, inWords[0], inWords[1], inWords[2]);
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}
	case SHELL_UTIL_METHOD_GET_PARENTAL_REMAINING:
		result = parentalGetRemainingSec();
		return session->respondToSyncMethodRequest(result, outList, outCount);
	default:
		return SHELL_UTIL_IPMI_ERR_UNSUPPORTED;
	}
}

/* 0x8124D4E4 */
static int dispatchAsyncMethod(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
	const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)inCount;
	int reqId = (int)requestId;

	switch (methodId) {
	case SHELL_UTIL_METHOD_LOCK:
		return submitLockRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_UNLOCK:
		return submitUnlockRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_UNLOCK_ALL:
		return submitUnlockAllRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_SET_LOCALE:
		return submitSetLocaleRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_SET_DATETIME_FORMAT:
		return submitDateTimeFormatRequest(session, reqId,
			(const wchar_t *)inList[0].pData, (const wchar_t *)inList[1].pData);
	case SHELL_UTIL_METHOD_LAUNCH_APP_BY_NAME:
		return submitLaunchAppByNameRequest(session, reqId, (const char *)inList[0].pData);
	case SHELL_UTIL_METHOD_SET_CLOCK_VISIBLE:
		return submitSetClockVisibleRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_SET_STATUSBAR_VISIBLE:
		return submitSetStatusbarVisibleRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_SET_MUSIC_PAUSE:
		return submitSetMusicPauseRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_REFRESH_LSDB:
		return submitRefreshLsdbRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_RELOAD_ACCESSIBILITY:
		return submitReloadAccessibilityRequest(session, reqId);
	case SHELL_UTIL_METHOD_NOTIFY_TOPMENU_UPDATE:
		return submitNotifyTopmenuUpdateRequest(session, reqId);
	case SHELL_UTIL_METHOD_RELOAD_WALLPAPER:
		return submitReloadWallpaperRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_RELOAD_KEYLOCK_WALLPAPER:
		return submitReloadKeylockWallpaperRequest(session, reqId);
	case SHELL_UTIL_METHOD_SET_BATTERY_PERCENT:
		return submitSetBatteryPercentRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_ACTIVATE_TITLE:
		return submitActivateTitleRequest(session, reqId, (const char *)inList[0].pData);
	case SHELL_UTIL_METHOD_SET_NOTIFICATION_ENABLED:
		return submitSetNotificationEnabledRequest(session, reqId,
			*(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_SET_KEY_NAVIGATION:
		return submitSetKeyNavigationRequest(session, reqId, *(const int *)inList[0].pData);
	case SHELL_UTIL_METHOD_RELOAD_LIVEAREA:
		return submitReloadLiveareaRequest(session, reqId);
	case SHELL_UTIL_METHOD_LAUNCH_APP_BY_URI:
		return submitLaunchUriRequest(session, reqId, (const char *)inList[0].pData);
	case SHELL_UTIL_METHOD_REFRESH_CALLER_APP:
		return submitRefreshCallerAppRequest(session, reqId);
	case SHELL_UTIL_METHOD_CHANGE_THEME:
		return shellSystemNoticeRequest(session, reqId, (const char *)inList[0].pData);
	case SHELL_UTIL_METHOD_DELETE_THEME:
		return submitDeleteThemeRequest(session, reqId, (const char *)inList[0].pData);
	case SHELL_UTIL_METHOD_CANCEL_TOPMENU_GUIDE:
		return submitCancelTopmenuGuideRequest(session, reqId);
	case SHELL_UTIL_METHOD_RESET_IMPOSE_STATUS:
		return submitResetImposeStatusRequest(session, reqId);
	case SHELL_UTIL_METHOD_ADD_USAGE_COUNTER: {
		const SceUInt32 *words = (const SceUInt32 *)inList[0].pData;
		return submitUsageCounterRequest(session, reqId, (int)words[0], words[1]);
	}
	default:
		return SHELL_UTIL_IPMI_ERR_UNSUPPORTED;
	}
}

} /* anonymous namespace */

/* 0x8124D420 */
int shellUtilSendAsyncResponse(IPMI::Session *session, int requestId, int result)
{
	return session->respondToAsyncMethodRequest(SHELL_UTIL_METHOD_ASYNC_RESULT,
		(SceUInt32)requestId, result, NULL, 0);
}

/* 0x8124A6E2 */
int shellShutdownRequest(IPMI::Session *session, int flags)
{
	if (s_utilServerMgr == NULL)
		return SHELL_UTIL_ERR_NOT_READY;

	ShellUtilRequestStandbyRequest *req = new ShellUtilRequestStandbyRequest();
	if (req == NULL)
		return SHELL_UTIL_ERR_NO_MEMORY;

	req->m_session = session;
	req->m_value = flags;
	common::SharedPtr<ShellUtilRequest> reqPtr(req);
	return s_utilServerMgr->EnqueueRequest(reqPtr);
}

/* 0x8124A7D8 */
int shellColdResetRequest(IPMI::Session *session, int flags)
{
	if (s_utilServerMgr == NULL)
		return SHELL_UTIL_ERR_NOT_READY;

	ShellUtilRequestColdResetRequest *req = new ShellUtilRequestColdResetRequest();
	if (req == NULL)
		return SHELL_UTIL_ERR_NO_MEMORY;

	req->m_session = session;
	req->m_value = flags;
	common::SharedPtr<ShellUtilRequest> reqPtr(req);
	return s_utilServerMgr->EnqueueRequest(reqPtr);
}

/* 0x8124A8CE */
int shellUtilShowErrorDialog(IPMI::Session *session, int errorType, int errorCode, int flags)
{
	if (s_utilServerMgr == NULL)
		return SHELL_UTIL_ERR_NOT_READY;

	ShellUtilErrorDialogRequest *req = new ShellUtilErrorDialogRequest();
	if (req == NULL)
		return SHELL_UTIL_ERR_NO_MEMORY;

	req->m_session = session;
	req->m_errorType = errorType;
	req->m_errorCode = errorCode;
	req->m_flags = flags;
	common::SharedPtr<ShellUtilRequest> reqPtr(req);
	return s_utilServerMgr->EnqueueRequest(reqPtr);
}

/* 0x8124B4DA */
int shellSystemNoticeRequest(IPMI::Session *session, int requestId, const char *themePath)
{
	if (s_utilServerMgr == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NOT_READY);
	ShellUtilChangeThemeRequest *req = new ShellUtilChangeThemeRequest();
	if (req == NULL)
		return shellUtilSendAsyncResponse(session, requestId, SHELL_UTIL_ERR_NO_MEMORY);
	if (themePath != NULL)
		req->m_themePath = themePath;
	return enqueueAsyncRequestInline(session, requestId, req);
}

/* 0x8124BAC8 */
int shellSvcDisp_0C_Sync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inList, SceUInt32 inCount,
	IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return dispatchSyncMethod(session, methodId, inList, inCount, outList, outCount);
}

/* 0x8124BADC */
int shellSvcDisp_0C_Async(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
	const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	return dispatchAsyncMethod(session, requestId, methodId, inList, inCount);
}

/* 0x8124BAEC */
void shellSvcDisp_0C_Disconn(IPMI::Session *session)
{
	if (s_utilServerMgr != NULL)
		s_utilServerMgr->OnSessionDisconnected(session);
}

/* 0x8124BB02 */
int shellUtilServerMgrCreate(void)
{
	if (s_utilServerMgr != NULL)
		return SHELL_ERROR_ALREADY_CREATED;

	s_utilServerMgr = new ShellUtilServerMgr();
	if (s_utilServerMgr == NULL)
		return SHELL_ERROR_NO_MEMORY;

	return 0;
}
