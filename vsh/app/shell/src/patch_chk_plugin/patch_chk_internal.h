/*
 * patch_chk_internal.h: internal classes, structures, error codes, and helpers
 * shared across patch_chk_plugin's translation units (0x812AB958-0x812B1B63).
 */
#ifndef SHELL_PATCH_CHK_INTERNAL_H
#define SHELL_PATCH_CHK_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_busyindicator.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "game_update.h"
#include "patch_chk_plugin.h"

/* patch_chk_plugin internal error codes (0x801043xx) */
#define PATCH_CHK_ERROR_NO_MEMORY		(-2146417920)	/* 0x80104300 */
#define PATCH_CHK_ERROR_USER_CANCELED		(-2146417919)	/* 0x80104301 */
#define PATCH_CHK_ERROR_EXTERNAL_CANCELED	(-2146417918)	/* 0x80104302 */
#define PATCH_CHK_ERROR_INVALID_ARGUMENT	(-2146417917)	/* 0x80104303 */
#define PATCH_CHK_ERROR_NO_UPDATE_INFO		(-2146417916)	/* 0x80104304 */
#define PATCH_CHK_ERROR_XML_INIT_FAILED		(-2146417915)	/* 0x80104305 */
#define PATCH_CHK_ERROR_VERSION_OVERFLOW	(-2146417914)	/* 0x80104306 */

/* BGDL task creation errors mapped to specific dialogs (0x80103Axx) */
#define PATCH_CHK_BGDL_ERR_NO_FREE_SPACE	(-2146420223)	/* 0x80103A01 */
#define PATCH_CHK_BGDL_ERR_NO_MEMORY_CARD	(-2146420222)	/* 0x80103A02 */
#define PATCH_CHK_BGDL_ERR_TASK_LIMIT		(-2146420221)	/* 0x80103A03 */
#define PATCH_CHK_BGDL_ERR_ALREADY_QUEUED	(-2146420199)	/* 0x80103A19 */
#define PATCH_CHK_BGDL_ERR_NOT_SUPPORTED	(-2146420153)	/* 0x80103A47 */

/* Job queue indices owned by PatchChkJobQueueMgr */
enum PatchChkQueueIndex {
	PATCH_CHK_QUEUE_ROOT         = 0,
	PATCH_CHK_QUEUE_DEFAULT      = 1,
	PATCH_CHK_QUEUE_GAME_UPDATE  = 2,
	PATCH_CHK_QUEUE_CREATE_TASK  = 3,
	PATCH_CHK_QUEUE_GAME_OPERATE = 4,
	PATCH_CHK_QUEUE_COUNT        = 5
};

/* PatchChkUi notification event kinds passed to PatchChkController */
enum PatchChkUiEvent {
	PATCH_CHK_UI_EVENT_CONFIRM = 0,
	PATCH_CHK_UI_EVENT_CANCEL  = 1,
	PATCH_CHK_UI_EVENT_OPENED  = 2
};

/* PatchChkController dialog / UI state machine values */
enum PatchChkUiState {
	PATCH_CHK_STATE_IDLE                = 0,
	PATCH_CHK_STATE_CHECKING            = 1,
	PATCH_CHK_STATE_SYSUPDATE_PROMPT    = 2,
	PATCH_CHK_STATE_UP_TO_DATE          = 3,
	PATCH_CHK_STATE_UPDATE_AVAILABLE    = 4,
	PATCH_CHK_STATE_DEBUG_PATCH_TEST    = 5,
	PATCH_CHK_STATE_DISCONTINUED        = 6,
	PATCH_CHK_STATE_ALREADY_DOWNLOADING = 7,
	PATCH_CHK_STATE_READY_TO_INSTALL    = 8,
	PATCH_CHK_STATE_BGDL_TASK_LIMIT     = 10,
	PATCH_CHK_STATE_BGDL_NO_MEMORY_CARD = 11,
	PATCH_CHK_STATE_BGDL_NO_FREE_SPACE  = 12,
	PATCH_CHK_STATE_BGDL_ALREADY_QUEUED = 13,
	PATCH_CHK_STATE_BGDL_GENERIC_ERROR  = 14,
	PATCH_CHK_STATE_BGDL_NOT_SUPPORTED  = 15,
	PATCH_CHK_STATE_ERROR_DIALOG        = 16
};

/* Single package entry passed into BGDL task creation (48 bytes, 0x813C85FA) */
struct BgdlPatchPackageEntry {
	paf::string url;
	SceUInt32 alignmentPad;
	SceUInt64 size;
	SceUInt8 sha1Digest[20];
	SceUInt32 trailingPad;
};

/* Full BGDL patch download task request (120 bytes, 0x813C861C / 0x813C8ADC) */
struct BgdlPatchDownloadTaskParam {
	SceUInt32 flags;
	SceUInt16 versionMajor;
	SceUInt16 versionMinor;
	paf::string titleId;
	paf::string patchTitleId;
	paf::string contentId;
	paf::string reservedText;
	paf::string titleName;
	paf::string iconPath;
	paf::vector<BgdlPatchPackageEntry> packages;
	SceUInt32 reservedWords[2];
	SceUInt32 packagePlatform;
	SceUInt32 alignmentPad;
	SceUInt64 requiredFreeSpace;
};

typedef char BgdlPatchPackageEntrySizeCheck[sizeof(BgdlPatchPackageEntry) == 48 ? 1 : -1];
typedef char BgdlPatchDownloadTaskParamSizeCheck[sizeof(BgdlPatchDownloadTaskParam) == 120 ? 1 : -1];

/* External BGDL client helpers (0x813C85FA, 0x813C861C, 0x813C8ADC, 0x813C8F76) */
extern void shellBgdlPatchPackageEntryClear(BgdlPatchPackageEntry *entry);
extern int shellBgdlPatchDownloadTaskParamClear(BgdlPatchDownloadTaskParam *param);
extern int shellBgdlCreatePatchDownloadTask(BgdlPatchDownloadTaskParam *param,
					    SceUInt32 *outTaskId);
extern int shellBgdlIsTitleDownloadingOrInstalled(const char *titleId, bool *outFound);

/* Utility helpers in patch_chk_util.cpp (0x812ABBD4-0x812AC3A7) */
void patchChkSetPlugin(paf::Plugin *plugin);
void patchChkClearPlugin(void);
paf::Plugin *patchChkGetPlugin(void);
const wchar_t *patchChkGetString(const paf::IDParam &id);
paf::ui::Widget *patchChkGetDialogPart(int dialogId, int partIndex);
void patchChkFormatSizeParen(paf::wstring *outText, SceUInt64 sizeBytes);
int patchChkShowLayoutDialog(const paf::wstring &text, const CommonGuiDialogLayout *layout,
			     CommonGuiDialogHandler *handler);
void patchChkCloseDialog(int dialogId);
int patchChkShowYesNoDialog(const paf::IDParam &textId, CommonGuiDialogHandler *handler);
int patchChkShowOkDialog(const paf::IDParam &textId, CommonGuiDialogHandler *handler);
int patchChkShowCancelDialog(const paf::IDParam &textId, CommonGuiDialogHandler *handler);
int patchChkShowErrorDialog(const paf::IDParam &messageId, int errorCode,
			    CommonGuiDialogHandler *handler,
			    CommonGuiMessageDialogOpenHandler *openHandler);
int patchChkShowIconDialog(const paf::IDParam &titleId, const paf::IDParam &textId,
			   const paf::IDParam &iconId, const paf::IDParam &badgeIconId,
			   CommonGuiDialogHandler *handler);
int patchChkSetDialogOpenCallback(int dialogId, CommonGuiDialogOpenFunction func, void *userData);
bool patchChkIsAppRunning(const char *titleId);
int patchChkParseVersionShort(char *versionStr, SceSize maxLen,
			      SceUInt16 *outMajor, SceUInt16 *outMinor);

/* Singleton manager owning patch_chk_plugin's 5 job queues (24 bytes, 0x812AE696) */
class PatchChkJobQueueMgr {
public:
	PatchChkJobQueueMgr();
	virtual ~PatchChkJobQueueMgr();

	static PatchChkJobQueueMgr *Create(void);
	static void Destroy(void);
	static PatchChkJobQueueMgr *GetInstance(void);

	void InitializeQueues(void);
	paf::job::JobQueue *GetQueue(int queueIndex);

private:
	paf::job::JobQueue *m_queues[PATCH_CHK_QUEUE_COUNT];
};

/* Generic callback-driven job item used by ChangeInfoController (44 bytes, 0x81431C1C) */
class PatchChkCallbackJob : public paf::job::JobItem {
public:
	typedef int (*RunCallback)(void *userData);
	typedef void (*FinishCallback)(int result, void *userData);
	typedef void (*CancelCallback)(void);

	PatchChkCallbackJob(const char *name, RunCallback runCb, FinishCallback finishCb,
			    CancelCallback cancelCb, void *userData);
	virtual ~PatchChkCallbackJob();
	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);

private:
	RunCallback m_runCallback;
	FinishCallback m_finishCallback;
	CancelCallback m_cancelCallback;
	void *m_userData;
};

/* Background job calling shellBgdlCreatePatchDownloadTask (176 bytes, 0x812B15F4) */
class PatchChkDownloaderTaskJob : public paf::job::JobItem {
public:
	typedef void (*CompletionCallback)(int result, int unused, SceUInt64 requiredFreeSpace,
					   void *userData);

	PatchChkDownloaderTaskJob(const BgdlPatchDownloadTaskParam &param,
				  CompletionCallback callback, void *userData);
	virtual ~PatchChkDownloaderTaskJob();
	virtual int Run();
	virtual void Finish(int result);

	void DetachAndCancel();

private:
	void *m_userData;
	bool m_notifyOnFinish;
	BgdlPatchDownloadTaskParam m_param;
	SceUInt64 m_requiredFreeSpace;
	CompletionCallback m_callback;
};

/* Background job calling sceGameUpdateRunEx & collecting packages (512 bytes, 0x812B1890) */
class PatchChkGameUpdateJob : public paf::job::JobItem {
public:
	typedef void (*CompletionCallback)(int result, const SceGameUpdateResult *updateResult,
					   const SceGameUpdateInfo *updateInfo,
					   SceUInt64 totalSize, bool hasExistingDownload,
					   void *userData);

	PatchChkGameUpdateJob(SceGameUpdateContext *context, const paf::string &titleId,
			      CompletionCallback callback, void *userData);
	virtual ~PatchChkGameUpdateJob();
	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);

	void DetachAndCancel();
	SceUInt32 GetPackageCount() const;
	int CopyPackageInfo(SceUInt32 index, SceGameUpdatePackageInfo *outInfo) const;

private:
	void *m_userData;
	bool m_notifyOnFinish;
	SceGameUpdateResult m_updateResult;
	SceGameUpdateInfo m_updateInfo;
	paf::vector<SceGameUpdatePackageInfo> m_packages;
	SceUInt64 m_totalSize;
	paf::string m_titleId;
	bool m_hasExistingDownload;
	CompletionCallback m_callback;
	SceGameUpdateContext *m_context;
};

/* Async runner coordinating GameUpdateJob and DownloaderTaskJob (536 bytes, 0x812AE068) */
class PatchChkGameUpdateRunner {
public:
	typedef void (*UpdateDoneCallback)(int result, void *userData);
	typedef void (*DownloadDoneCallback)(int result, void *userData);

	PatchChkGameUpdateRunner();
	~PatchChkGameUpdateRunner();

	int StartUpdateCheck(const SceGameUpdateInitParam *param);
	int DetachUpdateJob();
	int CancelUpdateJob();
	int StartDownloadTask(const BgdlPatchDownloadTaskParam &param);
	int DetachDownloadJob();
	int CancelDownloadJob();

	int SetUpdateDoneCallback(UpdateDoneCallback callback, void *userData);
	int SetDownloadDoneCallback(DownloadDoneCallback callback, void *userData);

	SceInt32 GetLastUpdateError() const { return m_lastUpdateError; }
	const SceGameUpdateResult *GetUpdateResult() const { return &m_updateResult; }
	const SceGameUpdateInfo *GetUpdateInfo() const { return &m_updateInfo; }
	const char *GetLatestVersionString() const { return m_updateResult.latestVersion; }
	SceUInt64 GetTotalPackageSize() const { return m_totalPackageSize; }
	SceUInt64 GetRequiredFreeSpace() const { return m_requiredFreeSpace; }
	bool HasExistingDownload() const { return m_hasExistingDownload; }
	int CopyPackageInfo(SceUInt32 index, SceGameUpdatePackageInfo *outInfo) const;

	bool IsUpdateIdle() const { return m_updateJob == NULL; }
	bool IsDownloadIdle() const { return m_downloadJob == NULL; }

private:
	static void OnUpdateJobDone(int result, const SceGameUpdateResult *updateResult,
				    const SceGameUpdateInfo *updateInfo, SceUInt64 totalSize,
				    bool hasExistingDownload, void *userData);
	static void OnDownloadJobDone(int result, int unused, SceUInt64 requiredFreeSpace,
				      void *userData);

	PatchChkGameUpdateJob *m_updateJob;
	PatchChkDownloaderTaskJob *m_downloadJob;
	SceInt32 m_lastUpdateError;
	SceInt32 m_lastDownloadError;
	SceGameUpdateResult m_updateResult;
	SceGameUpdateInfo m_updateInfo;
	SceUInt64 m_totalPackageSize;
	SceUInt64 m_requiredFreeSpace;
	paf::string m_reservedText;
	paf::vector<SceGameUpdatePackageInfo> m_packages;
	paf::string m_titleId;
	bool m_hasExistingDownload;
	UpdateDoneCallback m_updateDoneCallback;
	DownloadDoneCallback m_downloadDoneCallback;
	void *m_callbackUserData;
	SceGameUpdateContext m_gameUpdateCtx;
};

/* UI dialog and page manager (28 bytes, 0x812AE87C) */
class PatchChkUi {
public:
	typedef void (*UiEventCallback)(int eventId, void *userData);

	PatchChkUi();
	~PatchChkUi();

	void SetEventCallback(UiEventCallback callback, void *userData);
	void CloseCurrentDialog();
	void CloseErrorDialog();
	void OpenOverlayPage();
	void CloseOverlayPage();

	void ShowCheckingDialog();
	void ShowSysUpdatePromptDialog();
	void ShowUpToDateDialog();
	int ShowUpdateAvailableDialog(const paf::string &richTextHtml, bool hasExistingDownload);
	void ShowQuietUpdateIconDialog();
	void ShowDebugPatchTestDialog();
	void ShowDiscontinuedDialog();
	void ShowAlreadyDownloadingDialog();
	void ShowQuietDownloadingIconDialog();
	void ShowYesNoPromptDialog(const paf::IDParam &textId);
	void ShowBgdlTaskLimitDialog();
	void ShowBgdlNoMemoryCardDialog();
	void ShowBgdlNoFreeSpaceDialog(SceUInt64 requiredBytes);
	void ShowBgdlAlreadyQueuedDialog();
	void ShowBgdlGenericErrorDialog();
	void ShowBgdlNotSupportedDialog();
	void ShowErrorCodeDialog(const paf::IDParam &messageId, int errorCode);

	paf::ui::Widget *GetDialogContentWidget() const;
	paf::ui::Widget *GetOverlayRootWidget() const { return m_overlayRoot; }
	void NotifyEvent(int eventId);

private:
	static int OnCheckingDialogOpened(SceInt32 dialogId, void *userData);
	static void OnRichTextLayoutReady(SceInt32 eventType, paf::ui::Handler *handler,
					  paf::ui::Event *event, void *userData);

	UiEventCallback m_eventCallback;
	void *m_callbackUserData;
	SceInt32 m_reservedState;
	SceInt32 m_activeDialogId;
	SceInt32 m_errorDialogId;
	paf::ui::BusyIndicator *m_busyIndicator;
	paf::ui::Widget *m_overlayRoot;
};

/* System update check plugin launcher (20 bytes, 0x812AFD1C) */
class PatchChkSysUpChk {
public:
	typedef void (*SysUpChkCallback)(int result, void *userData);

	PatchChkSysUpChk();
	~PatchChkSysUpChk();

	int Launch(int checkMode, int dialogMode, SysUpChkCallback callback, void *userData);
	void Unload();

private:
	void StartSysUpChkPlugin();
	static void OnSysUpChkUnloaded(void *userData);
	static void OnSysUpChkFinished(int result, void *userData);

	SceInt32 m_checkMode;
	SceInt32 m_dialogMode;
	SceInt32 m_result;
	SysUpChkCallback m_callback;
	void *m_userData;
};

/* Single parsed <changes app_ver="..."> entry (16 bytes) */
struct PatchChkChangeInfoEntry {
	SceUInt32 versionNumber;
	paf::string bodyText;
};

/* Changeinfo HTTP downloader and bXCe SAX XML parser (20 bytes, 0x812B03C6) */
class PatchChkChangeInfoReader {
public:
	PatchChkChangeInfoReader();
	~PatchChkChangeInfoReader();

	void Cancel();
	int ParseXml(const char *xmlBuffer, int xmlLength, const char *currentAppVer);
	int FetchAndParse(const char *changeInfoUrl, const char *currentAppVer);
	void AddEntry(SceUInt32 versionNumber, const paf::string &bodyText);
	void AppendFormattedHtml(paf::string *outHtml) const;

private:
	paf::vector<PatchChkChangeInfoEntry *> m_entries;
	bool m_canceled;
};

/* Async controller running ChangeInfoJob on the Default job queue (48 bytes, 0x812AFF1C) */
class PatchChkChangeInfoController {
public:
	typedef void (*DoneCallback)(int result, void *userData);

	PatchChkChangeInfoController();
	~PatchChkChangeInfoController();

	int Start(DoneCallback callback, const char *currentAppVer,
		  const paf::string &changeInfoUrl, SceUInt64 totalSize, void *userData);
	void Cancel();
	void StopAndMarkCanceled();
	void AppendFormattedHtml(paf::string *outHtml) const;

private:
	static int OnJobRun(void *userData);
	static void OnJobFinish(int result, void *userData);

	paf::common::SharedPtr<paf::job::JobItem> m_job;
	paf::string m_changeInfoUrl;
	paf::string m_currentAppVer;
	PatchChkChangeInfoReader *m_reader;
	DoneCallback m_callback;
	void *m_userData;
	bool m_stopped;
};

/* Main patch check workflow controller (296 bytes, 0x812AC3A8) */
class PatchChkController {
public:
	PatchChkController();
	~PatchChkController();

	void Stop();
	void Cleanup();
	int StartCheck(const PatchChkStartParam &param,
		       PatchChkFinishCallback finishCallback, void *userData);
	int SetStateCallback(PatchChkStateCallback callback, void *userData);
	void ClearStateCallback();
	void Cancel();
	void HandleEvent(int eventId);

private:
	int InitializeSession(SceUInt16 checkMode, PatchChkFinishCallback finishCallback,
			      void *userData);
	void FinishAndNotify();
	void BuildUpdateHeaderHtml(paf::string *outHtml, SceUInt64 totalSize,
				   const char *latestVersionStr);
	void EnterUiState(SceUInt16 state);
	void HandlePresetStatus(SceInt32 status);
	int StartMinDisplayTimer(int timerState);
	void StopMinDisplayTimer();
	int BeginGameUpdateCheck();
	int BeginPatchDownloadTask();
	void HandleUiConfirm();
	void HandleUiEvent(int eventId);
	void ProcessUpdateResult(int result);

	static void OnUiEvent(int eventId, void *userData);
	static void OnSysUpChkDone(int result, void *userData);
	static void OnChangeInfoDone(int result, void *userData);
	static void OnUpdateRunnerDone(int result, void *userData);
	static void OnDownloadRunnerDone(int result, void *userData);
	static void OnTopmenuLaunchDone(int result, void *userData);
	static void OnMinDisplayTimerFired(SceInt32 eventType, paf::ui::Handler *handler,
					   paf::ui::Event *event, void *userData);

	PatchChkUi *m_ui;
	PatchChkGameUpdateRunner *m_runner;
	BgdlPatchDownloadTaskParam m_downloadParam;
	PatchChkSysUpChk *m_sysUpChk;
	PatchChkChangeInfoController *m_changeInfo;
	PatchChkFinishCallback m_finishCallback;
	PatchChkStateCallback m_stateCallback;
	PatchChkStartParam m_startParam;
	void *m_finishUserData;
	void *m_stateUserData;
	SceUInt32 m_checkMode;
	SceUInt32 m_uiState;
	SceInt32 m_lastError;
	SceUInt32 m_infoFlags;
	SceInt32 m_updateStatus;
	bool m_stopping;
	bool m_updateFinishedPendingTimer;
	SceInt32 m_timerState;
	SceInt32 m_deferredUpdateResult;
};

#endif
