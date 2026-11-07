/*
 * Facebook Permission Dialog's API (sceNpSnsFacebookDialog*): its Init, its
 * results and Abort on the worker thread. The dialog itself is
 * libcdlg_np_sns_fb's plugin. OnInit, OnInit2, GetResult, GetResultLongToken,
 * GetResultInternal and Abort are Sony's names (the names the calls keep in
 * the work memory); the file's name, the namespace, the system's
 * two-argument Init (SceCommonDialog_075E4490) and its argument are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <sysmodule/sysmodule.h>
#include <np_sns_facebook_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define NP_SNS_FACEBOOK_DIALOG_ERROR_NO_WORK_MEMORY	-2146410190	/* 0x80106132 */
#define NP_SNS_FACEBOOK_DIALOG_ERROR_API_NAME		-2146410191	/* 0x80106131 */

/* the modes of the two-argument Init: none (the application's Init), and
   the two whose results always come as the long token, the second with
   data for the dialog */
#define NP_SNS_FACEBOOK_DIALOG_INIT_MODE_NONE		0
#define NP_SNS_FACEBOOK_DIALOG_INIT_MODE_LONG_TOKEN	1
#define NP_SNS_FACEBOOK_DIALOG_INIT_MODE_DATA		3

/* the kinds of data the data mode gives, and their sizes */
#define NP_SNS_FACEBOOK_DIALOG_DATA_TYPE_1		1
#define NP_SNS_FACEBOOK_DIALOG_DATA_TYPE_2		2
#define NP_SNS_FACEBOOK_DIALOG_DATA_TYPE_3		3
#define NP_SNS_FACEBOOK_DIALOG_DATA_TYPE_4		4
#define NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_1		0x2D8
#define NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_2		0x218
#define NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_3		0x2CD0
#define NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_4		0x20D0
#define NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_MAX		NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_3

/* the start of the data mode's data: what kind it is (the rest is the
   plugin's) */
typedef struct NpSnsFacebookDialogData {
	SceUInt8 reserved[8];
	SceInt32 type;
} NpSnsFacebookDialogData;

/* the second argument of the two-argument Init */
typedef struct NpSnsFacebookDialogInitArg {
	SceInt32 mode;
	const NpSnsFacebookDialogData *data;
	SceUInt8 reserved[28];
} NpSnsFacebookDialogInitArg;

/* what SceCommonDialog_64D21971 passes GetResultInternal */
typedef struct NpSnsFacebookDialogGetResultInternalArg {
	SceNpSnsFacebookDialogResultLongToken *result;
	void *internalResult;
} NpSnsFacebookDialogGetResultInternalArg;

/*
 * The work memory (CommonDialogSlot::workMemory): Abort's state, the
 * caller's parameter and Init argument, the data mode's data, the
 * background colour, and the results the plugin leaves.
 */
typedef struct CommonDialogNpSnsFacebookParam {
	SceInt32 abortResult;
	SceInt32 abortRequested;
	SceNpSnsFacebookDialogParam param;
	NpSnsFacebookDialogInitArg initArg;
	SceInt32 dataType;
	SceUInt8 data[NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_MAX];
	SceCommonDialogColor bgColor;
	SceUInt8 hasBgColor;
	SceUInt8 padding[3];
	SceUInt8 internalResult[36];
	SceNpSnsFacebookDialogResultLongToken result;
	char apiName[64];
} CommonDialogNpSnsFacebookParam;

COMMON_DIALOG_LAYOUT_CHECK(NpSnsFacebookDialogParamSize, sizeof(SceNpSnsFacebookDialogParam) == 408);
COMMON_DIALOG_LAYOUT_CHECK(NpSnsFacebookDialogResultSize, sizeof(SceNpSnsFacebookDialogResult) == 336);
COMMON_DIALOG_LAYOUT_CHECK(NpSnsFacebookDialogResultLongTokenSize,
			   sizeof(SceNpSnsFacebookDialogResultLongToken) == 4184);
COMMON_DIALOG_LAYOUT_CHECK(NpSnsFacebookDialogInitArgSize, sizeof(NpSnsFacebookDialogInitArg) == 36);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpSnsFacebookResult,
			   __builtin_offsetof(CommonDialogNpSnsFacebookParam, result) == 11976);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpSnsFacebookSize, sizeof(CommonDialogNpSnsFacebookParam) == 16224);

typedef CommonDialogApiScope<CommonDialogNpSnsFacebookParam, COMMON_DIALOG_TYPE_NP_SNS_FB,
			     NP_SNS_FACEBOOK_DIALOG_ERROR_NO_WORK_MEMORY,
			     NP_SNS_FACEBOOK_DIALOG_ERROR_API_NAME> NpSnsFacebookApiScope;

namespace NpSnsFacebookDialog {

int GetResult(SceNpSnsFacebookDialogResult *result);
int GetResultLongToken(SceNpSnsFacebookDialogResultLongToken *result);
int GetResultInternal(NpSnsFacebookDialogGetResultInternalArg *arg);
int Abort(void);

}

using namespace NpSnsFacebookDialog;

extern "C" SceInt32 sceNpSnsFacebookDialogInit(const SceNpSnsFacebookDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_NP_SNS_FB, (void *)param);
}

extern "C" int SceCommonDialog_075E4490(void *param, void *arg)
{
	return OnInit2(COMMON_DIALOG_TYPE_NP_SNS_FB, param, arg);
}

extern "C" SceCommonDialogStatus sceNpSnsFacebookDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_NP_SNS_FB);
}

extern "C" SceInt32 sceNpSnsFacebookDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NP_SNS_FB, (void *)Abort, 0);
}

extern "C" SceInt32 sceNpSnsFacebookDialogGetResult(SceNpSnsFacebookDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NP_SNS_FB, (void *)GetResult, 1, result);
}

extern "C" SceInt32 sceNpSnsFacebookDialogGetResultLongToken(SceNpSnsFacebookDialogResultLongToken *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NP_SNS_FB, (void *)GetResultLongToken, 1, result);
}

extern "C" int SceCommonDialog_64D21971(SceNpSnsFacebookDialogResultLongToken *result, void *internalResult)
{
	NpSnsFacebookDialogGetResultInternalArg arg;

	arg.result = result;
	arg.internalResult = internalResult;
	return DispatchApiCall(COMMON_DIALOG_TYPE_NP_SNS_FB, (void *)GetResultInternal, 1, &arg);
}

extern "C" SceInt32 sceNpSnsFacebookDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_NP_SNS_FB);
}

namespace NpSnsFacebookDialog {

/* what a call returns: the dialog's own errors as they are, an unexpected
   error as the dialog's internal one */
static inline int convertResult(int ret)
{
	switch (ret) {
	case SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_INTERNAL:
	case SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED:
	case SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM:
	case SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_NOT_SIGNIN:
	case SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_CHAT_RESTRICTION:
	case SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_LIBRARY_NOT_INITIALIZED:
		return ret;
	case SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL:
		return SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_INTERNAL;
	}
	if (isCommonDialogError(ret))
		return ret;
	if (ret < 0)
		return SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_INTERNAL;
	return ret;
}

int OnInit(void *arg)
{
	const SceNpSnsFacebookDialogParam *param = (const SceNpSnsFacebookDialogParam *)arg;
	NpSnsFacebookDialogInitArg initArg;
	CommonDialogNpSnsFacebookParam *work;
	bool infobarShown;
	SceUInt32 i;
	int ret;

	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		for (i = 0; i < sizeof(param->reserved); i++) {
			if (param->reserved[i] != 0)
				return SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
		}
		for (i = 0; i < sizeof(param->accessTokenParam.reserved); i++) {
			if (param->accessTokenParam.reserved[i] != 0)
				return SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
		}
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_SNS_FACEBOOK) != SCE_OK)
			return SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
		if (ret != SCE_OK)
			return ret;
		/* the dialog draws its own background */
		if (param->commonParam.bgColor != NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_BG_COLOR;
		sceClibMemset(&initArg, 0, sizeof(initArg));
		initArg.mode = NP_SNS_FACEBOOK_DIALOG_INIT_MODE_NONE;
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NP_SNS_FB, sizeof(CommonDialogNpSnsFacebookParam));
		if (ret == SCE_OK) {
			NpSnsFacebookApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				work = scope.work();
				sceClibMemset(work, 0, __builtin_offsetof(CommonDialogNpSnsFacebookParam, apiName));
				sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
				sceClibMemcpy_safe(&work->initArg, &initArg, sizeof(work->initArg));
				if (param->commonParam.bgColor != NULL) {
					work->hasBgColor = 1;
					sceClibMemcpy_safe(&work->bgColor, param->commonParam.bgColor, sizeof(work->bgColor));
				} else {
					work->hasBgColor = 0;
				}
			}
		}
	}
	return convertResult(ret);
}

int OnInit2(void *arg1, void *arg2)
{
	const SceNpSnsFacebookDialogParam *param = (const SceNpSnsFacebookDialogParam *)arg1;
	const NpSnsFacebookDialogInitArg *initArg = (const NpSnsFacebookDialogInitArg *)arg2;
	CommonDialogNpSnsFacebookParam *work;
	bool infobarShown;
	int ret;

	if (param == NULL || initArg == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else if (initArg->mode == NP_SNS_FACEBOOK_DIALOG_INIT_MODE_NONE) {
		ret = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
	} else {
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
		if (ret == SCE_OK) {
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NP_SNS_FB, sizeof(CommonDialogNpSnsFacebookParam));
			if (ret == SCE_OK) {
				NpSnsFacebookApiScope scope(__FUNCTION__, ret);

				if (ret == SCE_OK) {
					work = scope.work();
					sceClibMemset(work, 0, __builtin_offsetof(CommonDialogNpSnsFacebookParam, apiName));
					sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
					sceClibMemcpy_safe(&work->initArg, initArg, sizeof(work->initArg));
					if (param->commonParam.bgColor != NULL) {
						work->hasBgColor = 1;
						sceClibMemcpy_safe(&work->bgColor, param->commonParam.bgColor,
								   sizeof(work->bgColor));
					} else {
						work->hasBgColor = 0;
					}
					if (initArg->mode == NP_SNS_FACEBOOK_DIALOG_INIT_MODE_DATA) {
						if (initArg->data == NULL) {
							ret = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
						} else {
							work->dataType = initArg->data->type;
							switch (work->dataType) {
							case NP_SNS_FACEBOOK_DIALOG_DATA_TYPE_1:
								sceClibMemcpy_safe(work->data, initArg->data,
										   NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_1);
								break;
							case NP_SNS_FACEBOOK_DIALOG_DATA_TYPE_2:
								sceClibMemcpy_safe(work->data, initArg->data,
										   NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_2);
								break;
							case NP_SNS_FACEBOOK_DIALOG_DATA_TYPE_3:
								sceClibMemcpy_safe(work->data, initArg->data,
										   NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_3);
								break;
							case NP_SNS_FACEBOOK_DIALOG_DATA_TYPE_4:
								sceClibMemcpy_safe(work->data, initArg->data,
										   NP_SNS_FACEBOOK_DIALOG_DATA_SIZE_4);
								break;
							default:
								ret = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
								break;
							}
						}
					}
				}
			}
		}
	}
	return convertResult(ret);
}

/* whether the Init's mode gives the results as the long token */
static inline bool isLongTokenMode(SceInt32 mode)
{
	return mode == NP_SNS_FACEBOOK_DIALOG_INIT_MODE_LONG_TOKEN || mode == NP_SNS_FACEBOOK_DIALOG_INIT_MODE_DATA;
}

int GetResult(SceNpSnsFacebookDialogResult *result)
{
	CommonDialogNpSnsFacebookParam *work;
	SceInt32 initMode;
	SceInt32 mode;
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_NP_SNS_FB) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
	}
	{
		NpSnsFacebookApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			initMode = work->initArg.mode;
			mode = work->param.mode;
			if (isLongTokenMode(initMode)) {
				if (mode != 0) {
					result->result = work->result.result;
					sceClibMemset(&result->accessTokenResult, 0, sizeof(result->accessTokenResult));
				} else {
					/* Sony's: the long token's result, into the short one's */
					sceClibMemcpy_safe(result, &work->result, sizeof(work->result));
				}
			} else if (mode != 0) {
				result->result = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
				sceClibMemset(&result->accessTokenResult, 0, sizeof(result->accessTokenResult));
			} else {
				result->result = work->result.result;
				sceClibMemset(&result->accessTokenResult, 0, sizeof(result->accessTokenResult));
				if (sceClibStrnlen(work->result.accessTokenResult.accessToken,
						   SCE_NP_SNS_FACEBOOK_LONG_ACCESS_TOKEN_LENGTH_MAX)
				    <= SCE_NP_SNS_FACEBOOK_ACCESS_TOKEN_LENGTH_MAX) {
					result->accessTokenResult.expiration = work->result.accessTokenResult.expiration;
					sceClibStrncpy(result->accessTokenResult.accessToken,
						       work->result.accessTokenResult.accessToken,
						       SCE_NP_SNS_FACEBOOK_ACCESS_TOKEN_LENGTH_MAX);
					ret = SCE_OK;
				} else {
					ret = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
				}
			}
		}
	}
	return convertResult(ret);
}

int GetResultLongToken(SceNpSnsFacebookDialogResultLongToken *result)
{
	CommonDialogNpSnsFacebookParam *work;
	SceInt32 initMode;
	SceInt32 mode;
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_NP_SNS_FB) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
	}
	{
		NpSnsFacebookApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			initMode = work->initArg.mode;
			mode = work->param.mode;
			if (isLongTokenMode(initMode)) {
				if (mode == 0) {
					result->result = work->result.result;
					sceClibMemset(&result->accessTokenResult, 0, sizeof(result->accessTokenResult));
				} else {
					sceClibMemcpy_safe(result, &work->result, sizeof(*result));
				}
			} else if (mode == 0) {
				result->result = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
				sceClibMemset(&result->accessTokenResult, 0, sizeof(result->accessTokenResult));
			} else {
				sceClibMemcpy_safe(result, &work->result, sizeof(*result));
			}
		}
	}
	return convertResult(ret);
}

int GetResultInternal(NpSnsFacebookDialogGetResultInternalArg *arg)
{
	CommonDialogNpSnsFacebookParam *work;
	SceNpSnsFacebookDialogResultLongToken *result;
	SceInt32 initMode;
	SceInt32 mode;
	int ret;

	if (arg->result == NULL || arg->internalResult == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_NP_SNS_FB) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	{
		NpSnsFacebookApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			initMode = work->initArg.mode;
			result = arg->result;
			mode = work->param.mode;
			if (isLongTokenMode(initMode)) {
				if (mode == 0) {
					result->result = work->result.result;
					sceClibMemset(&result->accessTokenResult, 0, sizeof(result->accessTokenResult));
				} else {
					sceClibMemcpy_safe(result, &work->result, sizeof(*result));
				}
			} else if (mode == 0) {
				result->result = SCE_NP_SNS_FACEBOOK_DIALOG_ERROR_PARAM;
				sceClibMemset(&result->accessTokenResult, 0, sizeof(result->accessTokenResult));
			} else {
				sceClibMemcpy_safe(result, &work->result, sizeof(*result));
			}
			sceClibMemcpy_safe(arg->internalResult, work->internalResult, sizeof(work->internalResult));
		}
	}
	return convertResult(ret);
}

int Abort(void)
{
	CommonDialogNpSnsFacebookParam *work;
	int ret;

	{
		NpSnsFacebookApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			if (work->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_NP_SNS_FB);
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
