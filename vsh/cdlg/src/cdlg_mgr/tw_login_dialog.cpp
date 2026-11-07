/*
 * The Twitter login dialog's API (sceTwLoginDialog*): its Init, GetResult
 * and Abort on the worker thread. The dialog itself is libcdlg_tw_login's
 * plugin. OnInit, GetResult and Abort are Sony's names (the names the calls
 * keep in the work memory); the file's name, the namespace and the
 * structures are ours: no SDK header has them.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <libsysmodule.h>
#include <modulemgr/module_info.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

#define TW_LOGIN_DIALOG_MODE_1		1
#define TW_LOGIN_DIALOG_MODE_2		2

/* the work memory's state and flags once OnInit has taken the parameter */
#define TW_LOGIN_DIALOG_STATE_RUNNING	1
#define TW_LOGIN_DIALOG_FLAG_INIT	0x1

/* the caller's parameter and the result: the dialog's own parts only its
   plugin reads */
typedef struct TwLoginDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceInt32 mode;
	SceUInt8 dialogData[12];
	SceChar8 reserved[32];
} TwLoginDialogParam;

typedef struct TwLoginDialogResult {
	SceInt32 result;
	SceInt32 dialogData;
	SceChar8 reserved[32];
} TwLoginDialogResult;

/* the work memory (CommonDialogSlot::workMemory) */
typedef struct CommonDialogTwLoginParam {
	SceInt32 state;
	SceUInt32 flags;
	SceInt32 abortResult;
	SceInt32 abortRequested;
	TwLoginDialogParam param;
	SceUInt8 dialogData[8];
	TwLoginDialogResult result;
	char apiName[64];
} CommonDialogTwLoginParam;

COMMON_DIALOG_LAYOUT_CHECK(TwLoginDialogParamSize, sizeof(TwLoginDialogParam) == 128);
COMMON_DIALOG_LAYOUT_CHECK(TwLoginDialogResultSize, sizeof(TwLoginDialogResult) == 40);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogTwLoginSize, sizeof(CommonDialogTwLoginParam) == 256);

typedef CommonDialogApiScope<CommonDialogTwLoginParam, COMMON_DIALOG_TYPE_TW_LOGIN,
			     TW_DIALOG_ERROR_NO_WORK_MEMORY, TW_DIALOG_ERROR_API_NAME,
			     COMMON_DIALOG_TYPE_MSG> TwLoginApiScope;

namespace TwLoginDialog {

int GetResult(TwLoginDialogResult *result);
int Abort(void);

}

using namespace TwLoginDialog;

extern "C" int SceCommonDialog_463DB9ED(const TwLoginDialogParam *param)
{
	return NestInit(COMMON_DIALOG_TYPE_TW_LOGIN, (void *)param, NULL);
}

extern "C" SceCommonDialogStatus sceTwLoginDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_TW_LOGIN);
}

extern "C" int sceTwLoginDialogGetResult(TwLoginDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_TW_LOGIN, (void *)GetResult, 1, result);
}

extern "C" int sceTwLoginDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_TW_LOGIN);
}

extern "C" int sceTwLoginDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_TW_LOGIN, (void *)Abort, 0);
}

namespace TwLoginDialog {

/* what a call returns: the parameter error as it is, else as any dialog's
   call */
static inline int convertResult(int ret)
{
	if (ret == TW_DIALOG_ERROR_PARAM)
		return ret;
	return toCommonDialogResult(ret);
}

int OnInit(void *arg)
{
	const TwLoginDialogParam *param = (const TwLoginDialogParam *)arg;
	SceKernelSystemSwVersion version;
	CommonDialogTwLoginParam *work;
	bool infobarShown;
	SceUInt32 i;
	int check;
	int ret;

	infobarShown = false;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		version.size = sizeof(version);
		if (sceKernelGetSystemSwVersion(&version) < 0)
			return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		if (param->mode != TW_LOGIN_DIALOG_MODE_1 && param->mode != TW_LOGIN_DIALOG_MODE_2) {
			check = 2;
			goto error;
		}
		for (i = 0; i < sizeof(param->reserved); i++) {
			if (param->reserved[i] != 0) {
				check = 3;
				goto error;
			}
		}
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) < 0)
			return TW_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) < 0)
			return TW_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_TW_LOGIN, sizeof(CommonDialogTwLoginParam));
		if (ret == SCE_OK) {
			TwLoginApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				work = scope.work();
				sceClibMemset(work, 0, __builtin_offsetof(CommonDialogTwLoginParam, apiName));
				ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
				if (ret == SCE_OK) {
					sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
					work->flags |= TW_LOGIN_DIALOG_FLAG_INIT;
					work->state = TW_LOGIN_DIALOG_STATE_RUNNING;
					ret = SCE_OK;
				}
			}
		}
	}
	return convertResult(ret);

error:
	sceClibPrintf(TW_DIALOG_PARAM_ERROR_MSG, check);
	return TW_DIALOG_ERROR_PARAM;
}

int GetResult(TwLoginDialogResult *result)
{
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_TW_LOGIN) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
		{
			sceClibPrintf(TW_DIALOG_PARAM_ERROR_MSG, 100);
			return TW_DIALOG_ERROR_PARAM;
		}
	}
	{
		TwLoginApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return convertResult(ret);
}

int Abort(void)
{
	CommonDialogTwLoginParam *work;
	int ret;

	{
		TwLoginApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			if (work->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_TW_LOGIN);
				if (ret == SCE_OK) {
					work->abortRequested = 1;
					if (work->abortResult == 0)
						work->abortResult = 1;
				}
			}
		}
	}
	return convertResult(ret);
}

}
