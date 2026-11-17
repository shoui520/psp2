/*
 * The NP IPC2 client's event flags: the kernel event flags a service's IPC events set, one array
 * per service, which the client polls by index.
 */

#include "np_ipc2_client.h"

extern "C" {


extern const void *g_npIpc2ClientEventFlagManagerVtable[];	/* 0x8102ad9c */
extern const void *g_npIpc2ClientEventFlagVtable[];		/* 0x8102ad8c */

struct NpIpc2ClientManager *npIpc2ClientEventFlagManagerConstruct(struct NpIpc2ClientManager *self,
	struct NpIpc2ServiceClient *client)
{
	self->client = client;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventFlagManagerVtable);
	return self;
}

void npIpc2ClientEventFlagManagerNullsub(void)
{
}

struct NpIpc2ClientManager *npIpc2ClientEventFlagManagerDestruct(struct NpIpc2ClientManager *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ClientEventFlagManagerDelete(struct NpIpc2ClientManager *self)
{
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

void npIpc2ClientEventFlagManagerNullsub2(void)
{
}

int npIpc2ClientEventFlagManagerInit(struct NpIpc2ClientManager *self, void *memory)
{
	return 0;
}

struct NpIpc2ClientEventFlag *npIpc2ClientEventFlagConstruct(struct NpIpc2ClientEventFlag *self,
	struct NpIpc2ClientManager *manager)
{
	self->manager = manager;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventFlagVtable);
	self->m_numEventFlag = 0;
	self->eventFlags = NULL;
	self->eventFlagMemory = NULL;
	return self;
}

struct NpIpc2ClientEventFlag *npIpc2ClientEventFlagDestruct(struct NpIpc2ClientEventFlag *self)
{
	struct NpCommonPs4EventFlag **eventFlags;
	void *memory;
	SceUInt32 i;

	eventFlags = self->eventFlags;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventFlagVtable);
	if (eventFlags != NULL) {
		i = 0;
		if (self->m_numEventFlag != 0) {
			do {
				((void (*)(void *))NP_IPC2_VSLOT(eventFlags[i], 0))(eventFlags[i]);
				i++;
				eventFlags = self->eventFlags;
			} while (i < self->m_numEventFlag);
		}
		npIpc2ClientFree(eventFlags);
	}
	memory = self->eventFlagMemory;
	if (memory != NULL)
		npIpc2ClientFree(memory);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ClientEventFlagDelete(struct NpIpc2ClientEventFlag *self)
{
	struct NpCommonPs4EventFlag **eventFlags;
	void *memory;
	SceUInt32 i;

	eventFlags = self->eventFlags;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventFlagVtable);
	if (eventFlags != NULL) {
		i = 0;
		if (self->m_numEventFlag != 0) {
			do {
				((void (*)(void *))NP_IPC2_VSLOT(eventFlags[i], 0))(eventFlags[i]);
				i++;
				eventFlags = self->eventFlags;
			} while (i < self->m_numEventFlag);
		}
		npIpc2ClientFree(eventFlags);
	}
	memory = self->eventFlagMemory;
	if (memory != NULL)
		npIpc2ClientFree(memory);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(self);
}

struct NpIpc2ClientEventFlag *npIpc2ClientEventFlagDestructBase(struct NpIpc2ClientEventFlag *self)
{
	struct NpCommonPs4EventFlag **eventFlags;
	void *memory;
	SceUInt32 i;

	eventFlags = self->eventFlags;
	self->vtable = NP_COMMON_PS4_VTABLE(g_npIpc2ClientEventFlagVtable);
	if (eventFlags != NULL) {
		i = 0;
		if (self->m_numEventFlag != 0) {
			do {
				((void (*)(void *))NP_IPC2_VSLOT(eventFlags[i], 0))(eventFlags[i]);
				i++;
				eventFlags = self->eventFlags;
			} while (i < self->m_numEventFlag);
		}
		npIpc2ClientFree(eventFlags);
	}
	memory = self->eventFlagMemory;
	if (memory != NULL)
		npIpc2ClientFree(memory);
	self->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return self;
}

int npIpc2ClientEventFlagInit(struct NpIpc2ClientEventFlag *self, const char *name, const SceUInt32 *pNumEventFlag)
{
	struct NpCommonPs4EventFlag **eventFlags;
	struct NpCommonPs4EventFlag *eventFlag;
	char eventFlagName[32];
	SceUInt32 count;
	SceUInt32 id;
	SceUInt32 i;
	int result;

	if (*pNumEventFlag > 0x10)
		return NP_IPC2_ERROR_8055A304;
	self->eventFlagMemory = npIpc2ClientAlloc(12 * *pNumEventFlag);
	if (self->eventFlagMemory == NULL) {
		result = NP_IPC2_ERROR_OUT_OF_MEMORY;
		eventFlags = self->eventFlags;
	} else {
		eventFlags = (struct NpCommonPs4EventFlag **)npIpc2ClientAlloc(4 * *pNumEventFlag);
		self->eventFlags = eventFlags;
		if (eventFlags == NULL) {
			result = NP_IPC2_ERROR_OUT_OF_MEMORY;
		} else {
			count = *pNumEventFlag;
			for (i = 0; i < count; i++) {
				eventFlag = new ((char *)self->eventFlagMemory + 12 * i) struct NpCommonPs4EventFlag;
				if (eventFlag != NULL)
					eventFlag = SceNpCommonPs4_945AC0FC(eventFlag);
				self->eventFlags[i] = eventFlag;
				count = *pNumEventFlag;
			}
			self->m_numEventFlag = count;
			id = SceNpCommonPs4_A37CE123();
			for (i = 0; i < *pNumEventFlag; i++) {
				sceClibSnprintf(eventFlagName, sizeof(eventFlagName), "%sEvf%02d%08x", name, i, id);
				result = SceNpCommonPs4_BA957ABE(self->eventFlags[i], eventFlagName, 0x1080);
				if (result < 0)
					break;
			}
			if (i >= *pNumEventFlag)
				return 0;
			eventFlags = self->eventFlags;
		}
	}
	if (eventFlags != NULL) {
		i = 0;
		if (*pNumEventFlag != 0) {
			do {
				((void (*)(void *))NP_IPC2_VSLOT(eventFlags[i], 0))(eventFlags[i]);
				i++;
				eventFlags = self->eventFlags;
			} while (i < *pNumEventFlag);
		}
		npIpc2ClientFree(eventFlags);
		self->eventFlags = NULL;
	}
	if (self->eventFlagMemory != NULL) {
		npIpc2ClientFree(self->eventFlagMemory);
		self->eventFlagMemory = NULL;
	}
	return result;
}

int npIpc2ClientEventFlagPoll(struct NpIpc2ClientEventFlag *self, SceUInt32 evfIndex, SceUInt32 bits, int reserved,
	SceUInt32 waitMode, SceUInt64 *pResult)
{
	static const char functionName[] = "Poll";
	int result;

	NP_IPC2_ASSERT(evfIndex < self->m_numEventFlag, functionName, "np_ipc2_client_eventflag.cpp", 135,
		"evfIndex < m_numEventFlag");
	result = SceNpCommonPs4_D6F2A0EC(self->eventFlags[evfIndex], evfIndex, bits, reserved, waitMode, pResult);
	if (result >= 0)
		return 0;
	if (result == NP_COMMON_PS4_ERROR_8055800E)
		return NP_IPC2_ERROR_8055A31B;
	return result;
}

const void *g_npIpc2ClientEventFlagManagerVtable[] = {		/* 0x8102ad9c */
	NULL,
	NULL,
	(const void *)npIpc2ClientEventFlagManagerDestruct,
	(const void *)npIpc2ClientEventFlagManagerDelete,
};

const void *g_npIpc2ClientEventFlagVtable[] = {			/* 0x8102ad8c */
	NULL,
	NULL,
	(const void *)npIpc2ClientEventFlagDestruct,
	(const void *)npIpc2ClientEventFlagDelete,
};

}
