/*
 * paf::ui::wutil (TOOL 0x811370cc..0x81137d40): the widget system's setup and shutdown, the
 * events posted from outside, the listeners it hands out, and the debug setup of a program
 * that runs without the Framework. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/color.h>
#include <graphics/drawobj/effect_path.h>
#include <common/common_internal.h>
#include <file/localfile.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_display.h>
#include <graphics/text/text_init.h>
#include <ime/ime.h>
#include <kernel/iofilemgr.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/motionsensor/motionsensor.h>
#include <inputdevice/pad/pad.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <memory/heap_allocator.h>
#include <std/math.h>
#include <std/stdlib.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/text_box_edit_menu.h>
#include <widget/text_box_ime_listener.h>
#ifdef PAF_HAS_WEBVIEW
#include <widget/web_ime_listener.h>
#endif
#include <widget/w_text_box.h>
#ifdef PAF_BUILD_2014
#include <widget/w_scene.h>
#endif
#include <widget/wutil.h>

namespace paf {
namespace ui {
namespace wutil {

#ifdef PAF_BUILD_2014
static gutil::FrameBufferMode s_fb_mode;
#endif
/* the Environment SetEnv was given */
static Environment *s_env;
/* the edit menu and the IME's listener Init made when it was given none (the others are
   the listeners it was given) */
static TextBoxEditMenu *s_edit_menu;
static TextBoxImeListener *s_ime_listener;
#ifdef PAF_HAS_WEBVIEW
static WebImeListener *s_web_ime_listener;
#endif
static TextBoxEditMenuListener *s_edit_menu_listener;
static TextBoxEditListener *s_edit_listener;
static void *s_web_edit_listener;
static MessageDialogListener *s_message_dialog_listener;
static void *s_listener_A55215E5;
static void *s_webview_listener;
static void *s_listener_E638AA1F;
#ifndef PAF_BUILD_2014
/* the default surface pool's memory when Init allocated it, and the device it came from */
static void *s_surface_pool_mem;
static GraphMem::DeviceType s_surface_pool_device_type;
#endif

InitParam::InitParam()
{
	surf_dev_mem_addr = NULL;
	surf_dev_mem_size = 0;
	surf_def_pool_addr = NULL;
	surf_def_pool_size = 0;
#ifdef PAF_BUILD_2014
	text_surface_pool_addr = NULL;
	text_surface_pool_size = 0;
	pvf_heap_size = 0;
	locale = -1;
	mem_pvf_japanese_addr = NULL;
	mem_pvf_japanese_size = 0;
	mem_pvf_latin_addr = NULL;
	mem_pvf_latin_size = 0;
	mem_pvf_hangul_addr = NULL;
	mem_pvf_hangul_size = 0;
	mem_pvf_chinese_addr = NULL;
	mem_pvf_chinese_size = 0;
	mem_emoji_font_addr = NULL;
	mem_emoji_font_size = 0;
	no_text = false;
	screen_width = 0;
	screen_height = 0;
	view_width = 0;
	view_height = 0;
	fov = 0.0f;
	env_param_num = 0;
	default_resolution_id = 0;
	env_param = NULL;
	input_device_mode = 0;
	input_handler_pad = NULL;
	input_handler_touch = NULL;
	input_handler_touch_rear = NULL;
	input_handler_motion_sensor = NULL;
	edit_listener = NULL;
	web_edit_listener = NULL;
#else
	surf_def_pool_device_type = 0;
	screen_width = 0;
	screen_height = 0;
	view_width = 0;
	view_height = 0;
	fov = 0.0f;
	text_param = NULL;
	input_device_mode = 0;
	input_handler_pad = NULL;
	input_handler_touch = NULL;
	input_handler_touch_rear = NULL;
	input_handler_motion_sensor = NULL;
	edit_listener = NULL;
	web_edit_listener = NULL;
#endif
}

#ifdef PAF_BUILD_2014
extern "C" {

InitParam *ScePafWidget_5245A0F2(InitParam *param, const gutil::WinHandle *whandle,
				 int32_t width, int32_t height, SceneResolution *env_param,
				 int32_t env_param_num, int32_t default_resolution_id)
{
	SceneResolution *found = NULL;

	param->whandle = *whandle;
	param->env_param = env_param;
	param->env_param_num = env_param_num;
	param->default_resolution_id = default_resolution_id;
	if (env_param != NULL && env_param_num > 0) {
		for (int32_t i = 0; i < env_param_num; i++) {
			if (env_param[i].m_id == (uint32_t)default_resolution_id) {
				found = &env_param[i];
				width = found->m_viewport_width;
				height = found->m_viewport_height;
				break;
			}
		}
	}
	if (width <= 0 || height <= 0) {
		param->screen_width = param->whandle.width;
		param->screen_height = param->whandle.height;
		param->view_width = param->whandle.width;
		param->view_height = param->whandle.height;
	} else {
		param->screen_width = width;
		param->screen_height = height;
		if (found != NULL) {
			param->view_width = found->m_scissor_width;
			param->view_height = found->m_scissor_height;
		} else {
			param->view_width = width;
			param->view_height = height;
		}
	}
	param->fov = 0.80285144f;
	return param;
}

}

namespace debug {

gutil::DevInitParam DevInit(uint32_t width, uint32_t height, SceneResolution *env_param,
			    int32_t env_param_num, int32_t default_resolution_id)
{
	gutil::DevInitParam dev_param;

	if (env_param != NULL && env_param_num > 0) {
		for (int32_t i = 0; i < env_param_num; i++) {
			if (env_param[i].m_id == (uint32_t)default_resolution_id) {
				width = env_param[i].m_screen_width;
				height = env_param[i].m_screen_height;
				break;
			}
		}
	}

	gutil::WinHandle whandle = gutil::debug::InitWindow(width, height, NULL);

	dev_param.whandle = whandle;
	dev_param.frame_buffer_mode = gutil::FrameBuffer_Normal;
	dev_param.graph_heap_size_on_main_memory = 0x100000;
	dev_param.graph_heap_size_on_video_memory = 0;
	dev_param.display_list_size = 0x100000;
	dev_param.parameter_buffer_size = 0x200000;
	dev_param.parameter_buffer_location = 0;
	return dev_param;
}

}
#else
extern "C" {

InitParam *ScePafWidget_5245A0F2(InitParam *param, const gutil::WinHandle *whandle)
{
	int32_t width = whandle->width;
	int32_t height = whandle->height;

	param->whandle.width = width;
	param->whandle.height = height;
	param->whandle.callback = whandle->callback;
	param->screen_width = width;
	param->screen_height = height;
	param->view_width = width;
	param->view_height = height;
	param->fov = 0.80285144f;
	return param;
}

}

namespace debug {

gutil::DevInitParam DevInit(uint32_t width, uint32_t height)
{
	gutil::DevInitParam dev_param;
	int32_t w = width;
	int32_t h = height;

	if (w == 0 || h == 0)
		graph::DisplaySurface::GetMaximumResolution(&w, &h);

	gutil::WinHandle whandle = gutil::debug::InitWindow(w, h, NULL);

	dev_param.parameter_buffer_size = 0x200000;
	dev_param.whandle = whandle;
	dev_param.display_list_size = 0x100000;
	return dev_param;
}

}

extern "C" {

InitParam ScePafWidget_63D72B1C(const gutil::WinHandle *whandle)
{
	InitParam param;

	param.whandle = *whandle;
	param.screen_width = whandle->width;
	param.screen_height = whandle->height;
	param.view_width = whandle->width;
	param.view_height = whandle->height;
	param.fov = 0.80285144f;

	static graph::TextInitParam text_param;

	param.text_param = &text_param;
	graph::s_page_opened = true;
	return param;
}

}
#endif

namespace debug {

int32_t Term(InitParam &param)
{
	if (param.surf_def_pool_addr != NULL)
		GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, param.surf_def_pool_addr);
	if (param.surf_dev_mem_addr != NULL)
		GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, param.surf_dev_mem_addr);
	s_env = NULL;
	return 0;
}

void DevTerm()
{
}

int32_t vec4_printf(char *str, const math::v4 &data)
{
	return sce_paf_printf_dummy();
}

int32_t LoadGimTexture(const string &path, intrusive_ptr<graph::Surface> &surface)
{
	int32_t error;
	common::SharedPtr<File> fp = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &error);

	if (fp.get() == NULL)
		return -1;

	memory::HeapAllocator heap(NULL, (uint32_t)fp->GetFileSize() + 1024, "ScePafHeap", NULL);
	size_t size = (uint32_t)fp->GetFileSize();
	void *buf = heap.sce_paf_memalign(16, (uint32_t)fp->GetFileSize());

	if (buf == NULL)
		return -1;
	if (fp->Read(buf, size) < 0) {
		heap.sce_paf_free(buf);
		return -1;
	}

	graph::Surface::LoadOption option;

	surface = graph::Surface::Load(gutil::GetDefaultSurfacePool(), buf, size, &option);
	if (surface.get() == NULL) {
		heap.sce_paf_free(buf);
		return option.error;
	}
	return 0;
}

int32_t LoadPngTexture(const string &path, intrusive_ptr<graph::Surface> &surface)
{
	int32_t error;
	common::SharedPtr<File> fp = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &error);

	if (fp.get() == NULL)
		return -1;

	memory::HeapAllocator heap(NULL, (uint32_t)fp->GetFileSize() + 1024, "ScePafHeap", NULL);
	size_t size = (uint32_t)fp->GetFileSize();
	void *buf = heap.sce_paf_memalign(16, size);

	if (buf == NULL)
		return -1;
	if (fp->Read(buf, size) < 0) {
		heap.sce_paf_free(buf);
		return -1;
	}

	graph::Surface::LoadOption option;

	surface = graph::Surface::Load(gutil::GetDefaultSurfacePool(), buf, size, &option);
	if (surface.get() == NULL) {
		heap.sce_paf_free(buf);
		return option.error;
	}
	if (fp->Close() < 0) {
		heap.sce_paf_free(buf);
		return -1;
	}
	return 0;
}

}

#ifdef PAF_BUILD_2014
int32_t DevInit(const gutil::DevInitParam &param)
{
	gutil::DevInitParam dev_param;

	dev_param.whandle.width = param.whandle.width;
	dev_param.whandle.height = param.whandle.height;
	s_fb_mode = param.frame_buffer_mode;
	dev_param.frame_buffer_mode = param.frame_buffer_mode;
	dev_param.graph_heap_size_on_main_memory = param.graph_heap_size_on_main_memory;
	dev_param.graph_heap_size_on_video_memory = param.graph_heap_size_on_video_memory;
	dev_param.display_list_size = param.display_list_size;
	dev_param.parameter_buffer_size = param.parameter_buffer_size;
	dev_param.parameter_buffer_location = param.parameter_buffer_location;
	if (param.frame_buffer_mode == gutil::FrameBuffer_Given)
		graph::s_page_opened = true;
	return gutil::DevInit(dev_param);
}

void DevTerm()
{
	gutil::DevTerm();
}

int32_t Init(InitParam &param)
{
	gutil::InitParam gparam;

	if (param.surf_def_pool_size == 0) {
		param.surf_def_pool_size = 0x800000;
		param.surf_def_pool_addr = GraphMem::AllocOSMemory(
			GraphMem::DeviceType_SystemMemory, 0x800000, "ScePafDefaultSurfacePool",
			NULL);
	}
	if (param.text_surface_pool_size == 0) {
		param.text_surface_pool_size = 0x100000;
		param.text_surface_pool_addr = GraphMem::AllocOSMemory(
			GraphMem::DeviceType_SystemMemory, 0x100000, "ScePafTextPool", NULL);
	}
	gparam.surf_dev_mem_addr = param.surf_dev_mem_addr;
	gparam.surf_dev_mem_size = param.surf_dev_mem_size;
	gparam.surf_def_pool_addr = param.surf_def_pool_addr;
	gparam.surf_def_pool_size = param.surf_def_pool_size;
	gparam.text_surface_pool_addr = param.text_surface_pool_addr;
	gparam.text_surface_pool_size = param.text_surface_pool_size;
	gparam.pvf_heap_size = param.pvf_heap_size;
	gparam.locale = param.locale;
	gparam.mem_pvf_japanese_addr = param.mem_pvf_japanese_addr;
	gparam.mem_pvf_japanese_size = param.mem_pvf_japanese_size;
	gparam.mem_pvf_latin_addr = param.mem_pvf_latin_addr;
	gparam.mem_pvf_latin_size = param.mem_pvf_latin_size;
	gparam.mem_pvf_hangul_addr = param.mem_pvf_hangul_addr;
	gparam.mem_pvf_hangul_size = param.mem_pvf_hangul_size;
	gparam.mem_pvf_chinese_addr = param.mem_pvf_chinese_addr;
	gparam.mem_pvf_chinese_size = param.mem_pvf_chinese_size;
	gparam.mem_emoji_font_addr = param.mem_emoji_font_addr;
	gparam.mem_emoji_font_size = param.mem_emoji_font_size;
	gparam.no_text = param.no_text;
	gparam.screen_width = param.screen_width;
	gparam.screen_height = param.screen_height;
	gparam.fov = ((float)param.view_width * 0.5f) / sce_paf_tanf(param.fov * 0.5f);

	int32_t result = gutil::Init(gparam);

	if (result < 0)
		return result;

	int32_t vp_width = param.screen_width;
	int32_t vp_height = param.screen_height;

	if (param.env_param != NULL && param.env_param_num > 0) {
		for (int32_t i = 0; i < param.env_param_num; i++) {
			if (param.env_param[i].m_id == (uint32_t)param.default_resolution_id) {
				param.whandle.width = param.env_param[i].m_screen_width;
				param.whandle.height = param.env_param[i].m_screen_height;
				vp_width = param.env_param[i].m_viewport_width;
				vp_height = param.env_param[i].m_viewport_height;
				break;
			}
		}
	}
	Screen::Init(param.whandle, vp_width, vp_height, s_fb_mode);

	inputdevice::InitParam input_param;

	input_param.m_mode = param.input_device_mode;
	inputdevice::Init(input_param);
	inputdevice::pad::SetDeviceHandler(NULL);
	inputdevice::touchscreen::SetDeviceHandler(0, NULL);
	inputdevice::touchscreen::SetDeviceHandler(1, NULL);
	inputdevice::motionsensor::SetDeviceHandler(
		(inputdevice::motionsensor::DeviceHandler)param.input_handler_motion_sensor);
	s_edit_listener = (TextBoxEditListener *)param.edit_listener;
	s_web_edit_listener = param.web_edit_listener;
	sceImeInit();
	if (s_edit_menu_listener == NULL) {
		s_edit_menu = new TextBoxEditMenu();
		if (s_edit_menu != NULL)
			s_edit_menu_listener = s_edit_menu;
	}
	if (s_edit_listener == NULL) {
		s_ime_listener = new TextBoxImeListener();
		if (s_ime_listener != NULL)
			s_edit_listener = s_ime_listener;
	}
#ifdef PAF_HAS_WEBVIEW
	if (s_web_edit_listener == NULL) {
		s_web_ime_listener = new WebImeListener();
		if (s_web_ime_listener != NULL)
			s_web_edit_listener = s_web_ime_listener;
	}
#endif
	s_message_dialog_listener = (MessageDialogListener *)param.message_dialog_listener;
	s_webview_listener = param.webview_listener;
	return 0;
}
#else
int32_t DevInit(const gutil::DevInitParam &param)
{
	if (gutil::GetFrameBufferMode() == gutil::FrameBuffer_Given)
		graph::s_page_opened = true;
	return gutil::DevInit(param);
}

void DevTerm()
{
	gutil::DevTerm();
}

int32_t Init(const InitParam &param)
{
	gutil::InitParam gparam;

	gparam.surf_def_pool_size = param.surf_def_pool_size;
	if (gparam.surf_def_pool_size == 0)
		gparam.surf_def_pool_size = 0x800000;
	gparam.surf_def_pool_addr = param.surf_def_pool_addr;
	s_surface_pool_mem = NULL;
	if (gparam.surf_def_pool_addr == NULL) {
		gparam.surf_def_pool_addr = GraphMem::AllocOSMemory(
			(GraphMem::DeviceType)param.surf_def_pool_device_type,
			gparam.surf_def_pool_size, "ScePafDefaultSurfacePool", NULL);
		if (gparam.surf_def_pool_addr == NULL)
			common_internal::on_error();
		s_surface_pool_mem = gparam.surf_def_pool_addr;
		s_surface_pool_device_type = (GraphMem::DeviceType)param.surf_def_pool_device_type;
	}

	float half_width = (float)param.view_width * 0.5f;

	gparam.surf_dev_mem_addr = param.surf_dev_mem_addr;
	gparam.surf_dev_mem_size = param.surf_dev_mem_size;
	gparam.screen_width = param.screen_width;
	gparam.screen_height = param.screen_height;
	float tan_half_fov = sce_paf_tanf(param.fov * 0.5f);
	gparam.text_param = param.text_param;
	gparam.fov = half_width / tan_half_fov;

	int32_t result = gutil::Init(gparam);

	if (result < 0)
		return result;

	gutil::WinHandle whandle = param.whandle;

	if (whandle.width <= 0 || whandle.height <= 0)
		graph::DisplaySurface::GetMaximumResolution(&whandle.width, &whandle.height);
	Screen::Init(whandle, 0, 0);

	inputdevice::InitParam input_param;

	input_param.m_mode = param.input_device_mode;
	inputdevice::Init(input_param);
	inputdevice::pad::SetDeviceHandler(NULL);
	inputdevice::touchscreen::SetDeviceHandler(0, NULL);
	inputdevice::touchscreen::SetDeviceHandler(1, NULL);
	inputdevice::motionsensor::SetDeviceHandler(
		(inputdevice::motionsensor::DeviceHandler)param.input_handler_motion_sensor);
	s_edit_listener = (TextBoxEditListener *)param.edit_listener;
	s_web_edit_listener = param.web_edit_listener;
	sceImeInit();
	if (s_edit_menu_listener == NULL) {
		s_edit_menu = new TextBoxEditMenu();
		if (s_edit_menu != NULL)
			s_edit_menu_listener = s_edit_menu;
	}
	if (s_edit_listener == NULL) {
		s_ime_listener = new TextBoxImeListener();
		if (s_ime_listener != NULL)
			s_edit_listener = s_ime_listener;
	}
#ifdef PAF_HAS_WEBVIEW
	if (s_web_edit_listener == NULL) {
		s_web_ime_listener = new WebImeListener();
		if (s_web_ime_listener != NULL)
			s_web_edit_listener = s_web_ime_listener;
	}
#endif
	s_message_dialog_listener = (MessageDialogListener *)param.message_dialog_listener;
	s_listener_A55215E5 = param.listener_A55215E5;
	s_webview_listener = param.webview_listener;
	s_listener_E638AA1F = param.listener_E638AA1F;
	return 0;
}
#endif

extern "C" {

/* the effects whose bits are set loaded from the folder */
int32_t ScePafWidget_BA6EB0C6(uint32_t effects, const string &dir)
{
	return gutil::LoadResource(effects, dir);
}

}

int32_t Term()
{
	int32_t result = Screen::Term();

	gutil::Term();
	inputdevice::End();
	if (s_edit_menu != NULL) {
		delete s_edit_menu;
		s_edit_menu = NULL;
	}
	if (s_ime_listener != NULL) {
		delete s_ime_listener;
		s_ime_listener = NULL;
	}
#ifdef PAF_HAS_WEBVIEW
	if (s_web_ime_listener != NULL) {
		delete s_web_ime_listener;
		s_web_ime_listener = NULL;
	}
#endif
	sceImeTerm();
	s_edit_menu_listener = NULL;
	s_edit_listener = NULL;
	s_web_edit_listener = NULL;
	s_message_dialog_listener = NULL;
	s_listener_A55215E5 = NULL;
	s_webview_listener = NULL;
	s_listener_E638AA1F = NULL;
#ifndef PAF_BUILD_2014
	if (s_surface_pool_mem != NULL) {
		GraphMem::FreeOSMemory(s_surface_pool_device_type, s_surface_pool_mem);
		s_surface_pool_mem = NULL;
	}
#endif
	return result;
}

extern "C" {

int32_t ScePafWidget_998F8F48(void)
{
	return 0;
}

/* the input devices updated */
int32_t ScePafWidget_909850C4(void)
{
	inputdevice::Update(0.0f);
	inputdevice::UpdateListener();
	return inputdevice::UpdateGesture(false);
}

int32_t ScePafWidget_095117B9(bool update_input)
{
	int32_t result;

	if (s_env != NULL)
		result = ScePafWidget_F67517A0(s_env);
	else
		result = ScePafWidget_0313D591();
	if (s_env != NULL)
		ScePafWidget_8E61FE0C(s_env);
	else
		ScePafWidget_323CEF70();
	if (update_input)
		ScePafWidget_909850C4();
	return result;
}

}

void SetEnv(Environment *env)
{
	s_env = env;
}

void PostTouchScreenEvent(Environment *env, int32_t type, int32_t state, int32_t x, int32_t y,
			  int32_t z, int32_t touch_id)
{
	if (env != NULL) {
		Event event(type, state, Event::ROUTE_FORWARD, 0, x, y, z, 0);

		event.SetDetail((touch_id & 0xF) << 20);
		env->m_ev_queue.PushBack(event);
	}
}

void PostKeycodeEvent(Environment *env, int32_t type, int32_t port, int32_t keydev,
		      int32_t keycode, int32_t modify, int32_t repeat, int32_t mode)
{
	if (env != NULL) {
		Event event(type, mode, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		event.SetDetail((uint8_t)keycode | Event::STATE_CONSUME);
		env->m_ev_queue.PushBack(event);
	}
}

void PostStateEvent(Environment *env, int32_t type, Handler *target)
{
	if (env != NULL && target != NULL) {
		Event event(type, target, Event::ROUTE_TERM, 0, 0, 0, 0, 0);

		env->m_ev_queue.PushBack(event);
	}
}

extern "C" {

TextBoxEditMenuListener *ScePafWidget_1EE9D37E(void)
{
	return s_edit_menu_listener;
}

}

TextBoxEditListener *GetEditListener()
{
	return s_edit_listener;
}

void *GetWebEditListener()
{
	return s_web_edit_listener;
}

MessageDialogListener *GetMessageDialogListener()
{
	return s_message_dialog_listener;
}

extern "C" {

void *ScePafWidget_A55215E5(void)
{
	return s_listener_A55215E5;
}

}

void *GetWebViewListener()
{
	return s_webview_listener;
}

extern "C" {

void *ScePafWidget_E638AA1F(void)
{
	return s_listener_E638AA1F;
}

}

}
}
}
