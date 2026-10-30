/*
 * Solaris processes (Sony's dtrace/sce/solaris_process.c): a proc_t is the
 * kernel's process object, found by its ID and held by a reference; locking
 * one for DTrace suspends it.
 */
#include <dtrace/logging.h>
#include <dtrace/sce/solaris_process.h>
#include <dtrace/utility.h>
#include <kernel/error.h>
#include <processmgr/processmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>
#include <sysmem/uid_guid.h>
#include <threadmgr/threadmgr.h>

pid_t proc_pid(proc_t *p)
{
	return p->pid;
}

pid_t proc_ppid(proc_t *p)
{
	return p->parentPid;
}

void proc_lock(proc_t *p)
{
	sceKernelWaitSema(p->lockSema, 1, NULL);
}

void proc_unlock(proc_t *p)
{
	sceKernelSignalSema(p->lockSema, 1);
}

proc_t *proc_find(pid_t pid)
{
	SceObjectBase *object = NULL;
	SceClass *processClass;
	int res;

	processClass = SceProcessmgrForKernel_98AE4BC8();
	if (pid == SCE_KERNEL_DUMMY_PROCESS_GAME_ID || pid == SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID)
		processClass = SceSysmemForDriver_85336A1C();
	else if (pid == 0)
		pid = sceKernelGetProcessId();

	res = sceGUIDReferObjectWithClass(pid, processClass, &object);
	if (res < 0 || object == NULL) {
		DTRACE_LOG_ERROR_AT(65, "failed to reference process 0x%08x (0x%08x)", pid, res);
		return NULL;
	}
	return (proc_t *)object;
}

int proc_rele(proc_t *p)
{
	int res;

	res = sceGUIDReleaseObject(p->pid);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(80, "failed to release process 0x%08x (0x%08x)", p->pid, res);
		return -1;
	}
	return 0;
}

proc_t *sprlock(pid_t pid)
{
	proc_t *p;
	int res;

	p = proc_find(pid);
	if (p == NULL)
		return NULL;

	dtrace_get_current_process_id();
	res = sceKernelProcessDebugSuspend(pid, 17);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(107, "failed to suspend process 0x%08x (0x%08x)", pid, res);

	proc_lock(p);
	return p;
}

void sprunlock(proc_t *p)
{
	SceUInt32 status;
	int res;

	proc_unlock(p);

	res = sceKernelGetProcessStatus(p->pid, &status);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(128, "failed to sceKernelGetProcessStatus (0x%08x)", res);
		return;
	}

	status = (SceUInt8)status;
	res = sceKernelProcessDebugResume(p->pid, status);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(136, "failed to sceKernelProcessDebugResume (0x%08x)", res);
		return;
	}

	proc_rele(p);
}

int uread(proc_t *p, void *kaddr, size_t len, uintptr_t uaddr)
{
	SceUID pid = p->pid;
	int res;

	if (len == 0)
		return 0;

	if (p->type == PROCESS_TYPE_KERNEL)
		return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;

	res = sceKernelCopyFromUserProc(pid, kaddr, (const void *)uaddr, len);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(169, "failed to read range of %d bytes from address 0x%08x in process 0x%08x (0x%08x)",
				    len, uaddr, pid, res);
	return res;
}

int uwrite(proc_t *p, void *kaddr, size_t len, uintptr_t uaddr)
{
	SceUID pid = p->pid;
	int res;

	if (len == 0)
		return 0;

	if (p->type == PROCESS_TYPE_KERNEL)
		return SCE_KERNEL_ERROR_INVALID_PROCESS_TYPE;

	res = sceKernelCopyToUserProcTextDomainForDBGP(pid, (void *)uaddr, kaddr, len);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(201, "failed to write range of %d bytes from address 0x%08x in process 0x%08x (0x%08x)",
				    len, uaddr, pid, res);
	sceKernelL1IcacheInvalidateEntireAllCore();
	return res;
}
