/*
 * The unit quad the page turn's copy shaders and the texture operations draw, as made and with
 * its texture coordinates turned upside down (the class names ours).
 */
#ifndef PAF_GRAPHICS_EFFECTS_PAGE_TURN_QUAD_H
#define PAF_GRAPHICS_EFFECTS_PAGE_TURN_QUAD_H

#include <stdint.h>
#include <gxm.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <math/ngp/v2.h>

namespace paf {
namespace graph {

/* a vertex of the copy shaders' quads: its place and its texture coordinates (names ours) */
class PageTurnQuadVertex {
public:
	math::v2 m_position;
	math::v2 m_uv;
};

/* a unit quad, a triangle strip, twice: as made, and with its texture coordinates turned upside
   down (names ours); 0x20 bytes */
class PageTurnQuad {
public:
	/* 0x812153bc: the place, the texture coordinates or both from -1 to 1 instead of 0 to 1
	   (Sony's build keeps it out of line) */
	__attribute__((noinline)) void Init(bool ndc_position, bool ndc_uv)
	{
		arl::ArlInstance *gl = arl::Gl();
		PageTurnQuadVertex vertices[4] = {
			{ math::v2(0.0f, 1.0f), math::v2(0.0f, 1.0f) },
			{ math::v2(0.0f, 0.0f), math::v2(0.0f, 0.0f) },
			{ math::v2(1.0f, 1.0f), math::v2(1.0f, 1.0f) },
			{ math::v2(1.0f, 0.0f), math::v2(1.0f, 0.0f) },
		};
		uint32_t i;

		if (ndc_position) {
			for (i = 0; i < 4; i++)
				vertices[i].m_position =
					(vertices[i].m_position - math::v2(0.5f, 0.5f)) * math::v2(2.0f, 2.0f);
		}
		if (ndc_uv) {
			for (i = 0; i < 4; i++)
				vertices[i].m_uv = (vertices[i].m_uv - math::v2(0.5f, 0.5f)) * math::v2(2.0f, 2.0f);
		}
		m_vertices = arl::VertexArray(gl->m_default_allocator,
					      ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F4,
								      SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					      vertices, 4);
		if (ndc_uv) {
			for (i = 0; i < 4; i++)
				vertices[i].m_uv = math::v2(vertices[i].m_uv.extract_x(),
							    -vertices[i].m_uv.extract_y());
		} else {
			for (i = 0; i < 4; i++)
				vertices[i].m_uv = math::v2(vertices[i].m_uv.extract_x(),
							    1.0f - vertices[i].m_uv.extract_y());
		}
		m_flipped = arl::VertexArray(gl->m_default_allocator,
					     ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F4,
								     SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					     vertices, 4);
	}

	arl::VertexArray m_vertices;
	arl::VertexArray m_flipped;
};

}
}

#endif
