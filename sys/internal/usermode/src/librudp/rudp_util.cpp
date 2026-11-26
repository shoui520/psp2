/*
 * rudp_util.cpp: memory allocator, address comparators, and RudpResult wrapper.
 */
#include "rudp_internal.h"

static void *rudpMspaceAlloc(SceSize size);
static void rudpMspaceFree(void *ptr);

static void *(*g_allocFn)(SceSize size) = rudpMspaceAlloc;
static void (*g_freeFn)(void *ptr) = rudpMspaceFree;
static void *g_mspace = 0;
static int g_mspaceInitialized = 0;

static const char *g_lastErrorFile = 0;
static int g_lastErrorLine = 0;

static uint32_t g_memCurrent = 0;
static uint32_t g_memPeak = 0;
static uint32_t g_allocCount = 0;
static uint32_t g_freeCount = 0;

struct RudpAllocHeader {
	uint64_t totalSize;
	uint32_t reservedB;
	uint32_t reservedC;
};

static void *rudpMspaceAlloc(SceSize size)
{
	if (g_mspace != 0) {
		return sceClibMspaceMalloc(g_mspace, size);
	}
	return 0;
}

static void rudpMspaceFree(void *ptr)
{
	if (g_mspace != 0) {
		sceClibMspaceFree(g_mspace, ptr);
	}
}

void rudpFree(void *ptr)
{
	RudpAllocHeader *hdr = (RudpAllocHeader *)ptr - 1;
	rudpAtomicAdd(&g_freeCount, 1);
	rudpAtomicSub(&g_memCurrent, (int32_t)hdr->totalSize);
	g_freeFn(hdr);
}

void operator delete(void *ptr)
{
	RudpAllocHeader *hdr = (RudpAllocHeader *)ptr - 1;
	rudpAtomicAdd(&g_freeCount, 1);
	rudpAtomicSub(&g_memCurrent, (int32_t)hdr->totalSize);
	g_freeFn(hdr);
}

int rudpSockaddrEqual(const RudpSockaddr *lhs, const RudpSockaddr *rhs)
{
	uint8_t fam = lhs->sa_family;
	if (fam != rhs->sa_family) {
		return 0;
	}
	if (fam != SCE_NET_AF_INET) {
		uint8_t len = lhs->sa_len;
		if (len == 0) {
			return 1;
		}
		return (len == rhs->sa_len) &&
		       (rudpMemcmp(lhs->sa_data, rhs->sa_data, len - 2) == 0);
	}
	return (rudpMemcmp(lhs->sa_data, rhs->sa_data, 2) == 0) &&
	       (rudpMemcmp(lhs->sa_data + 2, rhs->sa_data + 2, 4) == 0);
}

int rudpSockaddrLess(const RudpSockaddr *lhs, const RudpSockaddr *rhs)
{
	uint8_t lhsFam = lhs->sa_family;
	uint8_t rhsFam = rhs->sa_family;
	if (lhsFam < rhsFam) {
		return 1;
	}
	if (lhsFam > rhsFam) {
		return 0;
	}
	if (lhsFam == SCE_NET_AF_INET) {
		int cmpPort = rudpMemcmp(lhs->sa_data, rhs->sa_data, 2);
		if (cmpPort < 0) {
			return 1;
		}
		if (cmpPort > 0) {
			return 0;
		}
		if (rudpMemcmp(lhs->sa_data + 2, rhs->sa_data + 2, 4) < 0) {
			return 1;
		}
	} else {
		uint8_t lhsLen = lhs->sa_len;
		if (lhsLen == 0) {
			return 0;
		}
		uint8_t rhsLen = rhs->sa_len;
		if (lhsLen < rhsLen) {
			return 1;
		}
		if (lhsLen > rhsLen) {
			return 0;
		}
		if (rudpMemcmp(lhs->sa_data, rhs->sa_data, lhsLen - 2) < 0) {
			return 1;
		}
	}
	return 0;
}

int rudpRecordError(const char *file, int line)
{
	g_lastErrorFile = file;
	g_lastErrorLine = line;
	return -1;
}

RudpResult::RudpResult()
	: m_code(0), m_unchecked(false)
{
}

RudpResult::RudpResult(int code)
	: m_code(code), m_unchecked(code < 0)
{
}

RudpResult &RudpResult::operator=(int code)
{
	m_code = code;
	m_unchecked = (code < 0);
	return *this;
}

RudpResult::RudpResult(const RudpResult &other)
	: m_code(other.m_code), m_unchecked(other.m_unchecked)
{
	other.m_unchecked = false;
}

RudpResult &RudpResult::operator=(const RudpResult &other)
{
	m_code = other.m_code;
	m_unchecked = other.m_unchecked;
	other.m_unchecked = false;
	return *this;
}

int RudpResult::getCode()
{
	int code = m_code;
	m_unchecked = false;
	return code;
}

RudpResult::~RudpResult()
{
}

void *operator new(size_t size)
{
	uint32_t totalSize = (uint32_t)size + sizeof(RudpAllocHeader);
	RudpAllocHeader *hdr = (RudpAllocHeader *)g_allocFn(totalSize);
	if (hdr == 0) {
		return 0;
	}
	hdr->totalSize = totalSize;

	rudpAtomicAdd(&g_allocCount, 1);
	uint32_t curMem = totalSize + rudpAtomicAdd(&g_memCurrent, (int32_t)totalSize);
	uint32_t peak = rudpAtomicOr(&g_memPeak, 0);
	do {
		if (curMem <= peak) {
			break;
		}
		uint32_t prev = g_memPeak;
		peak = rudpAtomicCompareAndSwap(&g_memPeak, prev, curMem);
		if (peak == prev) {
			break;
		}
	} while (true);
	return hdr + 1;
}

int rudpMemInit(const RudpAllocator *alloc, void *pool, SceSize poolSize)
{
	if (alloc != 0) {
		g_allocFn = alloc->allocFn;
		g_freeFn = alloc->freeFn;
	} else {
		if (pool == 0 || (int)poolSize <= 0) {
			return SCE_RUDP_ERROR_INVALID_ARGUMENT;
		}
		if (!g_mspaceInitialized) {
			if ((int)poolSize > 0) {
				g_mspace = sceClibMspaceCreate(pool, poolSize);
				if (g_mspace == 0) {
					return SCE_RUDP_ERROR_MEMORY;
				}
			}
			g_mspaceInitialized = 1;
		}
		g_allocFn = rudpMspaceAlloc;
		g_freeFn = rudpMspaceFree;
	}
	g_memCurrent = 0;
	g_memPeak = 0;
	g_allocCount = 0;
	g_freeCount = 0;
	return 0;
}

void rudpMemTerm(void)
{
	if (g_mspace != 0) {
		sceClibMspaceDestroy(g_mspace);
		g_mspace = 0;
	}
	g_mspaceInitialized = 0;
}

void rudpMemGetStats(uint32_t *allocs, uint32_t *frees, uint32_t *memCurrent, uint32_t *memPeak)
{
	*memCurrent = g_memCurrent;
	*memPeak = g_memPeak;
	*allocs = g_allocCount;
	*frees = g_freeCount;
}
