/*
 * paf::graph::PanelObj: a draw object the panel renderer draws as a glass panel, from a block of
 * parameters the object keeps up to date. Class and method names from the mangled export names;
 * the parameter block and every member name are ours.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_PANEL_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_PANEL_OBJ_H

#include <stdint.h>
#include <arm_neon.h>
#include <common/shared_ptr.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/drawobj/draw_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <std/string.h>

namespace paf {
namespace graph {

/* the panel's turn: an axis and an angle (0, 0, 1) and 13 by default, after the 0.990 build's
   draw_revolution_panel; Widget::SetObjAnim animates it (names ours) */
class PanelRevolution {
public:
	float m_axis[3];
	float m_angle;
};

/* what the panel renderer draws a panel with (0x90 bytes) */
class PanelParam {
public:
	PanelParam()
		: m_pos(PanelVec(0.0f, 0.0f, 0.0f, 1.0f)), m_rot(PanelVec(0.0f, 0.0f, 0.0f, 1.0f)),
		  m_scale(PanelVec(1.0f, 1.0f, 1.0f, 1.0f)), m_edge(vdup_n_f32(0.1f)),
		  m_color(s_color_white.getImpl().get128()), m_pair_y(1.0f), m_pair_x(1.0f),
		  m_flag0(false), m_flag1(false)
	{
		m_revolution.m_axis[0] = 0.0f;
		m_revolution.m_axis[1] = 0.0f;
		m_revolution.m_axis[2] = 1.0f;
		m_revolution.m_angle = 13.0f;
		for (uint32_t i = 0; i < sizeof(m_name.m_words) / sizeof(m_name.m_words[0]); i++)
			m_name.m_words[i] = 0;
		SetName("");
	}

	/* the name, cut to 31 characters */
	void SetName(const char *name)
	{
		size_t len = sce_paf_strlen(name);
		size_t count = len < sizeof(m_name) - 1 ? len : sizeof(m_name) - 1;
		size_t i = 0;
		char *dst = GetName();

		for (; count != 0; count--, i++)
			*dst++ = *name++;
		GetName()[i] = 0;
	}

	/* the name, in the words it is kept in */
	char *GetName()
	{
		return reinterpret_cast<char *>(m_name.m_words);
	}

	const char *GetName() const
	{
		return reinterpret_cast<const char *>(m_name.m_words);
	}

	/* m_color as the vector the shaders are given it as, in place */
	const math::v4 &GetColor() const
	{
		return reinterpret_cast<const math::v4 &>(m_color);
	}

	static float32x4_t PanelVec(float x, float y, float z, float w)
	{
		return sce::Vectormath::Simd::Aos::Vector4(x, y, z, w).get128();
	}

	/* the object's world position and rotation, and its size */
	float32x4_t m_pos;
	float32x4_t m_rot;
	float32x4_t m_scale;
	/* set by ScePafGraphics_AEBC68E5 */
	float32x2_t m_edge;
	float32x4_t m_color;
	/* the panel's texture; the renderer's white one while it has none */
	arl::TextureSource m_texture;
	/* set together by ScePafGraphics_70ED91E6, its second value first */
	float m_pair_y;
	float m_pair_x;
	/* set by ScePafGraphics_0C968320 */
	PanelRevolution m_revolution;
	bool m_flag0;
	bool m_flag1;
	/* the name, kept in words (zeroed and copied a word at a time; names ours) */
	struct Name {
		uint32_t m_words[8];
	} m_name;
};

class PanelObj : public DrawObj {
public:
	static const char *DrawObjType();

	PanelObj(const DrawStyle &style);

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~PanelObj();
	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info);
	virtual void SetSize(math::v3 size);

	static const char m_drawobj_type[];

	PanelParam *m_param;
};

/* m_revolution, for Widget::SetObjAnim (not exported; name ours) */
math::v4 GetPanelRevolution(const PanelObj *obj);

}
}

extern "C" {
/* the pair, the edge and the revolution (names ours) */
void ScePafGraphics_70ED91E6(paf::graph::PanelObj *obj, const paf::math::v2 &value);
void ScePafGraphics_AEBC68E5(paf::graph::PanelObj *obj, const paf::math::v2 &edge);
void ScePafGraphics_0C968320(paf::graph::PanelObj *obj, const paf::math::v4 &revolution);
}

#endif
