/*
 * Ulthread.cpp: the ulthread runtimes and their ulthreads.
 */
#include "Ulthread.h"

#define ULTHREAD_ASSERT(line, text, cond) \
	ULT_ASSERT_AT("Ulthread.cpp", line, text, cond)
#define ULTHREAD_ASSERT_EQUAL(line, text, actual, expected) \
	ULT_ASSERT_EQUAL_AT("Ulthread.cpp", line, text, actual, expected)

const void *volatile gLibPerfCaptureFlag;

/* an ulthread with a context waits itself; a one-shot ulthread cannot */
const AbstractThreadCallbacks Ulthread::s_callbacks = {
	{ NULL, NULL },
	Ulthread::waitSignal,
	Ulthread::sendSignal
};

const AbstractThreadCallbacks Ulthread::s_callbacksNoWait = {
	{ NULL, NULL },
	NULL,
	NULL
};

/* the ulthread is ready to run: a worker takes it */
int UlthreadRuntime::pushReady(Ulthread *ulthread)
{
	ULTHREAD_ASSERT(286,
			"(ulthread->atomicState.getState() == Ulthread::kInitialized) || "
			"(ulthread->atomicState.getState() == Ulthread::kReady)",
			(ulthread->atomicState.getState() == Ulthread::kInitialized) ||
			(ulthread->atomicState.getState() == Ulthread::kReady));
	__builtin_dmb();
	ULTHREAD_ASSERT_EQUAL(288,
			      "ulthread->getAbstractThread()->getParent()==AbstractThread::INVALID_ID",
			      (int)ulthread->getAbstractThread()->parent,
			      (int)AbstractThread::INVALID_ID);
	return m_readyQueue.tryPush(&ulthread);
}

int UlthreadRuntime::initialize(const char *name, uint32_t maxNumUlthread,
				uint32_t numWorkerThread, void *workArea,
				const SceUltUlthreadRuntimeOptParam *optParam,
				uint32_t buildVersion)
{
	SceUltUlthreadRuntimeOptParam defaultOptParam;
	int ret;

	if (this == NULL || name == NULL || workArea == NULL)
		return SCE_ULT_ERROR_NULL;
	if (maxNumUlthread == 0 || numWorkerThread == 0)
		return SCE_ULT_ERROR_INVALID;
	if (!isAligned8(this) || !isAligned8(workArea))
		return SCE_ULT_ERROR_ALIGNMENT;
	ULTHREAD_ASSERT(50, "buildVersion >= 0x00910000", buildVersion >= ULT_BUILD_VERSION_MIN);
	if (optParam == NULL) {
		initializeOptParam(&defaultOptParam, ULT_VERSION);
		optParam = &defaultOptParam;
	} else {
		ret = checkOptParam(optParam);
		if (ret != SCE_OK)
			return ret;
	}

	sceClibMemset(this, 0, sizeof(*this));
	ret = setName(this->name, name);
	if (ret != SCE_OK)
		return ret;
	numUlthread = 0;
	this->maxNumUlthread = maxNumUlthread;
	this->numWorkerThread = numWorkerThread;
	workerThreads = (WorkerThread *)workArea;
	oneShotThreadStackSize = optParam->oneShotThreadStackSize;

	/* the work area: the workers, then the pools', then the list */
	char *waitingQueueWorkArea = (char *)workArea + sizeof(WorkerThread) * numWorkerThread;
	char *readyQueueWorkArea = waitingQueueWorkArea +
				   ((sizeof(NBQueueEntry2) * numWorkerThread + 135) & ~7);
	uint32_t numReady = maxNumUlthread + numWorkerThread;

	ret = m_waitingQueueResourcePool.initialize(numWorkerThread, 2, waitingQueueWorkArea);
	ULTHREAD_ASSERT_EQUAL(101, "ret==SCE_OK", ret, SCE_OK);
	ret = m_readyQueueDataResourcePool.initialize(name, numReady, sizeof(Ulthread *), 1,
						      &m_waitingQueueResourcePool,
						      readyQueueWorkArea, NULL, buildVersion);
	ULTHREAD_ASSERT_EQUAL(105, "ret==SCE_OK", ret, SCE_OK);
	char *freeIdQueueWorkArea = readyQueueWorkArea + ((36 * numReady + 103) & ~7);
	ret = m_freeIdQueueDataResourcePool.initialize(name, this->maxNumUlthread, sizeof(uint32_t),
						       1, (WaitingQueueResourcePoolInternal *)NULL,
						       freeIdQueueWorkArea, NULL, buildVersion);
	ULTHREAD_ASSERT_EQUAL(110, "ret==SCE_OK", ret, SCE_OK);
	ulthreads = (Ulthread **)(freeIdQueueWorkArea +
				  ((36 * this->maxNumUlthread + 103) & ~7));

	ret = m_readyQueue.initialize(name, sizeof(Ulthread *), &m_waitingQueueResourcePool,
				      &m_readyQueueDataResourcePool, NULL, true, buildVersion);
	ULTHREAD_ASSERT_EQUAL(120, "ret==SCE_OK", ret, SCE_OK);
	ret = m_freeIdQueue.initialize(name, sizeof(uint32_t), NULL,
				       &m_freeIdQueueDataResourcePool, NULL, true, buildVersion);
	ULTHREAD_ASSERT_EQUAL(127, "ret==SCE_OK", ret, SCE_OK);
	for (uint32_t id = 0; id < this->maxNumUlthread; id++) {
		ret = m_freeIdQueue.tryPush(&id);
		ULTHREAD_ASSERT_EQUAL(130, "ret==SCE_OK", ret, SCE_OK);
		ulthreads[id] = NULL;
	}

	for (uint32_t i = 0; i < numWorkerThread; i++) {
		WorkerThreadParam param;

		param.stackSize = WORKER_THREAD_STACK_SIZE;
		param.priority = optParam->workerThreadPriority;
		param.oneShotThreadStackSize = optParam->oneShotThreadStackSize;
		param.attr = optParam->workerThreadAttr;
		param.cpuAffinityMask = optParam->workerThreadCpuAffinityMask;
		param.optParam = optParam->workerThreadOptParam;
		ret = workerThreads[i].initialize(this, name, &param);
		if (ret != SCE_OK) {
			/* the workers made so far go */
			for (uint32_t j = 0; j < i; j++) {
				int ret2 = workerThreads[j].finalize();
				ULTHREAD_ASSERT_EQUAL(154, "ret2==SCE_OK", ret2, SCE_OK);
			}
			return ret;
		}
	}
	__builtin_dmb();
	razorWrite(RAZOR_ULTHREAD_RUNTIME_PACKET, RAZOR_ULT_CREATE, this, NULL);
	for (uint32_t i = 0; i < numWorkerThread; i++) {
		ret = workerThreads[i].start();
		ULTHREAD_ASSERT_EQUAL(166, "ret==SCE_OK", ret, SCE_OK);
	}
	SceUlobjDbg_D7F0F610(this, 1, &ulobjId);
	return SCE_OK;
}

/* no ulthread may be left: the workers are stopped (a NULL each) and joined */
int UlthreadRuntime::destroy()
{
	int32_t numThr;

	if (this == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(this))
		return SCE_ULT_ERROR_ALIGNMENT;
	do {
		numThr = __ldrex(&numUlthread);
		if (numThr > 0)
			return SCE_ULT_ERROR_STATE;
	} while (__strex(-1, &numUlthread) != 0);
	SceUlobjDbg_F9C0F5DA(ulobjId);
	for (uint32_t i = 0; i < numWorkerThread; i++) {
		Ulthread *stop = NULL;
		int ret = m_readyQueue.tryPush(&stop);
		ULTHREAD_ASSERT_EQUAL(202, "ret==SCE_OK", ret, SCE_OK);
	}
	for (uint32_t i = 0; i < numWorkerThread; i++) {
		int ret = workerThreads[i].join();
		ULTHREAD_ASSERT_EQUAL(208, "ret==SCE_OK", ret, SCE_OK);
		int ret2 = workerThreads[i].finalize();
		ULTHREAD_ASSERT_EQUAL(211, "ret2==SCE_OK", ret2, SCE_OK);
	}
	m_readyQueue.destroy();
	m_freeIdQueue.destroy();
	m_readyQueueDataResourcePool.destroy();
	m_freeIdQueueDataResourcePool.destroy();
	razorWrite(RAZOR_ULTHREAD_RUNTIME_PACKET, RAZOR_ULT_DELETE, this, name);
	return SCE_OK;
}

int UlthreadRuntime::getInfo(SceUltUlthreadRuntimeInfo *pInfo, SceUltUlthread **ulthreadList,
			     uint32_t maxNumUlthread, uint32_t *workerThreadIdList,
			     uint32_t maxNumWorkerThreadId) const
{
	if (this == NULL || pInfo == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(this) || !isAligned8(pInfo) || !isAligned8(ulthreadList))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (pInfo->size != sizeof(*pInfo))
		return SCE_ULT_ERROR_INVALID;
	sceClibMemset(pInfo, 0, sizeof(*pInfo));
	pInfo->ulthreadRuntimeId = (uint32_t)this;
	pInfo->size = sizeof(*pInfo);
	getName(name, pInfo->name);
	pInfo->numUlthread = numUlthread;
	pInfo->maxNumUlthread = this->maxNumUlthread;
	pInfo->numWorkerThread = numWorkerThread;
	pInfo->oneShotThreadStackSize = oneShotThreadStackSize;
	if (ulthreads != NULL) {
		SceUltUlthread **list = ulthreadList;
		uint32_t n = 0;

		for (uint32_t id = 0; id < this->maxNumUlthread; id++) {
			if (ulthreads[id] == NULL)
				continue;
			if (ulthreadList == NULL || n >= maxNumUlthread)
				break;
			*list++ = (SceUltUlthread *)ulthreads[id];
			n++;
		}
	}
	if (workerThreads != NULL) {
		for (uint32_t i = 0; i < numWorkerThread; i++) {
			if (workerThreadIdList == NULL || i >= maxNumWorkerThreadId)
				break;
			workerThreadIdList[i] = workerThreads[i].m_threadId;
		}
	}
	return SCE_OK;
}

Ulthread *UlthreadRuntime::popReady()
{
	Ulthread *ret;

	int iret = m_readyQueue.pop(&ret);
	ULTHREAD_ASSERT_EQUAL(299, "iret==SCE_OK", iret, SCE_OK);
	if (ret != NULL)
		ULTHREAD_ASSERT(302,
				"(ret->atomicState.getState() == Ulthread::kInitialized) || "
				"(ret->atomicState.getState() == Ulthread::kReady)",
				(ret->atomicState.getState() == Ulthread::kInitialized) ||
				(ret->atomicState.getState() == Ulthread::kReady));
	__builtin_dmb();
	return ret;
}

Ulthread *Ulthread::getSelf()
{
	return getSelfInline();
}

int Ulthread::exit(int32_t status)
{
	Ulthread *self = getSelfInline();

	if (self == NULL)
		return SCE_ULT_ERROR_PERMISSION;
	self->exitStatus = status;
	__builtin_dmb();
	self->worker->switchFromUlthread(self, WORKER_REQUEST_EXIT, status);
	ULTHREAD_ASSERT(611, "0", 0);
	return SCE_ULT_ERROR_PERMISSION;
}

/* the ulthread runs its entry, and exits with what it returns */
void Ulthread::fiberEntry(SceUInt32 argOnInitialize, SceUInt32 argOnRun)
{
	Ulthread *ulthread = (Ulthread *)argOnInitialize;

	int ret = exit(ulthread->entry(ulthread->arg));
	ULTHREAD_ASSERT(319, "ret == SCE_OK", ret == SCE_OK);
	__breakpoint(0);
	for (;;)
		;
}

int Ulthread::initialize(const char *name, SceUltUlthreadEntry entry, uint32_t arg,
			 void *context, uint32_t sizeContext, UlthreadRuntime *runtime,
			 const SceUltUlthreadOptParam *optParam, uint32_t buildVersion)
{
	SceUltUlthreadOptParam defaultOptParam;
	int ret;

	if (this == NULL || name == NULL || entry == NULL || runtime == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(this) || !isAligned8(runtime))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (context == NULL) {
		/* a one-shot ulthread runs on its worker's stack */
		if (sizeContext != 0)
			return SCE_ULT_ERROR_INVALID;
		if (runtime->oneShotThreadStackSize == 0)
			return SCE_ULT_ERROR_PERMISSION;
	} else {
		if (!isAligned8(context))
			return SCE_ULT_ERROR_ALIGNMENT;
		if ((sizeContext & 7) != 0)
			return SCE_ULT_ERROR_INVALID;
		if (sizeContext == 0)
			return SCE_ULT_ERROR_INVALID;
		if (sizeContext < SCE_FIBER_CONTEXT_MINIMUM_SIZE)
			return SCE_ULT_ERROR_RANGE;
	}
	ULTHREAD_ASSERT(442, "buildVersion >= 0x00910000", buildVersion >= ULT_BUILD_VERSION_MIN);
	if (optParam == NULL) {
		initializeOptParam(&defaultOptParam, ULT_VERSION);
		optParam = &defaultOptParam;
	} else {
		ret = checkOptParam(optParam);
		if (ret != SCE_OK)
			return ret;
	}
	ret = setName(this->name, name);
	if (ret != SCE_OK)
		return ret;
	this->entry = entry;
	this->arg = arg;
	this->runtime = runtime;
	this->context = context;
	if (context == NULL) {
		uint32_t flags = (optParam->attribute & SCE_ULT_ULTHREAD_ATTRIBUTE_FORCE_WAIT) != 0 ?
				 ABSTRACT_THREAD_FLAG_WAIT_ON_PARENT : 0;

		sceClibMemset(&abstractThread, 0, sizeof(abstractThread));
		abstractThread.id = (uint32_t)this;
		abstractThread.flags = flags;
		abstractThread.callbacks = &s_callbacksNoWait;
		abstractThread.parent = (AbstractThread *)AbstractThread::INVALID_ID;
		__builtin_dmb();
	} else {
		sceClibMemset(&abstractThread, 0, sizeof(abstractThread));
		abstractThread.id = (uint32_t)this;
		abstractThread.flags = 0;
		abstractThread.callbacks = &s_callbacks;
		abstractThread.parent = (AbstractThread *)AbstractThread::INVALID_ID;
		__builtin_dmb();
	}
	ret = _sceFiberInitializeWithInternalOptionImpl(&fiber, name, fiberEntry, (SceUInt32)this,
							context, sizeContext, NULL,
							ULT_FIBER_FLAG_NO_ULOBJ |
							ULT_FIBER_FLAG_NO_RAZOR, ULT_VERSION);
	ULTHREAD_ASSERT_EQUAL(483, "ret==SCE_OK", ret, SCE_OK);
	atomicState.asStr.state = kInitialized;
	atomicState.asStr.signal = 0;
	atomicState.asStr.joiner = AbstractThread::INVALID_ID;
	__builtin_dmb();
	razorWrite(RAZOR_ULTHREAD_PACKET, RAZOR_ULT_CREATE, this, NULL);
	ret = this->runtime->incrementNumUlthread();
	if (ret != SCE_OK)
		return ret;
	this->runtime->registerUlthread(this);
	ret = this->runtime->pushReady(this);
	ULTHREAD_ASSERT_EQUAL(501, "ret==0", ret, 0);
	return ret;
}

/* waits for the ulthread's end (the current thread must be able to wait) */
int Ulthread::join(int32_t *status)
{
	AtomicState as;
	int ret;

	if (this == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(this))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (!AbstractThread::canWait())
		return SCE_ULT_ERROR_PERMISSION;
	uint32_t id = AbstractThread::getID();
	/* 0: it had exited, joined now; 1: the thread waits for its exit */
	do {
		ATOMIC_STATE_LOAD(as);
		if (as.asStr.state == kJoined) {
			ret = SCE_ULT_ERROR_STATE;
			break;
		}
		if (as.asStr.state == kExit) {
			if (as.asStr.joiner != AbstractThread::INVALID_ID) {
				ret = SCE_ULT_ERROR_BUSY;
				break;
			}
			as.asStr.state = kJoined;
			ret = 0;
		} else {
			if (as.asStr.joiner != AbstractThread::INVALID_ID) {
				ret = SCE_ULT_ERROR_BUSY;
				break;
			}
			if (id == AbstractThread::INVALID_ID) {
				ret = SCE_ULT_ERROR_AGAIN;
				break;
			}
			as.asStr.joiner = id;
			ret = 1;
		}
	} while (!ATOMIC_STATE_STORE(as));
	if (ret == 0)
		razorWrite(RAZOR_ULTHREAD_PACKET, RAZOR_ULT_JOIN, this, name);
	if (ret < 0) {
		AbstractThread::releaseID(id);
		return ret;
	}
	if (ret == 1) {
		/* its exit signals the thread */
		int ret2 = AbstractThread::waitSignal(id, NULL);
		ULTHREAD_ASSERT_EQUAL(549, "ret2==SCE_OK", ret2, SCE_OK);
		setJoined();
	}
	AbstractThread::releaseID(id);
	__builtin_dmb();
	if (status != NULL)
		*status = exitStatus;
	sceFiberFinalize(&fiber);
	runtime->unregisterUlthread(this);
	runtime->decrementNumUlthread();
	return SCE_OK;
}

/* an ulthread that has exited is joined; SCE_ULT_ERROR_AGAIN while it runs */
int Ulthread::tryJoin(int32_t *status)
{
	AtomicState as;

	if (this == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(this))
		return SCE_ULT_ERROR_ALIGNMENT;
	do {
		ATOMIC_STATE_LOAD(as);
		if (as.asStr.state == kJoined)
			return SCE_ULT_ERROR_STATE;
		if (as.asStr.state != kExit)
			return as.asStr.joiner == AbstractThread::INVALID_ID ? SCE_ULT_ERROR_AGAIN :
									      SCE_ULT_ERROR_BUSY;
		if (as.asStr.joiner != AbstractThread::INVALID_ID)
			return SCE_ULT_ERROR_BUSY;
		as.asStr.state = kJoined;
	} while (!ATOMIC_STATE_STORE(as));
	razorWrite(RAZOR_ULTHREAD_PACKET, RAZOR_ULT_JOIN, this, name);
	if (status != NULL)
		*status = exitStatus;
	sceFiberFinalize(&fiber);
	runtime->unregisterUlthread(this);
	runtime->decrementNumUlthread();
	return SCE_OK;
}

int Ulthread::getInfo(SceUltUlthreadInfo *pInfo) const
{
	if (this == NULL || pInfo == NULL)
		return SCE_ULT_ERROR_NULL;
	if (!isAligned8(this) || !isAligned8(pInfo))
		return SCE_ULT_ERROR_ALIGNMENT;
	if (pInfo->size != sizeof(*pInfo))
		return SCE_ULT_ERROR_INVALID;
	sceClibMemset(pInfo, 0, sizeof(*pInfo));
	pInfo->ulthreadId = (uint32_t)this;
	pInfo->size = sizeof(*pInfo);
	getName(name, pInfo->name);
	pInfo->attribute = abstractThread.flags == ABSTRACT_THREAD_FLAG_WAIT_ON_PARENT;
	pInfo->ulthreadRuntimeId = (uint32_t)runtime;
	sceClibMemset(&pInfo->fiberInfo, 0, sizeof(pInfo->fiberInfo));
	int ret = sceFiberGetInfo((SceFiber *)&fiber, &pInfo->fiberInfo);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

/* the callbacks: the ulthread waits by switching back to its worker */
int Ulthread::waitSignal(uint32_t id, void *cause)
{
	Ulthread *ulthread = (Ulthread *)id;

	return ulthread->worker->switchFromUlthread(ulthread, WORKER_REQUEST_WAIT, 0);
}

/* a waiting ulthread is ready again */
int Ulthread::sendSignal(uint32_t id)
{
	Ulthread *ulthread = (Ulthread *)id;

	int ret = ulthread->signal();
	if (ret < 0)
		return ret;
	if (ret == 3) {
		int ret2 = ulthread->runtime->pushReady(ulthread);
		ULTHREAD_H_ASSERT_EQUAL(421, "ret2==SCE_OK", ret2, SCE_OK);
	}
	return SCE_OK;
}
