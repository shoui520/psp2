#ifndef PAF_JOB_JOB_H
#define PAF_JOB_JOB_H

#include "../std/string.h"
#include <sce_atomic.h>

typedef struct JobItem JobItem;
typedef struct JobQueue JobQueue;
typedef struct JobItemOption JobItemOption;

typedef struct JobQueueNode {
	struct JobQueueNode *prev;
	struct JobQueueNode *next;
	JobQueue *val;
} JobQueueNode;

typedef struct JobQueueList {
	JobQueueNode *root_node;
	int32_t count;
	unsigned char alloc_value;
	unsigned char alloc_node;
} JobQueueList;

typedef char JobQueueNodeSizeCheck[sizeof(JobQueueNode) == 12 ? 1 : -1];
typedef char JobQueueListSizeCheck[sizeof(JobQueueList) == 12 ? 1 : -1];

#define JobQueueQueueList _ZN3paf3job8JobQueue10queue_listE
extern JobQueueList JobQueueQueueList;

JobQueueList *JobQueueListConstruct(JobQueueList *self, const void *allocator);
JobQueueList *JobQueueListDestroy(JobQueueList *self);
void JobInitialize(void);

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

#define JobItemConstructBase _ZN3paf3job7JobItemC2EPKcPKNS1_6OptionE
#define JobItemDestroyBase _ZN3paf3job7JobItemD2Ev
#define JobItemCancel _ZN3paf3job7JobItem6CancelEv
#define JobQueueEnqueue _ZN3paf3job8JobQueue7EnqueueERNS_6common9SharedPtrINS0_7JobItemEEE
#define JobItemIsCanceled _ZNK3paf3job7JobItem10IsCanceledEv

JobItem *JobItemConstructBase(JobItem *self, const char *name,
			      const JobItemOption *option);
JobItem *JobItemDestroyBase(JobItem *self);
void JobItemCancel(JobItem *self);
int JobQueueEnqueue(JobQueue *self, JobItemSharedPtr *item);
int JobItemIsCanceled(JobItem *self);

static inline __attribute__((always_inline))
void JobItemSharedPtrAddRef(JobItemSharedPtr *item)
{
	if (item->m_ptr)
		sceAtomicIncrement32AcqRel(&item->m_ref_counter->m_counter);
}

static inline __attribute__((always_inline))
void JobItemSharedPtrRelease(JobItemSharedPtr *item)
{
	if (item->m_ptr &&
	    sceAtomicDecrement32AcqRel(&item->m_ref_counter->m_counter) == 1) {
		item->m_ref_counter->m_deleter(item->m_ptr);
		if (sceAtomicDecrement32AcqRel(&item->m_ref_counter->m_weak_counter) == 1)
			ScePafStdc_1B77082E(item->m_ref_counter);
	}
}

#endif
