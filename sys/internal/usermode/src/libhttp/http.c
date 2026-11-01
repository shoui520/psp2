/*
 * SceLibHttp's exports: the library's set-up, and every sceHttp call checked against it
 * before it goes to the template, connection, request, cookie or SSL code.
 */

#include <kernel/modulemgr.h>

#include "http_internal.h"

int g_httpInitialized = 0;

/* SCE_HTTP_ERROR_BEFORE_INIT until sceHttpInit, with the library's lock taken and given back */
static inline int _sceHttpCheckInit(void)
{
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	if (g_httpInitialized == 0) {
		ret = _sceHttpUnlock(0);
		if (ret >= 0)
			ret = SCE_HTTP_ERROR_BEFORE_INIT;
		return ret;
	}
	return _sceHttpUnlock(0);
}

int module_start(SceSize arglen, const void *argp)
{
	if (_sceHttpCreateLock() < 0)
		return SCE_KERNEL_START_FAILED;
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize arglen, const void *argp)
{
	if (_sceHttpDeleteLock() < 0)
		return SCE_KERNEL_STOP_FAIL;
	return SCE_KERNEL_STOP_SUCCESS;
}

SceInt32 sceHttpInit(SceSize poolSize)
{
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	if (g_httpInitialized == 1) {
		ret = _sceHttpUnlock(0);
		if (ret >= 0)
			ret = SCE_HTTP_ERROR_ALREADY_INITED;
		return ret;
	}
	ret = _sceHttpUnlock(0);
	if (ret < 0)
		return ret;

	ret = _sceHttpPoolCreate(poolSize);
	if (ret < 0)
		return ret;
	ret = _sceHttpObjectInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpCookieInit(NULL, NULL);
	if (ret < 0)
		return ret;
	ret = _sceHttpAuthCacheInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpRedirectCacheInit();
	if (ret < 0)
		return ret;
	ret = SceHttp_E0825488(_sceHttpDefaultMalloc, _sceHttpDefaultFree);
	if (ret < 0)
		return ret;
	_sceHttpsInit();
	_sceHttpCookieNop2();
	_sceHttpDbgInit();

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	g_httpInitialized = 1;
	return _sceHttpUnlock(0);
}

SceInt32 sceHttpTerm(void)
{
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	if (g_httpInitialized == 0) {
		ret = _sceHttpUnlock(0);
		if (ret >= 0)
			ret = SCE_HTTP_ERROR_BEFORE_INIT;
		return ret;
	}
	ret = _sceHttpUnlock(0);
	if (ret < 0)
		return ret;

	_sceHttpObjectTerm();
	ret = _sceHttpCookieTerm();
	if (ret < 0)
		return ret;
	ret = _sceHttpRedirectCacheTerm();
	if (ret < 0)
		return ret;
	ret = _sceHttpAuthCacheTerm();
	if (ret < 0)
		return ret;
	ret = SceHttp_11C9BCE8();
	if (ret < 0)
		return ret;
	ret = _sceHttpPoolDestroy();
	if (ret < 0)
		return ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	g_httpInitialized = 0;
	ret = _sceHttpUnlock(0);
	if (ret < 0)
		return ret;
	return 0;
}

SceInt32 sceHttpGetMemoryPoolStats(SceHttpMemoryPoolStats *currentStat)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (currentStat == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpPoolGetStats(NULL, currentStat);
}

SceInt32 sceHttpCreateTemplate(const char *userAgent, SceInt32 httpVer, SceBool autoProxyConf)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCreateTemplate(NULL, userAgent, httpVer, autoProxyConf);
}

SceInt32 sceHttpDeleteTemplate(SceInt32 tmplId)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpDeleteTemplate(tmplId);
	if (ret < 0)
		return ret;
	return 0;
}

SceInt32 sceHttpCreateConnectionWithURL(SceInt32 tmplId, const char *url, SceBool enableKeepalive)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCreateConnectionWithURL(NULL, tmplId, url, enableKeepalive, 0, NULL);
}

SceInt32 sceHttpCreateConnection(SceInt32 tmplId, const char *serverName, const char *scheme,
				 SceUShort16 port, SceBool enableKeepalive)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCreateConnection(tmplId, serverName, scheme, port, enableKeepalive);
}

SceInt32 sceHttpDeleteConnection(SceInt32 connId)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpDeleteConnection(connId);
	if (ret < 0)
		return ret;
	return 0;
}

SceInt32 sceHttpCreateRequestWithURL(SceInt32 connId, SceInt32 method, const char *url,
				     SceULong64 contentLength)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCreateRequestWithURL(connId, method, url, contentLength);
}

/* a method's name as its number; HTTP_METHOD_EXTRA for one of its own */
static __attribute__((noinline)) int _sceHttpGetMethod(const char *method)
{
	if (sceClibStrncmp(method, "GET", 3) == 0)
		return SCE_HTTP_METHOD_GET;
	if (sceClibStrncmp(method, "HEAD", 4) == 0)
		return SCE_HTTP_METHOD_HEAD;
	if (sceClibStrncmp(method, "POST", 4) == 0)
		return SCE_HTTP_METHOD_POST;
	if (sceClibStrncmp(method, "PUT", 3) == 0)
		return SCE_HTTP_METHOD_PUT;
	if (sceClibStrncmp(method, "TRACE", 5) == 0)
		return SCE_HTTP_METHOD_TRACE;
	if (sceClibStrncmp(method, "DELETE", 6) == 0)
		return SCE_HTTP_METHOD_DELETE;
	return HTTP_METHOD_EXTRA;
}

SceInt32 sceHttpCreateRequestWithURL2(SceInt32 connId, const char *method, const char *url,
				      SceULong64 contentLength)
{
	int ret, m;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	m = _sceHttpGetMethod(method);
	ret = _sceHttpCreateRequestWithURL(connId, m, url, contentLength);
	if (ret >= 0 && m == HTTP_METHOD_EXTRA && _sceHttpRequestSetMethod(ret, method) < 0)
		_sceHttpDeleteRequest(ret);
	return ret;
}

SceInt32 sceHttpCreateRequest(SceInt32 connId, SceInt32 method, const char *path,
			      SceULong64 contentLength)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCreateRequest(connId, method, path, contentLength);
}

SceInt32 sceHttpDeleteRequest(SceInt32 reqId)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpDeleteRequest(reqId);
	if (ret < 0)
		return ret;
	return 0;
}

SceInt32 sceHttpCreateRequest2(SceInt32 connId, const char *method, const char *path,
			       SceULong64 contentLength)
{
	int ret, m;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	m = _sceHttpGetMethod(method);
	ret = _sceHttpCreateRequest(connId, m, path, contentLength);
	if (ret >= 0 && m == HTTP_METHOD_EXTRA && _sceHttpRequestSetMethod(ret, method) < 0)
		_sceHttpDeleteRequest(ret);
	return ret;
}

SceInt32 sceHttpAbortRequest(SceInt32 reqId)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpAbortRequest(reqId, 0);
	if (ret < 0)
		return ret;
	return 0;
}

int sceHttpAbortRequestForce(int reqId)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpAbortRequest(reqId, 1);
	if (ret < 0)
		return ret;
	return 0;
}

SceInt32 sceHttpSendRequest(SceInt32 reqId, const void *postData, SceSize size)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpSendRequest(reqId, postData, size);
	if (ret < 0)
		return ret;
	return 0;
}

SceInt32 sceHttpReadData(SceInt32 reqId, void *data, SceSize size)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (data == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpReadData(reqId, data, size);
}

SceInt32 sceHttpGetResponseContentLength(SceInt32 reqId, SceULong64 *contentLength)
{
	HttpRequest *req = NULL;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (contentLength == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
		goto release;
	}
	ret = _sceHttpGetRequest(&req, reqId);
	if (ret < 0)
		goto release;
	ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
	if (ret < 0)
		goto release;

	if (req->state == 0) {
		ret = SCE_HTTP_ERROR_BEFORE_SEND;
	} else if (req->state == 19) {
		ret = req->lastError;
	} else if (req->response.contentLengthKnown && req->gzipEncoded != 1) {
		ret = 0;
		*contentLength = req->response.contentLength;
	} else {
		ret = SCE_HTTP_ERROR_NO_CONTENT_LENGTH;
		if (req->response.chunked == 1)
			ret = SCE_HTTP_ERROR_CHUNK_ENC;
	}

	if (req != NULL)
		sceKernelUnlockLwMutex(&req->lock, 1);
release:
	if (req != NULL)
		_sceHttpRequestRelease(req);
	return ret;
}

SceInt32 sceHttpGetStatusCode(SceInt32 reqId, SceInt32 *statusCode)
{
	HttpRequest *req = NULL;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (statusCode == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
		goto release;
	}
	ret = _sceHttpGetRequest(&req, reqId);
	if (ret < 0)
		goto release;
	ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
	if (ret < 0)
		goto release;

	if (req->state < 15) {
		ret = SCE_HTTP_ERROR_BEFORE_SEND;
	} else if (req->state == 19) {
		ret = req->lastError;
	} else {
		ret = 0;
		*statusCode = req->response.statusCode;
	}

	if (req != NULL)
		sceKernelUnlockLwMutex(&req->lock, 1);
release:
	if (req != NULL)
		_sceHttpRequestRelease(req);
	return ret;
}

SceInt32 sceHttpGetLastErrno(SceInt32 reqId, SceInt32 *errNum)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (errNum == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetLastErrno(reqId, errNum);
}

SceInt32 sceHttpAddRequestHeader(SceInt32 id, const char *name, const char *value, SceUInt32 mode)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (mode != SCE_HTTP_HEADER_OVERWRITE && mode != SCE_HTTP_HEADER_ADD)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpAddRequestHeader(id, name, value, mode);
}

int sceHttpAddRequestHeaderRaw(int id, const char *header, int mode)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (mode != SCE_HTTP_HEADER_ADD)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpAddRequestHeaderRaw(id, header, SCE_HTTP_HEADER_ADD);
}

SceInt32 sceHttpRemoveRequestHeader(SceInt32 id, const char *name)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpRemoveRequestHeader(id, name);
}

/*
 * The response's header lines, NUL-terminated; from SDK 3.50 a gzip response the library
 * inflates loses its Content-Length and Content-Encoding lines, which describe the
 * compressed body.
 */
SceInt32 sceHttpGetAllResponseHeaders(SceInt32 reqId, char **header, SceSize *headerSize)
{
	HttpRequest *req = NULL;
	char *copy;
	char *out;
	SceSize outSize;
	SceSize len;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (header == NULL || headerSize == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
		goto release;
	}
	ret = _sceHttpGetRequest(&req, reqId);
	if (ret < 0)
		goto release;
	ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
	if (ret < 0)
		goto release;
	ret = _sceHttpRequestWaitResponse(req);
	if (ret < 0)
		goto unlock;

	if (req->allResponseHeaders == NULL) {
		len = req->responseHeaderLen;
		copy = _sceHttpMalloc(req->tmpl.allocator.malloc, len + 1);
		if (copy == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto unlock;
		}
		sceClibMemcpy_safe(copy, req->responseHeader, req->responseHeaderLen);
		copy[len] = '\0';
		req->allResponseHeaders = copy;
		req->allResponseHeadersSize = len + 1;
	}

	if ((SceUInt32)_sceHttpGetSdkVersion() >= 0x3500000 && req->gzipEncoded == 1 &&
	    req->tmpl.inflateGzipEnabled) {
		out = NULL;
		outSize = 0;
		ret = _sceHttpRemoveHeaderField(req, req->allResponseHeaders,
			req->allResponseHeadersSize, "Content-Length", 14, &out, &outSize);
		if (ret < 0)
			goto unlock;
		if (out != NULL) {
			_sceHttpFree(req->tmpl.allocator.free, req->allResponseHeaders);
			req->allResponseHeaders = out;
			req->responseHeaderSize = outSize;
		}
		ret = _sceHttpRemoveHeaderField(req, req->allResponseHeaders,
			req->allResponseHeadersSize, "Content-Encoding", 16, &out, &outSize);
		if (ret < 0)
			goto unlock;
		if (out != NULL) {
			_sceHttpFree(req->tmpl.allocator.free, req->allResponseHeaders);
			req->allResponseHeaders = out;
			req->responseHeaderSize = outSize;
		}
	}

	ret = 0;
	copy = req->allResponseHeaders;
	len = req->allResponseHeadersSize;
	*header = copy;
	*headerSize = len;

unlock:
	if (req != NULL)
		sceKernelUnlockLwMutex(&req->lock, 1);
release:
	if (req != NULL)
		_sceHttpRequestRelease(req);
	return ret;
}

SceInt32 sceHttpSetRequestContentLength(SceInt32 id, SceULong64 contentLength)
{
	HttpRequest *req = NULL;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpGetRequest(&req, id);
	if (ret < 0)
		goto release;
	ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
	if (ret < 0)
		goto release;

	ret = SCE_HTTP_ERROR_BUSY;
	if (req->state == 0) {
		ret = 0;
		req->contentLength = contentLength;
	}

	if (req != NULL)
		sceKernelUnlockLwMutex(&req->lock, 1);
release:
	if (req != NULL)
		_sceHttpRequestRelease(req);
	return ret;
}

SceInt32 sceHttpSetAuthInfoCallback(SceInt32 id, SceHttpAuthInfoCallback cbfunc, void *userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetAuthInfoCallback(id, cbfunc, userArg);
}

SceInt32 sceHttpSetAuthEnabled(SceInt32 id, SceBool enable)
{
	SceBool digest = enable;
	SceBool basic = enable;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetAuthEnabled(id, &basic, &digest);
}

SceInt32 sceHttpGetAuthEnabled(SceInt32 id, SceBool *enable)
{
	SceBool digest, basic;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (enable == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = _sceHttpGetAuthEnabled(id, &basic, &digest);
	if (ret >= 0)
		*enable = (basic | digest) != 0;
	return ret;
}

SceInt32 sceHttpSetRedirectCallback(SceInt32 id, SceHttpRedirectCallback cbfunc, void *userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 1, cbfunc, 4, userArg);
}

SceInt32 sceHttpSetAutoRedirect(SceInt32 id, SceBool enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 0, &enable, 4, NULL);
}

SceInt32 sceHttpGetAutoRedirect(SceInt32 id, SceBool *enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (enable == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetOption(id, 0, enable, 4, NULL, 0);
}

SceInt32 sceHttpSetResolveTimeOut(SceInt32 id, SceUInt32 usec)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 3, (const void *)usec, 4, NULL);
}

SceInt32 sceHttpSetResolveRetry(SceInt32 id, SceInt32 retry)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (retry < 0)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpSetOption(id, 2, (const void *)retry, 4, NULL);
}

SceInt32 sceHttpSetConnectTimeOut(SceInt32 id, SceUInt32 usec)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 4, (const void *)usec, 4, NULL);
}

SceInt32 sceHttpSetSendTimeOut(SceInt32 id, SceUInt32 usec)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 5, (const void *)usec, 4, NULL);
}

SceInt32 sceHttpSetRecvTimeOut(SceInt32 id, SceUInt32 usec)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 6, (const void *)usec, 4, NULL);
}

SceInt32 sceHttpSetNonblock(SceInt32 id, SceBool enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 7, &enable, 4, NULL);
}

int SceHttp_43A7CC70(int id, int value)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 19, &value, 4, NULL);
}

SceInt32 sceHttpGetNonblock(SceInt32 id, SceBool *enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (enable == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetOption(id, 7, enable, 4, NULL, 0);
}

int sceHttpSetEpollId(int id, int epollId, void *userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 8, &epollId, 4, userArg);
}

int sceHttpGetEpollId(int id, int *epollId, void **userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (epollId == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetOption(id, 8, &epollId, 4, userArg, 4);
}

SceInt32 sceHttpWaitRequest(SceHttpEpollHandle eh, SceHttpNBEvent *nbev, int maxevents, int timeout)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpWaitRequest(eh, nbev, maxevents, timeout, 0);
}

SceInt32 sceHttpWaitRequestCB(SceHttpEpollHandle eh, SceHttpNBEvent *nbev, int maxevents,
			      int timeout)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpWaitRequest(eh, nbev, maxevents, timeout, 1);
}

SceInt32 sceHttpAbortWaitRequest(SceHttpEpollHandle eh)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpAbortWaitRequest(eh);
}

SceInt32 sceHttpCreateEpoll(SceHttpEpollHandle *eh)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCreateEpoll(eh);
}

SceInt32 sceHttpSetEpoll(SceInt32 id, SceHttpEpollHandle eh, void *userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpSetOption(id, 9, &eh, 4, userArg);
	if (ret >= 0)
		ret = _sceHttpEpollAdd(eh, id, userArg);
	return ret;
}

SceInt32 sceHttpUnsetEpoll(SceInt32 id)
{
	HttpRequest *req = NULL;
	SceHttpEpollHandle eh;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpGetRequest(&req, id);
	if (ret < 0)
		goto release;
	ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
	if (ret < 0)
		goto release;

	_sceHttpRequestAbortNonblock(req);
	ret = _sceHttpEpollRemove(req->tmpl.epoll, id);
	if (ret >= 0) {
		eh = NULL;
		ret = _sceHttpSetOption(id, 9, &eh, 4, NULL);
	}

	if (req != NULL)
		sceKernelUnlockLwMutex(&req->lock, 1);
release:
	if (req != NULL)
		_sceHttpRequestRelease(req);
	return ret;
}

SceInt32 sceHttpGetEpoll(SceInt32 id, SceHttpEpollHandle *eh, void **userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (eh == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetOption(id, 9, &eh, 4, userArg, 4);
}

SceInt32 sceHttpDestroyEpoll(SceHttpEpollHandle eh)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpDestroyEpoll(eh);
}

SceInt32 sceHttpGetCookie(const char *url, char *cookie, SceSize *required, SceSize prepared,
			  SceBool secure)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCookieGet(url, cookie, prepared, required);
}

SceInt32 sceHttpSetCookieEnabled(SceInt32 id, SceBool enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 10, &enable, 4, NULL);
}

SceInt32 sceHttpGetCookieEnabled(SceInt32 id, SceBool *enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (enable == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetOption(id, 10, enable, 4, NULL, 0);
}

SceInt32 sceHttpAddCookie(const char *url, const char *cookie, SceSize cookieLength)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = SCE_HTTP_ERROR_INVALID_VALUE;
	if (url != NULL && cookie != NULL)
		return _sceHttpCookieAddUrl(url, cookie, cookieLength);
	return ret;
}

SceInt32 sceHttpSetCookieRecvCallback(SceInt32 id, SceHttpCookieRecvCallback cbfunc, void *userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 12, cbfunc, 4, userArg);
}

SceInt32 sceHttpSetCookieSendCallback(SceInt32 id, SceHttpCookieSendCallback cbfunc, void *userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 11, cbfunc, 4, userArg);
}

SceInt32 sceHttpCookieExport(void *buffer, SceSize bufferSize, SceSize *exportSize)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (buffer == NULL && exportSize == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpCookieExport(buffer, bufferSize, exportSize);
}

SceInt32 sceHttpCookieImport(const void *buffer, SceSize bufferSize)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (buffer == NULL || bufferSize == 0)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpCookieImport(buffer, bufferSize);
}

SceInt32 sceHttpSetCookieTotalMaxSize(SceUInt32 size)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpCookieFlush();
	if (ret < 0)
		return ret;
	return _sceHttpCookieSetTotalMaxSize(size);
}

SceInt32 sceHttpSetCookieMaxSize(SceUInt32 size)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpCookieFlush();
	if (ret < 0)
		return ret;
	return _sceHttpCookieSetMaxSize(size);
}

SceInt32 sceHttpSetCookieMaxNum(SceUInt32 num)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpCookieFlush();
	if (ret < 0)
		return ret;
	return _sceHttpCookieSetMaxNum(num);
}

SceInt32 sceHttpSetCookieMaxNumPerDomain(SceUInt32 num)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	ret = _sceHttpCookieFlush();
	if (ret < 0)
		return ret;
	return _sceHttpCookieSetMaxNumPerDomain(num);
}

SceInt32 sceHttpGetCookieStats(SceHttpCookieStats *stats)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCookieGetStats(stats);
}

SceInt32 sceHttpsLoadCert(SceInt32 caCertNum, const SceHttpsData **caList, const SceHttpsData *cert,
			  const SceHttpsData *privKey)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpsLoadCert(caCertNum, caList, cert, privKey);
}

SceInt32 sceHttpsUnloadCert(void)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpsUnloadCert();
}

SceInt32 sceHttpsEnableOption(SceUInt32 sslFlags)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if ((sslFlags & HTTPS_FLAGS_ALL) != sslFlags)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpsEnableOption(sslFlags);
}

/*
 * A game built with SDK 2.10 or later may turn the server checks off only on a system set
 * up to allow it.
 */
static inline SceBool _sceHttpsMayDisable(SceUInt32 sslFlags)
{
	int value;

	if ((sceAppMgrIsGameProgram() == 0 ||
	     sceKernelIsCalledFromSysModule(__builtin_return_address(0)) != 0) &&
	    (SceUInt32)_sceHttpGetSdkVersion() >= 0x2100000 &&
	    (sslFlags & HTTPS_FLAGS_SERVER_CHECKS) != 0 &&
	    (sceRegMgrSystemParamGetInt(HTTP_REGMGR_SSL_VERIFY_DISABLE, &value) != 0 || value == 0))
		return 0;
	return 1;
}

SceInt32 sceHttpsDisableOption(SceUInt32 sslFlags)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if ((sslFlags & HTTPS_FLAGS_ALL) != sslFlags)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (!_sceHttpsMayDisable(sslFlags))
		return HTTPS_ERROR_DISABLE_OPTION_NOT_ALLOWED;
	return _sceHttpsDisableOption(sslFlags);
}

SceInt32 sceHttpsEnableOption2(SceInt32 id, SceUInt32 sslFlags)
{
	SceUInt32 flags;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if ((sslFlags & HTTPS_FLAGS_ALL) != sslFlags)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = _sceHttpGetOption(id, 13, &flags, 4, NULL, 0);
	if (ret < 0)
		return ret;
	flags |= sslFlags | HTTPS_FLAGS_SET;
	return _sceHttpSetOption(id, 13, &flags, 4, NULL);
}

SceInt32 sceHttpsDisableOption2(SceInt32 id, SceUInt32 sslFlags)
{
	SceUInt32 flags;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if ((sslFlags & HTTPS_FLAGS_ALL) != sslFlags)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (!_sceHttpsMayDisable(sslFlags))
		return HTTPS_ERROR_DISABLE_OPTION_NOT_ALLOWED;
	ret = _sceHttpGetOption(id, 13, &flags, 4, NULL, 0);
	if (ret < 0)
		return ret;
	flags = (flags & ~sslFlags) | HTTPS_FLAGS_SET;
	return _sceHttpSetOption(id, 13, &flags, 4, NULL);
}

int sceHttpsEnableOptionPrivate(int id, SceUInt32 sslFlags)
{
	SceUInt32 flags;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if ((sslFlags & HTTPS_FLAGS_PRIVATE) != sslFlags)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = _sceHttpGetOption(id, 13, &flags, 4, NULL, 0);
	if (ret < 0)
		return ret;
	flags |= sslFlags | HTTPS_FLAGS_SET;
	return _sceHttpSetOption(id, 13, &flags, 4, NULL);
}

int sceHttpsDisableOptionPrivate(int id, SceUInt32 sslFlags)
{
	SceUInt32 flags;
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if ((sslFlags & HTTPS_FLAGS_PRIVATE) != sslFlags)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (!_sceHttpsMayDisable(sslFlags))
		return HTTPS_ERROR_DISABLE_OPTION_NOT_ALLOWED;
	ret = _sceHttpGetOption(id, 13, &flags, 4, NULL, 0);
	if (ret < 0)
		return ret;
	flags = (flags & ~sslFlags) | HTTPS_FLAGS_SET;
	return _sceHttpSetOption(id, 13, &flags, 4, NULL);
}

SceInt32 sceHttpsSetSslCallback(SceInt32 id, SceHttpsCallback cbfunc, void *userArg)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpsSetSslCallback(id, cbfunc, userArg);
}

SceInt32 sceHttpsGetSslError(SceInt32 id, SceInt32 *errNum, SceUInt32 *detail)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (errNum == NULL || detail == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpsGetSslError(id, errNum, detail);
}

SceInt32 sceHttpsGetCaList(SceHttpsCaList *caList)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (caList == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpsGetCaList(&caList->caCerts, &caList->caNum);
}

SceInt32 sceHttpsFreeCaList(SceHttpsCaList *caList)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (caList == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpsFreeCaList(caList->caCerts, caList->caNum);
}

/* no init check */
int SceHttp_175685B8(int id, int version)
{
	if (version != 2 && version != 3)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpsSetSslVersion(id, version, 1);
}

SceInt32 sceHttpAuthCacheFlush(void)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpAuthCacheFlush();
}

SceInt32 sceHttpRedirectCacheFlush(void)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpRedirectCacheFlush();
}

SceInt32 sceHttpCookieFlush(void)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpCookieFlush();
}

int SceHttp_F30073EC(int id, SceUInt8 value)
{
	SceUInt8 buf[4];
	int ret;

	buf[0] = value;
	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 14, buf, 1, NULL);
}

int SceHttp_30F5D11F(int id, int *value)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (value == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetOption(id, 14, value, 4, NULL, 0);
}

int sceHttpSetIcmOption(int id, int option)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 15, &option, 4, NULL);
}

int sceHttpGetIcmOption(int id, int *option)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (option == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetOption(id, 15, option, 4, NULL, 0);
}

int sceHttpRequestGetAllHeaders(int reqId, HttpHeaderPair *headers, SceUInt32 *num,
				char *contentLength, SceSize *contentLengthSize)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (headers == NULL && num == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpRequestGetAllHeaders(reqId, headers, num, contentLength, contentLengthSize);
}

int sceHttpSslIsCtxCreated(int id, SceBool *created)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (created == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpSslIsCtxCreated(id, created);
}

int sceHttpSetAcceptEncodingGZIPEnabled(int id, SceBool enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 16, &enable, 4, NULL);
}

int sceHttpGetAcceptEncodingGZIPEnabled(int id, SceBool *enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (enable == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	return _sceHttpGetOption(id, 16, enable, 4, NULL, 0);
}

int sceHttpSetDefaultAcceptEncodingGZIPEnabled(SceBool enable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetDefaultOption(16, &enable, 4, NULL, 0);
}

/* the proxy: on or off, and the server and port */
int SceHttp_53B722F2(int id, SceBool enable, int mode, const char *server, SceUShort16 port)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (server == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = _sceHttpSetOption(id, 17, &enable, 4, &mode);
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 18, server, sceClibStrnlen(server, 0xffffffff), &port);
}

SceInt32 sceHttpSetResponseHeaderMaxSize(SceInt32 id, SceSize headerSize)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 20, &headerSize, 4, NULL);
}

int SceHttp_C498765A(int id, int value)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetOption(id, 21, &value, 4, NULL);
}

SceInt32 sceHttpSetInflateGZIPEnabled(SceInt32 id, SceBool isEnable)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	if (g_httpInitialized == 0)
		return SCE_HTTP_ERROR_BEFORE_INIT;
	return _sceHttpSetOption(id, 23, &isEnable, 4, NULL);
}

int SceHttp_855D5254(int value)
{
	int ret;

	ret = _sceHttpCheckInit();
	if (ret < 0)
		return ret;
	return _sceHttpSetDefaultOption(22, &value, 4, NULL, 0);
}
