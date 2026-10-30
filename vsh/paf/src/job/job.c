#include "job.h"
#include "../thread/mutex.h"
#include "../thread/thread.h"
#include "../std/stdc++/atexit.h"
#include "../std/new.h"

JobQueueList JobQueueQueueList;
static Mutex mutex;
void *sce_paf_default_job_queue;

int ScePafThread_B958E592(void)
{
	int count;

	MutexLock(&mutex);
	count = JobQueueQueueList.count;
	if (sce_paf_default_job_queue)
		count = JobQueueQueueList.count - 1;
	MutexUnlock(&mutex);
	return count;
}

typedef struct PafPendingJobNode {
	struct PafPendingJobNode *previous;
	struct PafPendingJobNode *next;
	JobItemSharedPtr item;
} PafPendingJobNode;

typedef struct PafPendingJobList {
	PafPendingJobNode *root;
	uint32_t count;
} PafPendingJobList;

struct JobQueue {
	uint8_t reserved[20];
	uint32_t workerCount;
	uint8_t reserved2[4];
	PafPendingJobList pending;
};

_Static_assert(offsetof(JobQueue, workerCount) == 20,
	       "job queue worker count offset");
_Static_assert(offsetof(JobQueue, pending) == 28,
	       "job queue pending list offset");
_Static_assert(sizeof(PafPendingJobNode) == 16,
	       "pending job node size");

__attribute__((noinline))
static void AppendPendingJob(PafPendingJobList *list,
			    const JobItemSharedPtr *item)
{
	PafPendingJobNode *node;
	PafPendingJobNode *root = list->root;

	if (!root)
		return;
	node = PafNew(sizeof(*node));
	if (node) {
		JobItemSharedPtr *placement =
			PafPlacementNew(sizeof(*placement), &node->item);

		if (placement) {
			JobItem *job = item->m_ptr;

			placement->m_ptr = job;
			JobItemRefCounter *counter = item->m_ref_counter;
			placement->m_ref_counter = counter;
			if (job)
				sceAtomicIncrement32AcqRel(
					&counter->m_counter);
		}
	} else {
		ScePafStdc_D608246A();
		node = list->root;
	}
	root = list->root;
	if (node != root) {
		node->previous = root->previous;
		node->next = root;
		root->previous->next = node;
		root->previous = node;
		++list->count;
	}
}

int JobQueueEnqueue(JobQueue *self, JobItemSharedPtr *item)
{
	MutexLock(&mutex);
	if (item->m_ptr->m_status != 0) {
		MutexUnlock(&mutex);
		return -2136012543;
	}
	if (!self->workerCount) {
		MutexUnlock(&mutex);
		return -2136012542;
	}
	item->m_ptr->m_status = 1;
	AppendPendingJob(&self->pending, item);
	MutexUnlock(&mutex);
	return 0;
}

JobItem *ScePafThread_88DA7B11(JobItem *self);
void ScePafThread_CCE903F3(JobItem *self);

/* The native base-class vtable has two pure-virtual job callbacks. */
static const JobItemVTable jobItemVTable = {
	ScePafThread_88DA7B11, ScePafThread_CCE903F3, JobItemCancel,
	(int (*)(JobItem *))__cxa_pure_virtual,
	(void (*)(JobItem *, int))__cxa_pure_virtual
};

JobItem *ScePafThread_A54DB064(JobItem *self, const char *name)
{
	size_t length = 0;

	self->vtable = &jobItemVTable;
	self->m_name.ptr = &PafStringEmpty;
	self->m_name.count = 0;
	if (name)
		length = sce_paf_strlen(name);
	PafStringAssign(&self->m_name, name, length);
	self->m_status = 0;
	self->m_result = 0;
	self->m_canceled = 0;
	return self;
}

JobItem *JobItemConstructBase(JobItem *self, const char *name,
			      const JobItemOption *option)
{
	size_t length = 0;

	self->vtable = &jobItemVTable;
	self->m_name.ptr = &PafStringEmpty;
	self->m_name.count = 0;
	if (name)
		length = sce_paf_strlen(name);
	PafStringAssign(&self->m_name, name, length);
	self->m_status = 0;
	self->m_result = 0;
	self->m_canceled = 0;
	return self;
}

static inline __attribute__((always_inline))
JobItem *DestroyJobItem(JobItem *self)
{
	char *ptr = self->m_name.ptr;

	self->vtable = &jobItemVTable;
	if (ptr != &PafStringEmpty)
		PafDelete(ptr);
	self->m_name.ptr = (char *)-1;
	self->m_name.count = (size_t)-1;
	return self;
}

JobItem *JobItemDestroyBase(JobItem *self)
{
	return DestroyJobItem(self);
}

JobItem *ScePafThread_88DA7B11(JobItem *self)
{
	return DestroyJobItem(self);
}

void ScePafThread_CCE903F3(JobItem *self)
{
	DestroyJobItem(self);
	PafDelete(self);
}

typedef struct PafJobWaitState {
	uint8_t reserved[12];
	uint8_t busy;
	uint8_t reserved2[19];
	uint32_t queued;
} PafJobWaitState;

typedef struct PafJobThreadOwner {
	uint8_t reserved[16];
	JobQueueNode *root;
} PafJobThreadOwner;

typedef struct PafJobCancelOwner {
	uint8_t reserved[28];
	JobQueueNode *root;
} PafJobCancelOwner;

_Static_assert(offsetof(PafJobWaitState, busy) == 12,
	       "job busy flag offset");
_Static_assert(offsetof(PafJobWaitState, queued) == 32,
	       "job queued count offset");
_Static_assert(offsetof(PafJobThreadOwner, root) == 16,
	       "job thread list offset");
_Static_assert(offsetof(PafJobCancelOwner, root) == 28,
	       "job cancellation list offset");

__attribute__((noinline)) void ScePafThread_88061E60(PafJobCancelOwner *self)
{
	JobQueueNode *root;
	JobQueueNode *next;
	JobQueueNode *node;

	MutexLock(&mutex);
	root = self->root;
	next = root ? root->next : NULL;
	node = next;
	while (node != self->root) {
		JobItem *job = (JobItem *)node->val;

		job->vtable->Cancel(job);
		node = next;
		if (next) {
			next = next->next;
			node = next;
		}
	}
	MutexUnlock(&mutex);
}

/* Cancel queued jobs across the three native job manager instances. */
PafJobCancelOwner *sce_paf_job_manager_one;
PafJobCancelOwner *sce_paf_job_manager_two;
PafJobCancelOwner *sce_paf_job_manager_three;

void ScePafMisc_E23ADF42(void)
{
	PafJobCancelOwner *owner = sce_paf_job_manager_one;
	if (owner)
		ScePafThread_88061E60(owner);
	owner = sce_paf_job_manager_two;
	if (owner)
		ScePafThread_88061E60(owner);
	owner = sce_paf_job_manager_three;
	if (owner)
		ScePafThread_88061E60(owner);
}

void ScePafThread_789B263C(PafJobWaitState *self)
{
	for (;;) {
		MutexLock(&mutex);
		if (!self->queued && !self->busy)
			break;
		MutexUnlock(&mutex);
		ScePafThread_651F4F81(10);
	}
	MutexUnlock(&mutex);
}

int ScePafThread_A59ABB3B(PafJobWaitState *self)
{
	int count;

	MutexLock(&mutex);
	count = self->queued;
	MutexUnlock(&mutex);
	return count;
}

void ScePafThread_64FF39DE(PafJobThreadOwner *self, int priority)
{
	JobQueueNode *root = self->root;
	JobQueueNode *next = root ? root->next : NULL;
	JobQueueNode *node = next;

	while (node != self->root) {
		ThreadChangePriority((Thread *)node->val, priority);
		node = next;
		if (next) {
			next = next->next;
			node = next;
		}
	}
}

int JobItemIsCanceled(JobItem *self)
{
	return self->m_canceled;
}

void JobItemCancel(JobItem *self)
{
	self->m_canceled = 1;
}

JobQueueList *JobQueueListConstruct(JobQueueList *self, const void *allocator)
{
	JobQueueNode *root;

	self->count = 0;
	/* Native JobQueue roots reserve a 36-byte node/control block. */
	root = PafNew(36);
	if (!root)
		ScePafStdc_D608246A();
	self->root_node = root;
	if (root) {
		root->prev = root;
		root->next = root;
	}
	return self;
}

JobQueueList *JobQueueListDestroy(JobQueueList *self)
{
	JobQueueNode *root = self->root_node;

	if (root) {
		JobQueueNode *node = root->next;

		while (node != root) {
			JobQueueNode *next = node->next;

			PafDelete(node);
			root = self->root_node;
			node = next;
		}
		PafDelete(root);
	}
	self->root_node = (JobQueueNode *)-1;
	self->count = -1;
	return self;
}

void JobInitialize(void)
{
	extern void *__dso_handle;
	unsigned char allocator[4];

	JobQueueListConstruct(&JobQueueQueueList, allocator);
	ScePafStdc_C95732BB(&JobQueueQueueList,
			  (void (*)(void *))JobQueueListDestroy, &__dso_handle);
	MutexConstruct(&mutex, "ScePafJob", MutexOption_None);
	ScePafStdc_C95732BB(&mutex, (void (*)(void *))MutexDestroy, &__dso_handle);
}
