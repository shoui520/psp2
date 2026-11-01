/*
 * SceLibHttp requests: creating and deleting them, aborting them, sending on their
 * connection and ending a response.
 */

#include "http_internal.h"

/* the connection a request talks on: its child connection (a proxy's tunnel) if it has one */
HttpConnection *_sceHttpRequestGetConnection(HttpRequest *req)
{
	if (req->childConnections != NULL)
		return req->childConnections;
	return req->connection;
}

/* the request's URL, from its connection's scheme, host and port and its own path */
char *_sceHttpRequestGetUrl(HttpRequest *req)
{
	SceHttpUriElement uri;
	HttpConnection *conn;
	SceSize require;
	char *url;

	sceClibMemset(&uri, 0, sizeof(uri));
	conn = _sceHttpRequestGetConnection(req);
	uri.scheme = conn->https == 0 ? "http" : "https";
	uri.hostname = conn->hostname;
	uri.port = conn->port;
	uri.path = req->path;
	uri.query = req->query;
	uri.fragment = req->fragment;
	if (sceHttpUriBuild(NULL, &require, 0, &uri, SCE_HTTP_URI_BUILD_WITH_ALL) >= 0) {
		url = _sceHttpMalloc(req->tmpl.allocator.malloc, require);
		if (url != NULL) {
			if (sceHttpUriBuild(url, NULL, require, &uri, SCE_HTTP_URI_BUILD_WITH_ALL) >= 0)
				return url;
			_sceHttpFree(req->tmpl.allocator.free, url);
		}
	}
	return NULL;
}

void _sceHttpRequestResetResponseHeader(HttpRequest *req)
{
	req->responseHeaderLen = 0;
	req->responseHeaderState = 0;
	req->responseHeaderPos = 0;
}

/*
 * A request on conn, with conn's settings: its path swept, its own copies of query and
 * fragment, and username and password as its credentials when both are given.
 */
int _sceHttpRequestCreate(HttpConnection *conn, int method, const char *path, const char *query,
			  const char *fragment, const char *username, const char *password,
			  SceULong64 contentLength)
{
	char lockName[32];
	SceKernelThreadInfo info;
	char *user = NULL;
	char *pass = NULL;
	void (*freeFunc)(void *ptr) = conn->tmpl.allocator.free;
	char *queryCopy = NULL;
	char *fragmentCopy = NULL;
	char *pathCopy;
	SceUInt8 locked = 0;
	HttpRequest *req;
	SceSize len;
	int ret;

	if (path == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (method != SCE_HTTP_METHOD_GET && method != SCE_HTTP_METHOD_HEAD &&
	    method != SCE_HTTP_METHOD_DELETE && method != SCE_HTTP_METHOD_PUT &&
	    method != SCE_HTTP_METHOD_TRACE && method != HTTP_METHOD_EXTRA &&
	    method != SCE_HTTP_METHOD_POST)
		return SCE_HTTP_ERROR_UNKNOWN_METHOD;
	if (_sceHttpStrHasLineBreak(path) == 1 ||
	    (query != NULL && _sceHttpStrHasLineBreak(query) == 1) ||
	    (fragment != NULL && _sceHttpStrHasLineBreak(fragment) == 1) ||
	    (username != NULL && _sceHttpStrHasLineBreak(username) == 1) ||
	    (password != NULL && _sceHttpStrHasLineBreak(password) == 1))
		return SCE_HTTP_ERROR_INVALID_VALUE;

	len = sceClibStrnlen(path, 0x3fff) + 1;
	pathCopy = _sceHttpMalloc(conn->tmpl.allocator.malloc, len);
	if (pathCopy == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	ret = sceHttpUriSweepPath(pathCopy, path, len);
	if (ret < 0)
		goto out;
	if (query != NULL) {
		len = sceClibStrnlen(query, 0x3fff);
		queryCopy = _sceHttpMalloc(conn->tmpl.allocator.malloc, len + 1);
		if (queryCopy == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibStrncpy(queryCopy, query, len);
		queryCopy[len] = '\0';
	}
	if (fragment != NULL) {
		len = sceClibStrnlen(fragment, 0x3fff);
		fragmentCopy = _sceHttpMalloc(conn->tmpl.allocator.malloc, len + 1);
		if (fragmentCopy == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibStrncpy(fragmentCopy, fragment, len);
		fragmentCopy[len] = '\0';
	}
	if (username != NULL && password != NULL) {
		len = sceClibStrnlen(username, SCE_HTTP_USERNAME_MAX_SIZE) + 1;
		user = _sceHttpMalloc(conn->tmpl.allocator.malloc, len);
		if (user == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibStrlcpy(user, username, len);
		len = sceClibStrnlen(password, SCE_HTTP_PASSWORD_MAX_SIZE) + 1;
		pass = _sceHttpMalloc(conn->tmpl.allocator.malloc, len);
		if (pass == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		sceClibStrlcpy(pass, password, len);
	}

	req = _sceHttpMalloc(conn->tmpl.allocator.malloc, sizeof(*req));
	if (req == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	sceClibMemset(req, 0, sizeof(*req));
	if (sceAppMgrIsGameProgram() != 0 || sceAppMgrIsNonGameProgram() != 0) {
		ret = sceKernelCreateLwMutex(&req->lock, "SceLibHttp",
					     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	} else {
		sceClibMemset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		ret = sceKernelGetThreadInfo(0, &info);
		if (ret < 0)
			goto fail;
		sceClibSnprintf(lockName, sizeof(lockName) - 1, "SceLibHttp_%s", info.name);
		ret = sceKernelCreateLwMutex(&req->lock, lockName,
					     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	}
	if (ret < 0)
		goto fail;
	locked = 1;
	ret = _sceHttpTemplateCopy(&conn->tmpl, &req->tmpl);
	if (ret < 0)
		goto fail;
	req->state = 0;
	req->method = method;
	req->path = pathCopy;
	req->query = queryCopy;
	req->fragment = fragmentCopy;
	if ((username != NULL && sceClibStrnlen(username, SCE_HTTP_USERNAME_MAX_SIZE) != 0) ||
	    (password != NULL && sceClibStrnlen(password, SCE_HTTP_PASSWORD_MAX_SIZE) != 0)) {
		if (req->tmpl.username != NULL)
			_sceHttpFree(req->tmpl.freeObject, req->tmpl.username);
		if (req->tmpl.password != NULL)
			_sceHttpFree(req->tmpl.freeObject, req->tmpl.password);
		req->tmpl.username = user;
		req->tmpl.password = pass;
		user = NULL;
		pass = NULL;
	} else {
		if (user != NULL) {
			_sceHttpFree(freeFunc, user);
			user = NULL;
		}
		if (pass != NULL) {
			_sceHttpFree(freeFunc, pass);
			pass = NULL;
		}
	}
	req->connection = conn;
	req->contentLength = contentLength;
	req->responseHeader = _sceHttpMalloc(req->tmpl.allocator.malloc, 1500);
	if (req->responseHeader == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	req->responseHeaderSize = 1500;
	sceClibMemset(req->responseHeader, 0, 1500);
	req->extra = NULL;
	req->icmSock = -1;
	req->authCache = NULL;
	req->proxyAuthCache = NULL;
	req->responseHeaderState = 0;
	req->responseHeaderPos = 0;
	req->url = _sceHttpRequestGetUrl(req);
	if (req->url == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	ret = _sceHttpObjectRegister(&req->object, HTTP_OBJECT_REQUEST);
	if (ret >= 0)
		return ret;

fail:
	if (req != NULL) {
		if (req->responseHeader != NULL)
			_sceHttpFree(freeFunc, req->responseHeader);
		_sceHttpHeaderFree(req->tmpl.headers);
		if (locked)
			sceKernelDeleteLwMutex(&req->lock);
		_sceHttpFree(freeFunc, req);
	}
out:
	if (pathCopy != NULL)
		_sceHttpFree(freeFunc, pathCopy);
	if (queryCopy != NULL)
		_sceHttpFree(freeFunc, queryCopy);
	if (fragmentCopy != NULL)
		_sceHttpFree(freeFunc, fragmentCopy);
	if (user != NULL)
		_sceHttpFree(freeFunc, user);
	if (pass != NULL)
		_sceHttpFree(freeFunc, pass);
	return ret;
}

int _sceHttpCreateRequestWithURL(int connId, int method, const char *url,
				 SceULong64 contentLength)
{
	SceHttpUriElement uri;
	HttpConnection *conn = NULL;
	void (*freeFunc)(void *ptr);
	SceSize require;
	void *pool;
	int ret;

	sceClibMemset(&uri, 0, sizeof(uri));
	ret = _sceHttpGetConnection(&conn, connId);
	if (ret >= 0) {
		freeFunc = conn->tmpl.allocator.free;
		ret = sceHttpUriParse(NULL, url, NULL, &require, 0);
		if (ret >= 0) {
			pool = _sceHttpMalloc(conn->tmpl.allocator.malloc, require);
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			if (pool != NULL) {
				sceHttpUriParse(&uri, url, pool, NULL, require);
				ret = _sceHttpRequestCreate(conn, method, uri.path, uri.query,
							    uri.fragment, uri.username, uri.password,
							    contentLength);
				_sceHttpFree(freeFunc, pool);
			}
		}
	}
	if (ret < 0 && conn != NULL)
		_sceHttpConnectionRelease(conn);
	return ret;
}

int _sceHttpCreateRequest(int connId, int method, const char *path, SceULong64 contentLength)
{
	HttpConnection *conn = NULL;
	int ret;

	ret = _sceHttpGetConnection(&conn, connId);
	if (ret >= 0)
		ret = _sceHttpRequestCreate(conn, method, path, NULL, NULL, NULL, NULL,
					    contentLength);
	if (ret < 0 && conn != NULL)
		_sceHttpConnectionRelease(conn);
	return ret;
}

/* takes a non-blocking request's socket, resolver and ICM socket off its epoll handle */
__attribute__((noinline))
int _sceHttpRequestAbortNonblock(HttpRequest *req)
{
	HttpConnection *conn;
	int ret = 0;

	if (req->tmpl.nonblock == 1) {
		conn = req->connection;
		if (conn->sock != -1 && _sceHttpConnectionIsOwner(conn, &req->object.id) &&
		    _sceHttpConnectionIsActive(req->connection))
			ret = _sceHttpRequestEpollRemove(req, HTTP_EPOLL_SOCKET);
		if (req->connection->resolverId != -1)
			ret = _sceHttpRequestEpollRemove(req, HTTP_EPOLL_RESOLVER);
		if (req->icmSock != -1)
			return _sceHttpRequestEpollRemove(req, HTTP_EPOLL_ICM);
	}
	return ret;
}

int _sceHttpDeleteRequest(int reqId)
{
	HttpConnection *conn, *child;
	HttpRequest *req;
	SceBool freed;
	int ret, err;

	ret = _sceHttpTakeObject((HttpObject **)&req, reqId, HTTP_OBJECT_REQUEST);
	if (ret < 0)
		return ret;
	ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
	if (ret < 0)
		return ret;
	ret = _sceHttpRequestAbortNonblock(req);
	conn = req->connection;
	if (ret < 0)
		ret = 0;
	if ((req->state > 0 && req->state < 18) || req->state == 19) {
		_sceHttpRequestIcmClose(req);
		ret = _sceHttpConnectionLock(conn, &req->object.id, 0, &req->tmpl.connectTimeout);
		if (ret < 0)
			goto release;
		_sceHttpRequestResolverAbort(req);
		_sceHttpRequestResolverDestroy(req);
		if (conn != NULL)
			_sceHttpConnectionShutdown(conn);
		_sceHttpConnectionUnlock(conn, &req->object.id);
		for (child = req->childConnections; child != NULL;
		     child = (HttpConnection *)child->object.next) {
			ret = _sceHttpConnectionLock(child, &req->object.id, 0,
						     &req->tmpl.connectTimeout);
			if (ret < 0)
				goto release;
			_sceHttpConnectionShutdown(child);
			_sceHttpConnectionUnlock(child, &req->object.id);
		}
		ret = 0;
	}
	err = _sceHttpRequestReleaseEx(req, &freed);
	if (err < 0)
		return err;
	if (freed != 1)
		sceKernelUnlockLwMutex(&req->lock, 1);
	if (ret == 0 && conn != NULL)
		_sceHttpConnectionRelease(conn);
	return ret;

release:
	err = _sceHttpRequestRelease(req);
	if (err < 0)
		ret = err;
	return ret;
}

/*
 * Aborts a request once: its ICM socket while it has not connected yet, else its resolver
 * or, when force is set or the request is still under way, its connection.
 */
int _sceHttpAbortRequest(int reqId, SceBool force)
{
	HttpRequest *req = NULL;
	HttpConnection *conn;
	SceBool aborted;
	SceUInt32 state;
	int owner;
	int ret;

	ret = _sceHttpGetRequest(&req, reqId);
	if (ret < 0)
		goto out;
	ret = _sceHttpLock(0);
	if (ret < 0)
		goto out;
	aborted = 1;
	if (req->aborted == 1) {
		ret = 0;
	} else {
		req->aborted = 1;
		aborted = 0;
	}
	_sceHttpUnlock(0);
	if (aborted)
		goto out;
	if (req->state <= 2 && req->icmSock != -1) {
		ret = _sceHttpRequestIcmAbort(req);
		if (ret < 0)
			goto out;
		goto done;
	}
	conn = _sceHttpRequestGetConnection(req);
	ret = _sceHttpLock(0);
	if (ret < 0)
		goto out;
	state = conn->state;
	owner = conn->owner;
	_sceHttpUnlock(0);
	if (owner != reqId)
		goto done;
	if (state == 1) {
		ret = _sceHttpRequestResolverAbort(req);
		if (ret < 0) {
			_sceHttpConnectionAbort(conn);
			goto out;
		}
		goto done;
	}
	if (force != 1 && (state == 0 || req->state == 18 || req->state == 19))
		goto done;
	ret = _sceHttpConnectionAbort(conn);
	if (ret < 0)
		goto out;
done:
	ret = 0;
out:
	if (req != NULL)
		_sceHttpRequestRelease(req);
	return ret;
}

/* sends on conn for req; a short blocking send of an aborted request is SCE_HTTP_ERROR_ABORTED */
int _sceHttpRequestSend(HttpRequest *req, HttpConnection *conn, const void *msg, SceSize len,
			int flags, SceBool nonblock, SceBool noDelay)
{
	int ret, err;

	ret = _sceHttpConnectionSend(conn, msg, len, flags, nonblock, noDelay);
	if (nonblock == 0 && ret > 0 && ret != len) {
		err = _sceHttpLock(0);
		if (err < 0)
			return err;
		if (req->aborted == 1)
			ret = SCE_HTTP_ERROR_ABORTED;
		err = _sceHttpUnlock(0);
		if (err < 0)
			return err;
	}
	return ret;
}

/*
 * Ends the response when its body is all read, or when close or end says so (state 18, or
 * 19 when failed), and lets go of the connection: closed unless it is kept alive.
 */
int _sceHttpRequestCompleteEx(HttpRequest *req, SceBool close, SceBool end, SceBool main,
			      SceBool failed)
{
	HttpConnection *conn, *first;
	int state = req->state;
	int ret = 0;

	if ((req->response.contentLengthKnown == 1 && req->gzipEncoded == 0 &&
	     req->receivedLength == req->response.contentLength) ||
	    (req->gzipEncoded == 1 && req->inflateEnd == 1) || close == 1 || end == 1) {
		req->response.chunked = 0;
		state = failed != 0 ? 19 : 18;
		req->state = state;
	}
	if (state != 18 && state != 19)
		return ret;
	if (main == 1)
		conn = req->connection;
	else
		conn = _sceHttpRequestGetConnection(req);
	if (conn->state == 7) {
		conn->state = 8;
		if (req->tmpl.nonblock == 1) {
			first = req->connection;
			if (conn == first) {
				first->tmpl.epoll = req->tmpl.epoll;
				ret = _sceHttpEpollModify(first, HTTP_EPOLL_CONNECTION,
							  SCE_NET_EPOLLIN);
				if (ret < 0)
					return ret;
			}
		}
	}
	if (conn->tmpl.keepAlive == 0 || req->response.close == 1 || close == 1) {
		if (req->tmpl.nonblock == 1) {
			ret = _sceHttpRequestEpollRemove(req, HTTP_EPOLL_SOCKET);
			if (ret < 0)
				return ret;
		}
		_sceHttpConnectionShutdown(conn);
	}
	return _sceHttpConnectionUnlock(conn, &req->object.id);
}

int _sceHttpRequestComplete(HttpRequest *req, SceBool close, SceBool end)
{
	return _sceHttpRequestCompleteEx(req, close, end, 0, 0);
}
