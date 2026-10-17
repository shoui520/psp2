/* Shared byte/halfword atomic primitives preserve native signed returns. */
#include <sysmem/atomic.h>

void SceCpuForDriver_0836537E(int8_t *address, int8_t value)
{
	AtomicSet8((uint8_t *)address, (uint8_t)value);
}

void SceCpuForDriver_532CA3E8(int16_t *address, int16_t value)
{
	AtomicSet16((uint16_t *)address, (uint16_t)value);
}

int8_t SceCpuForDriver_3627F4E0(int8_t *address, int8_t expected,
		int8_t replacement)
{
	return AtomicCompareAndSwap8((uint8_t *)address, expected, replacement);
}

int16_t SceCpuForDriver_6F63F56D(int16_t *address, int16_t expected,
		int16_t replacement)
{
	return AtomicCompareAndSwap16((uint16_t *)address, expected, replacement);
}

int8_t SceCpuForDriver_29599FC8(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SET, 0);
}

int16_t SceCpuForDriver_085532C8(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SET, 0);
}

int8_t SceCpuForDriver_FCDCD4DE(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_ADD, 0);
}

int16_t SceCpuForDriver_225DF91A(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_ADD, 0);
}

int8_t SceCpuForDriver_1E850481(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_ADD, 1);
}

int16_t SceCpuForDriver_59F74E94(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_ADD, 1);
}

int8_t SceCpuForDriver_7B43D0D7(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SUBTRACT, 0);
}

int16_t SceCpuForDriver_3EE9B5B8(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SUBTRACT, 0);
}

int8_t SceCpuForDriver_EB085370(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_SUBTRACT, 1);
}

int16_t SceCpuForDriver_515682C9(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_SUBTRACT, 1);
}

int8_t SceCpuForDriver_D8E675C0(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_AND, 0);
}

int16_t SceCpuForDriver_4A820BC5(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_AND, 0);
}

int8_t SceCpuForDriver_32B62B1A(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_AND, 1);
}

int16_t SceCpuForDriver_B281D52A(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_AND, 1);
}

int8_t SceCpuForDriver_BDF6F8E4(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_OR, 0);
}

int16_t SceCpuForDriver_004F09D1(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_OR, 0);
}

int8_t SceCpuForDriver_5D515F1B(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_OR, 1);
}

int16_t SceCpuForDriver_ADD39B84(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_OR, 1);
}

int8_t SceCpuForDriver_BAF47F7B(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_XOR, 0);
}

int16_t SceCpuForDriver_711801E6(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_XOR, 0);
}

int8_t SceCpuForDriver_03887992(int8_t *address, int8_t value)
{
	return AtomicUpdate8((uint8_t *)address, value, ATOMIC_XOR, 1);
}

int16_t SceCpuForDriver_646003D6(int16_t *address, int16_t value)
{
	return AtomicUpdate16((uint16_t *)address, value, ATOMIC_XOR, 1);
}

int8_t SceCpuForDriver_382D1466(int8_t *address, int8_t mask)
{
	return AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 0);
}

int16_t SceCpuForDriver_8E9C086D(int16_t *address, int16_t mask)
{
	return AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 0);
}

int8_t SceCpuForDriver_8E538AB5(int8_t *address, int8_t mask)
{
	return AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 1);
}

int16_t SceCpuForDriver_6B050D7C(int16_t *address, int16_t mask)
{
	return AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 1);
}

void SceCpuForDriver_1B3336B0(int8_t *address, int8_t mask)
{
	AtomicUpdate8((uint8_t *)address, mask, ATOMIC_CLEAR, 1);
}

void SceCpuForDriver_1BE58599(int16_t *address, int16_t mask)
{
	AtomicUpdate16((uint16_t *)address, mask, ATOMIC_CLEAR, 1);
}

int SceCpuForDriver_5CC62CEC(int8_t *address, int8_t value, int8_t limit)
{
	return AtomicAddWithLimit8((uint8_t *)address, value, limit);
}

int SceCpuForDriver_0F84AFE9(int16_t *address, int16_t value, int16_t limit)
{
	return AtomicAddWithLimit16((uint16_t *)address, value, limit);
}

int8_t SceCpuForDriver_45153D4E(int8_t *address)
{
	return AtomicDecrementNonnegative8((uint8_t *)address);
}

int16_t SceCpuForDriver_9A693F5B(int16_t *address)
{
	return AtomicDecrementNonnegative16((uint16_t *)address);
}
