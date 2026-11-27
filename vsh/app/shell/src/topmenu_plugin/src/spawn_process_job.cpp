/*
 * spawn_process_job.cpp: SpawnProcessJob, SpawnBgProcessJob, launch parameter
 * assembly, trophy/confirm/error dialogs, and version/promote helpers for
 * topmenu_plugin (0x8129BA98-0x8129F139).
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_INVALID_SELF_PATH		= -2146420478; /* 0x80103902 */
const SceInt32 TOPMENU_ERROR_PSGM_BG_FORBIDDEN		= -2146420472; /* 0x80103908 */
const SceInt32 TOPMENU_ERROR_OVERWRITE_PSP_PARAM	= -2146420465; /* 0x8010390F */
const SceInt32 TOPMENU_ERROR_MEM_BUDGET_EXCEEDED	= -2146420463; /* 0x80103911 */
const SceInt32 TOPMENU_ERROR_OVERWRITE_PSM_PARAM	= -2146420461; /* 0x80103913 */
const SceInt32 TOPMENU_ERROR_BLOCKED_TITLE		= -2146420459; /* 0x80103915 */
const SceInt32 TOPMENU_ERROR_LAUNCH_CANCELED_SILENT	= -2146420439; /* 0x80103929 */
const SceInt32 TOPMENU_ERROR_LAUNCH_DEFERRED		= -2146420438; /* 0x8010392A */
const SceInt32 TOPMENU_ERROR_CONFIRM_LAUNCH		= -2146420430; /* 0x80103932 */
const SceInt32 TOPMENU_ERROR_TROPHY_OWNERSHIP_A		= -2146420429; /* 0x80103933 */
const SceInt32 TOPMENU_ERROR_TROPHY_OWNERSHIP_B		= -2146420428; /* 0x80103934 */
const SceInt32 TOPMENU_ERROR_TROPHY_SIMPLE_CONFIRM	= -2146420427; /* 0x80103935 */
const SceInt32 TOPMENU_ERROR_SAVEDATA_OWNERSHIP		= -2146420426; /* 0x80103936 */
const SceInt32 TOPMENU_ERROR_ADDCONT_LICENSE_UPDATE	= -2146420423; /* 0x80103939 */
const SceInt32 TOPMENU_ERROR_REMOVE_SAVEDATA		= -2146420422; /* 0x8010393A */
const SceInt32 TOPMENU_ERROR_PSM_CONFIRM_DIALOG		= -2146420419; /* 0x8010393D */
const SceInt32 TOPMENU_ERROR_INVALID_VERSION_STR	= -2146420413; /* 0x80103943 */
const SceInt32 TOPMENU_ERROR_PSM_DEV_REMAPPED		= -2146420412; /* 0x80103944 */
const SceInt32 TOPMENU_ERROR_NPDRM_LICENSE_EXPIRED	= -2139095023; /* 0x80800011 */
const SceInt32 TOPMENU_ERROR_DETAIL_DIALOG_CODE		= -2146430699; /* 0x80101115 */
const SceInt32 TOPMENU_ERROR_PSM_DEV_UNSUPPORTED	= -2146497744; /* 0x800F0B30 */

const SceUInt32 LSDB_KEY_POCKETSTATION_FLAG		= 0xCE85B5B0u;
const SceUInt32 LSDB_KEY_NOTIFY_STATE			= 0x5142196Bu;
const SceUInt32 LSDB_KEY_INSTALL_DIR_SAVEDATA		= 0x10954174u;

const SceUInt32 MSG_TROPHY_HEADER			= 0x5767627Eu; /* 1466357118 */
const SceUInt32 MSG_TROPHY_BODY_OTHER_USER		= 0x5123F49Eu; /* 1361310878 */
const SceUInt32 MSG_TROPHY_BODY_UNEARNED		= 0x211A368Eu; /* 555365006 */
const SceUInt32 MSG_CONFIRM_LAUNCH_PROMPT		= 0x93B8480Du; /* -1816641523 */
const SceUInt32 MSG_SAVEDATA_CONFIRM_FALLBACK		= 0x65D76791u; /* 1708617617 */
const SceUInt32 WIDGET_TROPHY_RICH_TEXT			= 0xD0791765u; /* -797370523 */

typedef void (*GamePluginAlarmUpdateFunc)(const LsdbAppInfoPtr *appInfo);
typedef void (*GamePluginAlarmRemoveQuietFunc)(const LsdbAppInfoPtr *appInfo);
typedef int (*GamePluginGetSavedataStatFunc)(const paf::string *saveDir,
					     paf::LocalFileStat *outStat);

typedef void (*ApplauncherPsmConfirmCb)(const paf::string *titleId, SceInt32 launchAppId,
					SceInt32 checkFlags, bool confirmed);
typedef int (*ApplauncherShowPsmDialogFunc)(const paf::string *titleId, SceInt32 retryCount,
					    ApplauncherPsmConfirmCb callback,
					    SceInt32 launchAppId, SceInt32 checkFlags);
typedef int (*ApplauncherRetryPromoteCb)(const paf::string *titleId);
typedef int (*ApplauncherShowPromoteRetryFunc)(const paf::string *titleId, SceInt32 retryCount,
					       ApplauncherRetryPromoteCb callback,
					       SceInt32 launchAppId, SceInt32 checkFlags);

/* Promote-wait lock shared with launchAppWithLsdbInfo (0x81545A44 / 0x815ACA00) */
static bool g_promoteWaitLocked = false;
static char g_promoteWaitTitleId[33];

static void onSpawnDialogDeleted(paf::ui::Widget *widget, void *userData);
static void onSpawnDetailDialogDeleted(paf::ui::Widget *widget, void *userData);
static void onPsmConfirmCallback(const paf::string *titleId, SceInt32 launchAppId,
				 SceInt32 checkFlags, bool confirmed);

/* (0x8129EF66) Release indicator lock and event listener when dialog closes */
static void onSpawnDialogDeleted(paf::ui::Widget *widget, void *userData)
{
	(void)widget;
	if (g_topWindow->m_indicatorIf != NULL && g_topWindow->m_indicatorIf->UnlockButton != NULL)
		g_topWindow->m_indicatorIf->UnlockButton();
	SceShell_D4C7D2B7(9, topmenuSpawnOnInstallNoticeEvent, userData);
}

/* (0x8129EF8E) Transfer hold to detail sub-dialog if opened, else release hold */
static void onSpawnDetailDialogDeleted(paf::ui::Widget *widget, void *userData)
{
	(void)widget;
	SceInt32 dialogId = static_cast<SceInt32>(reinterpret_cast<intptr_t>(userData));
	paf::ui::Widget *detailWidget = SceCommonGuiDialog_8947FC23(dialogId, 9);
	if (detailWidget != NULL) {
		detailWidget->AddDeleteCB(onSpawnDialogDeleted, userData);
	} else {
		if (g_topWindow->m_indicatorIf != NULL &&
		    g_topWindow->m_indicatorIf->UnlockButton != NULL) {
			g_topWindow->m_indicatorIf->UnlockButton();
		}
		SceShell_D4C7D2B7(9, topmenuSpawnOnInstallNoticeEvent, userData);
	}
}

/* (0x8129CAD6) Lock indicator button and register close listener on dialog widget */
static void registerSpawnDialogHold(SceInt32 dialogId)
{
	paf::ui::Widget *dialogWidget = SceCommonGuiDialog_8947FC23(dialogId, 4);
	if (dialogWidget == NULL)
		return;

	if (g_topWindow->m_indicatorIf != NULL && g_topWindow->m_indicatorIf->LockButton != NULL)
		g_topWindow->m_indicatorIf->LockButton();
	SceShell_7FE4E732(9, topmenuSpawnOnInstallNoticeEvent,
			  reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
	dialogWidget->AddDeleteCB(onSpawnDialogDeleted,
				  reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
}

/* (0x8129EA4C / 0x8129EA88 / 0x8129EAC8) Error dialog handler returning to home on OK */
class SpawnErrorDialogHandler : public CommonGuiDialogHandler {
public:
	explicit SpawnErrorDialogHandler(const char *titleName)
		: m_titleId(titleName != NULL ? titleName : "")
	{
	}

	virtual ~SpawnErrorDialogHandler() {}

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

/* (0x8129BCA4 / 0x8129BCC0 / 0x8129BCE0) Confirm handler recording savedata cancel telemetry */
class SpawnConfirmTelemetryHandler : public TopmenuSpawnConfirmHandler {
public:
	SpawnConfirmTelemetryHandler(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
				     SceInt32 existingFlags, SceInt32 extraFlags)
		: TopmenuSpawnConfirmHandler(appInfo, launchAppId, existingFlags, extraFlags)
	{
		consoleInfoFileAddInt(277, 1, 0);
	}

	virtual ~SpawnConfirmTelemetryHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 5)
			consoleInfoFileAddInt(279, 1, 0);
		TopmenuSpawnConfirmHandler::operator()(dialogId, button);
	}
};

/* (0x8129C970) Show app spawn error dialog with indicator hold */
static SceInt32 showSpawnErrorDialog(SceInt32 errorCode, const LsdbAppInfoPtr *appInfo)
{
	const char *titleName = sceLsdbGetName(appInfo);
	SpawnErrorDialogHandler *handler = new SpawnErrorDialogHandler(titleName);
	SceInt32 dialogId = topmenuShowAppError(errorCode, appInfo, handler,
						NULL, NULL, NULL, NULL, NULL);
	if (dialogId >= 0) {
		paf::ui::Widget *dialogWidget = SceCommonGuiDialog_8947FC23(dialogId, 4);
		if (dialogWidget != NULL) {
			if (g_topWindow->m_indicatorIf != NULL &&
			    g_topWindow->m_indicatorIf->LockButton != NULL) {
				g_topWindow->m_indicatorIf->LockButton();
			}
			SceShell_7FE4E732(9, topmenuSpawnOnInstallNoticeEvent,
					  reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
			if (errorCode == TOPMENU_ERROR_DETAIL_DIALOG_CODE) {
				dialogWidget->AddDeleteCB(
					onSpawnDetailDialogDeleted,
					reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
			} else {
				dialogWidget->AddDeleteCB(
					onSpawnDialogDeleted,
					reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
			}
		}
	}
	return dialogId;
}

/* (0x8129CB68) Show Yes/No confirmation dialog with indicator hold */
static SceInt32 showSpawnConfirmDialog(const paf::wstring &message, CommonGuiDialogHandler *handler)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.layout = SceCommonGuiDialog_22B17F2B;
	param.text = message;
	param.handler = handler;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0)
		registerSpawnDialogHold(dialogId);
	return dialogId;
}

/* (0x8129CCBC) Replace all occurrences of needle with replacement in src */
static paf::string replaceAllSubstrings(const paf::string &src, const paf::string &needle,
					const paf::string &replacement)
{
	paf::string out = src;
	size_t pos = out.find(needle, 0);
	while (pos != paf::string::npos) {
		out.replace(pos, needle.length(), replacement);
		pos = out.find(needle, pos + replacement.length());
	}
	return out;
}

/* (0x8129F024) Fade in trophy RichText once layout finishes */
static void onTrophyRichTextReady(int32_t eventType, paf::ui::Handler *self,
				  paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	(void)userData;
	paf::ui::Widget *widget = static_cast<paf::ui::Widget *>(self);
	if (widget == NULL)
		return;

	paf::Timer *timer = new paf::Timer();
	if (timer != NULL)
		timer->InitTimer(200.0f, paf::Timer::FUNC_LINEAR, paf::Timer::REPEAT_NONE, -1, NULL);

	if (!widget->m_transition_complete) {
		widget->m_transition = 0xFF;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

/* Append localized resource message converted to HTML (<br> for newlines) */
static void appendLocalizedHtml(paf::string *outHtml, SceUInt32 msgId)
{
	const wchar_t *raw = g_topmenuPlugin->GetString(paf::IDParam(msgId));
	paf::wstring wide(raw != NULL ? raw : L"");
	paf::string utf8;
	paf::common::Utf16ToUtf8(wide, &utf8);
	paf::string newline("\n");
	paf::string brTag("<br>");
	paf::string converted = replaceAllSubstrings(utf8, newline, brTag);
	*outHtml += converted.c_str();
}

/* (0x8129CDCC) Show trophy eligibility confirmation dialog (plain or RichText HTML) */
static SceInt32 showTrophyConfirmDialog(SceInt32 errorCode, CommonGuiDialogHandler *handler)
{
	if (errorCode != TOPMENU_ERROR_TROPHY_OWNERSHIP_A &&
	    errorCode != TOPMENU_ERROR_TROPHY_OWNERSHIP_B &&
	    errorCode != TOPMENU_ERROR_TROPHY_SIMPLE_CONFIRM) {
		return -1;
	}

	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;

	if (errorCode == TOPMENU_ERROR_TROPHY_SIMPLE_CONFIRM) {
		param.layout = SceCommonGuiDialog_22B17F2B;
		const wchar_t *msg = g_topmenuPlugin->GetString(paf::IDParam(MSG_TROPHY_HEADER));
		param.text = (msg != NULL) ? msg : L"";
		param.handler = handler;
		SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
		if (dialogId >= 0)
			registerSpawnDialogHold(dialogId);
		return dialogId;
	}

	static const paf::math::v4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
	static const CommonGuiDialogPage trophyRichTextPage = {
		{ 0x3A918C2Cu, 0x438C7285u, 0x5F5431A7u }
	};

	paf::string html;
	if (errorCode == TOPMENU_ERROR_TROPHY_OWNERSHIP_B) {
		html = "<h2>";
		appendLocalizedHtml(&html, MSG_TROPHY_HEADER);
		html += "</h2>";
		html += "<h4>";
		appendLocalizedHtml(&html, MSG_TROPHY_BODY_UNEARNED);
		html += "</h4>";
	} else if (errorCode == TOPMENU_ERROR_TROPHY_OWNERSHIP_A) {
		html = "<h2>";
		appendLocalizedHtml(&html, MSG_TROPHY_HEADER);
		html += "</h2>";
		html += "<h4>";
		appendLocalizedHtml(&html, MSG_TROPHY_BODY_OTHER_USER);
		html += "</h4>";
	}

	param.layout.page = &trophyRichTextPage;
	param.layout.parts = SceCommonGuiDialog_22B17F2B.parts;
	param.layout.buttons = SceCommonGuiDialog_22B17F2B.buttons;
	param.handler = handler;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId < 0)
		return dialogId;

	paf::ui::Widget *rootWidget = SceCommonGuiDialog_8947FC23(dialogId, 4);
	if (rootWidget != NULL) {
		paf::ui::RichText *richText = static_cast<paf::ui::RichText *>(
			rootWidget->FindChild(paf::IDParam(WIDGET_TROPHY_RICH_TEXT), 0));
		if (richText != NULL) {
			paf::math::v4 margin = richText->m_vertical_bar_margin;
			margin.set_z(0.0f);
			richText->SetVerticalBarMargin(margin);
			richText->m_line_spacing = 10.0f;
			richText->SetColor(clearColor, 0, 0x10001, 0, 0, 0);
			richText->m_transition_complete = false;
			richText->m_transition = 0xFF;
			richText->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(richText, NULL, 2, 0.0f, NULL);
			richText->SetCText(html.c_str(), html.length());
			richText->AddEventListener(
				0x10000008,
				new paf::ui::EventCBListener(onTrophyRichTextReady, NULL), false);
		}
	}

	registerSpawnDialogHold(dialogId);
	return dialogId;
}

/* (0x8129BFC6) Validate app metadata, build launch parameters, and invoke SceAppMgr */
static SceInt32 launchAppWithLsdbInfo(SceInt32 *outAppId, const LsdbAppInfoPtr *appInfo,
				      SceInt32 pendingAppId, bool isBgLaunch, SceInt32 checkFlags,
				      const TopmenuAppAttrFlags *attrFlags)
{
	(void)SceThreadmgr_47F6DE49();

	const char *selfPath = sceLsdbGetSelfPath(appInfo);
	const char *titleName = sceLsdbGetName(appInfo);
	if (selfPath == NULL) {
		if (pendingAppId == -1)
			return TOPMENU_ERROR_INVALID_SELF_PATH;
		if (SceLsdb_9D27A87D(appInfo) != 3)
			return TOPMENU_ERROR_INVALID_SELF_PATH;
	}

	if (titleName != NULL && topmenuIsBlockedLaunchTitle(titleName))
		return TOPMENU_ERROR_BLOCKED_TITLE;

	bool alreadyRunning = false;
	SceAppMgrAppStatus existingStatus;
	if (sceAppMgrGetStatusByName(titleName, &existingStatus) == 0)
		alreadyRunning = (existingStatus.processId != existingStatus.appId);

	char launchParamBuf[1024];
	char peekParamBuf[1024];
	char psmRuntimePath[52];
	const char *launchParam = NULL;
	bool isPspType = false;
	bool isPsmApp = false;
	bool isPsmRuntime = false;

	int appType = sceLsdbGetType(appInfo);
	if (appType == 1) {
		if (!alreadyRunning) {
			SceInt32 checkRet = topmenuCheckVitaAppExec(
				appInfo, static_cast<SceUInt8>(checkFlags));
			if (checkRet != 0)
				return checkRet;
		}
	} else if (appType == 2 || appType == 3) {
		isPspType = true;
		if (!alreadyRunning) {
			SceInt32 checkRet = topmenuCheckPspAppExec(
				appInfo, static_cast<SceUInt32>(checkFlags));
			if (checkRet != 0)
				return checkRet;
		}

		const char *originalPath = sceLsdbGetOriginalPath(appInfo);
		const char *discId = sceLsdbGetDiscId(appInfo);
		SceInt64 parentalLevel = sceLsdbGetParentalLevel(appInfo);
		const char *gameDataId = sceLsdbGetGameDataId(appInfo);
		const char *appVer = sceLsdbGetAppVer(appInfo);
		SceInt64 bootable = sceLsdbGetBootable(appInfo);
		const char *category = sceLsdbGetCategory(appInfo);

		char parentalStr[8];
		char bootableStr[8];
		char extraParam[20];
		sce_paf_memset(parentalStr, 0, sizeof(parentalStr));
		sce_paf_memset(bootableStr, 0, sizeof(bootableStr));
		sce_paf_memset(extraParam, 0, sizeof(extraParam));

		if (originalPath == NULL)
			originalPath = "";
		if (discId == NULL)
			discId = "";
		if (gameDataId == NULL)
			gameDataId = "";
		if (appVer == NULL)
			appVer = "";
		if (parentalLevel >= 0)
			sce_paf_snprintf(parentalStr, 6, "%d", static_cast<int>(parentalLevel));
		if (bootable >= 0)
			sce_paf_snprintf(bootableStr, 6, "%d", static_cast<int>(bootable));
		if (appType == 3 &&
		    sceAppMgrPeekLaunchParamForShell(pendingAppId, peekParamBuf,
						     sizeof(peekParamBuf)) == 0) {
			sce_paf_snprintf(extraParam, sizeof(extraParam), "&%s", peekParamBuf);
		}

		sce_paf_snprintf(
			launchParamBuf, sizeof(launchParamBuf),
			"originalpath=%s&selfpath=%s&discid=%s&parentallevel=%s&gamedataid=%s&appver=%s&bootable=%s&category=%s%s",
			originalPath, selfPath, discId, parentalStr, gameDataId, appVer,
			bootableStr, category, extraParam);
		launchParam = launchParamBuf;

		selfPath = "vs0:app/NPXS10028/eboot.bin";
		LsdbAppInfo::iterator psIt = (*appInfo)->find(LSDB_KEY_POCKETSTATION_FLAG);
		if (psIt != (*appInfo)->end() &&
		    (psIt->second.type != LSDB_VALUE_INTEGER || psIt->second.integer == 0)) {
			selfPath = "vs0:app/NPXS10069/eboot.bin";
		}
	} else if (appType == 4) {
		isPsmApp = true;
		if (!alreadyRunning) {
			SceInt32 checkRet = topmenuCheckPsmAppExec(appInfo);
			if (checkRet != 0)
				return checkRet;
		}
		const char *originalPath = sceLsdbGetOriginalPath(appInfo);
		sce_paf_snprintf(launchParamBuf, sizeof(launchParamBuf), "%s", originalPath);
		launchParam = launchParamBuf;
		selfPath = "ux0:app/PCSI00011/eboot.bin";
	} else if (appType == 5) {
		isPsmRuntime = true;
		if (!alreadyRunning) {
			SceInt32 checkRet = topmenuCheckPsmRuntimeExec(appInfo);
			if (checkRet != 0)
				return checkRet;
		}
		const char *originalPath = sceLsdbGetOriginalPath(appInfo);
		if (originalPath != NULL) {
			sce_paf_snprintf(launchParamBuf, sizeof(launchParamBuf), "%s", originalPath);
			launchParam = launchParamBuf;
		}
		sce_paf_memset(psmRuntimePath, 0, 48);
		sce_paf_snprintf(psmRuntimePath, 48, "%spsm/%s/runtime/eboot.bin",
				 "ux0:", titleName);
		selfPath = psmRuntimePath;
	}

	SceUInt32 budgetSize = static_cast<SceUInt32>(SceLsdb_849E8DC6(appInfo)) << 10;
	SceInt32 pycontSize = SceLsdb_132FA3C6(appInfo) << 10;
	SceUInt32 lsdbProcAttr = SceLsdb_45B38A03(appInfo);
	SceInt64 appAttr = sceLsdbGetAttribute(appInfo);
	SceInt64 memSize = SceLsdb_1518F869(appInfo);

	SceUInt32 processAttr = (lsdbProcAttr & 1u) ? 1u : 0u;
	bool hasHighMemFlag = ((appAttr & 8LL) != 0);
	if (hasHighMemFlag) {
		if (budgetSize != 0 || memSize == 0 || memSize >= 52428800LL)
			processAttr |= 2u;
	}

	AppmgrLaunchAppOption launchOpt;
	SceAppMgrAppStatus outStatus;
	sce_paf_memset(&launchOpt, 0, sizeof(launchOpt));
	sce_paf_memset(&outStatus, 0, sizeof(outStatus));
	launchOpt.size = sizeof(launchOpt);
	launchOpt.status = &outStatus;
	launchOpt.launchArgumentMarker = isBgLaunch ? 1u : 0u;

	if (SceLsdb_9D27A87D(appInfo) == 2)
		launchOpt.launchOrigin = 1u;

	SceInt64 enlargeMode = SceLsdb_43153457(appInfo);
	if (enlargeMode == 1)
		launchOpt.enlargeGameMemorySize = 29u;
	else if (enlargeMode == 2)
		launchOpt.enlargeGameMemorySize = 77u;
	else if (enlargeMode == 3)
		launchOpt.enlargeGameMemorySize = 109u;

	if ((lsdbProcAttr & 2u) != 0 && (lsdbProcAttr & 4u) != 0 && budgetSize != 0)
		launchOpt.useCommonDialog = 1u;

	if (hasHighMemFlag && budgetSize != 0) {
		SceUInt32 totalMem = budgetSize;
		if (launchOpt.useCommonDialog != 0)
			totalMem += 9437184u;
		if (totalMem > 0x2000000u && vshSblSsIsDevelopmentMode() == 0)
			return TOPMENU_ERROR_MEM_BUDGET_EXCEEDED;
	}

	if (attrFlags != NULL) {
		launchOpt.defaultInfobar.visibility = attrFlags->hasFlagBitSeven ? 1 : 0;
		launchOpt.defaultInfobar.color = static_cast<unsigned char>(attrFlags->flagBitEight);
		launchOpt.defaultInfobar.transparency =
			static_cast<unsigned char>(attrFlags->flagBitNine);
	}
	if (pycontSize > 0)
		launchOpt.pycontSize = static_cast<SceUInt32>(pycontSize);

	if (g_promoteWaitLocked) {
		for (int attempt = 0; attempt < 10; ++attempt) {
			paf::thread::Sleep(500);
			if (!g_promoteWaitLocked)
				break;
			if (attempt == 9)
				g_promoteWaitLocked = false;
		}
	}

	SceInt32 launchRet;
	if (pendingAppId == -1) {
		if ((appAttr & 0x800LL) != 0)
			sce_paf_strncpy(launchOpt.proxyAppName, "NPXS19999",
					sizeof(launchOpt.proxyAppName));
		launchRet = sceAppMgrLaunchAppByPath4(selfPath, titleName, budgetSize,
						      launchParam, processAttr, &launchOpt);
	} else {
		SceAppMgrAppStatus pendingStatus;
		SceInt32 statusRet = sceAppMgrGetStatusByAppId(pendingAppId, &pendingStatus);
		if (statusRet != 0)
			return statusRet;

		if ((pendingStatus.launchMode & 0x100000u) != 0) {
			launchOpt.flags = pendingStatus.launchMode;
			sce_paf_strncpy(launchOpt.appName, titleName, sizeof(launchOpt.appName));
			char peekCheckBuf[1024];
			if (sceAppMgrPeekLaunchParamForShell(pendingAppId, peekCheckBuf,
							     sizeof(peekCheckBuf)) == 0 &&
			    topmenuParsePsgmUriScheme(peekCheckBuf) != 0 &&
			    sceLsdbGetType(appInfo) == 1 &&
			    SceLsdb_9D27A87D(appInfo) == 0) {
				return TOPMENU_ERROR_PSGM_BG_FORBIDDEN;
			}
		}

		if ((appAttr & 0x800LL) != 0) {
			int metaPath = sceLsdbGetMetaContentsPath(appInfo);
			if (pendingStatus.launchSourceAppId == -1 || metaPath == 1 ||
			    metaPath == 2) {
				sce_paf_strncpy(launchOpt.proxyAppName, "NPXS19999",
						sizeof(launchOpt.proxyAppName));
			} else {
				SceAppMgrAppStatus creatorStatus;
				if (sceAppMgrGetStatusByAppId(pendingStatus.launchSourceAppId,
							      &creatorStatus) == 0) {
					sce_paf_strncpy(launchOpt.proxyAppName,
							creatorStatus.appName,
							sizeof(launchOpt.proxyAppName));
				}
			}
		}

		if (launchParam != NULL) {
			if (isPspType) {
				if (sceAppMgrOverwriteLaunchParamForShell(pendingAppId,
									  launchParam) != 0) {
					return TOPMENU_ERROR_OVERWRITE_PSP_PARAM;
				}
			} else if (isPsmApp || isPsmRuntime) {
				if (sceAppMgrOverwriteLaunchParamForShell(pendingAppId,
									  launchParam) != 0) {
					return TOPMENU_ERROR_OVERWRITE_PSM_PARAM;
				}
			}
		}

		launchRet = sceAppMgrLaunchAppByName2ndStage(pendingAppId, selfPath, budgetSize,
							     processAttr, &launchOpt);
	}

	(void)SceThreadmgr_47F6DE49();
	*outAppId = outStatus.appId;
	return launchRet;
}

/* SpawnProcessJob (80 bytes, 0x8129BD02..0x8129E6D2) */
class SpawnProcessJob : public paf::job::JobItem {
public:
	SpawnProcessJob(TopmenuProcessBase *proc, SceInt32 launchAppId, SceInt32 checkFlags)
		: paf::job::JobItem("SpawnProcessJob", NULL),
		  m_process(proc),
		  m_appInfo(*topmenuProcessGetAppInfo(proc)),
		  m_launchAppId(launchAppId),
		  m_checkFlags(checkFlags),
		  m_attrFlags(),
		  m_isBgLaunch(proc->m_enabled),
		  m_updatedNotify(false),
		  m_notifyValue(0),
		  m_spawnedAppId(-1),
		  m_bootTimeStartUs(0)
	{
		m_attrFlags.hasFlagBitSeven = false;
		m_attrFlags.flagBitEight = 0;
		m_attrFlags.flagBitNine = 0;
		if (proc != NULL && proc->m_owner != NULL) {
			TopmenuAppProcess *owner = proc->m_owner;
			m_attrFlags.hasFlagBitSeven = owner->m_bgLayerOpt.useCustomColor;
			m_attrFlags.flagBitEight = owner->m_bgLayerOpt.style;
			m_attrFlags.flagBitNine = static_cast<SceInt32>(owner->m_bgLayerOpt.color);
		}
	}

	virtual ~SpawnProcessJob()
	{
		if (m_launchAppId != -1)
			topmenuCancelPendingAppLaunch(m_launchAppId);
	}

	/* (0x8129BE88 helper) Refresh appInfo and attrFlags before re-enqueue */
	void RefreshFromProcess()
	{
		LsdbAppInfoPtr *latestInfo = topmenuProcessGetAppInfo(m_process);
		if (latestInfo->get() != m_appInfo.get())
			m_appInfo = *latestInfo;
		if (m_process != NULL && m_process->m_owner != NULL) {
			TopmenuAppProcess *owner = m_process->m_owner;
			m_attrFlags.hasFlagBitSeven = owner->m_bgLayerOpt.useCustomColor;
			m_attrFlags.flagBitEight = owner->m_bgLayerOpt.style;
			m_attrFlags.flagBitNine = static_cast<SceInt32>(owner->m_bgLayerOpt.color);
		}
	}

	/* (0x8129E6D2) Detach process pointer and cancel job */
	void CancelProcess()
	{
		m_process = NULL;
		Cancel();
	}

	/* (0x8129C744) */
	virtual int32_t Run()
	{
		if (IsCanceled())
			return -1;

		const char *titleName = sceLsdbGetName(&m_appInfo);
		if (sce_paf_strcmp(titleName, "NPXS10085") == 0)
			sceAppMgrDestroyAppByName("NPXS10084");

		if (SceLsdb_849E8DC6(&m_appInfo) != 0)
			m_bootTimeStartUs = SceThreadmgr_47F6DE49();

		SceInt32 ret = launchAppWithLsdbInfo(&m_spawnedAppId, &m_appInfo, m_launchAppId,
						     m_isBgLaunch, m_checkFlags, &m_attrFlags);
		if (ret >= 0) {
			m_launchAppId = -1;
			const char *launchedName = sceLsdbGetName(&m_appInfo);
			if (launchedName != NULL && SceLsdb_F4325AD0(&m_appInfo) &&
			    sce_paf_strcmp("NPXS10006", launchedName) != 0) {
				LsdbAppInfo::iterator it = m_appInfo->find(LSDB_KEY_NOTIFY_STATE);
				bool alreadyNotified = (it != m_appInfo->end() &&
							it->second.type == LSDB_VALUE_INTEGER &&
							(it->second.integer & 0x200) != 0);
				if (!alreadyNotified) {
					SceInt64 newNotify = 0x200;
					if (it != m_appInfo->end() &&
					    it->second.type == LSDB_VALUE_INTEGER) {
						newNotify = it->second.integer | 0x200LL;
					}
					paf::string titleStr(launchedName);
					LsdbValue notifyVal;
					notifyVal.type = LSDB_VALUE_INTEGER;
					notifyVal.integer = newNotify;
					int updateRet = SceLsdb_7D7F6809(
						g_shellLsdbClient->client, &titleStr,
						LSDB_KEY_NOTIFY_STATE, &notifyVal, 0x200LL);
					if (updateRet == 0) {
						m_notifyValue = newNotify;
						m_updatedNotify = true;
						shellSvcActivityPostSystemSoftwareImpl(launchedName);
					}
				}
			}
		}
		return ret;
	}

	/* (0x8129DD5C) */
	virtual void Finish(int32_t result)
	{
		if (m_spawnedAppId >= 0) {
			SceAppMgrAppStatus appStatus;
			if (sceAppMgrGetStatusByAppId(m_spawnedAppId, &appStatus) == 0) {
				SceInt64 attr = sceLsdbGetAttribute(&m_appInfo);
				topmenuRecordLaunchedAppFlags(g_topWindow, m_spawnedAppId,
							      appStatus.appName, attr);
			}
		}

		GamePluginInterface *gameIf = g_topWindow->m_gameIf;

		if (m_updatedNotify) {
			LsdbValue &entry = (*m_appInfo)[LSDB_KEY_NOTIFY_STATE];
			if (entry.type == LSDB_VALUE_TEXT && entry.text != NULL) {
				delete entry.text;
				entry.text = NULL;
			}
			entry.type = LSDB_VALUE_INTEGER;
			entry.integer = m_notifyValue;
		}

		if (result >= 0) {
			if (m_process != NULL)
				m_process->m_enabled = false;
			if (IsCanceled()) {
				sceAppMgrDestroyAppByAppId(m_spawnedAppId);
				if (m_process != NULL)
					topmenuProcessCommand(m_process, 5);
				if (m_launchAppId != -1) {
					topmenuCancelPendingAppLaunch(m_launchAppId);
					m_launchAppId = -1;
				}
				return;
			}
			if (m_process != NULL) {
				m_process->m_appId = m_spawnedAppId;
				topmenuProcessSetState(m_process, 3);
				topmenuProcessFlushPendingAction(m_process);
				if (gameIf != NULL) {
					GamePluginAlarmUpdateFunc updateAlarm =
						reinterpret_cast<GamePluginAlarmUpdateFunc>(
							gameIf->func[26]);
					updateAlarm(&m_appInfo);
				}
				m_process->m_bootTimeStartUs = m_bootTimeStartUs;
			}
			if (m_process != NULL)
				m_process->m_spawnJob.reset();
			if (m_launchAppId != -1) {
				topmenuCancelPendingAppLaunch(m_launchAppId);
				m_launchAppId = -1;
			}
			return;
		}

		bool canceled = IsCanceled();
		bool handledByUi = false;

		if (!canceled) {
			if (result == TOPMENU_ERROR_CONFIRM_LAUNCH) {
				const wchar_t *msg = g_topmenuPlugin->GetString(
					paf::IDParam(MSG_CONFIRM_LAUNCH_PROMPT));
				paf::wstring text(msg != NULL ? msg : L"");
				TopmenuSpawnConfirmHandler *handler =
					new TopmenuSpawnConfirmHandler(&m_appInfo, m_launchAppId,
								       m_checkFlags, 1);
				if (showSpawnConfirmDialog(text, handler) >= 0) {
					m_launchAppId = -1;
					handledByUi = true;
				}
			} else if (result == TOPMENU_ERROR_TROPHY_OWNERSHIP_A ||
				   result == TOPMENU_ERROR_TROPHY_OWNERSHIP_B ||
				   result == TOPMENU_ERROR_TROPHY_SIMPLE_CONFIRM) {
				TopmenuSpawnConfirmHandler *handler =
					new TopmenuSpawnConfirmHandler(&m_appInfo, m_launchAppId,
								       m_checkFlags, 4);
				if (showTrophyConfirmDialog(result, handler) >= 0) {
					m_launchAppId = -1;
					handledByUi = true;
				}
			} else if (result == TOPMENU_ERROR_SAVEDATA_OWNERSHIP) {
				bool shownOnlinePrompt = false;
				LsdbAppInfo::iterator saveIt =
					m_appInfo->find(LSDB_KEY_INSTALL_DIR_SAVEDATA);
				if (saveIt != m_appInfo->end()) {
					const char *rawDir = "";
					if (saveIt->second.type == LSDB_VALUE_TEXT &&
					    saveIt->second.text != NULL) {
						rawDir = saveIt->second.text->c_str();
					}
					paf::string saveDir(rawDir);
					paf::LocalFileStat saveStat;
					if (gameIf != NULL) {
						GamePluginGetSavedataStatFunc getStat =
							reinterpret_cast<GamePluginGetSavedataStatFunc>(
								gameIf->func[63]);
						if (getStat(&saveDir, &saveStat) == 0) {
							if (topmenuShowOnlineStorageDeletePrompt(
								    &m_appInfo, m_launchAppId,
								    m_checkFlags, 0, &saveStat,
								    result) >= 0) {
								m_launchAppId = -1;
								handledByUi = true;
							}
							shownOnlinePrompt = true;
						}
					}
				}
				if (!shownOnlinePrompt) {
					const wchar_t *msg = g_topmenuPlugin->GetString(
						paf::IDParam(MSG_SAVEDATA_CONFIRM_FALLBACK));
					paf::wstring text(msg != NULL ? msg : L"");
					text += L"\n(";
					char codeBuf[16];
					sceErrorGetExternalString(codeBuf, result);
					paf::string codeUtf8(codeBuf);
					paf::wstring codeWide;
					paf::common::Utf8ToUtf16(codeUtf8.c_str(), &codeWide);
					text += codeWide.c_str();
					text += L")";
					SpawnConfirmTelemetryHandler *handler =
						new SpawnConfirmTelemetryHandler(
							&m_appInfo, m_launchAppId, m_checkFlags, 2);
					if (showSpawnConfirmDialog(text, handler) >= 0) {
						m_launchAppId = -1;
						handledByUi = true;
					}
				}
			} else if (result == TOPMENU_ERROR_NPDRM_LICENSE_EXPIRED) {
				paf::common::SharedPtr<paf::job::JobItem> job;
				topmenuStartUpdateLicenseJob(&job, &m_appInfo);
			} else if (result == TOPMENU_ERROR_ADDCONT_LICENSE_UPDATE) {
				paf::common::SharedPtr<paf::job::JobItem> job;
				const char *titleName = sceLsdbGetName(&m_appInfo);
				topmenuStartUpdateAddcontLicenseJob(&job, titleName, m_launchAppId,
								    m_checkFlags, 8);
				m_launchAppId = -1;
				handledByUi = true;
			} else if (result == TOPMENU_ERROR_REMOVE_SAVEDATA) {
				paf::common::SharedPtr<paf::job::JobItem> job;
				topmenuStartRemoveSavedataJob(&job, &m_appInfo, m_launchAppId,
							      m_checkFlags, 2);
				m_launchAppId = -1;
				handledByUi = true;
			} else if (result == TOPMENU_ERROR_PSM_CONFIRM_DIALOG) {
				const char *titleName = sceLsdbGetName(&m_appInfo);
				paf::string titleStr(titleName != NULL ? titleName : "");
				ApplauncherInterface *launcherIf = topmenuGetApplauncherInterface();
				ApplauncherShowPsmDialogFunc showPsmDialog =
					reinterpret_cast<ApplauncherShowPsmDialogFunc>(
						launcherIf->func[22]);
				SceInt32 dialogId = showPsmDialog(&titleStr, 0,
								  onPsmConfirmCallback,
								  m_launchAppId, m_checkFlags);
				if (dialogId >= 0) {
					registerSpawnDialogHold(dialogId);
					m_launchAppId = -1;
					handledByUi = true;
				}
			} else if (result != TOPMENU_ERROR_LAUNCH_DEFERRED &&
				   result != TOPMENU_ERROR_LAUNCH_CANCELED_SILENT) {
				SceInt32 displayError = result;
				if (result == TOPMENU_ERROR_PSM_DEV_UNSUPPORTED) {
					const char *titleName = sceLsdbGetName(&m_appInfo);
					paf::string titleStr(titleName != NULL ? titleName : "");
					if (titleStr == "PCSI00007" || titleStr == "PCSI00009")
						displayError = TOPMENU_ERROR_PSM_DEV_REMAPPED;
				}
				if (showSpawnErrorDialog(displayError, &m_appInfo) >= 0)
					handledByUi = true;
				if (gameIf != NULL) {
					GamePluginAlarmRemoveQuietFunc removeAlarm =
						reinterpret_cast<GamePluginAlarmRemoveQuietFunc>(
							gameIf->func[28]);
					removeAlarm(&m_appInfo);
				}
			}
		}

		if (m_process != NULL) {
			TopTabAppData *appData = topmenuProcessGetTabAppData(m_process);
			if (result != TOPMENU_ERROR_LAUNCH_CANCELED_SILENT && !handledByUi &&
			    (appData == NULL || appData->m_appViewState == 1)) {
				topmenuReturnToHomeIfActive(m_process->m_owner);
			}
			if (m_spawnedAppId != -1)
				sceAppMgrDestroyAppByAppId(m_spawnedAppId);
			topmenuProcessCommand(m_process, 5);
			if (appData != NULL)
				topmenuUpdateIduAfterAppFailure(g_topWindow, appData->m_ownerTab);
		}

		if (canceled || result != TOPMENU_ERROR_LAUNCH_DEFERRED) {
			if (m_process != NULL)
				m_process->m_spawnJob.reset();
		}

		if (m_launchAppId != -1) {
			topmenuCancelPendingAppLaunch(m_launchAppId);
			m_launchAppId = -1;
		}
	}

	TopmenuProcessBase *m_process;		/* +0x1C */
	LsdbAppInfoPtr m_appInfo;		/* +0x20 */
	SceInt32 m_launchAppId;			/* +0x28 */
	SceInt32 m_checkFlags;			/* +0x2C */
	TopmenuAppAttrFlags m_attrFlags;	/* +0x30 */
	bool m_isBgLaunch;			/* +0x3C */
	bool m_updatedNotify;			/* +0x3D */
	SceInt64 m_notifyValue;			/* +0x40 */
	SceInt32 m_spawnedAppId;		/* +0x48 */
	SceUInt32 m_bootTimeStartUs;		/* +0x4C */
};

typedef char SpawnProcessJobSizeCheck[sizeof(SpawnProcessJob) == 80 ? 1 : -1];

/* (0x8129E6DE) Callback from applauncher PSM confirmation dialog */
static void onPsmConfirmCallback(const paf::string *titleId, SceInt32 launchAppId,
				 SceInt32 checkFlags, bool confirmed)
{
	if (confirmed) {
		TopmenuAppProcess *proc =
			topmenuFindProcessByTitleId(g_topWindow, titleId->c_str());
		if (proc != NULL) {
			proc->ResumeWithArg(launchAppId, checkFlags | 0x10);
			if (proc == g_topWindow->m_foregroundApp)
				proc->SendProcessCommand(1);
		}
	} else {
		if (launchAppId != -1)
			topmenuCancelPendingAppLaunch(launchAppId);
		TopmenuAppProcess *proc =
			topmenuFindProcessByTitleId(g_topWindow, titleId->c_str());
		if (proc != NULL)
			topmenuReturnToHomeIfActive(proc);
	}
}

/* (0x8129E74A) Callback retrying app launch after promote failure */
static int onPromoteRetryCallback(const paf::string *titleId)
{
	return sceAppMgrLaunchAppByNameForShell(0x20000, titleId->c_str(), NULL);
}

/* SpawnBgProcessJob (48 bytes, 0x8129E75A..0x8129E960) */
class SpawnBgProcessJob : public paf::job::JobItem {
public:
	SpawnBgProcessJob(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId)
		: paf::job::JobItem("SpawnBgProcessJob", NULL),
		  m_appInfo(*appInfo),
		  m_launchAppId(launchAppId),
		  m_creatorAppId(-1),
		  m_spawnedAppId(-1)
	{
		const char *titleName = sceLsdbGetName(&m_appInfo);
		if (m_launchAppId != -1 &&
		    (sce_paf_strcmp(titleName, "NPXS10083") == 0 ||
		     sce_paf_strcmp(titleName, "NPXS10084") == 0)) {
			SceAppMgrAppStatus launchStatus;
			if (sceAppMgrGetStatusByAppId(m_launchAppId, &launchStatus) == 0) {
				SceAppMgrAppStatus existingStatus;
				sceAppMgrGetStatusByName(titleName, &existingStatus);
				m_creatorAppId = launchStatus.launchSourceAppId;
			}
		}
	}

	virtual ~SpawnBgProcessJob() {}

	/* (0x8129E91C) */
	virtual int32_t Run()
	{
		if (IsCanceled())
			return -1;

		SceInt32 ret = launchAppWithLsdbInfo(&m_spawnedAppId, &m_appInfo, m_launchAppId,
						     true, 0, NULL);
		if (ret < 0 && m_launchAppId != -1)
			topmenuCancelPendingAppLaunch(m_launchAppId);
		return ret;
	}

	/* (0x8129E960) */
	virtual void Finish(int32_t result)
	{
		if (m_spawnedAppId >= 0) {
			SceAppMgrAppStatus appStatus;
			if (sceAppMgrGetStatusByAppId(m_spawnedAppId, &appStatus) == 0) {
				SceInt64 attr = sceLsdbGetAttribute(&m_appInfo);
				topmenuRecordLaunchedAppFlags(g_topWindow, m_spawnedAppId,
							      appStatus.appName, attr);
			}
		}

		if (result >= 0) {
			const char *parentTitle = SceLsdb_2D538EB9(&m_appInfo);
			if (parentTitle != NULL &&
			    topmenuFindTabByTitleId(g_topWindow, parentTitle) == NULL &&
			    (sceLsdbGetAttribute(&m_appInfo) & 0x4000000LL) == 0) {
				const char *titleName = sceLsdbGetName(&m_appInfo);
				sceAppMgrDestroyAppByName(titleName);
			}
			if (m_creatorAppId != -1) {
				TopmenuAppProcess *creator =
					topmenuFindProcessByRunningAppId(g_topWindow,
									 m_creatorAppId);
				if (creator != NULL) {
					creator->SetHasBgPartner(true);
					if (creator == g_topWindow->m_foregroundApp)
						sceAppMgrResumeBgAppByShell(m_spawnedAppId);
				}
			}
		}
	}

	LsdbAppInfoPtr m_appInfo;	/* +0x1C */
	SceInt32 m_launchAppId;		/* +0x24 */
	SceInt32 m_creatorAppId;	/* +0x28 */
	SceInt32 m_spawnedAppId;	/* +0x2C */
};

typedef char SpawnBgProcessJobSizeCheck[sizeof(SpawnBgProcessJob) == 48 ? 1 : -1];

/* (0x8129EB32) Open a temporary LSDB client to read a single title's metadata */
static int queryLsdbTitleInfo(const char *titleId, LsdbAppInfoPtr *outInfo)
{
	AppInfoQClient *client = SceLsdb_A6D8772C(new AppInfoQClient());
	if (client == NULL)
		return -1;

	int ret = SceLsdb_E90B81CD(client);
	if (ret < 0) {
		SceLsdb_E2101B03(client);
		delete client;
		return ret;
	}

	paf::string titleStr(titleId != NULL ? titleId : "");
	ret = SceLsdb_D239031A(client, &titleStr, outInfo);
	SceLsdb_E2101B03(client);
	delete client;
	return (ret < 0) ? ret : 0;
}

/* (0x8129EBFE) Parse dotted decimal version string ("01.00") into scaled integer (100) */
static int parseAppVersionNumber(const char *str, SceUInt32 maxLen, SceInt32 *outVersion)
{
	*outVersion = 0;

	SceUInt32 len = 0;
	if (str != NULL) {
		while (len < maxLen && str[len] != '\0')
			++len;
	}
	if (len > maxLen - 1)
		return TOPMENU_ERROR_INVALID_VERSION_STR;

	SceInt32 value = 0;
	SceInt32 place = 1;
	for (SceInt32 idx = static_cast<SceInt32>(len) - 1; idx >= 0; --idx) {
		char ch = str[idx];
		if (ch != '.') {
			value += (ch - '0') * place;
			place *= 10;
		}
	}

	*outVersion = value;
	return 0;
}

} /* namespace */

/* ------------------------------------------------------------------ */
/* TopmenuSpawnConfirmHandler (0x8129BA98..0x8129BC06)                */
/* ------------------------------------------------------------------ */

/* (0x8129BA98 / 0x8129BAD6) */
TopmenuSpawnConfirmHandler::TopmenuSpawnConfirmHandler(const LsdbAppInfoPtr *appInfo,
						       SceInt32 launchAppId,
						       SceInt32 existingFlags,
						       SceInt32 extraFlags)
	: m_appInfo(*appInfo),
	  m_launchAppId(launchAppId),
	  m_existingFlags(existingFlags),
	  m_extraFlags(extraFlags)
{
}

/* (0x8129BAE6 / 0x8129BB62 / 0x8129BB72) */
TopmenuSpawnConfirmHandler::~TopmenuSpawnConfirmHandler()
{
	if (m_launchAppId != -1)
		topmenuCancelPendingAppLaunch(m_launchAppId);
}

/* (0x8129BB7A) */
void TopmenuSpawnConfirmHandler::CancelLaunchAndReturnHome()
{
	const char *titleName = sceLsdbGetName(&m_appInfo);
	if (m_launchAppId != -1) {
		topmenuCancelPendingAppLaunch(m_launchAppId);
		m_launchAppId = -1;
	}
	if (titleName != NULL) {
		TopmenuAppProcess *proc = topmenuFindProcessByTitleId(g_topWindow, titleName);
		if (proc != NULL)
			topmenuReturnToHomeIfActive(proc);
	}
}

/* (0x8129BBB4) */
void TopmenuSpawnConfirmHandler::ResumePendingLaunch()
{
	const char *titleName = sceLsdbGetName(&m_appInfo);
	if (titleName == NULL)
		return;

	TopmenuAppProcess *proc = topmenuFindProcessByTitleId(g_topWindow, titleName);
	if (proc != NULL) {
		proc->ResumeWithArg(m_launchAppId, m_existingFlags | m_extraFlags);
		m_launchAppId = -1;
		if (proc == g_topWindow->m_foregroundApp)
			proc->SendProcessCommand(1);
	}
}

/* (0x8129BC06) */
void TopmenuSpawnConfirmHandler::operator()(SceInt32 dialogId, SceInt32 button)
{
	if (button == 4) {
		ResumePendingLaunch();
		SceCommonGuiDialog_BC311DC0(dialogId);
	} else if (button == 5) {
		CancelLaunchAndReturnHome();
		SceCommonGuiDialog_BC311DC0(dialogId);
	}
}

/* ------------------------------------------------------------------ */
/* Spawn job & promote cross-TU APIs                                  */
/* ------------------------------------------------------------------ */

/* (0x8129BD9C) */
void topmenuCreateSpawnJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			   TopmenuProcessBase *proc, SceInt32 launchAppId, SceInt32 checkFlags)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new SpawnProcessJob(proc, launchAppId, checkFlags));
	*outJob = job;
}

/* (0x8129BDFA) */
int topmenuStartSpawnJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			 TopmenuProcessBase *proc, SceInt32 launchAppId, SceInt32 checkFlags)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new SpawnProcessJob(proc, launchAppId, checkFlags));
	*outJob = job;
	return g_topWindow->m_jobQueue->Enqueue(*outJob);
}

/* (0x8129BE88) */
int topmenuEnqueueExistingSpawnJob(paf::common::SharedPtr<paf::job::JobItem> *job)
{
	SpawnProcessJob *spawnJob = static_cast<SpawnProcessJob *>(job->get());
	spawnJob->RefreshFromProcess();
	return g_topWindow->m_jobQueue->Enqueue(*job);
}

/* Helper used by topmenuProcessGetPendingLaunchId (0x8129AB02) */
SceInt32 topmenuSpawnJobGetLaunchAppId(const paf::job::JobItem *job)
{
	return static_cast<const SpawnProcessJob *>(job)->m_launchAppId;
}

/* (0x8129E818) */
int topmenuStartSpawnBgJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			   const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId)
{
	paf::common::SharedPtr<paf::job::JobItem> job(
		new SpawnBgProcessJob(appInfo, launchAppId));
	*outJob = job;
	return g_topWindow->m_jobQueue->Enqueue(*outJob);
}

/* (0x8129EAF8) */
int topmenuSpawnHandlePromoteFailure(const paf::string *titleId, SceInt32 retryCount)
{
	ApplauncherInterface *launcherIf = topmenuGetApplauncherInterface();
	ApplauncherShowPromoteRetryFunc showDialog =
		reinterpret_cast<ApplauncherShowPromoteRetryFunc>(launcherIf->func[22]);
	SceInt32 dialogId = showDialog(titleId, retryCount, onPromoteRetryCallback, -1, 0);
	if (dialogId >= 0)
		registerSpawnDialogHold(dialogId);
	return dialogId;
}

/* (0x8129EC6A) */
int topmenuSpawnWaitPromoteReady(const paf::string *titleId)
{
	SceInt32 version = 100;
	LsdbAppInfoPtr appInfo(new LsdbAppInfo());

	if (queryLsdbTitleInfo(titleId->c_str(), &appInfo) != 0) {
		appInfo.reset();
		return version;
	}

	if (appInfo.get() != NULL) {
		LsdbAppInfo::iterator it = appInfo->find(LSDB_KEY_APP_VER);
		if (it != appInfo->end()) {
			const char *rawVer = "";
			if (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL)
				rawVer = it->second.text->c_str();
			paf::string verStr(rawVer);
			version = 0;

			char verBuf[8];
			sce_paf_memset(verBuf, 0, sizeof(verBuf));
			sce_paf_strncpy(verBuf, verStr.c_str(), sizeof(verBuf));

			bool valid = false;
			if (verBuf[1] == '.') {
				verBuf[4] = '\0';
				valid = (verBuf[0] >= '0' && verBuf[0] <= '9' &&
					 verBuf[2] >= '0' && verBuf[2] <= '9' &&
					 verBuf[3] >= '0' && verBuf[3] <= '9');
			} else {
				verBuf[5] = '\0';
				valid = (verBuf[0] >= '0' && verBuf[0] <= '9' &&
					 verBuf[1] >= '0' && verBuf[1] <= '9' &&
					 verBuf[2] == '.' &&
					 verBuf[3] >= '0' && verBuf[3] <= '9' &&
					 verBuf[4] >= '0' && verBuf[4] <= '9');
			}
			if (valid)
				parseAppVersionNumber(verBuf, sizeof(verBuf), &version);
		}
	}

	return version;
}

/* (0x8129EEF8) */
void topmenuSetPromoteWaitLock(bool locked, const char *titleId)
{
	if (titleId == NULL) {
		g_promoteWaitLocked = false;
		return;
	}
	if (locked) {
		sce_paf_memset(g_promoteWaitTitleId, 0, sizeof(g_promoteWaitTitleId));
		sce_paf_strncpy(g_promoteWaitTitleId, titleId, 32);
		g_promoteWaitLocked = true;
	} else if (sce_paf_strncmp(g_promoteWaitTitleId, titleId, 32) == 0) {
		g_promoteWaitLocked = false;
	}
}

/* (0x8129EF5C) */
void topmenuSpawnOnInstallNoticeEvent(int code, const void *data, SceSize size, void *arg)
{
	(void)code;
	(void)data;
	(void)size;
	SceInt32 dialogId = static_cast<SceInt32>(reinterpret_cast<intptr_t>(arg));
	SceCommonGuiDialog_BC311DC0(dialogId);
}
