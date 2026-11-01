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

#define JobCallQueueCheck _ZN3paf6common12JobCallQueue5CheckEv
#define JobCallQueueEnqueue _ZN3paf6common12JobCallQueue7EnqueueEPKcPFvPvES4_
#define JobCallQueueNumItems _ZN3paf6common12JobCallQueue8NumItemsEv
#define JobCallQueueWaitEmpty _ZN3paf6common12JobCallQueue9WaitEmptyEv
#define JobCallQueueConstruct _ZN3paf6common12JobCallQueueC1ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPNS_3job8JobQueueE
#define JobCallQueueConstructBase _ZN3paf6common12JobCallQueueC2ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPNS_3job8JobQueueE
#define JobCallQueueDestroy _ZN3paf6common12JobCallQueueD1Ev
#define JobCallQueueDestroyBase _ZN3paf6common12JobCallQueueD2Ev

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
