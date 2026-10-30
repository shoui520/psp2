/*
 * What DTrace asks about processes (Sony's dtrace/sce/dtrace_process.c): the
 * current process's name and modules, and what kind of program it is.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/utility.h>
#include <dtrace/sce/dtrace_process.h>
#include <dtrace/sce/dtrace_security.h>
#include <acmgr/acmgr.h>
#include <intrmgr/intrmgr.h>
#include <modulemgr/modulemgr.h>
#include <processmgr/processmgr.h>
#include <processmgr/process_info.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>

#define DTRACE_MODULE_PATH_LEN	0x100

SceUID dtrace_get_current_process_module_at_address(uintptr_t addr)
{
	SceUID pid;

	pid = dtrace_get_current_process_id();
	if (pid >= 0 && pid != DTRACE_KERNEL_PROCESS &&
	    dtrace_security_check_access_current_process_state() == 0)
		return sceKernelGetModuleIdByAddrForDebugger(pid, addr);
	return -1;
}

void dtrace_get_module_name(SceUID moduleId, char *name, size_t size)
{
	char path[DTRACE_MODULE_PATH_LEN];
	const char *base, *sep;

	if (size == 0)
		return;
	name[0] = '\0';
	SceModulemgrForKernel_79E761B5(moduleId, path, sizeof(path));
	if (path[0] == '\0') {
		base = SceSysmemForDriver_E655852F(moduleId);
		if (base[0] != '\0')
			strncpy(name, base, size);
	} else {
		/* Sony searches name, not path, for the backslash */
		sep = strrchr(name, '\\');
		if (sep == NULL)
			sep = strrchr(path, '/');
		base = sep != NULL ? sep + 1 : name;
		sep = strrchr(base, ':');
		if (sep != NULL)
			base = sep + 1;
		strncpy(name, base, size);
	}
	name[size - 1] = '\0';
}

void dtrace_get_current_process_name(char *name, size_t size)
{
	SceKernelProcessInfo info;
	char path[DTRACE_MODULE_PATH_LEN];
	const char *base, *sep;
	SceUID pid;
	int intr;

	pid = dtrace_get_current_process_id();
	if (size == 0)
		return;
	name[0] = '\0';
	if (pid < 0) {
		snprintf(name, size, "unknown");
	} else if (pid == DTRACE_KERNEL_PROCESS) {
		strncpy(name, "kernel", size);
	} else {
		if (dtrace_security_check_access_process(pid) == 0) {
			intr = sceKernelCpuSuspendIntr();
			info.size = sizeof(info);
			if (sceKernelGetProcessInfo(pid, &info) >= 0) {
				if (SceModulemgrForKernel_79E761B5(info.mainModule, path, sizeof(path)) < 0 ||
				    path[0] == '\0') {
					strncpy(name, SceSysmemForDriver_E655852F(info.mainModule), size);
				} else {
					sep = strrchr(path, '\\');
					if (sep == NULL)
						sep = strrchr(path, '/');
					base = sep != NULL ? sep + 1 : path;
					sep = strrchr(base, ':');
					if (sep != NULL)
						base = sep + 1;
					strncpy(name, base, size);
				}
			}
			sceKernelCpuResumeIntr(intr);
		}
		if (name[0] == '\0')
			snprintf(name, size, "0x%08x", pid);
	}
	name[size - 1] = '\0';
}

void dtrace_get_current_process_execname(char *name, size_t size)
{
	const char *guidName = NULL;
	SceUID pid;

	pid = dtrace_get_current_process_id();
	if (size == 0)
		return;
	name[0] = '\0';
	if (pid < 0) {
		snprintf(name, size, "unknown");
	} else if (pid == DTRACE_KERNEL_PROCESS) {
		strncpy(name, "SceKernelProcess", size);
	} else {
		if (dtrace_security_check_access_process(pid) == 0 &&
		    sceGUIDGetName(pid, &guidName) == 0 && guidName != NULL)
			strncpy(name, guidName, size);
		if (name[0] == '\0')
			snprintf(name, size, "0x%08x", pid);
	}
	name[size - 1] = '\0';
}

int dtrace_get_process_type(pid_t pid)
{
	if (pid == DTRACE_KERNEL_PROCESS)
		return DT_PROCESS_KERNEL;
	if (sceSblACMgrIsGameProgram(pid))
		return DT_PROCESS_GAME;
	return sceSblACMgrIsSystemProgram(pid) ? DT_PROCESS_SYSTEM : -1;
}

dtrace_helpers_t *dtrace_get_helpers(void)
{
	return dtrace_get_current_process()->p_dtrace_helpers;
}
