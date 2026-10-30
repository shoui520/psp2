/*
 * Solaris atomics (Sony's dtrace/sce/solaris_atomic.c), on SceCpu's.
 */
#include <dtrace/sce/solaris_atomic.h>
#include <sysmem/cpu.h>

void atomic_add_32(volatile uint32_t *target, int32_t delta)
{
	sceKernelAtomicAddAndGet32((volatile SceInt32 *)target, delta);
}

void atomic_add_64(volatile uint64_t *target, int64_t delta)
{
	sceKernelAtomicAddAndGet64((uint64_t *)target, delta);
}

uint32_t atomic_cas_32(volatile uint32_t *target, uint32_t cmp, uint32_t newval)
{
	return sceKernelAtomicCompareAndSet32((volatile SceInt32 *)target, cmp, newval);
}

void *atomic_cas_ptr(volatile void *target, void *cmp, void *newval)
{
	return (void *)sceKernelAtomicCompareAndSet32((volatile SceInt32 *)target, (SceInt32)cmp,
						      (SceInt32)newval);
}
