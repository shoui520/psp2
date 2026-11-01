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

#include <stdint.h>
#include <std/map>

namespace paf {
namespace ui {

class Event;
class EventQueue;
class EventListenerList;
class TimeoutListener;

class Handler {
public:
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

	EventQueue *m_event_queue;
	uint32_t m_event_enable;
	map<int32_t, EventListenerList *> m_map_e_listener;
	map<int32_t, TimeoutListener *> m_map_t_listener;
	int32_t reserved;
};

}
}

#endif
