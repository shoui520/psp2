/*
 * SceNetCallout timer thread used by SceNetAdhocMatching (0x81002d60..0x81003070),
 * matching the per-context callout core in vsh/app/common/src/np/np_common/callout.c.
 */
#include "matching_internal.h"

static int matchingCalloutThread(SceSize argSize, void *argBlock);

int matchingCalloutInit(AdhocMatchingCalloutContext *ctx, const char *name, SceSize stackSize,
			int priority, int cpuAffinityMask)
{
	int ret;

	if (ctx->initialized > 0)
		return ADHOC_CALLOUT_ERROR_ALREADY_INITIALIZED;
	ctx->stop = 0;
	ctx->list = NULL;
	ret = sceKernelCreateMutex(name, SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0)
		return ret;
	ctx->mutex = ret;
	ret = sceKernelCreateCond(name, 0, ctx->mutex, NULL);
	if (ret >= 0) {
		ctx->cond = ret;
		ret = sceKernelCreateThread(name, matchingCalloutThread, priority, stackSize, 0,
					    cpuAffinityMask, NULL);
		if (ret >= 0) {
			ctx->thread = ret;
			ret = sceKernelStartThread(ctx->thread, sizeof(ctx), &ctx);
			if (ret >= 0) {
				ctx->initialized = 1;
				return 0;
			}
			sceKernelDeleteThread(ctx->thread);
			ctx->thread = 0;
		}
		sceKernelDeleteCond(ctx->cond);
		ctx->cond = 0;
	}
	sceKernelDeleteMutex(ctx->mutex);
	ctx->mutex = 0;
	return ret;
}

int matchingCalloutTerm(AdhocMatchingCalloutContext *ctx)
{
	int ret = ctx->initialized;

	if (ret != 0) {
		ctx->stop = 1;
		sceKernelSignalCond(ctx->cond);
		sceKernelWaitThreadEnd(ctx->thread, NULL, NULL);
		sceKernelDeleteThread(ctx->thread);
		sceKernelDeleteCond(ctx->cond);
		ctx->cond = 0;
		ret = sceKernelDeleteMutex(ctx->mutex);
		ctx->mutex = 0;
		ctx->initialized = 0;
	}
	return ret;
}

int matchingCalloutAdd(AdhocMatchingCalloutContext *ctx, AdhocMatchingCallout *callout,
		       SceUInt64 delay, void (*func)(void *arg), void *arg)
{
	AdhocMatchingCallout *cur;
	AdhocMatchingCallout *prev;
	SceUInt64 now;
	SceUID mutex;

	if (ctx->initialized == 0)
		return ADHOC_CALLOUT_ERROR_NOT_INITIALIZED;
	sceClibMemset(callout, 0, sizeof(*callout));
	now = (SceUInt64)sceKernelGetSystemTimeWide();
	mutex = ctx->mutex;
	callout->func = func;
	callout->arg = arg;
	callout->time = now + delay;
	sceKernelLockMutex(mutex, 1, NULL);
	for (cur = ctx->list; cur != NULL; cur = cur->next) {
		if (callout == cur) {
			sceKernelUnlockMutex(ctx->mutex, 1);
			return ADHOC_CALLOUT_ERROR_ALREADY_QUEUED;
		}
	}
	prev = NULL;
	for (cur = ctx->list; cur != NULL; cur = cur->next) {
		if (callout->time <= cur->time)
			break;
		prev = cur;
	}
	callout->next = cur;
	if (prev != NULL)
		prev->next = callout;
	else
		ctx->list = callout;
	sceKernelSignalCond(ctx->cond);
	sceKernelUnlockMutex(ctx->mutex, 1);
	return 0;
}

int matchingCalloutRemove(AdhocMatchingCalloutContext *ctx, AdhocMatchingCallout *callout,
			  int *removed)
{
	AdhocMatchingCallout *cur;
	AdhocMatchingCallout *prev;
	int found;

	if (ctx->initialized == 0)
		return ADHOC_CALLOUT_ERROR_NOT_INITIALIZED;
	sceKernelLockMutex(ctx->mutex, 1, NULL);
	prev = NULL;
	for (cur = ctx->list; cur != NULL; cur = cur->next) {
		if (callout == cur)
			break;
		prev = cur;
	}
	if (cur == NULL) {
		found = 0;
	} else {
		if (prev != NULL)
			prev->next = callout->next;
		else
			ctx->list = callout->next;
		found = 1;
	}
	if (removed != NULL)
		*removed = found;
	sceKernelUnlockMutex(ctx->mutex, 1);
	return 0;
}

static int matchingCalloutThread(SceSize argSize, void *argBlock)
{
	AdhocMatchingCalloutContext *ctx;
	AdhocMatchingCallout *cur;
	SceUInt64 now;
	SceUInt32 wait;
	SceUInt32 timeout;

	(void)argSize;
	sceClibMemcpy_safe(&ctx, argBlock, sizeof(ctx));
	sceKernelLockMutex(ctx->mutex, 1, NULL);
	for (;;) {
		now = (SceUInt64)sceKernelGetSystemTimeWide();
		cur = ctx->list;
		if (cur == NULL || now < cur->time) {
			wait = 0;
			if (cur != NULL)
				wait = (SceUInt32)(cur->time - now);
			timeout = wait;
			if (ctx->stop > 0)
				break;
			if (wait == 0)
				sceKernelWaitCond(ctx->cond, NULL);
			else
				sceKernelWaitCond(ctx->cond, &timeout);
			if (ctx->stop > 0)
				break;
		} else {
			ctx->list = cur->next;
			sceKernelUnlockMutex(ctx->mutex, 1);
			cur->func(cur->arg);
			sceKernelLockMutex(ctx->mutex, 1, NULL);
		}
	}
	sceKernelUnlockMutex(ctx->mutex, 1);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}
