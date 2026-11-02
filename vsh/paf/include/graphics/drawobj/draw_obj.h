/*
 * paf::graph::DrawObj: what a widget draws, at a position, size and rotation, in a color and a
 * draw style that says how (blending, depth test, culling and paf's debug caps), with the shader
 * it draws with and the two callbacks it calls when it is deleted; and paf::graph::DrawObjParam,
 * the draw parameters a resource gives a set of draw objects.
 *
 * Names after ScePaf-RE (restored from Sony's DWARF) where it has one. DrawStyle's
 * deepen_color, reduce_transparency and surface_draw bits, DeleteCBInfo's fields, the
 * virtuals ScePaf-RE leaves unnamed and m_draw_list are ours: the first from the DrawObjParam
 * attributes and the caps they bind, the virtuals from what their bodies do.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_DRAW_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_DRAW_OBJ_H

#include <stdint.h>
#include <arm_neon.h>
#include <std/list>
#include <std/memory>
#include <common/shared_ptr.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <versatileparam/versatileparam.h>

namespace paf {
namespace graph {

class DrawEnv;
class DrawInfo;
class DrawObj;
class Shader;
class Surface;

/* how a draw object is drawn: the blend, compare and cull values are ScePaf-RE's BlendType,
   CompareType and CullFaceType; the word is compared and copied whole */
class DrawStyle {
public:
	DrawStyle()
	{
	}

	DrawStyle(uint32_t value) : m_value(value)
	{
	}

	bool operator!=(const DrawStyle &rhs) const
	{
		return m_value != rhs.m_value;
	}

	union {
		struct {
			uint32_t blend : 4;
			uint32_t depth : 4;
			uint32_t lighting : 4;
			uint32_t face_cull : 2;
			uint32_t scissor : 1;
			uint32_t deepen_color : 1;
			uint32_t reduce_transparency : 1;
			uint32_t surface_draw : 1;
			uint32_t reserved1 : 14;
		};
		uint32_t m_value;
	};
};

/* a callback a draw object calls with its data when it is deleted */
class DeleteCBInfo {
public:
	void (*func)(DrawObj *obj, void *data);
	void *data;
};

/* what slot 17 sets the surface_draw bit from; a text object also takes its font flags from
   m_font_flags (1: enlarge_font_size, 2: force_bold, 4: deepen_color; 128: leave them as they
   are) (names ours) */
class SurfaceDrawParam {
public:
	int32_t m_font_flags;
	bool m_surface_draw;
};

/* the two corners of a draw object's box (names ours) */
class DrawObjBounds {
public:
	DrawObjBounds(math::v3 min, math::v3 max) : m_min(min), m_max(max)
	{
	}

	math::v3 m_min;
	math::v3 m_max;
};

/* a draw object's state: made dirty for a while by each change, and visible (names ScePaf-RE's,
   m_draw_list and the class's ours) */
class DrawObjFlags {
public:
	DrawObjFlags()
	{
		m_draw_list = 0;
		m_dirty = 1;
		m_prepare = false;
		m_visible = true;
	}

	/* 0: drawn into the scene's draw lists, which the scene keeps in two globals */
	uint32_t m_draw_list : 2;
	uint32_t m_dirty : 2;
	bool m_prepare : 1;
	bool m_visible : 1;
};

class DrawObj {
public:
	/* what a queued draw calls the object's Draw with (Sony's name; self, world_mat and color
	   after ScePaf-RE, alpha ours) */
	class DrawCallbackArg {
	public:
		DrawObj *self;
		const math::matrix *world_mat;
		uint32_t color;
		float alpha;
	};

	static const char *DrawObjType();

	static bool IsSafety()
	{
		return s_safety;
	}

	static void EnterSafety();
	static void FinishSafety();
	static void SetDirtyCount(uint32_t count);

	DrawObj(const DrawStyle &style);

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~DrawObj();

	/* OnDraw queues the object for the frame under the parent's world matrix, with a color and
	   alpha to modulate by (ScePafGraphics_B7EB9246); Draw draws it (Sony's names, by NID) */
	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info)
	{
	}

	virtual void Draw(const math::matrix &world, uint32_t color, float alpha)
	{
	}

	virtual void ReleaseAll()
	{
	}

	/* the corners pos -/+ size/2 */
	virtual DrawObjBounds GetBounds() const
	{
		sce::Vectormath::Simd::Aos::Vector3 half = m_size.getImpl() * 0.5f;
		sce::Vectormath::Simd::Aos::Vector3 pos = m_pos.getImpl();

		return DrawObjBounds(math::v3(pos - half), math::v3(pos + half));
	}

	virtual void SetTexture(const intrusive_ptr<Surface> &texture, uint32_t index = 0)
	{
	}

	virtual void GetTexture(intrusive_ptr<Surface> &texture, uint32_t index = 0)
	{
	}

	virtual void SetPos(math::v3 pos)
	{
		m_pos = pos;
		UpdateDirty();
	}

	virtual void SetSize(math::v3 size)
	{
		m_size = math::v3(sce::Vectormath::Simd::Aos::maxPerElem(
			sce::Vectormath::Simd::Aos::Vector3(0.0f), size.getImpl()));
		UpdateDirty();
	}

	/* the color as RGBA bytes, each saturated */
	virtual void SetColor(math::v4 color)
	{
		uint16x4_t rgba = vqmovn_u32(vcvtq_u32_f32(
			vmulq_f32(color.getImpl().get128(), vdupq_n_f32(255.0f))));

		m_color = vget_lane_u32(vreinterpret_u32_u8(vqmovn_u16(vcombine_u16(rgba, rgba))), 0);
	}

	virtual void SetRot(math::v3 rot);

	virtual void Update()
	{
	}

	/* a drawn frame counts the dirty frames down */
	virtual void UpdatePost(const math::matrix &world_mat)
	{
		if (m_flags.m_dirty > 0)
			m_flags.m_dirty--;
	}

	/* the parent's matrix times this object's: its position, rotation and, unless unscaled,
	   its size (name ours) */
	virtual void CalcWorldMatrix(math::matrix &world_mat, const math::matrix &parent,
				     bool unscaled);

	virtual void SetSurfaceDraw(const SurfaceDrawParam &param)
	{
		m_draw_style.surface_draw = param.m_surface_draw;
	}

	void BindDrawStyle();
	void SetShader(const common::SharedPtr<Shader> &shader);

	bool IsDirty()
	{
		return m_flags.m_dirty != 0;
	}

	/* the RGBA bytes of m_color as four floats from 0 to 1 */
	math::v4 GetColor() const
	{
		uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(m_color)));

		return math::v4(sce::Vectormath::Simd::Aos::Vector4(
			vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
				  vdupq_n_f32(1.0f / 255.0f))));
	}

	static uint32_t s_dirty_count;
	static bool s_safety;
	static const char m_drawobj_type[];

	math::v3 m_pos;
	math::v3 m_size;
	math::v3 *m_rot;
	uint32_t m_color;
	DrawStyle m_draw_style;
	DrawObjFlags m_flags;
	common::SharedPtr<Shader> m_shader;
	DeleteCBInfo m_delete_cb[2];

protected:
	/* the frames drawn after a change, outside a safety section */
	void UpdateDirty()
	{
		if (!s_safety)
			m_flags.m_dirty = s_dirty_count;
	}
};

/* the style bound with the default marker: what it already is is not bound again */
void BindDrawStyle(DrawStyle style);

/* the draw calls the renderer draws */
common::SharedPtr<DrawEnv> &DrawCalls();

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

	/* the style's element of the draw object and the draw object's index in its widget (names ours) */
	const char *m_name;
	int32_t m_index;
	list<DrawObj *> m_drawobj_list;
	math::v4 m_color;
	int32_t m_blend;
	/* -1: leave the draw object's bit as it is */
	int8_t m_deepen_color;
	int8_t m_reduce_transparency;
};

}
}

extern "C" {
/* a color modulated by a draw object's color, its alpha into the alpha (names ours) */
void ScePafGraphics_B7EB9246(const paf::graph::DrawObj *obj, uint32_t *color, float *alpha);
}

#endif
