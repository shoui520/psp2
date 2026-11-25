/*
 * SceSaveDataDialogPlugin (vs0:sys/external/libcdlg_savedata.suprx):
 * plugin lifecycle, work-memory polling, parameter validation, and result dispatch.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <common_dialog/error.h>
#include <apputil/apputil_internal.h>
#include <std/string.h>
#include <std/string>
#include <plugin.h>
#include <module/module.h>
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include <common/encoded_string.h>
#include <widget/core/event.h>
#include "savedata_dialog_plugin.h"
#include "savedata_job.h"
#include "savedata_dialog_ctrl.h"

extern "C" {
#include "../../../paf/src/std/stdc++/atexit.h"
}

using namespace paf;

extern "C" void *__dso_handle;
void *__dso_handle;

extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

const char paf::s_empty_name[] = "";

Plugin *g_saveDataDialogPlugin = NULL;
SceInt32 g_saveDataParamErrorCode = 0;

static Plugin::ModuleInterface s_moduleInterface;

static InitEventFunctionEntry s_initEventTable[] = {
	{IDParam((uint32_t)0x5FCC2ECEu), NULL},
	{IDParam((uint32_t)0x4C944253u), NULL},
	{IDParam((uint32_t)0u), NULL}
};

/*
 * Locks SceCommonDialogMain's save-data work memory under an API tag name
 * and releases it when leaving scope.
 */
class SaveDataDialogPluginApiScope {
public:
	SaveDataDialogPluginApiScope(const char *apiName, SceInt32 &ret)
		: m_apiName(apiName), m_work(NULL)
	{
		ret = SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_SAVEDATA);
		if (ret != SCE_OK)
			return;
		m_work = (CommonDialogSaveDataParam *)SceCommonDialogMain_DD2CAC30(
			COMMON_DIALOG_TYPE_SAVEDATA);
		if (m_work == NULL) {
			ret = SAVEDATA_DIALOG_ERROR_NO_WORK_MEMORY;
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_SAVEDATA);
			return;
		}
		sce_paf_strncpy(m_work->apiName, apiName, sizeof(m_work->apiName));
		m_work->apiName[sizeof(m_work->apiName) - 1] = '\0';
		ret = SCE_OK;
	}

	~SaveDataDialogPluginApiScope()
	{
		if (m_work == NULL)
			return;
		if (sce_paf_strncmp(m_work->apiName, m_apiName, sizeof(m_work->apiName)) == 0) {
			sce_paf_memset(m_work->apiName, 0, sizeof(m_work->apiName));
			SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_SAVEDATA);
		}
	}

	CommonDialogSaveDataParam *work(void) const
	{
		return m_work;
	}

private:
	const char *m_apiName;
	CommonDialogSaveDataParam *m_work;
};

static void saveDataDialogInit(SceSize argSize, const void *argp);
static void saveDataDialogTerm(void);
static void saveDataDialogPluginInit(Plugin *plugin);
static void saveDataDialogPluginStart(Plugin *plugin);
static void saveDataDialogPluginStop(Plugin *plugin);
static void saveDataDialogPluginExit(Plugin *plugin);
static void saveDataDialogPluginSetParam(Plugin::InitParam2 *param);
static void OnUpdate(void *userData);
static int OnSaveDataSlotChanged(int event, SceAppUtilSaveDataSlotId slotId, void *arg);
static void SaveDataDialogCallback(SceInt32 result, SceSaveDataDialogMode mode,
	SceInt32 buttonAction, const SaveDataDialogSlotResultInfo *slotInfo, void *userData);

extern "C" int module_start(SceSize arglen, const void *argp)
{
	saveDataDialogInit(arglen, argp);
	return SCE_KERNEL_START_SUCCESS;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	saveDataDialogTerm();
	return SCE_KERNEL_STOP_SUCCESS;
}

static void saveDataDialogInit(SceSize argSize, const void *argp)
{
	(void)argSize;
	__dso_handle = &__dso_handle;
	for (void (**fn)(void) = __start__Zpreinit_array; fn < __stop__Zpreinit_array; ++fn) {
		if (*fn != NULL)
			(*fn)();
	}
	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; ++fn) {
		if (*fn != NULL)
			(*fn)();
	}

	Module *module = (argp != NULL) ? *(Module *const *)argp : NULL;
	s_moduleInterface.set_param_func = saveDataDialogPluginSetParam;
	s_moduleInterface.init_func = saveDataDialogPluginInit;
	s_moduleInterface.start_func = saveDataDialogPluginStart;
	s_moduleInterface.stop_func = saveDataDialogPluginStop;
	s_moduleInterface.exit_func = saveDataDialogPluginExit;
	if (module != NULL)
		module->SetInterface(0, &s_moduleInterface);
}

static void saveDataDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

static void ResetResultBuffers(CommonDialogSaveDataParam *work)
{
	sce_paf_memset(&work->result, 0, sizeof(work->result));
	work->result.slotId = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
	sce_paf_memset(&work->slotInfo, 0, sizeof(work->slotInfo));
	sce_paf_memset(&work->slotParam, 0, sizeof(work->slotParam));
	work->result.slotInfo = &work->slotInfo;
	work->slotInfo.slotParam = &work->slotParam;

	sce_paf_memset(&work->paramSfo, 0, sizeof(work->paramSfo));
	sce_paf_memset(&work->pspResult, 0, sizeof(work->pspResult));
	sce_paf_memset(&work->pspResult.dirName, 0, sizeof(work->pspResult.dirName));
	work->pspResult.paramSfo = &work->paramSfo;
	work->pspResult.buttonId = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
}

static void CopyWideToUtf8Buffer(const wstring &src, char *dst, SceSize dstLen)
{
	common::EncodedString encoded;
	ScePafMisc_7709D1B2(&encoded, src, common::EncodedString::Encoding_Utf16);
	const char *utf8 = ScePafMisc_A7C6AF5C(&encoded);
	if (utf8 != NULL)
		sce_paf_strncpy(dst, utf8, dstLen);
	ScePafMisc_B45A7521(&encoded);
}

static void CopyStringToUtf8Buffer(const string &src, char *dst, SceSize dstLen)
{
	common::EncodedString encoded;
	ScePafMisc_09BAD2FF(&encoded, src, common::EncodedString::Encoding_Utf8);
	const char *utf8 = ScePafMisc_A7C6AF5C(&encoded);
	if (utf8 != NULL)
		sce_paf_strncpy(dst, utf8, dstLen);
	ScePafMisc_B45A7521(&encoded);
}

static void WritePspResult(CommonDialogSaveDataParam *work, SceInt32 status,
	SceSaveDataDialogMode mode, SceInt32 buttonAction,
	const SaveDataDialogSlotResultInfo *slotInfo)
{
	ResetResultBuffers(work);
	work->pspResult.mode = mode;
	work->pspResult.userdata = work->pspParam.userdata;

	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_SAVEDATA)) {
		work->pspResult.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		return;
	}

	if (status < 0 && status != SAVEDATA_DIALOG_ERROR_PARAM) {
		work->pspResult.result = SAVEDATA_DIALOG_ERROR_COMMON_GENERAL;
		return;
	}

	work->pspResult.result = status;
	if (status < 0 || status == SCE_COMMON_DIALOG_RESULT_ABORTED || slotInfo == NULL)
		return;
	if (buttonAction == 3 && (mode == SCE_SAVEDATA_DIALOG_MODE_LIST ||
				  mode == SCE_SAVEDATA_DIALOG_MODE_FIXED))
		return;

	work->pspResult.isExist = (slotInfo->existState == 1) ? SCE_TRUE : SCE_FALSE;
	CopyWideToUtf8Buffer(slotInfo->dirName, (char *)work->pspResult.dirName.data,
		sizeof(work->pspResult.dirName.data) - 1);

	if (slotInfo->existState != 0 && work->pspResult.paramSfo != NULL) {
		CopyWideToUtf8Buffer(slotInfo->title, (char *)work->pspResult.paramSfo->title,
			sizeof(work->pspResult.paramSfo->title) - 1);
		CopyWideToUtf8Buffer(slotInfo->subTitle,
			(char *)work->pspResult.paramSfo->saveDataTitle,
			sizeof(work->pspResult.paramSfo->saveDataTitle) - 1);
		CopyWideToUtf8Buffer(slotInfo->detail, (char *)work->pspResult.paramSfo->detail,
			sizeof(work->pspResult.paramSfo->detail) - 1);
		CopyWideToUtf8Buffer(slotInfo->dirName,
			(char *)work->pspResult.paramSfo->saveDataDirectory,
			sizeof(work->pspResult.paramSfo->saveDataDirectory) - 1);
		work->pspResult.paramSfo->parentalLev = (SceUInt8)slotInfo->userParam;
	}
}

static void WriteVitaResult(CommonDialogSaveDataParam *work, SceInt32 status,
	SceSaveDataDialogMode mode, SceInt32 buttonAction,
	const SaveDataDialogSlotResultInfo *slotInfo)
{
	ResetResultBuffers(work);
	work->result.mode = mode;
	work->result.userdata = work->param.userdata;
	work->result.buttonId = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;

	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_SAVEDATA)) {
		work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		return;
	}

	if (status < 0 && status != SAVEDATA_DIALOG_ERROR_PARAM) {
		work->result.result = SAVEDATA_DIALOG_ERROR_COMMON_GENERAL;
		return;
	}

	work->result.result = status;
	if (status < 0 || status == SCE_COMMON_DIALOG_RESULT_ABORTED || slotInfo == NULL)
		return;
	if (buttonAction == 3 && (mode == SCE_SAVEDATA_DIALOG_MODE_LIST ||
				  mode == SCE_SAVEDATA_DIALOG_MODE_FIXED))
		return;

	work->result.slotId = slotInfo->slotId;
	if (work->result.slotInfo != NULL) {
		work->result.slotInfo->isExist = (slotInfo->existState == 1) ? SCE_TRUE : SCE_FALSE;
		if (slotInfo->existState != 0 && work->result.slotInfo->slotParam != NULL) {
			SceAppUtilSaveDataSlotParam *outParam = work->result.slotInfo->slotParam;
			CopyWideToUtf8Buffer(slotInfo->title, (char *)outParam->title,
				sizeof(outParam->title));
			outParam->title[sizeof(outParam->title) - 1] = '\0';
			CopyWideToUtf8Buffer(slotInfo->subTitle, (char *)outParam->subTitle,
				sizeof(outParam->subTitle));
			outParam->subTitle[sizeof(outParam->subTitle) - 1] = '\0';
			CopyWideToUtf8Buffer(slotInfo->detail, (char *)outParam->detail,
				sizeof(outParam->detail));
			outParam->detail[sizeof(outParam->detail) - 1] = '\0';
			CopyStringToUtf8Buffer(slotInfo->iconPath, (char *)outParam->iconPath,
				sizeof(outParam->iconPath));
			outParam->iconPath[sizeof(outParam->iconPath) - 1] = '\0';
			outParam->status = (SceAppUtilSaveDataSlotStatus)slotInfo->status;
			outParam->userParam = slotInfo->userParam;
			outParam->sizeKiB = slotInfo->sizeKiB;
			outParam->modifiedTime = slotInfo->modifiedTime;
		}
	}
}

static void MapButtonActionToResult(SceSaveDataDialogMode mode, SceInt32 buttonAction,
	SceInt32 *outResult, SceInt32 *outButtonId)
{
	switch (mode) {
	case SCE_SAVEDATA_DIALOG_MODE_LIST:
	case SCE_SAVEDATA_DIALOG_MODE_FIXED:
		if (buttonAction == 3)
			*outResult = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
		break;
	case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
	case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
	case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
	case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
		if (buttonAction == 2)
			*outButtonId = SCE_SAVEDATA_DIALOG_BUTTON_ID_OK;
		else if (buttonAction == 3)
			*outResult = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
		else if (buttonAction == 4)
			*outButtonId = SCE_SAVEDATA_DIALOG_BUTTON_ID_YES;
		else if (buttonAction == 5)
			*outButtonId = SCE_SAVEDATA_DIALOG_BUTTON_ID_NO;
		break;
	default:
		break;
	}
}

static void FinishPluginUpdate(CommonDialogSaveDataParam *work)
{
	work->closeRequested = 0;
	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	SceInt32 ret = SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_SAVEDATA);
	if (ret != SCE_OK)
		__builtin_trap();
}

static void OnUpdate(void *userData)
{
	(void)userData;
	SceInt32 ret = SCE_OK;
	SaveDataDialogPluginApiScope scope("OnUpdate", ret);
	CommonDialogSaveDataParam *work = scope.work();

	if (ret == SCE_OK && work != NULL) {
		if ((work->requestFlags & SAVEDATA_DIALOG_FLAG_PARAM) != 0) {
			if (work->isPsp == 1) {
				ret = ValidatePspSaveDataParam(&work->pspParam, SCE_FALSE);
				if (ret < 0) {
					if (ret == SAVEDATA_DIALOG_ERROR_PARAM)
						PrintSaveDataParamError();
				} else {
					ret = OpenPspSaveDataDialog(&work->pspParam,
						SaveDataDialogCallback, SAVEDATA_DIALOG_CALLBACK_TAG);
				}
			} else {
				ret = ValidateSaveDataParam(&work->param, SCE_FALSE);
				if (ret < 0) {
					if (ret == SAVEDATA_DIALOG_ERROR_PARAM)
						PrintSaveDataParamError();
				} else {
					ret = OpenVitaSaveDataDialog(&work->param,
						SaveDataDialogCallback, SAVEDATA_DIALOG_CALLBACK_TAG);
				}
			}
			work->requestFlags &= ~SAVEDATA_DIALOG_FLAG_PARAM;
		}

		if (ret == SCE_OK && (work->requestFlags & SAVEDATA_DIALOG_FLAG_CONTINUE) != 0) {
			if (work->isPsp == 1) {
				ret = ValidatePspSaveDataParam(&work->pspParam, SCE_TRUE);
				if (ret < 0) {
					if (ret == SAVEDATA_DIALOG_ERROR_PARAM)
						PrintSaveDataParamError();
				} else {
					ret = OpenPspSaveDataDialog(&work->pspParam,
						SaveDataDialogCallback, SAVEDATA_DIALOG_CALLBACK_TAG);
				}
			} else {
				ret = ValidateSaveDataParam(&work->param, SCE_TRUE);
				if (ret < 0) {
					if (ret == SAVEDATA_DIALOG_ERROR_PARAM)
						PrintSaveDataParamError();
				} else {
					ret = OpenVitaSaveDataDialog(&work->param,
						SaveDataDialogCallback, SAVEDATA_DIALOG_CALLBACK_TAG);
				}
			}
			work->requestFlags &= ~SAVEDATA_DIALOG_FLAG_CONTINUE;
		}

		if (ret == SCE_OK) {
			if ((work->requestFlags & SAVEDATA_DIALOG_FLAG_FINISH) != 0) {
				if (ValidateSaveDataFinishParam(&work->finishParam) != 0) {
					PrintSaveDataParamError();
					ret = SAVEDATA_DIALOG_ERROR_PARAM;
				} else {
					DestroySaveDataDialog();
					common::MainThreadCallList::Unregister(OnUpdate, NULL);
					SceInt32 closeRet = SceCommonDialogMain_05EFC635(
						COMMON_DIALOG_TYPE_SAVEDATA);
					if (closeRet != SCE_OK)
						__builtin_trap();
					ret = SCE_OK;
				}
				work->requestFlags &= ~SAVEDATA_DIALOG_FLAG_FINISH;
			} else {
				if ((work->requestFlags & SAVEDATA_DIALOG_FLAG_SUB_CLOSE) != 0) {
					work->requestFlags &= ~SAVEDATA_DIALOG_FLAG_SUB_CLOSE;
					ret = SCE_OK;
				}
				if ((work->requestFlags & SAVEDATA_DIALOG_FLAG_PROGRESS_INC) != 0) {
					SaveDataDialog *dialog = GetSaveDataDialogInstance();
					if (dialog != NULL)
						dialog->IncrementProgressBar(work->progressBarTarget,
							work->progressBarRate);
					work->requestFlags &= ~SAVEDATA_DIALOG_FLAG_PROGRESS_INC;
					ret = SCE_OK;
				}
				if ((work->requestFlags & SAVEDATA_DIALOG_FLAG_PROGRESS_SET) != 0) {
					SaveDataDialog *dialog = GetSaveDataDialogInstance();
					if (dialog != NULL)
						dialog->SetProgressBarValue(work->progressBarTarget,
							work->progressBarRate);
					work->requestFlags &= ~SAVEDATA_DIALOG_FLAG_PROGRESS_SET;
					ret = SCE_OK;
				}
				if (work->closeRequested == 1) {
					bool isAborted = SceCommonDialogMain_75F10F83(
						COMMON_DIALOG_TYPE_SAVEDATA);
					ret = CloseSaveDataDialog(isAborted);
					work->closeRequested = 2;
				}
			}
		}
	}

	if (ret < 0 && work != NULL) {
		if (work->isPsp == 1)
			WritePspResult(work, ret, (SceSaveDataDialogMode)work->pspParam.mode, 0, NULL);
		else
			WriteVitaResult(work, ret, work->param.mode, 0, NULL);
		FinishPluginUpdate(work);
	}
}

static void saveDataDialogPluginInit(Plugin *plugin)
{
	g_saveDataDialogPlugin = plugin;
	SceInt32 ret = SCE_OK;
	SaveDataDialogPluginApiScope scope("Init", ret);
	(void)ret;
}

static void saveDataDialogPluginStart(Plugin *plugin)
{
	common::MainThreadCallList::Register(OnUpdate, NULL);
	SceAppUtil_CD0E5C59(OnSaveDataSlotChanged, NULL);

	PluginJobQueueConfig queueConfig;
	queueConfig.allocator = NULL;
	queueConfig.surfacePool = (plugin != NULL) ? plugin->m_surface_pool : NULL;
	queueConfig.decodePool = queueConfig.surfacePool;
	queueConfig.reserved = NULL;
	queueConfig.jobQueue = NULL;
	PluginJobQueue::Initialize(&queueConfig);
	SceCommonDialogMain_877BB562();
}

static void saveDataDialogPluginStop(Plugin *plugin)
{
	(void)plugin;
	DestroySaveDataDialog();
	SceAppUtil_CD0E5C59(NULL, NULL);
}

static void saveDataDialogPluginExit(Plugin *plugin)
{
	(void)plugin;
	g_saveDataDialogPlugin = NULL;
	{
		SceInt32 ret = SCE_OK;
		SaveDataDialogPluginApiScope scope("Exit", ret);
		if (ret == SCE_OK && scope.work() != NULL)
			SceCommonDialogMain_68330971(COMMON_DIALOG_TYPE_SAVEDATA,
				SAVEDATA_DIALOG_SUB_STATUS_NONE);
	}
	PluginJobQueue::Finalize();
	SceAppUtil_C0418B1C(202);
}

static void saveDataDialogPluginSetParam(Plugin::InitParam2 *param)
{
	if (param != NULL)
		param->draw_priority = 6;
}

static int OnSaveDataSlotChanged(int event, SceAppUtilSaveDataSlotId slotId, void *arg)
{
	(void)arg;
	if (event == SCE_APPUTIL_INTERNAL_EVENT_SAVEDATA_SLOT) {
		SaveDataDialog *dialog = GetSaveDataDialogInstance();
		if (dialog != NULL)
			dialog->OnSlotUpdated(slotId);
	}
	return SCE_OK;
}

static void SaveDataDialogCallback(SceInt32 result, SceSaveDataDialogMode mode,
	SceInt32 buttonAction, const SaveDataDialogSlotResultInfo *slotInfo, void *userData)
{
	(void)userData;
	{
		SceInt32 lockRet = SCE_OK;
		SaveDataDialogPluginApiScope scope("SaveDataDialogCallback", lockRet);
		CommonDialogSaveDataParam *work = scope.work();
		if (lockRet == SCE_OK && work != NULL) {
			if (work->isPsp == 1) {
				WritePspResult(work, result, mode, buttonAction, slotInfo);
				if (work->pspResult.result == SCE_COMMON_DIALOG_RESULT_OK)
					MapButtonActionToResult(mode, buttonAction,
						&work->pspResult.result, &work->pspResult.buttonId);
			} else {
				WriteVitaResult(work, result, mode, buttonAction, slotInfo);
				if (work->result.result == SCE_COMMON_DIALOG_RESULT_OK)
					MapButtonActionToResult(mode, buttonAction,
						&work->result.result, &work->result.buttonId);
			}
			work->closeRequested = 0;
			if (!SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_SAVEDATA))
				SceCommonDialogMain_68330971(COMMON_DIALOG_TYPE_SAVEDATA,
					SAVEDATA_DIALOG_SUB_STATUS_FINISHED);
		}
	}

	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_SAVEDATA) ||
	    result == SAVEDATA_DIALOG_ERROR_PARAM) {
		common::MainThreadCallList::Unregister(OnUpdate, NULL);
		SceInt32 closeRet = SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_SAVEDATA);
		if (closeRet != SCE_OK)
			__builtin_trap();
	}
}

void PrintSaveDataParamError(void)
{
	sceClibPrintf("\n\n****** SceSaveDataDialog Parameter Error : %d ******\n\n",
		g_saveDataParamErrorCode);
}

static bool IsAllZeroBytes(const void *bytes, SceSize count)
{
	const SceUInt8 *cursor = (const SceUInt8 *)bytes;
	for (SceSize idx = 0; idx < count; ++idx) {
		if (cursor[idx] != 0)
			return false;
	}
	return true;
}

static SceInt32 ValidateIconPathString(const char *path)
{
	if (path == NULL)
		return 0;
	SceSize len = 0;
	while (len < 64 && path[len] != '\0') {
		char ch = path[len];
		bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
			       (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
			       ch == '.' || ch == ':' || ch == '/';
		if (!allowed)
			return -1;
		if (len + 1 < 64 && ch == '.' && path[len + 1] == '.')
			return -1;
		++len;
	}
	return (len == 64) ? -1 : 0;
}

static SceInt32 ValidateSubDirPathString(const char *path)
{
	SceSize len = 0;
	while (len < 64 && path[len] != '\0') {
		char ch = path[len];
		bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
			       (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
			       ch == '.' || ch == '/';
		if (!allowed)
			return -1;
		if (len + 1 < 64 && ch == '.' && path[len + 1] == '.')
			return -1;
		++len;
	}
	return (len == 64) ? -1 : 0;
}

static SceInt32 ValidatePspDirNameString(const char *dirName, bool allowEmpty)
{
	if (dirName == NULL)
		return -1;
	if (!allowEmpty && dirName[0] == '\0')
		return -1;
	SceSize len = 0;
	while (len < 32 && dirName[len] != '\0') {
		char ch = dirName[len];
		bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
			       (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
			       ch == '.';
		if (!allowed)
			return -1;
		if (len + 1 < 32 && ch == '.' && (dirName[len + 1] == '.' || dirName[len + 1] == '\0'))
			return -1;
		++len;
	}
	if (len > 32 || dirName[0] == '.' || dirName[31] == '.')
		return -1;
	return 0;
}

static SceInt32 ValidateEmptySlotParam(const SceAppUtilSaveDataSlotEmptyParam *emptyParam)
{
	if (emptyParam->title != NULL) {
		SceSize len = 0;
		SceInt32 lineBreaks = 0;
		const char *cursor = (const char *)emptyParam->title;
		while (len < 64 && cursor[len] != '\0') {
			if (cursor[len] == '\n' || cursor[len] == '\r')
				++lineBreaks;
			++len;
		}
		if (len == 64 || lineBreaks > 0) {
			g_saveDataParamErrorCode = 8;
			return -1;
		}
	}
	if (emptyParam->iconPath != NULL &&
	    ValidateIconPathString((const char *)emptyParam->iconPath) != 0) {
		g_saveDataParamErrorCode = 9;
		return -1;
	}
	if (emptyParam->iconBuf == NULL && emptyParam->iconBufSize != 0) {
		g_saveDataParamErrorCode = 10;
		return -1;
	}
	if (emptyParam->iconBufSize > 0x1FE000u) {
		g_saveDataParamErrorCode = 11;
		return -1;
	}
	if (!IsAllZeroBytes(emptyParam->reserved, sizeof(emptyParam->reserved))) {
		g_saveDataParamErrorCode = 12;
		return -1;
	}
	return 0;
}

static SceInt32 ValidateTargetSlot(const SceAppUtilSaveDataSlot *slot)
{
	g_saveDataParamErrorCode = 0;
	if (slot->id >= 256 && slot->id != SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID) {
		g_saveDataParamErrorCode = 6;
		return -1;
	}
	if ((SceUInt32)slot->status >= 2u) {
		g_saveDataParamErrorCode = 7;
		return -1;
	}
	if (slot->emptyParam != NULL && ValidateEmptySlotParam(slot->emptyParam) != 0)
		return -1;
	return (g_saveDataParamErrorCode != 0) ? -1 : 0;
}

static SceInt32 ValidateSlotConfigParam(const SceSaveDataDialogSlotConfigParam *config)
{
	if (config == NULL)
		return 0;
	if (config->mountPoint != NULL) {
		const char *mp = (const char *)config->mountPoint->data;
		if (sceClibStrncmp("savedata", mp, 8) != 0 ||
		    (mp[8] != '0' && mp[8] != '1') || mp[9] != ':' || mp[10] != '\0') {
			g_saveDataParamErrorCode = 54;
			return -1;
		}
	}
	if (config->appSubDir != NULL) {
		if (ValidateSubDirPathString((const char *)config->appSubDir->srcDir) != 0) {
			g_saveDataParamErrorCode = 57;
			return -1;
		}
		if (ValidateSubDirPathString((const char *)config->appSubDir->dstDir) != 0) {
			g_saveDataParamErrorCode = 58;
			return -1;
		}
		if (!IsAllZeroBytes(config->appSubDir->reserved,
				    sizeof(config->appSubDir->reserved))) {
			g_saveDataParamErrorCode = 59;
			return -1;
		}
	}
	if (!IsAllZeroBytes(config->reserved, sizeof(config->reserved))) {
		g_saveDataParamErrorCode = 56;
		return -1;
	}
	return 0;
}

static SceInt32 ValidateSystemMessageParam(const SceSaveDataDialogSystemMessageParam *sysMsg)
{
	if (sysMsg == NULL) {
		g_saveDataParamErrorCode = 30;
		return -1;
	}
	if (sysMsg->sysMsgType < 1 || sysMsg->sysMsgType > 12) {
		g_saveDataParamErrorCode = 32;
		return -1;
	}
	if (sysMsg->value != 0 &&
	    sysMsg->sysMsgType != SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE &&
	    sysMsg->sysMsgType != SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE) {
		g_saveDataParamErrorCode = 33;
		return -1;
	}
	if (ValidateTargetSlot(&sysMsg->targetSlot) != 0)
		return -1;
	if (!IsAllZeroBytes(sysMsg->reserved, sizeof(sysMsg->reserved))) {
		g_saveDataParamErrorCode = 34;
		return -1;
	}
	return 0;
}

static SceInt32 ValidateProgressBarParam(const SceSaveDataDialogProgressBarParam *progBar)
{
	if (progBar == NULL) {
		g_saveDataParamErrorCode = 39;
		return -1;
	}
	if (progBar->barType != SCE_SAVEDATA_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE) {
		g_saveDataParamErrorCode = 41;
		return -1;
	}
	if (progBar->sysMsgParam.sysMsgType == 0) {
		if (progBar->msg == NULL) {
			g_saveDataParamErrorCode = 43;
			return -1;
		}
		SceSize len = 0;
		SceInt32 lineBreaks = 0;
		const char *cursor = (const char *)progBar->msg;
		while (len < 256 && cursor[len] != '\0') {
			if (cursor[len] == '\n' || cursor[len] == '\r')
				++lineBreaks;
			++len;
		}
		if (len == 256 || lineBreaks > 3) {
			g_saveDataParamErrorCode = 45;
			return -1;
		}
	} else if (progBar->msg != NULL) {
		g_saveDataParamErrorCode = 44;
		return -1;
	}
	if (ValidateTargetSlot(&progBar->targetSlot) != 0)
		return -1;
	if (!IsAllZeroBytes(progBar->reserved, sizeof(progBar->reserved))) {
		g_saveDataParamErrorCode = 46;
		return -1;
	}
	return 0;
}

SceInt32 ValidateSaveDataParam(const SceSaveDataDialogParam *param, SceBool isContinue)
{
	g_saveDataParamErrorCode = 0;
	if (param == NULL) {
		g_saveDataParamErrorCode = 1;
		return SAVEDATA_DIALOG_ERROR_PARAM;
	}
	if (param->sdkVersion == 0) {
		g_saveDataParamErrorCode = 2;
		return SAVEDATA_DIALOG_ERROR_PARAM;
	}
	if (param->mode < SCE_SAVEDATA_DIALOG_MODE_LIST ||
	    param->mode > SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR) {
		g_saveDataParamErrorCode = 3;
		return SAVEDATA_DIALOG_ERROR_PARAM;
	}
	if (param->dispType < SCE_SAVEDATA_DIALOG_TYPE_SAVE ||
	    param->dispType > SCE_SAVEDATA_DIALOG_TYPE_DELETE) {
		g_saveDataParamErrorCode = 4;
		return SAVEDATA_DIALOG_ERROR_PARAM;
	}
	if (!isContinue && ValidateSlotConfigParam(param->slotConfParam) != 0)
		return SAVEDATA_DIALOG_ERROR_PARAM;

	switch (param->mode) {
	case SCE_SAVEDATA_DIALOG_MODE_LIST:
		if (param->listParam == NULL || param->listParam->slotList == NULL ||
		    param->listParam->slotListSize == 0 ||
		    param->listParam->slotListSize > SCE_SAVEDATA_DIALOG_SLOTLIST_MAXSIZE) {
			g_saveDataParamErrorCode = 14;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		for (SceUInt32 idx = 0; idx < param->listParam->slotListSize; ++idx) {
			if (ValidateTargetSlot(&param->listParam->slotList[idx]) != 0)
				return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_FIXED:
		if (param->fixedParam == NULL ||
		    ValidateTargetSlot(&param->fixedParam->targetSlot) != 0)
			return SAVEDATA_DIALOG_ERROR_PARAM;
		break;
	case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
		if (param->userMsgParam == NULL || param->userMsgParam->msg == NULL ||
		    ValidateTargetSlot(&param->userMsgParam->targetSlot) != 0) {
			if (g_saveDataParamErrorCode == 0)
				g_saveDataParamErrorCode = 24;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
		if (ValidateSystemMessageParam(param->sysMsgParam) != 0)
			return SAVEDATA_DIALOG_ERROR_PARAM;
		break;
	case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
		if (param->errorCodeParam == NULL ||
		    ValidateTargetSlot(&param->errorCodeParam->targetSlot) != 0) {
			if (g_saveDataParamErrorCode == 0)
				g_saveDataParamErrorCode = 36;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
		if (ValidateProgressBarParam(param->progBarParam) != 0)
			return SAVEDATA_DIALOG_ERROR_PARAM;
		break;
	default:
		g_saveDataParamErrorCode = 3;
		return SAVEDATA_DIALOG_ERROR_PARAM;
	}

	return SCE_OK;
}

SceInt32 ValidatePspSaveDataParam(const ScePspSaveDataDialogParam *param, SceBool isContinue)
{
	(void)isContinue;
	g_saveDataParamErrorCode = 0;
	if (param == NULL) {
		g_saveDataParamErrorCode = 1;
		return SAVEDATA_DIALOG_ERROR_PARAM;
	}
	if (param->mode < SCE_SAVEDATA_DIALOG_MODE_LIST ||
	    param->mode > SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR) {
		g_saveDataParamErrorCode = 3;
		return SAVEDATA_DIALOG_ERROR_PARAM;
	}
	switch (param->mode) {
	case SCE_SAVEDATA_DIALOG_MODE_LIST:
		if (param->listParam == NULL || param->listParam->targetDirNameList == NULL ||
		    param->listParam->dirNameListSize == 0) {
			g_saveDataParamErrorCode = 14;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		for (SceUInt32 idx = 0; idx < param->listParam->dirNameListSize; ++idx) {
			if (ValidatePspDirNameString(
				(const char *)param->listParam->targetDirNameList[idx].data, false) != 0) {
				g_saveDataParamErrorCode = 16;
				return SAVEDATA_DIALOG_ERROR_PARAM;
			}
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_FIXED:
		if (param->fixedParam == NULL ||
		    ValidatePspDirNameString(
			(const char *)param->fixedParam->targetDirName.data, true) != 0) {
			g_saveDataParamErrorCode = 20;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
		if (param->userMsgParam == NULL || param->userMsgParam->msg == NULL) {
			g_saveDataParamErrorCode = 24;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
		if (param->sysMsgParam == NULL ||
		    param->sysMsgParam->sysMsgType < 1 || param->sysMsgParam->sysMsgType > 12) {
			g_saveDataParamErrorCode = 32;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
		if (param->errorCodeParam == NULL) {
			g_saveDataParamErrorCode = 36;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
		if (param->progBarParam == NULL ||
		    param->progBarParam->barType != SCE_SAVEDATA_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE) {
			g_saveDataParamErrorCode = 41;
			return SAVEDATA_DIALOG_ERROR_PARAM;
		}
		break;
	default:
		return SAVEDATA_DIALOG_ERROR_PARAM;
	}
	return SCE_OK;
}

SceInt32 ValidateSaveDataFinishParam(const SceSaveDataDialogFinishParam *finishParam)
{
	g_saveDataParamErrorCode = 0;
	if (finishParam == NULL) {
		g_saveDataParamErrorCode = 60;
		return -1;
	}
	if ((SceUInt32)finishParam->flag >= 2u) {
		g_saveDataParamErrorCode = 61;
		return -1;
	}
	if (!IsAllZeroBytes(finishParam->reserved, sizeof(finishParam->reserved))) {
		g_saveDataParamErrorCode = 62;
		return -1;
	}
	return 0;
}
