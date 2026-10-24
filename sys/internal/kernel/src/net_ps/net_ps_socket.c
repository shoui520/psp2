/* SceNetPs socket adapters; original source filename is unavailable. */
/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1993
 * The Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "net_ps_internal.h"
#include <net_ps/netps_gateway.h>
#include <net_ps/netps.h>
#include <sys/domain.h>
#include <sys/ioccom.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/uio.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <string.h>

struct mbuf *m_get(int how, int type);
void sce_psnet_panic(const char *message, ...) __attribute__((noreturn));
extern struct malloc_type *mbtypes[8];
int sceNetPsDestroyDump(unsigned int dumpId, struct SceNetPsDump *dump);
int sceNetPsDestroyEpoll(unsigned int epollId, SceNetPsEpoll *epoll);
int sceNetPsDestroyDescriptor(unsigned int descriptorId,
			     SceNetPsDescriptor *descriptor);
int sceNetPsRoutingSysctl(const int *name, unsigned int nameLength, void *oldData,
			  unsigned int *oldLength, const void *newData,
			  unsigned int newLength);

/* SCE: the gateway allocator callbacks ignore their interface argument. */
int sceNetPsGatewayFree(struct SceNetKernelGateway *interface, void *pointer)
{
	(void)interface;
	return sceNetPsFree(pointer);
}

void *sceNetPsGatewayAllocate(struct SceNetKernelGateway *interface,
			    SceSize size, SceSize alignment)
{
	(void)interface;
	return sceNetPsMalloc(size, 1, alignment);
}

extern SceNetPsGatewayState *gNetPsGatewayInterfaceSlot;

/* SCE: release the jumbo lock before the socket send may block. */
int sceNetPsGatewaySend(struct SceNetKernelGateway *interface, const void *data,
			unsigned int length, unsigned int destinationAddress)
{
	SceNetSockaddrIn address;
	int socket;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (gNetPsGatewayInterfaceSlot->interface != interface) {
		result = 0x80418108;
		goto failed;
	}
	if (!gNetPsGatewayInterfaceSlot->interface) {
		result = 0x80418107;
		goto failed;
	}
	memset(&address, 0, sizeof(address));
	address.sin_addr.s_addr = destinationAddress;
	socket = gNetPsGatewayInterfaceSlot->socket;
	address.sin_family = 2;
	address.sin_len = sizeof(address);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return SceNetPsForDriver_AB746734(socket, data, length, 0,
			     (const SceNetSockaddr *)&address, sizeof(address));
failed:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

/* SCE: notifications carry the DNS addresses with all remaining words clear. */
int sceNetPsNotifyGatewayDns(const SceNetPsDnsConfig *config)
{
	SceNetPsDnsConfig notification;

	if (gNetPsGatewayInterfaceSlot->interface && config) {
		memset(&notification, 0, sizeof(notification));
		notification.dns = config->dns;
		gNetPsGatewayInterfaceSlot->interface->notify(
			gNetPsGatewayInterfaceSlot->interface, 1,
			&notification, sizeof(notification));
	}
	return 0;
}

extern SceNetPsMutex gNetPsDevSendMutex;
extern SceNetPsMutex gNetPsDevRecvMutex;

/* Reviewed export name: Sony's gateway socket string and prototype candidate. */
int SceNetKernelGateway(struct SceNetKernelGateway *interface)
{
	int socket;
	int value;
	int result;

	if (!interface || !interface->receive || !interface->notify)
		return 0x80418101;
	socket = SceNetPsForDriver_EB95B024("SceNetKernelGateway", 2, 3, 0);
	if (socket < 0)
		return 0x80418105;
	value = 1;
	if (SceNetPsForDriver_4BF5FAB4(socket, 0, 2, &value, sizeof(value)) < 0)
		goto socketFailed;
	value = 1;
	if (SceNetPsForDriver_4BF5FAB4(socket, 0xffff, 0x1002,
			      &value, sizeof(value)) < 0)
		goto socketFailed;
	value = 1;
	if (SceNetPsForDriver_4BF5FAB4(socket, 0, 25, &value, sizeof(value)) < 0)
		goto socketFailed;
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
	if (!gNetPsGatewayInterfaceSlot) {
		result = 0x80418106;
	} else if (gNetPsGatewayInterfaceSlot->interface) {
		result = 0x80418101;
	} else {
		gNetPsGatewayInterfaceSlot->interface = interface;
		gNetPsGatewayInterfaceSlot->socket = socket;
		interface->allocate = sceNetPsGatewayAllocate;
		gNetPsGatewayInterfaceSlot->interface->free = sceNetPsGatewayFree;
		gNetPsGatewayInterfaceSlot->interface->send = sceNetPsGatewaySend;
		gNetPsGatewayInterfaceSlot->interface->portBase = 40000;
		gNetPsGatewayInterfaceSlot->interface->portCount = 9152;
		sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
		sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	goto closeSocket;
socketFailed:
	result = 0x80418106;
closeSocket:
	SceNetPsForDriver_21F4428D(socket);
	return result;
}

/* SCE: detach under all three locks, then close the socket outside them. */
int sceNetPsUnregisterGateway(struct SceNetKernelGateway *interface)
{
	int socket;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
	if (!gNetPsGatewayInterfaceSlot) {
		result = 0x80418106;
		goto failed;
	}
	if (gNetPsGatewayInterfaceSlot->interface != interface) {
		result = 0x80418101;
		goto failed;
	}
	gNetPsGatewayInterfaceSlot->interface = NULL;
	socket = gNetPsGatewayInterfaceSlot->socket;
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (socket >= 0)
		SceNetPsForDriver_21F4428D(socket);
	return 0;
failed:
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

/* SCE: references in Sony's complete socket owner guard upstream sofree. */
void bnet_sorele(struct socket *socket)
{
	if (socket) {
		if (socket->refCount <= 0)
			sce_psnet_panic("", __func__);
		if (--socket->refCount == 0)
			sofree(socket);
	}
}

/*
 * SCE adaptation of NetBSD uipc_syscalls.c recvit: the locals live in a
 * heap frame, the socket comes from the net_ps socket table, copies go
 * through sceNetPsCopyOut with the caller's kernel flag, an interrupted
 * receive clears so_error, and from and control are always freed.
 */
struct SceNetPsRecvitFrame {
	struct socket	*so;
	unsigned int	padding;
	struct uio	auio;
	struct iovec	*iov;
	struct mbuf	*from;
	struct mbuf	*control;
	int		i;
	int		len;
	struct mbuf	*m;
	caddr_t		p;
};

_Static_assert(sizeof(struct SceNetPsRecvitFrame) == 0x48,
	       "SceNetPsRecvitFrame size");

int sceNetPsRecvit(int s, struct msghdr *mp, void *namelenp, int *retsize,
		   int kernel)
{
	struct SceNetPsRecvitFrame *f;
	int error = 0;

	f = sceNetPsMalloc(sizeof(*f), 3, 8);
	if (f == NULL)
		return -ENOMEM;
	f->from = NULL;
	f->control = NULL;
	f->so = sceNetPsLookupSocket(s, &error, 0);
	if (f->so == NULL)
		goto out;
	f->auio.uio_iov = mp->msg_iov;
	f->auio.uio_iovcnt = mp->msg_iovlen;
	f->auio.uio_segflg = UIO_USERSPACE;
	f->auio.uio_rw = UIO_READ;
	f->auio.uio_procp = curproc;
	f->auio.uio_offset = 0;			/* XXX */
	f->auio.uio_resid = 0;
	f->iov = mp->msg_iov;
	for (f->i = 0; f->i < mp->msg_iovlen; f->i++, f->iov++) {
		f->auio.uio_resid += f->iov->iov_len;
		if (f->iov->iov_len > SSIZE_MAX ||
		    f->auio.uio_resid > SSIZE_MAX) {
			error = EINVAL;
			goto out;
		}
	}
	f->len = f->auio.uio_resid;
	bnet_socket_sanity_check(f->so);
	error = (*f->so->so_receive)(f->so, &f->from, &f->auio, NULL,
				     mp->msg_control ? &f->control : NULL,
				     &mp->msg_flags);
	if (error) {
		if (f->auio.uio_resid != f->len &&
		    (error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
	if (error) {
		/* SCE: an interrupted receive leaves no socket error behind. */
		if (error == EINTR)
			f->so->so_error = 0;
		goto out;
	}
	*retsize = f->len - f->auio.uio_resid;
	if (mp->msg_name) {
		f->len = mp->msg_namelen;
		if (f->len <= 0 || f->from == NULL) {
			f->len = 0;
		} else {
			if (f->len > f->from->m_len)
				f->len = f->from->m_len;
			error = sceNetPsCopyOut(mtod(f->from, caddr_t),
						mp->msg_name, f->len, kernel);
			if (error)
				goto out;
		}
		mp->msg_namelen = f->len;
		if (namelenp &&
		    (error = sceNetPsCopyOut(&f->len, namelenp, sizeof(int),
					     kernel)))
			goto out;
	}
	if (mp->msg_control) {
		f->len = mp->msg_controllen;
		if (f->len <= 0 || f->control == NULL) {
			f->len = 0;
		} else {
			f->m = f->control;
			f->p = mp->msg_control;
			do {
				f->i = f->m->m_len;
				if (f->len < f->i) {
					mp->msg_flags |= MSG_CTRUNC;
					f->i = f->len;
				}
				error = sceNetPsCopyOut(mtod(f->m, caddr_t),
							f->p, f->i, kernel);
				if (f->m->m_next)
					f->i = ALIGN(f->i);
				f->p += f->i;
				f->len -= f->i;
				if (error != 0 || f->len <= 0)
					break;
			} while ((f->m = f->m->m_next) != NULL);
			f->len = f->p - (caddr_t)mp->msg_control;
		}
		mp->msg_controllen = f->len;
	}
out:
	m_freem(f->from);
	m_freem(f->control);
	sceNetPsFree(f);
	return error;
}

/*
 * SCE adaptation of NetBSD uipc_syscalls.c 1.90.2.4 sockargs:
 * copy selector, platform external allocation, M_EXT without M_EXT_RW,
 * no upstream length rejection, and m_freem on copy failure.
 */
int sceNetPsSockargs(struct mbuf **result, const void *buffer,
		     unsigned int length, int type, int kernel)
{
	struct sockaddr *address;
	struct mbuf *mbuf;
	int error;

	mbuf = m_get(0, type);
	if (length > MLEN) {
		mbuf->m_ext.ext_buf = sceNetPsMalloc(length, 0, 8);
		if (mbuf->m_ext.ext_buf != NULL) {
			mbuf->m_data = mbuf->m_ext.ext_buf;
			mbuf->m_ext.ext_size = length;
			mbuf->m_ext.ext_free = NULL;
			mbuf->m_ext.ext_arg = NULL;
			mbuf->m_flags = (mbuf->m_flags & ~M_EXTCOPYFLAGS) | M_EXT;
			mbuf->m_ext.ext_type = mbtypes[mbuf->m_type];
			mbuf->m_ext.ext_nextref = mbuf;
			mbuf->m_ext.ext_prevref = mbuf;
		}
	}
	mbuf->m_len = length;
	error = sceNetPsCopyIn(buffer, mbuf->m_data, length, kernel);
	if (error) {
		m_freem(mbuf);
		return error;
	}
	*result = mbuf;
	if (type == MT_SONAME) {
		address = mtod(mbuf, struct sockaddr *);
		if (address->sa_family == 0 && address->sa_len < AF_MAX)
			address->sa_family = address->sa_len;
		address->sa_len = length;
	}
	return error;
}

static int sceNetPsConnectPending(void *argument, int cancel);

/* SCE: the errors after which a connect waits for an automatic connection */
static inline int sceNetPsIsUnreachable(int error)
{
	return error == EADDRNOTAVAIL || error == ENETUNREACH ||
	       error == EHOSTUNREACH;
}

/*
 * SCE adaptation of NetBSD uipc_syscalls.c sys_connect: a pending
 * returned error is reported first, the wait is a timed sleep on the send
 * buffer, and an unreachable destination asks for the interface's
 * automatic connection, then retries once or, non-blocking, queues the
 * connect until the connection is up.  Returns the socket's returned error;
 * the errno goes to *errorp.
 */
int sceNetPsConnectSocket(int s, struct socket *so, const void *address,
			  unsigned int addressLength, int *errorp, int kernel)
{
	SceNetPsConnectRequest *request;
	SceUInt64 start, now;
	struct mbuf *nam;
	int result, error, retry, delay;

	so->refCount++;
	result = so->returnedError;
	if (result) {
		error = result;
		so->returnedError = 0;
		result = 0;
		goto out;
	}
	if (address == NULL || addressLength < sizeof(struct sockaddr)) {
		error = EINVAL;
		goto out;
	}
	if (sceNetPsIsRetriedSyscall() && (so->so_state & SS_ISCONNECTED)) {
		error = result;
		goto out;
	}
	if ((so->so_state & (SS_NBIO | SS_ISCONNECTING)) == SS_ISCONNECTING) {
		error = EALREADY;
		goto out;
	}
	if (addressLength > 255) {
		error = EINVAL;
		goto out;
	}
	error = sceNetPsSockargs(&nam, address, addressLength, MT_SONAME,
				 kernel);
	result = error;
	if (error)
		goto out;
	retry = 1;
again:
	result = soconnect(so, nam);
	if (result) {
		if (sceNetPsIsUnreachable(result))
			goto autoconnect;
		error = result;
		if ((so->so_state & (SS_NBIO | SS_ISCONNECTED)) == SS_NBIO)
			error = EINPROGRESS;
		result = 0;
		goto bad;
	}
	if ((so->so_state & (SS_NBIO | SS_ISCONNECTING)) ==
	    (SS_NBIO | SS_ISCONNECTING)) {
		m_freem(nam);
		error = EINPROGRESS;
		goto out;
	}
	sceNetPsGetSystemTime(&start);
	while (so->so_state & SS_ISCONNECTING) {
		if (so->so_error) {
			result = so->so_error;
			goto clear;
		}
		sceNetPsGetSystemTime(&now);
		delay = (int)(start - now) + so->so_snd.sb_timeo;
		if (so->so_snd.sb_timeo > 0 && delay <= 0) {
			result = 0;
			error = EWOULDBLOCK;
			goto out;
		}
		so->so_snd.sb_flags |= SB_WAIT;
		result = sceNetPsSleep(&so->so_snd.sb_cc, 0, NULL, delay);
		so->so_snd.sb_flags &= ~SB_WAIT;
		if (result)
			goto check;
	}
	result = so->so_error;
clear:
	so->so_error = 0;
check:
	if (!sceNetPsIsUnreachable(result)) {
		error = result;
		result = 0;
		goto bad;
	}
autoconnect:
	if (!retry || !sceNetPsRequestAutoConnect(so, nam, 0)) {
		error = result;
		result = 0;
		goto done;
	}
	if (!(so->so_state & SS_NBIO)) {
		result = sceNetPsWaitAutoConnect(so);
		if (result) {
			error = result;
			goto bad;
		}
		retry = 0;
		if (so->so_state & SS_ISCONNECTING)
			so->so_state &= ~SS_ISCONNECTING;
		goto again;
	}
	request = sceNetPsMalloc(sizeof(*request), 3, 8);
	if (request == NULL) {
		result = 0;
		error = ENOMEM;
		goto done;
	}
	request->param.socket = s;
	request->request.complete = sceNetPsConnectPending;
	request->request.argument = &request->param;
	result = sceNetPsCopyIn(address, &request->param.address,
				sizeof(request->param.address), kernel);
	if (result) {
		error = result;
		sceNetPsFree(request);
		result = 0;
		goto bad;
	}
	request->param.addressLength = addressLength;
	sceNetPsQueuePendingRequest(&request->request);
	error = EINPROGRESS;
	so->so_state |= SS_SCE_AUTOCONNECT;
	goto done;
bad:
	/* SCE: a cancelled wait leaves the socket connecting. */
	if (error == ECANCELED)
		goto free;
done:
	so->so_state &= ~SS_ISCONNECTING;
free:
	m_freem(nam);
out:
	bnet_sorele(so);
	*errorp = error;
	return result;
}

/* SCE: the queued connect, run when the automatic connection is up */
static int sceNetPsConnectPending(void *argument, int cancel)
{
	SceNetPsConnectParam *param = argument;
	struct socket *so;
	int error = 0;

	if (cancel)
		return 0;
	so = sceNetPsLookupSocket(param->socket, &error, 1);
	if (so == NULL)
		return error;
	if (!(so->so_state & SS_NBIO)) {
		so->so_state |= SS_NBIO;
		sceNetPsConnectSocket(param->socket, so, &param->address,
				      param->addressLength, &error, 1);
		so->so_state &= ~SS_NBIO;
		return error;
	}
	sceNetPsConnectSocket(param->socket, so, &param->address,
			      param->addressLength, &error, 1);
	return error;
}

static int sceNetPsSendPending(void *argument, int cancel);

/*
 * SCE adaptation of NetBSD uipc_syscalls.c sendit: the locals live in a
 * heap frame with a copy of the iovecs, so that after an unreachable
 * destination (except on a routing socket) the send can be retried once the
 * interface's automatic connection is up, or, non-blocking, queued with its
 * data copied.  A pending returned error is reported first.
 */
struct SceNetPsSenditFrame {
	struct uio	auio;
	struct iovec	*iov;
	struct mbuf	*to;
	struct mbuf	*control;
	int		i;
	int		len;
	struct iovec	savedIov[32];
	int		retries;
};

_Static_assert(sizeof(struct SceNetPsSenditFrame) == 0x138,
	       "SceNetPsSenditFrame size");

int sceNetPsSendit(int s, struct socket *so, struct msghdr *mp, int flags,
		   int *retsize, int kernel)
{
	struct SceNetPsSenditFrame *f;
	SceNetPsSendRequest *request;
	char *buffer, *p;
	int error = 0;
	int len, i;

	f = sceNetPsMalloc(sizeof(*f), 3, 8);
	if (f == NULL)
		return -ENOMEM;
	f->to = NULL;
	f->retries = 1;
	for (;;) {
		if (so == NULL) {
			so = sceNetPsLookupSocket(s, &error, 0);
			if (so == NULL)
				goto out;
		}
		if (so->returnedError) {
			error = so->returnedError;
			so->returnedError = 0;
			goto bad;
		}
		f->auio.uio_iov = mp->msg_iov;
		f->auio.uio_iovcnt = mp->msg_iovlen;
		f->auio.uio_segflg = kernel ? UIO_SYSSPACE : UIO_USERSPACE;
		f->auio.uio_rw = UIO_WRITE;
		f->auio.uio_procp = curproc;
		f->auio.uio_offset = 0;		/* XXX */
		f->auio.uio_resid = 0;
		f->iov = mp->msg_iov;
		for (f->i = 0; f->i < mp->msg_iovlen; f->i++, f->iov++) {
			f->auio.uio_resid += f->iov->iov_len;
			if (f->iov->iov_len > SSIZE_MAX ||
			    f->auio.uio_resid > SSIZE_MAX) {
				error = EINVAL;
				goto bad;
			}
			f->savedIov[f->i] = *f->iov;
		}
		if (mp->msg_name) {
			if (mp->msg_namelen > 255) {
				error = EINVAL;
				goto bad;
			}
			error = sceNetPsSockargs(&f->to, mp->msg_name,
						 mp->msg_namelen, MT_SONAME,
						 kernel);
			if (error)
				goto bad;
		}
		if (mp->msg_control) {
			if (mp->msg_controllen < sizeof(struct cmsghdr) ||
			    mp->msg_controllen > 4096) {
				error = EINVAL;
				goto bad;
			}
			error = sceNetPsSockargs(&f->control, mp->msg_control,
						 mp->msg_controllen, MT_CONTROL,
						 kernel);
			if (error)
				goto bad;
		}
		f->len = f->auio.uio_resid;
		bnet_socket_sanity_check(so);
		error = (*so->so_send)(so, f->to, &f->auio, NULL, f->control,
				       flags);
		if (error) {
			if (f->auio.uio_resid == f->len ||
			    (error != EINTR && error != EWOULDBLOCK)) {
				/* SCE: an interrupted send leaves no socket error behind. */
				if (error == EINTR)
					so->so_error = 0;
				goto bad;
			}
			error = 0;
		}
		*retsize = f->len - f->auio.uio_resid;
bad:
		if (so->so_proto->pr_domain->dom_family == PF_ROUTE)
			goto out;
		if ((error < EADDRNOTAVAIL || error > ENETUNREACH) &&
		    error != EHOSTUNREACH)
			goto out;
		if (f->retries <= 0)
			goto out;
		f->retries--;
		if (!sceNetPsRequestAutoConnect(so, f->to, 0))
			goto out;
		f->iov = mp->msg_iov;
		for (f->i = 0; f->i < mp->msg_iovlen; f->i++, f->iov++)
			*f->iov = f->savedIov[f->i];
		len = f->len;
		if (so->so_state & SS_NBIO)
			break;
		error = sceNetPsWaitAutoConnect(so);
		*retsize = error;
		if (error)
			goto out;
		so = NULL;
		m_freem(f->to);
		f->to = NULL;
	}
	request = sceNetPsMalloc(sizeof(*request), 3, 8);
	if (request == NULL) {
		error = ENOMEM;
		goto out;
	}
	buffer = sceNetPsMalloc(mp->msg_namelen + len, 1, 8);
	if (buffer == NULL) {
		sceNetPsFree(request);
		error = ENOMEM;
		goto out;
	}
	request->request.argument = &request->param;
	request->request.complete = sceNetPsSendPending;
	request->param.socket = s;
	request->param.message.msg_name = buffer + len;
	error = sceNetPsCopyIn(mp->msg_name, buffer + len, mp->msg_namelen,
			       kernel);
	if (error)
		goto free;
	request->param.message.msg_namelen = mp->msg_namelen;
	request->param.message.msg_iov = &request->param.iov;
	request->param.message.msg_iovlen = 1;
	for (i = 0, p = buffer; i < mp->msg_iovlen; i++) {
		error = sceNetPsCopyIn(mp->msg_iov[i].iov_base, p,
				       mp->msg_iov[i].iov_len, kernel);
		if (error)
			goto free;
		p += mp->msg_iov[i].iov_len;
	}
	request->param.iov.iov_base = buffer;
	request->param.iov.iov_len = len;
	request->param.message.msg_control = NULL;
	request->param.message.msg_controllen = 0;
	request->param.flags = flags;
	sceNetPsQueuePendingRequest(&request->request);
	error = 0;
	*retsize = len;
	goto out;
free:
	sceNetPsFree(buffer);
	sceNetPsFree(request);
out:
	m_freem(f->to);
	sceNetPsFree(f);
	return error;
}

/* SCE: the queued send, run when the automatic connection is up */
static int sceNetPsSendPending(void *argument, int cancel)
{
	SceNetPsSendParam *param = argument;
	struct socket *so;
	void *buffer;
	int retsize = 0;
	int error = 0;

	if (cancel) {
		sceNetPsFree(param->iov.iov_base);
		return 0;
	}
	so = sceNetPsLookupSocket(param->socket, &error, 1);
	if (so == NULL)
		return error;
	so->refCount++;
	buffer = param->iov.iov_base;
	error = sceNetPsSendit(param->socket, so, &param->message, param->flags,
			       &retsize, 1);
	sceNetPsFree(buffer);
	bnet_sorele(so);
	return error;
}

/*
 * SCE: drop the socket from the epolls and descriptors that name it and,
 * unless aborting, close it; returns the registrations that remain.
 */
int sceNetPsCloseSocket(struct socket *so, int *errorp, int abort)
{
	int references = so->epollReferences;

	if (references)
		references = sceNetPsRemoveSocketFromEpolls(so, abort);
	if (so->descriptorReferences)
		references += sceNetPsRemoveSocketFromDescriptors(so, abort);
	if (!abort) {
		so->refCount--;
		*errorp = soclose(so);
	}
	return references;
}

/* SCE: close a socket by its identifier and forget everything kept for it */
int sceNetPsDestroySocket(unsigned int socketId)
{
	struct socket *so;
	int error;

	so = sceNetPsLookupSocketUnchecked(socketId, &error);
	if (so) {
		sceNetPsCloseSocket(so, &error, 0);
		sceNetPsSetSocket(socketId, NULL);
	}
	sceNetPsSetSocketResume(socketId, 0);
	sceNetPsSetSocketAddressChanged(socketId, 0);
	sceNetPsRemoveSocketIdFromEpolls(socketId);
	sceNetPsRemoveSocketIdFromDescriptors(socketId);
	sceNetPsConsumeWakeup((const void *)socketId, 0);
	return 0;
}

/*
 * SCE: wake the socket's waiters with the abort, then reset it on close
 * (zero linger) with SCE_NET_EIPADDRCHANGED as its error.
 */
int sceNetPsAbortSocket(struct socket *so, int addressChanged)
{
	int error;

	soabortwakeup(so, &so->so_snd, 2, addressChanged);
	soabortwakeup(so, &so->so_rcv, 1, addressChanged);
	so->so_error = SCE_NET_EIPADDRCHANGED;
	so->so_linger = 0;
	so->so_options |= SO_LINGER;
	sceNetPsCloseSocket(so, &error, 1);
	return 0;
}

int sceNetPsCollectSockInfo(int socket, SceNetSockInfo *records, int capacity,
			 int flags, int *count, SceUID processId,
			 int kernel);

unsigned int sceNetPsGetSockinfo(int socket, void *buffer, int capacity, int flags)
{
	int count = 0;
	int result, records;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags & ~0x23) {
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -22;
	}
	result = sceNetPsCollectSockInfo(socket, buffer, capacity, flags,
					&count, 0, 0);
	records = count;
	if (result < 0) {
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return result;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (!result)
		return records;
	return -result;
}

/* SCE: release the stack lock; a negative error passes through as is */
static inline int sceNetPsUnlockResult(int error, int result)
{
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return result;
}

/*
 * SCE: wake the socket's waiters with EINTR; with nothing to wake and
 * wakeup events given, queue those for its next wait instead.
 */
int sceNetPsSocketAbort(int socket, int flags)
{
	struct socket *so;
	int error = 0;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags & ~3) {
		error = EINVAL;
		return sceNetPsUnlockResult(error, 0);
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so) {
		/* SCE: nothing was waiting */
		error = ENOTBLK;
		if (sb_notify(&so->so_rcv)) {
			soabortwakeup(so, &so->so_rcv, 1, 0);
			error = 0;
		}
		if (sb_notify(&so->so_snd)) {
			soabortwakeup(so, &so->so_snd, 2, 0);
			error = 0;
		}
		if (sceNetPsWakeupInterrupted(&so->so_timeo) == 0)
			error = 0;
		if (sceNetPsInterruptEmulationWait(so) == 0)
			error = 0;
		so->so_error = EINTR;
		if (error == ENOTBLK && flags)
			error = sceNetPsQueueWakeup((const void *)socket, flags);
	}
	return sceNetPsUnlockResult(error, 0);
}

/* SCE adaptation of NetBSD uipc_syscalls.c sys_socket */
int sceNetPsSocket(const char *name, int domain, int type, int protocol,
		   int kernel)
{
	struct socket *so = NULL;
	size_t copied;
	int socketId, error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (name == NULL)
		return sceNetPsUnlockResult(EINVAL, 0);
	socketId = sceNetPsAllocateSocketId();
	if (socketId < 0)
		return sceNetPsUnlockResult(EMFILE, 0);
	error = socreate(domain, &so, type, protocol);
	if (error)
		return sceNetPsUnlockResult(error, 0);
	so->refCount = 1;
	sceNetPsCopyString(name, so->name, sizeof(so->name) - 1, &copied,
			   kernel);
	sceNetPsSetSocket(socketId, so);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return socketId;
}

/* SCE adaptation of NetBSD uipc_syscalls.c sys_bind */
int sceNetPsBind(int socket, const SceNetSockaddr *address,
		 unsigned int addressLength, int kernel)
{
	struct socket *so;
	struct mbuf *nam = NULL;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (address == NULL || addressLength < sizeof(struct sockaddr)) {
		so = NULL;
		error = EINVAL;
		goto out;
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so == NULL)
		goto out;
	so->refCount++;
	if (addressLength > 255) {
		error = EINVAL;
		goto out;
	}
	error = sceNetPsSockargs(&nam, address, addressLength, MT_SONAME,
				 kernel);
	if (error == 0)
		error = sobind(so, nam, curproc);
out:
	m_freem(nam);
	bnet_sorele(so);
	return sceNetPsUnlockResult(error, 0);
}

/* SCE adaptation of NetBSD uipc_syscalls.c sys_listen */
int sceNetPsListen(int socket, int backlog, int kernel)
{
	struct socket *so;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (backlog <= 0) {
		error = EINVAL;
		return sceNetPsUnlockResult(error, 0);
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so)
		error = solisten(so, backlog);
	return sceNetPsUnlockResult(error, 0);
}

/*
 * SCE adaptation of NetBSD uipc_syscalls.c sys_accept: an abort queued
 * for the socket ends it first, and the wait is a timed sleep on the
 * receive buffer.
 */
int sceNetPsAccept(int socket, SceNetSockaddr *address,
		   unsigned int *addressLength, int kernel)
{
	struct socket *so = NULL;
	struct socket *so2;
	struct mbuf *nam = NULL;
	SceUInt64 start, now;
	int namelen = 0;
	int result = 0;
	int error, delay;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (sceNetPsConsumeWakeup((const void *)socket, 1)) {
		error = EINTR;
		goto out;
	}
	if ((address == NULL && addressLength != NULL) ||
	    (address != NULL && addressLength == NULL)) {
		error = EINVAL;
		goto out;
	}
	if (addressLength) {
		error = sceNetPsCopyIn(addressLength, &namelen, sizeof(namelen),
				       kernel);
		if (error)
			goto out;
		if (namelen < sizeof(struct sockaddr)) {
			error = EINVAL;
			goto out;
		}
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so == NULL)
		goto out;
	so->refCount++;
	if (!(so->so_proto->pr_flags & PR_LISTEN)) {
		error = EOPNOTSUPP;
		goto out;
	}
	if (!(so->so_options & SO_ACCEPTCONN)) {
		error = EINVAL;
		goto out;
	}
	if ((so->so_state & SS_NBIO) && so->so_qlen == 0) {
		error = EWOULDBLOCK;
		goto out;
	}
	sceNetPsGetSystemTime(&start);
	while (so->so_qlen == 0) {
		if (so->so_error) {
			error = so->so_error;
			goto clear;
		}
		sceNetPsGetSystemTime(&now);
		if (so->so_state & SS_CANTRCVMORE) {
			error = ECONNABORTED;
			goto clear;
		}
		delay = (int)(start - now) + so->so_rcv.sb_timeo;
		if (so->so_rcv.sb_timeo > 0 && delay <= 0) {
			error = EWOULDBLOCK;
			goto out;
		}
		so->so_rcv.sb_flags |= SB_WAIT;
		error = sceNetPsSleep(&so->so_rcv.sb_cc, 0, NULL, delay);
		so->so_rcv.sb_flags &= ~SB_WAIT;
		if (error)
			goto out;
	}
	if (so->so_error) {
		error = so->so_error;
clear:
		so->so_error = 0;
		goto out;
	}
	so2 = TAILQ_FIRST(&so->so_q);
	if (soqremque(so2, 1) == 0)
		sce_psnet_panic("");
	nam = m_get(M_WAIT, MT_SONAME);
	error = soaccept(so2, nam);
	if (error)
		goto out;
	if (address) {
		if (namelen > nam->m_len)
			namelen = nam->m_len;
		error = sceNetPsCopyOut(mtod(nam, caddr_t), address, namelen,
					kernel);
		if (error)
			goto out;
		error = sceNetPsCopyOut(&namelen, addressLength,
					sizeof(namelen), kernel);
		if (error)
			goto out;
	}
	so2->refCount = 1;
	result = sceNetPsAllocateSocketId();
	if (result < 0) {
		so2->refCount--;
		soclose(so2);
		error = EMFILE;
		goto out;
	}
	sceNetPsSetSocket(result, so2);
out:
	m_freem(nam);
	bnet_sorele(so);
	return sceNetPsUnlockResult(error, result);
}

/* SCE: connect, unless an abort is queued for the socket */
int sceNetPsConnect(int socket, const SceNetSockaddr *address,
		    unsigned int addressLength, int kernel)
{
	struct socket *so;
	int error = 0;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (sceNetPsConsumeWakeup((const void *)socket, 2)) {
		error = EINTR;
		return sceNetPsUnlockResult(error, 0);
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so)
		result = sceNetPsConnectSocket(socket, so, address,
					       addressLength, &error, kernel);
	else
		result = 0;
	return sceNetPsUnlockResult(error, result);
}

/*
 * SCE: ask for the socket's interface to connect and, unless MSG_DONTWAIT,
 * wait for it; with no connection to make, mark the socket's automatic
 * connection ended.  A cancelled wait leaves the socket error in place.
 */
int sceNetPsIcmConnect(int socket, int flags)
{
	struct socket *so;
	int error = 0;
	int result = 0;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so == NULL)
		return sceNetPsUnlockResult(error, result);
	if (sceNetPsRequestAutoConnect(so, NULL, 1)) {
		if (!(flags & MSG_DONTWAIT)) {
			result = sceNetPsWaitAutoConnect(so);
			if (result)
				error = result;
		}
	} else {
		so->so_snd.sb_flags |= SB_SCE_CONNECTED;
		sceNetPsWakeupEpolls(so);
	}
	if (error == ECANCELED) {
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -ECANCELED;
	}
	so->so_error = 0;
	return sceNetPsUnlockResult(error, result);
}

/* SCE: the iovecs a message may carry */
#define NET_PS_IOV_MAX	32

/* SCE: sendmsg's locals, in a heap frame */
struct SceNetPsSendmsgFrame {
	struct msghdr	msg;
	struct iovec	aiov[UIO_SMALLIOV];
	struct iovec	*iov;
	struct socket	*so;
};

_Static_assert(sizeof(struct SceNetPsSendmsgFrame) == 0x64,
	       "SceNetPsSendmsgFrame size");

/*
 * SCE adaptation of NetBSD uipc_syscalls.c sys_sendmsg, for user
 * messages: an abort queued for the socket ends it first, and a sent
 * message records the calling process as a sender.
 */
int sceNetPsSendmsg(int socket, const SceNetMsghdr *message, int flags)
{
	struct SceNetPsSendmsgFrame *f;
	int retsize = 0;
	int error;

	f = sceNetPsMalloc(sizeof(*f), 3, 8);
	if (f == NULL)
		return -ENOMEM;
	f->iov = NULL;
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (sceNetPsConsumeWakeup((const void *)socket, 2)) {
		error = EINTR;
		goto out;
	}
	if (message == NULL ||
	    (flags & ~(MSG_DONTWAIT | MSG_USECRYPTO | MSG_USESIGNATURE))) {
		error = EINVAL;
		goto out;
	}
	error = copyin(message, &f->msg, sizeof(f->msg));
	if (error)
		goto out;
	f->so = sceNetPsLookupSocket(socket, &error, 0);
	if (f->so == NULL)
		goto out;
	f->so->refCount++;
	if ((unsigned int)f->msg.msg_iovlen <= UIO_SMALLIOV) {
		f->iov = f->aiov;
	} else if ((unsigned int)f->msg.msg_iovlen > NET_PS_IOV_MAX) {
		error = EMSGSIZE;
		goto out;
	} else {
		f->iov = sceNetPsMalloc(sizeof(struct iovec) *
					f->msg.msg_iovlen, 0, 8);
		if (f->iov == NULL) {
			error = ENOMEM;
			goto out;
		}
	}
	if (f->msg.msg_iovlen &&
	    (error = copyin(f->msg.msg_iov, f->iov,
			    f->msg.msg_iovlen * sizeof(struct iovec))))
		goto out;
	f->msg.msg_iov = f->iov;
	error = sceNetPsSendit(socket, NULL, &f->msg, flags, &retsize, 0);
	if (error == 0)
		sceNetPsRecordSender(sceNetPsGetProcessId());
out:
	if (f->iov != f->aiov)
		sceNetPsFree(f->iov);
	bnet_sorele(f->so);
	sceNetPsFree(f);
	return sceNetPsUnlockResult(error, retsize);
}

/* SCE: sendto's and recvfrom's locals, in a heap frame */
struct SceNetPsMessageFrame {
	struct msghdr	msg;
	struct iovec	aiov;
	struct socket	*so;
};

_Static_assert(sizeof(struct SceNetPsMessageFrame) == 0x28,
	       "SceNetPsMessageFrame size");

/* SCE adaptation of NetBSD uipc_syscalls.c sys_sendto, as sendmsg */
int sceNetPsSendto(int socket, const void *buffer, unsigned int length,
		   int flags, const SceNetSockaddr *to, unsigned int toLength,
		   int kernel)
{
	struct SceNetPsMessageFrame *f;
	int retsize = 0;
	int error;

	f = sceNetPsMalloc(sizeof(*f), 3, 8);
	if (f == NULL)
		return -ENOMEM;
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (sceNetPsConsumeWakeup((const void *)socket, 2)) {
		error = EINTR;
		goto out;
	}
	if (buffer == NULL ||
	    (flags & ~(MSG_DONTWAIT | MSG_USECRYPTO | MSG_USESIGNATURE))) {
		error = EINVAL;
		goto out;
	}
	f->so = sceNetPsLookupSocket(socket, &error, 0);
	if (f->so == NULL)
		goto out;
	f->so->refCount++;
	f->msg.msg_name = (void *)to;
	f->msg.msg_namelen = toLength;
	f->msg.msg_iov = &f->aiov;
	f->msg.msg_iovlen = 1;
	f->msg.msg_control = NULL;
	f->msg.msg_flags = 0;
	f->aiov.iov_base = (void *)buffer;
	f->aiov.iov_len = length;
	error = sceNetPsSendit(socket, NULL, &f->msg, flags, &retsize, kernel);
	if (error == 0)
		sceNetPsRecordSender(sceNetPsGetProcessId());
out:
	bnet_sorele(f->so);
	sceNetPsFree(f);
	return sceNetPsUnlockResult(error, retsize);
}

/* SCE: recvmsg's locals, in a heap frame */
struct SceNetPsRecvmsgFrame {
	struct msghdr	msg;
	struct iovec	aiov[UIO_SMALLIOV];
	struct iovec	*uiov;
	struct iovec	*iov;
	struct socket	*so;
};

_Static_assert(sizeof(struct SceNetPsRecvmsgFrame) == 0x68,
	       "SceNetPsRecvmsgFrame size");

/* SCE adaptation of NetBSD uipc_syscalls.c sys_recvmsg, as sendmsg */
int sceNetPsRecvmsg(int socket, SceNetMsghdr *message, int flags)
{
	struct SceNetPsRecvmsgFrame *f;
	int retsize = 0;
	int error;

	f = sceNetPsMalloc(sizeof(*f), 3, 8);
	if (f == NULL)
		return -ENOMEM;
	f->iov = NULL;
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (sceNetPsConsumeWakeup((const void *)socket, 1)) {
		error = EINTR;
		goto out;
	}
	if (message == NULL ||
	    (flags & ~(MSG_PEEK | MSG_WAITALL | MSG_DONTWAIT | MSG_PEEKLEN))) {
		error = EINVAL;
		goto out;
	}
	error = copyin(message, &f->msg, sizeof(f->msg));
	if (error)
		goto out;
	f->so = sceNetPsLookupSocket(socket, &error, 0);
	if (f->so == NULL)
		goto out;
	f->so->refCount++;
	if ((unsigned int)f->msg.msg_iovlen <= UIO_SMALLIOV) {
		f->iov = f->aiov;
	} else if ((unsigned int)f->msg.msg_iovlen > NET_PS_IOV_MAX) {
		error = EMSGSIZE;
		goto out;
	} else {
		f->iov = sceNetPsMalloc(sizeof(struct iovec) *
					f->msg.msg_iovlen, 0, 8);
	}
	if (f->msg.msg_iovlen &&
	    (error = copyin(f->msg.msg_iov, f->iov,
			    f->msg.msg_iovlen * sizeof(struct iovec))))
		goto out;
	f->uiov = f->msg.msg_iov;
	f->msg.msg_iov = f->iov;
	f->msg.msg_flags = flags;
	error = sceNetPsRecvit(socket, &f->msg, NULL, &retsize, 0);
	if (error == 0) {
		sceNetPsRecordSender(sceNetPsGetProcessId());
		f->msg.msg_iov = f->uiov;
		error = copyout(&f->msg, message, sizeof(f->msg));
	}
out:
	if (f->iov != f->aiov)
		sceNetPsFree(f->iov);
	bnet_sorele(f->so);
	sceNetPsFree(f);
	return sceNetPsUnlockResult(error, retsize);
}

/*
 * SCE adaptation of NetBSD uipc_syscalls.c sys_recvfrom, as sendmsg; a
 * receive without a buffer takes MSG_PEEKLEN.
 */
int sceNetPsRecvfrom(int socket, void *buffer, unsigned int length, int flags,
		     SceNetSockaddr *from, unsigned int *fromLength, int kernel)
{
	struct SceNetPsMessageFrame *f;
	int retsize = 0;
	int error;

	f = sceNetPsMalloc(sizeof(*f), 3, 8);
	if (f == NULL)
		return -ENOMEM;
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (sceNetPsConsumeWakeup((const void *)socket, 1)) {
		error = EINTR;
		goto out;
	}
	if (length == 0 || (buffer == NULL && !(flags & MSG_PEEKLEN)) ||
	    (from == NULL && fromLength != NULL) ||
	    (from != NULL && fromLength == NULL) ||
	    (flags & ~(MSG_PEEK | MSG_WAITALL | MSG_DONTWAIT | MSG_PEEKLEN))) {
		error = EINVAL;
		goto out;
	}
	f->msg.msg_name = from;
	if (fromLength) {
		error = sceNetPsCopyIn(fromLength, &f->msg.msg_namelen,
				       sizeof(f->msg.msg_namelen), kernel);
		if (error)
			goto out;
	} else {
		f->msg.msg_namelen = 0;
	}
	f->so = sceNetPsLookupSocket(socket, &error, 0);
	if (f->so == NULL)
		goto out;
	f->so->refCount++;
	f->msg.msg_flags = flags;
	f->aiov.iov_base = buffer;
	f->msg.msg_iov = &f->aiov;
	f->msg.msg_iovlen = 1;
	f->aiov.iov_len = length;
	f->msg.msg_control = NULL;
	error = sceNetPsRecvit(socket, &f->msg, fromLength, &retsize, kernel);
	if (error == 0)
		sceNetPsRecordSender(sceNetPsGetProcessId());
out:
	bnet_sorele(f->so);
	sceNetPsFree(f);
	return sceNetPsUnlockResult(error, retsize);
}

/* SCE adaptation of NetBSD uipc_syscalls.c sys_shutdown */
int sceNetPsShutdown(int socket, int how, int kernel)
{
	struct socket *so;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so)
		error = soshutdown(so, how);
	return sceNetPsUnlockResult(error, 0);
}

/*
 * SCE adaptation of NetBSD uipc_syscalls.c sys_setsockopt: a pending
 * returned error is reported first, a one-int multicast TTL or loop
 * option is passed as its first byte, and after an unreachable
 * destination the option is set again once the interface's automatic
 * connection is up.
 */
int sceNetPsSetsockopt(int socket, int level, int option, const void *value,
		       unsigned int length, int kernel)
{
	struct socket *so;
	struct mbuf *m = NULL;
	int retry = 1;
	int error, result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (value == NULL) {
		so = NULL;
		error = EINVAL;
		goto check;
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so == NULL)
		goto check;
	so->refCount++;
	if (so->returnedError) {
		error = so->returnedError;
		so->returnedError = 0;
		goto check;
	}
	if (length > MCLBYTES) {
		error = EINVAL;
		goto check;
	}
	for (;;) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (length > MLEN)
			m_clget(m, M_WAIT);
		error = sceNetPsCopyIn(value, mtod(m, caddr_t), length, kernel);
		if (error == 0) {
			m->m_len = length;
			if (level == IPPROTO_IP &&
			    (option == IP_MULTICAST_TTL ||
			     option == IP_MULTICAST_LOOP) &&
			    length == sizeof(int))
				m->m_len = 1;
			error = sosetopt(so, level, option, m, kernel);
			m = NULL;
		}
check:
		m_freem(m);
		/* SCE: unlike connect's, this retry also follows ENETDOWN */
		if ((error < EADDRNOTAVAIL || error > ENETUNREACH) &&
		    error != EHOSTUNREACH)
			break;
		if (retry != 1 || !sceNetPsRequestAutoConnect(so, NULL, 0))
			break;
		result = sceNetPsWaitAutoConnect(so);
		if (result) {
			error = result;
			break;
		}
		retry = 0;
	}
	bnet_sorele(so);
	return sceNetPsUnlockResult(error, 0);
}

/*
 * SCE adaptation of NetBSD uipc_syscalls.c sys_getsockopt: a multicast
 * TTL or loop option asked for as an int is returned as one.
 */
int sceNetPsGetsockopt(int socket, int level, int option, void *value,
		       unsigned int *length, int kernel)
{
	struct socket *so = NULL;
	struct mbuf *m = NULL;
	unsigned int valsize = 0;
	int optval;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (length == NULL || value == NULL) {
		error = EINVAL;
		goto out;
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so == NULL)
		goto out;
	so->refCount++;
	error = sceNetPsCopyIn(length, &valsize, sizeof(valsize), kernel);
	if (error)
		goto out;
	error = sogetopt(so, level, option, &m);
	if (error || m == NULL)
		goto out;
	if (level == IPPROTO_IP &&
	    (option == IP_MULTICAST_TTL || option == IP_MULTICAST_LOOP) &&
	    valsize == sizeof(int)) {
		optval = *mtod(m, u_char *);
		error = sceNetPsCopyOut(&optval, value, sizeof(optval), kernel);
		goto out;
	}
	if (valsize > m->m_len)
		valsize = m->m_len;
	error = sceNetPsCopyOut(mtod(m, caddr_t), value, valsize, kernel);
	if (error)
		goto out;
	error = sceNetPsCopyOut(&valsize, length, sizeof(valsize), kernel);
out:
	m_freem(m);
	bnet_sorele(so);
	return sceNetPsUnlockResult(error, 0);
}

/* SCE adaptation of NetBSD uipc_syscalls.c sys_getsockname */
int sceNetPsGetsockname(int socket, SceNetSockaddr *address,
			unsigned int *addressLength, int kernel)
{
	struct socket *so = NULL;
	struct mbuf *m = NULL;
	int len, error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (addressLength == NULL || address == NULL) {
		error = EINVAL;
		goto out;
	}
	error = sceNetPsCopyIn(addressLength, &len, sizeof(len), kernel);
	if (error)
		goto out;
	if (len < sizeof(struct sockaddr)) {
		error = EINVAL;
		goto out;
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so == NULL)
		goto out;
	so->refCount++;
	m = m_getclr(M_WAIT, MT_SONAME);
	bnet_socket_sanity_check(so);
	error = (*so->so_proto->pr_usrreq)(so, PRU_SOCKADDR, NULL, m, NULL,
					   NULL);
	if (error)
		goto out;
	if (len > m->m_len)
		len = m->m_len;
	error = sceNetPsCopyOut(mtod(m, caddr_t), address, len, kernel);
	if (error)
		goto out;
	error = sceNetPsCopyOut(&len, addressLength, sizeof(len), kernel);
out:
	m_freem(m);
	bnet_sorele(so);
	return sceNetPsUnlockResult(error, 0);
}

/* SCE adaptation of NetBSD uipc_syscalls.c sys_getpeername */
int sceNetPsGetpeername(int socket, SceNetSockaddr *address,
			unsigned int *addressLength, int kernel)
{
	struct socket *so = NULL;
	struct mbuf *m = NULL;
	int len, error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (addressLength == NULL || address == NULL) {
		error = EINVAL;
		goto out;
	}
	error = sceNetPsCopyIn(addressLength, &len, sizeof(len), kernel);
	if (error)
		goto out;
	if (len < sizeof(struct sockaddr)) {
		error = EINVAL;
		goto out;
	}
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so == NULL)
		goto out;
	so->refCount++;
	if (!(so->so_state & (SS_ISCONNECTED | SS_ISCONFIRMING))) {
		error = ENOTCONN;
		goto out;
	}
	m = m_getclr(M_WAIT, MT_SONAME);
	bnet_socket_sanity_check(so);
	error = (*so->so_proto->pr_usrreq)(so, PRU_PEERADDR, NULL, m, NULL,
					   NULL);
	if (error)
		goto out;
	if (len > m->m_len)
		len = m->m_len;
	error = sceNetPsCopyOut(mtod(m, caddr_t), address, len, kernel);
	if (error)
		goto out;
	error = sceNetPsCopyOut(&len, addressLength, sizeof(len), kernel);
out:
	m_freem(m);
	bnet_sorele(so);
	return sceNetPsUnlockResult(error, 0);
}

/*
 * SCE adaptation of NetBSD sys_generic.c sys_ioctl and sys_socket.c
 * soo_ioctl, for user ioctls on a socket.
 */
int sceNetPsIoctl(int socket, unsigned int command, void *data)
{
	struct socket *so;
	u_long com = command;
	caddr_t buffer, memp = NULL;
	char stkbuf[128];	/* NetBSD sys_generic.c STK_PARAMS */
	unsigned int size;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so == NULL)
		goto out;
	so->refCount++;
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX) {
		error = ENOTTY;
		goto out;
	}
	if (size > sizeof(stkbuf)) {
		memp = sceNetPsMalloc(size, 0, 8);
		buffer = memp;
	} else {
		buffer = stkbuf;
	}
	if (com & IOC_IN) {
		if (size) {
			error = copyin(data, buffer, size);
			if (error)
				goto out;
		} else {
			*(caddr_t *)buffer = data;
		}
	} else if ((com & IOC_OUT) && size) {
		memset(buffer, 0, size);
	} else if (com & IOC_VOID) {
		*(caddr_t *)buffer = data;
	}
	switch (IOCGROUP(com)) {
	case 'i':
		error = ifioctl(so, com, buffer, curproc);
		break;
	case 'r':
		error = rtioctl(com, buffer, curproc);
		break;
	default:
		bnet_socket_sanity_check(so);
		error = (*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
						   (struct mbuf *)com,
						   (struct mbuf *)buffer, NULL,
						   curproc);
		break;
	}
	if (error == 0 && (com & IOC_OUT) && size)
		error = copyout(buffer, data, size);
out:
	sceNetPsFree(memp);
	bnet_sorele(so);
	return sceNetPsUnlockResult(error, 0);
}

/*
 * SCE: close a socket; one still named by an epoll or descriptor stays
 * there and the close reports ENOTEMPTY.  A socket lost to a resume or an
 * address change is only forgotten.
 */
int sceNetPsClose(int socket, int kernel)
{
	struct socket *so;
	int references, error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	so = sceNetPsLookupSocket(socket, &error, 0);
	if (so) {
		sceNetPsCancelEmulationWait(so);
		references = sceNetPsCloseSocket(so, &error, 0);
		sceNetPsConsumeWakeup((const void *)socket, 0);
		sceNetPsSetSocket(socket, NULL);
		if (references)
			error = ENOTEMPTY;
	} else if (error == SCE_NET_ERESUME || error == SCE_NET_EIPADDRCHANGED) {
		sceNetPsDestroySocket(socket);
		error = 0;
	}
	return sceNetPsUnlockResult(error, 0);
}

int sceNetPsGetIfList(void *list, int count)
{
	unsigned int size;
	void *buffer = NULL;
	int error = 0;
	int result = 0;
	int empty = list != NULL && count == 0;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (empty)
		goto freeBuffer;
	if (list == NULL) {
		result = sceNetPsGetInterfaceList(NULL, count);
		goto freeBuffer;
	}
	size = count * 320U;
	buffer = sceNetPsMalloc(size, 3, 8);
	if (buffer == NULL) {
		error = 12;
		goto freeBuffer;
	}
	result = sceNetPsGetInterfaceList(buffer, count);
	if (result < 0)
		goto freeBuffer;
	error = copyout(buffer, list, size);
	sceNetPsFree(buffer);
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
	goto out;
freeBuffer:
	sceNetPsFree(buffer);
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return result;
}

int sceNetPsControl(int interfaceIndex, int command, void *data, int length)
{
	void *buffer = NULL;
	int result = 0;
	int error = 0;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (data != NULL) {
		buffer = sceNetPsMalloc(length, 1, 8);
		if (buffer == NULL) {
			error = 12;
			goto freeBuffer;
		}
		if (command & 0x30000000) {
			error = copyin(data, buffer, length);
			if (error)
				goto copyError;
		}
	}
	if (interfaceIndex >= 0)
		result = sceNetPsInterfaceControl(interfaceIndex, command, buffer, length);
	else
		result = sceNetPsStackControl(NULL, command, buffer, length);
	if (result >= 0 && data != NULL &&
	    (command & 0x30000000) != 0x20000000) {
		error = copyout(buffer, data, length);
		goto copyError;
	}
freeBuffer:
	sceNetPsFree(buffer);
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return result;
copyError:
	sceNetPsFree(buffer);
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
	goto out;
}

int sceNetPsSysctl(const int *name, unsigned int nameLength, void *oldData,
		   unsigned int *oldLength, const void *newData,
		   unsigned int newLength)
{
	unsigned int length = 0;
	unsigned int *lengthPointer;
	unsigned int capacity;
	int identifiers[12];
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (oldLength != NULL) {
		error = copyin(oldLength, &length, sizeof(length));
		if (error)
			goto copyError;
		lengthPointer = &length;
		capacity = length;
	} else {
		lengthPointer = NULL;
		capacity = 0;
	}
	if (nameLength - 2 > 10) {
		error = 22;
		goto out;
	}
	error = copyin(name, identifiers,
					nameLength * sizeof(*identifiers));
	if (error)
		goto copyError;
	if (identifiers[0] != 4) {
		error = 45;
		goto out;
	}
	if (nameLength == 2 || (identifiers[1] != 0 && nameLength == 3)) {
		error = 21;
		goto positiveError;
	}
	if (identifiers[1] != 0) {
		if (identifiers[1] != 17) {
			error = 42;
			goto positiveError;
		}
		error = sceNetPsRoutingSysctl(identifiers + 3, nameLength - 3,
					      oldData, lengthPointer,
					      newData, newLength);
		if (error)
			goto copyError;
	}
	if (oldLength != NULL) {
		error = copyout(&length, oldLength, sizeof(length));
		if (error)
			goto copyError;
	}
	if (oldData != NULL)
		error = capacity < length ? 12 : 0;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return error;
copyError:
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
positiveError:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -error;
}

/* SCE: stop the entry's timeout and take it off its descriptor */
int sceNetPsUnlinkDescriptorEntry(SceNetPsDescriptorEntry *entry)
{
	callout_stop(&entry->timeout);
	if (entry->next)
		entry->next->prev = entry->prev;
	*entry->prev = entry->next;
	return 0;
}

/* SCE: drop the socket from every descriptor; returns the registrations left */
int sceNetPsRemoveSocketFromDescriptors(struct socket *so, int abort)
{
	SceNetPsDescriptor *descriptor;
	SceNetPsDescriptorEntry *entry;
	int first, last;
	int descriptorId;

	if (abort)
		return 0;
	sceNetPsGetDescriptorIdRange(&first, &last);
	for (descriptorId = first; descriptorId <= last; descriptorId++) {
		descriptor = sceNetPsLookupDescriptor(descriptorId, 1);
		if (descriptor == NULL)
			continue;
		for (entry = descriptor->entries; entry != NULL;
		     entry = entry->next) {
			if (entry->socket == so) {
				so->descriptorReferences--;
				sceNetPsUnlinkDescriptorEntry(entry);
				sceNetPsFree(entry);
				break;
			}
		}
	}
	return so->descriptorReferences;
}

/* SCE: descriptors keep nothing by socket identifier */
int sceNetPsRemoveSocketIdFromDescriptors(unsigned int socketId)
{
	return 0;
}

int sceNetPsDescriptorCreate(const char *name, int flags)
{
	SceNetPsDescriptor *descriptor;
	unsigned int copied;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags != 0 || name == NULL) {
		result = -22;
		goto out;
	}
	descriptor = sceNetPsMalloc(sizeof(*descriptor), 3, 8);
	if (descriptor == NULL) {
		result = -12;
		goto out;
	}
	result = sceNetPsAllocateDescriptorId();
	if (result < 0) {
		result = -24;
		sceNetPsFree(descriptor);
		goto out;
	}
	sceNetPsCopyInString(name, descriptor->name, 31, &copied);
	sceNetPsSetDescriptor(result, descriptor);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

int sceNetPsDescriptorClose(int descriptorId)
{
	SceNetPsDescriptor *descriptor;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	descriptor = sceNetPsLookupDescriptor(descriptorId, 0);
	if (descriptor != NULL) {
		sceNetPsDestroyDescriptor(descriptorId, descriptor);
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -9;
}

/* SCE: append a bounded record to the capture ring, or set overflow. */
void sceNetPsWriteDump(SceNetPsDump *dump, const void *buffer, int length)
{
	const char *source = buffer;
	int first;

	if (dump->flags & SCE_NET_DUMP_OVERFLOW)
		return;
	if (dump->length - dump->bufferedLength < length) {
		dump->flags |= SCE_NET_DUMP_OVERFLOW;
		return;
	}
	first = dump->length - dump->writePosition;
	if (length < first)
		first = length;
	if (source)
		memcpy(dump->buffer + dump->writePosition, source, first);
	else
		memset(dump->buffer + dump->writePosition, 0, first);
	dump->writePosition += first;
	dump->bufferedLength += first;
	length -= first;
	if (length > 0) {
		dump->writePosition = 0;
		if (source)
			memcpy(dump->buffer, source + first, length);
		else
			memset(dump->buffer, 0, length);
		dump->writePosition += length;
		dump->bufferedLength += length;
	}
	if (dump->writePosition >= dump->length)
		dump->writePosition -= dump->length;
}

int sceNetPsDestroyDump(unsigned int dumpId, SceNetPsDump *dump)
{
	extern int gNetPsPacketCaptureActive;

	--gNetPsPacketCaptureActive;
	if (dump->waiters)
		sceNetPsWakeupCanceled(dump);
	sceNetPsConsumeWakeup((const void *)(uintptr_t)dumpId, 0);
	sceNetPsFree(dump);
	sceNetPsSetDump(dumpId, NULL);
	return 0;
}

int sceNetPsDumpCreate(const char *name, int length, int flags)
{
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (length < 2048 || flags != 0 || name == NULL) {
		result = -22;
	} else {
		result = sceNetPsAllocateDumpId();
		if (result < 0)
			result = -24;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

int sceNetPsDumpClose(int dumpId)
{
	struct SceNetPsDump *dump;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	dump = sceNetPsLookupDump(dumpId, 0);
	if (dump != NULL) {
		sceNetPsDestroyDump(dumpId, dump);
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -9;
}

int sceNetPsDumpAbort(int dumpId, int flags)
{
	struct SceNetPsDump *dump;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags & ~1) {
		error = 22;
	} else {
		dump = sceNetPsLookupDump(dumpId, 0);
		if (dump == NULL) {
			error = 9;
		} else {
			error = sceNetPsWakeupInterrupted(dump);
			if (error != 0) {
				if (flags)
					error = sceNetPsQueueWakeup((const void *)(uintptr_t)dumpId, 1);
				if (error < 0) {
					error = -error;
					sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
					return -error;
				}
			}
		}
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return error ? -error : error;
}

/*
 * SCE: mark the socket's epoll entries closed and, unless aborting, free
 * them, waking each epoll that waits; returns the registrations left.
 */
int sceNetPsRemoveSocketFromEpolls(struct socket *so, int abort)
{
	SceNetPsEpoll *epoll;
	SceNetPsEpollEntry *entry;
	int first, last;
	int epollId;

	sceNetPsGetEpollIdRange(&first, &last);
	for (epollId = first; epollId <= last; epollId++) {
		epoll = sceNetPsLookupEpoll(epollId, 1);
		if (epoll == NULL)
			continue;
		for (entry = epoll->entries; entry != NULL; entry = entry->next)
			if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) &&
			    entry->target.socket == so)
				break;
		if (entry == NULL)
			continue;
		if (!(entry->kind & SCE_NETPS_EPOLL_ENTRY_CLOSED))
			so->epollReferences--;
		entry->kind |= SCE_NETPS_EPOLL_ENTRY_CLOSED;
		if (!abort) {
			if (entry->next)
				entry->next->prev = entry->prev;
			*entry->prev = entry->next;
			sceNetPsFree(entry);
		}
		if (epoll->flags & SCE_NETPS_EPOLL_WAITING)
			sceNetPsWakeupNormal(epoll);
	}
	return so->epollReferences;
}

/* SCE: free the epoll entries left for a socket identifier */
int sceNetPsRemoveSocketIdFromEpolls(unsigned int socketId)
{
	SceNetPsEpoll *epoll;
	SceNetPsEpollEntry *entry;
	int first, last;
	int epollId;

	sceNetPsGetEpollIdRange(&first, &last);
	for (epollId = first; epollId <= last; epollId++) {
		epoll = sceNetPsLookupEpoll(epollId, 1);
		if (epoll == NULL)
			continue;
		for (entry = epoll->entries; entry != NULL; entry = entry->next) {
			if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) &&
			    entry->id == socketId) {
				if (entry->next)
					entry->next->prev = entry->prev;
				*entry->prev = entry->next;
				sceNetPsFree(entry);
				break;
			}
		}
	}
	return 0;
}

/* SCE: wake each epoll that watches the socket, directly or through a descriptor */
void sceNetPsWakeupEpolls(struct socket *socket)
{
	SceNetPsEpoll *epoll;
	SceNetPsEpollEntry *entry;
	SceNetPsDescriptorEntry *descriptorEntry;
	int first, last;
	int epollId;

	sceNetPsGetEpollIdRange(&first, &last);
	for (epollId = first; epollId <= last; epollId++) {
		epoll = sceNetPsLookupEpoll(epollId, 1);
		if (epoll == NULL)
			continue;
		for (entry = epoll->entries; entry != NULL; entry = entry->next) {
			if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) &&
			    entry->target.socket == socket)
				goto wakeup;
			if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_DESCRIPTOR) &&
			    entry->target.descriptor != NULL) {
				for (descriptorEntry = entry->target.descriptor->entries;
				     descriptorEntry != NULL;
				     descriptorEntry = descriptorEntry->next)
					if (descriptorEntry->socket == socket)
						goto wakeup;
			}
		}
		continue;
wakeup:
		sceNetPsWakeupNormal(epoll);
	}
}

int sceNetPsEpollCreate(const char *name, int flags)
{
	SceNetPsEpoll *epoll;
	unsigned int copied;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags != 0 || name == NULL) {
		result = -22;
		goto out;
	}
	epoll = sceNetPsMalloc(sizeof(*epoll), 3, 8);
	if (epoll == NULL) {
		result = -12;
		goto out;
	}
	result = sceNetPsAllocateEpollId();
	if (result < 0) {
		result = -24;
		sceNetPsFree(epoll);
		goto out;
	}
	sceNetPsCopyInString(name, epoll->name, 31, &copied);
	sceNetPsSetEpoll(result, epoll);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

int sceNetPsEpollClose(int epollId)
{
	SceNetPsEpoll *epoll;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	epoll = sceNetPsLookupEpoll(epollId, 0);
	if (epoll != NULL) {
		sceNetPsDestroyEpoll(epollId, epoll);
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -9;
}

int sceNetPsEpollAbort(int epollId, int flags)
{
	SceNetPsEpoll *epoll;
	unsigned int invalidFlags;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	invalidFlags = flags & ~1;
	if (invalidFlags) {
		error = 22;
		goto out;
	}
	epoll = sceNetPsLookupEpoll(epollId, invalidFlags);
	if (epoll == NULL) {
		error = 9;
		goto out;
	}
	error = sceNetPsWakeupInterrupted(epoll);
	if (error == 15) {
		if (flags == 0)
			goto out;
		error = sceNetPsQueueWakeup((const void *)(uintptr_t)epollId, 1);
	}
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return invalidFlags;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -error;
}
