/*
 * paf::TimerListenerList: the listeners ticked once per frame (s_default_list
 * is the framework's). A listener registered with interval set has its timer
 * reset each time it finishes.
 */
#ifndef PAF_TIMER_TIMERLISTENERLIST_H
#define PAF_TIMER_TIMERLISTENERLIST_H

#include <std/list>
#include <thread/thread.h>

namespace paf {

class TimerListener;

class TimerListenerList {
public:
	class ListenerParam {
	public:
		ListenerParam(TimerListener *listener, bool interval, void *data1, void *data2);

		TimerListener *m_listener;
		void *m_data1;
		void *m_data2;
		bool m_interval : 1;
	};

	__declspec(dllexport) TimerListenerList() : m_iterator(NULL),
		m_list_mutex("ScePafTimerListenerListMutex", 0)
	{
		m_iterator = m_list.end();
		m_iterator_updated = false;
	}

	~TimerListenerList()
	{
	}

	static void Init();
	static void Exit();

	void Update();
	void Register(const ListenerParam &param);
	void Unregister(TimerListener *listener);

	static TimerListenerList *s_default_list;

private:
	list<ListenerParam> m_list;
	list<ListenerParam>::iterator m_iterator;
	bool m_iterator_updated;
	thread::RMutex m_list_mutex;
};

}

#endif /* PAF_TIMER_TIMERLISTENERLIST_H */
