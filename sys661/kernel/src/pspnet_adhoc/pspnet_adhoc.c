#include "adhoc_int.h"

const u16 g_adhoc_ethertype = 0xC888;
const u16 g_adhoc_hdr_ver = 0x0200;
const u8 g_bcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const u8 g_zero_mac[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
s32 g_adhoc_inited;
u32 g_global_wait_head;
static s32 g_last_sock_id;
static struct {
	AdhocMbuf *head;
	AdhocMbuf *tail;
	s32 count;
	s32 max_count;
	s32 drops;
} g_rx_queue;
static struct {
	AdhocSocket *head;
	AdhocSocket **tail;
} g_sock_list;

static void adhocRxSoftIntr(void);
static s32 adhocGetRxQueue(s32 a0, s32 a1, void **out);
static void adhocIfDetachNotify(void *ifp);
static s32 adhocPollSockets(void *arr, s32 count);
static void adhocSockInit(void);
static void adhocSockTerm(void);
static s32 adhocSockHasActive(void);
static void adhocSockWakeWaiters(AdhocSocket *sock, s32 flags);

/* === sceNetAdhocInit @ 0x0000 === */
s32 sceNetAdhocInit(void)
{
	s32 intr;
	s32 ret;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited > 0) {
		ret = 0x80410713;
		goto out;
	}

	g_rx_queue.head = NULL;
	g_rx_queue.tail = NULL;
	g_rx_queue.count = 0;
	g_rx_queue.max_count = 0x32;
	g_rx_queue.drops = 0;

	adhocSockInit();
	pdpInit();
	ptpInit();

	ret = sceNet_lib_B3A48B7F(adhocRxSoftIntr, adhocIfDetachNotify);
	if (ret >= 0) {
		ret = sceNet_lib_1F94AFD9(adhocGetRxQueue);
		if (ret >= 0) {
			ret = gameModeInit();
			if (ret >= 0) {
				g_adhoc_inited = 1;
				ret = 0;
				goto out;
			}
			sceNet_lib_0A5A8751();
		}
		sceNet_lib_949F1FBB();
	}
	ptpTerm();
	pdpTerm();
	adhocSockTerm();

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhocTerm @ 0x0148 === */
s32 sceNetAdhocTerm(void)
{
	s32 intr;
	s32 ret;
	AdhocMbuf *m;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	ret = 0;
	if (g_adhoc_inited != 0) {
		if (adhocSockHasActive() != 0 || pdpHasActive() != 0 || ptpHasActive() != 0) {
			ret = 0x80410714;
		} else {
			gameModeTerm();
			sceNet_lib_0A5A8751();
			sceNet_lib_949F1FBB();
			ptpTerm();
			pdpTerm();
			adhocSockTerm();
			for (;;) {
				m = g_rx_queue.head;
				if (m == NULL)
					break;
				g_rx_queue.head = m->m_nextpkt;
				if (g_rx_queue.head == NULL)
					g_rx_queue.tail = NULL;
				g_rx_queue.count--;
				m->m_nextpkt = NULL;
				sceNetIfhandle_lib_F56FAC82(m);
			}
			g_adhoc_inited = 0;
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === adhocRxSoftIntr @ 0x0270 === */
static void adhocRxSoftIntr(void)
{
	AdhocMbuf *m;
	u8 proto;

	for (;;) {
		m = g_rx_queue.head;
		if (m == NULL)
			return;
		g_rx_queue.head = m->m_nextpkt;
		if (g_rx_queue.head == NULL)
			g_rx_queue.tail = NULL;
		g_rx_queue.count--;
		m->m_nextpkt = NULL;

		if (m->m_pkthdr_len < 0x31) {
			sceNetIfhandle_lib_F56FAC82(m);
			continue;
		}
		if (m->m_len < 0x31) {
			sceNetIfhandle_lib_9A6261EC(m, 0x30, 1, &proto);
		} else {
			proto = m->m_data[0x30];
		}
		if (proto == 1) {
			pdpInputPacket(m);
		} else if (proto == 2) {
			ptpInputPacket(m);
		} else {
			sceNetIfhandle_lib_F56FAC82(m);
		}
	}
}

/* === adhocIfOutput @ 0x036c === */
s32 adhocIfOutput(void *ifp_arg, void *m_arg)
{
	AdhocIfnet *ifp = (AdhocIfnet *)ifp_arg;
	AdhocMbuf *m = (AdhocMbuf *)m_arg;
	u8 *sadl;

	sadl = ifp->if_sadl;
	if (sceNet_lib_E0A81C7C(m->m_data, sadl + sadl[5] + 8, 6) == 0) {
		m->m_rcvif = ifp;
		if (g_rx_queue.count >= g_rx_queue.max_count) {
			g_rx_queue.drops++;
			sceNetIfhandle_lib_F56FAC82(m);
			return 0;
		}
		m->m_nextpkt = NULL;
		if (g_rx_queue.tail == NULL)
			g_rx_queue.head = m;
		else
			g_rx_queue.tail->m_nextpkt = m;
		g_rx_queue.tail = m;
		g_rx_queue.count++;
		sceNet_lib_05D525E4(0x1d);
		return 0;
	}

	m->m_flags |= 0x8000;
	m->m_nextpkt = NULL;
	if (ifp->snd_tail == NULL)
		ifp->snd_head = m;
	else
		ifp->snd_tail->m_nextpkt = m;
	ifp->snd_tail = m;
	ifp->snd_len++;
	if (!(ifp->if_flags & 0x400))
		ifp->if_start(ifp);
	return 0;
}

/* === adhocGetRxQueue @ 0x047c === */
static s32 adhocGetRxQueue(s32 a0, s32 a1, void **out)
{
	(void)a0;
	(void)a1;
	*out = &g_rx_queue;
	return 0;
}

/* === adhocIfDetachNotify @ 0x0490 === */
static void adhocIfDetachNotify(void *ifp)
{
	pdpIfDetach(ifp);
	ptpIfDetach(ifp);
	ptpIfDetachNoop(ifp);
}

/* === sceNetAdhoc_7A662D6B @ 0x04ac === */
s32 sceNetAdhoc_7A662D6B(SceNetAdhocPollSd *sds, s32 count, u32 timeout, s32 nonblock)
{
	s32 intr;
	s32 ret;
	u32 rem_timeout;
	void **arr;
	s32 i, j;
	AdhocSocket *s;
	AdhocWaiter *w = NULL;

	rem_timeout = timeout;
	if (_sceKernelCheckThreadStack() < 0x7b0)
		return 0x80410005;
	if (sds == NULL)
		return 0x80410708;
	if (count <= 0)
		return 0x80410710;

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

	arr = (void **)sceNetIfhandle_lib_15CFE3C0((u32)(count << 3));
	if (arr == NULL) {
		ret = 0x80410709;
		goto out;
	}

	for (i = 0; i < count; i++) {
		s = adhocSockFind(0, sds[i].id);
		if (s == NULL) {
			ret = 0x80410707;
			goto free_arr;
		}
		for (j = 0; j < i; j++) {
			if ((AdhocSocket *)arr[j * 2] == s) {
				ret = 0x80410708;
				goto free_arr;
			}
		}
		arr[i * 2] = s;
		arr[i * 2 + 1] = &sds[i];
	}

	for (;;) {
		ret = adhocPollSockets(arr, count);
		if (ret > 0 || nonblock != 0)
			break;

		for (i = 0; i < count; i++) {
			s = (AdhocSocket *)arr[i * 2];
			s32 ev = ((SceNetAdhocPollSd *)arr[i * 2 + 1])->events;
			if (s->type == 1) {
				if (ev & 1)
					s->flags |= 2;
				if (ev & 2)
					s->flags |= 1;
			} else if (s->type == 2) {
				PtpPcb *ptp = (PtpPcb *)s->pcb;
				if (ptp != NULL) {
					if (ptp->mode == 1) {
						if (ev & 4)
							ptp->flags |= 4;
						if (ev & 1)
							ptp->tx_flags |= 2;
						if (ev & 2)
							ptp->rx_flags |= 2;
					} else if (ptp->mode == 2) {
						if (ev & 8)
							ptp->q_flags |= 2;
					}
				}
			}
		}

		s = (AdhocSocket *)arr[0];
		for (i = 0; i < count; i++) {
			AdhocSocket *cur = (AdhocSocket *)arr[i * 2];
			cur->flags &= ~0x80;
		}

		ret = sceNet_lib_F6DB0A0B(&g_global_wait_head, &rem_timeout);

		for (i = 0; i < count; i++) {
			s = (AdhocSocket *)arr[i * 2];
			if (s->type == 1) {
				s->flags &= ~3;
			} else if (s->type == 2) {
				PtpPcb *ptp = (PtpPcb *)s->pcb;
				if (ptp != NULL) {
					if (ptp->mode == 1) {
						ptp->flags &= ~4;
						ptp->tx_flags &= ~2;
						ptp->rx_flags &= ~2;
					} else if (ptp->mode == 2) {
						ptp->q_flags &= ~2;
					}
				}
			}
		}

		for (i = 0; i < count; i++) {
			s = (AdhocSocket *)arr[i * 2];
			if (s->flags & 0x80) {
				s->flags &= ~0x80;
				ret = 0x8041070c;
				goto cleanup_socks;
			}
		}

		if (ret < 0) {
			if ((u32)ret == 0x80410186)
				ret = 0;
			else if ((u32)ret == 0x800201a4)
				ret = 0x80410718;
			break;
		}
	}

cleanup_socks:
	for (i = 0; i < count; i++) {
		s = (AdhocSocket *)arr[i * 2];
		adhocSockTryFree(s);
	}

free_arr:
	sceNetIfhandle_lib_76BAD213(arr);

out:
	sceNet_lib_515B2F33(intr);
	if (w != NULL)
		sceNet_lib_757085B0(w, 0);
	return ret;
}

/* === adhocPollSockets @ 0x0974 === */
static s32 adhocPollSockets(void *arr_arg, s32 count)
{
	void **arr = (void **)arr_arg;
	s32 ready = 0;
	s32 i;

	for (i = 0; i < count; i++) {
		AdhocSocket *s = (AdhocSocket *)arr[i * 2];
		SceNetAdhocPollSd *sd = (SceNetAdhocPollSd *)arr[i * 2 + 1];
		s32 rev = 0;

		if (s->type == 1) {
			rev = pdpPollEvents(s, sd->events);
		} else if (s->type == 2) {
			rev = ptpPollEvents(s, sd->events);
		}
		sd->revents = rev;
		if (rev != 0)
			ready++;
	}
	return ready;
}

/* === sceNetAdhoc_73BFD52D @ 0x0a2c === */
s32 sceNetAdhoc_73BFD52D(s32 id, s32 flag)
{
	s32 intr;
	s32 ret;
	AdhocSocket *s;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if ((flag & ~0x1f) != 0)
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

	if (flag & 0x10) {
		for (s = g_sock_list.head; s != NULL; s = s->next) {
			if (s->pcb != NULL) {
				s->flags = (s->flags & ~0xff) | (u32)(flag & 0xff);
				if (flag != 0)
					sceNet_lib_6B294EE4(s, flag);
			}
		}
		ret = 0;
	} else {
		s = adhocSockFind(0, id);
		if (s == NULL) {
			ret = 0x80410707;
		} else {
			s->flags = (s->flags & ~0xff) | (u32)(flag & 0xff);
			if (flag != 0)
				sceNet_lib_6B294EE4(s, flag);
			ret = 0;
		}
	}

out:
	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === sceNetAdhoc_4D2CE199 @ 0x0c58 === */
s32 sceNetAdhoc_4D2CE199(s32 id, s32 *flag)
{
	s32 intr;
	s32 ret;
	AdhocSocket *s;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (id <= 0)
		return 0x80410707;
	if (flag == NULL)
		return 0x80410708;

	intr = sceNet_lib_94B44F26();
	if (intr < 0)
		return intr;

	if (g_adhoc_inited == 0) {
		ret = 0x80410712;
	} else if (sceWlanDevIsGameMode() != 0) {
		ret = 0x8041071b;
	} else {
		s = adhocSockFind(0, id);
		if (s == NULL) {
			ret = 0x80410707;
		} else {
			*flag = (s32)(s->flags & 0xff);
			ret = 0;
		}
	}

	sceNet_lib_515B2F33(intr);
	return ret;
}

/* === adhocSockInit @ 0x0dd0 === */
static void adhocSockInit(void)
{
	g_sock_list.head = NULL;
	g_sock_list.tail = &g_sock_list.head;
	sceNet_lib_2E005032(adhocSockWakeWaiters);
	g_last_sock_id = 0;
}

/* === adhocSockTerm @ 0x0e18 === */
static void adhocSockTerm(void)
{
	AdhocSocket *s;
	AdhocSocket *next;

	sceNet_lib_33B230BD();
	for (s = g_sock_list.head; s != NULL; s = next) {
		next = s->next;
		if (s->pcb == NULL) {
			if (s->next != NULL)
				s->next->prev = s->prev;
			else
				g_sock_list.tail = s->prev;
			*s->prev = s->next;
			sceNetIfhandle_lib_76BAD213(s);
		}
	}
}

/* === adhocSockFind @ 0x0e90 === */
AdhocSocket *adhocSockFind(s32 type, s32 id)
{
	AdhocSocket *s;

	for (s = g_sock_list.head; s != NULL; s = s->next) {
		if (s->id == id) {
			if (type == 0 || s->type == type)
				return s;
		}
	}
	return NULL;
}

/* === adhocSockAllocId @ 0x0ed8 === */
s32 adhocSockAllocId(void)
{
	s32 cand = g_last_sock_id + 1;
	s32 count = 0;

	if (cand >= 0xff)
		cand = 1;

	while (count < 0xfe) {
		if (adhocSockFind(0, cand) == NULL) {
			g_last_sock_id = cand;
			return cand;
		}
		cand++;
		if (cand >= 0xff)
			cand = 1;
		count++;
	}
	return -1;
}

/* === adhocSockCreate @ 0x0f60 === */
s32 adhocSockCreate(s32 type, AdhocSocket **out_sock)
{
	AdhocSocket *s;

	s = (AdhocSocket *)sceNetIfhandle_lib_15CFE3C0(0x28);
	if (s == NULL)
		return 0x80410709;
	sceKernelMemset(s, 0, 0x28);
	s->id = -1;
	s->type = type;
	s->next = NULL;
	s->prev = g_sock_list.tail;
	*g_sock_list.tail = s;
	g_sock_list.tail = &s->next;
	*out_sock = s;
	return 0;
}

/* === adhocSockTryFree @ 0x0ffc === */
void adhocSockTryFree(AdhocSocket *sock)
{
	if (sock->pcb == NULL && sock->wait_list == NULL) {
		if (sock->next != NULL)
			sock->next->prev = sock->prev;
		else
			g_sock_list.tail = sock->prev;
		*sock->prev = sock->next;
		sceNetIfhandle_lib_76BAD213(sock);
	}
}

/* === adhocSockHasActive @ 0x1044 === */
static s32 adhocSockHasActive(void)
{
	AdhocSocket *s;

	for (s = g_sock_list.head; s != NULL; s = s->next) {
		if (s->wait_list != NULL)
			return 1;
	}
	return 0;
}

/* === adhocSockWakeWaiters @ 0x1078 === */
static void adhocSockWakeWaiters(AdhocSocket *sock, s32 flags)
{
	AdhocWaiter *w;
	u32 chan;

	if (sock->wait_list == NULL)
		return;

	for (w = sock->wait_list; w != NULL; w = w->next) {
		chan = w->wait_chan;
		if (((flags & 1) && chan == sock->chan_rd) ||
		    ((flags & 2) && chan == sock->chan_wr) ||
		    ((flags & 4) && chan == sock->chan_ex) ||
		    ((flags & 8) && chan == (u32)&g_global_wait_head)) {
			if (chan == (u32)&g_global_wait_head)
				sock->flags |= 0x80;
			sceKernelSignalSema(w->sema_id, 1);
		}
	}
}

