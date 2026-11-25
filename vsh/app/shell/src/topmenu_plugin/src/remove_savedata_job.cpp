/*
 * remove_savedata_job.cpp: RemoveSavedataJob deleting another user's savedata
 * directory before resuming app launch (0x812A0D28-0x812A13AB).
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_MISSING_SAVEDATA_KEY	= -2146420480; /* 0x80103900 */
const SceInt32 TOPMENU_ERROR_NO_GAME_PLUGIN		= -2146420476; /* 0x80103904 */
const SceUInt32 LSDB_KEY_INSTALL_DIR_SAVEDATA		= 0x10954174u;
const SceUInt32 MSG_REMOVING_SAVEDATA_WAIT		= 0x632BDF33u; /* 1663788851 */

typedef int (*GamePluginAppDeletePartFunc)(const char *saveDir, int kind,
					   int unusedA, int unusedB, int unusedC);
typedef void (*GamePluginPlusSdDeleteFunc)(const char *saveDir);
typedef void (*GamePluginPlusAppStatusFunc)(const char *saveDir, int status);

/* (0x812A0D28 / 0x812A0D64) Error dialog handler for savedata removal failure */
class RemoveSavedataErrorHandler : public CommonGuiDialogHandler {
public:
	explicit RemoveSavedataErrorHandler(const char *titleName)
		: m_titleId(titleName != NULL ? titleName : "")
	{
	}

	virtual ~RemoveSavedataErrorHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 2) {
			if (!m_titleId.empty()) {
				TopmenuAppProcess *proc =
					topmenuFindProcessByTitleId(g_topWindow, m_titleId.c_str());
				if (proc != NULL)
					topmenuReturnToHomeIfActive(proc);
			}
			SceCommonGuiDialog_BC311DC0(dialogId);
		}
	}

	paf::string m_titleId;	/* +0x04 */
};

class RemoveSavedataJob : public paf::job::JobItem {
public:
	RemoveSavedataJob(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
			  SceInt32 existingFlags, SceInt32 extraFlags)
		: paf::job::JobItem("RemoveSavedataJob", NULL),
		  m_appInfo(*appInfo),
		  m_launchAppId(launchAppId),
		  m_existingFlags(existingFlags),
		  m_extraFlags(extraFlags),
		  m_waitDialogId(-1),
		  m_busyHeld(false)
	{
		paf::thread::RMutex::main_thread_mutex.Lock();
		if (!m_busyHeld) {
			topmenuBeginBusy();
			m_busyHeld = true;
		}
		paf::thread::RMutex::main_thread_mutex.Unlock();
	}

	virtual ~RemoveSavedataJob()
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

	/* (0x812A0ED8) */
	virtual int32_t Run()
	{
		paf::thread::RMutex::main_thread_mutex.Lock();
		CommonGuiLayoutDialogParam param;
		param.resourcePlugin = g_topmenuPlugin;
		param.priority = 33;
		param.layout = SceCommonGuiDialog_2A68B4A4;
		const wchar_t *waitStr =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_REMOVING_SAVEDATA_WAIT));
		param.text = (waitStr != NULL) ? waitStr : L"";
		m_waitDialogId = SceCommonGuiDialog_CCFCD9A6(&param);
		paf::thread::RMutex::main_thread_mutex.Unlock();

		LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_INSTALL_DIR_SAVEDATA);
		if (it == m_appInfo->end())
			return TOPMENU_ERROR_MISSING_SAVEDATA_KEY;

		const char *rawDir = "";
		if (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL)
			rawDir = it->second.text->c_str();
		paf::string saveDir(rawDir);

		GamePluginInterface *gameIf = g_topWindow->m_gameIf;
		if (gameIf == NULL)
			return TOPMENU_ERROR_NO_GAME_PLUGIN;

		GamePluginAppDeletePartFunc deletePart =
			reinterpret_cast<GamePluginAppDeletePartFunc>(
				gameIf->func[GAME_PLUGIN_FUNC_APP_DELETE_PART]);
		int ret = deletePart(saveDir.c_str(), 1, 0, 0, 0);
		if (ret < 0)
			return ret;

		GamePluginPlusSdDeleteFunc clearPlusSd =
			reinterpret_cast<GamePluginPlusSdDeleteFunc>(gameIf->func[41]);
		clearPlusSd(saveDir.c_str());

		GamePluginPlusAppStatusFunc clearPlusStatus =
			reinterpret_cast<GamePluginPlusAppStatusFunc>(gameIf->func[43]);
		clearPlusStatus(saveDir.c_str(), 0);
		return ret;
	}

	/* (0x812A114C) */
	virtual void Finish(int32_t result)
	{
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}

		if (result >= 0) {
			const char *titleName = sceLsdbGetName(&m_appInfo);
			if (titleName != NULL) {
				TopmenuAppProcess *proc =
					topmenuFindProcessByTitleId(g_topWindow, titleName);
				if (proc != NULL) {
					proc->ResumeWithArg(m_launchAppId,
							    m_existingFlags | m_extraFlags);
					m_launchAppId = -1;
					if (proc == g_topWindow->m_foregroundApp)
						proc->SendProcessCommand(1);
				}
			}
		} else {
			const char *titleName = sceLsdbGetName(&m_appInfo);
			RemoveSavedataErrorHandler *handler =
				new RemoveSavedataErrorHandler(titleName);
			topmenuShowAppErrorMessageDialog(result, &m_appInfo, handler,
							 NULL, NULL, NULL, NULL, NULL);
		}

		if (m_launchAppId != -1) {
			topmenuCancelPendingAppLaunch(m_launchAppId);
			m_launchAppId = -1;
		}
	}

	LsdbAppInfoPtr m_appInfo;	/* +0x1C */
	SceInt32 m_launchAppId;		/* +0x24 */
	SceInt32 m_existingFlags;	/* +0x28 */
	SceInt32 m_extraFlags;		/* +0x2C */
	SceInt32 m_waitDialogId;	/* +0x30 */
	bool m_busyHeld;		/* +0x34 */
};

typedef char RemoveSavedataJobSizeCheck[sizeof(RemoveSavedataJob) == 56 ? 1 : -1];

} /* namespace */

/* (0x812A122C) */
void topmenuStartRemoveSavedataJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
				   const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
				   SceInt32 existingFlags, SceInt32 extraFlags)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new RemoveSavedataJob(appInfo, launchAppId, existingFlags, extraFlags));
	*outJob = job;
	topmenuEnqueueInstallJob(&job);
}
