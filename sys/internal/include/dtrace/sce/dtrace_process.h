/*
 * What DTrace asks about processes (Sony's dtrace/sce/dtrace_process.c).
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <processmgr/processmgr.h>

#define DTRACE_KERNEL_PROCESS	0x10005		/* the kernel's process ID */

/* 3.6x gives a process its GUID name, at most 31 characters (4.00 the main module's file name) */
#if FW_SINCE(0x04000000)
#define DTRACE_PROCESS_NAME_LEN	256
#else
#define DTRACE_PROCESS_NAME_LEN	31
#endif
#define DTRACE_MODULE_NAME_LEN	256

/* dtrace_get_process_type */
#define DT_PROCESS_GAME		0
#define DT_PROCESS_SYSTEM	1
#define DT_PROCESS_KERNEL	2

int dtrace_get_process_type(pid_t pid);
void dtrace_get_current_process_name(char *name, size_t size);
void dtrace_get_current_process_execname(char *name, size_t size);
SceUID dtrace_get_current_process_module_at_address(uintptr_t addr);
void dtrace_get_module_name(SceUID moduleId, char *name, size_t size);

static inline pid_t dtrace_get_parent_pid(pid_t pid)
{
	return sceKernelGetProcessParentId(pid);
}
