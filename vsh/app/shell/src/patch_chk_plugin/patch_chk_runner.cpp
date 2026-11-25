/*
 * patch_chk_runner.cpp: coordinates async GameUpdateJob and DownloaderTaskJob
 * execution and stores the resulting package list (0x812AE068-0x812AE624).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include "patch_chk_internal.h"

using namespace paf;

/* Constructor (0x812AE068) */
PatchChkGameUpdateRunner::PatchChkGameUpdateRunner()
	: m_updateJob(NULL),
	  m_downloadJob(NULL),
	  m_lastUpdateError(0),
	  m_lastDownloadError(0),
	  m_totalPackageSize(0),
	  m_requiredFreeSpace(0),
	  m_hasExistingDownload(false),
	  m_updateDoneCallback(NULL),
	  m_downloadDoneCallback(NULL),
	  m_callbackUserData(NULL)
{
	sce_paf_memset(&m_updateResult, 0, sizeof(m_updateResult));
	sce_paf_memset(&m_updateInfo, 0, sizeof(m_updateInfo));
	sce_paf_memset(&m_gameUpdateCtx, 0, sizeof(m_gameUpdateCtx));
}

/* Destructor (0x812AE118) */
PatchChkGameUpdateRunner::~PatchChkGameUpdateRunner()
{
	m_updateJob = NULL;
}

/* Initializes SceGameUpdate and enqueues PatchChkGameUpdateJob (0x812AE186) */
int PatchChkGameUpdateRunner::StartUpdateCheck(const SceGameUpdateInitParam *param)
{
	int ret = sceGameUpdateInitEx(&m_gameUpdateCtx, 0, param);
	if (ret < 0)
		return ret;

	m_titleId = param->titleId;

	PatchChkGameUpdateJob *rawJob =
		new PatchChkGameUpdateJob(&m_gameUpdateCtx, m_titleId, OnUpdateJobDone, this);
	m_updateJob = rawJob;
	if (rawJob == NULL) {
		sceGameUpdateTerm(&m_gameUpdateCtx);
		return PATCH_CHK_ERROR_NO_MEMORY;
	}

	common::SharedPtr<job::JobItem> jobItem(rawJob);
	job::JobQueue *queue = PatchChkJobQueueMgr::GetInstance()->GetQueue(PATCH_CHK_QUEUE_GAME_UPDATE);
	ret = queue->Enqueue(jobItem);
	if (ret < 0) {
		m_updateJob = NULL;
		sceGameUpdateTerm(&m_gameUpdateCtx);
	}
	return ret;
}

/* Detaches and cancels any running GameUpdateJob (0x812AE300) */
int PatchChkGameUpdateRunner::DetachUpdateJob()
{
	if (m_updateJob != NULL)
		m_updateJob->DetachAndCancel();
	return 0;
}

/* Cancels any running GameUpdateJob while keeping finish callback (0x812AE30E) */
int PatchChkGameUpdateRunner::CancelUpdateJob()
{
	if (m_updateJob != NULL)
		m_updateJob->Cancel();
	return 0;
}

/* Enqueues PatchChkDownloaderTaskJob onto CreateTask queue (0x812AE31E) */
int PatchChkGameUpdateRunner::StartDownloadTask(const BgdlPatchDownloadTaskParam &param)
{
	PatchChkDownloaderTaskJob *rawJob =
		new PatchChkDownloaderTaskJob(param, OnDownloadJobDone, this);
	m_downloadJob = rawJob;
	if (rawJob == NULL)
		return PATCH_CHK_ERROR_NO_MEMORY;

	common::SharedPtr<job::JobItem> jobItem(rawJob);
	job::JobQueue *queue = PatchChkJobQueueMgr::GetInstance()->GetQueue(PATCH_CHK_QUEUE_CREATE_TASK);
	int ret = queue->Enqueue(jobItem);
	if (ret < 0)
		m_downloadJob = NULL;
	return ret;
}

/* Detaches and cancels any running DownloaderTaskJob (0x812AE40C) */
int PatchChkGameUpdateRunner::DetachDownloadJob()
{
	if (m_downloadJob != NULL)
		m_downloadJob->DetachAndCancel();
	return 0;
}

/* Cancels any running DownloaderTaskJob while keeping finish callback (0x812AE41A) */
int PatchChkGameUpdateRunner::CancelDownloadJob()
{
	if (m_downloadJob != NULL)
		m_downloadJob->Cancel();
	return 0;
}

/* Sets update completion callback (0x812AE42A) */
int PatchChkGameUpdateRunner::SetUpdateDoneCallback(UpdateDoneCallback callback, void *userData)
{
	m_callbackUserData = userData;
	m_updateDoneCallback = callback;
	return 0;
}

/* Sets download task completion callback (0x812AE5DC) */
int PatchChkGameUpdateRunner::SetDownloadDoneCallback(DownloadDoneCallback callback, void *userData)
{
	m_callbackUserData = userData;
	m_downloadDoneCallback = callback;
	return 0;
}

/* Copies out the index-th cached package descriptor (0x812AE5C0) */
int PatchChkGameUpdateRunner::CopyPackageInfo(SceUInt32 index,
					      SceGameUpdatePackageInfo *outInfo) const
{
	sce_paf_memcpy(outInfo, &m_packages[index], sizeof(SceGameUpdatePackageInfo));
	return 0;
}

/* Main-thread completion handler for PatchChkGameUpdateJob (0x812AE438) */
void PatchChkGameUpdateRunner::OnUpdateJobDone(int result,
					       const SceGameUpdateResult *updateResult,
					       const SceGameUpdateInfo *updateInfo,
					       SceUInt64 totalSize,
					       bool hasExistingDownload,
					       void *userData)
{
	PatchChkGameUpdateRunner *self = (PatchChkGameUpdateRunner *)userData;
	self->m_lastUpdateError = result;

	if (result >= 0) {
		self->m_updateResult = *updateResult;
		if (self->m_updateResult.latestVersion[0] == '0')
			self->m_updateResult.latestVersion[0] = ' ';

		self->m_updateInfo = *updateInfo;
		self->m_totalPackageSize = totalSize;
		self->m_hasExistingDownload = hasExistingDownload;

		if (self->m_updateResult.status == SCE_GAMEUPDATE_STATUS_AVAILABLE) {
			SceUInt32 count = self->m_updateJob->GetPackageCount();
			for (SceUInt32 i = 0; i < count; ++i) {
				SceGameUpdatePackageInfo pkg;
				sce_paf_memset(&pkg, 0, sizeof(pkg));
				self->m_updateJob->CopyPackageInfo(i, &pkg);
				self->m_packages.push_back(pkg);
			}
		}
	}

	UpdateDoneCallback cb = self->m_updateDoneCallback;
	self->m_updateJob = NULL;
	if (cb != NULL)
		cb(self->m_lastUpdateError, self->m_callbackUserData);
}

/* Main-thread completion handler for PatchChkDownloaderTaskJob (0x812AE5EA) */
void PatchChkGameUpdateRunner::OnDownloadJobDone(int result, int unused,
						 SceUInt64 requiredFreeSpace,
						 void *userData)
{
	(void)unused;
	PatchChkGameUpdateRunner *self = (PatchChkGameUpdateRunner *)userData;
	self->m_requiredFreeSpace = requiredFreeSpace;
	self->m_lastDownloadError = result;

	DownloadDoneCallback cb = self->m_downloadDoneCallback;
	self->m_downloadJob = NULL;
	if (cb != NULL)
		cb(result, self->m_callbackUserData);
}
