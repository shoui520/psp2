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
#include <common/char_encoding.h>
#include <cache/cache_image.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include "../../../shell.h"
#include "../../../theme.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* paf's vector push_back, out of line as in Sony's build */
PAF_VECTOR_NOINLINE_PUSH_BACK(paf::common::SharedPtr<LsdbPage>,
			      paf::allocator<paf::common::SharedPtr<LsdbPage> >)
PAF_VECTOR_NOINLINE_PUSH_BACK(ApplauncherFolderSurface, paf::allocator<ApplauncherFolderSurface>)
PAF_VECTOR_INLINE_DTOR(int)

/* PLACEHOLDER: the page widget's timer that destroys it, and its time */
#define APPLAUNCHER_HOME_PAGE_TIMER		0x0100FF00
#define APPLAUNCHER_HOME_PAGE_DESTROY_TIME	200.0f
/* PLACEHOLDER: the plugin's string a new folder is named with */
#define APPLAUNCHER_HOME_PAGE_MSG_NEW_FOLDER	0xEC7F98D1

/* PLACEHOLDER: how far the other icons move from an opened one, and its scale as it opens */
#define APPLAUNCHER_HOME_PAGE_ASIDE_DISTANCE	500.0f
#define APPLAUNCHER_HOME_PAGE_OPEN_SCALE	5.86956501f
#define APPLAUNCHER_HOME_PAGE_OPEN_SCALE_Z	5.94202900f

/* where each slot's icon stands on the page, before the home's scale (0x81555D70) */
static math::v2 s_slotPos[APPLAUNCHER_PAGE_SLOTS] = {
	math::v2(-229.0f, 158.0f), math::v2(0.0f, 158.0f), math::v2(229.0f, 158.0f),
	math::v2(-336.0f, 4.0f), math::v2(-112.0f, 4.0f), math::v2(112.0f, 4.0f),
	math::v2(336.0f, 4.0f),
	math::v2(-229.0f, -150.0f), math::v2(0.0f, -150.0f), math::v2(229.0f, -150.0f),
};

/* the widget faded in over the timer */
static inline void ShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

/* the widget faded out over the timer */
static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* the text in UTF-8 */
static inline string applauncherPageUtf8(const wstring &text)
{
	string utf8;

	common::Utf16ToUtf8(text, &utf8);
	return utf8;
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

/* the page's parameters set, its colour kept when the new one is -1, its icons' labels
   recoloured; saved when asked (but a host0: theme while the theme's icons are on), and the
   background changed when it is the page in view (0x810D274C) */
void ApplauncherPage::SetParam(const LsdbPage *param, bool save)
{
	int bgColor = m_param.bgColor;

	m_param = *param;
	if (m_param.bgColor == -1)
		m_param.bgColor = bgColor;
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; ++i) {
		if (m_slots[i].get() != NULL && m_slots[i]->m_icon != NULL)
			applauncherIconWidgetUpdateLabel(m_slots[i]->m_icon);
	}
	if (save && (!shellThemeIconsEnabled() || m_param.themeFile.length() == 0 ||
		     sce_paf_strncmp(m_param.themeFile.c_str(), "host0:", 5) != 0)) {
		int index = m_index;
		vector<int> pages;
		LsdbPageList params;

		pages.push_back(index);
		params.push_back(common::SharedPtr<LsdbPage>(new LsdbPage(m_param)));
		applauncherSetPageThemeJobStart(((job::JobQueue *(*)(void))
			g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE])(),
			pages, params, NULL, NULL);
	}

	ShellSnapListItem *focused = g_applauncherHome->m_scroller.GetFocused();
	ApplauncherPage *page = NULL;

	if (focused != NULL)
		page = static_cast<ApplauncherPage *>(focused);
	if (this == page)
		g_applauncherHome->UpdateBackground(this);
}

/* (0x810D29D6) */
void ApplauncherPage::KickIcons()
{
	if (m_state == 1)
		return;
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; ++i) {
		if (m_slots[i].get() == NULL)
			continue;

		math::v4 pos = m_slots[i]->GetSlotPos();
		float x = pos.extract_x() +
			  ((float)sce_paf_rand() / 32767.0f - 0.5f) * 600.0f;
		float y = pos.extract_y() + -500.0f;
		float len = math::v4(x, y, 800.0f, 0.0f).len();
		ApplauncherIconView *view = m_slots[i]->m_icon;

		if (view != NULL)
			applauncherIconWidgetSetIconPos(view,
				math::v3(x / len * 4.0f, y / len * 3.0f, 0.0f));
	}
}

/* (0x810D2B30) */
void ApplauncherPage::OnSnapped(void)
{
	KickIcons();
}

/* (0x810D2B38) */
void ApplauncherPage::OnUnsnapped(void)
{
	UnselectAll();
}

/* each icon given the data of its row in the list (0x810D2B40) */
void ApplauncherPage::UpdateIcons(LsdbIconList *list)
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; ++i) {
		ApplauncherIcon *icon = m_slots[i].get();

		if (icon == NULL)
			continue;
		for (SceUInt32 j = 0; j < list->size(); ++j) {
			if ((*list)[j]->rowid == icon->m_data->rowid) {
				LsdbIconPtr data = (*list)[j];

				icon->Reset(data);
				break;
			}
		}
	}
}

/* (0x810D2C3E) */
void ApplauncherPage::ReloadIcons()
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; ++i) {
		if (m_slots[i].get() != NULL)
			m_slots[i]->Reload();
	}
}

/* (0x810D2C58) */
int ApplauncherPage::MakeFolder(common::SharedPtr<ApplauncherIcon> &icon)
{
	ApplauncherIcon *target = icon.get();
	LsdbIconPtr folder(new LsdbIcon);
	vector<ApplauncherFolderSurface> images;

	{
		ApplauncherFolderSurface surface;

		surface.m_pos = 0;
		surface.m_surface = icon->m_image.get() != NULL ? icon->m_image->GetImage() :
				    intrusive_ptr<graph::Surface>();
		images.push_back(surface);
	}

	bool focusShown = ScePafWidget_529FB877(Framework::Instance()->GetEnvironmentInstance(), -1);

	applauncherIconRemove(icon, false);
	folder->pos = target->m_data->pos;
	folder->icon0Type = 7;
	folder->type = LSDB_ICON_TYPE_FOLDER;
	folder->title = applauncherPageUtf8(
		wstring(g_applauncherPlugin->GetString(IDParam(APPLAUNCHER_HOME_PAGE_MSG_NEW_FOLDER))));
	target->m_data->pos = 0;
	applauncherCreateNewFolderJobStart(((job::JobQueue *(*)(void))
		g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE])(),
		m_index, icon->m_data, folder, images, applauncherIconFolderCreated, NULL);
	applauncherIconDelete(icon, false, true);

	common::SharedPtr<ApplauncherIcon> added = AddIcon(folder);

	if (m_state == 1 && added.get() != NULL) {
		added->BeginEdit(false);
		if (added->GetWidget() != NULL)
			added->GetWidget()->SetFocus(NULL);
	}

	ui::Widget *widget = added->GetWidget();

	if (widget != NULL) {
		widget->SetScale(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, ui::Widget::ANIM_SCALE, 0);
		widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				 new Timer(300.0f, Timer::FUNC_BACK_OUT), ui::Widget::ANIM_SCALE, 2,
				 g_applauncherFolderMoveTime - 50.0f);
	}
	if (focusShown) {
		Framework::Instance()->GetEnvironmentInstance()->HideFocus(-1);
		Framework::Instance()->GetEnvironmentInstance()->ShowFocus(-1);
	}
	UpdateFocusLinks();
	return 1;
}

/* (0x810D319A) */
void ApplauncherPage::MoveIconsAside(const common::SharedPtr<ApplauncherIcon> &icon)
{
	if (icon->GetWidget() == NULL)
		return;

	math::v4 scale(APPLAUNCHER_HOME_PAGE_OPEN_SCALE, APPLAUNCHER_HOME_PAGE_OPEN_SCALE,
		       APPLAUNCHER_HOME_PAGE_OPEN_SCALE_Z, 0.0f);

	for (unsigned int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		ApplauncherIcon *slot = m_slots[i].get();
		ui::Widget *widget;

		if (slot == NULL)
			continue;
		if (icon.get() != slot) {
			if (slot->GetWidget() == NULL)
				continue;

			math::v4 from = m_slots[i]->GetWidget()->GetPos(0, NULL,
				math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			math::v4 to = icon->GetWidget()->GetPos(0, NULL,
				math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			math::v4 dir = (from - to).unit();

			widget = m_slots[i]->GetWidget();
			math::v4 pos = m_slots[i]->GetWidget()->GetPos(0, NULL,
				math::v4(0.0f, 0.0f, 0.0f, 0.0f)) +
				dir * APPLAUNCHER_HOME_PAGE_ASIDE_DISTANCE;
			widget->SetPos(pos, new Timer(g_applauncherIconAsideTime, Timer::FUNC_QUINT_OUT),
				       ui::Widget::ANIM_MOVE, 2);
			HideWidget(m_slots[i]->GetWidget(),
				   new Timer(g_applauncherIconMoveTime, Timer::FUNC_QUINT_OUT));
		} else {
			widget = m_slots[i]->GetWidget();
			widget->SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				       new Timer(g_applauncherIconAsideTime, Timer::FUNC_QUINT_OUT),
				       ui::Widget::ANIM_MOVE, 2);
			widget = m_slots[i]->GetWidget();
			widget->SetScale(scale, new Timer(g_applauncherIconAsideTime, Timer::FUNC_BACK_OUT),
					 ui::Widget::ANIM_SCALE, 2);
			HideWidget(m_slots[i]->GetWidget(),
				   new Timer(g_applauncherIconMoveTime, Timer::FUNC_QUINT_OUT));
		}
	}
}

/* (0x810D364E) */
void ApplauncherPage::RestoreIcons(const common::SharedPtr<ApplauncherIcon> &focus)
{
	if (focus.get() == NULL)
		return;

	for (unsigned int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		ui::Widget *widget;

		if (m_slots[i].get() == NULL || m_slots[i]->GetWidget() == NULL)
			continue;
		if (focus.get() != m_slots[i].get()) {
			widget = m_slots[i]->GetWidget();
			widget->SetPos(m_slots[i]->GetSlotPos(),
				       new Timer(g_applauncherIconRestoreTime, Timer::FUNC_QUART_OUT),
				       ui::Widget::ANIM_MOVE, 2);
			ShowWidget(m_slots[i]->GetWidget(),
				   new Timer(g_applauncherIconMoveTime, Timer::FUNC_QUART_OUT));
		} else {
			widget = m_slots[i]->GetWidget();
			widget->SetPos(m_slots[i]->GetSlotPos(),
				       new Timer(g_applauncherIconRestoreTime, Timer::FUNC_QUART_OUT),
				       ui::Widget::ANIM_MOVE, 2);
			widget = m_slots[i]->GetWidget();
			widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
					 new Timer(g_applauncherIconRestoreTime, Timer::FUNC_QUART_OUT),
					 ui::Widget::ANIM_SCALE, 2);
			ShowWidget(m_slots[i]->GetWidget(),
				   new Timer(g_applauncherIconMoveTime, Timer::FUNC_QUART_OUT));
		}
	}
}

/* (0x810D3996) */
common::SharedPtr<ApplauncherIcon> ApplauncherPage::FindIcon(SceUInt64 appId)
{
	for (unsigned int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL && m_slots[i]->m_data->rowid == appId)
			return m_slots[i];
	}
	return common::SharedPtr<ApplauncherIcon>();
}

/* (0x810D39EE) */
common::SharedPtr<ApplauncherIcon> ApplauncherPage::FindIconById(int id)
{
	for (unsigned int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL && m_slots[i]->IsFolder() &&
		    m_slots[i]->m_data.get() != NULL && m_slots[i]->m_data->folder.get() != NULL &&
		    m_slots[i]->m_data->folder->id == id)
			return m_slots[i];
	}
	return common::SharedPtr<ApplauncherIcon>();
}

/* (0x810D3A64) */
common::SharedPtr<ApplauncherIcon> ApplauncherPage::FindIcon(const string &titleId,
	common::SharedPtr<ApplauncherIcon> *folder)
{
	for (unsigned int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() == NULL)
			continue;
		if (m_slots[i]->m_data->titleId == titleId)
			return m_slots[i];
		if (m_slots[i]->HasChild(titleId)) {
			*folder = m_slots[i];
			break;
		}
	}
	return common::SharedPtr<ApplauncherIcon>();
}

/* (0x810D3B5E) */
int ApplauncherPage::GetIndex(void)
{
	return m_index;
}

/* (0x810D3B64) */
ui::Widget *ApplauncherPage::GetWidget(void)
{
	return m_widget;
}

/* (0x810D3B68) */
math::v2 ApplauncherPage::GetSlotPos(int slot)
{
	return s_slotPos[slot] * g_applauncherHome->m_scale;
}

/* PLACEHOLDER name: the same for any application page, without one (0x810D3B90) */
math::v2 applauncherPageGetSlotPos(int slot)
{
	return s_slotPos[slot] * g_applauncherHome->m_scale;
}
