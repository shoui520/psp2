/*
 * The net check dialog's API (sceNetCheckDialog*): its Init, Init2,
 * GetResult and GetPS3ConnectInfo on the worker thread. The dialog itself is
 * libcdlg_netcheck's plugin. OnInit, OnInit2, GetResult and
 * GetPS3ConnectInfo are Sony's names (the names the calls keep in the work
 * memory); the file's name, the namespace and the other names are ours. The
 * calls return their errors as they are.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <netcheck_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the last SDK whose applications know nothing of the age restriction */
#define NETCHECK_DIALOG_AGE_RESTRICTION_SDK_VERSION	0x02099999

/*
 * What Init2's caller gives the plugin besides the parameter, and what Init
 * gives it for an application. The names are ours: Init sets the first two
 * to 1 and clears the two after the age restriction; Init2 takes them from
 * its caller.
 */
typedef struct NetCheckDialogInitArg {
	SceUInt16 enabled[2];
	SceUInt16 useAgeRestriction;
	SceUInt16 cleared[2];
	SceUInt8 reserved[122];
} NetCheckDialogInitArg;

/* the work memory (CommonDialogSlot::workMemory) */
typedef struct CommonDialogNetCheckParam {
	SceNetCheckDialogParam param;
	NetCheckDialogInitArg initArg;
	SceNetCheckDialogPS3ConnectParam ps3ConnectParam;
	SceNetCheckDialogResult result;
	SceNetCheckDialogPS3ConnectInfo ps3ConnectInfo;
	SceUInt8 pluginState[44];
	SceCommonDialogColor bgColor;
	char apiName[64];
	SceNetAdhocctlGroupName groupName;
	SceNetCheckDialogAgeRestriction ageRestriction[SCE_NETCHECK_DIALOG_AGE_RESTRICTION_COUNT_MAX];
} CommonDialogNetCheckParam;

COMMON_DIALOG_LAYOUT_CHECK(NetCheckDialogInitArgSize, sizeof(NetCheckDialogInitArg) == 132);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNetCheckResult,
			   __builtin_offsetof(CommonDialogNetCheckParam, result) == 468);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNetCheckBgColor,
			   __builtin_offsetof(CommonDialogNetCheckParam, bgColor) == 788);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNetCheckSize, sizeof(CommonDialogNetCheckParam) == 1664);

typedef CommonDialogApiScope<CommonDialogNetCheckParam, COMMON_DIALOG_TYPE_NETCHECK,
			     SAVEDATA_DIALOG_ERROR_NO_WORK_MEMORY,
			     SAVEDATA_DIALOG_ERROR_API_NAME> NetCheckApiScope;

namespace NetCheckDialog {

int GetResult(SceNetCheckDialogResult *result);
int GetPS3ConnectInfo(SceNetCheckDialogPS3ConnectInfo *info);

}

extern "C" SceInt32 sceNetCheckDialogInit(SceNetCheckDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_NETCHECK, (void *)param);
}

extern "C" SceInt32 SceCommonDialog_243D6A36(SceNetCheckDialogParam *param, NetCheckDialogInitArg *arg)
{
	return OnInit2(COMMON_DIALOG_TYPE_NETCHECK, (void *)param, (void *)arg);
}

extern "C" SceCommonDialogStatus sceNetCheckDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_NETCHECK);
}

extern "C" SceInt32 sceNetCheckDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_NETCHECK);
}

extern "C" SceInt32 sceNetCheckDialogGetResult(SceNetCheckDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NETCHECK, (void *)NetCheckDialog::GetResult, 1, result);
}

extern "C" SceInt32 sceNetCheckDialogGetPS3ConnectInfo(SceNetCheckDialogPS3ConnectInfo *info)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NETCHECK, (void *)NetCheckDialog::GetPS3ConnectInfo, 1, info);
}

extern "C" SceInt32 sceNetCheckDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_NETCHECK);
}

namespace NetCheckDialog {

/* the age restrictions of a PSN check into the work memory */
static inline int copyAgeRestriction(CommonDialogNetCheckParam *work, const SceNetCheckDialogParam *param)
{
	SceInt32 count;

	if (param->mode != SCE_NETCHECK_DIALOG_MODE_PSN && param->mode != SCE_NETCHECK_DIALOG_MODE_PSN_ONLINE)
		return SCE_OK;
	count = param->ageRestrictionCount;
	if (count > SCE_NETCHECK_DIALOG_AGE_RESTRICTION_COUNT_MAX)
		return (int)SCE_NETCHECK_DIALOG_ERROR_PARAM;
	if (count > 0 && param->ageRestriction != NULL) {
		sceClibMemcpy_safe(work->ageRestriction, param->ageRestriction, count * sizeof(work->ageRestriction[0]));
		work->param.ageRestriction = work->ageRestriction;
	}
	return SCE_OK;
}

/* the rest of the parameter: the plugin's argument and the mode's own parameters */
static inline int copyModeParam(CommonDialogNetCheckParam *work, const SceNetCheckDialogParam *param)
{
	NetCheckDialogInitArg initArg;
	SceUInt32 sdkVersion;
	SceNetCheckDialogMode mode;
	int ret = SCE_OK;

	sceClibMemset(&initArg, 0, sizeof(initArg));
	initArg.cleared[0] = 0;
	initArg.enabled[0] = 1;
	initArg.enabled[1] = 1;
	sdkVersion = SceLibKernel_9F793F84();
	if (sdkVersion > NETCHECK_DIALOG_AGE_RESTRICTION_SDK_VERSION &&
	    (param->mode == SCE_NETCHECK_DIALOG_MODE_PSN || param->mode == SCE_NETCHECK_DIALOG_MODE_PSN_ONLINE))
		initArg.useAgeRestriction = 1;
	else if (sdkVersion <= NETCHECK_DIALOG_AGE_RESTRICTION_SDK_VERSION)
		initArg.useAgeRestriction = 0;
	initArg.cleared[1] = 0;
	sceClibMemcpy_safe(&work->initArg, &initArg, sizeof(work->initArg));
	mode = param->mode;
	if (mode == SCE_NETCHECK_DIALOG_MODE_PS3_CONNECT) {
		if (param->ps3ConnectParam != NULL) {
			sceClibMemcpy_safe(&work->ps3ConnectParam, param->ps3ConnectParam,
					   sizeof(work->ps3ConnectParam));
			mode = param->mode;
		} else {
			ret = (int)SCE_NETCHECK_DIALOG_ERROR_PARAM;
		}
	}
	if (mode == SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CONN || mode == SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CREATE ||
	    mode == SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_JOIN) {
		if (param->groupName != NULL) {
			sceClibMemcpy_safe(&work->groupName, param->groupName, sizeof(work->groupName));
			mode = param->mode;
		}
	}
	if (mode == SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_JOIN) {
		if (param->timeoutUs != 0)
			work->param.timeoutUs = param->timeoutUs;
		else
			ret = (int)SCE_NETCHECK_DIALOG_ERROR_INVALID_TIMEOUT_PARAM;
	}
	return ret;
}

int OnInit(void *arg)
{
	const SceNetCheckDialogParam *param = (const SceNetCheckDialogParam *)arg;
	bool infobarShown;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	switch (param->mode) {
	case SCE_NETCHECK_DIALOG_MODE_ADHOC_CONN:
	case SCE_NETCHECK_DIALOG_MODE_PSN:
	case SCE_NETCHECK_DIALOG_MODE_PSN_ONLINE:
	case SCE_NETCHECK_DIALOG_MODE_PS3_CONNECT:
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CONN:
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CREATE:
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_JOIN:
		break;
	default:
		return (int)SCE_NETCHECK_DIALOG_ERROR_INVALID_MODE;
	}
	ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NETCHECK, sizeof(CommonDialogNetCheckParam));
	if (ret != SCE_OK)
		return ret;
	{
		NetCheckApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
		if (ret == SCE_OK) {
			CommonDialogNetCheckParam *work = scope.work();

			sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
			if (param->commonParam.bgColor != NULL) {
				sceClibMemcpy_safe(&work->bgColor, param->commonParam.bgColor, sizeof(work->bgColor));
				work->param.commonParam.bgColor = &work->bgColor;
			}
			ret = copyAgeRestriction(work, param);
			if (ret == SCE_OK)
				ret = copyModeParam(work, param);
		}
	}
	return ret;
}

int onInit2(void *arg, void *initArg)
{
	const SceNetCheckDialogParam *param = (const SceNetCheckDialogParam *)arg;
	int ret;

	if (param == NULL || initArg == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	switch (param->mode) {
	case SCE_NETCHECK_DIALOG_MODE_ADHOC_CONN:
	case SCE_NETCHECK_DIALOG_MODE_PSN:
	case SCE_NETCHECK_DIALOG_MODE_PSN_ONLINE:
	case SCE_NETCHECK_DIALOG_MODE_PS3_CONNECT:
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CONN:
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_CREATE:
	case SCE_NETCHECK_DIALOG_MODE_PSP_ADHOC_JOIN:
		break;
	default:
		return (int)SCE_NETCHECK_DIALOG_ERROR_INVALID_MODE;
	}
	ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NETCHECK, sizeof(CommonDialogNetCheckParam));
	if (ret == SCE_OK) {
		NetCheckApiScope scope("OnInit2", ret);

		if (ret == SCE_OK) {
			CommonDialogNetCheckParam *work = scope.work();

			sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
			if (param->commonParam.bgColor != NULL) {
				sceClibMemcpy_safe(&work->bgColor, param->commonParam.bgColor,
						   sizeof(work->bgColor));
				work->param.commonParam.bgColor = &work->bgColor;
			}
			sceClibMemcpy_safe(&work->initArg, initArg, sizeof(work->initArg));
			work->initArg.useAgeRestriction = 0;
		}
	}
	return ret;
}

int GetResult(SceNetCheckDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_NETCHECK) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	{
		NetCheckApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return ret;
}

int GetPS3ConnectInfo(SceNetCheckDialogPS3ConnectInfo *info)
{
	int ret;

	if (info == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_NETCHECK) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	{
		NetCheckApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(info, &scope.work()->ps3ConnectInfo, sizeof(*info));
	}
	return ret;
}

}
