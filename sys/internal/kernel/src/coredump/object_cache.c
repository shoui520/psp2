/*
 * The object cache: SceCoredumpForDriver_EF20949F is called with each thread
 * the thread manager stops, and records the thread and the object it waits on
 * (with that object's waiting threads) before the threads go away. The
 * oalGet*Info functions look here first. Rings of 192 threads and 192 objects,
 * and 3072 waiting-thread records. The file, type and variable names are not
 * Sony's (unknown).
 */

#include <scetypes.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "oal_internal.h"
#include <sysmem/sysmem.h>

#define COREDUMP_CACHE_THREADS		192
#define COREDUMP_CACHE_OBJECTS		192
#define COREDUMP_CACHE_WAITING_THREADS	3072

/* set on a cached SCE_KERNEL_WAITTYPE_EVENT/MP_EVENTS object that is a timer */
#define COREDUMP_CACHE_TYPE_TIMER	0x10000000U

typedef struct CoredumpCachedThread {
	SceUID pid;
	CoredumpThreadInfo info;
} CoredumpCachedThread;

typedef struct CoredumpCachedObject {
	SceUID pid;
	SceUInt32 type;		/* the wait type, | COREDUMP_CACHE_TYPE_TIMER */
	union {
		struct {
			SceUInt32 size;
			SceUID uid;
		} header;
		CoredumpSemaphoreInfo semaphore;
		CoredumpEventFlagInfo eventFlag;
		CoredumpMutexInfo mutex;
		CoredumpLwMutexInfo lwMutex;
		CoredumpCondInfo cond;
		struct {
			CoredumpLwCondInfo info;
			void *work;
			ThreadmgrLwMutexWork *lwMutexWork;
		} lwCond;
		CoredumpMsgPipeInfo msgPipe;
		CoredumpRWLockInfo rwLock;
		CoredumpSimpleEventInfo simpleEvent;
		CoredumpTimerInfo timer;
	};
	int numWaitThreads;
	ThreadmgrWaitingThread *waitingThreads;
} CoredumpCachedObject;

typedef struct CoredumpCache {
	unsigned int numThreads;
	CoredumpCachedThread threads[COREDUMP_CACHE_THREADS];
	unsigned int numObjects;
	CoredumpCachedObject objects[COREDUMP_CACHE_OBJECTS];
	unsigned int numWaitingThreads;
	ThreadmgrWaitingThread waitingThreads[COREDUMP_CACHE_WAITING_THREADS];
} CoredumpCache;

_Static_assert(sizeof(CoredumpCachedThread) == 204, "cached thread size");
_Static_assert(sizeof(CoredumpCachedObject) == 116, "cached object size");
_Static_assert(sizeof(CoredumpCache) == 0x1B00C, "coredump cache size");

CoredumpCache coredumpCache;

int coredumpMemoryInit(void)
{
	memset(&coredumpCache, 0, sizeof(coredumpCache));
	return 0;
}

int coredumpGetCachedThreadInfo(SceUID pid, SceUID threadId,
	CoredumpThreadInfo *info)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numThreads; i++) {
		if (coredumpCache.threads[i].pid == pid &&
		    coredumpCache.threads[i].info.threadId == threadId) {
			memcpy(info, &coredumpCache.threads[i].info, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedSemaphoreInfo(SceUID pid, SceUID semaId,
	CoredumpSemaphoreInfo *info)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		if ((coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB) ==
			SCE_KERNEL_WAITTYPE_SEMAPHORE &&
		    coredumpCache.objects[i].header.uid == semaId &&
		    coredumpCache.objects[i].pid == pid) {
			memcpy(info, &coredumpCache.objects[i].semaphore, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedEventFlagInfo(SceUID pid, SceUID eventFlagId,
	CoredumpEventFlagInfo *info)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		if ((coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB) ==
			SCE_KERNEL_WAITTYPE_EVENTFLAG &&
		    coredumpCache.objects[i].header.uid == eventFlagId &&
		    coredumpCache.objects[i].pid == pid) {
			__builtin_memcpy(info, &coredumpCache.objects[i].eventFlag, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedMutexInfo(SceUID pid, SceUID mutexId,
	CoredumpMutexInfo *info)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		if ((coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB) ==
			SCE_KERNEL_WAITTYPE_MUTEX &&
		    coredumpCache.objects[i].header.uid == mutexId &&
		    coredumpCache.objects[i].pid == pid) {
			memcpy(info, &coredumpCache.objects[i].mutex, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedLwMutexInfo(SceUID pid, SceUID lwMutexId,
	CoredumpLwMutexInfo *info)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		if ((coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB) ==
			SCE_KERNEL_WAITTYPE_LW_MUTEX &&
		    coredumpCache.objects[i].header.uid == lwMutexId &&
		    coredumpCache.objects[i].pid == pid) {
			__builtin_memcpy(info, &coredumpCache.objects[i].lwMutex, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedCondInfo(SceUID pid, SceUID condId, CoredumpCondInfo *info)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		if ((coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB) ==
			SCE_KERNEL_WAITTYPE_COND_SIGNAL &&
		    coredumpCache.objects[i].header.uid == condId &&
		    coredumpCache.objects[i].pid == pid) {
			__builtin_memcpy(info, &coredumpCache.objects[i].cond, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedLwCondInfo(SceUID pid, SceUID lwCondId,
	CoredumpLwCondInfo *info, void **work, ThreadmgrLwMutexWork **lwMutexWork)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		if ((coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB) ==
			SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL &&
		    coredumpCache.objects[i].header.uid == lwCondId &&
		    coredumpCache.objects[i].pid == pid) {
			__builtin_memcpy(info, &coredumpCache.objects[i].lwCond.info, sizeof(*info));
			*work = coredumpCache.objects[i].lwCond.work;
			*lwMutexWork = coredumpCache.objects[i].lwCond.lwMutexWork;
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedMsgPipeInfo(SceUID pid, SceUID msgPipeId,
	CoredumpMsgPipeInfo *info)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		if ((coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB) ==
			SCE_KERNEL_WAITTYPE_MSG_PIPE &&
		    coredumpCache.objects[i].header.uid == msgPipeId &&
		    coredumpCache.objects[i].pid == pid) {
			memcpy(info, &coredumpCache.objects[i].msgPipe, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

/*
 * Sony's searches the message-pipe wait type, so a cached RW lock is never
 * found (the caller then asks the thread manager).
 */
int coredumpGetCachedRWLockInfo(SceUID pid, SceUID rwLockId,
	CoredumpRWLockInfo *info)
{
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		if ((coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB) ==
			SCE_KERNEL_WAITTYPE_MSG_PIPE &&
		    coredumpCache.objects[i].header.uid == rwLockId &&
		    coredumpCache.objects[i].pid == pid) {
			memcpy(info, &coredumpCache.objects[i].rwLock, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedSimpleEventInfo(SceUID pid, SceUID eventId,
	CoredumpSimpleEventInfo *info)
{
	SceUInt32 type;
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		type = coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB;
		if ((type == SCE_KERNEL_WAITTYPE_EVENT ||
		     type == SCE_KERNEL_WAITTYPE_MP_EVENTS) &&
		    coredumpCache.objects[i].header.uid == eventId &&
		    coredumpCache.objects[i].pid == pid) {
			__builtin_memcpy(info, &coredumpCache.objects[i].simpleEvent, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

int coredumpGetCachedTimerInfo(SceUID pid, SceUID timerId,
	CoredumpTimerInfo *info)
{
	SceUInt32 type;
	unsigned int i;

	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		type = coredumpCache.objects[i].type & ~SCE_KERNEL_WAITTYPE_CB;
		if ((type == (COREDUMP_CACHE_TYPE_TIMER | SCE_KERNEL_WAITTYPE_EVENT) ||
		     type == (COREDUMP_CACHE_TYPE_TIMER | SCE_KERNEL_WAITTYPE_MP_EVENTS)) &&
		    coredumpCache.objects[i].header.uid == timerId &&
		    coredumpCache.objects[i].pid == pid) {
			memcpy(info, &coredumpCache.objects[i].timer, sizeof(*info));
			return 0;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
}

/* the wait types whose object the cache can hold, as case labels */
#define CASE_CACHED_OBJECT_TYPES					\
	case SCE_KERNEL_WAITTYPE_EVENTFLAG:			\
	case SCE_KERNEL_WAITTYPE_SEMAPHORE:			\
	case SCE_KERNEL_WAITTYPE_MUTEX:				\
	case SCE_KERNEL_WAITTYPE_RW_LOCK:			\
	case SCE_KERNEL_WAITTYPE_COND_SIGNAL:			\
	case SCE_KERNEL_WAITTYPE_COND_MUTEX:			\
	case SCE_KERNEL_WAITTYPE_EVENT:				\
	case SCE_KERNEL_WAITTYPE_MP_EVENTS:			\
	case SCE_KERNEL_WAITTYPE_MSG_PIPE:			\
	case SCE_KERNEL_WAITTYPE_LW_MUTEX:			\
	case SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL:		\
	case SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX:		\
	case COREDUMP_CACHE_TYPE_TIMER | SCE_KERNEL_WAITTYPE_EVENT:	\
	case COREDUMP_CACHE_TYPE_TIMER | SCE_KERNEL_WAITTYPE_MP_EVENTS:	\
	case SCE_KERNEL_WAITTYPE_EVENTFLAG_CB:			\
	case SCE_KERNEL_WAITTYPE_SEMAPHORE_CB:			\
	case SCE_KERNEL_WAITTYPE_MUTEX_CB:			\
	case SCE_KERNEL_WAITTYPE_RW_LOCK_CB:			\
	case SCE_KERNEL_WAITTYPE_COND_SIGNAL_CB:		\
	case SCE_KERNEL_WAITTYPE_COND_MUTEX_CB:			\
	case SCE_KERNEL_WAITTYPE_EVENT_CB:			\
	case SCE_KERNEL_WAITTYPE_MP_EVENTS_CB:			\
	case SCE_KERNEL_WAITTYPE_MSG_PIPE_CB:			\
	case SCE_KERNEL_WAITTYPE_LW_MUTEX_CB:			\
	case SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL_CB:		\
	case SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX_CB:		\
	case COREDUMP_CACHE_TYPE_TIMER | SCE_KERNEL_WAITTYPE_EVENT_CB:	\
	case COREDUMP_CACHE_TYPE_TIMER | SCE_KERNEL_WAITTYPE_MP_EVENTS_CB

static inline int cachedWaitingThreadCount(SceUID pid, SceUID waitId)
{
	unsigned int i;

	if (waitId == -1)
		return 0;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		switch (coredumpCache.objects[i].type) {
		CASE_CACHED_OBJECT_TYPES:
			if (pid == coredumpCache.objects[i].pid &&
			    waitId == coredumpCache.objects[i].header.uid)
				return coredumpCache.objects[i].numWaitThreads;
			break;
		}
	}
	return 0;
}

int coredumpGetCachedWaitingThreadCount(SceUID pid, SceUID waitId)
{
	return cachedWaitingThreadCount(pid, waitId);
}

int coredumpGetCachedWaitingThreadList(SceUID pid, SceUID waitId,
	CoredumpWaitingThread *list, SceSize capacity)
{
	const ThreadmgrWaitingThread *thread;
	SceSize count, i;

	if (capacity == 0 || list == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	count = cachedWaitingThreadCount(pid, waitId);
	if (count == 0)
		return SCE_ERROR_ERRNO_ENODATA;
	for (i = 0; i < coredumpCache.numObjects; i++) {
		switch (coredumpCache.objects[i].type) {
		CASE_CACHED_OBJECT_TYPES:
			if (pid == coredumpCache.objects[i].pid &&
			    waitId == coredumpCache.objects[i].header.uid)
				goto found;
			break;
		}
	}
	return SCE_ERROR_ERRNO_ENODATA;
found:
	thread = coredumpCache.objects[i].waitingThreads;
	if (capacity < count)
		count = capacity;
	i = 0;
	do {
		list[i].processId = thread->processId;
		list[i].userThreadId = thread->userThreadId;
		thread++;
		i++;
	} while (i < count);
	return 0;
}

/* reads the object of wait type type into object; 0, or -1 when uid is gone */
int coredumpCacheObject(SceUID pid, SceUInt32 type, SceUID uid,
	CoredumpCachedObject *object)
{
	object->pid = pid;
	object->type = type;
	switch (type) {
	case SCE_KERNEL_WAITTYPE_EVENTFLAG:
	case SCE_KERNEL_WAITTYPE_EVENTFLAG_CB:
		oalGetEventFlagInfo(pid, uid, &object->eventFlag);
		break;
	case SCE_KERNEL_WAITTYPE_SEMAPHORE:
	case SCE_KERNEL_WAITTYPE_SEMAPHORE_CB:
		oalGetSemaphoreInfo(pid, uid, &object->semaphore);
		break;
	case SCE_KERNEL_WAITTYPE_MUTEX:
	case SCE_KERNEL_WAITTYPE_COND_MUTEX:
	case SCE_KERNEL_WAITTYPE_MUTEX_CB:
	case SCE_KERNEL_WAITTYPE_COND_MUTEX_CB:
		oalGetMutexInfo(pid, uid, &object->mutex);
		break;
	case SCE_KERNEL_WAITTYPE_RW_LOCK:
	case SCE_KERNEL_WAITTYPE_RW_LOCK_CB:
		oalGetRWLockInfo(pid, uid, &object->rwLock);
		break;
	case SCE_KERNEL_WAITTYPE_COND_SIGNAL:
	case SCE_KERNEL_WAITTYPE_COND_SIGNAL_CB:
		oalGetCondInfo(pid, uid, &object->cond);
		break;
	case SCE_KERNEL_WAITTYPE_EVENT:
	case SCE_KERNEL_WAITTYPE_MP_EVENTS:
	case SCE_KERNEL_WAITTYPE_EVENT_CB:
	case SCE_KERNEL_WAITTYPE_MP_EVENTS_CB:
		oalGetTimerInfo(pid, uid, &object->timer);
		if (object->header.uid == uid) {
			object->type |= COREDUMP_CACHE_TYPE_TIMER;
			return 0;
		}
		oalGetSimpleEventInfo(pid, uid, &object->simpleEvent);
		break;
	case SCE_KERNEL_WAITTYPE_MSG_PIPE:
	case SCE_KERNEL_WAITTYPE_MSG_PIPE_CB:
		oalGetMsgPipeInfo(pid, uid, &object->msgPipe);
		break;
	case SCE_KERNEL_WAITTYPE_LW_MUTEX:
	case SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX:
	case SCE_KERNEL_WAITTYPE_LW_MUTEX_CB:
	case SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX_CB:
		oalGetLwMutexInfo(pid, uid, &object->lwMutex);
		break;
	case SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL:
	case SCE_KERNEL_WAITTYPE_LW_COND_SIGNAL_CB:
		oalGetLwCondInfo(pid, uid, &object->lwCond.info,
				 &object->lwCond.work, &object->lwCond.lwMutexWork);
		break;
	default:
		return -1;
	}
	if (object->header.uid != uid)
		return -1;
	return 0;
}

/* records a stopped thread and the object it waits on */
int SceCoredumpForDriver_EF20949F(const CoredumpThreadSnapshot *snapshot)
{
	SceUInt32 waitType;
	SceUID waitId, guid;
	SceSize retNum;
	unsigned int i;
	int count;

	if (snapshot == NULL)
		return SCE_ERROR_ERRNO_ENOBUFS;
	coredumpCache.threads[coredumpCache.numThreads].pid = snapshot->info.processId;
	coredumpCache.threads[coredumpCache.numThreads].info.threadId =
		snapshot->info.threadId;
	oalFormatThreadInfo(&coredumpCache.threads[coredumpCache.numThreads].info,
			    snapshot);
	guid = SceSysmemForDriver_45D22597(snapshot->info.processId,
		coredumpCache.threads[coredumpCache.numThreads].info.waitId);
	waitId = coredumpCache.threads[coredumpCache.numThreads].info.waitId;
	if (waitId != 0) {
		waitType = coredumpCache.threads[coredumpCache.numThreads].info.waitType;
		for (i = 0; i < coredumpCache.numObjects; i++) {
			if ((coredumpCache.objects[i].type & ~COREDUMP_CACHE_TYPE_TIMER) !=
				waitType)
				continue;
			switch (coredumpCache.objects[i].type) {
			CASE_CACHED_OBJECT_TYPES:
				if (waitId == coredumpCache.objects[i].header.uid)
					goto found;
				break;
			}
		}
		if (coredumpCacheObject(coredumpCache.threads[coredumpCache.numThreads].pid,
			waitType, waitId,
			&coredumpCache.objects[coredumpCache.numObjects]) == 0) {
			coredumpCache.objects[coredumpCache.numObjects].numWaitThreads =
				oalGetNumOfWaitingThreads(
				coredumpCache.threads[coredumpCache.numThreads].pid,
				coredumpCache.threads[coredumpCache.numThreads].info.waitId);
			count = coredumpCache.objects[coredumpCache.numObjects].numWaitThreads;
			if (count != 0 && coredumpCache.numWaitingThreads + count <
				COREDUMP_CACHE_WAITING_THREADS) {
				retNum = 0;
				coredumpCache.objects[coredumpCache.numObjects].waitingThreads =
					&coredumpCache.waitingThreads[coredumpCache.numWaitingThreads];
				if (sceKernelGetWaitingThreadList(guid,
					&coredumpCache.waitingThreads[coredumpCache.numWaitingThreads],
					count * sizeof(ThreadmgrWaitingThread), &retNum) < 0) {
					coredumpCache.objects[coredumpCache.numObjects].numWaitThreads = 0;
					coredumpCache.objects[coredumpCache.numObjects].waitingThreads = NULL;
				} else {
					coredumpCache.numWaitingThreads +=
						coredumpCache.objects[coredumpCache.numObjects].numWaitThreads;
				}
			} else {
				coredumpCache.objects[coredumpCache.numObjects].numWaitThreads = 0;
				coredumpCache.objects[coredumpCache.numObjects].waitingThreads = NULL;
			}
			if (coredumpCache.numObjects + 1 < COREDUMP_CACHE_OBJECTS)
				coredumpCache.numObjects++;
			else
				coredumpCache.numObjects = 0;
		}
	}
found:
	if (coredumpCache.numThreads + 1 < COREDUMP_CACHE_THREADS)
		coredumpCache.numThreads++;
	else
		coredumpCache.numThreads = 0;
	return 0;
}
