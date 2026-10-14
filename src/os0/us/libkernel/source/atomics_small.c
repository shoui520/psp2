#include "../../../../common/sysclib/atomic.h"

void sceKernelAtomicSet8(int8_t *address, int8_t value)
{
	AtomicSet8((uint8_t *)address, (uint8_t)value);
}

void sceKernelAtomicSet16(int16_t *address, int16_t value)
{
	AtomicSet16((uint16_t *)address, (uint16_t)value);
}

int8_t sceKernelAtomicCompareAndSet8(int8_t *address, int8_t expected,
		int8_t replacement)
{
	return AtomicCompareAndSwap8((uint8_t *)address, expected, replacement);
}

int16_t sceKernelAtomicCompareAndSet16(int16_t *address, int16_t expected,
		int16_t replacement)
{
	return AtomicCompareAndSwap16((uint16_t *)address, expected, replacement);
}

int8_t sceKernelAtomicGetAndSet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SET, 0);
}

int16_t sceKernelAtomicGetAndSet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SET, 0);
}

int8_t sceKernelAtomicGetAndAdd8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_ADD, 0);
}

int16_t sceKernelAtomicGetAndAdd16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_ADD, 0);
}

int8_t sceKernelAtomicAddAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_ADD, 1);
}

int16_t sceKernelAtomicAddAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_ADD, 1);
}

int8_t sceKernelAtomicGetAndSub8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SUBTRACT, 0);
}

int16_t sceKernelAtomicGetAndSub16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SUBTRACT, 0);
}

int8_t sceKernelAtomicSubAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SUBTRACT, 1);
}

int16_t sceKernelAtomicSubAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SUBTRACT, 1);
}

int8_t sceKernelAtomicGetAndAnd8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_AND, 0);
}

int16_t sceKernelAtomicGetAndAnd16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_AND, 0);
}

int8_t sceKernelAtomicAndAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_AND, 1);
}

int16_t sceKernelAtomicAndAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_AND, 1);
}

int8_t sceKernelAtomicGetAndOr8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_OR, 0);
}

int16_t sceKernelAtomicGetAndOr16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_OR, 0);
}

int8_t sceKernelAtomicOrAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_OR, 1);
}

int16_t sceKernelAtomicOrAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_OR, 1);
}

int8_t sceKernelAtomicGetAndXor8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_XOR, 0);
}

int16_t sceKernelAtomicGetAndXor16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_XOR, 0);
}

int8_t sceKernelAtomicXorAndGet8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_XOR, 1);
}

int16_t sceKernelAtomicXorAndGet16(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_XOR, 1);
}

int8_t sceKernelAtomicGetAndClear8(int8_t *address, int8_t mask)
{
	return AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 0);
}

int16_t sceKernelAtomicGetAndClear16(int16_t *address, int16_t mask)
{
	return AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 0);
}

int8_t sceKernelAtomicClearAndGet8(int8_t *address, int8_t mask)
{
	return AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 1);
}

int16_t sceKernelAtomicClearAndGet16(int16_t *address, int16_t mask)
{
	return AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 1);
}

void sceKernelAtomicClearMask8(int8_t *address, int8_t mask)
{
	AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 1);
}

void sceKernelAtomicClearMask16(int16_t *address, int16_t mask)
{
	AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 1);
}

int sceKernelAtomicAddUnless8(int8_t *address, int8_t value, int8_t limit)
{
	return AtomicAddWithLimit8((uint8_t *)address, value, limit);
}

int sceKernelAtomicAddUnless16(int16_t *address, int16_t value, int16_t limit)
{
	return AtomicAddWithLimit16((uint16_t *)address, value, limit);
}

int8_t sceKernelAtomicDecIfPositive8(int8_t *address)
{
	return AtomicDecrementNonnegative8((uint8_t *)address);
}

int16_t sceKernelAtomicDecIfPositive16(int16_t *address)
{
	return AtomicDecrementNonnegative16((uint16_t *)address);
}
