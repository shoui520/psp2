/*
 * np_push_net.cpp: SceNpPush heap, allocator, and HTTP client context management.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_net.cpp
 * 3.65-CEX: 0x813AD14C..0x813AD2E1
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static NpPushHeap s_pushHeap;
static NpPushAllocator s_pushAllocator;
static NpPushAllocatorWithError s_pushAllocatorWithError;
static bool s_heapInitialized = false;

static NpPushHttpContext *s_npHttpClient = NULL;
static bool s_netInitialized = false;

/* 0x813AD14C: InitHeap */
int InitHeap(void)
{
	if (s_heapInitialized)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	sceClibMemset(&s_pushHeap, 0, sizeof(s_pushHeap));
	int ret = SceNpCommonPs4_39CF37A2(&s_pushHeap, NP_PUSH_HEAP_SIZE, "SceNpPushHeap");
	if (ret < 0)
		return ret;

	SceNpCommonPs4_EDB4C46E(&s_pushHeap, &s_pushAllocator);
	SceNpCommonPs4_5A6F85BC(&s_pushHeap, SCE_NP_PUSH_ERROR_OUT_OF_MEMORY,
				&s_pushAllocatorWithError);
	s_heapInitialized = true;
	return 0;
}

/* 0x813AD19C: TermHeap */
void TermHeap(void)
{
	if (!s_heapInitialized)
		return;

	SceNpCommonPs4_C52C787A(&s_pushHeap);
	sceClibMemset(&s_pushAllocator, 0, sizeof(s_pushAllocator));
	sceClibMemset(&s_pushAllocatorWithError, 0, sizeof(s_pushAllocatorWithError));
	s_heapInitialized = false;
}

/* 0x813AD1CC: GetAllocator */
NpPushAllocator *GetAllocator(void)
{
	return &s_pushAllocator;
}

/* 0x813AD1D8: GetAllocatorWithError */
NpPushAllocatorWithError *GetAllocatorWithError(void)
{
	return &s_pushAllocatorWithError;
}

/* 0x813AD1E4: Alloc */
void *Alloc(SceSize size)
{
	if (!s_heapInitialized)
		return NULL;
	return SceNpCommonPs4_EB6E37A8(&s_pushHeap.mspace, size);
}

/* 0x813AD1FC: Free */
void Free(void *pointer)
{
	if (!s_heapInitialized || pointer == NULL)
		return;
	SceNpCommonPs4_1514F370(&s_pushHeap.mspace, pointer);
}

/* 0x813AD214: InitNet */
int InitNet(void)
{
	if (s_netInitialized)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	SceInt32 httpCtxId = SceNpManager_F66A7AAE();
	if (httpCtxId < 0)
		return httpCtxId;

	void *rawMem = SceNpCommonPs4_0C922423(sizeof(NpPushHttpContext), GetAllocator());
	if (rawMem == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	NpPushHttpContext *httpClient = SceNpCommonPs4_53D6E2C9(
		static_cast<NpPushHttpContext *>(rawMem), GetAllocatorWithError());
	int ret = SceNpCommonPs4_100E042E(httpClient, httpCtxId, 4);
	if (ret < 0) {
		typedef void (*DestructorFn)(NpPushHttpContext *);
		DestructorFn deleteFn = reinterpret_cast<DestructorFn>(httpClient->vtable[1]);
		deleteFn(httpClient);
		return ret;
	}

	s_npHttpClient = httpClient;
	s_netInitialized = true;
	return 0;
}

/* 0x813AD28C: TermNet */
void TermNet(void)
{
	if (!s_netInitialized)
		return;

	if (s_npHttpClient != NULL) {
		typedef void (*DestructorFn)(NpPushHttpContext *);
		DestructorFn deleteFn = reinterpret_cast<DestructorFn>(s_npHttpClient->vtable[1]);
		deleteFn(s_npHttpClient);
		s_npHttpClient = NULL;
	}
	s_netInitialized = false;
}

/* 0x813AD2B8: GetNpHttpClient */
NpPushHttpContext *GetNpHttpClient(void)
{
	NP_PUSH_ASSERT(s_npHttpClient != NULL, "GetNpHttpClient",
		       "core\\np_push_net.cpp", 125, "s_npHttpClient != NULL");
	return s_npHttpClient;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
