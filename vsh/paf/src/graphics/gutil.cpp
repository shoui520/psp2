/*
 * paf::gutil: GXM, the graphics memory and the renderer set up for the Framework; the debug
 * heap, window and GUI functions of a program that runs without it; and the default surface
 * pool and shader (the file's name ours, after the header).
 */
#include <graphics/arl/arl_defines.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/color.h>
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <kernel/threadmgr.h>
#include <common/common_internal.h>
#include <common/shared_ptr.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface_display.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/text/text_init.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v4.h>
#include <memory/heap_allocator.h>
#include <std/string.h>
#include <std/vector>
#include <src/common/frame_count.h>
#include <src/std/math.h>

extern "C" {
int sceGxmVshInitialize(const SceGxmInitializeParams *params);
int sceGxmIsInitializationInternal(void);
}

namespace paf {
namespace gutil {

/* what gutil keeps between Init and Term (names ours) */
class State {
public:
	State() : m_debug_heap(NULL), m_gxm_initialized(false), m_frame_buffer_mode(FrameBuffer_Normal),
		  m_default_surface_pool(NULL)
	{
	}

	memory::HeapAllocator *m_debug_heap;
	bool m_gxm_initialized;
	FrameBufferMode m_frame_buffer_mode;
	InitParam m_param;
	graph::SurfacePool *m_default_surface_pool;
	common::SharedPtr<graph::Shader> m_default_shader;
};

/* a function the debug GUI calls each frame with its userdata (names ours) */
class DebugGuiFunc {
public:
	DebugGuiFunc(void (*func)(void *), void *userdata) : m_func(func), m_userdata(userdata)
	{
	}

	void (*m_func)(void *);
	void *m_userdata;
};

static State s_state;
static vector<DebugGuiFunc> s_debug_gui_funcs((allocator<DebugGuiFunc>()));

InitParam::InitParam()
	: surf_dev_mem_addr(NULL), surf_dev_mem_size(0), surf_def_pool_addr(NULL),
	  surf_def_pool_size(0), screen_width(0), screen_height(0), fov(0.0f), text_param(NULL)
{
}

DevInitParam::DevInitParam()
	: parameter_buffer_size(0), parameter_buffer_location(0),
	  graph_heap_size_on_main_memory(0x100000), graph_heap_size_on_video_memory(0),
	  display_list_size(0)
{
}

void InitParam::Free()
{
	GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, surf_def_pool_addr);
	surf_def_pool_addr = NULL;
	GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, surf_dev_mem_addr);
	surf_dev_mem_addr = NULL;
}

namespace debug {

void DevTerm()
{
}

void Term(InitParam &param)
{
	param.Free();
	if (s_state.m_debug_heap != NULL) {
		delete s_state.m_debug_heap;
		s_state.m_debug_heap = NULL;
	}
}

InitParam Init(int32_t width, int32_t height)
{
	s_state.m_debug_heap = new memory::HeapAllocator(NULL, 0x800000, "ScePafHeap", NULL);

	if (s_state.m_debug_heap == NULL)
		return InitParam();

	InitParam param;

	param.surf_def_pool_addr = GraphMem::AllocOSMemory(GraphMem::DeviceType_SystemMemory, 0x200000,
							    "ScePafDbgSurfacePool", NULL);
	param.surf_def_pool_size = 0x200000;
	param.screen_width = width;
	param.screen_height = height;
	return param;
}

void AddDebugGuiFunc(void (*func)(void *), void *userdata)
{
	vector<DebugGuiFunc>::iterator end = s_debug_gui_funcs.end();
	vector<DebugGuiFunc>::iterator it = s_debug_gui_funcs.begin();

	for (; it != end; ++it) {
		if (it->m_func == func && it->m_userdata == userdata)
			break;
	}
	if (it == end)
		s_debug_gui_funcs.push_back(DebugGuiFunc(func, userdata));
}

void RemoveDebugGuiFunc(void (*func)(void *), void *userdata)
{
	vector<DebugGuiFunc>::iterator end = s_debug_gui_funcs.end();
	vector<DebugGuiFunc>::iterator it = s_debug_gui_funcs.begin();

	for (; it != end; ++it) {
		if (it->m_func == func && it->m_userdata == userdata)
			break;
	}
	if (it != end)
		s_debug_gui_funcs.erase(it);
}

void RemoveDebugGuiFuncs(void (*func)(void *))
{
	vector<DebugGuiFunc>::iterator it = s_debug_gui_funcs.begin();

	while (it != s_debug_gui_funcs.end()) {
		if (it->m_func == func)
			it = s_debug_gui_funcs.erase(it);
		else
			++it;
	}
}

void CallDebugGuiFuncs()
{
	for (vector<DebugGuiFunc>::iterator it = s_debug_gui_funcs.begin();
	     it != s_debug_gui_funcs.end(); ++it)
		it->m_func(it->m_userdata);
}

WinHandle InitWindow(int32_t width, int32_t height, void *callback)
{
	WinHandle whandle;

	*graph::arl::GetTemporaryInitCheckFlag() |= 1;
	whandle.width = width;
	whandle.height = height;
	whandle.callback = callback;
	return whandle;
}

DevInitParam DevInit(int32_t width, int32_t height, void *window)
{
	DevInitParam param;

	param.parameter_buffer_size = 0x200000;
	param.whandle.width = (int32_t)window;
	param.whandle.height = (int32_t)window;
	param.whandle.callback = window;
	param.display_list_size = 0x100000;
	return param;
}

void WaitDummyVSync()
{
}

}

void DevTerm()
{
	ScePafGraphics_501B8EB8();
	GraphMem::Exit();
	if (s_state.m_frame_buffer_mode != FrameBuffer_Given && s_state.m_gxm_initialized) {
		sceGxmTerminate();
		s_state.m_gxm_initialized = false;
	}
}

/* the projection made again from the camera's field of view for an aspect, unless it is
   fixed */
static inline __attribute__((always_inline)) void SetCameraAspect(graph::Camera *camera,
								   float aspect)
{
	graph::Camera::Projection &projection = camera->m_projection;

	if (projection.m_fixed)
		return;

	float fov = projection.m_fov;
	float near_z = projection.m_near;
	int32_t fov_axis = projection.m_fov_axis;
	float far_z = projection.m_far;
	float t = sce_paf_tanf(fov * 0.5f);
	float left;
	float right;
	float bottom;
	float top;

	if (fov_axis == 1) {
		top = near_z * t;
		bottom = -(near_z * t);
		right = top * aspect;
		left = -(top * aspect);
	} else {
		right = near_z * t;
		left = -(near_z * t);
		top = right / aspect;
		bottom = -top;
	}

	math::matrix matrix = math::matrix::Frustum(left, right, bottom, top, near_z, far_z);

	projection.m_matrix = matrix;
	projection.m_near = near_z;
	projection.m_far = far_z;
	projection.m_left = left;
	projection.m_right = right;
	projection.m_bottom = bottom;
	projection.m_top = top;
	projection.m_fov = fov;
	projection.m_fov_axis = fov_axis;
	projection.m_aspect = aspect;
	projection.m_changed = false;
	projection.m_fixed = false;
}

/* the horizontal field of view */
static inline __attribute__((always_inline)) float GetCameraFovX(graph::Camera *camera)
{
	graph::Camera::Projection &projection = camera->m_projection;

	if (projection.m_fov_axis != 0)
		return sce_paf_atanf(sce_paf_tanf(projection.m_fov * 0.5f) * projection.m_aspect) * 2.0f;
	return projection.m_fov;
}

int32_t Init(const InitParam &param)
{
	graph::arl::ArlInstance *gl = graph::arl::Gl();
	graph::arl::Rect viewport;

	viewport.m_x1 = param.screen_width;
	viewport.m_y1 = param.screen_height;
	ScePafGraphics_93CBC733(gl, viewport);
	ScePafGraphics_1C294786();

	float width = (float)param.screen_width;
	float height = (float)param.screen_height;

	ScePafGraphics_4E038C05()->m_ptr->m_camera->Reset(1.0f);
	ScePafGraphics_4E038C05()->m_ptr->m_camera_2d->Reset(1.0f);

	float aspect = width / height;
	float half_width = width * 0.5f;

	SetCameraAspect(ScePafGraphics_4E038C05()->m_ptr->m_camera.get(), aspect);
	SetCameraAspect(ScePafGraphics_4E038C05()->m_ptr->m_camera_2d.get(), aspect);

	float fov_x = GetCameraFovX(ScePafGraphics_4E038C05()->m_ptr->m_camera.get());
	math::v4 eye = math::v4(sce::Vectormath::Simd::Aos::Vector4::zAxis()) * half_width;

	ScePafGraphics_4E038C05()->m_ptr->m_camera->SetPosition(eye / sce_paf_tanf(fov_x * 0.5f));
	fov_x = GetCameraFovX(ScePafGraphics_4E038C05()->m_ptr->m_camera_2d.get());
	ScePafGraphics_4E038C05()->m_ptr->m_camera_2d->SetPosition(eye / sce_paf_tanf(fov_x * 0.5f));

	if (param.surf_def_pool_size != 0)
		s_state.m_default_surface_pool = new graph::SurfacePool(
			param.surf_def_pool_addr, param.surf_def_pool_size,
			"ScePafGUtilDefaultSurfacePool", NULL, graph::SurfacePool::MemoryType_Normal);
	s_state.m_default_shader = common::SharedPtr<graph::Shader>(new graph::DefaultShader());
	s_state.m_param = param;
	if (param.text_param != NULL)
		graph::InitText(*param.text_param);
	return 0;
}

int32_t LoadResource(uint32_t flags, const string &path)
{
	ScePafGraphics_4E038C05()->m_ptr->m_resource_path = path;
	if (flags & 1)
		ScePafGraphics_4E038C05()->m_ptr->GetWaveRenderer(true);
	if (flags & 4)
		ScePafGraphics_4E038C05()->m_ptr->GetPageTurnRenderer(true);
	if (flags & 2)
		ScePafGraphics_4E038C05()->m_ptr->GetPanelRenderer(true);
	if (flags & 8)
		ScePafGraphics_71E2CB77(ScePafGraphics_4E038C05()->m_ptr, true);
	if (flags & 0x10) {
		ScePafGraphics_4E038C05()->m_ptr->GetCustomRenderer(true);
		ScePafGraphics_4E038C05()->m_ptr->GetCustomRenderer(false)->GetOption()->SetEnabled(
			false);
		ScePafGraphics_4E038C05()->m_ptr->GetCustomRenderer(false)->GetOption()->SetEnabled(
			false);
	}
	return 0;
}

graph::SurfacePool *GetDefaultSurfacePool()
{
	return s_state.m_default_surface_pool;
}

common::SharedPtr<graph::Shader> *GetDefaultShader()
{
	return &s_state.m_default_shader;
}

void SetFrameBufferMode(FrameBufferMode mode)
{
	s_state.m_frame_buffer_mode = mode;
	GraphMem::SetMode(mode == FrameBuffer_Given ? GraphMem::ModeType_CommonDialog :
						      GraphMem::ModeType_Normal);
}

FrameBufferMode GetFrameBufferMode()
{
	return s_state.m_frame_buffer_mode;
}

int32_t InitGxm(const DevInitParam &param)
{
	if (s_state.m_frame_buffer_mode == FrameBuffer_Given) {
		int32_t count = 0;

		while (sceGxmIsInitializationInternal() == 0) {
			sceKernelDelayThread(5000);
			if (++count > 200)
				common_internal::on_error();
		}
		return 0;
	}
	if (s_state.m_gxm_initialized)
		return 0;

	SceGxmInitializeParams params;

	sce_paf_memset(&params, 0, sizeof(params));
	params.parameterBufferSize = param.parameter_buffer_size;
	params.flags = 2;
	params.displayQueueMaxPendingCount = 1;
	switch (s_state.m_frame_buffer_mode) {
	case FrameBuffer_Normal:
		params.displayQueueCallback = graph::DisplaySurface::displayCallback;
		params.displayQueueCallbackDataSize = 40;
		break;
	case FrameBuffer_Shared_Create:
		params.flags = 6;
		params.displayQueueCallback = graph::DisplaySurface::displayCallback;
		params.displayQueueCallbackDataSize = 40;
		break;
	case FrameBuffer_Shared_Share:
	case FrameBuffer_Shared_OffScreen:
		params.flags = 10;
		break;
	default:
		break;
	}

	int32_t ret = sceGxmVshInitialize(&params);

	s_state.m_gxm_initialized = true;
	return ret;
}

int32_t DevInit(const DevInitParam &param)
{
	InitGxm(param);
	GraphMem::Init(param.graph_heap_size_on_main_memory, param.graph_heap_size_on_video_memory);

	graph::arl::InitParam arl_param;

	arl_param.m_ring_buffer_size = param.display_list_size;
	arl_param.m_memory_mode = s_state.m_frame_buffer_mode;
	ScePafGraphics_38901781(arl_param);
	common::internal::ResetFrameCount();
	return 0;
}

void Term()
{
	graph::TermText();
	if (s_state.m_default_shader.get() != NULL)
		s_state.m_default_shader.reset();
	if (s_state.m_default_surface_pool != NULL) {
		delete s_state.m_default_surface_pool;
		s_state.m_default_surface_pool = NULL;
	}
	ScePafGraphics_0B4F6C24();
}

}
}

using namespace paf;

int32_t ScePafGraphics_C5504119(void)
{
	return 0;
}

int32_t ScePafGraphics_27416D74(gutil::WinHandle whandle)
{
	static uint32_t s_frame_count = 0;

	ScePafGraphics_0240F825(graph::arl::Gl());
	if (s_frame_count != common::FrameCount()) {
		s_frame_count = common::FrameCount();
		gutil::debug::Mouse::GetInstance()->Update();
		gutil::debug::Keyboard::GetInstance()->Read();
		gutil::debug::Keyboard::GetInstance()->Update();

		gutil::debug::Mouse *mouse = gutil::debug::Mouse::GetInstance();
		graph::arl::Rect &viewport = graph::arl::Gl()->m_viewport.Top().m_viewport;
		int32_t width = viewport.m_x1 - viewport.m_x0;
		graph::arl::Rect &viewport2 = graph::arl::Gl()->m_viewport.Top().m_viewport;
		int32_t height = viewport2.m_y1 - viewport2.m_y0;

		mouse->m_screen_width = width;
		mouse->m_screen_height = height;
	}
	return 0;
}

namespace paf {
namespace gutil {

int32_t Swap(WinHandle whandle)
{
	return ScePafGraphics_27416D74(whandle);
}

}
}
