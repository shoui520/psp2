/*
 * paf::graph's FFTs for the FFT wave surface: Paul Bourke's 2D FFT over a grid of complex
 * values (math::v2, real and imaginary), and an inverse transform of 32 by 32 unrolled by the
 * stage over tables of twiddle factors (the file's name and the helpers' ours; FFT2D and
 * FFT2D32x32 Sony's). Sony's file does not include arl_defines.h: its only static
 * initializer fills the twiddle tables.
 */
#include <stddef.h>
#include <stdint.h>
#include <vectormath.h>
#include <math/ngp/v2.h>
#include <std/vector>
#include <std/string.h>
#include <graphics/effects/fft.h>

namespace paf {
namespace graph {

using sce::Vectormath::Simd::Aos::Vector2;

/* the square root from the halved exponent and one Newton step (name ours) */
static inline float FastSqrt(float x)
{
	union {
		float f;
		int32_t i;
	} guess;

	guess.f = x;
	guess.i = (guess.i >> 1) + 0x1fc00000;
	return (guess.f + x / guess.f) * 0.5f;
}

/* a times b, as complex numbers (name ours) */
static inline math::v2 ComplexMul(math::v2 a, math::v2 b)
{
	return math::v2(dot(Vector2(a.extract_x(), -a.extract_y()), b.getImpl()),
			dot(Vector2(a.extract_y(), a.extract_x()), b.getImpl()));
}

/* a twiddle factor (cos, sin) as the matrix that rotates by it: its columns (c, s) and (-s, c) (name ours) */
class Twiddle {
public:
	__attribute__((always_inline)) Twiddle(float c, float s)
		: m_cos_sin(c, s), m_neg_sin_cos(-s, c)
	{
	}

	math::v2 Rotate(math::v2 x) const
	{
		return m_cos_sin * x.extract_x() + m_neg_sin_cos * x.extract_y();
	}

	math::v2 m_cos_sin;
	math::v2 m_neg_sin_cos;
};

/* e^(i pi k/4), e^(i pi k/8) and e^(i pi k/16): the inverse transform's twiddles for the spans 4, 8 and 16 (names ours) */
static Twiddle s_twiddle4[4] = {
	Twiddle(1.0f, 0.0f),
	Twiddle(0.707107f, 0.707107f),
	Twiddle(-0.0f, 1.0f),
	Twiddle(-0.707107f, 0.707107f),
};
static Twiddle s_twiddle8[8] = {
	Twiddle(1.0f, 0.0f),
	Twiddle(0.92388f, 0.382683f),
	Twiddle(0.707107f, 0.707107f),
	Twiddle(0.382683f, 0.92388f),
	Twiddle(-0.0f, 1.0f),
	Twiddle(-0.382684f, 0.92388f),
	Twiddle(-0.707107f, 0.707107f),
	Twiddle(-0.92388f, 0.382683f),
};
static Twiddle s_twiddle16[16] = {
	Twiddle(1.0f, 0.0f),
	Twiddle(0.980785f, 0.19509f),
	Twiddle(0.92388f, 0.382683f),
	Twiddle(0.83147f, 0.55557f),
	Twiddle(0.707107f, 0.707107f),
	Twiddle(0.55557f, 0.83147f),
	Twiddle(0.382683f, 0.92388f),
	Twiddle(0.19509f, 0.980785f),
	Twiddle(-0.0f, 1.0f),
	Twiddle(-0.19509f, 0.980785f),
	Twiddle(-0.382684f, 0.92388f),
	Twiddle(-0.55557f, 0.83147f),
	Twiddle(-0.707107f, 0.707107f),
	Twiddle(-0.83147f, 0.55557f),
	Twiddle(-0.92388f, 0.382683f),
	Twiddle(-0.980785f, 0.19509f),
};

/* the five butterfly stages of an inverse FFT of 32 values stride apart, already in bit-reversed order (name ours) */
template <int32_t stride>
static inline void FFT32(math::v2 *x)
{
	int32_t i, j;

	for (i = 0; i < 32; i += 2) {
		math::v2 t = x[(i + 1) * stride];

		x[(i + 1) * stride] = x[i * stride] - t;
		x[i * stride] = x[i * stride] + t;
	}
	for (i = 0; i < 32; i += 4) {
		math::v2 t = x[(i + 2) * stride];

		x[(i + 2) * stride] = x[i * stride] - t;
		x[i * stride] = x[i * stride] + t;
	}
	for (i = 1; i < 32; i += 4) {
		math::v2 u = x[(i + 2) * stride];
		math::v2 t(-u.extract_y(), u.extract_x());

		x[(i + 2) * stride] = x[i * stride] - t;
		x[i * stride] = x[i * stride] + t;
	}
	for (j = 0; j < 4; j++) {
		for (i = j; i < 32; i += 8) {
			math::v2 t = s_twiddle4[j].Rotate(x[(i + 4) * stride]);

			x[(i + 4) * stride] = x[i * stride] - t;
			x[i * stride] = x[i * stride] + t;
		}
	}
	for (j = 0; j < 8; j++) {
		for (i = j; i < 32; i += 16) {
			math::v2 t = s_twiddle8[j].Rotate(x[(i + 8) * stride]);

			x[(i + 8) * stride] = x[i * stride] - t;
			x[i * stride] = x[i * stride] + t;
		}
	}
	for (j = 0; j < 16; j++) {
		math::v2 t = s_twiddle16[j].Rotate(x[(j + 16) * stride]);

		x[(j + 16) * stride] = x[j * stride] - t;
		x[j * stride] = x[j * stride] + t;
	}
}

void FFT2D(int32_t width, int32_t height, bool inverse, const vector<math::v2> &src,
	   vector<math::v2> &dst)
{
	math::v2 *out = &dst[0];
	int32_t log_width = 0;
	int32_t log_height = 0;
	int32_t n;

	for (n = 1; n < width; n *= 2)
		log_width++;
	for (n = 1; n < height; n *= 2)
		log_height++;

	int32_t w = 1 << log_width;
	int32_t h = 1 << log_height;
	int32_t row, col, i, j, k;

	for (row = 0; row < h; row++) {
		for (col = 0; col < h; col++)
			out[row * h + col] = src[row * h + col];
	}

	/* the columns' rows in bit-reversed order */
	for (col = 0; col < h; col++) {
		j = 0;
		for (i = 0; i < w - 1; i++) {
			out[col + i * h] = src[col + j * h];
			k = w / 2;
			while (k <= j) {
				j -= k;
				k /= 2;
			}
			j += k;
		}
	}

	/* each row's elements in bit-reversed order */
	for (row = 0; row < w; row++) {
		j = 0;
		for (i = 0; i < h - 1; i++) {
			if (i < j) {
				math::v2 swap = out[row * h + i];

				out[row * h + i] = out[row * h + j];
				out[row * h + j] = swap;
			}
			k = h / 2;
			while (k <= j) {
				j -= k;
				k /= 2;
			}
			j += k;
		}
	}

	/* along the rows */
	for (row = 0; row < w; row++) {
		math::v2 *x = &out[row * h];
		float c1 = -1.0f;
		float c2 = 0.0f;
		int32_t l2 = 1;

		for (int32_t l = 0; l < log_width; l++) {
			int32_t l1 = l2;
			math::v2 u(1.0f, 0.0f);

			l2 *= 2;
			for (j = 0; j < l1; j++) {
				for (i = j; i < w; i += l2) {
					math::v2 t = ComplexMul(u, x[i + l1]);

					x[i + l1] = x[i] - t;
					x[i] = x[i] + t;
				}
				u = ComplexMul(math::v2(c1, c2), u);
			}
			c2 = FastSqrt((1.0f - c1) * 0.5f);
			if (!inverse)
				c2 = -c2;
			c1 = FastSqrt((1.0f + c1) * 0.5f);
		}
	}

	/* along the columns */
	for (col = 0; col < h; col++) {
		math::v2 *x = &out[col];
		float c1 = -1.0f;
		float c2 = 0.0f;
		int32_t l2 = 1;

		for (int32_t l = 0; l < log_height; l++) {
			int32_t l1 = l2;
			math::v2 u(1.0f, 0.0f);

			l2 *= 2;
			for (j = 0; j < l1; j++) {
				for (i = j; i < w; i += l2) {
					math::v2 t = ComplexMul(u, x[(i + l1) * h]);

					x[(i + l1) * h] = x[i * h] - t;
					x[i * h] = x[i * h] + t;
				}
				u = ComplexMul(math::v2(c1, c2), u);
			}
			c2 = FastSqrt((1.0f - c1) * 0.5f);
			if (!inverse)
				c2 = -c2;
			c1 = FastSqrt((1.0f + c1) * 0.5f);
		}
	}

	float scale = (float)(inverse ? w : h);

	for (row = 0; row < w; row++) {
		for (col = 0; col < h; col++)
			out[row * h + col] = out[row * h + col] / scale;
	}
}

void FFT2D32x32(const vector<math::v2> &src, vector<math::v2> &dst)
{
	int32_t row, col;

	/* the rows in bit-reversed order */
	for (row = 0; row < 32; row++)
		sce_paf_memcpy(&dst[row * 32], &src[(__builtin_rbit(row) >> 27) * 32],
			       32 * sizeof(math::v2));

	math::v2 *out = &dst[0];

	/* each row's elements in bit-reversed order */
	for (col = 0; col < 31; col++) {
		int32_t rev = __builtin_rbit(col) >> 27;

		if (col < rev) {
			for (row = 0; row < 32; row++) {
				math::v2 swap = out[row * 32 + col];

				out[row * 32 + col] = out[row * 32 + rev];
				out[row * 32 + rev] = swap;
			}
		}
	}

	for (row = 0; row < 32; row++)
		FFT32<1>(&out[row * 32]);
	for (col = 0; col < 32; col++)
		FFT32<32>(&out[col]);

	for (row = 0; row < 32; row++) {
		for (col = 0; col < 32; col++)
			out[row * 32 + col] = out[row * 32 + col] * (1.0f / 32.0f);
	}
}

}
}
