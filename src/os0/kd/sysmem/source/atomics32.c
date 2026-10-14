/* Shared32-bit atomic primitives; ARM exclusives stay in the common header. */
#include "../../../../common/sysclib/atomic.h"

uint32_t ksceKernelAtomicCompareAndSet32(uint32_t *address, uint32_t expected,
				     uint32_t replacement)
{
	return AtomicCompareAndSwap32(address, expected, replacement);
}

void ksceKernelAtomicSet32(uint32_t *address, uint32_t value)
{
	AtomicSet32(address, value);
}

uint32_t ksceKernelAtomicGetAndSet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_SET, 0);
}

uint32_t ksceKernelAtomicGetAndAdd32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_ADD, 0);
}

uint32_t ksceKernelAtomicAddAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_ADD, 1);
}

uint32_t ksceKernelAtomicGetAndSub32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_SUBTRACT, 0);
}

uint32_t ksceKernelAtomicSubAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_SUBTRACT, 1);
}

uint32_t ksceKernelAtomicGetAndAnd32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_AND, 0);
}

uint32_t ksceKernelAtomicAndAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_AND, 1);
}

uint32_t ksceKernelAtomicGetAndOr32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_OR, 0);
}

uint32_t ksceKernelAtomicOrAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_OR, 1);
}

uint32_t ksceKernelAtomicGetAndXor32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_XOR, 0);
}

uint32_t ksceKernelAtomicXorAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_XOR, 1);
}

uint32_t ksceKernelAtomicGetAndClear32(uint32_t *address, uint32_t mask)
{
	return AtomicUpdate32(address, mask, ATOMIC_CLEAR, 0);
}

uint32_t ksceKernelAtomicClearAndGet32(uint32_t *address, uint32_t mask)
{
	return AtomicUpdate32(address, mask, ATOMIC_CLEAR, 1);
}

void ksceKernelAtomicClearMask32(uint32_t *address, uint32_t mask)
{
	AtomicUpdate32(address, mask, ATOMIC_CLEAR, 1);
}

int ksceKernelAtomicAddUnless32(uint32_t *address, uint32_t value, int32_t limit)
{
	return AtomicAddWithLimit32(address, value, limit);
}

uint32_t ksceKernelAtomicDecIfPositive32(uint32_t *address)
{
	return AtomicDecrementPositive32(address);
}
