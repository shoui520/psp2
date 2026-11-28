/*
 * psm_update_job.cpp: PsmUpdateJob and PsmAnimJob for PlayStation Mobile runtime
 * installation/update via BGDL and indicator inhale animation (0x8129AB80-0x8129B5DF).
 */
#include "topmenu_internal.h"
#include "../../indicator_plugin/src/indicator.h"

namespace {

const SceInt32 TOPMENU_ERROR_PSM_SYSUP_REQUIRED	= -2146420143; /* 0x80103A51 */
const SceUInt32 MSG_PSM_UPDATING_WAIT		= 0xF0C106FEu; /* -255783970 */

typedef int (*GamePluginCheckUpdateFunc)(const LsdbAppInfoPtr *info,
					 void (*callback)(SceInt32 result, void *userData),
					 void *userData, bool gameCard, int mode, int priority);

/* (0x8129AB80..0x8129ACBD) Delayed indicator inhale animation for PSM runtime icon */
class PsmAnimJob : public paf::job::JobItem {
public:
	explicit PsmAnimJob(const paf::string &iconPath)
		: paf::job::JobItem("PsmAnimJob", NULL),
		  m_busyHeld(false),
		  m_iconPath(iconPath)
	{
		if (!m_busyHeld) {
			topmenuBeginBusy();
			m_busyHeld = true;
		}
	}

	virtual ~PsmAnimJob()
	{
		if (m_busyHeld) {
			topmenuEndBusy();
			m_busyHeld = false;
		}
	}

	virtual int32_t Run()
	{
		paf::thread::Sleep(500);
		paf::Plugin *indicatorPlugin = paf::Plugin::Find("indicator_plugin");
		if (indicatorPlugin != NULL) {
			IndicatorPluginInterface *indicatorIf =
				static_cast<IndicatorPluginInterface *>(
					indicatorPlugin->GetInterface(1));
			if (indicatorIf != NULL && indicatorIf->StartInhaleAnim != NULL) {
				IndicatorInhaleAnim anim;
				sce_paf_memset(&anim, 0, sizeof(anim));
				if (!m_iconPath.empty())
					anim.iconPath = m_iconPath;
				else
					anim.iconPath = "vs0:vsh/game/psm/runtime_icon.png";
				indicatorIf->StartInhaleAnim(&anim);
			}
		}
		return 0;
	}

	virtual void Finish(int32_t result)
	{
		(void)result;
	}

	bool m_busyHeld;		/* +0x19 */
	paf::string m_iconPath;		/* +0x1C */
};

typedef char PsmAnimJobSizeCheck[sizeof(PsmAnimJob) == 40 ? 1 : -1];

/* (0x8129ACBE..0x8129B435) Enqueue BGDL PSM runtime task and trigger sysupchk if required */
class PsmUpdateJob : public paf::job::JobItem {
public:
	PsmUpdateJob(const paf::string &titleId, SceInt32 pageKey);

	virtual ~PsmUpdateJob()
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

	virtual int32_t Run()
	{
		paf::string iconPath("");
		int ret = shellBgdlAddPsmRuntimeTask(&iconPath);
		if (ret == 0) {
			m_iconPath = iconPath;
		} else if (ret == TOPMENU_ERROR_PSM_SYSUP_REQUIRED && !m_titleId.empty()) {
			m_appInfo.reset(new LsdbAppInfo());
			if (SceLsdb_87EE0526(g_shellLsdbClient->client, &m_titleId, &m_appInfo) != 0)
				m_appInfo.reset();
		}
		return ret;
	}

	virtual void Finish(int32_t result)
	{
		if (m_waitDialogId >= 0) {
			SceCommonGuiDialog_BC311DC0(m_waitDialogId);
			m_waitDialogId = -1;
		}

		if (result >= 0) {
			if (result == 0) {
				paf::string iconCopy = m_iconPath;
				paf::common::SharedPtr<paf::job::JobItem> animJob(
					new PsmAnimJob(iconCopy));
				topmenuEnqueueInstallJob(&animJob);
			}
		} else if (result == TOPMENU_ERROR_PSM_SYSUP_REQUIRED) {
			if (m_appInfo.get() != NULL && g_topWindow->m_gameIf != NULL) {
				GamePluginCheckUpdateFunc checkUpdate =
					reinterpret_cast<GamePluginCheckUpdateFunc>(
						g_topWindow->m_gameIf->func[40]);
				checkUpdate(&m_appInfo, topmenuSysupchkDoneCallback,
					    reinterpret_cast<void *>(
						    static_cast<intptr_t>(m_pageKey)),
					    false, 0, -1);
			}
		} else {
			topmenuShowAppErrorMessageDialog(result, NULL, NULL,
							 NULL, NULL, NULL, NULL, NULL);
		}
	}

	SceInt32 m_waitDialogId;	/* +0x1C */
	bool m_busyHeld;		/* +0x20 */
	SceInt32 m_pageKey;		/* +0x24 */
	paf::string m_titleId;		/* +0x28 */
	paf::string m_iconPath;		/* +0x34 */
	LsdbAppInfoPtr m_appInfo;	/* +0x40 */
};

typedef char PsmUpdateJobSizeCheck[sizeof(PsmUpdateJob) == 72 ? 1 : -1];

/* (0x8129ACBE) */
PsmUpdateJob::PsmUpdateJob(const paf::string &titleId, SceInt32 pageKey)
	: paf::job::JobItem("PsmUpdateJob", NULL),
	  m_waitDialogId(-1),
	  m_busyHeld(false),
	  m_pageKey(pageKey),
	  m_titleId(titleId),
	  m_iconPath(""),
	  m_appInfo()
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	paf::thread::RMutex::main_thread_mutex.Lock();

	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.layout = SceCommonGuiDialog_2A68B4A4;
	const wchar_t *waitStr =
		g_topmenuPlugin->GetString(paf::IDParam(MSG_PSM_UPDATING_WAIT));
	param.text = (waitStr != NULL) ? waitStr : L"";
	m_waitDialogId = SceCommonGuiDialog_CCFCD9A6(&param);

	paf::thread::RMutex::main_thread_mutex.Unlock();

	if (!m_busyHeld) {
		topmenuBeginBusy();
		m_busyHeld = true;
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

} /* namespace */

/* (0x8129B436) */
void topmenuStartPsmUpdateJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			      const paf::string *titleId, SceInt32 pageKey)
{
	paf::string titleCopy(*titleId);
	paf::common::SharedPtr<paf::job::JobItem> job(new PsmUpdateJob(titleCopy, pageKey));
	*outJob = job;
	topmenuEnqueueInstallJob(&job);
}
