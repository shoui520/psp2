/*
 * install_job.cpp: InstallJob, promotion error dialogs, and application execution
 * pre-checks (0x81289E86-0x8128D64B).
 * Handles pending BGDL/content-import promotion before app launch, storage/account/
 * savedata/version/license validation for Vita, PSP/PS1, and PSM applications.
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_MISSING_LSDB_KEY	= -2146420480; /* 0x80103900 */
const SceInt32 TOPMENU_ERROR_SYSUPCHK_WAIT	= -2146420475; /* 0x80103905 */
const SceInt32 TOPMENU_ERROR_NO_GAME_PLUGIN	= -2146420474; /* 0x80103906 */
const SceInt32 TOPMENU_ERROR_NO_GAMECARD	= -2146420441; /* 0x80103927 */
const SceInt32 TOPMENU_ERROR_CARD_EJECTED	= -2146420440; /* 0x80103928 */
const SceInt32 TOPMENU_ERROR_INSTALL_QUEUED	= -2146420439; /* 0x80103929 */
const SceInt32 TOPMENU_ERROR_SYSUPCHK_DEFERRED	= -2146420438; /* 0x8010392A */
const SceInt32 TOPMENU_ERROR_SAVEDATA_NEWER_VER	= -2146420437; /* 0x8010392B */
const SceInt32 TOPMENU_ERROR_NO_MEMORY_CARD	= -2146420431; /* 0x80103931 */
const SceInt32 TOPMENU_ERROR_NO_MC_WITH_PROMPT	= -2146420430; /* 0x80103932 */
const SceInt32 TOPMENU_ERROR_TROPHY_NO_ACCOUNT	= -2146420429; /* 0x80103933 */
const SceInt32 TOPMENU_ERROR_TROPHY_OTHER_UX0	= -2146420428; /* 0x80103934 */
const SceInt32 TOPMENU_ERROR_TROPHY_OTHER_CARD	= -2146420427; /* 0x80103935 */
const SceInt32 TOPMENU_ERROR_SAVEDATA_OTHER_USR	= -2146420426; /* 0x80103936 */
const SceInt32 TOPMENU_ERROR_ADDCONT_PENDING	= -2146420423; /* 0x80103939 */
const SceInt32 TOPMENU_ERROR_SAVEDATA_PROMPT	= -2146420422; /* 0x8010393A */
const SceInt32 TOPMENU_ERROR_MODEL_VITATV	= -2146420421; /* 0x8010393B */
const SceInt32 TOPMENU_ERROR_MODEL_RESTRICTED	= -2146420420; /* 0x8010393C */
const SceInt32 TOPMENU_ERROR_PROMOTE_RETRY	= -2146420141; /* 0x80103A53 */

const SceInt32 APPMGR_ERROR_NO_SAVEDATA		= -2147418110; /* 0x80010002 */
const SceInt32 APPMGR_ERROR_NO_CARD_MOUNT	= -2147418093; /* 0x80010013 */
const SceInt32 APPMGR_ERROR_SYS_UPDATE_NEEDED	= -2147417978; /* 0x80010086 */
const SceInt32 APPMGR_ERROR_SAVEDATA_ABSENT	= -2139095029; /* 0x8080000B */
const SceInt32 APPMGR_ERROR_LICENSE_EXPIRED	= -2139095023; /* 0x80800011 */
const SceInt32 NPDRM_ERROR_SYS_UPDATE_NEEDED	= -2138636277; /* 0x8087000B */
const SceInt32 NPDRM_ERROR_NO_LICENSE		= -2138636282; /* 0x80870006 */
const SceInt32 NPDRM_ERROR_ACTIVATION_A		= -2138629114; /* 0x80871C06 */
const SceInt32 NPDRM_ERROR_ACTIVATION_B		= -2138629113; /* 0x80871C07 */

const SceUInt32 MSG_APPLAUNCHER_WAITING		= 0x2A8588D7u; /* 713433959 */
const SceUInt32 MSG_INSTALL_ERROR_HEADING	= 0x77F41C22u; /* 2012452514 */
const SceUInt32 MSG_INSTALL_ERR_ACTIVATION	= 0xD763218Fu; /* -681353329 */
const SceUInt32 MSG_INSTALL_ERR_ADDCONT_LIC	= 0xB80E275Eu; /* -1207018050 */
const SceUInt32 MSG_INSTALL_ERR_APP_LIC		= 0xBB3E3551u; /* -1153536687 */
const SceUInt32 MSG_INSTALL_ERR_CARD_REMOVED	= 0xF4274E4Du; /* -198752243 */
const SceUInt32 MSG_INSTALL_ERR_NO_INFO		= 0x6C56BF43u; /* 1817642819 */
const SceUInt32 MSG_INSTALL_ERR_APP_RUNNING	= 0xC29F50DCu; /* -1029764004 */
const SceUInt32 TEX_INDICATOR_DEFAULT_ICON	= 0xBB28822Cu; /* -1154969172 */
const SceUInt32 TEX_LIVEAREA_ERROR_BADGE	= 0x3CAD0B5Fu; /* 1017993839 */

const char *const kPromoteCategories[6] = { "gd", "gp", "gdc", "gdd", "gpc", "gpd" };

/* Execution check queue and semaphore singleton (0x815AC698) */
struct TopmenuExecCheckContext {
	paf::job::JobQueue *jobQueue;
	SceUID waitDialogSema;
};

static TopmenuExecCheckContext *g_execCheckCtx = NULL;

typedef int (*GamePluginGetCategoryKindFunc)(const LsdbAppInfoPtr *info, int *outKind);
typedef int (*GamePluginGetStorageFunc)(const LsdbAppInfoPtr *info, int *outStorage);
typedef int (*GamePluginLicenseCheckFunc)(const LsdbAppInfoPtr *info, TopmenuLicenseCheckResult *out);
typedef int (*GamePluginAlarmSetFunc)(const char *path, const TopmenuLicenseCheckResult *status);
typedef void (*GamePluginAlarmRemoveFunc)(const LsdbAppInfoPtr *info);
typedef int (*GamePluginContentImportRunFunc)(const char *titleId);
typedef int (*GamePluginContentImportCheckFunc)(const char *titleId);
typedef void (*GamePluginDialogCloseFunc)(SceInt32 dialogId);
typedef int (*GamePluginNoticeOpenFunc)(SceInt32 *outDialogId,
					void (*callback)(SceInt32 dialogId, SceInt32 button,
							 void *userA, void *userB),
					void *userA, SceInt32 errorCode,
					paf::intrusive_ptr<paf::graph::Surface> icon,
					const paf::wstring &title, const paf::wstring &heading,
					const paf::wstring &message, bool isCustomIcon,
					paf::intrusive_ptr<paf::graph::Surface> *badgeIcon,
					SceInt32 badgeFlags, const paf::wstring &subText);
typedef int (*GamePluginCheckUpdateFunc)(const LsdbAppInfoPtr *info,
					 void (*callback)(SceInt32 result, void *userData),
					 void *userData, bool gameCard, int mode, int priority);
typedef int (*GamePluginAddcontInfoCheckFunc)(const char *titleId, bool *outFound);
typedef int (*GamePluginAppAllowedFunc)(const paf::string &titleId, bool *outAllowed, int *outModel);

typedef void (*TopmenuInterfaceSimpleFunc)(void);
typedef void (*TopmenuInterfaceTitleFunc)(const char *titleId);
typedef void (*TopmenuInterfaceTitleStatusFunc)(const char *titleId, SceInt32 status);

/* (0x8128A242) Look up an application's icon path in LSDB */
static paf::string lookupTitleIconPath(const char *titleId)
{
	paf::string result("");
	LsdbIconPtr record(new LsdbIcon());
	if (record.get() == NULL)
		return result;

	AppInfoQClient *client = static_cast<AppInfoQClient *>(::operator new(sizeof(AppInfoQClient)));
	if (client != NULL) {
		SceLsdb_A6D8772C(client);
		if (SceLsdb_E90B81CD(client) >= 0) {
			paf::string key(titleId != NULL ? titleId : "");
			if (SceLsdb_7AF9008C(client, &key, &record, 0) >= 0)
				result = record->iconPath;
		}
		AppInfoQClient *destroyed = SceLsdb_E2101B03(client);
		::operator delete(destroyed);
	}
	return result;
}

/* (0x8128A830) Load promotion error icon surface from iconPath, fallback path, or indicator texture */
static int loadInstallErrorIcon(const paf::string &primaryPath, const paf::string &fallbackPath,
				paf::intrusive_ptr<paf::graph::Surface> *outSurface,
				bool *outFromFallback)
{
	outSurface->clear();
	*outFromFallback = false;

	if (!primaryPath.empty()) {
		paf::graph::Surface::LoadOption loadOpt;
		int32_t openErr = 0;
		paf::common::SharedPtr<paf::File> fp =
			paf::LocalFile::Open(primaryPath.c_str(), 1, 0, &openErr);
		if (openErr == 0) {
			static paf::Plugin *s_topmenuPluginFirst = g_topmenuPlugin;
			if (s_topmenuPluginFirst != NULL) {
				*outSurface = paf::graph::Surface::Load(
					s_topmenuPluginFirst->m_surface_pool, fp, &loadOpt);
			}
			fp->Close();
		}
	}

	if (outSurface->get() == NULL && !fallbackPath.empty()) {
		paf::graph::Surface::LoadOption loadOpt;
		int32_t openErr = 0;
		paf::common::SharedPtr<paf::File> fp =
			paf::LocalFile::Open(fallbackPath.c_str(), 1, 0, &openErr);
		if (openErr == 0) {
			static paf::Plugin *s_topmenuPluginSecond = g_topmenuPlugin;
			if (s_topmenuPluginSecond != NULL) {
				*outSurface = paf::graph::Surface::Load(
					s_topmenuPluginSecond->m_surface_pool, fp, &loadOpt);
				if (outSurface->get() != NULL)
					*outFromFallback = true;
			}
			fp->Close();
		}
	}

	if (outSurface->get() == NULL) {
		paf::Plugin *indicatorPlugin = paf::Plugin::Find("indicator_plugin");
		if (indicatorPlugin != NULL) {
			*outSurface = indicatorPlugin->GetTexture(
				paf::IDParam(TEX_INDICATOR_DEFAULT_ICON));
		}
	}

	return (outSurface->get() != NULL) ? 0 : -1;
}

/* (0x8128B648) Callback for game_plugin promotion error notice dialog */
static void onInstallErrorNoticeButton(SceInt32 dialogId, SceInt32 button,
				       void *userA, void *userB)
{
	(void)userA;
	(void)userB;
	if (dialogId < 0)
		return;

	if (button != 0) {
		if (!topmenuIsPowerSuspendNormal()) {
			SceInt32 focusedPage = topmenuGetFocusedTabPageKey(g_topWindow);
			topmenuScrollToTabPageKey(focusedPage);
		}
		paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
		if (gamePlugin != NULL) {
			GamePluginInterface *gameIf =
				static_cast<GamePluginInterface *>(gamePlugin->GetInterface(1));
			if (gameIf != NULL) {
				GamePluginDialogCloseFunc closeDlg =
					reinterpret_cast<GamePluginDialogCloseFunc>(
						gameIf->func[GAME_PLUGIN_FUNC_DIALOG_CLOSE]);
				closeDlg(dialogId);
			}
		}
	} else {
		SceShell_D4C7D2B7(9, topmenuSpawnOnInstallNoticeEvent,
				  reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
		topmenuDialogOnPowerHold(dialogId);
	}
}

/* (0x8128D5B6) */
static void deleteTabDeferredTask(void *userData)
{
	topmenuCloseDialogAndDeleteTab(-1, 0,
				       static_cast<SceInt32>(reinterpret_cast<intptr_t>(userData)));
}

/* (0x8128D5C6) */
static void unloadSysupchkCallback(SceInt32 result, void *userData)
{
	(void)result;
	(void)userData;
	paf::Plugin::UnloadAsync("sysupchk_plugin", NULL, 0);
}

static bool lsdbFindText(const LsdbAppInfoPtr *appInfo, unsigned int key, paf::string *outText)
{
	LsdbAppInfo::iterator it = (*appInfo)->find(key);
	if (it == (*appInfo)->end())
		return false;
	if (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL)
		*outText = *it->second.text;
	else
		*outText = "";
	return true;
}

static bool lsdbFindInt64(const LsdbAppInfoPtr *appInfo, unsigned int key, SceInt64 *outValue)
{
	LsdbAppInfo::iterator it = (*appInfo)->find(key);
	if (it == (*appInfo)->end())
		return false;
	*outValue = (it->second.type == LSDB_VALUE_INTEGER) ? it->second.integer : 0;
	return true;
}

class InstallJob : public paf::job::JobItem {
public:
	InstallJob(SceInt32 mode, const char *titleId, const char *installTitleId,
		   SceInt32 bgdlTaskCount, SceInt32 importTaskCount);
	virtual ~InstallJob();
	virtual int32_t Run();
	virtual void Finish(int32_t result);

	SceInt32 m_installKind;			/* +0x1C (0: Vita, 1: PSP, 2: PSM) */
	paf::string m_titleId;			/* +0x20 */
	paf::string m_installTitleId;		/* +0x2C */
	SceInt32 m_bgdlTaskCount;		/* +0x38 */
	SceInt32 m_importTaskCount;		/* +0x3C */
	SceInt32 m_promoteWaitResult;		/* +0x40 */
	SceInt32 m_waitDialogId;		/* +0x44 */
	TopmenuInstallError m_errorInfo;	/* +0x48 (68 bytes) */
};

typedef char InstallJobSizeCheck[sizeof(InstallJob) == 140 ? 1 : -1];

/* (0x81289EB6) */
InstallJob::InstallJob(SceInt32 mode, const char *titleId, const char *installTitleId,
		       SceInt32 bgdlTaskCount, SceInt32 importTaskCount)
	: paf::job::JobItem("InstallJob", NULL),
	  m_installKind(mode),
	  m_titleId(titleId != NULL ? titleId : ""),
	  m_installTitleId(installTitleId != NULL ? installTitleId : ""),
	  m_bgdlTaskCount(bgdlTaskCount),
	  m_importTaskCount(importTaskCount),
	  m_promoteWaitResult(100),
	  m_waitDialogId(-1),
	  m_errorInfo()
{
	paf::thread::RMutex::main_thread_mutex.Lock();

	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 34;
	param.layout = SceCommonGuiDialog_2A68B4A4;

	paf::Plugin *applauncher = paf::Plugin::Find("applauncher_plugin");
	if (applauncher != NULL) {
		const wchar_t *waitMsg =
			applauncher->GetString(paf::IDParam(MSG_APPLAUNCHER_WAITING));
		param.text = (waitMsg != NULL) ? waitMsg : L"";
	}

	m_waitDialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	topmenuBeginBusy();

	paf::thread::RMutex::main_thread_mutex.Unlock();
}

/* (0x8128A0FA / 0x8128A1E6) */
InstallJob::~InstallJob()
{
	topmenuEndBusy();
	if (m_waitDialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(m_waitDialogId);
		m_waitDialogId = -1;
	}
}

/* (0x8128A4EC) */
int32_t InstallJob::Run()
{
	int installResult = 0;
	bool patched = false;

	if (g_topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIf =
			static_cast<ShellPluginInterface *>(g_topmenuPlugin->GetInterface(1));
		if (topmenuIf != NULL) {
			TopmenuInterfaceSimpleFunc notifyBegin =
				reinterpret_cast<TopmenuInterfaceSimpleFunc>(topmenuIf->func[27]);
			notifyBegin();
		}
	}

	if (g_topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIf =
			static_cast<ShellPluginInterface *>(g_topmenuPlugin->GetInterface(1));
		if (topmenuIf != NULL) {
			TopmenuInterfaceTitleFunc notifyTitleStart =
				reinterpret_cast<TopmenuInterfaceTitleFunc>(topmenuIf->func[76]);
			notifyTitleStart(m_titleId.c_str());
		}
	}

	BgdlTaskInfo failedTask;

	if (m_installKind == 0) {
		if (m_bgdlTaskCount != 0) {
			m_promoteWaitResult = topmenuSpawnWaitPromoteReady(&m_titleId);
			installResult = shellBgdlInstallTitleTasks(m_installTitleId.c_str(),
								   &failedTask, &patched);
			if (installResult != 0) {
				topmenuInstallErrorFill(installResult, &failedTask, &m_errorInfo);
			} else if (sce_paf_strncmp(m_installTitleId.c_str(), "PCSI00011",
						   m_installTitleId.length()) == 0) {
				installResult = shellBgdlInstallTitleTasksType2(
					m_titleId.c_str(), &failedTask);
				if (installResult != 0)
					topmenuInstallErrorFill(installResult, &failedTask,
								&m_errorInfo);
			}
		}

		if (installResult == 0 && m_importTaskCount != 0) {
			GamePluginInterface *gameIf = g_topWindow->m_gameIf;
			if (gameIf != NULL) {
				GamePluginContentImportRunFunc runImport =
					reinterpret_cast<GamePluginContentImportRunFunc>(
						gameIf->func[31]);
				installResult = runImport(m_installTitleId.c_str());
			} else {
				installResult = TOPMENU_ERROR_NO_GAME_PLUGIN;
			}
		}
	} else if (m_installKind == 1) {
		if (m_bgdlTaskCount != 0) {
			const char *target = m_installTitleId.empty() ?
				NULL : m_installTitleId.c_str();
			installResult = shellBgdlInstallTitleTasksType1(target, &failedTask);
			if (installResult != 0)
				topmenuInstallErrorFill(installResult, &failedTask, &m_errorInfo);
		}
	} else if (m_installKind == 2) {
		if (m_bgdlTaskCount != 0) {
			installResult = shellBgdlInstallTitleTasksOfKinds(
				m_installTitleId.c_str(), 3, 8, true, &failedTask, &patched);
			if (installResult != 0)
				topmenuInstallErrorFill(installResult, &failedTask, &m_errorInfo);
		}
	}

	SceInt32 completionStatus = (installResult == 0) ? 2 : 0;
	if (g_topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIf =
			static_cast<ShellPluginInterface *>(g_topmenuPlugin->GetInterface(1));
		if (topmenuIf != NULL) {
			TopmenuInterfaceTitleStatusFunc notifyDone =
				reinterpret_cast<TopmenuInterfaceTitleStatusFunc>(
					topmenuIf->func[77]);
			notifyDone(m_titleId.c_str(), completionStatus);
		}
	}

	if (patched && g_topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIf =
			static_cast<ShellPluginInterface *>(g_topmenuPlugin->GetInterface(1));
		if (topmenuIf != NULL) {
			TopmenuInterfaceTitleStatusFunc updateUpStatus =
				reinterpret_cast<TopmenuInterfaceTitleStatusFunc>(
					topmenuIf->func[TOPMENU_PLUGIN_FUNC_UP_STATUS]);
			updateUpStatus(m_titleId.c_str(), 3);
		}
	}

	if (m_errorInfo.errorCode != 0 && m_errorInfo.isAddcont && !m_errorInfo.path.empty())
		m_errorInfo.iconPath = lookupTitleIconPath(m_errorInfo.path.c_str());

	return installResult;
}

/* (0x8128B416) */
void InstallJob::Finish(int32_t result)
{
	(void)result;
	SceCommonGuiDialog_BC311DC0(m_waitDialogId);
	m_waitDialogId = -1;
	if (m_errorInfo.errorCode == TOPMENU_ERROR_PROMOTE_RETRY)
		topmenuSpawnHandlePromoteFailure(&m_titleId, m_promoteWaitResult);
	else
		topmenuInstallErrorShowDialog(&m_errorInfo);
}

/* (0x8128B7B0) Validate application storage availability (ux0:/gro0:) */
static int checkAppStorageAvailable(const LsdbAppInfoPtr *appInfo, SceUInt8 checkFlags)
{
	GamePluginInterface *gameIf = g_topWindow->m_gameIf;

	paf::string originalPath;
	if (!lsdbFindText(appInfo, LSDB_KEY_ORG_PATH, &originalPath))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	SceInt64 attr = 0;
	lsdbFindInt64(appInfo, LSDB_KEY_ATTRIBUTE, &attr);

	paf::LocalFileStat statInfo;
	int storageKind = 0;
	GamePluginGetStorageFunc getStorage =
		reinterpret_cast<GamePluginGetStorageFunc>(gameIf->func[GAME_PLUGIN_FUNC_GET_STORAGE]);
	if (getStorage(appInfo, &storageKind) == 0 && storageKind == 2 &&
	    paf::LocalFile::Getstat("ux0:", &statInfo) == GAME_PAF_ERROR_NO_DEVICE) {
		if ((attr & 0x100000) == 0)
			return TOPMENU_ERROR_NO_MEMORY_CARD;
		if ((checkFlags & 1) == 0)
			return TOPMENU_ERROR_NO_MC_WITH_PROMPT;
	}

	if (paf::LocalFile::Getstat(originalPath.c_str(), &statInfo) >= 0)
		return 0;

	if (originalPath.find("gro0:") != paf::string::npos)
		return TOPMENU_ERROR_CARD_EJECTED;
	return TOPMENU_ERROR_NO_GAMECARD;
}

/* (0x8128BA92) Validate LSDB title key presence after storage/savedata check error */
static int verifyTitleKeyPresent(int checkError, const LsdbAppInfoPtr *appInfo)
{
	LsdbAppInfo::iterator it = (*appInfo)->find(LSDB_KEY_TITLE_ID);
	if (it == (*appInfo)->end())
		return TOPMENU_ERROR_MISSING_LSDB_KEY;
	return checkError;
}

/* (0x8128BAE0) Enqueue InstallJob when pending BGDL or content-import tasks exist */
static int enqueueVitaInstallIfPending(const char *titleId, const char *installTitleId)
{
	int bgdlCount = shellBgdlCountTitleTasks(installTitleId);
	if (sce_paf_strncmp(installTitleId, "PCSI00011", sce_paf_strlen("PCSI00011")) == 0 &&
	    bgdlCount == 0) {
		if (shellBgdlTitleHasTasksOfKind(titleId, 2, 0xF))
			bgdlCount = 1;
	}

	int importCount = 0;
	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	if (gameIf != NULL) {
		GamePluginContentImportCheckFunc checkImport =
			reinterpret_cast<GamePluginContentImportCheckFunc>(gameIf->func[32]);
		importCount = checkImport(installTitleId);
	}

	if (bgdlCount == 0 && importCount == 0)
		return 0;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new InstallJob(0, titleId, installTitleId, bgdlCount, importCount));
	topmenuEnqueueInstallJob(&job);
	return TOPMENU_ERROR_INSTALL_QUEUED;
}

/* (0x8128BC80) Check trophy/account ownership against savedata account flags */
static int checkAccountTrophyOwnership(const LsdbAppInfoPtr *appInfo,
				       const TopmenuSaveDataInfo *saveInfo)
{
	paf::string category;
	if (!lsdbFindText(appInfo, LSDB_KEY_CATEGORY, &category))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	if (sce_paf_strcmp(category.c_str(), "gd") != 0 &&
	    sce_paf_strcmp(category.c_str(), "gp") != 0) {
		return 0;
	}

	paf::string originalPath;
	if (!lsdbFindText(appInfo, LSDB_KEY_ORG_PATH, &originalPath))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	if (gameIf == NULL)
		return TOPMENU_ERROR_NO_GAME_PLUGIN;

	int storageKind = 0;
	GamePluginGetStorageFunc getStorage =
		reinterpret_cast<GamePluginGetStorageFunc>(gameIf->func[GAME_PLUGIN_FUNC_GET_STORAGE]);
	int ret = getStorage(appInfo, &storageKind);
	if (ret < 0)
		return ret;
	if (storageKind != 1 && storageKind != 2 && storageKind != 3)
		return 0;

	if ((saveInfo->accountFlags & 1) == 0)
		return 0;

	if ((saveInfo->accountFlags & 2) != 0) {
		return (storageKind == 1) ?
			TOPMENU_ERROR_TROPHY_OTHER_UX0 : TOPMENU_ERROR_TROPHY_OTHER_CARD;
	}

	SceUInt8 rawAccountId[8];
	int regRet = sceRegMgrGetKeyBinForUser("/CONFIG/NP", "account_id", rawAccountId, 8);
	if (regRet < 0)
		return regRet;

	SceUInt64 accountId = 0;
	sce_paf_memcpy(&accountId, rawAccountId, sizeof(accountId));
	if (accountId == 0)
		return TOPMENU_ERROR_TROPHY_NO_ACCOUNT;

	return (storageKind == 1) ?
		TOPMENU_ERROR_TROPHY_OTHER_UX0 : TOPMENU_ERROR_TROPHY_OTHER_CARD;
}

/* (0x8128C0BC) Validate model whitelist, savedata ownership, and app version */
static int checkSaveDataAndVersion(const LsdbAppInfoPtr *appInfo, SceUInt8 checkFlags)
{
	GamePluginInterface *gameIf = g_topWindow->m_gameIf;

	paf::string titleId;
	if (!lsdbFindText(appInfo, LSDB_KEY_TITLE_ID, &titleId))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	bool allowed = false;
	int modelKind = 0;
	GamePluginAppAllowedFunc checkAllowed =
		reinterpret_cast<GamePluginAppAllowedFunc>(gameIf->func[47]);
	int ret = checkAllowed(titleId, &allowed, &modelKind);
	if (ret < 0)
		return ret;
	if (!allowed)
		return (modelKind == 1) ? TOPMENU_ERROR_MODEL_VITATV : TOPMENU_ERROR_MODEL_RESTRICTED;

	paf::string saveDirKey;
	if (!lsdbFindText(appInfo, 0x10954174u, &saveDirKey))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	paf::string originalPath;
	if (!lsdbFindText(appInfo, LSDB_KEY_ORG_PATH, &originalPath))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	SceInt64 attr = 0;
	lsdbFindInt64(appInfo, LSDB_KEY_ATTRIBUTE, &attr);

	char saveTitleDir[16];
	sce_paf_strncpy(saveTitleDir, saveDirKey.c_str(), 9);
	saveTitleDir[9] = '\0';

	TopmenuSaveDataInfo saveInfo;
	sce_paf_memset(&saveInfo, 0, sizeof(saveInfo));
	int saveRet = sceAppMgrGetSaveDataInfo(originalPath.c_str(), saveTitleDir,
					       static_cast<int>(attr), &saveInfo);

	bool isGro0 = (originalPath.find("gro0:") != paf::string::npos);
	bool ignoreSaveErr = (saveRet == APPMGR_ERROR_NO_SAVEDATA ||
			      saveRet == APPMGR_ERROR_SAVEDATA_ABSENT ||
			      (isGro0 && saveRet == APPMGR_ERROR_NO_CARD_MOUNT));

	if (saveRet < 0 && !ignoreSaveErr) {
		if (saveRet == APPMGR_ERROR_SYS_UPDATE_NEEDED) {
			GamePluginCheckUpdateFunc checkUpdate =
				reinterpret_cast<GamePluginCheckUpdateFunc>(gameIf->func[40]);
			checkUpdate(appInfo, unloadSysupchkCallback, NULL, false, 3, 33);
			return TOPMENU_ERROR_SYSUPCHK_WAIT;
		}
		if (saveInfo.versionMismatch != 0) {
			return ((checkFlags & 2) != 0) ?
				TOPMENU_ERROR_SAVEDATA_PROMPT : TOPMENU_ERROR_SAVEDATA_OTHER_USR;
		}
	} else if (saveRet == 0 && saveInfo.versionMismatch != 0) {
		return ((checkFlags & 2) != 0) ?
			TOPMENU_ERROR_SAVEDATA_PROMPT : TOPMENU_ERROR_SAVEDATA_OTHER_USR;
	}

	if (!ignoreSaveErr && (checkFlags & 4) == 0) {
		int trophyRet = checkAccountTrophyOwnership(appInfo, &saveInfo);
		if (trophyRet < 0)
			return trophyRet;
	}

	SceUInt32 installedAppVer = 0;
	paf::string appVerStr;
	if (lsdbFindText(appInfo, LSDB_KEY_APP_VER, &appVerStr) && !appVerStr.empty()) {
		SceUInt32 place = 1;
		for (int idx = static_cast<int>(appVerStr.length()) - 1; idx >= 0; --idx) {
			char ch = appVerStr.c_str()[idx];
			if (ch != '.') {
				installedAppVer += static_cast<SceUInt32>(ch - '0') * place;
				place *= 10;
			}
		}
	}

	return (installedAppVer >= saveInfo.requiredAppVer) ? 0 : TOPMENU_ERROR_SAVEDATA_NEWER_VER;
}

/* (0x8128D0F4) Enqueue PSM runtime InstallJob if pending */
static int enqueuePsmRuntimeInstallIfPending(const LsdbAppInfoPtr *appInfo)
{
	paf::string titleId;
	if (!lsdbFindText(appInfo, LSDB_KEY_TITLE_ID, &titleId) || titleId.empty())
		return 0;

	if (!shellBgdlTitleHasTasksOfKind(titleId.c_str(), 3, 0xFFFFFFFFu))
		return 0;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new InstallJob(2, titleId.c_str(), titleId.c_str(), 1, 0));
	topmenuEnqueueInstallJob(&job);
	return TOPMENU_ERROR_INSTALL_QUEUED;
}

} /* namespace */

/* (0x81289E86) */
TopmenuInstallError::TopmenuInstallError()
	: errorCode(0), isAddcont(false), url(), path(), title(), titleId(), iconPath()
{
}

/* (0x8128A1F8) */
void topmenuInstallErrorFill(SceInt32 errorCode, const BgdlTaskInfo *taskInfo,
			     TopmenuInstallError *outError)
{
	if (taskInfo == NULL || outError == NULL)
		return;
	outError->errorCode = errorCode;
	outError->url = taskInfo->url;
	outError->path = taskInfo->path;
	outError->title = taskInfo->title;
	outError->titleId = taskInfo->titleId;
	outError->isAddcont = (taskInfo->kind == 2);
}

/* (0x8128ACC8) */
void topmenuInstallErrorShowDialog(const TopmenuInstallError *errorInfo)
{
	paf::wstring heading(L"");
	paf::wstring message(L"");
	paf::wstring titleText(L"");
	paf::intrusive_ptr<paf::graph::Surface> iconSurface;
	paf::intrusive_ptr<paf::graph::Surface> badgeSurface;
	bool fromFallback = false;

	if (errorInfo->errorCode == 0)
		return;

	const wchar_t *headingStr =
		g_topmenuPlugin->GetString(paf::IDParam(MSG_INSTALL_ERROR_HEADING));
	heading = (headingStr != NULL) ? headingStr : L"";

	bool showBadge = false;
	switch (errorInfo->errorCode) {
	case NPDRM_ERROR_ACTIVATION_B:
	case NPDRM_ERROR_ACTIVATION_A: {
		const wchar_t *str =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_INSTALL_ERR_ACTIVATION));
		message = (str != NULL) ? str : L"";
		showBadge = true;
		break;
	}
	case NPDRM_ERROR_NO_LICENSE:
		if (errorInfo->isAddcont) {
			const wchar_t *str =
				g_topmenuPlugin->GetString(paf::IDParam(MSG_INSTALL_ERR_ADDCONT_LIC));
			message = (str != NULL) ? str : L"";
			showBadge = true;
		} else {
			const wchar_t *str =
				g_topmenuPlugin->GetString(paf::IDParam(MSG_INSTALL_ERR_APP_LIC));
			message = (str != NULL) ? str : L"";
		}
		break;
	case TOPMENU_ERROR_CARD_EJECTED: {
		const wchar_t *str =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_INSTALL_ERR_CARD_REMOVED));
		message = (str != NULL) ? str : L"";
		break;
	}
	case GAME_ERROR_NO_INSTALL_INFO: {
		const wchar_t *str =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_INSTALL_ERR_NO_INFO));
		message = (str != NULL) ? str : L"";
		break;
	}
	case GAME_ERROR_APP_RUNNING: {
		const wchar_t *str =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_INSTALL_ERR_APP_RUNNING));
		message = (str != NULL) ? str : L"";
		break;
	}
	default: {
		char extCode[16];
		sceErrorGetExternalString(extCode, errorInfo->errorCode);
		paf::common::String extCodeStr(extCode);
		paf::wstring extCodeWide = extCodeStr.GetWString();
		message = L"(";
		message += extCodeWide.c_str();
		message += L")";
		break;
	}
	}

	if (loadInstallErrorIcon(errorInfo->url, errorInfo->iconPath,
				 &iconSurface, &fromFallback) != 0) {
		return;
	}

	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin == NULL)
		return;
	GamePluginInterface *gameIf =
		static_cast<GamePluginInterface *>(gamePlugin->GetInterface(1));
	if (gameIf == NULL)
		return;

	paf::common::String titleConv("");
	if (!errorInfo->titleId.empty())
		titleConv = paf::common::String(errorInfo->titleId.c_str());
	titleText = titleConv.GetWString();

	if (showBadge) {
		paf::Plugin *liveareaPlugin = paf::Plugin::Find("livearea_plugin");
		if (liveareaPlugin != NULL) {
			badgeSurface = liveareaPlugin->GetTexture(
				paf::IDParam(TEX_LIVEAREA_ERROR_BADGE));
		}
	}

	SceInt32 dialogId = -1;
	paf::intrusive_ptr<paf::graph::Surface> *badgePtr =
		(badgeSurface.get() != NULL) ? &badgeSurface : NULL;
	paf::wstring emptySubText(L"");

	GamePluginNoticeOpenFunc openNotice =
		reinterpret_cast<GamePluginNoticeOpenFunc>(gameIf->func[37]);
	int openRet = openNotice(&dialogId, onInstallErrorNoticeButton, NULL,
				 errorInfo->errorCode, iconSurface, titleText, heading, message,
				 fromFallback, badgePtr, 0, emptySubText);
	if (openRet == 0 && dialogId >= 0) {
		if (topmenuIsPowerSuspendNormal()) {
			topmenuDialogRegisterAndHold(dialogId);
		} else {
			SceShell_7FE4E732(9, topmenuSpawnOnInstallNoticeEvent,
					  reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
		}
		topmenuDialogRegister(dialogId);
	}
}

/* (0x8128B44A) */
void topmenuExecCheckInit(void)
{
	TopmenuExecCheckContext *ctx = new TopmenuExecCheckContext();
	if (ctx != NULL) {
		ctx->jobQueue = NULL;
		ctx->waitDialogSema = sceKernelCreateSema("SceTopMenuWaitDialog", 0, 0, 1, NULL);

		paf::job::JobQueue::Option opt;
		opt.priority = 0x1000010A;
		opt.stackSize = 0x4000;
		opt.numThreads = 1;
		ctx->jobQueue = new paf::job::JobQueue("SceShellAppExecChk", &opt);
	}
	g_execCheckCtx = ctx;
}

/* (0x8128B4D2) */
void topmenuExecCheckTerm(void)
{
	if (g_execCheckCtx != NULL) {
		sceKernelDeleteSemaForUser(g_execCheckCtx->waitDialogSema);
		delete g_execCheckCtx->jobQueue;
		delete g_execCheckCtx;
		g_execCheckCtx = NULL;
	}
}

/* (0x8128B506) */
bool topmenuResolvePromoteTitleId(const LsdbAppInfoPtr *appInfo, paf::string *outTitleId)
{
	const char *category = sceLsdbGetCategory(appInfo);
	if (category == NULL || sce_paf_strlen(category) == 0)
		return false;

	paf::string catStr(category);
	bool matched = false;
	for (unsigned int idx = 0; idx < 6; ++idx) {
		if (sce_paf_strncmp(kPromoteCategories[idx], catStr.c_str(), catStr.length()) == 0) {
			matched = true;
			break;
		}
	}
	if (!matched)
		return false;

	if (!lsdbFindText(appInfo, LSDB_KEY_TITLE_ID, outTitleId))
		return false;
	return !outTitleId->empty();
}

/* (0x8128B6A8) */
void topmenuSysupchkCancelPowerKey(void *userData)
{
	topmenuPopPowerKeyHandler(topmenuSysupchkCancelPowerKey, userData);
	topmenuRemoveTabLeaveListener(static_cast<SceInt32>(reinterpret_cast<intptr_t>(userData)),
				      topmenuSysupchkCancelTabLeave, userData);
	paf::Plugin::UnloadAsync("sysupchk_plugin", NULL, 0);
	paf::thread::CallQueue::main_thread_callqueue.Enqueue("DeleteTab", deleteTabDeferredTask,
							      userData);
}

/* (0x8128B700) */
void topmenuSysupchkCancelTabLeave(SceInt32 eventType, void *sender, SceInt32 eventArg,
				   void *userData)
{
	(void)eventType;
	(void)sender;
	(void)eventArg;
	topmenuSysupchkCancelPowerKey(userData);
}

/* (0x8128B758) */
void topmenuSysupchkDoneCallback(SceInt32 result, void *userData)
{
	(void)result;
	topmenuSysupchkCancelPowerKey(userData);
}

/* (0x8128C8EC) */
int topmenuCheckVitaAppExec(const LsdbAppInfoPtr *appInfo, SceUInt8 checkFlags)
{
	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	if (gameIf == NULL)
		return TOPMENU_ERROR_NO_GAME_PLUGIN;

	int ret = checkAppStorageAvailable(appInfo, checkFlags);
	if (ret < 0) {
		if (ret == TOPMENU_ERROR_SYSUPCHK_WAIT)
			return TOPMENU_ERROR_SYSUPCHK_DEFERRED;
		int verified = verifyTitleKeyPresent(ret, appInfo);
		return (verified >= 0) ? TOPMENU_ERROR_SYSUPCHK_DEFERRED : verified;
	}

	paf::string promoteTitleId;
	if (topmenuResolvePromoteTitleId(appInfo, &promoteTitleId)) {
		ret = enqueueVitaInstallIfPending(promoteTitleId.c_str(), promoteTitleId.c_str());
		if (ret < 0)
			return ret;
	}

	ret = checkSaveDataAndVersion(appInfo, checkFlags);
	if (ret < 0) {
		if (ret == TOPMENU_ERROR_SYSUPCHK_WAIT)
			return TOPMENU_ERROR_SYSUPCHK_DEFERRED;
		int verified = verifyTitleKeyPresent(ret, appInfo);
		return (verified >= 0) ? TOPMENU_ERROR_SYSUPCHK_DEFERRED : verified;
	}

	paf::string originalPath;
	if (!lsdbFindText(appInfo, LSDB_KEY_ORG_PATH, &originalPath))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	GamePluginAlarmSetFunc setAlarm =
		reinterpret_cast<GamePluginAlarmSetFunc>(gameIf->func[25]);
	ret = setAlarm(originalPath.c_str(), NULL);
	if (ret < 0) {
		if (ret != APPMGR_ERROR_LICENSE_EXPIRED) {
			GamePluginAlarmRemoveFunc removeAlarm =
				reinterpret_cast<GamePluginAlarmRemoveFunc>(gameIf->func[28]);
			removeAlarm(appInfo);
		}
		return ret;
	}

	if ((checkFlags & 8) == 0) {
		paf::string titleId;
		if (!lsdbFindText(appInfo, LSDB_KEY_TITLE_ID, &titleId))
			return TOPMENU_ERROR_MISSING_LSDB_KEY;

		bool hasAddcont = false;
		GamePluginAddcontInfoCheckFunc checkAddcont =
			reinterpret_cast<GamePluginAddcontInfoCheckFunc>(gameIf->func[45]);
		checkAddcont(titleId.c_str(), &hasAddcont);
		if (hasAddcont)
			return TOPMENU_ERROR_ADDCONT_PENDING;
	}

	return 0;
}

/* (0x8128CAF2) */
int topmenuCheckPspAppExec(const LsdbAppInfoPtr *appInfo, SceUInt32 checkFlags)
{
	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	if (gameIf == NULL)
		return TOPMENU_ERROR_NO_GAME_PLUGIN;

	paf::string titleId;
	if (!lsdbFindText(appInfo, LSDB_KEY_TITLE_ID, &titleId))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	bool allowed = false;
	int modelKind = 0;
	GamePluginAppAllowedFunc checkEmuAllowed =
		reinterpret_cast<GamePluginAppAllowedFunc>(gameIf->func[48]);
	int ret = checkEmuAllowed(titleId, &allowed, &modelKind);
	if (ret < 0)
		return ret;
	if (!allowed)
		return (modelKind == 1) ? TOPMENU_ERROR_MODEL_VITATV : TOPMENU_ERROR_MODEL_RESTRICTED;

	TopmenuLicenseCheckResult licenseStatus;
	sce_paf_memset(&licenseStatus, 0, sizeof(licenseStatus));

	GamePluginLicenseCheckFunc checkLicense =
		reinterpret_cast<GamePluginLicenseCheckFunc>(gameIf->func[24]);
	ret = checkLicense(appInfo, &licenseStatus);
	if (ret < 0) {
		if (ret == NPDRM_ERROR_SYS_UPDATE_NEEDED) {
			GamePluginCheckUpdateFunc checkUpdate =
				reinterpret_cast<GamePluginCheckUpdateFunc>(gameIf->func[40]);
			if (checkUpdate(appInfo, unloadSysupchkCallback, NULL, false, 3, 33) < 0)
				return TOPMENU_ERROR_SYSUPCHK_WAIT;
			return TOPMENU_ERROR_SYSUPCHK_DEFERRED;
		}
		if (ret == TOPMENU_ERROR_SYSUPCHK_WAIT)
			return TOPMENU_ERROR_SYSUPCHK_DEFERRED;
		if ((*appInfo)->find(LSDB_KEY_TITLE_ID) == (*appInfo)->end())
			return TOPMENU_ERROR_MISSING_LSDB_KEY;
		return ret;
	}

	int categoryKind = 0;
	GamePluginGetCategoryKindFunc getCategoryKind =
		reinterpret_cast<GamePluginGetCategoryKindFunc>(
			gameIf->func[GAME_PLUGIN_FUNC_GET_CATEGORY_KIND]);
	if (getCategoryKind(appInfo, &categoryKind) < 0)
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	if (categoryKind != 3) {
		paf::string pspInstallId;
		if (bgdlResolvePspInstallTitleId(appInfo, &pspInstallId)) {
			int appTaskCount = shellBgdlCountAppTasks(pspInstallId.c_str());
			if (appTaskCount != 0) {
				paf::common::SharedPtr<paf::job::JobItem> job(
					new InstallJob(1, pspInstallId.c_str(),
						       pspInstallId.c_str(), appTaskCount, 0));
				topmenuEnqueueInstallJob(&job);
				return TOPMENU_ERROR_INSTALL_QUEUED;
			}
		}
	}

	paf::string originalPath;
	if (!lsdbFindText(appInfo, LSDB_KEY_ORG_PATH, &originalPath))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	GamePluginAlarmSetFunc setAlarm =
		reinterpret_cast<GamePluginAlarmSetFunc>(gameIf->func[25]);
	ret = setAlarm(originalPath.c_str(), &licenseStatus);
	if (ret < 0) {
		GamePluginAlarmRemoveFunc removeAlarm =
			reinterpret_cast<GamePluginAlarmRemoveFunc>(gameIf->func[28]);
		removeAlarm(appInfo);
		return ret;
	}

	if ((checkFlags & 8) != 0)
		return 0;

	paf::string addcontTitleId;
	if (!lsdbFindText(appInfo, LSDB_KEY_TITLE_ID, &addcontTitleId))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	bool hasAddcont = false;
	GamePluginAddcontInfoCheckFunc checkAddcont =
		reinterpret_cast<GamePluginAddcontInfoCheckFunc>(gameIf->func[45]);
	checkAddcont(addcontTitleId.c_str(), &hasAddcont);
	return hasAddcont ? TOPMENU_ERROR_ADDCONT_PENDING : 0;
}

/* (0x8128CFC2) */
int topmenuCheckPsmAppExec(const LsdbAppInfoPtr *appInfo)
{
	paf::string titleId;
	if (!lsdbFindText(appInfo, LSDB_KEY_TITLE_ID, &titleId))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	int ret = enqueueVitaInstallIfPending(titleId.c_str(), "PCSI00011");
	if (ret < 0)
		return ret;

	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	if (gameIf == NULL)
		return TOPMENU_ERROR_NO_GAME_PLUGIN;

	bool allowed = false;
	int modelKind = 0;
	GamePluginAppAllowedFunc checkPsmAllowed =
		reinterpret_cast<GamePluginAppAllowedFunc>(gameIf->func[50]);
	ret = checkPsmAllowed(titleId, &allowed, &modelKind);
	if (ret < 0)
		return ret;
	if (allowed)
		return 0;
	return (modelKind == 1) ? TOPMENU_ERROR_MODEL_VITATV : TOPMENU_ERROR_MODEL_RESTRICTED;
}

/* (0x8128D30E) */
int topmenuCheckPsmRuntimeExec(const LsdbAppInfoPtr *appInfo)
{
	int ret = enqueuePsmRuntimeInstallIfPending(appInfo);
	if (ret < 0)
		return ret;

	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	if (gameIf == NULL)
		return TOPMENU_ERROR_NO_GAME_PLUGIN;

	paf::string titleId;
	if (!lsdbFindText(appInfo, LSDB_KEY_TITLE_ID, &titleId))
		return TOPMENU_ERROR_MISSING_LSDB_KEY;

	if (titleId.length() != 9)
		return GAME_ERROR_BAD_SAVEDATA_DIR;

	const char *chars = titleId.c_str();
	for (SceSize idx = 0; idx < titleId.length(); ++idx) {
		char ch = chars[idx];
		bool isUpper = (ch >= 'A' && ch <= 'Z');
		bool isDigit = (ch >= '0' && ch <= '9');
		if (!isUpper && !isDigit)
			return GAME_ERROR_BAD_SAVEDATA_DIR;
	}

	bool allowed = false;
	int modelKind = 0;
	GamePluginAppAllowedFunc checkPsmAllowed =
		reinterpret_cast<GamePluginAppAllowedFunc>(gameIf->func[50]);
	ret = checkPsmAllowed(titleId, &allowed, &modelKind);
	if (ret < 0)
		return ret;
	if (allowed)
		return 0;
	return (modelKind == 1) ? TOPMENU_ERROR_MODEL_VITATV : TOPMENU_ERROR_MODEL_RESTRICTED;
}

/* (0x8128D510) */
void topmenuEnqueueInstallJob(paf::common::SharedPtr<paf::job::JobItem> *job)
{
	g_execCheckCtx->jobQueue->Enqueue(*job);
}
