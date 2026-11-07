/*
 * The compat dialog's API: its Init, GetResult, Abort and Resume on the
 * worker thread. The dialog itself is libcdlg_compat's plugin
 * (SceCompatDialogPlugin), which the PSP emulator calls for the PSP's
 * utility dialogs. OnInit, GetResult, Abort and Resume are Sony's names (the
 * names the calls keep in the work memory); the file's name, the namespace
 * and the other names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <modulemgr/module_info.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* no work memory for the call, or another call took it */
#define COMPAT_DIALOG_ERROR_NO_WORK_MEMORY	-2146412510	/* 0x80105822 */
#define COMPAT_DIALOG_ERROR_API_NAME		-2146412511	/* 0x80105821 */
/* a reserved field is not cleared; the calls return it as it is */
#define COMPAT_DIALOG_ERROR_PARAM		-2146412509	/* 0x80105823 */

/* the state Init sets, and the one Resume sets when the dialog has none */
#define COMPAT_DIALOG_STATE_RUNNING		1
#define COMPAT_DIALOG_STATE_RESUME		7

/* the caller's parameter and the result: no SDK header has them; the
   dialog's own parts only its plugin reads */
typedef struct CompatDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceUInt8 dialogData[160];
	SceUInt8 reserved[32];
} CompatDialogParam;

typedef struct CompatDialogResult {
	SceUInt8 dialogData[68];
	SceUInt8 reserved[32];
} CompatDialogResult;

/* the work memory (CommonDialogSlot::workMemory) */
typedef struct CommonDialogCompatParam {
	int abortRequested;
	int abortResult;
	int state;
	CompatDialogParam param;
	CompatDialogResult result;
	char apiName[64];
} CommonDialogCompatParam;

COMMON_DIALOG_LAYOUT_CHECK(CompatDialogParamSize, sizeof(CompatDialogParam) == 272);
COMMON_DIALOG_LAYOUT_CHECK(CompatDialogResultSize, sizeof(CompatDialogResult) == 100);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogCompatSize, sizeof(CommonDialogCompatParam) == 448);

typedef CommonDialogApiScope<CommonDialogCompatParam, COMMON_DIALOG_TYPE_COMPAT,
			     COMPAT_DIALOG_ERROR_NO_WORK_MEMORY,
			     COMPAT_DIALOG_ERROR_API_NAME> CompatApiScope;

namespace CompatDialog {

int GetResult(CompatDialogResult *result);
int Abort(void);
int Resume(void);

}

extern "C" int SceCommonDialog_9FA21E09(const CompatDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_COMPAT, (void *)param);
}

extern "C" SceCommonDialogStatus SceCommonDialog_FA26EB30(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_COMPAT);
}

extern "C" int SceCommonDialog_A68A44EA(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_COMPAT, (void *)CompatDialog::Abort, 0);
}

extern "C" int SceCommonDialog_64F65B8D(CompatDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_COMPAT, (void *)CompatDialog::GetResult, 1, result);
}

extern "C" int SceCommonDialog_C2D6F7F2(void)
{
	return Term(COMMON_DIALOG_TYPE_COMPAT);
}

extern "C" int SceCommonDialog_ED2F1495(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_COMPAT, (void *)CompatDialog::Resume, 0);
}

namespace CompatDialog {

int OnInit(void *arg)
{
	const CompatDialogParam *param = (const CompatDialogParam *)arg;
	SceKernelSystemSwVersion version;
	bool infobarShown;
	SceUInt32 i;
	int ret;

	if (param == NULL) {
		ret = SCE_COMMON_DIALOG_ERROR_NULL;
	} else {
		version.size = sizeof(version);
		if (sceKernelGetSystemSwVersion(&version) < 0)
			return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		for (i = 0; i < sizeof(param->reserved); i++) {
			if (param->reserved[i] != 0)
				return COMPAT_DIALOG_ERROR_PARAM;
		}
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
		if (ret == SCE_OK) {
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_COMPAT, sizeof(CommonDialogCompatParam));
			if (ret == SCE_OK) {
				CompatApiScope scope(__FUNCTION__, ret);

				if (ret == SCE_OK) {
					sceClibMemcpy_safe(&scope.work()->param, param, sizeof(*param));
					scope.work()->state = COMPAT_DIALOG_STATE_RUNNING;
					ret = SCE_OK;
				}
			}
		}
	}
	return toCommonDialogResult(ret);
}

int GetResult(CompatDialogResult *result)
{
	CommonDialogCompatParam *work;
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_COMPAT) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return COMPAT_DIALOG_ERROR_PARAM;
	}
	{
		CompatApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			if (work != NULL)
				sceClibMemcpy_safe(result, &work->result, sizeof(*result));
		}
	}
	return toCommonDialogResult(ret);
}

int Abort(void)
{
	CommonDialogCompatParam *work;
	int ret;

	{
		CompatApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			if (work != NULL) {
				if (work->abortRequested != 0) {
					ret = SCE_OK;
				} else {
					ret = SetParamReady(COMMON_DIALOG_TYPE_COMPAT);
					if (ret == SCE_OK) {
						work->abortRequested = 1;
						if (work->abortResult == 0)
							work->abortResult = 1;
					}
				}
			}
		}
	}
	return toCommonDialogResult(ret);
}

int Resume(void)
{
	CommonDialogCompatParam *work;
	int ret;

	{
		CompatApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			work = scope.work();
			if (work != NULL) {
				if (work->state == 0)
					work->state = COMPAT_DIALOG_STATE_RESUME;
				ret = SCE_OK;
			}
		}
	}
	return toCommonDialogResult(ret);
}

}
