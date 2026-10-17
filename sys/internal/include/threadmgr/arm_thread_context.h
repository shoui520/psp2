/* Saved-thread ABI shared by Intrmgr and the SDfMgr debugger return path (see threadmgr/threadcb.h). */
#pragma once
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr_syscall.h>
#include <threadmgr/threadcb.h>
#include <sysmem/barrier.h>



#define ARM_CONTEXT_WRITE_CP(cp, op, crn, crm, op2, value) \
	__asm__ volatile("mcr " #cp ", " #op ", %0, " #crn ", " #crm ", " #op2 \
		:: "r" (value) : "memory")

/* Banked registers and the live stack transition belong to the entry veneer. */
static __inline__ __attribute__((always_inline))
void armRestoreThreadSystemContext(const ThreadRegisterContext *context, int irq)
{
	int counter;

	ARM_CONTEXT_WRITE_CP(p14, 6, c1, c0, 0, context->teehbr);
	ARM_CONTEXT_WRITE_CP(p15, 0, c6, c0, 2, context->ifar);
	ARM_CONTEXT_WRITE_CP(p15, 0, c6, c0, 0, context->dfar);
	ARM_CONTEXT_WRITE_CP(p15, 0, c5, c0, 1, context->ifsr);
	ARM_CONTEXT_WRITE_CP(p15, 0, c5, c0, 0, context->dfsr);
	ARM_CONTEXT_WRITE_CP(p15, 0, c9, c12, 2, context->pmcntenset);
	for (counter = 5; counter >= 0; counter--) {
		ARM_CONTEXT_WRITE_CP(p15, 0, c9, c12, 5, counter);
		ARM_CONTEXT_WRITE_CP(p15, 0, c9, c13, 1, context->pmevent[counter].eventType & 255);
		ARM_CONTEXT_WRITE_CP(p15, 0, c9, c13, 2, context->pmevent[counter].count);
	}
	ARM_CONTEXT_WRITE_CP(p15, 0, c9, c13, 0, context->pmccntr);
	ARM_CONTEXT_WRITE_CP(p15, 0, c9, c14, 0, context->pmuserenr);
	ARM_CONTEXT_WRITE_CP(p15, 0, c9, c12, 5, context->pmselr);
	ARM_CONTEXT_WRITE_CP(p15, 0, c9, c12, 3, context->pmovsr);
	ARM_CONTEXT_WRITE_CP(p15, 0, c9, c12, 0, context->pmcr);
	ARM_CONTEXT_WRITE_CP(p15, 0, c7, c4, 0, context->par);
	if (irq) {
		ARM_CONTEXT_WRITE_CP(p15, 0, c13, c0, 3, context->tpidruro);
		ARM_CONTEXT_WRITE_CP(p15, 0, c13, c0, 2, context->tpidrurw);
	}
	ARM_CONTEXT_WRITE_CP(p15, 0, c13, c0, 1, 0);
	isb();
	ARM_CONTEXT_WRITE_CP(p15, 0, c2, c0, 1, context->ttbr1);
	isb();
	ARM_CONTEXT_WRITE_CP(p15, 0, c13, c0, 1, context->contextidr);
	if (!irq) {
		ARM_CONTEXT_WRITE_CP(p15, 0, c13, c0, 3, context->tpidruro);
		ARM_CONTEXT_WRITE_CP(p15, 0, c13, c0, 2, context->tpidrurw);
	}
	ARM_CONTEXT_WRITE_CP(p15, 0, c1, c0, 2, context->cpacr);
}

#undef ARM_CONTEXT_WRITE_CP

