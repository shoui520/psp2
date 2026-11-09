/*
 * paf::ui::UIInputGestureListener: the input gesture listeners of the widget system, each with
 * the environment it queues its events to and the widget each touch is bound to; and
 * UITouchListener, the one of a touch port, which turns the port's touches into touch events.
 * The file and class names are ScePaf-RE's (its DWARF file list); the members' names are ours.
 */
#ifndef PAF_WIDGET_CORE_UIINPUTLISTENER_H
#define PAF_WIDGET_CORE_UIINPUTLISTENER_H

#include <stddef.h>
#include <stdint.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/pad/pad.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <timer/timer.h>

namespace paf {
namespace ui {

class Environment;
class Widget;

class UIInputGestureListener : public inputdevice::InputGestureListener {
public:
	enum {
		BIND_NUM = 8
	};

	UIInputGestureListener(Environment *env) : m_env(env)
	{
		for (int32_t i = 0; i < BIND_NUM; i++)
			m_binded_widget[i] = NULL;
	}

	virtual ~UIInputGestureListener()
	{
	}

	virtual void Init()
	{
		for (int32_t i = 0; i < BIND_NUM; i++)
			m_binded_widget[i] = NULL;
	}

	Environment *m_env;
	/* the widget each touch is bound to */
	Widget *m_binded_widget[BIND_NUM];
};

/* the listener of a touch port: the touches down on it, the screen's resolution and the scale
   from the touch panel's to the screen's */
class UITouchListener : public UIInputGestureListener {
public:
	/* Sony's build keeps the constructor out of line (0x8123cdb4) */
	UITouchListener(Environment *env, uint32_t port);

	virtual ~UITouchListener()
	{
	}

	virtual void Init();
	virtual void OnUpdate(const inputdevice::Data *data);

	/* the port's mask of the touch (its ID) set */
	virtual int32_t SetInputMask(uint32_t id, uint32_t mask)
	{
		return inputdevice::touchscreen::SetMask(m_port, 1 << id);
	}

	/* the touches down, a bit for each */
	uint32_t m_touch_down;
	int32_t m_screen_width;
	int32_t m_screen_height;
	float m_scale_x;
	float m_scale_y;
	/* the touch port: 0 the front, 1 the rear */
	uint32_t m_port;
	/* the port's touches (eight) in the last update, and the update before */
	const inputdevice::touchscreen::Data *m_touch;
	const inputdevice::touchscreen::Data *m_touch_pre;
};

/* the listener of the rear touch panel: the front's with the port and the device type of the
   rear (vtable 812F8ED0; its OnUpdate is the front's, called through) */
class UITouchRearListener : public UITouchListener {
public:
	UITouchRearListener(Environment *env) : UITouchListener(env, 1)
	{
		m_type = inputdevice::DEVICE_TYPE_TOUCHSCREEN_BACK;
	}

	virtual ~UITouchRearListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data)
	{
		UITouchListener::OnUpdate(data);
	}
};

/* a gesture listener of the pad's keys of one port, its user data the widget (vtable 812F9274;
   constructor inline in apply_pad_gesture_tap and apply_pad_gesture_longpress; names ours) */
class PadGestureListener : public inputdevice::GestureListener {
public:
	PadGestureListener(Widget *widget, uint32_t port) : m_port(port)
	{
		if (port >= 5)
			m_port = 4;
		SetUserData(widget);
		on_update_core();
		m_state = 0;
	}

	virtual ~PadGestureListener()
	{
	}

	virtual void Init()
	{
		on_update_core();
	}

	virtual int32_t OnUpdate(const inputdevice::Data *data, uint32_t port_id)
	{
		return on_update_core();
	}

	/* the update of the port's keys now and the update before; the bits of
	   GestureListener::OnUpdate's result */
	virtual int32_t OnPadUpdate(const inputdevice::pad::Data *curr,
				    const inputdevice::pad::Data *prev)
	{
		return 1;
	}

	/* the current port taken, its keys given to OnPadUpdate (0x81149fc4) */
	int32_t on_update_core();
	/* the gesture's event sent to the widget: the gesture and the key, two numbers more
	   (0x8114a0b4) */
	int32_t PostGestureEvent(uint32_t gesture, uint32_t key, int32_t arg1, int32_t arg2);

	uint32_t m_port;
	/* where the gesture is (0 waiting for a key) */
	uint32_t m_state;
};

/* how long a pad tap takes and how long it waits for the next (milliseconds), and whether the
   taps are counted at the press (vtable 812F9298; names ours) */
class PadTapParam : public inputdevice::GestureParam {
public:
	PadTapParam() : m_press_time(500), m_gap_time(200), m_on_press(true)
	{
	}

	/* the press time kept within 400 to 1100, the gap within 200 to 400 */
	virtual void AdjustParam()
	{
		uint32_t press = m_press_time;
		uint32_t gap = m_gap_time;

		if (press >= 400) {
			if (press > 1100)
				m_press_time = 1100;
		} else {
			m_press_time = 400;
		}
		if (gap >= 200) {
			if (gap > 400)
				m_gap_time = 400;
		} else {
			m_gap_time = 200;
		}
	}

	uint32_t m_press_time;
	uint32_t m_gap_time;
	bool m_on_press;
};

/* the pad key tapped once or twice (vtable 812F92AC) */
class PadTapListener : public PadGestureListener {
public:
	PadTapListener(Widget *widget, uint32_t port)
		: PadGestureListener(widget, port), m_time(get_time_internal()), m_count(0),
		  m_target(1), m_pending(0), m_key(0)
	{
	}

	virtual void SetGestureParam(const inputdevice::GestureParam *param)
	{
		const PadTapParam *tap = static_cast<const PadTapParam *>(param);

		if (tap != NULL) {
			m_param.m_press_time = tap->m_press_time;
			m_param.m_gap_time = tap->m_gap_time;
			m_param.m_on_press = tap->m_on_press;
		}
	}

	virtual int32_t OnPadUpdate(const inputdevice::pad::Data *curr,
				    const inputdevice::pad::Data *prev);

	uint64_t m_time;
	/* the taps so far, and how many make the gesture */
	int32_t m_count;
	int32_t m_target;
	int32_t m_pending;
	/* the key pressed */
	uint32_t m_key;
	PadTapParam m_param;
};

/* how long a pad key is held and how long it repeats (milliseconds; vtable 812F92D0) */
class PadLongPressParam : public inputdevice::GestureParam {
public:
	PadLongPressParam() : m_hold_time(500), m_repeat_time(5), m_result(116)
	{
	}

	/* the hold time kept within 400 to 1100 */
	virtual void AdjustParam()
	{
		uint32_t hold = m_hold_time;

		if (hold >= 400) {
			if (hold > 1100)
				m_hold_time = 1100;
		} else {
			m_hold_time = 400;
		}
	}

	uint32_t m_hold_time;
	uint32_t m_repeat_time;
	/* what OnPadUpdate gives where it does not repeat */
	uint32_t m_result;
};

/* the pad key held (vtable 812F92E4) */
class PadLongPressListener : public PadGestureListener {
public:
	PadLongPressListener(Widget *widget, uint32_t port)
		: PadGestureListener(widget, port), m_time(get_time_internal()), m_key(0),
		  m_repeat(false)
	{
	}

	/* the key let go of after a hold ends it */
	virtual ~PadLongPressListener()
	{
		if (m_state == 2)
			PostGestureEvent(6, m_key, 0, 0);
	}

	virtual void SetGestureParam(const inputdevice::GestureParam *param)
	{
		const PadLongPressParam *hold = static_cast<const PadLongPressParam *>(param);

		if (hold != NULL) {
			m_param.m_hold_time = hold->m_hold_time;
			m_param.m_repeat_time = hold->m_repeat_time;
			m_param.m_result = hold->m_result;
		}
	}

	virtual int32_t OnPadUpdate(const inputdevice::pad::Data *curr,
				    const inputdevice::pad::Data *prev);

	uint64_t m_time;
	uint32_t m_key;
	/* the hold repeats (the widget asks for it with the long press type 8) */
	bool m_repeat;
	PadLongPressParam m_param;
};


/* the listener of the pad: its keys turned into key events, one key held at a time, repeating
   (vtable 812F9300; the fields' names ours) */
/* the update of the pad keys the UIPadListener keeps for each port, cleared by its constructor
   (uiinputlistener.cpp; names ours) */
extern inputdevice::pad::Data s_pad_curr[8];
extern inputdevice::pad::Data s_pad_prev[8];

class UIPadListener : public UIInputGestureListener {
public:
	UIPadListener(Environment *env)
		: UIInputGestureListener(env), m_repeat_delay(300.0f), m_repeat_interval(80.0f),
		  m_repeat_count(0), m_hold_key(0), m_analog_to_dpad(false), m_prev_mask(0), m_mode(0)
	{
		sce_paf_memset(s_pad_curr, 0, sizeof(s_pad_curr));
		sce_paf_memset(s_pad_prev, 0, sizeof(s_pad_prev));
		m_type = inputdevice::DEVICE_TYPE_UI_PAD;
	}

	virtual ~UIPadListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);

	/* the stick moves the cursor keys (0) or not (1) */
	virtual void SetMode(int32_t mode, int32_t option)
	{
		if (mode == 0)
			m_analog_to_dpad = true;
		else if (mode == 1)
			m_analog_to_dpad = false;
	}

	virtual int32_t SetInputMask(uint32_t port, uint32_t mask)
	{
		inputdevice::pad::SetMask(mask);
		return 0;
	}

	/* the key event of the type (17 down, 18 up, 19 repeat, 20 held) for the key, queued
	   (0x8114aa2a); -1 without an environment or a key code */
	virtual int32_t PostPadEvent(uint32_t type, uint32_t port, uint32_t key, bool flag,
				     int32_t mode);
	/* the keys of the update made into the masks the events come from: opposite directions
	   cancelled, the stick and a keyboard's keys added (0x8114aace) */
	virtual void UpdatePad(uint32_t *prev, uint32_t *curr, int32_t *port, int32_t *mode,
			       const inputdevice::Data *data);

	/* the time to the first repeat and between repeats, in milliseconds, and the repeats so
	   far */
	float m_repeat_delay;
	float m_repeat_interval;
	uint32_t m_repeat_count;
	/* the key held, and the keys of the last update */
	uint32_t m_hold_key;
	bool m_analog_to_dpad;
	uint32_t m_prev_mask;
	int32_t m_mode;
};

/* the pad listener that holds all the keys pressed (vtable at 812F9318; name ours) */
class UIPadHoldListener : public UIPadListener {
public:
	UIPadHoldListener(Environment *env) : UIPadListener(env)
	{
	}

	virtual ~UIPadHoldListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);
	/* the events of every key in the mask (0xdc5e436a) */
	virtual int32_t PostPadEvent(uint32_t type, uint32_t port, uint32_t key, bool flag,
				     int32_t mode);
};

/* the listener of the Bluetooth remote: its keys turned into events of the widget it is given or
   the scene (vtable at 812F93BC; the fields' names ours) */
class UIBtrmListener : public UIInputGestureListener {
public:
	UIBtrmListener(Environment *env) : UIInputGestureListener(env), m_hold_key(0), m_target(NULL)
	{
		m_type = inputdevice::DEVICE_TYPE_BTRM;
	}

	virtual ~UIBtrmListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);

	/* the event of the key (the remote's keys 1 to 16 are four directions and enter, 32 and 64
	   have three each) with the state (0 down, 1 up, 2 held up), queued (0x8114b582) */
	int32_t PostBtrmEvent(uint32_t key, int32_t state);

	/* the key held, and the widget the events go to (NULL: dispatched) */
	uint32_t m_hold_key;
	Widget *m_target;
};

/* the listener of the HID remote: its key turned into an event of the widget it is given or the
   scene (vtable at 812F93E8; name and fields ours, as UIBtrmListener's) */
class UIHidrmListener : public UIInputGestureListener {
public:
	UIHidrmListener(Environment *env) : UIInputGestureListener(env), m_hold_key(0), m_target(NULL)
	{
		m_type = inputdevice::DEVICE_TYPE_HIDRM;
	}

	virtual ~UIHidrmListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);

	/* the event of the remote's key (1 to 49) with the state (0 down, 1 up, 2 held up), queued
	   (0x8114b75c); -1 without an environment or for a key or state it has no code for */
	int32_t PostHidrmEvent(uint32_t key, int32_t state);

	uint32_t m_hold_key;
	Widget *m_target;
};

/* the listeners of the motion sensor and the analog sticks, which take no input of their own */
class UIMotionListener : public UIInputGestureListener {
public:
	UIMotionListener(Environment *env) : UIInputGestureListener(env)
	{
		m_type = inputdevice::DEVICE_TYPE_MOTIONSENSOR;
	}

	virtual ~UIMotionListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);
};

class UIAnalogListener : public UIInputGestureListener {
public:
	UIAnalogListener(Environment *env) : UIInputGestureListener(env)
	{
	}

	virtual ~UIAnalogListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);
};

}
}

#endif /* PAF_WIDGET_CORE_UIINPUTLISTENER_H */
