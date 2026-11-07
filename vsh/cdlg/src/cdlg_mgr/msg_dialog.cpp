/*
 * The message dialog's API (sceMsgDialog*): its Init, GetResult, Close,
 * Abort and the progress bar's calls on the worker thread. The dialog itself
 * is libcdlg_msg's plugin. OnInit, GetResult, Close, Abort, ProgInc, SetValue
 * and SetMsg are Sony's names (the names the calls keep in the work memory);
 * the file's name, the namespace and the other names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <message_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define MSG_DIALOG_ERROR_NO_WORK_MEMORY		-2146432478	/* 0x80100A22 */
#define MSG_DIALOG_ERROR_API_NAME		-2146432479	/* 0x80100A21 */

/* the draw priority of sceMsgDialogInit */
#define MSG_DIALOG_PRIORITY			229

/* the numbers of the parameter error messages */
#define MSG_DIALOG_PARAM_ERROR_RESULT_RESERVED	100
#define MSG_DIALOG_PARAM_ERROR_PROGRESS_TARGET	101
#define MSG_DIALOG_PARAM_ERROR_BG_COLOR		102

typedef CommonDialogApiScope<CommonDialogMsgParam, COMMON_DIALOG_TYPE_MSG,
			     MSG_DIALOG_ERROR_NO_WORK_MEMORY,
			     MSG_DIALOG_ERROR_API_NAME> MsgApiScope;

namespace MsgDialog {

int GetResult(SceMsgDialogResult *result);
int Close(void);
int Abort(void);
int ProgInc(SceMsgDialogProgressBarTarget target, SceUInt32 delta);
int SetValue(SceMsgDialogProgressBarTarget target, SceUInt32 rate);
int SetMsg(SceMsgDialogProgressBarTarget target, const SceChar8 *barMsg);

}

extern "C" SceInt32 sceMsgDialogInit(const SceMsgDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_MSG, (void *)param);
}

extern "C" SceCommonDialogStatus sceMsgDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_MSG);
}

extern "C" SceInt32 sceMsgDialogClose(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_MSG, (void *)MsgDialog::Close, 0);
}

extern "C" SceInt32 sceMsgDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_MSG, (void *)MsgDialog::Abort, 0);
}

extern "C" SceInt32 sceMsgDialogGetResult(SceMsgDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_MSG, (void *)MsgDialog::GetResult, 1, result);
}

extern "C" SceInt32 sceMsgDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_MSG);
}

extern "C" SceInt32 sceMsgDialogProgressBarSetMsg(SceMsgDialogProgressBarTarget target, const SceChar8 *barMsg)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_MSG, (void *)MsgDialog::SetMsg, 2, target, barMsg);
}

extern "C" SceInt32 sceMsgDialogProgressBarInc(SceMsgDialogProgressBarTarget target, SceUInt32 delta)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_MSG, (void *)MsgDialog::ProgInc, 2, target, delta);
}

extern "C" SceInt32 sceMsgDialogProgressBarSetValue(SceMsgDialogProgressBarTarget target, SceUInt32 rate)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_MSG, (void *)MsgDialog::SetValue, 2, target, rate);
}

namespace MsgDialog {

/* what a call returns: the dialog's parameter error as it is, else as any
   dialog's call */
static inline int convertResult(int ret)
{
	if (ret == SCE_MSG_DIALOG_ERROR_PARAM)
		return ret;
	return toCommonDialogResult(ret);
}

static inline int paramError(int number)
{
	sceClibPrintf("\n\n****** SceMsgDialog Parameter Error : %d ******\n\n", number);
	return SCE_MSG_DIALOG_ERROR_PARAM;
}

/* a button's text copied into the work memory */
static inline void copyButtonMsg(SceChar8 *dst, const SceChar8 **msg)
{
	sceClibStrncpy((char *)dst, (const char *)*msg, SCE_MSG_DIALOG_BUTTON_MSG_SIZE);
	*msg = dst;
	dst[SCE_MSG_DIALOG_BUTTON_MSG_SIZE - 1] = '\0';
}

/* the caller's parameter into the work memory, with what its mode points to */
int copyParam(CommonDialogMsgParam *work, const SceMsgDialogParam *param)
{
	sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
	switch (param->mode) {
	case SCE_MSG_DIALOG_MODE_USER_MSG:
		if (param->userMsgParam != NULL) {
			sceClibMemcpy_safe(&work->userMsgParam, param->userMsgParam, sizeof(work->userMsgParam));
			work->param.userMsgParam = &work->userMsgParam;
			if (param->userMsgParam->msg != NULL) {
				sceClibStrncpy((char *)work->msg, (const char *)param->userMsgParam->msg,
					       sizeof(work->msg));
				work->msg[sizeof(work->msg) - 1] = '\0';
				work->param.userMsgParam->msg = work->msg;
			}
			if (param->userMsgParam->buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS &&
			    param->userMsgParam->buttonParam != NULL) {
				sceClibMemcpy_safe(&work->buttonParam, param->userMsgParam->buttonParam,
						   sizeof(work->buttonParam));
				work->param.userMsgParam->buttonParam = &work->buttonParam;
				if (param->userMsgParam->buttonParam->msg1 != NULL)
					copyButtonMsg(work->buttonMsg1, &work->buttonParam.msg1);
				if (param->userMsgParam->buttonParam->msg2 != NULL)
					copyButtonMsg(work->buttonMsg2, &work->buttonParam.msg2);
				if (param->userMsgParam->buttonParam->msg3 != NULL)
					copyButtonMsg(work->buttonMsg3, &work->buttonParam.msg3);
			}
		}
		break;
	case SCE_MSG_DIALOG_MODE_SYSTEM_MSG:
		if (param->sysMsgParam != NULL) {
			sceClibMemcpy_safe(&work->sysMsgParam, param->sysMsgParam, sizeof(work->sysMsgParam));
			work->param.sysMsgParam = &work->sysMsgParam;
		}
		break;
	case SCE_MSG_DIALOG_MODE_ERROR_CODE:
		if (param->errorCodeParam != NULL) {
			sceClibMemcpy_safe(&work->errorCodeParam, param->errorCodeParam,
					   sizeof(work->errorCodeParam));
			work->param.errorCodeParam = &work->errorCodeParam;
		}
		break;
	case SCE_MSG_DIALOG_MODE_PROGRESS_BAR:
		if (param->progBarParam != NULL) {
			sceClibMemcpy_safe(&work->progBarParam, param->progBarParam, sizeof(work->progBarParam));
			work->param.progBarParam = &work->progBarParam;
			if (param->progBarParam->msg != NULL) {
				sceClibStrncpy((char *)work->msg, (const char *)param->progBarParam->msg,
					       sizeof(work->msg));
				work->msg[sizeof(work->msg) - 1] = '\0';
				work->param.progBarParam->msg = work->msg;
			}
		}
		break;
	}
	return SCE_OK;
}

int OnInit(void *arg)
{
	const SceMsgDialogParam *param = (const SceMsgDialogParam *)arg;
	CommonDialogParamOption option;
	int ret;

	option.keepInfobar = false;
	option.hideInfobar = false;
	option.drawPriority = MSG_DIALOG_PRIORITY;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_MSG, sizeof(CommonDialogMsgParam));
		if (ret == SCE_OK) {
			MsgApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				CommonDialogMsgParam *work = scope.work();

				sceClibMemset(work, 0, __builtin_offsetof(CommonDialogMsgParam, apiName));
				if (param->commonParam.bgColor != NULL) {
					ret = paramError(MSG_DIALOG_PARAM_ERROR_BG_COLOR);
				} else {
					ret = checkCommonParam(&option, &param->commonParam, NULL);
					if (ret == SCE_OK) {
						ret = copyParam(work, param);
						if (ret == SCE_OK) {
							work->request = MSG_DIALOG_REQUEST_INIT;
							work->requestFlags |= MSG_DIALOG_FLAG_PARAM;
						}
					}
				}
			}
		}
	}
	return convertResult(ret);
}

int GetResult(SceMsgDialogResult *result)
{
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_MSG) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return paramError(MSG_DIALOG_PARAM_ERROR_RESULT_RESERVED);
	}
	{
		MsgApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return convertResult(ret);
}

int Close(void)
{
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_MSG) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		MsgApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogMsgParam *work = scope.work();

			if (work->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				if (work->closeRequested == 0)
					work->closeRequested = 1;
				ret = SCE_OK;
			}
		}
	}
	return convertResult(ret);
}

int Abort(void)
{
	int ret;

	{
		MsgApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogMsgParam *work = scope.work();

			if (work->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_MSG);
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

int ProgInc(SceMsgDialogProgressBarTarget target, SceUInt32 delta)
{
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_MSG) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		MsgApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogMsgParam *work = scope.work();

			if (work->param.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED;
			} else if (target != SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT) {
				ret = paramError(MSG_DIALOG_PARAM_ERROR_PROGRESS_TARGET);
			} else {
				work->request = MSG_DIALOG_REQUEST_PROGRESS_INC;
				work->progressBarTarget = target;
				work->progressBarRate += delta;
				if (work->progressBarRate > 100)
					work->progressBarRate = 100;
				ret = SCE_OK;
				work->requestFlags = (work->requestFlags | MSG_DIALOG_FLAG_PROGRESS_INC) &
						     ~MSG_DIALOG_FLAG_PROGRESS_SET;
			}
		}
	}
	return convertResult(ret);
}

int SetValue(SceMsgDialogProgressBarTarget target, SceUInt32 rate)
{
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_MSG) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		MsgApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogMsgParam *work = scope.work();

			if (work->param.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED;
			} else if (target != SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT) {
				ret = paramError(MSG_DIALOG_PARAM_ERROR_PROGRESS_TARGET);
			} else {
				ret = SCE_OK;
				work->progressBarTarget = target;
				work->progressBarRate = rate;
				work->requestFlags = (work->requestFlags | MSG_DIALOG_FLAG_PROGRESS_SET) &
						     ~MSG_DIALOG_FLAG_PROGRESS_INC;
				work->request = MSG_DIALOG_REQUEST_PROGRESS_SET;
			}
		}
	}
	return convertResult(ret);
}

int SetMsg(SceMsgDialogProgressBarTarget target, const SceChar8 *barMsg)
{
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_MSG) != SCE_COMMON_DIALOG_STATUS_RUNNING)
		return SCE_COMMON_DIALOG_ERROR_NOT_RUNNING;
	{
		MsgApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogMsgParam *work = scope.work();

			if (work->param.mode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
				ret = SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED;
			} else if (target != SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT) {
				ret = paramError(MSG_DIALOG_PARAM_ERROR_PROGRESS_TARGET);
			} else {
				work->progressBarTarget = target;
				work->request = MSG_DIALOG_REQUEST_SET_MSG;
				sceClibStrncpy((char *)work->msg, barMsg != NULL ? (const char *)barMsg : "",
					       sizeof(work->msg));
				ret = SCE_OK;
				work->msg[sizeof(work->msg) - 1] = '\0';
				work->msgSize = sizeof(work->msg);
				work->requestFlags |= MSG_DIALOG_FLAG_MSG;
			}
		}
	}
	return convertResult(ret);
}

}
