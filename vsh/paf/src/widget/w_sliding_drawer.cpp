/*
 * paf::ui::SlidingDrawer (DEBUGGER 0x81128d50..0x81129d40): a scroll view whose view parent is
 * dragged within the room its size leaves around its scroll box. No record or string names the
 * file.
 */
#include <stddef.h>
#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <versatileparam/resourceparam.h>
#include <widget/core/event.h>
#include <widget/core/layouter.h>
#include <widget/factory.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_sliding_drawer.h>
#include "../paf_types.h"

namespace paf {
namespace ui {

using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector4;
using sce::Vectormath::Simd::floatInVec;

/* the zero the focus target compares the view's room with, made at start-up after the
   factory (name ours) */
namespace {
extern const floatInVec s_zero;
}

const char SlidingDrawer::m_widget_type[] = "sliding_drawer";

SlidingDrawer::SlidingDrawer(Widget *parent, CreateParam *param)
	: ScrollViewBase(parent, param), m_movable_direction(MoveDir_XY),
	  m_last_pos(0.0f, 0.0f, 0.0f, 0.0f), m_move(0.0f, 0.0f, 0.0f, 0.0f),
	  m_movable_range(0.0f, 0.0f, 0.0f, 0.0f)
{
	ScrollViewBase::SetVerticalScrollBar(NULL);
	ScrollViewBase::SetHorizontalScrollBar(NULL);
}

SlidingDrawerParam::SlidingDrawerParam()
{
}

SlidingDrawerLayoutParam::SlidingDrawerLayoutParam()
{
}

SlidingDrawerStyleParam::SlidingDrawerStyleParam()
{
}

SlidingDrawerHandlerParam::SlidingDrawerHandlerParam()
{
}

bool SlidingDrawer::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ScrollViewBase::IsInherit(type);
}

char *SlidingDrawer::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *SlidingDrawer::TypeName()
{
	return m_widget_type;
}

int32_t SlidingDrawer::AddSnapPoint(const ScrollViewSnapPoint &snap_point)
{
	for (list<Snap>::iterator it = m_snap_list.begin(); it != m_snap_list.end(); ++it)
		if (((snap_point.m_pos.getImpl().getY() == it->m_pos.getImpl().getY()) &
		     (snap_point.m_pos.getImpl().getX() == it->m_pos.getImpl().getX()))
			    .getAsBool())
			return SCE_PAF_ERROR_ALREADY_EXISTS;
	return ScrollViewBase::AddSnapPoint(snap_point);
}

/* where on the child the alignment puts a point, in the view parent's space, moved by the
   offset */
static inline math::v2 AlignedPos(Widget *view_parent, Widget *child,
				  SlidingDrawer::SNAP_ALIGN_TYPE align, const math::v2 &offset)
{
	math::v4 center = child->GetPosCenter(0, view_parent, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v4 shift;

	switch (align) {
	case SlidingDrawer::SNAP_ALIGN_TOP:
		shift = math::v4(0.0f, child->GetSize(0)->extract_y() * 0.5f, 0.0f, 0.0f);
		break;
	case SlidingDrawer::SNAP_ALIGN_BOTTOM:
		shift = math::v4(0.0f, child->GetSize(0)->extract_y() * -0.5f, 0.0f, 0.0f);
		break;
	case SlidingDrawer::SNAP_ALIGN_LEFT:
		shift = math::v4(child->GetSize(0)->extract_x() * -0.5f, 0.0f, 0.0f, 0.0f);
		break;
	case SlidingDrawer::SNAP_ALIGN_RIGHT:
		shift = math::v4(child->GetSize(0)->extract_x() * 0.5f, 0.0f, 0.0f, 0.0f);
		break;
	default:
		shift = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		break;
	}

	math::v4 pos = center + (shift + math::v4(Vector4(offset.getImpl(), offset.getImpl())));

	return math::v2(pos.getImpl().getXY());
}

int32_t SlidingDrawer::AddSnapPoint(Widget *child, SNAP_ALIGN_TYPE align,
				    const math::v2 &offset)
{
	if (child == NULL)
		return SCE_PAF_ERROR_TOPLEVEL_ARGUMENT;
	return AddSnapPoint(ScrollViewSnapPoint(AlignedPos(m_view_parent, child, align, offset)));
}

int32_t SlidingDrawer::AddSnapPoint(const IDParam &id, SNAP_ALIGN_TYPE align,
				    const math::v2 &offset)
{
	Widget *child = m_view_parent->FindChild(id, 0);

	if (child == NULL)
		return SCE_PAF_ERROR_NO_CHILD;
	return AddSnapPoint(child, align, offset);
}

void SlidingDrawer::DelAllSnapPoint()
{
	m_snap_list.clear();
	ScePafWidget_A0538225(-1);
}

void SlidingDrawer::SetViewFocus(Widget *child, SNAP_ALIGN_TYPE align, const math::v2 &offset,
				 Timer *timer)
{
	if (child == NULL)
		return;
	SetViewFocus(AlignedPos(m_view_parent, child, align, offset), timer, true, true);
}

void SlidingDrawer::SetViewFocus(const IDParam &id, SNAP_ALIGN_TYPE align,
				 const math::v2 &offset, Timer *timer)
{
	Widget *child = m_view_parent->FindChild(id, 0);

	if (child != NULL)
		SetViewFocus(child, align, offset, timer);
}

math::v2 SlidingDrawer::GetViewFocus(int32_t opt)
{
	math::v4 pos = m_view_parent->GetPos(opt, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	return math::v2(pos.getImpl().getXY()) * math::v2(-1.0f, -1.0f);
}

void SlidingDrawer::SetMovableDirection(uint32_t dir)
{
	m_movable_direction = dir;
	UpdateMovableDirection();
}

uint32_t SlidingDrawer::GetMovableDirection()
{
	return m_movable_direction;
}

void SlidingDrawer::CheckMovableRange()
{
	if (m_view_parent->m_prepare_count != 0)
		return;

	math::v4 range = m_movable_range;

	if (range.extract_x() > range.extract_z() || range.extract_y() < range.extract_w())
		return;

	math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v4 moved = pos;
	bool out = false;

	if (pos.extract_x() > m_movable_range.extract_z()) {
		out = true;
		moved.set_x(m_movable_range.extract_z());
	}
	if (pos.extract_x() < m_movable_range.extract_x()) {
		out = true;
		moved.set_x(m_movable_range.extract_x());
	}
	if (pos.extract_y() > m_movable_range.extract_y()) {
		out = true;
		moved.set_y(m_movable_range.extract_y());
	}
	if (pos.extract_y() < m_movable_range.extract_w()) {
		out = true;
		moved.set_y(m_movable_range.extract_w());
	}
	if (out == true) {
		m_view_parent->KillAnim(ANIM_MOVE);
		m_view_parent->SetPos(moved);
		m_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

int32_t SlidingDrawer::HandleTouchEvent(Event *e)
{
	int32_t ret = ScrollViewBase::HandleTouchEvent(e);

	if (m_layouter->IsDirty(0xff))
		CheckMovableRange();
	return ret;
}

/* the hit box: the scroll box, where the view parent is */
static inline void SetScrollBoxHitBox(ScrollViewBase *view)
{
	Vector2 box = view->m_scroll_box.getImpl().getXY();
	Vector2 size = view->m_scroll_box_size.getImpl();
	Vector2 corner = box - mulPerElem(size, Vector2(-0.5f, 0.5f));
	math::v4 center = view->m_view_parent->GetPosCenter(0, view, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	/* the size read again after GetPosCenter, as Sony's */
	view->SetHitBox(3, math::v4(Vector4(view->m_scroll_box_size.getImpl(), Vector2(1.0f))),
			math::v4(Vector4(corner + center.getImpl().getXY(), Vector2(0.0f))));
}

void SlidingDrawer::OnViewLayoutUpdated(bool is_dirty)
{
	if (is_dirty) {
		UpdateViewBox();
		SetScrollBoxHitBox(this);
	}
	ScrollViewBase::OnViewLayoutUpdated(is_dirty);
}

void SlidingDrawer::OnPartsLayoutUpdated(bool is_dirty)
{
	if (is_dirty)
		SetScrollBoxHitBox(this);
	ScrollViewBase::OnPartsLayoutUpdated(is_dirty);
}

SlidingDrawer::~SlidingDrawer()
{
}

void SlidingDrawer::UpdateAfterChildren()
{
	ScrollViewBase::UpdateAfterChildren();

	math::v4 last = m_last_pos;
	math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	m_last_pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	math::v4 move = pos - last;

	if (move == math::v4(0.0f, 0.0f, 0.0f, 0.0f) && length(m_move.getImpl()) >= 1.0f &&
	    m_layouter != NULL)
		/* stopped after a move: put back in range on the next layout */
		m_layouter->SetDirty(1);
	m_move = move;
}

ScrollViewFocusTarget SlidingDrawer::ScePafWidget_7F653E08(const math::v2 &focus,
							   int32_t snap_id)
{
	if (m_snap_list.size() != 0)
		return ScrollViewBase::ScePafWidget_7F653E08(focus, snap_id);

	Vector2 size = m_view_parent->GetSize(0)->getImpl().getXY();
	Vector4 half = mulPerElem(Vector4(size, size), Vector4(-0.5f, 0.5f, 0.5f, -0.5f));
	Vector4 room = m_scroll_box.getImpl() - (half - Vector4(focus.getImpl(), focus.getImpl()));
	ScrollViewFocusTarget target;

	if (m_scroll_box_size.extract_x() > m_size.extract_x() ||
	    m_scroll_box_size.extract_y() > m_size.extract_y()) {
		target.m_pos = math::v2(0.0f, 0.0f);
		target.m_move = math::v2(0.0f, 0.0f);
		target.m_dir = MoveDir_None;
		target.m_snap_id = -1;
		return target;
	}

	math::v2 move(0.0f, 0.0f);

	if (room.getX() < s_zero)
		move.set_x(room.getX());
	else if (room.getZ() > s_zero)
		move.set_x(room.getZ());
	if (room.getY() > s_zero)
		move.set_y(room.getY());
	else if (room.getW() < s_zero)
		move.set_y(room.getW());
	target.m_pos = move;
	target.m_move = math::v2(0.0f, 0.0f);
	target.m_dir = MoveDir_None;
	target.m_snap_id = -1;
	return target;
}

void SlidingDrawer::SetViewBox(const math::v4 &view_box)
{
	ScrollViewBase::SetViewBox(view_box);
}

void SlidingDrawer::UpdateViewBox()
{
	Vector4 box(0.0f);
	list<Widget *> &children = m_view_parent->m_children;

	for (list<Widget *>::iterator it = children.begin(); it != m_view_parent->m_children.end();
	     ++it) {
		Widget *child = *it;

		if (child->m_hold_count <= 0) {
			math::v4 center =
				child->GetPosCenter(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			Vector2 pos = mulPerElem(center.getImpl().getXY(), Vector2(1.0f, -1.0f));
			Vector2 half = child->GetSize(1)->getImpl().getXY() * 0.5f;

			box = Vector4(minPerElem(pos - half, box.getXY()),
				      maxPerElem(pos + half, Vector2(box.getZ(), box.getW())));
		}
	}

	math::v4 view_box(mulPerElem(box, Vector4(1.0f, -1.0f, 1.0f, -1.0f)));

	SetViewBox(view_box);
}

void SlidingDrawer::ScePafWidget_A0538225(int32_t snap_id)
{
	if (m_block_count == 0) {
		CheckMovableRange();
		if (length(m_move.getImpl().getXY()) >= 1.0f)
			/* still moving */
			return;
	}
	ScrollViewBase::ScePafWidget_A0538225(snap_id);
}

void SlidingDrawer::UpdateMovableDirection()
{
	Vector2 size = m_size.getImpl().getXY();
	Vector2 box = m_scroll_box_size.getImpl();
	uint32_t dir = size.getX() > box.getX() ? MoveDir_X : MoveDir_None;

	if (size.getY() > box.getY())
		dir |= MoveDir_Y;

	if ((m_movable_direction & MoveDir_X) == 0)
		dir &= ~MoveDir_X;
	if ((m_movable_direction & MoveDir_Y) == 0)
		dir &= ~MoveDir_Y;
	m_movable_dir = dir;
	m_movable_range = math::v4(mulPerElem(Vector4(size, size), Vector4(-0.5f, 0.5f, 0.5f, -0.5f)) -
				   m_scroll_box.getImpl());
}

void SlidingDrawer::MoveViewParent(const math::v2 &pos, Timer *timer)
{
	ScrollViewBase::MoveViewParent(pos, timer);
}

const char SlidingDrawerParam::m_param_name[] = "sliding_drawer_base";

const char *SlidingDrawerParam::GetParamName() const
{
	return m_param_name;
}

const char *SlidingDrawerParam::ParamName()
{
	return m_param_name;
}

int32_t SlidingDrawerParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	return 0;
}

int32_t SlidingDrawerParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;
	return 0;
}

int32_t SlidingDrawerParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;
	return 0;
}

const char SlidingDrawerStyleParam::m_param_name[] = "style_sliding_drawer";

const char *SlidingDrawerStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *SlidingDrawerStyleParam::ParamName()
{
	return m_param_name;
}

const char SlidingDrawerLayoutParam::m_param_name[] = "layout_sliding_drawer";

const char *SlidingDrawerLayoutParam::GetParamName() const
{
	return m_param_name;
}

const char *SlidingDrawerLayoutParam::ParamName()
{
	return m_param_name;
}

const char SlidingDrawerHandlerParam::m_param_name[] = "handler_sliding_drawer";

const char *SlidingDrawerHandlerParam::GetParamName() const
{
	return m_param_name;
}

const char *SlidingDrawerHandlerParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewSlidingDrawer(Widget *parent)
{
	return new SlidingDrawer(parent, NULL);
}

static WidgetParam *NewSlidingDrawerParam()
{
	return new SlidingDrawerParam;
}

static VersatileParam *NewSlidingDrawerLayoutParam()
{
	return new SlidingDrawerLayoutParam;
}

static VersatileParam *NewSlidingDrawerFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewSlidingDrawerStyleParam()
{
	return new SlidingDrawerStyleParam;
}

static VersatileParam *NewSlidingDrawerHandlerParam()
{
	return new SlidingDrawerHandlerParam;
}

static VersatileParam *NewSlidingDrawerSoundParam()
{
	return new ScrollViewBaseSoundParam;
}

FactoryFuncTable g_factorySlidingDrawer = {
	SlidingDrawer::TypeName(),
	NewSlidingDrawer,
	NewSlidingDrawerParam,
	NewSlidingDrawerLayoutParam,
	NewSlidingDrawerFocusParam,
	NewSlidingDrawerStyleParam,
	NewSlidingDrawerHandlerParam,
	NewSlidingDrawerSoundParam,
};

namespace paf {
namespace ui {
namespace {
const floatInVec s_zero(0.0f);
}
}
}
