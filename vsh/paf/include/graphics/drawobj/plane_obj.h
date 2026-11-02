/*
 * paf::graph::PlaneObj: a textured rectangle, laid out against the object's box by an align and
 * a scale mode on each axis, and drawn by the object's shader with up to four textures. A texture
 * divided by its surface (3x3, 3x1, ...) is drawn as a mesh of its divisions, whose borders keep
 * their size while the rest stretches. And paf::graph::PlaneObjParam, its draw object parameters.
 *
 * Class, method, enum and enumerator names from the mangled export names and ScePaf-RE's
 * header (m_align, m_scale_mode, m_scale_value, m_tex_region, m_div_border_scale and
 * m_tex_surface after its fields), the parameters' names from the names they are read under
 * ("div_center_pos", ...), PlaneObjDrawParams from the identifier of the 1.80 build. The
 * division tables, the flags and the other member names are ours.
 */
#ifndef PAF_GRAPHICS_DRAWOBJ_PLANE_OBJ_H
#define PAF_GRAPHICS_DRAWOBJ_PLANE_OBJ_H

#include <stdint.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <versatileparam/idparam.h>

namespace paf {
namespace graph {

namespace arl {
class VertexArray;
class IndexArray;
}

/* what a shader draws a plane with: the mesh, the textures and their region (0x2c bytes;
   m_instances and the reserved words ours, 1 and 0 here) */
class PlaneObjDrawParams {
public:
	PlaneObjDrawParams(intrusive_ptr<Surface> *textures, math::v4 *tex_region)
		: m_reserved(0), m_vertices(NULL), m_indices(NULL), m_count(0), m_instances(0),
		  m_textures(textures), m_num_textures(4), m_tex_region(tex_region)
	{
		m_reserved2[0] = 0;
		m_reserved2[1] = 0;
	}

	virtual ~PlaneObjDrawParams()
	{
	}

	uint32_t m_reserved;
	arl::VertexArray *m_vertices;
	/* NULL: the vertices drawn in order */
	arl::IndexArray *m_indices;
	uint32_t m_count;
	uint32_t m_instances;
	intrusive_ptr<Surface> *m_textures;
	uint32_t m_num_textures;
	/* NULL: the whole texture */
	math::v4 *m_tex_region;
	uint32_t m_reserved2[2];
};

/* what draws a draw object (slot 2 named ours) */
class Shader {
public:
	virtual ~Shader()
	{
	}

	virtual void Draw(PlaneObjDrawParams &params) = 0;
};

/* the shader gutil::Init makes the default (constructed by 0x81195248, its code not converted;
   name ours); 0x28 bytes */
class DefaultShader : public Shader {
public:
	DefaultShader();
	virtual ~DefaultShader();
	virtual void Draw(PlaneObjDrawParams &params);

	uint32_t m_data[9];
};

/* a division of a plane by its surface's divide mode: how it is cut, the borders' ratios across
   and down, and for the modes 4 and 5 the stretching part's (the surface's when the plane has
   none; the class name ours, the fields after 0.990's plane parameters div_mode,
   div_border_ratio_h/v and div_ratio_stretch) */
class PlaneDivide {
public:
	uint32_t m_div_mode;
	math::v2 m_div_border_ratio_h;
	math::v2 m_div_border_ratio_v;
	math::v2 m_div_ratio_stretch;
};

/* what a plane's layout and mesh are to be made again (names ours) */
class PlaneObjFlags {
public:
	PlaneObjFlags()
	{
		m_texcoord_dirty = false;
		m_mesh_dirty = false;
		m_layout_dirty = false;
		m_position_dirty = false;
		m_repeat_s = false;
		m_repeat_t = false;
		m_nine_patch = false;
	}

	bool m_texcoord_dirty : 1;
	bool m_mesh_dirty : 1;
	bool m_layout_dirty : 1;
	bool m_position_dirty : 1;
	/* the textures given wrap with REPEAT across and down */
	bool m_repeat_s : 1;
	bool m_repeat_t : 1;
	/* drawn as nine quads, the corners' texture coordinates 0 and 1 */
	bool m_nine_patch : 1;
	bool m_reserved : 1;
};

class PlaneObj : public DrawObj {
public:
	enum AlignMode {
		ALIGN_CENTER = 0,
		ALIGN_NONE = 0,
		ALIGN_BOTTOM = 1,
		ALIGN_LEFT = 1,
		ALIGN_MINUS = 1,
		ALIGN_PLUS = 2,
		ALIGN_RIGHT = 2,
		ALIGN_TOP = 2
	};

	enum ScaleMode {
		SCALE_SIZE = 0,
		SCALE_SURFACE = 1,
		SCALE_VALUE = 2,
		SCALE_ASPECT_SIZE = 3,
		SCALE_ASPECT_VALUE = 4,
		SCALE_REGION = 5,
		SCALE_REGION_ASPECT_SIZE = 6,
		SCALE_REGION_ASPECT_VALUE = 7,
		SCALE_MODE_MAX = 8
	};

	static const char *DrawObjType();
	static void DrawCallback(const DrawCallbackArg &arg);

	PlaneObj(const DrawStyle &style);

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;
	virtual ~PlaneObj();
	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha, DrawInfo &info);
	virtual void Draw(const math::matrix &world, uint32_t color, float alpha);

	virtual void ReleaseAll()
	{
		for (int32_t i = 0; i < 4; i++)
			m_tex_surface[i].clear();
	}

	/* the box the layout drew the plane in */
	virtual DrawObjBounds GetBounds() const
	{
		return DrawObjBounds(m_bounds_min, m_bounds_max);
	}

	virtual void SetTexture(const intrusive_ptr<Surface> &texture, uint32_t index = 0);
	virtual void GetTexture(intrusive_ptr<Surface> &texture, uint32_t index = 0);
	virtual void SetPos(math::v3 pos);
	virtual void SetSize(math::v3 size);
	virtual void Update();
	/* exported by NID F9B86D31 only: the plane's own position and size in place of the
	   object's */
	virtual void CalcWorldMatrix(math::matrix &world_mat, const math::matrix &parent,
				     bool unscaled);
	virtual void UpdateLayout();
	virtual void UpdateDivVerticesPosition();
	virtual void UpdateDivVerticesTexcoord();

	void SetTexRegion(const math::v4 &region);
	int32_t SetMinFilter(TextureFilter value, uint32_t index = 0);
	int32_t SetMagFilter(TextureFilter value, uint32_t index = 0);
	int32_t SetWrapS(TextureWrap value, uint32_t index = 0);
	int32_t SetWrapT(TextureWrap value, uint32_t index = 0);
	int32_t GetMinFilter(uint32_t index = 0);
	int32_t GetMagFilter(uint32_t index = 0);
	int32_t GetWrapS(uint32_t index = 0);
	int32_t GetWrapT(uint32_t index = 0);
	void SetAlign(AlignMode ax, AlignMode ay);
	void SetScaleMode(ScaleMode sx, ScaleMode sy);
	void SetScaleValue(const math::v3 &scale);
	void SetDivBorderScale(float scale);
	void SetDivCenterPos(float pos);

	static const char m_drawobj_type[];

	PlaneObjFlags m_plane_flags;
	/* the division whose index arrays the mesh draws with, -1 without a mesh */
	int8_t m_div_index;
	AlignMode m_align[2];
	ScaleMode m_scale_mode[2];
	math::v4 *m_tex_region;
	/* the plane's size and position in the object, which the layout works out */
	math::v3 m_plane_size;
	math::v3 m_plane_pos;
	math::v3 m_scale_value;
	math::v3 m_bounds_min;
	math::v3 m_bounds_max;
	/* NULL: the first texture's division */
	PlaneDivide *m_divide;
	float m_div_border_scale;
	/* how far the stretching part is moved off the middle, in the plane's units */
	float m_div_center_pos;
	intrusive_ptr<Surface> m_tex_surface[4];
	/* the mesh's vertices */
	arl::VertexArray *m_div_vertices;

private:
	friend class PlaneObjParam;

	/* the plane's division, else the first texture's, 0 without (name ours) */
	uint32_t DivideMode() const
	{
		if (m_divide != NULL)
			return m_divide->m_div_mode;
		if (m_tex_surface[0].get() != NULL)
			return m_tex_surface[0]->GetDivideMode();
		return 0;
	}

	/* the layout across (0) or down (1), from the texture's size and its margins (left,
	   bottom, right, top), and without a texture (names ours) */
	void UpdateLayoutAxis(uint32_t axis, math::v2 tex_size, math::v4 margin);
	void UpdateLayoutAxis(uint32_t axis);
	/* the mesh of the first texture's division, made and let go (names ours) */
	void CreateDivMesh();
	void ReleaseDivMesh();
	/* a new first texture: the mesh is made again when the division changes, and its texture
	   coordinates when the ratios do (name ours) */
	void CheckTextureDivide(const intrusive_ptr<Surface> &current,
				const intrusive_ptr<Surface> &texture);
};

class PlaneObjParam : public DrawObjParam {
public:
	PlaneObjParam();

	static const char m_param_name[];
	static const char *ParamName();

	virtual const char *GetParamName() const;

	virtual ~PlaneObjParam();

	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(DrawObj *obj);
	virtual int32_t GetEach(DrawObj *obj);

	IDParam m_texture[4];
	math::v3 m_pos;
	math::v3 m_size;
	math::v4 m_tex_region;
	math::v3 m_scale_value;
	PlaneObj::AlignMode m_align_x;
	PlaneObj::AlignMode m_align_y;
	PlaneObj::ScaleMode m_scale_mode_x;
	PlaneObj::ScaleMode m_scale_mode_y;
	float m_div_center_pos;
	float m_div_border_scale;
};

}
}

#endif
