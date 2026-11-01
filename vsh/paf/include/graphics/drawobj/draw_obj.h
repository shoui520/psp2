/*
 * paf::graph::DrawObj, as far as converted C++ uses it: what a widget draws,
 * with its color and the draw style that says how (BindDrawStyle reads it at
 * 0x30), and the two callbacks it calls when it is deleted; and
 * paf::graph::DrawObjParam, the draw parameters a resource gives a set of
 * draw objects.
 *
 * Names after ScePaf-RE (restored from Sony's DWARF); the fields before the
 * color are not converted yet. DrawStyle's deepen_color and
 * reduce_transparency bits and DeleteCBInfo's fields are named after the
 * DrawObjParam attributes and functions that set them (ours).
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_DRAW_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_DRAW_OBJ_H

#include <stdint.h>
#include <arm_neon.h>
#include <std/list>
#include <math/ngp/v4.h>
#include <versatileparam/versatileparam.h>

namespace paf {
namespace graph {

class DrawObj;

class DrawStyle {
public:
	uint32_t blend : 4;
	uint32_t depth : 4;
	uint32_t lighting : 4;
	uint32_t face_cull : 2;
	uint32_t scissor : 1;
	uint32_t deepen_color : 1;
	uint32_t reduce_transparency : 1;
	uint32_t reserved1 : 15;
};

/* a callback a draw object calls with its data when it is deleted */
class DeleteCBInfo {
public:
	void (*func)(DrawObj *obj, void *data);
	void *data;
};

class DrawObj {
public:
	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~DrawObj();
	/*
	 * Only SetColor is called from converted code: the other slots are
	 * declared without their arguments, which are not converted yet.
	 * Draw queues a draw call whose callback calls Render; GetBounds gives
	 * the corners pos -/+ size/2 (names ours).
	 */
	virtual void Draw();
	virtual void Render();
	virtual int32_t ReleaseAll();
	virtual void GetBounds();
	virtual void SetTexture();
	virtual void GetTexture();
	virtual void SetPos();
	virtual void SetSize();
	virtual void SetColor(math::v4 color);

	/* the RGBA bytes of m_color as four floats from 0 to 1 */
	math::v4 GetColor() const
	{
		uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(m_color)));

		return math::v4(sce::Vectormath::Simd::Aos::Vector4(
			vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
				  vdupq_n_f32(1.0f / 255.0f))));
	}

	uint8_t reserved[0x28];
	uint32_t m_color;
	DrawStyle m_draw_style;
	uint8_t reserved2[0xc];
	DeleteCBInfo m_delete_cb[2];
};

class DrawObjParam : public VersatileParam {
public:
	DrawObjParam()
	{
		Init();
	}

	static const char m_param_name[];
	static const char *s_names[];
	static const char *ParamName();

	virtual const char *GetParamName() const;

	/* each draw object forgets this parameter's delete callback */
	virtual ~DrawObjParam()
	{
		for (list<DrawObj *>::iterator it = m_drawobj_list.begin();
		     it != m_drawobj_list.end(); ++it) {
			DrawObj *obj = *it;

			if (obj->m_delete_cb[0].func == delete_cb && obj->m_delete_cb[0].data == this) {
				obj->m_delete_cb[0].func = NULL;
				obj->m_delete_cb[0].data = NULL;
			} else if (obj->m_delete_cb[1].func == delete_cb &&
				   obj->m_delete_cb[1].data == this) {
				obj->m_delete_cb[1].func = NULL;
				obj->m_delete_cb[1].data = NULL;
			}
		}
	}

	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(DrawObj *obj);
	virtual int32_t GetEach(DrawObj *obj);

	int32_t Init();
	int32_t AddDrawObj(DrawObj *obj);
	int32_t DelDrawObj(DrawObj *obj);
	static void delete_cb(DrawObj *obj, void *param);

	uint32_t reserved[2];
	list<DrawObj *> m_drawobj_list;
	math::v4 m_color;
	int32_t m_blend;
	/* -1: leave the draw object's bit as it is */
	int8_t m_deepen_color;
	int8_t m_reduce_transparency;
};

}
}

#endif
