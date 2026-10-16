#include "../../../../../common/sysclib/atomic.h"

uint32_t sceKernelAtomicCompareAndSet32(uint32_t *address, uint32_t expected,
				     uint32_t replacement)
{
	return AtomicCompareAndSwap32(address, expected, replacement);
}

void sceKernelAtomicSet32(uint32_t *address, uint32_t value)
{
	AtomicSet32(address, value);
}

uint32_t sceKernelAtomicGetAndSet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_SET, 0);
}

uint32_t sceKernelAtomicGetAndAdd32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_ADD, 0);
}

uint32_t sceKernelAtomicAddAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_ADD, 1);
}

uint32_t sceKernelAtomicGetAndSub32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_SUBTRACT, 0);
}

uint32_t sceKernelAtomicSubAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_SUBTRACT, 1);
}

uint32_t sceKernelAtomicGetAndAnd32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_AND, 0);
}

uint32_t sceKernelAtomicAndAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_AND, 1);
}

uint32_t sceKernelAtomicGetAndOr32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_OR, 0);
}

uint32_t sceKernelAtomicOrAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_OR, 1);
}

uint32_t sceKernelAtomicGetAndXor32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_XOR, 0);
}

uint32_t sceKernelAtomicXorAndGet32(uint32_t *address, uint32_t value)
{
	return AtomicUpdate32(address, value, ATOMIC_XOR, 1);
}

uint32_t sceKernelAtomicGetAndClear32(uint32_t *address, uint32_t mask)
{
	return AtomicUpdate32(address, mask, ATOMIC_CLEAR, 0);
}

uint32_t sceKernelAtomicClearAndGet32(uint32_t *address, uint32_t mask)
{
	return AtomicUpdate32(address, mask, ATOMIC_CLEAR, 1);
}

void sceKernelAtomicClearMask32(uint32_t *address, uint32_t mask)
{
	AtomicUpdate32(address, mask, ATOMIC_CLEAR, 1);
}

int sceKernelAtomicAddUnless32(uint32_t *address, uint32_t value, int32_t limit)
{
	return AtomicAddWithLimit32(address, value, limit);
}

uint32_t sceKernelAtomicDecIfPositive32(uint32_t *address)
{
	return AtomicDecrementPositive32(address);
}
