/*
 * The NP IPC2 client Sony links into SceNpCommonPs4 (np_ipc2/client): a client of the NP service
 * over IPMI, with its requests, event flags and event queues. Sony's names are the method names
 * its asserts print and the member names in their conditions (Init, beginRequest, m_sema,
 * m_outstandingSlot, m_waitingIpcSlotHead, m_numEventFlag, m_numEventQueue, m_ipmiClient); the
 * class names are ours, from the file names and from what each class does.
 */
#ifndef NP_IPC2_CLIENT_H
#define NP_IPC2_CLIENT_H

#include "../../../np_common_ps4/np_common_ps4_internal.h"
#include <ipmi/client.h>


#define NP_IPC2_SOURCE(path) \
	"W:\\Build\\" NP_COMMON_PS4_BUILD "\\vsh\\app\\common\\src\\np\\np_ipc_ps4\\np_ipc2\\client\\" path

#define NP_IPC2_ASSERT(condition, function, file, line, text) \
	do { \
		if (!(condition)) \
			SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", function, \
				NP_IPC2_SOURCE(file), line, "ASSERT", text)); \
	} while (0)

/* a virtual function of an object: the table the object's first word points into */
#define NP_IPC2_VSLOT(object, index) ((*(const void *const *const *)(object))[index])

/* the errors of the NP IPC2 client (0x8055a3XX; Sony's names for them are not known) */
#define NP_IPC2_ERROR_ALREADY_INITIALIZED	-2141871360	/* 0x8055a300 */
#define NP_IPC2_ERROR_NOT_INITIALIZED		-2141871359	/* 0x8055a301 */
#define NP_IPC2_ERROR_OUT_OF_MEMORY		-2141871358	/* 0x8055a302 */
#define NP_IPC2_ERROR_8055A303			-2141871357
#define NP_IPC2_ERROR_8055A304			-2141871356
#define NP_IPC2_ERROR_8055A307			-2141871353
#define NP_IPC2_ERROR_8055A308			-2141871352
#define NP_IPC2_ERROR_8055A309			-2141871351
#define NP_IPC2_ERROR_8055A30A			-2141871350
#define NP_IPC2_ERROR_8055A30B			-2141871349
#define NP_IPC2_ERROR_8055A30C			-2141871348
#define NP_IPC2_ERROR_8055A30D			-2141871347
#define NP_IPC2_ERROR_TIMEOUT			-2141871346	/* 0x8055a30e */
#define NP_IPC2_ERROR_8055A311			-2141871343
#define NP_IPC2_ERROR_8055A312			-2141871342
#define NP_IPC2_ERROR_8055A313			-2141871341
#define NP_IPC2_ERROR_8055A314			-2141871340
#define NP_IPC2_ERROR_8055A315			-2141871339
#define NP_IPC2_ERROR_8055A319			-2141871335
#define NP_IPC2_ERROR_8055A31B			-2141871333

#define NP_IPC2_REQUEST_STATE_IDLE		0
#define NP_IPC2_REQUEST_STATE_READY		1
#define NP_IPC2_REQUEST_STATE_WAITING		2
#define NP_IPC2_REQUEST_STATE_PENDING		3
#define NP_IPC2_REQUEST_STATE_ABORTED		4

#define NP_IPC2_INVALID_IPC_SLOT		-1
#define NP_IPC2_INVALID_REQUEST_ID		-1

#define NP_IPC2_MAX_SERVICES			32

extern "C" {

/* the allocator, the heap and the pieces of the library's own classes the client uses */
struct NpCommonPs4Allocator;
struct NpCommonPs4EventFlag *SceNpCommonPs4_945AC0FC(struct NpCommonPs4EventFlag *ef);
struct NpCommonPs4EventFlag *SceNpCommonPs4_4C3E58CA(struct NpCommonPs4EventFlag *ef);
int SceNpCommonPs4_E7D29E99(struct NpCommonPs4EventFlag *ef);
int SceNpCommonPs4_BA957ABE(struct NpCommonPs4EventFlag *ef, const char *name, SceUInt32 attr);
int SceNpCommonPs4_D6F2A0EC(struct NpCommonPs4EventFlag *ef, int reserved0, SceUInt32 bitPattern, int reserved1,
	SceUInt32 waitMode, SceUInt64 *pResult);
struct NpCommonPs4SemaphoreObject *SceNpCommonPs4_90A8B2FA(struct NpCommonPs4SemaphoreObject *sema);
struct NpCommonPs4SemaphoreObject *SceNpCommonPs4_CD40FE5E(struct NpCommonPs4SemaphoreObject *sema);
int SceNpCommonPs4_6BE664A1(struct NpCommonPs4SemaphoreObject *sema);
int SceNpCommonPs4_47268A8C(struct NpCommonPs4SemaphoreObject *sema, int initCount, int maxCount, const char *name);
int SceNpCommonPs4_1D3964FF(struct NpCommonPs4SemaphoreObject *sema, SceUInt32 timeout);
int SceNpCommonPs4_B764816E(struct NpCommonPs4SemaphoreObject *sema);
int SceNpCommonPs4_A37CE123(void);

}

void *operator new(SceSize size, void *where);

/*
 * Every class below is written as Sony's compiler laid it out: a vtable pointer first (the table
 * is a NP_COMMON_PS4_VTABLE, so the first two words of the object are before the pointer), the
 * members after it, and the virtual functions called through the table.
 */

struct NpIpc2ServiceClient;
struct NpIpc2ClientBlocking;
struct NpIpc2ClientBlockingService;

/* np_ipc2_client_blocking.cpp: the context of one request, which waits on its own semaphore */
struct NpIpc2ClientBlockingRequest {
	int state;
	int ipcSlot;
	int reqId;
	struct NpIpc2ClientBlockingRequest *nextWaiting;
	struct NpIpc2ClientBlockingService *service;
	struct NpCommonPs4SemaphoreObject m_sema;
};

/* the IPC slots the client's requests wait for */
struct NpIpc2ClientBlocking {
	const void *const *vtable;
	struct NpIpc2ServiceClient *client;
	SceUInt32 m_numIpcSlot;
	SceUInt32 numOutstanding;
	struct NpIpc2ClientBlockingRequest **m_outstandingSlot;
	struct NpIpc2ClientBlockingRequest *m_waitingIpcSlotHead;
	struct NpCommonPs4Mutex mutex;
};

/* a service's requests: the base class sends them to the NP service, the service's own class implements the sends */
struct NpIpc2ClientBlockingService {
	const void *const *vtable;
	struct NpIpc2ClientBlocking *blocking;
	struct NpIpc2Service *service;
	SceUInt32 numRequest;
	void *requestMemory;
	struct NpIpc2ClientBlockingRequest **requests;
};

/* np_ipc2_client_eventflag.cpp, np_ipc2_client_eventqueue.cpp: what the event flags and the event queues share */
struct NpIpc2ClientManager {
	const void *const *vtable;
	struct NpIpc2ServiceClient *client;
};

struct NpIpc2ClientEventFlag {
	const void *const *vtable;
	struct NpIpc2ClientManager *manager;
	SceUInt32 m_numEventFlag;
	void *eventFlagMemory;
	struct NpCommonPs4EventFlag **eventFlags;
	SceUInt32 serviceId;
};

struct NpIpc2ClientEventQueueSlot {
	struct NpIpc2ClientEventQueue *queue;
	SceUInt32 itemSize;
	SceUInt32 attr;
	SceUInt8 needWait;
};

struct NpIpc2ClientEventQueue {
	const void *const *vtable;
	struct NpIpc2ClientManager *manager;
	SceUInt32 m_numEventQueue;
	void *queueMemory;
	struct NpIpc2ClientEventQueueSlot **queues;
	struct NpCommonPs4EventFlag eventFlag;
	SceUInt32 serviceId;
};

/* np_ipc2_ipmi_client.cpp: the IPMI client and its state */
struct NpIpc2IpmiClientConfig {
	char name[16];
	SceUInt32 syncCallCount;
	SceSize maxSyncCallInSize;
	SceSize maxSyncCallOutSize;
	SceUInt32 asyncCallCount;
	SceSize maxAsyncCallInSize;
	SceSize maxAsyncCallOutSize;
	SceUInt32 eventFlagCount;
};

struct NpIpc2IpmiClient {
	const void *const *vtable;
	IPMI::Client *m_ipmiClient;
	void *ipmiClientBuffer;
	int disconnected;
	char name[16];
};

/* np_ipc2_service_client.cpp */
struct NpIpc2ServiceConfig {
	int serviceId;
	char name[16];
	SceUInt32 eventFlagCount;
	SceUInt32 eventQueueItemSize;
	SceUInt32 eventQueueAttr;
	SceUInt32 requestCount;
};

struct NpIpc2Service {
	const void *const *vtable;
	struct NpIpc2ServiceClient *client;
	struct NpIpc2ServiceConfig config;
	struct NpIpc2ClientBlockingService *blockingService;
	struct NpIpc2ClientEventQueue *eventQueue;
	struct NpIpc2ClientEventFlag *eventFlag;
	SceUInt8 started;
};

/* the configuration of the client: the IPMI client's, and the number of IPC slots the requests share */
struct NpIpc2ServiceClientConfig {
	char name[16];
	SceUInt32 syncCallCount;
	SceSize maxSyncCallInSize;
	SceSize maxSyncCallOutSize;
	SceUInt32 asyncCallCount;
	SceSize maxAsyncCallInSize;
	SceSize maxAsyncCallOutSize;
	SceUInt32 numIpcSlot;
};

struct NpIpc2ServiceClient {
	struct NpIpc2IpmiClient ipmiClient;
	struct NpCommonPs4Mutex mutex;
	SceUInt32 numService;
	struct NpIpc2Service *services[NP_IPC2_MAX_SERVICES];
	struct NpIpc2ClientBlocking *blocking;
	struct NpIpc2ClientManager *eventQueueManager;
	struct NpIpc2ClientManager *eventFlagManager;
};

extern "C" {

extern const void *g_baseVtable[];

int SceNpCommonPs4_39CF37A2(struct NpCommonPs4Heap *heap, SceSize size, const char *name);
void *SceNpCommonPs4_C52C787A(struct NpCommonPs4Heap *heap);
void *SceNpCommonPs4_EDB4C46E(struct NpCommonPs4Heap *heap, struct NpCommonPs4Allocator *allocator);
void *SceNpCommonPs4_EB6E37A8(void **pMspace, SceSize size);
void SceNpCommonPs4_1514F370(void **pMspace, void *pointer);

/* np_ipc2_ipmi_client.cpp */
struct NpIpc2IpmiClient *npIpc2IpmiClientConstruct(struct NpIpc2IpmiClient *self);
int npIpc2IpmiClientIsInit(struct NpIpc2IpmiClient *self);
struct NpIpc2IpmiClient *npIpc2IpmiClientDestructBase(struct NpIpc2IpmiClient *self);
int npIpc2IpmiClientInit(struct NpIpc2IpmiClient *self, const struct NpIpc2IpmiClientConfig *config);
int npIpc2IpmiClientTerm(struct NpIpc2IpmiClient *self);
int npIpc2IpmiClientConnect(struct NpIpc2IpmiClient *self, const void *param, SceSize paramSize);
int npIpc2IpmiClientDisconnect(struct NpIpc2IpmiClient *self);
int npIpc2IpmiClientIsConnected(struct NpIpc2IpmiClient *self);
int npIpc2IpmiClientCall(struct NpIpc2IpmiClient *self, SceUInt32 methodId, const void *in, SceSize inSize,
	void *out, SceSize *pOutSize, SceSize outSize);
int npIpc2IpmiClientWaitEventFlag(struct NpIpc2IpmiClient *self, SceUInt32 index, SceUInt32 bitPattern,
	SceUInt32 waitMode, SceUInt32 *pResult, SceUInt32 *pTimeout);

/* np_ipc2_client_blocking.cpp */
struct NpIpc2ClientBlocking *npIpc2ClientBlockingConstruct(struct NpIpc2ClientBlocking *self,
	struct NpIpc2ServiceClient *client);
int npIpc2ClientBlockingInit(struct NpIpc2ClientBlocking *self, const void *config, const SceUInt32 *pNumIpcSlot);
struct NpIpc2ClientBlockingService *npIpc2ClientBlockingServiceConstruct(struct NpIpc2ClientBlockingService *self,
	struct NpIpc2ClientBlocking *blocking, struct NpIpc2Service *service);
struct NpIpc2ClientBlockingService *npIpc2ClientBlockingServiceDestructBase(struct NpIpc2ClientBlockingService *self);
int npIpc2ClientBlockingServiceInit(struct NpIpc2ClientBlockingService *self, const SceUInt32 *pNumRequest);
int npIpc2ClientBlockingServiceGetServiceId(struct NpIpc2ClientBlockingService *self);
int npIpc2ClientBlockingServiceCreateRequest(struct NpIpc2ClientBlockingService *self, int *pReqId, const void *data,
	SceSize size, unsigned char flag);
int npIpc2ClientBlockingServiceDeleteRequest(struct NpIpc2ClientBlockingService *self, int reqId, unsigned char flag);
int npIpc2ClientBlockingServiceAbortRequest(struct NpIpc2ClientBlockingService *self, int reqId, unsigned char flag);
int npIpc2ClientBlockingServiceBeginRequest(struct NpIpc2ClientBlockingService *self, int reqId, SceUInt32 *pIndex,
	char noSend);
int npIpc2ClientBlockingServiceWaitRequest(struct NpIpc2ClientBlockingService *self, int reqId, SceUInt32 timeout,
	int reserved);
int npIpc2ClientBlockingServiceEndRequest(struct NpIpc2ClientBlockingService *self, int reqId, int reserved);

/* np_ipc2_client_eventflag.cpp, np_ipc2_client_eventqueue.cpp */
struct NpIpc2ClientManager *npIpc2ClientEventFlagManagerConstruct(struct NpIpc2ClientManager *self,
	struct NpIpc2ServiceClient *client);
int npIpc2ClientEventFlagManagerInit(struct NpIpc2ClientManager *self, void *memory);
struct NpIpc2ClientEventFlag *npIpc2ClientEventFlagConstruct(struct NpIpc2ClientEventFlag *self,
	struct NpIpc2ClientManager *manager);
struct NpIpc2ClientEventFlag *npIpc2ClientEventFlagDestructBase(struct NpIpc2ClientEventFlag *self);
int npIpc2ClientEventFlagInit(struct NpIpc2ClientEventFlag *self, const char *name, const SceUInt32 *pNumEventFlag);
int npIpc2ClientEventFlagPoll(struct NpIpc2ClientEventFlag *self, SceUInt32 evfIndex, SceUInt32 bits, int reserved,
	SceUInt32 waitMode, SceUInt64 *pResult);
struct NpIpc2ClientManager *npIpc2ClientEventQueueManagerConstruct(struct NpIpc2ClientManager *self,
	struct NpIpc2ServiceClient *client);
int npIpc2ClientEventQueueManagerInit(struct NpIpc2ClientManager *self, void *memory);
struct NpIpc2ClientEventQueue *npIpc2ClientEventQueueConstruct(struct NpIpc2ClientEventQueue *self,
	struct NpIpc2ClientManager *manager);
struct NpIpc2ClientEventQueue *npIpc2ClientEventQueueDestructBase(struct NpIpc2ClientEventQueue *self);
int npIpc2ClientEventQueueInit(struct NpIpc2ClientEventQueue *self, const char *name, const SceUInt32 *config);
int npIpc2ClientEventQueuePoll(struct NpIpc2ClientEventQueue *self, SceUInt32 queueIndex, void *buffer, SceSize size);

/* np_ipc2_service_client.cpp: the allocator of the client's heap and its memory, the services */
struct NpCommonPs4Allocator *npIpc2ClientGetAllocator(void);
void *npIpc2ClientAlloc(SceSize size);
void npIpc2ClientFree(void *pointer);
int npIpc2ServiceGetId(struct NpIpc2Service *service);
const char *npIpc2ServiceGetName(struct NpIpc2Service *service);
int npIpc2ClientHeapInit(SceSize size);
int npIpc2ClientHeapTerm(void);
int npIpc2ClientInit(void);
int npIpc2ClientTerm(void);
struct NpIpc2ServiceClient *npIpc2ServiceClientConstruct(struct NpIpc2ServiceClient *self);
int npIpc2ServiceClientInit(struct NpIpc2ServiceClient *self, const struct NpIpc2ServiceClientConfig *config);
void npIpc2ServiceClientTerm(struct NpIpc2ServiceClient *self);
int npIpc2ServiceClientConnect(struct NpIpc2ServiceClient *self, const void *param, SceSize paramSize);
int npIpc2ServiceClientDisconnect(struct NpIpc2ServiceClient *self);
int npIpc2ServiceClientAddServices(struct NpIpc2ServiceClient *self, const struct NpIpc2ServiceConfig *configs);
int npIpc2ServiceClientStartService(struct NpIpc2ServiceClient *self, int serviceId);
int npIpc2ServiceClientStopService(struct NpIpc2ServiceClient *self, int serviceId);
int npIpc2ServiceClientIsStarted(struct NpIpc2ServiceClient *self, int serviceId);
int npIpc2ServiceClientCreateRequest(struct NpIpc2ServiceClient *self, int *pReqId, int serviceId, const void *data,
	SceSize size);
int npIpc2ServiceClientDeleteRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId);
int npIpc2ServiceClientAbortRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId);
int npIpc2ServiceClientBeginRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId);
int npIpc2ServiceClientEndRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId);
int npIpc2ServiceClientWaitRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId, SceUInt32 timeout);
int npIpc2ServiceClientPollEventQueue(struct NpIpc2ServiceClient *self, int serviceId, void *buffer, SceSize size);
int npIpc2ServiceClientPollEventFlag(struct NpIpc2ServiceClient *self, int serviceId, SceUInt32 evfIndex, int reserved,
	SceUInt32 bits, int reserved1, SceUInt32 waitMode, SceUInt64 *pResult);

}

#endif
