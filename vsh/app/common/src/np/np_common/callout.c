/*
 * SceNpCommon: callout threads. A thread runs each timer of its list when its time comes; the
 * list is sorted by time and guarded by the context's mutex, and the condition wakes the thread
 * when a timer is added or the thread is to stop. The module keeps one such thread of its own,
 * "SceNetCallout".
 */

#include "np_common_internal.h"

static int npCalloutThread(SceSize argSize, void *argBlock);

static NpCalloutContext s_netCallout;

int SceNpCommon_9AEF645B(NpCalloutContext *ctx, const char *name, SceSize stackSize,
			 int priority, int cpuAffinityMask)
{
	int ret;

	if (ctx->initialized > 0)
		return NP_COMMON_ERROR_ALREADY_INITIALIZED;
	ctx->stop = 0;
	ctx->list = NULL;
	ret = sceKernelCreateMutex(name, SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0)
		return ret;
	ctx->mutex = ret;
	ret = sceKernelCreateCond(name, 0, ctx->mutex, NULL);
	if (ret >= 0) {
		ctx->cond = ret;
		ret = sceKernelCreateThread(name, npCalloutThread, priority, stackSize, 0,
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

int SceNpCommon_48314C6E(NpCalloutContext *ctx)
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

/* queues the timer to run func(arg) after delay microseconds */
int SceNpCommon_40724BEA(NpCalloutContext *ctx, NpCallout *callout, SceUInt64 delay,
			 void (*func)(void *arg), void *arg)
{
	NpCallout *c, *prev;
	SceUInt64 now;

	if (ctx->initialized == 0)
		return NP_COMMON_ERROR_NOT_INITIALIZED;
	sceClibMemset(callout, 0, sizeof(*callout));
	now = sceKernelGetSystemTimeWide();
	callout->func = func;
	callout->arg = arg;
	callout->time = now + delay;
	sceKernelLockMutex(ctx->mutex, 1, NULL);
	for (c = ctx->list; c != NULL; c = c->next) {
		if (callout == c) {
			sceKernelUnlockMutex(ctx->mutex, 1);
			return NP_COMMON_ERROR_ALREADY_QUEUED;
		}
	}
	prev = NULL;
	for (c = ctx->list; c != NULL; c = c->next) {
		if (callout->time <= c->time)
			break;
		prev = c;
	}
	callout->next = c;
	if (prev != NULL)
		prev->next = callout;
	else
		ctx->list = callout;
	sceKernelSignalCond(ctx->cond);
	sceKernelUnlockMutex(ctx->mutex, 1);
	return 0;
}

int SceNpCommon_BDCD318E(NpCalloutContext *ctx, NpCallout *callout, int *removed)
{
	NpCallout *c, *prev;
	int found;

	if (ctx->initialized == 0)
		return NP_COMMON_ERROR_NOT_INITIALIZED;
	sceKernelLockMutex(ctx->mutex, 1, NULL);
	prev = NULL;
	for (c = ctx->list; c != NULL; c = c->next) {
		if (callout == c)
			break;
		prev = c;
	}
	if (c == NULL) {
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

int SceNpCommon_A7EC5CDB(SceSize stackSize, int priority)
{
	return SceNpCommon_9AEF645B(&s_netCallout, "SceNetCallout", stackSize, priority, 0);
}

int SceNpCommon_E2743C07(void)
{
	return SceNpCommon_48314C6E(&s_netCallout);
}

NpCalloutContext *SceNpCommon_659ED07E(void)
{
	if (s_netCallout.initialized == 0)
		return NULL;
	return &s_netCallout;
}

int SceNpCommon_540B307E(NpCallout *callout, SceUInt64 delay, void (*func)(void *arg),
			 void *arg)
{
	return SceNpCommon_40724BEA(&s_netCallout, callout, delay, func, arg);
}

int SceNpCommon_38B88BA3(NpCallout *callout, int *removed)
{
	return SceNpCommon_BDCD318E(&s_netCallout, callout, removed);
}

static int npCalloutThread(SceSize argSize, void *argBlock)
{
	NpCalloutContext *ctx;
	NpCallout *c;
	SceUInt64 now;
	SceUInt32 wait, timeout;

	sceClibMemcpy_safe(&ctx, argBlock, sizeof(ctx));
	sceKernelLockMutex(ctx->mutex, 1, NULL);
	do {
		for (;;) {
			now = sceKernelGetSystemTimeWide();
			c = ctx->list;
			if (c == NULL || now < c->time)
				break;
			ctx->list = c->next;
			sceKernelUnlockMutex(ctx->mutex, 1);
			c->func(c->arg);
			sceKernelLockMutex(ctx->mutex, 1, NULL);
		}
		wait = 0;
		if (c != NULL)
			wait = c->time - now;
		timeout = wait;
		if (ctx->stop > 0)
			break;
		if (wait == 0)
			sceKernelWaitCond(ctx->cond, NULL);
		else
			sceKernelWaitCond(ctx->cond, &timeout);
	} while (ctx->stop <= 0);
	sceKernelUnlockMutex(ctx->mutex, 1);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}
