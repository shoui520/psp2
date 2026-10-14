/*
 * SceKernelThreadMgr: VFP/NEON exceptions. Each CPU's exception interrupt
 * (0x24-0x27) prints the exception and the CPU's branch trace, then takes
 * the thread that raised it off the CPUs (debug-suspended for reason 0x10)
 * and asks the debugger's handlers what to do: resume it, suspend its whole
 * process, leave it, or panic. In a kernel thread it is fatal.
 */

#include <string.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "vfp_exception.c"

#define THREAD_ATTR_USER		0x80000000
#define PROCESS_DEBUG_SUSPEND_EXCEPTION	0x11	/* PLACEHOLDER: SceSysrootForKernel_1247A825's reason */
#define PROCESS_DEBUG_SUSPEND_VFP	2	/* the processDebugSuspendHook kind */

/* what the debugger's handler decides */
#define VFP_EXCEPTION_RESUME		0
#define VFP_EXCEPTION_SUSPEND_PROCESS	1
#define VFP_EXCEPTION_IGNORE		2
#define VFP_EXCEPTION_PANIC		3

#define TRACE_ENTRIES			32
#define TRACE_VALID			0x80000000
#define TRACE_THUMB			0x00001000
#define TRACE_TAKEN			0x00000400
#define TRACE_T32			0x00000200
#define TRACE_LINK			0x00000100

#define SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED	0x80028030

/* the CPU's VFP exception state (VBAR +0x24) */
struct ThreadmgrVfpExceptionArea {
	uint32_t exceptionMask;			/* 0x00: the running thread's */
	uint32_t status;			/* 0x04: the first exception status register */
	uint8_t reserved0[0xf8];
	uint32_t traceControl;			/* 0x100: bits 8-12 the trace's next entry */
	uint8_t reserved1[0xfc];
	uint8_t trace[0x200];			/* 0x200: the branch trace, raw */
};

/* one entry of the decoded branch trace */
typedef struct ThreadmgrVfpTrace {
	uint32_t address[3];
	uint32_t flags;				/* TRACE_*; bits 4-7 the exception, 0-2 the kind */
} ThreadmgrVfpTrace;

/* what the debugger's VFP exception handler gets (0x210) */
typedef struct ThreadmgrVfpExceptionMessage {
	SceUID processId;
	SceUID threadId;
	uint32_t status;
	uint32_t reserved;
	ThreadmgrVfpTrace trace[TRACE_ENTRIES];
} ThreadmgrVfpExceptionMessage;

_Static_assert(sizeof(ThreadmgrVfpExceptionMessage) == 0x210, "VFP exception message size");

typedef void (*ThreadmgrProcessDebugSuspendHook)(SceUID pid, SceUID threadId, int kind,
						 uintptr_t arg);

extern void SceIntrmgrForKernel_71E08D71(ThreadmgrVfpTrace *trace, const void *raw);
extern int SceExcpmgrForKernel_B615A7DA(void);
extern int SceSysrootForKernel_1247A825(SceUID pid, uint32_t reason);
extern int ksceKernelIsIntrAllowedInCurrentContext(void);
extern const char *ksceGUIDName(SceUID guid);
extern int ksceKernelRegisterIntrHandler(int intrCode, const char *name, int interruptType,
					 void *handler, void *userCtx, int priority,
					 int targetCpu, void *opt);
extern int ksceKernelEnableIntr(int intrCode);
extern void sceKernelSysrootCorelockLock(unsigned int cpu);
extern void sceKernelSysrootCorelockUnlock(void);
void runningThreadPreemptByIntr(struct ThreadCB *context, uint32_t reason);
int ksceKernelDebugResumeThread(SceUID threadId, uint32_t reason);

static const char *const s_traceExceptions[16] = {
	"0", "Halting debug-mode", "Secure Monitor", "3", "Imprecise data abort", "T2EE Trap",
	"6", "7", "Reset", "Undefined", "SVC", "Prefetch abort", "Precise data abort",
	"software watchpoint", "IRQ", "FIQ",
};

static const char *const s_traceKinds[8] = {
	"Direct branch", "Indirect branch", "Exception", "DMB/DSB/ISB", "Debug entry",
	"Debug exit", "6", "7",
};

/* the exception interrupts, one per CPU */
static const struct {
	const char *name;
	int intrCode;
	int targetCpu;
} s_vfpExceptionIntrs[4] = {
	{ "SceThreadmgrVfpException0", 0x24, 1 },
	{ "SceThreadmgrVfpException1", 0x25, 2 },
	{ "SceThreadmgrVfpException2", 0x26, 4 },
	{ "SceThreadmgrVfpException3", 0x27, 8 },
};

/* the exception and the trace, oldest entry first */
static inline void printVfpException(struct ThreadmgrVfpExceptionArea *area,
				     const ThreadmgrVfpTrace *trace)
{
	const ThreadmgrVfpTrace *entry;
	unsigned int next, i, n;

	next = (area->traceControl >> 8) & 0x1f;
	SCE_PRINTF_WITH_INFO(0, 73, "\n");
	SCE_PRINTF_WITH_INFO(0, 74, "========================================================\n");
	SCE_PRINTF_WITH_INFO(0, 75, "[ VFP/NEON exception occured ]\n");
	SCE_PRINTF_WITH_INFO(0, 76, "  Core        : ARM%d\n", ksceKernelCpuId());
	SCE_PRINTF_WITH_INFO(0, 77, "  VFP 1STISR  : 0x%08x\n", area->status);
	if (s_threadmgrRuntime.vfpExceptionHook != NULL)
		s_threadmgrRuntime.vfpExceptionHook(0, 1, area->status);
	for (i = next + 1; i != next + 1 + TRACE_ENTRIES; i++) {
		n = i & (TRACE_ENTRIES - 1);
		entry = &trace[n];
		if (!(entry->flags & TRACE_VALID))
			continue;
		SCE_PRINTF_WITH_INFO(0, 96,
			"  WPT[%2d]     : 0x%08X 0x%08X 0x%08X %s %s%s%s %s %s\n", n,
			entry->address[0], entry->address[1], entry->address[2],
			(entry->flags & TRACE_THUMB) ? "Thumb" : "ARM",
			(entry->flags & TRACE_TAKEN) ? "Taken " : "",
			(entry->flags & TRACE_T32) ? "T32 " : "",
			(entry->flags & TRACE_LINK) ? "Link " : "",
			s_traceExceptions[(entry->flags >> 4) & 0xf], s_traceKinds[entry->flags & 7]);
		if (s_threadmgrRuntime.vfpExceptionHook != NULL)
			s_threadmgrRuntime.vfpExceptionHook(1, 5, n, trace[n].address[0], trace[n].address[1],
					   trace[n].address[2], trace[n].flags);
	}
	SCE_PRINTF_WITH_INFO(0, 103, "========================================================\n\n");
}

/* the exception interrupt handler of each CPU */
int threadmgrVfpExceptionIntrHandler(int code, void *arg)
{
	struct ThreadmgrVfpExceptionArea *area =
		(struct ThreadmgrVfpExceptionArea *)currentVfpException();
	ThreadmgrVfpTrace trace[TRACE_ENTRIES];
	ThreadmgrVfpExceptionMessage message;
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	ThreadmgrDebugHandler handler;
	ThreadmgrProcessDebugSuspendHook hook;
	SceProcessObject *process;
	uintptr_t self;
	SceUID threadId, pid;
	int intr, cpuId, action, ret, i;

	SceIntrmgrForKernel_71E08D71(trace, area->trace);
	printVfpException(area, trace);
	if (SceExcpmgrForKernel_B615A7DA() > 0) {
		SCE_PRINTF_WITH_INFO(0, 142,
			"fatal error: VFP/NEON exception occurred in exception handler.\n\n");
		if (s_threadmgrRuntime.vfpExceptionHook != NULL)
			s_threadmgrRuntime.vfpExceptionHook(2, 0);
		return -1;
	}
	if (ksceKernelIsIntrAllowedInCurrentContext() > 1) {
		SCE_PRINTF_WITH_INFO(0, 151,
			"fatal error: VFP/NEON exception occurred in interrupt handler.\n\n");
		if (s_threadmgrRuntime.vfpExceptionHook != NULL)
			s_threadmgrRuntime.vfpExceptionHook(3, 0);
		return -1;
	}
	intr = ksceKernelCpuSuspendIntr();
	cpuId = ksceKernelCpuId();
	cpu = currentCpuState();
	threadId = cpu->runningThreadId;
	if (ksceGUIDReferObjectWithClass(threadId, s_threadmgrRuntime.manager->threadClass,
					 (SceObjectBase **)&object) < 0) {
		SCE_DBG_ASSERT_FAIL(168);
		SCE_PRINTF_WITH_INFO(0, 169,
			"warning: VFP/NEON exception occurred, but the thread did not exist.\n\n");
		if (s_threadmgrRuntime.vfpExceptionHook != NULL)
			s_threadmgrRuntime.vfpExceptionHook(4, 0);
		ksceKernelCpuResumeIntr(intr);
		return -1;
	}
	thread = &object->context;
	pid = thread->processId;
	process = thread->process;
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	SCE_DBG_ASSERT_CHECK((uint16_t)thread->status == SCE_KERNEL_THREAD_STATUS_RUNNING, 182);
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == cpuId, 183);
	SCE_DBG_ASSERT_CHECK(thread->threadId != cpu->idleThread->threadId, 185);

	/* off the CPU, debug-suspended */
	runningThreadPreemptByIntr(thread, 2);
	thread->currentCpuId = -1;
	thread->debugSuspendReason |= DEBUG_SUSPEND_VFP_EXCEPTION;
	thread->status = ((thread->status & 0x7fff) << 16) | THREAD_STATE_DEBUG_SUSPENDED |
			     THREAD_STATE_OFF_CPU;
	manager = s_threadmgrRuntime.manager;
	self = (uintptr_t)thread;
	ksceKernelSpinlockLowLock(&manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext;
	thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
	s_threadmgrRuntime.manager->debugSuspendedPrev = self;
	*(uintptr_t *)thread->prev = self;
	ksceKernelSpinlockLowUnlock(&manager->threadMgrCBLock);

	if (!(thread->currentAttr & THREAD_ATTR_USER)) {
		SCE_PRINTF_WITH_INFO(0, 205,
			"fatal error: VFP/NEON exception occurred in kernel thread. ThreadId: 0x%08X [%s]\n\n",
			thread->threadId, ksceGUIDName(thread->threadId));
		if (s_threadmgrRuntime.vfpExceptionHook != NULL)
			s_threadmgrRuntime.vfpExceptionHook(5, 1, thread->threadId);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ksceGUIDReleaseObject(threadId);
		ksceKernelCpuResumeIntr(intr);
		action = VFP_EXCEPTION_PANIC;
	} else {
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		/* the debugger decides; the second group's handler has the last word */
		for (i = 0; i < 2; i++) {
			handler = s_threadmgrRuntime.manager->debugHandlers[i][DEBUG_HANDLER_VFP_EXCEPTION];
			if (handler == NULL)
				continue;
			message.status = area->status;
			message.threadId = threadId;
			message.processId = thread->processId;
			memcpy(message.trace, trace, sizeof(message.trace));
			action = handler(sizeof(message), &message);
			if (i == 1)
				goto decided;
		}
		action = VFP_EXCEPTION_SUSPEND_PROCESS;
decided:
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		SCE_DBG_ASSERT_LEVEL(0, thread->pVfpRegisterContext != 0, 233);
		/* the exception is cleared for when it runs again */
		*(uint32_t *)(thread->pVfpRegisterContext + 0x100) &= ~area->status;
		area->status = 1;
		area->traceControl |= 1;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ksceGUIDReleaseObject(threadId);
		ksceKernelCpuResumeIntr(intr);
	}
	if (thread == NULL)
		return -1;
	switch (action) {
	case VFP_EXCEPTION_RESUME:
		ret = ksceKernelDebugResumeThread(threadId, DEBUG_SUSPEND_VFP_EXCEPTION);
		if (ret < 0 && ret != (int)SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED)
			SCE_PANIC(254);
		break;
	case VFP_EXCEPTION_SUSPEND_PROCESS:
		if (ksceKernelAtomicCompareAndSet32((SceInt32 *)&process->abortRequested, 0, 1) != 0)
			break;
		ret = SceSysrootForKernel_1247A825(pid, PROCESS_DEBUG_SUSPEND_EXCEPTION);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 262, "(): sceKernelProcessDebugSuspend(0x%08X)=0x%08X\n",
				       pid, ret);
			SCE_PANIC(263);
		}
		hook = (ThreadmgrProcessDebugSuspendHook)
			s_threadmgrRuntime.manager->processDebugSuspendHook;
		if (hook != NULL)
			hook(pid, threadId, PROCESS_DEBUG_SUSPEND_VFP,
			     s_threadmgrRuntime.manager->processDebugSuspendArg);
		break;
	case VFP_EXCEPTION_IGNORE:
		break;
	case VFP_EXCEPTION_PANIC:
		SCE_PANIC(275);
	default:
		SCE_PANIC(278);
	}
	return -1;
}

/* SceThreadmgrForKernel_10853CCD: the hook told about each VFP exception */
void SceThreadmgrForKernel_10853CCD(ThreadmgrTraceHook hook)
{
	s_threadmgrRuntime.vfpExceptionHook = hook;
}

int registerThreadmgrVfpExceptionIntrHandler(void)
{
	int i, ret;

	for (i = 0; i < 4; i++) {
		ret = ksceKernelRegisterIntrHandler(s_vfpExceptionIntrs[i].intrCode,
			s_vfpExceptionIntrs[i].name, 0, threadmgrVfpExceptionIntrHandler, NULL, 0x10,
			s_vfpExceptionIntrs[i].targetCpu, NULL);
		if (ret < 0) {
			SCE_DBG_PRINTF(6, 318,
				"register threadmgr VFP/NEON exception handler failed. 0x%08x, intrcode %d\n",
				ret, s_vfpExceptionIntrs[i].intrCode);
			SCE_PANIC(319);
		}
		ksceKernelEnableIntr(s_vfpExceptionIntrs[i].intrCode);
	}
	return 0;
}

/* PLACEHOLDER: name; before a power suspend (nothing to do) */
void suspendThreadmgrVfpException(void)
{
}

/* PLACEHOLDER: name; after a power resume CPU 0 enables the exception interrupts again */
void resumeThreadmgrVfpException(void)
{
	sceKernelSysrootCorelockLock(0);
	if (ksceKernelCpuId() == 0) {
		ksceKernelEnableIntr(0x24);
		ksceKernelEnableIntr(0x25);
		ksceKernelEnableIntr(0x26);
		ksceKernelEnableIntr(0x27);
	}
	sceKernelSysrootCorelockUnlock();
}
