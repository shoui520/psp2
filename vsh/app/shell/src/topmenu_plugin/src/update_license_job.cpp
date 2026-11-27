/*
 * update_license_job.cpp: UpdateLicenseJob signing into PSN if needed and
 * refreshing an expired application's license and alarm (0x812A05BE-0x812A0D27).
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_NO_GAME_PLUGIN	= -2146420476; /* 0x80103904 */
const SceInt32 TOPMENU_ERROR_SIGNIN_LAUNCH_FAIL	= -2146420425; /* 0x80103937 */
const SceInt32 TOPMENU_ERROR_SIGNIN_CANCELED	= -2146420424; /* 0x80103938 */
const SceUInt32 MSG_UPDATING_LICENSE_WAIT	= 0x632BDF33u; /* 1663788851 */

typedef int (*GamePluginLicenseUpdateFunc)(const char *path, const char *titleId);
typedef int (*GamePluginAlarmSetFunc)(const char *path, const TopmenuLicenseCheckResult *status);

static int onLicenseSignInCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	SceInt32 *doneFlag = static_cast<SceInt32 *>(userData);
	if (doneFlag != NULL)
		*doneFlag = 1;
	return 0;
}

/* (0x812A0C32 / 0x812A0C72) Error dialog handler when license update fails */
class LicenseUpdateErrorHandler : public CommonGuiDialogHandler {
public:
	explicit LicenseUpdateErrorHandler(SceInt32 pageKey)
		: m_pageKey(pageKey),
		  m_dialogId(-1)
	{
		topmenuPushPowerKeyHandler(topmenuDialogPowerKeyCallback, this);
		topmenuAddTabLeaveListener(m_pageKey, topmenuOnLicenseDialogTabLeave, this);
	}

	virtual ~LicenseUpdateErrorHandler()
	{
		topmenuPopPowerKeyHandler(topmenuDialogPowerKeyCallback, this);
		topmenuRemoveTabLeaveListener(m_pageKey, topmenuOnLicenseDialogTabLeave, this);
	}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)button;
		SceCommonGuiDialog_BC311DC0(dialogId);
	}

	SceInt32 m_pageKey;	/* +0x04 */
	SceInt32 m_dialogId;	/* +0x08 */
};

class UpdateLicenseJob : public paf::job::JobItem {
public:
	explicit UpdateLicenseJob(const LsdbAppInfoPtr *appInfo)
		: paf::job::JobItem("UpdateLicenseJob", NULL),
		  m_appInfo(*appInfo),
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

	virtual ~UpdateLicenseJob()
	{
		if (m_busyHeld) {
			topmenuEndBusy();
			m_busyHeld = false;
		}
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}
	}

	/* (0x812A0BD6) */
	static void OnSignInAppEvent(int eventCode, const void *data, SceSize size, void *arg)
	{
		(void)size;
		const ShellAppEvent *ev = static_cast<const ShellAppEvent *>(data);
		UpdateLicenseJob *self = static_cast<UpdateLicenseJob *>(arg);
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

	/* (0x812A06D6) */
	virtual int32_t Run()
	{
		paf::thread::RMutex::main_thread_mutex.Lock();
		SceInt32 signInState = topmenuGetPsnSignInState();
		paf::thread::RMutex::main_thread_mutex.Unlock();

		if (signInState != 1) {
			paf::thread::Sleep(500);
			SceUID cbId = sceKernelCreateCallback(
				"SceShellUpdaeLicenseCallback", 0,
				onLicenseSignInCallback, &m_signInDone);
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

		GamePluginInterface *gameIf = g_topWindow->m_gameIf;
		if (gameIf == NULL)
			return TOPMENU_ERROR_NO_GAME_PLUGIN;

		const char *origPath = sceLsdbGetOriginalPath(&m_appInfo);
		const char *titleName = sceLsdbGetName(&m_appInfo);
		GamePluginLicenseUpdateFunc updateLic =
			reinterpret_cast<GamePluginLicenseUpdateFunc>(gameIf->func[29]);
		int ret = updateLic(origPath, titleName);
		if (ret >= 0) {
			const char *alarmPath = sceLsdbGetOriginalPath(&m_appInfo);
			GamePluginAlarmSetFunc setAlarm =
				reinterpret_cast<GamePluginAlarmSetFunc>(gameIf->func[25]);
			ret = setAlarm(alarmPath, NULL);
		}
		return ret;
	}

	/* (0x812A0A34) */
	virtual void Finish(int32_t result)
	{
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}

		if (result >= 0) {
			const char *titleName = sceLsdbGetName(&m_appInfo);
			sceAppMgrLaunchAppByNameForShell(0x20000, titleName, NULL);
		} else {
			const char *titleName = sceLsdbGetName(&m_appInfo);
			TopTab *tab = topmenuFindTabByTitleId(g_topWindow, titleName);
			SceInt32 pageKey = (tab != NULL) ? tab->m_pageKey : -1;
			LicenseUpdateErrorHandler *handler = new LicenseUpdateErrorHandler(pageKey);
			handler->m_dialogId = topmenuShowAppError(result, &m_appInfo, handler,
								  NULL, NULL, NULL, NULL, NULL);
		}
	}

	LsdbAppInfoPtr m_appInfo;	/* +0x1C */
	SceInt32 m_waitDialogId;	/* +0x24 */
	SceInt32 m_signInDone;		/* +0x28 */
	bool m_busyHeld;		/* +0x2C */
	bool m_signInEnded;		/* +0x2D */
};

typedef char UpdateLicenseJobSizeCheck[sizeof(UpdateLicenseJob) == 48 ? 1 : -1];

} /* namespace */

/* (0x812A0AEA) */
void topmenuStartUpdateLicenseJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				  const LsdbAppInfoPtr *appInfo)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new UpdateLicenseJob(appInfo));
	*outJob = job;
	topmenuEnqueueInstallJob(&job);
}
