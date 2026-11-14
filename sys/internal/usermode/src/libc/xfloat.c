/*
 * The extended format strtod and _Dtento compute in: conversions,
 * multiply, add, subtract and powers of ten.
 */
#include <errno.h>
#include <stddef.h>
#include "xmath.h"

#define XBIAS		0x3fff			/* the extended exponent of 1.0 */
#define XLEAD		0x8000000000000000ULL	/* the explicit leading bit */

typedef union {
	float _Val;
	struct {
		unsigned int _Frac : 23;
		unsigned int _Exp : 8;
		unsigned int _Sign : 1;
	} _Bits;
} _Fbits;

typedef union {
	double _Val;
	struct {
		unsigned long long _Frac : 52;
		unsigned long long _Exp : 11;
		unsigned long long _Sign : 1;
	} _Bits;
} _Dbits;

typedef union {
	long double _Val;
	struct {
		unsigned long long _Frac : 52;
		unsigned long long _Exp : 11;
		unsigned long long _Sign : 1;
	} _Bits;
} _Ldbits;

/* 10^(2^i), i = 0..8 */
static const _Xfloat pow10s[9] = {
	{ 0x4002, 0xa000000000000000ULL },	/* 1e1 */
	{ 0x4005, 0xc800000000000000ULL },	/* 1e2 */
	{ 0x400c, 0x9c40000000000000ULL },	/* 1e4 */
	{ 0x4019, 0xbebc200000000000ULL },	/* 1e8 */
	{ 0x4034, 0x8e1bc9bf04000000ULL },	/* 1e16 */
	{ 0x4069, 0x9dc5ada82b70b59eULL },	/* 1e32 */
	{ 0x40d3, 0xc2781f49ffcfa6d5ULL },	/* 1e64 */
	{ 0x41a8, 0x93ba48c980e98ce0ULL },	/* 1e128 */
	{ 0x4351, 0xaa7eebfb9df9de8eULL },	/* 1e256 */
};

/* 10^-(2^i), i = 0..8 */
static const _Xfloat negpow10s[9] = {
	{ 0x3ffb, 0xcccccccccccccccdULL },	/* 1e-1 */
	{ 0x3ff8, 0xa3d70a3d70a3d70aULL },	/* 1e-2 */
	{ 0x3ff1, 0xd1b71758e219652cULL },	/* 1e-4 */
	{ 0x3fe4, 0xabcc77118461cefdULL },	/* 1e-8 */
	{ 0x3fc9, 0xe69594bec44de15bULL },	/* 1e-16 */
	{ 0x3f94, 0xcfb11ead453994baULL },	/* 1e-32 */
	{ 0x3f2a, 0xa87fea27a539e9a5ULL },	/* 1e-64 */
	{ 0x3e55, 0xddd0467c64bce4a1ULL },	/* 1e-128 */
	{ 0x3cac, 0xc0314325637a193aULL },	/* 1e-256 */
};

_Xfloat _Xfromf(float x)
{
	_Xfloat r;
	_Fbits u;
	int n = 8;

	u._Val = x;
	while (u._Bits._Exp == 0 && n < 32 && (u._Bits._Frac & (0x80000000 >> n)) == 0)
		++n;	/* denormal: find the leading bit */
	/* SCE: the fraction is shifted as an int, so the shift overflows */
	r._Frac = u._Bits._Frac << (n + 32) | XLEAD;
	r._Exp = u._Bits._Exp - (u._Bits._Exp == 0 ? n - 9 : 0) + (XBIAS - 0x7f);
	return r;
}

float _Xtof(_Xfloat x, int *perr)
{
	_Fbits u;
	int e = x._Exp - (XBIAS - 0x7f);

	u._Val = 0.0F;
	if (e < 0 || 0xfe < e) {	/* out of range: 0 or inf */
		u._Bits._Exp = e < 0 ? 0 : 0xff;
		errno = ERANGE;
		if (perr != NULL)
			*perr |= 1;
	} else {
		unsigned int hi = (unsigned int)(x._Frac >> 32), frac;

		u._Bits._Exp = e;
		frac = (hi & 0x7fffffff) >> (7 + (u._Bits._Exp != 0));
		u._Bits._Frac = frac + ((hi >> (6 + (u._Bits._Exp != 0))) & 1);
		u._Bits._Exp = e + (u._Bits._Frac < frac);	/* the rounding carried out */
	}
	return u._Val;
}

_Xfloat _Xfromld(long double x)
{
	_Xfloat r;
	_Ldbits u;
	int n = 11;

	u._Val = x;
	while (u._Bits._Exp == 0 && n < 64 && ((XLEAD >> n) & u._Bits._Frac) == 0)
		++n;	/* denormal: find the leading bit */
	r._Frac = u._Bits._Frac << n | XLEAD;
	r._Exp = u._Bits._Exp - (u._Bits._Exp == 0 ? n - 12 : 0) + (XBIAS - 0x3ff);
	return r;
}

double _Xtod(_Xfloat x, int *perr)
{
	_Dbits u;
	int e = x._Exp - (XBIAS - 0x3ff);

	u._Val = 0.0;
	if (e < 0 || 0x7fe < e) {	/* out of range: 0 or inf */
		u._Bits._Exp = e < 0 ? 0 : 0x7ff;
		errno = ERANGE;
		if (perr != NULL)
			*perr |= 1;
	} else {
		unsigned long long frac;

		u._Bits._Exp = e;
		frac = (x._Frac & 0x7ffffffffffffc00ULL) >> (11 + (u._Bits._Exp == 0));
		u._Bits._Frac = frac + (((unsigned int)x._Frac >> (10 + (u._Bits._Exp == 0))) & 1);
		u._Bits._Exp = u._Bits._Exp + (frac > u._Bits._Frac);	/* the rounding carried out */
	}
	return u._Val;
}

_Xfloat _Xfromd(double x)
{
	_Xfloat r;
	_Dbits u;
	int n = 11;

	u._Val = x;
	while (u._Bits._Exp == 0 && n < 64 && ((XLEAD >> n) & u._Bits._Frac) == 0)
		++n;	/* denormal: find the leading bit */
	r._Frac = u._Bits._Frac << n | XLEAD;
	r._Exp = u._Bits._Exp - (u._Bits._Exp == 0 ? n - 12 : 0) + (XBIAS - 0x3ff);
	return r;
}

long double _Xtold(_Xfloat x, int *perr)
{
	_Ldbits u;
	int e = x._Exp - (XBIAS - 0x3ff);

	u._Val = 0.0L;
	if (e < 0 || 0x7fe < e) {	/* out of range: 0 or inf */
		u._Bits._Exp = e < 0 ? 0 : 0x7ff;
		errno = ERANGE;
		if (perr != NULL)
			*perr |= 1;
	} else {
		unsigned long long frac;

		u._Bits._Exp = e;
		frac = (x._Frac & 0x7ffffffffffffc00ULL) >> (11 + (u._Bits._Exp == 0));
		u._Bits._Frac = frac + (((unsigned int)x._Frac >> (10 + (u._Bits._Exp == 0))) & 1);
		u._Bits._Exp = u._Bits._Exp + (frac > u._Bits._Frac);	/* the rounding carried out */
	}
	return u._Val;
}

_Xfloat _Xmul(_Xfloat x, _Xfloat y)
{
	_Xfloat r;
	unsigned int xl = (unsigned int)x._Frac, xh = (unsigned int)(x._Frac >> 32);
	unsigned int yl = (unsigned int)y._Frac, yh = (unsigned int)(y._Frac >> 32);
	unsigned long long mid, hi, frac;
	unsigned int fh, fl;
	int i;

	/* the 128-bit product: hi:mid holds its top 96 bits */
	mid = ((unsigned long long)xl * yl >> 32)
	      + (unsigned int)((unsigned long long)xh * yl)
	      + (unsigned int)((unsigned long long)xl * yh);
	hi = (mid >> 32) + ((unsigned long long)xh * yl >> 32)
	     + ((unsigned long long)xl * yh >> 32) + (unsigned long long)xh * yh;

	for (i = 0; i < 32; ++i)	/* normalize */
		if (((0x80000000 >> i) & (unsigned int)(hi >> 32)) != 0)
			break;
	fh = (unsigned int)(hi >> 32) << i | (unsigned int)hi >> (32 - i);
	fl = (unsigned int)hi << i;
	frac = ((unsigned long long)fh << 32 | fl) | (unsigned long long)(unsigned int)mid >> (32 - i);
	r._Frac = frac + ((mid >> (31 - i)) & 1);
	r._Exp = (frac > r._Frac) + (x._Exp + y._Exp - i) - (XBIAS - 1);
	return r;
}

_Xfloat _Xsub(_Xfloat x, _Xfloat y)
{
	_Xfloat r;
	unsigned int xl = (unsigned int)x._Frac, xh = (unsigned int)(x._Frac >> 32);
	unsigned int lo = xl - (unsigned int)y._Frac;
	unsigned int hi = xh - (unsigned int)(y._Frac >> 32);
	unsigned int newlo = 0;
	int neg, i;

	neg = xh < hi;
	if (xl < lo) {	/* borrow */
		--hi;
		neg |= hi == 0xffffffff;
	}
	if (neg) {	/* SCE: an approximate negation */
		hi = ~hi;
		lo = -lo;
	}

	if (hi == 0) {	/* SCE: the low word is not moved up */
		for (i = 0; i < 32; ++i)
			if (((0x80000000 >> i) & lo) != 0)
				break;
		i += 32;
	} else {
		for (i = 0; i < 32; ++i)
			if (((0x80000000 >> i) & hi) != 0)
				break;
		hi = hi << i | lo >> (32 - i);
		newlo = lo << i;
	}
	r._Frac = (unsigned long long)hi << 32 | newlo;
	r._Exp = x._Exp - i;
	return r;
}

_Xfloat _Xadd(_Xfloat x, _Xfloat y)
{
	_Xfloat r;
	unsigned long long t = y._Frac >> (unsigned int)(x._Exp - y._Exp);
	unsigned long long lo = (unsigned long long)(unsigned int)x._Frac + (unsigned int)t;
	unsigned long long hi = (x._Frac >> 32) + (t >> 32) + (lo >> 32);
	int i;

	for (i = 0; i < 64; ++i)	/* normalize */
		if (((XLEAD >> i) & hi) != 0)
			break;
	r._Frac = hi << i | (unsigned long long)(unsigned int)lo >> (32 - i);
	r._Exp = x._Exp + (32 - i);
	return r;
}

_Xfloat _Xtento(_Xfloat x, int n)
{
	_Xfloat r, p = { XBIAS, XLEAD };
	const _Xfloat *pp;
	unsigned int i;

	if (n < 0) {
		unsigned int m = -n;

		for (i = 0, pp = negpow10s; m != 0 && i < 8; ++i, ++pp) {
			if (m & 1)
				p = _Xmul(p, *pp);
			m >>= 1;
		}
		for (; m != 0; --m)
			p = _Xmul(p, negpow10s[8]);
		r = _Xmul(x, p);
	} else if (0 < n) {
		for (i = 0, pp = pow10s; 0 < n && i < 8; ++i, ++pp) {
			if (n & 1)
				p = _Xmul(p, *pp);
			n >>= 1;
		}
		for (; 0 < n; --n)
			p = _Xmul(p, pow10s[8]);
		r = _Xmul(x, p);
	}
	return r;
}
