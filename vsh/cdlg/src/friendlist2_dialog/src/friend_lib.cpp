/*
 * SceFriendList2DialogPlugin: SceFriendLib shared core implementation
 * (SceFriendLibList, SceFriendLibNp, SceFriendLibJobQueue,
 *  SceFriendLibImageManager, UpdateButtonWidget).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <rtc.h>
#include <std/list>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>
#include <common/main_thread_call_list.h>
#include <common/shared_ptr.h>
#include <framework.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <job/job.h>
#include <buffer/buffer.h>
#include <memory/heap_allocator.h>
#include <plugin.h>
#include <thread/thread.h>
#include <widget/factory.h>
#include <widget/w_plane_button.h>
#include "friend_lib.h"
#include "friend_lib_http.h"
#include "popup_list_view.h"

extern "C" {
SceInt32 sceKernelGetProcessTime(SceKernelSysClock *pSysClock);
SceInt32 sceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick);
SceInt32 sceRtcGetCurrentTick(SceRtcTick *tick);
SceInt32 SceNpManager_B24FC028(int *state);
SceInt32 SceNpManager_A775A452(void);
SceInt32 SceNpManager_E45E4003(void *npIdOut);
SceInt32 sceNpWebApiUtilityParseNpId(const char *str, SceNpId *npId);
SceInt32 SceNpCommonPs4_EB1B7375(void *parserCtx, const void *schema,
				 int (*callback)(int, int, int, uint32_t, int, void *, int,
						 void *),
				 void *userData);
paf::memory::HeapAllocator *GetGlobalHeapAllocator(void);
void ScePafMisc_E9C6A564(paf::common::SharedPtr<paf::Buffer> *out, size_t size,
			 paf::memory::HeapAllocator *allocator);
}

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::job;
using namespace paf::memory;
using namespace paf::thread;
using namespace paf::ui;

#define FRIEND_LIB_ERROR_BUSY		-2136014815	/* 0x80AE0021 */
#define FRIEND_LIB_ERROR_NP_OFFLINE	-2141902585	/* 0x80552907 */

/* Global SceFriendLib state */
static int32_t s_libInitRefCount = 0;
static int32_t s_libStartRefCount = 0;
static int32_t s_factoryRefCount = 0;
static HeapAllocator *s_libAllocator = NULL;
static SurfacePool *s_libSurfacePool = NULL;
static JobQueue *s_libJobQueue = NULL;
static char s_selfOnlineId[32];
static FriendUserProfile s_emptyProfile;

/* System sound IDs for indices 0..4 */
static const uint32_t s_friendLibSounds[5] = { 1U, 2U, 3U, 12U, 13U };

/* SceNpCommonPs4 JSON schema for PSN friendList response */
struct JsonSchemaEntry {
	int32_t nodeType;
	const char *keyName;
	int32_t hasChildren;
};

struct JsonSchemaRoot {
	const JsonSchemaEntry *const *levels;
	uint32_t levelCount;
};

static const JsonSchemaEntry s_jsonLevelRoot[] = {
	{ 1, NULL, 1 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevelTop[] = {
	{ 1, "friendList", 1 },
	{ 1, "receivedRequests", 1 },
	{ 1, "sentRequests", 1 },
	{ 1, "blockList", 1 },
	{ 1, "start", 0 },
	{ 1, "totalResults", 0 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevelArray[] = {
	{ 2, NULL, 1 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevelUser[] = {
	{ 1, "npId", 0 },
	{ 1, "avatarUrls", 1 },
	{ 1, "panelBgc", 0 },
	{ 1, "panelUrl", 0 },
	{ 1, "plus", 0 },
	{ 1, "trophySummary", 1 },
	{ 1, "relation", 0 },
	{ 1, "presence", 1 },
	{ 1, "requestMessageFlag", 0 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevelSub[] = {
	{ 2, NULL, 1 },
	{ 1, "level", 0 },
	{ 1, "primaryInfo", 1 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevelLeaf[] = {
	{ 1, "avatarUrl", 0 },
	{ 1, "onlineStatus", 0 },
	{ 1, "platform", 0 },
	{ 1, "gameTitleInfo", 1 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevelGame[] = {
	{ 1, "titleName", 0 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry *const s_jsonLevels[7] = {
	s_jsonLevelRoot,  s_jsonLevelTop,  s_jsonLevelArray, s_jsonLevelUser,
	s_jsonLevelSub,   s_jsonLevelLeaf, s_jsonLevelGame
};

static const JsonSchemaRoot s_friendListJsonSchema = { s_jsonLevels, 7U };

/* JSON scalar value from SceNpCommonPs4 parser */
struct JsonTokenValue {
	int32_t valueType;
	char payload[1];
};

static void ParseJsonBool(const JsonTokenValue *val, bool *out)
{
	*out = (val->valueType == 1);
}

static void ParseJsonInt(const JsonTokenValue *val, int32_t *out)
{
	*out = (val->valueType == 3) ? (int32_t)sce_paf_strtol(val->payload, NULL, 10) : 0;
}

static void ParseJsonHexColor(const JsonTokenValue *val, uint32_t *out)
{
	if (val->valueType == 4) {
		uint32_t raw = (uint32_t)sce_paf_strtol(val->payload, NULL, 16);

		*out = ((raw & 0xFFU) << 24) | (((raw >> 8) & 0xFFU) << 16) |
		       (((raw >> 16) & 0xFFU) << 8) | ((raw >> 24) & 0xFFU);
	} else {
		*out = 0;
	}
}

static void ParseJsonString(const JsonTokenValue *val, string *out)
{
	if (val->valueType == 4)
		*out = val->payload;
	else
		out->clear();
}

static void ParseJsonNpId(const JsonTokenValue *val, SceNpId *out)
{
	if (val->valueType != 4 || sceNpWebApiUtilityParseNpId(val->payload, out) < 0)
		sce_paf_memset(out, 0, sizeof(*out));
}

static void ParseJsonPlatform(const JsonTokenValue *val, int32_t *out)
{
	if (val->valueType == 4) {
		if (sce_paf_strncmp(val->payload, "PSVITA", 0x600U) == 0)
			*out = FRIEND_PLATFORM_PSVITA;
		else if (sce_paf_strncmp(val->payload, "PS3", 0x600U) == 0)
			*out = FRIEND_PLATFORM_PS3;
		else if (sce_paf_strncmp(val->payload, "PS4", 0x600U) == 0)
			*out = FRIEND_PLATFORM_PS4;
		else
			*out = FRIEND_PLATFORM_OTHER;
	} else {
		*out = FRIEND_PLATFORM_OTHER;
	}
}

/* FriendUserProfile */
FriendUserProfile::FriendUserProfile()
	: relation(FRIEND_RELATION_NONE), avatarUrl(), panelBgColor(0), panelUrl(),
	  onlineStatus(FRIEND_STATUS_UNKNOWN), platform(FRIEND_PLATFORM_OTHER), gameTitleName(),
	  trophyLevel(0), isPlus(false), hasRequestMessage(false)
{
	sce_paf_memset(&npId, 0, sizeof(npId));
}

FriendUserProfile::~FriendUserProfile()
{
}

bool FriendUserProfile::IsSelf() const
{
	return npId.handle.data[0] != '\0' &&
	       sce_paf_strncmp(npId.handle.data, s_selfOnlineId, 16U) == 0;
}

void FriendUserProfile::SetRelationFromString(const string &relationStr)
{
	if (sce_paf_strcmp(relationStr.c_str(), "me") == 0)
		relation = FRIEND_RELATION_SELF;
	else if (sce_paf_strcmp(relationStr.c_str(), "friend") == 0)
		relation = FRIEND_RELATION_FRIEND;
	else if (sce_paf_strcmp(relationStr.c_str(), "requesting friend") == 0)
		relation = FRIEND_RELATION_REQUESTING;
	else if (sce_paf_strcmp(relationStr.c_str(), "requested friend") == 0)
		relation = FRIEND_RELATION_REQUESTED;
	else if (sce_paf_strcmp(relationStr.c_str(), "blocked") == 0)
		relation = FRIEND_RELATION_BLOCKED;
	else
		relation = FRIEND_RELATION_OTHER;
}

void FriendUserProfile::SetTrophyLevel(int32_t level)
{
	if (level > 999)
		level = 999;
	trophyLevel = level;
}

void FriendUserProfile::SetOnlineStatusFromString(const string &statusStr)
{
	if (sce_paf_strcmp(statusStr.c_str(), "online") == 0)
		onlineStatus = FRIEND_STATUS_ONLINE;
	else if (sce_paf_strcmp(statusStr.c_str(), "offline") == 0)
		onlineStatus = FRIEND_STATUS_OFFLINE;
	else if (sce_paf_strcmp(statusStr.c_str(), "standby") == 0)
		onlineStatus = FRIEND_STATUS_STANDBY;
	else
		onlineStatus = FRIEND_STATUS_UNKNOWN;
}

/* FriendListPageCache */
FriendListPageCache::FriendListPageCache()
	: defaultRelation(FRIEND_RELATION_FRIEND), startOffset(-1), profiles(), totalResults(-1)
{
}

FriendListPageCache::~FriendListPageCache()
{
	Clear();
}

void FriendListPageCache::Clear()
{
	profiles.clear();
	startOffset = -1;
	totalResults = -1;
}

static int OnFriendListJsonToken(int parserHandle, int eventType, int depth, uint32_t fieldIndex,
				 int arrayIndex, void *valuePtr, int extra, void *userData)
{
	(void)parserHandle;
	(void)arrayIndex;
	(void)extra;
	FriendListPageCache *page = (FriendListPageCache *)userData;

	if (eventType == 1 && depth == 2) {
		FriendUserProfile entry;

		entry.relation = page->defaultRelation;
		page->profiles.push_back(entry);
		return 0;
	}
	if (eventType != 6 || valuePtr == NULL)
		return 0;

	const JsonTokenValue *val = (const JsonTokenValue *)valuePtr;

	if (depth == 1) {
		if (fieldIndex == 4U)
			ParseJsonInt(val, &page->startOffset);
		else if (fieldIndex == 5U)
			ParseJsonInt(val, &page->totalResults);
		return 0;
	}
	if (page->profiles.empty())
		return 0;

	FriendUserProfile &cur = page->profiles[page->profiles.size() - 1];

	if (depth == 3) {
		switch (fieldIndex) {
		case 0U:
			ParseJsonNpId(val, &cur.npId);
			if (cur.IsSelf())
				cur.relation = FRIEND_RELATION_SELF;
			break;
		case 2U:
			ParseJsonHexColor(val, &cur.panelBgColor);
			break;
		case 3U:
			ParseJsonString(val, &cur.panelUrl);
			break;
		case 4U: {
			int32_t plusVal = 0;

			ParseJsonInt(val, &plusVal);
			cur.isPlus = (plusVal == 1);
			break;
		}
		case 6U: {
			string relStr;

			ParseJsonString(val, &relStr);
			cur.SetRelationFromString(relStr);
			break;
		}
		case 8U:
			ParseJsonBool(val, &cur.hasRequestMessage);
			break;
		default:
			break;
		}
	} else if (depth == 4) {
		if (fieldIndex == 1U) {
			int32_t levelVal = 0;

			ParseJsonInt(val, &levelVal);
			cur.SetTrophyLevel(levelVal);
		}
	} else if (depth == 5) {
		if (fieldIndex == 0U) {
			ParseJsonString(val, &cur.avatarUrl);
		} else if (fieldIndex == 1U) {
			string stStr;

			ParseJsonString(val, &stStr);
			cur.SetOnlineStatusFromString(stStr);
		} else if (fieldIndex == 2U) {
			ParseJsonPlatform(val, &cur.platform);
		}
	} else if (depth == 6) {
		if (fieldIndex == 0U) {
			ParseJsonString(val, &cur.gameTitleName);
			FriendLibNormalizeNewlines(cur.gameTitleName);
		}
	}
	return 0;
}

int FriendListPageCache::InitJsonParser(void *parserCtx, uint32_t reserveCount)
{
	int ret = SceNpCommonPs4_EB1B7375(parserCtx, &s_friendListJsonSchema,
					  OnFriendListJsonToken, this);

	if (ret >= 0) {
		Clear();
		if (profiles.capacity() < reserveCount)
			profiles.reserve(reserveCount);
	}
	return ret;
}

/* FriendListCache */
FriendListCache::FriendListCache()
	: m_mutex("SceFriendLibList", 0), m_pages(), m_fetchingPage(NULL), m_totalResults(-1),
	  m_sortByStatus(0), m_invalidated(false)
{
	for (int i = 0; i < 3; ++i)
		m_pages.push_back(new FriendListPageCache());
}

FriendListCache::~FriendListCache()
{
	for (list<FriendListPageCache *>::iterator it = m_pages.begin(); it != m_pages.end(); ++it)
		delete *it;
	m_pages.clear();
	if (m_fetchingPage != NULL) {
		delete m_fetchingPage;
		m_fetchingPage = NULL;
	}
}

void FriendListCache::ClearAll()
{
	m_mutex.Lock();
	for (list<FriendListPageCache *>::iterator it = m_pages.begin(); it != m_pages.end(); ++it)
		(*it)->Clear();
	m_totalResults = -1;
	m_mutex.Unlock();
}

void FriendListCache::SetSortByStatus(bool sortByStatus)
{
	m_mutex.Lock();
	m_sortByStatus = sortByStatus ? 1 : 0;
	ClearAll();
	if (m_fetchingPage != NULL)
		m_invalidated = true;
	m_mutex.Unlock();
}

int FriendListCache::BeginFetch()
{
	m_mutex.Lock();
	if (m_fetchingPage != NULL) {
		m_mutex.Unlock();
		return FRIEND_LIB_ERROR_BUSY;
	}
	m_fetchingPage = m_pages.back();
	m_pages.pop_back();
	m_mutex.Unlock();
	return 0;
}

void FriendListCache::EndFetch(int result)
{
	m_mutex.Lock();
	if (m_fetchingPage == NULL) {
		m_mutex.Unlock();
		return;
	}
	if (result < 0 || m_invalidated) {
		m_fetchingPage->Clear();
		m_pages.push_back(m_fetchingPage);
	} else {
		m_totalResults = m_fetchingPage->totalResults;
		m_pages.push_front(m_fetchingPage);
	}
	m_fetchingPage = NULL;
	m_invalidated = false;
	m_mutex.Unlock();
}

const FriendUserProfile *FriendListCache::GetProfileAt(int32_t index)
{
	m_mutex.Lock();
	if (m_totalResults < 0) {
		m_mutex.Unlock();
		return &s_emptyProfile;
	}
	for (list<FriendListPageCache *>::iterator it = m_pages.begin(); it != m_pages.end(); ++it) {
		FriendListPageCache *page = *it;

		if (page->startOffset >= 0 && index >= page->startOffset &&
		    index < page->startOffset + (int32_t)page->profiles.size()) {
			const FriendUserProfile *found =
				&page->profiles[(size_t)(index - page->startOffset)];
			m_pages.erase(it);
			m_pages.push_front(page);
			m_mutex.Unlock();
			return found;
		}
	}
	m_mutex.Unlock();
	return &s_emptyProfile;
}

/* FriendLibNpJob */
FriendLibNpJob::FriendLibNpJob(SceNpId *outNpId, FriendLibTaskCallback *callback)
	: FriendLibQueuedTask(FriendLibGetJobQueue(), callback, "SceFriendLibNp"), m_outNpId(outNpId)
{
}

FriendLibNpJob::~FriendLibNpJob()
{
}

int FriendLibNpJob::Run()
{
	int npState = 0;

	if (SceNpManager_B24FC028(&npState) < 0 || npState == 0)
		return FRIEND_LIB_ERROR_NP_OFFLINE;
	if (SceNpManager_A775A452() <= 0)
		return FRIEND_LIB_ERROR_NP_OFFLINE;

	uint8_t profileBuf[328];
	int ret = SceNpManager_E45E4003(profileBuf);

	if (ret >= 0 && m_outNpId != NULL)
		sce_paf_memcpy(m_outNpId, profileBuf, sizeof(SceNpId));
	return ret;
}

/* Clock jump watcher */
static list<FriendLibClockListener *> s_clockListeners;
static list<FriendLibClockListener *>::iterator s_clockIter;
static bool s_clockWatcherActive = false;
static bool s_clockIterAdvanced = false;
static uint64_t s_lastProcessClock = 0;
static SceRtcTick s_lastNetworkTick = { 0 };
static RMutex s_clockMutex("SceFriendLibClockMutex", 0);

static void ClockWatcherTick(void *userData)
{
	(void)userData;
	SceKernelSysClock procClock;
	SceRtcTick netTick = { 0 };

	procClock.quad = 0;
	if (sceKernelGetProcessTime(&procClock) < 0)
		return;
	if (sceRtcGetCurrentRetainedNetworkTick(&netTick) < 0 &&
	    sceRtcGetCurrentTick(&netTick) < 0)
		return;

	if (s_lastProcessClock != 0 && s_lastNetworkTick.tick != 0) {
		uint64_t procDelta = procClock.quad - s_lastProcessClock;
		uint64_t netDelta = netTick.tick - s_lastNetworkTick.tick;
		uint64_t diff = (procDelta > netDelta) ? (procDelta - netDelta)
						       : (netDelta - procDelta);

		if (diff > 100000ULL) {
			s_clockIter = s_clockListeners.begin();
			while (s_clockIter != s_clockListeners.end()) {
				(*s_clockIter)->OnClockChanged();
				if (s_clockIterAdvanced)
					s_clockIterAdvanced = false;
				else
					++s_clockIter;
			}
		}
	}
	s_lastProcessClock = procClock.quad;
	s_lastNetworkTick = netTick;
}

static void ClockWatcherInit(void)
{
	s_clockWatcherActive = false;
	s_clockIterAdvanced = false;
	s_lastProcessClock = 0;
	s_lastNetworkTick.tick = 0;
}

static void ClockWatcherTerm(void)
{
	if (s_clockWatcherActive) {
		s_clockMutex.Lock();
		MainThreadCallList::Unregister(ClockWatcherTick, NULL);
		s_clockWatcherActive = false;
		s_clockMutex.Unlock();
	}
	while (s_clockListeners.size() != 0) {
		FriendLibClockListener *listener = s_clockListeners.front();

		s_clockListeners.pop_front();
		delete listener;
	}
}

void FriendLibAddClockListener(FriendLibClockListener *listener)
{
	s_clockListeners.push_back(listener);
	if (!s_clockWatcherActive) {
		MainThreadCallList::Register(ClockWatcherTick, NULL);
		s_clockWatcherActive = true;
	}
}

void FriendLibRemoveClockListener(void *owner)
{
	for (list<FriendLibClockListener *>::iterator it = s_clockListeners.begin();
	     it != s_clockListeners.end(); ++it) {
		if ((*it)->MatchesOwner(owner)) {
			FriendLibClockListener *removed = *it;

			if (it == s_clockIter) {
				s_clockIter = s_clockListeners.erase(it);
				s_clockIterAdvanced = true;
			} else {
				s_clockListeners.erase(it);
			}
			delete removed;
			return;
		}
	}
}

void FriendLibPlaySystemSound(uint32_t soundIndex)
{
	if (soundIndex <= 4U) {
		Framework *fw = Framework::Instance();

		if (fw != NULL)
			fw->PlayCommonSE((int32_t)s_friendLibSounds[soundIndex], 0.0f, false);
	}
}

void FriendLibNormalizeNewlines(string &text)
{
	size_t len = text.size();
	char *clean = new char[len + 1U];

	if (clean == NULL)
		return;
	size_t outPos = 0;

	for (size_t i = 0; i < len; ++i) {
		char ch = text[i];

		if (ch == '\r') {
			clean[outPos++] = ' ';
			if (i + 1U < len && text[i + 1U] == '\n')
				++i;
		} else if (ch == '\n') {
			clean[outPos++] = ' ';
		} else {
			clean[outPos++] = ch;
		}
	}
	clean[outPos] = '\0';
	text.assign(clean, outPos);
	delete[] clean;
}

/* SceFriendLibImageManager */
struct ImageCacheEntry {
	ImageCacheEntry() : url(), surface(), waiters() {}

	string url;
	intrusive_ptr<Surface> surface;
	list<FriendLibImageRequestCallback *> waiters;
};

class FriendLibImageManager {
public:
	explicit FriendLibImageManager(SurfacePool *surfacePool)
		: m_surfacePool(surfacePool), m_jobQueue(NULL), m_loader(NULL),
		  m_mutex("SceFriendLibImageManager", 0)
	{
		JobQueue::Option opt;

		opt.priority = 0x10000101;
		m_jobQueue = new JobQueue("SceFriendLibImageManager", &opt);
		m_loader = new FriendLibImageLoader(m_jobQueue);
		m_loader->m_maxHttpConnections = 1;
		m_loader->m_maxHttpsConnections = 1;
	}

	virtual ~FriendLibImageManager()
	{
		ClearCache();
		if (m_loader != NULL) {
			m_loader->CancelAll();
			delete m_loader;
			m_loader = NULL;
		}
		if (m_jobQueue != NULL) {
			if (m_jobQueue->NumItems() != 0) {
				m_jobQueue->CancelAllItems();
				m_jobQueue->WaitEmpty();
			}
			delete m_jobQueue;
			m_jobQueue = NULL;
		}
	}

	void NotifyEntry(ImageCacheEntry *entry, int result)
	{
		m_mutex.Lock();
		while (entry->waiters.size() != 0) {
			FriendLibImageRequestCallback *cb = entry->waiters.front();

			entry->waiters.pop_front();
			if (cb != NULL) {
				if (result != -1)
					cb->OnImageReady(entry->url, result, &entry->surface);
				delete cb;
			}
		}
		m_mutex.Unlock();
	}

	class LoadCallback : public FriendLibImageCallback {
	public:
		LoadCallback(FriendLibImageManager *mgr, ImageCacheEntry *entry)
			: m_mgr(mgr), m_entry(entry)
		{
		}

		virtual ~LoadCallback() {}

		virtual void OnComplete(int result, intrusive_ptr<Surface> surface)
		{
			m_mgr->OnImageLoaded(m_entry, result, surface);
		}

		FriendLibImageManager *m_mgr;
		ImageCacheEntry *m_entry;
	};

	void OnImageLoaded(ImageCacheEntry *entry, int result, intrusive_ptr<Surface> surface)
	{
		m_mutex.Lock();
		for (list<ImageCacheEntry *>::iterator it = m_pending.begin();
		     it != m_pending.end(); ++it) {
			if (*it == entry) {
				m_pending.erase(it);
				break;
			}
		}
		if (result >= 0 && surface.get() != NULL) {
			entry->surface = surface;
			m_cached.push_front(entry);
			while (m_cached.size() > 64U) {
				ImageCacheEntry *evict = m_cached.back();

				m_cached.pop_back();
				delete evict;
			}
			NotifyEntry(entry, result);
		} else {
			NotifyEntry(entry, result);
			delete entry;
		}
		m_mutex.Unlock();
	}

	void Request(const char *url, FriendLibImageRequestCallback *callback, int32_t maxWidth,
		     int32_t maxHeight)
	{
		if (url == NULL || url[0] == '\0') {
			delete callback;
			return;
		}
		m_mutex.Lock();
		for (list<ImageCacheEntry *>::iterator it = m_cached.begin();
		     it != m_cached.end(); ++it) {
			if (sce_paf_strcmp((*it)->url.c_str(), url) == 0) {
				ImageCacheEntry *hit = *it;

				m_cached.erase(it);
				m_cached.push_front(hit);
				if (callback != NULL) {
					callback->OnImageReady(hit->url, 0, &hit->surface);
					delete callback;
				}
				m_mutex.Unlock();
				return;
			}
		}
		for (list<ImageCacheEntry *>::iterator it = m_pending.begin();
		     it != m_pending.end(); ++it) {
			if (sce_paf_strcmp((*it)->url.c_str(), url) == 0) {
				if (callback != NULL)
					(*it)->waiters.push_back(callback);
				m_mutex.Unlock();
				return;
			}
		}

		ImageCacheEntry *entry = new ImageCacheEntry();

		entry->url = url;
		if (callback != NULL)
			entry->waiters.push_back(callback);
		m_pending.push_back(entry);

		FriendLibImageOption opt;

		opt.maxWidth = maxWidth;
		opt.maxHeight = maxHeight;
		opt.surfacePool = m_surfacePool;
		opt.decodeHook = NULL;
		m_loader->LoadAsync(url, new LoadCallback(this, entry), opt);
		m_mutex.Unlock();
	}

	void CancelUrl(const char *url)
	{
		if (url == NULL)
			return;
		m_mutex.Lock();
		for (list<ImageCacheEntry *>::iterator it = m_pending.begin();
		     it != m_pending.end(); ++it) {
			if (sce_paf_strcmp((*it)->url.c_str(), url) == 0) {
				NotifyEntry(*it, -1);
				break;
			}
		}
		m_mutex.Unlock();
	}

	void ClearCache()
	{
		m_mutex.Lock();
		while (m_pending.size() != 0) {
			ImageCacheEntry *entry = m_pending.front();

			m_pending.pop_front();
			NotifyEntry(entry, -1);
			delete entry;
		}
		while (m_cached.size() != 0) {
			ImageCacheEntry *entry = m_cached.front();

			m_cached.pop_front();
			delete entry;
		}
		m_mutex.Unlock();
	}

	SurfacePool *m_surfacePool;
	JobQueue *m_jobQueue;
	FriendLibImageLoader *m_loader;
	list<ImageCacheEntry *> m_cached;
	list<ImageCacheEntry *> m_pending;
	RMutex m_mutex;
};

static FriendLibImageManager *s_imageManager = NULL;

void FriendLibRequestImage(const char *url, FriendLibImageRequestCallback *callback,
			   int32_t maxWidth, int32_t maxHeight)
{
	if (s_imageManager != NULL)
		s_imageManager->Request(url, callback, maxWidth, maxHeight);
	else
		delete callback;
}

void FriendLibCancelImageRequests(const char *url)
{
	if (s_imageManager != NULL)
		s_imageManager->CancelUrl(url);
}

void FriendLibClearImageCache(void)
{
	if (s_imageManager != NULL)
		s_imageManager->ClearCache();
}

/* UpdateButtonWidget ("update_button") */
const char *UpdateButtonWidget::TypeName()
{
	return "update_button";
}

UpdateButtonWidget::UpdateButtonWidget(Widget *parent, Widget::CreateParam *param)
	: PlaneButton(parent, param), m_spinning(false)
{
}

UpdateButtonWidget::~UpdateButtonWidget()
{
	StopSpin();
}

bool UpdateButtonWidget::IsInherit(const char *name) const
{
	if (name == TypeName())
		return true;
	return PlaneButton::IsInherit(name);
}

char *UpdateButtonWidget::GetTypeName() const
{
	return (char *)TypeName();
}

bool UpdateButtonWidget::StartSpin()
{
	if (m_spinning)
		return false;
	m_spinning = true;
	return true;
}

void UpdateButtonWidget::StopSpin()
{
	if (!m_spinning)
		return;
	m_spinning = false;
}

static Widget *CreateUpdateButton(Widget *parent)
{
	return new UpdateButtonWidget(parent, NULL);
}

/* Global SceFriendLib lifecycle */
SharedPtr<Buffer> FriendLibAllocBuffer(size_t size)
{
	SharedPtr<Buffer> out;

	ScePafMisc_E9C6A564(&out, size, s_libAllocator);
	return out;
}

JobQueue *FriendLibGetJobQueue(void)
{
	return s_libJobQueue;
}

SurfacePool *FriendLibGetSurfacePool(void)
{
	return s_libSurfacePool;
}

void FriendLibSetSelfOnlineId(const char *onlineId)
{
	if (onlineId != NULL) {
		sce_paf_strncpy(s_selfOnlineId, onlineId, sizeof(s_selfOnlineId) - 1);
		s_selfOnlineId[sizeof(s_selfOnlineId) - 1] = '\0';
	} else {
		s_selfOnlineId[0] = '\0';
	}
}

const char *FriendLibGetSelfOnlineId(void)
{
	return s_selfOnlineId;
}

void FriendLibInit(HeapAllocator *allocator, SurfacePool *surfacePool)
{
	if (s_libInitRefCount++ == 0) {
		s_libAllocator = (allocator != NULL) ? allocator : GetGlobalHeapAllocator();
		s_libSurfacePool = surfacePool;
		s_libJobQueue = new JobQueue("SceFriendLibJobQueue", NULL);
		FriendLibHttpInit();
		ClockWatcherInit();
		s_imageManager = new FriendLibImageManager(surfacePool);
	}
}

void FriendLibTerm(void)
{
	if (--s_libInitRefCount == 0) {
		if (s_imageManager != NULL) {
			delete s_imageManager;
			s_imageManager = NULL;
		}
		ClockWatcherTerm();
		FriendLibHttpTerm();
		if (s_libJobQueue != NULL) {
			if (s_libJobQueue->NumItems() != 0) {
				s_libJobQueue->CancelAllItems();
				s_libJobQueue->WaitEmpty();
			}
			delete s_libJobQueue;
			s_libJobQueue = NULL;
		}
		s_libSurfacePool = NULL;
		s_libAllocator = NULL;
	}
	if (s_libInitRefCount < 0)
		s_libInitRefCount = 0;
}

void FriendLibStart(void)
{
	if (s_libStartRefCount++ == 0)
		PopupListInit();
}

void FriendLibStop(void)
{
	if (--s_libStartRefCount == 0)
		PopupListClose();
	if (s_libStartRefCount < 0)
		s_libStartRefCount = 0;
}

void FriendLibRegisterWidgetFactory(void)
{
	if (s_factoryRefCount++ == 0) {
		FactoryFuncTable tbl;

		sce_paf_memset(&tbl, 0, sizeof(tbl));
		tbl.m_name = UpdateButtonWidget::TypeName();
		tbl.m_factory_widget = CreateUpdateButton;
		Plugin::RegistFactoryTable(UpdateButtonWidget::TypeName(), tbl);
	}
}

void FriendLibUnregisterWidgetFactory(void)
{
	if (--s_factoryRefCount == 0)
		Plugin::RemoveFactoryTable(UpdateButtonWidget::TypeName());
	if (s_factoryRefCount < 0)
		s_factoryRefCount = 0;
}
