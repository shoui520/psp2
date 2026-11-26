/*
 * np_ipc2_server.cpp: SceNpService IPMI server initialization, global server registry,
 * heap allocator, and 3-service registration table (SceNpPush, SceNpGamePrese, SceNpAsmClient).
 * 3.65-CEX: 0x813B6928..0x813B6C03
 */
#include "np_ipc2_internal.h"

namespace sce {
namespace Np {
namespace Ipc2 {

struct ServerRegistry {
	IpmiServer *head;
	IpmiServer *tail;
};

static ServiceServer s_npServiceServer;
static ServerRegistry s_serverRegistry = { NULL, NULL };
static NpIpc2Heap s_ipc2Heap;
static NpIpc2Allocator s_ipc2Allocator;
static NpIpc2AllocatorWithError s_ipc2AllocatorWithError;
static bool s_ipc2Initialized = false;

static int NoOpSessionCallback(IPMI::Session *session)
{
	(void)session;
	return 0;
}

static int AsmAsyncAdapter(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			   const IPMI::DataInfo *inList, SceUInt32 inCount)
{
	(void)session;
	(void)requestId;
	(void)methodId;
	(void)inList;
	(void)inCount;
	return npAsmVshIpcOnAsyncMethod();
}

/* 0x815490B8: SceNpService registered services table */
static const ServiceDescriptor kNpServiceDescriptors[] = {
	{
		0x10000U, 0x1FFFFU, 0, "SceNpPush",
		NULL, NULL,
		npPushIpcOnClientOpen, npPushIpcOnClientClose,
		npPushIpcOnSyncMethod, npPushIpcOnAsyncMethod,
		1, 0, 0, 0,
		NULL, NULL, NULL, 0
	},
	{
		0x20000U, 0x2FFFFU, 1, "SceNpGamePrese",
		NULL, NULL,
		NoOpSessionCallback, NoOpSessionCallback,
		npGamePresenceIpcOnSyncMethod, npGamePresenceIpcOnAsyncMethod,
		1, 0, 0, 32,
		npGamePresenceIpcCreateRequest,
		npGamePresenceIpcDestroyRequest,
		npGamePresenceIpcAbortRequest,
		0
	},
	{
		0x30000U, 0x3FFFFU, 2, "SceNpAsmClient",
		NULL, NULL,
		NoOpSessionCallback, NoOpSessionCallback,
		npAsmVshIpcOnSyncMethod, AsmAsyncAdapter,
		1, 0, 0, 0,
		NULL, NULL, NULL, 0
	},
	{
		0, 0, -1, "",
		NULL, NULL, NULL, NULL, NULL, NULL,
		0, 0, 0, 0, NULL, NULL, NULL, 0
	}
};

/* 0x813B6B64 */
int InitHeap(SceSize size)
{
	int ret = SceNpCommonPs4_39CF37A2(&s_ipc2Heap, size, "SceNpIpc2ServerHeap");
	return (ret >= 0) ? 0 : ret;
}

/* 0x813B6B86 */
void TermHeap(void)
{
	SceNpCommonPs4_C52C787A(&s_ipc2Heap);
}

/* 0x813B6B96 */
NpIpc2Allocator *GetAllocator(void)
{
	SceNpCommonPs4_EDB4C46E(&s_ipc2Heap, &s_ipc2Allocator);
	return &s_ipc2Allocator;
}

/* 0x813B6BB2 */
NpIpc2AllocatorWithError *GetAllocatorWithError(void)
{
	SceNpCommonPs4_5A6F85BC(&s_ipc2Heap, SCE_NP_IPC2_ERROR_OUT_OF_MEMORY,
				&s_ipc2AllocatorWithError);
	return &s_ipc2AllocatorWithError;
}

/* 0x813B6BD6 */
void *Alloc(SceSize size)
{
	return SceNpCommonPs4_EB6E37A8(&s_ipc2Heap.mspace, size);
}

/* 0x813B6BEC */
void Free(void *pointer)
{
	SceNpCommonPs4_1514F370(&s_ipc2Heap.mspace, pointer);
}

/* 0x813B6A46 */
static void RegisterServerInList(ServerRegistry *registry, IpmiServer *server)
{
	server->m_nextServer = NULL;
	server->m_prevServer = registry->tail;
	if (registry->tail != NULL)
		registry->tail->m_nextServer = server;
	if (registry->head == NULL)
		registry->head = server;
	registry->tail = server;
}

/* 0x813B6A60 */
static int StartAllServers(ServerRegistry *registry)
{
	for (IpmiServer *cur = registry->head; cur != NULL; cur = cur->m_nextServer) {
		int ret = cur->StartDispatch();
		if (ret < 0) {
			for (IpmiServer *rev = registry->tail; rev != NULL; rev = rev->m_prevServer) {
				if (rev->IsDispatching())
					rev->StopDispatch();
			}
			return ret;
		}
	}
	return 0;
}

/* 0x813B6AC4 */
static int InitIpc2Framework(void)
{
	if (s_ipc2Initialized)
		return SCE_NP_IPC2_ERROR_ALREADY_INITIALIZED;

	int ret = InitHeap(NP_IPC2_SERVER_HEAP_SIZE);
	if (ret < 0)
		return ret;

	s_ipc2Initialized = true;
	return 0;
}

/* 0x813B6B2E */
static int TermIpc2Framework(void)
{
	if (!s_ipc2Initialized)
		return SCE_NP_IPC2_ERROR_NOT_INITIALIZED;

	TermHeap();
	s_ipc2Initialized = false;
	return 0;
}

} /* namespace Ipc2 */
} /* namespace Np */
} /* namespace sce */

/* 0x813B6928: Initialize SceNpService IPC2 server */
int npServiceInit(void)
{
	using namespace sce::Np::Ipc2;

	int ret = InitIpc2Framework();
	if (ret < 0)
		return ret;

	IpmiServerConfig config;
	sceClibMemset(&config, 0, sizeof(config));
	sceClibStrlcpy(config.serverName, "SceNpService", sizeof(config.serverName));
	config.maxClients = 8;
	config.maxSyncRequestSize = 0x4000;
	config.maxSyncResponseSize = 0x4000;
	config.maxAsyncRequestSize = 8;
	config.maxAsyncResponseSize = 0x4000;
	config.maxEventDataSize = 0x4000;
	config.maxBlockingRequests = 4;

	ret = s_npServiceServer.Init(&config);
	if (ret >= 0) {
		ret = s_npServiceServer.RegisterServices(kNpServiceDescriptors);
		if (ret >= 0) {
			RegisterServerInList(&s_serverRegistry, &s_npServiceServer);
			return 0;
		}
		s_npServiceServer.Destroy();
	}

	TermIpc2Framework();
	return ret;
}

/* 0x813B69D4: Start dispatch threads for all registered NP IPC2 servers */
int npIpc2ServerStart(void)
{
	using namespace sce::Np::Ipc2;

	int ret = StartAllServers(&s_serverRegistry);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813B69E2: Finish an outstanding blocking request for a client session */
int npIpc2ServerFinishRequest(IPMI::Session *session, SceInt32 serviceType, int requestId)
{
	using namespace sce::Np::Ipc2;
	return s_npServiceServer.FinishRequest(session, serviceType, requestId);
}

/* 0x813B69F8: Signal an event flag bit pattern for a client session */
int npIpc2ServerSetEventFlag(IPMI::Session *session, SceUInt32 serviceId,
			     SceUInt32 evfIndex, SceUInt64 bitPattern)
{
	using namespace sce::Np::Ipc2;
	return s_npServiceServer.SetEventFlag(session, static_cast<SceInt32>(serviceId),
					      evfIndex, bitPattern);
}
