/*
 * paf::common::JobCallQueue: the calls queued on one call queue, run on a thread of a job queue
 * by one job at a time; the call queue is made at the first Enqueue and handed to its job by
 * Check, which leaves the next Enqueue to make a new one.
 */
#include <common/job_call_queue.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <thread/thread.h>
#include <std/new>

namespace paf {
namespace common {

/* the job that runs a call queue's calls and deletes it (name ours, from its job name) */
class CallQueueJobItem : public job::JobItem {
public:
	CallQueueJobItem(const string &name, thread::CallQueue *call_queue)
		: job::JobItem((string("ScePafCallQueueJobItem") + name).c_str(), NULL),
		  m_call_queue(call_queue)
	{
	}

	virtual ~CallQueueJobItem()
	{
	}

	virtual int Run()
	{
		m_call_queue->Check();
		delete m_call_queue;
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	thread::CallQueue *m_call_queue;
};

void JobCallQueue::Check()
{
	m_mutex.Lock();
	if (m_call_queue) {
		SharedPtr<job::JobItem> item(new CallQueueJobItem(m_name, m_call_queue));

		m_job_queue->Enqueue(item);
		m_call_queue = NULL;
	}
	m_mutex.Unlock();
}

void JobCallQueue::Enqueue(const char *name, void (*func)(void *), void *userdata)
{
	m_mutex.Lock();
	if (!m_call_queue)
		m_call_queue = new thread::CallQueue(
			(string("ScePafJobCallQueueBody") + m_name).c_str());
	m_call_queue->Enqueue(name, func, userdata);
	m_mutex.Unlock();
}

int JobCallQueue::NumItems()
{
	int count = 0;

	m_mutex.Lock();
	if (m_call_queue)
		count = m_call_queue->NumItems();
	m_mutex.Unlock();
	return count;
}

void JobCallQueue::WaitEmpty()
{
	while (NumItems() > 0)
		thread::Sleep(10);
}

JobCallQueue::JobCallQueue(const string &name, job::JobQueue *queue)
	: m_name(name), m_job_queue(queue),
	  m_mutex((string("ScePafJobCallQueueMutex") + name).c_str(), 0), m_call_queue(NULL)
{
}

JobCallQueue::~JobCallQueue()
{
	delete m_call_queue;
}

}
}
