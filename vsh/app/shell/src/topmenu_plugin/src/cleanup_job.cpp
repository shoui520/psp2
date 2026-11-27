/*
 * cleanup_job.cpp: CleanUpJob and application crash / coredump dialog handling
 * (0x8128F55C-0x8129289F).
 * Manages crash notification dialogs, live coredump progress updates, suggested
 * app/system update prompts, boot-time crash_report.caf recovery, and uploader
 * registry callbacks.
 */
#include "topmenu_internal.h"

namespace {

const SceInt32 TOPMENU_ERROR_APP_CRASHED	= -2146420471; /* 0x80103909 */
const SceInt32 TOPMENU_ERROR_PREV_CRASH		= -2146420462; /* 0x80103912 */

const SceUInt32 MSG_SUGGEST_HEADING		= 0x2F0C4CF0u; /* 789329520 */
const SceUInt32 MSG_SUGGEST_APP_UPDATE_DESC	= 0x147BDDDCu; /* 343654028 */
const SceUInt32 MSG_SUGGEST_APP_UPDATE_BTN	= 0xED2E4B58u; /* -315730104 */
const SceUInt32 MSG_SUGGEST_SYS_UPDATE_DESC	= 0x67883175u; /* 1736951957 */
const SceUInt32 MSG_SUGGEST_SYS_UPDATE_BTN	= 0x6C2BE8F2u; /* 1814789746 */
const SceUInt32 MSG_CRASH_BTN_SUGGEST		= 0x22CE3851u; /* 583974241 */
const SceUInt32 MSG_CRASH_BTN_REPORT		= 0x1390CF24u; /* 328256772 */
const SceUInt32 MSG_CRASH_BTN_CLOSE		= 0x805C6B76u; /* -2141459290 */
const SceUInt32 MSG_CRASH_APP_BODY		= 0x6EC22A40u; /* 1858223680 */
const SceUInt32 MSG_CRASH_SYSTEM_BODY		= 0xBA7D454Eu; /* -1166246962 */
const SceUInt32 MSG_PREV_CRASH_BODY		= 0xC0B0469Cu; /* -1062153652 */
const SceUInt32 SOUND_TOPMENU_DIALOG_CLOSE	= 0xD74A1EA5u; /* -682994459 */

const SceUInt32 TEMPLATE_CRASH_DUMP_APP		= 0x44B8B45Eu; /* 1152941726 */
const SceUInt32 TEMPLATE_CRASH_DUMP_SYS		= 0xB1B7C4B4u; /* -1313389468 */
const SceUInt32 TEMPLATE_CRASH_NODUMP_APP	= 0xF0AEBD69u; /* -256978247 */
const SceUInt32 WIDGET_CRASH_CONTENT_BOX	= 0x3F522B75u; /* 1062353013 */
const SceUInt32 WIDGET_CRASH_PROGRESS_BAR	= 0x01B1B9ADu; /* 28424461 */
const SceUInt32 WIDGET_CRASH_APP_ICON		= 0x08CBE62Bu; /* 147575179 */
const SceUInt32 WIDGET_CRASH_APP_TITLE		= 0x6D54B5CEu; /* 1834253806 */

const SceInt32 SUGGEST_BUTTON_APP_UPDATE	= 20;
const SceInt32 SUGGEST_BUTTON_SYS_UPDATE	= 21;

typedef void (*GamePluginSetDialogHoldFunc)(bool hold);
typedef int (*LiveAreaHasUpdateFunc)(SceInt32 pageKey);

/* Persistent crash / coredump dialog state (0x81545A18..0x81545A2C, 0x815AC718..0x815AC732) */
static SceInt32 g_crashDialogId		= -1;	/* 0x81545A18 */
static SceInt32 g_crashedAppId		= -1;	/* 0x81545A1C */
static SceInt32 g_crashReportMode	= 0;	/* 0x81545A20 */
static SceUID g_coredumpRegCbId		= -1;	/* 0x81545A24 */
static SceInt32 g_crashReportAppId	= -1;	/* 0x81545A28 */
static SceInt32 g_updaterAppId		= -1;	/* 0x81545A2C */
static paf::string g_crashDumpPath;		/* 0x815AC718 */
static paf::string g_crashedTitleId;		/* 0x815AC724 */
static bool g_hasBootCrashCaf		= false; /* 0x815AC730 */
static bool g_uploaderAutoSend		= false; /* 0x815AC731 */
static bool g_uploaderEnabled		= false; /* 0x815AC732 */

static void setGamePluginDialogHold(bool hold)
{
	GamePluginInterface *gameIf =
		static_cast<GamePluginInterface *>(applauncherPluginGetInterface("game_plugin", 1));
	if (gameIf != NULL) {
		GamePluginSetDialogHoldFunc setHold =
			reinterpret_cast<GamePluginSetDialogHoldFunc>(
				gameIf->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD]);
		setHold(hold);
	}
}

/* (0x8128F55C / 0x8128F578 / 0x8128F598) */
class CleanUpJob : public paf::job::JobItem {
public:
	CleanUpJob(bool deleteCaf, bool deleteDumps)
		: paf::job::JobItem("CleanUpJob", NULL),
		  m_deleteCaf(deleteCaf),
		  m_deleteDumps(deleteDumps)
	{
	}

	virtual ~CleanUpJob() {}

	virtual int32_t Run()
	{
		if (m_deleteCaf)
			vshCoredumpDeleteCrashReportCaf();
		if (m_deleteDumps)
			shellCoredumpCleanup();
		return 0;
	}

	virtual void Finish(int32_t result)
	{
		(void)result;
	}

	bool m_deleteCaf;	/* +0x19 */
	bool m_deleteDumps;	/* +0x1A */
};

static void enqueueCleanUpJob(bool deleteCaf, bool deleteDumps)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new CleanUpJob(deleteCaf, deleteDumps));
	paf::job::JobQueue::default_queue->Enqueue(job);
}

static void onCrashReportAppEvent(int code, const void *data, SceSize size, void *arg);
static void onSystemUpdateAppEvent(int code, const void *data, SceSize size, void *arg);
static void onLiveCrashPowerEvent(int event, const void *eventBuf, SceSize eventSize,
				  void *userData);
static void onBootCrashPowerKey(void *userData);
static int openSuggestedActionsDialog(bool offerAppUpdate, bool offerSystemUpdate);

/* (0x81292768) Delete callback that launches NPXS10077 after boot-crash dialog closes */
static void onBootCrashBusyDeleted(paf::ui::Widget *widget, void *userData)
{
	(void)widget;
	(void)userData;
	topmenuEndBusy();
	SceInt32 appId = sceAppMgrLaunchAppByNameForShell(0x20000, "NPXS10077", NULL);
	if (appId >= 0) {
		topmenuRegisterSystemAppId(g_topWindow, appId);
		g_crashReportAppId = appId;
		setGamePluginDialogHold(true);
		shellAppEventServerAddListener(1, onCrashReportAppEvent, NULL);
	}
}

/* (0x81292312) */
static void onCrashReportAppEvent(int code, const void *data, SceSize size, void *arg)
{
	(void)code;
	(void)size;
	(void)arg;
	const char *appName = static_cast<const char *>(data);
	if (sce_paf_strcmp(appName, "NPXS19999") == 0) {
		if (g_crashReportAppId != -1) {
			setGamePluginDialogHold(false);
			g_crashReportAppId = -1;
		}
		shellAppEventServerRemoveListener(1, onCrashReportAppEvent, NULL);
	}
}

/* (0x81292370) */
static void onSystemUpdateAppEvent(int code, const void *data, SceSize size, void *arg)
{
	(void)code;
	(void)size;
	(void)arg;
	const char *appName = static_cast<const char *>(data);
	if (sce_paf_strcmp(appName, "NPXS19999") == 0) {
		if (g_updaterAppId != -1) {
			setGamePluginDialogHold(false);
			g_updaterAppId = -1;
		}
		shellAppEventServerRemoveListener(1, onSystemUpdateAppEvent, NULL);
	}
}

/* (0x812923CE) */
static int onCoredumpRegChanged(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)userData;
	int flagVal = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/COREDUMP/UPLOADER", "enable_flag", &flagVal) == 0)
		g_uploaderEnabled = (flagVal != 0);
	if (sceRegMgrGetKeyInt("/CONFIG/COREDUMP/UPLOADER", "auto_flag", &flagVal) == 0)
		g_uploaderAutoSend = (flagVal != 0);
	return 0;
}

/* (0x8129244A) */
static void onCrashReportEnded(int code, const void *data, SceSize size, void *arg)
{
	(void)code;
	(void)size;
	(void)arg;
	const ShellAppEvent *ev = static_cast<const ShellAppEvent *>(data);
	if (ev != NULL && ev->titleId != NULL && sce_paf_strcmp(ev->titleId, "NPXS10077") == 0)
		enqueueCleanUpJob(true, true);
}

/* (0x81292540) */
static void onLiveCrashPowerEvent(int event, const void *eventBuf, SceSize eventSize,
				  void *userData)
{
	(void)event;
	(void)eventBuf;
	(void)eventSize;
	(void)userData;
	SceCommonGuiDialog_BC311DC0(g_crashDialogId);
	sceAppMgrFinishCoredumpForShell();
	g_crashDialogId = -1;
	g_crashedAppId = -1;
	if (g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->removePowerEventListener(onLiveCrashPowerEvent, NULL);
	enqueueCleanUpJob(true, true);
}

/* (0x81292662) */
static void onBootCrashPowerKey(void *userData)
{
	SceInt32 dialogId = static_cast<SceInt32>(reinterpret_cast<intptr_t>(userData));
	SceCommonGuiDialog_BC311DC0(dialogId);
	if (g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->popKeyHandler(onBootCrashPowerKey, userData);
	enqueueCleanUpJob(true, true);
}

static void adjustCrashDialogButtonWidth(paf::ui::Widget *btn, float minWidth)
{
	paf::ui::AdjustParam adjustParam = (btn->m_adjust_param != NULL) ?
		*btn->m_adjust_param : ScePafWidget_BE1CEA2E;
	adjustParam.m_min_size.set_x(minWidth);
	ScePafWidget_622193BD(btn, true);
	ScePafWidget_B1C23134(btn, adjustParam);
}

static void styleCloseButtonWithWrapper(paf::ui::Widget *closeBtn)
{
	paf::ui::Widget *wrapper = new paf::ui::Widget(closeBtn->m_parent, NULL);
	wrapper->SetSize(*closeBtn->GetSize(0), NULL, 0x10004, 0, 0.0f, NULL, NULL);
	wrapper->SetAdjust(closeBtn->m_adjust_x, closeBtn->m_adjust_y, closeBtn->m_adjust_z);

	adjustCrashDialogButtonWidth(closeBtn, 320.0f);
	closeBtn->SetParent(wrapper, NULL, 0);

	const wchar_t *label = g_topmenuPlugin->GetString(paf::IDParam(MSG_CRASH_BTN_CLOSE));
	paf::wstring labelStr(label != NULL ? label : L"");
	closeBtn->SetString(labelStr);
	closeBtn->SetAdjust(2, closeBtn->m_adjust_y, closeBtn->m_adjust_z);
}

/* Handler for Suggested Actions list dialog (0x8128F5B2) */
class SuggestedActionsHandler : public CommonGuiDialogHandler {
public:
	SuggestedActionsHandler(bool offerAppUpdate, bool offerSystemUpdate)
		: m_offerAppUpdate(offerAppUpdate), m_offerSystemUpdate(offerSystemUpdate)
	{
	}
	virtual ~SuggestedActionsHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		switch (button) {
		case 0:
			topmenuDialogOnPowerHold(dialogId);
			break;
		case SUGGEST_BUTTON_APP_UPDATE: {
			TopTab *tab = topmenuFindTabByTitleId(g_topWindow, g_crashedTitleId.c_str());
			if (tab != NULL)
				topmenuTriggerLiveAreaUpdate(tab->m_pageKey);
			break;
		}
		case SUGGEST_BUTTON_SYS_UPDATE: {
			SceInt32 appId = sceAppMgrLaunchAppByNameForShell(
				0x20000, "NPXS10016", "updater::via_downloader");
			if (appId >= 0) {
				topmenuRegisterSystemAppId(g_topWindow, appId);
				g_updaterAppId = appId;
				setGamePluginDialogHold(true);
				shellAppEventServerAddListener(1, onSystemUpdateAppEvent, NULL);
			}
			break;
		}
		default:
			break;
		}
	}

	bool m_offerAppUpdate;
	bool m_offerSystemUpdate;
};

/* (0x8128F64C) */
static int openSuggestedActionsDialog(bool offerAppUpdate, bool offerSystemUpdate)
{
	CommonGuiListDialogParam param(true);
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.soundId = SOUND_TOPMENU_DIALOG_CLOSE;
	param.notifyClose = true;
	param.type = 1;

	CommonGuiListDialogItem headerRow;
	headerRow.mark = false;
	headerRow.shadow = false;
	const wchar_t *headStr = g_topmenuPlugin->GetString(paf::IDParam(MSG_SUGGEST_HEADING));
	headerRow.text = (headStr != NULL) ? headStr : L"";
	param.items.push_back(headerRow);

	if (offerAppUpdate) {
		CommonGuiListDialogItem appRow;
		appRow.mark = true;
		appRow.shadow = false;
		const wchar_t *desc =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_SUGGEST_APP_UPDATE_DESC));
		const wchar_t *btnLabel =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_SUGGEST_APP_UPDATE_BTN));
		appRow.text = (desc != NULL) ? desc : L"";
		appRow.buttonLabel = (btnLabel != NULL) ? btnLabel : L"";
		appRow.buttonValue = SUGGEST_BUTTON_APP_UPDATE;
		param.items.push_back(appRow);

		if (offerSystemUpdate) {
			CommonGuiListDialogItem spacerRow;
			spacerRow.mark = false;
			spacerRow.shadow = false;
			param.items.push_back(spacerRow);
		}
	}

	if (offerSystemUpdate) {
		CommonGuiListDialogItem sysRow;
		sysRow.mark = true;
		sysRow.shadow = false;
		const wchar_t *desc =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_SUGGEST_SYS_UPDATE_DESC));
		const wchar_t *btnLabel =
			g_topmenuPlugin->GetString(paf::IDParam(MSG_SUGGEST_SYS_UPDATE_BTN));
		sysRow.text = (desc != NULL) ? desc : L"";
		sysRow.buttonLabel = (btnLabel != NULL) ? btnLabel : L"";
		sysRow.buttonValue = SUGGEST_BUTTON_SYS_UPDATE;
		param.items.push_back(sysRow);
	}

	param.handler = new SuggestedActionsHandler(offerAppUpdate, offerSystemUpdate);
	SceInt32 dialogId = SceCommonGuiDialog_F3D37CAE(&param);
	if (dialogId >= 0) {
		topmenuDialogRegisterDeferred(dialogId);
		topmenuDialogRegisterAndHold(dialogId);

		paf::ui::Widget *boxWidget = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (boxWidget != NULL && boxWidget->IsInherit(paf::ui::Box::TypeName())) {
			paf::ui::Box *box = static_cast<paf::ui::Box *>(boxWidget);
			paf::ui::Box::LayoutVars vars = box->GetLayoutVars();
			vars.min_size.set_y(352.0f);
			box->SetLayoutVars(vars);
			box->SetOrigin(box->m_origin_x, 2, box->m_origin_z, NULL);
		}
	}
	return dialogId;
}

/* Handler for boot-time previous crash dialog (0x8128FC7A / 0x8128FCF4) */
class BootCrashDialogHandler : public CommonGuiDialogHandler {
public:
	explicit BootCrashDialogHandler(bool hasSystemUpdate)
		: m_hasSystemUpdate(hasSystemUpdate)
	{
		setGamePluginDialogHold(true);
	}

	virtual ~BootCrashDialogHandler()
	{
		setGamePluginDialogHold(false);
	}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		SceInt32 suggestBtn = -1;
		SceInt32 reportBtn = 4;
		SceInt32 closeBtn = 5;
		if (m_hasSystemUpdate) {
			suggestBtn = 4;
			reportBtn = 5;
			closeBtn = 3;
		}

		void *dialogArg = reinterpret_cast<void *>(static_cast<intptr_t>(dialogId));
		if (button == suggestBtn) {
			SceCommonGuiDialog_BC311DC0(dialogId);
			if (g_topWindow->m_powerManageIf != NULL)
				g_topWindow->m_powerManageIf->popKeyHandler(onBootCrashPowerKey, dialogArg);
			enqueueCleanUpJob(true, true);
			openSuggestedActionsDialog(false, m_hasSystemUpdate);
		}

		if (button == reportBtn) {
			paf::ui::Widget *busyWidget = SceCommonGuiDialog_8947FC23(dialogId, 4);
			if (busyWidget != NULL) {
				topmenuBeginBusy();
				busyWidget->AddDeleteCB(onBootCrashBusyDeleted, NULL);
			}
			SceCommonGuiDialog_BC311DC0(dialogId);
			if (g_topWindow->m_powerManageIf != NULL)
				g_topWindow->m_powerManageIf->popKeyHandler(onBootCrashPowerKey, dialogArg);
		} else if (button == closeBtn) {
			SceCommonGuiDialog_BC311DC0(dialogId);
			if (g_topWindow->m_powerManageIf != NULL)
				g_topWindow->m_powerManageIf->popKeyHandler(onBootCrashPowerKey, dialogArg);
			enqueueCleanUpJob(true, true);
		}
	}

	bool m_hasSystemUpdate;
};

/* Handler for live app-crash dialog (0x8129007A) */
class LiveCrashDialogHandler : public CommonGuiDialogHandler {
public:
	LiveCrashDialogHandler(bool hasSuggestions, bool hasAppUpdate,
			       bool hasSystemUpdate, bool canReport)
		: m_hasSuggestions(hasSuggestions),
		  m_hasAppUpdate(hasAppUpdate),
		  m_hasSystemUpdate(hasSystemUpdate),
		  m_canReport(canReport)
	{
	}
	virtual ~LiveCrashDialogHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)dialogId;
		SceInt32 suggestBtn = -1;
		SceInt32 reportBtn = -1;
		SceInt32 closeBtn = 2;

		if (m_hasSuggestions) {
			suggestBtn = 4;
			if (m_canReport) {
				reportBtn = 5;
				closeBtn = 3;
			} else {
				closeBtn = 5;
			}
		} else if (m_canReport) {
			reportBtn = 4;
			closeBtn = 5;
		}

		if (button == suggestBtn) {
			SceCommonGuiDialog_BC311DC0(g_crashDialogId);
			sceAppMgrFinishCoredumpForShell();
			g_crashDialogId = -1;
			g_crashedAppId = -1;
			if (g_topWindow->m_powerManageIf != NULL) {
				g_topWindow->m_powerManageIf->removePowerEventListener(
					onLiveCrashPowerEvent, NULL);
			}
			enqueueCleanUpJob(true, true);
			openSuggestedActionsDialog(m_hasAppUpdate, m_hasSystemUpdate);
		} else if (button == reportBtn) {
			TopmenuCoredumpState dumpState;
			int stateRet = sceAppMgrGetCoredumpStateForShell(&dumpState);
			SceCommonGuiDialog_BC311DC0(g_crashDialogId);
			sceAppMgrFinishCoredumpForShell();
			g_crashDialogId = -1;
			g_crashedAppId = -1;
			if (g_topWindow->m_powerManageIf != NULL) {
				g_topWindow->m_powerManageIf->removePowerEventListener(
					onLiveCrashPowerEvent, NULL);
			}
			if (stateRet == 0) {
				g_crashDumpPath = dumpState.path;
				g_crashReportMode = dumpState.isAutoUpload;
				if (!g_crashDumpPath.empty()) {
					topmenuBeginBusy();
					topmenuLaunchPendingCrashReport();
				}
			}
		} else if (button == closeBtn) {
			SceCommonGuiDialog_BC311DC0(g_crashDialogId);
			sceAppMgrFinishCoredumpForShell();
			g_crashDialogId = -1;
			g_crashedAppId = -1;
			if (g_topWindow->m_powerManageIf != NULL) {
				g_topWindow->m_powerManageIf->removePowerEventListener(
					onLiveCrashPowerEvent, NULL);
			}
			enqueueCleanUpJob(true, true);
		}
	}

	bool m_hasSuggestions;
	bool m_hasAppUpdate;
	bool m_hasSystemUpdate;
	bool m_canReport;
};

/* Periodic coredump progress updater for live crash dialog (0x81290428) */
class LiveCrashProgressUpdater : public CommonGuiLayoutDialogOpenHandler {
public:
	explicit LiveCrashProgressUpdater(bool hasSuggestions)
		: m_hasSuggestions(hasSuggestions), m_finished(false)
	{
	}
	virtual ~LiveCrashProgressUpdater() {}

	virtual void operator()(SceInt32 dialogId)
	{
		TopmenuCoredumpState dumpState;
		if (sceAppMgrGetCoredumpStateForShell(&dumpState) != 0)
			return;

		paf::ui::Widget *contentBox = NULL;
		paf::ui::Widget *progressWidget = NULL;
		paf::ui::Widget *rootBox = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (rootBox != NULL) {
			contentBox = rootBox->FindChild(paf::IDParam(WIDGET_CRASH_CONTENT_BOX), 0);
			if (contentBox != NULL) {
				progressWidget = contentBox->FindChild(
					paf::IDParam(WIDGET_CRASH_PROGRESS_BAR), 0);
			}
		}

		if (dumpState.isCompleted == 0) {
			if (progressWidget != NULL) {
				static_cast<paf::ui::ProgressBar *>(progressWidget)->SetValue(
					dumpState.progress, true, paf::ui::ProgressBar::BAR_FORE);
			}
			return;
		}

		if (m_finished)
			return;
		m_finished = true;

		if (dumpState.saveResult == 0) {
			if (progressWidget != NULL) {
				static_cast<paf::ui::ProgressBar *>(progressWidget)->SetValue(
					dumpState.progress, true, paf::ui::ProgressBar::BAR_FORE);
				paf::Timer *timer = new paf::Timer();
				if (timer != NULL)
					timer->InitTimer(0.0f, paf::Timer::FUNC_LINEAR, 0, -1, NULL);
				progressWidget->Hide(0xff, timer, NULL, 0.0f);
			}

			if (contentBox != NULL) {
				SceInt32 reportPart = m_hasSuggestions ? 13 : 12;
				paf::ui::ButtonBase *reportBtn =
					static_cast<paf::ui::ButtonBase *>(
						SceCommonGuiDialog_8947FC23(dialogId, reportPart));
				if (reportBtn != NULL) {
					const wchar_t *label = g_topmenuPlugin->GetString(
						paf::IDParam(MSG_CRASH_BTN_REPORT));
					paf::wstring labelStr(label != NULL ? label : L"");
					reportBtn->SetString(labelStr);
					if (g_uploaderEnabled || dumpState.isAutoUpload != 0)
						reportBtn->Enable(false);
				}
			}

			if (dumpState.isAutoUpload == 0 && g_uploaderAutoSend) {
				SceCommonGuiDialog_BC311DC0(g_crashDialogId);
				sceAppMgrFinishCoredumpForShell();
				g_crashDialogId = -1;
				g_crashedAppId = -1;
				if (g_topWindow->m_powerManageIf != NULL) {
					g_topWindow->m_powerManageIf->removePowerEventListener(
						onLiveCrashPowerEvent, NULL);
				}
				g_crashDumpPath = dumpState.path;
				g_crashReportMode = dumpState.isAutoUpload;
				if (!g_crashDumpPath.empty())
					topmenuBeginBusy();
			}
		}

		if (dumpState.isAutoUpload == 0) {
			paf::ui::Widget *textWidget = SceCommonGuiDialog_8947FC23(dialogId, 1);
			if (textWidget != NULL) {
				paf::wstring currentText;
				textWidget->GetString(currentText);

				paf::string pathUtf8(dumpState.path);
				paf::wstring pathWide;
				paf::common::Utf8ToUtf16(pathUtf8.c_str(), &pathWide);

				const wchar_t *statusLine = (dumpState.saveResult != 0) ?
					L"\nSaved Core File Failed.\n" :
					L"\nSaved Core File Succeeded.\n";

				paf::wstring suffix(statusLine);
				suffix += pathWide.c_str();
				paf::wstring updated = currentText + suffix;
				textWidget->SetString(updated);
			}
		}
	}

	bool m_hasSuggestions;
	bool m_finished;
};

/* (0x8129186C) Show previous-boot crash report dialog */
static void showBootCrashDialog(void)
{
	CommonGuiMessageDialogParam param;
	bool hasSystemUpdate = shellBgdlHasPending();

	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.defaultButton = 0;
	param.cancelButton = 0;
	param.type = TOPMENU_ERROR_PREV_CRASH;
	param.messageId = MSG_PREV_CRASH_BODY;
	param.modal = false;
	param.useLayout = true;
	param.handler = new BootCrashDialogHandler(hasSystemUpdate);
	param.layout = hasSystemUpdate ? SceCommonGuiDialog_78399261 : SceCommonGuiDialog_22B17F2B;

	SceInt32 dialogId = SceCommonGuiDialog_20B141F1(&param);
	if (dialogId < 0) {
		enqueueCleanUpJob(true, true);
		return;
	}

	paf::ui::Widget *suggestBtn = NULL;
	paf::ui::Widget *reportBtn = NULL;
	paf::ui::Widget *closeBtn = NULL;
	if (hasSystemUpdate) {
		suggestBtn = SceCommonGuiDialog_8947FC23(dialogId, 12);
		reportBtn = SceCommonGuiDialog_8947FC23(dialogId, 13);
		closeBtn = SceCommonGuiDialog_8947FC23(dialogId, 14);
	} else {
		reportBtn = SceCommonGuiDialog_8947FC23(dialogId, 12);
		closeBtn = SceCommonGuiDialog_8947FC23(dialogId, 13);
	}

	if (hasSystemUpdate && suggestBtn != NULL) {
		const wchar_t *label = g_topmenuPlugin->GetString(paf::IDParam(MSG_CRASH_BTN_SUGGEST));
		paf::wstring labelStr(label != NULL ? label : L"");
		suggestBtn->SetString(labelStr);
		adjustCrashDialogButtonWidth(suggestBtn, 426.0f);
	}

	if (reportBtn != NULL) {
		const wchar_t *label = g_topmenuPlugin->GetString(paf::IDParam(MSG_CRASH_BTN_REPORT));
		paf::wstring labelStr(label != NULL ? label : L"");
		reportBtn->SetString(labelStr);
		adjustCrashDialogButtonWidth(reportBtn, 426.0f);
	}

	if (closeBtn != NULL)
		styleCloseButtonWithWrapper(closeBtn);

	if (g_topWindow->m_powerManageIf != NULL) {
		g_topWindow->m_powerManageIf->pushKeyHandler(
			onBootCrashPowerKey,
			reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
	}
}

} /* namespace */

/* (0x8128FFCC) */
int topmenuLaunchPendingCrashReport(void)
{
	if (g_crashDumpPath.empty())
		return 0;

	char launchParam[1024];
	sce_paf_snprintf(launchParam, sizeof(launchParam), "%s,%s,%d",
			 g_crashedTitleId.c_str(), g_crashDumpPath.c_str(), g_crashReportMode);
	sceAppMgrLaunchAppByNameForShell(0x20000, "NPXS10077", launchParam);

	if (!g_crashDumpPath.empty())
		topmenuEndBusy();

	g_crashDumpPath.clear();
	g_crashedTitleId.clear();
	return 1;
}

/* (0x81290A60) */
int topmenuHandleAppCrash(const TopmenuCrashEvent *crashEvent)
{
	TopmenuAppProcess *fgApp = g_topWindow->m_foregroundApp;
	if (fgApp == NULL || crashEvent->appId != fgApp->m_appId) {
		SceAppMgrAppStatus appStatus;
		if (sceAppMgrGetStatusByAppId(crashEvent->appId, &appStatus) != 0 ||
		    (sce_paf_strcmp(appStatus.appName, "NPXS10083") == 0 &&
		     sce_paf_strcmp(appStatus.appName, "NPXS10084") == 0) ||
		    fgApp == NULL || appStatus.launchSourceAppId != fgApp->m_appId) {
			sceAppMgrFinishCoredumpForShell();
			return 0;
		}
	}

	TopTab *crashedTab = NULL;
	if (fgApp != NULL && fgApp->m_tabAppData != NULL)
		crashedTab = fgApp->m_tabAppData->m_ownerTab;

	bool hasSystemUpdate = shellBgdlHasPending();
	bool hasAppUpdate = false;
	if (crashedTab != NULL) {
		LiveAreaPluginInterface *liveAreaIf = topmenuGetLiveAreaInterface();
		if (liveAreaIf != NULL && liveAreaIf->hasPatchIcon != NULL)
			hasAppUpdate = (liveAreaIf->hasPatchIcon(crashedTab->m_pageKey) != 0);
	}

	bool hasSuggestions = (hasSystemUpdate || hasAppUpdate);
	int dbState = shellBootGetDatabaseState();
	bool canReport = (dbState != -1 && dbState != 0 && dbState != 3);

	CommonGuiMessageDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.type = TOPMENU_ERROR_APP_CRASHED;
	param.defaultButton = 1;
	param.modal = false;
	param.useLayout = true;
	param.handler = new LiveCrashDialogHandler(hasSuggestions, hasAppUpdate,
						   hasSystemUpdate, canReport);

	if (hasSuggestions && canReport)
		param.layout = SceCommonGuiDialog_78399261;
	else if (hasSuggestions || canReport)
		param.layout = SceCommonGuiDialog_22B17F2B;
	else
		param.layout = SceCommonGuiDialog_AF75CA6D;

	param.messageId = (fgApp != NULL && fgApp->m_tabAppData != NULL) ?
		MSG_CRASH_APP_BODY : MSG_CRASH_SYSTEM_BODY;

	SceInt32 dialogId = SceCommonGuiDialog_20B141F1(&param);
	if (dialogId < 0) {
		sceAppMgrFinishCoredumpForShell();
		return 0;
	}

	if (fgApp != NULL) {
		g_crashedTitleId = fgApp->m_titleId;

		paf::ui::Widget *rootBox = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (rootBox != NULL && rootBox->IsInherit(paf::ui::Box::TypeName())) {
			paf::ui::Widget *msgWidget = SceCommonGuiDialog_8947FC23(dialogId, 1);
			if (msgWidget != NULL && msgWidget->IsInherit(paf::ui::Text::TypeName())) {
				paf::ui::Text *msgText = static_cast<paf::ui::Text *>(msgWidget);
				paf::math::v4 size = *msgText->GetSize(0);
				size.set_x(600.0f);
				msgText->SetSize(size, NULL, 0x10004, 0, 0.0f, NULL, NULL);
				msgText->SetAdjust(0, msgText->m_adjust_y, msgText->m_adjust_z);
				msgText->SetLayoutAttribute(
					static_cast<paf::graph::TextLayoutAttribute>(2), 0);
				msgText->SetLayoutAttribute(
					static_cast<paf::graph::TextLayoutAttribute>(0), 0);
			}

			paf::Plugin::TemplateOpenParam openParam;
			if (crashEvent->hasCoredump == 1) {
				SceUInt32 tplId = (crashedTab != NULL) ?
					TEMPLATE_CRASH_DUMP_APP : TEMPLATE_CRASH_DUMP_SYS;
				g_topmenuPlugin->TemplateOpen(rootBox, paf::IDParam(tplId),
							      openParam);
			} else if (crashedTab != NULL) {
				g_topmenuPlugin->TemplateOpen(rootBox,
							      paf::IDParam(TEMPLATE_CRASH_NODUMP_APP),
							      openParam);
			}

			paf::ui::Widget *contentBox =
				rootBox->FindChild(paf::IDParam(WIDGET_CRASH_CONTENT_BOX), 0);
			if (contentBox != NULL) {
				if (crashedTab != NULL) {
					paf::ui::Widget *iconWidget = contentBox->FindChild(
						paf::IDParam(WIDGET_CRASH_APP_ICON), 0);
					if (iconWidget != NULL)
						iconWidget->SetDrawObjMaskBit(&crashedTab->m_iconCache, 0);

					paf::ui::Widget *titleWidget = contentBox->FindChild(
						paf::IDParam(WIDGET_CRASH_APP_TITLE), 0);
					if (titleWidget != NULL) {
						const char *appTitle =
							sceLsdbGetTitle(&fgApp->m_appInfo);
						if (appTitle != NULL) {
							paf::string titleUtf8(appTitle);
							paf::wstring titleWide;
							paf::common::Utf8ToUtf16(titleUtf8.c_str(),
										 &titleWide);
							titleWidget->SetString(titleWide);
						}
					}
				}

				if (crashEvent->hasCoredump == 1) {
					paf::ui::Widget *suggestBtn = NULL;
					paf::ui::ButtonBase *reportBtn = NULL;
					paf::ui::Widget *closeBtn = NULL;

					if (hasSuggestions && canReport) {
						suggestBtn = SceCommonGuiDialog_8947FC23(dialogId, 12);
						reportBtn = static_cast<paf::ui::ButtonBase *>(
							SceCommonGuiDialog_8947FC23(dialogId, 13));
						closeBtn = SceCommonGuiDialog_8947FC23(dialogId, 14);
					} else if (canReport) {
						reportBtn = static_cast<paf::ui::ButtonBase *>(
							SceCommonGuiDialog_8947FC23(dialogId, 12));
						closeBtn = SceCommonGuiDialog_8947FC23(dialogId, 13);
					} else if (hasSuggestions) {
						suggestBtn = SceCommonGuiDialog_8947FC23(dialogId, 12);
						closeBtn = SceCommonGuiDialog_8947FC23(dialogId, 13);
					}

					if (suggestBtn != NULL) {
						const wchar_t *label = g_topmenuPlugin->GetString(
							paf::IDParam(MSG_CRASH_BTN_SUGGEST));
						paf::wstring labelStr(label != NULL ? label : L"");
						suggestBtn->SetString(labelStr);
						adjustCrashDialogButtonWidth(suggestBtn, 426.0f);
					}

					if (reportBtn != NULL) {
						const wchar_t *label = g_topmenuPlugin->GetString(
							paf::IDParam(MSG_CRASH_BTN_REPORT));
						paf::wstring labelStr(label != NULL ? label : L"");
						reportBtn->SetString(labelStr);
						reportBtn->Disable(false);
						adjustCrashDialogButtonWidth(reportBtn, 426.0f);
					}

					if (closeBtn != NULL)
						styleCloseButtonWithWrapper(closeBtn);
				}
			}
		}
	} else {
		g_crashedTitleId.clear();
	}

	g_crashDialogId = dialogId;
	g_crashedAppId = crashEvent->appId;

	if (g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->addPowerEventListener(onLiveCrashPowerEvent, NULL);

	if (crashEvent->hasCoredump == 1 && canReport) {
		SceCommonGuiDialog_17091699(g_crashDialogId,
					    new LiveCrashProgressUpdater(hasSuggestions), 100.0f);
	} else {
		paf::ui::Widget *rootBox = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (rootBox != NULL) {
			paf::ui::Widget *progressWidget =
				rootBox->FindChild(paf::IDParam(WIDGET_CRASH_PROGRESS_BAR), 0);
			if (progressWidget != NULL)
				progressWidget->Hide(0xff, NULL, NULL, 0.0f);
		}
	}

	return 1;
}

/* (0x812917C4) */
void topmenuCloseCrashDialogForApp(SceInt32 appId)
{
	if (appId > 0 && g_crashedAppId == appId) {
		SceCommonGuiDialog_BC311DC0(g_crashDialogId);
		sceAppMgrFinishCoredumpForShell();
		g_crashDialogId = -1;
		g_crashedAppId = -1;
		if (g_topWindow->m_powerManageIf != NULL) {
			g_topWindow->m_powerManageIf->removePowerEventListener(
				onLiveCrashPowerEvent, NULL);
		}
	}
}

/* (0x8129181A) */
bool topmenuHasPendingCrashReport(void)
{
	return !g_crashDumpPath.empty();
}

/* (0x8129182C) */
int topmenuCheckCrashReportFile(void)
{
	g_hasBootCrashCaf = paf::LocalFile::IsFile("ux0:data/crash_report.caf");
	if (g_hasBootCrashCaf)
		setGamePluginDialogHold(true);
	return g_hasBootCrashCaf ? 1 : 0;
}

/* (0x81291FE0) */
int topmenuProcessCrashReportOnBoot(void)
{
	if (g_hasBootCrashCaf) {
		if (shellStartupNeedsInitialSetup())
			enqueueCleanUpJob(true, true);
		else
			showBootCrashDialog();
		setGamePluginDialogHold(false);
		return 1;
	}

	enqueueCleanUpJob(false, true);
	return 0;
}

/* (0x812921BC) */
void topmenuCoredumpInit(void)
{
	SceShell_83D16CA8(SHELL_APP_EVENT_ENDED, onCrashReportEnded, NULL);

	SceUID cbId = sceKernelCreateCallback("SceShellCoreDumpReg", 0,
						     onCoredumpRegChanged, NULL);
	if (cbId < 0)
		return;

	g_coredumpRegCbId = cbId;
	if (sceRegMgrRegisterCallback(g_coredumpRegCbId) < 0 ||
	    sceRegMgrAddRegistryCallback(g_coredumpRegCbId,
						"/CONFIG/COREDUMP/UPLOADER/enable_flag") < 0 ||
	    sceRegMgrAddRegistryCallback(g_coredumpRegCbId,
						"/CONFIG/COREDUMP/UPLOADER/auto_flag") < 0) {
		sceKernelDeleteCallback(g_coredumpRegCbId);
		g_coredumpRegCbId = -1;
		return;
	}

	if (sceRegMgrStartCallback(g_coredumpRegCbId) < 0) {
		sceRegMgrUnregisterCallback(g_coredumpRegCbId);
		sceKernelDeleteCallback(g_coredumpRegCbId);
		g_coredumpRegCbId = -1;
		return;
	}

	int flagVal = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/COREDUMP/UPLOADER", "enable_flag", &flagVal) == 0)
		g_uploaderEnabled = (flagVal != 0);
	if (sceRegMgrGetKeyInt("/CONFIG/COREDUMP/UPLOADER", "auto_flag", &flagVal) == 0)
		g_uploaderAutoSend = (flagVal != 0);
}
