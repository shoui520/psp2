/*
 * SceNpWebApi: the HTTP request of an NpWebApi request: its URL, its method, and the headers every request
 * carries (authorization, title ID and token, language, debug settings, and the headers the caller added).
 * The names are ours.
 */

#include "np_webapi_internal.h"

int npWebApiRequestCreateHttpRequest(NpWebApiRequest *request, int connId, const char *baseUrl)
{
	NpWebApiUserContext *userCtx;
	int result;
	SceSize length;
	char *url;
	int method;
	int rate;
	NpWebApiListNode *head;
	NpWebApiListNode *node;
	char text[264];
	char titleId[16];
	char rateText[16];
	char language[128];

	result = 0;
	method = 0;
	userCtx = npWebApiLibContextFindUserContext(request->libContext, ((const int *)&request->requestId)[1]);
	if (userCtx == NULL)
		return NP_WEBAPI_ERROR_USER_CONTEXT_NOT_FOUND;
	length = sceClibStrnlen(baseUrl, 0xFFFFFFFF);
	url = (char *)npWebApiLibContextPoolAlloc(request->libContext, length + request->queryLength + 1);
	if (url == NULL) {
		result = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
	} else {
		sceClibMemset(url, 0, length + request->queryLength + 1);
		sceClibStrncpy(url, baseUrl, length);
		sceClibStrncat(url, request->query, request->queryLength);
		if (request->method > 1) {
			if (request->method <= 2)
				method = 4;
			else if (request->method <= 3)
				method = 5;
		} else if ((int)request->method >= 0) {
			method = request->method > 0;
		}
		result = sceHttpCreateRequestWithURL(connId, method, url, (SceULong64)(unsigned int)request->totalSize);
		if (result >= 0) {
			request->httpRequestId = result;
			if (request->totalSize == 0 ||
				((result = sceHttpSetRequestContentLength(result, (SceULong64)(unsigned int)request->totalSize)) >= 0 &&
				(result = sceHttpAddRequestHeader(request->httpRequestId, "Content-Type", request->contentType,
					0)) >= 0)) {
				sceClibMemset(text, 0, 263);
				sceClibSnprintf(text, 263, "%s %s", "Bearer",
					npWebApiAuthObjectGetAccessToken(npWebApiUserContextGetObject44(userCtx)));
				result = sceHttpAddRequestHeader(request->httpRequestId, "Authorization", text, 0);
				if (result >= 0) {
					if (!npWebApiIsAccountsService(request->path) ||
						(sceClibMemset(titleId, 0, 16),
						(result = SceNpCommonPs4_17EB36C2(titleId, 0)) >= 0) &&
						(result = sceHttpAddRequestHeader(request->httpRequestId, "X-NP-TITLE-ID", titleId,
							0)) >= 0) {
						const char *token = (const char *)request->extraData;
						int httpId = request->httpRequestId;

						if (token == NULL)
							token = (const char *)npWebApiRequestGetObject280((NpWebApiRequest *)
								npWebApiUserContextGetObject44(userCtx));
						result = sceHttpAddRequestHeader(httpId, "X-NP-TITLE-TOKEN", token, 0);
						if (result >= 0) {
							sceClibMemset(language, 0, 128);
							result = npWebApiGetLanguageSetting(language, 128);
							if (result >= 0) {
								result = sceHttpAddRequestHeader(request->httpRequestId, "Accept-Language",
									language, 0);
								if (result >= 0 && (!npWebApiLibContextIsIdle(request->libContext) ||
									SceNpCommon_BBC13621() != 1 ||
									(result = sceHttpAddRequestHeader(request->httpRequestId,
										"X-SCE-Debug-Settings", "NpDebug", 0)) >= 0)) {
									npWebApiLibContextLock(request->libContext);
									head = request->listAHead;
									node = NULL;
									if (head != NULL)
										node = head->next;
									if (node == head) {
										goto headersDone;
									}
									for (;;) {
										if (node->value8 != 0 && node->value12 != 0) {
											result = sceHttpAddRequestHeader(request->httpRequestId,
												(const char *)node->value8, (const char *)node->value12, 0);
											head = request->listAHead;
											if (result < 0) {
												npWebApiLibContextUnlock(request->libContext);
												goto done;
											}
										}
										node = node->next;
										if (node == head)
											break;
									}
headersDone:
									rate = SceNpCommonPs4_FA1249E8(npWebApiLibContextUnlock(request->libContext));
									if (npWebApiLibContextIsIdle(request->libContext) && rate < 100) {
										sceClibMemset(rateText, 0, 16);
										result = SceNpCommonPs4_9CAF0FE7(rateText, 16, rate, 10, 0, 0);
										if (result >= 0) {
											sceClibStrlcat(rateText, "%", 16);
											result = sceHttpAddRequestHeader(request->httpRequestId,
												"X-NP-Debug-Fake-RateLimit", rateText, 0);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
done:
	if (url != NULL)
		npWebApiLibContextDestroyPool(request->libContext, url);
	if (userCtx != NULL)
		npWebApiUserContextRelease(userCtx);
	return result;
}

/* asks the account manager for the access of the user (a token handle), has the token for the request
   made (the secure side's, or the shell's, by the mode) and gives both handles back */
int npWebApiAuthObjectRequestAccess(NpWebApiAuthObject *auth, NpWebApiRequest *request, unsigned char mode)
{
	int token;
	int handle;
	int result;
	int params[2];

	handle = 0;
	token = 0;
	result = SceNpManager_4110B787(&token);
	if (result >= 0) {
		npWebApiLibContextLock(auth->libContext);
		npWebApiRequestSetField228(request, token);
		if (npWebApiRequestGetBusy(request) != 0) {
			result = NP_WEBAPI_ERROR_REQUEST_BUSY;
			npWebApiLibContextUnlock(auth->libContext);
			goto done;
		}
		npWebApiLibContextUnlock(auth->libContext);
		result = SceNpManager_FDE78C74(token, auth->accessToken);
		if (result >= 0) {
			npWebApiLibContextLock(auth->libContext);
			SceNpManager_3907F96A(token);
			npWebApiRequestSetField228(request, 0);
			token = 0;
			npWebApiLibContextUnlock(auth->libContext);
			sceClibMemset(params, 0, 8);
			params[0] = npWebApiLibContextZero();
			params[1] = (int)SceNpCommonPs4_31DB2D97(npWebApiLibContextGetPool2(auth->libContext));
			if (mode != 0)
				result = SceNpCommonPs4_FB0B3FBD(params);
			else
				result = SceNpCommonPs4_639086A3(params);
			if (result >= 0) {
				npWebApiLibContextLock(auth->libContext);
				handle = result;
				npWebApiRequestSetField236(request, result);
				if (npWebApiRequestGetBusy(request) != 0) {
					result = NP_WEBAPI_ERROR_REQUEST_BUSY;
					npWebApiLibContextUnlock(auth->libContext);
					goto done;
				}
				npWebApiLibContextUnlock(auth->libContext);
				if (mode != 0)
					result = SceNpCommonPs4_25D1C02B(result, auth->field8, auth->titleToken, 0);
				else
					result = SceNpCommonPs4_13798894(result, auth->field8, auth->titleToken, 0);
				if (result >= 0) {
					npWebApiLibContextLock(auth->libContext);
					if (mode != 0)
						result = SceNpCommonPs4_9C4F1236(handle);
					else
						result = SceNpCommonPs4_530CAFDA(handle);
					npWebApiRequestSetField236(request, 0);
					npWebApiLibContextUnlock(auth->libContext);
					handle = 0;
				}
			}
		}
	}
done:
	npWebApiLibContextLock(auth->libContext);
	if (token != 0) {
		SceNpManager_3907F96A(token);
		npWebApiRequestSetField228(request, 0);
	}
	if (handle != 0) {
		if (mode != 0)
			SceNpCommonPs4_9C4F1236(handle);
		else
			SceNpCommonPs4_530CAFDA(handle);
		npWebApiRequestSetField236(request, 0);
	}
	npWebApiLibContextUnlock(auth->libContext);
	return result;
}

/* has the account's token checked or fetched (by the mode), then the access token read from the account manager */
int npWebApiAuthObjectGetAccess(NpWebApiAuthObject *auth, char *pFlagA, char *pFlagB, unsigned char mode)
{
	int result;

	npWebApiLibContextLock(auth->libContext);
	if (mode != 0) {
		result = SceNpCommonPs4_19B2A80D(auth->field8, pFlagA, auth->titleToken);
		if (result >= 0)
			result = SceNpManager_1F0C6314(auth->accessToken, pFlagB);
	} else {
		result = SceNpCommonPs4_1BF9EFFF(auth->field8, pFlagA, auth->titleToken);
		if (result >= 0)
			result = SceNpManager_1F0C6314(auth->accessToken, pFlagB);
	}
	npWebApiLibContextUnlock(auth->libContext);
	return result;
}

/* sends a piece of the body (the whole of it for a request without parts) */
int npWebApiRequestSendBody(NpWebApiRequest *request, const void *data, SceSize size)
{
	int result;

	result = sceHttpSendRequest(request->httpRequestId, data, size);
	if (result == NP_WEBAPI_HTTP_ERROR_SSL) {
		result = npWebApiConvertHttpError(request->httpRequestId, NP_WEBAPI_HTTP_ERROR_SSL);
		if (result < 0)
			return result;
	} else if (result < 0) {
		return result;
	}
	request->sentSize += size;
	return result;
}

/* the hash of the body the request has sent (it is kept for the response's check) */
void npWebApiRequestFinishBodyHash(NpWebApiRequest *request)
{
	unsigned char context[1856];
	NpWebApiBodyHash hash;

	sceClibMemset(context, 0, 1856);
	sceClibMemset(&hash, 0, 8);
	SceNpCommonPs4_358A8F68(context, &hash);
	SceNpCommonPs4_AD229434(context, request->data, request->field316);
	request->field312 = hash.value;
}

/* reads the response into the buffer of the caller (as much as fits) and hashes all of it */
int npWebApiRequestReadResponse(NpWebApiRequest *request, NpWebApiResponseBuffer *buffer)
{
	unsigned char context[1856];
	NpWebApiBodyHash hash;
	char chunk[32];
	int total;
	int result;
	int room;
	SceSize length;

	total = 0;
	sceClibMemset(context, 0, 1856);
	sceClibMemset(&hash, 0, 8);
	SceNpCommonPs4_358A8F68(context, &hash);
	for (;;) {
		sceClibMemset(chunk, 0, 32);
		result = sceHttpReadData(request->httpRequestId, chunk, 31);
		if (result < 0)
			break;
		if (result > 0) {
			if (buffer != NULL && buffer->data != NULL && buffer->size != 0) {
				room = buffer->size - total - 1;
				if (room > 0) {
					length = result;
					if (room < result)
						length = room;
					sceClibMemcpy_safe((char *)buffer->data + total, chunk, length);
				}
			}
			total += result;
			SceNpCommonPs4_AD229434(context, chunk, result);
			if (result > 0)
				continue;
		}
		if (hash.flag != 0)
			request->field312 = hash.value;
		if (buffer != NULL)
			buffer->length = total;
		break;
	}
	return result;
}

/* the URL of the service the request names, from the list the library keeps and, when the account manager
   knows a new one, from it */
int npWebApiPushObjectGetServiceUrl(NpWebApiPushObject *object, const char *service, char *pFound, char **pUrl)
{
	unsigned char idle;
	int result;
	int created;
	NpWebApiServiceUrl *entry;
	NpWebApiServiceUrl *node;
	char url[128];
	NpWebApiLibContext *libCtx;

	idle = 0;
	npWebApiLibContextGetAborted(object->libContext, &idle);
	sceClibMemset(url, 0, 128);
	if (idle != 0)
		result = SceNpCommonPs4_2A1D2A4B(service, pFound, url, 128);
	else
		result = SceNpCommonPs4_28372F53(service, pFound, url, 128);
	if (result < 0)
		return result;
	if (*pFound != 0) {
		npWebApiLibContextLock(object->libContext);
		npWebApiLibContextLock(object->libContext);
		for (entry = object->head; entry != NULL; entry = entry->next) {
			if (sceClibStrncmp(entry->name, service, sceClibStrnlen(service, 0xFFFFFFFF)) == 0)
				break;
		}
		npWebApiLibContextUnlock(object->libContext);
		libCtx = object->libContext;
		if (entry != NULL) {
			npWebApiLibContextLock(libCtx);
			sceClibMemset(entry->url, 0, 128);
			sceClibMemcpy_safe(entry->url, url, sceClibStrnlen(url, 0xFFFFFFFF));
			npWebApiLibContextUnlock(object->libContext);
			result = 0;
		} else {
			node = (NpWebApiServiceUrl *)npWebApiLibContextPoolAlloc(libCtx, 208);
			created = NP_WEBAPI_ERROR_OUT_OF_MEMORY;
			if (node != NULL) {
				sceClibMemset(node, 0, 208);
				sceClibStrncpy(node->name, service, 32);
				sceClibStrncpy(node->url, url, 128);
				entry = node;
				created = 0;
			}
			result = created;
			if (created < 0) {
				npWebApiLibContextUnlock(object->libContext);
				return result;
			}
			npWebApiLibContextLock(object->libContext);
			if (object->head != NULL)
				entry->next = object->head;
			created = object->count + 1;
			object->head = entry;
			object->count = created;
			npWebApiLibContextUnlock(object->libContext);
		}
		npWebApiLibContextUnlock(object->libContext);
		if (entry != NULL && pUrl != NULL)
			*pUrl = entry->url;
	}
	return result;
}
