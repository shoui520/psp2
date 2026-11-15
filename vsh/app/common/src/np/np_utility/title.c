/*
 * SceNpUtility: the lookup library's title contexts. A title context is a communication ID and
 * the caller's NP ID on one of the service's clients; its requests take their timeouts from
 * it. It lives until it is deleted and its last reference is released, and holds a reference
 * on its client until then.
 */

#include "np_utility_internal.h"

static NpLookupTitleList s_titleLists[NP_LOOKUP_SERVICE_NUM] = {
	{ 0, -1, "SceNpLookupTitleInt", 0x00400000, 0x004FFFFF },
	{ 0, -1, "SceNpLookupTitleLookup", 0x40000000, 0x40000020 },
	{ 0, -1, "SceNpLookupTitleCensor", 0x04000000, 0x04000020 },
};

int SceNpUtility_E38548AB(SceUInt32 service)
{
	NpLookupTitleList *list;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_titleLists[service];
	if (list->initialized != 0)
		return SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED;

	ret = sceKernelCreateMutex(list->name, SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0)
		return ret;
	list->mutex = ret;
	sceClibMemset(&list->head, 0, sizeof(list->head));
	list->head.id = list->idBase;
	list->initialized = 1;
	return 0;
}

/* drops a reference; a deleted title context goes with its last one, releasing its client */
void SceNpUtility_485F368A(NpLookupTitle *title, SceUInt32 service)
{
	NpLookupTitleList *list;
	int last = 0;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return;
	list = &s_titleLists[service];
	if (title == NULL)
		return;
	if (sceKernelLockMutex(list->mutex, 1, NULL) < 0)
		return;

	title->refCount--;
	if (title->deleted == 1 && title->refCount == 0) {
		if (title->prev != NULL)
			title->prev->next = title->next;
		if (title->next != NULL)
			title->next->prev = title->prev;
		last = 1;
	}
	sceKernelUnlockMutex(list->mutex, 1);
	if (last != 0) {
		SceNpUtility_75FD803B(title->client);
		SceNpUtility_6C1161AD(title, title->service);
	}
}

/* the title context of the ID with a reference taken (Sony's calls it, never inlined) */
__attribute__((noinline)) int SceNpUtility_B9FFB253(int titleId, NpLookupTitle **title, SceUInt32 service)
{
	NpLookupTitleList *list;
	NpLookupTitle *found;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_titleLists[service];
	if (titleId == 0)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (found = list->head.next; found != NULL; found = found->next) {
		if (found->id == titleId) {
			ret = 0;
			*title = found;
			found->refCount++;
			break;
		}
	}
	sceKernelUnlockMutex(list->mutex, 1);
	if (found == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	return ret;
}

int SceNpUtility_4013244C(int titleId, SceUInt32 service)
{
	NpLookupTitleList *list;
	NpLookupTitle *title;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	title = NULL;
	list = &s_titleLists[service];
	if (list->initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;

	ret = SceNpUtility_B9FFB253(titleId, &title, service);
	if (ret < 0)
		return ret;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	title->deleted = 1;
	sceKernelUnlockMutex(list->mutex, 1);
	SceNpUtility_485F368A(title, service);
	return 0;
}

int SceNpUtility_26B4AC00(SceUInt32 service)
{
	NpLookupTitleList *list;
	NpLookupTitle *title;
	NpLookupTitle *next;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_titleLists[service];
	if (list->initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (title = list->head.next; title != NULL; title = next) {
		next = title->next;
		SceNpUtility_4013244C(title->id, title->service);
	}
	sceKernelUnlockMutex(list->mutex, 1);
	sceKernelDeleteMutex(list->mutex);
	list->initialized = 0;
	return 0;
}

/* gives the title context the list's first free ID and links it in ID order */
static inline int addTitle(NpLookupTitle *title, SceUInt32 service)
{
	NpLookupTitleList *list;
	NpLookupTitle *prev;
	NpLookupTitle *next;
	int id;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_titleLists[service];
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
		title->prev = prev;
		title->id = ret;
		title->next = prev->next;
		if (prev->next != NULL)
			prev->next->prev = title;
		prev->next = title;
	} else {
		ret = SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
	}
	sceKernelUnlockMutex(list->mutex, 1);
	return ret;
}

/* a new title context of commId and npId on the client, its ID or an error */
int SceNpUtility_44A2F444(const SceNpCommunicationId *commId, const SceNpId *npId, int clientId,
			  SceUInt32 service)
{
	NpLookupTitleList *list;
	NpLookupTitle *title;
	NpLookupClient *client;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	client = NULL;
	list = &s_titleLists[service];
	if (list->initialized != 1) {
		ret = SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
		goto error;
	}
	if (npId == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
		goto error;
	}

	title = SceNpUtility_8CA24D19(sizeof(NpLookupTitle), service);
	if (title == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto free;
	}
	sceClibMemset(title, 0, sizeof(NpLookupTitle));
	title->service = service;
	title->npId = *npId;
	title->npId.handle.term = '\0';
	if (commId != NULL) {
		title->commId = *commId;
		title->commId.term = '\0';
	}
	title->resolveRetry = 0;
	title->resolveTimeout = 0;
	title->connectTimeout = 0;
	title->sendTimeout = 0;
	title->recvTimeout = 0;

	ret = addTitle(title, service);
	if (ret >= 0) {
		ret = SceNpUtility_F32ED20E(clientId, &client, service);
		if (ret >= 0) {
			title->client = client;
			return title->id;
		}
	}
free:
	if (title != NULL)
		SceNpUtility_6C1161AD(title, service);
error:
	if (client != NULL)
		SceNpUtility_75FD803B(client);
	return ret;
}

/* the title context of the ID in whichever service its ID range is, without a reference */
int SceNpUtility_D03605CD(int titleId, NpLookupTitle **title)
{
	NpLookupTitleList *list;
	NpLookupTitle *found;
	int service;
	int ret;

	if ((titleId & 0xF00000) == 0x400000)
		service = NP_LOOKUP_SERVICE_INT;
	else if ((titleId & 0xF0000000) == 0x40000000)
		service = NP_LOOKUP_SERVICE_LOOKUP;
	else if ((titleId & 0xF000000) == 0x4000000)
		service = NP_LOOKUP_SERVICE_CENSOR;
	else
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_titleLists[service];
	if (list->initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	if (titleId == 0)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (found = list->head.next; found != NULL; found = found->next) {
		if (found->id == titleId) {
			ret = 0;
			*title = found;
			break;
		}
	}
	sceKernelUnlockMutex(list->mutex, 1);
	if (found == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	return ret;
}
