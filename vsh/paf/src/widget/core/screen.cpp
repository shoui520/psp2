/*
 * paf::ui::Screen (TOOL 0x81147828..0x81148566): a target the widgets are drawn to, a surface with
 * its viewport and scissor, the root screen and the stack of targets. No record or string names
 * the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/surface/surface_display.h>
#include <graphics/surface/surface_screen.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <math/rect.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <timer/timer.h>
#include <widget/core/environment.h>

namespace paf {
namespace ui {

Screen *Screen::s_root_screen;
graph::DisplaySurface *Screen::s_root_surface;
vector<Screen *> Screen::s_target_list((allocator<Screen *>()));
/* the vector made in the static initializer with the constructor inlined (name ours) */
static inline __attribute__((always_inline)) math::v4 Vector(float x, float y, float z, float w)
{
	return math::v4(x, y, z, w);
}

math::v4 Screen::s_draw_offset = Vector(0.0f, 0.0f, 0.0f, 0.0f);

intrusive_ptr<graph::Surface> Screen::SetSurface(intrusive_ptr<graph::Surface> surface)
{
	intrusive_ptr<graph::Surface> old = m_surface;

	m_surface = surface;
	if (m_surface.get() != NULL) {
		math::v2 size((float)m_surface->GetWidth(), (float)m_surface->GetHeight());

		vp = math::rect(math::v2(0.0f, 0.0f), size);
		sp = math::rect(math::v2(0.0f, 0.0f), size);
	}
	return old;
}

Screen::Screen()
	: clear_color(0.0f, 0.0f, 0.0f, 0.0f), is_auto_delete(true), is_cleared(false), vp(), sp(),
	  clear_policy(240), m_yuv_csc_index(0), m_yuv_profile(0)
{
	if (s_root_screen != NULL) {
		SetSurface(s_root_screen->m_surface);
		vp = math::rect(s_root_screen->vp);
		sp = math::rect(s_root_screen->sp);
	}
	sce_paf_memset(&m_whandle, 0, sizeof(m_whandle));
}

Screen::Screen(intrusive_ptr<graph::Surface> surface)
	: clear_color(0.0f, 0.0f, 0.0f, 0.0f), is_auto_delete(true), is_cleared(false), vp(), sp(),
	  clear_policy(240), m_yuv_csc_index(0), m_yuv_profile(0)
{
	SetSurface(surface);
	sce_paf_memset(&m_whandle, 0, sizeof(m_whandle));
}

Screen::Screen(Screen *base)
	: clear_color(0.0f, 0.0f, 0.0f, 0.0f), is_auto_delete(true), is_cleared(false), vp(), sp(),
	  clear_policy(240), m_yuv_csc_index(0), m_yuv_profile(0)
{
	if (base != NULL) {
		SetSurface(base->m_surface);
		vp = math::rect(base->vp);
		sp = math::rect(base->sp);
	}
	sce_paf_memset(&m_whandle, 0, sizeof(m_whandle));
}

Screen::~Screen()
{
	m_surface.clear();
}

math::rect Screen::ToSurfaceRect(const math::rect &rect)
{
	math::v2 origin(vp.width() * -0.5f, vp.height() * 0.5f);
	math::v2 direction(1.0f, -1.0f);
	math::v2 size((float)m_surface->GetWidth(), (float)m_surface->GetHeight());
	math::v2 scale(sce::Vectormath::Simd::Aos::divPerElem(size.getImpl(), vp.size().getImpl()));

	return rect.convert(origin, direction, scale);
}

math::rect Screen::ToPixelRect(const math::rect &rect)
{
	graph::Surface *surface = m_surface.get();
	math::v2 size((float)surface->GetWidth(), (float)surface->GetHeight());
	graph::DisplaySurface *display = (graph::DisplaySurface *)surface;

	if (display->m_scale_mode == 2 && !display->m_full_size)
		size = math::v2(sce::Vectormath::Simd::Aos::divPerElem(
			size.getImpl(), math::v2(2.0f, 2.0f).getImpl()));

	math::v2 origin(vp.width() * -0.5f, vp.height() * 0.5f);
	math::v2 direction(1.0f, -1.0f);
	math::v2 scale(sce::Vectormath::Simd::Aos::divPerElem(size.getImpl(), vp.size().getImpl()));

	return rect.convert(origin, direction, scale);
}

bool Screen::IsFullSize()
{
	if (m_surface.get() == NULL)
		return false;

	graph::Surface *surface = m_surface.get();
	math::v2 size((float)surface->GetWidth(), (float)surface->GetHeight());
	graph::DisplaySurface *display = (graph::DisplaySurface *)surface;

	if (display->m_scale_mode == 2 && !display->m_full_size)
		size = math::v2(sce::Vectormath::Simd::Aos::divPerElem(
			size.getImpl(), math::v2(2.0f, 2.0f).getImpl()));

	bool full = false;
	math::v2 vp_size = vp.size();

	if (math::v2((float)m_surface->GetWidth(), (float)m_surface->GetHeight()) == vp_size)
		full = size == vp.size();
	return full;
}

int32_t Screen::PushTarget(Screen *screen)
{
	if (screen == NULL)
		return -1;

	int32_t result = 0;
	Screen *top = NULL;

	if (s_target_list.size() != 0)
		top = s_target_list[s_target_list.size() - 1];
	if (top == screen) {
		result = 1;
	} else {
		if (top != NULL) {
			result = top->GetSurface()->EndScene();
			if (result < 0)
				return result;
		}

		graph::ScreenSurface *surface = (graph::ScreenSurface *)screen->GetSurface().get();
		math::rect rect = screen->ToSurfaceRect(screen->sp);

		surface->SetScissor(rect.left(), rect.bottom(), rect.right(), rect.top(), false);
		if (screen->m_yuv_csc_index < 2) {
			surface->m_yuv_csc_index = screen->m_yuv_csc_index;
			surface->m_yuv_profile = (SceGxmYuvProfile)screen->m_yuv_profile;
		}
		surface->BeginScene();
	}
	s_target_list.push_back(screen);
	return result;
}

int32_t Screen::OnTarget()
{
	return PushTarget(this);
}

void Screen::OnClear()
{
	if (clear_policy & 0xF0) {
		graph::Surface *surface = m_surface.get();
		uint32_t mask = 0;

		if (clear_policy & 0x10)
			mask = 4;
		if (clear_policy & 0x20)
			mask |= 1;
		if (surface != NULL && !surface->IsCleared())
			surface->Clear(clear_color, mask);
	}
}

int32_t Screen::PopTarget(Screen *screen)
{
	if (screen == NULL)
		return -1;

	Screen *top = NULL;

	s_target_list.pop_back();
	if (s_target_list.size() != 0)
		top = s_target_list[s_target_list.size() - 1];
	if (top == screen)
		return 1;

	int32_t result = screen->GetSurface()->EndScene();

	if (result >= 0 && top != NULL) {
		graph::ScreenSurface *surface = (graph::ScreenSurface *)top->GetSurface().get();
		math::rect rect = top->ToSurfaceRect(top->sp);

		surface->SetScissor(rect.left(), rect.bottom(), rect.right(), rect.top(), false);
		surface->BeginScene();
	}
	return result;
}

void Screen::OnFinish()
{
	PopTarget(this);
}

void Screen::SetRootScreen(Screen *screen)
{
	if (s_root_screen != NULL)
		delete s_root_screen;
	s_root_screen = screen;
	if (screen != NULL) {
		screen->is_auto_delete = false;

		intrusive_ptr<graph::Surface> surface = s_root_screen->m_surface;

		s_root_surface = (graph::DisplaySurface *)surface.get();
	} else {
		s_root_surface = NULL;
	}
}

int32_t Screen::Init(gutil::WinHandle whandle, int32_t vp_width, int32_t vp_height)
{
	graph::ScreenSurface::Param param;

	param.width = whandle.width;
	param.height = whandle.height;
	param.format = (ImageMode)4;
	param.order = (ImageOrder)1;
	param.num_page = 2;
	param.depth_format = (ImageMode)100;
	param.depth_order = (ImageOrder)1;
	param.scale_mode = 0;
	param.full_size = false;

	gutil::FrameBufferMode mode = gutil::GetFrameBufferMode();
	void *bp = graph::DisplaySurface::AllocFrameBuffer(mode, &param);
	intrusive_ptr<graph::Surface> surface = new graph::DisplaySurface(mode, &param, bp);

	if (vp_width <= 0 || vp_height <= 0) {
		vp_width = surface->GetWidth();
		vp_height = surface->GetHeight();
	}

	Screen *screen = new Screen(surface);

	screen->m_whandle = whandle;
	screen->is_auto_delete = false;
	SetRootScreen(screen);
	screen->vp = math::rect(0.0f, 0.0f, (float)vp_width, (float)vp_height);
	screen->sp = math::rect(0.0f, 0.0f, (float)vp_width, (float)vp_height);
	graph::arl::Gl()->SetSceneData(surface);
	inputdevice::touchscreen::SetScreenResolution(whandle.width, whandle.height);
	inputdevice::touchscreen::SetScreenViewport(0, 0, vp_width, vp_height);
	if (s_target_list.capacity() < 16)
		s_target_list.reserve(16);
	s_target_list.clear();
	return 0;
}

int32_t Screen::Term()
{
	s_target_list.clear();
	if (s_root_screen != NULL)
		delete s_root_screen;
	s_root_screen = NULL;
	s_root_surface = NULL;
	return 0;
}


extern "C" {

/* the frame on the root surface ended and shown (the screen's swap before its targets are given
   up); -1 for no root screen or surface, else the frame counted */
int32_t ScePafWidget_0313D591(void)
{
	int32_t result;

	if (Screen::s_root_screen == NULL)
		return -1;
	if (Screen::s_root_surface == NULL)
		return -1;
	Screen::s_root_surface->EndFrame();
	Screen::s_root_surface->Swap();
	ScePafMisc_4357D271();
	result = 0;
	if (Screen::s_root_screen->m_whandle.width > 0)
		result = ScePafGraphics_27416D74(Screen::s_root_screen->m_whandle.width,
						 Screen::s_root_screen->m_whandle.height,
						 Screen::s_root_screen->m_whandle.callback);
	Screen::s_root_surface->WaitFrame();
	return result;
}

/* the targets given up, and the next frame on the root surface started */
int32_t ScePafWidget_323CEF70(void)
{
	ScePafGraphics_C5504119();
	ScePafMisc_EF1C440B();
	Screen::s_target_list.clear();
	if (Screen::s_root_surface != NULL)
		Screen::s_root_surface->StartFrame();
	return 0;
}

}

int32_t Screen::Swap()
{
	int32_t result;

	if (s_root_screen == NULL) {
		result = -1;
	} else if (s_root_surface == NULL) {
		result = -1;
	} else {
		s_root_surface->EndFrame();
		s_root_surface->Swap();
		ScePafMisc_4357D271();
		result = 0;
		if (s_root_screen->m_whandle.width > 0)
			result = ScePafGraphics_27416D74(s_root_screen->m_whandle.width,
							s_root_screen->m_whandle.height,
							s_root_screen->m_whandle.callback);
		s_root_surface->WaitFrame();
	}
	ScePafGraphics_C5504119();
	ScePafMisc_EF1C440B();
	s_target_list.clear();
	if (s_root_surface != NULL)
		s_root_surface->StartFrame();
	return result;
}

}
}
