/*
 * paf::ui::ListItem (DEBUGGER 0x810f352c..0x810f4384): the widget of a list view's cell, and its
 * parameters. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <std/string.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/factory.h>
#include <widget/w_button_base.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_plane.h>
#include <widget/w_state_button_base.h>
#include <widget/w_text_box.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

ListItem::ListItem(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_cell(NULL), m_focus_requested(false)
{
	m_event_enable |= 0xFBF0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_hit_check = 1;
	m_not_hit_target = true;
	m_list_item = true;
	if (param == NULL || param->m_create_drawobj)
		AddDrawObj(DrawObjEntry(new graph::PlaneObj(graph::PlaneObjStyle()), OBJ_BG), false);
}

ListItemParam::ListItemParam()
{
}

ListItemStyleParam::ListItemStyleParam()
{
	m_bg_obj.m_name = "bg_obj";
	m_bg_obj.m_index = ListItem::OBJ_BG;
	m_drawobj_param_list.push_back(&m_bg_obj);
}

/* the sounds its sound parameter names, by index */
static const struct {
	const char *name;
	uint32_t index;
} s_list_item_sounds[] = {
	{ "bar", 1 },
};

ListItemSoundParam::ListItemSoundParam()
{
	for (uint32_t i = 0; i < 1; i++)
		m_sound_list.push_back(Sound(s_list_item_sounds[i].name, s_list_item_sounds[i].index));
}

const char ListItem::m_widget_type[] = "list_item";

bool ListItem::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ListItem::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ListItem::TypeName()
{
	return m_widget_type;
}

int32_t ListItem::GetSegmentIndex()
{
	if (m_cell == NULL)
		return -1;
	return m_cell->GetSegmentIndex();
}

int32_t ListItem::GetCellIndex()
{
	if (m_cell == NULL)
		return -1;
	return m_cell->m_index;
}

ListView *ListItem::GetListView()
{
	if (m_cell == NULL)
		return NULL;
	return m_cell->GetListView();
}

ListItem *ListItem::GetListItem(Widget *widget)
{
	if (widget == NULL)
		return NULL;
	for (Widget *parent = widget->m_parent; parent != NULL; parent = parent->m_parent) {
		if (parent->IsInherit(m_widget_type))
			return (ListItem *)parent;
	}
	return NULL;
}

int32_t ListItem::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
			     uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->SetTexture(surf, index);
	return 0;
}

int32_t ListItem::GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->GetTexture(surf, index);
	return 0;
}

uint32_t ListItem::CheckState()
{
	if (m_state >= 2 && m_state != 3)
		return m_state;
	if (IsPrepareObjs())
		return 3;
	return 0;
}

void ListItem::UpdateSize()
{
	Widget::UpdateSize();
}

void ListItem::OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
{
	if (m_touch_captures.size() == 0 && m_cell != NULL)
		m_cell->GetListView()->OnCellUnGrab(m_cell);
}

int32_t ListItem::HandleTouchEvent(Event *e)
{
	if (e->GetType() == 0x41 && m_hit_check != 0)
		ScePafWidget_2B1A1493(this, 1);
	return Widget::HandleTouchEvent(e);
}

int32_t ListItem::HandleGestureEvent(Event *e)
{
	if (((e->m_detail >> 8) & 0x7F) == 14 && m_cell != NULL) {
		m_cell->GetListView()->OnCellDragMove(m_cell);
		e->Consume();
	}
	return Widget::HandleGestureEvent(e);
}

ListItem::~ListItem()
{
}

bool ListItem::BeginDrag(int32_t index)
{
	if (index < 0) {
		HitInfo hit;

		if (m_env->GetHitStack(&hit, this) != 0)
			return false;
		index = hit.m_touch_id;
	}
	return ScePafWidget_D3D0A7C3(this, 0, index, 1, 1) == 0 &&
	       ScePafWidget_DE592034(this, 0, index, 0x110) == 0;
}

void ListItem::EndDrag(int32_t index)
{
	ScePafWidget_093F1C89(this, 0, index, 1);
	ScePafWidget_5ABB3393(this, 0, index, 0);
}

Widget *SearchListItemFocus(ListItem *item, math::v2 dir, Widget *from, int32_t reserved)
{
	if (item->m_env == NULL)
		return NULL;

	Widget *origin = from;

	if (origin == NULL) {
		origin = item->m_env->m_focus_cursor.m_target;
		if (origin == NULL || item->FindChild(origin, 0) == NULL)
			origin = GetRootFocus(item);
		if (!(dir.getImpl().getX() == sce::Vectormath::Simd::floatInVec(0.0f) &
		      dir.getImpl().getY() == sce::Vectormath::Simd::floatInVec(0.0f))
			     .getAsBool() &&
		    item->FindChild(origin, 0) == NULL)
			return NULL;
		if (origin == NULL)
			origin = item;
	}

	Widget::FocusSearchInfo info(255);

	info.m_dir = GetFocusDir(dir);
	ScePafWidget_EA117CF8(&info, origin);

	ListView *list_view = NULL;

	if (item->m_cell != NULL)
		list_view = item->m_cell->GetListView();
	if (list_view->FindChild(origin, 0) == NULL)
		info.m_search_outside = false;

	bool searching = false;

	if (item->m_focus_searching)
		searching = true;
	else
		item->m_focus_searching = true;

	Widget *found;

	if (from != NULL && item->FindChild(from, 0) == NULL)
		found = item->SetFocusOption(&info, 1);
	else
		found = origin->SetFocusOption(&info, 0);
	if (!searching)
		item->m_focus_searching = false;
	if (found == item)
		found = NULL;
	return found;
}

bool ListItem::ScePafWidget_C98D1926(const math::v2 &dir)
{
	Widget *focus = m_env->m_focus_cursor.m_target;

	if (focus == NULL)
		focus = GetRootFocus(this);
	if (focus != NULL && FindChild(focus, 0) != NULL && ScePafWidget_DB0FFD33(focus)) {
		m_focus_requested = true;
		return true;
	}
	m_focus_requested = true;

	Widget *found = SearchListItemFocus(this, dir, focus, 0);

	if (found == NULL)
		return false;
	if (focus == GetListView()) {
		found->SetFocus(NULL);
	} else {
		Widget::SetFocusInfo info;

		info.m_mode = 0;
		info.m_notify = false;
		info.m_direction = math::v2(0.0f, 0.0f);
		info.m_keep = false;
		found->SetFocus(&info);
	}
	return true;
}

void ListItem::ScePafWidget_1F9DB221()
{
	if (m_focus_requested)
		m_focus_requested = false;
}

static bool IsShownFocusTarget(Widget *widget);
static bool IsEnabledFocusTarget(Widget *widget);

Widget *FindListItemFocusChild(Widget *widget, int32_t mode)
{
	bool (*check)(Widget *) = IsEnabledFocusTarget;
	Widget *found;

	if (mode == 1)
		check = IsShownFocusTarget;
	else if (mode == 2)
		check = NULL;
	found = widget->FindChild(StateButtonBase::TypeName(), 0, check);
	if (found != NULL)
		return found;
	found = widget->FindChild(ButtonBase::TypeName(), 0, check);
	if (found != NULL)
		return found;
	found = widget->FindChild(TextBox::TypeName(), 0, IsEnabledFocusTarget);
	if (found != NULL)
		return found;
	found = widget->FindChild(Text::TypeName(), 0, IsEnabledFocusTarget);
	if (found != NULL)
		return found;
	found = widget->FindChild(Plane::TypeName(), 0, IsEnabledFocusTarget);
	if (found == NULL)
		return NULL;
	if (found->m_map_e_listener.find(0x20004) == found->m_map_e_listener.end())
		return NULL;
	return found;
}

const char *ListItemParam::GetParamName() const
{
	return "list_item_base";
}

int32_t ListItemParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret == 0 && m_serial != NULL)
		m_serial->AddParam("text_auto_scroll_target", &m_text_auto_scroll_target,
				   ResourceParam::SEMANTIC_WIDGETREF);
	return ret;
}

int32_t ListItemParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret == 0) {
		ListItem *item = (ListItem *)m_widget;

		if (IDParam().GetIDHash() != m_text_auto_scroll_target.GetIDHash())
			item->SetTextAutoScrollTarget(m_text_auto_scroll_target);
	}
	return ret;
}

int32_t ListItemParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret == 0) {
		ListItem *item = (ListItem *)m_widget;

		m_text_auto_scroll_target = IDParam(item->m_text_auto_scroll_target);
	}
	return ret;
}

const char ListItemStyleParam::m_param_name[] = "style_list_item";

const char *ListItemStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ListItemStyleParam::ParamName()
{
	return m_param_name;
}

const char ListItemSoundParam::m_param_name[] = "sound_list_item";

const char *ListItemSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *ListItemSoundParam::ParamName()
{
	return m_param_name;
}

/* shown, at least 95% opaque, taking input and focus events, and not being destroyed (name
   ours) */
static bool IsShownFocusTarget(Widget *widget)
{
	return widget->m_input_target && widget->m_show_a_sum >= 0.95f &&
	       !widget->m_transition_in && !widget->m_transition_out &&
	       !widget->m_destroy_protected && (widget->m_event_enable & 0x400) != 0;
}

/* a shown focus target whose text events are on and that is at least half visible (name
   ours) */
static bool IsEnabledFocusTarget(Widget *widget)
{
	if (!widget->IsTextEventEnabled())
		return false;
	if (widget->m_meta_a_sum < 0.5f)
		return false;
	return IsShownFocusTarget(widget);
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewListItem(Widget *parent)
{
	return new ListItem(parent, NULL);
}

static WidgetParam *NewListItemParam()
{
	return new ListItemParam;
}

static VersatileParam *NewListItemLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewListItemFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewListItemStyleParam()
{
	return new ListItemStyleParam;
}

static VersatileParam *NewListItemHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewListItemSoundParam()
{
	return new ListItemSoundParam;
}

FactoryFuncTable g_factoryListItem = {
	ListItem::TypeName(),
	NewListItem,
	NewListItemParam,
	NewListItemLayoutParam,
	NewListItemFocusParam,
	NewListItemStyleParam,
	NewListItemHandlerParam,
	NewListItemSoundParam,
};
