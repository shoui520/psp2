/*
 * SceNpWebApi: the headers and the multipart content type of a request. The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiRequestGetMultipart(NpWebApiRequest *request)
{
	return request->multipart;
}

int npWebApiRequestGetFlag204(NpWebApiRequest *request)
{
	return request->flag204;
}

int npWebApiRequestSetMultipartContentType(NpWebApiRequest *request, const char *contentType, const char *boundary)
{
	int result;

	result = 0;
	if ((contentType != NULL && sceClibStrnlen(contentType, 0xFFFFFFFF) > 0x3F)
	    || (boundary != NULL && sceClibStrnlen(boundary, 0xFFFFFFFF) > 0x3F))
		return NP_WEBAPI_ERROR_INVALID_CONTENT_TYPE;
	if (contentType != NULL)
		sceClibStrncpy(request->buffer76, contentType, 0x40);
	if (boundary != NULL)
		sceClibStrncpy(request->boundary, boundary, 0x40);
	return result;
}

/* the list's insert for a pair of values: nodes of 16 bytes around a sentinel made at the first */
void npWebApiPairListInsert(NpWebApiListNode **pResult, NpWebApiList *list, const NpWebApiListPair *pair)
{
	NpWebApiListNode *sentinel;
	NpWebApiListNode *node;
	NpWebApiListNode *result;
	void *memory;
	void *allocator;
	int first;
	int second;
	unsigned long count;

	if (list->sentinel == NULL) {
		sentinel = NULL;
		allocator = list->allocator;
		if (allocator != NULL) {
			memory = SceNpCommonPs4_AA980113(allocator, 16);
			if (memory != NULL) {
				node = (NpWebApiListNode *)operator new(16, memory);
				if (node != NULL) {
					node->value8 = 0;
					node->value12 = 0;
					sentinel = node;
				}
			}
		}
		list->sentinel = sentinel;
		if (sentinel == NULL) {
			*pResult = NULL;
			return;
		}
		*(NpWebApiListNode **)sentinel = sentinel;
		sentinel->next = sentinel;
	}
	first = pair->first;
	allocator = list->allocator;
	second = pair->second;
	if (allocator != NULL) {
		memory = SceNpCommonPs4_AA980113(allocator, 16);
		if (memory != NULL) {
			result = (NpWebApiListNode *)operator new(16, memory);
			if (result != NULL) {
				result->value8 = first;
				result->value12 = second;
				*(NpWebApiListNode **)result = *(NpWebApiListNode **)list->sentinel;
				node = list->sentinel;
				result->next = node;
				(*(NpWebApiListNode **)node)->next = result;
				count = list->count + 1;
				*(NpWebApiListNode **)list->sentinel = result;
				list->count = count;
				*pResult = result;
				return;
			}
		}
	}
	*pResult = NULL;
}

/* a header the library sets itself is skipped without a word */
int npWebApiRequestAddHttpHeader(NpWebApiRequest *request, const char *name, const char *value)
{
	NpWebApiListPair pair;
	NpWebApiListNode *node;
	SceSize length;
	char *nameCopy;
	char *valueCopy;

	sceClibMemset(&pair, 0, 8);
	if (name != NULL) {
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		if (sceClibStrncasecmp("Content-Type", name, length) == 0)
			return 0;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		if (sceClibStrncasecmp("User-Agent", name, length) == 0)
			return 0;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		if (sceClibStrncasecmp("Authorization", name, length) == 0)
			return 0;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		if (sceClibStrncasecmp("Accept-Language", name, length) == 0 && npWebApiGetSdkVersion() < NP_WEBAPI_SDK_3_50)
			return 0;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		if (sceClibStrncasecmp("X-NP-TITLE-ID", name, length) == 0)
			return 0;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		if (sceClibStrncasecmp("X-NP-TITLE-TOKEN", name, length) == 0)
			return 0;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		if (sceClibStrncasecmp("X-SCE-Debug-Settings", name, length) == 0)
			return 0;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		if (sceClibStrncasecmp("NpDebug", name, length) == 0)
			return 0;
		length = sceClibStrnlen(name, 0xFFFFFFFF);
		nameCopy = (char *)npWebApiLibContextPoolAlloc(request->libContext, length + 1);
		pair.first = (int)nameCopy;
		if (nameCopy == NULL)
			goto fail;
		sceClibMemset(nameCopy, 0, length + 1);
		sceClibStrncpy((char *)pair.first, name, length);
	}
	if (value != NULL) {
		length = sceClibStrnlen(value, 0xFFFFFFFF);
		valueCopy = (char *)npWebApiLibContextPoolAlloc(request->libContext, length + 1);
		pair.second = (int)valueCopy;
		if (valueCopy == NULL) {
			nameCopy = (char *)pair.first;
			goto fail;
		}
		sceClibMemset(valueCopy, 0, length + 1);
		sceClibStrncpy((char *)pair.second, value, length);
	}
	npWebApiLibContextLock(request->libContext);
	npWebApiPairListInsert(&node, (NpWebApiList *)&request->listAHead, &pair);
	if (node != NULL) {
		npWebApiLibContextUnlock(request->libContext);
		return 0;
	}
	npWebApiLibContextUnlock(request->libContext);
	nameCopy = (char *)pair.first;
fail:
	if (nameCopy != NULL)
		npWebApiLibContextDestroyPool(request->libContext, nameCopy);
	if (pair.second != 0)
		npWebApiLibContextDestroyPool(request->libContext, (void *)pair.second);
	return NP_WEBAPI_ERROR_OUT_OF_MEMORY;
}

int npWebApiSetMultipartContentType(SceInt64 requestId, const char *contentType, const char *boundary)
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
					result = npWebApiRequestSetMultipartContentType(request, contentType, boundary);
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

int npWebApiAddHttpRequestHeader(SceInt64 requestId, const char *name, const char *value)
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
				if (g_sdkVersion >= NP_WEBAPI_SDK_3_50 && npWebApiRequestGetFlag328(request) != 0)
					result = NP_WEBAPI_ERROR_HEADER_NOT_ALLOWED;
				else
					result = npWebApiRequestAddHttpHeader(request, name, value);
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
