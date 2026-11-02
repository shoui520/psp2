/*
 * The unsigned BPABI 64-bit divide helper, from GCC's
 * libgcc/config/arm/bpabi.c (Copyright (C) 2003-2013 Free Software Foundation,
 * Inc.; GCC Runtime Library Exception 3.1). Sony's libnet object carries this
 * helper only: its signed counterpart __gnu_ldivmod_helper, which the same
 * source defines, is not in the module.
 */

unsigned long long __udivdi3(unsigned long long dividend, unsigned long long divisor);

unsigned long long __gnu_uldivmod_helper(unsigned long long dividend,
					 unsigned long long divisor,
					 unsigned long long *remainder)
{
	unsigned long long quotient = __udivdi3(dividend, divisor);

	*remainder = dividend - divisor * quotient;
	return quotient;
}
