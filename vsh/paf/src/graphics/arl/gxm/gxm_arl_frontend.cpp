/*
 * paf::graph::arl's GL-like front end over GXM: the ArlInstance Gl() returns, the scene it
 * draws into and the render state it keeps (the file's name from its header's, ScePaf-RE's).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <math/ngp/v2.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_clear.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/color.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/surface/surface_screen.h>
#include <system/system.h>
#include <timer/timer.h>
#include <src/common/frame_count.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "gxm_framebuffer_object.h"

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace graph {
namespace arl {

/* the memory GXM is given: the ring buffers and the shader patcher's (names ours) */
static void *s_ring_buffer;
static void *s_vdm_ring_buffer;
static void *s_vertex_ring_buffer;
static void *s_fragment_ring_buffer;
static void *s_fragment_usse_ring_buffer;
static void *s_patcher_buffer;
static void *s_patcher_vertex_usse;
static void *s_patcher_fragment_usse;
static int32_t s_memory_mode = -1;
static ArlInstance *s_instance;
volatile uint32_t *s_notification_region;

/* the shader patcher's buffer and each of its USSE memories (names ours) */
enum {
	PATCHER_BUFFER_SIZE = 0x40000,
	PATCHER_USSE_SIZE = 0x10000
};

/* the heap the ring buffers come from, and the one the allocators take (names ours) */
static common::SharedPtr<GraphMem::GraphHeapAllocator> s_ring_heap;
static common::SharedPtr<GraphMem::GraphHeapAllocator> s_heap;
static InitCheckFlag s_init_check_flag;

int32_t *GetTemporaryInitCheckFlag()
{
	return &s_init_check_flag.m_value;
}

static void *PatcherHostAlloc(void *user_data, uint32_t size);
static void PatcherHostFree(void *user_data, void *mem);

extern "C" void ScePafGraphics_9AA25028(ArlInstance *gl, const InitParam &param)
{
	GraphMem::Option ring_option;

	ring_option.gpu_map_attrib = SCE_GXM_MEMORY_ATTRIB_READ;
	s_ring_buffer = GraphMem::AllocOSMemory(GraphMem::DeviceType_SystemMemory,
						param.m_ring_buffer_size, "ScePafGxmRingBuffer",
						&ring_option);
	s_ring_heap = common::SharedPtr<GraphMem::GraphHeapAllocator>(new GraphMem::GraphHeapAllocator(
		s_ring_buffer, param.m_ring_buffer_size, "ScePafGxmRingBuffer"));

	/* a quarter of the ring buffer for the fragments, the rest but the VDM's and the
	   fragment USSE's for the vertices */
	int32_t quarter = (int32_t)param.m_ring_buffer_size / 4;
	uint32_t vertex_size = (quarter * 3 - (SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE +
					       SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE)) &
			       ~0xfff;
	uint32_t fragment_size = quarter & ~0xfff;

	s_vdm_ring_buffer = s_ring_heap->sce_paf_malloc(SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE);
	s_vertex_ring_buffer = s_ring_heap->sce_paf_malloc(vertex_size);
	s_fragment_ring_buffer = s_ring_heap->sce_paf_malloc(fragment_size);

	GraphMem::Option usse_option;

	usse_option.gpu_map_type = GraphMem::Option::GpuMapType_FragmentUsse;
	s_fragment_usse_ring_buffer = GraphMem::AllocOSMemory(
		GraphMem::DeviceType_SystemMemory, SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
		"ScePafGxmRingFragUsse", &usse_option);

	SceGxmContextParams context_params;

	sce_paf_memset(&context_params, 0, sizeof(context_params));
	context_params.hostMem = sce_paf_malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	context_params.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	context_params.vdmRingBufferMem = s_vdm_ring_buffer;
	context_params.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	context_params.vertexRingBufferMem = s_vertex_ring_buffer;
	context_params.vertexRingBufferMemSize = vertex_size;
	context_params.fragmentRingBufferMem = s_fragment_ring_buffer;
	context_params.fragmentRingBufferMemSize = fragment_size;
	context_params.fragmentUsseRingBufferMem = s_fragment_usse_ring_buffer;
	context_params.fragmentUsseRingBufferMemSize =
		SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	context_params.fragmentUsseRingBufferOffset = usse_option.gpu_usse_offset;
	gl->m_context_params = context_params;
	gl->m_context = NULL;
	sceGxmCreateContext(&gl->m_context_params, &gl->m_context);

	/* Sony's build names the patcher's buffer as the fragment USSE ring buffer */
	s_patcher_buffer = GraphMem::AllocOSMemory(GraphMem::DeviceType_SystemMemory,
						   PATCHER_BUFFER_SIZE, "ScePafGxmRingFragUsse",
						   NULL);

	GraphMem::Option vertex_usse_option;

	vertex_usse_option.gpu_map_type = GraphMem::Option::GpuMapType_VertexUsse;
	s_patcher_vertex_usse = GraphMem::AllocOSMemory(
		GraphMem::DeviceType_SystemMemory, PATCHER_USSE_SIZE,
		"ScePafGxmShaderPatcherVertUsse", &vertex_usse_option);

	GraphMem::Option fragment_usse_option;

	fragment_usse_option.gpu_map_type = GraphMem::Option::GpuMapType_FragmentUsse;
	s_patcher_fragment_usse = GraphMem::AllocOSMemory(
		GraphMem::DeviceType_SystemMemory, PATCHER_USSE_SIZE,
		"ScePafGxmShaderPatcherFragUsse", &fragment_usse_option);

	SceGxmShaderPatcherParams patcher_params;

	sce_paf_memset(&patcher_params, 0, sizeof(patcher_params));
	patcher_params.userData = NULL;
	patcher_params.hostAllocCallback = PatcherHostAlloc;
	patcher_params.hostFreeCallback = PatcherHostFree;
	patcher_params.bufferAllocCallback = NULL;
	patcher_params.bufferFreeCallback = NULL;
	patcher_params.bufferMem = s_patcher_buffer;
	patcher_params.bufferMemSize = PATCHER_BUFFER_SIZE;
	patcher_params.vertexUsseAllocCallback = NULL;
	patcher_params.vertexUsseFreeCallback = NULL;
	patcher_params.vertexUsseMem = s_patcher_vertex_usse;
	patcher_params.vertexUsseMemSize = PATCHER_USSE_SIZE;
	patcher_params.vertexUsseOffset = vertex_usse_option.gpu_usse_offset;
	patcher_params.fragmentUsseAllocCallback = NULL;
	patcher_params.fragmentUsseFreeCallback = NULL;
	patcher_params.fragmentUsseMem = s_patcher_fragment_usse;
	patcher_params.fragmentUsseMemSize = PATCHER_USSE_SIZE;
	patcher_params.fragmentUsseOffset = fragment_usse_option.gpu_usse_offset;

	SceGxmShaderPatcher *patcher = NULL;

	sceGxmShaderPatcherCreate(&patcher_params, &patcher);
	gl->m_patcher = common::SharedPtr<ShaderPatcher>(new ShaderPatcher(patcher));

	s_memory_mode = param.m_memory_mode;
	if (s_memory_mode != 4) {
		if (common::SharedPtr<GraphMem::GraphHeapAllocator>(GraphMem::SystemMemoryHeap())
			    .get() != NULL)
			s_heap = common::SharedPtr<GraphMem::GraphHeapAllocator>(
				GraphMem::SystemMemoryHeap());
		else
			s_heap = common::SharedPtr<GraphMem::GraphHeapAllocator>(
				GraphMem::VideoMemoryHeap());
	} else {
		s_heap = common::SharedPtr<GraphMem::GraphHeapAllocator>(
			GraphMem::SystemMemoryHeap());
	}

	gl->m_default_allocator =
		common::SharedPtr<VramAllocator>(new VramAllocator("default", s_heap));
	gl->m_debug_allocator =
		common::SharedPtr<VramAllocator>(new VramAllocator("debug", s_heap));
	gl->m_clear = new ClearBase();
	gl->m_quad_indices = new IndexArray(gl->m_default_allocator);
	gl->m_render_target_pool = new RenderTargetPool();
	gl->m_copy = new FrameBufferCopy();
	s_init_check_flag.m_value |= 2;
	gl->DefaultTextures::Init(gl->m_default_allocator);
	s_notification_region = sceGxmGetNotificationRegion();
}

extern "C" void ScePafGraphics_38161BF3(ArlInstance *gl)
{
	sceGxmFinish(gl->m_context);
	gl->m_next_scene = SceneData();
	gl->m_scene = SceneData();
	gl->DefaultTextures::Term();
	s_init_check_flag.m_value &= ~2;
	if (gl->m_render_target_pool != NULL) {
		delete gl->m_render_target_pool;
		gl->m_render_target_pool = NULL;
	}
	if (gl->m_quad_indices != NULL) {
		delete gl->m_quad_indices;
		gl->m_quad_indices = NULL;
	}
	if (gl->m_clear != NULL) {
		delete gl->m_clear;
		gl->m_clear = NULL;
	}
	if (gl->m_copy != NULL) {
		delete gl->m_copy;
		gl->m_copy = NULL;
	}
	gl->m_default_allocator.reset();
	gl->m_debug_allocator.reset();
	s_heap.reset();
	s_memory_mode = -1;
	sceGxmShaderPatcherDestroy(gl->m_patcher->m_patcher);
	gl->m_patcher.reset();
	GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, s_patcher_fragment_usse);
	GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, s_patcher_vertex_usse);
	GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, s_patcher_buffer);
	sceGxmDestroyContext(gl->m_context);
	gl->m_context = NULL;
	sce_paf_free(gl->m_context_params.hostMem);
	GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, s_fragment_usse_ring_buffer);
	if (s_fragment_ring_buffer != NULL)
		s_ring_heap->sce_paf_free(s_fragment_ring_buffer);
	if (s_vertex_ring_buffer != NULL)
		s_ring_heap->sce_paf_free(s_vertex_ring_buffer);
	if (s_vdm_ring_buffer != NULL)
		s_ring_heap->sce_paf_free(s_vdm_ring_buffer);
	s_ring_heap.reset();
	GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, s_ring_buffer);
}

ArlInstance::ArlInstance()
	: m_context(NULL), m_surface_width(0), m_surface_height(0), m_color_surface(NULL),
	  m_scene_requested(false), m_in_scene(false), m_clear(NULL), m_copy(NULL),
	  m_render_target_pool(NULL), m_quad_indices(NULL), m_reserved_value(0)
{
	for (uint8_t i = 0; i < sizeof(m_sequence); i++)
		m_sequence[i] = i + 1;
	m_reserved_flag = false;
	m_back_polygon_mode = POLYGON_MODE_FILL;
	m_front_polygon_mode = POLYGON_MODE_FILL;
	m_wclamp_enabled = false;
	m_wbuffer_enabled = false;
	m_two_sided_enabled = false;
	m_front_depth_func = DEPTH_NEVER;
	m_color = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	m_back_depth_func = DEPTH_NEVER;
	m_front_depth_write_enabled = false;
	m_back_depth_write_enabled = false;
	m_cull_mode = CULL_NONE;
	m_front_point_line_width = 0;
	m_back_point_line_width = 0;
	m_scissor_enabled = false;
	m_depth_test_enabled = true;
	m_blend_changed = true;
	m_blend_enabled = false;
	m_default_blend_info.colorMask = SCE_GXM_COLOR_MASK_ALL;
	m_blend_info.colorMask = SCE_GXM_COLOR_MASK_ALL;
	m_blend_info.colorFunc = SCE_GXM_BLEND_FUNC_ADD;
	m_blend_info.alphaFunc = SCE_GXM_BLEND_FUNC_ADD;
	m_blend_info.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
	m_blend_info.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	m_blend_info.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
	m_blend_info.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;
	m_default_blend_info.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
	m_default_blend_info.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	m_debug_color_shift = false;
	m_default_blend_info.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
	m_default_blend_info.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;
	m_debug_alpha_double = false;
	m_debug_surface_draw = false;
	m_default_blend_info.colorFunc = SCE_GXM_BLEND_FUNC_NONE;
	m_default_blend_info.alphaFunc = SCE_GXM_BLEND_FUNC_NONE;
	m_frame_start = get_time_internal();
	m_marker_depth = 0;
	m_marker = NULL;
}

ArlInstance::~ArlInstance()
{
}

ArlInstance *Gl()
{
	return s_instance;
}

}
}
}

using namespace paf;
using namespace paf::graph;
using namespace paf::graph::arl;

/* Sony's build calls these three rather than inlining them */
__attribute__((noinline)) void ScePafGraphics_A5C4B6AC(ArlInstance *gl, Rect rect,
						    int32_t reserved)
{
	if (gl->m_color_surface != NULL) {
		Rect clip(rect.m_x0, gl->m_surface_height - rect.m_y0, rect.m_x1,
			  gl->m_surface_height - rect.m_y1);

		sceGxmColorSurfaceSetClip(gl->m_color_surface, clip.m_x0, clip.m_y0, clip.m_x1,
					  clip.m_y1);
	}
}

__attribute__((noinline)) void ScePafGraphics_EA8C8FA0(ArlInstance *gl, SurfaceSize size,
						    SceGxmColorSurface *surface)
{
	gl->m_surface_width = size.m_width;
	gl->m_surface_height = size.m_height;
	gl->m_color_surface = surface;
	if (gl->m_scissor_enabled)
		ScePafGraphics_A5C4B6AC(gl, gl->m_viewport.Top().m_scissor, 0);
	else
		ScePafGraphics_A5C4B6AC(gl, Rect(0, 0, size.m_width, size.m_height), 0);
}

__attribute__((noinline)) void ScePafGraphics_AA7B8DB5(ArlInstance *gl, Rect rect, int32_t height)
{
	math::v2 half = math::v2((float)(rect.m_x1 - rect.m_x0), (float)(rect.m_y1 - rect.m_y0)) /
			2.0f;
	math::v2 center = math::v2((float)rect.m_x0, (float)rect.m_y0) + half;

	center.set_y((float)height - center.extract_y());
	sceGxmSetViewportEnable(gl->m_context, SCE_GXM_VIEWPORT_ENABLED);
	sceGxmSetViewport(gl->m_context, center.extract_x(), half.extract_x(), center.extract_y(),
			  -half.extract_y(), 0.5f, 0.5f);
}

void ScePafGraphics_38901781(const InitParam &param)
{
	s_instance = new ArlInstance();
	ScePafGraphics_9AA25028(s_instance, param);
}

void ScePafGraphics_501B8EB8(void)
{
	ScePafGraphics_38161BF3(s_instance);
	delete s_instance;
	s_instance = NULL;
}

void ScePafGraphics_56C831A9(ArlInstance *gl, const Rect &scissor)
{
	gl->m_viewport.Top().m_scissor = scissor;
}

void ScePafGraphics_93CBC733(ArlInstance *gl, const Rect &viewport)
{
	gl->m_viewport.Top().m_viewport = viewport;
	if (gl->m_in_scene)
		ScePafGraphics_AA7B8DB5(gl, gl->m_viewport.Top().m_viewport, gl->m_surface_height);
}

void ScePafGraphics_4581906A(ArlInstance *gl, SceGxmNotification *notification)
{
	if (gl->m_in_scene) {
		gl->m_in_scene = false;
		FragShaderObject::UnbindCurrent();
		VertShaderObject::UnbindCurrent();

		SceGxmNotification *vertex_notification = NULL;
		SceGxmNotification *fragment_notification = NULL;

		if (notification != NULL) {
			vertex_notification = notification;
			fragment_notification = notification + 1;
		}
		if (gl->m_scene.m_fbo != NULL)
			sceGxmEndScene(gl->m_context, vertex_notification, fragment_notification);
		else if (gl->m_scene.m_surface != NULL)
			static_cast<ScreenSurface *>(gl->m_scene.m_surface)
				->End(vertex_notification, fragment_notification);
		gl->m_marker = NULL;
		gl->m_markers.clear();
		gl->m_marker_depth = 0;
		gl->m_scene = SceneData();
		gl->m_surface_width = 0;
		gl->m_surface_height = 0;
		gl->m_color_surface = NULL;
	}
}

void ScePafGraphics_E7B32741(ArlInstance *gl)
{
	if (gl->m_scene.IsSet())
		ScePafGraphics_4581906A(gl, NULL);
}

namespace paf {
namespace graph {
namespace arl {

void ArlInstance::SetSceneData(intrusive_ptr<Surface> surface)
{
	SceneData old = m_next_scene;
	ScreenSurface *screen = static_cast<ScreenSurface *>(surface.get());

	m_next_scene.m_surface = screen;
	if (screen == NULL) {
		m_next_scene.m_memory = NULL;
	} else {
		m_next_scene.m_bounds = Rect(screen->m_scissor_x0, screen->m_scissor_y0,
					     screen->m_scissor_x1, screen->m_scissor_y1);
		m_next_scene.m_yuv_csc_index = screen->m_yuv_csc_index;
		m_next_scene.m_yuv_profile = screen->m_yuv_profile;

		int32_t page;

		screen->GetPageIndex(FrameBufferType_Back, &page);
		m_next_scene.m_memory = screen->Lock(page);
		screen->Unlock();
		m_next_scene.m_scale_mode = screen->m_scale_mode;
		m_next_scene.m_full_size = screen->m_full_size;
	}
	if (m_scene.IsSet() && old != m_next_scene)
		ScePafGraphics_E7B32741(this);
}

void ArlInstance::SetSceneData(FrameBufferObject *fbo)
{
	FrameBufferObject *old = m_next_scene.m_fbo;

	m_next_scene.m_fbo = fbo;
	if (m_scene.IsSet() && old != fbo)
		ScePafGraphics_E7B32741(this);
}

}
}
}

void ScePafGraphics_01CA92B7(ArlInstance *gl, uint32_t flags)
{
	if (gl->m_scene.m_fbo != NULL) {
		ScePafGraphics_EA8C8FA0(gl, gl->m_scene.m_fbo->GetSize(),
					gl->m_scene.m_fbo->GetColorSurface());

		SceGxmRenderTarget *target = gl->m_scene.m_fbo->GetRenderTarget();

		if (sceGxmBeginScene(gl->m_context, flags, target, NULL, NULL, NULL,
				     gl->m_scene.m_fbo->GetColorSurface(),
				     gl->m_scene.m_fbo->GetDepthStencilSurface()) == SCE_OK)
			gl->m_in_scene = true;
	} else {
		ScreenSurface *screen = static_cast<ScreenSurface *>(gl->m_scene.m_surface);

		if (screen != NULL) {
			SceGxmColorSurface *surface =
				screen->GetRawColorSurface(screen->GetPageCounter());
			Vector2 size;

			if (screen->m_scale_mode == 2 && !screen->m_full_size)
				size = Vector2((float)screen->GetWidth(), (float)screen->GetHeight()) /
				       2.0f;
			else
				size = Vector2((float)screen->GetWidth(), (float)screen->GetHeight());
			ScePafGraphics_EA8C8FA0(gl, SurfaceSize((int32_t)size.getX(),
								(int32_t)size.getY()),
						surface);
			if (screen->Begin(flags) == SCE_OK)
				gl->m_in_scene = true;
		} else {
			gl->m_surface_width = 0;
			gl->m_surface_height = 0;
			gl->m_color_surface = NULL;
		}
	}
}

void ScePafGraphics_199BD5F2(ArlInstance *gl, uint32_t flags)
{
	gl->m_scene_requested = true;
	if (gl->m_scene != gl->m_next_scene) {
		ScePafGraphics_E7B32741(gl);
		gl->m_scene = gl->m_next_scene;
		ScePafGraphics_01CA92B7(gl, flags);
	}
	ScePafGraphics_AA7B8DB5(gl, gl->m_viewport.Top().m_viewport, gl->m_surface_height);
}

void ScePafGraphics_29F5AA8B(ArlInstance *gl, SceGxmNotification *notification, bool end)
{
	gl->m_scene_requested = false;
	if (end)
		ScePafGraphics_4581906A(gl, notification);
}

namespace paf {
namespace graph {
namespace arl {

void ArlInstance::Color(math::v4 color)
{
	m_color = color;
	if (ScePafMisc_C51EF81D() & 4) {
		if (m_debug_color_shift) {
			math::v4 shift(0.2f, 0.2f, 0.2f, 0.0f);

			if (m_color.extract_x() + m_color.extract_y() + m_color.extract_z() < 1.6f)
				m_color = m_color - shift;
			else
				m_color = m_color + shift;
		}
		if (m_debug_alpha_double)
			m_color = mulPerElem(m_color.getImpl(), Vector4(1.0f, 1.0f, 1.0f, 2.0f));
	}
}

void ArlInstance::Enable(EnableCap cap)
{
	switch (cap) {
	case BLEND:
		if (!m_blend_enabled)
			m_blend_changed = true;
		m_blend_enabled = true;
		break;
	case DEPTH_TEST:
		sceGxmSetFrontDepthFunc(m_context, (SceGxmDepthFunc)m_front_depth_func);
		/* Sony's: the back face's function set on the front */
		sceGxmSetFrontDepthFunc(m_context, (SceGxmDepthFunc)m_back_depth_func);
		m_depth_test_enabled = true;
		break;
	case SCISSOR_TEST:
		m_scissor_enabled = true;
		break;
	case WCLAMP:
		sceGxmSetWClampEnable(m_context, SCE_GXM_WCLAMP_MODE_ENABLED);
		m_wclamp_enabled = true;
		break;
	case WBUFFER:
		sceGxmSetWBufferEnable(m_context, SCE_GXM_WBUFFER_ENABLED);
		m_wbuffer_enabled = true;
		break;
	case TWO_SIDED:
		sceGxmSetTwoSidedEnable(m_context, SCE_GXM_TWO_SIDED_ENABLED);
		m_two_sided_enabled = true;
		break;
	case DEBUG_COLOR_SHIFT:
		m_debug_color_shift = true;
		break;
	case DEBUG_ALPHA_DOUBLE:
		m_debug_alpha_double = true;
		break;
	case DEBUG_SURFACE_DRAW:
		m_debug_surface_draw = true;
		break;
	}
}

void ArlInstance::Disable(EnableCap cap)
{
	switch (cap) {
	case BLEND:
		if (m_blend_enabled)
			m_blend_changed = true;
		m_blend_enabled = false;
		break;
	case DEPTH_TEST:
		sceGxmSetFrontDepthFunc(m_context, SCE_GXM_DEPTH_FUNC_ALWAYS);
		sceGxmSetFrontDepthFunc(m_context, SCE_GXM_DEPTH_FUNC_ALWAYS);
		m_depth_test_enabled = false;
		break;
	case SCISSOR_TEST:
		m_scissor_enabled = false;
		break;
	case WCLAMP:
		sceGxmSetWClampEnable(m_context, SCE_GXM_WCLAMP_MODE_DISABLED);
		m_wclamp_enabled = false;
		break;
	case WBUFFER:
		sceGxmSetWBufferEnable(m_context, SCE_GXM_WBUFFER_DISABLED);
		m_wbuffer_enabled = false;
		break;
	case TWO_SIDED:
		sceGxmSetTwoSidedEnable(m_context, SCE_GXM_TWO_SIDED_DISABLED);
		m_two_sided_enabled = false;
		break;
	case DEBUG_COLOR_SHIFT:
		m_debug_color_shift = false;
		break;
	case DEBUG_ALPHA_DOUBLE:
		m_debug_alpha_double = false;
		break;
	case DEBUG_SURFACE_DRAW:
		m_debug_surface_draw = false;
		break;
	}
}

void ArlInstance::DepthMask(bool write_enabled, FaceEnum face)
{
	if (face & FRONT) {
		m_front_depth_write_enabled = write_enabled;
		sceGxmSetFrontDepthWriteEnable(m_context, write_enabled ?
						  SCE_GXM_DEPTH_WRITE_ENABLED :
						  SCE_GXM_DEPTH_WRITE_DISABLED);
	}
	if (face & BACK) {
		m_back_depth_write_enabled = write_enabled;
		sceGxmSetBackDepthWriteEnable(m_context, write_enabled ?
						 SCE_GXM_DEPTH_WRITE_ENABLED :
						 SCE_GXM_DEPTH_WRITE_DISABLED);
	}
}

void ArlInstance::ColorMask(bool r, bool g, bool b, bool a)
{
	uint8_t mask = (r ? SCE_GXM_COLOR_MASK_R : 0) | (g ? SCE_GXM_COLOR_MASK_G : 0) |
		       (b ? SCE_GXM_COLOR_MASK_B : 0) | (a ? SCE_GXM_COLOR_MASK_A : 0);

	if (m_blend_info.colorMask != mask) {
		m_blend_info.colorMask = mask;
		m_blend_changed = true;
	}
}

/* Sony's: the back face's function set on the front */
void ArlInstance::DepthFunc(DepthFuncEnum func, FaceEnum face)
{
	if (face & FRONT) {
		m_front_depth_func = func;
		if (m_depth_test_enabled)
			sceGxmSetFrontDepthFunc(m_context, (SceGxmDepthFunc)func);
	}
	if (face & BACK) {
		m_back_depth_func = func;
		if (m_depth_test_enabled)
			sceGxmSetFrontDepthFunc(m_context, (SceGxmDepthFunc)func);
	}
}

void ArlInstance::CullMode(CullModeEnum mode)
{
	m_cull_mode = mode;
	sceGxmSetCullMode(m_context, (SceGxmCullMode)mode);
}

void ArlInstance::BlendFunc(BlendingFactor src_rgb, BlendingFactor dst_rgb, BlendingFactor src_a,
			    BlendingFactor dst_a)
{
	if (m_blend_info.colorSrc != src_rgb || m_blend_info.alphaSrc != src_a ||
	    m_blend_info.colorDst != dst_rgb || m_blend_info.alphaDst != dst_a) {
		m_blend_changed = true;
		m_blend_info.colorSrc = src_rgb;
		m_blend_info.colorDst = dst_rgb;
		m_blend_info.alphaSrc = src_a;
		m_blend_info.alphaDst = dst_a;
	}
}

void ArlInstance::BlendEquation(BlendEquationEnum rgb, BlendEquationEnum a)
{
	if (m_blend_info.colorFunc != rgb || m_blend_info.alphaFunc != a) {
		m_blend_changed = true;
		m_blend_info.colorFunc = rgb;
		m_blend_info.alphaFunc = a;
	}
}

}
}
}

void ScePafGraphics_DAC195AC(ArlInstance *gl, SceGxmStencilFunc func,
			     SceGxmStencilOp stencil_fail, SceGxmStencilOp depth_fail,
			     SceGxmStencilOp depth_pass, uint8_t compare_mask, uint8_t write_mask,
			     FaceEnum face)
{
	if (face & FRONT)
		sceGxmSetFrontStencilFunc(gl->m_context, func, stencil_fail, depth_fail, depth_pass,
					  compare_mask, write_mask);
	if (face & BACK)
		sceGxmSetBackStencilFunc(gl->m_context, func, stencil_fail, depth_fail, depth_pass,
					 compare_mask, write_mask);
}

namespace paf {
namespace graph {
namespace arl {

void ArlInstance::PointSize(float value, FaceEnum face)
{
	uint32_t width = (uint32_t)value;

	if (face & FRONT) {
		m_front_point_line_width = width;
		sceGxmSetFrontPointLineWidth(m_context, width);
	}
	if (face & BACK) {
		m_back_point_line_width = width;
		sceGxmSetBackPointLineWidth(m_context, width);
	}
}

void ArlInstance::LineWidth(float value, FaceEnum face)
{
	uint32_t width = (uint32_t)value;

	if (face & FRONT) {
		m_front_point_line_width = width;
		sceGxmSetFrontPointLineWidth(m_context, width);
	}
	if (face & BACK) {
		m_back_point_line_width = width;
		sceGxmSetBackPointLineWidth(m_context, width);
	}
}

}
}
}

void ScePafGraphics_6E7B77AC(ArlInstance *gl, float value)
{
	sceGxmSetWClampValue(gl->m_context, value);
}

namespace paf {
namespace graph {
namespace arl {

void ArlInstance::PolygonMode(PolygonModeEnum mode, FaceEnum face)
{
	if (face & FRONT) {
		m_front_polygon_mode = mode;
		sceGxmSetFrontPolygonMode(m_context, (SceGxmPolygonMode)mode);
	}
	if (face & BACK) {
		m_back_polygon_mode = mode;
		sceGxmSetBackPolygonMode(m_context, (SceGxmPolygonMode)mode);
	}
}

void ArlInstance::Clear(uint32_t clear_mask)
{
	m_clear->clear(*this, clear_mask);
}

void ArlInstance::ClearColor(math::v4 color)
{
	m_clear->SetColor(color);
}

}
}
}

void ScePafGraphics_3B25B961(void)
{
}

void ScePafGraphics_65BACB67(void)
{
}

void ScePafGraphics_CFE5ACC9(void)
{
}

void ScePafGraphics_E4B3B1D2(ArlInstance *gl)
{
	sceGxmFinish(gl->m_context);
}

void ScePafGraphics_0240F825(ArlInstance *gl)
{
	common::internal::IncrFrameCount();
	gl->m_frame_time = (float)(get_time_internal() - gl->m_frame_start) / 1000000.0f;
	gl->m_frame_start = get_time_internal();
}

/* the surface the next scene draws into, held while it is read */
static inline intrusive_ptr<Surface> NextSurface(ArlInstance *gl)
{
	return intrusive_ptr<Surface>(gl->m_next_scene.m_surface);
}

SurfaceFlags ScePafGraphics_BB119805(ArlInstance *gl)
{
	ImageOrder order = NextSurface(gl)->GetOrder();
	SurfaceFlags flags;

	flags.Set(COLOR_SURFACE);
	switch (order) {
	case ImageOrder_Linear:
		flags.Set(COLOR_SURFACE_LINEAR);
		break;
	case ImageOrder_Swizzle:
		flags.Set(COLOR_SURFACE_SWIZZLED);
		break;
	case ImageOrder_Tiled:
		flags.Set(COLOR_SURFACE_TILED);
		break;
	default:
		flags.Set(COLOR_SURFACE_LINEAR);
		break;
	}
	flags.Set(COLOR_SURFACE_SCALE_NONE);
	return flags;
}

SurfaceSize ScePafGraphics_641E3FAE(ArlInstance *gl)
{
	return SurfaceSize(NextSurface(gl)->GetWidth(), NextSurface(gl)->GetHeight());
}

/* the surface's size in pixels, its width from its stride */
SurfaceSize ScePafGraphics_AC339F46(ArlInstance *gl)
{
	int32_t stride = NextSurface(gl)->GetStride();
	int32_t bytes = BytesPerPixel(NextSurface(gl)->m_format);

	return SurfaceSize(stride / bytes, NextSurface(gl)->GetHeight());
}

ImageMode ScePafGraphics_96173730(ArlInstance *gl)
{
	return NextSurface(gl)->m_format;
}

namespace paf {
namespace graph {
namespace arl {

void ArlInstance::CopyTexImage2D(common::SharedPtr<Texture2D> texture, int32_t level, int32_t x,
				 int32_t y, size_t width, size_t height)
{
	m_copy->CopyTexImage2D(*this, texture, texture->m_type, level, x, y, width, height);
}

common::SharedPtr<Texture2D> *ArlInstance::GetCopyTexture(uint32_t flags)
{
	return m_copy->GetTexture(*this, flags);
}

volatile uint32_t *ArlInstance::GetNotificationAddr(size_t index)
{
	return s_notification_region + index;
}

void ArlInstance::AddScenes(RenderTargetPool::Key key, int32_t scenes)
{
	m_render_target_pool->AddScenes(key, scenes);
}

static void *PatcherHostAlloc(void *user_data, uint32_t size)
{
	return sce_paf_malloc(size);
}

static void PatcherHostFree(void *user_data, void *mem)
{
	sce_paf_free(mem);
}

}
}
}
