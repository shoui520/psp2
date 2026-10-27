/* Shared32-bit atomic primitives; ARM exclusives stay in the common header. */
#include <sysmem/atomic.h>
#include <sysmem/cpu.h>

SceInt32 SceCpuForDriver_CDA96E81(volatile SceInt32 *address, SceInt32 expected,
				  SceInt32 replacement)
{
	return AtomicCompareAndSwap32((uint32_t *)address, expected, replacement);
}

#if !SYSMEM_NSKBL
void SceCpuForDriver_3168BC57(volatile SceInt32 *address, SceInt32 value)
{
	AtomicSet32((uint32_t *)address, value);
}

SceInt32 SceCpuForDriver_0EE04C03(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_SET, 0);
}

SceInt32 SceCpuForDriver_341B6E81(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_ADD, 0);
}
#endif

SceInt32 SceCpuForDriver_5F6A8743(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_ADD, 1);
}

#if !SYSMEM_NSKBL
SceInt32 SceCpuForDriver_F891CF2A(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_SUBTRACT, 0);
}
#endif

SceInt32 SceCpuForDriver_A4884C4E(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_SUBTRACT, 1);
}

#if !SYSMEM_NSKBL
SceInt32 SceCpuForDriver_10EB35EB(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_AND, 0);
}

SceInt32 SceCpuForDriver_DF899E4B(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_AND, 1);
}

SceInt32 SceCpuForDriver_2A40BB93(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_OR, 0);
}

SceInt32 SceCpuForDriver_BC248C30(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_OR, 1);
}

SceInt32 SceCpuForDriver_77E34309(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_XOR, 0);
}

SceInt32 SceCpuForDriver_4244BE65(volatile SceInt32 *address, SceInt32 value)
{
	return AtomicUpdate32((uint32_t *)address, value, ATOMIC_XOR, 1);
}

SceInt32 SceCpuForDriver_E36F3A46(volatile SceInt32 *address, SceInt32 mask)
{
	return AtomicUpdate32((uint32_t *)address, mask, ATOMIC_CLEAR, 0);
}

SceInt32 SceCpuForDriver_78C1F148(volatile SceInt32 *address, SceInt32 mask)
{
	return AtomicUpdate32((uint32_t *)address, mask, ATOMIC_CLEAR, 1);
}

void SceCpuForDriver_4AE1BCC0(volatile SceInt32 *address, SceInt32 mask)
{
	AtomicUpdate32((uint32_t *)address, mask, ATOMIC_CLEAR, 1);
}

SceInt32 SceCpuForDriver_1F157DC3(volatile SceInt32 *address, SceInt32 value, SceInt32 limit)
{
	return AtomicAddWithLimit32((uint32_t *)address, value, limit);
}
#endif

SceInt32 SceCpuForDriver_2A71B03C(volatile SceInt32 *address)
{
	return AtomicDecrementPositive32((uint32_t *)address);
}
