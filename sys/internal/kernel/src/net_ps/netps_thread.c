/* Shared SceNetPs thread adapters; original source filename is unavailable. */

#include "sys/internal/kernel/src/net_ps/net_ps_internal.h"
#include <sce/netps.h>
#include <string.h>

int gNetPsSoftTick = 10000;
SceNetPsMutex gNetPsJumboMutex;
SceNetPsMutex gNetPsDevSendMutex;
SceNetPsMutex gNetPsDevRecvMutex;
SceNetPsMutex gNetPsEmuMutex;
SceNetPsMutex gNetPsPktMutex;
SceNetPsMutex gNetPsMemMutex;
SceNetPsMutex gNetPsThreadMutex;
SceUID gNetPsInitialProcessId;
SceNetPsRuntime gNetPsRuntime;
SceNetPsThreadParam gNetPsSoftThreadParam;

extern int tick;
extern int ticks;
extern int netisr;
extern int gNetPsSoftTick;
extern volatile SceNetPsTimeval gNetPsTime;
extern volatile SceNetPsTimeval mono_time;
extern int gNetPsStartTime;
extern int gNetPsInitialized;

SceInt64 sceNetPsGetSystemTime(SceUInt64 *value);
int sceNetPsExitThread(void);
SceUID sceNetPsGetProcessId(void);
SceUID sceNetPsGetThreadId(void);
int sceNetPsFormatUidName(SceUID uid, char *name, int length);
int sce_psnet_bnet_sync_create(SceNetPsSync *sync, unsigned int events,
			     const char *name);
int sce_psnet_bnet_mutex_create(SceNetPsMutex *mutex, const char *name);
void sceNetPsSeedRandom(void);
int init_crypto(void *(*allocate)(SceSize size), void (*release)(void *ptr));
void callout_startup(void);
void mbinit(void);
void soinit(void);
void ifinit(void);
void domaininit(void);
void if_attachdomain(void);
void loopattach(int count);
void pppoeattach(int count);
int sceNetPsCreateThread(SceUID *threadId, SceNetPsThreadParam *param,
			const char *name);
int sceNetPsIsApplicationProgram(int *error);
int sceNetPsIsGameBudget(void);
int callout_hardclock(void);
void softclock(void *arg);
int sceNetPsPollDevices(void);
void sceNetPsReceivePackets(void);
void sceNetPsIsdnInterrupt(void);
void arpintr(void);
void ipintr(void);
int sce_psnet_bnet_sync_delete(SceNetPsSync *sync);
int sce_psnet_bnet_sync_wait(SceNetPsSync *sync, unsigned int *timeout,
			   unsigned int flags);
unsigned int sce_psnet_bnet_sync_clear(SceNetPsSync *sync);
void sce_psnet_panic(const char *message, ...) __attribute__((noreturn));

/* Name unknown; signal every network thread waiting on this channel. */
int sceNetPsWakeup(const void *waitChannel, unsigned int events)
{
	SceNetPsThread *thread;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	result = 15;
	for (thread = gNetPsRuntime.threadHead; thread != NULL;
	     thread = thread->next) {
		if (thread->waitChannel == waitChannel) {
			result = 0;
			sce_psnet_bnet_sync_signal(&thread->sync, events);
		}
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	return result;
}

/* Name unknown; timer, protocol and deferred-thread work dispatcher. */
int sceNetPsSoftThread(void)
{
	SceUInt64 deadline, now;
	unsigned int timeout;
	int events, count;
	SceNetPsThread *thread;

	sceNetPsGetSystemTime(&deadline);
	deadline += gNetPsSoftTick;
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	for (;;) {
		if (gNetPsRuntime.flags & 2)
			return sceNetPsExitThread();
		sceNetPsGetSystemTime(&now);
		if (now < deadline) {
			timeout = deadline - now;
			sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
			events = sce_psnet_bnet_sync_wait(&gNetPsRuntime.sync,
							&timeout, 0);
			sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
			if (events != 0) {
				if (events & 4)
					sceNetPsReceivePackets();
				if (events & 2) {
					if (netisr & (1 << 26))
						sceNetPsIsdnInterrupt();
					if (netisr & (1 << 28))
						arpintr();
					if (netisr & (1 << 2))
						ipintr();
				}
				continue;
			}
		}
		deadline += gNetPsSoftTick;
		if ((gNetPsTime.tv_usec += gNetPsSoftTick) > 999999) {
			gNetPsTime.tv_usec -= 1000000;
			gNetPsTime.tv_sec++;
		}
		if ((mono_time.tv_usec += gNetPsSoftTick) > 999999) {
			mono_time.tv_usec -= 1000000;
			mono_time.tv_sec++;
		}
		count = 0;
		while (count < gNetPsSoftTick / tick) {
			count++;
			ticks++;
			if (callout_hardclock() != 0)
				softclock(NULL);
		}
		sceNetPsPollDevices();
		sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
		if (gNetPsRuntime.flags & 4) {
			gNetPsRuntime.flags &= ~4u;
			while ((thread = gNetPsRuntime.threadHead) != NULL) {
				while (!(thread->flags & 0x10)) {
					thread = thread->next;
					if (thread == NULL)
						goto threadsDone;
				}
				if (thread->next != NULL)
					thread->next->prev = thread->prev;
				else
					gNetPsRuntime.threadTail = thread->prev;
				if (thread->prev != NULL)
					thread->prev->next = thread->next;
				else
					gNetPsRuntime.threadHead = thread->next;
				sce_psnet_bnet_sync_delete(&thread->sync);
				sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
				sceNetPsFree(thread);
				sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
			}
		}
threadsDone:
		sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	}
}

/* Name unknown; locate or create the network state of a kernel thread. */
SceNetPsThread *sceNetPsGetThread(SceUID threadId, int create)
{
	SceNetPsThread *thread;

	for (thread = gNetPsRuntime.threadHead; thread != NULL;
	     thread = thread->next) {
		if (thread->threadId == threadId && !(thread->flags & 0x10)) {
			if (thread->processId == gNetPsInitialProcessId)
				thread->processId = sceNetPsGetProcessId();
			return thread;
		}
	}
	if (!create)
		return NULL;
	thread = sceNetPsMalloc(sizeof(*thread), 2, 8);
	if (thread == NULL)
		return NULL;
	strncpy(thread->name, "SceNetKernelThread********", 31);
	sceNetPsFormatUidName(threadId, thread->name, 31);
	if (sce_psnet_bnet_sync_create(&thread->sync, 0, thread->name) < 0) {
		sceNetPsFree(thread);
		return NULL;
	}
	thread->processId = sceNetPsGetProcessId();
	thread->threadId = threadId;
	thread->prev = gNetPsRuntime.threadTail;
	if (gNetPsRuntime.threadTail != NULL)
		gNetPsRuntime.threadTail->next = thread;
	else
		gNetPsRuntime.threadHead = thread;
	thread->next = NULL;
	gNetPsRuntime.threadTail = thread;
	return thread;
}

/* Names unknown; defer destruction until the soft thread owns the lock. */
int sceNetPsRetireThread(SceUID threadId)
{
	SceNetPsThread *thread;

	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	for (thread = gNetPsRuntime.threadHead; thread != NULL;
	     thread = thread->next) {
		if (thread->threadId == threadId)
			thread->flags |= 0x10;
	}
	gNetPsRuntime.flags |= 4;
	return sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
}

SceUID sceNetPsGetSoftThread(void)
{
	return gNetPsRuntime.softThread;
}

int sceNetPsIsSoftThread(void)
{
	return sceNetPsGetThreadId() == gNetPsRuntime.softThread;
}

/* Names unknown; per-thread network error and call-mode accessors. */
int sceNetPsGetThreadError(SceUID threadId, int *error)
{
	SceNetPsThread *thread;

	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	thread = sceNetPsGetThread(threadId, 0);
	if (thread != NULL) {
		*error = thread->error;
		sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
		return 0;
	}
	*error = 0;
	sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	return -22;
}

int sceNetPsSetThreadError(SceUID threadId, int error)
{
	SceNetPsThread *thread;

	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	thread = sceNetPsGetThread(threadId, 1);
	if (thread != NULL) {
		thread->error = error;
		sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	return -22;
}

int sceNetPsGetThreadMode(SceUID threadId, int *mode)
{
	SceNetPsThread *thread;

	if (threadId == gNetPsRuntime.softThread) {
		*mode = 1;
		return 0;
	}
	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	thread = sceNetPsGetThread(threadId, 0);
	if (thread != NULL) {
		*mode = thread->mode;
		sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
		return 0;
	}
	*mode = 0;
	sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	return -22;
}

int sceNetPsSetThreadMode(SceUID threadId, int mode)
{
	SceNetPsThread *thread;

	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	thread = sceNetPsGetThread(threadId, 1);
	if (thread != NULL) {
		thread->mode = mode;
		sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	return -22;
}

/* Name unknown; snapshot the threads marked for network diagnostics. */
int sceNetPsGetThreadList(SceNetPsThreadInfo *info, int capacity)
{
	SceNetPsThread *thread;
	int count;

	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	count = 0;
	for (thread = gNetPsRuntime.threadHead; thread != NULL;
	     thread = thread->next) {
		if (thread->flags & 4) {
			count++;
			memset(info, 0, sizeof(*info));
			info++;
			info[-1].processId = thread->processId;
			info[-1].threadId = thread->threadId;
			if (count >= capacity)
				break;
		}
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	return count;
}

/* Name unknown; retrieve one recorded sleep-channel counter. */
int sceNetPsGetSleepCount(SceNetPsSleepCount *count, int index)
{
	if (index > 3)
		return 0;
	if ((int)gNetPsRuntime.sleepCounts[index].count <= 0)
		return 0;
	*count = gNetPsRuntime.sleepCounts[index];
	return 1;
}

/* Name unknown; maintain the four diagnostic activity counters. */
int sceNetPsRecordSleep(unsigned int identifier, int clear)
{
	SceNetPsThread *thread;
	SceNetPsSleepCount *count;
	int index, available;

	if (clear) {
		sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
		for (thread = gNetPsRuntime.threadHead; thread != NULL;
		     thread = thread->next)
			thread->flags &= ~4u;
		memset(gNetPsRuntime.sleepCounts, 0,
		       sizeof(gNetPsRuntime.sleepCounts));
		sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	thread = sceNetPsGetThread(sceNetPsGetThreadId(), 1);
	sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	if (thread == NULL)
		return 0;
	thread->flags |= 4;
	available = -1;
	for (index = 0; index != 4; index++) {
		count = &gNetPsRuntime.sleepCounts[index];
		if (count->count != 0) {
			if (count->identifier == identifier) {
				count->count++;
				return 0;
			}
		} else {
			available = index;
		}
	}
	if (available == -1)
		return 0;
	gNetPsRuntime.sleepCounts[available].identifier = identifier;
	gNetPsRuntime.sleepCounts[available].count = 1;
	return 0;
}

/* Name unknown; sleep on a channel while releasing the network mutex. */
int sceNetPsSleep(const void *waitChannel, unsigned int flags,
		 const char *message, int delay)
{
	SceNetPsThread *thread;
	unsigned int timeout = 0;
	unsigned int *timeoutp = NULL;
	unsigned int events;

	sce_psnet_bnet_mutex_lock(&gNetPsThreadMutex, NULL);
	thread = sceNetPsGetThread(sceNetPsGetThreadId(), 1);
	sce_psnet_bnet_mutex_unlock(&gNetPsThreadMutex);
	if (thread == NULL)
		sce_psnet_panic("", thread);
	thread->waitChannel = waitChannel;
	if (delay > 0) {
		timeout = delay;
		timeoutp = &timeout;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	events = sce_psnet_bnet_sync_wait(&thread->sync, timeoutp, flags);
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	thread->waitChannel = NULL;
	events |= sce_psnet_bnet_sync_clear(&thread->sync);
	if (events & 0x40000)
		return 87;
	if ((int)timeout < 0)
		return 35;
	if (events & 0x80000)
		return 163;
	if (events & 0x20000)
		return 4;
	return events != 0 ? 0 : 4;
}

/* Name unknown; consume the first pending wakeup for this channel. */
int sceNetPsConsumeWakeup(const void *waitChannel, unsigned int events)
{
	SceNetPsPendingWakeup *pending;

	for (pending = gNetPsRuntime.pendingHead; pending != NULL;
	     pending = pending->next) {
		if (pending->waitChannel == waitChannel &&
		    (events == 0 || (events & pending->events) != 0)) {
			if (pending->next != NULL)
				pending->next->prev = pending->prev;
			else
				gNetPsRuntime.pendingTail = pending->prev;
			if (pending->prev != NULL)
				pending->prev->next = pending->next;
			else
				gNetPsRuntime.pendingHead = pending->next;
			sceNetPsFree(pending);
			return 1;
		}
	}
	return 0;
}

/* Name unknown; retain one pending wakeup per channel. */
int sceNetPsQueueWakeup(const void *waitChannel, unsigned int events)
{
	SceNetPsPendingWakeup *pending;

	for (pending = gNetPsRuntime.pendingHead; pending != NULL;
	     pending = pending->next) {
		if (pending->waitChannel == waitChannel)
			return 0;
	}
	pending = sceNetPsMalloc(sizeof(*pending), 2, 8);
	if (pending == NULL)
		return 12;
	pending->waitChannel = waitChannel;
	pending->events = events;
	pending->prev = gNetPsRuntime.pendingTail;
	if (gNetPsRuntime.pendingTail != NULL)
		gNetPsRuntime.pendingTail->next = pending;
	else
		gNetPsRuntime.pendingHead = pending;
	pending->next = NULL;
	gNetPsRuntime.pendingTail = pending;
	return 0;
}

/* Name unknown; signal a normal channel wakeup. */
int sceNetPsWakeupNormal(const void *waitChannel)
{
	return sceNetPsWakeup(waitChannel, 0x10000);
}

/* Name unknown; cancel a channel wait. */
int sceNetPsWakeupCanceled(const void *waitChannel)
{
	return sceNetPsWakeup(waitChannel, 0x40000);
}

/* Name unknown; interrupt a channel wait. */
int sceNetPsWakeupInterrupted(const void *waitChannel)
{
	return sceNetPsWakeup(waitChannel, 0x20000);
}

/* Name unknown; report an IP address change to a channel waiter. */
int sceNetPsWakeupAddressChanged(const void *waitChannel)
{
	return sceNetPsWakeup(waitChannel, 0x80000);
}

/* Name unknown; copy the network stack's current wall clock. */
SceNetPsTimeval *sceNetPsGetTime(SceNetPsTimeval *value)
{
	value->tv_sec = gNetPsTime.tv_sec;
	value->tv_usec = gNetPsTime.tv_usec;
	return value;
}

/* SCE replacement: NetBSD's packet-rate limiter always permits the packet. */
int ppsratecheck(SceNetPsTimeval *lasttime, int *curpps, int maxpps)
{
	return 1;
}

/* Name unknown; request protocol processing on the network soft thread. */
int sceNetPsScheduleProtocols(void)
{
	return sce_psnet_bnet_sync_signal(&gNetPsRuntime.sync, 2);
}

/* Name unknown; request packet reception on the network soft thread. */
int sceNetPsScheduleReceive(void)
{
	return sce_psnet_bnet_sync_signal(&gNetPsRuntime.sync, 4);
}

/* Name unknown; normalize every negative timeout to the infinite sentinel. */
int sceNetPsNormalizeTimeout(int timeout)
{
	return timeout | (timeout >> 31);
}

/* Name unknown; initialize the kernel network synchronization objects. */
int sceNetPsInitializeSync(void)
{
	gNetPsInitialProcessId = sceNetPsGetProcessId();
	sce_psnet_bnet_sync_create(&gNetPsRuntime.sync, 0, "SceNetKernelSoft");
	sce_psnet_bnet_mutex_create(&gNetPsJumboMutex, "SceNetKernelJumbo");
	sce_psnet_bnet_mutex_create(&gNetPsDevSendMutex, "SceNetKernelDevSend");
	sce_psnet_bnet_mutex_create(&gNetPsDevRecvMutex, "SceNetKernelDevRecv");
	sce_psnet_bnet_mutex_create(&gNetPsEmuMutex, "SceNetKernelEmu");
	sce_psnet_bnet_mutex_create(&gNetPsPktMutex, "SceNetKernelPkt");
	sce_psnet_bnet_mutex_create(&gNetPsMemMutex, "SceNetKernelMem");
	sce_psnet_bnet_mutex_create(&gNetPsThreadMutex, "SceNetKernelThread");
	return 0;
}

/* Name unknown; initialize NetBSD and start Sony's network soft thread. */
int sceNetPsStartStack(const SceNetPsThreadParam *param)
{
	SceUInt64 now;

	if (gNetPsRuntime.flags & 1)
		sce_psnet_panic("", 0);
	sceNetPsSeedRandom();
	if (init_crypto(sceNetPsCalloc, sceNetPsRelease) < 0)
		sce_psnet_panic("init_crypto", 0);
	callout_startup();
	mbinit();
	soinit();
	ifinit();
	domaininit();
	if_attachdomain();
	loopattach(1);
	pppoeattach(1);
	sceNetPsGetSystemTime(&now);
	gNetPsTime.tv_sec = (int)now;
	mono_time.tv_sec = (int)now;
	gNetPsStartTime = (int)now;
	gNetPsSoftThreadParam = *param;
	gNetPsSoftThreadParam.arg = NULL;
	gNetPsSoftThreadParam.entry = (void (*)(void *))sceNetPsSoftThread;
	sceNetPsCreateThread(&gNetPsRuntime.softThread, &gNetPsSoftThreadParam,
			     param->name);
	gNetPsInitialized = 1;
	gNetPsRuntime.flags |= 1;
	return 0;
}


