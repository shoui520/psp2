/* Shared byte/halfword atomic primitives preserve native signed returns. */
#include "../../../../../common/sysclib/atomic.h"

void ksceKernelAtomicSet8(int8_t *address, int8_t value)
{
	AtomicSet8((uint8_t *)address, (uint8_t)value);
}

void ksceKernelAtomicSet16(int16_t *address, int16_t value)
{
	AtomicSet16((uint16_t *)address, (uint16_t)value);
}

int8_t ksceKernelAtomicCompareAndSet8(int8_t *address, int8_t expected,
		int8_t replacement)
{
	return AtomicCompareAndSwap8((uint8_t *)address, expected, replacement);
}

int16_t ksceKernelAtomicCompareAndSet16(int16_t *address, int16_t expected,
		int16_t replacement)
{
	return AtomicCompareAndSwap16((uint16_t *)address, expected, replacement);
}

int8_t ksceKernelAtomicGetAndSet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SET, 0);
}

int16_t ksceKernelAtomicGetAndSet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SET, 0);
}

int8_t ksceKernelAtomicGetAndAdd8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_ADD, 0);
}

int16_t ksceKernelAtomicGetAndAdd16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_ADD, 0);
}

int8_t ksceKernelAtomicAddAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_ADD, 1);
}

int16_t ksceKernelAtomicAddAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_ADD, 1);
}

int8_t ksceKernelAtomicGetAndSub8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SUBTRACT, 0);
}

int16_t ksceKernelAtomicGetAndSub16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SUBTRACT, 0);
}

int8_t ksceKernelAtomicSubAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SUBTRACT, 1);
}

int16_t ksceKernelAtomicSubAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SUBTRACT, 1);
}

int8_t ksceKernelAtomicGetAndAnd8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_AND, 0);
}

int16_t ksceKernelAtomicGetAndAnd16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_AND, 0);
}

int8_t ksceKernelAtomicAndAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_AND, 1);
}

int16_t ksceKernelAtomicAndAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_AND, 1);
}

int8_t ksceKernelAtomicGetAndOr8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_OR, 0);
}

int16_t ksceKernelAtomicGetAndOr16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_OR, 0);
}

int8_t ksceKernelAtomicOrAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_OR, 1);
}

int16_t ksceKernelAtomicOrAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_OR, 1);
}

int8_t ksceKernelAtomicGetAndXor8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_XOR, 0);
}

int16_t ksceKernelAtomicGetAndXor16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_XOR, 0);
}

int8_t ksceKernelAtomicXorAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_XOR, 1);
}

int16_t ksceKernelAtomicXorAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_XOR, 1);
}

int8_t ksceKernelAtomicGetAndClear8(int8_t *address, int8_t mask)
{
	return AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 0);
}

int16_t ksceKernelAtomicGetAndClear16(int16_t *address, int16_t mask)
{
	return AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 0);
}

int8_t ksceKernelAtomicClearAndGet8(int8_t *address, int8_t mask)
{
	return AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 1);
}

int16_t ksceKernelAtomicClearAndGet16(int16_t *address, int16_t mask)
{
	return AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 1);
}

void ksceKernelAtomicClearMask8(int8_t *address, int8_t mask)
{
	AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 1);
}

void ksceKernelAtomicClearMask16(int16_t *address, int16_t mask)
{
	AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 1);
}

int ksceKernelAtomicAddUnless8(int8_t *address, int8_t value, int8_t limit)
{
	return AtomicAddWithLimit8((uint8_t *)address, value, limit);
}

int ksceKernelAtomicAddUnless16(int16_t *address, int16_t value, int16_t limit)
{
	return AtomicAddWithLimit16((uint16_t *)address, value, limit);
}

int8_t ksceKernelAtomicDecIfPositive8(int8_t *address)
{
	return AtomicDecrementNonnegative8((uint8_t *)address);
}

int16_t ksceKernelAtomicDecIfPositive16(int16_t *address)
{
	return AtomicDecrementNonnegative16((uint16_t *)address);
}
