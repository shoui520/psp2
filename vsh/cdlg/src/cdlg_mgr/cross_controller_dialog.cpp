/*
 * The cross controller dialog's API (sceCrossControllerDialog*): its Init and
 * GetResult. The dialog itself is libcdlg_cross_controller's plugin
 * (SceCrossCtlDialogPlugin), which connects to the PS3 title. The file's
 * name and the namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <cross_controller_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

#define CROSS_CONTROLLER_DIALOG_DRAW_PRIORITY	120

namespace CrossControllerDialog {

int GetResult(SceCrossControllerDialogResult *result);

}

extern "C" SceInt32 sceCrossControllerDialogInit(const SceCrossControllerDialogParam *initParam)
{
	return OnInit(COMMON_DIALOG_TYPE_CROSS_CONTROLLER, (void *)initParam);
}

extern "C" SceInt32 sceCrossControllerDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
}

extern "C" SceInt32 sceCrossControllerDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
}

extern "C" SceCommonDialogStatus sceCrossControllerDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
}

extern "C" SceInt32 sceCrossControllerDialogGetResult(SceCrossControllerDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_CROSS_CONTROLLER, (void *)CrossControllerDialog::GetResult, 1,
			       result);
}

namespace CrossControllerDialog {

static inline bool isColor(const SceCommonDialogColor *color, SceUInt8 r, SceUInt8 g, SceUInt8 b, SceUInt8 a)
{
	return color->r == r && color->g == g && color->b == b && color->a == a;
}

/* the dimmer may only be black, clear or opaque, and the PS3 title and its
   version must be given */
static inline int checkParam(const SceCrossControllerDialogParam *param)
{
	const SceCommonDialogColor *dimmerColor;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	dimmerColor = param->commonParam.dimmerColor;
	if (dimmerColor != NULL && !isColor(dimmerColor, 0, 0, 0, 0) && !isColor(dimmerColor, 0, 0, 0, 255))
		return SCE_COMMON_DIALOG_ERROR_INVALID_DIMMER_COLOR;
	if (param->ps3TitleId[0] == '\0' || param->ps3AppMinVer[0] == '\0')
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	return SCE_OK;
}

int OnInit(void *arg)
{
	const SceCrossControllerDialogParam *param = (const SceCrossControllerDialogParam *)arg;
	CommonDialogCrossControllerParam *work;
	bool infobarShown;
	int ret;

	ret = checkParam(param);
	if (ret == SCE_OK) {
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
		if (ret == SCE_OK) {
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_CROSS_CONTROLLER,
						 sizeof(CommonDialogCrossControllerParam));
			if (ret == SCE_OK) {
				work = new (GetWorkMemory(COMMON_DIALOG_TYPE_CROSS_CONTROLLER))
					CommonDialogCrossControllerParam(param->ps3TitleId, param->ps3AppMinVer,
									 param->isNoCancel == SCE_FALSE);
				if (work != NULL)
					work->drawPriority = CROSS_CONTROLLER_DIALOG_DRAW_PRIORITY;
			}
		}
	}
	return ret;
}

int GetResult(SceCrossControllerDialogResult *result)
{
	CommonDialogCrossControllerParam *work;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_CROSS_CONTROLLER) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	work = (CommonDialogCrossControllerParam *)GetWorkMemory(COMMON_DIALOG_TYPE_CROSS_CONTROLLER);
	sceClibMemset(result, 0, sizeof(*result));
	result->inaddr = work->inaddr;
	result->result = work->result;
	return SCE_OK;
}

}
