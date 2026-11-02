/*
 * paf::graph::DrawObj: the draw objects' base, the draw style bound for them, and the draw
 * calls and renderer the draw objects are drawn by (the file's name after ScePaf-RE's
 * draw_obj.h).
 */
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/gutil.h>
#include <math/ngp/quaternion.h>

namespace paf {
namespace graph {

using namespace arl;

const char DrawObj::m_drawobj_type[] = "DrawObj";
bool DrawObj::s_safety;
uint32_t DrawObj::s_dirty_count = 1;

/* the style bound last, which the next bind with the same style skips (name ours) */
static DrawStyle s_draw_style(0);
static common::SharedPtr<DrawEnv> s_draw_calls;
static GlobalPtr<Renderer> s_renderer(NULL);
GlobalPtr<DrawLayer> g_scene_draw_layer(NULL);
GlobalPtr<DrawLayer> g_scene_draw_layer2(NULL);

bool DrawObj::IsInherit(const char *type) const
{
	return type == m_drawobj_type;
}

const char *DrawObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

const char *DrawObj::DrawObjType()
{
	return m_drawobj_type;
}

DrawObj::DrawObj(const DrawStyle &style)
	: m_pos(0.0f, 0.0f, 0.0f), m_size(0.0f, 0.0f, 0.0f), m_rot(NULL), m_color(0xFFFFFFFF),
	  m_draw_style(style), m_shader(*gutil::GetDefaultShader())
{
	m_delete_cb[0].func = NULL;
	m_delete_cb[0].data = NULL;
	m_delete_cb[1].func = NULL;
	m_delete_cb[1].data = NULL;
}

DrawObj::~DrawObj()
{
	if (m_delete_cb[0].func != NULL)
		m_delete_cb[0].func(this, m_delete_cb[0].data);
	if (m_delete_cb[1].func != NULL)
		m_delete_cb[1].func(this, m_delete_cb[1].data);
	if (m_rot != NULL)
		delete m_rot;
}

}
}

using namespace paf;
using namespace paf::graph;

/* the color modulated by the draw object's: each of red, green and blue scaled by the
   object's, alpha made opaque and the object's alpha given to the alpha (names ours) */
void ScePafGraphics_B7EB9246(const DrawObj *obj, uint32_t *color, float *alpha)
{
	uint32_t modulate = obj->m_color;

	if (modulate == 0xFFFFFFFF)
		return;

	uint32_t src = *color;
	uint32_t g = ((src >> 8) & 0xFF) * ((modulate >> 8) & 0xFF) + ((modulate >> 8) & 0xFF) +
		     ((src >> 8) & 0xFF);
	uint32_t r = (src & 0xFF) * (modulate & 0xFF) + (modulate & 0xFF) + (src & 0xFF);
	uint32_t b = ((src >> 16) & 0xFF) * ((modulate >> 16) & 0xFF) + ((modulate >> 16) & 0xFF) +
		     ((src >> 16) & 0xFF);

	*color = ((uint32_t)(uint8_t)(g >> 8) << 8) | ((uint32_t)(uint8_t)(b >> 8) << 16) | (r >> 8) |
		 0xFF000000;
	if ((obj->m_color & 0xFF000000) != 0xFF000000)
		*alpha *= (float)(obj->m_color >> 24) / 255.0f;
}

namespace paf {
namespace graph {

void DrawObj::EnterSafety()
{
	s_safety = true;
}

void DrawObj::FinishSafety()
{
	s_safety = false;
}

void DrawObj::SetDirtyCount(uint32_t count)
{
	s_dirty_count = count;
}

/* each angle kept within a turn */
void DrawObj::SetRot(math::v3 rot)
{
	const float turn = 6.2831855f;

	if (m_rot == NULL)
		m_rot = new math::v3;

	float x = rot.extract_x();
	float y = rot.extract_y();
	float z = rot.extract_z();

	*m_rot = math::v3(x - (float)(int32_t)(x / turn) * turn,
			  y - (float)(int32_t)(y / turn) * turn,
			  z - (float)(int32_t)(z / turn) * turn);
	UpdateDirty();
}

/* no shader: the default shader */
void DrawObj::SetShader(const common::SharedPtr<Shader> &shader)
{
	if (shader.get() != NULL)
		m_shader = shader;
	else
		m_shader = *gutil::GetDefaultShader();
}

void DrawObj::CalcWorldMatrix(math::matrix &world_mat, const math::matrix &parent,
			      bool unscaled)
{
	using sce::Vectormath::Simd::Aos::Vector3;

	if (!unscaled) {
		if (m_rot == NULL) {
			Vector3 size = maxPerElem(Vector3(0.0f), m_size.getImpl());

			world_mat = parent * math::matrix(math::v4(size.getX(), 0.0f, 0.0f, 0.0f),
							  math::v4(0.0f, size.getY(), 0.0f, 0.0f),
							  math::v4(0.0f, 0.0f, size.getZ(), 0.0f),
							  math::v4(m_pos, 1.0f));
		} else {
			math::matrix rot = math::quaternion::FromEulerAnglesYXZ(*m_rot).toMatrix(1.0e-4f);
			Vector3 size = maxPerElem(Vector3(0.0f), m_size.getImpl());
			math::matrix local = rot * math::matrix(math::v4(size.getX(), 0.0f, 0.0f, 0.0f),
								math::v4(0.0f, size.getY(), 0.0f, 0.0f),
								math::v4(0.0f, 0.0f, size.getZ(), 0.0f),
								math::v4(0.0f, 0.0f, 0.0f, 1.0f));

			world_mat = parent * math::matrix(local.x(), local.y(), local.z(),
							  math::v4(m_pos, 1.0f));
		}
	} else {
		if (m_rot == NULL) {
			world_mat = parent * math::matrix(math::v4(1.0f, 0.0f, 0.0f, 0.0f),
							  math::v4(0.0f, 1.0f, 0.0f, 0.0f),
							  math::v4(0.0f, 0.0f, 1.0f, 0.0f),
							  math::v4(m_pos, 1.0f));
		} else {
			math::matrix rot = math::quaternion::FromEulerAnglesYXZ(*m_rot).toMatrix(1.0e-4f);

			world_mat = parent * math::matrix(rot.x(), rot.y(), rot.z(),
							  math::v4(m_pos, 1.0f));
		}
	}
}

void BindDrawStyle(DrawStyle style)
{
	ArlInstance &gl = *Gl();

	if (s_draw_style != style || gl.m_marker != "DrawObj::DefaultMarker") {
		gl.PushMarker("DrawObj::DefaultMarker");
		s_draw_style = style;

		switch (style.blend) {
		case 1:
			gl.Enable(BLEND);
			gl.BlendEquation(FUNC_ADD, FUNC_ADD);
			gl.BlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA);
			break;
		case 2:
			gl.Enable(BLEND);
			gl.BlendEquation(FUNC_ADD, FUNC_ADD);
			gl.BlendFunc(SRC_ALPHA, ONE, ZERO, ONE);
			break;
		case 3:
			gl.Enable(BLEND);
			gl.BlendEquation(FUNC_REVERSE_SUBTRACT, FUNC_ADD);
			gl.BlendFunc(SRC_ALPHA, ONE, ZERO, ONE);
			break;
		case 4:
			gl.Enable(BLEND);
			gl.BlendEquation(FUNC_ADD, FUNC_ADD);
			gl.BlendFunc(ZERO, ONE, ONE, ZERO);
			break;
		case 5:
			gl.Enable(BLEND);
			gl.BlendEquation(FUNC_ADD, FUNC_ADD);
			gl.BlendFunc(DST_ALPHA, ONE_MINUS_DST_ALPHA, ZERO, ONE);
			break;
		case 15:
			gl.Enable(BLEND);
			gl.BlendEquation(FUNC_ADD, FUNC_ADD);
			gl.BlendFunc(ONE, ONE_MINUS_SRC_ALPHA, ONE, ONE_MINUS_SRC_ALPHA);
			break;
		default:
			gl.Disable(BLEND);
			break;
		}

		switch (style.depth) {
		case 2:
			gl.DepthFunc(DEPTH_NEVER, FRONT_AND_BACK);
			break;
		case 3:
			gl.DepthFunc(DEPTH_LESS, FRONT_AND_BACK);
			break;
		case 4:
			gl.DepthFunc(DEPTH_LEQUAL, FRONT_AND_BACK);
			break;
		case 5:
			gl.DepthFunc(DEPTH_EQUAL, FRONT_AND_BACK);
			break;
		case 6:
			gl.DepthFunc(DEPTH_NOTEQUAL, FRONT_AND_BACK);
			break;
		case 7:
			gl.DepthFunc(DEPTH_GEQUAL, FRONT_AND_BACK);
			break;
		case 8:
			gl.DepthFunc(DEPTH_GREATER, FRONT_AND_BACK);
			break;
		default:
			gl.DepthFunc(DEPTH_ALWAYS, FRONT_AND_BACK);
			break;
		}

		switch (style.face_cull) {
		case 0:
			gl.CullMode(CULL_NONE);
			break;
		case 1:
			gl.CullMode(CULL_CW);
			break;
		case 2:
			gl.CullMode(CULL_CCW);
			break;
		}

		if (style.deepen_color)
			gl.Enable(DEBUG_COLOR_SHIFT);
		else
			gl.Disable(DEBUG_COLOR_SHIFT);
		if (style.reduce_transparency)
			gl.Enable(DEBUG_ALPHA_DOUBLE);
		else
			gl.Disable(DEBUG_ALPHA_DOUBLE);
		if (style.surface_draw)
			gl.Enable(DEBUG_SURFACE_DRAW);
		else
			gl.Disable(DEBUG_SURFACE_DRAW);
		gl.PopMarker();
	}
}

void DrawObj::BindDrawStyle()
{
	graph::BindDrawStyle(m_draw_style);
}

common::SharedPtr<DrawEnv> &DrawCalls()
{
	return s_draw_calls;
}

}
}

GlobalPtr<Renderer> *ScePafGraphics_4E038C05()
{
	return &s_renderer;
}

void ScePafGraphics_1C294786()
{
	s_draw_calls = common::SharedPtr<DrawEnv>(new DrawEnv());
	s_renderer.m_ptr = new Renderer(s_draw_calls);
}

void ScePafGraphics_0B4F6C24()
{
	if (s_renderer.m_ptr != NULL) {
		delete s_renderer.m_ptr;
		s_renderer.m_ptr = NULL;
	}
	s_draw_calls.reset();
}

void ScePafGraphics_505CD74E(bool flag)
{
	s_renderer.m_ptr->m_filter.m_flag_505CD74E = flag;
}
