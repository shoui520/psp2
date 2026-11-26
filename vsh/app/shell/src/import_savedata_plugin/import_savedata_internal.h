/*
 * import_savedata_internal.h: internal types, error codes, and helper
 * declarations for import_savedata_plugin (0x81393628..0x8139AB6B).
 */
#ifndef SHELL_IMPORT_SAVEDATA_INTERNAL_H
#define SHELL_IMPORT_SAVEDATA_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/map>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <job/job.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../livespace_db/livespace_db_internal.h"

/* import_savedata_plugin error codes (0x801079xx) */
#define IMPORT_SAVEDATA_ERR_GENERAL		(-2146404096)	/* 0x80107900 */
#define IMPORT_SAVEDATA_ERR_INVALID_ARG		(-2146404095)	/* 0x80107901 */
#define IMPORT_SAVEDATA_ERR_NO_MEMORY		(-2146404093)	/* 0x80107903 */
#define IMPORT_SAVEDATA_ERR_NO_SPACE		(-2146404092)	/* 0x80107904 */
#define IMPORT_SAVEDATA_ERR_INCOMPLETE_COPY	(-2146404091)	/* 0x80107905 */
#define IMPORT_SAVEDATA_ERR_BAD_META		(-2146404090)	/* 0x80107906 */
#define IMPORT_SAVEDATA_ERR_NO_APP_INFO		(-2146404089)	/* 0x80107907 */
#define IMPORT_SAVEDATA_ERR_NO_MEDIA		(-2146404088)	/* 0x80107908 */

/* Kernel / AppMgr errors explicitly tested by import_savedata_plugin */
#define IMPORT_SAVEDATA_KERNEL_ENOENT		(-2147418110)	/* 0x80010002 */
#define IMPORT_SAVEDATA_KERNEL_ENODEV		(-2147418093)	/* 0x80010013 */
#define IMPORT_SAVEDATA_PFS_NO_MEDIA		(-2144206822)	/* 0x8032001A */
#define IMPORT_SAVEDATA_APPMGR_NO_APP		(-2139086830)	/* 0x80802012 */

/* Dialog sound selector values (0x8139391C) */
enum ImportSavedataDialogSound {
	IMPORT_SAVEDATA_SOUND_NONE = 0,
	IMPORT_SAVEDATA_SOUND_COMPLETE = 1,
	IMPORT_SAVEDATA_SOUND_ALERT = 2
};

/* Dialog event callbacks reported to ImportSaveDataController */
enum ImportSavedataUiEvent {
	IMPORT_SAVEDATA_UI_CONFIRM = 0,
	IMPORT_SAVEDATA_UI_CANCEL = 1,
	IMPORT_SAVEDATA_UI_OPENED = 2
};

/* State machine values for ImportSaveDataController::m_state */
enum ImportSavedataState {
	IMPORT_SAVEDATA_STATE_IDLE = 0,
	IMPORT_SAVEDATA_STATE_CLOSE_RUNNING_APP = 1,
	IMPORT_SAVEDATA_STATE_INTRO = 2,
	IMPORT_SAVEDATA_STATE_SELECT_TARGET = 3,
	IMPORT_SAVEDATA_STATE_NO_TARGET = 4,
	IMPORT_SAVEDATA_STATE_NO_MEMORY_CARD = 5,
	IMPORT_SAVEDATA_STATE_OVERWRITE_CONFIRM = 6,
	IMPORT_SAVEDATA_STATE_NO_FREE_SPACE = 7,
	IMPORT_SAVEDATA_STATE_COPYING = 8,
	IMPORT_SAVEDATA_STATE_COMPLETE = 9,
	IMPORT_SAVEDATA_STATE_CARD_REMOVED = 10,
	IMPORT_SAVEDATA_STATE_INFO_NOT_FOUND = 11,
	IMPORT_SAVEDATA_STATE_ERROR = 12
};

/* Title ID wrapper passed by value into StartImport (16 bytes) */
struct ImportSavedataTitleParam {
	char titleId[16];
};

/* Savedata meta.dat binary header (264 bytes) */
struct ImportSavedataMetaHeader {
	SceUInt32 version;
	SceUInt32 alignmentPad;
	SceRtcTick timestamp;
	SceUInt8 trailingBytes[248];
};

typedef char ImportSavedataMetaHeaderSize[sizeof(ImportSavedataMetaHeader) == 264 ? 1 : -1];

/* Parsed SFO file handle held in memory (12 bytes) */
struct SfoHandle;
struct ImportSavedataSfoFile {
	void *buffer;
	SceSize size;
	SfoHandle *handle;
};

/* Target savedata entry collected by ImportSaveDataSearchTargetJobItem */
struct ImportSavedataTargetEntry {
	bool selected;
	SceInt32 storageType;
	paf::string title;
	paf::string titleId;

	ImportSavedataTargetEntry() : selected(false), storageType(0) {}
};

typedef paf::map<paf::string, ImportSavedataTargetEntry> ImportSavedataTargetMap;

/* Plugin interface 1 callbacks (matches topmenu_plugin/import_savedata_launcher.cpp) */
typedef void (*ImportSavedataFinishCallback)(SceInt32 result, void *userData);
typedef void (*ImportSavedataStateCallback)(SceInt32 state, void *userData);

struct ImportSavedataPluginInterface {
	SceInt32 (*startImport)(ImportSavedataTitleParam titleParam,
				ImportSavedataFinishCallback finishCb, void *userData);
	SceInt32 (*cancelImport)(void);
	SceInt32 (*setStateCallback)(ImportSavedataStateCallback stateCb, void *userData);
};

/* Dialog event callback into controller */
typedef void (*ImportSavedataDialogCallback)(SceInt32 eventCode, void *userData);
typedef void (*ImportSavedataJobResultCallback)(SceInt32 result, void *userData);
typedef void (*ImportSavedataProgressCallback)(SceInt32 percent, void *userData);
typedef void (*ImportSavedataIconLoadedCallback)(SceInt32 cellIndex, SceInt32 result,
						 void *userData);

/* Forward declarations */
class ImportSaveDataController;
class ImportSaveDataDialog;
class ImportSaveDataExecutor;
class ImportSaveDataJobManager;
class ImportSaveDataExecuteJobItem;
class ImportSaveDataLoadIconJobItem;
class ImportSaveDataLoadInfoJobItem;
class ImportSaveDataSearchTargetJobItem;

/* Utility functions (import_savedata_util.cpp, 0x813937F4..0x8139541D) */
void importSavedataSetPlugin(paf::Plugin *plugin);
void importSavedataClearPlugin(void);
paf::Plugin *importSavedataGetPlugin(void);
SceUInt32 importSavedataResolveSoundId(SceInt32 soundType);
const wchar_t *importSavedataGetString(const paf::IDParam &id);
SceInt32 importSavedataShowLayoutDialog(const paf::wstring &text, SceUInt32 soundId,
					const CommonGuiDialogLayout *layout,
					CommonGuiDialogHandler *handler);
SceInt32 importSavedataShowOkDialog(const paf::IDParam &textId, SceInt32 soundType,
				    CommonGuiDialogHandler *handler);
SceInt32 importSavedataShowOkSizeDialog(const paf::IDParam &textId, SceInt32 soundType,
					SceInt64 sizeBytes, CommonGuiDialogHandler *handler);
SceInt32 importSavedataShowCancelOkDialog(const paf::IDParam &textId, SceInt32 soundType,
					  CommonGuiDialogHandler *handler);
SceInt32 importSavedataShowYesNoDialog(const paf::IDParam &textId, SceInt32 soundType,
				       CommonGuiDialogHandler *handler);
SceInt32 importSavedataShowWaitingDialog(const paf::IDParam &textId,
					 CommonGuiDialogHandler *handler);
SceInt32 importSavedataShowErrorDialog(const paf::IDParam &messageId, SceInt32 errorCode,
				       CommonGuiDialogHandler *handler,
				       CommonGuiMessageDialogOpenHandler *openHandler);
SceInt32 importSavedataCloseDialog(SceInt32 dialogId);
SceInt32 importSavedataOpenTemplate(paf::ui::Widget *parent, const paf::IDParam &templateId);
paf::wstring importSavedataFormatDateTime(const SceDateTime *dt);
SceInt32 importSavedataDestroyApp(SceInt32 appId);
SceInt32 importSavedataQueryLsdbAppInfo(const char *titleId, LsdbAppInfoPtr *outInfo);
SceInt32 importSavedataGetForegroundAppId(void);
SceInt32 importSavedataGetAppTitleId(SceInt32 appId, char *outTitleId, SceSize maxLen);
SceInt32 importSavedataAcquireGameDialogHold(void);
SceInt32 importSavedataReleaseGameDialogHold(void);
SceInt32 importSavedataEnqueueGameJob(paf::job::JobItem *job);
SceInt32 importSavedataLoadSurfaceFromFile(const char *path,
					   paf::intrusive_ptr<paf::graph::Surface> *outSurface);
SceInt32 importSavedataLoadLsdbIcon(paf::intrusive_ptr<paf::graph::Surface> *outSurface,
				    const LsdbAppInfoPtr *appInfo);
SceInt32 importSavedataGetDeviceFreeSpace(const char *device, SceUInt64 *outFreeBytes);
paf::string importSavedataBuildCardPath(const char *titleId);
SceInt32 importSavedataGetCardTreeSize(const char *titleId, void *cancelUserData,
				       SceUInt64 *outSize);
paf::string importSavedataBuildUserPath(const char *titleId, bool temporary);
bool importSavedataIsMemoryCardMounted(void);
SceInt32 importSavedataIsGameCardInserted(bool *outInserted);
SceInt32 importSavedataCopyFileStream(const char *srcPath, const char *dstPath,
				      SceInt32 copyOption, SceInt32 *outErrorStage);
SceInt32 importSavedataRemoveDirectoryRecursive(const char *path);
SceInt32 importSavedataRemoveUserSubdir(const char *subDirName);
SceInt32 importSavedataOpenSfoFile(const char *path, ImportSavedataSfoFile *outSfo);
SceInt32 importSavedataCloseSfoFile(ImportSavedataSfoFile *sfo);
SceInt32 importSavedataGetSfoParam(ImportSavedataSfoFile *sfo, const char *key,
				   const void **outValue, SceSize *outLen, SceSize *outMaxLen);
const char *importSavedataGetInfoKeyName(SceInt32 languageId);
const char *importSavedataGetTitleKeyName(SceInt32 languageId);

/* Singleton job queue manager (import_savedata_job_manager.cpp, 0x81396638..0x813968FB) */
class ImportSaveDataJobManager {
public:
	ImportSaveDataJobManager();
	virtual ~ImportSaveDataJobManager();

	static ImportSaveDataJobManager *Create(void);
	static void Destroy(void);
	static ImportSaveDataJobManager *GetInstance(void);

	void CreateQueue(void);
	void RemoveJob(SceInt32 key, bool cancelJob);
	void EnqueueJob(SceInt32 key, paf::job::JobItem *job);
	bool CancelAll(void);

private:
	paf::job::JobQueue *m_queue;
	paf::map<SceInt32, paf::job::JobItem *> m_activeJobs;
};

/* Dialog presenter (import_savedata_dialog.cpp, 0x813968FC..0x8139913D) */
class ImportSaveDataDialog {
public:
	ImportSaveDataDialog(ImportSavedataDialogCallback eventCb, void *userData);
	~ImportSaveDataDialog();

	SceInt32 ShowIntroDialog(const char *titleId);
	SceInt32 ShowTargetListDialog(ImportSavedataTargetMap *targets);
	SceInt32 ShowNoTargetDialog(void);
	SceInt32 ShowNoMemoryCardDialog(void);
	SceInt32 ShowCloseRunningAppDialog(const char *runningTitleId);
	SceInt32 ShowOverwriteConfirmDialog(const char *titleId);
	SceInt32 ShowInsufficientSpaceDialog(SceInt64 shortageBytes);
	SceInt32 ShowCopyingDialog(void);
	SceInt32 ShowCompleteDialog(void);
	SceInt32 ShowCardRemovedDialog(void);
	SceInt32 ShowInfoNotFoundDialog(void);
	SceInt32 ShowErrorDialog(SceInt32 errorCode);
	SceInt32 CloseCurrentDialog(void);

	void SetInfoText(const paf::string &infoText) { m_infoText = infoText; }
	void NotifyEvent(SceInt32 eventCode);

	ImportSavedataDialogCallback m_eventCallback;
	void *m_callbackUserData;
	SceInt32 m_dialogId;
	paf::string m_infoText;
};

/* Copy execution coordinator (import_savedata_controller.cpp, 0x813964E4..0x813965C5) */
class ImportSaveDataExecutor {
public:
	ImportSaveDataExecutor(ImportSavedataJobResultCallback finishCb, void *userData);
	~ImportSaveDataExecutor();

	SceInt32 StartCopy(const char *titleId, SceUInt64 totalBytes,
			   ImportSavedataTargetMap *targets);

	static void OnExecuteJobFinished(SceInt32 result, void *userData);
	static void OnExecuteJobProgress(SceInt32 phase, SceInt32 percent, void *userData);

	ImportSavedataJobResultCallback m_finishCallback;
	ImportSavedataProgressCallback m_progressCallback;
	void *m_callbackUserData;
	SceUInt64 m_reservedCounter;
};

/* Main state machine controller (import_savedata_controller.cpp, 0x81395490..0x81396473) */
class ImportSaveDataController {
public:
	ImportSaveDataController();
	~ImportSaveDataController();

	void Cleanup(void);
	SceInt32 CancelImport(void);
	SceInt32 TransitionState(SceInt32 nextState);
	void StartImport(ImportSavedataTitleParam titleParam,
			 ImportSavedataFinishCallback finishCb, void *userData);
	void SetStateCallback(ImportSavedataStateCallback stateCb, void *userData);
	SceInt32 CheckFreeSpace(void);

	void OnDialogCancel(void);
	void OnDialogConfirm(void);
	void OnDialogEvent(SceInt32 eventCode);
	void OnExecuteFinished(SceInt32 result);
	void OnLoadInfoFinished(SceInt32 result);
	void OnSearchTargetFinished(SceInt32 result);

	static void StaticDialogCallback(SceInt32 eventCode, void *userData);
	static void StaticExecuteCallback(SceInt32 result, void *userData);
	static void StaticLoadInfoCallback(SceInt32 result, void *userData);
	static void StaticSearchTargetCallback(SceInt32 result, void *userData);

private:
	void FinishAndNotify(void);

	char m_titleId[16];
	SceInt32 m_state;
	SceInt32 m_resultCode;
	ImportSavedataFinishCallback m_finishCallback;
	ImportSavedataStateCallback m_stateCallback;
	void *m_callbackUserData;
	SceInt32 m_runningAppId;
	ImportSavedataTargetMap m_targets;
	SceUInt64 m_shortageBytes;
	SceUInt64 m_totalCopyBytes;
	ImportSaveDataDialog *m_dialog;
	ImportSaveDataExecutor *m_executor;
	ImportSaveDataLoadInfoJobItem *m_loadInfoJob;
	ImportSaveDataSearchTargetJobItem *m_searchTargetJob;
};

/* Background job: copy savedata tree & swap temp folder (0x813991B0..0x81399A51) */
class ImportSaveDataExecuteJobItem : public paf::job::JobItem {
public:
	typedef void (*ProgressCallback)(SceInt32 phase, SceInt32 percent, void *userData);

	ImportSaveDataExecuteJobItem(ImportSavedataJobResultCallback finishCb,
				     ProgressCallback progressCb, SceUInt64 totalBytes,
				     void *userData);
	virtual ~ImportSaveDataExecuteJobItem();
	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);

	SceInt32 SetDestinationTitleId(const char *titleId);
	void SetTargets(ImportSavedataTargetMap *targets);

private:
	SceInt32 CopyDirectoryRecursive(const char *srcDir, const char *dstDir);

	ImportSavedataJobResultCallback m_finishCallback;
	ProgressCallback m_progressCallback;
	void *m_callbackUserData;
	bool m_suppressed;
	char m_destTitleId[15];
	paf::vector<paf::string> m_sourceTitleIds;
	SceUInt64 m_totalBytes;
	SceUInt64 m_copiedBytes;
	SceInt32 m_progressPercent;
};

/* Background job: load target title icon into ListItem widget (0x81399AC4..0x81399E87) */
class ImportSaveDataLoadIconJobItem : public paf::job::JobItem {
public:
	ImportSaveDataLoadIconJobItem(ImportSavedataIconLoadedCallback doneCb,
				      paf::ui::Widget *targetWidget, paf::Plugin *plugin,
				      void *userData);
	virtual ~ImportSaveDataLoadIconJobItem();
	virtual int Run();
	virtual void Finish(int result);

	void SetTarget(SceInt32 cellIndex, SceInt32 storageType, const paf::string &titleId);

private:
	ImportSavedataIconLoadedCallback m_doneCallback;
	paf::ui::Widget *m_targetWidget;
	paf::Plugin *m_plugin;
	void *m_callbackUserData;
	SceInt32 m_cellIndex;
	SceInt32 m_storageType;
	paf::string m_titleId;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
};

/* Background job: read IMPORT_SAVEDATA_INFO from param.sfo (0x81399EF8..0x8139A327) */
class ImportSaveDataLoadInfoJobItem : public paf::job::JobItem {
public:
	ImportSaveDataLoadInfoJobItem(ImportSavedataJobResultCallback doneCb, const char *titleId,
				      void *userData);
	virtual ~ImportSaveDataLoadInfoJobItem();
	virtual int Run();
	virtual void Finish(int result);

	const paf::string &GetInfoText() const { return m_infoText; }

private:
	SceInt32 ReadInfoFromSfo(const char *sfoPath);
	SceInt32 ResolveSfoPath(char *outSfoPath, SceSize pathCapacity, char *outMountPoint,
				bool *outMounted);

	ImportSavedataJobResultCallback m_doneCallback;
	paf::string m_titleId;
	void *m_callbackUserData;
	paf::string m_infoText;
};

/* Background job: search LSDB for importable savedata titles (0x8139A398..0x8139AAF9) */
class ImportSaveDataSearchTargetJobItem : public paf::job::JobItem {
public:
	ImportSaveDataSearchTargetJobItem(ImportSavedataJobResultCallback doneCb,
					  const char *titleId, void *userData);
	virtual ~ImportSaveDataSearchTargetJobItem();
	virtual int Run();
	virtual void Finish(int result);

	const ImportSavedataTargetMap &GetTargets() const { return m_targets; }

private:
	ImportSavedataJobResultCallback m_doneCallback;
	void *m_callbackUserData;
	paf::string m_titleId;
	ImportSavedataTargetMap m_targets;
};

#endif /* SHELL_IMPORT_SAVEDATA_INTERNAL_H */
