/*
 * paf::graph::PanelObj: the panel draw object. Each draw puts the object's color, world position
 * and rotation into its parameter block and queues it with the panel renderer (the file's name
 * ours, after the class).
 */
#include <stdint.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/drawobj/panel_obj.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/arl/arl_defines.h>
#include <math/ngp/matrix.h>
#include <graphics/surface/surface.h>
#include <math/ngp/quaternion.h>

namespace paf {
namespace graph {

const char PanelObj::m_drawobj_type[] = "PanelObj";

bool PanelObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return DrawObj::IsInherit(type);
}

const char *PanelObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

const char *PanelObj::DrawObjType()
{
	return m_drawobj_type;
}

/* a panel 574 by 288 and 6 deep, its edge 12 */
PanelObj::PanelObj(const DrawStyle &style) : DrawObj(style), m_param(NULL)
{
	RegisterPanelObj(this, 1);

	PanelParam *param = new PanelParam;

	m_param = param;
	if (param != NULL) {
		math::v4 rot(0.0f, 0.0f, 0.0f, 1.0f);

		param->m_pos = rot.getImpl().get128();
		param->m_rot = rot.unit().getImpl().get128();
		param->m_scale = PanelParam::PanelVec(574.0f, 288.0f, 6.0f, 0.0f);
		param->m_edge = vdup_n_f32(12.0f);
		param->m_pair_y = 0.54f;
		param->m_pair_x = 0.63f;
	}
}

PanelObj::~PanelObj()
{
	UnregisterPanelObj(this);
	delete m_param;
}

void PanelObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info)
{
	if (!m_flags.m_visible || m_param == NULL)
		return;

	ScePafGraphics_B7EB9246(this, &color, &alpha);
	if (alpha < 0.05f)
		return;

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));
	math::v4 panel_color(sce::Vectormath::Simd::Aos::Vector4(vmulq_f32(
		vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));

	m_param->m_color = panel_color.getImpl().get128();
	panel_color.set_w(alpha);
	m_param->m_color = panel_color.getImpl().get128();

	math::matrix world;

	CalcWorldMatrix(world, parent, true);

	PanelParam *param = m_param;
	math::v4 world_pos = world.w();
	math::quaternion rot(world);

	param->m_pos = world_pos.getImpl().get128();
	param->m_rot = math::v4(sce::Vectormath::Simd::Aos::Vector4(rot.getImpl())).unit().getImpl().get128();

	DrawPanelObj(this, 1, 0);
}

/* the panel's size is the object's */
void PanelObj::SetSize(math::v3 size)
{
	DrawObj::SetSize(size);
	if (m_param != NULL)
		m_param->m_scale = size.getImpl().get128();
}

}
}

using namespace paf;
using namespace paf::graph;

void ScePafGraphics_70ED91E6(PanelObj *obj, const math::v2 &value)
{
	PanelParam *param = obj->m_param;

	param->m_pair_x = value.extract_x();
	param->m_pair_y = value.extract_y();
}

void ScePafGraphics_AEBC68E5(PanelObj *obj, const math::v2 &edge)
{
	obj->m_param->m_edge = edge.getImpl().get64();
}

void ScePafGraphics_0C968320(PanelObj *obj, const math::v4 &revolution)
{
	float32x4_t value = revolution.getImpl().get128();
	PanelRevolution *param = &obj->m_param->m_revolution;

	param->m_axis[0] = vgetq_lane_f32(value, 0);
	param->m_axis[1] = vgetq_lane_f32(value, 1);
	param->m_axis[2] = vgetq_lane_f32(value, 2);
	param->m_angle = vgetq_lane_f32(value, 3);
}

namespace paf {
namespace graph {

math::v4 GetPanelRevolution(const PanelObj *obj)
{
	const PanelRevolution &revolution = obj->m_param->m_revolution;

	return math::v4(revolution.m_axis[0], revolution.m_axis[1], revolution.m_axis[2],
			revolution.m_angle);
}

}
}
