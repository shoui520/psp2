/*
 * adhoc_ptp.c: Packet Transmission Protocol (PTP) reliable stream sockets,
 * state machine, retransmission timers, and statistics for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

typedef struct AdhocPtpList {
	AdhocPtpPcb *first;
	AdhocPtpPcb **last;
} AdhocPtpList;

static SceUShort16 g_adhocPtpLastPort;
static SceUInt32 g_adhocPtpIss;
static AdhocPtpList g_adhocPtpList;

static SceUInt32 g_adhocPtpTxSyn;
static SceUInt32 g_adhocPtpRxSynOk;
static SceUInt32 g_adhocPtpRxSynDup;
static SceUInt32 g_adhocPtpRxSynDrop;
static SceUInt32 g_adhocPtpTxSynAck;
static SceUInt32 g_adhocPtpRxSynAckOk;
static SceUInt32 g_adhocPtpRxSynAckDup;
static SceUInt32 g_adhocPtpRxSynAckDrop;
static SceUInt32 g_adhocPtpTxAckSyn;
static SceUInt32 g_adhocPtpRxAckSynOk;
static SceUInt32 g_adhocPtpRxAckSynDup;
static SceUInt32 g_adhocPtpRxAckSynDrop;
static SceUInt32 g_adhocPtpTxData;
static SceUInt32 g_adhocPtpRxDataOk;
static SceUInt32 g_adhocPtpRxDataDropFull;
static SceUInt32 g_adhocPtpRxDataDup;
static SceUInt32 g_adhocPtpRxDataDrop;
static SceUInt32 g_adhocPtpTxAck;
static SceUInt32 g_adhocPtpRxAckOk;
static SceUInt32 g_adhocPtpRxAckDup;
static SceUInt32 g_adhocPtpRxAckDrop;
static SceUInt32 g_adhocPtpTxNack;
static SceUInt32 g_adhocPtpRxNackOk;
static SceUInt32 g_adhocPtpRxNackDrop;
static SceUInt32 g_adhocPtpTxRst;
static SceUInt32 g_adhocPtpRxRstOk;
static SceUInt32 g_adhocPtpRxRstDrop;
static SceUInt32 g_adhocPtpRxNoSocket;
static SceUInt32 g_adhocPtpRexmtCount;
static SceUInt32 g_adhocPtpWindowProbe;
static SceUInt32 g_adhocPtpWindowWait;

static inline AdhocMbuf *ptpAllocMbuf(int totalLen)
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

static inline void ptpUnlinkWaiter(AdhocSocket *sock, AdhocThreadInfo *ti, int *ret)
{
	AdhocThreadInfo *prev;

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
			adhocPtpDetach(sock);
			adhocSocketFree(sock);
		}
		*ret = SCE_ERROR_NET_ADHOC_SOCKET_DELETED;
	}
}

__attribute__((noinline))
int sceNetAdhocPtpOpen(const struct SceNetEtherAddr *saddr, SceUShort16 sport,
		       const struct SceNetEtherAddr *daddr, SceUShort16 dport,
		       unsigned int bufsize, unsigned int rexmt_int, int rexmt_cnt, int flag)
{
	int s, id, ret;
	AdhocSocket *sock;

	if (saddr == NULL || daddr == NULL || flag != 0)
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
			ret = adhocSocketCreate(PSPNET_ADHOC_SOCK_PTP, &sock);
			if (ret >= 0) {
				ret = adhocPtpAttach(sock);
				if (ret >= 0) {
					ret = adhocPtpSetBufSize((AdhocPtpPcb *)sock->pcb,
								 bufsize, bufsize);
					if (ret >= 0) {
						ret = adhocPtpBind((AdhocPtpPcb *)sock->pcb,
								   saddr, sport, daddr, dport);
						if (ret >= 0) {
							adhocPtpSetRexmt((AdhocPtpPcb *)sock->pcb,
									 rexmt_int, rexmt_cnt);
							sock->id = id;
						} else {
							adhocPtpFlushBuffers((AdhocPtpPcb *)sock->pcb);
						}
					}
					if (ret < 0)
						adhocPtpDetach(sock);
				}
				if (ret < 0)
					adhocSocketFree(sock);
			}
		}
		if (ret >= 0) {
			ret = adhocPtpConnectStart((AdhocPtpPcb *)sock->pcb, daddr, dport);
			if (ret >= 0) {
				ret = sock->id;
			} else {
				adhocPtpFlushBuffers((AdhocPtpPcb *)sock->pcb);
				adhocPtpDetach(sock);
				adhocSocketFree(sock);
			}
		}
	}
	adhocSplX(s);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocPtpConnect(int id, unsigned int timeout, int flag)
{
	int ret;
	AdhocThreadInfo *ti;
	int s;
	AdhocSocket *sock;
	AdhocPtpPcb *pcb;
	SceUInt64 startTime, elapsed;
	int waitTime;

	if ((flag & ~SCE_NET_ADHOC_F_NONBLOCK) != 0)
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
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PTP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else {
			ti->sockNext = sock->waiters;
			sock->waiters = ti;
			pcb = (AdhocPtpPcb *)sock->pcb;
			startTime = 0;
			for (;;) {
				if (sock->flags & PSPNET_ADHOC_SF_CONNECTED) {
					ret = 0;
					break;
				}
				if (sock->flags & PSPNET_ADHOC_SF_DISCONNECTED) {
					ret = SCE_ERROR_NET_ADHOC_CONNECTION_REFUSED;
					break;
				}
				if (sock->flags & SCE_NET_ADHOC_F_ALERTCONNECT) {
					ret = SCE_ERROR_NET_ADHOC_SOCKET_ALERTED;
					break;
				}
				if ((sock->flags & PSPNET_ADHOC_SF_CONNECTING) == 0) {
					ret = SCE_ERROR_NET_ADHOC_NOT_OPENED;
					break;
				}
				if (flag & SCE_NET_ADHOC_F_NONBLOCK) {
					ret = SCE_ERROR_NET_ADHOC_WOULD_BLOCK;
					break;
				}
				if (timeout != 0) {
					if (startTime == 0)
						startTime = sceKernelGetProcessTimeWide();
					elapsed = sceKernelGetProcessTimeWide() - startTime;
					if (elapsed >= (SceUInt64)timeout) {
						ret = SCE_ERROR_NET_ADHOC_TIMEOUT;
						break;
					}
					waitTime = (int)((SceUInt64)timeout - elapsed);
				} else {
					waitTime = 0;
				}
				pcb->rcv.flags |= PSPNET_ADHOC_SB_WAIT;
				ret = adhocSleep(&pcb->rcv, waitTime);
				if (sock->flags & SCE_NET_ADHOC_F_ALERTCONNECT)
					ret = PSPNET_ADHOC_EIO;
				if ((sock->flags & PSPNET_ADHOC_SF_DELETED) == 0 && ret < 0)
					break;
			}
			ptpUnlinkWaiter(sock, ti, &ret);
		}
	}
	adhocSplX(s);
	adhocThreadLeave(ti, 0);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocPtpListen(const struct SceNetEtherAddr *saddr, SceUShort16 sport,
			 unsigned int bufsize, unsigned int rexmt_int, int rexmt_cnt,
			 int backlog, int flag)
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
			ret = adhocSocketCreate(PSPNET_ADHOC_SOCK_PTP, &sock);
			if (ret >= 0) {
				ret = adhocPtpAttach(sock);
				if (ret >= 0) {
					ret = adhocPtpSetBufSize((AdhocPtpPcb *)sock->pcb,
								 bufsize, bufsize);
					if (ret >= 0) {
						ret = adhocPtpBind((AdhocPtpPcb *)sock->pcb,
								   saddr, sport, NULL, 0);
						if (ret >= 0) {
							adhocPtpSetRexmt((AdhocPtpPcb *)sock->pcb,
									 rexmt_int, rexmt_cnt);
							sock->id = id;
						} else {
							adhocPtpFlushBuffers((AdhocPtpPcb *)sock->pcb);
						}
					}
					if (ret < 0)
						adhocPtpDetach(sock);
				}
				if (ret < 0)
					adhocSocketFree(sock);
			}
		}
		if (ret >= 0) {
			ret = adhocPtpListenStart((AdhocPtpPcb *)sock->pcb, backlog);
			if (ret >= 0) {
				ret = sock->id;
			} else {
				adhocPtpFlushBuffers((AdhocPtpPcb *)sock->pcb);
				adhocPtpDetach(sock);
				adhocSocketFree(sock);
			}
		}
	}
	adhocSplX(s);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocPtpAccept(int id, struct SceNetEtherAddr *addr, SceUShort16 *port,
			 unsigned int timeout, int flag)
{
	int ret;
	AdhocThreadInfo *ti;
	int s, newId;
	AdhocSocket *sock;
	AdhocPtpPcb *pcb, *child, *nextChild;
	SceUInt64 startTime, elapsed;
	int waitTime;

	if ((flag & ~SCE_NET_ADHOC_F_NONBLOCK) != 0)
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
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PTP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else {
			ti->sockNext = sock->waiters;
			sock->waiters = ti;
			pcb = (AdhocPtpPcb *)sock->pcb;
			startTime = 0;
			for (;;) {
				if (sock->flags & PSPNET_ADHOC_SF_DISCONNECTED) {
					ret = SCE_ERROR_NET_ADHOC_DISCONNECTED;
					break;
				}
				if (sock->flags & SCE_NET_ADHOC_F_ALERTACCEPT) {
					ret = SCE_ERROR_NET_ADHOC_SOCKET_ALERTED;
					break;
				}
				if ((sock->flags & PSPNET_ADHOC_SF_LISTEN) == 0) {
					ret = SCE_ERROR_NET_ADHOC_NOT_LISTENED;
					break;
				}
				for (child = pcb->qHead; child != NULL; child = nextChild) {
					nextChild = child->qNext;
					if (child->sock->flags & PSPNET_ADHOC_SF_CONNECTED)
						break;
					if (child->sock->flags & PSPNET_ADHOC_SF_DISCONNECTED) {
						if (nextChild != NULL)
							nextChild->qPrev = child->qPrev;
						else
							pcb->qTail = child->qPrev;
						*child->qPrev = child->qNext;
						pcb->qLen--;
						adhocPtpDetach(child->sock);
						adhocSocketFree(child->sock);
					}
				}
				if (child != NULL) {
					newId = adhocSocketAllocId();
					if (newId < 0) {
						ret = SCE_ERROR_NET_ADHOC_SOCKET_ID_NOT_AVAIL;
					} else {
						if (child->qNext != NULL)
							child->qNext->qPrev = child->qPrev;
						else
							pcb->qTail = child->qPrev;
						*child->qPrev = child->qNext;
						pcb->qLen--;
						child->parent = NULL;
						if (addr != NULL)
							sceClibMemcpy_safe(addr, &child->paddr,
									   sizeof(*addr));
						if (port != NULL)
							*port = child->pport;
						child->sock->id = newId;
						ret = newId;
					}
					break;
				}
				if (flag & SCE_NET_ADHOC_F_NONBLOCK) {
					ret = SCE_ERROR_NET_ADHOC_WOULD_BLOCK;
					break;
				}
				if (timeout != 0) {
					if (startTime == 0)
						startTime = sceKernelGetProcessTimeWide();
					elapsed = sceKernelGetProcessTimeWide() - startTime;
					if (elapsed >= (SceUInt64)timeout) {
						ret = SCE_ERROR_NET_ADHOC_TIMEOUT;
						break;
					}
					waitTime = (int)((SceUInt64)timeout - elapsed);
				} else {
					waitTime = 0;
				}
				pcb->rcv.flags |= PSPNET_ADHOC_SB_WAIT;
				ret = adhocSleep(&pcb->rcv, waitTime);
				if (sock->flags & SCE_NET_ADHOC_F_ALERTACCEPT)
					ret = PSPNET_ADHOC_EIO;
				if ((sock->flags & PSPNET_ADHOC_SF_DELETED) == 0 && ret < 0)
					break;
			}
			ptpUnlinkWaiter(sock, ti, &ret);
		}
	}
	adhocSplX(s);
	adhocThreadLeave(ti, 0);
	return ret;
}

__attribute__((noinline))
int adhocPtpSendInternal(AdhocSocket *sock, const void *data, int *len,
			 unsigned int timeout, int flag)
{
	AdhocPtpPcb *pcb = (AdhocPtpPcb *)sock->pcb;
	int origLen = *len;
	int remain = origLen;
	SceUInt64 startTime = 0, elapsed;
	int space, chunk, waitTime;
	int ret = 0;
	AdhocMbuf *m;

	if (origLen < 0 || (origLen != 0 && data == NULL))
		return SCE_ERROR_NET_ADHOC_INVALID_DATALEN;
	while (remain > 0) {
		if (sock->flags & PSPNET_ADHOC_SF_DISCONNECTED) {
			ret = SCE_ERROR_NET_ADHOC_DISCONNECTED;
			break;
		}
		if (sock->flags & SCE_NET_ADHOC_F_ALERTSEND) {
			ret = SCE_ERROR_NET_ADHOC_SOCKET_ALERTED;
			break;
		}
		space = (int)(pcb->snd.hiwat - pcb->snd.cc);
		if (remain <= space || space >= (int)pcb->snd.lowat) {
			if (remain >= space)
				chunk = (space > (int)pcb->mss) ? (int)pcb->mss : space;
			else
				chunk = (remain > (int)pcb->mss) ? (int)pcb->mss : remain;
			m = ptpAllocMbuf(chunk);
			if (m == NULL) {
				ret = SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE;
				break;
			}
			sceClibMemcpy_safe(m->data, (const char *)data + (*len - remain),
					   (SceSize)chunk);
			m->len = chunk;
			m->storage.mh.pktHdr.rcvif = NULL;
			m->storage.mh.pktHdr.len = chunk;
			sceClibMemset(&m->storage.mh.pktHdr.rssi, 0, 16);
			m->nextPkt = NULL;
			if (pcb->snd.tail != NULL)
				pcb->snd.tail->nextPkt = m;
			else
				pcb->snd.head = m;
			pcb->snd.tail = m;
			remain -= chunk;
			pcb->snd.cc += (unsigned int)m->storage.mh.pktHdr.len;
			adhocPtpOutput(pcb, 0);
			pcb->sndQueued++;
		} else {
			if (flag & SCE_NET_ADHOC_F_NONBLOCK) {
				ret = SCE_ERROR_NET_ADHOC_WOULD_BLOCK;
				break;
			}
			if (timeout != 0) {
				if (startTime == 0)
					startTime = sceKernelGetProcessTimeWide();
				elapsed = sceKernelGetProcessTimeWide() - startTime;
				if (elapsed >= (SceUInt64)timeout) {
					ret = SCE_ERROR_NET_ADHOC_TIMEOUT;
					break;
				}
				waitTime = (int)((SceUInt64)timeout - elapsed);
			} else {
				waitTime = 0;
			}
			pcb->snd.flags |= PSPNET_ADHOC_SB_WAIT;
			ret = adhocSleep(&pcb->snd, waitTime);
			if (sock->flags & SCE_NET_ADHOC_F_ALERTSEND)
				ret = PSPNET_ADHOC_EIO;
			if (sock->flags & PSPNET_ADHOC_SF_DELETED)
				ret = PSPNET_ADHOC_EPIPE;
			else if (ret < 0)
				break;
		}
	}
	*len -= remain;
	return ret;
}

__attribute__((noinline))
int sceNetAdhocPtpSend(int id, const void *data, int *len, unsigned int timeout, int flag)
{
	int ret;
	AdhocThreadInfo *ti;
	int s;
	AdhocSocket *sock;

	if (len == NULL || (flag & ~SCE_NET_ADHOC_F_NONBLOCK) != 0)
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
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PTP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else if ((sock->flags & (PSPNET_ADHOC_SF_CONNECTED |
					   PSPNET_ADHOC_SF_DISCONNECTED)) == 0) {
			ret = SCE_ERROR_NET_ADHOC_NOT_CONNECTED;
		} else {
			ti->sockNext = sock->waiters;
			sock->waiters = ti;
			ret = adhocPtpSendInternal(sock, data, len, timeout, flag);
			ptpUnlinkWaiter(sock, ti, &ret);
		}
	}
	adhocSplX(s);
	adhocThreadLeave(ti, 0);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocPtpRecv(int id, void *buf, int *len, unsigned int timeout, int flag)
{
	int ret;
	AdhocThreadInfo *ti;
	int s;
	AdhocSocket *sock;
	AdhocPtpPcb *pcb;
	int remain, oldSpace, newSpace, quarter;
	SceUInt64 startTime, elapsed;
	int waitTime;
	AdhocMbuf *m;

	if (buf == NULL || len == NULL || (flag & ~SCE_NET_ADHOC_F_NONBLOCK) != 0)
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
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PTP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else if ((sock->flags & (PSPNET_ADHOC_SF_CONNECTED |
					   PSPNET_ADHOC_SF_DISCONNECTED)) == 0) {
			ret = SCE_ERROR_NET_ADHOC_NOT_CONNECTED;
		} else {
			ti->sockNext = sock->waiters;
			sock->waiters = ti;
			pcb = (AdhocPtpPcb *)sock->pcb;
			startTime = 0;
			remain = *len;
			for (;;) {
				if ((sock->flags & PSPNET_ADHOC_SF_DISCONNECTED) && pcb->rcv.cc == 0) {
					ret = SCE_ERROR_NET_ADHOC_DISCONNECTED;
					break;
				}
				if (sock->flags & SCE_NET_ADHOC_F_ALERTRECV) {
					ret = SCE_ERROR_NET_ADHOC_SOCKET_ALERTED;
					break;
				}
				if (pcb->rcv.cc >= pcb->rcv.lowat ||
				    (sock->flags & PSPNET_ADHOC_SF_DISCONNECTED)) {
					ret = 0;
					oldSpace = (int)(pcb->rcv.hiwat - pcb->rcv.cc);
					while ((m = pcb->rcv.head) != NULL && remain > 0) {
						char *dst = (char *)buf + (*len - remain);
						if (remain >= m->storage.mh.pktHdr.len) {
							adhocMbufCopyData(m, 0,
									  m->storage.mh.pktHdr.len,
									  dst);
							remain -= m->storage.mh.pktHdr.len;
							m = pcb->rcv.head;
							if (m != NULL) {
								pcb->rcv.head = m->nextPkt;
								if (pcb->rcv.head == NULL)
									pcb->rcv.tail = NULL;
								m->nextPkt = NULL;
								pcb->rcv.cc -= (unsigned int)m->storage.mh.pktHdr.len;
							}
							adhocMbufFreeChain(m);
						} else {
							adhocMbufCopyData(m, 0, remain, dst);
							adhocMbufAdj(m, remain);
							pcb->rcv.cc -= (unsigned int)remain;
							remain = 0;
						}
					}
					quarter = (int)(pcb->rcv.hiwat / 4);
					newSpace = (int)(pcb->rcv.hiwat - pcb->rcv.cc);
					if (oldSpace < quarter && oldSpace < (int)pcb->mss &&
					    (newSpace >= quarter || newSpace >= (int)pcb->mss))
						adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_ACK);
					*len -= remain;
					break;
				}
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
					ret = PSPNET_ADHOC_EIO;
				if ((sock->flags & PSPNET_ADHOC_SF_DELETED) == 0 && ret < 0)
					break;
			}
			ptpUnlinkWaiter(sock, ti, &ret);
		}
	}
	adhocSplX(s);
	adhocThreadLeave(ti, 0);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocPtpFlush(int id, unsigned int timeout, int flag)
{
	int ret;
	AdhocThreadInfo *ti;
	int s;
	AdhocSocket *sock;
	AdhocPtpPcb *pcb;
	SceUInt64 startTime, elapsed;
	int waitTime;

	if ((flag & ~SCE_NET_ADHOC_F_NONBLOCK) != 0)
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
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PTP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else if ((sock->flags & (PSPNET_ADHOC_SF_CONNECTED |
					   PSPNET_ADHOC_SF_DISCONNECTED)) == 0) {
			ret = SCE_ERROR_NET_ADHOC_NOT_CONNECTED;
		} else {
			ti->sockNext = sock->waiters;
			sock->waiters = ti;
			pcb = (AdhocPtpPcb *)sock->pcb;
			startTime = 0;
			ret = 0;
			while (pcb->snd.cc != 0) {
				if (sock->flags & PSPNET_ADHOC_SF_DISCONNECTED) {
					ret = SCE_ERROR_NET_ADHOC_DISCONNECTED;
					break;
				}
				if (sock->flags & SCE_NET_ADHOC_F_ALERTFLUSH) {
					ret = SCE_ERROR_NET_ADHOC_SOCKET_ALERTED;
					break;
				}
				if (flag & SCE_NET_ADHOC_F_NONBLOCK) {
					ret = SCE_ERROR_NET_ADHOC_WOULD_BLOCK;
					break;
				}
				if (timeout != 0) {
					if (startTime == 0)
						startTime = sceKernelGetProcessTimeWide();
					elapsed = sceKernelGetProcessTimeWide() - startTime;
					if (elapsed >= (SceUInt64)timeout) {
						ret = SCE_ERROR_NET_ADHOC_TIMEOUT;
						break;
					}
					waitTime = (int)((SceUInt64)timeout - elapsed);
				} else {
					waitTime = 0;
				}
				pcb->snd.flags |= PSPNET_ADHOC_SB_WAIT;
				ret = adhocSleep(&pcb->snd, waitTime);
				if (sock->flags & SCE_NET_ADHOC_F_ALERTFLUSH)
					ret = PSPNET_ADHOC_EIO;
				if ((sock->flags & PSPNET_ADHOC_SF_DELETED) == 0 && ret < 0)
					break;
				ret = 0;
			}
			ptpUnlinkWaiter(sock, ti, &ret);
		}
	}
	adhocSplX(s);
	adhocThreadLeave(ti, 0);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocPtpClose(int id, int flag)
{
	int s, ret = 0;
	AdhocSocket *sock;
	AdhocPtpPcb *pcb;

	if (flag != 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ARG;
	s = adhocSplNet();
	if (s < 0)
		return s;
	if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		sock = adhocSocketFind(PSPNET_ADHOC_SOCK_PTP, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else {
			adhocPtpDisconnect((AdhocPtpPcb *)sock->pcb);
			adhocPtpCloseChildren((AdhocPtpPcb *)sock->pcb);
			sock->flags |= PSPNET_ADHOC_SF_DELETED;
			sock->id = -1;
			if (sock->waiters == NULL) {
				adhocPtpDetach(sock);
				adhocSocketFree(sock);
			}
		}
	}
	adhocSplX(s);
	return ret;
}

__attribute__((noinline))
void adhocPtpInput(AdhocMbuf *m)
{
	SceUChar8 *frame;
	SceUShort16 ptpLen, sport, dport;
	SceUInt32 seq, wnd = 0;
	int frameLen;
	SceUChar8 opcode;
	AdhocPtpPcb *pcb, *child;
	AdhocMbuf *q;

	if (m->storage.mh.pktHdr.len < PSPNET_ADHOC_PKT_HDR_LEN) {
		adhocMbufFreeChain(m);
		return;
	}
	if (m->len < PSPNET_ADHOC_PKT_HDR_LEN) {
		m = adhocMbufPullup(m, PSPNET_ADHOC_PKT_HDR_LEN);
		if (m == NULL)
			return;
	}
	frame = (SceUChar8 *)m->data;
	sceClibMemcpy_safe(&ptpLen, frame + 50, 2);
	ptpLen = sceNetNtohs(ptpLen);
	sceClibMemcpy_safe(&sport, frame + 52, 2);
	sport = sceNetNtohs(sport);
	sceClibMemcpy_safe(&dport, frame + 54, 2);
	dport = sceNetNtohs(dport);
	sceClibMemcpy_safe(&seq, frame + 56, 4);
	seq = sceNetNtohl(seq);
	if (ptpLen < 12) {
		adhocMbufFreeChain(m);
		return;
	}
	frameLen = (int)ptpLen + PSPNET_ADHOC_FRAME_HDR_LEN;
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
	opcode = frame[49];
	if (opcode == 0 || opcode > PSPNET_ADHOC_PTP_OP_RST) {
		adhocMbufFreeChain(m);
		return;
	}
	if (opcode == PSPNET_ADHOC_PTP_OP_SYN || opcode == PSPNET_ADHOC_PTP_OP_SYN_ACK ||
	    opcode == PSPNET_ADHOC_PTP_OP_ACK) {
		if (ptpLen < 16) {
			adhocMbufFreeChain(m);
			return;
		}
		if (m->len >= PSPNET_ADHOC_PKT_HDR_LEN + 4)
			sceClibMemcpy_safe(&wnd, m->data + PSPNET_ADHOC_PKT_HDR_LEN, 4);
		else
			adhocMbufCopyData(m, PSPNET_ADHOC_PKT_HDR_LEN, 4, &wnd);
		wnd = sceNetNtohl(wnd);
	}

	for (;;) {
		pcb = adhocPtpLookup((const SceNetEtherAddr *)frame, dport,
				     (const SceNetEtherAddr *)(frame + 6), sport);
		if (pcb == NULL && opcode == PSPNET_ADHOC_PTP_OP_SYN)
			pcb = adhocPtpLookup((const SceNetEtherAddr *)frame, dport, NULL, 0);
		if (pcb == NULL) {
			if (opcode != PSPNET_ADHOC_PTP_OP_RST)
				adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
						   (const SceNetEtherAddr *)(frame + 6), sport,
						   PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
			g_adhocPtpRxNoSocket++;
			adhocMbufFreeChain(m);
			return;
		}
		switch (opcode) {
		case PSPNET_ADHOC_PTP_OP_SYN:
			switch (pcb->state) {
			case SCE_NET_ADHOC_PTP_STATE_CLOSED:
				g_adhocPtpRxSynDrop++;
				adhocMbufFreeChain(m);
				return;
			case SCE_NET_ADHOC_PTP_STATE_LISTEN:
				child = adhocPtpAcceptChild(pcb,
							    (const SceNetEtherAddr *)(frame + 6),
							    sport, seq);
				if (child != NULL) {
					child->sndWnd = wnd;
					if (child->mss > wnd)
						child->mss = wnd;
					adhocPtpSendControl(child, PSPNET_ADHOC_PTP_OP_SYN_ACK);
					child->rexmtRemain = child->rexmtCnt;
					adhocCalloutReset(&child->rexmtTimer, (int)child->rexmtInt,
							  adhocPtpRexmtTimeout, child);
					g_adhocPtpRxSynOk++;
				} else {
					g_adhocPtpRxSynDrop++;
				}
				adhocMbufFreeChain(m);
				return;
			case SCE_NET_ADHOC_PTP_STATE_SYN_RCVD:
				if (pcb->iss == seq) {
					adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_SYN_ACK);
					pcb->rexmtRemain = pcb->rexmtCnt;
					adhocCalloutReset(&pcb->rexmtTimer, (int)pcb->rexmtInt,
							  adhocPtpRexmtTimeout, pcb);
					g_adhocPtpRxSynDup++;
					adhocMbufFreeChain(m);
					return;
				}
				adhocPtpDisconnect(pcb);
				continue;
			case SCE_NET_ADHOC_PTP_STATE_SYN_SENT:
			case SCE_NET_ADHOC_PTP_STATE_ESTABLISHED:
				adhocPtpDisconnect(pcb);
				continue;
			default:
				adhocMbufFreeChain(m);
				return;
			}
		case PSPNET_ADHOC_PTP_OP_SYN_ACK:
			switch (pcb->state) {
			case SCE_NET_ADHOC_PTP_STATE_CLOSED:
				g_adhocPtpRxSynAckDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_SYN_SENT:
				if (pcb->iss == seq) {
					pcb->sndWnd = wnd;
					if (pcb->mss > wnd)
						pcb->mss = wnd;
					adhocPtpConnected(pcb);
					adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_ACK_SYN);
					g_adhocPtpRxSynAckOk++;
					break;
				}
				adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
						   (const SceNetEtherAddr *)(frame + 6), sport,
						   PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
				g_adhocPtpRxSynAckDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_ESTABLISHED:
				if (pcb->iss == seq) {
					adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_ACK_SYN);
					g_adhocPtpRxSynAckDup++;
					break;
				}
				adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
						   (const SceNetEtherAddr *)(frame + 6), sport,
						   PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
				g_adhocPtpRxSynAckDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_LISTEN:
			case SCE_NET_ADHOC_PTP_STATE_SYN_RCVD:
				adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
						   (const SceNetEtherAddr *)(frame + 6), sport,
						   PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
				g_adhocPtpRxSynAckDrop++;
				break;
			default:
				break;
			}
			adhocMbufFreeChain(m);
			return;
		case PSPNET_ADHOC_PTP_OP_ACK_SYN:
			switch (pcb->state) {
			case SCE_NET_ADHOC_PTP_STATE_CLOSED:
				g_adhocPtpRxAckSynDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_SYN_RCVD:
				if (pcb->iss == seq) {
					adhocPtpConnected(pcb);
					g_adhocPtpRxAckSynOk++;
					g_adhocPtpRxAckSynDup++;
				} else {
					adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
							   (const SceNetEtherAddr *)(frame + 6),
							   sport, PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
					g_adhocPtpRxAckSynDrop++;
				}
				break;
			case SCE_NET_ADHOC_PTP_STATE_ESTABLISHED:
				if (pcb->iss == seq) {
					g_adhocPtpRxAckSynDup++;
				} else {
					adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
							   (const SceNetEtherAddr *)(frame + 6),
							   sport, PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
					g_adhocPtpRxAckSynDrop++;
				}
				break;
			case SCE_NET_ADHOC_PTP_STATE_LISTEN:
			case SCE_NET_ADHOC_PTP_STATE_SYN_SENT:
				adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
						   (const SceNetEtherAddr *)(frame + 6), sport,
						   PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
				g_adhocPtpRxAckSynDrop++;
				break;
			default:
				break;
			}
			adhocMbufFreeChain(m);
			return;
		case PSPNET_ADHOC_PTP_OP_DATA:
			switch (pcb->state) {
			case SCE_NET_ADHOC_PTP_STATE_CLOSED:
				g_adhocPtpRxDataDrop++;
				adhocMbufFreeChain(m);
				return;
			case SCE_NET_ADHOC_PTP_STATE_LISTEN:
			case SCE_NET_ADHOC_PTP_STATE_SYN_SENT:
				adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
						   (const SceNetEtherAddr *)(frame + 6), sport,
						   PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
				g_adhocPtpRxDataDrop++;
				adhocMbufFreeChain(m);
				return;
			case SCE_NET_ADHOC_PTP_STATE_SYN_RCVD:
				if (pcb->rcvNxt == seq) {
					adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_SYN_ACK);
					pcb->rexmtRemain = pcb->rexmtCnt;
					adhocCalloutReset(&pcb->rexmtTimer, (int)pcb->rexmtInt,
							  adhocPtpRexmtTimeout, pcb);
				} else {
					g_adhocPtpRxDataDrop++;
				}
				adhocMbufFreeChain(m);
				return;
			case SCE_NET_ADHOC_PTP_STATE_ESTABLISHED:
				if (pcb->rcvNxt != seq) {
					if ((int)(pcb->rcvNxt - seq) > 0) {
						adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_ACK);
						g_adhocPtpRxDataDup++;
					} else {
						if ((pcb->flags & 1) == 0) {
							pcb->flags |= 1;
							adhocPtpSendControl(pcb,
									    PSPNET_ADHOC_PTP_OP_NACK);
						}
						g_adhocPtpRxDataDrop++;
					}
					adhocMbufFreeChain(m);
					return;
				}
				adhocMbufAdj(m, PSPNET_ADHOC_PKT_HDR_LEN);
				if ((int)(pcb->rcv.hiwat - pcb->rcv.cc) < m->storage.mh.pktHdr.len) {
					g_adhocPtpRxDataDropFull++;
					adhocMbufFreeChain(m);
					return;
				}
				pcb->flags &= ~1;
				m->nextPkt = NULL;
				if (pcb->rcv.tail != NULL)
					pcb->rcv.tail->nextPkt = m;
				else
					pcb->rcv.head = m;
				pcb->rcv.tail = m;
				pcb->rcv.cc += (unsigned int)m->storage.mh.pktHdr.len;
				pcb->rcvNxt++;
				adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_ACK);
				if (pcb->rcv.flags & PSPNET_ADHOC_SB_WAIT) {
					pcb->rcv.flags &= ~PSPNET_ADHOC_SB_WAIT;
					adhocWakeup(&pcb->rcv);
				}
				if (pcb->rcv.flags & PSPNET_ADHOC_SB_SEL) {
					pcb->rcv.flags &= ~PSPNET_ADHOC_SB_SEL;
					adhocWakeup(&g_adhocPollChannel);
				}
				g_adhocPtpRxDataOk++;
				return;
			default:
				adhocMbufFreeChain(m);
				return;
			}
		case PSPNET_ADHOC_PTP_OP_ACK:
			switch (pcb->state) {
			case SCE_NET_ADHOC_PTP_STATE_CLOSED:
				g_adhocPtpRxAckDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_LISTEN:
			case SCE_NET_ADHOC_PTP_STATE_SYN_SENT:
			case SCE_NET_ADHOC_PTP_STATE_SYN_RCVD:
				adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
						   (const SceNetEtherAddr *)(frame + 6), sport,
						   PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
				g_adhocPtpRxAckDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_ESTABLISHED:
				if ((int)(pcb->sndUna - seq) > 0) {
					pcb->sndWnd = wnd;
					if (pcb->snd.flags & PSPNET_ADHOC_SB_WAIT) {
						pcb->snd.flags &= ~PSPNET_ADHOC_SB_WAIT;
						adhocWakeup(&pcb->snd);
					}
					if (pcb->snd.flags & PSPNET_ADHOC_SB_SEL) {
						pcb->snd.flags &= ~PSPNET_ADHOC_SB_SEL;
						adhocWakeup(&g_adhocPollChannel);
					}
					adhocPtpOutput(pcb, 0);
					g_adhocPtpRxAckDup++;
				} else {
					int failed = 0;
					do {
						q = pcb->snd.head;
						if (q == NULL) {
							adhocPtpDisconnect(pcb);
							g_adhocPtpRxAckDrop++;
							failed = 1;
							break;
						}
						pcb->snd.head = q->nextPkt;
						if (pcb->snd.head == NULL)
							pcb->snd.tail = NULL;
						q->nextPkt = NULL;
						pcb->snd.cc -= (unsigned int)q->storage.mh.pktHdr.len;
						pcb->sndUna++;
						adhocMbufFreeChain(q);
					} while ((int)(pcb->sndUna - seq) <= 0);
					if (!failed) {
						if ((int)(pcb->sndNxt - pcb->sndUna) < 0)
							pcb->sndNxt = pcb->sndUna;
						if (pcb->sndUna == pcb->sndQueued)
							adhocCalloutStop(&pcb->rexmtTimer);
						pcb->sndWnd = wnd;
						if (pcb->snd.flags & PSPNET_ADHOC_SB_WAIT) {
							pcb->snd.flags &= ~PSPNET_ADHOC_SB_WAIT;
							adhocWakeup(&pcb->snd);
						}
						if (pcb->snd.flags & PSPNET_ADHOC_SB_SEL) {
							pcb->snd.flags &= ~PSPNET_ADHOC_SB_SEL;
							adhocWakeup(&g_adhocPollChannel);
						}
						adhocPtpOutput(pcb, 0);
						g_adhocPtpRxAckOk++;
					}
				}
				break;
			default:
				break;
			}
			adhocMbufFreeChain(m);
			return;
		case PSPNET_ADHOC_PTP_OP_NACK:
			switch (pcb->state) {
			case SCE_NET_ADHOC_PTP_STATE_CLOSED:
				g_adhocPtpRxNackDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_LISTEN:
			case SCE_NET_ADHOC_PTP_STATE_SYN_SENT:
			case SCE_NET_ADHOC_PTP_STATE_SYN_RCVD:
				adhocPtpSendPacket((const SceNetEtherAddr *)frame, dport,
						   (const SceNetEtherAddr *)(frame + 6), sport,
						   PSPNET_ADHOC_PTP_OP_RST, 0, NULL);
				g_adhocPtpRxNackDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_ESTABLISHED:
				if (pcb->sndUna == seq) {
					adhocCalloutStop(&pcb->rexmtTimer);
					pcb->sndNxt = pcb->sndUna;
					adhocPtpOutput(pcb, 0);
					g_adhocPtpRxNackOk++;
				} else {
					g_adhocPtpRxNackDrop++;
				}
				break;
			default:
				break;
			}
			adhocMbufFreeChain(m);
			return;
		case PSPNET_ADHOC_PTP_OP_RST:
			switch (pcb->state) {
			case SCE_NET_ADHOC_PTP_STATE_CLOSED:
			case SCE_NET_ADHOC_PTP_STATE_LISTEN:
				g_adhocPtpRxRstDrop++;
				break;
			case SCE_NET_ADHOC_PTP_STATE_SYN_SENT:
			case SCE_NET_ADHOC_PTP_STATE_SYN_RCVD:
			case SCE_NET_ADHOC_PTP_STATE_ESTABLISHED:
				g_adhocPtpRxRstOk++;
				adhocPtpDisconnect(pcb);
				break;
			default:
				break;
			}
			adhocMbufFreeChain(m);
			return;
		default:
			adhocMbufFreeChain(m);
			return;
		}
	}
}

__attribute__((noinline))
void adhocPtpSendPacket(const SceNetEtherAddr *saddr, SceUShort16 sport,
			const SceNetEtherAddr *daddr, SceUShort16 dport,
			int opcode, SceUInt32 seq, AdhocMbuf *payload)
{
	int dataLen = 0;
	int ptpLen, padLen, totalLen;
	AdhocMbuf *m;
	char *frame;
	SceUShort16 net16;
	SceUInt32 net32;

	if (payload != NULL)
		dataLen = payload->storage.mh.pktHdr.len;
	ptpLen = dataLen + 12;
	padLen = ptpLen % 16;
	if (padLen > 0)
		padLen = 16 - padLen;
	totalLen = dataLen + padLen + PSPNET_ADHOC_PKT_HDR_LEN;
	m = ptpAllocMbuf(totalLen);
	if (m == NULL)
		return;
	frame = m->data;
	sceClibMemcpy_safe(frame, daddr, 6);
	sceClibMemcpy_safe(frame + 6, saddr, 6);
	sceClibMemcpy_safe(frame + 12, g_adhocEtherType, 2);
	sceClibMemcpy_safe(frame + 14, g_adhocDataSubType, 2);
	frame = m->data;
	frame[48] = PSPNET_ADHOC_PROTO_PTP;
	frame[49] = (char)opcode;
	net16 = sceNetHtons((SceUShort16)ptpLen);
	sceClibMemcpy_safe(frame + 50, &net16, 2);
	net16 = sceNetHtons(sport);
	sceClibMemcpy_safe(frame + 52, &net16, 2);
	net16 = sceNetHtons(dport);
	sceClibMemcpy_safe(frame + 54, &net16, 2);
	net32 = sceNetHtonl(seq);
	sceClibMemcpy_safe(frame + 56, &net32, 4);
	if (dataLen > 0) {
		if (payload->len >= dataLen)
			sceClibMemcpy_safe(frame + PSPNET_ADHOC_PKT_HDR_LEN, payload->data,
					   (SceSize)dataLen);
		else
			adhocMbufCopyData(payload, 0, dataLen, frame + PSPNET_ADHOC_PKT_HDR_LEN);
	}
	if (padLen > 0)
		sceClibMemset(frame + PSPNET_ADHOC_PKT_HDR_LEN + dataLen, 0, (SceSize)padLen);
	m->len = totalLen;
	m->storage.mh.pktHdr.rcvif = NULL;
	m->storage.mh.pktHdr.len = totalLen;
	adhocOutput(NULL, m);
}

__attribute__((noinline))
void adhocPtpSendControl(AdhocPtpPcb *pcb, SceUChar8 opcode)
{
	SceUInt32 seq;
	AdhocMbuf *m = NULL;
	SceUInt32 netWnd;

	switch (opcode) {
	case PSPNET_ADHOC_PTP_OP_SYN:
	case PSPNET_ADHOC_PTP_OP_SYN_ACK:
	case PSPNET_ADHOC_PTP_OP_ACK_SYN:
	case PSPNET_ADHOC_PTP_OP_RST:
		seq = pcb->iss;
		break;
	case PSPNET_ADHOC_PTP_OP_ACK:
		seq = pcb->rcvNxt - 1;
		break;
	case PSPNET_ADHOC_PTP_OP_NACK:
		seq = pcb->rcvNxt;
		break;
	default:
		seq = 0;
		break;
	}
	if ((opcode <= PSPNET_ADHOC_PTP_OP_SYN_ACK && opcode != 0) ||
	    opcode == PSPNET_ADHOC_PTP_OP_ACK) {
		m = (AdhocMbuf *)adhocMalloc(PSPNET_ADHOC_MSIZE);
		if (m == NULL)
			return;
		m->type = PSPNET_ADHOC_MT_DATA;
		m->next = NULL;
		m->nextPkt = NULL;
		m->data = m->storage.mh.body.pktDat;
		m->flags = PSPNET_ADHOC_M_PKTHDR;
		m->storage.mh.pktHdr.aux = NULL;
		netWnd = sceNetHtonl(pcb->rcv.hiwat - pcb->rcv.cc);
		sceClibMemcpy_safe(m->data, &netWnd, 4);
		m->len = 4;
		m->storage.mh.pktHdr.rcvif = NULL;
		m->storage.mh.pktHdr.len = 4;
	}
	{
		SceUShort16 lport = pcb->lport;
		SceUShort16 pport = pcb->pport;

		switch (opcode) {
		case PSPNET_ADHOC_PTP_OP_SYN:
			g_adhocPtpTxSyn++;
			break;
		case PSPNET_ADHOC_PTP_OP_SYN_ACK:
			g_adhocPtpTxSynAck++;
			break;
		case PSPNET_ADHOC_PTP_OP_ACK_SYN:
			g_adhocPtpTxAckSyn++;
			break;
		case PSPNET_ADHOC_PTP_OP_ACK:
			g_adhocPtpTxAck++;
			break;
		case PSPNET_ADHOC_PTP_OP_NACK:
			g_adhocPtpTxNack++;
			break;
		case PSPNET_ADHOC_PTP_OP_RST:
			g_adhocPtpTxRst++;
			break;
		default:
			break;
		}
		adhocPtpSendPacket(&pcb->laddr, lport, &pcb->paddr, pport, opcode, seq, m);
	}
	if (m != NULL)
		adhocMbufFreeChain(m);
}

__attribute__((noinline))
void adhocPtpOutput(AdhocPtpPcb *pcb, int isRexmt)
{
	AdhocMbuf *m = pcb->snd.head;
	int inFlight = 0;
	SceUInt32 seq = pcb->sndUna;
	int sent = 0;

	while (m != NULL && seq != pcb->sndNxt) {
		inFlight += m->storage.mh.pktHdr.len;
		seq++;
		m = m->nextPkt;
	}
	if (m != NULL) {
		while ((int)pcb->sndWnd - inFlight >= m->storage.mh.pktHdr.len) {
			adhocPtpSendPacket(&pcb->laddr, pcb->lport, &pcb->paddr, pcb->pport,
					   PSPNET_ADHOC_PTP_OP_DATA, pcb->sndNxt, m);
			if ((m->storage.mh.pktHdr.sentTimeLo | m->storage.mh.pktHdr.sentTimeHi) == 0) {
				SceUInt64 now = sceKernelGetProcessTimeWide();
				m->storage.mh.pktHdr.sentTimeLo = (SceUInt32)now;
				m->storage.mh.pktHdr.sentTimeHi = (SceUInt32)(now >> 32);
			}
			pcb->sndNxt++;
			sent++;
			inFlight += m->storage.mh.pktHdr.len;
			g_adhocPtpTxData++;
			m = m->next;
			if (m == NULL)
				break;
		}
		if (m != NULL && sent == 0) {
			if (isRexmt != 0) {
				adhocPtpSendPacket(&pcb->laddr, pcb->lport, &pcb->paddr,
						   pcb->pport, PSPNET_ADHOC_PTP_OP_DATA,
						   pcb->sndUna - 1, NULL);
				g_adhocPtpWindowProbe++;
			} else if ((pcb->rexmtTimer.flags & PSPNET_ADHOC_CALLOUT_ACTIVE) == 0) {
				g_adhocPtpWindowWait++;
				pcb->rexmtRemain = pcb->rexmtCnt;
				adhocCalloutReset(&pcb->rexmtTimer, (int)pcb->rexmtInt,
						  adhocPtpRexmtTimeout, pcb);
			}
		}
	}
	if (isRexmt == 0 && sent > 0) {
		pcb->rexmtRemain = pcb->rexmtCnt;
		adhocCalloutReset(&pcb->rexmtTimer, (int)pcb->rexmtInt,
				  adhocPtpRexmtTimeout, pcb);
	}
}

__attribute__((noinline))
void adhocPtpRexmtTimeout(void *arg)
{
	AdhocPtpPcb *pcb = (AdhocPtpPcb *)arg;

	if ((unsigned int)pcb->state < SCE_NET_ADHOC_PTP_STATE_SYN_SENT)
		return;
	if (pcb->rexmtRemain-- <= 0) {
		adhocPtpDisconnect(pcb);
		return;
	}
	switch (pcb->state) {
	case SCE_NET_ADHOC_PTP_STATE_SYN_SENT:
		adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_SYN);
		break;
	case SCE_NET_ADHOC_PTP_STATE_SYN_RCVD:
		adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_SYN_ACK);
		break;
	case SCE_NET_ADHOC_PTP_STATE_ESTABLISHED:
		pcb->sndNxt = pcb->sndUna;
		adhocPtpOutput(pcb, 1);
		g_adhocPtpRexmtCount++;
		break;
	default:
		break;
	}
	adhocCalloutReset(&pcb->rexmtTimer, (int)pcb->rexmtInt, adhocPtpRexmtTimeout, pcb);
}

__attribute__((noinline))
void adhocPtpKeepaliveTimeout(void *arg)
{
	AdhocPtpPcb *pcb = (AdhocPtpPcb *)arg;
	AdhocMbuf *m;
	SceUInt64 now, limit;

	if (pcb->state != SCE_NET_ADHOC_PTP_STATE_ESTABLISHED)
		return;
	m = pcb->snd.head;
	if (m != NULL) {
		SceUInt64 sentTime;

		now = sceKernelGetProcessTimeWide();
		sentTime = ((SceUInt64)m->storage.mh.pktHdr.sentTimeHi << 32) |
			   m->storage.mh.pktHdr.sentTimeLo;
		if (sentTime != 0) {
			limit = (SceUInt64)pcb->rexmtInt * (SceInt64)(pcb->rexmtCnt + 1);
			if (now - sentTime > limit) {
				adhocPtpDisconnect(pcb);
				return;
			}
		}
	}
	adhocCalloutReset(&pcb->persistTimer, (int)pcb->rexmtInt, adhocPtpKeepaliveTimeout, pcb);
}

__attribute__((noinline))
int adhocPtpInit(void)
{
	g_adhocPtpList.first = NULL;
	g_adhocPtpList.last = &g_adhocPtpList.first;
	g_adhocPtpLastPort = (SceUShort16)(adhocRandNext() / 0x20009U + 0x4000U);
	g_adhocPtpIss = adhocRandNext();
	return 0;
}

static inline __attribute__((always_inline)) void ptpDetachInline(AdhocSocket *sock)
{
	AdhocPtpPcb *pcb = (AdhocPtpPcb *)sock->pcb;

	sock->pcb = NULL;
	if (pcb->next != NULL)
		pcb->next->prev = pcb->prev;
	else
		g_adhocPtpList.last = pcb->prev;
	*pcb->prev = pcb->next;
	adhocFree(pcb);
}

__attribute__((noinline))
void adhocPtpDetach(AdhocSocket *sock)
{
	ptpDetachInline(sock);
}

__attribute__((noinline))
void adhocPtpFlushBuffers(AdhocPtpPcb *pcb)
{
	AdhocMbuf *m;

	while ((m = pcb->snd.head) != NULL) {
		pcb->snd.head = m->nextPkt;
		if (pcb->snd.head == NULL)
			pcb->snd.tail = NULL;
		m->nextPkt = NULL;
		pcb->snd.cc -= (unsigned int)m->storage.mh.pktHdr.len;
		adhocMbufFreeChain(m);
	}
	pcb->snd.hiwat = 0;
	while ((m = pcb->rcv.head) != NULL) {
		pcb->rcv.head = m->nextPkt;
		if (pcb->rcv.head == NULL)
			pcb->rcv.tail = NULL;
		m->nextPkt = NULL;
		pcb->rcv.cc -= (unsigned int)m->storage.mh.pktHdr.len;
		adhocMbufFreeChain(m);
	}
	pcb->rcv.hiwat = 0;
}

__attribute__((noinline))
void adhocPtpTerm(void)
{
	AdhocPtpPcb *pcb;

	while ((pcb = g_adhocPtpList.first) != NULL) {
		adhocCalloutStop(&pcb->rexmtTimer);
		adhocPtpFlushBuffers(pcb);
		ptpDetachInline(pcb->sock);
	}
}

__attribute__((noinline))
AdhocPtpPcb *adhocPtpLookup(const SceNetEtherAddr *laddr, SceUShort16 lport,
			    const SceNetEtherAddr *paddr, SceUShort16 pport)
{
	AdhocPtpPcb *pcb;

	for (pcb = g_adhocPtpList.first; pcb != NULL; pcb = pcb->next) {
		if (sceClibMemcmp(&pcb->laddr, laddr, sizeof(*laddr)) == 0 &&
		    pcb->lport == lport &&
		    (paddr == NULL ||
		     (sceClibMemcmp(&pcb->paddr, paddr, sizeof(*paddr)) == 0 &&
		      pcb->pport == pport)))
			break;
	}
	return pcb;
}

static inline __attribute__((always_inline)) int ptpAttachInline(AdhocSocket *sock)
{
	AdhocPtpPcb *pcb;

	pcb = (AdhocPtpPcb *)adhocMalloc(sizeof(*pcb));
	if (pcb == NULL)
		return SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE;
	sceClibMemset(pcb, 0, sizeof(*pcb));
	pcb->qHead = NULL;
	pcb->qTail = &pcb->qHead;
	adhocCalloutInit(&pcb->rexmtTimer);
	adhocCalloutInit(&pcb->persistTimer);
	pcb->sock = sock;
	pcb->state = SCE_NET_ADHOC_PTP_STATE_CLOSED;
	sock->pcb = pcb;
	pcb->next = NULL;
	pcb->prev = g_adhocPtpList.last;
	*g_adhocPtpList.last = pcb;
	g_adhocPtpList.last = &pcb->next;
	return 0;
}

__attribute__((noinline))
int adhocPtpAttach(AdhocSocket *sock)
{
	return ptpAttachInline(sock);
}

__attribute__((noinline))
int adhocPtpBind(AdhocPtpPcb *pcb, const SceNetEtherAddr *saddr, SceUShort16 sport,
		 const SceNetEtherAddr *daddr, SceUShort16 dport)
{
	SceUShort16 port = sport;
	AdhocPtpPcb *cur;

	if (port == 0) {
		port = (g_adhocPtpLastPort == 0x7fff) ? 0x4000 : (SceUShort16)(g_adhocPtpLastPort + 1);
		if (port == g_adhocPtpLastPort) {
			port = 0;
		} else {
			for (;;) {
				for (cur = g_adhocPtpList.first; cur != NULL; cur = cur->next) {
					if (sceClibMemcmp(&cur->laddr, saddr, sizeof(*saddr)) == 0 &&
					    cur->lport == port)
						break;
				}
				if (cur == NULL) {
					g_adhocPtpLastPort = port;
					break;
				}
				port = (port == 0x7fff) ? 0x4000 : (SceUShort16)(port + 1);
				if (port == g_adhocPtpLastPort) {
					port = 0;
					break;
				}
			}
		}
		if (port == 0)
			return SCE_ERROR_NET_ADHOC_PORT_NOT_AVAIL;
	} else if (port == 0xffff) {
		return SCE_ERROR_NET_ADHOC_INVALID_PORT;
	}
	for (cur = g_adhocPtpList.first; cur != NULL; cur = cur->next) {
		if (sceClibMemcmp(&cur->laddr, saddr, sizeof(*saddr)) == 0 &&
		    cur->lport == port &&
		    (daddr == NULL ||
		     (sceClibMemcmp(&cur->paddr, daddr, sizeof(*daddr)) == 0 &&
		      cur->pport == dport)))
			return SCE_ERROR_NET_ADHOC_PORT_IN_USE;
	}
	sceClibMemcpy_safe(&pcb->laddr, saddr, sizeof(pcb->laddr));
	pcb->lport = port;
	pcb->mss = SCE_NET_ADHOC_PTP_MSS;
	pcb->sock->sendWchan = &pcb->snd;
	pcb->sock->recvWchan = &pcb->rcv;
	pcb->sock->pollWchan = &g_adhocPollChannel;
	return 0;
}

__attribute__((noinline))
int adhocPtpSetBufSize(AdhocPtpPcb *pcb, unsigned int sndSize, unsigned int rcvSize)
{
	pcb->snd.hiwat = sndSize;
	pcb->snd.lowat = 1;
	pcb->rcv.lowat = 1;
	pcb->rcv.hiwat = rcvSize;
	return 0;
}

__attribute__((noinline))
void adhocPtpSetRexmt(AdhocPtpPcb *pcb, unsigned int rexmtInt, int rexmtCnt)
{
	pcb->rexmtInt = rexmtInt;
	pcb->rexmtCnt = rexmtCnt;
}

__attribute__((noinline))
int adhocPtpConnectStart(AdhocPtpPcb *pcb, const SceNetEtherAddr *daddr, SceUShort16 dport)
{
	if (sceClibMemcmp(daddr, &g_adhocZeroAddr, sizeof(g_adhocZeroAddr)) == 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ADDR;
	if ((daddr->data[0] & 1) &&
	    sceClibMemcmp(daddr, g_adhocBroadcastAddr, sizeof(g_adhocBroadcastAddr)) != 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ADDR;
	if (dport == 0 || dport == 0xffff)
		return SCE_ERROR_NET_ADHOC_INVALID_PORT;
	sceClibMemcpy_safe(&pcb->paddr, daddr, sizeof(pcb->paddr));
	pcb->pport = dport;
	pcb->state = SCE_NET_ADHOC_PTP_STATE_SYN_SENT;
	pcb->iss = g_adhocPtpIss++;
	pcb->sock->flags |= PSPNET_ADHOC_SF_CONNECTING;
	adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_SYN);
	pcb->rexmtRemain = pcb->rexmtCnt;
	adhocCalloutReset(&pcb->rexmtTimer, (int)pcb->rexmtInt, adhocPtpRexmtTimeout, pcb);
	return 0;
}

__attribute__((noinline))
int adhocPtpListenStart(AdhocPtpPcb *pcb, int backlog)
{
	pcb->state = SCE_NET_ADHOC_PTP_STATE_LISTEN;
	pcb->qLimit = backlog;
	pcb->sock->flags |= PSPNET_ADHOC_SF_LISTEN;
	return 0;
}

__attribute__((noinline))
AdhocPtpPcb *adhocPtpAcceptChild(AdhocPtpPcb *listenPcb, const SceNetEtherAddr *paddr,
				 SceUShort16 pport, SceUInt32 seq)
{
	AdhocSocket *sock;
	AdhocPtpPcb *child;

	if (sceClibMemcmp(paddr, &g_adhocZeroAddr, sizeof(g_adhocZeroAddr)) == 0)
		return NULL;
	if ((paddr->data[0] & 1) &&
	    sceClibMemcmp(paddr, g_adhocBroadcastAddr, sizeof(g_adhocBroadcastAddr)) != 0)
		return NULL;
	if (pport == 0 || pport == 0xffff)
		return NULL;
	if (listenPcb->qLen >= listenPcb->qLimit)
		return NULL;
	if (adhocSocketCreate(PSPNET_ADHOC_SOCK_PTP, &sock) < 0)
		return NULL;
	if (ptpAttachInline(sock) < 0) {
		adhocSocketFree(sock);
		return NULL;
	}
	child = (AdhocPtpPcb *)sock->pcb;
	sceClibMemcpy_safe(&child->laddr, &listenPcb->laddr, sizeof(child->laddr));
	sceClibMemcpy_safe(&child->paddr, paddr, sizeof(child->paddr));
	child->lport = listenPcb->lport;
	child->pport = pport;
	child->snd.lowat = listenPcb->snd.lowat;
	child->snd.hiwat = listenPcb->snd.hiwat;
	child->rcv.lowat = listenPcb->rcv.lowat;
	child->rcv.hiwat = listenPcb->rcv.hiwat;
	child->state = SCE_NET_ADHOC_PTP_STATE_SYN_RCVD;
	child->mss = listenPcb->mss;
	child->iss = seq;
	child->rexmtInt = listenPcb->rexmtInt;
	child->rexmtCnt = listenPcb->rexmtCnt;
	sock->sendWchan = &child->snd;
	sock->recvWchan = &child->rcv;
	sock->pollWchan = &g_adhocPollChannel;
	sock->flags |= PSPNET_ADHOC_SF_CONNECTING;
	child->qNext = NULL;
	child->qPrev = listenPcb->qTail;
	*listenPcb->qTail = child;
	listenPcb->qTail = &child->qNext;
	listenPcb->qLen++;
	child->parent = listenPcb;
	return child;
}

__attribute__((noinline))
void adhocPtpConnected(AdhocPtpPcb *pcb)
{
	adhocCalloutStop(&pcb->rexmtTimer);
	pcb->state = SCE_NET_ADHOC_PTP_STATE_ESTABLISHED;
	pcb->sock->flags = (pcb->sock->flags & ~PSPNET_ADHOC_SF_CONNECTING) |
			   PSPNET_ADHOC_SF_CONNECTED;
	if (pcb->parent != NULL) {
		if (pcb->parent->rcv.flags & PSPNET_ADHOC_SB_WAIT) {
			pcb->parent->rcv.flags &= ~PSPNET_ADHOC_SB_WAIT;
			adhocWakeup(&pcb->parent->rcv);
		}
		if (pcb->parent->rcv.flags & PSPNET_ADHOC_SB_SEL) {
			pcb->parent->rcv.flags &= ~PSPNET_ADHOC_SB_SEL;
			adhocWakeup(&g_adhocPollChannel);
		}
	} else {
		if (pcb->rcv.flags & PSPNET_ADHOC_SB_WAIT) {
			pcb->rcv.flags &= ~PSPNET_ADHOC_SB_WAIT;
			adhocWakeup(&pcb->rcv);
		}
		if (pcb->rcv.flags & PSPNET_ADHOC_SB_SEL) {
			pcb->rcv.flags &= ~PSPNET_ADHOC_SB_SEL;
			adhocWakeup(&g_adhocPollChannel);
		}
	}
	adhocCalloutReset(&pcb->persistTimer, (int)pcb->rexmtInt, adhocPtpKeepaliveTimeout, pcb);
}

__attribute__((noinline))
void adhocPtpDisconnect(AdhocPtpPcb *pcb)
{
	adhocCalloutStop(&pcb->persistTimer);
	adhocCalloutStop(&pcb->rexmtTimer);
	if (pcb->sock->flags & (PSPNET_ADHOC_SF_CONNECTING | PSPNET_ADHOC_SF_CONNECTED))
		adhocPtpSendControl(pcb, PSPNET_ADHOC_PTP_OP_RST);
	pcb->sock->flags = (pcb->sock->flags & ~(PSPNET_ADHOC_SF_LISTEN |
						 PSPNET_ADHOC_SF_CONNECTING)) |
			   PSPNET_ADHOC_SF_DISCONNECTED;
	pcb->state = SCE_NET_ADHOC_PTP_STATE_CLOSED;
	if (pcb->parent != NULL) {
		if (pcb->parent->rcv.flags & PSPNET_ADHOC_SB_WAIT) {
			pcb->parent->rcv.flags &= ~PSPNET_ADHOC_SB_WAIT;
			adhocWakeup(&pcb->parent->rcv);
		}
		if (pcb->parent->rcv.flags & PSPNET_ADHOC_SB_SEL) {
			pcb->parent->rcv.flags &= ~PSPNET_ADHOC_SB_SEL;
			adhocWakeup(&g_adhocPollChannel);
		}
		if (pcb->parent->snd.flags & PSPNET_ADHOC_SB_WAIT) {
			pcb->parent->snd.flags &= ~PSPNET_ADHOC_SB_WAIT;
			adhocWakeup(&pcb->parent->snd);
		}
		if (pcb->parent->snd.flags & PSPNET_ADHOC_SB_SEL) {
			pcb->parent->snd.flags &= ~PSPNET_ADHOC_SB_SEL;
			adhocWakeup(&g_adhocPollChannel);
		}
	} else {
		if (pcb->rcv.flags & PSPNET_ADHOC_SB_WAIT) {
			pcb->rcv.flags &= ~PSPNET_ADHOC_SB_WAIT;
			adhocWakeup(&pcb->rcv);
		}
		if (pcb->rcv.flags & PSPNET_ADHOC_SB_SEL) {
			pcb->rcv.flags &= ~PSPNET_ADHOC_SB_SEL;
			adhocWakeup(&g_adhocPollChannel);
		}
		if (pcb->snd.flags & PSPNET_ADHOC_SB_WAIT) {
			pcb->snd.flags &= ~PSPNET_ADHOC_SB_WAIT;
			adhocWakeup(&pcb->snd);
		}
		if (pcb->snd.flags & PSPNET_ADHOC_SB_SEL) {
			pcb->snd.flags &= ~PSPNET_ADHOC_SB_SEL;
			adhocWakeup(&g_adhocPollChannel);
		}
	}
}

__attribute__((noinline))
void adhocPtpCloseChildren(AdhocPtpPcb *pcb)
{
	AdhocPtpPcb *child;
	AdhocSocket *childSock;

	while ((child = pcb->qHead) != NULL) {
		childSock = child->sock;
		if (child->qNext != NULL)
			child->qNext->qPrev = child->qPrev;
		else
			pcb->qTail = child->qPrev;
		*child->qPrev = child->qNext;
		pcb->qLen--;
		adhocPtpDisconnect(child);
		adhocPtpCloseChildren(child);
		ptpDetachInline(childSock);
		adhocSocketFree(childSock);
	}
	sceClibMemset(&pcb->laddr, 0, sizeof(pcb->laddr));
	sceClibMemset(&pcb->paddr, 0, sizeof(pcb->paddr));
	pcb->lport = 0;
	adhocPtpFlushBuffers(pcb);
}

__attribute__((noinline))
int adhocPtpPoll(AdhocSocket *sock, int events)
{
	AdhocPtpPcb *pcb = (AdhocPtpPcb *)sock->pcb;
	AdhocPtpPcb *child;
	int revents = 0;

	if ((events & SCE_NET_ADHOC_EV_RECV) && pcb->rcv.cc >= pcb->rcv.lowat)
		revents |= SCE_NET_ADHOC_EV_RECV;
	if ((events & SCE_NET_ADHOC_EV_SEND) && (pcb->snd.hiwat - pcb->snd.cc) >= pcb->snd.lowat)
		revents |= SCE_NET_ADHOC_EV_SEND;
	if ((events & SCE_NET_ADHOC_EV_CONNECT) && (sock->flags & PSPNET_ADHOC_SF_CONNECTED))
		revents |= SCE_NET_ADHOC_EV_CONNECT;
	if ((events & SCE_NET_ADHOC_EV_ACCEPT) && (sock->flags & PSPNET_ADHOC_SF_LISTEN)) {
		for (child = pcb->qHead; child != NULL; child = child->qNext) {
			if (child->sock->flags & PSPNET_ADHOC_SF_CONNECTED) {
				revents |= SCE_NET_ADHOC_EV_ACCEPT;
				break;
			}
		}
	}
	if ((events & SCE_NET_ADHOC_EV_FLUSH) && pcb->snd.cc == 0)
		revents |= SCE_NET_ADHOC_EV_FLUSH;
	if (revents == 0) {
		if (events & SCE_NET_ADHOC_EV_RECV)
			pcb->rcv.flags |= PSPNET_ADHOC_SB_SEL;
		if (events & SCE_NET_ADHOC_EV_SEND)
			pcb->snd.flags |= PSPNET_ADHOC_SB_SEL;
		if (events & SCE_NET_ADHOC_EV_CONNECT)
			pcb->rcv.flags |= PSPNET_ADHOC_SB_SEL;
		if (events & SCE_NET_ADHOC_EV_ACCEPT)
			pcb->rcv.flags |= PSPNET_ADHOC_SB_SEL;
		if (events & SCE_NET_ADHOC_EV_FLUSH)
			pcb->snd.flags |= PSPNET_ADHOC_SB_SEL;
	}
	return revents;
}

__attribute__((noinline))
int sceNetAdhocGetPtpStat(int *buflen, void *buf)
{
	int s, total = 0, ret = 0;
	AdhocPtpPcb *pcb;
	struct SceNetAdhocPtpStat *cur = (struct SceNetAdhocPtpStat *)buf;
	struct SceNetAdhocPtpStat *prev = NULL;

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
		for (pcb = g_adhocPtpList.first; pcb != NULL; pcb = pcb->next) {
			if (buf != NULL) {
				if (total + (int)sizeof(*cur) > *buflen)
					break;
				cur->next = NULL;
				cur->id = pcb->sock->id;
				sceClibMemcpy_safe(&cur->laddr, &pcb->laddr, sizeof(cur->laddr));
				sceClibMemcpy_safe(&cur->paddr, &pcb->paddr, sizeof(cur->paddr));
				cur->lport = pcb->lport;
				cur->pport = pcb->pport;
				cur->snd_sb_cc = pcb->snd.cc;
				cur->rcv_sb_cc = pcb->rcv.cc;
				cur->state = pcb->state;
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
