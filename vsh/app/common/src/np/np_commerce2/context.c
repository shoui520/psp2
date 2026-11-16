/*
 * SceNpCommerce2: the table of contexts and requests, a linked list of objects behind one
 * lightweight mutex. An object lives while someone holds a reference to it.
 */

#include "np_commerce2_internal.h"

SceKernelLwMutexWork g_tableLock;		/* 0x81027080 */

/* Init's settings, and the lock of the object table */
int npCommerce2InitTable(const char *environment, int npCommonValue, int environmentType)
{
	int ret;

	ret = npCommerce2SetNpEnvironment(environment);
	if (ret >= 0) {
		ret = npCommerce2SetNpCommonValue(npCommonValue);
		if (ret >= 0) {
			ret = npCommerce2SetEnvironmentType(environmentType);
			if (ret >= 0) {
				sceClibMemset(&g_tableLock, 0, 0x20);
				ret = sceKernelCreateLwMutex(&g_tableLock, "SceNpCommerce2", 2, 0, NULL);
				if (ret >= 0)
					return 0;
			}
		}
	}
	return ret;
}

/* Term: every object is released, then the lock goes */
int npCommerce2TermTable(void)
{
	npCommerce2_8101D9AC(&g_tableLock);
	return sceKernelDeleteLwMutex(&g_tableLock);
}

/* makes a context: its HTTP template and its place in the table; kind 1 is the shell's */
int npCommerce2CreateContext(SceUInt32 version, SceUInt32 *ctxId, int kind)
{
	NpCommerce2Context *ctx;
	SceUInt32 mode;
	int ret;

	if (version > 2)
		return SCE_NP_COMMERCE2_ERROR_UNSUPPORTED_VERSION;
	if (ctxId == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ctx = npCommerce2Alloc(sizeof(*ctx));
	if (ctx == NULL) {
		ret = SCE_NP_COMMERCE2_ERROR_OUT_OF_MEMORY;
	} else {
		sceClibMemset(ctx, 0, sizeof(*ctx));
		mode = ctx->kind;
		ctx->version = version;
		if (mode == 1)
			mode = 1;
		else
			mode = 3;
		ctx->mode = mode;
		ret = npCommerce2CreateHttpTemplate(&ctx->httpTemplate, &ctx->httpConnection);
		if (ret >= 0) {
			ret = npCommerce2AddContext(&g_tableLock, ctx, ctxId, kind);
			if (ret >= 0)
				return ret;
		}
	}
	if (ctx != NULL) {
		if (ctx->httpTemplate != 0)
			npCommerce2DeleteHttpTemplate(ctx->httpTemplate, ctx->httpConnection);
		npCommerce2Free(ctx);
	}
	return ret;
}

int npCommerce2DestroyAllContextsLocked(void)
{
	return npCommerce2DestroyAllContexts(&g_tableLock);
}

int npCommerce2DestroyContext(SceUInt32 ctxId)
{
	NpCommerce2Context *ctx = NULL;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, ctxId, &ctx, 1);
	if (ret >= 0)
		ret = npCommerce2UnlinkContext(&g_tableLock, ctxId);
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}

int npCommerce2DestroyRequest(SceUInt32 reqId)
{
	NpCommerce2Context *ctx = NULL;
	NpCommerce2Request *req = NULL;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = npCommerce2GetRequest(&g_tableLock, reqId, &req);
		if (ret >= 0) {
			npCommerce2RemoveRequest(&g_tableLock, reqId);
			ret = 0;
		}
	}
	if (req != NULL)
		npCommerce2ReleaseRequest(&g_tableLock, req);
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}

int npCommerce2AbortRequest(SceUInt32 reqId)
{
	NpCommerce2Context *ctx = NULL;
	NpCommerce2Request *req = NULL;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, reqId >> 16, &ctx, 1);
	if (ret >= 0) {
		ret = npCommerce2GetRequest(&g_tableLock, reqId, &req);
		if (ret >= 0) {
			npCommerce2AbortHttpRequest(req->httpRequest);
			npCommerce2AbortShellSlot(req->shellSlot);
			ret = 0;
			req->flags |= NP_COMMERCE2_REQUEST_FLAG_ABORTED;
		}
	}
	if (req != NULL)
		npCommerce2ReleaseRequest(&g_tableLock, req);
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}

/* gives the context another mode */
int npCommerce2SetMode(SceUInt32 ctxId, SceUInt32 mode)
{
	NpCommerce2Context *ctx = NULL;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, ctxId, &ctx, 1);
	if (ret >= 0)
		ctx->mode = mode;
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}

int npCommerce2GetUserValue(SceUInt32 ctxId, SceUInt32 *value)
{
	NpCommerce2Context *ctx = NULL;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, ctxId, &ctx, 1);
	if (ret >= 0)
		*value = ctx->userValue;
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}

int npCommerce2GetSslShortfall(SceUInt32 ctxId, SceInt32 *shortfall)
{
	NpCommerce2Context *ctx = NULL;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, ctxId, &ctx, 1);
	if (ret >= 0)
		*shortfall = ctx->sslShortfall;
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}

int npCommerce2GetHttpShortfall(SceUInt32 ctxId, SceInt32 *shortfall)
{
	NpCommerce2Context *ctx = NULL;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, ctxId, &ctx, 1);
	if (ret >= 0)
		*shortfall = ctx->httpShortfall;
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}

/* hands the response of one context to the caller and gives the other its text and cookie */
int npCommerce2CopyResponse(SceUInt32 ctxId, SceUInt32 targetId, void *buf, SceSize bufLen, SceSize *fillSize)
{
	NpCommerce2Context *ctx = NULL;
	NpCommerce2Context *target = NULL;
	int httpTemplate;
	int httpConnection;
	int ret;

	ret = npCommerce2GetContext(&g_tableLock, ctxId, &ctx, 1);
	if (ret >= 0) {
		if (ctx->responseSize == 0) {
			ret = NP_COMMERCE2_ERROR_NO_RESPONSE;
		} else if (bufLen < ctx->responseSize) {
			ret = SCE_NP_COMMERCE2_ERROR_INSUFFICIENT_BUF_SIZE;
		} else {
			sceClibMemcpy_safe(buf, ctx->response, ctx->responseSize);
			*fillSize = ctx->responseSize;
			ret = npCommerce2GetContext(&g_tableLock, targetId, &target, 1);
			if (ret >= 0) {
				sceClibStrncpy(target->text, ctx->text, 0x100);
				target->text[255] = '\0';
				httpTemplate = target->httpTemplate;
				httpConnection = target->httpConnection;
				target->userValue = ctx->userValue;
				ret = npCommerce2AddCookie(httpTemplate, httpConnection, target->text);
			}
		}
	}
	if (target != NULL)
		npCommerce2ReleaseContext(&g_tableLock, target);
	if (ctx != NULL)
		npCommerce2ReleaseContext(&g_tableLock, ctx);
	return ret;
}
