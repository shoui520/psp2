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
#include <inputdevice/touchscreen/touchscreen.h>

namespace paf {
namespace ui {

/* a gesture listener for one touch of one touch port (constructor E5EE88D6) */
class TouchGestureListener : public inputdevice::GestureListener {
public:
	/* the touch ID of the port, its user data the widget (5065F85A, E5EE88D6) */
	TouchGestureListener(uint32_t port, uint32_t touch_id, void *user_data);

	virtual ~TouchGestureListener()
	{
	}

	/* let go of where its user data is no longer valid */
	virtual void PreUpdate()
	{
		if (!IsValidUserData(m_user_data)) {
			m_attached_bitmap = 0;
			m_attached_counter = 0;
		}
		m_update_count = 0;
	}

	virtual int32_t OnUpdate(const inputdevice::Data *data, uint32_t port_id)
	{
		int32_t ret = OnUpdateTouch(port_id);

		m_update_count++;
		return ret;
	}

	/* what OnUpdate asks of the InputGestureListener for the touch (name ours) */
	virtual int32_t OnUpdateTouch(uint32_t port_id)
	{
		return 0;
	}

	uint32_t m_touch_id;
	/* the updates since the last PreUpdate (name ours) */
	uint32_t m_update_count;
	uint32_t m_port;
};

/* a touch gesture listener of several touches at once (vtable 812F91C8) */
class MultiTouchGestureListener : public TouchGestureListener {
public:
	virtual ~MultiTouchGestureListener();
	virtual int32_t OnUpdate(const inputdevice::Data *data, uint32_t port_id);
	/* what OnUpdate asks of the InputGestureListener for the touches of the bitmaps, now and
	   the update before (name ours; 0 here) */
	virtual int32_t OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre);
	virtual void RefreshActiveTouch(uint32_t touch_id);
};

/* how a drag gesture starts and ends (vtable 812F6CBC, 0x48 bytes; names ours, the
   distances and speeds in millimetres as the listener scales them by pixel_per_mm) */
class DragGestureParam : public inputdevice::GestureParam {
public:
	DragGestureParam() : m_move_tolerance(0.0f), m_hold_time(500.0f), m_dir(1), m_touch_count(1)
	{
		m_start_distance[0] = 0.0f;
		m_start_distance[1] = 0.0f;
		m_start_distance[2] = 0.0f;
		m_end_distance[0] = 0.0f;
		m_end_distance[1] = 0.0f;
		m_end_distance[2] = 0.0f;
		m_flick_speed = 20.0f;
		m_end_speed[0] = 0.0f;
		m_end_speed[1] = 0.0f;
	}

	/* the hold time kept within 400 to 1100 ms, the start distances at 0 or more */
	virtual void AdjustParam()
	{
		float hold_time = m_hold_time;
		float x = m_start_distance[0];
		float y = m_start_distance[1];
		float xy = m_start_distance[2];

		if (hold_time < 400.0f) {
			hold_time = 400.0f;
			m_hold_time = hold_time;
		}
		if (hold_time > 1100.0f)
			m_hold_time = 1100.0f;
		if (x < 0.0f)
			m_start_distance[0] = 0.0f;
		if (y < 0.0f)
			m_start_distance[1] = 0.0f;
		if (xy < 0.0f)
			m_start_distance[2] = 0.0f;
	}

	/* how far the touch may move while it is held, and how long it is held */
	float m_move_tolerance;
	float m_hold_time;
	math::rect m_rect;
	/* the axes it drags along: 0 X, 1 Y, 2 both */
	int32_t m_dir;
	int32_t m_touch_count;
	/* how far the touch moves along each of the axes, X, Y and both, before it drags */
	float m_start_distance[3];
	float m_end_distance[3];
	/* the least speed the drag starts at */
	float m_flick_speed;
	float m_end_speed[2];
};

/* the listener a touch is dragged with (constructor 0x81162fdc TOOL, vtable 812F95A4;
   names ours) */
class DragGestureListener : public MultiTouchGestureListener {
public:
	/* where the drag is, and how far it moved (meaning open) */
	virtual int32_t GetDragState(float *x, float *y, float *a, float *b, float *c);
	/* the parameters taken over, while it is not dragging */
	virtual void SetParam(const DragGestureParam *param);
	virtual void GetParam(DragGestureParam *param);
	/* the flick's velocity scaled by the time since it began */
	virtual bool ScaleFlick(float *x, float *y);
};

/* the flick parameters every drag gesture listener points to, made once (vtable 812F6CC8;
   names ours): m_value[3] scales a mochi gesture's velocity, the others' meaning is open */
class DragFlickParam {
public:
	DragFlickParam()
	{
		m_value[0] = 45.0f;
		m_value[1] = 45.0f;
		m_value[2] = 240.0f;
		m_value[3] = 1000.0f;
		m_value[5] = 1000.0f;
		m_value[4] = 80.0f;
	}

	virtual ~DragFlickParam()
	{
	}

	static DragFlickParam &Get()
	{
		static DragFlickParam s_param;

		return s_param;
	}

	float m_value[6];
};

extern "C" {
/* the widget's gesture listener of the type for the touch, made with the rect where it has
   none (no name found) */
common::SharedPtr<inputdevice::GestureListener> ScePafWidget_712AD1C7(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect &rect);
/* the gesture's event sent for the touch, with its velocity (no name found) */
void ScePafWidget_C5939A92(TouchGestureListener *listener, int32_t gesture, uint32_t touch_id,
			   int32_t arg, float vx, float vy, int32_t a, int32_t b);
}

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
