/*
 * The debugger's exception hooks: prefetch aborts (breakpoints, DTrace
 * probes), data aborts (watchpoints) and undefined instructions in the
 * processes it debugs (Sony's excp_handler.c).
 */

#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <deci4p_dbgp/dbgp.h>
#include "dbgp_internal.h"

/* what an exception hook returns to SceExcpmgr */
#define EXCP_HANDLED		1
#define EXCP_SUSPENDED		2

/* the process SceExcpmgr reports for the kernel */
#define EXCP_KERNEL_PID		0x10005

/* the DTrace USDT probe instructions: bkpt 0x90 */
#define DTRACE_USDT_ARM		0xe1200970
#define DTRACE_USDT_THUMB	0xbe90

/* the PC bit of sceKernelSetCpuRegisterForDebugger's register mask */
#define DBGP_REGISTER_PC	0x8000

extern SceExcpmgrHandler dbgpPrefetchAbortEntry;
extern SceExcpmgrHandler dbgpDataAbortEntry;
extern SceExcpmgrHandler dbgpUndefinedEntry;

static SceKernelSpinlock s_excpInfoLock;
static SceKernelSpinlock s_suspendLock;
static DbgpDTraceHandler s_dtraceBreakpointHandler;
static DbgpDTraceHandler s_dtraceUsdtHandler;

int excpHandlerInit(DbgpCB *dbgp)
{
	int result;

	s_excpInfoLock = 0;
	s_suspendLock = 0;
	result = sceKernelRegisterExceptionHandler(SCE_EXCP_PABT, 5,
						   &dbgpPrefetchAbortEntry);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 49, "0x%08x\n", result);
		return result;
	}
	result = sceKernelRegisterExceptionHandler(SCE_EXCP_DABT, 5,
						   &dbgpDataAbortEntry);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 59, "0x%08x\n", result);
		return result;
	}
	result = sceKernelRegisterExceptionHandler(SCE_EXCP_UNDEF, 5,
						   &dbgpUndefinedEntry);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 69, "0x%08x\n", result);
		return result;
	}
	return 0;
}

void sceDbgpSetDTraceBreakpointHandler(DbgpDTraceHandler handler)
{
	s_dtraceBreakpointHandler = handler;
}

DbgpDTraceHandler sceDbgpGetDTraceBreakpointHandler(void)
{
	return s_dtraceBreakpointHandler;
}

void sceDbgpSetDTraceUsdtHandler(DbgpDTraceHandler handler)
{
	s_dtraceUsdtHandler = handler;
}

DbgpDTraceHandler sceDbgpGetDTraceUsdtHandler(void)
{
	return s_dtraceUsdtHandler;
}

int excpHandlerPrefetchAbort(SceExcpmgrContext *context, SceUInt32 pc,
			     SceUInt32 cpsr, int code)
{
	SceUInt16 thumbOpcode;
	SceUInt32 kind;
	int isThumb;
	SceUInt32 armOpcode;
	DbgpThreadIdentity thread;
	ThreadmgrDebugRegisters registers[2];
	int cause, id, result;
	DbgpProcCB *proc;

	kind = 0;
	isThumb = 0;
	if (code == 4 || sceKernelIsIntrContext() > 0)
		return code;
	result = sceKernelGetThreadContextInfo((ThreadCBInfo *)&thread);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 155, "0x%08x\n", result);
		return EXCP_SUSPENDED;
	}
	if (thread.pid == EXCP_KERNEL_PID || (cpsr & 0x1f) == 0x1f)
		return code;

	/* a USDT probe the DTrace provider takes is stepped over */
	if (checkGetTargetAddrOpcode((const void *)pc, cpsr, thread.pid,
				     &armOpcode, &thumbOpcode, &isThumb) >= 0 &&
	    ((isThumb != 0 && thumbOpcode == DTRACE_USDT_THUMB) ||
	     (isThumb == 0 && armOpcode == DTRACE_USDT_ARM)) &&
	    s_dtraceUsdtHandler != NULL &&
	    s_dtraceUsdtHandler(thread.pid, thread.thid, pc,
				context->sp, cpsr, armOpcode, thumbOpcode,
				isThumb) >= 0) {
		registers[0].pc = pc + (isThumb == 0 ? 4 : 2);
		return sceKernelSetCpuRegisterForDebugger(thread.thid, registers,
							  DBGP_REGISTER_PC) < 0;
	}

	proc = dbgpGetProcCB(thread.pid);
	if (proc == NULL) {
		result = SceThreadmgrForDriver_04C6764B(thread.thid, 0x800);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 222, "0x%08x\n", result);
		sceKernelInitialHandlerDebugPabtCfunc(context);
		return EXCP_HANDLED;
	}
	cause = checkGetTargetAddrStopCause(proc, (const void *)pc, cpsr,
					    thread.pid, &kind);
	if (cause < 0) {
		SCE_PRINTF_WITH_INFO(15, 233, "0x%08x\n", cause);
		return EXCP_SUSPENDED;
	}
	if (cause == 1) {
		sceKernelInitialHandlerDebugPabtCfunc(context);
	} else {
		if (dbgpTestProcStatus(proc, 0x1000) != 0 &&
		    (cause == 32 || cause == 33))
			dbgpSetProcStatus(proc, 0x20000, 1);
		if (cause == 34 || cause == 35) {
			id = swbkptGetAlignId(proc, (const void *)pc,
					      thread,
					      &kind);
			if (id < 0) {
				SCE_PRINTF_WITH_INFO(15, 262,
						     "0x%08x:0x%08x:0x%08x\n", pc,
						     thread.pid,
						     thread.thid);
				return (int)0x80080001;
			}
			if (kind == 0)
				proc->swbkpt[id].hitCount++;
		} else if (cause == 40) {
			if (s_dtraceBreakpointHandler == NULL)
				cause = 32;
		} else if (cause == 41) {
			if (s_dtraceBreakpointHandler == NULL)
				cause = 33;
		}
	}

	sceKernelSpinlockLowLock(&s_suspendLock);
	if (procSuspendProcess(proc, thread.pid, thread.thid,
			       cause) < 0) {
		sceKernelSpinlockLowUnlock(&s_suspendLock);
		return EXCP_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(&s_suspendLock);
	dbgpSetProcStatus(proc, 0x10, 1);
	sceKernelSpinlockLowLock(&s_excpInfoLock);
	if ((cause == 40 || cause == 41) && dbgpTestProcStatus(proc, 0x20) == 0 &&
	    s_dtraceBreakpointHandler != NULL)
		s_dtraceBreakpointHandler(thread.pid, thread.thid, pc,
					  context->sp, cpsr, armOpcode,
					  thumbOpcode, isThumb);
	result = excpInfoSave(proc, &thread, 0, pc,
			      kind);
	sceKernelSpinlockLowUnlock(&s_excpInfoLock);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 329, "0x%08x\n", result);
	return EXCP_SUSPENDED;
}

int excpHandlerDataAbort(SceExcpmgrContext *context, SceUInt32 pc,
			 SceUInt32 cpsr, int code)
{
	DbgpThreadIdentity thread;
	SceUID pid, thid;
	DbgpProcCB *proc;
	int cause, result;

	if (code == 4 || sceKernelIsIntrContext() > 0)
		return code;
	result = sceKernelGetThreadContextInfo((ThreadCBInfo *)&thread);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 364, "0x%08x\n", result);
		return EXCP_SUSPENDED;
	}
	pid = thread.pid;
	thid = thread.thid;
	if (pid == EXCP_KERNEL_PID || (cpsr & 0x1f) == 0x1f)
		return code;
	proc = dbgpGetProcCB(pid);
	if (proc == NULL) {
		sceKernelInitialHandlerDebugDabtCfunc(context);
		return EXCP_HANDLED;
	}
	cause = checkGetTargetAddrStopCauseDabt(proc, (const void *)pc, cpsr,
						thread.pid);
	if (cause < 0) {
		SCE_PRINTF_WITH_INFO(15, 388, "0x%08x\n", cause);
		return EXCP_SUSPENDED;
	}
	if (cause == 2)
		sceKernelInitialHandlerDebugDabtCfunc(context);
	sceKernelSpinlockLowLock(&s_suspendLock);
	if (procSuspendProcess(proc, pid, thid,
			       cause) < 0) {
		sceKernelSpinlockLowUnlock(&s_suspendLock);
		return EXCP_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(&s_suspendLock);
	dbgpSetProcStatus(proc, 0x10, 1);
	result = excpInfoSave(proc, &thread, 0, pc, 0);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 420, "0x%08x\n", result);
	return EXCP_SUSPENDED;
}

int excpHandlerUndefined(SceExcpmgrContext *context, SceUInt32 pc,
			 SceUInt32 cpsr, int code)
{
	DbgpThreadIdentity thread;
	SceUID pid, thid;
	DbgpProcCB *proc;
	int result;

	if (code == 4 || sceKernelIsIntrContext() > 0)
		return code;
	result = sceKernelGetThreadContextInfo((ThreadCBInfo *)&thread);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 458, "0x%08x\n", result);
		return EXCP_SUSPENDED;
	}
	pid = thread.pid;
	thid = thread.thid;
	if (pid == EXCP_KERNEL_PID || (cpsr & 0x1f) == 0x1f)
		return code;
	proc = dbgpGetProcCB(pid);
	if (proc == NULL) {
		sceKernelInitialHandlerDebugUndefCfunc(context);
		return EXCP_HANDLED;
	}
	sceKernelInitialHandlerDebugUndefCfunc(context);
	sceKernelSpinlockLowLock(&s_suspendLock);
	if (procSuspendProcess(proc, pid, thid, 3) < 0) {
		sceKernelSpinlockLowUnlock(&s_suspendLock);
		return EXCP_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(&s_suspendLock);
	dbgpSetProcStatus(proc, 0x10, 1);
	result = excpInfoSave(proc, &thread, 0, pc, 0);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 501, "0x%08x\n", result);
	return EXCP_SUSPENDED;
}
