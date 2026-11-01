/*
 * SceLibHttp's helpers: schemes and object IDs, numbers, the end of a line, a URL without
 * its path, the reset of a request that is sent again, the library-wide defaults and the
 * options of a template, connection or request, header lines, and the versions.
 */

#include <net/errno.h>
#include "http_internal.h"

/* the library-wide defaults (sceHttpSetDefaultAcceptEncodingGZIPEnabled, SceHttp_855D5254) */
static SceBool g_httpDefaultAcceptEncodingGzip;		/* option 16 */
static int g_httpDefaultSslExtension;			/* option 22: the SSL glue's larger form */

/* whether scheme is https (*https 1) or http (0) */
int _sceHttpGetScheme(const char *scheme, SceBool *https)
{
	int ret = 0;

	*https = -1;
	if (scheme == NULL || https == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (sceClibStrncasecmp(scheme, "HTTP", 32) == 0)
		*https = 0;
	else if (sceClibStrncasecmp(scheme, "HTTPS", 32) == 0)
		*https = 1;
	else
		return SCE_HTTP_ERROR_UNKNOWN_SCHEME;
	return ret;
}

/* 0 for http; anything else is -1 (the HTTPS comparison's result is not used) */
int _sceHttpSchemeGetType(const char *scheme)
{
	if (scheme != NULL) {
		if (sceClibStrncasecmp(scheme, "HTTP", 32) == 0)
			return 0;
		sceClibStrncasecmp(scheme, "HTTPS", 32);
	}
	return -1;
}

/* what an ID names, by its top four bits */
__attribute__((always_inline))
int _sceHttpGetObjectType(int id)
{
	switch (id & 0xf0000000) {
	case 0x10000000:
		return HTTP_OBJECT_TEMPLATE;
	case 0x20000000:
		return HTTP_OBJECT_CONNECTION;
	case 0x30000000:
		return HTTP_OBJECT_REQUEST;
	default:
		return SCE_HTTP_ERROR_INVALID_ID;
	}
}

/* the decimal number s starts with, up to a space and 20 digits */
SceULong64 _sceHttpStrToULong64(const char *s)
{
	SceULong64 value = 0;
	SceSize n = 0;

	if (s == NULL)
		return value;
	for (; *s != '\0'; s++) {
		if (isspace(*s) || n >= 20)
			break;
		value = value * 10 + (*s - '0');
		n++;
	}
	return value;
}

/* reads up to the next CR LF; the connection is shut down when it closes first */
int _sceHttpRequestSkipLine(HttpRequest *req, int flags)
{
	HttpConnection *conn = _sceHttpRequestGetConnection(req);
	SceUChar8 c;
	int ret;

	while (req->lineState < 3) {
		switch (req->lineState) {
		case 0:
			req->lineState = 1;
			req->lineLen = 0;
			/* fall through */
		case 1:
			ret = _sceHttpRecv(req, &c, 1, flags);
			if (ret < 0)
				return ret;
			if (ret == 0) {
				ret = SCE_HTTP_ERROR_BAD_RESPONSE;
				_sceHttpConnectionShutdown(conn);
				return ret;
			}
			req->lineLen++;
			if (c != '\r')
				break;
			req->lineState = 2;
			/* fall through */
		case 2:
			ret = _sceHttpRecv(req, &c, 1, flags);
			if (ret < 0)
				return ret;
			if (ret == 0) {
				ret = SCE_HTTP_ERROR_BAD_RESPONSE;
				_sceHttpConnectionShutdown(conn);
				return ret;
			}
			req->lineLen++;
			if (c == '\n')
				req->lineState = 3;
			break;
		}
	}
	ret = req->lineLen;
	req->lineState = 0;
	return ret;
}

/* url cut to its host in place, when the host alone is not longer than url */
void _sceHttpUriNormalize(const HttpAllocator *allocator, char *url)
{
	SceHttpUriElement element;
	SceSize require;
	void *pool;

	if (url == NULL || sceHttpUriParse(NULL, url, NULL, &require, 0) < 0)
		return;
	pool = _sceHttpMalloc(allocator->malloc, require);
	if (pool == NULL)
		return;
	if (sceHttpUriParse(&element, url, pool, NULL, require) >= 0) {
		element.opaque = 1;
		if (sceHttpUriBuild(NULL, &require, 0, &element, SCE_HTTP_URI_BUILD_WITH_HOSTNAME) >= 0 &&
		    require <= sceClibStrnlen(url, 0x3fff) + 1)
			sceHttpUriBuild(url, NULL, require, &element, SCE_HTTP_URI_BUILD_WITH_HOSTNAME);
	}
	_sceHttpFree(allocator->free, pool);
}

/*
 * The rest of the response body read and dropped, so the connection can carry the request
 * again; the request is completed (the connection closed when the body did not end).
 */
__attribute__((noinline))
static int _sceHttpRequestDiscardBody(HttpRequest *req)
{
	char buf[128];
	SceBool end = 0;
	SceULong64 rest;
	SceBool close;
	int ret;

	if (req->method == SCE_HTTP_METHOD_HEAD) {
		end = 1;
		ret = 0;
		close = 0;
		goto complete;
	}
	if (req->response.contentLengthKnown == 1) {
		rest = req->response.contentLength - req->receivedLength;
		ret = 0;
		if (rest != 0) {
			for (;;) {
				if (rest >= sizeof(buf))
					ret = _sceHttpRecv(req, buf, sizeof(buf), 0);
				else
					ret = _sceHttpRecv(req, buf, rest, 0);
				if (ret < 0)
					return ret;
				if (ret == 0) {
					close = 1;
					goto complete;
				}
				rest -= ret;
				if (rest == 0)
					break;
			}
		}
		end = 1;
		close = 0;
		goto complete;
	}
	for (;;) {
		if (req->response.chunked == 1)
			ret = _sceHttpRequestReadChunked(req, buf, sizeof(buf), 0, &end, NULL);
		else
			ret = _sceHttpRecv(req, buf, sizeof(buf), 0);
		if (ret < 0)
			break;
		if (ret == 0) {
			close = 1;
			goto complete;
		}
		if (end == 1) {
			close = 0;
			goto complete;
		}
	}
	if (ret != SCE_NET_ERROR_EINACTIVEDISABLED)
		return ret;
	close = 1;
	ret = 0;
complete:
	_sceHttpRequestComplete(req, close, end);
	return ret;
}

/*
 * The request back to its start for sending again (after a redirect or an authentication),
 * with its Proxy-Authorization (proxy), Authorization (www) and Cookie headers taken out;
 * remove first drops the body of the response read so far.
 */
int _sceHttpRequestRemoveAuthHeaders(HttpRequest *req, SceBool remove, SceBool proxy,
				     SceBool www)
{
	int ret = 0;

	if (remove == 1) {
		ret = _sceHttpRequestDiscardBody(req);
		if (ret < 0)
			return ret;
	}
	req->state = 0;
	req->sentLength = 0;
	req->receivedLength = 0;
	req->responseHeaderPos = 0;
	req->chunkState = 0;
	req->chunkRemaining = 0;
	req->responseHeaderState = 0;
	if (proxy == 1)
		_sceHttpHeaderRemove(&req->tmpl.headers, "Proxy-Authorization");
	if (www == 1)
		_sceHttpHeaderRemove(&req->tmpl.headers, "Authorization");
	_sceHttpHeaderRemove(&req->tmpl.headers, "Cookie");
	return ret;
}

int _sceHttpSetDefaultOption(int option, const void *value, SceSize size, void *arg,
			     SceSize argSize)
{
	switch (option) {
	case 16:
		if (value == NULL || size != 4)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		sceClibMemcpy_safe(&g_httpDefaultAcceptEncodingGzip, value, 4);
		return 0;
	case 22:
		if (value == NULL || size != 4)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		sceClibMemcpy_safe(&g_httpDefaultSslExtension, value, 4);
		return 0;
	default:
		return SCE_HTTP_ERROR_INVALID_VALUE;
	}
}

int _sceHttpGetDefaultOption(int option, void *value, SceSize size, void *arg, SceSize argSize)
{
	switch (option) {
	case 16:
		if (value == NULL || size != 4)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		sceClibMemcpy_safe(value, &g_httpDefaultAcceptEncodingGzip, 4);
		return 0;
	case 22:
		if (value == NULL || size != 4)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		sceClibMemcpy_safe(value, &g_httpDefaultSslExtension, 4);
		return 0;
	default:
		return SCE_HTTP_ERROR_INVALID_VALUE;
	}
}

/* the template's proxy server, a copy of the len bytes of server (kept when it is the same) */
int _sceHttpTemplateSetProxyServer(HttpTemplate *tmpl, const char *server, SceSize len)
{
	int ret = 0;

	if (tmpl->proxyServer != NULL && sceClibStrncmp(tmpl->proxyServer, server, len) != 0 &&
	    tmpl->proxyServer != NULL) {
		_sceHttpFree(tmpl->allocator.free, tmpl->proxyServer);
		tmpl->proxyServer = NULL;
	} else if (tmpl->proxyServer != NULL) {
		return ret;
	}
	tmpl->proxyServer = _sceHttpMalloc(tmpl->allocator.malloc, len + 1);
	if (tmpl->proxyServer == NULL)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	sceClibStrncpy(tmpl->proxyServer, server, len + 1);
	return ret;
}

/*
 * The settings of the template, connection or request id, by option (value of size bytes,
 * arg of argSize bytes); a connection and a request change their own copy of the template.
 */
int _sceHttpSetOption(int id, int option, const void *value, SceSize size, void *arg,
		      SceSize argSize)
{
	HttpTemplate *tmpl = NULL;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	int ret;

	ret = _sceHttpGetObjectType(id);
	if (ret < 0)
		goto out;
	if (ret > HTTP_OBJECT_TEMPLATE) {
		if (ret <= HTTP_OBJECT_CONNECTION) {
			ret = _sceHttpGetConnection(&conn, id);
			if (ret < 0)
				goto out;
			ret = _sceHttpConnectionLock(conn, NULL, 0, NULL);
			if (ret < 0)
				goto out;
			tmpl = &conn->tmpl;
		} else {
			ret = _sceHttpGetRequest(&req, id);
			if (ret < 0)
				goto out;
			ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
			if (ret < 0)
				goto out;
			tmpl = &req->tmpl;
		}
	} else {
		ret = _sceHttpGetTemplate(&tmpl, id);
		if (ret < 0)
			goto out;
		ret = _sceHttpLock(0);
		if (ret < 0)
			goto out;
	}

	switch (option) {
	case 0:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->autoRedirect, value, 4);
		break;
	case 1:
		if (size != 4 || argSize != 4)
			goto invalid;
		tmpl->redirectCallback = (SceHttpRedirectCallback)value;
		tmpl->redirectUserArg = arg;
		break;
	case 2:
		if (value == NULL || size != 4)
			goto invalid;
		tmpl->resolveRetry = (int)value;
		break;
	case 3:
		if (value == NULL || size != 4)
			goto invalid;
		tmpl->resolveTimeout = (SceUInt32)value;
		break;
	case 4:
		if (value == NULL || size != 4)
			goto invalid;
		tmpl->connectTimeout = (SceUInt32)value;
		break;
	case 5:
		if (value == NULL || size != 4)
			goto invalid;
		tmpl->sendTimeout = (SceUInt32)value;
		break;
	case 6:
		if (value == NULL || size != 4)
			goto invalid;
		tmpl->recvTimeout = (SceUInt32)value;
		break;
	case 7:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->nonblock, value, 4);
		break;
	case 8:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->epollId, value, 4);
		break;
	case 9:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->epoll, value, 4);
		if (arg != NULL)
			sceClibMemcpy_safe(&tmpl->epollUserArg, &arg, argSize);
		break;
	case 10:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->cookieEnabled, value, 4);
		break;
	case 11:
		if (size != 4 || argSize != 4)
			goto invalid;
		tmpl->cookieSendCallback = (SceHttpCookieSendCallback)value;
		tmpl->cookieSendUserArg = arg;
		break;
	case 12:
		if (size != 4 || argSize != 4)
			goto invalid;
		tmpl->cookieRecvCallback = (SceHttpCookieRecvCallback)value;
		tmpl->cookieRecvUserArg = arg;
		break;
	case 13:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->sslFlags, value, 4);
		break;
	case 14:
		if (value == NULL || size != 1)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->sockFlags, value, 1);
		break;
	case 15:
		if (value == NULL || req == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&req->icmOption, value, 4);
		break;
	case 16:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->acceptEncodingGzipEnabled, value, 4);
		break;
	case 17:
		if (value == NULL || size != 4 || arg == NULL || argSize != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->proxyEnabled, value, 4);
		tmpl->autoProxyConf = arg == NULL;
		break;
	case 18:
		if (value == NULL || arg == NULL || argSize != 2)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->proxyPort, arg, 2);
		ret = _sceHttpTemplateSetProxyServer(tmpl, value, size);
		if (ret < 0)
			goto out;
		break;
	case 19:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->recvBufferSize, value, 4);
		break;
	case 20:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->responseHeaderMaxSize, value, 4);
		break;
	case 21:
		if (value == NULL || size != 4)
			goto invalid;
		tmpl->legacySdk = *(const int *)value == 1;
		break;
	case 23:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(&tmpl->inflateGzipEnabled, value, 4);
		break;
	default:
		goto invalid;
	}
	ret = 0;
	goto out;

invalid:
	ret = SCE_HTTP_ERROR_INVALID_VALUE;
out:
	if (req != NULL) {
		if (tmpl != NULL)
			sceKernelUnlockLwMutex(&req->lock, 1);
		_sceHttpRequestRelease(req);
	} else if (conn != NULL) {
		_sceHttpConnectionUnlock(conn, NULL);
		_sceHttpConnectionRelease(conn);
	} else if (tmpl != NULL) {
		_sceHttpUnlock(0);
		_sceHttpTemplateRelease(tmpl);
	}
	return ret;
}

/* what _sceHttpSetOption set, for the options that can be read back */
int _sceHttpGetOption(int id, int option, void *value, SceSize size, void *arg, SceSize argSize)
{
	HttpTemplate *tmpl = NULL;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	int ret;

	ret = _sceHttpGetObjectType(id);
	if (ret < 0)
		goto out;
	if (ret > HTTP_OBJECT_TEMPLATE) {
		if (ret <= HTTP_OBJECT_CONNECTION) {
			ret = _sceHttpGetConnection(&conn, id);
			if (ret < 0)
				goto out;
			ret = _sceHttpConnectionLock(conn, NULL, 0, NULL);
			if (ret < 0)
				goto out;
			tmpl = &conn->tmpl;
		} else {
			ret = _sceHttpGetRequest(&req, id);
			if (ret < 0)
				goto out;
			ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
			if (ret < 0)
				goto out;
			tmpl = &req->tmpl;
		}
	} else {
		ret = _sceHttpGetTemplate(&tmpl, id);
		if (ret < 0)
			goto out;
		ret = _sceHttpLock(0);
		if (ret < 0)
			goto out;
	}

	switch (option) {
	case 0:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(value, &tmpl->autoRedirect, 4);
		break;
	case 1:
		/* the user argument is read through, not copied: Sony's */
		if (value == NULL || size != 4 || argSize != 4)
			goto invalid;
		sceClibMemcpy_safe(value, &tmpl->redirectCallback, 4);
		sceClibMemcpy_safe(arg, tmpl->redirectUserArg, 4);
		break;
	case 7:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(value, &tmpl->nonblock, 4);
		break;
	case 8:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(value, &tmpl->epollId, 4);
		break;
	case 10:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(value, &tmpl->cookieEnabled, 4);
		break;
	case 13:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(value, &tmpl->sslFlags, 4);
		break;
	case 14:
		if (value == NULL || size != 1)
			goto invalid;
		sceClibMemcpy_safe(value, &tmpl->sockFlags, 1);
		break;
	case 16:
		if (value == NULL || size != 4)
			goto invalid;
		sceClibMemcpy_safe(value, &tmpl->acceptEncodingGzipEnabled, 4);
		break;
	default:
		goto invalid;
	}
	ret = 0;
	goto out;

invalid:
	ret = SCE_HTTP_ERROR_INVALID_VALUE;
out:
	if (req != NULL) {
		if (tmpl != NULL)
			sceKernelUnlockLwMutex(&req->lock, 1);
		_sceHttpRequestRelease(req);
	} else if (conn != NULL) {
		_sceHttpConnectionUnlock(conn, NULL);
		_sceHttpConnectionRelease(conn);
	} else if (tmpl != NULL) {
		_sceHttpUnlock(0);
		_sceHttpTemplateRelease(tmpl);
	}
	return ret;
}

SceBool _sceHttpStrNotEmpty(const char *s)
{
	SceBool ret = 0;

	if (s != NULL)
		return *s != '\0';
	return ret;
}

/* whether s holds a CR or an LF */
SceBool _sceHttpStrHasLineBreak(const char *s)
{
	if (s == NULL)
		return 0;
	for (; *s != '\0'; s++) {
		if (*s == '\n' || *s == '\r')
			return 1;
	}
	return 0;
}

/*
 * A copy of the header block without the line of the field name (matched at the start of a
 * line, without case); *out is NULL when no such line ends in the block.
 */
int _sceHttpRemoveHeaderField(HttpRequest *req, const char *header, SceSize headerSize,
			      const char *name, SceSize nameLen, char **out, SceSize *outSize)
{
	SceBool lineStart = 1;
	SceSize start;
	SceSize end;
	SceSize size;
	SceSize i;
	char *buf;
	int ret = 0;

	if (header == NULL || name == NULL || out == NULL || outSize == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	for (i = 0; i < headerSize; i++) {
		if (lineStart) {
			if (!isspace(header[i]) && nameLen < headerSize - i &&
			    sceClibStrncasecmp(name, &header[i], nameLen) == 0 &&
			    header[i + nameLen] == ':')
				goto found;
			lineStart = 0;
		}
		if (header[i] == '\n')
			lineStart = 1;
	}
	*out = NULL;
	*outSize = 0;
	return ret;

found:
	start = i;
	for (end = i + nameLen + 1; end < headerSize; end++) {
		if (header[end] == '\n')
			break;
	}
	if (end >= headerSize) {
		*out = NULL;
		*outSize = 0;
		return ret;
	}
	end++;
	size = headerSize - (end - start);
	buf = _sceHttpMalloc(req->tmpl.allocator.malloc, size);
	if (buf == NULL)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	sceClibMemcpy_safe(buf, header, start);
	sceClibMemcpy_safe(&buf[start], &header[end], headerSize - end);
	*out = buf;
	*outSize = size;
	return ret;
}

/*
 * The system software version as "%d.%02d" (8 bytes). The major number's tens digit is
 * masked with 0x96, not multiplied by 10: Sony's.
 */
void _sceHttpGetSwVersion(char *version)
{
	SceKernelSystemSwVersion info;

	if (version == NULL)
		return;
	sceClibMemset(version, 0, 8);
	info.size = sizeof(info);
	sceKernelGetSystemSwVersion(&info);
	sceClibSnprintf(version, 8, "%d.%02d",
			((info.version >> 28) & 0x96) + ((info.version >> 24) & 0xf),
			((info.version >> 20) & 0xf) * 10 + ((info.version >> 16) & 0xf));
}

int _sceHttpGetSdkVersion(void)
{
	return SceLibKernel_9F793F84();
}
