/*
 * The save data dialog's API (sceSaveDataDialog*, scePspSaveDataDialog*):
 * its Init, GetResult, Continue, GetSubStatus, Close, Finish, Abort and the
 * progress bar calls on the worker thread, for the PS Vita's save data and
 * for a PSP game's. The dialog itself is libcdlg_savedata's plugin. The
 * calls' names (OnInit, OnInitPsp, GetResult, GetResultPsp, Continue,
 * ContinuePsp, GetSubStatus, Close, Finish, Abort, ProgInc, SetValue) are
 * Sony's, the names they keep in the work memory; the file's name, the
 * namespace and the other names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <savedata_psp_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the draw priority the dialog asks checkCommonParam for */
#define SAVEDATA_DIALOG_PRIORITY		229

/* what a call asks the plugin to do (CommonDialogSaveDataParam::request) */
#define SAVEDATA_DIALOG_REQUEST_INIT		1
#define SAVEDATA_DIALOG_REQUEST_CONTINUE	2
#define SAVEDATA_DIALOG_REQUEST_FINISH		3
#define SAVEDATA_DIALOG_REQUEST_PROGRESS_INC	5
#define SAVEDATA_DIALOG_REQUEST_PROGRESS_SET	6

/* what changed for the plugin (CommonDialogSaveDataParam::requestFlags) */
#define SAVEDATA_DIALOG_FLAG_PARAM		0x01
#define SAVEDATA_DIALOG_FLAG_CONTINUE		0x02
#define SAVEDATA_DIALOG_FLAG_FINISH		0x04
#define SAVEDATA_DIALOG_FLAG_PROGRESS_INC	0x10
#define SAVEDATA_DIALOG_FLAG_PROGRESS_SET	0x20

/* the dialog's sub status (GetUserData of its slot) */
#define SAVEDATA_DIALOG_SUB_STATUS_RUNNING	1
#define SAVEDATA_DIALOG_SUB_STATUS_FINISHED	2

/* the numbers the parameter errors print */
#define SAVEDATA_DIALOG_PARAM_ERROR_RESULT_RESERVED	100
#define SAVEDATA_DIALOG_PARAM_ERROR_SLOT_INFO_RESERVED	101
#define SAVEDATA_DIALOG_PARAM_ERROR_PROGRESS_TARGET	102
#define SAVEDATA_DIALOG_PARAM_ERROR_BG_COLOR		103
#define SAVEDATA_DIALOG_PARAM_ERROR_PARAM_SFO_RESERVED	105

/*
 * The work memory (CommonDialogSlot::workMemory): the caller's parameter
 * with a copy of everything it points to, so that the plugin reads it after
 * the call has returned, the requests for the plugin and the results it
 * leaves.
 */
typedef struct CommonDialogSaveDataParam {
	int reserved;
	int request;
	SceUInt32 requestFlags;
	int closeRequested;
	int abortRequested;
	SceSaveDataDialogParam param;
	SceAppUtilSaveDataSlotEmptyParam emptyParam;
	SceSaveDataDialogListParam listParam;
	SceSaveDataDialogFixedParam fixedParam;
	SceSaveDataDialogUserMessageParam userMsgParam;
	SceSaveDataDialogSystemMessageParam sysMsgParam;
	SceSaveDataDialogErrorCodeParam errorCodeParam;
	SceSaveDataDialogProgressBarParam progBarParam;
	SceSaveDataDialogSlotConfigParam slotConfParam;
	SceSaveDataDialogSlotInfo slotInfo;
	SceAppUtilSaveDataSlotParam slotParam;
	SceSaveDataDialogProgressBarTarget progressBarTarget;
	SceUInt32 progressBarRate;
	int progressBarMsgSet;
	SceChar8 msg[SCE_SAVEDATA_DIALOG_USER_MSG_SIZE];
	SceAppUtilMountPoint mountPoint;
	SceSaveDataDialogAppSubDirParam appSubDir;
	SceSaveDataDialogFinishParam finishParam;
	SceSaveDataDialogResult result;
	int isPsp;
	ScePspSaveDataDialogParam pspParam;
	ScePspSaveDataDialogListParam pspListParam;
	ScePspSaveDataDialogFixedParam pspFixedParam;
	ScePspSaveDataDialogUserMessageParam pspUserMsgParam;
	ScePspSaveDataDialogSystemMessageParam pspSysMsgParam;
	ScePspSaveDataDialogErrorCodeParam pspErrorCodeParam;
	ScePspSaveDataDialogProgressBarParam pspProgBarParam;
	ScePspSaveDataDialogResult pspResult;
	SceAppUtilPspSaveDataParamSfo paramSfo;
	char apiName[64];
} CommonDialogSaveDataParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogSaveDataEmptyParam,
			   __builtin_offsetof(CommonDialogSaveDataParam, emptyParam) == 176);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogSaveDataSlotInfo,
			   __builtin_offsetof(CommonDialogSaveDataParam, slotInfo) == 640);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogSaveDataMsg,
			   __builtin_offsetof(CommonDialogSaveDataParam, msg) == 1536);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogSaveDataResult,
			   __builtin_offsetof(CommonDialogSaveDataParam, result) == 2004);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogSaveDataPspParam,
			   __builtin_offsetof(CommonDialogSaveDataParam, pspParam) == 2064);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogSaveDataParamSfo,
			   __builtin_offsetof(CommonDialogSaveDataParam, paramSfo) == 2804);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogSaveDataSize, sizeof(CommonDialogSaveDataParam) == 4184);

typedef CommonDialogApiScope<CommonDialogSaveDataParam, COMMON_DIALOG_TYPE_SAVEDATA,
			     SAVEDATA_DIALOG_ERROR_NO_WORK_MEMORY,
			     SAVEDATA_DIALOG_ERROR_API_NAME> SaveDataApiScope;

namespace SaveDataDialog {

int GetResult(SceSaveDataDialogResult *result);
int GetResultPsp(ScePspSaveDataDialogResult *result);
int Continue(const SceSaveDataDialogParam *param);
int ContinuePsp(const ScePspSaveDataDialogParam *param);
int GetSubStatus(int *subStatus);
int Close(void);
int Finish(const SceSaveDataDialogFinishParam *param);
int Abort(void);
int ProgInc(SceSaveDataDialogProgressBarTarget target, SceUInt32 delta);
int SetValue(SceSaveDataDialogProgressBarTarget target, SceUInt32 rate);

}

extern "C" SceInt32 sceSaveDataDialogInit(const SceSaveDataDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_SAVEDATA, (void *)param);
}

extern "C" SceCommonDialogStatus sceSaveDataDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_SAVEDATA);
}

extern "C" SceInt32 sceSaveDataDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::Abort, 0);
}

extern "C" SceInt32 sceSaveDataDialogGetResult(SceSaveDataDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::GetResult, 1, result);
}

extern "C" SceInt32 sceSaveDataDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_SAVEDATA);
}

extern "C" SceInt32 sceSaveDataDialogContinue(const SceSaveDataDialogParam *param)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::Continue, 1, param);
}

extern "C" SceCommonDialogStatus sceSaveDataDialogGetSubStatus(void)
{
	int ret;
	int subStatus;

	ret = 0;
	subStatus = 0;
	if (DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::GetSubStatus, 1,
			    &subStatus) == SCE_OK)
		ret = subStatus;
	return (SceCommonDialogStatus)ret;
}

extern "C" SceInt32 sceSaveDataDialogSubClose(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::Close, 0);
}

extern "C" SceInt32 sceSaveDataDialogFinish(const SceSaveDataDialogFinishParam *param)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::Finish, 1, param);
}

extern "C" SceInt32 sceSaveDataDialogProgressBarInc(SceSaveDataDialogProgressBarTarget target,
						     SceUInt32 delta)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::ProgInc, 2, target, delta);
}

extern "C" SceInt32 sceSaveDataDialogProgressBarSetValue(SceSaveDataDialogProgressBarTarget target,
							  SceUInt32 rate)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::SetValue, 2, target, rate);
}

/* the PSP save data: Init2 with the PSP's parameter as its argument */
extern "C" SceInt32 scePspSaveDataDialogInit(const ScePspSaveDataDialogParam *param)
{
	return OnInit2(COMMON_DIALOG_TYPE_SAVEDATA, (void *)1, (void *)param);
}

extern "C" SceInt32 scePspSaveDataDialogGetResult(ScePspSaveDataDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::GetResultPsp, 1, result);
}

extern "C" SceInt32 scePspSaveDataDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_SAVEDATA);
}

extern "C" SceInt32 scePspSaveDataDialogContinue(const ScePspSaveDataDialogParam *param)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_SAVEDATA, (void *)SaveDataDialog::ContinuePsp, 1, param);
}

namespace SaveDataDialog {

/* what a call returns: the dialog's parameter error as it is, else as any
   dialog's call */
static inline int convertResult(int ret)
{
	if (ret == (int)SCE_SAVEDATA_DIALOG_ERROR_PARAM)
		return ret;
	return toCommonDialogResult(ret);
}

static inline int paramError(int number)
{
	sceClibPrintf("\n\n****** SceSaveDataDialog Parameter Error : %d ******\n\n", number);
	return (int)SCE_SAVEDATA_DIALOG_ERROR_PARAM;
}

/* the caller's parameter into the work memory, with what it points to */
int copyParam(CommonDialogSaveDataParam *work, const SceSaveDataDialogParam *param)
{
	sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
	if (param->slotConfParam != NULL) {
		sceClibMemcpy_safe(&work->slotConfParam, param->slotConfParam, sizeof(work->slotConfParam));
		work->param.slotConfParam = &work->slotConfParam;
		if (param->slotConfParam->mountPoint != NULL) {
			sceClibMemcpy_safe(&work->mountPoint, param->slotConfParam->mountPoint,
					   sizeof(work->mountPoint));
			work->param.slotConfParam->mountPoint = &work->mountPoint;
		}
		if (param->slotConfParam->appSubDir != NULL) {
			sceClibMemcpy_safe(&work->appSubDir, param->slotConfParam->appSubDir,
					   sizeof(work->appSubDir));
			work->param.slotConfParam->appSubDir = &work->appSubDir;
		}
	}
	switch (param->mode) {
	case SCE_SAVEDATA_DIALOG_MODE_FIXED:
		if (param->fixedParam != NULL) {
			sceClibMemcpy_safe(&work->fixedParam, param->fixedParam, sizeof(work->fixedParam));
			work->param.fixedParam = &work->fixedParam;
			if (param->fixedParam->targetSlot.emptyParam != NULL) {
				sceClibMemcpy_safe(&work->emptyParam, param->fixedParam->targetSlot.emptyParam,
						   sizeof(work->emptyParam));
				work->param.fixedParam->targetSlot.emptyParam = &work->emptyParam;
			}
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_LIST:
		if (param->listParam != NULL) {
			sceClibMemcpy_safe(&work->listParam, param->listParam, sizeof(work->listParam));
			work->param.listParam = &work->listParam;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
		if (param->userMsgParam != NULL) {
			sceClibMemcpy_safe(&work->userMsgParam, param->userMsgParam, sizeof(work->userMsgParam));
			work->param.userMsgParam = &work->userMsgParam;
			if (param->userMsgParam->msg != NULL) {
				sceClibStrncpy((char *)work->msg, (const char *)param->userMsgParam->msg,
					       sizeof(work->msg));
				work->msg[sizeof(work->msg) - 1] = 0;
				work->param.userMsgParam->msg = work->msg;
			}
			if (param->userMsgParam->targetSlot.emptyParam != NULL) {
				sceClibMemcpy_safe(&work->emptyParam, param->userMsgParam->targetSlot.emptyParam,
						   sizeof(work->emptyParam));
				work->param.userMsgParam->targetSlot.emptyParam = &work->emptyParam;
			}
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
		if (param->sysMsgParam != NULL) {
			sceClibMemcpy_safe(&work->sysMsgParam, param->sysMsgParam, sizeof(work->sysMsgParam));
			work->param.sysMsgParam = &work->sysMsgParam;
			if (param->sysMsgParam->targetSlot.emptyParam != NULL) {
				sceClibMemcpy_safe(&work->emptyParam, param->sysMsgParam->targetSlot.emptyParam,
						   sizeof(work->emptyParam));
				work->param.sysMsgParam->targetSlot.emptyParam = &work->emptyParam;
			}
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
		if (param->errorCodeParam != NULL) {
			sceClibMemcpy_safe(&work->errorCodeParam, param->errorCodeParam,
					   sizeof(work->errorCodeParam));
			work->param.errorCodeParam = &work->errorCodeParam;
			if (param->errorCodeParam->targetSlot.emptyParam != NULL) {
				sceClibMemcpy_safe(&work->emptyParam, param->errorCodeParam->targetSlot.emptyParam,
						   sizeof(work->emptyParam));
				work->param.errorCodeParam->targetSlot.emptyParam = &work->emptyParam;
			}
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
		if (param->progBarParam != NULL) {
			sceClibMemcpy_safe(&work->progBarParam, param->progBarParam, sizeof(work->progBarParam));
			work->param.progBarParam = &work->progBarParam;
			if (param->progBarParam->msg != NULL) {
				sceClibStrncpy((char *)work->msg, (const char *)param->progBarParam->msg,
					       sizeof(work->msg));
				work->msg[sizeof(work->msg) - 1] = 0;
				work->param.progBarParam->msg = work->msg;
			}
			if (param->progBarParam->targetSlot.emptyParam != NULL) {
				sceClibMemcpy_safe(&work->emptyParam, param->progBarParam->targetSlot.emptyParam,
						   sizeof(work->emptyParam));
				work->param.progBarParam->targetSlot.emptyParam = &work->emptyParam;
			}
		}
		break;
	default:
		break;
	}
	return SCE_OK;
}

/* the PSP's parameter into the work memory, with what it points to */
int copyPspParam(CommonDialogSaveDataParam *work, const ScePspSaveDataDialogParam *param)
{
	sceClibMemcpy_safe(&work->pspParam, param, sizeof(work->pspParam));
	switch (param->mode) {
	case SCE_SAVEDATA_DIALOG_MODE_FIXED:
		if (param->fixedParam != NULL) {
			sceClibMemcpy_safe(&work->pspFixedParam, param->fixedParam, sizeof(work->pspFixedParam));
			work->pspParam.fixedParam = &work->pspFixedParam;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_LIST:
		if (param->listParam != NULL) {
			sceClibMemcpy_safe(&work->pspListParam, param->listParam, sizeof(work->pspListParam));
			work->pspParam.listParam = &work->pspListParam;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_USER_MSG:
		if (param->userMsgParam != NULL) {
			sceClibMemcpy_safe(&work->pspUserMsgParam, param->userMsgParam,
					   sizeof(work->pspUserMsgParam));
			work->pspParam.userMsgParam = &work->pspUserMsgParam;
			if (param->userMsgParam->msg != NULL) {
				sceClibStrncpy((char *)work->msg, (const char *)param->userMsgParam->msg,
					       sizeof(work->msg));
				work->msg[sizeof(work->msg) - 1] = 0;
				work->pspParam.userMsgParam->msg = work->msg;
			}
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
		if (param->sysMsgParam != NULL) {
			sceClibMemcpy_safe(&work->pspSysMsgParam, param->sysMsgParam,
					   sizeof(work->pspSysMsgParam));
			work->pspParam.sysMsgParam = &work->pspSysMsgParam;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
		if (param->errorCodeParam != NULL) {
			sceClibMemcpy_safe(&work->pspErrorCodeParam, param->errorCodeParam,
					   sizeof(work->pspErrorCodeParam));
			work->pspParam.errorCodeParam = &work->pspErrorCodeParam;
		}
		break;
	case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR:
		if (param->progBarParam != NULL) {
			sceClibMemcpy_safe(&work->pspProgBarParam, param->progBarParam,
					   sizeof(work->pspProgBarParam));
			work->pspParam.progBarParam = &work->pspProgBarParam;
			if (param->progBarParam->msg != NULL) {
				sceClibStrncpy((char *)work->msg, (const char *)param->progBarParam->msg,
					       sizeof(work->msg));
				work->msg[sizeof(work->msg) - 1] = 0;
				work->pspParam.progBarParam->msg = work->msg;
			}
		}
		break;
	default:
		break;
	}
	return SCE_OK;
}

int OnInit(void *arg)
{
	const SceSaveDataDialogParam *param = (const SceSaveDataDialogParam *)arg;
	CommonDialogParamOption option;
	int ret;

	option.keepInfobar = false;
	option.hideInfobar = false;
	option.drawPriority = SAVEDATA_DIALOG_PRIORITY;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_SAVEDATA, sizeof(CommonDialogSaveDataParam));
		if (ret == SCE_OK) {
			SaveDataApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				CommonDialogSaveDataParam *work = scope.work();

				sceClibMemset(work, 0, __builtin_offsetof(CommonDialogSaveDataParam, apiName));
				if (param->commonParam.bgColor != NULL) {
					ret = paramError(SAVEDATA_DIALOG_PARAM_ERROR_BG_COLOR);
				} else {
					ret = checkCommonParam(&option, &param->commonParam, NULL);
					if (ret == SCE_OK) {
						ret = copyParam(work, param);
						if (ret == SCE_OK) {
							SetUserData(COMMON_DIALOG_TYPE_SAVEDATA,
								    SAVEDATA_DIALOG_SUB_STATUS_RUNNING);
							work->isPsp = 0;
							work->requestFlags |= SAVEDATA_DIALOG_FLAG_PARAM;
							work->request = SAVEDATA_DIALOG_REQUEST_INIT;
						}
					}
				}
			}
		}
	}
	return convertResult(ret);
}

int OnInitPsp(void *arg)
{
	const ScePspSaveDataDialogParam *param = (const ScePspSaveDataDialogParam *)arg;
	CommonDialogParamOption option;
	int ret;

	option.keepInfobar = false;
	option.hideInfobar = false;
	option.drawPriority = SAVEDATA_DIALOG_PRIORITY;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_SAVEDATA, sizeof(CommonDialogSaveDataParam));
		if (ret == SCE_OK) {
			SaveDataApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				CommonDialogSaveDataParam *work = scope.work();

				sceClibMemset(work, 0, __builtin_offsetof(CommonDialogSaveDataParam, apiName));
				if (param->commonParam.bgColor != NULL) {
					ret = paramError(SAVEDATA_DIALOG_PARAM_ERROR_BG_COLOR);
				} else {
					ret = checkCommonParam(&option, &param->commonParam, NULL);
					if (ret == SCE_OK) {
						ret = copyPspParam(work, param);
						if (ret == SCE_OK) {
							SetUserData(COMMON_DIALOG_TYPE_SAVEDATA,
								    SAVEDATA_DIALOG_SUB_STATUS_RUNNING);
							work->isPsp = 1;
							work->request = SAVEDATA_DIALOG_REQUEST_INIT;
							work->requestFlags |= SAVEDATA_DIALOG_FLAG_PARAM;
						}
					}
				}
			}
		}
	}
	return convertResult(ret);
}

/* Init2: the PS Vita's save data (0) or a PSP game's (1) */
int onInit2(void *param, void *arg)
{
	switch ((int)param) {
	case 0:
		return OnInit(arg);
	case 1:
		return OnInitPsp(arg);
	}
	return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
}

/* the reserved bytes of a result and of its slot information: the number
   of the parameter error, or 0 */
static inline int checkResultReserved(const SceSaveDataDialogResult *result)
{
	SceUInt32 i;

	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return SAVEDATA_DIALOG_PARAM_ERROR_RESULT_RESERVED;
	}
	if (result->slotInfo != NULL) {
		for (i = 0; i < sizeof(result->slotInfo->reserved); i++) {
			if (result->slotInfo->reserved[i] != 0)
				return SAVEDATA_DIALOG_PARAM_ERROR_SLOT_INFO_RESERVED;
		}
	}
	return 0;
}

int GetResult(SceSaveDataDialogResult *result)
{
	SceSaveDataDialogSlotInfo *slotInfo;
	int error;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	error = checkResultReserved(result);
	if (error != 0)
		return paramError(error);
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_COMMON_DIALOG_ERROR_NOT_IN_USE) {
			ret = SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
		} else if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (work->isPsp != 0) {
				ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
			} else if (GetUserData(COMMON_DIALOG_TYPE_SAVEDATA) == SAVEDATA_DIALOG_SUB_STATUS_FINISHED ||
				   GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) == SCE_COMMON_DIALOG_STATUS_FINISHED) {
				result->buttonId = work->result.buttonId;
				result->slotId = work->result.slotId;
				result->mode = work->result.mode;
				result->result = work->result.result;
				result->userdata = work->result.userdata;
				slotInfo = result->slotInfo;
				if (slotInfo != NULL) {
					slotInfo->isExist = work->slotInfo.isExist;
					if (slotInfo->slotParam != NULL)
						sceClibMemcpy_safe(slotInfo->slotParam, work->slotInfo.slotParam,
								   sizeof(*slotInfo->slotParam));
				}
			} else {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
			}
		}
	}
	return convertResult(ret);
}

int GetResultPsp(ScePspSaveDataDialogResult *result)
{
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return paramError(SAVEDATA_DIALOG_PARAM_ERROR_RESULT_RESERVED);
	}
	if (result->paramSfo != NULL) {
		for (i = 0; i < sizeof(result->paramSfo->reserved); i++) {
			if (result->paramSfo->reserved[i] != 0)
				return paramError(SAVEDATA_DIALOG_PARAM_ERROR_PARAM_SFO_RESERVED);
		}
	}
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_COMMON_DIALOG_ERROR_NOT_IN_USE) {
			ret = SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
		} else if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (work->isPsp != 1) {
				ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
			} else if (GetUserData(COMMON_DIALOG_TYPE_SAVEDATA) == SAVEDATA_DIALOG_SUB_STATUS_FINISHED ||
				   GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) == SCE_COMMON_DIALOG_STATUS_FINISHED) {
				result->buttonId = work->pspResult.buttonId;
				result->mode = work->pspResult.mode;
				result->result = work->pspResult.result;
				result->userdata = work->pspResult.userdata;
				result->isExist = work->pspResult.isExist;
				sceClibMemcpy_safe(&result->dirName, &work->pspResult.dirName, sizeof(result->dirName));
				/* Sony's: the pointer itself, not what it points to */
				if (result->paramSfo != NULL)
					sceClibMemcpy_safe(&result->paramSfo, &work->paramSfo,
							   sizeof(result->paramSfo));
			} else {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
			}
		}
	}
	return convertResult(ret);
}

int Continue(const SceSaveDataDialogParam *param)
{
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (work->isPsp != 0) {
				ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
			} else if (GetUserData(COMMON_DIALOG_TYPE_SAVEDATA) == SAVEDATA_DIALOG_SUB_STATUS_FINISHED) {
				if (work->abortRequested != 0) {
					ret = SCE_OK;
				} else {
					work->progressBarRate = 0;
					work->progressBarMsgSet = 0;
					sceClibMemset(work->msg, 0, sizeof(work->msg));
					ret = copyParam(work, param);
					if (ret == SCE_OK) {
						SetUserData(COMMON_DIALOG_TYPE_SAVEDATA,
							    SAVEDATA_DIALOG_SUB_STATUS_RUNNING);
						work->request = SAVEDATA_DIALOG_REQUEST_CONTINUE;
						work->requestFlags = SAVEDATA_DIALOG_FLAG_CONTINUE;
					}
				}
			} else {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
			}
		}
	}
	return convertResult(ret);
}

int ContinuePsp(const ScePspSaveDataDialogParam *param)
{
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (work->isPsp != 1) {
				ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
			} else if (GetUserData(COMMON_DIALOG_TYPE_SAVEDATA) == SAVEDATA_DIALOG_SUB_STATUS_FINISHED) {
				if (work->abortRequested != 0) {
					ret = SCE_OK;
				} else {
					work->progressBarRate = 0;
					work->progressBarMsgSet = 0;
					sceClibMemset(work->msg, 0, sizeof(work->msg));
					ret = copyPspParam(work, param);
					if (ret == SCE_OK) {
						SetUserData(COMMON_DIALOG_TYPE_SAVEDATA,
							    SAVEDATA_DIALOG_SUB_STATUS_RUNNING);
						work->request = SAVEDATA_DIALOG_REQUEST_CONTINUE;
						work->requestFlags = SAVEDATA_DIALOG_FLAG_CONTINUE;
					}
				}
			} else {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
			}
		}
	}
	return convertResult(ret);
}

int GetSubStatus(int *subStatus)
{
	int ret;

	if (subStatus == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			*subStatus = GetUserData(COMMON_DIALOG_TYPE_SAVEDATA);
	}
	return convertResult(ret);
}

int Close(void)
{
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (GetUserData(COMMON_DIALOG_TYPE_SAVEDATA) == SAVEDATA_DIALOG_SUB_STATUS_RUNNING) {
				if (work->abortRequested != 0) {
					ret = SCE_OK;
				} else {
					if (work->closeRequested == 0)
						work->closeRequested = 1;
					ret = SCE_OK;
				}
			} else {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
			}
		}
	}
	return convertResult(ret);
}

int Finish(const SceSaveDataDialogFinishParam *param)
{
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (GetUserData(COMMON_DIALOG_TYPE_SAVEDATA) == SAVEDATA_DIALOG_SUB_STATUS_FINISHED) {
				if (work->abortRequested != 0) {
					ret = SCE_OK;
				} else {
					SetUserData(COMMON_DIALOG_TYPE_SAVEDATA,
						    SAVEDATA_DIALOG_SUB_STATUS_RUNNING);
					work->request = SAVEDATA_DIALOG_REQUEST_FINISH;
					sceClibMemcpy_safe(&work->finishParam, param, sizeof(work->finishParam));
					work->requestFlags |= SAVEDATA_DIALOG_FLAG_FINISH;
				}
			} else {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
			}
		}
	}
	return convertResult(ret);
}

int Abort(void)
{
	int ret;

	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (work->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_SAVEDATA);
				if (ret == SCE_OK) {
					work->abortRequested = 1;
					if (work->closeRequested == 0)
						work->closeRequested = 1;
				}
			}
		}
	}
	return convertResult(ret);
}

int ProgInc(SceSaveDataDialogProgressBarTarget target, SceUInt32 delta)
{
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (GetUserData(COMMON_DIALOG_TYPE_SAVEDATA) != SAVEDATA_DIALOG_SUB_STATUS_RUNNING) {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
			} else if (work->param.mode != SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR &&
				   work->pspParam.mode != SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR) {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED;
			} else if (target != SCE_SAVEDATA_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT) {
				ret = paramError(SAVEDATA_DIALOG_PARAM_ERROR_PROGRESS_TARGET);
			} else {
				work->request = SAVEDATA_DIALOG_REQUEST_PROGRESS_INC;
				work->progressBarTarget = target;
				work->progressBarRate += delta;
				if (work->progressBarRate > 100)
					work->progressBarRate = 100;
				ret = SCE_OK;
				work->requestFlags = (work->requestFlags | SAVEDATA_DIALOG_FLAG_PROGRESS_INC) &
						     ~SAVEDATA_DIALOG_FLAG_PROGRESS_SET;
				sceClibMemset(work->msg, 0, 64);
				work->progressBarMsgSet = 0;
			}
		}
	}
	return convertResult(ret);
}

int SetValue(SceSaveDataDialogProgressBarTarget target, SceUInt32 rate)
{
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_SAVEDATA) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		SaveDataApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogSaveDataParam *work = scope.work();

			if (GetUserData(COMMON_DIALOG_TYPE_SAVEDATA) != SAVEDATA_DIALOG_SUB_STATUS_RUNNING) {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
			} else if (work->param.mode != SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR &&
				   work->pspParam.mode != SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR) {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED;
			} else if (target != SCE_SAVEDATA_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT) {
				ret = paramError(SAVEDATA_DIALOG_PARAM_ERROR_PROGRESS_TARGET);
			} else {
				ret = SCE_OK;
				work->progressBarTarget = target;
				work->progressBarRate = rate;
				work->requestFlags = (work->requestFlags | SAVEDATA_DIALOG_FLAG_PROGRESS_SET) &
						     ~SAVEDATA_DIALOG_FLAG_PROGRESS_INC;
				work->request = SAVEDATA_DIALOG_REQUEST_PROGRESS_SET;
			}
		}
	}
	return convertResult(ret);
}

}
