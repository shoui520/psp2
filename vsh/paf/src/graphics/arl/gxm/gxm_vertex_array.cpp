/*
 * paf::graph::arl's draws: a range of vertices through the front end's index array, an index
 * array's indices, and both instanced (the file's name ours).
 */
#include <stdint.h>
#include <gxm.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/color.h>
#include <graphics/surface/surface.h>

namespace paf {
namespace graph {
namespace arl {

/* after a draw: the uniforms set again for the next, and the surface told */
static inline void DrawDone(ArlInstance &gl)
{
	ProgramState *state = &gl;

	state->m_dirty_uniforms |= ProgramState::DIRTY_FRAGMENT_UNIFORMS |
				   ProgramState::DIRTY_VERTEX_UNIFORMS;
	intrusive_ptr<Surface>(gl.m_next_scene.m_surface)->SetDirty(gl.m_debug_surface_draw);
}

void VertexArray::DrawArrays(ArlInstance &gl, Primitive primitive, int32_t first,
			     uint32_t count)
{
	IndexArray *indices = gl.m_quad_indices;
	uint32_t index_size = 4;

	if (indices->m_format == SCE_GXM_INDEX_FORMAT_U16)
		index_size = 2;
	sceGxmDraw(gl.m_context, (SceGxmPrimitiveType)primitive, indices->m_format,
		   (const uint8_t *)indices->m_memory->m_memory->m_addr + first * index_size,
		   count);
	DrawDone(gl);
}

void IndexArray::draw(ArlInstance &gl, Primitive primitive, uint32_t count)
{
	sceGxmDraw(gl.m_context, (SceGxmPrimitiveType)primitive, m_format,
		   m_memory->m_memory->m_addr, count);
	DrawDone(gl);
}

}
}
}

using namespace paf::graph::arl;

void ScePafGraphics_72FB29E9(ArlInstance *gl, Primitive primitive, uint32_t count, uint32_t wrap)
{
	IndexArray *indices = gl->m_quad_indices;

	sceGxmDrawInstanced(gl->m_context, (SceGxmPrimitiveType)primitive, indices->m_format,
			    indices->m_memory->m_memory->m_addr, count, wrap);
	DrawDone(*gl);
}

void ScePafGraphics_AE5BBA4B(IndexArray *indices, ArlInstance *gl, Primitive primitive,
			     uint32_t count, uint32_t wrap)
{
	sceGxmDrawInstanced(gl->m_context, (SceGxmPrimitiveType)primitive, indices->m_format,
			    indices->m_memory->m_memory->m_addr, count, wrap);
	DrawDone(*gl);
}
