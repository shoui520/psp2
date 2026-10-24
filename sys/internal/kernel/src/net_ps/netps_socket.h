/*	$NetBSD: socketvar.h,v 1.69.2.1 2004/05/30 07:02:37 tron Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)socketvar.h	8.3 (Berkeley) 2/19/95
 */


/* SCE adaptation: complete socket owners from BOTH TOOL originals.
 * Baseline: NetBSD 2.1 socketvar.h 1.69.2.1; the mixed stack release is unresolved.
 * MBUFTRACE owner pointers retain their upstream identities. Sony omits the
 * conditional tracking stores; they are not newly invented reserved fields.
 */
#ifndef SCE_NETPS_SOCKET_H
#define SCE_NETPS_SOCKET_H

#include <scetypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <stddef.h>
#include <net_ps/netps_gateway.h>
#include <net_ps/netps_thread.h>

struct socket;
struct mbuf;
struct mowner;
struct protosw;
struct uio;
struct SceNetPsEpoll;
struct SceNetPsDescriptor;

TAILQ_HEAD(soqhead, socket);

struct sockbuf {
	struct mowner *sb_mowner;
	u_long sb_cc;
	u_long sb_hiwat;
	u_long sb_mbcnt;
	u_long sb_mbmax;
	long sb_lowat;
	struct mbuf *sb_mb;
	struct mbuf *sb_mbtail;
	struct mbuf *sb_lastrecord;
	int sb_flags;
	int sb_timeo;
};

struct socket {
	short so_type;
	int so_options;
	int so_linger;
	/* SCE_NET_SO_TPPOLICY, application 0..31 / system 32..63. */
	int threadPriorityPolicy;
	u_short so_state;
	void *so_pcb;
	struct protosw *so_proto;
	struct socket *so_head;
	struct soqhead *so_onq;
	struct soqhead so_q0;
	struct soqhead so_q;
	TAILQ_ENTRY(socket) so_qe;
	short so_q0len;
	short so_qlen;
	short so_qlimit;
	short so_timeo;
	u_short so_error;
	/* SCE: in place of so_pgid, the libnetctl error while so_error is SCE_NET_ERETURN */
	int returnedError;
	u_long so_oobmark;
	struct sockbuf so_snd;
	struct sockbuf so_rcv;
	void *so_internal;
	void (*so_upcall)(struct socket *, caddr_t, int);
	caddr_t so_upcallarg;
	int (*so_send)(struct socket *, struct mbuf *, struct uio *,
		       struct mbuf *, struct mbuf *, int);
	int (*so_receive)(struct socket *, struct mbuf **, struct uio *,
			  struct mbuf **, struct mbuf **, int *);
	struct mowner *so_mowner;
	/* SCE extension, replacing upstream so_uid and extending the owner. */
	SceUID processId;
	struct SceNetPsEpoll *epoll;
	struct SceNetPsDescriptor *descriptor;
	char name[32];
	unsigned int filterFlags;
	SceUID reportedProcessOverride;
	SceUID initiatingThreadId;
	unsigned int emulationWaitChannel;
	int refCount;
};

_Static_assert(offsetof(struct sockbuf, sb_mowner) == 0,
	       "sockbuf sb_mowner");
_Static_assert(offsetof(struct sockbuf, sb_cc) == 4,
	       "sockbuf sb_cc");
_Static_assert(offsetof(struct sockbuf, sb_hiwat) == 8,
	       "sockbuf sb_hiwat");
_Static_assert(offsetof(struct sockbuf, sb_mbcnt) == 12,
	       "sockbuf sb_mbcnt");
_Static_assert(offsetof(struct sockbuf, sb_mbmax) == 16,
	       "sockbuf sb_mbmax");
_Static_assert(offsetof(struct sockbuf, sb_lowat) == 20,
	       "sockbuf sb_lowat");
_Static_assert(offsetof(struct sockbuf, sb_mb) == 24,
	       "sockbuf sb_mb");
_Static_assert(offsetof(struct sockbuf, sb_mbtail) == 28,
	       "sockbuf sb_mbtail");
_Static_assert(offsetof(struct sockbuf, sb_lastrecord) == 32,
	       "sockbuf sb_lastrecord");
_Static_assert(offsetof(struct sockbuf, sb_flags) == 36,
	       "sockbuf sb_flags");
_Static_assert(offsetof(struct sockbuf, sb_timeo) == 40,
	       "sockbuf sb_timeo");
_Static_assert(sizeof(struct sockbuf) == 44, "sockbuf size");

_Static_assert(offsetof(struct socket, so_type) == 0,
	       "socket so_type");
_Static_assert(offsetof(struct socket, so_options) == 4,
	       "socket so_options");
_Static_assert(offsetof(struct socket, so_linger) == 8,
	       "socket so_linger");
_Static_assert(offsetof(struct socket, threadPriorityPolicy) == 12,
	       "socket threadPriorityPolicy");
_Static_assert(offsetof(struct socket, so_state) == 16,
	       "socket so_state");
_Static_assert(offsetof(struct socket, so_pcb) == 20,
	       "socket so_pcb");
_Static_assert(offsetof(struct socket, so_proto) == 24,
	       "socket so_proto");
_Static_assert(offsetof(struct socket, so_head) == 28,
	       "socket so_head");
_Static_assert(offsetof(struct socket, so_onq) == 32,
	       "socket so_onq");
_Static_assert(offsetof(struct socket, so_q0) == 36,
	       "socket so_q0");
_Static_assert(offsetof(struct socket, so_q) == 44,
	       "socket so_q");
_Static_assert(offsetof(struct socket, so_qe) == 52,
	       "socket so_qe");
_Static_assert(offsetof(struct socket, so_q0len) == 60,
	       "socket so_q0len");
_Static_assert(offsetof(struct socket, so_qlen) == 62,
	       "socket so_qlen");
_Static_assert(offsetof(struct socket, so_qlimit) == 64,
	       "socket so_qlimit");
_Static_assert(offsetof(struct socket, so_timeo) == 66,
	       "socket so_timeo");
_Static_assert(offsetof(struct socket, so_error) == 68,
	       "socket so_error");
_Static_assert(offsetof(struct socket, returnedError) == 72,
	       "socket returnedError");
_Static_assert(offsetof(struct socket, so_oobmark) == 76,
	       "socket so_oobmark");
_Static_assert(offsetof(struct socket, so_snd) == 80,
	       "socket so_snd");
_Static_assert(offsetof(struct socket, so_rcv) == 124,
	       "socket so_rcv");
_Static_assert(offsetof(struct socket, so_internal) == 168,
	       "socket so_internal");
_Static_assert(offsetof(struct socket, so_upcall) == 172,
	       "socket so_upcall");
_Static_assert(offsetof(struct socket, so_upcallarg) == 176,
	       "socket so_upcallarg");
_Static_assert(offsetof(struct socket, so_send) == 180,
	       "socket so_send");
_Static_assert(offsetof(struct socket, so_receive) == 184,
	       "socket so_receive");
_Static_assert(offsetof(struct socket, so_mowner) == 188,
	       "socket so_mowner");
_Static_assert(offsetof(struct socket, processId) == 192,
	       "socket processId");
_Static_assert(offsetof(struct socket, epoll) == 196,
	       "socket epoll");
_Static_assert(offsetof(struct socket, descriptor) == 200,
	       "socket descriptor");
_Static_assert(offsetof(struct socket, name) == 204,
	       "socket name");
_Static_assert(offsetof(struct socket, filterFlags) == 236,
	       "socket filterFlags");
_Static_assert(offsetof(struct socket, reportedProcessOverride) == 240,
	       "socket reportedProcessOverride");
_Static_assert(offsetof(struct socket, initiatingThreadId) == 244,
	       "socket initiatingThreadId");
_Static_assert(offsetof(struct socket, emulationWaitChannel) == 248,
	       "socket emulationWaitChannel");
_Static_assert(offsetof(struct socket, refCount) == 252,
	       "socket refCount");
_Static_assert(sizeof(struct socket) == 256, "socket size");

_Static_assert(sizeof(struct soqhead) == 8, "soqhead size");
_Static_assert(offsetof(struct soqhead, tqh_first) == 0, "soqhead first");
_Static_assert(offsetof(struct soqhead, tqh_last) == 4, "soqhead last");

void sceNetPsSetSocket(unsigned int socketId, struct socket *socket);
struct socket *sceNetPsLookupSocket(unsigned int socketId, int *error,
				  unsigned int flags);

struct SceNetPsDriverContext;

/* net_ps_socket.c */
int sceNetPsNotifyGatewayDns(const SceNetPsDnsConfig *config);
struct msghdr;
int sceNetPsRecvit(int s, struct msghdr *mp, void *namelenp, int *retsize,
		   int kernel);
int sceNetPsConnectSocket(int s, struct socket *so, const void *address,
			  unsigned int addressLength, int *errorp, int kernel);
unsigned int sceNetPsGetSockinfo(int socket, void *buffer, int capacity, int flags);
int sceNetPsGetIfList(void *list, int count);
int sceNetPsControl(int interfaceIndex, int command, void *data, int length);
int sceNetPsStackControl(struct SceNetPsDriverContext *context, int command,
			 void *data, int length);
int sceNetPsSysctl(const int *name, unsigned int nameLength, void *oldData,
		   unsigned int *oldLength, const void *newData,
		   unsigned int newLength);
int sceNetPsDescriptorCreate(const char *name, int flags);
int sceNetPsDescriptorClose(int descriptorId);
int sceNetPsDumpClose(int dumpId);
int sceNetPsDumpAbort(int dumpId, int flags);
void bnet_socket_sanity_check(struct socket *socket);
void sceNetPsWakeupEpolls(struct socket *socket);
int sceNetPsCopyString(const char *source, char *destination, size_t length,
		       size_t *copied, int kernel);
int sceNetPsEpollCreate(const char *name, int flags);
int sceNetPsEpollClose(int epollId);
int sceNetPsEpollAbort(int epollId, int flags);

struct ifnet;

/* net_ps_glue.c */
int sceNetPsRequestAutoConnect(struct socket *so, struct mbuf *m,
			       int checkStates);
int sceNetPsSetNoAutoConnect(struct mbuf *m, struct ifnet *ifp,
			     struct socket *so);
int sceNetPsCheckDisabledInterface(struct mbuf *m, struct ifnet *ifp,
				   struct socket *so);
int sceNetPsInterfaceControl(int interfaceIndex, int command, void *data,
			     int length);

/* net_ps_emulation.c */
int sceNetPsFilterTxSocket(struct mbuf *m, struct ifnet *ifp,
			   struct socket *so);
int sceNetPsFilterRxSocket(struct mbuf *m, struct ifnet *ifp,
			   struct socket *so);

#endif

