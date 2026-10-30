/*
 * The debugger's SceThreadmgr handlers for errors a thread raises without
 * an exception: VFP, stack overflow, bad syscalls, GPU faults and a declared
 * suicide (Sony's intr_handler.c). Each records a stop for the process.
 */

#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "dbgp_internal.h"

/* what a handler of group 1 returns to SceThreadmgr */
#define INTR_IGNORED		1
#define INTR_STOPPED		2

/* the handler group that decides what happens to the thread */
#define INTR_GROUP_DECIDE	1

/* the stop reasons excpInfoSave records */
#define INTR_REASON_VFP			0x1000
#define INTR_REASON_STACK_OVERFLOW	0x10000
#define INTR_REASON_ILLEGAL_CONTEXT	0x20000
#define INTR_REASON_CRITICAL_USAGE	0x40000
#define INTR_REASON_GPU_EXCEPTION	0x80000
#define INTR_REASON_ILLEGAL_NUMBER	0x100000
#define INTR_REASON_DECLARE_SUICIDE	0x200000

/* the size of SceThreadmgr's VFP exception message */
#define INTR_VFP_MESSAGE_SIZE	0x210

static int _intrThreadHandlerApiDeclareSuicide(SceSize size, void *message)
{
	DbgpThreadIdentity thread;
	SceUInt32 reason;
	DbgpProcCB *proc;
	int result;

	if (size != 0 && message != NULL)
		reason = *(SceUInt32 *)message | INTR_REASON_DECLARE_SUICIDE;
	else
		reason = INTR_REASON_DECLARE_SUICIDE;
	thread.pid = 0;
	thread.thid = 0;
	result = sceKernelGetThreadContextInfo((ThreadCBInfo *)&thread);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 477, "0x%08x\n", result);
		return INTR_STOPPED;
	}
	proc = dbgpGetProcCB(thread.pid);
	if (proc == NULL || proc->intrStopReason == 54)
		return INTR_IGNORED;
	proc->intrStopReason = 54;
	dbgpSetProcStatus(proc, 0x10, 1);
	result = excpInfoSave(proc, &thread, reason, 0, 0);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 500, "0x%08x\n", result);
	return INTR_STOPPED;
}

static int _intrThreadHandlerSyscallErrorIllegalNumber(SceSize size,
						       void *message)
{
	DbgpThreadIdentity thread;
	DbgpProcCB *proc;
	int result;

	thread.pid = 0;
	thread.thid = 0;
	result = sceKernelGetThreadContextInfo((ThreadCBInfo *)&thread);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 431, "0x%08x\n", result);
		return INTR_STOPPED;
	}
	proc = dbgpGetProcCB(thread.pid);
	if (proc == NULL || proc->intrStopReason == 53)
		return INTR_IGNORED;
	proc->intrStopReason = 53;
	dbgpSetProcStatus(proc, 0x10, 1);
	result = excpInfoSave(proc, &thread, INTR_REASON_ILLEGAL_NUMBER, 0, 0);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 454, "0x%08x\n", result);
	return INTR_STOPPED;
}

static int _intrThreadHandlerSyscallErrorCriticalUsage(SceSize size,
						       void *message)
{
	DbgpThreadIdentity thread;
	DbgpProcCB *proc;
	int result;

	thread.pid = 0;
	thread.thid = 0;
	result = sceKernelGetThreadContextInfo((ThreadCBInfo *)&thread);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 347, "0x%08x\n", result);
		return 0;
	}
	proc = dbgpGetProcCB(thread.pid);
	if (proc == NULL)
		return 0;
	dbgpSetProcStatus(proc, 0x10, 1);
	result = excpInfoSave(proc, &thread, INTR_REASON_CRITICAL_USAGE, 0, 0);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 366, "0x%08x\n", result);
	return INTR_STOPPED;
}

static int _intrThreadHandlerSyscallErrorIllegalContext(SceSize size,
							void *message)
{
	DbgpThreadIdentity thread;
	DbgpProcCB *proc;
	int result;

	thread.pid = 0;
	thread.thid = 0;
	result = sceKernelGetThreadContextInfo((ThreadCBInfo *)&thread);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 303, "0x%08x\n", result);
		return INTR_STOPPED;
	}
	proc = dbgpGetProcCB(thread.pid);
	if (proc == NULL || proc->intrStopReason == 50)
		return INTR_IGNORED;
	proc->intrStopReason = 50;
	dbgpSetProcStatus(proc, 0x10, 1);
	result = excpInfoSave(proc, &thread, INTR_REASON_ILLEGAL_CONTEXT, 0, 0);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 326, "0x%08x\n", result);
	return INTR_STOPPED;
}

static int _intrThreadHandlerStackOverflow(SceSize size, void *message)
{
	DbgpThreadIdentity thread;
	DbgpProcCB *proc;
	int result;

	thread.pid = 0;
	thread.thid = 0;
	result = sceKernelGetThreadContextInfo((ThreadCBInfo *)&thread);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 260, "0x%08x\n", result);
		return INTR_STOPPED;
	}
	proc = dbgpGetProcCB(thread.pid);
	if (proc == NULL || proc->intrStopReason == 49)
		return INTR_IGNORED;
	proc->intrStopReason = 49;
	dbgpSetProcStatus(proc, 0x10, 1);
	result = excpInfoSave(proc, &thread, INTR_REASON_STACK_OVERFLOW, 0, 0);
	if (result < 0)
		SCE_PRINTF_WITH_INFO(15, 283, "0x%08x\n", result);
	return INTR_STOPPED;
}

static int _intrThreadHandlerGpuException(SceSize size, void *message)
{
	DbgpThreadIdentity thread;
	DbgpProcCB *proc;
	int result;

	if (message != NULL && size == sizeof(SceUID)) {
		thread.pid = *(SceUID *)message;
		thread.thid = -1;
		proc = dbgpGetProcCB(thread.pid);
		if (proc != NULL) {
			dbgpSetProcStatus(proc, 0x10, 1);
			result = excpInfoSave(proc, &thread,
					      INTR_REASON_GPU_EXCEPTION, 0, 0);
			if (result < 0)
				SCE_PRINTF_WITH_INFO(15, 411, "0x%08x\n",
						     result);
			return INTR_STOPPED;
		}
	}
	return INTR_IGNORED;
}

static int _intrThreadHandlerVfp(SceSize size, void *message)
{
	DbgpThreadIdentity thread;
	DbgpProcCB *proc;
	int result;

	if (message != NULL && size == INTR_VFP_MESSAGE_SIZE) {
		thread = *(DbgpThreadIdentity *)message;
		proc = dbgpGetProcCB(thread.pid);
		if (proc != NULL) {
			dbgpSetProcStatus(proc, 0x10, 1);
			result = excpInfoSave(proc, &thread, INTR_REASON_VFP, 0, 0);
			if (result < 0)
				SCE_PRINTF_WITH_INFO(15, 234, "0x%08x\n",
						     result);
			return INTR_STOPPED;
		}
	}
	return INTR_IGNORED;
}

int intrHandlerInit(DbgpCB *dbgp)
{
	int result;

	result = SceThreadmgrForDriver_6C2E3A49(INTR_GROUP_DECIDE, 1,
						_intrThreadHandlerVfp);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 53, "0x%08x\n", result);
		return result;
	}
	result = SceThreadmgrForDriver_6C2E3A49(INTR_GROUP_DECIDE, 3,
						_intrThreadHandlerStackOverflow);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 63, "0x%08x\n", result);
		return result;
	}
	result = SceThreadmgrForDriver_6C2E3A49(INTR_GROUP_DECIDE, 4,
			_intrThreadHandlerSyscallErrorIllegalContext);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 73, "0x%08x\n", result);
		return result;
	}
	result = SceThreadmgrForDriver_6C2E3A49(INTR_GROUP_DECIDE, 5,
			_intrThreadHandlerSyscallErrorCriticalUsage);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 83, "0x%08x\n", result);
		return result;
	}
	result = SceThreadmgrForDriver_6C2E3A49(INTR_GROUP_DECIDE, 2,
						_intrThreadHandlerGpuException);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 93, "0x%08x\n", result);
		return result;
	}
	result = SceThreadmgrForDriver_6C2E3A49(INTR_GROUP_DECIDE, 6,
			_intrThreadHandlerSyscallErrorIllegalNumber);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 103, "0x%08x\n", result);
		return result;
	}
	result = SceThreadmgrForDriver_6C2E3A49(INTR_GROUP_DECIDE, 7,
			_intrThreadHandlerApiDeclareSuicide);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 113, "0x%08x\n", result);
		return result;
	}
	return 0;
}
