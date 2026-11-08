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
#include <inputdevice/touchscreen/touchscreen.h>

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
	UITouchListener(Environment *env, uint32_t port) : UIInputGestureListener(env), m_port(port)
	{
		m_type = inputdevice::DEVICE_TYPE_TOUCHSCREEN_FRONT;
		Init();
	}

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

}
}

#endif /* PAF_WIDGET_CORE_UIINPUTLISTENER_H */
