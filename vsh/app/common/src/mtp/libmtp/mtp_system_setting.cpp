/*
 * SceLibMtp: the system settings (SSET).
 *
 * The shell's system setting and NP account requests go to the host as events; the host
 * fetches each request's data with GetSettingInfo and answers with SendSettingInfo.
 */

#include "mtp_internal.h"

#define MTP_TAG_SSET	"SSET:"

/* a request's data phase: how much the caller's buffer sends, and whether it is the last */
typedef struct MtpSettingTransfer {
	SceUInt32 size;
	int last;
} MtpSettingTransfer;

extern "C" {

static void _mtpGetNpAccountInfoResult(MtpContext *context, void *result);

int mtpContextGetSystemSetting(MtpContext *context, int setting)
{
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_GET_SETTING_INFO;
	event.numParams = 2;
	event.param[1] = setting;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SSET, 60, "mtpTaskQueuePush failed: 0x%x\n", ret);
		return ret;
	}
	return 0;
}

int mtpContextSetSystemSetting(MtpContext *context, int setting, SceUInt32 size)
{
	MtpSettingTransfer *transfer;
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	transfer = (MtpSettingTransfer *)mtpUtilMalloc(sizeof(*transfer));
	if (transfer == NULL) {
		MTP_ERR(MTP_TAG_SSET, 77, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_SEND_SETTING_INFO;
	event.numParams = 2;
	event.param[1] = setting;
	transfer->size = size;
	transfer->last = 1;
	context->data = transfer;
	context->dataSize = sizeof(*transfer);
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SSET, 93, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(transfer);
		return ret;
	}
	return 0;
}

int mtpContextGetNpAccountInfo(MtpContext *context, SceUInt32 size)
{
	MtpSettingTransfer *transfer;
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	transfer = (MtpSettingTransfer *)mtpUtilMalloc(sizeof(*transfer));
	if (transfer == NULL) {
		MTP_ERR(MTP_TAG_SSET, 111, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_NP_ACCOUNT_INFO;
	event.numParams = 1;
	transfer->size = size;
	transfer->last = 0;
	context->data = transfer;
	context->dataSize = sizeof(*transfer);
	context->onResult = _mtpGetNpAccountInfoResult;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SSET, 127, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(transfer);
		return ret;
	}
	return 0;
}

/* the host sends a setting's value (the size of what follows) */
int mtpCmdSendSettingInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	SceUInt32 size = 0;
	MtpContext *context = NULL;
	MtpSettingTransfer *transfer;
	SceUInt32 param;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_SSET, 152, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SSET, 160, "GetContext %u failed: 0x%x\n", contextId, ret);
		ret = -2;
		goto error;
	}
	if (context->dataSize != 0) {
		if (context->dataSize != sizeof(MtpSettingTransfer)) {
			MTP_ERR(MTP_TAG_SSET, 178, "Invalid data size\n");
			ret = -2;
			goto error;
		}
		transfer = (MtpSettingTransfer *)context->data;
		if (transfer == NULL) {
			MTP_ERR(MTP_TAG_SSET, 168, "pContext->pData is NULL\n");
			ret = -2;
			goto error;
		}
		if (transfer->last == 1) {
			MTP_ERR(MTP_TAG_SSET, 173, "Invalid data size\n");
			ret = -2;
			goto error;
		}
	}
	ret = mtpCoreRecvData(&size, sizeof(size), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SSET, 186, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	if (context->sendRecv == NULL) {
		MTP_ERR(MTP_TAG_SSET, 197, "RecvData is not set\n");
		ret = -2;
		goto error;
	}
	ret = mtpContextPassRecvData(context, size, &code, 1);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_SSET, 193, "Failed for context %u\n", context->id);
		goto error;
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	param = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &param, 1, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

/* the host fetches a request's data: its size, then the caller's buffer */
int mtpCmdGetSettingInfo(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	MtpContext *context = NULL;
	MtpSettingTransfer *transfer;
	SceUInt64 total;
	int send = 1;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_SSET, 236, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SSET, 244, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpSettingTransfer)) {
		MTP_ERR(MTP_TAG_SSET, 251, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	transfer = (MtpSettingTransfer *)context->data;
	if (transfer == NULL) {
		MTP_ERR(MTP_TAG_SSET, 258, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	total = transfer->size + sizeof(transfer->size);
	ret = mtpCoreSendData(transfer, sizeof(transfer->size), total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_SSET, 268, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	if (context->sendRecv == NULL) {
		MTP_ERR(MTP_TAG_SSET, 279, "SendRecvData is not set\n");
		ret = -2;
		goto error;
	}
	ret = mtpContextPassSendData(context->sendRecv, transfer->size, total, &code,
		transfer->last);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_SSET, 275, "Failed for context %u\n", context->id);
		goto error;
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

/* the NP account request's result: the host's result parameter */
static void _mtpGetNpAccountInfoResult(MtpContext *context, void *result)
{
	if (result != NULL)
		*(SceUInt32 *)result = context->resultParam;
}

} /* extern "C" */
