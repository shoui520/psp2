/*
 * SceKernelThreadMgr: the event object (SceUIDEventObject) and the waits on
 * several events at once (Sony's WMECB). Message pipes and CPU timers are
 * subclasses of SceUIDEventClass and start with this object.
 */

#ifndef THREADMGR_EVENT_H
#define THREADMGR_EVENT_H

#include <stdbool.h>
#include <stdint.h>
#include <psp2kern/types.h>
#include <threadmgr/threadmgr_event.h>
#include <threadmgr/event.h>
#include "private.h"
#include <threadmgr/uid_waitable.h>
#include "mutex.h"

#define EVENT_ATTR_AUTO_CLEAR		0x00000100	/* a wait that is satisfied clears its bits */
#define EVENT_ATTR_UNINTERRUPTIBLE	0x00008000	/* PLACEHOLDER: name; see WAIT_ATTR_UNINTERRUPTIBLE */
#define EVENT_PATTERN_RESERVED		0x00010000	/* never set or cleared by a caller */
#define EVENT_PATTERN_KERNEL		0x00ffffff	/* the bits a user caller may not touch */

#define WAIT_TYPE_EVENT			0x00010000
#define WAIT_TYPE_MULTIPLE_EVENTS	0x00020000
#define WAIT_ID_MULTIPLE_EVENTS		((SceUID)0x80000000)	/* thread->waitId while in a WME */

#define WME_MODE_ANY			1	/* the wait ends with the first event */
#define WME_MODE_ALL			2	/* the wait ends when every event has come */
#define WME_MAX_EVENTS			64

#define SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID	0x80028201
#define SCE_KERNEL_ERROR_EVENT_COND		0x80028202
#define SCE_KERNEL_ERROR_ILLEGAL_PATTERN	0x8002000d

#define EVENT_CALLBACK_HEAD(event)	((ThreadmgrEventCallback *)&(event)->callbackNext)
#define EVENT_WAIT_QUEUE(event)		((uintptr_t)&(event)->next)

/* WMECB: one wait on several events ((count + 6) * 8 bytes) */
typedef struct ThreadmgrWaitMultipleEvents {
	struct ThreadmgrWaitNode *next;		/* 0x00: the thread waiting on it */
	struct ThreadmgrWaitNode *prev;
	uint32_t mode;				/* 0x08: WME_MODE_* */
	uint32_t count;				/* 0x0c */
	uint32_t pending;			/* 0x10: nodes queued on events */
	uint32_t resultCount;			/* 0x14 */
	uint32_t busy;				/* 0x18: the waiter is setting up or tearing down */
	ThreadmgrWaitEventResult *results;	/* 0x1c */
	uint32_t ready[WME_MAX_EVENTS / 32];	/* 0x20: the events done with */
	const ThreadmgrWaitEvent *events;	/* 0x28 */
	void *userEventIds;			/* 0x2c: the caller's IDs, kept for a restarted syscall */
	ListNode nodes[];	/* 0x30: per event, its queued node */
} ThreadmgrWaitMultipleEvents;

#define WME_SIZE(count)	(offsetof(ThreadmgrWaitMultipleEvents, nodes) + \
			 (count) * sizeof(ListNode))

/* a WME's place in one event's waiters (0x40, from the waiting process's heap) */
typedef struct ThreadmgrEventWaitNode {
	struct ThreadmgrWaitNode *next;		/* 0x00: in the event's waiters */
	struct ThreadmgrWaitNode *prev;
	struct ThreadCB *thread;	/* 0x08: bit 0 clear, unlike a thread's own UID */
	SceUID eventId;				/* 0x0c */
	ListNode wmeLink;	/* 0x10: in the WMECB's nodes[index] */
	uint32_t pattern;			/* 0x18 */
	uint32_t index;				/* 0x1c */
	uint8_t reserved[0x20];
} ThreadmgrEventWaitNode;

_Static_assert(sizeof(ThreadmgrEventWaitNode) == 0x40, "WME node size");

#define EVENT_WAIT_NODE_FROM_LINK(link) \
	((ThreadmgrEventWaitNode *)((uintptr_t)(link) - offsetof(ThreadmgrEventWaitNode, wmeLink)))


void notifyCallbackToThreadBeforeSetEvent(ThreadmgrEventCallback *head,
					  struct ThreadCB *thread, uint32_t arg);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);
extern SceClass *SceSysmemForKernel_368F1C36(SceUID guid);

/* the waiter at a node of an event's queue: a thread itself, or a WME node */
static inline int EventWaiterIsThread(struct ThreadmgrWaitNode *node)
{
	return node->owner & 1;
}

/* takes a WME node off its event and its WMECB; returns the nodes the WME still has queued */
static inline uint32_t unlinkEventWaitNode(ThreadmgrEventWaitNode *node,
					   ThreadmgrWaitMultipleEvents *wme)
{
	node->next->prev = node->prev;
	node->prev->next = node->next;
	node->next = (struct ThreadmgrWaitNode *)node;
	node->prev = (struct ThreadmgrWaitNode *)node;
	ListLinkRemove(&node->wmeLink);
	node->wmeLink.next = &node->wmeLink;
	node->wmeLink.prev = &node->wmeLink;
	return --wme->pending;
}

static inline void addEventWaitResult(ThreadmgrWaitMultipleEvents *wme, SceUID eventId,
				      int result, uint32_t pattern, uint64_t userData)
{
	ThreadmgrWaitEventResult *res;

	if (wme->results == NULL)
		return;
	res = &wme->results[wme->resultCount];
	res->eventId = eventId;
	res->pattern = pattern;
	res->userData = userData;
	res->result = result;
	wme->resultCount++;
}

static inline void markEventReady(ThreadmgrWaitMultipleEvents *wme, uint32_t index)
{
	wme->ready[index >> 5] |= 1u << (index & 31);
}

/* the event class's release method for a WME node's thread */
static inline int releaseEventWaiter(ThreadmgrEvent *event, ThreadmgrEventWaitNode *node,
				     struct ThreadCB *thread)
{
	struct ThreadmgrWaitableClass *cls =
		(struct ThreadmgrWaitableClass *)s_threadmgrRuntime.manager->eventClass;

	return ((int (*)(SceClass *, void *, void *))cls->release)(
		SceSysmemForKernel_368F1C36(node->eventId), event, thread);
}

/*
 * A WME node's event came with result: the WME records it, and its thread
 * wakes when the wait is over (otherwise the node just leaves the event).
 */
static inline __attribute__((always_inline)) uint32_t completeEventWaitNode(
	ThreadmgrEvent *event, ThreadmgrEventWaitNode *node, struct ThreadCB *thread,
	ThreadmgrWaitMultipleEvents *wme, uint32_t left, SceUID waitId, int result,
	uint32_t pattern, uint64_t userData, int notify, uint32_t arg)
{
	addEventWaitResult(wme, node->eventId, result, pattern, userData);
	markEventReady(wme, node->index);
	if (waitId == WAIT_ID_MULTIPLE_EVENTS &&
	    (left == 0 || wme->mode == WME_MODE_ANY)) {
		if (notify)
			notifyCallbackToThreadBeforeSetEvent(EVENT_CALLBACK_HEAD(event), thread, arg);
		return wakeupAndDistributeThread(thread, EVENT_WAIT_QUEUE(event), 0);
	}
	releaseEventWaiter(event, node, thread);
	return 0;
}

uint32_t notifyCallbackAllAfterSetEvent(SceUID eventId, ThreadmgrEvent *event, uint32_t arg);
int freeEventWaitNode(SceUID pid, ThreadmgrEventWaitNode *node);
void cancelWME(ThreadmgrWaitMultipleEvents *wme);
void startEvent(void);
int sceKernelUIDInheritedEventClassInit(struct ThreadmgrWaitableClass *cls, const char *name,
				   SceSize size, SceClassCallback create, SceClassCallback destroy);
int uidEventSubclassInit(struct ThreadmgrWaitableClass *cls, const char *name, SceClass *parent,
			 SceSize size, SceClassCallback create, SceClassCallback destroy,
			 ThreadmgrCancelWaitMethod cancelWait, ThreadmgrRevalidateMethod revalidate, ThreadmgrReleaseMethod release);
uint32_t setEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
				    uint32_t arg, ThreadmgrEvent *event);
uint32_t pulseEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
				      uint32_t arg, ThreadmgrEvent *event);
int sceKernelEventObjectInit(SceUID eventId, uint32_t attr, uint64_t userData, uint32_t pattern);
int ksceKernelSetEvent(SceUID eventId, uint32_t pattern, uint64_t userData);
int sceKernelSetEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
				   uint32_t arg);
int ksceKernelClearEvent(SceUID eventId, uint32_t keep);

#endif
