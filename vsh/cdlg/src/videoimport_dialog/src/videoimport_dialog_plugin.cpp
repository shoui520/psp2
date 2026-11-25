/*
 * SceVideoImportDialogPlugin (vs0:sys/external/libcdlg_videoimport.suprx):
 * plugin entry points, CommonDialogMain work-memory bridge,
 * SceVideoImportDialogCtrl, and SceVideoImportVideoGetResultJob.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <videoimport_dialog.h>
#include <std/string.h>
#include <std/stdio.h>
#include <math/ngp/v4.h>
#include <framework.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include "../../msg_dialog/msg_dialog_util.h"
#include "videoimport_dialog_plugin.h"

extern "C" {
#include "../../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

extern "C" void *__dso_handle;
void *__dso_handle;

extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

extern "C" {
int sceAppMgrMmsMount(int id, char *mountPoint);
int sceAppMgrUmount(const char *mountPoint);
int sceSysmoduleIsLoadedInternal(SceUInt32 id);
int sceSysmoduleLoadModuleInternalWithArg(SceUInt32 id, SceSize args, void *argp,
					  SceUInt32 flags, int *status);
int sceSysmoduleUnloadModuleInternalWithArg(SceUInt32 id, SceSize args, void *argp,
					    SceUInt32 flags, int *status);
}

static const SceUInt32 kRequiredInternalModules[3] = {
	0x80000009u,
	0x80000018u,
	0x80000024u
};

static SceVideoImportDialogCtrl *s_controller = NULL;
static SceInt32 s_sysmoduleError = SCE_OK;

static void videoImportPluginSetParam(Plugin::InitParam2 *param);
static void videoImportPluginInit(Plugin *plugin);
static void videoImportPluginStart(Plugin *plugin);
static void videoImportPluginStop(Plugin *plugin);
static void videoImportPluginExit(Plugin *plugin);

static Plugin::ModuleInterface s_moduleInterface = {
	videoImportPluginSetParam,
	videoImportPluginInit,
	videoImportPluginStart,
	videoImportPluginStop,
	videoImportPluginExit
};

class VideoImportApiScope {
public:
	VideoImportApiScope(const char *apiName, SceInt32 &err)
		: m_work(NULL),
		  m_apiName(apiName)
	{
		err = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_VIDEOIMPORT);
		if (err != SCE_OK)
			return;
		m_work = (CommonDialogVideoImportParam *)
			SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_VIDEOIMPORT);
		if (m_work == NULL) {
			err = VIDEOIMPORT_DIALOG_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_VIDEOIMPORT);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		err = SCE_OK;
	}

	~VideoImportApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_VIDEOIMPORT);
		}
	}

	CommonDialogVideoImportParam *work() const
	{
		return m_work;
	}

private:
	CommonDialogVideoImportParam *m_work;
	const char *m_apiName;
};

static void OnUpdate(void *userData);

static SceInt32 GetImportParam(SceVideoImportDialogParam *outParam)
{
	SceInt32 ret = SCE_OK;
	VideoImportApiScope scope("GetImportParam", ret);
	CommonDialogVideoImportParam *work = scope.work();
	if (ret == SCE_OK && work != NULL && outParam != NULL)
		sce_paf_memcpy(outParam, &work->param, sizeof(SceVideoImportDialogParam));
	return ret;
}

static SceInt32 GetCommonParam(SceInt32 *outDrawPriority, SceCommonDialogColor *outBgColor)
{
	SceInt32 ret = SCE_OK;
	VideoImportApiScope scope("GetCommonParam", ret);
	CommonDialogVideoImportParam *work = scope.work();
	if (ret == SCE_OK && work != NULL) {
		if (outDrawPriority != NULL)
			*outDrawPriority = work->drawPriority;
		if (outBgColor != NULL)
			*outBgColor = work->bgColor;
	}
	return ret;
}

static void VideoImportDialogCallback(const SceVideoImportDialogResult *result, void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	VideoImportApiScope scope("VideoImportDialogCallback", ret);
	CommonDialogVideoImportParam *work = scope.work();
	if (ret == SCE_OK && work != NULL && result != NULL) {
		sce_paf_memcpy(&work->result, result, sizeof(SceVideoImportDialogResult));
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_VIDEOIMPORT);
	}
}

static void OnUpdate(void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	VideoImportApiScope scope("OnUpdate", ret);
	CommonDialogVideoImportParam *work = scope.work();
	if (ret == SCE_OK && work != NULL) {
		if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_VIDEOIMPORT)) {
			SceVideoImportDialogResult abortResult;
			sce_paf_memset(&abortResult, 0, sizeof(abortResult));
			abortResult.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
			sce_paf_memcpy(&work->result, &abortResult, sizeof(abortResult));
			common::MainThreadCallList::Unregister(OnUpdate, NULL);
			SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_VIDEOIMPORT);
		}
	}
}

static SceInt32 LoadRequiredInternalModules(bool standalone)
{
	Framework *fw = Framework::Instance();
	SceInt32 mode = (fw != NULL) ? fw->GetMode() : 0;
	for (SceUInt32 i = 0; i < 3; i++) {
		SceUInt32 modId = kRequiredInternalModules[i];
		if (mode == 5) {
			int status = 0;
			sceSysmoduleLoadModuleInternalWithArg(modId, 0, NULL, 16, &status);
		} else if (!standalone) {
			SceInt32 loaded = sceSysmoduleIsLoadedInternal(modId);
			if (loaded < 0) {
				s_sysmoduleError = loaded;
				return loaded;
			}
		}
	}
	s_sysmoduleError = SCE_OK;
	return SCE_OK;
}

static void UnloadRequiredInternalModules(void)
{
	Framework *fw = Framework::Instance();
	SceInt32 mode = (fw != NULL) ? fw->GetMode() : 0;
	if (mode != 5)
		return;
	for (SceInt32 i = 2; i >= 0; i--) {
		int status = 0;
		sceSysmoduleUnloadModuleInternalWithArg(kRequiredInternalModules[i], 0, NULL, 0,
							&status);
	}
}

static void OnVideoSelected(SceInt64 mrid, void *userData)
{
	SceVideoImportDialogCtrl *ctrl = (SceVideoImportDialogCtrl *)userData;
	if (ctrl != NULL)
		ctrl->StartGetResultJob(mrid);
}

SceVideoImportVideoGetResultJob::SceVideoImportVideoGetResultJob(
	VideoDBManager *dbManager,
	const SceVideoImportDialogParam *param,
	SceVideoImportDialogCtrl *controller,
	SceInt64 selectedMrid)
	: common::SlotJobItem("SceVideoImportVideoGetResultJob", NULL),
	  m_dbManager(dbManager),
	  m_param(param),
	  m_controller(controller),
	  m_selectedMrid(selectedMrid)
{
	sce_paf_memset(&m_result, 0, sizeof(m_result));
	m_result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
}

SceVideoImportVideoGetResultJob::~SceVideoImportVideoGetResultJob()
{
}

SceInt32 SceVideoImportVideoGetResultJob::Run()
{
	if (IsCanceled() || m_dbManager == NULL || m_param == NULL)
		return VIDEOIMPORT_DIALOG_ERROR_DB_QUERY;

	VideoRecordInfo info;
	SceInt32 ret = m_dbManager->QueryVideoDetail(m_selectedMrid, info);
	if (ret < 0)
		return VIDEOIMPORT_DIALOG_ERROR_DB_QUERY;

	if (m_param->itemData != NULL && m_param->itemCount > 0) {
		SceVideoImportDialogItemData *item = &m_param->itemData[0];
		sce_paf_memset(item, 0, sizeof(SceVideoImportDialogItemData));

		paf::string mappedPath = info.contentPath;
		const char *uxPrefix = "ux0:/video";
		size_t prefixLen = sce_paf_strlen(uxPrefix);
		if (sce_paf_strncmp(mappedPath.c_str(), uxPrefix, prefixLen) == 0) {
			mappedPath = paf::string("video0:") + (mappedPath.c_str() + prefixLen);
		}

		sce_paf_strncpy((char *)item->fileName, mappedPath.c_str(),
				SCE_VIDEOIMPORT_DIALOG_MAX_FS_PATH - 1);
		sce_paf_strncpy((char *)item->videoTitle, info.title.c_str(),
				SCE_VIDEOIMPORT_DIALOG_MAX_VIDEO_TITLE_SIZE - 1);
		item->width = info.width;
		item->height = info.height;
		item->fileSize = (SceUInt32)info.fileSize;
		item->duration = info.duration;
		item->updated = info.lastUpdatedTime;
		sce_paf_strncpy((char *)item->titleId, info.appliTitleId.c_str(),
				SCE_VIDEOIMPORT_DIALOG_TITLE_ID_DATA_SIZE - 1);
	}

	m_result.result = SCE_COMMON_DIALOG_RESULT_OK;
	m_result.importedItemNum = 1;
	return SCE_OK;
}

void SceVideoImportVideoGetResultJob::Finish(SceInt32 result)
{
	if (IsCanceled() || m_controller == NULL)
		return;
	if (result == SCE_OK)
		m_controller->NotifyResult(&m_result);
	else
		m_controller->ShowErrorDialog(result);
}

SceVideoImportDialogCtrl::SceVideoImportDialogCtrl(Plugin *plugin)
	: m_plugin(plugin),
	  m_dbManager(),
	  m_listView(plugin),
	  m_resultSlots(),
	  m_callback(NULL),
	  m_callbackUserData(NULL),
	  m_standalone(false),
	  m_mounted(false)
{
	m_resultSlots.Grow(1);
	sce_paf_memset(m_mountPoint, 0, sizeof(m_mountPoint));
	sce_paf_memset(&m_param, 0, sizeof(m_param));
	sce_paf_memset(&m_result, 0, sizeof(m_result));
	m_result.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
	m_listView.SetSelectCallback(OnVideoSelected, this);
}

SceVideoImportDialogCtrl::~SceVideoImportDialogCtrl()
{
	m_resultSlots.Cancel(0);
	m_listView.Close();
	m_dbManager.Close();
}

SceInt32 SceVideoImportDialogCtrl::SetCommonParam(SceInt32 drawPriority,
						  const SceCommonDialogColor *bgColor)
{
	math::v4 color(0.149f, 0.157f, 0.157f, 1.0f);
	if (bgColor != NULL) {
		color = math::v4((float)bgColor->r / 255.0f,
				 (float)bgColor->g / 255.0f,
				 (float)bgColor->b / 255.0f,
				 (float)bgColor->a / 255.0f);
	}
	m_listView.SetDisplayOption(drawPriority, color);
	return SCE_OK;
}

SceInt32 SceVideoImportDialogCtrl::SetImportParam(const SceVideoImportDialogParam *param,
						  VideoImportResultCallback callback,
						  void *userData)
{
	if (param != NULL) {
		sce_paf_memcpy(&m_param, param, sizeof(SceVideoImportDialogParam));
		if (m_param.itemCount > SCE_VIDEOIMPORT_DIALOG_MAX_ITEM_NUM)
			m_param.itemCount = SCE_VIDEOIMPORT_DIALOG_MAX_ITEM_NUM;
		SceInt32 selectMode = (m_param.mode == 1) ? 1 : 0;
		m_listView.SetSelectionOption(selectMode, m_param.visibleCategory,
					      m_param.filterType);
	}
	m_callback = callback;
	m_callbackUserData = userData;
	return SCE_OK;
}

SceInt32 SceVideoImportDialogCtrl::MountVideoDb(paf::string &outDbPath)
{
	SceInt32 ret = sceAppMgrMmsMount(402, m_mountPoint);
	if (ret == SCE_OK) {
		m_mounted = true;
		if (sce_paf_strlen(m_mountPoint) > 0) {
			outDbPath = paf::string(m_mountPoint) + "AVContent.db";
		}
	}
	return ret;
}

SceInt32 SceVideoImportDialogCtrl::InitStandaloneParam(Plugin *plugin)
{
	m_standalone = true;
	SceInt32 selectMode = (SceInt32)plugin->GetArg("select_mode", NULL);
	SceInt32 category = (SceInt32)plugin->GetArg("category", NULL);
	SceInt32 itemDataAddr = (SceInt32)plugin->GetArg("item_data", NULL);

	SceVideoImportDialogParam param;
	sce_paf_memset(&param, 0, sizeof(param));
	param.sdkVersion = 0x03650011u;
	param.mode = selectMode;
	param.visibleCategory = (SceUInt32)category;
	param.itemCount = 1;
	param.itemData = (SceVideoImportDialogItemData *)(SceUIntPtr)itemDataAddr;
	return SetImportParam(&param, NULL, NULL);
}

SceInt32 SceVideoImportDialogCtrl::OnInit(Plugin *plugin)
{
	SceInt32 processType = (SceInt32)plugin->GetArg("process_type", NULL);
	if (processType == 0x10000)
		InitStandaloneParam(plugin);

	if (s_sysmoduleError == SCE_OK) {
		paf::string mountDbPath;
		MountVideoDb(mountDbPath);
		m_dbManager.Open(mountDbPath, paf::string("ux0:/mms/video/AVContent.db"), 30000);
		m_listView.SetDBManager(&m_dbManager);
	}
	return SCE_OK;
}

SceInt32 SceVideoImportDialogCtrl::OnStart(Plugin *plugin)
{
	(void)plugin;
	if (s_sysmoduleError != SCE_OK) {
		ShowErrorDialog(s_sysmoduleError);
		return SCE_OK;
	}
	if (!m_mounted || !m_dbManager.IsOpen()) {
		ShowErrorDialog(VIDEOIMPORT_DIALOG_ERROR_NO_VIDEO);
		return SCE_OK;
	}
	return m_listView.Open();
}

SceInt32 SceVideoImportDialogCtrl::OnStop(Plugin *plugin)
{
	(void)plugin;
	if (s_sysmoduleError == SCE_OK)
		m_listView.Close();
	return SCE_OK;
}

SceInt32 SceVideoImportDialogCtrl::OnExit(Plugin *plugin)
{
	(void)plugin;
	if (s_sysmoduleError == SCE_OK) {
		m_dbManager.Close();
		if (m_mounted) {
			sceAppMgrUmount(m_mountPoint);
			m_mounted = false;
		}
	}
	return SCE_OK;
}

SceInt32 SceVideoImportDialogCtrl::StartGetResultJob(SceInt64 mrid)
{
	SceVideoImportVideoGetResultJob *job =
		new SceVideoImportVideoGetResultJob(&m_dbManager, &m_param, this, mrid);
	if (job == NULL)
		return -1;
	common::SharedPtr<job::JobItem> item(job);
	return m_resultSlots.Enqueue(0, job::JobQueue::default_queue, item);
}

void SceVideoImportDialogCtrl::NotifyResult(const SceVideoImportDialogResult *result)
{
	if (result != NULL)
		m_result = *result;
	if (m_callback != NULL)
		m_callback(&m_result, m_callbackUserData);
}

void SceVideoImportDialogCtrl::ShowErrorDialog(SceInt32 errorCode)
{
	(void)errorCode;
	SceVideoImportDialogResult cancelResult;
	sce_paf_memset(&cancelResult, 0, sizeof(cancelResult));
	cancelResult.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
	NotifyResult(&cancelResult);
}

static void videoImportPluginSetParam(Plugin::InitParam2 *param)
{
	(void)param;
}

static void videoImportPluginInit(Plugin *plugin)
{
	SceInt32 processType = (SceInt32)plugin->GetArg("process_type", NULL);
	LoadRequiredInternalModules(processType == 0x10000);

	s_controller = new SceVideoImportDialogCtrl(plugin);
	if (s_controller != NULL) {
		SceInt32 drawPriority = 0;
		SceCommonDialogColor bgColor;
		bgColor.r = 38;
		bgColor.g = 40;
		bgColor.b = 40;
		bgColor.a = 255;

		if (processType == 0) {
			SceVideoImportDialogParam param;
			sce_paf_memset(&param, 0, sizeof(param));
			GetCommonParam(&drawPriority, &bgColor);
			GetImportParam(&param);
			s_controller->SetImportParam(&param, VideoImportDialogCallback, NULL);
		}
		s_controller->SetCommonParam(drawPriority, &bgColor);
		s_controller->OnInit(plugin);
	}
}

static void videoImportPluginStart(Plugin *plugin)
{
	if (s_controller != NULL)
		s_controller->OnStart(plugin);

	SceInt32 processType = (SceInt32)plugin->GetArg("process_type", NULL);
	if (processType == 0) {
		common::MainThreadCallList::Register(OnUpdate, NULL);
		SceCommonDialogMain_877BB562();
	}
}

static void videoImportPluginStop(Plugin *plugin)
{
	if (s_controller != NULL)
		s_controller->OnStop(plugin);
}

static void videoImportPluginExit(Plugin *plugin)
{
	if (s_controller != NULL) {
		s_controller->OnExit(plugin);
		delete s_controller;
		s_controller = NULL;
	}
	UnloadRequiredInternalModules();
}

static SceInt32 videoImportModuleStart(SceSize arglen, const void *argp)
{
	(void)arglen;
	__dso_handle = &__dso_handle;
	for (void (**fn)(void) = __start__Zpreinit_array; fn < __stop__Zpreinit_array; fn++) {
		if (*fn != NULL)
			(*fn)();
	}
	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; fn++) {
		if (*fn != NULL)
			(*fn)();
	}
	Module *module = *(Module *const *)argp;
	module->SetInterface(0, &s_moduleInterface);
	return SCE_KERNEL_START_SUCCESS;
}

static SceInt32 videoImportModuleStop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	__cxa_finalize(&__dso_handle);
	return SCE_KERNEL_STOP_SUCCESS;
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	videoImportModuleStart(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	videoImportModuleStop(arglen, argp);
	return SCE_KERNEL_STOP_SUCCESS;
}
