/*
 * The Twitter dialog's API (sceTwDialog*): its parameter checks and its
 * Init, GetResult and Abort on the worker thread. The dialog itself is
 * libcdlg_twitter's plugin, which posts the message. OnInit, GetResult and
 * Abort are Sony's names (the names the calls keep in the work memory); the
 * file's name, the namespace and the structures are ours: no SDK header has
 * them.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr_stat.h>
#include <common_dialog.h>
#include <sysmodule/sysmodule.h>
#include <modulemgr/module_info.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the modes; the first posts through NP */
#define TW_DIALOG_MODE_NP		1
#define TW_DIALOG_MODE_2		2

/* the message's text and image path, as the work memory keeps them */
#define TW_DIALOG_TEXT_LEN_MAX		840
#define TW_DIALOG_IMAGE_PATH_LEN_MAX	256

/* the work memory's state and flags once OnInit has taken the parameter */
#define TW_DIALOG_STATE_RUNNING		1
#define TW_DIALOG_FLAG_INIT		0x1

/* the message the dialog posts */
typedef struct TwDialogMessage {
	const char *text;
	const char *imagePath;
	SceUInt8 dialogData[36];
	SceChar8 reserved[32];
} TwDialogMessage;

/* the caller's parameter and the result */
typedef struct TwDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceInt32 mode;
	const TwDialogMessage *message;
	SceChar8 reserved[32];
} TwDialogParam;

typedef struct TwDialogResult {
	SceInt32 result;
	SceInt32 dialogData;
	SceChar8 reserved[32];
} TwDialogResult;

/* the work memory (CommonDialogSlot::workMemory): the parameter, with the
   message and its strings copied for the plugin */
typedef struct CommonDialogTwParam {
	SceInt32 state;
	SceUInt32 flags;
	SceInt32 abortResult;
	SceInt32 abortRequested;
	TwDialogParam param;
	TwDialogMessage message;
	SceUInt8 dialogData[8];
	char text[TW_DIALOG_TEXT_LEN_MAX];
	char imagePath[TW_DIALOG_IMAGE_PATH_LEN_MAX];
	TwDialogResult result;
	char apiName[64];
} CommonDialogTwParam;

COMMON_DIALOG_LAYOUT_CHECK(TwDialogMessageSize, sizeof(TwDialogMessage) == 76);
COMMON_DIALOG_LAYOUT_CHECK(TwDialogParamSize, sizeof(TwDialogParam) == 120);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogTwText, __builtin_offsetof(CommonDialogTwParam, text) == 220);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogTwSize, sizeof(CommonDialogTwParam) == 1420);

typedef CommonDialogApiScope<CommonDialogTwParam, COMMON_DIALOG_TYPE_TWITTER, TW_DIALOG_ERROR_NO_WORK_MEMORY,
			     TW_DIALOG_ERROR_API_NAME, COMMON_DIALOG_TYPE_MSG> TwApiScope;

namespace TwDialog {

int GetResult(TwDialogResult *result);
int Abort(void);

}

using namespace TwDialog;

extern "C" int sceTwDialogInit(const TwDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_TWITTER, (void *)param);
}

extern "C" SceCommonDialogStatus sceTwDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_TWITTER);
}

extern "C" int sceTwDialogGetResult(TwDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_TWITTER, (void *)GetResult, 1, result);
}

extern "C" int sceTwDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_TWITTER);
}

extern "C" int sceTwDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_TWITTER, (void *)Abort, 0);
}

namespace TwDialog {

/* what a call returns: the parameter and plugin errors as they are, else as
   any dialog's call */
static inline int convertResult(int ret)
{
	switch (ret) {
	case TW_DIALOG_ERROR_PARAM:
	case TW_DIALOG_ERROR_PLUGIN:
		return ret;
	}
	return toCommonDialogResult(ret);
}

/* the message: 0, or the number of the check that failed */
int checkMessage(const TwDialogMessage *message)
{
	SceIoStat stat;
	SceUInt32 len;
	SceUInt32 i;

	if (message == NULL)
		return 10;
	if (message->text == NULL)
		return 12;
	len = 0;
	while (message->text[len] != '\0') {
		if (len >= TW_DIALOG_TEXT_LEN_MAX)
			break;
		len++;
	}
	if (len == TW_DIALOG_TEXT_LEN_MAX && message->text[len] != '\0')
		return 13;
	if (message->imagePath == NULL)
		return 14;
	if (message->imagePath[0] != '\0' && sceIoGetstat(message->imagePath, &stat) != SCE_OK)
		return 15;
	for (i = 0; i < sizeof(message->reserved); i++) {
		if (message->reserved[i] != 0)
			return 11;
	}
	return 0;
}

/* the parameter: 0, the number of the check that failed, or an error */
int checkParam(const TwDialogParam *param)
{
	SceKernelSystemSwVersion version;
	SceUInt32 i;
	int ret;

	version.size = sizeof(version);
	if (sceKernelGetSystemSwVersion(&version) < 0)
		return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
	if (param->mode != TW_DIALOG_MODE_NP && param->mode != TW_DIALOG_MODE_2)
		return 2;
	if (param->commonParam.bgColor == NULL)
		return 4;
	ret = checkMessage(param->message);
	if (ret != 0)
		return ret;
	for (i = 0; i < sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0)
			return 3;
	}
	return 0;
}

/* the parameter into the work memory, the message and its strings with it */
int copyParam(CommonDialogTwParam *work, const TwDialogParam *param)
{
	sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
	if (param->mode > 0 && param->mode <= TW_DIALOG_MODE_2 && param->message != NULL) {
		sceClibMemcpy_safe(&work->message, param->message, sizeof(work->message));
		work->param.message = &work->message;
		if (param->message->text != NULL) {
			sceClibStrncpy(work->text, param->message->text, sizeof(work->text));
			work->text[sizeof(work->text) - 1] = '\0';
			((TwDialogMessage *)work->param.message)->text = work->text;
		}
		if (param->message->imagePath != NULL) {
			sceClibStrncpy(work->imagePath, param->message->imagePath, sizeof(work->imagePath));
			work->imagePath[sizeof(work->imagePath) - 1] = '\0';
			((TwDialogMessage *)work->param.message)->imagePath = work->imagePath;
		}
	}
	return SCE_OK;
}

int OnInit(void *arg)
{
	const TwDialogParam *param = (const TwDialogParam *)arg;
	CommonDialogTwParam *work;
	bool infobarShown;
	int ret;

	infobarShown = false;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		ret = checkParam(param);
		if (ret < 0)
			return ret;
		if (ret > 0) {
			sceClibPrintf(TW_DIALOG_PARAM_ERROR_MSG, ret);
			return TW_DIALOG_ERROR_PARAM;
		}
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NET) < 0)
			return TW_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTPS) < 0)
			return TW_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
		if (param->mode == TW_DIALOG_MODE_NP && sceSysmoduleIsLoaded(SCE_SYSMODULE_NP) < 0)
			return TW_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_TWITTER, sizeof(CommonDialogTwParam));
		if (ret == SCE_OK) {
			TwApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				work = scope.work();
				sceClibMemset(work, 0, __builtin_offsetof(CommonDialogTwParam, apiName));
				ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
				if (ret == SCE_OK) {
					ret = copyParam(work, param);
					if (ret == SCE_OK) {
						work->state = TW_DIALOG_STATE_RUNNING;
						work->flags |= TW_DIALOG_FLAG_INIT;
					}
				}
			}
		}
	}
	return convertResult(ret);
}

int GetResult(TwDialogResult *result)
{
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_TWITTER) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0) {
			sceClibPrintf(TW_DIALOG_PARAM_ERROR_MSG, 100);
			return TW_DIALOG_ERROR_PARAM;
		}
	}
	{
		TwApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return convertResult(ret);
}

int Abort(void)
{
	CommonDialogTwParam *work;
	int ret;

	{
		TwApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			if (work->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_TWITTER);
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
