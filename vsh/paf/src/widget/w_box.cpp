/*
 * paf::ui::Box (DEBUGGER 0x810dbb74..0x810ddf4c): a plane that lays its children out, by hand
 * (each keeps its place, aligned as the box says), in a column from the top or in a row from the
 * left. It sizes children that adjust to it (CHILDREN_ADJUST_*: the widest child's size, a fixed
 * size, the room left, or the size of the box) over a few updates while their contents get
 * ready, and sizes itself to the boundary of its children, between its LayoutVars' min_size and
 * max_size. No record or string names the file; the names of the layouters and of the static
 * objects are ours.
 */
#include <graphics/drawobj/draw_obj.h>
#include <timer/timerqueue.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/animlistener.h>
#include <widget/factory.h>
#include <widget/w_box.h>
#include <std/string.h>
#include "../std/math.h"

namespace paf {
namespace ui {

/* the children keep their places; the box gives them its children's align */
class BoxManualLayouter : public BoxLayouter {
public:
	virtual void Layout()
	{
		Box *box = m_box;

		for (list<Widget *>::iterator it = m_widget->m_children.begin();
		     it != m_widget->m_children.end(); ++it) {
			Widget *w = *it;

			if (!Box::IsLayoutChild(w))
				continue;
			w->GetSize(0);

			uint32_t ax = w->m_align_x;
			uint32_t ay = w->m_align_y;
			uint32_t az = w->m_align_z;

			if (box->m_layout_vars.children_align_x != 3)
				ax = box->m_layout_vars.children_align_x;
			if (box->m_layout_vars.children_align_y != 3)
				ay = box->m_layout_vars.children_align_y;
			w->SetAlign(ax, ay, az, NULL);
		}
	}
};

/* the children one under the other, from the top (origin y 1), the middle or the bottom */
class BoxVerticalLayouter : public BoxLayouter {
public:
	virtual void Layout()
	{
		math::v4 info = m_box->GetChildrenInfo();
		Box *box = m_box;
		uint32_t ox, oy, oz;
		float y;

		m_widget->GetOrigin(ox, oy, oz);
		switch (oy) {
		case 1:
			y = info.extract_y();
			break;
		case 2:
			y = 0.0f;
			break;
		default:
			y = info.extract_y() * 0.5f;
			break;
		}
		for (list<Widget *>::iterator it = m_widget->m_children.begin();
		     it != m_widget->m_children.end(); ++it) {
			Widget *w = *it;

			if (!Box::IsLayoutChild(w))
				continue;

			math::v4 size = *w->GetSize(0);
			math::v4 pos = w->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			uint32_t anx, any, anz;

			w->GetAnchor(anx, any, anz);
			if (any != 4)
				w->SetAnchor(w->m_anchor_x, 4, w->m_anchor_z, NULL);

			uint32_t ax = w->m_align_x;
			uint32_t ay = w->m_align_y;
			uint32_t az = w->m_align_z;
			uint32_t align_x = ax;

			if (box->m_layout_vars.children_align_x != 3)
				align_x = box->m_layout_vars.children_align_x;
			if (ax != align_x || ay != 2)
				w->SetAlign(align_x, 2, az, NULL);
			pos.set_y(y);
			m_box->SetChildPos(w, pos);
			y = y - (size.extract_y() + box->m_layout_vars.space);
		}
	}
};

/* the children side by side, from the left (origin x 1), the middle or the right */
class BoxHorizontalLayouter : public BoxLayouter {
public:
	virtual void Layout()
	{
		math::v4 info = m_box->GetChildrenInfo();
		Box *box = m_box;
		uint32_t ox, oy, oz;
		float x;

		m_widget->GetOrigin(ox, oy, oz);
		switch (ox) {
		case 1:
			x = 0.0f;
			break;
		case 2:
			x = -info.extract_x();
			break;
		default:
			x = info.extract_x() * -0.5f;
			break;
		}
		for (list<Widget *>::iterator it = m_widget->m_children.begin();
		     it != m_widget->m_children.end(); ++it) {
			Widget *w = *it;

			if (!Box::IsLayoutChild(w))
				continue;

			math::v4 size = *w->GetSize(0);
			math::v4 pos = w->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			uint32_t anx, any, anz;

			w->GetAnchor(anx, any, anz);
			if (anx != 4)
				w->SetAnchor(4, w->m_anchor_y, w->m_anchor_z, NULL);

			uint32_t ax = w->m_align_x;
			uint32_t ay = w->m_align_y;
			uint32_t az = w->m_align_z;
			uint32_t align_y = ay;

			if (box->m_layout_vars.children_align_y != 3)
				align_y = box->m_layout_vars.children_align_y;
			if (ax != 1 || ay != align_y)
				w->SetAlign(1, align_y, az, NULL);
			pos.set_x(x);
			m_box->SetChildPos(w, pos);
			x = (size.extract_x() + box->m_layout_vars.space) + x;
		}
	}
};

void Box::SetLayoutType(LayoutType layout_type)
{
	if (m_layout_type == layout_type && m_layouter != NULL)
		return;

	Layouter *layouter;

	switch (layout_type) {
	case LAYOUT_VERTICAL:
		layouter = new BoxVerticalLayouter;
		break;
	case LAYOUT_HORIZONTAL:
		layouter = new BoxHorizontalLayouter;
		break;
	default:
		layouter = new BoxManualLayouter;
		break;
	}
	SetLayouter(layouter);
	m_layout_type = layout_type;
}

/* hidden until it first goes to state 0 */
Box::Box(Widget *parent, CreateParam *param)
	: Plane(parent, param), m_auto_show(true), m_cb_set_child_pos(NULL),
	  m_layout_size(0.0f, 0.0f, 0.0f, 0.0f), m_boundary_size(0.0f, 0.0f, 0.0f, 0.0f),
	  m_boundary_offs(0.0f, 0.0f, 0.0f, 0.0f), m_last_adjust_x(2), m_last_adjust_y(2),
	  m_children_adjust_state_x(CHILDREN_ADJUST_INIT),
	  m_children_adjust_state_y(CHILDREN_ADJUST_INIT),
	  m_children_adjust_x(CHILDREN_ADJUST_NONE), m_children_adjust_y(CHILDREN_ADJUST_NONE),
	  m_children_width(0.0f), m_children_height(0.0f), m_layout_vars(s_default_layout_vars)
{
	m_event_enable |= 0xFBF0;
	m_hit_check = 0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 2;
	m_hit_detect_b_mode = 2;
	SetOrigin(0, 0, 0, NULL);
	SetAdjust(2, 2, 2);
	SetLayoutType(LAYOUT_MANUAL);
	m_transition_complete = false;
	m_transition = 0xff;
	/* through the vtable, as Sony's */
	Widget *self = this;

	self->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(this, NULL, 2, 0.0f, NULL);
}

BoxParam::BoxParam() : m_exclusive_list(NULL)
{
}

/* the default LayoutVars, the adjust 2 and the origin 0 on every axis */
BoxLayoutParam::BoxLayoutParam()
	: m_layout_type(Box::LAYOUT_MANUAL), m_min_size(Box::s_default_layout_vars.min_size),
	  m_max_size(Box::s_default_layout_vars.max_size),
	  m_space_adjust(Box::s_default_layout_vars.space_adjust),
	  m_space(Box::s_default_layout_vars.space), m_top_margin(Box::s_default_layout_vars.top_margin),
	  m_bottom_margin(Box::s_default_layout_vars.bottom_margin),
	  m_left_margin(Box::s_default_layout_vars.left_margin),
	  m_right_margin(Box::s_default_layout_vars.right_margin),
	  m_children_align_x(Box::s_default_layout_vars.children_align_x),
	  m_children_align_y(Box::s_default_layout_vars.children_align_y),
	  m_children_adjust_x(Box::CHILDREN_ADJUST_NONE),
	  m_children_adjust_y(Box::CHILDREN_ADJUST_NONE), m_children_width(0.0f),
	  m_children_height(0.0f), m_boundary_mode_x(Box::s_default_layout_vars.boundary_mode_x),
	  m_boundary_mode_y(Box::s_default_layout_vars.boundary_mode_y)
{
	m_origin[1] = 0;
	m_origin[2] = 0;
	m_adjust[0] = 2;
	m_adjust[1] = 2;
	m_adjust[2] = 2;
	m_origin[0] = 0;
}

const char Box::m_widget_type[] = "box";

bool Box::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Plane::IsInherit(type);
}

char *Box::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *Box::TypeName()
{
	return m_widget_type;
}

Box::~Box()
{
}

bool Box::RemoveChild(Widget *obj)
{
	bool ret = Plane::RemoveChild(obj);

	if (ret) {
		if (m_children_adjust_x != CHILDREN_ADJUST_NONE)
			m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
		if (m_children_adjust_y != CHILDREN_ADJUST_NONE)
			m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
		ChangeState(3, -1);
	}
	return ret;
}

void Box::RegisterChild(Widget *obj, Widget *sibling)
{
	Plane::RegisterChild(obj, sibling);
	if (m_children_adjust_x != CHILDREN_ADJUST_NONE)
		m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
	if (m_children_adjust_y != CHILDREN_ADJUST_NONE)
		m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
	ChangeState(3, -1);
}

/* the offset of a boundary in the layout size, at the origin (1 its start, 2 its end) */
float Box::calc_offset(uint32_t orign, float layout_size, float boundary_size)
{
	switch (orign) {
	case 1:
		return (layout_size - boundary_size) * 0.5f;
	case 2:
		return (boundary_size - layout_size) * 0.5f;
	}
	return 0.0f;
}

/*
 * The children's boundary placed at the origin inside the margins; on an axis the box does not
 * follow its boundary on (boundary_mode, always on the axis it lays out along) the offset is
 * the one given (origin 3) or none.
 */
math::v4 Box::calc_origin_offset(const math::v4 &size, const math::v4 &offs, uint32_t ax,
				 uint32_t ay, uint32_t az)
{
	math::v4 rt(m_layout_vars.right_margin, m_layout_vars.top_margin, 0.0f, 0.0f);
	math::v4 lb(m_layout_vars.left_margin, m_layout_vars.bottom_margin, 0.0f, 0.0f);
	math::v4 layout = size - (rt + lb);
	math::v4 result = m_boundary_offs;
	math::v4 boundary = m_boundary_size;

	if (m_layout_type == LAYOUT_HORIZONTAL)
		m_layout_vars.boundary_mode_x = true;
	if (!m_layout_vars.boundary_mode_x) {
		boundary.set_x(0.0f);
		result.set_x(ax == 3 ? offs.extract_x() : 0.0f);
	}
	if (m_layout_type == LAYOUT_VERTICAL)
		m_layout_vars.boundary_mode_y = true;
	if (!m_layout_vars.boundary_mode_y) {
		boundary.set_y(0.0f);
		result.set_y(ay == 3 ? offs.extract_y() : 0.0f);
	}
	return result + math::v4(calc_offset(ax, layout.extract_x(), boundary.extract_x()),
				 calc_offset(ay, layout.extract_y(), boundary.extract_y()), 0.0f,
				 0.0f) +
	       (rt - lb) / 2.0f;
}

uint32_t Box::anchor_to_orign(uint32_t anchor)
{
	switch (anchor) {
	case 1:
		return 1;
	case 2:
		return 2;
	}
	return 0;
}

/* calc_origin_offset for the anchor: anchor 4 follows the boundary at the box's origin */
math::v4 Box::calc_anchor_offset(const math::v4 &size, const math::v4 &offs, uint32_t ax,
				 uint32_t ay, uint32_t az)
{
	math::v4 rt(m_layout_vars.right_margin, m_layout_vars.top_margin, 0.0f, 0.0f);
	math::v4 lb(m_layout_vars.left_margin, m_layout_vars.bottom_margin, 0.0f, 0.0f);
	math::v4 layout = size - (rt + lb);
	math::v4 result = m_boundary_offs;
	math::v4 boundary = m_boundary_size;

	if (ax == 4 && m_layout_type == LAYOUT_HORIZONTAL)
		m_layout_vars.boundary_mode_x = true;
	if (ax != 4 || !m_layout_vars.boundary_mode_x) {
		boundary.set_x(0.0f);
		result.set_x(ax == 3 ? offs.extract_x() : 0.0f);
	}
	if (ay == 4 && m_layout_type == LAYOUT_VERTICAL)
		m_layout_vars.boundary_mode_y = true;
	if (ay != 4 || !m_layout_vars.boundary_mode_y) {
		boundary.set_y(0.0f);
		result.set_y(ay == 3 ? offs.extract_y() : 0.0f);
	}

	uint32_t ox = ax == 4 ? m_origin_x : anchor_to_orign(ax);
	uint32_t oy = ay == 4 ? m_origin_y : anchor_to_orign(ay);

	return result + math::v4(calc_offset(ox, layout.extract_x(), boundary.extract_x()),
				 calc_offset(oy, layout.extract_y(), boundary.extract_y()), 0.0f,
				 0.0f) +
	       (rt - lb) / 2.0f;
}

/* kept between min_size and max_size */
int32_t Box::SetSize_core_force(const math::v4 &v, uint32_t notify_layout, bool force)
{
	math::v4 size(sce::Vectormath::Simd::Aos::maxPerElem(
		m_layout_vars.min_size.getImpl(),
		sce::Vectormath::Simd::Aos::minPerElem(m_layout_vars.max_size.getImpl(),
							v.getImpl())));

	return Plane::SetSize_core_force(size, notify_layout, force);
}

/* the size a child at the position may take in the box: to the nearer edge and as far again
   (align 0), to the far edge (1) or the near one (2), else the box's */
float Box::GetFitSize(uint32_t align, float pos, float box_size, float box_origin_offset,
		      float margin1_size, float margin2_size) const
{
	float half = box_size * 0.5f;
	float lo = (box_origin_offset - half) + margin1_size;
	float hi = (box_origin_offset + half) - margin2_size;
	float size = box_size;

	switch (align) {
	case 0: {
		float to_lo = pos - lo;
		float to_hi = hi - pos;

		size = (to_lo < to_hi ? to_lo : to_hi) * 2.0f;
		break;
	}
	case 1:
		size = hi - pos;
		break;
	case 2:
		size = pos - lo;
		break;
	}
	return size;
}

/*
 * Sizes the child on each axis as the box adjusts its children: the widest child's size once
 * they are ready (CONTENT_MAX, MAX), the fixed size (FIXED), or what fits (FIT, and children
 * that adjust to the box): the room the other children leave along the axis it lays out on,
 * else the room from the child to the box's edges.
 */
void Box::AdjustChildSize(Widget *w, const math::v4 &max_child_size,
			  const math::v4 &total_child_size, uint32_t num_children,
			  uint32_t num_fit_children_x, uint32_t num_fit_children_y,
			  ChildrenAdjust children_adjust_x, ChildrenAdjust children_adjust_y)
{
	math::v4 org_size = *w->GetSize(0);
	math::v4 size = org_size;
	math::v4 pos = w->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	uint32_t align_x = w->m_align_x;
	uint32_t align_y = w->m_align_y;
	uint32_t adjust_x = w->m_adjust_x;
	uint32_t adjust_y = w->m_adjust_y;
	uint32_t adjust_z = w->m_adjust_z;
	uint32_t anchor_x, anchor_y, anchor_z;
	uint32_t new_adjust_x = adjust_x;
	uint32_t new_adjust_y = adjust_y;
	bool fit_x = false;
	bool fit_y = false;

	w->GetAnchor(anchor_x, anchor_y, anchor_z);
	if (adjust_x != 1) {
		switch (children_adjust_x) {
		case CHILDREN_ADJUST_CONTENT_MAX:
			if (m_children_adjust_state_x == CHILDREN_ADJUST_INIT) {
				new_adjust_x = 2;
			} else if (m_children_adjust_state_x == CHILDREN_ADJUST_WAIT) {
				new_adjust_x = 0;
				size.set_x(max_child_size.extract_x());
			}
			break;
		case CHILDREN_ADJUST_FIXED:
			new_adjust_x = 0;
			size.set_x(m_children_width);
			break;
		case CHILDREN_ADJUST_FIT:
			if (m_adjust_x != 2 && m_adjust_x != 3) {
				new_adjust_x = 1;
				fit_x = true;
			}
			break;
		case CHILDREN_ADJUST_MAX:
			if (m_children_adjust_state_x == CHILDREN_ADJUST_INIT) {
				new_adjust_x = 0;
				size.set_x(max_child_size.extract_x());
			}
			break;
		}
	}
	if ((adjust_x == 1 &&
	     (m_adjust_x != 2 ||
	      (num_children > num_fit_children_x && m_layout_type != LAYOUT_HORIZONTAL))) ||
	    fit_x) {
		if (m_layout_type != LAYOUT_HORIZONTAL ||
		    m_layout_vars.space_adjust != SPACE_ADJUST_FIT) {
			if (m_layout_type == LAYOUT_HORIZONTAL) {
				float width = m_layout_size.extract_x() - total_child_size.extract_x();

				if (num_fit_children_x != 0)
					width = (width - m_layout_vars.space * (num_children - 1)) /
						num_fit_children_x;
				size.set_x(width);
			} else {
				float offs = anchor_x == 4 ? m_origin_offs.extract_x() :
							     w->GetAnchorOffset(0).extract_x();

				size.set_x(GetFitSize(align_x, pos.extract_x(), m_size.extract_x(), offs,
						      m_layout_vars.left_margin,
						      m_layout_vars.right_margin));
			}
		}
	}
	if (adjust_y != 1) {
		switch (children_adjust_y) {
		case CHILDREN_ADJUST_CONTENT_MAX:
			if (m_children_adjust_state_y == CHILDREN_ADJUST_INIT) {
				new_adjust_y = 2;
			} else if (m_children_adjust_state_y == CHILDREN_ADJUST_WAIT) {
				new_adjust_y = 0;
				size.set_y(max_child_size.extract_y());
			}
			break;
		case CHILDREN_ADJUST_FIXED:
			new_adjust_y = 0;
			size.set_y(m_children_height);
			break;
		case CHILDREN_ADJUST_FIT:
			if (m_adjust_y != 2 && m_adjust_y != 3) {
				new_adjust_y = 1;
				fit_y = true;
			}
			break;
		case CHILDREN_ADJUST_MAX:
			if (m_children_adjust_state_y == CHILDREN_ADJUST_INIT) {
				new_adjust_y = 0;
				size.set_y(max_child_size.extract_y());
			}
			break;
		}
	}
	if ((adjust_y == 1 &&
	     (m_adjust_y != 2 ||
	      (num_children > num_fit_children_y && m_layout_type != LAYOUT_VERTICAL))) ||
	    fit_y) {
		if (m_layout_type != LAYOUT_VERTICAL ||
		    m_layout_vars.space_adjust != SPACE_ADJUST_FIT) {
			if (m_layout_type == LAYOUT_VERTICAL) {
				float height = m_layout_size.extract_y() - total_child_size.extract_y();

				if (num_fit_children_y != 0)
					height = (height - m_layout_vars.space * (num_children - 1)) /
						 num_fit_children_y;
				size.set_y(height);
			} else {
				float offs = anchor_y == 4 ? m_origin_offs.extract_y() :
							     w->GetAnchorOffset(0).extract_y();

				size.set_y(GetFitSize(align_y, pos.extract_y(), m_size.extract_y(), offs,
						      m_layout_vars.bottom_margin,
						      m_layout_vars.top_margin));
			}
		}
	}
	if (adjust_x != new_adjust_x || adjust_y != new_adjust_y)
		w->SetAdjust(new_adjust_x, new_adjust_y, adjust_z);
	size = math::v4(sce::Vectormath::Simd::Aos::maxPerElem(
		sce::Vectormath::Simd::Aos::Vector4(0.0f), size.getImpl()));
	if (!(size == org_size))
		w->SetSize_core_force(size, 0xFF02, false);
}

/*
 * The next step of adjusting children: CONTENT_MAX waits a pass (WAIT), then sets their sizes
 * once the children are ready after their update (SET, DONE); MAX sets them at once; the rest
 * are done at once.
 */
Box::ChildrenAdjustState Box::GetNextAdjustChildrenState(bool after_children_onupdate,
							 ChildrenAdjustState state,
							 ChildrenAdjust mode)
{
	ChildrenAdjustState next = state;

	switch (mode) {
	case CHILDREN_ADJUST_NONE:
	case CHILDREN_ADJUST_FIXED:
	case CHILDREN_ADJUST_FIT:
		next = CHILDREN_ADJUST_DONE;
		break;
	case CHILDREN_ADJUST_CONTENT_MAX:
		switch (state) {
		case CHILDREN_ADJUST_INIT:
			next = CHILDREN_ADJUST_WAIT;
			break;
		case CHILDREN_ADJUST_WAIT:
			if (after_children_onupdate && m_prepare_count == 0)
				next = CHILDREN_ADJUST_SET;
			break;
		case CHILDREN_ADJUST_SET:
			if (after_children_onupdate && m_prepare_count == 0)
				next = CHILDREN_ADJUST_DONE;
			break;
		}
		break;
	case CHILDREN_ADJUST_MAX:
		switch (state) {
		case CHILDREN_ADJUST_INIT:
			if (after_children_onupdate && m_prepare_count == 0)
				next = CHILDREN_ADJUST_SET;
			break;
		case CHILDREN_ADJUST_SET:
			if (after_children_onupdate && m_prepare_count == 0)
				next = CHILDREN_ADJUST_DONE;
			break;
		}
		break;
	}
	return next;
}

/* on a step of either axis, sizes each child from the widest and the total of their sizes
   (children that fit counted apart, at 0) */
void Box::AdjustChildrenSize(bool after_children_onupdate)
{
	ChildrenAdjustState next_x = GetNextAdjustChildrenState(
		after_children_onupdate, m_children_adjust_state_x, m_children_adjust_x);
	ChildrenAdjustState next_y = GetNextAdjustChildrenState(
		after_children_onupdate, m_children_adjust_state_y, m_children_adjust_y);
	bool step_x = m_children_adjust_state_x != next_x;
	bool step_y = m_children_adjust_state_y != next_y;

	if (!step_x && !step_y)
		return;

	uint32_t num_fit_x = 0;
	uint32_t num_fit_y = 0;
	math::v4 max_size(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 total_size(0.0f, 0.0f, 0.0f, 0.0f);
	uint32_t num = 0;

	for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
		Widget *w = *it;

		if (!IsLayoutChild(w))
			continue;

		math::v4 size = *w->GetSize(0);

		if (w->m_adjust_x == 1 || m_children_adjust_x == CHILDREN_ADJUST_FIT) {
			num_fit_x++;
			size.set_x(0.0f);
		}
		if (w->m_adjust_y == 1 || m_children_adjust_y == CHILDREN_ADJUST_FIT) {
			num_fit_y++;
			size.set_y(0.0f);
		}
		num++;
		max_size = math::v4(sce::Vectormath::Simd::Aos::maxPerElem(size.getImpl(),
									  max_size.getImpl()));
		total_size = total_size + size;
	}

	ChildrenAdjust adjust_x = step_x ? m_children_adjust_x : CHILDREN_ADJUST_NONE;
	ChildrenAdjust adjust_y = step_y ? m_children_adjust_y : CHILDREN_ADJUST_NONE;

	for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
		Widget *w = *it;

		if (!IsLayoutChild(w))
			continue;
		AdjustChildSize(w, max_size, total_size, num, num_fit_x, num_fit_y, adjust_x,
				adjust_y);
	}
	m_children_adjust_state_x = next_x;
	m_children_adjust_state_y = next_y;
}

/* the layout size inside the margins on the axes that do not adjust (and, but with
   only_for_adjust_none, on those that adjust to the children or to their content) */
void Box::UpdateLayoutSizePre(bool only_for_adjust_none)
{
	math::v4 size = m_layout_size;
	uint32_t adjust_x = m_adjust_x;
	uint32_t adjust_y = m_adjust_y;

	if (adjust_x == 0 || (!only_for_adjust_none && (adjust_x == 1 || adjust_x == 4)))
		size.set_x((m_size.extract_x() - m_layout_vars.right_margin) -
			   m_layout_vars.left_margin);
	if (adjust_y == 0 || (!only_for_adjust_none && (adjust_y == 1 || adjust_y == 4)))
		size.set_y((m_size.extract_y() - m_layout_vars.top_margin) -
			   m_layout_vars.bottom_margin);
	if (!(size == m_layout_size)) {
		m_layout_size = size;
		if (m_layouter != NULL)
			m_layouter->SetDirty(1);
	}
}

void Box::Update()
{
	Plane::Update();
	UpdateLayoutSizePre(false);
	if (m_children_adjust_x == CHILDREN_ADJUST_NONE)
		m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
	if (m_children_adjust_y == CHILDREN_ADJUST_NONE)
		m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
	AdjustChildrenSize(false);
}

/*
 * After the children's update: children still getting ready start CONTENT_MAX again, and a
 * new layout starts FIT, MAX and NONE again (the y axis tests the x axis's NONE, as Sony's
 * does). The layouter is read without a NULL test.
 */
void Box::UpdateAfterChildren()
{
	Plane::UpdateAfterChildren();
	if (m_hold_count > 0)
		return;

	Layouter *layouter = m_layouter;

	if (m_prepare_count != 0) {
		if (m_children_adjust_state_x == CHILDREN_ADJUST_DONE &&
		    m_children_adjust_x == CHILDREN_ADJUST_CONTENT_MAX)
			m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
		if (m_children_adjust_state_y == CHILDREN_ADJUST_DONE &&
		    m_children_adjust_y == CHILDREN_ADJUST_CONTENT_MAX)
			m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
		if (layouter != NULL)
			layouter->SetDirty(1);
	}
	if (layouter->IsDirty(0xff)) {
		if (m_children_adjust_state_x == CHILDREN_ADJUST_DONE) {
			ChildrenAdjust adjust = m_children_adjust_x;

			if (adjust == CHILDREN_ADJUST_FIT || adjust == CHILDREN_ADJUST_MAX ||
			    adjust == CHILDREN_ADJUST_NONE)
				m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
		}
		if (m_children_adjust_state_y == CHILDREN_ADJUST_DONE) {
			ChildrenAdjust adjust = m_children_adjust_y;

			if (adjust == CHILDREN_ADJUST_FIT || adjust == CHILDREN_ADJUST_MAX ||
			    m_children_adjust_x == CHILDREN_ADJUST_NONE)
				m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
		}
	}
	AdjustChildrenSize(true);
}

/* state 3 while its children get ready, its layout is dirty or its children's sizes are not
   done */
uint32_t Box::CheckState()
{
	uint32_t state = Plane::CheckState();

	if (state != 0)
		return state;
	if (m_prepare_count != 0)
		return 3;
	if (m_layouter != NULL && m_layouter->IsDirty(0xff))
		return 3;
	if (m_children_adjust_state_x != CHILDREN_ADJUST_DONE ||
	    m_children_adjust_state_y != CHILDREN_ADJUST_DONE)
		return 3;
	return state;
}

uint32_t Box::ChangeState(uint32_t state, int32_t ev_type)
{
	if (m_state != state && state == 0 && m_auto_show) {
		if (!m_transition_complete) {
			m_transition = 0xff;
			m_transition_complete = true;
			DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(this, NULL, 2, 0.0f, NULL);
	}
	return Widget::ChangeState(state, ev_type);
}

/* Widget's (not Plane's), in the draw objects' safety; FIT starts again */
void Box::UpdateSize()
{
	graph::DrawObj::EnterSafety();
	Widget::UpdateSize();
	graph::DrawObj::FinishSafety();
	if (m_children_adjust_x == CHILDREN_ADJUST_FIT)
		m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
	if (m_children_adjust_y == CHILDREN_ADJUST_FIT)
		m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
}

void Box::UpdateAdjustSize()
{
	if (m_last_adjust_x == m_adjust_x && m_last_adjust_y == m_adjust_y)
		return;
	m_last_adjust_x = m_adjust_x;
	m_last_adjust_y = m_adjust_y;
	if (m_children_adjust_x == CHILDREN_ADJUST_FIT)
		m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
	if (m_children_adjust_y == CHILDREN_ADJUST_FIT)
		m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
	if (m_layouter != NULL)
		m_layouter->SetDirty(1);
}

/* only the children it does not lay out are adjusted as by any widget */
void Box::UpdateAdjustChildSize(Widget *child)
{
	if (child->m_parent_protected)
		Plane::UpdateAdjustChildSize(child);
}

/* the total size of the children it lays out, with the space between them along its layout
   (made to fill the layout size with SPACE_ADJUST_FIT) */
math::v4 Box::GetChildrenInfo()
{
	UpdateLayoutSizePre(true);

	math::v4 total(0.0f, 0.0f, 0.0f, 0.0f);
	uint32_t num = 0;

	for (list<Widget *>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
		Widget *w = *it;

		if (!IsLayoutChild(w))
			continue;
		total = total + *w->GetSize(0);
		num++;
	}
	if (m_layout_vars.space_adjust == SPACE_ADJUST_FIT) {
		if (m_layout_type == LAYOUT_HORIZONTAL &&
		    (m_adjust_x == 0 || m_adjust_x == 1 || m_adjust_x == 4)) {
			math::v4 size = m_layout_size;

			if (num > 1)
				m_layout_vars.space = sce_paf_floorf(
					(size.extract_x() - total.extract_x()) / (float)(num - 1));
		} else if (m_layout_type == LAYOUT_VERTICAL &&
			   (m_adjust_y == 0 || m_adjust_y == 1 || m_adjust_y == 4)) {
			math::v4 size = m_layout_size;

			if (num > 1)
				m_layout_vars.space = sce_paf_floorf(
					(size.extract_y() - total.extract_y()) / (float)(num - 1));
		}
	}
	if (num > 1) {
		float space = m_layout_vars.space * (float)(num - 1);

		if (m_layout_type == LAYOUT_HORIZONTAL)
			total = total + math::v4(space, 0.0f, 0.0f, 0.0f);
		else if (m_layout_type == LAYOUT_VERTICAL)
			total = total + math::v4(0.0f, space, 0.0f, 0.0f);
	}
	return total;
}

/* the layout size the boundary's on the axes adjusting to the children (2), and kept to its
   ratio on an axis adjusting to the other (3) */
void Box::UpdateLayoutSizePost(const math::v4 &boundary_size)
{
	uint32_t adjust_x = m_adjust_x;
	uint32_t adjust_y = m_adjust_y;

	if (adjust_x == 3 && adjust_y == 3) {
		adjust_x = 2;
		adjust_y = 2;
	}
	if (adjust_x == 2)
		m_layout_size.set_x(boundary_size.extract_x());
	if (adjust_y == 2)
		m_layout_size.set_y(boundary_size.extract_y());
	if (adjust_x == 3) {
		if (boundary_size.extract_y() != 0.0f)
			m_layout_size.set_x(m_layout_size.extract_y() * boundary_size.extract_x() /
					    boundary_size.extract_y());
	} else if (adjust_y == 3) {
		if (boundary_size.extract_x() != 0.0f)
			m_layout_size.set_y(m_layout_size.extract_x() * boundary_size.extract_y() /
					    boundary_size.extract_x());
	}
}

/*
 * The size and center of the box around the children it lays out, from their centers and
 * sizes; across its layout a child anchored 4 counts at its position when it adjusts to the box
 * (1), and a child not anchored 4 does not count.
 */
void Box::GetBoundary(math::v4 *size, math::v4 *offs) const
{
	math::v4 min(3.4028236e+18f, 3.4028236e+18f, 3.4028236e+18f, 3.4028236e+18f);
	math::v4 max(-3.4028236e+18f, -3.4028236e+18f, -3.4028236e+18f, -3.4028236e+18f);
	bool empty = true;
	math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);

	/* paf's list has no const_iterator */
	list<Widget *> &children = const_cast<list<Widget *> &>(m_children);

	for (list<Widget *>::iterator it = children.begin(); it != children.end(); ++it) {
		Widget *w = *it;

		if (!IsLayoutChild(w))
			continue;

		math::v4 child_size = *w->GetSize(0);
		math::v4 pos = w->GetPos(0, NULL, zero);
		math::v4 center = w->GetPosCenter(0, NULL, zero);
		math::v4 half = child_size * 0.5f;
		math::v4 lo = center - half;
		math::v4 hi = center + half;
		uint32_t ax, ay, az;

		w->GetAnchor(ax, ay, az);

		uint32_t adjust_x = w->m_adjust_x;
		uint32_t adjust_y = w->m_adjust_y;

		if (m_layout_type != LAYOUT_HORIZONTAL) {
			if (ax == 4) {
				if (adjust_x == 1) {
					hi.set_x(pos.extract_x());
					lo.set_x(pos.extract_x());
				}
			} else {
				lo.set_x(3.4028236e+18f);
				hi.set_x(-3.4028236e+18f);
			}
		}
		if (m_layout_type != LAYOUT_VERTICAL) {
			if (ay == 4) {
				if (adjust_y == 1) {
					lo.set_y(pos.extract_y());
					hi.set_y(pos.extract_y());
				}
			} else {
				lo.set_y(3.4028236e+18f);
				hi.set_y(-3.4028236e+18f);
			}
		}
		min = math::v4(sce::Vectormath::Simd::Aos::minPerElem(lo.getImpl(), min.getImpl()));
		empty = false;
		max = math::v4(sce::Vectormath::Simd::Aos::maxPerElem(hi.getImpl(), max.getImpl()));
	}
	if (empty) {
		*offs = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		*size = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	} else {
		*offs = (max + min) / 2.0f;
		*size = max - min;
	}
}

/*
 * Its size from its children's boundary, kept between min_size and max_size, and its origin
 * offset placed again (the target of a running origin animation, Sony's way: a NULL target
 * with no main queue is written through).
 */
void Box::UpdateBoxSize()
{
	GetBoundary(&m_boundary_size, &m_boundary_offs);
	UpdateLayoutSizePost(m_boundary_size);

	math::v4 margin(m_layout_vars.right_margin + m_layout_vars.left_margin,
			m_layout_vars.top_margin + m_layout_vars.bottom_margin, 0.0f, 0.0f);
	math::v4 size(sce::Vectormath::Simd::Aos::maxPerElem(
		sce::Vectormath::Simd::Aos::Vector4(0.0f),
		sce::Vectormath::Simd::Aos::maxPerElem(
			(m_layout_vars.min_size - margin).getImpl(),
			sce::Vectormath::Simd::Aos::minPerElem(
				(m_layout_vars.max_size - margin).getImpl(),
				m_layout_size.getImpl())) +
			margin.getImpl()));

	if (!(size == m_size))
		SetSize_core(size, 0xFF02, false);

	AnimQueue *anim = FindAnim(ANIM_ORIGIN);

	if (anim == NULL) {
		m_origin_offs = calc_origin_offset(m_size, m_origin_offs, m_origin_x, m_origin_y,
						   m_origin_z);
		SetUpdateParam(2);
	} else {
		math::v4 *target = NULL;
		AnimQueueBase *queue = anim->m_queue[0];

		if (queue != NULL)
			target = queue->GetTargetV4();
		*target = calc_origin_offset(m_size, *target, m_origin_x, m_origin_y, m_origin_z);
	}
}

void Box::SetChildrenAdjustX(ChildrenAdjust mode, float width)
{
	bool changed = false;

	if (m_children_adjust_x != mode) {
		changed = true;
		m_children_adjust_x = mode;
	}
	if (m_children_width != width) {
		if (m_children_adjust_x == CHILDREN_ADJUST_FIXED)
			changed = true;
		m_children_width = width;
	}
	if (changed) {
		m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
		if (m_layouter != NULL)
			m_layouter->SetDirty(1);
		ChangeState(3, -1);
	}
}

void Box::SetChildrenAdjustY(ChildrenAdjust mode, float height)
{
	bool changed = false;

	if (m_children_adjust_y != mode) {
		changed = true;
		m_children_adjust_y = mode;
	}
	if (m_children_height != height) {
		if (m_children_adjust_y == CHILDREN_ADJUST_FIXED)
			changed = true;
		m_children_height = height;
	}
	if (changed) {
		m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
		if (m_layouter != NULL)
			m_layouter->SetDirty(1);
		ChangeState(3, -1);
	}
}

/* a FIT axis starts again when its margins, its boundary mode (across the layout) or the space
   (along it) change */
void Box::SetLayoutVars(const LayoutVars &layout_vars)
{
	bool min_changed = !(layout_vars.min_size == m_layout_vars.min_size);
	bool max_changed = !(layout_vars.max_size == m_layout_vars.max_size);
	bool space_adjust_changed = m_layout_vars.space_adjust != layout_vars.space_adjust;
	bool space_changed = m_layout_vars.space != layout_vars.space;
	bool top_changed = m_layout_vars.top_margin != layout_vars.top_margin;
	bool bottom_changed = m_layout_vars.bottom_margin != layout_vars.bottom_margin;
	bool left_changed = m_layout_vars.left_margin != layout_vars.left_margin;
	bool right_changed = m_layout_vars.right_margin != layout_vars.right_margin;
	bool align_x_changed = m_layout_vars.children_align_x != layout_vars.children_align_x;
	bool align_y_changed = m_layout_vars.children_align_y != layout_vars.children_align_y;
	bool boundary_x_changed = m_layout_vars.boundary_mode_x != layout_vars.boundary_mode_x;
	bool boundary_y_changed = m_layout_vars.boundary_mode_y != layout_vars.boundary_mode_y;

	if (min_changed || max_changed || space_adjust_changed || space_changed || top_changed ||
	    bottom_changed || left_changed || right_changed || align_x_changed ||
	    align_y_changed || boundary_x_changed || boundary_y_changed) {
		m_layout_vars = layout_vars;
		if (m_layouter != NULL)
			m_layouter->SetDirty(1);
		ChangeState(3, -1);
	}
	if (m_children_adjust_x == CHILDREN_ADJUST_FIT &&
	    (left_changed || right_changed ||
	     (m_layout_type != LAYOUT_HORIZONTAL && boundary_x_changed) ||
	     (m_layout_type == LAYOUT_HORIZONTAL && space_changed)))
		m_children_adjust_state_x = CHILDREN_ADJUST_INIT;
	if (m_children_adjust_y == CHILDREN_ADJUST_FIT &&
	    (top_changed || bottom_changed ||
	     (m_layout_type != LAYOUT_VERTICAL && boundary_y_changed) ||
	     (m_layout_type == LAYOUT_VERTICAL && space_changed)))
		m_children_adjust_state_y = CHILDREN_ADJUST_INIT;
}

void Box::SetChildPos(Widget *w, math::v4 pos)
{
	if (m_cb_set_child_pos != NULL)
		m_cb_set_child_pos(this, w, pos);
	else
		w->SetPos_core(pos, 0xFF01, false);
}

void Box::AddToExclusiveList(Widget *w)
{
	if (!w->m_parent_protected) {
		w->m_parent_protected = true;
		if (m_layouter != NULL)
			m_layouter->SetDirty(1);
	}
}

void Box::DeleteFromExclusiveList(Widget *w)
{
	if (w->m_parent_protected) {
		w->m_parent_protected = false;
		if (m_layouter != NULL)
			m_layouter->SetDirty(1);
	}
}

const char BoxParam::m_param_name[] = "box_base";

const char *BoxParam::GetParamName() const
{
	return m_param_name;
}

const char *BoxParam::ParamName()
{
	return m_param_name;
}

int32_t BoxParam::Create(int32_t mode)
{
	int32_t ret = PlaneParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("exclusive_list", &m_exclusive_list,
				   ResourceParam::SEMANTIC_STRING);
	return 0;
}

/* the children named in the exclusive list are left out of the layout */
int32_t BoxParam::Set()
{
	int32_t ret = PlaneParam::Set();

	if (ret < 0)
		return ret;

	Box *box = (Box *)m_widget;
	char *names = new char[256];

	if (names != NULL) {
		char *str = names;
		char *save;

		sce_paf_strncpy(names, m_exclusive_list, 255);
		names[255] = '\0';
		save = NULL;
		for (;;) {
			char *name = sce_paf_strtok_r(str, ", \t", &save);

			if (name == NULL)
				break;

			Widget *w = box->FindChild(IDParam(string(name)), 2);

			if (w != NULL)
				box->AddToExclusiveList(w);
			str = NULL;
		}
		delete[] names;
	}
	return 0;
}

const char BoxLayoutParam::m_param_name[] = "layout_box";

const char *BoxLayoutParam::GetParamName() const
{
	return m_param_name;
}

const char *BoxLayoutParam::ParamName()
{
	return m_param_name;
}

/* each parameter added while the resource is there to read it */
int32_t BoxLayoutParam::Create(int32_t mode)
{
	int32_t ret = WidgetLayoutParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("layout_type", &m_layout_type, ResourceParam::SEMANTIC_INT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("min_size", &m_min_size, ResourceParam::SEMANTIC_VEC4);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("max_size", &m_max_size, ResourceParam::SEMANTIC_VEC4);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("space_adjust", &m_space_adjust, ResourceParam::SEMANTIC_INT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("space", &m_space, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("top_margin", &m_top_margin, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("bottom_margin", &m_bottom_margin, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("left_margin", &m_left_margin, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("right_margin", &m_right_margin, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("children_align_x", &m_children_align_x, ResourceParam::SEMANTIC_INT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("children_align_y", &m_children_align_y, ResourceParam::SEMANTIC_INT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("children_adjust_x", &m_children_adjust_x,
			   ResourceParam::SEMANTIC_INT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("children_adjust_y", &m_children_adjust_y,
			   ResourceParam::SEMANTIC_INT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("children_width", &m_children_width, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("children_height", &m_children_height, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("boundary_mode_x", &m_boundary_mode_x, ResourceParam::SEMANTIC_BOOL);
	if (m_serial == NULL)
		return 0;
	m_serial->AddParam("boundary_mode_y", &m_boundary_mode_y, ResourceParam::SEMANTIC_BOOL);
	return 0;
}

int32_t BoxLayoutParam::Set()
{
	int32_t ret = WidgetLayoutParam::Set();

	if (ret < 0)
		return ret;

	Box *box = (Box *)m_widget;
	Box::LayoutVars vars;

	box->SetLayoutType(m_layout_type);
	vars.min_size = m_min_size;
	vars.max_size = m_max_size;
	vars.space_adjust = m_space_adjust;
	vars.space = m_space;
	vars.top_margin = m_top_margin;
	vars.bottom_margin = m_bottom_margin;
	vars.left_margin = m_left_margin;
	vars.right_margin = m_right_margin;
	vars.children_align_x = m_children_align_x;
	vars.children_align_y = m_children_align_y;
	vars.boundary_mode_x = m_boundary_mode_x;
	vars.boundary_mode_y = m_boundary_mode_y;
	box->SetLayoutVars(vars);
	box->SetChildrenAdjustX(m_children_adjust_x, m_children_width);
	box->SetChildrenAdjustY(m_children_adjust_y, m_children_height);
	return 0;
}

int32_t BoxLayoutParam::Get()
{
	int32_t ret = WidgetLayoutParam::Get();

	if (ret < 0)
		return ret;

	Box *box = (Box *)m_widget;
	const Box::LayoutVars &vars = box->GetLayoutVars();

	m_layout_type = box->GetLayoutType();
	m_min_size = vars.min_size;
	m_max_size = vars.max_size;
	m_space_adjust = vars.space_adjust;
	m_space = vars.space;
	m_top_margin = vars.top_margin;
	m_bottom_margin = vars.bottom_margin;
	m_left_margin = vars.left_margin;
	m_right_margin = vars.right_margin;
	m_children_align_x = vars.children_align_x;
	m_children_align_y = vars.children_align_y;
	m_boundary_mode_x = vars.boundary_mode_x;
	m_boundary_mode_y = vars.boundary_mode_y;
	box->GetChildrenAdjustX(&m_children_adjust_x, &m_children_width);
	box->GetChildrenAdjustY(&m_children_adjust_y, &m_children_height);
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewBox(Widget *parent)
{
	return new Box(parent, NULL);
}

static WidgetParam *NewBoxParam()
{
	return new BoxParam;
}

static VersatileParam *NewBoxLayoutParam()
{
	return new BoxLayoutParam;
}

static VersatileParam *NewBoxFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewBoxStyleParam()
{
	return new PlaneStyleParam;
}

static VersatileParam *NewBoxHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewBoxSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryBox = {
	Box::TypeName(),
	NewBox,
	NewBoxParam,
	NewBoxLayoutParam,
	NewBoxFocusParam,
	NewBoxStyleParam,
	NewBoxHandlerParam,
	NewBoxSoundParam,
};

/* the vectors below, made in the static initializer with the constructor inlined (name ours) */
static inline __attribute__((always_inline)) math::v4 Vector(float x, float y, float z, float w)
{
	return math::v4(x, y, z, w);
}

/* no margins or space, the children's own align, its boundary followed on both axes, any size
   up to 3.4e18 */

Box::LayoutVars Box::s_default_layout_vars = {
	Vector(0.0f, 0.0f, 0.0f, 0.0f),
	Vector(3.4028236e+18f, 3.4028236e+18f, 0.0f, 0.0f),
	Box::SPACE_ADJUST_NONE,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	0.0f,
	3,
	3,
	true,
	true,
};
