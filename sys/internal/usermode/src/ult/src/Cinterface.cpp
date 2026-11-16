/*
 * Cinterface.cpp: the library's C interface (SceUlt's exports) over its C++ objects, and its
 * initialization.
 */
#include "ult_internal.h"
#include "Queue.h"
#include "Rwlock.h"
#include "Mutex.h"
#include "Ulthread.h"

/* the revision of the objects' layout the kernel's ulobjmgr reads (the debugger's view) */
#define ULT_PROTOCOL_REVISION	0x02100002

extern "C" {

int32_t _sceUltWaitingQueueResourcePoolOptParamInitialize(
	SceUltWaitingQueueResourcePoolOptParam *optParam, uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int32_t _sceUltQueueDataResourcePoolOptParamInitialize(
	SceUltQueueDataResourcePoolOptParam *optParam, uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int32_t _sceUltConditionVariableOptParamInitialize(SceUltConditionVariableOptParam *optParam,
						    uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int32_t _sceUltMutexOptParamInitialize(SceUltMutexOptParam *optParam, uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	optParam->attribute = 0;
	return SCE_OK;
}

int _sceUltQueueOptParamInitialize(SceUltQueueOptParam *optParam, uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int32_t _sceUltReaderWriterLockOptParamInitialize(SceUltReaderWriterLockOptParam *optParam,
						   uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int _sceUltSemaphoreOptParamInitialize(SceUltSemaphoreOptParam *optParam, uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int32_t _sceUltUlthreadRuntimeOptParamInitialize(SceUltUlthreadRuntimeOptParam *optParam,
						  uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	optParam->workerThreadPriority = ULT_DEFAULT_WORKER_THREAD_PRIORITY;
	optParam->oneShotThreadStackSize = ULT_DEFAULT_ONE_SHOT_THREAD_STACK_SIZE;
	optParam->workerThreadCpuAffinityMask = ULT_DEFAULT_WORKER_THREAD_CPU_AFFINITY_MASK;
	optParam->workerThreadAttr = 0;
	optParam->workerThreadOptParam = NULL;
	return SCE_OK;
}

int32_t _sceUltUlthreadOptParamInitialize(SceUltUlthreadOptParam *optParam, uint32_t buildVersion)
{
	int ret = initializeOptParam(optParam, buildVersion);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

/*
 * The module's start and stop (prx_start_stop.c): the kernel learns the revision of the
 * objects' layout its debugger support reads.
 */
int ultInitialize(void)
{
	_sceUlobjMgrRegisterLibultProtocolRevision(ULT_PROTOCOL_REVISION);
	initializeInternal();
	return SCE_OK;
}

int ultFinalize(void)
{
	finalizeInternal();
	return SCE_OK;
}

/* the work areas: a dummy and an entry per thread and per object, and the data */
uint32_t sceUltWaitingQueueResourcePoolGetWorkAreaSize(uint32_t numThreads, uint32_t numSyncObjects)
{
	return sizeof(NBQueueEntry2) * (numThreads + numSyncObjects) + 2 * sizeof(NBQueueEntry2);
}

/* SceUltWaitingQueueResourcePool */
int _sceUltWaitingQueueResourcePoolCreate(SceUltWaitingQueueResourcePool *pool, const char *name,
					  uint32_t numThreads, uint32_t numSyncObjects,
					  void *workArea,
					  const SceUltWaitingQueueResourcePoolOptParam *optParam,
					  uint32_t buildVersion)
{
	WaitingQueueResourcePoolObject *obj = (WaitingQueueResourcePoolObject *)pool;

	if (obj == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (name == NULL)
		return SCE_ULT_ERROR_NULL;
	if (optParam != NULL) {
		int ret = checkOptParam(optParam);
		if (ret != SCE_OK)
			return ret;
	}
	int ret = obj->internal.initialize(numThreads, numSyncObjects, workArea);
	if (ret == SCE_OK) {
		int ret2 = setName(obj->header.name, name);
		if (ret2 != SCE_OK)
			return ret2;
		obj->header.version = ULT_OBJECT_VERSION;
		obj->header.type = ULT_OBJECT_TYPE_WAITING_QUEUE_RESOURCE_POOL;
		obj->numThreads = numThreads;
		obj->numSyncObjects = numSyncObjects;
		SceUlobjDbg_D7F0F610(obj, ULOBJ_DBG_KIND_SYNC_OBJECT, &obj->ulobjId);
	}
	return ret;
}

int sceUltWaitingQueueResourcePoolDestroy(SceUltWaitingQueueResourcePool *pool)
{
	WaitingQueueResourcePoolObject *obj = (WaitingQueueResourcePoolObject *)pool;

	if (obj == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj))
		return SCE_ULT_ERROR_ALIGNMENT;
	int ret = obj->internal.destroy();
	if (ret != SCE_OK)
		return ret;
	SceUlobjDbg_F9C0F5DA(obj->ulobjId);
	return SCE_OK;
}

int32_t sceUltGetWaitingQueueResourcePoolInfo(const SceUltWaitingQueueResourcePool *pool,
					      SceUltWaitingQueueResourcePoolInfo *pInfo)
{
	const WaitingQueueResourcePoolObject *obj = (const WaitingQueueResourcePoolObject *)pool;

	if (obj == NULL || pInfo == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj) || !isAligned8(pInfo))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (pInfo->size != sizeof(*pInfo))
		return SCE_ULT_ERROR_INVALID;
	sceClibMemset(pInfo, 0, sizeof(*pInfo));
	pInfo->waitingQueueResourcePoolId = (uint32_t)obj;
	pInfo->size = sizeof(*pInfo);
	getName(obj->header.name, pInfo->name);
	pInfo->maxNumSyncObjects = obj->numSyncObjects;
	pInfo->numThreads = 0;
	pInfo->maxNumThreads = obj->numThreads;
	/* what is free: the entries in the free lists */
	int32_t ret = getWaitThreadInfo(obj->internal.m_waiterQueue.headAddress(), NULL, 0);
	if (ret < 0)
		return ret;
	pInfo->numThreads = ret;
	pInfo->numSyncObjects = 0;
	ret = getWaitThreadInfo(obj->internal.m_syncObjectQueue.headAddress(), NULL, 0);
	if (ret < 0)
		return ret;
	pInfo->numSyncObjects = ret;
	return SCE_OK;
}

/* SceUltQueueDataResourcePool */
uint32_t sceUltQueueDataResourcePoolGetWorkAreaSize(uint32_t numData, uint32_t dataSize,
						     uint32_t numQueueObject)
{
	return sizeof(NBQueueEntry2) * (numData + numQueueObject) + dataSize * numData +
	       2 * sizeof(NBQueueEntry2);
}

int _sceUltQueueDataResourcePoolCreate(SceUltQueueDataResourcePool *pool, const char *name,
				       uint32_t numData, uint32_t dataSize, uint32_t numQueueObject,
				       SceUltWaitingQueueResourcePool *waitingQueueResourcePool,
				       void *workArea, const SceUltQueueDataResourcePoolOptParam *optParam,
				       uint32_t buildVersion)
{
	QueueDataResourcePoolObject *obj = (QueueDataResourcePoolObject *)pool;

	if (obj == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj))
		return SCE_ULT_ERROR_ALIGNMENT;
	int ret = obj->internal.initialize(name, numData, dataSize, numQueueObject,
					   waitingQueueResourcePool, workArea, optParam, buildVersion);
	if (ret == SCE_OK) {
		int ret2 = setName(obj->header.name, name);
		if (ret2 != SCE_OK)
			return ret2;
		obj->header.version = ULT_OBJECT_VERSION;
		obj->header.type = ULT_OBJECT_TYPE_QUEUE_DATA_RESOURCE_POOL;
		obj->numData = numData;
		obj->numQueueObject = numQueueObject;
		obj->waitingQueueResourcePool = waitingQueueResourcePool;
		SceUlobjDbg_D7F0F610(obj, ULOBJ_DBG_KIND_SYNC_OBJECT, &obj->ulobjId);
	}
	return ret;
}

int sceUltQueueDataResourcePoolDestroy(SceUltQueueDataResourcePool *pool)
{
	QueueDataResourcePoolObject *obj = (QueueDataResourcePoolObject *)pool;

	if (obj == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj))
		return SCE_ULT_ERROR_ALIGNMENT;
	int ret = obj->internal.destroy();
	if (ret == SCE_OK)
		SceUlobjDbg_F9C0F5DA(obj->ulobjId);
	return ret;
}

int32_t sceUltGetQueueDataResourcePoolInfo(const SceUltQueueDataResourcePool *pool,
					   SceUltQueueDataResourcePoolInfo *pInfo)
{
	const QueueDataResourcePoolObject *obj = (const QueueDataResourcePoolObject *)pool;

	if (obj == NULL || pInfo == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj) || !isAligned8(pInfo))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (pInfo->size != sizeof(*pInfo))
		return SCE_ULT_ERROR_INVALID;
	sceClibMemset(pInfo, 0, sizeof(*pInfo));
	pInfo->queueDataResourcePoolId = (uint32_t)obj;
	pInfo->size = sizeof(*pInfo);
	getName(obj->header.name, pInfo->name);
	pInfo->waitingQueueResourcePoolId = (uint32_t)obj->waitingQueueResourcePool;
	pInfo->dataSize = obj->internal.m_dataSize;
	pInfo->maxNumQueueObjects = obj->numQueueObject;
	pInfo->numData = 0;
	pInfo->maxNumData = obj->numData;
	int32_t ret = getWaitThreadInfo(obj->internal.m_dataQueue.headAddress(), NULL, 0);
	if (ret < 0)
		return ret;
	pInfo->numData = ret;
	pInfo->numQueueObjects = 0;
	ret = getWaitThreadInfo(obj->internal.m_queueQueue.headAddress(), NULL, 0);
	if (ret < 0)
		return ret;
	pInfo->numQueueObjects = ret;
	return SCE_OK;
}

/* SceUltQueue */
int _sceUltQueueCreate(SceUltQueue *queue, const char *name, uint32_t dataSize,
		       SceUltWaitingQueueResourcePool *waitingQueueResourcePool,
		       SceUltQueueDataResourcePool *queueDataResourcePool,
		       const SceUltQueueOptParam *optParam, uint32_t buildVersion)
{
	Queue *obj = (Queue *)queue;

	int ret = obj->initialize(name, dataSize, internalOf(waitingQueueResourcePool),
				  internalOf(queueDataResourcePool), optParam, false, buildVersion);
	if (ret == SCE_OK) {
		obj->queueDataResourcePool = queueDataResourcePool;
		obj->waitingQueueResourcePool = waitingQueueResourcePool;
	}
	return ret;
}

int sceUltQueuePush(SceUltQueue *queue, const void *data)
{
	if (queue == NULL)
		return SCE_ULT_ERROR_NULL;
	return ((Queue *)queue)->push(data);
}

int sceUltQueueTryPush(SceUltQueue *queue, const void *data)
{
	if (queue == NULL)
		return SCE_ULT_ERROR_NULL;
	return ((Queue *)queue)->tryPush(data);
}

int sceUltQueuePop(SceUltQueue *queue, void *data)
{
	if (queue == NULL)
		return SCE_ULT_ERROR_NULL;
	return ((Queue *)queue)->pop(data);
}

int sceUltQueueTryPop(SceUltQueue *queue, void *data)
{
	if (queue == NULL)
		return SCE_ULT_ERROR_NULL;
	return ((Queue *)queue)->tryPop(data);
}

/* a user's queue leaves the debugger's table (an ulthread runtime's own were never in it) */
int sceUltQueueDestroy(SceUltQueue *queue)
{
	int ret = ((Queue *)queue)->destroy();
	if (ret != SCE_OK)
		return ret;
	SceUlobjDbg_F9C0F5DA(((Queue *)queue)->ulobjId);
	return SCE_OK;
}

int32_t sceUltGetQueueInfo(const SceUltQueue *queue, SceUltQueueInfo *info,
			   SceUltWaitThreadInfo *waitThreadInfoList, uint32_t numWaitThreadInfo)
{
	const Queue *obj = (const Queue *)queue;

	if (obj == NULL || info == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj) || !isAligned8(info) || !isAligned8(waitThreadInfoList))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (info->size != sizeof(*info))
		return SCE_ULT_ERROR_INVALID;
	int32_t ret = checkWaitThreadInfoList(waitThreadInfoList, numWaitThreadInfo);
	if (ret < 0)
		return ret;
	sceClibMemset(info, 0, sizeof(*info));
	info->queueId = (uint32_t)obj;
	info->size = sizeof(*info);
	getName(obj->header.name, info->name);
	SceUltQueueDataResourcePool *queueDataResourcePool = obj->queueDataResourcePool;
	info->waitingQueueResourcePoolId = (uint32_t)obj->waitingQueueResourcePool;
	info->queueDataResourcePoolId = (uint32_t)queueDataResourcePool;
	info->dataSize = obj->m_dataSize;

	/* the data queued: the entries behind the head's dummy */
	uint32_t numData = 0;
	uint32_t entry = obj->m_queue.headAddress() & ~7u;
	if (entry != 0)
		entry = nextAddress((const NBQueueEntry2 *)entry) & ~7u;
	for (; entry != 0; entry = nextAddress((const NBQueueEntry2 *)entry) & ~7u)
		numData++;
	info->numData = numData;

	/* the pushes wait on the pool's free data, the pops on the queue's */
	int32_t numPush;
	if (queueDataResourcePool != NULL) {
		numPush = getWaitThreadInfoByCause(
			internalOf(queueDataResourcePool)->m_semaphore.headAddress(),
			waitThreadInfoList, numWaitThreadInfo, obj);
		if (numPush < 0)
			return numPush;
	} else {
		numPush = 0;
	}
	info->numPushWaitThreads = numPush;
	uint32_t n = 0;
	if (waitThreadInfoList != NULL) {
		n = (uint32_t)numPush > numWaitThreadInfo ? numWaitThreadInfo : numPush;
		for (int32_t i = n; i > 0; i--)
			waitThreadInfoList[n - i].operation = SCE_ULT_THREAD_OPERATION_QUEUE_PUSH;
		ret = getWaitThreadInfo(obj->m_semaphore.headAddress(), waitThreadInfoList + n,
					numWaitThreadInfo - n);
	} else {
		ret = getWaitThreadInfo(obj->m_semaphore.headAddress(), NULL, 0);
	}
	if (ret < 0)
		return ret;
	info->numPopWaitThreads = ret;
	if (waitThreadInfoList != NULL) {
		int32_t m = (uint32_t)ret > numWaitThreadInfo ? numWaitThreadInfo : ret;
		for (SceUltWaitThreadInfo *p = waitThreadInfoList + n; m > 0; m--, p++)
			p->operation = SCE_ULT_THREAD_OPERATION_QUEUE_POP;
	}
	return SCE_OK;
}

/* SceUltReaderWriterLock */
int _sceUltReaderWriterLockCreate(SceUltReaderWriterLock *rwlock, const char *name,
				  SceUltWaitingQueueResourcePool *waitingQueueResourcePool,
				  const SceUltReaderWriterLockOptParam *optParam, uint32_t buildVersion)
{
	return ((Rwlock *)rwlock)->create(name, waitingQueueResourcePool, optParam, buildVersion);
}

int sceUltReaderWriterLockLockRead(SceUltReaderWriterLock *rwlock)
{
	return ((Rwlock *)rwlock)->lock(RWLOCK_OPERATION_READ);
}

int sceUltReaderWriterLockTryLockRead(SceUltReaderWriterLock *rwlock)
{
	return ((Rwlock *)rwlock)->tryLock(RWLOCK_OPERATION_READ);
}

int sceUltReaderWriterLockUnlockRead(SceUltReaderWriterLock *rwlock)
{
	return ((Rwlock *)rwlock)->unlock(RWLOCK_OPERATION_READ);
}

int sceUltReaderWriterLockLockWrite(SceUltReaderWriterLock *rwlock)
{
	return ((Rwlock *)rwlock)->lock(RWLOCK_OPERATION_WRITE);
}

int sceUltReaderWriterLockTryLockWrite(SceUltReaderWriterLock *rwlock)
{
	return ((Rwlock *)rwlock)->tryLock(RWLOCK_OPERATION_WRITE);
}

int sceUltReaderWriterLockUnlockWrite(SceUltReaderWriterLock *rwlock)
{
	return ((Rwlock *)rwlock)->unlock(RWLOCK_OPERATION_WRITE);
}

int sceUltReaderWriterLockDestroy(SceUltReaderWriterLock *rwlock)
{
	return ((Rwlock *)rwlock)->destroy();
}

/* a thread ID as the infos give it: the ulthread's or the kernel thread's */
static inline void describeThread(uint32_t id, uint32_t &threadId, int32_t &threadType)
{
	if (!isKernelThread(id)) {
		threadId = ((const AbstractThread *)id)->id;
		threadType = SCE_ULT_THREAD_TYPE_ULTHREAD;
	} else {
		threadType = SCE_ULT_THREAD_TYPE_KERNEL_THREAD;
		threadId = id >> 1;
	}
}

int32_t sceUltGetReaderWriterLockInfo(const SceUltReaderWriterLock *rwlock,
				      SceUltReaderWriterLockInfo *info,
				      SceUltWaitThreadInfo *waitThreadInfoList, uint32_t numWaitThreadInfo)
{
	const Rwlock *obj = (const Rwlock *)rwlock;

	if (obj == NULL || info == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj) || !isAligned8(info) || !isAligned8(waitThreadInfoList))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (info->size != sizeof(*info))
		return SCE_ULT_ERROR_INVALID;
	int32_t ret = checkWaitThreadInfoList(waitThreadInfoList, numWaitThreadInfo);
	if (ret < 0)
		return ret;
	sceClibMemset(info, 0, sizeof(*info));
	info->rwLockId = (uint32_t)obj;
	info->size = sizeof(*info);
	getName(obj->header.name, info->name);
	info->waitingQueueResourcePoolId = (uint32_t)obj->m_waitingQueueResourcePool;
	if (obj->state()->writer != 0) {
		info->lockStatus = SCE_ULT_READER_WRITER_LOCK_STATUS_WRITER_LOCK;
		info->numLockingReaders = 0;
		describeThread(obj->m_writeOwner, info->writeOwnerId, info->writeOwnerThreadType);
	} else {
		uint32_t readers = obj->state()->readers;
		if (readers != 0) {
			info->numLockingReaders = readers;
			info->lockStatus = SCE_ULT_READER_WRITER_LOCK_STATUS_READER_LOCK;
		} else {
			info->lockStatus = SCE_ULT_READER_WRITER_LOCK_STATUS_UNLOCK;
			info->numLockingReaders = 0;
		}
		info->writeOwnerId = 0;
		info->writeOwnerThreadType = 0;
	}
	info->numWaitThreads = 0;
	ret = getWaitThreadInfo(obj->m_q.headAddress(), waitThreadInfoList, numWaitThreadInfo);
	if (ret < 0)
		return ret;
	info->numWaitThreads = ret;
	return SCE_OK;
}

/* SceUltSemaphore */
struct SemaphoreObject {
	UltObjectHeader header;
	Semaphore semaphore;
	NBQueueEntry2 dummy;
	SceUltWaitingQueueResourcePool *waitingQueueResourcePool;
	uint32_t ulobjId;
};

int _sceUltSemaphoreCreate(SceUltSemaphore *semaphore, const char *name, int32_t numInitialResource,
			   SceUltWaitingQueueResourcePool *waitingQueueResourcePool,
			   const SceUltSemaphoreOptParam *optParam, uint32_t buildVersion)
{
	SemaphoreObject *obj = (SemaphoreObject *)semaphore;

	if (obj == NULL || name == NULL)
		return SCE_ULT_ERROR_NULL;
	int ret = obj->semaphore.initialize(numInitialResource, internalOf(waitingQueueResourcePool),
					    optParam, buildVersion,
					    &obj->dummy);
	if (ret != SCE_OK)
		return ret;
	ret = setName(obj->header.name, name);
	if (ret != SCE_OK)
		return ret;
	obj->header.version = ULT_OBJECT_VERSION;
	obj->header.type = ULT_OBJECT_TYPE_SEMAPHORE;
	obj->waitingQueueResourcePool = waitingQueueResourcePool;
	SceUlobjDbg_D7F0F610(obj, ULOBJ_DBG_KIND_SYNC_OBJECT, &obj->ulobjId);
	return SCE_OK;
}

int sceUltSemaphoreAcquire(SceUltSemaphore *semaphore, int32_t numResource)
{
	SemaphoreObject *obj = (SemaphoreObject *)semaphore;

	if (obj == NULL)
		return SCE_ULT_ERROR_NULL;
	return obj->semaphore.acquire(numResource, obj);
}

int sceUltSemaphoreTryAcquire(SceUltSemaphore *semaphore, int32_t numResource)
{
	SemaphoreObject *obj = (SemaphoreObject *)semaphore;

	if (obj == NULL)
		return SCE_ULT_ERROR_NULL;
	return obj->semaphore.tryAcquire(numResource, obj);
}

int sceUltSemaphoreRelease(SceUltSemaphore *semaphore, int32_t numResource)
{
	SemaphoreObject *obj = (SemaphoreObject *)semaphore;

	if (obj == NULL)
		return SCE_ULT_ERROR_NULL;
	return obj->semaphore.release(numResource, obj);
}

int sceUltSemaphoreDestroy(SceUltSemaphore *semaphore)
{
	SemaphoreObject *obj = (SemaphoreObject *)semaphore;

	if (obj == NULL)
		return SCE_ULT_ERROR_NULL;
	int ret = obj->semaphore.destroy();
	if (ret != SCE_OK)
		return ret;
	SceUlobjDbg_F9C0F5DA(obj->ulobjId);
	return SCE_OK;
}

int32_t sceUltGetSemaphoreInfo(const SceUltSemaphore *semaphore, SceUltSemaphoreInfo *info,
			       SceUltWaitThreadInfo *waitThreadInfoList, uint32_t numWaitThreadInfo)
{
	const SemaphoreObject *obj = (const SemaphoreObject *)semaphore;

	if (obj == NULL || info == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj) || !isAligned8(info) || !isAligned8(waitThreadInfoList))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (info->size != sizeof(*info))
		return SCE_ULT_ERROR_INVALID;
	int32_t ret = checkWaitThreadInfoList(waitThreadInfoList, numWaitThreadInfo);
	if (ret < 0)
		return ret;
	sceClibMemset(info, 0, sizeof(*info));
	info->semaphoreId = (uint32_t)obj;
	info->size = sizeof(*info);
	getName(obj->header.name, info->name);
	info->waitingQueueResourcePoolId = (uint32_t)obj->waitingQueueResourcePool;
	info->numCurrentResource = obj->semaphore.headCount();
	info->numWaitThreads = 0;
	ret = getWaitThreadInfo(obj->semaphore.headAddress(), waitThreadInfoList,
				numWaitThreadInfo);
	if (ret < 0)
		return ret;
	info->numWaitThreads = ret;
	if (waitThreadInfoList != NULL) {
		/* a waiter's count is its request negated */
		int32_t n = (uint32_t)ret > numWaitThreadInfo ? numWaitThreadInfo : ret;
		for (; n > 0; n--, waitThreadInfoList++)
			waitThreadInfoList->operation = -waitThreadInfoList->operation;
	}
	return SCE_OK;
}

/* SceUltMutex */
int32_t _sceUltMutexCreate(SceUltMutex *mutex, const char *name,
			   SceUltWaitingQueueResourcePool *waitingQueueResourcePool,
			   const SceUltMutexOptParam *optParam, uint32_t buildVersion)
{
	return ((Mutex *)mutex)->create(name, waitingQueueResourcePool, optParam, buildVersion);
}

int32_t sceUltMutexLock(SceUltMutex *mutex)
{
	return ((Mutex *)mutex)->lock(true);
}

int32_t sceUltMutexTryLock(SceUltMutex *mutex)
{
	return ((Mutex *)mutex)->lock(false);
}

int32_t sceUltMutexUnlock(SceUltMutex *mutex)
{
	return ((Mutex *)mutex)->unlock();
}

int32_t sceUltMutexDestroy(SceUltMutex *mutex)
{
	return ((Mutex *)mutex)->destroy();
}

int32_t sceUltGetMutexInfo(const SceUltMutex *mutex, SceUltMutexInfo *info,
			   SceUltWaitThreadInfo *waitThreadInfoList, const uint32_t numWaitThreadInfo)
{
	const Mutex *obj = (const Mutex *)mutex;

	if (obj == NULL || info == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj) || !isAligned8(info) || !isAligned8(waitThreadInfoList))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (info->size != sizeof(*info))
		return SCE_ULT_ERROR_INVALID;
	int32_t ret = checkWaitThreadInfoList(waitThreadInfoList, numWaitThreadInfo);
	if (ret < 0)
		return ret;
	sceClibMemset(info, 0, sizeof(*info));
	info->mutexId = (uint32_t)obj;
	info->size = sizeof(*info);
	getName(obj->header.name, info->name);
	SceUltWaitingQueueResourcePool *pool = obj->waitingQueueResourcePool;
	uint32_t ownerId = obj->internalMutex.ownerId;
	info->attribute = obj->attribute;
	info->waitingQueueResourcePoolId = (uint32_t)pool;
	uint32_t head = obj->internalMutex.waitQ.headAddress();
	int32_t threadType;
	if (ownerId != 0) {
		uint32_t id;
		if ((obj->internalMutex.ownerId & 1) != 0) {
			threadType = SCE_ULT_THREAD_TYPE_KERNEL_THREAD;
			id = obj->internalMutex.ownerId >> 1;
		} else {
			threadType = SCE_ULT_THREAD_TYPE_ULTHREAD;
			id = ((const AbstractThread *)obj->internalMutex.ownerId)->id;
		}
		info->currentOwnerId = id;
	} else {
		threadType = 0;
	}
	info->currentOwnerThreadType = threadType;
	info->numWaitThreads = 0;
	info->recursiveLockCount = obj->internalMutex.recursiveLockCount;
	ret = getWaitThreadInfo(head, waitThreadInfoList, numWaitThreadInfo);
	if (ret < 0)
		return ret;
	info->numWaitThreads = ret;
	return SCE_OK;
}

/* SceUltConditionVariable */
int32_t _sceUltConditionVariableCreate(SceUltConditionVariable *conditionVariable,
				       const char *name, SceUltMutex *mutex,
				       const SceUltConditionVariableOptParam *optParam,
				       uint32_t buildVersion)
{
	return ((ConditionVariable *)conditionVariable)->create(name, (Mutex *)mutex, optParam,
								 buildVersion);
}

int32_t sceUltConditionVariableSignal(SceUltConditionVariable *conditionVariable)
{
	return ((ConditionVariable *)conditionVariable)->signal();
}

int32_t sceUltConditionVariableSignalAll(SceUltConditionVariable *conditionVariable)
{
	return ((ConditionVariable *)conditionVariable)->signalAll();
}

int32_t sceUltConditionVariableWait(SceUltConditionVariable *conditionVariable)
{
	return ((ConditionVariable *)conditionVariable)->wait();
}

int32_t sceUltConditionVariableDestroy(SceUltConditionVariable *conditionVariable)
{
	return ((ConditionVariable *)conditionVariable)->destroy();
}

int32_t sceUltGetConditionVariableInfo(const SceUltConditionVariable *conditionVariable,
				       SceUltConditionVariableInfo *info,
				       SceUltWaitThreadInfo *waitThreadInfoList,
				       uint32_t numWaitThreadInfo)
{
	const ConditionVariable *obj = (const ConditionVariable *)conditionVariable;

	if (obj == NULL || info == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(obj) || !isAligned8(info) || !isAligned8(waitThreadInfoList))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (info->size != sizeof(*info))
		return SCE_ULT_ERROR_INVALID;
	int32_t ret = checkWaitThreadInfoList(waitThreadInfoList, numWaitThreadInfo);
	if (ret < 0)
		return ret;
	sceClibMemset(info, 0, sizeof(*info));
	info->cvId = (uint32_t)obj;
	info->size = sizeof(*info);
	getName(obj->header.name, info->name);
	info->mutexId = (uint32_t)obj->mutex;
	info->numWaitThreads = 0;
	ret = getWaitThreadInfo(obj->waiters.headAddress(), waitThreadInfoList, numWaitThreadInfo);
	if (ret < 0)
		return ret;
	info->numWaitThreads = ret;
	return SCE_OK;
}

uint32_t sceUltUlthreadRuntimeGetWorkAreaSize(uint32_t maxNumUlthread, uint32_t numWorkerThread)
{
	return UlthreadRuntime::getWorkAreaSize(maxNumUlthread, numWorkerThread);
}

int32_t _sceUltUlthreadRuntimeCreate(SceUltUlthreadRuntime *runtime, const char *name,
				     uint32_t maxNumUlthread, uint32_t numWorkerThread,
				     void *workArea, const SceUltUlthreadRuntimeOptParam *optParam,
				     uint32_t buildVersion)
{
	return ((UlthreadRuntime *)runtime)->initialize(name, maxNumUlthread, numWorkerThread,
							workArea, optParam, buildVersion);
}

int32_t sceUltUlthreadRuntimeDestroy(SceUltUlthreadRuntime *runtime)
{
	return ((UlthreadRuntime *)runtime)->destroy();
}

int32_t sceUltGetUlthreadRuntimeInfo(const SceUltUlthreadRuntime *runtime,
				     SceUltUlthreadRuntimeInfo *pInfo, SceUltUlthread **ulthreadList,
				     const uint32_t maxNumUlthread, uint32_t *workerThreadIdList,
				     const uint32_t maxNumWorkerThreadId)
{
	return ((const UlthreadRuntime *)runtime)->getInfo(pInfo, ulthreadList, maxNumUlthread,
							    workerThreadIdList,
							    maxNumWorkerThreadId);
}

int32_t _sceUltUlthreadCreate(SceUltUlthread *ulthread, const char *name,
			      SceUltUlthreadEntry entry, uint32_t arg, void *context,
			      uint32_t sizeContext, SceUltUlthreadRuntime *runtime,
			      const SceUltUlthreadOptParam *optParam, uint32_t buildVersion)
{
	return ((Ulthread *)ulthread)->initialize(name, entry, arg, context, sizeContext,
						  (UlthreadRuntime *)runtime, optParam,
						  buildVersion);
}

int32_t sceUltUlthreadJoin(SceUltUlthread *ulthread, int32_t *status)
{
	return ((Ulthread *)ulthread)->join(status);
}

int32_t sceUltUlthreadTryJoin(SceUltUlthread *ulthread, int32_t *status)
{
	return ((Ulthread *)ulthread)->tryJoin(status);
}

/* the current ulthread goes back to the ready ones; a one-shot ulthread cannot */
int32_t sceUltUlthreadYield(void)
{
	Ulthread *self = Ulthread::getSelf();

	if (self == NULL)
		return SCE_ULT_ERROR_PERMISSION;
	if (self->context == NULL)
		return SCE_ULT_ERROR_PERMISSION;
	return self->worker->switchFromUlthread(self, WORKER_REQUEST_YIELD, 0);
}

int32_t sceUltUlthreadExit(int32_t status)
{
	return Ulthread::exit(status);
}

int32_t sceUltUlthreadGetSelf(SceUltUlthread **ulthread)
{
	if (ulthread == NULL)
		return SCE_ULT_ERROR_NULL;
	*ulthread = (SceUltUlthread *)Ulthread::getSelf();
	return SCE_OK;
}

int32_t sceUltGetUlthreadInfo(const SceUltUlthread *ulthread, SceUltUlthreadInfo *pInfo)
{
	return ((const Ulthread *)ulthread)->getInfo(pInfo);
}

}
