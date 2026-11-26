/*
 * np_job_queue.cpp: SceNpManager job queue, worker thread, and lightweight
 * synchronization wrappers (0x81325EC4..0x8132668F, 0x81341444).
 */
#include "np_ipc_internal.h"

#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>

#define NP_JOB_ERR_GENERIC		-2141911538	/* 0x8055060E */
#define NP_JOB_ERR_ALREADY_INIT		-2141911423	/* 0x80550681 */
#define NP_JOB_ERR_NOT_INIT		-2141911422	/* 0x80550682 */
#define NP_JOB_ERR_DUPLICATE		-2141911421	/* 0x80550683 */
#define NP_JOB_ERR_ALREADY_RUNNING	-2141911420	/* 0x80550684 */
#define NP_JOB_ERR_QUEUE_STOPPED	-2141911419	/* 0x80550685 */
#define NP_JOB_ERR_NOT_FOUND		-2141911418	/* 0x80550686 */

enum NpJobState {
	NP_JOB_STATE_CREATED		= 1,
	NP_JOB_STATE_QUEUED		= 3,
	NP_JOB_STATE_RUNNING		= 4,
	NP_JOB_STATE_CANCELED_PENDING	= 5,
	NP_JOB_STATE_FINISHED		= 6
};

/* --- Lightweight mutex & cond wrappers (0x813264F2..0x813265DB + shared ops) --- */

void npLwMutexZero(NpIpcLwMutex *mutex)
{
	sceClibMemset(&mutex->work, 0, sizeof(mutex->work));
}

void npLwMutexDestroy(NpIpcLwMutex *mutex)
{
	SceKernelLwMutexWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(&mutex->work, &zeroWork, sizeof(mutex->work)) != 0) {
		sceKernelDeleteLwMutex(&mutex->work);
		sceClibMemset(&mutex->work, 0, sizeof(mutex->work));
	}
}

void npLwMutexDtor(NpIpcLwMutex *mutex)
{
	npLwMutexDestroy(mutex);
}

void npLwMutexLock(NpIpcLwMutex *mutex)
{
	if (sceKernelLockLwMutex(&mutex->work, 1, NULL) < 0) {
		SceKernelLwMutexInfo info;
		info.size = sizeof(info);
		sceKernelGetLwMutexInfo(&mutex->work, &info);
	}
}

void npLwMutexUnlock(NpIpcLwMutex *mutex)
{
	if (sceKernelUnlockLwMutex(&mutex->work, 1) < 0) {
		SceKernelLwMutexInfo info;
		info.size = sizeof(info);
		sceKernelGetLwMutexInfo(&mutex->work, &info);
	}
}

/* 0x81326562 */
int npLwMutexCreate(NpIpcLwMutex *mutex, const char *name, int recursive)
{
	SceKernelLwMutexWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(&mutex->work, &zeroWork, sizeof(mutex->work)) != 0)
		return NP_JOB_ERR_ALREADY_INIT;

	SceUInt32 attr = ((recursive & 1) != 0) ? (0x2000U | 0x0002U) : 0x2000U;
	int ret = sceKernelCreateLwMutex(&mutex->work, name, attr, 0, NULL);
	return (ret >= 0) ? 0 : ret;
}

static void npLwCondZero(NpIpcLwCond *cond)
{
	sceClibMemset(&cond->work, 0, sizeof(cond->work));
}

static void npLwCondDestroy(NpIpcLwCond *cond)
{
	SceKernelLwCondWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(&cond->work, &zeroWork, sizeof(cond->work)) != 0) {
		sceKernelDeleteLwCond(&cond->work);
		sceClibMemset(&cond->work, 0, sizeof(cond->work));
	}
}

/* 0x813264F2 */
static int npLwCondCreate(NpIpcLwCond *cond, const char *name, NpIpcLwMutex *mutex)
{
	SceKernelLwCondWork zeroWork;
	sceClibMemset(&zeroWork, 0, sizeof(zeroWork));
	if (sceClibMemcmp(&cond->work, &zeroWork, sizeof(cond->work)) != 0)
		return NP_JOB_ERR_ALREADY_INIT;

	int ret = sceKernelCreateLwCond(&cond->work, name, 0x2000U, &mutex->work, NULL);
	return (ret >= 0) ? 0 : ret;
}

static int npLwCondWait(NpIpcLwCond *cond, unsigned int *timeout)
{
	int ret = sceKernelWaitLwCond(&cond->work, timeout);
	return (ret >= 0) ? 0 : ret;
}

static int npLwCondSignal(NpIpcLwCond *cond)
{
	int ret = sceKernelSignalLwCond(&cond->work);
	return (ret >= 0) ? 0 : ret;
}

/* --- NpThreadBase (0x813265DC..0x8132668F, 0x811F4098) --- */

/* 0x813265DC */
NpThreadBase::NpThreadBase()
	: m_threadId(-1),
	  m_started(0),
	  m_exitCode(NP_JOB_ERR_GENERIC)
{
}

/* 0x81326600 / 0x81326626 */
NpThreadBase::~NpThreadBase()
{
	Delete();
}

void NpThreadBase::Delete()
{
	if (m_threadId != -1) {
		sceKernelDeleteThread(m_threadId);
		m_threadId = -1;
	}
}

bool NpThreadBase::IsStarted() const
{
	return m_started != 0;
}

int NpThreadBase::ThreadEntry(SceSize args, void *argp)
{
	(void)args;
	NpThreadBase *self = *static_cast<NpThreadBase **>(argp);
	self->m_exitCode = self->Run();
	return 0;
}

/* 0x81326652 */
int NpThreadBase::Create(const char *name, int priority, SceSize stackSize)
{
	if (m_threadId != -1)
		return NP_JOB_ERR_ALREADY_INIT;

	SceUID tid = sceKernelCreateThread(name, ThreadEntry, priority, stackSize, 0, 0, NULL);
	if (tid < 0)
		return tid;

	m_threadId = tid;
	m_started = 0;
	return 0;
}

int NpThreadBase::Start()
{
	NpThreadBase *self = this;
	int ret = sceKernelStartThread(m_threadId, sizeof(self), &self);
	if (ret < 0)
		return ret;
	m_started = 1;
	return 0;
}

int NpThreadBase::Join(int *outExitCode)
{
	int ret = sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
	if (ret < 0)
		return ret;
	if (outExitCode != NULL)
		*outExitCode = m_exitCode;
	return 0;
}

/* --- NpJobBase (0x81325EC4..0x81325FDF) --- */

/* 0x81325EC4 */
NpJobBase::NpJobBase(const char *name)
	: m_requestId(-1),
	  m_name(name),
	  m_state(NP_JOB_STATE_CREATED),
	  m_next(NULL),
	  m_prev(NULL),
	  m_queue(NULL),
	  m_cancelError(0),
	  m_canceled(0)
{
}

/* 0x81325EEC / 0x81325EFA */
NpJobBase::~NpJobBase()
{
}

bool NpJobBase::IsCanceled() const
{
	return m_canceled != 0;
}

/* 0x81325F10 */
void NpJobBase::Unlink()
{
	NpJobBase **headPtr = NULL;
	NpJobBase **tailPtr = NULL;

	if (m_state == NP_JOB_STATE_QUEUED) {
		headPtr = &m_queue->m_pendingHead;
		tailPtr = &m_queue->m_pendingTail;
	} else if (m_state == NP_JOB_STATE_RUNNING) {
		headPtr = &m_queue->m_runningHead;
		tailPtr = &m_queue->m_runningTail;
	} else if (m_state == NP_JOB_STATE_CANCELED_PENDING ||
		   m_state == NP_JOB_STATE_FINISHED) {
		headPtr = &m_queue->m_finishedHead;
		tailPtr = &m_queue->m_finishedTail;
	} else {
		return;
	}

	if (m_next != NULL)
		m_next->m_prev = m_prev;
	else
		*tailPtr = m_prev;

	if (m_prev != NULL)
		m_prev->m_next = m_next;
	else
		*headPtr = m_next;

	m_next = NULL;
	m_prev = NULL;
}

/* 0x81325F90 */
int NpJobBase::Cancel(int cancelError)
{
	int prevState = m_state;
	m_canceled = 1;
	if (prevState == NP_JOB_STATE_QUEUED) {
		Unlink();
		if (m_state == NP_JOB_STATE_RUNNING)
			m_state = NP_JOB_STATE_FINISHED;
		else if (m_state == NP_JOB_STATE_QUEUED)
			m_state = NP_JOB_STATE_CANCELED_PENDING;

		NpJobBase *tail = m_queue->m_finishedTail;
		m_prev = tail;
		m_next = NULL;
		if (tail != NULL)
			tail->m_next = this;
		if (m_queue->m_finishedHead == NULL)
			m_queue->m_finishedHead = this;
		m_queue->m_finishedTail = this;
		m_cancelError = cancelError;
	}
	return prevState;
}

/* --- NpJobWorkerThread (0x81326460..0x813264F1) --- */

NpJobWorkerThread::NpJobWorkerThread(NpJobQueue *queue)
	: NpThreadBase(),
	  m_queue(queue),
	  m_unused(0),
	  m_exitRequested(0)
{
}

/* 0x81326460 / 0x8132647C */
NpJobWorkerThread::~NpJobWorkerThread()
{
}

/* 0x8132649C */
int NpJobWorkerThread::Run()
{
	while (m_exitRequested == 0) {
		NpJobBase *job = NULL;
		if (m_queue->Dequeue(&job) < 0)
			break;
		int result = job->Run();
		m_queue->CompleteJob(job, result);
	}
	return 0;
}

/* --- NpJobQueue (0x81325FE0..0x8132645F, 0x81341444) --- */

/* 0x81325FE0 */
NpJobQueue::NpJobQueue()
	: m_stopRequested(0),
	  m_pendingHead(NULL),
	  m_pendingTail(NULL),
	  m_runningHead(NULL),
	  m_runningTail(NULL),
	  m_finishedHead(NULL),
	  m_finishedTail(NULL),
	  m_worker(NULL)
{
	npLwMutexZero(&m_mutex);
	npLwCondZero(&m_cond);
}

/* 0x813260E6 / 0x81326112 */
NpJobQueue::~NpJobQueue()
{
	Shutdown();
	npLwCondDestroy(&m_cond);
	npLwMutexDtor(&m_mutex);
}

NpJobBase *NpJobQueue::FindJobLocked(int requestId) const
{
	for (NpJobBase *job = m_pendingHead; job != NULL; job = job->m_next) {
		if (job->m_requestId == requestId)
			return job;
	}
	for (NpJobBase *job = m_runningHead; job != NULL; job = job->m_next) {
		if (job->m_requestId == requestId)
			return job;
	}
	for (NpJobBase *job = m_finishedHead; job != NULL; job = job->m_next) {
		if (job->m_requestId == requestId)
			return job;
	}
	return NULL;
}

void NpJobQueue::DrainFinishedByState(int targetState)
{
	for (;;) {
		NpJobBase *job = m_finishedHead;
		while (job != NULL && job->m_state != targetState)
			job = job->m_next;
		if (job == NULL)
			break;
		job->Unlink();
		job->Finish(job->m_cancelError);
		delete job;
	}
}

/* 0x81326018 */
int NpJobQueue::CancelAll(int cancelError)
{
	npLwMutexLock(&m_mutex);
	for (NpJobBase *job = m_pendingHead; job != NULL; job = job->m_next)
		job->Cancel(cancelError);
	for (NpJobBase *job = m_runningHead; job != NULL; job = job->m_next)
		job->Cancel(cancelError);
	npLwMutexUnlock(&m_mutex);

	DrainFinishedByState(NP_JOB_STATE_CANCELED_PENDING);
	return 0;
}

/* 0x81326090 */
void NpJobQueue::Shutdown()
{
	CancelAll(NP_JOB_ERR_GENERIC);
	if (m_worker != NULL) {
		if (m_worker->IsStarted()) {
			m_stopRequested = 1;
			m_worker->Join(NULL);
		}
		m_worker->Delete();
		delete m_worker;
		m_worker = NULL;
	}
	npLwCondDestroy(&m_cond);
	npLwMutexDestroy(&m_mutex);
}

/* 0x81326144 */
int NpJobQueue::Init(const char *name, SceUInt32 priority, SceSize stackSize)
{
	int ret = npLwMutexCreate(&m_mutex, name, 0);
	if (ret < 0)
		return ret;

	ret = npLwCondCreate(&m_cond, name, &m_mutex);
	if (ret < 0) {
		npLwMutexDestroy(&m_mutex);
		return ret;
	}

	m_worker = new NpJobWorkerThread(this);
	if (m_worker != NULL) {
		ret = m_worker->Create(name, static_cast<int>(priority), stackSize);
		if (ret >= 0)
			return 0;
		delete m_worker;
		m_worker = NULL;
	}

	npLwCondDestroy(&m_cond);
	npLwMutexDestroy(&m_mutex);
	return ret;
}

/* 0x81341444 */
int NpJobQueue::Start()
{
	if (m_worker == NULL)
		return NP_JOB_ERR_NOT_INIT;
	if (m_worker->IsStarted())
		return NP_JOB_ERR_ALREADY_RUNNING;

	m_stopRequested = 0;
	int ret = m_worker->Start();
	if (ret < 0) {
		m_stopRequested = 1;
		return ret;
	}
	return 0;
}

/* 0x813261E6 */
int NpJobQueue::Enqueue(int requestId, NpJobBase *job)
{
	if (m_stopRequested != 0)
		return NP_JOB_ERR_QUEUE_STOPPED;

	npLwMutexLock(&m_mutex);
	if (FindJobLocked(requestId) != NULL) {
		npLwMutexUnlock(&m_mutex);
		return NP_JOB_ERR_DUPLICATE;
	}

	job->m_requestId = requestId;
	job->m_next = NULL;
	job->m_state = NP_JOB_STATE_QUEUED;
	job->m_prev = m_pendingTail;
	job->m_queue = this;
	if (m_pendingTail != NULL)
		m_pendingTail->m_next = job;
	if (m_pendingHead == NULL)
		m_pendingHead = job;
	m_pendingTail = job;
	npLwMutexUnlock(&m_mutex);

	int ret = npLwCondSignal(&m_cond);
	return (ret >= 0) ? 0 : ret;
}

/* 0x8132628C */
int NpJobQueue::Cancel(int requestId, int cancelError)
{
	npLwMutexLock(&m_mutex);
	NpJobBase *job = FindJobLocked(requestId);
	if (job == NULL || job->m_state == NP_JOB_STATE_FINISHED) {
		npLwMutexUnlock(&m_mutex);
		return NP_JOB_ERR_NOT_FOUND;
	}

	job->Cancel(cancelError);
	npLwMutexUnlock(&m_mutex);
	DrainFinishedByState(NP_JOB_STATE_CANCELED_PENDING);
	return 0;
}

/* 0x8132633C */
int NpJobQueue::Dequeue(NpJobBase **outJob)
{
	NpJobBase *job = NULL;
	npLwMutexLock(&m_mutex);
	while (m_stopRequested == 0) {
		job = m_pendingHead;
		if (job != NULL)
			break;
		int ret = npLwCondWait(&m_cond, NULL);
		if (ret < 0) {
			npLwMutexUnlock(&m_mutex);
			return ret;
		}
	}

	if (m_stopRequested != 0) {
		npLwMutexUnlock(&m_mutex);
		return NP_JOB_ERR_QUEUE_STOPPED;
	}

	job->Unlink();
	job->m_next = NULL;
	job->m_state = NP_JOB_STATE_RUNNING;
	job->m_prev = m_runningTail;
	if (m_runningTail != NULL)
		m_runningTail->m_next = job;
	if (m_runningHead == NULL)
		m_runningHead = job;
	m_runningTail = job;
	*outJob = job;
	npLwMutexUnlock(&m_mutex);
	return 0;
}

/* 0x813263C6 */
void NpJobQueue::CompleteJob(NpJobBase *job, int result)
{
	npLwMutexLock(&m_mutex);
	job->Unlink();
	if (job->m_state == NP_JOB_STATE_RUNNING)
		job->m_state = NP_JOB_STATE_FINISHED;
	else if (job->m_state == NP_JOB_STATE_QUEUED)
		job->m_state = NP_JOB_STATE_CANCELED_PENDING;

	job->m_prev = m_finishedTail;
	job->m_next = NULL;
	if (m_finishedTail != NULL)
		m_finishedTail->m_next = job;
	if (m_finishedHead == NULL)
		m_finishedHead = job;
	m_finishedTail = job;
	job->m_cancelError = result;
	npLwMutexUnlock(&m_mutex);

	DrainFinishedByState(NP_JOB_STATE_FINISHED);
}

/* C++ free-function entrypoints matching np_ipc_internal.h */

NpJobQueue *npJobQueueConstruct(void *mem)
{
	return new (mem) NpJobQueue();
}

int npJobQueueInit(NpJobQueue *queue, const char *name, SceUInt32 priority, SceSize stackSize)
{
	return queue->Init(name, priority, stackSize);
}

int npJobQueueStart(NpJobQueue *queue)
{
	return queue->Start();
}

int npJobQueueEnqueue(NpJobQueue *queue, int reqHandle, NpJobBase *job)
{
	return queue->Enqueue(reqHandle, job);
}

int npJobQueueCancel(NpJobQueue *queue, int reqHandle, int cancelError)
{
	return queue->Cancel(reqHandle, cancelError);
}
