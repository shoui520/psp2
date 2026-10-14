/*
 * The complex analysis bank's modulation (0x86cc2a, 0x86cc64; kernels
 * 0x8e3988 and 0x8e3c98 for 32 points): the input turned pairwise into
 * length / 2 complex points, a complex FFT, and the result turned back,
 * the imaginary part's kernel placing and negating the same products.
 */
#include "aac.h"
#include "aacsbr.h"

/* the pre- and post-FFT twiddles for 32 points, Q28 and Q27 (0x924478, 0x9244f8) */
static const int32_t preTwiddle32[32] = {
	3293868, 16429683, 134177304, 133208348, 133854059, 132241727, 9873669, 22946116,
	29407271, 42101651, 130956523, 127443515, 129355834, 125224174, 35797580, 48304294,
	54390568, 66155675, 122703156, 116781099, 119886536, 113394326, 60345811, 71806165,
	77283667, 87667375, 109734376, 101630851, 105810066, 97206798, 82574986, 92548565,
};

static const int32_t postTwiddle32[32] = {
	134217727, 133571433, 0, 13155638, 13155638, 26184580, 133571433, 131638772,
	131638772, 128438358, 26184580, 38961350, 38961350, 51362901, 128438358, 124001012,
	124001012, 118369468, 51362901, 63269799, 63269799, 74567374, 118369468, 111597962,
	111597962, 103751707, 74567374, 85146825, 85146825, 94906266, 103751707, 94906266,
};

/* the 64-point kernels' twiddles: cos and sin of (n + 1/4) pi / 64, Q27, and sin(n pi / 64) (0x924578, 0x9245f8, 0x924678) */
static const int32_t preCos64[32] = {
	134207622, 133965145, 133399935, 132513353, 131307535, 129785385, 127950571, 125807513,
	123361373, 120618045, 117584138, 114266960, 110674503, 106815421, 102699012, 98335192,
	93734473, 88907941, 83867221, 78624457, 73192281, 67583778, 61812459, 55892229,
	49837350, 43662408, 37382280, 31012094, 24567198, 18063117, 11515520, 4940182,
};

static const int32_t preSin64[32] = {
	1647058, 8230330, 14793774, 21321579, 27798018, 34207490, 40534552, 46763964,
	52880716, 58870075, 64717610, 70409235, 75931238, 81270316, 86413607, 91348720,
	96063765, 100547385, 104788777, 108777724, 112504616, 115960474, 119136973, 122026461,
	124621977, 126917267, 128906802, 130585790, 131950185, 132996700, 133722815, 134126780,
};

static const int32_t postSin64[33] = {
	0, 6585752, 13155638, 19693831, 26184580, 32612248, 38961350, 45216591,
	51362901, 57385473, 63269799, 69001702, 74567374, 79953407, 85146825, 90135117,
	94906266, 99448777, 103751707, 107804690, 111597962, 115122385, 118369468, 121331389,
	124001012, 126371905, 128438358, 130195391, 131638772, 132765024, 133571433, 134056057,
	134217727,
};

static int32_t sum28(int32_t a, int32_t p, int32_t b, int32_t q)
{
	return (int32_t)(((int64_t)a * p + (int64_t)b * q) >> 28);
}

static int32_t diff28(int32_t a, int32_t p, int32_t b, int32_t q)
{
	return (int32_t)(((int64_t)a * p - (int64_t)b * q) >> 28);
}

/* the post-FFT products, each (+ or -) shifted by 27 */
static int32_t sum27(int32_t a, int32_t p, int32_t b, int32_t q, int32_t negate)
{
	int64_t value = (int64_t)a * p + (int64_t)b * q;

	return (int32_t)((negate ? -value : value) >> 27);
}

static int32_t diff27(int32_t a, int32_t p, int32_t b, int32_t q, int32_t negate)
{
	int64_t value = (int64_t)a * p - (int64_t)b * q;

	return (int32_t)((negate ? -value : value) >> 27);
}

/* 0x8e3988 (imaginary = 0) and 0x8e3c98 (imaginary = 1) */
static void modulate(const int32_t *in, int32_t length, int32_t *out, const int32_t *pre,
		     const int32_t *post, int32_t imaginary)
{
	int32_t i;

	for (i = 0; i < length >> 3; i++) {
		const int32_t *x = &in[8 * i], *p = &pre[8 * i];
		int32_t *head = &out[4 * i], *tail = &out[length - 1 - 4 * i];

		if (!imaginary) {
			head[0] = sum28(x[0], p[0], x[2], p[2]);
			head[1] = diff28(x[0], p[2], x[2], p[0]);
			head[2] = sum28(x[1], p[1], x[3], p[3]);
			head[3] = diff28(x[1], p[3], x[3], p[1]);
			tail[0] = diff28(x[4], p[6], x[6], p[4]);
			tail[-1] = sum28(x[4], p[4], x[6], p[6]);
			tail[-2] = diff28(x[5], p[7], x[7], p[5]);
			tail[-3] = sum28(x[5], p[5], x[7], p[7]);
		} else {
			head[0] = diff28(x[0], p[0], x[2], p[2]);
			head[1] = sum28(x[0], p[2], x[2], p[0]);
			head[2] = diff28(x[1], p[1], x[3], p[3]);
			head[3] = sum28(x[1], p[3], x[3], p[1]);
			tail[0] = sum28(x[4], p[6], x[6], p[4]);
			tail[-1] = diff28(x[4], p[4], x[6], p[6]);
			tail[-2] = sum28(x[5], p[7], x[7], p[5]);
			tail[-3] = diff28(x[5], p[5], x[7], p[7]);
		}
	}

	aacFft(out, length >> 1);

	for (i = 0; i < length >> 3; i++) {
		int32_t *head = &out[4 * i], *tail = &out[length - 1 - 4 * i];
		const int32_t *q = &post[8 * i];
		int32_t o0 = head[0], o1 = head[1], o2 = head[2], o3 = head[3];
		int32_t e1 = tail[0], e2 = tail[-1], e3 = tail[-2], e4 = tail[-3];

		if (!imaginary) {
			head[0] = sum27(o0, q[0], o1, q[2], 0);
			head[2] = sum27(o2, q[1], o3, q[3], 0);
			tail[0] = diff27(o0, q[2], o1, q[0], 0);
			tail[-2] = diff27(o2, q[3], o3, q[1], 0);
			tail[-1] = sum27(e2, q[4], e1, q[6], 0);
			tail[-3] = sum27(e4, q[5], e3, q[7], 0);
			head[1] = diff27(e2, q[6], e1, q[4], 0);
			head[3] = diff27(e4, q[7], e3, q[5], 0);
		} else {
			head[0] = diff27(o0, q[2], o1, q[0], 1);
			head[2] = diff27(o2, q[3], o3, q[1], 1);
			tail[0] = sum27(o0, q[0], o1, q[2], 1);
			tail[-2] = sum27(o2, q[1], o3, q[3], 1);
			tail[-1] = diff27(e2, q[6], e1, q[4], 1);
			tail[-3] = diff27(e4, q[7], e3, q[5], 1);
			head[1] = sum27(e2, q[4], e1, q[6], 1);
			head[3] = sum27(e4, q[5], e3, q[7], 1);
		}
	}
}

/*
 * The 64-point kernels (0x8e3560 real, 0x8e3fe0 imaginary), in place: each
 * point paired with its mirror under the pre-twiddle, the FFT, and each
 * complex result split back to both ends under the post-twiddle.
 */
void aacSbrModulate64(int32_t *data, int32_t imaginary)
{
	int32_t in[64];
	int32_t n;

	aacSbrCopy((int8_t *)in, (const int8_t *)data, sizeof(in));
	for (n = 0; n < 32; n++) {
		int32_t first = in[2 * n], second = in[63 - 2 * n];
		int32_t c = preCos64[n], s = preSin64[n];

		if (!imaginary) {
			data[2 * n] = sum28(first, c, second, s);
			data[2 * n + 1] = diff28(second, c, first, s);
		} else {
			data[2 * n] = diff28(second, s, first, c);
			data[2 * n + 1] = sum28(first, s, second, c);
		}
	}

	aacFft(data, 32);

	aacSbrCopy((int8_t *)in, (const int8_t *)data, sizeof(in));
	for (n = 0; n < 32; n++) {
		int32_t re = in[2 * n], im = in[2 * n + 1];
		int32_t c = postSin64[n], s = postSin64[32 - n];

		if (!imaginary) {
			data[2 * n] = sum27(re, s, im, c, 0);
			data[63 - 2 * n] = diff27(re, c, im, s, 0);
		} else {
			data[2 * n] = diff27(im, s, re, c, 0);
			data[63 - 2 * n] = sum27(re, s, im, c, 1);
		}
	}
}

void aacSbrModulateReal(int32_t *data, int32_t length, int32_t *out)
{
	if (length == 64)
		aacSbrModulate64(data, 0);
	else if (length == 32)
		modulate(data, 32, out, preTwiddle32, postTwiddle32, 0);
}

void aacSbrModulateImag(int32_t *data, int32_t length, int32_t *out)
{
	if (length == 64)
		aacSbrModulate64(data, 1);
	else if (length == 32)
		modulate(data, 32, out, preTwiddle32, postTwiddle32, 1);
}
