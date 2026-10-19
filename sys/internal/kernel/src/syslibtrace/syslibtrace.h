/*
 * SceSysLibTrace internals shared by its source files.
 */

#pragma once

#include <syslibtrace/syslibtrace.h>

extern SyscallTraceInfo g_syslibtraceSyscallInfo[SCE_SYSLIBTRACE_NUM_SYSCALLS];

/* syslibtrace_trampoline.S: the syscall handler of traced functions */
void SyslibtraceSyscallTrampoline(void);

int SyslibtraceInitialize(void);
