#include "../../../../../common/sysclib/atomic.h"

uint64_t sceKernelAtomicCompareAndSet64(uint64_t *address, uint64_t expected,
		uint64_t replacement)
{
	return AtomicCompareAndSwap64(address, expected, replacement);
}

void sceKernelAtomicSet64(uint64_t *address, uint64_t value)
{
	AtomicSet64(address, value);
}

uint64_t sceKernelAtomicGetAndSet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SET, 0);
}

uint64_t sceKernelAtomicGetAndAdd64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_ADD, 0);
}

uint64_t sceKernelAtomicAddAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_ADD, 1);
}

uint64_t sceKernelAtomicGetAndSub64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SUBTRACT, 0);
}

uint64_t sceKernelAtomicSubAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SUBTRACT, 1);
}

uint64_t sceKernelAtomicGetAndAnd64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_AND, 0);
}

uint64_t sceKernelAtomicAndAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_AND, 1);
}

uint64_t sceKernelAtomicGetAndOr64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_OR, 0);
}

uint64_t sceKernelAtomicOrAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_OR, 1);
}

uint64_t sceKernelAtomicGetAndXor64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_XOR, 0);
}

uint64_t sceKernelAtomicXorAndGet64(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_XOR, 1);
}

uint64_t sceKernelAtomicGetAndClear64(uint64_t *address, uint64_t mask)
{
	return AtomicUpdate64(address, mask, ATOMIC_CLEAR, 0);
}

uint64_t sceKernelAtomicClearAndGet64(uint64_t *address, uint64_t mask)
{
	return AtomicUpdate64(address, mask, ATOMIC_CLEAR, 1);
}

void sceKernelAtomicClearMask64(uint64_t *address, uint64_t mask)
{
	AtomicUpdate64(address, mask, ATOMIC_CLEAR, 1);
}

int sceKernelAtomicAddUnless64(uint64_t *address, uint64_t value, int64_t limit)
{
	return AtomicAddWithLimit64(address, value, limit);
}

uint64_t sceKernelAtomicDecIfPositive64(uint64_t *address)
{
	return AtomicDecrementPositive64(address);
}
