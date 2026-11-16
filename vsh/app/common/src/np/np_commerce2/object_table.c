/*
 * SceNpCommerce2: the list of contexts. A context gets a 16-bit ID, the next free one after
 * the last given; the library's context may exist once, the shell's up to six times.
 */

#include "np_commerce2_internal.h"

NpCommerce2Context *g_contextList = NULL;	/* 0x81026098 */
SceUInt32 g_lastContextId = 0;			/* 0x8102609C */

/* puts a context in the list under a new ID */
int npCommerce2AddContext(SceKernelLwMutexWork *lock, NpCommerce2Context *ctx, SceUInt32 *ctxId, int kind)
{
	SceUInt32 count = 0;
	SceUInt32 limit;
	SceUInt32 id;
	NpCommerce2Context *head;
	NpCommerce2Context *it;
	int locked;
	int ret;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	head = g_contextList;
	for (it = g_contextList; it != NULL; it = it->next) {
		if (kind == it->kind)
			count++;
	}
	limit = 1;
	if (kind == 1)
		limit = 6;
	if (limit > count) {
		id = 1;
		if (g_lastContextId != 0xFFFF)
			id = g_lastContextId + 1;
		if (id != g_lastContextId) {
			do {
				SceUInt32 next;

				for (it = g_contextList; it != NULL; it = it->next) {
					if (it->id == id)
						break;
				}
				if (it == NULL)
					break;
				next = 1;
				if (id != 0xFFFF)
					next = id + 1;
				id = next;
			} while (id != g_lastContextId);
		}
		if (id == g_lastContextId) {
			ret = SCE_NP_COMMERCE2_ERROR_CTXID_NOT_AVAILABLE;
		} else {
			ctx->kind = kind;
			ctx->id = id;
			*ctxId = id;
			ctx->next = head;
			g_lastContextId = id;
			ret = 0;
			g_contextList = ctx;
		}
	} else {
		ret = SCE_NP_COMMERCE2_ERROR_CTX_MAX;
	}
	if (locked == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* finds a context and takes a reference on it */
int npCommerce2GetContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId, NpCommerce2Context **ctx, int notRemoved)
{
	NpCommerce2Context *found = NULL;
	NpCommerce2Context *it;
	int i;
	int ret;
	int locked;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	it = g_contextList;
	for (i = 0; it != NULL; it = it->next) {
		if (it->id == ctxId)
			break;
	}
	if (it != NULL)
		found = it;
	else
		i = SCE_NP_COMMERCE2_ERROR_CTX_NOT_FOUND;
	ret = i;
	if (i >= 0) {
		if (notRemoved != 0 && (found->flags & NP_COMMERCE2_CONTEXT_FLAG_REMOVED) != 0) {
			ret = SCE_NP_COMMERCE2_ERROR_CTX_NOT_FOUND;
		} else {
			found->refCount++;
			*ctx = found;
		}
	}
	if (locked == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* drops a reference; the last one of a removed context frees it */
int npCommerce2ReleaseContext(SceKernelLwMutexWork *lock, NpCommerce2Context *ctx)
{
	int locked;
	int ret;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	ctx->refCount--;
	ret = ctx->flags & NP_COMMERCE2_CONTEXT_FLAG_REMOVED;
	if (ret != 0 && ctx->refCount == 0) {
		ret = (int)ctx->requests;
		if (ctx->requests == NULL) {
			if (ctx->httpTemplate != 0)
				npCommerce2DeleteHttpTemplate(ctx->httpTemplate, ctx->httpConnection);
			ctx->httpTemplate = 0;
			npCommerce2Free(ctx);
		}
	}
	if (locked == 0)
		return sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* takes a context out of the list; it is freed with its last reference */
int npCommerce2UnlinkContext(SceKernelLwMutexWork *lock, SceUInt32 ctxId)
{
	NpCommerce2Context *prev = NULL;
	NpCommerce2Context *it;
	int ret = 0;
	int locked;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	it = g_contextList;
	if (g_contextList != NULL) {
		for (;;) {
			NpCommerce2Context *next = it->next;

			if (it->id == ctxId) {
				if (prev != NULL)
					prev->next = next;
				else
					g_contextList = it->next;
				it->next = NULL;
				break;
			}
			prev = it;
			it = it->next;
			if (next == NULL)
				break;
		}
	}
	if (it != NULL) {
		it->flags |= NP_COMMERCE2_CONTEXT_FLAG_REMOVED;
		if ((it->flags & NP_COMMERCE2_CONTEXT_FLAG_REMOVED) != 0 && it->refCount == 0 &&
		    it->requests == NULL) {
			if (it->httpTemplate != 0)
				npCommerce2DeleteHttpTemplate(it->httpTemplate, it->httpConnection);
			it->httpTemplate = 0;
			npCommerce2Free(it);
		}
	} else {
		ret = SCE_NP_COMMERCE2_ERROR_CTX_NOT_FOUND;
	}
	if (locked == 0)
		sceKernelUnlockLwMutex(lock, 1);
	return ret;
}

/* removes every context; the requests of each go first */
int npCommerce2DestroyAllContexts(SceKernelLwMutexWork *lock)
{
	SceUInt32 ids[6];
	NpCommerce2Context *it;
	SceUInt32 n = 0;
	int locked;

	locked = sceKernelLockLwMutex(lock, 1, NULL);
	it = g_contextList;
	while (it != NULL && n < 6) {
		ids[n] = it->id;
		it = it->next;
		n++;
	}
	if (locked == 0)
		sceKernelUnlockLwMutex(lock, 1);
	if (n != 0) {
		const SceUInt32 *p = ids;

		do {
			SceUInt32 id = *p++;

			npCommerce2RemoveAllRequests(lock, id);
			npCommerce2UnlinkContext(lock, id);
			n--;
		} while (n != 0);
	}
	return 0;
}

/* the HTTP template of a context's requests */
int npCommerce2CreateHttpTemplate(int *httpTemplate, int *httpConnection)
{
	int tmpl;
	int ret;

	tmpl = sceHttpCreateTemplate("commerce2/1.0", SCE_HTTP_VERSION_1_1, 0);
	ret = tmpl;
	if (tmpl >= 0) {
		ret = sceHttpSetCookieEnabled(tmpl, 0);
		if (ret < 0) {
			if (tmpl != 0)
				sceHttpDeleteTemplate(tmpl);
		} else {
			sceHttpsEnableOptionPrivate(tmpl, 357);
			sceHttpsDisableOptionPrivate(tmpl, 24);
			*httpConnection = 0;
			*httpTemplate = tmpl;
		}
	}
	return ret;
}

__attribute__((noinline)) int npCommerce2DeleteHttpTemplate(int httpTemplate, int httpConnection)
{
	if (httpConnection != 0)
		sceHttpDeleteConnection(httpConnection);
	if (httpTemplate != 0)
		sceHttpDeleteTemplate(httpTemplate);
	return 0;
}
