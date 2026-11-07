/*
 * paf::ui::SlidingDrawer: a scroll view without scroll bars whose view parent is dragged
 * within the room its own size leaves around its scroll box, its snap points and focus placed
 * at a child's centre or edge (SNAP_ALIGN_TYPE).
 * Names after the exports and the parameters' names; the members' names are ours.
 */
#ifndef PAF_WIDGET_W_SLIDING_DRAWER_H
#define PAF_WIDGET_W_SLIDING_DRAWER_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>
#include <widget/w_scroll_view_base.h>

namespace paf {

class IDParam;
class Timer;

namespace ui {

class SlidingDrawer : public ScrollViewBase {
public:
	/* where on the child a snap point or the focus is put: its centre or the middle of one
	   of its edges, y up (names ours) */
	enum SNAP_ALIGN_TYPE {
		SNAP_ALIGN_CENTER = 0,
		SNAP_ALIGN_TOP = 1,
		SNAP_ALIGN_BOTTOM = 2,
		SNAP_ALIGN_LEFT = 3,
		SNAP_ALIGN_RIGHT = 4
	};

	static const char m_widget_type[];
	static const char *TypeName();

	SlidingDrawer(Widget *parent, CreateParam *param);
	virtual ~SlidingDrawer();

	virtual int32_t HandleTouchEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateAfterChildren();
	virtual int32_t AddSnapPoint(const ScrollViewSnapPoint &snap_point);
	virtual void OnViewLayoutUpdated(bool is_dirty);
	virtual void OnPartsLayoutUpdated(bool is_dirty);
	/* the focus target: ScrollViewBase's with snap points, else the view moved back inside
	   the room it may move in (ScePafWidget 5654DE15) */
	virtual ScrollViewFocusTarget ScePafWidget_7F653E08(const math::v2 &focus, int32_t snap_id);
	virtual void SetViewBox(const math::v4 &view_box);
	/* ScrollViewBase's move to the focus target, unless the view is still being dragged and
	   has not been put back in range (ScePafWidget 21DD74D2) */
	virtual void ScePafWidget_A0538225(int32_t snap_id);
	virtual void UpdateMovableDirection();
	/* ScrollViewBase's (ScePafWidget 933DF970) */
	virtual void MoveViewParent(const math::v2 &pos, Timer *timer);

	/* the slot after ScrollViewBase's, at 0x26c (index 155): the view box made from its view
	   parent's children */
	virtual void UpdateViewBox();

	/* a snap point at the child, by the alignment, moved by the offset */
	int32_t AddSnapPoint(Widget *child, SNAP_ALIGN_TYPE align, const math::v2 &offset);
	int32_t AddSnapPoint(const IDParam &id, SNAP_ALIGN_TYPE align, const math::v2 &offset);
	void DelAllSnapPoint();
	/* the view's focus put at the child, by the alignment, moved by the offset */
	void SetViewFocus(Widget *child, SNAP_ALIGN_TYPE align, const math::v2 &offset,
			  Timer *timer);
	void SetViewFocus(const IDParam &id, SNAP_ALIGN_TYPE align, const math::v2 &offset,
			  Timer *timer);
	using ScrollViewBase::SetViewFocus;
	using ScrollViewBase::AddSnapPoint;
	math::v2 GetViewFocus(int32_t opt);
	/* the axes it may move along (ScrollViewBase::MoveDir) */
	void SetMovableDirection(uint32_t dir);
	uint32_t GetMovableDirection();
	/* the view parent put back inside the room it may move in, stopped there */
	void CheckMovableRange();

	/* the axes it may move along (MoveDir) */
	uint32_t m_movable_direction;
	/* where the view parent was at the last update, and how far it moved then */
	math::v4 m_last_pos;
	math::v4 m_move;
	/* the room the view parent may move in: its left and top bound as x, y and its right
	   and bottom bound as z, w */
	math::v4 m_movable_range;
};

class SlidingDrawerParam : public ScrollViewBaseParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	SlidingDrawerParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
};

class SlidingDrawerLayoutParam : public WidgetLayoutParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	SlidingDrawerLayoutParam();
	virtual const char *GetParamName() const;
};

class SlidingDrawerStyleParam : public ScrollViewBaseStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	SlidingDrawerStyleParam();
	virtual const char *GetParamName() const;
};

class SlidingDrawerHandlerParam : public WidgetHandlerParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	SlidingDrawerHandlerParam();
	virtual const char *GetParamName() const;
};

}
}

#endif
