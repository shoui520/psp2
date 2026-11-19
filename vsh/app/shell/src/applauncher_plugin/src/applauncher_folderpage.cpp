/*
 * applauncher_folderpage.cpp: the page a folder's icon opens over the home (0x810BA824-
 * 0x810BB62E). The file's name and the names here are ours (PLACEHOLDER): no record of the 3.6x
 * shell names them.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/stdio.h>
#include <std/string>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scroll_view_base.h>
#include <timer/timer.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* PLACEHOLDER: the page's template and its widget in it */
#define APPLAUNCHER_FOLDER_PAGE_TEMPLATE	0x795B9E68
#define APPLAUNCHER_FOLDER_PAGE		0xC691233F

/* PLACEHOLDER: the timer the closed page is deleted on, its time, and the time a deleted
   page's widget is destroyed after */
#define APPLAUNCHER_FOLDER_PAGE_TIMER	0x0100FF00
#define APPLAUNCHER_FOLDER_PAGE_CLOSE_TIME	325.0f
#define APPLAUNCHER_FOLDER_PAGE_DESTROY_TIME	40.0f

/* the folder's icon's size on the page: the page stands at it before it opens */
#define APPLAUNCHER_FOLDER_PAGE_ICON_SCALE	0.16586539f

/* PLACEHOLDER: the page open (0x81544280) */
ApplauncherFolderPage *g_applauncherFolderPage;

/* where each slot's icon stands on the page, before the home's scale (0x81555328) */
static math::v2 s_slotPos[APPLAUNCHER_PAGE_SLOTS] = {
	math::v2(-200.0f, 124.0f), math::v2(0.0f, 124.0f), math::v2(200.0f, 124.0f),
	math::v2(-300.0f, -16.0f), math::v2(-100.0f, -16.0f), math::v2(100.0f, -16.0f),
	math::v2(300.0f, -16.0f),
	math::v2(-200.0f, -156.0f), math::v2(0.0f, -156.0f), math::v2(200.0f, -156.0f),
};

/* shows and hides a widget over the timer's time (inline in paf's callers) */
static inline void ShowWidget(ui::Widget *widget, Timer *timer, ui::AnimCB cb)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, cb);
}

static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

static void onLayoutDone(ApplauncherLsdbJob *job, void *arg);
static void onOpened(Timer *timer, ui::AnimListener *listener, void *data1, void *data2,
		     void *data3);
static void onCloseTimer(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);

/* (0x810BA824) */
ApplauncherFolderPage::ApplauncherFolderPage(const common::SharedPtr<ApplauncherIcon> &icon,
					     const char *name, bool select)
	: ApplauncherPageBase(2), m_icon(icon), m_index(icon->m_data->folder->id),
	  m_select(false), m_opened(false), m_widget(NULL), m_iconPos(m_icon->GetSlotPos())
{
	math::v4 origin(0.0f, 0.0f, 0.0f, 0.0f);

	if (name != NULL) {
		m_focusTitle = name;
		m_select = select;
	}

	g_applauncherPlugin->TemplateOpen(g_applauncherHome->m_scroller.m_scrollView->m_parent,
					  IDParam(APPLAUNCHER_FOLDER_PAGE_TEMPLATE),
					  Plugin::TemplateOpenParam());
	m_widget = g_applauncherHome->m_scroller.m_scrollView->m_parent->FindChild(
		IDParam(APPLAUNCHER_FOLDER_PAGE), 0);

	char id[64];

	sce_paf_snprintf(id, sizeof(id), "%p", this);
	m_widget->m_name.SetID(string(id));

	m_job = applauncherGetPageLayoutJobStart(
		((job::JobQueue *(*)(void))g_applauncherHome->m_topmenu->func[13])(), m_index, 1,
		onLayoutDone, NULL);

	m_widget->SetPos(icon->GetSlotPos(), NULL, ui::Widget::ANIM_MOVE, 0);
	HideWidget(m_widget, NULL);
	m_widget->SetScale(math::v4(APPLAUNCHER_FOLDER_PAGE_ICON_SCALE,
				    APPLAUNCHER_FOLDER_PAGE_ICON_SCALE,
				    APPLAUNCHER_FOLDER_PAGE_ICON_SCALE, 0.0f),
			   NULL, ui::Widget::ANIM_SCALE, 0);
	ShowWidget(m_widget, new Timer(375.0f, Timer::FUNC_QUAD_OUT), onOpened);
	m_widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
			   new Timer(375.0f, Timer::FUNC_BACK_OUT), ui::Widget::ANIM_SCALE, 2);
	m_widget->SetPos(origin, new Timer(275.0f, Timer::FUNC_QUINT_OUT), ui::Widget::ANIM_MOVE, 2);

	g_applauncherFolderPage = this;
	g_applauncherHome->HoldView();
	HideWidget(g_applauncherHome->m_scroller.m_scrollView,
		   new Timer(275.0f, Timer::FUNC_QUAD_OUT));
	m_widget->SetForward(g_applauncherHome->m_scroller.m_scrollView);
	AsPage()->Load(true);
	if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0)
		AsPage()->BeginEdit();
}

/* (0x810BAEBE) */
ApplauncherPage *applauncherIconPoolOpenAppPage(const common::SharedPtr<ApplauncherIcon> &icon,
						const char *name, bool animate)
{
	if (g_applauncherFolderPage != NULL)
		delete g_applauncherFolderPage;
	new ApplauncherFolderPage(icon, name, animate);
	return g_applauncherFolderPage->AsPage();
}

/* (0x810BAEFC) */
void applauncherIconPoolCloseAppPage(ApplauncherPage *appPage)
{
	ApplauncherFolderPage *page =
		static_cast<ApplauncherFolderPage *>(static_cast<ApplauncherPageBase *>(appPage));

	if (page->m_job.get() != NULL)
		page->m_job->Cancel();
	ShowWidget(g_applauncherHome->m_scroller.m_scrollView,
		   new Timer(275.0f, Timer::FUNC_QUART_OUT), NULL);
	g_applauncherHome->ReleaseView();

	ui::Widget *widget = page->m_widget;

	if (widget != NULL) {
		widget->SetTimerListener(APPLAUNCHER_FOLDER_PAGE_TIMER,
					 APPLAUNCHER_FOLDER_PAGE_CLOSE_TIME,
					 new ui::EventCBListener(onCloseTimer, page));
		HideWidget(page->m_widget, new Timer(275.0f, Timer::FUNC_QUART_OUT));
		page->m_widget->SetScale(math::v4(APPLAUNCHER_FOLDER_PAGE_ICON_SCALE,
						  APPLAUNCHER_FOLDER_PAGE_ICON_SCALE,
						  APPLAUNCHER_FOLDER_PAGE_ICON_SCALE, 0.0f),
					 new Timer(325.0f, Timer::FUNC_QUART_OUT),
					 ui::Widget::ANIM_SCALE, 2);
		page->m_widget->SetPos(page->m_iconPos, new Timer(325.0f, Timer::FUNC_QUART_OUT),
				       ui::Widget::ANIM_MOVE, 2);
		ScePafWidget_A96A0B4A(page->m_widget, 1);
	} else {
		delete page;
	}
}

/* (0x810BB216) */
ui::Widget *ApplauncherFolderPage::GetWidget(void)
{
	return m_widget;
}

/* (0x810BB21C) */
bool ApplauncherFolderPage::IsOpened(void)
{
	return m_opened;
}

/* (0x810BB222) */
math::v2 ApplauncherFolderPage::GetSlotPos(int slot)
{
	return s_slotPos[slot] * g_applauncherHome->m_scale;
}

/* PLACEHOLDER name: the same for any folder page, without one (0x810BB24A) */
math::v2 applauncherFolderPageGetSlotPos(int slot)
{
	return s_slotPos[slot] * g_applauncherHome->m_scale;
}

/* (0x810BB274) */
ApplauncherFolderPage::~ApplauncherFolderPage()
{
	job::JobItem *job = m_job.get();

	g_applauncherFolderPage = NULL;
	if (job != NULL)
		job->Cancel();
	Unload();
	if (m_widget != NULL) {
		m_widget->SetTimerListener(APPLAUNCHER_FOLDER_PAGE_TIMER,
					   APPLAUNCHER_FOLDER_PAGE_DESTROY_TIME,
					   new ui::EventCBListener(applauncherDestroyWidgetCB, NULL));
		m_widget = NULL;
	}
	ShowWidget(g_applauncherHome->m_scroller.m_scrollView, NULL, NULL);
}

/* (0x810BB428) */
void ApplauncherFolderPage::SetIcons(const LsdbIconListPtr *icons)
{
	string dragTitle;
	bool focused;
	unsigned int i;

	if (g_applauncherDragItem.get() != NULL)
		dragTitle = g_applauncherDragItem->m_data->titleId;

	if ((*icons)->size() != 0) {
		i = 0;
		focused = false;
		do {
			const LsdbIconPtr &data = (**icons)[i];

			if (data->type == APPLAUNCHER_ICON_TYPE_POWER)
				data->titleId = "power";
			if (dragTitle != data->titleId) {
				common::SharedPtr<ApplauncherIcon> icon = AsPage()->AddIcon(data);

				if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0)
					icon->BeginEdit(false);
				if (m_focusTitle == data->titleId) {
					icon->Focus();
					if (m_select)
						icon->Select();
				}
				if (m_focusTitle.length() == 0 && g_applauncherDragItem.get() == NULL &&
				    !focused) {
					icon->Focus();
					focused = true;
				}
			}
			i++;
		} while (i < (*icons)->size());
	}
	m_opened = true;
	m_select = false;
	AsPage()->UpdateFocusLinks();
}

/* the folder's page read: its icons put on the page still open (0x810BB5E6) */
static void onLayoutDone(ApplauncherLsdbJob *lsdbJob, void *arg)
{
	GetPageLayoutJob *job = static_cast<GetPageLayoutJob *>(lsdbJob);

	if (job->IsCanceled())
		return;

	ApplauncherFolderPage *page = static_cast<ApplauncherFolderPage *>(
		static_cast<ApplauncherPageBase *>(applauncherHomeGetAppPage()));

	if (job->m_icons.size() == 1 && page != NULL && page->m_index == job->m_page)
		page->SetIcons(&job->m_icons[0]);
}

/* the page's opening done: the home's view let go (0x810BB60E) */
static void onOpened(Timer *timer, ui::AnimListener *listener, void *data1, void *data2,
		     void *data3)
{
	g_applauncherHome->ReleaseView();
}

/* the closed page deleted (0x810BB620) */
static void onCloseTimer(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherFolderPage *page = (ApplauncherFolderPage *)data;

	if (page != NULL)
		delete page;
}
