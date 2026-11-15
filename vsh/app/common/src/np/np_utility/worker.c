/*
 * SceNpUtility: the lookup library's worker thread. Asynchronous requests are queued as jobs;
 * the thread wakes on its event flag, runs each job's handler and then its done callback with
 * the result, until it is told to stop.
 */

#include "np_utility_internal.h"

/* the queue of size jobs from the worker's allocator */
static inline int createQueue(NpLookupWorker *worker, SceUInt32 size)
{
	if (worker->alloc == NULL)
		return SCE_NP_UTIL_ERROR_UNKNOWN;
	if (sceKernelLockMutex(worker->mutex, 1, NULL) < 0)
		return SCE_NP_UTIL_ERROR_UNKNOWN;
	worker->queue = worker->alloc(size * sizeof(NpLookupJob));
	worker->queueSize = size;
	worker->count = 0;
	sceKernelUnlockMutex(worker->mutex, 1);
	return 0;
}

static inline void destroyQueue(NpLookupWorker *worker)
{
	if (worker->queue == NULL || worker->free == NULL)
		return;
	if (sceKernelLockMutex(worker->mutex, 1, NULL) < 0)
		return;
	worker->free(worker->queue);
	worker->queue = NULL;
	worker->queueSize = 0;
	worker->count = 0;
	sceKernelUnlockMutex(worker->mutex, 1);
}

/* the first queued job into job, *empty set when there is none */
static __attribute__((noinline)) int dequeue(NpLookupWorker *worker, NpLookupJob *job,
					     int *empty)
{
	SceUInt32 i;

	*empty = 0;
	if (worker == NULL)
		return 0;
	if (sceKernelLockMutex(worker->mutex, 1, NULL) < 0)
		return SCE_NP_UTIL_ERROR_UNKNOWN;

	if (worker->queue != NULL) {
		if (worker->count != 0) {
			*job = worker->queue[0];
			for (i = 0; i < worker->count - 1; i++)
				worker->queue[i] = worker->queue[i + 1];
			worker->count--;
		} else {
			*empty = 1;
		}
	}
	sceKernelUnlockMutex(worker->mutex, 1);
	return 0;
}

/* runs the queued jobs until the queue is empty */
static inline void runJobs(NpLookupWorker *worker)
{
	const NpLookupJobHandler *handler;
	NpLookupJob job;
	int result;
	int empty;
	int i;

	if (worker == NULL)
		return;
	empty = 0;
	if (dequeue(worker, &job, &empty) < 0 || empty != 0)
		return;
	do {
		if (worker->handlerCount == 0)
			break;
		i = 0;
		while (worker->handlers[i].type != job.type)
			i++;
		handler = &worker->handlers[i];
		result = handler->run(worker->thread, job.arg, job.job, job.flags);
		handler->done(worker->thread, job.arg, job.job, job.flags, result);
	} while (dequeue(worker, &job, &empty) >= 0 && empty == 0);
}

static int workerThread(SceSize argSize, void *argBlock)
{
	NpLookupWorker *worker = *(NpLookupWorker **)argBlock;
	SceUInt32 bits;
	int ret;

	if (worker == NULL)
		return 0;
	for (;;) {
		bits = 0;
		ret = sceKernelWaitEventFlag(worker->eventFlag, 0xFFFFFFFF,
					     SCE_KERNEL_EVF_WAITMODE_OR |
					     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &bits, NULL);
		if (ret < 0 && ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			break;
		if (bits & NP_LOOKUP_WORKER_EVENT_STOP)
			break;
		runJobs(worker);
	}
	return ret;
}

int SceNpUtility_6EB4C8FE(NpLookupWorker *worker, int priority, SceSize stackSize,
			  int cpuAffinityMask, SceUInt32 queueSize, void *(*alloc)(SceSize size),
			  void (*free)(void *ptr))
{
	int ret;

	if (worker == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	sceClibMemset(worker, 0, sizeof(*worker));
	worker->thread = -1;
	worker->eventFlag = -1;
	ret = sceKernelCreateMutex("sceNpLookupWorkerThrLock", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0,
				   NULL);
	if (ret >= 0)
		worker->mutex = ret;
	if (ret < 0)
		return ret;

	if (stackSize == 0)
		stackSize = 0x4000;
	ret = sceKernelCreateThread("SceNpLookupWorkerThread", workerThread, priority, stackSize, 0,
				    cpuAffinityMask, NULL);
	if (ret < 0)
		goto error;
	worker->thread = ret;
	ret = sceKernelCreateEventFlag("SceNpLookupWTEventFlag", 0, 0, NULL);
	if (ret < 0)
		goto error;
	worker->eventFlag = ret;
	worker->alloc = alloc;
	worker->free = free;
	ret = createQueue(worker, queueSize);
	if (ret >= 0)
		return ret;

error:
	destroyQueue(worker);
	if (worker->eventFlag != -1)
		sceKernelDeleteEventFlag(worker->eventFlag);
	if (worker->thread != -1)
		sceKernelDeleteThread(worker->thread);
	if (worker->mutex != 0)
		sceKernelDeleteMutex(worker->mutex);
	return ret;
}

/* stops the thread, waiting for it up to a minute, and frees everything */
int SceNpUtility_5631E596(NpLookupWorker *worker)
{
	SceInt32 status;
	SceUInt32 timeout;
	int ret;

	if (worker == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (worker->eventFlag == -1 || worker->thread == -1)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;

	sceKernelSetEventFlag(worker->eventFlag, NP_LOOKUP_WORKER_EVENT_STOP);
	status = 0;
	timeout = 60000000;
	sceKernelWaitThreadEnd(worker->thread, &status, &timeout);
	destroyQueue(worker);
	sceKernelDeleteEventFlag(worker->eventFlag);
	ret = sceKernelDeleteThread(worker->thread);
	sceKernelDeleteMutex(worker->mutex);
	sceClibMemset(worker, 0, sizeof(*worker));
	worker->thread = -1;
	worker->eventFlag = -1;
	return ret;
}

int SceNpUtility_575BBBC4(NpLookupWorker *worker, int count, const NpLookupJobHandler *handlers)
{
	if (worker == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	worker->handlerCount = count;
	worker->handlers = handlers;
	return 0;
}

int SceNpUtility_F73C4411(NpLookupWorker *worker)
{
	NpLookupWorker *arg;
	int ret;

	if (worker == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (worker->eventFlag == -1 || worker->thread == -1)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	arg = worker;
	ret = sceKernelStartThread(worker->thread, sizeof(arg), &arg);
	if (ret >= 0)
		ret = 0;
	return ret;
}

/* queues a job of the handler type and wakes the thread; a full queue stays locked (Sony's) */
int SceNpUtility_44FE1097(NpLookupWorker *worker, int type, int arg, void *job, int flags)
{
	NpLookupJob *entry;
	int ret;

	if (worker == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (worker->eventFlag == -1 || worker->thread == -1)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;

	if (sceKernelLockMutex(worker->mutex, 1, NULL) < 0) {
		ret = SCE_NP_UTIL_ERROR_UNKNOWN;
	} else if (worker->queue != NULL && worker->count < worker->queueSize) {
		entry = &worker->queue[worker->count];
		entry->type = type;
		entry->arg = arg;
		entry->job = job;
		entry->flags = flags;
		worker->count++;
		sceKernelUnlockMutex(worker->mutex, 1);
		ret = 0;
	} else {
		ret = SCE_NP_UTIL_ERROR_INSUFFICIENT;
	}
	if (ret >= 0)
		return sceKernelSetEventFlag(worker->eventFlag, NP_LOOKUP_WORKER_EVENT_JOB);
	return ret;
}
