/*
 * SceLibHttp requests: the Cookie header a request sends, the User-Agent, and the list of
 * headers a request will send.
 */

#include "http_internal.h"

/* req's Cookie header from the cookie jar, passed to the send callback first when set */
__attribute__((noinline))
int _sceHttpRequestAddCookieHeader(HttpRequest *req)
{
	HttpCookieMatch *match = NULL;
	HttpCookieDomainRef *refs = NULL;
	SceSize required = 0;
	HttpConnection *conn;
	const char *scheme;
	char *buf = NULL;
	int ret;

	conn = _sceHttpRequestGetConnection(req);
	if (conn->https == 0) {
		scheme = "http";
	} else if (conn->https == 1) {
		scheme = "https";
	} else {
		ret = SCE_HTTP_ERROR_UNKNOWN_SCHEME;
		goto out;
	}
	ret = _sceHttpCookieFind(&match, conn->hostname, req->path, scheme, &refs);
	if (ret < 0)
		goto out;
	ret = _sceHttpCookieMakeHeader(NULL, match, &required, 0);
	if (ret < 0)
		goto out;
	buf = _sceHttpMalloc(req->tmpl.allocator.malloc, required);
	if (buf == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	ret = _sceHttpCookieMakeHeader(buf, match, NULL, required);
	if (ret < 0)
		goto out;
	if (req->tmpl.cookieSendCallback != NULL) {
		ret = _sceHttpCookieCallSendCallback(req, req->url, buf);
		if (ret < 0)
			goto out;
		ret = 0;
	}
	_sceHttpAddRequestHeader(req->object.id, "Cookie", buf, SCE_HTTP_HEADER_OVERWRITE);

out:
	if (match != NULL)
		_sceHttpCookieMatchFree(match, refs);
	if (buf != NULL)
		_sceHttpFree(req->tmpl.allocator.free, buf);
	return ret;
}

/* "<name> libhttp/<system version> (PS Vita)", or with buf NULL its size */
__attribute__((noinline))
int _sceHttpMakeUserAgent(char *buf, const char *name, SceSize *size)
{
	char version[8];
	char lib[20];
	SceSize len = 0;

	_sceHttpGetSwVersion(version);
	if (name != NULL)
		len = sceClibStrnlen(name, 0xffffffff);
	len += 30;
	if (buf == NULL) {
		*size = len;
		return 0;
	}
	if (*size < len)
		return SCE_HTTP_ERROR_OUT_OF_SIZE;
	sceClibMemset(buf, 0, len);
	if (name != NULL) {
		sceClibStrlcat(buf, name, len);
		sceClibStrlcat(buf, " ", len);
	}
	sceClibSnprintf(lib, sizeof(lib), "libhttp/%s", version);
	sceClibStrlcat(buf, lib, len);
	sceClibStrlcat(buf, " ", len);
	sceClibStrlcat(buf, "(PS Vita)", len);
	return 0;
}

/*
 * The headers request reqId will send, the Connection, Content-Length and Accept-Encoding
 * ones included; headers NULL counts them. contentLength receives the length's digits
 * (contentLength NULL: *contentLengthSize the size it needs).
 */
__attribute__((noinline))
int _sceHttpRequestGetAllHeaders(int reqId, HttpHeaderPair *headers, SceUInt32 *num,
				 char *contentLength, SceSize *contentLengthSize)
{
	HttpTemplate *tmpl = NULL;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	HttpHeader *h;
	SceBool proxy;
	SceBool keepAlive;
	SceUInt32 n;
	int method;
	int ret;

	ret = _sceHttpGetTemplateById(reqId, &tmpl, &conn, &req);
	if (ret < 0)
		goto fail;
	if (req == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_ID;
		goto fail;
	}
	tmpl = &req->tmpl;
	conn = req->connection;
	ret = _sceHttpLock(0);
	if (ret < 0)
		goto fail;

	n = 0;
	proxy = conn->tmpl.proxyEnabled;
	keepAlive = conn->tmpl.keepAlive;
	method = req->method;
	for (h = req->tmpl.headers; h != NULL; h = h->next) {
		if (headers != NULL) {
			headers[n].name = h->name;
			headers[n].value = h->value;
		}
		n++;
	}
	if (keepAlive) {
		if (proxy == 1 && (conn->https == 0 || method == SCE_HTTP_METHOD_CONNECT)) {
			if (headers != NULL) {
				headers[n].name = "Proxy-Connection";
				headers[n].value = "Keep-Alive";
			}
		} else if (headers != NULL) {
			headers[n].name = "Connection";
			headers[n].value = "Keep-Alive";
		}
	} else if (proxy == 1 && (conn->https == 0 || method == SCE_HTTP_METHOD_CONNECT)) {
		if (headers != NULL) {
			headers[n].name = "Proxy-Connection";
			headers[n].value = "close";
		}
	} else if (headers != NULL) {
		headers[n].name = "Connection";
		headers[n].value = "close";
	}
	n++;
	if (method == SCE_HTTP_METHOD_POST || method == SCE_HTTP_METHOD_PUT ||
	    method == SCE_HTTP_METHOD_DELETE) {
		if (contentLength != NULL && contentLengthSize != NULL)
			sceClibSnprintf(contentLength, *contentLengthSize, "%lld",
					req->contentLength);
		else
			*contentLengthSize = 21;
		if (headers != NULL) {
			headers[n].name = "Content-Length";
			headers[n].value = contentLength;
		}
		n++;
	}
	if (req->tmpl.acceptEncodingGzipEnabled == 1) {
		if (headers != NULL) {
			headers[n].name = "Accept-Encoding: ";
			headers[n].value = "gzip";
		}
		n++;
	}
	_sceHttpUnlock(0);
	*num = n;
	ret = 0;

fail:
	if (req != NULL) {
		if (tmpl != NULL)
			sceKernelUnlockLwMutex(&req->lock, 1);
		_sceHttpRequestRelease(req);
	} else if (conn != NULL) {
		_sceHttpConnectionRelease(conn);
	} else if (tmpl != NULL) {
		_sceHttpTemplateRelease(tmpl);
	}
	return ret;
}
