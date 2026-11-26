/*
 * adhoc_pdp.c: Packet Datagram Protocol (PDP) sockets, fragmentation/reassembly,
 * and statistics for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

typedef struct AdhocPdpList {
	AdhocPdpPcb *first;
	AdhocPdpPcb **last;
} AdhocPdpList;

static SceUShort16 g_adhocPdpLastPort;
static AdhocPdpList g_adhocPdpList;

static SceUInt32 g_adhocPdpRxPackets;
static SceUInt32 g_adhocPdpRxDropFull;
static SceUInt32 g_adhocPdpRxFrags;
static SceUInt32 g_adhocPdpRxFragDrop;
static SceUInt32 g_adhocPdpRxNoSocket;
static SceUInt32 g_adhocPdpRxFragTimeout;
static SceUShort16 g_adhocPdpMsgId;

static inline AdhocMbuf *pdpAllocFrameMbuf(int totalLen)
{
	AdhocMbuf *m;
	char *ext;

	m = (AdhocMbuf *)adhocMalloc(PSPNET_ADHOC_MSIZE);
	if (m == NULL)
		return NULL;
	m->next = NULL;
	m->nextPkt = NULL;
	m->data = m->storage.mh.body.pktDat;
	m->type = PSPNET_ADHOC_MT_DATA;
	m->flags = PSPNET_ADHOC_M_PKTHDR;
	m->storage.mh.pktHdr.aux = NULL;
	if (totalLen > PSPNET_ADHOC_MHLEN) {
		ext = (char *)adhocMalloc(PSPNET_ADHOC_MCLBYTES);
		m->storage.mh.body.ext.extBuf = ext;
		if (ext == NULL) {
			ext = (char *)adhocMalloc(PSPNET_ADHOC_MCLBYTES);
			m->storage.mh.body.ext.extBuf = ext;
		}
		if (ext != NULL) {
			m->data = ext;
			m->flags |= (PSPNET_ADHOC_M_EXT | PSPNET_ADHOC_M_CLUSTER);
			m->storage.mh.body.ext.extFree = NULL;
			m->storage.mh.body.ext.extArg = NULL;
			m->storage.mh.body.ext.extSize = PSPNET_ADHOC_MCLBYTES;
			m->storage.mh.body.ext.extNextRef = m;
			m->storage.mh.body.ext.extPrevRef = m;
		}
		if ((m->flags & PSPNET_ADHOC_M_EXT) == 0) {
			adhocMbufFree(m);
			return NULL;
		}
	}
	return m;
}

__attribute__((noinline))
int sceNetAdhocPdpCreate(const struct SceNetEtherAddr *saddr, SceUShort16 sport,
			 unsigned int bufsize, int flag)
{
	int s, id, ret;
	AdhocSocket *sock;

	if (saddr == NULL || flag != 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ARG;
	s = adhocSplNet();
	if (s < 0)
		return s;
	if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		id = adhocSocketAllocId();
		if (id < 0) {
			ret = SCE_ERROR_NET_ADHOC_SOCKET_ID_NOT_AVAIL;
		} else {
			ret = adhocSocketCreate(PSPNET_ADHOC_SOCK_PDP, &sock);
			if (ret >= 0) {
				ret = adhocPdpAttach(sock);
				if (ret >= 0) {
					ret = adhocPdpSetBufSize((AdhocPdpPcb *)sock->pcb, bufsize);
					if (ret >= 0) {
						ret = adhocPdpBind((AdhocPdpPcb *)sock->pcb,
								   saddr, sport);
						if (ret >= 0) {
							sock->id = id;
							adhocSplX(s);
							return id;
						}
						adhocPdpFlush((AdhocPdpPcb *)sock->pcb);
					}
					adhocPdpDetach(sock);
				}
				adhocSocketFree(sock);
			}
		}
	}
	adhocSplX(s);
	return ret;
}

__attribute__((noinline))
int adhocPdpSendInternal(AdhocSocket *sock, const SceNetEtherAddr *daddr, SceUShort16 dport,
			 const void *data, int len)
{
	AdhocPdpPcb *pcb = (AdhocPdpPcb *)sock->pcb;
	int isBcast = 0;
	SceUShort16 msgId;
	AdhocMbuf *head = NULL;
	AdhocMbuf **tail = &head;
	AdhocMbuf *m;
	int offset = 0;
	int fragLen, pdpLen, padLen, totalLen;
	int err = 0;
	SceUShort16 net16;
	char *frame;

	if (sceClibMemcmp(daddr, &g_adhocZeroAddr, sizeof(g_adhocZeroAddr)) == 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ADDR;
	if (daddr->data[0] & 1) {
		if (sceClibMemcmp(daddr, g_adhocBroadcastAddr, sizeof(g_adhocBroadcastAddr)) != 0)
			return SCE_ERROR_NET_ADHOC_INVALID_ADDR;
		isBcast = 1;
	}
	if (dport == 0 || dport == 0xffff)
		return SCE_ERROR_NET_ADHOC_INVALID_PORT;
	if ((unsigned int)len > SCE_NET_ADHOC_PDP_MTU || (len != 0 && data == NULL))
		return SCE_ERROR_NET_ADHOC_INVALID_DATALEN;

	msgId = g_adhocPdpMsgId++;
	while (offset < len || head == NULL) {
		fragLen = len - offset;
		if (fragLen > SCE_NET_ADHOC_PDP_MFS)
			fragLen = SCE_NET_ADHOC_PDP_MFS;
		pdpLen = fragLen + 12;
		padLen = pdpLen % 16;
		if (padLen > 0)
			padLen = 16 - padLen;
		totalLen = fragLen + padLen + PSPNET_ADHOC_PKT_HDR_LEN;
		m = pdpAllocFrameMbuf(totalLen);
		if (m == NULL) {
			err = SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE;
			break;
		}
		frame = m->data;
		sceClibMemcpy_safe(frame, daddr, 6);
		sceClibMemcpy_safe(frame + 6, &pcb->laddr, 6);
		sceClibMemcpy_safe(frame + 12, g_adhocEtherType, 2);
		sceClibMemcpy_safe(frame + 14, g_adhocDataSubType, 2);
		frame[48] = PSPNET_ADHOC_PROTO_PDP;
		frame[49] = (offset + fragLen == len) ? 0 : 1;
		net16 = sceNetHtons((SceUShort16)pdpLen);
		sceClibMemcpy_safe(frame + 50, &net16, 2);
		net16 = sceNetHtons(pcb->lport);
		sceClibMemcpy_safe(frame + 52, &net16, 2);
		net16 = sceNetHtons(dport);
		sceClibMemcpy_safe(frame + 54, &net16, 2);
		net16 = sceNetHtons(msgId);
		sceClibMemcpy_safe(frame + 56, &net16, 2);
		net16 = sceNetHtons((SceUShort16)offset);
		sceClibMemcpy_safe(frame + 58, &net16, 2);
		if (fragLen > 0)
			sceClibMemcpy_safe(frame + PSPNET_ADHOC_PKT_HDR_LEN,
					   (const char *)data + offset, (SceSize)fragLen);
		if (padLen > 0)
			sceClibMemset(frame + PSPNET_ADHOC_PKT_HDR_LEN + fragLen, 0,
				      (SceSize)padLen);
		m->len = totalLen;
		if (isBcast)
			m->flags |= PSPNET_ADHOC_M_BCAST;
		m->storage.mh.pktHdr.rcvif = NULL;
		m->storage.mh.pktHdr.len = totalLen;
		if (head != NULL)
			*tail = m;
		else
			head = m;
		tail = &m->nextPkt;
		offset = (SceUShort16)(offset + fragLen);
	}

	if (err == 0) {
		while ((m = head) != NULL) {
			if (sock->flags & SCE_NET_ADHOC_F_ALERTSEND) {
				err = SCE_ERROR_NET_ADHOC_SOCKET_ALERTED;
				break;
			}
			head = m->nextPkt;
			m->nextPkt = NULL;
			adhocOutput(NULL, m);
		}
	}
	while ((m = head) != NULL) {
		head = m->nextPkt;
		m->nextPkt = NULL;
		adhocMbufFreeChain(m);
	}
	return err;
}

__attribute__((noinline))
int sceNetAdhocPdpSend(int id, const struct SceNetEtherAddr *daddr, SceUShort16 dport,
		       const void *data, int len, unsigned int timeout, int flag)
{
	int ret;
	AdhocThreadInfo *ti;
	int s;
	AdhocSocket *sock;
	AdhocThreadInfo *prev;

	(void)timeout;
	if (daddr == NULL || (flag & ~SCE_NET_ADHOC_F_NONBLOCK) != 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ARG;
	ti = adhocThreadEnter(&ret);
	if (ti == NULL)
		return ret;
	s = adhocSplNet();
	if (s < 0) {
		ret = s;
	} else if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PDP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else {
			ti->sockNext = sock->waiters;
			sock->waiters = ti;
			ret = adhocPdpSendInternal(sock, daddr, dport, data, len);
			if (sock->waiters == ti) {
				sock->waiters = sock->waiters->sockNext;
			} else {
				prev = sock->waiters;
				while (prev->sockNext != ti)
					prev = prev->sockNext;
				prev->sockNext = prev->sockNext->sockNext;
			}
			if (sock->flags & PSPNET_ADHOC_SF_DELETED) {
				if (sock->waiters == NULL) {
					adhocPdpDetach(sock);
					adhocSocketFree(sock);
				}
				ret = SCE_ERROR_NET_ADHOC_SOCKET_DELETED;
			}
		}
	}
	adhocSplX(s);
	adhocThreadLeave(ti, 0);
	return ret;
}

__attribute__((noinline))
int adhocPdpRecvInternal(int id, SceNetEtherAddr *saddr, SceUShort16 *sport,
			 void *buf, int *len, unsigned int timeout, int flag,
			 AdhocPdpRxInfo *rxInfo)
{
	int ret;
	AdhocThreadInfo *ti;
	int s;
	AdhocSocket *sock;
	AdhocPdpPcb *pcb;
	AdhocMbuf *m;
	SceUInt64 startTime, elapsed;
	int waitTime;
	SceUShort16 net16;
	int payloadLen;
	AdhocThreadInfo *prev;

	ti = adhocThreadEnter(&ret);
	if (ti == NULL)
		return ret;
	m = NULL;
	s = adhocSplNet();
	if (s < 0) {
		ret = s;
	} else if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PDP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else {
			ti->sockNext = sock->waiters;
			sock->waiters = ti;
			pcb = (AdhocPdpPcb *)sock->pcb;
			startTime = 0;
			for (;;) {
				if (sock->flags & SCE_NET_ADHOC_F_ALERTRECV) {
					ret = SCE_ERROR_NET_ADHOC_SOCKET_ALERTED;
					break;
				}
				m = pcb->rcv.head;
				if (m != NULL)
					break;
				if (flag & SCE_NET_ADHOC_F_NONBLOCK) {
					ret = SCE_ERROR_NET_ADHOC_WOULD_BLOCK;
					break;
				}
				if (timeout != 0) {
					if (startTime == 0)
						startTime = sceKernelGetProcessTimeWide();
					elapsed = sceKernelGetProcessTimeWide() - startTime;
					if (elapsed >= (SceUInt64)timeout) {
						adhocMbufFreeChain(NULL);
						ret = SCE_ERROR_NET_ADHOC_TIMEOUT;
						break;
					}
					waitTime = (int)((SceUInt64)timeout - elapsed);
				} else {
					waitTime = 0;
				}
				pcb->rcv.flags |= PSPNET_ADHOC_SB_WAIT;
				ret = adhocSleep(&pcb->rcv, waitTime);
				if (sock->flags & SCE_NET_ADHOC_F_ALERTRECV)
					ret = SCE_ERROR_NET_ADHOC_SOCKET_ALERTED;
				if (sock->flags & PSPNET_ADHOC_SF_DELETED)
					ret = SCE_ERROR_NET_ADHOC_SOCKET_DELETED;
				if (ret < 0)
					break;
			}
			if (m != NULL) {
				pcb->rcv.head = m->nextPkt;
				if (pcb->rcv.head == NULL)
					pcb->rcv.tail = NULL;
				m->nextPkt = NULL;
				pcb->rcv.cc -= (unsigned int)(m->storage.mh.pktHdr.len -
							      PSPNET_ADHOC_PKT_HDR_LEN);
				if (saddr != NULL)
					sceClibMemcpy_safe(saddr, m->data + 6, 6);
				sceClibMemcpy_safe(&net16, m->data + 52, 2);
				if (sport != NULL)
					*sport = sceNetNtohs(net16);
				sceClibMemcpy_safe(&net16, m->data + 50, 2);
				payloadLen = (int)sceNetNtohs(net16) - 12;
				if (*len < payloadLen) {
					m->nextPkt = pcb->rcv.head;
					if (pcb->rcv.tail == NULL)
						pcb->rcv.tail = m;
					pcb->rcv.head = m;
					pcb->rcv.cc += (unsigned int)(m->storage.mh.pktHdr.len -
								      PSPNET_ADHOC_PKT_HDR_LEN);
					*len = payloadLen;
					ret = SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE;
				} else {
					*len = payloadLen;
					if (payloadLen > 0)
						adhocMbufCopyData(m, PSPNET_ADHOC_PKT_HDR_LEN,
								  payloadLen, buf);
					if (rxInfo != NULL) {
						rxInfo->rssi = m->storage.mh.pktHdr.rssi;
						rxInfo->channel = m->storage.mh.pktHdr.channel;
					}
					adhocMbufFreeChain(m);
					ret = 0;
				}
			}
			if (sock->waiters == ti) {
				sock->waiters = sock->waiters->sockNext;
			} else {
				prev = sock->waiters;
				while (prev->sockNext != ti)
					prev = prev->sockNext;
				prev->sockNext = prev->sockNext->sockNext;
			}
			if (sock->flags & PSPNET_ADHOC_SF_DELETED) {
				if (sock->waiters == NULL) {
					adhocPdpDetach(sock);
					adhocSocketFree(sock);
				}
				ret = SCE_ERROR_NET_ADHOC_SOCKET_DELETED;
			}
		}
	}
	adhocSplX(s);
	adhocThreadLeave(ti, 0);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocPdpRecv(int id, struct SceNetEtherAddr *saddr, SceUShort16 *sport,
		       void *buf, int *len, unsigned int timeout, int flag)
{
	if (buf == NULL || len == NULL || (flag & ~SCE_NET_ADHOC_F_NONBLOCK) != 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ARG;
	return adhocPdpRecvInternal(id, saddr, sport, buf, len, timeout, flag, NULL);
}

__attribute__((noinline))
int sceNetAdhocPdpDelete(int id, int flag)
{
	int s, ret = 0;
	AdhocSocket *sock;
	AdhocPdpPcb *pcb;

	if (flag != 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ARG;
	s = adhocSplNet();
	if (s < 0)
		return s;
	if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PDP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else {
			pcb = (AdhocPdpPcb *)sock->pcb;
			if (pcb->rcv.flags & PSPNET_ADHOC_SB_WAIT) {
				pcb->rcv.flags &= ~PSPNET_ADHOC_SB_WAIT;
				adhocWakeup(&pcb->rcv);
			}
			if (pcb->rcv.flags & PSPNET_ADHOC_SB_SEL) {
				pcb->rcv.flags &= ~PSPNET_ADHOC_SB_SEL;
				adhocWakeup(&g_adhocPollChannel);
			}
			adhocWakeup(sock->sendWchan);
			adhocWakeup(&g_adhocPollChannel);
			adhocPdpFlush(pcb);
			sock->flags |= PSPNET_ADHOC_SF_DELETED;
			sock->id = -1;
			if (sock->waiters == NULL) {
				adhocPdpDetach(sock);
				adhocSocketFree(sock);
			}
		}
	}
	adhocSplX(s);
	return ret;
}

__attribute__((noinline))
int adhocPdpInit(void)
{
	g_adhocPdpList.first = NULL;
	g_adhocPdpList.last = &g_adhocPdpList.first;
	g_adhocPdpLastPort = (SceUShort16)(adhocRandNext() / 0x20009U + 0x4000U);
	g_adhocPdpMsgId = (SceUShort16)(adhocRandNext() / 0x8001U);
	return 0;
}

static inline __attribute__((always_inline)) void pdpDetachInline(AdhocSocket *sock)
{
	AdhocPdpPcb *pcb = (AdhocPdpPcb *)sock->pcb;

	sock->pcb = NULL;
	if (pcb->next != NULL)
		pcb->next->prev = pcb->prev;
	else
		g_adhocPdpList.last = pcb->prev;
	*pcb->prev = pcb->next;
	adhocFree(pcb);
}

__attribute__((noinline))
void adhocPdpDetach(AdhocSocket *sock)
{
	pdpDetachInline(sock);
}

__attribute__((noinline))
void adhocPdpFlush(AdhocPdpPcb *pcb)
{
	AdhocMbuf *m;
	AdhocPdpReasm *reasm;

	while ((m = pcb->rcv.head) != NULL) {
		pcb->rcv.head = m->nextPkt;
		if (pcb->rcv.head == NULL)
			pcb->rcv.tail = NULL;
		m->nextPkt = NULL;
		pcb->rcv.cc -= (unsigned int)(m->storage.mh.pktHdr.len -
					      PSPNET_ADHOC_PKT_HDR_LEN);
		adhocMbufFreeChain(m);
	}
	pcb->rcv.hiwat = 0;
	while ((reasm = pcb->reasmHead) != NULL) {
		adhocCalloutStop(&reasm->timer);
		adhocMbufFreeChain(reasm->mbuf);
		if (reasm->next != NULL)
			reasm->next->prev = reasm->prev;
		else
			pcb->reasmTail = reasm->prev;
		*reasm->prev = reasm->next;
		adhocFree(reasm);
	}
}

__attribute__((noinline))
void adhocPdpTerm(void)
{
	AdhocPdpPcb *pcb;

	while ((pcb = g_adhocPdpList.first) != NULL) {
		adhocPdpFlush(pcb);
		pdpDetachInline(pcb->sock);
	}
}

__attribute__((noinline))
int adhocPdpAttach(AdhocSocket *sock)
{
	AdhocPdpPcb *pcb;

	pcb = (AdhocPdpPcb *)adhocMalloc(sizeof(*pcb));
	if (pcb == NULL)
		return SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE;
	sceClibMemset(pcb, 0, sizeof(*pcb));
	pcb->sock = sock;
	pcb->reasmHead = NULL;
	pcb->reasmTail = &pcb->reasmHead;
	sock->pcb = pcb;
	pcb->next = NULL;
	pcb->prev = g_adhocPdpList.last;
	*g_adhocPdpList.last = pcb;
	g_adhocPdpList.last = &pcb->next;
	return 0;
}

__attribute__((noinline))
int adhocPdpBind(AdhocPdpPcb *pcb, const SceNetEtherAddr *saddr, SceUShort16 sport)
{
	SceUShort16 port = sport;
	AdhocPdpPcb *cur;

	if (port == 0) {
		port = (g_adhocPdpLastPort == 0x7fff) ? 0x4000 : (SceUShort16)(g_adhocPdpLastPort + 1);
		if (port == g_adhocPdpLastPort) {
			port = 0;
		} else {
			for (;;) {
				for (cur = g_adhocPdpList.first; cur != NULL; cur = cur->next) {
					if (sceClibMemcmp(&cur->laddr, saddr, sizeof(*saddr)) == 0 &&
					    cur->lport == port)
						break;
				}
				if (cur == NULL) {
					g_adhocPdpLastPort = port;
					break;
				}
				port = (port == 0x7fff) ? 0x4000 : (SceUShort16)(port + 1);
				if (port == g_adhocPdpLastPort) {
					port = 0;
					break;
				}
			}
		}
		if (port == 0)
			return SCE_ERROR_NET_ADHOC_PORT_NOT_AVAIL;
	} else {
		if (port == 0xffff)
			return SCE_ERROR_NET_ADHOC_INVALID_PORT;
		for (cur = g_adhocPdpList.first; cur != NULL; cur = cur->next) {
			if (sceClibMemcmp(&cur->laddr, saddr, sizeof(*saddr)) == 0 &&
			    cur->lport == port)
				return SCE_ERROR_NET_ADHOC_PORT_IN_USE;
		}
	}
	sceClibMemcpy_safe(&pcb->laddr, saddr, sizeof(pcb->laddr));
	pcb->lport = port;
	pcb->sock->recvWchan = &pcb->rcv;
	pcb->sock->pollWchan = &g_adhocPollChannel;
	return 0;
}

__attribute__((noinline))
int adhocPdpSetBufSize(AdhocPdpPcb *pcb, unsigned int bufsize)
{
	pcb->rcv.hiwat = bufsize;
	return 0;
}

__attribute__((noinline))
void adhocPdpInput(AdhocMbuf *m)
{
	char *frame;
	SceUShort16 net16, dport, pdpLen, fragOff, totalLen;
	int frameLen;
	SceNetEtherAddr localAddr;
	AdhocPdpPcb *pcb;
	AdhocPdpReasm *reasm;
	SceUChar8 moreFrags;

	if (m->storage.mh.pktHdr.len < PSPNET_ADHOC_PKT_HDR_LEN) {
		adhocMbufFreeChain(m);
		return;
	}
	if (m->len < PSPNET_ADHOC_PKT_HDR_LEN) {
		m = adhocMbufPullup(m, PSPNET_ADHOC_PKT_HDR_LEN);
		if (m == NULL)
			return;
	}
	frame = m->data;
	sceClibMemcpy_safe(&net16, frame + 50, 2);
	if (sceNetNtohs(net16) < 12) {
		adhocMbufFreeChain(m);
		return;
	}
	pdpLen = sceNetNtohs(net16);
	frameLen = (int)pdpLen + PSPNET_ADHOC_FRAME_HDR_LEN;
	if (m->storage.mh.pktHdr.len < frameLen) {
		adhocMbufFreeChain(m);
		return;
	}
	if (m->storage.mh.pktHdr.len > frameLen) {
		if (m->len == m->storage.mh.pktHdr.len) {
			m->len = frameLen;
			m->storage.mh.pktHdr.len = frameLen;
		} else {
			adhocMbufAdj(m, frameLen - m->storage.mh.pktHdr.len);
		}
	}
	sceClibMemcpy_safe(&net16, frame + 54, 2);
	dport = sceNetNtohs(net16);
	if (sceClibMemcmp(g_adhocBroadcastAddr, frame, 6) == 0) {
		adhocAuthGetLocalAddr(&localAddr);
		for (pcb = g_adhocPdpList.first; pcb != NULL; pcb = pcb->next) {
			if (sceClibMemcmp(&pcb->laddr, &localAddr, 6) == 0 && pcb->lport == dport)
				break;
		}
	} else {
		for (pcb = g_adhocPdpList.first; pcb != NULL; pcb = pcb->next) {
			if (sceClibMemcmp(&pcb->laddr, frame, 6) == 0 && pcb->lport == dport)
				break;
		}
	}
	if (pcb == NULL) {
		g_adhocPdpRxNoSocket++;
		adhocMbufFreeChain(m);
		return;
	}
	if (frame[58] != 0 || frame[59] != 0 || (frame[49] & 1) != 0) {
		for (reasm = pcb->reasmHead; reasm != NULL; reasm = reasm->next) {
			char *headFrame = reasm->mbuf->data;
			if (sceClibMemcmp(frame + 6, headFrame + 6, 6) == 0 &&
			    sceClibMemcmp(frame + 56, headFrame + 56, 2) == 0)
				break;
		}
		if (reasm == NULL) {
			if (frame[58] != 0 || frame[59] != 0) {
				g_adhocPdpRxFragDrop++;
				adhocMbufFreeChain(m);
				return;
			}
			reasm = (AdhocPdpReasm *)adhocMalloc(sizeof(*reasm));
			if (reasm == NULL) {
				g_adhocPdpRxFragDrop++;
				adhocMbufFreeChain(m);
				return;
			}
			sceClibMemcpy_safe(&reasm->totalLen, frame + 50, 2);
			reasm->totalLen = sceNetNtohs(reasm->totalLen);
			reasm->mbuf = m;
			adhocCalloutInit(&reasm->timer);
			reasm->pcb = pcb;
			reasm->next = NULL;
			reasm->prev = pcb->reasmTail;
			*pcb->reasmTail = reasm;
			pcb->reasmTail = &reasm->next;
			g_adhocPdpRxFrags++;
			adhocCalloutReset(&reasm->timer, 1000000, adhocPdpReasmTimeout, reasm);
			return;
		}
		sceClibMemcpy_safe(&net16, frame + 58, 2);
		fragOff = sceNetNtohs(net16);
		if (fragOff != (SceUShort16)(reasm->totalLen - 12)) {
			adhocCalloutStop(&reasm->timer);
			adhocMbufFreeChain(reasm->mbuf);
			if (reasm->next != NULL)
				reasm->next->prev = reasm->prev;
			else
				pcb->reasmTail = reasm->prev;
			*reasm->prev = reasm->next;
			adhocFree(reasm);
			g_adhocPdpRxFragDrop++;
			adhocMbufFreeChain(m);
			return;
		}
		sceClibMemcpy_safe(&net16, frame + 50, 2);
		totalLen = (SceUShort16)(reasm->totalLen + sceNetNtohs(net16) - 12);
		reasm->totalLen = totalLen;
		net16 = sceNetHtons(totalLen);
		sceClibMemcpy_safe(reasm->mbuf->data + 50, &net16, 2);
		moreFrags = (SceUChar8)frame[49];
		adhocMbufAdj(m, PSPNET_ADHOC_PKT_HDR_LEN);
		reasm->mbuf->storage.mh.pktHdr.len += m->storage.mh.pktHdr.len;
		adhocMbufCat(reasm->mbuf, m);
		g_adhocPdpRxFrags++;
		if (moreFrags & 1) {
			adhocCalloutReset(&reasm->timer, 1000000, adhocPdpReasmTimeout, reasm);
			return;
		}
		adhocCalloutStop(&reasm->timer);
		m = reasm->mbuf;
		if (reasm->next != NULL)
			reasm->next->prev = reasm->prev;
		else
			pcb->reasmTail = reasm->prev;
		*reasm->prev = reasm->next;
		adhocFree(reasm);
	}

	if (pcb->rcv.cc + (unsigned int)(m->storage.mh.pktHdr.len - PSPNET_ADHOC_PKT_HDR_LEN) >
	    pcb->rcv.hiwat) {
		g_adhocPdpRxDropFull++;
		adhocMbufFreeChain(m);
		return;
	}
	m->nextPkt = NULL;
	if (pcb->rcv.tail != NULL)
		pcb->rcv.tail->nextPkt = m;
	else
		pcb->rcv.head = m;
	pcb->rcv.tail = m;
	pcb->rcv.cc += (unsigned int)(m->storage.mh.pktHdr.len - PSPNET_ADHOC_PKT_HDR_LEN);
	if (pcb->rcv.flags & PSPNET_ADHOC_SB_WAIT) {
		pcb->rcv.flags &= ~PSPNET_ADHOC_SB_WAIT;
		adhocWakeup(&pcb->rcv);
	}
	if (pcb->rcv.flags & PSPNET_ADHOC_SB_SEL) {
		pcb->rcv.flags &= ~PSPNET_ADHOC_SB_SEL;
		adhocWakeup(&g_adhocPollChannel);
	}
	g_adhocPdpRxPackets++;
}

__attribute__((noinline))
int adhocPdpPoll(AdhocSocket *sock, int events)
{
	AdhocPdpPcb *pcb = (AdhocPdpPcb *)sock->pcb;
	int revents = 0;

	if ((events & SCE_NET_ADHOC_EV_RECV) && pcb->rcv.head != NULL)
		revents |= SCE_NET_ADHOC_EV_RECV;
	if (events & SCE_NET_ADHOC_EV_SEND)
		revents |= SCE_NET_ADHOC_EV_SEND;
	if (revents == 0) {
		if (events & SCE_NET_ADHOC_EV_RECV)
			pcb->rcv.flags |= PSPNET_ADHOC_SB_SEL;
		if (events & SCE_NET_ADHOC_EV_SEND)
			g_adhocSendPollFlags |= PSPNET_ADHOC_SB_SEL;
	}
	return revents;
}

__attribute__((noinline))
int sceNetAdhocGetPdpStat(int *buflen, void *buf)
{
	int s, total = 0, ret = 0;
	AdhocPdpPcb *pcb;
	struct SceNetAdhocPdpStat *cur = (struct SceNetAdhocPdpStat *)buf;
	struct SceNetAdhocPdpStat *prev = NULL;

	if (buflen == NULL)
		return SCE_ERROR_NET_ADHOC_INVALID_ARG;
	if (((SceUIntPtr)buf & 3) != 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ALIGNMENT;
	s = adhocSplNet();
	if (s < 0)
		return s;
	if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		for (pcb = g_adhocPdpList.first; pcb != NULL; pcb = pcb->next) {
			if (buf != NULL) {
				if (total + (int)sizeof(*cur) > *buflen)
					break;
				cur->next = NULL;
				cur->id = pcb->sock->id;
				sceClibMemcpy_safe(&cur->laddr, &pcb->laddr, sizeof(cur->laddr));
				cur->lport = pcb->lport;
				cur->rcv_sb_cc = pcb->rcv.cc;
				if (prev != NULL)
					prev->next = cur;
				prev = cur;
			}
			cur++;
			total += (int)sizeof(*cur);
		}
	}
	adhocSplX(s);
	*buflen = total;
	return ret;
}

__attribute__((noinline))
void adhocPdpReasmTimeout(void *arg)
{
	AdhocPdpReasm *reasm = (AdhocPdpReasm *)arg;

	adhocCalloutStop(&reasm->timer);
	adhocMbufFreeChain(reasm->mbuf);
	if (reasm->next != NULL)
		reasm->next->prev = reasm->prev;
	else
		reasm->pcb->reasmTail = reasm->prev;
	*reasm->prev = reasm->next;
	adhocFree(reasm);
	g_adhocPdpRxFragTimeout++;
}
