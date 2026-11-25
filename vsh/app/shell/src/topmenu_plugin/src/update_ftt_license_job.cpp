/*
 * update_ftt_license_job.cpp: UpdateFttLicenseJob for installing pending FTT
 * license updates from BGDL or game_plugin (0x8129FE26-0x812A05BD).
 */
#include "topmenu_internal.h"

namespace {

const SceUInt32 MSG_UPDATING_LICENSE_WAIT = 0x632BDF33u; /* 1663788851 */

typedef int (*GamePluginLicenseUpdateInstallFunc)(const paf::string &titleId);

/* (0x812A04AE / 0x812A04F4) Error dialog handler closing tab on dismissal */
class FttLicenseErrorHandler : public CommonGuiDialogHandler {
public:
	explicit FttLicenseErrorHandler(SceInt32 pageKey)
		: m_pageKey(pageKey),
		  m_dialogId(-1)
	{
		topmenuPushPowerKeyHandler(topmenuDialogPowerKeyCallback, this);
		topmenuAddTabLeaveListener(m_pageKey, topmenuOnLicenseDialogTabLeave, this);
	}

	virtual ~FttLicenseErrorHandler()
	{
		topmenuPopPowerKeyHandler(topmenuDialogPowerKeyCallback, this);
		topmenuRemoveTabLeaveListener(m_pageKey, topmenuOnLicenseDialogTabLeave, this);
		topmenuDeleteTabByPageKey(m_pageKey);
	}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)button;
		SceCommonGuiDialog_BC311DC0(dialogId);
	}

	SceInt32 m_pageKey;	/* +0x04 */
	SceInt32 m_dialogId;	/* +0x08 */
};

class UpdateFttLicenseJob : public paf::job::JobItem {
public:
	UpdateFttLicenseJob(TopmenuAppProcess *process, const LsdbAppInfoPtr *appInfo)
		: paf::job::JobItem("UpdateFttLicenseJob", NULL),
		  m_process(process),
		  m_waitDialogId(-1),
		  m_busyHeld(false),
		  m_appInfo(*appInfo)
	{
		paf::thread::RMutex::main_thread_mutex.Lock();
		if (!m_busyHeld) {
			topmenuBeginBusy();
			m_busyHeld = true;
		}
		paf::thread::RMutex::main_thread_mutex.Unlock();
	}

	virtual ~UpdateFttLicenseJob()
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

	/* (0x8129FF4E) */
	virtual int32_t Run()
	{
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

		const char *titleId = m_process->m_titleId.c_str();
		paf::string titleCopy(titleId);
		if (shellBgdlTitleHasTasksOfKind(titleCopy.c_str(), 0, 4)) {
			BgdlTaskInfo failedTask;
			bool patched = false;
			return shellBgdlInstallTitleTasksOfKinds(titleId, 0, 4, false,
								 &failedTask, &patched);
		}

		if (g_topWindow->m_gameIf == NULL)
			return 0;

		paf::string updateTitle(titleId);
		GamePluginLicenseUpdateInstallFunc installUpdate =
			reinterpret_cast<GamePluginLicenseUpdateInstallFunc>(
				g_topWindow->m_gameIf->func[59]);
		return installUpdate(updateTitle);
	}

	/* (0x812A0220) */
	virtual void Finish(int32_t result)
	{
		if (m_busyHeld) {
			topmenuEndBusy();
			m_busyHeld = false;
		}
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}

		if (result == 0) {
			paf::common::SharedPtr<paf::job::JobItem> checkJob;
			topmenuStartAppCheckJob(&checkJob, m_process);
			m_process->m_checkJob = checkJob;
		} else {
			SceInt32 pageKey = m_process->m_tabAppData->m_ownerTab->m_pageKey;
			FttLicenseErrorHandler *handler = new FttLicenseErrorHandler(pageKey);
			handler->m_dialogId = topmenuShowAppError(result, &m_appInfo, handler,
								  NULL, NULL, NULL, NULL, NULL);
			topmenuCancelLiveAreaOpen(pageKey);
		}
	}

	TopmenuAppProcess *m_process;	/* +0x1C */
	SceInt32 m_waitDialogId;	/* +0x20 */
	bool m_busyHeld;		/* +0x24 */
	LsdbAppInfoPtr m_appInfo;	/* +0x28 */
};

typedef char UpdateFttLicenseJobSizeCheck[sizeof(UpdateFttLicenseJob) == 48 ? 1 : -1];

} /* namespace */

/* (0x8129FF40) */
bool topmenuHasPendingFttUpdateTask(const paf::string *titleId)
{
	return shellBgdlTitleHasTasksOfKind(titleId->c_str(), 0, 4);
}

/* (0x812A03AE) */
void topmenuStartUpdateFttLicenseJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				     TopmenuAppProcess *proc,
				     const LsdbAppInfoPtr *appInfo)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new UpdateFttLicenseJob(proc, appInfo));
	*outJob = job;
	topmenuEnqueueInstallJob(&job);
}

/* (0x812A053E) */
void topmenuOnLicenseDialogTabLeave(SceInt32 eventType, void *sender, SceInt32 eventArg,
				    void *userData)
{
	(void)eventType;
	(void)sender;
	(void)eventArg;
	FttLicenseErrorHandler *handler = static_cast<FttLicenseErrorHandler *>(userData);
	if (handler != NULL)
		SceCommonGuiDialog_BC311DC0(handler->m_dialogId);
}
