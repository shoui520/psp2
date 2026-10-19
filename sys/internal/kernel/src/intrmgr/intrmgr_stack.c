/* Interrupt nesting stack guards and CPU entry configuration. */
#include "intrmgr.h"
#include <excpmgr/moncall.h>

/* name unknown: write the overflow magic below every nesting stack. */
void __attribute__((noinline)) initStackGuards(unsigned int magic)
{
	IntrLevelStack *stack = g_intrStacks;
	IntrLevelStack *end = g_intrStacks + 64;

	do {
		stack->guard = magic;
		stack++;
	} while (stack < end);
}

/* name unknown: check the guard of the current core's current nesting level. */
int __attribute__((noinline)) checkStackGuard(void)
{
	unsigned int cpu = readMpidr() & 15;
	int nesting = g_intrCpu[cpu].nesting;

	if (*StackGuard(cpu, nesting) != INTR_STACK_MAGIC)
		return -1;
	return nesting;
}

void *SceIntrmgrForKernel_01911612(unsigned int cpu, unsigned int level)
{
	if (cpu >= 4 || level < 1 || level > 16)
		return NULL;
	return StackGuard(cpu, level - 1);
}

void SceIntrmgrForKernel_BCD3BB05(void)
{
	enableIrq();
}

void SceIntrmgrForKernel_9CB39268(void *handler) { g_intrStackGuard.beforeIntrHandler = handler; }
void SceIntrmgrForKernel_46F04D82(void *handler) { g_intrDispatchPending = handler; }
void SceIntrmgrForKernel_764F140A(void *handler) { g_intrSetupNextContext = handler; }

void SceIntrmgrForKernel_04E635AA(struct ThreadMgrCpuCB *const *cpu)
{
	for (int i = 0; i < 4; i++)
		g_intrThreadmgrCpu[i] = cpu[i];
}

/* name unknown: SCE_MONCALL_SET_SHARED_MEMORY (GIC state handover on resume). */
int intrmgrSmc(unsigned int paddr, unsigned int command)
{
	return moncall2(SCE_MONCALL_SET_SHARED_MEMORY, paddr, command);
}
