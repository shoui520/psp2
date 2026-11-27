/*
 * sw_ecc.c — prime-field Jacobian-Montgomery ECC/ECDSA/ECDH and multi-word
 * big-integer arithmetic used by DTCPIP-SW.
 */
#include "sw_ecc.h"

#include <stddef.h>

struct SwBnPoint {
	uint32_t *x;
	uint32_t *y;
	uint32_t *z;
};

struct SwEccWinEntry {
	int bits;
	int tableIdx;
};

typedef void (*SwEccJacDoubleFn)(struct SwBnPoint *pt, const uint32_t *aMont,
				 const uint32_t *p, int words, uint32_t n0inv);

/* 3-bit sliding-window table for top bits 100..111 */
static const struct SwEccWinEntry g_swEccWindowTable[4] = {
	{ 1, 0 },
	{ 3, 2 },
	{ 2, 1 },
	{ 3, 3 },
};

static int bnCountLeadingZerosWord(uint32_t x)
{
	int n = 0;

	if (x == 0)
		return 32;
	if ((x & 0xFFFF0000u) == 0) {
		n += 16;
		x <<= 16;
	}
	if ((x & 0xFF000000u) == 0) {
		n += 8;
		x <<= 8;
	}
	if ((x & 0xF0000000u) == 0) {
		n += 4;
		x <<= 4;
	}
	if ((x & 0xC0000000u) == 0) {
		n += 2;
		x <<= 2;
	}
	if ((x & 0x80000000u) == 0)
		n += 1;
	return n;
}

static int bnCountLeadingZeros(const uint32_t *a, int words)
{
	int total = 0;
	int i;

	for (i = words - 1; i >= 0; --i) {
		int z = bnCountLeadingZerosWord(a[i]);

		total += z;
		if (z != 32)
			break;
	}
	return total;
}

static int bnIsZero(const uint32_t *a, int words)
{
	int i;

	for (i = words - 1; i >= 0; --i) {
		if (a[i] != 0)
			return 0;
	}
	return 1;
}

static uint32_t bnComputeN0Inv(uint32_t p0)
{
	uint32_t inv = 1;
	uint32_t neg = (uint32_t)(0u - p0);
	uint32_t bit;

	for (bit = 2; bit != 0; bit <<= 1)
		inv |= (inv * neg) & bit;
	return inv;
}

/* multi-word add with carry-in */
uint32_t swBnAdd(uint32_t *dst, const uint32_t *a, const uint32_t *b,
		 int words, uint32_t carryIn)
{
	uint32_t carry = carryIn & 1u;
	int i;

	for (i = 0; i < words; ++i) {
		uint32_t av = a[i];
		uint32_t bv = b[i];
		uint32_t sum = av + bv + carry;

		dst[i] = sum;
		carry = ((av & bv) | ((av | bv) & ~sum)) >> 31;
	}
	return carry;
}

/* multi-word subtract with borrow-in */
uint32_t swBnSub(uint32_t *dst, const uint32_t *a, const uint32_t *b,
		 int words, uint32_t borrowIn)
{
	uint32_t borrow = borrowIn & 1u;
	int i;

	for (i = 0; i < words; ++i) {
		uint32_t av = a[i];
		uint32_t bv = b[i];
		uint32_t diff = av - bv - borrow;

		dst[i] = diff;
		borrow = ((bv & ~av) | ((bv | ~av) & diff)) >> 31;
	}
	return borrow;
}

/* return 1 if a > b, 0 otherwise */
int swBnGt(const uint32_t *a, const uint32_t *b, int words)
{
	int i;

	if (words <= 0)
		return 0;
	for (i = words - 1; i > 0; --i) {
		if (a[i] != b[i])
			break;
	}
	return (a[i] > b[i]) ? 1 : 0;
}

/* return 1 if 0 < val < bound */
int swBnInRange(const uint32_t *val, const uint32_t *bound, int words)
{
	if (bnIsZero(val, words))
		return 0;
	return (swBnGt(bound, val, words) == 1) ? 1 : 0;
}

/* multi-word left shift by 0..31 bits */
void swBnShl(uint32_t *dst, const uint32_t *src, int words, unsigned int bits)
{
	unsigned int shift = bits & 0x3Fu;
	int i;

	if (shift == 0) {
		for (i = 0; i < words; ++i)
			dst[i] = src[i];
	} else {
		uint32_t carry = 0;

		for (i = 0; i < words; ++i) {
			uint32_t v = src[i];

			dst[i] = (v << shift) | carry;
			carry = v >> (32u - shift);
		}
	}
}

/* multi-word right shift by 0..31 bits */
void swBnShr(uint32_t *dst, const uint32_t *src, int words, unsigned int bits)
{
	unsigned int shift = bits & 0x3Fu;
	int i;

	if (shift == 0) {
		for (i = 0; i < words; ++i)
			dst[i] = src[i];
	} else {
		uint32_t carry = 0;

		for (i = words - 1; i >= 0; --i) {
			uint32_t v = src[i];

			dst[i] = (v >> shift) | carry;
			carry = v << (32u - shift);
		}
	}
}

/* multiply multi-word a[0..words-1] by 32-bit word into dst[0..words] */
void swBnMulWord(uint32_t *dst, const uint32_t *a, int words, uint32_t word)
{
	uint32_t carry = 0;
	int i;

	if (words <= 0)
		return;
	for (i = 0; i < words; ++i) {
		uint64_t prod = (uint64_t)a[i] * (uint64_t)word + (uint64_t)carry;

		dst[i] = (uint32_t)prod;
		carry = (uint32_t)(prod >> 32);
	}
	dst[words] = carry;
}

/* multi-word schoolbook multiply dst[0..aWords+bWords-1] = a * b */
void swBnMul(uint32_t *dst, const uint32_t *a, int aWords,
	     const uint32_t *b, int bWords)
{
	uint32_t prod[65];
	uint32_t acc[128];
	int total = aWords + bWords;
	int i;

	if (aWords <= 0 || aWords > 64 || bWords <= 0 || bWords > 64)
		return;
	for (i = 0; i < total; ++i)
		acc[i] = 0;
	for (i = 0; i < bWords; ++i) {
		swBnMulWord(prod, a, aWords, b[i]);
		swBnAdd(&acc[i], &acc[i], prod, aWords + 1, 0);
	}
	for (i = 0; i < total; ++i)
		dst[i] = acc[i];
}

/* multi-word remainder dst[0..modWords-1] = a mod m */
void swBnMod(uint32_t *dst, const uint32_t *a, int aWords,
	     const uint32_t *mod, int modWords)
{
	uint32_t mNorm[65];
	uint32_t qm[66];
	uint32_t rem[129];
	int aLz, mLz;
	int mActive, aActive;
	int shift;
	int i;

	if (aWords <= 0 || aWords > 128 || modWords <= 0 || modWords > 64)
		return;

	aLz = bnCountLeadingZeros(a, aWords);
	mLz = bnCountLeadingZeros(mod, modWords);
	mActive = modWords - (mLz >> 5);
	shift = mLz & 31;
	if (mActive == 0)
		return;

	aActive = aWords - (aLz >> 5);
	if (mActive > aActive || (mActive == aActive && swBnGt(mod, a, mActive))) {
		for (i = 0; i < aActive; ++i)
			dst[i] = a[i];
		for (i = aActive; i < modWords; ++i)
			dst[i] = 0;
		return;
	}

	mNorm[mActive] = 0;
	swBnShl(mNorm, mod, mActive, (unsigned int)shift);
	if ((aLz & 31) < shift)
		rem[aActive] = a[aActive - 1] >> (32 - shift);
	else
		rem[aActive] = 0;
	swBnShl(rem, a, aActive, (unsigned int)shift);

	{
		uint32_t dTop = mNorm[mActive - 1];
		int pos;

		for (pos = aActive; pos >= mActive; --pos) {
			uint32_t hi = rem[pos];
			uint32_t q;

			if (hi < dTop) {
				uint32_t lo = rem[pos - 1];
				int bit;

				for (bit = 0; bit < 32; ++bit) {
					uint32_t prevHi = hi;

					hi = (hi << 1) | (lo >> 31);
					lo <<= 1;
					if ((hi >= dTop) || ((prevHi >> 31) != 0)) {
						hi -= dTop;
						lo |= 1u;
					}
				}
				q = lo;
			} else {
				q = 0xFFFFFFFFu;
			}
			swBnMulWord(qm, mNorm, mActive, q);
			{
				uint32_t *slice = &rem[pos - mActive];
				uint32_t borrow = swBnSub(slice, slice, qm, mActive + 1, 0);

				while (borrow != 0)
					borrow ^= swBnAdd(slice, slice, mNorm, mActive + 1, 0);
			}
		}
	}

	swBnShr(dst, rem, modWords, (unsigned int)shift);
	for (i = mActive; i < modWords; ++i)
		dst[i] = 0;
}

/* binary extended GCD modular inverse: out = a^-1 mod m, returns 1 on success */
int swBnModInv(uint32_t *out, const uint32_t *a, const uint32_t *mod, int words)
{
	uint32_t u[64];
	uint32_t v[64];
	uint32_t x1[65];
	uint32_t y1[65];
	uint32_t x2[65];
	uint32_t y2[65];
	uint32_t orA = 0;
	uint32_t orM = 0;
	int i;

	if (words <= 0 || words > 64)
		return 0;
	if (((a[0] | mod[0]) & 1u) == 0)
		return 0;

	for (i = words - 1; i >= 0; --i) {
		orA |= a[i];
		orM |= mod[i];
		u[i] = a[i];
		v[i] = mod[i];
		x1[i + 1] = 0;
		y1[i + 1] = 0;
		x2[i + 1] = 0;
		y2[i + 1] = 0;
	}
	if (orA == 0 || orM == 0)
		return 0;

	x1[0] = 1;
	y1[0] = 0;
	x2[0] = 0;
	y2[0] = 1;

	do {
		while ((v[0] & 1u) == 0) {
			uint32_t cV = 0, cX, cY;

			if (((x1[0] | y1[0]) & 1u) != 0) {
				x1[words] += swBnAdd(x1, x1, a, words, 0);
				y1[words] -= swBnSub(y1, y1, mod, words, 0);
			}
			cX = x1[words] << 31;
			cY = y1[words] << 31;
			x1[words] = (uint32_t)((int32_t)x1[words] >> 1);
			y1[words] = (uint32_t)((int32_t)y1[words] >> 1);
			for (i = words - 1; i >= 0; --i) {
				uint32_t nv = v[i];
				uint32_t nx = x1[i];
				uint32_t ny = y1[i];

				v[i] = (nv >> 1) | cV;
				x1[i] = (nx >> 1) | cX;
				y1[i] = (ny >> 1) | cY;
				cV = nv << 31;
				cX = nx << 31;
				cY = ny << 31;
			}
		}
		while ((u[0] & 1u) == 0) {
			uint32_t cU = 0, cX, cY;

			if (((x2[0] | y2[0]) & 1u) != 0) {
				x2[words] += swBnAdd(x2, x2, a, words, 0);
				y2[words] -= swBnSub(y2, y2, mod, words, 0);
			}
			cX = x2[words] << 31;
			cY = y2[words] << 31;
			x2[words] = (uint32_t)((int32_t)x2[words] >> 1);
			y2[words] = (uint32_t)((int32_t)y2[words] >> 1);
			for (i = words - 1; i >= 0; --i) {
				uint32_t nu = u[i];
				uint32_t nx = x2[i];
				uint32_t ny = y2[i];

				u[i] = (nu >> 1) | cU;
				x2[i] = (nx >> 1) | cX;
				y2[i] = (ny >> 1) | cY;
				cU = nu << 31;
				cX = nx << 31;
				cY = ny << 31;
			}
		}
		if (swBnGt(u, v, words)) {
			swBnSub(u, u, v, words, 0);
			swBnSub(x2, x2, x1, words + 1, 0);
			swBnSub(y2, y2, y1, words + 1, 0);
		} else {
			swBnSub(v, v, u, words, 0);
			swBnSub(x1, x1, x2, words + 1, 0);
			swBnSub(y1, y1, y2, words + 1, 0);
		}
	} while (!bnIsZero(v, words));

	if (u[0] != 1)
		return 0;
	for (i = words - 1; i > 0; --i) {
		if (u[i] != 0)
			return 0;
	}

	if ((int32_t)y2[words] < 0) {
		do {
			y2[words] += swBnAdd(y2, y2, mod, words, 0);
		} while (y2[words] != 0);
	} else {
		while (y2[words] != 0 || !swBnGt(mod, y2, words)) {
			do {
				y2[words] -= swBnSub(y2, y2, mod, words, 0);
			} while (y2[words] != 0);
		}
	}

	for (i = 0; i < words; ++i)
		out[i] = y2[i];
	return 1;
}

/* big-endian bytes to little-endian 32-bit limbs */
void swBnFromBeBytes(uint32_t *dst, const unsigned char *src, int bytes)
{
	int words = bytes / 4;
	const unsigned char *p = src + bytes - 1;
	int i;

	for (i = 0; i < words; ++i) {
		uint32_t w = (uint32_t)p[0] | ((uint32_t)p[-1] << 8) |
			     ((uint32_t)p[-2] << 16) | ((uint32_t)p[-3] << 24);

		dst[i] = w;
		p -= 4;
	}
}

/* little-endian 32-bit limbs to big-endian bytes */
void swBnToBeBytes(unsigned char *dst, const uint32_t *src, int words)
{
	int i;
	int outPos = 0;

	for (i = words - 1; i >= 0; --i) {
		uint32_t w = src[i];

		dst[outPos++] = (unsigned char)(w >> 24);
		dst[outPos++] = (unsigned char)(w >> 16);
		dst[outPos++] = (unsigned char)(w >> 8);
		dst[outPos++] = (unsigned char)w;
	}
}

/* modular addition dst = (a + b) mod p */
void swBnModAdd(uint32_t *dst, const uint32_t *a, const uint32_t *b,
		const uint32_t *p, int words)
{
	uint32_t carry = swBnAdd(dst, a, b, words, 0);

	if (carry != 0 || !swBnGt(p, dst, words))
		swBnSub(dst, dst, p, words, 0);
}

/* modular subtraction dst = (a - b) mod p */
void swBnModSub(uint32_t *dst, const uint32_t *a, const uint32_t *b,
		const uint32_t *p, int words)
{
	uint32_t borrow = swBnSub(dst, a, b, words, 0);

	if (borrow != 0)
		swBnAdd(dst, dst, p, words, 0);
}

/* CIOS Montgomery multiplication dst = a * b * R^-1 mod p */
void swBnMontMul(uint32_t *dst, const uint32_t *a, const uint32_t *b,
		 const uint32_t *p, int words, uint32_t n0inv)
{
	uint32_t acc[65];
	uint32_t ab[65];
	uint32_t mp[65];
	int i;

	if (words <= 0 || words > 64)
		return;
	for (i = 0; i <= words; ++i)
		acc[i] = 0;

	for (i = 0; i < words; ++i) {
		uint32_t m = (acc[0] + a[i] * b[0]) * n0inv;
		uint32_t c1, c2;

		swBnMulWord(mp, p, words, m);
		swBnMulWord(ab, b, words, a[i]);
		c1 = swBnAdd(mp, mp, ab, words + 1, 0);
		c2 = swBnAdd(acc, &acc[1], &mp[1], words, (mp[0] | acc[0]) != 0u ? 1u : 0u);
		acc[words] = c1 + c2;
	}

	if (acc[words] != 0 || !swBnGt(p, acc, words))
		swBnSub(acc, acc, p, words, 0);
	for (i = 0; i < words; ++i)
		dst[i] = acc[i];
}

/* Montgomery reduction dst = a * R^-1 mod p */
void swBnMontReduce(uint32_t *dst, const uint32_t *a, int aWords,
		    const uint32_t *p, int words, uint32_t n0inv)
{
	uint32_t carry[64];
	uint32_t prod[66];
	uint32_t tmp[128];
	uint32_t topCarry;
	int i;

	if (words <= 0 || words > 64 || aWords < words || aWords > 2 * words)
		return;
	for (i = 0; i < words; ++i) {
		tmp[i] = a[i];
		tmp[words + i] = 0;
	}
	for (i = words; i < aWords; ++i)
		tmp[i] = a[i];

	for (i = 0; i < words; ++i) {
		swBnMulWord(prod, p, words, tmp[i] * n0inv);
		carry[i] = prod[words] + swBnAdd(&tmp[i], &tmp[i], prod, words, 0);
	}
	topCarry = swBnAdd(&tmp[words], &tmp[words], carry, words, 0);
	if (topCarry != 0 || !swBnGt(p, &tmp[words], words))
		swBnSub(&tmp[words], &tmp[words], p, words, 0);
	for (i = 0; i < words; ++i)
		dst[i] = tmp[words + i];
}

/* Jacobian point doubling (general curve parameter a) */
void swEccJacDouble(struct SwBnPoint *pt, const uint32_t *aMont,
		    const uint32_t *p, int words, uint32_t n0inv)
{
	uint32_t t1[7], t2[7], t3[7], t4[7];

	if (words <= 0 || words > 7 || bnIsZero(pt->z, words))
		return;

	swBnMontMul(t1, pt->x, pt->x, p, words, n0inv);
	swBnMontMul(t2, pt->z, pt->z, p, words, n0inv);
	swBnMontMul(t2, t2, t2, p, words, n0inv);
	swBnModAdd(t3, t1, t1, p, words);
	swBnModAdd(t1, t1, t3, p, words);
	swBnMontMul(t2, aMont, t2, p, words, n0inv);
	swBnModAdd(t1, t1, t2, p, words);

	swBnMontMul(t3, pt->y, pt->z, p, words, n0inv);
	swBnModAdd(pt->z, t3, t3, p, words);

	swBnMontMul(t2, pt->x, pt->y, p, words, n0inv);
	swBnMontMul(t2, t2, pt->y, p, words, n0inv);
	swBnModAdd(t2, t2, t2, p, words);
	swBnModAdd(t2, t2, t2, p, words);

	swBnMontMul(t4, t1, t1, p, words, n0inv);
	swBnModAdd(t3, t2, t2, p, words);
	swBnModSub(pt->x, t4, t3, p, words);

	swBnMontMul(t3, pt->y, pt->y, p, words, n0inv);
	swBnMontMul(t3, t3, t3, p, words, n0inv);
	swBnModAdd(t3, t3, t3, p, words);
	swBnModAdd(t3, t3, t3, p, words);
	swBnModAdd(t3, t3, t3, p, words);

	swBnModSub(t2, t2, pt->x, p, words);
	swBnMontMul(t1, t1, t2, p, words, n0inv);
	swBnModSub(pt->y, t1, t3, p, words);
}

/* Jacobian point doubling specialized for a = p - 3 */
void swEccJacDoubleA3(struct SwBnPoint *pt, const uint32_t *aMont,
		      const uint32_t *p, int words, uint32_t n0inv)
{
	uint32_t t1[7], t2[7], t3[7], t4[7];

	(void)aMont;
	if (words <= 0 || words > 7 || bnIsZero(pt->z, words))
		return;

	swBnMontMul(t1, pt->z, pt->z, p, words, n0inv);
	swBnModAdd(t2, pt->x, t1, p, words);
	swBnModSub(t3, pt->x, t1, p, words);
	swBnMontMul(t1, t2, t3, p, words, n0inv);
	swBnModAdd(t3, t1, t1, p, words);
	swBnModAdd(t1, t3, t1, p, words);

	swBnMontMul(t3, pt->y, pt->z, p, words, n0inv);
	swBnModAdd(pt->z, t3, t3, p, words);

	swBnMontMul(t2, pt->x, pt->y, p, words, n0inv);
	swBnMontMul(t2, t2, pt->y, p, words, n0inv);
	swBnModAdd(t2, t2, t2, p, words);
	swBnModAdd(t2, t2, t2, p, words);

	swBnMontMul(t4, t1, t1, p, words, n0inv);
	swBnModAdd(t3, t2, t2, p, words);
	swBnModSub(pt->x, t4, t3, p, words);

	swBnMontMul(t3, pt->y, pt->y, p, words, n0inv);
	swBnMontMul(t3, t3, t3, p, words, n0inv);
	swBnModAdd(t3, t3, t3, p, words);
	swBnModAdd(t3, t3, t3, p, words);
	swBnModAdd(t3, t3, t3, p, words);

	swBnModSub(t2, t2, pt->x, p, words);
	swBnMontMul(t1, t1, t2, p, words, n0inv);
	swBnModSub(pt->y, t1, t3, p, words);
}

/* Jacobian point addition P1 += P2 */
void swEccJacAdd(struct SwBnPoint *p1, const struct SwBnPoint *p2,
		 const uint32_t *aMont, const uint32_t *p, int words, uint32_t n0inv)
{
	uint32_t u2[7], u1[7], h[7], s2[7], s1[7], r[7];
	int i;

	if (words <= 0 || words > 7)
		return;
	if (bnIsZero(p1->z, words)) {
		for (i = 0; i < words; ++i) {
			p1->x[i] = p2->x[i];
			p1->y[i] = p2->y[i];
			p1->z[i] = p2->z[i];
		}
		return;
	}
	if (bnIsZero(p2->z, words))
		return;

	swBnMontMul(s2, p1->z, p1->z, p, words, n0inv);
	swBnMontMul(s1, p2->z, p2->z, p, words, n0inv);
	swBnMontMul(u2, p2->x, s2, p, words, n0inv);
	swBnMontMul(u1, p1->x, s1, p, words, n0inv);
	swBnModSub(h, u2, u1, p, words);
	swBnMontMul(s2, s2, p1->z, p, words, n0inv);
	swBnMontMul(s1, s1, p2->z, p, words, n0inv);
	swBnMontMul(s2, s2, p2->y, p, words, n0inv);
	swBnMontMul(s1, s1, p1->y, p, words, n0inv);
	swBnModSub(r, s2, s1, p, words);

	if (bnIsZero(h, words)) {
		if (bnIsZero(r, words)) {
			swEccJacDouble(p1, aMont, p, words, n0inv);
		} else {
			for (i = 1; i < words; ++i) {
				p1->x[i] = 0;
				p1->y[i] = 0;
				p1->z[i] = 0;
			}
			p1->x[0] = 1;
			p1->y[0] = 1;
			p1->z[0] = 0;
		}
		return;
	}

	swBnModAdd(u2, u2, u1, p, words);
	swBnModAdd(s2, s2, s1, p, words);
	swBnMontMul(s1, p2->z, p1->z, p, words, n0inv);
	swBnMontMul(p1->z, s1, h, p, words, n0inv);
	swBnMontMul(u1, r, r, p, words, n0inv);
	swBnMontMul(s1, h, h, p, words, n0inv);
	swBnMontMul(u2, u2, s1, p, words, n0inv);
	swBnModSub(p1->x, u1, u2, p, words);
	swBnModSub(u2, u2, p1->x, p, words);
	swBnModSub(u2, u2, p1->x, p, words);
	swBnMontMul(u2, u2, r, p, words, n0inv);
	swBnMontMul(s1, s1, h, p, words, n0inv);
	swBnMontMul(s2, s2, s1, p, words, n0inv);
	swBnModSub(s1, u2, s2, p, words);

	{
		uint32_t carry = 0;

		if ((s1[0] & 1u) != 0)
			carry = swBnAdd(s1, s1, p, words, 0) << 31;
		for (i = words - 1; i >= 0; --i) {
			uint32_t w = s1[i];

			p1->y[i] = (w >> 1) | carry;
			carry = w << 31;
		}
	}
}

/* convert affine point to Montgomery Jacobian point */
void swEccAffineToMontJac(struct SwBnPoint *dst, const struct SwBnPoint *src,
			  const uint32_t *r2, const uint32_t *p, int words, uint32_t n0inv)
{
	int i;

	if (bnIsZero(src->x, words) && bnIsZero(src->y, words)) {
		for (i = 1; i < words; ++i) {
			dst->x[i] = 0;
			dst->y[i] = 0;
			dst->z[i] = 0;
		}
		dst->x[0] = 1;
		dst->y[0] = 1;
		dst->z[0] = 0;
		return;
	}
	swBnMontMul(dst->x, src->x, r2, p, words, n0inv);
	swBnMontMul(dst->y, src->y, r2, p, words, n0inv);
	swBnMontReduce(dst->z, r2, words, p, words, n0inv);
}

/* compute R^2 mod p and convert up to two points + curve a to Montgomery form */
void swEccMontSetup(struct SwBnPoint *dst1, struct SwBnPoint *dst2, uint32_t *aMont,
		    const struct SwBnPoint *src1, const struct SwBnPoint *src2,
		    const uint32_t *a, const uint32_t *p, int words, uint32_t n0inv)
{
	uint32_t r2[15];
	int i;

	if (words <= 0 || words > 7)
		return;
	for (i = 0; i < 2 * words; ++i)
		r2[i] = 0;
	r2[2 * words] = 1;
	swBnMod(r2, r2, 2 * words + 1, p, words);
	if (src1 != NULL)
		swEccAffineToMontJac(dst1, src1, r2, p, words, n0inv);
	if (src2 != NULL)
		swEccAffineToMontJac(dst2, src2, r2, p, words, n0inv);
	if (a != NULL)
		swBnMontMul(aMont, r2, a, p, words, n0inv);
}

/* convert Montgomery Jacobian point back to affine coordinates */
void swEccMontJacToAffine(struct SwBnPoint *dst, const struct SwBnPoint *src,
			  const uint32_t *p, int words, uint32_t n0inv)
{
	uint32_t zinv[7];
	int i;

	if (words <= 0 || words > 7)
		return;
	if (bnIsZero(src->z, words)) {
		for (i = 0; i < words; ++i) {
			dst->x[i] = 0;
			dst->y[i] = 0;
		}
		return;
	}
	swBnMontMul(zinv, src->z, src->z, p, words, n0inv);
	swBnMontMul(zinv, zinv, src->z, p, words, n0inv);
	swBnMontReduce(zinv, zinv, words, p, words, n0inv);
	swBnModInv(zinv, zinv, p, words);
	swBnMontMul(dst->y, src->y, zinv, p, words, n0inv);
	swBnMontMul(zinv, zinv, src->z, p, words, n0inv);
	swBnMontMul(dst->x, src->x, zinv, p, words, n0inv);
}

/* 3-bit sliding-window Jacobian scalar multiplication out = k * pt */
void swEccJacMulWindow(struct SwBnPoint *out, const struct SwBnPoint *pt,
		       const uint32_t *k, const uint32_t *aMont, const uint32_t *p,
		       int words, uint32_t n0inv, SwEccJacDoubleFn dbl)
{
	uint32_t accX[7], accY[7], accZ[7];
	uint32_t kShift[7];
	uint32_t tblX[4][7], tblY[4][7], tblZ[4][7];
	struct SwBnPoint acc;
	struct SwBnPoint tbl[4];
	int lz, remBits, activeWords;
	unsigned int bitPos;
	uint32_t *kPtr;
	uint32_t *kTop;
	int skipDbl;
	int i;

	acc.x = accX;
	acc.y = accY;
	acc.z = accZ;
	for (i = 0; i < 4; ++i) {
		tbl[i].x = tblX[i];
		tbl[i].y = tblY[i];
		tbl[i].z = tblZ[i];
	}

	for (i = 0; i < words; ++i) {
		accX[i] = tblX[0][i] = tblX[1][i] = pt->x[i];
		accY[i] = tblY[0][i] = tblY[1][i] = pt->y[i];
		accZ[i] = tblZ[0][i] = tblZ[1][i] = pt->z[i];
	}

	/* acc = 2*P; tbl[0]=P, tbl[1]=3*P, tbl[2]=5*P, tbl[3]=7*P */
	dbl(&acc, aMont, p, words, n0inv);
	swEccJacAdd(&tbl[1], &acc, aMont, p, words, n0inv);
	for (i = 0; i < words; ++i) {
		tblX[2][i] = tblX[1][i];
		tblY[2][i] = tblY[1][i];
		tblZ[2][i] = tblZ[1][i];
	}
	swEccJacAdd(&tbl[2], &acc, aMont, p, words, n0inv);
	for (i = 0; i < words; ++i) {
		tblX[3][i] = tblX[2][i];
		tblY[3][i] = tblY[2][i];
		tblZ[3][i] = tblZ[2][i];
	}
	swEccJacAdd(&tbl[3], &acc, aMont, p, words, n0inv);

	lz = bnCountLeadingZeros(k, words);
	remBits = 32 * words - lz;
	activeWords = words - (lz >> 5);
	bitPos = (unsigned int)(lz & 31);
	kPtr = kShift;
	swBnShl(kShift, k, activeWords, bitPos);
	kTop = &kShift[activeWords - 1];

	for (i = 1; i < words; ++i) {
		accX[i] = 0;
		accY[i] = 0;
		accZ[i] = 0;
	}
	accX[0] = 1;
	accY[0] = 1;
	accZ[0] = 0;
	skipDbl = 4;

	while (activeWords > 0) {
		unsigned int idx = (*kTop >> 29) & 3u;
		int winBits = g_swEccWindowTable[idx].bits;
		int tblIdx = g_swEccWindowTable[idx].tableIdx;
		int j;

		for (j = skipDbl; j < winBits; ++j)
			dbl(&acc, aMont, p, words, n0inv);
		swEccJacAdd(&acc, &tbl[tblIdx], aMont, p, words, n0inv);
		swBnShl(kPtr, kPtr, activeWords, (unsigned int)winBits);
		bitPos += (unsigned int)winBits;
		remBits -= winBits;
		if (bitPos >= 32u) {
			--activeWords;
			++kPtr;
			bitPos &= 31u;
		}
		skipDbl = 0;
		while (((*kTop & 0x80000000u) == 0u) && remBits > 0) {
			swBnShl(kPtr, kPtr, activeWords, 1u);
			++bitPos;
			++skipDbl;
			--remBits;
			if (bitPos == 32u) {
				--activeWords;
				++kPtr;
				bitPos = 0;
			}
		}
		while (skipDbl > 0) {
			--skipDbl;
			dbl(&acc, aMont, p, words, n0inv);
		}
	}

	for (i = 0; i < words; ++i) {
		out->x[i] = accX[i];
		out->y[i] = accY[i];
		out->z[i] = accZ[i];
	}
}

/* affine scalar multiplication out = k * pt via Montgomery Jacobian */
void swEccAffineScalarMul(struct SwBnPoint *out, const struct SwBnPoint *pt,
			  const uint32_t *p, const uint32_t *a, const uint32_t *k, int words)
{
	uint32_t aMont[7];
	uint32_t resX[7], resY[7], resZ[7];
	uint32_t inX[7], inY[7], inZ[7];
	struct SwBnPoint resPt, inPt;
	SwEccJacDoubleFn dbl;
	uint32_t n0inv;
	uint32_t carry = 3;
	int i;

	if (words <= 0 || words > 7)
		return;
	resPt.x = resX;
	resPt.y = resY;
	resPt.z = resZ;
	inPt.x = inX;
	inPt.y = inY;
	inPt.z = inZ;

	for (i = 0; i < words; ++i) {
		uint32_t sum = a[i] + carry;

		if (sum != p[i])
			break;
		carry = (a[i] & ~sum) >> 31;
	}
	dbl = (carry == 0 && i == words) ? swEccJacDoubleA3 : swEccJacDouble;
	n0inv = bnComputeN0Inv(p[0]);

	swEccMontSetup(&inPt, NULL, aMont, pt, NULL, a, p, words, n0inv);
	swEccJacMulWindow(&resPt, &inPt, k, aMont, p, words, n0inv, dbl);
	swEccMontJacToAffine(out, &resPt, p, words, n0inv);
}

/* Shamir simultaneous Jacobian multiplication out = u1*P + u2*Q */
void swEccJacShamirMul(struct SwBnPoint *out, const struct SwBnPoint *pPt,
		       const struct SwBnPoint *qPt, const uint32_t *u1,
		       const uint32_t *u2, const uint32_t *aMont, const uint32_t *p,
		       int words, uint32_t n0inv, SwEccJacDoubleFn dbl)
{
	uint32_t pqX[7], pqY[7], pqZ[7];
	uint32_t accX[7], accY[7], accZ[7];
	struct SwBnPoint pq, acc;
	int lz1, lz2, maxBits, bit;
	int i;

	pq.x = pqX;
	pq.y = pqY;
	pq.z = pqZ;
	acc.x = accX;
	acc.y = accY;
	acc.z = accZ;

	lz1 = bnCountLeadingZeros(u1, words);
	lz2 = bnCountLeadingZeros(u2, words);
	maxBits = 32 * words - ((lz1 < lz2) ? lz1 : lz2);

	for (i = 0; i < words; ++i) {
		pqX[i] = pPt->x[i];
		pqY[i] = pPt->y[i];
		pqZ[i] = pPt->z[i];
	}
	swEccJacAdd(&pq, qPt, aMont, p, words, n0inv);

	for (i = 1; i < words; ++i) {
		accX[i] = 0;
		accY[i] = 0;
		accZ[i] = 0;
	}
	accX[0] = 1;
	accY[0] = 1;
	accZ[0] = 0;

	for (bit = maxBits - 1; bit >= 0; --bit) {
		unsigned int b1 = (u1[bit >> 5] >> (bit & 31)) & 1u;
		unsigned int b2 = (u2[bit >> 5] >> (bit & 31)) & 1u;
		unsigned int sel = (b1 << 1) | b2;

		dbl(&acc, aMont, p, words, n0inv);
		if (sel == 3u)
			swEccJacAdd(&acc, &pq, aMont, p, words, n0inv);
		else if (sel == 2u)
			swEccJacAdd(&acc, pPt, aMont, p, words, n0inv);
		else if (sel == 1u)
			swEccJacAdd(&acc, qPt, aMont, p, words, n0inv);
	}

	for (i = 0; i < words; ++i) {
		out->x[i] = accX[i];
		out->y[i] = accY[i];
		out->z[i] = accZ[i];
	}
}

/* affine Shamir simultaneous multiplication out = u1*P + u2*Q */
void swEccAffineShamirMul(struct SwBnPoint *out, const struct SwBnPoint *pPt,
			  const struct SwBnPoint *qPt, const uint32_t *u1,
			  const uint32_t *u2, const uint32_t *p, const uint32_t *a,
			  int words)
{
	uint32_t aMont[7];
	uint32_t resX[7], resY[7], resZ[7];
	uint32_t pX[7], pY[7], pZ[7];
	uint32_t qX[7], qY[7], qZ[7];
	struct SwBnPoint resPt, mP, mQ;
	SwEccJacDoubleFn dbl;
	uint32_t n0inv;
	uint32_t carry = 3;
	int i;

	if (words <= 0 || words > 7)
		return;
	resPt.x = resX;
	resPt.y = resY;
	resPt.z = resZ;
	mP.x = pX;
	mP.y = pY;
	mP.z = pZ;
	mQ.x = qX;
	mQ.y = qY;
	mQ.z = qZ;

	for (i = 0; i < words; ++i) {
		uint32_t sum = a[i] + carry;

		if (sum != p[i])
			break;
		carry = (a[i] & ~sum) >> 31;
	}
	dbl = (carry == 0 && i == words) ? swEccJacDoubleA3 : swEccJacDouble;
	n0inv = bnComputeN0Inv(p[0]);

	swEccMontSetup(&mP, &mQ, aMont, pPt, qPt, a, p, words, n0inv);
	swEccJacShamirMul(&resPt, &mP, &mQ, u1, u2, aMont, p, words, n0inv, dbl);
	swEccMontJacToAffine(out, &resPt, p, words, n0inv);
}

/* base-point scalar multiplication core */
static int swEccScalarMulBaseCore(struct SwEccPoint *out, const unsigned char *scalar,
				  const struct SwEccCurve *curve, int words, int bytes)
{
	uint32_t p[7], a[7], n[7], k[7];
	uint32_t gx[7], gy[7], gz[7];
	uint32_t rx[7], ry[7], rz[7];
	struct SwBnPoint genPt, resPt;

	genPt.x = gx;
	genPt.y = gy;
	genPt.z = gz;
	resPt.x = rx;
	resPt.y = ry;
	resPt.z = rz;

	swBnFromBeBytes(p, curve->prime, bytes);
	swBnFromBeBytes(a, curve->coeffA, bytes);
	swBnFromBeBytes(n, curve->order, bytes);
	swBnFromBeBytes(gx, curve->genX, bytes);
	swBnFromBeBytes(gy, curve->genY, bytes);
	swBnFromBeBytes(k, scalar, bytes);

	if (!swBnInRange(k, n, words))
		return -1;
	swEccAffineScalarMul(&resPt, &genPt, p, a, k, words);
	swBnToBeBytes(out->x, rx, words);
	swBnToBeBytes(out->y, ry, words);
	return 0;
}

/* 160-bit base-point scalar multiplication */
int swEccScalarMulBase(struct SwEccPoint *out, const unsigned char *scalar,
		       const struct SwEccCurve *curve)
{
	return swEccScalarMulBaseCore(out, scalar, curve, 5, 20);
}

/* 224-bit base-point scalar multiplication */
int swEccScalarMulBase224(struct SwEccPoint *out, const unsigned char *scalar,
			  const struct SwEccCurve *curve)
{
	return swEccScalarMulBaseCore(out, scalar, curve, 7, 28);
}

/* arbitrary-point scalar multiplication core */
static int swEccScalarMulPointCore(struct SwEccPoint *out, const struct SwEccPoint *point,
				   const unsigned char *scalar, const struct SwEccCurve *curve,
				   int words, int bytes)
{
	uint32_t p[7], a[7], n[7], k[7];
	uint32_t px[7], py[7], pz[7];
	uint32_t rx[7], ry[7], rz[7];
	struct SwBnPoint inPt, resPt;

	inPt.x = px;
	inPt.y = py;
	inPt.z = pz;
	resPt.x = rx;
	resPt.y = ry;
	resPt.z = rz;

	swBnFromBeBytes(p, curve->prime, bytes);
	swBnFromBeBytes(a, curve->coeffA, bytes);
	swBnFromBeBytes(n, curve->order, bytes);
	swBnFromBeBytes(px, point->x, bytes);
	swBnFromBeBytes(py, point->y, bytes);

	if (bnIsZero(p, words))
		return -1;
	swBnMod(px, px, words, p, words);
	swBnMod(py, py, words, p, words);
	swBnFromBeBytes(k, scalar, bytes);

	if (!swBnInRange(k, n, words))
		return -1;
	swEccAffineScalarMul(&resPt, &inPt, p, a, k, words);
	swBnToBeBytes(out->x, rx, words);
	swBnToBeBytes(out->y, ry, words);
	return 0;
}

/* 160-bit arbitrary-point scalar multiplication */
int swEccScalarMulPoint(struct SwEccPoint *out, const unsigned char *scalar,
			const struct SwEccPoint *point, const struct SwEccCurve *curve)
{
	return swEccScalarMulPointCore(out, point, scalar, curve, 5, 20);
}

/* 224-bit arbitrary-point scalar multiplication */
int swEccScalarMulPoint224(struct SwEccPoint *out, const unsigned char *scalar,
			   const struct SwEccPoint *point, const struct SwEccCurve *curve)
{
	return swEccScalarMulPointCore(out, point, scalar, curve, 7, 28);
}

/* ECDSA signing core */
static int swEccEcdsaSignCore(struct SwEccPoint *sig, const unsigned char *hash,
			      const unsigned char *nonce, const unsigned char *privKey,
			      const struct SwEccCurve *curve, int words, int bytes)
{
	uint32_t p[7], a[7], n[7], k[7], r[7], e[7], d[7];
	uint32_t gx[7], gy[7], gz[7];
	uint32_t rx[7], ry[7], rz[7];
	uint32_t tmp[14];
	struct SwBnPoint genPt, resPt;

	genPt.x = gx;
	genPt.y = gy;
	genPt.z = gz;
	resPt.x = rx;
	resPt.y = ry;
	resPt.z = rz;

	swBnFromBeBytes(p, curve->prime, bytes);
	swBnFromBeBytes(a, curve->coeffA, bytes);
	swBnFromBeBytes(n, curve->order, bytes);
	swBnFromBeBytes(gx, curve->genX, bytes);
	swBnFromBeBytes(gy, curve->genY, bytes);
	swBnFromBeBytes(d, privKey, bytes);
	swBnFromBeBytes(k, nonce, bytes);
	swBnFromBeBytes(e, hash, bytes);

	if (!swBnInRange(d, n, words))
		return -1;
	if (!swBnInRange(k, n, words))
		return -1;

	swEccAffineScalarMul(&resPt, &genPt, p, a, k, words);
	swBnMod(r, rx, words, n, words);
	if (bnIsZero(r, words))
		return -1;

	swBnMul(tmp, d, words, r, words);
	tmp[words] += swBnAdd(tmp, tmp, e, words, 0);
	swBnMod(tmp, tmp, 2 * words, n, words);
	swBnModInv(k, k, n, words);
	swBnMul(tmp, tmp, words, k, words);
	swBnMod(tmp, tmp, 2 * words, n, words);
	if (bnIsZero(tmp, words))
		return -1;

	swBnToBeBytes(sig->x, r, words);
	swBnToBeBytes(sig->y, tmp, words);
	return 0;
}

/* 160-bit ECDSA signing */
int swEccEcdsaSign(struct SwEccPoint *sig, const unsigned char *hash,
		   const unsigned char *nonce, const unsigned char *privKey,
		   const struct SwEccCurve *curve)
{
	return swEccEcdsaSignCore(sig, hash, nonce, privKey, curve, 5, 20);
}

/* 224-bit ECDSA signing */
int swEccEcdsaSign224(struct SwEccPoint *sig, const unsigned char *hash,
		      const unsigned char *nonce, const unsigned char *privKey,
		      const struct SwEccCurve *curve)
{
	return swEccEcdsaSignCore(sig, hash, nonce, privKey, curve, 7, 28);
}

/* ECDSA verification core */
static int swEccEcdsaVerifyCore(const struct SwEccPoint *sig, const unsigned char *hash,
				const struct SwEccPoint *pubKey, const struct SwEccCurve *curve,
				int words, int bytes)
{
	uint32_t p[7], a[7], n[7], r[7], s[7], e[7];
	uint32_t gx[7], gy[7], gz[7];
	uint32_t qx[7], qy[7], qz[7];
	uint32_t u1[7], u2[14];
	struct SwBnPoint genPt, pubPt;
	int i;

	genPt.x = gx;
	genPt.y = gy;
	genPt.z = gz;
	pubPt.x = qx;
	pubPt.y = qy;
	pubPt.z = qz;

	swBnFromBeBytes(p, curve->prime, bytes);
	swBnFromBeBytes(a, curve->coeffA, bytes);
	swBnFromBeBytes(n, curve->order, bytes);
	swBnFromBeBytes(gx, curve->genX, bytes);
	swBnFromBeBytes(gy, curve->genY, bytes);
	swBnFromBeBytes(e, hash, bytes);
	swBnFromBeBytes(qx, pubKey->x, bytes);
	swBnFromBeBytes(qy, pubKey->y, bytes);

	if (bnIsZero(p, words))
		return -1;
	swBnMod(qx, qx, words, p, words);
	swBnMod(qy, qy, words, p, words);
	swBnFromBeBytes(r, sig->x, bytes);
	swBnFromBeBytes(s, sig->y, bytes);

	if (!swBnInRange(r, n, words) || !swBnInRange(s, n, words))
		return -1;

	swBnModInv(s, s, n, words);
	swBnMul(u2, e, words, s, words);
	swBnMod(u1, u2, 2 * words, n, words);
	swBnMul(u2, r, words, s, words);
	swBnMod(u2, u2, 2 * words, n, words);

	swEccAffineShamirMul(&pubPt, &genPt, &pubPt, u1, u2, p, a, words);
	swBnMod(qx, qx, words, n, words);

	if (words <= 0)
		return -1;
	for (i = 0; i < words; ++i) {
		if (qx[i] != r[i])
			return -1;
	}
	return 0;
}

/* 160-bit ECDSA verification */
int swEccEcdsaVerify(const struct SwEccPoint *sig, const unsigned char *hash,
		     const struct SwEccPoint *pubKey, const struct SwEccCurve *curve)
{
	return swEccEcdsaVerifyCore(sig, hash, pubKey, curve, 5, 20);
}

/* 224-bit ECDSA verification */
int swEccEcdsaVerify224(const struct SwEccPoint *sig, const unsigned char *hash,
			const struct SwEccPoint *pubKey, const struct SwEccCurve *curve)
{
	return swEccEcdsaVerifyCore(sig, hash, pubKey, curve, 7, 28);
}
