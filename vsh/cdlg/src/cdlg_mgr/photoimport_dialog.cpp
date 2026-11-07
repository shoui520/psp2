/*
 * The photo import dialog's API (scePhotoImportDialog*): its parameter check
 * and its Init, GetResult and Abort on the worker thread. The dialog itself
 * is libcdlg_photoimport's plugin. _OnInit, GetResult and Abort are Sony's
 * names (the names the calls keep in the work memory); the file's name and
 * the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <photoimport_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define PHOTOIMPORT_DIALOG_ERROR_NO_WORK_MEMORY	-2146428126	/* 0x80101B22 */
#define PHOTOIMPORT_DIALOG_ERROR_API_NAME	-2146428127	/* 0x80101B21 */

/* the draw priorities of scePhotoImportDialogInit */
#define PHOTOIMPORT_DIALOG_PRIORITY		229
#define PHOTOIMPORT_DIALOG_DRAW_PRIORITY	120

/* the argument of the two-argument Init (SceCommonDialog_452DA757) */
typedef struct PhotoImportDialogInitArg {
	SceUInt32 priority;
	SceUInt8 reserved[32];
} PhotoImportDialogInitArg;

typedef CommonDialogApiScope<CommonDialogPhotoImportParam, COMMON_DIALOG_TYPE_PHOTOIMPORT,
			     PHOTOIMPORT_DIALOG_ERROR_NO_WORK_MEMORY,
			     PHOTOIMPORT_DIALOG_ERROR_API_NAME> PhotoImportApiScope;

namespace PhotoImportDialog {

int _OnInit(const ScePhotoImportDialogParam *param, int priority, int drawPriority);
int GetResult(ScePhotoImportDialogResult *result);
int Abort(void);

}

using namespace PhotoImportDialog;

extern "C" int scePhotoImportDialogInit(const ScePhotoImportDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_PHOTOIMPORT, (void *)param);
}

extern "C" int SceCommonDialog_FCDA2F0D(void *param, void *arg)
{
	return NestInit(COMMON_DIALOG_TYPE_PHOTOIMPORT, param, arg);
}

extern "C" int SceCommonDialog_452DA757(void *param, void *arg)
{
	return OnInit2(COMMON_DIALOG_TYPE_PHOTOIMPORT, param, arg);
}

extern "C" SceCommonDialogStatus scePhotoImportDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_PHOTOIMPORT);
}

extern "C" int scePhotoImportDialogGetResult(ScePhotoImportDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_PHOTOIMPORT, (void *)GetResult, 1, result);
}

extern "C" int scePhotoImportDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_PHOTOIMPORT, (void *)Abort, 0);
}

extern "C" int scePhotoImportDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_PHOTOIMPORT);
}

namespace PhotoImportDialog {

static inline int checkParam(const ScePhotoImportDialogParam *param)
{
	int ret;
	int i;

	ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->mode != SCE_PHOTOIMPORT_DIALOG_MODE_DEFAULT)
		return ret;
	if (param->visibleCategory == 0)
		return ret;
	if ((param->visibleCategory & ~SCE_PHOTOIMPORT_DIALOG_CATEGORY_DEFAULT) != 0)
		return ret;
	if (param->itemCount != SCE_PHOTOIMPORT_DIALOG_MAX_ITEM_NUM)
		return ret;
	if (param->itemData == NULL)
		return ret;
	for (i = 0; i < (int)sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0)
			return ret;
	}
	return SCE_OK;
}

int _OnInit(const ScePhotoImportDialogParam *param, int priority, int drawPriority)
{
	CommonDialogParamOption option;
	CommonDialogPhotoImportParam *work;
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
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_PHOTOIMPORT,
						 sizeof(CommonDialogPhotoImportParam));
			if (ret == SCE_OK) {
				PhotoImportApiScope scope(__FUNCTION__, ret);

				if (ret == SCE_OK) {
					work = scope.work();
					sceClibMemset(work, 0, __builtin_offsetof(CommonDialogPhotoImportParam, apiName));
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
	return _OnInit((const ScePhotoImportDialogParam *)param, PHOTOIMPORT_DIALOG_PRIORITY,
		       PHOTOIMPORT_DIALOG_DRAW_PRIORITY);
}

int onInit2(void *param, void *arg)
{
	const PhotoImportDialogInitArg *initArg = (const PhotoImportDialogInitArg *)arg;
	SceUInt32 i;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (initArg->priority == 0 || initArg->priority > 255)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < sizeof(initArg->reserved); i++) {
		if (initArg->reserved[i] != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	return _OnInit((const ScePhotoImportDialogParam *)param, initArg->priority, initArg->priority - 1);
}

int GetResult(ScePhotoImportDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_PHOTOIMPORT) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	if (result->result != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	{
		PhotoImportApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return toCommonDialogResult(ret);
}

int Abort(void)
{
	int ret;

	{
		PhotoImportApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			ret = SetParamReady(COMMON_DIALOG_TYPE_PHOTOIMPORT);
	}
	return toCommonDialogResult(ret);
}

}
