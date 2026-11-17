/*
 * The NP IPC2 client's blocking requests: every request of every service takes one of the client's
 * IPC slots while the NP service works on it, and waits on a semaphore of its own, in line, when
 * all slots are taken.
 */

#include "np_ipc2_client.h"

extern "C" {


extern const void *g_npIpc2ClientBlockingVtable[];		/* 0x8102ad7c */
extern const void *g_npIpc2ClientBlockingServiceVtable[];	/* 0x8102ad5c */

struct NpIpc2ClientBlocking *npIpc2ClientBlockingConstruct(struct NpIpc2ClientBlocking *self,
	struct NpIpc2ServiceClient *client)
{
	self->client = client;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientBlockingVtable);
	self->m_numIpcSlot = 0;
	self->numOutstanding = 0;
	self->m_outstandingSlot = NULL;
	self->m_waitingIpcSlotHead = NULL;
	SceNpCommonPs4_4266D870(&self->mutex);
	return self;
}

struct NpIpc2ClientBlocking *npIpc2ClientBlockingDestruct(struct NpIpc2ClientBlocking *self)
{
	struct NpIpc2ClientBlockingRequest **outstandingSlot;

	outstandingSlot = self->m_outstandingSlot;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientBlockingVtable);
	if (outstandingSlot != NULL)
		npIpc2ClientFree(outstandingSlot);
	SceNpCommonPs4_294F0B1A(&self->mutex);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ClientBlockingDelete(struct NpIpc2ClientBlocking *self)
{
	struct NpIpc2ClientBlockingRequest **outstandingSlot;

	outstandingSlot = self->m_outstandingSlot;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientBlockingVtable);
	if (outstandingSlot != NULL)
		npIpc2ClientFree(outstandingSlot);
	SceNpCommonPs4_294F0B1A(&self->mutex);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

void npIpc2ClientBlockingNullsub(void)
{
}

int npIpc2ClientBlockingInit(struct NpIpc2ClientBlocking *self, const void *config, const SceUInt32 *pNumIpcSlot)
{
	static const char functionName[] = "Init";
	void *slots;
	int result;

	NP_IPC2_ASSERT(self->m_outstandingSlot == NULL, functionName, "np_ipc2_client_blocking.cpp", 185, "!IsInit()");
	result = SceNpCommonPs4_8AA7E995(&self->mutex, "SceNpIpc2ClientBlockingCall", 0);
	if (result < 0)
		return result;
	slots = npIpc2ClientAlloc(4 * *pNumIpcSlot);
	self->m_outstandingSlot = (struct NpIpc2ClientBlockingRequest **)slots;
	if (slots != NULL) {
		sceClibMemset(slots, 0, 4 * *pNumIpcSlot);
		self->m_numIpcSlot = *pNumIpcSlot;
		self->numOutstanding = 0;
		NP_IPC2_ASSERT(self->m_outstandingSlot != NULL, functionName, "np_ipc2_client_blocking.cpp", 238, "IsInit()");
		return 0;
	}
	SceNpCommonPs4_63EED478(&self->mutex);
	return NP_IPC2_ERROR_OUT_OF_MEMORY;
}

int npIpc2ClientBlockingClearOutstandingIpcSlot(struct NpIpc2ClientBlocking *self,
	struct NpIpc2ClientBlockingRequest *request)
{
	static const char functionName[] = "clearOutstandingIpcSlot";
	int slot;
	struct NpIpc2ClientBlockingRequest **outstandingSlot;

	slot = request->ipcSlot;
	if (slot == NP_IPC2_INVALID_IPC_SLOT) {
		NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 362, "context->ipcSlot != INVALID_IPC_SLOT");
		slot = request->ipcSlot;
	}
	outstandingSlot = self->m_outstandingSlot;
	if (outstandingSlot[slot] != request) {
		NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 363,
			"m_outstandingSlot[context->ipcSlot] == context");
		slot = request->ipcSlot;
		outstandingSlot = self->m_outstandingSlot;
	}
	outstandingSlot[slot] = NULL;
	request->ipcSlot = NP_IPC2_INVALID_IPC_SLOT;
	return NP_IPC2_INVALID_IPC_SLOT;
}

__attribute__((noinline)) int npIpc2ClientBlockingRequestSetState(struct NpIpc2ClientBlockingRequest *request, int state)
{
	static const char functionName[] = "removeWaitingIpcSlot";
	struct NpIpc2ClientBlocking *blocking;
	struct NpIpc2ClientBlockingRequest *previous;
	struct NpIpc2ClientBlockingRequest *itr;
	int current;
	int result;

	result = (int)request;
	current = request->state;
	if (current != state) {
		if (current == NP_IPC2_REQUEST_STATE_WAITING) {
			previous = NULL;
			blocking = request->service->blocking;
			for (itr = blocking->m_waitingIpcSlotHead; itr != NULL; itr = itr->nextWaiting) {
				if (itr == request)
					break;
				previous = itr;
			}
			if (itr == request) {
				if (previous != NULL) {
					result = (int)itr->nextWaiting;
					previous->nextWaiting = itr->nextWaiting;
					current = request->state;
				} else {
					NP_IPC2_ASSERT(itr == blocking->m_waitingIpcSlotHead, functionName, "np_ipc2_client_blocking.cpp",
						321, "itr == m_waitingIpcSlotHead");
					result = (int)itr->nextWaiting;
					blocking->m_waitingIpcSlotHead = itr->nextWaiting;
					current = request->state;
				}
			}
		}
		if (current == NP_IPC2_REQUEST_STATE_PENDING)
			result = npIpc2ClientBlockingClearOutstandingIpcSlot(request->service->blocking, request);
		request->state = state;
		if (state == NP_IPC2_REQUEST_STATE_IDLE) {
			request->ipcSlot = NP_IPC2_INVALID_IPC_SLOT;
			request->reqId = NP_IPC2_INVALID_REQUEST_ID;
			return NP_IPC2_INVALID_REQUEST_ID;
		}
	}
	return result;
}

int npIpc2ClientBlockingServiceGetServiceId(struct NpIpc2ClientBlockingService *self)
{
	return npIpc2ServiceGetId(self->service);
}

__attribute__((noinline)) int npIpc2ClientBlockingBeginRequest(struct NpIpc2ClientBlocking *self, struct NpIpc2ClientBlockingRequest *request)
{
	static const char functionName[] = "beginRequest";
	static const char addFunctionName[] = "addWaitingIpcSlot";
	static const char setFunctionName[] = "setOutstandingIpcSlot";
	struct NpIpc2ClientBlockingService *service;
	struct NpIpc2ClientBlockingRequest *waiting;
	struct NpIpc2ClientBlockingRequest *last;
	struct NpIpc2ClientBlockingRequest *next;
	SceUInt32 slot;
	SceUInt32 count;
	int result;

	if (request->state != NP_IPC2_REQUEST_STATE_READY) {
		service = request->service;
		sceClibPrintf("[NpIpc2Client:%s(%d:%s)] Request(%d):<%d>:[%d]: BeginRequest: assertion failed. invalid state..\n",
			service->blocking->client->ipmiClient.name, npIpc2ServiceGetId(service->service),
			npIpc2ServiceGetName(request->service->service), request->reqId, request->state, request->ipcSlot);
	}
	if (request->state != NP_IPC2_REQUEST_STATE_READY)
		NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 386, "context->state == Request::STATE_READY");
	if (self->numOutstanding == self->m_numIpcSlot) {
		npIpc2ClientBlockingRequestSetState(request, NP_IPC2_REQUEST_STATE_WAITING);
		waiting = self->m_waitingIpcSlotHead;
		if (waiting != NULL) {
			while (waiting != request) {
				waiting = waiting->nextWaiting;
				if (waiting == NULL)
					goto notFound;
			}
			NP_IPC2_ASSERT(0, addFunctionName, "np_ipc2_client_blocking.cpp", 290, "!findWaitingIpcSlot(context)");
		}
notFound:
		request->nextWaiting = NULL;
		last = self->m_waitingIpcSlotHead;
		if (last == NULL) {
			self->m_waitingIpcSlotHead = request;
		} else {
			for (next = last->nextWaiting; next != NULL; next = next->nextWaiting)
				last = next;
			last->nextWaiting = request;
		}
		SceNpCommonPs4_6BA5A164(&self->mutex);
		SceNpCommonPs4_1D3964FF(&request->m_sema, 0);
		SceNpCommonPs4_A2A8493B(&self->mutex);
		if (request->state == NP_IPC2_REQUEST_STATE_ABORTED)
			return NP_IPC2_ERROR_8055A30C;
		if (request->state != NP_IPC2_REQUEST_STATE_WAITING)
			return NP_IPC2_ERROR_8055A303;
	} else {
		self->numOutstanding++;
	}
	npIpc2ClientBlockingRequestSetState(request, NP_IPC2_REQUEST_STATE_PENDING);
	if (request->ipcSlot != NP_IPC2_INVALID_IPC_SLOT)
		NP_IPC2_ASSERT(0, setFunctionName, "np_ipc2_client_blocking.cpp", 343, "context->ipcSlot == INVALID_IPC_SLOT");
	slot = 0;
	result = 0;
	count = self->m_numIpcSlot;
	if (count != 0) {
		struct NpIpc2ClientBlockingRequest **slots = self->m_outstandingSlot;
		SceUInt32 index = 0;

		while (slots[slot] != NULL) {
			slot++;
			index++;
			if (slot >= count)
				goto done;
		}
		slots[index] = request;
	}
done:
	request->ipcSlot = slot;
	return result;
}

__attribute__((noinline)) int npIpc2ClientBlockingEndRequest(struct NpIpc2ClientBlocking *self, struct NpIpc2ClientBlockingRequest *request)
{
	static const char functionName[] = "endRequest";
	static const char removeFunctionName[] = "removeWaitingIpcSlot";
	struct NpIpc2ClientBlockingService *service;
	struct NpIpc2ClientBlockingRequest *head;
	struct NpIpc2ClientBlockingRequest *itr;
	struct NpIpc2ClientBlockingRequest *previous;
	struct NpIpc2ClientBlockingRequest **slots;
	struct NpIpc2ClientBlockingRequest *current;
	int state;
	int slot;
	int result;

	state = request->state;
	if (state != NP_IPC2_REQUEST_STATE_READY && state != NP_IPC2_REQUEST_STATE_ABORTED) {
		service = request->service;
		sceClibPrintf("[NpIpc2Client:%s(%d:%s)] Request(%d):<%d>:[%d]): EndRequest: assertion failed. invalid state..\n",
			service->blocking->client->ipmiClient.name, npIpc2ServiceGetId(service->service),
			npIpc2ServiceGetName(request->service->service), request->reqId, request->state, request->ipcSlot);
		state = request->state;
	}
	if (state != NP_IPC2_REQUEST_STATE_READY && state != NP_IPC2_REQUEST_STATE_ABORTED)
		NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 455,
			"context->state == Request::STATE_READY || context->state == Request::STATE_ABORTED");
	head = self->m_waitingIpcSlotHead;
	if (head != NULL) {
		itr = self->m_waitingIpcSlotHead;
		previous = NULL;
		do {
			if (itr == head)
				break;
			previous = itr;
			itr = itr->nextWaiting;
		} while (itr != NULL);
		if (itr == head) {
			if (previous != NULL) {
				previous->nextWaiting = itr->nextWaiting;
			} else {
				if (itr != head)
					NP_IPC2_ASSERT(0, removeFunctionName, "np_ipc2_client_blocking.cpp", 321,
						"itr == m_waitingIpcSlotHead");
				self->m_waitingIpcSlotHead = itr->nextWaiting;
			}
		}
		result = SceNpCommonPs4_B764816E(&head->m_sema);
	} else {
		result = self->numOutstanding - 1;
		self->numOutstanding = result;
	}
	slot = request->ipcSlot;
	if (slot != NP_IPC2_INVALID_IPC_SLOT) {
		slots = self->m_outstandingSlot;
		current = slots[slot];
		if (current != request) {
			service = request->service;
			sceClibPrintf("[NpIpc2Client:%s(%d:%s)] Request(%d):(<%d>:[%d]): EndRequest: assertion failed. slot not matched.\n",
				service->blocking->client->ipmiClient.name, npIpc2ServiceGetId(service->service),
				npIpc2ServiceGetName(request->service->service), request->reqId, request->state, request->ipcSlot);
			slot = request->ipcSlot;
			slots = self->m_outstandingSlot;
			current = slots[slot];
		}
		if (current != request) {
			NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 483,
				"m_outstandingSlot[context->ipcSlot] == context");
			slot = request->ipcSlot;
			slots = self->m_outstandingSlot;
		}
		slots[slot] = NULL;
		request->ipcSlot = NP_IPC2_INVALID_IPC_SLOT;
		return NP_IPC2_INVALID_IPC_SLOT;
	}
	return result;
}

__attribute__((noinline)) int npIpc2ClientBlockingWaitRequest(struct NpIpc2ClientBlocking *self, struct NpIpc2ClientBlockingRequest *request,
	SceUInt32 timeout)
{
	static const char functionName[] = "waitRequest";
	int ipcSlot;
	int result;

	ipcSlot = request->ipcSlot;
	if (ipcSlot < 0)
		NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 507, "ipcSlot >= 0");
	SceNpCommonPs4_6BA5A164(&self->mutex);
	if (timeout != 0)
		result = npIpc2IpmiClientWaitEventFlag(&self->client->ipmiClient, ipcSlot / 32, 1 << (ipcSlot % 32), 5, NULL, &timeout);
	else
		result = npIpc2IpmiClientWaitEventFlag(&self->client->ipmiClient, ipcSlot / 32, 1 << (ipcSlot % 32), 5, NULL, NULL);
	SceNpCommonPs4_A2A8493B(&self->mutex);
	if (result == NP_IPC2_ERROR_TIMEOUT)
		return NP_IPC2_ERROR_TIMEOUT;
	if (result >= 0)
		return 0;
	return result;
}

__attribute__((noinline)) int npIpc2ClientBlockingRequestTerm(struct NpIpc2ClientBlockingRequest *request)
{
	static const char functionName[] = "removeWaitingIpcSlot";
	struct NpIpc2ClientBlocking *blocking;
	struct NpIpc2ClientBlockingRequest *previous;
	struct NpIpc2ClientBlockingRequest *itr;
	int state;
	int result;

	result = (int)request;
	state = request->state;
	if (state == NP_IPC2_REQUEST_STATE_WAITING) {
		previous = NULL;
		blocking = request->service->blocking;
		result = (int)blocking->m_waitingIpcSlotHead;
		for (itr = blocking->m_waitingIpcSlotHead; itr != NULL; itr = itr->nextWaiting) {
			if (itr == request)
				break;
			previous = itr;
		}
		if (itr == request) {
			if (previous != NULL) {
				result = (int)itr->nextWaiting;
				previous->nextWaiting = itr->nextWaiting;
				state = request->state;
			} else {
				if (itr != blocking->m_waitingIpcSlotHead)
					NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 321, "itr == m_waitingIpcSlotHead");
				result = (int)itr->nextWaiting;
				blocking->m_waitingIpcSlotHead = itr->nextWaiting;
				state = request->state;
			}
		}
	}
	if (state == NP_IPC2_REQUEST_STATE_PENDING)
		return npIpc2ClientBlockingClearOutstandingIpcSlot(request->service->blocking, request);
	return result;
}

__attribute__((noinline)) int npIpc2ClientBlockingRequestInit(struct NpIpc2ClientBlockingRequest *request, SceUInt32 index)
{
	static const char functionName[] = "Init";
	char name[32];
	int result;

	if (SceNpCommonPs4_6BE664A1(&request->m_sema) != 0)
		NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 80, "!m_sema.IsInit()");
	request->state = NP_IPC2_REQUEST_STATE_IDLE;
	request->ipcSlot = NP_IPC2_INVALID_IPC_SLOT;
	request->reqId = NP_IPC2_INVALID_REQUEST_ID;
	request->nextWaiting = NULL;
	sceClibSnprintf(name, sizeof(name), "%sBlSem%02u", npIpc2ServiceGetName(request->service->service), index);
	result = SceNpCommonPs4_47268A8C(&request->m_sema, 0, 1, name);
	if (result >= 0) {
		if (SceNpCommonPs4_6BE664A1(&request->m_sema) == 0)
			NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 92, "m_sema.IsInit()");
		result = 0;
	}
	return result;
}

struct NpIpc2ClientBlockingService *npIpc2ClientBlockingServiceConstruct(struct NpIpc2ClientBlockingService *self,
	struct NpIpc2ClientBlocking *blocking, struct NpIpc2Service *service)
{
	self->blocking = blocking;
	self->service = service;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientBlockingServiceVtable);
	self->numRequest = 0;
	self->requests = NULL;
	self->requestMemory = NULL;
	return self;
}

__attribute__((noinline)) void npIpc2ClientBlockingServiceTerm(struct NpIpc2ClientBlockingService *self)
{
	struct NpIpc2ClientBlockingRequest **requests;
	struct NpIpc2ClientBlockingRequest *request;
	struct NpCommonPs4Mutex *mutex;
	SceUInt32 i;

	if (self->requests != NULL) {
		mutex = &self->blocking->mutex;
		SceNpCommonPs4_A2A8493B(mutex);
		requests = self->requests;
		i = 0;
		if (self->numRequest != 0) {
			do {
				request = requests[i];
				npIpc2ClientBlockingRequestTerm(request);
				SceNpCommonPs4_CD40FE5E(&request->m_sema);
				i++;
				requests = self->requests;
			} while (i < self->numRequest);
		}
		npIpc2ClientFree(requests);
		SceNpCommonPs4_6BA5A164(mutex);
	}
	if (self->requestMemory != NULL)
		npIpc2ClientFree(self->requestMemory);
}

struct NpIpc2ClientBlockingService *npIpc2ClientBlockingServiceDestruct(struct NpIpc2ClientBlockingService *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientBlockingServiceVtable);
	npIpc2ClientBlockingServiceTerm(self);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ClientBlockingServiceDelete(struct NpIpc2ClientBlockingService *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientBlockingServiceVtable);
	npIpc2ClientBlockingServiceTerm(self);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

struct NpIpc2ClientBlockingService *npIpc2ClientBlockingServiceDestructBase(struct NpIpc2ClientBlockingService *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientBlockingServiceVtable);
	npIpc2ClientBlockingServiceTerm(self);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

void npIpc2ClientBlockingServiceNullsub(void)
{
}

int npIpc2ClientBlockingServiceInit(struct NpIpc2ClientBlockingService *self, const SceUInt32 *pNumRequest)
{
	static const char functionName[] = "Init";
	struct NpIpc2ClientBlockingRequest **requests;
	struct NpIpc2ClientBlockingRequest *request;
	struct NpIpc2ClientBlockingRequest *created;
	SceUInt32 i;
	SceUInt32 count;
	int result;

	if (self->requests != NULL)
		NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 712, "!IsInit()");
	self->requestMemory = npIpc2ClientAlloc(32 * *pNumRequest);
	if (self->requestMemory == NULL) {
		result = NP_IPC2_ERROR_OUT_OF_MEMORY;
		requests = self->requests;
	} else {
		requests = (struct NpIpc2ClientBlockingRequest **)npIpc2ClientAlloc(4 * *pNumRequest);
		self->requests = requests;
		if (requests == NULL) {
			result = NP_IPC2_ERROR_OUT_OF_MEMORY;
		} else {
			count = *pNumRequest;
			for (i = 0; i < count; i++) {
				created = NULL;
				request = new ((char *)self->requestMemory + 32 * i) struct NpIpc2ClientBlockingRequest;
				if (request != NULL) {
					request->state = NP_IPC2_REQUEST_STATE_IDLE;
					request->ipcSlot = NP_IPC2_INVALID_IPC_SLOT;
					request->reqId = NP_IPC2_INVALID_REQUEST_ID;
					request->nextWaiting = NULL;
					request->service = self;
					SceNpCommonPs4_90A8B2FA(&request->m_sema);
					created = request;
				}
				self->requests[i] = created;
				count = *pNumRequest;
			}
			for (i = 0; i < count; i++) {
				result = npIpc2ClientBlockingRequestInit(self->requests[i], i);
				if (result < 0)
					break;
				count = *pNumRequest;
			}
			if (i >= count) {
				self->numRequest = count;
				if (self->requests == NULL)
					NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 740, "IsInit()");
				return 0;
			}
			requests = self->requests;
		}
	}
	if (requests != NULL) {
		for (i = 0; i < *pNumRequest; i++) {
			request = requests[i];
			npIpc2ClientBlockingRequestTerm(request);
			SceNpCommonPs4_CD40FE5E(&request->m_sema);
			requests = self->requests;
		}
		npIpc2ClientFree(requests);
		self->requests = NULL;
	}
	if (self->requestMemory != NULL) {
		npIpc2ClientFree(self->requestMemory);
		self->requestMemory = NULL;
	}
	return result;
}

/* the request of the service that is not idle and has this request ID: its place in the table and its state */
#define NP_IPC2_FIND_REQUEST(service, id, slot, request, state) \
	do { \
		struct NpIpc2ClientBlockingRequest *candidate; \
		SceUInt32 index; \
		SceUInt32 count; \
		\
		(request) = NULL; \
		count = (service)->numRequest; \
		if (count != 0) { \
			(slot) = (service)->requests; \
			for (index = 0; index < count; index++, (slot)++) { \
				candidate = *(slot); \
				(state) = candidate->state; \
				if ((state) != NP_IPC2_REQUEST_STATE_IDLE && candidate->reqId == (id)) { \
					(request) = candidate; \
					break; \
				} \
			} \
		} \
	} while (0)

int npIpc2ClientBlockingServiceCreateRequest(struct NpIpc2ClientBlockingService *self, int *pReqId, const void *data,
	SceSize size, unsigned char flag)
{
	static const char functionName[] = "CreateRequest";
	struct NpIpc2ClientBlocking *blocking;
	struct NpIpc2ClientBlockingRequest **slot;
	struct NpIpc2ClientBlockingRequest *request;
	struct NpCommonPs4Mutex *mutex;
	SceUInt32 count;
	SceUInt32 i;
	int reqId;
	int result;

	if (npIpc2IpmiClientIsConnected(&self->blocking->client->ipmiClient) == 0)
		return NP_IPC2_ERROR_8055A308;
	mutex = &self->blocking->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	count = self->numRequest;
	i = 0;
	slot = self->requests;
	if (count != 0) {
		for (; i < count; i++, slot++) {
			request = *slot;
			if (request->state == NP_IPC2_REQUEST_STATE_IDLE)
				break;
		}
	}
	if (i >= count || request == NULL) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_IPC2_ERROR_8055A309;
	}
	npIpc2ClientBlockingRequestSetState(*slot, NP_IPC2_REQUEST_STATE_READY);
	SceNpCommonPs4_6BA5A164(mutex);
	reqId = NP_IPC2_INVALID_REQUEST_ID;
	if (flag != 0)
		result = ((int (*)(void *, const void *, SceSize, int *, int))self->vtable[2])(self, data, size, &reqId, 1);
	else
		result = ((int (*)(void *, const void *, SceSize, int *, int))self->vtable[2])(self, data, size, &reqId, 0);
	blocking = self->blocking;
	SceNpCommonPs4_A2A8493B(&blocking->mutex);
	if (result >= 0) {
		if (reqId == NP_IPC2_INVALID_REQUEST_ID) {
			if (reqId == NP_IPC2_INVALID_REQUEST_ID)
				sceClibPrintf("[NpIpc2Client:%s(%d:%s)] Request(%d:<%d>:[%d]): CreateRequest: assertion failed. assigned reqId is invalid.\n",
					request->service->blocking->client->ipmiClient.name, npIpc2ServiceGetId(request->service->service),
					npIpc2ServiceGetName(request->service->service), request->reqId, request->state, request->ipcSlot);
			if (reqId == NP_IPC2_INVALID_REQUEST_ID)
				NP_IPC2_ASSERT(0, functionName, "np_ipc2_client_blocking.cpp", 858, "reqId != INVALID_REQUEST_ID");
			npIpc2ClientBlockingRequestSetState(request, NP_IPC2_REQUEST_STATE_IDLE);
			SceNpCommonPs4_6BA5A164(&blocking->mutex);
			return NP_IPC2_ERROR_8055A303;
		}
		request->reqId = reqId;
		*pReqId = reqId;
		SceNpCommonPs4_6BA5A164(&blocking->mutex);
		return 0;
	}
	npIpc2ClientBlockingRequestSetState(request, NP_IPC2_REQUEST_STATE_IDLE);
	SceNpCommonPs4_6BA5A164(&blocking->mutex);
	return result;
}

int npIpc2ClientBlockingServiceDeleteRequest(struct NpIpc2ClientBlockingService *self, int reqId, unsigned char flag)
{
	struct NpIpc2ClientBlocking *blocking;
	struct NpIpc2ClientBlockingRequest **slot;
	struct NpIpc2ClientBlockingRequest *request;
	struct NpCommonPs4Mutex *mutex;
	int state;
	int result;

	if (npIpc2IpmiClientIsConnected(&self->blocking->client->ipmiClient) == 0)
		return NP_IPC2_ERROR_8055A308;
	mutex = &self->blocking->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	NP_IPC2_FIND_REQUEST(self, reqId, slot, request, state);
	if (request == NULL) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_IPC2_ERROR_8055A30A;
	}
	if (state == NP_IPC2_REQUEST_STATE_READY || state == NP_IPC2_REQUEST_STATE_ABORTED) {
		SceNpCommonPs4_6BA5A164(mutex);
		if (flag != 0)
			result = ((int (*)(void *, int, int))self->vtable[3])(self, reqId, 1);
		else
			result = ((int (*)(void *, int, int))self->vtable[3])(self, reqId, 0);
		blocking = self->blocking;
		SceNpCommonPs4_A2A8493B(&blocking->mutex);
		if (result == 0)
			npIpc2ClientBlockingRequestSetState(request, NP_IPC2_REQUEST_STATE_IDLE);
		SceNpCommonPs4_6BA5A164(&blocking->mutex);
		return result;
	}
	SceNpCommonPs4_6BA5A164(mutex);
	return NP_IPC2_ERROR_8055A30B;
}

int npIpc2ClientBlockingServiceAbortRequest(struct NpIpc2ClientBlockingService *self, int reqId, unsigned char flag)
{
	struct NpIpc2ClientBlockingRequest **slot;
	struct NpIpc2ClientBlockingRequest *request;
	struct NpCommonPs4Mutex *mutex;
	int state;
	int result;

	if (npIpc2IpmiClientIsConnected(&self->blocking->client->ipmiClient) == 0)
		return NP_IPC2_ERROR_8055A308;
	mutex = &self->blocking->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	NP_IPC2_FIND_REQUEST(self, reqId, slot, request, state);
	if (request == NULL) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_IPC2_ERROR_8055A30A;
	}
	if (state == NP_IPC2_REQUEST_STATE_ABORTED) {
		SceNpCommonPs4_6BA5A164(mutex);
		return 0;
	}
	SceNpCommonPs4_6BA5A164(mutex);
	if (flag != 0)
		result = ((int (*)(void *, int, int))self->vtable[4])(self, reqId, 1);
	else
		result = ((int (*)(void *, int, int))self->vtable[4])(self, reqId, 0);
	mutex = &self->blocking->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	if (result == 0 && request->state != NP_IPC2_REQUEST_STATE_IDLE && request->reqId == reqId &&
	    request->state != NP_IPC2_REQUEST_STATE_PENDING)
		npIpc2ClientBlockingRequestSetState(request, NP_IPC2_REQUEST_STATE_ABORTED);
	SceNpCommonPs4_6BA5A164(mutex);
	return result;
}

int npIpc2ClientBlockingServiceBeginRequest(struct NpIpc2ClientBlockingService *self, int reqId, SceUInt32 *pIndex,
	char noSend)
{
	struct NpIpc2ClientBlockingRequest **slot;
	struct NpIpc2ClientBlockingRequest *request;
	struct NpCommonPs4Mutex *mutex;
	int state;
	int result;
	int sent;

	if (npIpc2IpmiClientIsConnected(&self->blocking->client->ipmiClient) == 0)
		return NP_IPC2_ERROR_8055A308;
	mutex = &self->blocking->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	NP_IPC2_FIND_REQUEST(self, reqId, slot, request, state);
	if (request == NULL) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_IPC2_ERROR_8055A30A;
	}
	if (state == NP_IPC2_REQUEST_STATE_WAITING || state == NP_IPC2_REQUEST_STATE_PENDING) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_IPC2_ERROR_8055A30B;
	}
	if (state == NP_IPC2_REQUEST_STATE_ABORTED) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_IPC2_ERROR_8055A30C;
	}
	result = npIpc2ClientBlockingBeginRequest(self->blocking, *slot);
	if (result < 0) {
		SceNpCommonPs4_6BA5A164(mutex);
		return result;
	}
	SceNpCommonPs4_6BA5A164(mutex);
	sent = 0;
	if (noSend == 0)
		sent = ((int (*)(void *, int, int))self->vtable[5])(self, request->reqId, request->ipcSlot);
	mutex = &self->blocking->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	if (sent < 0) {
		SceNpCommonPs4_6BA5A164(mutex);
		return sent;
	}
	if (request->state == NP_IPC2_REQUEST_STATE_ABORTED) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_IPC2_ERROR_8055A30C;
	}
	if (pIndex != NULL) {
		pIndex[0] = request->ipcSlot / 32;
		pIndex[1] = 1 << (request->ipcSlot % 32);
	}
	SceNpCommonPs4_6BA5A164(mutex);
	return 0;
}

int npIpc2ClientBlockingServiceWaitRequest(struct NpIpc2ClientBlockingService *self, int reqId, SceUInt32 timeout,
	int reserved)
{
	struct NpIpc2ClientBlocking *blocking;
	struct NpIpc2ClientBlockingRequest **slot;
	struct NpIpc2ClientBlockingRequest *request;
	struct NpCommonPs4Mutex *mutex;
	int state;
	int result;

	if (npIpc2IpmiClientIsConnected(&self->blocking->client->ipmiClient) == 0)
		return NP_IPC2_ERROR_8055A308;
	mutex = &self->blocking->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	NP_IPC2_FIND_REQUEST(self, reqId, slot, request, state);
	if (request == NULL) {
		result = NP_IPC2_ERROR_8055A30A;
	} else if (state == NP_IPC2_REQUEST_STATE_PENDING) {
		blocking = self->blocking;
		result = npIpc2ClientBlockingWaitRequest(blocking, request, timeout);
		if (result == NP_IPC2_ERROR_TIMEOUT)
			result = NP_IPC2_ERROR_TIMEOUT;
		else if (result >= 0)
			result = 0;
	} else {
		result = NP_IPC2_ERROR_8055A303;
	}
	SceNpCommonPs4_6BA5A164(mutex);
	return result;
}

int npIpc2ClientBlockingServiceEndRequest(struct NpIpc2ClientBlockingService *self, int reqId, int reserved)
{
	struct NpIpc2ClientBlockingRequest **slot;
	struct NpIpc2ClientBlockingRequest *request;
	struct NpCommonPs4Mutex *mutex;
	int state;
	int result;

	if (npIpc2IpmiClientIsConnected(&self->blocking->client->ipmiClient) == 0)
		return NP_IPC2_ERROR_8055A308;
	mutex = &self->blocking->mutex;
	SceNpCommonPs4_A2A8493B(mutex);
	result = 0;
	NP_IPC2_FIND_REQUEST(self, reqId, slot, request, state);
	if (request == NULL) {
		SceNpCommonPs4_6BA5A164(mutex);
		return NP_IPC2_ERROR_8055A30A;
	}
	if (state > NP_IPC2_REQUEST_STATE_WAITING) {
		if (state <= NP_IPC2_REQUEST_STATE_PENDING) {
			npIpc2ClientBlockingRequestSetState(*slot, NP_IPC2_REQUEST_STATE_READY);
		} else if (state > NP_IPC2_REQUEST_STATE_ABORTED) {
			result = NP_IPC2_ERROR_8055A303;
			goto done;
		}
		npIpc2ClientBlockingEndRequest(self->blocking, request);
	} else {
		if (state != NP_IPC2_REQUEST_STATE_READY) {
			result = NP_IPC2_ERROR_8055A303;
			goto done;
		}
		result = NP_IPC2_ERROR_8055A30D;
	}
done:
	SceNpCommonPs4_6BA5A164(mutex);
	return result;
}

const void *g_npIpc2ClientBlockingServiceVtable[] = {		/* 0x8102ad5c */
	NULL,
	NULL,
	(const void *)npIpc2ClientBlockingServiceDestruct,
	(const void *)npIpc2ClientBlockingServiceDelete,
	(const void *)__cxa_pure_virtual,
	(const void *)__cxa_pure_virtual,
	(const void *)__cxa_pure_virtual,
	(const void *)__cxa_pure_virtual,
};

const void *g_npIpc2ClientBlockingVtable[] = {			/* 0x8102ad7c */
	NULL,
	NULL,
	(const void *)npIpc2ClientBlockingDestruct,
	(const void *)npIpc2ClientBlockingDelete,
};

}
