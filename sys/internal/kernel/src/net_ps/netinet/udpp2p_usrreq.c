/*
 * SCE: udpp2p, the SOCK_DGRAM_P2P protocol and the carrier of SOCK_STREAM_P2P
 * segments: datagrams between virtual ports inside UDP, from one process's
 * port to another's, optionally encrypted (AES-128) and signed (HMAC-SHA1)
 * with the key of the peer, and tagged with the sending process's tag.
 * Sony's file, after NetBSD's udp_usrreq.c (its BSD licence:
 * contrib/netbsd-2.1/LICENSE).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udpp2p_var.h>

#include <net/socket.h>		/* SCE_NET_ADHOC_PORT */

#include "../net_ps_internal.h"
#include "../netps_socket.h"

extern int udpcksum;			/* udp_usrreq.c */

#define	UDPP2P_SENDSPACE	9216
#define	UDPP2P_RECVSPACE	124800

/* the virtual ports an anonymous bind chooses from, an application's and the system's */
#define	UDPP2P_APPPORT_FIRST	32768
#define	UDPP2P_APPPORT_LAST	39999
#define	UDPP2P_PORT_FIRST	40000
#define	UDPP2P_PORT_LAST	49999

/* what one mbuf of an encrypted packet holds */
#define	UDPP2P_CRYPTCHUNK	1600

/* the address an ad hoc broadcast is sent to */
#define	UDPP2P_ADHOC_BROADCAST	0xffffFEA9	/* 169.254.255.255 */

/* udpp2p_crypt_output's state */
struct udpp2p_encrypt {
	int	  ue_off;		/* bytes copied so far */
	int	  ue_hdrlen;		/* the IV and signature words to prepend */
	struct mbuf *ue_top;		/* the copy */
	struct mbuf *ue_tail;
	struct mbuf *ue_cur;
	u_int32_t ue_iv[4];
	SceHmacSha1Context ue_hmac __attribute__((__packed__, __aligned__(4)));
	u_char	  ue_digest[20];
};

/* udpp2p_crypt_input's state */
struct udpp2p_decrypt {
	struct mbuf *ud_top;		/* the plain copy */
	struct mbuf *ud_tail;
	struct mbuf *ud_cur;
	int	  ud_crypto;		/* the packet is encrypted */
	int	  ud_sign;		/* the packet is signed */
	int	  ud_off;
	int	  ud_chunk;
	int	  ud_resid;
	u_char	  ud_iv[16];
	SceHmacSha1Context ud_hmac __attribute__((__packed__, __aligned__(4)));
	u_char	  ud_mac[8];		/* the signature received */
	u_char	  ud_digest[20];	/* the signature computed */
	u_int	  ud_ivlen;
	u_int	  ud_maclen;
};

LIST_HEAD(, udpp2pcb) udpp2pcbs;
struct pool udpp2pcb_pool;
in_port_t udpp2p_lastport;
struct udpp2pstat udpp2pstat;
LIST_HEAD(, udpp2p_proc) udpp2p_procs;
in_port_t udpp2p_applastport;
int udpp2pcb_pool_initialized;

struct udpp2pcb *
udpp2p_pcblookup_port(struct in_addr laddr, in_port_t lport, in_port_t lvport,
    int lookup_wildcard, SceUID pid)
{
	struct udpp2pcb *up, *match = NULL;
	struct inpcb *inp;
	int matchwild = 3, wildcard;

	LIST_FOREACH(up, &udpp2pcbs, up_list) {
		inp = &up->up_inpcb;
		if (inp->inp_af != AF_INET)
			continue;

		if (inp->inp_lport != lport || up->up_lvport != lvport ||
		    up->up_pid != pid)
			continue;
		wildcard = 0;
		if (!in_nullhost(inp->inp_faddr))
			wildcard++;
		if (in_nullhost(inp->inp_laddr)) {
			if (!in_nullhost(laddr))
				wildcard++;
		} else {
			if (in_nullhost(laddr))
				wildcard++;
			else {
				if (!in_hosteq(inp->inp_laddr, laddr))
					continue;
			}
		}
		if (wildcard && !lookup_wildcard)
			continue;
		if (wildcard < matchwild) {
			match = up;
			matchwild = wildcard;
			if (matchwild == 0)
				break;
		}
	}
	return (match);
}

struct udpp2pcb *
udpp2p_pcblookup_connect(struct in_addr faddr, in_port_t fport,
    in_port_t fvport, struct in_addr laddr, in_port_t lport, in_port_t lvport,
    SceUID pid)
{
	struct udpp2pcb *up, *match = NULL;
	struct inpcb *inp;

	LIST_FOREACH(up, &udpp2pcbs, up_list) {
		inp = &up->up_inpcb;
		if (inp->inp_af == AF_INET &&
		    in_hosteq(inp->inp_faddr, faddr) &&
		    inp->inp_fport == fport &&
		    inp->inp_lport == lport &&
		    in_hosteq(inp->inp_laddr, laddr) &&
		    up->up_fvport == fvport &&
		    up->up_lvport == lvport) {
			if (up->up_pid == pid)
				return (up);
			match = up;
		}
	}
	return (match);
}

struct udpp2pcb *
udpp2p_pcblookup_bind(struct in_addr laddr, in_port_t lport, in_port_t lvport,
    SceUID pid)
{
	struct udpp2pcb *up, *match = NULL;
	struct inpcb *inp;

	LIST_FOREACH(up, &udpp2pcbs, up_list) {
		inp = &up->up_inpcb;
		if (inp->inp_af == AF_INET && inp->inp_lport == lport &&
		    (in_hosteq(inp->inp_laddr, laddr) ||
		     in_hosteq(inp->inp_laddr, zeroin_addr)) &&
		    (up->up_lvport == lvport || lvport == 0)) {
			match = up;
			if (up->up_pid == pid)
				return (up);
		}
	}
	return (match);
}

/*
 * Encrypt and sign a packet with the peer's key, in place when it is one mbuf
 * of at most UDPP2P_CRYPTCHUNK bytes, else in a copy, and prepend the IV and
 * the signature (four bytes each).
 */
int
udpp2p_crypt_output(struct mbuf **mp, int crypto, int sign,
    struct udpp2p_peer *peer)
{
	struct mbuf *m = *mp, *n;
	struct udpp2p_encrypt *ue;
	int error, len;
	u_int32_t iv;

	ue = malloc(sizeof(*ue), M_TEMP, M_NOWAIT|M_ZERO);
	if (ue == NULL)
		return (ENOMEM);
	if (crypto) {
		if (m->m_pkthdr.len == m->m_len &&
		    m->m_pkthdr.len <= UDPP2P_CRYPTCHUNK) {
			iv = arc4random();
			ue->ue_iv[0] = ue->ue_iv[1] = ue->ue_iv[2] =
			    ue->ue_iv[3] = iv;
			ue->ue_cur = m;
			ue->ue_hdrlen += 4;
		} else {
			for (ue->ue_off = 0; ue->ue_off < m->m_pkthdr.len;
			    ue->ue_off += len) {
				len = m->m_pkthdr.len - ue->ue_off;
				if (len >= UDPP2P_CRYPTCHUNK)
					len = UDPP2P_CRYPTCHUNK;
				if (ue->ue_top != NULL) {
					MGET(ue->ue_cur, M_DONTWAIT, MT_DATA);
					if (ue->ue_cur == NULL) {
						error = ENOBUFS;
						m_freem(ue->ue_top);
						goto out;
					}
					if (len > MLEN) {
						MCLGET(ue->ue_cur, M_DONTWAIT);
						if ((ue->ue_cur->m_flags & M_EXT) == 0) {
							error = ENOBUFS;
							m_free(ue->ue_cur);
							m_freem(ue->ue_top);
							goto out;
						}
					}
				} else {
					MGETHDR(ue->ue_cur, M_DONTWAIT, MT_DATA);
					if (ue->ue_cur == NULL)
						goto nobufs;
					M_COPY_PKTHDR(ue->ue_cur, m);
					ue->ue_cur->m_data = ue->ue_cur->m_pktdat;
					if (len > MHLEN) {
						MCLGET(ue->ue_cur, M_DONTWAIT);
						if ((ue->ue_cur->m_flags & M_EXT) == 0) {
							error = ENOBUFS;
							m_free(ue->ue_cur);
							goto out;
						}
					}
				}
				m_copydata(m, ue->ue_off, len,
				    mtod(ue->ue_cur, caddr_t));
				ue->ue_cur->m_len = len;
				if (ue->ue_top != NULL) {
					ue->ue_tail->m_next = ue->ue_cur;
					ue->ue_tail = ue->ue_cur;
				} else
					ue->ue_top = ue->ue_tail = ue->ue_cur;
			}
			m_freem(m);
			m = ue->ue_top;
			iv = arc4random();
			ue->ue_cur = m;
			ue->ue_iv[0] = ue->ue_iv[1] = ue->ue_iv[2] =
			    ue->ue_iv[3] = iv;
			ue->ue_hdrlen += 4;
			if (m == NULL)
				goto signit;
		}
		for (n = m; ; ) {
			if (sceNetPsAesEncrypt(mtod(n, u_char *),
			    mtod(n, u_char *), n->m_len, peer->upp_key, 128,
			    (u_char *)ue->ue_iv) != 0)
				goto inval;
			n = ue->ue_cur = ue->ue_cur->m_next;
			if (n == NULL)
				break;
		}
	}
signit:
	if (sign) {
		memset(ue->ue_digest, 0, sizeof(ue->ue_digest));
		if (sceNetPsHmacSha1Init(&ue->ue_hmac, peer->upp_key,
		    sizeof(peer->upp_key)) != 0)
			goto inval;
		for (ue->ue_cur = m; ue->ue_cur != NULL;
		    ue->ue_cur = ue->ue_cur->m_next)
			if (sceNetPsHmacSha1Update(&ue->ue_hmac,
			    mtod(ue->ue_cur, caddr_t), ue->ue_cur->m_len) != 0)
				goto inval;
		if (sceNetPsHmacSha1Result(ue->ue_digest, &ue->ue_hmac) != 0)
			goto inval;
		ue->ue_hdrlen += 4;
	}
	M_PREPEND(m, ue->ue_hdrlen, M_DONTWAIT);
	if (m == NULL)
		goto nobufs;
	if (crypto && sign) {
		error = 0;
		memcpy(mtod(m, caddr_t), ue->ue_iv, 4);
		memcpy(mtod(m, caddr_t) + 4, ue->ue_digest, 4);
	} else if (crypto) {
		error = 0;
		memcpy(mtod(m, caddr_t), ue->ue_iv, 4);
	} else if (sign) {
		error = 0;
		memcpy(mtod(m, caddr_t), ue->ue_digest, 4);
	} else
		error = 0;
out:
	*mp = m;
	free(ue, M_TEMP);
	return (error);

nobufs:
	error = ENOBUFS;
	goto out;
inval:
	error = EINVAL;
	goto out;
}

void
udpp2p_peer_free(struct udpp2p_peer *peer)
{

	LIST_REMOVE(peer, upp_list);
	tcp_drop_peer(&peer->upp_addr);
	memset(peer->upp_key, 0, sizeof(peer->upp_key));
	free(peer, M_PCB);
}

void
udpp2p_ctx_clear(struct udpp2p_ctx *ctx)
{
	struct udpp2p_peer *peer;

	ctx->upc_flags = 0;
	memset(ctx->upc_key, 0, sizeof(ctx->upc_key));
	while ((peer = LIST_FIRST(&ctx->upc_peers)) != NULL)
		udpp2p_peer_free(peer);
	LIST_INIT(&ctx->upc_peers);
}

/* a peer by its address (the loopback address for one of ours) and UDP port */
struct udpp2p_peer *
udpp2p_peer_lookup(struct udpp2p_peers *head, struct in_addr addr, in_port_t port,
    int bcast)
{
	struct in_ifaddr *ia;
	struct udpp2p_peer *peer;

	TAILQ_FOREACH(ia, &in_ifaddrhead, ia_list)
		if (in_hosteq(ia->ia_addr.sin_addr, addr))
			break;
	if (ia != NULL)
		addr.s_addr = INADDR_LOOPBACK;
	LIST_FOREACH(peer, head, upp_list)
		if (in_hosteq(peer->upp_addr.sin_addr, addr) &&
		    peer->upp_addr.sin_port == port &&
		    peer->upp_bcast == bcast)
			break;
	return (peer);
}

int
udpp2p_peer_set(struct udpp2p_peerreq *req, struct udpp2p_ctx *ctx)
{
	struct udpp2p_peer *peer;
	int bcast = req->upq_flags & UPQ_BCAST;

	peer = udpp2p_peer_lookup(&ctx->upc_peers, req->upq_addr.sin_addr,
	    req->upq_addr.sin_port, bcast);
	if (peer != NULL) {
		if (memcmp(peer->upp_key, req->upq_key,
		    sizeof(peer->upp_key)) == 0) {
			peer->upp_refs++;
			return (0);
		}
		memcpy(peer->upp_key, req->upq_key, sizeof(peer->upp_key));
		peer->upp_refs = 1;
		peer->upp_group = req->upq_group;
		return (0);
	}
	peer = malloc(sizeof(*peer), M_PCB, M_NOWAIT);
	if (peer == NULL)
		return (ENOMEM);
	memset(peer, 0, sizeof(*peer));
	peer->upp_addr.sin_addr = req->upq_addr.sin_addr;
	peer->upp_addr.sin_port = req->upq_addr.sin_port;
	peer->upp_lport = req->upq_lport != 0 ? req->upq_lport : ctx->upc_lport;
	peer->upp_pid = ctx->upc_pid;
	memcpy(peer->upp_key, req->upq_key, sizeof(peer->upp_key));
	peer->upp_refs = 1;
	peer->upp_bcast = bcast;
	peer->upp_group = req->upq_group;
	LIST_INSERT_HEAD(&ctx->upc_peers, peer, upp_list);
	return (0);
}

void
udpp2p_detach(struct udpp2pcb *up)
{
	struct inpcb *inp = &up->up_inpcb;
	struct socket *so = inp->inp_socket;

	if (inp->inp_af != AF_INET)
		return;
	so->so_pcb = 0;
	sofree(so);
	if (inp->inp_options)
		(void)m_free(inp->inp_options);
	if (inp->inp_route.ro_rt)
		rtfree(inp->inp_route.ro_rt);
	ip_freemoptions(inp->inp_moptions);
	inp->inp_moptions = NULL;
	inp->inp_state = INP_ATTACHED;
	LIST_REMOVE(up, up_list);
	pool_put(&udpp2pcb_pool, up);
}

void
udpp2p_disconnect(struct udpp2pcb *up)
{
	struct inpcb *inp = &up->up_inpcb;

	inp->inp_state = INP_BOUND;
	inp->inp_fport = 0;
	inp->inp_faddr = zeroin_addr;
	up->up_fvport = 0;
	if (inp->inp_socket->so_state & SS_NOFDREF)
		udpp2p_detach(up);
}

int
udpp2p_connect(struct udpp2pcb *up, struct sockaddr_in *sin, int *lenp)
{
	struct inpcb *inp = &up->up_inpcb;
	struct in_ifaddr *ia = NULL;
	struct sockaddr_in *ifaddr = NULL;
	SceUID pid = sceNetPsGetProcessId();
	int error;

	if (inp->inp_af != AF_INET)
		return (EINVAL);

	if (*lenp != sizeof (*sin))
		return (EINVAL);
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (in_nullhost(sin->sin_addr))
		return (EADDRNOTAVAIL);
	if (IN_MULTICAST(sin->sin_addr.s_addr))
		return (EADDRNOTAVAIL);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);
	if (sin->sin_vport == 0)
		return (EADDRNOTAVAIL);
	if (inp->inp_lport == 0)
		return (EADDRNOTAVAIL);
	if (up->up_lvport == 0)
		return (EADDRNOTAVAIL);
	if (sin->sin_addr.s_addr == INADDR_BROADCAST &&
	    (inp->inp_socket->so_options & SO_ONESBCAST) == 0) {
		/* the broadcast address of the multicast interface, as in_pcbconnect */
		struct ifnet *ifp = NULL;

		if (inp->inp_moptions != NULL)
			ifp = inp->inp_moptions->imo_multicast_ifp;
		TAILQ_FOREACH(ia, &in_ifaddrhead, ia_list) {
			int flags = ia->ia_ifp->if_flags;

			if ((flags & IFF_BROADCAST) != 0 &&
			    (ifp == NULL ||
			     ifp->if_index == ia->ia_ifp->if_index)) {
				sin->sin_addr = ia->ia_broadaddr.sin_addr;
				if (flags & IFF_UP)
					break;
			}
		}
	}
	if (in_nullhost(inp->inp_laddr)) {
		ifaddr = in_selectsrc(sin, &inp->inp_route,
		    inp->inp_socket->so_options, inp->inp_moptions, &error);
		if (ifaddr == NULL) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			return (error);
		}
		INADDR_TO_IA(ifaddr->sin_addr, ia);
		if (ia == NULL)
			return (EADDRNOTAVAIL);
	}
	if (udpp2p_pcblookup_connect(sin->sin_addr, sin->sin_port,
	    sin->sin_vport,
	    !in_nullhost(inp->inp_laddr) ? inp->inp_laddr : ifaddr->sin_addr,
	    inp->inp_lport, up->up_lvport, pid) != NULL)
		return (EADDRINUSE);
	if (in_nullhost(inp->inp_laddr))
		inp->inp_laddr = ifaddr->sin_addr;
	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;
	up->up_fvport = sin->sin_vport;
	up->up_pid = pid;
	inp->inp_state = INP_CONNECTED;
	return (0);
}

/* tcp_p2p_input's lookup, which cannot see net_ps's process table */
struct udpp2p_ctx *
udpp2p_getctx(SceUID pid)
{

	return (sceNetPsGetProtocolContext(pid));
}

void
udpp2p_init(void)
{

	if (udpp2pcb_pool_initialized == 0) {
		pool_init(&udpp2pcb_pool, sizeof(struct udpp2pcb), 0, 0, 0,
		    "udpp2pcbpl", NULL);
		udpp2pcb_pool_initialized = 1;
	}
	LIST_INIT(&udpp2pcbs);
	udpp2p_applastport = UDPP2P_APPPORT_FIRST + arc4random() %
	    (UDPP2P_APPPORT_LAST - UDPP2P_APPPORT_FIRST + 1);
	udpp2p_lastport = UDPP2P_PORT_FIRST + arc4random() %
	    (UDPP2P_PORT_LAST - UDPP2P_PORT_FIRST + 1);
	memset(&udpp2pstat, 0, sizeof(udpp2pstat));
	LIST_INIT(&udpp2p_procs);
}

int
udpp2p_gettag(struct mbuf *m, int *offp, int *lenp, void *tag)
{

	if (m->m_pkthdr.len < *offp + UDPP2P_TAGLEN)
		return (-1);
	m_copydata(m, *offp, UDPP2P_TAGLEN, tag);
	*offp += UDPP2P_TAGLEN;
	*lenp -= UDPP2P_TAGLEN;
	return (UDPP2P_TAGLEN);
}

int
udpp2p_checktag(const void *tag, size_t len, struct socket *so)
{
	struct udpp2p_proc *pr;

	if (so == NULL)
		return (-1);
	LIST_FOREACH(pr, &udpp2p_procs, upr_list)
		if (pr->upr_pid == so->processId)
			break;
	if (pr == NULL)
		return (-1);
	if (memcmp(tag, pr->upr_tag, len) != 0)
		return (-1);
	return (0);
}

/* a SOCK_STREAM_P2P segment: udpp2p's header and tcp_p2p_input */
int
udpp2p_tcpinput(struct sockaddr_in *src, struct sockaddr_in *dst,
    struct mbuf *m, int off)
{
	u_int8_t magic, type;
	u_int16_t len;
	int hoff, delivered;

	if (off + sizeof(struct udphdr) + 2 > m->m_pkthdr.len)
		return (0);
	m_copydata(m, off + sizeof(struct udphdr), 1, (caddr_t)&magic);
	if (magic != UDPP2P_MAGIC)
		return (0);
	m_copydata(m, off + sizeof(struct udphdr) + 1, 1, (caddr_t)&type);
	hoff = off + sizeof(struct udphdr) + 2;
	if (type & UDPP2P_F_NOLEN)
		len = m->m_pkthdr.len - hoff;
	else {
		if (hoff + sizeof(len) > m->m_pkthdr.len)
			return (0);
		m_copydata(m, hoff, sizeof(len), (caddr_t)&len);
		hoff += sizeof(len);
		len = ntohs(len);
		if (hoff + len > m->m_pkthdr.len) {
			udpp2pstat.ups_badlen++;
			return (0);
		}
	}
	if ((type & UDPP2P_TYPE) < UDPP2P_T_STREAM ||
	    (type & UDPP2P_TYPE) > UDPP2P_T_STREAM_CRYPTOSIG)
		return (0);
	delivered = tcp_p2p_input(src, dst, m, hoff, len, type);
	if (delivered)
		udpp2pstat.ups_tcppackets++;
	return (delivered);
}

/* send a datagram from a SOCK_DGRAM_P2P socket, as udp_output */
int
udpp2p_output(struct mbuf *m, ...)
{
	struct udpp2pcb *up;
	struct inpcb *inp;
	struct socket *so;
	struct udpp2p_proc *pr;
	struct udpp2p_ctx *ctx;
	struct udpp2p_peer *peer;
	struct udpiphdr *ui;
	struct in_addr faddr;
	in_port_t fport;
	u_char *hdr, *p;
	int len, hlen, error = 0;
	va_list ap;

	va_start(ap, m);
	up = va_arg(ap, struct udpp2pcb *);
	va_end(ap);
	inp = &up->up_inpcb;

	so = inp->inp_socket;
	if (so == NULL) {
		error = EBADF;
		goto release;
	}
	LIST_FOREACH(pr, &udpp2p_procs, upr_list)
		if (pr->upr_pid == so->processId)
			break;
	ctx = sceNetPsGetProtocolContext(so->processId);
	if (ctx != NULL) {
		faddr = inp->inp_faddr;
		fport = inp->inp_fport;
		if (faddr.s_addr == UDPP2P_ADHOC_BROADCAST) {
			faddr.s_addr = INADDR_BROADCAST;
			fport = 0;
		}
		peer = udpp2p_peer_lookup(&ctx->upc_peers, faddr, fport, 0);
		if ((so->so_options & (SO_USECRYPTO|SO_USESIGNATURE)) &&
		    peer == NULL)
			goto release;
	} else {
		if (so->so_options & (SO_USECRYPTO|SO_USESIGNATURE))
			goto release;
		peer = NULL;
	}
	error = udpp2p_crypt_output(&m, so->so_options & SO_USECRYPTO,
	    so->so_options & SO_USESIGNATURE, peer);
	if (error)
		goto release;

	/* the type, the virtual ports and the tag */
	if (ntohs(up->up_lvport) > 0xff || ntohs(up->up_fvport) > 0xff)
		hlen = 6;
	else
		hlen = 4;
	if (pr != NULL)
		hlen += UDPP2P_TAGLEN;
	M_PREPEND(m, hlen, M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto release;
	}
	if (m->m_pkthdr.len + up->up_reserve + sizeof(struct udpiphdr) >
	    IP_MAXPACKET) {
		error = EMSGSIZE;
		goto release;
	}
	hdr = mtod(m, u_char *);
	hdr[0] = UDPP2P_MAGIC;
	if ((so->so_options & (SO_USECRYPTO|SO_USESIGNATURE)) ==
	    (SO_USECRYPTO|SO_USESIGNATURE))
		hdr[1] = UDPP2P_F_NOLEN | UDPP2P_T_DGRAM_CRYPTOSIG;
	else if (so->so_options & SO_USECRYPTO)
		hdr[1] = UDPP2P_F_NOLEN | UDPP2P_T_DGRAM_CRYPTO;
	else if (so->so_options & SO_USESIGNATURE)
		hdr[1] = UDPP2P_F_NOLEN | UDPP2P_T_DGRAM_SIG;
	else
		hdr[1] = UDPP2P_F_NOLEN | UDPP2P_T_DGRAM;
	if (ntohs(up->up_lvport) > 0xff || ntohs(up->up_fvport) > 0xff) {
		*(in_port_t *)(mtod(m, u_char *) + 2) = up->up_lvport;
		*(in_port_t *)(mtod(m, u_char *) + 4) = up->up_fvport;
		p = mtod(m, u_char *) + 6;
	} else {
		hdr[1] |= UDPP2P_F_SHORTPORT;
		mtod(m, u_char *)[2] = ntohs(up->up_lvport);
		mtod(m, u_char *)[3] = ntohs(up->up_fvport);
		p = mtod(m, u_char *) + 4;
	}
	if (pr != NULL) {
		hdr[1] |= UDPP2P_F_TAG;
		memcpy(p, pr->upr_tag, UDPP2P_TAGLEN);
	}

	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP headers.
	 */
	len = m->m_pkthdr.len;
	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto release;
	}

	/*
	 * Fill in mbuf with extended UDP header
	 * and addresses and length put into network format.
	 */
	ui = mtod(m, struct udpiphdr *);
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_src = inp->inp_laddr;
	ui->ui_dst = inp->inp_faddr;
	ui->ui_sport = inp->inp_lport;
	ui->ui_dport = inp->inp_fport;
	/* from the ad hoc port, the peer's or the process's UDP port */
	if (ui->ui_sport == htons(SCE_NET_ADHOC_PORT)) {
		if (peer != NULL && peer->upp_lport != 0)
			ui->ui_sport = peer->upp_lport;
		else if (ctx != NULL)
			ui->ui_sport = ctx->upc_lport;
	}
	ui->ui_ulen = htons((u_int16_t)len + sizeof(struct udphdr));

	/*
	 * Set up checksum and output datagram.
	 */
	if (udpcksum) {
		/*
		 * XXX Cache pseudo-header checksum part for
		 * XXX "connected" UDP sockets.
		 */
		ui->ui_sum = in_cksum_phdr(ui->ui_src.s_addr,
		    ui->ui_dst.s_addr, htons((u_int16_t)len +
		    sizeof(struct udphdr) + IPPROTO_UDP));
		m->m_pkthdr.csum_flags = M_CSUM_UDPv4;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
	} else
		ui->ui_sum = 0;
	((struct ip *)ui)->ip_len = htons(sizeof (struct udpiphdr) + len);
	((struct ip *)ui)->ip_ttl = inp->inp_ip.ip_ttl;	/* XXX */
	((struct ip *)ui)->ip_tos = inp->inp_ip.ip_tos;	/* XXX */
	udpp2pstat.ups_opackets++;

	return (ip_output(m, inp->inp_options, &inp->inp_route,
	    (so->so_options & (SO_DONTROUTE | SO_BROADCAST)) |
	    ((inp->inp_flags & INP_DONTFRAG) ? IP_MTUDISC : 0),
	    inp->inp_moptions, so));

release:
	m_freem(m);
	return (error);
}

/* send a SOCK_STREAM_P2P segment of so from src to dst inside UDP */
int
udpp2p_tcp_output(struct sockaddr_in *src, struct sockaddr_in *dst,
    struct mbuf *m, struct route *ro, struct socket *so, int crypto, int sign)
{
	struct udpp2p_proc *pr;
	struct udpp2p_ctx *ctx;
	struct udpp2p_peer *peer;
	struct inpcb *inp;
	struct udpiphdr *ui;
	u_char *hdr;
	int len, hlen, error;

	pr = NULL;
	if (so != NULL)
		LIST_FOREACH(pr, &udpp2p_procs, upr_list)
			if (pr->upr_pid == so->processId)
				break;
	ctx = sceNetPsGetProtocolContext(so->processId);
	if (ctx != NULL) {
		peer = udpp2p_peer_lookup(&ctx->upc_peers, dst->sin_addr, dst->sin_port, 0);
		if ((crypto || sign) && peer == NULL)
			goto drop;
	} else {
		if (crypto || sign)
			goto drop;
		peer = NULL;
	}
	error = udpp2p_crypt_output(&m, crypto, sign, peer);
	if (error)
		goto release;

	/* the UDP and IP headers, udpp2p's and the tag in one */
	hlen = pr != NULL ? 2 + UDPP2P_TAGLEN : 2;
	len = m->m_pkthdr.len;
	M_PREPEND(m, sizeof(struct udpiphdr) + hlen, M_DONTWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto release;
	}
	hdr = mtod(m, u_char *) + sizeof(struct udpiphdr);
	hdr[0] = UDPP2P_MAGIC;
	if (crypto && sign)
		hdr[1] = UDPP2P_T_STREAM_CRYPTOSIG;
	else if (crypto)
		hdr[1] = UDPP2P_T_STREAM_CRYPTO;
	else if (sign)
		hdr[1] = UDPP2P_T_STREAM_SIG;
	else
		hdr[1] = UDPP2P_T_STREAM;
	if (pr != NULL) {
		hdr[1] |= UDPP2P_F_NOLEN | UDPP2P_F_TAG;
		memcpy(hdr + 2, pr->upr_tag, UDPP2P_TAGLEN);
		len += 2 + UDPP2P_TAGLEN;
	} else {
		hdr[1] |= UDPP2P_F_NOLEN;
		len += 2;
	}

	ui = mtod(m, struct udpiphdr *);
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_src = src->sin_addr;
	ui->ui_dst = dst->sin_addr;
	ui->ui_sport = src->sin_port;
	ui->ui_dport = dst->sin_port;
	/* from the ad hoc port, the peer's or the process's UDP port */
	if (ui->ui_sport == htons(SCE_NET_ADHOC_PORT)) {
		if (peer != NULL && peer->upp_lport != 0)
			ui->ui_sport = peer->upp_lport;
		else if (ctx != NULL)
			ui->ui_sport = ctx->upc_lport;
	}
	ui->ui_ulen = htons((u_int16_t)len + sizeof(struct udphdr));
	if (udpcksum) {
		ui->ui_sum = in_cksum_phdr(ui->ui_src.s_addr,
		    ui->ui_dst.s_addr, htons((u_int16_t)len +
		    sizeof(struct udphdr) + IPPROTO_UDP));
		m->m_pkthdr.csum_flags = M_CSUM_UDPv4;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
	} else
		ui->ui_sum = 0;
	((struct ip *)ui)->ip_len = htons(sizeof (struct udpiphdr) + len);
	((struct ip *)ui)->ip_ttl = ip_defttl;
	((struct ip *)ui)->ip_tos = 0;
	inp = sotoinpcb(so);
	((struct ip *)ui)->ip_ttl = inp->inp_ip.ip_ttl;
	((struct ip *)ui)->ip_tos = inp->inp_ip.ip_tos;
	udpp2pstat.ups_tcppackets++;

	return (ip_output(m, NULL, ro,
	    (inp->inp_flags & INP_DONTFRAG) ? IP_MTUDISC : 0, NULL, so));

drop:
	error = 0;
release:
	m_freem(m);
	return (error);
}

/* the peer (of any process) a packet from addr and port to our UDP port lport is from */
struct udpp2p_peer *
udpp2p_peer_lookup_all(u_int32_t addr, in_port_t port, in_port_t lport,
    int bcast)
{
	struct in_ifaddr *ia;
	struct udpp2p_ctx *ctx;
	struct udpp2p_peer *peer;

	TAILQ_FOREACH(ia, &in_ifaddrhead, ia_list)
		if (ia->ia_addr.sin_addr.s_addr == addr)
			break;
	if (ia != NULL)
		addr = INADDR_LOOPBACK;
	for (ctx = NULL; (ctx = sceNetPsGetNextProtocolContext(ctx)) != NULL; )
		LIST_FOREACH(peer, &ctx->upc_peers, upp_list)
			if (peer->upp_addr.sin_addr.s_addr == addr &&
			    peer->upp_addr.sin_port == port &&
			    peer->upp_lport == lport &&
			    peer->upp_bcast == bcast)
				return (peer);
	return (NULL);
}

/* the process whose UDP port is lport */
struct udpp2p_ctx *
udpp2p_getctx_byport(in_port_t lport)
{
	struct udpp2p_ctx *ctx = NULL;

	do
		ctx = sceNetPsGetNextProtocolContext(ctx);
	while (ctx != NULL && ctx->upc_lport != lport);
	return (ctx);
}

int
udpp2p_hasproc(void)
{
	struct udpp2p_proc *pr;
	SceUID pid = sceNetPsGetProcessId();

	LIST_FOREACH(pr, &udpp2p_procs, upr_list)
		if (pr->upr_pid == pid)
			return (1);
	return (0);
}

int
udpp2p_hastag(struct socket *so)
{
	struct udpp2p_proc *pr;
	SceUID pid;

	if (so == NULL)
		return (0);
	pid = so->processId;
	LIST_FOREACH(pr, &udpp2p_procs, upr_list)
		if (pr->upr_pid == pid)
			return (1);
	return (0);
}

int
sceNetPsUnregisterProcessNetworkState(SceUID pid)
{
	struct udpp2p_proc *pr;

	LIST_FOREACH(pr, &udpp2p_procs, upr_list)
		if (pr->upr_pid == pid)
			break;
	if (pr == NULL)
		return (ENOENT);
	LIST_REMOVE(pr, upr_list);
	free(pr, M_PCB);
	return (0);
}

/*ARGSUSED*/
int
udpp2p_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
	struct udpp2pcb *up;
	struct inpcb *inp;
	struct udpp2p_ctx *ctx;
	struct udpp2p_peer *peer;
	struct udpp2p_peerreq *upq;
	int error = 0;

	if (req == PRU_CONTROL) {
		ctx = sceNetPsGetProtocolContext(sceNetPsGetProcessId());
		if (ctx == NULL)
			return (-ENOSPC);
		upq = (struct udpp2p_peerreq *)nam;
		switch ((u_long)m) {
		case SIOCP2PSETKEY: {
			struct udpp2p_peerreq self;

			memcpy(ctx->upc_key, upq->upq_key, sizeof(ctx->upc_key));
			ctx->upc_flags |= UPC_KEY;
			memset(&self, 0, sizeof(self));
			self.upq_addr.sin_addr.s_addr = INADDR_LOOPBACK;
			self.upq_addr.sin_port = htons(SCE_NET_ADHOC_PORT);
			memcpy(self.upq_key, upq->upq_key, sizeof(self.upq_key));
			udpp2p_peer_set(&self, ctx);
			return (0);
		}
		case SIOCP2PADDANY:
			upq->upq_addr.sin_addr.s_addr = INADDR_BROADCAST;
			upq->upq_addr.sin_port = 0;
			upq->upq_flags = 0;
			udpp2p_peer_set(upq, ctx);
			return (0);
		case SIOCP2PGETKEY:
			memcpy(upq->upq_key, ctx->upc_key, sizeof(upq->upq_key));
			return (0);
		case SIOCP2PGETANY:
			upq->upq_addr.sin_addr.s_addr = INADDR_BROADCAST;
			upq->upq_addr.sin_port = 0;
			upq->upq_flags = 0;
			peer = udpp2p_peer_lookup(&ctx->upc_peers, upq->upq_addr.sin_addr,
			    upq->upq_addr.sin_port, upq->upq_flags & UPQ_BCAST);
			if (peer != NULL)
				memcpy(upq->upq_key, peer->upp_key,
				    sizeof(upq->upq_key));
			else
				memset(upq->upq_key, 0, sizeof(upq->upq_key));
			return (0);
		case SIOCP2PGETPORT:
			memcpy(nam, &ctx->upc_lport, sizeof(ctx->upc_lport));
			return (0);
		case SIOCP2PADDPEER:
			if (upq->upq_addr.sin_addr.s_addr == INADDR_BROADCAST)
				return (EINVAL);
			return (udpp2p_peer_set(upq, ctx));
		case SIOCP2PDELPEER:
			peer = udpp2p_peer_lookup(&ctx->upc_peers, upq->upq_addr.sin_addr,
			    upq->upq_addr.sin_port, upq->upq_flags & UPQ_BCAST);
			if (peer == NULL || peer->upp_group != upq->upq_group)
				return (ENOENT);
			if (memcmp(peer->upp_key, upq->upq_key,
			    sizeof(peer->upp_key)) == 0 &&
			    --peer->upp_refs <= 0)
				udpp2p_peer_free(peer);
			return (0);
		case SIOCP2PSETTAG: {
			struct udpp2p_proc *pr;
			SceHmacSha1Context hmac;

			if (udpp2p_hasproc())
				sceNetPsUnregisterProcessNetworkState(
				    sceNetPsGetProcessId());
			error = ENOMEM;
			pr = malloc(sizeof(*pr), M_PCB, M_NOWAIT);
			if (pr != NULL) {
				memset(pr, 0, sizeof(*pr));
				pr->upr_pid = sceNetPsGetProcessId();
				if (sceNetPsHmacSha1Init(&hmac, nam, 16) != 0 ||
				    (error = sceNetPsHmacSha1Result(pr->upr_tag,
				    &hmac)) != 0) {
					error = EINVAL;
					free(pr, M_PCB);
				} else
					LIST_INSERT_HEAD(&udpp2p_procs, pr,
					    upr_list);
			}
			return (error);
		}
		case SIOCP2PDELTAG:
			return (sceNetPsUnregisterProcessNetworkState(
			    sceNetPsGetProcessId()));
		case SIOCP2PDELGROUP: {
			int group = upq->upq_group;

			for (;;) {
				LIST_FOREACH(peer, &ctx->upc_peers, upp_list)
					if (peer->upp_group == group)
						break;
				if (peer == NULL)
					return (0);
				udpp2p_peer_free(peer);
			}
		}
		case SIOCP2PSETPORT:
			memcpy(&ctx->upc_lport, nam, sizeof(ctx->upc_lport));
			return (0);
		case SIOCP2PCLEAR:
			udpp2p_ctx_clear(ctx);
			return (0);
		default:
			return (in_control(so, (long)m, (caddr_t)nam,
			    (struct ifnet *)control, p));
		}
	}

	up = (struct udpp2pcb *)so->so_pcb;
	if (req != PRU_ATTACH && up == NULL)
		return (EINVAL);
	inp = &up->up_inpcb;

	/*
	 * Note: need to block udpp2p_input while changing
	 * the udpp2p pcb queue and/or pcb addresses.
	 */
	switch (req) {

	case PRU_ATTACH:
		if (up != NULL) {
			error = EISCONN;
			break;
		}
		if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
			error = soreserve(so, UDPP2P_SENDSPACE,
			    UDPP2P_RECVSPACE);
			if (error)
				break;
		}
		up = pool_get(&udpp2pcb_pool, PR_NOWAIT);
		if (up == NULL)
			return (ENOBUFS);
		memset(up, 0, sizeof(*up));
		inp = &up->up_inpcb;
		inp->inp_socket = so;
		inp->inp_af = AF_INET;
		inp->inp_errormtu = -1;
		so->so_pcb = up;
		LIST_INSERT_HEAD(&udpp2pcbs, up, up_list);
		inp->inp_state = INP_ATTACHED;
		sotoinpcb(so)->inp_ip.ip_ttl = ip_defttl;
		break;

	case PRU_DETACH:
		udpp2p_detach(up);
		break;

	case PRU_BIND: {
		struct socket *so2 = inp->inp_socket;
		struct sockaddr_in *sin;
		struct udpp2pcb *t;
		struct in_ifaddr *ia;
		struct in_addr laddr;
		in_port_t lport, lvport, vport;
		u_int16_t mymin, mymax, cand;
		int options, cnt;
		SceUID pid;

		pid = sceNetPsGetProcessId();
		options = so2->so_options;
		if (inp->inp_lport != 0 || !in_nullhost(inp->inp_laddr) ||
		    up->up_lvport != 0 || nam == NULL)
			return (EINVAL);
		sin = mtod(nam, struct sockaddr_in *);
		if (nam->m_len != sizeof(*sin))
			return (EINVAL);
		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);
		lport = sin->sin_port;
		if (lport == 0)
			return (EINVAL);
		lvport = sin->sin_vport;
		if (IN_MULTICAST(sin->sin_addr.s_addr))
			return (EADDRNOTAVAIL);
		if (!in_nullhost(sin->sin_addr)) {
			sin->sin_port = 0;		/* yech... */
			sin->sin_vport = 0;
			INADDR_TO_IA(sin->sin_addr, ia);
			if (ia == NULL && ifa_ifwithaddr(sintosa(sin)) == NULL)
				return (EADDRNOTAVAIL);
		}
		laddr = sin->sin_addr;
		/* an application binds the system's virtual ports only with SO_SCE_OPTION_100000 */
		if (ntohs(lvport) >= UDPP2P_APPPORT_FIRST &&
		    sceNetPsIsApplicationProgram(NULL) &&
		    (so2->so_options & SO_SCE_OPTION_100000) == 0)
			return (EACCES);
		if (lvport != 0) {
			t = udpp2p_pcblookup_port(sin->sin_addr, lport, lvport,
			    (options & (SO_REUSEADDR|SO_REUSEPORT)) == 0, pid);
			if (t != NULL && (options &
			    t->up_inpcb.inp_socket->so_options & SO_REUSEPORT) == 0)
				return (EADDRINUSE);
		} else {
			if (sceNetPsIsApplicationProgram(NULL) &&
			    (so2->so_options & SO_SCE_OPTION_100000) == 0) {
				mymin = UDPP2P_APPPORT_FIRST;
				mymax = UDPP2P_APPPORT_LAST;
				cand = udpp2p_applastport - 1;
			} else {
				mymin = UDPP2P_PORT_FIRST;
				mymax = UDPP2P_PORT_LAST;
				cand = udpp2p_lastport - 1;
			}
			for (cnt = mymax - mymin + 1; ; cnt--, cand--) {
				if (cnt == 0)
					return (EAGAIN);
				if (cand < mymin || cand > mymax)
					cand = mymax;
				vport = htons(cand);
				if (udpp2p_pcblookup_port(laddr, lport, vport, 1,
				    pid) == NULL)
					break;
			}
			inp->inp_flags |= INP_ANONPORT;
			if (sceNetPsIsApplicationProgram(NULL) &&
			    (so2->so_options & SO_SCE_OPTION_100000) == 0)
				udpp2p_applastport = cand;
			else
				udpp2p_lastport = cand;
			lvport = vport;
		}
		inp->inp_laddr = laddr;
		inp->inp_lport = lport;
		up->up_lvport = lvport;
		up->up_pid = pid;
		inp->inp_state = INP_BOUND;
		return (0);
	}

	case PRU_LISTEN:
		error = EOPNOTSUPP;
		break;

	case PRU_CONNECT:
		error = udpp2p_connect(up, mtod(nam, struct sockaddr_in *),
		    &nam->m_len);
		if (error)
			break;
		soisconnected(so);
		break;

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	case PRU_DISCONNECT:
		/*soisdisconnected(so);*/
		so->so_state &= ~SS_ISCONNECTED;	/* XXX */
		if (inp->inp_af == AF_INET)
			udpp2p_disconnect(up);
		inp->inp_state = INP_BOUND;		/* XXX */
		inp->inp_laddr = zeroin_addr;		/* XXX */
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_RCVD:
		error = EOPNOTSUPP;
		break;

	case PRU_SEND:
		if (control && control->m_len) {
			m_freem(control);
			m_freem(m);
			error = EINVAL;
			break;
		}
	{
		struct in_addr laddr;			/* XXX */

		if (nam) {
			laddr = inp->inp_laddr;		/* XXX */
			if ((so->so_state & SS_ISCONNECTED) != 0) {
				error = EISCONN;
				goto die;
			}
			error = udpp2p_connect(up,
			    mtod(nam, struct sockaddr_in *), &nam->m_len);
			if (error)
				goto die;
		} else {
			if ((so->so_state & SS_ISCONNECTED) == 0) {
				error = ENOTCONN;
				goto die;
			}
		}
		error = udpp2p_output(m, up, PRU_SEND);
		m = NULL;
		if (nam) {
			if (inp->inp_af == AF_INET)
				udpp2p_disconnect(up);
			inp->inp_laddr = laddr;		/* XXX */
			inp->inp_state = INP_BOUND;	/* XXX */
		}
	die:
		if (m)
			m_freem(m);
	}
		break;

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	case PRU_RCVOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SENDOOB:
		m_freem(control);
		m_freem(m);
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		if (inp->inp_af == AF_INET) {
			struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

			nam->m_len = sizeof(*sin);
			memset(sin, 0, sizeof(*sin));
			sin->sin_family = AF_INET;
			sin->sin_len = sizeof(*sin);
			sin->sin_port = inp->inp_lport;
			sin->sin_addr = inp->inp_laddr;
			sin->sin_vport = up->up_lvport;
		}
		break;

	case PRU_PEERADDR:
		if (inp->inp_af == AF_INET) {
			struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);

			nam->m_len = sizeof(*sin);
			memset(sin, 0, sizeof(*sin));
			sin->sin_family = AF_INET;
			sin->sin_len = sizeof(*sin);
			sin->sin_port = inp->inp_fport;
			sin->sin_addr = inp->inp_faddr;
			sin->sin_vport = up->up_fvport;
		}
		break;

	default:
		panic("udpp2p_usrreq");
	}

	return (error);
}

/*
 * Check the signature and decrypt what follows off (len bytes) with key, into
 * a copy of the packet.
 */
struct mbuf *
udpp2p_crypt_input(struct mbuf *m, int off, int len, int type,
    const u_char *key)
{
	struct udpp2p_decrypt *ud;
	struct mbuf *n;
	u_char t = type & UDPP2P_TYPE;
	int i;

	ud = malloc(sizeof(*ud), M_TEMP, M_NOWAIT|M_ZERO);
	if (ud == NULL)
		return (NULL);
	switch (t) {
	case UDPP2P_T_DGRAM_CRYPTO:
	case UDPP2P_T_DGRAM_CRYPTOSIG:
	case UDPP2P_T_STREAM_CRYPTO:
	case UDPP2P_T_STREAM_CRYPTOSIG:
		ud->ud_crypto = 1;
		break;
	}
	switch (t) {
	case UDPP2P_T_DGRAM_SIG:
	case UDPP2P_T_DGRAM_CRYPTOSIG:
	case UDPP2P_T_STREAM_SIG:
	case UDPP2P_T_STREAM_CRYPTOSIG:
		ud->ud_sign = 1;
		break;
	}
	if (ud->ud_crypto) {
		ud->ud_ivlen = (type & UDPP2P_F_NOLEN) ? 4 : 8;
		if (len < ud->ud_ivlen)
			goto fail;
		for (i = 0; i < (int)sizeof(ud->ud_iv); i += ud->ud_ivlen)
			m_copydata(m, off, ud->ud_ivlen, (caddr_t)ud->ud_iv + i);
		off += ud->ud_ivlen;
		len -= ud->ud_ivlen;
	}
	if (ud->ud_sign) {
		ud->ud_maclen = (type & UDPP2P_F_NOLEN) ? 4 : 8;
		if (len < ud->ud_maclen)
			goto fail;
		memset(ud->ud_mac, 0, sizeof(ud->ud_mac));
		m_copydata(m, off, ud->ud_maclen, (caddr_t)ud->ud_mac);
		off += ud->ud_maclen;
		len -= ud->ud_maclen;
	}
	if (ud->ud_sign) {
		if (key == NULL)
			goto fail;
		memset(ud->ud_digest, 0, sizeof(ud->ud_digest));
		if (sceNetPsHmacSha1Init(&ud->ud_hmac, key, 16) != 0)
			goto fail;
		ud->ud_cur = m;
		ud->ud_off = off;
		ud->ud_resid = len;
		if (m != NULL && len > 0) {
			for (;;) {
				if (ud->ud_cur->m_len > ud->ud_off) {
					i = ud->ud_cur->m_len - ud->ud_off;
					ud->ud_chunk = ud->ud_resid < i ?
					    ud->ud_resid : i;
					if (sceNetPsHmacSha1Update(&ud->ud_hmac,
					    mtod(ud->ud_cur, caddr_t) + ud->ud_off,
					    ud->ud_chunk) != 0)
						goto fail;
					ud->ud_off = 0;
					ud->ud_resid -= ud->ud_chunk;
				} else
					ud->ud_off -= ud->ud_cur->m_len;
				ud->ud_cur = ud->ud_cur->m_next;
				if (ud->ud_cur == NULL || ud->ud_resid <= 0)
					break;
			}
		}
		if (sceNetPsHmacSha1Result(ud->ud_digest, &ud->ud_hmac) != 0 ||
		    sceNetPsTimingSafeCompare(ud->ud_digest, ud->ud_mac,
		    ud->ud_maclen) != 0)
			goto fail;
	}
	if (ud->ud_crypto == 0) {
		n = m_copypacket(m, M_DONTWAIT);
		m_adj(n, off);
		if (n == NULL)
			goto fail;
		free(ud, M_TEMP);
		return (n);
	}
	if (key == NULL)
		goto fail;
	for (ud->ud_off = 0; ud->ud_off < len; ud->ud_off += ud->ud_chunk) {
		ud->ud_chunk = len - ud->ud_off;
		if (ud->ud_chunk >= UDPP2P_CRYPTCHUNK)
			ud->ud_chunk = UDPP2P_CRYPTCHUNK;
		if (ud->ud_top != NULL) {
			MGET(ud->ud_cur, M_DONTWAIT, MT_DATA);
			if (ud->ud_cur == NULL) {
				m_freem(ud->ud_top);
				goto fail;
			}
			if (ud->ud_chunk > MLEN) {
				MCLGET(ud->ud_cur, M_DONTWAIT);
				if ((ud->ud_cur->m_flags & M_EXT) == 0) {
					m_free(ud->ud_cur);
					m_freem(ud->ud_top);
					goto fail;
				}
			}
		} else {
			MGETHDR(ud->ud_cur, M_DONTWAIT, MT_DATA);
			if (ud->ud_cur == NULL)
				goto fail;
			M_COPY_PKTHDR(ud->ud_cur, m);
			ud->ud_cur->m_data = ud->ud_cur->m_pktdat;
			if (ud->ud_chunk > MHLEN) {
				MCLGET(ud->ud_cur, M_DONTWAIT);
				if ((ud->ud_cur->m_flags & M_EXT) == 0) {
					m_free(ud->ud_cur);
					goto fail;
				}
			}
			ud->ud_cur->m_pkthdr.len = len;
		}
		m_copydata(m, off + ud->ud_off, ud->ud_chunk,
		    mtod(ud->ud_cur, caddr_t));
		ud->ud_cur->m_len = ud->ud_chunk;
		if (sceNetPsAesDecrypt(mtod(ud->ud_cur, u_char *),
		    mtod(ud->ud_cur, u_char *), ud->ud_cur->m_len, key, 128,
		    ud->ud_iv) != 0) {
			m_free(ud->ud_cur);
			m_freem(ud->ud_top);
			goto fail;
		}
		if (ud->ud_top != NULL) {
			ud->ud_tail->m_next = ud->ud_cur;
			ud->ud_tail = ud->ud_cur;
		} else
			ud->ud_top = ud->ud_tail = ud->ud_cur;
	}
	n = ud->ud_top;
	free(ud, M_TEMP);
	return (n);

fail:
	free(ud, M_TEMP);
	return (NULL);
}

/* deliver a datagram to so, as udp4_sendup */
void
udpp2p_sendup(struct mbuf *m, int off, int len, struct sockaddr *src,
    struct socket *so, u_int8_t type, struct udpp2p_ctx *ctx,
    struct udpp2p_peer *peer)
{
	struct mbuf *n;
	struct inpcb *inp;
	const u_char *key;

	if (ctx != NULL)
		key = (ctx->upc_flags & UPC_KEY) ? ctx->upc_key : NULL;
	else if (peer != NULL)
		key = peer->upp_key;
	else
		key = NULL;
	n = udpp2p_crypt_input(m, off, len, type, key);
	if (n == NULL)
		return;
	if (so == NULL)
		goto drop;
	/* SCE: what a socket with SO_SCE_OPTION_8000 sent reaches only such sockets */
	if ((so->so_options & SO_SCE_OPTION_8000) == 0 &&
	    (m->m_flags & M_SCE_OPTION_8000))
		goto drop;
	/* SCE: a packet the receive emulation chose to drop is filtered per socket */
	if ((m->m_flags & M_SCE_EMULATION_DROP) &&
	    sceNetPsFilterRxSocket(m, m->m_pkthdr.rcvif, so))
		goto drop;
	/* a socket that encrypts or signs takes only such datagrams */
	if ((so->so_options & SO_USECRYPTO) &&
	    (type & UDPP2P_TYPE) != UDPP2P_T_DGRAM_CRYPTO &&
	    (type & UDPP2P_TYPE) != UDPP2P_T_DGRAM_CRYPTOSIG)
		goto drop;
	if ((so->so_options & SO_USESIGNATURE) &&
	    (type & UDPP2P_TYPE) != UDPP2P_T_DGRAM_SIG &&
	    (type & UDPP2P_TYPE) != UDPP2P_T_DGRAM_CRYPTOSIG)
		goto drop;
	/* SCE: IP_TTLCHK records the highest TTL received */
	inp = sotoinpcb(so);
	if (inp != NULL && (inp->inp_flags & INP_TTLCHK))
		inp->inp_maxttl = max(inp->inp_maxttl,
		    mtod(m, struct ip *)->ip_ttl);
	if (sbappendaddr(&so->so_rcv, src, n, NULL) == 0) {
		udpp2pstat.ups_fullsock++;
		goto drop;
	}
	sorwakeup(so);
	return;

drop:
	m_freem(n);
}

/*
 * A datagram from src to dst, its UDP header at off: deliver it to the
 * SOCK_DGRAM_P2P socket bound to its virtual port if udpp2p's header is there.
 */
int
udpp2p_input(struct sockaddr_in *src, struct sockaddr_in *dst, struct mbuf *m,
    int off)
{
	struct udpp2p_peer *peer;
	struct udpp2p_ctx *ctx;
	struct udpp2pcb *up;
	struct in_addr faddr, laddr;
	in_port_t sport, dport, svport, dvport;
	u_int8_t magic, type, port;
	u_int16_t plen;
	u_char tag[UDPP2P_TAGLEN];
	int hoff, resid, taglen, bcast;
	SceUID pid;

	hoff = off + sizeof(struct udphdr);
	if (src->sin_family != AF_INET || dst->sin_family != AF_INET)
		return (0);
	dport = dst->sin_port;
	sport = src->sin_port;
	faddr = src->sin_addr;
	laddr = dst->sin_addr;
	bcast = in_broadcast(laddr, m->m_pkthdr.rcvif) != 0;

	/* the process the packet is for, by the peer or the UDP port */
	peer = udpp2p_peer_lookup_all(faddr.s_addr, sport, dport, bcast);
	if (peer == NULL) {
		for (ctx = NULL;
		    (ctx = sceNetPsGetNextProtocolContext(ctx)) != NULL; )
			if (ctx->upc_lport == dport) {
				pid = ctx->upc_pid;
				dport = htons(SCE_NET_ADHOC_PORT);
				goto found;
			}
		pid = -1;
	} else {
		ctx = sceNetPsGetProtocolContext(peer->upp_pid);
		if (ctx == NULL)
			pid = -1;
		else {
			if (peer->upp_lport == dport)
				dport = htons(SCE_NET_ADHOC_PORT);
			pid = ctx->upc_pid;
		}
	}
found:
	if (udpp2p_pcblookup_bind(laddr, dport, 0, pid) == NULL)
		return (0);

	/* udpp2p's header */
	if (hoff + sizeof(magic) + sizeof(type) > m->m_pkthdr.len)
		return (0);
	m_copydata(m, hoff++, sizeof(magic), (caddr_t)&magic);
	if (magic != UDPP2P_MAGIC)
		return (0);
	m_copydata(m, hoff++, sizeof(type), (caddr_t)&type);
	if (type & UDPP2P_F_NOLEN)
		plen = m->m_pkthdr.len - hoff;
	else {
		if (hoff + sizeof(plen) > m->m_pkthdr.len)
			return (0);
		m_copydata(m, hoff, sizeof(plen), (caddr_t)&plen);
		plen = ntohs(plen);
		hoff += sizeof(plen);
		if (hoff + plen > m->m_pkthdr.len) {
			udpp2pstat.ups_badlen++;
			return (0);
		}
	}
	if ((type & UDPP2P_TYPE) < UDPP2P_T_DGRAM ||
	    (type & UDPP2P_TYPE) > UDPP2P_T_DGRAM_CRYPTOSIG)
		return (0);
	if ((type & UDPP2P_TYPE) != UDPP2P_T_DGRAM && peer == NULL)
		return (0);
	if (type & UDPP2P_F_SHORTPORT) {
		if (plen < 2)
			goto hdrops;
		m_copydata(m, hoff++, sizeof(port), (caddr_t)&port);
		svport = htons(port);
		m_copydata(m, hoff, sizeof(port), (caddr_t)&port);
		dvport = htons(port);
		resid = plen - 2;
		hoff++;
	} else {
		if (plen < 4)
			goto hdrops;
		m_copydata(m, hoff, sizeof(svport), (caddr_t)&svport);
		hoff += sizeof(svport);
		m_copydata(m, hoff, sizeof(dvport), (caddr_t)&dvport);
		resid = plen - 4;
		hoff += sizeof(dvport);
	}

	up = udpp2p_pcblookup_connect(faddr, sport, svport, laddr, dport,
	    dvport, pid);
	if (up == NULL) {
		up = udpp2p_pcblookup_bind(laddr, dport, dvport, pid);
		if (up == NULL)
			return (0);
	}
	src->sin_vport = svport;
	if (type & UDPP2P_F_TAG) {
		taglen = udpp2p_gettag(m, &hoff, &resid, tag);
		if (taglen < 0 || udpp2p_checktag(tag, taglen,
		    up->up_inpcb.inp_socket) < 0)
			goto hdrops;
	}
	if (bcast)
		udpp2p_sendup(m, hoff, resid, sintosa(src),
		    up->up_inpcb.inp_socket, type, NULL, peer);
	else
		udpp2p_sendup(m, hoff, resid, sintosa(src),
		    up->up_inpcb.inp_socket, type, ctx, NULL);
	udpp2pstat.ups_ipackets++;
	return (1);

hdrops:
	udpp2pstat.ups_hdrops++;
	return (0);
}

int
sceNetPsInitializeProcessProtocol(SceUID pid)
{
	struct udpp2p_ctx *ctx;

	ctx = malloc(sizeof(*ctx), M_PCB, M_NOWAIT|M_ZERO);
	if (ctx == NULL)
		return (ENOMEM);
	ctx->upc_pid = pid;
	ctx->upc_lport = htons(SCE_NET_ADHOC_PORT);
	LIST_INIT(&ctx->upc_peers);
	sceNetPsSetProtocolContext(pid, ctx);
	return (0);
}

int
sceNetPsReleaseProcessProtocol(SceUID pid)
{
	struct udpp2p_ctx *ctx;

	ctx = sceNetPsGetProtocolContext(pid);
	if (ctx != NULL)
		udpp2p_ctx_clear(ctx);
	free(ctx, M_PCB);
	return (0);
}
