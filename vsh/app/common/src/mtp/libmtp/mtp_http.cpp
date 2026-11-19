/*
 * HTTP through the host ("HTTP:"): a caller's GET of a URL is a request the host takes from
 * the device (GetUrl), and whose data (SendData) or properties (SendProperty) it sends back.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <rtc.h>
#include "mtp_internal.h"

#define MTP_TAG_HTTP			"HTTP:"

#define MTP_HTTP_URL_MAX		1024
#define MTP_HTTP_CONTEXT_TIMEOUT	40000000
#define MTP_HTTP_PROPERTY_SIZE		0x2000

/* a request's data: the URL, and what the host reported for it (1064 bytes) */
typedef struct MtpHttpRequest {
	char url[MTP_HTTP_URL_MAX];
	SceUInt32 urlLength;
	SceUInt32 reserved;
	SceUInt64 offset;		/* where the data starts */
	SceUInt64 size;			/* the property: the data's size */
	SceDateTime mtime;		/* the property: when it was last modified */
} MtpHttpRequest;

/* what a GetProperty's caller receives */
typedef struct MtpHttpProperty {
	SceUInt64 *size;
	SceDateTime *mtime;
} MtpHttpProperty;

static void httpGetPropertyResult(MtpContext *context, void *result);

int mtpContextHttpGetData(MtpContext *context, const char *url, SceSize urlLength,
			  SceUInt64 offset)
{
	MtpEvent event;
	MtpHttpRequest *request;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	if (urlLength > MTP_HTTP_URL_MAX) {
		MTP_ERR(MTP_TAG_HTTP, 66, "size of url is too large: %u\n", urlLength);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	request = (MtpHttpRequest *)mtpUtilMalloc(sizeof(*request));
	if (request == NULL) {
		MTP_ERR(MTP_TAG_HTTP, 72, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(request, 0, sizeof(*request));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_HTTP_GET_DATA;
	event.numParams = 1;
	mtpUtilStrncpy(request->url, url, urlLength);
	request->urlLength = urlLength;
	request->offset = offset;
	context->data = request;
	context->dataSize = sizeof(*request);
	context->timeout = MTP_HTTP_CONTEXT_TIMEOUT;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 90, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(request);
		return ret;
	}
	return 0;
}

int mtpContextHttpGetProperty(MtpContext *context, const char *url, SceSize urlLength)
{
	MtpEvent event;
	MtpHttpRequest *request;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	if (urlLength > MTP_HTTP_URL_MAX) {
		MTP_ERR(MTP_TAG_HTTP, 108, "size of url is too large: %u\n", urlLength);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	request = (MtpHttpRequest *)mtpUtilMalloc(sizeof(*request));
	if (request == NULL) {
		MTP_ERR(MTP_TAG_HTTP, 114, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(request, 0, sizeof(*request));
	event.param[0] = context->id;
	event.code = MTP_OPERATION_HTTP_GET_PROPERTY;
	event.numParams = 1;
	mtpUtilStrncpy(request->url, url, urlLength);
	request->urlLength = urlLength;
	context->data = request;
	context->dataSize = sizeof(*request);
	context->onResult = httpGetPropertyResult;
	context->timeout = MTP_HTTP_CONTEXT_TIMEOUT;
	ret = mtpTaskQueuePush(context, &event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 132, "mtpTaskQueuePush failed: 0x%x\n", ret);
		mtpUtilFree(request);
		return ret;
	}
	return 0;
}

/* the host asks for the request: its offset, the URL's length, then the URL */
int mtpCmdHttpGetUrl(MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	MtpContext *context = NULL;
	MtpHttpRequest *request;
	SceUInt16 code = MTP_RESPONSE_GENERAL_ERROR;
	int send = 1;
	SceUInt64 total;
	int ret;

	if (operation->numParams == 0) {
		MTP_ERR(MTP_TAG_HTTP, 158, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 166, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpHttpRequest)) {
		MTP_ERR(MTP_TAG_HTTP, 173, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	request = (MtpHttpRequest *)context->data;
	if (request == NULL) {
		MTP_ERR(MTP_TAG_HTTP, 180, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	total = request->urlLength + sizeof(request->offset) + sizeof(request->urlLength);
	ret = mtpCoreSendData(&request->offset, sizeof(request->offset), total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 191, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	ret = mtpCoreSendData(&request->urlLength, sizeof(request->urlLength), total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 198, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	ret = mtpCoreSendData(request->url, request->urlLength, total);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 205, "sceMtpIfSendData failed: 0x%x\n", ret);
		send = 0;
		goto error;
	}
	mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, NULL, 0, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	return mtpDevDriverHandleErrorResponse(operation, response, ret, code, send);
}

/* the host sends the data: its size, then the data into the caller's buffers */
int mtpCmdHttpSendData(MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt32 code = MTP_RESPONSE_GENERAL_ERROR;
	MtpContext *context = NULL;
	SceUInt32 result;
	SceUInt64 size = 0;
	int ret;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_HTTP, 241, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 249, "GetContext %u failed: 0x%x\n", contextId, ret);
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpHttpRequest)) {
		MTP_ERR(MTP_TAG_HTTP, 256, "Invalid data size\n");
		code = MTP_RESPONSE_VENDOR_INVALID_CONTEXT;
		ret = -2;
		goto error;
	}
	ret = mtpCoreRecvData(&size, sizeof(size), NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 266, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto error;
	}
	if (context->sendRecv != NULL) {
		ret = mtpContextPassRecvData(context, size, &code, 1);
		if (ret != 0) {
			MTP_ERR(MTP_TAG_HTTP, 273, "Failed for context %u\n", context->id);
			goto error;
		}
	} else {
		MTP_ERR(MTP_TAG_HTTP, 277, "RecvData is not set\n");
		ret = -2;
		goto error;
	}
	mtpContextPutContext(context, 1);
	code = MTP_RESPONSE_OK;
	result = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &result, 1, operation, response);
	return 0;
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

/* the host sends the properties: the data's size and its modification date */
int mtpCmdHttpSendProperty(MtpOperation *operation, MtpOperation *response)
{
	SceUInt32 contextId = operation->param[0];
	SceUInt16 code = MTP_RESPONSE_GENERAL_ERROR;
	MtpContext *context = NULL;
	MtpHttpRequest *request;
	char dateString[20];
	SceUInt32 result;
	MtpString date;
	SceUInt8 *buf;
	int ret;

	if (operation->numParams < 1) {
		MTP_ERR(MTP_TAG_HTTP, 318, "wrong param\n");
		code = MTP_RESPONSE_INVALID_PARAMETER;
		ret = -2;
		goto error;
	}
	ret = mtpContextGet(contextId, 1, &context);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 326, "GetContext %u failed: 0x%x\n", contextId, ret);
		ret = -2;
		goto error;
	}
	if (context->dataSize != sizeof(MtpHttpRequest)) {
		MTP_ERR(MTP_TAG_HTTP, 332, "Invalid data size\n");
		ret = -2;
		goto error;
	}
	request = (MtpHttpRequest *)context->data;
	if (request == NULL) {
		MTP_ERR(MTP_TAG_HTTP, 338, "pContext->pData is NULL\n");
		ret = -2;
		goto error;
	}
	buf = (SceUInt8 *)mtpUtilMalloc(MTP_HTTP_PROPERTY_SIZE);
	if (buf == NULL) {
		MTP_ERR(MTP_TAG_HTTP, 349, "no memory\n");
		ret = -2;
		goto free;
	}
	ret = mtpCoreRecvData(buf, MTP_HTTP_PROPERTY_SIZE, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_HTTP, 356, "sceMtpIfRecvData failed: 0x%x\n", ret);
		goto free;
	}
	mtpUtilMemcpy(&request->size, buf, sizeof(request->size));
	date.numChars = buf[8];
	date.chars = buf + 9;
	if (date.numChars != 0 && mtpUtilUnpackMtpString(dateString, 18, &date) >= 0)
		mtpUtilStringToDateTime(dateString, &request->mtime);
	mtpUtilFree(buf);
	mtpContextPutContext(context, 1);
	result = 0;
	mtpUtilSetMtpResponse(MTP_RESPONSE_OK, &result, 1, operation, response);
	return 0;
free:
	if (buf != NULL)
		mtpUtilFree(buf);
error:
	if (context != NULL)
		mtpContextPutContext(context, 1);
	mtpUtilSetMtpResponse(code, NULL, 0, operation, response);
	return ret;
}

static void httpGetPropertyResult(MtpContext *context, void *result)
{
	MtpHttpRequest *request;
	MtpHttpProperty *property = (MtpHttpProperty *)result;

	if (context->dataSize != sizeof(MtpHttpRequest))
		return;
	request = (MtpHttpRequest *)context->data;
	if (property == NULL)
		return;
	/* both of the caller's pointers are read before the size is stored */
	SceUInt64 *size = property->size;
	SceDateTime *mtime = property->mtime;

	*size = request->size;
	mtpUtilMemcpy(mtime, &request->mtime, sizeof(request->mtime));
}
