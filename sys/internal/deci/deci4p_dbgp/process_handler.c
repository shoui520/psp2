/*
 * The debugger's process event handlers, called through SceSysroot
 * (Sony's process_handler.c). Each queues a ProcessInfo for the worker.
 *
 * Where nothing is queued Sony's handlers return what the last call left in
 * r0 (the control block they looked up, or a print's count): the explicit
 * returns below keep those values.
 */

#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include "dbgp_internal.h"

/* the SceSysroot spontaneous exit statuses the debugger stops on */
#define SPONTANEOUS_EXIT_FIRST	0x106
#define SPONTANEOUS_EXIT_COUNT	5

/* DbgpProcCB.prxDebugFlag: follow the process into the one it spawns */
#define PRX_DEBUG_FOLLOW_SPAWN	0x4

int processHandlerInit(DbgpCB *dbgp)
{
	return 0;
}

int processHandlerStartHandler(SceUID parentPid, SceUID pid, SceUInt64 time)
{
	ProcessInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(0);
	if (proc == NULL)
		return 0;
	info.type = PROCESS_INFO_START;
	info.pid = pid;
	info.parentPid = parentPid;
	info.exitStatus = 0;
	info.time = time;
	return processInfoSave(proc, &info);
}

int processHandlerExitHandler(SceUID pid, int exitStatus, SceUInt64 time)
{
	ProcessInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL || proc->pid != pid)
		return (int)proc;
	info.type = PROCESS_INFO_EXIT;
	info.pid = proc->pid;
	info.parentPid = -1;
	info.exitStatus = exitStatus;
	info.time = time;
	swbkptClearSwbrkptProcess(proc, info.pid);
	return processInfoSave(proc, &info);
}

int processHandlerKillHandler(SceUID pid, SceUInt64 time)
{
	ProcessInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL || proc->pid != pid)
		return (int)proc;
	info.type = PROCESS_INFO_KILL;
	info.pid = proc->pid;
	info.parentPid = -1;
	info.exitStatus = 0;
	info.time = time;
	swbkptClearSwbrkptProcess(proc, info.pid);
	return processInfoSave(proc, &info);
}

int processHandlerSuspendHandler(SceUID pid)
{
	ProcessInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL)
		return 0;
	if (dbgpTestProcStatus(proc, 0x40000) == 0)
		dbgpSetProcStatus(proc, 0x40000, 1);
	info.type = PROCESS_INFO_SUSPEND;
	info.pid = pid;
	info.parentPid = 0;
	info.exitStatus = 0;
	info.time = sceKernelGetSystemTimeWide();
	return processInfoSave(proc, &info);
}

int processHandlerResumeHandler(SceUID pid)
{
	ProcessInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL)
		return 0;
	info.type = PROCESS_INFO_RESUME;
	info.pid = pid;
	info.parentPid = 0;
	info.exitStatus = 0;
	info.time = sceKernelGetSystemTimeWide();
	return processInfoSave(proc, &info);
}

int processHandlerAppAbortHandler(SceUID pid)
{
	ProcessInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL)
		return 0;
	info.type = PROCESS_INFO_APP_ABORT;
	info.pid = pid;
	info.parentPid = 0;
	info.exitStatus = 0;
	info.time = sceKernelGetSystemTimeWide();
	return processInfoSave(proc, &info);
}

int processHandlerSpontaneousExitHandler(SceUID pid, SceUInt32 status,
					 SceUInt64 time)
{
	ProcessInfo info;
	DbgpProcCB *proc;
	int result;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL ||
	    status - SPONTANEOUS_EXIT_FIRST > SPONTANEOUS_EXIT_COUNT - 1)
		return (int)proc;
	info.type = PROCESS_INFO_SPONTANEOUS_EXIT;
	info.pid = pid;
	info.parentPid = 0;
	info.exitStatus = 0;
	info.time = time;
	proc->currentThread = sceKernelGetThreadId();
	processInfoSave(proc, &info);
	result = sceKernelWaitSema(proc->spontaneousExitSema, 1, NULL);
	if (result < 0)
		return SCE_PRINTF_WITH_INFO(15, 301, "0x%08x\n", result);
	return result;
}

int processHandlerExitToSpawnHandler(SceUID pid, SceUID spawnPid,
				     SceUInt64 time)
{
	ProcessInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL || proc->pid != pid ||
	    (proc->prxDebugFlag & PRX_DEBUG_FOLLOW_SPAWN) == 0)
		return (int)proc;
	info.type = PROCESS_INFO_EXIT_TO_SPAWN;
	info.pid = spawnPid;
	info.parentPid = proc->pid;
	info.exitStatus = 0;
	info.time = time;
	return processInfoSave(proc, &info);
}

int processHandlerExitSpawnHandler(SceUID pid, SceUID spawnPid, int result,
				   SceUInt64 time)
{
	ProcessInfo info;
	DbgpProcCB *proc;
	DbgpCB *dbgp;

	dbgp = dbgpGetDbgpCB();
	if (dbgp == NULL)
		return 0;
	proc = dbgpGetProcCB(0);
	if (proc == NULL ||
	    (dbgp->spawn.prxDebugFlag & PRX_DEBUG_FOLLOW_SPAWN) == 0)
		return (int)proc;
	info.type = PROCESS_INFO_EXIT_SPAWN;
	info.pid = spawnPid;
	info.parentPid = pid;
	info.exitStatus = result;
	info.time = time;
	return processInfoSave(proc, &info);
}
