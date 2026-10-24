/*
 * SCE: the UDP encapsulation of SOCK_DGRAM_P2P and SOCK_STREAM_P2P sockets
 * (udpp2p_usrreq, the udpp2pcbpl pool): datagrams between virtual ports,
 * carried in UDP
 */

#ifndef _NETINET_UDPP2P_VAR_H_
#define _NETINET_UDPP2P_VAR_H_

#ifdef _KERNEL
int	 udpp2p_output(struct sockaddr_in *, struct sockaddr_in *,
	    struct mbuf *, struct route *, struct socket *, int, int);
#endif

#endif /* !_NETINET_UDPP2P_VAR_H_ */
