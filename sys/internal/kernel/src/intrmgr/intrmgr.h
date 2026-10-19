/* Interrupt controller state shared with the CPU exception entry. */
#pragma once

#include <sysmem/cp15.h>
#include <sysmem/barrier.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <intrmgr/intrmgr.h>
#include <excpmgr/excpmgr.h>
#include <threadmgr/threadcb.h>
#include <processmgr/process.h>
#include <intrmgr/gic_regs.h>
/* the addresses of the GIC (integers: a store to a word may change them) */
extern SceUInt32 g_intrCpuInterface;
extern SceUInt32 g_intrDistributor;
#define GICD_WORD(offset) (*(volatile SceUInt32 *)(g_intrDistributor + (offset)))
#define GICD_BYTE(offset) (*(SceUInt8 *)(g_intrDistributor + (offset)))
#define GICC_WORD(offset) (*(volatile SceUInt32 *)(g_intrCpuInterface + (offset)))
#define INTR_GIC_REGISTERS
#include "intrmgr_state.h"

/* name unknown: update the interrupt configuration field. */
static inline void SetConfiguration(unsigned int intrCode, unsigned int config)
{
	volatile SceUInt32 *configuration = (volatile SceUInt32 *)g_intrDistributor;
	unsigned int index = 0x300 + (intrCode >> 4);
	unsigned int shift = (intrCode & 15) * 2;
	unsigned int value = configuration[index];

	configuration[index] = value ^ (((value >> shift) ^ config) & 3) << shift;
}
#include <sysmem/suspend.h>

#define INTR_CPU_STACK_SIZE 0x20000
#define INTR_LEVEL_STACK_SIZE 0x2000
#define INTR_STACK_MAGIC 0x1ab4e8dfU

typedef struct IntrIrqSystem {
	SceUInt32 translationBase;
	SceUInt32 dacr;
	SceUInt32 contextId;
	SceUInt32 physicalAddress;
} IntrIrqSystem;

typedef struct IntrIrqFrame {
	IntrIrqSystem system;
	void *thread;
	SceUInt32 pmuEnabled;
	SceUInt32 reserved[2];			/* stack space the entry leaves unwritten */
	SceUInt32 registers[5];
	SceUInt32 ip;
	SceUInt32 returnAddress;
	SceUInt32 returnStatus;
} IntrIrqFrame;

typedef struct IntrLevelStack {
	SceUInt32 guard;
	SceUInt8 storage[INTR_LEVEL_STACK_SIZE - sizeof(SceUInt32)];
} IntrLevelStack;

/* Independently addressed controller objects in Sony's BSS order. */
typedef union IntrStackGuard {
	void *beforeIntrHandler;		/* threadmgr's callThreadmgrBeforeIntrHandler */
	SceUInt32 outermostStackGuard;
} IntrStackGuard;

typedef struct SceSyscallTable {
	void *base;
	SceUID memBlock;
} SceSyscallTable;

extern IntrState g_intrState;
extern void *g_intrSysroot;
extern SceUInt32 g_intrReserved2[15];
extern ProcessHwBreakpoints *g_intrKernelBreakpoints;
extern ProcessHwWatchpoints *g_intrKernelWatchpoints;
extern struct ThreadMgrCpuCB *g_intrThreadmgrCpu[4];
extern SceUInt32 g_intrReserved1[10];
extern IntrLevelStack g_intrStacks[64];
extern IntrStackGuard g_intrStackGuard;
extern void *g_intrDispatchPending;
extern void *g_intrSetupNextContext;
extern SceUInt32 g_intrReserved0[13];
extern void *g_intrEnterSubIntr;
extern void *g_intrLeaveIntr;
extern void *g_intrEnterSyscall;
extern void *g_intrLeaveSyscall;

extern SceSyscallTable g_intrSyscall;
#define g_intrSyscallTable (g_intrSyscall.base)
#define g_intrSyscallBlock (g_intrSyscall.memBlock)
extern void *g_intrLeaveSubIntr;
extern void *g_intrEnterIntr;
extern int (*g_intrEvent)(unsigned int, unsigned int, void *);
extern void (*g_intrRegistered)(unsigned int);
extern SceUID g_intrHeap;
extern void (*g_intrReleased)(unsigned int);

/*
 * name unknown: the guard word at the bottom of one per-core nesting stack.
 * The stacks of a core run down from g_intrStackGuard (the guard of the
 * outermost level of core 0), 16 levels of INTR_LEVEL_STACK_SIZE per core.
 */
static inline SceUInt32 *StackGuard(unsigned int cpu, unsigned int level)
{
	return (SceUInt32 *)((char *)&g_intrStackGuard - cpu * INTR_CPU_STACK_SIZE -
			     level * INTR_LEVEL_STACK_SIZE);
}

extern SceExcpmgrHandler interruptEntry;
extern SceExcpmgrHandler syscallEntry;
void initStackGuards(unsigned int magic);
int checkStackGuard(void);
int intrmgrSmc(unsigned int paddr, unsigned int command);
extern char intrEnterHook[], intrLeaveHook[], syscallEnterHook[], syscallLeaveHook[];
extern SceUInt32 intrEnterBranch, intrLeaveBranch;
extern SceUInt16 syscallEnterBranch, syscallLeaveBranch;
/* the same slots by the labels the cache clean and the store use */
extern SceUInt32 intrEnterBranchClean, intrLeaveBranchClean, intrEnterBranchPatch, intrLeaveBranchPatch;
extern SceUInt16 syscallEnterBranchClean, syscallLeaveBranchClean, syscallEnterBranchPatch, syscallLeaveBranchPatch;

int callIntrEvent(unsigned int event, unsigned int size, void *data);
void invalidApiStatCfunc(const ThreadSyscallFrame *frame, void *result, unsigned int state);
void invalidFastMutexOwnerCfunc(void);

/* intrmgr.c */
void illegalUserDACRCfunc(const ThreadCB *thread, unsigned int dacr);
