/*
 * paf::common::JobCallQueue (src/common/job_call_queue.c): functions run one by one on a job
 * queue, each in a job item of its own, its call queue made at the first Enqueue. The class
 * and its methods are Sony's (the mangled exports); the members' names are ours.
 */
#ifndef PAF_INCLUDE_COMMON_JOB_CALL_QUEUE_H
#define PAF_INCLUDE_COMMON_JOB_CALL_QUEUE_H

#include <std/string>
#include <thread/thread.h>

namespace paf {

namespace job {
class JobQueue;
}

namespace common {

class JobCallQueue {
public:
	JobCallQueue(const string &name, job::JobQueue *queue);
	~JobCallQueue();
	/* the function queued, called with userdata on a thread of the job queue */
	void Enqueue(const char *name, void (*func)(void *), void *userdata);
	/* the next queued function handed to the job queue once the one before has finished */
	void Check();
	int NumItems();
	void WaitEmpty();

	string m_name;
	job::JobQueue *m_job_queue;
	thread::Mutex m_mutex;
	thread::CallQueue *m_call_queue;
};

}
}

#endif
