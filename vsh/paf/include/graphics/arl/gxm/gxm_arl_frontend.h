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
#include <graphics/arl/gxm/gxm_texture.h>
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
class ClearProgram;
class CopyProgram;
class IndexBuffer;
class ArlInstance;

/* the shader objects, laid out when their files are converted: the program bound last
   is unbound when a scene ends (names ours) */
class VertShaderObject {
public:
	static void UnbindCurrent();
};

class FragShaderObject {
public:
	static void UnbindCurrent();
};

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
		/* not taken over by a push */
		math::matrix m_inverse;
		/* the three set by an identity entry and taken over by a push */
		bool m_identity;
		int32_t m_revision;
		bool m_inverse_valid;
	};

	/* Sony's build keeps the constructor out of line */
	__attribute__((noinline)) MatrixStack(uint32_t capacity, const char *name)
		: m_depth(0), m_name(name)
	{
		m_stack.resize(capacity);
		Push();
	}

	~MatrixStack()
	{
		--m_depth;
	}

	void Push()
	{
		Entry &top = m_stack[m_depth];

		if (m_depth == 0) {
			top.m_matrix = math::matrix::Identity();
			top.m_inverse = math::matrix::Identity();
			top.m_revision = 0;
			top.m_inverse_valid = true;
			top.m_identity = true;
		} else {
			Entry &below = m_stack[m_depth - 1];

			top.m_matrix = below.m_matrix;
			top.m_identity = below.m_identity;
			top.m_revision = below.m_revision;
			top.m_inverse_valid = below.m_inverse_valid;
		}
		m_depth++;
	}

	vector<Entry> m_stack;
	uint32_t m_depth;
	string m_name;
};

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

	MatrixStack m_projection;
	MatrixStack m_model;
	MatrixStack m_view;
	MatrixStack m_texture;
	/* nothing in the front end reads or writes them */
	uint8_t m_reserved[0x44];
	ViewportStack m_viewport;
};

/* the shader state: the textures and buffers bound and which of them changed */
class ShaderState {
public:
	enum {
		MAX_BINDINGS = 144
	};

	ShaderState()
	{
		m_dirty_vertex = 0;
		m_dirty_fragment = 0;
		m_reserved0 = 0;
		m_reserved1 = 0;
		m_flags = 0;
	}

	common::SharedPtr<TextureObject> m_bindings[MAX_BINDINGS];
	uint32_t m_dirty_vertex;
	uint32_t m_dirty_fragment;
	uint32_t m_reserved0;
	uint32_t m_reserved1;
	uint32_t m_flags;
};

/* the GXM shader patcher, shared by the shader objects */
class ShaderPatcher {
public:
	ShaderPatcher(SceGxmShaderPatcher *patcher) : m_patcher(patcher)
	{
	}

	SceGxmShaderPatcher *m_patcher;
};

/* the frame buffer object or surface a scene draws into, and the part of it drawn */
class SceneData {
public:
	SceneData()
		: m_fbo(NULL), m_surface(NULL), m_page(0), m_extent_width(0), m_extent_height(0)
	{
	}

	bool operator!=(const SceneData &other) const
	{
		return m_fbo != other.m_fbo || m_page != other.m_page ||
		       (m_page == other.m_page &&
			(m_bounds.m_x0 != other.m_bounds.m_x0 ||
			 m_bounds.m_y0 != other.m_bounds.m_y0 ||
			 m_bounds.m_x1 != other.m_bounds.m_x1 ||
			 m_bounds.m_y1 != other.m_bounds.m_y1)) ||
		       (m_page == other.m_page &&
			(m_extent_width != other.m_extent_width ||
			 m_extent_height != other.m_extent_height)) ||
		       (m_page == other.m_page &&
			(m_scale_mode != other.m_scale_mode || m_full_size != other.m_full_size));
	}

	bool IsSet() const
	{
		return m_fbo != NULL || m_surface != NULL;
	}

	FrameBufferObject *m_fbo;
	Surface *m_surface;
	Rect m_bounds;
	int32_t m_page;
	/* copied from the screen surface, which clears them when a frame buffer is given */
	int32_t m_extent_width;
	int32_t m_extent_height;
	int32_t m_scale_mode;
	bool m_full_size;
};

/* the 16-bit indices 0 to 65535 a quad list draws with (names ours) */
class IndexMemory {
public:
	IndexMemory(const common::SharedPtr<VramAllocator> &allocator)
		: m_memory(new SurfaceMemory(allocator))
	{
	}

	common::SharedPtr<SurfaceMemory> m_memory;
};

class IndexBuffer {
public:
	enum {
		SIZE = 0x20000
	};

	IndexBuffer(const common::SharedPtr<VramAllocator> &allocator)
		: m_memory(new IndexMemory(allocator)), m_reserved(0)
	{
		SurfaceMemory *memory = m_memory->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, SIZE);
			memory->m_size = SIZE;
		}

		uint16_t *index = (uint16_t *)m_memory->m_memory->m_addr;

		for (uint32_t i = 0; i < 0x10000; i++)
			*index++ = i;
	}

	common::SharedPtr<IndexMemory> m_memory;
	uint32_t m_reserved;
};

/* a VramAllocator over one of GraphMem's heaps, counting what it gave (names ours) */
class HeapVramAllocator : public VramAllocator {
public:
	HeapVramAllocator(const char *name,
			  const common::SharedPtr<GraphMem::GraphHeapAllocator> &heap)
		: m_used(0), m_name(name), m_heap(heap)
	{
	}

	virtual ~HeapVramAllocator()
	{
	}

	virtual void Alloc(SurfaceMemory *memory, uint32_t size)
	{
		common::SharedPtr<GraphMem::GraphHeapAllocator> heap(m_heap);

		if (heap.get() != NULL)
			memory->m_addr = common::SharedPtr<GraphMem::GraphHeapAllocator>(m_heap)
						 ->sce_paf_malloc(size);
		memory->m_alloc_size = size;
		m_used += size;
	}

	virtual void Free(SurfaceMemory *memory)
	{
		if (memory->m_addr != NULL) {
			m_used -= memory->m_alloc_size;

			common::SharedPtr<GraphMem::GraphHeapAllocator> heap(m_heap);

			if (heap.get() != NULL)
				common::SharedPtr<GraphMem::GraphHeapAllocator>(m_heap)->sce_paf_free(
					memory->m_addr);
			memory->m_addr = NULL;
			memory->m_alloc_size = 0;
		}
	}

	uint32_t m_used;
	string m_name;
	common::WeakPtr<GraphMem::GraphHeapAllocator> m_heap;
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

/* the textures drawn when none is bound: white, black and noise (names ours) */
class DefaultTextures {
public:
	void Init(common::SharedPtr<VramAllocator> allocator);
	void Term();

	common::SharedPtr<Texture2D> m_white;
	common::SharedPtr<Texture2D> m_black;
	common::SharedPtr<Texture2D> m_noise;
};

/* the programs that clear a surface and copy into a texture, laid out when their files
   are converted */
class ClearProgram {
public:
	void SetColor(math::v4 color);
	void Clear(ArlInstance &gl, uint32_t clear_mask);
};

class CopyProgram {
public:
	void Copy(ArlInstance &gl, common::SharedPtr<TextureObject> texture, int32_t type,
		  int32_t level, int32_t x, int32_t y, size_t width, size_t height);
	void Draw(ArlInstance &gl, int32_t mode);
};

/* the flag GetTemporaryInitCheckFlag returns: 2 while the front end is set up */
class InitCheckFlag {
public:
	InitCheckFlag() : m_value(0)
	{
	}

	int32_t m_value;
};

class ArlInstance : public TransformState, public ShaderState {
public:
	ArlInstance();
	~ArlInstance();

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
	/* the copy program drawn in the mode given, and the scenes a frame draws into the
	   render targets of a size (names ours) */
	void DrawCopy(int32_t mode);
	void AddScenes(RenderTargetPool::Key key, int32_t scenes);

	SceGxmContext *GetRawContext()
	{
		return m_context;
	}

	common::SharedPtr<ShaderPatcher> m_patcher;
	DefaultTextures m_default_textures;
	SceneDataStack m_scene_stack;
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
	ClearProgram *m_clear;
	CopyProgram *m_copy;
	RenderTargetPool *m_render_target_pool;
	IndexBuffer *m_quad_indices;
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
	/* the debug colour caps: the colour pushed off grey, the alpha doubled, and one unread */
	bool m_debug_color_shift;
	bool m_debug_alpha_double;
	bool m_debug_reserved;
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
