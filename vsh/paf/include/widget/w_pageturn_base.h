/*
 * paf::ui::PageTurnBase: a page whose top right corner the touch or the keys drag: the drag
 * starts in the corner, follows the touch and ends turned over, back or cancelled, each step
 * sent as an event; and its style parameter. The class, its constructor, TypeName and the
 * style parameter's names are Sony's (the mangled exports and "pageturn_base",
 * "style_pageturn_base"); the members, the event and state names and the functions the
 * exports name only by NID are ours.
 */
#ifndef PAF_WIDGET_W_PAGETURN_BASE_H
#define PAF_WIDGET_W_PAGETURN_BASE_H

#include <stdint.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <widget/core/event.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class PageTurnBase : public Widget {
public:
	/* the events a drag sends (names ours) */
	enum {
		/* the touch or the key took the corner */
		CB_PAGETURN_BEGIN = 0x10000001,
		/* the corner moved; m_data[3] the turn so far, in percent of m_turn_length */
		CB_PAGETURN_MOVE = 0x10000002,
		/* dragged further than m_turn_length: the page is turned */
		CB_PAGETURN_TURNED = 0x10000003,
		/* dragged back past the corner, sent only where a listener takes it */
		CB_PAGETURN_RETURN = 0x10000004,
		/* let go short of a turn */
		CB_PAGETURN_CANCEL = 0x10000005
	};

	/* what the corner does (names ours) */
	enum {
		ST_PAGETURN_IDLE = 0,
		/* it takes no drag */
		ST_PAGETURN_DISABLE = 1,
		ST_PAGETURN_TOUCH = 2,
		ST_PAGETURN_KEY = 3
	};

	static const char m_widget_type[];
	static const char *TypeName();

	PageTurnBase(Widget *parent, CreateParam *param);
	virtual ~PageTurnBase();

	/* the key events drag the corner: 0x11 starts, 0x13 moves, 0x12 lets go and 0x14 cancels */
	virtual int32_t HandleKeycodeEvent(Event *e);
	/* a touch in the corner starts the drag, its moves follow it and its release ends it */
	virtual int32_t HandleTouchEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* 3 (ready) for states 0, 1 and 3 while its animator, its children or its draw objects still
	   prepare, else 0 */
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	/* the corner follows the touch, or goes back to it after a key let go */
	virtual void UpdateAfterChildren();
	/* the touch it held taken away: the drag cancelled */
	virtual void OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg);
	/* the state set, and returned as it is now: the touch's gesture reset for a drag, which
	   starts only from Widget's state 0 (no name found) */
	virtual int32_t ScePafWidget_A4245073(int32_t state);

	/* where the touch with the ID hits the page's plane, in its space: 0, or -1 (no name found) */
	int32_t ScePafWidget_85CB1F24(uint32_t id, math::v4 *pos);
	/* its touches let go, the drag enabled again (no name found) */
	int32_t ScePafWidget_D321F831();
	/* its touches let go, the drag disabled (no name found) */
	int32_t ScePafWidget_1FE12722();

	int32_t m_turn_state;
	/* how far from the right and the top edge a touch takes the corner */
	float m_corner_width;
	float m_corner_height;
	/* how close to the corner itself a touch may not start the drag */
	float m_corner_margin;
	/* how far a drag turns the page */
	float m_turn_length;
	/* written by the constructor only, -100.0 */
	float m_unused_offset_x;
	float m_unused_offset_y;
	/* the corner's position now, and where the drag started */
	math::v2 m_current;
	math::v2 m_start;
	/* the touch that drags it, -1 none */
	int16_t m_touch_id;
	/* written by the constructor only, -1 and 0 */
	int16_t m_unused_id;
	int16_t m_unused_count;
	bool reserved_flag : 1;
	/* a listener takes CB_PAGETURN_RETURN */
	bool m_return_listened : 1;
};

class PageTurnBaseStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	virtual const char *GetParamName() const;
};

}
}

#endif
