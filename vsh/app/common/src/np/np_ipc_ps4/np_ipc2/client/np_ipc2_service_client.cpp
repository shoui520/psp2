/*
 * The NP IPC2 client: the client's heap, the services it talks to (each with its blocking
 * requests, its event queue and its event flags) and the requests the services send over the
 * IPMI connection.
 */

#include "np_ipc2_client.h"

extern "C" {


extern const void *g_npIpc2ServiceBlockingImplVtable[];		/* 0x8102acf8 */
extern const void *g_npIpc2ServiceEventQueueImplVtable[];	/* 0x8102ad18 */
extern const void *g_npIpc2ServiceEventFlagImplVtable[];	/* 0x8102ad2c */
extern const void *g_npIpc2ServiceVtable[];			/* 0x8102ad3c */
extern const void *g_npIpc2ServiceClientVtable[];		/* 0x8102ad4c */

struct NpCommonPs4Heap g_npIpc2ClientHeap;			/* 0x8102ba20 */
struct NpCommonPs4Allocator g_npIpc2ClientAllocator;		/* 0x8102ba10 */
SceUInt8 g_npIpc2ClientInitialized;				/* 0x8102b128 */

/* the arguments of the NP service's methods */
struct NpIpc2CreateRequestArgs {
	SceUInt32 serviceId;
	int flag;
	char data[128];
	SceSize size;
};

struct NpIpc2RequestArgs {
	SceUInt32 serviceId;
	int arg1;
	int arg2;
};

struct NpIpc2ReceiveArgs {
	SceUInt32 serviceId;
	SceSize size;
};

/* the methods of the NP service, by the ID the IPMI call names */
#define NP_IPC2_METHOD_START_SERVICE		0
#define NP_IPC2_METHOD_STOP_SERVICE		1
#define NP_IPC2_METHOD_CREATE_REQUEST		2
#define NP_IPC2_METHOD_BEGIN_REQUEST		3
#define NP_IPC2_METHOD_DELETE_REQUEST		4
#define NP_IPC2_METHOD_ABORT_REQUEST		5
#define NP_IPC2_METHOD_RECEIVE			6

int npIpc2ClientInit(void)
{
	int result;

	if (g_npIpc2ClientInitialized != 0)
		return NP_IPC2_ERROR_ALREADY_INITIALIZED;
	result = npIpc2ClientHeapInit(0x10000);
	if (result >= 0) {
		g_npIpc2ClientInitialized = 1;
		return 0;
	}
	return result;
}

int npIpc2ClientTerm(void)
{
	if (g_npIpc2ClientInitialized == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	npIpc2ClientHeapTerm();
	g_npIpc2ClientInitialized = 0;
	return 0;
}

__attribute__((noinline)) int npIpc2ClientHeapInit(SceSize size)
{
	int result;

	result = SceNpCommonPs4_39CF37A2(&g_npIpc2ClientHeap, size, "SceNpIpc2ClientHeap");
	if (result >= 0)
		return 0;
	return result;
}

__attribute__((noinline)) int npIpc2ClientHeapTerm(void)
{
	return (int)SceNpCommonPs4_C52C787A(&g_npIpc2ClientHeap);
}

__attribute__((noinline)) struct NpCommonPs4Allocator *npIpc2ClientGetAllocator(void)
{
	SceNpCommonPs4_EDB4C46E(&g_npIpc2ClientHeap, &g_npIpc2ClientAllocator);
	return &g_npIpc2ClientAllocator;
}

void *npIpc2ClientAlloc(SceSize size)
{
	return SceNpCommonPs4_EB6E37A8(&g_npIpc2ClientHeap.mspace, size);
}

void npIpc2ClientFree(void *pointer)
{
	SceNpCommonPs4_1514F370(&g_npIpc2ClientHeap.mspace, pointer);
}

struct NpIpc2ServiceClient *npIpc2ServiceClientConstruct(struct NpIpc2ServiceClient *self)
{
	npIpc2IpmiClientConstruct(&self->ipmiClient);
	self->ipmiClient.vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceClientVtable);
	SceNpCommonPs4_4266D870(&self->mutex);
	self->numService = 0;
	self->blocking = NULL;
	self->eventQueueManager = NULL;
	self->eventFlagManager = NULL;
	sceClibMemset(self->services, 0, sizeof(self->services));
	return self;
}

/* the services, and the objects the client made for them */
__attribute__((noinline)) void npIpc2ServiceClientRelease(struct NpIpc2ServiceClient *self)
{
	struct NpIpc2Service **services;
	SceUInt32 i;
	SceUInt32 count;

	count = self->numService;
	i = 0;
	if (count != 0) {
		services = self->services;
		do {
			if (*services != NULL) {
				((void (*)(void *))NP_IPC2_VSLOT(*services, 1))(*services);
				count = self->numService;
			}
			i++;
			services++;
		} while (i < count);
	}
	self->numService = 0;
	sceClibMemset(self->services, 0, sizeof(self->services));
	if (self->blocking != NULL)
		((void (*)(void *))NP_IPC2_VSLOT(self->blocking, 1))(self->blocking);
	if (self->eventQueueManager != NULL)
		((void (*)(void *))NP_IPC2_VSLOT(self->eventQueueManager, 1))(self->eventQueueManager);
	if (self->eventFlagManager != NULL)
		((void (*)(void *))NP_IPC2_VSLOT(self->eventFlagManager, 1))(self->eventFlagManager);
}

struct NpIpc2ServiceClient *npIpc2ServiceClientDestruct(struct NpIpc2ServiceClient *self)
{
	self->ipmiClient.vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceClientVtable);
	npIpc2ServiceClientRelease(self);
	SceNpCommonPs4_294F0B1A(&self->mutex);
	npIpc2IpmiClientDestructBase(&self->ipmiClient);
	return self;
}

int npIpc2ServiceClientDelete(struct NpIpc2ServiceClient *self)
{
	self->ipmiClient.vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceClientVtable);
	npIpc2ServiceClientRelease(self);
	SceNpCommonPs4_294F0B1A(&self->mutex);
	npIpc2IpmiClientDestructBase(&self->ipmiClient);
	return SceNpCommonPs4_BB5282C8(self);
}

/* the event flags the IPMI client needs for this many IPC slots: one for every 64 of them */
__attribute__((noinline)) int npIpc2ClientGetEventFlagCount(const SceUInt32 *pNumIpcSlot)
{
	return (*pNumIpcSlot >> 6) + 1;
}

int npIpc2ServiceClientInit(struct NpIpc2ServiceClient *self, const struct NpIpc2ServiceClientConfig *config)
{
	static const char functionName[] = "Init";
	struct NpIpc2IpmiClientConfig ipmiConfig;
	SceUInt32 numIpcSlot;
	SceUInt32 numIpcSlot2;
	void *memory;
	void *object;
	int result;
	int error;

	if (npIpc2IpmiClientIsInit(&self->ipmiClient) != 0)
		NP_IPC2_ASSERT(0, functionName, "np_ipc2_service_client.cpp", 638, "!IsInit()");
	ipmiConfig.name[0] = '\0';
	ipmiConfig.syncCallCount = 1;
	ipmiConfig.maxSyncCallInSize = 5120;
	ipmiConfig.maxSyncCallOutSize = 5120;
	ipmiConfig.asyncCallCount = 1;
	ipmiConfig.maxAsyncCallInSize = 5120;
	ipmiConfig.maxAsyncCallOutSize = 5120;
	ipmiConfig.eventFlagCount = 0;
	sceClibStrlcpy(ipmiConfig.name, config->name, 16);
	ipmiConfig.syncCallCount = config->syncCallCount;
	ipmiConfig.maxSyncCallInSize = config->maxSyncCallInSize;
	ipmiConfig.maxSyncCallOutSize = config->maxSyncCallOutSize;
	ipmiConfig.asyncCallCount = config->asyncCallCount;
	ipmiConfig.maxAsyncCallInSize = config->maxAsyncCallInSize;
	ipmiConfig.maxAsyncCallOutSize = config->maxAsyncCallOutSize;
	ipmiConfig.eventFlagCount = 0;
	if (config->numIpcSlot != 0) {
		numIpcSlot = config->numIpcSlot;
		ipmiConfig.eventFlagCount = npIpc2ClientGetEventFlagCount(&numIpcSlot);
	}
	result = npIpc2IpmiClientInit(&self->ipmiClient, &ipmiConfig);
	if (result < 0)
		return result;
	error = SceNpCommonPs4_8AA7E995(&self->mutex, config->name, 0);
	if (error >= 0) {
		if (config->numIpcSlot != 0) {
			numIpcSlot2 = config->numIpcSlot;
			memory = SceNpCommonPs4_0C922423(36, npIpc2ClientGetAllocator());
			object = NULL;
			if (memory != NULL)
				object = npIpc2ClientBlockingConstruct((struct NpIpc2ClientBlocking *)memory, self);
			self->blocking = (struct NpIpc2ClientBlocking *)object;
			if (object == NULL) {
				error = NP_IPC2_ERROR_OUT_OF_MEMORY;
				goto failed;
			}
			error = npIpc2ClientBlockingInit(self->blocking, config, &numIpcSlot2);
			if (error < 0)
				goto failed;
		}
		memory = SceNpCommonPs4_0C922423(8, npIpc2ClientGetAllocator());
		object = NULL;
		if (memory != NULL)
			object = npIpc2ClientEventQueueManagerConstruct((struct NpIpc2ClientManager *)memory, self);
		self->eventQueueManager = (struct NpIpc2ClientManager *)object;
		if (object != NULL) {
			error = npIpc2ClientEventQueueManagerInit(self->eventQueueManager, memory);
			if (error >= 0) {
				memory = SceNpCommonPs4_0C922423(8, npIpc2ClientGetAllocator());
				object = NULL;
				if (memory != NULL)
					object = npIpc2ClientEventFlagManagerConstruct((struct NpIpc2ClientManager *)memory,
						self);
				self->eventFlagManager = (struct NpIpc2ClientManager *)object;
				if (object != NULL) {
					error = npIpc2ClientEventFlagManagerInit(self->eventFlagManager, memory);
					if (error >= 0)
						return 0;
				} else {
					error = NP_IPC2_ERROR_OUT_OF_MEMORY;
				}
			}
		} else {
			error = NP_IPC2_ERROR_OUT_OF_MEMORY;
		}
	}
failed:
	if (self->eventFlagManager != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->eventFlagManager, 1))(self->eventFlagManager);
		self->eventFlagManager = NULL;
	}
	if (self->eventQueueManager != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->eventQueueManager, 1))(self->eventQueueManager);
		self->eventQueueManager = NULL;
	}
	if (self->blocking != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->blocking, 1))(self->blocking);
		self->blocking = NULL;
	}
	if (SceNpCommonPs4_C78F5DF5(&self->mutex) != 0)
		SceNpCommonPs4_63EED478(&self->mutex);
	npIpc2IpmiClientTerm(&self->ipmiClient);
	return error;
}

void npIpc2ServiceClientTerm(struct NpIpc2ServiceClient *self)
{
	npIpc2ServiceClientRelease(self);
	SceNpCommonPs4_63EED478(&self->mutex);
	npIpc2IpmiClientTerm(&self->ipmiClient);
	self->numService = 0;
	sceClibMemset(self->services, 0, sizeof(self->services));
	self->blocking = NULL;
	self->eventQueueManager = NULL;
	self->eventFlagManager = NULL;
}

int npIpc2ServiceClientConnect(struct NpIpc2ServiceClient *self, const void *param, SceSize paramSize)
{
	int result;

	result = npIpc2IpmiClientConnect(&self->ipmiClient, param, paramSize);
	if (result >= 0)
		return 0;
	return result;
}

int npIpc2ServiceClientDisconnect(struct NpIpc2ServiceClient *self)
{
	int result;

	result = npIpc2IpmiClientDisconnect(&self->ipmiClient);
	if (result >= 0)
		return 0;
	return result;
}

int npIpc2ServiceGetId(struct NpIpc2Service *service)
{
	return service->config.serviceId;
}

__attribute__((noinline)) int npIpc2ServiceClientAddService(struct NpIpc2ServiceClient *self, const struct NpIpc2ServiceConfig *config)
{
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;

	count = self->numService;
	for (i = 0; i < count; i++) {
		service = self->services[i];
		if (service->config.serviceId == config->serviceId)
			if (service != NULL)
				return NP_IPC2_ERROR_8055A312;
	}
	if (count >= NP_IPC2_MAX_SERVICES)
		return NP_IPC2_ERROR_8055A315;
	service = (struct NpIpc2Service *)SceNpCommonPs4_0C922423(sizeof(*service), npIpc2ClientGetAllocator());
	if (service == NULL)
		return NP_IPC2_ERROR_OUT_OF_MEMORY;
	service->client = self;
	service->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceVtable);
	service->config = *config;
	service->blockingService = NULL;
	service->eventQueue = NULL;
	service->eventFlag = NULL;
	service->started = 0;
	self->services[self->numService] = service;
	self->numService++;
	return 0;
}

__attribute__((noinline)) int npIpc2ServiceClientRemoveService(struct NpIpc2ServiceClient *self, int serviceId)
{
	struct NpIpc2Service **slot;
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;

	count = self->numService;
	i = 0;
	if (count == 0)
		return NP_IPC2_ERROR_8055A313;
	for (slot = self->services;; slot++) {
		service = *slot;
		if (service->config.serviceId == serviceId)
			break;
		if (++i >= count)
			return NP_IPC2_ERROR_8055A313;
	}
	if (service != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(service, 1))(service);
		count = self->numService;
	}
	if (count - i > 1) {
		sceClibMemmove(slot, &self->services[i + 1], 4 * (count - i) - 4);
		count = self->numService;
	}
	self->numService = count - 1;
	self->services[count - 1] = NULL;
	return 0;
}

int npIpc2ServiceClientAddServices(struct NpIpc2ServiceClient *self, const struct NpIpc2ServiceConfig *configs)
{
	const int *config;
	const int *next;
	int added;
	int result;
	int serviceId;

	next = (const int *)configs;
	added = 0;
	if (*next == -1)
		return 0;
	for (config = next;; config += 9) {
		result = npIpc2ServiceClientAddService(self, (const struct NpIpc2ServiceConfig *)config);
		if (result < 0)
			break;
		serviceId = config[9];
		added++;
		if (serviceId == -1)
			return 0;
	}
	if (added > 0) {
		do {
			serviceId = *next;
			next += 9;
			npIpc2ServiceClientRemoveService(self, serviceId);
			added--;
		} while (added != 0);
	}
	return result;
}

__attribute__((noinline)) int npIpc2ServiceStart(struct NpIpc2Service *self)
{
	SceUInt32 numRequest;
	SceUInt32 numEventFlag;
	SceUInt32 queueConfig[65];
	struct NpIpc2ClientBlockingService *blockingService;
	struct NpIpc2ClientEventQueue *eventQueue;
	struct NpIpc2ClientEventFlag *eventFlag;
	void *memory;
	int result;

	if (self->started != 0)
		return NP_IPC2_ERROR_8055A319;
	if (self->config.requestCount != 0) {
		numRequest = self->config.requestCount;
		blockingService = NULL;
		memory = SceNpCommonPs4_0C922423(24, npIpc2ClientGetAllocator());
		if (memory != NULL) {
			npIpc2ClientBlockingServiceConstruct((struct NpIpc2ClientBlockingService *)memory,
				self->client->blocking, self);
			((struct NpIpc2ClientBlockingService *)memory)->vtable =
				NP_COMMON_PS4_VTABLE(g_npIpc2ServiceBlockingImplVtable);
			blockingService = (struct NpIpc2ClientBlockingService *)memory;
		}
		self->blockingService = blockingService;
		if (blockingService == NULL) {
			result = NP_IPC2_ERROR_OUT_OF_MEMORY;
			goto failed;
		}
		result = npIpc2ClientBlockingServiceInit(blockingService, &numRequest);
		if (result < 0)
			goto failed;
	}
	if (self->config.eventQueueItemSize != 0 && self->config.eventQueueAttr != 0) {
		queueConfig[0] = 1;
		queueConfig[1] = self->config.eventQueueItemSize;
		queueConfig[2] = self->config.eventQueueAttr;
		eventQueue = NULL;
		memory = SceNpCommonPs4_0C922423(36, npIpc2ClientGetAllocator());
		if (memory != NULL) {
			npIpc2ClientEventQueueConstruct((struct NpIpc2ClientEventQueue *)memory,
				self->client->eventQueueManager);
			((struct NpIpc2ClientEventQueue *)memory)->serviceId = self->config.serviceId;
			((struct NpIpc2ClientEventQueue *)memory)->vtable =
				NP_COMMON_PS4_VTABLE(g_npIpc2ServiceEventQueueImplVtable);
			eventQueue = (struct NpIpc2ClientEventQueue *)memory;
		}
		self->eventQueue = eventQueue;
		if (eventQueue == NULL) {
			result = NP_IPC2_ERROR_OUT_OF_MEMORY;
			goto failed;
		}
		result = npIpc2ClientEventQueueInit(eventQueue, self->config.name, queueConfig);
		if (result < 0)
			goto failed;
	}
	if (self->config.eventFlagCount != 0) {
		numEventFlag = self->config.eventFlagCount;
		eventFlag = NULL;
		memory = SceNpCommonPs4_0C922423(24, npIpc2ClientGetAllocator());
		if (memory != NULL) {
			npIpc2ClientEventFlagConstruct((struct NpIpc2ClientEventFlag *)memory,
				self->client->eventFlagManager);
			((struct NpIpc2ClientEventFlag *)memory)->serviceId = self->config.serviceId;
			((struct NpIpc2ClientEventFlag *)memory)->vtable =
				NP_COMMON_PS4_VTABLE(g_npIpc2ServiceEventFlagImplVtable);
			eventFlag = (struct NpIpc2ClientEventFlag *)memory;
		}
		self->eventFlag = eventFlag;
		if (eventFlag == NULL) {
			result = NP_IPC2_ERROR_OUT_OF_MEMORY;
			goto failed;
		}
		result = npIpc2ClientEventFlagInit(eventFlag, self->config.name, &numEventFlag);
		if (result < 0)
			goto failed;
	}
	self->started = 1;
	return 0;
failed:
	if (self->blockingService != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->blockingService, 1))(self->blockingService);
		self->blockingService = NULL;
	}
	if (self->eventQueue != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->eventQueue, 1))(self->eventQueue);
		self->eventQueue = NULL;
	}
	if (self->eventFlag != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->eventFlag, 1))(self->eventFlag);
		self->eventFlag = NULL;
	}
	return result;
}

/* the service of this ID, or NULL */
#define NP_IPC2_FIND_SERVICE(client, id, found, i, count) \
	do { \
		(found) = NULL; \
		for ((i) = 0; (i) < (count); (i)++) { \
			if ((client)->services[i]->config.serviceId == (id)) { \
				(found) = (client)->services[i]; \
				break; \
			} \
		} \
	} while (0)

int npIpc2ServiceClientStartService(struct NpIpc2ServiceClient *self, int serviceId)
{
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int args;
	int result;

	if (npIpc2IpmiClientIsInit(&self->ipmiClient) == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	SceNpCommonPs4_A2A8493B(&self->mutex);
	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	if (service == NULL) {
		SceNpCommonPs4_6BA5A164(&self->mutex);
		return NP_IPC2_ERROR_8055A313;
	}
	result = npIpc2ServiceStart(service);
	if (result >= 0) {
		args = service->config.serviceId;
		result = npIpc2IpmiClientCall(&self->ipmiClient, NP_IPC2_METHOD_START_SERVICE, &args, 4, NULL, NULL, 0);
		if (result >= 0)
			result = 0;
	}
	SceNpCommonPs4_6BA5A164(&self->mutex);
	return result;
}

__attribute__((noinline)) int npIpc2ServiceStop(struct NpIpc2Service *self)
{
	if (self->eventFlag != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->eventFlag, 1))(self->eventFlag);
		self->eventFlag = NULL;
	}
	if (self->eventQueue != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->eventQueue, 1))(self->eventQueue);
		self->eventQueue = NULL;
	}
	if (self->blockingService != NULL) {
		((void (*)(void *))NP_IPC2_VSLOT(self->blockingService, 1))(self->blockingService);
		self->blockingService = NULL;
	}
	self->started = 0;
	return 0;
}

int npIpc2ServiceClientStopService(struct NpIpc2ServiceClient *self, int serviceId)
{
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int args;
	int result;

	if (npIpc2IpmiClientIsInit(&self->ipmiClient) == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	SceNpCommonPs4_A2A8493B(&self->mutex);
	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	if (service == NULL) {
		SceNpCommonPs4_6BA5A164(&self->mutex);
		return NP_IPC2_ERROR_8055A313;
	}
	args = serviceId;
	result = npIpc2IpmiClientCall(&self->ipmiClient, NP_IPC2_METHOD_STOP_SERVICE, &args, 4, NULL, NULL, 0);
	if (result >= 0) {
		npIpc2ServiceStop(service);
		SceNpCommonPs4_6BA5A164(&self->mutex);
		return 0;
	}
	SceNpCommonPs4_6BA5A164(&self->mutex);
	return result;
}

int npIpc2ServiceClientIsStarted(struct NpIpc2ServiceClient *self, int serviceId)
{
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;

	SceNpCommonPs4_A2A8493B(&self->mutex);
	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	if (service != NULL && service->started != 0) {
		SceNpCommonPs4_6BA5A164(&self->mutex);
		return 1;
	}
	SceNpCommonPs4_6BA5A164(&self->mutex);
	return 0;
}

/* the requests of a started service: the service must be one the client made */
int npIpc2ServiceClientCreateRequest(struct NpIpc2ServiceClient *self, int *pReqId, int serviceId, const void *data,
	SceSize size)
{
	static const char functionName[] = "CreateRequest";
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int result;

	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	NP_IPC2_ASSERT(service != NULL, functionName, "np_ipc2_service_client.cpp", 1073, "service != 0");
	if (service->started == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	result = npIpc2ClientBlockingServiceCreateRequest(service->blockingService, pReqId, data, size, 0);
	if (result < 0)
		return result;
	return 0;
}

int npIpc2ServiceClientDeleteRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId)
{
	static const char functionName[] = "DeleteRequest";
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int result;

	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	NP_IPC2_ASSERT(service != NULL, functionName, "np_ipc2_service_client.cpp", 1082, "service != 0");
	if (service->started == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	result = npIpc2ClientBlockingServiceDeleteRequest(service->blockingService, reqId, 0);
	if (result < 0)
		return result;
	return 0;
}

int npIpc2ServiceClientAbortRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId)
{
	static const char functionName[] = "AbortRequest";
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int result;

	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	NP_IPC2_ASSERT(service != NULL, functionName, "np_ipc2_service_client.cpp", 1091, "service != 0");
	if (service->started == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	result = npIpc2ClientBlockingServiceAbortRequest(service->blockingService, reqId, 0);
	if (result < 0)
		return result;
	return 0;
}

int npIpc2ServiceClientBeginRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId)
{
	static const char functionName[] = "BeginRequest";
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int result;

	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	NP_IPC2_ASSERT(service != NULL, functionName, "np_ipc2_service_client.cpp", 1100, "service != 0");
	if (service->started == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	result = npIpc2ClientBlockingServiceBeginRequest(service->blockingService, reqId, NULL, 0);
	if (result < 0)
		return result;
	return 0;
}

int npIpc2ServiceClientEndRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId)
{
	static const char functionName[] = "EndRequest";
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int result;

	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	NP_IPC2_ASSERT(service != NULL, functionName, "np_ipc2_service_client.cpp", 1109, "service != 0");
	if (service->started == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	result = npIpc2ClientBlockingServiceEndRequest(service->blockingService, reqId, 0);
	if (result < 0)
		return result;
	return 0;
}

int npIpc2ServiceClientWaitRequest(struct NpIpc2ServiceClient *self, int serviceId, int reqId, SceUInt32 timeout)
{
	static const char functionName[] = "WaitRequest";
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int result;

	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	NP_IPC2_ASSERT(service != NULL, functionName, "np_ipc2_service_client.cpp", 1118, "service != 0");
	if (service->started == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	result = npIpc2ClientBlockingServiceWaitRequest(service->blockingService, reqId, timeout, 0);
	if (result == NP_IPC2_ERROR_TIMEOUT)
		return result;
	if (result >= 0)
		return 0;
	return result;
}

int npIpc2ServiceClientPollEventQueue(struct NpIpc2ServiceClient *self, int serviceId, void *buffer, SceSize size)
{
	static const char functionName[] = "PollEventQueue";
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int result;

	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	NP_IPC2_ASSERT(service != NULL, functionName, "np_ipc2_service_client.cpp", 1183, "service != 0");
	if (service->started == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	result = npIpc2ClientEventQueuePoll(service->eventQueue, 0, buffer, size);
	if (result < 0)
		return result;
	return 0;
}

int npIpc2ServiceClientPollEventFlag(struct NpIpc2ServiceClient *self, int serviceId, SceUInt32 evfIndex, int reserved,
	SceUInt32 bits, int reserved1, SceUInt32 waitMode, SceUInt64 *pResult)
{
	static const char functionName[] = "PollEventFlag";
	struct NpIpc2Service *service;
	SceUInt32 count;
	SceUInt32 i;
	int result;

	count = self->numService;
	NP_IPC2_FIND_SERVICE(self, serviceId, service, i, count);
	NP_IPC2_ASSERT(service != NULL, functionName, "np_ipc2_service_client.cpp", 1195, "service != 0");
	if (service->started == 0)
		return NP_IPC2_ERROR_NOT_INITIALIZED;
	result = npIpc2ClientEventFlagPoll(service->eventFlag, evfIndex, bits, reserved1, waitMode, pResult);
	if (result == NP_IPC2_ERROR_8055A31B)
		return result;
	if (result >= 0)
		return 0;
	return result;
}

/* the sends of a service's blocking requests */
int npIpc2ServiceBlockingSendCreateRequest(struct NpIpc2ClientBlockingService *self, const void *data, SceSize size,
	int *pReqId, int flag)
{
	struct NpIpc2CreateRequestArgs args;
	int result;

	if (size <= sizeof(args.data)) {
		args.serviceId = npIpc2ClientBlockingServiceGetServiceId(self);
		sceClibMemcpy_safe(args.data, data, size);
		args.size = size;
		args.flag = flag;
		result = npIpc2IpmiClientCall(&self->blocking->client->ipmiClient, NP_IPC2_METHOD_CREATE_REQUEST, &args,
			sizeof(args), NULL, NULL, 0);
		if (result >= 0) {
			*pReqId = result;
			result = 0;
		}
	} else {
		result = NP_IPC2_ERROR_8055A314;
	}
	return result;
}

int npIpc2ServiceBlockingSendDeleteRequest(struct NpIpc2ClientBlockingService *self, int reqId, int flag)
{
	struct NpIpc2RequestArgs args;
	int result;

	args.serviceId = npIpc2ClientBlockingServiceGetServiceId(self);
	args.arg2 = reqId;
	args.arg1 = flag;
	result = npIpc2IpmiClientCall(&self->blocking->client->ipmiClient, NP_IPC2_METHOD_DELETE_REQUEST, &args,
		sizeof(args), NULL, NULL, 0);
	if (result >= 0)
		result = 0;
	return result;
}

int npIpc2ServiceBlockingSendAbortRequest(struct NpIpc2ClientBlockingService *self, int reqId, int flag)
{
	struct NpIpc2RequestArgs args;
	int result;

	args.serviceId = npIpc2ClientBlockingServiceGetServiceId(self);
	args.arg2 = reqId;
	args.arg1 = flag;
	result = npIpc2IpmiClientCall(&self->blocking->client->ipmiClient, NP_IPC2_METHOD_ABORT_REQUEST, &args,
		sizeof(args), NULL, NULL, 0);
	if (result >= 0)
		result = 0;
	return result;
}

int npIpc2ServiceBlockingSendBeginRequest(struct NpIpc2ClientBlockingService *self, int reqId, int ipcSlot)
{
	struct NpIpc2RequestArgs args;
	int result;

	args.serviceId = npIpc2ClientBlockingServiceGetServiceId(self);
	args.arg1 = reqId;
	args.arg2 = ipcSlot;
	result = npIpc2IpmiClientCall(&self->blocking->client->ipmiClient, NP_IPC2_METHOD_BEGIN_REQUEST, &args,
		sizeof(args), NULL, NULL, 0);
	if (result >= 0)
		result = 0;
	return result;
}

int npIpc2ServiceEventQueueReceive(struct NpIpc2ClientEventQueue *self, int queueIndex, void *buffer, SceSize size)
{
	struct NpIpc2ReceiveArgs args;
	SceSize received;
	int result;

	args.serviceId = self->serviceId;
	args.size = size;
	result = npIpc2IpmiClientCall(&self->manager->client->ipmiClient, NP_IPC2_METHOD_RECEIVE, &args,
		sizeof(args), buffer, &received, size);
	if (result == NP_IPC2_ERROR_8055A311)
		return result;
	if (result >= 0) {
		if (received == size)
			result = 0;
		else
			result = NP_IPC2_ERROR_8055A303;
	}
	return result;
}

__attribute__((noinline)) struct NpIpc2Service *npIpc2ServiceDestruct(struct NpIpc2Service *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceVtable);
	if (self->eventFlag != NULL)
		((void (*)(void *))NP_IPC2_VSLOT(self->eventFlag, 1))(self->eventFlag);
	if (self->eventQueue != NULL)
		((void (*)(void *))NP_IPC2_VSLOT(self->eventQueue, 1))(self->eventQueue);
	if (self->blockingService != NULL)
		((void (*)(void *))NP_IPC2_VSLOT(self->blockingService, 1))(self->blockingService);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ServiceDelete(struct NpIpc2Service *self)
{
	npIpc2ServiceDestruct(self);
	return SceNpCommonPs4_BB5282C8(self);
}

const char *npIpc2ServiceGetName(struct NpIpc2Service *service)
{
	return service->config.name;
}

/* the derived classes only set their own table, the base destructors do the work */
struct NpIpc2ClientBlockingService *npIpc2ServiceBlockingImplDestruct(struct NpIpc2ClientBlockingService *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceBlockingImplVtable);
	npIpc2ClientBlockingServiceDestructBase(self);
	return self;
}

struct NpIpc2ClientEventQueue *npIpc2ServiceEventQueueImplDestruct(struct NpIpc2ClientEventQueue *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceEventQueueImplVtable);
	npIpc2ClientEventQueueDestructBase(self);
	return self;
}

struct NpIpc2ClientEventFlag *npIpc2ServiceEventFlagImplDestruct(struct NpIpc2ClientEventFlag *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceEventFlagImplVtable);
	npIpc2ClientEventFlagDestructBase(self);
	return self;
}

int npIpc2ServiceBlockingImplDelete(struct NpIpc2ClientBlockingService *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceBlockingImplVtable);
	npIpc2ClientBlockingServiceDestructBase(self);
	return SceNpCommonPs4_BB5282C8(self);
}

int npIpc2ServiceEventQueueImplDelete(struct NpIpc2ClientEventQueue *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceEventQueueImplVtable);
	npIpc2ClientEventQueueDestructBase(self);
	return SceNpCommonPs4_BB5282C8(self);
}

int npIpc2ServiceEventFlagImplDelete(struct NpIpc2ClientEventFlag *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ServiceEventFlagImplVtable);
	npIpc2ClientEventFlagDestructBase(self);
	return SceNpCommonPs4_BB5282C8(self);
}

const void *g_npIpc2ServiceBlockingImplVtable[] = {		/* 0x8102acf8 */
	NULL,
	NULL,
	(const void *)npIpc2ServiceBlockingImplDestruct,
	(const void *)npIpc2ServiceBlockingImplDelete,
	(const void *)npIpc2ServiceBlockingSendCreateRequest,
	(const void *)npIpc2ServiceBlockingSendDeleteRequest,
	(const void *)npIpc2ServiceBlockingSendAbortRequest,
	(const void *)npIpc2ServiceBlockingSendBeginRequest,
};

const void *g_npIpc2ServiceEventQueueImplVtable[] = {		/* 0x8102ad18 */
	NULL,
	NULL,
	(const void *)npIpc2ServiceEventQueueImplDestruct,
	(const void *)npIpc2ServiceEventQueueImplDelete,
	(const void *)npIpc2ServiceEventQueueReceive,
};

const void *g_npIpc2ServiceEventFlagImplVtable[] = {		/* 0x8102ad2c */
	NULL,
	NULL,
	(const void *)npIpc2ServiceEventFlagImplDestruct,
	(const void *)npIpc2ServiceEventFlagImplDelete,
};

const void *g_npIpc2ServiceVtable[] = {				/* 0x8102ad3c */
	NULL,
	NULL,
	(const void *)npIpc2ServiceDestruct,
	(const void *)npIpc2ServiceDelete,
};

const void *g_npIpc2ServiceClientVtable[] = {			/* 0x8102ad4c */
	NULL,
	NULL,
	(const void *)npIpc2ServiceClientDestruct,
	(const void *)npIpc2ServiceClientDelete,
};

}
