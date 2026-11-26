/*
 * np_ipc2_internal.h: Internal types, error codes, and server classes for
 * SceNpService / NpIpc2Server (vsh/app/common/src/np/np_ipc_ps4/np_ipc2/server/).
 */
#ifndef NP_IPC2_INTERNAL_H
#define NP_IPC2_INTERNAL_H

#include <stddef.h>
#include <std/new>
#include <fw.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <ipmi/server.h>
#include <ipmi/session.h>

/* ============================================================================
 * Error Codes (0x8055A3xx)
 * ============================================================================ */

#define SCE_NP_IPC2_ERROR_ALREADY_INITIALIZED		(-2141871360)	/* 0x8055A300 */
#define SCE_NP_IPC2_ERROR_NOT_INITIALIZED		(-2141871359)	/* 0x8055A301 */
#define SCE_NP_IPC2_ERROR_OUT_OF_MEMORY			(-2141871358)	/* 0x8055A302 */
#define SCE_NP_IPC2_ERROR_INVALID_ARGUMENT		(-2141871357)	/* 0x8055A303 */
#define SCE_NP_IPC2_ERROR_INVALID_CONFIG		(-2141871356)	/* 0x8055A304 */
#define SCE_NP_IPC2_ERROR_REQUEST_MAX			(-2141871351)	/* 0x8055A309 */
#define SCE_NP_IPC2_ERROR_REQUEST_NOT_FOUND		(-2141871350)	/* 0x8055A30A */
#define SCE_NP_IPC2_ERROR_CLIENT_MAX			(-2141871345)	/* 0x8055A30F */
#define SCE_NP_IPC2_ERROR_EVENT_QUEUE_EMPTY		(-2141871343)	/* 0x8055A311 */
#define SCE_NP_IPC2_ERROR_SERVICE_ALREADY_REGISTERED	(-2141871342)	/* 0x8055A312 */
#define SCE_NP_IPC2_ERROR_SERVICE_NOT_REGISTERED	(-2141871341)	/* 0x8055A313 */
#define SCE_NP_IPC2_ERROR_SERVICE_MAX			(-2141871339)	/* 0x8055A315 */
#define SCE_NP_IPC2_ERROR_SERVICE_NOT_AVAILABLE		(-2141871338)	/* 0x8055A316 */
#define SCE_NP_IPC2_ERROR_NOT_SUPPORTED			(-2141871337)	/* 0x8055A317 */
#define SCE_NP_IPC2_ERROR_CLIENT_NOT_FOUND		(-2141871336)	/* 0x8055A318 */
#define SCE_NP_IPC2_ERROR_SERVICE_ALREADY_OPEN		(-2141871335)	/* 0x8055A319 */
#define SCE_NP_IPC2_ERROR_SERVICE_NOT_OPEN		(-2141871334)	/* 0x8055A31A */

#define NP_IPC2_SERVER_HEAP_SIZE			(256 * 1024)
#define NP_IPC2_MAX_SERVICES				32
#define NP_IPC2_MAX_EVENT_FLAGS				16
#define NP_IPC2_MAX_EVENT_QUEUES			32
#define NP_IPC2_MAX_EVENT_SIZE				1024

#if defined(FW_TYPE_CEX)
#define NP_IPC2_BUILD_ID "J01509657"
#elif FW_VERSION == 0x03630011
#define NP_IPC2_BUILD_ID "J01508051"
#else
#define NP_IPC2_BUILD_ID "J01509658"
#endif

#define NP_IPC2_SOURCE_PATH(rel) \
	"W:\\Build\\" NP_IPC2_BUILD_ID "\\vsh\\app\\common\\src\\np\\np_ipc_ps4\\np_ipc2\\server\\" rel

#define NP_IPC2_ASSERT(cond, func, rel, line, expr) \
	do { \
		if (!(cond)) { \
			SceNpCommonPs4_F50BD96A(sceClibPrintf( \
				"Assertion in %s(%s:%d): %s(%s)\n", \
				func, NP_IPC2_SOURCE_PATH(rel), line, "ASSERT", expr)); \
		} \
	} while (0)

/* ============================================================================
 * SceNpCommonPs4 Primitives
 * ============================================================================ */

struct NpIpc2Allocator {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
};

struct NpIpc2AllocatorWithError {
	void *(*allocate)(SceSize size, void *context);
	void *(*reallocate)(void *pointer, SceSize size, void *context);
	void (*deallocate)(void *pointer, void *context);
	void *context;
	int errorCode;
};

struct NpIpc2MemBlock {
	SceSize size;
	SceUID uid;
	void *base;
};

struct NpIpc2Heap {
	int state;
	NpIpc2MemBlock block;
	void *mspace;
};

struct NpIpc2Mutex {
	const void *const *vtable;
	SceUID mutexId;
	SceUInt8 initialized;
};

struct NpIpc2ThreadBase {
	const void *const *vtable;
	SceUID threadId;
	int priority;
	SceSize stackSize;
	char name[32];
	int cpuAffinity;
	int state;
	int exitStatus;
};

struct NpIpc2EventFlagObj {
	const void *const *vtable;
	SceUID eventFlagId;
	SceUInt32 state;
};

struct NpIpc2RingBufAlloc {
	const void *const *vtable;
	SceUInt8 *buffer;
	SceUInt8 *readPtr;
	SceUInt8 *writePtr;
	SceSize size;
	SceSize capacity;
	NpIpc2AllocatorWithError *allocator;
};

extern "C" {
extern int SceNpCommonPs4_39CF37A2(NpIpc2Heap *heap, SceSize size, const char *name);
extern void *SceNpCommonPs4_C52C787A(NpIpc2Heap *heap);
extern void *SceNpCommonPs4_EB6E37A8(void **mspacePtr, SceSize size);
extern void SceNpCommonPs4_1514F370(void **mspacePtr, void *pointer);
extern void *SceNpCommonPs4_EDB4C46E(NpIpc2Heap *heap, NpIpc2Allocator *allocator);
extern void *SceNpCommonPs4_5A6F85BC(NpIpc2Heap *heap, int errorCode,
				     NpIpc2AllocatorWithError *allocator);
extern void *SceNpCommonPs4_0C922423(SceSize size, NpIpc2Allocator *allocator);
extern int SceNpCommonPs4_6B428829(NpIpc2Allocator *allocator, void *pointer);
extern int SceNpCommonPs4_BB5282C8(void *object);
extern void SceNpCommonPs4_F50BD96A(int printed);

extern NpIpc2Mutex *SceNpCommonPs4_4266D870(NpIpc2Mutex *mutex);
extern NpIpc2Mutex *SceNpCommonPs4_294F0B1A(NpIpc2Mutex *mutex);
extern int SceNpCommonPs4_8AA7E995(NpIpc2Mutex *mutex, const char *name, char recursive);
extern int SceNpCommonPs4_63EED478(NpIpc2Mutex *mutex);
extern int SceNpCommonPs4_A2A8493B(NpIpc2Mutex *mutex);
extern int SceNpCommonPs4_6BA5A164(NpIpc2Mutex *mutex);

extern NpIpc2ThreadBase *SceNpCommonPs4_04CBEB81(NpIpc2ThreadBase *thread);
extern NpIpc2ThreadBase *SceNpCommonPs4_502309F4(NpIpc2ThreadBase *thread);
extern int SceNpCommonPs4_D699FE34(NpIpc2ThreadBase *thread, const char *name,
				   int priority, SceSize stackSize, int cpuAffinity);
extern int SceNpCommonPs4_8CBDFDB7(NpIpc2ThreadBase *thread);
extern int SceNpCommonPs4_2C0585AD(NpIpc2ThreadBase *thread, int *exitStatus);

extern NpIpc2EventFlagObj *SceNpCommonPs4_945AC0FC(NpIpc2EventFlagObj *ef);
extern NpIpc2EventFlagObj *SceNpCommonPs4_4C3E58CA(NpIpc2EventFlagObj *ef);
extern int SceNpCommonPs4_909FBD24(NpIpc2EventFlagObj *ef, const char *name);
extern int SceNpCommonPs4_E7D29E99(NpIpc2EventFlagObj *ef);
extern int SceNpCommonPs4_0F1ED1EB(NpIpc2EventFlagObj *ef, int unused, SceUInt32 bitPattern);

extern NpIpc2RingBufAlloc *SceNpCommonPs4_3245630F(NpIpc2RingBufAlloc *ringBuf,
						   NpIpc2AllocatorWithError *allocator);
extern NpIpc2RingBufAlloc *SceNpCommonPs4_A264C490(NpIpc2RingBufAlloc *ringBuf);
extern int SceNpCommonPs4_530564F5(NpIpc2RingBufAlloc *ringBuf, SceSize bufSize);
extern int SceNpCommonPs4_B599A560(NpIpc2RingBufAlloc *ringBuf);
extern int SceNpCommonPs4_DC5607D0(NpIpc2RingBufAlloc *ringBuf, void *dst, SceSize length);
}

/* ============================================================================
 * sce::Np::Ipc2 Server Classes
 * ============================================================================ */

namespace sce {
namespace Np {
namespace Ipc2 {

int InitHeap(SceSize size);
void TermHeap(void);
NpIpc2Allocator *GetAllocator(void);
NpIpc2AllocatorWithError *GetAllocatorWithError(void);
void *Alloc(SceSize size);
void Free(void *pointer);

/* Forward declarations */
class IpmiSession;
class IpmiServer;
class BlockingServer;
class BlockingSession;
class EventFlagServer;
class EventFlagSession;
class EventQueueServer;
class EventQueueSession;
class ServiceEntry;
class ServiceSession;
class ServiceServer;

struct IpmiServerConfig {
	char serverName[16];
	SceUInt32 maxClients;
	SceUInt32 maxSyncRequestSize;
	SceUInt32 maxSyncResponseSize;
	SceUInt32 maxAsyncRequestSize;
	SceUInt32 maxAsyncResponseSize;
	SceUInt32 maxEventDataSize;
	SceUInt32 maxBlockingRequests;
};

struct BlockingServerConfig {
	SceUInt32 maxOutstanding;
};

struct BlockingHandler {
	virtual ~BlockingHandler() {}
	virtual int CreateRequest(IPMI::Session *session, const void *optionBuf,
				  SceSize optionSize, int *outRequestId) = 0;
	virtual int DestroyRequest(IPMI::Session *session, int requestId) = 0;
	virtual int AbortRequest(IPMI::Session *session, int requestId) = 0;
};

struct BlockingSessionConfig {
	SceUInt32 maxRequests;
	BlockingHandler *handler;
};

enum BlockingRequestState {
	BLOCKING_REQ_DORMANT	= 0,
	BLOCKING_REQ_CREATED	= 1,
	BLOCKING_REQ_STARTED	= 2,
	BLOCKING_REQ_ABORTING	= 3,
	BLOCKING_REQ_FINISHED	= 4
};

struct BlockingRequest {
	BlockingRequestState state;
	int requestId;
	int bitIndex;
	BlockingSession *owner;
};

class BlockingServer {
public:
	BlockingServer();
	~BlockingServer();

	int Init(ServiceServer *server, ServiceSession *session,
		 const BlockingServerConfig *config);
	ServiceSession *GetServiceSession(void) const { return m_serviceSession; }

private:
	ServiceServer *m_server;
	ServiceSession *m_serviceSession;
	SceUInt32 m_maxOutstanding;
	NpIpc2Mutex m_mutex;
};

class BlockingSession {
public:
	BlockingSession(BlockingServer *server, ServiceEntry *serviceEntry);
	~BlockingSession();

	int Init(const BlockingSessionConfig *config);
	int CreateRequest(IPMI::Session *session, int *outRequestId, const void *optionBuf,
			  SceSize optionSize, char flags);
	int DestroyRequest(IPMI::Session *session, int requestId, char flags);
	int AbortRequest(IPMI::Session *session, int requestId, char flags);
	int StartRequest(IPMI::Session *session, int requestId, int bitIndex);
	int FinishRequest(int requestId);
	void AutoAbortAll(IPMI::Session *session);
	void AutoDeleteAll(IPMI::Session *session);
	ServiceEntry *GetServiceEntry(void) const { return m_serviceEntry; }

private:
	void ChangeState(BlockingRequest *request, BlockingRequestState newState);

	BlockingServer *m_blockingServer;
	ServiceEntry *m_serviceEntry;
	SceUInt32 m_maxRequests;
	BlockingRequest *m_requestPool;
	BlockingRequest **m_requests;
	BlockingHandler *m_handler;
};

class EventFlagServer {
public:
	EventFlagServer();
	~EventFlagServer();

	int Init(ServiceServer *server);

private:
	ServiceServer *m_server;
};

struct EventFlagSessionConfig {
	SceUInt32 numEventFlags;
};

class EventFlagSession {
public:
	explicit EventFlagSession(EventFlagServer *server);
	~EventFlagSession();

	int Init(const char *serviceName, ServiceSession *session,
		 const EventFlagSessionConfig *config);
	int SetEventFlag(SceUInt32 evfIndex, SceUInt64 bitPattern);

private:
	EventFlagServer *m_server;
	SceUInt32 m_numEventFlags;
	NpIpc2EventFlagObj *m_efPool;
	NpIpc2EventFlagObj **m_efList;
};

class EventQueueServer {
public:
	EventQueueServer();
	~EventQueueServer();

	int Init(ServiceServer *server);

private:
	ServiceServer *m_server;
};

struct EventQueueEntrySpec {
	SceUInt32 itemCount;
	SceUInt32 itemSize;
};

struct EventQueueSessionConfig {
	SceUInt32 numQueues;
	EventQueueEntrySpec queues[NP_IPC2_MAX_EVENT_QUEUES];
};

struct EventQueueChannel {
	SceUInt32 reserved;
	SceUInt32 maxItems;
	SceUInt32 itemSize;
	NpIpc2RingBufAlloc ringBuf;
};

class EventQueueSession {
public:
	explicit EventQueueSession(EventQueueServer *server);
	~EventQueueSession();

	int Init(const char *serviceName, ServiceSession *session,
		 const EventQueueSessionConfig *config);
	int Dequeue(SceUInt32 queueIndex, void *outBuf, SceSize size);

private:
	EventQueueServer *m_server;
	SceUInt32 m_numQueues;
	EventQueueChannel *m_channelPool;
	EventQueueChannel **m_channels;
	NpIpc2EventFlagObj m_eventFlag;
	NpIpc2Mutex m_mutex;
};

/* Service registration descriptor (84 bytes, matches 0x815490B8 layout) */
typedef int (*ServiceSessionCallback)(IPMI::Session *session);
typedef int (*ServiceSyncDispatchFn)(IPMI::Session *session, SceUInt32 methodId,
				     const IPMI::DataInfo *inList, SceUInt32 inCount,
				     IPMI::BufferInfo *outList, SceUInt32 outCount);
typedef int (*ServiceAsyncDispatchFn)(IPMI::Session *session, SceUInt32 requestId,
				      SceUInt32 methodId, const IPMI::DataInfo *inList,
				      SceUInt32 inCount);
typedef int (*ServiceCreateRequestFn)(IPMI::Session *session, const void *optionBuf,
				      SceSize optionSize, int *outRequestId);
typedef int (*ServiceRequestOpFn)(IPMI::Session *session, int requestId);

struct ServiceDescriptor {
	SceUInt32 minMethodId;
	SceUInt32 maxMethodId;
	SceInt32 serviceType;
	char serviceName[16];
	ServiceSessionCallback onClientConnect;
	ServiceSessionCallback onClientDisconnect;
	ServiceSessionCallback onServiceOpen;
	ServiceSessionCallback onServiceClose;
	ServiceSyncDispatchFn onSyncMethod;
	ServiceAsyncDispatchFn onAsyncMethod;
	SceUInt32 numEventFlags;
	SceUInt32 eventQueueItemSize;
	SceUInt32 eventQueueItemCount;
	SceUInt32 maxBlockingRequests;
	ServiceCreateRequestFn onCreateRequest;
	ServiceRequestOpFn onDestroyRequest;
	ServiceRequestOpFn onAbortRequest;
	SceUInt32 cleanupFlags;
};

/* ServiceEntry wraps a registered ServiceDescriptor and exposes BlockingHandler */
class ServiceEntry : public BlockingHandler {
public:
	ServiceEntry();
	virtual ~ServiceEntry();

	int Init(ServiceServer *server, const ServiceDescriptor *desc);
	bool IsInit(void) const { return m_server != NULL; }
	ServiceServer *GetServer(void) const { return m_server; }
	SceInt32 GetServiceType(void) const;
	const char *GetServiceName(void) const;
	bool IsServiceIdRange(SceUInt32 methodId) const;
	bool AutoAbortRequest(void) const;
	bool AutoDeleteRequest(void) const;

	const ServiceDescriptor &GetDescriptor(void) const { return m_desc; }

	void OnClientConnect(IPMI::Session *session);
	void OnClientDisconnect(IPMI::Session *session);
	void OnServiceOpen(IPMI::Session *session);
	void OnServiceClose(IPMI::Session *session);
	int OnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount);
	int OnAsyncMethod(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount);

	virtual int CreateRequest(IPMI::Session *session, const void *optionBuf,
				  SceSize optionSize, int *outRequestId);
	virtual int DestroyRequest(IPMI::Session *session, int requestId);
	virtual int AbortRequest(IPMI::Session *session, int requestId);

private:
	ServiceServer *m_server;
	ServiceDescriptor m_desc;
};

/* Per-client, per-opened-service instance */
class ServiceBinding {
public:
	ServiceBinding();
	~ServiceBinding();

	int Init(ServiceEntry *entry, ServiceSession *session);
	bool IsInit(void) const { return m_entry != NULL; }
	ServiceEntry *GetEntry(void) const { return m_entry; }
	BlockingSession *GetBlockingSession(void) const { return m_blSvr; }
	EventQueueSession *GetEventQueueSession(void) const { return m_eqSvr; }
	EventFlagSession *GetEventFlagSession(void) const { return m_efSvr; }

private:
	ServiceEntry *m_entry;
	BlockingSession *m_blSvr;
	EventQueueSession *m_eqSvr;
	EventFlagSession *m_efSvr;
};

/* Base IPMI per-client session */
class IpmiSession {
public:
	IpmiSession();
	virtual ~IpmiSession();

	int Init(IpmiServer *server, const IPMI::Session::Config *sessionConfig);
	void Finalize(void);
	bool IsInit(void) const { return m_ipmiSession != NULL; }
	IPMI::Session *GetIpmiSession(void) const { return m_ipmiSession; }

	virtual void OnDisconnect(IPMI::Session *session) { (void)session; }

protected:
	IPMI::Session *m_ipmiSession;
	IpmiServer *m_ipmiServer;
	void *m_sessionWorkBuf;
};

/* Base IPMI server implementing IPMI::Server::EventHandler */
class IpmiServer : public IPMI::Server::EventHandler {
public:
	IpmiServer();
	virtual ~IpmiServer();

	int Init(const IpmiServerConfig *config);
	void Destroy(void);
	bool IsInit(void) const { return m_ipmiServer != NULL; }
	int StartDispatch(void);
	int StopDispatch(void);
	bool IsDispatching(void) const { return m_dispatchThread != NULL; }
	const char *GetServerName(void) const { return m_serverName; }
	NpIpc2Mutex *GetLock(void) const { return m_mutex; }
	IPMI::Server *GetIpmiServer(void) const { return m_ipmiServer; }

	IpmiSession *FindClient(IPMI::Session *session) const;
	int AddClient(IpmiSession *client);
	IpmiSession *RemoveClient(IPMI::Session *session);
	int DispatchLoop(void);

	/* Linked-list pointers for global server registry */
	IpmiServer *m_nextServer;
	IpmiServer *m_prevServer;

protected:
	IPMI::Server *m_ipmiServer;
	void *m_serverObjBuf;
	SceSize m_workBufSize;
	void *m_workBuf;
	NpIpc2Mutex *m_mutex;
	NpIpc2ThreadBase *m_dispatchThread;
	IpmiSession **m_clients;
	SceUInt32 m_clientCount;
	SceUInt32 m_maxClients;
	char m_serverName[16];
};

/* Per-client session on ServiceServer */
class ServiceSession : public IpmiSession {
public:
	ServiceSession();
	virtual ~ServiceSession();

	int Init(ServiceServer *server, const IPMI::Session::Config *sessionConfig,
		 const BlockingServerConfig *blockingConfig);
	ServiceBinding *FindService(SceInt32 serviceType) const;
	ServiceBinding *RemoveService(SceInt32 serviceType);
	ServiceBinding *FindServiceByMethodId(SceUInt32 methodId) const;
	int OpenService(SceInt32 serviceType, IPMI::Session *session);
	void CloseServiceBinding(ServiceBinding *binding, IPMI::Session *session);
	void CloseAllServices(IPMI::Session *session);

	int CreateRequest(SceInt32 serviceType, const void *optionBuf, SceSize optionSize,
			  char flags, int *outRequestId);
	int DestroyRequest(SceInt32 serviceType, IPMI::Session *session, int requestId,
			   char flags);
	int AbortRequest(SceInt32 serviceType, IPMI::Session *session, int requestId, char flags);
	int StartRequest(SceInt32 serviceType, int requestId, int bitIndex);
	int FinishRequest(ServiceBinding *binding, int requestId);
	int DequeueEvent(SceInt32 serviceType, void *outBuf, SceSize size);
	int SetEventFlag(SceInt32 serviceType, SceUInt32 evfIndex, SceUInt64 bitPattern);

	BlockingServer *GetBlockingServer(void) const { return m_blockingServer; }
	EventQueueServer *GetEventQueueServer(void) const { return m_eventQueueServer; }
	EventFlagServer *GetEventFlagServer(void) const { return m_eventFlagServer; }

	virtual void OnDisconnect(IPMI::Session *session);

private:
	ServiceServer *m_serviceServer;
	BlockingServer *m_blockingServer;
	EventQueueServer *m_eventQueueServer;
	EventFlagServer *m_eventFlagServer;
	ServiceBinding *m_bindings[NP_IPC2_MAX_SERVICES];
};

/* Top-level NpIpc2 ServiceServer */
class ServiceServer : public IpmiServer {
public:
	ServiceServer();
	virtual ~ServiceServer();

	int Init(const IpmiServerConfig *config);
	void Destroy(void);
	int RegisterService(const ServiceDescriptor *desc);
	int UnregisterService(SceInt32 serviceType);
	int RegisterServices(const ServiceDescriptor *descTable);

	int FinishRequest(IPMI::Session *session, SceInt32 serviceType, int requestId);
	int SetEventFlag(IPMI::Session *session, SceInt32 serviceType, SceUInt32 evfIndex,
			 SceUInt64 bitPattern);

	ServiceEntry *FindRegisteredService(SceInt32 serviceType) const;

	/* IPMI::Server::EventHandler overrides */
	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize);
	virtual void onSessionClosedByClient(IPMI::Session *pSession);
	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  void *pIn, SceSize inSize, SceUInt32 outCount,
					  void *pOut, SceSize outSize);
	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount);

private:
	void ClearServices(void);

	ServiceEntry *m_services[NP_IPC2_MAX_SERVICES];
	SceUInt32 m_serviceCount;
	SceUInt32 m_maxBlockingRequests;
};

} /* namespace Ipc2 */
} /* namespace Np */
} /* namespace sce */

/* Top-level C/C++ entry points (np_ipc2_server.cpp) */
extern int npServiceInit(void);
extern int npIpc2ServerStart(void);
extern int npIpc2ServerFinishRequest(IPMI::Session *session, SceInt32 serviceType, int requestId);
extern int npIpc2ServerSetEventFlag(IPMI::Session *session, SceUInt32 serviceId,
				    SceUInt32 evfIndex, SceUInt64 bitPattern);

/* Service handler callbacks implemented in sibling modules */
extern int npPushIpcOnClientOpen(IPMI::Session *session);
extern int npPushIpcOnClientClose(IPMI::Session *session);
extern int npPushIpcOnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
				 const IPMI::DataInfo *inList, SceUInt32 inCount,
				 IPMI::BufferInfo *outList, SceUInt32 outCount);
extern int npPushIpcOnAsyncMethod(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
				  const IPMI::DataInfo *inList, SceUInt32 inCount);

extern int npGamePresenceIpcOnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
					 const IPMI::DataInfo *inList, SceUInt32 inCount,
					 IPMI::BufferInfo *outList, SceUInt32 outCount);
extern int npGamePresenceIpcOnAsyncMethod(IPMI::Session *session, SceUInt32 requestId,
					  SceUInt32 methodId, const IPMI::DataInfo *inList,
					  SceUInt32 inCount);
extern int npGamePresenceIpcCreateRequest(IPMI::Session *session, const void *optionBuf,
					  SceSize optionSize, int *outRequestId);
extern int npGamePresenceIpcDestroyRequest(IPMI::Session *session, int requestId);
extern int npGamePresenceIpcAbortRequest(IPMI::Session *session, int requestId);

extern int npAsmVshIpcOnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
				   const IPMI::DataInfo *inList, SceUInt32 inCount,
				   IPMI::BufferInfo *outList, SceUInt32 outCount);
extern int npAsmVshIpcOnAsyncMethod(void);

#endif /* NP_IPC2_INTERNAL_H */
