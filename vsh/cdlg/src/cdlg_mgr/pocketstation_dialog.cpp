/*
 * The PocketStation dialog's API: its Init, GetResult, Abort and Resume on
 * the worker thread. The dialog itself is libcdlg_pocketstation's plugin
 * (vsh/cdlg/src/pocketstation_dialog). OnInit, GetResult, Abort and Resume
 * are Sony's names (the names the calls keep in the work memory); the
 * file's name, the namespace and the other names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <modulemgr/module_info.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* no work memory for the call, or another call took it */
#define POCKETSTATION_DIALOG_ERROR_NO_WORK_MEMORY	-2146399454	/* 0x80108B22 */
#define POCKETSTATION_DIALOG_ERROR_API_NAME		-2146399455	/* 0x80108B21 */
/* the result while the dialog has not finished; the calls return it as it is */
#define POCKETSTATION_DIALOG_RESULT_NOT_FINISHED	-2146399360	/* 0x80108B80 */

/* the state Resume sets when the dialog has none */
#define POCKETSTATION_DIALOG_STATE_RUNNING		1
#define POCKETSTATION_DIALOG_STATE_RESUME		7

/* the caller's parameter and the result: no SDK header has them; the
   dialog's own parts only its plugin reads */
typedef struct PocketStationDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceUInt8 dialogData[96];
} PocketStationDialogParam;

typedef struct PocketStationDialogResult {
	SceInt32 result;
	SceUInt8 dialogData[8];
} PocketStationDialogResult;

/* the work memory (CommonDialogSlot::workMemory) */
typedef struct CommonDialogPocketStationParam {
	int abortRequested;
	int abortResult;
	int state;
	PocketStationDialogParam param;
	PocketStationDialogResult result;
	char apiName[64];
} CommonDialogPocketStationParam;

COMMON_DIALOG_LAYOUT_CHECK(PocketStationDialogParamSize, sizeof(PocketStationDialogParam) == 176);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogPocketStationSize, sizeof(CommonDialogPocketStationParam) == 264);

typedef CommonDialogApiScope<CommonDialogPocketStationParam, COMMON_DIALOG_TYPE_POCKETSTATION,
			     POCKETSTATION_DIALOG_ERROR_NO_WORK_MEMORY,
			     POCKETSTATION_DIALOG_ERROR_API_NAME> PocketStationApiScope;

namespace PocketStationDialog {

int GetResult(PocketStationDialogResult *result);
int Abort(void);
int Resume(void);

}

extern "C" int SceCommonDialog_631DC45C(const PocketStationDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_POCKETSTATION, (void *)param);
}

extern "C" SceCommonDialogStatus SceCommonDialog_500229F0(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_POCKETSTATION);
}

extern "C" int SceCommonDialog_7030E4A6(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_POCKETSTATION, (void *)PocketStationDialog::Abort, 0);
}

extern "C" int SceCommonDialog_396D7B68(PocketStationDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_POCKETSTATION, (void *)PocketStationDialog::GetResult, 1,
			       result);
}

extern "C" int SceCommonDialog_675248CA(void)
{
	return Term(COMMON_DIALOG_TYPE_POCKETSTATION);
}

extern "C" int SceCommonDialog_63B08CA3(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_POCKETSTATION, (void *)PocketStationDialog::Resume, 0);
}

namespace PocketStationDialog {

/* what a call returns: the dialog's not-finished result as it is, else as
   any dialog's call */
static inline int convertResult(int ret)
{
	if (ret == POCKETSTATION_DIALOG_RESULT_NOT_FINISHED)
		return ret;
	return toCommonDialogResult(ret);
}

int OnInit(void *arg)
{
	const PocketStationDialogParam *param = (const PocketStationDialogParam *)arg;
	SceKernelSystemSwVersion version;
	bool infobarShown;
	int ret;

	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		version.size = sizeof(version);
		if (sceKernelGetSystemSwVersion(&version) < 0)
			return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
		if (ret == SCE_OK) {
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_POCKETSTATION,
						 sizeof(CommonDialogPocketStationParam));
			if (ret == SCE_OK) {
				PocketStationApiScope scope(__FUNCTION__, ret);

				if (ret == SCE_OK) {
					sceClibMemcpy_safe(&scope.work()->param, param, sizeof(*param));
					ret = SCE_OK;
					scope.work()->result.result = SCE_OK;
					scope.work()->state = POCKETSTATION_DIALOG_STATE_RUNNING;
				}
			}
		}
	}
	return convertResult(ret);
}

int GetResult(PocketStationDialogResult *result)
{
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_POCKETSTATION) != SCE_COMMON_DIALOG_STATUS_FINISHED) {
		result->result = POCKETSTATION_DIALOG_RESULT_NOT_FINISHED;
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	}
	{
		PocketStationApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return convertResult(ret);
}

int Abort(void)
{
	int ret;

	{
		PocketStationApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			if (scope.work()->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_POCKETSTATION);
				if (ret == SCE_OK) {
					scope.work()->abortRequested = 1;
					if (scope.work()->abortResult == 0)
						scope.work()->abortResult = 1;
				}
			}
		}
	}
	return convertResult(ret);
}

int Resume(void)
{
	int ret;

	{
		PocketStationApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			if (scope.work()->state == 0)
				scope.work()->state = POCKETSTATION_DIALOG_STATE_RESUME;
			ret = SCE_OK;
		}
	}
	return convertResult(ret);
}

}
