/* CPU context policy, separated from the live-register entry veneers. */
#include <sysmem/cp15.h>
#include <excpmgr/excpmgr_cpu.h>
#include "intrmgr.h"
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr_syscall.h>
#include <threadmgr/arm_thread_context.h>
#include <sysmem/mmu.h>
#include <sysmem/barrier.h>
#include <sysmem/compiler.h>



/* name unknown: enter the kernel address space while still on the IRQ stack.
 * The acknowledge number occupies the context ID's interrupt-tracing field.
 */
void intrSwitchKernelContext(SceUInt32 acknowledge)
{
	const MMUContext *kernel;
	SceUInt32 translationBase, contextId;

	writeTpidrprw(0);
	kernel = g_intrSysroot;
	translationBase = kernel->ttbr1;
	contextId = (kernel->contextId & ~0x03ff0000U) |
		((acknowledge & 0x3ffU) << 16);
	writeContextId(0);
	isb();
	writeTtbr1(translationBase);
	isb();
	writeContextId(contextId);
	writeDacr(0x15450000U);
}

/* name unknown: shared save policy in the IRQ and cooperative-save paths.
 * Coprocessor access itself cannot be expressed in C; its sequencing, context
 * fields and the six-counter traversal can. IRQ entry has already disabled the
 * saved PMU mask, whereas cooperative entry reads and disables it here.
 */
void intrSaveSystemContext(ThreadRegisterContext *context, SceUInt32 enabled,
			  const IntrIrqFrame *irq)
{
	unsigned int counter;

	if (irq) {
		context->full = 1;
		for (counter = 0; counter < 5; counter++)
			context->gpr[counter] = irq->registers[counter];
		if ((irq->returnStatus & 31) == 31)
			context->kernelDacr = irq->system.dacr;
	}
	context->cpacr = readCpacr();
	context->tpidrurw = readTpidrurw();
	context->tpidruro = readTpidruro();
	context->ttbr1 = irq ? irq->system.translationBase : readTtbr1();
	context->contextidr = irq ? irq->system.contextId : readContextId();
	context->par = irq ? irq->system.physicalAddress : readPar();
	context->pmcr = readPmcr();
	context->pmovsr = readPmovsr();
	context->pmselr = readPmselr();
	if (!irq) {
		enabled = readPmcntenset();
		writePmcntenclr(enabled);
	}
	context->pmcntenset = enabled;
	context->pmccntr = readPmccntr();
	for (counter = 0; counter < 6; counter++) {
		writePmselr(counter);
		context->pmevent[counter].eventType = readPmxevtyper();
		context->pmevent[counter].count = readPmxevcntr();
	}
	context->dfsr = readDfsr();
	context->ifsr = readIfsr();
	context->dfar = readDfar();
	context->ifar = readIfar();
	context->teehbr = readTeehbr();
	context->priorityMask = g_intrCpuInterface->priorityMask;
	if (irq) {
		context->pc = irq->returnAddress;
		context->cpsr = irq->returnStatus;
	}
}

/* name unknown: the inverse shared register restore policy. The veneers keep
 * live return registers and the banked stack transition outside this helper.
 */
void intrRestoreSystemContext(const ThreadRegisterContext *context, int irq)
{
	g_intrCpuInterface->priorityMask = context->priorityMask;
	armRestoreThreadSystemContext(context, irq);
}

/* name unknown: install the three kernel/thread-owned debug slots. The first
 * two breakpoint slots and first watchpoint slot are reserved by other users.
 */
void intrRestoreBreakpoints(ProcessHwBreakpoints *regs, int synchronize)
{
	ksceKernelSpinlockLowLock(&regs->lock);
	writeDbgbvr(2, regs->reg[2].value);
	writeDbgbcr(2, regs->reg[2].control);
	writeDbgbvr(3, regs->reg[3].value);
	writeDbgbcr(3, regs->reg[3].control);
	writeDbgbvr(4, regs->reg[4].value);
	writeDbgbcr(4, regs->reg[4].control);
	if (synchronize)
		isb();
	ksceKernelSpinlockLowUnlock(&regs->lock);
}

/* name unknown: restore the corresponding three watchpoint slots. */
void intrRestoreWatchpoints(ProcessHwWatchpoints *regs, int synchronize)
{
	ksceKernelSpinlockLowLock(&regs->lock);
	writeDbgwvr(1, regs->reg[1].value);
	writeDbgwcr(1, regs->reg[1].control);
	writeDbgwvr(2, regs->reg[2].value);
	writeDbgwcr(2, regs->reg[2].control);
	writeDbgwvr(3, regs->reg[3].value);
	writeDbgwcr(3, regs->reg[3].control);
	if (synchronize)
		isb();
	ksceKernelSpinlockLowUnlock(&regs->lock);
}

/* name unknown: suppress kernel-owned debug slots while changing context. */
void intrDisableKernelDebug(void)
{
	if (g_intrKernelBreakpoints) {
		writeDbgbcr(2, 0);
		writeDbgbcr(3, 0);
		writeDbgbcr(4, 0);
		writeDbgbcr(5, 0);
	}
	if (g_intrKernelWatchpoints) {
		writeDbgwcr(1, 0);
		writeDbgwcr(2, 0);
		writeDbgwcr(3, 0);
	}
}

/* name unknown: kernel watchpoints are installed only with breakpoints. */
void intrRestoreKernelDebug(void)
{
	if (!g_intrKernelBreakpoints)
		return;
	intrRestoreBreakpoints(g_intrKernelBreakpoints, 1);
	if (g_intrKernelWatchpoints)
		intrRestoreWatchpoints(g_intrKernelWatchpoints, 1);
}

typedef struct IntrEnterHookRecord {
	SceUInt32 returnAddress;
	void *handler;
	SceUInt32 intrCode;
	void *registeredHandler;
	void *argument;
	SceUInt32 priorityClass;
} IntrEnterHookRecord;

/* name unknown: the hook ABI deliberately carries the handler twice. */
void intrCallEnterHook(unsigned int code, const IntrHandler *info, SceUInt32 pc)
{
	void (*hook)(const IntrEnterHookRecord *) = g_intrEnterIntr;
	IntrEnterHookRecord record;

	if (!hook)
		return;
	record.returnAddress = pc;
	record.handler = info->handler;
	record.intrCode = code;
	record.registeredHandler = info->handler;
	record.argument = info->arg;
	record.priorityClass = info->priority >> 4;
	hook(&record);
}

typedef struct IntrLeaveHookRecord {
	int result;
	SceUInt32 acknowledge;
} IntrLeaveHookRecord;

/* name unknown: report the handler result and original GIC acknowledge token. */
void intrCallLeaveHook(int result, SceUInt32 acknowledge)
{
	void (*hook)(const IntrLeaveHookRecord *) = g_intrLeaveIntr;
	IntrLeaveHookRecord record;

	if (!hook)
		return;
	record.result = result;
	record.acknowledge = acknowledge;
	hook(&record);
}

/* name unknown: syscall instrumentation keeps the saved register image mutable. */
void intrCallSyscallEnterHook(ThreadSyscallFrame *frame)
{
	void (*hook)(ThreadSyscallFrame *) = g_intrEnterSyscall;

	if (hook)
		hook(frame);
}

/* name unknown: leave instrumentation may replace both arguments and results. */
void intrCallSyscallLeaveHook(ThreadSyscallFrame *frame, SceUInt32 *results)
{
	void (*hook)(ThreadSyscallFrame *, SceUInt32 *) = g_intrLeaveSyscall;

	if (hook)
		hook(frame, results);
}


/* name unknown: scheduler callbacks registered through the three opaque NIDs. */
void intrTraceInterruptedThread(SceUInt32 acknowledge, ThreadCB *thread)
{
	void (*trace)(SceUInt32, ThreadCB *) = g_intrStackGuard.beforeIntrHandler;

	if (thread->waitType == 0x2000)
		trace(acknowledge, thread);
}

/* name unknown: resume or replace the outermost interrupted thread context. */
ThreadRegisterContext *intrResumeInterruptedThread(ThreadCB *thread)
{
	int (*pending)(void) = g_intrDispatchPending;
	ThreadRegisterContext *context;
	int needResume = pending();

	writeTpidrprw(thread);
	context = thread->pRegisterContext;
	if (needResume) {
		ThreadRegisterContext *(*resume)(ThreadRegisterContext *, int) = g_intrSetupNextContext;

		context = resume(context, 1);
	}
	return context;
}

/* name unknown: scratch capture on the banked supervisor stack. */
typedef struct IntrSupervisorFrame {
	SceUInt32 syscallNumber;
	SceUInt32 dacr;
	SceUInt32 returnAddress;
	SceUInt32 returnStatus;
	SceUInt32 arguments[4];
} IntrSupervisorFrame;

/* name unknown: prepare the thread's kernel frame before capturing live user
 * registers. The veneer allocates the supervisor scratch record before calling
 * C, so this helper never writes below an unallocated stack pointer.
 */
ThreadSyscallFrame *intrPrepareSyscall(IntrSupervisorFrame *supervisor)
{
	ThreadCB *thread = getCurrentThreadCB();
	ThreadSyscallFrame *frame;

	supervisor->dacr = readDacr();
	thread->pRegisterContext->kernelDacr = 0x15450000U;
	writeDacr(0x15450000U);
	writeTpidruro(1);
	thread->permission = thread->initPermission;
	frame = (ThreadSyscallFrame *)thread->kernelStackTop - 1;
	frame->syscallAux = thread->pRegisterContext->syscallAux;
	thread->userFrame = frame;
	frame->apiDeclare = 0;
	frame->tpidrurw = readTpidrurw();
	return frame;
}

void illegalUserDACRCfunc(const ThreadCB *thread, unsigned int dacr);

/* name unknown: resolve the captured syscall and validate its entry domains. */
int intrSelectSyscall(ThreadSyscallFrame *frame, SceUInt32 previousDacr)
{
	void **table;

	if (frame->r[12] >= 4096)
		return 0;
	table = g_intrSyscallTable;
	frame->entry = (uintptr_t)table[frame->r[12]];
	frame->fpscr = readFpscr();
	if (previousDacr & 0xeabaaa8eU) {
		illegalUserDACRCfunc(getCurrentThreadCB(), previousDacr);
		__breakpoint(0);
	}
	writeFpscr(FPSCR_DN | FPSCR_FZ);
	return 1;
}

/* name unknown: load thread-owned slots after the CPU context has changed. */
void intrRestoreThreadDebug(const ThreadCB *thread)
{
	SceProcessObject *process = thread->process;

	if (!process->phbp)
		return;
	intrRestoreBreakpoints(process->phbp, 0);
	if (!process->phwp)
		return;
	intrRestoreWatchpoints(process->phwp, 0);
	writeDbgbvr(5, thread->thbp[0]);
	writeDbgbcr(5, thread->thbp[1]);
}

/* name unknown: select user/kernel domains from the eventual return mode.
 * Runs on the IRQ stack before the veneer restores the return registers.
 */
void intrPrepareContextReturn(const ThreadCB *thread, SceUInt32 status,
			      const ThreadRegisterContext *context)
{
	SceUInt32 dacr = context->kernelDacr;

	if ((status & 31) == 16) {
		dacr = context->userDacr;
		if (dacr & 0xeabaaa8eU) {
			illegalUserDACRCfunc(thread, dacr);
			__breakpoint(0);
		}
	}
	writeDacr(dacr);
	intrRestoreThreadDebug(thread);
}

/* name unknown: choose a normal return or the Sysroot slow-return/restart hook.
 * A nonzero return asks the veneer to call the (possibly replaced) syscall
 * entry again. Results remain in the caller's four-word register image.
 */
int intrFinishSyscall(ThreadSyscallFrame *frame, SceUInt32 *results, int invalidEntry)
{
	ThreadCB *thread;
	SceUInt32 flags, apiStat, kind;
	int classification;

	if (invalidEntry)
		goto event;
	apiStat = readTpidruro();
	if (apiStat != 1)
		invalidApiStatCfunc(frame, results, apiStat);
	thread = getCurrentThreadCB();
	if (thread->ownedFastMutexNext != (uintptr_t)&thread->ownedFastMutexNext) {
		invalidFastMutexOwnerCfunc();
		thread = getCurrentThreadCB();
	}
	thread->permission = thread->initPermission;
	flags = thread->flags & 0xf0000000U;
	if (flags)
		goto slow;
	if (thread->debugSuspendReason) {
		flags = 0x02000000;
		goto slow;
	}
	kind = frame->apiDeclare;
	if (kind == 1) {
		classification = (int)results[1];
		if (classification >= 0)
			return 0;
	} else if (kind == 2) {
		classification = 1;
	} else {
		classification = (int)kind;
		if (classification >= 0)
			return 0;
	}
	if (classification == ((int)results[0] >> 30)) {
		flags = 0x01000000;
		goto slow;
	}
	if (results[0] != 0x8002710cU)
		return 0;
event:
	callIntrEvent(0x80000, 0, NULL);
	thread = getCurrentThreadCB();
	flags = thread->flags & 0xf0000000U;
	if (!flags)
		flags = 0x02000000;
slow:
	return sceKernelSysrootTrapThreadAfterSyscall(frame, results, flags) == 0;
}

/* name unknown: restore the thread's user API/MMU state before register return.
 * LDREX/STREX is the sole non-C operation in the flags update; no extra memory
 * barrier is added to Sony's original exclusive loop.
 */
SceUInt32 intrRestoreSyscallContext(const ThreadSyscallFrame *frame)
{
	ThreadCB *thread = getCurrentThreadCB();
	SceUInt32 previous;

	thread->pRegisterContext->syscallAux = frame->syscallAux;
	thread->userFrame = NULL;
	writeTpidruro(thread->tls);
	previous = __atomic_fetch_and(&thread->flags, ~0xa000U, __ATOMIC_RELAXED);
	thread->pRegisterContext->kernelDacr = readDacr();
	writeDacr(thread->pRegisterContext->userDacr & 0x55555575U);
	return previous;
}
