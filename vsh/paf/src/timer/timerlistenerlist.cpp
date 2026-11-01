/*
 * paf::TimerListenerList: ticks every registered listener once per Update.
 * A listener may unregister itself from its callbacks: Unregister moves the
 * running iterator on and tells Update so.
 */
#include <timer/timerlistener.h>
#include <timer/timerlistenerlist.h>

namespace paf {

TimerListenerList *TimerListenerList::s_default_list;

void TimerListenerList::Register(const ListenerParam &param)
{
	m_list_mutex.Lock();
	m_list.push_back(param);
	m_list_mutex.Unlock();
}

void TimerListenerList::Unregister(TimerListener *listener)
{
	m_list_mutex.Lock();
	for (list<ListenerParam>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
		if (it->m_listener == listener) {
			if (it == m_iterator) {
				m_iterator = m_list.erase(it);
				m_iterator_updated = true;
			} else {
				m_list.erase(it);
			}
			break;
		}
	}
	m_list_mutex.Unlock();
}

void TimerListenerList::Update()
{
	m_list_mutex.Lock();

	list<ListenerParam>::iterator it = m_list.begin();

	m_iterator = it;
	while (it != m_list.end()) {
		it->m_listener->DoTick(it->m_data1, it->m_data2);
		if (m_iterator_updated) {
			/* the listener unregistered itself: m_iterator is already the next one */
			m_iterator_updated = false;
			it = m_iterator;
			continue;
		}
		if (it->m_interval) {
			TimerListener *listener = it->m_listener;

			if (listener->IsFinished())
				listener->ResetTimer();
		}
		it = ++m_iterator;
	}
	m_list_mutex.Unlock();
}

void TimerListenerList::Init()
{
	s_default_list = new TimerListenerList();
}

void TimerListenerList::Exit()
{
	delete s_default_list;
}

TimerListenerList::ListenerParam::ListenerParam(TimerListener *listener, bool interval,
						void *data1, void *data2)
{
	m_listener = listener;
	m_data1 = data1;
	m_data2 = data2;
	m_interval = interval;
}

}
