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
	__attribute__((noinline)) TouchGestureListener(uint32_t port, uint32_t touch_id, void *user_data);

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
	/* the listeners built on it set these fields in their own constructors (inline in
	   Sony's; name ours) */
	MultiTouchGestureListener(uint32_t port, uint32_t touch_id, void *user_data,
				  uint32_t touch_count_max)
		: TouchGestureListener(port, touch_id, user_data), m_touch_bitmap(0),
		  m_touch_count_max(touch_count_max), m_last_result(1)
	{
	}

	virtual ~MultiTouchGestureListener()
	{
	}

	virtual int32_t OnUpdate(const inputdevice::Data *data, uint32_t port_id);

	/* what OnUpdate asks of the InputGestureListener for the touches of the bitmaps, now and
	   the update before, and how many touches more it holds than the update before (name ours;
	   0 here) */
	virtual int32_t OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre, int32_t diff)
	{
		return 0;
	}

	/* the touch IDs of the bitmap it holds, at most m_touch_count_max of them, and how many
	   (0x8114931e TOOL; name ours) */
	virtual int32_t RefreshActiveTouch(uint32_t bitmap);

	/* the touch IDs it holds, a bit each */
	uint32_t m_touch_bitmap;
	/* the most touches it takes (8; meaning open) */
	uint32_t m_touch_count_max;
	/* what OnUpdateMultiTouch asked of the InputGestureListener last (name ours) */
	int32_t m_last_result;
};

/* the listener of gesture 20, two touches pinched (constructor 0x81164820 TOOL, vtable
   812F960C; names ours, the meaning of its positions open) */
class PinchGestureListener : public MultiTouchGestureListener {
public:
	/* the front touch panel's pixels per millimetre */
	float m_pixel_per_mm;
	/* the pinch's two positions, as its state gives them; false and 0 where it has none */
	bool GetPinchPos(float *x0, float *y0, float *x1, float *y1);

	/* what the pinch does (0 to 3) */
	int32_t m_pinch_state;
	float m_pos[8];
	uint8_t reserved[8];
	/* the touch IDs it pinches with, a bit each (ScrollViewBase's rear pinch gesture) */
	uint32_t m_pinch_touches;
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

	uint8_t reserved[0x120 - sizeof(MultiTouchGestureListener)];
	/* the touch IDs it drags with, a bit each (ScrollViewBase's rear mochi gesture; name
	   ours) */
	uint32_t m_drag_touches;
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
/* the widget's gesture listeners of the types 128, 16 and 2 for the touch, made where it has
   none, within the rect where one is given (no names found) */
common::SharedPtr<inputdevice::GestureListener> ScePafWidget_DE7EC3CF(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect);
common::SharedPtr<inputdevice::GestureListener> ScePafWidget_F4625FAD(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect);
common::SharedPtr<inputdevice::GestureListener> ScePafWidget_5E7757AA(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect);
/* the widget's gesture listeners of the types 4 and 8, and 32, for the touch (no names found) */
common::SharedPtr<inputdevice::GestureListener> ScePafWidget_4607555F(Widget *widget,
	uint32_t port, uint32_t touch_id, uint32_t type, const math::rect *rect);
common::SharedPtr<inputdevice::GestureListener> ScePafWidget_1975BA26(Widget *widget,
	uint32_t port, uint32_t touch_id);
/* the widget's gesture listener of the type on a rear touch, the first touch ID that has one
   (no name found) */
common::SharedPtr<inputdevice::GestureListener> ScePafWidget_0F9F14D4(Widget *widget,
								       int32_t type);
/* the gesture's event sent for the touch: the gesture, the four numbers of the event (floats'
   bits, as the callers give them), put in front of the queue if front, with the bit 0x8000 of its
   detail set if flag; 0, or -1 where there is no widget to send it to (no name found) */
int32_t ScePafWidget_C5939A92(TouchGestureListener *listener, int32_t gesture, uint32_t touch_id,
			   int32_t arg, int32_t data2, int32_t data3, int32_t front, int32_t flag);
}

/* how long a touch is held before it counts as a press, in milliseconds: 500, or 300 where
   the widget asks for the short hold, and the area it is held in (vtable 812F12D0) */
class HoldGestureParam : public inputdevice::GestureParam {
public:
	HoldGestureParam() : m_move_tolerance(3.0f), m_hold_time(500), m_repeat_time(5), m_hold_result(112)
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
	/* what the listener answers once the hold is sent, without the short hold (the update bits
	   of GestureListener: 112 detaches the others; name ours) */
	uint32_t m_hold_result;
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

/* the listener of a touch that taps or moves little: it counts the taps and sends gestures 0
   (a tap), 2 (taps), 1 and 25 (constructor 0x811487a0 TOOL, vtable 812F9180, 0x80 bytes;
   names ours) */
class MoveGestureListener : public TouchGestureListener {
public:
	MoveGestureListener(uint32_t port, uint32_t touch_id, void *user_data);

	virtual ~MoveGestureListener()
	{
	}

	virtual void SetGestureParam(const inputdevice::GestureParam *param)
	{
		const MoveGestureParam *move = (const MoveGestureParam *)param;

		if (move != NULL) {
			m_param.m_move_tolerance = move->m_move_tolerance;
			m_param.m_hold_time = move->m_hold_time;
			m_param.m_move_scale = move->m_move_scale;
			m_param.m_interval_a = move->m_interval_a;
			m_param.m_interval_b = move->m_interval_b;
			m_param.m_rect = move->m_rect;
		}
	}

	virtual int32_t OnUpdateTouch(uint32_t id);

	/* the touch moved from where it began by more than the tolerance, or is out of the rect
	   (0x8114880c; name ours) */
	bool IsMoved(int32_t x, int32_t y);

	/* 0 until the touch is down, 1 while it is held, 2 after a release */
	uint32_t m_state;
	/* when the state began */
	uint64_t m_start_time;
	/* the touch in the last update */
	const inputdevice::touchscreen::Data *m_touch;
	uint32_t reserved60;
	/* the taps counted */
	uint32_t m_tap_count;
	/* the taps the gesture waits for (meaning open) */
	uint32_t m_tap_target;
	/* the taps counted when the last one was released */
	uint32_t m_last_tap_count;
	/* where the touch began */
	int32_t m_start_x;
	int32_t m_start_y;
	MoveGestureParam m_param;
	/* the hold has been sent (gesture 25) */
	bool m_hold_sent;
};

/* how far a touch may move before it is no tap: kept within 0 and 3.3 (vtable 812F91F4; names
   ours) */
class TouchTapParam : public inputdevice::GestureParam {
public:
	TouchTapParam() : m_move_tolerance(2.3f)
	{
	}

	virtual void AdjustParam()
	{
		float tolerance = m_move_tolerance;

		if (tolerance < 0.0f) {
			tolerance = 0.0f;
			m_move_tolerance = tolerance;
		}
		if (tolerance > 3.3f)
			m_move_tolerance = 3.3f;
	}

	float m_move_tolerance;
};

/* the listener a widget taps with (vtable 812F9208, 0x50 bytes): its touch down sends gesture 13,
   a move 14 and 17, the release 15, a touch another listener took 18 (names ours) */
class TouchTapListener : public MultiTouchGestureListener {
public:
	TouchTapListener(uint32_t port, uint32_t touch_id, void *user_data);

	virtual ~TouchTapListener();

	virtual void SetGestureParam(const inputdevice::GestureParam *param)
	{
		const TouchTapParam *tap = (const TouchTapParam *)param;

		if (tap != NULL)
			m_param.m_move_tolerance = tap->m_move_tolerance;
	}

	virtual int32_t OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre, int32_t diff);

	/* 0 until the touch is down, 1 while it waits to move, 2 once it moved, 3 once another
	   listener took it */
	int32_t m_state;
	/* where the touch was pressed */
	int32_t m_x;
	int32_t m_y;
	bool reserved68;
	/* the touch is another listener's */
	bool m_other;
	TouchTapParam m_param;
};

/* how far a touch may move before it is no long press: kept within 2.0 and 3.3 (vtable
   812F9234; names ours) */
class TouchLongPressParam : public inputdevice::GestureParam {
public:
	TouchLongPressParam() : m_move_tolerance(2.3f)
	{
	}

	virtual void AdjustParam()
	{
		float tolerance = m_move_tolerance;

		if (tolerance < 2.0f) {
			tolerance = 2.0f;
			m_move_tolerance = tolerance;
		}
		if (tolerance > 3.3f)
			m_move_tolerance = 3.3f;
	}

	float m_move_tolerance;
};

/* the listener two touches of a widget make together (vtable 812F9248, 0x60 bytes): gestures 19
   to 24 (names ours) */
class TouchLongPressListener : public MultiTouchGestureListener {
public:
	TouchLongPressListener(uint32_t port, uint32_t touch_id, void *user_data);

	virtual ~TouchLongPressListener();

	virtual void SetGestureParam(const inputdevice::GestureParam *param)
	{
		const TouchLongPressParam *press = (const TouchLongPressParam *)param;

		if (press != NULL)
			m_param.m_move_tolerance = press->m_move_tolerance;
	}

	virtual int32_t OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre, int32_t diff);

	/* as the tap's */
	int32_t m_state;
	/* the touch IDs of the two touches and where each was pressed */
	int32_t m_id0;
	int32_t m_id1;
	int32_t m_x0;
	int32_t m_y0;
	int32_t m_x1;
	int32_t m_y1;
	bool reserved84;
	TouchLongPressParam m_param;
};

/* the listener a widget holds a touch with (constructor 0x81148d20 TOOL, vtable 812F91A4, 0x78
   bytes): its hold sends gesture 4, a held touch repeats 5, its release 6 */
class HoldGestureListener : public TouchGestureListener {
public:
	HoldGestureListener(uint32_t port, uint32_t touch_id, void *user_data);

	virtual ~HoldGestureListener();

	virtual void SetGestureParam(const inputdevice::GestureParam *param)
	{
		const HoldGestureParam *hold = (const HoldGestureParam *)param;

		if (hold != NULL) {
			m_param.m_hold_time = hold->m_hold_time;
			m_param.m_move_tolerance = hold->m_move_tolerance;
			m_param.m_repeat_time = hold->m_repeat_time;
			m_param.m_hold_result = hold->m_hold_result;
			m_param.m_rect = hold->m_rect;
		}
	}

	virtual int32_t OnUpdateTouch(uint32_t id);

	/* 0 until the touch is down, 1 while it is held, 2 once the hold was sent */
	uint32_t m_state;
	/* the timestamp of the touch when it was pressed */
	uint64_t m_press_time;
	/* when the state began */
	uint64_t m_create_time;
	/* the touch in the last update */
	const inputdevice::touchscreen::Data *m_touch;
	uint32_t reserved68;
	/* where the touch was pressed */
	int32_t m_x;
	int32_t m_y;
	/* the short hold (300 ms) asked for: the hold then goes on to repeat */
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
