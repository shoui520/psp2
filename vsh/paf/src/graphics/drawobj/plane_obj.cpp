/*
 * paf::graph::PlaneObj: the plane draw object. A plane is a quad of the object's shader with up
 * to four textures; a first texture divided by its surface draws as a mesh of 8 to 24 vertices,
 * whose borders keep their size in pixels (times the border scale) while the middle stretches.
 * The quad, the nine-quad grid and the meshes' index arrays are made once and shared by every
 * plane (the file's name ours, after the class).
 */
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <plugin.h>
#include <versatileparam/resourceparam.h>
#include "../../std/math.h"

namespace paf {
namespace graph {

extern "C" const uint32_t ScePafGraphics_E5D2E9F9 = 0x41;

using arl::ArlInstance;
using arl::IndexArray;
using arl::VertexArray;
using arl::VertexArraySpec;

/* a vertex of the planes' meshes: its position and texture coordinates (names ours) */
class PlaneVertex {
public:
	float x;
	float y;
	float z;
	float u;
	float v;
};

/* a division's mesh: its vertices and the indices drawn (names ours) */
class PlaneDivMesh {
public:
	uint32_t m_num_vertices;
	uint32_t m_num_indices;
	const uint16_t *m_indices;
};

/* a division's index arrays, made for the first plane drawn with it and kept while any is: the
   middle of the divisions 7 to 9 is drawn apart, after the rest (names ours) */
class PlaneDivIndices {
public:
	int32_t m_refcount;
	IndexArray *m_front;
	IndexArray *m_back;
};

static const PlaneVertex s_quad_vertex_data[4] = {
	{ -0.5f, 0.5f, 0.0f, 0.0f, 0.0f },
	{ -0.5f, -0.5f, 0.0f, 0.0f, 1.0f },
	{ 0.5f, 0.5f, 0.0f, 1.0f, 0.0f },
	{ 0.5f, -0.5f, 0.0f, 1.0f, 1.0f },
};

static const PlaneVertex s_nine_vertex_data[9] = {
	{ -0.5f, 0.5f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.5f, 0.0f, 1.0f, 0.0f },
	{ 0.5f, 0.5f, 0.0f, 0.0f, 0.0f },
	{ -0.5f, 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f, 1.0f },
	{ 0.5f, 0.0f, 0.0f, 0.0f, 1.0f },
	{ -0.5f, -0.5f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, -0.5f, 0.0f, 1.0f, 0.0f },
	{ 0.5f, -0.5f, 0.0f, 0.0f, 0.0f },
};

/* the meshes' vertices before the layout places them */
static const PlaneVertex s_div_vertex_data[24] = {
};

/* the triangles of the nine-quad grid, and of each division's grid of 2 to 6 vertices across
   and down, row by row from the top left */
static const uint16_t s_nine_indices_data[24] = {
	0, 1, 3, 1, 3, 4, 1, 2, 4, 2, 4, 5, 3, 4, 6, 4, 6, 7, 4, 5, 7, 5, 7, 8,
};

static const uint16_t s_div_3x1_indices[18] = {
	0, 1, 2, 1, 2, 3, 2, 3, 4, 3, 4, 5, 4, 5, 6, 5, 6, 7,
};

static const uint16_t s_div_3x1_ends_indices[12] = {
	0, 1, 2, 1, 2, 3, 4, 5, 6, 5, 6, 7,
};

static const uint16_t s_div_3x1_middle_indices[6] = {
	2, 3, 4, 3, 4, 5,
};

static const uint16_t s_div_1x3_indices[18] = {
	0, 1, 4, 1, 4, 5, 1, 2, 5, 2, 5, 6, 2, 3, 6, 3, 6, 7,
};

static const uint16_t s_div_1x3_ends_indices[12] = {
	0, 1, 4, 1, 4, 5, 2, 3, 6, 3, 6, 7,
};

static const uint16_t s_div_1x3_middle_indices[6] = {
	1, 2, 5, 2, 5, 6,
};

static const uint16_t s_div_3x3_indices[54] = {
	0, 1, 4, 1, 4, 5, 1, 2, 5, 2, 5, 6, 2, 3, 6, 3, 6, 7,
	4, 5, 8, 5, 8, 9, 5, 6, 9, 6, 9, 10, 6, 7, 10, 7, 10, 11,
	8, 9, 12, 9, 12, 13, 9, 10, 13, 10, 13, 14, 10, 11, 14, 11, 14, 15,
};

static const uint16_t s_div_3x3_frame_indices[48] = {
	0, 1, 4, 1, 4, 5, 1, 2, 5, 2, 5, 6, 2, 3, 6, 3, 6, 7,
	4, 5, 8, 5, 8, 9, 6, 7, 10, 7, 10, 11,
	8, 9, 12, 9, 12, 13, 9, 10, 13, 10, 13, 14, 10, 11, 14, 11, 14, 15,
};

static const uint16_t s_div_3x3_middle_indices[6] = {
	5, 6, 9, 6, 9, 10,
};

static const uint16_t s_div_3x5_indices[90] = {
	0, 1, 4, 1, 4, 5, 1, 2, 5, 2, 5, 6, 2, 3, 6, 3, 6, 7,
	4, 5, 8, 5, 8, 9, 5, 6, 9, 6, 9, 10, 6, 7, 10, 7, 10, 11,
	8, 9, 12, 9, 12, 13, 9, 10, 13, 10, 13, 14, 10, 11, 14, 11, 14, 15,
	12, 13, 16, 13, 16, 17, 13, 14, 17, 14, 17, 18, 14, 15, 18, 15, 18, 19,
	16, 17, 20, 17, 20, 21, 17, 18, 21, 18, 21, 22, 18, 19, 22, 19, 22, 23,
};

static const uint16_t s_div_5x3_indices[90] = {
	0, 1, 6, 1, 6, 7, 1, 2, 7, 2, 7, 8, 2, 3, 8, 3, 8, 9, 3, 4, 9, 4, 9, 10,
	4, 5, 10, 5, 10, 11,
	6, 7, 12, 7, 12, 13, 7, 8, 13, 8, 13, 14, 8, 9, 14, 9, 14, 15, 9, 10, 15, 10, 15, 16,
	10, 11, 16, 11, 16, 17,
	12, 13, 18, 13, 18, 19, 13, 14, 19, 14, 19, 20, 14, 15, 20, 15, 20, 21,
	15, 16, 21, 16, 21, 22, 16, 17, 22, 17, 22, 23,
};

/* by divide mode 1 to 9: 3x1, 1x3, 3x3, 3x5, 5x3, the 3x3 frame, the 3x3 frame and middle, the
   ends and middle of 3x1 and of 1x3 */
static const PlaneDivMesh s_div_meshes[9] = {
	{ 8, 18, s_div_3x1_indices },
	{ 8, 18, s_div_1x3_indices },
	{ 16, 54, s_div_3x3_indices },
	{ 24, 90, s_div_3x5_indices },
	{ 24, 90, s_div_5x3_indices },
	{ 16, 48, s_div_3x3_frame_indices },
	{ 16, 48, s_div_3x3_frame_indices },
	{ 8, 12, s_div_3x1_ends_indices },
	{ 8, 12, s_div_1x3_ends_indices },
};

/* the middles the divide modes 7 to 9 draw apart */
static const PlaneDivMesh s_div_middle_meshes[3] = {
	{ 0, 6, s_div_3x3_middle_indices },
	{ 0, 6, s_div_3x1_middle_indices },
	{ 0, 6, s_div_1x3_middle_indices },
};

static const PlaneDivMesh s_nine_mesh = { 9, 24, s_nine_indices_data };

const char PlaneObj::m_drawobj_type[] = "PlaneObj";

/* the planes alive; the shared quad, grid and index arrays go with the last */
static int32_t s_count;
static VertexArray *s_quad_vertices;
static VertexArray *s_nine_vertices;
static PlaneDivIndices s_div_indices[10];
static IndexArray *s_nine_indices;

bool PlaneObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return DrawObj::IsInherit(type);
}

const char *PlaneObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

const char *PlaneObj::DrawObjType()
{
	return m_drawobj_type;
}

PlaneObj::PlaneObj(const DrawStyle &style)
	: DrawObj(style), m_div_index(-1), m_tex_region(NULL),
	  m_plane_size(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_plane_pos(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_scale_value(sce::Vectormath::Simd::Aos::Vector3(1.0f)),
	  m_bounds_min(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_bounds_max(sce::Vectormath::Simd::Aos::Vector3(0.0f)), m_divide(NULL),
	  m_div_border_scale(1.0f), m_div_center_pos(0.0f), m_div_vertices(NULL)
{
	m_align[0] = ALIGN_CENTER;
	m_align[1] = ALIGN_CENTER;
	m_scale_mode[0] = SCALE_SIZE;
	m_scale_mode[1] = SCALE_SIZE;
	s_count++;
}

void PlaneObj::ReleaseDivMesh()
{
	if (m_div_vertices != NULL) {
		delete m_div_vertices;
		m_div_vertices = NULL;
	}
	if (m_div_index >= 0) {
		s_div_indices[m_div_index].m_refcount--;
		if (s_div_indices[m_div_index].m_refcount == 0) {
			if (s_div_indices[m_div_index].m_front != NULL)
				delete s_div_indices[m_div_index].m_front;
			s_div_indices[m_div_index].m_front = NULL;
			if (s_div_indices[m_div_index].m_back != NULL)
				delete s_div_indices[m_div_index].m_back;
			s_div_indices[m_div_index].m_back = NULL;
		}
		m_div_index = -1;
	}
}

PlaneObj::~PlaneObj()
{
	ReleaseDivMesh();
	if (--s_count == 0) {
		if (s_quad_vertices != NULL) {
			delete s_quad_vertices;
			s_quad_vertices = NULL;
		}
		if (s_nine_vertices != NULL) {
			delete s_nine_vertices;
			s_nine_vertices = NULL;
		}
		if (s_nine_indices != NULL) {
			delete s_nine_indices;
			s_nine_indices = NULL;
		}
	}
	if (m_tex_region != NULL)
		delete m_tex_region;
	if (m_divide != NULL)
		delete m_divide;
}

void PlaneObj::Update()
{
	if (m_plane_flags.m_layout_dirty) {
		UpdateLayout();
		m_plane_flags.m_layout_dirty = false;
	}
}

void PlaneObj::CreateDivMesh()
{
	ArlInstance *gl = arl::Gl();
	uint32_t mode = DivideMode();

	if (mode == 0)
		return;

	const PlaneDivMesh &mesh = s_div_meshes[mode - 1];

	if (mesh.m_num_vertices > 24)
		return;

	m_div_index = mode - 1;
	m_div_vertices = new VertexArray(gl->m_default_allocator,
					 ScePafGraphics_4D1E289A(gl, VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2,
								 SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					 s_div_vertex_data, mesh.m_num_vertices);

	PlaneDivIndices &indices = s_div_indices[mode - 1];

	if (indices.m_refcount == 0) {
		indices.m_front = new IndexArray(gl->m_default_allocator, mesh.m_indices,
						 mesh.m_num_indices);
		if (mode == 7 || mode == 8 || mode == 9) {
			const PlaneDivMesh &middle = s_div_middle_meshes[mode - 7];

			indices.m_back = new IndexArray(gl->m_default_allocator, middle.m_indices,
							middle.m_num_indices);
		}
	}
	indices.m_refcount++;
}

/* the layout first, then the mesh, its texture coordinates and its positions as they are dirty */
void PlaneObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info)
{
	if (!m_flags.m_visible)
		return;

	ScePafGraphics_B7EB9246(this, &color, &alpha);
	if (!(alpha > 0.0f))
		return;

	if (m_plane_flags.m_layout_dirty) {
		UpdateLayout();
		m_plane_flags.m_layout_dirty = false;
	}
	if (m_plane_flags.m_mesh_dirty) {
		ReleaseDivMesh();
		if (DivideMode() != 0) {
			CreateDivMesh();
			m_plane_flags.m_texcoord_dirty = true;
		}
		m_plane_flags.m_mesh_dirty = false;
	}
	if (m_plane_flags.m_texcoord_dirty) {
		UpdateDivVerticesTexcoord();
		m_plane_flags.m_position_dirty = true;
		m_plane_flags.m_texcoord_dirty = false;
	}
	if (m_plane_flags.m_position_dirty) {
		UpdateDivVerticesPosition();
		m_plane_flags.m_position_dirty = false;
	}

	if (g_scene_draw_layer.m_ptr != NULL || g_scene_draw_layer2.m_ptr != NULL) {
		uint32_t key = (info.m_layer << 16) + (info.m_sub_layer << 8) + info.m_order;
		DrawCallbackArg arg;

		arg.self = this;
		arg.world_mat = &parent;
		arg.color = color;
		arg.alpha = alpha;
		if (m_flags.m_draw_list == 0)
			g_scene_draw_layer.m_ptr->Push(key, DrawCallback, arg);
		else if (m_flags.m_draw_list == 1 && g_scene_draw_layer2.m_ptr != NULL)
			g_scene_draw_layer2.m_ptr->Push(key, DrawCallback, arg);
		info.m_order++;
	}
}

void PlaneObj::DrawCallback(const DrawCallbackArg &arg)
{
	arg.self->Draw(*arg.world_mat, arg.color, arg.alpha);
}

/* the mesh, the nine-quad grid or the quad; an untextured plane drawn opaque is drawn without
   blending, and so is the middle of the divisions 7 to 9, drawn after the rest */
void PlaneObj::Draw(const math::matrix &world, uint32_t color, float alpha)
{
	BindDrawStyle();

	ArlInstance *gl = arl::Gl();

	if (s_quad_vertices == NULL)
		s_quad_vertices = new VertexArray(gl->m_default_allocator,
						  ScePafGraphics_4D1E289A(gl,
							VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2,
							SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
						  s_quad_vertex_data, 4);
	if (s_nine_vertices == NULL)
		s_nine_vertices = new VertexArray(gl->m_default_allocator,
						  ScePafGraphics_4D1E289A(gl,
							VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2,
							SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
						  s_nine_vertex_data, 9);
	if (s_nine_indices == NULL)
		s_nine_indices = new IndexArray(gl->m_default_allocator, s_nine_mesh.m_indices,
						s_nine_mesh.m_num_indices);

	if (m_tex_surface[0].get() == NULL && m_draw_style.blend == 1 && alpha == 1.0f)
		graph::BindDrawStyle(DrawStyle(m_draw_style.m_value & ~0xF));

	math::matrix world_mat;
	PlaneObjDrawParams params(m_tex_surface, m_tex_region);

	CalcWorldMatrix(world_mat, world, false);
	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));
	math::v4 draw_color(sce::Vectormath::Simd::Aos::Vector4(vmulq_f32(
		vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));

	draw_color.set_w(alpha);
	gl->Color(draw_color);
	gl->m_model.Load(world_mat);

	Shader *shader = m_shader.get();

	if (m_div_vertices == NULL) {
		if (!m_plane_flags.m_nine_patch) {
			params.m_vertices = s_quad_vertices;
			params.m_count = 4;
			params.m_instances = 1;
		} else {
			params.m_vertices = s_nine_vertices;
			params.m_indices = s_nine_indices;
			params.m_count = s_nine_mesh.m_num_indices;
			params.m_instances = 1;
		}
	} else {
		params.m_vertices = m_div_vertices;
		params.m_indices = s_div_indices[m_div_index].m_front;
		params.m_count = s_div_meshes[m_div_index].m_num_indices;
		params.m_instances = 1;
	}
	shader->Draw(params);

	if (m_div_vertices != NULL && s_div_indices[m_div_index].m_back != NULL) {
		int32_t mode = m_div_index + 1;

		if (mode == 7 || mode == 8 || mode == 9) {
			DrawStyle style = m_draw_style;

			if (alpha == 1.0f)
				m_draw_style.m_value = style.m_value & ~0xF;
			DrawObj::BindDrawStyle();
			params.m_indices = s_div_indices[m_div_index].m_back;
			params.m_count = s_div_middle_meshes[mode - 7].m_num_indices;
			m_shader->Draw(params);
			m_draw_style = style;
		}
	}
}

void PlaneObj::CheckTextureDivide(const intrusive_ptr<Surface> &current,
				  const intrusive_ptr<Surface> &texture)
{
	if (m_divide != NULL)
		return;

	uint32_t mode;
	math::v2 ratio_h;
	math::v2 ratio_v;
	math::v2 stretch;

	if (current.get() != NULL) {
		mode = current->GetDivideMode();
		ratio_h = math::v2(0.0f, 0.0f);
		ratio_v = math::v2(0.0f, 0.0f);
		stretch = math::v2(0.0f, 0.0f);
		ScePafGraphics_AFAE7807(current.get(), &ratio_h, &ratio_v);
		current->GetDivideStretchRatio(stretch);
	} else {
		ratio_h = math::v2(0.0f, 0.0f);
		mode = 0;
		ratio_v = math::v2(0.0f, 0.0f);
		stretch = math::v2(0.0f, 0.0f);
	}

	uint32_t new_mode = 0;
	math::v2 new_ratio_h(0.0f, 0.0f);
	math::v2 new_ratio_v(0.0f, 0.0f);
	math::v2 new_stretch(0.0f, 0.0f);

	if (texture.get() != NULL) {
		new_mode = texture->GetDivideMode();
		ScePafGraphics_AFAE7807(texture.get(), &new_ratio_h, &new_ratio_v);
		texture->GetDivideStretchRatio(new_stretch);
	}

	if (mode != new_mode)
		m_plane_flags.m_mesh_dirty = true;
	if (!((ratio_h.getImpl().getX() == new_ratio_h.getImpl().getX()) &
	      (ratio_h.getImpl().getY() == new_ratio_h.getImpl().getY())).getAsBool())
		m_plane_flags.m_texcoord_dirty = true;
	if (!((ratio_v.getImpl().getX() == new_ratio_v.getImpl().getX()) &
	      (ratio_v.getImpl().getY() == new_ratio_v.getImpl().getY())).getAsBool())
		m_plane_flags.m_texcoord_dirty = true;
	if (!((stretch.getImpl().getX() == new_stretch.getImpl().getX()) &
	      (stretch.getImpl().getY() == new_stretch.getImpl().getY())).getAsBool())
		m_plane_flags.m_texcoord_dirty = true;
}

/* a new first texture may divide the plane otherwise */
void PlaneObj::SetTexture(const intrusive_ptr<Surface> &texture, uint32_t index)
{
	if (index >= 4)
		return;

	intrusive_ptr<Surface> &current = m_tex_surface[index];

	if (current.get() != texture.get()) {
		if (index == 0)
			CheckTextureDivide(current, texture);
		current = texture.get();
		m_plane_flags.m_layout_dirty = true;
		m_plane_flags.m_position_dirty = true;
	}
	if (current.get() != NULL) {
		if (m_plane_flags.m_repeat_s)
			current->SetWrapS(REPEAT);
		if (m_plane_flags.m_repeat_t)
			current->SetWrapT(REPEAT);
	}
}

void PlaneObj::GetTexture(intrusive_ptr<Surface> &texture, uint32_t index)
{
	if (index < 4)
		texture = m_tex_surface[index].get();
}

void PlaneObj::SetPos(math::v3 pos)
{
	if (!(pos == m_pos)) {
		m_pos = pos;
		UpdateDirty();
		m_plane_flags.m_layout_dirty = true;
	}
}

void PlaneObj::SetSize(math::v3 size)
{
	if (!(size == m_size)) {
		m_size = math::v3(sce::Vectormath::Simd::Aos::maxPerElem(
			sce::Vectormath::Simd::Aos::Vector3(0.0f), size.getImpl()));
		UpdateDirty();
		m_plane_flags.m_layout_dirty = true;
		m_plane_flags.m_position_dirty = true;
	}
}

/* DrawObj's, with the plane's size and position in the object */
void PlaneObj::CalcWorldMatrix(math::matrix &world_mat, const math::matrix &parent, bool unscaled)
{
	using sce::Vectormath::Simd::Aos::Vector3;

	if (m_rot == NULL) {
		Vector3 size = maxPerElem(Vector3(0.0f), m_plane_size.getImpl());

		world_mat = parent * math::matrix(math::v4(size.getX(), 0.0f, 0.0f, 0.0f),
						  math::v4(0.0f, size.getY(), 0.0f, 0.0f),
						  math::v4(0.0f, 0.0f, size.getZ(), 0.0f),
						  math::v4(m_plane_pos, 1.0f));
	} else {
		math::matrix rot = math::quaternion::FromEulerAnglesYXZ(*m_rot).toMatrix(1.0e-4f);
		Vector3 size = maxPerElem(Vector3(0.0f), m_plane_size.getImpl());
		math::matrix local = rot * math::matrix(math::v4(size.getX(), 0.0f, 0.0f, 0.0f),
							math::v4(0.0f, size.getY(), 0.0f, 0.0f),
							math::v4(0.0f, 0.0f, size.getZ(), 0.0f),
							math::v4(0.0f, 0.0f, 0.0f, 1.0f));

		world_mat = parent * math::matrix(local.x(), local.y(), local.z(),
						  math::v4(m_plane_pos, 1.0f));
	}
}

/* (0, 0, 1, 1), the whole texture, keeps no region */
void PlaneObj::SetTexRegion(const math::v4 &region)
{
	if (region == math::v4(0.0f, 0.0f, 1.0f, 1.0f)) {
		if (m_tex_region != NULL) {
			delete m_tex_region;
			m_tex_region = NULL;
			m_plane_flags.m_layout_dirty = true;
		}
	} else if (m_tex_region == NULL) {
		m_tex_region = new math::v4(region);
		m_plane_flags.m_layout_dirty = true;
	} else if (!(region == *m_tex_region)) {
		*m_tex_region = region;
		m_plane_flags.m_layout_dirty = true;
	}
}

int32_t PlaneObj::SetMinFilter(TextureFilter value, uint32_t index)
{
	if (index < 4 && m_tex_surface[index].get() != NULL)
		return m_tex_surface[index]->SetMinFilter(value);
	return -1;
}

int32_t PlaneObj::SetMagFilter(TextureFilter value, uint32_t index)
{
	if (index < 4 && m_tex_surface[index].get() != NULL)
		return m_tex_surface[index]->SetMagFilter(value);
	return -1;
}

int32_t PlaneObj::SetWrapS(TextureWrap value, uint32_t index)
{
	if (index < 4 && m_tex_surface[index].get() != NULL)
		return m_tex_surface[index]->SetWrapS(value);
	return -1;
}

int32_t PlaneObj::SetWrapT(TextureWrap value, uint32_t index)
{
	if (index < 4 && m_tex_surface[index].get() != NULL)
		return m_tex_surface[index]->SetWrapT(value);
	return -1;
}

/* Sony's reads the magnification filter too */
int32_t PlaneObj::GetMinFilter(uint32_t index)
{
	if (index < 4 && m_tex_surface[index].get() != NULL)
		return m_tex_surface[index]->GetMagFilter();
	return -1;
}

int32_t PlaneObj::GetMagFilter(uint32_t index)
{
	if (index < 4 && m_tex_surface[index].get() != NULL)
		return m_tex_surface[index]->GetMagFilter();
	return -1;
}

int32_t PlaneObj::GetWrapS(uint32_t index)
{
	if (index < 4 && m_tex_surface[index].get() != NULL)
		return m_tex_surface[index]->GetWrapS();
	return -1;
}

int32_t PlaneObj::GetWrapT(uint32_t index)
{
	if (index < 4 && m_tex_surface[index].get() != NULL)
		return m_tex_surface[index]->GetWrapT();
	return -1;
}

void PlaneObj::SetAlign(AlignMode ax, AlignMode ay)
{
	if (m_align[0] != ax || m_align[1] != ay) {
		m_align[0] = ax;
		m_align[1] = ay;
		UpdateDirty();
		m_plane_flags.m_layout_dirty = true;
	}
}

void PlaneObj::SetScaleMode(ScaleMode sx, ScaleMode sy)
{
	if (m_scale_mode[0] != sx || m_scale_mode[1] != sy) {
		m_scale_mode[0] = sx;
		m_scale_mode[1] = sy;
		UpdateDirty();
		m_plane_flags.m_layout_dirty = true;
		m_plane_flags.m_position_dirty = true;
	}
}

void PlaneObj::SetScaleValue(const math::v3 &scale)
{
	if (!(scale == m_scale_value)) {
		m_scale_value = scale;
		UpdateDirty();
		m_plane_flags.m_layout_dirty = true;
		m_plane_flags.m_position_dirty = true;
	}
}

void PlaneObj::SetDivBorderScale(float scale)
{
	if (scale != m_div_border_scale) {
		m_div_border_scale = scale;
		m_plane_flags.m_position_dirty = true;
	}
}

void PlaneObj::SetDivCenterPos(float pos)
{
	if (pos != m_div_center_pos) {
		m_div_center_pos = pos;
		m_plane_flags.m_position_dirty = true;
	}
}

/* the size a box of box_size fits the texture's inside (its size less the margins) to, keeping
   its aspect, on the axis; and the size of the whole texture at that scale (name ours) */
static inline float FitAspect(uint32_t axis, math::v2 box_size, math::v2 tex_size,
			      math::v2 margins, float *extent)
{
	float width = box_size.getImpl().getX();
	float height = box_size.getImpl().getY();

	if (width <= 0.0f || height <= 0.0f)
		return 0.0f;

	math::v2 inside = tex_size - margins;
	float aspect = inside.getImpl().getX() / inside.getImpl().getY();
	math::v2 ratio(divPerElem(inside.getImpl(), box_size.getImpl()));
	float size;

	if (ratio.extract_x() > ratio.extract_y())
		size = axis == 0 ? width : width / aspect;
	else
		size = axis == 1 ? height : height * aspect;

	*extent = size / (inside.getImpl().getElem(axis) / tex_size.getImpl().getElem(axis));
	return size;
}

/* the plane's size and position on the axis from the scale mode and the align: the size of the
   texture's inside (its size less its margins) the plane is drawn at, and the size of the whole
   texture, which the margins hang over */
void PlaneObj::UpdateLayoutAxis(uint32_t axis, math::v2 tex_size, math::v4 margin)
{
	math::v2 margins(margin.extract_x() + margin.extract_z(),
			 margin.extract_y() + margin.extract_w());
	float size = 0.0f;
	float extent = 0.0f;

	switch (m_scale_mode[axis]) {
	case SCALE_SIZE: {
		float tex = (float)tex_size.getImpl().getElem(axis);
		float margin_sum = margins.extract(axis);
		float scale = m_scale_value.extract(axis);
		float box = m_size.extract(axis);
		float inside = tex - margin_sum;

		if (!(inside <= 0.0f)) {
			size = box * scale;
			extent = scale * (box / (inside / tex));
		}
		break;
	}
	case SCALE_SURFACE: {
		float tex = (float)tex_size.getImpl().getElem(axis);
		float scale = m_scale_value.extract(axis);
		float inside = tex - margins.extract(axis);

		if (!(inside <= 0.0f)) {
			extent = scale * tex;
			size = scale * inside;
		}
		break;
	}
	case SCALE_VALUE: {
		float tex = (float)tex_size.getImpl().getElem(axis);
		float value = m_scale_value.extract(axis);
		float inside = tex - margins.extract(axis);

		if (!(inside <= 0.0f)) {
			size = value;
			extent = value / (inside / tex);
		}
		break;
	}
	case SCALE_ASPECT_SIZE:
		size = FitAspect(axis, math::v2(m_size.extract_x(), m_size.extract_y()), tex_size,
				 margins, &extent);
		break;
	case SCALE_ASPECT_VALUE:
		size = FitAspect(axis, math::v2(m_scale_value.extract_x(), m_scale_value.extract_y()),
				 tex_size, margins, &extent);
		break;
	case SCALE_REGION: {
		float tex = (float)tex_size.getImpl().getElem(axis);

		if (m_tex_region != NULL)
			tex = m_tex_region->extract(axis + 2) * tex;

		float scale = m_scale_value.extract(axis);
		float inside = tex - margins.extract(axis);

		if (!(inside <= 0.0f)) {
			extent = scale * tex;
			size = scale * inside;
		}
		break;
	}
	case SCALE_REGION_ASPECT_SIZE: {
		/* the region's size; the shift by the margins below keeps the whole texture's */
		math::v2 region_size = tex_size;

		if (m_tex_region != NULL)
			region_size = math::v2(m_tex_region->extract_z(), m_tex_region->extract_w()) *
				      tex_size;
		size = FitAspect(axis, math::v2(m_size.extract_x(), m_size.extract_y()), region_size,
				 margins, &extent);
		break;
	}
	case SCALE_REGION_ASPECT_VALUE: {
		math::v2 region_size = tex_size;

		if (m_tex_region != NULL)
			region_size = math::v2(m_tex_region->extract_z(), m_tex_region->extract_w()) *
				      tex_size;
		size = FitAspect(axis, math::v2(m_scale_value.extract_x(), m_scale_value.extract_y()),
				 region_size, margins, &extent);
		break;
	}
	default:
		break;
	}

	m_plane_size.set(axis, extent);

	float half = size * 0.5f;
	float pos = 0.0f;

	switch (m_align[axis]) {
	case ALIGN_MINUS:
		pos = m_size.extract(axis) * -0.5f + size * 0.5f;
		break;
	case ALIGN_PLUS:
		pos = m_size.extract(axis) * 0.5f - size * 0.5f;
		break;
	default:
		break;
	}
	if (size != 0.0f)
		pos -= (extent / (float)tex_size.getImpl().getElem(axis)) *
		       ((margin.extract(axis) - margin.extract(axis + 2)) * 0.5f);
	pos = m_pos.extract(axis) + pos;
	m_plane_pos.set(axis, pos);
	m_bounds_min.set(axis, pos - half);
	m_bounds_max.set(axis, half + pos);
}

/* without a texture: the size by the scale mode SCALE_SIZE or SCALE_VALUE, none by the others */
void PlaneObj::UpdateLayoutAxis(uint32_t axis)
{
	float size = 0.0f;

	switch (m_scale_mode[axis]) {
	case SCALE_SIZE:
		size = m_scale_value.extract(axis) * m_size.extract(axis);
		break;
	case SCALE_VALUE:
		size = m_scale_value.extract(axis);
		break;
	default:
		break;
	}

	m_plane_size.set(axis, size);

	float half = size * 0.5f;
	float pos = 0.0f;

	switch (m_align[axis]) {
	case ALIGN_MINUS:
		pos = m_size.extract(axis) * -0.5f + size * 0.5f;
		break;
	case ALIGN_PLUS:
		pos = m_size.extract(axis) * 0.5f - size * 0.5f;
		break;
	default:
		break;
	}
	pos = m_pos.extract(axis) + pos;
	m_plane_pos.set(axis, pos);
	m_bounds_min.set(axis, pos - half);
	m_bounds_max.set(axis, half + pos);
}

/* a divided texture's margins stay outside the plane on the axes its division cuts: the plane
   grows by them, off the middle by half their difference (across, turned with the object) */
void PlaneObj::UpdateLayout()
{
	Surface *texture = m_tex_surface[0].get();

	if (texture != NULL) {
		math::v2 margin_lb((float)texture->GetMarginLeft(), (float)texture->GetMarginBottom());
		math::v2 margin_rt((float)texture->GetMarginRight(), (float)texture->GetMarginTop());
		math::v2 tex_size((float)texture->GetWidth(), (float)texture->GetHeight());
		uint32_t mode = m_divide != NULL ? m_divide->m_div_mode : texture->GetDivideMode();

		if (mode < 2 || mode == 8) {
			UpdateLayoutAxis(0, tex_size, math::v4(margin_lb.extract_x(), margin_lb.extract_y(), margin_rt.extract_x(),
							  margin_rt.extract_y()));
		} else {
			UpdateLayoutAxis(0, tex_size, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			m_plane_size.set_x(m_plane_size.extract_x() + margin_lb.extract_x() +
					   margin_rt.extract_x());

			float shift = (margin_rt.extract_x() - margin_lb.extract_x()) * 0.5f;

			if (m_rot != NULL)
				shift = shift * sce_paf_cosf(m_rot->extract_y());
			m_plane_pos.set_x(m_plane_pos.extract_x() + shift);
		}
		if (mode != 0 && mode != 2) {
			UpdateLayoutAxis(1, tex_size, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			m_plane_size.set_y(m_plane_size.extract_y() + margin_lb.extract_y() +
					   margin_rt.extract_y());
			m_plane_pos.set_y((margin_rt.getImpl().getY() - margin_lb.getImpl().getY()) * 0.5f +
					  m_plane_pos.getImpl().getY());
		} else {
			UpdateLayoutAxis(1, tex_size, math::v4(margin_lb.extract_x(), margin_lb.extract_y(), margin_rt.extract_x(),
							  margin_rt.extract_y()));
		}
	} else {
		UpdateLayoutAxis(0);
		UpdateLayoutAxis(1);
	}
	m_plane_size.set_z(1.0f);
	m_plane_pos.set_z(m_pos.extract_z());
}

/* the borders' share of the plane across or down: the division's ratios of the texture's
   size scaled by the border scale, at most the whole plane (name ours) */
static inline math::v2 DivBorders(math::v2 ratio, float tex, float scale, float plane)
{
	math::v2 borders(0.0f, 0.0f);

	if (plane > 1.0f) {
		borders = math::v2(ratio * (scale * tex) / plane);
		if (borders.extract_x() + borders.extract_y() > 1.0f)
			borders = borders / (borders.extract_x() + borders.extract_y());
	}
	return borders;
}

/* the 2 to 6 columns and rows of the mesh's vertices: the borders keep their share, the modes 4
   and 5 split the middle down or across around a stretching part, moved off the middle by the
   center position as far as there is room */
void PlaneObj::UpdateDivVerticesPosition()
{
	if (m_div_vertices == NULL || m_tex_surface[0].get() == NULL)
		return;

	Surface *texture = m_tex_surface[0].get();
	uint32_t mode;
	math::v2 ratio_h;
	math::v2 ratio_v;

	if (m_divide != NULL) {
		mode = m_divide->m_div_mode;
		ratio_h = m_divide->m_div_border_ratio_h;
		ratio_v = m_divide->m_div_border_ratio_v;
	} else {
		mode = texture->GetDivideMode();
		ScePafGraphics_AFAE7807(texture, &ratio_h, &ratio_v);
	}
	if (m_div_vertices == NULL)
		return;

	PlaneVertex *vertex = (PlaneVertex *)m_div_vertices->m_buffer->m_memory->m_addr;

	if (mode == 4 || mode == 5) {
		math::v2 stretch;

		if (m_divide != NULL) {
			stretch = m_divide->m_div_ratio_stretch;
		} else if (m_tex_surface[0].get() != NULL) {
			m_tex_surface[0]->GetDivideStretchRatio(stretch);
		} else {
			stretch = math::v2(0.0f, 0.0f);
		}

		math::v2 plane(m_plane_size.extract_x(), m_plane_size.extract_y());
		float width = (float)m_tex_surface[0]->GetWidth();
		math::v2 borders_h = DivBorders(ratio_h, width, m_div_border_scale, plane.extract_x());
		float xs[6];
		uint32_t columns = 4;

		xs[0] = -0.5f;
		xs[1] = borders_h.extract_x() + -0.5f;
		xs[2] = 0.5f - borders_h.extract_y();
		xs[3] = 0.5f;
		if (mode == 5) {
			columns = 6;
			for (float *src = &xs[2], *dst = &xs[4]; src != &xs[4];)
				*dst++ = *src++;

			float room = 1.0f;
			math::v2 part(0.0f, 0.0f);

			if (plane.extract_x() > 1.0f) {
				part = math::v2(stretch * width / plane.extract_x());
				room = 0.0f;
				if (1.0f - (borders_h.extract_x() + borders_h.extract_y()) > 0.0f)
					room = 1.0f - (borders_h.extract_x() + borders_h.extract_y());
				if (room < part.extract_x() + part.extract_y())
					part = part * (room / (part.extract_x() + part.extract_y()));
			}
			xs[2] = xs[1] + part.extract_x();
			xs[3] = xs[4] - part.extract_y();
			if (plane.extract_x() > 1.0f) {
				float rest = 0.0f;
				float fixed = (width * ((1.0f - (ratio_h.extract_x() + ratio_h.extract_y())) -
							(stretch.extract_x() + stretch.extract_y()))) /
					      plane.extract_x();

				if (fixed > 0.0f)
					rest = fixed;

				float slack = room - ((part.extract_x() + part.extract_y()) + rest);

				if (slack > 0.0f) {
					float half = slack * 0.5f;
					float center = m_div_center_pos / plane.extract_x();

					if (-half > center)
						center = -half;

					float shift = half;

					if (half > center)
						shift = center;
					xs[2] = (shift + half) + xs[2];
					xs[3] = xs[3] - (half - shift);
				}
			}
		}

		float ys[6];
		uint32_t last_row = 3;

		ys[0] = 0.5f;
		ys[1] = 0.5f;
		ys[2] = -0.5f;
		ys[3] = -0.5f;
		if (m_tex_surface[0].get() != NULL) {
			float height = (float)m_tex_surface[0]->GetHeight();
			math::v2 borders_v = DivBorders(ratio_v, height, m_div_border_scale,
							plane.extract_y());

			ys[1] = 0.5f - borders_v.extract_x();
			ys[2] = borders_v.extract_y() + -0.5f;
			if (mode == 4) {
				last_row = 5;
				for (float *src = &ys[2], *dst = &ys[4]; src != &ys[4];)
					*dst++ = *src++;

				float room = 1.0f;
				math::v2 part(0.0f, 0.0f);

				if (plane.extract_y() > 1.0f) {
					part = math::v2(stretch * height / plane.extract_y());
					room = 0.0f;
					if (1.0f - (borders_v.extract_x() + borders_v.extract_y()) > 0.0f)
						room = 1.0f - (borders_v.extract_x() + borders_v.extract_y());
					if (room < part.extract_x() + part.extract_y())
						part = part * (room / (part.extract_x() + part.extract_y()));
				}
				ys[2] = ys[1] - part.extract_x();
				ys[3] = ys[4] + part.extract_y();
				if (plane.extract_y() > 1.0f) {
					float rest = 0.0f;
					float fixed = (height * ((1.0f - (ratio_v.extract_x() +
									  ratio_v.extract_y())) -
								 (stretch.extract_x() + stretch.extract_y()))) /
						      plane.extract_y();

					if (fixed > 0.0f)
						rest = fixed;

					float slack = room - ((part.extract_x() + part.extract_y()) + rest);

					if (slack > 0.0f) {
						float center = m_div_center_pos / plane.extract_y();
						float half = slack * 0.5f;

						if (-half > center)
							center = -half;

						float shift = half;

						if (half > center)
							shift = center;
						ys[2] = ys[2] - (half - shift);
						ys[3] = ys[3] + (shift + half);
					}
				}
			}
		}

		uint32_t first = 0;

		for (uint32_t row = 0; row <= last_row; row++) {
			PlaneVertex *v = &vertex[first];

			for (uint32_t column = 0; column < columns; column++) {
				v->x = xs[column];
				v->y = ys[row];
				v++;
			}
			first += columns;
		}
	} else {
		math::v2 borders_h(0.0f, 0.0f);
		math::v2 borders_v(0.0f, 0.0f);

		if (mode != 1 && mode != 8)
			borders_h = DivBorders(ratio_h, (float)texture->GetWidth(), m_div_border_scale,
					       m_plane_size.extract_x());
		if (mode != 2 && mode != 9)
			borders_v = DivBorders(ratio_v, (float)texture->GetHeight(), m_div_border_scale,
					       m_plane_size.extract_y());

		float xs[4];
		float ys[4];
		uint32_t columns = 2;
		uint32_t rows = 2;

		xs[0] = -0.5f;
		xs[1] = 0.5f;
		xs[2] = 0.5f - borders_h.extract_y();
		xs[3] = 0.5f;
		ys[0] = 0.5f;
		ys[1] = -0.5f;
		ys[2] = borders_v.extract_y() + -0.5f;
		ys[3] = -0.5f;
		if (mode != 1 && mode != 8) {
			columns = 4;
			xs[1] = borders_h.extract_x() + -0.5f;
		}
		if (mode != 2 && mode != 9) {
			rows = 4;
			ys[1] = 0.5f - borders_v.extract_x();
		}

		uint32_t first = 0;

		for (uint32_t row = 0; row < rows; row++) {
			PlaneVertex *v = &vertex[first];

			for (uint32_t column = 0; column < columns; column++) {
				v->y = ys[row];
				v->x = xs[column];
				v++;
			}
			first += columns;
		}
	}
}

/* the texture coordinates of the mesh's columns and rows: the borders' ratios, and for the
   modes 4 and 5 the stretching part after the first border */
void PlaneObj::UpdateDivVerticesTexcoord()
{
	if (m_div_vertices == NULL || m_tex_surface[0].get() == NULL)
		return;

	uint32_t mode;
	math::v2 ratio_h;
	math::v2 ratio_v;

	if (m_divide != NULL) {
		mode = m_divide->m_div_mode;
		ratio_h = m_divide->m_div_border_ratio_h;
		ratio_v = m_divide->m_div_border_ratio_v;
	} else {
		mode = m_tex_surface[0]->GetDivideMode();
		ScePafGraphics_AFAE7807(m_tex_surface[0].get(), &ratio_h, &ratio_v);
	}
	if (m_div_vertices == NULL)
		return;

	PlaneVertex *vertex = (PlaneVertex *)m_div_vertices->m_buffer->m_memory->m_addr;

	if (mode == 4 || mode == 5) {
		math::v2 stretch;

		if (m_divide != NULL) {
			stretch = m_divide->m_div_ratio_stretch;
		} else if (m_tex_surface[0].get() != NULL) {
			m_tex_surface[0]->GetDivideStretchRatio(stretch);
		} else {
			stretch = math::v2(0.0f, 0.0f);
		}

		float us[6];
		float vs[6];
		uint32_t columns = 4;
		uint32_t rows = 4;

		us[0] = 0.0f;
		us[1] = ratio_h.extract_x();
		us[2] = 1.0f - ratio_h.extract_y();
		us[3] = 1.0f;
		if (mode == 5) {
			for (float *src = &us[2], *dst = &us[4]; src != &us[4];)
				*dst++ = *src++;
			us[2] = us[1] + stretch.extract_x();
			us[3] = us[4] - stretch.extract_y();
		}
		vs[0] = 0.0f;
		vs[1] = ratio_v.extract_x();
		vs[2] = 1.0f - ratio_v.extract_y();
		vs[3] = 1.0f;
		if (mode == 4) {
			for (float *src = &vs[2], *dst = &vs[4]; src != &vs[4];)
				*dst++ = *src++;
			vs[2] = vs[1] + stretch.extract_x();
			vs[3] = vs[4] - stretch.extract_y();
		}
		if (mode == 5)
			columns = 6;
		if (mode == 4)
			rows = 6;

		uint32_t first = 0;

		for (uint32_t row = 0; row < rows; row++) {
			PlaneVertex *v = &vertex[first];

			for (uint32_t column = 0; column < columns; column++) {
				v->u = us[column];
				v->v = vs[row];
				v++;
			}
			first += columns;
		}
	} else {
		float us[4];
		float vs[4];
		uint32_t columns = 2;
		uint32_t rows = 2;

		us[0] = 0.0f;
		us[1] = 1.0f;
		us[2] = 1.0f;
		us[3] = 1.0f;
		if (mode != 1 && mode != 8) {
			columns = 4;
			us[1] = ratio_h.extract_x();
			us[2] = 1.0f - ratio_h.extract_y();
		}
		vs[0] = 0.0f;
		vs[1] = 1.0f;
		vs[2] = 1.0f;
		vs[3] = 1.0f;
		if (mode != 2 && mode != 9) {
			rows = 4;
			vs[1] = ratio_v.extract_x();
			vs[2] = 1.0f - ratio_v.extract_y();
		}

		uint32_t first = 0;

		for (uint32_t row = 0; row < rows; row++) {
			PlaneVertex *v = &vertex[first];

			for (uint32_t column = 0; column < columns; column++) {
				v->v = vs[row];
				v->u = us[column];
				v++;
			}
			first += columns;
		}
	}
}

const char PlaneObjParam::m_param_name[] = "planeobj";

const char *PlaneObjParam::GetParamName() const
{
	return m_param_name;
}

const char *PlaneObjParam::ParamName()
{
	return m_param_name;
}

PlaneObjParam::PlaneObjParam()
	: m_pos(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_size(sce::Vectormath::Simd::Aos::Vector3(0.0f)), m_tex_region(0.0f, 0.0f, 1.0f, 1.0f),
	  m_scale_value(sce::Vectormath::Simd::Aos::Vector3(1.0f)), m_align_x(PlaneObj::ALIGN_CENTER),
	  m_align_y(PlaneObj::ALIGN_CENTER), m_scale_mode_x(PlaneObj::SCALE_SIZE),
	  m_scale_mode_y(PlaneObj::SCALE_SIZE), m_div_center_pos(0.0f), m_div_border_scale(1.0f)
{
}

PlaneObjParam::~PlaneObjParam()
{
}

int32_t PlaneObjParam::Create(int32_t mode)
{
	int32_t ret = DrawObjParam::Create(mode);

	if (ret != 0)
		return ret;

	if (m_serial != NULL)
		m_serial->AddParam("texture0", &m_texture[0], ResourceParam::SEMANTIC_TEXTUREREF);
	if (m_serial != NULL)
		m_serial->AddParam("texture1", &m_texture[1], ResourceParam::SEMANTIC_TEXTUREREF);
	if (m_serial != NULL)
		m_serial->AddParam("texture2", &m_texture[2], ResourceParam::SEMANTIC_TEXTUREREF);
	if (m_serial != NULL)
		m_serial->AddParam("texture3", &m_texture[3], ResourceParam::SEMANTIC_TEXTUREREF);
	if (m_serial != NULL)
		m_serial->AddParam("pos", &m_pos, ResourceParam::SEMANTIC_VEC3);
	if (m_serial != NULL)
		m_serial->AddParam("size", &m_size, ResourceParam::SEMANTIC_VEC3);
	if (m_serial != NULL)
		m_serial->AddParam("tex_region", &m_tex_region, ResourceParam::SEMANTIC_VEC4);
	if (m_serial != NULL)
		m_serial->AddParam("align_x", &m_align_x, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("align_y", &m_align_y, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("scale_mode_x", &m_scale_mode_x, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("scale_mode_y", &m_scale_mode_y, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("scale_value", &m_scale_value, ResourceParam::SEMANTIC_VEC3);
	if (m_serial != NULL)
		m_serial->AddParam("div_center_pos", &m_div_center_pos,
				   ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("div_border_scale", &m_div_border_scale,
				   ResourceParam::SEMANTIC_FLOAT);
	return ret;
}

/* the textures by their IDs, the position, the size and a region other than the whole texture
   through the plane's setters, the rest written in */
int32_t PlaneObjParam::SetEach(DrawObj *obj)
{
	int32_t ret = DrawObjParam::SetEach(obj);

	if (ret != 0)
		return ret;

	PlaneObj *plane = (PlaneObj *)obj;

	if (m_plugin != NULL) {
		for (uint32_t i = 0; i < 4; i++) {
			if (m_texture[i].GetIDHash() != 0)
				plane->SetTexture(m_plugin->GetTexture(m_texture[i]), i);
		}
	}
	plane->SetPos(m_pos);
	plane->SetSize(m_size);
	if (!(m_tex_region == math::v4(0.0f, 0.0f, 1.0f, 1.0f)))
		plane->SetTexRegion(m_tex_region);
	plane->m_align[0] = m_align_x;
	plane->m_align[1] = m_align_y;
	plane->m_scale_mode[0] = m_scale_mode_x;
	plane->m_scale_mode[1] = m_scale_mode_y;
	plane->m_scale_value = m_scale_value;
	plane->m_div_center_pos = m_div_center_pos;
	plane->m_div_border_scale = m_div_border_scale;
	plane->UpdateDirty();
	plane->m_plane_flags.m_layout_dirty = true;
	plane->m_plane_flags.m_position_dirty = true;
	return ret;
}

int32_t PlaneObjParam::GetEach(DrawObj *obj)
{
	int32_t ret = DrawObjParam::GetEach(obj);

	if (ret != 0)
		return ret;

	PlaneObj *plane = (PlaneObj *)obj;

	m_pos = plane->m_pos;
	m_size = plane->m_size;
	m_tex_region = plane->m_tex_region != NULL ? *plane->m_tex_region :
						       math::v4(0.0f, 0.0f, 1.0f, 1.0f);
	m_align_x = plane->m_align[0];
	m_align_y = plane->m_align[1];
	m_scale_value = plane->m_scale_value;
	m_scale_mode_x = plane->m_scale_mode[0];
	m_scale_mode_y = plane->m_scale_mode[1];
	m_div_border_scale = plane->m_div_border_scale;
	m_div_center_pos = plane->m_div_center_pos;
	return ret;
}

}
}
