/* threadmgr.c */
#include "private.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "priority_change.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "timer.h"
#include <threadmgr/uid_waitable.h>
#include <string.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysroot.h>
#include <sysmem/sysroot_threadmgr.h>
#include <kernel/error.h>
#include "event.h"
#include "fast_mutex.h"
#include "event_flag.h"
#include "mutex.h"
#include "affinity.h"
#include <scetypes.h>
#include <sysmem/memblock.h>
#include <sysmem/spinlock.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/dipsw.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>
#include <acmgr/acmgr.h>
#include <sysmem/utils.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>

void issueChangeThreadPriorityIntrRequest(unsigned int cpuId, SceUID threadId, int priority);

/* SceThreadmgrForKernel_59D1D5B0: processmgr's process class */
int SceThreadmgrForKernel_59D1D5B0(SceClass *processClass)
{
	s_threadmgrRuntime.manager->processClass = processClass;
	return 0;
}

void *SceThreadmgrForKernel_565BD2DA(void)
{
	return (void *)s_threadmgrRuntime.manager->threadClass;
}

int sceKernelSetThreadMgrTestMode(void)
{
	if (s_threadmgrRuntime.manager->testMode)
		SCE_DBG_PRINTF(0, 11866, "error: sceKernelSetThreadMgrTestMode(), already set\n");
	s_threadmgrRuntime.manager->testMode = 1;
	return 0;
}

int sceKernelClearThreadMgrTestMode(void)
{
	if (!s_threadmgrRuntime.manager->testMode)
		SCE_DBG_PRINTF(0, 11876, "error: sceKernelClearThreadMgrTestMode(), not set\n");
	s_threadmgrRuntime.manager->testMode = 0;
	return 0;
}

int SceThreadmgrForDriver_5311CFB5(uintptr_t hook, uintptr_t arg)
{
	s_threadmgrRuntime.manager->processDebugSuspendHook = hook;
	s_threadmgrRuntime.manager->processDebugSuspendArg = arg;
	return 0;
}

#define WAIT_TYPE_SIGNAL		0x00000004
#define WAIT_ATTR_UNINTERRUPTIBLE	0x00008000	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_CAN_NOT_WAIT	0x80028004
#define SCE_KERNEL_ERROR_WAIT_TIMEOUT	0x80028005
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_THREAD_STOPPED	0x8002802c
#define SCE_KERNEL_ERROR_THREAD_SUSPENDED	0x8002802d
#define SCE_KERNEL_ERROR_SIGNAL_PENDING	0x80028121	/* PLACEHOLDER: name */
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */

typedef void (*ThreadmgrSchedSleepHandler)(const struct ThreadmgrSchedProbeDescriptor *probe);

int sceKernelCheckThreadUserStackSelf(void);
int callCallbackAll(struct ThreadCB *thread);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);

/* the __sched_:sleep probe of a signal wait */
static uint32_t s_signalSleepProbeId;
volatile uintptr_t s_signalSleepHandler;
extern const char s_signalSleepCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_signalSleepDescriptor = {
	.probeId = &s_signalSleepProbeId,
	.provider = "__sched_",
	.name = "sleep",
	.codeSite = s_signalSleepCodeSite,
	.handler = &s_signalSleepHandler,
};

/*
 * The running thread waits for a signal (arg0/arg1 are kept for the
 * debugger), optionally with a timeout and running callbacks.
 */
int waitSignal(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadmgrWaitableObject *signal = s_threadmgrRuntime.manager->signalObject;
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrSchedSleepHandler sleep;
	uint64_t wakeup;
	uint32_t saved, waiting;
	int intr, ret, again;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2727, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 2735, "*** error: irq suspended\n");
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	cpu = currentCpuState();
	if (cpu->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((int)thread->currentAttr < 0 && sceKernelCheckThreadUserStackSelf() == 0)
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	SceCpuForDriver_BF82DEB2(&signal->lock);
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&signal->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto resume;
			cpu = currentCpuState();
			SceCpuForDriver_BF82DEB2(&signal->lock);
		}
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
		if (thread->flags & THREAD_FLAG_SIGNALED) {
			/* the signal came first */
			ret = 0;
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNALED);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto unlock;
		}
		if ((thread->flags >> 28) != 0 &&
		    ((!(thread->flags & THREAD_FLAG_REQUEST_VM_SUSPEND) &&
		      (signal->attributes & WAIT_ATTR_UNINTERRUPTIBLE)) || ThreadmgrInSyscall())) {
			/* a pending request ends the wait */
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SIGNAL_DEFERRED);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_RESUMED_WAIT);
			switch (thread->flags & THREAD_FLAG_REQUEST_MASK) {
			case THREAD_FLAG_REQUEST_VM_SUSPEND:
			case THREAD_FLAG_REQUEST_SUSPEND:
				ret = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
				thread->waitResult = ret;
				break;
			case THREAD_FLAG_REQUEST_EXIT_DELETE:
			case THREAD_FLAG_REQUEST_EXIT:
				ret = SCE_KERNEL_ERROR_THREAD_STOPPED;
				thread->waitResult = ret;
				break;
			default:
				SCE_PANIC(SCE_LINE(2807, 2811));
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto unlock;
		}
		SCE_DBG_ASSERT_CHECK(thread->delayId == -1, 2821);
		SCE_DBG_ASSERT_CHECK(thread->timeoutOutput == 0, 2822);
		if (timeout == NULL) {
			thread->delayId = -1;
			thread->timeoutOutput = 0;
			if ((thread->flags & THREAD_FLAG_CALLBACK_WAKEUP) == THREAD_FLAG_CALLBACK_WAKEUP)
				goto cancelled;
		} else {
			ret = uidDelay_AllocObject(&wakeup, (uintptr_t)waitTimeoutHandler);
			if (ret < 0) {
				SCE_DBG_ASSERT_CHECK(ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT, 2827);
				thread->waitResult = ret;
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				*timeout = 0;
				goto unlock;
			}
			thread->delayId = ret;
			thread->remainingTimeLow = (uint32_t)wakeup;
			thread->remainingTimeHigh = (uint32_t)(wakeup >> 32);
			thread->timeoutOutput = (uintptr_t)timeout;
			if ((thread->flags & THREAD_FLAG_CALLBACK_WAKEUP) == THREAD_FLAG_CALLBACK_WAKEUP) {
				cancelDelayTimer(ret, NULL);
				thread->delayId = -1;
				thread->timeoutOutput = 0;
				goto cancelled;
			}
		}
		thread->waitType = WAIT_TYPE_SIGNAL;
		thread->waitId = s_threadmgrRuntime.manager->signalId;
		thread->waitResult = 0;
		SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 2861);
		thread->currentCpuId = -1;
		if (ThreadmgrInSyscall())
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAIT_MARK);
		if (callbacks)
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_WAKE_PENDING);
		thread->waitArg0 = arg0;
		thread->waitArg1 = arg1;
		waiting = ++signal->waiting;
		SCE_DBG_ASSERT_CHECK(waiting != 0, 2873);
		/* the DTrace probe site __sched_:sleep */
		__asm__ volatile(".global s_signalSleepCodeSite\ns_signalSleepCodeSite:");
		self->next = (struct ThreadmgrWaitNode *)&signal->next;
		self->prev = (struct ThreadmgrWaitNode *)signal->prev;
		thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_WAITING;
		signal->prev = (uintptr_t)self;
		self->prev->next = self;
		sceKernelSpinlockLowUnlock(&signal->lock);
		sleep = (ThreadmgrSchedSleepHandler)s_signalSleepHandler;
		if (sleep != NULL)
			sleep(&s_signalSleepDescriptor);
		SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 2885);
		cpu->nextRunningThread = 0;
		switchThread(2);
		SceCpuForDriver_BF82DEB2(&signal->lock);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = thread->waitResult;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		again = ret == (int)THREADMGR_ERROR_WAIT_CANCEL;
		goto next;
cancelled:
		ret = THREADMGR_ERROR_WAIT_CANCEL;
		thread->waitResult = ret;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		again = 1;
next:
		if (!callbacks)
			again = 0;
	} while (again);
	if (ret > 0)
		ret = 0;
unlock:
	sceKernelSpinlockLowUnlock(&signal->lock);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the kernel's waits for a signal */
int __attribute__((noinline)) sceKernelWaitSignal(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	return waitSignal(arg0, arg1, timeout, 0);
}

int __attribute__((noinline)) sceKernelWaitSignalCB(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	return waitSignal(arg0, arg1, timeout, 1);
}

/* sends threadId a signal: it wakes if it waits for one, else the signal stays pending */
int sendSignal(SceUID threadId)
{
	struct ThreadmgrWaitableObject *signal = s_threadmgrRuntime.manager->signalObject;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	uint32_t saved;
	int intr, ret, wake;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	if (SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	thread = &object->context;
	SceCpuForDriver_BF82DEB2(&signal->lock);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if ((thread->status & SCE_KERNEL_THREAD_STATUS_WAITING) &&
	    thread->waitId == s_threadmgrRuntime.manager->signalId) {
		wake = wakeupAndDistributeThread(thread, (uintptr_t)&signal->next, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		sceKernelSpinlockLowUnlock(&signal->lock);
		sceGUIDReleaseObject(threadId);
		ret = 0;
		if ((wake & 1) && currentThreadContext() != NULL)
			switchThread((intr >> 7) & 1);
	} else {
		if (!(thread->flags & THREAD_FLAG_SIGNALED)) {
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNALED);
			ret = 0;
		} else {
			ret = SCE_KERNEL_ERROR_SIGNAL_PENDING;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		sceKernelSpinlockLowUnlock(&signal->lock);
		sceGUIDReleaseObject(threadId);
	}
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* Native UID cache/waitable construction and class method foundations. */
/* UID-cache objects carry a link at +8, before the waitable base exists. */
struct ThreadmgrCacheObject {
	SceObjectBase base;
	uintptr_t next;
	uintptr_t prev;
};

_Static_assert(offsetof(struct ThreadmgrCacheObject, next) == 8,
	       "ThreadMgr cache link offset");

_Static_assert(sizeof(struct ThreadmgrWaitableClass) == 0x38,
	       "ThreadMgr extended class size");
_Static_assert(sizeof(struct ThreadmgrWaitableObject) == 0x28,
	       "ThreadMgr waitable object size");

/* the class a class extends */
static inline struct ThreadmgrWaitableClass *parentClass(void *cls)
{
	return (struct ThreadmgrWaitableClass *)((SceClass *)cls)->uidclass;
}

int uidCacheConstructor(void *object)
{
	volatile struct ThreadmgrCacheObject *cache = object;

	cache->next = 0;
	cache->prev = (uintptr_t)&cache->next;
	return 0;
}

int uidCacheDestructor(void *object)
{
	(void)object;
	return 0;
}

int uidWaitableConstructor(void *object)
{
	volatile struct ThreadmgrWaitableObject *waitable = object;

	waitable->lock = 1;
	waitable->next = (uintptr_t)&waitable->next;
	waitable->prev = (uintptr_t)&waitable->next;
	waitable->attributes = 0;
	waitable->reserved = 0;
	waitable->waiting = 0;
	return 0;
}

/* the default methods: the parent class's */
int uidDefaultCancelWait(void *cls, void *object, uint32_t a2)
{
	struct ThreadmgrWaitableClass *parent = parentClass(cls);

	return parent->cancelWait(parent, object, a2);
}

int uidWaitable_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread)
{
	volatile struct ThreadmgrWaitableObject *waitable = object;
#if FW_DEBUG
	if (SceDebugForDriver_374B7868() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 11612);
		if (SceDebugForDriver_374B7868() > 0) {
			SCE_DBG_ASSERT_LEVEL(1, waitable->lock != 0, 11615);
			if (SceDebugForDriver_374B7868() > 0)
				SCE_DBG_ASSERT_LEVEL(1, waitable->waiting != 0, 11618);
		}
	}
#endif
	(void)cls;
	/* Its wider meaning remains unknown; native release decrements this word. */
	waitable->waiting--;
	return 0;
}

int uidDefault_RevalidateMethod(void *cls, void *object)
{
	struct ThreadmgrWaitableObject *waitable = object;
	struct ThreadmgrWaitableClass *parent;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 11638);
	SCE_DBG_ASSERT_CHECK(waitable->lock != 0, 11641);
	parent = parentClass(cls);
	return parent->revalidate(parent, object);
}

int uidDefault_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread)
{
	struct ThreadmgrWaitableObject *waitable = object;
	struct ThreadmgrWaitableClass *parent;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 11655);
	SCE_DBG_ASSERT_CHECK(waitable->lock != 0, 11658);
	parent = parentClass(cls);
	return parent->release(parent, object, thread);
}

/* SceUIDWaitableClass: the waitable's queue and state cleared */
int uidWaitableDestructor(void *object)
{
	struct ThreadmgrWaitableObject *waitable = object;

	memset(&waitable->next, 0,
	       sizeof(*waitable) - offsetof(struct ThreadmgrWaitableObject, next));
	return 0;
}

int uidWaitable_RevalidateMethod(void *cls, void *object)
{
#if FW_DEBUG
	volatile struct ThreadmgrWaitableObject *waitable = object;
	if (SceDebugForDriver_374B7868() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 11595);
		if (SceDebugForDriver_374B7868() > 0)
			SCE_DBG_ASSERT_LEVEL(1, waitable->lock != 0, 11598);
	}
#else
	(void)object;
#endif
	(void)cls;
	return 0;
}

__attribute__((noinline, noreturn))
void uidCacheClassInit(void)
{
	SCE_PANIC(THREADMGR_UID_CACHE_CLASS_INIT_LINE);
}

int uidExtendedClassInit(struct ThreadmgrWaitableClass *cls, const char *name,
			SceClass *parent, SceSize itemsize,
			SceClassCallback create, SceClassCallback destroy,
			ThreadmgrCancelWaitMethod cancelWait, ThreadmgrRevalidateMethod revalidate,
			ThreadmgrReleaseMethod release)
{
	int result;

	result = SceSysmemForDriver_61317102(&cls->base, name, parent, itemsize,
				     create, destroy);
	if (result < 0)
		uidCacheClassInit();
	cls->cancelWait = cancelWait;
	cls->revalidate = revalidate;
	cls->release = release;
	return result;
}

/*
 * SceKernelThreadMgr (threadmgr.c): bringing the thread manager up.
 *
 * sceKernelThreadMgrModuleBoot runs on every CPU. CPU 0 builds the manager
 * (SceThreadMgrCBBlock), the UID classes, the cleaner thread, its interrupt,
 * exception and power handlers and every object kind; then each CPU sets up
 * its own state, its idle thread and the thread it is running on
 * ("SceKernelInitialThread_%d"). sceKernelThreadMgrStartAfterProcess,
 * called by sysroot once the kernel process exists, gives those threads
 * their register contexts in that process.
 */

#define CPU_COUNT			4
#define CPU_MASK_ALL			0xf
#define PARTITION_KERNEL		0x10009		/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_KERNEL		0x1020d006	/* PLACEHOLDER: name */
#define SWITCH_STACK_SIZE		0x1000		/* per CPU */
#define CB_BLOCK_SIZE			0x3000
#define CB_HEAP_SIZE			0xf000
#define THREAD_OBJECT_SIZE		0x200
#define GLOBAL_READY_QUEUES		0x7c
#define CPU_READY_QUEUES		0x100

#define CLEANER_THREAD_ID		0x10017		/* fixed UIDs */
#define IDLE_THREAD_ID(cpu)		(0x10019 + (cpu) * 2)
#define CLEANER_STACK_SIZE		0x1000
#define CLEANER_STACK_BLOCK		0x2000
#define CLEANER_PRIORITY		1
#define IDLE_PRIORITY			0xff
#define INITIAL_PRIORITY		0xfd
#define INITIAL_STACK_SIZE		0x3000
#define BOOST_PRIORITY_NONE		0xfe
#define THREAD_PERMISSION_KERNEL	0x80		/* PLACEHOLDER: name */
#define STACK_FILL			0xff

#define CACHE_OBJECT_SIZE		0x10
#define WAITABLE_OBJECT_SIZE		0x28
#define WAITABLE_CLASS_SIZE		0x38
#define CACHE_CLASS_SIZE		0x2c
#define THREAD_OBJECT_ITEM_SIZE		0x200
#define SIZEOF_THREAD_OBJECT		0x1d8	/* what the boot log reports */
#define SIZEOF_INTR_REQUEST		0x20

#define SGI_INTR_REQUEST		2	/* "SceThreadmgrSGI" */
#define SGI_STOP_CPUS			7	/* "SceThreadmgrSGILow" */
#define SGI_PRIORITY			0x30
#define SGI_LOW_PRIORITY		0x50
#define CPU_STOP_REQUEST		4

/* the priorities of threadmgr's excpmgr handlers */
#define EXCP_SLOT_FAULT			4	/* PLACEHOLDER: names */
#define EXCP_SLOT_DEBUG			6

#define SUSPEND_ORDER_THREADMGR		0x13	/* PLACEHOLDER: SceKernelSuspendForDriver's order */
#define SYSROOT_SLOT_THREADMGR		6	/* sceKernelSysrootSetModulePrivate */
#define DIPSW_THBP			0xe4
#define DBGDSCR_MDBGEN			0x8000	/* monitor debug-mode enable */
#define DACR_ALL_MANAGER		0xffff0000
#define CACHE_LINE			0x20

#define DACR_KERNEL			0x15450000
#define CPACR_VFP_FULL			0x00f00000
#define CPSR_SYS_ARM			0x5f
#define CPSR_SYS_THUMB			0x7f
#define REGISTER_CONTEXT_ALIGN		0x40
#define SYSCALL_RESTART_SLOT_SIZE	0x10
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define SCE_KERNEL_ERROR_NOT_DORMANT	0x80028028

/* the CPU states SceIntrmgrForKernel_04E635AA is given */
struct ThreadmgrCpuStates {
	struct ThreadMgrCpuCB *cpu[CPU_COUNT];
};

/* what the bootloader passes: per CPU, the stack it is running on */
struct ThreadmgrBootArgs {
	uint8_t reserved[0x200];
	struct {
		uint8_t reserved[0x1c];
		SceUID stackMemBlockId;		/* 0x1c */
	} cpu[CPU_COUNT];			/* 0x200 */
};

typedef int (*ThreadmgrSuspendHandler)(int event, void *arg);
extern int SceIntrmgrForKernel_9CB39268(void *handler);	/* before an interrupt handler */
extern int SceIntrmgrForKernel_46F04D82(void *handler);
extern int SceIntrmgrForKernel_764F140A(void *handler);	/* the next context after an interrupt */
extern int SceIntrmgrForKernel_04E635AA(struct ThreadmgrCpuStates *states);
extern int SceIntrmgrForKernel_E3B670BF(void *handler);	/* a thread's debug suspension */
extern int SceIntrmgrForKernel_BCD3BB05(void);
extern uint32_t SceIntrmgrForKernel_08FDB0CE(struct ThreadRegisterContext *context, uint32_t flags,
					     uintptr_t vfp);

/* excp_handlers.S */
void poisonVfpRegisters(void);
void idleThreadEntry(void);
extern SceExcpmgrHandler threadmgrAbortHandler, threadmgrPrefetchHandler, threadmgrUndefHandler;
extern SceExcpmgrHandler threadmgrDebugAbortHandler, threadmgrDebugPrefetchHandler;
extern SceExcpmgrHandler threadmgrDebugUndefHandler;

/* the rest of threadmgr */
extern uint8_t s_zeroBlock[0x80];
int uidThread_Constructor(struct ThreadmgrThreadObject *object);
int uidThread_Destructor(struct ThreadmgrThreadObject *object);
int uidThread_CancelWait(void *cls, void *uidObject, uint32_t a2);
void __attribute__((noreturn)) cleanerThread(void);
void kernelThreadReturnEntry(int status);
int syslibtraceThreadCBInit(struct ThreadCB *thread);
int dtraceThreadCBInit(struct ThreadCB *thread);
void initRegisterContext(struct ThreadRegisterContext *regs);
void initVfpContext(uintptr_t vfp);
int intrRequestSGIHandler(int code, void *arg);
void initIntrRequestPool(void);
void uidCacheClassInit(void) __attribute__((noreturn));
void startCpuTimer(struct ThreadMgrCpuCB *cpu);
void suspendCpuTimer(void);
void resumeCpuTimer(void);
void callThreadmgrBeforeIntrHandler(uintptr_t unused, struct ThreadCB *context);
int registerThreadmgrVfpExceptionIntrHandler(void);
void suspendThreadmgrVfpException(void);
void resumeThreadmgrVfpException(void);
void startFastMutex(void);
void startCallback(void);
void startThreadEvent(void);
void startEventFlag(void);
void startSemaphore(void);
void startMutex(void);
void startCond(void);
void startEvent(void);
int startMsgPipe(void);
void startLwMutex(void);
void startLwCond(void);
void startRWLock(void);
int startSimpleEvent(void);
void startWorkQueue(void);
void startException(void);
int waitThreadEnd(SceUID threadId, int *status, SceUInt32 *timeout, int callbacks);
int createGlobalWorkQueue(void);
int startMsgPipeAfterProcess(void);
int sceKernelTrapThreadAfterSyscall(ThreadSyscallFrame *frame, uint32_t *result,
				    uint32_t request);
void returnFromExcpToThread(void);
int sceKernelThreadMgrStartAfterProcess(void);

static const char *const s_initialThreadNames[CPU_COUNT] = {
	"SceKernelInitialThread_0",
	"SceKernelInitialThread_1",
	"SceKernelInitialThread_2",
	"SceKernelInitialThread_3",
};

static const char *const s_idleThreadNames[CPU_COUNT] = {
	"SceKernelIdleThread_0",
	"SceKernelIdleThread_1",
	"SceKernelIdleThread_2",
	"SceKernelIdleThread_3",
};

/* sysroot's view of the running thread: its process, or the kernel's outside a thread */
static SceProcessObject *getCurrentProcessCB(void)
{
	struct ThreadCB *thread = currentThreadContext();

	if (thread == NULL)
		return s_threadmgrRuntime.manager->kernelProcess;
	return thread->process;
}

static void *getCurrentAddressSpaceCB(void)
{
	struct ThreadCB *thread = currentThreadContext();

	if (thread == NULL)
		return s_threadmgrRuntime.manager->kernelAddressSpace;
	return thread->addressSpace;
}

static void *getCurrentUIDEntryHeapCB(void)
{
	struct ThreadCB *thread = currentThreadContext();

	if (thread == NULL)
		return s_threadmgrRuntime.manager->kernelUidEntryHeap;
	return thread->pUIDEntryHeap;
}

static const SysrootThreadmgrOps s_sysrootThreadmgrOps = {
	.size = sizeof(SysrootThreadmgrOps),
	.getThreadId = sceKernelGetThreadId,
	.isUserModeThread = sceKernelIsUserModeThread,
	.getThreadAccessLevel = SceThreadmgrForDriver_332E127C,
	.getCurrentThreadAttr = (int (*)(uint32_t *))SceThreadmgrForDriver_AB977C72,
	.getProcessId = sceKernelGetProcessId,
	.getCurrentProcessCB = (uint32_t (*)(void))getCurrentProcessCB,
	.getCurrentAddressSpaceCB = getCurrentAddressSpaceCB,
	.getCurrentUIDEntryHeapCB = getCurrentUIDEntryHeapCB,
	.returnFromExcpToThread = (void (*)(void *, void *))returnFromExcpToThread,
	.threadMgrStartAfterProcess = sceKernelThreadMgrStartAfterProcess,
	.trapThreadAfterSyscall = (int (*)(struct ThreadSyscallFrame *, SceUInt32 *, SceUInt32))
				  sceKernelTrapThreadAfterSyscall,
};

/* SceThreadmgrSGILow: this CPU holds still while another one stops the rest */
static int stopCpuIntrHandler(int code, void *arg)
{
	while (s_threadmgrRuntime.manager->cpuStopState == CPU_STOP_REQUEST)
		__asm__ volatile("wfe" : : : "memory");
	SceCpuForDriver_5F6A8743((SceInt32 *)&s_threadmgrRuntime.manager->cpuStopState, 1);
	return -1;
}

/* a power suspend and resume */
static int threadmgrSuspendHandler(int event, void *arg)
{
	suspendCpuTimer();
	suspendThreadmgrVfpException();
	return 0;
}

static int threadmgrResumeHandler(int event, void *arg)
{
	resumeThreadmgrVfpException();
	resumeCpuTimer();
	return 0;
}

/* sceKernelThreadMgrModuleBoot (module_bootstart): on every CPU */
int sceKernelThreadMgrModuleBoot(SceSize arglen, const struct ThreadmgrBootArgs *argp)
{
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrWaitableClass *waitableClass, *threadClass;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadmgrCpuStates states;
	SceClass *cacheClass;
	uintptr_t vbar, start, end;
	uint32_t dacr, dscr, mask;
	unsigned int cpuId;
	void *base;
	SceUID uid, block;
	int ret, i;

	cpuId = sceKernelCpuId();
	poisonVfpRegisters();
	if (SceDipswForDriver_A98FC2FD(DIPSW_THBP)) {
		/* no hardware breakpoints or watchpoints left over; monitor debug mode on */
		__asm__ volatile(
			"mcr p14, 0, %0, c0, c0, 4\n\tmcr p14, 0, %0, c0, c1, 4\n\t"
			"mcr p14, 0, %0, c0, c2, 4\n\tmcr p14, 0, %0, c0, c3, 4\n\t"
			"mcr p14, 0, %0, c0, c4, 4\n\tmcr p14, 0, %0, c0, c5, 4\n\t"
			"mcr p14, 0, %0, c0, c0, 5\n\tmcr p14, 0, %0, c0, c1, 5\n\t"
			"mcr p14, 0, %0, c0, c2, 5\n\tmcr p14, 0, %0, c0, c3, 5\n\t"
			"mcr p14, 0, %0, c0, c4, 5\n\tmcr p14, 0, %0, c0, c5, 5\n\t"
			"mcr p14, 0, %0, c0, c0, 6\n\tmcr p14, 0, %0, c0, c1, 6\n\t"
			"mcr p14, 0, %0, c0, c2, 6\n\tmcr p14, 0, %0, c0, c3, 6\n\t"
			"mcr p14, 0, %0, c0, c0, 7\n\tmcr p14, 0, %0, c0, c1, 7\n\t"
			"mcr p14, 0, %0, c0, c2, 7\n\tmcr p14, 0, %0, c0, c3, 7"
			: : "r"(0));
		__asm__ volatile("mrc p14, 0, %0, c0, c2, 2" : "=r"(dscr));
		if (!(dscr & DBGDSCR_MDBGEN))
			__asm__ volatile("mcr p14, 0, %0, c0, c2, 2" : : "r"(dscr | DBGDSCR_MDBGEN));
	}
	sceKernelSysrootCorelockLock(0);
	if (cpuId != 0)
		goto per_cpu;

	/* the manager */
	block = sceKernelAllocPartitionMemBlock(PARTITION_KERNEL, "SceThreadMgrCBBlock",
						 MEMBLOCK_TYPE_KERNEL, CB_BLOCK_SIZE, NULL);
	if (block < 0)
		SCE_PANIC(365);
	SceSysmemForDriver_B81CF0A3(block, (void **)&s_threadmgrRuntime.manager);
	sceKernelSysrootSetModulePrivate(SYSROOT_SLOT_THREADMGR, (void *)s_threadmgrRuntime.manager);
	SCE_DBG_PRINTF_LEVEL(2, 0, 370, " sizeof(SceKernelThreadmgrPrivate)=%d\n",
			     sizeof(struct SceKernelThreadmgrPrivate));
	SCE_DBG_PRINTF_LEVEL(2, 0, 371, "   gpThreadMgrCB=%p\n", s_threadmgrRuntime.manager);
	SCE_DBG_PRINTF_LEVEL(2, 0, 372, "   &gpThreadMgrCB->threadMgrCBLock=%p\n",
			     &s_threadmgrRuntime.manager->threadMgrCBLock);
	SCE_DBG_PRINTF_LEVEL(2, 0, 373, " sizeof(ThreadMgrCpuCB)=%d\n",
			     sizeof(struct ThreadMgrCpuCB));
	SCE_DBG_PRINTF_LEVEL(2, 0, 375,
		"   &cpuCB[0]=%p, &cpuCB[1]=%p, &cpuCB[2]=%p, &cpuCB[3]=%p\n",
		&s_threadmgrRuntime.manager->cpuCB[0], &s_threadmgrRuntime.manager->cpuCB[1],
		&s_threadmgrRuntime.manager->cpuCB[2], &s_threadmgrRuntime.manager->cpuCB[3]);
	SceSysrootForKernel_26458702(&s_sysrootThreadmgrOps);
	memset((void *)s_threadmgrRuntime.manager, 0, sizeof(struct SceKernelThreadmgrPrivate));
	manager = s_threadmgrRuntime.manager;
	manager->cbBlockId = block;
	manager->threadMgrCBLock = 0;
	manager->activeCpuMask = CPU_MASK_ALL;
	manager->delayCacheLock = 0;
	manager->delayCacheCount = 0;
	manager->delayCache = NULL;
	manager->reserved3 = 0;
	manager->reserved4 = 0;
	manager->reserved2 = 0;
	manager->deletedNext = (uintptr_t)&manager->deletedNext;
	manager->deletedPrev = (uintptr_t)&manager->deletedNext;
	manager->timedWaitNext = (uintptr_t)&manager->timedWaitNext;
	manager->timedWaitLock = 0;
	manager->timedWaitPrev = (uintptr_t)&manager->timedWaitNext;
	manager->dormantNext = (uintptr_t)&manager->dormantNext;
	manager->dormantPrev = (uintptr_t)&manager->dormantNext;
	manager->closedNext = (uintptr_t)&manager->closedNext;
	manager->closedPrev = (uintptr_t)&manager->closedNext;
	manager->reservedListNext = (uintptr_t)&manager->reservedListNext;
	manager->reservedListPrev = (uintptr_t)&manager->reservedListNext;
	manager->stagnantNext = (uintptr_t)&manager->stagnantNext;
	manager->stagnantPrev = (uintptr_t)&manager->stagnantNext;
	manager->debugSuspendedNext = (uintptr_t)&manager->debugSuspendedNext;
	manager->debugSuspendedPrev = (uintptr_t)&manager->debugSuspendedNext;
	manager->suspendedNext = (uintptr_t)&manager->suspendedNext;
	manager->suspendedPrev = (uintptr_t)&manager->suspendedNext;
	manager->heapId = SceSysmemForDriver_9328E0E8("SceKernelThreadMgrCBHeap", CB_HEAP_SIZE, NULL);
	if (s_threadmgrRuntime.manager->heapId < 0)
		SCE_PANIC(415);

	/* the UID classes under every object: cache, waitable, thread */
	cacheClass = SceSysmemForDriver_7B4CB60A(s_threadmgrRuntime.manager->heapId,
					       CACHE_CLASS_SIZE);
	if (cacheClass == NULL)
		SCE_PANIC(421);
	if (SceSysmemForDriver_61317102(cacheClass, "SceUIDCacheClass", SceSysmemForDriver_85336A1C(),
				  CACHE_OBJECT_SIZE, uidCacheConstructor, uidCacheDestructor) < 0)
		uidCacheClassInit();
	s_threadmgrRuntime.manager->cacheClass = (SceClass *)cacheClass;
	SCE_DBG_PRINTF_LEVEL(2, 0, 434, " sizeof(SceUIDCacheObject)=%d\n", CACHE_OBJECT_SIZE);
	waitableClass = SceSysmemForDriver_7B4CB60A(s_threadmgrRuntime.manager->heapId,
						  WAITABLE_CLASS_SIZE);
	if (waitableClass == NULL)
		SCE_PANIC(439);
	if (uidExtendedClassInit(waitableClass, "SceUIDWaitableClass",
				 s_threadmgrRuntime.manager->cacheClass,
				 WAITABLE_OBJECT_SIZE, uidWaitableConstructor, uidWaitableDestructor,
				 uidThread_CancelWait,
				 uidWaitable_RevalidateMethod,
				 uidWaitable_ReleaseMethod) < 0)
		SCE_PANIC(452);
	s_threadmgrRuntime.manager->waitableClass = (SceClass *)waitableClass;
	SCE_DBG_PRINTF_LEVEL(2, 0, 455, " sizeof(SceUIDWaitableObject)=%d\n", WAITABLE_OBJECT_SIZE);
	manager = s_threadmgrRuntime.manager;
	manager->signalId = SceSysmemForDriver_56A13E90(manager->waitableClass,
						 "SceUIDSignalObject",
						 (SceObjectBase **)&manager->signalObject);
	if (s_threadmgrRuntime.manager->signalId < 0)
		SCE_PANIC(461);
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->signalObject->lock);
	threadClass = SceSysmemForDriver_7B4CB60A(s_threadmgrRuntime.manager->heapId,
						WAITABLE_CLASS_SIZE);
	if (threadClass == NULL)
		SCE_PANIC(468);
	if (uidExtendedClassInit(threadClass, "SceUIDThreadClass",
				 s_threadmgrRuntime.manager->waitableClass,
				 THREAD_OBJECT_ITEM_SIZE, (SceClassCallback)uidThread_Constructor,
				 (SceClassCallback)uidThread_Destructor,
				 uidDefaultCancelWait,
				 uidDefault_RevalidateMethod,
				 uidDefault_ReleaseMethod) < 0)
		SCE_PANIC_FUNC(SCE_LINE(11682, 11687), "uidThreadClassInit");
	s_threadmgrRuntime.manager->threadClass = (SceClass *)threadClass;
	SCE_DBG_PRINTF_LEVEL(2, 0, 486, " sizeof(SceUIDThreadObject)=%d\n", SIZEOF_THREAD_OBJECT);
	s_threadmgrRuntime.manager->syslibtraceThreadCBEnable = 0;
	s_threadmgrRuntime.manager->dtraceThreadCBEnable = 0;

	/* the cleaner thread, waiting */
	object = SceSysmemForDriver_7B4CB60A(s_threadmgrRuntime.manager->heapId, THREAD_OBJECT_SIZE);
	if (object == NULL)
		SCE_PANIC(506);
	if (SceSysmemForKernel_BF275C5A(CLEANER_THREAD_ID,
					s_threadmgrRuntime.manager->threadClass,
					"SceKernelCleanerThread", object) < 0)
		SCE_PANIC(515);
	uidCacheConstructor(object);
	uidWaitableConstructor(object);
	uidThread_Constructor(object);
	thread = &object->context;
	thread->threadId = CLEANER_THREAD_ID;
	thread->stackMemBlockType = MEMBLOCK_TYPE_KERNEL;
	thread->processId = THREADMGR_KERNEL_PID;
	thread->kStackSize = CLEANER_STACK_SIZE;
	thread->kStackMemblkId = SceSysmemForKernel_40870E74("SceKernelCleanerThread",
							      CLEANER_STACK_BLOCK, CLEANER_STACK_SIZE);
	if (thread->kStackMemblkId < 0)
		SCE_PANIC(531);
	SceSysmemForDriver_0B1FD5C3(thread->kStackMemblkId, (void **)&thread->pKStack);
	memset((void *)thread->pKStack, STACK_FILL, thread->kStackSize);
	*(SceUID *)thread->pKStack = thread->threadId;
	thread->initAttr = 0;
	thread->currentAttr = 0;
	thread->ioFlags = 0;
	thread->syslibtraceState = NULL;
	thread->entry = (uintptr_t)cleanerThread;
	thread->exitStatus = SCE_KERNEL_ERROR_NOT_DORMANT;
	thread->magic = THREAD_CB_MAGIC;
	thread->initPriority = CLEANER_PRIORITY;
	thread->vfpException = 0x87;	/* PLACEHOLDER: meaning */
	thread->basePriority = CLEANER_PRIORITY;
	thread->currentPriority = CLEANER_PRIORITY;
	thread->initSp = thread->pKStack + thread->kStackSize;
	thread->boostPriority = BOOST_PRIORITY_NONE;
	thread->initCpuAffinityMask = CPU_MASK_ALL;
	thread->currentCpuAffinityMask = CPU_MASK_ALL;
	thread->currentCpuId = -1;
	thread->lastExecutedCpuId = -1;
	thread->changeCpuCount = -1;
	thread->userThreadId = -1;
	thread->waitId = -1;
	thread->delayId = -1;
	thread->initPermission = THREAD_PERMISSION_KERNEL;
	thread->permission = THREAD_PERMISSION_KERNEL;
	if (s_threadmgrRuntime.manager->syslibtraceThreadCBEnable)
		syslibtraceThreadCBInit(thread);
	thread->dtraceState = NULL;
	thread->dtraceLock = 0;
	if (s_threadmgrRuntime.manager->dtraceThreadCBEnable)
		dtraceThreadCBInit(thread);
	thread->kernelStackTop = thread->kStackSize + thread->pKStack;
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			 SCE_KERNEL_THREAD_STATUS_WAITING;
	s_threadmgrRuntime.manager->cleanerThread = thread;
	s_threadmgrRuntime.manager->reserved0 = 0;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(&object->waitLock);

	/* the interrupts, exceptions and power events */
	if (sceKernelRegisterIntrHandler(SGI_INTR_REQUEST, "SceThreadmgrSGI", 0,
					  intrRequestSGIHandler, (void *)s_threadmgrRuntime.manager,
					  SGI_PRIORITY, CPU_MASK_ALL, NULL) < 0)
		SCE_PANIC(587);
	SCE_DBG_PRINTF_LEVEL(2, 0, 589, " sizeof(IntrRequest)=%d\n", SIZEOF_INTR_REQUEST);
	initIntrRequestPool();
	if (sceKernelRegisterIntrHandler(SGI_STOP_CPUS, "SceThreadmgrSGILow", 0,
					  stopCpuIntrHandler, NULL, SGI_LOW_PRIORITY,
					  CPU_MASK_ALL, NULL) < 0)
		SCE_PANIC(599);
	SceIntrmgrForKernel_9CB39268(callThreadmgrBeforeIntrHandler);
	SceIntrmgrForKernel_46F04D82(SceThreadmgrForKernel_BB56D42F);
	SceIntrmgrForKernel_764F140A(setupNextContext_intr);
	for (i = 0; i < CPU_COUNT; i++)
		states.cpu[i] = &s_threadmgrRuntime.manager->cpuCB[i];
	SceIntrmgrForKernel_04E635AA(&states);
	startFastMutex();
	startCallback();
	startThreadEvent();
	startEventFlag();
	startSemaphore();
	startMutex();
	startCond();
	startEvent();
	startMsgPipe();
	startLwMutex();
	startLwCond();
	startRWLock();
	startSimpleEvent();
	startWorkQueue();
	startException();
	sceKernelRegisterExceptionHandler(SCE_EXCP_UNDEF, EXCP_SLOT_FAULT, &threadmgrUndefHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_PABT, EXCP_SLOT_FAULT, &threadmgrPrefetchHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_DABT, EXCP_SLOT_FAULT, &threadmgrAbortHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_UNDEF, EXCP_SLOT_DEBUG, &threadmgrDebugUndefHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_PABT, EXCP_SLOT_DEBUG, &threadmgrDebugPrefetchHandler);
	sceKernelRegisterExceptionHandler(SCE_EXCP_DABT, EXCP_SLOT_DEBUG, &threadmgrDebugAbortHandler);
	registerThreadmgrVfpExceptionIntrHandler();
	SceKernelSuspendForDriver_CE7A2207(SUSPEND_ORDER_THREADMGR, threadmgrSuspendHandler, NULL);
	SceKernelSuspendForDriver_105C5752(SUSPEND_ORDER_THREADMGR, threadmgrResumeHandler, NULL);
	s_threadmgrRuntime.manager->switchStackBlockId = sceKernelAllocPartitionMemBlock(
		PARTITION_KERNEL, "SceThreadMgrSwitchStackBlock", MEMBLOCK_TYPE_KERNEL,
		CPU_COUNT * SWITCH_STACK_SIZE, NULL);
	manager = s_threadmgrRuntime.manager;
	if (manager->switchStackBlockId < 0)
		SCE_PANIC(671);
	for (i = 0; i < GLOBAL_READY_QUEUES; i++) {
		manager->globalReadyQueue[i].next = (uintptr_t)&manager->globalReadyQueue[i];
		manager->globalReadyQueue[i].prev = (uintptr_t)&manager->globalReadyQueue[i];
	}
	manager->globalReadyLock = 0;
	SceIntrmgrForKernel_E3B670BF(debugSuspendThreadSelf);
	s_threadmgrRuntime.manager->cpuStopState = 0;
	s_threadmgrRuntime.manager->cpuStopOwner = 0;
	memset(s_zeroBlock, 0, sizeof(s_zeroBlock));

per_cpu:
	sceKernelSysrootCorelockUnlock();

	/* this CPU's state, found through VBAR+0x2c */
	cpu = &s_threadmgrRuntime.manager->cpuCB[cpuId];
	memset((void *)cpu, 0, sizeof(*cpu));
	__asm__ volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(vbar));
	__asm__ volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(dacr));
	__asm__ volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(DACR_ALL_MANAGER));
	start = (vbar + 0x2c) & ~(CACHE_LINE - 1);
	end = (vbar + 0x2c + sizeof(void *) + CACHE_LINE - 1) & ~(CACHE_LINE - 1);
	*(struct ThreadMgrCpuCB **)(vbar + 0x2c) = cpu;
	SceCpuForDriver_9CB9F0CE((void *)start, end - start);
	__asm__ volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(dacr));
	for (i = 0; i < CPU_READY_QUEUES; i++) {
		cpu->readyQueue[i].next = (uintptr_t)&cpu->readyQueue[i];
		cpu->readyQueue[i].prev = (uintptr_t)&cpu->readyQueue[i];
	}
	cpu->standbyNext = (uintptr_t)&cpu->standbyNext;
	cpu->standbyPrev = (uintptr_t)&cpu->standbyNext;
	cpu->standbyLock = 0;
	cpu->intrRequestHead = NULL;
	cpu->intrRequestMask = 0;
	startCpuTimer((struct ThreadMgrCpuCB *)cpu);

	/* its idle thread, ready */
	object = SceSysmemForDriver_7B4CB60A(s_threadmgrRuntime.manager->heapId, THREAD_OBJECT_SIZE);
	if (object == NULL)
		SCE_PANIC(728);
	ret = SceSysmemForKernel_BF275C5A(IDLE_THREAD_ID(cpuId),
					  s_threadmgrRuntime.manager->threadClass,
					  s_idleThreadNames[cpuId], object);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 737, "ret = %08X\n", ret);
		SCE_PANIC(738);
	}
	uidCacheConstructor(object);
	uidWaitableConstructor(object);
	uidThread_Constructor(object);
	thread = &object->context;
	mask = (1 << cpuId) & CPU_MASK_ALL;
	thread->processId = THREADMGR_KERNEL_PID;
	thread->boostPriority = BOOST_PRIORITY_NONE;
	thread->initCpuAffinityMask = mask;
	thread->entry = (uintptr_t)idleThreadEntry;
	thread->exitStatus = SCE_KERNEL_ERROR_NOT_DORMANT;
	thread->vfpException = 0x87;
	thread->threadId = IDLE_THREAD_ID(cpuId);
	thread->currentCpuAffinityMask = mask;
	thread->kStackSize = 0;
	thread->stackMemBlockType = 0;
	thread->initAttr = 0;
	thread->currentAttr = 0;
	thread->currentCpuId = cpuId;
	thread->initSp = 0;
	thread->ioFlags = 0;
	thread->syslibtraceState = NULL;
	thread->magic = THREAD_CB_MAGIC;
	thread->kStackMemblkId = -1;
	thread->lastExecutedCpuId = -1;
	thread->changeCpuCount = -1;
	thread->userThreadId = -1;
	thread->waitId = -1;
	thread->delayId = -1;
	thread->initPriority = IDLE_PRIORITY;
	thread->basePriority = IDLE_PRIORITY;
	thread->currentPriority = IDLE_PRIORITY;
	thread->initPermission = THREAD_PERMISSION_KERNEL;
	thread->permission = THREAD_PERMISSION_KERNEL;
	if (s_threadmgrRuntime.manager->syslibtraceThreadCBEnable)
		syslibtraceThreadCBInit(thread);
	thread->dtraceState = NULL;
	thread->dtraceLock = 0;
	if (s_threadmgrRuntime.manager->dtraceThreadCBEnable)
		dtraceThreadCBInit(thread);
	thread->kernelStackTop = thread->kStackSize + thread->pKStack;
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			 SCE_KERNEL_THREAD_STATUS_READY;
	addReadyTop(thread);
	cpu->idleThread = thread;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(&object->waitLock);

	/* the thread this CPU runs on now, on the bootloader's stack */
	uid = SceSysmemForDriver_56A13E90(s_threadmgrRuntime.manager->threadClass,
				   s_initialThreadNames[cpuId], (SceObjectBase **)&object);
	if (uid < 0)
		SCE_PANIC(803);
	thread = &object->context;
	thread->threadId = uid;
	thread->processId = THREADMGR_KERNEL_PID;
	thread->kStackSize = INITIAL_STACK_SIZE;
	thread->stackMemBlockType = MEMBLOCK_TYPE_KERNEL;
	thread->kStackMemblkId = argp->cpu[cpuId].stackMemBlockId;
	ret = SceSysmemForDriver_0B1FD5C3(thread->kStackMemblkId, (void **)&thread->pKStack);
	*(SceUID *)thread->pKStack = thread->threadId;
	thread->boostPriority = BOOST_PRIORITY_NONE;
	thread->initCpuAffinityMask = mask;
	thread->exitStatus = SCE_KERNEL_ERROR_NOT_DORMANT;
	thread->lastExecutedCpuId = cpuId;
	thread->kernelStackTop = thread->kStackSize + thread->pKStack;
	thread->initAttr = 0;
	thread->currentAttr = 0;
	thread->currentCpuAffinityMask = mask;
	thread->currentCpuId = cpuId;
	thread->changeCpuCount = 0;
	thread->ioFlags = 0;
	thread->syslibtraceState = NULL;
	thread->vfpException = 0x87;
	thread->magic = THREAD_CB_MAGIC;
	thread->initPriority = INITIAL_PRIORITY;
	thread->basePriority = INITIAL_PRIORITY;
	thread->currentPriority = INITIAL_PRIORITY;
	thread->initPermission = THREAD_PERMISSION_KERNEL;
	thread->permission = THREAD_PERMISSION_KERNEL;
	thread->userThreadId = -1;
	thread->waitId = -1;
	thread->delayId = -1;
	if (s_threadmgrRuntime.manager->syslibtraceThreadCBEnable)
		syslibtraceThreadCBInit(thread);
	thread->dtraceState = NULL;
	thread->dtraceLock = 0;
	if (s_threadmgrRuntime.manager->dtraceThreadCBEnable)
		dtraceThreadCBInit(thread);
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			 SCE_KERNEL_THREAD_STATUS_RUNNING;
	cpu->runningThread = (uintptr_t)thread;
	cpu->nextRunningThread = (uintptr_t)thread;
	s_threadmgrRuntime.manager->initialThreadId[cpuId] = thread->threadId;
	cpu->runningProcessId = thread->processId;
	cpu->runningThreadId = thread->threadId;
	cpu->processId = thread->processId;
	cpu->threadId = thread->threadId;
	cpu->priority = thread->currentPriority;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(&object->waitLock);
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 4" : : "r"(thread) : "memory");
	SceSysmemForDriver_B81CF0A3(s_threadmgrRuntime.manager->switchStackBlockId, &base);
	cpu->switchStackTop = (uintptr_t)base + (cpuId + 1) * SWITCH_STACK_SIZE;
	return ret;
}

int module_bootstart(SceSize arglen, const void *argp)
	__attribute__((alias("sceKernelThreadMgrModuleBoot")));

/* a kernel thread's register context in the kernel process, entering at its entry */
static inline int initKernelThreadRegisters(struct ThreadCB *thread,
					    MMUContext *context, SceAllocOpt *opt)
{
	struct ThreadRegisterContext *regs;

	opt->size = sizeof(*opt);
	opt->align = REGISTER_CONTEXT_ALIGN;
	regs = SceSysrootForKernel_D351EBC8(THREADMGR_KERNEL_PID, sizeof(*regs), opt);
	thread->pRegisterContext = regs;
	if (regs == NULL)
		return -1;
	initRegisterContext(regs);
	regs->pc = thread->entry;
	regs->cpsr = (thread->entry & 1) ? CPSR_SYS_THUMB : CPSR_SYS_ARM;
	regs->sp = thread->initSp;
	regs->lr = (uintptr_t)kernelThreadReturnEntry;
	regs->ttbr1 = context->ttbr1;
	regs->kernelDacr = DACR_KERNEL;
	regs->contextidr = (uint8_t)context->contextId | thread->threadId << 8;
	return 0;
}

/* the thread goes on its process's thread list */
static inline void listProcessThread(SceProcessObject *process,
				     struct ThreadCB *thread)
{
	THREAD_OBJECT(thread)->ownerPid = THREADMGR_KERNEL_PID;
	thread->processLink.next = &process->threads;
	thread->processLink.prev = process->threads.prev;
	process->threads.prev = &thread->processLink;
	thread->processLink.prev->next = &thread->processLink;
	if ((uint32_t)++process->threadCount > process->threadPeak)
		process->threadPeak = process->threadCount;
}

/*
 * sceKernelThreadMgrStartAfterProcess: once the kernel process exists, on
 * every CPU. Its threads join the kernel process; CPU 0 then waits for the
 * other initial threads to end and starts what needs threads.
 */
int sceKernelThreadMgrStartAfterProcess(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread;
	SceProcessObject *process;
	MMUContext *context;
	SceAllocOpt opt;
	void *addressSpace, *heap;
	unsigned int cpuId;
	int ret, i;

	cpuId = sceKernelCpuId();
	process = SceSysmemForKernel_FA6C098C(THREADMGR_KERNEL_PID);
	addressSpace = SceSysmemForKernel_FA6C098C(SceSysrootForKernel_7334F1E8(THREADMGR_KERNEL_PID));
	ret = SceSysrootForKernel_88DE85EF(THREADMGR_KERNEL_PID, &heap);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(963);
		return ret;
	}
	ret = SceSysmemForDriver_2ECF7944(THREADMGR_KERNEL_PID, &context);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(969);
		return ret;
	}
	sceKernelSysrootCorelockLock(0);
	if (cpuId == 0) {
		/* the cleaner thread */
		thread = s_threadmgrRuntime.manager->cleanerThread;
		thread->process = process;
		thread->addressSpace = addressSpace;
		thread->pUIDEntryHeap = heap;
		thread->tlsProcessId = THREADMGR_KERNEL_PID;
		if (initKernelThreadRegisters(thread, context, &opt) < 0) {
			SCE_DBG_ASSERT_FAIL(995);
			return SCE_KERNEL_ERROR_NO_MEMORY;
		}
		thread->pVfpRegisterContext = (uintptr_t)SceSysrootForKernel_D351EBC8(THREADMGR_KERNEL_PID,
			sizeof(struct ThreadmgrVfpContext), &opt);
		if (thread->pVfpRegisterContext == 0) {
			SCE_DBG_ASSERT_FAIL(1027);
			return SCE_KERNEL_ERROR_NO_MEMORY;
		}
		initVfpContext(thread->pVfpRegisterContext);
		thread->pRegisterContext->cpacr = CPACR_VFP_FULL;
		s_threadmgrRuntime.manager->kernelProcess = process;
		s_threadmgrRuntime.manager->kernelAddressSpace = addressSpace;
		s_threadmgrRuntime.manager->kernelUidEntryHeap = heap;
		listProcessThread(process, thread);
		s_threadmgrRuntime.manager->signalObject->ownerPid = THREADMGR_KERNEL_PID;
	}

	/* the idle thread */
	thread = cpu->idleThread;
	thread->tlsProcessId = THREADMGR_KERNEL_PID;
	thread->process = process;
	thread->addressSpace = addressSpace;
	thread->pUIDEntryHeap = heap;
	if (initKernelThreadRegisters(thread, context, &opt) < 0) {
		SCE_DBG_ASSERT_FAIL(1065);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	listProcessThread(process, thread);

	/* the initial thread, running now: the kernel process's MMU context from here on */
	thread = (struct ThreadCB *)cpu->runningThread;
	thread->process = process;
	thread->pUIDEntryHeap = heap;
	thread->tlsProcessId = THREADMGR_KERNEL_PID;
	thread->addressSpace = addressSpace;
	opt.size = sizeof(opt);
	opt.align = REGISTER_CONTEXT_ALIGN;
	thread->pRegisterContext = SceSysrootForKernel_D351EBC8(THREADMGR_KERNEL_PID,
						       sizeof(*thread->pRegisterContext), &opt);
	if (thread->pRegisterContext == NULL) {
		SCE_DBG_ASSERT_FAIL(1115);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	initRegisterContext(thread->pRegisterContext);
	thread->pRegisterContext->userDacr = 0;
	thread->pRegisterContext->ttbr1 = context->ttbr1;
	thread->pRegisterContext->kernelDacr = DACR_KERNEL;
	thread->pRegisterContext->contextidr = (uint8_t)context->contextId | thread->threadId << 8;
	__asm__ volatile(
		"dsb\n\t"
		"mcr p15, 0, %0, c13, c0, 1\n\t"	/* CONTEXTIDR: none while TTBR changes */
		"isb\n\t"
		"mcr p15, 0, %1, c2, c0, 1\n\t"		/* TTBR1 */
		"isb\n\t"
		"dsb\n\t"
		"mcr p15, 0, %2, c13, c0, 1\n\t"
		"mcr p15, 0, %3, c3, c0, 0"		/* DACR */
		: : "r"(0), "r"(context->ttbr1),
		    "r"((uint8_t)context->contextId | thread->threadId << 8), "r"(DACR_KERNEL)
		: "memory");
	opt.size = sizeof(opt);
	opt.align = REGISTER_CONTEXT_ALIGN;
	thread->pVfpRegisterContext = (uintptr_t)SceSysrootForKernel_D351EBC8(THREADMGR_KERNEL_PID,
		sizeof(struct ThreadmgrVfpContext), &opt);
	if (thread->pVfpRegisterContext == 0) {
		SCE_DBG_ASSERT_FAIL(1141);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	initVfpContext(thread->pVfpRegisterContext);
	listProcessThread(process, thread);
	cpu->processId = THREADMGR_KERNEL_PID;
	cpu->runningProcessId = THREADMGR_KERNEL_PID;
	sceKernelSysrootCorelockUnlock();
	SceIntrmgrForKernel_BCD3BB05();

	/* CPU 0 goes on alone */
	if (cpuId == 0) {
		for (i = 1; i < CPU_COUNT; i++)
			waitThreadEnd(s_threadmgrRuntime.manager->initialThreadId[i], NULL, NULL, 0);
	} else {
		sceKernelExitDeleteThread(0);
	}
	ret = createGlobalWorkQueue();
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(1186);
		return ret;
	}
	ret = startMsgPipeAfterProcess();
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(1193);
		return ret;
	}
	ret = SceThreadmgrForDriver_B645C7EF(SYSCALL_RESTART_SLOT_SIZE, NULL, 0);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(1200);
		return ret;
	}
	s_threadmgrRuntime.manager->syscallRestartSlot = ret;
	return 0;
}

/* back from an exception the debugger or the VM resolved: the thread runs on */
void returnFromExcpToThread(void)
{
	struct ThreadCB *thread = currentThreadContext();

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 11218);
	SCE_DBG_ASSERT_LEVEL(0, thread != NULL, 11220);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	setupNextContext_intr(0, 8);
	thread = currentThreadContext();
	SceIntrmgrForKernel_08FDB0CE(thread->pRegisterContext, 0, thread->pVfpRegisterContext);
	__builtin_unreachable();
}

/* SceKernelThreadMgr: the running thread's context queries and setters. */
SceUID sceKernelGetThreadId(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->threadId : SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

SceUID sceKernelGetProcessId(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->processId : 0x10005;
}

int SceThreadmgrForDriver_332E127C(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->permission : SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

int sceKernelIsUserModeThread(void)
{
	struct ThreadCB *thread;
	SceUInt32 state;
	int ret;

	state = sceSyscallEnter();
	if (!currentThreadContext()) {
		sceSyscallLeave(state);
		return 0;
	}
	thread = currentThreadContext();
	SCE_DBG_ASSERT_CHECK(thread != NULL, 7596);
	ret = thread->currentAttr >> 31;
	sceSyscallLeave(state);
	return ret;
}

/* SceThreadmgrForDriver_AB977C72: the calling thread's attributes */
int SceThreadmgrForDriver_AB977C72(SceUInt32 *attributes)
{
	struct ThreadCB *thread = currentThreadContext();
	SceUInt32 state;
	int intr, ret;

	state = sceSyscallEnter();
	if (attributes == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (currentThreadContext() == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else {
		intr = SceCpuForDriver_821FC0EE();
		SceCpuForDriver_BF82DEB2(&thread->lock);
		*attributes = thread->currentAttr;
		sceKernelSpinlockLowUnlock(&thread->lock);
		sceKernelCpuResumeIntr(intr);
		ret = 0;
	}
	sceSyscallLeave(state);
	return ret;
}

int sceKernelGetThreadCurrentPriority(void)
{
	struct ThreadCB *thread;
	SceUInt32 state;
	int priority;

	state = sceSyscallEnter();
	if (!currentThreadContext()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 6389, "sceKernelGetThreadCurrentPriority",
					   "*** error: illegal context\n");
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	thread = currentThreadContext();
	priority = thread->currentPriority;
	sceSyscallLeave(state);
	return priority;
}

int sceKernelCheckWaitableStatus(void)
{
	SceKernelIntrStatus state;
	int result = 0;

	if (!currentThreadContext())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	state = SceCpuForDriver_821FC0EE();
	if ((state & 0x80) || currentCpuState()->disabledDispatch)
		result = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	sceKernelCpuResumeIntr(state);
	return result;
}

void *SceThreadmgrForDriver_E938FB20(unsigned int index)
{
	struct ThreadCB *thread = currentThreadContext();
	uintptr_t tls;

	if (index > 0xff || !thread)
		return NULL;
	tls = thread->tls;
	if (!tls)
		return NULL;
	return (void *)(tls - 0x800 + index * 4);
}

int SceThreadmgrForDriver_20C228E4(void)
{
	if (!currentThreadContext())
		return 0;
	return (currentThreadContext()->flags >> 15) & 1;
}

int sceKernelSetThreadAccessLevel(int permission)
{
	struct ThreadCB *thread = currentThreadContext();
	int previous;

	if (permission < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!thread)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	previous = thread->permission;
	thread->permission = permission;
	return previous;
}

SceUID SceThreadmgrForDriver_FA54D49A(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->tlsProcessId : SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

SceUID SceThreadmgrForDriver_0486F239(SceUID pid)
{
	struct ThreadCB *thread = currentThreadContext();
	SceUID previous;

	if (pid < 0)
		return SCE_KERNEL_ERROR_INVALID_UID;
	if (!thread)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	previous = thread->tlsProcessId;
	thread->tlsProcessId = pid;
	return previous;
}

int SceThreadmgrForDriver_56C18B11(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->ioFlags : SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

int SceThreadmgrForDriver_84EB1EA4(int value)
{
	struct ThreadCB *thread = currentThreadContext();
	int previous;

	if (value < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!thread)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	previous = thread->ioFlags;
	thread->ioFlags = value;
	return previous;
}

/*
 * SceKernelThreadMgr (threadmgr.c): the life of a thread after it is
 * created. threadEntry is where a new thread first runs; it ends through
 * sceKernelExitThread / sceKernelExitDeleteThread (exitDeleteThread), is
 * deleted by another thread (deleteThread) or by the cleaner thread, and
 * its UID object's destructor frees what is left.
 */

#define THREAD_ATTR_USER		0x80000000
#define THREAD_ATTR_VM			0x04000000	/* PLACEHOLDER: a VM's thread */
#define THREAD_ATTR_UNK4000		0x00004000	/* PLACEHOLDER: counted apart in its process */
#define THREAD_EVENT_START		0x00000004	/* callThreadEventHandlers */
#define TRAP_REQUEST_DEBUG_SUSPEND	0x02000000
#define DEBUG_THREAD_EVENT_DELETE	1
#define TRACE_EVENT_THREAD_DELETE	1
#define DIPSW_FREE_RETRY_LOG		0x9f
#define FREE_RETRY_LIMIT		100000
#define FREE_RETRY_LOG_EVERY		1000
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006
#define SCE_KERNEL_ERROR_WAIT_CANCEL	0x80028007
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
int callThreadEventHandlers(uint32_t mask);
void __attribute__((noreturn)) exitDeleteThread(int delete, int intr, int status);
int releaseThreadTlsSlots(struct ThreadCB *thread);
int dtraceThreadCBExit(struct ThreadCB *thread);
int syslibtraceThreadCBExit(struct ThreadCB *thread);

/* the thread goes from the list it is on to the manager's closed list */
static inline void listClosed(struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread, *head;

	self->next->prev = self->prev;
	self->prev->next = self->next;
	head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->closedNext;
	self->next = head;
	self->prev = head->prev;
	head->prev = self;
	self->prev->next = self;
}

/* the thread leaves its process */
static inline void unlistFromProcess(struct ThreadCB *thread,
				     SceProcessObject *process)
{
	ListNode *next = thread->processLink.next;

	next->prev = thread->processLink.prev;
	thread->processLink.prev->next = next;
	if (thread->currentAttr & THREAD_ATTR_UNK4000)
		process->unlimitedThreadCount--;
	else
		process->threadCount--;
}

/*
 * Where a kernel thread's entry function returns to: the dispatch is
 * enabled again and the thread exits with the returned status.
 */
void kernelThreadReturn(int status)
{
	struct ThreadMgrCpuCB *cpu;
	int intr;

	intr = SceCpuForDriver_821FC0EE();
	cpu = currentCpuState();
	if (cpu->disabledDispatch != 0)
		cpu->disabledDispatch = 0;
	sceKernelCpuResumeIntr(intr);
	sceKernelExitThread(status);
}

/*
 * A new thread's first code: the thread event handlers, then its entry
 * function (a kernel thread) or its user context (a user thread).
 */
void threadEntry(void)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadRegisterContext *regs, *context;
	uintptr_t entry, vfp;
	uint32_t request;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 4821);
	if ((int)thread->currentAttr < 0)
		goto again;
	for (;;) {
		SCE_DBG_ASSERT_CHECK(thread->userFrame == NULL, 4827);
again:
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_IN_CALLBACK);
		callThreadEventHandlers(THREAD_EVENT_START);
		if ((int)thread->currentAttr < 0)
			break;
		/* a kernel thread: its entry function with the start argument */
		regs = thread->pRegisterContext;
		entry = thread->entry;
		regs->full = 1;
		regs->r[1] = thread->pArgBlock;
		regs->r[0] = thread->argSize;
		regs->pc = entry & ~1;
		regs->sp = thread->initSp;
		regs->lr = (uintptr_t)kernelThreadReturnEntry;
		regs->cpsr = (entry & 1) ? CPSR_SYS_THUMB : CPSR_SYS_ARM;
		SceIntrmgrForKernel_08FDB0CE(regs, 0, thread->pVfpRegisterContext);
		SCE_DBG_ASSERT_CHECK((thread->flags >> 16) & 1, 4825);
	}
	/* a user thread: the context _sceKernelStartThread prepared */
	thread->userFrame = NULL;
	thread->kernelStackTop = thread->pKStack + thread->kStackSize;
	vfp = thread->pRegisterContext->callbackVfp;
	context = thread->pRegisterContext->callbackContext;
	thread->pRegisterContext->callbackVfp = 0;
	thread->pRegisterContext->callbackContext = NULL;
	request = thread->flags & THREAD_FLAG_REQUEST_MASK;
	if (request == 0) {
		if (thread->debugSuspendReason == 0)
			goto load;
		request = TRAP_REQUEST_DEBUG_SUSPEND;
	}
	for (;;) {
		sceKernelTrapThreadAfterSyscall(NULL, NULL, request);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
					    THREAD_FLAG_RESUMED_WAIT);
load:
		request = SceIntrmgrForKernel_08FDB0CE(context, 0, vfp);
	}
}

/* sceKernelExitThread: the running thread ends with status */
int sceKernelExitThread(int status)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadMgrCpuCB *cpu;
	uintptr_t current;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	current = (uintptr_t)currentThreadContext();
	if (current == 0) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 5089, "sceKernelExitThread",
					   "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = SceCpuForDriver_821FC0EE();
	cpu = currentCpuState();
	if (thread->threadEventMask != 0) {
		SCE_PRINTF_WITH_INFO_FUNC(0, SCE_LINE(5097, 5103), "sceKernelExitThread",
			"*** error: illegal context(thread event handler). threadId=0x%08x[%s]\n",
			currentThreadContext()->threadId,
			SceSysmemForDriver_E655852F(currentThreadContext()->threadId));
		if ((int)thread->currentAttr >= 0)
			SCE_PANIC_FUNC(SCE_LINE(5104, 5110), "sceKernelExitThread");
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(NULL, NULL, TRAP_REQUEST_DEBUG_SUSPEND);
	}
	if (thread->callbackNest != 0) {
		SCE_PRINTF_WITH_INFO_FUNC(0, SCE_LINE(5112, 5118), "sceKernelExitThread",
			"*** error: illegal context(callback). threadId=0x%08x[%s]\n",
			currentThreadContext()->threadId,
			SceSysmemForDriver_E655852F(currentThreadContext()->threadId));
		if ((int)thread->currentAttr >= 0)
			goto panic;
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(NULL, NULL, TRAP_REQUEST_DEBUG_SUSPEND);
	}
	if (cpu->disabledDispatch != 0) {
		sceKernelCpuResumeIntr(intr);
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}
	exitDeleteThread(0, intr, status);
panic:
	SCE_PANIC_FUNC(SCE_LINE(5119, 5125), "sceKernelExitThread");
}

/* sceKernelExitDeleteThread: the running thread ends with status and is deleted */
int sceKernelExitDeleteThread(int status)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadMgrCpuCB *cpu;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 5168, "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = SceCpuForDriver_821FC0EE();
	cpu = currentCpuState();
	if (thread->threadEventMask != 0) {
		SCE_PRINTF_WITH_INFO(0, SCE_LINE(5176, 5182),
			"*** error: illegal context(thread event handler). threadId=0x%08x[%s]\n",
			currentThreadContext()->threadId,
			SceSysmemForDriver_E655852F(currentThreadContext()->threadId));
		if ((int)thread->currentAttr >= 0)
			SCE_PANIC(SCE_LINE(5183, 5189));
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(NULL, NULL, TRAP_REQUEST_DEBUG_SUSPEND);
	}
	if (thread->callbackNest != 0) {
		SCE_PRINTF_WITH_INFO(0, SCE_LINE(5191, 5197),
			"*** error: illegal context(callback). threadId=0x%08x[%s]\n",
			currentThreadContext()->threadId,
			SceSysmemForDriver_E655852F(currentThreadContext()->threadId));
		if ((int)thread->currentAttr >= 0)
			goto panic;
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(NULL, NULL, TRAP_REQUEST_DEBUG_SUSPEND);
	}
	if (cpu->disabledDispatch != 0) {
		sceKernelCpuResumeIntr(intr);
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}
	exitDeleteThread(1, intr, status);
panic:
	SCE_PANIC(SCE_LINE(5198, 5204));
}

/* deletes a dormant thread (its process's thread list and its own lock held) */
int deleteThread(struct ThreadCB *thread)
{
	SceProcessObject *process = thread->process;
	SceUID processId = thread->processId, threadId = thread->threadId;
	ThreadmgrTraceHook trace;
	int ret;

	if (!(thread->status & (SCE_KERNEL_THREAD_STATUS_DORMANT | SCE_KERNEL_THREAD_STATUS_DEAD))) {
		ret = SCE_KERNEL_ERROR_NOT_DORMANT;
		goto out;
	}
	releaseThreadTlsSlots(thread);
	thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_DELETED;
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 4279);
	thread->currentCpuId = -1;
	if (thread->dtraceState != NULL)
		dtraceThreadCBExit(thread);
	if (thread->syslibtraceState != NULL)
		syslibtraceThreadCBExit(thread);
	SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
	listClosed(thread);
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	if (thread->userThreadId != -1) {
		ret = SceSysmemForDriver_84A4AF5E(processId, thread->userThreadId);
		if (ret < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 4310);
			goto out;
		}
	}
	unlistFromProcess(thread, process);
	ret = sceGUIDClose(thread->threadId);
	if (ret < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 4329);
		goto out;
	}
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_THREAD_DELETE, 2, threadId, processId);
out:
	return ret;
}

/* sceKernelDeleteThread: deletes another, dormant, thread */
int sceKernelDeleteThread(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrDebugThreadEvent message;
	SceProcessObject *process;
	ThreadmgrDebugHandler handler;
	SceUID processId, userThreadId;
	uint32_t saved;
	int intr, ret, group;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 4356, "sceKernelDeleteThread",
					   "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (currentThreadContext()->threadEventMask != 0) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 4364, "sceKernelDeleteThread",
			"*** error: illegal context(thread event handler). threadId=0x%08x[%s]\n",
			threadId, SceSysmemForDriver_E655852F(threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (threadId == 0 || currentThreadContext()->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = ThreadmgrInSyscall() ?
			(int)(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT) :
			(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	userThreadId = object->context.userThreadId;
	process = object->context.process;
	processId = process->pid;
	SceCpuForDriver_BF82DEB2(&process->lock);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(&object->context));
	ret = deleteThread(&object->context);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceKernelSpinlockLowUnlock(&process->lock);
	sceGUIDReleaseObject(threadId);
	if (ret >= 0) {
		/* the debugger hears of the deletion */
		for (group = 0; group < 2; group++) {
			handler = s_threadmgrRuntime.manager->
				debugHandlers[group][0];
			if (handler == NULL)
				continue;
			message.processId = processId;
			message.threadId = userThreadId;
			message.event = DEBUG_THREAD_EVENT_DELETE;
			handler(sizeof(message), &message);
		}
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * The cleaner thread: deletes the threads exitDeleteThread left on the
 * deleted list, and sleeps when there are none.
 */
void __attribute__((noreturn)) cleanerThread(void)
{
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadCB *thread, *self;
	SceProcessObject *process;
	SceUID threadId;
	int intr, ret;

	for (;;) {
		intr = SceCpuForDriver_D32ACE9E(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		manager = s_threadmgrRuntime.manager;
		thread = (struct ThreadCB *)manager->deletedNext;
		if ((uintptr_t)thread == (uintptr_t)&manager->deletedNext) {
			/* nothing to do: sleep until exitDeleteThread wakes us */
			sceKernelSpinlockLowUnlockCpuResumeIntr(
				&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
			intr = SceCpuForDriver_821FC0EE();
			self = s_threadmgrRuntime.manager->cleanerThread;
			SceCpuForDriver_BF82DEB2(THREAD_LOCK(self));
			SCE_DBG_ASSERT_CHECK(self->currentCpuId == (int)sceKernelCpuId(), 1985);
			self->currentCpuId = -1;
			self->waitId = -1;
			self->status = (self->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_WAITING;
			currentCpuState()->nextRunningThread = 0;
			switchThread(2);
			sceKernelCpuResumeIntr(intr);
			continue;
		}
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1911);
		threadId = thread->threadId;
		if (SceCpuForDriver_5AC9D394(THREAD_LOCK(thread)) < 0)
			goto retry;
		process = thread->process;
		if (SceCpuForDriver_5AC9D394(&process->lock) < 0) {
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto retry;
		}
		if (thread->dtraceState != NULL)
			dtraceThreadCBExit(thread);
		listClosed(thread);
		sceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		unlistFromProcess(thread, process);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		sceKernelSpinlockLowUnlock(&process->lock);
		if (thread->userThreadId != -1) {
			ret = SceSysmemForDriver_84A4AF5E(thread->processId, thread->userThreadId);
			if (ret < 0)
				SCE_DBG_PRINTF(0, 1966,
					"cleanerThread(): scePUIDClose=0x%08X, process(0x%08X) may be exited.\n",
					ret, thread->processId);
		}
		if (sceGUIDClose(threadId) < 0)
			SCE_PANIC(SCE_LINE(1970, 1972));
		sceKernelCpuResumeIntr(intr);
		continue;
retry:
		sceKernelSpinlockLowUnlockCpuResumeIntr(
			&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
	}
}

/* SceUIDThreadClass: a new thread object, dormant, its lists empty */
int uidThread_Constructor(struct ThreadmgrThreadObject *object)
{
	struct ThreadCB *thread = &object->context;

	memset(thread, 0, sizeof(*thread));
	thread->next = (uintptr_t)thread;
	thread->prev = (uintptr_t)thread;
	thread->status = SCE_KERNEL_THREAD_STATUS_DORMANT;
	thread->waitId = -1;
	thread->lock = 1;
	thread->callbackNext = (struct ThreadmgrCallbackCB *)&thread->callbackNext;
	thread->callbackPrev = (struct ThreadmgrCallbackCB *)&thread->callbackNext;
	thread->heldEventFlagNext = (uintptr_t)&thread->heldEventFlagNext;
	thread->heldEventFlagPrev = (uintptr_t)&thread->heldEventFlagNext;
	thread->ownedMutexNext = (uintptr_t)&thread->ownedMutexNext;
	thread->ownedMutexPrev = (uintptr_t)&thread->ownedMutexNext;
	thread->ownedFastMutexNext = (uintptr_t)&thread->ownedFastMutexNext;
	thread->ownedFastMutexPrev = (uintptr_t)&thread->ownedFastMutexNext;
	thread->ownedRWLockNext = (uintptr_t)&thread->ownedRWLockNext;
	thread->ownedRWLockPrev = (uintptr_t)&thread->ownedRWLockNext;
	return 0;
}

/* the thread object's destructor: its stacks, contexts and waiters */
int uidThread_Destructor(struct ThreadmgrThreadObject *object)
{
	struct ThreadCB *thread = &object->context;
	struct ThreadmgrExtendedStack *extended;
	SceUID memBlockId;
	int intr, ret, tries, wake;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 11736);
	if (!(thread->status & SCE_KERNEL_THREAD_STATUS_DELETED)) {
		SCE_DBG_PRINTF(0xb, 11743,
			" fatal error: thread status is not DELETED.(status = 0x%08X)\n",
			thread->status);
		SCE_PANIC(SCE_LINE(11739, 11744));
	}
	intr = SceCpuForDriver_D32ACE9E(THREAD_LOCK(thread));
	SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
	((struct ThreadmgrWaitNode *)thread->next)->prev = (struct ThreadmgrWaitNode *)thread->prev;
	((struct ThreadmgrWaitNode *)thread->prev)->next = (struct ThreadmgrWaitNode *)thread->next;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	sceKernelSpinlockLowUnlockCpuResumeIntr(THREAD_LOCK(thread), intr);
	for (extended = thread->extendedStacks; extended != NULL;
	     extended = thread->extendedStacks) {
		ret = SceSysmemForDriver_009E1C61(extended->memBlockId);
		SCE_DBG_PRINTF(0, 11762, "*** uidThread_Destructor(), free ExKernelStack(0x%08X)\n",
			       ret);
		thread->extendedStacks = thread->extendedStacks->next;
	}
	if (thread->kStackMemblkId != -1) {
		ret = SceSysmemForDriver_009E1C61(thread->kStackMemblkId);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 11771,
				"*** error: uidThread_Destructor() -> sceKernelFreeMemBlock(0x%08X)=0x%08X, pThreadCB=%p, pThreadCB->magic=0x%08X\n",
				thread->kStackMemblkId, ret, thread, thread->magic);
			SCE_DBG_ASSERT(0, 11772);
			return 0;
		}
	}
	if ((int)thread->currentAttr < 0) {
		/* the user stack may still be in use for a while: retry */
		memBlockId = thread->uStackMemblkId;
		if (memBlockId != -1) {
			for (tries = 0; tries < FREE_RETRY_LIMIT; tries++) {
				ret = SceSysmemForDriver_009E1C61(memBlockId);
				if (ret >= 0)
					break;
				if (SceDipswForDriver_A98FC2FD(DIPSW_FREE_RETRY_LOG) &&
				    tries % FREE_RETRY_LOG_EVERY == 0)
					SCE_PRINTF_WITH_INFO(0, SCE_LINE(11786, 11791),
						"*** error: uidThread_Destructor() -> sceKernelFreeMemBlock(0x%08X[%s])=0x%08X, count=%d\n",
						thread->uStackMemblkId,
						SceSysmemForDriver_E655852F(thread->uStackMemblkId), ret, tries);
				memBlockId = thread->uStackMemblkId;
			}
		}
		if (thread->userTlsMemBlockId != -1)
			SceSysmemForDriver_009E1C61(thread->userTlsMemBlockId);
		if (thread->exceptionEntry != NULL) {
			SCE_DBG_ASSERT_CHECK((thread->currentAttr >> 26) & 1, 11806);
			ret = SceSysrootForKernel_7968F091(thread->processId, thread->exceptionEntry);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 11810,
					"*** error: uidThread_Destructor() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X, pThreadCB=%p, pThreadCB->magic=0x%08X, pExceptionList\n",
					thread->processId, thread->exceptionEntry, ret, thread,
					thread->magic);
				SCE_DBG_ASSERT(0, 11811);
				return 0;
			}
		}
	}
	if (thread->pRegisterContext != NULL) {
		ret = SceSysrootForKernel_7968F091(thread->processId, thread->pRegisterContext);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 11823,
				"*** error: uidThread_Destructor() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X, pThreadCB=%p, pThreadCB->magic=0x%08X, pRegisterContext\n",
				thread->processId, thread->pRegisterContext, ret, thread, thread->magic);
			SCE_DBG_ASSERT(0, 11824);
			return 0;
		}
	}
	if (thread->pVfpRegisterContext != 0) {
		ret = SceSysrootForKernel_7968F091(thread->processId, (void *)thread->pVfpRegisterContext);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 11835,
				"*** error: uidThread_Destructor() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X, pThreadCB=%p, pThreadCB->magic=0x%08X, pVfpRegisterContext\n",
				thread->processId, (void *)thread->pVfpRegisterContext, ret, thread,
				thread->magic);
			SCE_DBG_ASSERT(0, 11836);
			return 0;
		}
	}
	/* whoever still waits for this thread hears it is gone */
	intr = SceCpuForDriver_D32ACE9E(&object->waitLock);
	wake = wakeupWaitObjectAll((uintptr_t)&object->waitNext, SCE_KERNEL_ERROR_WAIT_DELETE,
				   NULL);
	sceKernelSpinlockLowUnlock(&object->waitLock);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/*
 * PLACEHOLDER: name. The thread class's cancel method: whoever waits for
 * the thread is woken with SCE_KERNEL_ERROR_WAIT_CANCEL; returns how many.
 */
int uidThread_CancelWait(void *cls, void *uidObject, uint32_t a2)
{
	struct ThreadmgrThreadObject *object = uidObject;
	uint32_t count;
	int intr, wake;

	(void)cls;
	intr = SceCpuForDriver_821FC0EE();
	SceCpuForDriver_BF82DEB2(&object->waitLock);
	wake = wakeupWaitObjectAll((uintptr_t)&object->waitNext, SCE_KERNEL_ERROR_WAIT_CANCEL,
				   &count);
	sceKernelSpinlockLowUnlock(&object->waitLock);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	sceKernelCpuResumeIntr(intr);
	return count;
}

/*
 * SceKernelThreadMgr (threadmgr.c): changing a thread's priority. A queued
 * thread is requeued (by another CPU's interrupt if it is on that CPU's
 * queues); a waiting one is moved within the priority-ordered queues it
 * waits on. The debugger's hook, the DTrace probe and the priority hook
 * hear of the change.
 */

#define CPU_ID_GLOBAL			0x7fff
#define PRIORITY_SHARED_LOW		0x80
#define PRIORITY_SHARED_RANGE		0x7c
#define WAIT_ID_MULTIPLE		((SceUID)0x80000000)
#define WAIT_ATTR_PRIORITY		0x00002000
#define USER_TLS_PRIORITY		0x7e0
#define TRACE_EVENT_PRIORITY		0x21
#define DEBUG_EVENT_PRIORITY_CHANGE	0x10

typedef void (*ThreadmgrPriorityEventHook)(unsigned int event, const uint32_t *values);
typedef void (*ThreadmgrChangePriorityProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
	SceUID threadId, SceUID processId, int oldPriority, int newPriority);

static inline void unlinkThread(struct ThreadCB *thread)
{
	((struct ThreadCB *)thread->next)->prev = thread->prev;
	((struct ThreadCB *)thread->prev)->next = thread->next;
}

/* the DTrace probe and the priority hook hear of a thread's new priority */
void notifyThreadPriorityChange(struct ThreadCB *thread,
			       int oldPriority, int newPriority)
{
	ThreadmgrChangePriorityProbe probe;
	ThreadmgrPriorityChangeHook hook;
	SceUID threadId;

	/* the DTrace probe site __sched_:change__pri */
	__asm__ volatile(".global s_schedChangePriorityCodeSite\ns_schedChangePriorityCodeSite:");
	probe = (ThreadmgrChangePriorityProbe)s_schedChangePriorityHandler;
	if (probe != NULL) {
		threadId = thread->userThreadId;
		if (threadId < 0)
			threadId = thread->threadId;
		probe(&s_schedChangePriorityDescriptor, threadId,
		      thread->process->pid, oldPriority, newPriority);
	}
	hook = s_threadmgrRuntime.priorityChangeHook;
	if (hook == NULL)
		return;
	threadId = thread->userThreadId;
	if (threadId < 0)
		threadId = thread->threadId;
	hook(thread->process->pid, threadId, newPriority,
	     oldPriority);
}

/* SceThreadmgrSGI: another CPU asks this one to change threadId's priority */
void changeThreadPrioritySGIHandler(SceUID threadId, int priority)
{
	struct ThreadmgrThreadObject *object;

	if (currentThreadContext() != NULL)
		SCE_PANIC(SCE_LINE(6152, 6158));
	if (SceSysmemForDriver_00ED6C14(threadId,
	    s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) >= 0) {
		changeThreadPriority(&object->context, priority);
		sceGUIDReleaseObject(threadId);
	}
}

/* the running thread should give way to a more urgent one */
#define PREEMPT_SELF(cpu, line) do {						\
	SCE_DBG_ASSERT_LEVEL(0, (cpu) == currentCpuState(), (line));		\
	(cpu)->nextRunningThread = 0;							\
} while (0)

/* changeThreadPriority: thread's current priority; returns 1 if this CPU should switch */
int changeThreadPriority(struct ThreadCB *thread, int priority)
{
	struct ThreadCB *context = thread;
	unsigned int currentCpuId = sceKernelCpuId();
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *node;
	ThreadmgrEvent *event;
	struct ThreadmgrWaitableObject *waitable;
	struct ThreadmgrWaitNode *next;
	struct ThreadCB *waiter;
	ThreadmgrPriorityEventHook hook;
	uint32_t state, index;
	SceUID waitId;
	int oldPriority, status = 0, result, cpuId, highest, target;
	bool notify;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 5907);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(context));
	oldPriority = context->currentPriority;
retry:
	state = context->status;
	if (!(state & THREAD_STATE_QUEUED)) {
		if (!(state & SCE_KERNEL_THREAD_STATUS_WAITING)) {
			context->currentPriority = priority;
			goto finish;
		}
		waitId = context->waitId;
		if (waitId == WAIT_ID_MULTIPLE) {
			/* each event it waits on requeues its node by the new priority */
			wme = (ThreadmgrWaitMultipleEvents *)context->next;
			for (index = 0; index < wme->count; index++) {
				if (wme->nodes[index].next == &wme->nodes[index])
					continue;
				node = (ThreadmgrEventWaitNode *)((uintptr_t)wme->nodes[index].next -
					offsetof(ThreadmgrEventWaitNode, wmeLink));
				waitId = node->eventId;
				result = SceSysmemForDriver_72A98D17(waitId,
					s_threadmgrRuntime.manager->eventClass,
					(SceObjectBase **)&event);
				if (result < 0)
					SCE_PANIC(SCE_LINE(6005, 6011));
				if (SceCpuForDriver_5AC9D394(&event->lock) < 0)
					goto retry_wait;
				state = event->attr;
				context->currentPriority = priority;
				if (state & WAIT_ATTR_PRIORITY) {
					node->next->prev = node->prev;
					node->prev->next = node->next;
					for (next = event->next;
					     next != (struct ThreadmgrWaitNode *)&event->next;
					     next = next->next) {
						waiter = EventWaiterIsThread(next) ?
							(struct ThreadCB *)next :
							(struct ThreadCB *)(next->owner & ~3u);
						SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC,
								     6038);
						if (context->currentPriority < waiter->currentPriority)
							break;
					}
					node->next = next;
					node->prev = next->prev;
					next->prev = (struct ThreadmgrWaitNode *)node;
					node->prev->next = (struct ThreadmgrWaitNode *)node;
				}
				sceKernelSpinlockLowUnlock(&event->lock);
				sceGUIDReleaseObject(node->eventId);
			}
			goto finish;
		}
		result = SceSysmemForDriver_72A98D17(waitId,
			s_threadmgrRuntime.manager->waitableClass,
			(SceObjectBase **)&waitable);
		if (result < 0) {
			SCE_DBG_ASSERT_PRINTF(context->delayId != -1, 6081,
				"*** error: sceGUIDReferObjectWithSubclass(0x%08X)=0x%08X, delayId=0x%08X\n",
				waitId, result, context->delayId);
			context->currentPriority = priority;
			goto finish;
		}
		if (SceCpuForDriver_5AC9D394(&waitable->lock) < 0)
			goto retry_wait;
		state = waitable->attributes;
		context->currentPriority = priority;
		if (state & WAIT_ATTR_PRIORITY) {
			unlinkThread(context);
			insertPriorityQueue((uintptr_t)&waitable->next, thread);
		}
		if (oldPriority > 0 && (int32_t)context->currentAttr < 0 &&
		    sceKernelCopyToUserProc(context->processId,
					     (void *)(context->tls - USER_TLS_PRIORITY),
					     (void *)&context->currentPriority, sizeof(int)) < 0)
			SCE_PANIC(SCE_LINE(6093, 6099));
		sceKernelSpinlockLowUnlock(THREAD_LOCK(context));
		status = revalidateWaitState((uintptr_t)&waitable->next);
		sceKernelSpinlockLowUnlock(&waitable->lock);
		sceGUIDReleaseObject(waitId);
		notify = oldPriority > 0 && status >= 0;
		goto notify_change;
	}

	/* queued: on another CPU, that CPU does it */
	cpuId = context->currentCpuId;
	if (cpuId != (int)currentCpuId && cpuId != CPU_ID_GLOBAL) {
		issueChangeThreadPriorityIntrRequest(cpuId, context->threadId, priority);
		oldPriority = -1;
		goto unlock;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
		state = cpu->disabledDispatch;
		context->currentPriority = priority;
		cpu->priority = priority;
		if (!state && (int)searchHighestPriority() < priority) {
			PREEMPT_SELF(cpu, 5929);
			status = 1;
		}
	} else if (state & SCE_KERNEL_THREAD_STATUS_READY) {
		removeReady(thread, context->currentPriority);
		context->currentPriority = priority;
		addReady(thread);
		if ((uint32_t)context->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			/* the CPU running the least urgent thread reschedules */
			highest = 1;
			target = -1;
			for (index = 0; index < CPU_COUNT; index++) {
				if (highest < s_threadmgrRuntime.manager->cpuCB[index].priority) {
					highest = s_threadmgrRuntime.manager->cpuCB[index].priority;
					target = index;
				}
			}
			if (priority < highest) {
				if (target == (int)currentCpuId) {
					PREEMPT_SELF(cpu, 5949);
					status = 1;
				} else {
					addIntrRequest(target, INTR_MASK_SCHEDULING);
				}
			}
		} else if (priority < cpu->priority) {
			PREEMPT_SELF(cpu, 5958);
			status = 1;
		}
	} else {
		context->currentPriority = priority;
		if ((uint32_t)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
			unlinkThread(context);
			sceKernelSpinlockLowUnlock(&cpu->standbyLock);
			status = distributeThread(selectExecCpu(thread), thread);
		} else if (priority < cpu->priority) {
			PREEMPT_SELF(cpu, 5977);
			status = 1;
		}
	}
finish:
	notify = false;
	if (oldPriority > 0) {
		if ((int32_t)context->currentAttr < 0) {
			/* a user thread's TLS reports it */
			result = sceKernelCopyToUserProc(context->processId,
				(void *)(context->tls - USER_TLS_PRIORITY),
				(void *)&context->currentPriority, sizeof(int));
			if (result < 0) {
				status = result | (ThreadmgrInSyscall() ? SYSCALL_ERROR_BIT : 0);
				goto unlock;
			}
		}
		notify = status >= 0;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(context));
notify_change:
	if (notify) {
		hook = (ThreadmgrPriorityEventHook)s_threadmgrRuntime.manager->paHooks[1];
		if (hook != NULL) {
			uint32_t values[] = { context->processId, context->threadId,
					      oldPriority, context->currentPriority };

			hook(DEBUG_EVENT_PRIORITY_CHANGE, values);
		}
		notifyThreadPriorityChange(thread, oldPriority, priority);
	}
	return status;
retry_wait:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(context));
	sceGUIDReleaseObject(waitId);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(context));
	goto retry;
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(context));
	return status;
}

/*
 * SceKernelThreadMgr (threadmgr.c): sceKernelDelayThread and its family.
 * The thread sleeps on a delay object until its wakeup time, optionally
 * running its callbacks; a suspend or exit request ends the delay early.
 */

#define WAIT_TYPE_DELAY			0x00000001
#define DELAY_INTERRUPTIBLE		0x00008000	/* a request ends the delay outside a syscall too */
#define DELAY_MIN_USEC			200
#define DELAY200_MIN_USEC		201
#define SDK_VERSION_DELAY_ZERO_ERROR	0x02100000	/* from 2.10 a delay of 0 is an error */
#define SDK_VERSION_DELAY_JITTER	0x03100000	/* from 3.10 a user delay gets a little jitter */
#define SCE_KERNEL_ERROR_ILLEGAL_CONTEXT_ARG	0x80020005	/* PLACEHOLDER: name */

/* the __sched_:sleep probe of a delay */
static uint32_t s_delaySleepProbeId;
volatile uintptr_t s_delaySleepHandler;
extern const char s_delaySleepCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_delaySleepDescriptor = {
	.probeId = &s_delaySleepProbeId,
	.provider = "__sched_",
	.name = "sleep",
	.codeSite = s_delaySleepCodeSite,
	.handler = &s_delaySleepHandler,
};

/* the delay object's timeout: the thread wakes if it still waits */
void delayTimeupHandler(struct ThreadCB *thread)
{
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2259);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if (thread->status & SCE_KERNEL_THREAD_STATUS_WAITING)
		wakeupAndDistributeThread(thread, 0, 0);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
}

/*
 * The running thread sleeps usec microseconds (at least DELAY_MIN_USEC);
 * flags DELAY_INTERRUPTIBLE, remaining receives the time left when a
 * request ended the sleep, callbacks runs them while it sleeps.
 */
int delayThread(SceUInt32 usec, uint32_t flags, SceUInt32 *remaining, int callbacks)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread, *head;
	ThreadmgrSchedSleepHandler sleep;
	SceUInt32 version, delay, jittered;
	uint64_t wakeup;
	SceSize length;
	char buffer[0x200];
	uint32_t saved;
	int intr, ret, result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2037, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (thread->processId == THREADMGR_KERNEL_PID) {
		version = SDK_VERSION_KERNEL_THREAD;
		if (usec == 0)
			goto zero;
	} else if (SceSysrootForDriver_CAE820B2(thread->processId, &version) < 0) {
		version = 0;
	} else if (usec == 0 && version >= SDK_VERSION_DELAY_ZERO_ERROR) {
		goto zero;
	}
	delay = 0;
	if (usec != 0) {
		delay = usec;
		if (usec - 1 < DELAY_MIN_USEC - 1)
			delay = DELAY_MIN_USEC;
		if (version >= SDK_VERSION_DELAY_JITTER && ThreadmgrInSyscall()) {
			jittered = delay + *(uint8_t *)&thread->runClock;
			if (jittered < usec)
				jittered = usec;
			delay = jittered;
		}
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		memset(buffer, 0, sizeof(buffer));
		if (sceKernelSysrootBacktrace(0, buffer, sizeof(buffer), &length, 3) >= 0)
			sceKernelSysrootPrintBacktrace(THREADMGR_KERNEL_PID, buffer, length, 0);
		SCE_DBG_PRINTF(0, 2088, " k: error: delayThread(usec 0x%08X) irq suspended\n", delay);
		SCE_DBG_ASSERT_FAIL(2090);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	cpu = currentCpuState();
	if (cpu->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((int)thread->currentAttr < 0 && sceKernelCheckThreadUserStackSelf() == 0)
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	getWakeupTime(&wakeup, delay);
	result = 0;
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ret = callCallbackAll(thread);
			if (ret < 0)
				break;
			cpu = currentCpuState();
		}
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
		if ((thread->flags >> 28) != 0 &&
		    ((!(thread->flags & THREAD_FLAG_REQUEST_VM_SUSPEND) &&
		      (flags & DELAY_INTERRUPTIBLE)) || ThreadmgrInSyscall())) {
			/* a pending request ends the delay */
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SIGNAL_DEFERRED);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_RESUMED_WAIT);
			switch (thread->flags & THREAD_FLAG_REQUEST_MASK) {
			case THREAD_FLAG_REQUEST_VM_SUSPEND:
			case THREAD_FLAG_REQUEST_SUSPEND:
				ret = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
				thread->waitResult = ret;
				break;
			case THREAD_FLAG_REQUEST_EXIT_DELETE:
			case THREAD_FLAG_REQUEST_EXIT:
				ret = SCE_KERNEL_ERROR_THREAD_STOPPED;
				thread->waitResult = ret;
				break;
			default:
				SCE_PANIC(SCE_LINE(2148, 2150));
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (remaining != NULL)
				*remaining = (uint32_t)wakeup +
					(uint32_t)thread->process->time -
					SceThreadmgrForDriver_47F6DE49();
			break;
		}
		SCE_DBG_ASSERT_CHECK(thread->delayId == -1, 2162);
		SCE_DBG_ASSERT_CHECK(thread->timeoutOutput == 0, 2163);
		ret = uidDelay_AllocObject(&wakeup, (uintptr_t)delayTimeupHandler);
		if (ret < 0) {
			if (ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
				/* already past */
				ret = 0;
				if (remaining != NULL)
					*remaining = 0;
			} else {
				SCE_DBG_ASSERT_FAIL(2172);
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			break;
		}
		thread->delayId = ret;
		thread->waitArg0 = usec;
		thread->remainingTimeLow = (uint32_t)wakeup;
		thread->remainingTimeHigh = (uint32_t)(wakeup >> 32);
		thread->timeoutOutput = (uintptr_t)remaining;
		thread->waitArg1 = 0;
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			/* callbacks came in meanwhile: run them first */
			cancelDelayTimer(ret, NULL);
			thread->waitResult = THREADMGR_ERROR_WAIT_CANCEL;
			thread->delayId = -1;
			thread->timeoutOutput = 0;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		} else {
			thread->waitId = -1;
			thread->waitResult = 0;
			thread->waitType = WAIT_TYPE_DELAY;
			SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 2207);
			thread->currentCpuId = -1;
			if (ThreadmgrInSyscall() || (flags & DELAY_INTERRUPTIBLE))
				SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
							   THREAD_FLAG_WAIT_MARK);
			if (callbacks)
				SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
							   THREAD_FLAG_WAKE_PENDING);
			/* the DTrace probe site __sched_:sleep */
			__asm__ volatile(".global s_delaySleepCodeSite\ns_delaySleepCodeSite:");
			thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_WAITING;
			SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->timedWaitLock);
			head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->timedWaitNext;
			self->next = head;
			self->prev = head->prev;
			head->prev = self;
			self->prev->next = self;
			sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->timedWaitLock);
			sleep = (ThreadmgrSchedSleepHandler)s_delaySleepHandler;
			if (sleep != NULL)
				sleep(&s_delaySleepDescriptor);
			SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 2227);
			cpu->nextRunningThread = 0;
			switchThread(2);
			SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_WAKE_PENDING);
			result = thread->waitResult;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		}
		ret = result;
	} while (callbacks && result == (int)THREADMGR_ERROR_WAIT_CANCEL);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
zero:
	if (ThreadmgrInSyscall()) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT_ARG | SYSCALL_ERROR_BIT;
	} else {
		SCE_DBG_ASSERT_PRINTF(0, 2055,
			"*** error: delayThread(0), processId=0x%08X[%s], threadId=0x%08X[%s]\n",
			thread->processId, SceSysmemForDriver_E655852F(thread->processId),
			thread->threadId, SceSysmemForDriver_E655852F(thread->threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT_ARG;
	}
	goto restore;
}

/* sceKernelDelayThread */
int sceKernelDelayThread(SceUInt32 usec)
{
	return delayThread(usec, 0, NULL, 0);
}

/* sceKernelDelayThread200: at least DELAY200_MIN_USEC */
int sceKernelDelayThread200(SceUInt32 usec)
{
	SCE_DBG_PRINTF_LEVEL(2, 3, 2287, "(0x%08x)\n", usec);
	if (usec < DELAY200_MIN_USEC)
		usec = DELAY200_MIN_USEC;
	return delayThread(usec, 0, NULL, 0);
}

/* sceKernelDelayThreadCB */
int sceKernelDelayThreadCB(SceUInt32 usec)
{
	return delayThread(usec, 0, NULL, 1);
}

/* sceKernelDelayThreadCB200 */
int sceKernelDelayThreadCB200(SceUInt32 usec)
{
	SCE_DBG_PRINTF_LEVEL(2, 3, 2310, "(0x%08x)\n", usec);
	if (usec < DELAY200_MIN_USEC)
		usec = DELAY200_MIN_USEC;
	return delayThread(usec, 0, NULL, 1);
}

/* SceThreadmgrForDriver_E9A99C50: an interruptible delay that reports the time left */
int SceThreadmgrForDriver_E9A99C50(SceUInt32 usec, SceUInt32 *remaining)
{
	return delayThread(usec, DELAY_INTERRUPTIBLE, remaining, 0);
}

/* SceThreadmgrForDriver_2B60B793: the same, running callbacks */
int SceThreadmgrForDriver_2B60B793(SceUInt32 usec, SceUInt32 *remaining)
{
	return delayThread(usec, DELAY_INTERRUPTIBLE, remaining, 1);
}

/*
 * SceKernelThreadMgr: what a process type allows of CPU affinity, the
 * per-CPU requests that move a thread's affinity or priority, and taking
 * the running thread off its CPU from an interrupt.
 */

#define AFFINITY_MASK			0xf
#define AFFINITY_SYSTEM_CPU		0x8	/* CPU 3 */
#define THREAD_ATTR_UNLIMITED		0x00004000
#define TRACE_EVENT_AFFINITY		0x20
#define FAST_MUTEX_MAGIC		0xa35db473
#define PREEMPT_WAIT			4
#define PREEMPT_EXCEPTION		8
#define PREEMPT_INTR			2
#define SWITCH_HOOK_OFF_CPU		2
#define SWITCH_HOOK_ARGC		5
#define SCHED_TRACE_OFF_CPU		2
#define PRIORITY_NONE			0xfe

/* the titles whose system-type processes may use any CPU */
#if FW_DEBUG
static inline int anyCpuTitle(SceProcessObject *process)
{
	return strncmp(process->TitleId, "NPXS10084", 31) == 0 ||
	       strncmp(process->TitleId, "NPXS10080", 31) == 0;
}
#else
#define anyCpuTitle(process) \
	(strncmp((process)->TitleId, "NPXS10084", 31) == 0 || \
	 strncmp((process)->TitleId, "NPXS10080", 31) == 0)
#endif

/*
 * checkCpuAffinityMaskPhy: the CPUs of mask may run a thread of process (attr: its attributes).
 * Sony's three callers call it whole: not split after the mask == 0 test.
 */
int __attribute__((noinline))
checkCpuAffinityMaskPhy(uint32_t mask, uint32_t attributes, SceProcessObject *pProcess)
{
	uint32_t type;

	if (mask == 0)
		return 0;
	SCE_DBG_ASSERT_CHECK(!(mask & ~AFFINITY_MASK), 3050);
	type = pProcess->budget->type;
#if FW_DEBUG
	if (type == PROCESS_TYPE_MINI_APP) {
		if (!(mask & ~AFFINITY_SYSTEM_CPU) || anyCpuTitle(pProcess))
			return 0;
		SCE_DBG_PRINTF(0, 3069, " k: checkCpuAffinityMaskPhy(), pProcess->TitleId=[%s]\n",
			       pProcess->TitleId);
	} else if (type > PROCESS_TYPE_MINI_APP) {
		switch (type) {
		case PROCESS_TYPE_SHELL:
		case PROCESS_TYPE_KERNEL:
			break;
		default:
			goto invalidType;
		}
		if (!(mask >> 4))
			return 0;
	} else {
		if (type != PROCESS_TYPE_GAME)
			goto invalidType;
		if (!(mask >> 3) || (attributes & THREAD_ATTR_UNLIMITED))
			return 0;
	}
#else
	switch (type) {
	case PROCESS_TYPE_MINI_APP:
		if (!(mask & ~AFFINITY_SYSTEM_CPU))
			return 0;
		return anyCpuTitle(pProcess) ? 0 : SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
	case PROCESS_TYPE_GAME:
		return (mask & ~0x7u) && !(attributes & THREAD_ATTR_UNLIMITED) ?
			SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK : 0;
	case PROCESS_TYPE_SHELL:
	case PROCESS_TYPE_KERNEL:
		return (mask & ~AFFINITY_MASK) ? SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK : 0;
	default:
		goto invalidType;
	}
#endif
	SCE_DBG_PRINTF(0, 3090, "*** error: checkCpuAffinityMaskPhy(0x%08X, 0x%08X), type=0x%08X\n",
		       mask, attributes, pProcess->budget->type);
	return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
invalidType:
	SCE_DBG_PRINTF(0, 3082, "error: processType=0x%08X\n", type);
	SCE_PANIC(SCE_LINE(3077, 3083));
}

/* sceKernelGetDefaultCpuAffinityMask: the CPUs a process type's threads use by default */
uint32_t sceKernelGetDefaultCpuAffinityMask(ProcessBudget *budget)
{
	uint32_t type = budget->type;

	switch (type) {
	case PROCESS_TYPE_GAME:
		return 0x7;
	case PROCESS_TYPE_MINI_APP:
	case PROCESS_TYPE_SHELL:
		return 0x8;
	case PROCESS_TYPE_KERNEL:
		return 0xf;
	default:
		SCE_DBG_PRINTF(0, 8634, "*** warning: unknown process type(0x%08X)\n", type);
		SCE_PANIC(SCE_LINE(8630, 8635));
	}
}

/* cpuId sets threadId's priority and affinity */
void issueChangeThreadPriorityAndCpuAffinityIntrRequest(unsigned int cpuId, SceUID threadId,
				       int priority, uint32_t mask)
{
	struct ThreadmgrIntrRequest *request =
		allocIntrRequest(INTR_REQUEST_CHANGE_THREAD_PRIORITY_AFFINITY);
	ThreadmgrTraceHook hook;

	hook = s_threadmgrRuntime.schedTraceHook;
	request->payload[2] = mask;
	request->payload[0] = threadId;
	request->payload[1] = priority;
	if (hook != NULL)
		hook(TRACE_EVENT_PRIORITY, 2, threadId, priority, mask);
	issueIntrRequest(cpuId, request);
}

#define INTR_SCHED_PROBE(symbol, label) \
	static uint32_t symbol##ProbeId; \
	volatile uintptr_t symbol##Handler; \
	extern const char symbol##CodeSite[]; \
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = { \
		.probeId = &symbol##ProbeId, .provider = "__sched_", .name = label, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler, \
	}

/*
 * SceKernelThreadMgr (threadmgr.c): the thread manager's part in a
 * process's life. sceKernelThreadMgrProcessCreate gives a new process its
 * lists, its msg pipe lock, its worker thread (which suspends it for the
 * debugger and keeps its object caches filled) and its exception object;
 * the two delete hooks take everything down again.
 */

#define WORKER_EVENT_SUSPEND		0x00000001	/* debugEventFlag: suspend the process */
#define WORKER_EVENT_EXIT		0x80000000	/* debugEventFlag: the worker ends */
#define WORKER_WAIT_MODE		3		/* OR, clear */
#define WORKER_IDLE_USEC		16000
#define WORKER_PRIORITY			0xfe
#define WORKER_STACK_SIZE		0x1000
#define WORKER_CPU_AFFINITY		0xf
#define PERMISSION_WORKER		0x80		/* PLACEHOLDER: the permission it is created with */
#define PROCESS_DEBUG_SUSPEND_EXCEPTION	0x11		/* PLACEHOLDER: SceSysrootForKernel_1247A825's reason */
#define SCE_KERNEL_ERROR_DORMANT	0x80028027
#define SCE_KERNEL_ERROR_NOT_SUSPENDED	0x8002802e
#define SCE_KERNEL_ERROR_UNKNOWN_UID	0x80028001	/* PLACEHOLDER: name */

typedef void (*ThreadmgrProcessDebugSuspendHook)(SceUID pid, SceUID threadId, int kind,
						 uintptr_t arg);
int stopThread(SceUID threadId, int priority, uint32_t request);
int deleteThread(struct ThreadCB *thread);
int startThread(SceUID threadId, SceSize argSize, const void *arg, int checkSource);
int createException(SceProcessObject *process);
int deleteException(SceProcessObject *process);
int uidLwMutex_DeleteCache(SceProcessObject *process, int target);
int uidLwMutex_ManageCache(SceProcessObject *process, int high, int low, int target);
int eventFlagProcessExit(SceProcessObject *process);
int semaphoreProcessExit(SceProcessObject *process);
int condProcessExit(SceProcessObject *process);
int mutexProcessExit(SceProcessObject *process);
int rwLockProcessExit(SceProcessObject *process);
int lwCondProcessExit(SceProcessObject *process);
int lwMutexProcessExit(SceProcessObject *process);
int msgPipeProcessExit(SceProcessObject *process);
int simpleEventProcessExit(SceProcessObject *process);
int threadEventProcessExit(SceProcessObject *process);
int callbackProcessExit(SceProcessObject *process);

/* SceThreadmgrForKernel_9928DFBA: ends another thread */
int SceThreadmgrForKernel_9928DFBA(SceUID threadId, int priority)
{
	return stopThread(threadId, priority, THREAD_FLAG_REQUEST_EXIT);
}

/* SceThreadmgrForKernel_800ACB9D: ends and deletes another thread */
int SceThreadmgrForKernel_800ACB9D(SceUID threadId, int priority)
{
	return stopThread(threadId, priority, THREAD_FLAG_REQUEST_EXIT_DELETE);
}

/* SceThreadmgrForKernel_15D6357C: suspends another thread */
int SceThreadmgrForKernel_15D6357C(SceUID threadId, int priority)
{
	return stopThread(threadId, priority, THREAD_FLAG_REQUEST_SUSPEND);
}

/* SceThreadmgrForKernel_490DAF14: resumes a suspended thread */
int SceThreadmgrForKernel_490DAF14(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	uint32_t saved;
	int intr, ret, wake;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (threadId == 0 ||
	    (currentThreadContext() != NULL && currentThreadContext()->threadId == threadId)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if ((uint16_t)thread->status == SCE_KERNEL_THREAD_STATUS_DORMANT) {
		ret = SCE_KERNEL_ERROR_DORMANT;
	} else if ((uint16_t)thread->status == SCE_KERNEL_THREAD_STATUS_SUSPENDED &&
		   (thread->flags & THREAD_FLAG_SUSPEND_COUNTED)) {
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_RESUME_CLEAR);
		thread->currentPriority = thread->basePriority < thread->boostPriority ?
			thread->basePriority : thread->boostPriority;
		if (thread->vmSuspendCount < 1) {
			/* runs again */
			SceCpuForDriver_BF82DEB2(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			((struct ThreadmgrWaitNode *)thread->next)->prev =
				(struct ThreadmgrWaitNode *)thread->prev;
			((struct ThreadmgrWaitNode *)thread->prev)->next =
				(struct ThreadmgrWaitNode *)thread->next;
			sceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			wake = distributeThread(selectExecCpu(thread), thread);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			sceGUIDReleaseObject(threadId);
			if ((wake & 1) && currentThreadContext() != NULL)
				switchThread((intr >> 7) & 1);
			goto resume;
		}
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_NOT_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sceKernelCountWaitingThreadsProc: how many threads of process pid wait on waitId */
int sceKernelCountWaitingThreadsProc(SceUID pid, SceUID waitId)
{
	struct ThreadmgrWaitQueue *queue;
	struct ThreadmgrWaitNode *node;
	struct ThreadCB *thread;
	SceObjectBase *processObject;
	int intr, ret;

	ret = SceSysmemForDriver_0FC24464(pid, &processObject);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_821FC0EE();
	ret = SceSysmemForDriver_72A98D17(waitId,
		s_threadmgrRuntime.manager->waitableClass, (SceObjectBase **)&queue);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_UID;
		goto resume;
	}
	queue = (struct ThreadmgrWaitQueue *)((uintptr_t)queue + 0x10);
	SceCpuForDriver_BF82DEB2(&queue->lock);
	ret = 0;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
		thread = (node->owner & 1) ? (struct ThreadCB *)node :
			(struct ThreadCB *)node->owner;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 10004);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
		if (thread->processId == pid)
			ret++;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlock(&queue->lock);
	sceGUIDReleaseObject(waitId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/*
 * A process's worker thread: suspends the process when a thread asks for
 * it (debugEventFlag), and while idle keeps its object caches filled.
 */
int processWorkerThread(SceSize argSize, void *arg)
{
	SceProcessObject *process = *(SceProcessObject **)arg;
	volatile uint32_t *state = &process->status;
	ThreadmgrProcessDebugSuspendHook hook;
	uint32_t pattern;
	SceUInt32 timeout;
	int ret, intr;

	(void)argSize;
	for (;;) {
		pattern = 0;
		timeout = WORKER_IDLE_USEC;
		ret = sceKernelWaitEventFlag(process->abortEventFlag,
					      WORKER_EVENT_EXIT | WORKER_EVENT_SUSPEND,
					      WORKER_WAIT_MODE, &pattern, &timeout);
		if (ret != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT && ret < 0)
			SCE_PRINTF_WITH_INFO(0, SCE_LINE(10238, 10243),
				"*** fatal error: processWorkerThread() -> sceKernelWaitEventFlag(0x%08X)=0x%08X\n",
				process->abortEventFlag, ret);
		if (ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			/* idle: refill the caches */
			if ((*state & PROCESS_STATE_RUNNING) &&
			    !(*state & PROCESS_FLAG_EXIT)) {
				if (process->pid == THREADMGR_KERNEL_PID) {
					ret = uidDelay_ManageCache(0x60, 0x20, 0x40);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 10277,
							"*** error: processWorkerThread() -> uidDelay_ManageCache()=0x%08X\n",
							ret);
					ret = uidAlarm_ManageCache(0x20, 0x10, 0x30);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 10282,
							"*** error: processWorkerThread() -> uidAlarm_ManageCache()=0x%08X\n",
							ret);
				} else {
					ret = uidLwMutex_ManageCache(process, 0x80, 0x40, 0x60);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 10271,
							"*** error: processWorkerThread()[id=0x%08X] -> uidLwMutex_ManageCache()=0x%08X\n",
							process->pid, ret);
				}
			}
			continue;
		}
		if (pattern & WORKER_EVENT_EXIT) {
			if (process->pid != THREADMGR_KERNEL_PID)
				uidLwMutex_DeleteCache(process, 0);
			return 0;
		}
		if (pattern & WORKER_EVENT_SUSPEND) {
			ret = SceSysrootForKernel_1247A825(process->pid, PROCESS_DEBUG_SUSPEND_EXCEPTION);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 10256,
					"(): sceKernelProcessDebugSuspend(0x%08X)=0x%08X\n", process->pid,
					ret);
			} else if (s_threadmgrRuntime.manager->processDebugSuspendHook != 0) {
				intr = SceCpuForDriver_821FC0EE();
				hook = (ThreadmgrProcessDebugSuspendHook)
					s_threadmgrRuntime.manager->processDebugSuspendHook;
				hook(process->pid, process->abortThread, process->abortCode,
				     s_threadmgrRuntime.manager->processDebugSuspendArg);
				sceKernelCpuResumeIntr(intr);
			}
		}
	}
}

/* sceKernelThreadMgrProcessCreate: a new process's thread manager state */
int sceKernelThreadMgrProcessCreate(SceProcessObject *process)
{
	struct ThreadCB *self;
	const char *processName;
	char name[32];
	int ret, permission;

	process->timers.next = &process->timers;
	process->timers.prev = &process->timers;
	process->threads.next = &process->threads;
	process->threads.prev = &process->threads;
	process->timerLock = 0;
	ret = sceKernelInitializeFastMutex(&process->msgPipeLock, "SceKernelMsgPipeListLock", 0,
					    NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(10353);
		return ret;
	}
	process->lwMutexCacheLock = 0;
	process->msgPipes.next = &process->msgPipes;
	process->msgPipes.prev = &process->msgPipes;
	process->lwMutexCache = NULL;
	process->lwMutexCacheCount = 0;
	ret = SceSysmemForDriver_A78755EB(process->pid, &processName);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 10368,
			"*** error: sceKernelThreadMgrProcessCreate() -> sceGUIDGetName(0x%08X)=0x%08X\n",
			process->pid, ret);
	snprintf(name, sizeof(name), "tworker: %s", processName);
	process->abortEventFlag = sceKernelCreateEventFlag(name, 0, 0, NULL);
	if (process->abortEventFlag < 0) {
		SCE_DBG_ASSERT_FAIL(10380);
		return ret;
	}
	/* the worker is created with the kernel's permission */
	self = currentThreadContext();
	if (self == NULL) {
		process->workerThread = sceKernelCreateThread(name,
			(uintptr_t)processWorkerThread, WORKER_PRIORITY, WORKER_STACK_SIZE, 0,
			WORKER_CPU_AFFINITY, NULL);
		self = currentThreadContext();
	} else {
		permission = self->permission;
		self->permission = PERMISSION_WORKER;
		process->workerThread = sceKernelCreateThread(name,
			(uintptr_t)processWorkerThread, WORKER_PRIORITY, WORKER_STACK_SIZE, 0,
			WORKER_CPU_AFFINITY, NULL);
		self = currentThreadContext();
		if (permission >= 0 && self != NULL)
			self->permission = permission;
	}
	if (process->workerThread < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 10396,
			"*** error: sceKernelCreateThread(\"%s\")=0x%08X, caller processId=0x%08X\n",
			name, process->workerThread, currentThreadContext()->processId);
		return ret;
	}
	ret = startThread(process->workerThread, sizeof(process), &process, 0);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(10402);
		return ret;
	}
	ret = createException(process);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(10409);
	return ret;
}

/* a process's threads are deleted with it (its thread list locked) */
static inline void threadProcessExit(SceProcessObject *process)
{
	ListNode *node;
	struct ThreadCB *thread;
	SceObjectBase *object;

	for (node = process->threads.next; node != &process->threads; node = node->next) {
		thread = THREAD_FROM_PROCESS_LINK(node);
		if (SceSysmemForDriver_00ED6C14(thread->threadId,
			s_threadmgrRuntime.manager->threadClass, &object) < 0)
			SCE_PANIC_FUNC(SCE_LINE(10310, 10315), "threadProcessExit");
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
		deleteThread(thread);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (sceGUIDReleaseObject(thread->threadId) < 0)
			SCE_PANIC_FUNC(SCE_LINE(10320, 10325), "threadProcessExit");
	}
}

/* sceKernelThreadMgrProcessDeleteBeforePUIDClose: the process's objects go */
int sceKernelThreadMgrProcessDeleteBeforePUIDClose(SceProcessObject *process)
{
	int intr, ret;

	ret = eventFlagProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10425);
	ret = semaphoreProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10427);
	ret = condProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10429);
	ret = mutexProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10431);
	ret = rwLockProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10433);
	ret = lwCondProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10436);
	ret = lwMutexProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10438);
	ret = msgPipeProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10441);
	ret = timerProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10443);
	ret = simpleEventProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10445);
	ret = threadEventProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10448);
	ret = callbackProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10450);
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	threadProcessExit(process);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	SCE_DBG_ASSERT_LEVEL(0, 1, 10452);
	return 0;
}

/* sceKernelThreadMgrProcessDeleteAfterPUIDClose: its exception object and worker go */
int sceKernelThreadMgrProcessDeleteAfterPUIDClose(SceProcessObject *process)
{
	int ret;

	ret = deleteException(process);
	SCE_DBG_ASSERT_CHECK(ret >= 0, 10466);
	if (process->abortEventFlag == -1)
		return ret;
	ret = sceKernelSetEventFlag(process->abortEventFlag, WORKER_EVENT_EXIT);
	SCE_DBG_ASSERT_PRINTF(ret >= 0, 10480,
		"Fatal Error: sceKernelSetEventFlag(0x%08x) failed 0x%08x\n",
		process->abortEventFlag, ret);
	if (process->workerThread != -1) {
		waitThreadEnd(process->workerThread, NULL, NULL, 0);
		ret = sceKernelDeleteThread(process->workerThread);
		if (ret != 0)
			SCE_DBG_PRINTF(0, 10486,
				" k: error: sceKernelThreadMgrProcessDeleteAfterPUIDClose() -> sceKernelDeleteThread()=0x%08X\n",
				ret);
	}
	ret = sceKernelDeleteEventFlag(process->abortEventFlag);
	if (ret != 0) {
		SCE_DBG_PRINTF(0, 10491,
			" k: error: sceKernelThreadMgrProcessDeleteAfterPUIDClose() -> sceKernelDeleteEventFlag()=0x%08X\n",
			ret);
		return ret;
	}
	return 0;
}

/*
 * SceKernelThreadMgr (threadmgr.c): a thread suspending itself for the
 * debugger. debugSuspendThreadSelf marks a user thread that hit a problem
 * (a broken user stack, a stray sceKernelExitCallback, ...), tells the
 * debugger's handlers, and asks for its process's suspension through the
 * process's debug event flag; _debugSuspendThreadSelf takes the running
 * thread off its CPU on the way out of the kernel.
 */

/* why a thread suspends itself, and the debugger's handler slot for each */

/* what the debugger's handler decides */
#define DEBUG_SUSPEND_ACTION_RETURN	0	/* PLACEHOLDER: name */
#define DEBUG_SUSPEND_ACTION_PROCESS	1	/* the process is suspended */
#define DEBUG_SUSPEND_ACTION_IGNORE	2

#define USER_TLS_STACK_RANGE		0x7f8

void issueCallPAScheduleHookIntrRequest(unsigned int cpuId, SceUID processId, SceUID threadId,
					uint32_t event, int priority);

/* the user stack left below the syscall's sp, 0 if it is out of the stack */
int sceKernelCheckThreadUserStackSelf(void)
{
	struct ThreadCB *thread =
		currentThreadContext();
	uint32_t range[2];			/* the stack's top and bottom, from the user TLS */
	uint32_t sp;

	if (thread == NULL) {
		SCE_DBG_ASSERT_FAIL(7654);
		return 0;
	}
	if (!(thread->currentAttr & THREAD_ATTR_USER)) {
		SCE_DBG_ASSERT_FAIL(7660);
		return 0;
	}
	SCE_DBG_ASSERT_CHECK(thread->userFrame != NULL, 7665);
	sp = thread->userFrame->sp;
	if (SceSysmemForDriver_BC996A7A(range, (const void *)(thread->tls - USER_TLS_STACK_RANGE),
				   sizeof(range)) >= 0 && sp <= range[0] && range[1] < sp)
		return sp - range[1];
	return 0;
}

/*
 * The current user thread stops for the debugger (reason, with arg and the
 * reason's info): the handlers of the debugger's slot for the reason are
 * told; unless one decides otherwise, the process is suspended through its
 * debug event flag (once).
 */
int debugSuspendThreadSelf(uint32_t reason, SceSize size, void *message)
{
	struct ThreadCB *thread =
		currentThreadContext();
	SceProcessObject *process;
	ThreadmgrDebugHandler notify;
	ThreadmgrDebugHandler decide;
	uint32_t kind;
	int intr, ret, action;

	SCE_DBG_ASSERT_CHECK(currentThreadContext() != NULL, 11306);
	SCE_DBG_ASSERT_CHECK(thread->currentAttr >> 31, 11308);
	process = thread->process;
	intr = SceCpuForDriver_821FC0EE();
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if (thread->flags >> 30 != 0) {
		/* it is exiting */
		ret = 0;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		goto resume;
	}
	thread->status |= THREAD_STATE_DEBUG_SUSPENDED;
	thread->debugSuspendReason |= reason;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	switch (reason) {
	case DEBUG_SUSPEND_EXIT_CALLBACK:
		notify = s_threadmgrRuntime.manager->debugHandlers[0][DEBUG_HANDLER_EXIT_CALLBACK];
		if (notify != NULL)
			notify(size, message);
		decide = s_threadmgrRuntime.manager->debugHandlers[1][DEBUG_HANDLER_EXIT_CALLBACK];
		if (decide != NULL) {
			action = decide(size, message);
			kind = 5;
			goto decided;
		}
		kind = 5;
		break;
	case DEBUG_SUSPEND_USER_STACK:
		notify = s_threadmgrRuntime.manager->debugHandlers[0][DEBUG_HANDLER_STACK_OVERFLOW];
		if (notify != NULL)
			notify(size, message);
		decide = s_threadmgrRuntime.manager->debugHandlers[1][DEBUG_HANDLER_STACK_OVERFLOW];
		if (decide != NULL) {
			action = decide(size, message);
			kind = 4;
			goto decided;
		}
		kind = 4;
		break;
	case DEBUG_SUSPEND_UNK80000:
		notify = s_threadmgrRuntime.manager->debugHandlers[0][DEBUG_HANDLER_UNK80000];
		if (notify != NULL)
			notify(size, message);
		decide = s_threadmgrRuntime.manager->debugHandlers[1][DEBUG_HANDLER_UNK80000];
		if (decide != NULL) {
			action = decide(size, message);
			kind = 7;
			goto decided;
		}
		kind = 7;
		break;
	case DEBUG_SUSPEND_UNK100000:
		kind = *(uint32_t *)message;
		notify = s_threadmgrRuntime.manager->debugHandlers[0][DEBUG_HANDLER_UNK100000];
		if (notify != NULL)
			notify(size, message);
		decide = s_threadmgrRuntime.manager->debugHandlers[1][DEBUG_HANDLER_UNK100000];
		if (decide != NULL) {
			action = decide(size, message);
			goto decided;
		}
		break;
	default:
		SCE_PANIC(SCE_LINE(11406, 11411));
	}
	goto suspend;
decided:
	if (action != DEBUG_SUSPEND_ACTION_PROCESS) {
		if (action == DEBUG_SUSPEND_ACTION_IGNORE)
			goto none;
		if (action != DEBUG_SUSPEND_ACTION_RETURN)
			SCE_PANIC(SCE_LINE(11431, 11436));
		SCE_DBG_ASSERT_CHECK(reason == DEBUG_SUSPEND_CRITICAL_ERROR, 11417);
		ret = 0;
		goto resume;
	}
suspend:
	if (SceCpuForDriver_CDA96E81((SceInt32 *)&process->abortRequested, 0, 1) != 0)
		goto none;
	process->abortCode = kind;
	process->abortThread = thread->threadId;
	ret = sceKernelSetEventFlag(process->abortEventFlag, 1);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 11427,
			"*** error: debugSuspendThreadSelf() -> sceKernelSetEventFlag(0x%08X)=0x%08X\n",
			process->abortEventFlag, ret);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
none:
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/*
 * The running thread leaves its CPU debug-suspended for reason (interrupts
 * off, on the way out of the kernel): its fast mutexes' locks are dropped,
 * its run time is accounted and the CPU is left idle.
 */
void _debugSuspendThreadSelf(uint32_t reason)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadmgrFastMutex *mutex;
	ThreadmgrTraceHook hook;
	uint64_t ran;
	uint32_t mpidr;
	uintptr_t self = (uintptr_t)thread;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 9498);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() != NULL, 9500);
	SCE_DBG_ASSERT_CHECK(thread == currentThreadContext(), 9501);
	for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
	     (uintptr_t)mutex != (uintptr_t)&thread->ownedFastMutexNext; mutex = mutex->next) {
		SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 9509);
		sceKernelSpinlockLowUnlock(&mutex->lock);
	}
	ran = thread->runClock;
	thread->runClock = SceSysrootForKernel_E20F6FC8() + (ran - cpu->lastThreadSwitchTime);
	thread->threadReleaseCount++;
	hook = s_threadmgrRuntime.switchHook;
	if (hook != NULL)
		hook(2, 5, thread->threadId, thread->currentPriority, -1, -1, 8);
	if (s_threadmgrRuntime.manager->paHooks[0] != 0) {
		__asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
		issueCallPAScheduleHookIntrRequest(mpidr & 3, thread->processId, thread->threadId, 2,
			     thread->currentPriority);
	}
	notifyOffCpu(8);
	manager = s_threadmgrRuntime.manager;
	cpu->priority = 0xfe;
	cpu->processId = -1;
	cpu->threadId = -1;
	cpu->runningThread = 0;
	thread->status = ((thread->status << 16) & 0x7fff0000) | THREAD_STATE_DEBUG_SUSPENDED |
			     THREAD_STATE_OFF_CPU;
	thread->debugSuspendReason |= reason;
	thread->currentCpuId = -1;
	SceCpuForDriver_BF82DEB2(&manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext;
	thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
	s_threadmgrRuntime.manager->debugSuspendedPrev = self;
	*(uintptr_t *)thread->prev = self;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/* Per-CPU cache of reusable extended kernel-stack memblocks. */
volatile struct ThreadmgrExtendedStackCache s_extendedStackCaches[4];

SceUID popExtendedStackCache(void **base)
{
	volatile struct ThreadmgrExtendedStackCache *cache;
	SceKernelIntrStatus intrState;
	uint32_t count;
	SceUID uid = -1;
	unsigned int slot;

	intrState = SceCpuForDriver_821FC0EE();
	cache = &s_extendedStackCaches[sceKernelCpuId()];
	count = cache->count;
	if (!count)
		goto done;
	for (slot = 0; slot < 3; slot++) {
		uid = cache->memblockIds[slot];
		if (uid != -1)
			break;
	}
	if (slot == 3)
		goto done;
	*base = (void *)cache->mappedBases[slot];
	cache->memblockIds[slot] = -1;
	cache->mappedBases[slot] = 0;
	cache->count = count - 1;
done:
	sceKernelCpuResumeIntr(intrState);
	return uid;
}

int pushExtendedStackCache(SceUID uid, void *base)
{
	volatile struct ThreadmgrExtendedStackCache *cache;
	SceKernelIntrStatus intrState;
	uint32_t count;
	unsigned int slot;
	int result = -1;

	intrState = SceCpuForDriver_821FC0EE();
	cache = &s_extendedStackCaches[sceKernelCpuId()];
	count = cache->count;
	if (cache->capacity <= count)
		goto done;
	for (slot = 0; slot < 3; slot++) {
		if (cache->memblockIds[slot] == -1)
			break;
	}
	if (slot == 3)
		goto done;
	cache->memblockIds[slot] = uid;
	cache->mappedBases[slot] = (uintptr_t)base;
	cache->count = count + 1;
	result = 0;
done:
	sceKernelCpuResumeIntr(intrState);
	return result;
}

/* the per-CPU stack caches empty, and no hooks */
int module_start(SceSize arglen, const void *argp)
{
	volatile struct ThreadmgrExtendedStackCache *cache;
	unsigned int cpu;

	for (cpu = 0; cpu < 4; cpu++) {
		cache = &s_extendedStackCaches[cpu];
		cache->count = 0;
		cache->capacity = 3;
		cache->memblockIds[0] = -1;
		cache->mappedBases[0] = 0;
		cache->memblockIds[1] = -1;
		cache->mappedBases[1] = 0;
		cache->memblockIds[2] = -1;
		cache->mappedBases[2] = 0;
	}
	s_threadmgrRuntime.switchHook = NULL;
	s_threadmgrRuntime.schedTraceHook = NULL;
	s_threadmgrRuntime.threadTraceHook = NULL;
	s_threadmgrRuntime.onCpuHook = NULL;
	s_threadmgrRuntime.offCpuHook = NULL;
	s_threadmgrRuntime.priorityChangeHook = NULL;
	s_threadmgrRuntime.vfpExceptionHook = NULL;
	s_threadmgrRuntime.alarmTraceHook = NULL;
	return 0;
}

/* SceKernelThreadMgr: saved-context initialization and exit setup. */
/* Sony also inlines this initializer at saved-context construction sites. */
void initRegisterContext(struct ThreadRegisterContext *regs)
{
	int i;

	regs->full = 1;
	for (i = 0; i < 15; i++)
		regs->r[i] = 0xdeadbeef;
	regs->status = 0;
	regs->tpidruro = 0;
	regs->pmselr = 0;
	regs->dfsr = 0;
	regs->ifsr = 0;
	regs->dfar = 0;
	regs->ifar = 0;
	regs->reserved0 = 0;
	regs->pc = 0;
	regs->cpsr = 0;
	regs->teehbr = 0x2000;
	regs->priorityMask = 0xff;
}

#define REGISTER_POISON			0xdeadbeef
#define VFP_POISON_LOW			0x7f80dead	/* a signalling NaN in each single */
#define VFP_POISON_HIGH			0x7ff8dead
#define FPSCR_DEFAULT			0x03000000	/* DN | FZ */
#define FPEXC_EN			0x40000000
#define CPSR_MODE_SYS_MASKED		0xdf	/* SYS mode, IRQ and FIQ masked */
#define CPSR_MODE_SYS			0x1f
#define CPSR_THUMB			0x20
#define THREAD_EXIT_MODE			0x87	/* PLACEHOLDER: thread +0x14 of a thread set up to exit */
void __attribute__((noreturn)) _sceKernelExitCallback(int result);

/* a fresh VFP state: every register a NaN, default FPSCR, VFP enabled */
void initVfpContext(uintptr_t vfp)
{
	struct ThreadmgrVfpContext *context = (struct ThreadmgrVfpContext *)vfp;
	uint32_t *reg = context->s;

	memset(context, 0, sizeof(*context));
	while (reg != context->s + 64) {
		reg[0] = VFP_POISON_LOW;
		reg[1] = VFP_POISON_HIGH;
		reg += 2;
	}
	context->fpscr = FPSCR_DEFAULT;
	context->fpexc = FPEXC_EN;
}

/*
 * PLACEHOLDER: name. Points a stopped thread's saved context at its end:
 * exitDeleteThread, or _sceKernelExitCallback when it is in a callback. A
 * user thread gets a syscall frame on its kernel stack first, as if it had
 * called sceKernelExitThread.
 */
void setupThreadExit(struct ThreadCB *thread, uint32_t request)
{
	struct ThreadRegisterContext *regs;
	ThreadSyscallFrame *frame;
	SceProcessObject *process;
	uintptr_t vfp, entry;

	if ((int)thread->currentAttr < 0) {
		uint32_t *dest;
		const uint32_t *source;

		frame = (ThreadSyscallFrame *)(thread->kernelStackTop - sizeof(*frame));
		regs = thread->pRegisterContext;
		dest = frame->r;
		source = regs->r;
		do {
			*dest++ = *source++;
		} while (dest != frame->r + 13);
		vfp = thread->pVfpRegisterContext;
		frame->syscallAux = regs->syscallAux;
		frame->entry = (uintptr_t)sceKernelExitThread;
		if (vfp != 0)
			frame->fpscr = ((struct ThreadmgrVfpContext *)vfp)->fpscr;
		frame->apiDeclare = 0;
		frame->tpidrurw = regs->tpidrurw;
		frame->sp = regs->sp;
		frame->lr = regs->lr;
		frame->pc = regs->pc;
		frame->cpsr = regs->cpsr;
		thread->userFrame = frame;
	} else {
		regs = thread->pRegisterContext;
		frame = NULL;
		vfp = thread->pVfpRegisterContext;
	}
	initRegisterContext(regs);
	if (thread->callbackNest < 1 && thread->threadEventHandler == NULL) {
		entry = (uintptr_t)exitDeleteThread;
		regs->r[0] = (request & THREAD_FLAG_REQUEST_EXIT_DELETE) != 0;
		regs->r[1] = 0;
		regs->r[2] = SCE_KERNEL_ERROR_THREAD_STOPPED;
		regs->cpsr = CPSR_MODE_SYS_MASKED;
	} else {
		entry = (uintptr_t)_sceKernelExitCallback;
		regs->r[0] = 0;
		regs->cpsr = CPSR_MODE_SYS;
	}
	thread->vfpException = THREAD_EXIT_MODE;
	regs->full = 1;
	if ((int)thread->currentAttr < 0) {
		regs->tpidruro = 1;
		regs->sp = (uintptr_t)frame;
	} else {
		regs->sp = thread->initSp;
	}
	regs->pc = entry & ~1;
	if (entry & 1)
		regs->cpsr |= CPSR_THUMB;
	regs->cpacr = CPACR_VFP_FULL;
	regs->lr = REGISTER_POISON;
	initVfpContext(vfp);
	process = thread->process;
	thread->pRegisterContext->pmuserenr = process->defaultPmuserenr;
	thread->pRegisterContext->pmcr = process->defaultPmcr;
	thread->ioFlags = 0;
	thread->permission = thread->initPermission;
	thread->tlsProcessId = thread->processId;
}

/*
 * SceKernelThreadMgr (threadmgr.c): the ready queues. Priorities 0x80-0xfb
 * share the manager's global queues (any CPU may take them); the others
 * are per CPU. A thread for another CPU goes on that CPU's standby list
 * until the CPU moves it to its queues.
 */

#define STANDBY_RETRY_DELAY		10

typedef void (*ThreadmgrQueueProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
				    SceUID threadId, SceUID processId, unsigned int cpu);

#define SCHED_PROBE(symbol, probeName)						\
	static uint32_t symbol##ProbeId;						\
	volatile uintptr_t symbol##Handler;					\
	extern const char symbol##CodeSite[];					\
	const struct ThreadmgrSchedProbeDescriptor symbol##Descriptor = {	\
		.probeId = &symbol##ProbeId, .provider = "__sched_", .name = probeName, \
		.codeSite = symbol##CodeSite, .handler = &symbol##Handler,	\
	}

SCHED_PROBE(s_schedGlobalEnqueue, "enqueue");
SCHED_PROBE(s_schedCpuEnqueue, "enqueue");
SCHED_PROBE(s_schedGlobalDequeue, "dequeue");
SCHED_PROBE(s_schedCpuDequeue, "dequeue");
SCHED_PROBE(s_schedGlobalEnqueueTop, "enqueue");
SCHED_PROBE(s_schedCpuEnqueueTop, "enqueue");

/* the ID the probes report: the thread's UID in its process, if it has one */
static inline SceUID probeThreadId(volatile struct ThreadCB *thread)
{
	SceUID userThreadId = thread->userThreadId;

	return userThreadId < 0 ? thread->threadId : userThreadId;
}

static inline unsigned int cpuIndex(struct ThreadMgrCpuCB *cpu)
{
	return -(s_threadmgrRuntime.manager->cpuCB - cpu);
}

/* CLZ: 32 for 0 */
static inline uint32_t leadingZeros(uint32_t value)
{
	return value != 0 ? (uint32_t)__builtin_clz(value) : 32;
}

/* ROR by a register: the amount modulo 32 */
static inline uint32_t rotateRight(uint32_t value, uint32_t shift)
{
	shift &= 31;
	return (value >> shift) | (value << ((-shift) & 31));
}

/* LSR by a register: the low byte of the amount, 0 from 32 on */
static inline uint32_t shiftRight(uint32_t value, uint32_t shift)
{
	shift &= 0xff;
	return shift < 32 ? value >> shift : 0;
}

/* thread goes before node (at the end of the queue node heads, or in front of node) */
static inline void insertBefore(uintptr_t node, struct ThreadCB *thread)
{
	struct ThreadCB *head = (struct ThreadCB *)node;

	thread->next = node;
	thread->prev = head->prev;
	head->prev = (uintptr_t)thread;
	((struct ThreadCB *)thread->prev)->next = (uintptr_t)thread;
}

static inline void unlinkQueuedThread(struct ThreadCB *thread)
{
	uintptr_t next = thread->next;

	((struct ThreadCB *)next)->prev = thread->prev;
	((struct ThreadCB *)thread->prev)->next = next;
}

/* the manager's list at head takes thread */
#define listManagerThread(head, thread) do {					\
	SceCpuForDriver_BF82DEB2(						\
		&s_threadmgrRuntime.manager->threadMgrCBLock); \
	insertBefore((head), (thread));						\
	sceKernelSpinlockLowUnlock(						\
		&s_threadmgrRuntime.manager->threadMgrCBLock); \
} while (0)

/* thread waits on cpu's standby list for cpu to take it */
void addStandby(struct ThreadMgrCpuCB *state, struct ThreadCB *thread)
{
	struct ThreadMgrCpuCB *cpu = state;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1300);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 1303);
	SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
	insertBefore((uintptr_t)&cpu->standbyNext, thread);
	sceKernelSpinlockLowUnlock(&cpu->standbyLock);
}

/* thread joins the end of its priority's ready queue */
void addReady(struct ThreadCB *thread)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrQueueProbe probe;
	int priority, shared;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1384);
	priority = context->currentPriority;
	if ((uint32_t)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
		/* the DTrace probe site __sched_:enqueue */
		__asm__ volatile(".global s_schedGlobalEnqueueCodeSite\ns_schedGlobalEnqueueCodeSite:");
		probe = (ThreadmgrQueueProbe)s_schedGlobalEnqueueHandler;
		if (probe != NULL) {
			probe(&s_schedGlobalEnqueueDescriptor, probeThreadId(context),
			      context->process->pid, CPU_ID_GLOBAL);
			priority = context->currentPriority;
		}
		shared = priority - PRIORITY_SHARED_LOW;
		SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->globalReadyLock);
		s_threadmgrRuntime.manager->globalReadyBitmap[shared / 32] |=
			0x80000000u >> (shared % 32);
		insertBefore((uintptr_t)&s_threadmgrRuntime.manager->globalReadyQueue[shared], context);
		sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
		context->currentCpuId = CPU_ID_GLOBAL;
		return;
	}
	/* the DTrace probe site __sched_:enqueue */
	__asm__ volatile(".global s_schedCpuEnqueueCodeSite\ns_schedCpuEnqueueCodeSite:");
	cpu = currentCpuState();
	probe = (ThreadmgrQueueProbe)s_schedCpuEnqueueHandler;
	if (probe != NULL)
		probe(&s_schedCpuEnqueueDescriptor, probeThreadId(context),
		      context->process->pid, cpuIndex(cpu));
	priority = context->currentPriority;
	cpu->readyBitmap[priority / 32] |= 0x80000000u >> (priority % 32);
	insertBefore((uintptr_t)&cpu->readyQueue[priority], context);
	context->currentCpuId = sceKernelCpuId();
}

/* this CPU takes the threads on its standby list into its queues; returns how many */
unsigned int moveStandbyToReady(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread;
	volatile unsigned int delay;
	unsigned int count = 0;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1330);
	for (;;) {
		SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
		thread = (struct ThreadCB *)cpu->standbyNext;
		while (thread != (struct ThreadCB *)&cpu->standbyNext) {
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1343);
			if (SceCpuForDriver_5AC9D394(THREAD_LOCK(thread)) < 0)
				break;
			unlinkQueuedThread(thread);
			sceKernelSpinlockLowUnlock(&cpu->standbyLock);
			SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 1356);
			thread->status = (thread->status & THREAD_STATE_HIGH_MASK) |
					     SCE_KERNEL_THREAD_STATUS_READY;
			addReady((struct ThreadCB *)thread);
			if (thread->currentPriority < cpu->priority) {
				SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 1363);
				cpu->nextRunningThread = 0;
			}
			count++;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
			thread = (struct ThreadCB *)cpu->standbyNext;
		}
		sceKernelSpinlockLowUnlock(&cpu->standbyLock);
		if (thread == (struct ThreadCB *)&cpu->standbyNext)
			return count;
		/* a thread was locked by someone: again shortly */
		for (delay = STANDBY_RETRY_DELAY; delay != 0; delay--)
			;
	}
}

/* thread goes to the front of its priority's ready queue (it was preempted) */
void addReadyTop(struct ThreadCB *thread)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrQueueProbe probe;
	int priority, shared;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1430);
	priority = context->currentPriority;
	if ((uint32_t)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
		/* the DTrace probe site __sched_:enqueue */
		__asm__ volatile(".global s_schedGlobalEnqueueTopCodeSite\n"
				 "s_schedGlobalEnqueueTopCodeSite:");
		probe = (ThreadmgrQueueProbe)s_schedGlobalEnqueueTopHandler;
		if (probe != NULL) {
			probe(&s_schedGlobalEnqueueTopDescriptor, probeThreadId(context),
			      context->process->pid, CPU_ID_GLOBAL);
			priority = context->currentPriority;
		}
		shared = priority - PRIORITY_SHARED_LOW;
		SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->globalReadyLock);
		s_threadmgrRuntime.manager->globalReadyBitmap[shared / 32] |=
			0x80000000u >> (shared % 32);
		insertBefore(s_threadmgrRuntime.manager->globalReadyQueue[shared].next, context);
		sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
		context->currentCpuId = CPU_ID_GLOBAL;
		return;
	}
	/* the DTrace probe site __sched_:enqueue */
	__asm__ volatile(".global s_schedCpuEnqueueTopCodeSite\ns_schedCpuEnqueueTopCodeSite:");
	cpu = currentCpuState();
	probe = (ThreadmgrQueueProbe)s_schedCpuEnqueueTopHandler;
	if (probe != NULL)
		probe(&s_schedCpuEnqueueTopDescriptor, probeThreadId(context),
		      context->process->pid, cpuIndex(cpu));
	priority = context->currentPriority;
	cpu->readyBitmap[priority / 32] |= 0x80000000u >> (priority % 32);
	insertBefore(cpu->readyQueue[priority].next, context);
	context->currentCpuId = sceKernelCpuId();
}

/* thread leaves the global queue of priority (the global lock held) */
void removeCommonRaw(struct ThreadCB *thread, int priority)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrQueueProbe probe;
	int shared;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1471);
	SCE_DBG_ASSERT_CHECK(s_threadmgrRuntime.manager->globalReadyLock != 0, 1474);
	if (context->next == context->prev && context->next != (uintptr_t)context) {
		/* the last of its queue */
		shared = priority - PRIORITY_SHARED_LOW;
		s_threadmgrRuntime.manager->globalReadyBitmap[shared / 32] &=
			~(0x80000000u >> (shared % 32));
	}
	unlinkQueuedThread(context);
	/* the DTrace probe site __sched_:dequeue */
	__asm__ volatile(".global s_schedGlobalDequeueCodeSite\ns_schedGlobalDequeueCodeSite:");
	probe = (ThreadmgrQueueProbe)s_schedGlobalDequeueHandler;
	if (probe != NULL) {
		cpu = currentCpuState();
		probe(&s_schedGlobalDequeueDescriptor, probeThreadId(context),
		      context->process->pid, cpuIndex(cpu));
	}
}

/* thread leaves its ready queue (priority: the queue it is on) */
void removeReady(struct ThreadCB *thread, int priority)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrQueueProbe probe;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1500);
	if ((uint32_t)context->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
		SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->globalReadyLock);
		removeCommonRaw(thread, priority);
		sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->globalReadyLock);
		return;
	}
	cpu = currentCpuState();
	if (context->next == context->prev && context->next != (uintptr_t)context)
		cpu->readyBitmap[priority / 32] &= ~(0x80000000u >> (priority % 32));
	unlinkQueuedThread(context);
	/* the DTrace probe site __sched_:dequeue */
	__asm__ volatile(".global s_schedCpuDequeueCodeSite\ns_schedCpuDequeueCodeSite:");
	probe = (ThreadmgrQueueProbe)s_schedCpuDequeueHandler;
	if (probe != NULL)
		probe(&s_schedCpuDequeueDescriptor, probeThreadId(context),
		      context->process->pid, cpuIndex(cpu));
}

/* the CPU thread should run on, of the ones it may (UINT32_MAX: none for now) */
int selectExecCpu(struct ThreadCB *thread)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadMgrCpuCB *state;
	SceProcessObject *process;
	uint32_t affinity, mask, preferred, sameProcess, lastCpu, rotation, selected;
	int bestPriority, priority;
	unsigned int cpu;

	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 1547);
	process = thread->process;
	if (process->threadsStopped != 0 && !(thread->flags >> THREAD_FLAG_REQUEST_SHIFT))
		return -1;
	affinity = thread->currentCpuAffinityMask;
	if (affinity == 0)
		affinity = process->cpuAffinity;
	mask = s_threadmgrRuntime.manager->activeCpuMask & affinity & CPU_MASK_ALL;
	if (((mask - 1) & mask) == 0)
		return 31 - leadingZeros(mask);

	/* the CPUs running the least urgent threads */
	preferred = 0;
	bestPriority = thread->currentPriority + 1;
	for (cpu = 0, state = manager->cpuCB; cpu < CPU_COUNT; cpu++, state++) {
		if (!(mask & (1u << cpu)))
			continue;
		priority = state->priority;
		if (bestPriority > priority)
			continue;
		if (bestPriority != priority)
			preferred = 0;
		preferred += 1u << cpu;
		bestPriority = priority;
	}
	if (preferred != 0) {
		if (((preferred - 1) & preferred) == 0)
			return 31 - leadingZeros(preferred);
		/* of those, the ones running its process */
		sameProcess = preferred;
		for (cpu = 0, state = manager->cpuCB; cpu < CPU_COUNT; cpu++, state++) {
			if (state->processId != thread->processId)
				sameProcess &= ~(1u << cpu);
		}
		if (((sameProcess - 1) & sameProcess) == 0) {
			if (sameProcess != 0)
				return 31 - leadingZeros(sameProcess);
			sameProcess = preferred;
		}
		mask = sameProcess;
	}
	lastCpu = thread->lastExecutedCpuId;
	if (lastCpu != UINT32_MAX && ((mask >> lastCpu) & 1))
		return lastCpu;
	/* round robin */
	rotation = s_threadmgrRuntime.manager->lastSelectedCpu;
	selected = (rotation - 1 - __builtin_clz(rotateRight(mask, rotation))) & (CPU_COUNT - 1);
	s_threadmgrRuntime.manager->lastSelectedCpu = selected;
	return selected;
}

/*
 * thread is ready to run on cpuId (selectExecCpu's choice): queued here, on
 * the global queues or that CPU's standby list; a debug-suspended thread
 * and one no CPU may run are listed aside. Returns whether this CPU should
 * switch now.
 */
bool distributeThread(int cpuId, struct ThreadCB *thread)
{
	struct ThreadCB *context = thread;
	struct ThreadMgrCpuCB *current = currentCpuState();
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadMgrCpuCB *target;
	bool reschedule = false;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1677);
	SCE_DBG_ASSERT_CHECK(context->lock != 0, 1680);
	SCE_DBG_ASSERT_CHECK(!(context->flags & THREAD_FLAG_EXCEPTION), 1682);
	if (cpuId == -1) {
		/* stagnant: no active CPU may run it */
		context->currentCpuId = -1;
		context->status = (context->status & THREAD_STATE_HIGH_MASK) |
				      SCE_KERNEL_THREAD_STATUS_STAGNANT;
		listManagerThread((uintptr_t)&s_threadmgrRuntime.manager->stagnantNext, context);
		goto out;
	}
	if ((int32_t)context->status < 0 && !(context->flags & THREAD_FLAG_EXITING) &&
	    (context->userFrame == NULL || (context->flags & THREAD_FLAG_SELF_STOPPED))) {
		/* debug-suspended */
		context->status = ((context->status << 16) & 0x7fffffff) |
				      THREAD_STATE_DEBUG_SUSPENDED | THREAD_STATE_OFF_CPU;
		listManagerThread((uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext,
				  context);
		context->currentCpuId = -1;
		goto out;
	}
	if ((int)sceKernelCpuId() == cpuId) {
		context->status = (context->status & THREAD_STATE_HIGH_MASK) |
				      SCE_KERNEL_THREAD_STATUS_READY;
		addReady(thread);
		reschedule = context->currentPriority < current->priority;
		if (reschedule) {
			SCE_DBG_ASSERT_LEVEL(0, current == currentCpuState(), 1719);
			current->nextRunningThread = 0;
		}
		SCE_DBG_ASSERT_CHECK(context->currentCpuId == (int)sceKernelCpuId() ||
				     context->currentCpuId == CPU_ID_GLOBAL, 1725);
		goto out;
	}
	target = &manager->cpuCB[cpuId];
	if ((uint32_t)context->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
		context->status = (context->status & THREAD_STATE_HIGH_MASK) |
				      SCE_KERNEL_THREAD_STATUS_READY;
		addReady(thread);
		if (context->currentPriority < target->priority)
			addIntrRequest(cpuId, INTR_MASK_SCHEDULING);
		SCE_DBG_ASSERT_CHECK(context->currentCpuId == CPU_ID_GLOBAL, 1734);
	} else {
		context->status = (context->status & THREAD_STATE_HIGH_MASK) |
				      SCE_KERNEL_THREAD_STATUS_STANDBY;
		addStandby((struct ThreadMgrCpuCB *)target, thread);
		if (context->currentPriority < target->priority)
			addIntrRequest(cpuId, INTR_MASK_SCHEDULING);
		context->currentCpuId = cpuId;
	}
out:
	return reschedule;
}

/*
 * SceKernelThreadMgr (threadmgr.c): starting a dormant thread. Its start
 * argument is copied to the top of its stack, its contexts are reset, and a
 * user thread also gets its user context, TLS words and VFP area on its
 * kernel stack; then it is queued to run threadEntry.
 */

#define DEBUG_THREAD_EVENT_START	2
#define TRACE_EVENT_THREAD_START	2
#define CPSR_START_MASKED		0xff	/* SYS mode, Thumb, IRQ and FIQ masked */
#define CPSR_USR_ARM			0x50	/* USR mode, FIQ masked */
#define CPSR_USR_THUMB			0x70
#define USER_TLS_AFFINITY		0x7dc	/* the initial CPU affinity mask << 16 */
#define USER_CONTEXT_SIZE		0x100
#define USER_CONTEXT_ALIGN		0x40
#define ACCESS_KERNEL			0x10	/* PLACEHOLDER: SceSysmemForDriver_9C78064C: kernel memory */
#define SDK_VERSION_DEFAULT_AFFINITY	0x02100000
#define SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT	0x80020005	/* PLACEHOLDER: name */
void __attribute__((noreturn)) threadEntry(void);

/* what a kernel argument copied to a user stack is replaced with (cleared at boot) */
uint8_t s_zeroBlock[0x80];

/*
 * Starts the dormant thread threadId with argSize bytes at arg; checkSource:
 * an argument in kernel memory is not given to a user thread (zeros are).
 */
int startThread(SceUID threadId, SceSize argSize, const void *arg, int checkSource)
{
	struct ThreadCB *self = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadRegisterContext *regs, *user;
	struct ThreadmgrDebugThreadEvent message;
	SceProcessObject *process;
	ThreadmgrDebugHandler handler;
	ThreadmgrTraceHook trace;
	uintptr_t sp, vfp, stackRange[2];
	SceSize stackSize, done, chunk;
	SceUID userThreadId, processId;
	SceUInt32 version, mask;
	uint32_t saved;
	int intr, ret, wake, group;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 4454, "_sceKernelStartThread",
					   "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (self->threadEventMask != 0) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 4462, "_sceKernelStartThread",
			"*** error: illegal context(thread event handler). threadId=0x%08x[%s]\n",
			threadId, SceSysmemForDriver_E655852F(threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (threadId == 0 || self->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(self));
	if (self->threadEventMask != 0) {
		sceKernelSpinlockLowUnlock(THREAD_LOCK(self));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto resume;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(self));
	ret = SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = ThreadmgrInSyscall() ?
			(int)(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT) :
			(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	userThreadId = thread->userThreadId;
	if (thread->threadEventMask != 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto unlock;
	}
	if (!(thread->status & SCE_KERNEL_THREAD_STATUS_DORMANT)) {
		ret = SCE_KERNEL_ERROR_NOT_DORMANT;
		goto unlock;
	}
	stackSize = (int)thread->currentAttr < 0 ? thread->uStackSize : thread->kStackSize;
	if (stackSize < argSize) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
		goto unlock;
	}
	/* the thread ID marks the bottom of its stacks */
	*(SceUID *)thread->pKStack = thread->threadId;
	if ((int)thread->currentAttr < 0) {
		sp = thread->pUStack;
		ret = sceKernelCopyToUserProc(thread->processId, (void *)sp, &thread->threadId,
					       sizeof(thread->threadId));
		if (ret < 0)
			goto unlock;
		sp += thread->uStackSize;
	} else {
		sp = thread->pKStack + thread->kStackSize;
	}
	/* the argument at the top of the stack */
	if (arg == NULL || argSize == 0) {
		thread->argSize = 0;
		thread->pArgBlock = 0;
	} else {
		thread->argSize = argSize;
		sp -= (argSize + 15) & ~15;
		thread->pArgBlock = sp;
		if ((int)thread->currentAttr >= 0) {
			memcpy((void *)sp, arg, argSize);
		} else if (ThreadmgrInSyscall()) {
			ret = SceSysmemForDriver_1BD44DD5((void *)sp, arg, argSize);
			if (ret < 0)
				goto unlock;
		} else if (!checkSource) {
			ret = sceKernelCopyToUserProc(thread->processId, (void *)sp, arg, argSize);
			if (ret < 0)
				goto unlock;
		} else if (SceSysmemForDriver_9C78064C(ACCESS_KERNEL, arg, argSize)) {
			/* kernel memory is not handed to a user thread */
			for (done = 0; done != argSize; done += chunk) {
				chunk = argSize - done;
				if (chunk > sizeof(s_zeroBlock))
					chunk = sizeof(s_zeroBlock);
				ret = SceSysmemForDriver_6D88EF8A((void *)(sp + done), s_zeroBlock, chunk);
				if (ret < 0)
					goto unlock;
			}
		} else {
			ret = SceSysmemForDriver_1BD44DD5((void *)sp, arg, argSize);
			if (ret < 0)
				goto unlock;
		}
	}
	/* a fresh start */
	regs = thread->pRegisterContext;
	thread->currentCpuAffinityMask = thread->initCpuAffinityMask;
	thread->exitStatus = SCE_KERNEL_ERROR_NOT_DORMANT;
	thread->currentAttr = thread->initAttr;
	thread->basePriority = thread->initPriority;
	thread->currentPriority = thread->initPriority;
	thread->ioFlags = 0;
	thread->boostPriority = BOOST_PRIORITY_NONE;
	thread->permission = thread->initPermission;
	thread->tlsProcessId = thread->processId;
	initRegisterContext(regs);
	thread->initSp = sp;
	regs->cpacr = CPACR_VFP_FULL;
	initVfpContext(thread->pVfpRegisterContext);
	process = thread->process;
	regs = thread->pRegisterContext;
	regs->pmuserenr = process->defaultPmuserenr;
	regs->pmcr = process->defaultPmcr;
	if ((int)thread->currentAttr >= 0) {
		thread->flags = 0;
		thread->kernelStackTop = 0;
		goto queue;
	}
	/* a user thread: its TLS words */
	SCE_DBG_ASSERT_LEVEL_FUNC(0, thread->userFrame != NULL, 4623, "_sceKernelStartThread");
	stackRange[1] = thread->pUStack;
	thread->userFrame = NULL;
	thread->flags = THREAD_FLAG_IN_CALLBACK;
	stackRange[0] = thread->uStackSize + stackRange[1];
	ret = sceKernelCopyToUserProc(thread->processId,
				       (void *)(thread->tls - USER_TLS_STACK_RANGE), stackRange,
				       sizeof(stackRange));
	if (ret < 0)
		goto unlock;
	ret = sceKernelCopyToUserProc(thread->processId, (void *)(thread->tls - USER_TLS_PRIORITY),
				       &thread->initPriority, sizeof(thread->initPriority));
	if (ret < 0)
		goto unlock;
	if (thread->initCpuAffinityMask != 0) {
		mask = thread->initCpuAffinityMask << 16;
	} else if (self->processId == THREADMGR_KERNEL_PID ||
		   (SceSysrootForDriver_CAE820B2(self->processId, &version) >= 0 &&
		    version >= SDK_VERSION_DEFAULT_AFFINITY)) {
		mask = 0;
	} else {
		mask = thread->process->cpuAffinity << 16;
	}
	ret = sceKernelCopyToUserProc(thread->processId, (void *)(thread->tls - USER_TLS_AFFINITY),
				       &mask, sizeof(mask));
	if (ret < 0)
		goto unlock;
	/* its user context and VFP area on its kernel stack, entered through threadEntry */
	user = (struct ThreadRegisterContext *)
		((thread->kernelStackTop - USER_CONTEXT_SIZE) & ~(USER_CONTEXT_ALIGN - 1));
	memset(user, 0, USER_CONTEXT_SIZE);
	initRegisterContext(user);
	user->full = 1;
	user->r[0] = thread->argSize;
	user->r[1] = thread->pArgBlock;
	user->sp = thread->initSp;
	user->pc = thread->entry & ~1;
	user->cpsr = (thread->entry & 1) ? CPSR_USR_THUMB : CPSR_USR_ARM;
	user->lr = (uint32_t)SceSysrootForKernel_5F2CE289(thread->process);
	user->cpacr = CPACR_VFP_FULL;
	user->tpidruro = thread->tls;
	regs = thread->pRegisterContext;
	user->pmuserenr = process->defaultPmuserenr;
	user->pmcr = process->defaultPmcr;
	user->ttbr1 = regs->ttbr1;
	user->contextidr = regs->contextidr;
	user->kernelDacr = regs->kernelDacr;
	user->userDacr = regs->userDacr;
	SCE_DBG_ASSERT_CHECK_FUNC(regs->callbackContext == NULL, 4701, "_sceKernelStartThread");
	regs->callbackContext = user;
	SCE_DBG_ASSERT_CHECK_FUNC(regs->callbackVfp == 0, 4704, "_sceKernelStartThread");
	vfp = (uintptr_t)user - sizeof(struct ThreadmgrVfpContext) - 0x20;
	memset((void *)vfp, 0, sizeof(struct ThreadmgrVfpContext));
	initVfpContext(vfp);
	thread->pRegisterContext->callbackVfp = vfp;
	thread->kernelStackTop = vfp;
	regs = thread->pRegisterContext;
queue:
	regs->full = 1;
	regs->sp = (int)thread->currentAttr < 0 ? thread->kernelStackTop : thread->initSp;
	regs->lr = 0;
	regs->pc = (uintptr_t)threadEntry;
	regs->cpsr |= CPSR_START_MASKED;
	SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
	((struct ThreadmgrWaitNode *)thread->next)->prev = (struct ThreadmgrWaitNode *)thread->prev;
	((struct ThreadmgrWaitNode *)thread->prev)->next = (struct ThreadmgrWaitNode *)thread->next;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	wake = distributeThread(selectExecCpu(thread), thread);
	processId = thread->processId;
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_THREAD_START, 4, processId, threadId, argSize, arg);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(threadId);
	for (group = 0; group < 2; group++) {
		handler = s_threadmgrRuntime.manager->debugHandlers[group][0];
		if (handler == NULL)
			continue;
		message.processId = processId;
		message.threadId = userThreadId;
		message.event = DEBUG_THREAD_EVENT_START;
		handler(sizeof(message), &message);
	}
	ret = 0;
	if (wake)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* _sceKernelStartThread */
int _sceKernelStartThread(SceUID threadId, SceSize argSize, const void *arg)
{
	return sceKernelStartThreadForUser(threadId, argSize, arg);
}

/*
 * SceKernelThreadMgr (threadmgr.c): the end of a thread. exitDeleteThread
 * runs on the thread itself: it tells the debugger and the tracers, lets go
 * of everything the thread holds, wakes the threads waiting for its end and
 * leaves the CPU for good, dormant or (deleted) for the cleaner thread.
 */

#define THREAD_EVENT_EXIT		0x00000008	/* callThreadEventHandlers */
#define WAIT_TYPE_THREAD_END		0x00000002
#define DEBUG_THREAD_EVENT_EXIT		3	/* the exit status comes with it */

/* the tracer's events (s_threadmgrRuntime.threadTraceHook) */
#define TRACE_EVENT_THREAD_EXIT		3
#define TRACE_EVENT_THREAD_EXIT_DELETE	4

#define THREAD_STATE_HOOK_EXIT		3

typedef void (*ThreadmgrStateHook)(SceSize size, void *message);
typedef void (*ThreadmgrLwpExitProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
				      SceUID threadId, SceUID processId);
typedef void (*ThreadmgrExitProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
				   int status);

/* what the thread state hook (paHooks[0]) hears */
struct ThreadmgrThreadStateMessage {
	SceUID processId;
	SceUID threadId;
	uint32_t state;				/* THREAD_STATE_HOOK_* */
	int priority;
};

_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, deletedNext) == 0x2218 &&
	       offsetof(struct SceKernelThreadmgrPrivate, dormantNext) == 0x2230 &&
	       offsetof(struct SceKernelThreadmgrPrivate, cleanerThread) == 0x2260 &&
	       offsetof(SceProcessObject, exitThread) == 0x70,
	       "thread end lists");
void deleteAllCallback(struct ThreadCB *thread);
uint32_t releaseAllFastMutex(struct ThreadCB *thread);
uint32_t releaseAllRWLock(struct ThreadCB *thread);

static uint32_t s_procLwpExitProbeId;
volatile uintptr_t s_procLwpExitHandler;
extern const char s_procLwpExitCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_procLwpExitDescriptor = {
	.probeId = &s_procLwpExitProbeId,
	.provider = "__proc_",
	.name = "lwp__exit",
	.codeSite = s_procLwpExitCodeSite,
	.handler = &s_procLwpExitHandler,
};

static uint32_t s_procExitProbeId;
volatile uintptr_t s_procExitHandler;
extern const char s_procExitCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_procExitDescriptor = {
	.probeId = &s_procExitProbeId,
	.provider = "__proc_",
	.name = "exit",
	.codeSite = s_procExitCodeSite,
	.handler = &s_procExitHandler,
};

void notifyDebugThreadEvent(SceUID processId, SceUID threadId, uint32_t event, int *status);

/* the thread goes at the end of one of the manager's lists */
static inline void listThread(struct ThreadCB *thread, uintptr_t *list)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;
	struct ThreadmgrWaitNode *head = (struct ThreadmgrWaitNode *)list;

	self->next = head;
	self->prev = head->prev;
	head->prev = self;
	self->prev->next = self;
}

/* wakes the cleaner thread to delete what is on the deleted list */
static inline void activateCleanerThread(void)
{
	struct ThreadCB *cleaner;

	SCE_DBG_ASSERT_CHECK_FUNC(ThreadmgrIntrDisabled(), 1770, "activateCleanerThread");
	cleaner = s_threadmgrRuntime.manager->cleanerThread;
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(cleaner));
	if (cleaner->status & SCE_KERNEL_THREAD_STATUS_WAITING)
		distributeThread(selectExecCpu(cleaner), cleaner);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(cleaner));
}

/* ends the running thread with status; delete: it is deleted too */
void exitDeleteThread(int delete, int intr, int status)
{
	struct ThreadCB *thread;
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrWaitNode *head, *node;
	struct ThreadmgrThreadStateMessage message;
	ThreadmgrLwpExitProbe lwpExit;
	ThreadmgrExitProbe exit;
	ThreadmgrStateHook stateHook;
	ThreadmgrTraceHook trace;
	SceProcessObject *process;
	SceUID threadId;
	int debugStatus;

	SceCpuForDriver_821FC0EE();
	thread = currentThreadContext();
	SCE_DBG_ASSERT_LEVEL(0, thread != NULL, 4944);
	callThreadEventHandlers(THREAD_EVENT_EXIT);
	debugStatus = status;
	notifyDebugThreadEvent(thread->processId, thread->userThreadId, DEBUG_THREAD_EVENT_EXIT,
			       &debugStatus);
	/* the DTrace probe site __proc_:lwp__exit */
	__asm__ volatile(".global s_procLwpExitCodeSite\ns_procLwpExitCodeSite:");
	lwpExit = (ThreadmgrLwpExitProbe)s_procLwpExitHandler;
	if (lwpExit != NULL) {
		threadId = thread->userThreadId;
		if (threadId < 0)
			threadId = thread->threadId;
		lwpExit(&s_procLwpExitDescriptor, threadId,
			thread->process->pid);
	}
	process = thread->process;
	if (process->exitThread == thread->threadId) {
		/* the DTrace probe site __proc_:exit */
		__asm__ volatile(".global s_procExitCodeSite\ns_procExitCodeSite:");
		exit = (ThreadmgrExitProbe)s_procExitHandler;
		if (exit != NULL)
			exit(&s_procExitDescriptor, status);
	}
	if (delete)
		notifyDebugThreadEvent(thread->processId, thread->userThreadId,
				       DEBUG_THREAD_EVENT_DELETE, NULL);
	stateHook = (ThreadmgrStateHook)s_threadmgrRuntime.manager->paHooks[0];
	if (stateHook != NULL) {
		message.processId = thread->processId;
		message.threadId = thread->threadId;
		message.state = THREAD_STATE_HOOK_EXIT;
		message.priority = thread->currentPriority;
		stateHook(sizeof(message), &message);
	}

	/* let go of what it holds */
	releaseAllEventFlag(thread);
	releaseAllMutex(thread);
	releaseAllFastMutex(thread);
	releaseAllRWLock(thread);
	deleteAllCallback(thread);

	object = (struct ThreadmgrThreadObject *)((uintptr_t)thread -
		offsetof(struct ThreadmgrThreadObject, context));
	SceCpuForDriver_BF82DEB2(&object->waitLock);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	thread->exitStatus = status;
	releaseThreadTlsSlots(thread);
	if (thread->restartWaitMultipleEvents != NULL) {
		SCE_DBG_ASSERT_CHECK((thread->flags & THREAD_FLAG_EXITING) != 0, 4991);
		SCE_DBG_ASSERT_CHECK(thread->currentAttr >> 31, 4992);
		SceSysrootForKernel_7968F091(thread->processId, thread->restartWaitMultipleEvents);
	}
	thread->restartWaitMultipleEvents = NULL;
	if (delete) {
		thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_DELETED;
		SceCpuForDriver_BF82DEB2(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		if (thread->syslibtraceState != NULL)
			syslibtraceThreadCBExit(thread);
		listThread(thread, (uintptr_t *)&s_threadmgrRuntime.manager->deletedNext);
		sceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
	} else {
		thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_DORMANT;
		SceCpuForDriver_BF82DEB2(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		listThread(thread, (uintptr_t *)&s_threadmgrRuntime.manager->dormantNext);
		sceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
	}
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 5019);
	thread->currentCpuId = -1;
	thread->waitId = -1;

	/* the threads waiting for its end */
	head = (struct ThreadmgrWaitNode *)&object->waitNext;
	for (node = head->next; node != head; node = head->next) {
		SCE_DBG_ASSERT_CHECK(((struct ThreadCB *)node)->magic ==
				     THREAD_CB_MAGIC, 5026);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK((struct ThreadCB *)node));
		if (((struct ThreadCB *)node)->waitType == WAIT_TYPE_THREAD_END) {
			if (((struct ThreadCB *)node)->waitArg0 != 0)
				*(int *)((struct ThreadCB *)node)->waitArg0 = status;
			wakeupAndDistributeThread((struct ThreadCB *)node,
						  (uintptr_t)head, 0);
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK((struct ThreadCB *)node));
	}
	sceKernelSpinlockLowUnlock(&object->waitLock);

	if (delete) {
		activateCleanerThread();
		trace = s_threadmgrRuntime.threadTraceHook;
		if (trace != NULL)
			trace(TRACE_EVENT_THREAD_EXIT_DELETE, 3, status, thread->processId,
			      thread->threadId);
	} else {
		trace = s_threadmgrRuntime.threadTraceHook;
		if (trace != NULL)
			trace(TRACE_EVENT_THREAD_EXIT, 3, status, thread->processId,
			      thread->threadId);
	}
	currentCpuState()->nextRunningThread = 0;
	switchThread(2);
	SCE_DBG_PRINTF(0xb, 5068, " fatal error: do not reach here.(threadId=0x%08X)\n",
		       thread->threadId);
	__asm__ volatile("bkpt #0");
	SCE_PANIC(SCE_LINE(5065, 5071));
}

/*
 * SceKernelThreadMgr (threadmgr.c): the trap on the way out of a syscall.
 * The exception handlers call sceKernelTrapThreadAfterSyscall when a thread
 * leaves the kernel with something pending: a suspend or exit request, a
 * syscall the process misused, or a debugger suspension. A suspended
 * thread stops here and goes on running from here when it is resumed.
 */

#define WAIT_TYPE_THREAD_SUSPEND	0x00000008	/* PLACEHOLDER: woken when the thread is suspended */

/* what the exception handler asks for, besides the request bits above */
#define TRAP_REQUEST_CRITICAL_ERROR	0x01000000	/* the syscall's misuse is in the frame */

#define CRITICAL_ERROR_MESSAGE		0x80000000	/* criticalError: | the message */
#define SDK_VERSION_CRITICAL_ERROR	0x01800000	/* reported for SDKs since 1.80 */
#define DIPSW_CRITICAL_ERROR		0xce

/* what the debugger's handler decides */
#define CRITICAL_ERROR_ACTION_CONTINUE	0
#define CRITICAL_ERROR_ACTION_SUSPEND	2

/* what the debugger's handlers hear of a critical error */
struct ThreadmgrCriticalErrorMessage {
	SceUID processId;
	SceUID threadId;
	uint32_t r[4];				/* 0x08: r0-r3 */
	uint32_t r12;				/* 0x18 */
	uint32_t sp;				/* 0x1c */
	uint32_t lr;
	uint32_t pc;
};

_Static_assert(sizeof(struct ThreadmgrCriticalErrorMessage) == 0x28, "critical error message");
void __attribute__((noreturn)) exitDeleteThread(int status, int intr, int error);
void _debugSuspendThreadSelf(uint32_t reason);

/* the thread object a context is part of */
static inline struct ThreadmgrThreadObject *threadObject(struct ThreadCB *thread)
{
	return (struct ThreadmgrThreadObject *)((uintptr_t)thread -
		offsetof(struct ThreadmgrThreadObject, context));
}

/* takes the running thread off its CPU (its lock held) until it is resumed */
static inline void stopSelf(struct ThreadCB *thread, uint32_t reason)
{
	currentCpuState()->nextRunningThread = 0;
	SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SELF_STOPPED);
	switchThread(reason);
	SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_SELF_STOPPED);
}

/* an exit request: out of a callback first, then the thread ends */
static inline void __attribute__((noreturn, always_inline))
exitOnRequest(struct ThreadCB *thread, int delete)
{
	if (thread->callbackNest > 0 || thread->threadEventHandler != NULL)
		_sceKernelExitCallback(0);
	exitDeleteThread(delete, 0, SCE_KERNEL_ERROR_THREAD_STOPPED);
}

/* the thread goes on the manager's suspended list */
static inline void listSuspended(struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread, *head;

	SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
	head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->suspendedNext;
	self->next = head;
	self->prev = head->prev;
	head->prev = self;
	self->prev->next = self;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/*
 * Returns 1 when the syscall returns as it is, 0 when a request broke its
 * wait and it starts again.
 */
int sceKernelTrapThreadAfterSyscall(ThreadSyscallFrame *frame, uint32_t *result,
				    uint32_t request)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrWaitNode *head, *node, *next;
	struct ThreadmgrCriticalErrorMessage message;
	ThreadmgrDebugHandler handler;
	SceUInt32 sdkVersion;
	SceSize length;
	uint32_t status;
	char buffer[0x200];
	int action, ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), SCE_LINE(9571, 9576));
	SCE_DBG_ASSERT_CHECK(thread != NULL, SCE_LINE(9573, 9578));

	switch (request) {
	case THREAD_FLAG_REQUEST_VM_SUSPEND:
	case THREAD_FLAG_REQUEST_SUSPEND:
	case THREAD_FLAG_REQUEST_VM_SUSPEND | THREAD_FLAG_REQUEST_SUSPEND:
		break;
	case TRAP_REQUEST_CRITICAL_ERROR:
		if (frame == NULL || result == NULL) {
			ret = 1;
			goto debugSuspend;
		}
		if (SceSysrootForDriver_CAE820B2(0, &sdkVersion) >= 0 &&
		    sdkVersion >= SDK_VERSION_CRITICAL_ERROR &&
		    SceDipswForDriver_A98FC2FD(DIPSW_CRITICAL_ERROR)) {
			if ((int)frame->apiDeclare < 0)
				SCE_PRINTF_WITH_INFO(0, SCE_LINE(9717, 9722), "\n*** %s\n",
					(const char *)(frame->apiDeclare & ~CRITICAL_ERROR_MESSAGE));
			else
				SCE_PRINTF_WITH_INFO(0, SCE_LINE(9719, 9724), "\n*** Syscall Critical Usage Error\n");
			SCE_PRINTF_WITH_INFO(0, SCE_LINE(9721, 9726), "ProcessID: 0x%08X [%s]\n", thread->processId,
					     SceSysmemForDriver_E655852F(thread->processId));
			SCE_PRINTF_WITH_INFO(0, SCE_LINE(9722, 9727), "ThreadID:  0x%08X [%s]\n", thread->threadId,
					     SceSysmemForDriver_E655852F(thread->threadId));
			SceDebugForKernel_BCC8D0B2(thread->processId, frame, result);
			memset(buffer, 0, sizeof(buffer));
			if (sceKernelSysrootBacktrace(0, buffer, sizeof(buffer), &length, 2) >= 0)
				sceKernelSysrootPrintBacktrace(0, buffer, length, 0);
			message.processId = thread->processId;
			message.threadId = thread->threadId;
			message.r[0] = frame->r[0];
			message.r12 = frame->r[12];
			message.r[1] = frame->r[1];
			message.r[2] = frame->r[2];
			message.r[3] = frame->r[3];
			message.sp = frame->sp;
			message.lr = frame->lr;
			message.pc = frame->pc;
			handler = s_threadmgrRuntime.manager->
				debugHandlers[0][DEBUG_HANDLER_CRITICAL_ERROR];
			if (handler != NULL)
				handler(sizeof(message), &message);
			handler = s_threadmgrRuntime.manager->
				debugHandlers[1][DEBUG_HANDLER_CRITICAL_ERROR];
			action = CRITICAL_ERROR_ACTION_CONTINUE;
			if (handler != NULL)
				action = handler(sizeof(message), &message);
			if (action == CRITICAL_ERROR_ACTION_SUSPEND) {
				SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
				_debugSuspendThreadSelf(DEBUG_SUSPEND_CRITICAL_ERROR);
				stopSelf(thread, 4);
				status = thread->flags;
				if (status >> 30)
					goto exiting;
			} else {
				SCE_DBG_ASSERT_LEVEL(0, action == CRITICAL_ERROR_ACTION_CONTINUE, SCE_LINE(9786, 9791));
			}
		}
		if (frame->apiDeclare > 1) {
			if ((int)frame->apiDeclare < 0) {
				ret = 1;
				goto debugSuspend;
			}
			SCE_PANIC(SCE_LINE(9803, 9808));
		}
		*result &= ~SYSCALL_ERROR_BIT;
		ret = 1;
		goto debugSuspend;
	case TRAP_REQUEST_DEBUG_SUSPEND:
		ret = 1;
		goto debugSuspend;
	case THREAD_FLAG_REQUEST_EXIT_DELETE:
		exitOnRequest(thread, 1);
	case THREAD_FLAG_REQUEST_EXIT:
		exitOnRequest(thread, 0);
	default:
		SCE_DBG_PRINTF(0, SCE_LINE(9812, 9817), "error: unexpected exitInst(%p, 0x%08X)\n", thread, request);
		SCE_PANIC(SCE_LINE(9813, 9818));
	}

	/* the threads waiting for it to stop */
	object = threadObject(thread);
	head = (struct ThreadmgrWaitNode *)&object->waitNext;
	SceCpuForDriver_BF82DEB2(&object->waitLock);
	for (node = head->next; node != head; node = next) {
		next = node->next;
		SCE_DBG_ASSERT_CHECK(((struct ThreadCB *)node)->magic ==
				     THREAD_CB_MAGIC, SCE_LINE(9611, 9616));
		SceCpuForDriver_BF82DEB2(THREAD_LOCK((struct ThreadCB *)node));
		if (((struct ThreadCB *)node)->waitType == WAIT_TYPE_THREAD_SUSPEND)
			wakeupAndDistributeThread((struct ThreadCB *)node,
						  (uintptr_t)head, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK((struct ThreadCB *)node));
	}
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(&object->waitLock);

	if (request == THREAD_FLAG_REQUEST_VM_SUSPEND) {
		if (!(thread->flags & THREAD_FLAG_REQUEST_VM_SUSPEND)) {
			/* resumed before it stopped */
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_REQUEST_MASK);
			if (thread->flags & THREAD_FLAG_SIGNAL_DEFERRED) {
				SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
							    THREAD_FLAG_SIGNAL_DEFERRED);
				SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
							   THREAD_FLAG_RESUMED_WAIT);
				ret = 0;
			} else {
				ret = 1;
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto debugSuspend;
		}
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_REQUEST_MASK);
	} else {
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_REQUEST_MASK);
		if (request & THREAD_FLAG_REQUEST_SUSPEND) {
			thread->currentCpuAffinityMask = thread->savedCpuAffinityMask;
			thread->savedCpuAffinityMask = 0;
			thread->currentPriority = thread->basePriority < thread->boostPriority ?
				thread->basePriority : thread->boostPriority;
		}
	}

	/* suspended: off the CPUs until it is resumed */
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), SCE_LINE(9656, 9661));
	thread->currentCpuId = -1;
	thread->waitId = -1;
	thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_SUSPENDED;
	listSuspended(thread);
	status = thread->flags;
	SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
	stopSelf(thread, 2);
	if (thread->flags & THREAD_FLAG_EXITING) {
		status = thread->flags;
		goto exiting;
	}
	if (status & THREAD_FLAG_SIGNAL_DEFERRED) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_RESUMED_WAIT);
		ret = 0;
	} else {
		ret = 1;
	}

debugSuspend:
	if (thread->debugSuspendReason != 0) {
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
		_debugSuspendThreadSelf(thread->debugSuspendReason);
		stopSelf(thread, 4);
		SCE_DBG_ASSERT_PRINTF(!(thread->flags & (THREAD_FLAG_REQUEST_VM_SUSPEND |
							 THREAD_FLAG_REQUEST_SUSPEND)),
				      9840, "threadId=0x%08X, flags=0x%08X, debugSuspendReason=0x%08X\n",
				      thread->threadId, thread->flags, thread->debugSuspendReason);
		status = thread->flags;
		if (status >> 30)
			goto exiting;
	}
	SCE_DBG_ASSERT_PRINTF(thread->debugSuspendReason == 0 &&
			      !(thread->flags & THREAD_FLAG_REQUEST_MASK),
			      9857, "threadId=0x%08X, flags=0x%08X, debugSuspendReason=0x%08X\n",
			      thread->threadId, thread->flags, thread->debugSuspendReason);
	return ret;

exiting:
	exitOnRequest(thread, (status & THREAD_FLAG_REQUEST_EXIT_DELETE) != 0);
}

/*
 * SceKernelThreadMgr (threadmgr.c): changing a thread's CPU affinity, alone
 * or with its priority. A thread queued on this CPU (or the global queues)
 * is requeued where it may run; one on another CPU's queues is changed by
 * that CPU (an interrupt request). A waiting thread is moved within the
 * priority-ordered queues it waits on.
 */

#define AFFINITY_CURRENT_CPU		0x40000000	/* the mask: the calling CPU */
#define SDK_VERSION_AFFINITY_CHECK	0x01800000
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION	0x80028026
#define USER_ERROR_UNKNOWN_THREAD_ID	(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT)

extern void notifyThreadPriorityChange(struct ThreadCB *thread, int oldPriority,
				       int newPriority);

/* the SDK version the caller's process was built with */
static inline uint32_t callerSdkVersion(void)
{
	uint32_t version;

	if (currentThreadContext() == NULL ||
	    currentThreadContext()->processId == THREADMGR_KERNEL_PID)
		return SDK_VERSION_KERNEL;
	if (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId, &version) < 0)
		return 0;
	return version;
}

/* a thread's mask, the process default for none (older SDKs only, if old) */
static inline uint32_t effectiveAffinity(struct ThreadCB *thread,
					 uint32_t mask)
{
	return mask != 0 ? mask :
	       thread->process->cpuAffinity;
}

/* the mask the thread reports: its own, else (before SDK 2.10) its process's */
static inline uint32_t reportedAffinity(struct ThreadCB *thread,
					uint32_t version)
{
	uint32_t affinity = thread->currentCpuAffinityMask;

	if (affinity == 0 && version < SDK_VERSION_DEFAULT_AFFINITY)
		affinity = thread->process->cpuAffinity;
	return affinity;
}

/* the error of a failed copy, marked for a syscall */
static inline int copyError(int result)
{
	return result | (ThreadmgrInSyscall() ? SYSCALL_ERROR_BIT : 0);
}

static inline void setPriorityAndAffinity(struct ThreadCB *thread,
					  int priority, uint32_t mask)
{
	thread->basePriority = priority;
	if (priority < thread->boostPriority)
		thread->currentPriority = priority;
	thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
}

/*
 * changeThreadPriorityAndCpuAffinityMask: thread (its lock held) gets
 * priority and mask together; returns 1 if this CPU should switch.
 */
int changeThreadPriorityAndCpuAffinityMask(struct ThreadCB *thread,
	int priority, uint32_t mask, int *previousPriority, uint32_t *previousMask)
{
	struct ThreadCB *context = thread;
	unsigned int currentCpuId = sceKernelCpuId();
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *node;
	ThreadmgrEvent *event;
	struct ThreadmgrWaitableObject *waitable;
	struct ThreadmgrWaitNode *next;
	struct ThreadCB *waiter;
	ThreadmgrPriorityEventHook hook;
	uint32_t state, index, version, affinity;
	SceUID waitId;
	int oldPriority, status = 0, result, cpuId, highest, target;

	SCE_DBG_ASSERT_CHECK(context->lock != 0, 6457);
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 6460);
	oldPriority = context->currentPriority;
	if (previousPriority != NULL)
		*previousPriority = oldPriority;
	version = SDK_VERSION_KERNEL;
	if (currentThreadContext() != NULL &&
	    currentThreadContext()->processId != THREADMGR_KERNEL_PID) {
		if (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId,
					      &version) < 0)
			version = 0;
	}
	if (previousMask != NULL)
		*previousMask = reportedAffinity(context, version);
retry:
	state = context->status;
	if (!(state & THREAD_STATE_QUEUED)) {
		if (!(state & SCE_KERNEL_THREAD_STATUS_WAITING)) {
			setPriorityAndAffinity(context, priority, mask);
			goto finish;
		}
		waitId = context->waitId;
		if (waitId == WAIT_ID_MULTIPLE) {
			/* each event it waits on requeues its node by the new priority */
			wme = (ThreadmgrWaitMultipleEvents *)context->next;
			for (index = 0; index < wme->count; index++) {
				if (wme->nodes[index].next == &wme->nodes[index])
					continue;
				node = (ThreadmgrEventWaitNode *)((uintptr_t)wme->nodes[index].next -
					offsetof(ThreadmgrEventWaitNode, wmeLink));
				waitId = node->eventId;
				result = SceSysmemForDriver_72A98D17(waitId,
					s_threadmgrRuntime.manager->eventClass,
					(SceObjectBase **)&event);
				if (result < 0)
					SCE_PANIC(SCE_LINE(6651, 6657));
				if (SceCpuForDriver_5AC9D394(&event->lock) < 0)
					goto retry_wait;
				state = event->attr;
				setPriorityAndAffinity(context, priority, mask);
				if (state & WAIT_ATTR_PRIORITY) {
					node->next->prev = node->prev;
					node->prev->next = node->next;
					for (next = event->next;
					     next != (struct ThreadmgrWaitNode *)&event->next;
					     next = next->next) {
						waiter = EventWaiterIsThread(next) ?
							(struct ThreadCB *)next :
							(struct ThreadCB *)(next->owner & ~3u);
						SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC,
								     6690);
						if (context->currentPriority < waiter->currentPriority)
							break;
					}
					node->next = next;
					node->prev = next->prev;
					next->prev = (struct ThreadmgrWaitNode *)node;
					node->prev->next = (struct ThreadmgrWaitNode *)node;
				}
				sceKernelSpinlockLowUnlock(&event->lock);
				sceGUIDReleaseObject(node->eventId);
			}
			goto finish;
		}
		result = SceSysmemForDriver_72A98D17(waitId,
			s_threadmgrRuntime.manager->waitableClass,
			(SceObjectBase **)&waitable);
		if (result < 0) {
			SCE_DBG_ASSERT_PRINTF(context->delayId != -1, 6731,
				"*** error: sceGUIDReferObjectWithSubclass(0x%08X)=0x%08X, delayId=0x%08X\n",
				waitId, result, context->delayId);
			setPriorityAndAffinity(context, priority, mask);
			goto finish;
		}
		if (SceCpuForDriver_5AC9D394(&waitable->lock) < 0)
			goto retry_wait;
		state = waitable->attributes;
		setPriorityAndAffinity(context, priority, mask);
		if (state & WAIT_ATTR_PRIORITY) {
			unlinkThread(context);
			insertPriorityQueue((uintptr_t)&waitable->next, thread);
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(context));
		status = revalidateWaitState((uintptr_t)&waitable->next);
		sceKernelSpinlockLowUnlock(&waitable->lock);
		sceGUIDReleaseObject(waitId);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(context));
		goto finish;
	}

	/* queued: on another CPU, that CPU does it */
	cpuId = context->currentCpuId;
	if (cpuId != (int)currentCpuId && cpuId != CPU_ID_GLOBAL)
		goto remote;
	if (!((1u << (currentCpuId & 0xff)) & AFFINITY_MASK & effectiveAffinity(context, mask))) {
		/* this CPU may no longer run it */
		if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			if (currentThreadContext() != NULL)
				goto remote;
			setPriorityAndAffinity(context, priority, mask);
			if (cpu->disabledDispatch)
				goto finish;
			runningThreadPreemptByIntr(thread, PREEMPT_INTR);
		} else {
			if (state & SCE_KERNEL_THREAD_STATUS_READY) {
				removeReady(thread, context->currentPriority);
			} else {
				SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
				unlinkThread(context);
				sceKernelSpinlockLowUnlock(&cpu->standbyLock);
			}
			setPriorityAndAffinity(context, priority, mask);
		}
		goto distribute;
	}
	context->currentCpuAffinityMask = mask & AFFINITY_MASK;
	context->basePriority = priority;
	if (priority >= context->boostPriority)
		goto finish;
	if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
		state = cpu->disabledDispatch;
		context->currentPriority = priority;
		cpu->priority = priority;
		if (!state && (int)searchHighestPriority() < priority) {
			PREEMPT_SELF(cpu, 6507);
			status = 1;
		}
	} else if (state & SCE_KERNEL_THREAD_STATUS_READY) {
		removeReady(thread, context->currentPriority);
		context->currentPriority = priority;
		addReady(thread);
		if ((uint32_t)context->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			struct ThreadMgrCpuCB *targetCpu = s_threadmgrRuntime.manager->cpuCB;

			/* the CPU running the least urgent thread reschedules */
			highest = 1;
			target = -1;
			for (index = 0; index < CPU_COUNT; index++, targetCpu++) {
				int runningPriority = targetCpu->priority;

				if (highest < runningPriority) {
					highest = runningPriority;
					target = index;
				}
			}
			if (priority < highest) {
				if (target == (int)currentCpuId) {
					PREEMPT_SELF(cpu, 6527);
					status = 1;
				} else {
					addIntrRequest(target, INTR_MASK_SCHEDULING);
				}
			}
		} else if (priority < cpu->priority) {
			PREEMPT_SELF(cpu, 6536);
			status = 1;
		}
	} else {
		context->currentPriority = priority;
		if ((uint32_t)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
			unlinkThread(context);
			sceKernelSpinlockLowUnlock(&cpu->standbyLock);
			goto distribute;
		} else if (priority < cpu->priority) {
			PREEMPT_SELF(cpu, 6555);
			status = 1;
		}
	}
finish:
	if (oldPriority > 0) {
		if ((int32_t)context->currentAttr < 0) {
			/* a user thread's TLS reports both */
			affinity = reportedAffinity(context, version) << 16;
			result = sceKernelCopyToUserProc(context->processId,
				(void *)(context->tls - USER_TLS_PRIORITY),
				(void *)&context->currentPriority, sizeof(int));
			if (result < 0)
				status = copyError(result);
			result = sceKernelCopyToUserProc(context->processId,
				(void *)(context->tls - USER_TLS_AFFINITY), &affinity,
				sizeof(affinity));
			if (result < 0)
				status = copyError(result);
		}
		if (status >= 0) {
			hook = (ThreadmgrPriorityEventHook)s_threadmgrRuntime.manager->paHooks[1];
			if (hook != NULL) {
				uint32_t values[] = { context->processId, context->threadId,
						      oldPriority, context->currentPriority };

				hook(DEBUG_EVENT_PRIORITY_CHANGE, values);
			}
			notifyThreadPriorityChange(thread, oldPriority, priority);
		}
	}
	return status;
retry_wait:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(context));
	sceGUIDReleaseObject(waitId);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(context));
	goto retry;
remote:
	issueChangeThreadPriorityAndCpuAffinityIntrRequest(cpuId, context->threadId, priority, mask);
	return 0;
distribute:
	status = distributeThread(selectExecCpu(thread), thread);
	goto finish;
}

/* SceThreadmgrSGI: another CPU asks this one to change threadId */
void changeThreadPriorityAndCpuAffinityMaskSGIHandler(SceUID threadId, int priority,
						    uint32_t mask)
{
	struct ThreadmgrThreadObject *object;

	if (currentThreadContext() != NULL)
		SCE_PANIC(SCE_LINE(6823, 6829));
	if (SceSysmemForDriver_00ED6C14(threadId,
	    s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) >= 0) {
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(&object->context));
		changeThreadPriorityAndCpuAffinityMask(&object->context, priority, mask, NULL, NULL);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
}

/* changeThreadCpuAffinityMask: threadId's mask; previousMask gets the old one */
int changeThreadCpuAffinityMask(SceUID threadId, uint32_t mask, uint32_t *previousMask)
{
	unsigned int cpuId = sceKernelCpuId();
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread;
	struct ThreadmgrThreadObject *object;
	ProcessBudget *type;
	uint32_t version, state, affinity;
	int status, result, count;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 5530);
	if (SceSysmemForDriver_00ED6C14(threadId,
	    s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	thread = &object->context;
	if (mask == AFFINITY_CURRENT_CPU) {
		mask = (1u << (cpuId & 0xff)) & AFFINITY_MASK;
	} else {
		status = checkCpuAffinityMaskPhy(mask, thread->currentAttr, thread->process);
		if (status < 0)
			goto release;
	}
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	version = callerSdkVersion();
	if (version < SDK_VERSION_AFFINITY_CHECK) {
		/* older SDKs: the mask must suit the priority */
		if (!s_threadmgrRuntime.manager->testMode &&
		    (uint32_t)thread->currentPriority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE &&
		    mask != 0 &&
		    SceSysmemForDriver_0FC24464(thread->process->budgetId,
				      (SceObjectBase **)&type) >= 0 &&
		    (sceKernelGetDefaultCpuAffinityMask(type) & mask) !=
			 sceKernelGetDefaultCpuAffinityMask(type)) {
			SceDebugForDriver_02B04343(0, SCE_DBGINFO(SCE_LINE(5562, 5568)),
				"*** error: illegal CPU affinity mask change. threadId=0x%08X[%s]"
				"(priority=%d), 0x%08X -> 0x%08X\n",
				threadId, SceSysmemForDriver_E655852F(thread->threadId), thread->currentPriority,
				thread->currentCpuAffinityMask, mask);
			status = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
			goto unlock;
		}
		if (version < SDK_VERSION_AFFINITY_CHECK && !s_threadmgrRuntime.manager->testMode &&
		    (uint32_t)thread->currentPriority - PRIORITY_SHARED_LOW >= PRIORITY_SHARED_RANGE &&
		    mask != 0) {
			count = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
			if (count > 1) {
				SceDebugForDriver_02B04343(0, SCE_DBGINFO(SCE_LINE(5582, 5588)),
					"*** error: illegal CPU affinity mask change. threadId=0x%08X[%s]"
					"(priority=%d), 0x%08X -> 0x%08X\n",
					threadId, SceSysmemForDriver_E655852F(thread->threadId),
					thread->currentPriority, thread->currentCpuAffinityMask, mask);
				status = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
				goto unlock;
			}
		}
	}
	if (previousMask != NULL)
		*previousMask = reportedAffinity(thread, version);
	state = thread->status;
	status = 0;
	if (!(state & THREAD_STATE_QUEUED)) {
		thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
		if ((state & SCE_KERNEL_THREAD_STATUS_STAGNANT) &&
		    thread->process->threadsStopped == 0 &&
		    (effectiveAffinity(thread, mask) & s_threadmgrRuntime.manager->activeCpuMask)) {
			/* a CPU that may run it is active again */
			SceCpuForDriver_BF82DEB2(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			unlinkThread(thread);
			sceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			goto distribute;
		}
	} else {
		if (thread->currentCpuId != (int)cpuId && thread->currentCpuId != CPU_ID_GLOBAL) {
			issueChangeThreadCpuAffinityIntrRequest(thread->currentCpuId, threadId, mask);
			goto unlock;
		}
		if ((1u << (cpuId & 0xff)) & AFFINITY_MASK & effectiveAffinity(thread, mask)) {
			thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
		} else if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			if (currentThreadContext() != NULL) {
				issueChangeThreadCpuAffinityIntrRequest(cpuId, threadId, mask);
				goto unlock;
			}
			thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
			if (!cpu->disabledDispatch) {
				runningThreadPreemptByIntr((struct ThreadCB *)thread,
							   PREEMPT_INTR);
				goto distribute;
			}
		} else {
			if (state & SCE_KERNEL_THREAD_STATUS_READY) {
				removeReady((struct ThreadCB *)thread,
					    thread->currentPriority);
			} else {
				SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
				unlinkThread(thread);
				sceKernelSpinlockLowUnlock(&cpu->standbyLock);
			}
			thread->currentCpuAffinityMask = mask & AFFINITY_MASK;
			goto distribute;
		}
	}
copy:
	if ((int)thread->currentAttr < 0) {
		/* a user thread's TLS reports its mask */
		affinity = reportedAffinity(thread, version) << 16;
		result = sceKernelCopyToUserProc(thread->processId,
			(void *)(thread->tls - USER_TLS_AFFINITY), &affinity, sizeof(affinity));
		if (result < 0 && copyError(result) < 0)
			status = 0;
	}
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
release:
	sceGUIDReleaseObject(threadId);
	return status;
distribute:
	status = distributeThread(selectExecCpu((struct ThreadCB *)thread),
				  (struct ThreadCB *)thread) == 1;
	goto copy;
}

/* SceKernelThreadMgr: kernel thread-start entry points. */
int sceKernelStartThread(SceUID threadId, SceSize argSize, const void *arg)
{
	return startThread(threadId, argSize, arg, 0);
}

/* A start whose argument may not be kernel memory. */
int SceThreadmgrForKernel_CBADD6B3(SceUID threadId, SceSize argSize, const void *arg)
{
	return startThread(threadId, argSize, arg, 1);
}

/*
 * SceKernelThreadMgr (threadmgr.c): running a function on a bigger kernel
 * stack. What fits in the thread's free stack runs there; otherwise a new
 * stack ("SceExt", or one from the per-CPU cache) stands in for the
 * thread's stack for the call.
 */

#define THREAD_ATTR_UNK10000000		0x10000000	/* PLACEHOLDER: its stack is only measured with DIPSW 0xd8 */
#define DIPSW_STACK_CHECK		0xd8
#define EXTEND_STACK_MIN			0x200
#define STACK_PAGE			0x1000
#define STACK_SAFETY			0x10		/* kept free below the caller */
#define STACK_LOW_WARNING		0x400
#define STACK_LOW_PANIC			0x300
#define STACK_CACHED_SIZE		0x2000		/* the size the per-CPU cache holds */
#define MEMBLOCK_TYPE_KERNEL_STACK	0x1020d006	/* PLACEHOLDER: name */
#define MEMBLOCK_ATTR_EXT_STACK		((4 << ALLOC_ATTR_BLOCK_SHIFT) | ALLOC_ATTR_NO_CLEAR | \
					 ALLOC_ATTR_HAS_EXTRA_LOW)
#define TRACE_EVENT_EXTEND_STACK	0x10
#define TRACE_EVENT_EXTEND_STACK_END	0x11
#define SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE	0x80028024

/* kernel_stack.S */
extern int64_t callWithStack(void *arg, uintptr_t stackTop, int *intrState,
			     ThreadmgrStackFunction function);

/* The early query is emitted separately in kernel_stack_space.c. */

/*
 * sceKernelExtendKernelStackWide: function(arg) runs with at least size
 * bytes of kernel stack; returns what it returns.
 */
int64_t sceKernelExtendKernelStackWide(SceSize size, ThreadmgrStackFunction function, void *arg)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadmgrExtendedStack frame, *saved;
	SceKernelAllocMemBlockKernelOpt opt;
	ThreadmgrTraceHook trace;
	SceSize free, pages, oldSize;
	SceUID memBlockId;
	void *base;
	int64_t ret;
	int intr, err, cached;

	free = SceThreadmgrForDriver_D74F66BD();
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_EXTEND_STACK, 5, thread->processId, thread->threadId, function,
		      size, free);
	if (free >= STACK_SAFETY)
		free -= STACK_SAFETY;
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF(0, 5368, "*** error: illegal context\n");
		ret = (int)SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto out;
	}
	if (size < EXTEND_STACK_MIN) {
		ret = (int)SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
		goto out;
	}
	pages = (size + STACK_PAGE - 1) & ~(STACK_PAGE - 1);
	if (pages <= free) {
		/* it fits where we are */
		intr = SceCpuForDriver_821FC0EE();
		oldSize = thread->kStackSize;
		thread->kStackSize = pages;
		ret = callWithStack(arg, thread->pKStack + pages, &intr, function);
		thread->kStackSize = oldSize;
		sceKernelCpuResumeIntr(intr);
		goto out;
	}
	SCE_DBG_ASSERT_PRINTF(free >= STACK_LOW_WARNING, 5394,
		"*** sceKernelExtendKernelStackWide(), Remaining stack size = 0x%08X\n", free);
	cached = 0;
	if (size == STACK_CACHED_SIZE &&
	    ((int)thread->currentAttr < 0 ||
	     thread->stackMemBlockType == MEMBLOCK_TYPE_KERNEL_STACK)) {
		memBlockId = popExtendedStackCache(&base);
		cached = 1;
		if (memBlockId > 0)
			goto have;
	}
	if (free < STACK_LOW_PANIC)
		SCE_PANIC(SCE_LINE(5407, 5413));
	memset(&opt, 0, sizeof(opt));
	opt.extraLow = STACK_PAGE;	/* a guard page below */
	opt.attr = MEMBLOCK_ATTR_EXT_STACK;
	opt.size = sizeof(opt);
	memBlockId = SceSysmemForDriver_C94850C9("SceExt",
		(int)thread->currentAttr < 0 ? MEMBLOCK_TYPE_KERNEL_STACK : thread->stackMemBlockType,
		pages + STACK_PAGE, &opt);
	if (memBlockId < 0) {
		ret = memBlockId;
		goto out;
	}
	SceSysmemForDriver_0B1FD5C3(memBlockId, &base);
have:
	if (!(thread->currentAttr & THREAD_ATTR_UNK10000000) || SceDipswForDriver_A98FC2FD(DIPSW_STACK_CHECK))
		memset(base, 0xff, pages);
	*(SceUID *)base = thread->threadId;
	/* the new stack stands in for the thread's while the function runs */
	intr = SceCpuForDriver_821FC0EE();
	frame.stackBase = thread->pKStack;
	frame.memBlockId = thread->kStackMemblkId;
	frame.next = thread->extendedStacks;
	thread->pKStack = (uintptr_t)base;
	frame.stackSize = thread->kStackSize;
	thread->extendedStacks = &frame;
	thread->kStackSize = pages;
	thread->kStackMemblkId = memBlockId;
	ret = callWithStack(arg, (uintptr_t)base + pages, &intr, function);
	saved = thread->extendedStacks;
	thread->extendedStacks = saved->next;
	thread->pKStack = saved->stackBase;
	thread->kStackSize = saved->stackSize;
	thread->kStackMemblkId = saved->memBlockId;
	sceKernelCpuResumeIntr(intr);
	if (!cached || pushExtendedStackCache(memBlockId, base) < 0) {
		err = SceSysmemForDriver_009E1C61(memBlockId);
		if (err < 0) {
			SCE_DBG_ASSERT_FAIL(5478);
			ret = err;
		}
	}
out:
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_EXTEND_STACK_END, 4, thread->processId, thread->threadId,
		      (uint32_t)(ret >> 32), (uint32_t)ret);
	return ret;
}

/* SceThreadmgrForDriver_E54FD746 */
int64_t SceThreadmgrForDriver_E54FD746(SceSize size, ThreadmgrStackFunction function, void *arg)
{
	return sceKernelExtendKernelStackWide(size, function, arg);
}

/*
 * SceKernelThreadMgr: reading a thread's CPU affinity mask. A thread
 * without one reports its process's default to callers built with an SDK
 * before 2.10; the syscall gives it in bits 16-19.
 */

/* sceKernelGetThreadCpuAffinityMask: uid's mask (0: the caller's) */
int sceKernelGetThreadCpuAffinityMask(SceUID uid)
{
	struct ThreadmgrThreadObject *object;
	SceKernelIntrStatus intr;
	uint32_t saved, version;
	int result, inIntr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	inIntr = SceIntrmgrForDriver_182EE3E3();
	if (uid == 0) {
		if (inIntr) {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 5822, "sceKernelGetThreadCpuAffinityMask",
						   "*** error: illegal context\n");
			result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto out;
		}
		uid = currentThreadContext() != NULL ? currentThreadContext()->threadId :
						       (SceUID)SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = SceCpuForDriver_821FC0EE();
	result = SceSysmemForDriver_00ED6C14(uid,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (result < 0) {
		result = ThreadmgrInSyscall() ? (int)USER_ERROR_UNKNOWN_THREAD_ID :
						(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(&object->context));
		result = object->context.currentCpuAffinityMask;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		if (result == 0 && currentThreadContext() != NULL &&
		    currentThreadContext()->processId != THREADMGR_KERNEL_PID &&
		    (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId, &version) < 0 ||
		     version < SDK_VERSION_DEFAULT_AFFINITY))
			result = (object->context.process)->
				cpuAffinity;
		sceGUIDReleaseObject(uid);
	}
	sceKernelCpuResumeIntr(intr);
out:
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/*
 * SceKernelThreadMgr: the priority change exports. A priority 0x100000e0-
 * 0x1000011f is relative to the process type's default; older SDKs may not
 * move a thread between the shared and the per-CPU priorities.
 */

#define PRIORITY_ABSOLUTE_RANGE		0xfe	/* 1-0xfe */
#define PRIORITY_RELATIVE		0x10000000
#define PRIORITY_RELATIVE_BASE		0x100000e0
#define PRIORITY_RELATIVE_RANGE		0x40
#define PRIORITY_RELATIVE_OFFSET	0x100000a0
#define PRIORITY_RELATIVE_OFFSET_SYSTEM	0x10000060
#define PRIORITY_RELATIVE_OFFSET_OLD	0x10000100
#define PRIORITY_USER_LOW		0x40
#define PRIORITY_USER_RANGE_SYSTEM	0x80
#define PRIORITY_USER_RANGE		0xbc

#define CHANGE_PRIORITY_FORMAT \
	"***\n***\n***\n*** error: sceKernelChangeThreadPriority(\"%s\"), illegal priority for user " \
	"mode thread(%d)\n*** This will be forbidden by 2011/04/13 !\n***\n***\n***\n"
#define CHANGE_PRIORITY_FUNC			"sceKernelChangeThreadPriority2"

static inline int sharedPriority(uint32_t priority)
{
	return priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE;
}

/* sceKernelChangeThreadPriority2: uid's priority (0: its own); returns the old one */
int sceKernelChangeThreadPriority2(SceUID uid, uint32_t priority)
{
	struct ThreadCB *current = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	SceKernelIntrStatus intr;
	uint32_t saved, type, version, previous;
	bool hasCurrent;
	int result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	hasCurrent = currentThreadContext() != NULL;
	if (uid == 0 || priority == 0) {
		if (!hasCurrent) {
			if (uid != 0)
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 6210, CHANGE_PRIORITY_FUNC,
							   "*** error: illegal context\n");
			else
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 6201, CHANGE_PRIORITY_FUNC,
							   "*** error: illegal context\n");
			result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto out;
		}
		if (uid == 0)
			uid = current->threadId;
		if (priority == 0)
			priority = current->currentPriority;
	}
	if ((priority & PRIORITY_RELATIVE) &&
	    priority - PRIORITY_RELATIVE_BASE >= PRIORITY_RELATIVE_RANGE) {
		SCE_DBG_PRINTF_FUNC(0, 6220, CHANGE_PRIORITY_FUNC, "***\n***\n***\n*** error: "
			"sceKernelChangeThreadPriority(\"%s\"), illegal priority offset(%d)\n"
			"*** This will be forbidden by 2011/04/13 !\n***\n***\n***\n",
			SceSysmemForDriver_E655852F(uid), priority - PRIORITY_RELATIVE_OFFSET_OLD);
		result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
		goto out;
	}
	intr = SceCpuForDriver_821FC0EE();
	result = SceSysmemForDriver_00ED6C14(uid,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (result < 0) {
		result = ThreadmgrInSyscall() ? (int)USER_ERROR_UNKNOWN_THREAD_ID :
						(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if (priority - 1 >= PRIORITY_ABSOLUTE_RANGE) {
		/* relative to the process type's default */
		if (!(priority & PRIORITY_RELATIVE)) {
			result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto unlock;
		}
		type = thread->process->budget->type;
		if (type == PROCESS_TYPE_GAME) {
			if ((thread->currentAttr & THREAD_ATTR_UNLIMITED) &&
			    (thread->currentCpuAffinityMask & AFFINITY_SYSTEM_CPU))
				priority -= PRIORITY_RELATIVE_OFFSET;
			else
				priority -= PRIORITY_RELATIVE_OFFSET_SYSTEM;
		} else {
			priority -= PRIORITY_RELATIVE_OFFSET;
			if (type != PROCESS_TYPE_MINI_APP && type != PROCESS_TYPE_SHELL &&
			    type != PROCESS_TYPE_KERNEL)
				SCE_DBG_PRINTF_FUNC(0, 6263, CHANGE_PRIORITY_FUNC, "*** warning: unknown process "
					"type(0x%08X), [threadId=0x%08X] priority %d\n", type, uid, priority);
		}
	}
	if ((int)thread->currentAttr < 0) {
		/* a user thread's range */
		type = thread->process->budget->type;
		if (priority - PRIORITY_USER_LOW >
		    (type == PROCESS_TYPE_GAME ? PRIORITY_USER_RANGE_SYSTEM - 1 :
						   PRIORITY_USER_RANGE - 1)) {
			if (type == PROCESS_TYPE_GAME)
				SCE_DBG_PRINTF_FUNC(0, 6276, CHANGE_PRIORITY_FUNC, CHANGE_PRIORITY_FORMAT,
						    SceSysmemForDriver_E655852F(uid), priority);
			else
				SCE_DBG_PRINTF_FUNC(0, 6283, CHANGE_PRIORITY_FUNC, CHANGE_PRIORITY_FORMAT,
						    SceSysmemForDriver_E655852F(uid), priority);
			result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto unlock;
		}
	}
	if (thread->processId == THREADMGR_KERNEL_PID)
		version = THREADMGR_KERNEL_SYSVER;
	else if (SceSysrootForDriver_CAE820B2(thread->processId, &version) < 0)
		version = 0;
	if (version < SDK_VERSION_AFFINITY_CHECK && !s_threadmgrRuntime.manager->testMode) {
		/* older SDKs: not between the shared and the per-CPU priorities */
		previous = thread->currentPriority;
		if (sharedPriority(previous) != sharedPriority(priority)) {
			if (sharedPriority(previous))
				SCE_PRINTF_WITH_INFO_FUNC(0, SCE_LINE(6297, 6303), CHANGE_PRIORITY_FUNC,
					"*** error: illegal priority change. threadId=0x%08X[%s], "
					"%d -> %d\n", uid, SceSysmemForDriver_E655852F(thread->threadId),
					thread->currentPriority, priority);
			else
				SCE_PRINTF_WITH_INFO_FUNC(0, SCE_LINE(6307, 6313), CHANGE_PRIORITY_FUNC,
					"*** error: illegal priority change. threadId=0x%08X[%s], "
					"%d -> %d\n", uid, SceSysmemForDriver_E655852F(thread->threadId),
					thread->currentPriority, priority);
			result = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
			goto unlock;
		}
	}
	previous = thread->currentPriority;
	thread->basePriority = priority;
	if ((int)priority <= thread->boostPriority) {
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		result = changeThreadPriority(thread, priority);
		sceGUIDReleaseObject(uid);
		if (result == 1 && hasCurrent)
			switchThread((intr >> 7) & 1);
		if (result >= 0)
			result = previous;
		goto resume;
	}
	result = previous;
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(uid);
resume:
	sceKernelCpuResumeIntr(intr);
out:
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* sceKernelChangeThreadPriority: 0 or an error */
int sceKernelChangeThreadPriority(SceUID uid, uint32_t priority)
{
	int result = sceKernelChangeThreadPriority2(uid, priority);

	SCE_DBG_ASSERT_CHECK_FUNC(result != 0, 6367, "sceKernelChangeThreadPriority");
	return result < 0 ? result : 0;
}

/*
 * SceKernelThreadMgr: the CPU affinity exports. A syscall gives the mask
 * in bits 16-19; a kernel caller may give either half;
 * AFFINITY_CURRENT_CPU means the calling CPU.
 */

#define AFFINITY_USER_MASK		0x000f0000
#define CHANGE_AFFINITY_FUNC			"sceKernelChangeThreadPriorityAndCpuAffinityMask"

/* the mask as a CPU mask (bits 0-3); a syscall's is in bits 16-19 */
static int normalizeAffinityMask(uint32_t *mask)
{
	if (ThreadmgrInSyscall()) {
		if (*mask == AFFINITY_CURRENT_CPU || (*mask & ~AFFINITY_USER_MASK))
			return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		*mask = (int32_t)*mask >> 16;
	} else if (*mask != AFFINITY_CURRENT_CPU) {
		if ((*mask & ~(AFFINITY_USER_MASK | AFFINITY_MASK)) ||
		    ((*mask & AFFINITY_USER_MASK) && (*mask & AFFINITY_MASK)))
			return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		if (*mask & AFFINITY_USER_MASK)
			*mask = (int32_t)*mask >> 16;
	}
	return 0;
}

/* sceKernelChangeThreadPriorityAndCpuAffinityMask: both at once (0: the thread's own) */
int sceKernelChangeThreadPriorityAndCpuAffinityMask(SceUID uid, uint32_t priority,
	uint32_t mask, int *previousPriority, uint32_t *previousMask)
{
	struct ThreadCB *current = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	SceKernelIntrStatus intr;
	uint32_t saved, type;
	bool hasCurrent;
	int result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	hasCurrent = currentThreadContext() != NULL;
	if (uid == 0 || priority == 0) {
		if (!hasCurrent) {
			if (uid != 0)
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 6920, CHANGE_AFFINITY_FUNC,
							   "*** error: illegal context\n");
			else
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 6911, CHANGE_AFFINITY_FUNC,
							   "*** error: illegal context\n");
			result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto out;
		}
		if (uid == 0)
			uid = current->threadId;
		if (priority == 0)
			priority = current->currentPriority;
	}
	if ((priority & PRIORITY_RELATIVE) &&
	    priority - PRIORITY_RELATIVE_BASE >= PRIORITY_RELATIVE_RANGE) {
		SCE_DBG_PRINTF_FUNC(0, 6930, CHANGE_AFFINITY_FUNC, "***\n***\n***\n*** error: "
			"sceKernelChangeThreadPriorityAndCpuAffinityMask(\"%s\"), illegal priority "
			"offset(%d)\n***\n***\n***\n",
			SceSysmemForDriver_E655852F(uid), priority - PRIORITY_RELATIVE_OFFSET_OLD);
		result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
		goto out;
	}
	result = normalizeAffinityMask(&mask);
	if (result < 0)
		goto out;
	intr = SceCpuForDriver_821FC0EE();
	result = SceSysmemForDriver_00ED6C14(uid,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (result < 0) {
		result = ThreadmgrInSyscall() ? (int)USER_ERROR_UNKNOWN_THREAD_ID :
						(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	if (mask == AFFINITY_CURRENT_CPU) {
		mask = (1u << (sceKernelCpuId() & 0xff)) & AFFINITY_MASK;
	} else {
		result = checkCpuAffinityMaskPhy(mask, thread->currentAttr, thread->process);
		if (result < 0) {
			sceGUIDReleaseObject(uid);
			goto resume;
		}
	}
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if (priority - 1 >= PRIORITY_ABSOLUTE_RANGE) {
		/* relative to the process type's default */
		if (!(priority & PRIORITY_RELATIVE)) {
			result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto unlock;
		}
		type = thread->process->budget->type;
		if (type == PROCESS_TYPE_GAME) {
			if ((thread->currentAttr & THREAD_ATTR_UNLIMITED) &&
			    (thread->currentCpuAffinityMask & AFFINITY_SYSTEM_CPU))
				priority -= PRIORITY_RELATIVE_OFFSET;
			else
				priority -= PRIORITY_RELATIVE_OFFSET_SYSTEM;
		} else {
			priority -= PRIORITY_RELATIVE_OFFSET;
			if (type != PROCESS_TYPE_MINI_APP && type != PROCESS_TYPE_SHELL &&
			    type != PROCESS_TYPE_KERNEL)
				SCE_DBG_PRINTF_FUNC(0, 7017, CHANGE_AFFINITY_FUNC, "*** warning: unknown process "
					"type(0x%08X), [threadId=0x%08X] priority %d\n", type, uid, priority);
		}
	}
	if ((int)thread->currentAttr < 0) {
		/* a user thread's range */
		type = thread->process->budget->type;
		if (priority - PRIORITY_USER_LOW >
		    (type == PROCESS_TYPE_GAME ? PRIORITY_USER_RANGE_SYSTEM - 1 :
						   PRIORITY_USER_RANGE - 1)) {
			if (type == PROCESS_TYPE_GAME)
				SCE_DBG_PRINTF_FUNC(0, 7029, CHANGE_AFFINITY_FUNC, "***\n***\n***\n*** error: "
					"sceKernelChangeThreadPriorityAndCpuAffinityMask(\"%s\"), illegal "
					"priority for user mode thread(%d)\n***\n***\n***\n",
					SceSysmemForDriver_E655852F(uid), priority);
			else
				SCE_DBG_PRINTF_FUNC(0, 7035, CHANGE_AFFINITY_FUNC, "***\n***\n***\n*** error: "
					"sceKernelChangeThreadPriorityAndCpuAffinityMask(\"%s\"), illegal "
					"priority for user mode thread(%d)\n***\n***\n***\n",
					SceSysmemForDriver_E655852F(uid), priority);
			result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto unlock;
		}
	}
	result = changeThreadPriorityAndCpuAffinityMask(thread, priority, mask, previousPriority,
							previousMask);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(uid);
	if (result == 1 && hasCurrent) {
		switchThread((intr >> 7) & 1);
		result = 1;
	}
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(uid);
resume:
	sceKernelCpuResumeIntr(intr);
out:
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* sceKernelChangeThreadCpuAffinityMask: uid's mask (0: its own); returns the old one */
int sceKernelChangeThreadCpuAffinityMask(SceUID uid, uint32_t mask)
{
	SceKernelIntrStatus intr;
	uint32_t saved, previousMask;
	bool dispatch;
	int result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (uid == 0) {
		if (currentThreadContext() == NULL) {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 5737, "sceKernelChangeThreadCpuAffinityMask",
						   "*** error: illegal context\n");
			result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto out;
		}
		uid = currentThreadContext() != NULL ? currentThreadContext()->threadId :
						       (SceUID)SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	result = normalizeAffinityMask(&mask);
	if (result < 0)
		goto out;
	intr = SceCpuForDriver_821FC0EE();
	result = changeThreadCpuAffinityMask(uid, mask, &previousMask);
	if (result >= 0) {
		dispatch = result == 1;
		result = previousMask;
		if (dispatch && currentThreadContext() != NULL)
			switchThread((intr >> 7) & 1);
	}
	sceKernelCpuResumeIntr(intr);
out:
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/*
 * SceKernelThreadMgr (threadmgr.c): the set of active CPUs. A CPU leaving
 * the set hands every thread it holds (standby, ready, running) to the
 * other CPUs; one joining it takes the stagnant threads it may run.
 */

#define CPU_MASK_ALT			0x000f0000	/* the same mask, shifted up 16 */
#define PERMISSION_SYSTEM		0x40	/* PLACEHOLDER: names of the permissions allowed */
#define PERMISSION_ROOT			0x80
#define DEACTIVATE_SWITCH		3	/* deactivateCpu: the running thread moved away */
#define SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK	0x80028025

/*
 * This CPU leaves the active set: its threads go to the others. Returns
 * DEACTIVATE_SWITCH when the running thread moved and the CPU must switch.
 */
int deactivateCpu(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *thread, *next;
	volatile int spin;
	uintptr_t head;
	int priority;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 7084);
	/* the standby list; a busy thread is retried after a short spin */
	for (;;) {
		SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
		for (;;) {
			thread = (void *)cpu->standbyNext;
			if ((uintptr_t)thread == (uintptr_t)&cpu->standbyNext)
				goto ready;
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 7099);
			if (SceCpuForDriver_5AC9D394(THREAD_LOCK(thread)) < 0)
				break;
			((struct ThreadCB *)thread->next)->prev = thread->prev;
			((struct ThreadCB *)thread->prev)->next = thread->next;
			sceKernelSpinlockLowUnlock(&cpu->standbyLock);
			distributeThread(selectExecCpu(thread), thread);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
		}
		sceKernelSpinlockLowUnlock(&cpu->standbyLock);
		for (spin = 10; spin != 0; spin--)
			;
	}
ready:
	sceKernelSpinlockLowUnlock(&cpu->standbyLock);
	/* the ready queues this CPU keeps itself: the high priorities and the idle ones */
	for (priority = 0; priority < 0x80; priority++) {
		head = (uintptr_t)&cpu->readyQueue[priority];
		for (thread = (void *)cpu->readyQueue[priority].next; (uintptr_t)thread != head;
		     thread = next) {
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 7126);
			next = (void *)thread->next;
			if (thread == cpu->idleThread)
				continue;
			SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
			removeReady(thread, priority);
			distributeThread(selectExecCpu(thread), thread);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		}
	}
	for (priority = 0xfc; priority < 0x100; priority++) {
		head = (uintptr_t)&cpu->readyQueue[priority];
		for (thread = (void *)cpu->readyQueue[priority].next; (uintptr_t)thread != head;
		     thread = next) {
			SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 7144);
			next = (void *)thread->next;
			if (thread == cpu->idleThread)
				continue;
			SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
			removeReady(thread, priority);
			distributeThread(selectExecCpu(thread), thread);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		}
	}
	/* and the running thread */
	thread = (void *)cpu->runningThread;
	if (thread == cpu->idleThread)
		return 0;
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	distributeThread(selectExecCpu(thread), thread);
	if (currentThreadContext() == NULL)
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	else
		thread->waitId = -1;
	SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 7179);
	cpu->nextRunningThread = 0;
	return DEACTIVATE_SWITCH;
}

/*
 * Sets the active CPUs (bits 0-3, or the same in bits 16-19). Only system
 * processes may call it.
 */
int sceKernelChangeActiveCpuMask(uint32_t mask)
{
	struct ThreadCB *thread = (void *)currentThreadContext();
	unsigned int cpuId, i;
	uint32_t saved, old, bit;
	int denied, intr, ret, moved;

	denied = 1;
	if (thread != NULL &&
	    (thread->permission == PERMISSION_SYSTEM || thread->permission == PERMISSION_ROOT))
		denied = 0;
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	SCE_DBG_ASSERT_CHECK(!ThreadmgrInSyscall(), 7212);
	if (denied) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto restore;
	}
	if (mask & ~(CPU_MASK_ALL | CPU_MASK_ALT)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		goto restore;
	}
	if (mask & CPU_MASK_ALT) {
		if (mask & CPU_MASK_ALL) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
			goto restore;
		}
		mask >>= 16;
	}
	moved = 0;
	intr = SceCpuForDriver_D32ACE9E(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	cpuId = sceKernelCpuId();
	old = s_threadmgrRuntime.manager->activeCpuMask;
	s_threadmgrRuntime.manager->activeCpuMask = mask & CPU_MASK_ALL;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	for (i = 0; i < CPU_COUNT; i++) {
		bit = 1 << i;
		if (!(bit & (mask ^ old)))
			continue;
		if (bit & mask) {
			if (i == cpuId) {
				SCE_DBG_PRINTF(0xb, 7256,
					" fatal error: should not be executed. CPU ID = %d\n", i);
				SCE_PANIC(SCE_LINE(7250, 7257));
			}
			addIntrRequest(i, INTR_MASK_SALVAGE);
		} else if (i == cpuId) {
			moved = deactivateCpu();
		} else {
			addIntrRequest(i, INTR_MASK_DEACTIVATE);
		}
	}
	if (moved && currentThreadContext() != NULL)
		switchThread(2);
	sceKernelCpuResumeIntr(intr);
	ret = 0;
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForKernel_16D1961A: every other CPU reschedules, then this one */
int SceThreadmgrForKernel_16D1961A(void)
{
	unsigned int cpuId, i;
	int intr;

	intr = SceCpuForDriver_821FC0EE();
	cpuId = sceKernelCpuId();
	for (i = 0; i < CPU_COUNT; i++) {
		if (i != cpuId)
			addIntrRequest(i, INTR_MASK_SCHEDULING);
	}
	currentCpuState()->nextRunningThread = 0;
	if (currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/*
 * SceKernelThreadMgr: small thread state operations: the running thread's
 * attributes, the active CPU mask, a thread's exit status.
 */

#define THREAD_ATTR_USER_SETTABLE	0x81000000	/* what a syscall may change */
#define THREAD_ATTR_KERNEL_SETTABLE	0xf0000000	/* what a kernel caller may change */
#define THREAD_PERMISSION_SYSTEM	0x40	/* PLACEHOLDER: names */

/* sceKernelChangeCurrentThreadAttr: the running thread's attributes, clear then set */
int sceKernelChangeCurrentThreadAttr(uint32_t clear, uint32_t set)
{
	struct ThreadCB *thread = currentThreadContext();
	SceKernelIntrStatus intr;
	uint32_t saved;
	int result = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ThreadmgrInSyscall()) {
		if ((clear | set) & ~THREAD_ATTR_USER_SETTABLE)
			result = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	} else if ((clear | set) & ~THREAD_ATTR_KERNEL_SETTABLE) {
		result = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	} else if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 7392, "sceKernelChangeCurrentThreadAttr",
					   "*** error: illegal context\n");
		result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else {
		intr = SceCpuForDriver_821FC0EE();
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
		thread->currentAttr = (thread->currentAttr & ~clear) | set;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		sceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* SceThreadmgrForDriver_86DAE59B: the active CPU mask, for system threads */
uint32_t SceThreadmgrForDriver_86DAE59B(void)
{
	struct ThreadCB *thread = currentThreadContext();
	SceKernelIntrStatus intr;
	uint32_t saved, result = SCE_ERROR_ERRNO_ENOSYS;
	int permission;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	permission = thread != NULL ? thread->permission : 0;
	if (thread != NULL && (permission == THREAD_PERMISSION_SYSTEM ||
			       permission == THREAD_PERMISSION_KERNEL)) {
		intr = SceCpuForDriver_821FC0EE();
		SceCpuForDriver_BF82DEB2(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		result = s_threadmgrRuntime.manager->activeCpuMask;
		sceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		sceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* sceKernelGetThreadExitStatus: an ended thread's exit status */
int sceKernelGetThreadExitStatus(SceUID uid, int *status)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	SceKernelIntrStatus intr;
	uint32_t saved;
	int result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (uid == 0 || currentThreadContext()->threadId == uid) {
		result = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		SCE_DBG_PRINTF(0, 7507, "*** error: sceKernelGetThreadExitStatus(0x%08X[%s])=0x%08X\n",
			       uid, SceSysmemForDriver_E655852F(uid), SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID);
	} else if (status == NULL) {
		result = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else {
		intr = SceCpuForDriver_821FC0EE();
		result = SceSysmemForDriver_00ED6C14(uid,
			s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
		if (result < 0) {
			result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
			if (ThreadmgrInSyscall())
				result |= SYSCALL_ERROR_BIT;
		} else {
			thread = &object->context;
			SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
			if (!(thread->status & THREAD_STATE_ENDED))
				result = SCE_KERNEL_ERROR_NOT_DORMANT;
			else if (thread->exitStatus == (int)SCE_KERNEL_ERROR_DORMANT)
				result = SCE_KERNEL_ERROR_DORMANT;
			else
				*status = thread->exitStatus;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			sceGUIDReleaseObject(uid);
		}
		sceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/*
 * SceKernelThreadMgr (threadmgr.c): waiting for another thread to end or
 * to be suspended. The waiter queues on the thread object (its waitNext
 * list), which exitDeleteThread and the suspension paths wake.
 */

#define SCE_KERNEL_ERROR_DELETED	0x8002802a
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);

/* the error for a thread ID the caller cannot name */
static inline int unknownThreadError(void)
{
	if (ThreadmgrInSyscall())
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
}

/* the running thread waits for threadId to end; status receives its exit status */
int waitThreadEnd(SceUID threadId, int *status, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *self = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	uint64_t wakeup;
	uint32_t saved;
	int intr, ret, locked;

	locked = 0;
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2367, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (threadId == 0 || self->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 2381, "*** error: irq suspended\n");
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((int)self->currentAttr < 0 && sceKernelCheckThreadUserStackSelf() == 0)
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = unknownThreadError();
		goto resume;
	}
	SceCpuForDriver_BF82DEB2(&object->waitLock);
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (self->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&object->waitLock);
			ret = callCallbackAll(self);
			if (ret < 0)
				goto release;
			SceCpuForDriver_BF82DEB2(&object->waitLock);
		}
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(&object->context));
		if (object->context.status & THREAD_STATE_ENDED) {
			if (status != NULL)
				*status = object->context.exitStatus;
			ret = 0;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			break;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(self));
		self->waitArg0 = (uint32_t)status;
		if (!callbacks) {
			ret = waitWaitableObject(threadId, (uintptr_t)&object->waitNext,
						 WAIT_TYPE_THREAD_END, &wakeup, timeout, NULL, &locked);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&self->flags,
						    THREAD_FLAG_WAKE_PENDING);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(self));
			if (!locked)
				goto resume;
			break;
		}
		SceCpuForDriver_BC248C30((SceInt32 *)&self->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(threadId, (uintptr_t)&object->waitNext,
					 WAIT_TYPE_THREAD_END, &wakeup, timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&self->flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(self));
		if (!locked)
			goto resume;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	sceKernelSpinlockLowUnlock(&object->waitLock);
release:
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sceKernelWaitThreadEnd */
int sceKernelWaitThreadEnd(SceUID threadId, int *status, SceUInt32 *timeout)
{
	return waitThreadEnd(threadId, status, timeout, 0);
}

/* SceThreadmgrForDriver_9EF4F62C */
int SceThreadmgrForDriver_9EF4F62C(SceUID threadId, int *status, SceUInt32 *timeout)
{
	return waitThreadEnd(threadId, status, timeout, 1);
}

/* the running thread waits for threadId to be suspended */
int waitThreadSuspend(SceUID threadId, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *self = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	uint32_t state;
	uint64_t wakeup;
	int intr, ret, locked;

	locked = 0;
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2545, "*** error: illegal context\n");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (threadId == 0 || self->threadId == threadId)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 2559, "*** error: irq suspended\n");
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((int)self->currentAttr < 0 && sceKernelCheckThreadUserStackSelf() == 0)
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = unknownThreadError();
		goto resume;
	}
	SceCpuForDriver_BF82DEB2(&object->waitLock);
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (self->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&object->waitLock);
			ret = callCallbackAll(self);
			if (ret < 0)
				goto release;
			SceCpuForDriver_BF82DEB2(&object->waitLock);
		}
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(&object->context));
		state = object->context.status;
		if (state & SCE_KERNEL_THREAD_STATUS_SUSPENDED) {
			ret = 0;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			break;
		}
		if (state & SCE_KERNEL_THREAD_STATUS_DORMANT) {
			ret = SCE_KERNEL_ERROR_DORMANT;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			break;
		}
		if (state & SCE_KERNEL_THREAD_STATUS_DELETED) {
			ret = SCE_KERNEL_ERROR_DELETED;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			break;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(self));
		if (!callbacks) {
			ret = waitWaitableObject(threadId, (uintptr_t)&object->waitNext,
						 WAIT_TYPE_THREAD_SUSPEND, &wakeup, timeout, NULL, &locked);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&self->flags,
						    THREAD_FLAG_WAKE_PENDING);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(self));
			if (!locked)
				goto resume;
			break;
		}
		SceCpuForDriver_BC248C30((SceInt32 *)&self->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(threadId, (uintptr_t)&object->waitNext,
					 WAIT_TYPE_THREAD_SUSPEND, &wakeup, timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&self->flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(self));
		if (!locked)
			goto resume;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	sceKernelSpinlockLowUnlock(&object->waitLock);
release:
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForKernel_392599F1: waits for threadId to be suspended */
int SceThreadmgrForKernel_392599F1(SceUID threadId, SceUInt32 *timeout)
{
	uint32_t saved = ThreadmgrUserRoWord();
	int ret;

	ThreadmgrSetUserRoWord(saved << 16);
	ret = waitThreadSuspend(threadId, timeout, 0);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForKernel_614BF2BE: the same, running callbacks */
int SceThreadmgrForKernel_614BF2BE(SceUID threadId, SceUInt32 *timeout)
{
	uint32_t saved = ThreadmgrUserRoWord();
	int ret;

	ThreadmgrSetUserRoWord(saved << 16);
	ret = waitThreadSuspend(threadId, timeout, 1);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * SceKernelThreadMgr: whether a user thread's stack pointer is inside its
 * stack (the bounds the user TLS holds).
 */

/* checkThreadUserStack: the free stack left (0: overflowed; 1: not checked) */
int checkThreadUserStack(struct ThreadCB *thread)
{
	uint32_t sp, bounds[2], version;

	SCE_DBG_ASSERT_CHECK(thread != NULL, 7710);
	SCE_DBG_ASSERT_CHECK(thread->currentAttr >> 31, 7711);
	SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_IN_CALLBACK), 7712);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 7713);
	if (thread->userFrame != NULL)
		sp = thread->userFrame->sp;
	else
		sp = thread->pRegisterContext->sp;
	if (sceKernelCopyFromUserProc(thread->processId, bounds,
				       (void *)(thread->tls - USER_TLS_STACK_RANGE),
				       sizeof(bounds)) < 0)
		return 0;
	if (sp <= bounds[0] && bounds[1] < sp)
		return sp - bounds[1];
	if (thread->processId != THREADMGR_KERNEL_PID &&
	    (SceSysrootForDriver_CAE820B2(thread->processId, &version) < 0 ||
	     version < SDK_VERSION_DEFAULT_AFFINITY))
		return 1;
	if (sp > bounds[0])
		SCE_DBG_PRINTF(0, 7743, " k: checkThreadUserStack(), threadId=0x%08X[%s], "
			"sp=0x%08X, bottom=0x%08X, top=0x%08X\n", thread->threadId,
			SceSysmemForDriver_E655852F(thread->threadId), sp, bounds[0], bounds[1]);
	else
		SCE_DBG_PRINTF(0, 7761, " k: checkThreadUserStack(), threadId=0x%08X[%s], "
			"sp=0x%08X, bottom=0x%08X, top=0x%08X\n", thread->threadId,
			SceSysmemForDriver_E655852F(thread->threadId), sp, bounds[0], bounds[1]);
	return 0;
}

/*
 * SceKernelThreadMgr (threadmgr.c): what the thread manager tells about its
 * threads and CPUs: a thread's info and free stack, the threads running on
 * each CPU, the CPUs' idle times and the thread ID lists.
 */

#define WAIT_TYPE_WITH_CALLBACKS	0x80000000	/* SceKernelThreadInfo.waitType: the wait runs callbacks */
#define THREAD_ID_LIST_ALL		0x00010021	/* PLACEHOLDER: name; every thread */
#define UID_VECTOR_LEVEL		7		/* PLACEHOLDER: name */
#define STACK_FILL_WORD			0xffffffff
#define SDK_VERSION_RUN_STATUS_PUID	0x01800000	/* newer SDKs see PUIDs in the run status */
#define SCE_KERNEL_ERROR_ILLEGAL_ADDR	0x80020006
#define SCE_KERNEL_ERROR_ILLEGAL_SIZE	0x8002000b
#define SCE_KERNEL_ERROR_NOT_SUPPORTED	0x80020004	/* PLACEHOLDER: name */

/* the calling process's SDK version (kernel callers count as the newest) */
static inline void getCallerSdkVersion(SceUInt32 *version)
{
	if (currentThreadContext() == NULL ||
	    currentThreadContext()->processId == THREADMGR_KERNEL_PID)
		*version = SDK_VERSION_KERNEL_THREAD;
	else if (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId, version) < 0)
		*version = 0;
}

/* SceThreadmgrForDriver_7B278A0B: the stack the thread never touched (0 is the caller) */
int SceThreadmgrForDriver_7B278A0B(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	uint32_t saved, *stack;
	SceSize size, words, i;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (threadId == 0) {
		if (currentThreadContext() == NULL) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto restore;
		}
		threadId = currentThreadContext()->threadId;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = unknownThreadError();
		goto resume;
	}
	thread = &object->context;
	if ((thread->currentAttr & THREAD_ATTR_UNK10000000) &&
	    !SceDipswForDriver_A98FC2FD(DIPSW_STACK_CHECK)) {
		ret = SCE_KERNEL_ERROR_NOT_SUPPORTED;
		goto release;
	}
	if ((int)thread->currentAttr < 0) {
		stack = (uint32_t *)thread->pUStack;
		size = thread->uStackSize;
	} else {
		stack = (uint32_t *)thread->pKStack;
		size = thread->kStackSize;
	}
	sceKernelCpuResumeIntr(intr);
	if ((int)thread->currentAttr < 0) {
		/* the untouched words past the stack's first */
		ret = sceKernelCountFillValueFromUserProc(thread->processId, stack + 1, STACK_FILL_WORD,
							   size - 4);
		if (ret < 0)
			goto release;	/* Sony resumes the interrupts a second time here */
		ret = (ret + 4) * 4;
	} else {
		words = size >> 2;
		if (words < 2 || stack[1] != STACK_FILL_WORD) {
			ret = 4;
		} else {
			for (i = 2; i < words && stack[i] == STACK_FILL_WORD; i++)
				;
			ret = i * 4;
		}
	}
	sceGUIDReleaseObject(threadId);
	goto restore;
release:
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sceKernelGetThreadInfo: 0 is the calling thread; a syscall sees PUIDs */
int sceKernelGetThreadInfo(SceUID threadId, SceKernelThreadInfo *output)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	SceKernelThreadInfo info;
	SceUInt32 version, state, waiting;
	SceSize count;
	SceUID puid;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (output == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (threadId == 0) {
		if (currentThreadContext() == NULL) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto restore;
		}
		threadId = currentThreadContext()->threadId;
	}
	if (output->size > sizeof(info)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	getCallerSdkVersion(&version);
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	intr = SceCpuForDriver_821FC0EE();
	ret = SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = unknownThreadError();
		sceKernelCpuResumeIntr(intr);
		goto restore;
	}
	thread = &object->context;
	strncpy(info.name, SceSysmemForDriver_E655852F(threadId), sizeof(info.name) - 1);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	state = thread->status;
	if (state & SCE_KERNEL_THREAD_STATUS_DELETED) {
		SCE_DBG_PRINTF_FUNC(0, 8002, "sceKernelGetThreadInfo", "error: sceKernelGetThreadInfo(0x%08X), deleted thread\n",
			       threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		sceGUIDReleaseObject(threadId);
		sceKernelCpuResumeIntr(intr);
		goto restore;
	}
	info.processId = thread->processId;
	info.initPriority = thread->initPriority;
	info.initCpuAffinityMask = thread->initCpuAffinityMask;
	if (ThreadmgrInSyscall()) {
		if (info.initCpuAffinityMask == 0 && version < SDK_VERSION_DEFAULT_AFFINITY)
			info.initCpuAffinityMask =
				thread->process->cpuAffinity;
		info.initCpuAffinityMask <<= 16;
	}
	info.currentPriority = thread->currentPriority;
	info.lastExecutedCpuId = thread->lastExecutedCpuId;
	info.currentCpuAffinityMask = thread->currentCpuAffinityMask;
	if (ThreadmgrInSyscall()) {
		if (info.currentCpuAffinityMask == 0 && version < SDK_VERSION_DEFAULT_AFFINITY)
			info.currentCpuAffinityMask =
				thread->process->cpuAffinity;
		info.currentCpuAffinityMask <<= 16;
	}
	if (state & THREAD_STATE_OFF_CPU) {
		/* debug-suspended: the state it was in */
		info.status = (state << 1) >> 17;
		if ((info.status & 7) == 0) {
			info.status = SCE_KERNEL_THREAD_STATUS_READY;
			waiting = 0;
			SCE_DBG_ASSERT_CHECK_FUNC(thread->currentCpuId == -1, 8039, "sceKernelGetThreadInfo");
		} else {
			waiting = info.status & SCE_KERNEL_THREAD_STATUS_WAITING;
		}
	} else {
		info.status = state & 0xffff;
		waiting = state & SCE_KERNEL_THREAD_STATUS_WAITING;
	}
	info.attr = thread->currentAttr;
	info.entry = (SceKernelThreadEntry)thread->entry;
	info.pStack = (void *)((int)info.attr < 0 ? thread->pUStack : thread->pKStack);
	info.currentCpuId = thread->currentCpuId;
	if (waiting) {
		info.waitType = thread->waitType;
		if (thread->flags & THREAD_FLAG_WAKE_PENDING)
			info.waitType |= WAIT_TYPE_WITH_CALLBACKS;
	}
	info.waitId = thread->waitId;
	if (ThreadmgrInSyscall() && info.waitId != -1) {
		if (PUIDGetPUIDVectorByGUID(0, info.waitId, 3, &puid, 4, &count) >= 0 &&
		    count != 0)
			info.waitId = puid;
	}
	info.stackSize = (int)info.attr < 0 ? thread->uStackSize : thread->kStackSize;
	ret = 0;
	info.exitStatus = thread->exitStatus;
	info.threadReleaseCount = thread->threadReleaseCount;
	info.fNotifyCallback = (thread->flags & THREAD_FLAG_CALLBACKS) != 0;
	info.runClocks.quad = thread->runClock;
	info.intrPreemptCount = thread->intrPreemptCount;
	info.threadPreemptCount = thread->threadPreemptCount;
	info.changeCpuCount = thread->changeCpuCount;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(threadId);
	sceKernelCpuResumeIntr(intr);
	memcpy(output, &info, output->size < sizeof(info) ? output->size : sizeof(info));
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_33E85E9E: what runs on each CPU */
int SceThreadmgrForDriver_33E85E9E(SceKernelThreadRunStatus *output)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadMgrCpuCB *cpu;
	SceKernelThreadRunStatus status;
	SceUInt32 version;
	uint32_t saved;
	SceUID threadId;
	int intr, ret, i;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (output == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (output->size > sizeof(status)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	status = (SceKernelThreadRunStatus){ .size = sizeof(status) };
	intr = SceCpuForDriver_821FC0EE();
	cpu = s_threadmgrRuntime.manager->cpuCB;
	if (currentThreadContext() == NULL ||
	    currentThreadContext()->processId == THREADMGR_KERNEL_PID)
		version = SDK_VERSION_KERNEL_THREAD;
	else if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
		version = 0;
	for (i = 0; i < CPU_COUNT; i++, cpu++) {
		if (ThreadmgrInSyscall() && version >= SDK_VERSION_RUN_STATUS_PUID) {
			/* a user sees only its own threads */
			threadId = cpu->threadId;
			if (SceSysmemForDriver_00ED6C14(threadId,
				s_threadmgrRuntime.manager->threadClass,
				(SceObjectBase **)&object) < 0) {
				status.cpuInfo[i].processId = -1;
				status.cpuInfo[i].threadId = -1;
				status.cpuInfo[i].priority = -1;
				continue;
			}
			status.cpuInfo[i].processId = object->context.processId;
			if (object->context.processId == currentThreadContext()->processId)
				status.cpuInfo[i].threadId = object->context.userThreadId;
			else
				status.cpuInfo[i].threadId = -1;
			status.cpuInfo[i].priority = object->context.currentPriority;
			sceGUIDReleaseObject(threadId);
		} else {
			status.cpuInfo[i].processId = cpu->processId;
			status.cpuInfo[i].threadId = cpu->threadId;
			status.cpuInfo[i].priority = cpu->priority;
		}
	}
	sceKernelCpuResumeIntr(intr);
	memcpy(output, &status, output->size < sizeof(status) ? output->size : sizeof(status));
	ret = 0;
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_7E280B69: the active CPUs and their idle threads' times */
int SceThreadmgrForDriver_7E280B69(SceKernelSystemInfo *output)
{
	struct ThreadMgrCpuCB *cpu;
	struct ThreadCB *idle;
	SceKernelSystemInfo info;
	uint32_t saved;
	int intr, ret, i;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (output == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (output->size > sizeof(info)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	intr = SceCpuForDriver_D32ACE9E(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	info.activeCpuMask = s_threadmgrRuntime.manager->activeCpuMask;
	if (ThreadmgrInSyscall())
		info.activeCpuMask <<= 16;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	for (i = 0; i < CPU_COUNT; i++) {
		cpu = &s_threadmgrRuntime.manager->cpuCB[i];
		idle = cpu->idleThread;
		info.cpuInfo[i].threadSwitchCount = cpu->threadSwitchCount;
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(idle));
		info.cpuInfo[i].comesOutOfIdleCount = idle->intrPreemptCount;
		info.cpuInfo[i].idleClock.quad = idle->runClock;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(idle));
	}
	sceKernelCpuResumeIntr(intr);
	memcpy(output, &info, output->size < sizeof(info) ? output->size : sizeof(info));
	ret = 0;
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * sceKernelGetThreadIdList: the threads of process pid (THREAD_ID_LIST_ALL:
 * every thread), as GUIDs.
 */
int sceKernelGetThreadIdList(SceUID pid, SceUID *ids, SceSize size, SceSize *count)
{
	SceSize found;
	int ret, i;

	if (size >= sizeof(SceUID) && ids == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	if (pid == THREAD_ID_LIST_ALL)
		return SceSysmemForKernel_52137FA3(
			s_threadmgrRuntime.manager->threadClass, UID_VECTOR_LEVEL, ids,
			size >> 2, count);
	ret = SceSysmemForKernel_08C05493(pid, s_threadmgrRuntime.manager->threadClass,
					  UID_VECTOR_LEVEL, ids, size >> 2, &found);
	if (ret > 0 && (int)found > 0) {
		for (i = 0; i < (int)found; i++)
			ids[i] = SceSysmemForDriver_45D22597(pid, ids[i]);
	}
	if (count != NULL)
		*count = found;
	return ret;
}

/*
 * SceKernelThreadMgr (threadmgr.c): which of the thread manager's classes
 * an object belongs to, as the small number the user libraries know.
 */

#define THREADMGR_UID_CLASS_THREAD		1
#define THREADMGR_UID_CLASS_SEMAPHORE		2
#define THREADMGR_UID_CLASS_EVENT_FLAG		3
#define THREADMGR_UID_CLASS_MUTEX		4
#define THREADMGR_UID_CLASS_COND		5
#define THREADMGR_UID_CLASS_TIMER		6
#define THREADMGR_UID_CLASS_MSG_PIPE		7
#define THREADMGR_UID_CLASS_CALLBACK		8
#define THREADMGR_UID_CLASS_THREAD_EVENT	9
#define THREADMGR_UID_CLASS_LW_MUTEX		10
#define THREADMGR_UID_CLASS_LW_COND		11
#define THREADMGR_UID_CLASS_RW_LOCK		12
#define THREADMGR_UID_CLASS_SIMPLE_EVENT	13
#define THREADMGR_UID_CLASS_EXCEPTION		0x20
#define THREADMGR_UID_CLASS_FAST_MUTEX		0x40
#define THREADMGR_UID_CLASS_DELAY		0x41
#define THREADMGR_UID_CLASS_ALARM		0x42
#define THREADMGR_UID_CLASS_UNK2374		0x43
#define THREADMGR_UID_CLASS_WORK_TASK		0x44

/* sceKernelGetThreadmgrUIDClass: the internal classes have no number */
int sceKernelGetThreadmgrUIDClass(SceUID uid)
{
	volatile struct SceKernelThreadmgrPrivate *manager;
	SceClass *cls;
	uint32_t saved;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	cls = SceSysmemForKernel_368F1C36(uid);
	manager = s_threadmgrRuntime.manager;
	if (cls == NULL)
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	else if (manager->threadClass == cls)
		ret = THREADMGR_UID_CLASS_THREAD;
	else if (manager->semaphoreClass == cls)
		ret = THREADMGR_UID_CLASS_SEMAPHORE;
	else if (manager->eventFlagClass == cls)
		ret = THREADMGR_UID_CLASS_EVENT_FLAG;
	else if (manager->mutexClass == cls)
		ret = THREADMGR_UID_CLASS_MUTEX;
	else if (manager->condClass == cls)
		ret = THREADMGR_UID_CLASS_COND;
	else if (manager->timerClass == cls)
		ret = THREADMGR_UID_CLASS_TIMER;
	else if (manager->msgPipeClass == cls)
		ret = THREADMGR_UID_CLASS_MSG_PIPE;
	else if (manager->callbackClass == cls)
		ret = THREADMGR_UID_CLASS_CALLBACK;
	else if (manager->threadEventClass == cls)
		ret = THREADMGR_UID_CLASS_THREAD_EVENT;
	else if (manager->lwMutexClass == cls)
		ret = THREADMGR_UID_CLASS_LW_MUTEX;
	else if (manager->lwCondClass == cls)
		ret = THREADMGR_UID_CLASS_LW_COND;
	else if (manager->rwLockClass == cls)
		ret = THREADMGR_UID_CLASS_RW_LOCK;
	else if (manager->simpleEventClass == cls)
		ret = THREADMGR_UID_CLASS_SIMPLE_EVENT;
	else if (manager->exceptionClass == cls)
		ret = THREADMGR_UID_CLASS_EXCEPTION;
	else if (manager->fastMutexClass == cls)
		ret = THREADMGR_UID_CLASS_FAST_MUTEX;
	else if (manager->delayClass == cls)
		ret = THREADMGR_UID_CLASS_DELAY;
	else if (manager->alarmClass == cls)
		ret = THREADMGR_UID_CLASS_ALARM;
	else if (manager->workQueueClass == cls)
		ret = THREADMGR_UID_CLASS_UNK2374;
	else if (manager->workTaskClass == cls)
		ret = THREADMGR_UID_CLASS_WORK_TASK;
	else {
		if (manager->cacheClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8455, "sceKernelGetThreadmgrUIDClass");
		else if (manager->waitableClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8458, "sceKernelGetThreadmgrUIDClass");
		else if (manager->registerCallbackClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8461, "sceKernelGetThreadmgrUIDClass");
		else if (manager->eventClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8464, "sceKernelGetThreadmgrUIDClass");
		else if (manager->cpuTimerClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8467, "sceKernelGetThreadmgrUIDClass");
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * SceKernelThreadMgr: thread queries by UID: a TLS slot's address, and
 * whether a thread is outside the process's object limits.
 */

#define TLS_SLOTS			0x100
#define TLS_SLOT_AREA			0x800	/* the slots: below the TLS pointer */
#define THREAD_ATTR_UNLIMITED_SHIFT	14
#define UNKNOWN_THREAD			0x40000000	/* PLACEHOLDER: what an unknown UID gets */

/* SceThreadmgrForDriver_66EEA46A: uid's TLS slot index (NULL if none) */
void *SceThreadmgrForDriver_66EEA46A(SceUID uid, unsigned int index)
{
	struct ThreadmgrThreadObject *object;
	SceKernelIntrStatus intr;
	uint32_t saved;
	void *result = NULL;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (index < TLS_SLOTS) {
		intr = SceCpuForDriver_821FC0EE();
		if (SceSysmemForDriver_00ED6C14(uid,
		    s_threadmgrRuntime.manager->threadClass,
		    (SceObjectBase **)&object) >= 0) {
			if (object->context.tls != 0)
				result = (void *)(object->context.tls - TLS_SLOT_AREA +
						  index * sizeof(uint32_t));
			sceGUIDReleaseObject(uid);
		}
		sceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* SceThreadmgrForDriver_1096042E: whether uid (0: the caller) has attribute 0x4000 */
int SceThreadmgrForDriver_1096042E(SceUID uid)
{
	struct ThreadmgrThreadObject *object;
	int result;

	if (uid == 0) {
		if (currentThreadContext() == NULL)
			return 0;
		return (currentThreadContext()->currentAttr >> THREAD_ATTR_UNLIMITED_SHIFT) & 1;
	}
	if (SceSysmemForDriver_00ED6C14(uid, s_threadmgrRuntime.manager->threadClass,
					 (SceObjectBase **)&object) < 0)
		return UNKNOWN_THREAD;
	result = (object->context.currentAttr >> THREAD_ATTR_UNLIMITED_SHIFT) & 1;
	sceGUIDReleaseObject(uid);
	return result;
}

/*
 * SceKernelThreadMgr (threadmgr.c): creating a thread. The thread object
 * gets its register and VFP save areas from the process heap and a kernel
 * stack; a user thread also gets a user stack, a user TLS block and a UID
 * in its process, all filled in through temporary kernel mappings. The new
 * thread waits, dormant, for a start.
 */

/* createThread's flags */
#define CREATE_FLAG_USER		0x00000001	/* PLACEHOLDER: name; not read here */
#define CREATE_FLAG_PROCESS		0x00000002	/* the thread is for the process given */
#define THREAD_ATTR_NO_STACK_FILL	0x10000000	/* PLACEHOLDER: its stacks are filled only with DIPSW 0xd8 */
#define THREAD_ATTR_EXCEPTION_LIST	0x04000000	/* PLACEHOLDER: name; it gets an exception entry */
#define THREAD_ATTR_NO_MODULE_TLS	0x02000000	/* PLACEHOLDER: name; no module TLS image is copied */
#define THREAD_ATTR_IN_SUSPENDED	0x00800000	/* PLACEHOLDER: name; may be created in a suspended process */
#define THREAD_ATTR_UID_ATTR_MASK	0x00070000	/* passed on to its UID */
#define THREAD_ATTR_OBJECT_MASK		0x0000a000	/* kept in the object header */

/* the attributes each kind of caller may not give */
#define THREAD_ATTR_ILLEGAL_ROOT	0x87f85fff
#define THREAD_ATTR_ILLEGAL_SYSTEM	0x05785fff
#define THREAD_ATTR_ILLEGAL_KERNEL	0x797f5fff	/* a kernel caller or a shared module */
#define THREAD_ATTR_ILLEGAL_USER	0x7affdfff

/* SceKernelThreadOptParam.attr: the fields given */
#define THREAD_OPT_KSTACK_TYPE		0x01
#define THREAD_OPT_USTACK_TYPE		0x02
#define THREAD_OPT_UTLS_TYPE		0x04
#define THREAD_OPT_USTACK_BLOCK		0x08
#define THREAD_OPT_HANDED_EXCP		0x10
#define THREAD_OPT_ATTR_MASK		0x1f
#define THREAD_HANDED_EXCP_DEFAULT	0x301
#define THREAD_HANDED_EXCP_MASK		0x301

/* priorities */
#define PRIORITY_DEFAULT		0		/* the creator's */
#define AFFINITY_CPU_COUNT		4
#define SDK_VERSION_BIG_STACK		0x03000000	/* from here a user stack may not pass 32 MiB */
#define USER_STACK_MAX			0x02000000
#define STACK_MIN			0x1000
#define KERNEL_STACK_USER		0x1000	/* a user thread's kernel stack */
#define KERNEL_STACK_USER_BIG		0x4000	/* the same with DIPSW 0xd9 */
#define DIPSW_BIG_KERNEL_STACK		0xd9
#define MEMBLOCK_TYPE_KSTACK_UNLIMITED	0x10a0d006	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_KSTACK_USER	0x10c0d006	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_USER_RW		0x0c20d060	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_USER_SHARED	0x0e20d060	/* PLACEHOLDER: name */
#define MEMBLOCK_TYPE_KERNEL_MIRROR	0x6020d006	/* PLACEHOLDER: name */
#define MEMBLOCK_ATTR_KSTACK		((4 << ALLOC_ATTR_BLOCK_SHIFT) | ALLOC_ATTR_NO_CLEAR | \
					 ALLOC_ATTR_HAS_EXTRA_LOW)
#define MEMBLOCK_ATTR_USTACK		((5 << ALLOC_ATTR_BLOCK_SHIFT) | ALLOC_ATTR_NO_CLEAR | \
					 ALLOC_ATTR_NO_MAP | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID | \
					 ALLOC_ATTR_HAS_EXTRA_LOW)
#define MEMBLOCK_ATTR_UTLS		((6 << ALLOC_ATTR_BLOCK_SHIFT) | ALLOC_ATTR_NO_CLEAR | \
					 ALLOC_ATTR_NO_MAP | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID)
#define MEMBLOCK_ATTR_MIRROR		(ALLOC_ATTR_SHARE_PHYPAGE | \
					 SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID)

#define ACCESS_ENTRY_KERNEL		5	/* PLACEHOLDER: SceSysmemForDriver_F4AD89D8 modes */
#define ACCESS_ENTRY_USER		0x50

/* the user TLS block: 0x800 bytes of thread words below the TLS, the module TLS above */
#define USER_TLS_OFFSET			0x800
#define USER_TLS_BLOCK_MIN		0x1000
#define USER_TLS_AREA_MAX		0xffffe800
#define USER_TLS_ROUND			(USER_TLS_OFFSET + STACK_PAGE - 1)
#define EXCEPTION_ENTRY_ALIGN		8
#define DACR_USER			0x15455551	/* and its userDacr */
#define USER_FRAME_FPSCR		0x03000000
#define USER_FRAME_UNK40		4		/* PLACEHOLDER: meaning */
#define USER_FRAME_POISON_COUNT		13
#define PROCESS_THREAD_MAX		0x80

#define DEBUG_THREAD_EVENT_CREATE	0
#define TRACE_EVENT_THREAD_CREATE	0

#define SCE_KERNEL_ERROR_ILLEGAL_TYPE		0x8002000c
#define SCE_KERNEL_ERROR_ALREADY_DEBUG_SUSPENDED	0x8002802f
#define SCE_KERNEL_ERROR_UID_RL_OVERFLOW_SYSCALL	0xc002450d	/* 0x8002450d | SYSCALL_ERROR_BIT */

typedef void (*ThreadmgrLwpCreateProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
					SceUID threadId, SceUID processId);

/* the words a user thread's TLS block starts with (below the TLS) */
struct ThreadmgrUserTls {
	uint32_t reserved0;
	SceUID threadId;			/* 0x04: its UID in the process */
	uintptr_t stackTop;			/* 0x08 */
	uintptr_t stackBase;			/* 0x0c */
	uint32_t reserved1[2];			/* 0x10: zeroed */
	uint32_t stackGuard[2];			/* 0x18: random, then the process's */
	int priority;				/* 0x20 */
	uint32_t cpuAffinityMask;		/* 0x24: << 16 */
};

static uint32_t s_procLwpCreateProbeId;
volatile uintptr_t s_procLwpCreateHandler;
extern const char s_procLwpCreateCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_procLwpCreateDescriptor = {
	.probeId = &s_procLwpCreateProbeId,
	.provider = "__proc_",
	.name = "lwp__create",
	.codeSite = s_procLwpCreateCodeSite,
	.handler = &s_procLwpCreateHandler,
};

#define PRIORITY_FORMAT \
	"***\n***\n***\n*** error: createThread(\"%s\"), illegal priority for user mode thread(%d)\n" \
	"*** This will be forbidden by 2011/04/13 !\n***\n***\n***\n"
#define AFFINITY_FORMAT \
	"*** error: createThread(), CPU affinity mask and priority mismatch. " \
	"name=[%s], mask=0x%08X, priority=%d\n"
#define ALLOC_FORMAT \
	"*** error: createThread(), sceKernelAllocMemBlockForKernel(\"%s\")=0x%08X\n"
#define FREE_USER_TLS_FORMAT "*** free uTLSMemblkId, eRet=0x%08X\n"

/*
 * Creates a thread of process pid (with CREATE_FLAG_PROCESS; otherwise of
 * the caller's process for a user thread, else the kernel's). callerAddr is
 * where a user caller called from. Returns the thread's UID: a syscall gets
 * the UID in its process.
 */
SceUID createThread(SceUID pid, const char *name, uintptr_t entry, int initPriority,
		    SceSize stackSize, uint32_t attr, uint32_t cpuAffinityMask,
		    const SceKernelThreadOptParam *option, uint32_t flags, const void *callerAddr)
{
	struct ThreadCB *self, *thread;
	struct ThreadmgrThreadObject *object;
	struct ThreadRegisterContext *regs;
	ThreadSyscallFrame *frame;
	struct ThreadmgrExceptionEntry *exception;
	ProcessBudget *type;
	SceProcessObject *process;
	struct ThreadmgrUserTls *userTls;
	UIDCreateOpt uidOpt;
	SceKernelAllocMemBlockKernelOpt opt;
	SceKernelModuleInfo info;
	SceKernelThreadOptParam param;
	MMUContext *context;
	SceAllocOpt heapOpt;
	ThreadmgrLwpCreateProbe lwpCreate;
	ThreadmgrTraceHook trace;
	SceUID ret, uid, puid, moduleId, stackMirror, tlsMirror;
	SceUInt32 sdkVersion, defaultMask;
	SceSize tlsSize;
	uint32_t saved, access, memType, handedExceptions, *stackMirrorBase;
	void *tlsBase;
	int inSyscall, shared, priority, intr, n, cpus, err;
	unsigned int cpu;

	stackMirrorBase = NULL;
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	inSyscall = ThreadmgrInSyscall();
	shared = 0;
	if (inSyscall)
		shared = SceModulemgrForKernel_F1DE6949(currentThreadContext()->processId,
							   (SceUInt32)callerAddr);
	self = currentThreadContext();
	if (self == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 3164, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (currentThreadContext()->threadEventMask != 0) {
		SCE_DBG_ASSERT_PRINTF(0, 3172,
			"*** error: illegal context(thread event handler). [%s]\n", name);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	access = (int)attr < 0 ? ACCESS_ENTRY_USER : ACCESS_ENTRY_KERNEL;
	err = SceSysmemForDriver_F4AD89D8(pid, access, (const void *)entry, 4);
	if (err != 0) {
		SCE_DBG_PRINTF(0, 3189, "*** error: createThread(%s), "
			"sceKernelIsEqualAccessibleRangeProcBySW(0x%08x, 0x%08x, %p): error 0x%08X\n",
			name, pid, access, entry, err);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}

	/* the attributes this caller may give */
	if (SceSblACMgrForKernel_sceSblACMgrIsKernelProgram(pid)) {
		if (attr & THREAD_ATTR_ILLEGAL_ROOT) {
			SCE_DBG_PRINTF(0, 3206, " k: error: createThread(%s), attr=0x%08X\n",
				       name, attr);
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(pid)) {
		if (attr & THREAD_ATTR_ILLEGAL_SYSTEM) {
			SCE_DBG_PRINTF(0, 3224, " k: error: createThread(%s), attr=0x%08X\n",
				       name, attr);
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (!inSyscall || shared) {
		if (attr & THREAD_ATTR_ILLEGAL_KERNEL) {
			SCE_DBG_PRINTF(0, 3238, " k: error: createThread(%s), attr=0x%08X\n",
				       name, attr);
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (attr & THREAD_ATTR_ILLEGAL_USER) {
		SCE_DBG_PRINTF(0, 3249, " k: error: createThread(%s), attr=0x%08X\n", name, attr);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}

	/* the options (one that does not fit is ignored) */
	param = (SceKernelThreadOptParam){ 0 };
	handedExceptions = THREAD_HANDED_EXCP_DEFAULT;
	if (option != NULL && option->size <= sizeof(param) &&
	    (option->attr & ~THREAD_OPT_ATTR_MASK) == 0) {
#if FW_DEBUG
		__memcpy_chk(&param, option, option->size, sizeof(param));
#else
		memcpy(&param, option, option->size);
#endif
		if ((int)attr < 0 && (param.attr & THREAD_OPT_KSTACK_TYPE)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
		if ((param.attr & THREAD_OPT_USTACK_TYPE) &&
		    param.uStackMemType != MEMBLOCK_TYPE_USER_RW) {
			if (param.uStackMemType != MEMBLOCK_TYPE_USER_SHARED || (!shared && inSyscall)) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
				goto restore;
			}
			attr |= THREAD_ATTR_UNLIMITED;
		}
		if ((param.attr & THREAD_OPT_UTLS_TYPE) &&
		    param.uTLSMemType != MEMBLOCK_TYPE_USER_RW &&
		    (param.uTLSMemType != MEMBLOCK_TYPE_USER_SHARED || (!shared && inSyscall))) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
			goto restore;
		}
		if ((param.attr & THREAD_OPT_USTACK_BLOCK) &&
		    (inSyscall || (int)param.uStackMemid < 0)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
		if (param.attr & THREAD_OPT_HANDED_EXCP) {
			handedExceptions = param.handedExceptions;
			if (handedExceptions & ~THREAD_HANDED_EXCP_MASK) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_TYPE;
				goto restore;
			}
		}
	}

	/* the process it is for */
	if (!(flags & CREATE_FLAG_PROCESS)) {
		if ((int)attr < 0 && (self = currentThreadContext()) != NULL)
			pid = self->processId;
		else
			pid = THREADMGR_KERNEL_PID;
	}
	if (pid == THREADMGR_KERNEL_PID)
		sdkVersion = SDK_VERSION_KERNEL;
	else if (SceSysrootForDriver_CAE820B2(pid, &sdkVersion) < 0)
		sdkVersion = 0;
	process = SceSysmemForKernel_FA6C098C(pid);
	if (process == NULL) {
		ret = SCE_KERNEL_ERROR_INVALID_UID;
		goto restore;
	}
	ret = SceSysmemForDriver_0FC24464(process->budgetId, (SceObjectBase **)&type);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 3344,
			" k: error: createThread(%s)=0x%08X, pProcess->uidBudget=0x%08X\n",
			name, ret, process->budgetId);
		goto restore;
	}

	/* its priority */
	priority = initPriority;
	if ((unsigned int)initPriority - 1 >= BOOST_PRIORITY_NONE) {
		if (initPriority == PRIORITY_DEFAULT) {
			priority = currentThreadContext()->currentPriority;
		} else if (!(initPriority & PRIORITY_RELATIVE)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto restore;
		} else if ((unsigned int)initPriority - PRIORITY_RELATIVE_BASE >=
			   PRIORITY_RELATIVE_RANGE) {
			priority = initPriority - PRIORITY_RELATIVE_OFFSET_OLD;
			SCE_DBG_PRINTF(0, 3355, "***\n***\n***\n*** error: createThread(\"%s\"), "
				"illegal priority offset(%d)\n*** This will be forbidden by 2011/04/13 !\n"
				"***\n***\n***\n", name, priority);
			ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto restore;
		} else {
			switch (type->type) {
			case PROCESS_TYPE_GAME:
				if (!(attr & THREAD_ATTR_UNLIMITED) ||
				    !(cpuAffinityMask & AFFINITY_SYSTEM_CPU)) {
					priority = initPriority - PRIORITY_RELATIVE_OFFSET_SYSTEM;
					break;
				}
				/* fall through */
			case PROCESS_TYPE_MINI_APP:
			case PROCESS_TYPE_SHELL:
			case PROCESS_TYPE_KERNEL:
				priority = initPriority - PRIORITY_RELATIVE_OFFSET;
				break;
			default:
				priority = initPriority - PRIORITY_RELATIVE_OFFSET;
				SCE_DBG_PRINTF(0, 3375,
					"*** warning: unknown process type(0x%08X), [%s] priority %d\n",
					type->type, name, priority);
				break;
			}
		}
	}
	if ((int)attr < 0) {
		if (type->type == PROCESS_TYPE_GAME) {
			if ((unsigned int)priority - PRIORITY_USER_LOW >= PRIORITY_USER_RANGE_SYSTEM) {
				SCE_DBG_PRINTF(0, 3388, PRIORITY_FORMAT, name, priority);
				ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
				goto restore;
			}
		} else if ((unsigned int)priority - PRIORITY_USER_LOW >= PRIORITY_USER_RANGE) {
			SCE_DBG_PRINTF(0, 3394, PRIORITY_FORMAT, name, priority);
			ret = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto restore;
		}
	}

	/* its CPU affinity */
	ret = checkCpuAffinityMaskPhy(cpuAffinityMask, attr, process);
	if (ret < 0)
		goto restore;
	if (sdkVersion < SDK_VERSION_AFFINITY_CHECK && s_threadmgrRuntime.manager->testMode == 0) {
		if ((unsigned int)priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE) {
			/* a shared priority: on the process type's CPUs */
			if (cpuAffinityMask != 0) {
				defaultMask = sceKernelGetDefaultCpuAffinityMask(type);
				if ((defaultMask & cpuAffinityMask) !=
				    sceKernelGetDefaultCpuAffinityMask(type)) {
					SCE_PRINTF_WITH_INFO(0, SCE_LINE(3407, 3413), AFFINITY_FORMAT,
							     name, cpuAffinityMask, priority);
					ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
					goto restore;
				}
			}
		} else if (cpuAffinityMask != 0) {
			/* a CPU's own priority: on one CPU */
			cpus = 0;
			for (cpu = 0; cpu != AFFINITY_CPU_COUNT; cpu++) {
				if (((int)cpuAffinityMask >> cpu) & 1)
					cpus++;
			}
			if (cpus > 1) {
				if (strncmp(name, "ScePsp2SwuCui", 0x1f) != 0) {
					SCE_PRINTF_WITH_INFO(0, SCE_LINE(3425, 3431), AFFINITY_FORMAT,
							     name, cpuAffinityMask, priority);
					ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
					goto restore;
				}
				SCE_DBG_PRINTF(0, 3437, "=== [ScePsp2SwuCui]: initPriority=%d, "
					"cpuAffinityMask=0x%08X, accepted to keep compatibility.\n",
					priority, cpuAffinityMask);
			}
		}
	}

	/* its stack size */
	if ((int)attr < 0 && stackSize > USER_STACK_MAX && sdkVersion >= SDK_VERSION_BIG_STACK) {
		SCE_DBG_PRINTF(0, 3447, "*** error: createThread(), name=[%s], stackSize=0x%08X\n",
			       name, stackSize);
		ret = SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
		goto restore;
	}
	stackSize = ((stackSize + STACK_PAGE - 1) / STACK_PAGE) * STACK_PAGE;
	ret = SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	if (stackSize == 0)
		goto restore;
	ret = SceSysmemForDriver_2ECF7944(pid, &context);
	if (ret < 0)
		goto restore;

	/* the object, handed over locked */
	uidOpt = (UIDCreateOpt){
		.flags = UID_CREATE_OPT_ATTR,
		.attr = attr & THREAD_ATTR_UID_ATTR_MASK,
	};
	if (pid != THREADMGR_KERNEL_PID) {
		uidOpt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
		uidOpt.pid = pid;
		if (attr & THREAD_ATTR_UNLIMITED)
			uidOpt.heapKind = 1;
	}
	intr = SceCpuForDriver_D32ACE9E(&process->lock);
	uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->threadClass, name,
					  &uidOpt,
					  (SceObjectBase **)&object);
	ret = uid;
	if (uid < 0)
		goto unlock_process;
	thread = &object->context;
	object->ownerPid = pid;
	object->attributes = attr & THREAD_ATTR_OBJECT_MASK;
	thread->boostPriority = BOOST_PRIORITY_NONE;
	thread->threadId = uid;
	thread->initAttr = attr;
	thread->currentAttr = attr;
	thread->changeCpuCount = -1;
	thread->initPriority = priority;
	thread->basePriority = priority;
	thread->currentPriority = priority;
	thread->waitId = -1;
	thread->currentCpuId = -1;
	thread->lastExecutedCpuId = -1;
	thread->delayId = -1;
	thread->uStackMemblkId = -1;
	thread->kStackMemblkId = -1;
	thread->userTlsMemBlockId = -1;
	thread->entry = entry;
	thread->processId = pid;
	thread->process = process;
	thread->magic = THREAD_CB_MAGIC;
	thread->syslibtraceState = NULL;
	if (s_threadmgrRuntime.manager->syslibtraceThreadCBEnable)
		syslibtraceThreadCBInit(thread);
	thread->dtraceState = NULL;
	thread->dtraceLock = 0;
	if (s_threadmgrRuntime.manager->dtraceThreadCBEnable)
		dtraceThreadCBInit(thread);
	thread->addressSpace = SceSysmemForKernel_FA6C098C(SceSysrootForKernel_7334F1E8(pid));
	ret = SceSysrootForKernel_88DE85EF(pid, &thread->pUIDEntryHeap);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 3539, "*** error: createThread(), sceKernelSysrootGetUIDEntryHeap"
			"(0x%08X)=0x%08X, pThreadCB->pUIDEntryHeap=%p\n",
			pid, ret, thread->pUIDEntryHeap);
		goto fail_kernel_stack;
	}
	thread->initCpuAffinityMask = cpuAffinityMask & AFFINITY_MASK;
	thread->currentCpuAffinityMask = cpuAffinityMask & AFFINITY_MASK;
	thread->initPermission = process->threadPermission;
	thread->permission = process->threadPermission;
	thread->tlsProcessId = pid;
	thread->ioFlags = 0;

	/* its kernel stack */
	memset(&opt, 0, sizeof(opt));
	opt.attr = MEMBLOCK_ATTR_KSTACK;
	opt.size = sizeof(opt);
	if ((int)attr < 0) {
		thread->kStackSize = SceDipswForDriver_A98FC2FD(DIPSW_BIG_KERNEL_STACK) ?
			KERNEL_STACK_USER_BIG : KERNEL_STACK_USER;
		thread->stackMemBlockType = (attr & THREAD_ATTR_UNLIMITED) ?
			MEMBLOCK_TYPE_KSTACK_UNLIMITED : MEMBLOCK_TYPE_KSTACK_USER;
		opt.attr |= SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID;
		opt.pid = pid;
	} else {
		if (stackSize < STACK_MIN) {
			stackSize = STACK_MIN;
			SCE_DBG_PRINTF(0, 3581, "warning: specified stack size for [%s] is less "
				       "than 8KiB.(resized)\n", name);
		}
		thread->kStackSize = stackSize;
		if ((param.attr & THREAD_OPT_KSTACK_TYPE) && param.kStackMemType != 0)
			thread->stackMemBlockType = param.kStackMemType;
		else
			thread->stackMemBlockType = MEMBLOCK_TYPE_KERNEL_STACK;
	}
	opt.extraLow = STACK_PAGE;	/* a guard page below */
	thread->kStackMemblkId = SceSysmemForDriver_C94850C9(name, thread->stackMemBlockType,
		thread->kStackSize + STACK_PAGE, &opt);
	if (thread->kStackMemblkId < 0) {
		SCE_DBG_PRINTF(0, 3604, "*** error: createThread(), "
			"sceKernelAllocMemBlockForKernel(\"kStack: %s\")=0x%08X\n",
			name, thread->kStackMemblkId);
		ret = thread->kStackMemblkId;
		goto fail_kernel_stack;
	}
	SceSysmemForDriver_0B1FD5C3(thread->kStackMemblkId, (void **)&thread->pKStack);

	if ((int)attr >= 0) {
		tlsSize = 0;
		moduleId = -1;
		tlsMirror = -1;
		thread->uStackMemblkId = -1;
		stackMirror = -1;
		puid = -1;
		thread->userTlsMemBlockId = -1;
		thread->userThreadId = -1;
		thread->vfpException = 0x87;	/* PLACEHOLDER: meaning */
		goto contexts;
	}

	/* a user thread: its user stack, seen by the kernel through a mirror */
	if (param.attr & THREAD_OPT_USTACK_BLOCK) {
		thread->uStackSize = stackSize;
		thread->uStackMemblkId = param.uStackMemid;
	} else {
		memType = (param.attr & THREAD_OPT_USTACK_TYPE) ?
			param.uStackMemType : MEMBLOCK_TYPE_USER_RW;
		memset(&opt, 0, sizeof(opt));
		thread->uStackSize = stackSize;
		opt.size = sizeof(opt);
		opt.attr = MEMBLOCK_ATTR_USTACK;
		opt.extraLow = STACK_PAGE;	/* a guard page below */
		opt.pid = pid;
		thread->uStackMemblkId = SceSysmemForDriver_C94850C9(name, memType,
			stackSize + STACK_PAGE, &opt);
		if (thread->uStackMemblkId < 0) {
			SCE_DBG_PRINTF(0, 3637, "*** error: createThread(), "
				"sceKernelAllocMemBlockForKernel(\"uStack: %s\")=0x%08X\n",
				name, thread->uStackMemblkId);
			ret = thread->uStackMemblkId;
			goto fail_user_stack;
		}
	}
	SceSysmemForDriver_0B1FD5C3(thread->uStackMemblkId,
					(void **)&thread->pUStack);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = MEMBLOCK_ATTR_MIRROR;
	opt.mirror_blockid = thread->uStackMemblkId;
	stackMirror = SceSysmemForDriver_C94850C9("UserStackKVA", MEMBLOCK_TYPE_KERNEL_MIRROR, 0,
					      &opt);
	if (stackMirror < 0) {
		SCE_DBG_PRINTF(0, 3667, ALLOC_FORMAT, "UserStackKVA", stackMirror);
		ret = stackMirror;
		goto fail_stack_mirror;
	}
	SceSysmemForDriver_0B1FD5C3(stackMirror, (void **)&stackMirrorBase);

	/* its TLS block: the thread words, then the main module's TLS */
	moduleId = -1;
	tlsSize = USER_TLS_BLOCK_MIN;
	if (!(attr & THREAD_ATTR_NO_MODULE_TLS)) {
		moduleId = sceKernelGetModuleIdByPid(pid);
		if (SceSysmemForDriver_0F5C84B7(moduleId, NULL) < 0) {
			moduleId = -1;
			tlsSize = USER_TLS_BLOCK_MIN;
		} else {
			memset(&info, 0, sizeof(info));
			info.size = sizeof(info);
			if (SceModulemgrForKernel_DAA90093(pid, moduleId, &info) < 0 ||
			    info.tlsAreaSize > USER_TLS_AREA_MAX)
				tlsSize = USER_TLS_BLOCK_MIN;
			else
				tlsSize = (info.tlsAreaSize + USER_TLS_ROUND) & ~(STACK_PAGE - 1);
		}
	}
	memType = (param.attr & THREAD_OPT_UTLS_TYPE) ? param.uTLSMemType : MEMBLOCK_TYPE_USER_RW;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = MEMBLOCK_ATTR_UTLS;
	opt.pid = pid;
	thread->userTlsMemBlockId = SceSysmemForDriver_C94850C9(name, memType, tlsSize,
		&opt);
	if (thread->userTlsMemBlockId < 0) {
		SCE_DBG_PRINTF(0, 3720, "*** error: createThread(), "
			"sceKernelAllocMemBlockForKernel(\"uTLS: %s\")=0x%08X\n",
			name, thread->userTlsMemBlockId);
		ret = thread->userTlsMemBlockId;
		thread->userTlsMemBlockId = -1;
		thread->tls = 0;
		goto fail_tls;
	}
	SceSysmemForDriver_B81CF0A3(thread->userTlsMemBlockId, &tlsBase);
	thread->tls = (uintptr_t)tlsBase + USER_TLS_OFFSET;
	memset(&opt, 0, sizeof(opt));
	opt.attr = MEMBLOCK_ATTR_MIRROR;
	opt.mirror_blockid = thread->userTlsMemBlockId;
	opt.size = sizeof(opt);
	tlsMirror = SceSysmemForDriver_C94850C9("UserTLSKVA", MEMBLOCK_TYPE_KERNEL_MIRROR, 0,
					    &opt);
	if (tlsMirror < 0) {
		SCE_DBG_PRINTF(0, 3744, ALLOC_FORMAT, "UserTLSKVA", thread->userTlsMemBlockId);
		ret = tlsMirror;
		goto fail_tls_mirror;
	}
	SceSysmemForDriver_B81CF0A3(tlsMirror, (void **)&userTls);

	if (attr & THREAD_ATTR_EXCEPTION_LIST) {
		memset(&heapOpt, 0, sizeof(heapOpt));
		heapOpt.size = sizeof(heapOpt);
		heapOpt.align = EXCEPTION_ENTRY_ALIGN;
		exception = SceSysrootForKernel_D351EBC8(pid, 0x28, &heapOpt);
		thread->exceptionEntry = exception;
		if (exception == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			SCE_DBG_PRINTF(0, 3760, "*** error: createThread(), "
				"sceKernelSysrootAllocRemoteProcessHeap(0x%08X)=%p, pExceptionList\n",
				pid, NULL);
			goto fail_exception;
		}
		memset(exception, 0, 0x28);
		thread->exceptionEntry->thread = thread;
		thread->handedExceptions = handedExceptions;
	}
	thread->vfpException = 0;
	puid = SceSysmemForDriver_BF209859(pid, uid);
	if (puid < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 3773);
		ret = puid;
		goto fail_puid;
	}
	thread->userThreadId = puid;

contexts:
	/* its register and VFP save areas */
	memset(&heapOpt, 0, sizeof(heapOpt));
	heapOpt.size = sizeof(heapOpt);
	heapOpt.align = REGISTER_CONTEXT_ALIGN;
	thread->pRegisterContext = SceSysrootForKernel_D351EBC8(pid, sizeof(*thread->pRegisterContext), &heapOpt);
	if (thread->pRegisterContext == NULL) {
		SCE_DBG_PRINTF(0, 3795, "*** error: createThread(), "
			"sceKernelSysrootAllocRemoteProcessHeap(0x%08X)=%p, pRegisterContext\n",
			pid, NULL);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto fail_registers;
	}
	memset(thread->pRegisterContext, 0, sizeof(*thread->pRegisterContext));
	regs = thread->pRegisterContext;
	regs->ttbr1 = context->ttbr1;
	regs->kernelDacr = DACR_KERNEL;
	regs->contextidr = (uint8_t)context->contextId | uid << 8;
	if ((int)thread->currentAttr < 0) {
		/* the syscall frame its first return to user mode leaves through */
		regs->userDacr = DACR_USER;
		thread->kernelStackTop = thread->pKStack + thread->kStackSize;
		frame = (ThreadSyscallFrame *)thread->kernelStackTop - 1;
		for (n = 0; n < USER_FRAME_POISON_COUNT; n++)
			frame->r[n] = REGISTER_POISON;
		frame->syscallAux = 0;
		frame->entry = REGISTER_POISON;
		frame->fpscr = USER_FRAME_FPSCR;
		frame->apiDeclare = USER_FRAME_UNK40;
		frame->sp = thread->uStackSize + thread->pUStack;
		frame->lr = (uint32_t)SceSysrootForKernel_5F2CE289(thread->process);
		frame->pc = entry & ~1;
		thread->userFrame = frame;
	}
	memset(&heapOpt, 0, sizeof(heapOpt));
	heapOpt.size = sizeof(heapOpt);
	heapOpt.align = REGISTER_CONTEXT_ALIGN;
	thread->pVfpRegisterContext = (uintptr_t)SceSysrootForKernel_D351EBC8(pid,
		sizeof(struct ThreadmgrVfpContext), &heapOpt);
	if (thread->pVfpRegisterContext == 0) {
		SCE_DBG_PRINTF(0, 3854, "*** error: createThread(), "
			"sceKernelSysrootAllocRemoteProcessHeap(0x%08X)=%p, pVfpRegisterContext\n",
			pid, NULL);
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto fail_vfp;
	}
	if (SceDipswForDriver_A98FC2FD(DIPSW_THBP)) {
		thread->thbp[0] = thread->process->contextBreakpoint->value;
		thread->thbp[1] = thread->process->contextBreakpoint->control;
	}

	/* dormant */
	thread->exitStatus = SCE_KERNEL_ERROR_DORMANT;
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			 SCE_KERNEL_THREAD_STATUS_DORMANT;
	SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->dormantNext;
	thread->prev = s_threadmgrRuntime.manager->dormantPrev;
	s_threadmgrRuntime.manager->dormantPrev = (uintptr_t)thread;
	((struct ThreadCB *)thread->prev)->next = (uintptr_t)thread;
	sceKernelSpinlockLowUnlock(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	if ((int)thread->currentAttr < 0) {
		/* a user thread of a process the debugger holds */
		if (!(flags & CREATE_FLAG_PROCESS)) {
			if (process->status & (PROCESS_FLAG_DEBUG_SUSPENDING | PROCESS_STATE_DEBUG)) {
				thread->debugSuspendReason |= DEBUG_SUSPEND_PROCESS;
				thread->status |= THREAD_STATE_DEBUG_SUSPENDED;
			}
		} else if (((process->status & (PROCESS_FLAG_DEBUG_SUSPENDING | PROCESS_STATE_DEBUG)) ||
			    (process->status & PROCESS_STATE_MASK) == PROCESS_STATE_SUSPENDED) &&
			   !(attr & THREAD_ATTR_IN_SUSPENDED)) {
			ret = SCE_KERNEL_ERROR_ALREADY_DEBUG_SUSPENDED;
			goto fail_vfp_free;
		}
	}

	/* counted and listed in its process */
	if (!(attr & THREAD_ATTR_UNLIMITED)) {
		if ((unsigned int)process->threadCount >= PROCESS_THREAD_MAX) {
			ret = SCE_KERNEL_ERROR_UID_RL_OVERFLOW_SYSCALL;
			goto fail_vfp_free;
		}
		if ((uint32_t)++process->threadCount > process->threadPeak)
			process->threadPeak = process->threadCount;
	} else {
		if ((uint32_t)++process->unlimitedThreadCount > process->unlimitedThreadPeak)
			process->unlimitedThreadPeak = process->unlimitedThreadCount;
	}
	thread->processLink.next = &process->threads;
	thread->processLink.prev = process->threads.prev;
	process->threads.prev = &thread->processLink;
	thread->processLink.prev->next = &thread->processLink;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(&object->waitLock);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);

	/* its stacks: filled, with its UID at the bottom; a user thread's TLS words */
	if ((int)attr < 0) {
		if (!(attr & THREAD_ATTR_NO_STACK_FILL) || SceDipswForDriver_A98FC2FD(DIPSW_STACK_CHECK)) {
			memset((void *)thread->pKStack, STACK_FILL,
			       thread->kStackSize - sizeof(ThreadSyscallFrame));
			memset(stackMirrorBase, STACK_FILL, thread->uStackSize);
		}
		*(SceUID *)thread->pKStack = thread->threadId;
		*stackMirrorBase = uid;
		memset(userTls, 0, tlsSize);
		userTls->threadId = puid;
		userTls->stackBase = thread->pUStack;
		userTls->reserved1[0] = 0;
		userTls->reserved1[1] = 0;
		userTls->stackTop = thread->pUStack + thread->uStackSize;
		userTls->priority = thread->initPriority;
		defaultMask = thread->initCpuAffinityMask;
		if (defaultMask == 0 && sdkVersion < SDK_VERSION_DEFAULT_AFFINITY)
			defaultMask = process->cpuAffinity;
		userTls->cpuAffinityMask = defaultMask << 16;
		SceKernelUtilsForDriver_875B2A1C(&userTls->stackGuard[0], sizeof(userTls->stackGuard[0]));
		userTls->stackGuard[1] =
			thread->process->stackGuard;
		if (!(attr & THREAD_ATTR_NO_MODULE_TLS) && info.tlsInitSize != 0) {
			ret = sceKernelCopyFromUserProc(pid, (uint8_t *)userTls + USER_TLS_OFFSET,
							 info.tlsInit, info.tlsInitSize);
			if (ret < 0) {
				SCE_DBG_ASSERT_LEVEL(0, 0, 3978);
				intr = SceCpuForDriver_D32ACE9E(
					&process->lock);
				SceCpuForDriver_BF82DEB2(&object->waitLock);
				SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
				thread->processLink.next->prev = thread->processLink.prev;
				thread->processLink.prev->next = thread->processLink.next;
				if (!(attr & THREAD_ATTR_UNLIMITED))
					process->threadCount--;
				else
					process->unlimitedThreadCount--;
				goto fail_vfp_free;
			}
		}
	} else {
		if (!(attr & THREAD_ATTR_NO_STACK_FILL) || SceDipswForDriver_A98FC2FD(DIPSW_STACK_CHECK))
			memset((void *)thread->pKStack, STACK_FILL, thread->kStackSize);
		*(SceUID *)thread->pKStack = thread->threadId;
	}

	/* the mirrors go; the thread is announced */
	intr = SceCpuForDriver_821FC0EE();
	if (moduleId != -1)
		sceGUIDReleaseObject(moduleId);
	if ((int)attr < 0) {
		if (SceSysmemForDriver_58D21746(thread->uStackMemblkId) < 0)
			SCE_PANIC(SCE_LINE(3997, 4003));
		if (SceSysmemForDriver_009E1C61(stackMirror) < 0)
			SCE_PANIC(SCE_LINE(4002, 4008));
		if (SceSysmemForDriver_58D21746(thread->userTlsMemBlockId) < 0)
			SCE_PANIC(SCE_LINE(4007, 4013));
		if (SceSysmemForDriver_009E1C61(tlsMirror) < 0)
			SCE_PANIC(SCE_LINE(4012, 4018));
	}
	notifyDebugThreadEvent(pid, puid, DEBUG_THREAD_EVENT_CREATE, NULL);
	/* the DTrace probe site __proc_:lwp__create */
	__asm__ volatile(".global s_procLwpCreateCodeSite\ns_procLwpCreateCodeSite:");
	lwpCreate = (ThreadmgrLwpCreateProbe)s_procLwpCreateHandler;
	if (lwpCreate != NULL)
		lwpCreate(&s_procLwpCreateDescriptor, (int)attr < 0 ? puid : uid, pid);
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_THREAD_CREATE, 3, entry, uid, pid);
	ret = inSyscall ? puid : uid;
	sceKernelCpuResumeIntr(intr);
	goto restore;

	/* undone in reverse */
fail_vfp_free:
	err = SceSysrootForKernel_7968F091(pid, (void *)thread->pVfpRegisterContext);
	SCE_DBG_PRINTF(0, 4058, "*** free pVfpRegisterContext, eRet=0x%08X\n", err);
	thread->pVfpRegisterContext = 0;
fail_vfp:
	err = SceSysrootForKernel_7968F091(pid, thread->pRegisterContext);
	SCE_DBG_PRINTF(0, 4062, "*** free pRegisterContext, eRet=0x%08X\n", err);
	thread->pRegisterContext = NULL;
fail_registers:
	if ((int)attr < 0) {
		err = SceSysmemForDriver_84A4AF5E(pid, puid);
		SCE_DBG_PRINTF(0, 4067, "*** scePUIDClose, eRet=0x%08X\n", err);
		if (err < 0)
			SCE_PANIC(SCE_LINE(4063, 4069));
	}
fail_puid:
	if (attr & THREAD_ATTR_EXCEPTION_LIST) {
		err = SceSysrootForKernel_7968F091(pid, thread->exceptionEntry);
		SCE_DBG_PRINTF(0, 4075, "*** free pExceptionList, eRet=0x%08X\n", err);
		thread->exceptionEntry = NULL;
	}
	if ((int)attr < 0) {
fail_exception:
		err = SceSysmemForDriver_009E1C61(tlsMirror);
		SCE_DBG_PRINTF(0, 4081, "*** free uTLSTempId, eRet=0x%08X\n", err);
	}
fail_tls_mirror:
	err = SceSysmemForDriver_009E1C61(thread->userTlsMemBlockId);
	SCE_DBG_PRINTF(0, 4085, FREE_USER_TLS_FORMAT, err);
	thread->userTlsMemBlockId = -1;
	thread->tls = 0;
	if ((int)attr < 0) {
fail_tls:
		err = SceSysmemForDriver_009E1C61(stackMirror);
		SCE_DBG_PRINTF(0, 4091, "*** free uStackTempId, eRet=0x%08X\n", err);
		if (moduleId != -1)
			sceGUIDReleaseObject(moduleId);
	}
fail_stack_mirror:
	err = SceSysmemForDriver_009E1C61(thread->uStackMemblkId);
	SCE_DBG_PRINTF(0, 4099, "*** free uStackMemblkId, eRet=0x%08X\n", err);
fail_user_stack:
	thread->uStackMemblkId = -1;
	thread->pUStack = 0;
	err = SceSysmemForDriver_009E1C61(thread->kStackMemblkId);
	SCE_DBG_PRINTF(0, 4104, "*** free kStackMemblkId, eRet=0x%08X\n", err);
fail_kernel_stack:
	thread->kStackMemblkId = -1;
	thread->pKStack = 0;
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			 SCE_KERNEL_THREAD_STATUS_DELETED;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowUnlock(&object->waitLock);
	if (sceGUIDClose(uid) < 0)
		SCE_PANIC(SCE_LINE(4107, 4113));
unlock_process:
	sceKernelSpinlockLowUnlock(&process->lock);
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sceKernelCreateThread: cpuAffinityMask may name the CPUs in bits 16-19 */
SceUID sceKernelCreateThread(const char *name, SceKernelThreadEntry entry, int initPriority,
			     SceSize stackSize, uint32_t attr, uint32_t cpuAffinityMask,
			     const SceKernelThreadOptParam *option)
{
	if (cpuAffinityMask & ~0xf000f)
		return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
	if (cpuAffinityMask & 0xf0000) {
		if (cpuAffinityMask & 0xf)
			return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		cpuAffinityMask = (int)cpuAffinityMask >> 16;
	}
	return createThread(0, name, (uintptr_t)entry, initPriority, stackSize, attr,
			    cpuAffinityMask, option, attr >> 31, NULL);
}

/* SceThreadmgrForDriver_C8E57BB4: a user thread of process pid */
SceUID SceThreadmgrForDriver_C8E57BB4(SceUID pid, const char *name, SceKernelThreadEntry entry,
				      int initPriority, SceSize stackSize, uint32_t attr,
				      uint32_t cpuAffinityMask,
				      const SceKernelThreadOptParam *option)
{
	SceUID ret;

	if (pid == THREADMGR_KERNEL_PID || pid == 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = SceSysmemForDriver_00ED6C14(pid,
		s_threadmgrRuntime.manager->processClass, NULL);
	if (ret < 0)
		return ret;
	if (cpuAffinityMask & ~0xf000f) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
	} else if ((cpuAffinityMask & 0xf0000) && (cpuAffinityMask & 0xf)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
	} else {
		if (cpuAffinityMask & 0xf0000)
			cpuAffinityMask = (int)cpuAffinityMask >> 16;
		ret = createThread(pid, name, (uintptr_t)entry, initPriority, stackSize,
				   attr | THREAD_ATTR_USER, cpuAffinityMask, option,
				   CREATE_FLAG_PROCESS, NULL);
	}
	sceGUIDReleaseObject(pid);
	return ret;
}

/*
 * SceKernelThreadMgr (threadmgr.c): stopping another thread. A suspend or
 * exit request is marked on the thread; wherever it is (running, queued,
 * waiting, suspended, ...) it is moved so that it runs at the given priority
 * on any CPU until it reaches the point where it stops: the trap on the way
 * out of a syscall, or, when it is not in one, an exit set up in its saved
 * context. A suspension that can happen at once happens here.
 */

#define THREAD_CPU_NONE			0x7fff
#define WAIT_TYPE_COND			0x00000100
#define WAIT_TYPE_MUTEX_2		0x00000200
#define WAIT_TYPE_LW_COND		0x00200000
#define WAIT_TYPE_LW_MUTEX_2		0x00400000
#define EVENT_ATTR_PRIORITY		WAIT_ATTR_PRIORITY
#define EVENT_DELETED_PATTERN		0x00000010	/* PLACEHOLDER: the pattern a cut wait reports */
#define SCE_KERNEL_ERROR_ILLEGAL_PRIORITY	0x80028023
void setupThreadExit(struct ThreadCB *thread, uint32_t request);
void insertPriorityQueue(uintptr_t head, struct ThreadCB *thread);
int revalidateWaitState(uintptr_t waitable);
void runningThreadPreemptByIntr(struct ThreadCB *thread, uint32_t reason);
void issueStopThreadIntrRequest(unsigned int cpuId, SceUID threadId, uint32_t arg1,
				uint32_t arg2);

/* the thread leaves the manager list it is on */
static inline void unlistThread(struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;

	SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
	self->next->prev = self->prev;
	self->prev->next = self->next;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/* the thread leaves its CPU's standby list */
static inline void unlistStandby(struct ThreadMgrCpuCB *cpu,
				 struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;

	SceCpuForDriver_BF82DEB2(&cpu->standbyLock);
	self->next->prev = self->prev;
	self->prev->next = self->next;
	sceKernelSpinlockLowUnlock(&cpu->standbyLock);
}

/* until it stops, the thread runs at priority on any CPU */
static inline void boostToStop(struct ThreadCB *thread, int priority)
{
	thread->savedCpuAffinityMask = thread->currentCpuAffinityMask;
	thread->currentPriority = priority;
	thread->currentCpuAffinityMask |= CPU_MASK_ALL;
}

/* a priority only this CPU's queues hold (not the shared 0x80-0xfb) */
static inline int cpuLocalPriority(int priority)
{
	return (unsigned int)(priority - 0x80) > 0x7b;
}

/*
 * Stops the thread threadId: request is a THREAD_FLAG_REQUEST_* bit, the
 * thread runs at priority until it gets there. A thread on another CPU is
 * reached with SGI kind 9.
 */
int stopThread(SceUID threadId, int priority, uint32_t request)
{
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadmgrWaitNode *head, *node, *next;
	struct ThreadmgrWaitQueue *queue;
	struct ThreadmgrExceptionEntry *entry;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrEventWaitNode *eventNode;
	struct ThreadCB *waiter;
	ThreadmgrEvent *event;
	SceUID waitId, eventId;
	uint32_t state, status, suspend, i, left;
	int intr, ret, error, cpuId, target;
	volatile int spin;
	uint32_t wake;

	if (threadId == 0 ||
	    (currentThreadContext() != NULL && currentThreadContext()->threadId == threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	if ((unsigned int)(priority - 1) > 0xfd)
		return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
	intr = SceCpuForDriver_821FC0EE();
	cpuId = sceKernelCpuId();
	cpu = currentCpuState();
	ret = SceSysmemForDriver_00ED6C14(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	wake = 0;
	suspend = request & THREAD_FLAG_REQUEST_SUSPEND;
retry:
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if (thread->flags & THREAD_FLAG_PENDING_STOP)
		goto unlock;
	state = thread->status;
	if (state & THREAD_STATE_QUEUED) {
		/* on a CPU: another CPU's thread is stopped there */
		if (thread->currentCpuId != cpuId && thread->currentCpuId != THREAD_CPU_NONE) {
			issueStopThreadIntrRequest(thread->currentCpuId, threadId, priority, request);
			goto unlock;
		}
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, request);
		if (thread->status & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			/* it is the thread this SGI interrupted */
			SCE_DBG_ASSERT_CHECK(SceIntrmgrForDriver_182EE3E3() != 0, 8809);
			SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_SELF_STOPPED), 8810);
			if (!(request & THREAD_FLAG_REQUEST_SUSPEND)) {
				if (thread->userFrame == NULL)
					setupThreadExit(thread, request);
				thread->currentPriority = priority;
				cpu->priority = priority;
				cpu->disabledDispatch = 0;
				if (searchHighestPriority() < priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8848);
					wake = 1;
					cpu->nextRunningThread = 0;
				}
				goto unlock;
			}
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
			if (thread->userFrame == NULL) {
				runningThreadPreemptByIntr(thread, 2);
				SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
							    THREAD_FLAG_REQUEST_MASK);
				thread->status = (thread->status >> 16) << 16 |
						     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
				SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 8832);
				thread->currentCpuId = -1;
				wake = 1;
				goto suspended;
			}
			boostToStop(thread, priority);
			cpu->priority = priority;
			if (searchHighestPriority() < priority) {
				SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8820);
				wake = 1;
				cpu->nextRunningThread = 0;
			}
			goto unlock;
		}
		if (thread->status & SCE_KERNEL_THREAD_STATUS_READY) {
			if (!(request & THREAD_FLAG_REQUEST_SUSPEND)) {
				if (thread->userFrame == NULL)
					setupThreadExit(thread, request);
				removeReady(thread, thread->currentPriority);
				thread->currentPriority = priority;
				if (!cpuLocalPriority(priority))
					goto distribute;
				addReady(thread);
				if (priority < cpu->priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8898);
					wake = 1;
					cpu->nextRunningThread = 0;
				}
				goto unlock;
			}
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
			if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
				SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
							    THREAD_FLAG_REQUEST_MASK);
				removeReady(thread, thread->currentPriority);
				thread->status = (thread->status >> 16) << 16 |
						     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
				SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId() ||
						     thread->currentCpuId == THREAD_CPU_NONE, 8880);
				goto offCpu;
			}
			removeReady(thread, thread->currentPriority);
			thread->currentPriority = priority;
			thread->savedCpuAffinityMask = thread->currentCpuAffinityMask;
			thread->currentCpuAffinityMask |= CPU_MASK_ALL;
			if (!cpuLocalPriority(priority))
				goto distribute;
			addReady(thread);
			if (priority < cpu->priority) {
				SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8869);
				wake = 1;
				cpu->nextRunningThread = 0;
			}
			goto unlock;
		}
		/* on standby */
		if (request & THREAD_FLAG_REQUEST_SUSPEND) {
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
			if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
				SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
							    THREAD_FLAG_REQUEST_MASK);
				unlistStandby(cpu, thread);
				thread->status = (thread->status >> 16) << 16 |
						     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
				SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 8936);
				goto offCpu;
			}
			thread->currentPriority = priority;
			thread->savedCpuAffinityMask = thread->currentCpuAffinityMask;
			thread->currentCpuAffinityMask |= CPU_MASK_ALL;
			if (cpuLocalPriority(priority)) {
				if (priority < cpu->priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8921);
					wake = 1;
					cpu->nextRunningThread = 0;
				}
				goto unlock;
			}
		} else {
			if (thread->userFrame == NULL)
				setupThreadExit(thread, request);
			thread->currentPriority = priority;
			if (cpuLocalPriority(priority)) {
				if (priority < cpu->priority) {
					SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 8956);
					wake = 1;
					cpu->nextRunningThread = 0;
				}
				goto unlock;
			}
		}
		unlistStandby(cpu, thread);
		goto distribute;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_STAGNANT) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, request);
		unlistThread(thread);
		if (!(request & THREAD_FLAG_REQUEST_SUSPEND))
			goto boostAndRun;
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
					   THREAD_FLAG_SUSPEND_COUNTED);
		if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_REQUEST_MASK);
			SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 8990);
			thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_SUSPENDED;
			goto suspended;
		}
		boostToStop(thread, priority);
		target = selectExecCpu(thread);
		if ((unsigned int)target > 3) {
			SCE_DBG_PRINTF(0, 8983, " k: can not select cpu(0x%08X)\n", target);
			SCE_PANIC(SCE_LINE(8979, 8984));
		}
		wake = distributeThread(target, thread);
		goto unlock;
	}
	if (state & THREAD_STATE_OFF_CPU) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, request);
		unlistThread(thread);
		if (!(request & THREAD_FLAG_REQUEST_SUSPEND))
			goto boostAndRun;
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
					   THREAD_FLAG_SUSPEND_COUNTED);
		if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_REQUEST_MASK);
			thread->status &= THREAD_STATE_DEBUG_SUSPENDED | 0xffff;
			SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 9043);
			thread->status = (thread->status & 0xffff0000) | SCE_KERNEL_THREAD_STATUS_SUSPENDED;
			listSuspended(thread);
			goto wakeSuspendWaiters;
		}
		boostToStop(thread, priority);
		wake = distributeThread(selectExecCpu(thread), thread);
		goto unlock;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_WAITING) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, request);
		if (suspend) {
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
			error = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
		} else {
			error = SCE_KERNEL_ERROR_THREAD_STOPPED;
		}
		waitId = thread->waitId;
		if (waitId == WAIT_ID_MULTIPLE_EVENTS) {
			if (thread->flags & THREAD_FLAG_WAIT_MARK) {
				/* in a syscall: the wait ends with the error */
				boostToStop(thread, priority);
				SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
							   THREAD_FLAG_SIGNAL_DEFERRED);
				SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
							    THREAD_FLAG_RESUMED_WAIT);
				wake |= wakeupAndDistributeThread(thread, 0, error);
				goto unlock;
			}
			/* each event of the WME: cut its node, or requeue it by the new priority */
			wme = (ThreadmgrWaitMultipleEvents *)thread->next;
			thread->currentPriority = priority;
			for (i = 0; i < wme->count; i++) {
				if (wme->nodes[i].next == &wme->nodes[i])
					continue;
				eventNode = (ThreadmgrEventWaitNode *)((uintptr_t)wme->nodes[i].next -
					offsetof(ThreadmgrEventWaitNode, wmeLink));
				eventId = eventNode->eventId;
				if (SceSysmemForDriver_72A98D17(eventId,
					s_threadmgrRuntime.manager->eventClass,
					(SceObjectBase **)&event) < 0)
					SCE_PANIC(SCE_LINE(9110, 9115));
				if (SceCpuForDriver_5AC9D394(&event->lock) < 0) {
					SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						request | THREAD_FLAG_SUSPEND_COUNTED);
					thread->currentPriority =
						thread->basePriority < thread->boostPriority ?
						thread->basePriority : thread->boostPriority;
					sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					sceGUIDReleaseObject(eventId);
					goto retry;
				}
				if (event->attr & EVENT_ATTR_UNINTERRUPTIBLE) {
					left = unlinkEventWaitNode(eventNode, wme);
					if (wme->busy == 0 && thread->waitId != WAIT_ID_MULTIPLE_EVENTS) {
						ret = releaseEventWaiter(event, eventNode, thread);
					} else {
						addEventWaitResult(wme, eventNode->eventId, error,
								   EVENT_DELETED_PATTERN, 0);
						markEventReady(wme, eventNode->index);
						if (thread->waitId == WAIT_ID_MULTIPLE_EVENTS &&
						    (left == 0 || wme->mode == WME_MODE_ANY))
							wake |= wakeupAndDistributeThread(thread,
								EVENT_WAIT_QUEUE(event), 0);
						else
							ret = releaseEventWaiter(event, eventNode, thread);
					}
					if (freeEventWaitNode(thread->processId, eventNode) < 0)
						SCE_PANIC(SCE_LINE(9169, 9174));
				} else if (event->attr & EVENT_ATTR_PRIORITY) {
					/* requeued in front of the first waiter of a lower priority */
					eventNode->next->prev = eventNode->prev;
					eventNode->prev->next = eventNode->next;
					head = (struct ThreadmgrWaitNode *)EVENT_WAIT_QUEUE(event);
					for (node = head->next; node != head; node = node->next) {
						waiter = EventWaiterIsThread(node) ?
							(struct ThreadCB *)node :
							(struct ThreadCB *)(node->owner & ~3);
						SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC,
								     9190);
						if (thread->currentPriority < waiter->currentPriority)
							break;
					}
					eventNode->next = node;
					eventNode->prev = node->prev;
					node->prev = (struct ThreadmgrWaitNode *)eventNode;
					eventNode->prev->next = (struct ThreadmgrWaitNode *)eventNode;
				}
				sceKernelSpinlockLowUnlock(&event->lock);
				sceGUIDReleaseObject(eventNode->eventId);
			}
			thread->savedCpuAffinityMask = thread->currentCpuAffinityMask;
			thread->currentCpuAffinityMask |= CPU_MASK_ALL;
			goto unlock;
		}
		ret = SceSysmemForDriver_72A98D17(waitId,
			s_threadmgrRuntime.manager->waitableClass,
			(SceObjectBase **)&queue);
		if (ret < 0) {
			/* only a delay */
			SCE_DBG_ASSERT_CHECK(thread->delayId != -1, 9232);
			waitId = -1;
			queue = NULL;
			ret = 0;
		} else {
			queue = (struct ThreadmgrWaitQueue *)((uintptr_t)queue + 0x10);
			if (SceCpuForDriver_5AC9D394(&queue->lock) < 0) {
				SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
					request | THREAD_FLAG_SUSPEND_COUNTED);
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				sceGUIDReleaseObject(waitId);
				goto retry;
			}
		}
		status = thread->flags;
		boostToStop(thread, priority);
		if ((status & THREAD_FLAG_WAIT_MARK) ||
		    (queue != NULL && (queue->attributes & WAIT_ATTR_UNINTERRUPTIBLE))) {
			/* the wait ends with the error */
			SCE_DBG_ASSERT_CHECK(!(status & THREAD_FLAG_COND_RETRY_MASK), 9242);
			if (suspend) {
				switch (thread->waitType) {
				case WAIT_TYPE_COND:
					SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
								   THREAD_FLAG_COND_KEEP_MUTEX);
					break;
				case WAIT_TYPE_MUTEX_2:
					SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
								   THREAD_FLAG_COND_RELOCK);
					break;
				case WAIT_TYPE_LW_COND:
					SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
								   THREAD_FLAG_LW_COND_KEEP_MUTEX);
					break;
				case WAIT_TYPE_LW_MUTEX_2:
					SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
								   THREAD_FLAG_LW_COND_RELOCK);
					break;
				}
			}
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SIGNAL_DEFERRED);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
						    THREAD_FLAG_RESUMED_WAIT);
			wake |= wakeupAndDistributeThread(thread, (uintptr_t)queue, error);
		} else if (queue != NULL && (queue->attributes & WAIT_ATTR_PRIORITY)) {
			((struct ThreadmgrWaitNode *)thread)->next->prev =
				((struct ThreadmgrWaitNode *)thread)->prev;
			((struct ThreadmgrWaitNode *)thread)->prev->next =
				((struct ThreadmgrWaitNode *)thread)->next;
			insertPriorityQueue((uintptr_t)queue, thread);
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (waitId != -1) {
			wake = revalidateWaitState((uintptr_t)queue);
			sceKernelSpinlockLowUnlock(&queue->lock);
			sceGUIDReleaseObject(waitId);
		}
		goto release;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_DORMANT) {
		if (!(request & THREAD_FLAG_REQUEST_EXIT_DELETE)) {
			ret = SCE_KERNEL_ERROR_DORMANT;
			goto unlock;
		}
		/* deleting a dormant thread: its UIDs close now */
		SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 9288);
		thread->currentCpuId = -1;
		thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_DELETED;
		SceCpuForDriver_BF82DEB2(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		((struct ThreadmgrWaitNode *)thread)->next->prev =
			((struct ThreadmgrWaitNode *)thread)->prev;
		((struct ThreadmgrWaitNode *)thread)->prev->next =
			((struct ThreadmgrWaitNode *)thread)->next;
		head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->closedNext;
		((struct ThreadmgrWaitNode *)thread)->next = head;
		((struct ThreadmgrWaitNode *)thread)->prev = head->prev;
		head->prev = (struct ThreadmgrWaitNode *)thread;
		((struct ThreadmgrWaitNode *)thread)->prev->next = (struct ThreadmgrWaitNode *)thread;
		sceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		if (thread->userThreadId != -1) {
			ret = SceSysmemForDriver_84A4AF5E(thread->processId, thread->userThreadId);
			if (ret < 0) {
				SCE_DBG_ASSERT_LEVEL(0, 0, 9306);
				goto unlock;
			}
		}
		ret = sceGUIDClose(thread->threadId);
		goto unlock;
	}
	if (state & SCE_KERNEL_THREAD_STATUS_DELETED) {
		ret = SCE_KERNEL_ERROR_DELETED;
		goto unlock;
	}
	if (!(state & SCE_KERNEL_THREAD_STATUS_SUSPENDED)) {
		SCE_DBG_PRINTF(0, 9375, "error: unexpected thread status(%p, 0x%08x)\n", thread,
			       state);
		SCE_PANIC(SCE_LINE(9371, 9376));
	}
	/* suspended */
	if (suspend) {
		if (thread->flags & THREAD_FLAG_SUSPEND_COUNTED) {
			ret = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
		} else {
			ret = 0;
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SUSPEND_COUNTED);
		}
		goto unlock;
	}
	if (SceCpuForDriver_5AC9D394(
		&thread->process->exception->lock) < 0) {
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (spin = 10; spin != 0; spin--)
			;
		goto retry;
	}
	if (thread->flags & THREAD_FLAG_EXCEPTION) {
		entry = thread->exceptionEntry;
		entry->next->prev = entry->prev;
		entry->prev->next = entry->next;
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_EXCEPTION);
	}
	sceKernelSpinlockLowUnlock(&thread->process->exception->lock);
	SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, request);
	unlistThread(thread);
	boostToStop(thread, priority);
	thread->status &= 0xffff;
	if (thread->userFrame == NULL)
		setupThreadExit(thread, request);
	goto distribute;

boostAndRun:
	boostToStop(thread, priority);
	if (thread->userFrame == NULL)
		setupThreadExit(thread, request);
distribute:
	wake = distributeThread(selectExecCpu(thread), thread);
	goto unlock;

offCpu:
	thread->currentCpuId = -1;
suspended:
	listSuspended(thread);
wakeSuspendWaiters:
	/* the threads waiting for it to be suspended */
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	SceCpuForDriver_BF82DEB2(&object->waitLock);
	head = (struct ThreadmgrWaitNode *)&object->waitNext;
	for (node = head->next; node != head; node = next) {
		next = node->next;
		SCE_DBG_ASSERT_CHECK(((struct ThreadCB *)node)->magic ==
				     THREAD_CB_MAGIC, 9391);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK((struct ThreadCB *)node));
		if (((struct ThreadCB *)node)->waitType == WAIT_TYPE_THREAD_SUSPEND)
			wake |= wakeupAndDistributeThread((struct ThreadCB *)node,
							  (uintptr_t)head, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK((struct ThreadCB *)node));
	}
	sceKernelSpinlockLowUnlock(&object->waitLock);
	goto release;

unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
release:
	sceGUIDReleaseObject(threadId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/*
 * SceKernelThreadMgr (threadmgr.c): taking a process's threads off this
 * CPU (processmgr suspends a process CPU by CPU). Its ready threads leave
 * this CPU's queues and the global ones, and its running thread the CPU;
 * all go on the manager's stagnant list.
 */

#define LOCKED_RETRY_DELAY		10

/* the thread goes on the manager's stagnant list */
static inline __attribute__((always_inline)) void listStagnantThread(struct ThreadCB *thread)
{
	SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->stagnantNext;
	thread->prev = s_threadmgrRuntime.manager->stagnantPrev;
	s_threadmgrRuntime.manager->stagnantPrev = (uintptr_t)thread;
	((struct ThreadCB *)thread->prev)->next = (uintptr_t)thread;
	sceKernelSpinlockLowUnlock(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/* sceKernelCpuScanThreadForProcessmgr: processId's threads leave this CPU */
int sceKernelCpuScanThreadForProcessmgr(SceUID processId)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *current = currentThreadContext(), *thread, *next;
	struct ThreadCB *head;
	SceKernelIntrStatus intr = SceCpuForDriver_821FC0EE();
	uint32_t bits, priority, word;
	volatile unsigned int delay;
	bool retry;

	SCE_DBG_ASSERT_CHECK((intr >> 7) & 1, 10065);
	if (current == NULL)
		current = (struct ThreadCB *)currentCpuState()->runningThread;
	else
		SCE_DBG_ASSERT_CHECK(current->process->threadsStopped == 0,
				     10068);
	moveStandbyToReady();

	/* this CPU's queues */
	for (word = 0; word < 4; word++) {
		bits = cpu->readyBitmap[word];
		while (bits != 0) {
			priority = __builtin_clz(bits) + word * 32;
			head = (struct ThreadCB *)&cpu->readyQueue[priority];
			for (thread = (struct ThreadCB *)head->next;
			     thread != head; thread = next) {
				next = (struct ThreadCB *)thread->next;
				SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 10087);
				SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
				if (thread->processId == processId) {
					SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_REQUEST_MASK),
							     10093);
					removeReady((struct ThreadCB *)thread, priority);
					thread->status = (thread->status & THREAD_STATE_HIGH_MASK) |
							     SCE_KERNEL_THREAD_STATUS_STAGNANT;
					SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)sceKernelCpuId(), 10098);
					thread->currentCpuId = -1;
					listStagnantThread(thread);
				}
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			}
			bits &= ~(0x80000000u >> (priority % 32));
		}
	}

	/* the global queues, from a thread (a locked thread: all over again shortly) */
	if (currentThreadContext() != NULL) {
		do {
			retry = false;
			SceCpuForDriver_BF82DEB2(
				&s_threadmgrRuntime.manager->globalReadyLock);
			for (word = 0; word < 4 && !retry; word++) {
				bits = s_threadmgrRuntime.manager->globalReadyBitmap[word];
				while (bits != 0 && !retry) {
					priority = __builtin_clz(bits) + PRIORITY_SHARED_LOW + word * 32;
					head = (struct ThreadCB *)&s_threadmgrRuntime.
						manager->globalReadyQueue[priority - PRIORITY_SHARED_LOW];
					for (thread = (struct ThreadCB *)head->next;
					     thread != head; thread = next) {
						next = (struct ThreadCB *)thread->next;
						SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC,
								     10127);
						if (SceCpuForDriver_5AC9D394(THREAD_LOCK(thread)) < 0) {
							retry = true;
							break;
						}
						SCE_DBG_ASSERT_CHECK(
							s_threadmgrRuntime.manager->globalReadyLock != 0 ||
							thread->lock != 0, 10134);
						if (thread->processId == processId) {
							removeCommonRaw((struct ThreadCB *)thread,
									priority);
							thread->status =
								(thread->status & THREAD_STATE_HIGH_MASK) |
								SCE_KERNEL_THREAD_STATUS_STAGNANT;
							SCE_DBG_ASSERT_CHECK(thread->currentCpuId == CPU_ID_GLOBAL, 10140);
							thread->currentCpuId = -1;
							listStagnantThread(thread);
						}
						sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					}
					bits &= ~(0x80000000u >> (priority % 32));
				}
			}
			sceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->globalReadyLock);
			if (retry) {
				for (delay = LOCKED_RETRY_DELAY; delay != 0; delay--)
					;
			}
		} while (retry);
	}

	/* the running thread */
	if (current != NULL) {
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(current));
		if (current->processId == processId && (current->status & SCE_KERNEL_THREAD_STATUS_RUNNING)) {
			runningThreadPreemptByIntr((struct ThreadCB *)current,
						   PREEMPT_INTR);
			SCE_DBG_ASSERT_CHECK(current->currentCpuId == (int)sceKernelCpuId(), 10199);
			current->currentCpuId = -1;
			current->status = (current->status & THREAD_STATE_HIGH_MASK) |
					      SCE_KERNEL_THREAD_STATUS_STAGNANT;
			listStagnantThread(current);
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(current));
	}
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/*
 * SceKernelThreadMgr: the running thread's VFP exception mask, kept on the
 * thread and in the word the VFP exception handler reads (VBAR+0x24).
 */

#define VFP_EXCEPTION_ILLEGAL		0xf7ffff60	/* bits a caller may not change */
#define CHANGE_VFP_FUNC			"sceKernelChangeThreadVfpException"

/* sceKernelChangeThreadVfpException: clear then set; returns the old mask */
uint32_t sceKernelChangeThreadVfpException(uint32_t clear, uint32_t set)
{
	struct ThreadCB *thread = currentThreadContext();
	volatile uint32_t *word;
	SceKernelIntrStatus intr;
	uint32_t saved, previous, value;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 10725, CHANGE_VFP_FUNC, "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (((clear | set) & VFP_EXCEPTION_ILLEGAL) || (clear & set)) {
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	SCE_DBG_ASSERT_CHECK_FUNC(thread != NULL, 10745, CHANGE_VFP_FUNC);
	intr = SceCpuForDriver_821FC0EE();
	word = currentVfpException();
	SCE_DBG_ASSERT_CHECK_FUNC(word != NULL, 10750, CHANGE_VFP_FUNC);
	previous = thread->vfpException;
	value = set | (previous & ~clear);
	thread->vfpException = value;
	*word = value;
	__asm__ volatile("dsb sy" : : : "memory");
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return previous;
}

/* sceKernelGetCurrentThreadVfpException: the running thread's mask */
uint32_t sceKernelGetCurrentThreadVfpException(void)
{
	struct ThreadCB *thread = currentThreadContext();
	uint32_t saved, value;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 10777, "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	SCE_DBG_ASSERT_CHECK(thread != NULL, 10782);
	SCE_DBG_ASSERT_CHECK(thread->vfpException == *currentVfpException(), 10783);
	value = thread->vfpException;
	ThreadmgrSetUserRoWord(saved);
	return value;
}

/*
 * SceKernelThreadMgr (threadmgr.c): CPU exceptions of threads. The
 * exception handlers (excp_handlers.S) save the faulting thread's registers
 * and call these. A user thread whose process takes exceptions has the
 * exception handed to its process's exception object (the VM's monitor
 * waits there) and is suspended; anything else is debug-suspended.
 */

/* what the handler is told about the exception */
#define EXCP_KIND_PREFETCH_ABORT	0x100
#define EXCP_KIND_DATA_ABORT		0x200
#define EXCP_KIND_UNDEFINED		0x400

/* what the exception handler does next */
#define EXCP_ACTION_DEFAULT		1	/* PLACEHOLDER: names; the excpmgr chain's codes */
#define EXCP_ACTION_KERNEL		3
#define EXCP_ACTION_SUSPENDED		4

/* the kinds the VM is told of (ThreadmgrExceptionInfo.kind) */
#define VM_EXCEPTION_DATA_ABORT		0x001
#define VM_EXCEPTION_PREFETCH_ABORT	0x100
#define VM_EXCEPTION_DEBUG		0x200

/* thread->handedExceptions: the kinds this thread hands over (its option's handedExceptions) */
#define THREAD_EXCP_DATA_ABORT		0x001
#define THREAD_EXCP_PREFETCH_ABORT	0x100
#define THREAD_EXCP_DEBUG		0x200

#define CPSR_MODE_MASK			0x1f
#define CPSR_MODE_USR			0x10
#define CPSR_MODE_SVC			0x13
#define FSR_STATUS_MASK			0x140f
#define DFSR_FIRST			5	/* translation, domain and permission faults */
#define DFSR_COUNT			11
#define DFSR_HANDED			0x555
#define IFSR_FIRST			2	/* a debug event, then as for data */
#define IFSR_COUNT			14
#define IFSR_HANDED			0x2aa8
#define IFSR_DEBUG			0x0001
#define PREEMPT_REASON_EXCEPTION	8	/* PLACEHOLDER: name */
#define CPU_STOP_ALL_STOPPED		3	/* each of the other CPUs counted itself in */
#define PROCESS_DEBUG_SUSPEND_CPU	0	/* the processDebugSuspendHook kind */
#define SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED	0x80028030	/* PLACEHOLDER: name */

/*
 * threadMgrExcpHandlerCfunc_0: an abort or undefined instruction of the
 * running thread (excpKind EXCP_KIND_*), its registers saved. Returns the
 * handler's next step.
 */
int threadMgrExcpHandlerCfunc_0(struct ThreadCB *thread, uint32_t excpKind)
{
	struct ThreadmgrThreadObject *object = THREAD_OBJECT(thread);
	struct ThreadCB *waiter, *node, *next;
	struct ThreadmgrExceptionEntry *entry;
	struct ThreadmgrException *exception;
	ThreadmgrExceptionInfo *info;
	uint32_t kind, status, far;
	int action;
	SceUID uid;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 10817);
	SCE_DBG_ASSERT_CHECK(thread != NULL, 10819);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	SCE_DBG_ASSERT_CHECK(thread->threadId != currentCpuState()->idleThread->threadId, 10825);
	SCE_DBG_ASSERT_PRINTF((thread->status &
			       (SCE_KERNEL_THREAD_STATUS_RUNNING | THREAD_STATE_OFF_CPU)) != 0,
			      10832, "status=0x%08X\n", thread->status);

	/* is it the process's to handle? */
	if ((thread->pRegisterContext->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USR) {
		action = (thread->pRegisterContext->cpsr & CPSR_MODE_MASK) == CPSR_MODE_SVC ?
			EXCP_ACTION_DEFAULT : EXCP_ACTION_KERNEL;
		goto suspend;
	}
	action = EXCP_ACTION_DEFAULT;
	if ((thread->currentAttr & THREAD_ATTR_IN_SUSPENDED) ||
	    !(thread->currentAttr & THREAD_ATTR_EXCEPTION_LIST))
		goto suspend;
	switch (excpKind) {
	case EXCP_KIND_DATA_ABORT:
		status = (thread->pRegisterContext->dfsr & FSR_STATUS_MASK) - DFSR_FIRST;
		if (status < DFSR_COUNT && ((1 << status) & DFSR_HANDED) &&
		    (thread->handedExceptions & THREAD_EXCP_DATA_ABORT)) {
			kind = VM_EXCEPTION_DATA_ABORT;
			goto handed;
		}
		goto suspend;
	case EXCP_KIND_UNDEFINED:
		goto suspend;
	case EXCP_KIND_PREFETCH_ABORT:
		break;
	default:
		SCE_PANIC(SCE_LINE(10897, 10902));
	}
	status = (thread->pRegisterContext->ifsr & FSR_STATUS_MASK) - IFSR_FIRST;
	if (status >= IFSR_COUNT)
		goto suspend;
	if ((1 << status) & IFSR_HANDED) {
		if (!(thread->handedExceptions & THREAD_EXCP_PREFETCH_ABORT))
			goto suspend;
		kind = VM_EXCEPTION_PREFETCH_ABORT;
	} else if (((1 << status) & IFSR_DEBUG) && (thread->handedExceptions & THREAD_EXCP_DEBUG)) {
		kind = VM_EXCEPTION_DEBUG;
	} else {
		goto suspend;
	}
handed:
	runningThreadPreemptByIntr(thread, PREEMPT_REASON_EXCEPTION);
	if (!(thread->currentAttr & THREAD_ATTR_IN_SUSPENDED)) {
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		goto deliver;
	}
	action = EXCP_ACTION_SUSPENDED;
	goto debug_suspend;
suspend:
	kind = 0;
	runningThreadPreemptByIntr(thread, PREEMPT_REASON_EXCEPTION);
debug_suspend:
	if (thread->debugSuspendReason == 0) {
		thread->debugSuspendReason = excpKind;
		thread->currentCpuId = -1;
		thread->status = ((thread->status & 0x7fff) << 16) |
				     THREAD_STATE_DEBUG_SUSPENDED | THREAD_STATE_OFF_CPU;
		SceCpuForDriver_BF82DEB2(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		thread->next = (uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext;
		thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
		s_threadmgrRuntime.manager->debugSuspendedPrev = (uintptr_t)thread;
		*(uintptr_t *)thread->prev = (uintptr_t)thread;
		sceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	if (action != EXCP_ACTION_SUSPENDED)
		return action;

deliver:
	if (thread->currentAttr & THREAD_ATTR_IN_SUSPENDED)
		return EXCP_ACTION_SUSPENDED;
	/* VM-suspended, the exception goes to the process's exception object */
	SceCpuForDriver_BF82DEB2(&thread->process->exception->lock);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	uid = thread->userThreadId;
	if (thread->debugSuspendReason != 0) {
		thread->status &= THREAD_STATE_KEEP_SUSPEND;
		SceCpuForDriver_BF82DEB2(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		((struct ThreadCB *)thread->next)->prev = thread->prev;
		((struct ThreadCB *)thread->prev)->next = thread->next;
		sceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
	}
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == sceKernelCpuId(), 10967);
	thread->currentCpuId = -1;
	thread->vmSuspendCount++;
	thread->status = (thread->status & ~SCE_KERNEL_THREAD_STATUS_MASK) |
			     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
	SceCpuForDriver_BF82DEB2(&s_threadmgrRuntime.manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->suspendedNext;
	thread->prev = s_threadmgrRuntime.manager->suspendedPrev;
	s_threadmgrRuntime.manager->suspendedPrev = (uintptr_t)thread;
	*(uintptr_t *)thread->prev = (uintptr_t)thread;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	exception = thread->process->exception;
	waiter = (struct ThreadCB *)exception->next;
	if (waiter == (struct ThreadCB *)&exception->next ||
	    !(kind & waiter->waitArg0)) {
		/* nobody waits for it: it is left pending */
		entry = thread->exceptionEntry;
		entry->value = uid;
		SCE_DBG_ASSERT_CHECK(kind != 0, 10988);
		entry->kind = kind;
		if (kind == VM_EXCEPTION_DATA_ABORT)
			entry->value2 = thread->pRegisterContext->dfar;
		SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_EXCEPTION), 10998);
		exception = thread->process->exception;
		entry->prev = exception->pendingPrev;
		entry->next = (struct ThreadmgrExceptionEntry *)&exception->pendingNext;
		exception->pendingPrev = entry;
		entry->prev->next = entry;
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_EXCEPTION);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	} else {
		/* the waiter takes it */
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 11004);
		far = thread->pRegisterContext->dfar;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 11015);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(waiter));
		info = (ThreadmgrExceptionInfo *)waiter->waitArg1;
		info->value = uid;
		info->kind = kind;
		if (kind == VM_EXCEPTION_DATA_ABORT)
			info->value2 = far;
		wakeupAndDistributeThread(waiter,
			(uintptr_t)&thread->process->exception->next, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	sceKernelSpinlockLowUnlock(&thread->process->exception->lock);

	/* the threads waiting for it to be suspended */
	SceCpuForDriver_BF82DEB2(&object->waitLock);
	for (node = (struct ThreadCB *)object->waitNext;
	     node != (struct ThreadCB *)&object->waitNext; node = next) {
		next = (struct ThreadCB *)node->next;
		SCE_DBG_ASSERT_CHECK(node->magic == THREAD_CB_MAGIC, 11042);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(node));
		if (node->waitType == WAIT_TYPE_THREAD_SUSPEND)
			wakeupAndDistributeThread(node, (uintptr_t)&object->waitNext, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(node));
	}
	sceKernelSpinlockLowUnlock(&object->waitLock);
	return EXCP_ACTION_SUSPENDED;
}

/* threadMgrExcpHandlerCfunc_2: the debugger lets the thread go on */
void threadMgrExcpHandlerCfunc_2(SceUID threadId)
{
	int reason, ret;

	reason = SceThreadmgrForDriver_A0B1AB21(threadId);
	if (reason <= 0)
		return;
	ret = sceKernelDebugResumeThread(threadId, reason);
	if (ret < 0 && ret != (int)SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED)
		SCE_PANIC(SCE_LINE(11066, 11071));
}

/*
 * threadMgrExcpHandlerCfunc_3: the debugger stops the thread's process.
 * The first exception of a process debug-suspends it and reports its
 * threads, the other CPUs held still meanwhile.
 */
void threadMgrExcpHandlerCfunc_3(struct ThreadCB *thread)
{
	struct ThreadCB *other;
	ThreadmgrThreadInfoForDebugger info;
	ThreadmgrProcessDebugSuspendHook hook;
	ListNode *link;
	int stopped, ret;

	SCE_DBG_ASSERT_FAIL(11113);
	if (SceCpuForDriver_CDA96E81(
		(SceInt32 *)&thread->process->abortRequested, 0, 1) != 0)
		return;
	stopped = SceCpuForDriver_CDA96E81(
		(SceInt32 *)&s_threadmgrRuntime.manager->cpuStopOwner, 0, 1);
	if (stopped == 0) {
		s_threadmgrRuntime.manager->cpuStopState = CPU_STOP_REQUEST;
		sceKernelGenerateSoftIntr(SGI_STOP_CPUS, 0,
				     ~(1 << sceKernelCpuId()) & CPU_MASK_ALL);
	}
	sceKernelCpuResumeIntr(0);
	ret = SceSysrootForKernel_1247A825(thread->processId, PROCESS_DEBUG_SUSPEND_EXCEPTION);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 11135, "(): sceKernelProcessDebugSuspend(0x%08X)=0x%08X\n",
			       thread->processId, ret);
	SceCpuForDriver_D32ACE9E(
		&thread->process->lock);
	for (link = thread->process->threads.next; link != &thread->process->threads;
	     link = link->next) {
		other = THREAD_FROM_PROCESS_LINK(link);
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		ret = SceThreadmgrForDriver_DD8D9429(other->threadId, 0, &info);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(11148);
			continue;
		}
		ret = SceSysrootForDriver_DABD4A47(&info);
		if (ret < 0)
			SCE_DBG_ASSERT_FAIL(11156);
	}
	sceKernelSpinlockLowUnlock(&thread->process->lock);
	if (stopped == 0) {
		/* the other CPUs go on once all of them stopped */
		s_threadmgrRuntime.manager->cpuStopState = 0;
		__asm__ volatile("dsb" : : : "memory");
		while (s_threadmgrRuntime.manager->cpuStopState != CPU_STOP_ALL_STOPPED)
			__asm__ volatile("wfe");
		s_threadmgrRuntime.manager->cpuStopState = 0;
		s_threadmgrRuntime.manager->cpuStopOwner = 0;
	}
	hook = (ThreadmgrProcessDebugSuspendHook)s_threadmgrRuntime.manager->processDebugSuspendHook;
	if (ret >= 0 && hook != NULL)
		hook(thread->processId, thread->threadId, PROCESS_DEBUG_SUSPEND_CPU,
		     s_threadmgrRuntime.manager->processDebugSuspendArg);
}
