/*
 * applauncher_page.cpp: a page of the home (0x810B3BBA-): its icons in their ten slots (three,
 * four and three in rows), the focus moves between them, its widgets taken from and given back
 * to the page pool. The file's name and the names here are ours (PLACEHOLDER) unless a comment
 * says otherwise: no record or string of the 3.6x shell names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <plugin.h>
#include <vectormath.h>
#include <std/math.h>
#include <std/string>
#include <std/vector>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include "../../../shell.h"
#include "../../../plugins.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* the slots of a page: the top row, the middle row and the bottom row */
#define APPLAUNCHER_PAGE_SLOTS		10
#define APPLAUNCHER_PAGE_TOP_FIRST	0
#define APPLAUNCHER_PAGE_TOP_LAST	2
#define APPLAUNCHER_PAGE_MIDDLE_FIRST	3
#define APPLAUNCHER_PAGE_MIDDLE_LAST	6
#define APPLAUNCHER_PAGE_BOTTOM_FIRST	7
#define APPLAUNCHER_PAGE_BOTTOM_LAST	9

/*
 * PLACEHOLDER: an icon's move between the slots (0x81555250-0x8155527F): what has happened
 * since it began (three marks), the icon dragged and the one it is over, the page and the slot
 * it began on, and where it was last over (-1 for nowhere).
 */
static bool s_moveMark0;
static bool s_moveMark1;
static bool s_moveMark2;
common::SharedPtr<ApplauncherIcon> g_applauncherDragItem;
static common::SharedPtr<ApplauncherIcon> s_moveIcon;
common::SharedPtr<ApplauncherIcon> g_applauncherDropTarget;
static int s_moveStartPage;
static int s_moveStartSlot;
static int s_moveOverPage = -1;
static int s_moveOverSlot = -1;
static int s_moveOverTime = -1;

/* the directions a widget's focus moves in (Widget::SetFocusDirection) */
#define APPLAUNCHER_PAGE_FOCUS_UP	0
#define APPLAUNCHER_PAGE_FOCUS_DOWN	1

/* the page's widgets given back to the pool, its icons' first (0x810B3BBA) */
void ApplauncherPageBase::Unload()
{
	if (m_view == NULL)
		return;
	if (m_iconsLoaded) {
		m_iconsLoaded = false;
		for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
			if (m_slots[i].get() != NULL)
				m_slots[i]->ReleaseView(false);
		}
	}
	applauncherPageViewRelease(m_view);
	m_view = NULL;
}

/* whether the slot holds an icon with a widget */
#define APPLAUNCHER_PAGE_HAS_WIDGET(slot) \
	(m_slots[slot].get() != NULL && m_slots[slot]->GetWidget() != NULL)

/*
 * The focus moves between the icons set: up and down only, each icon to the nearest one of the
 * row above and below (the middle row's ends straight to the rows' ends), the moves cleared
 * first (0x810B3BF0).
 */
void ApplauncherPage::UpdateFocusLinks()
{
	SceUInt32 i;

	for (i = APPLAUNCHER_PAGE_TOP_FIRST; i <= APPLAUNCHER_PAGE_TOP_LAST; i++) {
		if (!APPLAUNCHER_PAGE_HAS_WIDGET(i))
			continue;
		m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP, IDParam());
		m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN, IDParam());
		if (APPLAUNCHER_PAGE_HAS_WIDGET(i + 3))
			m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN,
				m_slots[i + 3]->GetWidget()->m_name);
		else if (APPLAUNCHER_PAGE_HAS_WIDGET(i + 4))
			m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN,
				m_slots[i + 4]->GetWidget()->m_name);
	}

	if (APPLAUNCHER_PAGE_HAS_WIDGET(APPLAUNCHER_PAGE_MIDDLE_FIRST)) {
		m_slots[3]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP, IDParam());
		m_slots[3]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN, IDParam());
		if (APPLAUNCHER_PAGE_HAS_WIDGET(0))
			m_slots[3]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP,
				m_slots[0]->GetWidget()->m_name);
		if (APPLAUNCHER_PAGE_HAS_WIDGET(7))
			m_slots[3]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN,
				m_slots[7]->GetWidget()->m_name);
	}

	for (i = APPLAUNCHER_PAGE_MIDDLE_FIRST + 1; i <= APPLAUNCHER_PAGE_MIDDLE_LAST - 1; i++) {
		if (!APPLAUNCHER_PAGE_HAS_WIDGET(i))
			continue;
		m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP, IDParam());
		m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN, IDParam());
		if (APPLAUNCHER_PAGE_HAS_WIDGET(i - 3))
			m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP,
				m_slots[i - 3]->GetWidget()->m_name);
		else if (APPLAUNCHER_PAGE_HAS_WIDGET(i - 4))
			m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP,
				m_slots[i - 4]->GetWidget()->m_name);
		if (APPLAUNCHER_PAGE_HAS_WIDGET(i + 4))
			m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN,
				m_slots[i + 4]->GetWidget()->m_name);
		else if (APPLAUNCHER_PAGE_HAS_WIDGET(i + 3))
			m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN,
				m_slots[i + 3]->GetWidget()->m_name);
	}

	if (APPLAUNCHER_PAGE_HAS_WIDGET(APPLAUNCHER_PAGE_MIDDLE_LAST)) {
		m_slots[6]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP, IDParam());
		m_slots[6]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN, IDParam());
		if (APPLAUNCHER_PAGE_HAS_WIDGET(2))
			m_slots[6]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP,
				m_slots[2]->GetWidget()->m_name);
		if (APPLAUNCHER_PAGE_HAS_WIDGET(9))
			m_slots[6]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN,
				m_slots[9]->GetWidget()->m_name);
	}

	for (i = APPLAUNCHER_PAGE_BOTTOM_FIRST; i <= APPLAUNCHER_PAGE_BOTTOM_LAST; i++) {
		if (!APPLAUNCHER_PAGE_HAS_WIDGET(i))
			continue;
		m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP, IDParam());
		m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_DOWN, IDParam());
		if (APPLAUNCHER_PAGE_HAS_WIDGET(i - 4))
			m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP,
				m_slots[i - 4]->GetWidget()->m_name);
		else if (APPLAUNCHER_PAGE_HAS_WIDGET(i - 3))
			m_slots[i]->GetWidget()->SetFocusDirection(APPLAUNCHER_PAGE_FOCUS_UP,
				m_slots[i - 3]->GetWidget()->m_name);
	}
}

/* no icon in any slot (0x810B43F0) */
bool ApplauncherPage::IsEmpty()
{
	for (int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL)
			return false;
	}
	return true;
}

/*
 * The icon taken off its page: out of the folder open on the page first when it came from
 * there, its slot cleared, the page shown empty when sorting is on, and the focus moves set
 * again (0x810B4408).
 */
void applauncherIconRemove(common::SharedPtr<ApplauncherIcon> icon, bool fromFolder)
{
	ApplauncherPage *page = icon->m_page;
	SceUInt32 i;

	if (page == NULL)
		return;
	for (i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (page->m_slots[i].get() == icon.get())
			break;
	}
	if (i >= APPLAUNCHER_PAGE_SLOTS)
		return;

	if (page->m_type == 2 && fromFolder && page->m_focus.get() != NULL)
		page->m_focus->RemoveChild(icon->m_data->titleId.c_str());

	icon->m_page->m_changed = true;
	icon->m_page->m_slots[i].reset();
	icon->m_page = NULL;
	icon->m_slot = -1;
	if (g_applauncherState->m_settings->m_sortEnabled)
		page->SetEmpty(page->IsEmpty());
	page->UpdateFocusLinks();
}

/* the page's icons taken off and deleted, its widgets given back (0x810B4502) */
ApplauncherPageBase::~ApplauncherPageBase()
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		common::SharedPtr<ApplauncherIcon> icon = m_slots[i];

		if (icon.get() != NULL) {
			applauncherIconRemove(icon, false);
			applauncherIconDelete(icon, false, false);
		}
	}
	Unload();
}

/* the widget its icons go on, NULL without its view (0x810B46F6) */
ui::Widget *ApplauncherPageBase::GetRoot()
{
	if (m_view == NULL)
		return NULL;
	return m_view->m_root;
}

/* the first free slot, -1 when none is (0x810B4702) */
int ApplauncherPage::GetEmptySlot()
{
	for (int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() == NULL)
			return i;
	}
	return -1;
}

/* PLACEHOLDER: the icon widget's animation of its icon's size (object 0) */
#define APPLAUNCHER_PAGE_ANIM_ICON_SIZE	0x20200
/* PLACEHOLDER: the icon widget's mode while it is dragged */
#define APPLAUNCHER_PAGE_ICON_MODE_DRAG	2

/*
 * The icon taken off its page and put in the slot (the first free one for -1): its widget
 * moved onto the page when attach is set, sized for the page, grown back to its size (a fifth
 * larger while it is held) when it is the one being dragged, and the icon put in the folder
 * open on the page when toFolder is set. False when the slot is taken (0x810B471A).
 */
bool ApplauncherPage::PutIcon(const common::SharedPtr<ApplauncherIcon> &icon, int slot,
			      bool attach, bool toFolder)
{
	if (slot < 0) {
		slot = GetEmptySlot();
		if (slot < 0)
			return false;
	}
	if (icon->m_page == this && icon->m_slot == slot)
		return true;
	if (m_slots[slot].get() != NULL)
		return false;

	applauncherIconRemove(icon, true);
	if (slot < 0)
		return false;

	icon->m_slot = slot;
	icon->m_data->pos = slot;
	m_slots[slot] = icon;
	icon->m_page = this;
	if (m_iconsLoaded)
		icon->LoadView();
	else
		icon->ReleaseView(false);

	if (icon->m_icon != NULL) {
		math::v4 size;
		float edge;

		if (attach)
			(*icon->m_icon)->SetParent(GetRoot(), NULL, 0);
		if (icon->m_page->m_type != 0)
			edge = applauncherIconWidgetFolderSize();
		else
			edge = applauncherIconWidgetSize();
		size = math::v4(edge, edge, 0.0f, 0.0f);
		applauncherIconWidgetUpdateLabel(icon->m_icon);
		(*icon->m_icon)->SetSize(size, NULL, ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		applauncherIconWidgetUpdateMark(icon->m_icon);

		if (icon.get() == g_applauncherDragItem.get()) {
			ui::AppIconBase *widget = *icon->m_icon;

			if (widget->m_touch_captures.size() != 0 ||
			    widget->m_icon_mode == APPLAUNCHER_PAGE_ICON_MODE_DRAG) {
				math::v4 held;

				widget->KillObjAnim(0, APPLAUNCHER_PAGE_ANIM_ICON_SIZE);
				widget = *icon->m_icon;
				held = size * 1.2f;
				widget->SetObjAnim(0, APPLAUNCHER_PAGE_ANIM_ICON_SIZE, held,
					new Timer(100.0f, Timer::FUNC_QUAD_OUT), 0);
			} else {
				widget->KillObjAnim(0, APPLAUNCHER_PAGE_ANIM_ICON_SIZE);
				(*icon->m_icon)->SetObjAnim(0, APPLAUNCHER_PAGE_ANIM_ICON_SIZE, size,
					new Timer(100.0f, Timer::FUNC_QUAD_OUT), 0);
			}
		}
	}

	if (m_state == 1)
		SetEmpty(IsEmpty());
	if (icon->m_page->m_type == 2 && toFolder)
		icon->m_page->m_focus->AddChild(icon->m_data->titleId.c_str());
	m_changed = true;
	return true;
}

/* an icon in every slot (0x810B4B0C) */
bool ApplauncherPage::IsFull()
{
	for (int i = APPLAUNCHER_PAGE_SLOTS - 1; i >= 0; i--) {
		if (m_slots[i].get() == NULL)
			return false;
	}
	return true;
}

/* an icon made for the data and put in its data's slot, none when the page is full
   (0x810B4B24) */
common::SharedPtr<ApplauncherIcon> ApplauncherPage::AddIcon(const LsdbIconPtr &data)
{
	if (IsFull())
		return common::SharedPtr<ApplauncherIcon>();

	common::SharedPtr<ApplauncherIcon> icon = applauncherIconCreate(g_applauncherPlugin, data);

	if (icon.get() != NULL)
		PutIcon(icon, icon->m_data->pos, true, false);
	return icon;
}

/* PLACEHOLDER: the icon taken off its page, its slot cleared and the page shown empty when
   sorting is on; no folder and no focus moves (0x810B4C02) */
void applauncherIconDetach(ApplauncherIcon *icon)
{
	ApplauncherPage *page = icon->m_page;
	SceUInt32 i;

	if (page == NULL)
		return;
	for (i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (page->m_slots[i].get() == icon)
			break;
	}
	if (i >= APPLAUNCHER_PAGE_SLOTS)
		return;

	page->m_changed = true;
	page->m_slots[i].reset();
	icon->m_page = NULL;
	icon->m_slot = -1;
	if (g_applauncherState->m_settings->m_sortEnabled)
		page->SetEmpty(page->IsEmpty());
}

/*
 * The icon's move begun on the page: the page and the slot it began on kept, nothing it was
 * over yet. The page must be the one its move began on; it stops at a breakpoint after
 * printing the pages otherwise (Sony's name, from its message; 0x810B4CBE).
 */
void ApplauncherPage::iconMoveBegin(const common::SharedPtr<ApplauncherIcon> &icon)
{
	if (this != icon->m_movePage) {
		sceClibPrintf("[SceShell] iconMoveBegin %p %p %p\n", this, icon->m_movePage,
			      icon.get());
		for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
		     it != g_applauncherHome->m_scroller.m_items.end(); ++it) {
			ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

			sceClibPrintf("[SceShell] tab%d %p\n", page->m_index, page);
		}
		__breakpoint(0);
	}
	s_moveStartPage = icon->m_movePage->GetIndex();
	s_moveStartSlot = icon->m_moveSlot;
	s_moveOverPage = -1;
	s_moveOverSlot = -1;
	s_moveOverTime = -1;
	m_moveSlot = s_moveStartSlot;
	s_moveMark0 = false;
	s_moveMark1 = false;
	s_moveMark2 = false;
}

/*
 * PLACEHOLDER: the icon's widget moved to the position and faded to the colour, over a time
 * that grows with the fourth root of the way to go (0x810B4DA2).
 */
static void moveIconWidget(ui::Widget *widget, const math::v4 &pos, const math::v4 &color)
{
	math::v4 from = widget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v4 way = pos - from;
	float length = sce::Vectormath::Simd::Aos::length(
		sce::Vectormath::Simd::Aos::Vector2(vget_low_f32(way.getImpl().get128())));
	float time;

	if (length < 1.0f)
		length = 1.0f;
	time = sqrtf(sqrtf(length)) * 60.0f;
	widget->SetPos(pos, new Timer(time, Timer::FUNC_QUART_OUT), ui::Widget::ANIM_MOVE, 2);
	widget->SetColor(color, new Timer(time, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 2);
}

/*
 * PLACEHOLDER: the icon moved aside for the dragged one changed to this one: the one before
 * put back in its slot on its page first, its widget moving back there. False when the icon
 * was the one moved aside already, which is then none (0x810B4F62).
 */
static bool setMovedIcon(const common::SharedPtr<ApplauncherIcon> &icon)
{
	if (s_moveIcon.get() != NULL) {
		if (s_moveIcon->m_slot != s_moveIcon->m_homeSlot ||
		    s_moveIcon->m_page != s_moveIcon->m_movePage) {
			ui::Widget *widget = *s_moveIcon->m_icon;

			if (s_moveIcon->m_page != NULL)
				applauncherIconRemove(s_moveIcon, true);
			s_moveIcon->m_movePage->PutIcon(s_moveIcon, s_moveIcon->m_homeSlot, true,
							true);
			s_moveIcon->m_movePage->UpdateFocusLinks();
			widget->KillAnim(ui::Widget::ANIM_MOVE);
			widget->KillAnim(ui::Widget::ANIM_COLOR);
			moveIconWidget(widget, s_moveIcon->GetSlotPos(),
				       math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		}
		if (s_moveIcon.get() == icon.get()) {
			s_moveIcon.reset();
			return false;
		}
	}
	s_moveIcon = icon;
	return true;
}

/*
 * PLACEHOLDER: the icon the dragged one is over changed to this one: the one before faded and
 * shrunk back, this one dimmed and grown. False when it is the same (0x810B519E).
 */
static bool setDropTarget(const common::SharedPtr<ApplauncherIcon> &icon)
{
	if (g_applauncherDropTarget.get() == icon.get())
		return false;

	if (g_applauncherDropTarget.get() != NULL) {
		ui::Widget *widget = *g_applauncherDropTarget->m_icon;

		widget->KillAnim(ui::Widget::ANIM_COLOR);
		widget->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
				 new Timer(200.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 0);
		widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				 new Timer(200.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_SCALE, 0);
	}
	if (icon.get() != NULL) {
		ui::Widget *widget = *icon->m_icon;

		widget->KillAnim(ui::Widget::ANIM_COLOR);
		widget->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.2f),
				 new Timer(200.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 0);
		widget->SetScale(math::v4(1.4f, 1.4f, 1.0f, 0.0f),
				 new Timer(200.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_SCALE, 0);
	}
	g_applauncherDropTarget = icon;
	return true;
}

/* the icons in its slots (0x810B5506) */
int ApplauncherPage::GetIconCount()
{
	int count = 0;

	for (int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL)
			count++;
	}
	return count;
}
