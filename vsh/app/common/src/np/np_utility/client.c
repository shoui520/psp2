/*
 * SceNpUtility: the lookup library's clients. A service's client holds the HTTP template its
 * requests run on, made for one online ID, and the memory its requests own; a client lives
 * until it is deleted and its last reference is released. IDs run from the list's base, the
 * first free one is taken.
 */

#include "np_utility_internal.h"

static NpLookupClientList s_clientLists[NP_LOOKUP_SERVICE_NUM] = {
	{ 0, -1, "SceNpLookupClientInt", 0x00100000, 0x001FFFFF },
	{ 0, -1, "SceNpLookupClientLookup", 0x10000000, 0x10000020 },
	{ 0, -1, "SceNpLookupClientCensor", 0x01000000, 0x01000020 },
};

/* unlinks the client and frees it with everything it owns */
static inline void freeClient(NpLookupClient *client)
{
	NpLookupAllocation *allocation;
	NpLookupAllocation *next;

	if (client->prev != NULL)
		client->prev->next = client->next;
	if (client->next != NULL)
		client->next->prev = client->prev;
	for (allocation = client->allocations; allocation != NULL; allocation = next) {
		next = allocation->next;
		SceNpUtility_6C1161AD(allocation->ptr, client->service);
		SceNpUtility_6C1161AD(allocation, client->service);
	}
	if (client->httpTemplate > 0) {
		sceHttpDeleteTemplate(client->httpTemplate);
		client->httpTemplate = 0;
	}
	sceKernelDeleteMutex(client->mutex);
	SceNpUtility_6C1161AD(client, client->service);
}

/* the HTTP template of the client's requests */
static inline int createTemplate(NpLookupClient *client)
{
	int ret;

	ret = sceHttpCreateTemplate("PSP2Community-agent/1.0.0", SCE_HTTP_VERSION_1_1, SCE_TRUE);
	if (ret < 0) {
		if (client->httpTemplate > 0)
			sceHttpDeleteTemplate(client->httpTemplate);
		return ret;
	}
	client->httpTemplate = ret;
	sceHttpsEnableOptionPrivate(ret, 357);
	sceHttpsDisableOptionPrivate(client->httpTemplate, 24);
	sceHttpSetAcceptEncodingGZIPEnabled(client->httpTemplate, SCE_FALSE);
	return 0;
}

/* gives the client the list's first free ID and links it in ID order */
static inline int addClient(NpLookupClient *client, int service)
{
	NpLookupClientList *list;
	NpLookupClient *prev;
	NpLookupClient *next;
	int id;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_clientLists[service];
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	prev = &list->head;
	id = list->head.id;
	for (next = list->head.next; next != NULL; next = next->next) {
		if (next->id != id + 1)
			break;
		prev = next;
		id = prev->id;
	}
	if (id < list->idMax) {
		ret = id + 1;
		client->prev = prev;
		client->id = ret;
		client->next = prev->next;
		if (prev->next != NULL)
			prev->next->prev = client;
		prev->next = client;
	} else {
		ret = SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
	}
	sceKernelUnlockMutex(list->mutex, 1);
	return ret;
}

int SceNpUtility_17385829(int service)
{
	NpLookupClientList *list;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_clientLists[service];
	if (list->initialized != 0)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;

	sceClibMemset(&list->head, 0, sizeof(list->head));
	list->head.id = list->idBase;
	ret = sceKernelCreateMutex(list->name, SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0)
		return ret;
	list->mutex = ret;
	list->initialized = 1;
	return 0;
}

int SceNpUtility_7A86C37E(int service)
{
	NpLookupClientList *list;
	NpLookupClient *client;
	NpLookupClient *next;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_clientLists[service];
	if (list->initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (client = list->head.next; client != NULL; client = next) {
		next = client->next;
		freeClient(client);
	}
	sceKernelUnlockMutex(list->mutex, 1);
	sceKernelDeleteMutex(list->mutex);
	list->initialized = 0;
	return 0;
}

/* a new client of the online ID, its ID or an error */
int SceNpUtility_F4B21646(const SceNpOnlineId *onlineId, const SceNpId *npId, int service)
{
	NpLookupClient *client;
	char environment[17];
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;

	client = SceNpUtility_8CA24D19(sizeof(NpLookupClient), service);
	if (client == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto error;
	}
	sceClibMemset(client, 0, sizeof(NpLookupClient));
	client->service = service;
	sceClibMemcpy_safe(&client->onlineId, onlineId, sizeof(SceNpOnlineId));
	ret = sceNpCommonGetNpEnviroment(environment);
	if (ret < 0)
		goto error;
	sceClibMemcpy_safe(client->environment, environment, sizeof(client->environment));
	sceClibMemset(client->platform, 0, sizeof(client->platform));
	client->platform[0] = npId->opt[2];
	client->platform[1] = npId->opt[3];

	ret = sceKernelCreateMutex("SceNpLookupClient", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret >= 0)
		client->mutex = ret;
	if (ret < 0)
		goto error;
	ret = createTemplate(client);
	if (ret < 0)
		goto error;
	ret = addClient(client, service);
	if (ret < 0)
		goto error;
	return client->id;

error:
	if (client != NULL) {
		if (client->httpTemplate > 0) {
			sceHttpDeleteTemplate(client->httpTemplate);
			client->httpTemplate = 0;
		}
		if (client->mutex > 0)
			sceKernelDeleteMutex(client->mutex);
		SceNpUtility_6C1161AD(client, client->service);
	}
	return ret;
}

/* drops a reference; a deleted client goes with its last one */
void SceNpUtility_75FD803B(NpLookupClient *client)
{
	NpLookupClientList *list;

	if (client == NULL)
		return;
	if (client->service >= NP_LOOKUP_SERVICE_NUM)
		return;
	list = &s_clientLists[client->service];
	if (sceKernelLockMutex(list->mutex, 1, NULL) < 0)
		return;

	client->refCount--;
	if (client->deleted == 1 && client->refCount == 0)
		freeClient(client);
	sceKernelUnlockMutex(list->mutex, 1);
}

int SceNpUtility_ED2B86ED(int clientId, int service)
{
	NpLookupClientList *list;
	NpLookupClient *client;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_clientLists[service];
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (client = list->head.next; client != NULL; client = client->next) {
		if (client->id == clientId) {
			client->deleted = 1;
			client->refCount++;
			break;
		}
	}
	sceKernelUnlockMutex(list->mutex, 1);
	if (client == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	SceNpUtility_75FD803B(client);
	return 0;
}

/* the client of the ID with a reference taken, unless it is deleted */
int SceNpUtility_F32ED20E(int clientId, NpLookupClient **client, int service)
{
	NpLookupClientList *list;
	NpLookupClient *found;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_clientLists[service];
	*client = NULL;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (found = list->head.next; found != NULL; found = found->next) {
		if (found->id == clientId && found->deleted == 0) {
			ret = 0;
			*client = found;
			found->refCount++;
			break;
		}
	}
	sceKernelUnlockMutex(list->mutex, 1);
	if (found == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ONLINE_ID;
	return ret;
}

/* makes ptr the client's, freed with it */
int SceNpUtility_A15150C9(NpLookupClient *client, void *ptr)
{
	NpLookupClientList *list;
	NpLookupAllocation *allocation;
	int ret;

	if (client == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
	if (client->service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_clientLists[client->service];
	if (ptr == NULL)
		return 0;

	allocation = SceNpUtility_8CA24D19(sizeof(NpLookupAllocation), client->service);
	if (allocation == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	} else {
		ret = sceKernelLockMutex(list->mutex, 1, NULL);
		if (ret >= 0) {
			allocation->ptr = ptr;
			allocation->next = client->allocations;
			client->allocations = allocation;
			sceKernelUnlockMutex(list->mutex, 1);
			return 0;
		}
	}
	if (allocation != NULL)
		SceNpUtility_6C1161AD(allocation, client->service);
	return ret;
}

void SceNpUtility_CD3A3712(NpLookupClient *client)
{
	NpLookupClientList *list;

	if (client == NULL)
		return;
	if (client->service >= NP_LOOKUP_SERVICE_NUM)
		return;
	list = &s_clientLists[client->service];
	if (sceKernelLockMutex(list->mutex, 1, NULL) < 0)
		return;
	client->deleted = 1;
	sceKernelUnlockMutex(list->mutex, 1);
}

int SceNpUtility_944FE83C(int reqId)
{
	return SceNpUtility_C63A0F6B(reqId);
}
