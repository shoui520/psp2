/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * PowerTickGuard (auto-suspend prevention), RegDateFormatTask, PlayCommonSE,
 * and TaskBase / JobTask / TaskManager single-slot async task runner.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <common/main_thread_call_list.h>
#include <common/shared_ptr.h>
#include <framework.h>
#include <job/job.h>
#include <regmgr/regmgr.h>
#include "signin_ext_internal.h"

extern "C" {
int sceKernelPowerTick(int type);
}

using namespace paf;
using namespace paf::common;
using namespace paf::job;

#define REGMGR_DATE_FORMAT_PARAM_ID	558966

static const int32_t s_commonSeIds[6] = { 1, 2, 3, 4, 12, 13 };
static TaskBase *s_activeTask = NULL;

/* ---- PowerTickGuard ---- */

PowerTickGuard::PowerTickGuard() : m_registered(false)
{
}

PowerTickGuard::~PowerTickGuard()
{
	if (m_registered) {
		MainThreadCallList::Unregister(OnTick, this);
		m_registered = false;
	}
}

void PowerTickGuard::Enable(void)
{
	if (!m_registered) {
		m_registered = true;
		MainThreadCallList::Register(OnTick, this);
	}
}

void PowerTickGuard::OnTick(void *userData)
{
	(void)userData;
	sceKernelPowerTick(1);
}

/* ---- RegDateFormatTask ---- */

RegDateFormatTask::RegDateFormatTask(Callback1<int32_t> *callback)
	: JobTask(SigninExtGetJobQueue(), callback, "SceSigninExtReg")
{
}

RegDateFormatTask::~RegDateFormatTask()
{
}

int32_t RegDateFormatTask::Run(void)
{
	int32_t regFormat = 0;
	int32_t ret = sceRegMgrSystemParamGetInt(REGMGR_DATE_FORMAT_PARAM_ID, &regFormat);
	if (ret < 0)
		return ret;

	BirthDateModel date;
	if (regFormat == 1)
		date.m_dateFormat = 2;
	else if (regFormat == 2)
		date.m_dateFormat = 1;
	else
		date.m_dateFormat = 0;

	g_dobVerifyModel->m_birthDate.CopyFrom(date);
	return ret;
}

/* ---- Common sound effect helper ---- */

void SigninExtPlayCommonSE(uint32_t index)
{
	if (index <= 5) {
		Framework *fw = Framework::Instance();
		fw->PlayCommonSE(s_commonSeIds[index], 1.0f, false);
	}
}

/* ---- TaskManager ---- */

bool SigninExtTaskIsRunning(void)
{
	return s_activeTask != NULL;
}

void SigninExtTaskCancel(void)
{
	if (s_activeTask != NULL)
		s_activeTask->Cancel();
}

void SigninExtTaskStart(TaskBase *task)
{
	if (task == NULL)
		return;

	if (s_activeTask != NULL) {
		task->OnComplete(SIGNIN_EXT_ERROR_BUSY);
		delete task;
		return;
	}

	s_activeTask = task;
	task->Start();
}

/* ---- TaskBase ---- */

TaskBase::TaskBase(Callback1<int32_t> *callback)
	: m_callback(callback)
{
}

TaskBase::~TaskBase()
{
	if (m_callback != NULL) {
		delete m_callback;
		m_callback = NULL;
	}
}

void TaskBase::Complete(int32_t result)
{
	TaskBase *self = s_activeTask;
	s_activeTask = NULL;
	OnComplete(result);
	if (self != NULL)
		delete self;
}

void TaskBase::OnComplete(int32_t result)
{
	if (m_callback != NULL) {
		Callback1<int32_t> *cb = m_callback;
		m_callback = NULL;
		cb->Invoke(result);
		delete cb;
	}
}

/* ---- JobTask ---- */

JobTask::JobTask(JobQueue *queue, Callback1<int32_t> *callback, const char *name)
	: TaskBase(callback), JobItem(name, NULL), m_queue(queue)
{
}

JobTask::~JobTask()
{
}

void JobTask::Start(void)
{
	SharedPtr<JobItem> item(static_cast<JobItem *>(this));
	m_queue->Enqueue(item);
}

void JobTask::Cancel(void)
{
	JobItem::Cancel();
}

void JobTask::Complete(int32_t result)
{
	s_activeTask = NULL;
	OnComplete(result);
}

void JobTask::Finish(int32_t result)
{
	if (IsCanceled())
		result = -1;
	Complete(result);
}
