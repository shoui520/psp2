/*
 * adhoc.c: SceNetAdhoc core lifecycle, Ethernet packet queue/dispatch, socket table,
 * polling, and alert APIs for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

const SceUChar8 g_adhocEtherType[2] = { 0x88, 0xc8 };
const SceUChar8 g_adhocDataSubType[2] = { 0x00, 0x02 };
const SceUChar8 g_adhocBroadcastAddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

int g_adhocRawSocket = 0;

int g_adhocInitialized;
SceNetEtherAddr g_adhocZeroAddr;
int g_adhocSendPollFlags;
SceNetEtherAddr g_adhocLocalEtherAddr;
AdhocInputQueue g_adhocInputQueue;
int g_adhocLastSocketId;
AdhocSocket *g_adhocSocketHead;
AdhocSocket **g_adhocSocketTail;
int g_adhocPollChannel;

__attribute__((noinline))
int sceNetAdhocInit(void)
{
	int s, ret;

	adhocRandSeed(sceKernelGetProcessTimeLow());
	ret = adhocSplInit();
	if (ret < 0)
		return ret;
	s = adhocSplNet();
	if (s < 0)
		return s;
	if (g_adhocInitialized > 0) {
		adhocSplX(s);
		return SCE_ERROR_NET_ADHOC_ALREADY_INITIALIZED;
	}
	sceClibMemset(&g_adhocInputQueue, 0, sizeof(g_adhocInputQueue));
	g_adhocInputQueue.maxLen = 50;
	ret = adhocSocketTableInit();
	if (ret >= 0) {
		ret = adhocPdpInit();
		if (ret >= 0) {
			ret = adhocPtpInit();
			if (ret >= 0) {
				ret = adhocThreadInit();
				if (ret >= 0) {
					ret = adhocSlpqueInit();
					if (ret >= 0) {
						ret = sceNetSocket("SceNetPspAdhoc",
								   SCE_NET_AF_INET,
								   PSPNET_ADHOC_SOCK_RAW_ETHER,
								   PSPNET_ADHOC_ETHER_PROTO);
						if (ret >= 0) {
							g_adhocRawSocket = ret;
							g_adhocInitialized = 1;
							adhocSplX(s);
							return 0;
						}
						adhocSlpqueTerm();
					}
					adhocThreadTerm();
				}
				adhocPtpTerm();
			}
			adhocPdpTerm();
		}
		adhocSocketTableTerm();
	}
	adhocSplX(s);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocTerm(void)
{
	int s, ret = 0;
	AdhocMbuf *m;

	s = adhocSplNet();
	if (s < 0)
		return s;
	if (g_adhocInitialized != 0) {
		if (adhocSocketAnyBusy() != 0) {
			ret = SCE_ERROR_NET_ADHOC_BUSY;
		} else {
			sceNetSocketClose(g_adhocRawSocket);
			adhocSlpqueTerm();
			adhocThreadTerm();
			adhocPtpTerm();
			adhocPdpTerm();
			adhocSocketTableTerm();
			while ((m = g_adhocInputQueue.head) != NULL) {
				g_adhocInputQueue.head = m->nextPkt;
				if (g_adhocInputQueue.head == NULL)
					g_adhocInputQueue.tail = NULL;
				m->nextPkt = NULL;
				g_adhocInputQueue.len--;
				adhocMbufFreeChain(m);
			}
			g_adhocInitialized = 0;
		}
	}
	adhocSplX(s);
	adhocSplTerm();
	return ret;
}

__attribute__((noinline))
int adhocEnqueueInput(void *ifp, AdhocMbuf *m)
{
	int s;
	AdhocMbuf *old;

	(void)ifp;
	s = adhocSplNet();
	if (g_adhocInputQueue.len < g_adhocInputQueue.maxLen) {
		m->nextPkt = NULL;
		if (g_adhocInputQueue.tail != NULL)
			g_adhocInputQueue.tail->nextPkt = m;
		else
			g_adhocInputQueue.head = m;
		g_adhocInputQueue.tail = m;
		g_adhocInputQueue.len++;
	} else {
		old = g_adhocInputQueue.head;
		if (old != NULL) {
			g_adhocInputQueue.head = old->nextPkt;
			if (g_adhocInputQueue.head == NULL)
				g_adhocInputQueue.tail = NULL;
			old->nextPkt = NULL;
			g_adhocInputQueue.len--;
		}
		adhocMbufFreeChain(old);
	}
	adhocSplX(s);
	return 0;
}

__attribute__((noinline))
void adhocProcessInput(void)
{
	AdhocMbuf *m;
	SceUChar8 proto;

	for (;;) {
		m = g_adhocInputQueue.head;
		if (m == NULL)
			break;
		g_adhocInputQueue.head = m->nextPkt;
		if (g_adhocInputQueue.head == NULL)
			g_adhocInputQueue.tail = NULL;
		m->nextPkt = NULL;
		g_adhocInputQueue.len--;
		if (m->storage.mh.pktHdr.len < PSPNET_ADHOC_FRAME_HDR_LEN + 1) {
			adhocMbufFreeChain(m);
			continue;
		}
		if (m->len < PSPNET_ADHOC_FRAME_HDR_LEN + 1)
			adhocMbufCopyData(m, PSPNET_ADHOC_FRAME_HDR_LEN, 1, &proto);
		else
			proto = ((const SceUChar8 *)m->data)[PSPNET_ADHOC_FRAME_HDR_LEN];
		switch (proto) {
		case PSPNET_ADHOC_PROTO_PDP:
			adhocPdpInput(m);
			break;
		case PSPNET_ADHOC_PROTO_PTP:
			adhocPtpInput(m);
			break;
		default:
			adhocMbufFreeChain(m);
			break;
		}
	}
}

__attribute__((noinline))
void adhocOutput(void *ifp, AdhocMbuf *m)
{
	(void)ifp;
	if (sceClibMemcmp(m->data, &g_adhocLocalEtherAddr, sizeof(g_adhocLocalEtherAddr)) == 0) {
		if (g_adhocInputQueue.len >= g_adhocInputQueue.maxLen) {
			g_adhocInputQueue.drops++;
			adhocMbufFreeChain(m);
			return;
		}
	} else {
		if (adhocAuthEncryptFrame(m) < 0) {
			adhocMbufFreeChain(m);
			return;
		}
	}
	m->flags |= (SceShort16)PSPNET_ADHOC_M_SENT;
	sceNetSend(g_adhocRawSocket, m->data, (SceSize)m->storage.mh.pktHdr.len, 0);
	adhocMbufFreeChain(m);
}

__attribute__((noinline))
int adhocSocketTableInit(void)
{
	int ret;

	g_adhocSocketHead = NULL;
	g_adhocSocketTail = &g_adhocSocketHead;
	ret = adhocSetAlertCallback(adhocSocketAlertWakeup);
	if (ret < 0)
		return ret;
	g_adhocLastSocketId = 0;
	return 0;
}

__attribute__((noinline))
void adhocSocketFree(AdhocSocket *sock)
{
	if (sock->pcb != NULL)
		return;
	if (sock->next != NULL)
		sock->next->prev = sock->prev;
	else
		g_adhocSocketTail = sock->prev;
	*sock->prev = sock->next;
	adhocFree(sock);
}

__attribute__((noinline))
void adhocSocketTableTerm(void)
{
	AdhocSocket *sock;

	adhocClearAlertCallback();
	for (sock = g_adhocSocketHead; sock != NULL; sock = g_adhocSocketHead) {
		if (sock->pcb == NULL) {
			if (sock->next != NULL)
				sock->next->prev = sock->prev;
			else
				g_adhocSocketTail = sock->prev;
			*sock->prev = sock->next;
			adhocFree(sock);
		}
	}
}

AdhocSocket *adhocSocketFind(SceUChar8 type, int id)
{
	AdhocSocket *sock;

	for (sock = g_adhocSocketHead; sock != NULL; sock = sock->next) {
		if ((type == 0 || sock->type == type) && sock->id == id)
			break;
	}
	return sock;
}

__attribute__((noinline))
int adhocSocketAllocId(void)
{
	int id;
	AdhocSocket *sock;

	id = (g_adhocLastSocketId == 255) ? 1 : (g_adhocLastSocketId + 1);
	if (id == g_adhocLastSocketId)
		return -1;
	for (;;) {
		for (sock = g_adhocSocketHead; sock != NULL; sock = sock->next) {
			if (sock->id == id)
				break;
		}
		if (sock == NULL)
			break;
		id = (id == 255) ? 1 : (id + 1);
		if (id == g_adhocLastSocketId)
			return -1;
	}
	g_adhocLastSocketId = id;
	return id;
}

__attribute__((noinline))
int adhocSocketCreate(SceUChar8 type, AdhocSocket **outSock)
{
	AdhocSocket *sock;

	sock = (AdhocSocket *)adhocMalloc(sizeof(*sock));
	if (sock == NULL)
		return SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE;
	sceClibMemset(sock, 0, sizeof(*sock));
	sock->type = type;
	sock->id = -1;
	sock->next = NULL;
	sock->prev = g_adhocSocketTail;
	*g_adhocSocketTail = sock;
	g_adhocSocketTail = &sock->next;
	*outSock = sock;
	return 0;
}

__attribute__((noinline))
int sceNetAdhocPollSocket(struct SceNetAdhocPollSd *sds, int nsds,
			  unsigned int timeout, int flag)
{
	int ret;
	AdhocThreadInfo *ti;
	int s, i, ready;
	AdhocPollEntry *entries;
	SceUInt64 startTime, elapsed;
	int waitTime;
	AdhocThreadInfo *prev;

	if (sds == NULL || nsds <= 0 || (flag & ~SCE_NET_ADHOC_F_NONBLOCK) != 0)
		return SCE_ERROR_NET_ADHOC_INVALID_ARG;
	ti = adhocThreadEnter(&ret);
	if (ti == NULL)
		return ret;
	startTime = 0;
	ret = 0;
	s = adhocSplNet();
	if (s < 0) {
		ret = s;
	} else if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		entries = (AdhocPollEntry *)adhocMalloc((SceSize)nsds * sizeof(*entries));
		if (entries == NULL) {
			ret = SCE_ERROR_NET_ADHOC_NOT_ENOUGH_SPACE;
		} else {
			for (i = 0; i < nsds; i++) {
				AdhocSocket *sock = adhocSocketFind(0, sds[i].id);
				if (sock != NULL) {
					entries[i].sock = sock;
					entries[i].sd = &sds[i];
				} else {
					sds->revents = SCE_NET_ADHOC_EV_INVALID;
					ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
				}
			}
			if (ret >= 0) {
				for (i = 0; i < nsds; i++) {
					if (entries[i].sock->flags & SCE_NET_ADHOC_F_ALERTPOLL) {
						entries[i].sd->revents = SCE_NET_ADHOC_EV_ALERT;
						ret = SCE_ERROR_NET_ADHOC_EXCEPTION_EVENT;
					}
					if (entries[i].sock->flags & PSPNET_ADHOC_SF_DISCONNECTED) {
						entries[i].sd->revents = SCE_NET_ADHOC_EV_DISCONNECT;
						ret = SCE_ERROR_NET_ADHOC_EXCEPTION_EVENT;
					}
				}
			}
			while (ret >= 0) {
				ready = 0;
				for (i = 0; i < nsds; i++) {
					if (entries[i].sock->type == PSPNET_ADHOC_SOCK_PDP) {
						entries[i].sd->revents = adhocPdpPoll(
							entries[i].sock, entries[i].sd->events);
					} else if (entries[i].sock->type == PSPNET_ADHOC_SOCK_PTP) {
						entries[i].sd->revents = adhocPtpPoll(
							entries[i].sock, entries[i].sd->events);
					}
					if (entries[i].sd->revents != 0)
						ready++;
				}
				ret = ready;
				if (ready != 0 || (flag & SCE_NET_ADHOC_F_NONBLOCK) != 0)
					break;
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
				for (i = 0; i < nsds; i++) {
					ti->sockNext = entries[i].sock->waiters;
					entries[i].sock->waiters = ti;
				}
				ret = adhocSleep(&g_adhocPollChannel, waitTime);
				for (i = 0; i < nsds; i++) {
					AdhocSocket *sock = entries[i].sock;
					if (sock->waiters == ti) {
						sock->waiters = ti->sockNext;
					} else {
						prev = sock->waiters;
						while (prev->sockNext != ti)
							prev = prev->sockNext;
						prev->sockNext = ti->sockNext;
					}
					if (sock->flags & SCE_NET_ADHOC_F_ALERTPOLL) {
						entries[i].sd->revents = SCE_NET_ADHOC_EV_ALERT;
						ret = PSPNET_ADHOC_EIO;
					}
					if (sock->flags & PSPNET_ADHOC_SF_DISCONNECTED) {
						entries[i].sd->revents = SCE_NET_ADHOC_EV_DISCONNECT;
						ret = PSPNET_ADHOC_EIO;
					}
					if (sock->flags & PSPNET_ADHOC_SF_DELETED) {
						entries[i].sd->revents = SCE_NET_ADHOC_EV_DELETE;
						ret = PSPNET_ADHOC_EIO;
						if (sock->waiters == NULL) {
							if (sock->type == PSPNET_ADHOC_SOCK_PDP)
								adhocPdpDetach(sock);
							else if (sock->type == PSPNET_ADHOC_SOCK_PTP)
								adhocPtpDetach(sock);
							adhocSocketFree(sock);
						}
					}
				}
				if (ret < 0)
					break;
			}
			adhocFree(entries);
		}
	}
	adhocSplX(s);
	adhocThreadLeave(ti, 0);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocSetSocketAlert(int id, int flag)
{
	int s, ret = 0, changed = 0;
	AdhocSocket *sock;

	s = adhocSplNet();
	if (s < 0)
		return s;
	if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		sock = adhocSocketFind(0, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else {
			if (flag & SCE_NET_ADHOC_F_ALERTSEND) {
				if ((sock->flags & SCE_NET_ADHOC_F_ALERTSEND) == 0) {
					sock->flags |= SCE_NET_ADHOC_F_ALERTSEND;
					changed++;
				}
			} else {
				sock->flags &= ~SCE_NET_ADHOC_F_ALERTSEND;
			}
			if (flag & SCE_NET_ADHOC_F_ALERTRECV) {
				if ((sock->flags & SCE_NET_ADHOC_F_ALERTRECV) == 0) {
					sock->flags |= SCE_NET_ADHOC_F_ALERTRECV;
					changed++;
				}
			} else {
				sock->flags &= ~SCE_NET_ADHOC_F_ALERTRECV;
			}
			if (flag & SCE_NET_ADHOC_F_ALERTPOLL) {
				if ((sock->flags & SCE_NET_ADHOC_F_ALERTPOLL) == 0) {
					sock->flags |= SCE_NET_ADHOC_F_ALERTPOLL;
					changed++;
				}
			} else {
				sock->flags &= ~SCE_NET_ADHOC_F_ALERTPOLL;
			}
			if (flag & SCE_NET_ADHOC_F_ALERTCONNECT) {
				if ((sock->flags & SCE_NET_ADHOC_F_ALERTCONNECT) == 0) {
					sock->flags |= SCE_NET_ADHOC_F_ALERTCONNECT;
					changed++;
				}
			} else {
				sock->flags &= ~SCE_NET_ADHOC_F_ALERTCONNECT;
			}
			if (flag & SCE_NET_ADHOC_F_ALERTACCEPT) {
				if ((sock->flags & SCE_NET_ADHOC_F_ALERTACCEPT) == 0) {
					sock->flags |= SCE_NET_ADHOC_F_ALERTACCEPT;
					changed++;
				}
			} else {
				sock->flags &= ~SCE_NET_ADHOC_F_ALERTACCEPT;
			}
			if (flag & SCE_NET_ADHOC_F_ALERTFLUSH) {
				if ((sock->flags & SCE_NET_ADHOC_F_ALERTFLUSH) == 0) {
					sock->flags |= SCE_NET_ADHOC_F_ALERTFLUSH;
					changed++;
				}
			} else {
				sock->flags &= ~SCE_NET_ADHOC_F_ALERTFLUSH;
			}
			if (changed != 0)
				adhocNotifyAlert(sock, flag);
		}
	}
	adhocSplX(s);
	return ret;
}

__attribute__((noinline))
int sceNetAdhocGetSocketAlert(int id, int *flag)
{
	int s, ret = 0;
	AdhocSocket *sock;

	if (flag == NULL)
		return SCE_ERROR_NET_ADHOC_INVALID_ARG;
	s = adhocSplNet();
	if (s < 0)
		return s;
	if (g_adhocInitialized == 0) {
		ret = SCE_ERROR_NET_ADHOC_NOT_INITIALIZED;
	} else {
		sock = adhocSocketFind(0, id);
		if (sock == NULL) {
			ret = SCE_ERROR_NET_ADHOC_INVALID_SOCKET_ID;
		} else {
			*flag = 0;
			if (sock->flags & SCE_NET_ADHOC_F_ALERTSEND)
				*flag |= SCE_NET_ADHOC_F_ALERTSEND;
			if (sock->flags & SCE_NET_ADHOC_F_ALERTRECV)
				*flag |= SCE_NET_ADHOC_F_ALERTRECV;
			if (sock->flags & SCE_NET_ADHOC_F_ALERTPOLL)
				*flag |= SCE_NET_ADHOC_F_ALERTPOLL;
			if (sock->flags & SCE_NET_ADHOC_F_ALERTCONNECT)
				*flag |= SCE_NET_ADHOC_F_ALERTCONNECT;
			if (sock->flags & SCE_NET_ADHOC_F_ALERTACCEPT)
				*flag |= SCE_NET_ADHOC_F_ALERTACCEPT;
			if (sock->flags & SCE_NET_ADHOC_F_ALERTFLUSH)
				*flag |= SCE_NET_ADHOC_F_ALERTFLUSH;
		}
	}
	adhocSplX(s);
	return ret;
}

__attribute__((noinline))
int adhocSocketAnyBusy(void)
{
	AdhocSocket *sock;

	for (sock = g_adhocSocketHead; sock != NULL; sock = sock->next) {
		if (sock->waiters != NULL)
			return 1;
	}
	return 0;
}

__attribute__((noinline))
void adhocSocketAlertWakeup(AdhocSocket *sock, int alertFlags)
{
	AdhocThreadInfo *ti;
	int wakeSend = alertFlags & (SCE_NET_ADHOC_F_ALERTSEND | SCE_NET_ADHOC_F_ALERTFLUSH);
	int wakeRecv = alertFlags & (SCE_NET_ADHOC_F_ALERTRECV | SCE_NET_ADHOC_F_ALERTCONNECT |
				     SCE_NET_ADHOC_F_ALERTACCEPT);
	int wakePoll = alertFlags & SCE_NET_ADHOC_F_ALERTPOLL;

	for (ti = sock->waiters; ti != NULL; ti = ti->sockNext) {
		if (ti->wchan == NULL)
			continue;
		if ((wakeSend != 0 && ti->wchan == sock->sendWchan) ||
		    (wakeRecv != 0 && ti->wchan == sock->recvWchan) ||
		    (wakePoll != 0 && ti->wchan == sock->pollWchan)) {
			ti->wchan = NULL;
			sceKernelSignalSema(ti->sema, 1);
		}
	}
}
