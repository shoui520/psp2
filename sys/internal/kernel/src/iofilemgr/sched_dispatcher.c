/*
 * SceIofilemgr: the dispatcher of the I/O scheduler (Sony's sched_dispatcher.c).
 *
 * One scheduler serves each kind of device; the dispatcher hands a request to
 * the scheduler of its device's mount option. The schedulers' worker threads
 * wait on the dispatcher's event flag: the low byte holds a bit per scheduler
 * with work, the high byte a bit per scheduler another worker may help.
 */

#include "iofilemgr_internal.h"

SchedDispatcher g_schedDispatcher;
int g_ioSchedEnabled;

/* the dispatcher's event flag (names unknown) */
#define SCHED_EVENT_OWN_MASK		0xff		/* work for the waiting worker's scheduler */
#define SCHED_EVENT_HELP_MASK		0xff00		/* work any worker may take */
#define SCHED_EVENT_EXIT		0x10000

/* the workers that may run one scheduler's requests at once */
#define SCHED_WORKER_MAX		2

/* a weight with this bit holds an fs resource (name unknown) */
#define SCHED_FS_RESOURCE_COUNTED	0x10000

/* the default chunk size of the schedulers that split reads and writes */
#define SCHED_CHUNK_SIZE		0x40000

int sceIoSchedDispatcherInitializeSchedDispatcher(SchedDispatcher *dispatcher)
{
	IoSchedulerAttr internal;
	IoSchedulerAttr gameCard;
	IoSchedulerAttr removable;
	IoSchedulerAttr host;
	IoSchedulerAttr other;
	int ret;

	ret = ksceKernelInitializeFastMutex(&dispatcher->mutex, "SceIoSchedulerDispatcher",
					    SCE_KERNEL_MUTEX_ATTR_RECURSIVE | 0x2000, 0);
	SCE_DBG_ASSERT(ret == 0, 52);
	dispatcher->evf = ksceKernelCreateEventFlag("SceIoSchedulerDispatcher", 0x1000, 0, NULL);
	SCE_DBG_ASSERT(dispatcher->evf > 0, 60);

	__builtin_memset(&gameCard, 0, sizeof(gameCard));
	__builtin_memset(&removable, 0, sizeof(removable));
	__builtin_memset(&host, 0, sizeof(host));
	__builtin_memset(&other, 0, sizeof(other));
	internal.type = IO_SCHED_TYPE_INTERNAL;
	internal.name = "Internal";
	internal.readChunkSize = SCHED_CHUNK_SIZE;
	internal.writeChunkSize = SCHED_CHUNK_SIZE;
	dispatcher->internal = sceIoSchedulerCreateIoScheduler(dispatcher, &internal);
	if (dispatcher->internal == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	gameCard.type = IO_SCHED_TYPE_GAME_CARD;
	gameCard.name = "Game Card";
	gameCard.readChunkSize = SCHED_CHUNK_SIZE;
	gameCard.writeChunkSize = SCHED_CHUNK_SIZE;
	dispatcher->gameCard = sceIoSchedulerCreateIoScheduler(dispatcher, &gameCard);
	if (dispatcher->gameCard == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	removable.type = IO_SCHED_TYPE_REMOVABLE;
	removable.name = "Removable";
	removable.readChunkSize = SCHED_CHUNK_SIZE;
	removable.writeChunkSize = SCHED_CHUNK_SIZE;
	dispatcher->removable = sceIoSchedulerCreateIoScheduler(dispatcher, &removable);
	if (dispatcher->removable == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	host.type = IO_SCHED_TYPE_HOST;
	host.name = "Host File System";
	host.readChunkSize = 0;
	host.writeChunkSize = 0;
	dispatcher->host = sceIoSchedulerCreateIoScheduler(dispatcher, &host);
	if (dispatcher->host == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	other.type = 0;
	other.name = "Default";
	other.readChunkSize = 0;
	other.writeChunkSize = 0;
	dispatcher->other = sceIoSchedulerCreateIoScheduler(dispatcher, &other);
	if (dispatcher->other == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	dispatcher->fsResourceNum = 0;
	dispatcher->fsResource = 0;
	dispatcher->chunkAlignmentOverride = 0;
	dispatcher->timeout = 10000000;
	g_ioSchedEnabled = 1;
	return ret;
}

/* name unknown: the scheduler of a mount option */
IoScheduler *sceIoSchedDispatcherGetIoScheduler(SchedDispatcher *dispatcher, int mntOpt)
{
	if (mntOpt >= 0)
		mntOpt &= 0xffff;

	switch (mntOpt) {
	case IO_SCHED_TYPE_INTERNAL:
		return dispatcher->internal;
	case IO_SCHED_TYPE_GAME_CARD:
		return dispatcher->gameCard;
	case IO_SCHED_TYPE_REMOVABLE:
		return dispatcher->removable;
	case IO_SCHED_TYPE_GAME_CARD2:
		return dispatcher->gameCard;
	case IO_SCHED_TYPE_HOST:
		return dispatcher->host;
	default:
		return dispatcher->other;
	}
}

/* name unknown: the scheduler a worker's event bits are for */
#if FW_VERSION < 0x04000000
static inline __attribute__((always_inline))
#endif
IoScheduler *sceIoSchedDispatcherGetIoSchedulerByEvent(SchedDispatcher *dispatcher,
						      SceUInt32 bits)
{
	if (bits & 0x101)
		return dispatcher->internal;
	if (bits & 0x202)
		return dispatcher->gameCard;
	if (bits & 0x404)
		return dispatcher->removable;
	if (bits & 0x808)
		return dispatcher->host;
	return dispatcher->other;
}

int sceIoSchedDispatcherDispatchIoRequest(SchedDispatcher *dispatcher, IoRequest *req)
{
	IoScheduler *sched;

	sched = sceIoSchedDispatcherGetIoScheduler(dispatcher, req->mntOpt);
	if (sched != NULL) {
		SCE_DBG_ASSERT(req->next == NULL, 301);
		if (req->isAsync == 0)
			return sceIoSchedulerExecuteSyncIoRequest(sched, req);
		sceIoSchedulerAddAsyncIoRequest(sched, req);
		return 0;
	}
	SCE_DBG_ASSERT(0, 312);
	return 0;
}

int sceIoSchedDispatcherRemoveIoRequest(SchedDispatcher *dispatcher, IoRequest *req)
{
	IoScheduler *sched;

	sched = sceIoSchedDispatcherGetIoScheduler(dispatcher, req->mntOpt);
	SCE_DBG_ASSERT(sched != NULL, 417);
	return sceIoSchedulerRemoveIoRequest(sched, req);
}

/* cancel the asynchronous requests of a process that ends */
int sceIoSchedDispatcherFreeAllAsyncEvents(SchedDispatcher *dispatcher, SceUID pid)
{
	IoAsyncEvent *event = NULL;
	IoAsyncEvent *current;
	SceUID list[8];
	SceSize num = 0;
	SceUID guid;
	int freed, i, ret;

	for (;;) {
		ret = SceSysmemForKernel_08C05493(pid, &g_ioAsyncEventClass, 7, list, 8, &num);
		if (ret < 0)
			return ret;

		freed = 0;
		if ((int)num > 0) {
			for (i = 0; i < (int)num; i++) {
				guid = kscePUIDtoGUID(pid, list[i]);
				if (guid < 0)
					continue;
				ret = ksceGUIDReferObjectWithClass(guid, &g_ioAsyncEventClass,
								   (SceObjectBase **)&event);
				if (ret < 0)
					continue;
				current = event;
				SCE_DBG_ASSERT(event->request.isAsync == 1, 369);
				if (current->request.isCanceled != 0 ||
				    (ret = sceIoSchedDispatcherRemoveIoRequest(dispatcher, &current->request)) >= 0) {
					freed++;
					ksceGUIDReleaseObject(guid);
					ksceGUIDClose(guid);
					ret = kscePUIDClose(pid, list[i]);
				}
			}
			if (num == 8)
				continue;
		}
		if (freed != (int)num)
			sceKernelDelayThread(1000);
		if ((int)num < 1)
			return ret;
	}
}

int sceIoSchedDispatcherFsResourceAvailable(SchedDispatcher *dispatcher, SceUInt32 weight)
{
	int ret;

	ret = ksceKernelLockFastMutex(&dispatcher->mutex);
	SCE_DBG_ASSERT(ret == 0, 426);
	if (weight & SCHED_FS_RESOURCE_COUNTED)
		dispatcher->fsResourceNum++;
	dispatcher->fsResource |= weight;
	ret = ksceKernelUnlockFastMutex(&dispatcher->mutex);
	SCE_DBG_ASSERT(ret == 0, 435);
	return 0;
}

int sceIoSchedDispatcherFsResourceUnavailable(SchedDispatcher *dispatcher, SceUInt32 weight)
{
	int ret;

	ret = ksceKernelLockFastMutex(&dispatcher->mutex);
	SCE_DBG_ASSERT(ret == 0, 444);
	if (weight & SCHED_FS_RESOURCE_COUNTED) {
		SCE_DBG_ASSERT(dispatcher->fsResourceNum > 0, 447);
		dispatcher->fsResourceNum--;
	}
	dispatcher->fsResource &= ~weight;
	ret = ksceKernelUnlockFastMutex(&dispatcher->mutex);
	SCE_DBG_ASSERT(ret == 0, 456);
	return 0;
}

/* name unknown: let the scheduler of a weight issue its next request */
int sceIoSchedDispatcherIssueAnotherIoRequest(SchedDispatcher *dispatcher, SceInt32 weight)
{
	return sceIoSchedulerIssueAnotherIoRequestInternal(
		sceIoSchedDispatcherGetIoScheduler(dispatcher, weight));
}

void sceIoSchedDispatcherWorker(SchedDispatcher *dispatcher, IoScheduler *sched)
{
	IoScheduler *target = NULL;
	SceUInt32 bits;
	int ret;

	for (;;) {
		bits = 0;
		ret = ksceKernelWaitEventFlag(dispatcher->evf, sched->eventBits | SCHED_EVENT_HELP_MASK,
					      SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, &bits, NULL);
		SCE_DBG_ASSERT(ret == 0, 508);
		if (bits & SCHED_EVENT_EXIT)
			break;

		if (bits & SCHED_EVENT_OWN_MASK) {
			/* both kinds of work: leave one for another worker */
			if (bits & SCHED_EVENT_HELP_MASK) {
				if (bits == sched->eventBits) {
					ret = ksceKernelSetEventFlag(dispatcher->evf,
								     bits & SCHED_EVENT_HELP_MASK);
					SCE_DBG_ASSERT(ret == 0, 526);
					bits &= ~SCHED_EVENT_HELP_MASK;
				} else {
					ret = ksceKernelSetEventFlag(dispatcher->evf,
								     bits & SCHED_EVENT_OWN_MASK);
					SCE_DBG_ASSERT(ret == 0, 534);
					bits &= ~SCHED_EVENT_OWN_MASK;
				}
			}
		}

		if (bits & SCHED_EVENT_OWN_MASK) {
			SCE_DBG_ASSERT(sched == sceIoSchedDispatcherGetIoSchedulerByEvent(dispatcher, bits),
				       541);
			getIofilemgrTls()->isCountedWorker = 0;
			target = sched;
		} else if (bits & SCHED_EVENT_HELP_MASK) {
			target = sceIoSchedDispatcherGetIoSchedulerByEvent(dispatcher, bits);
			SCE_DBG_ASSERT(target != NULL, 550);
			ret = ksceKernelLockFastMutex(&dispatcher->mutex);
			SCE_DBG_ASSERT(ret == 0, 553);
			if (target->workerNum >= SCHED_WORKER_MAX) {
				getIofilemgrTls()->isCountedWorker = 0;
				ret = ksceKernelUnlockFastMutex(&dispatcher->mutex);
				SCE_DBG_ASSERT(ret == 0, 563);
				continue;
			}
			target->workerNum++;
			getIofilemgrTls()->isCountedWorker = 1;
			ret = ksceKernelUnlockFastMutex(&dispatcher->mutex);
			SCE_DBG_ASSERT(ret == 0, 572);
		} else {
			SCE_DBG_ASSERT(0, 574);
		}

		if (sceIoSchedulerExecuteIoRequestWorkerForDispatcher(target) == 1)
			break;

		ret = ksceKernelLockFastMutex(&dispatcher->mutex);
		SCE_DBG_ASSERT(ret == 0, 584);
		if (getIofilemgrTls()->isCountedWorker == 1) {
			SCE_DBG_ASSERT(target->workerNum > 0, 587);
			target->workerNum--;
			getIofilemgrTls()->isCountedWorker = 0;
		}
		ret = ksceKernelUnlockFastMutex(&dispatcher->mutex);
		SCE_DBG_ASSERT(ret == 0, 593);
	}
}
