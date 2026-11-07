/*
 * The store checkout dialog's API (sceStoreCheckoutDialog*): its parameter
 * checks and its Init and GetResult on the worker thread. The dialog itself
 * is libcdlg_checkout's plugin. OnInit and GetResult are Sony's names (the
 * names the calls keep in the work memory); the file's name and the
 * namespace are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <store_checkout_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define STORE_CHECKOUT_DIALOG_ERROR_NO_WORK_MEMORY	-2146426334	/* 0x80102222 */
#define STORE_CHECKOUT_DIALOG_ERROR_API_NAME		-2146426335	/* 0x80102221 */

/* the modes only the system's callers may use (sceAppMgrIsGameProgram) */
#define STORE_CHECKOUT_DIALOG_MODE_SYSTEM1		6
#define STORE_CHECKOUT_DIALOG_MODE_SYSTEM2		7
#define STORE_CHECKOUT_DIALOG_MODE_SYSTEM3		8

/* the SDK versions that introduced the version 2 modes and the install mode */
#define STORE_CHECKOUT_DIALOG_SDK_VERSION_MODE2		0x01600000
#define STORE_CHECKOUT_DIALOG_SDK_VERSION_INSTALL	0x01650000

/* the checks' message, with a number for the check that failed */
#define STORE_CHECKOUT_PARAM_ERROR_MSG	"\n\n****** SceStoreCheckoutDialog Parameter Error : %d ******\n\n"

typedef CommonDialogApiScope<CommonDialogStoreCheckoutParam, COMMON_DIALOG_TYPE_CHECKOUT,
			     STORE_CHECKOUT_DIALOG_ERROR_NO_WORK_MEMORY,
			     STORE_CHECKOUT_DIALOG_ERROR_API_NAME> StoreCheckoutApiScope;

namespace StoreCheckoutDialog {

int GetResult(SceStoreCheckoutDialogResult *result);

}

extern "C" SceInt32 sceStoreCheckoutDialogInit(const SceStoreCheckoutDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_CHECKOUT, (void *)param);
}

extern "C" SceCommonDialogStatus sceStoreCheckoutDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_CHECKOUT);
}

extern "C" SceInt32 sceStoreCheckoutDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_CHECKOUT);
}

extern "C" SceInt32 sceStoreCheckoutDialogGetResult(SceStoreCheckoutDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_CHECKOUT, (void *)StoreCheckoutDialog::GetResult, 1,
			       result);
}

extern "C" SceInt32 sceStoreCheckoutDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_CHECKOUT);
}

namespace StoreCheckoutDialog {

/* what a call returns: the dialog's own errors as they are, an unexpected
   error as the dialog's internal one */
static inline int convertResult(int ret)
{
	switch (ret) {
	case (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_INTERNAL:
	case (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_PARAM:
	case (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_NP:
	case (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_ADDCONT_BUSY:
		return ret;
	case SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL:
		return (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_INTERNAL;
	}
	if (isCommonDialogError(ret))
		return ret;
	if (ret < 0)
		return (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_INTERNAL;
	return ret;
}

int checkParam(const SceStoreCheckoutDialogParam *param)
{
	SceUInt32 i;

	if (param == NULL) {
		sceClibPrintf(STORE_CHECKOUT_PARAM_ERROR_MSG, 1);
		return SCE_COMMON_DIALOG_ERROR_NULL;
	}
	for (i = 0; i < sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0) {
			sceClibPrintf(STORE_CHECKOUT_PARAM_ERROR_MSG, 2);
			return (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_PARAM;
		}
	}
	switch (param->mode) {
	case SCE_STORE_CHECKOUT_DIALOG_MODE_CHECKOUT:
	case SCE_STORE_CHECKOUT_DIALOG_MODE_DOWNLOAD:
		break;
	case SCE_STORE_CHECKOUT_DIALOG_MODE_CHECKOUT2:
	case SCE_STORE_CHECKOUT_DIALOG_MODE_DOWNLOAD2:
	case SCE_STORE_CHECKOUT_DIALOG_MODE_APPLY:
		if (param->sdkVersion >= STORE_CHECKOUT_DIALOG_SDK_VERSION_MODE2)
			break;
		/* fall through */
	case SCE_STORE_CHECKOUT_DIALOG_MODE_INSTALL_AND_UPGRADE:
		if (param->sdkVersion >= STORE_CHECKOUT_DIALOG_SDK_VERSION_INSTALL)
			break;
		goto error;
	case STORE_CHECKOUT_DIALOG_MODE_SYSTEM1:
	case STORE_CHECKOUT_DIALOG_MODE_SYSTEM2:
	case STORE_CHECKOUT_DIALOG_MODE_SYSTEM3:
		if (sceAppMgrIsGameProgram() != 0)
			goto error;
		break;
	default:
		goto error;
	}
	return SCE_OK;

error:
	sceClibPrintf(STORE_CHECKOUT_PARAM_ERROR_MSG, 3);
	return (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_PARAM;
}

static inline int checkResult(const SceStoreCheckoutDialogResult *result)
{
	SceUInt32 i;

	if (result == NULL) {
		sceClibPrintf(STORE_CHECKOUT_PARAM_ERROR_MSG, 100);
		return SCE_COMMON_DIALOG_ERROR_NULL;
	}
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0) {
			sceClibPrintf(STORE_CHECKOUT_PARAM_ERROR_MSG, 101);
			return (int)SCE_STORE_CHECKOUT_DIALOG_ERROR_PARAM;
		}
	}
	return SCE_OK;
}

int OnInit(void *arg)
{
	const SceStoreCheckoutDialogParam *param = (const SceStoreCheckoutDialogParam *)arg;
	CommonDialogParamOption option;
	CommonDialogStoreCheckoutParam *work;
	int i;
	int ret;

	option.hideInfobar = false;
	option.keepInfobar = false;
	option.drawPriority = 229;
	ret = checkParam(param);
	if (ret >= 0) {
		ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_CHECKOUT, sizeof(CommonDialogStoreCheckoutParam));
		if (ret == SCE_OK) {
			StoreCheckoutApiScope scope(__FUNCTION__, ret);

			if (ret == SCE_OK) {
				option.keepInfobar = true;
				ret = checkCommonParam(&option, &param->commonParam, NULL);
				if (ret == SCE_OK) {
					work = scope.work();
					sceClibMemset(work, 0, __builtin_offsetof(CommonDialogStoreCheckoutParam, apiName));
					sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
					if (param->commonParam.bgColor != NULL) {
						ret = SCE_COMMON_DIALOG_ERROR_INVALID_BG_COLOR;
					} else {
						if (param->commonParam.dimmerColor != NULL) {
							sceClibMemcpy_safe(&work->dimmerColor, param->commonParam.dimmerColor,
									   sizeof(work->dimmerColor));
							work->param.commonParam.dimmerColor = &work->dimmerColor;
						}
						if (param->skuIds != NULL && param->skuIds[0] != NULL) {
							for (i = 0; (SceUInt32)i < param->skuNum; i++) {
								if (i >= STORE_CHECKOUT_DIALOG_SKU_NUM_MAX)
									break;
								sceClibStrncpy((char *)work->skuIds[i],
									       (const char *)param->skuIds[i],
									       STORE_CHECKOUT_DIALOG_SKU_ID_LEN);
								work->skuIds[i][STORE_CHECKOUT_DIALOG_SKU_ID_LEN] = '\0';
							}
						}
						if (param->serviceId != NULL) {
							sceClibStrncpy((char *)work->serviceId, (const char *)param->serviceId,
								       STORE_CHECKOUT_DIALOG_SERVICE_ID_LEN);
							work->serviceId[STORE_CHECKOUT_DIALOG_SERVICE_ID_LEN] = '\0';
						}
					}
				}
			}
		}
	}
	return convertResult(ret);
}

int GetResult(SceStoreCheckoutDialogResult *result)
{
	int ret;

	if (GetStatus(COMMON_DIALOG_TYPE_CHECKOUT) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	ret = checkResult(result);
	if (ret >= 0) {
		StoreCheckoutApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return convertResult(ret);
}

}
