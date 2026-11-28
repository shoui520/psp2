/*
 * patch_chk_controller.cpp: main patch check state machine coordinating UI
 * dialogs, SceGameUpdate, ChangeInfo fetch, BGDL task creation, sysupchk_plugin,
 * and topmenu_plugin launch handoff (0x812AC3A8-0x812ADFF6).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <common/char_encoding.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "../../../../paf/include/rtc/rtc.h"
#include "../../plugins.h"
#include "src/livespace_db/lsdb_keys.h"
#include "patch_chk_internal.h"

using namespace paf;

/* RCO localized string hashes used by PatchChkController */
#define MSG_UPDATE_HEADER_TITLE		0x92D7ABCCu
#define MSG_UPDATE_HEADER_VERSION	0xEC28B1FEu
#define MSG_CLOSE_APP_TO_INSTALL	0x382AD98Eu
#define MSG_QUIET_INSTALL_PROMPT	0x3B96EA09u
#define MSG_DEFAULT_ERROR_SENTINEL	0xC51B20E4u

/* Topmenu interface slot 65 (+260 = 0x104): close running app / launch update installer */
typedef int (*TopmenuLaunchUpdateFunc)(const char *titleId,
				       void (*callback)(int result, void *userData),
				       void *userData);

/* Constructor (0x812AC3A8) */
PatchChkController::PatchChkController()
	: m_ui(NULL),
	  m_runner(NULL),
	  m_sysUpChk(NULL),
	  m_changeInfo(NULL),
	  m_finishCallback(NULL),
	  m_stateCallback(NULL),
	  m_finishUserData(NULL),
	  m_stateUserData(NULL),
	  m_checkMode(0),
	  m_uiState(PATCH_CHK_STATE_IDLE),
	  m_lastError(0),
	  m_infoFlags(0),
	  m_updateStatus(0),
	  m_stopping(false),
	  m_updateFinishedPendingTimer(false),
	  m_timerState(0),
	  m_deferredUpdateResult(-1)
{
	shellBgdlPatchDownloadTaskParamClear(&m_downloadParam);
	sce_paf_memset(&m_startParam, 0, sizeof(m_startParam));
}

/* Destructor (0x812AC43C) */
PatchChkController::~PatchChkController()
{
}

/* Stops and detaches active background jobs without invoking finish callbacks (0x812AC51C) */
void PatchChkController::Stop()
{
	m_stopping = true;
	if (m_runner != NULL) {
		if (!m_runner->IsUpdateIdle())
			m_runner->DetachUpdateJob();
		if (!m_runner->IsDownloadIdle())
			m_runner->DetachDownloadJob();
	}
	if (m_changeInfo != NULL)
		m_changeInfo->StopAndMarkCanceled();
}

/* Deletes all owned sub-controllers (0x812AC554) */
void PatchChkController::Cleanup()
{
	if (m_sysUpChk != NULL) {
		delete m_sysUpChk;
		m_sysUpChk = NULL;
	}
	if (m_ui != NULL) {
		delete m_ui;
		m_ui = NULL;
	}
	if (m_runner != NULL) {
		delete m_runner;
		m_runner = NULL;
	}
	if (m_changeInfo != NULL) {
		delete m_changeInfo;
		m_changeInfo = NULL;
	}
}

/* Allocates UI and runner for a new patch check session (0x812AC5A2) */
int PatchChkController::InitializeSession(SceUInt16 checkMode,
					  PatchChkFinishCallback finishCallback,
					  void *userData)
{
	m_checkMode = checkMode;
	if (m_ui != NULL)
		delete m_ui;
	m_ui = new PatchChkUi();
	if (m_ui == NULL)
		return PATCH_CHK_ERROR_NO_MEMORY;

	m_ui->SetEventCallback(OnUiEvent, this);
	if (m_checkMode == 1)
		m_ui->OpenOverlayPage();

	if (m_runner != NULL)
		delete m_runner;
	m_runner = new PatchChkGameUpdateRunner();
	if (m_runner == NULL)
		return PATCH_CHK_ERROR_NO_MEMORY;

	m_runner->SetUpdateDoneCallback(OnUpdateRunnerDone, this);
	m_runner->SetDownloadDoneCallback(OnDownloadRunnerDone, this);
	m_finishCallback = finishCallback;
	m_finishUserData = userData;
	return 0;
}

/* Closes active dialogs/pages and invokes finish callback (0x812AC666) */
void PatchChkController::FinishAndNotify()
{
	if (m_sysUpChk != NULL && m_uiState == PATCH_CHK_STATE_SYSUPDATE_PROMPT)
		m_sysUpChk->Unload();

	if (m_ui != NULL) {
		if (m_checkMode == 1)
			m_ui->CloseOverlayPage();

		if (m_uiState >= PATCH_CHK_STATE_SYSUPDATE_PROMPT) {
			if (m_checkMode == 1)
				patchChkGetPlugin()->Notify(2);
			else
				m_ui->CloseCurrentDialog();
		}
	}

	if (m_finishCallback != NULL) {
		PatchChkResultData resultData;
		resultData.infoFlags = m_infoFlags;
		resultData.updateStatus = m_updateStatus;
		m_finishCallback(m_lastError, &resultData, m_finishUserData);
	}
}

/* Builds the RichText <h3> header showing title, latest version, and download size (0x812AC6FC) */
void PatchChkController::BuildUpdateHeaderHtml(string *outHtml, SceUInt64 totalSize,
					       const char *latestVersionStr)
{
	wstring versionWide;
	common::Utf8ToUtf16(latestVersionStr, &versionWide);

	outHtml->append("<h3>");
	IDParam headerTitleId(MSG_UPDATE_HEADER_TITLE);
	const wchar_t *headerTitleWide = patchChkGetString(headerTitleId);
	string headerTitleUtf8;
	if (headerTitleWide != NULL)
		common::Utf16ToUtf8(headerTitleWide, &headerTitleUtf8);
	outHtml->append(headerTitleUtf8);
	outHtml->append("</h3><br/>");

	wstring sizeParen;
	patchChkFormatSizeParen(&sizeParen, totalSize);
	versionWide.append(sizeParen);

	outHtml->append("<h3>");
	IDParam versionFmtId(MSG_UPDATE_HEADER_VERSION);
	const wchar_t *versionFmt = patchChkGetString(versionFmtId);
	wstring versionLine = shellFormatText(versionFmt, versionWide.c_str());
	string versionLineUtf8;
	common::Utf16ToUtf8(versionLine, &versionLineUtf8);
	outHtml->append(versionLineUtf8);
	outHtml->append("</h3><br/>");
}

/* Transitions into the specified PatchChkUiState and opens its UI (0x812ACC1E) */
void PatchChkController::EnterUiState(SceUInt16 state)
{
	m_uiState = state;
	if (m_stopping)
		return;

	switch (state) {
	case PATCH_CHK_STATE_CHECKING:
		if (m_checkMode == 1)
			patchChkGetPlugin()->Notify(1);
		else
			m_ui->ShowCheckingDialog();
		break;

	case PATCH_CHK_STATE_SYSUPDATE_PROMPT:
		m_ui->ShowSysUpdatePromptDialog();
		break;

	case PATCH_CHK_STATE_UP_TO_DATE:
		m_ui->ShowUpToDateDialog();
		break;

	case PATCH_CHK_STATE_UPDATE_AVAILABLE:
		if (m_checkMode != 0) {
			m_ui->ShowQuietUpdateIconDialog();
		} else {
			string html;
			BuildUpdateHeaderHtml(&html, m_runner->GetTotalPackageSize(),
					      m_runner->GetLatestVersionString());
			if (m_changeInfo != NULL)
				m_changeInfo->AppendFormattedHtml(&html);

			int err = m_ui->ShowUpdateAvailableDialog(html,
								  m_runner->HasExistingDownload());
			if (err != 0) {
				m_lastError = err;
				IDParam defaultErrId(MSG_DEFAULT_ERROR_SENTINEL);
				m_ui->ShowErrorCodeDialog(defaultErrId, err);
				m_uiState = PATCH_CHK_STATE_ERROR_DIALOG;
			}
			if (m_changeInfo != NULL) {
				delete m_changeInfo;
				m_changeInfo = NULL;
			}
		}
		break;

	case PATCH_CHK_STATE_DEBUG_PATCH_TEST:
		m_ui->ShowDebugPatchTestDialog();
		break;

	case PATCH_CHK_STATE_DISCONTINUED:
		m_ui->ShowDiscontinuedDialog();
		break;

	case PATCH_CHK_STATE_ALREADY_DOWNLOADING:
		if (m_checkMode != 0)
			m_ui->ShowQuietDownloadingIconDialog();
		else
			m_ui->ShowAlreadyDownloadingDialog();
		break;

	case PATCH_CHK_STATE_READY_TO_INSTALL:
		if (m_checkMode != 0) {
			IDParam msgId(MSG_QUIET_INSTALL_PROMPT);
			m_ui->ShowYesNoPromptDialog(msgId);
		} else if (patchChkIsAppRunning(m_startParam.titleId)) {
			IDParam msgId(MSG_CLOSE_APP_TO_INSTALL);
			m_ui->ShowYesNoPromptDialog(msgId);
		} else {
			Plugin *topmenu = Plugin::Find("topmenu_plugin");
			ShellPluginInterface *iface =
				(topmenu != NULL) ? (ShellPluginInterface *)topmenu->GetInterface(1)
						  : NULL;
			TopmenuLaunchUpdateFunc launchFunc =
				(iface != NULL) ? (TopmenuLaunchUpdateFunc)iface->func[65] : NULL;
			if (launchFunc == NULL ||
			    launchFunc(m_startParam.titleId, OnTopmenuLaunchDone, this) == 0)
				FinishAndNotify();
		}
		break;

	case PATCH_CHK_STATE_BGDL_TASK_LIMIT:
		m_ui->ShowBgdlTaskLimitDialog();
		break;

	case PATCH_CHK_STATE_BGDL_NO_MEMORY_CARD:
		m_ui->ShowBgdlNoMemoryCardDialog();
		break;

	case PATCH_CHK_STATE_BGDL_NO_FREE_SPACE:
		m_ui->ShowBgdlNoFreeSpaceDialog(m_runner->GetRequiredFreeSpace());
		break;

	case PATCH_CHK_STATE_BGDL_ALREADY_QUEUED:
		m_ui->ShowBgdlAlreadyQueuedDialog();
		break;

	case PATCH_CHK_STATE_BGDL_GENERIC_ERROR:
		m_ui->ShowBgdlGenericErrorDialog();
		break;

	case PATCH_CHK_STATE_BGDL_NOT_SUPPORTED:
		m_ui->ShowBgdlNotSupportedDialog();
		break;

	case PATCH_CHK_STATE_ERROR_DIALOG: {
		IDParam defaultErrId(MSG_DEFAULT_ERROR_SENTINEL);
		m_ui->ShowErrorCodeDialog(defaultErrId, m_lastError);
		break;
	}

	default:
		break;
	}
}

/* Handles a caller-supplied preset SceGameUpdateResult::status in quiet mode (0x812ACF84) */
void PatchChkController::HandlePresetStatus(SceInt32 status)
{
	switch (status) {
	case SCE_GAMEUPDATE_STATUS_DOWNLOADING:
		patchChkGetPlugin()->Notify(4);
		EnterUiState(PATCH_CHK_STATE_ALREADY_DOWNLOADING);
		break;

	case SCE_GAMEUPDATE_STATUS_AVAILABLE:
	case SCE_GAMEUPDATE_STATUS_READY_TO_INSTALL:
		patchChkGetPlugin()->Notify(4);
		EnterUiState(PATCH_CHK_STATE_UPDATE_AVAILABLE);
		break;

	case SCE_GAMEUPDATE_STATUS_DISCONTINUED:
		m_lastError = 0;
		FinishAndNotify();
		break;

	case SCE_GAMEUPDATE_STATUS_SYSUPDATE_NEEDED:
		patchChkGetPlugin()->Notify(4);
		m_sysUpChk = new PatchChkSysUpChk();
		if (m_sysUpChk != NULL)
			m_sysUpChk->Launch(m_checkMode, 1, OnSysUpChkDone, this);
		break;

	case SCE_GAMEUPDATE_STATUS_UP_TO_DATE:
	default:
		EnterUiState(PATCH_CHK_STATE_CHECKING);
		break;
	}
}

/* Public entry point: copies parameters and starts check flow (0x812AD03A) */
int PatchChkController::StartCheck(const PatchChkStartParam &param,
				   PatchChkFinishCallback finishCallback, void *userData)
{
	sce_paf_memcpy(&m_startParam, &param, sizeof(m_startParam));
	int err = InitializeSession(param.checkMode, finishCallback, userData);
	if (err != 0) {
		m_lastError = err;
		EnterUiState(PATCH_CHK_STATE_ERROR_DIALOG);
	} else if (param.checkMode == 1 && param.presetStatus != -1) {
		HandlePresetStatus(param.presetStatus);
	} else {
		EnterUiState(PATCH_CHK_STATE_CHECKING);
	}
	return 0;
}

/* Registers state callback (0x812AD0BE) */
int PatchChkController::SetStateCallback(PatchChkStateCallback callback, void *userData)
{
	if (callback == NULL)
		return PATCH_CHK_ERROR_INVALID_ARGUMENT;
	m_stateCallback = callback;
	m_stateUserData = userData;
	return 0;
}

/* Clears state callback (0x812AD0D8) */
void PatchChkController::ClearStateCallback()
{
	m_stateCallback = NULL;
	m_stateUserData = NULL;
}

/* External cancel request (0x812AD0F4) */
void PatchChkController::Cancel()
{
	m_lastError = PATCH_CHK_ERROR_EXTERNAL_CANCELED;
	if (!m_runner->IsUpdateIdle()) {
		m_runner->CancelUpdateJob();
		return;
	}
	if (!m_runner->IsDownloadIdle()) {
		m_runner->CancelDownloadJob();
		return;
	}
	if (m_changeInfo != NULL) {
		m_changeInfo->Cancel();
		return;
	}
	FinishAndNotify();
}

/* Starts minimum display timer so the checking dialog does not flicker (0x812AD13A) */
int PatchChkController::StartMinDisplayTimer(int timerState)
{
	ui::Widget *target = (m_checkMode == 1) ? m_ui->GetOverlayRootWidget()
						: m_ui->GetDialogContentWidget();
	if (target == NULL)
		return -1;

	m_timerState = timerState;
	if (timerState == 1) {
		return target->SetTimerListener(
			0x01000002, 1000.0f,
			new ui::EventCBListener(OnMinDisplayTimerFired, this));
	}
	return -1;
}

/* Cancels the minimum display timer if armed (0x812ADDEC) */
void PatchChkController::StopMinDisplayTimer()
{
	ui::Widget *target = (m_checkMode == 1) ? m_ui->GetOverlayRootWidget()
						: m_ui->GetDialogContentWidget();
	if (target != NULL) {
		if (m_timerState == 1)
			target->KillTimer(0x01000002);
		m_timerState = 0;
	}
}

/* Builds SceGameUpdateInitParam and starts async update check (0x812AD1C6) */
int PatchChkController::BeginGameUpdateCheck()
{
	SceGameUpdateInitParam initParam;
	sce_paf_memset(&initParam, 0, sizeof(initParam));

	if (sce_paf_strlen(m_startParam.patchTitleId) != 0)
		sce_paf_memcpy(initParam.patchTitleId, m_startParam.patchTitleId, 13);
	sce_paf_memcpy(initParam.titleId, m_startParam.titleId, 13);
	sce_paf_memcpy(initParam.currentAppVer, m_startParam.currentAppVer, 6);
	sce_paf_memcpy(initParam.contentId, m_startParam.contentId, 48);

	if (m_startParam.targetPlatform == 1)
		initParam.targetPlatform = SCE_GAMEUPDATE_PLATFORM_PSP2;
	else if (m_startParam.targetPlatform == 2)
		initParam.targetPlatform = SCE_GAMEUPDATE_PLATFORM_PSP;
	else
		initParam.targetPlatform = SCE_GAMEUPDATE_PLATFORM_DEFAULT;

	initParam.currentPsp2SysVer = m_startParam.currentPsp2SysVer;
	initParam.currentPspSysVer = m_startParam.currentPspSysVer;
	initParam.allocFunc = sce_paf_malloc;
	initParam.freeFunc = sce_paf_free;

	int err = m_runner->StartUpdateCheck(&initParam);
	if (err == 0 && m_stateCallback != NULL)
		m_stateCallback(1, m_stateUserData);
	return err;
}

/* Populates BgdlPatchDownloadTaskParam from runner results and starts BGDL job (0x812AD290) */
int PatchChkController::BeginPatchDownloadTask()
{
	const SceGameUpdateInfo *info = m_runner->GetUpdateInfo();
	if (info == NULL)
		return PATCH_CHK_ERROR_NO_UPDATE_INFO;

	const SceGameUpdateResult *result = m_runner->GetUpdateResult();
	if (result == NULL)
		return PATCH_CHK_ERROR_NO_UPDATE_INFO;

	BgdlPatchPackageEntry pkgEntry;
	for (SceUInt32 i = 0; i < info->packageCount; ++i) {
		shellBgdlPatchPackageEntryClear(&pkgEntry);
		SceGameUpdatePackageInfo pkgInfo;
		sce_paf_memset(&pkgInfo, 0, sizeof(pkgInfo));
		m_runner->CopyPackageInfo(i, &pkgInfo);

		pkgEntry.size = pkgInfo.packageSize;
		sce_paf_memcpy(pkgEntry.sha1Digest, pkgInfo.sha1Digest, 20);
		pkgEntry.url = pkgInfo.packageUrl;
		m_downloadParam.packages.push_back(pkgEntry);
	}

	if (sce_paf_strlen(m_startParam.patchTitleId) != 0)
		m_downloadParam.patchTitleId = m_startParam.patchTitleId;
	m_downloadParam.titleId = m_startParam.titleId;
	m_downloadParam.contentId = m_startParam.contentId;

	char verBuf[8];
	sce_paf_memcpy(verBuf, result->latestVersion, sizeof(verBuf));
	patchChkParseVersionShort(verBuf, 6, &m_downloadParam.versionMajor,
				  &m_downloadParam.versionMinor);

	m_downloadParam.packagePlatform = (m_startParam.targetPlatform == 2) ? 1u : 0u;
	m_downloadParam.titleName = info->titleName;
	m_downloadParam.iconPath.clear();

	return m_runner->StartDownloadTask(m_downloadParam);
}

/* Handles user confirmation on the active UI dialog (0x812AD53C) */
void PatchChkController::HandleUiConfirm()
{
	switch (m_uiState) {
	case PATCH_CHK_STATE_SYSUPDATE_PROMPT: {
		m_sysUpChk = new PatchChkSysUpChk();
		int err = PATCH_CHK_ERROR_NO_MEMORY;
		if (m_sysUpChk != NULL)
			err = m_sysUpChk->Launch(m_checkMode, 1, OnSysUpChkDone, this);
		if (err != 0) {
			m_lastError = err;
			EnterUiState(PATCH_CHK_STATE_ERROR_DIALOG);
		}
		break;
	}

	case PATCH_CHK_STATE_UP_TO_DATE:
	case PATCH_CHK_STATE_DISCONTINUED:
	case PATCH_CHK_STATE_BGDL_TASK_LIMIT:
	case PATCH_CHK_STATE_BGDL_NO_MEMORY_CARD:
	case PATCH_CHK_STATE_BGDL_NO_FREE_SPACE:
	case PATCH_CHK_STATE_BGDL_ALREADY_QUEUED:
	case PATCH_CHK_STATE_BGDL_GENERIC_ERROR:
		m_ui->CloseCurrentDialog();
		m_uiState = PATCH_CHK_STATE_IDLE;
		FinishAndNotify();
		break;

	case PATCH_CHK_STATE_UPDATE_AVAILABLE:
		m_ui->CloseCurrentDialog();
		m_uiState = PATCH_CHK_STATE_IDLE;
		if (m_checkMode == 1) {
			m_lastError = 0;
			FinishAndNotify();
		} else {
			int err = BeginPatchDownloadTask();
			if (err != 0) {
				m_lastError = err;
				EnterUiState(PATCH_CHK_STATE_ERROR_DIALOG);
			}
		}
		break;

	case PATCH_CHK_STATE_DEBUG_PATCH_TEST:
		m_lastError = 0;
		FinishAndNotify();
		break;

	case PATCH_CHK_STATE_ALREADY_DOWNLOADING:
		m_ui->CloseCurrentDialog();
		m_uiState = PATCH_CHK_STATE_IDLE;
		m_lastError = 0;
		FinishAndNotify();
		break;

	case PATCH_CHK_STATE_READY_TO_INSTALL: {
		m_ui->CloseCurrentDialog();
		m_uiState = PATCH_CHK_STATE_IDLE;

		int err = 0;
		Plugin *topmenu = Plugin::Find("topmenu_plugin");
		ShellPluginInterface *iface =
			(topmenu != NULL) ? (ShellPluginInterface *)topmenu->GetInterface(1)
					  : NULL;
		TopmenuLaunchUpdateFunc launchFunc =
			(iface != NULL) ? (TopmenuLaunchUpdateFunc)iface->func[65] : NULL;
		if (launchFunc == NULL ||
		    launchFunc(m_startParam.titleId, OnTopmenuLaunchDone, this) == 0)
			err = PATCH_CHK_ERROR_NO_MEMORY;

		if (err != 0) {
			FinishAndNotify();
			m_lastError = err;
			EnterUiState(PATCH_CHK_STATE_ERROR_DIALOG);
		}
		break;
	}

	case PATCH_CHK_STATE_ERROR_DIALOG:
		FinishAndNotify();
		break;

	default:
		if (m_uiState == PATCH_CHK_STATE_CHECKING && m_checkMode == 1)
			patchChkGetPlugin()->Notify(2);
		else
			m_ui->CloseCurrentDialog();
		m_uiState = PATCH_CHK_STATE_IDLE;
		FinishAndNotify();
		break;
	}
}

/* Dispatches UI confirm/cancel/opened events (0x812AD898) */
void PatchChkController::HandleUiEvent(int eventId)
{
	if (m_stopping)
		return;

	if (eventId == PATCH_CHK_UI_EVENT_CONFIRM) {
		HandleUiConfirm();
	} else if (eventId == PATCH_CHK_UI_EVENT_CANCEL) {
		m_lastError = PATCH_CHK_ERROR_USER_CANCELED;
		if (m_uiState != PATCH_CHK_STATE_CHECKING) {
			FinishAndNotify();
			return;
		}
		if (!m_runner->IsUpdateIdle()) {
			m_runner->CancelUpdateJob();
			return;
		}
		if (!m_runner->IsDownloadIdle()) {
			m_runner->CancelDownloadJob();
			return;
		}
		if (m_changeInfo != NULL) {
			m_changeInfo->Cancel();
			return;
		}
		FinishAndNotify();
	} else if (eventId == PATCH_CHK_UI_EVENT_OPENED) {
		int err = BeginGameUpdateCheck();
		if (err != 0) {
			m_lastError = err;
			EnterUiState(PATCH_CHK_STATE_ERROR_DIALOG);
		} else {
			m_updateFinishedPendingTimer = false;
			StartMinDisplayTimer(1);
		}
	}
}

/* External event forwarding hook (0x812AD944) */
void PatchChkController::HandleEvent(int eventId)
{
	if (eventId == 3)
		HandleUiEvent(PATCH_CHK_UI_EVENT_CANCEL);
	else if (eventId == 1)
		HandleUiEvent(PATCH_CHK_UI_EVENT_OPENED);
}

/* Static trampoline for PatchChkUi callbacks (0x812AD95E) */
void PatchChkController::OnUiEvent(int eventId, void *userData)
{
	PatchChkController *self = (PatchChkController *)userData;
	if (self != NULL)
		self->HandleUiEvent(eventId);
}

/* Completion callback from PatchChkSysUpChk (0x812AD96E) */
void PatchChkController::OnSysUpChkDone(int result, void *userData)
{
	(void)result;
	PatchChkController *self = (PatchChkController *)userData;
	if (self != NULL) {
		self->m_lastError = PATCH_CHK_ERROR_USER_CANCELED;
		self->FinishAndNotify();
	}
}

/* Completion callback from PatchChkChangeInfoController (0x812AD986) */
void PatchChkController::OnChangeInfoDone(int result, void *userData)
{
	PatchChkController *self = (PatchChkController *)userData;
	if (self == NULL || self->m_stopping)
		return;

	if (self->m_lastError == PATCH_CHK_ERROR_USER_CANCELED ||
	    self->m_lastError == PATCH_CHK_ERROR_EXTERNAL_CANCELED) {
		self->FinishAndNotify();
		return;
	}

	if (self->m_uiState == PATCH_CHK_STATE_CHECKING && self->m_checkMode == 1)
		patchChkGetPlugin()->Notify(2);
	else
		self->m_ui->CloseCurrentDialog();
	self->m_uiState = PATCH_CHK_STATE_IDLE;

	if (result != 0) {
		self->m_lastError = result;
		self->EnterUiState(PATCH_CHK_STATE_ERROR_DIALOG);
	} else {
		self->EnterUiState(PATCH_CHK_STATE_UPDATE_AVAILABLE);
	}
}

/* Processes the completed SceGameUpdate check once min display timer allows (0x812ADA04) */
void PatchChkController::ProcessUpdateResult(int result)
{
	(void)result;
	if (m_stateCallback != NULL)
		m_stateCallback(0, m_stateUserData);

	m_updateFinishedPendingTimer = false;
	if (m_lastError == PATCH_CHK_ERROR_USER_CANCELED ||
	    m_lastError == PATCH_CHK_ERROR_EXTERNAL_CANCELED) {
		FinishAndNotify();
		return;
	}

	int updateErr = m_runner->GetLastUpdateError();
	if (updateErr < 0) {
		if (m_uiState == PATCH_CHK_STATE_CHECKING && m_checkMode == 1)
			patchChkGetPlugin()->Notify(2);
		else
			m_ui->CloseCurrentDialog();
		m_lastError = updateErr;
		m_uiState = PATCH_CHK_STATE_IDLE;
		EnterUiState(PATCH_CHK_STATE_ERROR_DIALOG);
		return;
	}

	const SceGameUpdateResult *updRes = m_runner->GetUpdateResult();
	const SceGameUpdateInfo *updInfo = m_runner->GetUpdateInfo();
	m_updateStatus = updRes->status;
	m_infoFlags = updInfo->flags;

	if (m_updateStatus != SCE_GAMEUPDATE_STATUS_AVAILABLE) {
		switch (m_updateStatus) {
		case SCE_GAMEUPDATE_STATUS_UP_TO_DATE:
			if (m_checkMode != 1) {
				m_ui->CloseCurrentDialog();
				m_uiState = PATCH_CHK_STATE_IDLE;
				EnterUiState(PATCH_CHK_STATE_UP_TO_DATE);
				return;
			}
			break;

		case SCE_GAMEUPDATE_STATUS_SYSUPDATE_NEEDED:
			if (m_uiState == PATCH_CHK_STATE_CHECKING && m_checkMode == 1)
				patchChkGetPlugin()->Notify(2);
			else
				m_ui->CloseCurrentDialog();
			m_uiState = PATCH_CHK_STATE_IDLE;
			m_sysUpChk = new PatchChkSysUpChk();
			if (m_sysUpChk != NULL)
				m_sysUpChk->Launch(m_checkMode, 1, OnSysUpChkDone, this);
			return;

		case SCE_GAMEUPDATE_STATUS_DISCONTINUED:
			if (m_checkMode != 1) {
				m_ui->CloseCurrentDialog();
				m_uiState = PATCH_CHK_STATE_IDLE;
				EnterUiState(PATCH_CHK_STATE_DISCONTINUED);
				return;
			}
			break;

		case SCE_GAMEUPDATE_STATUS_DOWNLOADING:
			if (m_uiState == PATCH_CHK_STATE_CHECKING && m_checkMode == 1)
				patchChkGetPlugin()->Notify(2);
			else
				m_ui->CloseCurrentDialog();
			m_uiState = PATCH_CHK_STATE_IDLE;
			EnterUiState(PATCH_CHK_STATE_ALREADY_DOWNLOADING);
			return;

		case SCE_GAMEUPDATE_STATUS_READY_TO_INSTALL:
			if (m_uiState == PATCH_CHK_STATE_CHECKING && m_checkMode == 1)
				patchChkGetPlugin()->Notify(2);
			else
				m_ui->CloseCurrentDialog();
			m_uiState = PATCH_CHK_STATE_IDLE;
			if (m_checkMode == 1)
				EnterUiState(PATCH_CHK_STATE_UPDATE_AVAILABLE);
			else
				EnterUiState(PATCH_CHK_STATE_READY_TO_INSTALL);
			return;

		default:
			return;
		}

		m_lastError = 0;
		FinishAndNotify();
		return;
	}

	if (m_checkMode == 1) {
		if (m_uiState == PATCH_CHK_STATE_CHECKING)
			patchChkGetPlugin()->Notify(2);
		else
			m_ui->CloseCurrentDialog();
		m_uiState = PATCH_CHK_STATE_IDLE;
		EnterUiState(PATCH_CHK_STATE_UPDATE_AVAILABLE);
		return;
	}

	if ((m_infoFlags & SCE_GAMEUPDATE_INFO_FLAG_DEBUG_MOCK) != 0) {
		m_ui->CloseCurrentDialog();
		m_uiState = PATCH_CHK_STATE_IDLE;
		EnterUiState(PATCH_CHK_STATE_DEBUG_PATCH_TEST);
		return;
	}

	SceUInt64 totalSize = m_runner->GetTotalPackageSize();
	m_changeInfo = new PatchChkChangeInfoController();
	int err = PATCH_CHK_ERROR_NO_MEMORY;
	if (m_changeInfo != NULL) {
		string changeInfoUrl(updInfo->changeInfoUrl);
		err = m_changeInfo->Start(OnChangeInfoDone, m_startParam.currentAppVer,
					  changeInfoUrl, totalSize, this);
	}
	if (err < 0) {
		if (m_uiState == PATCH_CHK_STATE_CHECKING && m_checkMode == 1)
			patchChkGetPlugin()->Notify(2);
		else
			m_ui->CloseCurrentDialog();
		m_lastError = err;
		m_uiState = PATCH_CHK_STATE_IDLE;
		EnterUiState(PATCH_CHK_STATE_ERROR_DIALOG);
	}
}

/* Completion callback from PatchChkGameUpdateRunner update check (0x812ADE66) */
void PatchChkController::OnUpdateRunnerDone(int result, void *userData)
{
	PatchChkController *self = (PatchChkController *)userData;
	self->m_updateFinishedPendingTimer = true;
	self->m_deferredUpdateResult = result;

	if (self->m_runner->GetLastUpdateError() < 0) {
		self->StopMinDisplayTimer();
		self->ProcessUpdateResult(result);
		self->m_deferredUpdateResult = -1;
		return;
	}

	ui::Widget *target = (self->m_checkMode == 1) ? self->m_ui->GetOverlayRootWidget()
						      : self->m_ui->GetDialogContentWidget();
	if (target == NULL || self->m_timerState != 1) {
		self->ProcessUpdateResult(result);
		self->m_deferredUpdateResult = -1;
	}
}

/* Completion callback from PatchChkGameUpdateRunner BGDL download task (0x812ADF14) */
void PatchChkController::OnDownloadRunnerDone(int result, void *userData)
{
	PatchChkController *self = (PatchChkController *)userData;
	if (self->m_lastError == PATCH_CHK_ERROR_USER_CANCELED ||
	    self->m_lastError == PATCH_CHK_ERROR_EXTERNAL_CANCELED) {
		self->FinishAndNotify();
		return;
	}

	self->m_lastError = result;
	if (self->m_checkMode != 0 || result >= 0) {
		self->FinishAndNotify();
		return;
	}

	switch (result) {
	case PATCH_CHK_BGDL_ERR_NO_FREE_SPACE:
		self->EnterUiState(PATCH_CHK_STATE_BGDL_NO_FREE_SPACE);
		break;
	case PATCH_CHK_BGDL_ERR_NO_MEMORY_CARD:
		self->EnterUiState(PATCH_CHK_STATE_BGDL_NO_MEMORY_CARD);
		break;
	case PATCH_CHK_BGDL_ERR_TASK_LIMIT:
		self->EnterUiState(PATCH_CHK_STATE_BGDL_TASK_LIMIT);
		break;
	case PATCH_CHK_BGDL_ERR_ALREADY_QUEUED:
		self->EnterUiState(PATCH_CHK_STATE_BGDL_ALREADY_QUEUED);
		break;
	case PATCH_CHK_BGDL_ERR_NOT_SUPPORTED:
		self->EnterUiState(PATCH_CHK_STATE_BGDL_NOT_SUPPORTED);
		break;
	default:
		self->EnterUiState(PATCH_CHK_STATE_BGDL_GENERIC_ERROR);
		break;
	}
}

/* Completion callback from topmenu_plugin update installer handoff (0x812ADFC6) */
void PatchChkController::OnTopmenuLaunchDone(int result, void *userData)
{
	PatchChkController *self = (PatchChkController *)userData;
	self->m_lastError = result;
	self->FinishAndNotify();
}

/* Timer callback firing after minimum checking dialog display time (0x812ADFD6) */
void PatchChkController::OnMinDisplayTimerFired(SceInt32 eventType, ui::Handler *handler,
						ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	PatchChkController *self = (PatchChkController *)userData;
	if (self != NULL && self->m_updateFinishedPendingTimer) {
		self->ProcessUpdateResult(self->m_deferredUpdateResult);
		self->m_deferredUpdateResult = -1;
	}
}
