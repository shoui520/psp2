/*
 * SceNpWebApi: the push events the library context receives from the NP service (Sony's
 * np_push/src/np_push_lib_context.cpp and the IPC calls it makes), and how it hands them on to the
 * callbacks registered with it. The names are ours.
 */

#include "np_webapi_internal.h"

static volatile int g_pushCallbackCounter;	/* 0x8101003C */

#define NP_PUSH_RECEIVE_BUFFER_SIZE	0x2000
#define NP_PUSH_RECEIVE_EXDATA_MAX	10
#define NP_PUSH_FLAG_ACTIVE		1

/* asks the service for the header of the next event: where it is from and to, what it is, and its type */
int npPushIpcReceiveHeader(NpWebApiIpmiClient *client, unsigned char *id, const void *to, const void *from,
	const void *extra, const char *dataType)
{
	NpWebApiIpmiOut out[5];
	int ret;
	int result;

	ret = 0;
	out[0].data = id;
	out[0].size = 4;
	out[1].data = (void *)to;
	out[1].size = 32;
	out[2].data = (void *)from;
	out[2].size = 32;
	out[3].data = (void *)extra;
	out[3].size = 12;
	out[4].data = (void *)dataType;
	out[4].size = 65;
	result = client->vtbl->invoke(client, 0x10002, NULL, 0, &ret, out, 5);
	if (result == 0)
		result = ret;
	return result;
}

/* and its data: the names of the extended data wanted go in, the packet comes out */
int npPushIpcReceiveEvent(NpWebApiIpmiClient *client, int zero, const void *names, int nameCount, const void *to,
	const void *from, const void *extra, const char *dataType, void *buffer, SceSize size, int *length)
{
	NpWebApiIpmiIn in[2];
	NpWebApiIpmiOut out[6];
	int header[3];
	int ret;
	int result;

	ret = 0;
	header[0] = zero;
	in[0].data = header;
	in[0].size = 12;
	in[1].data = names;
	in[1].size = 33 * nameCount;
	header[1] = nameCount;
	out[0].data = (void *)to;
	out[0].size = 32;
	out[1].data = (void *)from;
	out[1].size = 32;
	out[2].data = (void *)extra;
	out[2].size = 12;
	out[3].data = (void *)dataType;
	out[3].size = 65;
	out[4].data = buffer;
	out[4].size = size;
	header[2] = size;
	out[5].data = length;
	out[5].size = 4;
	result = client->vtbl->invoke(client, 0x10003, in, 2, &ret, out, 6);
	if (result == 0)
		result = ret;
	return result;
}

int npPushIpcReceiveServiceEvent(NpWebApiIpmiClient *client, int zero, const void *names, int nameCount,
	const void *to, const void *from, const void *extra, const char *dataType, void *buffer, SceSize size,
	int *length)
{
	NpWebApiIpmiIn in[2];
	NpWebApiIpmiOut out[6];
	int header[3];
	int ret;
	int result;

	ret = 0;
	header[0] = zero;
	in[0].data = header;
	in[0].size = 12;
	in[1].data = names;
	in[1].size = 33 * nameCount;
	header[1] = nameCount;
	out[0].data = (void *)to;
	out[0].size = 32;
	out[1].data = (void *)from;
	out[1].size = 32;
	out[2].data = (void *)extra;
	out[2].size = 12;
	out[3].data = (void *)dataType;
	out[3].size = 65;
	out[4].data = buffer;
	out[4].size = size;
	header[2] = size;
	out[5].data = length;
	out[5].size = 4;
	result = client->vtbl->invoke(client, 0x10007, in, 2, &ret, out, 6);
	if (result == 0)
		result = ret;
	return result;
}

/* the names of the extended data the services listening for a type want, taken into a list (made when
   the first is added) the caller holds a reference to */
int npPushServicesCollectNames(NpWebApiExtdEntryList *services, const char *name, NpPushExDataNames **pNames)
{
	NpWebApiExtdEntry *entry;
	NpWebApiExtdName *extdName;
	NpPushExDataNames *names;
	int result;

	entry = services->head;
	if (entry == NULL)
		return 0;
	while (1) {
		if (npPushServiceNameEquals(entry->service, name) != 0) {
			names = *pNames;
			if (names != NULL)
				SceNpCommonPs4_64BAC888(names);
			extdName = entry->head;
			if (extdName != NULL) {
				while (1) {
					if (npPushExDataNamesContain(names, extdName->name) == 0) {
						result = npPushExDataNamesAdd(names, extdName->name);
						if (result < 0)
							break;
					}
					extdName = extdName->next;
					if (extdName == NULL)
						goto done;
				}
			} else {
done:
				result = 0;
			}
			if (names != NULL)
				SceNpCommonPs4_93C97239(names);
			if (result < 0)
				return result;
		}
		entry = entry->next;
		if (entry == NULL)
			return 0;
	}
}

int npPushLibContextCollectNames(NpPushLibContext *ctx, const char *name, NpPushExDataNames **pNames)
{
	NpPushExDataNames *names;
	NpPushExDataNames *current;
	void *lock;
	int result;

	lock = ctx->mutex;
	SceNpCommonPs4_A2A8493B(lock);
	npPushExDataNamesClear(ctx->exDataNames);
	names = ctx->exDataNames;
	if (names != NULL)
		SceNpCommonPs4_64BAC888(names);
	result = npPushServicesCollectNames(&ctx->services, name, &names);
	if (names != NULL)
		SceNpCommonPs4_93C97239(names);
	if (result >= 0) {
		current = *pNames;
		names = ctx->exDataNames;
		if (names != current) {
			if (current != NULL)
				SceNpCommonPs4_93C97239(current);
			*pNames = names;
			if (names != NULL)
				SceNpCommonPs4_64BAC888(names);
		}
		SceNpCommonPs4_6BA5A164(lock);
		result = 0;
	} else {
		SceNpCommonPs4_6BA5A164(lock);
	}
	return result;
}

/* receives an event's header and data into the buffer */
int npPushLibContextReceive(NpPushLibContext *ctx, void *to, void *from, void *extra, char *dataType, void *buffer,
	SceSize size, int *length)
{
	NpPushExDataNames *names;
	unsigned char id[4];
	char *namesArray;
	int nameCount;
	int result;

	if ((ctx->flags & NP_PUSH_FLAG_ACTIVE) == 0)
		return NP_PUSH_ERROR_NOT_READY;
	result = SceNpCommonPs4_6B1D3984(0, 0, 1, 0, 5, id);
	if (result == NP_PUSH_STATE_NOT_READY)
		return NP_PUSH_ERROR_NOT_READY;
	if (result < 0)
		return result;
	namesArray = NULL;
	nameCount = 0;
	if (npPushServiceListIsEmpty(&ctx->services) == 0) {
		result = npPushIpcReceiveHeader(SceNpCommonPs4_A0599F24(), id, to, from, extra, dataType);
		if (result < 0)
			return result;
		names = NULL;
		result = npPushLibContextCollectNames(ctx, dataType, &names);
		if (result < 0) {
			if (names != NULL)
				SceNpCommonPs4_93C97239(names);
			return result;
		}
		if (names != NULL) {
			namesArray = (char *)names->names;
			nameCount = names->count;
			SceNpCommonPs4_93C97239(names);
		}
	}
	sceClibMemset(buffer, 0, size);
	result = npPushIpcReceiveEvent(SceNpCommonPs4_A0599F24(), 0, namesArray, nameCount, to, from, extra, dataType,
		buffer, size, length);
	if (result >= 0)
		result = 0;
	return result;
}

/* hands an event to the callbacks registered for events */
void npPushLibContextDispatchEvent(NpPushLibContext *ctx, const void *to, const void *from, const void *extra,
	const char *dataType, const char *data, SceSize dataLength, const NpPushExDataArgument *exData, int exDataNum)
{
	NpPushCallbackNode *sentinel;
	NpPushCallbackNode *node;

	sentinel = ctx->callbacks.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			((NpPushEventCallback)node->callback)(to, from, extra, dataType, data, dataLength, exData,
				exDataNum, node->argument);
			node = node->next;
		} while (node != ctx->callbacks.sentinel);
	}
}

/* and to the ones registered for a service */
void npPushLibContextDispatchServiceEvent(NpPushLibContext *ctx, const void *to, const void *from,
	const char *service, const char *data, SceSize dataLength)
{
	NpPushServiceCallbackNode *sentinel;
	NpPushServiceCallbackNode *node;

	sentinel = ctx->serviceCallbacks.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			if (npPushServiceNameMatches(node->service, service, node->exact == 1) != 0)
				((NpPushServiceEventCallback)node->callback)(to, from, service, data, dataLength,
					node->argument);
			node = node->next;
		} while (node != ctx->serviceCallbacks.sentinel);
	}
}

void npPushLibContextReceivePush(NpPushLibContext *ctx)
{
	NpPushExData exData;
	NpPushExDataSlot *dummy;
	unsigned char to[32];
	unsigned char fromBuffer[32];
	unsigned char extra[12];
	unsigned char zero[12];
	char dataType[68];
	unsigned int consumed;
	int length;
	int exDataNum;
	const void *from;
	int result;

	exData.data = NULL;
	sceClibMemset(exData.slots, 0, sizeof(exData.slots));
	exDataNum = 0;
	from = NULL;
	if (npPushLibContextReceive(ctx, to, fromBuffer, extra, dataType, ctx->receiveBuffer,
		NP_PUSH_RECEIVE_BUFFER_SIZE, &length) == 0) {
		result = npPushExDataParse(&exData, (char *)ctx->receiveBuffer, length, &consumed);
		if (result >= 0)
			result = npPushExDataGather(&exData, (NpPushExDataArgument *)ctx->exDataBuffer,
				NP_PUSH_RECEIVE_EXDATA_MAX, &exDataNum);
		if (result == 0) {
			sceClibMemset(zero, 0, 12);
			if (sceClibMemcmp(extra, zero, 12) != 0) {
				if (*(unsigned long long *)fromBuffer != 0)
					from = fromBuffer;
				npPushLibContextDispatchEvent(ctx, to, from, extra, dataType, npPushExDataGetData(&exData),
					npPushExDataGetDataLength(&exData), (NpPushExDataArgument *)ctx->exDataBuffer,
					exDataNum);
			} else {
				if (*(unsigned long long *)fromBuffer != 0)
					from = fromBuffer;
				npPushLibContextDispatchEvent(ctx, to, from, NULL, dataType, npPushExDataGetData(&exData),
					npPushExDataGetDataLength(&exData), (NpPushExDataArgument *)ctx->exDataBuffer,
					exDataNum);
			}
		}
	}
	(void)dummy;
}

void npPushLibContextReceiveService(NpPushLibContext *ctx)
{
	NpPushExData exData;
	unsigned char to[32];
	unsigned char fromBuffer[32];
	unsigned char extra[12];
	unsigned char zero[12];
	char dataType[68];
	void *buffer;
	unsigned int consumed;
	int length;
	unsigned int flags;
	unsigned char id[8];
	const void *from;

	exData.data = NULL;
	buffer = ctx->receiveBuffer;
	flags = ctx->flags;
	from = NULL;
	sceClibMemset(exData.slots, 0, sizeof(exData.slots));
	if ((flags & NP_PUSH_FLAG_ACTIVE) != 0 && SceNpCommonPs4_6B1D3984(0, 0, 2, 0, 5, id) >= 0) {
		sceClibMemset(buffer, 0, NP_PUSH_RECEIVE_BUFFER_SIZE);
		if (npPushIpcReceiveServiceEvent(SceNpCommonPs4_A0599F24(), 0, NULL, 0, to, fromBuffer, extra, dataType,
			buffer, NP_PUSH_RECEIVE_BUFFER_SIZE, &length) >= 0
			&& npPushExDataParse(&exData, (char *)ctx->receiveBuffer, length, &consumed) == 0) {
			sceClibMemset(zero, 0, 12);
			if (sceClibMemcmp(extra, zero, 12) == 0) {
				if (*(unsigned long long *)fromBuffer != 0)
					from = fromBuffer;
				npPushLibContextDispatchServiceEvent(ctx, to, from, dataType, npPushExDataGetData(&exData),
					npPushExDataGetDataLength(&exData));
			}
		}
	}
}

int npPushLibContextCheckCallback(NpPushLibContext *ctx)
{
	void *lock;

	lock = ctx->receiveLock;
	SceNpCommonPs4_A2A8493B(lock);
	npPushLibContextReceivePush(ctx);
	npPushLibContextReceiveService(ctx);
	SceNpCommonPs4_6BA5A164(lock);
	return 0;
}

int npPushCheckCallback(void)
{
	NpPushLibContext *ctx;

	ctx = npPushLibContextGet();
	if (ctx == NULL)
		return NP_PUSH_ERROR_NO_LIB_CONTEXT;
	npPushLibContextCheckCallback(ctx);
	return 0;
}

int npWebApiCheckCallback(void)
{
	return npPushCheckCallback();
}

/* the list's insert: a ring around a sentinel node made when the first comes */
void npPushCallbackListInsert(NpPushCallbackNode **pResult, NpPushCallbackList *list, const NpPushCallbackRecord *record)
{
	NpPushCallbackNode *sentinel;
	NpPushCallbackNode *node;
	NpPushCallbackNode *created;
	void *memory;
	void *callback;
	void *argument;
	int id;

	if (list->sentinel == NULL) {
		created = NULL;
		if (list->allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(list->allocator, 20);
			if (memory != NULL) {
				sentinel = (NpPushCallbackNode *)operator new(20, memory);
				if (sentinel != NULL) {
					sentinel->callback = NULL;
					sentinel->argument = NULL;
					sentinel->id = 0;
					created = sentinel;
				}
			}
		}
		list->sentinel = created;
		if (created == NULL) {
			*pResult = NULL;
			return;
		}
		created->prev = created;
		created->next = created;
	}
	callback = record->callback;
	argument = record->argument;
	id = record->id;
	if (list->allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(list->allocator, 20);
		if (memory != NULL) {
			node = (NpPushCallbackNode *)operator new(20, memory);
			if (node != NULL) {
				node->callback = callback;
				node->argument = argument;
				node->prev = list->sentinel->prev;
				node->id = id;
				node->next = list->sentinel;
				sentinel = list->sentinel;
				sentinel->prev->next = node;
				list->count++;
				list->sentinel->prev = node;
				*pResult = node;
				return;
			}
		}
	}
	*pResult = NULL;
}

int npPushCallbackListErase(NpPushCallbackNode **pResult, NpPushCallbackList *list, NpPushCallbackNode **pAt)
{
	NpPushCallbackNode *node;
	NpPushCallbackNode *next;
	NpPushCallbackNode *prev;
	int count;

	node = *pAt;
	next = node->next;
	prev = next;
	if (node == list->sentinel) {
		SceNpCommonPs4_F50BD96A(sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "erase",
			"../../../../../app/common/src/np/include/np_common_ps4/npext/list.h", 364, "HALT_ASSERT",
			"at != end()"));
		prev = node->next;
	}
	prev->prev = node->prev;
	node->prev->next = node->next;
	if (list->allocator != NULL)
		SceNpCommonPs4_6B428829(list->allocator, node);
	count = list->count - 1;
	list->count = count;
	*pResult = next;
	return count;
}

/* adds a callback (the library's own users' dispatcher) with a fresh ID */
int npPushLibContextAddCallback(NpPushLibContext *ctx, void *callback, void *argument, int *pId)
{
	NpPushCallbackRecord record;
	NpPushCallbackNode *node;
	int id;

	SceNpCommonPs4_A2A8493B(ctx->receiveLock);
	record.callback = callback;
	record.argument = argument;
	id = SceNpCommonPs4_741F88FE(&g_pushCallbackCounter) + 1;
	if (id == 0)
		id = SceNpCommonPs4_741F88FE(&g_pushCallbackCounter) + 1;
	record.id = id;
	npPushCallbackListInsert(&node, &ctx->callbacks, &record);
	if (node != NULL) {
		*pId = record.id;
		SceNpCommonPs4_6BA5A164(ctx->receiveLock);
		return 0;
	}
	SceNpCommonPs4_6BA5A164(ctx->receiveLock);
	return -2141871612;
}

int npPushLibContextRemoveCallback(NpPushLibContext *ctx, int id)
{
	NpPushCallbackNode *sentinel;
	NpPushCallbackNode *result;
	NpPushCallbackNode *node;

	SceNpCommonPs4_A2A8493B(ctx->receiveLock);
	sentinel = ctx->callbacks.sentinel;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		while (node->id != id) {
			node = node->next;
			if (node == sentinel)
				goto notFound;
		}
		npPushCallbackListErase(&result, &ctx->callbacks, &node);
		SceNpCommonPs4_6BA5A164(ctx->receiveLock);
		return 0;
	}
notFound:
	SceNpCommonPs4_6BA5A164(ctx->receiveLock);
	return -2141871613;
}

int npPushLibContextRegisterDispatch(void (*dispatch)(const struct NpPushPeer *, const struct NpPushPeer *, const char *,
	const char *, const void *, SceSize, const struct NpPushExDataArgument *, int), int a2, int *pId)
{
	NpPushLibContext *ctx;
	int result;

	ctx = npPushLibContextGet();
	if (ctx == NULL)
		return -2141871614;
	result = npPushLibContextAddCallback(ctx, (void *)dispatch, (void *)a2, pId);
	if (result >= 0)
		return 0;
	return result;
}

int npPushLibContextUnregisterDispatch(int id)
{
	NpPushLibContext *ctx;
	int result;

	ctx = npPushLibContextGet();
	if (ctx == NULL)
		return -2141871614;
	result = npPushLibContextRemoveCallback(ctx, id);
	if (result >= 0)
		return 0;
	return result;
}
