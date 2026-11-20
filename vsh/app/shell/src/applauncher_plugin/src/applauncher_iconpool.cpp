/*
 * applauncher_iconpool.cpp: the icon pool, the views (widgets) the home's icons are shown with
 * while they are on a page in view (0x810BB748-0x810BD35C). The file's name and the names here
 * are ours (PLACEHOLDER): no record of the 3.6x shell names them.
 */
#include <stddef.h>
#include <string.h>
#include <arm_neon.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <math/ngp/v4.h>
#include <system/system.h>
#include <versatileparam/idparam.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/drawobj/bubble_obj.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/environment.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_appicon2d.h>
#include <widget/w_scene.h>
#include <common/shared_ptr.h>
#include "../../../shell.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

using namespace paf;

/* the views the pool makes */
#define APPLAUNCHER_ICON_POOL_SIZE	42

/* the time a view moves and shrinks in, and its size shrunk to */
#define APPLAUNCHER_ICON_MOVE_TIME	250.0f
#define APPLAUNCHER_ICON_FREE_SCALE	0.1f

/* PLACEHOLDER: the bubble's light while the views are dimmed and not, and its range */
#define APPLAUNCHER_ICON_DIM_LIGHT	2.0f
#define APPLAUNCHER_ICON_LIGHT		1.0f
#define APPLAUNCHER_ICON_LIGHT_RANGE	200.0f

/* PLACEHOLDER: the textures of the views (the cursor, the icon's frame, the edit's mark) */
#define APPLAUNCHER_ICON_FOCUS_TEXTURE	0xCF115394
#define APPLAUNCHER_ICON_FRAME_TEXTURE	0xAFDEF9D0
#define APPLAUNCHER_ICON_EDIT_TEXTURE	0xB86FDFD9

/* PLACEHOLDER: the icon types (LsdbIcon icon0Type) a view draws its frame for */
#define APPLAUNCHER_ICON_FRAME_NONE	5
#define APPLAUNCHER_ICON_FRAME_WIDE	7

/* PLACEHOLDER: the text styles of the label: its shadow, and its color */
#define APPLAUNCHER_LABEL_STYLE_SHADOW	((graph::TextStyleAttribute)12)
#define APPLAUNCHER_LABEL_STYLE_COLOR	((graph::TextStyleAttribute)2)

/* PLACEHOLDER: the page's label color word's shadow bit (LsdbPage reserved01) */
#define APPLAUNCHER_PAGE_LABEL_SHADOW	0x1000000

/* PLACEHOLDER: the views free (0x81555378), those made (0x81555388), and the names given
   (0x81544284) */
vector<ApplauncherIconView *> g_applauncherIconPoolFree((allocator<ApplauncherIconView *>()));
ApplauncherIconView *g_applauncherIconViews;
int g_applauncherIconViewCount;

/* PLACEHOLDER: the views dimmed (0x8155538C) */
static bool s_dimmed;

/* PLACEHOLDER: the time a view is held for before its hold event, 1000 ms (0x81507DE8) */
float g_applauncherIconHoldTime = 1000.0f;

static void onUpdate(ui::AppIconBase *icon, void *data);
static void onIcon(int32_t mode, int32_t prev, void *data);
static void onPressed(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onFocus(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onHold(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onHoldEnd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onDrag(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void onFreed(Timer *timer, ui::AnimListener *listener, void *data1, void *data2,
		    void *data3);

/* (0x810BB748) */
ApplauncherIconView::ApplauncherIconView()
	: m_widget(NULL), m_owner()
{
	m_widget = new ui::AppIcon2d(g_applauncherHome->m_foreground, NULL);
	if (m_widget == NULL)
		return;

	m_widget->m_hold_time = g_applauncherIconHoldTime;
	m_widget->SetIconSize(math::v4(138.0f, 138.0f, 0.0f, 0.0f), NULL,
			      ui::Widget::ANIM_SIZE, 0);
	m_widget->SetUpdateCB(onUpdate, this);
	m_widget->SetIconCB(onIcon, this);
	m_widget->AddEventCallback(0x10000001, onPressed, this);
	m_widget->AddEventCallback(0x10000002, (ui::HandlerCB)applauncherPagesHoldRequest, this);
	m_widget->AddEventCallback(0x10001, onFocus, this);
	m_widget->AddEventCallback(0x10003, onFocus, this);
	m_widget->AddEventCallback(0x40001, onHold, this);
	m_widget->AddEventCallback(0x40002, onHoldEnd, this);
	m_widget->AddEventCallback(0x2000D, onDrag, this);
	m_widget->AddEventCallback(0x2000E, onDrag, this);
	m_widget->AddEventCallback(0x2000F, onDrag, this);

	ui::AppIconBase *widget = m_widget;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);

	const math::v4 *size = m_widget->GetSize(1);
	ui::FocusShape shape;

	shape.m_shape = 4;
	shape.m_size = math::v2(size->extract_x() + 69.0f, size->extract_y() + 69.0f);
	shape.m_texture =
		g_applauncherPlugin->GetTexture(IDParam(APPLAUNCHER_ICON_FOCUS_TEXTURE));
	ScePafWidget_13B246E0(m_widget, &shape);
}

/* (0x810BBBC8) */
void applauncherIconPoolCreate(void)
{
	unsigned int i;

	g_applauncherIconViews = new ApplauncherIconView[APPLAUNCHER_ICON_POOL_SIZE];
	for (i = 0; i < APPLAUNCHER_ICON_POOL_SIZE; i++)
		g_applauncherIconPoolFree.push_back(&g_applauncherIconViews[i]);
}

/* (0x810BBC6E) */
ApplauncherIconView::~ApplauncherIconView()
{
	if (m_widget != NULL) {
		m_widget->DestroyWidget();
		m_widget = NULL;
	}
}

/* (0x810BBCD4) */
void applauncherIconPoolDestroy(void)
{
	delete[] g_applauncherIconViews;
	g_applauncherIconViews = NULL;
}

/* the cursor's size: the widget's, and its margin, smaller on a folder's page (0x810BBD22) */
static void updateFocusSize(ApplauncherIconView *view)
{
	ui::FocusShape *shape = view->m_widget->m_focus_shape;

	if (shape == NULL)
		return;

	float margin = 69.0f;

	if (view->m_owner.get() != NULL && view->m_owner->m_page != NULL) {
		if (view->m_owner->m_page->m_type == 2)
			margin = system::Model::GetType() == 1 ? 60.0f : 69.0f;
		else
			margin = system::Model::GetType() == 1 ? 62.0f : 69.0f;
	}

	const math::v4 *size = view->m_widget->GetSize(1);
	sce::Vectormath::Simd::floatInVec pad(margin);

	shape->m_size = math::v2(sce::Vectormath::Simd::Aos::Vector2(size->getImpl().getX() + pad,
								     size->getImpl().getY() + pad));
}

/* the cursor's size in the edit: the widget's and its margin, larger while the icon is held
   (0x810BBDB0) */
static void updateEditFocusSize(ApplauncherIconView *view)
{
	ui::FocusShape *shape = view->m_widget->m_focus_shape;

	if (shape == NULL)
		return;

	float scale = 1.0f;
	float margin = 76.0f;

	const math::v4 *size = view->m_widget->GetSize(1);

	shape->m_size = math::v2(size->extract_x(), size->extract_y());
	if (view->m_widget->m_icon_mode == 2)
		scale = 1.2f;
	if (view->m_owner.get() != NULL && view->m_owner->m_page != NULL) {
		if (view->m_owner->m_page->m_type == 2)
			margin = system::Model::GetType() == 1 ? 64.0f : 74.0f;
		else
			margin = system::Model::GetType() == 1 ? 66.0f : 76.0f;
	}
	sce::Vectormath::Simd::floatInVec pad(margin);
	const sce::Vectormath::Simd::Aos::Vector2 &edit = shape->m_size.getImpl();

	shape->m_size = math::v2(sce::Vectormath::Simd::Aos::Vector2((edit.getX() + pad) * scale,
								     (edit.getY() + pad) * scale));
}

/* (0x810BBE66) */
void applauncherIconWidgetUpdateMark(ApplauncherIconView *view)
{
	if (view->m_widget->m_focus_shape == NULL)
		return;
	if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0)
		updateEditFocusSize(view);
	else
		updateFocusSize(view);
}

/* (0x810BBE8C) */
float applauncherIconWidgetFolderSize(void)
{
	return system::Model::GetType() == 1 ? 108.0f : 124.0f;
}

/* (0x810BBEAE) */
float applauncherIconWidgetSize(void)
{
	return system::Model::GetType() == 1 ? 120.0f : 138.0f;
}

/* PLACEHOLDER name: a free view taken for the icon and set up for it, none when the pool is
   empty or the icon is hidden in the IDU mode (0x810BBED0) */
ApplauncherIconView *applauncherIconPoolAlloc(const common::SharedPtr<ApplauncherIcon> &icon)
{
	if (g_applauncherIconPoolFree.size() == 0)
		return NULL;
	if (shellStartupGetIduMode() == 1 && icon->m_hidden)
		return NULL;

	ApplauncherIconView *view = g_applauncherIconPoolFree.back();

	if (view == NULL)
		return view;

	graph::TextObj *label = (graph::TextObj *)view->m_widget->GetDrawObj(2);

	g_applauncherIconPoolFree.pop_back();
	view->m_owner = icon;
	view->m_effect.Reset();
	if (icon->m_page->m_type != 2) {
		float size = system::Model::GetType() == 1 ? 120.0f : 138.0f;

		view->m_widget->SetSize(math::v4(size, size, 0.0f, 0.0f), NULL,
					ui::Widget::ANIM_SIZE, 0);

		ApplauncherPage *page = icon->m_page;

		if (page->m_type == 0) {
			LsdbPage param;

			page->GetParam(&param);
			label->SetStyleAttribute(APPLAUNCHER_LABEL_STYLE_SHADOW, 0, 0,
						 (param.reserved01 & APPLAUNCHER_PAGE_LABEL_SHADOW) != 0);

			uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(
				vdup_n_u32(param.reserved01 | 0xFF000000)));
			math::v4 color(sce::Vectormath::Simd::Aos::Vector4(
				vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
					  vdupq_n_f32(1.0f / 255.0f))));

			label->SetStyleAttribute(APPLAUNCHER_LABEL_STYLE_COLOR, 0, 0, color);
		}
	} else {
		float size = system::Model::GetType() == 1 ? 108.0f : 124.0f;

		view->m_widget->SetSize(math::v4(size, size, 0.0f, 0.0f), NULL,
					ui::Widget::ANIM_SIZE, 0);
		label->SetStyleAttribute(APPLAUNCHER_LABEL_STYLE_SHADOW, 0, 0, true);
		label->SetStyleAttribute(APPLAUNCHER_LABEL_STYLE_COLOR, 0, 0,
					 math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	if (view->m_widget->m_focus_shape != NULL) {
		if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0)
			updateEditFocusSize(view);
		else
			updateFocusSize(view);
	}

	char name[32];

	g_applauncherIconViewCount++;
	sce_paf_snprintf(name, sizeof(name), "%d", g_applauncherIconViewCount);
	view->m_widget->m_name.SetID(string(name));
	view->m_widget->SetParent(icon->m_page->GetRoot(), NULL, 0);
	view->m_widget->SetPos(icon->GetSlotPos(), NULL, ui::Widget::ANIM_MOVE, 0);
	view->m_widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f), NULL, ui::Widget::ANIM_SCALE, 0);
	view->m_widget->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), NULL, ui::Widget::ANIM_COLOR, 0);
	view->m_widget->SetTexture(
		g_applauncherPlugin->GetTexture(IDParam(APPLAUNCHER_ICON_FRAME_TEXTURE)), 0, 0);
	view->m_widget->SetDrawObjMaskBit(&icon->m_image, 0, 0);
	view->m_widget->SetLabelString(icon->m_label);

	ui::AppIconBase *widget = view->m_widget;

	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);

	int type = icon->m_data->icon0Type;

	if (type == APPLAUNCHER_ICON_FRAME_NONE) {
		ScePafGraphics_5E94DC78((graph::BubbleObj *)view->m_widget->GetDrawObj(0), 0);
		((graph::BubbleObj *)view->m_widget->GetDrawObj(0))->m_opaque = false;
	} else if (type == APPLAUNCHER_ICON_FRAME_WIDE) {
		ScePafGraphics_5E94DC78((graph::BubbleObj *)view->m_widget->GetDrawObj(0), 2);
		((graph::BubbleObj *)view->m_widget->GetDrawObj(0))->m_opaque = true;
	} else {
		ScePafGraphics_5E94DC78((graph::BubbleObj *)view->m_widget->GetDrawObj(0), 1);
		((graph::BubbleObj *)view->m_widget->GetDrawObj(0))->m_opaque = false;
	}
	if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0) {
		view->m_widget->SetIconMode(1);
		if (icon->m_data->icon0Type != APPLAUNCHER_ICON_FRAME_WIDE) {
			view->m_widget->SetInfoTexture(g_applauncherPlugin->GetTexture(
				IDParam(APPLAUNCHER_ICON_EDIT_TEXTURE)));
			view->m_widget->m_show_info = true;
		}
		view->m_effect.Begin(view);
	}
	return view;
}

/* PLACEHOLDER name: the view given back to the pool: hidden and taken off its page at once, or
   shrunk away and hidden first (0x810BC560) */
void applauncherIconPoolFree(ApplauncherIconView *view, bool animate)
{
	if (g_applauncherIconViews == NULL || view == NULL)
		return;

	view->m_owner.reset();
	if (!animate) {
		ui::AppIconBase *widget = view->m_widget;

		if (widget != NULL) {
			widget->m_transition_complete = false;
			widget->m_transition = 0xff;
			widget->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
			view->m_widget->SetIconMode(0);
			view->m_widget->m_show_info = false;
			view->m_widget->SetParent(g_applauncherHome->m_foreground, NULL, 0);

			{
				intrusive_ptr<ui::TextureSource> none;

				view->m_widget->SetDrawObjMaskBit(&none, 0, 0);
			}
			static_cast<ui::Widget *>(view->m_widget)->SetTexture(intrusive_ptr<graph::Surface>());
			view->m_widget->GetDrawObj(0)->SetVisible(true);
		}
		g_applauncherIconPoolFree.push_back(view);
		return;
	}
	if (view->m_widget != NULL) {
		view->m_widget->KillAnim(ui::Widget::ANIM_SCALE);
		view->m_widget->SetScale(math::v4(APPLAUNCHER_ICON_FREE_SCALE,
						  APPLAUNCHER_ICON_FREE_SCALE,
						  APPLAUNCHER_ICON_FREE_SCALE, 0.0f),
					 new Timer(APPLAUNCHER_ICON_MOVE_TIME, Timer::FUNC_QUAD_OUT),
					 ui::Widget::ANIM_SCALE, 0, 0.0f, onFreed, view);

		ui::AppIconBase *widget = view->m_widget;
		Timer *timer = new Timer(APPLAUNCHER_ICON_MOVE_TIME, Timer::FUNC_QUAD_OUT);
		float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

		widget->m_transition_complete = false;
		widget->m_transition = 0xff;
		widget->DoTransitionReverse_core(time, true);
		ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
	} else {
		g_applauncherIconPoolFree.push_back(view);
	}
}

/* PLACEHOLDER name: the view put in the edit: its mark shown (an application's), moved to its
   slot, its pose taken when animated (0x810BC8D0) */
void applauncherIconWidgetBeginEdit(ApplauncherIconView *view, bool animate)
{
	view->m_widget->SetIconMode(1);
	view->m_widget->SetInfoTexture(
		g_applauncherPlugin->GetTexture(IDParam(APPLAUNCHER_ICON_EDIT_TEXTURE)));
	if (!view->m_owner->IsFolder())
		view->m_widget->m_show_info = true;
	view->m_widget->SetPos(view->m_owner->GetSlotPos(),
			       new Timer(g_applauncherEditMoveTime, Timer::FUNC_QUAD_OUT),
			       ui::Widget::ANIM_MOVE, 2);
	if (animate)
		view->m_effect.Capture(view);
	updateEditFocusSize(view);
}

/* PLACEHOLDER name: the view out of the edit (0x810BCA36) */
void applauncherIconWidgetEndEdit(ApplauncherIconView *view)
{
	view->m_widget->SetIconMode(0);
	view->m_widget->m_show_info = false;
	updateFocusSize(view);
}

/* PLACEHOLDER name: (0x810BCA56) */
void applauncherIconWidgetSetLabel(ApplauncherIconView *view, const wstring &label)
{
	view->m_widget->SetLabelString(label);
}

/* PLACEHOLDER name: (0x810BCA64) */
void applauncherIconWidgetDropIn(ApplauncherIconView *view)
{
	view->m_effect.DropIn(view);
}

/* PLACEHOLDER name: the icon's image placed in its widget (0x810BCA72) */
void applauncherIconWidgetSetIconPos(ApplauncherIconView *view, const math::v3 &pos)
{
	math::v3 at = pos;

	view->m_widget->GetDrawObj(0)->SetPos(at);
}

/* PLACEHOLDER name: the view's drag begun: raised, its icon's drag begun and moved, the home
   told, its scene's drag taken (0x810BCABA) */
void applauncherIconWidgetBeginDrag(ApplauncherIconView *view)
{
	if (view->m_widget->m_icon_mode == 2)
		return;

	view->m_widget->SetIconMode(2);
	view->m_owner->UpdateDrag(0, view->m_widget, 0);
	view->m_owner->UpdateDrag(1, view->m_widget, 0);
	g_applauncherHome->RequestEditState(3);

	ui::Widget *scene = view->m_widget->m_scene;

	scene->m_drag_begun = true;
	scene->m_focus_direction_off = false;
	scene->m_focus_target = false;
	if (view->m_widget->m_focus_shape != NULL) {
		if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0)
			updateEditFocusSize(view);
		else
			updateFocusSize(view);
	}
}

/* (0x810BCB44) */
void applauncherIconWidgetLeaveDrag(ApplauncherIconView *view)
{
	if (view->m_widget->m_icon_mode != 2)
		return;
	view->m_widget->SetIconMode(1);
	view->m_widget->m_scene->m_drag_begun = false;
	if (view->m_owner.get() == NULL)
		return;
	if (view->m_owner->IsFolder())
		g_applauncherHome->RequestEditState(2);
	else
		g_applauncherHome->RequestEditState(1);
}

/* (0x810BCB94) */
void applauncherIconWidgetUpdateLabel(ApplauncherIconView *view)
{
	if (view->m_owner.get() == NULL)
		return;

	ApplauncherPage *page = view->m_owner->m_page;

	if (page->m_type != 0)
		return;

	LsdbPage param;
	graph::TextObj *label = (graph::TextObj *)view->m_widget->GetDrawObj(2);

	page->GetParam(&param);
	label->SetStyleAttribute(APPLAUNCHER_LABEL_STYLE_SHADOW, 0, 0,
				 (param.reserved01 & APPLAUNCHER_PAGE_LABEL_SHADOW) != 0);

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(param.reserved01 | 0xFF000000)));
	math::v4 color(sce::Vectormath::Simd::Aos::Vector4(
		vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))), vdupq_n_f32(1.0f / 255.0f))));

	label->SetStyleAttribute(APPLAUNCHER_LABEL_STYLE_COLOR, 0, 0, color);
}

/* (0x810BCC60) */
void applauncherIconPoolSetDim(bool dim)
{
	unsigned int i;

	if (s_dimmed == dim)
		return;

	s_dimmed = dim;
	i = 0;
	do {
		graph::BubbleObj *bubble =
			(graph::BubbleObj *)g_applauncherIconViews[i].m_widget->GetDrawObj(0);

		if (s_dimmed)
			ScePafGraphics_8454D7FB(bubble, true,
						math::v4(0.0f, 0.0f, 0.0f, APPLAUNCHER_ICON_DIM_LIGHT),
						APPLAUNCHER_ICON_LIGHT_RANGE);
		else
			ScePafGraphics_8454D7FB(bubble, false,
						math::v4(0.0f, 0.0f, 0.0f, APPLAUNCHER_ICON_LIGHT),
						APPLAUNCHER_ICON_LIGHT_RANGE);
		i++;
	} while (i < APPLAUNCHER_ICON_POOL_SIZE);
}

/* the widget pressed: the icon opened, in the edit only a folder (0x810BCD16) */
static void onPressed(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherIconView *view = (ApplauncherIconView *)data;

	if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] == 0 || (SceUInt8)e->GetDetail() != 27) {
		view->m_owner->OnPressed();
		return;
	}
	if (view->m_owner.get() != NULL && view->m_owner->IsFolder())
		view->m_owner->OnPressed();
}

/* the widget's drag events, given to the icon (0x810BCD46) */
static void onDrag(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherIconView *view = (ApplauncherIconView *)data;

	if (view->m_owner.get() != NULL)
		view->m_owner->OnDragEvent(e, view->m_widget);
}

/* the widget's update: the effect played while the icon has one (0x810BCD56) */
static void onUpdate(ui::AppIconBase *icon, void *data)
{
	ApplauncherIconView *view = (ApplauncherIconView *)data;

	if (view->m_owner.get() != NULL && view->m_owner->m_effectOn != 0)
		view->m_effect.Update(view);
}

/* the view shrunk away: hidden, taken off its page and given back to the pool (0x810BCD6A) */
static void onFreed(Timer *timer, ui::AnimListener *listener, void *data1, void *data2,
		    void *data3)
{
	ApplauncherIconView *view = (ApplauncherIconView *)data2;

	if (view->m_widget != NULL) {
		view->m_widget->SetIconMode(0);

		ui::AppIconBase *widget = view->m_widget;

		widget->m_transition_complete = false;
		widget->m_transition = 0xff;
		widget->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
		view->m_widget->m_show_info = false;
		view->m_widget->ReleaseFocus();
		view->m_widget->SetParent(g_applauncherHome->m_foreground, NULL, 0);

		{
			intrusive_ptr<ui::TextureSource> none;

			view->m_widget->SetDrawObjMaskBit(&none, 0, 0);
		}
		static_cast<ui::Widget *>(view->m_widget)->SetTexture(intrusive_ptr<graph::Surface>());
		view->m_widget->GetDrawObj(0)->SetVisible(true);
	}
	g_applauncherIconPoolFree.push_back(view);
}

/* the widget's focus and press events: in the edit, its drag begun, left and moved with the
   arrows; outside it, the icon opened (0x810BCEC8) */
static void onFocus(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherIconView *view = (ApplauncherIconView *)data;

	if (shellStartupGetIduMode() == 1)
		return;

	SceUInt32 kind = e->GetType();
	SceUInt8 key = (SceUInt8)e->GetDetail();
	bool shown = ScePafWidget_529FB877(Framework::Instance()->GetEnvironmentInstance(), -1);

	if (kind == 17) {
		if (key == 29) {
			if (shown) {
				if (g_applauncherModeManager->m_currentMode == 0) {
					if (((bool (*)(int))g_applauncherHome->m_topmenu
						     ->func[TOPMENU_PLUGIN_FUNC_IS_LAYER_ACTIVE])(
						    g_applauncherHome->m_layer) &&
					    g_applauncherModeManager->m_currentMode != -1)
						g_applauncherModeManager->m_nextMode = 1;
				} else if (view->m_widget->m_icon_mode != 2 &&
					   view->m_owner.get() != NULL && !view->m_owner->IsFolder()) {
					view->m_owner->OnPressed();
				}
			}
		} else if (key == 30) {
			if (g_applauncherModeManager->m_nextMode == 1 && shown) {
				if (view->m_widget->m_icon_mode != 2)
					applauncherIconWidgetBeginDrag(view);
				else
					applauncherIconWidgetLeaveDrag(view);
			}
		} else if (key == 27) {
			if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0)
				applauncherIconWidgetLeaveDrag(view);
		}
	}
	if (key == 1 || key == 2 || key == 3 || key == 4) {
		if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0 &&
		    view->m_widget->m_icon_mode == 2)
			view->m_owner->MoveDrag(key);
	}
}

/* the widget's icon mode changed: the scroll views over it take events first while it is not
   dragged; a drag ended (0x810BD070) */
static void onIcon(int32_t mode, int32_t prev, void *data)
{
	ApplauncherIconView *view = (ApplauncherIconView *)data;

	if (mode != 2 && prev != 2)
		return;

	ui::Widget *widget = g_applauncherHome->m_scroller.m_scrollView;

	while (widget != NULL) {
		bool scroll = widget->IsInherit(ui::ScrollViewBase::TypeName());
		ui::Widget *parent = widget->m_parent;

		if (scroll) {
			if (mode == 2)
				widget->m_pre_dispatch = false;
			else if (prev == 2)
				widget->m_pre_dispatch = true;
		}
		widget = parent;
	}
	if (prev != 2)
		return;
	if (view->m_owner.get() != NULL)
		view->m_owner->UpdateDrag(2, view->m_widget, 0);
	if (view->m_widget->m_focus_shape != NULL) {
		if (g_applauncherModeManager->m_modes[APPLAUNCHER_MODE_EDIT] != 0)
			updateEditFocusSize(view);
		else
			updateFocusSize(view);
	}
}

/* the widget held: the edit asked for, a folder's or an application's (0x810BD108) */
static void onHold(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherIconView *view = (ApplauncherIconView *)data;

	if (view->m_owner.get() == NULL || g_applauncherModeManager->m_currentMode != 1)
		return;
	if (view->m_owner->IsFolder())
		g_applauncherHome->RequestEditState(2);
	else
		g_applauncherHome->RequestEditState(1);
}

/* the widget's hold ended: raised again, its scene's drag let go, the edit asked for
   (0x810BD140) */
static void onHoldEnd(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	ApplauncherIconView *view = (ApplauncherIconView *)data;

	if (view->m_widget->m_icon_mode != 2)
		return;
	view->m_widget->SetIconMode(1);
	view->m_widget->m_scene->m_drag_begun = false;
	if (view->m_owner.get() == NULL)
		return;
	if (view->m_owner->IsFolder())
		g_applauncherHome->RequestEditState(2);
	else
		g_applauncherHome->RequestEditState(1);
}
