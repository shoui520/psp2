#include "job.h"
#include "../thread/mutex.h"
#include "../thread/thread.h"
#include "../std/stdc++/atexit.h"

JobQueueList JobQueueQueueList;
static Mutex mutex;

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

_Static_assert(offsetof(PafJobWaitState, busy) == 12,
	       "job busy flag offset");
_Static_assert(offsetof(PafJobWaitState, queued) == 32,
	       "job queued count offset");
_Static_assert(offsetof(PafJobThreadOwner, root) == 16,
	       "job thread list offset");

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
	root = PafNew(sizeof(*root));
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
