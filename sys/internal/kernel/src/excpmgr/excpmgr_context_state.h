/*
 * SceExcpmgr: saving and restoring the interrupted CPU state around an
 * exception. Shared by os0:kd/excpmgr.skprx and the secure-world
 * slb2:kbl/SceExcpmgr, which Sony built from the same excpmgr source (the
 * debug records carry identical line numbers).
 *
 * This header holds function definitions: include it from exactly one C file
 * of a module, after providing
 *
 *	g_excpData			the module's data block with nestCount[],
 *					stackTop[], kernelMmu, kernelBreakpoints
 *					and kernelWatchpoints
 *	excpReturnToAbortMode()		the module's resume stub (assembly)
 *	excpReturnToContext		optional #define giving the return-into-context
 *					function its exported name
 *
 * The kernel import names used here (spinlocks, memcpy) are the os0 ones; the
 * secure-world module maps them to its ForTZS imports before including.
 *
 * Sony wrote all of this as ARM assembly, so none of these functions has a
 * debug-record name: the names are descriptive.
 */

#pragma once

#include <scetypes.h>
#include <excpmgr/excpmgr.h>
#include <excpmgr/excpmgr_cpu.h>

void excpReturnToAbortMode(SceExcpmgrContext *context) __attribute__((noreturn));
void sceKernelPanicHandlerReturnFromNestedExceptionCfunc(SceExcpmgrContext *context);

/* DBGBVRn/DBGBCRn and DBGWVRn/DBGWCRn; CRm is the register number */
#define writeBreakpoint(n, value, control) do {				\
	unsigned int breakpointValue = (value);				\
	unsigned int breakpointControl = (control);			\
	CP_WRITE(p14, 0, c0, c##n, 4, breakpointValue);			\
	CP_WRITE(p14, 0, c0, c##n, 5, breakpointControl);			\
} while (0)
#define writeWatchpoint(n, value, control) do {				\
	unsigned int watchpointValue = (value);				\
	unsigned int watchpointControl = (control);			\
	CP_WRITE(p14, 0, c0, c##n, 6, watchpointValue);			\
	CP_WRITE(p14, 0, c0, c##n, 7, watchpointControl);			\
} while (0)

#ifndef EXCP_CONTEXT_DEBUG_LINKAGE
#define EXCP_CONTEXT_DEBUG_LINKAGE static
#endif
EXCP_CONTEXT_DEBUG_LINKAGE void excpRestoreDebugRegisters(SceExcpmgrContext *context);

static inline unsigned int excpCpu(void)
{
	return readMpidr() & 15;
}

static inline ExcpTraceArea *excpTraceArea(void)
{
	return ((const ExcpVectorBank *)readVbar())->trace;
}

/* switch TTBR1 with the ASID parked at 0 in between */
static inline void excpSetAddressSpace(unsigned int ttbr1, unsigned int contextId)
{
	writeContextId(0);
	isb();
	writeTtbr1(ttbr1);
	isb();
	writeContextId(contextId);
}

/* everything about the interrupted state the entry stub did not save */
#define EXCP_SAVE_PMU_COUNTER(context, n) do {			\
	unsigned int type, count;				\
	writePmselr(n);						\
	type = readPmxevtyper();					\
	count = readPmxevcntr();					\
	(context)->pmuCounter[n].type = type;			\
	(context)->pmuCounter[n].count = count;			\
} while (0)

#define EXCP_COPY_WAYPOINTS_8(dst, src) do {			\
	*(dst)++ = *(src)++;					\
	*(dst)++ = *(src)++;					\
	*(dst)++ = *(src)++;					\
	*(dst)++ = *(src)++;					\
	*(dst)++ = *(src)++;					\
	*(dst)++ = *(src)++;					\
	*(dst)++ = *(src)++;					\
	*(dst)++ = *(src)++;					\
} while (0)

static inline void excpSaveState(SceExcpmgrContext *context, unsigned int code)
{
	ExcpTraceArea *trace;
	SceExcpmgrWaypoint *dst;
	const SceExcpmgrWaypoint *src;
	unsigned int pmccntr, pmcr, pmcntenset, pmcntenset2, pmselr, pmuserenr;

	pmccntr = readPmccntr();
	pmcr = readPmcr();
	pmcntenset = readPmcntenset();
	pmcntenset2 = readPmcntenset();
	pmselr = readPmselr();
	pmuserenr = readPmuserenr();
	context->pmcr = pmcr;
	context->pmcntenset = pmcntenset;
	context->pmcntenset2 = pmcntenset2;
	context->pmselr = pmselr;
	context->pmccntr = pmccntr;
	context->pmuserenr = pmuserenr;
	EXCP_SAVE_PMU_COUNTER(context, 0);
	EXCP_SAVE_PMU_COUNTER(context, 1);
	EXCP_SAVE_PMU_COUNTER(context, 2);
	EXCP_SAVE_PMU_COUNTER(context, 3);
	EXCP_SAVE_PMU_COUNTER(context, 4);
	EXCP_SAVE_PMU_COUNTER(context, 5);
	context->cpacr = readCpacr();
	writeCpacr(0xf00000);
	context->fpexc = readFpexc();
	context->fpscr = readFpscr();
	writeFpexc(0x40000000);
	writeFpscr(0x3000000);
	context->excpCode = code;
	context->ttbr1 = readTtbr1();
	context->contextId = readContextId();
	context->tpidrurw = readTpidrurw();
	context->tpidruro = readTpidruro();
	context->thread = (void *)readTpidrprw();
	context->dacr = readDacr();
	context->dfsr = readDfsr();
	context->ifsr = readIfsr();
	context->dfar = readDfar();
	context->ifar = readIfar();
	context->par = readPar();
	context->teehbr = readTeehbr();
	context->dbgdscr = readDbgdscr();
	trace = excpTraceArea();
	if (trace != NULL) {
		context->waypointControl = trace->control;
		dst = context->waypoint;
		src = trace->waypoint;
		EXCP_COPY_WAYPOINTS_8(dst, src);
		EXCP_COPY_WAYPOINTS_8(dst, src);
		EXCP_COPY_WAYPOINTS_8(dst, src);
		EXCP_COPY_WAYPOINTS_8(dst, src);
	}
}

#undef EXCP_SAVE_PMU_COUNTER
#undef EXCP_COPY_WAYPOINTS_8

/* run the handler in the kernel address space without debug events */
static inline void excpEnterKernel(void)
{
	const unsigned int *mmu;

	if (g_excpData.kernelBreakpoints != NULL) {
		CP_WRITE(p14, 0, c0, c0, 5, 0);
		CP_WRITE(p14, 0, c0, c1, 5, 0);
		CP_WRITE(p14, 0, c0, c2, 5, 0);
		CP_WRITE(p14, 0, c0, c3, 5, 0);
		CP_WRITE(p14, 0, c0, c4, 5, 0);
		CP_WRITE(p14, 0, c0, c5, 5, 0);
	}
	if (g_excpData.kernelWatchpoints != NULL) {
		CP_WRITE(p14, 0, c0, c0, 7, 0);
		CP_WRITE(p14, 0, c0, c1, 7, 0);
		CP_WRITE(p14, 0, c0, c2, 7, 0);
		CP_WRITE(p14, 0, c0, c3, 7, 0);
	}
	mmu = g_excpData.kernelMmu;
	excpSetAddressSpace(mmu[0], mmu[2]);
	writeDacr(DACR_KERNEL);
	writeTpidrprw(0);
	writeTpidruro(0);
}

/*
 * Breakpoints and watchpoints of the thread being resumed (the kernel set
 * when it is an interrupt context) and the thread's context-ID breakpoint.
 */
static inline void excpRearmTrace(void)
{
	ExcpTraceArea *trace = excpTraceArea();

	if (trace != NULL)
		trace->control = 1;
}

static void __attribute__((noreturn)) excpNestedPanic(SceExcpmgrContext *context)
{
	for (;;)
		sceKernelPanicHandlerReturnFromNestedExceptionCfunc(context);
}

/* the interrupted state that an emulated instruction resumes with */
static inline void excpRestoreFaultState(SceExcpmgrContext *context)
{
	writeDfsr(context->dfsr);
	writeDfar(context->dfar);
	writeTpidruro(context->tpidruro);
	writeTpidrprw((unsigned int)context->thread);
	excpSetAddressSpace(context->ttbr1, context->contextId);
	writeDacr(context->dacr);
	writePar(context->par);
	writeFpexc(context->fpexc);
	writeFpscr(context->fpscr);
	writeCpacr(context->cpacr);
}

/* leave the exception: one less nesting level on this core */
static inline void excpLeave(SceExcpmgrContext *context)
{
	unsigned int cpu = excpCpu();

	if (g_excpData.nestCount[cpu] - 1 != 0)
		excpNestedPanic(context);
	g_excpData.nestCount[cpu] = 0;
}

/* one more nesting level: the handler chain runs on this core's exception stack */
static inline void *excpEnterNested(void)
{
	unsigned int cpu = excpCpu();

	g_excpData.nestCount[cpu]++;
	return g_excpData.stackTop[cpu];
}

