/*
 * paf::ui::EventListenerList, TimeoutListener and Handler (TOOL 0x81144bd4..0x81146010): what
 * receives events, and the listeners and timers of its event types. No record or string names the
 * file.
 */
#include <stddef.h>
#include <stdint.h>
#include <timer/timer.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>

namespace paf {
namespace ui {

int32_t EventListenerList::DelAll()
{
	if (m_on_exec != 0) {
		for (list<EventListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
			if (*it != NULL)
				(*it)->m_killed = true;
		}
	} else if (m_list.size() != 0) {
		do {
			list<EventListener *>::iterator it = m_list.begin();

			if ((*it) != NULL && (*it)->m_autodelete)
				delete *it;
			m_list.erase(it);
		} while (m_list.size() != 0);
	}
	return 0;
}

bool EventListenerList::UpdateKill()
{
	list<EventListener *>::iterator it = m_list.begin();

	while (it != m_list.end()) {
		EventListener *listener = *it;

		if (listener != NULL) {
			if (!listener->m_killed) {
				++it;
				continue;
			}
			delete listener;
		}
		it = m_list.erase(it);
	}
	return m_list.size() == 0;
}

int32_t EventListenerList::Do(int32_t type, Handler *self, Event *e)
{
	m_on_exec = 1;
	for (list<EventListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
		if (*it != NULL)
			(*it)->Do(type, self, e);
	}
	m_on_exec = 0;
	UpdateKill();
	return 0;
}

int32_t EventListenerList::Add(EventListener *listener)
{
	if (Find(listener) == NULL || listener == NULL) {
		m_list.push_back(listener);
		return 0;
	}
	return -1;
}

int32_t EventListenerList::AddFront(EventListener *listener)
{
	if (Find(listener) == NULL || listener == NULL) {
		m_list.push_front(listener);
		return 0;
	}
	return -1;
}

int32_t EventListenerList::Del(EventListener *listener)
{
	if (listener == NULL)
		return -1;
	if (m_on_exec != 0) {
		listener->m_killed = true;
		return 0;
	}
	for (list<EventListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
		if (*it == listener) {
			if (listener->m_autodelete)
				delete listener;
			m_list.erase(it);
			return 0;
		}
	}
	return -1;
}

EventListener *EventListenerList::Find(EventListener *listener)
{
	for (list<EventListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
		if (*it == listener)
			return listener;
	}
	return NULL;
}

EventListener *EventListenerList::Find(HandlerCB func)
{
	for (list<EventListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
		if (*it != NULL && (*it)->GetFunc() == func)
			return *it;
	}
	return NULL;
}

Handler::Handler(uint32_t enable) : m_event_queue(NULL), m_event_enable(enable), m_hold_count(0)
{
}

int32_t Handler::delete_timer_listener(_map_iterator<int32_t, TimeoutListener *, less<int32_t>,
						    allocator<TimeoutListener *> > it,
				       bool map_erase)
{
	if (it.m_it->second != NULL) {
		delete it.m_it->second;
		it.m_it->second = NULL;
	}
	if (map_erase)
		m_map_t_listener.erase(it.m_it);
	return 0;
}

Handler::~Handler()
{
	for (map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.begin();
	     it != m_map_e_listener.end(); ++it) {
		if (it->second != NULL)
			delete it->second;
	}
	m_map_e_listener.clear();
	for (map<int32_t, TimeoutListener *>::iterator it = m_map_t_listener.begin();
	     it != m_map_t_listener.end(); ++it) {
		if (it->second != NULL) {
			delete it->second;
			it->second = NULL;
		}
	}
	m_map_t_listener.clear();
}

int32_t Handler::HandlerProc(Event *e)
{
	if (e == NULL)
		return -1;
	if (e->m_type & Event::STATE_CONSUME)
		return 0;

	switch ((uint16_t)e->m_type & m_event_enable) {
	case 0x8000:
		return HandleCommandEvent(e);
	case 0x4000:
		return HandleTimerEvent(e);
	case 0x2000:
		return HandleSystemEvent(e);
	case 0x1000:
		return HandleStateEvent(e);
	case 0x800:
		return HandleGrabEvent(e);
	case 0x400:
		return HandleFocusEvent(e);
	case 0x200:
		return HandleAnalogEvent(e);
	case 0x100:
		return HandleInputEvent(e);
	case 0x80:
		return HandleGestureEvent(e);
	case 0x40:
		if (e->m_type & Event::ROUTE_FORWARD)
			return HandleTouchFwEvent(e);
		return HandleTouchEvent(e);
	case 0x20:
		if (e->m_type & Event::ROUTE_FORWARD)
			return HandlePointFwEvent(e);
		return HandlePointEvent(e);
	case 0x10:
		if (e->m_detail & Event::STATE_CONSUME)
			return HandleKeyEvent(e);
		return HandleKeycodeEvent(e);
	default:
		return -1;
	}
}

int32_t Handler::HandlePointEvent_core(Event *e, bool is_forward)
{
	int32_t extra = is_forward ? 0x1000 : 0;

	switch ((int16_t)e->m_type) {
	case 0x21:
		DoEvent(extra | 0x10004, e);
		break;
	case 0x22:
		DoEvent(extra | 0x10005, e);
		break;
	case 0x23:
		DoEvent(extra | 0x10006, e);
		break;
	case 0x24:
		DoEvent(extra | 0x10009, e);
		break;
	case 0x25:
		DoEvent(extra | 0x10007, e);
		break;
	case 0x26:
		DoEvent(extra | 0x10008, e);
		break;
	case 0x28:
		DoEvent(extra | 0x1000A, e);
		break;
	default:
		return 0;
	}
	return 0;
}

int32_t Handler::HandlePointFwEvent(Event *e)
{
	e->m_type = (e->m_type & 0xFFF0FFFF) | Event::ROUTE_FORWARD;
	HandlePointEvent_core(e, true);
	return 0;
}

int32_t Handler::HandleTouchEvent_core(Event *e, bool is_forward)
{
	if ((e->m_type & Event::DETAIL_SUPPLY) ||
	    ((((e->m_detail >> 20) & 0xF) != 0 || IsTouchFrontEventEnabled()) &&
	     (((e->m_detail >> 20) & 0xF) != 1 || IsTouchRearEventEnabled()))) {
		int32_t extra = is_forward ? 0x1000 : 0;

		switch ((uint16_t)e->m_type) {
		case 0x41:
			DoEvent(extra | 0x1000B, e);
			break;
		case 0x42:
			DoEvent(extra | 0x1000C, e);
			break;
		case 0x43:
			DoEvent(extra | 0x1000D, e);
			break;
		case 0x44:
			DoEvent(extra | 0x1000E, e);
			break;
		case 0x45:
			DoEvent(extra | 0x1000F, e);
			break;
		case 0x47:
			DoEvent(extra | 0x10010, e);
			break;
		default:
			return 0;
		}
	}
	return 0;
}

int32_t Handler::HandleTouchFwEvent(Event *e)
{
	e->m_type = (e->m_type & 0xFFF0FFFF) | Event::ROUTE_FORWARD;
	return HandleTouchEvent_core(e, true);
}

int32_t Handler::HandleKeycodeEvent(Event *e)
{
	uint32_t type = e->m_type;

	if ((uint16_t)type > 0x11) {
		if ((uint16_t)type <= 0x12) {
			DoEvent(0x10002, e);
			type = e->m_type;
		} else if ((uint16_t)type <= 0x13) {
			DoEvent(0x10003, e);
			type = e->m_type;
		}
	} else if ((uint16_t)type > 0x10) {
		DoEvent(0x10001, e);
		type = e->m_type;
	}
	if (!(type & Event::STATE_CONSUME) && ((uint16_t)type == 0x11 || (uint16_t)type == 0x13))
		HandleFocusSwitchEvent(e);
	return 0;
}

int32_t Handler::HandleAnalogEvent(Event *e)
{
	uint32_t type = (uint16_t)e->m_type;

	if (type > 0x201) {
		if (type <= 0x202)
			DoEvent(0x10012, e);
	} else if (type > 0x200) {
		DoEvent(0x10011, e);
	}
	return 0;
}

int32_t Handler::HandleInputEvent(Event *e)
{
	DoEvent(0x10013, e);
	return 0;
}

int32_t Handler::HandleKeyEvent(Event *e)
{
	HandleKeycodeEvent(e);
	return 0;
}

int32_t Handler::HandlePointEvent(Event *e)
{
	return HandlePointEvent_core(e, false);
}

int32_t Handler::HandleTouchEvent(Event *e)
{
	return HandleTouchEvent_core(e, false);
}

int32_t Handler::HandleGestureEvent(Event *e)
{
	int32_t kind = (e->m_detail >> 8) & 0x7F;

	switch (kind) {
	case 0:
		DoEvent(0x20004, e);
		break;
	case 2:
		DoEvent(0x20005, e);
		break;
	case 3:
		DoEvent(0x20006, e);
		break;
	case 4:
		DoEvent(0x20001, e);
		break;
	case 5:
		DoEvent(0x20002, e);
		break;
	case 6:
		DoEvent(0x20003, e);
		break;
	case 7:
		DoEvent(0x20007, e);
		break;
	case 8:
		DoEvent(0x20008, e);
		break;
	case 9:
		DoEvent(0x20009, e);
		break;
	case 10:
		DoEvent(0x2000A, e);
		break;
	case 11:
		DoEvent(0x2000B, e);
		break;
	case 12:
		DoEvent(0x2000C, e);
		break;
	case 13:
		DoEvent(0x2000D, e);
		break;
	case 14:
		DoEvent(0x2000E, e);
		break;
	case 15:
		DoEvent(0x2000F, e);
		break;
	case 16:
		DoEvent(0x20012, e);
		break;
	case 17:
		DoEvent(0x20010, e);
		break;
	case 18:
		DoEvent(0x20011, e);
		break;
	case 19:
		DoEvent(0x20013, e);
		break;
	case 20:
		DoEvent(0x20014, e);
		break;
	case 21:
		DoEvent(0x20015, e);
		break;
	case 22:
		DoEvent(0x20018, e);
		break;
	case 23:
		DoEvent(0x20016, e);
		break;
	case 24:
		DoEvent(0x20017, e);
		break;
	case 25:
		DoEvent(0x2001B, e);
		break;
	default:
		if (kind >= 27)
			DoEvent(kind + 0x200E5, e);
		break;
	}
	return 0;
}

int32_t Handler::HandleFocusEvent(Event *e)
{
	uint32_t type = (uint16_t)e->m_type;

	if (type <= 0x402) {
		if (type > 0x400) {
			if (type <= 0x401)
				DoEvent(0x40001, e);
			else
				DoEvent(0x40002, e);
			return 0;
		}
		return -1;
	}
	if (type <= 0x403) {
		DoEvent(0x40003, e);
	} else {
		if (type > 0x404)
			return -1;
		DoEvent(0x40004, e);
	}
	return 0;
}

int32_t Handler::HandleFocusSwitchEvent(Event *e)
{
	uint32_t kind = (uint8_t)e->m_detail;

	if (kind > 2) {
		if (kind <= 3)
			DoEvent(0x40007, e);
		else if (kind <= 4)
			DoEvent(0x40008, e);
	} else if (kind != 0) {
		if (kind > 1)
			DoEvent(0x40006, e);
		else
			DoEvent(0x40005, e);
	}
	return 0;
}

int32_t Handler::HandleGrabEvent(Event *e)
{
	uint32_t type = (uint16_t)e->m_type;

	if (type > 0x801) {
		if (type <= 0x802) {
			DoEvent(0x4000B, e);
			return 0;
		}
		return -1;
	}
	if (type <= 0x800)
		return -1;
	DoEvent(0x4000A, e);
	return 0;
}

int32_t Handler::HandleStateEvent(Event *e)
{
	switch ((uint16_t)e->m_type) {
	case 0x1001:
		DoEvent(0x100001, e);
		break;
	case 0x1002:
		DoEvent(0x100002, e);
		break;
	case 0x1003:
		DoEvent(0x100004, e);
		break;
	case 0x1004:
		DoEvent(0x100003, e);
		break;
	case 0x1005:
		DoEvent(0x100005, e);
		break;
	case 0x1006:
		DoEvent(0x100006, e);
		break;
	case 0x1007:
		DoEvent(0x100007, e);
		break;
	case 0x1008:
		DoEvent(0x100008, e);
		break;
	case 0x1009:
		DoEvent(0x20019, e);
		break;
	case 0x100A:
		DoEvent(0x2001A, e);
		break;
	default:
		return 0;
	}
	return 0;
}

int32_t Handler::HandleCommandEvent(Event *e)
{
	DoEvent(0x100009, e);
	return 0;
}

int32_t Handler::HandleSystemEvent(Event *e)
{
	DoEvent(0x800000, e);
	return 0;
}

int32_t Handler::HandleTimerEvent(Event *e)
{
	DoEvent(e->m_data[0], e);
	return 0;
}

int32_t Handler::AddEventListener(int32_t type, EventListener *listener, bool del_prev)
{
	if (m_hold_count > 0) {
		if (listener->m_autodelete)
			delete listener;
		return -1;
	}

	map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.find(type);

	if (it == m_map_e_listener.end()) {
		EventListenerList *listeners = new EventListenerList();

		listeners->Add(listener);
		m_map_e_listener.insert(pair<int32_t, EventListenerList *>(type, listeners));
	} else {
		EventListenerList *listeners = it->second;

		if (del_prev)
			listeners->DelAll();
		if (listeners != NULL) {
			if (del_prev)
				listeners->AddFront(listener);
			else
				listeners->Add(listener);
		}
	}
	return 0;
}

int32_t Handler::DeleteEventListener(int32_t type, EventListener *listener)
{
	if (m_hold_count > 0)
		return -1;
	if (listener != NULL) {
		map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.find(type);

		if (it == m_map_e_listener.end())
			return -1;
		if (it->second == NULL || it->second->Del(listener) != 0)
			return -1;
		return 0;
	}

	map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.find(type);

	if (it != m_map_e_listener.end())
		it->second->DelAll();
	return 0;
}

int32_t Handler::DeleteEventCallback(int32_t type, HandlerCB func, void *data)
{
	if (m_hold_count > 0)
		return -1;
	if (func != NULL) {
		map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.find(type);

		if (it == m_map_e_listener.end())
			return -1;

		EventListenerList *listeners = it->second;

		if (listeners == NULL)
			return -1;
		if (listeners->Del(listeners->Find(func)) != 0)
			return -1;
		return 0;
	}

	map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.find(type);

	if (it != m_map_e_listener.end())
		it->second->DelAll();
	return 0;
}

int32_t Handler::DoEvent(int32_t type, Event *e)
{
	if (m_hold_count > 0 && (uint16_t)e->m_type != 0x1002)
		return -1;

	map<int32_t, EventListenerList *>::iterator it = m_map_e_listener.find(type);

	if (it != m_map_e_listener.end()) {
		EventListenerList *listeners = it->second;
		int32_t key = it->first;

		listeners->m_on_exec = 1;
		for (list<EventListener *>::iterator l = listeners->m_list.begin();
		     l != listeners->m_list.end(); ++l) {
			if (*l != NULL)
				(*l)->Do(key, this, e);
		}
		listeners->m_on_exec = 0;
		listeners->UpdateKill();

		uint32_t flags = e->m_type;

		if (!(flags & Event::STATE_IGNORE) && !(flags & Event::ROUTE_FORWARD))
			e->m_type = (flags & 0xF0FFFFFF) | Event::STATE_CONSUME;
		if (listeners->m_list.size() == 0) {
			delete listeners;
			m_map_e_listener.erase(it);
		}
	}
	return 0;
}

void Handler::UpdateTimer()
{
	map<int32_t, TimeoutListener *>::iterator it = m_map_t_listener.begin();

	while (it != m_map_t_listener.end()) {
		TimeoutListener *listener = it->second;

		if (listener != NULL) {
			Timer *timer = listener->m_timer;

			if (timer != NULL && timer->m_state != Timer::State_Hold &&
			    (timer->m_state != Timer::State_Finished || timer->m_repeat != 0)) {
				listener->DoTick(m_event_queue, this);
				if (it->first & Handler::CB_INTERVAL) {
					Timer *t = listener->m_timer;

					if (t == NULL || t->m_state == Timer::State_Hold ||
					    (t->m_state == Timer::State_Finished && t->m_repeat == 0))
						listener->ResetTimer();
				}
				++it;
			} else if (it->first & Handler::CB_TIMEOUT) {
				map<int32_t, TimeoutListener *>::iterator cur = it;

				++it;
				m_map_t_listener.erase(cur);
				delete listener;
			}
		} else {
			map<int32_t, TimeoutListener *>::iterator cur = it;

			++it;
			m_map_t_listener.erase(cur);
		}
	}
}

int32_t Handler::SetTimerListener(int32_t id, float timeout, EventListener *listener)
{
	if (m_hold_count > 0) {
		if (listener->m_autodelete)
			delete listener;
		return -1;
	}

	AddEventListener(id, listener, true);

	map<int32_t, TimeoutListener *>::iterator it = m_map_t_listener.find(id);

	if (it != m_map_t_listener.end()) {
		if (it->second != NULL) {
			delete it->second;
			it->second = NULL;
		}
		m_map_t_listener.erase(it);
	}
	m_map_t_listener.insert(pair<int32_t, TimeoutListener *>(
		id, new TimeoutListener(new Timer(timeout, 0, 0, -1, NULL), id)));
	return 0;
}

int32_t Handler::KillTimer(int32_t id)
{
	if (m_hold_count > 0)
		return -1;

	map<int32_t, EventListenerList *>::iterator e_it = m_map_e_listener.find(id);

	if (e_it != m_map_e_listener.end()) {
		if (e_it->second != NULL)
			e_it->second->DelAll();
	}

	map<int32_t, TimeoutListener *>::iterator t_it = m_map_t_listener.find(id);

	if (t_it != m_map_t_listener.end()) {
		if (t_it->second != NULL) {
			delete t_it->second;
			t_it->second = NULL;
		}
		m_map_t_listener.erase(t_it);
	}
	return 0;
}

void TimeoutListener::OnFinish(void *data1, void *data2)
{
	if (data1 != NULL) {
		Event ev(0x4000, (Handler *)data2, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		ev.m_data[0] = m_id;
		((EventQueue *)data1)->PushBack(ev);
	}
}

}
}
