/*
 * SceKernelThreadMgr: the calling context of an export (TPIDRURO's syscall
 * flag, the caller's SDK version).
 */

#ifndef THREADMGR_SYSCALL_CONTEXT_H
#define THREADMGR_SYSCALL_CONTEXT_H

#include <stdint.h>
#include <scetypes.h>
#include "private.h"
#include <sysmem/sysroot.h>

#define USERRO_IN_SYSCALL		0x00010000	/* TPIDRURO: serving a user syscall */
#define SYSCALL_ERROR_BIT		0x40000000	/* set in errors returned to a syscall */
#define THREADMGR_KERNEL_PID		0x10005
#define SDK_VERSION_KERNEL		THREADMGR_KERNEL_SYSVER	/* kernel callers: the firmware's SDK (0x03650011 on 3.65) */

static inline int ThreadmgrIntrDisabled(void)
{
	uint32_t cpsr;

	__asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
	return (cpsr >> 7) & 1;
}

/* TPIDRURO: exports save it and shift it left 16 for their duration */
static inline uint32_t ThreadmgrUserRoWord(void)
{
	uint32_t value;

	__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(value));
	return value;
}

static inline void ThreadmgrSetUserRoWord(uint32_t value)
{
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" : : "r"(value));
}

static inline int ThreadmgrInSyscall(void)
{
	return (ThreadmgrUserRoWord() & USERRO_IN_SYSCALL) != 0;
}

/* the SDK version the calling thread's process was built with */
static inline void ThreadmgrCallerSdkVersion(struct ThreadCB *thread,
					     SceUInt32 *version)
{
	if (thread == NULL || thread->processId == THREADMGR_KERNEL_PID)
		*version = SDK_VERSION_KERNEL;
	else if (SceSysrootForDriver_CAE820B2(thread->processId, version) < 0)
		*version = 0;
}

#endif
