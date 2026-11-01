/*
 * SceLibHttp's sockets: a connection's receive buffer and the reads through it (buffered,
 * peeked, or straight into the caller's buffer), a chunked body's reader, the socket
 * options a template sets, and sending.
 */

#include <libnetctl.h>
#include <net/errno.h>
#include <net/resolver.h>
#include <net/sockinfo.h>

#include "http_internal.h"

/* closes a connection's socket and empties its buffer; once only */
int _sceHttpConnectionShutdown(HttpConnection *conn)
{
	SceBool done;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	if (conn->state == 0 || conn->state == 9) {
		done = 1;
	} else {
		conn->state = 9;
		done = 0;
	}
	ret = _sceHttpUnlock(0);
	if (done != 0)
		return ret;

	if (conn->https == 1)
		_sceHttpsClose(conn);
	if (conn->sock >= 0)
		sceNetSocketClose(conn->sock);
	conn->sock = -1;
	if (conn->recvBuffer != NULL)
		sceClibMemset(conn->recvBuffer, 0, conn->recvBufferSize);
	conn->recvPos = 0;
	conn->recvLen = 0;
	conn->recvEof = 0;
	conn->state = 0;
	conn->reserved18 = 0;
	return 0;
}

/* a read from the socket, through SSL once the connection is secured */
static inline int _sceHttpConnectionRecv(HttpConnection *conn, void *buf, SceSize len, int flags)
{
	if (conn->https != 1 || conn->state == 6)
		return sceNetRecv(conn->sock, buf, len, flags);
	return _sceHttpsRecv(conn, buf, len);
}

/* a failed read: would block (what was read so far, if any) or ends the request */
static inline int _sceHttpRecvError(HttpRequest *req, int ret, SceSize done)
{
	if (ret == SCE_NET_ERROR_EAGAIN || ret == SCE_HTTP_ERROR_EAGAIN) {
		if (req->tmpl.nonblock == 1) {
			ret = SCE_HTTP_ERROR_EAGAIN;
			if (done != 0)
				return done;
			return ret;
		}
		ret = SCE_HTTP_ERROR_TIMEOUT;
	}
	req->state = 19;
	return ret;
}

/*
 * Reads up to len bytes of the response. Buffered, a read fills the connection's buffer and
 * copies from it; peek leaves what it copied in the buffer; direct copies what the buffer
 * holds and then reads into buf itself.
 */
int _sceHttpRequestRecvBuffered(HttpRequest *req, char *buf, SceSize len, int flags, SceBool peek,
				SceBool direct)
{
	HttpConnection *conn;
	char *dst;
	SceSize done = 0;
	SceSize kept = 0;
	SceSize avail, n, total;
	SceSize pos, end;
	SceBool last = 0;
	int ret;

	dst = buf;
	if (req == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (req->state == 19)
		return req->lastError;
	conn = _sceHttpRequestGetConnection(req);
	if (conn == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	if (peek == 1 && conn->recvBufferSize < len)
		return SCE_HTTP_ERROR_OUT_OF_SIZE;

	if (direct == 1) {
		if (len == 0)
			return done;
		for (;;) {
			pos = conn->recvPos;
			avail = conn->recvLen - pos;
			if (avail != 0) {
				n = avail;
				total = avail + done;
				if (avail >= len - done) {
					last = 1;
					n = len - done;
					total = len;
				}
				sceClibMemcpy_safe(dst, (char *)conn->recvBuffer + pos, n);
				conn->recvPos += n;
			} else {
				if (conn->recvEof == 1)
					goto eof;
				ret = _sceHttpConnectionRecv(conn, dst, len - done, flags);
				if (ret < 0)
					return _sceHttpRecvError(req, ret, done);
				if (ret != 0) {
					n = ret;
					total = ret + done;
					last = 1;
				} else {
eof:
					last = 1;
					n = 0;
					total = done;
					conn->recvEof = 1;
				}
			}
			done = total;
			if (last == 1)
				return done;
			dst += n;
			if (total == len)
				return done;
		}
	}

	if (len == 0)
		goto out;
	for (;;) {
		if (conn->recvEof == 1)
			return done;
		pos = conn->recvPos;
		end = conn->recvLen;
		if (end == pos) {
			if (conn->state == 0 || conn->state == 9)
				return SCE_HTTP_ERROR_NOT_IN_COM;
			if (peek == 1 && done != 0) {
				kept = done;
				sceClibMemcpy_safe(conn->recvBuffer, buf, done);
				ret = _sceHttpConnectionRecv(conn, (char *)conn->recvBuffer + done,
							     conn->recvBufferSize - done, flags);
			} else {
				ret = _sceHttpConnectionRecv(conn, conn->recvBuffer,
							     conn->recvBufferSize, flags);
			}
			if (ret < 0)
				return _sceHttpRecvError(req, ret, done);
			if (ret != 0) {
				end = ret + kept;
				conn->recvPos = kept;
				pos = kept;
				conn->recvLen = end;
			} else {
				pos = conn->recvPos;
				conn->recvEof = 1;
				end = conn->recvLen;
			}
		}
		avail = end - pos;
		n = len - done;
		done += avail;
		if (n <= avail) {
			done = len;
			avail = n;
		}
		sceClibMemcpy_safe(dst, (char *)conn->recvBuffer + pos, avail);
		conn->recvPos += avail;
		dst += avail;
		if (done == len)
			break;
	}
out:
	if (peek == 1)
		conn->recvPos -= done;
	return done;

}

int _sceHttpRecvDirect(HttpRequest *req, void *buf, SceSize len, int flags)
{
	return _sceHttpRequestRecvBuffered(req, buf, len, flags, 0, 1);
}

int _sceHttpRecv(HttpRequest *req, void *buf, SceSize len, int flags)
{
	return _sceHttpRequestRecvBuffered(req, buf, len, flags, 0, 0);
}

/*
 * Reads a chunked body: each chunk's size line, its data, the CR LF after it, and after the
 * last chunk the trailer. With buf NULL, stops at a chunk's start and gives its size.
 */
int _sceHttpRequestReadChunked(HttpRequest *req, char *buf, SceSize len, int flags, SceBool *end,
			       SceSize *chunkSize)
{
	HttpConnection *conn;
	char c[4];
	SceSize done;
	SceSize n;
	SceUInt64 size;
	int ret;

	*end = 0;
	if (req->state < 15)
		return SCE_HTTP_ERROR_NOT_IN_COM;
	done = 0;

	while (len != 0 && *end != 1) {
		switch (req->chunkState) {
		case 0:
		case 1:
		case 2:
		case 3:
			if (req->chunkRemaining != 0)
				goto data;
			conn = _sceHttpRequestGetConnection(req);
			if (req->chunkState <= 1) {
				if (req->chunkState == 0) {
					req->chunkHexLen = 0;
					req->chunkState = 1;
				}
				for (;;) {
					if (req->chunkHexLen >= 11) {
						ret = SCE_HTTP_ERROR_BAD_RESPONSE;
						goto parsed;
					}
					ret = _sceHttpRequestRecvBuffered(req,
						&req->chunkHex[req->chunkHexLen], 1, flags, 0, 0);
					if (ret < 0)
						goto parsed;
					if (ret == 0)
						goto bad;
					if (req->chunkHex[req->chunkHexLen] == ' ') {
						req->chunkState = 2;
						goto extension;
					}
					if (req->chunkHex[req->chunkHexLen] == '\r') {
						req->chunkState = 3;
						goto lf;
					}
					req->chunkHexLen++;
				}
			} else if (req->chunkState <= 2) {
extension:
				ret = _sceHttpRequestSkipLine(req, flags);
				if (ret < 0)
					goto parsed;
			} else if (req->chunkState <= 3) {
lf:
				ret = _sceHttpRequestRecvBuffered(req, c, 1, flags, 0, 0);
				if (ret < 0)
					goto parsed;
				if (ret == 0 || c[0] != '\n') {
bad:
					ret = SCE_HTTP_ERROR_BAD_RESPONSE;
					_sceHttpConnectionShutdown(conn);
					goto parsed;
				}
			} else {
				ret = SCE_HTTP_ERROR_NOT_IN_COM;
				goto parsed;
			}
			req->chunkHex[req->chunkHexLen] = '\0';
			size = sceClibStrtoll(req->chunkHex, NULL, 16);
			if (size > 0x7fffffff) {
				ret = SCE_HTTP_ERROR_BAD_RESPONSE;
			} else {
				req->chunkState = 0;
				ret = 0;
				req->chunkRemaining = size;
			}
parsed:
			if (ret < 0)
				goto again;
			if (req->chunkRemaining == 0) {
				req->chunkState = 6;
				break;
			}
data:
			req->chunkState = 4;
			if (buf == NULL && chunkSize != NULL) {
				*chunkSize = req->chunkRemaining;
				return 0;
			}
			/* fall through */
		case 4:
			n = len;
			if (n >= req->chunkRemaining)
				n = req->chunkRemaining;
			ret = _sceHttpRequestRecvBuffered(req, buf + done, n, flags, 0, 1);
			if (ret < 0)
				goto again;
			if (ret == 0) {
				ret = SCE_HTTP_ERROR_BAD_RESPONSE;
				goto fail;
			}
			done += ret;
			len -= ret;
			req->chunkRemaining -= ret;
			if (req->chunkRemaining != 0)
				break;
			req->chunkState = 5;
			/* fall through */
		case 5:
			ret = _sceHttpRequestSkipLine(req, flags);
			if (ret < 0)
				goto again;
			req->chunkState = 0;
			break;
		case 6:
			/* the trailer: lines up to the empty one */
			if (_sceHttpRequestSkipLine(req, flags) == 2) {
				ret = 0;
			} else {
				for (;;) {
					ret = _sceHttpRequestSkipLine(req, flags);
					if (ret < 0)
						break;
					if (ret == 2) {
						ret = 0;
						break;
					}
				}
			}
			if (ret < 0)
				goto again;
			*end = 1;
			req->chunkState = 7;
			break;
		default:
			ret = SCE_HTTP_ERROR_NOT_IN_COM;
			goto fail;
		}
	}
	return done;

again:
	if (ret != SCE_HTTP_ERROR_EAGAIN)
		goto fail;
	if (done != 0)
		ret = done;
	return ret;

fail:
	req->response.chunked = 0;
	req->chunkRemaining = 0;
	return ret;
}

/* a template's timeouts, blocking mode, TCP_NODELAY and private flags on a socket */
int _sceHttpSocketSetOptions(int sock, const HttpTemplate *tmpl)
{
	int optval;
	int ret;

	ret = sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_RCVTIMEO, &tmpl->recvTimeout, 4);
	if (ret < 0)
		return ret;
	ret = sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &tmpl->sendTimeout, 4);
	if (ret < 0)
		return ret;
	ret = sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_NBIO, &tmpl->nonblock, 4);
	if (ret < 0)
		return ret;
	ret = sceNetSetsockopt(sock, SCE_NET_IPPROTO_TCP, SCE_NET_TCP_NODELAY, &tmpl->tcpNoDelay, 4);
	if (ret < 0)
		return ret;
	optval = 8;
	if ((tmpl->sockFlags & 1) == 0)
		optval = 0;
	return sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, HTTP_SO_PRIVATE_FLAGS, &optval, 4);
}

int _sceHttpPeek(HttpRequest *req, void *buf, SceSize len, int flags)
{
	return _sceHttpRequestRecvBuffered(req, buf, len, flags, 1, 0);
}

int _sceHttpSocketPeek(int sock, void *buf, SceSize len)
{
	return sceNetRecv(sock, buf, len, SCE_NET_MSG_PEEK);
}

/* sends through SSL once secured; with noDelay, TCP_NODELAY is on for this send only */
int _sceHttpConnectionSend(HttpConnection *conn, const void *msg, SceSize len, int flags,
			   SceBool nonblock, SceBool noDelay)
{
	SceSize optlen;
	int saved;
	int on;
	int ret, r;

	if (noDelay == 1) {
		optlen = 4;
		ret = sceNetGetsockopt(conn->sock, SCE_NET_IPPROTO_TCP, SCE_NET_TCP_NODELAY, &saved,
				       &optlen);
		if (ret < 0)
			return ret;
		on = 1;
		optlen = 4;
		ret = sceNetSetsockopt(conn->sock, SCE_NET_IPPROTO_TCP, SCE_NET_TCP_NODELAY, &on, 4);
		if (ret < 0)
			return ret;
	}

	if (conn->https == 1 && conn->state >= 6)
		ret = _sceHttpsSend(conn, msg, len);
	else
		ret = sceNetSend(conn->sock, msg, len, flags);
	if (ret == SCE_NET_ERROR_EAGAIN) {
		ret = SCE_HTTP_ERROR_EAGAIN;
		if (nonblock == 0 && (SceUInt32)_sceHttpGetSdkVersion() >= 0x1800000)
			ret = SCE_HTTP_ERROR_TIMEOUT;
	}

	if (noDelay == 1) {
		optlen = 4;
		r = sceNetSetsockopt(conn->sock, SCE_NET_IPPROTO_TCP, SCE_NET_TCP_NODELAY, &saved, 4);
		if (r < 0)
			ret = r;
	}
	return ret;
}

/* a connect's send timeout: the socket's own one saved, the template's set */
int _sceHttpSocketSetSendTimeout(int sock, const SceNetSockaddrIn *addr, SceUInt32 timeout,
				 int *saved)
{
	int optval = timeout;
	SceSize optlen = 4;
	int ret;

	ret = sceNetGetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, saved, &optlen);
	if (ret < 0)
		return ret;
	return sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &optval, 4);
}

int _sceHttpSocketRestoreSendTimeout(int sock, int saved)
{
	int optval = saved;

	return sceNetSetsockopt(sock, SCE_NET_SOL_SOCKET, SCE_NET_SO_SNDTIMEO, &optval, 4);
}

int _sceHttpSocketConnect(int sock, const SceNetSockaddrIn *addr, SceUInt32 timeout, int arg)
{
	int ret;

	ret = sceNetConnect(sock, (const SceNetSockaddr *)addr, sizeof(*addr));
	if (ret >= 0 || ret == SCE_NET_ERROR_EISCONN)
		return 0;
	if (ret == SCE_NET_ERROR_EINPROGRESS)
		return SCE_HTTP_ERROR_EAGAIN;
	if (ret == SCE_NET_ERROR_EAGAIN || ret == SCE_NET_ERROR_ETIMEDOUT)
		return SCE_HTTP_ERROR_TIMEOUT;
	return ret;
}

/* a resolver allocates with the request's allocator */
void *_sceHttpResolverAllocate(SceSize size, SceNetId rid, const char *name, void *user)
{
	const HttpAllocator *allocator = user;

	return _sceHttpMalloc(allocator->malloc, size);
}

void _sceHttpResolverFree(void *ptr, SceNetId rid, const char *name, void *user)
{
	const HttpAllocator *allocator = user;

	_sceHttpFree(allocator->free, ptr);
}

/* is the object on the epoll handle's list */
static inline int _sceHttpEpollCheck(HttpEpoll *ep, int id)
{
	HttpEpollEntry *entry;
	int ret, r;

	ret = sceKernelLockLwMutex(&ep->listLock, 1, NULL);
	if (ret < 0)
		return ret;
	entry = ep->entries;
	if (entry == NULL) {
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
	} else {
		while (entry->id != id) {
			entry = entry->next;
			if (entry == NULL) {
				ret = SCE_HTTP_ERROR_INVALID_VALUE;
				break;
			}
		}
	}
	r = sceKernelUnlockLwMutex(&ep->listLock, 1);
	if (r < 0)
		ret = r;
	return ret;
}

int _sceHttpRequestEpollRemove(HttpRequest *req, SceUInt32 kind)
{
	HttpEpoll *ep = req->tmpl.epoll;
	HttpConnection *conn;
	int ret, r;

	if (ep == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = sceKernelLockLwMutex(&ep->lock, 1, NULL);
	if (ret < 0)
		return ret;
	conn = _sceHttpRequestGetConnection(req);
	ret = _sceHttpEpollCheck(ep, req->object.id);
	if (ret == 0) {
		switch (kind) {
		case HTTP_EPOLL_SOCKET:
			ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_DEL, conn->sock, NULL);
			break;
		case HTTP_EPOLL_RESOLVER:
			ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_DEL, conn->resolverId,
						 NULL);
			break;
		case HTTP_EPOLL_ICM:
			ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_DEL, req->icmSock, NULL);
			break;
		default:
			ret = SCE_HTTP_ERROR_INVALID_VALUE;
			break;
		}
	}
	r = sceKernelUnlockLwMutex(&ep->lock, 1);
	if (r < 0)
		return r;
	return ret;
}

/* a resolver's error as libhttp's */
__attribute__((noinline)) int _sceHttpResolverGetError(SceNetId rid)
{
	int error;
	int ret;

	ret = sceNetResolverGetError(rid, &error);
	if (ret < 0)
		return ret;
	switch (error) {
	case SCE_NET_ERROR_RESOLVER_EPACKET:
		return SCE_HTTP_ERROR_RESOLVER_EPACKET;
	case SCE_NET_ERROR_RESOLVER_ENODNS:
		return SCE_HTTP_ERROR_RESOLVER_ENODNS;
	case SCE_NET_ERROR_RESOLVER_ETIMEDOUT:
		return SCE_HTTP_ERROR_RESOLVER_ETIMEDOUT;
	case SCE_NET_ERROR_RESOLVER_ENOSUPPORT:
		return SCE_HTTP_ERROR_RESOLVER_ENOSUPPORT;
	case SCE_NET_ERROR_RESOLVER_EFORMAT:
		return SCE_HTTP_ERROR_RESOLVER_EFORMAT;
	case SCE_NET_ERROR_RESOLVER_ESERVERFAILURE:
		return SCE_HTTP_ERROR_RESOLVER_ESERVERFAILURE;
	case SCE_NET_ERROR_RESOLVER_ENOHOST:
		return SCE_HTTP_ERROR_RESOLVER_ENOHOST;
	case SCE_NET_ERROR_RESOLVER_ENOTIMPLEMENTED:
		return SCE_HTTP_ERROR_RESOLVER_ENOTIMPLEMENTED;
	case SCE_NET_ERROR_RESOLVER_ESERVERREFUSED:
		return SCE_HTTP_ERROR_RESOLVER_ESERVERREFUSED;
	case SCE_NET_ERROR_RESOLVER_ENORECORD:
		return SCE_HTTP_ERROR_RESOLVER_ENORECORD;
	default:
		return error;
	}
}

int _sceHttpRequestResolverDestroy(HttpRequest *req)
{
	HttpConnection *conn = _sceHttpRequestGetConnection(req);
	int ret;

	if (conn->resolverId < 0)
		return 0;
	if (req->tmpl.nonblock == 1)
		_sceHttpRequestEpollRemove(req, HTTP_EPOLL_RESOLVER);
	ret = sceNetResolverDestroy(conn->resolverId);
	if (ret >= 0) {
		conn->resolverId = -1;
		conn->resolveState = 0;
		return 0;
	}
	if (ret == SCE_NET_ERROR_RESOLVER_EBUSY)
		return SCE_HTTP_ERROR_EAGAIN;
	return _sceHttpResolverGetError(conn->resolverId);
}

int _sceHttpRequestEpollAdd(HttpRequest *req, SceUInt32 kind, SceUInt32 events)
{
	HttpEpoll *ep = req->tmpl.epoll;
	HttpConnection *conn;
	SceNetEpollEvent ev;
	int ret, r;

	if (ep == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = sceKernelLockLwMutex(&ep->lock, 1, NULL);
	if (ret < 0)
		return ret;
	conn = _sceHttpRequestGetConnection(req);
	ret = _sceHttpEpollCheck(ep, req->object.id);
	if (ret == 0) {
		sceClibMemset(&ev, 0, sizeof(ev));
		ev.events = events;
		ev.data.ext.u32 = req->object.id;
		ev.data.fd = kind == HTTP_EPOLL_ICM ? req->icmSock : conn->sock;
		switch (kind) {
		case HTTP_EPOLL_SOCKET:
			ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_ADD, conn->sock, &ev);
			break;
		case HTTP_EPOLL_RESOLVER:
			ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_ADD, conn->resolverId,
						 &ev);
			break;
		case HTTP_EPOLL_ICM:
			ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_ADD, req->icmSock, &ev);
			break;
		default:
			ret = SCE_HTTP_ERROR_INVALID_VALUE;
			break;
		}
	}
	r = sceKernelUnlockLwMutex(&ep->lock, 1);
	if (r < 0)
		return r;
	return ret;
}

static inline const char *_sceHttpConnectionGetHost(const HttpConnection *conn)
{
	if (conn->tmpl.proxyEnabled == 1)
		return conn->tmpl.proxyServer;
	return conn->hostname;
}

/*
 * Resolves the connection's server (its proxy, when one is set) into its address: at once
 * when the name is an address, else with a resolver, which a nonblocking request waits for
 * on its epoll handle.
 */
int _sceHttpRequestResolve(HttpRequest *req)
{
	HttpConnection *conn = _sceHttpRequestGetConnection(req);
	SceNetResolverParam param;
	const char *host;
	int state;
	int started, r;
	int ret;

	ret = _sceHttpLock(0);
	if (ret < 0)
		goto fail;
	state = conn->resolveState;
	_sceHttpUnlock(0);
	switch (state) {
	case 0:
		host = _sceHttpConnectionGetHost(conn);
		sceClibMemset(&conn->addr, 0, sizeof(conn->addr));
		if (SceNetInternal_916EA798(SCE_NET_AF_INET, host, &conn->addr.sin_addr) > 0) {
			conn->addr.sin_len = sizeof(conn->addr);
			conn->addr.sin_family = SCE_NET_AF_INET;
			return 0;
		}
		if (conn->resolverId >= 0) {
			if (sceNetResolverDestroy(conn->resolverId) < 0) {
				ret = _sceHttpResolverGetError(conn->resolverId);
				break;
			}
			conn->resolverId = -1;
		}
		param.allocate = _sceHttpResolverAllocate;
		param.free = _sceHttpResolverFree;
		param.user = &req->tmpl.allocator;
		ret = sceNetResolverCreate("SceLibHttp", &param, 0);
		if (ret < 0) {
			if (ret == SCE_NET_ERROR_ENOLIBMEM)
				ret = SCE_HTTP_ERROR_OUT_OF_MEMORY;
			break;
		}
		conn->resolverId = ret;
		sceClibMemset(&conn->addr, 0, sizeof(conn->addr));
		if (req->tmpl.nonblock == 1) {
			ret = _sceHttpRequestEpollAdd(req, HTTP_EPOLL_RESOLVER, SCE_NET_EPOLLIN);
			if (ret < 0)
				break;
		}
		ret = _sceHttpLock(0);
		if (ret < 0)
			break;
		if (req->aborted == 1) {
			req->state = 19;
			ret = SCE_HTTP_ERROR_ABORTED;
		} else {
			conn->state = 1;
			conn->resolveState = 1;
		}
		_sceHttpUnlock(0);
		if (ret < 0)
			break;
		/* fall through */
	case 1:
		host = _sceHttpConnectionGetHost(conn);
		ret = _sceHttpLock(0);
		if (ret < 0)
			break;
		conn->resolveState = 2;
		_sceHttpUnlock(0);
		ret = sceKernelUnlockLwMutex(&req->lock, 1);
		if (ret < 0)
			break;
		started = sceNetResolverStartNtoa(conn->resolverId, host, &conn->addr.sin_addr,
						  conn->tmpl.resolveTimeout, conn->tmpl.resolveRetry,
						  req->tmpl.nonblock == 1);
		r = sceKernelLockLwMutex(&req->lock, 1, NULL);
		if (r < 0) {
			ret = r;
			break;
		}
		if (started < 0) {
			ret = _sceHttpResolverGetError(conn->resolverId);
			break;
		}
		/* fall through */
	case 2:
		if (req->tmpl.nonblock == 1)
			return SCE_HTTP_ERROR_EAGAIN;
		conn->resolveState = 3;
		/* fall through */
	case 3:
		conn->addr.sin_len = sizeof(conn->addr);
		conn->addr.sin_family = SCE_NET_AF_INET;
		return _sceHttpRequestResolverDestroy(req);
	case 4:
		ret = req->lastError;
		break;
	default:
		break;
	}
fail:
	_sceHttpRequestResolverDestroy(req);
	return ret;
}

int _sceHttpRequestResolverAbort(HttpRequest *req)
{
	SceNetId rid = _sceHttpRequestGetConnection(req)->resolverId;

	if (rid >= 0)
		return sceNetResolverAbort(rid, SCE_NET_RESOLVER_ABORT_FLAG_NTOA_PRESERVATION);
	return 0;
}

int _sceHttpConnectionClose(HttpConnection *conn)
{
	int ret = 0;

	if (conn->resolverId >= 0) {
		ret = sceNetResolverAbort(conn->resolverId,
					  SCE_NET_RESOLVER_ABORT_FLAG_NTOA_PRESERVATION);
		if (ret >= 0) {
			ret = sceNetResolverDestroy(conn->resolverId);
			if (ret < 0)
				return _sceHttpResolverGetError(conn->resolverId);
			conn->resolveState = 0;
			conn->resolverId = -1;
		}
	}
	return ret;
}

int _sceHttpConnectionOpenSocket(HttpConnection *conn)
{
	int sock;

	sock = sceNetSocket("SceLibHttp", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
	if (sock >= 0) {
		conn->sock = sock;
		conn->state = 2;
	} else {
		_sceHttpConnectionShutdown(conn);
	}
	return sock;
}

SceBool _sceHttpConnectionIsConnected(HttpConnection *conn)
{
	SceNetSockInfo info;

	return conn->aborted != 1 && sceNetGetSockInfo(conn->sock, &info, 1, 0) >= 0 &&
	       info.state == SCE_NET_SOCKINFO_STATE_ESTABLISHED;
}

int _sceHttpConnectionAbort(HttpConnection *conn)
{
	int sock = conn->sock;

	conn->aborted = 1;
	return sceNetSocketAbort(sock, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION |
				       SCE_NET_SOCKET_ABORT_FLAG_SND_PRESERVATION);
}

/* the request's "SceLibHttpIcm" socket, opened once and started */
int _sceHttpRequestIcmOpen(HttpRequest *req)
{
	int optval;
	int ret;

	ret = sceKernelLockLwMutex(&req->lock, 1, NULL);
	if (ret < 0)
		return ret;
	if (req->aborted == 1) {
		ret = SCE_HTTP_ERROR_ABORTED;
		goto fail;
	}
	if (req->icmSock == -1) {
		ret = sceNetSocket("SceLibHttpIcm", SCE_NET_AF_INET, SCE_NET_SOCK_DGRAM, 0);
		if (ret < 0)
			goto fail;
		req->icmSock = ret;
		if ((req->icmOption & 1) != 0) {
			optval = 1;
			ret = sceNetSetsockopt(ret, SCE_NET_SOL_SOCKET, HTTP_SO_ICM_OPTION, &optval, 4);
			if (ret < 0)
				goto fail;
		}
	}
	if (req->tmpl.nonblock == 1) {
		ret = _sceHttpRequestEpollAdd(req, HTTP_EPOLL_ICM, HTTP_EPOLL_ICM_EVENTS);
		if (ret < 0)
			goto fail;
		ret = sceKernelUnlockLwMutex(&req->lock, 1);
		if (ret < 0)
			goto fail;
		ret = SceNetInternal_93F2FF08(req->icmSock, 128);
	} else {
		ret = sceKernelUnlockLwMutex(&req->lock, 1);
		if (ret < 0)
			goto fail;
		ret = SceNetInternal_93F2FF08(req->icmSock, 0);
	}
	if (ret >= 0)
		return ret;
	sceKernelLockLwMutex(&req->lock, 1, NULL);
fail:
	if (req->icmSock != -1) {
		sceNetSocketClose(req->icmSock);
		req->icmSock = -1;
	}
	sceKernelUnlockLwMutex(&req->lock, 1);
	return ret;
}

int _sceHttpRequestIcmAbort(HttpRequest *req)
{
	if (req->icmSock != -1)
		return sceNetSocketAbort(req->icmSock, SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION |
					 SCE_NET_SOCKET_ABORT_FLAG_SND_PRESERVATION);
	return 0;
}

int _sceHttpRequestIcmClose(HttpRequest *req)
{
	int ret = 0;

	if (req->icmSock != -1) {
		if (req->tmpl.nonblock == 1)
			ret = _sceHttpRequestEpollRemove(req, HTTP_EPOLL_ICM);
		sceNetSocketClose(req->icmSock);
		req->icmSock = -1;
	}
	return ret;
}

/* the template's proxy from the network settings (auto proxy configuration) */
int _sceHttpTemplateGetProxyConfig(HttpRequest *req, HttpTemplate *tmpl)
{
	SceNetCtlInfo info;
	int ret;

	ret = sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_HTTP_PROXY_CONFIG, &info);
	if (ret < 0)
		return ret;
	if (info.http_proxy_config != SCE_NET_CTL_HTTP_PROXY_ON) {
		tmpl->proxyEnabled = 0;
		return ret;
	}
	tmpl->proxyEnabled = 1;
	ret = sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_HTTP_PROXY_PORT, &info);
	if (ret < 0)
		return ret;
	tmpl->proxyPort = info.http_proxy_port;
	sceClibMemset(&info, 0, sizeof(info));
	ret = sceNetCtlInetGetInfo(SCE_NET_CTL_INFO_HTTP_PROXY_SERVER, &info);
	if (ret < 0)
		return ret;
	_sceHttpUriNormalize(&tmpl->allocator, info.http_proxy_server);
	ret = _sceHttpLock(0);
	if (ret < 0)
		return ret;
	ret = _sceHttpTemplateSetProxyServer(tmpl, info.http_proxy_server,
					     sceClibStrnlen(info.http_proxy_server,
							    sizeof(info.http_proxy_server)));
	_sceHttpUnlock(0);
	return ret;
}

/* waits on the handle's epoll, unless sceHttpAbortWaitRequest was called */
int _sceHttpEpollWait(SceHttpEpollHandle eh, SceNetEpollEvent *events, int maxevents, int timeout,
		      SceBool cb)
{
	HttpEpoll *ep = eh;
	SceNetId eid;
	int ret, r;

	ret = sceKernelLockLwMutex(&ep->lock, 1, NULL);
	if (ret < 0)
		return ret;
	if (ep->aborted != 1) {
		eid = ep->epollId;
		ret = sceKernelUnlockLwMutex(&ep->lock, 1);
		if (ret < 0)
			return ret;
		if (cb != 0)
			return sceNetEpollWaitCB(eid, events, maxevents, timeout);
		return sceNetEpollWait(eid, events, maxevents, timeout);
	}
	ret = SCE_HTTP_ERROR_ABORTED;
	r = sceKernelUnlockLwMutex(&ep->lock, 1);
	if (r < 0)
		return r;
	return ret;
}

int _sceHttpEpollAbort(SceHttpEpollHandle eh)
{
	HttpEpoll *ep = eh;
	int ret, r;

	ret = sceKernelLockLwMutex(&ep->lock, 1, NULL);
	if (ret < 0)
		return ret;
	if (ep->aborted != 1) {
		ret = sceNetEpollAbort(ep->epollId, SCE_NET_EPOLL_ABORT_FLAG_PRESERVATION);
		ep->aborted = 1;
	}
	r = sceKernelUnlockLwMutex(&ep->lock, 1);
	if (r < 0)
		return r;
	return ret;
}

/* puts a request on the handle's list, or changes its userArg */
int _sceHttpEpollAdd(SceHttpEpollHandle eh, int id, void *userArg)
{
	HttpEpoll *ep = eh;
	HttpEpollEntry *entry, *e, *last;
	int ret;

	if (ep == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	entry = _sceHttpMalloc(NULL, sizeof(*entry));
	if (entry == NULL)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	entry->id = id;
	entry->userArg = userArg;
	entry->next = NULL;
	ret = sceKernelLockLwMutex(&ep->listLock, 1, NULL);
	if (ret < 0)
		return ret;
	e = ep->entries;
	if (e == NULL) {
		ep->entries = entry;
	} else {
		last = NULL;
		for (; e != NULL; e = e->next) {
			if (e->id == id) {
				e->userArg = userArg;
				_sceHttpFree(NULL, entry);
				goto out;
			}
			last = e;
		}
		if (last != NULL)
			last->next = entry;
	}
out:
	return sceKernelUnlockLwMutex(&ep->listLock, 1);
}

int _sceHttpEpollFind(SceHttpEpollHandle eh, int id, HttpEpollEntry **entry)
{
	HttpEpoll *ep = eh;
	HttpEpollEntry *e;
	int ret, r;

	if (ep == NULL || ep->entries == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = sceKernelLockLwMutex(&ep->listLock, 1, NULL);
	if (ret < 0)
		return ret;
	for (e = ep->entries; e != NULL; e = e->next) {
		if (e->id == id) {
			*entry = e;
			break;
		}
	}
	if (e == NULL)
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
	r = sceKernelUnlockLwMutex(&ep->listLock, 1);
	if (r < 0)
		return r;
	return ret;
}

/* changes the events a request (or, HTTP_EPOLL_CONNECTION, a connection) waits for */
int _sceHttpEpollModify(void *object, SceUInt32 kind, SceUInt32 events)
{
	HttpRequest *req = NULL;
	HttpConnection *conn;
	HttpEpoll *ep;
	SceNetEpollEvent ev;
	int id, sock;
	int ret, r;

	if (kind == HTTP_EPOLL_CONNECTION) {
		conn = object;
		ep = conn->tmpl.epoll;
		if (ep == NULL)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		ret = sceKernelLockLwMutex(&ep->lock, 1, NULL);
		if (ret < 0)
			return ret;
		sceClibMemset(&ev, 0, sizeof(ev));
		id = conn->object.id;
		sock = conn->sock;
	} else {
		req = object;
		ep = req->tmpl.epoll;
		if (ep == NULL)
			return SCE_HTTP_ERROR_INVALID_VALUE;
		ret = sceKernelLockLwMutex(&ep->lock, 1, NULL);
		if (ret < 0)
			return ret;
		conn = _sceHttpRequestGetConnection(req);
		ret = _sceHttpEpollCheck(ep, req->object.id);
		if (ret != 0)
			goto out;
		sceClibMemset(&ev, 0, sizeof(ev));
		id = req->object.id;
		sock = conn->sock;
	}
	ev.events = events;
	ev.data.ext.u32 = id;
	ev.data.fd = sock;
	switch (kind) {
	case HTTP_EPOLL_SOCKET:
	case HTTP_EPOLL_CONNECTION:
		ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_MOD, conn->sock, &ev);
		break;
	case HTTP_EPOLL_RESOLVER:
		ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_MOD, conn->resolverId, &ev);
		break;
	case HTTP_EPOLL_ICM:
		ret = sceNetEpollControl(ep->epollId, SCE_NET_EPOLL_CTL_MOD, req->icmSock, &ev);
		break;
	default:
		ret = SCE_HTTP_ERROR_INVALID_VALUE;
		break;
	}
out:
	r = sceKernelUnlockLwMutex(&ep->lock, 1);
	if (r < 0)
		return r;
	return ret;
}

int _sceHttpEpollRemove(SceHttpEpollHandle eh, int id)
{
	HttpEpoll *ep = eh;
	HttpEpollEntry *entry, *prev, *next;
	int ret;

	if (ep == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ret = sceKernelLockLwMutex(&ep->listLock, 1, NULL);
	if (ret < 0)
		return ret;
	prev = ep->entries;
	if (prev != NULL) {
		if (prev->id == id) {
			entry = prev;
			ep->entries = entry->next;
		} else {
			for (;;) {
				next = prev->next;
				if (next != NULL && next->id == id) {
					entry = next;
					prev->next = entry->next;
					break;
				}
				prev = next;
				if (next == NULL)
					goto out;
			}
		}
		if (entry != NULL)
			_sceHttpFree(NULL, entry);
	}
out:
	return sceKernelUnlockLwMutex(&ep->listLock, 1);
}

int _sceHttpCreateEpoll(SceHttpEpollHandle *eh)
{
	HttpEpoll *ep;
	int ret;

	if (eh == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	ep = _sceHttpMalloc(NULL, sizeof(*ep));
	if (ep == NULL)
		return SCE_HTTP_ERROR_OUT_OF_MEMORY;
	sceClibMemset(ep, 0, sizeof(*ep));
	ret = sceKernelCreateLwMutex(&ep->listLock, "SceLibHttpEhList",
				     SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelCreateLwMutex(&ep->lock, "SceLibHttpEh", SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0,
				     NULL);
	if (ret < 0)
		return ret;
	ep->epollId = sceNetEpollCreate("SceLibHttp", 0);
	if (ep->epollId < 0)
		return ep->epollId;
	*eh = ep;
	return ret;
}

int _sceHttpDestroyEpoll(SceHttpEpollHandle eh)
{
	HttpEpoll *ep = eh;
	HttpEpollEntry *entry, *next;
	int ret;

	if (ep == NULL)
		return SCE_HTTP_ERROR_INVALID_VALUE;
	for (entry = ep->entries; entry != NULL; entry = next) {
		next = entry->next;
		_sceHttpFree(NULL, entry);
	}
	ret = sceKernelDeleteLwMutex(&ep->listLock);
	if (ret < 0)
		return ret;
	ret = sceKernelDeleteLwMutex(&ep->lock);
	if (ret < 0)
		return ret;
	ret = sceNetEpollDestroy(ep->epollId);
	if (ret < 0)
		return ret;
	_sceHttpFree(NULL, ep);
	return ret;
}
