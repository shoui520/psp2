/*	$NetBSD: if_inarp.h,v 1.33 2003/08/07 16:33:08 agc Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)if_ether.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IF_INARP_H_
#define _NETINET_IF_INARP_H_

#define	LA_HOLD_MAX	10	/* SCE: packets held per entry */

struct llinfo_arp {
	LIST_ENTRY(llinfo_arp) la_list;
	struct	rtentry *la_rt;
	/* SCE: the last packets until resolved/timeout, a ring */
	struct	mbuf *la_hold[LA_HOLD_MAX];
	int	la_holdnext;		/* SCE: the slot the next packet takes */
	long	la_asked;		/* last time we QUERIED for this addr */
#define la_timer la_rt->rt_rmx.rmx_expire /* deletion time in seconds */
};

struct sockaddr_inarp {
	u_int8_t  sin_len;
	u_int8_t  sin_family;
	u_int16_t sin_port;
	struct	  in_addr sin_addr;
	struct	  in_addr sin_srcaddr;
	u_int16_t sin_tos;
	u_int16_t sin_other;
#define SIN_PROXY 1
};

/*
 * IP and ethernet specific routing flags
 */
#define	RTF_USETRAILERS	RTF_PROTO1	/* use trailers */
#define	RTF_ANNOUNCE	RTF_PROTO2	/* announce new arp entry */

#ifdef _KERNEL
extern struct ifqueue arpintrq;
extern struct ifqueue isdnintrq;	/* SCE: the raw Ethernet sockets' input */
void arp_ifinit __P((struct ifnet *, struct ifaddr *));
void arp_rtrequest __P((int, struct rtentry *, struct rt_addrinfo *));
int arpresolve __P((struct ifnet *, struct rtentry *, struct mbuf *,
		    struct sockaddr *, u_char *));
void arpintr __P((void));
void arp_drain __P((void));
/* SCE: the ARP and raw Ethernet (ISDN) sockets' protocols */
void arpsock_init __P((void));
int arpsock_input __P((struct mbuf *));
int arpsock_usrreq __P((struct socket *, int, struct mbuf *, struct mbuf *,
    struct mbuf *, struct proc *));
void isdn_init __P((void));
int isdn_input __P((struct mbuf *));
int isdn_output __P((struct mbuf *, struct socket *, int));
int isdn_usrreq __P((struct socket *, int, struct mbuf *, struct mbuf *,
    struct mbuf *, struct proc *));
void sceNetPsIsdnInterrupt __P((void));

void arpwhohas __P((struct ifnet *, struct in_addr *));

/* SCE: no arpioctl, revarprequest or revarpwhoarewe */
void revarpinput __P((struct mbuf *));
void in_revarpinput __P((struct mbuf *));
#endif

#endif /* _NETINET_IF_INARP_H_ */
