/*
 * The NP profile dialog's API (sceNpProfileDialog*): its Init and
 * GetResult on the worker thread. The dialog itself is libcdlg_npprofile2's
 * plugin. OnInit, OnInit2 and GetResult are Sony's
 * names (the names the calls keep in the work memory); the file's name and
 * the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <sysmodule/sysmodule.h>
#include <np_profile_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define NP_PROFILE_DIALOG_ERROR_NO_WORK_MEMORY	-2146427102	/* 0x80101F22 */
#define NP_PROFILE_DIALOG_ERROR_API_NAME	-2146427103	/* 0x80101F21 */

/* the caller's parameter and the result for GetResult (CommonDialogSlot::workMemory) */
typedef struct CommonDialogNpProfileParam {
	SceNpProfileDialogParam param;
	SceNpProfileDialogResult result;
	SceCommonDialogColor bgColor;
	char apiName[64];
} CommonDialogNpProfileParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpProfileSize, sizeof(CommonDialogNpProfileParam) == 256);

typedef CommonDialogApiScope<CommonDialogNpProfileParam, COMMON_DIALOG_TYPE_NPPROFILE2,
			     NP_PROFILE_DIALOG_ERROR_NO_WORK_MEMORY,
			     NP_PROFILE_DIALOG_ERROR_API_NAME> NpProfileApiScope;

namespace NpProfileDialog {

int GetResult(SceNpProfileDialogResult *result);

}

extern "C" int sceNpProfileDialogInit(const SceNpProfileDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_NPPROFILE2, (void *)param);
}

extern "C" int SceCommonDialog_3D3F9AD6(const SceNpProfileDialogParam *param)
{
	return OnInit2(COMMON_DIALOG_TYPE_NPPROFILE2, (void *)param, NULL);
}

extern "C" int sceNpProfileDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_NPPROFILE2);
}

extern "C" int sceNpProfileDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_NPPROFILE2);
}

extern "C" SceCommonDialogStatus sceNpProfileDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_NPPROFILE2);
}

extern "C" int sceNpProfileDialogGetResult(SceNpProfileDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NPPROFILE2, (void *)NpProfileDialog::GetResult, 1, result);
}

namespace NpProfileDialog {

int OnInit(void *arg)
{
	const SceNpProfileDialogParam *param = (const SceNpProfileDialogParam *)arg;
	CommonDialogNpProfileParam *work;
	bool infobarShown;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
	if (ret != SCE_OK)
		return ret;
	/* the system's callers may ask for the other modes */
	if (!isApplicationMode() && param->mode != SCE_NP_PROFILE_DIALOG_MODE_NORMAL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* the network modules the dialog needs loaded by the application */
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) != SCE_OK) {
		ret = (int)SCE_NP_PROFILE_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	} else {
		ret = SCE_OK;
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_UTILITY) != SCE_OK)
			ret = (int)SCE_NP_PROFILE_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	}
	if (ret >= 0) {
		restoreInfobar();
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NPPROFILE2, sizeof(CommonDialogNpProfileParam));
		if (ret == SCE_OK) {
			NpProfileApiScope scope(__FUNCTION__, ret);

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
	const SceNpProfileDialogParam *param = (const SceNpProfileDialogParam *)arg;
	CommonDialogNpProfileParam *work;
	bool infobarShown;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (param->commonParam.dimmerColor != NULL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_DIMMER_COLOR;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
	if (ret != SCE_OK)
		return ret;
	/* the system's callers may ask for the other modes */
	if (!isApplicationMode() && param->mode != SCE_NP_PROFILE_DIALOG_MODE_NORMAL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	/* the network modules the dialog needs loaded by the application */
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) != SCE_OK) {
		ret = (int)SCE_NP_PROFILE_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	} else {
		ret = SCE_OK;
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_UTILITY) != SCE_OK)
			ret = (int)SCE_NP_PROFILE_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	}
	if (ret >= 0) {
		restoreInfobar();
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NPPROFILE2, sizeof(CommonDialogNpProfileParam));
		if (ret == SCE_OK) {
			NpProfileApiScope scope(__FUNCTION__, ret);

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

int GetResult(SceNpProfileDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_NPPROFILE2) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	{
		NpProfileApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return ret;
}

}
