/*
 * The trophy setup dialog's API (sceNpTrophySetupDialog*): its Init and
 * GetResult on the worker thread. The dialog itself is
 * libcdlg_trophy_setup's plugin. OnInit and GetResult are Sony's names (the
 * names the calls keep in the work memory); the file's name and the
 * namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <np_trophy_setup_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define TROPHY_SETUP_DIALOG_ERROR_NO_WORK_MEMORY	-2146425310	/* 0x80102622 */
#define TROPHY_SETUP_DIALOG_ERROR_API_NAME		-2146425311	/* 0x80102621 */

/*
 * The work memory (CommonDialogSlot::workMemory): the caller's parameter,
 * the result for GetResult, and copies of what the parameter's common part
 * points to, which the copied parameter then points to instead.
 */
typedef struct CommonDialogTrophySetupParam {
	SceNpTrophySetupDialogParam param;
	SceNpTrophySetupDialogResult result;
	SceCommonDialogInfobarParam infobarParam;
	SceCommonDialogColor bgColor;
	SceCommonDialogColor dimmerColor;
	char apiName[64];
} CommonDialogTrophySetupParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogTrophySetupSize, sizeof(CommonDialogTrophySetupParam) == 464);

typedef CommonDialogApiScope<CommonDialogTrophySetupParam, COMMON_DIALOG_TYPE_TROPHY_SETUP,
			     TROPHY_SETUP_DIALOG_ERROR_NO_WORK_MEMORY,
			     TROPHY_SETUP_DIALOG_ERROR_API_NAME> TrophySetupApiScope;

namespace TrophySetupDialog {

int GetResult(SceNpTrophySetupDialogResult *result);

}

extern "C" SceInt32 sceNpTrophySetupDialogInit(SceNpTrophySetupDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_TROPHY_SETUP, (void *)param);
}

extern "C" SceCommonDialogStatus sceNpTrophySetupDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_TROPHY_SETUP);
}

extern "C" SceInt32 sceNpTrophySetupDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_TROPHY_SETUP);
}

extern "C" SceInt32 sceNpTrophySetupDialogGetResult(SceNpTrophySetupDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_TROPHY_SETUP, (void *)TrophySetupDialog::GetResult, 1,
			       result);
}

extern "C" SceInt32 sceNpTrophySetupDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_TROPHY_SETUP);
}

namespace TrophySetupDialog {

int OnInit(void *arg)
{
	const SceNpTrophySetupDialogParam *param = (const SceNpTrophySetupDialogParam *)arg;
	const SceCommonDialogParam *commonParam;
	CommonDialogTrophySetupParam *work;
	bool infobarShown;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	/* the dialog draws its own background */
	if (param->commonParam.bgColor != NULL)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
	if (ret != SCE_OK)
		return ret;
	if (param->options != 0
	    && (param->options & ~SCE_NP_TROPHY_SETUP_DIALOG_OPTION_SHOW_PROCESSING_DIALOG) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_TROPHY_SETUP, sizeof(CommonDialogTrophySetupParam));
	if (ret == SCE_OK) {
		TrophySetupApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
			commonParam = &param->commonParam;
			if (commonParam->infobarParam != NULL) {
				sceClibMemcpy_safe(&work->infobarParam, commonParam->infobarParam,
						   sizeof(work->infobarParam));
				work->param.commonParam.infobarParam = &work->infobarParam;
			}
			if (commonParam->bgColor != NULL) {
				sceClibMemcpy_safe(&work->bgColor, commonParam->bgColor, sizeof(work->bgColor));
				work->param.commonParam.bgColor = &work->bgColor;
			}
			if (commonParam->dimmerColor != NULL) {
				sceClibMemcpy_safe(&work->dimmerColor, commonParam->dimmerColor,
						   sizeof(work->dimmerColor));
				work->param.commonParam.dimmerColor = &work->dimmerColor;
			}
		}
	}
	return ret;
}

int GetResult(SceNpTrophySetupDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_TROPHY_SETUP) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	{
		TrophySetupApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return ret;
}

}
