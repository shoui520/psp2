/* Pending calls are handed to the job queue as one reference-counted job. */
#include "job_call_queue.h"
#include <sce_atomic.h>

extern size_t sce_paf_strlen(const char *str);
extern void _ZN3paf6thread5SleepEi(unsigned int milliseconds);

typedef struct CallQueueJobItem {
	JobItem base;
	CallQueue *m_call_queue;
} CallQueueJobItem;

typedef char CallQueueJobItemSizeCheck[sizeof(CallQueueJobItem) == 32 ? 1 : -1];

JobItem *CallQueueJobItemDestroy(JobItem *base);
void CallQueueJobItemDefaultDelete(JobItem *base);
int CallQueueJobItemRun(JobItem *base);
void CallQueueJobItemFinish(JobItem *base, int result);
void CallQueueJobItemDelete(JobItem *base);

static const struct {
	int offset_to_top;
	const void *type_info;
	JobItemVTable methods;
} callQueueJobItemClass = {
	0, NULL, {
		CallQueueJobItemDestroy, CallQueueJobItemDelete, JobItemCancel,
		CallQueueJobItemRun, CallQueueJobItemFinish
	}
};

/* This string expression is inlined in all three queue-name constructions. */
static inline __attribute__((always_inline))
void JobCallQueueName(PafString *prefix, PafString *joined,
		      const char *text, const PafString *name)
{
	PafStringInitialize(prefix);
	PafStringAssign(prefix, text, sce_paf_strlen(text));
	PafStringInitialize(joined);
	PafStringCopy(joined, prefix);
	PafStringAppendBytes(joined, name->ptr, name->count);
}

void JobCallQueueCheck(JobCallQueue *self)
{
	Mutex *mutex = &self->m_mutex;

	_ZN3paf6thread5Mutex4LockEv(mutex);
	if (self->m_call_queue) {
		CallQueueJobItem *job = ScePafStdc_FC5CD359(sizeof(*job));
		JobItemSharedPtr item;

		if (job) {
			CallQueue *body = self->m_call_queue;
			PafString prefix, joined, name;

			JobCallQueueName(&prefix, &joined, "ScePafCallQueueJobItem", &self->m_name);
			PafStringInitialize(&name);
			PafStringCopy(&name, &joined);
			PafStringDestroy(&joined);
			JobItemConstructBase(&job->base, name.ptr, NULL);
			PafStringDestroy(&name);
			PafStringDestroy(&prefix);
			job->m_call_queue = body;
			job->base.vtable = &callQueueJobItemClass.methods;
		}
		item.m_ptr = job ? &job->base : NULL;
		item.m_ref_counter = NULL;
		if (job) {
			JobItemRefCounter *counter = ScePafStdc_FC5CD359(sizeof(*counter));

			item.m_ref_counter = counter;
			if (counter) {
				counter->m_ptr = &job->base;
				counter->m_counter = 1;
				counter->m_weak_counter = 1;
				counter->m_deleter = CallQueueJobItemDefaultDelete;
			} else {
				item.m_ptr = NULL;
			}
		}
		JobQueueEnqueue(self->m_job_queue, &item);
		self->m_call_queue = NULL;
		if (item.m_ptr &&
		    sceAtomicDecrement32AcqRel(&item.m_ref_counter->m_counter) == 1) {
			item.m_ref_counter->m_deleter(item.m_ptr);
			if (sceAtomicDecrement32AcqRel(&item.m_ref_counter->m_weak_counter) == 1)
				ScePafStdc_1B77082E(item.m_ref_counter);
		}
	}
	_ZN3paf6thread5Mutex6UnlockEv(mutex);
}

void JobCallQueueEnqueue(JobCallQueue *self, const char *name,
			 CallQueueFunction entry, void *userdata)
{
	Mutex *mutex = &self->m_mutex;

	_ZN3paf6thread5Mutex4LockEv(mutex);
	if (!self->m_call_queue) {
		CallQueue *body = ScePafStdc_FC5CD359(sizeof(*body));

		if (body) {
			PafString prefix, joined, queue_name;

			JobCallQueueName(&prefix, &joined, "ScePafJobCallQueueBody", &self->m_name);
			PafStringInitialize(&queue_name);
			PafStringCopy(&queue_name, &joined);
			PafStringDestroy(&joined);
			self->m_call_queue = CallQueueConstruct(body, queue_name.ptr);
			PafStringDestroy(&queue_name);
			PafStringDestroy(&prefix);
		} else {
			self->m_call_queue = NULL;
		}
	}
	CallQueueEnqueue(self->m_call_queue, name, entry, userdata);
	_ZN3paf6thread5Mutex6UnlockEv(mutex);
}

int JobCallQueueNumItems(JobCallQueue *self)
{
	int count = 0;

	_ZN3paf6thread5Mutex4LockEv(&self->m_mutex);
	if (self->m_call_queue)
		count = CallQueueNumItems(self->m_call_queue);
	_ZN3paf6thread5Mutex6UnlockEv(&self->m_mutex);
	return count;
}

void JobCallQueueWaitEmpty(JobCallQueue *self)
{
	for (;;) {
		int count = 0;

		_ZN3paf6thread5Mutex4LockEv(&self->m_mutex);
		if (self->m_call_queue)
			count = CallQueueNumItems(self->m_call_queue);
		_ZN3paf6thread5Mutex6UnlockEv(&self->m_mutex);
		if (count <= 0)
			return;
		_ZN3paf6thread5SleepEi(10);
	}
}

JobCallQueue *JobCallQueueConstruct(JobCallQueue *self, const PafString *name,
				   JobQueue *job_queue)
{
	PafString prefix, joined, mutex_name;

	PafStringInitialize(&self->m_name);
	PafStringCopy(&self->m_name, name);
	self->m_job_queue = job_queue;
	JobCallQueueName(&prefix, &joined, "ScePafJobCallQueueMutex", name);
	PafStringInitialize(&mutex_name);
	PafStringCopy(&mutex_name, &joined);
	PafStringDestroy(&joined);
	_ZN3paf6thread5MutexC1EPKci(&self->m_mutex, mutex_name.ptr, 0);
	PafStringDestroy(&mutex_name);
	PafStringDestroy(&prefix);
	self->m_call_queue = NULL;
	return self;
}

JobCallQueue *JobCallQueueConstructBase(JobCallQueue *self, const PafString *name,
				       JobQueue *job_queue)
{
	return JobCallQueueConstruct(self, name, job_queue);
}

JobCallQueue *JobCallQueueDestroy(JobCallQueue *self)
{
	if (self->m_call_queue)
		ScePafStdc_1B77082E(CallQueueDestroy(self->m_call_queue));
	_ZN3paf6thread5MutexD1Ev(&self->m_mutex);
	PafStringDestroy(&self->m_name);
	return self;
}

JobCallQueue *JobCallQueueDestroyBase(JobCallQueue *self)
{
	if (self->m_call_queue)
		ScePafStdc_1B77082E(CallQueueDestroy(self->m_call_queue));
	_ZN3paf6thread5MutexD1Ev(&self->m_mutex);
	PafStringDestroy(&self->m_name);
	return self;
}

JobItem *CallQueueJobItemDestroy(JobItem *base)
{
	base->vtable = &callQueueJobItemClass.methods;
	JobItemDestroyBase(base);
	return base;
}

void CallQueueJobItemDefaultDelete(JobItem *base)
{
	if (base)
		base->vtable->Delete(base);
}

int CallQueueJobItemRun(JobItem *base)
{
	CallQueueJobItem *self = (CallQueueJobItem *)base;

	CallQueueCheck(self->m_call_queue);
	if (self->m_call_queue)
		ScePafStdc_1B77082E(CallQueueDestroy(self->m_call_queue));
	return 0;
}

void CallQueueJobItemFinish(JobItem *base, int result)
{
}

void CallQueueJobItemDelete(JobItem *base)
{
	base->vtable = &callQueueJobItemClass.methods;
	JobItemDestroyBase(base);
	ScePafStdc_1B77082E(base);
}
