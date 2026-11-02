/*
 * The heights of waves computed by FFT, from Tessendorf's spectrum (as NVIDIA's oceanFFT sample
 * computes it), into a texture of half floats, and the parameters they are computed from (the
 * parameters' set is named "FFTWave"; its members gravity, A, logA, patch_size, wind_speed,
 * wind_dir and time_step). Every class and member name here is ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_FFT_WAVE_SURFACE_H
#define PAF_GRAPHICS_EFFECTS_FFT_WAVE_SURFACE_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <math/ngp/v2.h>
#include <std/string>
#include <std/vector>

namespace paf {
namespace graph {

class FFTWaveParam {
public:
	/* 0x8109fec0 */
	FFTWaveParam();

	float m_gravity;
	float m_a;
	/* logf(m_a) */
	float m_log_a;
	float m_wind_speed;
	float m_wind_dir;
	float m_patch_size;
	/* how far the waves move on each update */
	float m_time_step;
	/* the heights' scale before they are put in 0..1 */
	float m_height_scale;
};

/* a view of an array as rows of a width */
template<class T>
class Array2D {
public:
	Array2D() : m_data(NULL), m_width(0), m_height(0), m_count(0), m_last_x(-1), m_last_y(-1)
	{
	}

	Array2D(T *data, int32_t width, int32_t height)
		: m_data(data), m_width(width), m_height(height), m_count(width * height),
		  m_last_x(width - 1), m_last_y(height - 1)
	{
	}

	T *m_data;
	int32_t m_width;
	int32_t m_height;
	uint32_t m_count;
	int32_t m_last_x;
	int32_t m_last_y;
};

/* a wave's phase: where it is, and the turn it makes on each update */
class FFTWavePhase {
public:
	FFTWavePhase(math::v2 rotation, math::v2 step) : m_rotation(rotation), m_step(step)
	{
	}

	math::v2 m_rotation;
	math::v2 m_step;
};

/* a 16-bit float; made without a value */
class Half {
public:
	Half()
	{
	}

	/* rounded towards zero; what is too small is 0, too large keeps the largest exponent */
	Half(float value)
	{
		const uint8_t *bytes = (const uint8_t *)&value;
		uint32_t bits = bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24;

		if (bits == 0) {
			m_bits = 0;
		} else {
			int32_t exponent = (int32_t)((bits >> 23) & 0xff) - 112;

			if (exponent < 0) {
				m_bits = 0;
			} else {
				if (exponent > 31)
					exponent = 31;
				m_bits = (exponent & 0x1f) << 10 | (bits >> 31) << 15 | (bits >> 13 & 0x3ff);
			}
		}
	}

	uint16_t m_bits;
};

/* 0x811944c0: the 2D FFT of src into dst, inverse when asked */
void FFT2D(int32_t width, int32_t height, bool inverse, const vector<math::v2> &src,
	   vector<math::v2> &dst);
/* 0x811949ea: the inverse 2D FFT of 32 by 32 */
void FFT2D32x32(const vector<math::v2> &src, vector<math::v2> &dst);

/* 0xd8 bytes */
class FFTWaveSurface {
public:
	/* 0x8109ff1e */
	FFTWaveSurface(const char *name);

	/* 0x8109f94c: the amplitudes from the spectrum and the random values */
	void GenerateH0();
	/* 0x8109fc6e: the amplitudes turned by their phases into the spectrum, the phases moved on */
	void UpdateSpectrum(const vector<math::v2> &h0, vector<math::v2> &spectrum);
	/* 0x8109fd46: every phase at 0, turned on each update by its frequency */
	void InitPhases();
	/* 0x810a000c: the amplitudes made again */
	void Reset();
	/* 0x810a0042: the size, the random values, the texture */
	void Init(arl::SurfaceSize size);
	/* 0x810a03ae: the waves moved on unless paused, and computed again; the second flag is
	   not read */
	void Update(bool paused, bool unused);

	arl::TextureDesc m_desc;
	common::SharedPtr<arl::Texture2D> m_texture;
	int32_t m_width;
	int32_t m_height;
	/* two in -1..1 for each wave, and the view of them */
	vector<math::v2> m_random_values;
	Array2D<math::v2> m_random;
	FFTWaveParam m_param;
	float m_time;
	string m_name;
	vector<math::v2> m_h0;
	/* the amplitudes the spectrum is made from */
	vector<math::v2> m_amplitudes;
	vector<math::v2> m_spectrum;
	vector<math::v2> m_heights;
	vector<FFTWavePhase> m_phases;
	/* the texture's data */
	vector<Half> m_texels;
};

}
}

#endif
