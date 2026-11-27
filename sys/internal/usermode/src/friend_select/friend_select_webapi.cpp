/*
 * SceFriendSelect: SceFriendSelectWebApi context, base WebAPI GET job,
 * and FriendListFetchJob ("/v1/users/%s/friendList").
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sysmodule/sysmodule.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <memory/heap_allocator.h>
#include "friend_select_internal.h"

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
using namespace paf::graph;
using namespace paf::job;
using namespace paf::memory;

static HeapAllocator *s_webApiAllocator = NULL;
static SurfacePool *s_webApiSurfacePool = NULL;
static void *s_webApiPoolMem = NULL;
static void *s_webApiReadBuf = NULL;
static int32_t s_webApiCtxId = -1;
static bool s_webApiModuleLoaded = false;

void FriendSelectWebApiCtxTerm(void)
{
	if (s_webApiReadBuf != NULL) {
		FriendSelectWebApiFree(s_webApiReadBuf);
		s_webApiReadBuf = NULL;
	}
	if (s_webApiCtxId >= 0) {
		SceNpWebApi_66CEC1A6(s_webApiCtxId);
		s_webApiCtxId = -1;
	}
	if (s_webApiPoolMem != NULL) {
		FriendSelectWebApiFree(s_webApiPoolMem);
		s_webApiPoolMem = NULL;
	}
	if (s_webApiModuleLoaded) {
		sceSysmoduleUnloadModuleInternal(
			(SceSysmoduleInternalModuleId)FRIEND_SELECT_WEBAPI_SYSMODULE_ID);
		s_webApiModuleLoaded = false;
	}
}

void FriendSelectWebApiCtxInit(void)
{
	if (sceSysmoduleLoadModuleInternal(
		    (SceSysmoduleInternalModuleId)FRIEND_SELECT_WEBAPI_SYSMODULE_ID) < 0) {
		FriendSelectWebApiCtxTerm();
		return;
	}
	s_webApiModuleLoaded = true;
	s_webApiPoolMem = FriendSelectWebApiAlloc(0x4000U);
	if (s_webApiPoolMem == NULL) {
		FriendSelectWebApiCtxTerm();
		return;
	}
	int32_t ctx = SceNpWebApi_A11FBA1A(0x4000U, s_webApiPoolMem, "SceFriendSelect");
	if (ctx < 0) {
		FriendSelectWebApiCtxTerm();
		return;
	}
	s_webApiCtxId = ctx;
	if (SceNpWebApi_C42F1E67(ctx) < 0) {
		FriendSelectWebApiCtxTerm();
		return;
	}
	s_webApiReadBuf = FriendSelectWebApiAlloc(0x4000U);
	if (s_webApiReadBuf == NULL) {
		FriendSelectWebApiCtxTerm();
		return;
	}
}

void FriendSelectWebApiInit(void)
{
	Plugin *plugin = FriendSelectGetPlugin();
	s_webApiAllocator = (plugin != NULL) ? plugin->m_private_heap : NULL;
	if (s_webApiAllocator == NULL)
		s_webApiAllocator = GetGlobalHeapAllocator();
	s_webApiSurfacePool = (plugin != NULL) ? plugin->m_surface_pool : NULL;
	FriendSelectWebApiCtxInit();
}

void FriendSelectWebApiTerm(void)
{
	FriendSelectWebApiCtxTerm();
	s_webApiSurfacePool = NULL;
	s_webApiAllocator = NULL;
}

void *FriendSelectWebApiAlloc(size_t size)
{
	return s_webApiAllocator->sce_paf_malloc(size);
}

void FriendSelectWebApiFree(void *ptr)
{
	if (ptr != NULL)
		s_webApiAllocator->sce_paf_free(ptr);
}

/* FriendSelectWebApiJob */
FriendSelectWebApiJob::FriendSelectWebApiJob(Callback1<int32_t> *callback)
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

int32_t FriendSelectWebApiJob::ExecuteGetRequest(const char *apiGroup, const char *path,
						 FriendSelectJsonReceiver *receiver)
{
	if (receiver == NULL)
		return 0;

	int32_t ret = SceNpWebApi_0D910413(s_webApiCtxId, apiGroup, path, 0, NULL, NULL,
					   &m_requestId);
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
							void *parserCtx = ::operator new(1856);
							if (parserCtx != NULL) {
								ret = receiver->InitParser(
									parserCtx);
								if (ret >= 0) {
									while (!IsCanceled()) {
										ret = sceNpWebApiReadData(
											m_requestId,
											s_webApiReadBuf,
											0x4000U);
										if (ret > 0) {
											ret = SceNpCommonPs4_F5EE219F(
												parserCtx,
												s_webApiReadBuf,
												(SceSize)ret);
											if (ret >= 0)
												continue;
										}
										break;
									}
									if (IsCanceled())
										ret = -1;
								}
								::operator delete(parserCtx);
							} else {
								ret = FRIEND_SELECT_ERROR_NOMEM;
							}
						} else {
							int32_t readBytes = sceNpWebApiReadData(
								m_requestId, s_webApiReadBuf,
								0x4000U);
							ret = readBytes;
							if (readBytes >= 0) {
								if (readBytes != 0)
									ret = sceNpWebApiGetErrorCode(
										statusCode,
										s_webApiReadBuf,
										(SceSize)readBytes);
								else
									ret = FRIEND_SELECT_ERROR_HTTP_STATUS;
							}
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

/* FriendListFetchJob */
class FriendListJsonReceiver : public FriendSelectJsonReceiver {
public:
	explicit FriendListJsonReceiver(FriendListCache *cache) : m_cache(cache) {}
	virtual ~FriendListJsonReceiver() {}

	virtual int32_t InitParser(void *parserCtx)
	{
		return m_cache->m_fetchingPage->InitJsonParser(parserCtx, 20U);
	}

	FriendListCache *m_cache;
};

FriendListFetchJob::FriendListFetchJob(FriendListCache *cache, int32_t startIndex,
				       Callback1<int32_t> *callback)
	: FriendSelectWebApiJob(callback), m_cache(cache), m_alignedOffset((startIndex / 20) * 20)
{
	if (m_cache->BeginFetch() < 0)
		m_cache = NULL;
}

int32_t FriendListFetchJob::Run()
{
	if (m_cache == NULL)
		return FRIEND_SELECT_ERROR_BUSY;

	const char *sortStr =
		(m_cache->m_sortMode != 0) ? "onlineStatus%2BonlineId" : "onlineId";
	char pathBuf[256];

	sce_paf_snprintf(
		pathBuf, sizeof(pathBuf),
		"/v1/users/%s/friendList?friendStatus=friend&fields=npId,avatarUrls,"
		"panelBgc,panelUrl,plus,trophySummary&offset=%d&limit=%d&sort=%s"
		"&avatarSizes=n&presenceType=primary",
		g_selfNpId.handle.data, m_alignedOffset, 20, sortStr);

	return ExecuteGetRequest("userProfile", pathBuf, new FriendListJsonReceiver(m_cache));
}

void FriendListFetchJob::NotifyCallback(int32_t result)
{
	if (m_cache != NULL)
		m_cache->EndFetch(result);
	FriendLibAsyncTask::NotifyCallback(result);
}
