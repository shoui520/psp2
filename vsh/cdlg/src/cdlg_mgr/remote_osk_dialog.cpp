/*
 * The remote OSK dialog's API (sceRemoteOSKDialog*): its Init, GetResult and
 * Abort on the worker thread. The dialog itself is libcdlg_remote_osk's
 * plugin. OnInit, GetResult and Abort are Sony's names (the names the calls
 * keep in the work memory); the file's name, the namespace and the other
 * names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <common_dialog.h>
#include <libime/libime_error.h>
#include <modulemgr/module_info.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own error for a call it does not know */
#define REMOTE_OSK_DIALOG_ERROR_UNKNOWN		-2130706687	/* 0x80FFFF01 */
/* no work memory for the call, or another call took it */
#define REMOTE_OSK_DIALOG_ERROR_WORK_MEMORY	(-2147483647 - 1)	/* 0x80000000 */

typedef CommonDialogApiScope<CommonDialogRemoteOskParam, COMMON_DIALOG_TYPE_REMOTE_OSK,
			     REMOTE_OSK_DIALOG_ERROR_WORK_MEMORY,
			     REMOTE_OSK_DIALOG_ERROR_WORK_MEMORY> RemoteOskApiScope;

namespace RemoteOskDialog {

int GetResult(RemoteOskDialogResult *result);
int Abort(void);

}

extern "C" int sceRemoteOSKDialogInit(const RemoteOskDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_REMOTE_OSK, (void *)param);
}

extern "C" int sceRemoteOSKDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_REMOTE_OSK);
}

extern "C" int sceRemoteOSKDialogAbort(void)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_REMOTE_OSK, (void *)RemoteOskDialog::Abort, 0);
}

extern "C" SceCommonDialogStatus sceRemoteOSKDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_REMOTE_OSK);
}

extern "C" int sceRemoteOSKDialogGetResult(RemoteOskDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_REMOTE_OSK, (void *)RemoteOskDialog::GetResult, 1, result);
}

namespace RemoteOskDialog {

static const SceWChar16 s_emptyTitle[] = { 0 };

/* the caller's parameter into the work memory, with a copy of its title */
int setParam(CommonDialogRemoteOskParam *work, const RemoteOskDialogParam *param)
{
	const SceWChar16 *title;
	SceWChar16 c;
	int n;

	sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
	title = param->title;
	if (title == NULL)
		title = s_emptyTitle;
	c = *title;
	n = 0;
	while (c != 0 && n < REMOTE_OSK_DIALOG_TITLE_LENGTH) {
		work->title[n] = c;
		c = *++title;
		n++;
	}
	if (n >= REMOTE_OSK_DIALOG_TITLE_LENGTH)
		work->title[n - 1] = 0;
	else
		work->title[n] = 0;
	work->param.title = work->title;
	return SCE_OK;
}

/* what a call returns: the common dialog's errors the application handles and
   the dialog's own as they are, any other error as an unexpected one */
int convertResult(int ret)
{
	switch (ret) {
	case SCE_COMMON_DIALOG_ERROR_BUSY:
	case SCE_COMMON_DIALOG_ERROR_NULL:
	case SCE_COMMON_DIALOG_ERROR_NOT_RUNNING:
	case SCE_COMMON_DIALOG_ERROR_NOT_SUPPORTED:
	case SCE_COMMON_DIALOG_ERROR_NOT_FINISHED:
	case SCE_COMMON_DIALOG_ERROR_NOT_IN_USE:
	case SCE_COMMON_DIALOG_ERROR_INVALID_INFOBAR_PARAM:
	case SCE_COMMON_DIALOG_ERROR_INVALID_BG_COLOR:
	case SCE_COMMON_DIALOG_ERROR_INVALID_DIMMER_COLOR:
	case REMOTE_OSK_DIALOG_ERROR_UNKNOWN:
		break;
	default:
		if (ret < 0)
			ret = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		break;
	}
	return ret;
}

int OnInit(void *arg)
{
	const RemoteOskDialogParam *param = (const RemoteOskDialogParam *)arg;
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
				return (int)SCE_IME_ERROR_INVALID_RESERVED;
		}
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, false, false);
		if (ret == SCE_OK) {
			ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_REMOTE_OSK, sizeof(CommonDialogRemoteOskParam));
			if (ret == SCE_OK) {
				RemoteOskApiScope scope(__FUNCTION__, ret);

				if (ret == SCE_OK) {
					ret = setParam(scope.work(), param);
					if (ret == SCE_OK) {
						scope.work()->paramSet = 1;
						scope.work()->infobarShown = infobarShown;
					}
				}
			}
		}
	}
	return convertResult(ret);
}

int GetResult(RemoteOskDialogResult *result)
{
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_REMOTE_OSK) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0)
			return (int)SCE_IME_ERROR_INVALID_RESERVED;
	}
	{
		RemoteOskApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK)
			sceClibMemcpy_safe(result, &scope.work()->result, sizeof(*result));
	}
	return convertResult(ret);
}

int Abort(void)
{
	int ret;

	{
		RemoteOskApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			if (scope.work()->abortRequested != 0) {
				ret = SCE_OK;
			} else {
				ret = SetParamReady(COMMON_DIALOG_TYPE_REMOTE_OSK);
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

}
