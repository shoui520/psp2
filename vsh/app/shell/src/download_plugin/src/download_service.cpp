/*
 * download_service.cpp: the shell's "SceDownloadService" IPMI server (0x8113A18E-0x8113AC58):
 * the service made and destroyed with download_plugin, its dispatcher thread, its clients'
 * sessions, the state the foreground installs share through it, and the jobs that a client's
 * method queues on the service's job queue. The file's name and every name in it are ours
 * (PLACEHOLDER): no record or message names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/list>
#include <thread/thread.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <ipmi/server.h>
#include <ipmi/session.h>

#include "../../esecron/include/esebase.hpp"
#include "../../esecron/include/esetask.hpp"
#include "../../esecron/include/eseutil.hpp"
#include "src/livespace_db/lsdb_keys.h"
#include "download_service.h"

using namespace paf;

/* the service's job queue and dispatcher thread */
#define DOWNLOAD_SVC_PRIORITY		0x1000010A
#define DOWNLOAD_SVC_QUEUE_STACK	0x6000
#define DOWNLOAD_SVC_THREAD_STACK	0x4000
#define DOWNLOAD_SVC_REQUEST_SIZE	7680		/* a request's and a message's room */
#define DOWNLOAD_SVC_SERVER_MEMORY	256

static DownloadService *s_instance;			/* (0x815448BC) */

list<DownloadServiceJob *> DownloadServiceJob::s_jobs;
thread::Mutex DownloadServiceJob::s_lock("SceDownloadServiceJobMutex", 0);

static int Bgdl_8113A632(void *arg);

/* the service made and its dispatcher started */
int DownloadService::Init(void)
{
	if (m_server != NULL)
		return DOWNLOAD_SVC_ERROR_BUSY;
	sce_paf_memset(m_sessions, 0, sizeof(m_sessions));
	m_sessionCount = 0;

	IPMI::Server::Config config;

	config.budget = IPMI::USER_BUDGET;
	sce_paf_strncpy(config.serviceName, "SceDownload", sizeof(config.serviceName));
	config.maxRequestSize = DOWNLOAD_SVC_REQUEST_SIZE;
	config.maxMessageSize = DOWNLOAD_SVC_REQUEST_SIZE;
	config.pEventHandler = this;
	m_serverMemory = new SceUInt8[DOWNLOAD_SVC_SERVER_MEMORY];
	if (m_serverMemory == NULL)
		return DOWNLOAD_SVC_ERROR_NO_MEMORY;
	m_workBufferSize = IPMI::Server::getWorkBufferSize(&config);
	m_workBuffer = new SceUInt8[m_workBufferSize];
	if (m_workBuffer == NULL) {
		delete[] m_serverMemory;
		return DOWNLOAD_SVC_ERROR_NO_MEMORY;
	}

	int ret = IPMI::Server::create(&m_server, &config, this, m_serverMemory);

	if (ret != 0) {
		delete[] m_serverMemory;
		delete[] m_workBuffer;
		return ret;
	}

	job::JobQueue::Option option;

	option.priority = DOWNLOAD_SVC_PRIORITY;
	option.stackSize = DOWNLOAD_SVC_QUEUE_STACK;
	option.numThreads = 1;
	m_jobQueue = new job::JobQueue("SceDownloadServiceJobQueue", &option);
	if (m_jobQueue == NULL) {
		m_server->destroy();
		delete[] m_serverMemory;
		delete[] m_workBuffer;
		return DOWNLOAD_SVC_ERROR_NO_MEMORY;
	}
	m_cancelLock.Lock();
	m_stopped = false;
	m_cancelLock.Unlock();
	m_thread = new DownloadServiceThread();
	if (m_thread == NULL) {
		m_server->destroy();
		delete[] m_serverMemory;
		delete[] m_workBuffer;
		delete m_jobQueue;
		m_jobQueue = NULL;
		return DOWNLOAD_SVC_ERROR_INTERNAL;
	}
	m_thread->Start(Bgdl_8113A632, this);
	sce_paf_memset(m_state, 0, sizeof(m_state));
	sce_paf_memset(&m_result, 0, sizeof(m_result));
	s_instance = this;
	return 0;
}

/* the dispatcher stopped and the service destroyed, once no client is left */
int DownloadService::Term(void)
{
	if (m_server == NULL)
		return DOWNLOAD_SVC_ERROR_NOT_STARTED;
	if (m_sessionCount != 0)
		return DOWNLOAD_SVC_ERROR_BUSY;
	if (m_thread != NULL) {
		m_cancelLock.Lock();
		if (!m_stopped && m_server != NULL)
			m_server->shutdownDispatcher();
		m_stopped = true;
		m_cancelLock.Unlock();
		m_thread->Join();
		delete m_thread;
		m_thread = NULL;
	}
	if (m_jobQueue != NULL) {
		m_jobQueue->CancelAllItems();
		m_jobQueue->WaitEmpty();
		delete m_jobQueue;
		m_jobQueue = NULL;
	}

	int ret = m_server->destroy();

	if (ret != 0)
		return ret;
	m_server = NULL;
	delete[] m_serverMemory;
	m_serverMemory = NULL;
	delete[] m_workBuffer;
	m_workBuffer = NULL;
	s_instance = NULL;
	return 0;
}

__attribute__((noinline)) DownloadService *DownloadService::Bgdl_8113A434(void)
{
	return s_instance;
}

/* the state set, the second one when second is set */
void DownloadService::Bgdl_8113A440(const BgdlDispatcherState *state, bool second)
{
	m_dataLock.Lock();
	if (second)
		sce_paf_memcpy(&m_state[1], state, sizeof(m_state[1]));
	else
		sce_paf_memcpy(&m_state[0], state, sizeof(m_state[0]));
	m_dataLock.Unlock();
}

/* and read back */
void DownloadService::Bgdl_8113A47C(BgdlDispatcherState *state, bool second)
{
	m_dataLock.Lock();
	if (second)
		sce_paf_memcpy(state, &m_state[1], sizeof(*state));
	else
		sce_paf_memcpy(state, &m_state[0], sizeof(*state));
	m_dataLock.Unlock();
}

/* a title install's result kept */
void DownloadService::Bgdl_8113A4B8(const int *result)
{
	m_dataLock.Lock();
	if (result != NULL)
		m_result = *result;
	m_dataLock.Unlock();
}

/* and read back */
void DownloadService::Bgdl_8113A4DA(int *result)
{
	m_dataLock.Lock();
	if (result != NULL)
		*result = m_result;
	m_dataLock.Unlock();
}

/* a job queued on the service's queue */
__attribute__((noinline)) int DownloadService::Bgdl_8113A4FC(common::SharedPtr<job::JobItem> &job)
{
	if (m_jobQueue != NULL)
		return m_jobQueue->Enqueue(job);
	return DOWNLOAD_SVC_ERROR_INTERNAL;
}

void DownloadService::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					   const IPMI::DataInfo *pInList, SceUInt32 inCount,
					   IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	Bgdl_8113BCFE(pSession, methodId, pInList, inCount, pOutList, outCount);
}

void DownloadService::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					    SceUInt32 methodId, const IPMI::DataInfo *pInList,
					    SceUInt32 inCount)
{
	Bgdl_8113C220(pSession, requestId, methodId, pInList, inCount);
}

int DownloadService::onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				 const void *pParam, SceSize paramSize)
{
	if (paramSize != DOWNLOAD_SVC_CLIENT_PARAM_SIZE)
		return DOWNLOAD_SVC_ERROR_INVALID_ARGUMENT;
	if (m_sessionCount == DOWNLOAD_SVC_MAX_SESSIONS)
		return DOWNLOAD_SVC_ERROR_TOO_MANY_CLIENTS;
	m_sessionCount++;

	IPMI::Session **slot = NULL;

	for (SceUInt32 i = 0; i < DOWNLOAD_SVC_MAX_SESSIONS; i++) {
		if (m_sessions[i] == NULL) {
			slot = &m_sessions[i];
			break;
		}
	}

	SceUInt8 *memory = new SceUInt8[pConfig->bufferSize];

	if (memory == NULL) {
		m_sessionCount--;
		return DOWNLOAD_SVC_ERROR_NO_MEMORY;
	}
	if (pServer->createSession(slot, this, memory) == (int)0x8002050C) {
		delete[] memory;
		return DOWNLOAD_SVC_ERROR_NO_PARAM;
	}
	return 0;
}

static void Bgdl_8113A714(IPMI::Session *session);

void DownloadService::onSessionClosedByClient(IPMI::Session *pSession)
{
	Bgdl_8113A714(pSession);

	IPMI::Session **slot = NULL;

	for (SceUInt32 i = 0; i < DOWNLOAD_SVC_MAX_SESSIONS; i++) {
		if (m_sessions[i] == pSession) {
			slot = &m_sessions[i];
			break;
		}
	}
	pSession->destroy();
	delete[] (SceUInt8 *)pSession;
	*slot = NULL;
	m_sessionCount--;
}

/* the dispatcher thread's function */
static int Bgdl_8113A632(void *arg)
{
	DownloadService *service = (DownloadService *)arg;

	service->m_server->runDispatcher(service->m_workBuffer, service->m_workBufferSize);
	return 0;
}

/* the service stopped and deleted, waiting while clients are left */
int BgdlJobDispatcher::Bgdl_8113A644(void)
{
	if (m_service != NULL) {
		int ret;

		do {
			ret = m_service->Term();
			thread::Sleep(0);
		} while (ret == DOWNLOAD_SVC_ERROR_BUSY);
		delete m_service;
		m_service = NULL;
	}
	return 0;
}

BgdlJobDispatcher::~BgdlJobDispatcher()
{
	if (m_service != NULL) {
		int ret;

		do {
			ret = m_service->Term();
			thread::Sleep(0);
		} while (ret == DOWNLOAD_SVC_ERROR_BUSY);
		delete m_service;
		m_service = NULL;
	}
}

/* the service made and started */
int BgdlJobDispatcher::Bgdl_8113A6A8(void)
{
	int ret = -1;

	if (m_service == NULL) {
		m_service = new DownloadService();
		if (m_service != NULL) {
			ret = m_service->Init();
			m_result = ret;
		}
	}
	return ret;
}

/* a closed session's jobs cancelled */
__attribute__((noinline)) static void Bgdl_8113A714(IPMI::Session *session)
{
	DownloadServiceJob::s_lock.Lock();
	for (list<DownloadServiceJob *>::iterator it = DownloadServiceJob::s_jobs.begin();
	     it != DownloadServiceJob::s_jobs.end(); ++it) {
		if ((*it)->m_session == session)
			(*it)->Cancel();
	}
	DownloadServiceJob::s_lock.Unlock();
}

/* the queue checked, when ux0: is there */
int Bgdl_8113A76C(IPMI::Session *session, const void *arg)
{
	if (Bgdl_81112284("ux0:") == true)
		return bgdlCheckQueue();
	return BGDL_ERROR_NO_UX0;
}

/* an auto download's job queued; its reply sent when it cannot be */
int Bgdl_8113A78C(IPMI::Session *session, SceUInt32 requestId, const DownloadServiceParam *param)
{
	int ret;

	{
		common::SharedPtr<job::JobItem> job(new DownloadServiceAutoJob(session, requestId, param));

		if (job.get() == NULL)
			ret = DOWNLOAD_SVC_ERROR_NO_JOB;
		else
			ret = DownloadService::Bgdl_8113A434()->Bgdl_8113A4FC(job);
	}
	if (ret < 0)
		Bgdl_8113BC9A(session, requestId, ret);
	return 0;
}

/* a title's job queued; its reply sent when it cannot be */
int Bgdl_8113A982(IPMI::Session *session, SceUInt32 requestId, const char *id)
{
	int ret;

	{
		common::SharedPtr<job::JobItem> job(new DownloadServiceTitleJob(session, requestId, id));

		if (job.get() == NULL)
			ret = DOWNLOAD_SVC_ERROR_NO_JOB;
		else
			ret = DownloadService::Bgdl_8113A434()->Bgdl_8113A4FC(job);
	}
	if (ret < 0)
		Bgdl_8113BCBC(session, requestId, ret);
	return 0;
}
