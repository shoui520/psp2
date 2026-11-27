/*
 * sw_net_vita.c — Vita SceNet POSIX-like socket, epoll, resolver, select,
 * and IP_TTL inspection wrappers for DTCPIP-SW.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include <net.h>

#define SW_NET_EINTR 4
#define SW_NET_EAGAIN 35
#define SW_NET_EINPROGRESS 36
#define SW_NET_ETIMEDOUT 116
#define SW_NET_ECANCELED 140
#define SW_NET_EINVAL 22
#define SW_NET_SO_REUSEADDR 4
#define SW_NET_SO_ERROR 4103
#define SW_NET_SO_NBIO 4352
#define SW_NET_SOL_SOCKET 0xFFFF
#define SW_NET_IPPROTO_IP 0
#define SW_NET_IPPROTO_TCP 6
#define SW_NET_TCP_NODELAY 1
#define SW_NET_IP_TTL 4
#define SW_NET_IP_RECVTTL 23
#define SW_NET_IP_GETTTL 24

struct SwFdSet {
	uint32_t bits[32];
};

static int swNetMapErrno(int err, int ctx)
{
	(void)ctx;
	return err;
}

int swNetSocketCreate(int *outSock, int domain, int type)
{
	int s = sceNetSocket("sce_sws_socket", domain, type, 0);

	if (s >= 0) {
		*outSock = s;
		return 0;
	}
	return swNetMapErrno(*sceNetErrnoLoc(), 0);
}

int swNetSocketCreateWithEpoll(int *outSock, int domain, int type, int *outEpoll)
{
	int ret = SW_NET_EINVAL;

	if (outEpoll == NULL)
		return ret;
	*outEpoll = sceNetEpollCreate("sws socket epoll", 0);
	if (*outEpoll < 0)
		return swNetMapErrno(*sceNetErrnoLoc(), 0);
	ret = swNetSocketCreate(outSock, domain, type);
	if (ret != 0) {
		sceNetEpollDestroy(*outEpoll);
		*outEpoll = -1;
	}
	return ret;
}

int swNetSocketClose(int sock)
{
	if (sceNetSocketClose(sock) >= 0)
		return 0;
	return swNetMapErrno(*sceNetErrnoLoc(), 0);
}

int swNetSocketCloseWithEpoll(int sock, int epollId)
{
	int ret = swNetSocketClose(sock);

	if (epollId >= 0)
		sceNetEpollDestroy(epollId);
	return ret;
}

int swNetSocketBind(int sock, const SceNetSockaddr *addr, unsigned int addrLen, int reuseAddr)
{
	int optval = 1;

	if (reuseAddr != 0)
		sceNetSetsockopt(sock, SW_NET_SOL_SOCKET, SW_NET_SO_REUSEADDR, &optval, 4u);
	if (sceNetBind(sock, addr, addrLen) >= 0)
		return 0;
	return swNetMapErrno(*sceNetErrnoLoc(), 0);
}

int swNetSocketListen(int sock, int backlog)
{
	if (sceNetListen(sock, backlog) >= 0)
		return 0;
	return swNetMapErrno(*sceNetErrnoLoc(), 0);
}

int swNetSocketAccept(int sock, SceNetSockaddr *addr, unsigned int *addrLen, int *outSock)
{
	int client;

	for (;;) {
		client = sceNetAccept(sock, addr, addrLen);
		if (client >= 0)
			break;
		if (*sceNetErrnoLoc() != SW_NET_EINTR)
			return swNetMapErrno(*sceNetErrnoLoc(), 0);
	}
	*outSock = client;
	return 0;
}

int swNetSocketSend(int sock, const void *buf, unsigned int len, int *outSent)
{
	int sent;

	*outSent = 0;
	for (;;) {
		sent = sceNetSend(sock, buf, len, 0);
		if (sent >= 0)
			break;
		{
			int err = *sceNetErrnoLoc();

			if (err != SW_NET_EINTR && err != SW_NET_EAGAIN)
				return swNetMapErrno(err, 0);
		}
	}
	*outSent = sent;
	return 0;
}

int swNetSocketRecv(int sock, void *buf, unsigned int len, int *outRecv)
{
	int n;

	for (;;) {
		n = sceNetRecv(sock, buf, len, 0);
		if (n >= 0)
			break;
		if (*sceNetErrnoLoc() != SW_NET_EINTR)
			return swNetMapErrno(*sceNetErrnoLoc(), 0);
	}
	*outRecv = n;
	return 0;
}

int swNetSocketConnect(int sock, const SceNetSockaddr *addr, unsigned int addrLen)
{
	while (sceNetConnect(sock, addr, addrLen) < 0) {
		if (*sceNetErrnoLoc() != SW_NET_EINTR)
			return swNetMapErrno(*sceNetErrnoLoc(), 0);
	}
	return 0;
}

int swNetGetSocketError(int sock)
{
	int optval = 0;
	unsigned int optlen = 4u;

	if (sceNetGetsockopt(sock, SW_NET_SOL_SOCKET, SW_NET_SO_ERROR, &optval, &optlen) >= 0) {
		if (optval != 0)
			return swNetMapErrno(optval, 0);
		return 0;
	}
	optval = *sceNetErrnoLoc();
	return swNetMapErrno(optval, 0);
}

int swNetSocketConnectTimeout(int sock, const SceNetSockaddr *addr, unsigned int addrLen,
			      int timeoutMs, int epollId, const int *cancelFlag)
{
	int optval;
	SceNetEpollEvent event;
	int status;

	if (timeoutMs < 0)
		return swNetSocketConnect(sock, addr, addrLen);
	if (epollId < 0)
		return SW_NET_EINVAL;

	optval = 1;
	if (sceNetSetsockopt(sock, SW_NET_SOL_SOCKET, SW_NET_SO_NBIO, &optval, 4u) < 0)
		return swNetMapErrno(*sceNetErrnoLoc(), 0);

	memset(&event, 0, sizeof(event));
	event.events = SCE_NET_EPOLLIN | SCE_NET_EPOLLOUT;
	if (sceNetEpollControl(epollId, SCE_NET_EPOLL_CTL_ADD, sock, &event) < 0)
		return swNetMapErrno(*sceNetErrnoLoc(), 0);

	status = sceNetConnect(sock, addr, addrLen);
	if (status != 0) {
		int err = *sceNetErrnoLoc();

		if (status < 0 && err == SW_NET_EINPROGRESS) {
			int elapsed = 0;

			while (elapsed < timeoutMs) {
				status = sceNetEpollWait(epollId, &event, 1, 100000);
				if (status != 0)
					break;
				if (cancelFlag != NULL && *cancelFlag != 0) {
					status = SW_NET_ECANCELED;
					break;
				}
				elapsed += 100;
			}
			if (elapsed >= timeoutMs) {
				status = SW_NET_ETIMEDOUT;
			} else if (status != SW_NET_ECANCELED) {
				if (status <= 0)
					status = swNetMapErrno(*sceNetErrnoLoc(), 0);
				else
					status = swNetGetSocketError(sock);
			}
		} else {
			status = swNetMapErrno(err, 0);
		}
	}

	optval = 0;
	if (sceNetSetsockopt(sock, SW_NET_SOL_SOCKET, SW_NET_SO_NBIO, &optval, 4u) < 0)
		(void)*sceNetErrnoLoc();
	return status;
}

int swNetGetSockName(int sock, SceNetSockaddr *addr, unsigned int *addrLen)
{
	if (sceNetGetsockname(sock, addr, addrLen) >= 0)
		return 0;
	return swNetMapErrno(*sceNetErrnoLoc(), 0);
}

int swNetGetPeerName(int sock, SceNetSockaddr *addr, unsigned int *addrLen)
{
	if (sceNetGetpeername(sock, addr, addrLen) >= 0)
		return 0;
	return swNetMapErrno(*sceNetErrnoLoc(), 0);
}

int swNetSetTcpNoDelay(int sock, int enable)
{
	int optval = (enable == 1) ? 1 : 0;

	return sceNetSetsockopt(sock, SW_NET_IPPROTO_TCP, SW_NET_TCP_NODELAY, &optval, 4u);
}

int swNetSetIpTtl(int sock, int ttl)
{
	int optval = (unsigned char)ttl;

	return sceNetSetsockopt(sock, SW_NET_IPPROTO_IP, SW_NET_IP_TTL, &optval, 4u);
}

int swNetResolveHost(const char *hostname, uint16_t port, unsigned char family,
		     SceNetSockaddrIn *outAddr)
{
	int rid;
	int ret;
	SceNetInAddr addr;

	rid = sceNetResolverCreate("sws_net_resolver", NULL, 0);
	ret = sceNetResolverStartNtoa(rid, hostname, &addr, 3000000, 0, 0);
	if (ret < 0) {
		sceNetResolverDestroy(rid);
		return ret;
	}
	ret = sceNetResolverDestroy(rid);
	if (ret < 0)
		return ret;

	outAddr->sin_addr = addr;
	outAddr->sin_family = family;
	outAddr->sin_port = (uint16_t)((port << 8) | (port >> 8));
	return 0;
}

int swNetInetPton(int family, const char *src, void *dst)
{
	return (sceNetInetPton(family, src, dst) <= 0) ? SW_NET_EINVAL : 0;
}

int swNetInetNtop(int family, const void *src, char *dst, unsigned int size)
{
	if (sceNetInetNtop(family, src, dst, size) != NULL)
		return 0;
	dst[0] = '\0';
	return swNetMapErrno(*sceNetErrnoLoc(), 0);
}

int swNetWaitReadable(int sock, int timeoutMs, int epollId, const int *cancelFlag)
{
	SceNetEpollEvent event;
	int ready = 0;
	int elapsed = 0;
	int ret;

	memset(&event, 0, sizeof(event));
	event.events = SCE_NET_EPOLLIN;
	event.data.fd = sock;
	if (sceNetEpollControl(epollId, SCE_NET_EPOLL_CTL_MOD, sock, &event) < 0)
		return 0;

	while (elapsed < timeoutMs) {
		ret = sceNetEpollWait(epollId, &event, 1, 100000);
		if (ret != 0 || cancelFlag == NULL || *cancelFlag != 0) {
			if (ret > 0) {
				if ((event.events & SCE_NET_EPOLLIN) != 0)
					ready = 1;
				if ((event.events & SCE_NET_EPOLLERR) != 0)
					swNetGetSocketError(sock);
			}
			break;
		}
		elapsed += 100;
	}
	return ready;
}

int swNetWaitWritable(int sock, int timeoutMs, int epollId, const int *cancelFlag)
{
	SceNetEpollEvent event;
	int ready = 0;
	int elapsed = 0;
	int ret;

	memset(&event, 0, sizeof(event));
	event.events = SCE_NET_EPOLLOUT;
	event.data.fd = sock;
	if (sceNetEpollControl(epollId, SCE_NET_EPOLL_CTL_MOD, sock, &event) < 0)
		return 0;

	while (elapsed < timeoutMs) {
		ret = sceNetEpollWait(epollId, &event, 1, 100000);
		if (ret != 0 || cancelFlag == NULL || *cancelFlag != 0) {
			if (ret > 0) {
				if ((event.events & SCE_NET_EPOLLOUT) != 0)
					ready = 1;
				if ((event.events & SCE_NET_EPOLLERR) != 0)
					swNetGetSocketError(sock);
			}
			break;
		}
		elapsed += 100;
	}
	return ready;
}

int swNetSelect(int nfds, struct SwFdSet *readFds, struct SwFdSet *writeFds,
		int timeoutMs, int *outCount)
{
	struct SwFdSet exceptFds;
	SceNetEpollEvent event;
	SceNetEpollEvent events[3];
	int readyCount = 0;
	unsigned int i;

	if (outCount != NULL)
		*outCount = 0;
	for (i = 0; i < 32u; i++)
		exceptFds.bits[i] = 0;

	for (;;) {
		int eid = sceNetEpollCreate("sce_sws_select", 0);
		int fd;
		int nev;
		int count;

		if (eid < 0)
			return readyCount;

		for (fd = 0; fd < nfds; fd++) {
			uint32_t mask = 0;
			unsigned int word = (unsigned int)fd >> 5;
			uint32_t bit = 1u << (fd & 31);

			if (readFds != NULL && (readFds->bits[word] & bit) != 0)
				mask |= SCE_NET_EPOLLIN;
			if (writeFds != NULL && (writeFds->bits[word] & bit) != 0)
				mask |= SCE_NET_EPOLLOUT;
			if (mask != 0 || (exceptFds.bits[word] & bit) != 0) {
				memset(&event, 0, sizeof(event));
				event.events = mask;
				event.data.fd = fd;
				if (sceNetEpollControl(eid, SCE_NET_EPOLL_CTL_ADD, fd, &event) < 0) {
					sceNetEpollDestroy(eid);
					return readyCount;
				}
			}
		}

		nev = sceNetEpollWait(eid, events, 3, 1000 * timeoutMs);
		if (nev < 0) {
			sceNetEpollDestroy(eid);
			return readyCount;
		}

		if (readFds != NULL) {
			for (i = 0; i < 32u; i++)
				readFds->bits[i] = 0;
		}
		if (writeFds != NULL) {
			for (i = 0; i < 32u; i++)
				writeFds->bits[i] = 0;
		}
		for (i = 0; i < 32u; i++)
			exceptFds.bits[i] = 0;

		count = 0;
		for (fd = 0; fd < nev; fd++) {
			unsigned int efd = (unsigned int)events[fd].data.fd;
			uint32_t ev = events[fd].events;

			if ((ev & SCE_NET_EPOLLIN) != 0) {
				if (readFds != NULL && efd < 1024u)
					readFds->bits[efd >> 5] |= 1u << (efd & 31u);
				count++;
			}
			if ((ev & SCE_NET_EPOLLOUT) != 0) {
				if (writeFds != NULL && efd < 1024u)
					writeFds->bits[efd >> 5] |= 1u << (efd & 31u);
				count++;
			}
			if ((ev & (SCE_NET_EPOLLERR | SCE_NET_EPOLLHUP)) != 0) {
				if (efd < 1024u)
					exceptFds.bits[efd >> 5] |= 1u << (efd & 31u);
				count++;
			}
		}

		sceNetEpollDestroy(eid);
		readyCount = count;
		if (count > 0) {
			if (outCount != NULL)
				*outCount = readyCount;
			return 0;
		}
		if (readyCount == 0)
			return SW_NET_ETIMEDOUT;
		if (*sceNetErrnoLoc() != SW_NET_EINTR)
			return swNetMapErrno(*sceNetErrnoLoc(), 0);
	}
}

/* IP_TTL inspection helpers for DTCP-IP locality check (TTL <= 3) */
int swTtlEnableCapture(int *outHandle, int sock)
{
	int optval = 1;

	if (sceNetSetsockopt(sock, SW_NET_IPPROTO_IP, SW_NET_IP_RECVTTL, &optval, 4u) >= 0) {
		int curTtl = 0;
		unsigned int optlen = 4u;

		sceNetGetsockopt(sock, SW_NET_IPPROTO_IP, SW_NET_IP_GETTTL, &curTtl, &optlen);
		*outHandle = sock;
		return 0;
	}
	*outHandle = -1;
	return *sceNetErrnoLoc();
}

int swTtlOpenSink(int *outHandle, int sock)
{
	return swTtlEnableCapture(outHandle, sock);
}

int swTtlCloseSink(int handle)
{
	(void)handle;
	return 0;
}

int swTtlOpenSource(int *outHandle, int sock)
{
	return swTtlEnableCapture(outHandle, sock);
}

int swTtlOpenSourceWithCtx(void *unusedCtx, int *outHandle, int sock)
{
	(void)unusedCtx;
	return swTtlEnableCapture(outHandle, sock);
}

int swTtlGetPeerTtl(int handle, int *outTtl)
{
	int ttl = 0;
	unsigned int optlen = 4u;

	if (handle == -1)
		return -1;
	if (sceNetGetsockopt(handle, SW_NET_IPPROTO_IP, SW_NET_IP_GETTTL, &ttl, &optlen) != 0)
		return *sceNetErrnoLoc();
	*outTtl = ttl;
	return 0;
}

int swTtlCloseSource(int handle)
{
	(void)handle;
	return 0;
}
