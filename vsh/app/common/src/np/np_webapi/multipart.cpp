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
