/*
 * The friend list dialog's API (sceNpFriendListDialog*): its parameter
 * check and its Init and GetResult on the worker thread. The dialog itself
 * is libcdlg_friendlist's plugin. OnInit, OnInit2 and GetResult are Sony's
 * names (the names the calls keep in the work memory); the file's name and
 * the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <sysmodule/sysmodule.h>
#include <np_friendlist_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define FRIENDLIST_DIALOG_ERROR_NO_WORK_MEMORY	-2146427358	/* 0x80101E22 */
#define FRIENDLIST_DIALOG_ERROR_API_NAME	-2146427359	/* 0x80101E21 */

/* the friend list (the SDK names only the player history) */
#define FRIENDLIST_DIALOG_MODE_FRIENDLIST	1

/* the caller's parameter and the result for GetResult (CommonDialogSlot::workMemory) */
typedef struct CommonDialogFriendListParam {
	SceNpFriendListDialogParam param;
	SceNpFriendListDialogResult result;
	SceCommonDialogColor bgColor;
	char apiName[64];
} CommonDialogFriendListParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogFriendListSize, sizeof(CommonDialogFriendListParam) == 256);

typedef CommonDialogApiScope<CommonDialogFriendListParam, COMMON_DIALOG_TYPE_FRIENDLIST,
			     FRIENDLIST_DIALOG_ERROR_NO_WORK_MEMORY,
			     FRIENDLIST_DIALOG_ERROR_API_NAME> FriendListApiScope;

namespace FriendListDialog {

int GetResult(SceNpFriendListDialogResult *result);

}

extern "C" int sceNpFriendListDialogInit(const SceNpFriendListDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_FRIENDLIST, (void *)param);
}

extern "C" int SceCommonDialog_40CD30CD(const SceNpFriendListDialogParam *param)
{
	return OnInit2(COMMON_DIALOG_TYPE_FRIENDLIST, (void *)param, NULL);
}

extern "C" int sceNpFriendListDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_FRIENDLIST);
}

extern "C" int sceNpFriendListDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_FRIENDLIST);
}

extern "C" SceCommonDialogStatus sceNpFriendListDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_FRIENDLIST);
}

extern "C" int sceNpFriendListDialogGetResult(SceNpFriendListDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_FRIENDLIST, (void *)FriendListDialog::GetResult, 1, result);
}

namespace FriendListDialog {

int OnInit(void *arg)
{
	const SceNpFriendListDialogParam *param = (const SceNpFriendListDialogParam *)arg;
	CommonDialogFriendListParam *work;
	bool infobarShown;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
	if (ret != SCE_OK)
		return ret;
	if (param->mode < FRIENDLIST_DIALOG_MODE_FRIENDLIST
	    || param->mode > SCE_NP_FRIENDLIST_DIALOG_MODE_PLAYERHISTORY)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* the network modules the dialog needs loaded by the application */
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) != SCE_OK) {
		ret = (int)SCE_NP_FRIENDLIST_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	} else {
		ret = SCE_OK;
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_UTILITY) != SCE_OK)
			ret = (int)SCE_NP_FRIENDLIST_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	}
	if (ret >= 0) {
		restoreInfobar();
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_FRIENDLIST, sizeof(CommonDialogFriendListParam));
		if (ret == SCE_OK) {
			FriendListApiScope scope(__FUNCTION__, ret);

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
	const SceNpFriendListDialogParam *param = (const SceNpFriendListDialogParam *)arg;
	CommonDialogFriendListParam *work;
	bool infobarShown;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (param->commonParam.dimmerColor != NULL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_DIMMER_COLOR;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
	if (ret != SCE_OK)
		return ret;
	if (param->mode < FRIENDLIST_DIALOG_MODE_FRIENDLIST
	    || param->mode > SCE_NP_FRIENDLIST_DIALOG_MODE_PLAYERHISTORY)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* the network modules the dialog needs loaded by the application */
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) != SCE_OK) {
		ret = (int)SCE_NP_FRIENDLIST_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	} else {
		ret = SCE_OK;
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_UTILITY) != SCE_OK)
			ret = (int)SCE_NP_FRIENDLIST_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	}
	if (ret >= 0) {
		restoreInfobar();
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_FRIENDLIST, sizeof(CommonDialogFriendListParam));
		if (ret == SCE_OK) {
			FriendListApiScope scope(__FUNCTION__, ret);

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

int GetResult(SceNpFriendListDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_FRIENDLIST) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	{
		FriendListApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return ret;
}

}
