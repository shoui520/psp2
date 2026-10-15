/*
 * SceKernelThreadMgr: timer.c, the CPU timers. One queue, kept by expiry
 * under the manager's timerLock, drives the LT5 hardware timer: its
 * interrupt ends delays (timed waits), calls alarms (kernel callbacks) and
 * sets or pulses the events of timers (counters of their process's time).
 * The user syscall wrappers are still in timer_syscalls.c
 * (agent/match-notes/os0/kd/threadmgr.txt).
 */

#include <string.h>
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/error.h"
#include "sce/sysmem.h"
#include "sce/timer_regs.h"
#include <psp2kern/kernel/sysmem.h>
#include "scheduler.h"
#include "timer.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "timer.c"

#define TIMER_INTR			0x8d	/* the LT5's interrupt */
#define TIMER_MIN_DELAY			200	/* microseconds */
#define TIMER_ITEM_SIZE			0x80	/* the class's object allocation size */
#define TIMER_ATTR_LW			0x00000080
#define TIMER_ATTR_UNLIMITED		0x00004000
#define TIMER_KERNEL_ATTR_MASK		0x0000a980	/* what kernel callers may pass */
#define TIMER_USER_ATTR_MASK		0x00002980	/* what a syscall may pass */
#define TIMER_OPTION_MAX_SIZE		4
#define TIMER_EVENT_EXPIRED		0x00008000	/* the bit a timer's event gets */
#define TIMER_EVENT_CREATED		0x00000004
#define TIMER_EVENT_DELETED		0x00000008
#define TIMER_EVENT_PROCESS_EXITED	0x00000200
#define TIMER_UID_ATTR_LW		0x0000c000	/* PLACEHOLDER: name */
#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define WAIT_TYPE_LW_OBJECT		0x00040000
#define PUID_VECTOR_LEVEL		0x00050000	/* PLACEHOLDER: name */
#define SDK_VERSION_DELAY_TIMEOUT	0x020fffff	/* newer SDKs time out a wait already due */
#define SDK_VERSION_DELETE_CHECK	0x03100000	/* from here delete must match create */
#define UID_MAX_PER_PROCESS		0x800
#define UID_LW_MAX_PER_PROCESS		0x100
#define ALARM_HOOK_CREATE		5
#define ALARM_HOOK_CANCEL		6
#define ALARM_HOOK_CALL			7
#define ALARM_HOOK_END			8
#define SCE_KERNEL_ERROR_ILLEGAL_ATTR	0x8002000e
#define SCE_KERNEL_ERROR_ILLEGAL_TYPE	0x8002000c	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_WAIT_CANCEL	0x80028007
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define SCE_KERNEL_ERROR_UNKNOWN_ALARM_ID	0x800280c1	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_ALARM_BUSY	0x800280c2	/* PLACEHOLDER: name; cancelled from its own handler */
#define SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID	0x80028241	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_NOT_SETTED	0x80028242	/* PLACEHOLDER: name; no event time */
#define SCE_KERNEL_ERROR_TIMER_BUSY	0x80024501	/* PLACEHOLDER: name; from its own handler */


extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern SceClass *SceSysmemForKernel_368F1C36(SceUID guid);
extern uint64_t SceSysrootForKernel_E20F6FC8(void);
extern void sceKernelSysrootCorelockLock(unsigned int cpu);
extern void sceKernelSysrootCorelockUnlock(void);
extern void SceSysrootForKernel_5B5EBFB1(volatile SceLongTimerRegisters *timer);
extern int ksceGUIDClose(SceUID guid);
extern SceUID ksceGUIDKernelCreate(SceClass *cls, const char *name, SceObjectBase **object);
extern int SceSysmemForKernel_FF464A49(SceUID guid, void **entry);
extern SceUID SceSysmemForKernel_03A2CAF0(void *entry);
extern int SceSysmemForKernel_9C23DFF7(SceUID pid, SceUID puid, SceClass *cls,
				       SceObjectBase **object);
extern int UIDEntryReleaseObject(SceUID pid, SceUID puid);
extern int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				       SceSize *count);
extern int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level,
				       SceUID *vector, SceSize num, SceSize *count);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern int kscePUIDClose(SceUID pid, SceUID puid);
extern int scePUIDGetAttr(SceUID pid, SceUID puid, SceUInt32 *attr);
extern int ksceKernelIsIntrAllowedInCurrentContext(void);
extern int ksceKernelRegisterIntrHandler(int intr, const char *name, int type,
	int (*handler)(int code, void *arg), void *arg, int priority, int cpuMask, void *opt);
extern int ksceKernelEnableIntr(int intr);
extern int ksceKernelDisableIntr(int intr);
extern int ksceKernelCopyToUserProc(SceUID pid, void *dst, const void *src, SceSize len);

#define TIMER_LOCK()	(&s_threadmgrRuntime.manager->timerLock)
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
	struct ThreadmgrTimer *head =
		(struct ThreadmgrTimer *)&s_threadmgrRuntime.manager->timerQueueNext;
	struct ThreadmgrTimer *pos;

	SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->timerLock != 0, 619);
	for (pos = head->next; pos != head; pos = pos->next) {
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
	SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->timerLock != 0, 639);
	if (timer == s_threadmgrRuntime.manager->timerNextToHandle)
		s_threadmgrRuntime.manager->timerNextToHandle = timer->next;
	timer->next->prev = timer->prev;
	timer->prev->next = timer->next;
	timer->next = timer;
	timer->prev = timer;
}

/*
 * Programs the LT5 for the first timer, gathering the timers due within
 * TIMER_MIN_DELAY of it, and never sooner than TIMER_MIN_DELAY from now.
 */
void setNextTimer(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrTimer *head = (struct ThreadmgrTimer *)&manager->timerQueueNext;
	struct ThreadmgrTimer *timer;
	uint64_t first, limit, expiry, chosen, earliest;

	SCE_DBG_ASSERT_CHECK(manager->timerLock != 0, 548);
	timer = head->next;
	if (timer == head) {
		manager->systemTime->compareHi = 0xffffffff;
		manager->systemTime->compareLo = 0xffffffff;
		return;
	}
	first = timer->expiry;
	if (manager->timerHandlingId != -1)
		manager->timerNextToHandle = timer;
	/* the last of the timers due within TIMER_MIN_DELAY of the first */
	limit = first + TIMER_MIN_DELAY;
	expiry = first;
	chosen = first;
	while (expiry < limit) {
		chosen = expiry;
		timer = timer->next;
		if (timer == head)
			break;
		expiry = timer->expiry;
	}
	earliest = SceSysrootForKernel_E20F6FC8() + TIMER_MIN_DELAY;
	if (earliest <= chosen)
		earliest = chosen;
	manager->timerCompare = earliest;
	manager->systemTime->compare = earliest;
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
	struct SceKernelThreadmgrPrivate *manager;

	SCE_DBG_ASSERT_LEVEL(0, ThreadmgrIntrDisabled(), 3899);
	delay->base.event.next = (struct ThreadmgrWaitNode *)&delay->base.event.next;
	delay->base.event.prev = (struct ThreadmgrWaitNode *)&delay->base.event.next;
	delay->base.event.attr = 0;
	delay->base.event.pattern = 0;
	delay->base.event.waiting = 0;
	memset(&delay->base.timer, 0, sizeof(delay->base.timer));
	delay->callback = NULL;
	delay->thread = NULL;
	manager = s_threadmgrRuntime.manager;
	ksceKernelSpinlockLowLock(&manager->delayCacheLock);
	delay->base.event.cacheNext = manager->delayCache;
	manager->delayCacheCount++;
	manager->delayCache = delay;
	ksceKernelSpinlockLowUnlock(&manager->delayCacheLock);
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

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(TIMER_LOCK());
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
		manager->timerCpuId = ksceKernelCpuId();
		cls = SceSysmemForKernel_368F1C36(timer->uid);
		if (cls == manager->delayClass) {
			/* a timed wait's timeout */
			delay = (ThreadmgrDelay *)CPU_TIMER_FROM_TIMER(timer);
			timer->deleted = 1;
			ksceKernelSpinlockLowUnlock(TIMER_LOCK());
			delay->callback(delay->thread);
			uidDelay_FreeObject(delay);
			ksceKernelSpinlockLowLock(TIMER_LOCK());
			manager->timerCpuId = -1;
			manager->timerHandlingId = -1;
			timer = manager->timerNextToHandle;
			continue;
		}
		if (cls == manager->alarmClass) {
			alarm = (ThreadmgrAlarm *)CPU_TIMER_FROM_TIMER(timer);
			uid = timer->uid;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(TIMER_LOCK(), intr);
			hook = s_threadmgrRuntime.threadTraceHook;
			if (hook != NULL)
				hook(ALARM_HOOK_CALL, 3, uid, alarm->handler, alarm->arg);
			next = alarm->handler(alarm->arg);
			intr = ksceKernelSpinlockLowLockCpuSuspendIntr(TIMER_LOCK());
			manager->timerCpuId = -1;
			manager->timerHandlingId = -1;
			if (!timer->deleted) {
				if (next == 0) {
					/* the alarm is over */
					timer->deleted = 1;
					ksceKernelSpinlockLowUnlock(TIMER_LOCK());
					if (ksceGUIDClose(uid) < 0)
						SCE_PANIC(1105);
					hook = s_threadmgrRuntime.threadTraceHook;
					if (hook != NULL)
						hook(ALARM_HOOK_END, 1, uid);
					goto relock;
				}
				if (next < TIMER_MIN_DELAY)
					next = TIMER_MIN_DELAY;
				timer->expiry = expiry + next;
				insertTimer(timer);
			}
			ksceKernelSpinlockLowUnlock(TIMER_LOCK());
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
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (pulse == 0)
			setEventWithNotifyCallback(uid, TIMER_EVENT_EXPIRED, 0, 0, &uidTimer->base.event);
		else
			pulseEventWithNotifyCallback(uid, TIMER_EVENT_EXPIRED, 0, 0,
						     &uidTimer->base.event);
		ksceKernelSpinlockLowLock(TIMER_LOCK());
		manager->timerCpuId = -1;
		manager->timerHandlingId = -1;
		if (timer->deleted) {
			ksceKernelSpinlockLowUnlock(TIMER_LOCK());
			ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
			if (ksceGUIDClose(uid) < 0)
				SCE_PANIC(SCE_LINE(1172, 1180));
			goto relock;
		}
		if ((uidTimer->flags & TIMER_FLAG_RUNNING) &&
		    (uidTimer->flags & (TIMER_FLAG_REPEAT | TIMER_FLAG_RESCHEDULE))) {
			interval = uidTimer->interval;
			if (interval < TIMER_MIN_DELAY)
				interval = TIMER_MIN_DELAY;
			timer->expiry = expiry + interval;
			if (ksceGUIDReferObjectWithClass(timer->pid, PROCESS_CLASS(), NULL) < 0) {
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
				ksceKernelSpinlockLowUnlock(TIMER_LOCK());
				ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
				if (pid != -1)
					ksceGUIDReleaseObject(pid);
				goto relock;
#else
				ksceGUIDReleaseObject(pid);
				uidTimer->flags &= ~TIMER_FLAG_RESCHEDULE;
#endif
			}
		}
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
relock:
		ksceKernelSpinlockLowLock(TIMER_LOCK());
		timer = manager->timerNextToHandle;
	}
	manager->timerNextToHandle = NULL;
	manager->systemTime->intr = 2;
	setNextTimer();
	ksceKernelSpinlockLowUnlockCpuResumeIntr(TIMER_LOCK(), intr);
	return -1;
}

/* SceUIDTimerClass's destructor: off its process's list, the counts down */
static int uidTimer_Destructor(void *object)
{
	ThreadmgrUidTimer *uidTimer = object;
	SceProcessObject *process = NULL;
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 3650);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&uidTimer->base.event.lock);
#if FW_DEBUG
	if (ksceGUIDReferObjectWithClass(uidTimer->base.timer.pid, PROCESS_CLASS(),
					 (SceObjectBase **)&process) >= 0) {
#else
	if (ksceGUIDReferObjectWithClass(uidTimer->base.timer.pid, PROCESS_CLASS(), NULL) >= 0) {
#endif
		ksceKernelSpinlockLowLock(&uidTimer->base.timer.process->timerLock);
		ListLinkRemove(&uidTimer->processLink);
		ksceKernelSpinlockLowUnlock(&uidTimer->base.timer.process->timerLock);
#if !FW_DEBUG
		ksceGUIDReleaseObject(uidTimer->base.timer.pid);
#endif
	}
	ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
#if FW_DEBUG
	if (process != NULL)
		ksceGUIDReleaseObject(uidTimer->base.timer.pid);
#endif
	pid = uidTimer->base.event.ownerPid;
	if (ksceGUIDReferObjectWithClass(pid, PROCESS_CLASS(), (SceObjectBase **)&process) >= 0) {
		if (process->pid != THREADMGR_KERNEL_PID) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (uidTimer->base.event.attr & TIMER_ATTR_LW)
				process->uidLwCount--;
			else if (uidTimer->base.event.attr & TIMER_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->timerCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		ksceGUIDReleaseObject(pid);
	}
	ksceKernelCpuResumeIntr(intr);
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
	uint32_t saved, flags, wake;
	SceUID threadId;
	bool close;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 1425, "*** error: illegal context. timerId=0x%08x[%s]\n",
				      timerId, ksceGUIDName(timerId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1440, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	ksceKernelSpinlockLowLock(TIMER_LOCK());
	if (uidTimer->base.timer.deleted == 1) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		ksceGUIDReleaseObject(timerId);
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
				flags = uidTimer->flags;
			}
		}
		uidTimer->flags = flags & ~(TIMER_FLAG_EVENT | TIMER_FLAG_REPEAT);
	}
	ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	if (puid == -1) {
		threadId = thread->threadId;
	} else {
		ret = kscePUIDClose(0, puid);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 1480);
		threadId = thread->userThreadId;
	}
	if (close && ksceGUIDClose(timerId) < 0)
		SCE_PANIC(SCE_LINE(1489, 1490));
	wake = setEventWithNotifyCallback(timerId, TIMER_EVENT_DELETED,
		TimerEventCaller(threadId, thread->processId), TIMER_EVENT_DELETED,
		&uidTimer->base.event);
	if ((int)wake < 0)
		SCE_PANIC(SCE_LINE(1495, 1496));
	ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	ksceGUIDReleaseObject(timerId);
	ret = 0;
	if (wake & 1)
		switchThread(0);
resume:
	ksceKernelCpuResumeIntr(intr);
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
	intr = ksceKernelCpuSuspendIntr();
	uid = ksceGUIDKernelCreate(s_threadmgrRuntime.manager->alarmClass, name,
				   (SceObjectBase **)&alarm);
	if (uid >= 0) {
		ksceKernelSpinlockLowUnlock(&alarm->base.event.lock);
		alarm->base.event.ownerPid = THREADMGR_KERNEL_PID;
		alarm->base.timer.deleted = 0;
		alarm->base.timer.uid = uid;
		alarm->base.timer.expiry = SceSysrootForKernel_E20F6FC8() + delay;
		alarm->base.timer.pid = thread == NULL ? THREADMGR_KERNEL_PID : thread->processId;
		alarm->base.timer.process = process;
		alarm->handler = handler;
		alarm->arg = arg;
		strncpy(alarm->name, name, sizeof(alarm->name));
		ksceKernelSpinlockLowLock(TIMER_LOCK());
		insertTimer(&alarm->base.timer);
		setNextTimer();
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		hook = s_threadmgrRuntime.alarmTraceHook;
		if (hook != NULL)
			hook(ALARM_HOOK_CREATE, (uint32_t)(delay >> 32), (uint32_t)delay, handler, arg,
			     uid);
	}
	ksceKernelCpuResumeIntr(intr);
	return uid;
}

/* an alarm's information */
static int getAlarmInfo(SceUID alarmId, ThreadmgrAlarmInfo *info)
{
	ThreadmgrAlarm *alarm;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(alarmId, s_threadmgrRuntime.manager->alarmClass,
					 (SceObjectBase **)&alarm) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_ALARM_ID;
	} else {
		info->alarmId = alarmId;
		ret = 0;
		strncpy(info->name, alarm->name, sizeof(info->name) - 1);
		ksceKernelSpinlockLowLock(TIMER_LOCK());
		info->handler = alarm->handler;
		info->arg = alarm->arg;
		info->expiry = alarm->base.timer.expiry;
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		ksceGUIDReleaseObject(alarmId);
	}
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* a timer's information */
static int getTimerInfo(SceUID timerId, ThreadmgrTimerInfo *info)
{
	ThreadmgrUidTimer *uidTimer;
	SceProcessObject *process;
	uint32_t flags;
	SceUID pid;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		ret = TimerUnknownIdError();
		goto resume;
	}
	info->timerId = timerId;
	strncpy(info->name, ksceGUIDName(timerId), sizeof(info->name) - 1);
	ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	flags = uidTimer->flags;
	info->numWaitThreads = uidTimer->base.event.waiting;
	info->time = uidTimer->time;
	info->attr = uidTimer->base.event.attr;
	info->running = flags & TIMER_FLAG_RUNNING;
	if (flags & TIMER_FLAG_EVENT)
		info->eventTime = uidTimer->eventTime;
	info->repeat = (flags & TIMER_FLAG_REPEAT) >> 2;
	info->interval = uidTimer->interval;
	info->pulse = uidTimer->pulse;
	pid = uidTimer->base.timer.pid;
	process = uidTimer->base.timer.process;
	ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	ksceGUIDReleaseObject(timerId);
	if (ksceGUIDReferObjectWithClass(pid, PROCESS_CLASS(), NULL) >= 0) {
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			info->current = SceSysrootForKernel_E20F6FC8() - process->time -
					info->time;
		else
			info->current = info->time;
		ksceGUIDReleaseObject(pid);
	}
	ret = 0;
resume:
	ksceKernelCpuResumeIntr(intr);
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

SceInt64 ksceKernelGetSystemTimeWide(void)
{
	return ReadSystemTime();
}

SceUInt32 ksceKernelGetSystemTimeLow(void)
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
	if (ksceKernelCpuId() == 0) {
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.paddr = TIMER_LONG_REG_BASE(5);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
		uid = ksceKernelAllocMemBlock("SceLT5", LT5_MEMBLOCK_TYPE, TIMER_REG_SIZE, &opt);
		manager = s_threadmgrRuntime.manager;
		if (uid < 0) {
			SCE_DBG_PRINTF(0xb, 331, "memBlockId 0x%08x\n", uid);
			SCE_PANIC(332);
		}
		manager->timerMemBlockId = uid;
		ksceKernelGetMemBlockBase(uid, (void **)&manager->systemTime);
		SceSysrootForKernel_5B5EBFB1(s_threadmgrRuntime.manager->systemTime);
		ret = ksceKernelRegisterIntrHandler(TIMER_INTR, "SceThreadmgrTimer", 0,
			timerIntrHandler, (void *)s_threadmgrRuntime.manager, 0x30, 0xf, NULL);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 344, "error: register timer handler.(0x%08X)\n", ret);
			SCE_PANIC(345);
		}
		cls = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
		if (cls == NULL)
			SCE_PANIC(351);
		if (uidCpuTimerClassInit(cls, "SceUIDCpuTimerClass",
				s_threadmgrRuntime.manager->eventClass,
				sizeof(ThreadmgrCpuTimer), uidCpuTimer_Constructor,
				uidCpuTimer_Destructor) < 0)
			SCE_PANIC(361);
		manager = s_threadmgrRuntime.manager;
		manager->cpuTimerClass = (SceClass *)cls;
		cls = ksceKernelAllocHeapMemory(manager->heapId, sizeof(*cls));
		if (cls == NULL)
			SCE_PANIC(367);
		uidDelayClassInit(cls);
		s_threadmgrRuntime.manager->delayClass = (SceClass *)cls;
		SCE_DBG_PRINTF_LEVEL(2, 0, 380, " sizeof(SceUIDDelayObject)=%d\n",
				     sizeof(ThreadmgrDelay));
		cls = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
		if (cls == NULL)
			SCE_PANIC(385);
		uidAlarmClassInit(cls);
		s_threadmgrRuntime.manager->alarmClass = (SceClass *)cls;
		SCE_DBG_PRINTF_LEVEL(2, 0, 398, " sizeof(SceUIDAlarmObject)=%d\n",
				     sizeof(ThreadmgrAlarm));
		cls = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
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
	if (ksceKernelCpuId() == 0) {
		cpu->lastThreadSwitchTime = 0;
		ret = ksceKernelEnableIntr(TIMER_INTR);
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
	if (ksceKernelCpuId() == 0) {
		s_suspendedTime = SceSysrootForKernel_E20F6FC8();
		ksceKernelDisableIntr(TIMER_INTR);
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
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	volatile SceLongTimerRegisters *lt5;
	uint32_t control;

	sceKernelSysrootCorelockLock(0);
	if (ksceKernelCpuId() == 0) {
		lt5 = manager->systemTime;
		control = lt5->control;
		lt5->control = 0;
		lt5->countLo = (uint32_t)s_suspendedTime;
		lt5->countHi = (uint32_t)(s_suspendedTime >> 32);
		lt5->compareLo = (uint32_t)manager->timerCompare;
		lt5->compareHi = (uint32_t)(manager->timerCompare >> 32);
		lt5->control = control;
		ksceKernelEnableIntr(TIMER_INTR);
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
	ksceKernelSpinlockLowLock(TIMER_LOCK());
	insertTimer(&delay->base.timer);
	setNextTimer();
	ksceKernelSpinlockLowUnlock(TIMER_LOCK());
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
		if (thread->processId == THREADMGR_KERNEL_PID)
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
	ksceKernelSpinlockLowLock(&manager->delayCacheLock);
	if (manager->delayCacheCount < 1) {
		ksceKernelSpinlockLowUnlock(&manager->delayCacheLock);
		uid = ksceGUIDKernelCreate(s_threadmgrRuntime.manager->delayClass,
					   "SceKernelDelayObject", (SceObjectBase **)&delay);
		if (uid < 0) {
			SCE_DBG_PRINTF(0, 3859,
				"*** error: uidDelay_AllocObject() -> sceGUIDKernelCreate()=0x%08X\n", uid);
		} else {
			ksceKernelSpinlockLowUnlock(&delay->base.event.lock);
			delay->base.event.ownerPid = THREADMGR_KERNEL_PID;
			ret = SceSysmemForKernel_FF464A49(uid, &delay->base.event.uidEntry);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 3871,
					"*** error: uidDelay_AllocObject() -> sceGUIDGetEntry(0x%08X)=0x%08X\n",
					uid, ret);
				if (ksceGUIDClose(uid) < 0)
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
		ksceKernelSpinlockLowUnlock(&manager->delayCacheLock);
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
	SceUInt32 *timeout;
	SceUInt32 remaining;
	uint64_t now;
	int ret;

	ret = ksceGUIDReferObjectWithClass(delayId, s_threadmgrRuntime.manager->delayClass,
					   (SceObjectBase **)&delay);
	if (ret < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 764, "error: sceGUIDReferObjectWithClass(0x%08X)=0x%08X\n",
				      delayId, ret);
		return;
	}
	ksceKernelSpinlockLowLock(TIMER_LOCK());
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
			if (thread->waitType == WAIT_TYPE_LW_OBJECT &&
			    thread->timeoutProcessId != THREADMGR_KERNEL_PID) {
				ret = ksceKernelCopyToUserProc(thread->timeoutProcessId, timeout,
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
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		uidDelay_FreeObject(delay);
	} else {
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	}
	ksceGUIDReleaseObject(delayId);
}

/*
 * A process exits: its timers are deleted (those it created) and its
 * timer PUIDs closed, each event told TIMER_EVENT_PROCESS_EXITED.
 */
int timerProcessExit(SceProcessObject *process)
{
	ThreadmgrUidTimer *uidTimer;
	SceUID *vector, guid, pid;
	SceSize got = 0;
	uint32_t flags;
	bool close;
	int num, i, intr, ret;

	num = SceSysmemForKernel_08C05493(process->pid, TIMER_CLASS(), PUID_VECTOR_LEVEL, NULL, 0,
					  NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 866);
		num = SceSysmemForKernel_08C05493(process->pid, TIMER_CLASS(), 5, vector, num, &got);
		if ((int)got >= num)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < num; i++) {
		guid = kscePUIDtoGUID(process->pid, vector[i]);
		intr = ksceKernelCpuSuspendIntr();
		ret = SceSysmemForKernel_9C23DFF7(process->pid, vector[i], TIMER_CLASS(),
						  (SceObjectBase **)&uidTimer);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 887,
				" k: timerProcessExit()->scePUIDReferObjectWithClass(0x%08X)=0x%08X\n",
				vector[i], ret);
			SCE_PANIC(888);
		}
		ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		pid = process->pid;
		if (uidTimer->base.timer.deleted == 0 && uidTimer->base.timer.pid == pid) {
			flags = uidTimer->flags;
			close = true;
			uidTimer->base.timer.deleted = 1;
			if ((flags & (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) ==
			    (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) {
				if (!(flags & TIMER_FLAG_SUSPENDED)) {
					ksceKernelSpinlockLowLock(TIMER_LOCK());
					if (s_threadmgrRuntime.manager->timerHandlingId != guid) {
						unlinkTimer(&uidTimer->base.timer);
						setNextTimer();
					} else {
						/* the interrupt handler closes it */
						close = false;
					}
					ksceKernelSpinlockLowUnlock(TIMER_LOCK());
					flags = uidTimer->flags;
				}
				uidTimer->flags = flags & ~(TIMER_FLAG_EVENT | TIMER_FLAG_REPEAT);
			}
			if (close && ksceGUIDClose(guid) < 0) {
				SCE_DBG_PRINTF(0, 921, "\n");
				SCE_PANIC(922);
			}
			if (uidTimer->base.timer.process == process) {
				ksceKernelSpinlockLowLock(&process->timerLock);
				ListLinkRemove(&uidTimer->processLink);
				ksceKernelSpinlockLowUnlock(&process->timerLock);
			}
			pid = process->pid;
		}
		ret = kscePUIDClose(pid, vector[i]);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 937, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(938);
		}
		if ((int)setEventWithNotifyCallback(guid, TIMER_EVENT_PROCESS_EXITED,
				(uint64_t)(int64_t)process->pid, TIMER_EVENT_PROCESS_EXITED,
				&uidTimer->base.event) < 0)
			SCE_PANIC(947);
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		if (UIDEntryReleaseObject(process->pid, vector[i]) < 0)
			SCE_PANIC(955);
		ksceKernelCpuResumeIntr(intr);
	}
	if (vector != NULL) {
		ret = ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 963);
		return ret;
	}
	return 0;
}

typedef struct ThreadmgrTimerOptParam {
	SceSize size;
} ThreadmgrTimerOptParam;

/*
 * The early errors return with TPIDRURO still shifted, as Sony's code
 * does (see ksceKernelCreateSema).
 */
SceUID sceKernelCreateTimer(const char *name, SceUInt32 attr,
			    const ThreadmgrTimerOptParam *option)
{
	struct ThreadCB *thread =
		currentThreadContext();
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrUidTimer *uidTimer;
	uint32_t saved;
	SceUID uid, threadId;
	bool syscall;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
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
	if (attr & TIMER_ATTR_LW) {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.attr = TIMER_UID_ATTR_LW;
	} else {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		if (ThreadmgrInSyscall()) {
			opt.pid = thread->processId;
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
			if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
				attr |= TIMER_ATTR_UNLIMITED;
			opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
		}
	}
#if FW_DEBUG
	if (!ThreadmgrInSyscall()) {
		intr = ksceKernelCpuSuspendIntr();
	} else {
		process = thread->process;
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
#else
	intr = ksceKernelCpuSuspendIntr();
	if (ThreadmgrInSyscall()) {
		process = thread->process;
		ksceKernelSpinlockLowLock(&process->resourceLock);
#endif
		if (attr & TIMER_ATTR_LW) {
			if (process->uidLwCount >= UID_LW_MAX_PER_PROCESS) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF(0, 1317, "sceKernelCreateTimer(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidLwCount > process->uidLwPeak)
				process->uidLwPeak = process->uidLwCount;
		} else if (attr & TIMER_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= UID_MAX_PER_PROCESS) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
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
		ksceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = ksceGUIDKernelCreateWithOpt(TIMER_CLASS(), name, (SceGUIDKernelCreateOpt *)&opt,
					  (SceObjectBase **)&uidTimer);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (attr & TIMER_ATTR_LW)
				process->uidLwCount--;
			else if (attr & TIMER_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->timerCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		goto resume;
	}
	syscall = ThreadmgrInSyscall();
	process = thread->process;
	uidTimer->base.timer.process = process;
	uidTimer->base.event.ownerPid = syscall ? thread->processId : THREADMGR_KERNEL_PID;
	uidTimer->base.timer.uid = uid;
	uidTimer->base.timer.pid = thread->processId;
	uidTimer->base.timer.deleted = 0;
	ksceKernelSpinlockLowLock(&process->timerLock);
	process = uidTimer->base.timer.process;
	if ((process->status & 0xff) != PROCESS_STATE_RUNNING)
		uidTimer->flags |= TIMER_FLAG_SUSPENDED;
	uidTimer->processLink.next = &process->timers;
	uidTimer->processLink.prev = process->timers.prev;
	process->timers.prev = &uidTimer->processLink;
	uidTimer->processLink.prev->next = &uidTimer->processLink;
	ksceKernelSpinlockLowUnlock(&process->timerLock);
	threadId = ThreadmgrInSyscall() ? thread->userThreadId : thread->threadId;
	/* sceKernelEventObjectInit unlocks the object the class hands over locked */
	ret = sceKernelEventObjectInit(uid, attr, TimerEventCaller(threadId, thread->processId),
				       TIMER_EVENT_CREATED);
	SCE_DBG_ASSERT_CHECK(ret >= 0, 1383);
resume:
	ksceKernelCpuResumeIntr(intr);
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

	guid = kscePUIDtoGUID(0, puid);
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
	return ksceKernelSpinlockLowLockCpuSuspendIntr(TIMER_LOCK());
#else
	int intr = ksceKernelCpuSuspendIntr();

	ksceKernelSpinlockLowLock(TIMER_LOCK());
	return intr;
#endif
}

/* starts counting; returns 1 if it already was */
int ksceKernelStartTimer(SceUID timerId)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	ThreadmgrUidTimer *uidTimer;
	uint32_t saved;
	uint64_t now;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = lockTimerQueueCpuSuspendIntr();
	if (manager->timerHandlingId == timerId && manager->timerCpuId == (int)ksceKernelCpuId()) {
		ret = SCE_KERNEL_ERROR_TIMER_BUSY;
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		goto resume;
	}
	ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1620, "sceKernelStartTimer", "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	if (uidTimer->flags & TIMER_FLAG_RUNNING) {
		ret = 1;
	} else {
		uidTimer->flags |= TIMER_FLAG_RUNNING;
		now = SceSysrootForKernel_E20F6FC8();
		if (uidTimer->flags & TIMER_FLAG_EVENT) {
			ksceKernelSpinlockLowLock(TIMER_LOCK());
			uidTimer->base.timer.expiry = now + (uidTimer->eventTime - uidTimer->time);
			if (!(uidTimer->flags & TIMER_FLAG_SUSPENDED)) {
				insertTimer(&uidTimer->base.timer);
				setNextTimer();
			}
			ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		}
		ret = 0;
		/* from now on time is where the count starts from */
		uidTimer->time = now - uidTimer->base.timer.process->time - uidTimer->time;
	}
	ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	ksceGUIDReleaseObject(timerId);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* stops counting; returns 1 if it was counting */
int ksceKernelStopTimer(SceUID timerId)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	ThreadmgrUidTimer *uidTimer;
	uint32_t saved;
	uint64_t now;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = lockTimerQueueCpuSuspendIntr();
	if (manager->timerHandlingId == timerId && manager->timerCpuId == (int)ksceKernelCpuId()) {
		ret = SCE_KERNEL_ERROR_TIMER_BUSY;
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		goto resume;
	}
	ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1703, "sceKernelStopTimer", "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	ret = 0;
	if (uidTimer->flags & TIMER_FLAG_RUNNING) {
		uidTimer->flags &= ~TIMER_FLAG_RUNNING;
		now = SceSysrootForKernel_E20F6FC8();
		ksceKernelSpinlockLowLock(TIMER_LOCK());
		/* from now on time is the count */
		uidTimer->time = now - uidTimer->base.timer.process->time - uidTimer->time;
		if ((uidTimer->flags & (TIMER_FLAG_EVENT | TIMER_FLAG_SUSPENDED)) == TIMER_FLAG_EVENT &&
		    manager->timerHandlingId != timerId) {
			unlinkTimer(&uidTimer->base.timer);
			setNextTimer();
		}
		ret = 1;
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	}
	ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	ksceGUIDReleaseObject(timerId);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* where a running timer counts from (0 when stopped) */
int sceKernelGetTimerBase(SceUID timerId, SceUInt64 *base)
{
	ThreadmgrUidTimer *uidTimer;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (base == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1780, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
	} else {
		ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			*base = uidTimer->time;
		else
			*base = 0;
		ret = 0;
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		ksceGUIDReleaseObject(timerId);
	}
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

SceUInt64 ksceKernelGetTimerBaseWide(SceUID timerId)
{
	ThreadmgrUidTimer *uidTimer;
	SceUInt64 base = (SceUInt64)-1;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) >= 0) {
		ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			base = uidTimer->time;
		else
			base = 0;
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		ksceGUIDReleaseObject(timerId);
	}
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return base;
}

/* the count */
int sceKernelGetTimerTime(SceUID timerId, SceUInt64 *time)
{
	ThreadmgrUidTimer *uidTimer;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (time == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1896, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
	} else {
		ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			*time = SceSysrootForKernel_E20F6FC8() -
				uidTimer->base.timer.process->time - uidTimer->time;
		else
			*time = uidTimer->time;
		ret = 0;
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		ksceGUIDReleaseObject(timerId);
	}
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

SceUInt64 ksceKernelGetTimerTimeWide(SceUID timerId)
{
	ThreadmgrUidTimer *uidTimer;
	SceUInt64 time;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		time = (SceUInt64)-1;
	} else {
		ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING)
			time = SceSysrootForKernel_E20F6FC8() -
			       uidTimer->base.timer.process->time - uidTimer->time;
		else
			time = uidTimer->time;
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		ksceGUIDReleaseObject(timerId);
	}
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return time;
}

/* sets the count to *time, returning the old count there */
int sceKernelSetTimerTime(SceUID timerId, SceUInt64 *time)
{
	ThreadmgrUidTimer *uidTimer;
	uint64_t now, elapsed, old, previous;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (time == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 2008, "*** error: illegal context. timerId=0x%08x[%s]\n",
				      timerId, ksceGUIDName(timerId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 2022, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	if (uidTimer->flags & TIMER_FLAG_RUNNING) {
		now = SceSysrootForKernel_E20F6FC8();
		ksceKernelSpinlockLowLock(TIMER_LOCK());
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
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	} else {
		previous = uidTimer->time;
		uidTimer->time = *time;
	}
	*time = previous;
	ret = 0;
	ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	ksceGUIDReleaseObject(timerId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the same with the count by value; returns the old count, or -1 */
SceUInt64 ksceKernelSetTimerTimeWide(SceUID timerId, SceUInt64 time)
{
	ThreadmgrUidTimer *uidTimer;
	uint64_t now, elapsed, previous = (uint64_t)-1;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext())
		goto restore;
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) >= 0) {
		ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (uidTimer->flags & TIMER_FLAG_RUNNING) {
			now = SceSysrootForKernel_E20F6FC8();
			ksceKernelSpinlockLowLock(TIMER_LOCK());
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
			ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		} else {
			previous = uidTimer->time;
			uidTimer->time = time;
		}
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		ksceGUIDReleaseObject(timerId);
	}
	ksceKernelCpuResumeIntr(intr);
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
	uint32_t saved, flags;
	bool requeue;
	uint64_t now;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 2188, "*** error: illegal context. timerId=0x%08x[%s]\n",
				      timerId, ksceGUIDName(timerId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (type > 1) {
		SCE_DBG_PRINTF(0, 2196,
			"*** error: sceKernelSetTimerEvent(0x%08X[%s])=0x%08X, type=0x%08X\n",
			timerId, ksceGUIDName(timerId), SCE_KERNEL_ERROR_ILLEGAL_TYPE, type);
		ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
		goto restore;
	}
	if (interval == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 2215, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	ksceKernelSpinlockLowLock(TIMER_LOCK());
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
	ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	ksceGUIDReleaseObject(timerId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the event time goes away and every waiter wakes with SCE_KERNEL_ERROR_WAIT_CANCEL */
int sceKernelCancelTimer(SceUID timerId, int *numWaitThreads)
{
	struct ThreadmgrWaitNode *queue, *node, *next;
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *wait;
	ThreadmgrUidTimer *uidTimer;
	uint32_t saved, flags, wake, left;
	SceUID waitId;
	int intr, ret, count;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer) < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 2330, "*** error: unknown timer id. timerId=0x%08x\n",
					      timerId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_TIMER_ID;
		}
		goto resume;
	}
	queue = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(&uidTimer->base.event);
	ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
	flags = uidTimer->flags;
	if (flags & TIMER_FLAG_EVENT) {
		if (!(flags & TIMER_FLAG_SUSPENDED)) {
			ksceKernelSpinlockLowLock(TIMER_LOCK());
			if (s_threadmgrRuntime.manager->timerHandlingId != timerId) {
				unlinkTimer(&uidTimer->base.timer);
				setNextTimer();
			}
			ksceKernelSpinlockLowUnlock(TIMER_LOCK());
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
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
			if (thread->waitType == WAIT_TYPE_EVENT) {
				if (thread->waitEventPatternOutput != NULL)
					*thread->waitEventPatternOutput = 0x10;
				if (thread->waitEventDataOutput != NULL)
					*thread->waitEventDataOutput = 0;
				wake |= wakeupAndDistributeThread(thread, (uintptr_t)queue,
								  SCE_KERNEL_ERROR_WAIT_CANCEL);
				count++;
			}
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			continue;
		}
		wait = (ThreadmgrEventWaitNode *)node;
		thread = wait->thread;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 2374);
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
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
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (freeEventWaitNode(thread->processId, wait) < 0)
			SCE_PANIC(SCE_LINE(2430, 2427));
	}
	ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	if (numWaitThreads != NULL)
		*numWaitThreads = count;
	ksceGUIDReleaseObject(timerId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ret = 0;
resume:
	ksceKernelCpuResumeIntr(intr);
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

	intr = ksceKernelCpuSuspendIntr();
retry:
	ksceKernelSpinlockLowLock(&process->timerLock);
	for (link = head->next; link != head; link = next) {
		uidTimer = UID_TIMER_FROM_PROCESS_LINK(link);
		next = link->next;
		if (ksceKernelSpinlockLowTrylock(&uidTimer->base.event.lock) < 0) {
			/* lock order: let go and try again */
			ksceKernelSpinlockLowUnlock(&process->timerLock);
			for (spin = 10; spin != 0; spin--)
				;
			goto retry;
		}
		flags = uidTimer->flags;
		if (!(flags & TIMER_FLAG_SUSPENDED)) {
			uidTimer->flags = flags | TIMER_FLAG_SUSPENDED;
			if ((flags & (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) ==
			    (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) {
				ksceKernelSpinlockLowLock(TIMER_LOCK());
				if (s_threadmgrRuntime.manager->timerHandlingId != uidTimer->base.timer.uid)
					unlinkTimer(&uidTimer->base.timer);
				ksceKernelSpinlockLowUnlock(TIMER_LOCK());
			}
		}
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	}
	ksceKernelSpinlockLowUnlock(&process->timerLock);
	ksceKernelSpinlockLowLock(TIMER_LOCK());
	setNextTimer();
	ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	ksceKernelCpuResumeIntr(intr);
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

	intr = ksceKernelCpuSuspendIntr();
retry:
	ksceKernelSpinlockLowLock(&process->timerLock);
	for (link = head->next; link != head; link = next) {
		uidTimer = UID_TIMER_FROM_PROCESS_LINK(link);
		next = link->next;
		if (ksceKernelSpinlockLowTrylock(&uidTimer->base.event.lock) < 0) {
			ksceKernelSpinlockLowUnlock(&process->timerLock);
			for (spin = 10; spin != 0; spin--)
				;
			goto retry;
		}
		flags = uidTimer->flags;
		if (flags & TIMER_FLAG_SUSPENDED) {
			uidTimer->flags = flags & ~TIMER_FLAG_SUSPENDED;
			if ((flags & (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) ==
			    (TIMER_FLAG_RUNNING | TIMER_FLAG_EVENT)) {
				ksceKernelSpinlockLowLock(TIMER_LOCK());
				uidTimer->base.timer.expiry =
					uidTimer->base.timer.process->time +
					(uidTimer->time + uidTimer->eventTime);
				insertTimer(&uidTimer->base.timer);
				ksceKernelSpinlockLowUnlock(TIMER_LOCK());
			}
		}
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
	}
	ksceKernelSpinlockLowUnlock(&process->timerLock);
	ksceKernelSpinlockLowLock(TIMER_LOCK());
	setNextTimer();
	ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

/* SceThreadmgrForDriver_19F3E92A: a timer's information */
int SceThreadmgrForDriver_19F3E92A(SceUID timerId, ThreadmgrTimerInfo *info)
{
	ThreadmgrTimerInfo local;
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
int SceThreadmgrForDriver_3C297724(int level, ThreadmgrTimerInfo *infos, SceSize size,
				   SceSize *count)
{
	ThreadmgrTimerInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(TIMER_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(TIMER_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
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
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/* SceThreadmgrForDriver_C327CA7C: the same for a process's timer PUIDs */
int SceThreadmgrForDriver_C327CA7C(SceUID pid, int level, ThreadmgrTimerInfo *infos,
				   SceSize size, SceSize *count)
{
	ThreadmgrTimerInfo info;
	SceUInt32 attr;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = SceSysmemForKernel_08C05493(pid, TIMER_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = SceSysmemForKernel_08C05493(pid, TIMER_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getTimerInfo(kscePUIDtoGUID(pid, vector[i]), &info) != 0)
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
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
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
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(timerId, TIMER_CLASS(), (SceObjectBase **)&uidTimer);
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
	ret = ksceGUIDReferObjectWithClass(uidTimer->base.timer.pid, PROCESS_CLASS(),
					   (SceObjectBase **)&process);
	if (ret >= 0) {
		ksceKernelSpinlockLowLock(&uidTimer->base.event.lock);
		if (!(uidTimer->flags & TIMER_FLAG_EVENT)) {
			ret = SCE_KERNEL_ERROR_NOT_SETTED;
		} else if (!(uidTimer->flags & TIMER_FLAG_RUNNING)) {
			*remaining = uidTimer->eventTime - uidTimer->time;
			ret = 0;
		} else {
			ret = 0;
			*remaining = uidTimer->time + uidTimer->eventTime + process->time -
				     SceSysrootForKernel_E20F6FC8();
		}
		ksceKernelSpinlockLowUnlock(&uidTimer->base.event.lock);
		ksceGUIDReleaseObject(uidTimer->base.timer.pid);
	}
	ksceGUIDReleaseObject(timerId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

SceUID ksceKernelRegisterTimer(const char *name, SceUInt32 delay, ThreadmgrAlarmHandler handler,
			       void *arg)
{
	if (name == NULL || handler == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	return setAlarmWithName(name, delay, handler, arg);
}

/* SceThreadmgrForDriver_10517330: ksceKernelRegisterTimer with a 64-bit delay */
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
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	ThreadmgrTraceHook hook;
	ThreadmgrAlarm *alarm;
	int intr, ret;

	intr = ksceKernelCpuSuspendIntr();
	if (manager->timerCpuId == (int)ksceKernelCpuId() && manager->timerHandlingId == alarmId) {
		ret = SCE_KERNEL_ERROR_ALARM_BUSY;
		goto resume;
	}
	ret = ksceGUIDReferObjectWithClass(alarmId, manager->alarmClass,
					   (SceObjectBase **)&alarm);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_ALARM_ID;
		goto resume;
	}
	ksceKernelSpinlockLowLock(TIMER_LOCK());
	if (alarm->base.timer.deleted == 0) {
		alarm->base.timer.deleted = 1;
		if (s_threadmgrRuntime.manager->timerHandlingId == alarmId) {
			/* the interrupt handler closes it */
			ksceKernelSpinlockLowUnlock(TIMER_LOCK());
		} else {
			unlinkTimer(&alarm->base.timer);
			setNextTimer();
			ksceKernelSpinlockLowUnlock(TIMER_LOCK());
			if (ksceGUIDClose(alarmId) < 0)
				SCE_PANIC(SCE_LINE(3261, 3230));
		}
		ret = 0;
		hook = s_threadmgrRuntime.threadTraceHook;
		if (hook != NULL)
			hook(ALARM_HOOK_CANCEL, 1, alarmId);
	} else {
		ksceKernelSpinlockLowUnlock(TIMER_LOCK());
	}
	ksceGUIDReleaseObject(alarmId);
resume:
	ksceKernelCpuResumeIntr(intr);
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
	ThreadmgrAlarmInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->alarmClass, level,
					  NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->alarmClass, level, vector, num, &got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
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
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
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
		intr = ksceKernelCpuSuspendIntr();
		uid = ksceGUIDKernelCreate(s_threadmgrRuntime.manager->delayClass,
					   "SceKernelDelayObject", (SceObjectBase **)&delay);
		if (uid < 0) {
			ksceKernelCpuResumeIntr(intr);
			SCE_DBG_PRINTF(0, 3753,
				"*** error: uidDelay_CreateCache() -> sceGUIDKernelCreate()=0x%08X\n", uid);
			return uid;
		}
		ksceKernelSpinlockLowUnlock(&delay->base.event.lock);
		delay->base.event.ownerPid = THREADMGR_KERNEL_PID;
		ret = SceSysmemForKernel_FF464A49(uid, &delay->base.event.uidEntry);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 3766,
				"*** error: uidDelay_CreateCache() -> sceGUIDGetEntry(0x%08X)=0x%08X\n",
				uid, ret);
			if (ksceGUIDClose(uid) < 0)
				SCE_PANIC(SCE_LINE(3809, 3769));
		} else {
			SCE_DBG_ASSERT_CHECK(delay->base.event.uidEntry != NULL, 3774);
			manager = s_threadmgrRuntime.manager;
			ksceKernelSpinlockLowLock(&manager->delayCacheLock);
			delay->base.event.cacheNext = manager->delayCache;
			manager->delayCacheCount++;
			manager->delayCache = delay;
			if (manager->delayCacheCount >= target)
				more = false;
			ksceKernelSpinlockLowUnlock(&manager->delayCacheLock);
		}
		ksceKernelCpuResumeIntr(intr);
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
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
			&manager->delayCacheLock);
		if (manager->delayCacheCount <= target)
			break;
		delay = manager->delayCache;
		manager->delayCacheCount--;
		manager->delayCache = delay->base.event.cacheNext;
		ksceKernelSpinlockLowUnlock(&manager->delayCacheLock);
		if (ksceGUIDClose(SceSysmemForKernel_03A2CAF0(delay->base.event.uidEntry)) < 0)
			SCE_PANIC(SCE_LINE(3848, 3808));
		ksceKernelCpuResumeIntr(intr);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->delayCacheLock,
						 intr);
}

/* the cleaner thread keeps the delay cache between low and high */
int uidDelay_ManageCache(int high, int low, int target)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	int intr, count, ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->delayCacheLock);
	count = manager->delayCacheCount;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->delayCacheLock, intr);
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
