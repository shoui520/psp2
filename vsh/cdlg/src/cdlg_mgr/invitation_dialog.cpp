/*
 * The invitation dialog's API (sceInvitationDialog*, in the
 * SceNpWebApiCommonDialog library): the check of the invitation to send,
 * its Init and its GetResult on the worker thread. The dialog itself is
 * libcdlg_invitation's plugin. OnInit and GetResult are Sony's names (the
 * names the calls keep in the work memory); the file's name, the namespace
 * and the other names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <invitation_dialog.h>
#include <np/np_npid.h>
#include <sysmodule/sysmodule.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

#define INVITATION_MODULE_NUM		5

namespace InvitationDialog {

int GetResult(SceInvitationDialogResult *result);

}

extern "C" int sceInvitationDialogInit(const SceInvitationDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_INVITATION, (void *)param);
}

extern "C" SceCommonDialogStatus sceInvitationDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_INVITATION);
}

extern "C" int sceInvitationDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_INVITATION);
}

extern "C" int sceInvitationDialogGetResult(SceInvitationDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_INVITATION, (void *)InvitationDialog::GetResult, 1, result);
}

extern "C" int sceInvitationDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_INVITATION);
}

namespace InvitationDialog {

using GameCustomDataDialog::checkUtf8Length;
using GameCustomDataDialog::checkStringLength;
using GameCustomDataDialog::isZeroFilled;

int checkSendParam(const SceInvitationDialogParam *param)
{
	const SceInvitationDialogDataParam *dataParam;
	const SceNpOnlineId *onlineId;
	SceUInt32 i;

	dataParam = param->dataParam;
	if (dataParam == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (dataParam->SendInfo.sessionId == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (dataParam->SendInfo.sessionId->data[0] == '\0' || dataParam->SendInfo.sessionId->term != '\0')
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if ((dataParam->SendInfo.options & ~(SCE_INVITATION_DIALOG_OPTION_ASSUME_SEND
					     | SCE_INVITATION_DIALOG_OPTION_SUSPEND_BLOCK)) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (dataParam->SendInfo.userMessage != NULL && dataParam->SendInfo.userMessage[0] != '\0'
	    && checkUtf8Length((const char *)dataParam->SendInfo.userMessage,
			       SCE_INVITATION_DIALOG_MAX_USER_MESSAGE_LENGTH) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	switch (dataParam->SendInfo.addressParam.addressType) {
	case SCE_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE:
		if (dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress.onlineIdsCount == 0
		    || dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress.onlineIdsCount
		       > SCE_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_NUMBER)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		for (i = 0; i < dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress.onlineIdsCount;
		     i++) {
			onlineId = &dataParam->SendInfo.addressParam.addressInfo.UserSelectDisableAddress.onlineIds[i];
			if (onlineId == NULL)
				return SCE_COMMON_DIALOG_ERROR_NULL;
			if (checkStringLength(onlineId->data, SCE_NP_ONLINEID_MAX_LENGTH, false) != 0)
				return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		}
		break;
	case SCE_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE:
		if (dataParam->SendInfo.addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount == 0
		    || dataParam->SendInfo.addressParam.addressInfo.UserSelectEnableAddress.onlineIdsMaxCount
		       > SCE_INVITATION_DIALOG_ADDRESS_USER_LIST_MAX_NUMBER)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		break;
	default:
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	if ((dataParam->SendInfo.options & SCE_INVITATION_DIALOG_OPTION_ASSUME_SEND) == 0
	    || dataParam->SendInfo.addressParam.addressType == SCE_INVITATION_DIALOG_ADDRESS_TYPE_USERDISABLE)
		return SCE_OK;
	return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
}

int checkParam(const SceInvitationDialogParam *param)
{
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (param->dataParam == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	switch (param->mode) {
	case SCE_INVITATION_DIALOG_MODE_SEND:
		ret = checkSendParam(param);
		break;
	case SCE_INVITATION_DIALOG_MODE_RECV:
		ret = SCE_OK;
		if ((param->dataParam->RecvInfo.options & ~SCE_INVITATION_DIALOG_OPTION_SUSPEND_BLOCK) != 0)
			ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		break;
	default:
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	if (ret != SCE_OK)
		return ret;
	if (isZeroFilled(param->reserved, sizeof(param->reserved)))
		return SCE_OK;
	return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
}

int OnInit(void *arg)
{
	const SceInvitationDialogParam *param = (const SceInvitationDialogParam *)arg;
	CommonDialogInvitationParam *work;
	bool infobarShown;
	SceUInt16 modules[INVITATION_MODULE_NUM];
	SceUInt32 i;
	int ret;

	ret = checkParam(param);
	if (ret != SCE_OK)
		return ret;
	/* the network modules the dialog needs loaded by the application */
	modules[0] = SCE_SYSMODULE_NET;
	modules[1] = SCE_SYSMODULE_HTTPS;
	modules[2] = SCE_SYSMODULE_NP;
	modules[3] = SCE_SYSMODULE_NP_BASIC;
	modules[4] = SCE_SYSMODULE_NP_UTILITY;
	for (i = 0; i < INVITATION_MODULE_NUM; i++) {
		ret = sceSysmoduleIsLoaded(modules[i]);
		if (ret != SCE_OK)
			return ret;
	}
	ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_INVITATION, sizeof(CommonDialogInvitationParam));
	if (ret != SCE_OK)
		return ret;
	LockDialog(COMMON_DIALOG_TYPE_INVITATION);
	work = (CommonDialogInvitationParam *)GetWorkMemory(COMMON_DIALOG_TYPE_INVITATION);
	sceClibStrncpy(work->apiName, __FUNCTION__, sizeof(work->apiName) - 1);
	work->apiName[sizeof(work->apiName) - 1] = '\0';
	sceClibMemset(work, 0, __builtin_offsetof(CommonDialogInvitationParam, apiName));
	work->param.mode = param->mode;
	work->param.sdkVersion = param->sdkVersion;
	sceClibMemcpy_safe(&work->param.commonParam, &param->commonParam, sizeof(work->param.commonParam));
	work->param.callbackArg = param->callbackArg;
	work->param.dataParam = param->dataParam;
	work->drawPriority = -1;
	if (param->commonParam.bgColor != NULL) {
		sceClibMemcpy_safe(&work->bgColor, param->commonParam.bgColor, sizeof(work->bgColor));
		work->param.commonParam.bgColor = &work->bgColor;
	} else {
		work->bgColor.r = 38;
		work->bgColor.g = 40;
		work->bgColor.b = 46;
		work->bgColor.a = 255;
	}
	sceClibMemset(work->apiName, 0, sizeof(work->apiName));
	UnlockDialog(COMMON_DIALOG_TYPE_INVITATION);
	return checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
}

int GetResult(SceInvitationDialogResult *result)
{
	CommonDialogInvitationParam *work;
	SceUInt32 i;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_INVITATION) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	if (!isZeroFilled(result->reserved, sizeof(result->reserved)))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	LockDialog(COMMON_DIALOG_TYPE_INVITATION);
	work = (CommonDialogInvitationParam *)GetWorkMemory(COMMON_DIALOG_TYPE_INVITATION);
	sceClibStrncpy(work->apiName, __FUNCTION__, sizeof(work->apiName) - 1);
	work->apiName[sizeof(work->apiName) - 1] = '\0';
	result->callbackArg = work->param.callbackArg;
	result->result = work->result;
	result->errorCode = work->errorCode;
	if (work->param.mode == SCE_INVITATION_DIALOG_MODE_SEND && work->result == SCE_COMMON_DIALOG_RESULT_OK
	    && work->errorCode == SCE_OK && result->sentOnlineIds != NULL) {
		for (i = 0; i < work->sentNpIdCount; i++)
			sceClibMemcpy_safe(&result->sentOnlineIds->onlineId[i], &work->sentNpId[i].handle,
					   sizeof(SceNpOnlineId));
		result->sentOnlineIds->count = work->sentNpIdCount;
	}
	sceClibMemset(work->apiName, 0, sizeof(work->apiName));
	UnlockDialog(COMMON_DIALOG_TYPE_INVITATION);
	return SCE_OK;
}

}
