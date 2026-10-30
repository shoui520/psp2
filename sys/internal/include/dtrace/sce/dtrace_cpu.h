/*
 * The CPU DTrace runs on (Sony's dtrace/sce/dtrace_cpu.c).
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <intrmgr/intrmgr.h>

typedef uint32_t dtrace_icookie_t;

/* MPIDR's CPU number; Sony's read is a compiler barrier (the CPU's state is re-read after it) */
static inline processorid_t dtrace_processor_id(void)
{
	unsigned int mpidr;

	__asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r" (mpidr) :: "memory");
	return mpidr & 3;
}

/* the thread key's interrupt bits: the intrmgr's answer as it is */
static inline int dtrace_is_interrupt_context(void)
{
	return sceKernelIsIntrContext();
}

dtrace_icookie_t dtrace_interrupt_disable(void);
void dtrace_interrupt_enable(dtrace_icookie_t cookie);
void dtrace_xcall(processorid_t cpu, void (*func)(void *), void *arg);
void dtrace_sync(void);
