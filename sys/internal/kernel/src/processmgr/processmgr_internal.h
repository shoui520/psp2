/*
 * SceProcessmgr internals shared by the module's sources.
 *
 * Source files follow Sony's (from the 4.00 debug records): processmgr.c and
 * budget.c; the kernel libc time wrappers at the end of the module have no
 * debug records (libctime.c, name unknown).
 */

#ifndef PROCESSMGR_INTERNAL_H
#define PROCESSMGR_INTERNAL_H

#include <scetypes.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>
#include <sysmem/procevent.h>
#include <threadmgr/threadcb.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>
#include <intrmgr/intrmgr.h>
#include <acmgr/acmgr.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/sysroot.h>
#include <sysmem/utils.h>
#include <excpmgr/excpmgr.h>
#include <modulemgr/modulemgr.h>
#include <processmgr/process.h>
#include <processmgr/processmgr.h>

#define ARRAY_SIZE(a)			(sizeof(a) / sizeof((a)[0]))

#define KERNEL_PID			0x10005
#define KERNEL_ADDRESS_SPACE		0x10007
#define KERNEL_HEAP			0x1000b
#define TOOL_HEAP			0x1002b	/* the tool heap (dipsw 0xd2) */
#define SHELL_DUMMY_PID			0x10013	/* dummy processes (dipsw 0xc2) */
#define NPDRM_DUMMY_PID			0x10015

/* SceProcessObject.attr */
#define PROCESS_ATTR_LOAD_DEBUG		0x00010000	/* name unknown: LoadProcessImage flag */
#define PROCESS_ATTR_NOT_ACTIVE		0x00400000	/* not counted in the budget's active processes */
#define PROCESS_ATTR_PRIORITY		0x10000000	/* the given main thread priority */
#define PROCESS_ATTR_STACK_SIZE		0x20000000	/* the given main thread stack size */
#define PROCESS_ATTR_LOAD_FLAG		0x40000000
#define PROCESS_ATTR_SUSPENDED		0x80000000	/* created suspended (preload) */

/* the process event flag (SceProcessObject.eventFlag) */
#define PROCESS_EVF_CREATED		0x00000001
#define PROCESS_EVF_STARTED		0x00000002
#define PROCESS_EVF_SUSPENDED		0x00000010
#define PROCESS_EVF_RESUMED		0x00000020
#define PROCESS_EVF_DELETED		0x00000100
#define PROCESS_EVF_NOT_DEBUG_SUSPENDED	0x00010000
#define PROCESS_EVF_DEBUG_RESUME	0x10000000

/* requests queued to the process event thread */
#define PROCESS_REQUEST_EXIT		0x01
#define PROCESS_REQUEST_KILL		0x02
#define PROCESS_REQUEST_SUSPEND		0x04
#define PROCESS_REQUEST_RESUME		0x08
#define PROCESS_REQUEST_DEBUG_SUSPEND	0x10
#define PROCESS_REQUEST_FORCE_KILL	0x20
#define PROCESS_REQUEST_EXIT_SPAWN	0x40
#define PROCESS_REQUEST_FORCE_SUSPEND	0x80

/* process event handler events: sysmem/procevent.h */

#define PROCESS_THREAD_VECTOR_MAX	192

_Static_assert(sizeof(ProcessBudget) == 0x74, "ProcessBudget size");

#define THREAD_ATTR_COREDUMP_HANDLER	0x800000	/* name unknown */

/* the system call state in TPIDRURO (see the module manager) */
static inline SceUInt32 ProcessSyscallEnter(void)
{
	SceUInt32 t;

	__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(t));
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(t << 16) : "memory");
	return t;
}

static inline void ProcessSyscallLeave(SceUInt32 t)
{
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(t) : "memory");
}

/* processmgr.c */
extern SceClass g_processClass;
SceProcessObject *GetCurrentProcess(void);

/* libctime.c */
SceUInt32 ProcessmgrLibcClock(void);
int ProcessmgrLibcGmtime_r(const SceKernelTime *t, void *tm);
int ProcessmgrLibcLocaltime_r(const SceKernelTime *t, void *tm);
int ProcessmgrLibcMktime(void *tm, SceKernelTime *t, SceUInt64 *wide);

/* budget.c */
extern ProcessBudget *g_gameBudget;
extern ProcessBudget *g_kernelBudget;
int BudgetLock(ProcessBudget *budget);
int BudgetUnlock(ProcessBudget *budget);
SceUID BudgetGetKernelBudgetId(void);
int BudgetIDtoBudgetObj(SceUID budgetId, ProcessBudget **budget);
int SetBudget(SceUInt32 type, SceUID addressSpace, ProcessBudget *budget);
int CreateDefaultBudget(void);

#endif
