/*
 * applauncher_pageview.cpp: the pool of page views, the widgets a page of the home takes while
 * it is in view: three for the home's pages and one for a folder's page, with the edit's buttons
 * and a folder's name (0x810C3B3C-0x810C61DC). The file's name and the names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include <graphics/drawobj/plane_obj.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/environment.h>
#include <widget/w_button_base.h>
#include <widget/w_composite_drawer.h>
#include <widget/w_text_box.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <timer/timer.h>
#include "../../livespace_db/lsdb_keys.h"
#include "../../../shell.h"
#include "applauncher.h"
#include "applauncher_home.h"
#include "applauncher_mode.h"

using namespace paf;

/* PLACEHOLDER: the templates of a folder's view and a page's, and the page's widget in them */
#define APPLAUNCHER_PAGE_VIEW_FOLDER_TEMPLATE	0xEED0F97E
#define APPLAUNCHER_PAGE_VIEW_TEMPLATE		0x428E23B7
#define APPLAUNCHER_PAGE_VIEW			0xAE029D8E
/* PLACEHOLDER: the slots' frames and their image, and the widget the icons go on */
#define APPLAUNCHER_PAGE_VIEW_SLOT_FRAMES	0xF1B811BF
#define APPLAUNCHER_PAGE_VIEW_SLOT_TEXTURE	0xA952EF1E
#define APPLAUNCHER_PAGE_VIEW_ROOT		0xC6063E76
/* PLACEHOLDER: a folder's name, its frame and the frame's image */
#define APPLAUNCHER_PAGE_VIEW_NAME		0xDB885465
#define APPLAUNCHER_PAGE_VIEW_NAME_FRAME	0xD676E8D1
#define APPLAUNCHER_PAGE_VIEW_NAME_TEXTURE	0xF9B2F483
/* PLACEHOLDER: the edit's buttons */
#define APPLAUNCHER_PAGE_VIEW_DELETE		0x8B6C584A
#define APPLAUNCHER_PAGE_VIEW_THEME		0x6A33D160
#define APPLAUNCHER_PAGE_VIEW_ADD		0x83A42A08

/* PLACEHOLDER: a folder's page, by its type */
#define APPLAUNCHER_PAGE_TYPE_FOLDER		2
/* PLACEHOLDER: the home's pages at most */
#define APPLAUNCHER_PAGES_MAX			10
/* PLACEHOLDER: the frames' draw object animation: their color */
#define APPLAUNCHER_PAGE_VIEW_OBJ_COLOR		0x20300
/* PLACEHOLDER: a folder's name's place in the edit and out of it */
#define APPLAUNCHER_PAGE_VIEW_NAME_EDIT_Y	270.0f
#define APPLAUNCHER_PAGE_VIEW_NAME_Y		246.0f
/* PLACEHOLDER: how far from the root's centre a touch still hits it, at the camera's
   distance */
#define APPLAUNCHER_PAGE_VIEW_HIT_RADIUS	416.0f

/* PLACEHOLDER: a folder's name as its edit began (0x815554B8) */
static wstring s_nameBefore;
/* PLACEHOLDER: the views free for the home's pages (0x81555490) and for a folder's
   (0x815554A0), those made (0x815554B0) and the folder's (0x815554B4) */
static vector<ApplauncherPageView *> s_freeViews((allocator<ApplauncherPageView *>()));
static vector<ApplauncherPageView *> s_freeFolderViews((allocator<ApplauncherPageView *>()));
static ApplauncherPageView *s_views;
static ApplauncherPageView *s_folderView;

static void onPageLongPress(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onDelete(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onTheme(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onAdd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onNameEditBegin(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onNameEditEnd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onNameEnter(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onFolderDecide(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onFolderLongPress(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onEditTouch(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onFolderKey(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);

/* shows and hides a widget over the timer's time (inline in paf's callers) */
static inline void ShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* the same over the edit's time, at once without one */
static inline void ShowWidgetEdit(ui::Widget *widget)
{
	float time = g_applauncherEditMoveTime;

	if (time > 0.0f)
		ShowWidget(widget, new Timer(time));
	else
		ShowWidget(widget, NULL);
}

static inline void HideWidgetEdit(ui::Widget *widget)
{
	float time = g_applauncherEditMoveTime;

	if (time > 0.0f)
		HideWidget(widget, new Timer(time));
	else
		HideWidget(widget, NULL);
}

/* the name's show hold taken and its focus events off, or given back and on */
static inline void HoldName(ApplauncherPageView *view)
{
	if (!view->m_nameHeld) {
		ScePafWidget_A96A0B4A(view->m_name, 1);
		view->m_name->EnableFocusEvent(false);
		view->m_nameHeld = true;
	}
}

static inline void ReleaseName(ApplauncherPageView *view)
{
	if (view->m_nameHeld) {
		ScePafWidget_A96A0B4A(view->m_name, -1);
		view->m_name->EnableFocusEvent(true);
		view->m_nameHeld = false;
	}
}

/* the page's widget opened in the home's foreground and named after the view, hidden; its
   slots' frames made at their places, and the buttons of the edit (a folder's: its name) given
   their handlers (0x810C3B3C) */
void ApplauncherPageView::Init(bool folder)
{
	if (folder) {
		m_isFolder = folder;
		g_applauncherPlugin->TemplateOpen(g_applauncherHome->m_foreground,
			IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_FOLDER_TEMPLATE),
			Plugin::TemplateOpenParam());
	} else {
		g_applauncherPlugin->TemplateOpen(g_applauncherHome->m_foreground,
			IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_TEMPLATE),
			Plugin::TemplateOpenParam());
	}
	m_widget = g_applauncherHome->m_foreground->FindChild(
		IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW), 0);
	if (m_widget == NULL)
		return;

	char name[64];

	sce_paf_snprintf(name, sizeof(name), "%p", this);
	m_widget->m_name.SetID(string(name));
	HideWidget(m_widget, NULL);

	m_slotFrames = (ui::CompositeDrawer *)m_widget->FindChild(
		IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_SLOT_FRAMES), 0);
	HideWidget(m_slotFrames, NULL);
	for (unsigned int i = 0; i < APPLAUNCHER_PAGE_SLOTS; i++) {
		common::SharedPtr<graph::DrawObj> plane(
			(graph::DrawObj *)new graph::PlaneObj(graph::PlaneObjStyle()));

		m_slotFrames->InsertDrawObj(plane, i, false);

		graph::DrawObj *obj = m_slotFrames->GetDrawObjByIndex(i);

		if (folder)
			obj->SetPos(math::v3(applauncherFolderPageGetSlotPos(i).extract_x(),
					     applauncherFolderPageGetSlotPos(i).extract_y(), 0.0f));
		else
			obj->SetPos(math::v3(applauncherPageGetSlotPos(i).extract_x(),
					     applauncherPageGetSlotPos(i).extract_y(), 0.0f));
		obj->SetTexture(g_applauncherPlugin->GetTexture(
			IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_SLOT_TEXTURE)), 0);
		((graph::PlaneObj *)obj)->SetScaleMode(graph::PlaneObj::SCALE_SURFACE,
						       graph::PlaneObj::SCALE_SURFACE);
	}
	ScePafWidget_CB95F8F2(m_slotFrames, 1);

	m_root = m_widget->FindChild(IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_ROOT), 0);
	m_root->EnableFocusEvent(false);

	ui::ButtonBase *root = (ui::ButtonBase *)m_root;

	root->m_enable_long_press = true;
	root->m_long_press_flag = false;
	root->m_long_press_msec = g_applauncherIconHoldTime;
	root->SetGestureMask(root->m_touch_gesture_mask | 4, false);

	if (!folder) {
		m_root->AddEventListener(ui::ButtonBase::CB_BTN_LONG_PRESS,
					 new ui::EventCBListener(onPageLongPress, this));

		m_deleteButton = m_widget->FindChild(
			IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_DELETE), 0);
		m_deleteButton->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
						 new ui::EventCBListener(onDelete, this));
		m_deleteButton->AddEventListener(0x40001, new ui::EventCBListener(onEditTouch, this));
		HideWidget(m_deleteButton, NULL);

		m_themeButton = m_widget->FindChild(
			IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_THEME), 0);
		m_themeButton->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
						new ui::EventCBListener(onTheme, this));
		m_themeButton->AddEventListener(0x40001, new ui::EventCBListener(onEditTouch, this));
		HideWidget(m_themeButton, NULL);

		m_addButton = m_widget->FindChild(IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_ADD), 0);
		m_addButton->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
					      new ui::EventCBListener(onAdd, this));
		m_addButton->AddEventListener(0x40001, new ui::EventCBListener(onEditTouch, this));
		HideWidget(m_addButton, NULL);
		return;
	}

	ui::CompositeDrawer *frame = (ui::CompositeDrawer *)m_widget->FindChild(
		IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_NAME_FRAME), 0);
	common::SharedPtr<graph::DrawObj> obj((graph::DrawObj *)new ApplauncherFolderNameFrame());

	frame->InsertDrawObj(obj, 0, true);
	frame->SetTexture(g_applauncherPlugin->GetTexture(
		IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_NAME_TEXTURE)), 0, 0);

	m_name = (ui::TextBox *)m_widget->FindChild(
		IDParam(s_empty_name, APPLAUNCHER_PAGE_VIEW_NAME), 0);
	m_name->AddEventListener(ui::TextBox::CB_TEXT_BOX_IME_OPEN,
				 new ui::EventCBListener(onNameEditBegin, this), true);
	m_name->AddEventListener(ui::TextBox::CB_TEXT_BOX_EDIT_END,
				 new ui::EventCBListener(onNameEditEnd, this), true);
	m_name->AddEventListener(ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED,
				 new ui::EventCBListener(onNameEnter, this), true);
	m_name->AddEventListener(0x40001, new ui::EventCBListener(onEditTouch, this), true);
	ScePafWidget_3E4AABD3(m_name, NULL);
	m_name->GetDrawObj(1)->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	HoldName(this);

	m_root->AddEventListener(ui::ButtonBase::CB_BTN_LONG_PRESS,
				 new ui::EventCBListener(onFolderLongPress, this));
	m_root->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
				 new ui::EventCBListener(onFolderDecide, NULL), true);
	ScePafWidget_21125870(m_root, 64);
	m_root->AddEventListener(0x10001, new ui::EventCBListener(onFolderKey, NULL));
}

/* the views made: three for the home's pages and the folder's (0x810C480E) */
void applauncherPageViewPoolCreate(void)
{
	unsigned int i;

	s_views = new ApplauncherPageView[3];
	for (i = 0; i < 3; i++) {
		s_views[i].Init(false);
		s_freeViews.push_back(&s_views[i]);
	}
	s_folderView = new ApplauncherPageView;
	s_folderView->Init(true);
	s_freeFolderViews.push_back(s_folderView);
}

/* (0x810C49B4) */
void applauncherPageViewPoolDestroy(void)
{
	delete[] s_views;
	delete s_folderView;
	s_views = NULL;
	s_folderView = NULL;
}

/* a free view for the page, shown on the page's widget: in the edit with its frames and
   buttons, a folder's with its name; NULL when none is free (0x810C4A3A) */
ApplauncherPageView *applauncherPageViewGet(ApplauncherPage *page, bool fast)
{
	if (page->m_type != APPLAUNCHER_PAGE_TYPE_FOLDER) {
		if (s_freeViews.size() == 0)
			return NULL;

		ApplauncherPageView *view = s_freeViews.back();

		if (view != NULL) {
			ui::Widget *widget = view->m_widget;

			s_freeViews.pop_back();
			view->m_page = page;
			ShowWidget(widget, NULL);
			widget->SetParent(page->GetWidget(), NULL, 0);
			if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != NULL) {
				view->m_root->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
				ShowWidget(view->m_slotFrames, NULL);
				ShowWidget(view->m_themeButton, NULL);
				if (g_applauncherHome->m_scroller.m_items.size() < APPLAUNCHER_PAGES_MAX)
					ShowWidget(view->m_addButton, NULL);
				if (page->IsEmpty())
					ShowWidget(view->m_deleteButton, NULL);
			} else {
				view->m_root->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
			}
		}
		return view;
	}

	if (s_freeFolderViews.size() == 0)
		return NULL;

	ApplauncherPageView *view = s_freeFolderViews.back();

	if (view != NULL) {
		ui::Widget *widget = view->m_widget;

		s_freeFolderViews.pop_back();
		view->m_page = page;
		ShowWidget(widget, NULL);
		widget->SetParent(page->GetWidget(), NULL, 0);

		{
			ui::TextBox *name = view->m_name;
			wstring text;

			common::Utf8ToUtf16(
				applauncherPageAsFolder(page)->m_icon.get()->m_data.get()->title, &text, 0);
			name->SetString(text);
		}
		if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != NULL) {
			view->m_name->SetPos(math::v4(0.0f, APPLAUNCHER_PAGE_VIEW_NAME_EDIT_Y,
						      0.0f, 0.0f));
			ShowWidget(view->m_slotFrames, NULL);
			ScePafWidget_164BFFA5(view->m_root, 64);
		} else {
			view->m_name->SetPos(math::v4(0.0f, APPLAUNCHER_PAGE_VIEW_NAME_Y,
						      0.0f, 0.0f));
			HideWidget(view->m_slotFrames, NULL);
			ScePafWidget_21125870(view->m_root, 64);
		}
	}
	return view;
}

/* the view hidden and put back in the foreground, out of the edit, and freed (0x810C4E74) */
void applauncherPageViewRelease(ApplauncherPageView *view)
{
	if (s_views == NULL || view == NULL)
		return;

	view->m_page = NULL;
	if (view->m_widget != NULL) {
		HideWidget(view->m_widget, NULL);
		view->m_widget->SetParent(g_applauncherHome->m_foreground, NULL, 0);
		HideWidget(view->m_slotFrames, NULL);
		if (view != s_folderView) {
			HideWidget(view->m_deleteButton, NULL);
			HideWidget(view->m_themeButton, NULL);
			HideWidget(view->m_addButton, NULL);
		} else if (view->m_name != NULL) {
			view->m_name->SetEditMode(false);
			view->m_name->EndEdit();
			view->m_name->SetObjAnim(1, APPLAUNCHER_PAGE_VIEW_OBJ_COLOR,
						 math::v4(1.0f, 1.0f, 1.0f, 0.0f),
						 new Timer(0.1f, Timer::FUNC_QUAD_OUT), 2);
			HoldName(view);
		}
	}
	if (view != s_folderView)
		s_freeViews.push_back(view);
	else
		s_freeFolderViews.push_back(view);
}

/* (0x810C5156) */
void ApplauncherPageView::SetEmpty(bool empty)
{
	if (m_deleteButton == NULL)
		return;
	if (empty)
		ShowWidget(m_deleteButton, NULL);
	else
		HideWidget(m_deleteButton, NULL);
}

/* the edit's buttons hidden, the frames faded out and the root's color with them; a folder's
   name out of its edit, back in its place (0x810C51E2) */
void ApplauncherPageView::EndEdit()
{
	if (m_themeButton != NULL)
		HideWidgetEdit(m_themeButton);
	if (m_addButton != NULL)
		HideWidgetEdit(m_addButton);
	HideWidget(m_slotFrames, new Timer(150.0f));
	if (m_themeButton != NULL)
		m_root->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				 new Timer(g_applauncherEditMoveTime, Timer::FUNC_QUAD_OUT),
				 ui::Widget::ANIM_COLOR, 2);
	if (m_isFolder)
		ScePafWidget_21125870(m_root, 64);
	SetEmpty(false);
	if (m_name != NULL) {
		m_name->SetEditMode(false);
		m_name->SetObjAnim(1, APPLAUNCHER_PAGE_VIEW_OBJ_COLOR,
				   math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				   new Timer(300.0f, Timer::FUNC_QUAD_OUT), 2);
		m_name->SetPos(math::v4(0.0f, APPLAUNCHER_PAGE_VIEW_NAME_Y, 0.0f, 0.0f),
			       new Timer(g_applauncherEditMoveTime, Timer::FUNC_QUAD_OUT),
			       ui::Widget::ANIM_MOVE, 2);
		HoldName(this);
	}
}

/* the edit's buttons shown, the delete button on an empty page, the frames faded in and the
   root's color with them; a folder's name made editable and dropped into its edit place
   (0x810C5688) */
void ApplauncherPageView::BeginEdit(bool empty)
{
	if (m_themeButton != NULL)
		m_root->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
				 new Timer(g_applauncherEditMoveTime, Timer::FUNC_QUAD_OUT),
				 ui::Widget::ANIM_COLOR, 2);
	if (m_isFolder)
		ScePafWidget_164BFFA5(m_root, 64);
	SetEmpty(empty);
	if (m_themeButton != NULL)
		ShowWidgetEdit(m_themeButton);
	ShowWidget(m_slotFrames, new Timer(150.0f));
	if (m_name != NULL) {
		m_name->SetEditMode(true);
		m_name->SetObjAnim(1, APPLAUNCHER_PAGE_VIEW_OBJ_COLOR,
				   math::v4(1.0f, 1.0f, 1.0f, 1.0f),
				   new Timer(300.0f, Timer::FUNC_QUAD_OUT), 2);
		m_name->SetPos(math::v4(0.0f, APPLAUNCHER_PAGE_VIEW_NAME_EDIT_Y, 0.0f, 0.0f),
			       new Timer(g_applauncherEditMoveTime, Timer::FUNC_BOUNCE_OUT),
			       ui::Widget::ANIM_MOVE, 2);
		ReleaseName(this);
	}
}

/* the add button hidden at the most pages, shown below (0x810C5A0E) */
void ApplauncherPageView::Refresh()
{
	if (m_addButton == NULL)
		return;
	if (g_applauncherHome->m_scroller.m_items.size() >= APPLAUNCHER_PAGES_MAX)
		HideWidgetEdit(m_addButton);
	else
		ShowWidgetEdit(m_addButton);
}

/* (0x810C5C08) */
void ApplauncherPageView::ShowSlotFrame(int slot, bool animate)
{
	if (m_slotShown[slot])
		return;

	m_slotFrames->KillObjAnim(slot, APPLAUNCHER_PAGE_VIEW_OBJ_COLOR);

	ui::CompositeDrawer *frames = m_slotFrames;
	math::v4 color(1.0f, 1.0f, 1.0f, 1.0f);

	if (animate)
		frames->SetObjAnim(slot, APPLAUNCHER_PAGE_VIEW_OBJ_COLOR, color,
				   new Timer(150.0f, Timer::FUNC_QUART_OUT), 0);
	else
		frames->GetDrawObj(slot)->SetColor(color);
	m_slotShown[slot] = true;
}

/* (0x810C5D0C) */
void ApplauncherPageView::HideSlotFrame(int slot, bool animate)
{
	if (!m_slotShown[slot])
		return;

	m_slotFrames->KillObjAnim(slot, APPLAUNCHER_PAGE_VIEW_OBJ_COLOR);

	ui::CompositeDrawer *frames = m_slotFrames;

	if (animate)
		frames->SetObjAnim(slot, APPLAUNCHER_PAGE_VIEW_OBJ_COLOR,
				   math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				   new Timer(150.0f, Timer::FUNC_QUART_OUT), 0);
	else
		frames->GetDrawObj(slot)->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	m_slotShown[slot] = false;
}

/* a page's root held: the applauncher's request kept (0x810C5E1C) */
static void onPageLongPress(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	applauncherPagesHoldRequest(((ApplauncherPageView *)data)->m_page);
}

/* the edit's buttons, the page's (0x810C5E26, 0x810C5E32, 0x810C5E3E) */
static void onDelete(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	((ApplauncherPageView *)data)->m_page->reserved4();
}

static void onTheme(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	((ApplauncherPageView *)data)->m_page->reserved5();
}

static void onAdd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	((ApplauncherPageView *)data)->m_page->reserved6();
}

/* a folder's name's edit begun: the name kept, the page's show held, its focus taken in and
   let go of where it is in the view (0x810C5E4A) */
static void onNameEditBegin(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherPageView *view = (ApplauncherPageView *)data;
	ui::Widget *text = (ui::Widget *)self;

	text->GetString(s_nameBefore);
	if (text->m_parent != NULL && text->m_parent->m_parent != NULL)
		ScePafWidget_A96A0B4A(text->m_parent->m_parent, 1);

	ui::Widget *widget = view->m_widget;

	if (widget == NULL)
		return;
	widget->m_composite_focus = true;
	ui::Widget *focus =
		Framework::Instance()->GetEnvironmentInstance()->m_focus_cursor.m_target;

	if (focus != NULL && widget->FindChild(focus, 0) != NULL)
		focus->ReleaseFocus();
}

/* a folder's name's edit ended: the page's show let go, the name's focus dropped, and a
   changed name given to the folder (0x810C5EAA) */
static void onNameEditEnd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherPageView *view = (ApplauncherPageView *)data;
	ui::Widget *text = (ui::Widget *)self;
	wstring name;

	text->GetString(name);
	if (text->m_parent != NULL && text->m_parent->m_parent != NULL)
		ScePafWidget_A96A0B4A(text->m_parent->m_parent, -1);
	if (view->m_widget != NULL)
		view->m_widget->m_composite_focus = false;
	if (view->m_name != NULL)
		view->m_name->SetFocus(NULL);
	if (name == s_nameBefore)
		return;

	ApplauncherPage *page = view->m_page;

	if (page == NULL || page->m_type != APPLAUNCHER_PAGE_TYPE_FOLDER)
		return;
	applauncherFolderRename(applauncherPageAsFolder(page)->m_icon.get(), name);
	text->SetString(name);
}

/* (0x810C5FA4) */
static void onNameEnter(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	((ui::TextBox *)self)->EndEdit();
}

/* whether the touch is inside the root's circle, its radius at the camera's distance from the
   root (0x810C5FAE) */
static __attribute__((noinline)) bool applauncherPageViewHit(ui::Widget *widget, ui::Event *e)
{
	ui::Camera *camera = widget->m_scene->m_camera;
	float radius = APPLAUNCHER_PAGE_VIEW_HIT_RADIUS;

	ScePafWidget_72F479CA(camera);

	float distance = camera->m_distance;
	math::v4 pos = widget->GetWorldMatrix().w();
	float scale = (distance + pos.extract_z()) / distance;
	float r = radius * scale;

	return r * r >= (float)(e->m_data[0] * e->m_data[0]) + (float)(e->m_data[1] * e->m_data[1]);
}

/* the folder's page decided off its icons: the page closed (0x810C6036) */
static void onFolderDecide(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	if (!applauncherPageViewHit((ui::Widget *)self, e))
		applauncherHomeCloseAppPage(true);
}

/* the folder's page held on its icons: the applauncher's request kept (0x810C604C) */
static void onFolderLongPress(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	if (applauncherPageViewHit((ui::Widget *)self, e))
		applauncherPagesHoldRequest(((ApplauncherPageView *)data)->m_page);
}

/* a button of the edit touched in the edit: the edit's state 5 asked for (0x810C6064) */
static void onEditTouch(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	if (g_applauncherModeManager->m_currentMode == APPLAUNCHER_MODE_EDIT)
		g_applauncherHome->RequestEditState(5);
}

/* the folder's page's key: a long press on the home in view, outside the IDU and the edit, the
   edit asked for (0x810C6088) */
static void onFolderKey(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	int detail = (SceUInt8)e->GetDetail();

	if (shellStartupGetIduMode() == 1)
		return;
	if (e->GetType() == 17 && detail == 29 &&
	    g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] == NULL &&
	    ((bool (*)(int))g_applauncherHome->m_topmenu->func[TOPMENU_PLUGIN_FUNC_IS_LAYER_ACTIVE])
		    (g_applauncherHome->m_layer))
		g_applauncherModeManager->ChangeMode(APPLAUNCHER_MODE_EDIT);
}
