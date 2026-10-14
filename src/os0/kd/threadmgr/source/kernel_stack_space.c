/* SceKernelThreadMgr: the early kernel-stack space query in threadmgr.c. */
#include "private.h"

SceSize __attribute__((noinline)) SceThreadmgrForDriver_D74F66BD(void)
{
	struct ThreadCB *thread = currentThreadContext();
	intptr_t sp;
	SceInt32 free;

	if (thread == NULL)
		return 0;
	__asm__ volatile("mov %0, sp" : "=r"(sp));
	if (sp <= (SceInt32)thread->pKStack)
		return 0;
	free = sp - thread->pKStack;
	if ((SceInt32)thread->kStackSize <= free)
		return 0;
	return free;
}
