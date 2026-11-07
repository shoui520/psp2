/*
 * EventDispatcher: tells its listeners of an event at once (Notify), or once a delay has passed
 * (its timers, counted down on the main thread one frame at a time). Names ours; see
 * event_dispatcher.h.
 */
#include <stddef.h>
#include <stdint.h>
#include <std/new>
#include <std/list>
#include "event_dispatcher.h"

using namespace paf;

/* a frame at 60 frames a second, in milliseconds */
#define EVENT_DISPATCHER_FRAME_MS	16.666666f

EventListener::~EventListener()
{
}

EventDispatcher::EventDispatcher() : m_registered(false)
{
}

__attribute__((noinline)) void EventDispatcher::ClearTimers()
{
	while (m_timers.size() != 0) {
		operator delete(m_timers.front().arg);
		m_timers.pop_front();
	}
}

EventDispatcher::~EventDispatcher()
{
	if (m_registered) {
		EventDispatcherUnregister(UpdateTimersCB, this);
		m_registered = false;
		ClearTimers();
	}
}

void EventDispatcher::AddListener(EventListener *listener)
{
	m_listeners.push_back(listener);
}

void EventDispatcher::RemoveListener(EventListener *listener)
{
	list<EventListener *>::iterator it;

	for (it = m_listeners.begin(); it != m_listeners.end(); ++it) {
		if (*it == listener) {
			m_listeners.erase(it);
			return;
		}
	}
}

/* to a copy of the listeners, so a listener may add or remove listeners as it hears */
void EventDispatcher::Notify(int32_t event, void *arg)
{
	list<EventListener *> listeners(m_listeners);
	list<EventListener *>::iterator it;

	for (it = listeners.begin(); it != listeners.end(); it++)
		(*it)->OnEvent(this, event, arg);
}

/* a frame has passed: the timers whose delay is over send their event and go */
void EventDispatcher::UpdateTimers()
{
	list<Timer>::iterator it = m_timers.begin();

	while (it != m_timers.end()) {
		list<Timer>::iterator next = it;

		++next;
		it->remaining -= EVENT_DISPATCHER_FRAME_MS;
		if (it->remaining < 0.0f) {
			Notify(it->event, it->arg);
			operator delete(it->arg);
			m_timers.erase(it);
		}
		it = next;
	}
}

void EventDispatcher::UpdateTimersCB(void *userdata)
{
	if (userdata != NULL)
		((EventDispatcher *)userdata)->UpdateTimers();
}
