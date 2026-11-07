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
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/drawobj/draw_obj.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/memory>

namespace paf {
namespace graph {

class BubbleParam;

class BubbleObj : public DrawObj {
public:
	static const char *DrawObjType();

	BubbleObj(const DrawStyle &style);

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~BubbleObj();
	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info);
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

/* the refraction preset (1 and 2; others turn it off), the fixed light and the light (names
   ours) */
extern "C" {
void ScePafGraphics_5E94DC78(paf::graph::BubbleObj *obj, int32_t preset);
void ScePafGraphics_5AF8179A(paf::graph::BubbleObj *obj, const paf::math::v4 &pos);
void ScePafGraphics_8454D7FB(paf::graph::BubbleObj *obj, bool enable, const paf::math::v4 &pos,
			     float range);
}

#endif
