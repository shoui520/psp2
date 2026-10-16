/*
 * SceSysLibTrace internals shared by its source files.
 */

#pragma once

#include "sce/syslibtrace.h"

#ifndef SCE_KERNEL_ERROR_ILLEGAL_ADDR
#define SCE_KERNEL_ERROR_ILLEGAL_ADDR	0x80020006
#endif

extern SceSyslibtraceSyscallInfo g_syslibtraceSyscallInfo[SCE_SYSLIBTRACE_NUM_SYSCALLS];

/* syslibtrace_trampoline.S: the syscall handler of traced functions */
void SyslibtraceSyscallTrampoline(void);

int SyslibtraceInitialize(void);

#ifndef SCE_ERROR_ERRNO_EPERM
#define SCE_ERROR_ERRNO_EPERM		0x80010001
#endif
