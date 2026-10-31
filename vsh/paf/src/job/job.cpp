/*
 * paf::job: one mutex guards every queue's items and the list of queues. A
 * queue's Check() hands queued items to idle workers and, with the mutex
 * released, calls Finish() for each item that is done or was canceled.
 */
#include <job/job.h>

namespace paf {
namespace job {

list<JobQueue *> JobQueue::queue_list;
static thread::Mutex s_mutex("ScePafJob", 0);
JobQueue *JobQueue::default_queue = NULL;

/* A queue's worker: waits on its semaphore for an item, runs it and marks it done. */
class JobThread : public thread::Thread {
public:
	JobThread(int priority, size_t stackSize, const char *name, const Option *option)
		: Thread(priority, stackSize, name, option), m_sema("ScePafJobThread", 0, 1, 0)
	{
	}

	virtual ~JobThread()
	{
	}

	/* Also wakes the worker, so that it sees the cancel. */
	void Cancel()
	{
		Thread::Cancel();
		m_sema.Release();
	}

	virtual void EntryFunction()
	{
		while (!IsCanceled()) {
			m_sema.Acquire();
			if (m_item.get() == NULL)
				continue;
			m_item->m_result = m_item->Run();
			s_mutex.Lock();
			m_item->m_status = JobItem::STATUS_DONE;
			m_item.reset();
			s_mutex.Unlock();
		}
	}

	thread::Semaphore m_sema;
	common::SharedPtr<JobItem> m_item;
};

JobItem::JobItem(const char *name, const Option *option) : m_name(name)
{
	m_status = STATUS_IDLE;
	m_result = 0;
	m_canceled = false;
}

JobItem::~JobItem()
{
}

void JobItem::Cancel()
{
	m_canceled = true;
}

bool JobItem::IsCanceled() const
{
	return m_canceled;
}

JobQueue::Option::Option()
{
	priority = thread::GetPriority()->normal;
	stackSize = 0x4000;
	numThreads = 1;
	threadOption = NULL;
}


JobQueue::JobQueue(const char *name, const Option *option)
	: m_name(name), m_busy(false), m_current(m_items.end())
{
	Option defaultOption;

	if (option == NULL)
		option = &defaultOption;
	for (unsigned int i = 0; i < option->numThreads; ++i) {
		JobThread *thread = new JobThread(option->priority, option->stackSize, name,
						  option->threadOption);

		if (thread->Start() != SCE_OK) {
			delete thread;
			break;
		}
		m_threads.push_back(thread);
	}
	s_mutex.Lock();
	queue_list.push_back(this);
	s_mutex.Unlock();
}

JobQueue::~JobQueue()
{
	WaitEmpty();
	for (list<JobThread *>::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
		(*it)->Cancel();
		(*it)->Join();
		delete *it;
	}
	s_mutex.Lock();
	queue_list.remove(this);
	s_mutex.Unlock();
}

void JobQueue::Init(const Option *option)
{
	default_queue = new JobQueue("ScePafJobDefault", option);
}

void JobQueue::WaitEmpty()
{
	for (;;) {
		s_mutex.Lock();
		if (m_items.size() == 0 && !m_busy)
			break;
		s_mutex.Unlock();
		thread::Sleep(10);
	}
	s_mutex.Unlock();
}

void JobQueue::Exit()
{
	delete default_queue;
	default_queue = NULL;
}

/* Called with the mutex held. */
void JobQueue::Check()
{
	if (m_items.size() == 0)
		return;
	if (m_current == m_items.end())
		m_current = m_items.begin();

	JobThread *idle = NULL;

	while (m_current != m_items.end()) {
		if (idle == NULL) {
			for (list<JobThread *>::iterator it = m_threads.begin(); it != m_threads.end(); ++it) {
				if ((*it)->m_item.get() == NULL) {
					idle = *it;
					break;
				}
			}
			if (idle == NULL) {
				m_current = m_items.end();
				return;
			}
		}

		switch ((*m_current)->m_status) {
		case JobItem::STATUS_IDLE:
			break;
		case JobItem::STATUS_QUEUED:
			if ((*m_current)->m_canceled) {
				{
					common::SharedPtr<JobItem> done(*m_current);

					m_current = m_items.erase(m_current);
					done->m_status = JobItem::STATUS_DONE;
					m_busy = true;
					s_mutex.Unlock();
					done->Finish(done->m_result);
				}
				s_mutex.Lock();
				m_busy = false;
				idle = NULL;
			} else {
				(*m_current)->m_status = JobItem::STATUS_RUNNING;
				idle->m_item = *m_current;
				idle->m_sema.Release();
				idle = NULL;
			}
			break;
		case JobItem::STATUS_RUNNING:
			++m_current;
			break;
		case JobItem::STATUS_DONE:
			{
				common::SharedPtr<JobItem> done(*m_current);

				m_current = m_items.erase(m_current);
				m_busy = true;
				s_mutex.Unlock();
				done->Finish(done->m_result);
			}
			s_mutex.Lock();
			m_busy = false;
			idle = NULL;
			break;
		}
	}
}

void JobQueue::CheckAllQueues()
{
	s_mutex.Lock();
	for (list<JobQueue *>::iterator it = queue_list.begin(); it != queue_list.end(); ++it)
		(*it)->Check();
	s_mutex.Unlock();
}

/* The default queue does not count. */
int JobQueue::NumQueues()
{
	s_mutex.Lock();

	int count = queue_list.size();

	if (default_queue != NULL)
		count--;
	s_mutex.Unlock();
	return count;
}

int JobQueue::Enqueue(common::SharedPtr<JobItem> &item)
{
	s_mutex.Lock();
	if (item->m_status != JobItem::STATUS_IDLE) {
		s_mutex.Unlock();
		return ERROR_ITEM_BUSY;
	}
	if (m_threads.size() == 0) {
		s_mutex.Unlock();
		return ERROR_NO_THREAD;
	}
	item->m_status = JobItem::STATUS_QUEUED;
	m_items.push_back(item);
	s_mutex.Unlock();
	return SCE_OK;
}

int JobQueue::NumItems() const
{
	s_mutex.Lock();

	int count = m_items.size();

	s_mutex.Unlock();
	return count;
}

void JobQueue::CancelAllItems()
{
	s_mutex.Lock();
	for (list<common::SharedPtr<JobItem> >::iterator it = m_items.begin(); it != m_items.end(); ++it)
		(*it)->Cancel();
	s_mutex.Unlock();
}

void JobQueue::ChangePriority(int priority)
{
	for (list<JobThread *>::iterator it = m_threads.begin(); it != m_threads.end(); ++it)
		(*it)->ChangePriority(priority);
}

}
}
