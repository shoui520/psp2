#include "ifhandle_int.h"

static const uint8_t g_bcast_mac[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static const GameWorkaround g_game_wa_table[8] = {
	{ "ULKS46138", 3, { 0x0007a120, 0, 0, 0, 0 } },
	{ "UCAS40142", 3, { 0x0007a120, 0, 0, 0, 0 } },
	{ "ULJM05217", 3, { 0x0007a120, 0, 0, 0, 0 } },
	{ "ULJS00007", 3, { 0, 3, 0, 0, 0 } },
	{ "ULES00647", 3, { 0, 0, 0x2710, 0, 0 } },
	{ "ULUS10217", 3, { 0, 0, 0x2710, 0, 0 } },
	{ "ULES00776", 3, { 0, 0, 0, 0x007a1200, 0 } },
	{ "ULJM05696", 3, { 0x0000c350, 0, 0, 0, 0x000186a0 } },
};


/* .bss */
static int g_net_evf_id;
static SceNetIf *g_attach_if;
static int g_attach_sema;
static SceNetIf *g_detach_if;
static int g_detach_sema;
static int g_eapol_enabled;
static int (*g_eapol_cb1)(SceNetMbuf *m);
static int (*g_eapol_cb2)(SceNetMbuf *m);
static int (*g_eapol_cb3)(SceNetMbuf *m);
static uint32_t g_drop_counter;
static SceNetIfHandle *g_if_table[8];
static SceNetMbufQueue g_rx_queue;
static const GameWorkaround *g_cur_game_wa;


void sceNetIfhandle_lib_B9096E48(void)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	while (g_rx_queue.head != NULL) {
		SceNetMbuf *m = g_rx_queue.head;
		SceNetMbuf *next = m->m_nextpkt;
		g_rx_queue.head = next;
		if (next == NULL) {
			g_rx_queue.tail = NULL;
		}
		m->m_nextpkt = NULL;
		g_rx_queue.count--;
		sceNetIfhandle_lib_F56FAC82(m);
	}

	for (int i = 0; i < 8; i++) {
		g_if_table[i] = NULL;
	}

	g_net_evf_id = 0;
	sceNetIfhandle_lib_0B258B5E();
	pspSetK1(old_k1);
}

void sceNetIfhandle_lib_B1F5BB87(SceNetIf *ifp, SceNetMbuf *m)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (ifp == NULL || ((int32_t)(k1_shifted & (uint32_t)ifp) < 0) ||
	    m == NULL || ((int32_t)m < 0)) {
		pspSetK1(old_k1);
		return;
	}

	SceNetIfHandle *ifh = NULL;
	int i;
	for (i = 0; i < 8; i++) {
		if (g_if_table[i] != NULL && g_if_table[i]->ifp == ifp) {
			ifh = g_if_table[i];
			break;
		}
	}
	if (i == 8 || m == NULL) {
		pspSetK1(old_k1);
		return;
	}

	uint32_t drop_mod = sceNetIfhandle_lib_07505747(1);
	if (drop_mod != 0 && (int16_t)m->m_flags < 0) {
		uint32_t cur = g_drop_counter;
		g_drop_counter = cur + 1;
		if ((cur % drop_mod) != 0) {
			sceNetIfhandle_lib_F56FAC82(m);
			pspSetK1(old_k1);
			return;
		}
	}

	int intr = sceKernelCpuSuspendIntr();
	m->m_nextpkt = NULL;
	if (ifh->tx_tail != NULL) {
		ifh->tx_tail->m_nextpkt = m;
	} else {
		ifh->tx_head = m;
	}
	ifh->tx_tail = m;
	ifh->tx_count++;
	sceKernelCpuResumeIntr(intr);

	ifh->start_tx(ifh);
	pspSetK1(old_k1);
}

void sceNetIfhandle_lib_0296C7D6(SceNetIf *ifp, uint32_t cmd, void *arg)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (ifp == NULL || ((int32_t)(k1_shifted & (uint32_t)ifp) < 0) ||
	    arg == NULL || ((int32_t)(k1_shifted & (uint32_t)arg) < 0)) {
		pspSetK1(old_k1);
		return;
	}

	SceNetIfHandle *ifh = NULL;
	int i;
	for (i = 0; i < 8; i++) {
		if (g_if_table[i] != NULL && g_if_table[i]->ifp == ifp) {
			ifh = g_if_table[i];
			break;
		}
	}
	if (i == 8) {
		pspSetK1(old_k1);
		return;
	}

	void *ptrs[2] = { NULL, NULL };
	void **uarg = (void **)arg;

	switch (cmd) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 56:
	case 57:
	case 58:
	case 59:
	case 60:
	case 61:
	case 66:
	case 67:
	case 69:
	case 70:
	case 100:
		break;
	case 50:
	case 51:
	case 53:
	case 54:
	case 55:
	case 64:
	case 65:
	case 71:
		ptrs[0] = uarg[4];
		break;
	case 52:
		ptrs[0] = uarg[4];
		ptrs[1] = uarg[6];
		break;
	case 62:
	case 63:
		ptrs[0] = uarg[6];
		break;
	case 68:
		ptrs[0] = uarg[4];
		ptrs[1] = uarg[5];
		break;
	default:
		arg = NULL;
		break;
	}

	if (ptrs[0] != NULL && ((int32_t)(k1_shifted & (uint32_t)ptrs[0]) < 0)) {
		pspSetK1(old_k1);
		return;
	}
	if (ptrs[1] != NULL && ((int32_t)(k1_shifted & (uint32_t)ptrs[1]) < 0)) {
		pspSetK1(old_k1);
		return;
	}

	ifh->ioctl(ifh, cmd, arg, ptrs);
	pspSetK1(old_k1);
}

void sceNetIfhandle_driver_263767F6(SceNetIfHandle *ifh, int type, int arg)
{
	if (ifh == NULL) {
		return;
	}

	SceNetIf *ifp = ifh->ifp;
	SceNetIfEvent *ev = NULL;
	int intr = sceKernelCpuSuspendIntr();
	if (ifp->evf_id != 0 && ifp->evf_bits != 0) {
		ev = (SceNetIfEvent *)sceNetIfhandle_lib_15CFE3C0(sizeof(SceNetIfEvent));
		if (ev != NULL) {
			SceNetIfEvent *tail = ifp->ev_tail;
			ev->type = type;
			ev->prev = tail;
			ev->next = NULL;
			ev->arg = arg;
			tail->next = ev;
			ifp->ev_tail = ev;
		}
	}
	sceKernelCpuResumeIntr(intr);

	if (ev != NULL) {
		sceKernelSetEventFlag(ifp->evf_id, ifp->evf_bits);
	}
}

SceNetMbuf *sceNetIfhandle_driver_E2F4F1C9(SceNetIfHandle *ifh)
{
	if (ifh == NULL) {
		return NULL;
	}

	SceNetMbuf *m;
	for (;;) {
		int intr = sceKernelCpuSuspendIntr();
		m = ifh->tx_head;
		if (m != NULL) {
			SceNetMbuf *next = m->m_nextpkt;
			ifh->tx_head = next;
			if (next == NULL) {
				ifh->tx_tail = NULL;
			}
			m->m_nextpkt = NULL;
			ifh->tx_count--;
		}
		sceKernelCpuResumeIntr(intr);

		sceKernelSetEventFlag(g_net_evf_id, 8);

		if (m != NULL && (int16_t)m->m_flags < 0) {
			if (g_eapol_enabled <= 0 || g_eapol_cb2(m) < 0) {
				sceNetIfhandle_lib_F56FAC82(m);
				continue;
			}
		}
		break;
	}

	if (sceWlanDrv_driver_2519EAA7() > 0) {
		sceNetIfhandle_lib_F56FAC82(m);
		m = NULL;
	}
	return m;
}

int sceNetIfhandle_lib_30F69334(int evf_id)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	void *cbRaw;
	int res = 0;
	if (sceKernelGetUIDcontrolBlock(evf_id, &cbRaw) != 0) {
		res = (int)0x80410003;
	} else {
		SceNetUidCB *cb = (SceNetUidCB *)cbRaw;
		uint32_t ok = (pspGetK1() >> 31) ? (cb->attr & 2u) : 2u;
		if (ok == 0) {
			res = (int)0x80410003;
		} else {
			g_net_evf_id = evf_id;
			for (int i = 0; i < 8; i++) {
				g_if_table[i] = NULL;
			}
			g_rx_queue.drops = 0;
			g_rx_queue.tail = NULL;
			g_rx_queue.count = 0;
			g_rx_queue.max_count = 0;
			g_rx_queue.head = NULL;
			sceNetIfhandle_lib_35FAB6A2();
		}
	}

	pspSetK1(old_k1);
	return res;
}

static int get_attach_info(SceNetIf **out_ifp, int *out_sema)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;

	if (out_ifp == NULL || ((int32_t)(k1_shifted & (uint32_t)out_ifp) < 0) ||
	    out_sema == NULL || ((int32_t)(k1_shifted & (uint32_t)out_sema) < 0)) {
		pspSetK1(old_k1);
		return (int)0x80410003;
	}

	int res;
	if (g_attach_if == NULL) {
		res = (int)0x80410004;
	} else {
		*out_ifp = g_attach_if;
		*out_sema = g_attach_sema;
		res = 0;
	}
	pspSetK1(old_k1);
	return res;
}

static int get_detach_info(SceNetIf **out_ifp, int *out_sema)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;

	if (out_ifp == NULL || ((int32_t)(k1_shifted & (uint32_t)out_ifp) < 0) ||
	    out_sema == NULL || ((int32_t)(k1_shifted & (uint32_t)out_sema) < 0)) {
		pspSetK1(old_k1);
		return (int)0x80410003;
	}

	int res;
	if (g_detach_if == NULL) {
		res = (int)0x80410004;
	} else {
		*out_ifp = g_detach_if;
		*out_sema = g_detach_sema;
		res = 0;
	}
	pspSetK1(old_k1);
	return res;
}

int sceNetIfhandle_lib_B8188F96(SceNetIf **out_ifp, int *out_sema)
{
	return get_attach_info(out_ifp, out_sema);
}

int sceNetIfhandle_lib_4FB43BCE(SceNetIf **out_ifp, int *out_sema)
{
	return get_detach_info(out_ifp, out_sema);
}

int sceNetIfhandle_lib_B40A882F(SceNetIf **out_ifp, int *out_sema)
{
	return get_attach_info(out_ifp, out_sema);
}

int sceNetIfhandle_lib_9173FD47(SceNetIf **out_ifp, int *out_sema)
{
	return get_detach_info(out_ifp, out_sema);
}

void sceNetIfhandle_lib_8FCB05A1(SceNetIf *ifp)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (ifp != NULL && ((int32_t)(k1_shifted & (uint32_t)ifp) >= 0)) {
		for (int i = 0; i < 8; i++) {
			SceNetIfHandle *ifh = g_if_table[i];
			if (ifh != NULL && ifh->ifp == ifp) {
				ifh->cb_up(ifh);
				break;
			}
		}
	}
	pspSetK1(old_k1);
}

void sceNetIfhandle_lib_EAD3A759(SceNetIf *ifp)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (ifp != NULL && ((int32_t)(k1_shifted & (uint32_t)ifp) >= 0)) {
		for (int i = 0; i < 8; i++) {
			SceNetIfHandle *ifh = g_if_table[i];
			if (ifh != NULL && ifh->ifp == ifp) {
				ifh->cb_down(ifh);
				break;
			}
		}
	}
	pspSetK1(old_k1);
}

SceNetMbuf *sceNetIfhandle_lib_E440A7D8(void)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	int intr = sceKernelCpuSuspendIntr();
	SceNetMbuf *m = g_rx_queue.head;
	if (m != NULL) {
		SceNetMbuf *next = m->m_nextpkt;
		g_rx_queue.head = next;
		if (next == NULL) {
			g_rx_queue.tail = NULL;
		}
		m->m_nextpkt = NULL;
		g_rx_queue.count--;
	}
	sceKernelCpuResumeIntr(intr);

	pspSetK1(old_k1);
	return m;
}

static inline __attribute__((always_inline)) int check_net_sema(int semaid)
{
	for (int i = 0; i < 8; i++) {
		SceNetIfHandle *ifh = g_if_table[i];
		if (ifh != NULL && ifh->ifp != NULL) {
			if (semaid == ifh->ifp->ioctl_sema || semaid == ifh->ifp->ioctl_ok_sema) {
				return 1;
			}
		}
	}
	if (semaid == g_attach_sema || semaid == g_detach_sema) {
		return 1;
	}
	return 0;
}

int sceNetIfhandle_lib_30602CE9(int semaid, int signal)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	int res = (int)0x80410003;
	if (check_net_sema(semaid)) {
		res = sceKernelSignalSema(semaid, signal);
	}

	pspSetK1(old_k1);
	return res;
}

int sceNetIfhandle_lib_D5DA7B3C(int semaid, int signal, uint32_t *timeout)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	int res = (int)0x80410003;
	if (((int32_t)(k1_shifted & (uint32_t)timeout) >= 0) && check_net_sema(semaid)) {
		res = sceKernelWaitSema(semaid, signal, timeout);
	}

	pspSetK1(old_k1);
	return res;
}

int sceNetIfhandle_lib_2162EE67(int semaid, int signal)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	int res = (int)0x80410003;
	if (check_net_sema(semaid)) {
		res = sceKernelPollSema(semaid, signal);
	}

	pspSetK1(old_k1);
	return res;
}

static int alloc_ifhandle(SceNetIfHandle *ifh, uint32_t ifp_size)
{
	if (ifh == NULL) {
		return (int)0x80000103;
	}

	int slot = -1;
	for (int i = 0; i < 8; i++) {
		if (g_if_table[i] == NULL) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		return (int)0x80000022;
	}

	SceNetIf *ifp = (SceNetIf *)sceNetIfhandle_lib_15CFE3C0(ifp_size);
	ifh->ifp = ifp;
	if (ifp == NULL) {
		return (int)0x80000022;
	}
	memset(ifp, 0, ifp_size);

	int ioctl_sema = sceKernelCreateSema("SceNetIoctlSema", 7, 0, 0xff, NULL);
	if (ioctl_sema < 0) {
		sceNetIfhandle_lib_76BAD213(ifh->ifp);
		return ioctl_sema;
	}
	ifp->ioctl_sema = ioctl_sema;

	int ioctl_ok_sema = sceKernelCreateSema("SceNetIoctlOKSema", 7, 1, 1, NULL);
	if (ioctl_ok_sema < 0) {
		sceKernelDeleteSema(ifp->ioctl_sema);
		sceNetIfhandle_lib_76BAD213(ifh->ifp);
		return ioctl_ok_sema;
	}
	ifp->ioctl_ok_sema = ioctl_ok_sema;
	g_if_table[slot] = ifh;
	return 0;
}

int sceNetIfhandle_driver_16042084(SceNetIfHandle *ifh)
{
	return alloc_ifhandle(ifh, 0x140);
}

int sceNetIfhandle_driver_4342ED24(SceNetIfHandle *ifh)
{
	return alloc_ifhandle(ifh, 0x23b8);
}

static inline __attribute__((always_inline)) int free_ifhandle(SceNetIfHandle *ifh)
{
	if (ifh == NULL) {
		return (int)0x80000103;
	}

	int slot;
	for (slot = 0; slot < 8; slot++) {
		if (g_if_table[slot] == ifh) {
			break;
		}
	}
	if (slot == 8) {
		return (int)0x80000025;
	}

	SceNetIf *ifp = ifh->ifp;
	sceKernelDeleteSema(ifp->ioctl_sema);
	sceKernelDeleteSema(ifp->ioctl_ok_sema);
	sceNetIfhandle_lib_76BAD213(ifh->ifp);
	g_if_table[slot] = NULL;
	return 0;
}

int sceNetIfhandle_driver_C9344A59(SceNetIfHandle *ifh)
{
	return free_ifhandle(ifh);
}

int sceNetIfhandle_driver_2EAF809F(SceNetIfHandle *ifh)
{
	return free_ifhandle(ifh);
}

static int do_attach_ifhandle(SceNetIfHandle *ifh, const uint8_t *mac, const char *name, uint32_t ev_bit)
{
	if (g_attach_if != NULL) {
		return (int)0x80000021;
	}

	int slot;
	for (slot = 0; slot < 8; slot++) {
		if (g_if_table[slot] == ifh) {
			break;
		}
	}
	if (slot == 8) {
		return (int)0x80000025;
	}

	SceNetIf *ifp = ifh->ifp;
	ifh->tx_head = NULL;
	ifh->tx_tail = NULL;
	ifh->tx_count = 0;
	ifh->tx_max = 0;
	ifh->tx_drops = 0;
	strncpy(ifp->name, name, 16);

	if (mac != NULL) {
		memcpy(ifp->mac, mac, 6);
	}

	int sema = sceKernelCreateSema("SceNetAttachIfhandle", 3, 0, 1, NULL);
	if (sema < 0) {
		return sema;
	}

	g_attach_if = ifp;
	g_attach_sema = sema;
	int res = sceKernelSetEventFlag(g_net_evf_id, ev_bit);
	if (res >= 0) {
		res = sceKernelWaitSema(sema, 1, NULL);
	}
	sceKernelDeleteSema(sema);
	g_attach_sema = 0;
	g_attach_if = NULL;
	return res;
}

int sceNetIfhandle_driver_AE81C0CB(SceNetIfHandle *ifh, const uint8_t *mac, const char *name)
{
	if (ifh == NULL || mac == NULL || name == NULL) {
		return (int)0x80000103;
	}
	return do_attach_ifhandle(ifh, mac, name, 1);
}

int sceNetIfhandle_driver_6FB246F5(SceNetIfHandle *ifh, const char *name)
{
	if (ifh == NULL || name == NULL) {
		return (int)0x80000103;
	}
	return do_attach_ifhandle(ifh, NULL, name, 0x20);
}

static int do_detach_ifhandle(SceNetIfHandle *ifh, uint32_t ev_bit)
{
	if (ifh == NULL) {
		return (int)0x80000103;
	}
	if (g_detach_if != NULL) {
		return (int)0x80000021;
	}

	int slot;
	for (slot = 0; slot < 8; slot++) {
		if (g_if_table[slot] == ifh) {
			break;
		}
	}
	if (slot == 8) {
		return (int)0x80000025;
	}

	SceNetMbufQueue tmp_q = { NULL, NULL, 0, 0, 0 };
	SceNetIf *ifp = ifh->ifp;

	for (;;) {
		int intr = sceKernelCpuSuspendIntr();
		SceNetMbuf *m = g_rx_queue.head;
		if (m != NULL) {
			SceNetMbuf *next = m->m_nextpkt;
			g_rx_queue.head = next;
			if (next == NULL) {
				g_rx_queue.tail = NULL;
			}
			m->m_nextpkt = NULL;
			g_rx_queue.count--;
		}
		sceKernelCpuResumeIntr(intr);

		if (m == NULL) {
			break;
		}
		if (m->pkthdr_rcvif == ifp) {
			sceNetIfhandle_lib_F56FAC82(m);
		} else {
			m->m_nextpkt = NULL;
			if (tmp_q.tail != NULL) {
				tmp_q.tail->m_nextpkt = m;
			} else {
				tmp_q.head = m;
			}
			tmp_q.tail = m;
			tmp_q.count++;
		}
	}

	while (tmp_q.head != NULL) {
		SceNetMbuf *m = tmp_q.head;
		SceNetMbuf *next = m->m_nextpkt;
		tmp_q.head = next;
		if (next == NULL) {
			tmp_q.tail = NULL;
		}
		tmp_q.count--;
		m->m_nextpkt = NULL;

		int intr = sceKernelCpuSuspendIntr();
		m->m_nextpkt = NULL;
		if (g_rx_queue.tail != NULL) {
			g_rx_queue.tail->m_nextpkt = m;
		} else {
			g_rx_queue.head = m;
		}
		g_rx_queue.tail = m;
		g_rx_queue.count++;
		sceKernelCpuResumeIntr(intr);
	}

	int sema = sceKernelCreateSema("SceNetDetachIfhandle", 3, 0, 1, NULL);
	if (sema < 0) {
		return sema;
	}

	g_detach_if = ifp;
	g_detach_sema = sema;
	int res = sceKernelSetEventFlag(g_net_evf_id, ev_bit);
	if (res >= 0) {
		res = sceKernelWaitSema(sema, 1, NULL);
		while (ifh->tx_head != NULL) {
			SceNetMbuf *m = ifh->tx_head;
			SceNetMbuf *next = m->m_nextpkt;
			ifh->tx_head = next;
			if (next == NULL) {
				ifh->tx_tail = NULL;
			}
			m->m_nextpkt = NULL;
			ifh->tx_count--;
			sceNetIfhandle_lib_F56FAC82(m);
		}
	}
	sceKernelDeleteSema(sema);
	g_detach_sema = 0;
	g_detach_if = NULL;
	return res;
}

int sceNetIfhandle_driver_54D1AEA1(SceNetIfHandle *ifh)
{
	return do_detach_ifhandle(ifh, 2);
}

int sceNetIfhandle_driver_153C5167(SceNetIfHandle *ifh)
{
	return do_detach_ifhandle(ifh, 0x40);
}

void sceNetIfhandle_driver_F94BAF52(SceNetIfHandle *ifh, int unused, int res)
{
	(void)unused;
	if (ifh != NULL) {
		SceNetIf *ifp = ifh->ifp;
		ifp->ioctl_res = res;
		sceKernelSignalSema(ifp->ioctl_sema, 1);
	}
}

static void copy_from_mbuf_internal(SceNetMbuf *m, int off, int len, void *dst)
{
	if (dst == NULL) {
		return;
	}
	while (off > 0) {
		if ((int32_t)m < 0) {
			return;
		}
		if (off < m->m_len) {
			break;
		}
		off -= m->m_len;
		m = m->m_next;
	}
	uint8_t *out = (uint8_t *)dst;
	while (len > 0) {
		if ((int32_t)m < 0) {
			return;
		}
		int avail = m->m_len - off;
		int chunk = (avail < len) ? avail : len;
		uint8_t *src = m->m_data + off;
		if ((int32_t)src < 0 || (int32_t)(src + chunk) < 0) {
			return;
		}
		memcpy(out, src, chunk);
		out += chunk;
		len -= chunk;
		m = m->m_next;
		off = 0;
	}
}

static void rx_input(SceNetIfHandle *ifh, SceNetMbuf *m, int type)
{
	if (ifh == NULL) {
		return;
	}
	if (type == 6) {
		if (sceWlanDrv_driver_2519EAA7() > 0) {
			sceNetIfhandle_lib_F56FAC82(m);
			return;
		}
	}

	m->pkthdr_rcvif = ifh->ifp;
	if (type == 6) {
		uint8_t *data = m->m_data;
		if ((int32_t)data < 0) {
			sceNetIfhandle_lib_F56FAC82(m);
			return;
		}
		uint16_t ethertype = ((uint16_t)data[12] << 8) | data[13];
		if (data[0] & 1) {
			if (memcmp(g_bcast_mac, data, 6) == 0) {
				m->m_flags |= M_BCAST;
			} else {
				m->m_flags |= M_MCAST;
			}
		}
		if (ethertype == 0x88c8) {
			if (m->pkthdr_len < 0x10) {
				sceNetIfhandle_lib_F56FAC82(m);
				return;
			}
			uint8_t hdr_ver[2];
			copy_from_mbuf_internal(m, 14, 2, hdr_ver);
			uint16_t ver = ((uint16_t)hdr_ver[0] << 8) | hdr_ver[1];
			if (ver == 1) {
				if (g_eapol_enabled <= 0 || g_eapol_cb3(m) < 0) {
					sceNetIfhandle_lib_F56FAC82(m);
				}
				return;
			} else if (ver == 2) {
				if (m->pkthdr_len < 0x30 || g_eapol_enabled <= 0 || g_eapol_cb1(m) < 0) {
					sceNetIfhandle_lib_F56FAC82(m);
					return;
				}
			} else {
				sceNetIfhandle_lib_F56FAC82(m);
				return;
			}
		}
	}

	int intr = sceKernelCpuSuspendIntr();
	m->m_nextpkt = NULL;
	if (g_rx_queue.tail != NULL) {
		g_rx_queue.tail->m_nextpkt = m;
	} else {
		g_rx_queue.head = m;
	}
	g_rx_queue.tail = m;
	g_rx_queue.count++;
	sceKernelCpuResumeIntr(intr);

	sceKernelSetEventFlag(g_net_evf_id, 4);
}

void sceNetIfhandle_driver_C28F6FF2(SceNetIfHandle *ifh, SceNetMbuf *m)
{
	rx_input(ifh, m, 6);
}

void sceNetIfhandle_driver_A5C40C88(SceNetIfHandle *ifh, SceNetMbuf *m)
{
	rx_input(ifh, m, 0x17);
}

void sceNetIfhandle_driver_16246B99(SceNetIfHandle *ifh, SceNetMbuf *m)
{
	if (ifh == NULL) {
		return;
	}
	int intr = sceKernelCpuSuspendIntr();
	m->m_nextpkt = ifh->tx_head;
	if (ifh->tx_tail == NULL) {
		ifh->tx_tail = m;
	}
	ifh->tx_head = m;
	ifh->tx_count++;
	sceKernelCpuResumeIntr(intr);
}

int sceNetIfhandle_driver_C6D14282(SceNetIfHandle *ifh, void *priv)
{
	if (ifh == NULL) {
		return (int)0x80000103;
	}
	ifh->priv = priv;
	return 0;
}

void *sceNetIfhandle_driver_D5AD6DEA(SceNetIfHandle *ifh)
{
	if (ifh == NULL) {
		return NULL;
	}
	return ifh->priv;
}

int sceNetIfhandle_driver_0542835F(int (*cb1)(SceNetMbuf *), int (*cb2)(SceNetMbuf *), int (*cb3)(SceNetMbuf *))
{
	int intr = sceKernelCpuSuspendIntr();
	g_eapol_cb2 = cb2;
	g_eapol_cb1 = cb1;
	g_eapol_cb3 = cb3;
	g_eapol_enabled = 1;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

int sceNetIfhandle_driver_773FC77C(void)
{
	int intr = sceKernelCpuSuspendIntr();
	g_eapol_cb1 = NULL;
	g_eapol_enabled = 0;
	g_eapol_cb2 = NULL;
	g_eapol_cb3 = NULL;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

SceNetIfHandle *sceNetIfhandle_driver_9CBA24D4(const char *name)
{
	for (int i = 0; i < 8; i++) {
		SceNetIfHandle *ifh = g_if_table[i];
		if (ifh != NULL && ifh->ifp != NULL) {
			if (strcmp(ifh->ifp->name, name) == 0) {
				return ifh;
			}
		}
	}
	return NULL;
}

int sceNetIfhandle_driver_C5623112(SceNetIfHandle *ifh, uint8_t *mac)
{
	for (int i = 0; i < 8; i++) {
		if (g_if_table[i] == ifh) {
			memcpy(mac, ifh->ifp->mac, 6);
			return 0;
		}
	}
	return (int)0x80000025;
}

int sceNetGetDropRate(void *inBuf, void *outBuf)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	int res = (int)0x80410003;
	if (inBuf != NULL && ((int32_t)(k1_shifted & (uint32_t)inBuf) >= 0) &&
	    outBuf != NULL && ((int32_t)(k1_shifted & (uint32_t)outBuf) >= 0)) {
		res = sceWlanDrv_driver_B6A9700D(inBuf, outBuf);
	}
	pspSetK1(old_k1);
	return res;
}

int sceNetSetDropRate(int wlanMode)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	int res = sceWlanDrv_driver_325F7172(wlanMode);
	pspSetK1(old_k1);
	return res;
}


void sceNetIfhandle_lib_35FAB6A2(void)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	g_cur_game_wa = NULL;
	uint8_t *game_info = (uint8_t *)sceKernelGetGameInfo();
	if (game_info != NULL) {
		const char *title_id = (const char *)(game_info + 0x44);
		int disc_img = sceKernelInitDiscImage();
		uint32_t has_disc = (disc_img != 0) ? 1 : 0;
		uint32_t no_disc = (disc_img == 0) ? 1 : 0;

		for (int i = 0; i < 8; i++) {
			if (strcmp(title_id, g_game_wa_table[i].title_id) == 0) {
				uint32_t mode = g_game_wa_table[i].mode_mask;
				if (((mode & 1) && no_disc) || (((mode >> 1) & 1) && has_disc)) {
					g_cur_game_wa = &g_game_wa_table[i];
				}
			}
		}
	}

	pspSetK1(old_k1);
}

void sceNetIfhandle_lib_0B258B5E(void)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	g_cur_game_wa = NULL;
	pspSetK1(old_k1);
}

uint32_t sceNetIfhandle_lib_07505747(int idx)
{
	if (g_cur_game_wa != NULL && idx >= 0 && idx < 5) {
		return g_cur_game_wa->params[idx];
	}
	return 0;
}

int module_start(int args, void *argp)
{
	(void)args;
	(void)argp;
	return 0;
}

int module_stop(int args, void *argp)
{
	(void)args;
	(void)argp;
	return 0;
}
