/*
 * slpque.c: BSD-style sleep/wakeup hash queue for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

void (*g_adhocSleepCallback)(AdhocThreadInfo *ti) = NULL;
static void (*g_adhocAlertCallback)(AdhocSocket *sock, int alertFlags) = NULL;
static int g_adhocAlertCallbackSet = 0;
int g_adhocSleepCallbackSet = 0;
static int g_adhocSlpqueMutexInit = -1;

static SceKernelLwMutexWork g_adhocSlpqueMutex;
static AdhocSlpqueHead g_adhocSlpque[PSPNET_ADHOC_SLPQUE_BUCKETS];

static inline AdhocSlpqueHead *slpqueBucket(const void *wchan)
{
	return &g_adhocSlpque[((SceUIntPtr)wchan >> 8) & (PSPNET_ADHOC_SLPQUE_BUCKETS - 1)];
}

__attribute__((noinline))
int adhocSlpqueInit(void)
{
	int i, ret;

	for (i = 0; i < PSPNET_ADHOC_SLPQUE_BUCKETS; i++) {
		g_adhocSlpque[i].first = NULL;
		g_adhocSlpque[i].last = &g_adhocSlpque[i].first;
	}
	ret = sceKernelCreateLwMutex(&g_adhocSlpqueMutex, "SceNetSlpque",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO |
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
				     0, NULL);
	g_adhocSlpqueMutexInit = ret;
	if (ret >= 0)
		return 0;
	return ret;
}

__attribute__((noinline))
void adhocSlpqueTerm(void)
{
	if (g_adhocSlpqueMutexInit >= 0)
		sceKernelDeleteLwMutex(&g_adhocSlpqueMutex);
	g_adhocSlpqueMutexInit = -1;
}

__attribute__((noinline))
int adhocSleep(void *wchan, int timo)
{
	AdhocThreadInfo *ti;
	AdhocSlpqueHead *head;
	int unlocked = 0;
	int ret = 0;

	ti = adhocThreadFind(sceKernelGetThreadId());
	sceKernelLockLwMutex(&g_adhocSlpqueMutex, 1, NULL);
	if (g_adhocSleepCallbackSet > 0)
		g_adhocSleepCallback(ti);
	if (ti->slpFlags & PSPNET_ADHOC_SLP_ABORTED) {
		ti->slpFlags &= ~PSPNET_ADHOC_SLP_ABORTED;
		ret = SCE_NET_ERROR_EINTR;
	} else {
		ti->wchan = wchan;
		ti->slpNext = NULL;
		head = slpqueBucket(wchan);
		ti->slpPrev = head->last;
		*head->last = ti;
		head->last = &ti->slpNext;
		sceKernelUnlockLwMutex(&g_adhocSlpqueMutex, 1);
		if (timo != 0)
			adhocCalloutReset(&ti->timeout, timo, adhocSleepTimeout, ti);
		if (adhocSplX(1) >= 0)
			unlocked = 1;
		sceKernelWaitSema(ti->sema, 1, NULL);
		if (unlocked != 0)
			adhocSplNet();
		if (timo != 0)
			adhocCalloutStop(&ti->timeout);
		sceKernelLockLwMutex(&g_adhocSlpqueMutex, 1, NULL);
		if (ti->slpNext != NULL)
			ti->slpNext->slpPrev = ti->slpPrev;
		else
			head->last = ti->slpPrev;
		*ti->slpPrev = ti->slpNext;
		ti->slpNext = NULL;
		ti->slpPrev = NULL;
		if (ti->slpFlags & PSPNET_ADHOC_SLP_TIMEDOUT) {
			ret = SCE_NET_ERROR_EAGAIN;
			ti->slpFlags &= ~PSPNET_ADHOC_SLP_TIMEDOUT;
		} else if (ti->slpFlags & PSPNET_ADHOC_SLP_ABORTED) {
			ti->slpFlags &= ~PSPNET_ADHOC_SLP_ABORTED;
			ret = SCE_NET_ERROR_EINTR;
		}
	}
	sceKernelUnlockLwMutex(&g_adhocSlpqueMutex, 1);
	return ret;
}

__attribute__((noinline))
void adhocSleepTimeout(void *arg)
{
	AdhocThreadInfo *ti = (AdhocThreadInfo *)arg;

	sceKernelLockLwMutex(&g_adhocSlpqueMutex, 1, NULL);
	if (ti->wchan != NULL) {
		ti->wchan = NULL;
		ti->slpFlags |= PSPNET_ADHOC_SLP_TIMEDOUT;
		sceKernelSignalSema(ti->sema, 1);
	}
	sceKernelUnlockLwMutex(&g_adhocSlpqueMutex, 1);
}

__attribute__((noinline))
void adhocWakeup(void *wchan)
{
	AdhocSlpqueHead *head;
	AdhocThreadInfo *ti;

	sceKernelLockLwMutex(&g_adhocSlpqueMutex, 1, NULL);
	head = slpqueBucket(wchan);
	for (ti = head->first; ti != NULL; ti = ti->slpNext) {
		if (ti->wchan == wchan) {
			ti->wchan = NULL;
			sceKernelSignalSema(ti->sema, 1);
		}
	}
	sceKernelUnlockLwMutex(&g_adhocSlpqueMutex, 1);
}

__attribute__((noinline))
void adhocNotifyAlert(AdhocSocket *sock, int alertFlags)
{
	sceKernelLockLwMutex(&g_adhocSlpqueMutex, 1, NULL);
	if (g_adhocAlertCallbackSet > 0)
		g_adhocAlertCallback(sock, alertFlags);
	sceKernelUnlockLwMutex(&g_adhocSlpqueMutex, 1);
}

__attribute__((noinline))
int adhocSetAlertCallback(void (*callback)(AdhocSocket *sock, int alertFlags))
{
	g_adhocAlertCallback = callback;
	g_adhocAlertCallbackSet = 1;
	return 0;
}

__attribute__((noinline))
int adhocClearAlertCallback(void)
{
	g_adhocAlertCallbackSet = 0;
	g_adhocAlertCallback = NULL;
	return 0;
}

__attribute__((noinline))
void adhocSlpqueNoop(void)
{
}
