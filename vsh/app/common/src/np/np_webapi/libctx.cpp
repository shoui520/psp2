/*
 * SceNpWebApi: the library context's small operations (its lock, its reference count and its
 * flags) and the lock of the list of contexts. The names are ours.
 */

#include "np_webapi_internal.h"

NpWebApiMutex g_globalLock;		/* 0x81010048 */
NpWebApiLibContext *g_libContexts;	/* 0x81010014 */

int npWebApiLibContextUnlock(NpWebApiLibContext *ctx)
{
	return SceNpCommonPs4_4A1C1C3C(&ctx->lock);
}

int npWebApiLibContextLock(NpWebApiLibContext *ctx)
{
	return SceNpCommonPs4_F5840D6E(&ctx->lock);
}

int npWebApiLibContextRelease(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	ctx->refCount--;
	return SceNpCommonPs4_4A1C1C3C(lock);
}

int npWebApiLibContextIsDeleting(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;
	int deleting;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	deleting = ctx->deleting;
	SceNpCommonPs4_4A1C1C3C(lock);
	return deleting;
}

/* the context of an ID, with a reference taken; none when it is not there or being deleted */
NpWebApiLibContext *npWebApiFindLibContext(int libCtxId, unsigned char allowDeleting)
{
	NpWebApiLibContext *ctx;
	int deleting;

	npWebApiGlobalLock();
	ctx = g_libContexts;
	if (ctx != NULL) {
		for (;;) {
			if (npWebApiGetSdkVersion() >= NP_WEBAPI_SDK_3_30) {
				if (ctx->id == libCtxId) {
					if (allowDeleting == 0) {
						SceNpCommonPs4_F5840D6E(&ctx->lock);
						deleting = ctx->deleting;
						SceNpCommonPs4_4A1C1C3C(&ctx->lock);
						if (deleting != 0) {
							npWebApiGlobalUnlock();
							return NULL;
						}
					}
					goto found;
				}
			} else if (ctx->id == libCtxId) {
				goto found;
			}
			ctx = ctx->next;
			if (ctx == NULL)
				break;
		}
	}
	npWebApiGlobalUnlock();
	return ctx;
found:
	SceNpCommonPs4_F5840D6E(&ctx->lock);
	ctx->refCount++;
	SceNpCommonPs4_4A1C1C3C(&ctx->lock);
	npWebApiGlobalUnlock();
	return ctx;
}

/* the user contexts of a library context are a list, the newest first */
int npWebApiLibContextAddUserContext(NpWebApiLibContext *ctx, NpWebApiUserContext *userCtx)
{
	NpWebApiMutex *lock;
	int count;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	if (ctx->userContextHead != NULL)
		npWebApiUserContextSetNext(userCtx, ctx->userContextHead);
	count = ctx->userContextCount + 1;
	ctx->userContextHead = userCtx;
	ctx->userContextCount = count;
	return SceNpCommonPs4_4A1C1C3C(lock);
}

int npWebApiLibContextRemoveUserContext(NpWebApiLibContext *ctx, NpWebApiUserContext *userCtx)
{
	NpWebApiMutex *lock;
	NpWebApiUserContext *node;
	NpWebApiUserContext *prev;
	int count;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	node = ctx->userContextHead;
	for (prev = NULL; node != NULL; node = npWebApiUserContextNext(node)) {
		if (node == userCtx)
			break;
		prev = node;
	}
	if (node == NULL)
		return SceNpCommonPs4_4A1C1C3C(lock);
	if (prev != NULL) {
		npWebApiUserContextSetNext(prev, npWebApiUserContextNext(node));
		count = ctx->userContextCount;
	} else {
		NpWebApiUserContext *next;

		next = npWebApiUserContextNext(node);
		count = ctx->userContextCount;
		ctx->userContextHead = next;
	}
	ctx->userContextCount = count - 1;
	return SceNpCommonPs4_4A1C1C3C(lock);
}

NpWebApiUserContext *npWebApiLibContextFindUserContext(NpWebApiLibContext *ctx, int userCtxId)
{
	NpWebApiMutex *lock;
	NpWebApiUserContext *node;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	node = ctx->userContextHead;
	if (node == NULL)
		goto done;
	while (npWebApiGetSdkVersion() < NP_WEBAPI_SDK_3_30) {
		if (npWebApiUserContextGetId(node) == userCtxId)
			goto found;
next:
		node = npWebApiUserContextNext(node);
		if (node == NULL)
			goto done;
	}
	if (npWebApiUserContextGetId(node) != userCtxId)
		goto next;
	if (npWebApiUserContextFlag158(node) != 0) {
		SceNpCommonPs4_4A1C1C3C(lock);
		return NULL;
	}
found:
	npWebApiUserContextAddRef(node);
done:
	SceNpCommonPs4_4A1C1C3C(lock);
	return node;
}

NpWebApiUserContext *npWebApiLibContextFindUserContextByOnlineId(NpWebApiLibContext *ctx,
	const SceNpOnlineId *onlineId)
{
	NpWebApiMutex *lock;
	NpWebApiUserContext *node;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	node = ctx->userContextHead;
	if (node != NULL) {
		while (SceNpCommon_8C21F489(npWebApiUserContextOnlineId(node), onlineId) != 0) {
			node = npWebApiUserContextNext(node);
			if (node == NULL)
				goto done;
		}
		npWebApiUserContextAddRef(node);
	}
done:
	SceNpCommonPs4_4A1C1C3C(lock);
	return node;
}

void *npWebApiLibContextGetPool(NpWebApiLibContext *ctx)
{
	return ctx->heapBlock->poolObject;
}

int npWebApiLibContextDestroyPool(NpWebApiLibContext *ctx, void *block)
{
	return SceNpCommonPs4_38CC9AF5(ctx->heapBlock->poolObject, block);
}

void *npWebApiLibContextGetPool2(NpWebApiLibContext *ctx)
{
	return ctx->heapBlock->poolObject;
}

int npWebApiLibContextPoolAlloc(NpWebApiLibContext *ctx, int size)
{
	return SceNpCommonPs4_299ADFC3(ctx->heapBlock->poolObject, size);
}

int npWebApiLibContextGetId(NpWebApiLibContext *ctx)
{
	return ctx->id;
}

int npWebApiLibContextZero(void)
{
	return 0;
}

void *npWebApiLibContextGetPushEventObject(NpWebApiLibContext *ctx)
{
	return ctx->pushEventObject;
}

int npWebApiLibContextGetField116(NpWebApiLibContext *ctx)
{
	return ctx->reserved116;
}

int npWebApiGlobalLockCreate(void)
{
	return SceNpCommonPs4_4858EFB6(&g_globalLock, "SceNpWebApiGlobalLock", 1);
}

int npWebApiGlobalLock(void)
{
	return SceNpCommonPs4_F5840D6E(&g_globalLock);
}

int npWebApiGlobalUnlock(void)
{
	return SceNpCommonPs4_4A1C1C3C(&g_globalLock);
}

int npWebApiLibContextIsIdle(NpWebApiLibContext *ctx)
{
	return ctx->state == 0;
}

int npWebApiLibContextIsStopped(NpWebApiLibContext *ctx)
{
	return ctx->state == 2;
}

int npWebApiLibContextHasOtherUsers(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;
	int others;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	others = ctx->refCount > 1;
	SceNpCommonPs4_4A1C1C3C(lock);
	return others;
}

int npWebApiLibContextGetAborted(NpWebApiLibContext *ctx, unsigned char *aborted)
{
	NpWebApiMutex *lock;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	*aborted = ctx->aborted;
	return SceNpCommonPs4_4A1C1C3C(lock);
}

int npWebApiLibContextSetAborted(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	ctx->aborted = 1;
	return SceNpCommonPs4_4A1C1C3C(lock);
}

int npWebApiLibContextClearAborted(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	ctx->aborted = 0;
	return SceNpCommonPs4_4A1C1C3C(lock);
}

int npWebApiLibContextSetDeleting(NpWebApiLibContext *ctx)
{
	NpWebApiMutex *lock;

	lock = &ctx->lock;
	SceNpCommonPs4_F5840D6E(lock);
	ctx->deleting = 1;
	return SceNpCommonPs4_4A1C1C3C(lock);
}

int npWebApiLibContextIsStarted(void *unused, NpWebApiLibContext *ctx)
{
	return ctx->state & 1;
}

int npWebApiLibContextSetStarted(void *unused, NpWebApiLibContext *ctx)
{
	ctx->state |= 1;
	return ctx->state;
}

NpWebApiLibContext *npWebApiLibContextSetFlag121(NpWebApiLibContext *ctx, char value)
{
	ctx->flag121 = value;
	return ctx;
}

int npWebApiLibContextGetFlag121(NpWebApiLibContext *ctx)
{
	return ctx->flag121;
}
