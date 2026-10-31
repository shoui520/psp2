/*
 * paf::thread::CallQueue: functions queued under a mutex and run, one by one and
 * with the mutex released, by the thread that calls Check().
 */
#include <thread/thread.h>

namespace paf {
namespace thread {

CallQueue::CallQueue(const char *name) : m_mutex(name, 0)
{
}

CallQueue::~CallQueue()
{
}

void CallQueue::Check()
{
	m_mutex.Lock();
	while (m_queue.size() != 0) {
		Item *item = m_queue.front();

		m_queue.pop_front();
		m_mutex.Unlock();
		item->func(item->arg);
		m_mutex.Lock();
		delete item;
	}
	m_mutex.Unlock();
}

void CallQueue::Enqueue(const char *name, void (*func)(void *), void *arg)
{
	m_mutex.Lock();

	Item *item = new Item;

	item->name = name;
	item->func = func;
	item->arg = arg;
	m_queue.push_back(item);
	m_mutex.Unlock();
}

int CallQueue::NumItems()
{
	m_mutex.Lock();

	int count = m_queue.size();

	m_mutex.Unlock();
	return count;
}

void CallQueue::WaitEmpty()
{
	while (NumItems() > 0)
		Sleep(10);
}

CallQueue CallQueue::main_thread_callqueue("ScePafMainThreadCallQueue");

}
}
