/*
 * SceNetInet BSD socket API implementation.
 */
#include "pspnet_inet_internal.h"

InetSocket *inetGetSocket(int sockFd)
{
    if (!g_inet.initialized || sockFd < 0 || sockFd >= INET_MAX_SOCKETS) {
        return NULL;
    }
    if (!g_inet.sockets[sockFd].inUse) {
        return NULL;
    }
    return &g_inet.sockets[sockFd];
}

u16 inetAllocEphemeralPort(void)
{
    u16 port = g_inet.nextEphemeralPort++;
    if (g_inet.nextEphemeralPort < 49152) {
        g_inet.nextEphemeralPort = 49152;
    }
    /* Return in network byte order */
    return (u16)((port >> 8) | ((port & 0xFFU) << 8));
}

int inetSocketCreateInternal(int domain, int type, int protocol)
{
    int idx;

    if (!g_inet.initialized) {
        inetSetErrno(INET_EINVAL);
        return -1;
    }
    if (domain != INET_AF_INET) {
        inetSetErrno(INET_EAFNOSUPPORT);
        return -1;
    }
    if (type != INET_SOCK_STREAM && type != INET_SOCK_DGRAM && type != INET_SOCK_RAW) {
        inetSetErrno(INET_EPROTONOSUPPORT);
        return -1;
    }

    for (idx = 0; idx < INET_MAX_SOCKETS; idx++) {
        if (!g_inet.sockets[idx].inUse) {
            InetSocket *sock = &g_inet.sockets[idx];
            sceKernelMemset(sock, 0, sizeof(*sock));
            sock->inUse = 1;
            sock->domain = domain;
            sock->type = type;
            if (protocol == 0) {
                sock->protocol = (type == INET_SOCK_STREAM) ? INET_IPPROTO_TCP : INET_IPPROTO_UDP;
            } else {
                sock->protocol = protocol;
            }
            sock->state = INET_SOCK_STATE_CREATED;
            sock->sndBufSize = 16384;
            sock->rcvBufSize = 16384;
            sock->localAddr.sinLen = sizeof(SceNetInetSockaddrIn);
            sock->localAddr.sinFamily = INET_AF_INET;
            inetSetErrno(0);
            return idx;
        }
    }

    inetSetErrno(INET_EMFILE);
    return -1;
}

int sceNetInetSocket(int domain, int type, int protocol)
{
    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    return inetSocketCreateInternal(domain, type, protocol);
}

int sceNetInetBind(int sockFd, const SceNetInetSockaddr *name, u32 namelen)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (name == NULL || namelen < sizeof(SceNetInetSockaddrIn)) {
        inetSetErrno(INET_EINVAL);
        return -1;
    }

    sceKernelMemcpy(&sock->localAddr, name, sizeof(SceNetInetSockaddrIn));
    if (sock->localAddr.sinPort == 0) {
        sock->localAddr.sinPort = inetAllocEphemeralPort();
    }
    sock->state = INET_SOCK_STATE_BOUND;
    inetSetErrno(0);
    return 0;
}

int sceNetInetListen(int sockFd, int backlog)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (sock->type != INET_SOCK_STREAM) {
        inetSetErrno(INET_EOPNOTSUPP);
        return -1;
    }
    sock->backlog = (backlog > 0) ? backlog : 1;
    sock->state = INET_SOCK_STATE_LISTENING;
    inetSetErrno(0);
    return 0;
}

int sceNetInetAccept(int sockFd, SceNetInetSockaddr *addr, u32 *addrlen)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (sock->state != INET_SOCK_STATE_LISTENING) {
        inetSetErrno(INET_EINVAL);
        return -1;
    }
    (void)addr;
    (void)addrlen;
    inetSetErrno(INET_EAGAIN);
    return -1;
}

int sceNetInetConnect(int sockFd, const SceNetInetSockaddr *name, u32 namelen)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (name == NULL || namelen < sizeof(SceNetInetSockaddrIn)) {
        inetSetErrno(INET_EINVAL);
        return -1;
    }
    if (sock->localAddr.sinPort == 0) {
        sock->localAddr.sinPort = inetAllocEphemeralPort();
    }
    sceKernelMemcpy(&sock->peerAddr, name, sizeof(SceNetInetSockaddrIn));
    sock->state = INET_SOCK_STATE_CONNECTED;
    inetSetErrno(0);
    return 0;
}

int sceNetInetSendto(int sockFd, const void *buf, u32 len, int flags,
                     const SceNetInetSockaddr *to, u32 tolen)
{
    InetSocket *sock;

    (void)flags;
    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (buf == NULL && len > 0) {
        inetSetErrno(INET_EFAULT);
        return -1;
    }
    if (sock->shutWrite || sock->aborted) {
        inetSetErrno(INET_ESHUTDOWN);
        return -1;
    }
    if (sock->type == INET_SOCK_STREAM && sock->state != INET_SOCK_STATE_CONNECTED) {
        inetSetErrno(INET_ENOTCONN);
        return -1;
    }
    if (to != NULL && tolen >= sizeof(SceNetInetSockaddrIn)) {
        sceKernelMemcpy(&sock->peerAddr, to, sizeof(SceNetInetSockaddrIn));
    }
    inetSetErrno(0);
    return (int)len;
}

int sceNetInetSend(int sockFd, const void *buf, u32 len, int flags)
{
    return sceNetInetSendto(sockFd, buf, len, flags, NULL, 0);
}

int sceNetInetSendmsg(int sockFd, const SceNetInetMsghdr *msg, int flags)
{
    int idx;
    int totalSent = 0;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    if (msg == NULL || (msg->msgIov == NULL && msg->msgIovlen > 0)) {
        inetSetErrno(INET_EINVAL);
        return -1;
    }
    for (idx = 0; idx < msg->msgIovlen; idx++) {
        int sent = sceNetInetSendto(sockFd, msg->msgIov[idx].iovBase, msg->msgIov[idx].iovLen,
                                    flags, (const SceNetInetSockaddr *)msg->msgName, msg->msgNamelen);
        if (sent < 0) {
            return (totalSent > 0) ? totalSent : -1;
        }
        totalSent += sent;
    }
    return totalSent;
}

int sceNetInetRecvfrom(int sockFd, void *buf, u32 len, int flags,
                       SceNetInetSockaddr *from, u32 *fromlen)
{
    InetSocket *sock;
    u32 toRead;
    u32 idx;
    u8 *dst = (u8 *)buf;

    (void)flags;
    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (buf == NULL && len > 0) {
        inetSetErrno(INET_EFAULT);
        return -1;
    }
    if (sock->aborted) {
        inetSetErrno(INET_ECONNABORTED);
        return -1;
    }
    if (sock->shutRead) {
        return 0;
    }
    if (sock->rxCount == 0) {
        inetSetErrno(INET_EAGAIN);
        return -1;
    }

    toRead = (len < sock->rxCount) ? len : sock->rxCount;
    for (idx = 0; idx < toRead; idx++) {
        dst[idx] = sock->rxBuf[sock->rxHead];
        sock->rxHead = (sock->rxHead + 1U) % INET_RING_BUF_SIZE;
    }
    sock->rxCount -= toRead;

    if (from != NULL && fromlen != NULL && *fromlen >= sizeof(SceNetInetSockaddrIn)) {
        sceKernelMemcpy(from, &sock->peerAddr, sizeof(SceNetInetSockaddrIn));
        *fromlen = sizeof(SceNetInetSockaddrIn);
    }
    inetSetErrno(0);
    return (int)toRead;
}

int sceNetInetRecv(int sockFd, void *buf, u32 len, int flags)
{
    return sceNetInetRecvfrom(sockFd, buf, len, flags, NULL, NULL);
}

int sceNetInetRecvmsg(int sockFd, SceNetInetMsghdr *msg, int flags)
{
    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    if (msg == NULL || msg->msgIov == NULL || msg->msgIovlen <= 0) {
        inetSetErrno(INET_EINVAL);
        return -1;
    }
    return sceNetInetRecvfrom(sockFd, msg->msgIov[0].iovBase, msg->msgIov[0].iovLen, flags,
                              (SceNetInetSockaddr *)msg->msgName, &msg->msgNamelen);
}

int sceNetInetSetsockopt(int sockFd, int level, int optname, const void *optval, u32 optlen)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (optval == NULL) {
        inetSetErrno(INET_EFAULT);
        return -1;
    }

    if (level == INET_SOL_SOCKET) {
        if (optname == INET_SO_LINGER && optlen >= sizeof(SceNetInetLinger)) {
            sceKernelMemcpy(&sock->linger, optval, sizeof(SceNetInetLinger));
        } else if (optlen >= sizeof(int)) {
            int val = *(const int *)optval;
            switch (optname) {
            case INET_SO_REUSEADDR: sock->reuseAddr = val; break;
            case INET_SO_KEEPALIVE: sock->keepAlive = val; break;
            case INET_SO_BROADCAST: sock->broadcast = val; break;
            case INET_SO_SNDBUF:    sock->sndBufSize = (u32)val; break;
            case INET_SO_RCVBUF:    sock->rcvBufSize = (u32)val; break;
            case INET_SO_SNDTIMEO:  sock->sndTimeout = (u32)val; break;
            case INET_SO_RCVTIMEO:  sock->rcvTimeout = (u32)val; break;
            case INET_SO_NBIO:      sock->nonBlocking = val; break;
            default: break;
            }
        }
    }
    inetSetErrno(0);
    return 0;
}

int sceNetInetGetsockopt(int sockFd, int level, int optname, void *optval, u32 *optlen)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (optval == NULL || optlen == NULL) {
        inetSetErrno(INET_EFAULT);
        return -1;
    }

    if (level == INET_SOL_SOCKET) {
        if (optname == INET_SO_LINGER && *optlen >= sizeof(SceNetInetLinger)) {
            sceKernelMemcpy(optval, &sock->linger, sizeof(SceNetInetLinger));
            *optlen = sizeof(SceNetInetLinger);
        } else if (*optlen >= sizeof(int)) {
            int val = 0;
            switch (optname) {
            case INET_SO_REUSEADDR: val = sock->reuseAddr; break;
            case INET_SO_KEEPALIVE: val = sock->keepAlive; break;
            case INET_SO_BROADCAST: val = sock->broadcast; break;
            case INET_SO_SNDBUF:    val = (int)sock->sndBufSize; break;
            case INET_SO_RCVBUF:    val = (int)sock->rcvBufSize; break;
            case INET_SO_SNDTIMEO:  val = (int)sock->sndTimeout; break;
            case INET_SO_RCVTIMEO:  val = (int)sock->rcvTimeout; break;
            case INET_SO_ERROR:     val = sock->lastError; sock->lastError = 0; break;
            case INET_SO_TYPE:      val = sock->type; break;
            case INET_SO_NBIO:      val = sock->nonBlocking; break;
            default: break;
            }
            *(int *)optval = val;
            *optlen = sizeof(int);
        }
    }
    inetSetErrno(0);
    return 0;
}

int sceNetInetGetsockname(int sockFd, SceNetInetSockaddr *name, u32 *namelen)
{
    InetSocket *sock;
    u32 copyLen;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (name == NULL || namelen == NULL) {
        inetSetErrno(INET_EFAULT);
        return -1;
    }

    copyLen = (*namelen < sizeof(SceNetInetSockaddrIn)) ? *namelen : (u32)sizeof(SceNetInetSockaddrIn);
    sceKernelMemcpy(name, &sock->localAddr, copyLen);
    *namelen = sizeof(SceNetInetSockaddrIn);
    inetSetErrno(0);
    return 0;
}

int sceNetInetGetpeername(int sockFd, SceNetInetSockaddr *name, u32 *namelen)
{
    InetSocket *sock;
    u32 copyLen;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (sock->state != INET_SOCK_STATE_CONNECTED) {
        inetSetErrno(INET_ENOTCONN);
        return -1;
    }
    if (name == NULL || namelen == NULL) {
        inetSetErrno(INET_EFAULT);
        return -1;
    }

    copyLen = (*namelen < sizeof(SceNetInetSockaddrIn)) ? *namelen : (u32)sizeof(SceNetInetSockaddrIn);
    sceKernelMemcpy(name, &sock->peerAddr, copyLen);
    *namelen = sizeof(SceNetInetSockaddrIn);
    inetSetErrno(0);
    return 0;
}

int sceNetInetSelect(int nfds, SceNetInetFdSet *readfds, SceNetInetFdSet *writefds,
                     SceNetInetFdSet *exceptfds, SceNetInetTimeval *timeout)
{
    int fd;
    int ready = 0;

    (void)timeout;
    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    if (nfds < 0 || nfds > INET_MAX_SOCKETS) {
        nfds = INET_MAX_SOCKETS;
    }

    for (fd = 0; fd < nfds; fd++) {
        u32 word = (u32)fd >> 5;
        u32 mask = 1U << ((u32)fd & 31U);
        InetSocket *sock = inetGetSocket(fd);

        if (readfds != NULL && (readfds->bits[word] & mask) != 0) {
            if (sock != NULL && (sock->rxCount > 0 || sock->shutRead)) {
                ready++;
            } else {
                readfds->bits[word] &= ~mask;
            }
        }
        if (writefds != NULL && (writefds->bits[word] & mask) != 0) {
            if (sock != NULL && !sock->shutWrite) {
                ready++;
            } else {
                writefds->bits[word] &= ~mask;
            }
        }
        if (exceptfds != NULL && (exceptfds->bits[word] & mask) != 0) {
            if (sock != NULL && sock->aborted) {
                ready++;
            } else {
                exceptfds->bits[word] &= ~mask;
            }
        }
    }
    inetSetErrno(0);
    return ready;
}

int sceNetInetPoll(SceNetInetPollfd *fds, u32 nfds, int timeout)
{
    u32 idx;
    int ready = 0;

    (void)timeout;
    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    if (fds == NULL && nfds > 0) {
        inetSetErrno(INET_EFAULT);
        return -1;
    }

    for (idx = 0; idx < nfds; idx++) {
        InetSocket *sock = inetGetSocket(fds[idx].fd);
        fds[idx].revents = 0;
        if (sock == NULL) {
            fds[idx].revents = INET_POLLNVAL;
            ready++;
            continue;
        }
        if ((fds[idx].events & INET_POLLIN) != 0 && (sock->rxCount > 0 || sock->shutRead)) {
            fds[idx].revents |= INET_POLLIN;
        }
        if ((fds[idx].events & INET_POLLOUT) != 0 && !sock->shutWrite) {
            fds[idx].revents |= INET_POLLOUT;
        }
        if (sock->aborted) {
            fds[idx].revents |= INET_POLLHUP;
        }
        if (fds[idx].revents != 0) {
            ready++;
        }
    }
    inetSetErrno(0);
    return ready;
}

int sceNetInetShutdown(int sockFd, int how)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4048) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    if (how == 0 || how == 2) {
        sock->shutRead = 1;
    }
    if (how == 1 || how == 2) {
        sock->shutWrite = 1;
    }
    inetSetErrno(0);
    return 0;
}

int sceNetInetClose(int sockFd)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4064) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    sock->inUse = 0;
    sock->state = INET_SOCK_STATE_CLOSED;
    inetSetErrno(0);
    return 0;
}

int sceNetInetCloseWithRST(int sockFd)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4064) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    sock->linger.onoff = 1;
    sock->linger.linger = 0;
    sock->inUse = 0;
    sock->state = INET_SOCK_STATE_CLOSED;
    inetSetErrno(0);
    return 0;
}

int sceNetInetSocketAbort(int sockFd)
{
    InetSocket *sock;

    if (_sceKernelCheckThreadStack() < 4064) {
        return -1;
    }
    sock = inetGetSocket(sockFd);
    if (sock == NULL) {
        inetSetErrno(INET_EBADF);
        return -1;
    }
    sock->aborted = 1;
    inetSetErrno(0);
    return 0;
}
