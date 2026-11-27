/*
 * app_check_job.cpp: AppCheckJob and LiveArea pre-launch check dialogs
 * (0x81288666-0x81289E85).
 * Validates launch eligibility (model whitelist, free-trial license period and PSN
 * sign-in state, system/PSM runtime version) before opening an application's LiveArea.
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_NO_GAME_PLUGIN	= -2146420474; /* 0x80103906 */
const SceInt32 TOPMENU_ERROR_MODEL_RESTRICTED	= -2146420420; /* 0x8010393C */
const SceInt32 TOPMENU_ERROR_MODEL_VITATV	= -2146420421; /* 0x8010393B */
const SceInt32 TOPMENU_ERROR_FTT_CONFIRM	= -2146420417; /* 0x8010393F */
const SceInt32 TOPMENU_ERROR_FTT_OFFLINE	= -2146420416; /* 0x80103940 */
const SceInt32 TOPMENU_ERROR_FTT_EXPIRED	= -2146420415; /* 0x80103941 */
const SceInt32 TOPMENU_ERROR_FTT_NEEDS_UPDATE	= -2146420414; /* 0x80103942 */
const SceInt32 GAME_ERROR_SYSUPCHK_REQUIRED	= -2146430687; /* 0x80101121 */
const SceInt32 GAME_ERROR_PSM_INSTALL_REQUIRED	= -2146430643; /* 0x8010114D */
const SceInt32 GAME_ERROR_PSM_UPDATE_REQUIRED	= -2146430641; /* 0x8010114F */

const SceUInt32 MSG_FTT_TRIAL_REMAINING		= 0x196AA9A2u; /* 426412162 */
const SceUInt32 MSG_FTT_BUTTON_START		= 0x8A9C57E7u; /* -1969464713 */
const SceUInt32 MSG_FTT_OFFLINE_SIGNIN		= 0xF7B35F99u; /* -139239015 */
const SceUInt32 MSG_FTT_BUTTON_SIGNIN		= 0x56684E5Eu; /* 1449665214 */
const SceUInt32 MSG_PSM_UPDATE_PROMPT		= 0xEFD342F5u; /* -271357451 */
const SceUInt32 MSG_PSM_INSTALL_PROMPT		= 0x4E492160u; /* 1313445728 */
const SceUInt32 TEMPLATE_PSM_RUNTIME_ICON	= 0xD04970AEu; /* -800505938 */
const SceUInt32 SOUND_TOPMENU_DIALOG_CLOSE	= 0xD74A1EA5u; /* -682994459 */

typedef int (*GamePluginAppAllowedFunc)(const paf::string &titleId, bool *allowed, int *model);
typedef int (*GamePluginFttReadFunc)(const char *titleId, TopmenuFttLicenseInfo *outInfo);
typedef int (*GamePluginAppCheckStartFunc)(const LsdbAppInfoPtr *info, char *outVersion);
typedef int (*GamePluginAppCheckUpdateFunc)(const LsdbAppInfoPtr *info,
					    void (*callback)(SceInt32 result, void *userData),
					    void *userData, bool gameCard, int mode, int priority);

struct AppCheckCancelContext {
	SceInt32 pageKey;
	SceInt32 dialogId;
};

static void cancelLiveAreaOpenTask(void *userData)
{
	topmenuCancelLiveAreaOpen(static_cast<SceInt32>(reinterpret_cast<intptr_t>(userData)));
}

static void deleteTabTask(void *userData)
{
	topmenuDeleteTabByPageKey(static_cast<SceInt32>(reinterpret_cast<intptr_t>(userData)));
}

static void onErrorDialogPowerKey(void *userData);
static void onErrorDialogTabLeave(SceInt32 eventType, void *sender, SceInt32 eventArg,
				  void *userData);

static void dismissErrorDialogAndDeleteTab(AppCheckCancelContext *ctx)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	if (ctx == NULL) {
		paf::thread::RMutex::main_thread_mutex.Unlock();
		return;
	}
	if (ctx->dialogId != -1)
		SceCommonGuiDialog_BC311DC0(ctx->dialogId);
	topmenuPopPowerKeyHandler(onErrorDialogPowerKey, ctx);
	topmenuRemoveTabLeaveListener(ctx->pageKey, onErrorDialogTabLeave, ctx);
	paf::thread::CallQueue::main_thread_callqueue.Enqueue(
		"DeleteTab", deleteTabTask,
		reinterpret_cast<void *>(static_cast<intptr_t>(ctx->pageKey)));
	paf::thread::RMutex::main_thread_mutex.Unlock();
}

static void onErrorDialogPowerKey(void *userData)
{
	dismissErrorDialogAndDeleteTab(static_cast<AppCheckCancelContext *>(userData));
}

static void onErrorDialogTabLeave(SceInt32 eventType, void *sender, SceInt32 eventArg,
				  void *userData)
{
	(void)eventType;
	(void)sender;
	(void)eventArg;
	dismissErrorDialogAndDeleteTab(static_cast<AppCheckCancelContext *>(userData));
}

/* Error dialog handler that cleans up AppCheckCancelContext when dismissed */
class AppCheckErrorDialogHandler : public CommonGuiDialogHandler {
public:
	explicit AppCheckErrorDialogHandler(AppCheckCancelContext *ctx) : m_context(ctx) {}
	virtual ~AppCheckErrorDialogHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 0) {
			if (m_context != NULL) {
				topmenuPopPowerKeyHandler(onErrorDialogPowerKey, m_context);
				topmenuRemoveTabLeaveListener(m_context->pageKey,
							      onErrorDialogTabLeave, m_context);
				topmenuCloseDialogAndDeleteTab(-1, 0, m_context->pageKey);
				delete m_context;
				m_context = NULL;
			}
			topmenuDialogOnPowerHold(dialogId);
		} else {
			SceCommonGuiDialog_BC311DC0(dialogId);
		}
	}

	AppCheckCancelContext *m_context;
};

class AppCheckJob : public paf::job::JobItem {
public:
	explicit AppCheckJob(TopmenuAppProcess *process);
	virtual ~AppCheckJob();
	virtual int32_t Run();
	virtual void Finish(int32_t result);

	int CheckModelAllowed();
	int CheckFreeTrialLicense();
	bool HandleFreeTrialResult(SceInt32 errorCode, SceInt32 pageKey);

	TopmenuAppProcess *m_process;		/* +0x1C */
	LsdbAppInfoPtr m_appInfo;		/* +0x20 */
	bool m_hasAppInfo;			/* +0x28 */
	paf::string m_psmVersion;		/* +0x2C */
	TopmenuFttLicenseInfo m_fttInfo;	/* +0x38 */
};

typedef char AppCheckJobSizeCheck[sizeof(AppCheckJob) == 128 ? 1 : -1];

/* Dialog handler for FTT trial remaining confirmation (0x81289B28) */
class FttConfirmDialogHandler : public CommonGuiDialogHandler {
public:
	FttConfirmDialogHandler(SceInt32 pageKey, TopmenuAppProcess *process)
		: m_pageKey(pageKey), m_process(process), m_confirmed(false)
	{
	}
	virtual ~FttConfirmDialogHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 0) {
			if (!m_confirmed) {
				topmenuCancelLiveAreaOpen(m_pageKey);
				topmenuDeleteTabByPageKey(m_pageKey);
			}
			topmenuDialogOnPowerHold(dialogId);
			return;
		}
		if (button == 2) {
			m_process->m_pendingResume = true;
			paf::common::SharedPtr<paf::job::JobItem> job(new AppCheckJob(m_process));
			g_topWindow->m_jobQueue->Enqueue(job);
			m_process->m_checkJob = job;
			m_confirmed = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
	}

	SceInt32 m_pageKey;
	TopmenuAppProcess *m_process;
	bool m_confirmed;
};

/* Dialog handler for FTT offline sign-in prompt (0x81289CD0) */
class FttSignInDialogHandler : public CommonGuiDialogHandler {
public:
	FttSignInDialogHandler(SceInt32 pageKey, TopmenuAppProcess *process)
		: m_pageKey(pageKey), m_process(process), m_signInStarted(false)
	{
	}
	virtual ~FttSignInDialogHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 0) {
			if (!m_signInStarted) {
				topmenuCancelLiveAreaOpen(m_pageKey);
				topmenuDeleteTabByPageKey(m_pageKey);
			}
			topmenuDialogOnPowerHold(dialogId);
			return;
		}
		if (button == 2) {
			paf::common::SharedPtr<paf::job::JobItem> job;
			topmenuStartPsnSignInDialogJob(&job, m_process);
			m_process->m_checkJob = job;
			m_signInStarted = true;
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
	}

	SceInt32 m_pageKey;
	TopmenuAppProcess *m_process;
	bool m_signInStarted;
};

/* Dialog handler for PSM runtime install/update prompt (0x81289A34) */
class PsmUpdateDialogHandler : public CommonGuiDialogHandler {
public:
	PsmUpdateDialogHandler(SceInt32 pageKey, const char *titleId)
		: m_pageKey(pageKey), m_reservedJob(), m_titleId(titleId != NULL ? titleId : "")
	{
	}
	virtual ~PsmUpdateDialogHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 0) {
			topmenuDialogOnPowerHold(dialogId);
			topmenuCancelLiveAreaOpen(m_pageKey);
			topmenuDeleteTabByPageKey(m_pageKey);
			return;
		}
		if (button == 2) {
			paf::string titleCopy(m_titleId);
			paf::common::SharedPtr<paf::job::JobItem> job;
			topmenuStartPsmUpdateJob(&job, &titleCopy, m_pageKey);
		}
		SceCommonGuiDialog_BC311DC0(dialogId);
	}

	SceInt32 m_pageKey;
	paf::common::SharedPtr<paf::job::JobItem> m_reservedJob;
	paf::string m_titleId;
};

/* (0x81288C5C) */
static void showFttConfirmDialog(SceInt32 pageKey, TopmenuAppProcess *process,
				 const paf::wstring &expireText, const paf::wstring &minutesText)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.layout = SceCommonGuiDialog_92752DE2;
	param.soundId = SOUND_TOPMENU_DIALOG_CLOSE;
	param.notifyClose = true;

	const wchar_t *fmt = g_topmenuPlugin->GetString(paf::IDParam(MSG_FTT_TRIAL_REMAINING));
	param.text = shellFormatText(fmt, expireText.c_str(), minutesText.c_str());
	param.handler = new FttConfirmDialogHandler(pageKey, process);

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0) {
		topmenuDialogRegister(dialogId);
		topmenuDialogRegisterAndHold(dialogId);

		paf::ui::Widget *btnWidget = SceCommonGuiDialog_8947FC23(dialogId, 13);
		if (btnWidget != NULL && btnWidget->IsInherit(paf::ui::Button::TypeName())) {
			const wchar_t *label =
				g_topmenuPlugin->GetString(paf::IDParam(MSG_FTT_BUTTON_START));
			paf::wstring labelStr(label != NULL ? label : L"");
			btnWidget->SetString(labelStr);
		}
	}
}

/* (0x81288F24) */
static void showFttSignInDialog(SceInt32 pageKey, TopmenuAppProcess *process)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.layout = SceCommonGuiDialog_92752DE2;
	param.soundId = SOUND_TOPMENU_DIALOG_CLOSE;
	param.notifyClose = true;

	const wchar_t *msg = g_topmenuPlugin->GetString(paf::IDParam(MSG_FTT_OFFLINE_SIGNIN));
	param.text = (msg != NULL) ? msg : L"";
	param.handler = new FttSignInDialogHandler(pageKey, process);

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0) {
		topmenuDialogRegister(dialogId);
		topmenuDialogRegisterAndHold(dialogId);

		paf::ui::Widget *btnWidget = SceCommonGuiDialog_8947FC23(dialogId, 13);
		if (btnWidget != NULL && btnWidget->IsInherit(paf::ui::Button::TypeName())) {
			const wchar_t *label =
				g_topmenuPlugin->GetString(paf::IDParam(MSG_FTT_BUTTON_SIGNIN));
			paf::wstring labelStr(label != NULL ? label : L"");
			btnWidget->SetString(labelStr);
		}
	}
}

/* (0x81289450) */
static void showPsmUpdateDialog(const char *titleId, SceInt32 pageKey, SceInt32 errorCode)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.layout = SceCommonGuiDialog_92752DE2;
	param.soundId = SOUND_TOPMENU_DIALOG_CLOSE;
	param.notifyClose = true;

	SceUInt32 msgId = (errorCode == GAME_ERROR_PSM_UPDATE_REQUIRED) ?
		MSG_PSM_UPDATE_PROMPT : MSG_PSM_INSTALL_PROMPT;
	const wchar_t *msg = g_topmenuPlugin->GetString(paf::IDParam(msgId));
	param.text = (msg != NULL) ? msg : L"";
	param.handler = new PsmUpdateDialogHandler(pageKey, titleId);

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0) {
		topmenuDialogRegister(dialogId);
		topmenuDialogRegisterAndHold(dialogId);

		paf::ui::Widget *boxWidget = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (boxWidget != NULL && boxWidget->IsInherit(paf::ui::Box::TypeName())) {
			paf::Plugin::TemplateOpenParam openParam;
			g_topmenuPlugin->TemplateOpen(boxWidget,
						      paf::IDParam(TEMPLATE_PSM_RUNTIME_ICON),
						      openParam);
		}
	}
}

/* (0x81288666) */
AppCheckJob::AppCheckJob(TopmenuAppProcess *process)
	: paf::job::JobItem("AppCheckJob", NULL),
	  m_process(process),
	  m_appInfo(),
	  m_hasAppInfo(false),
	  m_psmVersion()
{
	if (process->m_appInfo.get() != NULL) {
		m_appInfo = process->m_appInfo;
		m_hasAppInfo = true;
	} else {
		m_appInfo.reset(new LsdbAppInfo());
	}

	sce_paf_memset(&m_fttInfo, 0, sizeof(m_fttInfo));
	topmenuBeginBusy();
}

/* (0x81288922 / 0x812889A6) */
AppCheckJob::~AppCheckJob()
{
}

/* (0x812889B8) */
int AppCheckJob::CheckModelAllowed()
{
	bool allowed = false;
	int modelKind = 0;

	paf::string titleId(m_process->m_titleId);
	GamePluginAppAllowedFunc checkAllowed =
		reinterpret_cast<GamePluginAppAllowedFunc>(g_topWindow->m_gameIf->func[47]);
	int ret = checkAllowed(titleId, &allowed, &modelKind);
	if (ret < 0)
		return ret;
	if (allowed)
		return 0;

	SceUInt8 patchStatus[3] = { 0, 0, 0 };
	if (shellBgdlGetPatchVersion(m_process->m_titleId.c_str(), patchStatus) == 0 &&
	    patchStatus[0] != 0 &&
	    ((modelKind == 1 && patchStatus[2] != 0) ||
	     (modelKind == 0 && patchStatus[1] != 0))) {
		return 0;
	}

	return (modelKind == 1) ? TOPMENU_ERROR_MODEL_VITATV : TOPMENU_ERROR_MODEL_RESTRICTED;
}

/* (0x81288A9C) */
int AppCheckJob::CheckFreeTrialLicense()
{
	if (topmenuHasPendingFttUpdateTask(&m_process->m_titleId))
		return TOPMENU_ERROR_FTT_NEEDS_UPDATE;

	int ret = CheckModelAllowed();
	if (ret != 0)
		return ret;

	paf::thread::RMutex::main_thread_mutex.Lock();
	SceInt32 signInState = topmenuGetPsnSignInState();
	paf::thread::RMutex::main_thread_mutex.Unlock();

	ret = (signInState == 1) ? 0 : TOPMENU_ERROR_FTT_OFFLINE;
	if (ret == 0) {
		GamePluginFttReadFunc readFtt =
			reinterpret_cast<GamePluginFttReadFunc>(g_topWindow->m_gameIf->func[58]);
		ret = readFtt(m_process->m_titleId.c_str(), &m_fttInfo);
		if (ret == 0) {
			SceRtcTick networkTick;
			ret = sceRtcGetCurrentNetworkTick(&networkTick);
			if (ret == 0) {
				SceRtcTick expireTick;
				expireTick.tick = m_fttInfo.expireTick;
				ret = (sceRtcCompareTick(&networkTick, &expireTick) <= 0) ?
					0 : TOPMENU_ERROR_FTT_EXPIRED;
			}
		}
		if (ret == 0) {
			ret = (m_process->m_pendingResume || m_fttInfo.isTrialValid) ?
				TOPMENU_ERROR_FTT_NEEDS_UPDATE : TOPMENU_ERROR_FTT_CONFIRM;
		}
	}

	m_process->m_pendingResume = false;
	return ret;
}

/* (0x81288B80) */
int32_t AppCheckJob::Run()
{
	if (!m_hasAppInfo) {
		int ret = SceLsdb_87EE0526(g_shellLsdbClient->client,
					   &m_process->m_titleId, &m_appInfo);
		if (ret != 0)
			return ret;
	}

	if ((SceLsdb_0B16523B(&m_appInfo) & 4) != 0) {
		int fttRet = CheckFreeTrialLicense();
		if (fttRet < 0)
			return fttRet;
	}

	GamePluginInterface *gameIf = g_topWindow->m_gameIf;
	if (gameIf == NULL)
		return TOPMENU_ERROR_NO_GAME_PLUGIN;

	char versionBuf[8];
	sce_paf_memset(versionBuf, 0, sizeof(versionBuf));

	GamePluginAppCheckStartFunc checkStart =
		reinterpret_cast<GamePluginAppCheckStartFunc>(gameIf->func[39]);
	int checkRet = checkStart(&m_appInfo, versionBuf);
	if (checkRet == GAME_ERROR_PSM_UPDATE_REQUIRED ||
	    checkRet == GAME_ERROR_PSM_INSTALL_REQUIRED) {
		m_psmVersion = versionBuf;
	}
	return (checkRet < 0) ? checkRet : 0;
}

/* (0x812891B6) */
bool AppCheckJob::HandleFreeTrialResult(SceInt32 errorCode, SceInt32 pageKey)
{
	switch (errorCode) {
	case TOPMENU_ERROR_FTT_CONFIRM: {
		paf::wstring expireText;
		paf::rtc::Tick utcTick = m_fttInfo.expireTick;
		paf::rtc::Tick localTick = 0;
		paf::rtc::DateTime dtRaw;
		if (paf::rtc::ConvertUtcToLocalTime(&utcTick, &localTick) >= 0 &&
		    paf::rtc::ConvertTickToDateTime(&localTick, &dtRaw, NULL) >= 0) {
			wchar_t formatted[64];
			sce_paf_memset(formatted, 0, sizeof(formatted));
			paf::datetime::DateTime dt(dtRaw.year, dtRaw.month, dtRaw.day,
						   dtRaw.hour, dtRaw.minute, dtRaw.second,
						   dtRaw.microsecond);
			dt.Format(formatted, 64, L"%c", false);
			expireText = formatted;
		}

		char minutesAscii[64];
		sce_paf_snprintf(minutesAscii, sizeof(minutesAscii), "%u",
				 m_fttInfo.remainingSeconds / 60u);
		paf::string minutesUtf8(minutesAscii);
		paf::wstring minutesWide;
		paf::common::Utf8ToUtf16(minutesUtf8.c_str(), &minutesWide);

		showFttConfirmDialog(pageKey, m_process, expireText, minutesWide);
		return true;
	}
	case TOPMENU_ERROR_FTT_OFFLINE:
		showFttSignInDialog(pageKey, m_process);
		return true;
	case TOPMENU_ERROR_FTT_EXPIRED:
		return false;
	case TOPMENU_ERROR_FTT_NEEDS_UPDATE: {
		paf::common::SharedPtr<paf::job::JobItem> job;
		topmenuStartUpdateFttLicenseJob(&job, m_process, &m_appInfo);
		m_process->m_checkJob = job;
		return true;
	}
	default:
		return false;
	}
}

/* (0x81289746) */
void AppCheckJob::Finish(int32_t result)
{
	topmenuEndBusy();
	if (IsCanceled())
		return;

	if (!m_process->OnCheckJobComplete()) {
		m_process = NULL;
		return;
	}

	if (result >= 0) {
		m_process->OnAppCheckSucceeded(m_appInfo);
		return;
	}

	const char *titleName = sceLsdbGetName(&m_appInfo);
	SceInt32 pageKey = -1;
	if (m_process->m_tabAppData != NULL &&
	    sce_paf_strncmp(titleName, m_process->m_tabAppData->m_titleId.c_str(), 10) == 0) {
		pageKey = m_process->m_tabAppData->m_ownerTab->m_pageKey;
	}

	if (result == GAME_ERROR_SYSUPCHK_REQUIRED) {
		GamePluginInterface *gameIf = g_topWindow->m_gameIf;
		if (gameIf == NULL)
			return;
		GamePluginAppCheckUpdateFunc checkUpdate =
			reinterpret_cast<GamePluginAppCheckUpdateFunc>(gameIf->func[40]);
		int updateRet = checkUpdate(&m_appInfo, topmenuSysupchkDoneCallback,
					    reinterpret_cast<void *>(static_cast<intptr_t>(pageKey)),
					    false, 0, -1);
		if (updateRet >= 0) {
			void *keyArg = reinterpret_cast<void *>(static_cast<intptr_t>(pageKey));
			topmenuPushPowerKeyHandler(topmenuSysupchkCancelPowerKey, keyArg);
			topmenuAddTabLeaveListener(pageKey, topmenuSysupchkCancelTabLeave, keyArg);
			return;
		}
		result = updateRet;
	} else if (result == GAME_ERROR_PSM_INSTALL_REQUIRED ||
		   result == GAME_ERROR_PSM_UPDATE_REQUIRED) {
		showPsmUpdateDialog(titleName, pageKey, result);
		return;
	} else if (HandleFreeTrialResult(result, pageKey)) {
		return;
	}

	AppCheckCancelContext *ctx = new AppCheckCancelContext();
	ctx->pageKey = pageKey;
	ctx->dialogId = -1;

	AppCheckErrorDialogHandler *handler = new AppCheckErrorDialogHandler(ctx);
	SceInt32 dialogId = topmenuShowAppError(result, &m_appInfo, handler,
						NULL, NULL, NULL, NULL, NULL);
	ctx->dialogId = dialogId;
	if (dialogId != -1) {
		paf::thread::RMutex::main_thread_mutex.Lock();
		topmenuPushPowerKeyHandler(onErrorDialogPowerKey, ctx);
		topmenuAddTabLeaveListener(ctx->pageKey, onErrorDialogTabLeave, ctx);
		paf::thread::CallQueue::main_thread_callqueue.Enqueue(
			"LiveAreaOpenCancel", cancelLiveAreaOpenTask,
			reinterpret_cast<void *>(static_cast<intptr_t>(ctx->pageKey)));
		paf::thread::RMutex::main_thread_mutex.Unlock();
	}
}

} /* namespace */

/* (0x812888A2) */
int topmenuStartAppCheckJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			    TopmenuAppProcess *proc)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new AppCheckJob(proc));
	*outJob = job;
	return g_topWindow->m_jobQueue->Enqueue(job);
}

/* (0x81289B0A) */
void topmenuCloseDialogAndDeleteTab(SceInt32 dialogId, SceInt32 unusedFlag, SceInt32 pageKey)
{
	(void)unusedFlag;
	if (dialogId != -1)
		SceCommonGuiDialog_BC311DC0(dialogId);
	topmenuCancelLiveAreaOpen(pageKey);
	topmenuDeleteTabByPageKey(pageKey);
}
