#ifndef THREADMGR_THREAD_CONTEXT_H
#define THREADMGR_THREAD_CONTEXT_H

#include "sce/threadcb.h"

/* Sony also inlines this initializer at saved-context construction sites. */
#ifndef THREADMGR_CONTEXT_DEFINE
extern inline __attribute__((gnu_inline))
#endif
void initRegisterContext(struct ThreadRegisterContext *regs)
{
	int i;

	regs->full = 1;
	for (i = 0; i < 15; i++)
		regs->r[i] = 0xdeadbeef;
	regs->status = 0;
	regs->tpidruro = 0;
	regs->pmselr = 0;
	regs->dfsr = 0;
	regs->ifsr = 0;
	regs->dfar = 0;
	regs->ifar = 0;
	regs->reserved0 = 0;
	regs->pc = 0;
	regs->cpsr = 0;
	regs->teehbr = 0x2000;
	regs->priorityMask = 0xff;
}

#endif
