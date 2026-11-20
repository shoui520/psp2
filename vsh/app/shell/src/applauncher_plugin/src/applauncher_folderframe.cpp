/*
 * applauncher_folderframe.cpp: FolderFrameDrawObj, the ring drawn round a folder page's name
 * (0x810D808C-0x810D8ACC). A plane's draw object cut down to one texture on a strip of 26
 * vertices round a circle, shared by every ring. The type name is Sony's; the file's name and
 * the other names here are ours, after PlaneObj's.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include "applauncher_folderframe.h"

using namespace paf;
using namespace paf::graph;
using arl::ArlInstance;
using arl::VertexArray;
using arl::VertexArraySpec;

/* a vertex of the ring: its position and texture coordinates (names ours) */
struct FolderFrameVertex {
	float x;
	float y;
	float z;
	float u;
	float v;
};

const char FolderFrameDrawObj::m_drawobj_type[] = "FolderFrameDrawObj";

/* the style rings are made with (blend 1, depth 4), as its word */
static const uint32_t s_folderFrameStyle = 0x41;

/* the ring, a triangle strip of the outer and the inner edge in turn every 30 degrees from the
   bottom, the texture across it and round it */
static const FolderFrameVertex s_folderFrameVertexData[26] = {
	{ 0.0f, -0.5f, 0.0f, 0.0393931232f, 0.0f },
	{ 0.0f, -0.442307681f, 0.0f, 0.0925400704f, 1.0f },
	{ 0.25f, -0.433012694f, 0.0f, 0.960606873f, 0.0f },
	{ 0.22115384f, -0.383049697f, 0.0f, 0.907459915f, 1.0f },
	{ 0.433012694f, -0.25f, 0.0f, 0.0393931232f, 0.0f },
	{ 0.383049697f, -0.22115384f, 0.0f, 0.0925400704f, 1.0f },
	{ 0.5f, 0.0f, 0.0f, 0.960606873f, 0.0f },
	{ 0.442307681f, 0.0f, 0.0f, 0.907459915f, 1.0f },
	{ 0.433012694f, 0.25f, 0.0f, 0.0393931232f, 0.0f },
	{ 0.383049697f, 0.22115384f, 0.0f, 0.0925400704f, 1.0f },
	{ 0.25f, 0.433012694f, 0.0f, 0.960606873f, 0.0f },
	{ 0.22115384f, 0.383049697f, 0.0f, 0.907459915f, 1.0f },
	{ 0.0f, 0.5f, 0.0f, 0.0393931232f, 0.0f },
	{ 0.0f, 0.442307681f, 0.0f, 0.0925400704f, 1.0f },
	{ -0.25f, 0.433012694f, 0.0f, 0.960606873f, 0.0f },
	{ -0.22115384f, 0.383049697f, 0.0f, 0.907459915f, 1.0f },
	{ -0.433012694f, 0.25f, 0.0f, 0.0393931232f, 0.0f },
	{ -0.383049697f, 0.22115384f, 0.0f, 0.0925400704f, 1.0f },
	{ -0.5f, 0.0f, 0.0f, 0.960606873f, 0.0f },
	{ -0.442307681f, 0.0f, 0.0f, 0.907459915f, 1.0f },
	{ -0.433012694f, -0.25f, 0.0f, 0.0393931232f, 0.0f },
	{ -0.383049697f, -0.22115384f, 0.0f, 0.0925400704f, 1.0f },
	{ -0.25f, -0.433012694f, 0.0f, 0.960606873f, 0.0f },
	{ -0.22115384f, -0.383049697f, 0.0f, 0.907459915f, 1.0f },
	{ 0.0f, -0.5f, 0.0f, 0.0393931232f, 0.0f },
	{ 0.0f, -0.442307681f, 0.0f, 0.0925400704f, 1.0f },
};

#define FOLDER_FRAME_VERTEX_COUNT	26

/* the ring's vertices, made for the first ring and kept (0x815443D4) */
static VertexArray *s_folderFrameVertices;

bool FolderFrameDrawObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return DrawObj::IsInherit(type);
}

const char *FolderFrameDrawObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

/* the ring's vertices made, the first time (0x810D80B0) */
static __attribute__((noinline)) void folderFrameCreateVertices(void)
{
	ArlInstance *gl = arl::Gl();

	if (s_folderFrameVertices == NULL)
		s_folderFrameVertices = new VertexArray(gl->m_default_allocator,
							ScePafGraphics_4D1E289A(gl,
								VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2,
								SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
							s_folderFrameVertexData,
							FOLDER_FRAME_VERTEX_COUNT);
}

FolderFrameDrawObj::FolderFrameDrawObj()
	: DrawObj(reinterpret_cast<const DrawStyle &>(s_folderFrameStyle)),
	  m_plane_size(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_plane_pos(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_scale_value(sce::Vectormath::Simd::Aos::Vector3(1.0f)),
	  m_bounds_min(sce::Vectormath::Simd::Aos::Vector3(0.0f)),
	  m_bounds_max(sce::Vectormath::Simd::Aos::Vector3(0.0f))
{
	m_layout_dirty = true;
	folderFrameCreateVertices();
}

FolderFrameDrawObj::~FolderFrameDrawObj()
{
}

void FolderFrameDrawObj::Update()
{
	if (m_layout_dirty) {
		UpdateLayout();
		m_layout_dirty = false;
	}
}

void FolderFrameDrawObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha,
				DrawInfo &info)
{
	if (!m_flags.m_visible)
		return;

	ScePafGraphics_B7EB9246(this, &color, &alpha);
	if (!(alpha > 0.0f))
		return;

	if (m_layout_dirty) {
		UpdateLayout();
		m_layout_dirty = false;
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

void FolderFrameDrawObj::DrawCallback(const DrawCallbackArg &arg)
{
	arg.self->Draw(*arg.world_mat, arg.color, arg.alpha);
}

/* the strip in the scene's colour times the ring's, with its one texture */
void FolderFrameDrawObj::Draw(const math::matrix &world, uint32_t color, float alpha)
{
	BindDrawStyle();

	ArlInstance *gl = arl::Gl();
	math::matrix world_mat;
	PlaneObjDrawParams params(&m_texture, NULL);

	params.m_num_textures = 1;
	CalcWorldMatrix(world_mat, world, false);
	params.m_vertices = s_folderFrameVertices;
	params.m_count = FOLDER_FRAME_VERTEX_COUNT;
	params.m_instances = 1;

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));
	math::v4 draw_color(sce::Vectormath::Simd::Aos::Vector4(vmulq_f32(
		vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));

	draw_color.set_w(alpha);
	gl->Color(draw_color);
	gl->m_model.Load(world_mat);
	m_shader->Draw(params);
}

void FolderFrameDrawObj::ReleaseAll()
{
	for (int32_t i = 0; i < 1; i++)
		m_texture.clear();
}

void FolderFrameDrawObj::SetTexture(const intrusive_ptr<Surface> &texture, uint32_t index)
{
	if (index == 0 && m_texture.get() != texture.get())
		m_texture = texture.get();
}

void FolderFrameDrawObj::GetTexture(intrusive_ptr<Surface> &texture, uint32_t index)
{
	if (index == 0 && m_texture.get() != texture.get())
		texture = m_texture.get();
}

void FolderFrameDrawObj::SetPos(math::v3 pos)
{
	if (!(pos == m_pos)) {
		m_pos = pos;
		UpdateDirty();
		m_layout_dirty = true;
	}
}

void FolderFrameDrawObj::SetSize(math::v3 size)
{
	if (!(size == m_size)) {
		m_size = math::v3(sce::Vectormath::Simd::Aos::maxPerElem(
			sce::Vectormath::Simd::Aos::Vector3(0.0f), size.getImpl()));
		UpdateDirty();
		m_layout_dirty = true;
	}
}

/* DrawObj's, with the ring's size and position in the object */
void FolderFrameDrawObj::CalcWorldMatrix(math::matrix &world_mat, const math::matrix &parent,
					 bool unscaled)
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

/* the ring the scale times the object's size, at the object's position, its box round it */
void FolderFrameDrawObj::UpdateLayout()
{
	sce::Vectormath::Simd::Aos::Vector4 size(
		mulPerElem(m_scale_value.getImpl(), m_size.getImpl()), 1.0f);

	m_plane_pos = m_pos;
	m_plane_size = math::v3(size.getXYZ());
	m_bounds_min = math::v3((size * -0.5f).getXYZ());
	m_bounds_max = math::v3((size * 0.5f).getXYZ());
}
