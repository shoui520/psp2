/*
 * Syscall entry / exit as Sony's exported syscalls do it: while the kernel works
 * for the user thread, TPIDRURO holds the user value shifted left by 16, and the
 * original value is restored before returning.
 */

#pragma once

#include <psp2kern/types.h>

static inline SceUInt32 sceSyscallEnter(void)
{
	SceUInt32 t;

	__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(t) : : "memory");
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" : : "r"(t << 16) : "memory");
	return t;
}

static inline void sceSyscallLeave(SceUInt32 t)
{
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" : : "r"(t) : "memory");
}

/* SceSysmem: a syscall that returns a 64-bit value says so first (sysroot.c) */
void sceKernelApiDeclareTypeInt64(void);
