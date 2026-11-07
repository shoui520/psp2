/*
 * The check-in library: the near background service (NearBGService, NPXS10025) does the check-in
 * and the gift download; the library launches it with the mode as near_auto_checkin's URI, connects
 * to it over IPMI and polls its status in jobs on its own queue, one job at a time: the connection
 * (SceNearCheckinLibConnectJob), the status (SceNearCheckinLibStatusJob) and the disconnection
 * (SceNearCheckinLibTermJob). Sony's names are the jobs', the queue's and the mutex's, the
 * service's and the URI's modes (MODE_*); no record names the file, the classes or the functions
 * (the names are ours).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/libkernel.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <job/job.h>
#include "near_checkin_lib.h"

using namespace paf;

/* queues the job when the queue has none */
static inline int NearCheckinLibEnqueue(NearCheckinLib *lib, common::SharedPtr<job::JobItem> item)
{
	if (lib->m_queue->NumItems() != 0)
		return NearCheckinLib::RESULT_STATE;
	return lib->m_queue->Enqueue(item);
}

int NearCheckinLib::StartStatusJob(void)
{
	NearCheckinLibStatusJob *item;
	int ret;

	if (!m_initialized)
		return RESULT_STATE;
	if (m_state != STATE_STARTED)
		return RESULT_STATE;

	item = new NearCheckinLibStatusJob();

	common::SharedPtr<job::JobItem> job(item);

	if (job.get() == NULL)
		return RESULT_NO_MEMORY;
	ret = item->Setup(m_jobType, this, StatusJobEnd, UpdateCheckinStatus,
			  UpdateItemDownloadStatus);
	if (ret != 0)
		return ret;
	ret = NearCheckinLibEnqueue(this, job);
	if (ret != 0)
		return ret;
	m_job = job;
	return ret;
}

int NearCheckinLib::StartTermJob(NearCheckinLibFunction callback, void *userdata)
{
	NearCheckinLibTermJob *item;
	int ret;

	if (!m_initialized)
		return RESULT_STATE;
	if (m_state != STATE_TERMINATING)
		return RESULT_STATE;

	item = new NearCheckinLibTermJob();

	common::SharedPtr<job::JobItem> job(item);

	if (job.get() == NULL)
		return RESULT_NO_MEMORY;
	ret = item->Setup(m_jobType, this, TermJobEnd, NULL, NULL);
	if (ret != 0)
		return ret;
	ret = NearCheckinLibEnqueue(this, job);
	if (ret != 0)
		return ret;
	m_job = job;
	m_callback = callback;
	m_userdata = userdata;
	return ret;
}

int NearCheckinLib::Init(void)
{
	int ret;

	m_mutex = new thread::Mutex("SceNearCheckinLibMutex", 0);
	if (m_mutex == NULL) {
		ret = RESULT_NO_MEMORY;
	} else {
		job::JobQueue::Option option;

		option.priority = NEAR_CHECKIN_LIB_QUEUE_PRIORITY;
		option.stackSize = NEAR_CHECKIN_LIB_QUEUE_STACK_SIZE;
		option.numThreads = 1;
		m_queue = new job::JobQueue("SceNearCheckinLibQueueName", &option);
		ret = m_queue == NULL ? RESULT_NO_MEMORY : 0;
		if (ret == 0)
			return ret;
	}
	if (m_mutex != NULL) {
		delete m_mutex;
		m_mutex = NULL;
	}
	return ret;
}

NearCheckinLib::NearCheckinLib()
	: m_initialized(false), m_jobType(0), m_state(STATE_NONE), m_queue(NULL), m_mutex(NULL),
	  m_callback(NULL), m_userdata(NULL), m_error(NEAR_BG_SERVICE_ERROR_UNKNOWN)
{
	if (Init() != 0) {
		m_initialized = false;
	} else {
		m_job.reset();
		m_initialized = true;
	}
}

int NearCheckinLib::Fini(void)
{
	m_job.reset();
	if (m_queue != NULL) {
		if (m_queue->NumItems() != 0)
			m_queue->CancelAllItems();
		delete m_queue;
		m_queue = NULL;
	}
	if (m_mutex != NULL) {
		delete m_mutex;
		m_mutex = NULL;
	}
	return 0;
}

NearCheckinLib::~NearCheckinLib()
{
	Fini();
	m_initialized = false;
}

int NearCheckinLib::Start(SceInt32 type, NearCheckinLibFunction callback, void *userdata,
			  string giftDiscoveringId)
{
	NearCheckinLibConnectJob *item;
	int ret;

	switch (type) {
	case MODE_SDK_CHECKIN:
	case MODE_SDK_CHECKIN_RETRY: {
		SceAppMgrAppStatus status;

		sceClibMemset(&status, 0, sizeof(status));
		ret = sceAppMgrGetStatusByName(NULL, &status);
		if (ret != 0)
			return ret;
		giftDiscoveringId = status.appName;
		break;
	}
	case MODE_SDK_ITEM_DL:
		break;
	default:
		return RESULT_PARAM;
	}
	if (!m_initialized)
		return RESULT_STATE;
	if (m_state != STATE_NONE)
		return RESULT_BUSY;

	item = new NearCheckinLibConnectJob();

	common::SharedPtr<job::JobItem> job(item);

	if (job.get() == NULL)
		return RESULT_NO_MEMORY;
	ret = item->Setup(type, this, ConnectJobEnd, UpdateCheckinStatus, UpdateItemDownloadStatus,
			  giftDiscoveringId);
	if (ret != 0)
		return ret;
	ret = NearCheckinLibEnqueue(this, job);
	if (ret != 0)
		return ret;
	m_jobType = type;
	m_state = STATE_STARTED;
	m_job = job;
	m_callback = callback;
	m_userdata = userdata;
	return ret;
}

int NearCheckinLib::StartCheckin(NearCheckinLibFunction callback, void *userdata)
{
	return Start(MODE_SDK_CHECKIN, callback, userdata, string(""));
}

int NearCheckinLib::StartItemDownload(const string &giftDiscoveringId,
				      NearCheckinLibFunction callback, void *userdata)
{
	return Start(MODE_SDK_ITEM_DL, callback, userdata, giftDiscoveringId);
}

int NearCheckinLib::StartCheckinRetry(NearCheckinLibFunction callback, void *userdata)
{
	return Start(MODE_SDK_CHECKIN_RETRY, callback, userdata, string(""));
}

int NearCheckinLib::GetCheckinStatus(NearCheckinStatus *status)
{
	SceInt32 error;

	if (!m_initialized)
		return RESULT_STATE;
	if (status == NULL)
		return RESULT_PARAM;
	if (m_jobType != MODE_SDK_CHECKIN && m_jobType != MODE_SDK_CHECKIN_RETRY)
		return RESULT_STATE;
	if (m_state != STATE_STARTED)
		return RESULT_STATE;
	if (m_callback != NULL)
		return RESULT_STATE;
	m_mutex->Lock();
	*status = m_checkinStatus;
	error = m_error;
	m_mutex->Unlock();
	if (error != 0)
		return error;
	return 0;
}

int NearCheckinLib::GetItemDownloadStatus(NearItemDownloadStatus *status)
{
	SceInt32 error;

	if (!m_initialized)
		return RESULT_STATE;
	if (status == NULL)
		return RESULT_PARAM;
	if (m_jobType != MODE_SDK_ITEM_DL)
		return RESULT_STATE;
	if (m_state != STATE_STARTED)
		return RESULT_STATE;
	if (m_callback != NULL)
		return RESULT_STATE;
	m_mutex->Lock();
	*status = m_downloadStatus;
	error = m_error;
	m_mutex->Unlock();
	if (error != 0)
		return error;
	return 0;
}

int NearCheckinLib::Cancel(void)
{
	if (!m_initialized)
		return RESULT_STATE;
	if (m_state == STATE_NONE)
		return RESULT_NOT_STARTED;
	if (m_state != STATE_STARTED)
		return RESULT_STATE;
	if (m_callback != NULL)
		return RESULT_STATE;
	if (m_job.get() != NULL) {
		((NearCheckinLibJob *)m_job.get())->RequestAbort();
		m_job->Cancel();
	}
	return 0;
}

int NearCheckinLib::Term(NearCheckinLibFunction callback, void *userdata)
{
	if (!m_initialized)
		return RESULT_STATE;
	if (m_state == STATE_NONE)
		return RESULT_NOT_STARTED;
	if (m_state != STATE_STARTED)
		return RESULT_STATE;
	if (m_callback != NULL)
		return RESULT_STATE;
	m_state = STATE_TERMINATING;
	if (m_job.get() == NULL)
		return StartTermJob(callback, userdata);
	m_job->Cancel();
	m_callback = callback;
	m_userdata = userdata;
	return 0;
}

void NearCheckinLib::Update(void)
{
}

void NearCheckinLib::UpdateCheckinStatus(NearCheckinLib *lib, SceInt32 error,
					 const NearCheckinStatus *status)
{
	if (lib == NULL)
		return;
	lib->m_mutex->Lock();
	lib->m_error = error;
	lib->m_checkinStatus = *status;
	lib->m_mutex->Unlock();
}

void NearCheckinLib::UpdateItemDownloadStatus(NearCheckinLib *lib, SceInt32 error,
					      const NearItemDownloadStatus *status)
{
	if (lib == NULL)
		return;
	lib->m_mutex->Lock();
	lib->m_error = error;
	lib->m_downloadStatus = *status;
	lib->m_mutex->Unlock();
}

/* the service is connected to: its status is polled next */
void NearCheckinLib::ConnectJobEnd(NearCheckinLib *lib, SceInt32 result)
{
	lib->m_job.reset();
	if (result == 0)
		result = lib->StartStatusJob();
	if (lib->m_callback != NULL) {
		NearCheckinLibFunction callback = lib->m_callback;
		void *userdata = lib->m_userdata;

		lib->m_callback = NULL;
		lib->m_userdata = NULL;
		callback(result, userdata);
	}
}

/* the service's job has finished: when the library is terminating, it disconnects */
void NearCheckinLib::StatusJobEnd(NearCheckinLib *lib, SceInt32 result)
{
	lib->m_job.reset();
	if (lib->m_state == STATE_TERMINATING) {
		int ret = lib->StartTermJob(lib->m_callback, lib->m_userdata);

		if (ret != 0 && lib->m_callback != NULL) {
			NearCheckinLibFunction callback = lib->m_callback;
			void *userdata = lib->m_userdata;

			lib->m_callback = NULL;
			lib->m_userdata = NULL;
			callback(ret, userdata);
		}
	}
}

void NearCheckinLib::TermJobEnd(NearCheckinLib *lib, SceInt32 result)
{
	lib->m_job.reset();
	lib->m_state = STATE_NONE;
	if (lib->m_callback != NULL) {
		NearCheckinLibFunction callback = lib->m_callback;
		void *userdata = lib->m_userdata;

		lib->m_callback = NULL;
		lib->m_userdata = NULL;
		callback(result, userdata);
	}
}
