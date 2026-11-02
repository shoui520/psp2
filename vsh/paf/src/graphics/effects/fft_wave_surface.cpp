/*
 * The heights of waves computed by FFT (the file's name ours): each wave's amplitude from the
 * Phillips spectrum and a random value, turned by its phase on each update, the inverse FFT of
 * that, and the heights put into a texture of half floats.
 */
#include <stdint.h>
#include <arm_neon.h>
#include <vectormath.h>
#include <graphics/effects/fft_wave_surface.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <std/algorithm>
#include <src/std/math.h>

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::Aos::Vector2;

namespace paf {
namespace graph {

/* x lane from a, y lane from b */
static inline float32x2_t SelectX(float32x2_t a, float32x2_t b)
{
	return vbsl_f32(vreinterpret_u32_u64(vcreate_u64(0xffffffffULL)), a, b);
}

/* -1..1 */
static inline float RandomValue()
{
	return (float)((uint32_t)sce_paf_rand() % 2056) * 2.0f / 2056.0f - 1.0f;
}

void FFTWaveSurface::GenerateH0()
{
	float wind_dir = m_param.m_wind_dir;
	Vector2 scale = Vector2(3.14159265f, 2.0f * 3.14159265f) / m_param.m_patch_size;
	float wind_x = sce_paf_cosf(wind_dir);
	float wind_y = sce_paf_sinf(wind_dir);
	Vector2 wind(wind_x, wind_y);
	float length = m_param.m_wind_speed * m_param.m_wind_speed / m_param.m_gravity;
	float length2 = length * length;
	int32_t i = 0;

	for (int32_t y = 0; y < m_height; y++) {
		for (int32_t x = 0; x < m_width; x++, i++) {
			Vector2 k = mulPerElem(scale, Vector2((float)x, (float)y));
			floatInVec k2 = dot(k, k);
			floatInVec phillips(0.0f);

			if (!(k2 == floatInVec(0.0f)).getAsBool()) {
				floatInVec e = sce::Vectormath::Simd::expf(
					floatInVec(m_param.m_log_a) - recipf(k2 * floatInVec(length2)));
				Vector2 dir = k * recipf(sce::Vectormath::Simd::sqrtf(k2));
				floatInVec w = dot(dir, wind);

				phillips = e * recipf(k2 * k2) * w * w;
			}
			m_h0[i] = math::v2(m_random.m_data[i].getImpl() *
					   sce::Vectormath::Simd::sqrtf(phillips) *
					   floatInVec(0.70710678f));
		}
	}
}

void FFTWaveSurface::UpdateSpectrum(const vector<math::v2> &h0, vector<math::v2> &spectrum)
{
	int32_t i = 0;

	for (int32_t y = 0; y < m_height; y++) {
		int32_t mirror = (m_height - y - 1) * m_width;

		for (int32_t x = 0; x < m_width; x++, i++, mirror++) {
			FFTWavePhase &phase = m_phases[i];
			float32x2_t rotation = phase.m_rotation.getImpl().get64();
			float32x2_t step = phase.m_step.getImpl().get64();
			float32x2_t other = h0[mirror].getImpl().get64();
			float32x2_t amplitude = vsub_f32(h0[i].getImpl().get64(),
							 SelectX(other, vneg_f32(other)));
			float32x2_t re = vmul_f32(SelectX(rotation, vneg_f32(rotation)), amplitude);
			float32x2_t im = vmul_f32(vrev64_f32(rotation), amplitude);

			phase.m_rotation = math::v2(Vector2(vadd_f32(
				vmul_n_f32(step, vget_lane_f32(rotation, 0)),
				vmul_f32(SelectX(vneg_f32(vdup_lane_f32(rotation, 1)), rotation),
					 vrev64_f32(step)))));
			float32x2x2_t sum = vtrn_f32(vpadd_f32(re, re), vpadd_f32(im, im));

			spectrum[i] = math::v2(Vector2(sum.val[0]));
		}
	}
}

void FFTWaveSurface::InitPhases()
{
	m_phases.resize(m_width * m_height,
			FFTWavePhase(math::v2(1.0f, 0.0f), math::v2(1.0f, 0.0f)));

	Vector2 scale = Vector2(3.14159265f, 2.0f * 3.14159265f) / m_param.m_patch_size;
	Vector2 row(0.0f);

	for (int32_t y = 0; y < m_height; y++) {
		Vector2 pos = row;

		for (int32_t x = 0; x < m_width; x++) {
			float k = length(mulPerElem(scale, pos)).getAsFloat();
			float angle = __builtin_sqrtf(m_param.m_gravity * k) * m_param.m_time_step * 0.15f;
			float c = sce_paf_cosf(angle);
			float s = sce_paf_sinf(angle);
			FFTWavePhase &phase = m_phases[x + m_width * y];

			phase.m_rotation = math::v2(1.0f, 0.0f);
			phase.m_step = math::v2(c, s);
			pos = pos + Vector2(0.0f, 1.0f);
		}
		row = row + Vector2(1.0f, 0.0f);
	}
}

FFTWaveParam::FFTWaveParam()
	: m_gravity(9.81f), m_a(1.55e-8f), m_log_a(sce_paf_logf(1.55e-8f)), m_wind_speed(15.0f),
	  m_wind_dir(3.14159265f / 2.0f), m_patch_size(100.0f), m_time_step(0.025f),
	  m_height_scale(625.0f)
{
}

FFTWaveSurface::FFTWaveSurface(const char *name) : m_name(name)
{
}

void FFTWaveSurface::Reset()
{
	GenerateH0();
	copy(m_h0.begin(), m_h0.end(), m_amplitudes.begin());
}

void FFTWaveSurface::Init(arl::SurfaceSize size)
{
	m_width = size.m_width;
	m_height = size.m_height;
	m_random_values.resize(m_width * m_height);
	m_random = Array2D<math::v2>(m_random_values.ptr, m_width, m_height);
	for (uint32_t i = 0; i < m_random.m_count; i++) {
		math::v2 &value = m_random.m_data[i];
		float x = RandomValue();
		float y = RandomValue();

		value = math::v2(x, y);
	}
	m_time = 0.0f;
	m_h0.resize(m_width * m_height);
	m_amplitudes.resize(m_width * m_height);
	m_spectrum.resize(m_width * m_height);
	m_heights.resize(m_width * m_height);
	for (uint32_t i = 0; i != m_h0.size(); i++) {
		m_heights[i] = math::v2(0.0f, 0.0f);
		m_spectrum[i] = math::v2(0.0f, 0.0f);
		m_amplitudes[i] = math::v2(0.0f, 0.0f);
		m_h0[i] = math::v2(0.0f, 0.0f);
	}
	Reset();

	m_texture = common::SharedPtr<arl::Texture2D>(
		new arl::Texture2D(arl::Gl()->m_default_allocator));
	m_desc = arl::TextureDesc(m_width, m_height, ImageMode_A16FP, 0);
	ScePafGraphics_C4BD3558(m_texture.get(), m_desc.m_width, m_desc.m_height, m_desc.m_format,
				m_desc.m_levels, arl::SurfaceFlags());
	m_texels.resize(m_width * m_height);
	InitPhases();
}

void FFTWaveSurface::Update(bool paused, bool unused)
{
	if (!paused)
		m_time += m_param.m_time_step;
	UpdateSpectrum(m_amplitudes, m_spectrum);
	if (m_width == 32 && m_height == 32)
		FFT2D32x32(m_spectrum, m_heights);
	else
		FFT2D(m_width, m_height, true, m_spectrum, m_heights);

	uint32_t count = m_width * m_height;

	if (count == 32 * 32) {
		float32x4_t one = vdupq_n_f32(1.0f);
		float32x4_t half = vdupq_n_f32(0.5f);
		float32x4_t scale = vdupq_n_f32(m_param.m_height_scale);

		for (uint32_t i = 0; i < 32 * 32 / 4; i++) {
			const math::v2 *heights = &m_heights[i * 4];
			float32x2x2_t low = vtrn_f32(heights[0].getImpl().get64(),
						     heights[1].getImpl().get64());
			float32x2x2_t high = vtrn_f32(heights[2].getImpl().get64(),
						      heights[3].getImpl().get64());
			float32x4_t value = vcombine_f32(low.val[0], high.val[0]);

			vst1_u16(&m_texels[i * 4].m_bits, vreinterpret_u16_f16(vcvt_f16_f32(
				vmulq_f32(vmlaq_f32(one, value, scale), half))));
		}
	} else {
		for (uint32_t i = 0; i < count; i++)
			m_texels[i] = Half((m_heights[i].extract_x() * m_param.m_height_scale + 1.0f) *
					   0.5f);
	}
	arl::TextureDesc desc = m_desc;

	ScePafGraphics_309D2902(m_texture.get(), 0, desc.m_width, desc.m_height, desc.m_format,
				desc.m_levels, m_texels.ptr);
}

}
}
