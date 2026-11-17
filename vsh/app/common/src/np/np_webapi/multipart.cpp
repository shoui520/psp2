/*
 * SceNpWebApi: sending the parts of a multipart request. The names are ours.
 */

#include "np_webapi_internal.h"

/* sends one piece of the body; an SSL error is converted, any other negative result is the result */
#define SEND_PIECE(request, data, size, result)					\
	do {									\
		(result) = sceHttpSendRequest((request)->httpRequestId, (data), (size));	\
		if ((result) == NP_WEBAPI_HTTP_ERROR_SSL) {				\
			(result) = npWebApiConvertHttpError((request)->httpRequestId, NP_WEBAPI_HTTP_ERROR_SSL);	\
			if ((result) < 0)					\
				return (result);				\
		} else if ((result) < 0) {					\
			return (result);					\
		}								\
	} while (0)

int npWebApiRequestSendMultipartPart(NpWebApiRequest *request, const void *data, SceSize size, int partId)
{
	NpWebApiListNode *head;
	NpWebApiListNode *node;
	NpWebApiListNode *first;
	NpWebApiMultipartPart *part;
	SceSize length;
	SceSize last;
	int result;
	int count;
	int empty;
	SceSize sent;
	int total;

	npWebApiLibContextLock(request->libContext);
	head = request->listBHead;
	first = NULL;
	if (head != NULL)
		first = head->next;
	node = first;
	empty = first == head;
	part = NULL;
	if (!empty) {
		do {
			part = (NpWebApiMultipartPart *)node->value8;
			if (part->partId == partId)
				break;
			node = node->next;
			if (node == head)
				part = NULL;
		} while (node != head);
	}
	npWebApiLibContextUnlock(request->libContext);
	if (part == NULL)
		return NP_WEBAPI_ERROR_MULTIPART_PART_NOT_FOUND;
	if (part->sentLength == 0) {
		length = sceClibStrnlen("--", 0xFFFFFFFF);
		SEND_PIECE(request, "--", length, result);
		request->sentSize += length;
		length = sceClibStrnlen(request->boundary, 0xFFFFFFFF);
		SEND_PIECE(request, request->boundary, length, result);
		request->sentSize += length;
		length = sceClibStrnlen("\r\n", 0xFFFFFFFF);
		SEND_PIECE(request, "\r\n", length, result);
		request->sentSize += length;
		length = sceClibStrnlen(part->header, 0xFFFFFFFF);
		SEND_PIECE(request, part->header, length, result);
		request->sentSize += length;
	}
	SEND_PIECE(request, data, size, result);
	sent = part->sentLength + size;
	total = part->contentLength;
	part->sentLength = sent;
	request->sentSize += size;
	if (sent == total) {
		length = sceClibStrnlen("\r\n", 0xFFFFFFFF);
		SEND_PIECE(request, "\r\n", length, result);
		request->sentSize += length;
		npWebApiLibContextLock(request->libContext);
		count = request->listBCount;
		npWebApiLibContextUnlock(request->libContext);
		if (partId == count) {
			length = sceClibStrnlen("--", 0xFFFFFFFF);
			SEND_PIECE(request, "--", length, result);
			request->sentSize += length;
			last = sceClibStrnlen(request->boundary, 0xFFFFFFFF);
			SEND_PIECE(request, request->boundary, last, result);
			request->sentSize += last;
			length = sceClibStrnlen("--\r\n", 0xFFFFFFFF);
			SEND_PIECE(request, "--\r\n", length, result);
			request->sentSize += length;
		}
	}
	return result;
}

/* the part list's insert: the handle list's, for a list of parts */
void npWebApiPartListInsert(NpWebApiHandleNode **pResult, NpWebApiHandleList *list, NpWebApiMultipartPart **pPart)
{
	NpWebApiHandleNode *sentinel;
	NpWebApiHandleNode *node;
	NpWebApiHandleNode *result;
	void *memory;
	NpWebApiMultipartPart *part;
	void *allocator;
	unsigned long count;

	allocator = list->allocator;
	if (list->sentinel == NULL) {
		sentinel = NULL;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 12);
			if (memory != NULL) {
				node = (NpWebApiHandleNode *)operator new(12, memory);
				if (node != NULL) {
					node->handle = NULL;
					sentinel = node;
				}
			}
		}
		list->sentinel = sentinel;
		if (sentinel == NULL) {
			*pResult = NULL;
			return;
		}
		sentinel->prev = sentinel;
		sentinel->next = sentinel;
		allocator = list->allocator;
	}
	part = *pPart;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 12);
		if (memory != NULL) {
			result = (NpWebApiHandleNode *)operator new(12, memory);
			if (result != NULL) {
				result->handle = (NpWebApiHandle *)part;
				result->prev = list->sentinel->prev;
				node = list->sentinel;
				result->next = node;
				node->prev->next = result;
				count = list->count + 1;
				list->sentinel->prev = result;
				list->count = count;
				*pResult = result;
				return;
			}
		}
	}
	*pResult = NULL;
}

int npWebApiRequestAddMultipartPart(NpWebApiRequest *request, const SceNpWebApiMultipartPartParameter *param, int *pIndex)
{
	NpWebApiHandleNode *node;
	NpWebApiLibContext *libCtx;
	NpWebApiMultipartPart *part;
	SceSize remaining;
	int position;
	unsigned int i;
	int offset;
	const char *name;
	int written;
	int result;

	part = NULL;
	if (param->headerNum != 0 && param->pHeaders == NULL)
		return NP_WEBAPI_ERROR_INVALID_ARGUMENT;
	part = (NpWebApiMultipartPart *)npWebApiLibContextPoolAlloc(request->libContext, 1036);
	if (part == NULL)
		goto fail;
	sceClibMemset(part, 0, 0x40C);
	remaining = 1023;
	part->contentLength = param->contentLength;
	position = 0;
	if (param->headerNum != 0) {
		offset = 0;
		i = 0;
		position = 0;
		remaining = 1023;
		do {
			name = *(const char **)((char *)param->pHeaders + offset);
			if (name != NULL && *(const char **)((char *)param->pHeaders + offset + 4) != NULL) {
				if (sceClibStrncmp(name, "Content-Length", sceClibStrnlen("Content-Length", 0xFFFFFFFF)) != 0) {
					position += sceClibSnprintf(&part->header[position], remaining, "%s: %s\r\n",
						*(const char **)((char *)param->pHeaders + offset),
						*(const char **)((char *)param->pHeaders + offset + 4));
					remaining = 1023 - position;
				}
			}
			i++;
			offset += 8;
		} while (i < param->headerNum);
	}
	written = sceClibSnprintf(&part->header[position], remaining, "Content-Length: %d\r\n", param->contentLength);
	sceClibSnprintf(&part->header[position + written], 1023 - (position + written), "\r\n");
	npWebApiLibContextLock(request->libContext);
	npWebApiPartListInsert(&node, (NpWebApiHandleList *)&request->listBHead, &part);
	libCtx = request->libContext;
	if (node == NULL) {
		npWebApiLibContextUnlock(libCtx);
		goto fail;
	}
	part->partId = request->listBCount;
	if (pIndex != NULL)
		*pIndex = part->partId;
	npWebApiLibContextUnlock(libCtx);
	return 0;
fail:
	result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
	if (part != NULL)
		npWebApiLibContextDestroyPool(request->libContext, part);
	return result;
}

int npWebApiAddMultipartPart(SceInt64 requestId, const SceNpWebApiMultipartPartParameter *param, int *pIndex)
{
	NpWebApiLibContext *libCtx;
	NpWebApiUserContext *userCtx;
	NpWebApiRequest *request;
	unsigned int libCtxId;
	int userCtxId;
	int result;

	libCtxId = npWebApiRequestIdGetLibContextId(requestId);
	userCtxId = npWebApiRequestIdGetUserContextId(requestId);
	libCtx = npWebApiFindLibContext(libCtxId, 0);
	if (libCtx != NULL) {
		userCtx = npWebApiLibContextFindUserContext(libCtx, userCtxId);
		if (userCtx != NULL) {
			request = npWebApiUserContextFindRequest(userCtx, requestId);
			if (request != NULL) {
				if (npWebApiRequestGetMultipart(request) != 0 && npWebApiRequestGetFlag204(request) == 0)
					result = npWebApiRequestAddMultipartPart(request, param, pIndex);
				else
					result = NP_WEBAPI_ERROR_MULTIPART_NOT_ALLOWED;
			} else {
				result = NP_WEBAPI_ERROR_REQUEST_NOT_FOUND;
			}
			if (request != NULL)
				npWebApiRequestRelease(request);
		} else {
			result = NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
		}
		if (userCtx != NULL)
			npWebApiUserContextRelease(userCtx);
	} else {
		result = NP_WEBAPI_ERROR_LIB_CONTEXT_NOT_FOUND;
	}
	if (libCtx != NULL)
		npWebApiLibContextRelease(libCtx);
	return result;
}

/* gives a multipart request its content type (the default one, a boundary made when there is none) and
   works out the length of the body from the parts it has */
int npWebApiRequestPrepareMultipart(NpWebApiRequest *request)
{
	char *contentType;
	char *boundary;
	SceSize typeLength;
	SceSize boundaryLength;
	SceSize length;
	SceSize total;
	int offset;
	int i;
	char *text;
	NpWebApiListNode *head;
	NpWebApiListNode *node;
	NpWebApiMultipartPart *part;

	contentType = request->buffer76;
	boundary = request->boundary;
	if (sceClibStrnlen(contentType, 0xFFFFFFFF) == 0)
		sceClibSnprintf(contentType, 64, "multipart/mixed");
	typeLength = sceClibStrnlen(contentType, 0xFFFFFFFF);
	if (sceClibStrnlen(boundary, 0xFFFFFFFF) == 0) {
		sceClibMemset(boundary, 0, 64);
		SceNpCommonPs4_61DAB7E5(sceKernelGetProcessTimeLow());
		offset = sceClibSnprintf(boundary, 63, "---------------------------");
		for (i = 15; i != 0; i--)
			offset += sceClibSnprintf(boundary + offset, 63 - offset, "%x", SceNpCommonPs4_00F74689() % 16);
	}
	boundaryLength = sceClibStrnlen(boundary, 0xFFFFFFFF);
	length = typeLength + boundaryLength + sceClibStrnlen("; boundary=", 0xFFFFFFFF) + 1;
	text = (char *)npWebApiLibContextPoolAlloc(request->libContext, length);
	if (text == NULL)
		return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
	sceClibMemset(text, 0, length);
	sceClibSnprintf(text, length, "%s; boundary=%s", contentType, boundary);
	request->contentType = text;
	npWebApiLibContextLock(request->libContext);
	total = boundaryLength;
	head = request->listBHead;
	node = NULL;
	if (head != NULL)
		node = head->next;
	if (node != head) {
		do {
			part = (NpWebApiMultipartPart *)node->value8;
			length = sceClibStrnlen(part->header, 0xFFFFFFFF);
			node = node->next;
			total += boundaryLength + length + part->contentLength + 6;
		} while (node != request->listBHead);
	}
	npWebApiLibContextUnlock(request->libContext);
	request->totalSize = total + 6;
	request->flag204 = 1;
	return 0;
}
