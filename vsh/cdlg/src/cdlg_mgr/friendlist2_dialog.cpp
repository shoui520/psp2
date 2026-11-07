/*
 * The friend list 2 dialog's API (sceNpFriendList2Dialog*): its parameter
 * check and its Init and GetResult on the worker thread. The dialog itself
 * is libcdlg_friendlist2's plugin. OnInit, OnInit2 and GetResult are Sony's
 * names (the names the calls keep in the work memory); the file's name and
 * the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <sysmodule/sysmodule.h>
#include <np_friendlist2_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define FRIENDLIST2_DIALOG_ERROR_NO_WORK_MEMORY	-2146400222	/* 0x80108822 */
#define FRIENDLIST2_DIALOG_ERROR_API_NAME	-2146400223	/* 0x80108821 */

/* the caller's parameter and the result for GetResult (CommonDialogSlot::workMemory) */
typedef struct CommonDialogFriendList2Param {
	SceNpFriendList2DialogParam param;
	SceNpFriendList2DialogResult result;
	SceCommonDialogColor bgColor;
	char apiName[64];
} CommonDialogFriendList2Param;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogFriendList2Size, sizeof(CommonDialogFriendList2Param) == 260);

typedef CommonDialogApiScope<CommonDialogFriendList2Param, COMMON_DIALOG_TYPE_FRIENDLIST2,
			     FRIENDLIST2_DIALOG_ERROR_NO_WORK_MEMORY,
			     FRIENDLIST2_DIALOG_ERROR_API_NAME> FriendList2ApiScope;

namespace FriendList2Dialog {

int GetResult(SceNpFriendList2DialogResult *result);

}

extern "C" int sceNpFriendList2DialogInit(const SceNpFriendList2DialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_FRIENDLIST2, (void *)param);
}

extern "C" int SceCommonDialog_AA624C1F(const SceNpFriendList2DialogParam *param)
{
	return OnInit2(COMMON_DIALOG_TYPE_FRIENDLIST2, (void *)param, NULL);
}

extern "C" int sceNpFriendList2DialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_FRIENDLIST2);
}

extern "C" int sceNpFriendList2DialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_FRIENDLIST2);
}

extern "C" SceCommonDialogStatus sceNpFriendList2DialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_FRIENDLIST2);
}

extern "C" int sceNpFriendList2DialogGetResult(SceNpFriendList2DialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_FRIENDLIST2, (void *)FriendList2Dialog::GetResult, 1, result);
}

namespace FriendList2Dialog {

int OnInit(void *arg)
{
	const SceNpFriendList2DialogParam *param = (const SceNpFriendList2DialogParam *)arg;
	CommonDialogFriendList2Param *work;
	bool infobarShown;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
	if (ret != SCE_OK)
		return ret;
	if (param->mode != SCE_NP_FRIENDLIST2_DIALOG_MODE_SINGLE_SELECT)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if ((SceInt32)param->defaultSort < SCE_NP_FRIENDLIST2_DIALOG_SORT_BY_STATUS
	    || param->defaultSort > SCE_NP_FRIENDLIST2_DIALOG_SORT_BY_ID)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* the network modules the dialog needs loaded by the application */
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) != SCE_OK) {
		ret = (int)SCE_NP_FRIENDLIST2_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	} else {
		ret = SCE_OK;
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_UTILITY) != SCE_OK)
			ret = (int)SCE_NP_FRIENDLIST2_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	}
	if (ret >= 0) {
		restoreInfobar();
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_FRIENDLIST2, sizeof(CommonDialogFriendList2Param));
		if (ret == SCE_OK) {
			FriendList2ApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				work = scope.work();
				sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
				if (param->commonParam.bgColor != NULL)
					sceClibMemcpy_safe(&work->bgColor, param->commonParam.bgColor,
							   sizeof(work->bgColor));
			}
		}
	}
	return ret;
}

int OnInit2(void *arg, void *initArg)
{
	const SceNpFriendList2DialogParam *param = (const SceNpFriendList2DialogParam *)arg;
	CommonDialogFriendList2Param *work;
	bool infobarShown;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (param->commonParam.dimmerColor != NULL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_DIMMER_COLOR;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
	if (ret != SCE_OK)
		return ret;
	if (param->mode != SCE_NP_FRIENDLIST2_DIALOG_MODE_SINGLE_SELECT)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if ((SceInt32)param->defaultSort < SCE_NP_FRIENDLIST2_DIALOG_SORT_BY_STATUS
	    || param->defaultSort > SCE_NP_FRIENDLIST2_DIALOG_SORT_BY_ID)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* the network modules the dialog needs loaded by the application */
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) != SCE_OK) {
		ret = (int)SCE_NP_FRIENDLIST2_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	} else {
		ret = SCE_OK;
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_UTILITY) != SCE_OK)
			ret = (int)SCE_NP_FRIENDLIST2_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	}
	if (ret >= 0) {
		restoreInfobar();
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_FRIENDLIST2, sizeof(CommonDialogFriendList2Param));
		if (ret == SCE_OK) {
			FriendList2ApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				work = scope.work();
				sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
				if (param->commonParam.bgColor != NULL)
					sceClibMemcpy_safe(&work->bgColor, param->commonParam.bgColor,
							   sizeof(work->bgColor));
			}
		}
	}
	return ret;
}

int GetResult(SceNpFriendList2DialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_FRIENDLIST2) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	{
		FriendList2ApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return ret;
}

}
