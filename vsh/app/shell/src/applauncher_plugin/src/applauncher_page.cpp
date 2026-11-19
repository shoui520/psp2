/*
 * applauncher_page.cpp: a page of the home (0x810B3BBA-): its icons in their ten slots (three,
 * four and three in rows), the focus moves between them, its widgets taken from and given back
 * to the page pool. The file's name and the names here are ours (PLACEHOLDER) unless a comment
 * says otherwise: no record or string of the 3.6x shell names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <intrinsics.h>
#include <libkernel/libkernel.h>
#include <plugin.h>
#include <vectormath.h>
#include <std/math.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <job/job.h>
#include <thread/thread.h>
#include <regmgr/regmgr.h>
#include <common/shared_ptr.h>
#include "../../../shell.h"
#include "../../../theme.h"
#include "../../../plugins.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* Sony calls vector<int>::push_back out of line here */
PAF_VECTOR_NOINLINE_PUSH_BACK(int, paf::allocator<int>)
PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbIconListPtr, paf::allocator<LsdbIconListPtr>)
PAF_VECTOR_NOINLINE_PUSH_BACK(ApplauncherFolderImage, paf::allocator<ApplauncherFolderImage>)
PAF_VECTOR_NOINLINE_PUSH_BACK(ApplauncherFolderSurface, paf::allocator<ApplauncherFolderSurface>)

/* the slots of a page: the top row, the middle row and the bottom row */
#define APPLAUNCHER_PAGE_TOP_FIRST	0
#define APPLAUNCHER_PAGE_TOP_LAST	2
#define APPLAUNCHER_PAGE_MIDDLE_FIRST	3
#define APPLAUNCHER_PAGE_MIDDLE_LAST	6
#define APPLAUNCHER_PAGE_BOTTOM_FIRST	7
#define APPLAUNCHER_PAGE_BOTTOM_LAST	9

/*
 * PLACEHOLDER: an icon's move between the slots (0x81555250-0x8155527F): whether it has just
 * left a folder, just gone into one and just opened one; the icon dragged, the one moved aside
 * for it and the one it is over; the page and the slot its move began on; and the folder it
 * last left: that folder's page and slot and the index of the folder's own page.
 */
static bool s_leftFolder;
static bool s_enteredFolder;
static bool s_folderOpened;
common::SharedPtr<ApplauncherIcon> g_applauncherDragItem;
static common::SharedPtr<ApplauncherIcon> s_moveIcon;
common::SharedPtr<ApplauncherIcon> g_applauncherDropTarget;
static int s_moveStartPage;
static int s_moveStartSlot;
static int s_folderPage;
static int s_folderSlot;
static int s_folderAppPage;

/* PLACEHOLDER: the seconds the icon has been held out of its folder, and over a folder
   (0x81544274, 0x81544278) */
static float s_leaveTime = 0.0f;
static float s_enterTime = 0.0f;

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

	if (page == NULL)
		return;
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (page->m_slots[i].get() != icon.get())
			continue;

		if (page->m_type == 2 && fromFolder && page->m_focus.get() != NULL)
			page->m_focus->RemoveChild(icon->m_data->titleId.c_str());
		icon->m_page->m_changed = true;
		icon->m_page->m_slots[i].reset();
		icon->m_page = NULL;
		icon->m_slot = -1;
		if (g_applauncherState->m_settings->m_sortEnabled)
			page->SetEmpty(page->IsEmpty());
		page->UpdateFocusLinks();
		return;
	}
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
			icon->m_icon->m_widget->SetParent(GetRoot(), NULL, 0);
		if (icon->m_page->m_type != 0)
			edge = applauncherIconWidgetFolderSize();
		else
			edge = applauncherIconWidgetSize();
		size = math::v4(edge, edge, 0.0f, 0.0f);
		applauncherIconWidgetUpdateLabel(icon->m_icon);
		icon->m_icon->m_widget->SetSize(size, NULL, ui::Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		applauncherIconWidgetUpdateMark(icon->m_icon);

		if (icon.get() == g_applauncherDragItem.get()) {
			ui::AppIconBase *widget = icon->m_icon->m_widget;

			if (widget->m_touch_captures.size() != 0 ||
			    widget->m_icon_mode == APPLAUNCHER_PAGE_ICON_MODE_DRAG) {
				math::v4 held;

				widget->KillObjAnim(0, APPLAUNCHER_PAGE_ANIM_ICON_SIZE);
				widget = icon->m_icon->m_widget;
				held = size * 1.2f;
				widget->SetObjAnim(0, APPLAUNCHER_PAGE_ANIM_ICON_SIZE, held,
					new Timer(100.0f, Timer::FUNC_QUAD_OUT), 0);
			} else {
				widget->KillObjAnim(0, APPLAUNCHER_PAGE_ANIM_ICON_SIZE);
				icon->m_icon->m_widget->SetObjAnim(0, APPLAUNCHER_PAGE_ANIM_ICON_SIZE, size,
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

	if (page == NULL)
		return;
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (page->m_slots[i].get() != icon)
			continue;

		page->m_changed = true;
		page->m_slots[i].reset();
		icon->m_page = NULL;
		icon->m_slot = -1;
		if (g_applauncherState->m_settings->m_sortEnabled)
			page->SetEmpty(page->IsEmpty());
		return;
	}
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
	s_folderPage = -1;
	s_folderSlot = -1;
	s_folderAppPage = -1;
	m_moveSlot = s_moveStartSlot;
	s_leftFolder = false;
	s_enteredFolder = false;
	s_folderOpened = false;
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
	float root;

	if (length < 1.0f)
		length = 1.0f;
	root = __builtin_sqrtf(length);
	float time = __builtin_sqrtf(root) * 60.0f;
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
			ui::Widget *widget = s_moveIcon->m_icon->m_widget;

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
		ui::Widget *widget = g_applauncherDropTarget->m_icon->m_widget;

		widget->KillAnim(ui::Widget::ANIM_COLOR);
		widget->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
				 new Timer(200.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 0);
		widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				 new Timer(200.0f, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_SCALE, 0);
	}
	if (icon.get() != NULL) {
		ui::Widget *widget = icon->m_icon->m_widget;

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

/* PLACEHOLDER: how far (in pixels) the icon is dragged off a folder's page before it leaves
   the folder, and the seconds it waits there or over a folder: longer just after it has gone
   into or out of one */
#define APPLAUNCHER_PAGE_LEAVE_DISTANCE	450.0f
#define APPLAUNCHER_PAGE_WAIT		0.7f
#define APPLAUNCHER_PAGE_WAIT_AGAIN	2.0f
/* PLACEHOLDER: the drag's phases in which the icon leaves the folder at once */
#define APPLAUNCHER_PAGE_PHASE_LEAVE	3
#define APPLAUNCHER_PAGE_PHASE_CANCEL	4

/*
 * The dragged icon moved over the page: the slot nearest to it found; on a folder's page, the
 * folder left (closed, the folder deleted when the icon was its last) when the icon is far
 * off it or the drag says so, after it has waited; otherwise the icon put in the slot, the
 * slot's icon moved aside for it, or, over a folder, the folder opened after it has waited.
 * The page the view goes to is given back, NULL when it stays (0x810B551E).
 */
ApplauncherPage *ApplauncherPage::iconMove(const common::SharedPtr<ApplauncherIcon> &icon,
					   bool held, int phase)
{
	sce::Vectormath::Simd::floatInVec best(-1.0f);
	ApplauncherPage *next = NULL;
	ui::Widget *widget = NULL;
	bool inFolder = false;
	SceUInt32 i;

	if (icon.get() != NULL) {
		widget = icon->m_icon->m_widget;
		ui::Widget *layer = g_applauncherHome->m_scroller.m_scrollView->m_parent;

		if (layer != icon->GetWidget()->m_parent)
			icon->GetWidget()->SetParent(g_applauncherHome->m_scroller.m_scrollView->m_parent,
						     NULL, 0);
	}
	if (m_type == 2)
		inFolder = true;
	m_moveSlot = -1;

	math::v4 pos = widget->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	sce::Vectormath::Simd::Aos::Vector2 at(vget_low_f32(pos.getImpl().get128()));
	at += sce::Vectormath::Simd::Aos::Vector2(
		vget_low_f32(widget->m_parent->GetWorldMatrix().w().getImpl().get128()));
	ui::Widget *root = GetWidget();
	sce::Vectormath::Simd::Aos::Vector2 origin(
		vget_low_f32(root->GetWorldMatrix().w().getImpl().get128()));
	if (!inFolder)
		at -= origin;

	for (i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		sce::Vectormath::Simd::floatInVec distance =
			sce::Vectormath::Simd::powf(GetSlotPos(i).getImpl().getX() - at.getX(),
						    sce::Vectormath::Simd::floatInVec(2.0f)) +
			sce::Vectormath::Simd::powf(GetSlotPos(i).getImpl().getY() - at.getY(),
						    sce::Vectormath::Simd::floatInVec(2.0f));

		if (best < sce::Vectormath::Simd::floatInVec(0.0f) || distance < best) {
			best = distance;
			m_moveSlot = i;
		}
	}
	g_applauncherHome->UpdateLoadedPages(false);

	if (inFolder &&
	    ((m_moveSlot >= 0 &&
	      sce::Vectormath::Simd::Aos::length(at) > APPLAUNCHER_PAGE_LEAVE_DISTANCE) ||
	     phase == APPLAUNCHER_PAGE_PHASE_LEAVE || phase == APPLAUNCHER_PAGE_PHASE_CANCEL)) {
		float wait = s_enteredFolder ? APPLAUNCHER_PAGE_WAIT_AGAIN : APPLAUNCHER_PAGE_WAIT;

		if (phase == APPLAUNCHER_PAGE_PHASE_LEAVE || phase == APPLAUNCHER_PAGE_PHASE_CANCEL)
			s_leaveTime = wait;
		s_leaveTime += Timer::s_tick_unit * Timer::s_tick_interval;
		if (s_leaveTime >= wait) {
			common::SharedPtr<ApplauncherIcon> folder;
			ApplauncherPage *page = icon->m_page;

			if (page != NULL && page->GetIconCount() == 1 && page->m_focus.get() != NULL) {
				folder = page->m_focus;
				if (folder.get() != NULL) {
					s_folderPage = folder->m_page->GetIndex();
					s_folderSlot = folder->m_moveSlot;
					s_folderAppPage = page->GetIndex();
				}
			}
			applauncherIconRemove(icon, true);
			setMovedIcon(common::SharedPtr<ApplauncherIcon>());
			setDropTarget(common::SharedPtr<ApplauncherIcon>());
			if (icon->m_movePage == applauncherHomeGetAppPage()) {
				icon->m_movePage = NULL;
				icon->m_moveSlot = -1;
			}
			applauncherHomeCloseAppPage(false);
			if (folder.get() != NULL) {
				applauncherIconRemove(folder, true);
				applauncherIconDelete(folder, false, true);
			}
			s_enteredFolder = false;
			s_leftFolder = true;
			ShellSnapListItem *item = g_applauncherHome->m_scroller.GetFocused();

			return item != NULL ? static_cast<ApplauncherPage *>(item) : NULL;
		}
		m_moveSlot = -1;
	} else {
		s_leaveTime = 0.0f;
		if (!s_folderOpened)
			s_enteredFolder = false;
	}

	s_folderOpened = false;
	if (m_moveSlot < 0) {
		s_leftFolder = false;
		return next;
	}

	common::SharedPtr<ApplauncherIcon> target = m_slots[m_moveSlot];

	if (target.get() == NULL) {
		setDropTarget(common::SharedPtr<ApplauncherIcon>());
		applauncherIconRemove(icon, true);
		PutIcon(icon, m_moveSlot, false, true);
		UpdateFocusLinks();
		setMovedIcon(common::SharedPtr<ApplauncherIcon>());
	} else if (target.get() == icon.get()) {
		setDropTarget(common::SharedPtr<ApplauncherIcon>());
	} else {
		ui::Widget *targetWidget = target->m_icon->m_widget;

		if (target->IsFolder() && !icon->IsFolder()) {
			/* over a folder: it opens after the icon has waited over it */
			if (g_applauncherDropTarget.get() != target.get()) {
				applauncherIconRemove(icon, true);
				setMovedIcon(common::SharedPtr<ApplauncherIcon>());
				setDropTarget(target);
				s_enterTime = 0.0f;
				if (!held)
					s_leftFolder = false;
				return next;
			}

			float wait = s_leftFolder ? APPLAUNCHER_PAGE_WAIT_AGAIN : APPLAUNCHER_PAGE_WAIT;

			s_enterTime += Timer::s_tick_unit * Timer::s_tick_interval;
			if (s_enterTime < wait || applauncherHomeGetAppPage() != NULL)
				return next;
			s_leftFolder = false;
			s_folderOpened = true;
			s_enteredFolder = true;
			return applauncherHomeOpenAppPage(g_applauncherDropTarget, NULL, false);
		}

		setDropTarget(common::SharedPtr<ApplauncherIcon>());
		applauncherIconRemove(icon, true);
		if (setMovedIcon(target)) {
			math::v4 to = target->GetSlotPos();
			math::v4 color(1.0f, 1.0f, 1.0f, 1.0f);

			if (icon->m_homeSlot == -1 || (target->IsFolder() && !icon->IsFolder())) {
				applauncherIconRemove(target, true);
				color = math::v4(1.0f, 1.0f, 1.0f, 0.2f);
				to.set_z(-100.0f);
			} else {
				applauncherIconRemove(target, true);
				PutIcon(target, icon->m_homeSlot, true, true);
				to = target->GetSlotPos();
			}
			targetWidget->KillAnim(ui::Widget::ANIM_MOVE);
			targetWidget->KillAnim(ui::Widget::ANIM_COLOR);
			moveIconWidget(targetWidget, to, color);
		}
		PutIcon(icon, m_moveSlot, false, true);
		UpdateFocusLinks();
	}

	if (icon->m_page == NULL) {
		PutIcon(icon, m_moveSlot, false, true);
		UpdateFocusLinks();
	}
	s_leftFolder = false;
	return next;
}

/* PLACEHOLDER: the ids of the icons in folders (-20000000 up to -10000000) */
#define APPLAUNCHER_PAGE_FOLDER_ID_FIRST	-20000000
#define APPLAUNCHER_PAGE_FOLDER_ID_END		-10000000
#define APPLAUNCHER_PAGE_IS_FOLDER_ID(id) \
	((id) >= APPLAUNCHER_PAGE_FOLDER_ID_FIRST && (id) < APPLAUNCHER_PAGE_FOLDER_ID_END)

/*
 * PLACEHOLDER: the icons of the ids that are in folders added to the list once each, with the
 * page and the slot each is in (0x810B6826).
 */
static void listRemovedIcons(vector<ApplauncherRemovedIcon> *removed, const vector<int> &ids)
{
	for (SceUInt32 i = 0; i < ids.size(); i++) {
		int id = ids[i];

		if (!APPLAUNCHER_PAGE_IS_FOLDER_ID(id))
			continue;

		vector<ApplauncherRemovedIcon>::iterator it;

		for (it = removed->begin(); it != removed->end(); ++it) {
			if (it->m_id == id)
				break;
		}
		if (it != removed->end())
			continue;

		common::SharedPtr<ApplauncherIcon> icon = g_applauncherHome->FindIconById(ids[i]);

		if (icon.get() != NULL && icon->m_page != NULL) {
			ApplauncherRemovedIcon entry;

			entry.m_id = id;
			entry.m_pageIndex = icon->m_page->GetIndex();
			entry.m_slot = icon->m_slot;
			removed->push_back(entry);
		}
	}
}

/* PLACEHOLDER: the icon's widget moved to the position over a time that grows with the
   fourth root of the way to go (0x810B6980) */
static void moveIconWidget(ui::Widget *widget, const math::v4 &pos)
{
	math::v4 from = widget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v4 way = pos - from;
	float length = sce::Vectormath::Simd::Aos::length(
		sce::Vectormath::Simd::Aos::Vector2(vget_low_f32(way.getImpl().get128())));
	float root;

	if (length < 1.0f)
		length = 1.0f;
	root = __builtin_sqrtf(length);
	float time = __builtin_sqrtf(root) * 60.0f;
	widget->SetPos(pos, new Timer(time, Timer::FUNC_QUART_OUT), ui::Widget::ANIM_MOVE, 2);
}

/* the last free slot, -1 when none is (0x810B6ACC) */
int ApplauncherPage::GetFreeSlot()
{
	for (int i = APPLAUNCHER_PAGE_SLOTS - 1; i >= 0; i--) {
		if (m_slots[i].get() == NULL)
			return i;
	}
	return -1;
}

/*
 * PLACEHOLDER: the dropped icon put on a page with room: the page in view from a folder's
 * page, else the page or the next one; else a page added after it (the two page indexes moved
 * on past it), else the last page with a free slot. The icon's widget kept where it is on the
 * screen and moved to its slot from there (0x810B6AE6).
 */
static void placeIcon(ApplauncherPage *page, const common::SharedPtr<ApplauncherIcon> &icon,
		      int *pageIndex, int *otherIndex)
{
	ApplauncherPage *from;
	ApplauncherPage *dest;
	int slot = -1;

	if (page->m_type == 2) {
		ShellSnapListItem *item = g_applauncherHome->m_scroller.GetFocused();

		dest = item != NULL ? static_cast<ApplauncherPage *>(item) : NULL;
		from = dest;
		if (dest != NULL) {
			slot = dest->GetEmptySlot();
			if (slot >= 0)
				goto place;
		}
	} else {
		from = page;
	}
	dest = applauncherHomeGetNextPage(from);
	if (dest != NULL)
		slot = dest->GetEmptySlot();

place:
	ui::Widget *widget = icon->m_icon->m_widget;
	math::v4 screen = widget->GetWorldMatrix().w();

	if (slot == -1) {
		dest = applauncherHomeAddPage(NULL, from, false, true);
		if (dest != NULL) {
			if (!APPLAUNCHER_PAGE_IS_FOLDER_ID(*pageIndex) && *pageIndex >= 0 &&
			    dest->GetIndex() <= *pageIndex)
				(*pageIndex)++;
			if (!APPLAUNCHER_PAGE_IS_FOLDER_ID(*otherIndex) && *otherIndex >= 0 &&
			    dest->GetIndex() <= *otherIndex)
				(*otherIndex)++;
			slot = 0;
		} else {
			for (int i = g_applauncherHome->m_scroller.m_items.size() - 1; i >= 0; i--) {
				ShellSnapListItem *item = g_applauncherHome->m_scroller.m_items[i];
				ApplauncherPage *last = item != NULL ?
					static_cast<ApplauncherPage *>(item) : NULL;
				int free = last->GetFreeSlot();

				if (free >= 0) {
					dest = last;
					slot = free;
					break;
				}
			}
		}
	}

	dest->PutIcon(icon, slot, true, true);
	dest->UpdateFocusLinks();
	icon->m_movePage = dest;
	icon->m_homeSlot = slot;
	screen = screen - icon->m_page->GetWidget()->GetWorldMatrix().w();
	widget->SetPos(screen, NULL, ui::Widget::ANIM_MOVE, 0);
	moveIconWidget(widget, icon->GetSlotPos(), math::v4(1.0f, 1.0f, 1.0f, 1.0f));
}

static void onPageLayoutRun(ApplauncherLsdbJob *job, void *arg);
static void onPageLayoutDone(ApplauncherLsdbJob *job, void *arg);

/*
 * PLACEHOLDER name (after iconMoveBegin and iconMove): the dragged icon let go of on the page.
 * Every icon of the page moved back to its slot. When the home is full, the icon goes back to
 * its slot; else, out of the application's page, it goes on the page in view, a folder it
 * leaves alone deleted. The icon moved aside, or the dragged one without room, goes on a page
 * with room. An icon over a folder that takes it goes in; else it goes back where its move
 * began. The moves are saved by a SetPageLayoutJob, an emptied folder deleted first, and the
 * view jumps to the icon's page (0x810B6D62).
 */
void ApplauncherPage::iconMoveEnd(const common::SharedPtr<ApplauncherIcon> &icon)
{
	int startPage = -1;
	int startSlot = -1;
	int movedPage = -1;
	int movedSlot = -1;
	vector<int> fromPages;
	vector<int> fromPositions;
	vector<int> toPages;
	vector<int> toPositions;
	bool full = true;
	bool noRoom = false;

	s_leftFolder = false;
	s_enteredFolder = false;
	s_folderOpened = false;

	/* full: ten pages, and no icon page with a free slot */
	if (g_applauncherHome->m_scroller.m_items.size() >= 10) {
		for (ShellSnapList::iterator it = g_applauncherHome->m_scroller.m_items.begin();
		     it != g_applauncherHome->m_scroller.m_items.end(); ++it) {
			ApplauncherPage *page = static_cast<ApplauncherPage *>(*it);

			if (page->m_type == 0 && page->GetFreeSlot() >= 0) {
				full = false;
				break;
			}
		}
	} else {
		full = false;
	}

	if (s_folderPage >= 0 && s_folderSlot >= 0) {
		fromPages.push_back(s_folderPage);
		fromPositions.push_back(s_folderSlot);
		toPages.push_back(-1);
		toPositions.push_back(-1);
	}
	if (icon.get() != NULL) {
		startPage = s_moveStartPage;
		startSlot = s_moveStartSlot;
		if (s_moveIcon.get() != NULL) {
			movedPage = s_moveIcon->m_movePage->GetIndex();
			movedSlot = s_moveIcon->m_moveSlot;
		}
	}

	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() == NULL)
			continue;

		ApplauncherIconView *view = m_slots[i]->m_icon;

		m_slots[i]->m_movePage = m_slots[i]->m_page;
		m_slots[i]->m_homeSlot = m_slots[i]->m_slot;
		if (view != NULL) {
			ui::Widget *widget = view->m_widget;

			widget->KillAnim(ui::Widget::ANIM_MOVE);
			widget->KillAnim(ui::Widget::ANIM_COLOR);
			moveIconWidget(widget, m_slots[i]->GetSlotPos(),
				       math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		}
	}

	if (icon.get() != NULL) {
		ui::Widget *widget = icon->m_icon->m_widget;

		if (m_moveSlot == -1) {
			ApplauncherPage *focused = static_cast<ApplauncherPage *>(
				g_applauncherHome->m_scroller.GetFocused());

			if (full) {
				ApplauncherPage *page = icon->m_page;
				int slot = icon->m_slot;

				applauncherIconRemove(icon, true);
				page->PutIcon(icon, slot, true, true);
				page->UpdateFocusLinks();
				moveIconWidget(icon->GetWidget(), icon->GetSlotPos());
			} else {
				applauncherHomeCloseAppPage(false);
				if (focused != NULL) {
					int free = focused->GetEmptySlot();
					ApplauncherPage *from = icon->m_page;
					bool done = false;

					/* the last icon out of a folder: the folder undone, the icon
					   put in the folder's place */
					if (from != NULL && from->m_type == 2 &&
					    from->GetIconCount() == 1) {
						common::SharedPtr<ApplauncherIcon> folder = from->m_focus;

						if (folder.get() != NULL) {
							int slot = folder->m_moveSlot;
							ApplauncherPage *page = folder->m_movePage;

							applauncherIconRemove(folder, true);
							applauncherIconDelete(folder, false, true);
							applauncherIconRemove(icon, true);
							page->PutIcon(icon, slot, true, true);
							page->UpdateFocusLinks();
							fromPages.push_back(page->GetIndex());
							fromPositions.push_back(slot);
							toPages.push_back(-1);
							toPositions.push_back(-1);
							done = true;
						}
					}
					if (!done) {
						if (free < 0) {
							noRoom = true;
						} else {
							applauncherIconRemove(icon, true);
							focused->PutIcon(icon, free, true, true);
							focused->UpdateFocusLinks();
						}
					}
				}
			}
		}
		if (m_moveSlot != -1)
			m_moveSlot = -1;
		widget->KillAnim(ui::Widget::ANIM_MOVE);
		if (icon->m_page != NULL)
			moveIconWidget(widget, icon->GetSlotPos());

		if (s_moveIcon.get() != NULL && s_moveIcon->m_page == NULL && !full)
			placeIcon(this, s_moveIcon, &startPage, &movedPage);
		if (noRoom) {
			ApplauncherPage *page = static_cast<ApplauncherPage *>(
				g_applauncherHome->m_scroller.GetFocused());

			if (page != NULL)
				placeIcon(page, icon, &startPage, &movedPage);
		}

		/* the icon moved aside: its move saved, or back in its folder */
		if (s_moveIcon.get() != NULL) {
			if (s_moveIcon->m_page != NULL) {
				if (s_moveIcon->m_page->GetIndex() != movedPage ||
				    s_moveIcon->m_slot != movedSlot) {
					fromPages.push_back(movedPage);
					fromPositions.push_back(movedSlot);
					toPages.push_back(s_moveIcon->m_page->GetIndex());
					toPositions.push_back(static_cast<int>(s_moveIcon->m_slot));
					s_moveIcon->m_movePage = s_moveIcon->m_page;
					s_moveIcon->m_moveSlot = s_moveIcon->m_slot;
					s_moveIcon->m_homeSlot = s_moveIcon->m_moveSlot;
				}
			} else {
				int slot = -1;

				if (APPLAUNCHER_PAGE_IS_FOLDER_ID(startPage))
					slot = startSlot;
				fromPages.push_back(movedPage);
				fromPositions.push_back(movedSlot);
				toPages.push_back(startPage);
				toPositions.push_back(slot);
				s_moveIcon->PutInFolder(g_applauncherHome->FindIconById(startPage));
			}
		}

		/* the dragged icon: its move saved, or its move's start kept */
		if (icon->m_page != NULL && (icon->m_page->GetIndex() != startPage ||
					     icon->m_slot != startSlot)) {
			if (g_applauncherDropTarget.get() == NULL) {
				fromPages.push_back(startPage);
				fromPositions.push_back(startSlot);
				toPages.push_back(icon->m_page->GetIndex());
				toPositions.push_back(static_cast<int>(icon->m_slot));
				icon->m_movePage = icon->m_page;
				icon->m_moveSlot = icon->m_slot;
				icon->m_homeSlot = icon->m_moveSlot;
			}
		} else if (icon->m_page != NULL && icon->m_page->GetIndex() == startPage &&
			   icon->m_slot == startSlot) {
			icon->m_movePage = icon->m_page;
			icon->m_moveSlot = icon->m_slot;
			icon->m_homeSlot = icon->m_moveSlot;
		}

		/* over a folder: in it when it takes the icon, else back where the move began */
		if (g_applauncherDropTarget.get() != NULL) {
			if (IsOpened() && g_applauncherDropTarget->m_data->folder.get() != NULL &&
			    g_applauncherDropTarget->CanAddChild(icon->m_data->titleId.c_str())) {
				if (startPage == g_applauncherDropTarget->m_data->folder->id) {
					string titleId(icon->m_data->titleId);

					icon->PutInFolder(g_applauncherDropTarget);
				} else {
					fromPages.push_back(startPage);
					fromPositions.push_back(s_moveStartSlot);
					toPages.push_back(g_applauncherDropTarget->m_data->folder->id);
					toPositions.push_back(-1);
					icon->PutInFolder(g_applauncherDropTarget);
				}
				if (g_applauncherDropTarget.get() != NULL)
					g_applauncherDropTarget->GetWidget()->SetFocus(NULL);
			} else if (!APPLAUNCHER_PAGE_IS_FOLDER_ID(startPage)) {
				if (startPage < (int)g_applauncherHome->m_scroller.m_items.size()) {
					math::v4 screen = widget->GetWorldMatrix().w();
					ApplauncherPage &page = static_cast<ApplauncherPage &>(
						*g_applauncherHome->m_scroller.m_items.at(startPage));
					math::v4 pos = screen - page.GetWidget()->GetWorldMatrix().w();

					pos.set_z(widget->GetPos(0, NULL,
						math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_z());
					pos.set_w(widget->GetPos(0, NULL,
						math::v4(0.0f, 0.0f, 0.0f, 0.0f)).extract_w());
					widget->SetPos(pos, NULL, ui::Widget::ANIM_MOVE, 0);
					static_cast<ApplauncherPage &>(
						*g_applauncherHome->m_scroller.m_items.at(startPage))
						.PutIcon(icon, startSlot, true, true);
					icon->m_page->UpdateFocusLinks();
					icon->m_movePage = icon->m_page;
					icon->m_homeSlot = startSlot;
					moveIconWidget(widget, icon->GetSlotPos());
					sceClibPrintf("[SceShell] revertTab %p %p\n", icon->m_page,
						      icon.get());
				}
			} else if (s_folderAppPage == startPage) {
				fromPages.push_back(startPage);
				fromPositions.push_back(startSlot);
				toPages.push_back(s_folderPage);
				toPositions.push_back(s_folderSlot);
				if (s_folderPage < (int)g_applauncherHome->m_scroller.m_items.size()) {
					static_cast<ApplauncherPage &>(
						*g_applauncherHome->m_scroller.m_items.at(s_folderPage))
						.PutIcon(icon, s_folderSlot, true, true);
					icon->m_movePage = icon->m_page;
					icon->m_moveSlot = icon->m_slot;
					icon->m_homeSlot = icon->m_moveSlot;
					icon->m_page->UpdateFocusLinks();
					moveIconWidget(widget, icon->GetSlotPos());
				}
			} else {
				icon->PutInFolder(g_applauncherHome->FindIconById(startPage));
			}
			setDropTarget(common::SharedPtr<ApplauncherIcon>());
		}

		if (fromPages.size() != 0) {
			common::SharedPtr<ApplauncherIcon> folder =
				g_applauncherHome->FindIconById(startPage);

			/* the folder the icon left, emptied: deleted */
			if (folder.get() != NULL) {
				LsdbFolder *data = folder->m_data->folder.get();

				if (data != NULL && data->iconCount == 0) {
					fromPages.push_back(folder->m_movePage->GetIndex());
					fromPositions.push_back(folder->m_moveSlot);
					toPages.push_back(-1);
					toPositions.push_back(-1);
					applauncherIconRemove(folder, true);
					applauncherIconDelete(folder, false, true);
				}
			}

			vector<ApplauncherRemovedIcon> removed;

			listRemovedIcons(&removed, fromPages);
			listRemovedIcons(&removed, toPages);
			applauncherSetPageLayoutJobStart(((job::JobQueue *(*)(void))
				g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE])(),
				fromPages, fromPositions, toPages, toPositions, removed,
				onPageLayoutRun, onPageLayoutDone, NULL);
		}
		s_moveIcon.reset();
		if (icon->m_page != NULL && icon->m_page->m_type != 2)
			applauncherHomeJumpTo(icon->m_page);
	}
	UpdateFocusLinks();
	s_folderPage = -1;
	s_folderSlot = -1;
	s_folderAppPage = -1;
}

/* the page's view shown empty, or not (0x810B7DD4) */
void ApplauncherPageBase::SetEmpty(bool empty)
{
	if (m_view != NULL)
		m_view->SetEmpty(empty);
}

/* PLACEHOLDER name: the page's view refreshed (0x810B7DE2) */
void ApplauncherPageBase::Refresh()
{
	if (m_view != NULL)
		m_view->Refresh();
}

/* the icon of the title in a slot, none when no slot has it (0x810B7DEE) */
common::SharedPtr<ApplauncherIcon> ApplauncherPage::FindIcon(const string &titleId)
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL && m_slots[i]->m_data->titleId == titleId)
			return m_slots[i];
	}
	return common::SharedPtr<ApplauncherIcon>();
}

/*
 * The icons' widgets taken from the pool, once unless forced; an icon just put on the home is
 * hidden at once, to be faded in (0x810B7E5A).
 */
void ApplauncherPage::LoadIcons(bool force)
{
	if (m_iconsLoaded && !force)
		return;

	m_iconsLoaded = true;
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() == NULL)
			continue;

		m_slots[i]->LoadView();
		if (m_slots[i]->m_added && m_slots[i]->GetWidget() != NULL) {
			ui::Widget *widget = m_slots[i]->GetWidget();

			widget->m_transition_complete = false;
			widget->m_transition = 0xff;
			widget->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
		}
	}
	UpdateFocusLinks();
}

/* the selected marks of its icons cleared (0x810B7EF2) */
void ApplauncherPage::UnselectAll()
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL)
			m_slots[i]->m_selected = false;
	}
}

/* the slot whose icon has the cursor, -1 without one or before its icons are loaded
   (0x810B7F08) */
int ApplauncherPage::GetCursorSlot()
{
	if (!m_iconsLoaded)
		return -1;

	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL && m_slots[i]->GetWidget() != NULL &&
		    m_slots[i]->GetWidget()->m_cursor_on)
			return i;
	}
	return -1;
}

/* the slot's icon given the focus, once its icons are loaded (0x810B7F48) */
void ApplauncherPage::FocusSlot(int slot)
{
	if (m_iconsLoaded && slot >= 0 && slot < APPLAUNCHER_PAGE_SLOTS &&
	    m_slots[slot].get() != NULL && m_slots[slot]->GetWidget() != NULL)
		m_slots[slot]->GetWidget()->SetFocus(NULL);
}

/* each icon's effect resumed (0x810B7F7A) */
void ApplauncherPage::ResumeIcons()
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL)
			m_slots[i]->Resume();
	}
}

/* an icon fits after the last one: not the last, empty page, and its last slot free
   (0x810B7F94) */
bool ApplauncherPage::CanAddIcon()
{
	return m_type != 1 && m_slots[APPLAUNCHER_PAGE_SLOTS - 1].get() == NULL;
}

/* the icon in the last slot used, none on an empty page (0x810B7FA8) */
common::SharedPtr<ApplauncherIcon> ApplauncherPage::GetLastIcon()
{
	for (int i = APPLAUNCHER_PAGE_SLOTS - 1; i >= 0; i--) {
		if (m_slots[i].get() != NULL)
			return m_slots[i];
	}
	return common::SharedPtr<ApplauncherIcon>();
}

/* PLACEHOLDER: the path a folder's child's icon is shown from (2048 bytes, the longest) */
#define APPLAUNCHER_PAGE_PATH_MAX	2048
/* PLACEHOLDER: an icon image's job done (CacheImageInterface::GetJobStatus) */
#define APPLAUNCHER_PAGE_IMAGE_DONE	2

/*
 * The folders' icons drawn again: each folder's children read from the database, their images
 * loaded (the parental lock's for a child locked above the console's level, the theme's when it
 * has icons), waited for, and drawn together by the icon capture; the drawing saved as the
 * folder's icon in its page and slot (0x810B7FE0).
 */
void applauncherPagesRemoveIcons(vector<ApplauncherRemovedIcon> *icons)
{
	int parental = 0;
	char path[APPLAUNCHER_PAGE_PATH_MAX];

	sceRegMgrGetKeyInt("/CONFIG/SECURITY/PARENTAL", "content_start_control", &parental);
	for (SceUInt32 i = 0; i < icons->size(); i++) {
		int id = (*icons)[i].m_id;
		int page = (*icons)[i].m_pageIndex;
		int slot = (*icons)[i].m_slot;
		LsdbPageIconLists lists;

		lists.push_back(LsdbIconListPtr(new LsdbIconList()));
		if (SceLsdb_D74AC354(g_shellLsdbClient->client, id, 1, &lists) != 0)
			continue;

		vector<ApplauncherFolderImage> images;

		for (SceUInt32 j = 0; j < lists[0]->size(); j++) {
			const LsdbIconPtr &child = lists[0]->at(j);
			ApplauncherFolderImage image;
			int level;

			image.m_pos = child->pos;
			level = child->parentalLockLv;
			if (level == 99 || (parental != 0 && level != 0 && level > parental)) {
				sce_paf_strncpy(path, "vs0:data/internal/icon/parental_lock.png",
						sizeof(path));
			} else if (shellThemeIconsEnabled()) {
				thread::RMutex::main_thread_mutex.Lock();

				common::SharedPtr<LsdbThemeInfo> *info = shellThemeGetInfo();

				if (info->get() != NULL) {
					const IconParam *param = info->get()->m_homeProperty.GetIconParam(
						child->titleId.c_str(), 0);

					if (param != NULL && param->m_iconFilePath.length() != 0)
						sce_paf_snprintf(path, sizeof(path), "%s%s",
								 info->get()->path.c_str(),
								 param->m_iconFilePath.c_str());
				}
				thread::RMutex::main_thread_mutex.Unlock();
			} else {
				sce_paf_strncpy(path, child->iconPath.c_str(), sizeof(path));
			}
			applauncherIconCacheLoad(string(path), &image.m_image, NULL, 0, false);
			images.push_back(image);
		}

		for (SceUInt32 j = 0; j < images.size(); j++) {
			if (images[j].m_image.get() == NULL)
				continue;
			while (images[j].m_image->GetJobStatus() != APPLAUNCHER_PAGE_IMAGE_DONE)
				thread::Sleep(16);
		}

		vector<ApplauncherFolderSurface> surfaces;

		for (SceUInt32 j = 0; j < images.size(); j++) {
			if (images[j].m_image.get() == NULL ||
			    images[j].m_image->GetImage().get() == NULL)
				continue;

			ApplauncherFolderSurface surface;

			surface.m_pos = images[j].m_pos;
			surface.m_surface = images[j].m_image->GetImage();
			surfaces.push_back(surface);
		}

		if (surfaces.size() != 0) {
			int size = 0;
			void *png = applauncherIconCaptureFolder(&surfaces, &size);
			LsdbIconPtr icon(new LsdbIcon());

			icon->pos = slot;
			SceLsdb_C1A82AC8(g_shellLsdbClient->client, page, &icon, LSDB_ICON_FIELD_IMAGE,
					 png, size);
			if (png != NULL)
				sce_paf_free(png);
		}
	}
}

/* the page of the type, its slots empty (0x810B8516) */
ApplauncherPageBase::ApplauncherPageBase(int type)
	: m_type(type), m_state(0), m_moveSlot(-1), m_view(NULL), m_changed(false),
	  m_iconsLoaded(false)
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++)
		m_slots[i].reset();
}

/* PLACEHOLDER: how far (the square of 64 pixels) a folder's widget is from its slot before
   the slot's frame shows */
#define APPLAUNCHER_PAGE_FRAME_DISTANCE2	4096.0f

/*
 * The slots' frames in the edit: shown on an empty slot, an application's and a folder's moved
 * off its slot while shown; hidden under a folder in place (0x810B85C6).
 */
void ApplauncherPage::UpdateSlotFrames(bool animate)
{
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_view == NULL)
			continue;

		if (m_slots[i].get() != NULL && m_slots[i]->IsFolder()) {
			ui::Widget *widget = m_slots[i]->GetWidget();

			if (widget == NULL)
				continue;

			float32x4_t pos = widget->GetPos(0, NULL,
							 math::v4(0.0f, 0.0f, 0.0f, 0.0f)).getImpl().get128();
			float32x2_t xy = vsub_f32(vget_low_f32(pos),
						  vget_low_f32(m_slots[i]->GetSlotPos().getImpl().get128()));

			xy = vmul_f32(xy, xy);
			if (vget_lane_f32(vpadd_f32(xy, xy), 0) < APPLAUNCHER_PAGE_FRAME_DISTANCE2 ||
			    widget->m_show_alpha < 0.99f)
				m_view->HideSlotFrame(i, animate);
			else
				m_view->ShowSlotFrame(i, animate);
		} else {
			m_view->ShowSlotFrame(i, animate);
		}
	}
}

/*
 * The edit begun on the page: its view refreshed while the home has fewer than ten pages, its
 * icons and its view put in the edit, the slots' frames set at once (0x810B86B4).
 */
void ApplauncherPage::BeginEdit()
{
	m_state = 1;
	if (g_applauncherHome->m_scroller.m_items.size() < 10)
		Refresh();
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL)
			m_slots[i]->BeginEdit(true);
	}
	if (m_view != NULL)
		m_view->BeginEdit(IsEmpty());
	UpdateSlotFrames(false);
	m_changed = false;
}

/* the page out of the edit, its view's and its icons' edit ended (0x810B871E) */
void ApplauncherPage::EndEdit()
{
	m_state = 0;
	if (m_view != NULL)
		m_view->EndEdit();
	for (SceUInt32 i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		if (m_slots[i].get() != NULL)
			m_slots[i]->EndEdit();
	}
	UpdateFocusLinks();
	m_changed = false;
}

/*
 * Its widgets taken from the page pool and its icons' views loaded, the icons' views again on
 * a fast load; the last, empty page takes none (0x810B8752).
 */
void ApplauncherPage::Load(bool fast)
{
	if (m_view != NULL) {
		if (fast)
			LoadIcons(fast);
	} else if (m_type != 1) {
		m_view = applauncherPageViewGet(this, fast);
		LoadIcons(false);
	}
}

/* PLACEHOLDER: a SetPageLayoutJob run: its folders' icons taken off the pages' saved order
   (0x810B877E) */
static void onPageLayoutRun(ApplauncherLsdbJob *job, void *arg)
{
	applauncherPagesRemoveIcons(&static_cast<SetPageLayoutJob *>(job)->m_removed);
}

/* PLACEHOLDER: a SetPageLayoutJob done: its folders' icons updated (0x810B8788) */
static void onPageLayoutDone(ApplauncherLsdbJob *job, void *arg)
{
	SetPageLayoutJob *layout = static_cast<SetPageLayoutJob *>(job);

	for (SceUInt32 i = 0; i < layout->m_removed.size(); i++) {
		common::SharedPtr<ApplauncherIcon> folder =
			g_applauncherHome->FindIconById(layout->m_removed[i].m_id);

		if (folder.get() != NULL)
			folder->Update(true);
	}
}

/* PLACEHOLDER: the applauncher's request kept, outside the IDU mode and without one
   (0x810B8830) */
void applauncherPagesHoldRequest(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherState *state;

	if (shellStartupGetIduMode() == 1)
		return;
	state = g_applauncherState;
	if (state->m_requestId == 0 && state->m_requestId != -1)
		state->m_requestState = 1;
}
