/*
 * SceNpWebApi: the request object's construction and destruction. The names are ours.
 */

#include "np_webapi_internal.h"

/* the counter the request IDs are taken from (0x81012070) */
static SceUInt64 s_requestCounter;

NpWebApiRequest *npWebApiRequestConstruct(NpWebApiRequest *request, NpWebApiLibContext *libCtx)
{
	request->listAHead = NULL;
	request->listACount = 0;
	request->listAAllocator = NULL;
	request->listBHead = NULL;
	request->listBCount = 0;
	request->listBAllocator = NULL;
	request->vtable = g_requestVtable;
	request->libContext = libCtx;
	request->refCount = 0;
	request->requestId = 0;
	request->path = NULL;
	request->query = NULL;
	request->queryLength = 0;
	request->method = 0;
	request->totalSize = 0;
	request->contentType = NULL;
	request->listAAllocator = (void *)SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool2(libCtx));
	request->listBAllocator = (void *)SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool2(libCtx));
	request->multipart = 0;
	sceClibMemset(&request->reserved2c[64], 0, 0x40);
	sceClibMemset(&request->reserved2c[0], 0, 0x40);
	request->reserved2c[128] = 0;
	request->kind = 0;
	request->busy = 0;
	request->httpRequestId = 0;
	request->field220 = 0;
	request->field224 = 0;
	request->field236 = 0;
	request->next = NULL;
	request->extraData = NULL;
	sceClibMemset(request->data, 0, 0x40);
	request->field316 = 0;
	request->readOffset = 0;
	request->sentSize = 0;
	request->flag328 = 0;
	request->flag389 = 0;
	return request;
}

void npWebApiRequestDestruct(NpWebApiRequest *request)
{
	NpWebApiListNode *head;
	NpWebApiListNode *node;
	NpWebApiListNode *next;
	void *allocator;

	request->vtable = g_requestVtable;
	head = request->listBHead;
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
	}
	head = request->listAHead;
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
	}
	request->vtable = g_baseVtable;
}

void npWebApiRequestDelete(NpWebApiRequest *request)
{
	npWebApiRequestDestruct(request);
	SceNpCommonPs4_BB5282C8(request);
}

void npWebApiRequestDestroy(NpWebApiRequest *request, int *pHttpRequestId)
{
	NpWebApiLibContext *libCtx;
	int httpRequestId;

	if (request == NULL)
		return;
	libCtx = request->libContext;
	if (request->path != NULL)
		npWebApiLibContextDestroyPool(libCtx, request->path);
	if (request->query != NULL)
		npWebApiLibContextDestroyPool(libCtx, request->query);
	if (request->contentType != NULL) {
		npWebApiLibContextDestroyPool(libCtx, request->contentType);
		request->contentType = NULL;
	}
	if (request->extraData != NULL) {
		npWebApiLibContextDestroyPool(libCtx, request->extraData);
		request->extraData = NULL;
	}
	npWebApiRequestClearListA(request, libCtx);
	npWebApiRequestClearListB(request, libCtx);
	httpRequestId = request->httpRequestId;
	if (pHttpRequestId != NULL)
		*pHttpRequestId = httpRequestId;
	else if (httpRequestId != 0)
		sceHttpDeleteRequest(httpRequestId);
	if (npWebApiGetSdkVersion() >= 0x3300000) {
		while (request->refCount != 0)
			sceKernelDelayThread(50000);
	}
	if (request != NULL)
		((void (*)(NpWebApiRequest *))request->vtable[1])(request);
}

int npWebApiRequestCreate(NpWebApiUserContext *userCtx, const char *path, const char *query,
	unsigned int method, const SceNpWebApiContentParameter *content, const void *extraData,
	NpWebApiRequest **pRequest, unsigned char multipart)
{
	NpWebApiLibContext *libCtx;
	NpWebApiLibContext *poolCtx;
	NpWebApiRequest *request;
	NpWebApiRequest *found;
	SceUInt64 requestId;
	SceSize length;
	char *copy;
	int result;

	request = NULL;
	poolCtx = npWebApiUserContextGetLibContext(userCtx);
	result = 0;
	void *memory = SceNpCommonPs4_D154D84F(392, npWebApiLibContextGetPool(poolCtx));
	if (memory != NULL) {
		request = npWebApiRequestConstruct((NpWebApiRequest *)memory, poolCtx);
		if (request != NULL) {
			while (1) {
				npWebApiUserContextLockLib(userCtx);
				s_requestCounter++;
				if (s_requestCounter > 0xFFFFFFFFULL)
					s_requestCounter = 1;
				requestId = ((SceUInt64)npWebApiUserContextGetId(userCtx) << 32) | s_requestCounter;
				npWebApiUserContextUnlockLib(userCtx);
				found = npWebApiUserContextFindRequest(userCtx, requestId);
				if (found == NULL)
					break;
				npWebApiLibContextLock(found->libContext);
				libCtx = found->libContext;
				found->refCount--;
				npWebApiLibContextUnlock(libCtx);
			}
			request->requestId = requestId;
			if (path != NULL) {
				length = sceClibStrnlen(path, 0xFFFFFFFF);
				request->path = (char *)npWebApiLibContextPoolAlloc(poolCtx, length + 1);
				if (request->path == NULL)
					goto fail;
				sceClibMemset(request->path, 0, length + 1);
				sceClibStrncpy(request->path, path, length);
			}
			if (query != NULL) {
				length = sceClibStrnlen(query, 0xFFFFFFFF);
				request->query = (char *)npWebApiLibContextPoolAlloc(poolCtx, length + 1);
				if (request->query == NULL)
					goto fail;
				sceClibMemset(request->query, 0, length + 1);
				sceClibStrncpy(request->query, query, length);
				request->queryLength = length;
			}
			request->method = method;
			request->multipart = multipart;
			if (content != NULL) {
				request->totalSize = content->contentLength;
				if (content->pContentType != NULL) {
					length = sceClibStrnlen(content->pContentType, 0xFFFFFFFF) + 1;
					copy = (char *)npWebApiLibContextPoolAlloc(poolCtx, length);
					if (copy == NULL)
						goto fail;
					sceClibMemset(copy, 0, length);
					sceClibStrncpy(copy, content->pContentType, length);
					request->contentType = copy;
				}
			}
			if (extraData != NULL) {
				request->extraData = (void *)npWebApiLibContextPoolAlloc(poolCtx, 1040);
				if (request->extraData == NULL)
					goto fail;
				sceClibMemcpy_safe(request->extraData, extraData, 0x410);
			}
			if (pRequest != NULL)
				*pRequest = request;
			return result;
		}
	}
fail:
	result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
	if (request != NULL) {
		if (request->contentType != NULL) {
			npWebApiLibContextDestroyPool(poolCtx, request->contentType);
			request->contentType = NULL;
		}
		npWebApiRequestDestroy(request, NULL);
	}
	return result;
}
