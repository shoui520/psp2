/*
 * paf::graph::BubbleObj: a draw object the panel renderer draws as a glass bubble, from a block
 * of parameters the object keeps up to date. Class and method names from the mangled export
 * names; the parameter block and every member name are ours.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_BUBBLE_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_BUBBLE_OBJ_H

#include <stdint.h>
#include <arm_neon.h>
#include <common/shared_ptr.h>
#include <graphics/color.h>
#include <graphics/drawobj/draw_obj.h>
#include <math/ngp/v4.h>
#include <std/memory>

namespace paf {
namespace graph {

/* a vector of the parameter block: a bare NEON vector, which is 8-byte aligned (name ours) */
static inline float32x4_t BubbleVec(float x, float y, float z, float w)
{
	return sce::Vectormath::Simd::Aos::Vector4(x, y, z, w).get128();
}

/* the texture the renderer made from m_texture (its type is not read here) */
class BubbleTexture;

/* what the panel renderer draws a bubble with (0xb8 bytes) */
class BubbleParam {
public:
	BubbleParam()
		: m_pos(BubbleVec(0.0f, 0.0f, 0.0f, 1.0f)), m_rot(BubbleVec(0.0f, 0.0f, 0.0f, 1.0f)),
		  m_scale(BubbleVec(1.0f, 1.0f, 1.0f, 1.0f)), m_edge(vdup_n_f32(0.1f)),
		  m_color(s_color_white.getImpl().get128()),
		  m_drawn(false), m_enabled(true), m_refract(false), m_tint(false), m_light(false),
		  m_refract_scale(1.087f), m_refract_offset(-0.026f), m_reflect(1.0f),
		  m_light_range(800.0f), m_light_pos(BubbleVec(0.0f, 0.0f, 0.0f, 1.0f)),
		  m_tint_color(BubbleVec(600.0f, 850.0f, 2300.0f, 0.0f)),
		  m_ambient(BubbleVec(0.6f, 0.6f, 0.6f, 0.6f)),
		  m_eye(BubbleVec(0.0f, 0.0f, -100.0f, 1.0f)), m_up(BubbleVec(0.0f, 0.0f, 1.0f, 0.0f))
	{
	}

	/* the object's world position and rotation, and its size across */
	float32x4_t m_pos;
	float32x4_t m_rot;
	float32x4_t m_scale;
	float32x2_t m_edge;
	float32x4_t m_color;
	common::SharedPtr<BubbleTexture> m_texture_cache;
	intrusive_ptr<Surface> m_texture;
	/* cleared by the renderer when it draws */
	bool m_drawn;
	bool m_enabled : 1;
	/* set with m_refract_scale and m_refract_offset (ScePafGraphics_5E94DC78) */
	bool m_refract : 1;
	/* set with m_tint_color (ScePafGraphics_5AF8179A) */
	bool m_tint : 1;
	/* set by ScePafGraphics_8454D7FB with m_light_range and m_light_pos */
	bool m_light : 1;
	float m_refract_scale;
	float m_refract_offset;
	float m_reflect;
	float m_light_range;
	float32x4_t m_light_pos;
	float32x4_t m_tint_color;
	float32x4_t m_ambient;
	float32x4_t m_eye;
	float32x4_t m_up;
};

class BubbleObj : public DrawObj {
public:
	static const char *DrawObjType();

	BubbleObj(const DrawStyle &style);

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~BubbleObj();
	virtual void Draw(const math::matrix &parent, uint32_t color, float alpha);
	virtual void SetTexture(const intrusive_ptr<Surface> &texture, uint32_t index = 0);
	virtual void GetTexture(intrusive_ptr<Surface> &texture, uint32_t index = 0);
	virtual void SetSize(math::v3 size);

	static const char m_drawobj_type[];

	BubbleParam *m_param;
	/* drawn as the renderer's kind 2 instead of 0 */
	bool m_opaque : 1;
};

}
}

/* the refraction preset (1 and 2; others turn it off), the tint and the light (names ours) */
extern "C" {
void ScePafGraphics_5E94DC78(paf::graph::BubbleObj *obj, int32_t preset);
void ScePafGraphics_5AF8179A(paf::graph::BubbleObj *obj, const paf::math::v4 &color);
void ScePafGraphics_8454D7FB(paf::graph::BubbleObj *obj, bool enable, const paf::math::v4 &pos,
			     float range);
}

#endif
