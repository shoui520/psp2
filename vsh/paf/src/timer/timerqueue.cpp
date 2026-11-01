/*
 * paf::TimerQueue: the front listener is ticked until it finishes, then
 * deleted; m_total is the time left in the queue, in milliseconds.
 */
#include <timer/timerqueue.h>

namespace paf {

/* Drops NULL listeners from the front. */
TimerListener *TimerQueue::GetFront()
{
	if (m_list.size() == 0)
		return NULL;

	TimerListener *t = m_list.front();

	if (t != NULL)
		return t;
	for (;;) {
		m_list.pop_front();
		if (m_list.size() == 0)
			break;
		t = m_list.front();
		if (t != NULL)
			return t;
	}
	return NULL;
}

int32_t TimerQueue::EraseFront(bool auto_delete)
{
	if (m_list.size() == 0)
		return -2;

	TimerListener *t = GetFront();

	m_total -= t->GetTotalTime();
	m_list.erase(m_list.begin());
	if (t == NULL)
		return -1;
	if (auto_delete)
		delete t;
	return 0;
}

void TimerQueue::ClearAll()
{
	if (m_list.size() == 0)
		return;
	for (list<TimerListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it)
		delete *it;
	m_list.clear();
	m_total = 0.0f;
}

/* Drops every listener but the front one. */
void TimerQueue::Clear()
{
	if (m_list.size() == 0)
		return;

	TimerListener *front = GetFront();

	if (front == NULL)
		return;

	list<TimerListener *>::iterator it = m_list.begin();

	++it;
	while (it != m_list.end()) {
		delete *it;
		it = m_list.erase(it);
	}
	m_total = front->GetTotalTime();
}

/* Sony's: a NULL back empties the queue. */
TimerListener *TimerQueue::GetBack()
{
	if (m_list.size() == 0)
		return NULL;

	TimerListener *t = m_list.back();

	if (t != NULL)
		return t;
	do {
		m_list.pop_back();
	} while (m_list.size() != 0);
	return NULL;
}

float TimerQueue::DoTick(void *data1, void *data2)
{
	TimerListener *t = GetFront();

	if (t == NULL)
		return 0.0f;
	while (t->IsFinished()) {
		EraseFront(true);
		t = GetFront();
		if (t == NULL)
			return 0.0f;
	}
	return t->DoTick(data1, data2);
}

bool TimerQueue::IsFinished()
{
	for (list<TimerListener *>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
		if (*it != NULL && !(*it)->IsFinished())
			return false;
	}
	return true;
}

}
