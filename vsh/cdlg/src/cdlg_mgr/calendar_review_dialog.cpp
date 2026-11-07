/*
 * The calendar review dialog's API (SceCommonDialog's exports 76A52CDE,
 * E7F645AE, A785D111, 460FDFF7 and EFABDD6F; no SDK names them): its Init,
 * GetResult and Abort on the worker thread. The dialog itself is
 * libcdlg_calendar_review's plugin. _OnInit, GetResult and Abort are Sony's
 * names (the names the calls keep in the work memory); the file's name and
 * the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define CALENDAR_REVIEW_DIALOG_ERROR_NO_WORK_MEMORY	-2146399966	/* 0x80108922 */
#define CALENDAR_REVIEW_DIALOG_ERROR_API_NAME		-2146399967	/* 0x80108921 */

/* the draw priorities of the Init */
#define CALENDAR_REVIEW_DIALOG_PRIORITY		229
#define CALENDAR_REVIEW_DIALOG_DRAW_PRIORITY	120

typedef CommonDialogApiScope<CommonDialogCalendarReviewParam, COMMON_DIALOG_TYPE_CALENDAR_REVIEW,
			     CALENDAR_REVIEW_DIALOG_ERROR_NO_WORK_MEMORY,
			     CALENDAR_REVIEW_DIALOG_ERROR_API_NAME> CalendarReviewApiScope;

namespace CalendarReviewDialog {

int GetResult(CalendarReviewDialogResult *result);
int Abort(void);

}

using namespace CalendarReviewDialog;

extern "C" int SceCommonDialog_76A52CDE(const CalendarReviewDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_CALENDAR_REVIEW, (void *)param);
}

extern "C" SceCommonDialogStatus SceCommonDialog_E7F645AE(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
}

extern "C" int SceCommonDialog_A785D111(CalendarReviewDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_CALENDAR_REVIEW, (void *)GetResult, 1, result);
}

extern "C" int SceCommonDialog_460FDFF7(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_CALENDAR_REVIEW, (void *)Abort, 0);
}

extern "C" int SceCommonDialog_EFABDD6F(void)
{
	return Term(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
}

namespace CalendarReviewDialog {

int _OnInit(const CalendarReviewDialogParam *param, int priority, int drawPriority)
{
	CommonDialogParamOption option;
	CommonDialogCalendarReviewParam *work;
	const SceCommonDialogColor *bgColor;
	bool infobarShown;
	int ret;

	infobarShown = false;
	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		option.keepInfobar = false;
		option.hideInfobar = false;
		option.drawPriority = priority;
		ret = checkCommonParam(&option, &param->commonParam, &infobarShown);
		if (ret == SCE_OK) {
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_CALENDAR_REVIEW,
						 sizeof(CommonDialogCalendarReviewParam));
			if (ret == SCE_OK) {
				CalendarReviewApiScope scope(__FUNCTION__, ret);

				if (ret == SCE_OK) {
					work = scope.work();
					sceClibMemset(work, 0, __builtin_offsetof(CommonDialogCalendarReviewParam, apiName));
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
	return _OnInit((const CalendarReviewDialogParam *)param, CALENDAR_REVIEW_DIALOG_PRIORITY,
		       CALENDAR_REVIEW_DIALOG_DRAW_PRIORITY);
}

int GetResult(CalendarReviewDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_CALENDAR_REVIEW) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	if (result->result != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	{
		CalendarReviewApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return toCommonDialogResult(ret);
}

int Abort(void)
{
	int ret;

	{
		CalendarReviewApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			ret = SetParamReady(COMMON_DIALOG_TYPE_CALENDAR_REVIEW);
	}
	return toCommonDialogResult(ret);
}

}
