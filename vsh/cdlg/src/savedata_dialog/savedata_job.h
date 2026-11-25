/*
 * SceSaveDataDialogPlugin: background job queue and save-data I/O jobs.
 */
#ifndef SAVEDATA_JOB_H
#define SAVEDATA_JOB_H

#include <scetypes.h>
#include <apputil.h>
#include <std/string>
#include <std/vector>
#include <std/memory>
#include <graphics/surface/surface.h>
#include <job/job.h>
#include <widget/core/widget.h>
#include <appmgr/appmgr_savedata.h>
#include "savedata_dialog_plugin.h"

struct PluginJobQueueConfig {
	void *allocator;
	void *surfacePool;
	void *decodePool;
	void *reserved;
	paf::job::JobQueue *jobQueue;
};

/*
 * Singleton job queue coordinating image-decode and save-data enumeration jobs.
 */
class PluginJobQueue {
public:
	static void Initialize(const PluginJobQueueConfig *config);
	static void Finalize(void);
	static void CancelAllJobs(void);
	static PluginJobQueue *Instance(void);

	static void EnqueueSlotJob(SceInt32 slotIndex,
		paf::common::SharedPtr<paf::job::JobItem> &job);
	static void ClearSlotJob(SceInt32 slotIndex);
	static void CancelSlotJob(SceInt32 slotIndex);

	virtual ~PluginJobQueue();

private:
	PluginJobQueue() {}
};

struct SaveDataImageRequest {
	paf::string path;
	const void *buffer;
	SceSize bufferSize;
	paf::intrusive_ptr<paf::graph::Surface> surface;
	SceInt32 state;
};

struct SaveDataImageDecodeOption {
	SceInt32 cropX;
	SceInt32 cropY;
	SceInt32 cropWidth;
	SceInt32 cropHeight;
	SceInt32 maxWidth;
	SceInt32 maxHeight;
	SceInt32 scaleMode;
};

typedef void (*SaveDataImageDecodeCallback)(SceInt32 result,
	class Job_ImageDecode *job,
	paf::intrusive_ptr<paf::graph::Surface> surface,
	void *userData);

class Job_ImageDecode : public paf::job::JobItem {
public:
	Job_ImageDecode(const paf::common::SharedPtr<SaveDataImageRequest> &request,
		paf::ui::Widget *targetWidget, SceInt32 partIndex,
		const paf::intrusive_ptr<paf::graph::Surface> &fallbackSurface,
		PluginJobQueue *queue);
	virtual ~Job_ImageDecode();

	void SetOption(const SaveDataImageDecodeOption &option);
	void SetCallback(SaveDataImageDecodeCallback callback, void *userData);
	void Enqueue(void);

	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	PluginJobQueue *m_queue;
	paf::common::SharedPtr<SaveDataImageRequest> m_request;
	paf::intrusive_ptr<paf::graph::Surface> m_fallbackSurface;
	SaveDataImageDecodeOption m_option;
	paf::intrusive_ptr<paf::graph::Surface> m_decodedSurface;
	SaveDataImageDecodeCallback m_callback;
	void *m_callbackUserData;
	paf::ui::Widget *m_targetWidget;
	SceInt32 m_partIndex;
};

/*
 * Single entry in the save data list (Vita slot or PSP directory).
 */
struct SaveDataListEntry {
	SceInt32 existState;
	SceAppUtilSaveDataSlotId slotId;
	SceInt32 status;
	SceInt32 userParam;
	SceUInt32 sizeKiB;
	SceDateTime modifiedTime;
	paf::wstring title;
	paf::wstring subTitle;
	paf::wstring detail;
	paf::wstring dirName;
	paf::string iconPath;
	const void *iconBuf;
	SceSize iconBufSize;
	SceAppUtilSaveDataSlotParam rawSlotParam;
	SceBool isEmptyCustom;
};

/*
 * Collection of Vita or PSP save data slots shown in the dialog.
 */
class SaveDataSlotList {
public:
	SaveDataSlotList();
	~SaveDataSlotList();

	void Clear(void);
	void Reserve(SceSize count);
	void AddVitaSlot(const SceAppUtilSaveDataSlot &slot);
	void AddPspDirName(const char *dirName);
	SceSize GetCount(void) const;
	SaveDataListEntry *GetEntry(SceSize index);
	const SaveDataListEntry *GetEntry(SceSize index) const;
	const char *GetPspDirName(SceSize index) const;
	void UpdateVitaEntry(SceSize index, SceInt32 existState,
		const SceAppUtilSaveDataSlotParam &slotParam);
	void UpdatePspEntry(SceSize index, SceInt32 existState,
		const SceAppUtilPspSaveDataParamSfo &paramSfo,
		const SceDateTime &modifiedTime, SceUInt32 sizeKiB,
		const char *iconPath);

	void SetLoadState(SceInt32 state) { m_loadState = state; }
	SceInt32 GetLoadState(void) const { return m_loadState; }

private:
	paf::vector<SaveDataListEntry> m_entries;
	SceInt32 m_loadState;
};

typedef void (*SaveDataInfoJobCallback)(SceInt32 result,
	const SaveDataDialogSlotResultInfo &slotInfo, void *userData);

typedef void (*SaveDataListJobCallback)(SceInt32 result,
	paf::common::SharedPtr<SaveDataSlotList> list, void *userData);

class Job_GetSaveDataInfo : public paf::job::JobItem {
public:
	Job_GetSaveDataInfo(SceAppUtilSaveDataSlotId slotId,
		const SceAppUtilMountPoint *mountPoint,
		const SceAppUtilSaveDataSlotEmptyParam *emptyParam,
		SaveDataInfoJobCallback callback, void *userData,
		PluginJobQueue *queue);
	virtual ~Job_GetSaveDataInfo();

	void Enqueue(void);
	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	PluginJobQueue *m_queue;
	SceAppUtilSaveDataSlotId m_slotId;
	SceAppUtilMountPoint m_mountPoint;
	SceBool m_hasEmptyParam;
	SceAppUtilSaveDataSlotEmptyParam m_emptyParam;
	char m_emptyTitle[64];
	char m_emptyIconPath[64];
	SaveDataInfoJobCallback m_callback;
	void *m_callbackUserData;
	SaveDataDialogSlotResultInfo m_resultInfo;
};

class Job_GetSaveDataList : public paf::job::JobItem {
public:
	Job_GetSaveDataList(const paf::common::SharedPtr<SaveDataSlotList> &list,
		const SceAppUtilMountPoint *mountPoint,
		SaveDataListJobCallback callback, void *userData,
		PluginJobQueue *queue);
	virtual ~Job_GetSaveDataList();

	void Enqueue(void);
	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	PluginJobQueue *m_queue;
	paf::common::SharedPtr<SaveDataSlotList> m_list;
	SceAppUtilMountPoint m_mountPoint;
	SaveDataListJobCallback m_callback;
	void *m_callbackUserData;
};

class Job_GetSaveDataSlotParam : public paf::job::JobItem {
public:
	Job_GetSaveDataSlotParam(SceAppUtilSaveDataSlotId slotId,
		const SceAppUtilMountPoint *mountPoint,
		SaveDataInfoJobCallback callback, void *userData,
		PluginJobQueue *queue);
	virtual ~Job_GetSaveDataSlotParam();

	void Enqueue(void);
	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	PluginJobQueue *m_queue;
	SceAppUtilSaveDataSlotId m_slotId;
	SceAppUtilMountPoint m_mountPoint;
	SaveDataInfoJobCallback m_callback;
	void *m_callbackUserData;
	SaveDataDialogSlotResultInfo m_resultInfo;
};

class Job_GetSaveDataPspInfo : public paf::job::JobItem {
public:
	Job_GetSaveDataPspInfo(const char *dirName, const char *mountPoint,
		SceUInt32 queryFlags, SaveDataInfoJobCallback callback,
		void *userData, PluginJobQueue *queue);
	virtual ~Job_GetSaveDataPspInfo();

	void Enqueue(void);
	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	PluginJobQueue *m_queue;
	char m_dirName[32];
	char m_mountPoint[16];
	SceUInt32 m_queryFlags;
	SaveDataInfoJobCallback m_callback;
	void *m_callbackUserData;
	SaveDataDialogSlotResultInfo m_resultInfo;
};

class Job_GetSaveDataPspInfoList : public paf::job::JobItem {
public:
	Job_GetSaveDataPspInfoList(const paf::common::SharedPtr<SaveDataSlotList> &list,
		const char *mountPoint, SaveDataListJobCallback callback,
		void *userData, PluginJobQueue *queue);
	virtual ~Job_GetSaveDataPspInfoList();

	void Enqueue(void);
	virtual SceInt32 Run(void);
	virtual void Finish(SceInt32 result);

private:
	PluginJobQueue *m_queue;
	paf::common::SharedPtr<SaveDataSlotList> m_list;
	char m_mountPoint[16];
	SaveDataListJobCallback m_callback;
	void *m_callbackUserData;
};

/* Storage & formatting helpers */
SceBool SaveDataIsMediaAvailable(const char *mountPoint);
SceInt32 SaveDataReadPspParamSfo(const char *dirName, const char *mountPoint,
	SceAppUtilPspSaveDataParamSfo *outSfo, SceUInt32 queryFlags);
SceInt32 SaveDataReadPspDirMetadata(const char *dirName, const char *mountPoint,
	SceDateTime *outModifiedTime, SceUInt32 *outSizeKiB,
	char *outIconPath, SceSize iconPathSize, SceUInt32 queryFlags);
void SaveDataFormatBytes(paf::wstring *outText, SceUInt64 bytes);
void SaveDataFormatModifiedTime(paf::wstring *outText, const SceDateTime &dateTime);

#endif /* SAVEDATA_JOB_H */
