/* Saved system-register policy; banked and live registers stay in the veneer. */
#include "sdfmgr_internal.h"
#include <sce/arm_thread_context.h>
#include <sce/excpmgr_cpu.h>
#include <sce/intrmgr.h>

/* The debugger only needs the GIC CPU-interface prefix returned by Intrmgr. */
typedef struct SdfMgrCpuInterface {
	SceUInt32 control;
	SceUInt32 priorityMask;
	SceUInt32 binaryPoint;
	SceUInt32 acknowledge;
	SceUInt32 endOfIntr;
} SdfMgrCpuInterface;

extern volatile SdfMgrCpuInterface *SceIntrmgrForKernel_807B2004(void);
extern void *SceIntrmgrForKernel_01911612(unsigned int cpu, unsigned int level);
extern void sceSDfMgrExcpAssert(void);

/* A handled exception may return only after leaving the outermost level. */
void sdfMgrFinishException(const SceExcpmgrContext *context)
{
	SceUInt32 *depth = &s_sdfMgrState.exceptionDepth[readMpidr() & 15];
	SceUInt32 nesting = *depth - 1;

	if (nesting) {
		for (;;)
			sceSDfMgrExcpAssert();
	}
	*depth = nesting;
	writeDfsr(0);
	writeIfsr(0);
	writeAdfsr(0);
	writeAifsr(0);
	writeDfar(0);
	writeIfar(0);
	/* The native condition tests the depth pointer, not the saved thread. */
	if (depth)
		writeTpidrprw(context->thread);
}


typedef struct SdfMgrIrqStack {
	SceUInt32 guard;
	unsigned char storage[0x2000 - sizeof(SceUInt32)];
} SdfMgrIrqStack;

_Static_assert(sizeof(SdfMgrIrqStack) == 0x2000, "interrupt stack stride");

void sdfMgrMarkTracePending(void)
{
	const ExcpVectorBank *vectors = (const ExcpVectorBank *)readVbar();
	ExcpTraceArea *trace = vectors->trace;

	if (trace)
		trace->control = 1;
}

void sdfMgrRestoreExceptionAddressSpace(const SceExcpmgrContext *context)
{
	SceUInt32 translationBase = context->ttbr1;
	SceUInt32 contextId = context->contextId;

	writeTpidruro(context->tpidruro);
	writeContextId(0);
	isb();
	writeTtbr1(translationBase);
	isb();
	writeContextId(contextId);
	writeDacr(context->dacr);
	writePar(context->par);
}

void sdfMgrRestoreThreadDacr(const ThreadRegisterContext *context, SceUInt32 status)
{
	SceUInt32 dacr = (status & 31) == 16 ? context->userDacr : context->kernelDacr;

	writeDacr(dacr);
}

/* Complete the SDIO return without Intrmgr's ordinary interrupt-disable policy. */
SceUInt32 sdfMgrCompleteInterrupt(SceUInt32 acknowledge)
{
	volatile SdfMgrCpuInterface *cpu = SceIntrmgrForKernel_807B2004();
	const ExcpVectorBank *vectors;
	const SdfMgrIrqStack *stack;
	SceUInt32 *depth;
	SceUInt32 nesting, core;
	int level;

	cpu->endOfIntr = acknowledge;
	vectors = (const ExcpVectorBank *)readVbar();
	depth = vectors->nesting;
	nesting = *depth;
	core = readMpidr() & 15;
	level = ksceKernelIsIntrAllowedInCurrentContext();
	stack = SceIntrmgrForKernel_01911612(core, level);
	if ((stack - nesting)->guard != 0x1ab4e8dfU) {
		for (;;)
			;
	}
	*depth = --nesting;
	return nesting;
}

void sdfMgrRestoreSystemContext(const ThreadRegisterContext *context)
{
	SceUInt32 priorityMask = context->priorityMask;
	volatile SdfMgrCpuInterface *cpu = SceIntrmgrForKernel_807B2004();

	cpu->priorityMask = priorityMask;
	armRestoreThreadSystemContext(context, 1);
}

/* Native SDfMgr restores these slots directly, without Intrmgr's table locks. */
void sdfMgrRestoreDebugContext(const ThreadCB *thread,
			      SceUInt32 resumeValue, SceUInt32 resumeControl,
			      int threadReturn)
{
	const ProcessHwBreakpoints *regs;
	const ProcessHwWatchpoints *watch;

	if (thread)
		regs = thread->process->phbp;
	else
		regs = ((SceExcpmgrData *)s_sdfMgrState.exceptionDepth)->kernelBreakpoints;
	if (!regs)
		return;
	CP_WRITE(p14, 0, c0, c1, 4, resumeValue);
	CP_WRITE(p14, 0, c0, c1, 5, resumeControl);
	if (threadReturn) {
		CP_WRITE(p14, 0, c0, c0, 4, regs->reg[0].value);
		CP_WRITE(p14, 0, c0, c0, 5, regs->reg[0].control);
	}
	CP_WRITE(p14, 0, c0, c2, 4, regs->reg[2].value);
	CP_WRITE(p14, 0, c0, c2, 5, regs->reg[2].control);
	CP_WRITE(p14, 0, c0, c3, 4, regs->reg[3].value);
	CP_WRITE(p14, 0, c0, c3, 5, regs->reg[3].control);
	CP_WRITE(p14, 0, c0, c4, 4, regs->reg[4].value);
	CP_WRITE(p14, 0, c0, c4, 5, regs->reg[4].control);
	if (thread)
		watch = thread->process->phwp;
	else
		watch = ((SceExcpmgrData *)s_sdfMgrState.exceptionDepth)->kernelWatchpoints;
	if (!watch)
		return;
	CP_WRITE(p14, 0, c0, c1, 6, watch->reg[1].value);
	CP_WRITE(p14, 0, c0, c1, 7, watch->reg[1].control);
	CP_WRITE(p14, 0, c0, c2, 6, watch->reg[2].value);
	CP_WRITE(p14, 0, c0, c2, 7, watch->reg[2].control);
	CP_WRITE(p14, 0, c0, c3, 6, watch->reg[3].value);
	CP_WRITE(p14, 0, c0, c3, 7, watch->reg[3].control);
	if (thread) {
		CP_WRITE(p14, 0, c0, c5, 4, thread->thbp[0]);
		CP_WRITE(p14, 0, c0, c5, 5, thread->thbp[1]);
	}
}

