/*
 * import_savedata_controller.cpp: ImportSaveDataController state machine and
 * ImportSaveDataExecutor copy coordinator (0x81395490..0x81396637).
 */
#include "import_savedata_internal.h"

#include <kernel/iofilemgr.h>
#include <std/stdio.h>
#include <std/string.h>
#include "../../../game/game_plugin/game_plugin.h"

extern "C" {
int sceAppMgrGetUserDirPath(int partition, char *outPath, SceSize maxLen);
SceInt64 SceLsdb_9D27A87D(const LsdbAppInfoPtr *app);
}

static const SceUInt64 kSavedataCopyHeadroomBytes = 307200ULL;
static const SceUInt64 kSavedataMinFreeSpaceBytes = 0x220000ULL;

/* 0x81395490 */
ImportSaveDataController::ImportSaveDataController()
	: m_state(IMPORT_SAVEDATA_STATE_IDLE),
	  m_resultCode(0),
	  m_finishCallback(NULL),
	  m_stateCallback(NULL),
	  m_callbackUserData(NULL),
	  m_runningAppId(-1),
	  m_shortageBytes(0),
	  m_totalCopyBytes(0),
	  m_dialog(NULL),
	  m_executor(NULL),
	  m_loadInfoJob(NULL),
	  m_searchTargetJob(NULL)
{
	sce_paf_memset(m_titleId, 0, sizeof(m_titleId));
}

/* 0x813954F2 */
ImportSaveDataController::~ImportSaveDataController()
{
}

/* 0x81395514 */
void ImportSaveDataController::Cleanup(void)
{
	if (m_executor != NULL) {
		delete m_executor;
		m_executor = NULL;
	}
	if (m_dialog != NULL) {
		delete m_dialog;
		m_dialog = NULL;
	}
}

void ImportSaveDataController::FinishAndNotify(void)
{
	if (m_dialog != NULL)
		m_dialog->CloseCurrentDialog();

	ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
	if (jobMgr != NULL)
		jobMgr->CancelAll();

	importSavedataReleaseGameDialogHold();
	if (m_finishCallback != NULL)
		m_finishCallback(m_resultCode, m_callbackUserData);
}

/* 0x8139553C */
SceInt32 ImportSaveDataController::CancelImport(void)
{
	ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
	if (jobMgr != NULL)
		jobMgr->CancelAll();

	SceInt32 ret = importSavedataReleaseGameDialogHold();
	if (m_finishCallback != NULL)
		m_finishCallback(m_resultCode, m_callbackUserData);
	return ret;
}

/* 0x81395560 */
SceInt32 ImportSaveDataController::TransitionState(SceInt32 nextState)
{
	m_state = nextState;
	SceInt32 ret = 0;

	switch (nextState) {
	case IMPORT_SAVEDATA_STATE_INTRO:
		ret = importSavedataRemoveUserSubdir("_temp_savedata_vc");
		if (ret == 0)
			ret = importSavedataRemoveUserSubdir("_temp2_savedata_vc");
		if (ret == 0)
			ret = m_dialog->ShowIntroDialog(m_titleId);
		break;
	case IMPORT_SAVEDATA_STATE_SELECT_TARGET:
		ret = m_dialog->ShowTargetListDialog(&m_targets);
		break;
	case IMPORT_SAVEDATA_STATE_NO_TARGET:
		ret = m_dialog->ShowNoTargetDialog();
		break;
	case IMPORT_SAVEDATA_STATE_NO_MEMORY_CARD:
		ret = m_dialog->ShowNoMemoryCardDialog();
		break;
	case IMPORT_SAVEDATA_STATE_OVERWRITE_CONFIRM:
		ret = m_dialog->ShowOverwriteConfirmDialog(m_titleId);
		break;
	case IMPORT_SAVEDATA_STATE_NO_FREE_SPACE:
		ret = m_dialog->ShowInsufficientSpaceDialog(static_cast<SceInt64>(m_shortageBytes));
		break;
	case IMPORT_SAVEDATA_STATE_COPYING:
		ret = m_dialog->ShowCopyingDialog();
		break;
	case IMPORT_SAVEDATA_STATE_COMPLETE:
		ret = m_dialog->ShowCompleteDialog();
		break;
	case IMPORT_SAVEDATA_STATE_CARD_REMOVED:
		ret = m_dialog->ShowCardRemovedDialog();
		break;
	case IMPORT_SAVEDATA_STATE_INFO_NOT_FOUND:
		ret = m_dialog->ShowInfoNotFoundDialog();
		break;
	default:
		break;
	}

	if (m_state == IMPORT_SAVEDATA_STATE_INTRO && ret == IMPORT_SAVEDATA_KERNEL_ENODEV) {
		m_state = IMPORT_SAVEDATA_STATE_NO_MEMORY_CARD;
		ret = m_dialog->ShowNoMemoryCardDialog();
	}

	if (ret != 0) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		return m_dialog->ShowErrorDialog(ret);
	}
	return 0;
}

/* 0x8139564E */
void ImportSaveDataController::StartImport(ImportSavedataTitleParam titleParam,
					   ImportSavedataFinishCallback finishCb, void *userData)
{
	sce_paf_memcpy(m_titleId, titleParam.titleId, sizeof(m_titleId));

	m_dialog = new ImportSaveDataDialog(StaticDialogCallback, this);
	if (m_dialog == NULL)
		return;

	m_executor = new ImportSaveDataExecutor(StaticExecuteCallback, this);
	if (m_executor == NULL) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_NO_MEMORY);
		return;
	}

	sce_paf_memcpy(m_titleId, titleParam.titleId, sizeof(m_titleId));
	m_finishCallback = finishCb;
	m_callbackUserData = userData;
	m_targets.clear();

	LsdbAppInfoPtr targetAppInfo(new LsdbAppInfo());
	if (targetAppInfo.get() == NULL) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_NO_MEMORY);
		return;
	}

	SceInt32 ret = importSavedataQueryLsdbAppInfo(titleParam.titleId, &targetAppInfo);
	if (ret < 0) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(ret);
		return;
	}

	SceInt32 storageType = SceLsdb_298134FE(&targetAppInfo);
	bool needsMemoryCard = (storageType == 2 || storageType == 3);
	if (needsMemoryCard && !importSavedataIsMemoryCardMounted()) {
		TransitionState(IMPORT_SAVEDATA_STATE_NO_MEMORY_CARD);
		return;
	}

	SceInt32 fgAppId = importSavedataGetForegroundAppId();
	char runningTitleId[16];
	sce_paf_memset(runningTitleId, 0, 13);
	ret = importSavedataGetAppTitleId(fgAppId, runningTitleId, 13);
	if (ret != 0) {
		if (ret == IMPORT_SAVEDATA_APPMGR_NO_APP) {
			m_loadInfoJob = new ImportSaveDataLoadInfoJobItem(
				StaticLoadInfoCallback, m_titleId, this);
			if (m_loadInfoJob == NULL) {
				m_state = IMPORT_SAVEDATA_STATE_ERROR;
				m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_NO_MEMORY);
				return;
			}
			ret = importSavedataEnqueueGameJob(m_loadInfoJob);
			if (ret == 0)
				return;
		}
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(ret);
		return;
	}

	LsdbAppInfoPtr runningAppInfo(new LsdbAppInfo());
	if (runningAppInfo.get() == NULL) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_NO_MEMORY);
		return;
	}

	if (importSavedataQueryLsdbAppInfo(runningTitleId, &runningAppInfo) != 0) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_NO_APP_INFO);
		return;
	}

	SceInt32 runningStorage = SceLsdb_298134FE(&runningAppInfo);
	SceInt64 appAttr = SceLsdb_9D27A87D(&runningAppInfo);
	if (appAttr == 1 && (runningStorage == 1 || runningStorage == 2)) {
		m_runningAppId = fgAppId;
		m_state = IMPORT_SAVEDATA_STATE_CLOSE_RUNNING_APP;
		ret = m_dialog->ShowCloseRunningAppDialog(runningTitleId);
		if (ret != 0) {
			m_state = IMPORT_SAVEDATA_STATE_ERROR;
			m_dialog->ShowErrorDialog(ret);
		}
		return;
	}

	m_loadInfoJob = new ImportSaveDataLoadInfoJobItem(StaticLoadInfoCallback, m_titleId, this);
	if (m_loadInfoJob == NULL) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_NO_MEMORY);
		return;
	}

	ret = importSavedataEnqueueGameJob(m_loadInfoJob);
	if (ret != 0) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(ret);
	}
}

/* 0x81395C00 */
void ImportSaveDataController::SetStateCallback(ImportSavedataStateCallback stateCb, void *userData)
{
	m_stateCallback = stateCb;
	if (userData != NULL)
		m_callbackUserData = userData;
}

/* 0x81395C08 */
SceInt32 ImportSaveDataController::CheckFreeSpace(void)
{
	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	if (appInfo.get() == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	SceInt32 ret = importSavedataQueryLsdbAppInfo(m_titleId, &appInfo);
	if (ret != 0)
		return ret;

	m_totalCopyBytes = 0;
	for (ImportSavedataTargetMap::iterator it = m_targets.begin(); it != m_targets.end(); ++it) {
		if (!it->second.selected)
			continue;

		SceUInt64 dirSize = 0;
		ret = importSavedataGetCardTreeSize(it->first.c_str(), this, &dirSize);
		if (ret != 0) {
			if (ret == IMPORT_SAVEDATA_KERNEL_ENODEV ||
			    ret == IMPORT_SAVEDATA_PFS_NO_MEDIA)
				ret = IMPORT_SAVEDATA_ERR_NO_MEDIA;
			return ret;
		}
		m_totalCopyBytes += dirSize;
	}

	SceUInt64 freeBytes = 0;
	ret = importSavedataGetDeviceFreeSpace("ux0:", &freeBytes);
	if (ret != 0)
		return ret;

	SceUInt64 requiredBytes = m_totalCopyBytes + kSavedataCopyHeadroomBytes;
	if (freeBytes >= requiredBytes && freeBytes >= kSavedataMinFreeSpaceBytes)
		return 0;

	if (requiredBytes >= kSavedataMinFreeSpaceBytes)
		m_shortageBytes = requiredBytes - freeBytes;
	else
		m_shortageBytes = kSavedataMinFreeSpaceBytes - freeBytes;

	return IMPORT_SAVEDATA_ERR_NO_SPACE;
}

/* 0x81395F66 */
void ImportSaveDataController::OnDialogCancel(void)
{
	if (m_state == IMPORT_SAVEDATA_STATE_SELECT_TARGET) {
		ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
		if (jobMgr != NULL)
			jobMgr->CancelAll();
	}
	FinishAndNotify();
}

/* 0x81395FD2 */
void ImportSaveDataController::OnDialogConfirm(void)
{
	switch (m_state) {
	case IMPORT_SAVEDATA_STATE_CLOSE_RUNNING_APP: {
		SceInt32 ret = importSavedataDestroyApp(m_runningAppId);
		if (ret != 0 && ret != IMPORT_SAVEDATA_APPMGR_NO_APP) {
			m_state = IMPORT_SAVEDATA_STATE_ERROR;
			m_dialog->ShowErrorDialog(ret);
			return;
		}
		m_loadInfoJob = new ImportSaveDataLoadInfoJobItem(
			StaticLoadInfoCallback, m_titleId, this);
		if (m_loadInfoJob == NULL) {
			m_state = IMPORT_SAVEDATA_STATE_ERROR;
			m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_NO_MEMORY);
			return;
		}
		ret = importSavedataEnqueueGameJob(m_loadInfoJob);
		if (ret != 0) {
			m_state = IMPORT_SAVEDATA_STATE_ERROR;
			m_dialog->ShowErrorDialog(ret);
		}
		break;
	}
	case IMPORT_SAVEDATA_STATE_INTRO: {
		bool inserted = false;
		if (importSavedataIsGameCardInserted(&inserted) >= 0 && inserted) {
			m_searchTargetJob = new ImportSaveDataSearchTargetJobItem(
				StaticSearchTargetCallback, m_titleId, this);
			if (m_loadInfoJob == NULL) {
				m_state = IMPORT_SAVEDATA_STATE_ERROR;
				m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_NO_MEMORY);
				return;
			}
			SceInt32 ret = importSavedataEnqueueGameJob(m_searchTargetJob);
			if (ret != 0) {
				m_state = IMPORT_SAVEDATA_STATE_ERROR;
				m_dialog->ShowErrorDialog(ret);
			}
		} else {
			FinishAndNotify();
		}
		break;
	}
	case IMPORT_SAVEDATA_STATE_SELECT_TARGET: {
		ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
		if (jobMgr != NULL)
			jobMgr->CancelAll();

		if (!importSavedataIsMemoryCardMounted()) {
			TransitionState(IMPORT_SAVEDATA_STATE_NO_MEMORY_CARD);
			return;
		}

		char userDir[32];
		SceInt32 ret = sceAppMgrGetUserDirPath(GAME_USER_DIR_PARTITION, userDir,
						       sizeof(userDir));
		if (ret == 0) {
			char metaPath[512];
			SceIoStat st;
			sce_paf_memset(metaPath, 0, sizeof(metaPath));
			sce_paf_snprintf(metaPath, sizeof(metaPath), "%s/savedata_vc/%s",
					 userDir, "meta.dat");
			ret = sceIoGetstat(metaPath, &st);
			if (ret == 0) {
				TransitionState(IMPORT_SAVEDATA_STATE_OVERWRITE_CONFIRM);
				return;
			}
		}

		if (ret != IMPORT_SAVEDATA_KERNEL_ENOENT) {
			m_state = IMPORT_SAVEDATA_STATE_ERROR;
			m_dialog->ShowErrorDialog(ret);
			return;
		}

		ret = CheckFreeSpace();
		if (ret == IMPORT_SAVEDATA_ERR_NO_SPACE) {
			TransitionState(IMPORT_SAVEDATA_STATE_NO_FREE_SPACE);
		} else if (ret == IMPORT_SAVEDATA_ERR_NO_MEDIA) {
			TransitionState(IMPORT_SAVEDATA_STATE_CARD_REMOVED);
		} else if (ret != 0) {
			m_state = IMPORT_SAVEDATA_STATE_ERROR;
			m_dialog->ShowErrorDialog(ret);
		} else {
			TransitionState(IMPORT_SAVEDATA_STATE_COPYING);
		}
		break;
	}
	case IMPORT_SAVEDATA_STATE_OVERWRITE_CONFIRM: {
		SceInt32 ret = CheckFreeSpace();
		if (ret == IMPORT_SAVEDATA_ERR_NO_SPACE) {
			TransitionState(IMPORT_SAVEDATA_STATE_NO_FREE_SPACE);
		} else if (ret == IMPORT_SAVEDATA_ERR_NO_MEDIA) {
			TransitionState(IMPORT_SAVEDATA_STATE_CARD_REMOVED);
		} else if (ret != 0) {
			m_state = IMPORT_SAVEDATA_STATE_ERROR;
			m_dialog->ShowErrorDialog(ret);
		} else {
			TransitionState(IMPORT_SAVEDATA_STATE_COPYING);
		}
		break;
	}
	case IMPORT_SAVEDATA_STATE_COPYING:
		break;
	default:
		FinishAndNotify();
		break;
	}
}

/* 0x81396272 */
void ImportSaveDataController::OnDialogEvent(SceInt32 eventCode)
{
	if (eventCode == IMPORT_SAVEDATA_UI_CONFIRM) {
		OnDialogConfirm();
	} else if (eventCode == IMPORT_SAVEDATA_UI_CANCEL) {
		OnDialogCancel();
	} else if (eventCode == IMPORT_SAVEDATA_UI_OPENED &&
		   m_state == IMPORT_SAVEDATA_STATE_COPYING) {
		m_executor->StartCopy(m_titleId, m_totalCopyBytes, &m_targets);
		m_targets.clear();
	}
}

/* 0x813962B8 */
void ImportSaveDataController::StaticDialogCallback(SceInt32 eventCode, void *userData)
{
	ImportSaveDataController *self = static_cast<ImportSaveDataController *>(userData);
	if (self != NULL)
		self->OnDialogEvent(eventCode);
}

/* 0x813962CA */
void ImportSaveDataController::OnExecuteFinished(SceInt32 result)
{
	if (result == 0) {
		FinishAndNotify();
	} else if (result == IMPORT_SAVEDATA_ERR_NO_MEDIA) {
		TransitionState(IMPORT_SAVEDATA_STATE_CARD_REMOVED);
	} else {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(result);
	}
}

void ImportSaveDataController::StaticExecuteCallback(SceInt32 result, void *userData)
{
	ImportSaveDataController *self = static_cast<ImportSaveDataController *>(userData);
	if (self != NULL)
		self->OnExecuteFinished(result);
}

/* 0x81396320 */
void ImportSaveDataController::OnLoadInfoFinished(SceInt32 result)
{
	if (result == IMPORT_SAVEDATA_KERNEL_ENOENT) {
		TransitionState(IMPORT_SAVEDATA_STATE_INFO_NOT_FOUND);
	} else if (result != 0) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(result);
	} else {
		paf::string infoText = m_loadInfoJob->GetInfoText();
		m_dialog->SetInfoText(infoText);
		TransitionState(IMPORT_SAVEDATA_STATE_INTRO);
	}
}

void ImportSaveDataController::StaticLoadInfoCallback(SceInt32 result, void *userData)
{
	ImportSaveDataController *self = static_cast<ImportSaveDataController *>(userData);
	if (self != NULL)
		self->OnLoadInfoFinished(result);
}

/* 0x813963B8 */
void ImportSaveDataController::OnSearchTargetFinished(SceInt32 result)
{
	if (result <= 0) {
		if (result != 0) {
			m_state = IMPORT_SAVEDATA_STATE_ERROR;
			m_dialog->ShowErrorDialog(result);
		} else {
			TransitionState(IMPORT_SAVEDATA_STATE_NO_TARGET);
		}
		return;
	}

	if (m_searchTargetJob == NULL) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(IMPORT_SAVEDATA_ERR_GENERAL);
		return;
	}

	SceInt32 holdRet = importSavedataAcquireGameDialogHold();
	if (holdRet < 0) {
		m_state = IMPORT_SAVEDATA_STATE_ERROR;
		m_dialog->ShowErrorDialog(holdRet);
		return;
	}

	m_targets = m_searchTargetJob->GetTargets();
	TransitionState(IMPORT_SAVEDATA_STATE_SELECT_TARGET);
}

/* 0x8139646A */
void ImportSaveDataController::StaticSearchTargetCallback(SceInt32 result, void *userData)
{
	ImportSaveDataController *self = static_cast<ImportSaveDataController *>(userData);
	if (self != NULL)
		self->OnSearchTargetFinished(result);
}

/* --- ImportSaveDataExecutor (0x813964E4..0x813965C5) --- */

/* 0x813964E4 */
ImportSaveDataExecutor::ImportSaveDataExecutor(ImportSavedataJobResultCallback finishCb,
					       void *userData)
	: m_finishCallback(finishCb),
	  m_progressCallback(NULL),
	  m_callbackUserData(userData),
	  m_reservedCounter(0)
{
}

/* 0x813964F4 */
ImportSaveDataExecutor::~ImportSaveDataExecutor()
{
	ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
	if (jobMgr != NULL)
		jobMgr->RemoveJob(0, true);
}

/* 0x8139650C */
SceInt32 ImportSaveDataExecutor::StartCopy(const char *titleId, SceUInt64 totalBytes,
					   ImportSavedataTargetMap *targets)
{
	ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
	if (jobMgr == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	ImportSaveDataExecuteJobItem *job = new ImportSaveDataExecuteJobItem(
		OnExecuteJobFinished, OnExecuteJobProgress, totalBytes, this);
	if (job == NULL)
		return IMPORT_SAVEDATA_ERR_NO_MEMORY;

	job->SetDestinationTitleId(titleId);
	job->SetTargets(targets);
	jobMgr->EnqueueJob(0, job);
	return 0;
}

/* 0x81396588 */
void ImportSaveDataExecutor::OnExecuteJobFinished(SceInt32 result, void *userData)
{
	ImportSaveDataExecutor *self = static_cast<ImportSaveDataExecutor *>(userData);
	if (self == NULL)
		return;

	ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
	if (jobMgr != NULL)
		jobMgr->RemoveJob(0, false);

	if (self->m_finishCallback != NULL)
		self->m_finishCallback(result, self->m_callbackUserData);
}

/* 0x813965AE */
void ImportSaveDataExecutor::OnExecuteJobProgress(SceInt32 phase, SceInt32 percent, void *userData)
{
	ImportSaveDataExecutor *self = static_cast<ImportSaveDataExecutor *>(userData);
	if (self != NULL && phase == 0 && self->m_progressCallback != NULL)
		self->m_progressCallback(percent, self->m_callbackUserData);
}
