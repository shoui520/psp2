/*
 * The photo review dialog's API (scePhotoReviewDialog*): its parameter
 * check and its Init, GetResult and Abort on the worker thread. The dialog
 * itself is libcdlg_photoreview's plugin. _OnInit, GetResult and Abort are
 * Sony's names (the names the calls keep in the work memory); the file's
 * name and the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <libsysmodule.h>
#include <ces/libces_ucs.h>
#include <photoreview_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define PHOTOREVIEW_DIALOG_ERROR_NO_WORK_MEMORY		-2146420958	/* 0x80109122 */
#define PHOTOREVIEW_DIALOG_ERROR_API_NAME		-2146420959	/* 0x80109121 */
/* the default mode exports the photo: the application has not loaded the
   photo export module; the calls return it as it is */
#define PHOTOREVIEW_DIALOG_ERROR_PHOTO_EXPORT_NOT_LOADED	-2146420984	/* 0x80109108 */

/* the draw priorities of scePhotoReviewDialogInit */
#define PHOTOREVIEW_DIALOG_PRIORITY		229
#define PHOTOREVIEW_DIALOG_DRAW_PRIORITY	120

/* the argument of the two-argument Init (SceCommonDialog_AC20DAF9) */
typedef struct PhotoReviewDialogInitArg {
	SceUInt32 priority;
	SceUInt8 reserved[32];
} PhotoReviewDialogInitArg;

typedef CommonDialogApiScope<CommonDialogPhotoReviewParam, COMMON_DIALOG_TYPE_PHOTOREVIEW,
			     PHOTOREVIEW_DIALOG_ERROR_NO_WORK_MEMORY,
			     PHOTOREVIEW_DIALOG_ERROR_API_NAME> PhotoReviewApiScope;

namespace PhotoReviewDialog {

int _OnInit(const ScePhotoReviewDialogParam *param, int priority, int drawPriority);
int GetResult(ScePhotoReviewDialogResult *result);
int Abort(void);

}

using namespace PhotoReviewDialog;

extern "C" SceInt32 scePhotoReviewDialogInit(const ScePhotoReviewDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_PHOTOREVIEW, (void *)param);
}

extern "C" int SceCommonDialog_AC20DAF9(void *param, void *arg)
{
	return OnInit2(COMMON_DIALOG_TYPE_PHOTOREVIEW, param, arg);
}

extern "C" SceCommonDialogStatus scePhotoReviewDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_PHOTOREVIEW);
}

extern "C" SceInt32 scePhotoReviewDialogGetResult(ScePhotoReviewDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_PHOTOREVIEW, (void *)GetResult, 1, result);
}

extern "C" SceInt32 scePhotoReviewDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_PHOTOREVIEW, (void *)Abort, 0);
}

extern "C" SceInt32 scePhotoReviewDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_PHOTOREVIEW);
}

namespace PhotoReviewDialog {

/* the characters of the first len bytes of a UTF-8 string, -1 when they
   are not UTF-8 */
static inline int getUtf8Length(const SceChar8 *str, int len)
{
	SceUInt32 used;
	SceUInt32 pos;
	int rest;
	int num;

	num = 0;
	pos = 0;
	rest = len;
	while ((SceUInt32)len > pos) {
		used = 0;
		if (sceCesUtf8CheckCode((const uint8_t *)&str[pos], rest, &used) != 0)
			return -1;
		num++;
		pos += used;
		rest -= used;
	}
	return num;
}

int checkParam(const ScePhotoReviewDialogParam *param)
{
	int ret;
	int len;
	int num;
	int i;

	ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->mode < SCE_PHOTOREVIEW_DIALOG_MODE_DEFAULT
	    || param->mode > SCE_PHOTOREVIEW_DIALOG_MODE_ONLY_REVIEW)
		return ret;
	if (param->fileName[0] == '\0')
		return ret;
	if (param->workMemory == NULL)
		return ret;
	if (param->workMemorySize < SCE_PHOTOREVIEW_DIALOG_DEFAULT_WORKMEMORY_SIZE)
		return ret;
	if (param->exportParam != NULL) {
		if (param->exportParam->photoTitle != NULL) {
			len = sceClibStrnlen((const char *)param->exportParam->photoTitle,
					     SCE_PHOTOREVIEW_DIALOG_MAX_PHOTO_TITLE_SIZE);
			if (len >= SCE_PHOTOREVIEW_DIALOG_MAX_PHOTO_TITLE_SIZE)
				return ret;
			num = getUtf8Length(param->exportParam->photoTitle, len);
			if (num == -1 || num > SCE_PHOTOREVIEW_DIALOG_MAX_PHOTO_TITLE_LENGTH)
				return ret;
		}
		if (param->exportParam->gameTitle != NULL) {
			len = sceClibStrnlen((const char *)param->exportParam->gameTitle,
					     SCE_PHOTOREVIEW_DIALOG_MAX_GAME_TITLE_SIZE);
			if (len >= SCE_PHOTOREVIEW_DIALOG_MAX_GAME_TITLE_SIZE)
				return ret;
			num = getUtf8Length(param->exportParam->gameTitle, len);
			if (num == -1 || num > SCE_PHOTOREVIEW_DIALOG_MAX_GAME_TITLE_LENGTH)
				return ret;
		}
		if (param->exportParam->gameComment != NULL) {
			len = sceClibStrnlen((const char *)param->exportParam->gameComment,
					     SCE_PHOTOREVIEW_DIALOG_MAX_GAME_COMMENT_SIZE);
			if (len >= SCE_PHOTOREVIEW_DIALOG_MAX_GAME_COMMENT_SIZE)
				return ret;
			num = getUtf8Length(param->exportParam->gameComment, len);
			if (num == -1 || num > SCE_PHOTOREVIEW_DIALOG_MAX_GAME_COMMENT_LENGTH)
				return ret;
		}
	}
	for (i = 0; i < (int)sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0)
			return ret;
	}
	return SCE_OK;
}

/* what a call returns: the photo export error as it is, else as any
   dialog's call */
static inline int convertResult(int ret)
{
	if (ret == PHOTOREVIEW_DIALOG_ERROR_PHOTO_EXPORT_NOT_LOADED)
		return ret;
	return toCommonDialogResult(ret);
}

int _OnInit(const ScePhotoReviewDialogParam *param, int priority, int drawPriority)
{
	CommonDialogParamOption option;
	CommonDialogPhotoReviewParam *work;
	const SceCommonDialogColor *bgColor;
	bool infobarShown;
	int ret;

	infobarShown = false;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else if (param->mode == SCE_PHOTOREVIEW_DIALOG_MODE_DEFAULT
		   && sceSysmoduleIsLoaded(SCE_SYSMODULE_PHOTO_EXPORT) != SCE_OK) {
		ret = PHOTOREVIEW_DIALOG_ERROR_PHOTO_EXPORT_NOT_LOADED;
	} else if (checkParam(param) != SCE_OK) {
		ret = SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	} else {
		option.keepInfobar = false;
		option.hideInfobar = false;
		option.drawPriority = priority;
		ret = checkCommonParam(&option, &param->commonParam, &infobarShown);
		if (ret == SCE_OK) {
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_PHOTOREVIEW,
						 sizeof(CommonDialogPhotoReviewParam));
			if (ret == SCE_OK) {
				PhotoReviewApiScope scope(__FUNCTION__, ret);

				if (ret == SCE_OK) {
					work = scope.work();
					sceClibMemset(work, 0, __builtin_offsetof(CommonDialogPhotoReviewParam, apiName));
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
					if (param->exportParam != NULL) {
						work->param.exportParam = &work->exportParam;
						if (param->exportParam->photoTitle != NULL) {
							sceClibStrncpy((char *)work->photoTitle,
								       (const char *)param->exportParam->photoTitle,
								       sizeof(work->photoTitle));
							work->exportParam.photoTitle = work->photoTitle;
						}
						if (param->exportParam->gameTitle != NULL) {
							sceClibStrncpy((char *)work->gameTitle,
								       (const char *)param->exportParam->gameTitle,
								       sizeof(work->gameTitle));
							work->exportParam.gameTitle = work->gameTitle;
						}
						if (param->exportParam->gameComment != NULL) {
							sceClibStrncpy((char *)work->gameComment,
								       (const char *)param->exportParam->gameComment,
								       sizeof(work->gameComment));
							work->exportParam.gameComment = work->gameComment;
						}
					}
				}
			}
		}
	}
	return convertResult(ret);
}

int onInit(void *param)
{
	return _OnInit((const ScePhotoReviewDialogParam *)param, PHOTOREVIEW_DIALOG_PRIORITY,
		       PHOTOREVIEW_DIALOG_DRAW_PRIORITY);
}

int onInit2(void *param, void *arg)
{
	const PhotoReviewDialogInitArg *initArg = (const PhotoReviewDialogInitArg *)arg;
	SceUInt32 i;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (initArg->priority == 0 || initArg->priority > 255)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < sizeof(initArg->reserved); i++) {
		if (initArg->reserved[i] != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	return _OnInit((const ScePhotoReviewDialogParam *)param, initArg->priority, initArg->priority - 1);
}

int GetResult(ScePhotoReviewDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_PHOTOREVIEW) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	if (result->result != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	{
		PhotoReviewApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return convertResult(ret);
}

int Abort(void)
{
	int ret;

	{
		PhotoReviewApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			ret = SetParamReady(COMMON_DIALOG_TYPE_PHOTOREVIEW);
	}
	return convertResult(ret);
}

}
