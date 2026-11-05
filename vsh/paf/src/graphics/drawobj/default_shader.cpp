/*
 * paf::graph::DefaultShader, the shader gutil::Init makes the default: it draws a plane's mesh
 * with the basic programs of paf/cg/drawobj, a rectangle in the colour of the scene, the mesh's
 * edges, one or two textures, and a texture with a shadow under it, embossed, or both. Each pair
 * of programs is made the first time it draws. The programs' paths and parameter names are
 * Sony's (their strings); the file's name and every class, method and member name ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/color.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/effects/wave_renderer.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include "../../paf_types.h"

/* the default shader could not make a program's interface (name ours) */
#define SCE_PAF_ERROR_NO_MEMORY (-2136014814) /* 0x80AF0022 */

namespace paf {
namespace graph {

using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector4;

/* paf/cg/drawobj/basic_rect.vp: the mesh moved by the model view projection */
class BasicRectVertShader : public arl::ShaderInterfaceBase {
public:
	BasicRectVertShader() : m_mvp(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_rect.vp", 320);

	arl::ShaderParameter<math::matrix> m_mvp;
};

/* paf/cg/drawobj/basic_line.vp */
class BasicLineVertShader : public arl::ShaderInterfaceBase {
public:
	BasicLineVertShader() : m_mvp(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_line.vp", 320);

	arl::ShaderParameter<math::matrix> m_mvp;
};

/* paf/cg/drawobj/basic_image.vp: the texture coordinates mapped into the region of the texture */
class BasicImageVertShader : public arl::ShaderInterfaceBase {
public:
	BasicImageVertShader() : m_mvp(m_setup), m_region0(m_setup)
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

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_image.vp", 368);

	arl::ShaderParameter<math::matrix> m_mvp;
	arl::ShaderParameter<math::v4> m_region0;
};

/* paf/cg/drawobj/basic_rect.fp: the colour */
class BasicRectFragShader : public arl::ShaderInterfaceBase {
public:
	BasicRectFragShader() : m_color(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_rect.fp", 228);

	arl::ShaderParameter<math::v4> m_color;
};

/* paf/cg/drawobj/basic_image.fp: the texture times the colour */
class BasicImageFragShader : public arl::ShaderInterfaceBase {
public:
	BasicImageFragShader() : m_color(m_setup), m_texture0(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_texture0.init("Texture0", 1, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
		m_texture0.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_image.fp", 300);

	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<arl::TextureSource> m_texture0;
};

/* paf/cg/drawobj/basic_image_copy.fp: the texture alone */
class BasicImageCopyFragShader : public arl::ShaderInterfaceBase {
public:
	BasicImageCopyFragShader() : m_texture0(m_setup)
	{
	}

	virtual void Init()
	{
		m_texture0.init("Texture0", 0, 0);
	}

	virtual void Apply()
	{
		m_texture0.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_image_copy.fp", 264);

	arl::ShaderParameter<arl::TextureSource> m_texture0;
};

/* paf/cg/drawobj/basic_dual_image.fp: two textures and the colour */
class BasicDualImageFragShader : public arl::ShaderInterfaceBase {
public:
	BasicDualImageFragShader() : m_color(m_setup), m_texture0(m_setup), m_texture1(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_texture0.init("Texture0", 1, 0);
		m_texture1.init("Texture1", 2, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
		m_texture0.Apply();
		m_texture1.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_dual_image.fp", 348);

	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<arl::TextureSource> m_texture0;
	arl::ShaderParameter<arl::TextureSource> m_texture1;
};

/* paf/cg/drawobj/basic_shadow.fp: the texture's shadow in the colour, sampled a texel apart */
class BasicShadowFragShader : public arl::ShaderInterfaceBase {
public:
	BasicShadowFragShader() : m_color(m_setup), m_texture0(m_setup), m_reciprocal0(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_texture0.init("Texture0", 1, 0);
		m_reciprocal0.init("Reciprocal0", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
		m_texture0.Apply();
		m_reciprocal0.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_shadow.fp", 680);

	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<arl::TextureSource> m_texture0;
	/* one over the texture's size in texels */
	arl::ShaderParameter<math::v2> m_reciprocal0;
};

/* paf/cg/drawobj/basic_emboss.fp: the texture embossed, sampled a texel apart */
class BasicEmbossFragShader : public arl::ShaderInterfaceBase {
public:
	BasicEmbossFragShader() : m_color(m_setup), m_texture0(m_setup), m_reciprocal0(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("Color", arl::ShaderParameterBase::UNIFORM, 0);
		m_texture0.init("Texture0", 1, 0);
		m_reciprocal0.init("Reciprocal0", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
		m_texture0.Apply();
		m_reciprocal0.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/basic_emboss.fp", 568);

	arl::ShaderParameter<math::v4> m_color;
	arl::ShaderParameter<arl::TextureSource> m_texture0;
	arl::ShaderParameter<math::v2> m_reciprocal0;
};

/* a vertex program's interface made, its programs for the plane's vertex format; false when
   there is no memory (name ours) */
template<class Instance>
static inline __attribute__((always_inline)) bool NewVert(Instance *&instance, arl::ArlInstance *gl,
			   const PlaneObjDrawParams &params)
{
	instance = new Instance;
	if (instance == NULL)
		return false;

	arl::ProgramState *state = gl;

	instance->m_object.init(*gl, state->m_patcher, params.m_vertices->m_spec,
				common::SharedPtr<arl::VertexArraySpec>(),
				common::SharedPtr<arl::VertexArraySpec>(),
				common::SharedPtr<arl::VertexArraySpec>());
	return true;
}

/* a fragment program's interface made (name ours) */
template<class Instance>
static inline __attribute__((always_inline)) bool NewFrag(Instance *&instance, arl::ArlInstance *gl)
{
	instance = new Instance;
	if (instance == NULL)
		return false;

	arl::ProgramState *state = gl;

	instance->m_object.init(*gl, state->m_patcher);
	return true;
}

/* the surface's texture, made in place: TextureSource's own constructor stays out of line
   (name ours) */
static inline __attribute__((always_inline)) arl::TextureSource
SurfaceSource(const intrusive_ptr<Surface> &surface)
{
	arl::TextureSource source;

	source.m_surface = surface;
	return source;
}

/* the part of the first texture the image maps onto: the plane's region of it, else the whole
   of it (name ours) */
static inline void SetRegion(BasicImageVertShader &vert, const PlaneObjDrawParams &params)
{
	Vector2 factor = params.m_textures[0]->GetFactor().getImpl();

	if (params.m_tex_region != NULL)
		vert.m_region0.Set(math::v4(
			mulPerElem(params.m_tex_region->getImpl(), Vector4(factor, factor))));
	else
		vert.m_region0.Set(math::v4(Vector4(Vector2(0.0f, 0.0f), factor)));
}

/* one over the first texture's size (name ours) */
static inline math::v2 TexelSize(const PlaneObjDrawParams &params)
{
	Surface *surface = params.m_textures[0].get();

	return math::v2(divPerElem(Vector2(1.0f),
				   Vector2((float)surface->GetTexWidth(), (float)surface->GetTexHeight())));
}

/* the plane's vertex array i bound to the stream (name ours) */
static inline void BindVertices(arl::ArlInstance *gl, const PlaneObjDrawParams &params,
				uint32_t i)
{
	arl::ShaderState *state;

	state = gl;
	state->m_dirty_vertex |= params.m_vertices[i].m_spec->m_kinds;
	state = gl;
	state->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(gl->m_context, 0, params.m_vertices[i].m_buffer->m_memory->m_addr);
}

/* the inputs marked unchanged again (name ours) */
static inline void UnbindVertices(arl::ArlInstance *gl)
{
	arl::ShaderState *state;

	state = gl;
	state->m_dirty_vertex = 0;
	state = gl;
	state->m_dirty_fragment = 0;
}

/* the plane's meshes drawn with the programs begun: by their index arrays when it has them,
   else as strips (name ours) */
static inline void DrawMeshes(arl::ArlInstance *gl, const PlaneObjDrawParams &params)
{
	uint32_t i;

	if (params.m_indices != NULL) {
		for (i = 0; i < params.m_instances; i++) {
			BindVertices(gl, params, i);
			params.m_indices[i].draw(*gl, arl::TRIANGLES, params.m_count);
			UnbindVertices(gl);
		}
	} else {
		for (i = 0; i < params.m_instances; i++) {
			BindVertices(gl, params, i);
			arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_STRIP, 0, params.m_count);
			UnbindVertices(gl);
		}
	}
}

DefaultShader::DefaultShader()
	: m_rect_vert(NULL), m_line_vert(NULL), m_image_vert(NULL), m_rect_frag(NULL),
	  m_image_frag(NULL), m_image_copy_frag(NULL), m_dual_image_frag(NULL),
	  m_shadow_frag(NULL), m_emboss_frag(NULL)
{
}

DefaultShader::~DefaultShader()
{
	delete m_rect_vert;
	delete m_line_vert;
	delete m_image_vert;
	delete m_rect_frag;
	delete m_image_frag;
	delete m_image_copy_frag;
	delete m_dual_image_frag;
	delete m_shadow_frag;
	delete m_emboss_frag;
}

int32_t DefaultShader::DrawShadowEmboss(PlaneObjDrawParams &params)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_image_vert == NULL && !NewVert(m_image_vert, gl, params))
		return SCE_PAF_ERROR_NO_MEMORY;
	if (m_emboss_frag == NULL && !NewFrag(m_emboss_frag, gl))
		return SCE_PAF_ERROR_NO_MEMORY;
	if (m_shadow_frag == NULL && !NewFrag(m_shadow_frag, gl))
		return SCE_PAF_ERROR_NO_MEMORY;

	m_image_vert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
	SetRegion(m_image_vert->m_interface, params);

	math::v2 texel = TexelSize(params);

	m_emboss_frag->m_interface.m_color.Set(gl->m_color);
	m_emboss_frag->m_interface.m_texture0.Set(SurfaceSource(params.m_textures[0]));
	m_emboss_frag->m_interface.m_reciprocal0.Set(texel);
	m_shadow_frag->m_interface.m_color.Set(*params.m_effect_color);
	m_shadow_frag->m_interface.m_texture0.Set(SurfaceSource(params.m_textures[0]));
	m_shadow_frag->m_interface.m_reciprocal0.Set(texel);

	m_image_vert->m_object.begin();
	for (uint32_t i = 0; i < params.m_instances; i++) {
		BindVertices(gl, params, i);
		m_shadow_frag->m_object.begin();
		arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_STRIP, 0, params.m_count);
		m_shadow_frag->m_object.end();
		m_emboss_frag->m_object.begin();
		arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_STRIP, 0, params.m_count);
		m_emboss_frag->m_object.end();
		UnbindVertices(gl);
	}
	m_image_vert->m_object.end();

	m_shadow_frag->m_interface.m_texture0.Set(arl::TextureSource());
	m_emboss_frag->m_interface.m_texture0.Set(arl::TextureSource());
	return SCE_PAF_OK;
}

int32_t DefaultShader::DrawEmboss(PlaneObjDrawParams &params)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_image_vert == NULL && !NewVert(m_image_vert, gl, params))
		return SCE_PAF_ERROR_NO_MEMORY;
	if (m_emboss_frag == NULL && !NewFrag(m_emboss_frag, gl))
		return SCE_PAF_ERROR_NO_MEMORY;

	m_image_vert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
	SetRegion(m_image_vert->m_interface, params);
	m_emboss_frag->m_interface.m_color.Set(gl->m_color);
	m_emboss_frag->m_interface.m_texture0.Set(SurfaceSource(params.m_textures[0]));
	m_emboss_frag->m_interface.m_reciprocal0.Set(TexelSize(params));

	m_image_vert->m_object.begin();
	m_emboss_frag->m_object.begin();
	for (uint32_t i = 0; i < params.m_instances; i++) {
		BindVertices(gl, params, i);
		arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_STRIP, 0, params.m_count);
		UnbindVertices(gl);
	}
	m_emboss_frag->m_object.end();
	m_image_vert->m_object.end();

	m_emboss_frag->m_interface.m_texture0.Set(arl::TextureSource());
	return SCE_PAF_OK;
}

int32_t DefaultShader::DrawShadow(PlaneObjDrawParams &params)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_image_vert == NULL && !NewVert(m_image_vert, gl, params))
		return SCE_PAF_ERROR_NO_MEMORY;
	if (m_image_frag == NULL && !NewFrag(m_image_frag, gl))
		return SCE_PAF_ERROR_NO_MEMORY;
	if (m_shadow_frag == NULL && !NewFrag(m_shadow_frag, gl))
		return SCE_PAF_ERROR_NO_MEMORY;

	m_image_vert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
	SetRegion(m_image_vert->m_interface, params);
	m_image_frag->m_interface.m_color.Set(gl->m_color);
	m_image_frag->m_interface.m_texture0.Set(SurfaceSource(params.m_textures[0]));
	m_shadow_frag->m_interface.m_color.Set(*params.m_effect_color);
	m_shadow_frag->m_interface.m_texture0.Set(SurfaceSource(params.m_textures[0]));
	m_shadow_frag->m_interface.m_reciprocal0.Set(TexelSize(params));

	m_image_vert->m_object.begin();
	for (uint32_t i = 0; i < params.m_instances; i++) {
		BindVertices(gl, params, i);
		m_shadow_frag->m_object.begin();
		arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_STRIP, 0, params.m_count);
		m_shadow_frag->m_object.end();
		m_image_frag->m_object.begin();
		arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_STRIP, 0, params.m_count);
		m_image_frag->m_object.end();
		UnbindVertices(gl);
	}
	m_image_vert->m_object.end();

	m_shadow_frag->m_interface.m_texture0.Set(arl::TextureSource());
	m_image_frag->m_interface.m_texture0.Set(arl::TextureSource());
	return SCE_PAF_OK;
}

int32_t DefaultShader::DrawLine(PlaneObjDrawParams &params)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_line_vert == NULL && !NewVert(m_line_vert, gl, params))
		return SCE_PAF_ERROR_NO_MEMORY;
	if (m_rect_frag == NULL && !NewFrag(m_rect_frag, gl))
		return SCE_PAF_ERROR_NO_MEMORY;

	gl->PolygonMode(arl::POLYGON_MODE_LINE, arl::FRONT_AND_BACK);
	m_line_vert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
	m_rect_frag->m_interface.m_color.Set(gl->m_color);

	m_line_vert->m_object.begin();
	m_rect_frag->m_object.begin();
	for (uint32_t i = 0; i < params.m_instances; i++) {
		BindVertices(gl, params, i);
		arl::VertexArray::DrawArrays(*gl, arl::LINES, 0, params.m_count);
		UnbindVertices(gl);
	}
	m_rect_frag->m_object.end();
	m_line_vert->m_object.end();
	gl->PolygonMode(arl::POLYGON_MODE_FILL, arl::FRONT_AND_BACK);
	return SCE_PAF_OK;
}

int32_t DefaultShader::DrawRect(PlaneObjDrawParams &params)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_rect_vert == NULL && !NewVert(m_rect_vert, gl, params))
		return SCE_PAF_ERROR_NO_MEMORY;
	if (m_rect_frag == NULL && !NewFrag(m_rect_frag, gl))
		return SCE_PAF_ERROR_NO_MEMORY;

	m_rect_vert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
	m_rect_frag->m_interface.m_color.Set(gl->m_color);

	m_rect_vert->m_object.begin();
	m_rect_frag->m_object.begin();
	DrawMeshes(gl, params);
	m_rect_frag->m_object.end();
	m_rect_vert->m_object.end();
	return SCE_PAF_OK;
}

int32_t DefaultShader::DrawImage(PlaneObjDrawParams &params)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_image_vert == NULL && !NewVert(m_image_vert, gl, params))
		return SCE_PAF_ERROR_NO_MEMORY;

	m_image_vert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
	SetRegion(m_image_vert->m_interface, params);

	bool copy = gl->m_color == math::v4(1.0f, 1.0f, 1.0f, 1.0f);

	if (copy) {
		if (m_image_copy_frag == NULL && !NewFrag(m_image_copy_frag, gl))
			return SCE_PAF_ERROR_NO_MEMORY;
		m_image_copy_frag->m_interface.m_texture0.Set(SurfaceSource(params.m_textures[0]));
		m_image_vert->m_object.begin();
		m_image_copy_frag->m_object.begin();
	} else {
		if (m_image_frag == NULL && !NewFrag(m_image_frag, gl))
			return SCE_PAF_ERROR_NO_MEMORY;
		m_image_frag->m_interface.m_color.Set(gl->m_color);
		m_image_frag->m_interface.m_texture0.Set(SurfaceSource(params.m_textures[0]));
		m_image_vert->m_object.begin();
		m_image_frag->m_object.begin();
	}

	DrawMeshes(gl, params);

	if (copy) {
		m_image_copy_frag->m_object.end();
		m_image_vert->m_object.end();
		m_image_copy_frag->m_interface.m_texture0.Set(arl::TextureSource());
	} else {
		m_image_frag->m_object.end();
		m_image_vert->m_object.end();
		m_image_frag->m_interface.m_texture0.Set(arl::TextureSource());
	}
	return SCE_PAF_OK;
}

int32_t DefaultShader::DrawDualImage(PlaneObjDrawParams &params)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_image_vert == NULL && !NewVert(m_image_vert, gl, params))
		return SCE_PAF_ERROR_NO_MEMORY;
	if (m_dual_image_frag == NULL && !NewFrag(m_dual_image_frag, gl))
		return SCE_PAF_ERROR_NO_MEMORY;

	m_image_vert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
	SetRegion(m_image_vert->m_interface, params);
	m_dual_image_frag->m_interface.m_color.Set(gl->m_color);
	m_dual_image_frag->m_interface.m_texture0.Set(SurfaceSource(params.m_textures[0]));
	m_dual_image_frag->m_interface.m_texture1.Set(SurfaceSource(params.m_textures[1]));

	m_image_vert->m_object.begin();
	m_dual_image_frag->m_object.begin();
	DrawMeshes(gl, params);
	m_dual_image_frag->m_object.end();
	m_image_vert->m_object.end();

	m_dual_image_frag->m_interface.m_texture1.Set(arl::TextureSource());
	m_dual_image_frag->m_interface.m_texture0.Set(arl::TextureSource());
	return SCE_PAF_OK;
}

int32_t DefaultShader::Draw(PlaneObjDrawParams &params)
{
	int32_t ret = SCE_PAF_ERROR_INVALID_ARGUMENT;

	if (!arl::Gl()->m_in_scene)
		return -1;

	switch (params.m_effect) {
	case PlaneObjDrawParams::Effect_None:
		if (params.m_num_textures >= 2 && params.m_textures != NULL &&
		    params.m_textures[0].get() != NULL && params.m_textures[1].get() != NULL)
			ret = DrawDualImage(params);
		else if (params.m_textures != NULL && params.m_textures[0].get() != NULL)
			ret = DrawImage(params);
		else
			ret = DrawRect(params);
		break;
	case PlaneObjDrawParams::Effect_Line:
		ret = DrawLine(params);
		break;
	case PlaneObjDrawParams::Effect_Shadow:
		if (params.m_textures != NULL && params.m_textures[0].get() != NULL &&
		    params.m_effect_color != NULL)
			ret = DrawShadow(params);
		break;
	case PlaneObjDrawParams::Effect_Emboss:
		if (params.m_textures != NULL && params.m_textures[0].get() != NULL)
			ret = DrawEmboss(params);
		break;
	case PlaneObjDrawParams::Effect_ShadowEmboss:
		if (params.m_textures != NULL && params.m_textures[0].get() != NULL &&
		    params.m_effect_color != NULL)
			ret = DrawShadowEmboss(params);
		break;
	}
	return ret;
}

}
}
