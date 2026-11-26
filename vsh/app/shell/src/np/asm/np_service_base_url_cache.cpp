/*
 * np_service_base_url_cache.cpp: LRU cache of ASM service base URLs
 * (sce::np::NpServiceBaseUrlCache, 64 entries, 12-hour TTL).
 * Sony path: vsh/app/common/src/np/np_common_ps4_vsh/npext_vsh/np_service_base_url/np_service_base_url_cache.cpp
 * 3.65-CEX: 0x813B976C..0x813B9D03
 */
#include <stddef.h>
#include <fw.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>

#define NP_BASE_URL_CACHE_CAPACITY	64U
#define NP_BASE_URL_CACHE_TTL_USEC	43200000000ULL	/* 12 hours in microseconds */

#if defined(FW_TYPE_CEX)
#define NP_ASM_CACHE_BUILD_ID "J01509657"
#elif FW_VERSION == 0x03630011
#define NP_ASM_CACHE_BUILD_ID "J01508051"
#else
#define NP_ASM_CACHE_BUILD_ID "J01509658"
#endif

#define NP_ASM_CACHE_SOURCE_PATH \
	"W:\\Build\\" NP_ASM_CACHE_BUILD_ID \
	"\\vsh\\app\\common\\src\\np\\np_common_ps4_vsh\\npext_vsh\\np_service_base_url\\np_service_base_url_cache.cpp"

#define NP_ASM_CACHE_ASSERT(cond, func, line, expr) \
	do { \
		if (!(cond)) { \
			SceNpCommonPs4_F50BD96A(sceClibPrintf( \
				"Assertion in %s(%s:%d): %s(%s)\n", \
				func, NP_ASM_CACHE_SOURCE_PATH, line, "ASSERT", expr)); \
		} \
	} while (0)

struct NpPushAllocator;
struct NpPushAllocatorWithError;
struct NpPushHandle;
struct NpPushHttpContext;
struct NpPushStringHolder;

struct NpBaseUrlCacheAllocator {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
	int errorCode;
};

struct NpBaseUrlCacheMutex {
	const void *const *vtable;
	SceUID mutexId;
	SceUInt8 initialized;
};

struct NpBaseUrlTitleIdObj {
	const void *const *vtable;
	char titleId[13];
	char padding[3];
};

extern "C" {
extern void *SceNpCommonPs4_D154D84F(SceSize size, NpBaseUrlCacheAllocator *allocator);
extern NpBaseUrlCacheAllocator *SceNpCommonPs4_31DB2D97(NpBaseUrlCacheAllocator *allocator);
extern void *SceNpCommonPs4_AA980113(NpBaseUrlCacheAllocator *allocator, SceSize size);
extern int SceNpCommonPs4_6B428829(NpBaseUrlCacheAllocator *allocator, void *pointer);
extern int SceNpCommonPs4_BB5282C8(void *object);
extern int SceNpCommonPs4_F50BD96A(int printed);
extern NpBaseUrlCacheMutex *SceNpCommonPs4_4266D870(NpBaseUrlCacheMutex *mutex);
extern NpBaseUrlCacheMutex *SceNpCommonPs4_294F0B1A(NpBaseUrlCacheMutex *mutex);
extern int SceNpCommonPs4_8AA7E995(NpBaseUrlCacheMutex *mutex, const char *name, char recursive);
extern int SceNpCommonPs4_63EED478(NpBaseUrlCacheMutex *mutex);
extern int SceNpCommonPs4_A2A8493B(NpBaseUrlCacheMutex *mutex);
extern int SceNpCommonPs4_6BA5A164(NpBaseUrlCacheMutex *mutex);
extern int SceNpCommonPs4_F56B0EAB(SceInt64 *outTick);
extern int SceNpCommonPs4_17EB36C2(void *outTitleId, void *outTitleSecret);
extern NpBaseUrlTitleIdObj *SceNpCommonPs4_DAA742B0(NpBaseUrlTitleIdObj *titleObj,
						    const void *rawTitleId);
extern NpBaseUrlTitleIdObj *SceNpCommonPs4_57005284(NpBaseUrlTitleIdObj *titleObj);
extern int SceNpCommonPs4_0B2A3748(NpPushStringHolder *outUrl, NpPushHandle *handle,
				   NpPushHttpContext *httpContext, const char *onlineId,
				   const char *titleId, const char *serviceName);
extern int SceNpCommonPs4_B0F7DE6E(NpPushStringHolder *holder, const char *url);
extern const char *SceNpCommonPs4_2CE76BE5(const NpPushStringHolder *holder);
}

namespace sce {
namespace np {

struct BaseUrlCacheRecord {
	BaseUrlCacheRecord *prev;
	BaseUrlCacheRecord *next;
	SceUInt64 timestampUsec;
	char titleId[13];
	char serviceName[33];
	char url[129];
};

class NpServiceBaseUrlCache {
public:
	static int Initialize(NpBaseUrlCacheAllocator *allocator);
	static void Terminate(void);
	static NpServiceBaseUrlCache *GetInstance(void);

	explicit NpServiceBaseUrlCache(NpBaseUrlCacheAllocator *allocator);
	~NpServiceBaseUrlCache();

	int Lookup(const char *titleId, const char *serviceName, NpPushStringHolder *outUrl);
	int Store(const char *titleId, const char *serviceName, const char *url);

private:
	void MoveToFront(BaseUrlCacheRecord *record);
	void EvictOldest(void);

	NpBaseUrlCacheMutex m_mutex;
	NpBaseUrlCacheAllocator *m_allocator;
	SceUInt32 m_capacity;
	SceUInt64 m_ttlUsec;
	BaseUrlCacheRecord *m_head;
	BaseUrlCacheRecord *m_tail;
	SceUInt32 m_count;
};

static NpServiceBaseUrlCache *s_cacheInstance = NULL;

/* 0x813B9858 */
int NpServiceBaseUrlCache::Initialize(NpBaseUrlCacheAllocator *allocator)
{
	NP_ASM_CACHE_ASSERT(s_cacheInstance == NULL, "Initialize", 48, "s_instance == 0");

	void *rawMem = SceNpCommonPs4_D154D84F(sizeof(NpServiceBaseUrlCache), allocator);
	if (rawMem == NULL)
		return allocator->errorCode;

	NpServiceBaseUrlCache *instance = static_cast<NpServiceBaseUrlCache *>(rawMem);
	instance->m_allocator = allocator;
	instance->m_capacity = NP_BASE_URL_CACHE_CAPACITY;
	instance->m_ttlUsec = NP_BASE_URL_CACHE_TTL_USEC;
	instance->m_head = NULL;
	instance->m_tail = NULL;
	instance->m_count = 0;
	SceNpCommonPs4_4266D870(&instance->m_mutex);
	SceNpCommonPs4_8AA7E995(&instance->m_mutex, "sce::np::NpServiceBaseUrlCache", 0);

	s_cacheInstance = instance;
	return 0;
}

/* 0x813B9912 */
void NpServiceBaseUrlCache::Terminate(void)
{
	if (s_cacheInstance == NULL)
		return;

	s_cacheInstance->~NpServiceBaseUrlCache();
	SceNpCommonPs4_BB5282C8(s_cacheInstance);
	s_cacheInstance = NULL;
}

/* 0x813B9938 */
NpServiceBaseUrlCache *NpServiceBaseUrlCache::GetInstance(void)
{
	NP_ASM_CACHE_ASSERT(s_cacheInstance != NULL, "GetInstance", 78, "s_instance");
	return s_cacheInstance;
}

NpServiceBaseUrlCache::~NpServiceBaseUrlCache()
{
	NpBaseUrlCacheAllocator *pool = SceNpCommonPs4_31DB2D97(m_allocator);
	BaseUrlCacheRecord *cur = m_head;
	while (cur != NULL) {
		BaseUrlCacheRecord *nextRecord = cur->next;
		SceNpCommonPs4_6B428829(pool, cur);
		cur = nextRecord;
	}
	m_head = NULL;
	m_tail = NULL;
	m_count = 0;
	SceNpCommonPs4_63EED478(&m_mutex);
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

void NpServiceBaseUrlCache::MoveToFront(BaseUrlCacheRecord *record)
{
	if (record == m_head)
		return;

	if (record->prev != NULL)
		record->prev->next = record->next;
	if (record->next != NULL)
		record->next->prev = record->prev;
	else
		m_tail = record->prev;

	record->prev = NULL;
	record->next = m_head;
	if (m_head != NULL)
		m_head->prev = record;
	m_head = record;
	if (m_tail == NULL)
		m_tail = record;
}

void NpServiceBaseUrlCache::EvictOldest(void)
{
	if (m_tail == NULL)
		return;

	BaseUrlCacheRecord *oldest = m_tail;
	m_tail = oldest->prev;
	if (m_tail != NULL)
		m_tail->next = NULL;
	else
		m_head = NULL;

	NpBaseUrlCacheAllocator *pool = SceNpCommonPs4_31DB2D97(m_allocator);
	SceNpCommonPs4_6B428829(pool, oldest);
	if (m_count > 0)
		m_count--;
}

/* 0x813B998C */
int NpServiceBaseUrlCache::Lookup(const char *titleId, const char *serviceName,
				  NpPushStringHolder *outUrl)
{
	SceNpCommonPs4_A2A8493B(&m_mutex);
	SceInt64 nowSigned = 0;
	if (SceNpCommonPs4_F56B0EAB(&nowSigned) < 0) {
		SceNpCommonPs4_6BA5A164(&m_mutex);
		return 0;
	}
	SceUInt64 nowUsec = static_cast<SceUInt64>(nowSigned);

	for (BaseUrlCacheRecord *cur = m_head; cur != NULL; cur = cur->next) {
		if (sceClibStrncmp(cur->titleId, titleId, sizeof(cur->titleId)) == 0 &&
		    sceClibStrncmp(cur->serviceName, serviceName, sizeof(cur->serviceName)) == 0) {
			if (cur->timestampUsec <= nowUsec &&
			    (nowUsec - cur->timestampUsec) <= m_ttlUsec) {
				SceNpCommonPs4_B0F7DE6E(outUrl, cur->url);
				MoveToFront(cur);
				SceNpCommonPs4_6BA5A164(&m_mutex);
				return 1;
			}
		}
	}

	SceNpCommonPs4_6BA5A164(&m_mutex);
	return 0;
}

/* 0x813B9AB4 */
int NpServiceBaseUrlCache::Store(const char *titleId, const char *serviceName, const char *url)
{
	SceNpCommonPs4_A2A8493B(&m_mutex);
	if (sceClibStrnlen(titleId, (SceSize)-1) > 12U ||
	    sceClibStrnlen(serviceName, (SceSize)-1) > 32U ||
	    sceClibStrnlen(url, (SceSize)-1) > 128U) {
		SceNpCommonPs4_6BA5A164(&m_mutex);
		return 0;
	}

	if (m_count >= m_capacity)
		EvictOldest();

	SceInt64 nowSigned = 0;
	int ret = SceNpCommonPs4_F56B0EAB(&nowSigned);
	if (ret < 0) {
		SceNpCommonPs4_6BA5A164(&m_mutex);
		return ret;
	}

	NpBaseUrlCacheAllocator *pool = SceNpCommonPs4_31DB2D97(m_allocator);
	BaseUrlCacheRecord *record = static_cast<BaseUrlCacheRecord *>(
		SceNpCommonPs4_AA980113(pool, sizeof(BaseUrlCacheRecord)));
	if (record == NULL) {
		SceNpCommonPs4_6BA5A164(&m_mutex);
		return m_allocator->errorCode;
	}

	sceClibMemset(record, 0, sizeof(*record));
	record->timestampUsec = static_cast<SceUInt64>(nowSigned);
	sceClibStrncpy(record->titleId, titleId, sizeof(record->titleId));
	sceClibStrncpy(record->serviceName, serviceName, sizeof(record->serviceName));
	sceClibStrncpy(record->url, url, sizeof(record->url));

	record->prev = NULL;
	record->next = m_head;
	if (m_head != NULL)
		m_head->prev = record;
	m_head = record;
	if (m_tail == NULL)
		m_tail = record;
	m_count++;

	SceNpCommonPs4_6BA5A164(&m_mutex);
	return 0;
}

} /* namespace np */
} /* namespace sce */

/* 0x813B976C */
int npAsmServiceCacheInit(NpBaseUrlCacheAllocator *allocator)
{
	return sce::np::NpServiceBaseUrlCache::Initialize(allocator);
}

/* 0x813B9774 */
void npAsmServiceCacheTerm(void)
{
	sce::np::NpServiceBaseUrlCache::Terminate();
}

/* 0x813B977E: Lookup or fetch & cache ASM service base URL */
int npAsmGetServiceBaseUrl(NpPushHandle *handle, NpPushHttpContext *httpContext,
			   const char *onlineId, const char *serviceName,
			   NpPushStringHolder *outUrl)
{
	unsigned char titleIdRaw[16];
	unsigned char titleSecretBuf[128];
	int ret = SceNpCommonPs4_17EB36C2(titleIdRaw, titleSecretBuf);
	if (ret < 0)
		return ret;

	NpBaseUrlTitleIdObj titleObj;
	SceNpCommonPs4_DAA742B0(&titleObj, titleIdRaw);

	sce::np::NpServiceBaseUrlCache *cache = sce::np::NpServiceBaseUrlCache::GetInstance();
	if (cache->Lookup(titleObj.titleId, serviceName, outUrl) == 0) {
		ret = SceNpCommonPs4_0B2A3748(outUrl, handle, httpContext, onlineId,
					      titleObj.titleId, serviceName);
		if (ret >= 0) {
			const char *fetchedUrl = SceNpCommonPs4_2CE76BE5(outUrl);
			cache->Store(titleObj.titleId, serviceName, fetchedUrl);
			ret = 0;
		}
	} else {
		ret = 0;
	}

	SceNpCommonPs4_57005284(&titleObj);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813B9826: Called by np_asm_client_vsh.cpp */
int npAsmServiceCacheGet(const char *titleId, const char *serviceName,
			 NpPushStringHolder *outUrl)
{
	sce::np::NpServiceBaseUrlCache *cache = sce::np::NpServiceBaseUrlCache::GetInstance();
	return cache->Lookup(titleId, serviceName, outUrl);
}

/* 0x813B983E: Called by np_asm_client_vsh.cpp */
int npAsmServiceCachePut(const char *titleId, const char *serviceName, const char *url)
{
	sce::np::NpServiceBaseUrlCache *cache = sce::np::NpServiceBaseUrlCache::GetInstance();
	return cache->Store(titleId, serviceName, url);
}
