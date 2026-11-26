/*
 * SceDtcpIp: the SNC runtime objects the library carries. The shifts and the 32-bit
 * division are libsnc's own (they link byte-identical from i32ops.c/i64ops.c), but the
 * 64-bit division is a different build of it: Sony's has the extra normalization and the
 * slow path's shape below, which libsnc's current source does not produce. Each routine
 * is its own object (no inlining across them), as in libsnc.
 */

#ifdef __SNC__
#pragma control autoinlinesize=0
#endif

#include <scetypes.h>
#include "../../../../sdk/target/src/libsnc/i32ops.h"
#include "../../../../sdk/target/src/libsnc/i64ops.h"

/* the runtime objects refer to each other through absolute (interworking) calls, as
 * SNC writes a call whose target lives in another of its runtime objects */
extern unsigned int __udivsi3(unsigned int n, unsigned int d) __attribute__((weak));
extern unsigned long long __lshrdi3(unsigned long long value, long long count) __attribute__((weak));
extern long long __aeabi_ldiv0(long long return_value) __attribute__((weak));
extern long long __ashldi3(long long value, long long count) __attribute__((weak));
extern unsigned long long __u64_shift_right_1bit(unsigned long long value) __attribute__((weak));

typedef union {
	unsigned long long v;
	struct {
		unsigned int lo;
		unsigned int hi;
	} w;
} U64Parts;

long long __ashldi3(long long value, long long count)
{
	U64Parts a, r;

	if ((unsigned long long)count > 63)
		return 0;
	if (count == 0)
		return value;
	a.v = value;
	if (count < 32) {
		r.w.hi = a.w.hi << count | a.w.lo >> (32 - count);
		r.w.lo = a.w.lo << count;
	} else {
		r.w.hi = a.w.lo << (count - 32);
		r.w.lo = 0;
	}
	return r.v;
}

unsigned long long __lshrdi3(unsigned long long value, long long count)
{
	U64Parts a, r;

	if ((unsigned long long)count > 63)
		return 0;
	if (count == 0)
		return value;
	a.v = value;
	if (count < 32) {
		r.w.lo = a.w.lo >> count | a.w.hi << (32 - count);
		r.w.hi = a.w.hi >> count;
	} else {
		r.w.hi = 0;
		r.w.lo = a.w.hi >> (count - 32);
	}
	return r.v;
}

unsigned long long __u64_shift_right_1bit(unsigned long long value)
{
	U64Parts a;

	a.v = value;
	a.w.lo = a.w.lo >> 1 | a.w.hi << 31;
	a.w.hi >>= 1;
	return a.v;
}

/*
 * the unsigned 64-bit division: 32-bit fast paths, then restoring shift-and-subtract on
 * the normalized divisor. The quotient's high word collects the bits the low word's
 * shifts push out.
 */
unsigned long long __udivdi3(unsigned long long n, unsigned long long d)
{
	U64Parts dividend, quotient;
	unsigned int clzDividend;
	unsigned int clzDivisor;
	unsigned int low;
	unsigned int shift;
	unsigned int steps;
	unsigned int high;

	if ((d >> 32) == 0) {
		if ((n >> 32) == 0)
			return __udivsi3((unsigned int)n, (unsigned int)d);
		if ((unsigned int)d == 1)
			return n;
		if ((unsigned int)d == 0)
			return __aeabi_ldiv0((long long)n);
		if (((unsigned int)d & (0u - (unsigned int)d)) == (unsigned int)d) {
			shift = 31 - __builtin_clz((unsigned int)d);
			return __lshrdi3(n, shift);
		}
	}
	dividend.v = n;
	if (d > n)
		return 0;
	if (n == d)
		return 1;
	clzDividend = (n >> 32) != 0 ? __builtin_clz((unsigned int)(n >> 32))
				     : __builtin_clz((unsigned int)n) + 32;
	clzDivisor = (d >> 32) != 0 ? __builtin_clz((unsigned int)(d >> 32))
				    : __builtin_clz((unsigned int)d) + 32;
	low = 0;
	shift = clzDivisor - clzDividend;
	steps = shift + 1;
	if (shift != 0)
		d = __ashldi3(d, shift);
	high = 0;
	while (1) {
		--steps;
		high = (low >> 31) | (2 * high);
		low *= 2;
		if (dividend.v >= d) {
			low |= 1;
			if (steps == 0)
				break;
			dividend.v -= d;
		}
		if (steps == 0)
			break;
		d = __u64_shift_right_1bit(d);
	}
	quotient.w.lo = low;
	quotient.w.hi = high;
	return quotient.v;
}

