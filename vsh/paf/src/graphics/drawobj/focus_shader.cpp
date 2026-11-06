/*
 * paf::graph::FocusShader (focus_shader.h): the key focus's planes drawn with
 * paf/cg/drawobj/focus, their texture lit in the shades of the focus's colour. The file's name
 * is ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/color.h>
#include <graphics/drawobj/focus_shader.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/effects/wave_renderer.h>
#include <graphics/surface/surface.h>
#include <framework.h>
#include <std/string.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v4.h>
#include "../../std/math.h"

namespace paf {
namespace graph {

using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector4;

/* paf/cg/drawobj/focus/focus.vp: the mesh moved by the model view projection, its texture
   coordinates mapped into the region of the texture */
class FocusVertShader : public arl::ShaderInterfaceBase {
public:
	FocusVertShader() : m_mvp(m_setup), m_region0(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
		m_region0.init("Region0", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_region0.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/focus/focus.vp", 368);

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_region0;
};

/* paf/cg/drawobj/focus/focus.fp: the texture lit in the three colours */
class FocusFragShader : public arl::ShaderInterfaceBase {
public:
	FocusFragShader()
		: m_color(m_setup), m_base_color(m_setup), m_glow_color(m_setup), m_texture0(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_base_color.init("BaseColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_glow_color.init("GlowColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_texture0.init("Texture0", 0, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
		m_base_color.Apply();
		m_glow_color.Apply();
		m_texture0.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/focus/focus.fp", 464);

	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<math::v4> m_base_color;
	arl::ShaderParameter<math::v4> m_glow_color;
	arl::ShaderParameter<arl::TextureSource> m_texture0;
};

static common::SharedPtr<FocusShader::VertInstance> s_vert;
static common::SharedPtr<FocusShader::FragInstance> s_frag;
static common::SharedPtr<FocusShader> s_shader;
/* how many have called Init and not Term */
static int32_t s_init_count;
/* what is drawn when the plane has no texture: 8 by 8 texels of transparent black */
static intrusive_ptr<Surface> s_blank;

/* a colour as its hue, saturation and value, from 0 to 1 each, and its alpha (name ours) */
static inline math::v4 RgbToHsv(const math::v4 &color)
{
	float r = color.extract_x();
	float g = color.extract_y();
	float b = color.extract_z();
	float a = color.extract_w();
	float min = g > r ? r : g;
	float max;
	float delta;
	float h;
	float s;

	min = b > min ? min : b;
	max = g > r ? g : r;
	max = b > max ? b : max;
	delta = max - min;
	if (__builtin_fabsf(max) > 0.00001f) {
		s = delta / max;
		if (__builtin_fabsf(delta) < 0.00001f) {
			h = 0.0f;
			s = 0.0f;
		} else {
			if (max == r)
				h = (g - b) / delta;
			else if (g == max)
				h = (b - r) / delta + 2.0f;
			else
				h = (r - g) / delta + 4.0f;
			h *= 60.0f;
			if (h < 0.0f)
				h += 360.0f;
			h /= 360.0f;
		}
	} else {
		h = 0.0f;
		s = 0.0f;
	}
	return math::v4(h, s, max, a);
}

/* the colour of a hue, saturation and value, and an alpha (name ours) */
static inline math::v4 HsvToRgb(const math::v4 &hsv)
{
	float h = hsv.extract_x();
	float s = hsv.extract_y();
	float v = hsv.extract_z();
	float a = hsv.extract_w();

	if (s == 0.0f)
		return math::v4(v, v, v, a);

	int32_t sector = (int32_t)sce_paf_floorf(h * 6.0f);
	float f = h * 6.0f - (float)sector;
	float p = v * (1.0f - s);
	float q = v * (1.0f - f * s);
	float t = v * (1.0f - s * (1.0f - f));

	switch (sector) {
	case 0:
		return math::v4(v, t, p, a);
	case 1:
		return math::v4(q, v, p, a);
	case 2:
		return math::v4(p, v, t, a);
	case 3:
		return math::v4(p, q, v, a);
	case 4:
		return math::v4(t, p, v, a);
	default:
		return math::v4(v, p, q, a);
	}
}

/* the colour scaled by the scene's alpha */
static inline math::v4 SceneAlpha(arl::ArlInstance *gl, const math::v4 &color)
{
	Vector4 value = color.getImpl();

	value.setW(gl->m_color.getImpl().getW() * value.getW());
	return math::v4(value);
}

FocusShader::FocusShader()
	: m_vert(s_vert), m_frag(s_frag), m_base_color(1.0f, 1.0f, 1.0f, 1.0f),
	  m_glow_color(1.0f, 1.0f, 1.0f, 1.0f), m_color(1.0f, 1.0f, 1.0f, 1.0f)
{
}

FocusShader::~FocusShader()
{
}

int32_t FocusShader::Draw(PlaneObjDrawParams &params)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_vert.get() == NULL || m_frag.get() == NULL)
		return 0;

	intrusive_ptr<Surface> texture = s_blank;

	if (params.m_textures != NULL && params.m_num_textures != 0 &&
	    params.m_textures[0].get() != NULL) {
		params.m_textures[0]->Texturize();
		texture = params.m_textures[0];
	}

	m_vert->m_interface.m_mvp.Set(gl->GetModelViewProjection());

	if (params.m_tex_region != NULL) {
		Vector2 factor = params.m_textures[0]->GetFactor().getImpl();

		m_vert->m_interface.m_region0.Set(
			math::v4(mulPerElem(params.m_tex_region->getImpl(), Vector4(factor, factor))));
	} else {
		Vector2 factor = params.m_textures[0]->GetFactor().getImpl();

		m_vert->m_interface.m_region0.Set(math::v4(Vector4(Vector2(0.0f, 0.0f), factor)));
	}
	m_frag->m_interface.m_base_color.Set(SceneAlpha(gl, m_base_color));
	m_frag->m_interface.m_glow_color.Set(SceneAlpha(gl, m_glow_color));
	m_frag->m_interface.m_color.Set(SceneAlpha(gl, m_color));
	{
		arl::TextureSource source;

		source.m_surface = texture;
		m_frag->m_interface.m_texture0.Set(source);
	}

	m_vert->m_object.begin();
	m_frag->m_object.begin();
	for (uint32_t i = 0; i < params.m_instances; i++) {
		arl::ShaderState *state;

		state = gl;
		state->m_dirty_vertex |= params.m_vertices[i].m_spec->m_kinds;
		state = gl;
		state->m_dirty_fragment |= 1;
		sceGxmSetVertexStream(gl->m_context, 0,
				      params.m_vertices[i].m_buffer->m_memory->m_addr);
		if (params.m_indices != NULL)
			params.m_indices[i].draw(*gl, arl::TRIANGLES, params.m_count);
		else
			arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_STRIP, 0, params.m_count);
		state = gl;
		state->m_dirty_vertex = 0;
		state = gl;
		state->m_dirty_fragment = 0;
	}
	m_frag->m_object.end();
	m_vert->m_object.end();
	return 0;
}

void FocusShader::Init()
{
	if (s_init_count == 0) {
		arl::ArlInstance *gl = arl::Gl();
		arl::ProgramState *state;

		s_vert = common::SharedPtr<VertInstance>(new VertInstance);
		if (s_vert.get() != NULL) {
			state = gl;
			s_vert->m_object.init(*gl, state->m_patcher,
					      ScePafGraphics_4D1E289A(gl,
						      arl::VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2,
						      SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					      common::SharedPtr<arl::VertexArraySpec>(),
					      common::SharedPtr<arl::VertexArraySpec>(),
					      common::SharedPtr<arl::VertexArraySpec>());
		}
		s_frag = common::SharedPtr<FragInstance>(new FragInstance);
		if (s_frag.get() != NULL) {
			state = gl;
			s_frag->m_object.init(*gl, state->m_patcher);
		}
		s_shader = common::SharedPtr<FocusShader>(new FocusShader);
		s_blank = intrusive_ptr<Surface>(new Surface(Framework::Instance()->GetSurfacePool(), 8, 8,
							     ImageMode_RGBA8888, ImageOrder_Linear, 1,
							     1, false));
		sce_paf_memset(s_blank->Lock(0), 0, 8 * 8 * 4);
		s_blank->Unlock();
		s_blank->Texturize();
	}
	s_init_count++;
}

void FocusShader::Term()
{
	if (--s_init_count == 0) {
		s_vert.reset();
		s_frag.reset();
		s_blank.clear();
		s_shader.reset();
	}
}

common::SharedPtr<Shader> &FocusShader::GetShader()
{
	return reinterpret_cast<common::SharedPtr<Shader> &>(s_shader);
}

void FocusShader::SetColor(const math::v4 &color)
{
	FocusShader *shader = s_shader.get();
	math::v4 hsv = RgbToHsv(color);

	shader->m_base_color = HsvToRgb(math::v4(hsv.extract_x(), hsv.extract_y(),
						 hsv.extract_z() * 0.6f, hsv.extract_w() * 0.6f));
	shader->m_glow_color = HsvToRgb(math::v4(hsv.extract_x(), hsv.extract_y() * 0.7f,
						 hsv.extract_z(), hsv.extract_w()));
	shader->m_color = HsvToRgb(hsv);
}

}
}
