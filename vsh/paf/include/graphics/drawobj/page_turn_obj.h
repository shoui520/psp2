/*
 * paf::graph::PageTurnObj: a draw object the page-turn renderer draws as a turning page, from a
 * block of parameters the object keeps up to date and a mesh the renderer builds; and
 * paf::graph::PageTurnObjParam, its draw object parameters. Class and method names from the
 * mangled export names; the parameter block, the mesh and every member name are ours.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_PAGE_TURN_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_PAGE_TURN_OBJ_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/draw_obj.h>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <std/vector>

namespace paf {
namespace graph {

/* what the page-turn renderer draws a page with (0x70 bytes) */
class PageTurnParam {
public:
	PageTurnParam()
		: m_alpha(1.0f), m_page_size(3.52941179f, 2.0f), m_margin(0.0f, 0.0f),
		  m_renderer_param(0.2f, 0.2f), m_scale(1.0f, 1.0f, 1.0f, 0.0f),
		  m_region(0.0f, 0.0f, 1.0f, 1.0f), m_rot(0.0f, 0.0f, 0.0f, 1.0f)
	{
		math::matrix world = math::matrix::Identity();

		m_pos = world.w();

		math::quaternion rot(world);

		m_curl_changed = true;
		m_curl = math::v2(1.38235295f, 0.753675997f);
		m_rot = math::v4(sce::Vectormath::Simd::Aos::Vector4(rot.getImpl())).unit();
	}

	intrusive_ptr<Surface> m_texture;
	float m_alpha;
	/* the object's size less a margin on each side, and the margin */
	math::v2 m_page_size;
	math::v2 m_margin;
	/* 0.2 here, cleared by PageTurnObj's constructor; only the renderer reads it */
	math::v2 m_renderer_param;
	/* the object's world scale, texture region, position and rotation */
	math::v4 m_scale;
	math::v4 m_region;
	math::v4 m_pos;
	math::v4 m_rot;
	/* how far the page is turned, across and down; the renderer rebuilds the mesh when it
	   changes */
	math::v2 m_curl;
	bool m_curl_changed;
};

/* a strip of the mesh: the indices and their count */
class PageTurnStrip {
public:
	const uint16_t *m_indices;
	uint32_t m_count;
};

namespace arl {
class VertexArraySpec;
}

class PageTurnVertices;

/* the page's mesh, which the renderer builds (0x810c7cd0) and draws (0x48 bytes); the words
   not named are not read in this file, only by the renderer, which is not converted */
class PageTurnMesh {
public:
	PageTurnMesh() : m_division(0), m_reserved1(0.0f, 0.0f)
	{
		m_reserved2 = 0;
	}

	/* the renderer sets 200 when it builds the mesh */
	int32_t m_division;
	common::SharedPtr<arl::VertexArraySpec> m_spec;
	common::SharedPtr<PageTurnVertices> m_vertices;
	uint32_t m_reserved0;
	math::v2 m_reserved1;
	uint32_t m_reserved2;
	/* drawn as triangle strips, the front and the back of the page */
	vector<PageTurnStrip> m_front_strips;
	vector<PageTurnStrip> m_back_strips;
	uint32_t m_reserved3;
};

class PageTurnObj : public DrawObj {
public:
	static const char *DrawObjType();
	static void SetBgTexture(intrusive_ptr<Surface> texture);
	static void DrawCallback(const DrawCallbackArg &arg);

	PageTurnObj(const DrawStyle &style);

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~PageTurnObj();
	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info);
	virtual void Draw(const math::matrix &world, uint32_t color, float alpha);
	virtual void ReleaseAll();
	virtual void SetTexture(const intrusive_ptr<Surface> &texture, uint32_t index = 0);
	virtual void GetTexture(intrusive_ptr<Surface> &texture, uint32_t index = 0);

	static const char m_drawobj_type[];
	/* the page-turn renderer is made with the first page */
	static bool s_renderer_made;
	/* the pages alive and the pages drawn this frame */
	static int32_t s_count;
	static int32_t s_draw_count;
	static intrusive_ptr<Surface> s_bg_texture;

	/* set when the turn changes: the next draw works out m_param's curl */
	bool m_turn_changed;
	math::v2 m_turn;
	math::v2 m_turn_offset;
	math::v2 m_margin;
	PageTurnParam *m_param;
	PageTurnMesh *m_mesh;
};

class PageTurnObjParam : public DrawObjParam {
public:
	PageTurnObjParam();

	static const char m_param_name[];
	static const char *ParamName();

	virtual const char *GetParamName() const;

	virtual ~PageTurnObjParam();

	virtual int32_t Create(int32_t mode);
};

}
}

extern "C" {
/* at the end of a frame no page drew: the renderer lets go of its textures (name ours) */
void ScePafGraphics_1612A18D();
/* empty */
void ScePafGraphics_2B9FB1E5();
}

#endif
