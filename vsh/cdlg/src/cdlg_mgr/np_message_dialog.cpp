/*
 * The NP message dialog's API (sceNpMessageDialog*): its parameter check and
 * Init, and its GetResult on the worker thread. The dialog itself is
 * libcdlg_np_message's plugin. OnInit and GetResult are Sony's names (the
 * names the calls keep in the work memory); the file's name, the namespace
 * and the other names are ours.
 */
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/threadmgr.h>
#include <common_dialog.h>
#include <np_message_dialog.h>
#include "common_dialog_mgr.h"
#include "common_dialog_mgr_internal.h"

/* the dialog's own errors: no work memory for the call, another call took it */
#define NP_MESSAGE_DIALOG_ERROR_NO_WORK_MEMORY	-2146431454	/* 0x80100E22 */
#define NP_MESSAGE_DIALOG_ERROR_API_NAME	-2146431455	/* 0x80100E21 */

/* the numbers of the parameter error messages */
#define NP_MESSAGE_DIALOG_PARAM_ERROR_RESULT_RESERVED	100

/* a title ID and an NP communication ID with their ends */
#define NP_MESSAGE_DIALOG_TITLE_ID_SIZE		10
#define NP_MESSAGE_DIALOG_COMMUNICATION_ID_SIZE	13

/* the store link an attachment of a store link message starts with */
#define NP_MESSAGE_DIALOG_STORE_LINK_SCHEME	"psts:"
#define NP_MESSAGE_DIALOG_STORE_LINK_SCHEME_LENGTH	5

/* the communication ID that may send invitations that do not expire (the system's) */
#define NP_MESSAGE_DIALOG_SYSTEM_COMMUNICATION_ID	"NPXS10001_00"

/* the work memory (CommonDialogSlot::workMemory) */
typedef struct CommonDialogNpMessageParam {
	SceNpMessageDialogParam param;
	SceNpMessageDialogResult result;
	SceUInt8 reservedResult[4];
	SceNpId sendNpIds[SCE_NP_MESSAGE_DIALOG_MAX_RECIPIENTS];
	SceUInt8 pluginState[1612];
	int pluginResult;
	SceCommonDialogColor bgColor;
	char apiName[64];
} CommonDialogNpMessageParam;

COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpMessageParamResult, __builtin_offsetof(CommonDialogNpMessageParam, result) == 140);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpMessageParamNpIds, __builtin_offsetof(CommonDialogNpMessageParam, sendNpIds) == 496);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpMessageParamPlugin, __builtin_offsetof(CommonDialogNpMessageParam, pluginResult) == 2684);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpMessageParamName, __builtin_offsetof(CommonDialogNpMessageParam, apiName) == 2692);
COMMON_DIALOG_LAYOUT_CHECK(CommonDialogNpMessageParamSize, sizeof(CommonDialogNpMessageParam) == 2756);

typedef CommonDialogApiScope<CommonDialogNpMessageParam, COMMON_DIALOG_TYPE_NP_MESSAGE,
			     NP_MESSAGE_DIALOG_ERROR_NO_WORK_MEMORY,
			     NP_MESSAGE_DIALOG_ERROR_API_NAME> NpMessageApiScope;

namespace NpMessageDialog {

int GetResult(SceNpMessageDialogResult *result);

}

extern "C" SceInt32 sceNpMessageDialogInit(const SceNpMessageDialogParam *param)
{
	return OnInit(COMMON_DIALOG_TYPE_NP_MESSAGE, (void *)param);
}

/* sceNpMessageDialogInitInternal */
extern "C" SceInt32 SceCommonDialog_C6DB0740(const SceNpMessageDialogParamInternal *param)
{
	return OnInit(COMMON_DIALOG_TYPE_NP_MESSAGE, (void *)param);
}

extern "C" SceCommonDialogStatus sceNpMessageDialogGetStatus(void)
{
	return (SceCommonDialogStatus)GetStatus(COMMON_DIALOG_TYPE_NP_MESSAGE);
}

extern "C" SceInt32 sceNpMessageDialogAbort(void)
{
	return SetParamReady(COMMON_DIALOG_TYPE_NP_MESSAGE);
}

extern "C" SceInt32 sceNpMessageDialogGetResult(SceNpMessageDialogResult *result)
{
	return DispatchApiCall(COMMON_DIALOG_TYPE_NP_MESSAGE, (void *)NpMessageDialog::GetResult, 1, result);
}

extern "C" SceInt32 sceNpMessageDialogTerm(void)
{
	return Term(COMMON_DIALOG_TYPE_NP_MESSAGE);
}

namespace NpMessageDialog {

/* the titles whose detail dialog without a message shows the message list */
static inline bool showsListForDetail(const char *titleId)
{
	return sceClibStrncmp("PCSE00112", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSB00255", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSG00185", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSH00024", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSE00013", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSE00025", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSE00026", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSB00051", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSB00052", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSB00053", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSB00054", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSB00055", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSB00057", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0 ||
	       sceClibStrncmp("PCSG00039", titleId, NP_MESSAGE_DIALOG_TITLE_ID_SIZE) == 0;
}

/* the send dialog's parameter */
static inline int checkSendParam(const SceNpMessageDialogParam *param, const char *communicationId)
{
	SceIoStat stat;
	const SceChar8 *body;
	SceChar8 c;
	SceUInt32 n;

	if (param->sendParam->npIdsCount > SCE_NP_MESSAGE_DIALOG_MAX_RECIPIENTS)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->sendParam->indexIconPath != NULL &&
	    (sceIoGetstat((const char *)param->sendParam->indexIconPath, &stat) < 0 ||
	     stat.st_size > SCE_NP_MESSAGE_DIALOG_MAX_INDEX_ICON_SIZE))
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	body = param->sendParam->body;
	if (body != NULL) {
		c = *body;
		n = 0;
		if (c != 0) {
			while (n < SCE_NP_MESSAGE_DIALOG_MAX_BODY_SIZE) {
				c = *++body;
				n++;
				if (c == 0)
					break;
			}
			if (n == SCE_NP_MESSAGE_DIALOG_MAX_BODY_SIZE && c != 0)
				return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		}
	}
	if (param->sendParam->dataSize > SCE_NP_MESSAGE_DIALOG_MAX_ATTACHMENT_SIZE)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->type == SCE_NP_MESSAGE_DIALOG_TYPE_STORE_LINK_ATTACHMENT) {
		if (param->sendParam->indexIconPath != NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (param->sendParam->data == NULL ||
		    param->sendParam->dataSize < NP_MESSAGE_DIALOG_STORE_LINK_SCHEME_LENGTH ||
		    sceClibStrncmp((const char *)param->sendParam->data, NP_MESSAGE_DIALOG_STORE_LINK_SCHEME,
				   NP_MESSAGE_DIALOG_STORE_LINK_SCHEME_LENGTH) != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	if (param->type == SCE_NP_MESSAGE_DIALOG_TYPE_INVITATION_ATTACHMENT &&
	    param->sendParam->expireMinutes == 0 && !sceAppMgrIsDevelopmentMode()) {
		if (sceClibStrncmp(communicationId, NP_MESSAGE_DIALOG_SYSTEM_COMMUNICATION_ID,
				   sceClibStrnlen(NP_MESSAGE_DIALOG_SYSTEM_COMMUNICATION_ID,
						  NP_MESSAGE_DIALOG_COMMUNICATION_ID_SIZE)) != 0)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	return SCE_OK;
}

/* the caller's parameter checked, then into the work memory (the detail
   dialog of some titles turned into the list dialog in the caller's own
   parameter) */
int OnInit(void *arg)
{
	SceNpMessageDialogParam *param = (SceNpMessageDialogParam *)arg;
	char titleId[NP_MESSAGE_DIALOG_TITLE_ID_SIZE] = { 0 };
	char communicationId[NP_MESSAGE_DIALOG_COMMUNICATION_ID_SIZE] = { 0 };
	CommonDialogNpMessageParam *work;
	bool infobarShown;
	int unlockRet;
	int ret;

	if (param == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (sceAppMgrAppParamGetString(sceKernelGetProcessId(), CDLG_APP_PARAM_TITLE_ID, titleId,
				       sizeof(titleId)) != SCE_OK)
		return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
	if (showsListForDetail(titleId) && param->detailParam == NULL && param->listParam != NULL &&
	    param->mode == SCE_NP_MESSAGE_DIALOG_MODE_MESSAGE_DETAIL)
		param->mode = SCE_NP_MESSAGE_DIALOG_MODE_MESSAGE_LIST;
	if (!sceAppMgrIsDevelopmentMode()) {
		if (sceAppMgrAppParamGetString(sceKernelGetProcessId(), CDLG_APP_PARAM_NP_COMMUNICATION_ID,
					       communicationId, sizeof(communicationId)) != SCE_OK)
			return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		if (communicationId[0] == '\0') {
			sceClibPrintf("sceNpMessageDialogInit() Error param.sfo NP_COMMUNICATION_ID\n");
			return SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
		}
	}
	if (param->mode <= SCE_NP_MESSAGE_DIALOG_MODE_INVALID || param->mode > SCE_NP_MESSAGE_DIALOG_MODE_MESSAGE_GET)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->type <= SCE_NP_MESSAGE_DIALOG_TYPE_INVALID ||
	    param->type > SCE_NP_MESSAGE_DIALOG_TYPE_STORE_LINK_ATTACHMENT)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if ((param->options & SCE_NP_MESSAGE_DIALOG_OPTION_FORBID_EDIT_MESSAGE) != 0 &&
	    (param->options & SCE_NP_MESSAGE_DIALOG_OPTION_PERMIT_EDIT_PREDEFINED_MESSAGE) != 0)
		return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	if (param->mode == SCE_NP_MESSAGE_DIALOG_MODE_MESSAGE_SEND && param->sendParam != NULL) {
		ret = checkSendParam(param, communicationId);
		if (ret != SCE_OK)
			return ret;
	}
	if (param->mode == SCE_NP_MESSAGE_DIALOG_MODE_MESSAGE_GET) {
		if (param->getParam->messageBody == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		if (param->getParam->attachedData == NULL)
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
	}
	ret = AllocateWorkMemory(COMMON_DIALOG_TYPE_NP_MESSAGE, sizeof(CommonDialogNpMessageParam));
	if (ret != SCE_OK)
		return ret;
	/* the work memory taken by hand: when the info bar cannot be set, Sony's
	   returns with the dialog still locked */
	work = NULL;
	ret = LockDialog(COMMON_DIALOG_TYPE_NP_MESSAGE);
	if (ret == SCE_OK) {
		work = (CommonDialogNpMessageParam *)GetWorkMemory(COMMON_DIALOG_TYPE_NP_MESSAGE);
		if (work != NULL) {
			sceClibStrncpy(work->apiName, __FUNCTION__, sizeof(work->apiName));
			work->apiName[sizeof(work->apiName) - 1] = '\0';
			ret = SCE_OK;
		} else {
			ret = NP_MESSAGE_DIALOG_ERROR_NO_WORK_MEMORY;
			UnlockDialog(COMMON_DIALOG_TYPE_NP_MESSAGE);
		}
	}
	if (ret == SCE_OK) {
		sceClibMemset(work, 0, __builtin_offsetof(CommonDialogNpMessageParam, apiName));
		sceClibMemcpy_safe(&work->param, param, sizeof(work->param));
		work->pluginResult = -1;
		if (param->commonParam.bgColor != NULL) {
			sceClibMemcpy_safe(&work->bgColor, param->commonParam.bgColor, sizeof(work->bgColor));
			work->param.commonParam.bgColor = &work->bgColor;
		} else {
			work->bgColor.r = 38;
			work->bgColor.g = 40;
			work->bgColor.b = 46;
			work->bgColor.a = 255;
		}
		ret = checkCommonParamWithInfobar(&param->commonParam, &infobarShown, true, false);
		if (ret != SCE_OK)
			return ret;
	}
	if (work != NULL) {
		if (sceClibStrncmp(work->apiName, __FUNCTION__, sizeof(work->apiName)) != 0) {
			unlockRet = NP_MESSAGE_DIALOG_ERROR_API_NAME;
		} else {
			sceClibMemset(work->apiName, 0, sizeof(work->apiName));
			unlockRet = UnlockDialog(COMMON_DIALOG_TYPE_NP_MESSAGE);
		}
		if (unlockRet != SCE_OK && ret == SCE_OK)
			ret = unlockRet;
	}
	return ret;
}

int GetResult(SceNpMessageDialogResult *result)
{
	SceUInt32 i;
	int ret;

	if (result == NULL)
		return SCE_COMMON_DIALOG_ERROR_NULL;
	if (GetStatus(COMMON_DIALOG_TYPE_NP_MESSAGE) != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return SCE_COMMON_DIALOG_ERROR_NOT_FINISHED;
	for (i = 0; i < sizeof(result->reserved); i++) {
		if (result->reserved[i] != 0) {
			sceClibPrintf("\n\n****** sceNpMessageDialog Parameter Error : %d ******\n\n",
				      NP_MESSAGE_DIALOG_PARAM_ERROR_RESULT_RESERVED);
			return SCE_COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
		}
	}
	{
		NpMessageApiScope scope(__FUNCTION__, ret);

		if (ret == SCE_OK) {
			CommonDialogNpMessageParam *work = scope.work();

			if (work->result.userAction != SCE_NP_MESSAGE_DIALOG_USER_ACTION_MESSAGE_SEND_OK ||
			    result->sendNpIds == NULL)
				work->result.sendNpIdCount = 0;
			else
				sceClibMemcpy_safe(result->sendNpIds, work->sendNpIds, sizeof(work->sendNpIds));
			sceClibMemcpy_safe(result, &work->result, sizeof(*result));
		}
	}
	return ret;
}

}
