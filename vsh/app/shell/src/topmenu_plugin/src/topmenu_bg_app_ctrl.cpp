/*
 * topmenu_bg_app_ctrl.cpp: TopmenuProcessBase lifecycle, background partner
 * teardown (NPXS10083/NPXS10084), app event broadcasts, and spawn coordination
 * (0x8129A0E0-0x8129AB7F).
 */
#include "topmenu_internal.h"
#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif

namespace {

const SceInt32 SHELL_APP_EVENT_LIVEAREA_OPEN	= 1;
const SceInt32 SHELL_APP_EVENT_LIVEAREA_CLOSE	= 2;
const SceInt32 SHELL_APP_EVENT_SHOW_APP		= 7;
const SceInt32 SHELL_APP_EVENT_HIDE_APP		= 8;

static void sendProcessAppEvent(TopmenuProcessBase *proc, SceInt32 eventCode)
{
	ShellAppActivatedEvent ev;
	ev.processId = (proc->m_procState >= 2) ?
		sceAppMgrGetProcessIdByAppIdForShell(proc->m_appId) : -1;
	ev.appId = proc->m_appId;
	ev.appName = (proc->m_owner != NULL) ? proc->m_owner->m_titleId.c_str() : "";
	ev.fromBoot = false;
	shellAppEventSend(eventCode, &ev, 0);
}

/* (0x8129A59C) Execute an immediate process action when running (m_procState == 3) */
static int executeRunningCommand(TopmenuProcessBase *proc, SceInt32 command)
{
	if (proc->m_procState != 3)
		return 0;
	if (proc->m_appId == -1)
		return 0;

	switch (command) {
	case 1:
		if (!proc->m_isActive) {
			proc->m_isActive = true;
			sceAppMgrActivateApp(proc->m_appId);
			sendProcessAppEvent(proc, SHELL_APP_EVENT_ACTIVATED);
#if !defined(FW_TYPE_CEX)
			ScePafAutoTestTty_9E10E873(
				3, "activate_%s",
				(proc->m_owner != NULL) ? proc->m_owner->m_titleId.c_str() : "");
#endif
			if (proc->m_owner != NULL)
				proc->m_owner->OnResumeForeground();
		}
		break;
	case 2:
		if (proc->m_owner != NULL) {
			TopTabAppData *appData = proc->m_owner->m_tabAppData;
			if (appData != NULL && appData->m_viewKind == 1) {
				LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
				if (liveAreaIf != NULL && liveAreaIf->setGateSurface != NULL) {
					paf::intrusive_ptr<paf::graph::Surface> snap =
						appData->GetScreenCopySurface();
					paf::graph::Surface *rawSurf = snap.get();
					liveAreaIf->setGateSurface(appData->m_ownerTab->m_pageKey,
								   &rawSurf);
				}
			}
		}
		if (proc->m_isActive) {
			proc->m_isActive = false;
			if (proc->m_owner != NULL)
				proc->m_owner->OnSuspendBackground();
			sceAppMgrDeactivateApp(proc->m_appId);
			g_topWindow->m_launchSeq.state = 2;
			sendProcessAppEvent(proc, SHELL_APP_EVENT_DEACTIVATED);
#if !defined(FW_TYPE_CEX)
			ScePafAutoTestTty_9E10E873(
				3, "deactivate_%s",
				(proc->m_owner != NULL) ? proc->m_owner->m_titleId.c_str() : "");
#endif
		}
		break;
	case 4:
		sceAppMgrNotifyLiveBoardModeForShell(proc->m_appId, 1);
		break;
	case 5:
		sceAppMgrDestroyAppByAppId(proc->m_appId);
		topmenuProcessResetAndCancel(proc);
		return 1;
	default:
		return 0;
	}
	return 1;
}

} /* namespace */

/* (0x8129A0E0) */
TopmenuProcessBase::TopmenuProcessBase(TopmenuAppProcess *owner)
	: m_owner(owner),
	  m_enabled(true),
	  m_spawnJob(),
	  m_pendingAction(0),
	  m_procState(0),
	  m_wantActive(false),
	  m_isActive(false),
	  m_suspended(false),
	  m_hasAppId(false),
	  m_appId(-1),
	  m_liveAreaFocus(false),
	  m_bootTimeStartUs(0)
{
	topmenuProcessListAdd(this);
}

/* (0x8129A110) */
TopmenuProcessBase::~TopmenuProcessBase()
{
	topmenuProcessListRemove(this);
}

/* (0x8129A16C) */
LsdbAppInfoPtr *topmenuProcessGetAppInfo(TopmenuProcessBase *proc)
{
	return &proc->m_owner->m_appInfo;
}

/* (0x8129A172) */
void topmenuProcessSetState(TopmenuProcessBase *proc, SceInt32 state)
{
	proc->m_procState = state;
	if (proc->m_owner == NULL)
		return;

	if (state == 3 || state == 0) {
		if (SceLsdb_849E8DC6(&proc->m_owner->m_appInfo) == 0 &&
		    SceLsdb_9D27A87D(&proc->m_owner->m_appInfo) == 1) {
			TopTabAppData *appData = proc->m_owner->m_tabAppData;
			if (appData != NULL && appData->m_ownerTab != NULL)
				appData->m_ownerTab->m_lockedByApp = (state == 3);
		}
	}
}

/* (0x8129A1FA) */
const char *topmenuProcessGetTitleId(const TopmenuProcessBase *proc)
{
	if (proc->m_owner != NULL)
		return proc->m_owner->m_titleId.c_str();
	return "";
}

/* (0x8129A20C) */
SceUID topmenuProcessGetPid(const TopmenuProcessBase *proc)
{
	if (proc->m_procState >= 2)
		return sceAppMgrGetProcessIdByAppIdForShell(proc->m_appId);
	return -1;
}

/* (0x8129A222) */
void topmenuProcessSetLiveAreaFocus(TopmenuProcessBase *proc, bool focused)
{
	if (proc->m_liveAreaFocus == focused)
		return;

	SceInt32 eventCode = proc->m_liveAreaFocus ?
		SHELL_APP_EVENT_LIVEAREA_OPEN : SHELL_APP_EVENT_LIVEAREA_CLOSE;
	sendProcessAppEvent(proc, eventCode);
	proc->m_liveAreaFocus = focused;
}

/* (0x8129A294) */
void topmenuProcessActivateLiveArea(TopmenuProcessBase *proc)
{
	if (proc->m_appId > 0)
		topmenuCloseCrashDialogForApp(proc->m_appId);

	if (proc->m_liveAreaFocus) {
		sendProcessAppEvent(proc, SHELL_APP_EVENT_LIVEAREA_OPEN);
		proc->m_liveAreaFocus = false;
	}

	if (proc->m_owner != NULL && proc->m_owner->m_launchedBgPartner) {
		SceAppMgrAppStatus status;
		if (sceAppMgrGetStatusByName("NPXS10083", &status) == 0 &&
		    proc->m_appId == status.launchSourceAppId) {
			sceAppMgrDestroyAppByName("NPXS10083");
		}
		if (sceAppMgrGetStatusByName("NPXS10084", &status) == 0 &&
		    proc->m_appId == status.launchSourceAppId) {
			sceAppMgrDestroyAppByName("NPXS10084");
		}
	}

	proc->m_appId = -1;
	topmenuProcessSetState(proc, 0);
	proc->m_pendingAction = 0;
	proc->m_wantActive = false;
	proc->m_isActive = false;
	proc->m_suspended = false;
	proc->m_spawnJob.reset();
	proc->m_hasAppId = false;
}

/* (0x8129A3D8) */
TopTabAppData *topmenuProcessGetTabAppData(const TopmenuProcessBase *proc)
{
	if (proc->m_owner != NULL)
		return proc->m_owner->m_tabAppData;
	return NULL;
}

/* (0x8129A3E4 / 0x8129A46E) */
void topmenuProcessResetAndCancel(TopmenuProcessBase *proc)
{
	if (proc->m_owner != NULL) {
		TopTabAppData *appData = proc->m_owner->m_tabAppData;
		if (appData != NULL && appData->m_viewKind == 1) {
			const char *bgTitleId = proc->m_owner->GetBgTitleId();
			if (bgTitleId == NULL ||
			    !topmenuHasRunningAppWithTitleId(g_topWindow, bgTitleId) ||
			    (topmenuGetAppFlags(g_topWindow, bgTitleId) & 0x4000000ULL) != 0) {
				LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
				if (liveAreaIf != NULL && liveAreaIf->clearGateSurface != NULL)
					liveAreaIf->clearGateSurface(appData->m_ownerTab->m_pageKey);
			}
		}

		proc->m_owner->OnAppCheckSucceeded(LsdbAppInfoPtr());
		proc->m_owner->SetParentAppId(-1);
	}

	if (proc->m_spawnJob.get() != NULL)
		proc->m_spawnJob->Cancel();

	topmenuProcessActivateLiveArea(proc);
}

/* (0x8129A476) */
void topmenuProcessOnEnterViewport(TopmenuProcessBase *proc)
{
	if (proc->m_owner == NULL)
		return;

	TopTabAppData *appData = proc->m_owner->m_tabAppData;
	if (appData == NULL || appData->m_viewKind != 3 || appData->m_appViewState == 1)
		return;

	sendProcessAppEvent(proc, SHELL_APP_EVENT_SHOW_APP);

	if (proc->m_wantActive) {
		appData->SetAppViewState(1);
		topmenuSetShellViewMode(g_topWindow, 4);
	} else {
		appData->RefreshLiveAreaInfo();
		proc->m_owner->SendProcessCommand(4);
	}
}

/* (0x8129A510) */
void topmenuProcessOnLeaveViewport(TopmenuProcessBase *proc)
{
	if (proc->m_owner == NULL)
		return;

	TopTabAppData *appData = proc->m_owner->m_tabAppData;
	if (appData == NULL || appData->m_viewKind != 3 || appData->m_appViewState == 2)
		return;

	appData->SetAppViewState(2);
	sendProcessAppEvent(proc, SHELL_APP_EVENT_HIDE_APP);

	if (proc->m_wantActive)
		topmenuSetShellViewMode(g_topWindow, 5);
}

/* (0x8129A6FA) */
int topmenuProcessCommand(TopmenuProcessBase *proc, SceInt32 command)
{
	bool prevWantActive = proc->m_wantActive;
	SceInt32 curState = proc->m_procState;

	if (command == 1)
		proc->m_wantActive = true;
	else if (command == 2)
		proc->m_wantActive = false;

	switch (curState) {
	case 0:
		if (command == 5) {
			topmenuProcessResetAndCancel(proc);
			return 1;
		}
		if (proc->m_owner == NULL)
			return 0;
		if (proc->m_owner->m_tabAppData != NULL &&
		    proc->m_owner->m_tabAppData->m_viewKind != 0) {
			return 0;
		}
		proc->m_pendingAction = command;
		return 1;
	case 1:
	case 2:
		if (command == 5) {
			if (proc->m_appId != -1)
				sceAppMgrDestroyAppByAppId(proc->m_appId);
			topmenuProcessResetAndCancel(proc);
			return 1;
		}
		if (prevWantActive && command == 2 && proc->m_owner == g_topWindow->m_foregroundApp) {
			TopTabAppData *appData = proc->m_owner->m_tabAppData;
			if (appData != NULL && appData->m_viewKind == 1 &&
			    appData->m_screenCopy != NULL) {
				appData->m_screenCopy->CaptureAndPublish();
			}
		}
		proc->m_pendingAction = command;
		return 1;
	case 3:
		return executeRunningCommand(proc, command);
	default:
		return 1;
	}
}

/* (0x8129A7A4) */
int topmenuProcessFlushPendingAction(TopmenuProcessBase *proc)
{
	SceInt32 pending = proc->m_pendingAction;
	if (pending == 0)
		return 0;
	executeRunningCommand(proc, pending);
	proc->m_pendingAction = 0;
	return 1;
}

/* (0x8129A7BE) */
void topmenuProcessClearSpawnJob(TopmenuProcessBase *proc)
{
	if (proc->m_procState == 1) {
		topmenuProcessActivateLiveArea(proc);
		return;
	}
	proc->m_spawnJob.reset();
}

/* (0x8129A828) */
int topmenuProcessSpawn(TopmenuProcessBase *proc, SceInt32 startArg, SceInt32 checkFlags)
{
	if (proc->m_spawnJob.get() != NULL) {
		if (startArg != -1) {
			topmenuCancelPendingAppLaunch(startArg);
			return 0;
		}
		topmenuEnqueueExistingSpawnJob(&proc->m_spawnJob);
		topmenuProcessSetState(proc, 2);
		return 1;
	}

	if (proc->m_procState != 0) {
		if (startArg != -1) {
			topmenuCancelPendingAppLaunch(startArg);
			return 0;
		}
		return 1;
	}

	paf::common::SharedPtr<paf::job::JobItem> job;
	topmenuStartSpawnJob(&job, proc, startArg, checkFlags);
	proc->m_spawnJob = job;
	topmenuProcessSetState(proc, 2);
	return 1;
}

/* (0x8129A966) */
int topmenuProcessLaunchAsync(TopmenuProcessBase *proc, SceInt32 startArg)
{
	if (startArg == -1)
		return 1;
	if (proc->m_spawnJob.get() != NULL)
		return 0;

	paf::common::SharedPtr<paf::job::JobItem> job;
	topmenuCreateSpawnJob(&job, proc, startArg, 0);
	proc->m_spawnJob = job;
	if (proc->m_procState == 0)
		topmenuProcessSetState(proc, 1);
	return 1;
}

/* (0x8129AA7E) */
int topmenuProcessAttachAppId(TopmenuProcessBase *proc, SceInt32 appId)
{
	proc->m_appId = appId;
	topmenuProcessSetState(proc, 3);
	if (appId != -1)
		proc->m_hasAppId = true;
	return 1;
}

/* (0x8129AA9A) */
int topmenuProcessPollSharedFb(TopmenuProcessBase *proc)
{
	if (proc->m_hasAppId)
		return 1;
	if (proc->m_appId == -1)
		return 0;

	SceUID pid = (proc->m_procState >= 2) ?
		sceAppMgrGetProcessIdByAppIdForShell(proc->m_appId) : -1;
	if (sceSharedFbGetRenderingInfo(pid) == 0) {
		proc->m_hasAppId = true;
		return 1;
	}
	return 0;
}

/* (0x8129AAC8) */
bool topmenuProcessIsSpawnedOrRunning(const TopmenuProcessBase *proc)
{
	if (proc->m_procState >= 2)
		return true;
	const char *bgTitle = SceLsdb_457F369E(&proc->m_owner->m_appInfo);
	return (bgTitle != NULL && topmenuHasRunningAppWithTitleId(g_topWindow, bgTitle));
}

/* (0x8129AB02) */
SceInt32 topmenuProcessGetPendingLaunchId(const TopmenuProcessBase *proc)
{
	if (proc->m_spawnJob.get() == NULL)
		return -1;
	return topmenuSpawnJobGetLaunchAppId(proc->m_spawnJob.get());
}
