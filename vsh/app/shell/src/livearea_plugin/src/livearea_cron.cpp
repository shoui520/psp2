/*
 * livearea_cron.cpp: livearea_plugin daily cron job scheduler ("SceLiveAreaDailyCronJob";
 * 0x81199DA8-0x8119A408, sti 0x81199DA8). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <thread/thread.h>
#include <job/job.h>
#include <common/main_thread_call_list.h>
#include <std/map>
#include "../../../shell_cron.h"
#include "livearea_internal.h"

extern "C" int sceKernelPowerTick(SceUInt32 type);

/* 0x8141FF6C */
void liveareaDisableAutoSuspendCb(void *arg)
{
	(void)arg;
	sceKernelPowerTick(4);
}

class LiveAreaDisableAutoSuspendJob : public paf::job::JobItem {
public:
	typedef void (*DoneCallback)(void);

	LiveAreaDisableAutoSuspendJob(DoneCallback doneCb)
		: paf::job::JobItem("SceLiveAreaDiableAutoSuspend", NULL),
		  m_doneCb(doneCb)
	{
		paf::common::MainThreadCallList::Register(liveareaDisableAutoSuspendCb, this);
	}

	virtual ~LiveAreaDisableAutoSuspendJob() {}
	virtual int Run() { return 0; }
	virtual void Finish(int result)
	{
		(void)result;
		if (m_doneCb != NULL)
			m_doneCb();
	}

	DoneCallback m_doneCb;
};

class LiveAreaDailyCronJob {
public:
	typedef void (*CronCallback)(void *);

	LiveAreaDailyCronJob();					/* 0x81199E1A */
	~LiveAreaDailyCronJob();				/* 0x8119A3A8 */

	static LiveAreaDailyCronJob &GetInstance()
	{
		static LiveAreaDailyCronJob s_instance;
		return s_instance;
	}

	static int Enqueue(paf::common::SharedPtr<paf::job::JobItem> &item); /* 0x81199EC0 */
	static void RegisterCallback(CronCallback cb, void *arg);	/* 0x81199F26 */
	static void UnregisterCallback(CronCallback cb);		/* 0x81199FC0 */
	static int Start(void);						/* 0x8119A072 */
	void EnqueueDisableAutoSuspend(void);				/* 0x8119A14E */
	static int OnCronCallback(SceUID handle, void *userdata);	/* 0x8119A244 */
	static void OnDoneCallback(void);				/* 0x8119A346 */

	paf::thread::RMutex m_mutex;				/* +0x00 */
	paf::job::JobQueue *m_queue;				/* +0x20 */
	SceInt64 m_reserved;					/* +0x28 */
	SceUID m_cronId;					/* +0x30 */
	paf::map<CronCallback, void *> m_callbacks;		/* +0x34 */
	bool m_running;						/* +0x40 */
};

/* 0x81199E1A */
LiveAreaDailyCronJob::LiveAreaDailyCronJob()
	: m_mutex("SceLiveAreaDailyCronJob", 0),
	  m_queue(NULL),
	  m_reserved(0),
	  m_cronId(0),
	  m_callbacks(),
	  m_running(false)
{
	paf::job::JobQueue::Option option;
	option.priority = 0x1000010A;
	option.stackSize = 0x4000;
	option.numThreads = 1;
	m_queue = new paf::job::JobQueue("SceLiveAreaDailyCronJob", &option);
}

/* 0x81199EC0 */
int LiveAreaDailyCronJob::Enqueue(paf::common::SharedPtr<paf::job::JobItem> &item)
{
	return GetInstance().m_queue->Enqueue(item);
}

/* 0x81199F26 */
void LiveAreaDailyCronJob::RegisterCallback(CronCallback cb, void *arg)
{
	LiveAreaDailyCronJob &inst = GetInstance();
	inst.m_mutex.Lock();
	inst.m_callbacks.insert(paf::pair<CronCallback, void *>(cb, arg));
	inst.m_mutex.Unlock();
}

/* 0x81199FC0 */
void LiveAreaDailyCronJob::UnregisterCallback(CronCallback cb)
{
	LiveAreaDailyCronJob &inst = GetInstance();
	inst.m_mutex.Lock();
	paf::map<CronCallback, void *>::iterator it = inst.m_callbacks.find(cb);
	if (it != inst.m_callbacks.end())
		inst.m_callbacks.erase(it);
	inst.m_mutex.Unlock();
}

/* 0x8119A072 */
int LiveAreaDailyCronJob::Start(void)
{
	LiveAreaDailyCronJob &inst = GetInstance();
	ShellCron *cron = shellCronGetInstance();
	if (cron == NULL)
		return (int)0x80107901;

	ShellCronOption option;
	option.wakeUp = true;
	option.persist = false;
	int secondOfDay = shellCronGetInstance()->GetDailyTime();
	int ret = cron->AddDaily(&inst.m_cronId, OnCronCallback, &inst,
				 secondOfDay, SHELL_CRON_EVERY_DAY, &option);
	if (ret != 0)
		return ret;
	return 0;
}

int liveareaCronEnqueueJob(paf::common::SharedPtr<paf::job::JobItem> &item)
{
	return LiveAreaDailyCronJob::Enqueue(item);
}

void liveareaCronRegister(void (*cb)(void *), void *arg)
{
	LiveAreaDailyCronJob::RegisterCallback(cb, arg);
}

void liveareaCronUnregister(void (*cb)(void *))
{
	LiveAreaDailyCronJob::UnregisterCallback(cb);
}

int shellLiveAreaDailyCronStart(void)
{
	return LiveAreaDailyCronJob::Start();
}

/* 0x8119A14E */
void LiveAreaDailyCronJob::EnqueueDisableAutoSuspend(void)
{
	paf::job::JobItem *raw = new LiveAreaDisableAutoSuspendJob(OnDoneCallback);
	paf::common::SharedPtr<paf::job::JobItem> sp(raw);
	m_queue->Enqueue(sp);
}

/* 0x8119A244 */
int LiveAreaDailyCronJob::OnCronCallback(SceUID handle, void *userdata)
{
	(void)handle;
	LiveAreaDailyCronJob *self = (LiveAreaDailyCronJob *)userdata;
	paf::map<CronCallback, void *> copy;

	if (!self->m_running) {
		self->m_running = true;
		self->m_mutex.Lock();
		copy = self->m_callbacks;
		self->m_mutex.Unlock();

		for (paf::map<CronCallback, void *>::iterator it = copy.begin();
		     it != copy.end(); ++it) {
			it->first(it->second);
		}
		self->EnqueueDisableAutoSuspend();
	}
	return 0;
}

/* 0x8119A346 */
void LiveAreaDailyCronJob::OnDoneCallback(void)
{
	GetInstance().m_running = false;
}

/* 0x8119A3A8 */
LiveAreaDailyCronJob::~LiveAreaDailyCronJob()
{
	ShellCron *cron = shellCronGetInstance();
	if (cron != NULL && cron->Remove(m_cronId) == 0)
		m_cronId = 0;

	if (m_queue != NULL) {
		m_queue->CancelAllItems();
		delete m_queue;
		m_queue = NULL;
	}
	m_reserved = 0;
}
