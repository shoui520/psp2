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
