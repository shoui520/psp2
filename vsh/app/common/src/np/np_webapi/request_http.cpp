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
