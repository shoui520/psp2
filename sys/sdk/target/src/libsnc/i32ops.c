/*
 * The 32-bit division helpers of SNC's runtime library (libsnc i32ops.o and
 * aeabi_div0.o), which SNC links into every module that divides: SNC calls
 * __udivsi3 / __divsi3 / __umodsi3 / __modsi3 for "/" and "%".  A division
 * by zero goes to the system's __sce_aeabi_idiv0.
 *
 * GCC calls the ARM run-time ABI names instead; they map onto the same
 * code, so the module carries no SceRtabi import that Sony's does not have.
 */

#include "i32ops.h"

int __sce_aeabi_idiv0(int return_value);
long long __sce_aeabi_ldiv0(long long return_value);

int __aeabi_idiv0(int return_value)
{
	return __sce_aeabi_idiv0(return_value);
}

long long __aeabi_ldiv0(long long return_value)
{
	return __sce_aeabi_ldiv0(return_value);
}

unsigned int __udivsi3(unsigned int n, unsigned int d)
{
	unsigned int shift, dd, q;

	if (d == 0)
		return __aeabi_idiv0(n);
	if (d == 1)
		return n;
	if (d == n)
		return 1;
	if (d > n)
		return 0;
	if ((d & (d - 1)) == 0)
		return n >> (31 - __builtin_clz(d));

	shift = __builtin_clz(d) - __builtin_clz(n);
	dd = d << shift;
	if (n - dd < d)
		return 1u << shift;
	if (shift < 7) {
		/* a few quotient bits: shift and subtract */
		q = 0;
		while (n >= d) {
			q <<= 1;
			if (dd <= n) {
				n -= dd;
				q |= 1;
			}
			shift--;
			dd >>= 1;
		}
		return q << (shift + 1);
	}
	return (unsigned int)((double)n / (double)d);
}

int __divsi3(int n, int d)
{
	int sign = n ^ d;
	int q;

	if (n < 0)
		n = -n;
	if (d < 0)
		d = -d;
	q = __udivsi3(n, d);
	if (sign < 0)
		q = -q;
	return q;
}

unsigned int __umodsi3(unsigned int n, unsigned int d)
{
	return n - __udivsi3(n, d) * d;
}

int __modsi3(int n, int d)
{
	int sign = n ^ d;
	unsigned int an = n, ad = d;
	int q;

	if (n < 0)
		an = -n;
	if (d < 0)
		ad = -d;
	q = __udivsi3(an, ad);
	if (sign < 0)
		q = -q;
	return n - q * d;
}

#ifndef __SNC__
unsigned int __aeabi_uidiv(unsigned int n, unsigned int d)
{
	return __udivsi3(n, d);
}

int __aeabi_idiv(int n, int d)
{
	return __divsi3(n, d);
}

/* quotient in r0, remainder in r1 */
unsigned long long __aeabi_uidivmod(unsigned int n, unsigned int d)
{
	unsigned int q = __udivsi3(n, d);

	return q | (unsigned long long)(n - q * d) << 32;
}

long long __aeabi_idivmod(int n, int d)
{
	int q = __divsi3(n, d);

	return (unsigned int)q | (unsigned long long)(unsigned int)(n - q * d) << 32;
}
#endif
