/*
 * debugger_check_job.cpp: TopmenuAppProcess lifecycle and DebbugerCheckJob
 * (0x81286FC0-0x81288665).
 * Manages per-tab application launch state, debugger/devkit synthetic LSDB entries,
 * param.sfo inspection via game_plugin, and foreground/background transitions.
 */
#include "topmenu_internal.h"

namespace {

typedef int (*GamePluginSfoLoadFunc)(const char *sfoPath, LsdbAppInfoPtr *outInfo);
#if !defined(FW_TYPE_CEX)
typedef int (*GamePluginReadEbootParamFunc)(const char *ebootPath, SceUInt32 *outHeader,
					    void *outParams);

struct AppHomeEbootHeaderParam {
	SceUInt32 attribute;
	SceUInt32 phyMemsize;
	SceUInt32 appMemsize;
	SceUInt32 extraWords[4];
};
typedef char AppHomeEbootHeaderParamSizeCheck[sizeof(AppHomeEbootHeaderParam) == 28 ? 1 : -1];
#endif

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

	if (sceRegMgrGetKeyInt("/CONFIG/NP", "debug_upgradable", &debugUpgradable) != 0 ||
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

#if !defined(FW_TYPE_CEX)
class AppHomeDirCheckJob : public paf::job::JobItem {
public:
	explicit AppHomeDirCheckJob(TopmenuAppProcess *process);
	virtual ~AppHomeDirCheckJob();
	virtual int32_t Run();
	virtual void Finish(int32_t result);

	TopmenuAppProcess *m_process;		/* +0x1C */
	paf::string m_discId;			/* +0x20 */
	LsdbAppInfoPtr m_sfoInfo;		/* +0x2C */
	LsdbAppInfoPtr m_subSfoInfo;		/* +0x34 */
	int m_subSfoResult;			/* +0x3C */
	AppHomeEbootHeaderParam m_ebootParam;	/* +0x40 */
	int m_ebootParamResult;			/* +0x5C */
};

typedef char AppHomeDirCheckJobSizeCheck[sizeof(AppHomeDirCheckJob) == 96 ? 1 : -1];

/* (0x812795E6, TOOL) */
AppHomeDirCheckJob::AppHomeDirCheckJob(TopmenuAppProcess *process)
	: paf::job::JobItem("AppHomeDirCheckJob", NULL),
	  m_process(process),
	  m_discId(),
	  m_sfoInfo(new LsdbAppInfo()),
	  m_subSfoInfo(new LsdbAppInfo()),
	  m_subSfoResult(-1),
	  m_ebootParamResult(-1)
{
}

/* (0x8127ACEA / 0x8127ADBE, TOOL) */
AppHomeDirCheckJob::~AppHomeDirCheckJob()
{
}

/* (0x8127B08A, TOOL) */
int32_t AppHomeDirCheckJob::Run()
{
	LsdbAppInfo *target = m_process->m_appInfo.get();
	const char *origLiveArea = SceLsdb_C070E42E(&m_process->m_appInfo);
	paf::string resolvedLiveArea = resolveDebugLiveAreaPath(origLiveArea);
	lsdbSetText(target, LSDB_KEY_LIVEAREA_PATH, resolvedLiveArea.c_str());

	const char *liveAreaPath = SceLsdb_C070E42E(&m_process->m_appInfo);
	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	const char *originalPath = sceLsdbGetOriginalPath(&m_process->m_appInfo);
	if (originalPath != NULL) {
		char sfoPath[2048];
		GamePluginSfoLoadFunc sfoLoad =
			reinterpret_cast<GamePluginSfoLoadFunc>(gameIf->func[15]);
		GamePluginReadEbootParamFunc readEbootParam =
			reinterpret_cast<GamePluginReadEbootParamFunc>(gameIf->func[16]);

		sce_paf_snprintf(sfoPath, sizeof(sfoPath), "%s%s", originalPath,
				 "/sce_sys/param.sfo");
		if (sfoLoad(sfoPath, &m_sfoInfo) == 0) {
			const char *discId = sceLsdbGetDiscId(&m_sfoInfo);
			m_discId.assign(discId, discId != NULL ? sce_paf_strlen(discId) : 0);
		}

		paf::string subEbootPath(originalPath);
		subEbootPath += "/eboot2.bin";
		sce_paf_snprintf(sfoPath, sizeof(sfoPath), "%s%s", originalPath,
				 "/sce_sys/param2.sfo");
		m_subSfoResult = sfoLoad(sfoPath, &m_subSfoInfo);
		if (m_subSfoResult == 0) {
			LsdbAppInfo *subInfo = m_subSfoInfo.get();
			lsdbSetText(subInfo, LSDB_KEY_SELF_PATH, subEbootPath.c_str());
			lsdbSetText(subInfo, LSDB_KEY_TITLE_ID, sceLsdbGetDiscId(&m_subSfoInfo));
			lsdbSetInteger(subInfo, LSDB_KEY_APPTYPE, 2);
		}

		SceUInt32 subHeader = 0;
		AppHomeEbootHeaderParam subParam;
		if (readEbootParam(subEbootPath.c_str(), &subHeader, &subParam) == 0) {
			LsdbAppInfo *subInfo = m_subSfoInfo.get();
			lsdbSetInteger(subInfo, LSDB_KEY_APP_MEMSIZE, subParam.appMemsize);
			lsdbSetInteger(subInfo, LSDB_KEY_PHY_MEMSIZE, subParam.phyMemsize);
			lsdbSetInteger(subInfo, LSDB_KEY_EBOOT_ATTR, subParam.attribute);
		}
	}

	const char *selfPath = sceLsdbGetSelfPath(&m_process->m_appInfo);
	if (selfPath != NULL) {
		SceUInt32 selfHeader = 0;
		GamePluginReadEbootParamFunc readEbootParam =
			reinterpret_cast<GamePluginReadEbootParamFunc>(gameIf->func[16]);
		m_ebootParamResult = readEbootParam(selfPath, &selfHeader, &m_ebootParam);
	}

	if (liveAreaPath != NULL) {
		char templatePath[2048];
		sce_paf_snprintf(templatePath, sizeof(templatePath), "%s/template.xml",
				 liveAreaPath);
		if (paf::LocalFile::Exists(templatePath))
			return 0;
	}
	return -1;
}

/* (0x8127B53C, TOOL) */
void AppHomeDirCheckJob::Finish(int32_t result)
{
	if (IsCanceled())
		return;

	if (m_process->m_pendingDelete) {
		delete m_process;
		return;
	}

	LsdbAppInfo *target = m_process->m_appInfo.get();
	if (result != 0) {
		lsdbSetText(target, LSDB_KEY_LIVEAREA_PATH,
			    "vs0:data/internal/livearea/default/sce_sys/livearea/contents");
	}

	if (m_subSfoResult == 0) {
		m_process->m_subAppInfo = m_subSfoInfo;
		const char *subName = sceLsdbGetName(&m_subSfoInfo);
		lsdbSetText(m_process->m_appInfo.get(), LSDB_KEY_BG_TITLE_ID, subName);
	}

	if (m_process->m_tabAppData != NULL) {
		if (m_discId.length() != 0) {
			TopTab *existingTab =
				topmenuFindTabByTitleId(g_topWindow, m_discId.c_str());
			if (existingTab != NULL &&
			    existingTab != m_process->m_tabAppData->m_ownerTab) {
				TopTabBase **nextIter = NULL;
				topmenuCloseTabWithNext(existingTab, false, &nextIter);
			}
			m_process->m_tabAppData->SetTitleId(m_discId.c_str());
		}

		LsdbAppInfo *infoMap = m_process->m_appInfo.get();
		LsdbAppInfo::iterator verIt = m_sfoInfo->find(LSDB_KEY_APP_VER);
		if (verIt == m_sfoInfo->end()) {
			lsdbSetText(infoMap, LSDB_KEY_APP_VER, "01.00");
		} else {
			lsdbSetText(infoMap, LSDB_KEY_APP_VER, LsdbValueText(verIt->second));
		}

		const char *npCommId = SceLsdb_E26FC9E8(&m_sfoInfo);
		if (npCommId != NULL)
			lsdbSetText(infoMap, LSDB_KEY_NP_COMMUNICATION_ID, npCommId);

		const char *supportUri = SceLsdb_AE60C3A5(&m_sfoInfo);
		if (supportUri != NULL)
			lsdbSetText(infoMap, LSDB_KEY_SUPPORT_URI, supportUri);

		lsdbSetInteger(infoMap, LSDB_KEY_ATTRIBUTE, sceLsdbGetAttribute(&m_sfoInfo));

		if (m_ebootParamResult != 0) {
			lsdbSetInteger(infoMap, LSDB_KEY_APP_MEMSIZE, 0);
			lsdbSetInteger(infoMap, LSDB_KEY_PHY_MEMSIZE, 0);
			lsdbSetInteger(infoMap, LSDB_KEY_EBOOT_ATTR, 0);
		} else {
			lsdbSetInteger(infoMap, LSDB_KEY_APP_MEMSIZE, m_ebootParam.appMemsize);
			lsdbSetInteger(infoMap, LSDB_KEY_PHY_MEMSIZE, m_ebootParam.phyMemsize);
			lsdbSetInteger(infoMap, LSDB_KEY_EBOOT_ATTR, m_ebootParam.attribute);
		}

		m_process->m_tabAppData->SetupContent();
	}

	if (m_process->m_spawnJob.get() == NULL) {
		topmenuProcessResetAndCancel(m_process);
		topmenuReturnToHomeIfActive(m_process);
	}
	m_process->m_launchState = 3;
}
#endif

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
	  m_parentAppId(-1),
	  m_launchMode(launchMode),
	  m_pendingDelete(false),
	  m_appInfo(appInfo),
	  m_checkJob(),
	  m_pendingStart(false),
	  m_launchedBgPartner(false),
#if !defined(FW_TYPE_CEX)
	  m_subAppInfo(),
#endif
	  m_pendingResume(false)
{
	m_bgLayerOpt.useCustomColor = false;
	m_bgLayerOpt.style = 0;
	m_bgLayerOpt.color = 0;
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
	if (m_bgLayerOpt.useCustomColor != flags->hasFlagBitSeven ||
	    m_bgLayerOpt.style != flags->flagBitEight ||
	    m_bgLayerOpt.color != static_cast<SceUInt32>(flags->flagBitNine)) {
		m_bgLayerOpt.useCustomColor = flags->hasFlagBitSeven;
		m_bgLayerOpt.style = flags->flagBitEight;
		m_bgLayerOpt.color = static_cast<SceUInt32>(flags->flagBitNine);
	}
}

/* (0x81287244) */
void TopmenuAppProcess::SetParentAppId(SceInt32 parentAppId)
{
	if (m_parentAppId != parentAppId)
		m_parentAppId = parentAppId;
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

		if (m_bgLayerOpt.useCustomColor || m_bgLayerOpt.style != 0 ||
		    m_bgLayerOpt.color != 0) {
			m_bgLayerOpt.useCustomColor = false;
			m_bgLayerOpt.style = 0;
			m_bgLayerOpt.color = 0;
		}

		paf::common::SharedPtr<paf::job::JobItem> job(new DebbugerCheckJob(this));
		g_topWindow->m_jobQueue->Enqueue(job);
		m_checkJob = job;

		m_launchState = 1;
		if (m_launchMode == 2 || m_launchMode == 3)
			topmenuProcessSetState(this, 2);
		return;
	}
#if !defined(FW_TYPE_CEX)
	if (m_launchMode == 4) {
		LsdbAppInfoPtr syntheticInfo(new LsdbAppInfo());
		m_appInfo = syntheticInfo;

		LsdbAppInfo *infoMap = m_appInfo.get();
		lsdbSetText(infoMap, LSDB_KEY_TITLE, "APP_HOME");
		lsdbSetInteger(infoMap, LSDB_KEY_LAREA_TYPE, 0);
		lsdbSetInteger(infoMap, LSDB_KEY_APPTYPE, 2);
		lsdbSetText(infoMap, LSDB_KEY_CATEGORY, "gd");
		lsdbSetInteger(infoMap, LSDB_KEY_APP_MEMSIZE, 0);
		lsdbSetInteger(infoMap, LSDB_KEY_PROMOTE_ATTR, 2);

		if (g_topWindow->m_appHomeSource == 2) {
			lsdbSetText(infoMap, LSDB_KEY_LIVEAREA_PATH,
				    "sd0:app/sce_sys/livearea/contents");
			lsdbSetText(infoMap, LSDB_KEY_SELF_PATH, "sd0:app/eboot.bin");
			lsdbSetText(infoMap, LSDB_KEY_ORG_PATH, "sd0:app");
		} else if (g_topWindow->m_appHomeSource == 3) {
			lsdbSetText(infoMap, LSDB_KEY_LIVEAREA_PATH,
				    "ux0:data/app/sce_sys/livearea/contents");
			lsdbSetText(infoMap, LSDB_KEY_SELF_PATH, "ux0:data/app/eboot.bin");
			lsdbSetText(infoMap, LSDB_KEY_ORG_PATH, "ux0:data/app");
		} else {
			lsdbSetText(infoMap, LSDB_KEY_LIVEAREA_PATH,
				    "host0:app/sce_sys/livearea/contents");
			lsdbSetText(infoMap, LSDB_KEY_SELF_PATH, "host0:app/eboot.bin");
			lsdbSetText(infoMap, LSDB_KEY_ORG_PATH, "host0:app");
		}

		lsdbSetText(infoMap, LSDB_KEY_TITLE_ID, m_titleId.c_str());
		lsdbSetText(infoMap, LSDB_KEY_TITLE_ID_SFO, m_titleId.c_str());

		TopmenuAppAttrFlags clearFlags;
		clearFlags.hasFlagBitSeven = false;
		clearFlags.flagBitEight = 0;
		clearFlags.flagBitNine = 0;
		SetAttrFlags(&clearFlags);

		paf::common::SharedPtr<paf::job::JobItem> job(new AppHomeDirCheckJob(this));
		g_topWindow->m_jobQueue->Enqueue(job);
		m_checkJob = job;
		m_launchState = 1;
		return;
	}
#endif

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

#if !defined(FW_TYPE_CEX)
/* (0x8127ABD2, TOOL) */
void TopmenuAppProcess::SetTitleId(const char *titleId)
{
	SceInt64 appType = SceLsdb_9D27A87D(&m_appInfo);
	if (appType == 2 || appType == 3) {
		m_titleId.assign(titleId, titleId != NULL ? sce_paf_strlen(titleId) : 0);
		LsdbAppInfo *infoMap = m_appInfo.get();
		lsdbSetText(infoMap, LSDB_KEY_TITLE_ID, m_titleId.c_str());
		lsdbSetText(infoMap, LSDB_KEY_TITLE_ID_SFO, m_titleId.c_str());
	}
}
#endif
