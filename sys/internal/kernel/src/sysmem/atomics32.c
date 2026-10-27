/* Shared32-bit atomic primitives; ARM exclusives stay in the common header. */
#include <sysmem/atomic.h>
#include <sysmem/cpu.h>

SceInt32 sceKernelAtomicCompareAndSet32(volatile SceInt32 *address, SceInt32 expected,
				  SceInt32 replacement)
{
	return AtomicCompareAndSwap32((uint32_t *)address, expected, replacement);
}

#if !SYSMEM_NSKBL
void sceKernelAtomicSet32(volatile SceInt32 *address, SceInt32 value)
{
	AtomicSet32((uint32_t *)address, value);
}

SceInt32 sceKernelAtomicGetAndSet32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_SET, 0);
}

SceInt32 sceKernelAtomicGetAndAdd32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_ADD, 0);
}
#endif

SceInt32 sceKernelAtomicAddAndGet32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_ADD, 1);
}

#if !SYSMEM_NSKBL
SceInt32 sceKernelAtomicGetAndSub32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_SUBTRACT, 0);
}
#endif

SceInt32 sceKernelAtomicSubAndGet32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_SUBTRACT, 1);
}

#if !SYSMEM_NSKBL
SceInt32 sceKernelAtomicGetAndAnd32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_AND, 0);
}

SceInt32 sceKernelAtomicAndAndGet32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_AND, 1);
}

SceInt32 sceKernelAtomicGetAndOr32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_OR, 0);
}

SceInt32 sceKernelAtomicOrAndGet32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_OR, 1);
}

SceInt32 sceKernelAtomicGetAndXor32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_XOR, 0);
}

SceInt32 sceKernelAtomicXorAndGet32(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_XOR, 1);
}

SceInt32 sceKernelAtomicGetAndClear32(volatile SceInt32 *address, SceInt32 mask)
{
	return AtomicUpdate32((uint32_t *)address, mask, ATOMIC_CLEAR, 0);
}

SceInt32 sceKernelAtomicClearAndGet32(volatile SceInt32 *address, SceInt32 mask)
{
	return AtomicUpdate32((uint32_t *)address, mask, ATOMIC_CLEAR, 1);
}

void sceKernelAtomicClearMask32(volatile SceInt32 *address, SceInt32 mask)
{
	AtomicUpdate32((uint32_t *)address, mask, ATOMIC_CLEAR, 1);
}

SceInt32 sceKernelAtomicAddUnless32(volatile SceInt32 *address, SceInt32 value, SceInt32 limit)
{
	return AtomicAddWithLimit32((uint32_t *)address, value, limit);
}
#endif

SceInt32 sceKernelAtomicDecIfPositive32(volatile SceInt32 *address)
{
	return AtomicDecrementPositive32((uint32_t *)address);
}
