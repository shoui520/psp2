#include "adhoc_int.h"

static struct {
	PdpPcb *head;
	PdpPcb **tail;
} g_pdp_list;
static u16 g_pdp_anon_port;
static u32 g_pdp_stats[6];
static u16 g_pdp_seq;

static s32 pdpSendInternal(PdpPcb *pdp, const u8 *dst_mac, u16 dport, const void *data, s32 len, u32 timeout, s32 flags);
static s32 pdpRecvInternal(PdpPcb *pdp, u8 *src_mac, u16 *sport, void *data, s32 *data_len, u32 timeout, s32 flags);
static s32 pdpRecvById(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags);
static void pdpFlushQueues(PdpPcb *pdp);
static s32 pdpAllocPcb(PdpPcb **out_pdp);
static s32 pdpBindPcb(PdpPcb *pdp, const u8 *mac, u16 port, u32 bufsize);
static void pdpFreePcb(PdpPcb *pdp);
static u16 pdpAllocAnonPort(void);

/* === sceNetAdhocPdpCreate @ 0x1130 === */
s32 sceNetAdhocPdpCreate(const u8 *mac, u16 port, u32 bufsize, s32 flags)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	PdpPcb *pdp;
	s32 id;
	u32 delay;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (mac == NULL)
		return 0x80410701;
	if (bufsize == 0)
		return 0x80410705;
	if (flags != 0)
		return 0x80410708;

	delay = sceNetIfhandle_lib_07505747(3);
	if (delay != 0)
		sceKernelDelayThread(delay);

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

	ret = adhocSockCreate(1, &sock);
	if (ret < 0)
		goto out;

	ret = pdpAllocPcb(&pdp);
	if (ret < 0) {
		adhocSockTryFree(sock);
		goto out;
	}

	ret = pdpBindPcb(pdp, mac, port, bufsize);
	if (ret < 0) {
		pdpFreePcb(pdp);
		adhocSockTryFree(sock);
		goto out;
	}

	id = adhocSockAllocId();
	if (id < 0) {
		pdpFreePcb(pdp);
		adhocSockTryFree(sock);
		ret = 0x8041070b;
		goto out;
	}

	sock->id = id;
	sock->pcb = pdp;
	sock->chan_rd = (u32)&pdp->rx_head;
	sock->chan_wr = (u32)&pdp->state_flags;
	sock->chan_ex = 0;
	pdp->sock = sock;
	ret = id;

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocPdpSend @ 0x12ac === */
s32 sceNetAdhocPdpSend(s32 id, const u8 *dst_mac, u16 dport, const void *data, s32 len, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (_sceKernelCheckThreadStack() < 0x7c0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (dst_mac == NULL)
		return 0x80410701;
	if (dport == 0)
		return 0x80410702;
	if (len < 0 || len > 0xffff)
		return 0x80410705;
	if (len > 0 && data == NULL)
		return 0x80410706;
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

	sock = adhocSockFind(1, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = pdpSendInternal((PdpPcb *)sock->pcb, dst_mac, dport, data, len, timeout, nonblock);
	adhocSockTryFree(sock);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === sceNetAdhocPdpDelete @ 0x1470 === */
s32 sceNetAdhocPdpDelete(s32 id, s32 flags)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	PdpPcb *pdp;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (flags != 0)
		return 0x80410708;

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

	sock = adhocSockFind(1, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	pdp = (PdpPcb *)sock->pcb;
	sock->pcb = NULL;
	pdp->sock = NULL;
	pdpFreePcb(pdp);

	if (sock->wait_list != NULL) {
		if (sock->flags & 1)
			sceNet_lib_7574FDA1((void *)sock->chan_rd);
		if (sock->flags & 2)
			sceNet_lib_7574FDA1((void *)sock->chan_wr);
		sceNet_lib_7574FDA1(&g_global_wait_head);
	}
	adhocSockTryFree(sock);
	ret = 0;

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === pdpSendInternal @ 0x1620 === */
static s32 pdpSendInternal(PdpPcb *pdp, const u8 *dst_mac, u16 dport, const void *data, s32 len, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	AdhocIfnet *ifp;
	s32 frag_cnt;
	u16 seq;
	s32 cur_off;
	s32 rem_len;
	s32 ret;

	if ((dst_mac[0] & 1) && sceNet_lib_E0A81C7C(dst_mac, g_bcast_mac, 6) != 0)
		return 0x80410701;

	ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
	if (ifp == NULL)
		return 0x80410715;

	frag_cnt = (len + 0x54f) / 0x550;
	if (frag_cnt == 0)
		frag_cnt = 1;

	if (frag_cnt > ifp->snd_maxlen)
		return 0x80410705;

	while (ifp->snd_maxlen - ifp->snd_len < frag_cnt) {
		if (flags & 1)
			return 0x80410709;
		pdp->state_flags |= 1;
		ret = sceNet_lib_F6DB0A0B(&pdp->state_flags, &rem_timeout);
		if (pdp->sock == NULL)
			return 0x8041070c;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
		ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
		if (ifp == NULL)
			return 0x80410715;
	}

	seq = g_pdp_seq++;
	cur_off = 0;
	rem_len = len;

	do {
		s32 chunk = rem_len;
		u16 off_field;
		s32 tot_body;
		s32 pad_len;
		s32 alloc_len;
		AdhocMbuf *m;
		u8 *hdr;
		u16 tmp16;

		if (chunk > 0x550) {
			chunk = 0x550;
			off_field = (u16)(cur_off | 0x8000);
		} else {
			off_field = (u16)cur_off;
		}

		tot_body = chunk + 8;
		pad_len = tot_body & 0x0f;
		if (pad_len > 0)
			pad_len = 0x10 - pad_len;
		alloc_len = chunk + pad_len;

		m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (m == NULL) {
			g_pdp_stats[5]++;
			return 0x80410709;
		}
		m->m_next = NULL;
		m->m_nextpkt = NULL;
		m->m_data = (u8 *)m + 0x30;
		m->m_type = 1;
		m->m_flags = 2;
		m->m_pkthdr_Aux = 0;

		if (alloc_len + 0x38 >= 0xd1) {
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
				g_pdp_stats[5]++;
				return 0x80410709;
			}
		}

		hdr = m->m_data;
		sceKernelMemcpy(hdr, dst_mac, 6);
		sceKernelMemcpy(hdr + 6, pdp->mac, 6);
		sceKernelMemcpy(hdr + 0x0c, &g_adhoc_ethertype, 2);
		sceKernelMemcpy(hdr + 0x0e, &g_adhoc_hdr_ver, 2);

		hdr = m->m_data;
		hdr[0x30] = 1;
		hdr[0x31] = 0;
		tmp16 = bswap16((u16)tot_body);
		sceKernelMemcpy(hdr + 0x32, &tmp16, 2);
		tmp16 = bswap16(pdp->port);
		sceKernelMemcpy(hdr + 0x34, &tmp16, 2);
		tmp16 = bswap16(dport);
		sceKernelMemcpy(hdr + 0x36, &tmp16, 2);
		tmp16 = bswap16(seq);
		sceKernelMemcpy(hdr + 0x38, &tmp16, 2);
		tmp16 = bswap16(off_field);
		sceKernelMemcpy(hdr + 0x3a, &tmp16, 2);

		if (chunk > 0)
			sceKernelMemcpy(m->m_data + 0x3c, (const u8 *)data + cur_off, (u32)chunk);
		if (pad_len > 0)
			sceKernelMemset(m->m_data + 0x3c + chunk, 0, (u32)pad_len);

		ret = sceNetAdhocAuth_lib_AAB06250(m->m_data + 0x10, (u32)(alloc_len + 0x2c));
		if (ret < 0) {
			sceNetIfhandle_lib_F8825DC4(m);
			return ret;
		}

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
		g_pdp_stats[4]++;

		cur_off += chunk;
		rem_len -= chunk;
	} while (rem_len > 0);

	return 0;
}

/* === pdpRecvInternal @ 0x1c48 === */
static s32 pdpRecvInternal(PdpPcb *pdp, u8 *src_mac, u16 *sport, void *data, s32 *data_len, u32 timeout, s32 flags)
{
	u32 rem_timeout = timeout;
	AdhocMbuf *m;
	AdhocMbuf *cur;
	s32 pkt_len;
	u8 hdr_buf[8];
	s32 copy_off;
	s32 ret;

	for (;;) {
		m = pdp->rx_head;
		if (m != NULL)
			break;
		if (flags & 1)
			return 0x80410709;
		pdp->state_flags |= 2;
		ret = sceNet_lib_F6DB0A0B(&pdp->rx_head, &rem_timeout);
		if (pdp->sock == NULL)
			return 0x8041070c;
		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				return 0x80410715;
			if ((u32)ret == 0x800201a4)
				return 0x80410718;
			return ret;
		}
	}

	pkt_len = m->m_pkthdr_len - 8;
	if (*data_len < pkt_len) {
		*data_len = pkt_len;
		return 0x8041070a;
	}

	pdp->rx_head = m->m_nextpkt;
	if (pdp->rx_head == NULL)
		pdp->rx_tail = NULL;
	pdp->rx_cc -= (u32)pkt_len;
	m->m_nextpkt = NULL;

	if (m->m_len < 8) {
		sceNetIfhandle_lib_9A6261EC(m, 0, 8, hdr_buf);
		sceKernelMemcpy(src_mac, hdr_buf, 6);
		sceKernelMemcpy(sport, hdr_buf + 6, 2);
	} else {
		sceKernelMemcpy(src_mac, m->m_data, 6);
		sceKernelMemcpy(sport, m->m_data + 6, 2);
	}
	sceNetIfhandle_lib_BFF3CEA5(m, 8);

	if (flags & 2) {
		*(AdhocMbuf **)data = m;
		*data_len = pkt_len;
		return 0;
	}

	copy_off = 0;
	for (cur = m; cur != NULL; cur = cur->m_next) {
		if (cur->m_len > 0) {
			sceKernelMemcpy((u8 *)data + copy_off, cur->m_data, (u32)cur->m_len);
			copy_off += cur->m_len;
		}
	}
	*data_len = copy_off;
	sceNetIfhandle_lib_F56FAC82(m);
	return 0;
}

/* === sceNetAdhocPdpRecv @ 0x1f30 === */
s32 sceNetAdhocPdpRecv(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (src_mac == NULL || sport == NULL || buf == NULL || len == NULL)
		return 0x80410706;
	if (*len <= 0)
		return 0x80410705;
	if ((nonblock & ~1) != 0)
		return 0x80410708;

	return pdpRecvById(id, src_mac, sport, buf, len, timeout, nonblock);
}

/* === sceNetAdhoc_67346A2A @ 0x2004 === */
s32 sceNetAdhoc_67346A2A(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 nonblock)
{
	if ((nonblock & ~1) != 0)
		return 0x80410708;
	return pdpRecvById(id, src_mac, sport, buf, len, timeout, nonblock | 2);
}

/* === sceNetAdhoc_lib_F9240FBA @ 0x2060 === */
void sceNetAdhoc_lib_F9240FBA(void)
{
	sceKernelMemset(g_pdp_stats, 0, 0x18);
}

/* === sceNetAdhoc_lib_1059683E @ 0x2088 === */
s32 sceNetAdhoc_lib_1059683E(void *dst)
{
	if (dst == NULL)
		return 0x80410708;
	sceKernelMemcpy(dst, g_pdp_stats, 0x18);
	return 0;
}

/* === pdpRecvById @ 0x20c4 === */
static s32 pdpRecvById(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 flags)
{
	s32 intr;
	s32 ret;
	AdhocSocket *sock;
	AdhocWaiter *w = NULL;

	if (id <= 0)
		return 0x80410707;
	if (src_mac == NULL || sport == NULL || buf == NULL || len == NULL)
		return 0x80410706;
	if (*len <= 0)
		return 0x80410705;

	if ((flags & 1) == 0) {
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

	sock = adhocSockFind(1, id);
	if (sock == NULL || sock->pcb == NULL) {
		ret = 0x80410707;
		goto out;
	}

	ret = pdpRecvInternal((PdpPcb *)sock->pcb, src_mac, sport, buf, len, timeout, flags);
	adhocSockTryFree(sock);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === pdpFlushQueues @ 0x225c === */
static void pdpFlushQueues(PdpPcb *pdp)
{
	AdhocMbuf *m;
	PdpFragEntry *fe;

	while ((m = pdp->rx_head) != NULL) {
		pdp->rx_head = m->m_nextpkt;
		if (pdp->rx_head == NULL)
			pdp->rx_tail = NULL;
		m->m_nextpkt = NULL;
		pdp->rx_cc = (pdp->rx_cc - (u32)m->m_pkthdr_len) + 0x3c;
		sceNetIfhandle_lib_F56FAC82(m);
	}
	pdp->rx_cc = 0;

	while ((fe = pdp->frag_head) != NULL) {
		sceNet_lib_A93A93E9(fe->timer);
		sceNetIfhandle_lib_F56FAC82(fe->m_head);
		if (fe->next != NULL)
			fe->next->prev = fe->prev;
		else
			pdp->frag_tail = fe->prev;
		*fe->prev = fe->next;
		sceNetIfhandle_lib_76BAD213(fe);
	}
}

/* === pdpInputPacket @ 0x231c === */
void pdpInputPacket(AdhocMbuf *m)
{
	u8 dst_mac[6];
	u8 src_mac[6];
	u16 body_len;
	u16 sport;
	u16 dport;
	u16 seq;
	u16 off_field;
	u16 frag_off;
	s32 payload_len;
	PdpPcb *pdp;
	u8 *hdr;
	s32 is_bcast;

	if (m->m_pkthdr_len < 0x3c) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[0]++;
		return;
	}
	if (m->m_len < 0x3c) {
		m = (AdhocMbuf *)sceNetIfhandle_lib_49EDBB18(m, 0x3c);
		if (m == NULL) {
			g_pdp_stats[0]++;
			return;
		}
	}

	hdr = m->m_data;
	if (hdr[0x31] != 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[0]++;
		return;
	}

	sceKernelMemcpy(&body_len, hdr + 0x32, 2);
	body_len = bswap16(body_len);
	if (body_len < 8 || m->m_pkthdr_len < (s32)body_len + 0x34) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[0]++;
		return;
	}

	payload_len = (s32)body_len - 8;
	sceKernelMemcpy(dst_mac, hdr, 6);
	sceKernelMemcpy(src_mac, hdr + 6, 6);
	sceKernelMemcpy(&sport, hdr + 0x34, 2);
	sport = bswap16(sport);
	sceKernelMemcpy(&dport, hdr + 0x36, 2);
	dport = bswap16(dport);
	sceKernelMemcpy(&seq, hdr + 0x38, 2);
	seq = bswap16(seq);
	sceKernelMemcpy(&off_field, hdr + 0x3a, 2);
	off_field = bswap16(off_field);

	if (sport == 0 || dport == 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[0]++;
		return;
	}

	is_bcast = (sceNet_lib_E0A81C7C(dst_mac, g_bcast_mac, 6) == 0);
	for (pdp = g_pdp_list.head; pdp != NULL; pdp = pdp->next) {
		if (pdp->port == dport) {
			if (is_bcast || sceNet_lib_E0A81C7C(pdp->mac, dst_mac, 6) == 0)
				break;
		}
	}
	if (pdp == NULL) {
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[1]++;
		return;
	}

	if (sceNetAdhocAuth_lib_015A8A64(m->m_data + 0x10, (u32)(m->m_pkthdr_len - 0x10)) < 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		return;
	}

	{
		s32 pad_trim = (payload_len + 0x3c) - m->m_pkthdr_len;
		if (pad_trim < 0)
			sceNetIfhandle_lib_BFF3CEA5(m, pad_trim);
	}

	frag_off = (u16)(off_field & 0x7fff);
	if (frag_off == 0 && !(off_field & 0x8000)) {
		PdpFragEntry *fe;
		PdpFragEntry *next_fe;

		for (fe = pdp->frag_head; fe != NULL; fe = next_fe) {
			next_fe = fe->next;
			if (sceNet_lib_E0A81C7C(fe->src_mac, src_mac, 6) == 0 && fe->sport == sport) {
				if (fe->next != NULL)
					fe->next->prev = fe->prev;
				else
					pdp->frag_tail = fe->prev;
				*fe->prev = fe->next;
				pdp->frag_count--;
				sceNetIfhandle_lib_F56FAC82(fe->m_head);
				sceNetIfhandle_lib_76BAD213(fe);
			}
		}

		sceNetIfhandle_lib_BFF3CEA5(m, 0x34);
		sceKernelMemcpy(m->m_data, src_mac, 6);
		sceKernelMemcpy(m->m_data + 6, &sport, 2);
	} else {
		PdpFragEntry *fe;

		sceNetIfhandle_lib_BFF3CEA5(m, 0x3c);
		for (fe = pdp->frag_head; fe != NULL; fe = fe->next) {
			if (sceNet_lib_E0A81C7C(fe->src_mac, src_mac, 6) == 0 && fe->sport == sport)
				break;
		}

		if (frag_off == 0) {
			AdhocMbuf *hdr_m;

			if (fe != NULL) {
				if (fe->next != NULL)
					fe->next->prev = fe->prev;
				else
					pdp->frag_tail = fe->prev;
				*fe->prev = fe->next;
				pdp->frag_count--;
				sceNetIfhandle_lib_F56FAC82(fe->m_head);
				sceNetIfhandle_lib_76BAD213(fe);
			}

			if (pdp->frag_count >= 4) {
				PdpFragEntry *old = pdp->frag_head;
				if (old != NULL) {
					if (old->next != NULL)
						old->next->prev = old->prev;
					else
						pdp->frag_tail = old->prev;
					*old->prev = old->next;
					pdp->frag_count--;
					sceNetIfhandle_lib_F56FAC82(old->m_head);
					sceNetIfhandle_lib_76BAD213(old);
				}
			}

			fe = (PdpFragEntry *)sceNetIfhandle_lib_15CFE3C0(0x34);
			if (fe == NULL) {
				sceNetIfhandle_lib_F56FAC82(m);
				g_pdp_stats[2]++;
				return;
			}
			sceKernelMemcpy(fe->src_mac, src_mac, 6);
			fe->sport = sport;
			fe->seq = seq;
			fe->cur_off = (u16)payload_len;

			hdr_m = (AdhocMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
			if (hdr_m == NULL) {
				sceNetIfhandle_lib_76BAD213(fe);
				sceNetIfhandle_lib_F56FAC82(m);
				g_pdp_stats[2]++;
				return;
			}
			hdr_m->m_next = m;
			hdr_m->m_nextpkt = NULL;
			hdr_m->m_data = (u8 *)hdr_m + 0x30;
			hdr_m->m_len = 8;
			hdr_m->m_type = 1;
			hdr_m->m_flags = 2;
			hdr_m->m_rcvif = NULL;
			hdr_m->m_pkthdr_len = payload_len + 8;
			hdr_m->m_pkthdr_Aux = 0;
			sceKernelMemcpy(hdr_m->m_data, src_mac, 6);
			sceKernelMemcpy(hdr_m->m_data + 6, &sport, 2);

			fe->m_head = hdr_m;
			fe->next = NULL;
			fe->prev = pdp->frag_tail;
			*pdp->frag_tail = fe;
			pdp->frag_tail = &fe->next;
			pdp->frag_count++;
			return;
		} else {
			if (fe == NULL || fe->seq != seq || fe->cur_off != frag_off ||
			    (s32)fe->cur_off + payload_len > 0xffff) {
				if (fe != NULL) {
					if (fe->next != NULL)
						fe->next->prev = fe->prev;
					else
						pdp->frag_tail = fe->prev;
					*fe->prev = fe->next;
					pdp->frag_count--;
					sceNetIfhandle_lib_F56FAC82(fe->m_head);
					sceNetIfhandle_lib_76BAD213(fe);
				}
				sceNetIfhandle_lib_F56FAC82(m);
				g_pdp_stats[0]++;
				return;
			}

			sceNetIfhandle_lib_3E8DD3F8(fe->m_head, m);
			fe->m_head->m_pkthdr_len += payload_len;
			fe->cur_off = (u16)(fe->cur_off + payload_len);

			if (off_field & 0x8000)
				return;

			m = fe->m_head;
			fe->m_head = NULL;
			if (fe->next != NULL)
				fe->next->prev = fe->prev;
			else
				pdp->frag_tail = fe->prev;
			*fe->prev = fe->next;
			pdp->frag_count--;
			sceNetIfhandle_lib_76BAD213(fe);
			payload_len = m->m_pkthdr_len - 8;
		}
	}

	if (pdp->rx_hiwat - pdp->rx_cc < (u32)payload_len) {
		pdp->rx_drops++;
		sceNetIfhandle_lib_F56FAC82(m);
		g_pdp_stats[2]++;
		return;
	}

	m->m_nextpkt = NULL;
	if (pdp->rx_tail == NULL)
		pdp->rx_head = m;
	else
		pdp->rx_tail->m_nextpkt = m;
	pdp->rx_tail = m;
	pdp->rx_cc += (u32)payload_len;

	if (pdp->state_flags & 2) {
		pdp->state_flags &= ~2;
		sceNet_lib_7574FDA1(&pdp->rx_head);
	}
	if (pdp->sock != NULL && (pdp->sock->flags & 1)) {
		pdp->sock->flags &= ~1;
		sceNet_lib_7574FDA1(&g_global_wait_head);
	}
	g_pdp_stats[3]++;
}

/* === sceNetAdhocGetPdpStat @ 0x2850 === */
s32 sceNetAdhocGetPdpStat(s32 *buflen, void *buf)
{
	s32 intr;
	s32 ret;
	PdpPcb *pdp;
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
		for (pdp = g_pdp_list.head; pdp != NULL; pdp = pdp->next)
			total_bytes += 0x14;
		*buflen = total_bytes;
		ret = 0;
	} else {
		s32 max_len = *buflen;
		out = (u8 *)buf;
		total_bytes = 0;
		for (pdp = g_pdp_list.head; pdp != NULL; pdp = pdp->next) {
			if (total_bytes + 0x14 > max_len)
				break;
			((ScePdpStatOut *)out)->next = NULL;
			((ScePdpStatOut *)out)->pdp_id = (pdp->sock != NULL) ? pdp->sock->id : -1;
			sceKernelMemcpy(out + 0x08, pdp->mac, 6);
			((ScePdpStatOut *)out)->port = pdp->port;
			((ScePdpStatOut *)out)->rcvd_data = pdp->rx_cc;
			if (total_bytes >= 0x14)
				*(void **)(out - 0x14) = out;
			out += 0x14;
			total_bytes += 0x14;
		}
		*buflen = total_bytes;
		ret = 0;
	}

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === pdpInit @ 0x29ac === */
void pdpInit(void)
{
	g_pdp_list.head = NULL;
	g_pdp_list.tail = &g_pdp_list.head;
	g_pdp_anon_port = (u16)((sceNet_lib_1858883D() % 0xc000) + 0x4000);
	sceKernelMemset(g_pdp_stats, 0, 0x18);
	g_pdp_seq = (u16)(sceNet_lib_1858883D() / 0x8001);
}

/* === pdpTerm @ 0x2a18 === */
void pdpTerm(void)
{
	PdpPcb *pdp;

	while ((pdp = g_pdp_list.head) != NULL) {
		if (pdp->sock != NULL) {
			pdp->sock->pcb = NULL;
			adhocSockTryFree(pdp->sock);
			pdp->sock = NULL;
		}
		pdpFreePcb(pdp);
	}
}

/* === pdpAllocPcb @ 0x2a9c === */
static s32 pdpAllocPcb(PdpPcb **out_pdp)
{
	PdpPcb *pdp;

	pdp = (PdpPcb *)sceNetIfhandle_lib_15CFE3C0(0x3c);
	if (pdp == NULL)
		return 0x80410709;
	sceKernelMemset(pdp, 0, 0x3c);
	pdp->frag_head = NULL;
	pdp->frag_tail = &pdp->frag_head;
	pdp->next = NULL;
	pdp->prev = g_pdp_list.tail;
	*g_pdp_list.tail = pdp;
	g_pdp_list.tail = &pdp->next;
	*out_pdp = pdp;
	return 0;
}

/* === pdpBindPcb @ 0x2b2c === */
static s32 pdpBindPcb(PdpPcb *pdp, const u8 *mac, u16 port, u32 bufsize)
{
	u8 local_mac[8];
	PdpPcb *cur;
	s32 ret;

	ret = sceNetGetLocalEtherAddr(local_mac);
	if (ret < 0)
		return ret;
	if (sceNet_lib_E0A81C7C(mac, local_mac, 6) != 0)
		return 0x80410701;

	if (port == 0) {
		port = pdpAllocAnonPort();
		if (port == 0)
			return 0x8041070b;
	} else {
		for (cur = g_pdp_list.head; cur != NULL; cur = cur->next) {
			if (cur != pdp && cur->port == port &&
			    sceNet_lib_E0A81C7C(cur->mac, mac, 6) == 0)
				return 0x80410703;
		}
	}

	sceKernelMemcpy(pdp->mac, mac, 6);
	pdp->port = port;
	pdp->rx_hiwat = bufsize;
	return 0;
}

/* === pdpFreePcb @ 0x2d04 === */
static void pdpFreePcb(PdpPcb *pdp)
{
	pdpFlushQueues(pdp);
	if (pdp->next != NULL)
		pdp->next->prev = pdp->prev;
	else
		g_pdp_list.tail = pdp->prev;
	*pdp->prev = pdp->next;
	sceNetIfhandle_lib_76BAD213(pdp);
}

/* === pdpHasActive @ 0x2d50 === */
s32 pdpHasActive(void)
{
	return (g_pdp_list.head != NULL) ? 1 : 0;
}

/* === pdpIfDetach @ 0x2d5c === */
void pdpIfDetach(void *ifp_arg)
{
	AdhocIfnet *ifp = (AdhocIfnet *)ifp_arg;
	PdpPcb *pdp;

	for (pdp = g_pdp_list.head; pdp != NULL; pdp = pdp->next) {
		if (pdp->state_flags & 1) {
			pdp->state_flags &= ~1;
			sceNet_lib_7574FDA1(&pdp->state_flags);
		}
		if (pdp->sock != NULL && (pdp->sock->flags & 2)) {
			if (ifp->snd_len < ifp->snd_maxlen) {
				pdp->sock->flags &= ~2;
				sceNet_lib_7574FDA1(&g_global_wait_head);
			}
		}
	}
}

/* === pdpPollEvents @ 0x2dc4 === */
s32 pdpPollEvents(AdhocSocket *sock, s32 events)
{
	PdpPcb *pdp = (PdpPcb *)sock->pcb;
	s32 rev = 0;

	if (pdp == NULL)
		return 0x10;

	if (events & 1) {
		AdhocIfnet *ifp = (AdhocIfnet *)sceNet_lib_D60225A3();
		if (ifp == NULL)
			return 0x100;
		if (ifp->snd_len < ifp->snd_maxlen)
			rev |= 1;
	}
	if (events & 2) {
		if (pdp->rx_head != NULL)
			rev |= 2;
	}
	return rev;
}

/* === pdpAllocAnonPort @ 0x2e78 === */
static u16 pdpAllocAnonPort(void)
{
	u16 cand = g_pdp_anon_port;
	s32 count = 0;
	PdpPcb *cur;

	while (count < 0x4000) {
		s32 used = 0;
		for (cur = g_pdp_list.head; cur != NULL; cur = cur->next) {
			if (cur->port == cand) {
				used = 1;
				break;
			}
		}
		cand++;
		if (cand < 0xc000)
			cand = 0xc000;
		if (!used) {
			g_pdp_anon_port = cand;
			return (u16)(cand == 0xc000 ? 0xffff : cand - 1);
		}
		count++;
	}
	return 0;
}


s32 sceNetAdhoc_lib_67346A2A(s32 id, u8 *src_mac, u16 *sport, void *buf, s32 *len, u32 timeout, s32 nonblock)
	__attribute__((alias("sceNetAdhoc_67346A2A")));
