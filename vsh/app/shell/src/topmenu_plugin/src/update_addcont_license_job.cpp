/*
 * update_addcont_license_job.cpp: UpdateAddContLicenseJob for refreshing
 * additional content (DLC) licenses after PSN sign-in (0x8129F13A-0x8129F861).
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_NO_GAME_PLUGIN	= -2146420476; /* 0x80103904 */
const SceInt32 TOPMENU_ERROR_SIGNIN_LAUNCH_FAIL	= -2146420425; /* 0x80103937 */
const SceInt32 TOPMENU_ERROR_SIGNIN_CANCELED	= -2146420424; /* 0x80103938 */
const SceUInt32 MSG_UPDATING_LICENSE_WAIT	= 0x632BDF33u; /* 1663788851 */

typedef int (*GamePluginAddcontLicenseUpdateFunc)(const char *titleId);

/* (0x8129F678) Callback signaled when NPXS10016 sign-in completes */
static int onSignInCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	SceInt32 *doneFlag = static_cast<SceInt32 *>(userData);
	if (doneFlag != NULL)
		*doneFlag = 1;
	return 0;
}

class UpdateAddContLicenseJob : public paf::job::JobItem {
public:
	UpdateAddContLicenseJob(const char *titleId, SceInt32 launchAppId,
				SceInt32 existingFlags, SceInt32 extraFlags)
		: paf::job::JobItem("UpdateAddContLicenseJob", NULL),
		  m_titleId(titleId != NULL ? titleId : ""),
		  m_launchAppId(launchAppId),
		  m_existingFlags(existingFlags),
		  m_extraFlags(extraFlags),
		  m_waitDialogId(-1),
		  m_signInDone(0),
		  m_busyHeld(false),
		  m_signInEnded(false)
	{
		paf::thread::RMutex::main_thread_mutex.Lock();
		if (!m_busyHeld) {
			topmenuBeginBusy();
			m_busyHeld = true;
		}
		paf::thread::RMutex::main_thread_mutex.Unlock();
	}

	virtual ~UpdateAddContLicenseJob()
	{
		if (m_busyHeld) {
			topmenuEndBusy();
			m_busyHeld = false;
		}
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}
		if (m_launchAppId != -1)
			topmenuCancelPendingAppLaunch(m_launchAppId);
	}

	/* (0x8129F792) Track NPXS10016 lifecycle to pause/resume shell busy overlay */
	static void OnSignInAppEvent(int eventCode, const void *data, SceSize size, void *arg)
	{
		(void)size;
		const ShellAppEvent *ev = static_cast<const ShellAppEvent *>(data);
		UpdateAddContLicenseJob *self = static_cast<UpdateAddContLicenseJob *>(arg);
		if (ev == NULL || ev->titleId == NULL ||
		    sce_paf_strcmp(ev->titleId, "NPXS10016") != 0) {
			return;
		}

		switch (eventCode) {
		case 0:
		case 4:
		case 6:
			if (!self->m_signInEnded) {
				self->m_signInEnded = true;
				if (!self->m_busyHeld) {
					topmenuBeginBusy();
					self->m_busyHeld = true;
				}
			}
			break;
		case 5:
			if (self->m_busyHeld) {
				topmenuEndBusy();
				self->m_busyHeld = false;
			}
			break;
		default:
			break;
		}
	}

	/* (0x8129F2A0) */
	virtual int32_t Run()
	{
		int npStatus = 0;
		if (SceNpManager_BF8921C8(&npStatus) != 0 || npStatus != 1)
			return 0;

		paf::thread::RMutex::main_thread_mutex.Lock();
		SceInt32 signInState = topmenuGetPsnSignInState();
		paf::thread::RMutex::main_thread_mutex.Unlock();

		if (signInState != 1) {
			paf::thread::Sleep(500);
			SceUID cbId = sceKernelCreateCallback(
				"SceShellUpdaeAddContLicenseCallback", 0,
				onSignInCallback, &m_signInDone);
			if (cbId == -1)
				return TOPMENU_ERROR_SIGNIN_LAUNCH_FAIL;

			AppmgrLaunchAppByNameOption opt;
			sce_paf_memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.launchMode = 0x20000;
			opt.launchCallbackId = -1;
			opt.paramCallbackId = cbId;
			opt.creatorAppId = -1;

			SceShell_83D16CA8(5, OnSignInAppEvent, this);
			SceShell_83D16CA8(6, OnSignInAppEvent, this);
			SceShell_83D16CA8(4, OnSignInAppEvent, this);
			SceShell_83D16CA8(0, OnSignInAppEvent, this);

			if (sceAppMgrLaunchAppByName2("NPXS10016", "signin", &opt) != 0) {
				sceKernelDeleteCallback(cbId);
				SceShell_E3D3A162(5, OnSignInAppEvent, this);
				SceShell_E3D3A162(6, OnSignInAppEvent, this);
				SceShell_E3D3A162(4, OnSignInAppEvent, this);
				SceShell_E3D3A162(0, OnSignInAppEvent, this);
				return TOPMENU_ERROR_SIGNIN_LAUNCH_FAIL;
			}

			while (m_signInDone == 0) {
				sceKernelCheckCallback();
				if (m_signInDone != 0)
					break;
				paf::thread::Sleep(16);
			}
			sceKernelDeleteCallback(cbId);

			SceAppMgrAppStatus appStatus;
			while (sceAppMgrGetStatusByName("NPXS10016", &appStatus) == 0)
				paf::thread::Sleep(16);

			SceShell_E3D3A162(5, OnSignInAppEvent, this);
			SceShell_E3D3A162(6, OnSignInAppEvent, this);
			SceShell_E3D3A162(4, OnSignInAppEvent, this);
			SceShell_E3D3A162(0, OnSignInAppEvent, this);

			paf::thread::RMutex::main_thread_mutex.Lock();
			signInState = topmenuGetPsnSignInState();
			paf::thread::RMutex::main_thread_mutex.Unlock();
		}

		if (signInState != 1)
			return TOPMENU_ERROR_SIGNIN_CANCELED;

		SceRtcTick secureTick;
		if (vshRtcGetCurrentSecureTick(&secureTick) < 0)
			return 0;

		paf::thread::RMutex::main_thread_mutex.Lock();
		CommonGuiLayoutDialogParam param;
		param.resourcePlugin = g_topmenuPlugin;
		param.priority = 33;
		param.layout = SceCommonGuiDialog_2A68B4A4;
		const wchar_t *waitStr =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_UPDATING_LICENSE_WAIT));
		param.text = (waitStr != NULL) ? waitStr : L"";
		m_waitDialogId = SceCommonGuiDialog_CCFCD9A6(&param);
		paf::thread::RMutex::main_thread_mutex.Unlock();

		if (g_topWindow->m_gameIf == NULL)
			return TOPMENU_ERROR_NO_GAME_PLUGIN;

		GamePluginAddcontLicenseUpdateFunc updateAddcont =
			reinterpret_cast<GamePluginAddcontLicenseUpdateFunc>(
				g_topWindow->m_gameIf->func[46]);
		return updateAddcont(m_titleId.c_str());
	}

	/* (0x8129F602) */
	virtual void Finish(int32_t result)
	{
		(void)result;
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}

		TopmenuAppProcess *proc =
			topmenuFindProcessByTitleId(g_topWindow, m_titleId.c_str());
		if (proc != NULL) {
			proc->ResumeWithArg(m_launchAppId, m_existingFlags | m_extraFlags);
			m_launchAppId = -1;
			if (proc == g_topWindow->m_foregroundApp)
				proc->SendProcessCommand(1);
			else
				topmenuSwitchToProcess(g_topWindow, proc, true);
		}

		if (m_launchAppId != -1) {
			topmenuCancelPendingAppLaunch(m_launchAppId);
			m_launchAppId = -1;
		}
	}

	paf::string m_titleId;		/* +0x1C */
	SceInt32 m_launchAppId;		/* +0x28 */
	SceInt32 m_existingFlags;	/* +0x2C */
	SceInt32 m_extraFlags;		/* +0x30 */
	SceInt32 m_waitDialogId;	/* +0x34 */
	SceInt32 m_signInDone;		/* +0x38 */
	bool m_busyHeld;		/* +0x3C */
	bool m_signInEnded;		/* +0x3D */
};

typedef char UpdateAddContLicenseJobSizeCheck[sizeof(UpdateAddContLicenseJob) == 64 ? 1 : -1];

} /* namespace */

/* (0x8129F682) */
void topmenuStartUpdateAddcontLicenseJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
					 const char *titleId, SceInt32 launchAppId,
					 SceInt32 existingFlags, SceInt32 extraFlags)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new UpdateAddContLicenseJob(titleId, launchAppId, existingFlags, extraFlags));
	*outJob = job;
	topmenuEnqueueInstallJob(&job);
}
