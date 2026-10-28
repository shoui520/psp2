#ifndef PAF_JOB_JOB_H
#define PAF_JOB_JOB_H

#include "../std/string.h"

typedef struct JobItem JobItem;
typedef struct JobQueue JobQueue;
typedef struct JobItemOption JobItemOption;

typedef struct JobItemVTable {
	JobItem *(*Destroy)(JobItem *self);
	void (*Delete)(JobItem *self);
	void (*Cancel)(JobItem *self);
	int (*Run)(JobItem *self);
	void (*Finish)(JobItem *self, int result);
} JobItemVTable;

struct JobItem {
	const JobItemVTable *vtable;
	PafString m_name;
	int m_status;
	int m_result;
	unsigned char m_canceled;
};

typedef struct JobItemRefCounter {
	JobItem *m_ptr;
	int32_t m_counter;
	int32_t m_weak_counter;
	void (*m_deleter)(JobItem *ptr);
} JobItemRefCounter;

typedef struct JobItemSharedPtr {
	JobItem *m_ptr;
	JobItemRefCounter *m_ref_counter;
} JobItemSharedPtr;

typedef char JobItemSizeCheck[sizeof(JobItem) == 28 ? 1 : -1];
typedef char JobItemRefCounterSizeCheck[sizeof(JobItemRefCounter) == 16 ? 1 : -1];

#define JobItemConstructBase ScePafThread_ECF8334D
#define JobItemDestroyBase ScePafThread_E639D331
#define JobItemCancel ScePafThread_2AE6D3EB
#define JobQueueEnqueue ScePafThread_93F7A9D6

JobItem *JobItemConstructBase(JobItem *self, const char *name,
			      const JobItemOption *option);
JobItem *JobItemDestroyBase(JobItem *self);
void JobItemCancel(JobItem *self);
int JobQueueEnqueue(JobQueue *self, JobItemSharedPtr *item);

#endif
