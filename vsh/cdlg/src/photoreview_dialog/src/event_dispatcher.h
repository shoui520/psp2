/*
 * An object that tells its listeners of its events, now or after a delay. Sony's photo apps and
 * the image dialogs (photocam, calendar, libcdlg_photoreview, libcdlg_cameraimport, ...) build the
 * same code; no binary names it, so the classes, their methods and this file's name are ours.
 */
#ifndef PHOTOREVIEW_DIALOG_EVENT_DISPATCHER_H
#define PHOTOREVIEW_DIALOG_EVENT_DISPATCHER_H

#include <stdint.h>
#include <std/list>

class EventDispatcher;

/* what hears an EventDispatcher's events */
class EventListener {
public:
	virtual ~EventListener();

	virtual void OnEvent(EventDispatcher *sender, int32_t event, void *arg) = 0;
};

class EventDispatcher {
public:
	/* an event sent once its delay has passed, with an argument the dispatcher frees */
	class Timer {
	public:
		int32_t event;
		void *arg;
		/* what is left of the delay, in milliseconds */
		float remaining;
	};

	EventDispatcher();
	virtual ~EventDispatcher();

	void ClearTimers();
	void AddListener(EventListener *listener);
	void RemoveListener(EventListener *listener);
	void Notify(int32_t event, void *arg);
	void UpdateTimers();

	/* UpdateTimers on the main thread, for its MainThreadCallList registration */
	static void UpdateTimersCB(void *userdata);

	paf::list<EventListener *> m_listeners;
	paf::list<Timer> m_timers;
	/* UpdateTimersCB is registered */
	bool m_registered;
};

/* the timers' main-thread registration ends: the photo apps' build of it (no Sony name) */
int EventDispatcherUnregister(void (*callback)(void *userdata), void *userdata);

#endif
