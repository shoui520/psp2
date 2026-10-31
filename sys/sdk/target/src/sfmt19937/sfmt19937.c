/*
 * libSceSfmt19937 (the SDK's static library, sfmt19937.o): SFMT-19937, Saito and Matsumoto's
 * SIMD-oriented Fast Mersenne Twister, on NEON. Sony's object is hand-written assembly; this is
 * the same steps in C. Names are the archive's symbols.
 */
#include <libsfmt19937.h>
#include <arm_neon.h>
#include <stddef.h>

#define SFMT_N		SCE_SFMT19937_ARRAY_SIZE	/* 128-bit words of state */
#define SFMT_N32	(SFMT_N * 4)
#define SFMT_POS1	122

static const SceUInt32 _sceSfmt19937Parity[4] = { 0x00000001, 0x00000000, 0x00000000, 0x13c9e684 };
static const SceUInt32 _sceSfmt19937Mask[4] = { 0xdfffffef, 0xddfecb7f, 0xbffaffff, 0xbffffff6 };

/* one step of the recursion: a ^ (a << 8) ^ ((b >> 11) & mask) ^ (c >> 8) ^ (d << 18) */
static inline __attribute__((always_inline)) uint32x4_t
sfmtRecursion(uint32x4_t a, uint32x4_t b, uint32x4_t c, uint32x4_t d, uint32x4_t mask)
{
	const uint8x16_t zero = vdupq_n_u8(0);
	uint32x4_t x;

	x = veorq_u32(a, vreinterpretq_u32_u8(vextq_u8(zero, vreinterpretq_u8_u32(a), 15)));
	x = veorq_u32(x, vandq_u32(vshrq_n_u32(b, 11), mask));
	x = veorq_u32(x, vreinterpretq_u32_u8(vextq_u8(vreinterpretq_u8_u32(c), zero, 1)));
	return veorq_u32(x, vshlq_n_u32(d, 18));
}

SceUInt32 sceSfmt19937GenRand32(SceSfmt19937Context *pCtx);
SceUInt64 sceSfmt19937GenRand64(SceSfmt19937Context *pCtx);
static SceUInt64 _sceSfmt19937GenRandCore(SceSfmt19937Context *pCtx, SceUInt32 used);

SceUInt32 sceSfmt19937GenRand32(SceSfmt19937Context *pCtx)
{
	SceUInt32 idx = pCtx->idx;

	if (idx >= SFMT_N32)
		return (SceUInt32)_sceSfmt19937GenRandCore(pCtx, 1);
	pCtx->idx = idx + 1;
	return ((SceUInt32 *)pCtx->sfmt)[idx];
}

/* an odd index takes the word there and a zero high half */
SceUInt64 sceSfmt19937GenRand64(SceSfmt19937Context *pCtx)
{
	SceUInt32 idx = pCtx->idx;
	const SceUInt32 *word;
	SceUInt32 high;

	if (idx >= SFMT_N32)
		return _sceSfmt19937GenRandCore(pCtx, 2);
	pCtx->idx = idx + 2 - (idx & 1);
	word = &((const SceUInt32 *)pCtx->sfmt)[idx];
	high = (idx & 1) == 0 ? word[1] : 0;
	return (SceUInt64)high << 32 | word[0];
}

/* regenerates the state; USED (1 or 2) words of it are returned and consumed */
static SceUInt64 _sceSfmt19937GenRandCore(SceSfmt19937Context *pCtx, SceUInt32 used)
{
	SceUInt32 *state = &pCtx->sfmt[0][0];
	const uint32x4_t mask = vld1q_u32(_sceSfmt19937Mask);
	uint32x4_t r1, r2, r;
	int i;

	r1 = vld1q_u32(&state[(SFMT_N - 2) * 4]);
	r2 = vld1q_u32(&state[(SFMT_N - 1) * 4]);
	for (i = 0; i < SFMT_N - SFMT_POS1; i++) {
		r = sfmtRecursion(vld1q_u32(&state[i * 4]), vld1q_u32(&state[(i + SFMT_POS1) * 4]),
				  r1, r2, mask);
		r1 = r2;
		r2 = r;
		vst1q_u32(&state[i * 4], r);
	}
	for (; i < SFMT_N; i++) {
		r = sfmtRecursion(vld1q_u32(&state[i * 4]),
				  vld1q_u32(&state[(i + SFMT_POS1 - SFMT_N) * 4]), r1, r2, mask);
		r1 = r2;
		r2 = r;
		vst1q_u32(&state[i * 4], r);
	}
	pCtx->idx = used;
	return (used == 1 ? 0 : (SceUInt64)state[1] << 32) | state[0];
}

/* SIZE 32-bit words: a multiple of four */
SceInt32 sceSfmt19937FillArray32(SceSfmt19937Context *pCtx, SceUInt32 array[], SceUInt32 size)
{
	if (pCtx == NULL || array == NULL)
		return SCE_SFMT19937_ERROR_INVALID_POINTER;
	if ((size & 3) != 0)
		return SCE_SFMT19937_ERROR_INVALID_STATE;
	return sceSfmt19937FillArray64(pCtx, (SceUInt64 *)array, size >> 1);
}

static SceInt32 _sceSfmt19937FillArrayCore(SceSfmt19937Context *pCtx, SceUInt32 *array, int size);

/* SIZE 64-bit words: even, at least the state's size, and only from a fresh state */
SceInt32 sceSfmt19937FillArray64(SceSfmt19937Context *pCtx, SceUInt64 array[], SceUInt32 size)
{
	if (pCtx == NULL || array == NULL)
		return SCE_SFMT19937_ERROR_INVALID_POINTER;
	if ((size & 1) != 0)
		return SCE_SFMT19937_ERROR_INVALID_STATE;
	if (pCtx->idx != SFMT_N32 || (int)(size >> 1) < SFMT_N)
		return SCE_SFMT19937_ERROR_INVALID_STATE;
	return _sceSfmt19937FillArrayCore(pCtx, (SceUInt32 *)array, size >> 1);
}

/*
 * SIZE 128-bit words into ARRAY, the state following on from its end. The pointers walk as
 * Sony's do: when SIZE is at most twice the state, the last loop's first operand goes on from
 * where the first two loops left it, past the state, where upstream reads array[i - N].
 */
static SceInt32 _sceSfmt19937FillArrayCore(SceSfmt19937Context *pCtx, SceUInt32 *array, int size)
{
	const uint32x4_t mask = vld1q_u32(_sceSfmt19937Mask);
	SceUInt32 *state, *a, *b, *out;
	uint32x4_t r1, r2, r;
	int i, j;

	pCtx->idx = SFMT_N32;
	state = &pCtx->sfmt[0][0];
	r1 = vld1q_u32(&state[(SFMT_N - 2) * 4]);
	r2 = vld1q_u32(&state[(SFMT_N - 1) * 4]);
	a = state;
	b = &state[SFMT_POS1 * 4];
	out = array;
	for (i = 0; i < SFMT_N - SFMT_POS1; i++) {
		r = sfmtRecursion(vld1q_u32(a), vld1q_u32(b), r1, r2, mask);
		a += 4;
		b += 4;
		r1 = r2;
		r2 = r;
		vst1q_u32(out, r);
		out += 4;
	}
	b = out - (SFMT_N - SFMT_POS1) * 4;
	for (; i < SFMT_N; i++) {
		r = sfmtRecursion(vld1q_u32(a), vld1q_u32(b), r1, r2, mask);
		a += 4;
		b += 4;
		r1 = r2;
		r2 = r;
		vst1q_u32(out, r);
		out += 4;
	}
	if (i + SFMT_N < size) {
		a = out - SFMT_N * 4;
		b = a + SFMT_POS1 * 4;
		do {
			r = sfmtRecursion(vld1q_u32(a), vld1q_u32(b), r1, r2, mask);
			a += 4;
			b += 4;
			r1 = r2;
			r2 = r;
			vst1q_u32(out, r);
			out += 4;
			i++;
		} while (i + SFMT_N < size);
	}
	if (size < SFMT_N * 2) {
		const SceUInt32 *from = &array[(size - SFMT_N) * 4];

		for (j = size; j < SFMT_N * 2; j++) {
			vst1q_u32(state, vld1q_u32(from));
			from += 4;
			state += 4;
		}
	}
	for (; i < size; i++) {
		r = sfmtRecursion(vld1q_u32(a), vld1q_u32(b), r1, r2, mask);
		a += 4;
		b += 4;
		r1 = r2;
		r2 = r;
		vst1q_u32(out, r);
		out += 4;
		vst1q_u32(state, r);
		state += 4;
	}
	return 0;
}

/* makes the state's period 2^19937 - 1: flips the parity bit when the inner product is even */
static void _sceSfmt19937PeriodCheck(SceSfmt19937Context *pCtx)
{
	SceUInt32 *state = &pCtx->sfmt[0][0];
	SceUInt32 inner = 0, work;
	int i;

	for (i = 0; i < 4; i++) {
		work = state[i] & _sceSfmt19937Parity[i];
		work ^= work >> 16;
		work ^= work >> 8;
		work ^= work >> 4;
		work ^= work >> 2;
		inner ^= work ^ (work >> 1);
	}
	if ((inner & 1) != 0)
		return;
	for (i = 0; i < 4; i++) {
		if (_sceSfmt19937Parity[i] != 0) {
			state[i] ^= 0x80000000U >> __builtin_clz(_sceSfmt19937Parity[i] ^
								 (_sceSfmt19937Parity[i] - 1));
			return;
		}
	}
}

SceInt32 sceSfmt19937InitGenRand(SceSfmt19937Context *pCtx, SceUInt32 seed)
{
	SceUInt32 *state;
	int i;

	if (pCtx == NULL)
		return SCE_SFMT19937_ERROR_INVALID_POINTER;
	pCtx->idx = SFMT_N32;
	state = &pCtx->sfmt[0][0];
	state[0] = seed;
	for (i = 1; i < SFMT_N32; i++) {
		seed = 1812433253U * (seed ^ (seed >> 30)) + i;
		state[i] = seed;
	}
	_sceSfmt19937PeriodCheck(pCtx);
	return 0;
}

SceInt32 sceSfmt19937InitByArray(SceSfmt19937Context *pCtx, const SceUInt32 initkey[],
				 SceUInt32 keylength)
{
	SceUInt32 *state, *p0, *p1, *p2, *p3, *end, r;
	int count, done, i, k;

	if (pCtx == NULL || initkey == NULL)
		return SCE_SFMT19937_ERROR_INVALID_POINTER;
	pCtx->idx = SFMT_N32;
	state = &pCtx->sfmt[0][0];
	end = state + SFMT_N32;
	for (p0 = state; p0 != end; p0++)
		*p0 = 0x8b8b8b8b;
	count = (int)keylength + 1 > SFMT_N32 ? (int)keylength + 1 : SFMT_N32;
	/* lag 11, mid 306 (the upstream's 623-word variant) */
	p0 = state;
	p1 = state + 306;
	p2 = state + 623;
	p3 = state + 317;
	i = 0;
	done = -1;
	do {
		r = *p0 ^ *p1 ^ *p2;
		r = 1664525U * (r ^ (r >> 27));
		*p1 += r;
		if (initkey != NULL) {
			if (i == 0) {
				r += keylength;
			} else {
				r += *initkey++;
				if (done + 1 == (int)keylength)
					initkey = NULL;
			}
		}
		r += i;
		*p0 = r;
		*p3 += r;
		i++;
		done++;
		p0++;
		p1++;
		p2++;
		p3++;
		if (i >= SFMT_N32)
			i = 0;
		if (p0 == end)
			p0 = state;
		if (p1 == end)
			p1 = state;
		if (p2 == end)
			p2 = state;
		if (p3 == end)
			p3 = state;
	} while (done != count - 1);
	for (k = SFMT_N32; k != 0; k--) {
		r = *p0 + *p1 + *p2;
		r = 1566083941U * (r ^ (r >> 27));
		*p1 ^= r;
		r -= i;
		*p0 = r;
		*p3 ^= r;
		i++;
		p0++;
		p1++;
		p2++;
		p3++;
		if (i >= SFMT_N32)
			i = 0;
		if (p0 == end)
			p0 = state;
		if (p1 == end)
			p1 = state;
		if (p2 == end)
			p2 = state;
		if (p3 == end)
			p3 = state;
	}
	_sceSfmt19937PeriodCheck(pCtx);
	return 0;
}
