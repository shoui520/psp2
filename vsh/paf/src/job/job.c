#include "job.h"
#include "../thread/mutex.h"
#include "../std/stdc++/atexit.h"

JobQueueList JobQueueQueueList;
static Mutex mutex;

int JobItemIsCanceled(JobItem *self)
{
	return self->m_canceled;
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
