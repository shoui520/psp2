/*
 * The "near" dialog's API: its Init, GetResult and Abort on the worker
 * thread, which the "near" Dialog utility (sceNearDialog*) calls. The
 * dialog itself is libcdlg_near's plugin. OnInit, GetResult and Abort are
 * Sony's names (the names the calls keep in the work memory), as are the
 * modes, the result and SCE_NEAR_DIALOG_ERROR_PARAM (the "near" Dialog
 * Utility Reference); the file's name, the namespace, the parameter and the
 * work memory are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* no work memory for the call, another call took it */
#define NEAR_DIALOG_ERROR_NO_WORK_MEMORY	-2146402686	/* 0x80107E82 */
#define NEAR_DIALOG_ERROR_API_NAME		-2146402687	/* 0x80107E81 */

/* the work memory's state and flags once OnInit has taken the parameter */
#define NEAR_DIALOG_STATE_RUNNING	1
#define NEAR_DIALOG_FLAG_INIT		0x1

typedef CommonDialogApiScope<CommonDialogNearParam, COMMON_DIALOG_TYPE_NEAR, NEAR_DIALOG_ERROR_NO_WORK_MEMORY,
			     NEAR_DIALOG_ERROR_API_NAME> NearApiScope;

namespace NearDialog {

int GetResult(SceNearDialogResult *result);
int Abort(void);

}

using namespace NearDialog;

extern "C" int SceCommonDialog_089FB01E(const SceNearDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_NEAR, (void *)param);
}

extern "C" int SceCommonDialog_35387E0C(void)
{
	return Term(COMMON_DIALOG_TYPE_NEAR);
}

extern "C" int SceCommonDialog_F2F0049E(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NEAR, (void *)Abort, 0);
}

extern "C" SceCommonDialogStatus SceCommonDialog_C36C7DD1(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_NEAR);
}

extern "C" int SceCommonDialog_47E7AD8E(SceNearDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NEAR, (void *)GetResult, 1, result);
}

namespace NearDialog {

int OnInit(void *arg)
{
	const SceNearDialogParam *param = (const SceNearDialogParam *)arg;
	CommonDialogNearParam *work;
	bool infobarShown;
	SceUInt32 i;
	int ret;

	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		for (i = 0; i < sizeof(param->reserved); i++) {
			if (param->reserved[i] != 0)
				return SCE_NEAR_DIALOG_ERROR_PARAM;
		}
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
		if (ret != SCE_OK)
			return ret;
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NEAR, sizeof(CommonDialogNearParam));
		if (ret == SCE_OK) {
			NearApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				work = scope.work();
				sceClibMemset(work, 0, __builtin_offsetof(CommonDialogNearParam, apiName));
				sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
				switch (param->mode) {
				case SCE_NEAR_DIALOG_MODE_UPDATE:
				case SCE_NEAR_DIALOG_MODE_UPDATE_RETRY:
					ret = SCE_OK;
					break;
				case SCE_NEAR_DIALOG_MODE_DOWNLOAD:
					if (param->giftDiscoveringId != NULL)
						sceClibStrncpy(work->giftDiscoveringId, param->giftDiscoveringId,
							       NEAR_DIALOG_GIFT_ID_LEN);
					ret = SCE_OK;
					break;
				default:
					ret = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
					break;
				}
				if (ret == SCE_OK) {
					work->state = NEAR_DIALOG_STATE_RUNNING;
					work->flags |= NEAR_DIALOG_FLAG_INIT;
				}
			}
		}
	}
	return toCommonDialogResult(ret);
}

int GetResult(SceNearDialogResult *result)
{
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_NEAR) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return SCE_NEAR_DIALOG_ERROR_PARAM;
	}
	{
		NearApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return toCommonDialogResult(ret);
}

int Abort(void)
{
	CommonDialogNearParam *work;
	int ret;

	{
		NearApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			if (work->abortRequested) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_NEAR);
				if (ret == SCE_OK) {
					work->abortRequested = true;
					if (work->closeRequest == 0)
						work->closeRequest = 1;
				}
			}
		}
	}
	return toCommonDialogResult(ret);
}

}
