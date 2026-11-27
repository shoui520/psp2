/*
 * psn_signin_dialog_job.cpp: PsnSignInDialogJob launching NPXS10016 ("signin")
 * for offline FTT license checks and re-running AppCheckJob on success
 * (0x8129F862-0x8129FE25).
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_SIGNIN_ABORTED	= -2146420475; /* 0x80103905 */
const SceInt32 TOPMENU_ERROR_SIGNIN_FAILED	= -2146420418; /* 0x8010393E */

static int onSignInFinishedCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	SceInt32 *doneFlag = static_cast<SceInt32 *>(userData);
	if (doneFlag != NULL)
		*doneFlag = 1;
	return 0;
}

/* (0x8129FC06) Signaled when launch itself fails with a negative error */
static int onSignInLaunchErrorCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId;
	(void)notifyCount;
	SceInt32 *errFlag = static_cast<SceInt32 *>(userData);
	if (errFlag != NULL && notifyArg < 0)
		*errFlag = 1;
	return 0;
}

class PsnSignInDialogJob : public paf::job::JobItem {
public:
	explicit PsnSignInDialogJob(TopmenuAppProcess *process)
		: paf::job::JobItem("PsnSignInDialogJob", NULL),
		  m_signInDone(0),
		  m_launchError(0),
		  m_busyHeld(false),
		  m_signInEnded(false),
		  m_process(process)
	{
		paf::thread::RMutex::main_thread_mutex.Lock();
		if (!m_busyHeld) {
			topmenuBeginBusy();
			m_busyHeld = true;
		}
		paf::thread::RMutex::main_thread_mutex.Unlock();
	}

	virtual ~PsnSignInDialogJob() {}

	/* (0x8129FD58) */
	static void OnSignInAppEvent(int eventCode, const void *data, SceSize size, void *arg)
	{
		(void)size;
		const ShellAppEvent *ev = static_cast<const ShellAppEvent *>(data);
		PsnSignInDialogJob *self = static_cast<PsnSignInDialogJob *>(arg);
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

	/* (0x8129F89E) */
	virtual int32_t Run()
	{
		paf::thread::RMutex::main_thread_mutex.Lock();
		SceInt32 signInState = topmenuGetPsnSignInState();
		paf::thread::RMutex::main_thread_mutex.Unlock();

		if (signInState == 1)
			return 0;

		paf::thread::Sleep(500);
		SceUID paramCbId = sceKernelCreateCallback(
			"SceShellPsnSignInDialogJob", 0,
			onSignInFinishedCallback, &m_signInDone);
		SceUID launchCbId = sceKernelCreateCallback(
			"SceShellPsnSignInDialogJob2", 0,
			onSignInLaunchErrorCallback, &m_launchError);
		if ((paramCbId | launchCbId) < 0) {
			if (paramCbId >= 0)
				sceKernelDeleteCallback(paramCbId);
			if (launchCbId >= 0)
				sceKernelDeleteCallback(launchCbId);
			return TOPMENU_ERROR_SIGNIN_FAILED;
		}

		AppmgrLaunchAppByNameOption opt;
		sce_paf_memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.launchMode = 0x20000;
		opt.launchCallbackId = launchCbId;
		opt.paramCallbackId = paramCbId;
		opt.creatorAppId = -1;

		SceShell_83D16CA8(5, OnSignInAppEvent, this);
		SceShell_83D16CA8(6, OnSignInAppEvent, this);
		SceShell_83D16CA8(4, OnSignInAppEvent, this);
		SceShell_83D16CA8(0, OnSignInAppEvent, this);

		if (sceAppMgrLaunchAppByName2("NPXS10016", "signin", &opt) != 0) {
			sceKernelDeleteCallback(paramCbId);
			sceKernelDeleteCallback(launchCbId);
			SceShell_E3D3A162(5, OnSignInAppEvent, this);
			SceShell_E3D3A162(6, OnSignInAppEvent, this);
			SceShell_E3D3A162(4, OnSignInAppEvent, this);
			SceShell_E3D3A162(0, OnSignInAppEvent, this);
			return TOPMENU_ERROR_SIGNIN_FAILED;
		}

		while (m_signInDone == 0 && m_launchError == 0) {
			sceKernelCheckCallback();
			if (m_signInDone != 0 || m_launchError != 0)
				break;
			paf::thread::Sleep(16);
		}
		sceKernelDeleteCallback(paramCbId);
		sceKernelDeleteCallback(launchCbId);

		if (m_launchError != 0) {
			SceShell_E3D3A162(5, OnSignInAppEvent, this);
			SceShell_E3D3A162(6, OnSignInAppEvent, this);
			SceShell_E3D3A162(4, OnSignInAppEvent, this);
			SceShell_E3D3A162(0, OnSignInAppEvent, this);
			return TOPMENU_ERROR_SIGNIN_ABORTED;
		}

		SceAppMgrAppStatus appStatus;
		while (sceAppMgrGetStatusByName("NPXS10016", &appStatus) == 0)
			paf::thread::Sleep(16);

		paf::thread::Sleep(500);

		SceShell_E3D3A162(5, OnSignInAppEvent, this);
		SceShell_E3D3A162(6, OnSignInAppEvent, this);
		SceShell_E3D3A162(4, OnSignInAppEvent, this);
		SceShell_E3D3A162(0, OnSignInAppEvent, this);

		paf::thread::RMutex::main_thread_mutex.Lock();
		signInState = topmenuGetPsnSignInState();
		paf::thread::RMutex::main_thread_mutex.Unlock();

		return (signInState == 1) ? 0 : TOPMENU_ERROR_SIGNIN_FAILED;
	}

	/* (0x8129FAE0) */
	virtual void Finish(int32_t result)
	{
		if (m_busyHeld) {
			topmenuEndBusy();
			m_busyHeld = false;
		}

		if (result == 0) {
			paf::common::SharedPtr<paf::job::JobItem> checkJob;
			topmenuStartAppCheckJob(&checkJob, m_process);
			m_process->m_checkJob = checkJob;
		} else {
			SceInt32 pageKey = m_process->m_tabAppData->m_ownerTab->m_pageKey;
			topmenuCancelLiveAreaOpen(pageKey);
			topmenuDeleteTabByPageKey(pageKey);
		}
	}

	SceInt32 m_signInDone;		/* +0x1C */
	SceInt32 m_launchError;		/* +0x20 */
	bool m_busyHeld;		/* +0x24 */
	bool m_signInEnded;		/* +0x25 */
	TopmenuAppProcess *m_process;	/* +0x28 */
};

typedef char PsnSignInDialogJobSizeCheck[sizeof(PsnSignInDialogJob) == 44 ? 1 : -1];

} /* namespace */

/* (0x8129FC14) */
void topmenuStartPsnSignInDialogJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				    TopmenuAppProcess *proc)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new PsnSignInDialogJob(proc));
	*outJob = job;
	topmenuEnqueueInstallJob(&job);
}
