/*
 * SceNpCommerce2: the results of the requests. A request's result is its HTTP response (read here,
 * with the library's header in front of it) or, for the requests the shell answers, what the shell gives.
 */

#include "np_commerce2_internal.h"

/* the library's header of a result: the text before the response's data, which the library puts in front */
typedef struct NpCommerce2ResponseHead {
	SceUInt32 version;
	SceUInt32 reserved1[1];
	SceUInt32 length;
	SceUInt32 status;
	SceUInt32 nativeBuildVersion;
	char currencyCode[4];
	char countryCode[4];
	char decimalPoint[4];
	char thousandsSeparator[4];
	SceUInt8 reserved2[0x64 - 36];
} NpCommerce2ResponseHead;

/* makes the header of a result in the buffer and reads the four bytes the response starts with; gives the
 * length of both (the status byte of the response in *status, the length of the response's own bytes in
 * *headLength) */
int npCommerce2ReadResponseHead(NpCommerce2Context *ctx, int httpRequest, int type, char *status, char *buf,
				SceUInt32 bufSize, SceSize *headLength)
{
	NpCommerce2ResponseHead head;
	char first[4];
	int ret;

	if (bufSize < 0x68)
		return SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
	sceClibMemset(&head, 0, 0x64);
	head.version = ctx->version;
	head.nativeBuildVersion = ctx->nativeBuildVersion;
	sceClibStrncpy(head.currencyCode, ctx->currencyCode, 4);
	head.currencyCode[3] = '\0';
	sceClibStrncpy(head.countryCode, ctx->countryCode, 4);
	head.countryCode[3] = '\0';
	sceClibStrncpy(head.decimalPoint, ".", 4);
	head.decimalPoint[3] = '\0';
	sceClibStrncpy(head.thousandsSeparator, ",", 4);
	head.thousandsSeparator[3] = '\0';
	sceClibMemcpy_safe(buf, &head, 0x64);
	ret = npCommerce2ReadResponseData(httpRequest, first, 4);
	if (ret < 0)
		return ret;
	if (ret < 4)
		return SCE_NP_COMMERCE2_ERROR_COULD_NOT_RECV_WHOLE_RESPONSE_DATA;
	/* the four bytes go through the checksum's loops, which keep nothing */
	{
		SceSize done = 0;

		do {
			SceSize chunk = 0x80;
			SceSize end;
			SceSize count;

			if (4 - done <= 0x80)
				chunk = 4 - done;
			end = done + chunk;
			if (done < end) {
				count = end - done;
				do {
					count--;
					done++;
				} while (count != 0);
			}
		} while (done < 4);
	}
	*status = first[1];
	*(SceUInt16 *)(first + 2) = 0;
	sceClibMemcpy_safe(buf + 0x64, first, 4);
	*headLength = 4;
	return 0x68;
}

/* the result of a request read from its HTTP response */
int npCommerce2GetResultData(SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	NpCommerce2Context *ctx = NULL;
	NpCommerce2Request *req = NULL;
	SceKernelLwMutexWork *lock = &g_tableLock;
	char *data = buf;
	char status;
	char *cursor;
	int tooLong;
	SceSize headLength = 0;
	int hasRecord;
	int ret;
	int length;

	if (data == NULL || bufLen == 0 || fillSize == NULL) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		goto done;
	}
	ret = npCommerce2GetContext(lock, reqId >> 16, &ctx, 1);
	if (ret < 0)
		goto done;
	ret = npCommerce2GetRequest(lock, reqId, &req);
	if (ret < 0)
		goto done;
	if (req->type > 2 && req->type <= 4 && ctx->kind != 1 && bufLen < 0x40000) {
		ret = SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUF_SIZE;
		goto done;
	}
	ret = npCommerce2CheckRequest(ctx, req);
	if (ret < 0)
		goto done;
	ret = npCommerce2GetResponseStatus(req->httpRequest, (int *)&ctx->nativeBuildVersion);
	if (ret < 0)
		goto done;
	ret = npCommerce2ReadResponseHead(ctx, req->httpRequest, req->type, &status, data, bufLen, &headLength);
	if (ret < 0)
		goto done;
	cursor = data + ret;
	ret = npCommerce2ReadResponseBody(req->httpRequest, data, cursor, bufLen, &tooLong, headLength);
	if (ret < 0)
		goto done;
	length = ret;
	cursor += length;
	ret = npCommerce2ForwardCookie(ctx->httpTemplate, ctx->httpConnection, req->httpRequest, ctx->text, 256);
	if (ret < 0)
		goto done;
	if (tooLong == 1 && req->type != 3 && req->type != 7 && req->type != 5 && req->type != 53 &&
	    req->type != 54 && req->type != 55 && req->type != 76) {
		ret = SCE_NP_COMMERCE2_ERROR_RESPONSE_BUF_TOO_SMALL;
		goto done;
	}
	ret = npCommerce2SetHeaderStatus(data, bufLen, length, tooLong);
	if (ret < 0)
		goto done;
	*fillSize = cursor - data;
	if ((SceUInt8)status > 0) {
		ret = (SceUInt32)(SceUInt8)status | 0x80551000u;
		if (req->type == 64 || req->type == 23) {
			if (ret == -2141908991) {
				ret = -2141908949;
				goto done;
			}
		} else if (req->type == 38) {
			if (ret == -2141908991) {
				ret = -2141908927;
				goto done;
			}
		}
		npCommerce2ResponseHasRecord78(data, *fillSize, &hasRecord);
		if (hasRecord == 1)
			ret = -2141909101;
		if (ret == -2141908987)
			npCommerce2ShellOp02(ctx->shellSlot);
	} else {
		ret = 0;
	}
	if (ret == 0)
		npCommerce2StoreResponse(ctx, req, data, bufLen, *fillSize);
done:
	if (req != NULL) {
		if ((req->flags & NP_COMMERCE2_REQUEST_FLAG_ABORTED) != 0)
			ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
		npCommerce2ReleaseRequest(lock, req);
	}
	if (ctx != NULL)
		npCommerce2ReleaseContext(lock, ctx);
	return ret;
}

/* the result of a request the shell answers, with the shell's text and cookie for the context */
int npCommerce2GetResult(int type, SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	NpCommerce2Context *ctx = NULL;
	NpCommerce2Request *req = NULL;
	SceKernelLwMutexWork *lock = &g_tableLock;
	int result;
	int ret;

	if (buf == NULL || bufLen == 0 || fillSize == NULL) {
		ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		goto done;
	}
	ret = npCommerce2GetContext(lock, reqId >> 16, &ctx, 1);
	if (ret < 0)
		goto done;
	ret = npCommerce2GetRequest(lock, reqId, &req);
	if (ret < 0)
		goto done;
	if (type == 4) {
		if (ctx->mode == 8)
			type = 9;
		else if (ctx->mode == 7)
			type = 10;
	}
	ret = npCommerce2WaitShellSlot(req->shellSlot);
	if (ret < 0)
		goto done;
	ret = npCommerce2GetShellSlotResult(req->shellSlot, &result, &ctx->shellCounter, buf, bufLen, fillSize,
					    ctx->text, 256, &ctx->nativeBuildVersion);
	if (ret < 0)
		goto done;
	switch (type) {
	case 2: case 3: case 4: case 9: case 10:
		ctx->shellSlot = type;
		break;
	default:
		ctx->shellSlot = 0;
		ctx->shellCounter = 0;
		break;
	}
	if (type == 8) {
		ctx->text[0] = '\0';
	} else {
		ret = npCommerce2AddCookie(ctx->httpTemplate, ctx->httpConnection, ctx->text);
		if (ret < 0)
			goto done;
		npCommerce2StoreResponse(ctx, req, buf, bufLen, *fillSize);
	}
	if (result < 0)
		ret = result;
done:
	if (req != NULL) {
		if ((req->flags & NP_COMMERCE2_REQUEST_FLAG_ABORTED) != 0)
			ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
		npCommerce2ReleaseRequest(lock, req);
	}
	if (ctx != NULL)
		npCommerce2ReleaseContext(lock, ctx);
	return ret;
}

/* the result of a request: the shell's when the context talks to the shell, the HTTP response's otherwise */
int npCommerce2GetResultByContext(int type, SceUInt32 reqId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	NpCommerce2Context *ctx = NULL;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		if (ctx->shellSlot != 0)
			ret = npCommerce2GetResult(type, reqId, buf, bufLen, fillSize);
		else
			ret = npCommerce2GetResultData(reqId, buf, bufLen, fillSize);
	}
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}
