/*
 * paf::ui::ScrollView (DEBUGGER 0x81115668..0x81116228): a scroll view whose view box is fitted
 * to its children and whose snap points are its children's centres. No record or string names
 * the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <std/map>
#include <std/vector>
#include <versatileparam/resourceparam.h>
#include <widget/core/event.h>
#include <widget/factory.h>
#include <widget/w_scroll_view_base.h>

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::Vector2;

/* the view box's corners, y up: its left bottom and its right top (names ours) */
static inline Vector2 ViewBoxMin(const math::v4 &box)
{
	return Vector2(box.extract_x(), box.extract_w());
}

static inline Vector2 ViewBoxMax(const math::v4 &box)
{
	return Vector2(box.extract_z(), box.extract_y());
}

/* the child's centre in its parent */
static inline math::v2 ChildCenter(Widget *child)
{
	math::v4 center = child->GetPosCenter(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	return math::v2(center.getImpl().getXY());
}

const char ScrollView::m_widget_type[] = "scroll_view";

ScrollView::ScrollView(Widget *parent, CreateParam *param)
	: ScrollViewBase(parent, param), m_is_auto_snap(false), m_is_auto_bound(true),
	  m_min_view_size(0.0f, 0.0f), m_max_view_size(3.4028236e18f, 3.4028236e18f),
	  m_manual_view_box(0.0f, 0.0f, 0.0f, 0.0f)
{
}

ScrollViewParam::ScrollViewParam() : m_auto_snap(0)
{
}

bool ScrollView::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ScrollViewBase::IsInherit(type);
}

char *ScrollView::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ScrollView::TypeName()
{
	return m_widget_type;
}

Widget *ScrollView::GetViewParent()
{
	return GetInternalWidget();
}

void ScrollView::Clear()
{
	for (list<Widget *>::iterator it = m_view_parent->m_children.begin();
	     it != m_view_parent->m_children.end(); it++)
		(*it)->DestroyWidget();
}

void ScrollView::DelSnapChild(Widget *child)
{
	map<Widget *, int32_t>::iterator it = m_snap_map.find(child);

	if (it != m_snap_map.end()) {
		DelSnapPoint(it->second);
		m_snap_map.erase(it);
	}
}

void ScrollView::AddSnapChild(Widget *child)
{
	if (child->m_parent != m_view_parent)
		return;
	if (m_snap_map.find(child) != m_snap_map.end())
		return;

	int32_t id = AddSnapPoint(ScrollViewSnapPoint(ChildCenter(child)));

	m_snap_map[child] = id;
}

void ScrollView::SetAutoSnap(bool auto_snap)
{
	if (m_is_auto_snap == auto_snap)
		return;
	if (auto_snap) {
		for (list<Widget *>::iterator it = m_view_parent->m_children.begin();
		     it != m_view_parent->m_children.end(); it++)
			AddSnapChild(*it);
	} else {
		for (list<Widget *>::iterator it = m_view_parent->m_children.begin();
		     it != m_view_parent->m_children.end(); it++)
			DelSnapChild(*it);
	}
	m_is_auto_snap = auto_snap;
}

int32_t ScrollView::GetSnapID(Widget *child)
{
	map<Widget *, int32_t>::iterator it = m_snap_map.find(child);

	if (it == m_snap_map.end())
		return -1;
	return it->second;
}

void ScrollView::SetAutoBound(bool auto_bound)
{
	if (m_is_auto_bound != auto_bound) {
		m_is_auto_bound = auto_bound;
		UpdateViewBox();
	}
}

void ScrollView::AddIgnoredChild(Widget *child)
{
	if (child->m_parent != m_view_parent)
		return;

	vector<Widget *>::iterator it;

	for (it = m_ignore_list.begin(); it != m_ignore_list.end(); ++it) {
		if (*it == child)
			break;
	}
	if (it == m_ignore_list.end())
		m_ignore_list.push_back(child);
}

void ScrollView::DelIgnoredChild(Widget *child)
{
	vector<Widget *>::iterator it;

	for (it = m_ignore_list.begin(); it != m_ignore_list.end(); ++it) {
		if (*it == child)
			break;
	}
	if (it != m_ignore_list.end())
		m_ignore_list.erase(it);
}

void ScrollView::SetViewPadding(const math::v4 &padding)
{
	SetPadding(padding);
}

__attribute__((noinline)) void ScrollView::SetMinViewSize(const math::v2 &size)
{
	if (!(m_min_view_size == size)) {
		m_min_view_size = size;
		UpdateViewBox();
	}
}

__attribute__((noinline)) void ScrollView::SetMaxViewSize(const math::v2 &size)
{
	if (!(m_max_view_size == size)) {
		m_max_view_size = size;
		UpdateViewBox();
	}
}

/* the box kept with its right not left of its left and its top not below its bottom */
__attribute__((noinline)) void ScrollView::SetManualViewBox(const math::v4 &box)
{
	if (!(box == m_manual_view_box)) {
		math::v4 low(box.extract_x(), box.extract_w(), box.extract_x(), box.extract_w());

		m_manual_view_box = math::v4(
			sce::Vectormath::Simd::Aos::maxPerElem(low.getImpl(), box.getImpl()));
		UpdateViewBox();
	}
}

void ScrollView::OnViewLayoutUpdated(bool is_dirty)
{
	if (is_dirty) {
		UpdateViewBox();
		UpdateAutoSnapPoint();
	}
	ScrollViewBase::OnViewLayoutUpdated(is_dirty);
}

/* the view box fitted again on gesture 0x16 of the front touch port */
int32_t ScrollView::HandleGestureEvent(Event *e)
{
	if (e != NULL && ((e->m_detail >> 20) & 0xf) == 0 &&
	    ((e->m_detail >> 8) & 0x7f) == 0x16)
		UpdateViewBox();
	return ScrollViewBase::HandleGestureEvent(e);
}

ScrollView::~ScrollView()
{
}

void ScrollView::OnChildRegistered(Widget *child)
{
	child->m_layout_notify_mask |= 0x300;
	if (m_is_auto_snap && !child->m_parent_protected)
		AddSnapChild(child);
}

void ScrollView::OnChildRemoved(Widget *child)
{
	DelSnapChild(child);
	DelIgnoredChild(child);
}

/*
 * The box holding its children (or the manual box, where it has one) grown to its smallest size
 * and shrunk to its largest, about its centre.
 */
void ScrollView::UpdateViewBox()
{
	Vector2 box_min = ViewBoxMin(m_manual_view_box);
	Vector2 box_max = ViewBoxMax(m_manual_view_box);
	bool manual = !(math::v2(box_max - box_min) == math::v2(0.0f, 0.0f));
	Vector2 hi = manual ? box_max : Vector2(-3.4028236e18f);
	Vector2 lo = manual ? box_min : Vector2(3.4028236e18f);
	bool found = manual;

	if (m_is_auto_bound) {
		for (list<Widget *>::iterator it = m_view_parent->m_children.begin();
		     it != m_view_parent->m_children.end(); it++) {
			Widget *child = *it;

			if (child->m_hold_count <= 0 && !IsIgnored(child) &&
			    !child->m_parent_protected) {
				Vector2 center = ChildCenter(child).getImpl();
				Vector2 half = child->GetSize(0)->getImpl().getXY() * 0.5f;

				found = true;
				lo = sce::Vectormath::Simd::Aos::minPerElem(center - half, lo);
				hi = sce::Vectormath::Simd::Aos::maxPerElem(center + half, hi);
			}
		}
	}
	if (!found) {
		hi = Vector2(0.0f);
		lo = Vector2(0.0f);
	}

	Vector2 grow = sce::Vectormath::Simd::Aos::maxPerElem(
			       Vector2(0.0f), m_min_view_size.getImpl() - (hi - lo)) * 0.5f;

	lo = lo - grow;
	hi = hi + grow;

	Vector2 shrink = sce::Vectormath::Simd::Aos::maxPerElem(
				 Vector2(0.0f), (hi - lo) - m_max_view_size.getImpl()) * 0.5f;

	lo = lo + shrink;
	hi = hi - shrink;
	SetViewBox(math::v4(lo.getX(), hi.getY(), hi.getX(), lo.getY()));
}

void ScrollView::UpdateAutoSnapPoint()
{
	for (map<Widget *, int32_t>::iterator it = m_snap_map.begin(); it != m_snap_map.end();
	     ++it) {
		Widget *child = it->first;
		ScrollViewSnapPoint snap_point = GetSnapPoint(it->second);
		math::v2 center = ChildCenter(child);

		if (!(snap_point.m_pos == center))
			UpdateSnapPoint(it->second, ScrollViewSnapPoint(center));
	}
}

bool ScrollView::IsIgnored(Widget *child)
{
	vector<Widget *>::iterator it;

	for (it = m_ignore_list.begin(); it != m_ignore_list.end(); ++it) {
		if (*it == child)
			break;
	}
	return it != m_ignore_list.end();
}

const char ScrollViewParam::m_param_name[] = "scroll_view_base";

const char *ScrollViewParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollViewParam::ParamName()
{
	return m_param_name;
}

int32_t ScrollViewParam::Create(int32_t mode)
{
	int32_t ret = ScrollViewBaseParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("auto_snap", &m_auto_snap, ResourceParam::SEMANTIC_INT);
	return 0;
}

int32_t ScrollViewParam::Set()
{
	int32_t ret = ScrollViewBaseParam::Set();

	if (ret < 0)
		return ret;
	((ScrollView *)m_widget)->SetAutoSnap(m_auto_snap == 1);
	return 0;
}

int32_t ScrollViewParam::Get()
{
	int32_t ret = ScrollViewBaseParam::Get();

	if (ret < 0)
		return ret;
	m_auto_snap = ((ScrollView *)m_widget)->m_is_auto_snap ? 1 : 0;
	return 0;
}

const char ScrollViewStyleParam::m_param_name[] = "style_scroll_view";

const char *ScrollViewStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ScrollViewStyleParam::ParamName()
{
	return m_param_name;
}

int32_t ScrollViewStyleParam::Create(int32_t mode)
{
	int32_t ret = ScrollViewBaseStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("min_view_size", &m_min_view_size, ResourceParam::SEMANTIC_VEC2);
		if (m_serial != NULL) {
			m_serial->AddParam("max_view_size", &m_max_view_size,
					   ResourceParam::SEMANTIC_VEC2);
			if (m_serial != NULL) {
				m_serial->AddParam("manual_view_box", &m_manual_view_box,
						   ResourceParam::SEMANTIC_VEC4);
				if (m_serial != NULL)
					m_serial->AddParam("auto_bound", &m_auto_bound,
							   ResourceParam::SEMANTIC_INT);
			}
		}
	}
	return 0;
}

int32_t ScrollViewStyleParam::SetEach(Widget *widget)
{
	int32_t ret = ScrollViewBaseStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;

	ScrollView *view = (ScrollView *)widget;

	view->SetMinViewSize(m_min_view_size);
	view->SetMaxViewSize(m_max_view_size);
	view->SetManualViewBox(m_manual_view_box);
	view->SetAutoBound(m_auto_bound == 1);
	return 0;
}

int32_t ScrollViewStyleParam::GetEach(Widget *widget)
{
	int32_t ret = ScrollViewBaseStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;

	ScrollView *view = (ScrollView *)widget;

	m_min_view_size = view->m_min_view_size;
	m_max_view_size = view->m_max_view_size;
	m_manual_view_box = view->m_manual_view_box;
	m_auto_bound = view->m_is_auto_bound ? 1 : 0;
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewScrollView(Widget *parent)
{
	return new ScrollView(parent, NULL);
}

static WidgetParam *NewScrollViewParam()
{
	return new ScrollViewParam;
}

static VersatileParam *NewScrollViewLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewScrollViewFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewScrollViewStyleParam()
{
	return new ScrollViewStyleParam;
}

static VersatileParam *NewScrollViewHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewScrollViewSoundParam()
{
	return new ScrollViewBaseSoundParam;
}

FactoryFuncTable g_factoryScrollView = {
	ScrollView::TypeName(),
	NewScrollView,
	NewScrollViewParam,
	NewScrollViewLayoutParam,
	NewScrollViewFocusParam,
	NewScrollViewStyleParam,
	NewScrollViewHandlerParam,
	NewScrollViewSoundParam,
};
