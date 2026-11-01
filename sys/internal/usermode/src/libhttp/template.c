/*
 * SceLibHttp templates: a new template with the library's defaults, and the copy of a
 * template's settings every connection and request starts from.
 */

#include "http_internal.h"

int _sceHttpCreateTemplate(const HttpAllocator *allocator, const char *userAgent, int httpVer,
			   SceBool autoProxyConf)
{
	void *(*mallocFunc)(SceSize size) = _sceHttpDefaultMalloc;
	void (*freeFunc)(void *ptr) = _sceHttpDefaultFree;
	HttpTemplate *tmpl;
	HttpHeader *header;
	char *agent;
	SceSize size;
	int gzip;
	int ret;

	if (allocator != NULL) {
		mallocFunc = allocator->malloc;
		freeFunc = allocator->free;
	}
	if (httpVer <= 0 || httpVer > 2)
		return SCE_HTTP_ERROR_INVALID_VERSION;
	tmpl = _sceHttpMalloc(mallocFunc, sizeof(*tmpl));
	if (tmpl == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	sceClibMemset(tmpl, 0, sizeof(*tmpl));
	ret = _sceHttpMakeUserAgent(NULL, userAgent, &size);
	if (ret < 0)
		goto fail;
	agent = _sceHttpMalloc(mallocFunc, size);
	if (agent == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	} else {
		ret = _sceHttpMakeUserAgent(agent, userAgent, &size);
		if (ret >= 0) {
			ret = _sceHttpHeaderCreate(NULL, &header, "User-Agent", agent);
			if (ret >= 0) {
				if (tmpl->headers == NULL)
					header->next = NULL;
				tmpl->httpVer = httpVer;
				tmpl->headers = header;
				tmpl->autoProxyConf = autoProxyConf;
				tmpl->resolveRetry = 5;
				tmpl->resolveTimeout = 1000000;
				tmpl->connectTimeout = 30000000;
				tmpl->sendTimeout = 120000000;
				tmpl->recvTimeout = 120000000;
				tmpl->recvBufferSize = 1500;
				tmpl->maxRedirects = 6;
				tmpl->maxAuthRetries = 6;
				tmpl->responseHeaderMaxSize = 5000;
				tmpl->autoRedirect = 1;
				tmpl->cookieEnabled = 1;
				tmpl->basicAuthEnabled = 1;
				tmpl->digestAuthEnabled = 1;
				tmpl->legacySdk = (SceUInt32)_sceHttpGetSdkVersion() < 0x3100000;
				tmpl->proxyAuthEnabled = 1;
				ret = _sceHttpGetDefaultOption(16, &gzip, 4, NULL, 0);
				if (ret >= 0) {
					tmpl->allocator.malloc = mallocFunc;
					tmpl->allocator.free = freeFunc;
					tmpl->sslFlags = 101;
					tmpl->acceptEncodingGzipEnabled = gzip;
					tmpl->sslVersion = 3;
					tmpl->inflateGzipEnabled = 1;
					tmpl->freeObject = freeFunc;
					tmpl->epollId = -1;
					ret = _sceHttpObjectRegister(&tmpl->object, HTTP_OBJECT_TEMPLATE);
					_sceHttpFree(freeFunc, agent);
					return ret;
				}
			}
		}
	}
	if (agent != NULL)
		_sceHttpFree(freeFunc, agent);
fail:
	if (tmpl != NULL) {
		_sceHttpHeaderFree(tmpl->headers);
		_sceHttpFree(freeFunc, tmpl);
	}
	return ret;
}

int _sceHttpDeleteTemplate(int tmplId)
{
	HttpTemplate *tmpl;
	int ret;

	ret = _sceHttpTakeObject((HttpObject **)&tmpl, tmplId, HTTP_OBJECT_TEMPLATE);
	if (ret >= 0)
		ret = _sceHttpTemplateRelease(tmpl);
	return ret;
}

/* copy's own strings and headers, from tmpl's allocator; nothing of its list place */
int _sceHttpTemplateCopy(const HttpTemplate *tmpl, HttpTemplate *copy)
{
	SceSize len;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		goto fail;
	sceClibMemcpy_safe(copy, tmpl, sizeof(*copy));
	copy->object.next = NULL;
	copy->object.prev = NULL;
	copy->proxyServer = NULL;
	copy->username = NULL;
	copy->password = NULL;
	copy->headers = NULL;
	copy->object.refCount = 0;
	copy->allocations = NULL;
	if (tmpl->proxyServer != NULL) {
		len = sceClibStrnlen(tmpl->proxyServer, 0xff);
		copy->proxyServer = _sceHttpMalloc(tmpl->allocator.malloc, len + 1);
		if (copy->proxyServer == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto unlock;
		}
		sceClibStrncpy(copy->proxyServer, tmpl->proxyServer, len);
		copy->proxyServer[len] = '\0';
	}
	if (tmpl->username != NULL) {
		len = sceClibStrnlen(tmpl->username, SCE_HTTP_USERNAME_MAX_SIZE);
		copy->username = _sceHttpMalloc(tmpl->allocator.malloc, len + 1);
		if (copy->username == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto unlock;
		}
		sceClibStrncpy(copy->username, tmpl->username, len);
		copy->username[len] = '\0';
	}
	if (tmpl->password != NULL) {
		len = sceClibStrnlen(tmpl->password, SCE_HTTP_PASSWORD_MAX_SIZE);
		copy->password = _sceHttpMalloc(tmpl->allocator.malloc, len + 1);
		if (copy->password == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto unlock;
		}
		sceClibStrncpy(copy->password, tmpl->password, len);
		copy->password[len] = '\0';
	}
	ret = _sceHttpHeaderCopy(&tmpl->allocator, &copy->headers, tmpl->headers);
unlock:
	_sceHttpUnlock(0);
	if (ret >= 0)
		return ret;
fail:
	if (copy->proxyServer != NULL) {
		_sceHttpFree(tmpl->allocator.free, copy->proxyServer);
		copy->proxyServer = NULL;
	}
	if (copy->username != NULL) {
		_sceHttpFree(tmpl->allocator.free, copy->username);
		copy->username = NULL;
	}
	if (copy->password != NULL) {
		_sceHttpFree(tmpl->allocator.free, copy->password);
		copy->password = NULL;
	}
	return ret;
}

int _sceHttpTemplateClear(HttpTemplate *tmpl)
{
	int ret = 0;

	if (tmpl->headers != NULL) {
		_sceHttpHeaderFree(tmpl->headers);
		tmpl->headers = NULL;
	}
	if (tmpl->allocations != NULL)
		ret = _sceHttpFreeList(tmpl->allocations);
	if (tmpl->proxyServer != NULL) {
		_sceHttpFree(tmpl->allocator.free, tmpl->proxyServer);
		ret = 0;
		tmpl->proxyServer = NULL;
	}
	if (tmpl->username != NULL) {
		_sceHttpFree(tmpl->allocator.free, tmpl->username);
		ret = 0;
		tmpl->username = NULL;
	}
	if (tmpl->password != NULL) {
		_sceHttpFree(tmpl->allocator.free, tmpl->password);
		tmpl->password = NULL;
		return 0;
	}
	return ret;
}
