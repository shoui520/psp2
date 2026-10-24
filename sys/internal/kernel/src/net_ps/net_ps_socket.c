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
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/udpp2p_var.h>
#include <net/sockinfo.h>
#include <string.h>

extern struct domain inetdomain;
extern struct inpcbtable rawcbtable;

int sceNetPsCollectSockInfo(int socket, SceNetSockInfo *records, int capacity,
			    int flags, int *count, SceUID processId, int kernel);

struct mbuf *m_get(int how, int type);
void sce_psnet_panic(const char *message, ...) __attribute__((noreturn));
extern struct malloc_type *mbtypes[8];
int sceNetPsDestroyDump(unsigned int dumpId, struct SceNetPsDump *dump);
int sceNetPsDestroyDescriptor(unsigned int descriptorId,
			     SceNetPsDescriptor *descriptor);
int sysctl_rtable(int *name, u_int namelen, void *where, size_t *given,
		  void *new, size_t newlen);

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
	sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
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
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (socket >= 0)
		SceNetPsForDriver_21F4428D(socket);
	return 0;
failed:
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
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

	so = sceNetPsLookupSocketUnchecked(socketId, &error, 0);
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
		error = sysctl_rtable(identifiers + 3, nameLength - 3, oldData,
				      lengthPointer, (void *)newData, newLength);
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

/*
 * SCE: act on a process's sockets, or on all of them with flags bit 0:
 * 8 aborts the connected ones after an address change, 2 suspends them
 * (aborting those that asked for it), 4 resumes them, none closes them;
 * sockets lost to a resume or an address change are forgotten. 0x1000000:
 * the caller holds the stack lock; 0x2000000: only try the lock.
 */
int sceNetPsManageProcessSockets(SceUID processId, unsigned int flags)
{
	struct socket *so;
	struct inpcb *inp;
	int first, error = 0, last, id, count;

	if (!(flags & 0x1000000)) {
		if (flags & 0x2000000) {
			/* Sony's timeout is the range's last */
			last = 0;
			sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex,
						  (unsigned int *)&last);
			if (last < 0)
				return -ENOLCK;
		} else {
			sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
		}
	}
	sceNetPsGetSocketIdRange(&first, &last);
	count = 0;
	for (id = first; id <= last; id++) {
		so = sceNetPsLookupSocketUnchecked(id, &error, 0);
		if (so && !(flags & 1) && so->processId != processId)
			continue;
		so = sceNetPsLookupSocket(id, &error, 1);
		if (so == NULL) {
			if (flags == 0 && (error == SCE_NET_ERESUME ||
					   error == SCE_NET_EIPADDRCHANGED)) {
				count++;
				sceNetPsDestroySocket(id);
			}
			continue;
		}
		if (flags & 8) {
			switch (so->so_type) {
			case SOCK_STREAM:
				inp = sotoinpcb(so);
				if (inp && intotcpcb(inp)->t_state == TCPS_CLOSED)
					continue;
				break;
			case SOCK_DGRAM:
				if (!(so->so_state & SS_ISCONNECTED))
					continue;
				break;
			case SCE_NET_SOCK_DGRAM_P2P:
				if (!(so->so_state & SS_ISCONNECTED)) {
					inp = sotoinpcb(so);
					if (!inp || inp->inp_state != INP_BOUND)
						continue;
				}
				break;
			case SCE_NET_SOCK_STREAM_P2P:
				break;
			default:
				continue;
			}
			count++;
			so->so_options |= SO_SCE_OPTION_10000;
			sceNetPsAbortSocket(so, 1);
			sceNetPsSetSocketAddressChanged(id, 1);
		} else if (flags & 2) {
			so->so_state |= SS_SCE_SUSPENDED;
			if (so->so_options & SO_SCE_OPTION_80000) {
				count++;
				sceNetPsAbortSocket(so, 1);
				sceNetPsSetSocketResume(id, 1);
			}
		} else if (flags & 4) {
			so->so_state &= ~SS_SCE_SUSPENDED;
		} else {
			count++;
			sceNetPsAbortSocket(so, 0);
			sceNetPsDestroySocket(id);
		}
	}
	if (flags & 0x1000000)
		return count;
	return sceNetPsUnlockResult(error, count);
}

/* SCE: a buffer sceNetControl 0x20000008 reserves from the stack's heap */
typedef struct SceNetPsReservedMemory {
	void *address;
	SceSize size;
	unsigned int flags;		/* 1: reserved */
} SceNetPsReservedMemory;

static SceNetPsReservedMemory gNetPsReservedMemory[4];

/*
 * SCE: sceNetControl's stack commands, on one interface (context) or on
 * the stack (NULL): 0x1xxxxxxx and the small ones get, 0x2xxxxxxx set
 */
int sceNetPsStackControl(struct SceNetPsDriverContext *context, int command,
			 void *data, int length)
{
	unsigned int *args = data;
	SceNetPsReservedMemory *memory;
	SceNetPsSleepCount *counts;
	SceUID threadId;
	int index, total, count;

	if (data == NULL)
		return -EINVAL;
	switch (command) {
	case 1:
		if (context)
			return -EIO;
		if (length <= 15)
			return -EINVAL;
		return sceNetPsGetKernelStatistics(data, 0);
	case 2:
		if (context)
			return -EIO;
		if (length <= 39)
			return -EINVAL;
		return sceNetPsGetDnsConfig(data, 0, 0);
	case 4:
		if (!context)
			return -EIO;
		if (length <= 19)
			return -EINVAL;
		return sceNetPsGetConnectionParam(context, data);
	case 0x13:
		if (!context)
			return -EIO;
		if (length <= 7)
			return -EINVAL;
		return sceNetPsGetPromiscuous(context, data);
	case 0x14:
		if (length <= 3)
			return -EINVAL;
		args[0] = sceNetPsGetRandom();
		return 0;
	case 0x16:
		total = length / (int)sizeof(*counts);
		if (total < 0)
			return -EINVAL;
		if (total == 0)
			return 0;
		counts = data;
		count = 0;
		for (index = 0; index != total; index++) {
			if (sceNetPsGetSleepCount(&counts[count], index) == 1) {
				sceNetPsGetProcessPolicy(counts[count].identifier,
							 &counts[count].policy);
				count++;
			}
		}
		return count;
	case 0x21:
		total = length / (int)sizeof(SceNetPsThreadInfo);
		if (total < 0)
			return -EINVAL;
		return sceNetPsGetThreadList(data, total);
	case 0x22:
		if (context)
			return -EIO;
		if (length <= 67)
			return -EINVAL;
		return sceNetPsTakeSenders(data);
	case 0x10000002:
		if (length <= 31)
			return -EINVAL;
		return sceNetPsTakeDriverEvents(context, args[0], data);
	case 0x10000012:
		if (length < 512)
			return -EINVAL;
		return sceNetPsGetDriverEmulation(context, data, 0);
	case 0x10000017:
		if (length <= 7 || args[1] != 0)
			return -EINVAL;
		return sceNetPsGetProcessPolicy(args[0], (int *)&args[1]);
	case 0x10000018:
		if (length <= 7)
			return -EINVAL;
		if (args[1] & 1)
			return sceNetPsGetThreadError(sceNetPsGetThreadId(),
						      (int *)&args[0]);
		return sceNetPsGetProcessError(sceNetPsGetProcessId(),
					       (int *)&args[0]);
	case 0x10000019:
		if (length <= 11 || args[2] != 0)
			return -EINVAL;
		if (args[1] & 1)
			return sceNetPsGetThreadMode(args[0], (int *)&args[2]);
		threadId = bnet_puid_to_guid(args[0]);
		if (threadId < 0)
			return -EINVAL;
		return sceNetPsGetThreadMode(threadId, (int *)&args[2]);
	case 0x20000002:
		if (context)
			return -EIO;
		if (length <= 39 || ((SceNetPsDnsConfig *)data)->flags != 0)
			return -EINVAL;
		return sceNetPsSetDnsConfig(data);
	case 0x20000003:
		if (length <= 7)
			return -EINVAL;
		return sceNetPsRegisterDriverCallback(context, args[0], args[1]);
	case 0x20000004:
		if (!context)
			return -EIO;
		if (length <= 19)
			return -EINVAL;
		return sceNetPsSetConnectionParam(context, data);
	case 0x20000005:
		if (length <= 31)
			return -EINVAL;
		return sceNetPsPostDriverEvents(context, args[0], data);
	case 0x20000006:
		if (length <= 7)
			return -EINVAL;
		if ((args[1] & 1) && sceNetPsHasPendingDriverEvents(0x1000000))
			return 0;
		return sceNetPsUpdateConnections(context, args[0]);
	case 0x20000007:
		if (length <= 7)
			return -EINVAL;
		return sceNetPsUnregisterDriverCallback(context, args[0]);
	case 0x20000008:
		if (context)
			return -EIO;
		if (length <= 7 || args[0] > 3)
			return -EINVAL;
		memory = &gNetPsReservedMemory[args[0]];
		if (memory->flags & 1)
			return -EEXIST;
		memory->address = sceNetPsMalloc(args[1], 1, 8);
		if (memory->address == NULL)
			return 0;
		memory->size = args[1];
		memory->flags |= 1;
		return 0;
	case 0x20000009:
		if (context)
			return -EIO;
		if (length <= 7 || args[0] > 3)
			return -EINVAL;
		memory = &gNetPsReservedMemory[args[0]];
		if (!(memory->flags & 1))
			return -EBADF;
		sceNetPsFree(memory->address);
		memory->size = 0;
		memory->flags &= ~1;
		return 0;
	case 0x20000010:
		if (!context)
			return -EIO;
		if (length <= 3)
			return -EINVAL;
		return sceNetPsNotifyLinkState(context);
	case 0x20000011:
		if (length <= 3 || args[0] != 0)
			return -EINVAL;
		return sceNetPsResetIdleTime(context);
	case 0x20000012:
		if (length < 512)
			return -EINVAL;
		return sceNetPsSetDriverEmulation(context, data, 0, 0);
	case 0x20000013:
		if (!context)
			return -EIO;
		if (length <= 7)
			return -EINVAL;
		return sceNetPsSetPromiscuous(context, data);
	case 0x20000017:
		if (length <= 7)
			return -EINVAL;
		return sceNetPsSetProcessPolicy(args[0], args[1]);
	case 0x20000018:
		if (length <= 7)
			return -EINVAL;
		if (args[1] & 1)
			return sceNetPsSetThreadError(sceNetPsGetThreadId(), args[0]);
		return sceNetPsSetProcessError(sceNetPsGetProcessId(), args[0]);
	case 0x20000019:
		if (length <= 11)
			return -EINVAL;
		threadId = bnet_puid_to_guid(args[0]);
		if (threadId < 0)
			return -EINVAL;
		return sceNetPsSetThreadMode(threadId, args[2]);
	case 0x20000020:
		if (length <= 3)
			return -EINVAL;
		sceNetPsSetSocketSuspendPolicy(args[0]);
		return 0;
	case 0x20000023:
		if (length <= 7)
			return -EINVAL;
		tcp_maxmss_to_advertise = args[0];
		tcp_maxmss_from_peer = args[1];
		return 0;
	}
	return -EINVAL;
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

/* SCE: the entry's timeout: report SCE_NET_ETIME on its socket */
static void sceNetPsDescriptorEntryTimeout(void *argument)
{
	SceNetPsDescriptorEntry *entry = argument;

	entry->flags &= ~SCE_NETPS_DESCRIPTOR_ENTRY_TIMER;
	entry->socket->so_error = SCE_NET_ETIME;
	sceNetPsWakeupEpolls(entry->socket);
}

/* SCE: free a descriptor with its entries and release its identifier */
int sceNetPsDestroyDescriptor(unsigned int descriptorId,
			      SceNetPsDescriptor *descriptor)
{
	SceNetPsDescriptorEntry *entry;

	sceNetPsRemoveDescriptorFromEpolls(descriptor);
	while ((entry = descriptor->entries) != NULL) {
		if (entry->socket)
			entry->socket->descriptorReferences--;
		sceNetPsUnlinkDescriptorEntry(entry);
		sceNetPsFree(entry);
	}
	sceNetPsFree(descriptor);
	sceNetPsSetDescriptor(descriptorId, NULL);
	return 0;
}

/*
 * SCE: put the entry at the head of the descriptor; the caller's system
 * data names the socket again and may carry a timeout
 */
static __attribute__((noinline))
int sceNetPsLinkDescriptorEntry(int socketId, SceNetPsDescriptor *descriptor,
				SceNetPsDescriptorEntry *entry,
				const void *system)
{
	int error, timeout;

	if (system) {
		error = copyin(system, &entry->system, sizeof(entry->system));
		if (error)
			return error;
		if (entry->system.system[2] != socketId)
			return EINVAL;
		timeout = sceNetPsNormalizeTimeout(entry->system.system[3]);
		if (timeout >= 0) {
			callout_init(&entry->timeout);
			callout_reset(&entry->timeout, timeout / 1000,
				      sceNetPsDescriptorEntryTimeout, entry);
			entry->flags |= SCE_NETPS_DESCRIPTOR_ENTRY_TIMER;
		}
	}
	entry->next = descriptor->entries;
	if (descriptor->entries)
		descriptor->entries->prev = &entry->next;
	descriptor->entries = entry;
	entry->prev = &descriptor->entries;
	return 0;
}

/* SCE: stop the entry's timeout once its socket is reported */
void sceNetPsStopDescriptorEntry(SceNetPsDescriptorEntry *entry)
{
	callout_stop(&entry->timeout);
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

/* SCE: the first descriptor that holds the socket, or -1 */
int sceNetPsFindSocketDescriptor(struct socket *so)
{
	SceNetPsDescriptor *descriptor;
	SceNetPsDescriptorEntry *entry;
	int first, last;
	unsigned int descriptorId;

	sceNetPsGetDescriptorIdRange(&first, &last);
	for (descriptorId = first; (int)descriptorId <= last; descriptorId++) {
		descriptor = sceNetPsLookupDescriptor(descriptorId, 1);
		if (descriptor == NULL)
			continue;
		for (entry = descriptor->entries; entry != NULL;
		     entry = entry->next) {
			if (entry->socket == so)
				return descriptorId;
		}
	}
	return -1;
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
	copyinstr(name, descriptor->name, 31, &copied);
	sceNetPsSetDescriptor(result, descriptor);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

/*
 * SCE: sceNetDescriptorControl: add (or add watched), modify or delete
 * the socket's entry in the descriptor
 */
int sceNetPsDescriptorCtl(int descriptorId, int operation, int socketId,
			  const void *system)
{
	SceNetPsDescriptor *descriptor;
	SceNetPsDescriptorEntry *entry;
	struct socket *so;
	unsigned int flags;
	int error = 0;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	descriptor = sceNetPsLookupDescriptor(descriptorId, 0);
	if (descriptor == NULL)
		goto badf;
	so = sceNetPsLookupSocket(socketId, &error, 0);
	if (so == NULL)
		goto out;
	switch (operation) {
	case SCE_NET_EPOLL_CTL_ADD:
	case SCE_NETPS_DESCRIPTOR_CTL_WATCH:
		for (entry = descriptor->entries; entry != NULL;
		     entry = entry->next) {
			if (entry->socket == so) {
				error = EEXIST;
				goto fail;
			}
		}
		entry = sceNetPsMalloc(sizeof(*entry), 3, 8);
		if (entry == NULL) {
			error = ENOMEM;
			goto fail;
		}
		error = sceNetPsLinkDescriptorEntry(socketId, descriptor, entry,
						    system);
		if (error)
			goto release;
		entry->socket = so;
		if (operation == SCE_NETPS_DESCRIPTOR_CTL_WATCH)
			entry->flags |= SCE_NETPS_DESCRIPTOR_ENTRY_WATCH;
		sceNetPsWakeupEpolls(so);
		so->descriptorReferences++;
		break;
	case SCE_NET_EPOLL_CTL_MOD:
		for (entry = descriptor->entries; entry != NULL;
		     entry = entry->next) {
			if (entry->socket == so)
				break;
		}
		if (entry == NULL)
			goto badf;
		flags = entry->flags;
		sceNetPsUnlinkDescriptorEntry(entry);
		error = sceNetPsLinkDescriptorEntry(socketId, descriptor, entry,
						    system);
		if (error)
			goto release;
		entry->socket = so;
		entry->flags = flags;
		sceNetPsWakeupEpolls(so);
		break;
	case SCE_NET_EPOLL_CTL_DEL:
		if (system) {
			error = EINVAL;
			goto fail;
		}
		for (entry = descriptor->entries; entry != NULL;
		     entry = entry->next) {
			if (entry->socket == so)
				break;
		}
		if (entry == NULL)
			goto badf;
		so->descriptorReferences--;
		sceNetPsUnlinkDescriptorEntry(entry);
		goto release;
	default:
		error = EINVAL;
		goto fail;
	}
out:
	return sceNetPsUnlockResult(error, 0);
release:
	sceNetPsFree(entry);
	goto out;
badf:
	error = EBADF;
fail:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -error;
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

/* SCE: destroy every descriptor the process owns */
int sceNetPsDestroyProcessDescriptors(SceUID processId)
{
	SceNetPsDescriptor *descriptor;
	int first, last, descriptorId;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sceNetPsGetDescriptorIdRange(&first, &last);
	for (descriptorId = first; descriptorId <= last; descriptorId++) {
		descriptor = sceNetPsLookupDescriptor(descriptorId, 1);
		if (descriptor && descriptor->processId == processId)
			sceNetPsDestroyDescriptor(descriptorId, descriptor);
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
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

/*
 * SCE: a transmitted packet, padded to Ethernet's minimum frame, goes to
 * every capture ring with room for it; a ring without room overflows.
 */
int sceNetPsCaptureTxPacket(struct NetDriverPacket *packet)
{
	SceNetPsPcapRecordHeader header;
	struct NetDriverBuffer *buffer;
	SceNetPsDump *dump;
	SceUInt64 tick;
	int first, last, dumpId, length, padding;

	sceNetPsGetCurrentTick(&tick);
	header.seconds = tick / 1000000;
	header.microseconds = tick % 1000000;
	length = packet->length;
	padding = 60 - length;
	if (padding < 0)
		padding = 0;
	header.capturedLength = length + padding;
	header.length = length + padding;
	sceNetPsGetDumpIdRange(&first, &last);
	for (dumpId = first; dumpId <= last; dumpId++) {
		dump = sceNetPsLookupDump(dumpId, 1);
		if (dump == NULL || (dump->flags & SCE_NET_DUMP_OVERFLOW))
			continue;
		if (dump->length - dump->bufferedLength <
		    (int)sizeof(header) + length + padding) {
			dump->flags |= SCE_NET_DUMP_OVERFLOW;
			continue;
		}
		sceNetPsWriteDump(dump, &header, sizeof(header));
		for (buffer = packet->buffer; buffer; buffer = buffer->next)
			sceNetPsWriteDump(dump, buffer->data,
					  buffer->end - buffer->data);
		if (padding)
			sceNetPsWriteDump(dump, NULL, padding);
		sceNetPsWakeupNormal(dump);
	}
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

/* SCE: destroy the capture rings a process owns */
int sceNetPsDestroyProcessDumps(SceUID processId)
{
	SceNetPsDump *dump;
	int first, last, dumpId;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sceNetPsGetDumpIdRange(&first, &last);
	for (dumpId = first; dumpId <= last; dumpId++) {
		dump = sceNetPsLookupDump(dumpId, 1);
		if (dump && dump->processId == processId)
			sceNetPsDestroyDump(dumpId, dump);
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
}

/* SCE: free an epoll with its entries, wake its waiter and release its identifier */
int sceNetPsDestroyEpoll(unsigned int epollId, SceNetPsEpoll *epoll)
{
	SceNetPsEpollEntry *entry;

	if (epoll->flags & SCE_NETPS_EPOLL_WAITING)
		sceNetPsWakeupCanceled(epoll);
	while ((entry = epoll->entries) != NULL) {
		if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) &&
		    entry->target.socket)
			entry->target.socket->epollReferences--;
		if (entry->next)
			entry->next->prev = entry->prev;
		*entry->prev = entry->next;
		sceNetPsFree(entry);
	}
	sceNetPsConsumeWakeup((const void *)(uintptr_t)epollId, 0);
	sceNetPsFree(epoll);
	sceNetPsSetEpoll(epollId, NULL);
	return 0;
}

/*
 * SCE: NetBSD's soreadable and sowritable for the requested events, with
 * SB_SEL set on the buffers that are not ready; a closed entry reports what
 * it asked for
 */
static int sceNetPsPollEpollEntry(unsigned int kind, const unsigned int *events,
				  struct socket *so)
{
	int revents, error;

	if (kind & SCE_NETPS_EPOLL_ENTRY_CLOSED)
		return *events & (SCE_NET_EPOLLIN | SCE_NET_EPOLLOUT);
	if (so == NULL)
		return 0;
	revents = 0;
	if (*events & SCE_NET_EPOLLIN) {
		if (soreadable(so))
			revents = SCE_NET_EPOLLIN;
		else
			so->so_rcv.sb_flags |= SB_SEL;
	}
	error = so->so_error;
	if (*events & SCE_NET_EPOLLOUT) {
		if ((sbspace(&so->so_snd) >= so->so_snd.sb_lowat &&
		     ((so->so_state & SS_ISCONNECTED) ||
		      (so->so_proto->pr_flags & PR_CONNREQUIRED) == 0)) ||
		    (so->so_state & SS_CANTSENDMORE) || error)
			revents |= SCE_NET_EPOLLOUT;
		else
			so->so_snd.sb_flags |= SB_SEL;
	}
	if (so->so_snd.sb_flags & SB_SCE_CONNECTED) {
		revents |= SCE_NETPS_EPOLL_CONNECTED | SCE_NETPS_EPOLL_NOTICE;
		so->so_snd.sb_flags &= ~SB_SCE_CONNECTED;
	}
	if (error == EINTR)
		return SCE_NET_EPOLLHUP;
	if (error == SCE_NET_ETIME) {
		so->so_error = 0;
		return revents | SCE_NETPS_EPOLL_NOTICE;
	}
	if (error && !(revents & (SCE_NET_EPOLLIN | SCE_NET_EPOLLOUT)))
		return SCE_NET_EPOLLERR;
	return revents;
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
	copyinstr(name, epoll->name, 31, &copied);
	sceNetPsSetEpoll(result, epoll);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

/* SCE: the epoll's entry for the socket or the descriptor */
static inline SceNetPsEpollEntry *sceNetPsFindEpollEntry(SceNetPsEpoll *epoll,
							 struct socket *so,
							 SceNetPsDescriptor *descriptor)
{
	SceNetPsEpollEntry *entry;

	for (entry = epoll->entries; entry != NULL; entry = entry->next) {
		if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) &&
		    entry->target.socket == so)
			break;
		if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_DESCRIPTOR) &&
		    entry->target.descriptor == descriptor)
			break;
	}
	return entry;
}

/* SCE: the event's system words are the kernel's; the caller leaves them 0 */
static inline int sceNetPsIsEpollSystemDataClear(const SceNetEpollEvent *event)
{
	return event->system.system[0] == 0 && event->system.system[1] == 0 &&
	       event->system.system[2] == 0 && event->system.system[3] == 0;
}

/*
 * SCE: sceNetEpollControl: an identifier in the descriptor range names a
 * descriptor, any other a socket
 */
int sceNetPsEpollCtl(int epollId, int operation, int id,
		     const SceNetEpollEvent *event)
{
	SceNetPsEpoll *epoll;
	SceNetPsEpollEntry *entry;
	SceNetPsDescriptor *descriptor;
	SceNetPsDescriptorEntry *member;
	struct socket *so;
	int first, last, error = 0;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	epoll = sceNetPsLookupEpoll(epollId, 0);
	if (epoll == NULL)
		goto badf;
	sceNetPsGetDescriptorIdRange(&first, &last);
	if (id >= first && id <= last) {
		so = NULL;
		descriptor = sceNetPsLookupDescriptor(id, 0);
		if (descriptor == NULL)
			goto badf;
	} else {
		descriptor = NULL;
		so = sceNetPsLookupSocket(id, &error, 0);
		if (so == NULL)
			goto out;
	}
	if (operation == SCE_NET_EPOLL_CTL_MOD) {
		if (event == NULL)
			goto inval;
		entry = sceNetPsFindEpollEntry(epoll, so, descriptor);
		if (entry == NULL)
			goto badf;
		error = copyin(event, &entry->event, sizeof(entry->event));
		if (error)
			goto out;
		if (!sceNetPsIsEpollSystemDataClear(&entry->event))
			goto inval;
		goto wakeup;
	}
	if (operation == SCE_NET_EPOLL_CTL_DEL) {
		if (event != NULL)
			goto inval;
		entry = sceNetPsFindEpollEntry(epoll, so, descriptor);
		if (entry == NULL)
			goto badf;
		if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) &&
		    entry->target.socket == so) {
			so->so_rcv.sb_flags &= ~SB_SEL;
			so->so_snd.sb_flags &= ~SB_SEL;
			so->epollReferences--;
		} else {
			for (member = descriptor->entries; member != NULL;
			     member = member->next) {
				so = member->socket;
				if (so) {
					so->so_rcv.sb_flags &= ~SB_SEL;
					so->so_snd.sb_flags &= ~SB_SEL;
				}
			}
		}
		if (entry->next)
			entry->next->prev = entry->prev;
		*entry->prev = entry->next;
		sceNetPsFree(entry);
		goto wakeup;
	}
	if (operation != SCE_NET_EPOLL_CTL_ADD || event == NULL)
		goto inval;
	if (sceNetPsFindEpollEntry(epoll, so, descriptor) != NULL) {
		error = EEXIST;
		goto fail;
	}
	entry = sceNetPsMalloc(sizeof(*entry), 3, 8);
	if (entry == NULL) {
		error = ENOMEM;
		goto fail;
	}
	error = copyin(event, &entry->event, sizeof(entry->event));
	if (error) {
		sceNetPsFree(entry);
		goto out;
	}
	if (!sceNetPsIsEpollSystemDataClear(&entry->event)) {
		error = EINVAL;
		sceNetPsFree(entry);
		goto out;
	}
	if (so) {
		entry->kind = SCE_NETPS_EPOLL_ENTRY_SOCKET;
		entry->target.socket = so;
		so->epollReferences++;
	} else if (descriptor) {
		entry->kind = SCE_NETPS_EPOLL_ENTRY_DESCRIPTOR;
		entry->target.descriptor = descriptor;
	} else {
		error = ENOSPC;
		sceNetPsFree(entry);
		goto out;
	}
	entry->id = id;
	entry->next = epoll->entries;
	if (epoll->entries)
		epoll->entries->prev = &entry->next;
	epoll->entries = entry;
	entry->prev = &epoll->entries;
wakeup:
	if (epoll->flags & SCE_NETPS_EPOLL_WAITING)
		sceNetPsWakeupNormal(epoll);
out:
	return sceNetPsUnlockResult(error, 0);
inval:
	error = EINVAL;
	goto fail;
badf:
	error = EBADF;
fail:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -error;
}

/*
 * SCE: sceNetEpollWait: collect up to maxevents ready entries, sleeping
 * until one is ready or the timeout ends; a descriptor reports its first
 * ready socket with that entry's system data, and a watched socket once
 * when its connection is writable. The time left goes back to *timeoutp.
 */
int sceNetPsEpollWait(int epollId, SceNetEpollEvent *events, int maxevents,
		      int *timeoutp, int callback)
{
	SceNetPsEpoll *epoll;
	SceNetPsEpollEntry *entry;
	SceNetPsDescriptor *descriptor;
	SceNetPsDescriptorEntry *member;
	SceNetEpollEvent *ready;
	struct socket *so;
	SceUInt64 elapsed = 0, now;
	unsigned int sleepFlags;
	int timeout, delay, size, count, revents, result, error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	epoll = sceNetPsLookupEpoll(epollId, 0);
	if (epoll == NULL) {
		error = EBADF;
		goto fail;
	}
	if (events == NULL || maxevents <= 0) {
		error = EINVAL;
		goto fail;
	}
	count = 0;
	error = copyin(timeoutp, &timeout, sizeof(timeout));
	if (error)
		goto out;
	if (sceNetPsConsumeWakeup((const void *)(uintptr_t)epollId, 1)) {
		error = EINTR;
		goto fail;
	}
	size = maxevents * sizeof(*ready);
	ready = sceNetPsMalloc(size, 3, 8);
	if (ready == NULL) {
		error = ENOMEM;
		goto fail;
	}
	error = 0;
	delay = sceNetPsNormalizeTimeout(timeout);
	sleepFlags = callback != 0;
	for (;;) {
		count = 0;
		for (entry = epoll->entries; entry != NULL; entry = entry->next) {
			if (entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) {
				revents = sceNetPsPollEpollEntry(entry->kind,
						&entry->event.events,
						entry->target.socket);
				if (entry->event.events & SCE_NETPS_EPOLL_CONNECTED)
					revents &= ~SCE_NETPS_EPOLL_NOTICE;
				else
					revents &= ~(SCE_NETPS_EPOLL_NOTICE |
						     SCE_NETPS_EPOLL_CONNECTED);
				if (revents == 0)
					continue;
				ready[count] = entry->event;
				ready[count].events = revents;
			} else if (entry->kind & SCE_NETPS_EPOLL_ENTRY_DESCRIPTOR) {
				descriptor = entry->target.descriptor;
				if (descriptor == NULL)
					continue;
				for (member = descriptor->entries; member != NULL;
				     member = member->next) {
					so = member->socket;
					if ((member->flags &
					     (SCE_NETPS_DESCRIPTOR_ENTRY_WATCH |
					      SCE_NETPS_DESCRIPTOR_ENTRY_FIRED)) ==
					    SCE_NETPS_DESCRIPTOR_ENTRY_WATCH &&
					    so->so_type == SOCK_STREAM &&
					    sowritable(so)) {
						revents = SCE_NET_EPOLLIN |
							  SCE_NET_EPOLLDESCID |
							  SCE_NETPS_EPOLL_NOTICE;
						member->flags |=
							SCE_NETPS_DESCRIPTOR_ENTRY_FIRED;
						sceNetPsPollEpollEntry(entry->kind,
							&entry->event.events, so);
						break;
					}
					revents = sceNetPsPollEpollEntry(entry->kind,
							&entry->event.events, so);
					if (revents) {
						if (revents & ~(SCE_NETPS_EPOLL_NOTICE |
								SCE_NET_EPOLLHUP))
							revents |= SCE_NET_EPOLLDESCID;
						break;
					}
				}
				if (member == NULL)
					continue;
				sceNetPsStopDescriptorEntry(member);
				ready[count] = entry->event;
				ready[count].events = revents;
				memcpy(&ready[count].system, &member->system,
				       sizeof(member->system));
			} else {
				continue;
			}
			if (++count >= maxevents)
				break;
		}
		if (count) {
			error = 0;
			break;
		}
		if (error)
			break;
		if (delay > 0) {
			if (elapsed >= (SceUInt64)delay)
				break;
			delay -= (int)elapsed;
		}
		if (delay == 0)
			break;
		/* elapsed holds the time the sleep starts until it ends */
		sceNetPsGetSystemTime(&elapsed);
		epoll->flags |= SCE_NETPS_EPOLL_WAITING;
		error = sceNetPsSleep(epoll, sleepFlags, NULL, delay);
		if (error == ECANCELED) {
			count = 0;
			goto release;
		}
		epoll->flags &= ~SCE_NETPS_EPOLL_WAITING;
		sceNetPsGetSystemTime(&now);
		if (delay > 0) {
			if (timeout - (int)(now - elapsed) >= 0)
				timeout -= now - elapsed;
			else
				timeout = 0;
			elapsed = now - elapsed;
		}
	}
	for (entry = epoll->entries; entry != NULL; entry = entry->next) {
		if (entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) {
			so = entry->target.socket;
			if (so) {
				if (entry->event.events & SCE_NET_EPOLLIN)
					so->so_rcv.sb_flags &= ~SB_SEL;
				if (entry->event.events & SCE_NET_EPOLLOUT)
					so->so_snd.sb_flags &= ~SB_SEL;
			}
		} else if (entry->kind & SCE_NETPS_EPOLL_ENTRY_DESCRIPTOR) {
			descriptor = entry->target.descriptor;
			if (descriptor == NULL)
				continue;
			for (member = descriptor->entries; member != NULL;
			     member = member->next) {
				so = member->socket;
				if (so == NULL)
					continue;
				if (entry->event.events & SCE_NET_EPOLLIN)
					so->so_rcv.sb_flags &= ~SB_SEL;
				if (entry->event.events & SCE_NET_EPOLLOUT)
					so->so_snd.sb_flags &= ~SB_SEL;
			}
		}
	}
	/* a timed-out sleep is a wait that found nothing */
	result = copyout(&timeout, timeoutp, sizeof(timeout));
	if (error == 0 || error == EWOULDBLOCK) {
		error = copyout(ready, events, size);
		if (result)
			error = result;
	} else {
		copyout(ready, events, size);
	}
release:
	sceNetPsFree(ready);
out:
	return sceNetPsUnlockResult(error, count);
fail:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -error;
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

/* SCE: destroy every epoll the process owns */
int sceNetPsDestroyProcessEpolls(SceUID processId)
{
	SceNetPsEpoll *epoll;
	int first, last, epollId;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sceNetPsGetEpollIdRange(&first, &last);
	for (epollId = first; epollId <= last; epollId++) {
		epoll = sceNetPsLookupEpoll(epollId, 1);
		if (epoll && epoll->processId == processId)
			sceNetPsDestroyEpoll(epollId, epoll);
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
}

/*
 * SCE: take the descriptor out of every epoll that watches it, with the
 * select marks of its sockets, and wake the epolls that wait
 */
void sceNetPsRemoveDescriptorFromEpolls(SceNetPsDescriptor *descriptor)
{
	SceNetPsEpoll *epoll;
	SceNetPsEpollEntry *entry;
	SceNetPsDescriptorEntry *member;
	struct socket *so;
	int first, last, epollId;

	sceNetPsGetEpollIdRange(&first, &last);
	for (epollId = first; epollId <= last; epollId++) {
		epoll = sceNetPsLookupEpoll(epollId, 1);
		if (epoll == NULL)
			continue;
		for (entry = epoll->entries; entry != NULL; entry = entry->next) {
			if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_DESCRIPTOR) &&
			    entry->target.descriptor == descriptor)
				break;
		}
		if (entry == NULL)
			continue;
		for (member = descriptor->entries; member != NULL;
		     member = member->next) {
			so = member->socket;
			if (so) {
				so->so_rcv.sb_flags &= ~SB_SEL;
				so->so_snd.sb_flags &= ~SB_SEL;
			}
		}
		if (entry->next)
			entry->next->prev = entry->prev;
		*entry->prev = entry->next;
		sceNetPsFree(entry);
		if (epoll->flags & SCE_NETPS_EPOLL_WAITING)
			sceNetPsWakeupNormal(epoll);
	}
}

/* if_arp.c: the ARP and raw Ethernet sockets */
extern struct inpcbtable arpcbtable;
extern struct inpcbtable isdncbtable;

/*
 * SCE: copy out the socket's sceNetGetSockInfo record when the caller may
 * see it; with no buffer only count it. A record of the identifier walk
 * (overwrite) is written without moving on: the protocol tables write
 * every socket again.
 */
static int sceNetPsWriteSockInfo(int socketId, struct socket *so,
				 SceNetSockInfo **cursor, int capacity,
				 int *count, int flags,
				 SceNetSockInfo *record, SceUID processId,
				 int overwrite, int kernel)
{
	struct inpcb *inp, *p2p;
	struct tcpcb *tp;
	unsigned int recvLength, sendLength, owner;
	int descriptorId, error;

	if (flags & SCE_NET_SOCKINFO_F_OTHERS) {
		if (processId) {
			if (so->processId != processId)
				return 0;
			owner = SCE_NET_SOCKINFO_F_SELF;
		} else {
			owner = SCE_NET_SOCKINFO_F_OTHERS;
		}
	} else {
		if (so->processId == sceNetPsGetProcessId()) {
			owner = SCE_NET_SOCKINFO_F_SELF;
		} else if (so->processId == gNetPsInitialProcessId) {
			if (sceNetPsIsApplicationProgram(NULL))
				return 0;
			owner = SCE_NET_SOCKINFO_F_KERNEL;
		} else {
			if (sceNetPsIsApplicationProgram(NULL))
				return 0;
			owner = SCE_NET_SOCKINFO_F_OTHERS;
		}
		if (!((owner | flags) & SCE_NET_SOCKINFO_F_SELF))
			return 0;
	}
	if (so->so_proto < inetdomain.dom_protosw ||
	    so->so_proto >= inetdomain.dom_protoswNPROTOSW)
		return 0;
	if (*cursor == NULL) {
		(*count)++;
		return 0;
	}
	if (*count >= capacity)
		return 0;
	memset(record, 0, sizeof(*record));
	if (flags & SCE_NET_SOCKINFO_F_KERNEL)
		memcpy(record->name, so->name, sizeof(record->name));
	record->pid = so->reportedProcessOverride != -1 ?
		      so->reportedProcessOverride : so->processId;
	descriptorId = sceNetPsFindSocketDescriptor(so);
	record->s = descriptorId < 0 ? socketId : descriptorId;
	inp = sotoinpcb(so);
	tp = NULL;
	p2p = NULL;
	switch (so->so_type) {
	case SCE_NET_SOCK_DGRAM_P2P:
		p2p = inp;
		break;
	case SCE_NET_SOCK_STREAM_P2P:
		/* a stream without its pcb is closing: its record is kept */
		if (inp == NULL) {
			overwrite = 0;
			break;
		}
		tp = intotcpcb(inp);
		p2p = inp;
		break;
	case SOCK_STREAM:
		if (inp == NULL) {
			overwrite = 0;
			break;
		}
		tp = intotcpcb(inp);
		break;
	}
	recvLength = so->so_rcv.sb_cc;
	record->recv_queue_length = recvLength;
	sendLength = so->so_snd.sb_cc;
	record->send_queue_length = sendLength;
	if (inp) {
		record->local_adr.s_addr = inp->inp_laddr.s_addr;
		record->local_port = inp->inp_lport;
		record->remote_adr.s_addr = inp->inp_faddr.s_addr;
		record->remote_port = inp->inp_fport;
	}
	if (tp) {
		switch (tp->t_state) {
		case TCPS_CLOSED:
			record->state = SCE_NET_SOCKINFO_STATE_CLOSED;
			break;
		case TCPS_LISTEN:
			record->state = SCE_NET_SOCKINFO_STATE_LISTEN;
			break;
		case TCPS_SYN_SENT:
			record->state = SCE_NET_SOCKINFO_STATE_SYN_SENT;
			break;
		case TCPS_SYN_RECEIVED:
			record->state = SCE_NET_SOCKINFO_STATE_SYN_RECEIVED;
			break;
		case TCPS_ESTABLISHED:
			record->state = SCE_NET_SOCKINFO_STATE_ESTABLISHED;
			break;
		case TCPS_CLOSE_WAIT:
			record->state = SCE_NET_SOCKINFO_STATE_CLOSE_WAIT;
			break;
		case TCPS_FIN_WAIT_1:
			record->state = SCE_NET_SOCKINFO_STATE_FIN_WAIT_1;
			break;
		case TCPS_CLOSING:
			record->state = SCE_NET_SOCKINFO_STATE_CLOSING;
			break;
		case TCPS_LAST_ACK:
			record->state = SCE_NET_SOCKINFO_STATE_LAST_ACK;
			break;
		case TCPS_FIN_WAIT_2:
			record->state = SCE_NET_SOCKINFO_STATE_FIN_WAIT_2;
			break;
		case TCPS_TIME_WAIT:
			record->state = SCE_NET_SOCKINFO_STATE_TIME_WAIT;
			break;
		default:
			record->state = SCE_NET_SOCKINFO_STATE_UNKNOWN;
			break;
		}
		record->socket_type = so->so_type;
		record->policy = so->threadPriorityPolicy;
		if (p2p) {
			record->local_vport = tp->t_local_vport;
			record->remote_vport = tp->t_remote_vport;
		}
		goto flags;
	}
	record->state = SCE_NET_SOCKINFO_STATE_OPENED;
	record->socket_type = so->so_type;
	record->policy = so->threadPriorityPolicy;
	if (p2p) {
		record->local_vport = ((struct udpp2pcb *)p2p)->up_lvport;
		record->remote_vport = ((struct udpp2pcb *)p2p)->up_fvport;
	}
flags:
	if (so->so_rcv.sb_flags & SB_WAIT)
		owner |= SCE_NET_SOCKINFO_F_RECV_WAIT;
	if (so->so_snd.sb_flags & SB_WAIT)
		owner |= SCE_NET_SOCKINFO_F_SEND_WAIT;
	if (so->so_rcv.sb_flags & SB_SEL)
		owner |= SCE_NET_SOCKINFO_F_RECV_EWAIT;
	if (so->so_snd.sb_flags & SB_SEL)
		owner |= SCE_NET_SOCKINFO_F_SEND_EWAIT;
	if (so->so_snd.sb_flags & SB_SCE_CONNECTING)
		owner |= SCE_NET_SOCKINFO_F_WAKEUP_SIGNAL;
	record->flags = owner;
	if (flags & 8) {
		record->pid = htonl(record->pid);
		record->s = htonl(record->s);
		record->recv_queue_length = htonl(recvLength);
		record->send_queue_length = htonl(sendLength);
		record->state = htonl(record->state);
		record->flags = htonl(owner);
	}
	if (flags & 0x10) {
		record->local_port = htons(record->local_port);
		record->remote_port = htons(record->remote_port);
		record->local_vport = htons(record->local_vport);
		record->remote_vport = htons(record->remote_vport);
		record->local_adr.s_addr = htonl(record->local_adr.s_addr);
		record->remote_adr.s_addr = htonl(record->remote_adr.s_addr);
	}
	error = sceNetPsCopyOut(record, *cursor, sizeof(*record), kernel);
	if (error)
		return error;
	if (overwrite)
		return 0;
	(*cursor)++;
	(*count)++;
	return 0;
}

/* SCE: the record of a socket lost to a resume or an address change: its identifier only */
static int sceNetPsWriteLostSockInfo(int socketId, SceNetSockInfo **cursor,
				     int capacity, int *count,
				     SceNetSockInfo *record, int kernel)
{
	int error;

	if (*cursor == NULL) {
		(*count)++;
		return 0;
	}
	if (*count >= capacity)
		return 0;
	memset(record, 0, sizeof(*record));
	record->s = socketId;
	error = sceNetPsCopyOut(record, *cursor, sizeof(*record), kernel);
	if (error == 0) {
		(*cursor)++;
		(*count)++;
	}
	return error;
}

/* SCE: write every socket of one protocol table */
static inline int sceNetPsWriteTableSockInfo(struct inpcbtable *table,
					     SceNetSockInfo **cursor,
					     int capacity, int *count,
					     int flags,
					     SceNetSockInfo *record,
					     SceUID processId, int kernel)
{
	struct inpcb_hdr *inph, *next;
	struct socket *so;
	int error;

	for (inph = CIRCLEQ_FIRST(&table->inpt_queue);
	     inph != (void *)&table->inpt_queue; inph = next) {
		so = ((struct inpcb *)inph)->inp_socket;
		next = CIRCLEQ_NEXT(inph, inph_queue);
		if (so == NULL)
			continue;
		error = sceNetPsWriteSockInfo(sceNetPsFindSocketId(so), so,
					      cursor, capacity, count, flags,
					      record, processId, 0, kernel);
		if (error)
			return error;
	}
	return 0;
}

/*
 * SCE: sceNetGetSockInfo's records: one socket, or every socket by
 * identifier and protocol table, and with flags 0x20 the epolls too.
 * Returns a positive error.
 */
int sceNetPsCollectSockInfo(int socket, SceNetSockInfo *records, int capacity,
			    int flags, int *count, SceUID processId, int kernel)
{
	SceNetSockInfo record;
	SceNetSockInfo *cursor = records;
	struct udpp2pcb *up;
	struct socket *so;
	SceNetPsEpoll *epoll;
	unsigned int owner;
	int first, last, id, error;

	if (capacity < 0)
		return EINVAL;
	error = 0;
	if (socket >= 0) {
		so = sceNetPsLookupSocket(socket, &error, 1);
		if (so)
			return sceNetPsWriteSockInfo(socket, so, &cursor,
						     capacity, count, flags,
						     &record, processId, 0, kernel);
		if (error == SCE_NET_ERESUME || error == SCE_NET_EIPADDRCHANGED)
			return sceNetPsWriteLostSockInfo(socket, &cursor,
							 capacity, count,
							 &record, kernel);
		return error;
	}
	sceNetPsGetSocketIdRange(&first, &last);
	for (id = first; id <= last; id++) {
		so = sceNetPsLookupSocket(id, &error, 1);
		if (so) {
			error = sceNetPsWriteSockInfo(id, so, &cursor, capacity,
						      count, flags, &record,
						      processId, 1, kernel);
			if (error)
				return error;
		} else if (error == SCE_NET_ERESUME ||
			   error == SCE_NET_EIPADDRCHANGED) {
			error = sceNetPsWriteLostSockInfo(id, &cursor, capacity,
							  count, &record, kernel);
			if (error)
				return error;
		}
	}
	error = 0;
	error = sceNetPsWriteTableSockInfo(&tcbtable, &cursor, capacity, count,
					   flags, &record, processId, kernel);
	if (error)
		return error;
	error = sceNetPsWriteTableSockInfo(&udbtable, &cursor, capacity, count,
					   flags, &record, processId, kernel);
	if (error)
		return error;
	LIST_FOREACH(up, &udpp2pcbs, up_list) {
		so = up->up_inpcb.inp_socket;
		if (so == NULL)
			continue;
		error = sceNetPsWriteSockInfo(sceNetPsFindSocketId(so), so,
					      &cursor, capacity, count, flags,
					      &record, processId, 0, kernel);
		if (error)
			return error;
	}
	error = sceNetPsWriteTableSockInfo(&rawcbtable, &cursor, capacity, count,
					   flags, &record, processId, kernel);
	if (error)
		return error;
	error = sceNetPsWriteTableSockInfo(&arpcbtable, &cursor, capacity, count,
					   flags, &record, processId, kernel);
	if (error)
		return error;
	error = sceNetPsWriteTableSockInfo(&isdncbtable, &cursor, capacity,
					   count, flags, &record, processId, kernel);
	if (error)
		return error;
	if (!(flags & 0x20))
		return error;
	sceNetPsGetEpollIdRange(&first, &last);
	for (id = first; id <= last; id++) {
		epoll = sceNetPsLookupEpoll(id, 1);
		if (epoll == NULL)
			continue;
		if (flags & SCE_NET_SOCKINFO_F_OTHERS) {
			if (processId == 0)
				owner = SCE_NET_SOCKINFO_F_OTHERS;
			else if (processId != epoll->processId)
				continue;
			else
				owner = SCE_NET_SOCKINFO_F_SELF;
		} else if (epoll->processId == sceNetPsGetProcessId()) {
			owner = SCE_NET_SOCKINFO_F_SELF;
		} else {
			owner = SCE_NET_SOCKINFO_F_OTHERS;
		}
		if (cursor == NULL) {
			(*count)++;
			continue;
		}
		if (*count >= capacity)
			continue;
		memset(&record, 0, sizeof(record));
		record.s = id;
		record.pid = epoll->processId;
		record.state = SCE_NET_SOCKINFO_STATE_OPENED;
		record.socket_type = 11;	/* an epoll */
		if (flags & SCE_NET_SOCKINFO_F_KERNEL)
			memcpy(record.name, epoll->name, sizeof(record.name));
		record.flags = owner;
		error = sceNetPsCopyOut(&record, cursor, sizeof(record), kernel);
		if (error)
			return error;
		cursor++;
		(*count)++;
	}
	return error;
}
