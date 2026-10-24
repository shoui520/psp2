/*
 * SCE: the UDP encapsulation of SOCK_DGRAM_P2P and SOCK_STREAM_P2P sockets
 * (udpp2p_usrreq, the udpp2pcbpl pool): datagrams between virtual ports,
 * carried in UDP
 */

#ifndef _NETINET_UDPP2P_VAR_H_
#define _NETINET_UDPP2P_VAR_H_

#include <sys/queue.h>
#include <sys/ioccom.h>
#include <scetypes.h>

/* what udpp2p adds to a TCP segment it carries (tcp_output's segment size) */
#define	UDPP2P_HDRLEN		12	/* the UDP header and udpp2p's own */
#define	UDPP2P_TAGLEN		4	/* the sending process's tag, when it has one */
#define	UDPP2P_CRYPTOLEN	8	/* SO_USECRYPTO */
#define	UDPP2P_SIGLEN		8	/* SO_USESIGNATURE */

/*
 * udpp2p's header, after the UDP header: a marker byte, a type byte, the length
 * of what follows (unless UDPP2P_F_NOLEN), the virtual ports of a datagram
 * (one byte each with UDPP2P_F_SHORTPORT), the sender's tag (UDPP2P_F_TAG), the
 * IV and the signature
 */
#define	UDPP2P_MAGIC		0xff
#define	UDPP2P_F_NOLEN		0x80	/* the packet's end is the payload's */
#define	UDPP2P_F_SHORTPORT	0x40	/* both virtual ports below 256 */
#define	UDPP2P_F_TAG		0x20	/* the sending process's tag follows */
#define	UDPP2P_TYPE		0x0f
#define	UDPP2P_T_DGRAM		3	/* a SOCK_DGRAM_P2P datagram */
#define	UDPP2P_T_DGRAM_CRYPTO	4
#define	UDPP2P_T_DGRAM_SIG	5
#define	UDPP2P_T_DGRAM_CRYPTOSIG 6
#define	UDPP2P_T_STREAM		7	/* a SOCK_STREAM_P2P segment */
#define	UDPP2P_T_STREAM_CRYPTO	8
#define	UDPP2P_T_STREAM_SIG	9
#define	UDPP2P_T_STREAM_CRYPTOSIG 10

/* a SOCK_DGRAM_P2P socket's protocol control block: an inpcb with virtual ports */
struct udpp2pcb {
	struct inpcb up_inpcb;
	LIST_ENTRY(udpp2pcb) up_list;	/* udpp2pcbs */
	in_port_t up_fvport;		/* foreign virtual port */
	in_port_t up_lvport;		/* local virtual port */
	SceUID	  up_pid;		/* the process that bound or connected it */
	int	  up_reserve;		/* counted against IP_MAXPACKET; nothing sets it */
};

LIST_HEAD(udpp2pcbhead, udpp2pcb);
extern struct udpp2pcbhead udpp2pcbs;	/* every udpp2pcb, for sceNetPsCollectSockInfo */

/* a peer a process exchanges keys with (SIOCP2PADDPEER) */
struct udpp2p_peer {
	LIST_ENTRY(udpp2p_peer) upp_list;	/* udpp2p_ctx's upc_peers */
	SceUID	  upp_pid;		/* the owning process */
	struct sockaddr_in upp_addr;	/* its address and UDP port */
	u_char	  upp_key[16];		/* AES-128 and HMAC-SHA1 key */
	in_port_t upp_lport;		/* our UDP port towards it */
	int	  upp_refs;		/* SIOCP2PADDPEER calls with this key */
	int	  upp_bcast;		/* reached by broadcast */
	int	  upp_group;		/* SIOCP2PDELGROUP removes a group */
};

/* a process's udpp2p state (sceNetPsSetProtocolContext) */
struct udpp2p_ctx {
	SceUID	  upc_pid;
	in_port_t upc_lport;		/* its UDP port; SCE_NET_ADHOC_PORT at first */
	u_int16_t upc_reserved;
	u_char	  upc_key[16];		/* its own key (SIOCP2PSETKEY) */
	int	  upc_flags;
	LIST_HEAD(udpp2p_peers, udpp2p_peer) upc_peers;
};
#define	UPC_KEY		0x1		/* upc_key is set */

/* a process that tags what it sends (SIOCP2PSETTAG) */
struct udpp2p_proc {
	LIST_ENTRY(udpp2p_proc) upr_list;	/* udpp2p_procs */
	SceUID	  upr_pid;
	u_char	  upr_tag[20];		/* HMAC-SHA1 of nothing under its key */
};

/* the argument of the peer and key ioctls */
struct udpp2p_peerreq {
	struct sockaddr_in upq_addr;
	u_char	  upq_key[16];
	in_port_t upq_lport;
	u_char	  upq_flags;
	u_char	  upq_group;
};
#define	UPQ_BCAST	0x1

/* the peer and key ioctls (PRU_CONTROL) */
#define	SIOCP2PCLEAR	_IO('P', 0xc8)		/* forget the key and the peers */
#define	SIOCP2PDELGROUP	_IOW('P', 0xc8, struct udpp2p_peerreq)
#define	SIOCP2PSETKEY	_IOW('P', 0xc9, struct udpp2p_peerreq)
#define	SIOCP2PGETKEY	_IOWR('P', 0xca, struct udpp2p_peerreq)
#define	SIOCP2PADDPEER	_IOW('P', 0xcb, struct udpp2p_peerreq)
#define	SIOCP2PDELPEER	_IOW('P', 0xcc, struct udpp2p_peerreq)
#define	SIOCP2PADDANY	_IOW('P', 0xcd, struct udpp2p_peerreq)
#define	SIOCP2PGETANY	_IOWR('P', 0xce, struct udpp2p_peerreq)
#define	SIOCP2PSETPORT	_IOW('P', 0xcf, in_port_t)
#define	SIOCP2PGETPORT	_IOWR('P', 0xd0, in_port_t)
#define	SIOCP2PSETTAG	_IOW('P', 0xd8, u_char[16])
#define	SIOCP2PDELTAG	_IOW('P', 0xd9, u_char[16])

struct udpp2pstat {
	u_quad_t ups_ipackets;		/* datagrams delivered */
	u_quad_t ups_hdrops;		/* header too short or bad tag */
	u_quad_t ups_badlen;		/* length beyond the packet */
	u_quad_t ups_fullsock;		/* not delivered, socket full */
	u_quad_t ups_opackets;		/* datagrams sent */
	u_quad_t ups_reserved;
	u_quad_t ups_tcppackets;	/* TCP segments received and sent */
};

#ifdef _KERNEL
extern struct udpp2pstat udpp2pstat;

void	 udpp2p_init(void);
int	 udpp2p_input(struct sockaddr_in *, struct sockaddr_in *,
	    struct mbuf *, int);
int	 udpp2p_tcpinput(struct sockaddr_in *, struct sockaddr_in *,
	    struct mbuf *, int);
int	 udpp2p_output(struct mbuf *, ...);
int	 udpp2p_tcp_output(struct sockaddr_in *, struct sockaddr_in *,
	    struct mbuf *, struct route *, struct socket *, int, int);
int	 udpp2p_usrreq(struct socket *,
	    int, struct mbuf *, struct mbuf *, struct mbuf *, struct proc *);
struct mbuf *udpp2p_crypt_input(struct mbuf *, int, int, int, const u_char *);
struct udpp2p_ctx *udpp2p_getctx(SceUID);
struct udpp2p_ctx *udpp2p_getctx_byport(in_port_t);
struct udpp2p_peer *udpp2p_peer_lookup_all(u_int32_t, in_port_t, in_port_t, int);
int	 udpp2p_gettag(struct mbuf *, int *, int *, void *);
int	 udpp2p_checktag(const void *, size_t, struct socket *);
int	 udpp2p_hasproc(void);	/* the calling process has a udpp2p entry */
int	 udpp2p_hastag(struct socket *);
#endif

#endif /* !_NETINET_UDPP2P_VAR_H_ */
