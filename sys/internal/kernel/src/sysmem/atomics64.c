/* Shared64-bit atomic primitives; doubleword exclusives stay in the common header. */
#include <sysmem/atomic.h>

uint64_t SceCpuForDriver_4B527009(uint64_t *address, uint64_t expected,
		uint64_t replacement)
{
	return AtomicCompareAndSwap64(address, expected, replacement);
}

void SceCpuForDriver_C381CE8C(uint64_t *address, uint64_t value)
{
	AtomicSet64(address, value);
}

uint64_t SceCpuForDriver_D2DEE625(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SET, 0);
}

uint64_t SceCpuForDriver_043FD446(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_ADD, 0);
}

uint64_t SceCpuForDriver_4E459A03(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_ADD, 1);
}

uint64_t SceCpuForDriver_A7585370(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SUBTRACT, 0);
}

uint64_t SceCpuForDriver_B5F8919C(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_SUBTRACT, 1);
}

uint64_t SceCpuForDriver_18A17E07(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_AND, 0);
}

uint64_t SceCpuForDriver_D18E7B54(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_AND, 1);
}

uint64_t SceCpuForDriver_CB73D6D5(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_OR, 0);
}

uint64_t SceCpuForDriver_3E218AF7(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_OR, 1);
}

uint64_t SceCpuForDriver_E212ECAD(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_XOR, 0);
}

uint64_t SceCpuForDriver_692C51B3(uint64_t *address, uint64_t value)
{
	return AtomicUpdate64(address, value, ATOMIC_XOR, 1);
}

uint64_t SceCpuForDriver_88BA6002(uint64_t *address, uint64_t mask)
{
	return AtomicUpdate64(address, mask, ATOMIC_CLEAR, 0);
}

uint64_t SceCpuForDriver_2149CD4C(uint64_t *address, uint64_t mask)
{
	return AtomicUpdate64(address, mask, ATOMIC_CLEAR, 1);
}

void SceCpuForDriver_55760309(uint64_t *address, uint64_t mask)
{
	AtomicUpdate64(address, mask, ATOMIC_CLEAR, 1);
}

int SceCpuForDriver_06CCFA4B(uint64_t *address, uint64_t value, int64_t limit)
{
	return AtomicAddWithLimit64(address, value, limit);
}

uint64_t SceCpuForDriver_267D0B33(uint64_t *address)
{
	return AtomicDecrementPositive64(address);
}
