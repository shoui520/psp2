/*
 * ProbeBackground: one image, G_PixelJunk.dds from the background folder, drawn over the screen
 * with the background_image programs; its reflections and refractions sample the same image.
 * The quad, the shaders and the image are made the first time the background is used. The
 * programs' names are Sony's; the class, the file's and the other names ours.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/color.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/effects/background.h>

using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

/* the texture as a sampler's source (name ours) */
static inline arl::TextureSource WhiteSource(common::SharedPtr<arl::TextureObject> texture)
{
	arl::TextureSource source;

	source = texture;
	return source;
}

void ProbeBackground::Prepare()
{
	if (m_loaded)
		return;

	arl::ArlInstance *gl = arl::Gl();

	m_quad.Init(false, false);

	{
		common::SharedPtr<arl::VertexArraySpec> spec = m_quad.m_vertices.m_spec;
		arl::ProgramState *state = gl;

		m_vert.m_object.init(*gl, state->m_patcher, spec,
				     common::SharedPtr<arl::VertexArraySpec>(),
				     common::SharedPtr<arl::VertexArraySpec>(),
				     common::SharedPtr<arl::VertexArraySpec>());
		state = gl;
		m_frag.m_object.init(*gl, state->m_patcher);
	}

	{
		string path = s_background_path + "/G_PixelJunk.dds";

		m_image.Load(path, false);
	}
	m_loaded = true;
}

ProbeBackground::ProbeBackground()
	: m_position_min(0.0f, 0.0f), m_position_max(1.0f, 1.0f), m_uv_min(0.0f, 0.0f),
	  m_uv_max(1.0f, 1.0f), m_loaded(false)
{
}

void ProbeBackground::Load(const string &dir)
{
}

void ProbeBackground::Init(common::SharedPtr<Camera> camera,
			   common::SharedPtr<SharedTexture> texture)
{
	m_shared_texture = texture;
}

void ProbeBackground::Draw()
{
	Prepare();
	if (m_image.m_texture.get() == NULL && m_image.m_surface.get() == NULL)
		return;

	arl::ArlInstance *gl = arl::Gl();

	gl->Disable(arl::BLEND);
	gl->Disable(arl::DEPTH_TEST);
	gl->DepthMask(false, arl::FRONT_AND_BACK);
	ScePafGraphics_199BD5F2(gl, 0);

	math::matrix ortho = math::matrix::Ortho(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	math::v2 size = m_position_max - m_position_min;

	m_vert.m_interface.m_mvp.Set(
		ortho * math::matrix(math::v4(size.extract_x(), 0.0f, 0.0f, 0.0f),
				     math::v4(0.0f, size.extract_y(), 0.0f, 0.0f),
				     math::v4(0.0f, 0.0f, 1.0f, 0.0f),
				     math::v4(m_position_min.extract_x(),
					      m_position_min.extract_y(), 0.0f, 1.0f)));

	math::v2 uv_size = m_uv_max - m_uv_min;

	m_vert.m_interface.m_uv_transform.Set(
		math::v4(Vector4(m_uv_min.getImpl(), uv_size.getImpl())));
	m_frag.m_interface.m_color_map.Set(m_image);
	m_vert.m_object.begin();
	m_frag.m_object.begin();

	arl::ArlInstance &draw = *arl::Gl();
	arl::ShaderState *shader_state = &draw;

	shader_state->m_dirty_vertex |= m_quad.m_flipped.m_spec->m_kinds;
	shader_state = &draw;
	shader_state->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(draw.m_context, 0, m_quad.m_flipped.m_buffer->m_memory->m_addr);
	arl::VertexArray::DrawArrays(draw, arl::TRIANGLE_STRIP, 0, 4);
	shader_state = &draw;
	shader_state->m_dirty_vertex = 0;
	shader_state = &draw;
	shader_state->m_dirty_fragment = 0;
	m_frag.m_object.end();
	m_vert.m_object.end();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	gl->Enable(arl::DEPTH_TEST);
	gl->DepthMask(true, arl::FRONT_AND_BACK);
}

arl::TextureSource ProbeBackground::GetReflectionTexture()
{
	Prepare();
	if (m_image.m_texture.get() != NULL || m_image.m_surface.get() != NULL)
		return m_image;

	arl::DefaultTextures *textures = arl::Gl();

	return WhiteSource(textures->m_white);
}

arl::TextureSource ProbeBackground::GetRefractionTexture()
{
	Prepare();
	if (m_image.m_texture.get() != NULL || m_image.m_surface.get() != NULL)
		return m_image;

	arl::DefaultTextures *textures = arl::Gl();

	return WhiteSource(textures->m_white);
}

common::SharedPtr<arl::Texture2D> ProbeBackground::GetSharedTexture()
{
	Prepare();

	SharedTexture *shared = m_shared_texture.get();
	arl::DefaultTextures *textures = arl::Gl();

	if (shared->get() != NULL)
		return *shared;
	return textures->m_white;
}

math::matrix ProbeBackground::GetUVTransform()
{
	Prepare();
	return math::matrix::Identity();
}

void ProbeBackground::SetUVTransform(const math::matrix &transform)
{
	Prepare();
}

}
}
