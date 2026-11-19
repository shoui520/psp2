/*
 * The task queue's tasks (mtp_queue.cpp, mtp_task.cpp): what a task is for and the job that
 * runs it.
 */
#ifndef MTP_QUEUE_H
#define MTP_QUEUE_H

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

#endif
