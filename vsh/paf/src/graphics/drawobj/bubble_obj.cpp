/*
 * paf::graph::BubbleObj: the bubble draw object. Each draw puts the object's world position,
 * rotation, size and color into its parameter block and queues it with the panel renderer
 * (the file's name ours, after the class).
 */
#include <stdint.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/drawobj/bubble_obj.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/effects/panel_renderer.h>
#include <graphics/arl/arl_defines.h>
#include <math/ngp/matrix.h>
#include <graphics/surface/surface.h>
#include <math/ngp/quaternion.h>
#include "../../std/math.h"

namespace paf {
namespace graph {

const char BubbleObj::m_drawobj_type[] = "BubbleObj";

bool BubbleObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return DrawObj::IsInherit(type);
}

const char *BubbleObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

const char *BubbleObj::DrawObjType()
{
	return m_drawobj_type;
}

BubbleObj::BubbleObj(const DrawStyle &style) : DrawObj(style), m_param(NULL)
{
	m_opaque = false;
	RegisterPanelObj(this, 0);
	m_param = new BubbleParam;
}

BubbleObj::~BubbleObj()
{
	UnregisterPanelObj(this);
	delete m_param;
}

/* the screen is 480 by 272, seen through a 25-degree half angle; a bubble off the eye's
   depth is moved as the panels' perspective would move it */
void BubbleObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info)
{
	if (!m_flags.m_visible || m_param == NULL)
		return;

	ScePafGraphics_B7EB9246(this, &color, &alpha);
	if (alpha < 0.05f)
		return;

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));
	math::v4 bubble_color(sce::Vectormath::Simd::Aos::Vector4(vmulq_f32(
		vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));

	m_param->m_color = bubble_color.getImpl().get128();
	bubble_color.set_w(alpha);
	m_param->m_color = bubble_color.getImpl().get128();

	Camera *camera = ScePafGraphics_4E038C05()->m_ptr->GetPanelRenderer(false)->m_camera.m_ptr;
	math::v4 eye = camera->GetEyePos();
	math::matrix world;

	CalcWorldMatrix(world, parent, true);

	if (world.w().extract_z() != 0.0f) {
		math::v4 pos = world.w();
		static bool s_screen_distance_set;
		static float s_screen_distance;

		if (!s_screen_distance_set) {
			s_screen_distance_set = true;
			s_screen_distance = 480.0f / sce_paf_tanf(0.43633235f);
		}

		float near = (s_screen_distance - pos.extract_z()) / s_screen_distance;
		float far = (eye.extract_z() - pos.extract_z()) / eye.extract_z();

		pos.set_x(far * 480.0f * (pos.extract_x() / (near * 480.0f)));
		pos.set_y(far * 272.0f * (pos.extract_y() / (near * 272.0f)));
		world.set_w(pos);
	}

	BubbleParam *param = m_param;

	param->m_scale = sce::Vectormath::Simd::Aos::Vector4(
		world.x().len() * m_size.getImpl().getX()).get128();

	math::v4 world_pos = world.w();
	math::quaternion rot(world);

	param->m_pos = world_pos.getImpl().get128();
	param->m_rot = math::v4(sce::Vectormath::Simd::Aos::Vector4(rot.getImpl())).unit().getImpl().get128();

	if (m_opaque)
		DrawPanelObj(this, 2, 0);
	else
		DrawPanelObj(this, 0, 0);
}

/* only texture 0: the texture made from the old one is dropped */
void BubbleObj::SetTexture(const intrusive_ptr<Surface> &texture, uint32_t index)
{
	BubbleParam *param = m_param;

	if (param == NULL || index != 0)
		return;

	intrusive_ptr<Surface> surface;

	surface = texture.get();

	if (param->m_texture.m_texture.get() != NULL)
		param->m_texture.m_texture.reset();
	param->m_texture.m_surface = surface;
}

void BubbleObj::GetTexture(intrusive_ptr<Surface> &texture, uint32_t index)
{
	BubbleParam *param = m_param;

	if (param == NULL || index != 0)
		return;
	texture = param->m_texture.m_surface.get();
}

/* the bubble's size across is the object's width */
void BubbleObj::SetSize(math::v3 size)
{
	DrawObj::SetSize(size);
	if (m_param != NULL)
		m_param->m_scale = sce::Vectormath::Simd::Aos::Vector4(size.getImpl().getX()).get128();
}

}
}

using namespace paf;
using namespace paf::graph;

void ScePafGraphics_5E94DC78(BubbleObj *obj, int32_t preset)
{
	BubbleParam *param = obj->m_param;

	if (param == NULL)
		return;

	switch (preset) {
	case 1:
		param->m_refract = true;
		param->m_refract_scale = 1.087f;
		param->m_refract_offset = -0.026f;
		break;
	case 2:
		param->m_refract = true;
		param->m_refract_scale = 1.01f;
		param->m_refract_offset = -0.025f;
		break;
	default:
		param->m_refract = false;
		break;
	}
}

void ScePafGraphics_5AF8179A(BubbleObj *obj, const math::v4 &pos)
{
	BubbleParam *param = obj->m_param;

	if (param == NULL)
		return;
	float32x4_t light = pos.getImpl().get128();

	param->m_fixed_light = true;
	param->m_fixed_light_pos = light;
}

void ScePafGraphics_8454D7FB(BubbleObj *obj, bool enable, const math::v4 &pos, float range)
{
	BubbleParam *param = obj->m_param;

	if (param == NULL)
		return;
	float32x4_t light = pos.getImpl().get128();

	param->m_light_range = range;
	param->m_light = enable;
	param->m_light_pos = light;
}
