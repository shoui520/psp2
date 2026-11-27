/*
 * impose_busy_screen.cpp: impose_plugin's black screen with a busy indicator (0x8114FD14-
 * 0x8115000C): a scene of its own over everything, a plane faded to black and a busy
 * indicator turning on it; the caller's function is called once it is black. The file's and
 * the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <timer/timer.h>
#include <math/math.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <widget/w_plane.h>
#include <widget/w_busyindicator.h>
#include "../../livespace_db/lsdb_keys.h"
#include "impose.h"

using namespace paf;

/* the scene's draw priority, above the plugin's pages */
#define BUSY_SCREEN_PRIORITY	4
/* the fade to black, in milliseconds */
#define BUSY_SCREEN_FADE_TIME	280.0f

/* (0x815448EC) */
static ImposeBusyScreen *s_busyScreen;

/* (0x8114FD14) */
ImposeBusyScreen::ImposeBusyScreen(Plugin *plugin)
	: m_plugin(plugin), m_scene(NULL), m_onShown(NULL), m_onShownData(NULL)
{
	s_busyScreen = this;
}

/* (0x8114FD2C) */
ImposeBusyScreen::~ImposeBusyScreen()
{
	if (m_scene != NULL) {
		m_scene->DestroyWidget();
		m_scene = NULL;
	}
	s_busyScreen = NULL;
}

static void imposeBusyScreenOnShown(Timer *t, ui::AnimListener *listener, void *target,
				    void *userData, void *context);

/* the scene made, the plane faded to black and the indicator started (0x8114FD50) */
int ImposeBusyScreen::Show(void (*onShown)(void *data), void *data)
{
	m_onShown = onShown;
	m_onShownData = data;
	m_scene = new ui::Scene(Framework::Instance()->GetEnvironmentInstance(),
				BUSY_SCREEN_PRIORITY, NULL, NULL, 128);
	new ui::Camera(m_scene, NULL);

	ui::Plane *plane = new ui::Plane(m_scene, NULL);

	if (plane != NULL) {
		plane->DisableEvent(0xFFF0);
		ScePafWidget_0253DC9C(plane, true);
		plane->SetSize(math::v4(960.0f, 544.0f, 0.0f, 0.0f));

		math::v4 color(0.0f, 0.0f, 0.0f, 0.0f);

		plane->SetColor(color);
		color = math::v4(0.0f, 0.0f, 0.0f, 1.0f);
		plane->SetColor(color, new Timer(BUSY_SCREEN_FADE_TIME, Timer::FUNC_QUAD_OUT),
				ui::Widget::ANIM_COLOR, 2, 0.0f, imposeBusyScreenOnShown, NULL);
	}

	ui::BusyIndicator *busy = new ui::BusyIndicator(m_scene, NULL);

	if (busy != NULL) {
		busy->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		busy->SetSize(math::v4(72.0f, 72.0f, 72.0f, 72.0f));
		busy->SetBallSize(16.0f);
		busy->Start();
	}
	return 1;
}

/* the fade has ended: the caller's function (0x8114FF80) */
static void imposeBusyScreenOnShown(Timer *t, ui::AnimListener *listener, void *target,
				    void *userData, void *context)
{
	ImposeBusyScreen *screen = s_busyScreen;

	if (screen != NULL && screen->m_onShown != NULL)
		screen->m_onShown(screen->m_onShownData);
}
