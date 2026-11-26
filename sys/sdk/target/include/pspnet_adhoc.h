/*
 * pspnet_adhoc.h: ScePspnetAdhoc SDK header, under Sony's names and definitions
 * (SDK Release 03.570.011, target/include/pspnet_adhoc.h).
 */
#ifndef _SCE_PSPNET_ADHOC_H
#define _SCE_PSPNET_ADHOC_H

#include <scetypes.h>
#include <net.h>
#include <pspnet_adhoc/pspnet_adhocctl.h>

#define SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID		-2143222015	/* 0x80410701 */
#define SCE_ERROR_NET_ADHOC_INVALID_ADDR		-2143222014	/* 0x80410702 */
#define SCE_ERROR_NET_ADHOC_INVALID_PORT		-2143222013	/* 0x80410703 */
#define SCE_ERROR_NET_ADHOC_INVALID_BUFLEN		-2143222012	/* 0x80410704 */
#define SCE_ERROR_NET_ADHOC_INVALID_DATALEN		-2143222011	/* 0x80410705 */
#define SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE		-2143222010	/* 0x80410706 */
#define SCE_ERROR_NET_ADHOC_SOCKET_DELETED		-2143222009	/* 0x80410707 */
#define SCE_ERROR_NET_ADHOC_SOCKET_ALERTED		-2143222008	/* 0x80410708 */
#define SCE_ERROR_NET_ADHOC_WOULD_BLOCK			-2143222007	/* 0x80410709 */
#define SCE_ERROR_NET_ADHOC_PORT_IN_USE			-2143222006	/* 0x8041070a */
#define SCE_ERROR_NET_ADHOC_NOT_CONNECTED		-2143222005	/* 0x8041070b */
#define SCE_ERROR_NET_ADHOC_DISCONNECTED		-2143222004	/* 0x8041070c */
#define SCE_ERROR_NET_ADHOC_NOT_OPENED			-2143222003	/* 0x8041070d */
#define SCE_ERROR_NET_ADHOC_NOT_LISTENED		-2143222002	/* 0x8041070e */
#define SCE_ERROR_NET_ADHOC_SOCKET_ID_NOT_AVAIL		-2143222001	/* 0x8041070f */
#define SCE_ERROR_NET_ADHOC_PORT_NOT_AVAIL		-2143222000	/* 0x80410710 */
#define SCE_ERROR_NET_ADHOC_INVALID_ARG			-2143221999	/* 0x80410711 */
#define SCE_ERROR_NET_ADHOC_NOT_INITIALIZED		-2143221998	/* 0x80410712 */
#define SCE_ERROR_NET_ADHOC_ALREADY_INITIALIZED		-2143221997	/* 0x80410713 */
#define SCE_ERROR_NET_ADHOC_BUSY			-2143221996	/* 0x80410714 */
#define SCE_ERROR_NET_ADHOC_TIMEOUT			-2143221995	/* 0x80410715 */
#define SCE_ERROR_NET_ADHOC_NO_ENTRY			-2143221994	/* 0x80410716 */
#define SCE_ERROR_NET_ADHOC_EXCEPTION_EVENT		-2143221993	/* 0x80410717 */
#define SCE_ERROR_NET_ADHOC_CONNECTION_REFUSED		-2143221992	/* 0x80410718 */
#define SCE_ERROR_NET_ADHOC_THREAD_ABORTED		-2143221991	/* 0x80410719 */
#define SCE_ERROR_NET_ADHOC_ALREADY_CREATED		-2143221990	/* 0x8041071a */
#define SCE_ERROR_NET_ADHOC_NOT_IN_GAMEMODE		-2143221989	/* 0x8041071b */
#define SCE_ERROR_NET_ADHOC_NOT_CREATED			-2143221988	/* 0x8041071c */
#define SCE_ERROR_NET_ADHOC_INVALID_ALIGNMENT		-2143221987	/* 0x8041071d */

#ifdef __cplusplus
extern "C" {
#endif

/* socket descriptor for sceNetAdhocPollSocket() */
struct SceNetAdhocPollSd {
	int id;
	int events;
	int revents;
};

#define SCE_NET_ADHOC_EV_SEND				0x0001
#define SCE_NET_ADHOC_EV_RECV				0x0002
#define SCE_NET_ADHOC_EV_CONNECT			0x0004
#define SCE_NET_ADHOC_EV_ACCEPT				0x0008
#define SCE_NET_ADHOC_EV_FLUSH				0x0010
#define SCE_NET_ADHOC_EV_INVALID			0x0100
#define SCE_NET_ADHOC_EV_DELETE				0x0200
#define SCE_NET_ADHOC_EV_ALERT				0x0400
#define SCE_NET_ADHOC_EV_DISCONNECT			0x0800

/* PDP statistics */
struct SceNetAdhocPdpStat {
	struct SceNetAdhocPdpStat *next;
	int id;
	struct SceNetEtherAddr laddr;
	SceUShort16 lport;
	unsigned int rcv_sb_cc;
};

/* PTP statistics */
#define SCE_NET_ADHOC_PTP_STATE_CLOSED			0
#define SCE_NET_ADHOC_PTP_STATE_LISTEN			1
#define SCE_NET_ADHOC_PTP_STATE_SYN_SENT		2
#define SCE_NET_ADHOC_PTP_STATE_SYN_RCVD		3
#define SCE_NET_ADHOC_PTP_STATE_ESTABLISHED		4

struct SceNetAdhocPtpStat {
	struct SceNetAdhocPtpStat *next;
	int id;
	struct SceNetEtherAddr laddr;
	struct SceNetEtherAddr paddr;
	SceUShort16 lport;
	SceUShort16 pport;
	unsigned int snd_sb_cc;
	unsigned int rcv_sb_cc;
	int state;
};

/* flags for adhoc socket API */
#define SCE_NET_ADHOC_F_NONBLOCK			0x0001

#define SCE_NET_ADHOC_F_ALERTSEND			0x0010
#define SCE_NET_ADHOC_F_ALERTRECV			0x0020
#define SCE_NET_ADHOC_F_ALERTPOLL			0x0040
#define SCE_NET_ADHOC_F_ALERTCONNECT			0x0080
#define SCE_NET_ADHOC_F_ALERTACCEPT			0x0100
#define SCE_NET_ADHOC_F_ALERTFLUSH			0x0200
#define SCE_NET_ADHOC_F_ALERTALL \
	(SCE_NET_ADHOC_F_ALERTSEND \
		| SCE_NET_ADHOC_F_ALERTRECV \
		| SCE_NET_ADHOC_F_ALERTPOLL \
		| SCE_NET_ADHOC_F_ALERTCONNECT \
		| SCE_NET_ADHOC_F_ALERTACCEPT \
		| SCE_NET_ADHOC_F_ALERTFLUSH \
	)

/* PDP Maximum Fragment Size */
#define SCE_NET_ADHOC_PDP_MFS				1444

/* PDP Maximum Transfer Unit */
#define SCE_NET_ADHOC_PDP_MTU				65523

/* PTP Maximum Segment Size */
#define SCE_NET_ADHOC_PTP_MSS				1444

int sceNetAdhocInit(void);
int sceNetAdhocTerm(void);

int sceNetAdhocPollSocket(struct SceNetAdhocPollSd *sds, int nsds,
			  unsigned int timeout, int flag);
int sceNetAdhocSetSocketAlert(int id, int flag);
int sceNetAdhocGetSocketAlert(int id, int *flag);

int sceNetAdhocPdpCreate(const struct SceNetEtherAddr *saddr, SceUShort16 sport,
			 unsigned int bufsize, int flag);
int sceNetAdhocPdpSend(int id, const struct SceNetEtherAddr *daddr, SceUShort16 dport,
		       const void *data, int len, unsigned int timeout, int flag);
int sceNetAdhocPdpRecv(int id, struct SceNetEtherAddr *saddr, SceUShort16 *sport,
		       void *buf, int *len, unsigned int timeout, int flag);
int sceNetAdhocPdpDelete(int id, int flag);
int sceNetAdhocGetPdpStat(int *buflen, void *buf);

int sceNetAdhocPtpOpen(const struct SceNetEtherAddr *saddr, SceUShort16 sport,
		       const struct SceNetEtherAddr *daddr, SceUShort16 dport,
		       unsigned int bufsize, unsigned int rexmt_int, int rexmt_cnt, int flag);
int sceNetAdhocPtpConnect(int id, unsigned int timeout, int flag);
int sceNetAdhocPtpListen(const struct SceNetEtherAddr *saddr, SceUShort16 sport,
			 unsigned int bufsize, unsigned int rexmt_int, int rexmt_cnt,
			 int backlog, int flag);
int sceNetAdhocPtpAccept(int id, struct SceNetEtherAddr *addr, SceUShort16 *port,
			 unsigned int timeout, int flag);
int sceNetAdhocPtpSend(int id, const void *data, int *len, unsigned int timeout, int flag);
int sceNetAdhocPtpRecv(int id, void *buf, int *len, unsigned int timeout, int flag);
int sceNetAdhocPtpFlush(int id, unsigned int timeout, int flag);
int sceNetAdhocPtpClose(int id, int flag);
int sceNetAdhocGetPtpStat(int *buflen, void *buf);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_PSPNET_ADHOC_H */
