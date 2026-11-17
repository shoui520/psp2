/*
 * The screen the shell shows while its boot waits: a black plane over the display with a busy
 * indicator, in a scene of its own. Closing it fades the plane out; the scene goes once the
 * indicator has stopped. The file's and every name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <framework.h>
#include <timer/timer.h>
#include <math/math.h>
#include <versatileparam/idparam.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_display.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <widget/w_plane.h>
#include <widget/w_busyindicator.h>
#include "shell.h"

using namespace paf;

/* what a busy indicator sends once its stop has run (paf's headers do not name it) */
#define BUSY_INDICATOR_CB_STOPPED	0x10000001

/* the common texture of the busy indicator's balls, by its ID's hash */
#define BOOT_SCREEN_BUSY_TEXTURE	0xBA195A37

class ShellBootScreen {
public:
	ShellBootScreen();

	ui::Scene *m_scene;
	ui::Camera *m_camera;
	ui::Plane *m_plane;
	ui::BusyIndicator *m_busyIndicator;
};

static ShellBootScreen *s_bootScreen;

ShellBootScreen::ShellBootScreen()
{
	m_scene = new ui::Scene(Framework::Instance()->GetEnvironmentInstance(), 25, NULL, NULL,
				128);
	m_camera = new ui::Camera(m_scene, NULL);
	m_scene->SetCamera(m_camera);

	m_plane = new ui::Plane(m_scene, NULL);
	ScePafWidget_0253DC9C(m_plane, true);
	m_plane->SetSize(math::v4(960.0f, 544.0f, 0.0f, 0.0f));
	m_plane->SetColor(math::v4(0.0f, 0.0f, 0.0f, 1.0f));
	ScePafWidget_42C5A4D4(m_plane, 0, 0);

	m_busyIndicator = new ui::BusyIndicator(m_scene, NULL);
	m_busyIndicator->SetSize(math::v4(72.0f, 72.0f, 0.0f, 0.0f));
	m_busyIndicator->SetBallSize(16.0f);
	m_busyIndicator->SetTexture(
		Framework::Instance()->GetCommonTexture(IDParam(BOOT_SCREEN_BUSY_TEXTURE)));
	m_busyIndicator->Start();
	graph::s_page_opened = true;
}

/* the indicator has stopped: the scene and its camera go */
static void shellBootScreenOnStopped(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	ShellBootScreen *screen = s_bootScreen;

	if (screen != NULL) {
		screen->m_scene->SetCamera(NULL);
		screen->m_scene->DestroyWidget();
		screen->m_camera->DestroyWidget();
		delete screen;
	}
	s_bootScreen = NULL;
}

void shellBootScreenCreate(void)
{
	if (s_bootScreen == NULL)
		s_bootScreen = new ShellBootScreen();
}

/* the plane fades out over 200 ms after 300 ms, and the indicator stops */
void shellBootScreenClose(void)
{
	if (s_bootScreen == NULL)
		return;

	ui::Plane *plane = s_bootScreen->m_plane;
	Timer *timer = new Timer(200.0f);

	plane->m_transition_complete = false;
	plane->m_transition = 0xff;
	plane->DoTransitionReverse_core((timer != NULL ? timer->GetTotalTime() : 0.0f) + 300.0f,
					true);
	ScePafWidget_4588699E(plane, timer, 2, 300.0f, NULL);

	s_bootScreen->m_busyIndicator->Stop();
	s_bootScreen->m_busyIndicator->AddEventListener(BUSY_INDICATOR_CB_STOPPED,
		new ui::EventCBListener(shellBootScreenOnStopped, NULL), true);
}
