/*
 * SceLibMtp: the network operations the host runs for the system software.
 *
 * A caller's request goes to the host as an RpcNetOperation event with the data it sends;
 * the host fetches that data with GetNetOperationData and returns its answer with
 * SendNetOperationData, which the caller reads when it ends the request.
 */

#include "mtp_internal.h"

#define MTP_TAG_NET	""

/* the most of the host's answer a caller gets */
#define MTP_NET_OPERATION_RESULT_MAX	1024

extern "C" {

static void _mtpRpcNetOperationResult(MtpContext *context, void *result);

int mtpContextRpcNetOperation(MtpContext *context, int type, const void *data, SceSize size)
{
	MtpNetOperation *operation;
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	operation = (MtpNetOperation *)mtpUtilMalloc(sizeof(*operation));
	if (operation == NULL) {
		MTP_ERR(MTP_TAG_NET, 125, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(operation, 0, sizeof(*operation));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_RPC_NET_OPERATION;
	event.numParams = 2;
	event.param[1] = 0;
	if (data != NULL && size != 0) {
		operation->sendData = mtpUtilMalloc(size);
		if (operation->sendData == NULL) {
			MTP_ERR(MTP_TAG_NET, 138, "no memory\n");
			mtpUtilFree(operation);
			return SCE_MTP_ERROR_NO_MEMORY;
		}
		sceClibMemcpy_safe(operation->sendData, data, size);
	}
	operation->sendSize = size;
	operation->type = type;
	context->data = operation;
	context->dataSize = sizeof(*operation);
	context->onResult = _mtpRpcNetOperationResult;
	context->opcode = MTP_OPERATION_REQUEST_RPC_NET_OPERATION;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET, 155, "mtpTaskQueuePush failed: 0x%x\n", ret);
		if (operation->sendData != NULL) {
			mtpUtilFree(operation->sendData);
			operation->sendData = NULL;
		}
		mtpUtilFree(operation);
		return ret;
	}
	return 0;
}

/* the host fetches what the request sends */
int mtpCmdGetNetOperationData(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	MtpNetOperation *net = NULL;
	MtpContext *context = NULL;
	int send = 1;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_NET, 188, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET, 196, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpNetOperation)) {
		MTP_ERR(MTP_TAG_NET, 204, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->data == NULL) {
		MTP_ERR(MTP_TAG_NET, 211, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	net = (MtpNetOperation *)context->data;
	if (context->sendRecv != NULL && net->type == MTP_NET_OPERATION_SEND) {
		ret = mtpContextPassSendData(context->sendRecv, net->sendSize, net->sendSize, &code,
			0);
		if (ret != 0) {
			MTP_ERR(MTP_TAG_NET, 223, "Failed for context %u: 0x%x\n", context->id, ret);
			send = ret == -2;
			goto error;
		}
	} else {
		ret = mtpCoreSendData(net->sendData, net->sendSize, net->sendSize);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_NET, 233, "mtpCoreSendData64 failed: 0x%x\n", ret);
			send = 0;
			goto error;
		}
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	if (net->sendData != NULL) {
		mtpUtilFree(net->sendData);
		net->sendData = NULL;
	}
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	/* Sony's: also before the request's data is known */
	if (net->sendData != NULL) {
		mtpUtilFree(net->sendData);
		net->sendData = NULL;
	}
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

/* the host sends its answer (its size, then the answer) */
int mtpCmdSendNetOperationData(const MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	SceUInt32 size = 0;
	MtpContext *context = NULL;
	MtpNetOperation *net;
	SceUInt32 param;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_NET, 282, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET, 290, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpNetOperation)) {
		MTP_ERR(MTP_TAG_NET, 298, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	net = (MtpNetOperation *)context->data;
	if (net == NULL) {
		MTP_ERR(MTP_TAG_NET, 305, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(&size, sizeof(size), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET, 313, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	if (context->sendRecv != NULL && net->type == MTP_NET_OPERATION_RECV) {
		ret = mtpContextPassRecvData(context, size, &code, 0);
		if (ret != 0) {
			MTP_ERR(MTP_TAG_NET, 321, "Failed for context %u\n", context->id);
			goto error;
		}
		net->recvData = NULL;
		net->recvSize = 0;
	} else {
		if (net->recvData != NULL)
			mtpUtilFree(net->recvData);
		net->recvData = mtpUtilMalloc(size);
		net->recvSize = size;
		ret = mtpCoreRecvData(net->recvData, size, NULL);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_NET, 333, "mtpCoreRecvData64 failed: 0x%x\n", ret);
			goto error;
		}
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

int mtpContextRpcNetOperationAbort(MtpContext *context, int operation, int arg, int id)
{
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_REQUEST_RPC_NET_OPERATION_ABORT;
	event.param[2] = id | operation << 16;
	event.numParams = 3;
	event.param[1] = arg;
	context->opcode = MTP_OPERATION_REQUEST_RPC_NET_OPERATION_ABORT;
	ret = mtpCoreSendEvent(&event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_NET, 374,
			"mtpCoreSendEventWithoutCheckConnection failed: 0x%x\n", ret);
		return ret;
	}
	return 0;
}

/* the request's end: the host's answer into the caller's buffer, and its size */
static void _mtpRpcNetOperationResult(MtpContext *context, void *result)
{
	MtpNetOperationResult *out = (MtpNetOperationResult *)result;
	MtpNetOperation *net;
	SceSize *outSize;
	void *data;
	SceSize size;

	if (out == NULL || context->dataSize != sizeof(MtpNetOperation))
		return;
	net = (MtpNetOperation *)context->data;
	if (net == NULL)
		return;
	data = net->recvData;
	outSize = out->size;
	if (data == NULL || net->recvSize == 0) {
		*outSize = 0;
		return;
	}
	*outSize = net->recvSize;
	size = MTP_NET_OPERATION_RESULT_MAX;
	if (net->recvSize <= MTP_NET_OPERATION_RESULT_MAX)
		size = net->recvSize;
	mtpUtilMemcpy(out->buffer, data, size);
	mtpUtilFree(net->recvData);
	net->recvData = NULL;
	net->recvSize = 0;
}

} /* extern "C" */
