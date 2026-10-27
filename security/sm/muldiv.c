/*
 * muldiv.c: Shared libgcc 32-bit integer multiplication, division, and modulo
 * runtime primitives for os0:sm modules.
 */

#include "sm_common.h"

uint32_t __mulsi3(uint32_t a, uint32_t b)
{
	uint32_t res = 0u;

	while (a != 0u) {
		if ((a & 1u) != 0u)
			res += b;
		a >>= 1;
		b <<= 1;
	}
	return res;
}

uint32_t udivmodsi4(uint32_t num, uint32_t den, int modwanted)
{
	uint32_t bit = 1u;
	uint32_t res = 0u;

	while (den < num && bit != 0u && (den & (1u << 31)) == 0u) {
		den <<= 1;
		bit <<= 1;
	}
	while (bit != 0u) {
		if (num >= den) {
			num -= den;
			res |= bit;
		}
		bit >>= 1;
		den >>= 1;
	}
	return modwanted ? num : res;
}

int32_t __divsi3(int32_t a, int32_t b)
{
	int neg = 0;
	int32_t res;

	if (a < 0) {
		a = -a;
		neg = 1;
	}
	if (b < 0) {
		b = -b;
		neg ^= 1;
	}
	res = (int32_t)udivmodsi4((uint32_t)a, (uint32_t)b, 0);
	if (neg)
		res = -res;
	return res;
}

int32_t __modsi3(int32_t a, int32_t b)
{
	int neg = 0;
	int32_t res;

	if (a < 0) {
		a = -a;
		neg = 1;
	}
	if (b < 0)
		b = -b;
	res = (int32_t)udivmodsi4((uint32_t)a, (uint32_t)b, 1);
	if (neg)
		res = -res;
	return res;
}

uint32_t __udivsi3(uint32_t a, uint32_t b)
{
	return udivmodsi4(a, b, 0);
}

uint32_t __umodsi3(uint32_t a, uint32_t b)
{
	return udivmodsi4(a, b, 1);
}
