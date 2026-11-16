/*
 * SceNpCommerce2: the requests of a context, a list behind the context's reference.
 */

#include "np_commerce2_internal.h"

/* finds a request of a context and takes a reference on it */
int npCommerce2GetRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId, NpCommerce2Request **req)
{
	NpCommerce2Context *ctx = NULL;
	NpCommerce2Request *found = NULL;
	NpCommerce2Request *it;
	int ret;
	int locked;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	ret = npCommerce2GetContext(lock, reqId >> 16, &ctx, 0);
	if (ret >= 0) {
		for (it = ctx->requests; it != NULL; it = it->next) {
			if ((SceUInt16)it->id == (SceUInt16)reqId)
				break;
		}
		if (it != NULL)
			found = it;
		else
			ret = SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND;
	}
	if (ctx != NULL)
		npCommerce2ReleaseContext(lock, ctx);
	if (ret >= 0) {
		found->refCount++;
		*req = found;
	}
	if (locked == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* drops a reference; the last one of a removed request ends it */
void npCommerce2ReleaseRequest(SceKernelLwMutexWork *lock, NpCommerce2Request *req)
{
	int locked;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	req->refCount--;
	if (req->refCount == 0 && (req->flags & NP_COMMERCE2_REQUEST_FLAG_REMOVED) != 0) {
		if (req->httpRequest != 0)
			npCommerce2DeleteHttpRequest(req->httpRequest);
		req->httpRequest = 0;
		npCommerce2DeleteShellSlot(req->shellSlot);
		npCommerce2Free(req);
	}
	if (locked == 0)
		sceKernelUnlockLwMutex(lock, 1);
}

/* takes a request out of its context's list and ends it, unless someone still holds it */
int npCommerce2RemoveRequest(SceKernelLwMutexWork *lock, SceUInt32 reqId)
{
	NpCommerce2Context *ctx = NULL;
	NpCommerce2Request *req;
	NpCommerce2Request *prev;
	int ret;
	int locked;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	ret = npCommerce2GetContext(lock, reqId >> 16, &ctx, 0);
	if (ret >= 0) {
		NpCommerce2Context *c = ctx;

		req = c->requests;
		if (req != NULL) {
			prev = NULL;
			for (;;) {
				NpCommerce2Request *next = req->next;

				if (req->id == reqId) {
					if (prev != NULL)
						prev->next = next;
					else
						ctx->requests = next;
					req->next = NULL;
					c = ctx;
					break;
				}
				prev = req;
				req = req->next;
				if (next == NULL)
					goto notFound;
			}
		}
notFound:
		npCommerce2ReleaseContext(lock, c);
		if (req != NULL) {
			SceUInt32 flags = req->flags;

			if (req->httpRequest != 0) {
				npCommerce2AbortHttpRequest(req->httpRequest);
				npCommerce2AbortShellSlot(req->shellSlot);
				flags = req->flags | NP_COMMERCE2_REQUEST_FLAG_ABORTED;
			}
			flags |= NP_COMMERCE2_REQUEST_FLAG_REMOVED;
			req->flags = flags;
			if (req->refCount == 0 && (flags & NP_COMMERCE2_REQUEST_FLAG_REMOVED) != 0) {
				if (req->httpRequest != 0)
					npCommerce2DeleteHttpRequest(req->httpRequest);
				req->httpRequest = 0;
				npCommerce2DeleteShellSlot(req->shellSlot);
				npCommerce2Free(req);
			}
			ret = 0;
		} else {
			ret = SCE_NP_COMMERCE2_ERROR_REQ_NOT_FOUND;
		}
	}
	if (locked == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* removes every request of a context (at most eight) */
int npCommerce2RemoveAllRequests(SceKernelLwMutexWork *lock, SceUInt32 ctxId)
{
	NpCommerce2Context *ctx = NULL;
	SceUInt32 ids[8];
	NpCommerce2Request *it;
	SceUInt32 n;
	SceUInt32 i;
	int ret;
	int locked;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	ret = npCommerce2GetContext(lock, ctxId, &ctx, 0);
	if (ret >= 0) {
		n = 0;
		for (it = ctx->requests; it != NULL; it = it->next) {
			if (n >= 8)
				break;
			ids[n] = it->id;
			n++;
		}
		npCommerce2ReleaseContext(lock, ctx);
		if (locked == 0)
			sceKernelUnlockLwMutex(lock, 1);
		for (i = 0; i < n; i++)
			npCommerce2RemoveRequest(lock, ids[i]);
	} else if (locked == 0) {
		sceKernelUnlockLwMutex(lock, 1);
	}
	return ret;
}
