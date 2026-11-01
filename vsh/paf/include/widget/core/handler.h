/*
 * paf::ui::Handler: what receives events. Each kind of event has its own
 * virtual handler; DoEvent passes an event on to its listeners.
 *
 * Names after ScePaf-RE (restored from Sony's DWARF), the virtual functions in
 * its order, which is the order of Handler's vtable. Where no name is known the
 * name says what the function does (namecheck lists them).
 */
#ifndef PAF_WIDGET_CORE_HANDLER_H
#define PAF_WIDGET_CORE_HANDLER_H

#include <stddef.h>
#include <stdint.h>
#include <std/map>

namespace paf {
namespace ui {

class Event;
class EventQueue;
class EventListenerList;
class TimeoutListener;
class Handler;

typedef void (*HandlerCB)(int32_t type, Handler *self, Event *e, void *userdata);

/* what a handler calls for an event of a type it listens to */
class EventListener {
public:
	EventListener() : m_killed(false), m_autodelete(true)
	{
	}

	virtual ~EventListener()
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e) = 0;

	virtual HandlerCB GetFunc()
	{
		return NULL;
	}

	bool m_killed : 1;
	bool m_autodelete : 1;
};

/* a listener that calls a function with its data */
class EventCBListener : public EventListener {
public:
	EventCBListener(HandlerCB func, void *data) : m_data(data), m_func(func)
	{
	}

	virtual ~EventCBListener()
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		if (m_killed)
			return 0x80AF4101;	/* no name found */
		if (m_func != NULL)
			m_func(type, self, e, m_data);
		return 0;
	}

	virtual HandlerCB GetFunc()
	{
		return m_func;
	}

	void *m_data;
	HandlerCB m_func;
};

class Handler {
public:
	enum {
		CB_STATE = 0x100000,
		CB_STATE_INIT = 0x100001,
		CB_STATE_TERM = 0x100002,
		CB_STATE_PREPARE = 0x100003,
		CB_STATE_READY = 0x100004,
		CB_STATE_READY_CACHEIMAGE = 0x100005,
		CB_COMMAND = 0x100006
	};

	Handler(uint32_t enable);

	virtual ~Handler();

	virtual int32_t HandlerProc(Event *e);
	virtual int32_t HandlePointFwEvent(Event *e);
	virtual int32_t HandleTouchFwEvent(Event *e);
	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleAnalogEvent(Event *e);
	/* passes the event on as input event 0x10013 */
	virtual int32_t HandleInputEvent(Event *e);
	/* passes the event on to HandleKeycodeEvent */
	virtual int32_t HandleKeyEvent(Event *e);
	virtual int32_t HandlePointEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual int32_t HandleFocusEvent(Event *e);
	virtual int32_t HandleFocusSwitchEvent(Event *e);
	virtual int32_t HandleGrabEvent(Event *e);
	virtual int32_t HandleStateEvent(Event *e);
	virtual int32_t HandleCommandEvent(Event *e);
	virtual int32_t HandleSystemEvent(Event *e);
	virtual int32_t HandleTimerEvent(Event *e);
	virtual void EnableFocusEvent(bool on_off);
	virtual int32_t EnableTouchRearEvent(bool on_off);
	virtual bool IsTouchRearEventEnabled();
	virtual void EnableTouchFrontEvent(bool on_off);
	virtual bool IsTouchFrontEventEnabled();
	virtual int32_t DoEvent(int32_t type, Event *e);

	int32_t AddEventListener(int32_t type, EventListener *listener, bool del_prev = false);

	int32_t AddEventCallback(int32_t type, HandlerCB func, void *data = NULL)
	{
		EventCBListener *listener = new EventCBListener(func, data);

		return AddEventListener(type, listener, false);
	}

	EventQueue *m_event_queue;
	uint32_t m_event_enable;
	map<int32_t, EventListenerList *> m_map_e_listener;
	map<int32_t, TimeoutListener *> m_map_t_listener;
	int32_t reserved;
};

}
}

#endif
