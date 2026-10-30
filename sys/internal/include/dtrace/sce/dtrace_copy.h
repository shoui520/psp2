/*
 * Copies between DTrace and a user process (Sony's dtrace/sce/dtrace_copy.c):
 * a fault sets the CPU's DTrace fault flags instead of raising one.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

#include <sysmem/sysmem.h>

/* Sony's copies from and to the calling process: the kernel's own, inlined */
static inline int dtrace_copyin_user(void *kaddr, uintptr_t uaddr, size_t size)
{
	return sceKernelCopyFromUser(kaddr, (const void *)uaddr, size);
}

static inline int copyin(uintptr_t uaddr, void *kaddr, size_t size)
{
	return sceKernelCopyFromUser(kaddr, (const void *)uaddr, size);
}

static inline int copyout(const void *kaddr, uintptr_t uaddr, size_t size)
{
	return sceKernelCopyToUser((void *)uaddr, kaddr, size);
}
