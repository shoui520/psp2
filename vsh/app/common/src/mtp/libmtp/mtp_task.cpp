/*
 * The task queue's tasks: what a task is for (its title's kind and priority, its context and
 * event) and the job that runs the host's operations for it.
 */
#include "mtp_queue.h"

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
		m_event = (MtpEvent *)operator new(sizeof(MtpEvent), g_mtpUtilHeap);
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
