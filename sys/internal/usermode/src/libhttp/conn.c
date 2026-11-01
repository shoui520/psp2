/*
 * SceLibHttp connections: creating and destroying them, the semaphore a request holds a
 * connection by, and connecting its socket.
 */

#include "http_internal.h"

/*
 * A connection to hostname with tmpl's settings. internal: one a request makes for itself
 * (it goes on parent's list instead of getting an id).
 */
__attribute__((noinline))
int _sceHttpConnectionCreate(HttpTemplate *tmpl, const char *hostname, const char *scheme,
			     const char *username, const char *password, SceUShort16 port,
			     SceBool keepAlive, SceBool internal, HttpRequest *parent)
{
	SceKernelThreadInfo info;
	char name[32];
	HttpConnection *conn = NULL;
	HttpHeader *header = NULL;
	char *host = NULL, *user = NULL, *pass = NULL, *hostHeader = NULL;
	SceSize len;
	SceBool https;
	SceUID sema;
	int ret;

	if (hostname == NULL || scheme == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
		goto fail;
	}
	ret = _sceHttpGetScheme(scheme, &https);
	if (ret < 0)
		goto fail;
	conn = _sceHttpMalloc(tmpl->allocator.malloc, sizeof(*conn));
	if (conn == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	sceClibMemset(conn, 0, sizeof(*conn));
	len = sceClibStrnlen(hostname, 0xff);
	host = _sceHttpMalloc(tmpl->allocator.malloc, len + 1);
	if (host == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto fail;
	}
	sceClibStrncpy(host, hostname, len);
	host[len] = '\0';
	if (username != NULL && password != NULL) {
		len = sceClibStrnlen(username, SCE_HTTP_USERNAME_MAX_SIZE);
		user = _sceHttpMalloc(tmpl->allocator.malloc, len + 1);
		if (user == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto fail;
		}
		sceClibStrncpy(user, username, len);
		user[len] = '\0';
		/* Sony's sizes the password by the username */
		len = sceClibStrnlen(username, SCE_HTTP_USERNAME_MAX_SIZE);
		pass = _sceHttpMalloc(tmpl->allocator.malloc, len + 1);
		if (pass == NULL) {
			ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			goto fail;
		}
		sceClibStrncpy(pass, password, len);
		pass[len] = '\0';
	}
	ret = _sceHttpTemplateCopy(tmpl, &conn->tmpl);
	if (ret < 0)
		goto fail;
	if (sceAppMgrIsGameProgram() != 0 || sceAppMgrIsNonGameProgram() != 0) {
		ret = sceKernelCreateSema("SceLibHttp", 0, 1, 1, NULL);
	} else {
		sceClibMemset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		ret = sceKernelGetThreadInfo(0, &info);
		if (ret < 0)
			goto fail;
		sceClibSnprintf(name, 0x1f, "SceLibHttp_%s", info.name);
		ret = sceKernelCreateSema(name, 0, 1, 1, NULL);
	}
	if (ret < 0)
		goto fail;
	sema = ret;
	sceClibMemset(&conn->recvBuffer, 0, 0x14);
	conn->recvBuffer = _sceHttpMalloc(tmpl->allocator.malloc, tmpl->recvBufferSize);
	if (conn->recvBuffer == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
		goto fail_sema;
	}
	conn->recvBufferSize = tmpl->recvBufferSize;
	conn->hostname = host;
	conn->https = https;
	conn->port = port;
	conn->state = 0;
	conn->tmpl.keepAlive = keepAlive;
	if (conn->tmpl.username != NULL)
		_sceHttpFree(tmpl->allocator.free, conn->tmpl.username);
	conn->tmpl.username = user;
	if (conn->tmpl.password != NULL)
		_sceHttpFree(tmpl->allocator.free, conn->tmpl.password);
	conn->tmpl.password = pass;
	conn->sema = sema;
	conn->resolverId = -1;
	conn->sock = -1;
	conn->aborted = 0;
	host = NULL;
	user = NULL;
	pass = NULL;
	ret = _sceHttpMakeHostHeader(&conn->tmpl.allocator, &hostHeader, conn->https, conn->hostname,
				     conn->port);
	if (ret < 0)
		goto fail_sema;
	ret = _sceHttpHeaderCreate(&conn->tmpl.allocator, &header, "Host", hostHeader);
	if (ret < 0)
		goto fail_sema;
	_sceHttpHeaderRemove(&conn->tmpl.headers, "Host");
	header->next = conn->tmpl.headers;
	conn->tmpl.headers = header;
	_sceHttpFree(conn->tmpl.allocator.free, hostHeader);
	hostHeader = NULL;
	if (!internal) {
		ret = _sceHttpObjectRegister(&conn->object, HTTP_OBJECT_CONNECTION);
		if (ret >= 0)
			return ret;
		goto fail_sema;
	}
	ret = _sceHttpLock(0);
	if (ret < 0)
		goto fail_sema;
	if (parent->childConnections != NULL)
		conn->object.next = &parent->childConnections->object;
	parent->childConnections = conn;
	conn->object.refCount++;
	_sceHttpUnlock(0);
	return ret;

fail_sema:
	sceKernelDeleteSema(sema);
fail:
	if (hostHeader != NULL)
		_sceHttpFree(tmpl->allocator.free, hostHeader);
	if (host != NULL)
		_sceHttpFree(tmpl->allocator.free, host);
	if (user != NULL)
		_sceHttpFree(tmpl->allocator.free, user);
	if (pass != NULL)
		_sceHttpFree(tmpl->allocator.free, pass);
	if (conn != NULL) {
		if (conn->recvBuffer != NULL)
			_sceHttpFree(tmpl->allocator.free, conn->recvBuffer);
		_sceHttpFree(tmpl->allocator.free, conn);
	}
	if (header != NULL)
		_sceHttpHeaderFree(header);
	return ret;
}

/* internal: tmpl is given (held by the caller) instead of tmplId */
__attribute__((noinline))
int _sceHttpCreateConnectionWithURL(HttpTemplate *tmpl, int tmplId, const char *url, SceBool keepAlive,
				    SceBool internal, HttpRequest *parent)
{
	SceHttpUriElement uri;
	SceSize require;
	char *buf = NULL;
	int ret;

	if (!internal) {
		ret = _sceHttpGetTemplate(&tmpl, tmplId);
		if (ret < 0)
			goto out;
	}
	sceClibMemset(&uri, 0, sizeof(uri));
	ret = sceHttpUriParse(NULL, url, NULL, &require, 0);
	if (ret < 0)
		goto out;
	buf = _sceHttpMalloc(tmpl->allocator.malloc, require);
	if (buf == NULL) {
		ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
	} else {
		ret = sceHttpUriParse(&uri, url, buf, NULL, require);
		if (ret >= 0)
			ret = _sceHttpConnectionCreate(tmpl, uri.hostname, uri.scheme, uri.username,
						       uri.password, uri.port, keepAlive, internal,
						       parent);
	}
	if (buf != NULL)
		_sceHttpFree(tmpl->allocator.free, buf);
out:
	if (!internal && tmpl != NULL)
		_sceHttpTemplateRelease(tmpl);
	return ret;
}

__attribute__((noinline))
int _sceHttpCreateConnection(int tmplId, const char *serverName, const char *scheme,
			     SceUShort16 port, SceBool keepAlive)
{
	HttpTemplate *tmpl = NULL;
	int ret;

	ret = _sceHttpGetTemplate(&tmpl, tmplId);
	if (ret >= 0)
		ret = _sceHttpConnectionCreate(tmpl, serverName, scheme, NULL, NULL, port, keepAlive,
					       0, NULL);
	if (tmpl != NULL)
		_sceHttpTemplateRelease(tmpl);
	return ret;
}

__attribute__((noinline))
int _sceHttpDeleteConnection(int connId)
{
	HttpConnection *conn;
	int ret;

	ret = _sceHttpTakeObject((HttpObject **)&conn, connId, HTTP_OBJECT_CONNECTION);
	if (ret >= 0)
		ret = _sceHttpConnectionRelease(conn);
	return ret;
}

__attribute__((noinline))
void _sceHttpConnectionDestroy(HttpConnection *conn)
{
	void (*freeObject)(void *ptr) = conn->tmpl.freeObject;

	_sceHttpTemplateClear(&conn->tmpl);
	_sceHttpConnectionClose(conn);
	_sceHttpConnectionShutdown(conn);
	if (conn->recvBuffer != NULL) {
		_sceHttpFree(freeObject, conn->recvBuffer);
		conn->recvBuffer = NULL;
	}
	if (conn->hostname != NULL) {
		_sceHttpFree(freeObject, conn->hostname);
		conn->hostname = NULL;
	}
	if (conn->sema >= 0) {
		sceKernelDeleteSema(conn->sema);
		conn->sema = -1;
	}
}

/*
 * Holds conn for owner (a request's id): at once when owner already holds it, else by its
 * semaphore, without waiting (nonblock) or for timeout.
 */
__attribute__((noinline))
int _sceHttpConnectionLock(HttpConnection *conn, const int *owner, SceBool nonblock,
			   SceUInt *timeout)
{
	SceBool held = 0;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return 0;
	if (owner != NULL && conn->owner == *owner) {
		ret = 0;
		held = 1;
	}
	_sceHttpUnlock(0);
	if (held)
		return ret;
	if (nonblock == 1) {
		ret = sceKernelPollSema(conn->sema, 1);
		if (ret < 0) {
			if (ret == SCE_KERNEL_ERROR_SEMA_ZERO)
				return SCE_HTTP_ERROR_BUSY;
			return ret;
		}
	} else {
		ret = sceKernelWaitSema(conn->sema, 1, timeout);
		if (ret < 0) {
			if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
				return SCE_HTTP_ERROR_BUSY;
			return ret;
		}
	}
	if (owner != NULL)
		conn->owner = *owner;
	return ret;
}

__attribute__((noinline))
SceBool _sceHttpConnectionIsOwner(HttpConnection *conn, const int *owner)
{
	SceBool ret;

	if (conn == NULL || owner == NULL)
		return 0;
	if (_sceHttpLock(0) < 0)
		return 0;
	ret = conn->owner == *owner;
	_sceHttpUnlock(0);
	return ret;
}

/* between connecting and the end of a response */
__attribute__((noinline))
SceBool _sceHttpConnectionIsActive(const HttpConnection *conn)
{
	if (conn == NULL)
		return 0;
	if (conn->state > 2)
		return conn->state < 8;
	return 0;
}

__attribute__((noinline))
int _sceHttpConnectionUnlock(HttpConnection *conn, const int *owner)
{
	SceBool other = 0;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	if (owner != NULL) {
		if (conn->owner == *owner)
			conn->owner = 0;
		else
			other = 1;
	}
	_sceHttpUnlock(0);
	if (other)
		return ret;
	return sceKernelSignalSema(conn->sema, 1);
}

/* the next step of connecting req's connection: 0 when it is connected */
__attribute__((noinline))
int _sceHttpConnectionConnect(HttpRequest *req)
{
	HttpConnection *conn = _sceHttpRequestGetConnection(req);
	int optval;
	unsigned int optlen;
	int ret;

	if (conn->state <= 2) {
		if (conn->state <= 1) {
			ret = SCE_HTTP_ERROR_BUSY;
			goto fail;
		}
		if (req->tmpl.nonblock != 1) {
			ret = _sceHttpSocketSetSendTimeout(conn->sock, &conn->addr,
							   req->tmpl.connectTimeout,
							   &conn->savedSendTimeout);
			if (ret < 0)
				goto fail;
		}
		conn->state = 3;
	} else if (conn->state > 4) {
		if (conn->state > 5) {
			ret = SCE_HTTP_ERROR_BUSY;
			goto fail;
		}
		goto connected;
	}
	ret = _sceHttpSocketConnect(conn->sock, &conn->addr, req->tmpl.connectTimeout,
				    conn->savedSendTimeout);
	if (ret < 0) {
		if (ret == SCE_HTTP_ERROR_EAGAIN && req->tmpl.nonblock == 1 && conn->state == 3) {
			int r = _sceHttpRequestEpollAdd(req, HTTP_EPOLL_SOCKET, SCE_NET_EPOLLOUT);

			if (r >= 0)
				conn->state = 4;
			else
				ret = r;
		}
		goto fail;
	}
	conn->state = 5;
connected:
	if (req->tmpl.nonblock == 1) {
		optval = 0;
		optlen = sizeof(optval);
		ret = sceNetGetsockopt(conn->sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_ERROR_EX, &optval,
				       &optlen);
		if (ret < 0)
			goto fail;
		ret = optval;
		if (optval != 0)
			goto fail;
	}
	return _sceHttpSocketRestoreSendTimeout(conn->sock, conn->savedSendTimeout);
fail:
	if (ret != SCE_HTTP_ERROR_EAGAIN && conn->sock >= 0 && conn->state != 3) {
		sceNetSocketClose(conn->sock);
		conn->state = 0;
		conn->sock = -1;
	}
	return ret;
}
