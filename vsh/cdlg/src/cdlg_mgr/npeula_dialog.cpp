/*
 * The NP EULA dialog's API: its Init and GetResult. The dialog itself is
 * libcdlg_npeula's plugin (SceNpEulaDialogPlugin), which fetches the EULA
 * for the NP communication ID and version given here. The file's name is
 * Sony's (npeula_dialog, in libcdlg_main); the namespace, the structures and
 * their fields are ours (the plugin's strings name its query's npcommid and
 * version), and no SDK header has them.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <libsysmodule.h>
#include <np/np_common.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's errors: an invalid parameter, a parameter without an NP
   communication ID, and the HTTP or NP module not loaded */
#define NPEULA_DIALOG_ERROR_PARAM			-2146407168	/* 0x80106D00 */
#define NPEULA_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED	-2146407163	/* 0x80106D05 */
#define NPEULA_DIALOG_ERROR_INVALID_NP_COMM_ID		-2146407157	/* 0x80106D0B */

/* the SDK version that introduced NpEulaDialogParam::option */
#define NPEULA_DIALOG_SDK_VERSION_OPTION		0x02100000
#define NPEULA_DIALOG_OPTION_MAX			1

#define NPEULA_DIALOG_DRAW_PRIORITY			120

/* the caller's parameter */
typedef struct NpEulaDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceNpCommunicationId npCommId;
	SceUInt32 version;
	SceUInt8 option;
} NpEulaDialogParam;

/* the result GetResult gives */
typedef struct NpEulaDialogResult {
	SceInt32 result;
	SceInt32 errorCode;
	SceUInt32 version;
	SceUInt8 reserved[32];
} NpEulaDialogResult;

COMMON_DIALOG_LAYOUT_CHECK(NpEulaDialogParamOption, __builtin_offsetof(NpEulaDialogParam, option) == 96);
COMMON_DIALOG_LAYOUT_CHECK(NpEulaDialogResultSize, sizeof(NpEulaDialogResult) == 44);

namespace NpEulaDialog {

int GetResult(NpEulaDialogResult *result);

}

extern "C" int SceCommonDialog_33626E2C(const NpEulaDialogParam *param)
{
	if (param != NULL && param->sdkVersion >= NPEULA_DIALOG_SDK_VERSION_OPTION
	    && param->option > NPEULA_DIALOG_OPTION_MAX)
		return NPEULA_DIALOG_ERROR_PARAM;
	return OnInit(COMMON_DIALOG_TYPE_NPEULA, (void *)param);
}

extern "C" int SceCommonDialog_BEBAEF25(void)
{
	return Term(COMMON_DIALOG_TYPE_NPEULA);
}

extern "C" int SceCommonDialog_B55CF0E3(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_NPEULA);
}

extern "C" SceCommonDialogStatus SceCommonDialog_B7D4C911(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_NPEULA);
}

extern "C" int SceCommonDialog_D1858A78(NpEulaDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NPEULA, (void *)NpEulaDialog::GetResult, 1, result);
}

namespace NpEulaDialog {

static inline int checkParam(const NpEulaDialogParam *param)
{
	if (param == NULL)
		return NPEULA_DIALOG_ERROR_PARAM;
	if (param->npCommId.data[0] == '\0')
		return NPEULA_DIALOG_ERROR_INVALID_NP_COMM_ID;
	return SCE_OK;
}

static inline int checkRequiredModules(void)
{
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTP) != SCE_OK)
		return NPEULA_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP) != SCE_OK)
		return NPEULA_DIALOG_ERROR_REQUIRED_MODULES_NOT_LOADED;
	return SCE_OK;
}

int OnInit(void *arg)
{
	const NpEulaDialogParam *param = (const NpEulaDialogParam *)arg;
	CommonDialogNpEulaParam *work;
	bool infobarShown;
	int ret;

	ret = checkParam(param);
	if (ret == SCE_OK) {
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
		if (ret == SCE_OK) {
			ret = checkRequiredModules();
			if (ret == SCE_OK) {
				restoreInfobar();
				ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NPEULA, sizeof(CommonDialogNpEulaParam));
				if (ret == SCE_OK) {
					work = (CommonDialogNpEulaParam *)GetWorkMemory(COMMON_DIALOG_TYPE_NPEULA);
					sceClibMemset(work, 0, sizeof(*work));
					sceClibMemcpy_safe(&work->npCommId, &param->npCommId, sizeof(work->npCommId));
					work->version = param->version;
					work->drawPriority = NPEULA_DIALOG_DRAW_PRIORITY;
					if (param->sdkVersion >= NPEULA_DIALOG_SDK_VERSION_OPTION)
						work->option = param->option;
					ret = SCE_OK;
				}
			}
		}
	}
	return ret;
}

int GetResult(NpEulaDialogResult *result)
{
	CommonDialogNpEulaParam *work;

	if (result == NULL)
		return NPEULA_DIALOG_ERROR_PARAM;
	if (GetStatus(COMMON_DIALOG_TYPE_NPEULA) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	work = (CommonDialogNpEulaParam *)GetWorkMemory(COMMON_DIALOG_TYPE_NPEULA);
	sceClibMemset(result, 0, sizeof(*result));
	result->result = work->result;
	result->errorCode = work->errorCode;
	result->version = work->resultVersion;
	return SCE_OK;
}

}
