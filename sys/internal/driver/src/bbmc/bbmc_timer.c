/* SceBbmc: timer queue owner (Sony's bbmc_timer.c). */
#include <stddef.h>
#include <string.h>
#include <sysmem/debug.h>
#include <threadmgr/fast_mutex_object.h>
#include <threadmgr/threadmgr.h>
#include "bbmc_memory.h"

typedef struct BbmcTimerNode {
	struct BbmcTimerNode *previous;
	struct BbmcTimerNode *next;
	void (*callback)(void *arg);
	void *arg;
	int id;
	SceUInt32 padding;
	SceUInt64 deadline;
} BbmcTimerNode;

typedef struct BbmcTimerState {
	BbmcTimerNode *head;
	BbmcTimerNode *tail;
	SceUID thread;
	struct ThreadmgrFastMutex mutex;
	SceUID semaphore;
	SceUID alarm;
} BbmcTimerState;

_Static_assert(sizeof(BbmcTimerNode) == 32, "BbmcTimerNode size");
_Static_assert(offsetof(BbmcTimerNode, deadline) == 24, "timer deadline offset");
_Static_assert(sizeof(BbmcTimerState) == 84, "BbmcTimerState size");
_Static_assert(offsetof(BbmcTimerState, semaphore) == 76, "timer semaphore offset");

static BbmcTimerState g_bbmcTimerState;
static BbmcTimerState *g_bbmcTimer;

/* Canonical owner work area at the SDK's opaque fast-mutex boundary. */
extern int bbmcInitializeFastMutex(struct ThreadmgrFastMutex *mutex, const char *name,
				 int attr, void *opt) __asm__("sceKernelInitializeFastMutex");
extern int bbmcFinalizeFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelFinalizeFastMutex");
extern int bbmcLockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("lockFastMutex");
extern int bbmcUnlockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelUnlockFastMutex");

/* Sony names/NIDs from the ThreadMgr owner export table. */

int bbmcTimerThread(SceSize args, void *argp);

SceUInt32 alarmHandler(void *arg)
{
	BbmcTimerState *state = arg;

	if (sceKernelSignalSema(state->semaphore, 1) < 0)
		SCE_DBG_ASSERT(0, 253);
	state->alarm = 0;
	return 0;
}

/* name unknown: execute due callbacks and arm the next timer in queue order. */
int bbmcTimerThread(SceSize args, void *argp)
{
	BbmcTimerState *state = *(BbmcTimerState **)argp;
	BbmcTimerNode *node, *previous, *next;
	SceUInt64 now, deadline;
	SceUInt32 delay;
	int ret;

	while (sceKernelWaitSema(state->semaphore, 1, NULL) >= 0) {
		if (bbmcLockFastMutex(&g_bbmcTimer->mutex) < 0)
			continue;
		if (state->alarm > 0) {
			sceKernelCancelAlarm(state->alarm);
			state->alarm = 0;
		}
		sceKernelGetSystemTimeWide();
		while (state->head != NULL) {
			now = sceKernelGetSystemTimeWide();
			node = state->head;
			deadline = node != NULL ? node->deadline : 0;
			if (deadline - now > 499 && deadline >= now) {
				if (node != NULL) {
					delay = (SceUInt32)node->deadline - (SceUInt32)now;
					if (state->alarm > 0) {
						sceKernelCancelAlarm(state->alarm);
						state->alarm = 0;
					}
					ret = SceThreadmgrForDriver_C58DF384("SceBbmcTimerEvent", delay,
								      alarmHandler, state);
					if (ret >= 0)
						state->alarm = ret;
				}
				break;
			}
			node = g_bbmcTimer->head;
			previous = node->previous;
			next = node->next;
			if (previous != NULL) {
				previous->next = next;
				if (next != NULL)
					goto link_next;
				state->tail = previous;
				previous->next = NULL;
				goto check_links;
			}
			state->head = next;
			if (next != NULL)
				goto link_next;
			goto clear_links;
link_next:
			next->previous = previous;
check_links:
			if (state->head != NULL && state->tail != NULL)
				goto run_callback;
clear_links:
			state->head = NULL;
			state->tail = NULL;
run_callback:
			node->callback(node->arg);
			bbmcFree(node);
		}
		bbmcUnlockFastMutex(&g_bbmcTimer->mutex);
	}
	return 0;
}

/* name unknown: creates the timer worker and its complete controller. */
int bbmcInitializeTimer(void)
{
	BbmcTimerState *state;
	int ret, mutexReady = 0;

	if (g_bbmcTimer != NULL)
		return 0x80330004;
	memset(&g_bbmcTimerState, 0, sizeof(g_bbmcTimerState));
	ret = bbmcInitializeFastMutex(&g_bbmcTimerState.mutex, "SceBbmcTimerEvent", 2, NULL);
	if (ret < 0)
		goto fail;
	mutexReady = 1;
	ret = sceKernelCreateSema("SceBbmcTimerEvent", 0, 0, 1, NULL);
	if (ret < 0)
		goto fail;
	g_bbmcTimerState.semaphore = ret;
	ret = sceKernelCreateThread("SceBbmcTimerEvent", bbmcTimerThread, 94, 0x2000,
				     0x10000000, 0x80000, NULL);
	if (ret < 0)
		goto fail;
	g_bbmcTimerState.thread = ret;
	state = &g_bbmcTimerState;
	ret = sceKernelStartThread(ret, sizeof(state), &state);
	if (ret < 0)
		goto fail;
	g_bbmcTimer = &g_bbmcTimerState;
	return 0;

fail:
	if (g_bbmcTimerState.thread > 0)
		sceKernelDeleteThread(g_bbmcTimerState.thread);
	if (g_bbmcTimerState.semaphore > 0)
		sceKernelDeleteSema(g_bbmcTimerState.semaphore);
	if (mutexReady != 0)
		bbmcFinalizeFastMutex(&g_bbmcTimerState.mutex);
	return ret;
}

/* name unknown: stops the worker before destroying its owner and queued nodes. */
int bbmcFinalizeTimer(void)
{
	BbmcTimerNode *node, *next;

	if (g_bbmcTimer == NULL)
		return 0x80330003;
	g_bbmcTimer = NULL;
	if (g_bbmcTimerState.alarm > 0) {
		sceKernelCancelAlarm(g_bbmcTimerState.alarm);
		g_bbmcTimerState.alarm = 0;
	}
	sceKernelCancelSema(g_bbmcTimerState.semaphore, 0, NULL);
	sceKernelWaitThreadEnd(g_bbmcTimerState.thread, NULL, NULL);
	sceKernelDeleteThread(g_bbmcTimerState.thread);
	sceKernelDeleteSema(g_bbmcTimerState.semaphore);
	bbmcFinalizeFastMutex(&g_bbmcTimerState.mutex);
	for (node = g_bbmcTimerState.head; node != NULL; node = next) {
		next = node->next;
		bbmcFree(node);
	}
	return 0;
}

/* name unknown: clears pending timers without destroying their controller. */
int bbmcFlushTimers(void)
{
	BbmcTimerNode *node, *next;

	if (g_bbmcTimer == NULL)
		return 0x80330003;
	if (g_bbmcTimer->alarm > 0) {
		sceKernelCancelAlarm(g_bbmcTimerState.alarm);
		g_bbmcTimer->alarm = 0;
	}
	node = g_bbmcTimer->head;
	if (node != NULL) {
		do {
			next = node->next;
			bbmcFree(node);
			node = next;
		} while (node != NULL);
		g_bbmcTimer->head = NULL;
		g_bbmcTimer->tail = NULL;
	}
	return 0;
}

/* name unknown: explicit no-op called before GPIO reset by the reset path. */
int bbmcTimerResetHook(void)
{
	return 0;
}

static inline __attribute__((always_inline)) int bbmcUnlinkTimer(BbmcTimerState *state,
							 BbmcTimerNode *node)
{
	BbmcTimerNode *previous = node->previous;
	BbmcTimerNode *next = node->next;

	if (previous != NULL) {
		previous->next = next;
		if (next != NULL)
			goto link_next;
		state->tail = previous;
		previous->next = NULL;
		goto check_links;
	}
	state->head = next;
	if (next != NULL)
		goto link_next;
	goto clear_links;
link_next:
	next->previous = previous;
check_links:
	if (state->head != NULL && state->tail != NULL)
		return 1;
clear_links:
	state->head = NULL;
	state->tail = NULL;
	return 0;
}

/* name unknown: insert a callback in deadline order and return its time-derived ID. */
int bbmcTimerSetHandler(SceUInt32 delay, void (*callback)(void *arg), void *arg)
{
	SceUInt64 now = sceKernelGetSystemTimeWide();
	BbmcTimerNode *node, *head, *tail, *next;
	SceUInt32 deadline;
	int id, ret;

	if (g_bbmcTimer == NULL)
		return 0x80330003;
	node = bbmcAlloc(sizeof(*node), 0);
	if (node == NULL)
		return 0x80330005;
	memset(node, 0, sizeof(*node));
	id = (SceInt32)(SceUInt32)now;
	if (id >= 0)
		node->id = id;
	else
		node->id = (SceInt32)(0U - (SceUInt32)id);
	node->callback = callback;
	node->deadline = now + delay;
	node->arg = arg;
	ret = bbmcLockFastMutex(&g_bbmcTimer->mutex);
	if (ret < 0)
		goto fail;
	if (g_bbmcTimer->alarm > 0) {
		sceKernelCancelAlarm(g_bbmcTimer->alarm);
		g_bbmcTimer->alarm = 0;
	}
	head = g_bbmcTimer->head;
	if (head != NULL) {
		tail = g_bbmcTimer->tail;
		if (tail != NULL) {
			if (tail->deadline < node->deadline)
				goto link_after;
			while ((tail = tail->previous) != NULL) {
				if (tail->deadline < node->deadline)
					goto link_after;
			}
		}
		g_bbmcTimer->head = node;
		node->previous = NULL;
		node->next = head;
		head->previous = node;
		head = node;
		goto register_alarm;
	}
	g_bbmcTimer->head = node;
	g_bbmcTimer->tail = node;
	node->previous = NULL;
	node->next = NULL;
	head = node;
	goto register_alarm;

link_after:
	next = tail->next;
	tail->next = node;
	if (next != NULL) {
		next->previous = node;
		node->previous = tail;
		node->next = next;
	} else {
		node->previous = tail;
		node->next = NULL;
		g_bbmcTimer->tail = node;
	}
register_alarm:
	deadline = (SceUInt32)head->deadline;
	now = sceKernelGetSystemTimeWide();
	ret = SceThreadmgrForDriver_C58DF384("SceBbmcTimerEvent", deadline - (SceUInt32)now,
				      alarmHandler, g_bbmcTimer);
	if (ret >= 0) {
		g_bbmcTimer->alarm = ret;
		id = node->id;
		bbmcUnlockFastMutex(&g_bbmcTimer->mutex);
		return id;
	}
	bbmcUnlinkTimer(g_bbmcTimer, node);
fail:
	bbmcFree(node);
	bbmcUnlockFastMutex(&g_bbmcTimer->mutex);
	return ret;
}

int bbmcTimerCancelHandler(int id)
{
	BbmcTimerNode *node;
	SceUInt64 now, deadline;
	SceUInt32 delay;
	int ret;

	if (g_bbmcTimer == NULL)
		return 0x80330003;
	ret = bbmcLockFastMutex(&g_bbmcTimer->mutex);
	if (ret < 0)
		return ret;
	if (g_bbmcTimer->alarm <= 0) {
		ret = 0x80330002;
		goto unlock;
	}
	sceKernelCancelAlarm(g_bbmcTimer->alarm);
	node = g_bbmcTimer->head;
	g_bbmcTimer->alarm = 0;
	while (node != NULL && node->id != id)
		node = node->next;
	if (node == NULL) {
		now = sceKernelGetSystemTimeWide();
		if (g_bbmcTimer->head != NULL &&
		    g_bbmcTimer->head->deadline - now >= 500 &&
		    g_bbmcTimer->head->deadline >= now)
			delay = (SceUInt32)(g_bbmcTimer->head->deadline - now);
		else
			delay = 200;
		ret = SceThreadmgrForDriver_C58DF384("SceBbmcTimerEvent", delay, alarmHandler,
					      g_bbmcTimer);
		if (ret < 0) {
			SCE_DBG_ASSERT(0, 640);
		} else {
			g_bbmcTimer->alarm = ret;
			ret = 0x80330002;
		}
		goto unlock;
	}
	now = sceKernelGetSystemTimeWide();
	bbmcUnlinkTimer(g_bbmcTimer, node);
	if (g_bbmcTimer->head == NULL || g_bbmcTimer->tail == NULL)
		goto free_node;
	deadline = g_bbmcTimer->head->deadline;
	if (deadline == 0)
		goto free_node;
	if (deadline - now < 500 || deadline < now) {
		delay = 200;
		bbmcFree(node);
		goto register_alarm;
	}
	delay = (SceUInt32)(deadline - now);
	bbmcFree(node);
	if (delay == 0)
		goto clear_alarm;
register_alarm:
	ret = SceThreadmgrForDriver_C58DF384("SceBbmcTimerEvent", delay, alarmHandler, g_bbmcTimer);
	if (ret < 0) {
		SCE_DBG_ASSERT(0, 659);
	} else {
		g_bbmcTimer->alarm = ret;
		ret = 0;
	}
	goto unlock;
free_node:
	bbmcFree(node);
clear_alarm:
	g_bbmcTimer->alarm = 0;
unlock:
	bbmcUnlockFastMutex(&g_bbmcTimer->mutex);
	return ret;
}
