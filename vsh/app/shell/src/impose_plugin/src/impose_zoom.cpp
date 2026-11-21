/*
 * impose_zoom.cpp: impose_plugin's screen zoom (0x81151B08-0x81152770): a page of its own with
 * a plane the screen's capture is shown on, zoomed in from a point or out to it and faded out,
 * and a dimmer over it while the zoom is held. The page is opened by the first user and closed
 * by the last. The file's, the class's and the other names are ours (PLACEHOLDER), after the
 * interface's zoom entries and the widget names "screenshot" and "news_temporary_dimmer".
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/memory>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <math/math.h>
#include <common/transition.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <appmgr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../shell.h"
#include "impose.h"

using namespace paf;

extern "C" int sceAppMgrGetAppMgrState(SceAppMgrAppState *state);

/* the page (PLACEHOLDER name) */
#define ZOOM_PAGE		0xA28670CF
/* the page's draw priority */
#define ZOOM_PRIORITY		4

/* the capture's flags: while an application's events are pending, else (PLACEHOLDER names) */
#define ZOOM_CAPTURE_APP	4098
#define ZOOM_CAPTURE_DEFAULT	-5

/* the zoom: the plane's scale when it shows the whole screen at the point, and the timers */
#define ZOOM_IN_SCALE		0.74333333f
#define ZOOM_OUT_SCALE		1.3636364f
#define ZOOM_IN_TIME		250.0f
#define ZOOM_OUT_TIME		300.0f
#define ZOOM_FADE_TIME		200.0f

/* (0x815448F4) */
static ImposeZoom *s_zoom;

/* (0x81151B08) */
ImposeZoom::ImposeZoom() : m_page(NULL), m_plane(NULL), m_count(0)
{
	s_zoom = this;
}

/* (0x81151B20) */
ImposeZoom::~ImposeZoom()
{
	Plugin::PageCloseParam param;

	g_imposePlugin->PageClose(IDParam(ZOOM_PAGE), param);
	m_page = NULL;
	s_zoom = NULL;
}

/* the page opened by its first user, with the plane the capture is shown on (0x81151BDE) */
__attribute__((noinline)) ui::Scene *ImposeZoom::Acquire()
{
	if (m_page == NULL) {
		Plugin::PageOpenParam param;

		param.overwrite_draw_priority = ZOOM_PRIORITY;
		m_page = g_imposePlugin->PageOpen(IDParam(ZOOM_PAGE), param);
		m_plane = g_imposePlugin->CreateWidget(m_page, "plane", IDParam("screenshot"),
						       IDParam());
		m_plane->SetSize(math::v4(960.0f, 544.0f, 0.0f, 0.0f));
	}
	m_count++;
	return m_page;
}

/* the page closed by its last user (0x81151FBC) */
__attribute__((noinline)) void ImposeZoom::Release()
{
	if (--m_count <= 0) {
		Plugin::PageCloseParam param;

		g_imposePlugin->PageClose(IDParam(ZOOM_PAGE), param);
		m_page = NULL;
		m_plane = NULL;
	}
}

/* the dimmer shown over the page (0x81151DC0) */
void ImposeZoom::Begin()
{
	ui::Scene *page = Acquire();

	if (page != NULL) {
		ui::Widget *dimmer = page->FindChild(IDParam("news_temporary_dimmer"), 0);

		if (dimmer == NULL) {
			dimmer = g_imposePlugin->CreateWidget(page, "plane",
							      IDParam("news_temporary_dimmer"),
							      IDParam());
			dimmer->SetColor(math::v4(0.9f, 0.5f, 0.5f, 0.0f));
			dimmer->SetAdjust(4, 4, 0);
			ScePafWidget_42C5A4D4(dimmer, 0, 0);
		}
		ScePafWidget_44B22ACA(dimmer, common::transition::Type_FadeinSlow, 0.0f, NULL, NULL);
	}
}

/* the dimmer taken off (0x81152072) */
void ImposeZoom::End()
{
	ui::Scene *page = m_page;

	if (page != NULL) {
		ui::Widget *dimmer = page->FindChild(IDParam("news_temporary_dimmer"), 0);

		dimmer->DestroyWidget();
	}
	Release();
}

/* the screen captured into the surface (0x81152130) */
void ImposeZoom::Prepare()
{
	Shell_81244BB0();
	m_surface.clear();

	int flags = ZOOM_CAPTURE_DEFAULT;
	SceAppMgrAppState state;

	if (sceAppMgrGetAppMgrState(&state) == 0 && state.appEventNum != 0)
		flags = ZOOM_CAPTURE_APP;
	shellScreenshotCaptureSurface(&m_surface, flags);
}

/* the zoom has faded out: the capture let go of (0x81152186) */
__attribute__((noinline)) void ImposeZoom::Finish()
{
	m_plane->SetTexture(intrusive_ptr<graph::Surface>());
	m_surface.clear();
	shellAppStateMarkForeground();
	Release();
}

static void imposeZoomOnFaded(Timer *t, ui::AnimListener *listener, void *data1, void *data2,
			      void *data3);

/* the capture zoomed in from the point to the whole screen, then faded out (0x811521DC) */
void ImposeZoom::Show(int x, int y)
{
	if (m_plane != NULL && (m_plane->FindAnim(ui::Widget::ANIM_SCALE) != NULL ||
				m_plane->FindAnim(ui::Widget::ANIM_META_ALPHA) != NULL))
		Release();
	Acquire();
	m_plane->SetMetaAlpha(1.0f);
	m_plane->SetPos(math::v4((float)x, (float)y, 0.0f, 0.0f));
	m_plane->SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
			new Timer(ZOOM_IN_TIME, Timer::FUNC_CUBIC_IN));
	m_plane->SetTexture(m_surface);
	m_plane->SetScale(math::v4(ZOOM_IN_SCALE, ZOOM_IN_SCALE, 0.0f, 0.0f));
	m_plane->SetScale(math::v4(1.0f, 1.0f, 0.0f, 0.0f),
			  new Timer(ZOOM_IN_TIME, Timer::FUNC_CUBIC_IN));
	m_plane->SetMetaAlpha(0.0f, new Timer(ZOOM_FADE_TIME), ui::Widget::ANIM_META_ALPHA, 0,
			      ZOOM_IN_TIME, imposeZoomOnFaded, NULL);
}

/* the capture zoomed out from the whole screen to the point, then faded out (0x81152468) */
void ImposeZoom::Hide(int x, int y)
{
	if (m_plane != NULL && (m_plane->FindAnim(ui::Widget::ANIM_SCALE) != NULL ||
				m_plane->FindAnim(ui::Widget::ANIM_META_ALPHA) != NULL))
		Release();
	Acquire();
	m_plane->SetMetaAlpha(1.0f);
	m_plane->SetPos(math::v4((float)-x * ZOOM_OUT_SCALE, (float)-y * ZOOM_OUT_SCALE, 0.0f,
				 0.0f));
	m_plane->SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
			new Timer(ZOOM_OUT_TIME, Timer::FUNC_CUBIC_IN));
	m_plane->SetTexture(m_surface);
	m_plane->SetScale(math::v4(ZOOM_OUT_SCALE, ZOOM_OUT_SCALE, ZOOM_OUT_SCALE, 0.0f));
	m_plane->SetScale(math::v4(1.0f, 1.0f, 0.0f, 0.0f),
			  new Timer(ZOOM_OUT_TIME, Timer::FUNC_CUBIC_IN));
	m_plane->SetMetaAlpha(0.0f, new Timer(ZOOM_FADE_TIME), ui::Widget::ANIM_META_ALPHA, 0,
			      ZOOM_OUT_TIME, imposeZoomOnFaded, NULL);
}

/* whether the plane still shows a capture (0x81152718) */
int ImposeZoom::IsBlocked()
{
	if (m_plane == NULL)
		return 0;

	intrusive_ptr<graph::Surface> texture;

	m_plane->GetTexture(texture);
	if (texture.get() == NULL)
		return 0;
	return 1;
}

/* (0x8115275E) */
static void imposeZoomOnFaded(Timer *t, ui::AnimListener *listener, void *data1, void *data2,
			      void *data3)
{
	s_zoom->Finish();
}
