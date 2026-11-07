/*
 * The video import dialog's API (sceVideoImportDialog*): its parameter
 * check and its Init, GetResult and Abort on the worker thread. The dialog
 * itself is libcdlg_videoimport's plugin. _OnInit, GetResult and Abort are
 * Sony's names (the names the calls keep in the work memory); the file's
 * name and the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <videoimport_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define VIDEOIMPORT_DIALOG_ERROR_NO_WORK_MEMORY	-2146396638	/* 0x80109622 */
#define VIDEOIMPORT_DIALOG_ERROR_API_NAME	-2146396639	/* 0x80109621 */

/* the draw priorities of sceVideoImportDialogInit */
#define VIDEOIMPORT_DIALOG_PRIORITY		229
#define VIDEOIMPORT_DIALOG_DRAW_PRIORITY	120

/* a title ID of the filter: four upper case letters, five digits, NUL */
#define VIDEOIMPORT_TITLE_ID_LETTERS		4
#define VIDEOIMPORT_TITLE_ID_DIGITS		9

/* the argument of the two-argument Init (SceCommonDialog_FBE52F6D) */
typedef struct VideoImportDialogInitArg {
	SceUInt32 priority;
	SceUInt8 reserved[32];
} VideoImportDialogInitArg;

typedef CommonDialogApiScope<CommonDialogVideoImportParam, COMMON_DIALOG_TYPE_VIDEOIMPORT,
			     VIDEOIMPORT_DIALOG_ERROR_NO_WORK_MEMORY,
			     VIDEOIMPORT_DIALOG_ERROR_API_NAME> VideoImportApiScope;

namespace VideoImportDialog {

int _OnInit(const SceVideoImportDialogParam *param, int priority, int drawPriority);
int GetResult(SceVideoImportDialogResult *result);
int Abort(void);

}

using namespace VideoImportDialog;

extern "C" int sceVideoImportDialogInit(const SceVideoImportDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_VIDEOIMPORT, (void *)param);
}

extern "C" int SceCommonDialog_D474A749(void *param, void *arg)
{
	return NestInit(COMMON_DIALOG_TYPE_VIDEOIMPORT, param, arg);
}

extern "C" int SceCommonDialog_FBE52F6D(void *param, void *arg)
{
	return OnInit2(COMMON_DIALOG_TYPE_VIDEOIMPORT, param, arg);
}

extern "C" SceCommonDialogStatus sceVideoImportDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_VIDEOIMPORT);
}

extern "C" int sceVideoImportDialogGetResult(SceVideoImportDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_VIDEOIMPORT, (void *)GetResult, 1, result);
}

extern "C" int sceVideoImportDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_VIDEOIMPORT, (void *)Abort, 0);
}

extern "C" int sceVideoImportDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_VIDEOIMPORT);
}

namespace VideoImportDialog {

int checkParam(const SceVideoImportDialogParam *param)
{
	int ret;
	int num;
	int i;
	int j;

	ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->mode != SCE_VIDEOIMPORT_DIALOG_MODE_DEFAULT)
		return ret;
	if (param->visibleCategory == 0)
		return ret;
	if ((param->visibleCategory & ~SCE_VIDEOIMPORT_DIALOG_CATEGORY_DEFAULT) != 0)
		return ret;
	if (param->itemCount == 0)
		return ret;
	if (param->itemCount > SCE_VIDEOIMPORT_DIALOG_MAX_ITEM_NUM)
		return ret;
	if ((param->filterType & ~(SCE_VIDEOIMPORT_DIALOG_FILTER_SELF_RECORDING
				   | SCE_VIDEOIMPORT_DIALOG_FILTER_TITLE_ID)) != 0)
		return ret;
	if ((param->filterType & SCE_VIDEOIMPORT_DIALOG_FILTER_TITLE_ID) != 0) {
		if (param->filterTitleIdNum == 0)
			return ret;
		if (param->filterTitleIdNum > SCE_VIDEOIMPORT_DIALOG_MAX_TITLE_ID_FILTER_NUM)
			return ret;
		num = param->filterTitleIdNum;
		for (i = 0; i < num; i++) {
			for (j = 0; j < SCE_VIDEOIMPORT_DIALOG_TITLE_ID_DATA_SIZE; j++) {
				char c = (char)param->filterTitleId[i][j];

				if (j < VIDEOIMPORT_TITLE_ID_LETTERS) {
					if (c < 'A' || c > 'Z')
						return ret;
				} else if (j < VIDEOIMPORT_TITLE_ID_DIGITS) {
					if (c < '0' || c > '9')
						return ret;
				} else if (c != '\0') {
					return ret;
				}
			}
		}
	}
	if (param->itemData == NULL)
		return ret;
	for (i = 0; i < (int)sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0)
			return ret;
	}
	return SCE_OK;
}

int _OnInit(const SceVideoImportDialogParam *param, int priority, int drawPriority)
{
	CommonDialogParamOption option;
	CommonDialogVideoImportParam *work;
	const SceCommonDialogColor *bgColor;
	bool infobarShown;
	int ret;

	infobarShown = false;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else if (checkParam(param) != SCE_OK) {
		ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	} else {
		option.keepInfobar = false;
		option.hideInfobar = false;
		option.drawPriority = priority;
		ret = checkCommonParam(&option, &param->commonParam, &infobarShown);
		if (ret == SCE_OK) {
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_VIDEOIMPORT,
						 sizeof(CommonDialogVideoImportParam));
			if (ret == SCE_OK) {
				VideoImportApiScope scope(__FUNCTION__, ret);

				if (ret == SCE_OK) {
					work = scope.work();
					sceClibMemset(work, 0, __builtin_offsetof(CommonDialogVideoImportParam, apiName));
					sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
					bgColor = param->commonParam.bgColor;
					if (bgColor != NULL) {
						work->bgColor.r = bgColor->r;
						work->bgColor.g = bgColor->g;
						work->bgColor.b = bgColor->b;
						work->bgColor.a = bgColor->a;
					} else {
						work->bgColor.r = 38;
						work->bgColor.g = 40;
						work->bgColor.b = 46;
						work->bgColor.a = 255;
					}
					work->infobarShown = infobarShown ? 1 : 0;
					work->drawPriority = drawPriority;
				}
			}
		}
	}
	return toCommonDialogResult(ret);
}

int onInit(void *param)
{
	return _OnInit((const SceVideoImportDialogParam *)param, VIDEOIMPORT_DIALOG_PRIORITY,
		       VIDEOIMPORT_DIALOG_DRAW_PRIORITY);
}

int onInit2(void *param, void *arg)
{
	const VideoImportDialogInitArg *initArg = (const VideoImportDialogInitArg *)arg;
	SceUInt32 i;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (initArg->priority == 0 || initArg->priority > 255)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < sizeof(initArg->reserved); i++) {
		if (initArg->reserved[i] != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	return _OnInit((const SceVideoImportDialogParam *)param, initArg->priority, initArg->priority - 1);
}

int GetResult(SceVideoImportDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_VIDEOIMPORT) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	if (result->result != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	{
		VideoImportApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return toCommonDialogResult(ret);
}

int Abort(void)
{
	int ret;

	{
		VideoImportApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			ret = SetParamReady(COMMON_DIALOG_TYPE_VIDEOIMPORT);
	}
	return toCommonDialogResult(ret);
}

}
