/*
 * The companion util dialog's API (sceCompanionUtilDialog*): its parameter
 * check and its Init and GetResult on the worker thread. The dialog itself
 * is libcdlg_companion's plugin. OnInit, OnInit2 and GetResult are Sony's
 * names (the names the calls keep in the work memory); the file's name and
 * the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's error for a parameter whose reserved bytes are not zero */
#define COMPANION_UTIL_DIALOG_ERROR_INVALID_PARAM	-2130706602	/* 0x80FFFF56 */
/* no work memory for the call, or another call took it */
#define COMPANION_UTIL_DIALOG_ERROR_WORK_MEMORY		(-2147483647 - 1)	/* 0x80000000 */

/* the plugin's draw priority */
#define COMPANION_UTIL_DIALOG_DRAW_PRIORITY		120

typedef CommonDialogApiScope<CommonDialogCompanionParam, COMMON_DIALOG_TYPE_COMPANION,
			     COMPANION_UTIL_DIALOG_ERROR_WORK_MEMORY,
			     COMPANION_UTIL_DIALOG_ERROR_WORK_MEMORY> CompanionUtilApiScope;

namespace CompanionUtilDialog {

int GetResult(CompanionUtilDialogResult *result);

}

extern "C" int sceCompanionUtilDialogInit(const CompanionUtilDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_COMPANION, (void *)param);
}

extern "C" int SceCommonDialog_62B60B87(const CompanionUtilDialogParam *param,
					 const CompanionUtilDialogInitArg *initArg)
{
	return OnInit2(COMMON_DIALOG_TYPE_COMPANION, (void *)param, (void *)initArg);
}

extern "C" int sceCompanionUtilDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_COMPANION);
}

extern "C" int sceCompanionUtilDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_COMPANION);
}

extern "C" SceCommonDialogStatus sceCompanionUtilDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_COMPANION);
}

extern "C" int sceCompanionUtilDialogGetResult(CompanionUtilDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_COMPANION, (void *)CompanionUtilDialog::GetResult, 1, result);
}

namespace CompanionUtilDialog {

/* what a call returns: the common dialog's errors the application handles and
   the dialog's own as they are, any other error as an unexpected one */
int convertResult(int ret)
{
	switch (ret) {
	case SCE_COMMON_DIALOG_ERROR_BUSY:
	case SCE_COMMON_DIALOG_ERROR_NULL:
	case SCE_COMMON_DIALOG_ERROR_NOT_RUNNING:
	case SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED:
	case SCE_COMMON_DIALOG_ERROR_NOT_FINISHED:
	case SCE_COMMON_DIALOG_ERROR_NOT_IN_USE:
	case SCE_COMMON_DIALOG_ERROR_INVALID_INFOBAR_PARAM:
	case SCE_COMMON_DIALOG_ERROR_INVALID_BG_COLOR:
	case SCE_COMMON_DIALOG_ERROR_INVALID_DIMMER_COLOR:
	case COMPANION_UTIL_DIALOG_ERROR_INVALID_PARAM:
		break;
	default:
		if (ret < 0)
			ret = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		break;
	}
	return ret;
}

static inline int checkParam(const CompanionUtilDialogParam *param)
{
	SceUInt32 i;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	for (i = 0; i < sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0)
			return COMPANION_UTIL_DIALOG_ERROR_INVALID_PARAM;
	}
	if (param->commonParam.bgColor != NULL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_BG_COLOR;
	for (i = 0; i < sizeof(param->commonParam.reserved); i++) {
		if (param->commonParam.reserved[i] != 0)
			return COMPANION_UTIL_DIALOG_ERROR_INVALID_PARAM;
	}
	return SCE_OK;
}

int OnInit(void *arg)
{
	const CompanionUtilDialogParam *param = (const CompanionUtilDialogParam *)arg;
	CommonDialogCompanionParam *work;
	bool infobarShown;
	int ret;

	ret = checkParam(param);
	if (ret != SCE_OK)
		return ret;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
	if (ret != SCE_OK)
		return ret;
	ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_COMPANION, sizeof(CommonDialogCompanionParam));
	if (ret != SCE_OK)
		return ret;
	{
		CompanionUtilApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			sceClibMemset(work, 0, __builtin_offsetof(CommonDialogCompanionParam, apiName));
			sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
			work->drawPriority = COMPANION_UTIL_DIALOG_DRAW_PRIORITY;
		}
	}
	return convertResult(ret);
}

int OnInit2(void *arg, void *initArg)
{
	const CompanionUtilDialogParam *param = (const CompanionUtilDialogParam *)arg;
	CommonDialogCompanionParam *work;
	bool infobarShown;
	int ret;

	ret = checkParam(param);
	if (ret != SCE_OK)
		return ret;
	if (initArg == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
	if (ret != SCE_OK)
		return ret;
	ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_COMPANION, sizeof(CommonDialogCompanionParam));
	if (ret != SCE_OK)
		return ret;
	{
		CompanionUtilApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			sceClibMemset(work, 0, __builtin_offsetof(CommonDialogCompanionParam, apiName));
			sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
			sceClibMemcpy_safe(&work->initArg, initArg, sizeof(work->initArg));
			work->drawPriority = COMPANION_UTIL_DIALOG_DRAW_PRIORITY;
		}
	}
	return convertResult(ret);
}

int GetResult(CompanionUtilDialogResult *result)
{
	SceUInt32 i;
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_COMPANION) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return COMPANION_UTIL_DIALOG_ERROR_INVALID_PARAM;
	}
	{
		CompanionUtilApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return convertResult(ret);
}

}
