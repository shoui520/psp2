/*
 * The blur behind the panels: the screen captured at half its size, copied down to a quarter,
 * an eighth and a sixteenth of it, each copy box blurred and drawn back into the capture's
 * mip levels, and the capture drawn over the screen with the levels faded by the amount.
 * Blur is Sony's name (its marker "Blur::draw_uniform_blur"); the file's name is ours.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <common/shared_ptr.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/color.h>
#include <graphics/effects/post_effects.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>

using namespace sce::Vectormath::Simd::Aos;

namespace paf {

/* Sony's build calls the temporaries' push_back rather than inlining it. */
template<>
inline __attribute__((noinline)) int
vector<common::SharedPtr<graph::arl::Texture2D> >::push_back(
	const common::SharedPtr<graph::arl::Texture2D> &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) common::SharedPtr<graph::arl::Texture2D>(val);
		result = ++count;
	}
	return result;
}

namespace graph {

/* the index of the highest bit set */
static inline uint32_t Log2(uint32_t value)
{
	uint32_t shift;
	uint32_t result;

	result = (value > 0xFFFF) << 4;
	value >>= result;
	shift = (value > 0xFF) << 3;
	value >>= shift;
	result |= shift;
	shift = (value > 0xF) << 2;
	value >>= shift;
	result |= shift;
	shift = (value > 0x3) << 1;
	value >>= shift;
	result |= shift;
	return result | (value >> 1);
}

/* the powers of two at or below a value and at or above it (names ours) */
static inline uint32_t LowerPow2(uint32_t value)
{
	return 1 << Log2(value);
}

static inline uint32_t UpperPow2(uint32_t value)
{
	uint32_t pow2 = 1 << Log2(value);

	if (value > pow2)
		pow2 *= 2;
	return pow2;
}

/* a digit, '?' for anything else (name ours) */
static inline char DigitChar(int32_t value)
{
	if (value < 0 || value > 9)
		return '?';

	const char digits[10] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

	return digits[value];
}

/* the texture clamped to its border both ways (name ours) */
static inline void ClampToBorder(arl::TextureObject *texture)
{
	texture->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	texture->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
}

common::SharedPtr<arl::Texture2D> Blur::GetTemporary(const common::SharedPtr<arl::Texture2D> &like)
{
	for (uint32_t i = 0; i < m_temporaries.size(); i++) {
		if (m_temporaries[i]->m_desc.m_width == like->m_desc.m_width &&
		    m_temporaries[i]->m_desc.m_height == like->m_desc.m_height &&
		    m_temporaries[i]->m_desc.m_format == like->m_desc.m_format)
			return m_temporaries[i];
	}

	arl::ArlInstance *gl = arl::Gl();
	common::SharedPtr<arl::Texture2D> texture(new arl::Texture2D(gl->m_default_allocator));

	ScePafGraphics_C4BD3558(texture.get(), like->m_desc.m_width, like->m_desc.m_height,
				like->m_desc.m_format, like->m_desc.m_levels, like->m_storage->m_flags);
	ClampToBorder(texture.get());
	m_temporaries.push_back(texture);
	return texture;
}

/* 0 to 1 eased in and out (name ours) */
static inline float SmoothStep(float x)
{
	return x * x * (3.0f - x * 2.0f);
}

void Blur::draw_uniform_blur(float amount)
{
	arl::ArlInstance *gl = arl::Gl();

	gl->PushMarker("Blur::draw_uniform_blur");
	gl->DepthMask(false, arl::FRONT_AND_BACK);
	gl->Disable(arl::DEPTH_TEST);

	float levels = (float)m_capture->m_desc.m_levels;
	float level = (levels + 1.0f) * amount;
	float bias = levels * amount;

	m_capture->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, true);
	m_capture->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	m_capture->setLodBias(bias);

	QuadArea uv_area = m_capture_area;

	if (level < 0.0f)
		level = 0.0f;
	level = level < 2.0f ? level : 2.0f;

	float alpha = SmoothStep(level * 0.5f);
	QuadArea area;

	area.m_min = math::v2(0.0f, 0.0f);
	area.m_max = math::v2(1.0f, 1.0f);
	m_texture_ops->m_vert.m_interface.m_mvp.Set(GetQuadMatrix(area));
	m_texture_ops->m_vert.m_interface.m_uv_transform.Set(
		math::v4(uv_area.m_min.extract_x(), uv_area.m_min.extract_y(),
			 (uv_area.m_max - uv_area.m_min).extract_x(),
			 (uv_area.m_max - uv_area.m_min).extract_y()));
	m_uniform_blur.m_interface.m_alpha.Set(alpha);
	{
		common::SharedPtr<arl::Texture2D> capture = m_capture;

		m_uniform_blur.m_interface.m_blur_levels.Set(common::SharedPtr<arl::TextureObject>(capture));
	}
	m_texture_ops->m_vert.m_object.begin();
	m_uniform_blur.m_object.begin();
	m_texture_ops->DrawQuad();
	m_uniform_blur.m_object.end();
	m_texture_ops->m_vert.m_object.end();
	gl->PopMarker();
}

Blur::Blur()
	: m_levels(m_level_textures), m_num_levels(NUM_LEVELS), m_max_levels(NUM_LEVELS),
	  m_level_offset(0), m_copy_first_level(true), m_strength(1.0f), m_bicubic_slow(false),
	  m_bicubic(true), m_box_blur_passes(3), m_quarter_scale(2.0f), m_eighth_scale(2.0f),
	  m_sixteenth_scale(2.0f), m_alpha_scale(1.0f), m_half_flag(false), m_initialized(false),
	  m_capture_area(math::v2(0.0f, 0.0f), math::v2(1.0f, 1.0f)),
	  m_source_area(math::v2(0.0f, 0.0f), math::v2(1.0f, 1.0f)), m_pow2(true)
{
}

void Blur::Capture(common::SharedPtr<arl::Texture2D> texture, arl::SurfaceSize size,
		   const math::v2 &amount)
{
	arl::ArlInstance *gl = arl::Gl();

	ClampToBorder(texture.get());
	texture->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	texture->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);

	if (!m_initialized) {
		m_initialized = true;

		int32_t width = texture->m_desc.m_width;
		int32_t height = texture->m_desc.m_height;
		ImageMode format = texture->m_desc.m_format;
		uint32_t levels = texture->m_desc.m_levels;

		m_texture_width = width;
		m_texture_height = height;
		m_width = size.m_width;
		m_height = size.m_height;

		/* the power of two nearest the size, below or above it */
		int32_t lower_width = LowerPow2(width);
		int32_t lower_height = LowerPow2(height);
		int32_t upper_width = UpperPow2(width);
		int32_t upper_height = UpperPow2(height);

		if (sce_paf_abs(upper_width - width) < sce_paf_abs(lower_width - width))
			lower_width = upper_width;
		if (sce_paf_abs(upper_height - height) < sce_paf_abs(lower_height - height))
			lower_height = upper_height;
		m_pow2_width = lower_width;
		m_pow2_height = lower_height;
		if (m_pow2) {
			width = lower_width;
			height = lower_height;
		}

		arl::SurfaceFlags flags = texture->m_storage->m_flags;

		flags.Set(arl::TEXTURE).Set(arl::COLOR_SURFACE);
		m_capture = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
		flags.Set(arl::TEXTURE_ALLOC_MIPMAPS).Set(arl::COLOR_SURFACE_SCALE_NONE);
		ScePafGraphics_C4BD3558(m_capture.get(), width >> 1, height >> 1, format, 4, flags);
		ClampToBorder(m_capture.get());
		for (uint32_t i = 0; i != NUM_LEVELS; i++) {
			m_levels[i] = common::SharedPtr<arl::Texture2D>(
				new arl::Texture2D(gl->m_default_allocator));
			ScePafGraphics_C4BD3558(m_levels[i].get(), width >> (i + 2), height >> (i + 2),
						format, levels, flags);
			ClampToBorder(m_levels[i].get());
		}
		if (m_texture_ops.get() == NULL)
			m_texture_ops = common::SharedPtr<TextureOps>(new TextureOps());

		arl::ProgramState *state = gl;

		m_uniform_blur.m_object.init(*gl, state->m_patcher);
		state = gl;
		m_variable_blur.m_object.init(*gl, state->m_patcher);
	}

	gl->Disable(arl::BLEND);

	math::v2 screen((float)m_width, (float)m_height);
	math::v2 scale(divPerElem(screen.getImpl(),
				  math::v2((float)m_texture_width, (float)m_texture_height).getImpl()));
	float screen_aspect = (float)m_width / (float)m_height;
	float aspect = (float)texture->m_desc.m_width / (float)texture->m_desc.m_height;
	/* the part of the capture's width, or height, the screen fills */
	float width_part = screen_aspect / aspect;
	float height_part = aspect / screen_aspect;
	math::v2 fill(width_part, 1.0f);

	if (aspect < screen_aspect)
		fill = math::v2(1.0f, height_part);

	TextureOps::CopyArea area;

	area.m_dest.m_min = math::v2(0.0f, 0.0f);
	area.m_dest.m_max = fill;
	area.m_source.m_min = math::v2(0.0f, 0.0f);
	area.m_source.m_max = scale;
	m_texture_ops->half(m_capture, texture, 0, area, !m_pow2, m_half_flag);
	if (m_pow2) {
		/* the rest of the capture filled from the screen's last column or row */
		TextureOps::CopyArea edge;

		if (aspect < screen_aspect) {
			edge.m_dest.m_min = math::v2(0.0f, height_part);
			edge.m_source.m_min = math::v2(0.0f, 1.0f);
		} else {
			edge.m_dest.m_min = math::v2(width_part, 0.0f);
			edge.m_source.m_min = math::v2(1.0f, 0.0f);
		}
		edge.m_dest.m_max = math::v2(1.0f, 1.0f);
		edge.m_source.m_max = math::v2(1.0f, 1.0f);
		m_texture_ops->copy(m_capture, texture, 0, edge, 1.0f);
	}
	m_capture_area.m_min = math::v2(0.0f, 0.0f);
	m_capture_area.m_max = fill;
	m_source_area.m_min = math::v2(0.0f, 0.0f);
	m_source_area.m_max = scale;
	m_capture->setLodBias(-10.0f);

	for (uint32_t i = 0; i != NUM_LEVELS; i++) {
		TextureOps::CopyArea whole;

		whole.m_dest.m_min = math::v2(0.0f, 0.0f);
		whole.m_dest.m_max = math::v2(1.0f, 1.0f);
		whole.m_source.m_min = math::v2(0.0f, 0.0f);
		whole.m_source.m_max = math::v2(1.0f, 1.0f);
		m_texture_ops->half(m_levels[i], i == 0 ? m_capture : m_levels[i - 1], 0, whole,
				    !m_pow2, false);
	}
	for (uint32_t i = 0; i < m_box_blur_passes; i++)
		m_texture_ops->boxBlur(m_levels[NUM_LEVELS - 1 - i],
				       GetTemporary(m_levels[NUM_LEVELS - 1 - i]));

	uint32_t levels = m_capture->m_desc.m_levels;

	m_amount = amount * (float)levels;
	m_level_names.resize(levels);
	for (int32_t level = 0; level < m_capture->m_desc.m_levels; level++) {
		m_level_names[level] = DigitChar(level);

		TextureOps::CopyArea whole;

		if (level == 0 && m_copy_first_level) {
			whole.m_dest.m_min = math::v2(0.0f, 0.0f);
			whole.m_dest.m_max = math::v2(1.0f, 1.0f);
			whole.m_source.m_min = math::v2(0.0f, 0.0f);
			whole.m_source.m_max = math::v2(1.0f, 1.0f);
			m_texture_ops->copy(m_capture, m_levels[0], 0, whole, 1.0f);
		} else {
			int32_t index = level + m_level_offset - 1;

			if (index < 0)
				index = 0;

			const common::SharedPtr<arl::Texture2D> &source =
				m_levels[index < NUM_LEVELS - 1 ? index : NUM_LEVELS - 1];

			if (m_bicubic) {
				if (m_bicubic_slow)
					m_texture_ops->bicubicSlow(m_capture, source, level);
				else
					m_texture_ops->bicubic(m_capture, source, level);
			} else {
				whole.m_dest.m_min = math::v2(0.0f, 0.0f);
				whole.m_dest.m_max = math::v2(1.0f, 1.0f);
				whole.m_source.m_min = math::v2(0.0f, 0.0f);
				whole.m_source.m_max = math::v2(1.0f, 1.0f);
				m_texture_ops->copy(m_capture, source, level, whole, 1.0f);
			}
		}
	}
	m_capture->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, true);
	m_capture->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
}

/* blending by the source's alpha (name ours) */
static inline void EnableAlphaBlend()
{
	arl::ArlInstance *gl = arl::Gl();

	gl->Enable(arl::BLEND);
	gl->BlendFunc(arl::SRC_ALPHA, arl::ONE_MINUS_SRC_ALPHA, arl::ONE, arl::ONE_MINUS_SRC_ALPHA);
	gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);
}

void Blur::Draw(float amount)
{
	arl::ArlInstance *gl = arl::Gl();

	EnableAlphaBlend();
	ScePafGraphics_199BD5F2(gl, 0);
	draw_uniform_blur(amount);
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	gl->Disable(arl::BLEND);
}

}
}
