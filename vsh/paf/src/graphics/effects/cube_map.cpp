/*
 * The cube of six textures drawn around the camera (the programs "paf/cg/misc/cube_map.vp"
 * and ".fp"); the file's name is ours.
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
#include <graphics/effects/cube_map.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/string>
#include "../../std/math.h"

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace graph {

/* the rotation by the angle about the unit axis (name ours; Sony's build keeps it out of
   line) */
inline __attribute__((noinline)) math::matrix RotationAbout(float angle, const math::v3 &axis)
{
	Vector3 a = axis.getImpl();
	sce::Vectormath::Simd::floatInVec c(sce_paf_cosf(angle));
	Vector3 as = a * sce_paf_sinf(angle);
	Vector3 t = a * (sce::Vectormath::Simd::floatInVec(1.0f) - c);
	sce::Vectormath::Simd::floatInVec zero(0.0f);

	return math::matrix(Matrix4(Vector4(a.getX() * t.getX() + c, a.getX() * t.getY() + as.getZ(),
					    a.getX() * t.getZ() - as.getY(), zero),
				    Vector4(a.getY() * t.getX() - as.getZ(), a.getY() * t.getY() + c,
					    a.getY() * t.getZ() + as.getX(), zero),
				    Vector4(a.getZ() * t.getX() + as.getY(), a.getZ() * t.getY() - as.getX(),
					    a.getZ() * t.getZ() + c, zero),
				    Vector4::wAxis()));
}

/* the image's width and height, as a texture or as a surface; none without one (name ours) */
static inline math::v2 GetImageSize(const arl::TextureSource &source)
{
	if (source.m_texture.get() != NULL)
		return math::v2((float)source.m_texture->m_desc.m_width,
				(float)source.m_texture->m_desc.m_height);
	if (source.m_surface.get() != NULL)
		return math::v2((float)source.m_surface->GetWidth(), (float)source.m_surface->GetHeight());
	return math::v2(0.0f, 0.0f);
}

math::v4 CubeMap::GetFaceColor(int32_t face)
{
	switch (face) {
	case 0:
		return Color(1.0f, 0.0f, 0.0f, 1.0f);
	case 1:
		return Color(0.0f, 1.0f, 1.0f, 1.0f);
	case 2:
		return Color(0.0f, 1.0f, 0.0f, 1.0f);
	case 3:
		return Color(1.0f, 0.0f, 1.0f, 1.0f);
	case 4:
		return Color(0.0f, 0.0f, 1.0f, 1.0f);
	case 5:
		return Color(1.0f, 1.0f, 0.0f, 1.0f);
	default:
		return Color(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

void CubeMap::DrawFace(int32_t face, arl::ArlInstance &gl)
{
	Face &f = m_faces[face];

	gl.Color(GetFaceColor(face));
	m_frag.m_interface.m_tex.Set(f.m_texture);

	/* the colour read back and not used */
	math::v4 color = gl.m_color;

	(void)color;

	arl::ShaderState *state = &gl;

	state->m_dirty_vertex |= f.m_quad.m_spec->m_kinds;
	state = &gl;
	state->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(gl.m_context, 0, f.m_quad.m_buffer->m_memory->m_addr);
	arl::VertexArray::DrawArrays(gl, arl::TRIANGLE_STRIP, 0, 4);
	state = &gl;
	state->m_dirty_vertex = 0;
	state = &gl;
	state->m_dirty_fragment = 0;
}

const string &CubeMap::GetFaceName(int32_t face)
{
	static string names[NUM_FACES + 1] = { "x+", "x-", "y+", "y-", "z+", "z-", "error" };

	if (face >= NUM_FACES || face < 0)
		face = NUM_FACES;
	return names[face];
}

void CubeMap::LoadFace(uint32_t face, common::SharedPtr<arl::VramAllocator> allocator)
{
	Face &f = m_faces[face];
	string path = m_dir;

	path.append(GetFaceName(face));
	path += ".dds";
	f.m_texture.Load(path, false);

	float size = GetImageSize(f.m_texture).getImpl().getX();

	f.m_uv_min = 0.5f / size;
	f.m_uv_max = 1.0f - 0.5f / size;
}

CubeMap::CubeMap() : m_dir("")
{
	m_faces[0].m_index = 0;
	m_faces[1].m_index = 1;
	m_faces[2].m_index = 2;
	m_faces[3].m_index = 3;
	m_faces[4].m_index = 4;
	m_faces[5].m_index = 5;
}

void CubeMap::LoadFaces(common::SharedPtr<arl::VramAllocator> allocator)
{
	LoadFace(0, allocator);
	LoadFace(1, allocator);
	LoadFace(2, allocator);
	LoadFace(3, allocator);
	LoadFace(4, allocator);
	LoadFace(5, allocator);
}

math::matrix CubeMap::GetFaceMatrix(int32_t face)
{
	if (face >= NUM_FACES || face < 0)
		return math::matrix::Identity();

	static math::matrix s_faces[NUM_FACES];
	static bool s_initialized = false;

	if (!s_initialized) {
		for (uint32_t i = 0; i < NUM_FACES; i++)
			s_faces[i] = math::matrix::Identity();

		/* a quarter turn about z, then a quarter turn either way about y */
		math::matrix about_z = RotationAbout(1.5707964f, math::v3(0.0f, 0.0f, 1.0f));
		math::matrix about_y_back = RotationAbout(-1.5707964f, math::v3(0.0f, 1.0f, 0.0f));
		math::matrix about_y = RotationAbout(1.5707964f, math::v3(0.0f, 1.0f, 0.0f));

		s_faces[2] = s_faces[0] * about_z;
		s_faces[1] = s_faces[2] * about_z;
		s_faces[3] = s_faces[1] * about_z;
		s_faces[4] = s_faces[0] * about_y_back;
		s_faces[5] = s_faces[0] * about_y;
		s_initialized = true;
	}
	return s_faces[face];
}

void CubeMap::Init()
{
	arl::ArlInstance *gl = arl::Gl();

	for (uint32_t i = 0; i != NUM_FACES; i++) {
		Face &f = m_faces[i];
		/* the x+ face: its positions, each followed by its UVs */
		math::v4 vertices[8] = {
			math::v4(1.0f, 1.0f, 1.0f, 1.0f),
			math::v4(f.m_uv_min, f.m_uv_max, 0.0f, 0.0f),
			math::v4(1.0f, 1.0f, -1.0f, 1.0f),
			math::v4(f.m_uv_min, f.m_uv_min, 0.0f, 0.0f),
			math::v4(1.0f, -1.0f, 1.0f, 1.0f),
			math::v4(f.m_uv_max, f.m_uv_max, 0.0f, 0.0f),
			math::v4(1.0f, -1.0f, -1.0f, 1.0f),
			math::v4(f.m_uv_max, f.m_uv_min, 0.0f, 0.0f),
		};
		math::matrix turn = GetFaceMatrix(i);

		vertices[0] = math::v4(turn.getImpl() * vertices[0].getImpl());
		vertices[2] = math::v4(turn.getImpl() * vertices[2].getImpl());
		vertices[4] = math::v4(turn.getImpl() * vertices[4].getImpl());
		vertices[6] = math::v4(turn.getImpl() * vertices[6].getImpl());
		/* Sony's count is the array's eight vectors, not its four vertices: twice the
		   array is copied */
		f.m_quad = arl::VertexArray(gl->m_default_allocator,
					    ScePafGraphics_4D1E289A(gl,
								    arl::VertexArraySpec::ATTRIB0_F4_ATTRIB3_F4,
								    SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					    vertices, 8);
	}

	arl::ProgramState *state = gl;

	m_vert.m_object.init(*gl, state->m_patcher, m_faces[0].m_quad.m_spec,
			     common::SharedPtr<arl::VertexArraySpec>(),
			     common::SharedPtr<arl::VertexArraySpec>(),
			     common::SharedPtr<arl::VertexArraySpec>());
	state = gl;
	m_frag.m_object.init(*gl, state->m_patcher);
}

void CubeMap::Draw(float scale)
{
	arl::ArlInstance *gl = arl::Gl();

	if (scale == 0.0f)
		return;

	bool in_scene = gl->m_in_scene;

	if (!in_scene)
		ScePafGraphics_199BD5F2(gl, 0);
	gl->m_model.PushTop();

	/* the top of the model stack scaled (MatrixStack::Scale's body, inlined in Sony's
	   build) */
	arl::MatrixStack::Entry &top = gl->m_model.Top();

	top.m_matrix = top.m_matrix * math::matrix(Matrix4::scale(Vector3(scale)));
	top.m_rigid = false;
	top.m_inverse_valid = false;
	top.m_inverse_entry = gl->m_model.m_depth - 1;
	m_vert.m_interface.m_mvp.Set(gl->GetModelViewProjection());
	m_vert.m_interface.m_color.Set(s_color_white);
	m_frag.m_interface.m_tex.Set(m_faces[0].m_texture);
	m_vert.m_object.begin();
	m_frag.m_object.begin();
	DrawFace(0, *gl);
	DrawFace(1, *gl);
	DrawFace(2, *gl);
	DrawFace(3, *gl);
	DrawFace(4, *gl);
	DrawFace(5, *gl);
	m_frag.m_object.end();
	m_vert.m_object.end();
	gl->m_model.Pop();
	if (!in_scene)
		ScePafGraphics_29F5AA8B(gl, NULL, false);
}

void CubeMap::SetDirectory(const string &dir)
{
	m_dir = dir;
}

}
}
