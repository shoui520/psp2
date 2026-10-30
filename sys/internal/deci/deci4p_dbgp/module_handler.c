/*
 * The debugger's SceKernelModulemgr handlers: a module started or stopped
 * in a debugged process is queued for the worker (Sony's module_handler.c).
 */

#include "dbgp_internal.h"

int moduleHandlerInit(DbgpCB *dbgp)
{
	return 0;
}

int moduleHandlerStartHandler(SceUID pid, SceUID modid, SceUInt64 time)
{
	ModuleInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL || proc->pid != pid)
		return (int)proc;
	info.pid = proc->pid;
	info.modid = modid;
	info.time.low = (SceUInt32)time;
	info.time.high = (SceUInt32)(time >> 32);
	info.type = MODULE_INFO_START;
	return moduleInfoSave(proc, &info);
}

int moduleHandlerStopHandler(SceUID pid, SceUID modid, SceUInt64 time)
{
	ModuleInfo info;
	DbgpProcCB *proc;

	proc = dbgpGetProcCB(pid);
	if (proc == NULL || proc->pid != pid)
		return (int)proc;
	info.pid = proc->pid;
	info.modid = modid;
	info.time.low = (SceUInt32)time;
	info.time.high = (SceUInt32)(time >> 32);
	info.type = MODULE_INFO_STOP;
	swbkptClearSwbrkptModule(proc, info.pid, modid);
	return moduleInfoSave(proc, &info);
}
