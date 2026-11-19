/*
 * The task queue ("QUEUE:"): each request of the API's callers becomes a task, a job that runs
 * the host's operations for it through the command handler. The tasks wait in a list in order
 * of priority; the one at the front runs on paf's job queue, and when it finishes a job on the
 * finish queue takes it off the list and starts the next. While the queue has nothing to do,
 * an idle task holds the front and answers the host.
 */
#include "mtp_queue.h"

/* Sony's build calls the task list's clear() rather than inlining it */
namespace paf {
template<> inline __attribute__((noinline)) void
list<common::SharedPtr<job::JobItem>, allocator<common::SharedPtr<job::JobItem> > >::clear()
{
	if (head != NULL) {
		_Node *node = head->next;

		while (node != head) {
			_Node *next = node->next;

			destroy(&node->value);
			operator delete(node);
			node = next;
		}
		head->prev = head;
		head->next = head;
		count = 0;
	}
}
}

class MtpTaskQueue {
public:
	MtpTaskQueue() : m_jobQueue(NULL), m_finishQueue(NULL), m_handler(NULL),
		m_listMutex(NULL), m_currentMutex(NULL), m_evf(-1), m_evf2(-1), m_current(NULL),
		m_terminating(0)
	{
	}

	virtual ~MtpTaskQueue();

	int Finalize();
	int Kick();
	int Push(MtpContext *context, const MtpEvent *event);
	int Initialize(MtpCommandHandler handler);
	int PushCommand(const MtpOperation *operation, SceUInt32 bits);
	int Cancel(MtpContext *context);
	int CancelAll();
	int CancelAllForDoCancelAndPause();
	int GetContext(SceUInt32 contextId, MtpContext **context);
	static void RunCallback(MtpTaskJob *job, void *arg);
	static void FinishCallback(MtpTaskJob *job, void *arg);

	paf::list<SharedPtr<JobItem> > m_tasks;	/* by priority, the running one first */
	JobQueue *m_jobQueue;
	JobQueue *m_finishQueue;
	MtpCommandHandler m_handler;	/* NULL: not initialized */
	char m_titleId[32];		/* the title the next tasks are for */
	Mutex *m_listMutex;
	Mutex *m_currentMutex;
	SceUID m_evf;			/* MTP_TASK_EVF_CURRENT */
	SceUID m_evf2;
	MtpTaskJob *m_current;		/* the running task */
	MtpTaskRequest m_request;	/* the command being handed to it */
	int m_terminating;
};

/* takes a finished task off the list and starts the next one */
class MtpTaskQueueFinishJob : public JobItem {
public:
	MtpTaskQueueFinishJob(MtpTaskQueue *queue, SceUID evf, MtpTaskJob *job)
		: JobItem("SceMTpTaskQueueFinishJob", NULL), m_queue(queue), m_job(), m_evf(evf)
	{
		m_queue->m_currentMutex->Lock();
		if (m_queue->m_current == job) {
			m_queue->m_current = NULL;
			sceKernelClearEventFlag(m_evf, 0);
		}
		m_queue->m_currentMutex->Unlock();
		m_queue->m_listMutex->Lock();
		paf::list<SharedPtr<JobItem> >::iterator it;
		for (it = m_queue->m_tasks.begin(); it != m_queue->m_tasks.end(); ++it) {
			if (it->get() == job)
				break;
		}
		if (it == m_queue->m_tasks.end()) {
			m_queue->m_listMutex->Unlock();
		} else {
			m_job = *it;
			m_queue->m_listMutex->Unlock();
		}
	}

	virtual ~MtpTaskQueueFinishJob()
	{
	}

	virtual int Run()
	{
		MtpTaskJob *job = (MtpTaskJob *)m_job.get();

		if (job != NULL && job->GetInfo() != NULL) {
			if (job->IsCanceled() || job->GetResult() < 0) {
				SharedPtr<MtpContext> context = job->GetInfo()->GetContext();

				if (context.get() != NULL)
					mtpContextCancelById(context->id);
			}
			m_queue->m_listMutex->Lock();
			paf::list<SharedPtr<JobItem> >::iterator it;
			for (it = m_queue->m_tasks.begin(); it != m_queue->m_tasks.end(); ++it) {
				if (it->get() == job)
					break;
			}
			if (it != m_queue->m_tasks.end())
				m_queue->m_tasks.erase(it);
			m_queue->m_listMutex->Unlock();
			if (m_queue->m_terminating == 0)
				m_queue->Kick();
		}
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	MtpTaskQueue *m_queue;
	SharedPtr<JobItem> m_job;
	SceUID m_evf;
};

static MtpTaskQueue *s_taskQueue;

int MtpTaskQueue::Finalize()
{
	m_terminating = 1;
	m_handler = NULL;
	if (m_jobQueue != NULL) {
		m_jobQueue->CancelAllItems();
		m_jobQueue->WaitEmpty();
		delete m_jobQueue;
		m_jobQueue = NULL;
	}
	if (m_finishQueue != NULL) {
		m_finishQueue->CancelAllItems();
		m_finishQueue->WaitEmpty();
		delete m_finishQueue;
		m_finishQueue = NULL;
	}
	if (m_evf != -1) {
		sceKernelDeleteEventFlag(m_evf);
		m_evf = -1;
	}
	if (m_evf2 != -1) {
		sceKernelDeleteEventFlag(m_evf2);
		m_evf2 = -1;
	}
	if (m_listMutex != NULL) {
		delete m_listMutex;
		m_listMutex = NULL;
	}
	if (m_currentMutex != NULL) {
		delete m_currentMutex;
		m_currentMutex = NULL;
	}
	return 0;
}

/* run the task at the front, or make the idle task when there is none */
int MtpTaskQueue::Kick()
{
	if (m_tasks.size() != 0) {
		m_listMutex->Lock();
		SharedPtr<JobItem> front = m_tasks.front();
		m_listMutex->Unlock();
		MtpTaskJob *job = (MtpTaskJob *)front.get();

		if (job != NULL && job->GetInfo() != NULL && job->GetInfo()->IsRunnable() &&
		    !job->IsQueued()) {
			job->SetQueued();
			m_jobQueue->Enqueue(front);
		}
	} else {
		MtpEvent event;

		event.code = MTP_TASK_EVENT_IDLE;
		Push(NULL, &event);
	}
	return 0;
}

int MtpTaskQueue::Push(MtpContext *context, const MtpEvent *event)
{
	int background = 0;
	MtpTaskInfo *info;
	MtpTaskJob *job;
	int ret;

	if (m_handler == NULL)
		return SCE_MTP_ERROR_DEVICE;
	m_currentMutex->Lock();
	if (m_current != NULL && m_current->GetInfo() != NULL)
		background = m_current->GetInfo()->GetKind() == MTP_TASK_KIND_BACKGROUND;
	m_currentMutex->Unlock();
	info = new MtpTaskInfo();
	if (info == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	if (event->code != MTP_TASK_EVENT_IDLE)
		info->SetKind(m_titleId);
	info->Set(context, event);
	job = new MtpTaskJob(info, RunCallback, m_handler, FinishCallback, this);
	if (job == NULL) {
		delete info;
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = job->Initialize();
	if (ret < 0) {
		delete job;
		return ret;
	}
	SharedPtr<JobItem> item(job);
	{
		MutexLock lock(m_listMutex);
		paf::list<SharedPtr<JobItem> >::iterator it;

		for (it = m_tasks.begin(); it != m_tasks.end(); ++it) {
			SharedPtr<JobItem> task = *it;
			MtpTaskJob *other = (MtpTaskJob *)task.get();

			if (other == NULL)
				continue;
			if (other->GetInfo() != NULL &&
			    other->GetInfo()->GetPriority() < info->GetPriority()) {
				m_tasks.insert(it, item);
				other->CancelIfIdle(info->GetPriority());
				break;
			}
			if (other->GetInfo() != NULL &&
			    other->GetInfo()->GetPriority() == MTP_TASK_PRIORITY_URGENT &&
			    info->GetPriority() == MTP_TASK_PRIORITY_URGENT) {
				m_tasks.insert(it, item);
				other->CancelIfIdle(info->GetPriority());
				break;
			}
		}
		if (it == m_tasks.end())
			m_tasks.push_back(item);
	}
	if (!background || info->GetKind() == MTP_TASK_KIND_BACKGROUND) {
		SharedPtr<JobItem> front = m_tasks.front();

		if (front.get() == item.get())
			Kick();
	} else {
		SceUInt16 code = mtpCoreGetOperationCode();

		if (code == MTP_OPERATION_GET_OBJECT || code == MTP_OPERATION_GET_THUMB ||
		    code == 0x952C || code == 0x952F || code == 0x953F)
			mtpCallbackInvokeEvent(1);
		else
			mtpCallbackInvokeEvent(2);
	}
	return 0;
}

int MtpTaskQueue::Initialize(MtpCommandHandler handler)
{
	SceUID evf;

	m_handler = handler;
	m_terminating = 0;
	m_jobQueue = new JobQueue("SceMtpTaskJobQueue", NULL);
	if (m_jobQueue == NULL)
		goto fail;
	m_finishQueue = new JobQueue("SceMtpTaskJobFinishQueue", NULL);
	if (m_finishQueue == NULL)
		goto fail;
	evf = sceKernelCreateEventFlag("SceMtpTaskJobQueueEvent", 0, 0, NULL);
	if (evf < 0)
		goto fail;
	m_evf = evf;
	evf = sceKernelCreateEventFlag("SceMtpTaskJobQueueEvent", 0, 0, NULL);
	if (evf < 0)
		goto fail;
	m_evf2 = evf;
	m_listMutex = new Mutex("SceMtpTaskListMutex", 0);
	if (m_listMutex == NULL)
		goto fail;
	m_currentMutex = new Mutex("SceMtpCurrentTaskMutex", 0);
	if (m_currentMutex == NULL)
		goto fail;
	{
		MtpEvent event;

		event.code = MTP_TASK_EVENT_IDLE;
		Push(NULL, &event);
	}
	return 0;
fail:
	Finalize();
	return 0;
}

int mtpQueueInit(MtpCommandHandler handler)
{
	MtpTaskQueue *queue;

	if (s_taskQueue != NULL)
		return 0;
	queue = new MtpTaskQueue();
	s_taskQueue = queue;
	if (queue == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	return queue->Initialize(handler);
}

int mtpQueueTerminate(void)
{
	int ret;

	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	ret = s_taskQueue->Finalize();
	delete s_taskQueue;
	s_taskQueue = NULL;
	return ret;
}

int mtpTaskQueuePush(MtpContext *context, const MtpEvent *event)
{
	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return s_taskQueue->Push(context, event);
}

/* hand an operation of the host's to the running task, and wait for its result */
int MtpTaskQueue::PushCommand(const MtpOperation *operation, SceUInt32 bits)
{
	MtpTaskJob *job;
	int state;
	int ret;

	mtpUtilMemset(&m_request, 0, sizeof(m_request));
	m_request.bits = bits;
	mtpUtilMemcpy(&m_request.operation, operation, sizeof(m_request.operation));
	for (;;) {
		MtpTaskJob *current;

		for (;;) {
			m_currentMutex->Lock();
			current = m_current;
			m_currentMutex->Unlock();
			if (current != NULL)
				break;
			SceUInt32 outBits = 0;
			SceUInt32 timeout = MTP_TASK_WAIT_CURRENT_TIMEOUT;

			if (sceKernelWaitEventFlag(m_evf, MTP_TASK_EVF_CURRENT,
			    SCE_KERNEL_EVF_WAITMODE_OR, &outBits, &timeout) ==
			    SCE_KERNEL_ERROR_WAIT_TIMEOUT)
				return SCE_MTP_ERROR_NOT_INITIALIZED;
		}
		m_listMutex->Lock();
		paf::list<SharedPtr<JobItem> >::iterator it;
		for (it = m_tasks.begin(); it != m_tasks.end(); ++it) {
			if (it->get() == current)
				break;
		}
		m_listMutex->Unlock();
		if (it == m_tasks.end())
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		SharedPtr<JobItem> task = *it;

		job = (MtpTaskJob *)task.get();
		if (job == NULL)
			return SCE_MTP_ERROR_NOT_INITIALIZED;
		for (;;) {
			state = job->GetState();
			if (state == MTP_TASK_JOB_STATE_WAIT_COMMAND)
				break;
			if ((SceUInt32)state > MTP_TASK_JOB_STATE_RUNNING)
				goto pushed;
			sceKernelDelayThread(MTP_TASK_POLL_INTERVAL);
		}
		ret = job->PushCommand(&m_request);
		if (ret < 0)
			return ret;
pushed:
		if (state != MTP_TASK_JOB_STATE_FINISHED)
			break;
		sceKernelDelayThread(MTP_TASK_POLL_INTERVAL);
	}
	return job->WaitResult();
}

int mtpQueuePushCommand(MtpOperation *operation, int bits)
{
	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return s_taskQueue->PushCommand(operation, bits);
}

void mtpTaskQueueSetTitleId(const char *titleId)
{
	if (s_taskQueue != NULL)
		mtpUtilStrncpy(s_taskQueue->m_titleId, titleId, sizeof(s_taskQueue->m_titleId));
}

int mtpTaskQueueGetTitleId(char *titleId, SceSize size)
{
	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	if (titleId == NULL || size > sizeof(s_taskQueue->m_titleId))
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	mtpUtilStrncpy(titleId, s_taskQueue->m_titleId, size);
	return 0;
}

/* cancel the task of a context: a queued one is cancelled, a waiting one dropped */
int MtpTaskQueue::Cancel(MtpContext *context)
{
	Mutex *mutex = m_listMutex;
	int ret;

	mutex->Lock();
	paf::list<SharedPtr<JobItem> >::iterator it;
	for (it = m_tasks.begin(); it != m_tasks.end(); ++it) {
		MtpTaskJob *job = (MtpTaskJob *)it->get();

		if (job != NULL && job->GetInfo() != NULL) {
			SharedPtr<MtpContext> owner = job->GetInfo()->GetContext();

			if (owner.get() == context) {
				if (job->IsQueued())
					(*it)->Cancel();
				else
					m_tasks.erase(it);
				break;
			}
		}
	}
	if (it == m_tasks.end()) {
		mutex->Unlock();
		ret = SCE_MTP_ERROR_NOT_FOUND;
	} else {
		mutex->Unlock();
		ret = 0;
	}
	return ret;
}

int mtpTaskQueueCancel(MtpContext *context)
{
	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return s_taskQueue->Cancel(context);
}

int MtpTaskQueue::CancelAll()
{
	Mutex *mutex = m_listMutex;

	mutex->Lock();
	for (paf::list<SharedPtr<JobItem> >::iterator it = m_tasks.begin(); it != m_tasks.end();
	     ++it) {
		SharedPtr<JobItem> task = *it;

		if (task.get() != NULL)
			task->Cancel();
	}
	m_tasks.clear();
	mutex->Unlock();
	Kick();
	return 0;
}

int mtpTaskQueueCancelAll(void)
{
	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return s_taskQueue->CancelAll();
}

/* cancel the tasks not running yet that the background copy made */
int MtpTaskQueue::CancelAllForDoCancelAndPause()
{
	Mutex *mutex = m_listMutex;

	mutex->Lock();
	for (paf::list<SharedPtr<JobItem> >::iterator it = m_tasks.begin(); it != m_tasks.end();
	     ++it) {
		MtpTaskJob *job = (MtpTaskJob *)it->get();

		if (job != NULL && job->GetState() == MTP_TASK_JOB_STATE_IDLE &&
		    (mtpUtilStrncmp(m_titleId, "NPXS10100", sizeof(m_titleId)) != 0 ||
		     (job->GetInfo() != NULL &&
		      job->GetInfo()->GetKind() == MTP_TASK_KIND_BACKGROUND)))
			job->Cancel();
	}
	mutex->Unlock();
	return 0;
}

int mtpTaskQueueCancelAllForDoCancelAndPause(void)
{
	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return s_taskQueue->CancelAllForDoCancelAndPause();
}

int MtpTaskQueue::GetContext(SceUInt32 contextId, MtpContext **context)
{
	Mutex *mutex = m_listMutex;
	int ret;

	mutex->Lock();
	paf::list<SharedPtr<JobItem> >::iterator it;
	for (it = m_tasks.begin(); it != m_tasks.end(); ++it) {
		MtpTaskJob *job = (MtpTaskJob *)it->get();

		if (job != NULL && job->GetInfo() != NULL) {
			SharedPtr<MtpContext> owner = job->GetInfo()->GetContext();

			if (owner.get() != NULL && owner->id == contextId) {
				*context = owner.get();
				break;
			}
		}
	}
	if (it == m_tasks.end()) {
		mutex->Unlock();
		ret = SCE_MTP_ERROR_NOT_FOUND;
	} else {
		mutex->Unlock();
		ret = 0;
	}
	return ret;
}

int mtpTaskQueueGetContext(SceUInt32 contextId, MtpContext **context)
{
	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return s_taskQueue->GetContext(contextId, context);
}

int mtpQueueKick(void)
{
	if (s_taskQueue == NULL)
		return SCE_MTP_ERROR_DEVICE;
	return s_taskQueue->Kick();
}

inline MtpTaskQueue::~MtpTaskQueue()
{
}

/* a task starts to run: it is the current one */
void MtpTaskQueue::RunCallback(MtpTaskJob *job, void *arg)
{
	MtpTaskQueue *queue = (MtpTaskQueue *)arg;
	Mutex *current = queue->m_currentMutex;

	current->Lock();
	queue->m_listMutex->Lock();
	Mutex *list = queue->m_listMutex;
	paf::list<SharedPtr<JobItem> >::iterator it;
	for (it = queue->m_tasks.begin(); it != queue->m_tasks.end(); ++it) {
		if (it->get() == job)
			break;
	}
	if (it == queue->m_tasks.end()) {
		list->Unlock();
		current->Unlock();
		return;
	}
	list->Unlock();
	{
		SharedPtr<JobItem> task = *it;

		queue->m_current = (MtpTaskJob *)task.get();
		sceKernelSetEventFlag(queue->m_evf, MTP_TASK_EVF_CURRENT);
	}
	current->Unlock();
}

/* a task has finished: the finish queue takes it off the list */
void MtpTaskQueue::FinishCallback(MtpTaskJob *job, void *arg)
{
	MtpTaskQueue *queue = (MtpTaskQueue *)arg;

	if (queue == NULL)
		return;
	SharedPtr<JobItem> item(new MtpTaskQueueFinishJob(queue, queue->m_evf, job));

	queue->m_finishQueue->Enqueue(item);
}
