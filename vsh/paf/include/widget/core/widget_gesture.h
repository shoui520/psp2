/*
 * The gesture listeners a widget attaches to the Environment's touch gesture listeners
 * (ScePafWidget 4607555F makes them): one per touch port and touch ID, its user data the widget.
 * No Sony name was found for the classes or their fields; the names are ours, and the fields
 * only their constructors write are reserved until those are C.
 */
#ifndef PAF_WIDGET_CORE_WIDGET_GESTURE_H
#define PAF_WIDGET_CORE_WIDGET_GESTURE_H

#include <stdint.h>
#include <math/rect.h>
#include <inputdevice/inputlistener.h>

namespace paf {
namespace ui {

/* a gesture listener for one touch of one touch port (constructor E5EE88D6) */
class TouchGestureListener : public inputdevice::GestureListener {
public:
	uint32_t m_touch_id;
	uint32_t reserved1;
	uint32_t m_port;
};

/* how long a touch is held before it counts as a press, in milliseconds: 500, or 300 where
   the widget asks for the short hold, and the area it is held in (vtable 812F12D0) */
class HoldGestureParam : public inputdevice::GestureParam {
public:
	HoldGestureParam() : m_move_tolerance(3.0f), m_hold_time(500), m_repeat_time(5), reserved3(112)
	{
	}

	/* the tolerance kept within 2.0 to 3.3, the hold time within 400 to 1100 ms */
	virtual void AdjustParam()
	{
		float tolerance = m_move_tolerance;
		uint32_t hold_time = m_hold_time;

		if (tolerance < 2.0f) {
			tolerance = 2.0f;
			m_move_tolerance = tolerance;
		}
		if (tolerance > 3.3f)
			m_move_tolerance = 3.3f;
		if (hold_time < 400)
			m_hold_time = 400;
		else if (hold_time > 1100)
			m_hold_time = 1100;
	}

	/* how far the touch may move while it is held */
	float m_move_tolerance;
	uint32_t m_hold_time;
	/* how often a held touch repeats, in milliseconds (ButtonBase's repeat_msec) */
	uint32_t m_repeat_time;
	uint32_t reserved3;
	math::rect m_rect;
};

/* the parameters AppIconBase gives a touch's gesture listener of priority 0xA while it is
   dragged (vtable 812F12BC) */
class MoveGestureParam : public inputdevice::GestureParam {
public:
	MoveGestureParam()
		: m_move_tolerance(2.3f), m_hold_time(500), m_move_scale(4.0f), m_interval_a(200),
		  m_interval_b(200)
	{
	}

	/* each kept within its range; m_interval_a's out-of-range values are Sony's (200 below
	   400, 400 above 1100) */
	virtual void AdjustParam()
	{
		float tolerance = m_move_tolerance;
		uint32_t hold_time = m_hold_time;
		float scale = m_move_scale;
		uint32_t interval_a = m_interval_a;
		uint32_t interval_b = m_interval_b;

		if (tolerance < 2.0f) {
			tolerance = 2.0f;
			m_move_tolerance = tolerance;
		}
		if (tolerance > 3.3f)
			m_move_tolerance = 3.3f;
		if (hold_time < 400)
			m_hold_time = 400;
		else if (hold_time > 1100)
			m_hold_time = 1100;
		if (scale < 4.0f) {
			scale = 4.0f;
			m_move_scale = scale;
		}
		if (scale > 10.0f)
			m_move_scale = 10.0f;
		if (interval_b < 200)
			m_interval_b = 200;
		else if (interval_b > 400)
			m_interval_b = 400;
		if (interval_a < 400)
			m_interval_a = 200;
		else if (interval_a > 1100)
			m_interval_a = 400;
	}

	/* how far the touch may move, the hold time, and the move's scale and intervals as
	   AdjustParam keeps them (names ours) */
	float m_move_tolerance;
	uint32_t m_hold_time;
	float m_move_scale;
	uint32_t m_interval_a;
	uint32_t m_interval_b;
	math::rect m_rect;
};

/* the listener a widget holds a touch with (constructor 911DF76B, 0x78 bytes) */
class HoldGestureListener : public TouchGestureListener {
public:
	uint32_t reserved2;
	uint8_t reserved3[8];
	/* when it was made */
	uint64_t m_create_time;
	uint32_t reserved4[4];
	/* the short hold (300 ms) asked for */
	bool m_short_hold;
	HoldGestureParam m_param;
};

/* the listener a widget holds a pad key with, its hold and repeat as a touch's (the
   Environment's pad gesture listener gives it; layout from ButtonBase's uses, names ours) */
class PadHoldGestureListener : public inputdevice::GestureListener {
public:
	uint8_t reserved[0x14];
	HoldGestureParam m_param;
};

}
}

#endif
