/*
 * Ulthread.h: the ulthreads (SceUltUlthread) and their runtime (SceUltUlthreadRuntime).
 *
 * An ulthread is a fiber with an AbstractThread; the runtime's workers run the ready ones.
 * The runtime keeps its ulthreads in a list indexed by their IDs, the free IDs in a queue,
 * and the ready ulthreads in a queue its workers wait on. An ulthread with its own context
 * can yield and wait; one without (a one-shot ulthread) runs on its worker's stack to its
 * end.
 */
#ifndef ULTHREAD_H
#define ULTHREAD_H

#include "Queue.h"
#include "WorkerThread.h"
extern "C" {
#include <libperf/razor.h>
}

#define ULTHREAD_H_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("Ulthread.h", line, text, cond)
#define ULTHREAD_H_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("Ulthread.h", line, text, actual, expected)

/* the Razor capture's packets of the runtimes and ulthreads (libperf's) */
#define RAZOR_ULTHREAD_PACKET		2
#define RAZOR_ULTHREAD_RUNTIME_PACKET	3
#define RAZOR_ULT_CREATE		0x01
#define RAZOR_ULT_DELETE		0x02
#define RAZOR_ULT_WAKEUP		0x05
#define RAZOR_ULT_JOIN			0x07
#define RAZOR_ULT_SWITCH_OUT		0x43
#define RAZOR_ULT_SWITCH_OUT_WAIT	0x44
#define RAZOR_ULT_SWITCH_OUT_EXIT	0x46
#define RAZOR_ULT_SWITCH_IN		0x82
#define RAZOR_ULT_SWITCH_IN_RESUME	0x88

/*
 * &_pLibPerfCaptureFlagPtr, stored and read back so the weak import's address is tested
 * (Ulthread.cpp)
 */
extern const void *volatile gLibPerfCaptureFlag;

#define RAZOR_CAPTURING()						\
	(gLibPerfCaptureFlag = &_pLibPerfCaptureFlagPtr,		\
	 gLibPerfCaptureFlag != NULL && *_pLibPerfCaptureFlagPtr != 0)

/* a packet on an object, with its name (if given) */
ULT_INLINE static inline void razorWrite(uint32_t kind, uint32_t event, const void *object,
					 const char *name)
{
	if (RAZOR_CAPTURING())
		_sceRazorCpuWriteFiberUltPkt(kind, event, name != NULL, object, name);
}

class UlthreadRuntime;

/* an ulthread's state, its signal and the thread waiting for its end, changed together */
union AtomicState {
	struct {
		int16_t state;		/* Ulthread::k* */
		int16_t signal;		/* a signal sent while it was not waiting */
		uint32_t joiner;	/* the thread waiting for its end, INVALID_ID if none */
	} asStr;
	uint64_t asInt;

	int16_t getState() const
	{
		return ((const volatile AtomicState *)this)->asStr.state;
	}
};

class Ulthread {
public:
	enum {
		kInitialized = 0,
		kRunning = 1,
		kExit = 2,
		kJoined = 3,
		kWaiting = 4,
		kReady = 5
	};

	/* setWaiting: the ulthread waits (no signal was pending) */
	enum {
		START_WAITING = 2
	};

	char name[SCE_ULT_MAX_NAME_LENGTH + 1];
	SceFiber fiber;
	AtomicState atomicState;
	SceUltUlthreadEntry entry;
	uint32_t arg;
	int32_t exitStatus;
	WorkerThread *worker;			/* running it, or the last to */
	uint32_t id;				/* its index in the runtime's list */
	void *context;				/* NULL: a one-shot ulthread */
	uint32_t reserved;
	UlthreadRuntime *runtime;
	AbstractThread abstractThread;
	uint32_t reserved2[6];

	/* the callbacks of an ulthread that waits (with a context) and of one that cannot */
	static const AbstractThreadCallbacks s_callbacks;
	static const AbstractThreadCallbacks s_callbacksNoWait;

	AbstractThread *getAbstractThread()
	{
		return &abstractThread;
	}

	__attribute__((noinline)) int initialize(const char *name, SceUltUlthreadEntry entry,
						 uint32_t arg, void *context, uint32_t sizeContext,
						 UlthreadRuntime *runtime,
						 const SceUltUlthreadOptParam *optParam,
						 uint32_t buildVersion);
	__attribute__((noinline)) int join(int32_t *status);
	__attribute__((noinline)) int tryJoin(int32_t *status);
	__attribute__((noinline)) int getInfo(SceUltUlthreadInfo *pInfo) const;

	/* the current ulthread, NULL on any other thread */
	ULT_INLINE static Ulthread *getSelfInline()
	{
		AbstractThread *t = (AbstractThread *)AbstractThread::getID2();

		if (isKernelThread((uint32_t)t))
			return NULL;
		if (t->callbacks == &s_callbacks || t->callbacks == &s_callbacksNoWait)
			return (Ulthread *)t->id;
		return NULL;
	}
	__attribute__((noinline)) static Ulthread *getSelf();
	/* the current ulthread ends with status; SCE_ULT_ERROR_PERMISSION on another thread */
	__attribute__((noinline)) static int exit(int32_t status);

	/* the fiber's entry, and the callbacks */
	__attribute__((noreturn)) static void fiberEntry(SceUInt32 argOnInitialize,
							 SceUInt32 argOnRun);
	static int waitSignal(uint32_t id, void *cause);
	static int sendSignal(uint32_t id);

	/* Ulthread_inline.h: the state's changes */
	ULT_INLINE int setRunning();
	ULT_INLINE int setReady();
	ULT_INLINE int setExit();
	ULT_INLINE int setWaiting();
	ULT_INLINE int signal();
	ULT_INLINE void setJoined();

	/* the worker runs it, never to come back here */
	ULT_INLINE void run(WorkerThread *worker)
	{
		if (atomicState.getState() != kInitialized)
			AbstractThread::resume(&abstractThread);
		else
			AbstractThread::enter(&abstractThread);
		int ret = setRunning();
		ULTHREAD_H_ASSERT_EQUAL(387, "ret==SCE_OK", ret, SCE_OK);
		this->worker = worker;
		if (context == NULL) {
			ret = _sceFiberAttachContextAndSwitch(&fiber, worker->m_stack,
							      worker->m_oneShotThreadStackSize, 0,
							      NULL);
			ULTHREAD_H_ASSERT_EQUAL(397, "ret==SCE_OK", ret, SCE_OK);
		} else {
			ret = sceFiberSwitch(&fiber, 0, NULL);
			ULTHREAD_H_ASSERT_EQUAL(400, "ret==SCE_OK", ret, SCE_OK);
		}
		ULTHREAD_H_ASSERT(402, "0", 0);
	}
};

/* SceUltUlthreadRuntime */
class UlthreadRuntime {
public:
	char name[SCE_ULT_MAX_NAME_LENGTH + 1];
	uint32_t numWorkerThread;
	WorkerThread *workerThreads;
	volatile int32_t numUlthread;		/* -1 once destroyed */
	uint32_t maxNumUlthread;
	Ulthread **ulthreads;			/* by ID */
	uint32_t oneShotThreadStackSize;
	uint32_t ulobjId;
	uint32_t reserved;
	QueueDataResourcePoolInternal m_readyQueueDataResourcePool;
	WaitingQueueResourcePoolInternal m_waitingQueueResourcePool;	/* the workers wait in it */
	/* each queue in a user queue's storage (SceUltQueue) */
	union {
		Queue m_readyQueue;		/* the ready ulthreads; NULL stops a worker */
		SceUltQueue m_readyQueueStorage;
	};
	union {
		Queue m_freeIdQueue;		/* the IDs not in use */
		SceUltQueue m_freeIdQueueStorage;
	};
	QueueDataResourcePoolInternal m_freeIdQueueDataResourcePool;
	uint32_t reserved2[36];

	/* the work area: the workers, then the pools' work areas, then the list */
	ULT_INLINE static uint32_t getWorkAreaSize(uint32_t maxNumUlthread, uint32_t numWorkerThread)
	{
		uint32_t numReady = maxNumUlthread + numWorkerThread;

		return ((sizeof(NBQueueEntry2) * numWorkerThread + 135) & ~7) +
		       sizeof(WorkerThread) * numWorkerThread +
		       ((sizeof(NBQueueEntry2) * (numReady + 1) + sizeof(Ulthread *) * numReady +
			 71) & ~7) +
		       ((36 * maxNumUlthread + 103) & ~7) +
		       sizeof(Ulthread *) * maxNumUlthread;
	}

	/* the default option parameters */
	ULT_INLINE static int initializeOptParam(SceUltUlthreadRuntimeOptParam *optParam,
						 uint32_t buildVersion)
	{
		if (!isAligned8(optParam))
			return SCE_ULT_ERROR_ALIGNMENT;
		sceClibMemset(optParam, 0, sizeof(*optParam));
		int ret = initializeOptParamHeader((OptParamHeader *)optParam, buildVersion);
		if (ret != SCE_OK)
			return ret;
		optParam->oneShotThreadStackSize = ULT_DEFAULT_ONE_SHOT_THREAD_STACK_SIZE;
		optParam->workerThreadPriority = ULT_DEFAULT_WORKER_THREAD_PRIORITY;
		optParam->workerThreadCpuAffinityMask = ULT_DEFAULT_WORKER_THREAD_CPU_AFFINITY_MASK;
		optParam->workerThreadAttr = 0;
		optParam->workerThreadOptParam = NULL;
		return SCE_OK;
	}

	__attribute__((noinline)) int initialize(const char *name, uint32_t maxNumUlthread,
						 uint32_t numWorkerThread, void *workArea,
						 const SceUltUlthreadRuntimeOptParam *optParam,
						 uint32_t buildVersion);
	__attribute__((noinline)) int destroy();
	__attribute__((noinline)) int getInfo(SceUltUlthreadRuntimeInfo *pInfo,
					      SceUltUlthread **ulthreadList, uint32_t maxNumUlthread,
					      uint32_t *workerThreadIdList,
					      uint32_t maxNumWorkerThreadId) const;

	/* a ready ulthread for the workers; the next one, NULL when one is to stop */
	__attribute__((noinline)) int pushReady(Ulthread *ulthread);
	__attribute__((noinline)) Ulthread *popReady();

	/* one more ulthread: SCE_ULT_ERROR_AGAIN with maxNumUlthread of them */
	ULT_INLINE int incrementNumUlthread()
	{
		int32_t numThr;

		do {
			numThr = __ldrex(&numUlthread);
			if (numThr < 0)
				return SCE_ULT_ERROR_STATE;
			if ((uint32_t)numThr >= maxNumUlthread)
				return SCE_ULT_ERROR_AGAIN;
		} while (__strex(numThr + 1, &numUlthread) != 0);
		return SCE_OK;
	}

	/* the ulthread gets a free ID in the list */
	ULT_INLINE void registerUlthread(Ulthread *ulthread)
	{
		uint32_t id;

		int ret = m_freeIdQueue.tryPopOutOfLine(&id);
		ULTHREAD_H_ASSERT_EQUAL(184, "ret==SCE_OK", ret, SCE_OK);
		__builtin_dmb();
		ULTHREAD_H_ASSERT_EQUAL(187, "ulthreads[id]==NULL", (int)ulthreads[id], 0);
		ulthreads[id] = ulthread;
		ulthread->id = id;
		__builtin_dmb();
	}

	/* the ulthread's ID is free again */
	ULT_INLINE void unregisterUlthread(Ulthread *ulthread)
	{
		uint32_t id = ulthread->id;

		__builtin_dmb();
		ULTHREAD_H_ASSERT_EQUAL(195, "ulthreads[id]==ulthread", (int)ulthreads[id],
					(int)ulthread);
		ulthreads[id] = NULL;
		int ret = m_freeIdQueue.tryPush(&id);
		ULTHREAD_H_ASSERT_EQUAL(198, "ret==SCE_OK", ret, SCE_OK);
	}

	ULT_INLINE void decrementNumUlthread()
	{
		int32_t numThr;

		do {
			numThr = __ldrex(&numUlthread);
			ULTHREAD_H_ASSERT(224, "numThr > 0", numThr > 0);
		} while (__strex(numThr - 1, &numUlthread) != 0);
	}
};

#include "Ulthread_inline.h"

#endif
