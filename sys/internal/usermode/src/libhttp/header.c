/*
 * SceLibHttp headers: the header lists of templates and requests, the request header a
 * request sends and the response header it receives.
 */

#include "http_internal.h"

/* a header name: value, its strings from allocator (NULL: the library's pool) */
__attribute__((noinline))
int _sceHttpHeaderCreate(const HttpAllocator *allocator, HttpHeader **header, const char *name,
			 const char *value)
{
	void *(*mallocFunc)(SceSize size);
	void (*freeFunc)(void *ptr);
	HttpHeader *h;
	SceSize len;

	if (name == NULL || value == NULL || header == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	*header = NULL;

	mallocFunc = _sceHttpDefaultMalloc;
	freeFunc = _sceHttpDefaultFree;
	if (allocator != NULL) {
		mallocFunc = allocator->malloc;
		freeFunc = allocator->free;
	}

	h = _sceHttpMalloc(mallocFunc, sizeof(*h));
	if (h != NULL) {
		sceClibMemset(h, 0, sizeof(*h));
		len = sceClibStrnlen(name, 0xfff);
		h->name = _sceHttpMalloc(mallocFunc, len + 1);
		if (h->name != NULL) {
			sceClibMemcpy_safe(h->name, name, len);
			h->name[len] = '\0';
			len = sceClibStrnlen(value, 0xffff);
			h->value = _sceHttpMalloc(mallocFunc, len + 1);
			if (h->value != NULL) {
				sceClibMemcpy_safe(h->value, value, len);
				h->value[len] = '\0';
				h->free = freeFunc;
				*header = h;
				return 0;
			}
		}
	}

	if (h != NULL) {
		if (h->name != NULL)
			_sceHttpFree(freeFunc, h->name);
		if (h->value != NULL)
			_sceHttpFree(freeFunc, h->value);
		_sceHttpFree(freeFunc, h);
	}
	return SCE_HTTP_ERROR_OUT_OF_MEMORY;
}

/* header and every header after it */
void _sceHttpHeaderFree(HttpHeader *header)
{
	void (*freeFunc)(void *ptr);
	HttpHeader *next;

	while (header != NULL) {
		freeFunc = header->free;
		if (header->name != NULL)
			_sceHttpFree(freeFunc, header->name);
		if (header->value != NULL)
			_sceHttpFree(freeFunc, header->value);
		next = header->next;
		_sceHttpFree(freeFunc, header);
		header = next;
	}
}

/* a copy of list, from allocator (NULL, or a NULL function: the library's pool) */
__attribute__((noinline))
int _sceHttpHeaderCopy(const HttpAllocator *allocator, HttpHeader **copy, const HttpHeader *list)
{
	void *(*mallocFunc)(SceSize size);
	void (*freeFunc)(void *ptr);
	HttpHeader *first;
	HttpHeader *h;
	SceSize size;

	if (allocator == NULL || (freeFunc = allocator->free) == NULL)
		freeFunc = _sceHttpDefaultFree;
	if (allocator == NULL || (mallocFunc = allocator->malloc) == NULL)
		mallocFunc = _sceHttpDefaultMalloc;

	if (list == NULL) {
		*copy = NULL;
		return 0;
	}

	h = _sceHttpMalloc(mallocFunc, sizeof(*h));
	if (h == NULL)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	first = h;
	sceClibMemset(h, 0, sizeof(*h));
	while (list != NULL) {
		if (list->name != NULL) {
			size = sceClibStrnlen(list->name, 0xfff) + 1;
			h->name = _sceHttpMalloc(mallocFunc, size);
			if (h->name == NULL)
				goto fail;
			sceClibStrncpy(h->name, list->name, size - 1);
			h->name[size - 1] = '\0';
		}
		if (list->value != NULL) {
			size = sceClibStrnlen(list->value, 0xffff) + 1;
			h->value = _sceHttpMalloc(mallocFunc, size);
			if (h->value == NULL)
				goto fail;
			sceClibStrncpy(h->value, list->value, size - 1);
			h->value[size - 1] = '\0';
		}
		if (list->next != NULL) {
			h->next = _sceHttpMalloc(mallocFunc, sizeof(*h));
			if (h->next == NULL)
				goto fail;
			sceClibMemset(h->next, 0, sizeof(*h));
		}
		h->free = freeFunc;
		list = list->next;
		h = h->next;
	}
	*copy = first;
	return 0;

fail:
	h = first;
	while (h != NULL) {
		HttpHeader *next;
		void (*hFree)(void *ptr) = h->free;

		if (h->name != NULL)
			_sceHttpFree(hFree, h->name);
		if (h->value != NULL)
			_sceHttpFree(hFree, h->value);
		next = h->next;
		_sceHttpFree(hFree, h);
		h = next;
	}
	return SCE_HTTP_ERROR_OUT_OF_MEMORY;
}

/* appends s (n bytes) to buf when the header so far, len, fits in size */
#define HEADER_PUT(s, n)							\
	do {									\
		if (!countOnly && size >= len)					\
			sceClibStrncat(buf, s, n);				\
	} while (0)

/*
 * The request line and header of req into buf (size bytes, cleared by the caller), or with
 * countOnly only their length. Returns the length with the terminating NUL; 0 for a method
 * it does not know.
 */
__attribute__((noinline))
SceSize _sceHttpRequestMakeHeader(HttpRequest *req, char *buf, SceSize size, SceBool countOnly)
{
	SceSize require = 0;
	SceHttpUriElement uri;
	char contentLength[21];
	HttpConnection *conn;
	HttpHeader *h;
	const char *method;
	const char *connection;
	SceSize methodLen;
	SceSize connectionLen;
	SceSize len;
	int option;

	sceClibMemset(&uri, 0, sizeof(uri));
	conn = _sceHttpRequestGetConnection(req);
	switch (req->method) {
	case SCE_HTTP_METHOD_GET:
		method = "GET";
		methodLen = 3;
		break;
	case SCE_HTTP_METHOD_POST:
		method = "POST";
		methodLen = 4;
		break;
	case SCE_HTTP_METHOD_HEAD:
		method = "HEAD";
		methodLen = 4;
		break;
	case SCE_HTTP_METHOD_OPTIONS:
		method = "OPTIONS";
		methodLen = 7;
		break;
	case SCE_HTTP_METHOD_PUT:
		method = "PUT";
		methodLen = 3;
		break;
	case SCE_HTTP_METHOD_DELETE:
		method = "DELETE";
		methodLen = 6;
		break;
	case SCE_HTTP_METHOD_TRACE:
		method = "TRACE";
		methodLen = 5;
		break;
	case SCE_HTTP_METHOD_CONNECT:
		method = "CONNECT";
		methodLen = 7;
		break;
	case HTTP_METHOD_EXTRA:
		method = req->extra->method;
		methodLen = sceClibStrnlen(method, 0xffffffff);
		break;
	default:
		return 0;
	}

	/* the request line */
	len = methodLen;
	HEADER_PUT(method, size - 1);
	len++;
	HEADER_PUT(" ", 1);

	uri.hostname = conn->hostname;
	uri.port = conn->port;
	if (req->method != SCE_HTTP_METHOD_CONNECT) {
		uri.path = req->path;
		uri.query = req->query;
		uri.fragment = req->fragment;
	}
	if (uri.path == NULL || sceClibStrnlen(uri.path, 2) == 0 ||
	    (sceClibStrnlen(uri.path, 2) == 1 && *uri.path == ' '))
		uri.path = "/";
	if (req->method == SCE_HTTP_METHOD_CONNECT) {
		option = SCE_HTTP_URI_BUILD_WITH_HOSTNAME | SCE_HTTP_URI_BUILD_WITH_PORT;
		uri.opaque = 1;
	} else if (conn->tmpl.proxyEnabled == 1 && conn->tmpl.proxyServer != NULL &&
		   conn->https == 0) {
		uri.scheme = "http";
		option = SCE_HTTP_URI_BUILD_WITH_SCHEME | SCE_HTTP_URI_BUILD_WITH_HOSTNAME |
			 SCE_HTTP_URI_BUILD_WITH_PORT | SCE_HTTP_URI_BUILD_WITH_PATH |
			 SCE_HTTP_URI_BUILD_WITH_QUERY | SCE_HTTP_URI_BUILD_WITH_FRAGMENT;
	} else {
		uri.opaque = 1;
		option = SCE_HTTP_URI_BUILD_WITH_PATH | SCE_HTTP_URI_BUILD_WITH_QUERY |
			 SCE_HTTP_URI_BUILD_WITH_FRAGMENT;
	}
	sceHttpUriBuild(NULL, &require, 0, &uri, option);
	len += require;
	if (!countOnly) {
		if (size >= len - 1)
			sceHttpUriBuild(buf + sceClibStrnlen(buf, len - 1), NULL, require, &uri,
					option);
		if (size >= len)
			sceClibStrncat(buf, " ", 1);
	}
	if (!countOnly && size >= len + 8) {
		if (req->tmpl.httpVer == SCE_HTTP_VERSION_1_0)
			sceClibStrncat(buf, "HTTP/1.0", 8);
		else
			sceClibStrncat(buf, "HTTP/1.1", 8);
	}
	len += 10;
	HEADER_PUT("\r\n", 2);

	/* the header lines: for CONNECT only Host and Proxy-Authorization */
	for (h = req->tmpl.headers; h != NULL; h = h->next) {
		if (req->method == SCE_HTTP_METHOD_CONNECT) {
			if (sceClibStrncasecmp(h->name, "Host", 0x1000) != 0 &&
			    sceClibStrncasecmp(h->name, "Proxy-Authorization", 0x1000) != 0)
				continue;
		} else if (conn->https == 1 &&
			   sceClibStrncasecmp(h->name, "Proxy-Authorization", 0x1000) == 0) {
			continue;
		}
		len += sceClibStrnlen(h->name, 0xfff);
		HEADER_PUT(h->name, 0xfff);
		len += 2;
		HEADER_PUT(": ", 2);
		if (h->value != NULL) {
			len += sceClibStrnlen(h->value, 0xffff);
			HEADER_PUT(h->value, 0xffff);
		}
		len += 2;
		HEADER_PUT("\r\n", 2);
	}

	if (conn->tmpl.keepAlive) {
		if (conn->tmpl.proxyEnabled == 1 &&
		    (conn->https == 0 || req->method == SCE_HTTP_METHOD_CONNECT)) {
			connection = "Proxy-Connection: Keep-Alive";
			connectionLen = 28;
		} else {
			connection = "Connection: Keep-Alive";
			connectionLen = 22;
		}
	} else {
		if (conn->tmpl.proxyEnabled == 1 &&
		    (conn->https == 0 || req->method == SCE_HTTP_METHOD_CONNECT)) {
			connection = "Proxy-Connection: close";
			connectionLen = 23;
		} else {
			connection = "Connection: close";
			connectionLen = 17;
		}
	}
	len += connectionLen;
	HEADER_PUT(connection, connectionLen);
	len += 2;
	HEADER_PUT("\r\n", 2);

	if (req->method == SCE_HTTP_METHOD_POST || req->method == HTTP_METHOD_EXTRA ||
	    req->method == SCE_HTTP_METHOD_PUT || req->method == SCE_HTTP_METHOD_DELETE) {
		len += 16;
		HEADER_PUT("Content-Length: ", 16);
		sceClibMemset(contentLength, 0, sizeof(contentLength));
		sceClibSnprintf(contentLength, sizeof(contentLength), "%lld", req->contentLength);
		len += sceClibStrnlen(contentLength, 20);
		HEADER_PUT(contentLength, sceClibStrnlen(contentLength, 20));
		len += 2;
		HEADER_PUT("\r\n", 2);
	}

	if (req->tmpl.acceptEncodingGzipEnabled == 1) {
		if (!countOnly && size >= len + 17)
			sceClibStrncat(buf, "Accept-Encoding: ", 17);
		if (!countOnly && size >= len + 21)
			sceClibStrncat(buf, "gzip", 4);
		len += 23;
		HEADER_PUT("\r\n", 2);
	}

	/* the empty line and the NUL */
	len += 3;
	HEADER_PUT("\r\n", 2);
	return len;
}

/* req's request header in a buffer from its allocator; returns its size */
__attribute__((noinline))
int _sceHttpRequestCreateHeader(char **header, HttpRequest *req)
{
	char *buf = NULL;
	SceSize size;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	size = _sceHttpRequestMakeHeader(req, NULL, 0, 1);
	buf = _sceHttpMalloc(req->tmpl.allocator.malloc, size);
	if (buf != NULL) {
		sceClibMemset(buf, 0, size);
		_sceHttpRequestMakeHeader(req, buf, size, 0);
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	_sceHttpUnlock(0);
	if (ret >= 0) {
		*header = buf;
		return size;
	}
	if (buf != NULL)
		_sceHttpFree(req->tmpl.allocator.free, buf);
	return ret;
}

__attribute__((noinline))
int _sceHttpRequestFreeHeader(HttpRequest *req, char **header)
{
	if (*header != NULL) {
		_sceHttpFree(req->tmpl.allocator.free, *header);
		*header = NULL;
	}
	req->requestHeaderSize = 0;
	req->requestHeaderSent = 0;
	return 0;
}

/*
 * Reads the response header a byte at a time up to its empty line, growing the buffer by
 * 1500 bytes up to the template's maximum. A legacy-SDK request takes a response that does
 * not start with "HTTP/" as HTTP/0.9. Returns the header's length.
 */
__attribute__((noinline))
int _sceHttpReadResponseHeader(HttpRequest *req)
{
	int state = req->responseHeaderState;
	SceSize pos = req->responseHeaderPos;
	char *buf = req->responseHeader;
	SceSize size;
	SceSize max;
	int ret;
	char c;

	if (state == 0) {
		if (req->tmpl.legacySdk == 1) {
			ret = _sceHttpPeek(req, buf, 5, 0);
			if (ret == SCE_HTTP_ERROR_OUT_OF_SIZE) {
				ret = SCE_HTTP_ERROR_BAD_RESPONSE;
				goto out;
			}
			if (ret <= 0)
				goto out;
			if (sceClibStrncmp(buf, "HTTP/", ret) != 0) {
				sceClibStrncpy(buf, "HTTP/0.9 200 OK\r\n\r\n", req->responseHeaderSize);
				req->responseHeaderLen = 20;
				goto done;
			}
		}
		state = 1;
	}

	while (pos < req->tmpl.responseHeaderMaxSize) {
		ret = _sceHttpRecv(req, &buf[pos], 1, 0);
		if (ret == 0) {
			buf[pos] = '\0';
			ret = SCE_HTTP_ERROR_BAD_RESPONSE;
			goto out;
		}
		if (ret < 0)
			goto out;

		/* 1: in a line, 2: CR, 3: end of line, 4: CR after it, 5: the empty line */
		c = buf[pos];
		switch (state) {
		case 2:
			state = c == '\n' ? 3 : 1;
			break;
		case 3:
			if (c == '\r')
				state = 4;
			else if (c == '\n')
				state = 5;
			else
				state = 1;
			break;
		case 4:
			state = c == '\n' ? 5 : 1;
			break;
		case 5:
			break;
		default:
			if (c == '\r') {
				state = 2;
				goto next;
			}
			if (c == '\n') {
				state = 3;
				goto next;
			}
			break;
		}
		if (state == 5) {
			req->responseHeaderLen = pos + 1;
			break;
		}
next:
		size = req->responseHeaderSize;
		max = req->tmpl.responseHeaderMaxSize;
		if (pos == size - 1) {
			if (max == size) {
				ret = SCE_HTTP_ERROR_TOO_LARGE_RESPONSE_HEADER;
				goto out;
			}
			if (max > size + 1500)
				max = size + 1500;
			buf = _sceHttpMalloc(req->tmpl.allocator.malloc, max);
			if (buf == NULL) {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
				goto out;
			}
			sceClibMemset(buf, 0, max);
			sceClibMemcpy_safe(buf, req->responseHeader, req->responseHeaderSize);
			_sceHttpFree(req->tmpl.allocator.free, req->responseHeader);
			req->responseHeaderSize = max;
			req->responseHeader = buf;
		}
		pos++;
	}
	if (state != 5) {
		ret = SCE_HTTP_ERROR_BAD_RESPONSE;
		goto out;
	}

done:
	ret = req->responseHeaderLen;
	req->responseHeaderState = state;
	req->responseHeaderPos = pos;
	return ret;

out:
	req->responseHeaderState = state;
	req->responseHeaderPos = pos;
	return ret;
}

/* the status line and the headers a request keeps */
__attribute__((noinline))
int _sceHttpResponseParse(const char *header, SceSize len, HttpResponse *resp)
{
	int ret;

	ret = sceHttpParseStatusLine(header, len, &resp->httpMajorVer, &resp->httpMinorVer,
				     &resp->statusCode, &resp->reasonPhrase,
				     &resp->reasonPhraseLen);
	if (ret < 0)
		return ret;
	sceHttpParseResponseHeader(header, len, "Date", &resp->date, &resp->dateLen);
	sceHttpParseResponseHeader(header, len, "Server", &resp->server, &resp->serverLen);
	sceHttpParseResponseHeader(header, len, "ETag", &resp->etag, &resp->etagLen);
	sceHttpParseResponseHeader(header, len, "Last-Modifiled", &resp->lastModified,
				   &resp->lastModifiedLen);
	sceHttpParseResponseHeader(header, len, "Location", &resp->location, &resp->locationLen);
	sceHttpParseResponseHeader(header, len, "WWW-Authenticate", &resp->wwwAuthenticate,
				   &resp->wwwAuthenticateLen);
	return 0;
}

/*
 * Receives req's response header and reads what libhttp needs from it: the length, the
 * cookies it sets (when cookies are on), chunked and gzip transfer, and whether the
 * connection closes. An interim response (1xx) is skipped. Returns the header's length.
 */
__attribute__((noinline))
int _sceHttpRequestRecvResponseHeader(HttpRequest *req, HttpConnection *conn)
{
	char contentLength[21];
	const char *value;
	SceSize valueLen;
	const char *encoding;
	SceSize encodingLen;
	const char *transfer;
	SceSize transferLen;
	HttpCookie *list;
	SceUInt32 count;
	SceSize size;
	const char *cookie;
	SceSize cookieLen;
	const char *header;
	SceSize headerLen;
	SceSize pos;
	SceUInt32 maxNum;
	SceBool chunked;
	int status;
	int ret;
	int err;

	for (;;) {
		ret = _sceHttpReadResponseHeader(req);
		if (ret < 0)
			break;
		if ((req->responseHeaderState | ret) == 0)
			break;
		ret = _sceHttpResponseParse(req->responseHeader, req->responseHeaderLen,
					    &req->response);
		if (ret < 0)
			break;

		valueLen = 0;
		value = NULL;
		if (sceHttpParseResponseHeader(req->responseHeader, req->responseHeaderLen,
					       "Content-Length", &value, &valueLen) <= 0) {
			req->response.contentLengthKnown = 0;
		} else {
			req->response.contentLengthKnown = 1;
			req->response.chunked = 0;
			if (valueLen > 20)
				valueLen = 20;
			sceClibMemcpy_safe(contentLength, value, valueLen);
			contentLength[valueLen] = '\0';
			req->response.contentLength = _sceHttpStrToULong64(contentLength);
		}

		if (req->tmpl.cookieEnabled == 1) {
			header = req->responseHeader;
			headerLen = req->responseHeaderLen;
			list = NULL;
			count = 0;
			size = 0;
			pos = 0;
			maxNum = _sceHttpCookieGetMaxNumPerDomain();
			err = 0;
			while (pos < headerLen && maxNum != 0 && req->tmpl.cookieEnabled == 1) {
				ret = sceHttpParseResponseHeader(header + pos, headerLen - pos,
								 "Set-Cookie", &cookie, &cookieLen);
				if (ret < 0) {
					err = SCE_HTTP_ERROR_NOT_FOUND;
					break;
				}
				pos += ret;
				err = _sceHttpCookieParseHeader(cookie, cookieLen,
								req->connection->hostname, req->path,
								&list, &count, &size);
				if (err < 0)
					break;
				if (list == NULL)
					continue;
				if (_sceHttpCookieCallRecvCallback(req, req->url, cookie,
								   cookieLen) == 0) {
					err = _sceHttpCookieAdd(list, size, count);
					if (err >= 0)
						continue;
					_sceHttpCookieFreeList(list);
					list = NULL;
					break;
				}
				_sceHttpCookieFreeList(list);
				list = NULL;
				err = 0;
			}
			ret = err < 0 ? err : 0;
			if (ret < 0 && ret != SCE_HTTP_ERROR_NOT_FOUND)
				break;
		}

		if (sceHttpParseResponseHeader(req->responseHeader, req->responseHeaderLen,
					       "Transfer-Encoding", &transfer, &transferLen) > 0 &&
		    sceClibStrncasecmp(transfer, "chunked", 7) == 0) {
			req->response.contentLengthKnown = 0;
			req->response.contentLength = 0;
			chunked = 1;
		} else {
			chunked = 0;
		}
		req->response.chunked = chunked;
		req->gzipEncoded = sceHttpParseResponseHeader(req->responseHeader,
							      req->responseHeaderLen,
							      "Content-Encoding", &encoding,
							      &encodingLen) > 0 &&
				   sceClibStrncasecmp(encoding, "gzip", 4) == 0;

		req->response.close = 0;
		if (conn->tmpl.proxyEnabled == 1 &&
		    (conn->https == 0 || req->method == SCE_HTTP_METHOD_CONNECT))
			ret = sceHttpParseResponseHeader(req->responseHeader, req->responseHeaderLen,
							 "Proxy-Connection", &req->connectionValue,
							 &req->connectionValueLen);
		else
			ret = sceHttpParseResponseHeader(req->responseHeader, req->responseHeaderLen,
							 "Connection", &req->connectionValue,
							 &req->connectionValueLen);
		if (ret < 0) {
			if (conn->tmpl.proxyEnabled != 0 && conn->https != 1)
				req->response.close = 1;
			else if (req->response.httpMajorVer != 1 || req->response.httpMinorVer != 1)
				req->response.close = 1;
		} else if (sceClibStrncasecmp(req->connectionValue, "close",
					      req->connectionValueLen) == 0) {
			req->response.close = 1;
		}

		status = req->response.statusCode;
		if (status != 100 && status != 101 && status != 102 &&
		    ((SceUInt32)_sceHttpGetSdkVersion() < 0x3000000 ||
		     req->response.statusCode <= 102 || req->response.statusCode >= 200)) {
			ret = req->responseHeaderLen;
			break;
		}
		_sceHttpRequestResetResponseHeader(req);
	}
	return ret;
}

__attribute__((noinline))
int _sceHttpRequestGetResponseHeader(HttpRequest *req)
{
	int ret;

	if (req == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = _sceHttpRequestRecvResponseHeader(req, _sceHttpRequestGetConnection(req));
	if (ret >= 0)
		return req->responseHeaderLen;
	return ret;
}

/* removes the first header called name from list */
int _sceHttpHeaderRemove(HttpHeader **list, const char *name)
{
	void (*freeFunc)(void *ptr);
	HttpHeader *prev = NULL;
	HttpHeader *next;
	HttpHeader *h;
	int ret = 0;
	int cmp;

	if (list == NULL || name == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	for (h = *list; h != NULL; h = next) {
		cmp = sceClibStrncasecmp(h->name, name, 0xfff);
		next = h->next;
		if (cmp == 0) {
			if (prev != NULL)
				prev->next = next;
			else
				*list = next;
			freeFunc = h->free;
			_sceHttpFree(freeFunc, h->name);
			_sceHttpFree(freeFunc, h->value);
			_sceHttpFree(freeFunc, h);
			return ret;
		}
		prev = h;
	}
	return SCE_HTTP_ERROR_NOT_FOUND;
}

/* removes every header called name from the template, connection or request id */
__attribute__((noinline))
int _sceHttpRemoveRequestHeader(int id, const char *name)
{
	HttpTemplate *tmpl = NULL;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	SceBool removed = 0;
	int ret;

	if (name == NULL) {
		ret = SCE_HTTP_ERROR_NOT_FOUND;
		goto out;
	}
	ret = _sceHttpGetObjectType(id);
	if (ret < 0)
		goto out;
	if (ret == HTTP_OBJECT_TEMPLATE) {
		ret = _sceHttpGetTemplate(&tmpl, id);
		if (ret < 0)
			goto out;
	} else if (ret == HTTP_OBJECT_CONNECTION) {
		ret = _sceHttpGetConnection(&conn, id);
		if (ret < 0)
			goto out;
		ret = _sceHttpConnectionLock(conn, NULL, 0, NULL);
		if (ret < 0)
			goto out;
		tmpl = &conn->tmpl;
	} else if (ret == HTTP_OBJECT_REQUEST) {
		ret = _sceHttpGetRequest(&req, id);
		if (ret < 0)
			goto out;
		ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
		if (ret < 0)
			goto out;
		tmpl = &req->tmpl;
	} else {
		ret = 0;
		goto out;
	}

	ret = _sceHttpLock(0);
	if (ret >= 0) {
		while ((ret = _sceHttpHeaderRemove(&tmpl->headers, name)) >= 0)
			removed = 1;
		_sceHttpUnlock(0);
	}
	if (req != NULL)
		sceKernelUnlockLwMutex(&req->lock, 1);

out:
	if (req != NULL) {
		_sceHttpRequestRelease(req);
	} else if (conn != NULL) {
		_sceHttpConnectionUnlock(conn, NULL);
		_sceHttpConnectionRelease(conn);
	} else if (tmpl != NULL) {
		_sceHttpTemplateRelease(tmpl);
	}
	if (removed == 1)
		ret = 0;
	return ret;
}

/* the settings of the template, connection or request id, with the object it belongs to */
__attribute__((noinline))
int _sceHttpGetTemplateById(int id, HttpTemplate **tmpl, HttpConnection **conn,
			    HttpRequest **req)
{
	int ret;

	ret = _sceHttpGetObjectType(id);
	if (ret < 0)
		return ret;
	if (ret <= HTTP_OBJECT_TEMPLATE)
		return _sceHttpGetTemplate(tmpl, id);
	if (ret <= HTTP_OBJECT_CONNECTION) {
		ret = _sceHttpGetConnection(conn, id);
		if (ret >= 0)
			*tmpl = &(*conn)->tmpl;
	} else if (ret <= HTTP_OBJECT_REQUEST) {
		ret = _sceHttpGetRequest(req, id);
		if (ret >= 0) {
			*tmpl = NULL;
			ret = sceKernelLockLwMutex(&(*req)->lock, 1, NULL);
			if (ret >= 0)
				*tmpl = &(*req)->tmpl;
		}
	} else {
		return SCE_HTTP_ERROR_INVALID_ID;
	}
	return ret;
}

/* releases what _sceHttpGetTemplateById took */
static inline void _sceHttpReleaseTemplateById(HttpTemplate *tmpl, HttpConnection *conn,
					       HttpRequest *req)
{
	if (req != NULL) {
		if (tmpl != NULL)
			sceKernelUnlockLwMutex(&req->lock, 1);
		_sceHttpRequestRelease(req);
	} else if (conn != NULL) {
		_sceHttpConnectionRelease(conn);
	} else if (tmpl != NULL) {
		_sceHttpTemplateRelease(tmpl);
	}
}

/* adds name: value to the template, connection or request id (OVERWRITE: replacing it) */
__attribute__((noinline))
int _sceHttpAddRequestHeader(int id, const char *name, const char *value, int mode)
{
	HttpTemplate *tmpl = NULL;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	HttpHeader *header = NULL;
	HttpAllocator allocator;
	int ret;

	if (mode == SCE_HTTP_HEADER_OVERWRITE)
		_sceHttpRemoveRequestHeader(id, name);
	ret = _sceHttpGetTemplateById(id, &tmpl, &conn, &req);
	if (ret >= 0) {
		allocator = tmpl->allocator;
		ret = _sceHttpHeaderCreate(&allocator, &header, name, value);
		if (ret >= 0) {
			ret = _sceHttpLock(0);
			if (ret >= 0) {
				header->next = tmpl->headers;
				tmpl->headers = header;
				_sceHttpUnlock(0);
				_sceHttpReleaseTemplateById(tmpl, conn, req);
				return 0;
			}
		}
	}
	_sceHttpReleaseTemplateById(tmpl, conn, req);
	_sceHttpHeaderFree(header);
	return ret;
}

/* adds a "name: value" line to the template, connection or request id */
__attribute__((noinline))
int _sceHttpAddRequestHeaderRaw(int id, const char *header, int mode)
{
	HttpTemplate *tmpl = NULL;
	HttpConnection *conn = NULL;
	HttpRequest *req = NULL;
	void *(*mallocFunc)(SceSize size);
	void (*freeFunc)(void *ptr);
	const char *name = NULL;
	const char *colon = NULL;
	const char *value = NULL;
	char *nameBuf;
	char *valueBuf = NULL;
	SceSize len;
	SceSize pos;
	SceSize n;
	int ret;

	if (mode == 0) {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
		goto out;
	}
	ret = _sceHttpGetObjectType(id);
	if (ret < 0)
		goto out;
	if (ret <= HTTP_OBJECT_TEMPLATE) {
		ret = _sceHttpGetTemplate(&tmpl, id);
		if (ret < 0)
			goto out;
		mallocFunc = tmpl->allocator.malloc;
		freeFunc = tmpl->allocator.free;
	} else if (ret <= HTTP_OBJECT_CONNECTION) {
		ret = _sceHttpGetConnection(&conn, id);
		if (ret < 0)
			goto out;
		mallocFunc = conn->tmpl.allocator.malloc;
		freeFunc = conn->tmpl.allocator.free;
		tmpl = &conn->tmpl;
	} else if (ret > HTTP_OBJECT_REQUEST) {
		ret = SCE_HTTP_ERROR_INVALID_ID;
		goto out;
	} else {
		ret = _sceHttpGetRequest(&req, id);
		if (ret < 0)
			goto out;
		ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
		if (ret < 0)
			goto out;
		mallocFunc = req->tmpl.allocator.malloc;
		freeFunc = req->tmpl.allocator.free;
		tmpl = &req->tmpl;
	}

	/* the name after leading spaces, up to the ':'; the value after the spaces after it */
	len = sceClibStrnlen(header, 0xffff);
	pos = 0;
	if (len != 0) {
		while (isspace(header[pos])) {
			if (++pos >= len)
				goto name_done;
		}
		name = &header[pos];
	}
name_done:
	if (pos < len) {
		while (header[pos] != ':') {
			if (++pos >= len)
				goto colon_done;
		}
		colon = &header[pos++];
	}
colon_done:
	if (pos < len) {
		while (isspace(header[pos])) {
			if (++pos >= len)
				goto value_done;
		}
		value = &header[pos];
	}
value_done:
	n = colon - name;
	nameBuf = _sceHttpMalloc(mallocFunc, n + 1);
	if (nameBuf != NULL) {
		sceClibMemcpy_safe(nameBuf, name, n);
		nameBuf[n] = '\0';
		n = sceClibStrnlen(value, 0xffff);
		valueBuf = _sceHttpMalloc(mallocFunc, n + 1);
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		if (valueBuf != NULL) {
			sceClibMemcpy_safe(valueBuf, value, n);
			valueBuf[n] = '\0';
			ret = _sceHttpAddRequestHeader(id, nameBuf, valueBuf, mode);
		}
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	if (nameBuf != NULL)
		_sceHttpFree(freeFunc, nameBuf);
	if (valueBuf != NULL)
		_sceHttpFree(freeFunc, valueBuf);

out:
	_sceHttpReleaseTemplateById(tmpl, conn, req);
	return ret;
}

/* the Host header's value for hostname and port */
__attribute__((noinline))
int _sceHttpMakeHostHeader(const HttpAllocator *allocator, char **header, SceBool https,
			   const char *hostname, SceUShort16 port)
{
	SceHttpUriElement uri;
	char *host;
	char *buf = NULL;
	SceSize require;
	int ret;

	require = sceClibStrnlen(hostname, 0xff) + 1;
	host = _sceHttpMalloc(allocator->malloc, require);
	if (host != NULL) {
		sceClibMemcpy_safe(host, hostname, require - 1);
		host[require - 1] = '\0';
		sceClibMemset(&uri, 0, sizeof(uri));
		uri.port = port;
		uri.hostname = host;
		uri.opaque = 1;
		uri.scheme = https ? "https" : "http";
		ret = sceHttpUriBuild(NULL, &require, 0, &uri,
				      SCE_HTTP_URI_BUILD_WITH_HOSTNAME | SCE_HTTP_URI_BUILD_WITH_PORT);
		if (ret >= 0) {
			buf = _sceHttpMalloc(allocator->malloc, require);
			if (buf != NULL) {
				ret = sceHttpUriBuild(buf, NULL, require, &uri,
						      SCE_HTTP_URI_BUILD_WITH_HOSTNAME |
						      SCE_HTTP_URI_BUILD_WITH_PORT);
				if (ret >= 0) {
					_sceHttpFree(allocator->free, host);
					*header = buf;
					return 0;
				}
			}
		}
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	if (host != NULL)
		_sceHttpFree(allocator->free, host);
	if (buf != NULL)
		_sceHttpFree(allocator->free, buf);
	return ret;
}

/* req's [Proxy-]Authorization: Basic header */
__attribute__((noinline))
int _sceHttpRequestSetBasicAuth(HttpRequest *req, const char *username, const char *password,
				SceBool proxy)
{
	SceSize size = 0;
	char *buf;
	int ret;

	ret = _sceHttpAuthBasic(&req->tmpl.allocator, NULL, &size, 0, username, password);
	if (ret < 0)
		return ret;
	buf = _sceHttpMalloc(req->tmpl.allocator.malloc, size);
	if (buf != NULL) {
		ret = _sceHttpAuthBasic(&req->tmpl.allocator, buf, NULL, size, username, password);
		if (ret >= 0) {
			if (proxy == 1)
				ret = _sceHttpAddRequestHeader(req->object.id, "Proxy-Authorization", buf,
							       SCE_HTTP_HEADER_OVERWRITE);
			else
				ret = _sceHttpAddRequestHeader(req->object.id, "Authorization", buf,
							       SCE_HTTP_HEADER_OVERWRITE);
		}
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	if (buf != NULL)
		_sceHttpFree(req->tmpl.allocator.free, buf);
	return ret;
}

/*
 * The path, query and fragment of req's request line (for CONNECT the host and port), in a
 * buffer from its allocator. As in Sony's, the element is not cleared first.
 */
__attribute__((noinline))
int _sceHttpRequestGetPath(HttpRequest *req, char **path)
{
	SceHttpUriElement uri;
	HttpConnection *conn;
	SceSize require;
	char *buf;
	int option;
	int ret;

	conn = _sceHttpRequestGetConnection(req);
	uri.hostname = conn->hostname;
	uri.port = conn->port;
	if (req->method != SCE_HTTP_METHOD_CONNECT) {
		uri.path = req->path;
		uri.query = req->query;
		uri.fragment = req->fragment;
	}
	if (uri.path == NULL || sceClibStrnlen(uri.path, 2) == 0 ||
	    (sceClibStrnlen(uri.path, 2) == 1 && *uri.path == ' '))
		uri.path = "/";
	option = SCE_HTTP_URI_BUILD_WITH_PATH | SCE_HTTP_URI_BUILD_WITH_QUERY |
		 SCE_HTTP_URI_BUILD_WITH_FRAGMENT;
	if (req->method == SCE_HTTP_METHOD_CONNECT)
		option = SCE_HTTP_URI_BUILD_WITH_HOSTNAME | SCE_HTTP_URI_BUILD_WITH_PORT;
	uri.opaque = 1;
	ret = sceHttpUriBuild(NULL, &require, 0, &uri, option);
	if (ret < 0)
		return ret;
	buf = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
	if (buf != NULL) {
		ret = sceHttpUriBuild(buf, NULL, require, &uri, option);
		if (ret >= 0) {
			*path = buf;
			return ret;
		}
	} else {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	}
	if (buf != NULL) {
		/* Sony's order: the byte is cleared after the buffer is freed */
		_sceHttpFree(req->tmpl.allocator.free, buf);
		*buf = '\0';
	}
	return ret;
}

/* req's [Proxy-]Authorization: Digest header */
__attribute__((noinline))
int _sceHttpRequestSetDigestAuth(HttpRequest *req, const char *username, const char *password,
				 const void *entity, SceSize entitySize,
				 const HttpAuthDigest *digest, SceBool proxy)
{
	SceSize size = 0;
	char *path = NULL;
	char *buf;
	int ret;

	ret = _sceHttpRequestGetPath(req, &path);
	if (ret >= 0) {
		ret = _sceHttpAuthDigest(NULL, &size, 0, username, password, path, req->method,
					 entity, entitySize, digest);
		if (ret >= 0) {
			buf = _sceHttpMalloc(req->tmpl.allocator.malloc, size);
			if (buf != NULL) {
				ret = _sceHttpAuthDigest(buf, NULL, size, username, password, path,
							 req->method, entity, entitySize, digest);
				if (ret >= 0) {
					if (proxy == 1)
						ret = _sceHttpAddRequestHeader(req->object.id,
									       "Proxy-Authorization",
									       buf,
									       SCE_HTTP_HEADER_OVERWRITE);
					else
						ret = _sceHttpAddRequestHeader(req->object.id,
									       "Authorization", buf,
									       SCE_HTTP_HEADER_OVERWRITE);
				}
			} else {
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			}
			if (buf != NULL)
				_sceHttpFree(req->tmpl.allocator.free, buf);
		}
	}
	if (path != NULL)
		_sceHttpFree(req->tmpl.allocator.free, path);
	return ret;
}
