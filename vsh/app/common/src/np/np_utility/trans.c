/*
 * SceNpUtility: the lookup library's transactions. A transaction is one request of a title
 * context, with that context's timeouts and a reference on it; its ID is the request ID the
 * caller polls. Its result is set when the request ends, and it lives until it is deleted
 * and its last reference is released.
 */

#include "np_utility_internal.h"

static NpLookupTransList s_transLists[NP_LOOKUP_SERVICE_NUM] = {
	{ 0, -1, "SceNpLookupTransInt", 0x00200000, 0x002FFFFF },
	{ 0, -1, "SceNpLookupTrans", 0x20000000, 0x20000020 },
	{ 0, -1, "SceNpLookupTransCensor", 0x02000000, 0x02000020 },
};

/* what 16954FFF reports and 5D6FC99E sets (the community's) */
static int s_state;

/* frees what the transaction owns, then the transaction */
static inline void freeTrans(NpLookupTrans *trans)
{
	NpLookupAllocation *allocation;
	NpLookupAllocation *next;

	allocation = trans->allocations;
	trans->allocations = NULL;
	for (; allocation != NULL; allocation = next) {
		next = allocation->next;
		SceNpUtility_6C1161AD(allocation->ptr, trans->service);
		SceNpUtility_6C1161AD(allocation, trans->service);
	}
	SceNpUtility_6C1161AD(trans, trans->service);
}

int SceNpUtility_9F142A92(int service)
{
	NpLookupTransList *list;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[service];
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

/* aborts the service's requests, the first 32 of them */
int SceNpUtility_200BBAC5(int service)
{
	NpLookupTransList *list;
	NpLookupTrans *trans;
	int reqIds[32];
	int count = 0;
	int ret;
	int i;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[service];
	for (i = 0; i < 32; i++)
		reqIds[i] = 0;
	if (list->initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (trans = list->head.next; trans != NULL; trans = trans->next) {
		reqIds[count++] = trans->id;
		if (count >= 32)
			break;
	}
	sceKernelUnlockMutex(list->mutex, 1);
	for (i = 0; i < count; i++)
		SceNpUtility_C63A0F6B(reqIds[i]);
	return ret;
}

/* drops a reference; a deleted transaction goes with its last one */
void SceNpUtility_31D3542E(NpLookupTrans *trans)
{
	NpLookupTransList *list;

	if (trans->service >= NP_LOOKUP_SERVICE_NUM)
		return;
	list = &s_transLists[trans->service];
	if (sceKernelLockMutex(list->mutex, 1, NULL) < 0)
		return;

	trans->refCount--;
	if (trans->deleted == 1 && trans->refCount == 0) {
		SceNpUtility_866D737A(trans);
		SceNpUtility_485F368A(trans->title, trans->service);
		freeTrans(trans);
	}
	sceKernelUnlockMutex(list->mutex, 1);
}

/* the transaction of the request ID with a reference taken */
int SceNpUtility_25D26B6F(int reqId, NpLookupTrans **trans)
{
	NpLookupTransList *list;
	NpLookupTrans *found;
	int service;
	int ret;

	if ((reqId & 0xF00000) == 0x200000)
		service = NP_LOOKUP_SERVICE_INT;
	else if ((reqId & 0xF0000000) == 0x20000000)
		service = NP_LOOKUP_SERVICE_LOOKUP;
	else if ((reqId & 0xF000000) == 0x2000000)
		service = NP_LOOKUP_SERVICE_CENSOR;
	else
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[service];
	*trans = NULL;
	if (reqId == 0)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (found = list->head.next; found != NULL; found = found->next) {
		if (found->id == reqId) {
			ret = 0;
			*trans = found;
			found->refCount++;
			break;
		}
		if (found->id > reqId) {
			*trans = NULL;
			ret = SCE_NP_COMMUNITY_ERROR_INVALID_ID;
			break;
		}
	}
	sceKernelUnlockMutex(list->mutex, 1);
	if (found == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	return ret;
}

/* aborts the request, unlinks its transaction and lets it go */
int SceNpUtility_D5BB8464(int reqId, SceUInt32 service)
{
	NpLookupTransList *list;
	NpLookupTrans *trans;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[service];
	trans = NULL;
	ret = SceNpUtility_25D26B6F(reqId, &trans);
	if (ret < 0)
		goto error;
	if (trans->service != service) {
		ret = SCE_NP_COMMUNITY_ERROR_INVALID_ID;
		goto error;
	}
	SceNpUtility_C63A0F6B(reqId);
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		goto error;

	if (trans->prev != NULL)
		trans->prev->next = trans->next;
	if (trans->next != NULL)
		trans->next->prev = trans->prev;
	trans->deleted = 1;
	sceKernelUnlockMutex(list->mutex, 1);
	if (trans != NULL)
		SceNpUtility_31D3542E(trans);
	return 0;

error:
	if (trans != NULL)
		SceNpUtility_31D3542E(trans);
	return ret;
}

int SceNpUtility_6DF884DA(int service)
{
	NpLookupTransList *list;
	NpLookupTrans *trans;
	NpLookupTrans *next;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[service];
	if (list->initialized != 1)
		return SCE_NP_COMMUNITY_ERROR_NOT_INITIALIZED;
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (trans = list->head.next; trans != NULL; trans = next) {
		next = trans->next;
		SceNpUtility_D5BB8464(trans->id, service);
	}
	sceKernelUnlockMutex(list->mutex, 1);
	sceKernelDeleteMutex(list->mutex);
	list->initialized = 0;
	return 0;
}

/* makes ptr the transaction's, freed with it */
int SceNpUtility_46543307(NpLookupTrans *trans, void *ptr)
{
	NpLookupTransList *list;
	NpLookupAllocation *allocation;
	int ret;

	if (trans->service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[trans->service];
	if (ptr == NULL)
		return 0;

	allocation = SceNpUtility_8CA24D19(sizeof(NpLookupAllocation), trans->service);
	if (allocation == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
	} else {
		ret = sceKernelLockMutex(list->mutex, 1, NULL);
		if (ret >= 0) {
			allocation->ptr = ptr;
			allocation->next = trans->allocations;
			trans->allocations = allocation;
			sceKernelUnlockMutex(list->mutex, 1);
			return 0;
		}
	}
	if (allocation != NULL)
		SceNpUtility_6C1161AD(allocation, trans->service);
	return ret;
}

/* gives the transaction the list's first free ID and links it in ID order */
static inline int addTrans(NpLookupTrans *trans, SceUInt32 service)
{
	NpLookupTransList *list;
	NpLookupTrans *prev;
	NpLookupTrans *next;
	int id;
	int ret;

	if (service >= NP_LOOKUP_SERVICE_NUM)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[service];
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
		trans->prev = prev;
		trans->id = ret;
		trans->next = prev->next;
		if (prev->next != NULL)
			prev->next->prev = trans;
		prev->next = trans;
	} else {
		ret = SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
	}
	sceKernelUnlockMutex(list->mutex, 1);
	return ret;
}

/*
 * A new transaction of the operation on the title context, its request ID or an error. It
 * takes *data, which is then the transaction's.
 */
int SceNpUtility_A48B44C9(NpLookupClient *client, int operation, SceUInt32 service, void **data,
			  int titleId)
{
	NpLookupTitle *title = NULL;
	NpLookupTitle *context;
	NpLookupTrans *trans;
	void *ptr = *data;
	int ret;

	trans = SceNpUtility_8CA24D19(sizeof(NpLookupTrans), client->service);
	if (trans == NULL) {
		ret = SCE_NP_COMMUNITY_ERROR_OUT_OF_MEMORY;
		goto error;
	}
	sceClibMemset(trans, 0, sizeof(NpLookupTrans));
	trans->operation = operation;
	trans->service = service;
	trans->data = ptr;
	ret = SceNpUtility_B9FFB253(titleId, &title, client->service);
	if (ret < 0)
		goto error;

	context = title;
	trans->result = 0;
	trans->state = NP_LOOKUP_TRANS_IDLE;
	trans->title = context;
	trans->resolveRetry = context->resolveRetry;
	trans->resolveTimeout = context->resolveTimeout;
	trans->connectTimeout = context->connectTimeout;
	trans->sendTimeout = context->sendTimeout;
	trans->recvTimeout = context->recvTimeout;
	if (ptr != NULL) {
		ret = SceNpUtility_46543307(trans, ptr);
		if (ret < 0)
			goto error;
	}
	*data = NULL;
	ret = addTrans(trans, service);
	if (ret >= 0)
		return trans->id;

error:
	if (title != NULL)
		SceNpUtility_485F368A(title, client->service);
	if (trans != NULL)
		freeTrans(trans);
	return ret;
}

/* the request's result: it is done */
int SceNpUtility_F5086697(int reqId, int result)
{
	NpLookupTransList *list;
	NpLookupTrans *trans;
	int service;
	int ret;

	if ((reqId & 0xF00000) == 0x200000)
		service = NP_LOOKUP_SERVICE_INT;
	else if ((reqId & 0xF0000000) == 0x20000000)
		service = NP_LOOKUP_SERVICE_LOOKUP;
	else if ((reqId & 0xF000000) == 0x2000000)
		service = NP_LOOKUP_SERVICE_CENSOR;
	else
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[service];
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (trans = list->head.next; trans != NULL; trans = trans->next) {
		if (trans->id == reqId) {
			trans->result = result;
			ret = 0;
			trans->state = NP_LOOKUP_TRANS_DONE;
			break;
		}
		if (trans->id > reqId) {
			ret = SCE_NP_COMMUNITY_ERROR_INVALID_ID;
			trans = NULL;
			break;
		}
	}
	sceKernelUnlockMutex(list->mutex, 1);
	if (trans == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	return ret;
}

/*
 * 0 with the request's result once it is done, 1 while it runs. A request never queued is
 * still running for an application built with SDK 3.30 or later, an invalid ID before.
 */
int SceNpUtility_8904FE15(int reqId, int *result)
{
	NpLookupTransList *list;
	NpLookupTrans *trans;
	int value = 0;
	SceUInt32 state = NP_LOOKUP_TRANS_IDLE;
	int service;
	int ret;

	if ((reqId & 0xF00000) == 0x200000)
		service = NP_LOOKUP_SERVICE_INT;
	else if ((reqId & 0xF0000000) == 0x20000000)
		service = NP_LOOKUP_SERVICE_LOOKUP;
	else if ((reqId & 0xF000000) == 0x2000000)
		service = NP_LOOKUP_SERVICE_CENSOR;
	else
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	list = &s_transLists[service];
	ret = sceKernelLockMutex(list->mutex, 1, NULL);
	if (ret < 0)
		return ret;

	for (trans = list->head.next; trans != NULL; trans = trans->next) {
		if (trans->id == reqId) {
			ret = 0;
			value = trans->result;
			state = trans->state;
			break;
		}
		if (trans->id > reqId) {
			ret = SCE_NP_COMMUNITY_ERROR_INVALID_ID;
			trans = NULL;
			break;
		}
	}
	sceKernelUnlockMutex(list->mutex, 1);
	if (ret < 0)
		return ret;
	if (trans == NULL)
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;

	switch (state) {
	case NP_LOOKUP_TRANS_DONE:
		*result = value;
		return 0;
	case NP_LOOKUP_TRANS_IDLE:
		if (SceLibKernel_9F793F84() >= 0x03300000)
			return 1;
		return SCE_NP_COMMUNITY_ERROR_INVALID_ID;
	}
	return 1;
}

int SceNpUtility_16954FFF(void)
{
	return s_state;
}

void SceNpUtility_5D6FC99E(int state)
{
	s_state = state;
}

/* NP full test: the requests are slowed down */
SceBool SceNpUtility_2835BC9B(void)
{
	return SceNpCommon_BBC13621();
}

void SceNpUtility_191D314C(void)
{
	sceKernelDelayThread(1000000);
}
