/*
 * applauncher_homepage.cpp: a page of the home, the part of ApplauncherPage after the icon's file
 * (0x810D2578-0x810D4568): made and taken down, its parameters, its icons given the database's
 * data, moved aside from an opened folder and back, looked up. The file's name and the names here
 * are ours (PLACEHOLDER): no record of the 3.6x shell names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <widget/core/widget.h>
#include <timer/timer.h>
#include "../../../shell.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* PLACEHOLDER: the page widget's timer that destroys it, and its time */
#define APPLAUNCHER_HOME_PAGE_TIMER		0x0100FF00
#define APPLAUNCHER_HOME_PAGE_DESTROY_TIME	200.0f

/* the widget faded out over the timer */
static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* (0x810D2578) */
ApplauncherPage::~ApplauncherPage()
{
	if (m_widget != NULL) {
		m_widget->KillAnim(ui::Widget::ANIM_SHOW_ALPHA);
		HideWidget(m_widget, new Timer(APPLAUNCHER_HOME_PAGE_DESTROY_TIME));
		m_widget->SetTimerListener(APPLAUNCHER_HOME_PAGE_TIMER,
					   APPLAUNCHER_HOME_PAGE_DESTROY_TIME,
					   new ui::EventCBListener(applauncherDestroyWidgetCB, NULL));
		m_widget = NULL;
	}
	if (g_applauncherHome != NULL)
		applauncherPagesCleanTabpaper();
}

/* (0x810D2716) */
void ApplauncherPage::GetParam(LsdbPage *param)
{
	*param = m_param;
}
