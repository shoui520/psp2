/*
 * SceSaveDataDialogPlugin: PluginJobQueue, image decode job, and Vita/PSP save-data I/O jobs.
 */
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_savedata.h>
#include <apputil/apputil_internal.h>
#include <std/stdio.h>
#include <std/string.h>
#include <common/char_encoding.h>
#include <common/encoded_string.h>
#include <graphics/surface/surface.h>
#include <file/localfile.h>
#include <widget/w_plane.h>
#include "../msg_dialog/msg_dialog_util.h"
#include "savedata_job.h"

using namespace paf;

extern "C" {
int sceAppMgrGetMediaTypeFromDrive(const char *drive, SceInt32 *mediaType);
int sceAppMgrPspSaveDataGetParams(AppmgrPspSaveDataParamSfo *param);
}

static PluginJobQueue *s_jobQueueInstance = NULL;
static PluginJobQueueConfig s_jobQueueConfig;
static job::JobItem *s_activeSlotJobs[6];

static void ConvertUtf8ToWide(const char *utf8, wstring *outWide)
{
	if (utf8 == NULL || utf8[0] == '\0') {
		outWide->clear();
		return;
	}
	common::EncodedString encoded;
	ScePafMisc_C11CB5E4(&encoded, utf8, common::EncodedString::Encoding_Utf8);
	const wchar_t *wide = ScePafMisc_53948F03(&encoded);
	if (wide != NULL)
		*outWide = wide;
	else
		outWide->clear();
	ScePafMisc_B45A7521(&encoded);
}

void PluginJobQueue::Initialize(const PluginJobQueueConfig *config)
{
	if (s_jobQueueInstance == NULL) {
		s_jobQueueInstance = new PluginJobQueue();
		if (config != NULL)
			s_jobQueueConfig = *config;
		if (s_jobQueueConfig.allocator == NULL && g_saveDataDialogPlugin != NULL)
			s_jobQueueConfig.allocator = g_saveDataDialogPlugin->m_private_heap;
		if (s_jobQueueConfig.jobQueue == NULL)
			s_jobQueueConfig.jobQueue = job::JobQueue::default_queue;
	}
	sce_paf_memset(s_activeSlotJobs, 0, sizeof(s_activeSlotJobs));
}

void PluginJobQueue::CancelAllJobs(void)
{
	for (SceInt32 idx = 0; idx < 6; ++idx) {
		if (s_activeSlotJobs[idx] != NULL) {
			s_activeSlotJobs[idx]->Cancel();
			s_activeSlotJobs[idx] = NULL;
		}
	}
}

void PluginJobQueue::Finalize(void)
{
	if (s_jobQueueInstance == NULL)
		return;
	if (s_jobQueueConfig.jobQueue != NULL &&
	    s_jobQueueConfig.jobQueue != job::JobQueue::default_queue) {
		CancelAllJobs();
		s_jobQueueConfig.jobQueue->WaitEmpty();
		delete s_jobQueueConfig.jobQueue;
		s_jobQueueConfig.jobQueue = NULL;
	}
	delete s_jobQueueInstance;
	s_jobQueueInstance = NULL;
}

PluginJobQueue *PluginJobQueue::Instance(void)
{
	return s_jobQueueInstance;
}

PluginJobQueue::~PluginJobQueue()
{
}

void PluginJobQueue::EnqueueSlotJob(SceInt32 slotIndex,
	common::SharedPtr<job::JobItem> &job)
{
	if (slotIndex < 0 || slotIndex >= 6)
		return;
	if (s_activeSlotJobs[slotIndex] != NULL) {
		s_activeSlotJobs[slotIndex]->Cancel();
		s_activeSlotJobs[slotIndex] = NULL;
	}
	if (s_jobQueueConfig.jobQueue != NULL)
		s_jobQueueConfig.jobQueue->Enqueue(job);
	s_activeSlotJobs[slotIndex] = job.get();
}

void PluginJobQueue::ClearSlotJob(SceInt32 slotIndex)
{
	if (slotIndex >= 0 && slotIndex < 6)
		s_activeSlotJobs[slotIndex] = NULL;
}

void PluginJobQueue::CancelSlotJob(SceInt32 slotIndex)
{
	if (slotIndex >= 0 && slotIndex < 6 && s_activeSlotJobs[slotIndex] != NULL) {
		s_activeSlotJobs[slotIndex]->Cancel();
		s_activeSlotJobs[slotIndex] = NULL;
	}
}

Job_ImageDecode::Job_ImageDecode(const common::SharedPtr<SaveDataImageRequest> &request,
	ui::Widget *targetWidget, SceInt32 partIndex,
	const intrusive_ptr<graph::Surface> &fallbackSurface,
	PluginJobQueue *queue)
	: job::JobItem("PluginJobQueue::Job_ImageDecode", NULL)
	, m_queue(queue)
	, m_request(request)
	, m_fallbackSurface(fallbackSurface)
	, m_callback(NULL)
	, m_callbackUserData(NULL)
	, m_targetWidget(targetWidget)
	, m_partIndex(partIndex)
{
	sce_paf_memset(&m_option, 0, sizeof(m_option));
	m_option.scaleMode = 2;
}

Job_ImageDecode::~Job_ImageDecode()
{
}

void Job_ImageDecode::SetOption(const SaveDataImageDecodeOption &option)
{
	m_option = option;
}

void Job_ImageDecode::SetCallback(SaveDataImageDecodeCallback callback, void *userData)
{
	m_callback = callback;
	m_callbackUserData = userData;
}

void Job_ImageDecode::Enqueue(void)
{
	if (m_queue == NULL)
		return;
	if (m_request.get() != NULL)
		m_request->state = 1;
	common::SharedPtr<job::JobItem> self(this);
	PluginJobQueue::EnqueueSlotJob(0, self);
}

SceInt32 Job_ImageDecode::Run(void)
{
	if (IsCanceled() || m_request.get() == NULL)
		return SAVEDATA_DIALOG_ERROR_CANCELLED;

	if (m_request->buffer != NULL && m_request->bufferSize > 0) {
		intrusive_ptr<graph::Surface> surface = graph::Surface::Load(
			(graph::SurfacePool *)s_jobQueueConfig.surfacePool,
			(void *)m_request->buffer, (int64_t)m_request->bufferSize, NULL);
		if (surface.get() != NULL) {
			m_decodedSurface = surface;
			return SCE_OK;
		}
		return SAVEDATA_DIALOG_ERROR_INTERNAL;
	}

	if (m_request->path.empty())
		return SAVEDATA_DIALOG_ERROR_INTERNAL;

	SceInt32 openErr = SCE_OK;
	common::SharedPtr<File> file = LocalFile::Open(
		m_request->path.c_str(), SCE_O_RDONLY, 0, &openErr);
	if (file.get() == NULL || openErr < 0)
		return (openErr < 0) ? openErr : SAVEDATA_DIALOG_ERROR_INTERNAL;

	intrusive_ptr<graph::Surface> surface = graph::Surface::Load(
		(graph::SurfacePool *)s_jobQueueConfig.surfacePool, file, NULL);
	if (surface.get() != NULL) {
		m_decodedSurface = surface;
		return SCE_OK;
	}
	return SAVEDATA_DIALOG_ERROR_INTERNAL;
}

void Job_ImageDecode::Finish(SceInt32 result)
{
	if (!IsCanceled())
		PluginJobQueue::ClearSlotJob(0);

	if (m_request.get() != NULL) {
		if (result != SCE_OK || IsCanceled()) {
			m_request->surface = m_fallbackSurface;
			m_request->state = 3;
		} else {
			m_request->surface = m_decodedSurface;
			m_request->state = 2;
		}
	}

	if (!IsCanceled() && m_targetWidget != NULL && m_request.get() != NULL) {
		m_targetWidget->SetTexture(m_request->surface);
	}

	if (!IsCanceled() && m_callback != NULL && m_request.get() != NULL)
		m_callback(result, this, m_request->surface, m_callbackUserData);
}

SaveDataSlotList::SaveDataSlotList()
	: m_loadState(0)
{
}

SaveDataSlotList::~SaveDataSlotList()
{
	Clear();
}

void SaveDataSlotList::Clear(void)
{
	m_entries.clear();
	m_loadState = 0;
}

void SaveDataSlotList::Reserve(SceSize count)
{
	m_entries.reserve(count);
}

void SaveDataSlotList::AddVitaSlot(const SceAppUtilSaveDataSlot &slot)
{
	SaveDataListEntry entry;
	entry.existState = 0;
	entry.slotId = slot.id;
	entry.status = slot.status;
	entry.userParam = slot.userParam;
	entry.sizeKiB = 0;
	sce_paf_memset(&entry.modifiedTime, 0, sizeof(entry.modifiedTime));
	sce_paf_memset(&entry.rawSlotParam, 0, sizeof(entry.rawSlotParam));
	entry.iconBuf = NULL;
	entry.iconBufSize = 0;
	entry.isEmptyCustom = SCE_FALSE;

	if (slot.emptyParam != NULL) {
		entry.isEmptyCustom = SCE_TRUE;
		if (slot.emptyParam->title != NULL)
			ConvertUtf8ToWide((const char *)slot.emptyParam->title, &entry.title);
		if (slot.emptyParam->iconPath != NULL)
			entry.iconPath = (const char *)slot.emptyParam->iconPath;
		entry.iconBuf = slot.emptyParam->iconBuf;
		entry.iconBufSize = slot.emptyParam->iconBufSize;
	}
	m_entries.push_back(entry);
}

void SaveDataSlotList::AddPspDirName(const char *dirName)
{
	SaveDataListEntry entry;
	entry.existState = 0;
	entry.slotId = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
	entry.status = 0;
	entry.userParam = 0;
	entry.sizeKiB = 0;
	sce_paf_memset(&entry.modifiedTime, 0, sizeof(entry.modifiedTime));
	sce_paf_memset(&entry.rawSlotParam, 0, sizeof(entry.rawSlotParam));
	entry.iconBuf = NULL;
	entry.iconBufSize = 0;
	entry.isEmptyCustom = SCE_FALSE;
	if (dirName != NULL)
		ConvertUtf8ToWide(dirName, &entry.dirName);
	m_entries.push_back(entry);
}

SceSize SaveDataSlotList::GetCount(void) const
{
	return m_entries.size();
}

SaveDataListEntry *SaveDataSlotList::GetEntry(SceSize index)
{
	if (index >= m_entries.size())
		return NULL;
	return &m_entries[index];
}

const SaveDataListEntry *SaveDataSlotList::GetEntry(SceSize index) const
{
	if (index >= m_entries.size())
		return NULL;
	return &m_entries[index];
}

const char *SaveDataSlotList::GetPspDirName(SceSize index) const
{
	if (index >= m_entries.size())
		return NULL;
	return m_entries[index].iconPath.c_str();
}

void SaveDataSlotList::UpdateVitaEntry(SceSize index, SceInt32 existState,
	const SceAppUtilSaveDataSlotParam &slotParam)
{
	if (index >= m_entries.size())
		return;
	SaveDataListEntry &entry = m_entries[index];
	entry.existState = existState;
	entry.rawSlotParam = slotParam;
	if (existState == 1) {
		entry.status = slotParam.status;
		entry.userParam = slotParam.userParam;
		entry.sizeKiB = slotParam.sizeKiB;
		entry.modifiedTime = slotParam.modifiedTime;
		ConvertUtf8ToWide((const char *)slotParam.title, &entry.title);
		ConvertUtf8ToWide((const char *)slotParam.subTitle, &entry.subTitle);
		ConvertUtf8ToWide((const char *)slotParam.detail, &entry.detail);
		entry.iconPath = (const char *)slotParam.iconPath;
	}
}

void SaveDataSlotList::UpdatePspEntry(SceSize index, SceInt32 existState,
	const SceAppUtilPspSaveDataParamSfo &paramSfo,
	const SceDateTime &modifiedTime, SceUInt32 sizeKiB,
	const char *iconPath)
{
	if (index >= m_entries.size())
		return;
	SaveDataListEntry &entry = m_entries[index];
	entry.existState = existState;
	if (existState == 1) {
		ConvertUtf8ToWide((const char *)paramSfo.title, &entry.title);
		ConvertUtf8ToWide((const char *)paramSfo.saveDataTitle, &entry.subTitle);
		ConvertUtf8ToWide((const char *)paramSfo.detail, &entry.detail);
		ConvertUtf8ToWide((const char *)paramSfo.saveDataDirectory, &entry.dirName);
		entry.userParam = paramSfo.parentalLev;
		entry.sizeKiB = sizeKiB;
		entry.modifiedTime = modifiedTime;
		if (iconPath != NULL)
			entry.iconPath = iconPath;
	}
}

Job_GetSaveDataInfo::Job_GetSaveDataInfo(SceAppUtilSaveDataSlotId slotId,
	const SceAppUtilMountPoint *mountPoint,
	const SceAppUtilSaveDataSlotEmptyParam *emptyParam,
	SaveDataInfoJobCallback callback, void *userData,
	PluginJobQueue *queue)
	: job::JobItem("Job_GetSaveDataInfo", NULL)
	, m_queue(queue)
	, m_slotId(slotId)
	, m_hasEmptyParam(SCE_FALSE)
	, m_callback(callback)
	, m_callbackUserData(userData)
{
	sce_paf_memset(&m_mountPoint, 0, sizeof(m_mountPoint));
	if (mountPoint != NULL)
		m_mountPoint = *mountPoint;
	else
		sce_paf_strncpy((char *)m_mountPoint.data, "savedata0:",
			sizeof(m_mountPoint.data) - 1);

	sce_paf_memset(&m_emptyParam, 0, sizeof(m_emptyParam));
	sce_paf_memset(m_emptyTitle, 0, sizeof(m_emptyTitle));
	sce_paf_memset(m_emptyIconPath, 0, sizeof(m_emptyIconPath));
	if (emptyParam != NULL) {
		m_hasEmptyParam = SCE_TRUE;
		if (emptyParam->title != NULL) {
			sce_paf_strncpy(m_emptyTitle, (const char *)emptyParam->title,
				sizeof(m_emptyTitle) - 1);
			m_emptyParam.title = (SceChar8 *)m_emptyTitle;
		}
		if (emptyParam->iconPath != NULL) {
			sce_paf_strncpy(m_emptyIconPath, (const char *)emptyParam->iconPath,
				sizeof(m_emptyIconPath) - 1);
			m_emptyParam.iconPath = (SceChar8 *)m_emptyIconPath;
		}
		m_emptyParam.iconBuf = emptyParam->iconBuf;
		m_emptyParam.iconBufSize = emptyParam->iconBufSize;
	}
	m_resultInfo.slotId = slotId;
	m_resultInfo.status = 0;
	m_resultInfo.userParam = 0;
	m_resultInfo.sizeKiB = 0;
	m_resultInfo.existState = 0;
	sce_paf_memset(&m_resultInfo.modifiedTime, 0, sizeof(m_resultInfo.modifiedTime));
}

Job_GetSaveDataInfo::~Job_GetSaveDataInfo()
{
}

void Job_GetSaveDataInfo::Enqueue(void)
{
	if (m_queue == NULL)
		return;
	common::SharedPtr<job::JobItem> self(this);
	PluginJobQueue::EnqueueSlotJob(1, self);
}

SceInt32 Job_GetSaveDataInfo::Run(void)
{
	if (IsCanceled())
		return SAVEDATA_DIALOG_ERROR_CANCELLED;

	SceAppMgrSaveDataSlot req;
	sce_paf_memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.slotId = m_slotId;
	req.mountPoint = m_mountPoint;

	SceInt32 ret = sceAppMgrSaveDataSlotGetParam(&req);
	if (ret == SCE_OK) {
		m_resultInfo.existState = 1;
		m_resultInfo.status = req.slotParam.status;
		m_resultInfo.userParam = req.slotParam.userParam;
		m_resultInfo.sizeKiB = req.slotParam.sizeKiB;
		m_resultInfo.modifiedTime = req.slotParam.modifiedTime;
		ConvertUtf8ToWide(req.slotParam.title, &m_resultInfo.title);
		ConvertUtf8ToWide(req.slotParam.subTitle, &m_resultInfo.subTitle);
		ConvertUtf8ToWide(req.slotParam.detail, &m_resultInfo.detail);
		m_resultInfo.iconPath = req.slotParam.iconPath;
		return SCE_OK;
	}

	m_resultInfo.existState = 0;
	if (m_hasEmptyParam) {
		if (m_emptyParam.title != NULL)
			ConvertUtf8ToWide((const char *)m_emptyParam.title, &m_resultInfo.title);
		if (m_emptyParam.iconPath != NULL)
			m_resultInfo.iconPath = (const char *)m_emptyParam.iconPath;
	}
	return SCE_OK;
}

void Job_GetSaveDataInfo::Finish(SceInt32 result)
{
	if (!IsCanceled())
		PluginJobQueue::ClearSlotJob(1);
	if (m_callback != NULL) {
		SceInt32 cbResult = IsCanceled() ? 1 : result;
		m_callback(cbResult, m_resultInfo, m_callbackUserData);
	}
}

Job_GetSaveDataList::Job_GetSaveDataList(
	const common::SharedPtr<SaveDataSlotList> &list,
	const SceAppUtilMountPoint *mountPoint,
	SaveDataListJobCallback callback, void *userData,
	PluginJobQueue *queue)
	: job::JobItem("Job_GetSaveDataList", NULL)
	, m_queue(queue)
	, m_list(list)
	, m_callback(callback)
	, m_callbackUserData(userData)
{
	sce_paf_memset(&m_mountPoint, 0, sizeof(m_mountPoint));
	if (mountPoint != NULL)
		m_mountPoint = *mountPoint;
	else
		sce_paf_strncpy((char *)m_mountPoint.data, "savedata0:",
			sizeof(m_mountPoint.data) - 1);
}

Job_GetSaveDataList::~Job_GetSaveDataList()
{
}

void Job_GetSaveDataList::Enqueue(void)
{
	if (m_queue == NULL)
		return;
	if (m_list.get() != NULL)
		m_list->SetLoadState(1);
	common::SharedPtr<job::JobItem> self(this);
	PluginJobQueue::EnqueueSlotJob(2, self);
}

SceInt32 Job_GetSaveDataList::Run(void)
{
	if (m_list.get() == NULL)
		return SAVEDATA_DIALOG_ERROR_INTERNAL;

	AppmgrSaveDataSlotFileInfo fileInfo;
	sce_paf_memset(&fileInfo, 0, sizeof(fileInfo));
	SceInt32 openRet = sceAppMgrSaveDataSlotFileOpen((const char *)m_mountPoint.data, &fileInfo);
	SceSize count = m_list->GetCount();
	for (SceSize idx = 0; idx < count; ++idx) {
		if (IsCanceled()) {
			if (openRet == SCE_OK)
				sceAppMgrSaveDataSlotFileClose((const char *)m_mountPoint.data);
			return SAVEDATA_DIALOG_ERROR_CANCELLED;
		}
		SaveDataListEntry *entry = m_list->GetEntry(idx);
		if (entry == NULL)
			continue;

		SceAppMgrSaveDataSlotParam mgrParam;
		sce_paf_memset(&mgrParam, 0, sizeof(mgrParam));
		SceInt32 ret = -1;
		if (openRet == SCE_OK) {
			ret = sceAppMgrSaveDataSlotFileGetParam(entry->slotId, &mgrParam);
		} else {
			SceAppMgrSaveDataSlot req;
			sce_paf_memset(&req, 0, sizeof(req));
			req.size = sizeof(req);
			req.slotId = entry->slotId;
			req.mountPoint = m_mountPoint;
			ret = sceAppMgrSaveDataSlotGetParam(&req);
			if (ret == SCE_OK)
				mgrParam = req.slotParam;
		}

		if (ret == SCE_OK) {
			SceAppUtilSaveDataSlotParam utilParam;
			sce_paf_memset(&utilParam, 0, sizeof(utilParam));
			utilParam.status = (SceAppUtilSaveDataSlotStatus)mgrParam.status;
			sceClibMemcpy_safe(utilParam.title, mgrParam.title, sizeof(utilParam.title));
			sceClibMemcpy_safe(utilParam.subTitle, mgrParam.subTitle,
				sizeof(utilParam.subTitle));
			sceClibMemcpy_safe(utilParam.detail, mgrParam.detail,
				sizeof(utilParam.detail));
			sceClibMemcpy_safe(utilParam.iconPath, mgrParam.iconPath,
				sizeof(utilParam.iconPath));
			utilParam.userParam = mgrParam.userParam;
			utilParam.sizeKiB = mgrParam.sizeKiB;
			utilParam.modifiedTime = mgrParam.modifiedTime;
			m_list->UpdateVitaEntry(idx, 1, utilParam);
		}
	}

	if (openRet == SCE_OK)
		sceAppMgrSaveDataSlotFileClose((const char *)m_mountPoint.data);
	return SCE_OK;
}

void Job_GetSaveDataList::Finish(SceInt32 result)
{
	if (!IsCanceled())
		PluginJobQueue::ClearSlotJob(2);
	if (m_list.get() != NULL)
		m_list->SetLoadState((result != SCE_OK || IsCanceled()) ? 3 : 2);
	if (m_callback != NULL) {
		SceInt32 cbResult = IsCanceled() ? 1 : result;
		m_callback(cbResult, m_list, m_callbackUserData);
	}
}

Job_GetSaveDataSlotParam::Job_GetSaveDataSlotParam(SceAppUtilSaveDataSlotId slotId,
	const SceAppUtilMountPoint *mountPoint,
	SaveDataInfoJobCallback callback, void *userData,
	PluginJobQueue *queue)
	: job::JobItem("Job_GetSaveDataSlotParam", NULL)
	, m_queue(queue)
	, m_slotId(slotId)
	, m_callback(callback)
	, m_callbackUserData(userData)
{
	sce_paf_memset(&m_mountPoint, 0, sizeof(m_mountPoint));
	if (mountPoint != NULL)
		m_mountPoint = *mountPoint;
	else
		sce_paf_strncpy((char *)m_mountPoint.data, "savedata0:",
			sizeof(m_mountPoint.data) - 1);
	m_resultInfo.slotId = slotId;
	m_resultInfo.status = 0;
	m_resultInfo.userParam = 0;
	m_resultInfo.sizeKiB = 0;
	m_resultInfo.existState = 0;
	sce_paf_memset(&m_resultInfo.modifiedTime, 0, sizeof(m_resultInfo.modifiedTime));
}

Job_GetSaveDataSlotParam::~Job_GetSaveDataSlotParam()
{
}

void Job_GetSaveDataSlotParam::Enqueue(void)
{
	if (m_queue == NULL)
		return;
	common::SharedPtr<job::JobItem> self(this);
	PluginJobQueue::EnqueueSlotJob(3, self);
}

SceInt32 Job_GetSaveDataSlotParam::Run(void)
{
	if (IsCanceled())
		return SAVEDATA_DIALOG_ERROR_CANCELLED;

	SceAppMgrSaveDataSlot req;
	sce_paf_memset(&req, 0, sizeof(req));
	req.size = sizeof(req);
	req.slotId = m_slotId;
	req.mountPoint = m_mountPoint;

	SceInt32 ret = sceAppMgrSaveDataSlotGetParam(&req);
	if (ret == SCE_OK) {
		m_resultInfo.existState = 1;
		m_resultInfo.status = req.slotParam.status;
		m_resultInfo.userParam = req.slotParam.userParam;
		m_resultInfo.sizeKiB = req.slotParam.sizeKiB;
		m_resultInfo.modifiedTime = req.slotParam.modifiedTime;
		ConvertUtf8ToWide(req.slotParam.title, &m_resultInfo.title);
		ConvertUtf8ToWide(req.slotParam.subTitle, &m_resultInfo.subTitle);
		ConvertUtf8ToWide(req.slotParam.detail, &m_resultInfo.detail);
		m_resultInfo.iconPath = req.slotParam.iconPath;
	}
	return ret;
}

void Job_GetSaveDataSlotParam::Finish(SceInt32 result)
{
	if (!IsCanceled())
		PluginJobQueue::ClearSlotJob(3);
	if (m_callback != NULL) {
		SceInt32 cbResult = IsCanceled() ? 1 : result;
		m_callback(cbResult, m_resultInfo, m_callbackUserData);
	}
}

SceBool SaveDataIsMediaAvailable(const char *mountPoint)
{
	SceInt32 mediaType = 0;
	SceInt32 ret = sceAppMgrGetMediaTypeFromDrive(mountPoint, &mediaType);
	return (ret == SCE_OK && mediaType != 0) ? SCE_TRUE : SCE_FALSE;
}

SceInt32 SaveDataReadPspParamSfo(const char *dirName, const char *mountPoint,
	SceAppUtilPspSaveDataParamSfo *outSfo, SceUInt32 queryFlags)
{
	(void)mountPoint;
	if (dirName == NULL || outSfo == NULL)
		return SAVEDATA_DIALOG_ERROR_INTERNAL;
	if (queryFlags == 0)
		queryFlags = 0xFFu;
	if ((queryFlags & 0x8Fu) == 0)
		return SCE_OK;

	AppmgrPspSaveDataParamSfo req;
	sceClibMemset(&req, 0, sizeof(req));
	sceClibStrncpy(req.saveDataDirectory, dirName, sizeof(req.saveDataDirectory) - 1);
	SceInt32 ret = sceAppMgrPspSaveDataGetParams(&req);
	if (ret != SCE_OK)
		return ret;

	if ((queryFlags & 0x01u) != 0)
		sceClibStrncpy((char *)outSfo->title, req.title, sizeof(outSfo->title) - 1);
	if ((queryFlags & 0x02u) != 0)
		sceClibStrncpy((char *)outSfo->saveDataTitle, req.saveDataTitle,
			sizeof(outSfo->saveDataTitle) - 1);
	if ((queryFlags & 0x04u) != 0)
		sceClibStrncpy((char *)outSfo->detail, req.detail, sizeof(outSfo->detail) - 1);
	if ((queryFlags & 0x08u) != 0)
		sceClibStrncpy((char *)outSfo->saveDataDirectory, req.saveDataDirectory,
			sizeof(outSfo->saveDataDirectory) - 1);
	if ((queryFlags & 0x80u) != 0)
		outSfo->parentalLev = req.parentalLev;
	return SCE_OK;
}

SceInt32 SaveDataReadPspDirMetadata(const char *dirName, const char *mountPoint,
	SceDateTime *outModifiedTime, SceUInt32 *outSizeKiB,
	char *outIconPath, SceSize iconPathSize, SceUInt32 queryFlags)
{
	if (dirName == NULL)
		return SAVEDATA_DIALOG_ERROR_INTERNAL;
	if (queryFlags == 0)
		queryFlags = 0xFFu;

	const char *root = (mountPoint != NULL) ? mountPoint : "";
	char pathBuf[256];

	if ((queryFlags & 0x10u) != 0 && outModifiedTime != NULL) {
		sceClibMemset(pathBuf, 0, sizeof(pathBuf));
		sceClibSnprintf(pathBuf, sizeof(pathBuf), "%s/PSP/SAVEDATA/%s/%s",
			root, dirName, "PARAM.SFO");
		SceIoStat stat;
		if (sceIoGetstat(pathBuf, &stat) == SCE_OK) {
			SceRtcTick ctick, mtick;
			sceRtcGetTick(&stat.st_ctime, &ctick);
			sceRtcGetTick(&stat.st_mtime, &mtick);
			if (sceRtcCompareTick(&ctick, &mtick) > 0)
				*outModifiedTime = stat.st_ctime;
			else
				*outModifiedTime = stat.st_mtime;
		}
	}

	if ((queryFlags & 0x40u) != 0 && outIconPath != NULL && iconPathSize > 0) {
		sceClibMemset(pathBuf, 0, sizeof(pathBuf));
		sceClibSnprintf(pathBuf, sizeof(pathBuf), "%s/PSP/SAVEDATA/%s/%s",
			root, dirName, "ICON0.PNG");
		sceClibStrncpy(outIconPath, pathBuf, iconPathSize - 1);
		outIconPath[iconPathSize - 1] = '\0';
	}

	if ((queryFlags & 0x20u) != 0 && outSizeKiB != NULL) {
		sceClibMemset(pathBuf, 0, sizeof(pathBuf));
		sceClibSnprintf(pathBuf, sizeof(pathBuf), "%s/PSP/SAVEDATA/%s/%s",
			root, dirName, "");
		SceUID dirFd = sceIoDopen(pathBuf);
		if (dirFd >= 0) {
			SceIoDirent dirent;
			SceUInt32 totalBytes = 0;
			while (sceIoDread(dirFd, &dirent) > 0)
				totalBytes += (SceUInt32)dirent.d_stat.st_size;
			sceIoDclose(dirFd);
			*outSizeKiB = totalBytes >> 10;
		}
	}

	return SCE_OK;
}

Job_GetSaveDataPspInfo::Job_GetSaveDataPspInfo(const char *dirName, const char *mountPoint,
	SceUInt32 queryFlags, SaveDataInfoJobCallback callback,
	void *userData, PluginJobQueue *queue)
	: job::JobItem("Job_GetSaveDataPspInfo", NULL)
	, m_queue(queue)
	, m_queryFlags(queryFlags)
	, m_callback(callback)
	, m_callbackUserData(userData)
{
	sce_paf_memset(m_dirName, 0, sizeof(m_dirName));
	sce_paf_memset(m_mountPoint, 0, sizeof(m_mountPoint));
	if (dirName != NULL)
		sce_paf_strncpy(m_dirName, dirName, sizeof(m_dirName) - 1);
	if (mountPoint != NULL)
		sce_paf_strncpy(m_mountPoint, mountPoint, sizeof(m_mountPoint) - 1);
	m_resultInfo.slotId = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
	m_resultInfo.status = 0;
	m_resultInfo.userParam = 0;
	m_resultInfo.sizeKiB = 0;
	m_resultInfo.existState = 0;
	sce_paf_memset(&m_resultInfo.modifiedTime, 0, sizeof(m_resultInfo.modifiedTime));
}

Job_GetSaveDataPspInfo::~Job_GetSaveDataPspInfo()
{
}

void Job_GetSaveDataPspInfo::Enqueue(void)
{
	if (m_queue == NULL)
		return;
	common::SharedPtr<job::JobItem> self(this);
	PluginJobQueue::EnqueueSlotJob(4, self);
}

SceInt32 Job_GetSaveDataPspInfo::Run(void)
{
	if (IsCanceled())
		return SAVEDATA_DIALOG_ERROR_CANCELLED;

	SceAppUtilPspSaveDataParamSfo sfo;
	sceClibMemset(&sfo, 0, sizeof(sfo));
	if (MsgDialogGetSdkLevel() >= 4)
		sceClibStrncpy((char *)sfo.saveDataDirectory, m_dirName,
			sizeof(sfo.saveDataDirectory) - 1);

	SceInt32 sfoRet = SaveDataReadPspParamSfo(m_dirName, m_mountPoint, &sfo, m_queryFlags);
	SceDateTime modTime;
	sceClibMemset(&modTime, 0, sizeof(modTime));
	SceUInt32 sizeKiB = 0;
	char iconPath[64];
	sceClibMemset(iconPath, 0, sizeof(iconPath));
	SceInt32 metaRet = SaveDataReadPspDirMetadata(m_dirName, m_mountPoint,
		&modTime, &sizeKiB, iconPath, sizeof(iconPath), m_queryFlags);

	if ((sfoRet | metaRet) == 0) {
		m_resultInfo.existState = 1;
		ConvertUtf8ToWide((const char *)sfo.title, &m_resultInfo.title);
		ConvertUtf8ToWide((const char *)sfo.saveDataTitle, &m_resultInfo.subTitle);
		ConvertUtf8ToWide((const char *)sfo.detail, &m_resultInfo.detail);
		ConvertUtf8ToWide((const char *)sfo.saveDataDirectory, &m_resultInfo.dirName);
		m_resultInfo.userParam = sfo.parentalLev;
		m_resultInfo.modifiedTime = modTime;
		m_resultInfo.sizeKiB = sizeKiB;
		m_resultInfo.iconPath = iconPath;
		return SCE_OK;
	}

	m_resultInfo.existState = 0;
	ConvertUtf8ToWide(m_dirName, &m_resultInfo.dirName);
	return metaRet;
}

void Job_GetSaveDataPspInfo::Finish(SceInt32 result)
{
	if (!IsCanceled())
		PluginJobQueue::ClearSlotJob(4);
	if (m_callback != NULL) {
		SceInt32 cbResult = IsCanceled() ? 1 : result;
		m_callback(cbResult, m_resultInfo, m_callbackUserData);
	}
}

Job_GetSaveDataPspInfoList::Job_GetSaveDataPspInfoList(
	const common::SharedPtr<SaveDataSlotList> &list,
	const char *mountPoint, SaveDataListJobCallback callback,
	void *userData, PluginJobQueue *queue)
	: job::JobItem("Job_GetSaveDataPspInfoList", NULL)
	, m_queue(queue)
	, m_list(list)
	, m_callback(callback)
	, m_callbackUserData(userData)
{
	sce_paf_memset(m_mountPoint, 0, sizeof(m_mountPoint));
	if (mountPoint != NULL)
		sce_paf_strncpy(m_mountPoint, mountPoint, sizeof(m_mountPoint) - 1);
}

Job_GetSaveDataPspInfoList::~Job_GetSaveDataPspInfoList()
{
}

void Job_GetSaveDataPspInfoList::Enqueue(void)
{
	if (m_queue == NULL)
		return;
	if (m_list.get() != NULL)
		m_list->SetLoadState(1);
	common::SharedPtr<job::JobItem> self(this);
	PluginJobQueue::EnqueueSlotJob(5, self);
}

SceInt32 Job_GetSaveDataPspInfoList::Run(void)
{
	if (m_list.get() == NULL)
		return SAVEDATA_DIALOG_ERROR_INTERNAL;

	SceSize count = m_list->GetCount();
	for (SceSize idx = 0; idx < count; ++idx) {
		if (IsCanceled())
			return SAVEDATA_DIALOG_ERROR_CANCELLED;
		SaveDataListEntry *entry = m_list->GetEntry(idx);
		if (entry == NULL)
			continue;

		common::EncodedString dirUtf8;
		ScePafMisc_7709D1B2(&dirUtf8, entry->dirName, common::EncodedString::Encoding_Utf16);
		const char *dirName = ScePafMisc_A7C6AF5C(&dirUtf8);
		SceAppUtilPspSaveDataParamSfo sfo;
		sceClibMemset(&sfo, 0, sizeof(sfo));
		SceDateTime modTime;
		sceClibMemset(&modTime, 0, sizeof(modTime));
		SceUInt32 sizeKiB = 0;
		char iconPath[64];
		sceClibMemset(iconPath, 0, sizeof(iconPath));

		if (dirName != NULL &&
		    SaveDataReadPspParamSfo(dirName, m_mountPoint, &sfo, 0x7Bu) == SCE_OK &&
		    SaveDataReadPspDirMetadata(dirName, m_mountPoint, &modTime, &sizeKiB,
			iconPath, sizeof(iconPath), 0x7Bu) == SCE_OK) {
			m_list->UpdatePspEntry(idx, 1, sfo, modTime, sizeKiB, iconPath);
		}
		ScePafMisc_B45A7521(&dirUtf8);
	}
	return SCE_OK;
}

void Job_GetSaveDataPspInfoList::Finish(SceInt32 result)
{
	if (!IsCanceled())
		PluginJobQueue::ClearSlotJob(5);
	if (m_list.get() != NULL)
		m_list->SetLoadState((result != SCE_OK || IsCanceled()) ? 3 : 2);
	if (m_callback != NULL) {
		SceInt32 cbResult = IsCanceled() ? 1 : result;
		m_callback(cbResult, m_list, m_callbackUserData);
	}
}

void SaveDataFormatBytes(wstring *outText, SceUInt64 bytes)
{
	wchar_t textBuf[16];
	if (bytes > 0x9C3C00ULL) {
		if (bytes > 0x270F00000ULL)
			sce_paf_swprintf(textBuf, 16, L"%llu GB", bytes >> 30);
		else
			sce_paf_swprintf(textBuf, 16, L"%llu MB", bytes >> 20);
	} else {
		sce_paf_swprintf(textBuf, 16, L"%llu KB", bytes >> 10);
	}
	*outText = textBuf;
}

void SaveDataFormatModifiedTime(wstring *outText, const SceDateTime &dateTime)
{
	wchar_t textBuf[32];
	sce_paf_swprintf(textBuf, 32, L"%04u/%02u/%02u %02u:%02u",
		(unsigned int)dateTime.year, (unsigned int)dateTime.month,
		(unsigned int)dateTime.day, (unsigned int)dateTime.hour,
		(unsigned int)dateTime.minute);
	*outText = textBuf;
}
