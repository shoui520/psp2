#include "adhoc_int.h"

static struct {
	PtpPcb *head;
	PtpPcb **tail;
} g_ptp_list;
static u16 g_ptp_anon_port;
static u32 g_ptp_initial_seq;
static u32 g_ptp_stats[31];

static s32 ptpCreateSocket(const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 queue_max, s32 mode);
static s32 ptpFlushWrapper(s32 id, u32 timeout, s32 nonblock);
static s32 ptpWaitConnect(PtpPcb *ptp, u32 timeout, s32 flags);
static s32 ptpAcceptInternal(PtpPcb *ptp, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 flags, s32 *out_id);
static s32 ptpSendInternal(PtpPcb *ptp, const void *data, s32 *len, u32 timeout, s32 flags);
static s32 ptpRecvInternal(PtpPcb *ptp, void *data, s32 *len, u32 timeout, s32 flags);
static s32 ptpFlushInternal(PtpPcb *ptp, u32 timeout, s32 flags);
static s32 ptpAllocPcb(PtpPcb **out_ptp);
static void ptpDropTxQueue(PtpPcb *ptp);
static s32 ptpBindPcb(PtpPcb *ptp, const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 queue_max, s32 mode);
static void ptpMarkConnected(PtpPcb *ptp);
static void ptpClosePcb(PtpPcb *ptp);
static PtpPcb *ptpFindPcb(const u8 *local_mac, u16 local_port, const u8 *peer_mac, u16 peer_port);
static void ptpFreePcb(PtpPcb *ptp);
static void ptpClearTxQueue(PtpPcb *ptp);
static void ptpClearRxQueue(PtpPcb *ptp);
static void ptpFreeListenQueue(PtpPcb *ptp);
static u16 ptpAllocAnonPort(void);
static void ptpBuildAndSendPacket(const u8 *src_mac, u16 sport, const u8 *dst_mac, u16 dport, u8 type, u32 seq_or_ack, AdhocMbuf *payload_m);
static void ptpSendControlPacket(PtpPcb *ptp, u8 type);
static void ptpTransmitData(PtpPcb *ptp, s32 rexmt_only);
static void ptpRexmtTimeout(PtpPcb *ptp);
static void ptpProbeTimeout(PtpPcb *ptp);

/* === sceNetAdhocPtpOpen @ 0x2eec === */
s32 sceNetAdhocPtpOpen(const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 flags)
{
	u32 delay;

	if (_sceKernelCheckThreadStack() < 0x7c0)
		return 0x80410005;
	if (src_mac == NULL || dst_mac == NULL)
		return 0x80410701;
	if (dst_port == 0)
		return 0x80410702;
	if (bufsize == 0 || rexmt_int == 0 || rexmt_cnt <= 0)
		return 0x80410705;
	if (flags != 0)
		return 0x80410708;

	delay = sceNetIfhandle_lib_07505747(0);
	if (delay != 0)
		sceKernelDelayThread(delay);

	return ptpCreateSocket(src_mac, src_port, dst_mac, dst_port, bufsize, rexmt_int, rexmt_cnt, 0, 1);
}

/* === sceNetAdhocPtpConnect @ 0x3060 === */
s32 sceNetAdhocPtpConnect(s32 id, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	u32 delay;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	delay = sceNetIfhandle_lib_07505747(4);
	if (delay != 0)
		sceKernelDelayThread(delay);

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = adhocSockFind(2, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = ptpWaitConnect((PtpPcb *)sock->pcb, timeout, nonblock);
	adhocSockTryFree(sock);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpListen @ 0x31fc === */
s32 sceNetAdhocPtpListen(const u8 *src_mac, u16 src_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 backlog, s32 flags)
{
	if (_sceKernelCheckThreadStack() < 0x7c0)
		return 0x80410005;
	if (src_mac == NULL)
		return 0x80410701;
	if (src_port == 0)
		return 0x80410702;
	if (bufsize == 0 || rexmt_int == 0 || rexmt_cnt <= 0 || backlog <= 0)
		return 0x80410705;
	if (flags != 0)
		return 0x80410708;

	return ptpCreateSocket(src_mac, src_port, g_zero_mac, 0, bufsize, rexmt_int, rexmt_cnt, backlog, 2);
}

/* === sceNetAdhocPtpAccept @ 0x3334 === */
s32 sceNetAdhocPtpAccept(s32 id, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	s32 new_id = -1;
	u32 delay;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if ((peer_mac != NULL && peer_port == NULL) || (peer_mac == NULL && peer_port != NULL))
		return 0x80410706;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	delay = sceNetIfhandle_lib_07505747(2);
	if (delay != 0)
		sceKernelDelayThread(delay);

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = adhocSockFind(2, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = ptpAcceptInternal((PtpPcb *)sock->pcb, peer_mac, peer_port, timeout, nonblock, &new_id);
	adhocSockTryFree(sock);
	if (ret == 0)
		ret = new_id;

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpSend @ 0x34ec === */
s32 sceNetAdhocPtpSend(s32 id, const void *data, s32 *len, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (data == NULL || len == NULL)
		return 0x80410706;
	if (*len <= 0)
		return 0x80410705;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	sock = adhocSockFind(2, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = ptpSendInternal((PtpPcb *)sock->pcb, data, len, timeout, nonblock);
	adhocSockTryFree(sock);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpRecv @ 0x36a8 === */
s32 sceNetAdhocPtpRecv(s32 id, void *data, s32 *len, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (data == NULL || len == NULL)
		return 0x80410706;
	if (*len <= 0)
		return 0x80410705;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
	} else {
		sock = adhocSockFind(2, id);
		if (sock == NULL || sock->pcb == NULL) {
			ret = 0x80410707;
		} else {
			ret = ptpRecvInternal((PtpPcb *)sock->pcb, data, len, timeout, nonblock);
			adhocSockTryFree(sock);
		}
	}
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpFlush @ 0x3870 === */
s32 sceNetAdhocPtpFlush(s32 id, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	if (nonblock == 0) {
		w = (AdhocWaiter *)sceNet_lib_CA3CF5EB(&ret);
		if (w == NULL)
			return ret;
	}

	intr = sceNet_lib_94B44F26();
	if (intr < 0) {
		if (w != NULL)
			sceNet_lib_757085B0(w, 0);
		return intr;
	}

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
	} else {
		sock = adhocSockFind(2, id);
		if (sock == NULL || sock->pcb == NULL) {
			ret = 0x80410707;
		} else {
			ret = ptpFlushInternal((PtpPcb *)sock->pcb, timeout, nonblock);
			adhocSockTryFree(sock);
		}
	}

	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPtpClose @ 0x3a00 === */
s32 sceNetAdhocPtpClose(s32 id, s32 flags)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (flags != 0)
		return 0x80410711;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else {
		sock = adhocSockFind(2, id);
		if (sock == NULL) {
			ret = 0x80410701;
		} else {
			ptpClosePcb((PtpPcb *)sock->pcb);
			ptpFreeListenQueue((PtpPcb *)sock->pcb);
			sock->flags |= 1;
			sock->id = -1;
			if (sock->wait_list == NULL) {
				ptpFreePcb((PtpPcb *)sock->pcb);
				sock->pcb = NULL;
				adhocSockTryFree(sock);
			}
			ret = 0;
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === ptpCreateSocket @ 0x3afc === */
static s32 ptpCreateSocket(const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 queue_max, s32 mode)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	PtpPcb *ptp;
	s32 id;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	ret = adhocSockCreate(2, &sock);
	if (ret < 0)
		goto out;

	ret = ptpAllocPcb(&ptp);
	if (ret < 0) {
		adhocSockTryFree(sock);
		goto out;
	}

	ret = ptpBindPcb(ptp, src_mac, src_port, dst_mac, dst_port, bufsize, rexmt_int, rexmt_cnt, queue_max, mode);
	if (ret < 0) {
		ptpFreePcb(ptp);
		adhocSockTryFree(sock);
		goto out;
	}

	id = adhocSockAllocId();
	if (id < 0) {
		ptpFreePcb(ptp);
		adhocSockTryFree(sock);
		ret = 0x8041070b;
		goto out;
	}

	sock->id = id;
	sock->pcb = ptp;
	if (mode == 2) {
		sock->chan_rd = (u32)&ptp->q_head;
		sock->chan_wr = 0;
		sock->chan_ex = 0;
	} else {
		sock->chan_rd = (u32)&ptp->rx_head;
		sock->chan_wr = (u32)&ptp->tx_head;
		sock->chan_ex = (u32)&ptp->state;
	}
	ptp->sock = sock;
	ret = id;

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === ptpFlushWrapper @ 0x3c44 === */
static s32 ptpFlushWrapper(s32 id, u32 timeout, s32 nonblock)
{
	return sceNetAdhocPtpFlush(id, timeout, nonblock);
}

/* === ptpWaitConnect @ 0x3c7c === */
static s32 ptpWaitConnect(PtpPcb *ptp, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	s32 ret;

	if (ptp->mode != 1)
		return 0x80410704;

	if (ptp->state == 0) {
		AdhocIfnet *ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
		if (ifp == NULL)
			return 0x80410715;
		ptp->state = 2;
		ptpSendControlPacket(ptp, 1);
		ptp->rexmt_rem = ptp->rexmt_max;
		sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, ptpRexmtTimeout, ptp);
	} else if (ptp->state == 4) {
		return 0;
	} else if (ptp->state != 2 && ptp->state != 3) {
		return 0x80410704;
	}

	while (ptp->state != 4) {
		if (ptp->state == 0)
			return 0x80410717;
		if (flags & 1)
			return 0x80410709;
		ptp->flags |= 2;
		ret = sceNet_lib_F6DB0A0B(&ptp->state, &rem_timeout);
		if (ptp->sock == NULL)
			return 0x8041070c;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}
	return 0;
}

/* === ptpAcceptInternal @ 0x3e30 === */
static s32 ptpAcceptInternal(PtpPcb *ptp, u8 *peer_mac, u16 *peer_port, u32 timeout, s32 flags, s32 *out_id)
{
	u32 rem_timeout = timeout;
	PtpPcb *child;
	AdhocSocket *new_sock;
	s32 id;
	s32 ret;

	if (ptp->mode != 2 || ptp->state != 1)
		return 0x80410704;

	for (;;) {
		for (child = ptp->q_head; child != NULL; child = child->acc_next) {
			if (child->state == 4)
				break;
		}
		if (child != NULL)
			break;
		if (flags & 1)
			return 0x80410709;
		ptp->q_flags |= 1;
		ret = sceNet_lib_F6DB0A0B(&ptp->q_head, &rem_timeout);
		if (ptp->sock == NULL)
			return 0x8041070c;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}

	ret = adhocSockCreate(2, &new_sock);
	if (ret < 0)
		return ret;

	id = adhocSockAllocId();
	if (id < 0) {
		adhocSockTryFree(new_sock);
		return 0x8041070b;
	}

	if (child->acc_next != NULL)
		child->acc_next->acc_prev = child->acc_prev;
	else
		ptp->q_tail = child->acc_prev;
	*child->acc_prev = child->acc_next;
	child->acc_next = NULL;
	child->acc_prev = NULL;
	child->parent = NULL;
	ptp->q_len--;

	new_sock->id = id;
	new_sock->pcb = child;
	new_sock->chan_rd = (u32)&child->rx_head;
	new_sock->chan_wr = (u32)&child->tx_head;
	new_sock->chan_ex = (u32)&child->state;
	child->sock = new_sock;

	if (peer_mac != NULL)
		sceKernelMemcpy(peer_mac, child->peer_mac, 6);
	if (peer_port != NULL)
		*peer_port = child->peer_port;
	*out_id = id;
	return 0;
}

/* === ptpSendInternal @ 0x40ec === */
static s32 ptpSendInternal(PtpPcb *ptp, const void *data, s32 *len, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	s32 req_len = *len;
	s32 sent = 0;
	s32 ret;

	*len = 0;
	if (ptp->mode != 1 || ptp->state != 4)
		return 0x80410704;

	while (sent < req_len) {
		u32 space = ptp->tx_hiwat - ptp->tx_cc;
		if (space == 0) {
			if (flags & 1)
				return (sent > 0) ? 0 : 0x80410709;
			ptp->tx_flags |= 1;
			ret = sceNet_lib_F6DB0A0B(&ptp->tx_head, &rem_timeout);
			if (ptp->sock == NULL)
				return 0x8041070c;
			if (ptp->state != 4)
				return 0x8041070d;
			if (ret < 0) {
				if ((u32)ret == 0x80410186)
					return 0x80410715;
				if ((u32)ret == 0x800201a4)
					return 0x80410718;
				return ret;
			}
			continue;
		}

		while (sent < req_len && ptp->tx_hiwat - ptp->tx_cc > 0) {
			u32 avail = ptp->tx_hiwat - ptp->tx_cc;
			s32 chunk = req_len - sent;
			AdhocMbuf *m;

			if ((u32)chunk > avail)
				chunk = (s32)avail;
			if (chunk > 0x54c)
				chunk = 0x54c;

			m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
			if (m == NULL) {
				g_ptp_stats[13]++;
				return (sent > 0) ? 0 : 0x80410709;
			}
			m->m_next = NULL;
			m->m_nextpkt = NULL;
			m->m_data = (u8 *)m + 0x30;
			m->m_type = 1;
			m->m_flags = 2;
			m->m_pkthdr_Aux = 0;

			if (chunk + 0x20 >= 0xd1) {
				void *ext = sceNetIfhandle_lib_15CFE3C0(0x800);
				if (ext == NULL)
					ext = sceNetIfhandle_lib_15CFE3C0(0x800);
				*(void **)((u8 *)m + 0x30) = ext;
				if (ext != NULL) {
					m->m_data = (u8 *)ext;
					m->m_flags |= 9;
					*(u32 *)((u8 *)m + 0x3c) = 0x800;
					*(u32 *)((u8 *)m + 0x34) = 0;
					*(u32 *)((u8 *)m + 0x38) = 0;
					*(AdhocMbuf **)((u8 *)m + 0x48) = m;
					*(AdhocMbuf **)((u8 *)m + 0x44) = m;
				}
				if (!(m->m_flags & 1)) {
					sceNetIfhandle_lib_F8825DC4(m);
					g_ptp_stats[13]++;
					return (sent > 0) ? 0 : 0x80410709;
				}
			}

			sceKernelMemset(m->m_data, 0, 0x10);
			m->m_data += 0x10;
			sceKernelMemcpy(m->m_data, (const u8 *)data + sent, (u32)chunk);
			m->m_rcvif = NULL;
			m->m_pkthdr_len = chunk;
			m->m_len = chunk;

			if (ptp->tx_tail == NULL)
				ptp->tx_head = m;
			else
				ptp->tx_tail->m_nextpkt = m;
			ptp->tx_tail = m;
			ptp->tx_cc += (u32)chunk;
			ptp->snd_max++;

			sent += chunk;
			*len = sent;
		}

		ptpTransmitData(ptp, 0);
		if (flags & 1)
			break;
	}

	return 0;
}

/* === ptpRecvInternal @ 0x44d4 === */
static s32 ptpRecvInternal(PtpPcb *ptp, void *data, s32 *len, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	s32 req_len = *len;
	s32 rcvd = 0;
	AdhocMbuf *m;
	s32 ret;

	*len = 0;
	if (ptp->mode != 1 || ptp->state != 4)
		return 0x80410704;

	for (;;) {
		m = ptp->rx_head;
		if (m != NULL)
			break;
		if (flags & 1)
			return 0x80410709;
		ptp->rx_flags |= 1;
		ret = sceNet_lib_F6DB0A0B(&ptp->rx_head, &rem_timeout);
		if (ptp->sock == NULL)
			return 0x8041070c;
		if (ptp->state != 4)
			return 0x8041070d;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}

	while ((m = ptp->rx_head) != NULL && rcvd < req_len) {
		s32 pkt_len = m->m_pkthdr_len;
		s32 rem_req = req_len - rcvd;

		if (pkt_len <= rem_req) {
			AdhocMbuf *cur;

			ptp->rx_head = m->m_nextpkt;
			if (ptp->rx_head == NULL)
				ptp->rx_tail = NULL;
			m->m_nextpkt = NULL;

			for (cur = m; cur != NULL; cur = cur->m_next) {
				if (cur->m_len > 0) {
					sceKernelMemcpy((u8 *)data + rcvd, cur->m_data, (u32)cur->m_len);
					rcvd += cur->m_len;
				}
			}
			ptp->rx_cc -= (u32)pkt_len;
			sceNetIfhandle_lib_F56FAC82(m);
		} else {
			sceNetIfhandle_lib_9A6261EC(m, 0, rem_req, (u8 *)data + rcvd);
			sceNetIfhandle_lib_BFF3CEA5(m, rem_req);
			rcvd += rem_req;
			ptp->rx_cc -= (u32)rem_req;
		}
	}

	*len = rcvd;
	if ((ptp->flags & 1) && ptp->rx_hiwat - ptp->rx_cc >= 0x54c) {
		ptp->flags &= ~1;
		ptpSendControlPacket(ptp, 5);
	}
	return 0;
}

/* === ptpFlushInternal @ 0x4808 === */
static s32 ptpFlushInternal(PtpPcb *ptp, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	s32 ret;

	if (ptp->mode != 1 || ptp->state != 4)
		return 0x80410704;

	while (ptp->tx_cc != 0) {
		if (flags & 1)
			return 0x80410709;
		ptp->tx_flags |= 1;
		ret = sceNet_lib_F6DB0A0B(&ptp->tx_head, &rem_timeout);
		if (ptp->sock == NULL)
			return 0x8041070c;
		if (ptp->state != 4)
			return 0x8041070d;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}
	return 0;
}

/* === sceNetAdhoc_lib_62B875A8 @ 0x49cc === */
void sceNetAdhoc_lib_62B875A8(void)
{
	sceKernelMemset(g_ptp_stats, 0, 0x7c);
}

/* === sceNetAdhoc_lib_2CA12790 @ 0x49f4 === */
s32 sceNetAdhoc_lib_2CA12790(void *dst)
{
	if (dst == NULL)
		return 0x80410708;
	sceKernelMemcpy(dst, g_ptp_stats, 0x7c);
	return 0;
}

/* === ptpAllocPcb @ 0x4a30 === */
static s32 ptpAllocPcb(PtpPcb **out_ptp)
{
	PtpPcb *ptp;

	ptp = (PtpPcb *)sceNetIfhandle_lib_15CFE3C0(0xf0);
	if (ptp == NULL)
		return 0x80410709;
	sceKernelMemset(ptp, 0, 0xf0);
	sceNet_lib_25CC373A(ptp->timer_rexmt);
	sceNet_lib_25CC373A(ptp->timer_probe);
	ptp->q_head = NULL;
	ptp->q_tail = &ptp->q_head;
	ptp->next = NULL;
	ptp->prev = g_ptp_list.tail;
	*g_ptp_list.tail = ptp;
	g_ptp_list.tail = &ptp->next;
	*out_ptp = ptp;
	return 0;
}

/* === ptpDropTxQueue @ 0x4ad4 === */
static void ptpDropTxQueue(PtpPcb *ptp)
{
	AdhocMbuf *m;

	while ((m = ptp->tx_head) != NULL) {
		ptp->tx_head = m->m_nextpkt;
		if (ptp->tx_head == NULL)
			ptp->tx_tail = NULL;
		m->m_nextpkt = NULL;
		ptp->tx_cc -= (u32)m->m_pkthdr_len;
		sceNetIfhandle_lib_F56FAC82(m);
	}
	ptp->tx_hiwat = 0;

	while ((m = ptp->rx_head) != NULL) {
		ptp->rx_head = m->m_nextpkt;
		if (ptp->rx_head == NULL)
			ptp->rx_tail = NULL;
		m->m_nextpkt = NULL;
		ptp->rx_cc -= (u32)m->m_pkthdr_len;
		sceNetIfhandle_lib_F56FAC82(m);
	}
	ptp->rx_hiwat = 0;
}

/* === ptpBindPcb @ 0x4b7c === */
static s32 ptpBindPcb(PtpPcb *ptp, const u8 *src_mac, u16 src_port, const u8 *dst_mac, u16 dst_port, u32 bufsize, u32 rexmt_int, s32 rexmt_cnt, s32 queue_max, s32 mode)
{
	u8 local_mac[8];
	PtpPcb *cur;
	s32 ret;

	ret = sceNetGetLocalEtherAddr(local_mac);
	if (ret < 0)
		return ret;
	if (sceNet_lib_E0A81C7C(src_mac, local_mac, 6) != 0)
		return 0x80410701;

	if (mode == 1) {
		if ((dst_mac[0] & 1) || sceNet_lib_E0A81C7C(dst_mac, g_zero_mac, 6) == 0)
			return 0x80410701;
	}

	if (src_port == 0) {
		src_port = ptpAllocAnonPort();
		if (src_port == 0)
			return 0x8041070b;
	} else {
		for (cur = g_ptp_list.head; cur != NULL; cur = cur->next) {
			if (cur == ptp)
				continue;
			if (cur->local_port == src_port &&
			    sceNet_lib_E0A81C7C(cur->local_mac, src_mac, 6) == 0) {
				if (mode == 2 || cur->mode == 2)
					return 0x80410703;
				if (cur->peer_port == dst_port &&
				    sceNet_lib_E0A81C7C(cur->peer_mac, dst_mac, 6) == 0)
					return 0x80410703;
			}
		}
	}

	sceKernelMemcpy(ptp->local_mac, src_mac, 6);
	ptp->local_port = src_port;
	sceKernelMemcpy(ptp->peer_mac, dst_mac, 6);
	ptp->peer_port = dst_port;
	ptp->tx_hiwat = bufsize;
	ptp->rx_hiwat = bufsize;
	ptp->rcv_wnd = bufsize;
	ptp->rexmt_intvl = rexmt_int;
	ptp->rexmt_max = rexmt_cnt;
	ptp->q_maxlen = queue_max;
	ptp->mode = mode;
	if (mode == 2)
		ptp->state = 1;
	else
		ptp->state = 0;
	ptpDropTxQueue(ptp);
	return 0;
}

/* === ptpMarkConnected @ 0x4d48 === */
static void ptpMarkConnected(PtpPcb *ptp)
{
	sceNet_lib_A93A93E9(ptp->timer_rexmt);
	ptp->state = 4;
	ptp->snd_una = ptp->iss + 1;
	ptp->snd_nxt = ptp->iss + 1;
	ptp->snd_max = ptp->iss + 1;
	ptp->rcv_nxt = ptp->irs + 1;

	sceNet_lib_A8B6205A(ptp->timer_probe, ptp->rexmt_intvl, ptpProbeTimeout, ptp);

	if (ptp->parent != NULL) {
		PtpPcb *parent = ptp->parent;
		if (parent->q_flags & 1) {
			parent->q_flags &= ~1;
			sceNet_lib_7574FDA1(&parent->q_head);
		}
		if (parent->q_flags & 2) {
			parent->q_flags &= ~2;
			sceNet_lib_7574FDA1(&g_global_wait_head);
		}
	} else {
		if (ptp->flags & 2) {
			ptp->flags &= ~2;
			sceNet_lib_7574FDA1(&ptp->state);
		}
		if (ptp->flags & 4) {
			ptp->flags &= ~4;
			sceNet_lib_7574FDA1(&g_global_wait_head);
		}
	}
}

/* === ptpClosePcb @ 0x4e3c === */
static void ptpClosePcb(PtpPcb *ptp)
{
	sceNet_lib_A93A93E9(ptp->timer_rexmt);
	sceNet_lib_A93A93E9(ptp->timer_probe);

	if (ptp->parent != NULL) {
		ptpFreePcb(ptp);
		return;
	}

	ptp->state = 0;
	ptpClearTxQueue(ptp);
	ptpClearRxQueue(ptp);

	if (ptp->flags & 2) {
		ptp->flags &= ~2;
		sceNet_lib_7574FDA1(&ptp->state);
	}
	if (ptp->tx_flags & 1) {
		ptp->tx_flags &= ~1;
		sceNet_lib_7574FDA1(&ptp->tx_head);
	}
	if (ptp->rx_flags & 1) {
		ptp->rx_flags &= ~1;
		sceNet_lib_7574FDA1(&ptp->rx_head);
	}
	if ((ptp->flags & 4) || (ptp->tx_flags & 2) || (ptp->rx_flags & 2)) {
		ptp->flags &= ~4;
		ptp->tx_flags &= ~2;
		ptp->rx_flags &= ~2;
		sceNet_lib_7574FDA1(&g_global_wait_head);
	}
}

/* === ptpPollEvents @ 0x4fc8 === */
s32 ptpPollEvents(AdhocSocket *sock, s32 events)
{
	PtpPcb *ptp = (PtpPcb *)sock->pcb;
	s32 rev = 0;

	if (ptp == NULL)
		return 0x10;

	if (ptp->mode == 1) {
		if (ptp->state == 0)
			return 0x10;
		if (events & 4) {
			if (ptp->state == 4)
				rev |= 4;
		}
		if (events & 1) {
			if (ptp->state == 4 && ptp->tx_cc < ptp->tx_hiwat)
				rev |= 1;
		}
		if (events & 2) {
			if (ptp->state == 4 && ptp->rx_head != NULL)
				rev |= 2;
		}
	} else if (ptp->mode == 2) {
		if (ptp->state != 1)
			return 0x10;
		if (events & 8) {
			PtpPcb *child;
			for (child = ptp->q_head; child != NULL; child = child->acc_next) {
				if (child->state == 4) {
					rev |= 8;
					break;
				}
			}
		}
	}
	return rev;
}

/* === sceNetAdhocGetPtpStat @ 0x5104 === */
s32 sceNetAdhocGetPtpStat(s32 *buflen, void *buf)
{
	s32 intr;
	s32 ret;
	PtpPcb *ptp;
	s32 total_bytes;
	u8 *out;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (buflen == NULL)
		return 0x80410708;
	if (*buflen < 0)
		return 0x80410705;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
		goto out;
	}
	if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
		goto out;
	}

	if (buf == NULL) {
		total_bytes = 0;
		for (ptp = g_ptp_list.head; ptp != NULL; ptp = ptp->next) {
			if (ptp->parent == NULL)
				total_bytes += 0x20;
		}
		*buflen = total_bytes;
		ret = 0;
	} else {
		s32 max_len = *buflen;
		out = (u8 *)buf;
		total_bytes = 0;
		for (ptp = g_ptp_list.head; ptp != NULL; ptp = ptp->next) {
			if (ptp->parent != NULL)
				continue;
			if (total_bytes + 0x20 > max_len)
				break;
			((ScePdpStatOut *)out)->next = NULL;
			((ScePtpStatOut *)out)->ptp_id = (ptp->sock != NULL) ? ptp->sock->id : -1;
			sceKernelMemcpy(out + 0x08, ptp->local_mac, 6);
			sceKernelMemcpy(out + 0x0e, ptp->peer_mac, 6);
			((ScePtpStatOut *)out)->local_port = ptp->local_port;
			((ScePtpStatOut *)out)->peer_port = ptp->peer_port;
			((ScePtpStatOut *)out)->sent_data = ptp->tx_cc;
			((ScePtpStatOut *)out)->rcvd_data = ptp->rx_cc;
			if (total_bytes >= 0x20)
				*(void **)(out - 0x20) = out;
			out += 0x20;
			total_bytes += 0x20;
		}
		*buflen = total_bytes;
		ret = 0;
	}

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === ptpInit @ 0x5288 === */
void ptpInit(void)
{
	g_ptp_list.head = NULL;
	g_ptp_list.tail = &g_ptp_list.head;
	g_ptp_anon_port = (u16)((sceNet_lib_1858883D() % 0xc000) + 0x4000);
	g_ptp_initial_seq = sceNet_lib_1858883D();
	sceKernelMemset(g_ptp_stats, 0, 0x7c);
}

/* === ptpTerm @ 0x52e0 === */
void ptpTerm(void)
{
	PtpPcb *ptp;

	while ((ptp = g_ptp_list.head) != NULL) {
		if (ptp->sock != NULL) {
			ptp->sock->pcb = NULL;
			adhocSockTryFree(ptp->sock);
			ptp->sock = NULL;
		}
		ptpFreePcb(ptp);
	}
}

/* === ptpFindPcb @ 0x536c === */
static PtpPcb *ptpFindPcb(const u8 *local_mac, u16 local_port, const u8 *peer_mac, u16 peer_port)
{
	PtpPcb *ptp;

	for (ptp = g_ptp_list.head; ptp != NULL; ptp = ptp->next) {
		if (ptp->local_port == local_port && ptp->peer_port == peer_port &&
		    sceNet_lib_E0A81C7C(ptp->local_mac, local_mac, 6) == 0 &&
		    sceNet_lib_E0A81C7C(ptp->peer_mac, peer_mac, 6) == 0)
			return ptp;
	}
	return NULL;
}

/* === ptpFreePcb @ 0x542c === */
static void ptpFreePcb(PtpPcb *ptp)
{
	PtpPcb *child;

	sceNet_lib_A93A93E9(ptp->timer_rexmt);
	sceNet_lib_A93A93E9(ptp->timer_probe);

	while ((child = ptp->q_head) != NULL) {
		ptpFreePcb(child);
	}

	if (ptp->parent != NULL) {
		PtpPcb *parent = ptp->parent;
		if (ptp->acc_next != NULL)
			ptp->acc_next->acc_prev = ptp->acc_prev;
		else
			parent->q_tail = ptp->acc_prev;
		*ptp->acc_prev = ptp->acc_next;
		parent->q_len--;
	}

	if (ptp->next != NULL)
		ptp->next->prev = ptp->prev;
	else
		g_ptp_list.tail = ptp->prev;
	*ptp->prev = ptp->next;

	ptpClearTxQueue(ptp);
	ptpClearRxQueue(ptp);
	sceNetIfhandle_lib_76BAD213(ptp);
}

/* === ptpClearTxQueue @ 0x5590 === */
static void ptpClearTxQueue(PtpPcb *ptp)
{
	AdhocMbuf *m;

	while ((m = ptp->tx_head) != NULL) {
		ptp->tx_head = m->m_nextpkt;
		if (ptp->tx_head == NULL)
			ptp->tx_tail = NULL;
		m->m_nextpkt = NULL;
		sceNetIfhandle_lib_F56FAC82(m);
	}
	ptp->tx_cc = 0;
}

/* === ptpClearRxQueue @ 0x55dc === */
static void ptpClearRxQueue(PtpPcb *ptp)
{
	AdhocMbuf *m;

	while ((m = ptp->rx_head) != NULL) {
		ptp->rx_head = m->m_nextpkt;
		if (ptp->rx_head == NULL)
			ptp->rx_tail = NULL;
		m->m_nextpkt = NULL;
		sceNetIfhandle_lib_F56FAC82(m);
	}
	ptp->rx_cc = 0;
}

/* === ptpHasActive @ 0x55f8 === */
s32 ptpHasActive(void)
{
	return (g_ptp_list.head != NULL) ? 1 : 0;
}

/* === ptpIfDetach @ 0x5604 === */
void ptpIfDetach(void *ifp_arg)
{
	AdhocIfnet *ifp = (AdhocIfnet *)ifp_arg;
	PtpPcb *ptp;

	if (ifp->snd_len >= ifp->snd_maxlen)
		return;

	for (ptp = g_ptp_list.head; ptp != NULL; ptp = ptp->next) {
		if (ptp->state == 4 && ptp->tx_head != NULL && ptp->snd_nxt < ptp->snd_max) {
			ptpTransmitData(ptp, 0);
			if (ifp->snd_len >= ifp->snd_maxlen)
				break;
		}
	}
}

/* === ptpIfDetachNoop @ 0x572c === */
void ptpIfDetachNoop(void *ifp_arg)
{
	(void)ifp_arg;
}

/* === ptpFreeListenQueue @ 0x5750 === */
static void ptpFreeListenQueue(PtpPcb *ptp)
{
	PtpPcb *child;

	while ((child = ptp->q_head) != NULL) {
		AdhocSocket *csock = child->sock;
		if (child->acc_next != NULL)
			child->acc_next->acc_prev = child->acc_prev;
		else
			ptp->q_tail = child->acc_prev;
		*child->acc_prev = child->acc_next;
		ptp->q_len--;
		ptpClosePcb(child);
		ptpFreeListenQueue(child);
		if (csock != NULL) {
			PtpPcb *cpcb = (PtpPcb *)csock->pcb;
			csock->pcb = NULL;
			if (cpcb != NULL) {
				if (cpcb->next != NULL)
					cpcb->next->prev = cpcb->prev;
				else
					g_ptp_list.tail = cpcb->prev;
				*cpcb->prev = cpcb->next;
				sceNetIfhandle_lib_76BAD213(cpcb);
			}
			adhocSockTryFree(csock);
		}
	}
	sceKernelMemset(ptp->local_mac, 0, 6);
	sceKernelMemset(ptp->peer_mac, 0, 6);
	ptp->local_port = 0;
	ptpDropTxQueue(ptp);
}

/* === ptpAllocAnonPort @ 0x584c === */
static u16 ptpAllocAnonPort(void)
{
	u16 cand = g_ptp_anon_port;
	s32 count = 0;
	PtpPcb *cur;

	while (count < 0x4000) {
		s32 used = 0;
		for (cur = g_ptp_list.head; cur != NULL; cur = cur->next) {
			if (cur->local_port == cand) {
				used = 1;
				break;
			}
		}
		cand++;
		if (cand < 0xc000)
			cand = 0xc000;
		if (!used) {
			g_ptp_anon_port = cand;
			return (u16)(cand == 0xc000 ? 0xffff : cand - 1);
		}
		count++;
	}
	return 0;
}

/* === ptpInputPacket @ 0x5938 === */
void ptpInputPacket(AdhocMbuf *m)
{
	u8 dst_mac[6];
	u8 src_mac[6];
	u16 body_len;
	u16 sport;
	u16 dport;
	u32 seq;
	u32 wnd;
	u8 type;
	s32 payload_len;
	PtpPcb *ptp;
	u8 *hdr;

	if (m->m_pkthdr_len < 0x3c) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}
	if (m->m_len < 0x3c) {
		m = (AdhocMbuf *)sceNetIfhandle_lib_49EDBB18(m, 0x3c);
		if (m == NULL) {
			g_ptp_stats[0]++;
			return;
		}
	}

	hdr = m->m_data;
	type = hdr[0x31];
	if (type == 0 || type >= 8) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}

	sceKernelMemcpy(&body_len, hdr + 0x32, 2);
	body_len = bswap16(body_len);
	if (body_len < 0x0c || m->m_pkthdr_len < (s32)body_len + 0x30) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}

	payload_len = (s32)body_len - 0x0c;
	if (((type == 1 || type == 2 || type == 5) && payload_len != 4) ||
	    ((type == 3 || type == 6 || type == 7) && payload_len != 0)) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}

	sceKernelMemcpy(dst_mac, hdr, 6);
	sceKernelMemcpy(src_mac, hdr + 6, 6);
	sceKernelMemcpy(&sport, hdr + 0x34, 2);
	sport = bswap16(sport);
	sceKernelMemcpy(&dport, hdr + 0x36, 2);
	dport = bswap16(dport);
	sceKernelMemcpy(&seq, hdr + 0x38, 4);
	seq = bswap32(seq);

	if (sport == 0 || dport == 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_ptp_stats[0]++;
		return;
	}

	wnd = 0;
	if (type == 1 || type == 2 || type == 5) {
		if (m->m_len < 0x40) {
			m = (AdhocMbuf *)sceNetIfhandle_lib_49EDBB18(m, 0x40);
			if (m == NULL) {
				g_ptp_stats[0]++;
				return;
			}
		}
		sceKernelMemcpy(&wnd, m->m_data + 0x3c, 4);
		wnd = bswap32(wnd);
	}

	{
		s32 pad_trim = (payload_len + 0x3c) - m->m_pkthdr_len;
		if (pad_trim < 0)
			sceNetIfhandle_lib_BFF3CEA5(m, pad_trim);
	}

	ptp = ptpFindPcb(dst_mac, dport, src_mac, sport);
	if (ptp == NULL) {
		if (type == 1) {
			ptp = ptpFindPcb(dst_mac, dport, g_zero_mac, 0);
			type = m->m_data[0x31];
		}
		if (ptp == NULL) {
			if (type != 7)
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[27]++;
			sceNetIfhandle_lib_F56FAC82(m);
			return;
		}
	}

	switch (type) {
	case 1: /* SYN */
		switch (ptp->state) {
		case 0:
		case 2:
		case 4:
			ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[3]++;
			break;
		case 1: {
			PtpPcb *child;
			if (ptp->q_len >= ptp->q_maxlen) {
				ptpFreeListenQueue(ptp);
				if (ptp->q_len >= ptp->q_maxlen) {
					ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
					g_ptp_stats[2]++;
					break;
				}
			}
			if (ptpAllocPcb(&child) < 0) {
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[2]++;
				break;
			}
			sceKernelMemcpy(child->local_mac, dst_mac, 6);
			child->local_port = dport;
			sceKernelMemcpy(child->peer_mac, src_mac, 6);
			child->peer_port = sport;
			child->tx_hiwat = ptp->tx_hiwat;
			child->rx_hiwat = ptp->rx_hiwat;
			child->rcv_wnd = (wnd < ptp->rcv_wnd) ? wnd : ptp->rcv_wnd;
			child->snd_wnd = wnd;
			child->irs = seq;
			child->rexmt_intvl = ptp->rexmt_intvl;
			child->rexmt_max = ptp->rexmt_max;
			child->mode = 1;
			child->state = 3;
			child->parent = ptp;
			ptpDropTxQueue(child);
			child->acc_next = NULL;
			child->acc_prev = ptp->q_tail;
			*ptp->q_tail = child;
			ptp->q_tail = &child->acc_next;
			ptp->q_len++;
			ptpSendControlPacket(child, 2);
			child->rexmt_rem = child->rexmt_max;
			sceNet_lib_A8B6205A(child->timer_rexmt, child->rexmt_intvl, ptpRexmtTimeout, child);
			g_ptp_stats[1]++;
			break;
		}
		case 3:
			if (ptp->irs != seq) {
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[3]++;
			} else {
				ptpSendControlPacket(ptp, 2);
				g_ptp_stats[1]++;
			}
			break;
		default:
			break;
		}
		break;

	case 2: /* SYN-ACK */
		switch (ptp->state) {
		case 0:
		case 1:
		case 3:
			ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[7]++;
			break;
		case 2:
			if (ptp->iss != seq) {
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[7]++;
			} else {
				ptp->snd_wnd = wnd;
				if (wnd < ptp->rcv_wnd)
					ptp->rcv_wnd = wnd;
				ptpMarkConnected(ptp);
				ptpSendControlPacket(ptp, 3);
				g_ptp_stats[5]++;
			}
			break;
		case 4:
			if (ptp->iss != seq) {
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[7]++;
			} else {
				ptpSendControlPacket(ptp, 3);
				g_ptp_stats[6]++;
			}
			break;
		default:
			break;
		}
		break;

	case 3: /* SYN-ACK-ACK */
		switch (ptp->state) {
		case 0:
			g_ptp_stats[11]++;
			break;
		case 1:
		case 2:
			ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[11]++;
			break;
		case 3:
			if (ptp->iss == seq) {
				ptpMarkConnected(ptp);
				g_ptp_stats[9]++;
			} else {
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[11]++;
			}
			break;
		case 4:
			if (ptp->iss != seq) {
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[11]++;
			} else {
				g_ptp_stats[10]++;
			}
			break;
		default:
			break;
		}
		break;

	case 4: /* DATA */
		switch (ptp->state) {
		case 0:
			g_ptp_stats[16]++;
			break;
		case 1:
		case 2:
			ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
			g_ptp_stats[16]++;
			break;
		case 3:
			if (ptp->rcv_nxt == seq) {
				ptpSendControlPacket(ptp, 2);
				sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, ptpRexmtTimeout, ptp);
				ptp->rexmt_rem = ptp->rexmt_max;
			}
			break;
		case 4:
			if (ptp->rcv_nxt != seq) {
				if ((s32)(ptp->rcv_nxt - seq) > 0) {
					ptpSendControlPacket(ptp, 5);
					g_ptp_stats[15]++;
				} else {
					if (!(ptp->flags & 1)) {
						ptp->flags |= 1;
						ptpSendControlPacket(ptp, 6);
					}
					g_ptp_stats[16]++;
				}
				break;
			}
			sceNetIfhandle_lib_BFF3CEA5(m, 0x3c);
			if ((s32)(ptp->rx_hiwat - ptp->rx_cc) < m->m_pkthdr_len) {
				g_ptp_stats[14]++;
				break;
			}
			m->m_nextpkt = NULL;
			ptp->flags &= ~1;
			if (ptp->rx_tail == NULL)
				ptp->rx_head = m;
			else
				ptp->rx_tail->m_nextpkt = m;
			ptp->rx_tail = m;
			ptp->rx_cc += (u32)m->m_pkthdr_len;
			ptp->rcv_nxt++;
			ptpSendControlPacket(ptp, 5);
			if (ptp->rx_flags & 1) {
				ptp->rx_flags &= ~1;
				sceNet_lib_7574FDA1(&ptp->rx_head);
			}
			if (ptp->rx_flags & 2) {
				ptp->rx_flags &= ~2;
				sceNet_lib_7574FDA1(&g_global_wait_head);
			}
			g_ptp_stats[13]++;
			return;
		default:
			break;
		}
		break;

	case 5: /* ACK */
		if (ptp->state < 4) {
			if (ptp->state > 0) {
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[20]++;
			} else if (ptp->state == 0) {
				g_ptp_stats[20]++;
			}
			break;
		}
		if (ptp->state != 4)
			break;

		if ((s32)(ptp->snd_una - seq) > 0) {
			ptp->snd_wnd = wnd;
			if (ptp->tx_flags & 1) {
				ptp->tx_flags &= ~1;
				sceNet_lib_7574FDA1(&ptp->tx_head);
			}
			if (ptp->tx_flags & 2) {
				ptp->tx_flags &= ~2;
				sceNet_lib_7574FDA1(&g_global_wait_head);
			}
			ptpTransmitData(ptp, 0);
			g_ptp_stats[19]++;
			break;
		}

		for (;;) {
			AdhocMbuf *txm = ptp->tx_head;
			if (txm == NULL) {
				ptpClosePcb(ptp);
				g_ptp_stats[20]++;
				goto drop;
			}
			ptp->tx_head = txm->m_nextpkt;
			if (ptp->tx_head == NULL)
				ptp->tx_tail = NULL;
			ptp->snd_una++;
			ptp->tx_cc -= (u32)txm->m_pkthdr_len;
			txm->m_nextpkt = NULL;
			sceNetIfhandle_lib_F56FAC82(txm);
			if ((s32)(ptp->snd_una - seq) > 0)
				break;
		}

		if ((s32)(ptp->snd_nxt - ptp->snd_una) < 0)
			ptp->snd_nxt = ptp->snd_una;
		if (ptp->snd_una == ptp->snd_max)
			sceNet_lib_A93A93E9(ptp->timer_rexmt);

		ptp->snd_wnd = wnd;
		if (ptp->tx_flags & 1) {
			ptp->tx_flags &= ~1;
			sceNet_lib_7574FDA1(&ptp->tx_head);
		}
		if (ptp->tx_flags & 2) {
			ptp->tx_flags &= ~2;
			sceNet_lib_7574FDA1(&g_global_wait_head);
		}
		ptpTransmitData(ptp, 0);
		g_ptp_stats[18]++;
		break;

	case 6: /* NACK / FAST REXMT */
		if (ptp->state < 4) {
			if (ptp->state > 0) {
				ptpBuildAndSendPacket(dst_mac, dport, src_mac, sport, 7, 0, NULL);
				g_ptp_stats[23]++;
			} else if (ptp->state == 0) {
				g_ptp_stats[23]++;
			}
			break;
		}
		if (ptp->state == 4) {
			if (ptp->snd_una != seq) {
				g_ptp_stats[23]++;
				break;
			}
			sceNet_lib_A93A93E9(ptp->timer_rexmt);
			ptp->snd_nxt = ptp->snd_una;
			ptpTransmitData(ptp, 0);
			g_ptp_stats[22]++;
		}
		break;

	case 7: /* RST */
		if (ptp->state == 1 || ptp->state == 0) {
			g_ptp_stats[26]++;
		} else if (ptp->state >= 2 && ptp->state < 5) {
			g_ptp_stats[25]++;
			ptpClosePcb(ptp);
		}
		break;

	default:
		break;
	}

drop:
	sceNetIfhandle_lib_F56FAC82(m);
}

/* === ptpBuildAndSendPacket @ 0x637c === */
static void ptpBuildAndSendPacket(const u8 *src_mac, u16 sport, const u8 *dst_mac, u16 dport, u8 type, u32 seq_or_ack, AdhocMbuf *payload_m)
{
	AdhocIfnet *ifp;
	s32 payload_len;
	s32 tot_body;
	s32 pad_len;
	s32 alloc_len;
	AdhocMbuf *m;
	u8 *hdr;
	u16 tmp16;
	u32 tmp32;

	ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
	if (ifp == NULL)
		return;
	if (ifp->snd_len >= ifp->snd_maxlen)
		return;

	payload_len = (payload_m != NULL) ? payload_m->m_pkthdr_len : 0;
	m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (m == NULL)
		return;

	tot_body = payload_len + 0x0c;
	pad_len = tot_body & 0x0f;
	if (pad_len > 0)
		pad_len = 0x10 - pad_len;
	alloc_len = payload_len + pad_len;

	m->m_next = NULL;
	m->m_nextpkt = NULL;
	m->m_data = (u8 *)m + 0x30;
	m->m_type = 1;
	m->m_flags = 2;
	m->m_pkthdr_Aux = 0;

	if (alloc_len + 0x3c >= 0xd1) {
		void *ext = sceNetIfhandle_lib_15CFE3C0(0x800);
		if (ext == NULL)
			ext = sceNetIfhandle_lib_15CFE3C0(0x800);
		*(void **)((u8 *)m + 0x30) = ext;
		if (ext != NULL) {
			m->m_data = (u8 *)ext;
			m->m_flags |= 9;
			*(u32 *)((u8 *)m + 0x3c) = 0x800;
			*(u32 *)((u8 *)m + 0x34) = 0;
			*(u32 *)((u8 *)m + 0x38) = 0;
			*(AdhocMbuf **)((u8 *)m + 0x48) = m;
			*(AdhocMbuf **)((u8 *)m + 0x44) = m;
		}
		if (!(m->m_flags & 1)) {
			sceNetIfhandle_lib_F8825DC4(m);
			return;
		}
	}

	hdr = m->m_data;
	sceKernelMemcpy(hdr, dst_mac, 6);
	sceKernelMemcpy(hdr + 6, src_mac, 6);
	sceKernelMemcpy(hdr + 0x0c, &g_adhoc_ethertype, 2);
	sceKernelMemcpy(hdr + 0x0e, &g_adhoc_hdr_ver, 2);

	hdr = m->m_data;
	hdr[0x30] = 2;
	hdr[0x31] = type;
	tmp16 = bswap16((u16)tot_body);
	sceKernelMemcpy(hdr + 0x32, &tmp16, 2);
	tmp16 = bswap16(sport);
	sceKernelMemcpy(hdr + 0x34, &tmp16, 2);
	tmp16 = bswap16(dport);
	sceKernelMemcpy(hdr + 0x36, &tmp16, 2);
	tmp32 = bswap32(seq_or_ack);
	sceKernelMemcpy(hdr + 0x38, &tmp32, 4);

	if (payload_len > 0) {
		if (payload_m->m_len >= payload_len)
			sceKernelMemcpy(m->m_data + 0x3c, payload_m->m_data, (u32)payload_len);
		else
			sceNetIfhandle_lib_9A6261EC(payload_m, 0, payload_len, m->m_data + 0x3c);
	}
	if (pad_len > 0)
		sceKernelMemset(m->m_data + 0x3c + payload_len, 0, (u32)pad_len);

	m->m_rcvif = NULL;
	m->m_pkthdr_len = alloc_len + 0x3c;
	m->m_len = alloc_len + 0x3c;
	{
		u32 pkt_len = (u32)(alloc_len + 0x3c);
		u32 new_lo = ifp->if_obytes_lo + pkt_len;
		u32 carry = (new_lo < pkt_len) ? 1 : 0;
		ifp->if_obytes_lo = new_lo;
		ifp->if_obytes_hi += carry;
	}
	adhocIfOutput(ifp, m);
}

/* === ptpSendControlPacket @ 0x668c === */
static void ptpSendControlPacket(PtpPcb *ptp, u8 type)
{
	u32 seq_val;
	AdhocMbuf *m = NULL;

	switch (type) {
	case 1:
	case 2:
	case 3:
	case 7:
		seq_val = ptp->iss;
		break;
	case 5:
		seq_val = ptp->rcv_nxt - 1;
		break;
	case 6:
		seq_val = ptp->rcv_nxt;
		break;
	default:
		seq_val = 0;
		break;
	}

	if (type == 1 || type == 2 || type == 5) {
		u32 wnd_be;
		m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (m == NULL)
			return;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
		m->m_data = (u8 *)m + 0x30;
		m->m_type = 1;
		m->m_flags = 2;
		m->m_rcvif = NULL;
		m->m_pkthdr_len = 4;
		m->m_len = 4;
		m->m_pkthdr_Aux = 0;
		wnd_be = bswap32(ptp->rx_hiwat - ptp->rx_cc);
		sceKernelMemcpy(m->m_data, &wnd_be, 4);
	}

	switch (type) {
	case 1:
		g_ptp_stats[0]++;
		break;
	case 2:
		g_ptp_stats[4]++;
		break;
	case 3:
		g_ptp_stats[8]++;
		break;
	case 5:
		g_ptp_stats[17]++;
		break;
	case 6:
		g_ptp_stats[21]++;
		break;
	case 7:
		g_ptp_stats[24]++;
		break;
	default:
		break;
	}

	ptpBuildAndSendPacket(ptp->local_mac, ptp->local_port, ptp->peer_mac, ptp->peer_port, type, seq_val, m);
	if (m != NULL)
		sceNetIfhandle_lib_F56FAC82(m);
}

/* === ptpTransmitData @ 0x6878 === */
static void ptpTransmitData(PtpPcb *ptp, s32 rexmt_only)
{
	AdhocMbuf *m = ptp->tx_head;
	u32 off_bytes = 0;
	s32 sent_cnt = 0;
	u32 seq;

	if (m == NULL)
		return;

	for (seq = ptp->snd_una; seq != ptp->snd_nxt; seq++) {
		off_bytes += (u32)m->m_pkthdr_len;
		m = m->m_nextpkt;
		if (m == NULL)
			return;
	}

	while (m != NULL) {
		u32 *ts = (u32 *)((u8 *)m + 0x20);
		if ((s32)(ptp->snd_wnd - off_bytes) < m->m_pkthdr_len) {
			if (sent_cnt != 0)
				break;
			if (rexmt_only != 0) {
				ptpBuildAndSendPacket(ptp->local_mac, ptp->local_port, ptp->peer_mac, ptp->peer_port, 4, ptp->snd_una - 1, NULL);
				g_ptp_stats[29]++;
				return;
			}
			if (!(ptp->timer_rexmt_active & 1)) {
				g_ptp_stats[30]++;
				ptp->rexmt_rem = ptp->rexmt_max;
				sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, ptpRexmtTimeout, ptp);
			}
			break;
		}

		ptpBuildAndSendPacket(ptp->local_mac, ptp->local_port, ptp->peer_mac, ptp->peer_port, 4, ptp->snd_nxt, m);
		sent_cnt++;
		if ((ts[2] | ts[3]) == 0) {
			s64 now = sceKernelGetSystemTimeWide();
			ts[2] = (u32)now;
			ts[3] = (u32)((u64)now >> 32);
		}
		ptp->snd_nxt++;
		g_ptp_stats[12]++;
		off_bytes += (u32)m->m_pkthdr_len;
		m = m->m_nextpkt;
	}

	if (rexmt_only == 0 && sent_cnt > 0) {
		ptp->rexmt_rem = ptp->rexmt_max;
		sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, ptpRexmtTimeout, ptp);
	}
}

/* === ptpRexmtTimeout @ 0x6a54 === */
static void ptpRexmtTimeout(PtpPcb *ptp)
{
	if (ptp->state < 2)
		return;

	if (ptp->rexmt_rem <= 0) {
		ptp->rexmt_rem--;
		ptpClosePcb(ptp);
		return;
	}
	ptp->rexmt_rem--;

	if (ptp->state == 2) {
		ptpSendControlPacket(ptp, 1);
	} else if (ptp->state == 3) {
		ptpSendControlPacket(ptp, 2);
	} else if (ptp->state == 4) {
		ptp->snd_nxt = ptp->snd_una;
		ptpTransmitData(ptp, 1);
		g_ptp_stats[28]++;
	}
	sceNet_lib_A8B6205A(ptp->timer_rexmt, ptp->rexmt_intvl, ptpRexmtTimeout, ptp);
}

/* === ptpProbeTimeout @ 0x6b20 === */
static void ptpProbeTimeout(PtpPcb *ptp)
{
	AdhocMbuf *m;

	if (ptp->state != 4)
		return;

	m = ptp->tx_head;
	if (m != NULL) {
		u32 *ts = (u32 *)((u8 *)m + 0x20);
		if ((ts[2] | ts[3]) != 0) {
			s64 now = sceKernelGetSystemTimeWide();
			s64 start = (s64)(((u64)ts[3] << 32) | ts[2]);
			s64 max_wait = (s64)ptp->rexmt_intvl * (s64)(ptp->rexmt_max + 1);
			if (now - start > max_wait) {
				ptpClosePcb(ptp);
				return;
			}
		}
	}
	sceNet_lib_A8B6205A(ptp->timer_probe, ptp->rexmt_intvl, ptpProbeTimeout, ptp);
}

