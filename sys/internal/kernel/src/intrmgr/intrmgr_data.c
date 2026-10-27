/* Interrupt controller objects, laid out once for the live CPU boundaries. */
#include <fw.h>
#include "intrmgr.h"

#define INTR_DATA __attribute__((section(".bss.intrmgr"), aligned(4)))

/* GCC emits these independent definitions in reverse declaration order. */
#if defined(INTRMGR_NSKBL)
/* the boot loader keeps the handler table, the stacks, the heap and the hooks */
void (*g_intrRegistered)(unsigned int) INTR_DATA;
SceUID g_intrHeap INTR_DATA;
IntrLevelStack g_intrStacks[64] INTR_DATA;
IntrState g_intrState
	__attribute__((section(".bss.intrmgr"), aligned(128)));
void *g_intrEnterIntr INTR_DATA;
void *g_intrLeaveSubIntr INTR_DATA;
SceUInt32 g_intrDistributor INTR_DATA;
SceSyscallTable g_intrSyscall INTR_DATA;
SceUInt32 g_intrCpuInterface INTR_DATA;
void *g_intrLeaveSyscall INTR_DATA;
void *g_intrEnterSyscall INTR_DATA;
void *g_intrLeaveIntr INTR_DATA;
void *g_intrEnterSubIntr INTR_DATA;

/* the end of the nesting stacks, where os0 keeps its stack guard */
__asm__(".global g_intrStackGuard\n"
	".set g_intrStackGuard, g_intrStacks + 64 * 0x2000");
#else
#if FW_SINCE(0x04000000)
void (*g_intrReleased)(unsigned int) INTR_DATA;
SceUID g_intrHeap INTR_DATA;
void (*g_intrRegistered)(unsigned int) INTR_DATA;
#else
void (*g_intrRegistered)(unsigned int) INTR_DATA;
SceUID g_intrHeap INTR_DATA;
void (*g_intrReleased)(unsigned int) INTR_DATA;
#endif
int (*g_intrEvent)(unsigned int, unsigned int, void *) INTR_DATA;
void *g_intrEnterIntr INTR_DATA;
void *g_intrLeaveSubIntr INTR_DATA;
SceUInt32 g_intrDistributor INTR_DATA;
SceSyscallTable g_intrSyscall INTR_DATA;
SceUInt32 g_intrCpuInterface INTR_DATA;
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
#endif

/*
 * The first nested-IRQ frame, 0x138 bytes below the end of the nesting
 * stacks, as a symbol for intrmgr_asm.S's movw/movt: a REL movw addend holds
 * only 16 bits, so the offset is folded in here, where g_intrStacks is
 * defined.
 */
__asm__(".global g_intrInitialIrqStack\n"
	".set g_intrInitialIrqStack, g_intrStacks + 64 * 0x2000 - 0x138");

/* the per-CPU nesting state, as a symbol for intrmgr_asm.S's movw/movt */
__asm__(".global g_intrCpu\n"
	".set g_intrCpu, g_intrState + 0x4000");
