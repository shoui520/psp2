/*
 * paf::ui::ScrollViewBase, as far as converted C++ uses it: the base of the widgets that scroll
 * their contents, and ScrollView. The virtual functions follow Widget's, in Sony's vtable order
 * up to SetViewFocus (+0x194, its name by NID hash); names after ScePaf-RE.
 */
#ifndef PAF_WIDGET_W_SCROLL_VIEW_BASE_H
#define PAF_WIDGET_W_SCROLL_VIEW_BASE_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>

namespace paf {

class Timer;

namespace ui {

class ScrollBar;
class ScrollViewScrollBarController;
class ScrollViewSnapPoint;

class ScrollViewBase : public Widget {
public:
	static const char *TypeName();

	virtual void SetVerticalScrollBar(ScrollBar *scroll_bar);
	virtual void SetHorizontalScrollBar(ScrollBar *scroll_bar);
	virtual void SetScrollBarController(ScrollViewScrollBarController *controller);
	virtual int32_t AddSnapPoint(const ScrollViewSnapPoint &snap_point);
	virtual void DelSnapPoint(int32_t snap_id);
	virtual void UpdateSnapPoint(int32_t snap_id, const ScrollViewSnapPoint &snap_point);
	virtual void SetSnapPower(float snap_power);
	virtual void DoSnap(int32_t snap_id, Timer *timer);
	virtual void SetPreferredSnapPoint(int32_t snap_id, float time, int32_t func);
	/* the view moved to the position, over the timer where one is given */
	virtual void SetViewFocus(const math::v2 &pos, Timer *timer, bool flag_a, bool flag_b);

	/* where the view is: its contents' position, negated */
	math::v2 GetViewFocus();

	/* its members from Widget's end (0x280), not converted yet */
	uint8_t reserved[0x2A0 - 0x280];
	/* the box the view shows (ScePafWidget_46F36554's, after ScePaf-RE's SetViewBox) */
	math::v4 m_view_box;
};

class ScrollView : public ScrollViewBase {
public:
	static const char *TypeName();
};

}
}

/* the view box set (+672), and the scroll layout updated where it changed (name ours) */
extern "C" void ScePafWidget_46F36554(paf::ui::ScrollViewBase *self,
				      const paf::math::v4 &view_box);

#endif
