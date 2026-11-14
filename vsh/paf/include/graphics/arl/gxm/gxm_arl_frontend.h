/*
 * paf::graph::arl's GL-like front end over GXM: the ArlInstance Gl() returns, the GXM context
 * and shader patcher it made, the scene it draws into and the render state it keeps. Laid out
 * from its constructor, destructor and the exports that set it up and tear it down (0x7d0
 * bytes). Names from the mangled export names and ScePaf-RE's gxm_arl_frontend.h; the
 * helper classes' names, the bases' and the members' names ours.
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_ARL_FRONTEND_H
#define PAF_GRAPHICS_ARL_GXM_GXM_ARL_FRONTEND_H

#include <stdint.h>
#include <gxm.h>
#include <common/shared_ptr.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_render_target_pool.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/graphmem/graphmem.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <std/string>
#include <std/vector>

namespace paf {
namespace graph {

class Surface;

namespace arl {

class FrameBufferObject;
class ClearBase;
class FrameBufferCopy;
class ArlInstance;

/* a rectangle in pixels by two corners, the first the smaller (names ours) */
class Rect {
public:
	Rect() : m_x0(0), m_y0(0), m_x1(0), m_y1(0)
	{
	}

	Rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
		: m_x0(x0 < x1 ? x0 : x1), m_y0(y0 < y1 ? y0 : y1), m_x1(x0 > x1 ? x0 : x1),
		  m_y1(y0 > y1 ? y0 : y1)
	{
	}

	int32_t m_x0;
	int32_t m_y0;
	int32_t m_x1;
	int32_t m_y1;
};

/* a stack of matrices, the top one in use, named for the messages that report it */
class MatrixStack {
public:
	class Entry {
	public:
		math::matrix m_matrix;
		/* not taken over by a push: a pushed entry's inverse is the one of the entry
		   m_inverse_entry names (names ours) */
		math::matrix m_inverse;
		/* the inverse worked out */
		bool m_inverse_valid;
		int32_t m_inverse_entry;
		/* a rotation and translation only, inverted by inverse1 */
		bool m_rigid;
	};

	/* Sony's build keeps the constructor out of line */
	__attribute__((noinline)) MatrixStack(uint32_t capacity, const char *name)
		: m_depth(0), m_name(name)
	{
		m_stack.resize(capacity);
		PushEntry();
	}

	~MatrixStack()
	{
		--m_depth;
	}

	/* the top entry pushed again; the first one the identity (Sony's build keeps it out of
	   line, 0x81225680, and inline in the constructor) */
	__attribute__((noinline)) void Push()
	{
		PushEntry();
	}

	/* Push's body (name ours) */
	__attribute__((always_inline)) void PushEntry()
	{
		Entry &top = m_stack[m_depth];

		if (m_depth == 0) {
			top.m_matrix = math::matrix::Identity();
			top.m_inverse = math::matrix::Identity();
			top.m_inverse_entry = 0;
			top.m_rigid = true;
			top.m_inverse_valid = true;
		} else {
			Entry &below = m_stack[m_depth - 1];

			top.m_matrix = below.m_matrix;
			top.m_inverse_valid = below.m_inverse_valid;
			top.m_inverse_entry = below.m_inverse_entry;
			top.m_rigid = below.m_rigid;
		}
		m_depth++;
	}

	/* the top entry pushed again; the first one the identity (name ours) */
	void PushTop()
	{
		if (m_depth == 0) {
			m_depth++;
			LoadIdentity();
		} else {
			Entry &below = m_stack[m_depth - 1];
			Entry &top = m_stack[m_depth];

			top.m_matrix = below.m_matrix;
			top.m_inverse_valid = below.m_inverse_valid;
			top.m_inverse_entry = below.m_inverse_entry;
			top.m_rigid = below.m_rigid;
			m_depth++;
		}
	}

	void Pop()
	{
		--m_depth;
	}

	/* the top matrix the identity, its own inverse (name ours; Sony's build keeps it out of
	   line) */
	__attribute__((noinline)) void LoadIdentity()
	{
		Entry &top = m_stack[m_depth - 1];

		top.m_matrix = math::matrix::Identity();
		top.m_inverse = math::matrix::Identity();
		top.m_inverse_entry = m_depth - 1;
		top.m_rigid = true;
		top.m_inverse_valid = true;
	}

	/* the top matrix replaced, its inverse to be worked out again (name ours) */
	void Load(const math::matrix &matrix)
	{
		Entry &top = m_stack[m_depth - 1];

		top.m_matrix = matrix;
		top.m_inverse_valid = false;
		top.m_inverse_entry = m_depth - 1;
		top.m_rigid = false;
	}

	/* the top matrix replaced, the inverse flag kept as it was (name ours) */
	void LoadKeepInverse(const math::matrix &matrix)
	{
		Entry &top = m_stack[m_depth - 1];

		top.m_matrix = matrix;
		top.m_inverse_valid = false;
		top.m_inverse_entry = m_depth - 1;
		top.m_rigid &= true;
	}

	Entry &Top()
	{
		return m_stack[m_depth - 1];
	}

	/* the top matrix's inverse, worked out the first time it is asked for (name ours) */
	const math::matrix &GetInverse()
	{
		Entry &top = m_stack[m_depth - 1];

		if (!top.m_inverse_valid) {
			top.m_inverse_valid = true;
			top.m_inverse_entry = m_depth - 1;
			if (top.m_rigid)
				top.m_inverse = top.m_matrix.inverse1();
			else
				top.m_inverse = top.m_matrix.inverse();
		}
		return m_stack[top.m_inverse_entry].m_inverse;
	}

	/* the top matrix multiplied by another on its right (name ours) */
	__attribute__((noinline)) void MultMatrix(const math::matrix &matrix);

	/* the top matrix scaled on its right (name ours) */
	__attribute__((noinline)) void Scale(const math::v3 &scale)
	{
		Entry &top = m_stack[m_depth - 1];

		top.m_matrix = top.m_matrix *
			       math::matrix(sce::Vectormath::Simd::Aos::Matrix4::scale(scale.getImpl()));
		top.m_rigid = false;
		top.m_inverse_valid = false;
		top.m_inverse_entry = m_depth - 1;
	}

	vector<Entry> m_stack;
	uint32_t m_depth;
	string m_name;
};

inline __attribute__((noinline)) void MatrixStack::MultMatrix(const math::matrix &matrix)
{
	Entry &top = m_stack[m_depth - 1];

	top.m_matrix = top.m_matrix * matrix;
	top.m_inverse_valid = false;
	top.m_inverse_entry = m_depth - 1;
	top.m_rigid &= true;
}

/* the viewport and scissor rectangles, saved and restored as a pair */
class ViewportStack {
public:
	enum {
		MAX_DEPTH = 6
	};

	class Entry {
	public:
		Rect m_viewport;
		Rect m_scissor;
	};

	/* Sony's build keeps the constructor out of line */
	__attribute__((noinline)) ViewportStack(Rect viewport, Rect scissor) : m_depth(1)
	{
		m_stack[0].m_viewport = viewport;
		m_stack[0].m_scissor = scissor;
	}

	Entry &Top()
	{
		return m_stack[m_depth - 1];
	}

	/* the top entry pushed again, while there is room */
	void Push()
	{
		if (m_depth < MAX_DEPTH) {
			m_stack[m_depth] = m_stack[m_depth - 1];
			++m_depth;
		}
	}

	/* the bottom entry stays */
	void Pop()
	{
		if (m_depth > 1)
			--m_depth;
	}

	uint32_t m_depth;
	Entry m_stack[MAX_DEPTH];
};

/* the transform state: the matrix stacks and the viewport stack */
class TransformState {
public:
	TransformState()
		: m_projection(8, "Projection"), m_model(32, "Model"), m_view(8, "View"),
		  m_texture(4, "Texture"), m_viewport(Rect(), Rect())
	{
	}

	/* the projection stack, marked changed for whoever may load it (name ours) */
	MatrixStack &Projection()
	{
		m_projection_changed = true;
		return m_projection;
	}

	/* the view stack, marked changed the same way (name ours) */
	MatrixStack &View()
	{
		m_view_changed = true;
		return m_view;
	}

	/* the projection and the model view multiplied, worked out again when either changed
	   (names ours) */
	math::matrix GetModelViewProjection()
	{
		if (m_projection_changed || m_view_changed) {
			m_view_projection = Projection().Top().m_matrix * View().Top().m_matrix;
			m_projection_changed = false;
			m_view_changed = false;
		}
		return m_view_projection * m_model.Top().m_matrix;
	}

	MatrixStack m_projection;
	MatrixStack m_model;
	MatrixStack m_view;
	MatrixStack m_texture;
	/* the projection times the view, as GetModelViewProjection last worked it out */
	math::matrix m_view_projection;
	/* set by a camera that loads the view or the projection stack */
	bool m_view_changed;
	bool m_projection_changed;
	uint8_t m_reserved2[2];
	ViewportStack m_viewport;
};

/* the shader state: the vertex array specs made for each predefined slot, and which kinds of
   input changed */
class ShaderState {
public:
	enum {
		/* one for each VertexArraySpec::Predefined layout */
		MAX_SLOTS = 36,
		/* a spec for each GXM index source, kept 16-bit index, instance, then 32-bit */
		MAX_SPEC_MODES = 4
	};

	ShaderState()
	{
		m_dirty_vertex = 0;
		m_dirty_fragment = 0;
	}

	common::SharedPtr<VertexArraySpec> m_specs[MAX_SLOTS][MAX_SPEC_MODES];
	uint32_t m_dirty_vertex;
	uint32_t m_dirty_fragment;
};

/* the frame buffer object or surface a scene draws into, and the part of it drawn */
class SceneData {
public:
	SceneData()
		: m_fbo(NULL), m_surface(NULL), m_memory(NULL), m_yuv_csc_index(0),
		  m_yuv_profile(SCE_GXM_YUV_PROFILE_BT601_STANDARD)
	{
	}

	bool operator!=(const SceneData &other) const
	{
		return m_fbo != other.m_fbo || m_memory != other.m_memory ||
		       (m_memory == other.m_memory &&
			(m_bounds.m_x0 != other.m_bounds.m_x0 ||
			 m_bounds.m_y0 != other.m_bounds.m_y0 ||
			 m_bounds.m_x1 != other.m_bounds.m_x1 ||
			 m_bounds.m_y1 != other.m_bounds.m_y1)) ||
		       (m_memory == other.m_memory &&
			(m_yuv_csc_index != other.m_yuv_csc_index ||
			 m_yuv_profile != other.m_yuv_profile)) ||
		       (m_memory == other.m_memory &&
			(m_scale_mode != other.m_scale_mode || m_full_size != other.m_full_size));
	}

	bool IsSet() const
	{
		return m_fbo != NULL || m_surface != NULL;
	}

	FrameBufferObject *m_fbo;
	Surface *m_surface;
	Rect m_bounds;
	/* the memory of the surface's page drawn into */
	void *m_memory;
	/* the screen surface's YUV profile */
	uint32_t m_yuv_csc_index;
	SceGxmYuvProfile m_yuv_profile;
	int32_t m_scale_mode;
	bool m_full_size;
};

/* the scenes drawn into, innermost last: the bottom one stays */
class SceneDataStack {
public:
	enum {
		MAX_DEPTH = 8
	};

	SceneDataStack();

	/* an empty function nothing calls; the ArlInstance's destructor calls no destructor of
	   its scene stack, so this is not one (name ours) */
	void Unused();

	void Push();
	void Set(FrameBufferObject *fbo);
	void Pop();

	uint32_t m_depth;
	FrameBufferObject *m_scene[MAX_DEPTH];
};

/* the sizes of the memory GXM is set up with */
class InitParam {
public:
	uint32_t m_ring_buffer_size;
	/* 4: the allocators take the system memory heap whatever it holds */
	int32_t m_memory_mode;
};

/* the textures drawn when none is bound: 16x16 of 0xFFFFFFFF and of 0x000000FF, 64x64 of noise
   (names ours) */
class DefaultTextures {
public:
	void Init(common::SharedPtr<VramAllocator> allocator);
	void Term();

	common::SharedPtr<Texture2D> m_white;
	common::SharedPtr<Texture2D> m_black;
	common::SharedPtr<Texture2D> m_noise;
};

/* the flag GetTemporaryInitCheckFlag returns: 2 while the front end is set up */
class InitCheckFlag {
public:
	InitCheckFlag() : m_value(0)
	{
	}

	int32_t m_value;
};

class ArlInstance : public TransformState, public ShaderState, public ProgramState,
		    public DefaultTextures, public SceneDataStack {
public:
	ArlInstance();
	~ArlInstance();

	/* the default allocator, a reference taken (name ours; Sony's TextGraphicsImpl::Draw
	   calls it out of line) */
	__attribute__((noinline)) common::SharedPtr<VramAllocator> GetDefaultAllocator()
	{
		return m_default_allocator;
	}

	void CullMode(CullModeEnum mode);
	void BlendFunc(BlendingFactor src_rgb, BlendingFactor dst_rgb, BlendingFactor src_a,
		       BlendingFactor dst_a);
	void BlendEquation(BlendEquationEnum rgb, BlendEquationEnum a);
	void Color(math::v4 color);
	void PointSize(float value, FaceEnum face);
	void LineWidth(float value, FaceEnum face);
	void SetSceneData(intrusive_ptr<Surface> surface);
	void SetSceneData(FrameBufferObject *fbo);
	void Clear(uint32_t clear_mask);
	void ClearColor(math::v4 color);
	void CopyTexImage2D(common::SharedPtr<Texture2D> texture, int32_t level, int32_t x,
			    int32_t y, size_t width, size_t height);
	volatile uint32_t *GetNotificationAddr(size_t index);
	void PolygonMode(PolygonModeEnum mode, FaceEnum face);
	void Enable(EnableCap cap);
	void Disable(EnableCap cap);
	void DepthFunc(DepthFuncEnum func, FaceEnum face);
	void ColorMask(bool r, bool g, bool b, bool a);
	void DepthMask(bool write_enabled, FaceEnum face);
	/* the texture the screen was last copied into, the other one with the flag 1, and the
	   scenes a frame draws into the render targets of a size (names ours) */
	common::SharedPtr<Texture2D> *GetCopyTexture(uint32_t flags);
	void AddScenes(RenderTargetPool::Key key, int32_t scenes);

	SceGxmContext *GetRawContext()
	{
		return m_context;
	}

	/* the marker of what the GPU is doing pushed, and popped: the marker left is the one
	   popped (names ours) */
	void PushMarker(const char *marker)
	{
		++m_marker_depth;
		m_markers.push_back(marker);
		m_marker = marker;
	}

	void PopMarker()
	{
		if (m_marker_depth != 0) {
			m_marker = m_markers.back();
			m_markers.pop_back();
			--m_marker_depth;
		}
	}

	SceGxmContextParams m_context_params;
	SceGxmContext *m_context;
	SceneData m_next_scene;
	SceneData m_scene;
	/* the surface the scene draws into, its clip set from the scissor */
	int32_t m_surface_width;
	int32_t m_surface_height;
	SceGxmColorSurface *m_color_surface;
	bool m_scene_requested;
	bool m_in_scene;
	ClearBase *m_clear;
	FrameBufferCopy *m_copy;
	RenderTargetPool *m_render_target_pool;
	IndexArray *m_quad_indices;
	/* 1 to 64, set by the constructor; nothing in the front end reads them */
	uint8_t m_sequence[64];
	bool m_reserved_flag;
	int32_t m_reserved_value;
	PolygonModeEnum m_back_polygon_mode;
	PolygonModeEnum m_front_polygon_mode;
	bool m_wclamp_enabled;
	bool m_wbuffer_enabled;
	bool m_two_sided_enabled;
	DepthFuncEnum m_front_depth_func;
	DepthFuncEnum m_back_depth_func;
	bool m_front_depth_write_enabled;
	bool m_back_depth_write_enabled;
	CullModeEnum m_cull_mode;
	uint32_t m_front_point_line_width;
	uint32_t m_back_point_line_width;
	bool m_scissor_enabled;
	math::v4 m_color;
	bool m_blend_changed;
	bool m_blend_enabled;
	SceGxmBlendInfo m_blend_info;
	SceGxmBlendInfo m_default_blend_info;
	bool m_depth_test_enabled;
	/* the debug colour caps: the colour pushed off grey, the alpha doubled, and one each draw
	   hands the surface */
	bool m_debug_color_shift;
	bool m_debug_alpha_double;
	bool m_debug_surface_draw;
	uint64_t m_frame_start;
	float m_frame_time;
	/* the markers of what the GPU is doing, innermost last */
	uint32_t m_marker_depth;
	vector<const char *> m_markers;
	const char *m_marker;
	common::SharedPtr<VramAllocator> m_default_allocator;
	common::SharedPtr<VramAllocator> m_debug_allocator;
};

ArlInstance *Gl();
int32_t *GetTemporaryInitCheckFlag();

}
}
}

/* the front end's exports Sony's exports do not name (names of the parameters ours) */
extern "C" {
/* the colour surface's clip from a rectangle, y counted from the bottom */
void ScePafGraphics_A5C4B6AC(paf::graph::arl::ArlInstance *gl, paf::graph::arl::Rect rect,
			     int32_t reserved);
/* the surface drawn into: its size and colour surface, its clip from the scissor */
void ScePafGraphics_EA8C8FA0(paf::graph::arl::ArlInstance *gl,
			     paf::graph::arl::SurfaceSize size, SceGxmColorSurface *surface);
/* the viewport over a rectangle, y counted from the bottom of a surface of the height */
void ScePafGraphics_AA7B8DB5(paf::graph::arl::ArlInstance *gl, paf::graph::arl::Rect rect,
			     int32_t height);
/* the GXM context and shader patcher set up, and torn down */
void ScePafGraphics_9AA25028(paf::graph::arl::ArlInstance *gl,
			     const paf::graph::arl::InitParam &param);
void ScePafGraphics_38161BF3(paf::graph::arl::ArlInstance *gl);
/* the ArlInstance Gl() returns made and set up, and torn down */
void ScePafGraphics_38901781(const paf::graph::arl::InitParam &param);
void ScePafGraphics_501B8EB8(void);
/* the scissor and the viewport on top of the stack */
void ScePafGraphics_56C831A9(paf::graph::arl::ArlInstance *gl,
			     const paf::graph::arl::Rect &scissor);
void ScePafGraphics_93CBC733(paf::graph::arl::ArlInstance *gl,
			     const paf::graph::arl::Rect &viewport);
/* the scene ended, the notifications given to GXM */
void ScePafGraphics_4581906A(paf::graph::arl::ArlInstance *gl,
			     SceGxmNotification *notification);
void ScePafGraphics_E7B32741(paf::graph::arl::ArlInstance *gl);
/* the spec of a predefined layout for an index source, made the first time it is asked for */
paf::common::SharedPtr<paf::graph::arl::VertexArraySpec>
ScePafGraphics_4D1E289A(paf::graph::arl::ShaderState *state,
			paf::graph::arl::VertexArraySpec::Predefined predefined,
			SceGxmIndexSource index_source);
/* the scene begun with GXM's scene flags; a scene asked for, and done with */
void ScePafGraphics_01CA92B7(paf::graph::arl::ArlInstance *gl, uint32_t flags);
void ScePafGraphics_199BD5F2(paf::graph::arl::ArlInstance *gl, uint32_t flags);
void ScePafGraphics_29F5AA8B(paf::graph::arl::ArlInstance *gl,
			     SceGxmNotification *notification, bool end);
/* both faces' stencil functions */
void ScePafGraphics_DAC195AC(paf::graph::arl::ArlInstance *gl, SceGxmStencilFunc func,
			     SceGxmStencilOp stencil_fail, SceGxmStencilOp depth_fail,
			     SceGxmStencilOp depth_pass, uint8_t compare_mask, uint8_t write_mask,
			     paf::graph::arl::FaceEnum face);
void ScePafGraphics_6E7B77AC(paf::graph::arl::ArlInstance *gl, float value);
/* nothing on GXM */
void ScePafGraphics_3B25B961(void);
void ScePafGraphics_65BACB67(void);
void ScePafGraphics_CFE5ACC9(void);
/* waits for the GPU */
void ScePafGraphics_E4B3B1D2(paf::graph::arl::ArlInstance *gl);
/* the frame counted and its time measured, in seconds */
void ScePafGraphics_0240F825(paf::graph::arl::ArlInstance *gl);
/* the format, size and memory of the surface the next scene draws into */
paf::graph::arl::SurfaceFlags ScePafGraphics_BB119805(paf::graph::arl::ArlInstance *gl);
paf::graph::arl::SurfaceSize ScePafGraphics_641E3FAE(paf::graph::arl::ArlInstance *gl);
paf::graph::arl::SurfaceSize ScePafGraphics_AC339F46(paf::graph::arl::ArlInstance *gl);
ImageMode ScePafGraphics_96173730(paf::graph::arl::ArlInstance *gl);
}

#endif
