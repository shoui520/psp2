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
#include <std/list>
#include <std/map>
#include <timer/timerlistener.h>

namespace paf {

/* a map's iterator by Sony's name, as delete_timer_listener takes it (the node of the map) */
template<class _Kty, class _Ty, class _Pr, class _Ax>
class _map_iterator {
public:
	_map_iterator(typename map<_Kty, _Ty, _Pr, _Ax>::iterator it) : m_it(it)
	{
	}

	typename map<_Kty, _Ty, _Pr, _Ax>::iterator m_it;
};

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

/* the listeners of an event type, called in order; the ones killed while it runs go when it is
   done (names after ScePaf-RE) */
class EventListenerList {
public:
	EventListenerList() : m_killed(false), m_on_exec(0)
	{
	}

	virtual ~EventListenerList()
	{
		m_on_exec = 0;
		DelAll();
		for (list<EventListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it)
			delete *it;
	}

	/* the killed listeners deleted: whether the list is empty (0x81144c50) */
	bool UpdateKill();
	/* all of them killed, or deleted when none is running (0x81144bd4) */
	int32_t DelAll();
	/* each called with the type, the handler and the event */
	int32_t Do(int32_t type, Handler *self, Event *e);
	/* the listener when it is in the list, else NULL (0x81144e70) */
	EventListener *Find(EventListener *listener);
	/* the first listener of the function (0x81144e70) */
	EventListener *Find(HandlerCB func);
	/* put at the end, or at the front, unless it is in the list: -1 when it is */
	int32_t Add(EventListener *listener);
	int32_t AddFront(EventListener *listener);
	/* killed while the list runs, else deleted */
	int32_t Del(EventListener *listener);

	bool IsOnExec()
	{
		return m_on_exec != 0;
	}

	uint32_t Size()
	{
		return m_list.size();
	}

	list<EventListener *> m_list;
	bool m_killed;
	int32_t m_on_exec;
};

/* the timer of a timeout or interval event: the event of its id put in the event queue when it
   ends (names after ScePaf-RE) */
class TimeoutListener : public TimerListener {
public:
	TimeoutListener(Timer *t, int32_t id) : TimerListener(t, NULL), m_id(id)
	{
	}

	virtual ~TimeoutListener()
	{
	}

	virtual void OnFinish(void *data1, void *data2);

	int32_t m_id;
};

class Handler {
public:
	/* the timer IDs of SetTimeoutEvent and SetIntervalEvent (names after ScePaf-RE) */
	enum {
		CB_TIMEOUT = 0x1000000,
		CB_TIMEOUT_END = 0x100FFFF,
		CB_INTERVAL = 0x2000000,
		CB_INTERVAL_END = 0x200FFFF
	};

	enum {
		CB_STATE = 0x100000,
		CB_STATE_INIT = 0x100001,
		CB_STATE_TERM = 0x100002,
		CB_STATE_PREPARE = 0x100003,
		CB_STATE_READY = 0x100004,
		CB_STATE_READY_CACHEIMAGE = 0x100005,
		CB_COMMAND = 0x100006
	};

	/* what a modal widget is sent for a touch outside it (HandleStateEvent's for the state
	   events 0x1009 and 0x100a; names ours) */
	enum {
		CB_MODAL_TOUCH_OUTSIDE = 0x20019,
		CB_MODAL_RELEASE_OUTSIDE = 0x2001a
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
	virtual void EnableFocusEvent(bool on_off)
	{
		if (on_off)
			m_event_enable |= 0x400;
		else
			m_event_enable &= ~0x400;
	}

	virtual int32_t EnableTouchRearEvent(bool on_off)
	{
	}

	virtual bool IsTouchRearEventEnabled()
	{
		return false;
	}

	virtual void EnableTouchFrontEvent(bool on_off)
	{
	}

	virtual bool IsTouchFrontEventEnabled()
	{
		return true;
	}

	virtual int32_t DoEvent(int32_t type, Event *e);

	/* the callbacks of a point and a touch event, which forward events pass on to their listeners
	   with 0x1000 added */
	int32_t HandlePointEvent_core(Event *e, bool is_forward);
	int32_t HandleTouchEvent_core(Event *e, bool is_forward);
	/* the timers of the handler run (0x81145c48) */
	void UpdateTimer();
	/* the timer listener deleted, and taken out of the map (0x81144f20) */
	int32_t delete_timer_listener(_map_iterator<int32_t, TimeoutListener *, less<int32_t>,
						    allocator<TimeoutListener *> > it,
				      bool map_erase);

	int32_t AddEventListener(int32_t type, EventListener *listener, bool del_prev = false);
	/* the listener taken off the event type (export AC0AC756) */
	int32_t DeleteEventListener(int32_t type, EventListener *listener);
	int32_t SetTimerListener(int32_t id, float timeout, EventListener *listener);
	int32_t KillTimer(int32_t id);

	int32_t AddEventCallback(int32_t type, HandlerCB func, void *data = NULL)
	{
		EventCBListener *listener = new EventCBListener(func, data);

		return AddEventListener(type, listener, false);
	}

	int32_t DeleteEventCallback(int32_t type, HandlerCB func, void *data);

	int32_t SetTimeoutEvent(int32_t id, float timeout, HandlerCB func, void *data = NULL)
	{
		return SetTimerListener(CB_TIMEOUT | (id & 0xFFFF), timeout,
					new EventCBListener(func, data));
	}

	int32_t KillTimeoutEvent(int32_t id)
	{
		return KillTimer(CB_TIMEOUT | (id & 0xFFFF));
	}

	int32_t SetIntervalEvent(int32_t id, float interval, HandlerCB func, void *data = NULL)
	{
		return SetTimerListener(CB_INTERVAL | (id & 0xFFFF), interval,
					new EventCBListener(func, data));
	}

	int32_t KillIntervalEvent(int32_t id)
	{
		return KillTimer(CB_INTERVAL | (id & 0xFFFF));
	}

	EventQueue *m_event_queue;
	uint32_t m_event_enable;
	map<int32_t, EventListenerList *> m_map_e_listener;
	map<int32_t, TimeoutListener *> m_map_t_listener;
	/* zero from the constructor; a text with it above zero is not scrolled with its friends
	   (TextAutoScroll, name ours) */
	int32_t m_hold_count;
};

}
}

#endif
