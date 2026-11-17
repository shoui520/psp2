/*
 * SceNpWebApi: the request's small accessors (its reference count under the library context's lock,
 * its ID and the next request). The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiRequestRelease(NpWebApiRequest *request)
{
	NpWebApiLibContext *libCtx;

	npWebApiLibContextLock(request->libContext);
	libCtx = request->libContext;
	request->refCount--;
	return npWebApiLibContextUnlock(libCtx);
}

SceInt64 npWebApiRequestGetId(NpWebApiRequest *request)
{
	return request->requestId;
}

NpWebApiRequest *npWebApiRequestNext(NpWebApiRequest *request)
{
	return request->next;
}

int npWebApiRequestAddRef(NpWebApiRequest *request)
{
	NpWebApiLibContext *libCtx;

	npWebApiLibContextLock(request->libContext);
	libCtx = request->libContext;
	request->refCount++;
	return npWebApiLibContextUnlock(libCtx);
}

int npWebApiRequestHasOtherRefs(NpWebApiRequest *request)
{
	NpWebApiLibContext *libCtx;
	int others;

	npWebApiLibContextLock(request->libContext);
	others = request->refCount > 1;
	libCtx = request->libContext;
	npWebApiLibContextUnlock(libCtx);
	return others;
}

int npWebApiRequestGetFixedSize(void)
{
	return 64;
}

NpWebApiRequest *npWebApiRequestSetField316(NpWebApiRequest *request, int value)
{
	request->field316 = value;
	return request;
}

int npWebApiRequestGetField312(NpWebApiRequest *request)
{
	return request->field312;
}

int npWebApiRequestGetFlag328(NpWebApiRequest *request)
{
	return request->flag328;
}

NpWebApiRequest *npWebApiRequestSetFlag328(NpWebApiRequest *request, char value)
{
	request->flag328 = value;
	return request;
}

int npWebApiRequestGetFlag389(NpWebApiRequest *request)
{
	return request->flag389;
}

NpWebApiRequest *npWebApiRequestSetFlag389(NpWebApiRequest *request, char value)
{
	request->flag389 = value;
	return request;
}

void *npWebApiRequestGetObject280(NpWebApiRequest *request)
{
	return request->data + 32;
}

NpWebApiRequest *npWebApiRequestSetNext(NpWebApiRequest *request, NpWebApiRequest *next)
{
	request->next = next;
	return request;
}

/* free the second list's nodes (each gives back its pool object) and its ring */
int npWebApiRequestClearListB(NpWebApiRequest *request, NpWebApiLibContext *libCtx)
{
	NpWebApiListNode *head;
	NpWebApiListNode *node;
	NpWebApiListNode *next;
	void *allocator;

	npWebApiLibContextLock(libCtx);
	head = request->listBHead;
	node = NULL;
	if (head != NULL)
		node = head->next;
	for (; node != head; node = node->next) {
		npWebApiLibContextDestroyPool(libCtx, (void *)node->value8);
		head = request->listBHead;
	}
	if (head != NULL) {
		node = head->next;
		allocator = request->listBAllocator;
		if (node != head) {
			do {
				next = node->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, node);
					allocator = request->listBAllocator;
					head = request->listBHead;
				}
				node = next;
			} while (next != head);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, node);
		request->listBHead = NULL;
		request->listBCount = 0;
	}
	return npWebApiLibContextUnlock(libCtx);
}

int npWebApiRequestClearListA(NpWebApiRequest *request, NpWebApiLibContext *libCtx)
{
	NpWebApiListNode *head;
	NpWebApiListNode *node;
	NpWebApiListNode *next;
	void *allocator;

	npWebApiLibContextLock(libCtx);
	head = request->listAHead;
	node = NULL;
	if (head != NULL)
		node = head->next;
	for (; node != head; node = node->next) {
		if (node->value8 != 0)
			npWebApiLibContextDestroyPool(libCtx, (void *)node->value8);
		if (node->value12 != 0)
			npWebApiLibContextDestroyPool(libCtx, (void *)node->value12);
		head = request->listAHead;
	}
	if (head != NULL) {
		node = head->next;
		allocator = request->listAAllocator;
		if (node != head) {
			do {
				next = node->next;
				if (allocator != NULL) {
					SceNpCommonPs4_6B428829(allocator, node);
					allocator = request->listAAllocator;
					head = request->listAHead;
				}
				node = next;
			} while (next != head);
		}
		if (allocator != NULL)
			SceNpCommonPs4_6B428829(allocator, node);
		request->listAHead = NULL;
		request->listACount = 0;
	}
	return npWebApiLibContextUnlock(libCtx);
}

int npWebApiRequestIdGetUserContextId(SceInt64 requestId)
{
	return (int)(requestId >> 32);
}

unsigned int npWebApiRequestIdGetLibContextId(SceInt64 requestId)
{
	return (unsigned int)(requestId >> 32) >> 16;
}

int npWebApiRequestGetKind(NpWebApiRequest *request)
{
	return request->kind;
}

int npWebApiRequestGetBusy(NpWebApiRequest *request)
{
	return request->busy;
}

NpWebApiRequest *npWebApiRequestSetBusy(NpWebApiRequest *request, char value)
{
	request->busy = value;
	return request;
}

int npWebApiRequestGetField232(NpWebApiRequest *request)
{
	return request->field232;
}

int npWebApiRequestGetHttpRequestId(NpWebApiRequest *request)
{
	return request->httpRequestId;
}

int npWebApiRequestGetField228(NpWebApiRequest *request)
{
	return request->field228;
}

int npWebApiRequestGetField236(NpWebApiRequest *request)
{
	return request->field236;
}

/* gives out what the request has buffered, up to len bytes */
SceSize npWebApiRequestReadBuffered(NpWebApiRequest *request, void *dst, SceSize len)
{
	int size;
	int offset;
	SceSize left;
	SceSize count;

	size = request->field316;
	if (size == 0)
		return 0;
	offset = request->readOffset;
	if (size == offset)
		return 0;
	left = size - offset;
	count = len;
	if (left < len)
		count = left;
	sceClibMemcpy_safe(dst, (unsigned char *)request + offset + 248, count);
	request->readOffset += count;
	return count;
}

int npWebApiRequestIsSendComplete(NpWebApiRequest *request)
{
	return request->sentSize == request->totalSize;
}

NpWebApiRequest *npWebApiRequestSetKind(NpWebApiRequest *request, int value)
{
	request->kind = value;
	return request;
}

const char *npWebApiRequestGetPath(NpWebApiRequest *request)
{
	return request->path;
}

NpWebApiRequest *npWebApiRequestSetField228(NpWebApiRequest *request, int value)
{
	request->field228 = value;
	return request;
}

NpWebApiRequest *npWebApiRequestSetField236(NpWebApiRequest *request, int value)
{
	request->field236 = value;
	return request;
}

NpWebApiRequest *npWebApiRequestSetResponseHeaders(NpWebApiRequest *request, int headers, int size)
{
	request->field220 = headers;
	request->field224 = size;
	return request;
}

void *npWebApiRequestGetData(NpWebApiRequest *request)
{
	return request->data;
}

char *npWebApiAuthObjectGetAccessToken(void *object)
{
	return (char *)object + 16;
}
