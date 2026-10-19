/* Interrupt controller objects, laid out once for the live CPU boundaries. */
#include "intrmgr.h"

#define INTR_DATA __attribute__((section(".bss.intrmgr"), aligned(4)))

/* GCC emits these independent definitions in reverse declaration order. */
void (*g_intrReleased)(unsigned int) INTR_DATA;
SceUID g_intrHeap INTR_DATA;
void (*g_intrRegistered)(unsigned int) INTR_DATA;
int (*g_intrEvent)(unsigned int, unsigned int, void *) INTR_DATA;
void *g_intrEnterIntr INTR_DATA;
void *g_intrLeaveSubIntr INTR_DATA;
GicDistributor *volatile g_intrDistributor INTR_DATA;
SceSyscallTable g_intrSyscall INTR_DATA;
GicCpuInterface *volatile g_intrCpuInterface INTR_DATA;
void *g_intrLeaveSyscall INTR_DATA;
void *g_intrEnterSyscall INTR_DATA;
void *g_intrLeaveIntr INTR_DATA;
void *g_intrEnterSubIntr INTR_DATA;
SceUInt32 g_intrReserved0[13] INTR_DATA;
void *g_intrSetupNextContext INTR_DATA;
void *g_intrDispatchPending INTR_DATA;
IntrStackGuard g_intrStackGuard INTR_DATA;
IntrLevelStack g_intrStacks[64] INTR_DATA;
SceUInt32 g_intrReserved1[10] INTR_DATA;
struct ThreadMgrCpuCB *g_intrThreadmgrCpu[4] INTR_DATA;
ProcessHwWatchpoints *g_intrKernelWatchpoints INTR_DATA;
ProcessHwBreakpoints *g_intrKernelBreakpoints INTR_DATA;
SceUInt32 g_intrReserved2[15] INTR_DATA;
void *g_intrSysroot INTR_DATA;
IntrState g_intrState
	__attribute__((section(".bss.intrmgr"), aligned(128)));

/*
 * The first nested-IRQ frame, 0x138 bytes below the end of the nesting
 * stacks, as a symbol for intrmgr_cpu.S's movw/movt: a REL movw addend holds
 * only 16 bits, so the offset is folded in here, where g_intrStacks is
 * defined.
 */
__asm__(".global g_intrInitialIrqStack\n"
	".set g_intrInitialIrqStack, g_intrStacks + 64 * 0x2000 - 0x138");
