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

#include <string.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "sce/sysroot.h"
#include "sce/sysroot_threadmgr.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "timer.h"
#include "uid_waitable.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

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

/* excpmgr's exception kinds and handler slots */
#define EXCP_UNDEFINED			1
#define EXCP_PREFETCH_ABORT		3
#define EXCP_DATA_ABORT			4
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

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern void sceKernelSysrootCorelockUnlock(void);
extern int sceKernelSysrootSetModulePrivate(unsigned int slot, void *value);
extern int SceSysrootForKernel_26458702(const SysrootThreadmgrOps *ops);
extern void *ksceKernelAllocHeapMemory(SceUID heap, SceSize size);
extern void *ksceKernelAllocHeapMemory2(SceUID pid, SceSize size, SceAllocOpt *opt);
extern SceClass *ksceKernelGetUIDClass(void);
extern int SceSysmemForKernel_BF275C5A(SceUID uid, SceClass *cls, const char *name,
				       void *object);		/* a GUID of a fixed value */
extern SceUID SceSysmemForKernel_40870E74(const char *name, SceSize size, SceSize alignment);
extern int ksceKernelGetMemBlockMappedBase(SceUID uid, void **base);
extern void *SceSysmemForKernel_FA6C098C(SceUID uid);
extern SceUID SceSysrootForKernel_7334F1E8(SceUID pid);
extern int ksceKernelSysrootGetPUIDEntryHeap(SceUID pid, void **heap);
extern int ksceKernelCheckDipsw(unsigned int bit);
extern int ksceKernelRegisterIntrHandler(int code, const char *name, int type, void *handler,
					 void *arg, int priority, int targetCpu, void *opt);
extern int SceIntrmgrForKernel_9CB39268(void *handler);	/* before an interrupt handler */
extern int SceIntrmgrForKernel_46F04D82(void *handler);
extern int SceIntrmgrForKernel_764F140A(void *handler);	/* the next context after an interrupt */
extern int SceIntrmgrForKernel_04E635AA(struct ThreadmgrCpuStates *states);
extern int SceIntrmgrForKernel_E3B670BF(void *handler);	/* a thread's debug suspension */
extern int SceIntrmgrForKernel_BCD3BB05(void);
extern void SceIntrmgrForKernel_08FDB0CE(struct ThreadRegisterContext *context, uint32_t flags,
					 uintptr_t vfp) __attribute__((noreturn));
extern int ksceExcpmgrRegisterHandler(int kind, int slot, void *handler);
extern int SceKernelSuspendForDriver_CE7A2207(int order, ThreadmgrSuspendHandler handler,
					      void *arg);	/* before a power suspend */
extern int SceKernelSuspendForDriver_105C5752(int order, ThreadmgrSuspendHandler handler,
					      void *arg);	/* after a resume */

/* excp_handlers.S */
void poisonVfpRegisters(void);
void idleThreadEntry(void);
extern uint8_t threadmgrAbortHandler[], threadmgrPrefetchHandler[], threadmgrUndefHandler[];
extern uint8_t threadmgrDebugAbortHandler[], threadmgrDebugPrefetchHandler[];
extern uint8_t threadmgrDebugUndefHandler[];

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
int SceThreadmgrForKernel_BB56D42F(void);
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
int sceKernelExitDeleteThread(int status);
int createGlobalWorkQueue(void);
int startMsgPipeAfterProcess(void);
int SceThreadmgrForDriver_B645C7EF(uint32_t size, void (*destructor)(void *, uintptr_t),
				   uintptr_t argument);
SceUID ksceKernelGetThreadId(void);
SceUID sceKernelGetProcessId(void);
int SceThreadmgrForDriver_332E127C(void);
int sceKernelIsUserModeThread(void);
int SceThreadmgrForDriver_AB977C72(SceUInt32 *attributes);
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
	.getThreadId = ksceKernelGetThreadId,
	.isUserModeThread = sceKernelIsUserModeThread,
	.getThreadAccessLevel = SceThreadmgrForDriver_332E127C,
	.getCurrentThreadAttr = (int (*)(uint32_t *))SceThreadmgrForDriver_AB977C72,
	.getProcessId = sceKernelGetProcessId,
	.getCurrentProcessCB = (uint32_t (*)(void))getCurrentProcessCB,
	.getCurrentAddressSpaceCB = getCurrentAddressSpaceCB,
	.getCurrentUIDEntryHeapCB = getCurrentUIDEntryHeapCB,
	.returnFromExcpToThread = (int (*)(void))returnFromExcpToThread,
	.threadMgrStartAfterProcess = sceKernelThreadMgrStartAfterProcess,
	.trapThreadAfterSyscall = (int (*)(void))sceKernelTrapThreadAfterSyscall,
};

/* SceThreadmgrSGILow: this CPU holds still while another one stops the rest */
static int stopCpuIntrHandler(int code, void *arg)
{
	while (s_threadmgrRuntime.manager->cpuStopState == CPU_STOP_REQUEST)
		__asm__ volatile("wfe" : : : "memory");
	ksceKernelAtomicAddAndGet32((SceInt32 *)&s_threadmgrRuntime.manager->cpuStopState, 1);
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

	cpuId = ksceKernelCpuId();
	poisonVfpRegisters();
	if (ksceKernelCheckDipsw(DIPSW_THBP)) {
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
	block = ksceKernelAllocPartitionMemBlock(PARTITION_KERNEL, "SceThreadMgrCBBlock",
						 MEMBLOCK_TYPE_KERNEL, CB_BLOCK_SIZE, NULL);
	if (block < 0)
		SCE_PANIC(365);
	ksceKernelGetMemBlockVBase(block, (void **)&s_threadmgrRuntime.manager);
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
	manager->heapId = ksceKernelCreateHeap("SceKernelThreadMgrCBHeap", CB_HEAP_SIZE, NULL);
	if (s_threadmgrRuntime.manager->heapId < 0)
		SCE_PANIC(415);

	/* the UID classes under every object: cache, waitable, thread */
	cacheClass = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
					       CACHE_CLASS_SIZE);
	if (cacheClass == NULL)
		SCE_PANIC(421);
	if (ksceUIDClassInitClass(cacheClass, "SceUIDCacheClass", ksceKernelGetUIDClass(),
				  CACHE_OBJECT_SIZE, uidCacheConstructor, uidCacheDestructor) < 0)
		uidCacheClassInit();
	s_threadmgrRuntime.manager->cacheClass = (SceClass *)cacheClass;
	SCE_DBG_PRINTF_LEVEL(2, 0, 434, " sizeof(SceUIDCacheObject)=%d\n", CACHE_OBJECT_SIZE);
	waitableClass = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
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
	manager->signalId = ksceGUIDKernelCreate(manager->waitableClass,
						 "SceUIDSignalObject",
						 (SceObjectBase **)&manager->signalObject);
	if (s_threadmgrRuntime.manager->signalId < 0)
		SCE_PANIC(461);
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->signalObject->lock);
	threadClass = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
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
	object = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, THREAD_OBJECT_SIZE);
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
	ksceKernelGetMemBlockMappedBase(thread->kStackMemblkId, (void **)&thread->pKStack);
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
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(&object->waitLock);

	/* the interrupts, exceptions and power events */
	if (ksceKernelRegisterIntrHandler(SGI_INTR_REQUEST, "SceThreadmgrSGI", 0,
					  intrRequestSGIHandler, (void *)s_threadmgrRuntime.manager,
					  SGI_PRIORITY, CPU_MASK_ALL, NULL) < 0)
		SCE_PANIC(587);
	SCE_DBG_PRINTF_LEVEL(2, 0, 589, " sizeof(IntrRequest)=%d\n", SIZEOF_INTR_REQUEST);
	initIntrRequestPool();
	if (ksceKernelRegisterIntrHandler(SGI_STOP_CPUS, "SceThreadmgrSGILow", 0,
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
	ksceExcpmgrRegisterHandler(EXCP_UNDEFINED, EXCP_SLOT_FAULT, threadmgrUndefHandler);
	ksceExcpmgrRegisterHandler(EXCP_PREFETCH_ABORT, EXCP_SLOT_FAULT, threadmgrPrefetchHandler);
	ksceExcpmgrRegisterHandler(EXCP_DATA_ABORT, EXCP_SLOT_FAULT, threadmgrAbortHandler);
	ksceExcpmgrRegisterHandler(EXCP_UNDEFINED, EXCP_SLOT_DEBUG, threadmgrDebugUndefHandler);
	ksceExcpmgrRegisterHandler(EXCP_PREFETCH_ABORT, EXCP_SLOT_DEBUG,
				   threadmgrDebugPrefetchHandler);
	ksceExcpmgrRegisterHandler(EXCP_DATA_ABORT, EXCP_SLOT_DEBUG, threadmgrDebugAbortHandler);
	registerThreadmgrVfpExceptionIntrHandler();
	SceKernelSuspendForDriver_CE7A2207(SUSPEND_ORDER_THREADMGR, threadmgrSuspendHandler, NULL);
	SceKernelSuspendForDriver_105C5752(SUSPEND_ORDER_THREADMGR, threadmgrResumeHandler, NULL);
	s_threadmgrRuntime.manager->switchStackBlockId = ksceKernelAllocPartitionMemBlock(
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
	ksceKernelDcacheCleanRangeForL1WBWA((void *)start, end - start);
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
	object = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, THREAD_OBJECT_SIZE);
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
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(&object->waitLock);

	/* the thread this CPU runs on now, on the bootloader's stack */
	uid = ksceGUIDKernelCreate(s_threadmgrRuntime.manager->threadClass,
				   s_initialThreadNames[cpuId], (SceObjectBase **)&object);
	if (uid < 0)
		SCE_PANIC(803);
	thread = &object->context;
	thread->threadId = uid;
	thread->processId = THREADMGR_KERNEL_PID;
	thread->kStackSize = INITIAL_STACK_SIZE;
	thread->stackMemBlockType = MEMBLOCK_TYPE_KERNEL;
	thread->kStackMemblkId = argp->cpu[cpuId].stackMemBlockId;
	ret = ksceKernelGetMemBlockMappedBase(thread->kStackMemblkId, (void **)&thread->pKStack);
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
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(&object->waitLock);
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 4" : : "r"(thread) : "memory");
	ksceKernelGetMemBlockVBase(s_threadmgrRuntime.manager->switchStackBlockId, &base);
	cpu->switchStackTop = (uintptr_t)base + (cpuId + 1) * SWITCH_STACK_SIZE;
	return ret;
}

int module_bootstart(SceSize arglen, const void *argp)
	__attribute__((alias("sceKernelThreadMgrModuleBoot")));

/* a kernel thread's register context in the kernel process, entering at its entry */
static inline int initKernelThreadRegisters(struct ThreadCB *thread,
					    SceKernelProcessContext *context, SceAllocOpt *opt)
{
	struct ThreadRegisterContext *regs;

	opt->size = sizeof(*opt);
	opt->align = REGISTER_CONTEXT_ALIGN;
	regs = ksceKernelAllocHeapMemory2(THREADMGR_KERNEL_PID, sizeof(*regs), opt);
	thread->pRegisterContext = regs;
	if (regs == NULL)
		return -1;
	initRegisterContext(regs);
	regs->pc = thread->entry;
	regs->cpsr = (thread->entry & 1) ? CPSR_SYS_THUMB : CPSR_SYS_ARM;
	regs->sp = thread->initSp;
	regs->lr = (uintptr_t)kernelThreadReturnEntry;
	regs->ttbr1 = context->TTBR1;
	regs->kernelDacr = DACR_KERNEL;
	regs->contextidr = (uint8_t)context->CONTEXTIDR | thread->threadId << 8;
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
	SceKernelProcessContext *context;
	SceAllocOpt opt;
	void *addressSpace, *heap;
	unsigned int cpuId;
	int ret, i;

	cpuId = ksceKernelCpuId();
	process = SceSysmemForKernel_FA6C098C(THREADMGR_KERNEL_PID);
	addressSpace = SceSysmemForKernel_FA6C098C(SceSysrootForKernel_7334F1E8(THREADMGR_KERNEL_PID));
	ret = ksceKernelSysrootGetPUIDEntryHeap(THREADMGR_KERNEL_PID, &heap);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(963);
		return ret;
	}
	ret = ksceKernelProcessGetContext(THREADMGR_KERNEL_PID, &context);
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
		thread->pVfpRegisterContext = (uintptr_t)ksceKernelAllocHeapMemory2(THREADMGR_KERNEL_PID,
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
	thread->pRegisterContext = ksceKernelAllocHeapMemory2(THREADMGR_KERNEL_PID,
						       sizeof(*thread->pRegisterContext), &opt);
	if (thread->pRegisterContext == NULL) {
		SCE_DBG_ASSERT_FAIL(1115);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	initRegisterContext(thread->pRegisterContext);
	thread->pRegisterContext->userDacr = 0;
	thread->pRegisterContext->ttbr1 = context->TTBR1;
	thread->pRegisterContext->kernelDacr = DACR_KERNEL;
	thread->pRegisterContext->contextidr = (uint8_t)context->CONTEXTIDR | thread->threadId << 8;
	__asm__ volatile(
		"dsb\n\t"
		"mcr p15, 0, %0, c13, c0, 1\n\t"	/* CONTEXTIDR: none while TTBR changes */
		"isb\n\t"
		"mcr p15, 0, %1, c2, c0, 1\n\t"		/* TTBR1 */
		"isb\n\t"
		"dsb\n\t"
		"mcr p15, 0, %2, c13, c0, 1\n\t"
		"mcr p15, 0, %3, c3, c0, 0"		/* DACR */
		: : "r"(0), "r"(context->TTBR1),
		    "r"((uint8_t)context->CONTEXTIDR | thread->threadId << 8), "r"(DACR_KERNEL)
		: "memory");
	opt.size = sizeof(opt);
	opt.align = REGISTER_CONTEXT_ALIGN;
	thread->pVfpRegisterContext = (uintptr_t)ksceKernelAllocHeapMemory2(THREADMGR_KERNEL_PID,
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
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	setupNextContext_intr(0, 8);
	thread = currentThreadContext();
	SceIntrmgrForKernel_08FDB0CE(thread->pRegisterContext, 0,
				     thread->pVfpRegisterContext);
}
