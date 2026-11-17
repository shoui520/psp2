/*
 * The NP IPC2 client's event queues: a service's queues of events, announced by one event flag,
 * which the client polls by index.
 */

#include "np_ipc2_client.h"

extern "C" {


extern const void *g_npIpc2ClientEventQueueManagerVtable[];	/* 0x8102adac */
extern const void *g_npIpc2ClientEventQueueVtable[];		/* 0x8102adbc */

struct NpIpc2ClientManager *npIpc2ClientEventQueueManagerConstruct(struct NpIpc2ClientManager *self,
	struct NpIpc2ServiceClient *client)
{
	self->client = client;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventQueueManagerVtable);
	return self;
}

void npIpc2ClientEventQueueManagerNullsub(void)
{
}

struct NpIpc2ClientManager *npIpc2ClientEventQueueManagerDestruct(struct NpIpc2ClientManager *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ClientEventQueueManagerDelete(struct NpIpc2ClientManager *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

void npIpc2ClientEventQueueManagerNullsub2(void)
{
}

int npIpc2ClientEventQueueManagerInit(struct NpIpc2ClientManager *self, void *memory)
{
	return 0;
}

void npIpc2ClientEventQueueManagerNullsub3(void)
{
}

struct NpIpc2ClientEventQueue *npIpc2ClientEventQueueConstruct(struct NpIpc2ClientEventQueue *self,
	struct NpIpc2ClientManager *manager)
{
	self->manager = manager;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventQueueVtable);
	self->m_numEventQueue = 0;
	self->queueMemory = NULL;
	self->queues = NULL;
	SceNpCommonPs4_945AC0FC(&self->eventFlag);
	return self;
}

struct NpIpc2ClientEventQueue *npIpc2ClientEventQueueDestruct(struct NpIpc2ClientEventQueue *self)
{
	struct NpIpc2ClientEventQueueSlot **queues;
	void *memory;

	queues = self->queues;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventQueueVtable);
	if (queues != NULL)
		npIpc2ClientFree(queues);
	memory = self->queueMemory;
	if (memory != NULL)
		npIpc2ClientFree(memory);
	SceNpCommonPs4_4C3E58CA(&self->eventFlag);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ClientEventQueueDelete(struct NpIpc2ClientEventQueue *self)
{
	struct NpIpc2ClientEventQueueSlot **queues;
	void *memory;

	queues = self->queues;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventQueueVtable);
	if (queues != NULL)
		npIpc2ClientFree(queues);
	memory = self->queueMemory;
	if (memory != NULL)
		npIpc2ClientFree(memory);
	SceNpCommonPs4_4C3E58CA(&self->eventFlag);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

struct NpIpc2ClientEventQueue *npIpc2ClientEventQueueDestructBase(struct NpIpc2ClientEventQueue *self)
{
	struct NpIpc2ClientEventQueueSlot **queues;
	void *memory;

	queues = self->queues;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventQueueVtable);
	if (queues != NULL)
		npIpc2ClientFree(queues);
	memory = self->queueMemory;
	if (memory != NULL)
		npIpc2ClientFree(memory);
	SceNpCommonPs4_4C3E58CA(&self->eventFlag);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ClientEventQueueInit(struct NpIpc2ClientEventQueue *self, const char *name, const SceUInt32 *config)
{
	struct NpIpc2ClientEventQueueSlot **queues;
	struct NpIpc2ClientEventQueueSlot *slot;
	struct NpIpc2ClientEventQueueSlot *queue;
	char eventFlagName[32];
	const SceUInt32 *entry;
	SceUInt32 count;
	SceUInt32 id;
	SceUInt32 i;
	int result;

	count = *config;
	if (count > 0x20)
		return NP_IPC2_ERROR_8055A304;
	self->queueMemory = npIpc2ClientAlloc(16 * count);
	if (self->queueMemory == NULL) {
		result = NP_IPC2_ERROR_OUT_OF_MEMORY;
	} else {
		queues = (struct NpIpc2ClientEventQueueSlot **)npIpc2ClientAlloc(4 * *config);
		self->queues = queues;
		if (queues == NULL) {
			result = NP_IPC2_ERROR_OUT_OF_MEMORY;
		} else {
			count = *config;
			for (i = 0; i < count; i++) {
				slot = NULL;
				queue = new ((char *)self->queueMemory + 16 * i) struct NpIpc2ClientEventQueueSlot;
				if (queue != NULL) {
					queue->queue = self;
					slot = queue;
				}
				self->queues[i] = slot;
				count = *config;
			}
			self->m_numEventQueue = count;
			entry = config;
			for (i = 0; i < count; i++) {
				result = NP_IPC2_ERROR_8055A304;
				slot = self->queues[i];
				if (entry[1] <= 0x400) {
					result = 0;
					slot->itemSize = entry[1];
					slot->attr = entry[2];
					slot->needWait = 1;
				}
				if (result < 0)
					goto fail;
				entry += 2;
			}
			id = SceNpCommonPs4_A37CE123();
			sceClibSnprintf(eventFlagName, sizeof(eventFlagName), "%sEqEvf%08x", name, id);
			result = SceNpCommonPs4_BA957ABE(&self->eventFlag, eventFlagName, 0x1080);
			if (result >= 0)
				return 0;
			return result;
		}
	}
fail:
	SceNpCommonPs4_E7D29E99(&self->eventFlag);
	if (self->queues != NULL) {
		npIpc2ClientFree(self->queues);
		self->queues = NULL;
	}
	if (self->queueMemory != NULL) {
		npIpc2ClientFree(self->queueMemory);
		self->queueMemory = NULL;
	}
	return result;
}

int npIpc2ClientEventQueuePoll(struct NpIpc2ClientEventQueue *self, SceUInt32 queueIndex, void *buffer, SceSize size)
{
	static const char functionName[] = "Poll";
	struct NpIpc2ClientEventQueueSlot *slot;
	int result;

	NP_IPC2_ASSERT(queueIndex < self->m_numEventQueue, functionName, "np_ipc2_client_eventqueue.cpp", 281,
		"queueIndex < m_numEventQueue");
	slot = self->queues[queueIndex];
	for (;; slot->needWait = 1) {
		while (slot->needWait != 0) {
			result = SceNpCommonPs4_D6F2A0EC(&self->eventFlag, (int)self->queues, 1 << (queueIndex & 31), 0, 5, NULL);
			if (result == NP_COMMON_PS4_ERROR_8055800E)
				return NP_IPC2_ERROR_8055A311;
			if (result < 0)
				return result;
			slot->needWait = 0;
		}
		result = ((int (*)(void *, SceUInt32, void *, SceSize))slot->queue->vtable[2])(
			slot->queue, queueIndex, buffer, size);
		if (result != NP_IPC2_ERROR_8055A311)
			break;
	}
	if (result >= 0)
		return 0;
	return result;
}

const void *g_npIpc2ClientEventQueueManagerVtable[] = {		/* 0x8102adac */
	NULL,
	NULL,
	(const void *)npIpc2ClientEventQueueManagerDestruct,
	(const void *)npIpc2ClientEventQueueManagerDelete,
};

const void *g_npIpc2ClientEventQueueVtable[] = {		/* 0x8102adbc */
	NULL,
	NULL,
	(const void *)npIpc2ClientEventQueueDestruct,
	(const void *)npIpc2ClientEventQueueDelete,
	(const void *)__cxa_pure_virtual,
};

}
