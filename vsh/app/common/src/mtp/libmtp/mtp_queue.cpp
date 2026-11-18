/*
 * The task queue ("QUEUE:"): each request of the API's callers becomes a task, a job that runs
 * the host's operations for it through the command handler. The tasks wait in a list in order
 * of priority; the one at the front runs on paf's job queue, and when it finishes a job on the
 * finish queue takes it off the list and starts the next. While the queue has nothing to do,
 * an idle task holds the front and answers the host.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include <rtc.h>
#include <std/list>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <thread/thread.h>
#include "mtp_internal.h"

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

using paf::common::SharedPtr;
using paf::job::JobItem;
using paf::job::JobQueue;
using paf::thread::Mutex;
using paf::thread::MutexLock;

/* the idle task's event code */
#define MTP_TASK_EVENT_IDLE		0xFFFF

/* the titles whose tasks run differently */
#define MTP_TASK_KIND_NONE		0
#define MTP_TASK_KIND_SYSTEM		1	/* NPXS10004, NPXS10009, NPXS10010 */
#define MTP_TASK_KIND_CONTENT_MANAGER	2	/* NPXS10026 */
#define MTP_TASK_KIND_SETTINGS		3	/* NPXS10015 */
#define MTP_TASK_KIND_BACKGROUND	4	/* NPXS10100, the background copy */
#define MTP_TASK_KIND_TEST		5	/* NPXS19999 */

/* a task's priority: the higher runs first */
#define MTP_TASK_PRIORITY_IDLE		0
#define MTP_TASK_PRIORITY_BACKGROUND	1
#define MTP_TASK_PRIORITY_NORMAL	2
#define MTP_TASK_PRIORITY_URGENT	3

/* a task job's state */
#define MTP_TASK_JOB_STATE_IDLE		0
#define MTP_TASK_JOB_STATE_RUNNING	1
#define MTP_TASK_JOB_STATE_WAIT_COMMAND	2
#define MTP_TASK_JOB_STATE_FINISHED	3

/* a task job's receive event flag: a command to run, or the end of the task */
#define MTP_TASK_EVF_COMMAND		0x10000
#define MTP_TASK_EVF_END		0x20000

/* the queue's and the job's other event flags */
#define MTP_TASK_EVF_CURRENT		0x1	/* a task is running */
#define MTP_TASK_EVF_RESULT		0x1	/* the command's result is ready */

#define MTP_TASK_WAIT_CURRENT_TIMEOUT	5000000		/* us */
#define MTP_TASK_POLL_INTERVAL		10000
#define MTP_TASK_CANCEL_DELAY		100000

/* the operations a task treats specially */
#define MTP_OPERATION_GET_OBJECT	0x1009
#define MTP_OPERATION_GET_THUMB		0x100D
#define MTP_OPERATION_END_TASK		0x9518
#define MTP_OPERATION_HTTP_GET_DATA_EX	0xC10D
#define MTP_OPERATION_HTTP_GET_STATUS	0xC112
#define MTP_OPERATION_HTTP_CANCEL	0xC125

#define MTP_STATUS_STATE_DISCONNECTED	8

/* a command handed to the running task: the operation, and the bits to set */
typedef struct MtpTaskRequest {
	MtpOperation operation;
	SceUInt32 bits;			/* MTP_TASK_EVF_* */
} MtpTaskRequest;

class MtpTaskJob;
class MtpTaskQueue;

typedef void (*MtpTaskJobCallback)(MtpTaskJob *job, void *arg);

/* what a task is for: the context it runs and the event it was made for */
class MtpTaskInfo {
public:
	MtpTaskInfo();
	virtual ~MtpTaskInfo();
	int SetKind(const char *titleId);
	void Set(MtpContext *context, const MtpEvent *event);
	SharedPtr<MtpContext> GetContext();
	int GetKind();
	int GetPriority();
	int IsRunnable();

	int m_kind;			/* MTP_TASK_KIND_* */
	int m_priority;			/* MTP_TASK_PRIORITY_* */
	SharedPtr<MtpContext> m_context;
	MtpEvent *m_event;		/* on the utilities' heap */
};

/* a task as paf's job queue runs it */
class MtpTaskJob : public JobItem {
public:
	MtpTaskJob(MtpTaskInfo *info, MtpTaskJobCallback run, MtpCommandHandler handler,
		MtpTaskJobCallback finish, void *arg);
	virtual ~MtpTaskJob();
	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);
	MtpTaskInfo *GetInfo();
	int Initialize();
	int PushCommand(const MtpTaskRequest *request);
	int WaitResult();
	void CancelIfIdle(int priority);
	int GetState();
	int GetResult();
	void SetQueued();
	int IsQueued();

	SceUID m_recvEvf;		/* MTP_TASK_EVF_COMMAND, MTP_TASK_EVF_END */
	SceUID m_waitEvf;		/* MTP_TASK_EVF_RESULT */
	MtpTaskJobCallback m_run;
	MtpTaskJobCallback m_finish;
	MtpCommandHandler m_handler;
	void *m_arg;
	MtpTaskInfo *m_info;
	MtpTaskRequest m_request;
	int m_result;
	int m_state;			/* MTP_TASK_JOB_STATE_* */
	Mutex *m_mutex;
	int m_queued;
};

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
			if (state > MTP_TASK_JOB_STATE_RUNNING)
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
		    (mtpUtilStrncmp(m_titleId, "NPXS10100", sizeof(m_titleId)) == 0 ||
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

MtpTaskInfo::MtpTaskInfo() : m_kind(MTP_TASK_KIND_NONE), m_priority(MTP_TASK_PRIORITY_IDLE),
	m_context(), m_event(NULL)
{
}

MtpTaskInfo::~MtpTaskInfo()
{
	if (m_event != NULL) {
		delete m_event;
		m_event = NULL;
	}
}

int MtpTaskInfo::SetKind(const char *titleId)
{
	if (mtpUtilStrncmp(titleId, "NPXS10100", 32) == 0)
		m_kind = MTP_TASK_KIND_BACKGROUND;
	else if (mtpUtilStrncmp(titleId, "NPXS10026", 32) == 0)
		m_kind = MTP_TASK_KIND_CONTENT_MANAGER;
	else if (mtpUtilStrncmp(titleId, "NPXS10015", 32) == 0)
		m_kind = MTP_TASK_KIND_SETTINGS;
	else if (mtpUtilStrncmp(titleId, "NPXS10004", 32) == 0 ||
		 mtpUtilStrncmp(titleId, "NPXS10009", 32) == 0 ||
		 mtpUtilStrncmp(titleId, "NPXS10010", 32) == 0)
		m_kind = MTP_TASK_KIND_SYSTEM;
	else if (mtpUtilStrncmp(titleId, "NPXS19999", 32) == 0)
		m_kind = MTP_TASK_KIND_TEST;
	else
		m_kind = MTP_TASK_KIND_NONE;
	return m_kind;
}

void MtpTaskInfo::Set(MtpContext *context, const MtpEvent *event)
{
	if (context != NULL)
		m_context.reset(context);
	else
		m_context.reset();
	if (m_event == NULL)
		m_event = (MtpEvent *)ScePafStdc_D00387E7(sizeof(MtpEvent), g_mtpUtilHeap);
	if (m_event == NULL)
		return;
	if (event != NULL)
		mtpUtilMemcpy(m_event, event, sizeof(*m_event));
	else
		mtpUtilMemset(m_event, 0, sizeof(*m_event));
	switch (m_event->code) {
	case MTP_TASK_EVENT_IDLE:
		m_priority = MTP_TASK_PRIORITY_IDLE;
		break;
	case MTP_OPERATION_HTTP_GET_DATA_EX:
	case MTP_OPERATION_HTTP_GET_STATUS:
	case MTP_OPERATION_HTTP_CANCEL:
		m_priority = MTP_TASK_PRIORITY_URGENT;
		break;
	default:
		if (m_kind == MTP_TASK_KIND_BACKGROUND)
			m_priority = MTP_TASK_PRIORITY_BACKGROUND;
		else
			m_priority = MTP_TASK_PRIORITY_NORMAL;
		break;
	}
}

SharedPtr<MtpContext> MtpTaskInfo::GetContext()
{
	return m_context;
}

int MtpTaskInfo::GetKind()
{
	return m_kind;
}

int MtpTaskInfo::GetPriority()
{
	return m_priority;
}

/* whether the task may run while the host is in the state it is */
static inline int isRunnable(MtpTaskInfo *info)
{
	MtpStatus status;
	int runnable = 0;

	mtpStatusGet(&status);
	if (info->m_event->code == MTP_TASK_EVENT_IDLE ||
	    info->m_priority == MTP_TASK_PRIORITY_URGENT ||
	    ((info->m_kind == MTP_TASK_KIND_SETTINGS || info->m_kind == MTP_TASK_KIND_TEST) &&
	     status.arg != 0) ||
	    (info->m_kind != MTP_TASK_KIND_SETTINGS && info->m_kind != MTP_TASK_KIND_TEST &&
	     status.arg == 1))
		runnable = 1;
	return runnable;
}

int MtpTaskInfo::IsRunnable()
{
	return isRunnable(this);
}

MtpTaskJob::MtpTaskJob(MtpTaskInfo *info, MtpTaskJobCallback run, MtpCommandHandler handler,
	MtpTaskJobCallback finish, void *arg)
	: JobItem("SceMtpTaskJob", NULL), m_recvEvf(-1), m_waitEvf(-1), m_run(run),
	  m_finish(finish), m_handler(handler), m_arg(arg), m_info(info), m_result(0),
	  m_state(MTP_TASK_JOB_STATE_IDLE), m_mutex(NULL), m_queued(0)
{
	mtpUtilMemset(&m_request, 0, sizeof(m_request));
}

MtpTaskJob::~MtpTaskJob()
{
	if (m_info != NULL) {
		delete m_info;
		m_info = NULL;
	}
	if (m_mutex != NULL) {
		delete m_mutex;
		m_mutex = NULL;
	}
	if (m_recvEvf != -1) {
		sceKernelDeleteEventFlag(m_recvEvf);
		m_recvEvf = -1;
	}
	if (m_waitEvf != -1) {
		sceKernelDeleteEventFlag(m_waitEvf);
		m_waitEvf = -1;
	}
}

MtpTaskInfo *MtpTaskJob::GetInfo()
{
	return m_info;
}

/* hand the command's result to the thread that pushed it */
static inline void setResult(MtpTaskJob *job, int result)
{
	job->m_result = result;
	sceKernelSetEventFlag(job->m_waitEvf, MTP_TASK_EVF_RESULT);
}

/*
 * Run the task: tell the host the request's event, then run each command it sends for the
 * task through the command handler and answer it, until the task ends.
 */
int MtpTaskJob::Run()
{
	MtpTaskInfo *info = m_info;
	MtpEvent *event = info->m_event;
	SharedPtr<MtpContext> context = info->m_context;
	int ret = 0;

	if (context.get() != NULL) {
		SceRtcTick tick;

		sceRtcGetCurrentTick(&tick);
		context->startTick = tick.tick;
	}
	if (event == NULL || event->code != MTP_TASK_EVENT_IDLE) {
		mtpContextRegister(context);
		if (!IsCanceled()) {
			if (!isRunnable(m_info))
				return SCE_MTP_ERROR_NOT_PERMITTED;
			mtpBgCopyRestartIdleTimer();
			ret = mtpCoreSendResponse(event, context->id);
			if (ret < 0)
				return ret;
		}
	}
	if (m_run != NULL)
		m_run(this, m_arg);
	for (;;) {
		SceUInt32 outBits = 0;
		MtpTaskRequest request;
		MtpOperation response;
		int last = 0;
		int done = 0;
		int wait;
		int result;

		{
			MutexLock lock(m_mutex);

			m_state = MTP_TASK_JOB_STATE_WAIT_COMMAND;
		}
		wait = sceKernelWaitEventFlag(m_recvEvf, MTP_TASK_EVF_COMMAND | MTP_TASK_EVF_END,
			SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL, &outBits, NULL);
		{
			MutexLock lock(m_mutex);

			m_state = MTP_TASK_JOB_STATE_RUNNING;
		}
		if (wait < 0) {
			ret = wait;
			MTP_ERR("QUEUE:", 274, "MTP receive command event error=0x%x\n", wait);
			break;
		}
		mtpUtilMemcpy(&request, &m_request, sizeof(request));
		if (!(outBits & MTP_TASK_EVF_COMMAND)) {
			if (!(outBits & MTP_TASK_EVF_END))
				continue;
			ret = wait;
			sceKernelSetEventFlag(m_waitEvf, MTP_TASK_EVF_RESULT);
			break;
		}
		m_result = 0;
		mtpUtilMemset(&response, 0, sizeof(response));
		mtpDevIfChangePhase(1);
		mtpCoreSetPublicStatus(5, -1);
		mtpCoreWaitSema();
		result = m_handler(&request.operation, &response);
		mtpCoreSignalSema();
		if (result < 0 && result != -2) {
			MTP_ERR("QUEUE:", 296, "MTP command handler error=0x%x\n", result);
			if (mtpDevGetIfType() == MTP_IF_TYPE_NET && result == MTP_ERROR_PROTOCOL)
				mtpStatusSetState(0, MTP_STATUS_STATE_DISCONNECTED, 0);
			ret = result;
			setResult(this, result);
			if (result != MTP_ERROR_CANCELLED ||
			    request.operation.code != MTP_OPERATION_GET_OBJECT)
				break;
			if (context.get() == NULL)
				break;
			if (mtpContextGetPauseContextId() != context->id ||
			    mtpStatusGetPlatformClass() != MTP_PLATFORM_CLASS_PS)
				break;
			response.code = MTP_RESPONSE_TRANSACTION_CANCELLED;
			sceKernelDelayThread(MTP_TASK_CANCEL_DELAY);
			last = 1;
		}
		mtpDevIfChangePhase(8);
		result = mtpDevIfSendResponse(&response);
		if (result < 0) {
			ret = result;
			MTP_ERR("QUEUE:", 323, "failed in sending response=0x%x\n", result);
			if (last)
				break;
			setResult(this, result);
		}
		ret = result;
		if (last)
			break;
		setResult(this, result);
		if ((request.operation.code == MTP_OPERATION_END_TASK &&
		     SharedPtr<MtpContext>(m_info->m_context).get() != NULL &&
		     SharedPtr<MtpContext>(m_info->m_context)->id == request.operation.param[0]) ||
		    ret < 0)
			done = 1;
		if (done)
			break;
	}
	return ret;
}

void MtpTaskJob::Finish(int result)
{
	{
		MutexLock lock(m_mutex);

		m_state = MTP_TASK_JOB_STATE_FINISHED;
	}
	if (m_finish != NULL)
		m_finish(this, m_arg);
}

void MtpTaskJob::Cancel()
{
	sceKernelSetEventFlag(m_recvEvf, MTP_TASK_EVF_END);
	JobItem::Cancel();
}

int MtpTaskJob::Initialize()
{
	int ret;

	m_mutex = new Mutex("SceMtpTaskJobMutex", 0);
	if (m_mutex == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	ret = sceKernelCreateEventFlag("SceMtpTaskJobRecvCommand", 0, 0, NULL);
	if (ret < 0)
		return ret;
	m_recvEvf = ret;
	ret = sceKernelCreateEventFlag("SceMtpTaskJobWaitCommand", 0, 0, NULL);
	if (ret < 0)
		return ret;
	m_waitEvf = ret;
	return ret;
}

int MtpTaskJob::PushCommand(const MtpTaskRequest *request)
{
	int ret;

	mtpUtilMemcpy(&m_request, request, sizeof(m_request));
	ret = sceKernelSetEventFlag(m_recvEvf, request->bits);
	if (ret >= 0)
		return 0;
	return ret;
}

int MtpTaskJob::WaitResult()
{
	SceUInt32 outBits = 0;
	int ret;

	ret = sceKernelWaitEventFlag(m_waitEvf, MTP_TASK_EVF_RESULT,
		SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL, &outBits, NULL);
	if (ret >= 0)
		ret = m_result;
	return ret;
}

/* a task put before the idle task ends it */
void MtpTaskJob::CancelIfIdle(int priority)
{
	if (m_info->m_event->code == MTP_TASK_EVENT_IDLE)
		Cancel();
}

int MtpTaskJob::GetState()
{
	MutexLock lock(m_mutex);

	return m_state;
}

int MtpTaskJob::GetResult()
{
	return m_result;
}

void MtpTaskJob::SetQueued()
{
	m_queued = 1;
}

int MtpTaskJob::IsQueued()
{
	return m_queued;
}
