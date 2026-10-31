/*
 * paf::job: work items run by the worker threads of a job queue. A worker runs
 * an item's Run(); the thread that calls CheckAllQueues() then calls its
 * Finish() with the result, also for items canceled before they ran.
 */
#ifndef PAF_JOB_JOB_H
#define PAF_JOB_JOB_H

#include <std/string>
#include <std/list>
#include <common/shared_ptr.h>
#include <thread/thread.h>

namespace paf {
namespace job {

class JobItem {
public:
	class Option;

	enum Status {
		STATUS_IDLE = 0,
		STATUS_QUEUED = 1,
		STATUS_RUNNING = 2,
		STATUS_DONE = 3
	};

	JobItem(const char *name, const Option *option);
	virtual ~JobItem();
	virtual void Cancel();
	virtual int Run() = 0;
	virtual void Finish(int result) = 0;
	bool IsCanceled() const;

	string m_name;
	int m_status;
	int m_result;
	bool m_canceled;
};

class JobThread;

class JobQueue {
public:
	class Option {
	public:
		Option();

		int priority;
		size_t stackSize;
		unsigned int numThreads;
		const thread::Thread::Option *threadOption;
	};

	enum {
		ERROR_ITEM_BUSY = -2136012543,		/* 0x80AF0901 */
		ERROR_NO_THREAD = -2136012542		/* 0x80AF0902 */
	};

	JobQueue(const char *name, const Option *option);
	~JobQueue();
	static void Init(const Option *option);
	void WaitEmpty();
	static void Exit();
	void Check();
	static void CheckAllQueues();
	static int NumQueues();
	int Enqueue(common::SharedPtr<JobItem> &item);
	int NumItems() const;
	void CancelAllItems();
	void ChangePriority(int priority);

	static list<JobQueue *> queue_list;
	static JobQueue *default_queue;

	string m_name;
	bool m_busy;
	list<JobThread *> m_threads;
	list<common::SharedPtr<JobItem> > m_items;
	list<common::SharedPtr<JobItem> >::iterator m_current;
};

}
}

#endif
