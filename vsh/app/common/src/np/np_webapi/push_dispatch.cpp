/*
 * SceNpWebApi: the push event dispatcher. The library's receive path calls npPushDispatch for each event; it
 * copies the event for each library context and hands it to the callbacks of the user contexts the event is
 * for. The names are ours.
 */

#include "np_webapi_internal.h"

typedef void (*PushEventCallbackFn)(int, SceNpPeerAddress *, SceNpPeerAddress *, const char *, const void *, SceSize,
	void *);
typedef void (*ServiceEventCallbackFn)(int, const char *, int, SceNpPeerAddress *, SceNpPeerAddress *, const char *,
	const void *, SceSize, void *);
typedef void (*ServiceEventCallbackExFn)(int, const char *, int, const char *, SceNpPeerAddress *,
	SceNpPeerAddress *, const char *, const void *, SceSize, void *);
typedef void (*ExtdEventCallbackFn)(int, const char *, int, SceNpPeerAddress *, SceNpPeerAddress *, const char *,
	const void *, SceSize, SceNpWebApiExtdPushEventExtdData *, SceSize, void *);
typedef void (*LegacyCallbackFn)(int, SceNpPeerAddress *, SceNpPeerAddress *, char *, void *, SceSize, int);

int npWebApiUserContextHandleHasNext(NpWebApiUserContextHandle *handle)
{
	if (handle->reserved != 0)
		return npWebApiUserContextNext((NpWebApiUserContext *)handle->reserved) != NULL;
	return handle->userContext != NULL;
}

NpWebApiUserContext *npWebApiUserContextHandleNext(NpWebApiUserContextHandle *handle)
{
	NpWebApiUserContext *current;
	NpWebApiUserContext *next;

	current = (NpWebApiUserContext *)handle->reserved;
	if (current != NULL)
		next = npWebApiUserContextNext(current);
	else
		next = handle->userContext;
	handle->reserved = (int)next;
	return next;
}

NpWebApiLibContext *npWebApiGetFirstLibContext(void)
{
	return g_libContexts;
}

NpWebApiLibContext *npWebApiLibContextNext(NpWebApiLibContext *ctx)
{
	return ctx->next;
}

/* the filter listens to the data type: one of its types starts the type */
int npPushFilterContainsType(const NpWebApiPushFilter *filter, const char *dataType)
{
	int found;
	unsigned int i;
	const char *type;
	int offset;

	found = 0;
	i = 0;
	if (filter->typeCount != 0) {
		offset = 0;
		do {
			type = (const char *)filter->types + offset;
			if (sceClibStrncmp(type, dataType, sceClibStrnlen(type, 0xFFFFFFFF)) == 0)
				found = 1;
			i++;
			offset += 65;
		} while (i < filter->typeCount);
	}
	return found;
}

/* a service filter takes the service (its name and label) and the data type */
int npWebApiServiceFilterMatches(const NpWebApiServiceFilter *filter, const char *service, const char *dataType)
{
	const NpWebApiHandleNode *sentinel;
	const NpWebApiHandleNode *node;
	unsigned int count;
	int offset;
	const char *type;
	int result;

	result = 0;
	if (filter->all == 0) {
		sentinel = filter->list.sentinel;
		node = NULL;
		if (sentinel != NULL)
			node = sentinel->next;
		if (node == sentinel)
			return result;
		while (sceClibStrncmp((const char *)node + 8, service, 9) != 0 ||
			((const unsigned char *)node)[18] != ((const unsigned char *)service)[10]) {
			node = node->next;
			if (node == filter->list.sentinel)
				return result;
		}
	}
	count = 0;
	if (filter->typeCount != 0) {
		offset = 0;
		do {
			type = (const char *)filter->types + offset;
			if (sceClibStrncmp(type, dataType, sceClibStrnlen(type, 0xFFFFFFFF)) == 0) {
				result = 1;
				break;
			}
			offset += 65;
			count++;
		} while (count < filter->typeCount);
	}
	return result;
}

/* the same for an extended filter: the record of the data type is given back */
int npWebApiExtdFilterMatches(const NpWebApiExtdFilter *filter, const char *service, const char *dataType,
	NpWebApiExtdFilterRecord **pRecord)
{
	const NpWebApiHandleNode *sentinel;
	const NpWebApiHandleNode *node;
	unsigned int i;
	int offset;
	const char *type;
	int found;

	found = 0;
	if (filter->all == 0) {
		if (sceClibStrnlen(filter->name, 0xFFFFFFFF) == 0 && filter->handle == -1) {
			if (service != NULL)
				return found;
		} else {
			if (service == NULL)
				return found;
			sentinel = filter->list.sentinel;
			node = NULL;
			if (sentinel != NULL)
				node = sentinel->next;
			if (node == sentinel)
				return found;
			while (sceClibStrncmp((const char *)node + 8, service, 9) != 0 ||
				((const unsigned char *)node)[18] != ((const unsigned char *)service)[10]) {
				node = node->next;
				if (node == filter->list.sentinel)
					return found;
			}
		}
	}
	i = 0;
	if (filter->recordCount != 0) {
		offset = 0;
		do {
			type = (const char *)filter->records + offset;
			if (sceClibStrncmp(type, dataType, sceClibStrnlen(type, 0xFFFFFFFF)) == 0) {
				found = 1;
				if (pRecord != NULL)
					*pRecord = (NpWebApiExtdFilterRecord *)((char *)filter->records + offset);
				break;
			}
			offset += 80;
			i++;
		} while (i < filter->recordCount);
	}
	return found;
}

/* the extended data the record names, taken out of the event's */
int npWebApiExtdDataCollect(NpWebApiLibContext *ctx, const NpWebApiExtdFilterRecord *record,
	const NpPushExDataArgument *exData, int exDataNum, SceNpWebApiExtdPushEventExtdData **pData, SceSize *pCount)
{
	SceNpWebApiExtdPushEventExtdData *data;
	unsigned char *entry;
	const NpPushExDataArgument *argument;
	const char *name;
	unsigned int count;
	unsigned int total;
	unsigned int i;
	unsigned int j;
	int offset;
	int n;
	unsigned int length;
	void *text;

	data = NULL;
	count = 0;
	if (exData == NULL || exDataNum <= 0 || record->nameCount == 0)
		goto done;
	total = record->nameCount;
	offset = 0;
	for (i = 0; i < total; i++) {
		for (j = 0; (int)j < exDataNum; j++) {
			name = record->names + offset;
			if (sceClibStrncmp(name, exData[j].name, sceClibStrnlen(name, 0xFFFFFFFF)) == 0)
				count++;
		}
		total = record->nameCount;
		offset += 33;
	}
	if (count == 0)
		goto done;
	data = (SceNpWebApiExtdPushEventExtdData *)npWebApiLibContextPoolAlloc(ctx, 44 * count);
	if (data == NULL)
		goto fail;
	sceClibMemset(data, 0, 44 * count);
	n = 0;
	offset = 0;
	for (i = 0; i < record->nameCount; i++) {
		entry = (unsigned char *)data + 44 * n;
		argument = exData;
		for (j = 0; (int)j < exDataNum; j++) {
			name = record->names + offset;
			if (sceClibStrncmp(name, argument->name, sceClibStrnlen(name, 0xFFFFFFFF)) == 0) {
				sceClibStrncpy((char *)entry, argument->name, sceClibStrnlen(argument->name, 0xFFFFFFFF));
				if (argument->text != NULL) {
					length = argument->length;
					if (length != 0) {
						*(unsigned int *)(entry + 40) = length;
						text = (void *)npWebApiLibContextPoolAlloc(ctx, length + 1);
						*(void **)(entry + 36) = text;
						if (text == NULL)
							goto fail;
						sceClibMemset(text, 0, *(unsigned int *)(entry + 40) + 1);
						sceClibStrncpy(*(char **)(entry + 36), argument->text, *(unsigned int *)(entry + 40));
					}
				}
				n++;
				entry += 44;
				if ((unsigned int)n >= count)
					goto done;
			}
			argument++;
		}
		offset += 33;
	}
done:
	if (pData != NULL)
		*pData = data;
	if (pCount != NULL)
		*pCount = count;
	return 0;
fail:
	if (data != NULL && ctx != NULL) {
		entry = (unsigned char *)data;
		for (i = 0; i < count; i++) {
			if (*(void **)(entry + 36) != NULL) {
				npWebApiLibContextDestroyPool(ctx, *(void **)(entry + 36));
				*(void **)(entry + 36) = NULL;
			}
			entry += 44;
		}
		npWebApiLibContextDestroyPool(ctx, data);
	}
	return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
}

/* copies a peer address (its 20 bytes of online ID and its platform) into a pool block */
static SceNpPeerAddress *npWebApiPeerCopy(NpWebApiLibContext *ctx, const NpPushPeer *peer)
{
	SceNpPeerAddress *copy;

	copy = (SceNpPeerAddress *)npWebApiLibContextPoolAlloc(ctx, 24);
	if (copy != NULL) {
		sceClibMemset(copy, 0, 24);
		sceClibMemcpy_safe(copy, &peer->address.onlineId, 20);
		copy->platform = peer->address.platform;
	}
	return copy;
}

/* gives a user context's own callback (the one set with its user context) the event */
int npWebApiUserContextCallLegacyCallback(NpWebApiLibContext *ctx, NpWebApiUserContext *userCtx, const NpPushPeer *to,
	const NpPushPeer *from, const char *dataType, const void *data, SceSize dataLength)
{
	SceNpPeerAddress *toCopy;
	SceNpPeerAddress *fromCopy;
	char *typeCopy;
	void *dataCopy;
	SceSize length;
	LegacyCallbackFn callback;
	int argument;
	int result;

	toCopy = NULL;
	fromCopy = NULL;
	typeCopy = NULL;
	length = 0;
	dataCopy = NULL;
	callback = (LegacyCallbackFn)npWebApiUserContextGetParam0(userCtx);
	argument = npWebApiUserContextGetParam1(userCtx);
	result = argument;
	if (callback != NULL) {
		if (to != NULL) {
			toCopy = (SceNpPeerAddress *)npWebApiLibContextPoolAlloc(ctx, 24);
			sceClibMemset(toCopy, 0, 24);
			sceClibMemcpy_safe(toCopy, &to->address.onlineId, 20);
			toCopy->platform = to->address.platform;
		}
		if (from != NULL) {
			fromCopy = (SceNpPeerAddress *)npWebApiLibContextPoolAlloc(ctx, 24);
			sceClibMemset(fromCopy, 0, 24);
			sceClibMemcpy_safe(fromCopy, &from->address.onlineId, 20);
			fromCopy->platform = from->address.platform;
		}
		if (dataType != NULL) {
			typeCopy = (char *)npWebApiLibContextPoolAlloc(ctx, 65);
			sceClibMemset(typeCopy, 0, 65);
			sceClibStrncpy(typeCopy, dataType, 64);
		}
		if (data != NULL && dataLength != 0) {
			dataCopy = (void *)npWebApiLibContextPoolAlloc(ctx, dataLength);
			if (dataCopy != NULL) {
				sceClibMemcpy_safe(dataCopy, data, dataLength);
				length = dataLength;
			}
		}
		callback(npWebApiUserContextGetId(userCtx), toCopy, fromCopy, typeCopy, dataCopy, length, argument);
		if (toCopy != NULL)
			npWebApiLibContextDestroyPool(ctx, toCopy);
		if (fromCopy != NULL)
			npWebApiLibContextDestroyPool(ctx, fromCopy);
		if (typeCopy != NULL)
			npWebApiLibContextDestroyPool(ctx, typeCopy);
		result = (int)dataCopy;
		if (dataCopy != NULL)
			return npWebApiLibContextDestroyPool(ctx, dataCopy);
	}
	return result;
}

/* copies the event for a library context's user contexts */
int npWebApiLibContextCopyPeers(NpWebApiLibContext *ctx, const NpPushPeer *to, const NpPushPeer *from,
	const char *dataType, const void *data, SceSize dataLength, SceNpPeerAddress **pTo, SceNpPeerAddress **pFrom,
	char **pDataType, void **pData, SceSize *pLength)
{
	SceNpPeerAddress *toCopy;
	SceNpPeerAddress *fromCopy;
	char *typeCopy;
	void *dataCopy;
	SceSize length;

	toCopy = NULL;
	fromCopy = NULL;
	length = 0;
	typeCopy = NULL;
	dataCopy = NULL;
	if (to != NULL) {
		toCopy = npWebApiPeerCopy(ctx, to);
		if (toCopy == NULL)
			goto fail;
	}
	if (from != NULL) {
		fromCopy = npWebApiPeerCopy(ctx, from);
		if (fromCopy == NULL)
			goto fail;
	}
	if (dataType != NULL) {
		typeCopy = (char *)npWebApiLibContextPoolAlloc(ctx, 65);
		if (typeCopy == NULL)
			goto fail;
		sceClibMemset(typeCopy, 0, 65);
		sceClibStrncpy(typeCopy, dataType, 64);
	}
	if (data != NULL && dataLength != 0) {
		dataCopy = (void *)npWebApiLibContextPoolAlloc(ctx, dataLength + 1);
		if (dataCopy == NULL)
			goto fail;
		sceClibMemset(dataCopy, 0, dataLength + 1);
		sceClibMemcpy_safe(dataCopy, data, dataLength);
		length = dataLength;
	}
	if (pTo != NULL)
		*pTo = toCopy;
	if (pFrom != NULL)
		*pFrom = fromCopy;
	if (pDataType != NULL)
		*pDataType = typeCopy;
	if (pData != NULL)
		*pData = dataCopy;
	if (pLength != NULL)
		*pLength = length;
	return 0;
fail:
	if (toCopy != NULL)
		npWebApiLibContextDestroyPool(ctx, toCopy);
	if (fromCopy != NULL)
		npWebApiLibContextDestroyPool(ctx, fromCopy);
	if (typeCopy != NULL)
		npWebApiLibContextDestroyPool(ctx, typeCopy);
	if (dataCopy != NULL)
		npWebApiLibContextDestroyPool(ctx, dataCopy);
	return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
}

void npWebApiUserContextDispatchPushEvent(NpWebApiUserContext *userCtx, SceNpPeerAddress *to, SceNpPeerAddress *from,
	const char *dataType, const void *data, SceSize dataLength)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *node;
	NpWebApiLibContext *libCtx;
	int filter;

	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->pushCallbacks.sentinel;
	libCtx = userCtx->libContext;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			filter = npWebApiLibContextHasPushEventFilter(libCtx, node->filterId);
			if (filter != 0 && npPushFilterContainsType((const NpWebApiPushFilter *)filter, dataType) != 0)
				((PushEventCallbackFn)node->callback)(node->id, to, from, dataType, data, dataLength,
					node->argument);
			node = node->next;
			libCtx = userCtx->libContext;
		} while (node != userCtx->pushCallbacks.sentinel);
	}
	npWebApiLibContextUnlock(libCtx);
}

void npWebApiUserContextDispatchPushEventLocked(NpWebApiUserContext *userCtx, SceNpPeerAddress *to,
	SceNpPeerAddress *from, const char *dataType, const void *data, SceSize dataLength)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *node;
	NpWebApiCallbackNode *next;
	NpWebApiLibContext *libCtx;
	int filter;

	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->pushCallbacks.sentinel;
	libCtx = userCtx->libContext;
	node = NULL;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			next = node->next;
			node->state = 1;
			node = next;
		} while (next != sentinel);
	}
	npWebApiLibContextUnlock(libCtx);
	while (1) {
		npWebApiLibContextLock(userCtx->libContext);
		sentinel = userCtx->pushCallbacks.sentinel;
		node = NULL;
		libCtx = userCtx->libContext;
		if (sentinel != NULL)
			node = sentinel->next;
		if (node != sentinel) {
			do {
				if (node->state == 1) {
					node->state = 2;
					break;
				}
				node = node->next;
			} while (node != sentinel);
		}
		npWebApiLibContextUnlock(libCtx);
		libCtx = userCtx->libContext;
		if (node == userCtx->pushCallbacks.sentinel)
			return;
		filter = npWebApiLibContextHasPushEventFilter(libCtx, node->filterId);
		if (filter != 0 && npPushFilterContainsType((const NpWebApiPushFilter *)filter, dataType) != 0)
			((PushEventCallbackFn)node->callback)(node->id, to, from, dataType, data, dataLength, node->argument);
		npWebApiLibContextLock(userCtx->libContext);
		node->state = 0;
		npWebApiLibContextUnlock(userCtx->libContext);
	}
}

void npWebApiUserContextDispatchServiceEvent(NpWebApiUserContext *userCtx, const char *service, SceNpPeerAddress *to,
	SceNpPeerAddress *from, const char *dataType, const void *data, SceSize dataLength)
{
	NpWebApiServiceCallbackNode *sentinel;
	NpWebApiServiceCallbackNode *node;
	NpWebApiLibContext *libCtx;
	NpWebApiServiceFilter *filter;

	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->serviceCallbacks.sentinel;
	node = NULL;
	libCtx = userCtx->libContext;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			filter = (NpWebApiServiceFilter *)npWebApiLibContextHasServicePushEventFilter(libCtx, node->filterId);
			if (filter != NULL && npWebApiServiceFilterMatches(filter, service, dataType) != 0) {
				if (node->callback != NULL)
					((ServiceEventCallbackFn)node->callback)(node->id, npWebApiServiceFilterGetName(filter), npWebApiServiceFilterGetHandle(filter), to, from,
						dataType, data, dataLength, node->argument);
				if (node->callbackEx != NULL)
					((ServiceEventCallbackExFn)node->callbackEx)(node->id, npWebApiServiceFilterGetName(filter), npWebApiServiceFilterGetHandle(filter),
						service, to, from, dataType, data, dataLength, node->argument);
			}
			node = node->next;
			libCtx = userCtx->libContext;
		} while (node != userCtx->serviceCallbacks.sentinel);
	}
	npWebApiLibContextUnlock(libCtx);
}

void npWebApiUserContextDispatchServiceEventLocked(NpWebApiUserContext *userCtx, const char *service,
	SceNpPeerAddress *to, SceNpPeerAddress *from, const char *dataType, const void *data, SceSize dataLength)
{
	NpWebApiServiceCallbackNode *sentinel;
	NpWebApiServiceCallbackNode *node;
	NpWebApiServiceCallbackNode *next;
	NpWebApiLibContext *libCtx;
	NpWebApiServiceFilter *filter;

	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->serviceCallbacks.sentinel;
	node = NULL;
	libCtx = userCtx->libContext;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			next = node->next;
			node->state = 1;
			node = next;
		} while (next != sentinel);
	}
	npWebApiLibContextUnlock(libCtx);
	while (1) {
		npWebApiLibContextLock(userCtx->libContext);
		sentinel = userCtx->serviceCallbacks.sentinel;
		node = NULL;
		libCtx = userCtx->libContext;
		if (sentinel != NULL)
			node = sentinel->next;
		if (node != sentinel) {
			do {
				if (node->state == 1) {
					node->state = 2;
					break;
				}
				node = node->next;
			} while (node != sentinel);
		}
		npWebApiLibContextUnlock(libCtx);
		libCtx = userCtx->libContext;
		if (node == userCtx->serviceCallbacks.sentinel)
			return;
		filter = (NpWebApiServiceFilter *)npWebApiLibContextHasServicePushEventFilter(libCtx, node->filterId);
		if (filter != NULL && npWebApiServiceFilterMatches(filter, service, dataType) != 0) {
			if (node->callback != NULL)
				((ServiceEventCallbackFn)node->callback)(node->id, npWebApiServiceFilterGetName(filter), npWebApiServiceFilterGetHandle(filter), to, from,
					dataType, data, dataLength, node->argument);
			if (node->callbackEx != NULL)
				((ServiceEventCallbackExFn)node->callbackEx)(node->id, npWebApiServiceFilterGetName(filter), npWebApiServiceFilterGetHandle(filter), service, to,
					from, dataType, data, dataLength, node->argument);
		}
		npWebApiLibContextLock(userCtx->libContext);
		node->state = 0;
		npWebApiLibContextUnlock(userCtx->libContext);
	}
}

static void npWebApiExtdDataFree(NpWebApiLibContext *ctx, SceNpWebApiExtdPushEventExtdData *data, SceSize count)
{
	unsigned char *entry;
	SceSize i;

	if (data != NULL && ctx != NULL && count != 0) {
		entry = (unsigned char *)data;
		for (i = 0; i < count; i++) {
			if (*(void **)(entry + 36) != NULL) {
				npWebApiLibContextDestroyPool(ctx, *(void **)(entry + 36));
				*(void **)(entry + 36) = NULL;
			}
			entry += 44;
		}
		npWebApiLibContextDestroyPool(ctx, data);
	}
}

void npWebApiUserContextDispatchExtdEvent(NpWebApiUserContext *userCtx, const char *service, SceNpPeerAddress *to,
	SceNpPeerAddress *from, const char *dataType, const void *data, SceSize dataLength,
	const NpPushExDataArgument *exData, int exDataNum)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *node;
	NpWebApiLibContext *libCtx;
	NpWebApiExtdFilter *filter;
	NpWebApiExtdFilterRecord *record;
	SceNpWebApiExtdPushEventExtdData *extd;
	SceSize extdCount;

	extd = NULL;
	extdCount = 0;
	record = NULL;
	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->extdCallbacks.sentinel;
	node = NULL;
	libCtx = userCtx->libContext;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			filter = npWebApiLibContextFindExtdPushEventFilter(libCtx, node->filterId);
			if (filter != NULL) {
				if (npWebApiExtdFilterMatches(filter, service, dataType, &record) != 0) {
					extd = NULL;
					extdCount = 0;
					if (npWebApiExtdDataCollect(userCtx->libContext, record, exData, exDataNum, &extd,
						&extdCount) >= 0) {
						if (node->callback != NULL)
							((ExtdEventCallbackFn)node->callback)(node->id, npPushFilterName((const unsigned char *)filter),
								npWebApiExtdFilterGetHandle(filter), to, from, dataType, data, dataLength, extd, extdCount,
								node->argument);
						npWebApiExtdDataFree(userCtx->libContext, extd, extdCount);
					}
				}
			}
			node = node->next;
			libCtx = userCtx->libContext;
		} while (node != userCtx->extdCallbacks.sentinel);
	}
	npWebApiLibContextUnlock(libCtx);
}

void npWebApiUserContextDispatchExtdEventLocked(NpWebApiUserContext *userCtx, const char *service,
	SceNpPeerAddress *to, SceNpPeerAddress *from, const char *dataType, const void *data, SceSize dataLength,
	const NpPushExDataArgument *exData, int exDataNum)
{
	NpWebApiCallbackNode *sentinel;
	NpWebApiCallbackNode *node;
	NpWebApiCallbackNode *next;
	NpWebApiLibContext *libCtx;
	NpWebApiExtdFilter *filter;
	NpWebApiExtdFilterRecord *record;
	SceNpWebApiExtdPushEventExtdData *extd;
	SceSize extdCount;

	extd = NULL;
	extdCount = 0;
	record = NULL;
	npWebApiLibContextLock(userCtx->libContext);
	sentinel = userCtx->extdCallbacks.sentinel;
	node = NULL;
	libCtx = userCtx->libContext;
	if (sentinel != NULL)
		node = sentinel->next;
	if (node != sentinel) {
		do {
			next = node->next;
			node->state = 1;
			node = next;
		} while (next != sentinel);
	}
	npWebApiLibContextUnlock(libCtx);
	while (1) {
		npWebApiLibContextLock(userCtx->libContext);
		sentinel = userCtx->extdCallbacks.sentinel;
		node = NULL;
		libCtx = userCtx->libContext;
		if (sentinel != NULL)
			node = sentinel->next;
		if (node != sentinel) {
			do {
				if (node->state == 1) {
					node->state = 2;
					break;
				}
				node = node->next;
			} while (node != sentinel);
		}
		npWebApiLibContextUnlock(libCtx);
		libCtx = userCtx->libContext;
		if (node == userCtx->extdCallbacks.sentinel)
			break;
		extd = NULL;
		filter = npWebApiLibContextFindExtdPushEventFilter(libCtx, node->filterId);
		if (filter != NULL) {
			if (npWebApiExtdFilterMatches(filter, service, dataType, &record) != 0) {
				extd = NULL;
				extdCount = 0;
				if (npWebApiExtdDataCollect(userCtx->libContext, record, exData, exDataNum, &extd, &extdCount) >=
					0) {
					if (node->callback != NULL)
						((ExtdEventCallbackFn)node->callback)(node->id, npPushFilterName((const unsigned char *)filter),
							npWebApiExtdFilterGetHandle(filter), to, from, dataType, data, dataLength, extd, extdCount,
							node->argument);
					npWebApiExtdDataFree(userCtx->libContext, extd, extdCount);
				}
			}
		}
		npWebApiLibContextLock(userCtx->libContext);
		node->state = 0;
		npWebApiLibContextUnlock(userCtx->libContext);
	}
}

void npPushDispatch(const NpPushPeer *to, const NpPushPeer *from, const char *service, const char *dataType,
	const void *data, SceSize dataLength, const NpPushExDataArgument *exData, int exDataNum)
{
	NpWebApiLibContext *libCtx;
	NpWebApiLibContext *current;
	NpWebApiUserContext *userCtx;
	NpWebApiUserContextHandle *handle;
	SceNpPeerAddress *toCopy;
	SceNpPeerAddress *fromCopy;
	char *typeCopy;
	void *dataCopy;
	SceSize lengthCopy;
	int libCtxId;
	int userCtxId;
	SceNpPeerAddress *toCopy2;
	SceNpPeerAddress *fromCopy2;
	char *typeCopy2;
	void *dataCopy2;
	SceSize lengthCopy2;

	if (npWebApiGetSdkVersion() < NP_WEBAPI_SDK_3_50) {
		toCopy = NULL;
		fromCopy = NULL;
		typeCopy = NULL;
		dataCopy = NULL;
		lengthCopy = 0;
		npWebApiGlobalLock();
		libCtx = npWebApiGetFirstLibContext();
		if (libCtx != NULL) {
			do {
				toCopy = NULL;
				fromCopy = NULL;
				typeCopy = NULL;
				dataCopy = NULL;
				lengthCopy = 0;
				if (npWebApiLibContextCopyPeers(libCtx, to, from, dataType, data, dataLength, &toCopy, &fromCopy,
					&typeCopy, &dataCopy, &lengthCopy) >= 0) {
					npWebApiLibContextLock(libCtx);
					handle = npWebApiLibContextCreateUserContextHandle(libCtx);
					while (npWebApiUserContextHandleHasNext(handle)) {
						userCtx = npWebApiUserContextHandleNext(handle);
						if (SceNpCommon_8C21F489(&to->address.onlineId, npWebApiUserContextOnlineId(userCtx)) == 0) {
							if (npWebApiGetSdkVersion() >= NP_WEBAPI_SDK_3_50)
								npWebApiUserContextDispatchExtdEvent(userCtx, service, toCopy, fromCopy,
									typeCopy, dataCopy, lengthCopy, exData, exDataNum);
							if (service != NULL) {
								npWebApiUserContextDispatchServiceEvent(userCtx, service, toCopy, fromCopy,
									typeCopy, dataCopy, lengthCopy);
							} else {
								npWebApiUserContextCallLegacyCallback(libCtx, userCtx, to, from, dataType, data,
									dataLength);
								npWebApiUserContextDispatchPushEvent(userCtx, toCopy, fromCopy, typeCopy,
									dataCopy, lengthCopy);
							}
						}
					}
					if (handle != NULL)
						((void (*)(void *))handle->vtable[1])(handle);
					npWebApiLibContextUnlock(libCtx);
					if (toCopy != NULL)
						npWebApiLibContextDestroyPool(libCtx, toCopy);
					if (fromCopy != NULL)
						npWebApiLibContextDestroyPool(libCtx, fromCopy);
					if (typeCopy != NULL)
						npWebApiLibContextDestroyPool(libCtx, typeCopy);
					if (dataCopy != NULL)
						npWebApiLibContextDestroyPool(libCtx, dataCopy);
				}
				libCtx = npWebApiLibContextNext(libCtx);
			} while (libCtx != NULL);
		}
		npWebApiGlobalUnlock();
	} else {
		toCopy2 = NULL;
		fromCopy2 = NULL;
		typeCopy2 = NULL;
		dataCopy2 = NULL;
		lengthCopy2 = 0;
		npWebApiGlobalLock();
		for (current = npWebApiGetFirstLibContext(); current != NULL; current = npWebApiLibContextNext(current))
			npWebApiLibContextSetFlag121(current, 1);
		libCtxId = 0;
		npWebApiGlobalUnlock();
		while (1) {
			libCtxId = 0;
			npWebApiGlobalLock();
			current = npWebApiGetFirstLibContext();
			if (current != NULL) {
				while (npWebApiLibContextGetFlag121(current) != 1) {
					current = npWebApiLibContextNext(current);
					if (current == NULL)
						goto next;
				}
				libCtxId = npWebApiLibContextGetId(current);
				npWebApiLibContextSetFlag121(current, 0);
			}
next:
			npWebApiGlobalUnlock();
			if (libCtxId == 0)
				break;
			libCtx = npWebApiFindLibContext(libCtxId, 0);
			if (libCtx != NULL) {
				npWebApiLibContextLock(libCtx);
				handle = npWebApiLibContextCreateUserContextHandle(libCtx);
				while (npWebApiUserContextHandleHasNext(handle))
					npWebApiUserContextSetFlag97(npWebApiUserContextHandleNext(handle), 1);
				if (handle != NULL)
					((void (*)(void *))handle->vtable[1])(handle);
				npWebApiLibContextUnlock(libCtx);
				toCopy2 = NULL;
				fromCopy2 = NULL;
				typeCopy2 = NULL;
				dataCopy2 = NULL;
				lengthCopy2 = 0;
				if (npWebApiLibContextCopyPeers(libCtx, to, from, dataType, data, dataLength, &toCopy2, &fromCopy2,
					&typeCopy2, &dataCopy2, &lengthCopy2) >= 0) {
					while (1) {
						userCtxId = 0;
						npWebApiLibContextLock(libCtx);
						handle = npWebApiLibContextCreateUserContextHandle(libCtx);
						while (npWebApiUserContextHandleHasNext(handle)) {
							userCtx = npWebApiUserContextHandleNext(handle);
							if (npWebApiUserContextGetFlag97(userCtx) == 1) {
								userCtxId = npWebApiUserContextGetId(userCtx);
								npWebApiUserContextSetFlag97(userCtx, 0);
								break;
							}
						}
						if (handle != NULL)
							((void (*)(void *))handle->vtable[1])(handle);
						npWebApiLibContextUnlock(libCtx);
						if (userCtxId == 0)
							break;
						userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
						if (userCtx != NULL) {
							if (SceNpCommon_8C21F489(&to->address.onlineId, npWebApiUserContextOnlineId(userCtx)) == 0) {
								npWebApiUserContextDispatchExtdEventLocked(userCtx, service, toCopy2, fromCopy2,
									typeCopy2, dataCopy2, lengthCopy2, exData, exDataNum);
								if (service != NULL) {
									npWebApiUserContextDispatchServiceEventLocked(userCtx, service, toCopy2,
										fromCopy2, typeCopy2, dataCopy2, lengthCopy2);
								} else {
									npWebApiUserContextCallLegacyCallback(libCtx, userCtx, to, from, dataType,
										data, dataLength);
									npWebApiUserContextDispatchPushEventLocked(userCtx, toCopy2, fromCopy2,
										typeCopy2, dataCopy2, lengthCopy2);
								}
							}
							npWebApiUserContextRelease(userCtx);
						}
					}
					if (toCopy2 != NULL)
						npWebApiLibContextDestroyPool(libCtx, toCopy2);
					if (fromCopy2 != NULL)
						npWebApiLibContextDestroyPool(libCtx, fromCopy2);
					if (typeCopy2 != NULL)
						npWebApiLibContextDestroyPool(libCtx, typeCopy2);
					if (dataCopy2 != NULL)
						npWebApiLibContextDestroyPool(libCtx, dataCopy2);
					npWebApiLibContextRelease(libCtx);
				}
			}
		}
	}
}
