/*
 * fw1 runtime: the multiplication and division GCC calls for, since the cores
 * have no multiply or divide unit. Dividing by zero stops at a break instruction.
 */
#include <stdint.h>

uint32_t __udivsi3(uint32_t dividend, uint32_t divisor);
uint32_t __umodsi3(uint32_t dividend, uint32_t divisor);
int32_t __divsi3(int32_t dividend, int32_t divisor);
int32_t __modsi3(int32_t dividend, int32_t divisor);
uint64_t __udivdi3(uint64_t dividend, uint64_t divisor);
uint64_t __umoddi3(uint64_t dividend, uint64_t divisor);
int64_t __divdi3(int64_t dividend, int64_t divisor);
uint32_t __mulsi3(uint32_t a, uint32_t b);
uint64_t __muldi3(uint64_t a, uint64_t b);

static void divideByZero(void)
{
	__asm__ volatile ("break");
}

/* the quotient, or with remainder set the remainder, of a nonzero divisor */
static uint32_t divide(uint32_t dividend, uint32_t divisor, uint32_t remainder)
{
	uint32_t quotient = 0, bit = 1;

	while (divisor < dividend && !(divisor & 0x80000000)) {
		divisor <<= 1;
		bit <<= 1;
	}
	while (bit) {
		if (dividend >= divisor) {
			dividend -= divisor;
			quotient |= bit;
		}
		divisor >>= 1;
		bit >>= 1;
	}
	return remainder ? dividend : quotient;
}

uint32_t __udivsi3(uint32_t dividend, uint32_t divisor)
{
	if (!divisor)
		divideByZero();
	return divide(dividend, divisor, 0);
}

uint32_t __umodsi3(uint32_t dividend, uint32_t divisor)
{
	if (!divisor)
		divideByZero();
	return divide(dividend, divisor, 1);
}

int32_t __divsi3(int32_t dividend, int32_t divisor)
{
	uint32_t quotient;

	if (!divisor)
		divideByZero();
	quotient = divide(dividend < 0 ? -(uint32_t)dividend : (uint32_t)dividend,
		divisor < 0 ? -(uint32_t)divisor : (uint32_t)divisor, 0);
	return (dividend < 0) != (divisor < 0) ? -quotient : quotient;
}

int32_t __modsi3(int32_t dividend, int32_t divisor)
{
	uint32_t remainder;

	if (!divisor)
		divideByZero();
	remainder = divide(dividend < 0 ? -(uint32_t)dividend : (uint32_t)dividend,
		divisor < 0 ? -(uint32_t)divisor : (uint32_t)divisor, 1);
	return dividend < 0 ? -remainder : remainder;
}

/* 64-bit, for PU1's clock (its own copy works on pointers to the values) */
uint64_t __udivdi3(uint64_t dividend, uint64_t divisor)
{
	uint64_t quotient = 0, bit = 1;

	if (!divisor)
		divideByZero();
	while (divisor < dividend && !(divisor >> 63)) {
		divisor <<= 1;
		bit <<= 1;
	}
	while (bit) {
		if (dividend >= divisor) {
			dividend -= divisor;
			quotient |= bit;
		}
		divisor >>= 1;
		bit >>= 1;
	}
	return quotient;
}

uint64_t __umoddi3(uint64_t dividend, uint64_t divisor)
{
	return dividend - __udivdi3(dividend, divisor) * divisor;
}

int64_t __divdi3(int64_t dividend, int64_t divisor)
{
	uint64_t quotient;

	quotient = __udivdi3(dividend < 0 ? -(uint64_t)dividend : (uint64_t)dividend,
		divisor < 0 ? -(uint64_t)divisor : (uint64_t)divisor);
	return (dividend < 0) != (divisor < 0) ? -quotient : quotient;
}

/* shift and add, by the bits of the smaller factor */
uint32_t __mulsi3(uint32_t a, uint32_t b)
{
	uint32_t product = 0, t;

	if (a < b) {
		t = a;
		a = b;
		b = t;
	}
	while (b) {
		if (b & 1)
			product += a;
		a <<= 1;
		b >>= 1;
	}
	return product;
}

uint64_t __muldi3(uint64_t a, uint64_t b)
{
	uint64_t product = 0;

	while (b) {
		if (b & 1)
			product += a;
		a <<= 1;
		b >>= 1;
	}
	return product;
}
