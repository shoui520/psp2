/*
 * SceFriendList2DialogPlugin: SceFriendSelectWebApi context and base job.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sysmodule/sysmodule.h>
#include <std/stdlib.h>
#include <memory/heap_allocator.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include "friend_select_webapi.h"
#include "friend_lib.h"

extern "C" {
int SceNpWebApi_A11FBA1A(SceSize poolSize, void *poolMem, const char *name);
int SceNpWebApi_C42F1E67(int libCtxId);
int SceNpWebApi_66CEC1A6(int libCtxId);
int SceNpWebApi_0D910413(int userCtxId, const char *apiGroup, const char *path, int method,
			 const void *reqParam, const void *extraHeader, int64_t *requestId);
int sceNpWebApiSendRequest(int64_t requestId, const void *data, SceSize dataSize);
int sceNpWebApiGetHttpStatusCode(int64_t requestId, int *statusCode);
int sceNpWebApiReadData(int64_t requestId, void *data, SceSize size);
int sceNpWebApiGetErrorCode(int statusCode, const void *body, SceSize bodySize);
int sceNpWebApiDeleteRequest(int64_t requestId);
int sceNpWebApiAbortRequest(int64_t requestId);
int SceNpCommonPs4_F5EE219F(void *parserCtx, const void *chunk, SceSize size);
paf::memory::HeapAllocator *GetGlobalHeapAllocator(void);
}

using namespace paf;
using namespace paf::common;
using namespace paf::job;
using namespace paf::memory;

#define FRIEND_SELECT_WEBAPI_SYSMODULE_ID	0x53U
#define FRIEND_SELECT_WEBAPI_ERROR_BUSY		-2136014815	/* 0x80AE0021 */
#define FRIEND_SELECT_WEBAPI_ERROR_NOMEM	-2136014814	/* 0x80AE0022 */
#define FRIEND_SELECT_WEBAPI_ERROR_HTTP		-2135994284	/* 0x80AE5054 */

static HeapAllocator *s_selectHeap = NULL;
static void *s_webApiPool = NULL;
static void *s_webApiReadBuf = NULL;
static int s_webApiCtxId = -1;
static bool s_webApiSysmoduleLoaded = false;
static FriendLibAsyncTask *s_activeTask = NULL;

void *FriendSelectAlloc(size_t size)
{
	if (s_selectHeap == NULL)
		s_selectHeap = GetGlobalHeapAllocator();
	return s_selectHeap->sce_paf_malloc(size);
}

void FriendSelectFree(void *ptr)
{
	if (ptr != NULL && s_selectHeap != NULL)
		s_selectHeap->sce_paf_free(ptr);
}

void FriendSelectWebApiTerm(void)
{
	if (s_webApiReadBuf != NULL) {
		FriendSelectFree(s_webApiReadBuf);
		s_webApiReadBuf = NULL;
	}
	if (s_webApiCtxId >= 0) {
		SceNpWebApi_66CEC1A6(s_webApiCtxId);
		s_webApiCtxId = -1;
	}
	if (s_webApiPool != NULL) {
		FriendSelectFree(s_webApiPool);
		s_webApiPool = NULL;
	}
	if (s_webApiSysmoduleLoaded) {
		sceSysmoduleUnloadModuleInternal(
			(SceSysmoduleInternalModuleId)FRIEND_SELECT_WEBAPI_SYSMODULE_ID);
		s_webApiSysmoduleLoaded = false;
	}
}

int FriendSelectWebApiInit(void)
{
	if (sceSysmoduleLoadModuleInternal(
		    (SceSysmoduleInternalModuleId)FRIEND_SELECT_WEBAPI_SYSMODULE_ID) < 0) {
		FriendSelectWebApiTerm();
		return -1;
	}
	s_webApiSysmoduleLoaded = true;
	s_webApiPool = FriendSelectAlloc(0x4000U);
	if (s_webApiPool == NULL) {
		FriendSelectWebApiTerm();
		return FRIEND_SELECT_WEBAPI_ERROR_NOMEM;
	}
	s_webApiCtxId = SceNpWebApi_A11FBA1A(0x4000U, s_webApiPool, "SceFriendSelect");
	if (s_webApiCtxId < 0) {
		FriendSelectWebApiTerm();
		return s_webApiCtxId;
	}
	int ret = SceNpWebApi_C42F1E67(s_webApiCtxId);

	if (ret < 0) {
		FriendSelectWebApiTerm();
		return ret;
	}
	s_webApiReadBuf = FriendSelectAlloc(0x4000U);
	if (s_webApiReadBuf == NULL) {
		FriendSelectWebApiTerm();
		return FRIEND_SELECT_WEBAPI_ERROR_NOMEM;
	}
	return 0;
}

/* Single-active-task scheduler */
bool FriendLibIsTaskRunning(void)
{
	return s_activeTask != NULL;
}

void FriendLibCancelActiveTask(void)
{
	if (s_activeTask != NULL)
		s_activeTask->Cancel();
}

void FriendLibStartTask(FriendLibAsyncTask *task)
{
	if (task == NULL)
		return;
	if (s_activeTask != NULL) {
		task->NotifyCallback(FRIEND_SELECT_WEBAPI_ERROR_BUSY);
		delete task;
		return;
	}
	s_activeTask = task;
	task->Start();
}

/* FriendLibAsyncTask */
FriendLibAsyncTask::FriendLibAsyncTask(FriendLibTaskCallback *callback)
	: m_callback(callback)
{
}

FriendLibAsyncTask::~FriendLibAsyncTask()
{
	if (m_callback != NULL) {
		delete m_callback;
		m_callback = NULL;
	}
}

void FriendLibAsyncTask::OnJobComplete(int result)
{
	FriendLibAsyncTask *finished = s_activeTask;

	s_activeTask = NULL;
	NotifyCallback(result);
	if (finished != NULL)
		delete finished;
}

void FriendLibAsyncTask::NotifyCallback(int result)
{
	if (m_callback != NULL) {
		FriendLibTaskCallback *cb = m_callback;

		m_callback = NULL;
		cb->OnFinished(result);
		delete cb;
	}
}

/* FriendLibQueuedTask */
FriendLibQueuedTask::FriendLibQueuedTask(JobQueue *queue, FriendLibTaskCallback *callback,
					 const char *jobName)
	: FriendLibAsyncTask(callback), JobItem(jobName, NULL),
	  m_queue(queue != NULL ? queue : FriendLibGetJobQueue())
{
}

FriendLibQueuedTask::~FriendLibQueuedTask()
{
}

void FriendLibQueuedTask::Start()
{
	SharedPtr<JobItem> item(static_cast<JobItem *>(this));

	m_queue->Enqueue(item);
}

void FriendLibQueuedTask::Cancel()
{
	JobItem::Cancel();
}

void FriendLibQueuedTask::OnJobComplete(int result)
{
	s_activeTask = NULL;
	NotifyCallback(result);
}

void FriendLibQueuedTask::Finish(int result)
{
	if (IsCanceled())
		result = -1;
	OnJobComplete(result);
}

/* FriendSelectWebApiJob */
FriendSelectWebApiJob::FriendSelectWebApiJob(FriendLibTaskCallback *callback)
	: FriendLibQueuedTask(NULL, callback, "SceFriendSelectWebApi"), m_requestId(-1)
{
}

FriendSelectWebApiJob::~FriendSelectWebApiJob()
{
}

void FriendSelectWebApiJob::Cancel()
{
	JobItem::Cancel();
	if (m_requestId >= 0)
		sceNpWebApiAbortRequest(m_requestId);
}

int FriendSelectWebApiJob::ExecuteGetRequest(const char *apiGroup, const char *path,
					     FriendSelectJsonReceiver *receiver)
{
	if (receiver == NULL)
		return 0;

	int ret = SceNpWebApi_0D910413(s_webApiCtxId, apiGroup, path, 0, NULL, NULL, &m_requestId);

	if (ret >= 0) {
		if (IsCanceled()) {
			ret = -1;
		} else {
			ret = sceNpWebApiSendRequest(m_requestId, NULL, 0);
			if (ret >= 0) {
				if (IsCanceled()) {
					ret = -1;
				} else {
					int statusCode = 0;

					ret = sceNpWebApiGetHttpStatusCode(m_requestId,
									   &statusCode);
					if (ret >= 0) {
						if (statusCode == 200) {
							uint8_t *parserCtx = new uint8_t[1856];

							if (parserCtx == NULL) {
								ret = FRIEND_SELECT_WEBAPI_ERROR_NOMEM;
							} else {
								ret = receiver->InitParser(
									parserCtx);
								if (ret >= 0) {
									for (;;) {
										if (IsCanceled()) {
											ret = -1;
											break;
										}
										int readBytes =
											sceNpWebApiReadData(
												m_requestId,
												s_webApiReadBuf,
												0x4000U);
										if (readBytes <= 0) {
											ret = readBytes;
											break;
										}
										ret = SceNpCommonPs4_F5EE219F(
											parserCtx,
											s_webApiReadBuf,
											(SceSize)readBytes);
										if (ret < 0)
											break;
									}
								}
								delete[] parserCtx;
							}
						} else {
							int readBytes = sceNpWebApiReadData(
								m_requestId, s_webApiReadBuf,
								0x4000U);
							if (readBytes > 0)
								ret = sceNpWebApiGetErrorCode(
									statusCode, s_webApiReadBuf,
									(SceSize)readBytes);
							else if (readBytes == 0)
								ret = FRIEND_SELECT_WEBAPI_ERROR_HTTP;
							else
								ret = readBytes;
						}
					}
				}
			}
		}
	}

	if (m_requestId >= 0) {
		sceNpWebApiDeleteRequest(m_requestId);
		m_requestId = -1;
	}
	delete receiver;
	return ret;
}
