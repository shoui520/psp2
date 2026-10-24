/*
 * SCE: the UDP encapsulation of SOCK_DGRAM_P2P and SOCK_STREAM_P2P sockets
 * (udpp2p_usrreq, the udpp2pcbpl pool): datagrams between virtual ports,
 * carried in UDP
 */

#ifndef _NETINET_UDPP2P_VAR_H_
#define _NETINET_UDPP2P_VAR_H_

/* what udpp2p adds to a TCP segment it carries (tcp_output's segment size) */
#define	UDPP2P_HDRLEN		12	/* the UDP header and udpp2p's own */
#define	UDPP2P_TAGLEN		4	/* the sending process's tag, when it has one */
#define	UDPP2P_CRYPTOLEN	8	/* SO_USECRYPTO */
#define	UDPP2P_SIGLEN		8	/* SO_USESIGNATURE */

#ifdef _KERNEL
int	 udpp2p_hasproc(void);	/* the calling process has a udpp2p entry */
int	 udpp2p_hastag(struct socket *);
int	 udpp2p_output(struct sockaddr_in *, struct sockaddr_in *,
	    struct mbuf *, struct route *, struct socket *, int, int);
#endif

#endif /* !_NETINET_UDPP2P_VAR_H_ */
