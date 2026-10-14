/*
 * The 64-bit helpers of SNC's runtime library (libsnc i64div.o, u64div.o,
 * i64mod.o, u64mod.o, a64shl.o, l64shr.o, i64utils.o): SNC calls them for
 * 64-bit "/", "%" and shifts by a variable count.  Division is restoring
 * shift-and-subtract on the normalised divisor; divisions that fit 32 bits
 * go to __udivsi3 / __divsi3.
 *
 * GCC uses __aeabi_uldivmod / __aeabi_ldivmod instead (aeabi_ldivmod.S),
 * which return the remainder in r2:r3 and so are written in assembly.
 */

#ifdef __SNC__
/* each helper is its own object in libsnc: none is expanded into another */
#pragma control autoinlinesize=0
#endif

#include "common/runtime/i32ops.h"
#include "common/runtime/i64ops.h"

typedef union {
	unsigned long long v;
	struct {
		unsigned int lo;
		unsigned int hi;
	} w;
} u64parts;

long long __ashldi3(long long value, long long count)
{
	u64parts a, r;

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
	u64parts a, r;

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
	u64parts a;

	a.v = value;
	a.w.lo = a.w.lo >> 1 | a.w.hi << 31;
	a.w.hi >>= 1;
	return a.v;
}

static inline int clz64(u64parts a)
{
	return a.w.hi != 0 ? __builtin_clz(a.w.hi) : __builtin_clz(a.w.lo) + 32;
}

unsigned long long __udivdi3(unsigned long long n, unsigned long long d)
{
	u64parts nn, dd, rem, t, q;
	int shift, count, bit;

	nn.v = n;
	dd.v = d;
	if (dd.w.hi == 0) {
		if (nn.w.hi == 0)
			return __udivsi3(nn.w.lo, dd.w.lo);
		if (dd.w.lo == 1)
			return n;
		if (dd.w.lo == 0)
			return __aeabi_ldiv0(n);
		if ((dd.w.lo & -dd.w.lo) == dd.w.lo)
			return __lshrdi3(n, 31 - __builtin_clz(dd.w.lo));
	}
	if (d > n)
		return 0;
	if (nn.w.hi == dd.w.hi && nn.w.lo == dd.w.lo)
		return 1;

	shift = clz64(dd) - clz64(nn);
	count = shift + 1;
	if (shift != 0)
		dd.v = __ashldi3(dd.v, shift);
	q.v = 0;
	rem.v = n;
	for (;;) {
		t.v = rem.v - dd.v;
		count--;
		bit = (int)t.w.hi >= 0;
		q.w.hi = q.w.hi << 1 | q.w.lo >> 31;
		q.w.lo <<= 1;
		if (bit) {
			q.w.lo |= 1;
			if (count == 0)
				break;
			rem.v = t.v;
		} else if (count == 0) {
			break;
		}
		dd.v = __u64_shift_right_1bit(dd.v);
	}
	return q.v;
}

long long __divdi3(long long n, long long d)
{
	u64parts nn, dd;
	int nneg, dneg;
	long long q;

	nn.v = n;
	dd.v = d;
	/* a divisor that fits 32 bits */
	if ((int)dd.w.lo < 0 ? dd.w.hi == 0xffffffff : dd.w.hi == 0) {
		if ((int)nn.w.lo < 0 ? nn.w.hi == 0xffffffff : nn.w.hi == 0)
			return __divsi3(nn.w.lo, dd.w.lo);
		if ((int)dd.w.lo == 1)
			return n;
		if ((int)dd.w.lo == -1)
			return -n;
		if (dd.w.lo == 0)
			return __aeabi_ldiv0(n);
	}
	if (dd.w.hi == nn.w.hi && dd.w.lo == nn.w.lo)
		return 1;
	nneg = nn.w.hi >> 31;
	dneg = dd.w.hi >> 31;
	if (nneg)
		n = -n;
	if (dneg)
		d = -d;
	q = __udivdi3(n, d);
	if (nneg ^ dneg)
		q = -q;
	return q;
}

unsigned long long __umoddi3(unsigned long long n, unsigned long long d)
{
	return n - __udivdi3(n, d) * d;
}

long long __moddi3(long long n, long long d)
{
	return n - __divdi3(n, d) * d;
}
