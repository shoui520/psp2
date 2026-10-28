#ifndef PAF_COMMON_JOB_CALL_QUEUE_H
#define PAF_COMMON_JOB_CALL_QUEUE_H

#include "../thread/callqueue.h"
#include "../job/job.h"

typedef struct JobCallQueue {
	PafString m_name;
	JobQueue *m_job_queue;
	Mutex m_mutex;
	CallQueue *m_call_queue;
} JobCallQueue;

typedef char JobCallQueueSizeCheck[sizeof(JobCallQueue) == 56 ? 1 : -1];
typedef char JobCallQueueBodyOffsetCheck[offsetof(JobCallQueue, m_call_queue) == 48 ? 1 : -1];

#define JobCallQueueCheck ScePafCommon_C16C2378
#define JobCallQueueEnqueue ScePafCommon_FDD1CBBD
#define JobCallQueueNumItems ScePafCommon_B01BE5EF
#define JobCallQueueWaitEmpty ScePafCommon_354C389A
#define JobCallQueueConstruct ScePafCommon_91CD88B3
#define JobCallQueueConstructBase ScePafCommon_543B37D2
#define JobCallQueueDestroy ScePafCommon_BCD1863A
#define JobCallQueueDestroyBase ScePafCommon_03E15726

void JobCallQueueCheck(JobCallQueue *self);
void JobCallQueueEnqueue(JobCallQueue *self, const char *name,
			 CallQueueFunction entry, void *userdata);
int JobCallQueueNumItems(JobCallQueue *self);
void JobCallQueueWaitEmpty(JobCallQueue *self);
JobCallQueue *JobCallQueueConstruct(JobCallQueue *self, const PafString *name,
				   JobQueue *job_queue);
JobCallQueue *JobCallQueueConstructBase(JobCallQueue *self, const PafString *name,
				       JobQueue *job_queue);
JobCallQueue *JobCallQueueDestroy(JobCallQueue *self);
JobCallQueue *JobCallQueueDestroyBase(JobCallQueue *self);

#endif
