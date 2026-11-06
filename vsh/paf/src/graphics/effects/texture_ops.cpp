/*
 * The texture operations the effects share: a texture drawn into a level of another copied,
 * faded, halved, scaled bicubic, box blurred, its normals taken or drawn through the flip
 * pattern, each with its fragment program over the unit quad. TextureOps is Sony's name (the
 * markers "TextureOps::copy" to "TextureOps::fliptri"); the file's name is ours.
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
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/color.h>
#include <graphics/effects/fft_wave_surface.h>
#include <graphics/effects/page_turn_quad.h>
#include <graphics/effects/post_effects.h>
#include <image/image_types.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/vector>
#include "../arl/gxm/gxm_framebuffer_object.h"
#include "texture_ops_programs.h"

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace graph {

/* the box blurs drawn with the programs of radius 4 and 5 (name ours); nothing reads it */
static uint32_t s_wide_box_blurs;

/* the whole of both textures (name ours) */
static inline TextureOps::CopyArea WholeArea()
{
	TextureOps::CopyArea area;

	area.m_dest = QuadArea(math::v2(0.0f, 0.0f), math::v2(1.0f, 1.0f));
	area.m_source = QuadArea(math::v2(0.0f, 0.0f), math::v2(1.0f, 1.0f));
	return area;
}

/* the area's offset, then its size: the quad's UVs placed on it (name ours) */
static inline math::v4 GetUVTransform(const QuadArea &area)
{
	return math::v4(area.m_min.extract_x(), area.m_min.extract_y(),
			(area.m_max - area.m_min).extract_x(), (area.m_max - area.m_min).extract_y());
}

/* the render states every operation draws with: no blending, no depth (name ours) */
static inline void SetDrawStates()
{
	arl::ArlInstance *gl = arl::Gl();

	gl->Disable(arl::BLEND);
	gl->DepthMask(false, arl::FRONT_AND_BACK);
	gl->Disable(arl::DEPTH_TEST);
}

/*
 * The weights and offsets of the cubic B-spline bicubic reads, one texel per step across the
 * texture: the offsets of the two texels read, then the sums of the weights each of them
 * stands for; 16-bit floats, or bytes when not half (name ours; Sony's build keeps it out of
 * line).
 */
inline __attribute__((noinline)) void InitBicubicTable(common::SharedPtr<arl::Texture2D> texture,
						       uint32_t size, bool half)
{
	float x = 0.0f;
	float last = (float)(size - 1);

	if (half) {
		ScePafGraphics_C4BD3558(texture.get(), size, 1, ImageMode_WZYX16FP, 0,
					arl::SurfaceFlags());

		Half *texels = new Half[size * 4];
		float step = 1.0f / last;
		Half *texel = texels;

		for (uint32_t i = 0; i < size; i++) {
			float x2 = x * x;
			float x3 = x2 * x;
			float w1 = (x3 * 3.0f - x2 * 6.0f + 4.0f) * 0.16666667f;
			float w3 = x3 * 0.16666667f;
			float g0 = (x2 * 3.0f - x3 - x * 3.0f + 1.0f) * 0.16666667f + w1;
			float g1 = (x3 * -3.0f + x2 * 3.0f + x * 3.0f + 1.0f) * 0.16666667f + w3;

			texel[0] = Half(x + 1.0f - w1 / g0);
			texel[1] = Half(1.0f - x + w3 / g1);
			texel[2] = Half(g0);
			texel[3] = Half(g1);
			texel += 4;
			x += step;
		}
		ScePafGraphics_309D2902(texture.get(), 0, size, 1, ImageMode_WZYX16FP, 0, texels);
		texture->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
		texture->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
		texture->setWrapS(arl::TEXTURE_WRAP_REPEAT);
		texture->setWrapT(arl::TEXTURE_WRAP_REPEAT);
		delete[] texels;
	} else {
		ScePafGraphics_C4BD3558(texture.get(), size, 1, ImageMode_RGBA8888, 0,
					arl::SurfaceFlags());

		uint8_t *texels = new uint8_t[size * 4];
		float step = 1.0f / last;
		uint8_t *texel = texels;

		for (uint32_t i = 0; i < size; i++) {
			float x2 = x * x;
			float x3 = x2 * x;
			float w1 = (x3 * 3.0f - x2 * 6.0f + 4.0f) * 0.16666667f;
			float w3 = x3 * 0.16666667f;
			float g0 = (x2 * 3.0f - x3 - x * 3.0f + 1.0f) * 0.16666667f + w1;
			float g1 = (x3 * -3.0f + x2 * 3.0f + x * 3.0f + 1.0f) * 0.16666667f + w3;
			float value;

			value = (x + 1.0f - w1 / g0) * 255.0f;
			if (value < 0.0f)
				value = 0.0f;
			texel[0] = (uint8_t)(value < 255.0f ? value : 255.0f);
			value = (1.0f - x + w3 / g1) * 255.0f;
			if (value < 0.0f)
				value = 0.0f;
			texel[1] = (uint8_t)(value < 255.0f ? value : 255.0f);
			value = g0 * 255.0f;
			if (value < 0.0f)
				value = 0.0f;
			texel[2] = (uint8_t)(value < 255.0f ? value : 255.0f);
			value = g1 * 255.0f;
			if (value < 0.0f)
				value = 0.0f;
			texel[3] = (uint8_t)(value < 255.0f ? value : 255.0f);
			texel += 4;
			x += step;
		}
		ScePafGraphics_309D2902(texture.get(), 0, size, 1, ImageMode_RGBA8888, 0, texels);
		texture->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
		texture->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
		texture->setWrapS(arl::TEXTURE_WRAP_REPEAT);
		texture->setWrapT(arl::TEXTURE_WRAP_REPEAT);
		delete[] texels;
	}
}

TextureOps::TextureOps()
{
	m_programs = new TextureOpsPrograms;

	arl::ArlInstance *gl = arl::Gl();

	m_fbo = common::SharedPtr<arl::FrameBufferObject>(new arl::FrameBufferObject());
	m_quad.Init(false, false);

	arl::ProgramState *state = gl;

	m_vert.m_object.init(*gl, state->m_patcher, m_quad.m_vertices.m_spec,
			     common::SharedPtr<arl::VertexArraySpec>(),
			     common::SharedPtr<arl::VertexArraySpec>(),
			     common::SharedPtr<arl::VertexArraySpec>());
	state = gl;
	m_programs->m_clear.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_bicubic.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_bicubic_slow.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_half.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_copy.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_pure_copy.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_n.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_1.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_2.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_3.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_4.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_5.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_n_modulate.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_1_modulate.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_2_modulate.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_3_modulate.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_4_modulate.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_box_blur_5_modulate.m_object.init(*gl, state->m_patcher);
	/* the normals are written as four halves */
	m_programs->m_grid_normal.m_object.setOutputRegisterFormat(SCE_GXM_OUTPUT_REGISTER_FORMAT_HALF4);
	state = gl;
	m_programs->m_grid_normal.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_grid_normal_8.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_flip_quad.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_programs->m_flip_tri.m_object.init(*gl, state->m_patcher);

	m_bicubic_table = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
	InitBicubicTable(m_bicubic_table, 128, false);

	m_checker = arl::CreateCheckerTexture(gl->m_default_allocator, arl::SurfaceSize(8, 8),
					      arl::SurfaceSize(1, 1), arl::SurfaceFlags());
	m_checker->setMinFilter(SCE_GXM_TEXTURE_FILTER_POINT, false);
	m_checker->setMagFilter(SCE_GXM_TEXTURE_FILTER_POINT);
	m_checker->setWrapS(arl::TEXTURE_WRAP_REPEAT);
	m_checker->setWrapT(arl::TEXTURE_WRAP_REPEAT);

	vector<uint32_t> pixels(64, 0);

	for (uint32_t y = 0; y != 8; y++) {
		for (uint32_t x = 0; x != 8; x++) {
			uint32_t &pixel = pixels[y * 8 + x];

			pixel = 0;
			if (y & 1)
				pixel = 0xff00;
			if (x & 1)
				pixel |= 0xff;
		}
	}
	m_flip_pattern = arl::CreateTexture<ImageMode_RGBA8888>(gl->m_default_allocator,
								arl::SurfaceSize(8, 8), &pixels[0],
								arl::SurfaceFlags(), 0);
	m_flip_pattern->setMinFilter(SCE_GXM_TEXTURE_FILTER_POINT, false);
	m_flip_pattern->setMagFilter(SCE_GXM_TEXTURE_FILTER_POINT);
	m_flip_pattern->setWrapS(arl::TEXTURE_WRAP_REPEAT);
	m_flip_pattern->setWrapT(arl::TEXTURE_WRAP_REPEAT);
}

TextureOps::~TextureOps()
{
	delete m_programs;
	m_programs = NULL;
}

bool TextureOps::IsFlipped(const common::SharedPtr<arl::Texture2D> &dest,
			   const common::SharedPtr<arl::Texture2D> &source)
{
	if (dest.get() == NULL)
		return false;

	bool dest_surface = dest->m_storage->m_flags.IsSet(arl::COLOR_SURFACE);

	return source->m_storage->m_flags.IsSet(arl::COLOR_SURFACE) && dest_surface;
}

void TextureOps::copy(const common::SharedPtr<arl::Texture2D> &dest,
		      const common::SharedPtr<arl::Texture2D> &source, uint32_t level,
		      const CopyArea &area, float alpha)
{
	SetDrawStates();

	arl::ArlInstance *gl = arl::Gl();

	if (dest.get() != NULL)
		m_fbo->Bind(common::SharedPtr<arl::TextureObject>(dest), level, 0);
	ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("TextureOps::copy");
	{
		common::SharedPtr<arl::Texture2D> texture = source;

		m_programs->m_copy.m_interface.m_source.Set(common::SharedPtr<arl::TextureObject>(texture));
	}
	m_programs->m_copy.m_interface.m_alpha.Set(alpha);
	m_vert.m_interface.m_mvp.Set(GetQuadMatrix(area.m_dest));
	m_vert.m_interface.m_uv_transform.Set(GetUVTransform(area.m_source));
	m_vert.m_object.begin();
	m_programs->m_copy.m_object.begin();
	DrawQuad(IsFlipped(dest, source));
	m_programs->m_copy.m_object.end();
	m_vert.m_object.end();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	if (dest.get() != NULL)
		arl::FrameBufferObject::Unbind();
}

void TextureOps::pureCopy(const common::SharedPtr<arl::Texture2D> &dest,
			  const common::SharedPtr<arl::Texture2D> &source, uint32_t level,
			  const CopyArea &area)
{
	SetDrawStates();

	arl::ArlInstance *gl = arl::Gl();

	if (dest.get() != NULL)
		m_fbo->Bind(common::SharedPtr<arl::TextureObject>(dest), level, 0);
	ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("TextureOps::pureCopy");
	{
		common::SharedPtr<arl::Texture2D> texture = source;

		m_programs->m_pure_copy.m_interface.m_source.Set(
			common::SharedPtr<arl::TextureObject>(texture));
	}
	m_vert.m_interface.m_mvp.Set(GetQuadMatrix(area.m_dest));
	m_vert.m_interface.m_uv_transform.Set(GetUVTransform(area.m_source));
	m_vert.m_object.begin();
	m_programs->m_pure_copy.m_object.begin();
	DrawQuad(IsFlipped(dest, source));
	m_programs->m_pure_copy.m_object.end();
	m_vert.m_object.end();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	if (dest.get() != NULL)
		arl::FrameBufferObject::Unbind();
}

void TextureOps::half(const common::SharedPtr<arl::Texture2D> &dest,
		      const common::SharedPtr<arl::Texture2D> &source, uint32_t level,
		      const CopyArea &area, bool flag0, bool flag1)
{
	SetDrawStates();

	arl::ArlInstance *gl = arl::Gl();

	if (dest.get() != NULL)
		m_fbo->Bind(common::SharedPtr<arl::TextureObject>(dest), level, 0);
	ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("TextureOps::half");
	{
		common::SharedPtr<arl::Texture2D> texture = source;

		m_programs->m_half.m_interface.m_source.Set(common::SharedPtr<arl::TextureObject>(texture));
	}
	m_vert.m_interface.m_mvp.Set(GetQuadMatrix(area.m_dest));
	m_vert.m_interface.m_uv_transform.Set(GetUVTransform(area.m_source));
	source->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
	source->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	m_vert.m_object.begin();
	m_programs->m_half.m_object.begin();
	DrawQuad(IsFlipped(dest, source));
	m_programs->m_half.m_object.end();
	m_vert.m_object.end();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	if (dest.get() != NULL)
		arl::FrameBufferObject::Unbind();
}

void TextureOps::bicubic(const common::SharedPtr<arl::Texture2D> &dest,
			 const common::SharedPtr<arl::Texture2D> &source, uint32_t level)
{
	SetDrawStates();

	arl::ArlInstance *gl = arl::Gl();

	if (dest.get() != NULL)
		m_fbo->Bind(common::SharedPtr<arl::TextureObject>(dest), level, 0);
	ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("TextureOps::bicubic");
	{
		common::SharedPtr<arl::Texture2D> texture = source;

		m_programs->m_bicubic.m_interface.m_source.Set(
			common::SharedPtr<arl::TextureObject>(texture));
	}

	Vector2 size((float)source->m_desc.m_width, (float)source->m_desc.m_height);
	Vector2 texel = recipPerElem(size);

	m_programs->m_bicubic.m_interface.m_source_size.Set(
		math::v4(size.getX(), size.getY(), texel.getX(), texel.getY()));
	{
		common::SharedPtr<arl::Texture2D> table = m_bicubic_table;

		m_programs->m_bicubic.m_interface.m_table.Set(common::SharedPtr<arl::TextureObject>(table));
	}
	m_vert.m_interface.m_mvp.Set(GetQuadMatrix(WholeArea().m_dest));
	m_vert.m_interface.m_uv_transform.Set(GetUVTransform(WholeArea().m_source));
	m_vert.m_object.begin();
	m_programs->m_bicubic.m_object.begin();
	DrawQuad(IsFlipped(dest, source));
	m_programs->m_bicubic.m_object.end();
	m_vert.m_object.end();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	if (dest.get() != NULL)
		arl::FrameBufferObject::Unbind();
}

void TextureOps::bicubicSlow(const common::SharedPtr<arl::Texture2D> &dest,
			     const common::SharedPtr<arl::Texture2D> &source, uint32_t level)
{
	SetDrawStates();

	arl::ArlInstance *gl = arl::Gl();

	if (dest.get() != NULL)
		m_fbo->Bind(common::SharedPtr<arl::TextureObject>(dest), level, 0);
	ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("TextureOps::bicubicSlow");
	{
		common::SharedPtr<arl::Texture2D> texture = source;

		m_programs->m_bicubic_slow.m_interface.m_source.Set(
			common::SharedPtr<arl::TextureObject>(texture));
	}

	Vector2 size((float)source->m_desc.m_width, (float)source->m_desc.m_height);
	Vector2 texel = recipPerElem(size);

	m_programs->m_bicubic_slow.m_interface.m_source_size.Set(
		math::v4(size.getX(), size.getY(), texel.getX(), texel.getY()));
	m_vert.m_interface.m_mvp.Set(GetQuadMatrix(WholeArea().m_dest));
	m_vert.m_interface.m_uv_transform.Set(GetUVTransform(WholeArea().m_source));
	m_vert.m_object.begin();
	m_programs->m_bicubic_slow.m_object.begin();
	DrawQuad(IsFlipped(dest, source));
	m_programs->m_bicubic_slow.m_object.end();
	m_vert.m_object.end();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	if (dest.get() != NULL)
		arl::FrameBufferObject::Unbind();
}

void TextureOps::boxBlur(const common::SharedPtr<arl::Texture2D> &texture,
			 const common::SharedPtr<arl::Texture2D> &temporary, float radius, float step)
{
	if ((int32_t)radius <= 0)
		return;

	SetDrawStates();

	arl::ArlInstance *gl = arl::Gl();

	m_vert.m_interface.m_mvp.Set(GetQuadMatrix(WholeArea().m_dest));
	m_vert.m_interface.m_uv_transform.Set(GetUVTransform(WholeArea().m_source));

	/* the temporary drawn into first, from the texture, then the texture from it */
	common::SharedPtr<arl::Texture2D> targets[2] = { temporary, texture };
	BoxBlurParams *params = &m_programs->m_box_blur_n.m_interface;
	arl::FragShaderObject *object = &m_programs->m_box_blur_n.m_object;

	switch ((int32_t)radius) {
	case 1:
		params = &m_programs->m_box_blur_1.m_interface;
		object = &m_programs->m_box_blur_1.m_object;
		break;
	case 2:
		params = &m_programs->m_box_blur_2.m_interface;
		object = &m_programs->m_box_blur_2.m_object;
		break;
	case 3:
		params = &m_programs->m_box_blur_3.m_interface;
		object = &m_programs->m_box_blur_3.m_object;
		break;
	case 4:
		params = &m_programs->m_box_blur_4.m_interface;
		object = &m_programs->m_box_blur_4.m_object;
		s_wide_box_blurs++;
		break;
	case 5:
		params = &m_programs->m_box_blur_5.m_interface;
		object = &m_programs->m_box_blur_5.m_object;
		s_wide_box_blurs++;
		break;
	default:
		break;
	}

	for (uint32_t pass = 0; pass != 2; pass++) {
		m_fbo->Bind(common::SharedPtr<arl::TextureObject>(targets[pass & 1]), 0, 0);
		ScePafGraphics_199BD5F2(gl, 0);
		gl->PushMarker(pass == 0 ? "TextureOps::boxBlur(0)" : "TextureOps::boxBlur(1)");

		common::SharedPtr<arl::Texture2D> &source = targets[(pass + 1) & 1];

		source->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
		source->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
		{
			common::SharedPtr<arl::TextureObject> source_object(source);
			arl::TextureSource texture_source;

			texture_source = source_object;
			params->SetSource(texture_source);
		}
		params->SetSpread(radius);

		arl::Rect &viewport = gl->m_viewport.Top().m_viewport;
		Vector2 size((float)(viewport.m_x1 - viewport.m_x0),
			     (float)(viewport.m_y1 - viewport.m_y0));
		Vector2 direction = pass == 0 ? Vector2(1.0f, 0.0f) : Vector2(0.0f, 1.0f);

		params->SetStep(math::v2(mulPerElem(recipPerElem(size), direction) * step));
		m_vert.m_object.begin();
		object->begin();
		DrawQuad();
		object->end();
		m_vert.m_object.end();
		gl->PopMarker();
		ScePafGraphics_29F5AA8B(gl, NULL, false);
		arl::FrameBufferObject::Unbind();
	}
}

void TextureOps::gridNormal8(common::SharedPtr<arl::Texture2D> &normals,
			     common::SharedPtr<arl::Texture2D> &source, uint32_t level,
			     uint32_t scene_flags)
{
	SetDrawStates();

	arl::ArlInstance *gl = arl::Gl();

	if (normals.get() != NULL)
		m_fbo->Bind(common::SharedPtr<arl::TextureObject>(normals), level, 0);
	ScePafGraphics_199BD5F2(gl, scene_flags);
	gl->PushMarker("TextureOps::gridNormal8");
	{
		common::SharedPtr<arl::Texture2D> texture = source;

		m_programs->m_grid_normal_8.m_interface.m_source.Set(
			common::SharedPtr<arl::TextureObject>(texture));
	}

	Vector2 texel = recipPerElem(Vector2((float)source->m_desc.m_width,
					     (float)source->m_desc.m_height));
	bool flipped = IsFlipped(normals, source);

	texel.setY(texel.getY() * (flipped ? -1.0f : 1.0f));
	m_programs->m_grid_normal_8.m_interface.m_source_du_dv.Set(
		math::v4(texel.getX(), 0.0f, 0.0f, texel.getY()));
	m_vert.m_interface.m_mvp.Set(GetQuadMatrix(WholeArea().m_dest));
	m_vert.m_interface.m_uv_transform.Set(GetUVTransform(WholeArea().m_source));
	m_vert.m_object.begin();
	m_programs->m_grid_normal_8.m_object.begin();
	DrawQuad(flipped);
	m_programs->m_grid_normal_8.m_object.end();
	m_vert.m_object.end();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	if (normals.get() != NULL)
		arl::FrameBufferObject::Unbind();
}

void TextureOps::fliptri(common::SharedPtr<arl::Texture2D> &dest,
			 common::SharedPtr<arl::Texture2D> &source, uint32_t level, float amount)
{
	SetDrawStates();

	arl::ArlInstance *gl = arl::Gl();

	if (dest.get() != NULL)
		m_fbo->Bind(common::SharedPtr<arl::TextureObject>(dest), level, 0);
	ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("TextureOps::fliptri");
	{
		common::SharedPtr<arl::Texture2D> texture = source;

		m_programs->m_flip_tri.m_interface.m_source.Set(
			common::SharedPtr<arl::TextureObject>(texture));
	}

	Vector2 texel = recipPerElem(Vector2((float)source->m_desc.m_width,
					     (float)source->m_desc.m_height));
	Vector2 half_texel = texel * 0.5f;

	m_programs->m_flip_tri.m_interface.m_offset0.Set(
		math::v2(mulPerElem(Vector2(0.0f, 1.0f), texel) - half_texel));
	m_programs->m_flip_tri.m_interface.m_offset1.Set(
		math::v2(mulPerElem(Vector2(1.0f - amount, 0.0f), texel) - half_texel));
	m_programs->m_flip_tri.m_interface.m_offset2.Set(
		math::v2(mulPerElem(Vector2(1.0f, amount), texel) - half_texel));
	{
		common::SharedPtr<arl::Texture2D> pattern = m_flip_pattern;

		m_programs->m_flip_tri.m_interface.m_flip_tex.Set(
			common::SharedPtr<arl::TextureObject>(pattern));
	}
	m_programs->m_flip_tri.m_interface.m_flip_tex_size.Set(
		math::v2((float)m_flip_pattern->m_desc.m_width, (float)m_flip_pattern->m_desc.m_height));
	m_vert.m_interface.m_mvp.Set(GetQuadMatrix(WholeArea().m_dest));
	m_vert.m_interface.m_uv_transform.Set(GetUVTransform(WholeArea().m_source));
	m_vert.m_object.begin();
	m_programs->m_flip_tri.m_object.begin();
	DrawQuad(IsFlipped(dest, source));
	m_programs->m_flip_tri.m_object.end();
	m_vert.m_object.end();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	if (dest.get() != NULL)
		arl::FrameBufferObject::Unbind();
}

}
}
