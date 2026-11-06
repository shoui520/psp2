/*
 * paf::graph::arl's ClearBase: the scene's colour cleared by a quad drawn over it (the file's
 * name ours).
 */
#include <stdint.h>
#include <gxm.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_clear.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/color.h>
#include <graphics/arl/arl_defines.h>
#include <math/ngp/v2.h>

namespace paf {
namespace graph {
namespace arl {

/* counts the first clears down; nothing reads it */
static int32_t s_countdown = 5;

void ClearBase::init(ArlInstance &gl)
{
	math::v2 vertices[] = {
		math::v2(-1.0f, 1.0f),
		math::v2(-1.0f, -1.0f),
		math::v2(1.0f, 1.0f),
		math::v2(1.0f, -1.0f),
	};

	m_quad = VertexArray(gl.m_default_allocator,
			     ScePafGraphics_4D1E289A(&gl, VertexArraySpec::ATTRIB0_F2,
						     SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
			     vertices, 4);

	common::SharedPtr<VertexArraySpec> spec = m_quad.m_spec;
	ProgramState *state = &gl;

	m_vert.m_object.init(gl, state->m_patcher, spec, common::SharedPtr<VertexArraySpec>(),
			   common::SharedPtr<VertexArraySpec>(),
			   common::SharedPtr<VertexArraySpec>());
	state = &gl;
	m_frag.m_object.init(gl, state->m_patcher);
}

ClearBase::ClearBase()
	: m_initialized(false), m_color(0.0f, 0.0f, 1.0f, 0.0f)
{
}

void ClearBase::SetColor(math::v4 color)
{
	m_color = color;
}

void ClearBase::clear(ArlInstance &gl, uint32_t clear_mask)
{
	if (clear_mask & COLOR_BUFFER_BIT) {
		if (!m_initialized) {
			init(gl);
			m_initialized = true;
		}

		bool in_scene = gl.m_in_scene;

		if (s_countdown > 0)
			--s_countdown;
		if (!in_scene) {
			ScePafGraphics_199BD5F2(&gl, 0);
			gl.PushMarker("ClearBase::clear GPU TIME WASTED HERE");
		}
		gl.Disable(BLEND);
		m_vert.m_interface.m_clear_color.Set(m_color);
		m_vert.m_object.begin();
		m_frag.m_object.begin();

		ShaderState *state = &gl;

		state->m_dirty_vertex |= m_quad.m_spec->m_kinds;
		state = &gl;
		state->m_dirty_fragment |= 1;
		sceGxmSetVertexStream(gl.m_context, 0, m_quad.m_buffer->m_memory->m_addr);
		VertexArray::DrawArrays(gl, TRIANGLE_STRIP, 0, 4);
		state = &gl;
		state->m_dirty_vertex = 0;
		state = &gl;
		state->m_dirty_fragment = 0;
		m_frag.m_object.end();
		m_vert.m_object.end();
		if (!in_scene) {
			gl.PopMarker();
			ScePafGraphics_29F5AA8B(&gl, NULL, false);
		}
	}
}

}
}
}
