/*
 * SceKernelThreadMgr: event.c, the events (SceUIDEventClass) and the waits
 * on several events at once (WME). Message pipes and CPU timers derive from
 * SceUIDEventClass. The user syscall wrappers are still in
 * event_notification.c.
 */

#include <string.h>
#include "timer.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "event.h"
#include "syscall_context.h"
#include <sysmem/sysroot.h>
#include <sysmem/uid_class.h>

int uidRegisterCallbackClassInit(struct ThreadmgrWaitableClass *cls, const char *name,
	SceClass *parent, SceSize size, SceClassCallback create, SceClassCallback destroy,
	ThreadmgrCancelWaitMethod cancelWait, ThreadmgrRevalidateMethod revalidate, ThreadmgrReleaseMethod release);
void deleteRegisteredCallbackList(ThreadmgrEventCallback *entry);

#define EVENT_CLASS()	(s_threadmgrRuntime.manager->eventClass)

static inline int EventUnknownIdError(void)
{
	return ThreadmgrInSyscall() ? (SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT) :
				      SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
}

/* a thread waiting on the event itself: it wakes if the pattern has its bits */
static inline uint32_t wakeEventWaiter(ThreadmgrEvent *event, struct ThreadCB *thread,
				       uint64_t userData)
{
	uint32_t matched, wake;

	if (thread->waitType != SCE_KERNEL_WAITTYPE_EVENT)
		return 0;
	matched = event->pattern & thread->waitEventPattern;
	if (matched == 0)
		return 0;
	if (thread->waitEventPatternOutput != NULL)
		*thread->waitEventPatternOutput = matched;
	if (thread->waitEventDataOutput != NULL)
		*thread->waitEventDataOutput = userData;
	wake = wakeupAndDistributeThread(thread, EVENT_WAIT_QUEUE(event), 0);
	if (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
		event->pattern &= ~(matched & ~EVENT_PATTERN_RESERVED);
	return wake;
}

/* SceUIDEventClass's constructor */
static int uidEvent_Constructor(void *object)
{
	__builtin_memset((char *)object + offsetof(ThreadmgrEvent, userData), 0, sizeof(uint64_t));
	return 0;
}

/* the callback of thread's registered on the event is told before the thread wakes */
void notifyCallbackToThreadBeforeSetEvent(ThreadmgrEventCallback *head,
					  struct ThreadCB *thread, uint32_t arg)
{
	struct ThreadmgrCallback *callback;
	ThreadmgrEventCallback *entry;
	SceUID callbackId;

	for (entry = head->next; entry != head; entry = entry->next) {
		if (entry->threadId != thread->threadId)
			continue;
		if (sceGUIDReferObjectWithClass(entry->callbackId,
				s_threadmgrRuntime.manager->callbackClass,
				(SceObjectBase **)&callback) < 0) {
			entry->next->prev = entry->prev;
			entry->prev->next = entry->next;
			deleteRegisteredCallbackList(entry);
		} else {
			SCE_DBG_ASSERT_LEVEL(0, !((entry->notifiedCpus >> sceKernelCpuId()) & 1), 118);
			entry->notifiedCpus |= 1u << sceKernelCpuId();
			callbackId = entry->callbackId;
			callback->cb.notifyId = entry->notifyId;
			callback->cb.notifyCount++;
			callback->cb.notifyArg = arg;
			sceGUIDReleaseObject(callbackId);
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_CALLBACKS);
		}
		break;
	}
}

/*
 * Every callback registered on the event is told, and a thread of theirs
 * waiting with callbacks is woken to run it. Called with the event locked.
 */
uint32_t notifyCallbackAllAfterSetEvent(SceUID eventId, ThreadmgrEvent *event, uint32_t arg)
{
	ThreadmgrEventCallback *head = EVENT_CALLBACK_HEAD(event), *entry, *next;
	struct ThreadCB *thread;
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrCallback *callback;
	struct ThreadmgrWaitNode *self;
	ThreadmgrEvent *waitObject;
	unsigned int cpu = sceKernelCpuId();
	uint32_t mask = 1u << cpu, wake = 0;
	uintptr_t waitable;
	SceUID waitId;
	int ret;

restart:
	for (entry = head->next; entry != head; entry = next) {
		next = entry->next;
		if (entry->notifiedCpus & mask)
			continue;
		if (sceGUIDReferObjectWithClass(entry->callbackId,
				s_threadmgrRuntime.manager->callbackClass,
				(SceObjectBase **)&callback) < 0) {
			entry->next->prev = entry->prev;
			entry->prev->next = entry->next;
			deleteRegisteredCallbackList(entry);
			continue;
		}
		callback->cb.notifyId = entry->notifyId;
		callback->cb.notifyArg = arg;
		callback->cb.notifyCount++;
		if (sceGUIDReferObjectWithClass(callback->cb.threadId,
				s_threadmgrRuntime.manager->threadClass,
				(SceObjectBase **)&object) < 0) {
			entry->next->prev = entry->prev;
			entry->prev->next = entry->next;
			sceGUIDReleaseObject(entry->callbackId);
			deleteRegisteredCallbackList(entry);
			continue;
		}
		thread = &object->context;
		waitObject = NULL;
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (!(thread->status & SCE_KERNEL_THREAD_STATUS_WAITING) ||
		    !(thread->flags & THREAD_FLAG_WAKE_PENDING)) {
			waitId = -1;
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_CALLBACKS);
		} else if (thread->waitType == SCE_KERNEL_WAITTYPE_DELAY) {
			/* a delay: the thread leaves the timed waits */
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_CALLBACKS);
			waitId = -1;
			self = (struct ThreadmgrWaitNode *)thread;
			sceKernelSpinlockLowLock(
				&s_threadmgrRuntime.manager->timedWaitLock);
			self->next->prev = self->prev;
			self->prev->next = self->next;
			sceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->timedWaitLock);
			releaseThreadWaitState(thread, 0, SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
			wake |= distributeThread(selectExecCpu(thread), thread);
		} else {
			waitId = thread->waitId;
			if (waitId == eventId) {
				waitable = EVENT_WAIT_QUEUE(event);
			} else {
				ret = sceGUIDReferObjectWithSubclass(waitId,
					s_threadmgrRuntime.manager->waitableClass,
					(SceObjectBase **)&waitObject);
				if (ret < 0) {
					SCE_DBG_ASSERT_PRINTF(waitId == -1 || waitId == WAIT_ID_MULTIPLE_EVENTS,
						248, "*** error: sceGUIDReferObjectWithSubclass(0x%08X)=0x%08X\n",
						waitId, ret);
					waitable = 0;
				} else if (SceCpuForDriver_5AC9D394(&waitObject->lock) < 0) {
					/* lock order: drop everything and start over */
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					sceKernelSpinlockLowUnlock(&event->lock);
					sceGUIDReleaseObject(waitId);
					sceGUIDReleaseObject(callback->cb.threadId);
					sceGUIDReleaseObject(entry->callbackId);
					sceKernelSpinlockLowLock(&event->lock);
					goto restart;
				} else {
					waitable = EVENT_WAIT_QUEUE(waitObject);
				}
			}
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_CALLBACKS);
			wake |= wakeupAndDistributeThread(thread, waitable, SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
#if !FW_DEBUG
		sceGUIDReleaseObject(callback->cb.threadId);
#endif
		if (waitObject != NULL) {
			wake |= revalidateWaitState(EVENT_WAIT_QUEUE(waitObject));
			sceKernelSpinlockLowUnlock(&waitObject->lock);
		}
#if FW_DEBUG
		sceGUIDReleaseObject(callback->cb.threadId);
#endif
		if (waitObject != NULL)
			sceGUIDReleaseObject(waitId);
		sceGUIDReleaseObject(entry->callbackId);
		SCE_DBG_ASSERT_LEVEL(0, !((entry->notifiedCpus >> cpu) & 1), 280);
		entry->notifiedCpus |= mask;
	}
	for (entry = head->next; entry != head; entry = entry->next) {
		SCE_DBG_ASSERT_LEVEL(0, (entry->notifiedCpus >> cpu) & 1, 290);
		entry->notifiedCpus &= ~mask;
	}
	return wake;
}

/* waits for any of pattern's bits (optionally running callbacks) */
static int waitEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
		     uint64_t *outUserData, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrEvent *event;
	uint64_t wakeup;
	uint32_t saved, matched;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 321, "*** error: illegal context. eventId=0x%08x[%s]\n",
				      eventId, SceSysmemForDriver_E655852F(eventId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 329, "*** error: irq suspended. eventId=0x%08x[%s]\n",
				      eventId, SceSysmemForDriver_E655852F(eventId));
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	thread = currentThreadContext();
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((thread->currentAttr & THREAD_ATTR_USER) && !sceKernelCheckThreadUserStackSelf())
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 355, "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
		goto resume;
	}
	sceKernelSpinlockLowLock(&event->lock);
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&event->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			sceKernelSpinlockLowLock(&event->lock);
		}
		matched = pattern & event->pattern;
		if (matched != 0) {
			/* already there: ret keeps the last result (Sony's) */
			if (outPattern != NULL)
				*outPattern = matched;
			if (outUserData != NULL)
				*outUserData = event->userData;
			if (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
				event->pattern &= ~(pattern & ~EVENT_PATTERN_RESERVED);
			sceKernelSpinlockLowUnlock(&event->lock);
			goto release;
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitEventPattern = pattern;
		thread->waitEventPatternOutput = outPattern;
		thread->waitEventDataOutput = outUserData;
		if (!callbacks) {
			ret = waitWaitableObject(eventId, EVENT_WAIT_QUEUE(event), SCE_KERNEL_WAITTYPE_EVENT,
						 &wakeup, timeout, NULL, &locked);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_WAKE_PENDING);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(eventId, EVENT_WAIT_QUEUE(event), SCE_KERNEL_WAITTYPE_EVENT, &wakeup,
					 timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto callbacks;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	sceKernelSpinlockLowUnlock(&event->lock);
	sceGUIDReleaseObject(eventId);
	if (!callbacks)
		goto resume;
callbacks:
	if (thread->flags & THREAD_FLAG_CALLBACKS) {
		cbRet = callCallbackAll(thread);
		if (cbRet < 0)
			ret = cbRet;
	}
	goto resume;
release:
	sceGUIDReleaseObject(eventId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* an event's information */
static int getEventInfo(SceUID eventId, SceKernelEventInfo *info)
{
	ThreadmgrEvent *event;
	SceClass *cls;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event);
	if (ret < 0) {
		ret = EventUnknownIdError();
	} else {
		ret = GUIDGetClass(eventId, &cls);
		/* delays are not reported */
		if (ret >= 0 && s_threadmgrRuntime.manager->delayClass != cls) {
			info->eventId = eventId;
			strncpy(info->name, SceSysmemForDriver_E655852F(eventId), sizeof(info->name) - 1);
			sceKernelSpinlockLowLock(&event->lock);
			info->attr = event->attr;
			info->numWaitThreads = event->waiting;
			info->eventPattern = event->pattern;
			info->userData = event->userData;
			sceKernelSpinlockLowUnlock(&event->lock);
		}
		sceGUIDReleaseObject(eventId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

_Noreturn void uidEventClassInit(void)
{
	SCE_PANIC(SCE_LINE(3030, 3033));
}

/* a WME node goes back to its process's heap */
int freeEventWaitNode(SceUID pid, ThreadmgrEventWaitNode *node)
{
	memset(node, 0, sizeof(*node));
	return SceSysrootForKernel_7968F091(pid, node);
}

/* SceUIDEventClass's destructor: every waiter wakes with SCE_KERNEL_ERROR_WAIT_DELETE */
static int uidEvent_Destructor(void *object)
{
	ThreadmgrEvent *event = object;
	struct ThreadmgrWaitNode *queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	SceUID waitId;
	uint32_t wake = 0, left;
	int intr;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&event->lock);
	if (event->next == queue) {
		sceKernelSpinlockLowUnlock(&event->lock);
		goto resume;
	}
	for (node = event->next; node != queue; node = next) {
		next = node->next;
		if (EventWaiterIsThread(node)) {
			thread = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 3144);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			if (thread->waitType == SCE_KERNEL_WAITTYPE_EVENT) {
				if (thread->waitEventPatternOutput != NULL)
					*thread->waitEventPatternOutput = EVENT_DELETED_PATTERN;
				if (thread->waitEventDataOutput != NULL)
					*thread->waitEventDataOutput = 0;
			}
			wake |= wakeupAndDistributeThread(thread, (uintptr_t)queue,
							  SCE_KERNEL_ERROR_WAIT_DELETE);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			continue;
		}
		wait = (ThreadmgrEventWaitNode *)node;
		thread = wait->thread;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 3087);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		wme = thread->waitMultipleEvents;
		left = unlinkEventWaitNode(wait, wme);
		waitId = thread->waitId;
		if (!wme->busy && waitId != WAIT_ID_MULTIPLE_EVENTS)
			releaseEventWaiter(event, wait, thread);
		else
			wake |= completeEventWaitNode(event, wait, thread, wme, left, waitId,
				SCE_KERNEL_ERROR_WAIT_DELETE, EVENT_DELETED_PATTERN, 0, 0, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (freeEventWaitNode(thread->processId, wait) < 0)
			SCE_PANIC(SCE_LINE(3135, 3138));
	}
	sceKernelSpinlockLowUnlock(&event->lock);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
	__builtin_memset((char *)object + offsetof(ThreadmgrEvent, userData), 0, sizeof(uint64_t));
	return 0;
}

/*
 * The current thread's WME leaves every event it is still queued on.
 * Called with the thread locked.
 */
void cancelWME(ThreadmgrWaitMultipleEvents *wme)
{
	struct ThreadCB *thread =
		currentThreadContext();
	ListNode *head, *link;
	ThreadmgrEventWaitNode *node;
	ThreadmgrEvent *event;
	SceUID eventId;
	uint32_t i;

	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 2093);
	for (i = 0; i < wme->count; i++) {
		head = &wme->nodes[i];
		while ((link = head->next) != head) {
			node = EVENT_WAIT_NODE_FROM_LINK(link);
			eventId = node->eventId;
			if (sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(),
							    (SceObjectBase **)&event) < 0) {
				SCE_DBG_PRINTF(0, 2107, "   WME cancel: deletetd event(0x%08X)\n",
					       eventId);
				break;
			}
			if (SceCpuForDriver_5AC9D394(&event->lock) >= 0) {
				unlinkEventWaitNode(node, wme);
				releaseEventWaiter(event, node, thread);
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				revalidateWaitState(EVENT_WAIT_QUEUE(event));
				sceKernelSpinlockLowUnlock(&event->lock);
				sceGUIDReleaseObject(node->eventId);
				if (freeEventWaitNode(thread->processId, node) < 0)
					SCE_PANIC(SCE_LINE(2139, 2142));
				sceKernelSpinlockLowLock(THREAD_LOCK(thread));
				break;
			}
			/* lock order: let go of the thread and try again */
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			sceGUIDReleaseObject(eventId);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		}
	}
	wme->busy = 0;
}

void startEvent(void)
{
	struct ThreadmgrWaitableClass *cls;

	cls = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
	if (cls == NULL)
		SCE_PANIC(SCE_LINE(504, 507));
	if (uidRegisterCallbackClassInit(cls, "SceUIDEventClass",
			s_threadmgrRuntime.manager->registerCallbackClass,
			sizeof(ThreadmgrEvent), uidEvent_Constructor, uidEvent_Destructor,
			uidDefaultCancelWait, uidDefault_RevalidateMethod,
			uidDefault_ReleaseMethod) < 0)
		uidEventClassInit();
	s_threadmgrRuntime.manager->eventClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 523, " sizeof(SceUIDEventObject)=%d\n", sizeof(ThreadmgrEvent));
}

/* sceKernelUIDInheritedEventClassInit: a subclass of SceUIDEventClass */
int sceKernelUIDInheritedEventClassInit(struct ThreadmgrWaitableClass *cls, const char *name,
				   SceSize size, SceClassCallback create, SceClassCallback destroy)
{
	int ret;

	ret = uidRegisterCallbackClassInit(cls, name, EVENT_CLASS(), size, create, destroy,
		uidDefaultCancelWait, uidDefault_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		uidEventClassInit();
	return ret;
}

/* a new event object of a subclass gets its attributes (it comes locked) */
int sceKernelEventObjectInit(SceUID eventId, uint32_t attr, uint64_t userData, uint32_t pattern)
{
	ThreadmgrEvent *event;

	if (pattern & EVENT_PATTERN_RESERVED)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	if (sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event) < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 589, "*** error: unknown event id. eventId=0x%08x\n",
				      eventId);
		return SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
	}
	event->attr = attr;
	event->pattern = pattern;
	event->userData = userData;
	sceKernelSpinlockLowUnlock(&event->lock);
	return sceGUIDReleaseObject(eventId);
}

int sceKernelSetEvent(SceUID eventId, uint32_t pattern, uint64_t userData)
{
	struct ThreadmgrWaitNode *queue, *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	SceUID waitId;
	ThreadmgrEvent *event;
	uint32_t saved, wake, matched, left;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 654, "sceKernelSetEvent", "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
		goto resume;
	}
	if (pattern & EVENT_PATTERN_RESERVED) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
		goto release;
	}
	ret = 0;
	if (pattern == 0)
		goto release;
	queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
	sceKernelSpinlockLowLock(&event->lock);
	event->userData = userData;
	event->pattern |= pattern;
	wake = 0;
	for (node = event->next; node != queue; node = next) {
		next = node->next;
		if (EventWaiterIsThread(node)) {
			thread = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK_FUNC(thread->magic == THREAD_CB_MAGIC, 759, "sceKernelSetEvent");
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			wake |= wakeEventWaiter(event, thread, userData);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			continue;
		}
		wait = (ThreadmgrEventWaitNode *)node;
		thread = wait->thread;
		SCE_DBG_ASSERT_CHECK_FUNC(thread->magic == THREAD_CB_MAGIC, 696, "sceKernelSetEvent");
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		wme = thread->waitMultipleEvents;
		matched = event->pattern & wait->pattern;
		if (matched != 0) {
			left = unlinkEventWaitNode(wait, wme);
			waitId = thread->waitId;
			if (!wme->busy && waitId != WAIT_ID_MULTIPLE_EVENTS) {
				releaseEventWaiter(event, wait, thread);
			} else {
				wake |= completeEventWaitNode(event, wait, thread, wme, left, waitId, 0,
							      matched, userData, 0, 0);
				if (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
					event->pattern &= ~(matched & ~EVENT_PATTERN_RESERVED);
			}
			if (freeEventWaitNode(thread->processId, wait) < 0)
				SCE_PANIC_FUNC(SCE_LINE(748, 751), "sceKernelSetEvent");
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlock(&event->lock);
	sceGUIDReleaseObject(eventId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread(1);
	goto resume;
release:
	sceGUIDReleaseObject(eventId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * Sets pattern's bits with the callbacks told (timers and message pipes).
 * Called with the event locked and interrupts off; returns whether a
 * reschedule is due.
 */
uint32_t setEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
				    uint32_t arg, ThreadmgrEvent *event)
{
	struct ThreadmgrWaitNode *queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	SceUID waitId;
	uint32_t wake = 0, matched, left;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 841);
	SCE_DBG_ASSERT_CHECK(event->lock != 0, 844);
	event->userData = userData;
	event->pattern |= pattern;
	for (node = event->next; node != queue; node = next) {
		next = node->next;
		if (EventWaiterIsThread(node)) {
			thread = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 939);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			wake |= wakeEventWaiter(event, thread, userData);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			continue;
		}
		wait = (ThreadmgrEventWaitNode *)node;
		thread = wait->thread;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 868);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		wme = thread->waitMultipleEvents;
		matched = event->pattern & wait->pattern;
		if (matched != 0) {
			left = unlinkEventWaitNode(wait, wme);
			waitId = thread->waitId;
			if (!wme->busy && waitId != WAIT_ID_MULTIPLE_EVENTS) {
				if (releaseEventWaiter(event, wait, thread) < 0)
					SCE_PANIC(SCE_LINE(889, 892));
			} else {
				wake |= completeEventWaitNode(event, wait, thread, wme, left, waitId, 0,
							      matched, userData, 1, arg);
				if (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
					event->pattern &= ~(matched & ~EVENT_PATTERN_RESERVED);
			}
			if (freeEventWaitNode(thread->processId, wait) < 0)
				SCE_PANIC(SCE_LINE(928, 931));
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	return notifyCallbackAllAfterSetEvent(eventId, event, arg) | wake;
}

/* sceKernelSetEventWithNotifyCallback: sceKernelSetEvent with the callbacks told */
int sceKernelSetEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
				   uint32_t arg)
{
	ThreadmgrEvent *event;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event);
	if (ret < 0) {
		ret = EventUnknownIdError();
	} else if (pattern == 0) {
		sceGUIDReleaseObject(eventId);
	} else {
		sceKernelSpinlockLowLock(&event->lock);
		wake = setEventWithNotifyCallback(eventId, pattern, userData, arg, event);
		ret &= (int)wake >> 31;
		sceKernelSpinlockLowUnlock(&event->lock);
		sceGUIDReleaseObject(eventId);
		if ((wake & 1) && currentThreadContext() != NULL)
			switchThread(1);
	}
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* keeps only the bits of keep */
int sceKernelClearEvent(SceUID eventId, uint32_t keep)
{
	ThreadmgrEvent *event;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1078, "sceKernelClearEvent", "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
	} else {
		if (keep & EVENT_PATTERN_RESERVED) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
		} else {
			sceKernelSpinlockLowLock(&event->lock);
			event->pattern &= keep;
			sceKernelSpinlockLowUnlock(&event->lock);
			ret = 0;
		}
		sceGUIDReleaseObject(eventId);
	}
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sets pattern's bits for the waiters there now, then clears them again */
int sceKernelPulseEvent(SceUID eventId, uint32_t pattern, uint64_t userData)
{
	struct ThreadmgrWaitNode *queue, *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	SceUID waitId;
	ThreadmgrEvent *event;
	uint32_t saved, wake, matched, left;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1152, "sceKernelPulseEvent", "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
		goto resume;
	}
	if (pattern & EVENT_PATTERN_RESERVED) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
		goto release;
	}
	ret = 0;
	if (pattern == 0)
		goto release;
	queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
	sceKernelSpinlockLowLock(&event->lock);
	event->userData = userData;
	event->pattern |= pattern;
	wake = 0;
	for (node = event->next; node != queue; node = next) {
		next = node->next;
		if (EventWaiterIsThread(node)) {
			thread = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK_FUNC(thread->magic == THREAD_CB_MAGIC, 1257, "sceKernelPulseEvent");
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			wake |= wakeEventWaiter(event, thread, userData);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			continue;
		}
		wait = (ThreadmgrEventWaitNode *)node;
		thread = wait->thread;
		SCE_DBG_ASSERT_CHECK_FUNC(thread->magic == THREAD_CB_MAGIC, 1194, "sceKernelPulseEvent");
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		wme = thread->waitMultipleEvents;
		matched = event->pattern & wait->pattern;
		if (matched != 0) {
			left = unlinkEventWaitNode(wait, wme);
			waitId = thread->waitId;
			if (!wme->busy && waitId != WAIT_ID_MULTIPLE_EVENTS) {
				releaseEventWaiter(event, wait, thread);
			} else {
				wake |= completeEventWaitNode(event, wait, thread, wme, left, waitId, 0,
							      matched, userData, 0, 0);
				if (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
					event->pattern &= ~(matched & ~EVENT_PATTERN_RESERVED);
			}
			if (freeEventWaitNode(thread->processId, wait) < 0)
				SCE_PANIC_FUNC(SCE_LINE(1246, 1249), "sceKernelPulseEvent");
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	event->pattern &= ~pattern;
	sceKernelSpinlockLowUnlock(&event->lock);
	sceGUIDReleaseObject(eventId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread(1);
	goto resume;
release:
	sceGUIDReleaseObject(eventId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sceKernelPulseEvent with the callbacks told; called like setEventWithNotifyCallback */
uint32_t pulseEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
				      uint32_t arg, ThreadmgrEvent *event)
{
	struct ThreadmgrWaitNode *queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	SceUID waitId;
	uint32_t wake = 0, matched, left;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1343);
	SCE_DBG_ASSERT_CHECK(event->lock != 0, 1346);
	event->userData = userData;
	event->pattern |= pattern;
	for (node = event->next; node != queue; node = next) {
		next = node->next;
		if (EventWaiterIsThread(node)) {
			thread = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1439);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			wake |= wakeEventWaiter(event, thread, userData);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			continue;
		}
		wait = (ThreadmgrEventWaitNode *)node;
		thread = wait->thread;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1370);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		wme = thread->waitMultipleEvents;
		matched = event->pattern & wait->pattern;
		if (matched != 0) {
			left = unlinkEventWaitNode(wait, wme);
			waitId = thread->waitId;
			if (!wme->busy && waitId != WAIT_ID_MULTIPLE_EVENTS) {
				if (releaseEventWaiter(event, wait, thread) < 0)
					SCE_PANIC(SCE_LINE(1391, 1394));
			} else {
				wake |= completeEventWaitNode(event, wait, thread, wme, left, waitId, 0,
							      matched, userData, 1, arg);
				if (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
					event->pattern &= ~(matched & ~EVENT_PATTERN_RESERVED);
			}
			if (freeEventWaitNode(thread->processId, wait) < 0)
				SCE_PANIC(SCE_LINE(1428, 1431));
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	event->pattern &= ~pattern;
	return notifyCallbackAllAfterSetEvent(eventId, event, arg) | wake;
}

int sceKernelPulseEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
					  uint32_t arg)
{
	ThreadmgrEvent *event;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1514, "sceKernelPulseEventWithNotifyCallback", "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
	} else if (pattern == 0) {
		sceGUIDReleaseObject(eventId);
	} else {
		sceKernelSpinlockLowLock(&event->lock);
		wake = pulseEventWithNotifyCallback(eventId, pattern, userData, arg, event);
		ret &= (int)wake >> 31;
		sceKernelSpinlockLowUnlock(&event->lock);
		sceGUIDReleaseObject(eventId);
		if ((wake & 1) && currentThreadContext() != NULL)
			switchThread(1);
	}
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int sceKernelGetEventPattern(SceUID eventId, uint32_t *pattern)
{
	ThreadmgrEvent *event;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (pattern == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1584, "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
	} else {
		ret = 0;
		sceKernelSpinlockLowLock(&event->lock);
		*pattern = event->pattern;
		sceKernelSpinlockLowUnlock(&event->lock);
		sceGUIDReleaseObject(eventId);
	}
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* every waiter wakes with SCE_KERNEL_ERROR_WAIT_CANCEL; the event then holds setPattern */
int sceKernelCancelEventWithSetPattern(SceUID eventId, uint32_t setPattern, uint64_t userData,
				       int *numWaitThreads)
{
	struct ThreadmgrWaitNode *queue, *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	SceUID waitId;
	ThreadmgrEvent *event;
	uint32_t saved, wake, left;
	int intr, ret, count;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1647, "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
		goto resume;
	}
	if (setPattern & EVENT_PATTERN_RESERVED) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
		sceGUIDReleaseObject(eventId);
		goto resume;
	}
	sceKernelSpinlockLowLock(&event->lock);
	queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
	count = 0;
	wake = 0;
	for (node = event->next; node != queue; node = next) {
		next = node->next;
		if (EventWaiterIsThread(node)) {
			thread = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1734);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			if (thread->waitType == SCE_KERNEL_WAITTYPE_EVENT) {
				if (thread->waitEventPatternOutput != NULL)
					*thread->waitEventPatternOutput = EVENT_DELETED_PATTERN;
				if (thread->waitEventDataOutput != NULL)
					*thread->waitEventDataOutput = 0;
				wake |= wakeupAndDistributeThread(thread, (uintptr_t)queue,
								  SCE_KERNEL_ERROR_WAIT_CANCEL);
				count++;
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			continue;
		}
		wait = (ThreadmgrEventWaitNode *)node;
		thread = wait->thread;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1676);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		wme = thread->waitMultipleEvents;
		left = unlinkEventWaitNode(wait, wme);
		waitId = thread->waitId;
		if (!wme->busy && waitId != WAIT_ID_MULTIPLE_EVENTS) {
			releaseEventWaiter(event, wait, thread);
		} else {
			wake |= completeEventWaitNode(event, wait, thread, wme, left, waitId,
				SCE_KERNEL_ERROR_WAIT_CANCEL, EVENT_DELETED_PATTERN, 0, 0, 0);
			count++;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (freeEventWaitNode(thread->processId, wait) < 0)
			SCE_PANIC(SCE_LINE(1726, 1729));
	}
	event->pattern = setPattern;
	event->userData = userData;
	sceKernelSpinlockLowUnlock(&event->lock);
	if (numWaitThreads != NULL)
		*numWaitThreads = count;
	sceGUIDReleaseObject(eventId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int SceThreadmgrForDriver_C529EA32(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			uint64_t *outUserData, SceUInt32 *timeout)
{
	return waitEvent(eventId, pattern, outPattern, outUserData, timeout, 0);
}

int SceThreadmgrForDriver_360C655C(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			  uint64_t *outUserData, SceUInt32 *timeout)
{
	return waitEvent(eventId, pattern, outPattern, outUserData, timeout, 1);
}

int sceKernelPollEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
		       uint64_t *outUserData)
{
	ThreadmgrEvent *event;
	uint32_t saved, matched;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (pattern == 0) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1874, "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
	} else {
		sceKernelSpinlockLowLock(&event->lock);
		matched = event->pattern & pattern;
		if (matched == 0) {
			ret = SCE_KERNEL_ERROR_EVENT_COND;
		} else {
			if (outPattern != NULL)
				*outPattern = matched;
			if (outUserData != NULL)
				*outUserData = event->userData;
			ret = 0;
			if (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
				event->pattern &= ~(pattern & ~EVENT_PATTERN_RESERVED);
		}
		sceKernelSpinlockLowUnlock(&event->lock);
		sceGUIDReleaseObject(eventId);
	}
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* every waiter wakes with SCE_KERNEL_ERROR_WAIT_CANCEL */
int sceKernelCancelEvent(SceUID eventId, int *numWaitThreads)
{
	struct ThreadmgrWaitNode *queue, *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	SceUID waitId;
	ThreadmgrEvent *event;
	uint32_t saved, wake, left;
	int intr, ret, count;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithSubclass(eventId, EVENT_CLASS(), (SceObjectBase **)&event) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1951, "*** error: unknown event id. eventId=0x%08x\n",
					      eventId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
		}
		goto resume;
	}
	queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
	sceKernelSpinlockLowLock(&event->lock);
	count = 0;
	wake = 0;
	for (node = event->next; node != queue; node = next) {
		next = node->next;
		if (EventWaiterIsThread(node)) {
			thread = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 2031);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			if (thread->waitType == SCE_KERNEL_WAITTYPE_EVENT) {
				if (thread->waitEventPatternOutput != NULL)
					*thread->waitEventPatternOutput = EVENT_DELETED_PATTERN;
				if (thread->waitEventDataOutput != NULL)
					*thread->waitEventDataOutput = 0;
				wake |= wakeupAndDistributeThread(thread, (uintptr_t)queue,
								  SCE_KERNEL_ERROR_WAIT_CANCEL);
				count++;
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			continue;
		}
		wait = (ThreadmgrEventWaitNode *)node;
		thread = wait->thread;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1973);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		wme = thread->waitMultipleEvents;
		left = unlinkEventWaitNode(wait, wme);
		waitId = thread->waitId;
		if (!wme->busy && waitId != WAIT_ID_MULTIPLE_EVENTS) {
			releaseEventWaiter(event, wait, thread);
		} else {
			wake |= completeEventWaitNode(event, wait, thread, wme, left, waitId,
				SCE_KERNEL_ERROR_WAIT_CANCEL, EVENT_DELETED_PATTERN, 0, 0, 0);
			count++;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (freeEventWaitNode(thread->processId, wait) < 0)
			SCE_PANIC(SCE_LINE(2023, 2026));
	}
	sceKernelSpinlockLowUnlock(&event->lock);
	if (numWaitThreads != NULL)
		*numWaitThreads = count;
	sceGUIDReleaseObject(eventId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* queues a node for event i of the WME on the event (both locked by the caller as needed) */
static inline void queueEventWaitNode(ThreadmgrEvent *event, ThreadmgrEventWaitNode *node,
				      struct ThreadCB *thread,
				      ThreadmgrWaitMultipleEvents *wme, uint32_t i)
{
	struct ThreadmgrWaitNode *queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
	struct ThreadmgrWaitNode *pos, *self = (struct ThreadmgrWaitNode *)node;
	struct ThreadCB *other;
	ListNode *head = &wme->nodes[i];

	wme->pending++;
	node->wmeLink.next = head;
	node->wmeLink.prev = head->prev;
	head->prev = &node->wmeLink;
	node->wmeLink.prev->next = &node->wmeLink;
	if (event->attr & SCE_KERNEL_ATTR_TH_PRIO) {
		for (pos = event->next; pos != queue; pos = pos->next) {
			other = EventWaiterIsThread(pos) ? (struct ThreadCB *)pos :
				(struct ThreadCB *)(pos->owner & ~3u);
			SCE_DBG_ASSERT_CHECK_FUNC(other->magic == THREAD_CB_MAGIC, 2361,
						  "waitMultipleEvents");
			if (thread->currentPriority < other->currentPriority)
				break;
		}
		self->next = pos;
		self->prev = pos->prev;
		pos->prev = self;
		self->prev->next = self;
	} else {
		self->next = queue;
		self->prev = event->prev;
		event->prev = self;
		self->prev->next = self;
	}
	event->waiting++;
}

/*
 * The current thread waits on the WME's events (optionally running
 * callbacks): until any of them comes (WME_MODE_ANY) or all of them have.
 */
int waitMultipleEvents(ThreadmgrWaitMultipleEvents *wme, SceUInt32 *timeout,
			      int callbacks, uint32_t *outPattern)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;
	const SceKernelWaitEvent *wanted;
	ThreadmgrEventWaitNode *node;
	ThreadmgrEvent *event;
	SceAllocOpt opt;
	uint64_t wakeup;
	uint32_t saved, matched, i;
	bool uninterruptible = false, queueing = true;
	SceUID delayId;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2184, "*** error: illegal context. pWMECB=%p\n", wme);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		sceKernelCpuResumeIntr(intr);
		SCE_DBG_ASSERT_PRINTF(0, 2193, "*** error: irq suspended. pWMECB=%p\n", wme);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto restore;
	}
	if (currentCpuState()->disabledDispatch != 0) {
		sceKernelCpuResumeIntr(intr);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto restore;
	}
	if ((thread->currentAttr & THREAD_ATTR_USER) && !sceKernelCheckThreadUserStackSelf())
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	sceKernelCpuResumeIntr(intr);
	intr = SceCpuForDriver_821FC0EE();
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto resume;
		}
		SCE_DBG_ASSERT_CHECK(wme->pending == 0, 2233);
		wme->busy = 1;
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMultipleEvents = wme;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		/* every event not done with yet: taken now, or a node queued on it */
		for (i = 0; i < wme->count; i++) {
			if (wme->ready[i >> 5] & (1u << (i & 31)))
				continue;
			wanted = &wme->events[i];
			if (sceGUIDReferObjectWithSubclass(wanted->eventId, EVENT_CLASS(),
							    (SceObjectBase **)&event) < 0) {
				if (wme->results != NULL) {
					sceKernelSpinlockLowLock(THREAD_LOCK(thread));
					addEventWaitResult(wme, wanted->eventId,
						SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID, EVENT_DELETED_PATTERN, 0);
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				}
				markEventReady(wme, i);
				if (wme->mode == WME_MODE_ANY)
					queueing = false;
				continue;
			}
			sceKernelSpinlockLowLock(&event->lock);
			if (event->attr & EVENT_ATTR_UNINTERRUPTIBLE)
				uninterruptible = true;
			matched = event->pattern & wanted->eventPattern;
			if (matched == 0) {
				if (queueing) {
					opt.size = sizeof(opt);
					opt.mmapKind = 0;
					opt.align = 4;
					opt.mmapBase = 0;
					opt.mmapSize = 0;
					node = sceKernelSysrootAllocRemoteProcessHeap(thread->processId, sizeof(*node),
									  &opt);
					if (node == NULL) {
						SCE_DBG_ASSERT_LEVEL(0, 0, 2324);
						sceKernelSpinlockLowUnlock(&event->lock);
						sceGUIDReleaseObject(wanted->eventId);
						ret = SCE_KERNEL_ERROR_NO_MEMORY;
						goto resume;
					}
					memset(node, 0, sizeof(*node));
					node->next = (struct ThreadmgrWaitNode *)node;
					node->prev = (struct ThreadmgrWaitNode *)node;
					node->thread = thread;
					node->pattern = wanted->eventPattern;
					node->eventId = wanted->eventId;
					node->index = i;
					node->wmeLink.next = &node->wmeLink;
					node->wmeLink.prev = &node->wmeLink;
					sceKernelSpinlockLowLock(THREAD_LOCK(thread));
					queueEventWaitNode(event, node, thread, wme, i);
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				}
			} else {
				if (wme->results != NULL) {
					sceKernelSpinlockLowLock(THREAD_LOCK(thread));
					addEventWaitResult(wme, wanted->eventId, 0,
							   wanted->eventPattern & event->pattern,
							   event->userData);
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				}
				if (event->attr & SCE_KERNEL_EVENT_ATTR_AUTO_RESET)
					event->pattern &= ~(wanted->eventPattern &
							    ~EVENT_PATTERN_RESERVED);
				markEventReady(wme, i);
				if (wme->mode & WME_MODE_ANY)
					queueing = false;
			}
			sceKernelSpinlockLowUnlock(&event->lock);
			sceGUIDReleaseObject(wanted->eventId);
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if ((thread->flags & THREAD_FLAG_REQUEST_MASK) != 0 &&
		    ((!(thread->flags & THREAD_FLAG_REQUEST_VM_SUSPEND) && uninterruptible) ||
		     ThreadmgrInSyscall())) {
			/* a pending suspend or exit request ends the wait */
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, 0x4000);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, 0x8000);
			switch (thread->flags & THREAD_FLAG_REQUEST_MASK) {
			case THREAD_FLAG_REQUEST_VM_SUSPEND:
			case THREAD_FLAG_REQUEST_SUSPEND:
				ret = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
				break;
			case THREAD_FLAG_REQUEST_EXIT_DELETE:
			case THREAD_FLAG_REQUEST_EXIT:
				ret = SCE_KERNEL_ERROR_THREAD_STOPPED;
				break;
			default:
				SCE_PANIC(SCE_LINE(2407, 2410));
			}
			cancelWME(wme);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto resume;
		}
		if (wme->pending == 0 ||
		    ((wme->mode & WME_MODE_ANY) && wme->pending != wme->count)) {
			/* nothing to wait for any more */
			if (wme->pending != 0)
				cancelWME(wme);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ret = 0;
			goto resume;
		}
		thread->waitEventPatternOutput = outPattern;
		SCE_DBG_ASSERT_CHECK(thread->delayId == -1, 2442);
		SCE_DBG_ASSERT_CHECK(thread->timeoutOutput == 0, 2443);
		if (timeout == NULL) {
			thread->delayId = -1;
			thread->timeoutOutput = 0;
			if ((thread->flags & THREAD_FLAG_CALLBACK_WAKEUP) == THREAD_FLAG_CALLBACK_WAKEUP)
				goto cancelled;
		} else {
			delayId = uidDelay_AllocObject(&wakeup, (uintptr_t)waitTimeoutHandler);
			if (delayId < 0) {
				SCE_DBG_ASSERT_CHECK(delayId == (SceUID)SCE_KERNEL_ERROR_WAIT_TIMEOUT,
						     2449);
				thread->waitResult = delayId;
				cancelWME(wme);
				ret = thread->waitResult;
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				*timeout = 0;
				goto resume;
			}
			thread->delayId = delayId;
			thread->remainingTimeLow = (uint32_t)wakeup;
			thread->remainingTimeHigh = (uint32_t)(wakeup >> 32);
			thread->timeoutOutput = (uintptr_t)timeout;
			if ((thread->flags & THREAD_FLAG_CALLBACK_WAKEUP) == THREAD_FLAG_CALLBACK_WAKEUP) {
				cancelDelayTimer(delayId, NULL);
				thread->delayId = -1;
				thread->timeoutOutput = 0;
				goto cancelled;
			}
		}
		thread->waitType = SCE_KERNEL_WAITTYPE_MP_EVENTS;
		thread->waitId = WAIT_ID_MULTIPLE_EVENTS;
		thread->waitResult = 0;
		SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 2485);
		thread->currentCpuId = -1;
		if (callbacks)
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_WAKE_PENDING);
		if (ThreadmgrInSyscall())
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAIT_MARK);
		self->next = (struct ThreadmgrWaitNode *)wme;
		self->prev = wme->prev;
		thread->status = (thread->status & 0xffff0000) | SCE_KERNEL_THREAD_STATUS_WAITING;
		wme->prev = self;
		self->prev->next = self;
		wme->busy = 0;
		currentCpuState()->nextRunningThread = 0;
		switchThread(2);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		cancelWME(wme);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = thread->waitResult;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		continue;
cancelled:
		thread->waitResult = SCE_KERNEL_ERROR_NOTIFY_CALLBACK;
		cancelWME(wme);
		ret = thread->waitResult;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK && callbacks);
	if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
		i = callCallbackAll(thread);
		if ((int)i < 0)
			ret = i;
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* a WMECB for count events, then the wait; returns the results written or an error */
static inline __attribute__((always_inline)) int waitMultipleEventsWith(
	const SceKernelWaitEvent *events, uint32_t count, uint32_t mode,
	SceKernelResultEvent *results, SceUInt32 *timeout, int callbacks)
{
	ThreadmgrWaitMultipleEvents *wme;
	SceAllocOpt opt;
	uint32_t i;
	int ret;

	if (events == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (count > WME_MAX_EVENTS)
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	if (mode & ~3u)
		return SCE_KERNEL_ERROR_ILLEGAL_MODE;
	opt.size = sizeof(opt);
	opt.mmapKind = 0;
	opt.align = 4;
	opt.mmapBase = 0;
	opt.mmapSize = 0;
	wme = sceKernelSysrootAllocRemoteProcessHeap(SCE_KERNEL_PROCESS_ID, WME_SIZE(count), &opt);
	if (wme == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(wme, 0, offsetof(ThreadmgrWaitMultipleEvents, nodes));
	wme->next = (struct ThreadmgrWaitNode *)wme;
	wme->prev = (struct ThreadmgrWaitNode *)wme;
	for (i = 0; i < count; i++) {
		wme->nodes[i].next = &wme->nodes[i];
		wme->nodes[i].prev = &wme->nodes[i];
	}
	wme->mode = (mode & 1) ? WME_MODE_ANY : WME_MODE_ALL;
	wme->results = results;
	wme->resultCount = 0;
	wme->userEventIds = NULL;
	wme->ready[0] = 0;
	wme->ready[1] = 0;
	wme->count = count;
	wme->events = events;
	ret = waitMultipleEvents(wme, timeout, callbacks, NULL);
	i = wme->resultCount;
	SceSysrootForKernel_7968F091(SCE_KERNEL_PROCESS_ID, wme);
	return ret < 0 ? ret : (int)i;
}

/* SceThreadmgrForDriver_36F8E58A: waits on several events */
int SceThreadmgrForDriver_36F8E58A(const SceKernelWaitEvent *events, uint32_t count,
				   uint32_t mode, SceKernelResultEvent *results,
				   SceUInt32 *timeout)
{
	return waitMultipleEventsWith(events, count, mode, results, timeout, 0);
}

/* SceThreadmgrForDriver_8C48E53B: the same, running callbacks */
int SceThreadmgrForDriver_8C48E53B(const SceKernelWaitEvent *events, uint32_t count,
				   uint32_t mode, SceKernelResultEvent *results,
				   SceUInt32 *timeout)
{
	return waitMultipleEventsWith(events, count, mode, results, timeout, 1);
}

/* SceThreadmgrForDriver_CBA7FAAA: an event's information */
int SceThreadmgrForDriver_CBA7FAAA(SceUID eventId, SceKernelEventInfo *info)
{
	SceKernelEventInfo local;
	uint32_t saved;
	SceSize size;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		ret = getEventInfo(eventId, &local);
		if (ret >= 0) {
			size = info->size;
			if (size > sizeof(local))
				size = sizeof(local);
			memcpy(info, &local, size);
		}
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_995F32E1: the information of every event visible at level */
int SceThreadmgrForDriver_995F32E1(int level, SceKernelEventInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelEventInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = sceGUIDGetUIDVectorByClass(EVENT_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = sceGUIDGetUIDVectorByClass(EVENT_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getEventInfo(vector[i], &info) != 0)
			continue;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/* SceThreadmgrForDriver_7D12344A: the same for a process's event PUIDs */
int SceThreadmgrForDriver_7D12344A(SceUID pid, int level, SceKernelEventInfo *infos,
				   SceSize size, SceSize *count)
{
	SceKernelEventInfo info;
	SceUInt32 attr;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = scePUIDGetUIDVectorByClass(pid, EVENT_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = scePUIDGetUIDVectorByClass(pid, EVENT_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getEventInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.eventId = vector[i];
		if (scePUIDGetAttr(pid, info.eventId, &attr) >= 0 && (attr & PUID_ATTR_OPENED))
			info.attr |= PUID_ATTR_OPENED;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/* a subclass of SceUIDEventClass under parent (CPU timers) */
int uidEventSubclassInit(struct ThreadmgrWaitableClass *cls, const char *name, SceClass *parent,
			 SceSize size, SceClassCallback create, SceClassCallback destroy,
			 ThreadmgrCancelWaitMethod cancelWait, ThreadmgrRevalidateMethod revalidate, ThreadmgrReleaseMethod release)
{
	int ret;

	ret = uidRegisterCallbackClassInit(cls, name, parent, size, create, destroy, cancelWait,
					   revalidate, release);
	if (ret < 0)
		uidEventClassInit();
	return ret;
}
