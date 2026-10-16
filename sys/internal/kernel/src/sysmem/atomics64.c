/* Shared64-bit atomic primitives; doubleword exclusives stay in the common header. */
#include "../../../../../common/sysclib/atomic.h"

uint64_t ksceKernelAtomicCompareAndSet64(uint64_t *address, uint64_t expected,
		uint64_t replacement)
{
	return AtomicCompareAndSwap64(address, expected, replacement);
}

void ksceKernelAtomicSet64(uint64_t *address, uint64_t value)
{
	AtomicSet64(address, value);
}

uint64_t ksceKernelAtomicGetAndSet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SET, 0);
}

uint64_t ksceKernelAtomicGetAndAdd64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_ADD, 0);
}

uint64_t ksceKernelAtomicAddAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_ADD, 1);
}

uint64_t ksceKernelAtomicGetAndSub64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SUBTRACT, 0);
}

uint64_t ksceKernelAtomicSubAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SUBTRACT, 1);
}

uint64_t ksceKernelAtomicGetAndAnd64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_AND, 0);
}

uint64_t ksceKernelAtomicAndAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_AND, 1);
}

uint64_t ksceKernelAtomicGetAndOr64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_OR, 0);
}

uint64_t ksceKernelAtomicOrAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_OR, 1);
}

uint64_t ksceKernelAtomicGetAndXor64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_XOR, 0);
}

uint64_t ksceKernelAtomicXorAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_XOR, 1);
}

uint64_t ksceKernelAtomicGetAndClear64(uint64_t *address, uint64_t mask)
{
	return AtomicUpdate64(address, mask, ATOMIC_CLEAR, 0);
}

uint64_t ksceKernelAtomicClearAndGet64(uint64_t *address, uint64_t mask)
{
	return AtomicUpdate64(address, mask, ATOMIC_CLEAR, 1);
}

void ksceKernelAtomicClearMask64(uint64_t *address, uint64_t mask)
{
	AtomicUpdate64(address, mask, ATOMIC_CLEAR, 1);
}

int ksceKernelAtomicAddUnless64(uint64_t *address, uint64_t value, int64_t limit)
{
	return AtomicAddWithLimit64(address, value, limit);
}

uint64_t ksceKernelAtomicDecIfPositive64(uint64_t *address)
{
	return AtomicDecrementPositive64(address);
}
