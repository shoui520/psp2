/*
 * SceFriendLib: FriendUserProfile, FriendListPageCache, FriendListCache
 * ("SceFriendLibList"), JSON scalar parse helpers, and global SceFriendLib
 * ref-counted lifecycle hooks.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <plugin.h>
#include <widget/factory.h>
#include "friend_select_internal.h"

extern "C" {
SceInt32 sceNpWebApiUtilityParseNpId(const char *str, SceNpId *npId);
SceInt32 SceNpCommonPs4_EB1B7375(void *parserCtx, const void *schema,
				 int (*callback)(int, int, int, uint32_t, int, void *, int,
						 void *),
				 void *userData);
}

using namespace paf;
using namespace paf::graph;
using namespace paf::memory;
using namespace paf::thread;
using namespace paf::ui;

SceNpId g_selfNpId;

static FriendUserProfile s_emptyProfile;
static int32_t s_libInitRefCount = 0;
static int32_t s_libStartRefCount = 0;
static int32_t s_factoryRefCount = 0;

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

static const JsonSchemaEntry s_jsonLevel0[] = {
	{ 1, "", 1 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevel1[] = {
	{ 1, "friendList", 1 },
	{ 1, "receivedRequests", 1 },
	{ 1, "sentRequests", 1 },
	{ 1, "blockList", 1 },
	{ 1, "start", 0 },
	{ 1, "totalResults", 0 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevel2[] = {
	{ 2, "", 1 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevel3[] = {
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

static const JsonSchemaEntry s_jsonLevel4[] = {
	{ 2, "", 1 },
	{ 1, "level", 0 },
	{ 1, "primaryInfo", 1 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevel5[] = {
	{ 1, "avatarUrl", 0 },
	{ 1, "onlineStatus", 0 },
	{ 1, "platform", 0 },
	{ 1, "gameTitleInfo", 1 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry s_jsonLevel6[] = {
	{ 1, "titleName", 0 },
	{ 0, "", 0 }
};

static const JsonSchemaEntry *const s_jsonLevels[7] = {
	s_jsonLevel0, s_jsonLevel1, s_jsonLevel2, s_jsonLevel3,
	s_jsonLevel4, s_jsonLevel5, s_jsonLevel6
};

static const JsonSchemaRoot s_friendListSchema = { s_jsonLevels, 7U };

/* JSON scalar value from SceNpCommonPs4 parser */
struct JsonTokenValue {
	int32_t valueType;
	char payload[1];
};

bool ParseJsonBool(const JsonTokenValue *val, bool *out)
{
	if (val->valueType == 1) {
		*out = true;
		return true;
	}
	*out = false;
	return false;
}

int32_t ParseJsonInt(const JsonTokenValue *val, int32_t *out)
{
	int32_t result = 0;
	if (val->valueType == 3)
		result = (int32_t)sce_paf_strtol(val->payload, NULL, 10);
	*out = result;
	return result;
}

void ParseJsonHexColor(const JsonTokenValue *val, uint32_t *out)
{
	uint32_t color = 0;
	if (val->valueType == 4) {
		uint32_t raw = (uint32_t)sce_paf_strtol(val->payload, NULL, 16);
		color = ((raw & 0xFFU) << 24) | (((raw >> 8) & 0xFFU) << 16) |
			(((raw >> 16) & 0xFFU) << 8) | ((raw >> 24) & 0xFFU);
	}
	*out = color;
}

void ParseJsonString(const JsonTokenValue *val, string *out)
{
	if (val->valueType == 4)
		out->assign(val->payload);
	else
		out->clear();
}

void ParseJsonNpId(const JsonTokenValue *val, SceNpId *out)
{
	if (val->valueType != 4 || sceNpWebApiUtilityParseNpId(val->payload, out) < 0)
		sce_paf_memset(out, 0, sizeof(*out));
}

int32_t ParseJsonPlatform(const JsonTokenValue *val, int32_t *out)
{
	if (val->valueType == 4) {
		if (sce_paf_strncmp(val->payload, "PSVITA", 0x600U) == 0) {
			*out = FRIEND_PLATFORM_PSVITA;
			return FRIEND_PLATFORM_PSVITA;
		}
		if (sce_paf_strncmp(val->payload, "PS3", 0x600U) == 0) {
			*out = FRIEND_PLATFORM_PS3;
			return FRIEND_PLATFORM_PS3;
		}
		if (sce_paf_strncmp(val->payload, "PS4", 0x600U) == 0) {
			*out = FRIEND_PLATFORM_PS4;
			return FRIEND_PLATFORM_PS4;
		}
	}
	*out = FRIEND_PLATFORM_OTHER;
	return FRIEND_PLATFORM_OTHER;
}

/* FriendUserProfile */
FriendUserProfile::FriendUserProfile()
	: m_relation(FRIEND_RELATION_NONE), m_avatarUrl(),
	  m_panelBgColor(FRIEND_SELECT_DEFAULT_PANEL_COLOR), m_panelUrl(),
	  m_onlineStatus(FRIEND_STATUS_UNKNOWN), m_platform(FRIEND_PLATFORM_OTHER),
	  m_gameTitleName(), m_trophyLevel(-1), m_isPlus(false), m_hasRequestMessage(false)
{
	sce_paf_memset(&m_npId, 0, sizeof(m_npId));
}

FriendUserProfile::~FriendUserProfile()
{
}

void FriendUserProfile::Reset()
{
	m_relation = FRIEND_RELATION_NONE;
	m_avatarUrl.clear();
	m_panelBgColor = FRIEND_SELECT_DEFAULT_PANEL_COLOR;
	m_panelUrl.clear();
	m_onlineStatus = FRIEND_STATUS_UNKNOWN;
	m_platform = FRIEND_PLATFORM_OTHER;
	m_gameTitleName.clear();
	m_isPlus = false;
	m_hasRequestMessage = false;
	m_trophyLevel = -1;
}

bool FriendUserProfile::MatchesNpId(const SceNpId &other) const
{
	return m_npId.handle.data[0] != '\0' &&
	       sce_paf_strncmp(m_npId.handle.data, other.handle.data, 16U) == 0;
}

bool FriendUserProfile::IsSelf() const
{
	return m_npId.handle.data[0] != '\0' &&
	       sce_paf_strncmp(m_npId.handle.data, g_selfNpId.handle.data, 16U) == 0;
}

void FriendUserProfile::SetRelationFromString(const string &relationStr)
{
	if (relationStr == string("me"))
		m_relation = FRIEND_RELATION_SELF;
	else if (relationStr == string("friend"))
		m_relation = FRIEND_RELATION_FRIEND;
	else if (relationStr == string("requesting friend"))
		m_relation = FRIEND_RELATION_REQUESTING;
	else if (relationStr == string("requested friend"))
		m_relation = FRIEND_RELATION_REQUESTED;
	else if (relationStr == string("blocked"))
		m_relation = FRIEND_RELATION_BLOCKED;
	else
		m_relation = FRIEND_RELATION_OTHER;
}

void FriendUserProfile::SetTrophyLevel(int32_t level)
{
	if (level > 999)
		level = 999;
	m_trophyLevel = level;
}

void FriendUserProfile::SetOnlineStatusFromString(const string &statusStr)
{
	if (statusStr == string("online"))
		m_onlineStatus = FRIEND_STATUS_ONLINE;
	else if (statusStr == string("offline"))
		m_onlineStatus = FRIEND_STATUS_OFFLINE;
	else if (statusStr == string("standby"))
		m_onlineStatus = FRIEND_STATUS_STANDBY;
	else
		m_onlineStatus = FRIEND_STATUS_UNKNOWN;
}

/* FriendListPageCache */
FriendListPageCache::FriendListPageCache()
	: m_defaultRelation(FRIEND_RELATION_FRIEND), m_startOffset(-1), m_profiles(),
	  m_totalResults(-1)
{
}

FriendListPageCache::~FriendListPageCache()
{
}

namespace paf {
template <>
__attribute__((noinline)) int
vector<FriendUserProfile, allocator<FriendUserProfile> >::push_back(const FriendUserProfile &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) FriendUserProfile(val);
		result = ++count;
	}
	return result;
}
}

int32_t FriendListPageCache::HandleJsonEvent(int32_t parserHandle, int32_t eventType,
					     int32_t depth, uint32_t fieldIndex,
					     int32_t arrayIndex, void *valuePtr)
{
	(void)parserHandle;
	(void)arrayIndex;
	if (eventType == 1) {
		if (depth == 2) {
			FriendUserProfile entry;
			m_profiles.push_back(entry);
			m_profiles[m_profiles.size() - 1].m_relation = m_defaultRelation;
		}
		return 0;
	}
	if (eventType != 6 || valuePtr == NULL)
		return 0;

	const JsonTokenValue *val = (const JsonTokenValue *)valuePtr;
	switch (depth) {
	case 1:
		if (fieldIndex == 4U)
			ParseJsonInt(val, &m_startOffset);
		else if (fieldIndex == 5U)
			ParseJsonInt(val, &m_totalResults);
		break;
	case 3: {
		FriendUserProfile &cur = m_profiles[m_profiles.size() - 1];
		switch (fieldIndex) {
		case 0U:
			ParseJsonNpId(val, &cur.m_npId);
			if (cur.IsSelf())
				cur.m_relation = FRIEND_RELATION_SELF;
			break;
		case 2U:
			ParseJsonHexColor(val, &cur.m_panelBgColor);
			break;
		case 3U:
			ParseJsonString(val, &cur.m_panelUrl);
			break;
		case 4U: {
			int32_t plusVal = 0;
			ParseJsonInt(val, &plusVal);
			cur.m_isPlus = (plusVal == 1);
			break;
		}
		case 6U: {
			string relStr;
			ParseJsonString(val, &relStr);
			cur.SetRelationFromString(relStr);
			break;
		}
		case 8U:
			ParseJsonBool(val, &cur.m_hasRequestMessage);
			break;
		default:
			break;
		}
		break;
	}
	case 4:
		if (fieldIndex == 1U) {
			int32_t levelVal = 0;
			ParseJsonInt(val, &levelVal);
			m_profiles[m_profiles.size() - 1].SetTrophyLevel(levelVal);
		}
		break;
	case 5: {
		FriendUserProfile &cur = m_profiles[m_profiles.size() - 1];
		if (fieldIndex == 0U) {
			ParseJsonString(val, &cur.m_avatarUrl);
		} else if (fieldIndex == 1U) {
			string stStr;
			ParseJsonString(val, &stStr);
			cur.SetOnlineStatusFromString(stStr);
		} else if (fieldIndex == 2U) {
			ParseJsonPlatform(val, &cur.m_platform);
		}
		break;
	}
	case 6:
		if (fieldIndex == 0U) {
			FriendUserProfile &cur = m_profiles[m_profiles.size() - 1];
			ParseJsonString(val, &cur.m_gameTitleName);
			FriendLibNormalizeNewlines(cur.m_gameTitleName);
		}
		break;
	default:
		break;
	}
	return 0;
}

static int FriendListJsonCallback(int parserHandle, int eventType, int depth, uint32_t fieldIndex,
				  int arrayIndex, void *valuePtr, int extra, void *userData)
{
	(void)extra;
	return ((FriendListPageCache *)userData)
		->HandleJsonEvent(parserHandle, eventType, depth, fieldIndex, arrayIndex, valuePtr);
}

int32_t FriendListPageCache::InitJsonParser(void *parserCtx, uint32_t reserveCount)
{
	int32_t ret = SceNpCommonPs4_EB1B7375(parserCtx, &s_friendListSchema,
					      FriendListJsonCallback, this);
	if (ret >= 0) {
		m_profiles.clear();
		m_startOffset = -1;
		m_totalResults = -1;
		if (m_profiles.capacity() < reserveCount)
			m_profiles.reserve(reserveCount);
	}
	return ret;
}

/* FriendListCache */
FriendListCache::FriendListCache()
	: m_mutex("SceFriendLibList", 0), m_pages(), m_fetchingPage(NULL), m_totalResults(-1),
	  m_sortMode(0), m_invalidated(false)
{
	for (int32_t i = 0; i < 3; ++i)
		m_pages.push_back(new FriendListPageCache());
}

FriendListCache::~FriendListCache()
{
	for (list<FriendListPageCache *>::iterator it = m_pages.begin(); it != m_pages.end(); ++it)
		delete *it;
	if (m_fetchingPage != NULL)
		delete m_fetchingPage;
}

void FriendListCache::ClearAll()
{
	m_mutex.Lock();
	for (list<FriendListPageCache *>::iterator it = m_pages.begin(); it != m_pages.end(); ++it) {
		(*it)->m_profiles.clear();
		(*it)->m_startOffset = -1;
		(*it)->m_totalResults = -1;
	}
	m_totalResults = -1;
	m_mutex.Unlock();
}

void FriendListCache::SetSortMode(int32_t sortMode)
{
	m_mutex.Lock();
	m_sortMode = sortMode;
	ClearAll();
	if (m_fetchingPage != NULL)
		m_invalidated = true;
	m_mutex.Unlock();
}

int32_t FriendListCache::BeginFetch()
{
	m_mutex.Lock();
	if (m_fetchingPage != NULL) {
		m_mutex.Unlock();
		return FRIEND_SELECT_ERROR_BUSY;
	}
	m_fetchingPage = m_pages.back();
	m_pages.pop_back();
	m_mutex.Unlock();
	return 0;
}

void FriendListCache::EndFetch(int32_t result)
{
	m_mutex.Lock();
	if (m_fetchingPage == NULL) {
		m_mutex.Unlock();
		return;
	}
	if (result < 0 || m_invalidated) {
		m_fetchingPage->m_profiles.clear();
		m_fetchingPage->m_startOffset = -1;
		m_fetchingPage->m_totalResults = -1;
		m_pages.push_back(m_fetchingPage);
	} else {
		m_totalResults = m_fetchingPage->m_totalResults;
		m_pages.push_front(m_fetchingPage);
	}
	m_fetchingPage = NULL;
	m_invalidated = false;
	m_mutex.Unlock();
}

namespace paf {
template <>
inline __attribute__((always_inline))
list<FriendListPageCache *, allocator<FriendListPageCache *> >::iterator
list<FriendListPageCache *, allocator<FriendListPageCache *> >::erase(iterator where)
{
	_Node *node = where.node;
	_Node *next = node->next;
	node->next->prev = node->prev;
	node->prev->next = node->next;
	destroy(&node->value);
	operator delete(node);
	--count;
	return iterator(next);
}
}

const FriendUserProfile &FriendListCache::GetProfileAt(int32_t index)
{
	m_mutex.Lock();
	if (m_totalResults >= 0) {
		for (list<FriendListPageCache *>::iterator it = m_pages.begin();
		     it != m_pages.end(); ++it) {
			FriendListPageCache *page = *it;
			if (page->m_startOffset >= 0 && index >= page->m_startOffset &&
			    index < page->m_startOffset + (int32_t)page->m_profiles.size()) {
				const FriendUserProfile &found =
					page->m_profiles[(size_t)(index - page->m_startOffset)];
				m_pages.erase(it);
				m_pages.push_front(page);
				m_mutex.Unlock();
				return found;
			}
		}
	}
	m_mutex.Unlock();
	return s_emptyProfile;
}

/* Global SceFriendLib ref-counted lifecycle */
void FriendLibInit(HeapAllocator *allocator, SurfacePool *surfacePool)
{
	if (s_libInitRefCount++ == 0) {
		FriendLibCoreInit(allocator, surfacePool);
		FriendLibImageManagerInit(surfacePool);
	}
}

void FriendLibStart(void)
{
	if (s_libStartRefCount++ == 0)
		FriendLibPopupInit();
}

void FriendLibStop(void)
{
	if (--s_libStartRefCount == 0)
		FriendLibPopupTerm();
	if (s_libStartRefCount < 0)
		s_libStartRefCount = 0;
}

void FriendLibTerm(void)
{
	if (--s_libInitRefCount == 0) {
		FriendLibImageManagerTerm();
		FriendLibCoreTerm();
	}
	if (s_libInitRefCount < 0)
		s_libInitRefCount = 0;
}

void FriendLibRegisterWidgetFactory(void)
{
	if (s_factoryRefCount++ == 0)
		Plugin::RegistFactoryTable(g_updateButtonFactoryTable.m_name,
					   g_updateButtonFactoryTable);
}

void FriendLibUnregisterWidgetFactory(void)
{
	if (--s_factoryRefCount == 0)
		Plugin::RemoveFactoryTable(g_updateButtonFactoryTable.m_name);
	if (s_factoryRefCount < 0)
		s_factoryRefCount = 0;
}
