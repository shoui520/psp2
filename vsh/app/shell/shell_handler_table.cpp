/*
 * shell_handler_table.cpp: SceShell's ShellUtil lock handler table & ShellLockManager
 * (0x81248A10-0x812499F5).
 *
 * Manages 12 system lock bits (0x001..0x800) with per-session reference counting,
 * asynchronous teardown coordination (download_plugin, trigger_mgr, app termination,
 * musiccore), and PS-button notification dispatch to sessions holding bit 11 (0x800).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string.h>
#include <plugin.h>
#include <job/job.h>
#include <thread/thread.h>
#include <common/main_thread_call_list.h>
#include <inputdevice/pad/pad.h>
#include <appmgr/appmgr.h>

#include "shell.h"
#include "plugins.h"
#include "theme.h"
#include "shell_util_server.h"
#include "src/power_manage_plugin/src/power_manage_internal.h"
#include "src/download_plugin/src/download_plugin.h"

using namespace paf;

extern "C" {
extern SceUID sceAppMgrGetProcessIdByAppIdForShell(int appId);
extern int sceAppMgrDestroyOtherAppByPidForShell(SceUID keepPid);
extern int sceAppMgrGetPidListForShell(SceUID *pidList, int maxCount);
}

/* External shell helpers in other TUs */
extern int shellParentalControlPause(void);
extern int shellParentalControlResume(void);
extern int mediaDetectMaskEvent(int eventBit);
extern int mediaDetectUnmaskEvent(int eventBit);
extern int musicCorePlayerControl(SceUInt8 command, int param);
struct MusicCoreImposeInfo;
extern int musicCoreGetImposeInfo(MusicCoreImposeInfo *outInfo);
extern int shellSvcNotifyEvent(IPMI::Session *session, int eventIndex);
extern int shellTriggerMgrSuspend(void (*callback)(void *), void *userData);
extern int shellTriggerMgrResume(void (*callback)(void *), void *userData);

namespace {

enum {
	SHELL_LOCK_OP_LOCK		= 1,
	SHELL_LOCK_OP_UNLOCK		= 2,
	SHELL_LOCK_OP_UNLOCK_ALL	= 3,

	SHELL_PS_BTN_BASE_EVENT		= 53,
	SHELL_MAX_PAD_PORT		= 4,
	SHELL_MAX_PID_LIST		= 20,
	SHELL_MUSIC_IMPOSE_INFO_SIZE	= 6176,
};

/* MusicCoreImposeInfo header: playback stopped flag sits at byte offset 4 */
struct MusicImposeStatusHeader {
	SceUInt32 status;
	SceUInt8 isStopped;
	SceUInt8 tail[SHELL_MUSIC_IMPOSE_INFO_SIZE - 5];
};

static ShellLockManager *s_lockManager = NULL;

/* Forward declarations for static callbacks */
static int notifyAsyncLockStepDone(void *userData);
static int asyncLockCompletionTrampoline(void *userData);
static void pollOtherAppsExited(void *keepPidArg);
static void pollMusicStoppedForUmount(void *userData);
static void onPsButtonKeyPressed(void *userData);
static void onShellAppDeactivatedEvent(int eventCode, const void *eventData,
	SceSize eventSize, void *userData);

/* Background job: waits for liblocation readiness once other apps exit (0x812499EC) */
class WaitLiblocationReadyJob : public job::JobItem {
public:
	WaitLiblocationReadyJob() : job::JobItem(NULL, NULL) {}
	virtual ~WaitLiblocationReadyJob() {}

	virtual int Run()
	{
		return shellLiblocationWaitReady();
	}

	virtual void Finish(int result)
	{
		(void)result;
		thread::SyncCall::main_thread_synccall.Call(notifyAsyncLockStepDone, NULL);
	}
};

/* Bit 0 (0x001): parental control pause + topmenu game hold (0x81249474 / 0x8124949C) */
static int lockGameSession(SceUID clientPid)
{
	(void)clientPid;
	shellParentalControlPause();
	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
		if (iface != NULL)
			iface->func[TOPMENU_PLUGIN_FUNC_GAME_HOLD]();
	}
	return 0;
}

static int unlockGameSession(SceUID clientPid)
{
	(void)clientPid;
	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
		if (iface != NULL)
			iface->func[TOPMENU_PLUGIN_FUNC_GAME_RELEASE]();
	}
	shellParentalControlResume();
	return 0;
}

/* Bit 1 (0x002): PS button lock (0x812494C6 / 0x812494EA) */
static int lockPsButtonHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->lockPsButton();
	}
	return 0;
}

static int unlockPsButtonHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->unlockPsButton();
	}
	return 0;
}

/* Bit 2 (0x004): Power button lock (0x8124950E / 0x81249532) */
static int lockPowerButtonHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->lockPowerButton();
	}
	return 0;
}

static int unlockPowerButtonHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->unlockPowerButton();
	}
	return 0;
}

/* Bit 3 (0x008): Power switch hold (0x81249556 / 0x8124957A) */
static int lockPowerSwitchHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->holdPowerSwitch();
	}
	return 0;
}

static int unlockPowerSwitchHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->releasePowerSwitch();
	}
	return 0;
}

/* Bit 4 (0x010): Topmenu hold (0x8124959E / 0x812495C4) */
static int lockTopmenuHoldHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
		if (iface != NULL)
			iface->func[TOPMENU_PLUGIN_FUNC_HOLD_BEGIN]();
	}
	return 0;
}

static int unlockTopmenuHoldHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
		if (iface != NULL)
			iface->func[TOPMENU_PLUGIN_FUNC_HOLD_END]();
	}
	return 0;
}

/* Bit 5 (0x020): Mask memory card mount/unmount events (0x812495EA / 0x812495FC) */
static int lockMemoryCardDetectHandler(SceUID clientPid)
{
	(void)clientPid;
	mediaDetectMaskEvent(0);
	mediaDetectMaskEvent(4);
	return 0;
}

static int unlockMemoryCardDetectHandler(SceUID clientPid)
{
	(void)clientPid;
	mediaDetectUnmaskEvent(0);
	mediaDetectUnmaskEvent(4);
	return 0;
}

/* Bit 6 (0x040): Mask game card mount/unmount events (0x8124960E / 0x81249620) */
static int lockGameCardDetectHandler(SceUID clientPid)
{
	(void)clientPid;
	mediaDetectMaskEvent(2);
	mediaDetectMaskEvent(6);
	return 0;
}

static int unlockGameCardDetectHandler(SceUID clientPid)
{
	(void)clientPid;
	mediaDetectUnmaskEvent(2);
	mediaDetectUnmaskEvent(6);
	return 0;
}

/* Bit 7 (0x080): Mask media I/O error event (0x81249632 / 0x8124963E) */
static int lockMediaIoErrorHandler(SceUID clientPid)
{
	(void)clientPid;
	mediaDetectMaskEvent(10);
	return 0;
}

static int unlockMediaIoErrorHandler(SceUID clientPid)
{
	(void)clientPid;
	mediaDetectUnmaskEvent(10);
	return 0;
}

/* Bit 8 (0x100): Suspend/resume background downloads (0x8124964A / 0x81249682) */
static void asyncLockCallbackAdapter(void *userData)
{
	asyncLockCompletionTrampoline(userData);
}

static int lockBackgroundDownloadsHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *dl = Plugin::Find("download_plugin");
	if (dl != NULL) {
		DownloadPluginInterface *iface = (DownloadPluginInterface *)dl->GetInterface(1);
		if (iface != NULL)
			return iface->Notify(asyncLockCallbackAdapter, NULL) == 0 ? 1 : 0;
	}
	return 0;
}

static int unlockBackgroundDownloadsHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *dl = Plugin::Find("download_plugin");
	if (dl != NULL) {
		DownloadPluginInterface *iface = (DownloadPluginInterface *)dl->GetInterface(1);
		if (iface != NULL)
			return iface->SetPermission(asyncLockCallbackAdapter, NULL) == 0 ? 1 : 0;
	}
	return 0;
}

/* Bit 9 (0x200): Suspend/resume trigger manager (0x812496BA / 0x812496D6) */
static int lockTriggerMgrHandler(SceUID clientPid)
{
	(void)clientPid;
	return shellTriggerMgrSuspend(asyncLockCallbackAdapter, NULL) == 0 ? 1 : 0;
}

static int unlockTriggerMgrHandler(SceUID clientPid)
{
	(void)clientPid;
	return shellTriggerMgrResume(asyncLockCallbackAdapter, NULL) == 0 ? 1 : 0;
}

/* Bit 10 (0x400): App exclusive / storage umount & remount (0x812496F2 / 0x81249754) */
static int lockStorageExclusiveHandler(SceUID clientPid)
{
	int pendingSteps = 0;

	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
		if (iface != NULL)
			iface->func[TOPMENU_PLUGIN_FUNC_BUSY_BEGIN]();
	}

	if (sceAppMgrDestroyOtherAppByPidForShell(clientPid) == 0) {
		common::MainThreadCallList::Register(pollOtherAppsExited,
			(void *)(intptr_t)clientPid);
		pendingSteps = 1;
	}

	if (musicCorePlayerControl(2, 0) == 0) {
		common::MainThreadCallList::Register(pollMusicStoppedForUmount, NULL);
		++pendingSteps;
	}

	return pendingSteps;
}

static int unlockStorageExclusiveHandler(SceUID clientPid)
{
	(void)clientPid;
	shellStartAppDataMount();

	Plugin *topmenu = Plugin::Find("topmenu_plugin");
	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);
		if (iface != NULL)
			iface->func[TOPMENU_PLUGIN_FUNC_BUSY_END]();
	}

	shellThemeCheckLicense();
	return 0;
}

/* Bit 11 (0x800): PS button key handler intercept (0x81249782 / 0x812497B2) */
static int lockPsButtonEventHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->pushKeyHandler(onPsButtonKeyPressed, NULL);
	}
	return 0;
}

static int unlockPsButtonEventHandler(SceUID clientPid)
{
	(void)clientPid;
	Plugin *pm = Plugin::Find("power_manage_plugin");
	if (pm != NULL) {
		power_manage::PowerManageInterface *iface =
			(power_manage::PowerManageInterface *)pm->GetInterface(1);
		if (iface != NULL)
			iface->popKeyHandler(onPsButtonKeyPressed, NULL);
	}
	return 0;
}

/* 0x812497E4 */
static int asyncLockCompletionTrampoline(void *userData)
{
	(void)userData;
	return thread::SyncCall::main_thread_synccall.Call(notifyAsyncLockStepDone, NULL);
}

/* 0x812497FE */
static void pollOtherAppsExited(void *keepPidArg)
{
	SceUID keepPid = (SceUID)(intptr_t)keepPidArg;
	SceUID pidList[SHELL_MAX_PID_LIST];
	int count = sceAppMgrGetPidListForShell(pidList, SHELL_MAX_PID_LIST);
	SceUID shellPid = sceAppMgrGetProcessIdByAppIdForShell(-2);

	for (int i = 0; i < count; ++i) {
		if (pidList[i] != keepPid && pidList[i] != shellPid)
			return;
	}

	common::SharedPtr<job::JobItem> jobItem(new WaitLiblocationReadyJob());
	if (jobItem.get() != NULL) {
		common::MainThreadCallList::Unregister(pollOtherAppsExited, keepPidArg);
		job::JobQueue::default_queue->Enqueue(jobItem);
	}
}

/* 0x8124991E */
static void pollMusicStoppedForUmount(void *userData)
{
	MusicImposeStatusHeader *imposeInfo = new MusicImposeStatusHeader;
	if (imposeInfo == NULL)
		return;

	sce_paf_memset(imposeInfo, 0, sizeof(*imposeInfo));
	if (musicCoreGetImposeInfo(reinterpret_cast<MusicCoreImposeInfo *>(imposeInfo)) == 0 &&
	    imposeInfo->isStopped != 0) {
		shellStartAppDataUmount();
		common::MainThreadCallList::Unregister(pollMusicStoppedForUmount, userData);
		thread::SyncCall::main_thread_synccall.Call(notifyAsyncLockStepDone, NULL);
	}
	delete imposeInfo;
}

/* 0x81249978 */
static int notifyAsyncLockStepDone(void *userData)
{
	(void)userData;
	if (s_lockManager != NULL)
		return s_lockManager->OnAsyncStepCompleted();
	return 0;
}

/* 0x8124998A */
static void onPsButtonKeyPressed(void *userData)
{
	(void)userData;
	if (s_lockManager != NULL)
		s_lockManager->OnPsButtonPressed();
}

/* 0x8124999C */
static void onShellAppDeactivatedEvent(int eventCode, const void *eventData,
	SceSize eventSize, void *userData)
{
	(void)eventCode;
	(void)eventSize;
	(void)userData;
	if (s_lockManager != NULL && eventData != NULL) {
		const SceUID *processId = (const SceUID *)eventData;
		s_lockManager->OnAppExited(*processId);
	}
}

} /* anonymous namespace */

/* 0x81248A10 */
ShellLockManager::ShellLockManager()
	: m_pendingAsyncCount(0)
{
	m_slots[0].lockFn = lockGameSession;
	m_slots[0].unlockFn = unlockGameSession;
	m_slots[1].lockFn = lockPsButtonHandler;
	m_slots[1].unlockFn = unlockPsButtonHandler;
	m_slots[2].lockFn = lockPowerButtonHandler;
	m_slots[2].unlockFn = unlockPowerButtonHandler;
	m_slots[3].lockFn = lockPowerSwitchHandler;
	m_slots[3].unlockFn = unlockPowerSwitchHandler;
	m_slots[4].lockFn = lockTopmenuHoldHandler;
	m_slots[4].unlockFn = unlockTopmenuHoldHandler;
	m_slots[5].lockFn = lockMemoryCardDetectHandler;
	m_slots[5].unlockFn = unlockMemoryCardDetectHandler;
	m_slots[6].lockFn = lockGameCardDetectHandler;
	m_slots[6].unlockFn = unlockGameCardDetectHandler;
	m_slots[7].lockFn = lockMediaIoErrorHandler;
	m_slots[7].unlockFn = unlockMediaIoErrorHandler;
	m_slots[8].lockFn = lockBackgroundDownloadsHandler;
	m_slots[8].unlockFn = unlockBackgroundDownloadsHandler;
	m_slots[9].lockFn = lockTriggerMgrHandler;
	m_slots[9].unlockFn = unlockTriggerMgrHandler;
	m_slots[10].lockFn = lockStorageExclusiveHandler;
	m_slots[10].unlockFn = unlockStorageExclusiveHandler;
	m_slots[11].lockFn = lockPsButtonEventHandler;
	m_slots[11].unlockFn = unlockPsButtonEventHandler;

	SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, onShellAppDeactivatedEvent, NULL);
}

/* 0x81248C0A */
ShellLockManager::~ShellLockManager()
{
	SceShell_E3D3A162(SHELL_APP_EVENT_DEACTIVATED, onShellAppDeactivatedEvent, NULL);
}

/* 0x81248CAC */
int ShellLockManager::Lock(SceUInt32 lockMask, IPMI::Session *session, int requestId)
{
	int result = SHELL_UTIL_ERR_INVALID_PARAM;

	if ((lockMask & SHELL_LOCK_ALL_MASK) != 0) {
		result = 0;
		if (m_pendingAsyncCount == 0) {
			bool hasTransitionalSlot = false;
			for (int i = 0; i < SHELL_LOCK_SLOT_COUNT; ++i) {
				if (m_slots[i].active && m_slots[i].totalCount == 0) {
					hasTransitionalSlot = true;
					break;
				}
			}

			bool permitted = true;
			if (!hasTransitionalSlot && session != NULL) {
				SceUID clientPid = session->getProcessId();
				SceUID shellPid = sceAppMgrGetProcessIdByAppIdForShell(-2);
				SceUID frontPid = sceAppMgrGetProcessIdByAppIdForShell(-4);
				if (clientPid != shellPid && clientPid != frontPid) {
					result = SHELL_UTIL_ERR_PERMISSION_DENIED;
					permitted = false;
				}
			}

			if (permitted) {
				for (int i = 0; i < SHELL_LOCK_SLOT_COUNT; ++i) {
					if ((lockMask & (1u << i)) != 0 &&
					    !m_slots[i].active && m_slots[i].totalCount == 0) {
						SceUID clientPid = (session != NULL) ?
							session->getProcessId() : -1;
						m_pendingAsyncCount += m_slots[i].lockFn(clientPid);
						m_slots[i].active = true;
					}
				}

				if (m_pendingAsyncCount == 0) {
					for (int i = 0; i < SHELL_LOCK_SLOT_COUNT; ++i) {
						if ((lockMask & (1u << i)) == 0)
							continue;
						++m_slots[i].totalCount;
						map<IPMI::Session *, int>::iterator it =
							m_slots[i].sessionRefs.find(session);
						if (it == m_slots[i].sessionRefs.end())
							m_slots[i].sessionRefs[session] = 1;
						else
							++it->second;
					}
				}
			}
		}
	}

	if (result != 0 || m_pendingAsyncCount == 0) {
		if (session != NULL && requestId != -1)
			shellUtilSendAsyncResponse(session, requestId, result);
	} else {
		ShellLockPendingOp pending;
		pending.operation = SHELL_LOCK_OP_LOCK;
		pending.lockMask = lockMask;
		pending.session = session;
		pending.requestId = requestId;
		m_pendingOps.push_back(pending);
	}

	return result;
}

/* 0x81248E64 */
int ShellLockManager::Unlock(SceUInt32 lockMask, IPMI::Session *session, int requestId)
{
	int result = SHELL_UTIL_ERR_INVALID_PARAM;

	if ((lockMask & SHELL_LOCK_ALL_MASK) != 0) {
		result = 0;
		if (m_pendingAsyncCount == 0) {
			for (int i = 0; i < SHELL_LOCK_SLOT_COUNT; ++i) {
				if ((lockMask & (1u << i)) != 0 &&
				    m_slots[i].active && m_slots[i].totalCount == 1) {
					map<IPMI::Session *, int>::iterator it =
						m_slots[i].sessionRefs.find(session);
					if (it != m_slots[i].sessionRefs.end()) {
						SceUID clientPid = (session != NULL) ?
							session->getProcessId() : -1;
						m_pendingAsyncCount += m_slots[i].unlockFn(clientPid);
						m_slots[i].active = false;
					}
				}
			}

			if (m_pendingAsyncCount == 0) {
				result = SHELL_UTIL_ERR_NOT_LOCKED;
				for (int i = 0; i < SHELL_LOCK_SLOT_COUNT; ++i) {
					if ((lockMask & (1u << i)) == 0)
						continue;
					map<IPMI::Session *, int>::iterator it =
						m_slots[i].sessionRefs.find(session);
					if (it != m_slots[i].sessionRefs.end()) {
						--m_slots[i].totalCount;
						--it->second;
						if (it->second == 0)
							m_slots[i].sessionRefs.erase(it);
						result = 0;
					}
				}
			}
		}
	}

	if (result != 0 || m_pendingAsyncCount == 0) {
		if (session != NULL && requestId != -1)
			shellUtilSendAsyncResponse(session, requestId, result);
	} else {
		ShellLockPendingOp pending;
		pending.operation = SHELL_LOCK_OP_UNLOCK;
		pending.lockMask = lockMask;
		pending.session = session;
		pending.requestId = requestId;
		m_pendingOps.push_back(pending);
	}

	return result;
}

/* 0x81249030 */
int ShellLockManager::UnlockAll(SceUInt32 lockMask, IPMI::Session *session, int requestId)
{
	int result = SHELL_UTIL_ERR_INVALID_PARAM;

	if ((lockMask & SHELL_LOCK_ALL_MASK) != 0) {
		result = 0;
		if (m_pendingAsyncCount == 0) {
			for (int i = 0; i < SHELL_LOCK_SLOT_COUNT; ++i) {
				if ((lockMask & (1u << i)) != 0 && m_slots[i].active) {
					map<IPMI::Session *, int>::iterator it =
						m_slots[i].sessionRefs.find(session);
					if (it != m_slots[i].sessionRefs.end() &&
					    m_slots[i].totalCount == it->second) {
						SceUID clientPid = (session != NULL) ?
							session->getProcessId() : -1;
						m_pendingAsyncCount += m_slots[i].unlockFn(clientPid);
						m_slots[i].active = false;
					}
				}
			}

			if (m_pendingAsyncCount == 0) {
				result = SHELL_UTIL_ERR_NOT_LOCKED;
				for (int i = 0; i < SHELL_LOCK_SLOT_COUNT; ++i) {
					if ((lockMask & (1u << i)) == 0)
						continue;
					map<IPMI::Session *, int>::iterator it =
						m_slots[i].sessionRefs.find(session);
					if (it != m_slots[i].sessionRefs.end()) {
						m_slots[i].totalCount -= it->second;
						m_slots[i].sessionRefs.erase(it);
						result = 0;
					}
				}
			}
		}
	}

	if (result != 0 || m_pendingAsyncCount == 0) {
		if (session != NULL && requestId != -1)
			shellUtilSendAsyncResponse(session, requestId, result);
	} else {
		ShellLockPendingOp pending;
		pending.operation = SHELL_LOCK_OP_UNLOCK_ALL;
		pending.lockMask = lockMask;
		pending.session = session;
		pending.requestId = requestId;
		m_pendingOps.push_back(pending);
	}

	return result;
}

/* 0x812491EA */
int ShellLockManager::OnAsyncStepCompleted()
{
	--m_pendingAsyncCount;
	if (m_pendingAsyncCount != 0)
		return 0;

	list<ShellLockPendingOp> queuedOps(m_pendingOps);
	m_pendingOps.clear();

	for (list<ShellLockPendingOp>::iterator it = queuedOps.begin();
	     it != queuedOps.end(); ++it) {
		switch (it->operation) {
		case SHELL_LOCK_OP_LOCK:
			Lock(it->lockMask, it->session, it->requestId);
			break;
		case SHELL_LOCK_OP_UNLOCK:
			Unlock(it->lockMask, it->session, it->requestId);
			break;
		case SHELL_LOCK_OP_UNLOCK_ALL:
			UnlockAll(it->lockMask, it->session, it->requestId);
			break;
		default:
			break;
		}
	}
	return 0;
}

/* 0x812492E8 */
int ShellLockManager::OnPsButtonPressed()
{
	int port = inputdevice::pad::GetCurrPort();
	if ((unsigned int)port > SHELL_MAX_PAD_PORT)
		return 0;

	int eventIndex = SHELL_PS_BTN_BASE_EVENT + port;
	map<IPMI::Session *, int> &holders = m_slots[SHELL_LOCK_PS_BUTTON_SLOT].sessionRefs;
	for (map<IPMI::Session *, int>::iterator it = holders.begin();
	     it != holders.end(); ++it) {
		if (it->first != NULL)
			shellSvcNotifyEvent(it->first, eventIndex);
	}
	return 0;
}

/* 0x81249386 */
int ShellLockManager::OnAppExited(SceUID processId)
{
	IPMI::Session *matchedSession = NULL;

	for (list<ShellLockPendingOp>::iterator it = m_pendingOps.begin();
	     it != m_pendingOps.end(); ++it) {
		if (it->session != NULL && it->session->getProcessId() == processId) {
			matchedSession = it->session;
			break;
		}
	}

	if (matchedSession == NULL) {
		for (int i = 0; i < SHELL_LOCK_SLOT_COUNT && matchedSession == NULL; ++i) {
			for (map<IPMI::Session *, int>::iterator it = m_slots[i].sessionRefs.begin();
			     it != m_slots[i].sessionRefs.end(); ++it) {
				if (it->first != NULL && it->first->getProcessId() == processId) {
					matchedSession = it->first;
					break;
				}
			}
		}
	}

	if (matchedSession != NULL)
		UnlockAll(SHELL_LOCK_ALL_MASK, matchedSession, -1);

	return 0;
}

/* 0x81248BB4 */
int shellHandlerTableCreate(void)
{
	if (s_lockManager != NULL)
		return SHELL_ERROR_ALREADY_CREATED;

	s_lockManager = new ShellLockManager();
	if (s_lockManager == NULL)
		return SHELL_ERROR_NO_MEMORY;

	return 0;
}

/* 0x81248BF8 */
ShellLockManager *shellHandlerTableGetInstance(void)
{
	return s_lockManager;
}

/* 0x81248C04 */
SceUInt32 shellHandlerTableGetAllMask(void)
{
	return SHELL_LOCK_ALL_MASK;
}
