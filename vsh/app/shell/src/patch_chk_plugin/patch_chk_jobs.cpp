/*
 * patch_chk_jobs.cpp: background paf::job::JobItem implementations for
 * patch_chk_plugin (DownLoaderTaskJob, GameUpdateJob, and CallbackJob;
 * 0x812B15F4-0x812B1B63, 0x81431C1C-0x81431C74).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include "patch_chk_internal.h"

using namespace paf;

/* PatchChkCallbackJob (0x81431C1C-0x81431C74) */
PatchChkCallbackJob::PatchChkCallbackJob(const char *name, RunCallback runCb,
					 FinishCallback finishCb, CancelCallback cancelCb,
					 void *userData)
	: job::JobItem(name, NULL),
	  m_runCallback(runCb),
	  m_finishCallback(finishCb),
	  m_cancelCallback(cancelCb),
	  m_userData(userData)
{
}

PatchChkCallbackJob::~PatchChkCallbackJob()
{
}

void PatchChkCallbackJob::Cancel()
{
	if (m_cancelCallback != NULL)
		m_cancelCallback();
}

int PatchChkCallbackJob::Run()
{
	if (m_runCallback != NULL)
		return m_runCallback(m_userData);
	return 0;
}

void PatchChkCallbackJob::Finish(int result)
{
	if (m_finishCallback != NULL)
		m_finishCallback(result, m_userData);
}

/* PatchChkDownloaderTaskJob (0x812B15F4-0x812B188F) */
PatchChkDownloaderTaskJob::PatchChkDownloaderTaskJob(const BgdlPatchDownloadTaskParam &param,
						     CompletionCallback callback,
						     void *userData)
	: job::JobItem("DownLoaderTaskJob", NULL),
	  m_userData(userData),
	  m_notifyOnFinish(true),
	  m_requiredFreeSpace(0),
	  m_callback(callback)
{
	shellBgdlPatchDownloadTaskParamClear(&m_param);
	m_param = param;
}

/* Destructor (0x812B16F8, 0x812B1802) */
PatchChkDownloaderTaskJob::~PatchChkDownloaderTaskJob()
{
	m_callback = NULL;
	m_userData = NULL;
}

/* Suppresses completion callback and requests job cancel (0x812B1812) */
void PatchChkDownloaderTaskJob::DetachAndCancel()
{
	m_notifyOnFinish = false;
	Cancel();
}

/* Submits patch download task to BGDL service (0x812B1826) */
int PatchChkDownloaderTaskJob::Run()
{
	SceUInt32 createdTaskId = 0;
	int ret = shellBgdlCreatePatchDownloadTask(&m_param, &createdTaskId);
	if (ret == 1)
		return 0;
	if (ret < 0)
		m_requiredFreeSpace = m_param.requiredFreeSpace;
	return ret;
}

/* Notifies runner on main thread if still attached (0x812B1868) */
void PatchChkDownloaderTaskJob::Finish(int result)
{
	if (m_notifyOnFinish && m_callback != NULL)
		m_callback(result, 0, m_requiredFreeSpace, m_userData);
}

/* PatchChkGameUpdateJob (0x812B1890-0x812B1B63) */
PatchChkGameUpdateJob::PatchChkGameUpdateJob(SceGameUpdateContext *context,
					     const string &titleId,
					     CompletionCallback callback,
					     void *userData)
	: job::JobItem("GameUpdateJob", NULL),
	  m_userData(userData),
	  m_notifyOnFinish(true),
	  m_totalSize(0),
	  m_titleId(titleId),
	  m_hasExistingDownload(false),
	  m_callback(callback),
	  m_context(context)
{
	sce_paf_memset(&m_updateResult, 0, sizeof(m_updateResult));
	sce_paf_memset(&m_updateInfo, 0, sizeof(m_updateInfo));
}

/* Destructor: terminates SceGameUpdateContext (0x812B1916, 0x812B198E) */
PatchChkGameUpdateJob::~PatchChkGameUpdateJob()
{
	if (m_context != NULL)
		sceGameUpdateTerm(m_context);
	m_context = NULL;
	m_callback = NULL;
	m_userData = NULL;
}

/* Aborts HTTP request and cancels the job (0x812B199E) */
void PatchChkGameUpdateJob::Cancel()
{
	if (m_context != NULL)
		sceGameUpdateAbort(m_context);
	job::JobItem::Cancel();
}

/* Suppresses finish notification and cancels the job (0x812B1812 shared logic) */
void PatchChkGameUpdateJob::DetachAndCancel()
{
	m_notifyOnFinish = false;
	Cancel();
}

/* Returns the count of packages returned by SceGameUpdate (0x812B19B2) */
SceUInt32 PatchChkGameUpdateJob::GetPackageCount() const
{
	return m_updateInfo.packageCount;
}

/* Copies the index-th package descriptor (0x812B19B8) */
int PatchChkGameUpdateJob::CopyPackageInfo(SceUInt32 index,
					   SceGameUpdatePackageInfo *outInfo) const
{
	sce_paf_memcpy(outInfo, &m_packages[index], sizeof(SceGameUpdatePackageInfo));
	return 0;
}

/* Runs SceGameUpdate query, enumerates packages, and checks BGDL status (0x812B19D6) */
int PatchChkGameUpdateJob::Run()
{
	sce_paf_memset(&m_updateResult, 0, sizeof(m_updateResult));
	sce_paf_memset(&m_updateInfo, 0, sizeof(m_updateInfo));
	m_updateResult.size = sizeof(SceGameUpdateResult);

	int ret = sceGameUpdateRunEx(m_context, &m_updateResult, &m_updateInfo);
	if (ret >= 0 && m_updateResult.status == SCE_GAMEUPDATE_STATUS_AVAILABLE) {
		if ((m_updateInfo.flags & SCE_GAMEUPDATE_INFO_FLAG_DEBUG_MOCK) == 0) {
			for (SceUInt32 i = 0; i < m_updateInfo.packageCount; ++i) {
				SceGameUpdatePackageInfo pkg;
				sce_paf_memset(&pkg, 0, sizeof(pkg));
				ret = sceGameUpdateGetPackageInfo(m_context, i, &pkg);
				if (ret < 0)
					break;
				m_packages.push_back(pkg);
				m_totalSize += pkg.packageSize;
			}
		}
		shellBgdlIsTitleDownloadingOrInstalled(m_titleId.c_str(), &m_hasExistingDownload);
	}
	return ret;
}

/* Notifies runner on main thread if still attached (0x812B1B22) */
void PatchChkGameUpdateJob::Finish(int result)
{
	if (m_notifyOnFinish && m_callback != NULL) {
		m_callback(result, &m_updateResult, &m_updateInfo, m_totalSize,
			   m_hasExistingDownload, m_userData);
	}
}
