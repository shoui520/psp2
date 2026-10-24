/* Shared SceNetPs thread adapters; original source filename is unavailable. */

#include <fw.h>
#include "net_ps_internal.h"
#include "netps_resource.h"
#include <net_ps/netps.h>
#include <string.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>

/* NetBSD's kernel parameters (conf/param.c, kern_clock.c), with net_ps's configuration */
int mcllowat = 8;
int mblowat = 16;
int tick = 1000000 / 1000;
struct proc *curproc = &proc0;
int hz = 1000;
int nmbclusters = 1024;
int gNetPsSoftTick = 10000;
const int msize = MSIZE;
const int mclbytes = MCLBYTES;
struct timeval boottime;
int hardclock_ticks;
volatile struct timeval time __attribute__((__aligned__(__alignof__(quad_t))));
volatile struct timeval mono_time;
struct proc proc0;
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

extern int netisr;
extern int gNetPsInitialized;

void callout_startup(void);
void mbinit(void);
void ifinit(void);
void domaininit(void);
void if_attachdomain(void);
void loopattach(int count);
void pppoeattach(int count);
int callout_hardclock(void);
void softclock(void *arg);
void sceNetPsReceivePackets(void);
void sceNetPsIsdnInterrupt(void);
void arpintr(void);
void ipintr(void);

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
		if ((time.tv_usec += gNetPsSoftTick) > 999999) {
			time.tv_usec -= 1000000;
			time.tv_sec++;
		}
		if ((mono_time.tv_usec += gNetPsSoftTick) > 999999) {
			mono_time.tv_usec -= 1000000;
			mono_time.tv_sec++;
		}
		count = 0;
		while (count < gNetPsSoftTick / tick) {
			count++;
			hardclock_ticks++;
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
	if (thread == NULL) {
		sce_psnet_panic("", thread);
		__builtin_unreachable();
	}
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

/* NetBSD's microtime: the network stack's current wall clock (if_up, if_down). */
void microtime(struct timeval *tv)
{
	tv->tv_sec = time.tv_sec;
	tv->tv_usec = time.tv_usec;
}

/* SCE replacement: NetBSD's packet-rate limiter always permits the packet. */
int ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
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

/*
 * The bounds of the sanity checks: a protocol switch lies in the data segment's first
 * 0x105c bytes (from its first object), a protocol function between the first function and
 * an end the original compares without a relocation (its link's symbols are unknown, and
 * the value is neither segment's end): 0x40 bytes further in 4.00, 0x120 in the development
 * kits' build
 */
#define bnet_protosw_start	((const char *)&gNetPsOtherDumpCursor)
#define bnet_protosw_end	((const char *)&gNetPsOtherDumpCursor + 0x105c)
#define bnet_text_start		((const char *)sceNetPsCalloc)
#if FW_SINCE(0x04000000)
#define bnet_text_base		0x810558e0
#else
#define bnet_text_base		0x810558a0
#endif
#ifdef NET_PS_PACKET_CAPTURE
#define bnet_text_end		((const char *)(bnet_text_base + 0x120))
#else
#define bnet_text_end		((const char *)bnet_text_base)
#endif

/*
 * A bad socket does not come back from the panic; a bad protocol switch is reported
 * and the checks go on (the original's code)
 */
#define BNET_SANITY_PANIC(value) \
	do { \
		sce_psnet_panic("", __func__, (value)); \
		__builtin_unreachable(); \
	} while (0)

static int bnet_outside(const void *pointer, const char *start, const char *end)
{
	return (const char *)pointer < start || (const char *)pointer > end;
}

static void bnet_protosw_check(const struct protosw *pr)
{
	if (pr != NULL && bnet_outside(pr, bnet_protosw_start, bnet_protosw_end))
		sce_psnet_panic("", __func__, pr);
	if (pr->pr_usrreq != NULL &&
	    bnet_outside(pr->pr_usrreq, bnet_text_start, bnet_text_end))
		sce_psnet_panic("", __func__, pr->pr_usrreq);
	if (pr->pr_output != NULL &&
	    bnet_outside(pr->pr_output, bnet_text_start, bnet_text_end))
		sce_psnet_panic("", __func__, pr->pr_output);
	if (pr->pr_ctloutput != NULL &&
	    bnet_outside(pr->pr_ctloutput, bnet_text_start, bnet_text_end))
		sce_psnet_panic("", __func__, pr->pr_ctloutput);
}

/* Check a socket before its protocol is called: a heap block with net_ps's handlers */
void bnet_socket_sanity_check(struct socket *so)
{
	if (!sceNetPsIsHeapAddress(so))
		BNET_SANITY_PANIC(so);
	if (so->so_send != sosend)
		BNET_SANITY_PANIC(so->so_send);
	if (so->so_receive != soreceive)
		BNET_SANITY_PANIC(so->so_receive);
	bnet_protosw_check(so->so_proto);
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

	if (gNetPsRuntime.flags & 1) {
		sce_psnet_panic("", 0);
		__builtin_unreachable();
	}
	sceNetPsSeedRandom();
	if (init_crypto(sceNetPsCalloc, sceNetPsRelease) < 0) {
		sce_psnet_panic("init_crypto", 0);
		__builtin_unreachable();
	}
	callout_startup();
	mbinit();
	soinit();
	ifinit();
	domaininit();
	if_attachdomain();
	loopattach(1);
	pppoeattach(1);
	sceNetPsGetSystemTime(&now);
	time.tv_sec = (int)now;
	mono_time.tv_sec = (int)now;
	boottime.tv_sec = (int)now;
	gNetPsSoftThreadParam = *param;
	gNetPsSoftThreadParam.arg = NULL;
	gNetPsSoftThreadParam.entry = (void (*)(void *))sceNetPsSoftThread;
	sceNetPsCreateThread(&gNetPsRuntime.softThread, &gNetPsSoftThreadParam,
			     param->name);
	gNetPsInitialized = 1;
	gNetPsRuntime.flags |= 1;
	return 0;
}

