/*
 * SceLibHttp's sockets: a connection's receive buffer and the reads through it (buffered,
 * peeked, or straight into the caller's buffer), a chunked body's reader, the socket
 * options a template sets, and sending.
 */

#include <net/errno.h>

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
