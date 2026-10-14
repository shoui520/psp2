/* Interrupt controller state shared with the CPU exception entry. */
#pragma once

#include "sce/cp15.h"
#include "sce/barrier.h"
#include <psp2kern/types.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include "sce/debug.h"
#include "sce/error.h"
#include "sce/sysmem.h"
#include "sce/sysroot.h"
#include "sce/intrmgr.h"
#include "sce/threadcb.h"
#include "sce/process.h"

#define INTR_COUNT 256
#define INTR_CPU_STACK_SIZE 0x20000
#define INTR_LEVEL_STACK_SIZE 0x2000
#define INTR_STACK_MAGIC 0x1ab4e8dfU

/* the GIC in the Cortex-A9 private memory region */
#define GIC_CPU_INTERFACE_OFFSET 0x100
#define GIC_DISTRIBUTOR_OFFSET 0x1000

/* ARM GIC distributor (PERIPHBASE + 0x1000) */
typedef struct GicDistributor {
	SceUInt32 control;		/* 0x000 */
	SceUInt32 type;			/* 0x004 */
	SceUInt32 reserved0[30];
	SceUInt32 group[32];		/* 0x080 */
	SceUInt32 setEnable[32];	/* 0x100 */
	SceUInt32 clearEnable[32];	/* 0x180 */
	SceUInt32 setPending[32];	/* 0x200 */
	SceUInt32 clearPending[32];	/* 0x280 */
	SceUInt32 active[32];		/* 0x300 */
	SceUInt32 reserved1[32];
	SceUInt8 priority[1024];	/* 0x400 */
	SceUInt8 target[1024];		/* 0x800 */
	SceUInt32 config[64];		/* 0xc00 two bits per interrupt */
	SceUInt32 reserved2[128];
	SceUInt32 softIntr;		/* 0xf00 */
} GicDistributor;

/* ARM GIC CPU interface (PERIPHBASE + 0x100) */
typedef struct GicCpuInterface {
	SceUInt32 control;		/* 0x00 */
	SceUInt32 priorityMask;		/* 0x04 */
	SceUInt32 binaryPoint;		/* 0x08 */
	SceUInt32 acknowledge;		/* 0x0c */
	SceUInt32 endOfIntr;		/* 0x10 */
} GicCpuInterface;

typedef int (*SubIntrCallback)(unsigned int, unsigned int, ...);

typedef struct IntrHandler {
	void *handler;
	SceUInt16 intrCode;
	SceUInt16 targetCpu;
	void *arg;
	SceUInt16 priority;
	SceUInt8 type;
	SceUInt8 configuration;
	char *name;
	void *oldHandler;
	SceUInt32 reserved[2];
	SceUInt32 count[4];
	SceKernelSpinlock lock;
	struct IntrHandler *sub;
	SubIntrCallback *callbacks;
	SceUInt32 subCount;
} IntrHandler;

typedef struct SceKernelIntrHandlerInfo {
	SceSize size;
	void *handler;
	void *arg;
	SceUInt16 intrCode;
	SceUInt16 type;
	SceUInt16 subCount;
	SceUInt16 priority;
	SceUInt16 targetCpu;
	SceUInt16 enabled;
	SceUInt32 count[4];
	SceUInt32 reserved0[4];		/* always 0 */
	SceUInt32 reserved1[4];
	SceUInt32 reserved2[8];
	char *name;
	SceUInt32 typeFlag;		/* 0x10: the handler's type has 0x10 */
} SceKernelIntrHandlerInfo;

typedef struct IntrCpuState {
	SceUInt32 nesting;
	SceUInt32 padding[31];		/* one 0x80 line per CPU */
} IntrCpuState;

/* Handler records and their per-CPU nesting state share the native base. */
typedef struct IntrState {
	IntrHandler handlers[INTR_COUNT];
	IntrCpuState cpu[4];
} IntrState;

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
#define g_intrHandlers (g_intrState.handlers)
#define g_intrCpu (g_intrState.cpu)
extern void *g_intrSysroot;
extern SceUInt32 g_intrReserved2[15];
extern ProcessHwBreakpoints *g_intrKernelBreakpoints;
extern ProcessHwWatchpoints *g_intrKernelWatchpoints;
extern SceUInt32 g_intrThreadmgrCpu[4];
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
extern GicCpuInterface *volatile g_intrCpuInterface;
extern SceSyscallTable g_intrSyscall;
#define g_intrSyscallTable (g_intrSyscall.base)
#define g_intrSyscallBlock (g_intrSyscall.memBlock)
extern GicDistributor *volatile g_intrDistributor;
extern void *g_intrLeaveSubIntr;
extern void *g_intrEnterIntr;
extern int (*g_intrEvent)(unsigned int, unsigned int, void *);
extern void (*g_intrRegistered)(unsigned int);
extern SceUID g_intrHeap;
extern void (*g_intrReleased)(unsigned int);

/* name unknown: the guard word at the bottom of one per-core nesting stack. */
static inline SceUInt32 *StackGuard(unsigned int cpu, unsigned int level)
{
	if (!cpu && !level)
		return &g_intrStackGuard.outermostStackGuard;
	return &g_intrStacks[(4 - cpu) * 16 - level].guard;
}

void interruptEntry(void);
void syscallEntry(void);
void initStackGuards(unsigned int magic);
int checkStackGuard(void);
int intrmgrSmc(unsigned int paddr, unsigned int command);
extern char intrEnterHook[], intrLeaveHook[], syscallEnterHook[], syscallLeaveHook[];
extern SceUInt32 intrEnterBranch, intrLeaveBranch;
extern SceUInt16 syscallEnterBranch, syscallLeaveBranch;



int SceSysmemForKernel_B628B46B(const char *name, unsigned int flags, char **out);
int SceSysmemForKernel_634D0D0C(char *name, unsigned int flags);
void *SceSysrootForKernel_118657C6(void);
int sceKernelSysrootSetModulePrivate(unsigned int slot, void *value);
int sceKernelSysrootTrapThreadAfterSyscall(void *args, void *results, unsigned int flags);
void *ksceKernelSysrootGetVbaseResetVector(void);
int ksceExcpmgrRegisterHandler(int code, int flags, void *handler);
int SceKernelSuspendForDriver_CE7A2207(unsigned int slot, void *handler, void *arg);
int SceKernelSuspendForDriver_105C5752(unsigned int slot, void *handler, void *arg);

const char *ksceGUIDName(SceUID uid);
int callIntrEvent(unsigned int event, unsigned int size, void *data);
void invalidApiStatCfunc(const ThreadSyscallFrame *frame, void *result, unsigned int state);
void invalidFastMutexOwnerCfunc(void);
