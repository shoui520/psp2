/*
 * debugger_check_job.cpp: TopmenuAppProcess lifecycle and DebbugerCheckJob
 * (0x81286FC0-0x81288665).
 * Manages per-tab application launch state, debugger/devkit synthetic LSDB entries,
 * param.sfo inspection via game_plugin, and foreground/background transitions.
 */
#include "topmenu_internal.h"

namespace {

typedef int (*GamePluginSfoLoadFunc)(const char *sfoPath, LsdbAppInfoPtr *outInfo);

static void lsdbSetInteger(LsdbAppInfo *map, unsigned int key, SceInt64 value)
{
	LsdbValue &entry = (*map)[key];
	if (entry.type == LSDB_VALUE_TEXT) {
		delete entry.text;
		entry.text = NULL;
	}
	entry.integer = value;
	entry.type = LSDB_VALUE_INTEGER;
}

static void lsdbSetText(LsdbAppInfo *map, unsigned int key, const char *value)
{
	LsdbValue &entry = (*map)[key];
	if (entry.type == LSDB_VALUE_TEXT && entry.text != NULL) {
		*entry.text = (value != NULL) ? value : "";
	} else {
		entry.text = new paf::string(value != NULL ? value : "");
	}
	entry.type = LSDB_VALUE_TEXT;
}

/* (0x8128808C) Rewrite LiveArea path to retail folder when debug_upgradable == 3 */
static paf::string resolveDebugLiveAreaPath(const char *liveAreaPath)
{
	int debugUpgradable = 0;

	if (sceRegMgrGetKeyIntForUser("/CONFIG/NP", "debug_upgradable", &debugUpgradable) != 0 ||
	    debugUpgradable != 3) {
		return paf::string(liveAreaPath != NULL ? liveAreaPath : "");
	}

	static paf::string s_liveAreaSegment("/sce_sys/livearea/");
	static paf::string s_retailSegment("/sce_sys/retail/livearea/");

	paf::string rewritten(liveAreaPath != NULL ? liveAreaPath : "");
	size_t matchPos = rewritten.find(s_liveAreaSegment);
	if (matchPos == paf::string::npos)
		return paf::string(liveAreaPath != NULL ? liveAreaPath : "");

	rewritten.replace(matchPos, s_liveAreaSegment.length(), s_retailSegment);
	return rewritten;
}

class DebbugerCheckJob : public paf::job::JobItem {
public:
	DebbugerCheckJob(TopmenuAppProcess *process);
	virtual ~DebbugerCheckJob();
	virtual int32_t Run();
	virtual void Finish(int32_t result);

	TopmenuAppProcess *m_process;		/* +0x1C */
	paf::string m_liveAreaPath;		/* +0x20 */
	paf::string m_originalPath;		/* +0x2C */
	LsdbAppInfoPtr m_sfoInfo;		/* +0x38 */
};

typedef char DebbugerCheckJobSizeCheck[sizeof(DebbugerCheckJob) == 64 ? 1 : -1];

/* (0x81287404) */
DebbugerCheckJob::DebbugerCheckJob(TopmenuAppProcess *process)
	: paf::job::JobItem("DebbugerCheckJob", NULL),
	  m_process(process),
	  m_liveAreaPath(),
	  m_originalPath(),
	  m_sfoInfo(new LsdbAppInfo())
{
	const char *liveAreaPath = SceLsdb_C070E42E(&process->m_appInfo);
	m_liveAreaPath = (liveAreaPath != NULL) ? liveAreaPath : "";

	const char *originalPath = sceLsdbGetOriginalPath(&m_process->m_appInfo);
	m_originalPath = (originalPath != NULL) ? originalPath : "";
}

/* (0x81287FDE / 0x8128807C) */
DebbugerCheckJob::~DebbugerCheckJob()
{
}

/* (0x81288348) */
int32_t DebbugerCheckJob::Run()
{
	m_liveAreaPath = resolveDebugLiveAreaPath(m_liveAreaPath.c_str());

	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	const char *basePath = m_originalPath.c_str();
	if (basePath == NULL)
		return 0;

	char sfoPath[2048];
	sce_paf_snprintf(sfoPath, sizeof(sfoPath), "%s%s", basePath, "/sce_sys/param.sfo");

	GamePluginSfoLoadFunc sfoLoad =
		reinterpret_cast<GamePluginSfoLoadFunc>(gameIf->func[15]);
	return sfoLoad(sfoPath, &m_sfoInfo);
}

/* (0x812883DE) */
void DebbugerCheckJob::Finish(int32_t result)
{
	if (IsCanceled())
		return;

	if (m_process->m_pendingDelete) {
		delete m_process;
		return;
	}

	LsdbAppInfo *target = m_process->m_appInfo.get();
	lsdbSetText(target, LSDB_KEY_LIVEAREA_PATH, m_liveAreaPath.c_str());
	lsdbSetInteger(target, LSDB_KEY_ATTRIBUTE, sceLsdbGetAttribute(&m_sfoInfo));

	if (result == 0) {
		const char *npCommId = SceLsdb_E26FC9E8(&m_sfoInfo);
		if (npCommId != NULL)
			lsdbSetText(target, LSDB_KEY_NP_COMMUNICATION_ID, npCommId);

		const char *supportUri = SceLsdb_AE60C3A5(&m_sfoInfo);
		if (supportUri != NULL)
			lsdbSetText(target, LSDB_KEY_SUPPORT_URI, supportUri);
	}

	if (m_process->m_tabAppData != NULL)
		m_process->m_tabAppData->SetupContent();
	m_process->m_launchState = 3;
}

} /* namespace */

/* (0x81286FC0) */
bool TopmenuAppProcess::StartOrResume(SceInt32 startArg)
{
	if (m_launchState < 2)
		return topmenuProcessLaunchAsync(this, startArg) != 0;

	m_pendingStart = false;
	topmenuProcessSpawn(this, startArg, 0);
	return true;
}

/* (0x81286FDE) */
TopmenuAppProcess::TopmenuAppProcess(const paf::string &titleId, const LsdbAppInfoPtr &appInfo,
				     TopTabAppData *tabAppData, SceInt32 launchMode,
				     bool autoStart, SceInt32 startArg)
	: TopmenuProcessBase(this),
	  m_titleId(titleId),
	  m_tabAppData(tabAppData),
	  m_launchState(0),
	  m_hasLaunchArg(false),
	  m_launchArgBuf(NULL),
	  m_launchArgSize(0),
	  m_lastError(-1),
	  m_launchMode(launchMode),
	  m_pendingDelete(false),
	  m_appInfo(appInfo),
	  m_checkJob(),
	  m_pendingStart(false),
	  m_launchedBgPartner(false),
	  m_pendingResume(false)
{
	if (autoStart && m_launchMode != 1 && m_launchMode != 2 && m_launchMode != 3) {
		if (m_launchState >= 2) {
			m_pendingStart = false;
			topmenuProcessSpawn(this, startArg, 0);
		} else {
			topmenuProcessLaunchAsync(this, startArg);
		}
	}
}

/* (0x812870BC) */
TopmenuAppProcess::~TopmenuAppProcess()
{
	if (m_checkJob.get() != NULL) {
		m_checkJob->Cancel();
		m_checkJob.reset();
	}
}

/* (0x81287190) */
const char *TopmenuAppProcess::GetBgTitleId() const
{
	if (m_appInfo.get() != NULL)
		return SceLsdb_457F369E(&m_appInfo);
	return NULL;
}

/* (0x812871A2) */
bool TopmenuAppProcess::ResumeWithArg(SceInt32 startArg, SceInt32 resumeFlag)
{
	if (m_launchState >= 2) {
		m_pendingStart = false;
		topmenuProcessSpawn(this, startArg, resumeFlag);
		return true;
	}
	m_pendingStart = true;
	return false;
}

/* (0x812871C2) */
bool TopmenuAppProcess::ResumeDefault()
{
	if (m_launchState >= 2) {
		m_pendingStart = false;
		topmenuProcessSpawn(this, -1, 0);
		return true;
	}
	m_pendingStart = true;
	return false;
}

/* (0x812871E8) */
int TopmenuAppProcess::SendProcessCommand(SceInt32 command)
{
	return topmenuProcessCommand(this, command);
}

/* (0x812871F0) */
void TopmenuAppProcess::DestroyOrDefer()
{
	if (m_launchState == 1) {
		m_pendingDelete = true;
	} else {
		delete this;
	}
}

/* (0x8128720C) */
bool TopmenuAppProcess::IsProcessRunning() const
{
	return m_procState >= 2;
}

/* (0x81287218) */
void TopmenuAppProcess::SetAttrFlags(const TopmenuAppAttrFlags *flags)
{
	if (m_hasLaunchArg != flags->hasFlagBitSeven ||
	    reinterpret_cast<SceInt32>(m_launchArgBuf) != flags->flagBitEight ||
	    static_cast<SceInt32>(m_launchArgSize) != flags->flagBitNine) {
		m_hasLaunchArg = flags->hasFlagBitSeven;
		m_launchArgBuf = reinterpret_cast<void *>(flags->flagBitEight);
		m_launchArgSize = static_cast<SceSize>(flags->flagBitNine);
	}
}

/* (0x81287244) */
void TopmenuAppProcess::SetParentAppId(SceInt32 parentAppId)
{
	if (m_lastError != parentAppId)
		m_lastError = parentAppId;
}

/* (0x8128724E) */
void TopmenuAppProcess::UpdateAppInfo(const LsdbAppInfoPtr &appInfo)
{
	m_appInfo = appInfo;
	if (m_tabAppData != NULL)
		m_tabAppData->RefreshLiveAreaInfo();
}

/* (0x812872D6) */
void TopmenuAppProcess::SetHasBgPartner(bool hasPartner)
{
	m_launchedBgPartner = hasPartner;
}

/* (0x812872DC) */
void TopmenuAppProcess::OnEnterViewport()
{
	topmenuProcessOnEnterViewport(this);
}

/* (0x812872E4) */
void TopmenuAppProcess::OnLeaveViewport()
{
	topmenuProcessOnLeaveViewport(this);
}

/* (0x812872EC) */
void TopmenuAppProcess::OnResumeForeground()
{
	if (m_tabAppData == NULL) {
		topmenuSetShellViewMode(g_topWindow, 2);
	} else {
		m_tabAppData->OnEnterViewport();
		SceInt32 viewKind = m_tabAppData->m_viewKind;
		if (viewKind == 1) {
			topmenuSetShellViewMode(g_topWindow, 2);
		} else if (viewKind == 2 || viewKind == 3) {
			m_tabAppData->m_captureCountdown = 1;
			if (m_tabAppData->m_viewKind == 2) {
				topmenuSetShellViewMode(g_topWindow, 3);
			} else if (m_tabAppData->m_appViewState == 1) {
				topmenuSetShellViewMode(g_topWindow, 4);
			} else {
				topmenuSetShellViewMode(g_topWindow, 5);
			}
		}
	}

	if (m_launchedBgPartner)
		topmenuResumeOrDestroyBgPartners(m_appId);
}

/* (0x8128736E) */
void TopmenuAppProcess::OnSuspendBackground()
{
	if (m_tabAppData != NULL) {
		m_tabAppData->OnLeaveViewport();
		SceInt32 viewKind = m_tabAppData->m_viewKind;
		if (viewKind == 1) {
			if (m_tabAppData->m_screenCopy != NULL)
				m_tabAppData->m_screenCopy->CaptureAndPublish();
		} else if (viewKind == 2 || viewKind == 3) {
			TopTabScreenCopy *screenCopy = m_tabAppData->m_screenCopy;
			if (m_tabAppData->m_appViewState == 1) {
				if (screenCopy != NULL)
					screenCopy->CaptureAndPublish();
			} else if (screenCopy != NULL && screenCopy->IsReady()) {
				m_tabAppData->SetSnapshotSurface(screenCopy->m_surface);
			} else {
				paf::intrusive_ptr<paf::graph::Surface> emptySurface;
				m_tabAppData->SetSnapshotSurface(emptySurface);
			}
			m_tabAppData->m_captureCountdown = 1;
			if (m_tabAppData->m_screenCopy != NULL)
				m_tabAppData->m_screenCopy->ClearState();
		}
	}

	if (m_launchedBgPartner)
		topmenuSuspendBgPartners();
}

/* (0x812874F2) */
void TopmenuAppProcess::StartAppCheck(const char *debugBasePath)
{
	if (m_launchMode == 1 || m_launchMode == 2 || m_launchMode == 3) {
		LsdbAppInfoPtr syntheticInfo(new LsdbAppInfo());
		m_appInfo = syntheticInfo;

		LsdbAppInfo *infoMap = m_appInfo.get();
		if (m_launchMode == 1)
			lsdbSetInteger(infoMap, LSDB_KEY_LAREA_TYPE, 0);
		else if (m_launchMode == 2)
			lsdbSetInteger(infoMap, LSDB_KEY_LAREA_TYPE, 1);
		else
			lsdbSetInteger(infoMap, LSDB_KEY_LAREA_TYPE, 2);

		lsdbSetInteger(infoMap, LSDB_KEY_APPTYPE, 3);
		lsdbSetInteger(infoMap, LSDB_KEY_APP_MEMSIZE, 0);

		if (debugBasePath != NULL) {
			char contentsPath[2048];
			sce_paf_snprintf(contentsPath, sizeof(contentsPath),
					 "%s/sce_sys/livearea/contents", debugBasePath);
			lsdbSetText(infoMap, LSDB_KEY_LIVEAREA_PATH, contentsPath);
			lsdbSetText(infoMap, LSDB_KEY_ORG_PATH, debugBasePath);
		} else {
			lsdbSetText(infoMap, LSDB_KEY_LIVEAREA_PATH,
				    "vs0:data/internal/livearea/default/sce_sys/livearea/contents");
		}

		lsdbSetText(infoMap, LSDB_KEY_TITLE_ID, m_titleId.c_str());
		lsdbSetText(infoMap, LSDB_KEY_TITLE_ID_SFO, m_titleId.c_str());
		lsdbSetText(infoMap, LSDB_KEY_CATEGORY, "gd");
		lsdbSetText(infoMap, LSDB_KEY_APP_VER, "00.00");

		if (m_hasLaunchArg || m_launchArgBuf != NULL || m_launchArgSize != 0) {
			m_hasLaunchArg = false;
			m_launchArgBuf = NULL;
			m_launchArgSize = 0;
		}

		paf::common::SharedPtr<paf::job::JobItem> job(new DebbugerCheckJob(this));
		g_topWindow->m_jobQueue->Enqueue(job);
		m_checkJob = job;

		m_launchState = 1;
		if (m_launchMode == 2 || m_launchMode == 3)
			topmenuProcessSetState(this, 2);
		return;
	}

	paf::common::SharedPtr<paf::job::JobItem> job;
	topmenuStartAppCheckJob(&job, this);
	m_checkJob = job;
	m_launchState = 1;
}

/* (0x81287D8E) */
bool TopmenuAppProcess::OnCheckJobComplete()
{
	if (m_pendingDelete) {
		if (m_tabAppData != NULL)
			topmenuCancelLiveAreaOpen(m_tabAppData->m_ownerTab->m_pageKey);
		delete this;
		return false;
	}

	m_checkJob.reset();
	return true;
}

/* (0x81287E0E) */
void TopmenuAppProcess::ApplyAppAttributes()
{
	SceInt64 attr = sceLsdbGetAttribute(&m_appInfo);
	TopmenuAppAttrFlags flags;
	flags.hasFlagBitSeven = (attr & 0x80) != 0;
	flags.flagBitEight = (attr & 0x100) != 0 ? 1 : 0;
	flags.flagBitNine = (attr & 0x200) != 0 ? 1 : 0;
	SetAttrFlags(&flags);
}

/* (0x81287E94) */
void TopmenuAppProcess::OnAppCheckSucceeded(const LsdbAppInfoPtr &appInfo)
{
	m_appInfo = appInfo;
	m_launchState = 2;
	ApplyAppAttributes();
	SetParentAppId(-1);

	bool shouldSpawn = false;
	if (m_tabAppData != NULL) {
		m_tabAppData->SetupContent();
		SceInt32 viewKind = m_tabAppData->m_viewKind;
		if (viewKind == 2 || viewKind == 3) {
			if (m_launchState < 2)
				m_pendingStart = true;
			else
				shouldSpawn = true;
		} else if (m_spawnJob.get() == NULL) {
			topmenuProcessActivateLiveArea(this);
			topmenuNotifyForegroundReady(this);
		} else if (!m_pendingStart) {
			topmenuNotifyForegroundReady(this);
		} else if (m_launchState < 2) {
			m_pendingStart = true;
		} else {
			shouldSpawn = true;
		}
	} else if (m_launchState < 2) {
		m_pendingStart = true;
	} else {
		shouldSpawn = true;
	}

	if (shouldSpawn) {
		m_pendingStart = false;
		topmenuProcessSpawn(this, -1, 0);
	}

	m_launchState = 3;
	if (m_tabAppData != NULL)
		topmenuNotifyLaunchSeqReady(m_tabAppData->m_ownerTab->m_pageKey);
}

/* C-linkage wrappers declared in topmenu_internal.h for Batch 2 callers */
const char *topmenuAppProcessGetTitleId(const TopmenuAppProcess *proc)
{
	return proc->GetBgTitleId();
}

bool topmenuAppProcessIsRunning(const TopmenuAppProcess *proc)
{
	return proc->IsProcessRunning();
}

void topmenuAppProcessOnEnterViewport(TopmenuAppProcess *proc)
{
	proc->OnEnterViewport();
}

void topmenuAppProcessOnLeaveViewport(TopmenuAppProcess *proc)
{
	proc->OnLeaveViewport();
}

void topmenuAppProcessCancel(TopmenuAppProcess *proc)
{
	proc->StartAppCheck(NULL);
}
