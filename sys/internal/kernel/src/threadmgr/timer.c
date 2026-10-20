/*
 * SceKernelThreadMgr: timer.c, the CPU timers. One queue, kept by expiry
 * under the manager's timerLock, drives the LT5 hardware timer: its
 * interrupt ends delays (timed waits), calls alarms (kernel callbacks) and
 * sets or pulses the events of timers (counters of their process's time).
 * The user syscall wrappers are still in
 * timer_syscalls.c.
 */

#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>
#include <systimer/timer_regs.h>
#include "scheduler.h"
#include "timer.h"
#include "syscall_context.h"
#include <sysmem/uid_class.h>
#include <sysmem/sysroot.h>

#define TIMER_INTR			0x8d	/* the LT5's interrupt */
#define TIMER_MIN_DELAY			200	/* microseconds */
#define TIMER_ITEM_SIZE			0x80	/* the class's object allocation size */
#define TIMER_ATTR_UNLIMITED		0x00004000
#define TIMER_KERNEL_ATTR_MASK		0x0000a980	/* what kernel callers may pass */
#define TIMER_USER_ATTR_MASK		0x00002980	/* what a syscall may pass */
#define TIMER_OPTION_MAX_SIZE		4
#define TIMER_EVENT_EXPIRED		0x00008000	/* the bit a timer's event gets */
#define TIMER_EVENT_CREATED		0x00000004
#define TIMER_EVENT_DELETED		0x00000008
#define TIMER_EVENT_PROCESS_EXITED	0x00000200
#define SDK_VERSION_DELAY_TIMEOUT	0x020fffff	/* newer SDKs time out a wait already due */
#define SDK_VERSION_DELETE_CHECK	0x03100000	/* from here delete must match create */
#define ALARM_HOOK_CREATE		5
#define ALARM_HOOK_CANCEL		6
#define ALARM_HOOK_CALL			7
#define ALARM_HOOK_END			8

#define TIMER_LOCK()	(&s_threadmgrRuntime.manager->timerLock)
#define TIMER_QUEUE_HEAD()	((struct ThreadmgrTimer *)&s_threadmgrRuntime.manager->timerQueueNext)
#define PROCESS_CLASS()	(s_threadmgrRuntime.manager->processClass)
#define TIMER_CLASS()	(s_threadmgrRuntime.manager->timerClass)

static uint64_t s_suspendedTime;	/* the LT5's count across a suspend */

/* the user data of the bits a thread sets: its process, and its thread ID above */
static inline uint64_t TimerEventCaller(SceUID threadId, SceUID pid)
{
	return (uint64_t)(int64_t)threadId << 32 | (uint64_t)(int64_t)pid;
}

static inline uint64_t ReadSystemTime(void)
{
	volatile SceLongTimerRegisters *time = s_threadmgrRuntime.manager->systemTime;
	uint32_t high, low;

	do {
		high = time->countHi;
		low = time->countLo;
	} while (high != time->countHi);
	return (uint64_t)high << 32 | low;
}

static inline int TimerUnknownIdError(void)
{
	return ThreadmgrInSyscall() ?
		(SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT) :
		SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
}

/* queues a timer behind every timer that expires no later. Called with timerLock held. */
void insertTimer(struct ThreadmgrTimer *timer)
{
	struct ThreadmgrTimer *pos;

	SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->timerLock != 0, 619);
	for (pos = TIMER_QUEUE_HEAD()->next; pos != TIMER_QUEUE_HEAD(); pos = pos->next) {
		SCE_DBG_ASSERT_LEVEL(0, pos != NULL, 623);
		if (timer->expiry < pos->expiry)
			break;
	}
	timer->next = pos;
	timer->prev = pos->prev;
	pos->prev = timer;
	timer->prev->next = timer;
}

/* takes a timer off the queue (moving the interrupt handler's cursor past it) */
static void unlinkTimer(struct ThreadmgrTimer *timer)
{
	struct ThreadmgrTimer *next;

	SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->timerLock != 0, 639);
	next = timer->next;
	if (timer == s_threadmgrRuntime.manager->timerNextToHandle)
		s_threadmgrRuntime.manager->timerNextToHandle = next;
	next->prev = timer->prev;
	timer->prev->next = next;
	timer->next = timer;
	timer->prev = timer;
}

/*
 * Programs the LT5 for the first timer, gathering the timers due within
 * TIMER_MIN_DELAY of it, and never sooner than TIMER_MIN_DELAY from now.
 */
void setNextTimer(void)
{
	struct ThreadmgrTimer *head, *timer;
	uint64_t chosen, limit, earliest;

	SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->timerLock != 0, 548);
	timer = TIMER_QUEUE_HEAD()->next;
	head = TIMER_QUEUE_HEAD();
	if (head == timer) {
		s_threadmgrRuntime.manager->systemTime->compareHi = 0xffffffff;
		s_threadmgrRuntime.manager->systemTime->compareLo = 0xffffffff;
		return;
	}
	chosen = timer->expiry;
	if (s_threadmgrRuntime.manager->timerHandlingId != -1)
		s_threadmgrRuntime.manager->timerNextToHandle = timer;
	limit = chosen + TIMER_MIN_DELAY;
	if (chosen < limit) {
		for (;;) {
			timer = timer->next;
			if (head == timer || timer->expiry >= limit)
				break;
			chosen = timer->expiry;
		}
	}
	earliest = SceSysrootForKernel_E20F6FC8() + TIMER_MIN_DELAY;
	if (chosen >= earliest)
		earliest = chosen;
	s_threadmgrRuntime.manager->timerCompare = earliest;
	s_threadmgrRuntime.manager->systemTime->compare = earliest;
}

static int uidCpuTimer_Constructor(void *object)
{
	memset(&((ThreadmgrCpuTimer *)object)->timer, 0, sizeof(struct ThreadmgrTimer));
	return 0;
}

static int uidDelay_Constructor(void *object)
{
	__builtin_memset((char *)object + offsetof(ThreadmgrDelay, callback), 0,
			 sizeof(ThreadmgrDelay) - offsetof(ThreadmgrDelay, callback));
	return 0;
}

static int uidAlarm_Constructor(void *object)
{
	memset(&((ThreadmgrAlarm *)object)->handler, 0,
	       sizeof(ThreadmgrAlarm) - offsetof(ThreadmgrAlarm, handler));
	return 0;
}

static int uidTimer_Constructor(void *object)
{
	memset(&((ThreadmgrUidTimer *)object)->time, 0,
	       sizeof(ThreadmgrUidTimer) - offsetof(ThreadmgrUidTimer, time));
	return 0;
}

static int uidAlarm_Destructor(void *object)
{
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 3592);
	memset(&((ThreadmgrAlarm *)object)->handler, 0,
	       sizeof(ThreadmgrAlarm) - offsetof(ThreadmgrAlarm, handler));
	return 0;
}

static int uidDelay_Destructor(void *object)
{
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 3541);
	__builtin_memset((char *)object + offsetof(ThreadmgrDelay, callback), 0,
			 sizeof(ThreadmgrDelay) - offsetof(ThreadmgrDelay, callback));
	return 0;
}

static int uidCpuTimer_Destructor(void *object)
{
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 3489);
	memset(&((ThreadmgrCpuTimer *)object)->timer, 0, sizeof(struct ThreadmgrTimer));
	return 0;
}

/* a delay goes back to the delay cache, its GUID kept for reuse */
void uidDelay_FreeObject(ThreadmgrDelay *delay)
{
	SCE_DBG_ASSERT_LEVEL(0, ThreadmgrIntrDisabled(), 3899);
	delay->base.event.next = (struct ThreadmgrWaitNode *)&delay->base.event.next;
	delay->base.event.prev = (struct ThreadmgrWaitNode *)&delay->base.event.next;
	delay->base.event.attr = 0;
	delay->base.event.pattern = 0;
	delay->base.event.waiting = 0;
	memset(&delay->base.timer, 0, sizeof(delay->base.timer));
	delay->callback = NULL;
	delay->thread = NULL;
	sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->delayCacheLock);
	delay->base.event.cacheNext = s_threadmgrRuntime.manager->delayCache;
	s_threadmgrRuntime.manager->delayCacheCount++;
	s_threadmgrRuntime.manager->delayCache = delay;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->delayCacheLock);
}

/*
 * The LT5's interrupt: every timer due is taken off the queue and served
 * (the queue lock is dropped around each, the cursor timerNextToHandle
 * surviving changes to the queue meanwhile).
 */
static int timerIntrHandler(int code, void *arg)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrTimer *head = (struct ThreadmgrTimer *)&manager->timerQueueNext;
	ThreadmgrTraceHook hook;
	struct ThreadmgrTimer *timer;
	ThreadmgrUidTimer *uidTimer;
	ThreadmgrDelay *delay;
	ThreadmgrAlarm *alarm;
	uint64_t now, expiry, interval;
	SceUInt32 next;
	SceClass *cls;
	SceUID uid, pid;
	uint32_t pulse;
	int intr;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(TIMER_LOCK());
	now = SceSysrootForKernel_E20F6FC8();
	timer = head->next;
	while (timer != head) {
		manager->timerNextToHandle = timer->next;
		SCE_DBG_ASSERT_CHECK(timer->deleted == 0, 1018);
		expiry = timer->expiry;
		if (expiry > now)
			break;
		SCE_DBG_ASSERT_CHECK(manager->timerLock != 0, 1028);
		timer->next->prev = timer->prev;
		timer->prev->next = timer->next;
		timer->next = timer;
		timer->prev = timer;
		manager->timerHandlingId = timer->uid;
		manager->timerCpuId = sceKernelCpuId();
		cls = SceSysmemForKernel_368F1C36(timer->uid);
		if (cls == manager->delayClass) {
			/* a timed wait's timeout */
			delay = (ThreadmgrDelay *)CPU_TIMER_FROM_TIMER(timer);
			timer->deleted = 1;
			sceKernelSpinlockLowUnlock(TIMER_LOCK());
			delay->callback(delay->thread);
			uidDelay_FreeObject(delay);
			sceKernelSpinlockLowLock(TIMER_LOCK());
			manager->timerCpuId = -1;
			manager->timerHandlingId = -1;
			timer = manager->timerNextToHandle;
			continue;
		}
		if (cls == manager->alarmClass) {
			alarm = (ThreadmgrAlarm *)CPU_TIMER_FROM_TIMER(timer);
			uid = timer->uid;
			sceKernelSpinlockLowUnlockCpuResumeIntr(TIMER_LOCK(), intr);
			hook = s_threadmgrRuntime.threadTraceHook;
			if (__builtin_expect(hook != NULL, 0))
				hook(ALARM_HOOK_CALL, 3, uid, alarm->handler, alarm->arg);
			next = alarm->handler(alarm->arg);
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(TIMER_LOCK());
			manager->timerCpuId = -1;
			manager->timerHandlingId = -1;
			if (!timer->deleted) {
				if (next == 0) {
					/* the alarm is over */
					timer->deleted = 1;
					sceKernelSpinlockLowUnlock(TIMER_LOCK());
					if (sceGUIDClose(uid) < 0)
						SCE_PANIC(1105);
					hook = s_threadmgrRuntime.threadTraceHook;
					if (__builtin_expect(hook != NULL, 0))
						hook(ALARM_HOOK_END, 1, uid);
					goto relock;
				}
				if (next < TIMER_MIN_DELAY)
					next = TIMER_MIN_DELAY;
				timer->expiry = expiry + next;
				insertTimer(timer);
			}
			sceKernelSpinlockLowUnlock(TIMER_LOCK());
			goto relock;
		}
		if (cls != TIMER_CLASS()) {
			SCE_DBG_PRINTF(0, 1186,
				" error: unexpected object class(UID = 0x%08X, name = %s).\n",
				timer->uid, cls->name);
			SCE_PANIC(SCE_LINE(1179, 1187));
		}
		/* a timer's event time */
		uidTimer = (ThreadmgrUidTimer *)CPU_TIMER_FROM_TIMER(timer);
		pulse = uidTimer->pulse;
		uid = timer->uid;
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
		sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (pulse == 0)
			setEventWithNotifyCallback(uid, TIMER_EVENT_EXPIRED, 0, 0, &uidTimer->base.event);
		else
			pulseEventWithNotifyCallback(uid, TIMER_EVENT_EXPIRED, 0, 0,
						     &uidTimer->base.event);
		sceKernelSpinlockLowLock(TIMER_LOCK());
		manager->timerCpuId = -1;
		manager->timerHandlingId = -1;
		if (timer->deleted) {
			sceKernelSpinlockLowUnlock(TIMER_LOCK());
			sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
			if (sceGUIDClose(uid) < 0)
				SCE_PANIC(SCE_LINE(1172, 1180));
			goto relock;
		}
		if ((uidTimer->flags & TIMER_FLAG_RUNNING) &&
		    (uidTimer->flags & (TIMER_FLAG_REPEAT | TIMER_FLAG_RESCHEDULE))) {
			interval = uidTimer->interval;
			if (interval < TIMER_MIN_DELAY)
				interval = TIMER_MIN_DELAY;
			timer->expiry = expiry + interval;
			if (sceGUIDReferObjectWithClass(timer->pid, PROCESS_CLASS(), NULL) < 0) {
				/* the process is gone: the timer stays off the queue */
				uidTimer->flags = (uidTimer->flags & ~TIMER_FLAG_RESCHEDULE) |
						  TIMER_FLAG_SUSPENDED;
			} else {
				uidTimer->eventTime = timer->expiry - timer->process->time -
						      uidTimer->time;
				pid = timer->pid;
				if (!(uidTimer->flags & TIMER_FLAG_SUSPENDED))
					insertTimer(timer);
#if FW_DEBUG
				uidTimer->flags &= ~TIMER_FLAG_RESCHEDULE;
				sceKernelSpinlockLowUnlock(TIMER_LOCK());
				sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
				if (pid != -1)
					sceGUIDReleaseObject(pid);
				goto relock;
#else
				sceGUIDReleaseObject(pid);
				uidTimer->flags &= ~TIMER_FLAG_RESCHEDULE;
#endif
			}
		}
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
relock:
		sceKernelSpinlockLowLock(TIMER_LOCK());
		timer = manager->timerNextToHandle;
	}
	manager->timerNextToHandle = NULL;
	manager->systemTime->intr = 2;
	setNextTimer();
	sceKernelSpinlockLowUnlockCpuResumeIntr(TIMER_LOCK(), intr);
	return -1;
}

/* SceUIDTimerClass's destructor: off its process's list, the counts down */
static int uidTimer_Destructor(void *object)
{
	ThreadmgrUidTimer *uidTimer = object;
#if FW_DEBUG
	SceProcessObject *process = NULL;
#else
	SceProcessObject *process;
#endif
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 3650);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&uidTimer->base.event.lock);
#if FW_DEBUG
	if (sceGUIDReferObjectWithClass(uidTimer->base.timer.pid, PROCESS_CLASS(),
					 (SceObjectBase **)&process) >= 0) {
#else
	if (sceGUIDReferObjectWithClass(uidTimer->base.timer.pid, PROCESS_CLASS(), NULL) >= 0) {
#endif
		sceKernelSpinlockLowLock(&uidTimer->base.timer.process->timerLock);
		ListLinkRemove(&uidTimer->processLink);
		sceKernelSpinlockLowUnlock(&uidTimer->base.timer.process->timerLock);
#if !FW_DEBUG
		sceGUIDReleaseObject(uidTimer->base.timer.pid);
#endif
	}
	sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
#if FW_DEBUG
	if (process != NULL)
		sceGUIDReleaseObject(uidTimer->base.timer.pid);
#endif
	pid = uidTimer->base.event.ownerPid;
	if (sceGUIDReferObjectWithClass(pid, PROCESS_CLASS(), (SceObjectBase **)&process) >= 0) {
		if (process->pid != SCE_KERNEL_PROCESS_ID) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (uidTimer->base.event.attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (uidTimer->base.event.attr & TIMER_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->timerCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		sceGUIDReleaseObject(pid);
	}
	sceKernelCpuResumeIntr(intr);
	memset(&uidTimer->time, 0, sizeof(ThreadmgrUidTimer) - offsetof(ThreadmgrUidTimer, time));
	return 0;
}

/*
 * Deletes a timer (by its GUID, or by a syscall's PUID, which is closed
 * too), telling its waiters TIMER_EVENT_DELETED.
 */
static int deleteTimer(SceUID timerId, SceUID puid)
{
	struct ThreadCB *thread =
		currentThreadContext();
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint32_t saved, flags, wake;
	SceUID threadId;
	uint64_t caller;
	bool close;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF(0, 1425, "*** error: illegal context. timerId=0x%08x[%s]\n",
				      timerId, SceSysmemForDriver_E655852F(timerId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1440, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	sceKernelSpinlockLowLock(&((ThreadmgrUidTimer *)pObject)->base.event.lock);
	sceKernelSpinlockLowLock(TIMER_LOCK());
	uidTimer = (ThreadmgrUidTimer *)pObject;
	if (uidTimer->base.timer.deleted == 1) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		sceGUIDReleaseObject(timerId);
		goto resume;
	}
	flags = uidTimer->flags;
	close = true;
	uidTimer->base.timer.deleted = 1;
	if ((flags & (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) ==
	    (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) {
		if (!(flags & TIMER_FLAG_SUSPENDED)) {
			if (s_threadmgrRuntime.manager->timerHandlingId == timerId) {
				/* the interrupt handler closes it */
				close = false;
			} else {
				unlinkTimer(&uidTimer->base.timer);
				setNextTimer();
				uidTimer = (ThreadmgrUidTimer *)pObject;
				flags = uidTimer->flags;
			}
		}
		uidTimer->flags = flags & ~(TIMER_FLAG_EVENT | TIMER_FLAG_REPEAT);
	}
	sceKernelSpinlockLowUnlock(TIMER_LOCK());
	if (puid == -1) {
		threadId = thread->threadId;
	} else {
		ret = SceSysmemForDriver_84A4AF5E(0, puid);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 1480);
		threadId = thread->userThreadId;
	}
	caller = TimerEventCaller(threadId, thread->processId);
	if (close && sceGUIDClose(timerId) < 0)
		SCE_PANIC(SCE_LINE(1489, 1490));
	wake = setEventWithNotifyCallback(timerId, TIMER_EVENT_DELETED,
		caller, TIMER_EVENT_DELETED,
		&((ThreadmgrUidTimer *)pObject)->base.event);
	if ((int)wake < 0)
		SCE_PANIC(SCE_LINE(1495, 1496));
	sceKernelSpinlockLowUnlock(&((ThreadmgrUidTimer *)pObject)->base.event.lock);
	sceGUIDReleaseObject(timerId);
	ret = 0;
	if (wake & 1)
		switchThread(0);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * A kernel alarm: handler(arg) runs from the timer interrupt after delay
 * microseconds, and again after each nonzero delay it returns.
 */
static SceUID setAlarmWithName(const char *name, SceUInt64 delay, ThreadmgrAlarmHandler handler,
			       void *arg)
{
	struct ThreadCB *thread =
		currentThreadContext();
	SceProcessObject *process;
	ThreadmgrTraceHook hook;
	ThreadmgrAlarm *alarm;
	SceUID uid;
	int intr;

	if (handler == NULL) {
		SCE_DBG_ASSERT_FAIL(3039);
		return -1;
	}
	if (thread == NULL)
		process = s_threadmgrRuntime.manager->kernelProcess;
	else
		process = thread->process;
	if (name == NULL)
		name = "SceKernelAlarmObject";
	intr = SceCpuForDriver_821FC0EE();
	uid = sceGUIDKernelCreate(s_threadmgrRuntime.manager->alarmClass, name,
				   (SceObjectBase **)&alarm);
	if (uid >= 0) {
		sceKernelSpinlockLowUnlock(&alarm->base.event.lock);
		alarm->base.event.ownerPid = SCE_KERNEL_PROCESS_ID;
		alarm->base.timer.deleted = 0;
		alarm->base.timer.uid = uid;
		alarm->base.timer.expiry = SceSysrootForKernel_E20F6FC8() + delay;
		alarm->base.timer.pid = thread == NULL ? SCE_KERNEL_PROCESS_ID : thread->processId;
		alarm->base.timer.process = process;
		alarm->handler = handler;
		alarm->arg = arg;
		strncpy(alarm->name, name, sizeof(alarm->name));
		sceKernelSpinlockLowLock(TIMER_LOCK());
		insertTimer(&alarm->base.timer);
		setNextTimer();
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
		hook = s_threadmgrRuntime.alarmTraceHook;
		if (hook != NULL)
			hook(ALARM_HOOK_CREATE, (uint32_t)(delay >> 32), (uint32_t)delay, handler, arg,
			     uid);
	}
	sceKernelCpuResumeIntr(intr);
	return uid;
}

/* an alarm's information */
static int getAlarmInfo(SceUID alarmId, ThreadmgrAlarmInfo *info)
{
	ThreadmgrAlarm *alarm;
	SceObjectBase *pObject;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(alarmId, s_threadmgrRuntime.manager->alarmClass,
					 &pObject) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_ALARM_ID;
	} else {
		alarm = (ThreadmgrAlarm *)pObject;
		info->alarmId = alarmId;
		ret = 0;
		strncpy(info->name, alarm->name, sizeof(info->name) - 1);
		sceKernelSpinlockLowLock(TIMER_LOCK());
		info->handler = alarm->handler;
		info->arg = alarm->arg;
		info->expiry = alarm->base.timer.expiry;
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
		sceGUIDReleaseObject(alarmId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* a timer's information */
static int getTimerInfo(SceUID timerId, SceKernelTimerInfo *info)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	SceProcessObject *process;
	uint32_t flags;
	SceUID pid;
	int intr, ret;

	*info = (SceKernelTimerInfo){ .size = sizeof(*info) };
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		ret = TimerUnknownIdError();
		goto resume;
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	info->timerId = timerId;
	strncpy(info->name, SceSysmemForDriver_E655852F(timerId), sizeof(info->name) - 1);
	sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	flags = uidTimer->flags;
	info->numWaitThreads = uidTimer->base.event.waiting;
	info->baseTime.quad = uidTimer->time;
	info->attr = uidTimer->base.event.attr;
	info->fActive = flags & TIMER_FLAG_RUNNING;
	if (flags & TIMER_FLAG_EVENT)
		info->schedule.quad = uidTimer->eventTime;
	info->fRepeat = (flags & TIMER_FLAG_REPEAT) >> 2;
	info->interval.quad = uidTimer->interval;
	info->type = uidTimer->pulse;
	pid = uidTimer->base.timer.pid;
	process = uidTimer->base.timer.process;
	sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	sceGUIDReleaseObject(timerId);
	if (sceGUIDReferObjectWithClass(pid, PROCESS_CLASS(), NULL) >= 0) {
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			info->currentTime.quad = SceSysrootForKernel_E20F6FC8() - process->time -
					info->baseTime.quad;
		else
			info->currentTime = info->baseTime;
		sceGUIDReleaseObject(pid);
	}
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

int uidCpuTimerClassInit(struct ThreadmgrWaitableClass *cls, const char *name, SceClass *parent,
			 SceSize size, SceClassCallback create, SceClassCallback destroy)
{
	int ret;

	ret = uidEventSubclassInit(cls, name, parent, size, create, destroy,
		uidDefaultCancelWait, uidDefault_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC(SCE_LINE(3493, 3462));
	return ret;
}

/* SceThreadmgrForKernel_ABBE9BA5: the hook told of every new alarm */
void SceThreadmgrForKernel_ABBE9BA5(ThreadmgrTraceHook hook)
{
	s_threadmgrRuntime.alarmTraceHook = hook;
}

/* SceThreadmgrForDriver_419E6736: the system time */
int SceThreadmgrForDriver_419E6736(SceUInt64 *time)
{
	if (time == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	*time = ReadSystemTime();
	return 0;
}

SceInt64 sceKernelGetSystemTimeWide(void)
{
	return ReadSystemTime();
}

SceUInt32 SceThreadmgrForDriver_47F6DE49(void)
{
	return s_threadmgrRuntime.manager->systemTime->countLo;
}

static inline void uidDelayClassInit(struct ThreadmgrWaitableClass *cls)
{
	if (uidCpuTimerClassInit(cls, "SceUIDDelayClass",
			s_threadmgrRuntime.manager->cpuTimerClass, TIMER_ITEM_SIZE,
			uidDelay_Constructor, uidDelay_Destructor) < 0)
		SCE_PANIC_FUNC(SCE_LINE(3544, 3513), "uidDelayClassInit");
}

static inline void uidAlarmClassInit(struct ThreadmgrWaitableClass *cls)
{
	if (uidCpuTimerClassInit(cls, "SceUIDAlarmClass",
			s_threadmgrRuntime.manager->cpuTimerClass, TIMER_ITEM_SIZE,
			uidAlarm_Constructor, uidAlarm_Destructor) < 0)
		SCE_PANIC_FUNC(SCE_LINE(3596, 3565), "uidAlarmClassInit");
}

static inline void uidTimerClassInit(struct ThreadmgrWaitableClass *cls)
{
	if (uidCpuTimerClassInit(cls, "SceUIDTimerClass",
			s_threadmgrRuntime.manager->cpuTimerClass, TIMER_ITEM_SIZE,
			uidTimer_Constructor, uidTimer_Destructor) < 0)
		SCE_PANIC_FUNC(SCE_LINE(3656, 3616), "uidTimerClassInit");
}

#define LT5_MEMBLOCK_TYPE		0x20100806	/* kernel device, uncached */

/*
 * Each CPU calls it at boot: CPU 0 maps the LT5, takes its interrupt and
 * makes the CPU timer classes; then every CPU (only CPU 0 does anything)
 * enables the interrupt.
 */
void startCpuTimer(struct ThreadMgrCpuCB *cpu)
{
	struct SceKernelThreadmgrPrivate *manager;
	SceKernelAllocMemBlockKernelOpt opt;
	struct ThreadmgrWaitableClass *cls;
	SceUID uid;
	int ret;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.paddr = TIMER_LONG_REG_BASE(5);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
		uid = sceKernelAllocMemBlockForKernel("SceLT5", LT5_MEMBLOCK_TYPE, TIMER_REG_SIZE, &opt);
		manager = s_threadmgrRuntime.manager;
		if (uid < 0) {
			SCE_DBG_PRINTF(0xb, 331, "memBlockId 0x%08x\n", uid);
			SCE_PANIC(332);
		}
		manager->timerMemBlockId = uid;
		sceKernelGetMemBlockBase(uid, (void **)&manager->systemTime);
		SceSysrootForKernel_5B5EBFB1(s_threadmgrRuntime.manager->systemTime);
		ret = sceKernelRegisterIntrHandler(TIMER_INTR, "SceThreadmgrTimer", 0,
			timerIntrHandler, (void *)s_threadmgrRuntime.manager, 0x30, 0xf, NULL);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 344, "error: register timer handler.(0x%08X)\n", ret);
			SCE_PANIC(345);
		}
		cls = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
		if (cls == NULL)
			SCE_PANIC(351);
		if (uidCpuTimerClassInit(cls, "SceUIDCpuTimerClass",
				s_threadmgrRuntime.manager->eventClass,
				sizeof(ThreadmgrCpuTimer), uidCpuTimer_Constructor,
				uidCpuTimer_Destructor) < 0)
			SCE_PANIC(361);
		manager = s_threadmgrRuntime.manager;
		manager->cpuTimerClass = (SceClass *)cls;
		cls = sceKernelAllocHeapMemory(manager->heapId, sizeof(*cls));
		if (cls == NULL)
			SCE_PANIC(367);
		uidDelayClassInit(cls);
		s_threadmgrRuntime.manager->delayClass = (SceClass *)cls;
		SCE_DBG_PRINTF_LEVEL(2, 0, 380, " sizeof(SceUIDDelayObject)=%d\n",
				     sizeof(ThreadmgrDelay));
		cls = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
		if (cls == NULL)
			SCE_PANIC(385);
		uidAlarmClassInit(cls);
		s_threadmgrRuntime.manager->alarmClass = (SceClass *)cls;
		SCE_DBG_PRINTF_LEVEL(2, 0, 398, " sizeof(SceUIDAlarmObject)=%d\n",
				     sizeof(ThreadmgrAlarm));
		cls = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
		if (cls == NULL)
			SCE_PANIC(403);
		uidTimerClassInit(cls);
		s_threadmgrRuntime.manager->timerClass = (SceClass *)cls;
		SCE_DBG_PRINTF_LEVEL(2, 0, 416, " sizeof(SceUIDTimerObject)=%d\n",
				     sizeof(ThreadmgrUidTimer));
		manager = s_threadmgrRuntime.manager;
		manager->timerQueueNext = (struct ThreadmgrTimer *)&manager->timerQueueNext;
		manager->timerQueuePrev = (struct ThreadmgrTimer *)&manager->timerQueueNext;
		manager->timerCompare = 0;
		manager->timerLock = 0;
		manager->timerHandlingId = -1;
		manager->timerNextToHandle = NULL;
		manager->timerCpuId = -1;
	}
	sceKernelSysrootCorelockUnlock();
	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		cpu->lastThreadSwitchTime = 0;
		ret = sceKernelEnableIntr(TIMER_INTR);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 439, "error: enable timer intr.(0x%08X)\n", ret);
			SCE_PANIC(440);
		}
	}
	sceKernelSysrootCorelockUnlock();
}

/* PLACEHOLDER: name; before a suspend: the LT5 stops, its count is kept */
void suspendCpuTimer(void)
{
	struct SceKernelThreadmgrPrivate *manager;
	uint64_t compare;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		s_suspendedTime = SceSysrootForKernel_E20F6FC8();
		sceKernelDisableIntr(TIMER_INTR);
		manager = s_threadmgrRuntime.manager;
		compare = manager->timerCompare;
		/* resume just before the next expiry, so it is not lost */
		if (compare - s_suspendedTime - 1 > TIMER_MIN_DELAY - 1)
			s_suspendedTime = compare - 1;
	}
	sceKernelSysrootCorelockUnlock();
}

/* PLACEHOLDER: name; after a resume: the LT5 counts on from the kept count */
void resumeCpuTimer(void)
{
	volatile SceLongTimerRegisters *lt5;
	uint32_t control;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		lt5 = s_threadmgrRuntime.manager->systemTime;
		control = lt5->control;
		lt5->control = 0;
		lt5->countLo = (uint32_t)s_suspendedTime;
		lt5->countHi = (uint32_t)(s_suspendedTime >> 32);
		lt5->compareLo = (uint32_t)s_threadmgrRuntime.manager->timerCompare;
		lt5->compareHi = (uint32_t)(s_threadmgrRuntime.manager->timerCompare >> 32);
		lt5->control = control;
		sceKernelEnableIntr(TIMER_INTR);
	}
	sceKernelSysrootCorelockUnlock();
}

/*
 * The process time a wait of timeout microseconds ends at; a syscall
 * restarted after a suspend goes on with the time it had left.
 */
void getWakeupTime(uint64_t *wakeup, uint32_t timeout)
{
	struct ThreadCB *thread =
		currentThreadContext();
	uint64_t now, delay;

	SCE_DBG_ASSERT_CHECK(thread != NULL, 520);
	if ((thread->flags & THREAD_FLAG_RESUMED_WAIT) && ThreadmgrInSyscall()) {
		now = SceSysrootForKernel_E20F6FC8();
		delay = (uint64_t)thread->remainingTimeHigh << 32 | thread->remainingTimeLow;
	} else {
		now = SceSysrootForKernel_E20F6FC8();
		delay = timeout;
	}
	*wakeup = now + (delay - thread->process->time);
}

/* the delay object gets its timer and goes on the queue */
static inline __attribute__((always_inline)) SceUID setDelayTimer(SceUID uid,
	ThreadmgrDelay *delay, const uint64_t *wakeup, uintptr_t callback,
	struct ThreadCB *thread, SceProcessObject *process)
{
	if (uid < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 698, "setDelayTimer",
				    "error: can not create delay object.(0x%08X)\n", uid);
		SCE_DBG_ASSERT_LEVEL_FUNC(0, 0, 699, "setDelayTimer");
		return uid;
	}
	SCE_DBG_ASSERT_CHECK_FUNC(((uintptr_t)delay & 7) == 0, 717, "setDelayTimer");
	delay->base.timer.deleted = 0;
	delay->base.timer.uid = uid;
	delay->base.timer.expiry = *wakeup + process->time;
	delay->callback = (void (*)(struct ThreadCB *))callback;
	delay->thread = thread;
	delay->base.timer.pid = thread->processId;
	delay->base.timer.process = process;
	sceKernelSpinlockLowLock(TIMER_LOCK());
	insertTimer(&delay->base.timer);
	setNextTimer();
	sceKernelSpinlockLowUnlock(TIMER_LOCK());
	return uid;
}

/*
 * A delay for the current thread ending at wakeup (process time), from the
 * delay cache or new; callback(thread) runs when it expires. A wakeup
 * already past is SCE_KERNEL_ERROR_WAIT_TIMEOUT (kernel and newer SDK
 * callers) or a timer due now (older SDKs, at exactly the time).
 */
int uidDelay_AllocObject(const uint64_t *wakeup, uintptr_t callback)
{
	struct ThreadCB *thread =
		currentThreadContext();
	SceProcessObject *process = thread->process;
	struct SceKernelThreadmgrPrivate *manager;
	ThreadmgrDelay *delay;
	SceUInt32 version;
	uint64_t now;
	SceUID uid;
	int ret;

	now = SceSysrootForKernel_E20F6FC8() - process->time;
	if (*wakeup <= now) {
		if (thread->processId == SCE_KERNEL_PROCESS_ID)
			return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
		if (SceSysrootForDriver_CAE820B2(thread->processId, &version) < 0)
			version = 0;
		else if (version > SDK_VERSION_DELAY_TIMEOUT)
			return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
		if (now > *wakeup)
			return SCE_KERNEL_ERROR_WAIT_TIMEOUT;
	}
	SCE_DBG_ASSERT_LEVEL(0, ThreadmgrIntrDisabled(), 3832);
	manager = s_threadmgrRuntime.manager;
	sceKernelSpinlockLowLock(&manager->delayCacheLock);
	if (manager->delayCacheCount < 1) {
		sceKernelSpinlockLowUnlock(&manager->delayCacheLock);
		uid = sceGUIDKernelCreate(s_threadmgrRuntime.manager->delayClass,
					   "SceKernelDelayObject", (SceObjectBase **)&delay);
		if (uid < 0) {
			SCE_DBG_PRINTF(0, 3859,
				"*** error: uidDelay_AllocObject() -> sceGUIDKernelCreate()=0x%08X\n", uid);
		} else {
			sceKernelSpinlockLowUnlock(&delay->base.event.lock);
			delay->base.event.ownerPid = SCE_KERNEL_PROCESS_ID;
			ret = sceGUIDGetEntry(uid, &delay->base.event.uidEntry);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 3871,
					"*** error: uidDelay_AllocObject() -> sceGUIDGetEntry(0x%08X)=0x%08X\n",
					uid, ret);
				if (sceGUIDClose(uid) < 0)
					SCE_PANIC(SCE_LINE(3913, 3873));
				uid = ret;
			} else {
				SCE_DBG_ASSERT_CHECK(delay->base.event.uidEntry != NULL, 3878);
			}
		}
	} else {
		delay = manager->delayCache;
		manager->delayCacheCount--;
		manager->delayCache = delay->base.event.cacheNext;
		sceKernelSpinlockLowUnlock(&manager->delayCacheLock);
		delay->base.event.cacheNext = &delay->base.event.uidEntry;
		uid = SceSysmemForKernel_03A2CAF0(delay->base.event.uidEntry);
	}
	return setDelayTimer(uid, delay, wakeup, callback, thread, process);
}

/*
 * A wait ended before its delay: the delay goes back to the cache. With
 * a thread, the time it had left is kept (and copied to the wait's
 * timeout argument).
 */
void cancelDelayTimer(SceUID delayId, struct ThreadCB *thread)
{
	ThreadmgrDelay *delay;
	SceObjectBase *pObject;
	SceUInt32 *timeout;
	SceUInt32 remaining;
	uint64_t now;
	int ret;

	ret = sceGUIDReferObjectWithClass(delayId, s_threadmgrRuntime.manager->delayClass,
					   &pObject);
	if (ret < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 764, "error: sceGUIDReferObjectWithClass(0x%08X)=0x%08X\n",
				      delayId, ret);
		return;
	}
	delay = (ThreadmgrDelay *)pObject;
	sceKernelSpinlockLowLock(TIMER_LOCK());
	if (thread != NULL) {
		now = SceSysrootForKernel_E20F6FC8();
		timeout = (SceUInt32 *)thread->timeoutOutput;
		if (now <= delay->base.timer.expiry)
			remaining = (uint32_t)delay->base.timer.expiry - (uint32_t)now;
		else
			remaining = 0;
		thread->remainingTimeLow = remaining;
		thread->remainingTimeHigh = 0;
		if (timeout != NULL) {
			if (thread->waitType == SCE_KERNEL_WAITTYPE_MSG_PIPE &&
			    thread->timeoutProcessId != SCE_KERNEL_PROCESS_ID) {
				ret = sceKernelCopyToUserProc(thread->timeoutProcessId, timeout,
							       &remaining, sizeof(remaining));
				if (ret < 0)
					SCE_DBG_PRINTF(0, 795,
						"warning: cancelDelayTimer(): sceKernelCopyToUserProc()=0x%08X\n",
						ret);
			} else {
				*timeout = remaining;
			}
		}
	}
	if (delay->base.timer.deleted == 0) {
		delay->base.timer.deleted = 1;
		unlinkTimer(&delay->base.timer);
		setNextTimer();
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
		uidDelay_FreeObject((ThreadmgrDelay *)pObject);
	} else {
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
	}
	sceGUIDReleaseObject(delayId);
}

/*
 * A process exits: its timers are deleted (those it created) and its
 * timer PUIDs closed, each event told TIMER_EVENT_PROCESS_EXITED.
 */
int timerProcessExit(SceProcessObject *process)
{
	SceSize got = 0;
	ThreadmgrUidTimer *uidTimer;
	SceUID *vector, guid, pid;
	uint32_t flags;
	bool close;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid, TIMER_CLASS(), PUID_VECTOR_LEVEL, NULL, 0,
					  NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 866);
		num = scePUIDGetUIDVectorByClass(process->pid, TIMER_CLASS(), 5, vector, num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < num; i++) {
		guid = SceSysmemForDriver_45D22597(process->pid, vector[i]);
		intr = SceCpuForDriver_821FC0EE();
		ret = SceSysmemForKernel_9C23DFF7(process->pid, vector[i], TIMER_CLASS(),
						  (SceObjectBase **)&uidTimer);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 887,
				" k: timerProcessExit()->scePUIDReferObjectWithClass(0x%08X)=0x%08X\n",
				vector[i], ret);
			SCE_PANIC(888);
		}
		sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		pid = process->pid;
		if (uidTimer->base.timer.deleted == 0 && uidTimer->base.timer.pid == pid) {
			flags = uidTimer->flags;
			close = true;
			uidTimer->base.timer.deleted = 1;
			if ((flags & (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) ==
			    (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) {
				if (!(flags & TIMER_FLAG_SUSPENDED)) {
					sceKernelSpinlockLowLock(TIMER_LOCK());
					if (s_threadmgrRuntime.manager->timerHandlingId != guid) {
						unlinkTimer(&uidTimer->base.timer);
						setNextTimer();
					} else {
						/* the interrupt handler closes it */
						close = false;
					}
					sceKernelSpinlockLowUnlock(TIMER_LOCK());
					flags = uidTimer->flags;
				}
				uidTimer->flags = flags & ~(TIMER_FLAG_EVENT | TIMER_FLAG_REPEAT);
			}
			if (close && sceGUIDClose(guid) < 0) {
				SCE_DBG_PRINTF(0, 921, "\n");
				SCE_PANIC(922);
			}
			if (uidTimer->base.timer.process == process) {
				sceKernelSpinlockLowLock(&process->timerLock);
				ListLinkRemove(&uidTimer->processLink);
				sceKernelSpinlockLowUnlock(&process->timerLock);
			}
			pid = process->pid;
		}
		ret = SceSysmemForDriver_84A4AF5E(pid, vector[i]);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 937, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(938);
		}
		if ((int)setEventWithNotifyCallback(guid, TIMER_EVENT_PROCESS_EXITED,
				(uint64_t)(int64_t)process->pid, TIMER_EVENT_PROCESS_EXITED,
				&uidTimer->base.event) < 0)
			SCE_PANIC(947);
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		if (UIDEntryReleaseObject(process->pid, vector[i]) < 0)
			SCE_PANIC(955);
		sceKernelCpuResumeIntr(intr);
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 963);
		return ret;
	}
	return 0;
}

/*
 * The early errors return with TPIDRURO still shifted, as Sony's code
 * does (see sceKernelCreateSema).
 */
SceUID sceKernelCreateTimer(const char *name, SceUInt32 attr,
			    const SceKernelTimerOptParam *option)
{
	struct ThreadCB *thread =
		currentThreadContext();
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint32_t saved;
	SceUID uid, threadId;
	bool syscall;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ThreadmgrNoContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 1244, "*** error: illegal context\n");
		uid = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (ThreadmgrInSyscall()) {
		if (attr & ~TIMER_USER_ATTR_MASK) {
			SCE_DBG_PRINTF(0, 1258, " k: error: sceKernelCreateTimer(%s), attr=0x%08X\n",
				       name, attr);
			uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (attr & ~TIMER_KERNEL_ATTR_MASK) {
		SCE_DBG_PRINTF(0, 1271, " k: error: sceKernelCreateTimer(%s), attr=0x%08X\n", name,
			       attr);
		uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > TIMER_OPTION_MAX_SIZE) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	if (attr & SCE_KERNEL_ATTR_OPENABLE) {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.attr = UID_ATTR_OPENABLE;
	} else {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		if (ThreadmgrInSyscall()) {
			opt.pid = thread->processId;
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
			if (thread->currentAttr & THREAD_ATTR_UNLIMITED)
				attr |= TIMER_ATTR_UNLIMITED;
			opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED) != 0;
		}
	}
#if FW_DEBUG
	if (!ThreadmgrInSyscall()) {
		intr = SceCpuForDriver_821FC0EE();
	} else {
		process = thread->process;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
#else
	intr = SceCpuForDriver_821FC0EE();
	if (ThreadmgrInSyscall()) {
		process = thread->process;
		sceKernelSpinlockLowLock(&process->resourceLock);
#endif
		if (attr & SCE_KERNEL_ATTR_OPENABLE) {
			if (process->uidOpenableCount >= UID_OPENABLE_MAX_PER_PROCESS) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF(0, 1317, "sceKernelCreateTimer(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidOpenableCount > process->uidOpenablePeak)
				process->uidOpenablePeak = process->uidOpenableCount;
		} else if (attr & TIMER_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= UID_MAX_PER_PROCESS) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_PRINTF(0xb, 1334,
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->timerCount > process->timerPeak)
			process->timerPeak = process->timerCount;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = sceGUIDKernelCreateWithOpt(TIMER_CLASS(), name, &opt,
					  &pObject);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (attr & TIMER_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->timerCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		goto resume;
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	syscall = ThreadmgrInSyscall();
	process = thread->process;
	uidTimer->base.timer.process = process;
	uidTimer->base.event.ownerPid = syscall ? thread->processId : SCE_KERNEL_PROCESS_ID;
	uidTimer->base.timer.uid = uid;
	uidTimer->base.timer.pid = thread->processId;
	uidTimer->base.timer.deleted = 0;
	sceKernelSpinlockLowLock(&process->timerLock);
	process = uidTimer->base.timer.process;
	if ((process->status & 0xff) != PROCESS_STATE_RUNNING)
		uidTimer->flags |= TIMER_FLAG_SUSPENDED;
	uidTimer->processLink.next = &process->timers;
	uidTimer->processLink.prev = process->timers.prev;
	process->timers.prev = &uidTimer->processLink;
	uidTimer->processLink.prev->next = &uidTimer->processLink;
	sceKernelSpinlockLowUnlock(&process->timerLock);
	threadId = ThreadmgrInSyscall() ? thread->userThreadId : thread->threadId;
	/* sceKernelEventObjectInit unlocks the object the class hands over locked */
	ret = sceKernelEventObjectInit(uid, attr, TimerEventCaller(threadId, thread->processId),
				       TIMER_EVENT_CREATED);
	SCE_DBG_ASSERT_CHECK(ret >= 0, 1383);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

/* ksceKernelDeleteTimer */
int SceThreadmgrForDriver_91A8F38B(SceUID timerId)
{
	return deleteTimer(timerId, -1);
}

/* the syscall: from SDK 0x03100000 a timer opened (PUID attr 0x80000) must be closed instead */
int sceKernelDeleteTimerForUser(SceUID puid)
{
	SceUInt32 attr, version;
	SceUID guid;

	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid >= 0 && scePUIDGetAttr(0, puid, &attr) >= 0) {
		if (!(attr & PUID_ATTR_OPENED))
			return deleteTimer(guid, puid);
		if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
			version = SDK_VERSION_DELETE_CHECK;
		else if (version < SDK_VERSION_DELETE_CHECK)
			return deleteTimer(guid, puid);
		SCE_DBG_PRINTF(0, 1564,
			"*** error: sceKernelDeleteTimerForUser(0x%08X), delete/close mismatch\n", puid);
	}
	return SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
}

/* interrupts off and the timer queue locked (one call in Sony's debug builds) */
static inline int lockTimerQueueCpuSuspendIntr(void)
{
#if FW_DEBUG
	return sceKernelSpinlockLowLockCpuSuspendIntr(TIMER_LOCK());
#else
	int intr = SceCpuForDriver_821FC0EE();

	sceKernelSpinlockLowLock(TIMER_LOCK());
	return intr;
#endif
}

/* starts counting; returns 1 if it already was */
int sceKernelStartTimer(SceUID timerId)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint32_t saved;
	uint64_t now;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = lockTimerQueueCpuSuspendIntr();
	if (s_threadmgrRuntime.manager->timerHandlingId == timerId && s_threadmgrRuntime.manager->timerCpuId == (int)sceKernelCpuId()) {
		ret = SCE_KERNEL_ERROR_INVALID_UID;
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
		goto resume;
	}
	sceKernelSpinlockLowUnlock(TIMER_LOCK());
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1620, "sceKernelStartTimer", "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	if (uidTimer->flags & TIMER_FLAG_RUNNING) {
		ret = 1;
	} else {
		uidTimer->flags |= TIMER_FLAG_RUNNING;
		now = SceSysrootForKernel_E20F6FC8();
		if (uidTimer->flags & TIMER_FLAG_EVENT) {
			sceKernelSpinlockLowLock(TIMER_LOCK());
			uidTimer->base.timer.expiry = now + (uidTimer->eventTime - uidTimer->time);
			if (!(uidTimer->flags & TIMER_FLAG_SUSPENDED)) {
				insertTimer(&uidTimer->base.timer);
				setNextTimer();
			}
			sceKernelSpinlockLowUnlock(TIMER_LOCK());
		}
		ret = 0;
		/* from now on time is where the count starts from */
		uidTimer->time = now - uidTimer->base.timer.process->time - uidTimer->time;
	}
	sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	sceGUIDReleaseObject(timerId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* stops counting; returns 1 if it was counting */
int sceKernelStopTimer(SceUID timerId)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint32_t saved;
	uint64_t now;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = lockTimerQueueCpuSuspendIntr();
	if (s_threadmgrRuntime.manager->timerHandlingId == timerId && s_threadmgrRuntime.manager->timerCpuId == (int)sceKernelCpuId()) {
		ret = SCE_KERNEL_ERROR_INVALID_UID;
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
		goto resume;
	}
	sceKernelSpinlockLowUnlock(TIMER_LOCK());
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1703, "sceKernelStopTimer", "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	ret = 0;
	if (uidTimer->flags & TIMER_FLAG_RUNNING) {
		uidTimer->flags &= ~TIMER_FLAG_RUNNING;
		now = SceSysrootForKernel_E20F6FC8();
		sceKernelSpinlockLowLock(TIMER_LOCK());
		/* from now on time is the count */
		uidTimer->time = now - uidTimer->base.timer.process->time - uidTimer->time;
		if ((uidTimer->flags & (TIMER_FLAG_EVENT | TIMER_FLAG_SUSPENDED)) == TIMER_FLAG_EVENT &&
		    s_threadmgrRuntime.manager->timerHandlingId != timerId) {
			unlinkTimer(&uidTimer->base.timer);
			setNextTimer();
		}
		ret = 1;
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
	}
	sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	sceGUIDReleaseObject(timerId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* where a running timer counts from (0 when stopped) */
int sceKernelGetTimerBase(SceUID timerId, SceUInt64 *base)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (base == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1780, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
	} else {
		uidTimer = (ThreadmgrUidTimer *)pObject;
		sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			*base = uidTimer->time;
		else
			*base = 0;
		ret = 0;
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		sceGUIDReleaseObject(timerId);
	}
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

SceUInt64 SceThreadmgrForDriver_A6D11DD3(SceUID timerId)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	SceUInt64 base = (SceUInt64)-1;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) >= 0) {
		uidTimer = (ThreadmgrUidTimer *)pObject;
		sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			base = uidTimer->time;
		else
			base = 0;
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		sceGUIDReleaseObject(timerId);
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return base;
}

/* the count */
int sceKernelGetTimerTime(SceUID timerId, SceUInt64 *time)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (time == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1896, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
	} else {
		uidTimer = (ThreadmgrUidTimer *)pObject;
		sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			*time = SceSysrootForKernel_E20F6FC8() -
				uidTimer->base.timer.process->time - uidTimer->time;
		else
			*time = uidTimer->time;
		ret = 0;
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		sceGUIDReleaseObject(timerId);
	}
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

SceUInt64 SceThreadmgrForDriver_C1286004(SceUID timerId)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	SceUInt64 time;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		time = (SceUInt64)-1;
	} else {
		uidTimer = (ThreadmgrUidTimer *)pObject;
		sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			time = SceSysrootForKernel_E20F6FC8() -
			       uidTimer->base.timer.process->time - uidTimer->time;
		else
			time = uidTimer->time;
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		sceGUIDReleaseObject(timerId);
	}
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return time;
}

/* sets the count to *time, returning the old count there */
int sceKernelSetTimerTime(SceUID timerId, SceUInt64 *time)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint64_t now, elapsed, old, previous;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (time == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF(0, 2008, "*** error: illegal context. timerId=0x%08x[%s]\n",
				      timerId, SceSysmemForDriver_E655852F(timerId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 2022, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	if (uidTimer->flags & TIMER_FLAG_RUNNING) {
		now = SceSysrootForKernel_E20F6FC8();
		sceKernelSpinlockLowLock(TIMER_LOCK());
		old = uidTimer->time;
		elapsed = now - uidTimer->base.timer.process->time;
		uidTimer->time = elapsed - *time;
		previous = elapsed - old;
		if ((uidTimer->flags & (TIMER_FLAG_EVENT | TIMER_FLAG_SUSPENDED)) == TIMER_FLAG_EVENT &&
		    *time < uidTimer->eventTime &&
		    s_threadmgrRuntime.manager->timerHandlingId != timerId) {
			unlinkTimer(&uidTimer->base.timer);
			uidTimer->base.timer.expiry = now + (uidTimer->eventTime - *time);
			insertTimer(&uidTimer->base.timer);
			setNextTimer();
		}
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
	} else {
		previous = uidTimer->time;
		uidTimer->time = *time;
	}
	*time = previous;
	ret = 0;
	sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	sceGUIDReleaseObject(timerId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the same with the count by value; returns the old count, or -1 */
SceUInt64 SceThreadmgrForDriver_85195A16(SceUID timerId, SceUInt64 time)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint64_t now, elapsed, previous = (uint64_t)-1;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (SceIntrmgrForDriver_182EE3E3())
		goto restore;
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) >= 0) {
		uidTimer = (ThreadmgrUidTimer *)pObject;
		sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING) {
			now = SceSysrootForKernel_E20F6FC8();
			sceKernelSpinlockLowLock(TIMER_LOCK());
			elapsed = now - uidTimer->base.timer.process->time;
			previous = elapsed - uidTimer->time;
			uidTimer->time = elapsed - time;
			if ((uidTimer->flags & (TIMER_FLAG_EVENT | TIMER_FLAG_SUSPENDED)) ==
			    TIMER_FLAG_EVENT && time < uidTimer->eventTime &&
			    s_threadmgrRuntime.manager->timerHandlingId != timerId) {
				unlinkTimer(&uidTimer->base.timer);
				uidTimer->base.timer.expiry = now + (uidTimer->eventTime - time);
				insertTimer(&uidTimer->base.timer);
				setNextTimer();
			}
			sceKernelSpinlockLowUnlock(TIMER_LOCK());
		} else {
			previous = uidTimer->time;
			uidTimer->time = time;
		}
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		sceGUIDReleaseObject(timerId);
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return previous;
}

/*
 * The event comes when the count reaches its present value plus
 * *interval (set, or pulsed for type 1), and every interval after with
 * repeat.
 */
int sceKernelSetTimerEvent(SceUID timerId, SceUInt32 type, SceUInt64 *interval, int repeat)
{
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint32_t saved, flags;
	bool requeue;
	uint64_t now;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF(0, 2188, "*** error: illegal context. timerId=0x%08x[%s]\n",
				      timerId, SceSysmemForDriver_E655852F(timerId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (type > 1) {
		SCE_DBG_PRINTF(0, 2196,
			"*** error: sceKernelSetTimerEvent(0x%08X[%s])=0x%08X, type=0x%08X\n",
			timerId, SceSysmemForDriver_E655852F(timerId), SCE_KERNEL_ERROR_ILLEGAL_TYPE, type);
		ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
		goto restore;
	}
	if (interval == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 2215, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	sceKernelSpinlockLowLock(TIMER_LOCK());
	flags = uidTimer->flags;
	requeue = false;
	if ((flags & (TIMER_FLAG_EVENT | TIMER_FLAG_SUSPENDED)) == TIMER_FLAG_EVENT) {
		if (s_threadmgrRuntime.manager->timerHandlingId == timerId) {
			/* the interrupt handler puts it back */
			flags |= TIMER_FLAG_RESCHEDULE;
			uidTimer->flags = flags;
		} else {
			requeue = true;
			unlinkTimer(&uidTimer->base.timer);
			flags = uidTimer->flags;
		}
	}
	if (repeat == 0)
		flags &= ~TIMER_FLAG_REPEAT;
	else
		flags |= TIMER_FLAG_REPEAT;
	uidTimer->flags = flags;
	uidTimer->pulse = type;
	uidTimer->interval = *interval;
	if (flags & TIMER_FLAG_RUNNING) {
		now = SceSysrootForKernel_E20F6FC8();
		flags = uidTimer->flags;
		uidTimer->eventTime = now + (*interval -
			(uidTimer->base.timer.process->time + uidTimer->time));
		if ((flags & TIMER_FLAG_SUSPENDED) ||
		    s_threadmgrRuntime.manager->timerHandlingId == timerId)
			goto armed;
		uidTimer->base.timer.expiry = SceSysrootForKernel_E20F6FC8() + *interval;
		insertTimer(&uidTimer->base.timer);
		uidTimer->flags |= TIMER_FLAG_EVENT;
		setNextTimer();
	} else {
		uidTimer->eventTime = uidTimer->time + *interval;
armed:
		uidTimer->flags = flags | TIMER_FLAG_EVENT;
		if (requeue)
			setNextTimer();
	}
	ret = 0;
	sceKernelSpinlockLowUnlock(TIMER_LOCK());
	sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	sceGUIDReleaseObject(timerId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the event time goes away and every waiter wakes with SCE_KERNEL_ERROR_WAIT_CANCEL */
int sceKernelCancelTimer(SceUID timerId, SceUInt32 *numWaitThreads)
{
	struct ThreadmgrWaitNode *queue, *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	ThreadmgrUidTimer *uidTimer;
	SceObjectBase *pObject;
	uint32_t saved, flags, wake, left;
	SceUID waitId;
	int intr, ret, count;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), &pObject) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 2330, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	uidTimer = (ThreadmgrUidTimer *)pObject;
	queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(&uidTimer->base.event);
	sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	flags = uidTimer->flags;
	if (flags & TIMER_FLAG_EVENT) {
		if (!(flags & TIMER_FLAG_SUSPENDED)) {
			sceKernelSpinlockLowLock(TIMER_LOCK());
			if (s_threadmgrRuntime.manager->timerHandlingId != timerId) {
				unlinkTimer(&uidTimer->base.timer);
				setNextTimer();
			}
			sceKernelSpinlockLowUnlock(TIMER_LOCK());
			flags = uidTimer->flags;
		}
		uidTimer->flags = flags & ~(TIMER_FLAG_EVENT | TIMER_FLAG_REPEAT |
					    TIMER_FLAG_RESCHEDULE);
	}
	count = 0;
	wake = 0;
	for (node = uidTimer->base.event.next; node != queue; node = next) {
		next = node->next;
		if (EventWaiterIsThread(node)) {
			thread = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 2432);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			if (thread->waitType == SCE_KERNEL_WAITTYPE_EVENT) {
				if (thread->waitEventPatternOutput != NULL)
					*thread->waitEventPatternOutput = 0x10;
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
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 2374);
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		wme = thread->waitMultipleEvents;
		left = unlinkEventWaitNode(wait, wme);
		waitId = thread->waitId;
		if (!wme->busy && waitId != WAIT_ID_MULTIPLE_EVENTS) {
			releaseEventWaiter(&uidTimer->base.event, wait, thread);
		} else {
			wake |= completeEventWaitNode(&uidTimer->base.event, wait, thread, wme, left,
				waitId, SCE_KERNEL_ERROR_WAIT_CANCEL, 0x10, 0, 0, 0);
			count++;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (freeEventWaitNode(thread->processId, wait) < 0)
			SCE_PANIC(SCE_LINE(2430, 2427));
	}
	sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	if (numWaitThreads != NULL)
		*numWaitThreads = count;
	sceGUIDReleaseObject(timerId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ret = 0;
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * SceThreadmgrForKernel_7425185E: a process stops running; its timers
 * leave the queue (TIMER_FLAG_SUSPENDED) until it runs again.
 */
int SceThreadmgrForKernel_7425185E(SceProcessObject *process)
{
	ListNode *head = &process->timers;
	ListNode *link, *next;
	ThreadmgrUidTimer *uidTimer;
	volatile int spin;
	uint32_t flags;
	int intr;

	intr = SceCpuForDriver_821FC0EE();
retry:
	sceKernelSpinlockLowLock(&process->timerLock);
	for (link = head->next; link != head; link = next) {
		uidTimer = UID_TIMER_FROM_PROCESS_LINK(link);
		next = link->next;
		if (SceCpuForDriver_5AC9D394(&uidTimer->base.event.lock) < 0) {
			/* lock order: let go and try again */
			sceKernelSpinlockLowUnlock(&process->timerLock);
			for (spin = 10; spin != 0; spin--)
				;
			goto retry;
		}
		flags = uidTimer->flags;
		if (!(flags & TIMER_FLAG_SUSPENDED)) {
			uidTimer->flags = flags | TIMER_FLAG_SUSPENDED;
			if ((flags & (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) ==
			    (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) {
				sceKernelSpinlockLowLock(TIMER_LOCK());
				if (s_threadmgrRuntime.manager->timerHandlingId != uidTimer->base.timer.uid)
					unlinkTimer(&uidTimer->base.timer);
				sceKernelSpinlockLowUnlock(TIMER_LOCK());
			}
		}
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	}
	sceKernelSpinlockLowUnlock(&process->timerLock);
	sceKernelSpinlockLowLock(TIMER_LOCK());
	setNextTimer();
	sceKernelSpinlockLowUnlock(TIMER_LOCK());
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* SceThreadmgrForKernel_89C17825: the process runs again; its timers go back on the queue */
int SceThreadmgrForKernel_89C17825(SceProcessObject *process)
{
	ListNode *head = &process->timers;
	ListNode *link, *next;
	ThreadmgrUidTimer *uidTimer;
	volatile int spin;
	uint32_t flags;
	int intr;

	intr = SceCpuForDriver_821FC0EE();
retry:
	sceKernelSpinlockLowLock(&process->timerLock);
	for (link = head->next; link != head; link = next) {
		uidTimer = UID_TIMER_FROM_PROCESS_LINK(link);
		next = link->next;
		if (SceCpuForDriver_5AC9D394(&uidTimer->base.event.lock) < 0) {
			sceKernelSpinlockLowUnlock(&process->timerLock);
			for (spin = 10; spin != 0; spin--)
				;
			goto retry;
		}
		flags = uidTimer->flags;
		if (flags & TIMER_FLAG_SUSPENDED) {
			uidTimer->flags = flags & ~TIMER_FLAG_SUSPENDED;
			if ((flags & (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) ==
			    (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) {
				sceKernelSpinlockLowLock(TIMER_LOCK());
				uidTimer->base.timer.expiry =
					uidTimer->base.timer.process->time +
					(uidTimer->time + uidTimer->eventTime);
				insertTimer(&uidTimer->base.timer);
				sceKernelSpinlockLowUnlock(TIMER_LOCK());
			}
		}
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	}
	sceKernelSpinlockLowUnlock(&process->timerLock);
	sceKernelSpinlockLowLock(TIMER_LOCK());
	setNextTimer();
	sceKernelSpinlockLowUnlock(TIMER_LOCK());
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* SceThreadmgrForDriver_19F3E92A: a timer's information */
int SceThreadmgrForDriver_19F3E92A(SceUID timerId, SceKernelTimerInfo *info)
{
	SceKernelTimerInfo local;
	uint32_t saved;
	SceSize size;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		ret = getTimerInfo(timerId, &local);
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

/* SceThreadmgrForDriver_3C297724: the information of every timer visible at level */
int SceThreadmgrForDriver_3C297724(int level, SceKernelTimerInfo *infos, SceSize size,
				   SceSize *count)
{
	SceSize got = 0;
	SceKernelTimerInfo info;
	SceUID *vector;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = sceGUIDGetUIDVectorByClass(TIMER_CLASS(), level, NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = sceGUIDGetUIDVectorByClass(TIMER_CLASS(), level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getTimerInfo(vector[i], &info) != 0)
			continue;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		memcpy(&infos[got++], &info, sizeof(info));
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
end:
	return ret;
}

/* SceThreadmgrForDriver_C327CA7C: the same for a process's timer PUIDs */
int SceThreadmgrForDriver_C327CA7C(SceUID pid, int level, SceKernelTimerInfo *infos,
				   SceSize size, SceSize *count)
{
	SceSize got = 0;
	SceKernelTimerInfo info;
	SceUInt32 attr;
	SceUID *vector;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = scePUIDGetUIDVectorByClass(pid, TIMER_CLASS(), level, NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = scePUIDGetUIDVectorByClass(pid, TIMER_CLASS(), level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getTimerInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.timerId = vector[i];
		if (scePUIDGetAttr(pid, info.timerId, &attr) >= 0 && (attr & PUID_ATTR_OPENED))
			info.attr |= PUID_ATTR_OPENED;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		memcpy(&infos[got++], &info, sizeof(info));
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
end:
	return ret;
}

/* how long until the event (in the timer's count while it is stopped) */
int sceKernelGetTimerEventRemainingTime(SceUID timerId, SceUInt64 *remaining)
{
	ThreadmgrUidTimer *uidTimer;
	SceProcessObject *process;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (remaining == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 2977, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	ret = sceGUIDReferObjectWithClass(uidTimer->base.timer.pid, PROCESS_CLASS(),
					   (SceObjectBase **)&process);
	if (ret >= 0) {
		sceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (!(uidTimer->flags & TIMER_FLAG_EVENT)) {
			ret = SCE_KERNEL_ERROR_EVENT_NOT_SET;
		} else if (!(uidTimer->flags & TIMER_FLAG_RUNNING)) {
			*remaining = uidTimer->eventTime - uidTimer->time;
			ret = 0;
		} else {
			ret = 0;
			*remaining = uidTimer->time + uidTimer->eventTime + process->time -
				     SceSysrootForKernel_E20F6FC8();
		}
		sceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		sceGUIDReleaseObject(uidTimer->base.timer.pid);
	}
	sceGUIDReleaseObject(timerId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

SceUID SceThreadmgrForDriver_C58DF384(const char *name, SceUInt32 delay,
				      ThreadmgrAlarmHandler handler, void *arg)
{
	if (name == NULL || handler == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	return setAlarmWithName(name, delay, handler, arg);
}

/* SceThreadmgrForDriver_10517330: SceThreadmgrForDriver_C58DF384 with a 64-bit delay */
SceUID SceThreadmgrForDriver_10517330(const char *name, const SceUInt64 *delay,
				      ThreadmgrAlarmHandler handler, void *arg)
{
	if (name == NULL || delay == NULL || handler == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	return setAlarmWithName(name, *delay, handler, arg);
}

/* an alarm is taken off (not from its own handler) */
int sceKernelCancelAlarm(SceUID alarmId)
{
	ThreadmgrTraceHook hook;
	ThreadmgrAlarm *alarm;
	SceObjectBase *pObject;
	int intr, ret;

	intr = SceCpuForDriver_821FC0EE();
	if (s_threadmgrRuntime.manager->timerCpuId == (int)sceKernelCpuId() && s_threadmgrRuntime.manager->timerHandlingId == alarmId) {
		ret = SCE_KERNEL_ERROR_ALARM_CAN_NOT_CANCEL;
		goto resume;
	}
	ret = sceGUIDReferObjectWithClass(alarmId, s_threadmgrRuntime.manager->alarmClass,
					   &pObject);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_ALARM_ID;
		goto resume;
	}
	alarm = (ThreadmgrAlarm *)pObject;
	sceKernelSpinlockLowLock(TIMER_LOCK());
	if (alarm->base.timer.deleted == 0) {
		alarm->base.timer.deleted = 1;
		if (s_threadmgrRuntime.manager->timerHandlingId == alarmId) {
			/* the interrupt handler closes it */
			sceKernelSpinlockLowUnlock(TIMER_LOCK());
		} else {
			unlinkTimer(&alarm->base.timer);
			setNextTimer();
			sceKernelSpinlockLowUnlock(TIMER_LOCK());
			if (sceGUIDClose(alarmId) < 0)
				SCE_PANIC(SCE_LINE(3261, 3230));
		}
		ret = 0;
		hook = s_threadmgrRuntime.threadTraceHook;
		if (hook != NULL)
			hook(ALARM_HOOK_CANCEL, 1, alarmId);
	} else {
		sceKernelSpinlockLowUnlock(TIMER_LOCK());
	}
	sceGUIDReleaseObject(alarmId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForDriver_4A038803: an alarm's information */
int SceThreadmgrForDriver_4A038803(SceUID alarmId, ThreadmgrAlarmInfo *info)
{
	ThreadmgrAlarmInfo local;
	SceSize size;
	int ret;

	if (info->size > sizeof(local))
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	ret = getAlarmInfo(alarmId, &local);
	if (ret == 0) {
		size = info->size;
		if (size > sizeof(local))
			size = sizeof(local);
		memcpy(info, &local, size);
	}
	return ret;
}

/* SceThreadmgrForDriver_2828F884: the information of every alarm visible at level */
int SceThreadmgrForDriver_2828F884(int level, ThreadmgrAlarmInfo *infos, SceSize size,
				   SceSize *count)
{
	SceSize got = 0;
	ThreadmgrAlarmInfo info;
	SceUID *vector;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = sceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->alarmClass, level,
					  NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = sceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->alarmClass, level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getAlarmInfo(vector[i], &info) != 0)
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
end:
	return ret;
}

/* new delays into the cache until it holds target */
static inline __attribute__((always_inline)) int uidDelay_CreateCache(int target)
{
	struct SceKernelThreadmgrPrivate *manager;
	ThreadmgrDelay *delay;
	bool more = true;
	SceUID uid;
	int intr, ret;

	do {
		intr = SceCpuForDriver_821FC0EE();
		uid = sceGUIDKernelCreate(s_threadmgrRuntime.manager->delayClass,
					   "SceKernelDelayObject", (SceObjectBase **)&delay);
		if (uid < 0) {
			sceKernelCpuResumeIntr(intr);
			SCE_DBG_PRINTF(0, 3753,
				"*** error: uidDelay_CreateCache() -> sceGUIDKernelCreate()=0x%08X\n", uid);
			return uid;
		}
		sceKernelSpinlockLowUnlock(&delay->base.event.lock);
		delay->base.event.ownerPid = SCE_KERNEL_PROCESS_ID;
		ret = sceGUIDGetEntry(uid, &delay->base.event.uidEntry);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 3766,
				"*** error: uidDelay_CreateCache() -> sceGUIDGetEntry(0x%08X)=0x%08X\n",
				uid, ret);
			if (sceGUIDClose(uid) < 0)
				SCE_PANIC(SCE_LINE(3809, 3769));
		} else {
			SCE_DBG_ASSERT_CHECK(delay->base.event.uidEntry != NULL, 3774);
			manager = s_threadmgrRuntime.manager;
			sceKernelSpinlockLowLock(&manager->delayCacheLock);
			delay->base.event.cacheNext = manager->delayCache;
			manager->delayCacheCount++;
			manager->delayCache = delay;
			if (manager->delayCacheCount >= target)
				more = false;
			sceKernelSpinlockLowUnlock(&manager->delayCacheLock);
		}
		sceKernelCpuResumeIntr(intr);
	} while (more);
	return ret;
}

/* cached delays go until the cache holds target */
static inline __attribute__((always_inline)) void uidDelay_DeleteCache(int target)
{
	struct SceKernelThreadmgrPrivate *manager;
	ThreadmgrDelay *delay;
	int intr;

	for (;;) {
		manager = s_threadmgrRuntime.manager;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(
			&manager->delayCacheLock);
		if (manager->delayCacheCount <= target)
			break;
		delay = manager->delayCache;
		manager->delayCacheCount--;
		manager->delayCache = delay->base.event.cacheNext;
		sceKernelSpinlockLowUnlock(&manager->delayCacheLock);
		if (sceGUIDClose(SceSysmemForKernel_03A2CAF0(delay->base.event.uidEntry)) < 0)
			SCE_PANIC(SCE_LINE(3848, 3808));
		sceKernelCpuResumeIntr(intr);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&manager->delayCacheLock,
						 intr);
}

/* the cleaner thread keeps the delay cache between low and high */
int uidDelay_ManageCache(int high, int low, int target)
{
	int intr, count, ret;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_threadmgrRuntime.manager->delayCacheLock);
	count = s_threadmgrRuntime.manager->delayCacheCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_threadmgrRuntime.manager->delayCacheLock, intr);
	if (count < low) {
		ret = uidDelay_CreateCache(target);
		if (ret < 0)
			SCE_DBG_PRINTF(0, 3723,
				"*** error: uidDelay_ManageCache() -> uidDelay_CreateCache()=0x%08X\n",
				ret);
		return ret;
	}
	if (count > high)
		uidDelay_DeleteCache(target);
	return 0;
}

/* alarms are not cached: nothing to do */
int uidAlarm_ManageCache(int high, int low, int target)
{
	return 0;
}
