#include <stdint.h>
#include <stddef.h>

/* Imports */
int sceKernelInitDiscImage(void);
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int state);
void Kprintf(const char *fmt, ...);
int sceKernelQueryModuleInfo(int modid, void *info);
int sceKernelSearchModuleByName(const char *name);
int sceKernelGetUIDcontrolBlock(int uid, void **cb);
void *sceKernelGetGameInfo(void);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strncpy(char *dst, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int sceKernelSetEventFlag(int evf, uint32_t bits);
int sceKernelDeleteSema(int semaid);
int sceKernelReferVplStatus(int vplid, void *info);
int sceKernelSignalSema(int semaid, int signal);
int sceKernelWaitSema(int semaid, int signal, uint32_t *timeout);
int sceKernelCreateVpl(const char *name, int part, int attr, uint32_t size, void *opt);
int sceKernelPollSema(int semaid, int signal);
int sceKernelDeleteVpl(int vplid);
int sceKernelTryAllocateVpl(int vplid, uint32_t size, void **data);
int sceKernelFreeVpl(int vplid, void *data);
int sceKernelCreateSema(const char *name, uint32_t attr, int init_val, int max_val, void *opt);
int sceWlanDrv_driver_2519EAA7(void);
int sceWlanDrv_driver_325F7172(int a0);
int sceWlanDrv_driver_B6A9700D(void *a0, void *a1);

#define M_EXT       0x0001
#define M_PKTHDR    0x0002
#define M_RDONLY    0x0008
#define M_BCAST     0x0100
#define M_MCAST     0x0200

typedef struct SceNetMbuf {
	struct SceNetMbuf *m_next;      /* +0x00 */
	struct SceNetMbuf *m_nextpkt;   /* +0x04 */
	uint8_t *m_data;                /* +0x08 */
	int32_t m_len;                  /* +0x0c */
	uint16_t m_type;                /* +0x10 */
	uint16_t m_flags;               /* +0x12 */
	void *pkthdr_rcvif;             /* +0x14 */
	int32_t pkthdr_len;             /* +0x18 */
	uint32_t pkthdr_pad[5];         /* +0x1c..0x2c */
	uint8_t *ext_buf;               /* +0x30 */
	void *ext_free;                 /* +0x34 */
	void *ext_arg;                  /* +0x38 */
	uint32_t ext_size;              /* +0x3c */
	uint32_t ext_type;              /* +0x40 */
	struct SceNetMbuf *ext_nextref; /* +0x44 */
	struct SceNetMbuf *ext_prevref; /* +0x48 */
	uint8_t pad[0x100 - 0x4c];
} SceNetMbuf;

typedef struct SceNetIfEvent {
	struct SceNetIfEvent *next; /* +0x00 */
	struct SceNetIfEvent *prev; /* +0x04 */
	int type;                   /* +0x08 */
	int arg;                    /* +0x0c */
} SceNetIfEvent;

typedef struct SceNetIf {
	uint8_t pad0[0x14];
	char name[16];              /* +0x14 */
	uint8_t pad1[0xec - 0x24];
	uint8_t mac[6];             /* +0xec */
	uint8_t pad2[0x100 - 0xf2];
	int ioctl_sema;             /* +0x100 */
	int ioctl_ok_sema;          /* +0x104 */
	int pad108;                 /* +0x108 */
	int ioctl_res;              /* +0x10c */
	SceNetIfEvent *ev_head;     /* +0x110 */
	SceNetIfEvent *ev_tail;     /* +0x114 */
	int evf_id;                 /* +0x118 */
	uint32_t evf_bits;          /* +0x11c */
} SceNetIf;

typedef struct SceNetIfHandle {
	SceNetIf *ifp;                                             /* +0x00 */
	void *priv;                                                /* +0x04 */
	void (*cb_up)(struct SceNetIfHandle *ifh);                 /* +0x08 */
	void (*cb_down)(struct SceNetIfHandle *ifh);               /* +0x0c */
	void (*start_tx)(struct SceNetIfHandle *ifh);              /* +0x10 */
	void (*ioctl)(struct SceNetIfHandle *ifh, uint32_t cmd, void *arg, void **ptrs); /* +0x14 */
	SceNetMbuf *tx_head;                                       /* +0x18 */
	SceNetMbuf *tx_tail;                                       /* +0x1c */
	int tx_count;                                              /* +0x20 */
	int tx_max;                                                /* +0x24 */
	int tx_drops;                                              /* +0x28 */
} SceNetIfHandle;

typedef struct SceNetMbufQueue {
	SceNetMbuf *head; /* +0x00 */
	SceNetMbuf *tail; /* +0x04 */
	int count;        /* +0x08 */
	int max_count;    /* +0x0c */
	int drops;        /* +0x10 */
} SceNetMbufQueue;

typedef struct GameWorkaround {
	const char *title_id;
	uint32_t mode_mask;
	uint32_t params[5];
} GameWorkaround;

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

/* .data */
static int g_vpl_uid = -1;
static uint32_t g_vpl_max_used = 0xffffffffu;

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
static uint32_t g_mbuf_cfg0;
static uint32_t g_mbuf_cfg1;
static uint32_t g_mbuf_cfg2;
static uint32_t g_mbuf_cfg3;
static uint32_t g_copym_drops;
static uint32_t g_pullup_drops;
static const GameWorkaround *g_cur_game_wa;

/* Forward declarations */
void *sceNetIfhandle_lib_15CFE3C0(uint32_t size);
void sceNetIfhandle_lib_76BAD213(void *ptr);
SceNetMbuf *sceNetIfhandle_lib_F8825DC4(SceNetMbuf *m);
void sceNetIfhandle_lib_F56FAC82(SceNetMbuf *m);
SceNetMbuf *sceNetIfhandle_lib_59F0D619(int how, uint16_t type);
void sceNetIfhandle_lib_9A6261EC(SceNetMbuf *m, int off, int len, void *cp);
void sceNetIfhandle_lib_BFF3CEA5(SceNetMbuf *mp, int req_len);
SceNetMbuf *sceNetIfhandle_lib_456E3146(SceNetMbuf *m, int off0, int len, int wait);
void sceNetIfhandle_lib_35FAB6A2(void);
void sceNetIfhandle_lib_0B258B5E(void);
uint32_t sceNetIfhandle_lib_07505747(int idx);

static inline uint32_t get_k1(void)
{
	uint32_t k1;
	__asm__ volatile("move %0, $k1" : "=r"(k1));
	return k1;
}

static inline void set_k1(uint32_t k1)
{
	__asm__ volatile("move $k1, %0" : : "r"(k1));
}

void sceNetIfhandle_lib_B9096E48(void)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

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
	set_k1(old_k1);
}

void sceNetIfhandle_lib_B1F5BB87(SceNetIf *ifp, SceNetMbuf *m)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (ifp == NULL || ((int32_t)(k1_shifted & (uint32_t)ifp) < 0) ||
	    m == NULL || ((int32_t)m < 0)) {
		set_k1(old_k1);
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
		set_k1(old_k1);
		return;
	}

	uint32_t drop_mod = sceNetIfhandle_lib_07505747(1);
	if (drop_mod != 0 && (int16_t)m->m_flags < 0) {
		uint32_t cur = g_drop_counter;
		g_drop_counter = cur + 1;
		if ((cur % drop_mod) != 0) {
			sceNetIfhandle_lib_F56FAC82(m);
			set_k1(old_k1);
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
	set_k1(old_k1);
}

void sceNetIfhandle_lib_0296C7D6(SceNetIf *ifp, uint32_t cmd, void *arg)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (ifp == NULL || ((int32_t)(k1_shifted & (uint32_t)ifp) < 0) ||
	    arg == NULL || ((int32_t)(k1_shifted & (uint32_t)arg) < 0)) {
		set_k1(old_k1);
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
		set_k1(old_k1);
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
		set_k1(old_k1);
		return;
	}
	if (ptrs[1] != NULL && ((int32_t)(k1_shifted & (uint32_t)ptrs[1]) < 0)) {
		set_k1(old_k1);
		return;
	}

	ifh->ioctl(ifh, cmd, arg, ptrs);
	set_k1(old_k1);
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
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	void *cb = NULL;
	int res = 0;
	if (sceKernelGetUIDcontrolBlock(evf_id, &cb) != 0) {
		res = (int)0x80410003;
	} else {
		uint16_t attr = *((uint16_t *)((uint8_t *)cb + 0x16));
		uint32_t ok = (k1_shifted >> 31) ? (attr & 2) : 2;
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

	set_k1(old_k1);
	return res;
}

static int get_attach_info(SceNetIf **out_ifp, int *out_sema)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;

	if (out_ifp == NULL || ((int32_t)(k1_shifted & (uint32_t)out_ifp) < 0) ||
	    out_sema == NULL || ((int32_t)(k1_shifted & (uint32_t)out_sema) < 0)) {
		set_k1(old_k1);
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
	set_k1(old_k1);
	return res;
}

static int get_detach_info(SceNetIf **out_ifp, int *out_sema)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;

	if (out_ifp == NULL || ((int32_t)(k1_shifted & (uint32_t)out_ifp) < 0) ||
	    out_sema == NULL || ((int32_t)(k1_shifted & (uint32_t)out_sema) < 0)) {
		set_k1(old_k1);
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
	set_k1(old_k1);
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
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (ifp != NULL && ((int32_t)(k1_shifted & (uint32_t)ifp) >= 0)) {
		for (int i = 0; i < 8; i++) {
			SceNetIfHandle *ifh = g_if_table[i];
			if (ifh != NULL && ifh->ifp == ifp) {
				ifh->cb_up(ifh);
				break;
			}
		}
	}
	set_k1(old_k1);
}

void sceNetIfhandle_lib_EAD3A759(SceNetIf *ifp)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (ifp != NULL && ((int32_t)(k1_shifted & (uint32_t)ifp) >= 0)) {
		for (int i = 0; i < 8; i++) {
			SceNetIfHandle *ifh = g_if_table[i];
			if (ifh != NULL && ifh->ifp == ifp) {
				ifh->cb_down(ifh);
				break;
			}
		}
	}
	set_k1(old_k1);
}

SceNetMbuf *sceNetIfhandle_lib_E440A7D8(void)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

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

	set_k1(old_k1);
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
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	int res = (int)0x80410003;
	if (check_net_sema(semaid)) {
		res = sceKernelSignalSema(semaid, signal);
	}

	set_k1(old_k1);
	return res;
}

int sceNetIfhandle_lib_D5DA7B3C(int semaid, int signal, uint32_t *timeout)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	int res = (int)0x80410003;
	if (((int32_t)(k1_shifted & (uint32_t)timeout) >= 0) && check_net_sema(semaid)) {
		res = sceKernelWaitSema(semaid, signal, timeout);
	}

	set_k1(old_k1);
	return res;
}

int sceNetIfhandle_lib_2162EE67(int semaid, int signal)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	int res = (int)0x80410003;
	if (check_net_sema(semaid)) {
		res = sceKernelPollSema(semaid, signal);
	}

	set_k1(old_k1);
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

	int s1 = sceKernelCreateSema("SceNetIoctlSema", 7, 0, 0xff, NULL);
	if (s1 < 0) {
		sceNetIfhandle_lib_76BAD213(ifh->ifp);
		return s1;
	}
	ifp->ioctl_sema = s1;

	int s2 = sceKernelCreateSema("SceNetIoctlOKSema", 7, 1, 1, NULL);
	if (s2 < 0) {
		sceKernelDeleteSema(ifp->ioctl_sema);
		sceNetIfhandle_lib_76BAD213(ifh->ifp);
		return s2;
	}
	ifp->ioctl_ok_sema = s2;
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

	int slot = -1;
	for (int i = 0; i < 8; i++) {
		if (g_if_table[i] == ifh) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
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

	int slot = -1;
	for (int i = 0; i < 8; i++) {
		if (g_if_table[i] == ifh) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
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

	int slot = -1;
	for (int i = 0; i < 8; i++) {
		if (g_if_table[i] == ifh) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
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

int sceNetIfhandle_C80181A2(void *a0, void *a1)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	int res = (int)0x80410003;
	if (a0 != NULL && ((int32_t)(k1_shifted & (uint32_t)a0) >= 0) &&
	    a1 != NULL && ((int32_t)(k1_shifted & (uint32_t)a1) >= 0)) {
		res = sceWlanDrv_driver_B6A9700D(a0, a1);
	}
	set_k1(old_k1);
	return res;
}

int sceNetIfhandle_FD8585E1(int a0)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);
	int res = sceWlanDrv_driver_325F7172(a0);
	set_k1(old_k1);
	return res;
}

int sceNetIfhandle_lib_0C391E9F(uint32_t arg0, uint32_t arg1)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	int modid = sceKernelSearchModuleByName("sceNet_Library");
	uint32_t info[24];
	memset(info, 0, sizeof(info));
	info[0] = sizeof(info);
	int res = sceKernelQueryModuleInfo(modid, info);
	if (res >= 0) {
		int part = 2;
		uint32_t size = arg0;
		if ((info[16] & 0xffff0000u) != 0x01010000u) {
			part = (int)arg0;
			size = arg1;
		}
		if (part != 2 && part != 5) {
			res = (int)0x80410103;
		} else {
			res = sceKernelCreateVpl("SceNet", part, 1, size, NULL);
			if (res >= 0) {
				g_vpl_uid = res;
				g_vpl_max_used = 0;
			}
		}
	}

	set_k1(old_k1);
	return res;
}

int sceNetIfhandle_lib_29ED84C5(void)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	if (g_vpl_uid > 0) {
		sceKernelDeleteVpl(g_vpl_uid);
		g_vpl_uid = -1;
	}

	set_k1(old_k1);
	return 0;
}

int sceNetIfhandle_lib_4C2886CB(uint32_t *stat)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (stat == NULL || ((int32_t)(k1_shifted & (uint32_t)stat) < 0)) {
		set_k1(old_k1);
		return (int)0x80410103;
	}

	uint32_t vpl_info[13];
	vpl_info[0] = 0x34;
	int res = sceKernelReferVplStatus(g_vpl_uid, vpl_info);
	if (res < 0) {
		set_k1(old_k1);
		return res;
	}

	set_k1(old_k1);
	stat[0] = vpl_info[10];
	stat[1] = g_vpl_max_used;
	stat[2] = vpl_info[11];
	return 0;
}

void *sceNetIfhandle_lib_15CFE3C0(uint32_t size)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	void *ptr = NULL;
	if (sceKernelTryAllocateVpl(g_vpl_uid, size, &ptr) < 0 || (int32_t)ptr < 0) {
		set_k1(old_k1);
		return NULL;
	}

	uint32_t vpl_info[13];
	vpl_info[0] = 0x34;
	if (sceKernelReferVplStatus(g_vpl_uid, vpl_info) == 0) {
		uint32_t used = vpl_info[10] - vpl_info[11];
		if ((int32_t)used > (int32_t)g_vpl_max_used) {
			g_vpl_max_used = used;
		}
	}

	set_k1(old_k1);
	return ptr;
}

void sceNetIfhandle_lib_76BAD213(void *ptr)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (ptr == NULL || ((int32_t)(k1_shifted & (uint32_t)ptr) < 0) || (int32_t)ptr < 0) {
		set_k1(old_k1);
		return;
	}

	int res = sceKernelFreeVpl(g_vpl_uid, ptr);
	if (res < 0) {
		Kprintf("sceNetFree: invalid ptr 0x%p, ret = 0x%08x\n", ptr, res);
	}
	set_k1(old_k1);
}

SceNetMbuf *sceNetIfhandle_lib_59F0D619(int how, uint16_t type)
{
	(void)how;
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	SceNetMbuf *m = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (m == NULL) {
		set_k1(old_k1);
		return NULL;
	}

	uint8_t *dat = (uint8_t *)m + 0x14;
	m->m_type = type;
	m->m_next = NULL;
	m->m_nextpkt = NULL;
	m->m_data = dat;
	m->m_flags = 0;
	if ((int32_t)dat < 0) {
		set_k1(old_k1);
		return NULL;
	}
	memset(dat, 0, 0xec);
	set_k1(old_k1);
	return m;
}

SceNetMbuf *sceNetIfhandle_lib_F8825DC4(SceNetMbuf *m)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	if (m == NULL || (int32_t)m < 0) {
		set_k1(old_k1);
		return NULL;
	}

	uint16_t flags = m->m_flags;
	SceNetMbuf *nextref = m->ext_nextref;
	SceNetMbuf *prevref = m->ext_prevref;
	if (flags & M_EXT) {
		if ((int32_t)nextref < 0 || (int32_t)prevref < 0 || (int32_t)m->ext_buf < 0) {
			set_k1(old_k1);
			return NULL;
		}
		if (nextref == m) {
			sceNetIfhandle_lib_76BAD213(m->ext_buf);
			flags = m->m_flags;
		} else {
			prevref->ext_nextref = nextref;
			nextref->ext_prevref = prevref;
		}
		m->m_flags = flags & 0xfff6u;
		m->ext_size = 0;
	}

	SceNetMbuf *next = m->m_next;
	sceNetIfhandle_lib_76BAD213(m);
	set_k1(old_k1);
	if ((int32_t)next < 0) {
		return NULL;
	}
	return next;
}

void sceNetIfhandle_lib_F56FAC82(SceNetMbuf *m)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	while (m != NULL) {
		if ((int32_t)m < 0) {
			set_k1(old_k1);
			return;
		}
		uint16_t flags = m->m_flags;
		SceNetMbuf *nextref = m->ext_nextref;
		SceNetMbuf *prevref = m->ext_prevref;
		if (flags & M_EXT) {
			if ((int32_t)nextref < 0 || (int32_t)prevref < 0 || (int32_t)m->ext_buf < 0) {
				set_k1(old_k1);
				return;
			}
			if (nextref == m) {
				sceNetIfhandle_lib_76BAD213(m->ext_buf);
				flags = m->m_flags;
			} else {
				prevref->ext_nextref = nextref;
				nextref->ext_prevref = prevref;
			}
			m->m_flags = flags & 0xfff6u;
			m->ext_size = 0;
		}
		SceNetMbuf *next = m->m_next;
		sceNetIfhandle_lib_76BAD213(m);
		m = next;
	}

	set_k1(old_k1);
}

SceNetMbuf *sceNetIfhandle_lib_C3325FDC(SceNetMbuf *m, int len, int how)
{
	(void)how;
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	if (m == NULL || (int32_t)m < 0) {
		set_k1(old_k1);
		return NULL;
	}

	SceNetMbuf *n = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (n == NULL) {
		sceNetIfhandle_lib_F56FAC82(m);
		set_k1(old_k1);
		return NULL;
	}

	n->m_flags = 0;
	n->m_data = (uint8_t *)n + 0x14;
	n->m_type = m->m_type;
	n->m_next = NULL;
	n->m_nextpkt = NULL;

	if (m->m_flags & M_PKTHDR) {
		n->m_flags = m->m_flags & 0xfff6u;
		n->m_data = (uint8_t *)n + 0x30;
		n->pkthdr_rcvif = m->pkthdr_rcvif;
		n->pkthdr_len = m->pkthdr_len;
		m->m_flags &= ~M_PKTHDR;
		for (int i = 0; i < 5; i++) {
			n->pkthdr_pad[i] = m->pkthdr_pad[i];
		}
	}

	n->m_next = m;
	if ((uint32_t)len < 0xd0u) {
		n->m_data += ((0xd0u - (uint32_t)len) & ~3u);
	}
	n->m_len = len;
	set_k1(old_k1);
	return n;
}

void sceNetIfhandle_lib_9A6261EC(SceNetMbuf *m, int off, int len, void *cp)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (cp == NULL) {
		set_k1(old_k1);
		return;
	}
	uint32_t end = (uint32_t)cp + (uint32_t)len;
	if (((int32_t)(k1_shifted & ((uint32_t)cp | end | (uint32_t)len)) < 0) ||
	    (int32_t)cp < 0 || (int32_t)end < 0) {
		set_k1(old_k1);
		return;
	}
	if ((len | off) < 0) {
		Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf.c", "sceNetMCopydata");
		for (;;) {}
	}

	while (off > 0) {
		if (m == NULL) {
			Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf.c", "sceNetMCopydata");
			for (;;) {}
		}
		if ((int32_t)m < 0) {
			set_k1(old_k1);
			return;
		}
		if (off < m->m_len) {
			break;
		}
		off -= m->m_len;
		m = m->m_next;
	}

	uint8_t *out = (uint8_t *)cp;
	while (len > 0) {
		if (m == NULL) {
			Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf.c", "sceNetMCopydata");
			for (;;) {}
		}
		if ((int32_t)m < 0) {
			set_k1(old_k1);
			return;
		}
		int avail = m->m_len - off;
		int chunk = (avail < len) ? avail : len;
		uint8_t *src = m->m_data + off;
		len -= chunk;
		if ((int32_t)src < 0 || (int32_t)(src + chunk) < 0) {
			set_k1(old_k1);
			return;
		}
		memcpy(out, src, chunk);
		out += chunk;
		m = m->m_next;
		off = 0;
	}

	set_k1(old_k1);
}

void sceNetIfhandle_lib_3E8DD3F8(SceNetMbuf *m, SceNetMbuf *n)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	if (m == NULL || (int32_t)m < 0) {
		set_k1(old_k1);
		return;
	}

	while (m->m_next != NULL) {
		m = m->m_next;
		if ((int32_t)m < 0) {
			set_k1(old_k1);
			return;
		}
	}

	uint8_t *m_end = (uint8_t *)m + 0x100;
	while (n != NULL) {
		if ((int32_t)n < 0) {
			set_k1(old_k1);
			return;
		}
		if ((m->m_flags & M_EXT) || (m->m_data + m->m_len + n->m_len >= m_end)) {
			m->m_next = n;
			set_k1(old_k1);
			return;
		}
		uint8_t *dst = m->m_data + m->m_len;
		uint8_t *src = n->m_data;
		if ((int32_t)dst < 0 || (int32_t)(dst + n->m_len) < 0 ||
		    (int32_t)src < 0 || (int32_t)(src + n->m_len) < 0) {
			set_k1(old_k1);
			return;
		}
		memcpy(dst, src, n->m_len);
		m->m_len += n->m_len;
		n = sceNetIfhandle_lib_F8825DC4(n);
	}

	set_k1(old_k1);
}

void sceNetIfhandle_lib_BFF3CEA5(SceNetMbuf *mp, int req_len)
{
	uint32_t old_k1 = get_k1();
	int len = req_len;
	SceNetMbuf *m = mp;
	if (m == NULL || (int32_t)m < 0) {
		return;
	}

	if (len >= 0) {
		while (m != NULL && len > 0) {
			if ((int32_t)m < 0) {
				set_k1(old_k1);
				return;
			}
			if (m->m_len <= len) {
				len -= m->m_len;
				m->m_len = 0;
				m = m->m_next;
			} else {
				m->m_len -= len;
				m->m_data += len;
				len = 0;
			}
		}
		if (mp->m_flags & M_PKTHDR) {
			mp->pkthdr_len -= (req_len - len);
		}
		set_k1(old_k1);
	} else {
		len = -len;
		int count = 0;
		for (;;) {
			if ((int32_t)m < 0) {
				set_k1(old_k1);
				return;
			}
			count += m->m_len;
			if (m->m_next == NULL) {
				break;
			}
			m = m->m_next;
		}
		if (m->m_len >= len) {
			m->m_len -= len;
			if (mp->m_flags & M_PKTHDR) {
				mp->pkthdr_len -= len;
			}
			set_k1(old_k1);
			return;
		}
		count -= len;
		if (count < 0) {
			count = 0;
		}
		m = mp;
		if (m->m_flags & M_PKTHDR) {
			m->pkthdr_len = count;
		}
		for (; m != NULL; m = m->m_next) {
			if ((int32_t)m < 0) {
				set_k1(old_k1);
				return;
			}
			if (m->m_len >= count) {
				m->m_len = count;
				break;
			}
			count -= m->m_len;
		}
		if (m != NULL) {
			for (m = m->m_next; m != NULL; m = m->m_next) {
				if ((int32_t)m < 0) {
					set_k1(old_k1);
					return;
				}
				m->m_len = 0;
			}
		}
		set_k1(old_k1);
	}
}

SceNetMbuf *sceNetIfhandle_lib_49EDBB18(SceNetMbuf *n, int len)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	if (n == NULL || (int32_t)n < 0) {
		set_k1(old_k1);
		return NULL;
	}

	SceNetMbuf *m;
	if ((n->m_flags & M_EXT) == 0 &&
	    (n->m_data + len < (uint8_t *)n + 0x100) &&
	    n->m_next != NULL) {
		if (n->m_len >= len) {
			set_k1(old_k1);
			return n;
		}
		m = n;
		n = n->m_next;
		len -= m->m_len;
		if ((int32_t)n < 0) {
			set_k1(old_k1);
			return NULL;
		}
	} else {
		if ((uint32_t)len > 0xd0u) {
			sceNetIfhandle_lib_F56FAC82(n);
			g_pullup_drops++;
			set_k1(old_k1);
			return NULL;
		}
		m = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (m == NULL) {
			sceNetIfhandle_lib_F56FAC82(n);
			g_pullup_drops++;
			set_k1(old_k1);
			return NULL;
		}
		m->m_flags = 0;
		m->m_data = (uint8_t *)m + 0x14;
		m->m_type = n->m_type;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR) {
			m->m_flags = n->m_flags & 0xfff6u;
			m->m_data = (uint8_t *)m + 0x30;
			m->pkthdr_rcvif = n->pkthdr_rcvif;
			m->pkthdr_len = n->pkthdr_len;
			n->m_flags &= ~M_PKTHDR;
			for (int i = 0; i < 5; i++) {
				m->pkthdr_pad[i] = n->pkthdr_pad[i];
			}
		}
	}

	int space = (int)(((uint8_t *)m + 0x100) - (m->m_data + m->m_len));
	do {
		if ((int32_t)n < 0) {
			set_k1(old_k1);
			return NULL;
		}
		int count = (int)g_mbuf_cfg1;
		if (count < len) {
			count = len;
		}
		if (count > space) {
			count = space;
		}
		if (count > n->m_len) {
			count = n->m_len;
		}
		uint8_t *dst = m->m_data + m->m_len;
		uint8_t *src = n->m_data;
		if ((int32_t)dst < 0 || (int32_t)(dst + count) < 0 ||
		    (int32_t)src < 0 || (int32_t)(src + count) < 0) {
			set_k1(old_k1);
			return NULL;
		}
		memcpy(dst, src, count);
		len -= count;
		m->m_len += count;
		space -= count;
		n->m_len -= count;
		if (n->m_len != 0) {
			n->m_data += count;
		} else {
			n = sceNetIfhandle_lib_F8825DC4(n);
		}
	} while (len > 0 && n != NULL);

	if (len > 0) {
		sceNetIfhandle_lib_F8825DC4(m);
		sceNetIfhandle_lib_F56FAC82(n);
		g_pullup_drops++;
		set_k1(old_k1);
		return NULL;
	}
	m->m_next = n;
	set_k1(old_k1);
	return m;
}

void sceNetIfhandle_lib_1560F143(SceNetMbuf *m0, int off, int len, const void *cp)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (cp == NULL) {
		set_k1(old_k1);
		return;
	}
	uint32_t end = (uint32_t)cp + (uint32_t)len;
	if (((int32_t)(k1_shifted & ((uint32_t)cp | end | (uint32_t)len)) < 0) ||
	    (int32_t)cp < 0 || (int32_t)end < 0 || m0 == NULL || (int32_t)m0 < 0) {
		set_k1(old_k1);
		return;
	}

	SceNetMbuf *m = m0;
	int totlen = 0;
	while (off > m->m_len) {
		if ((int32_t)m < 0) {
			set_k1(old_k1);
			return;
		}
		off -= m->m_len;
		totlen += m->m_len;
		if (m->m_next == NULL) {
			SceNetMbuf *n = sceNetIfhandle_lib_59F0D619(1, m->m_type);
			if (n == NULL) {
				goto out;
			}
			int nlen = off + len;
			if (nlen > 0xec) {
				nlen = 0xec;
			}
			n->m_len = nlen;
			m->m_next = n;
		}
		m = m->m_next;
	}

	const uint8_t *src = (const uint8_t *)cp;
	while (len > 0) {
		if ((int32_t)m < 0) {
			set_k1(old_k1);
			return;
		}
		int mlen = m->m_len - off;
		if (mlen > len) {
			mlen = len;
		}
		uint8_t *dst = m->m_data + off;
		if ((int32_t)dst < 0 || (int32_t)(dst + mlen) < 0 ||
		    (int32_t)src < 0 || (int32_t)(src + mlen) < 0) {
			set_k1(old_k1);
			return;
		}
		memcpy(dst, src, mlen);
		src += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0) {
			break;
		}
		if (m->m_next == NULL) {
			SceNetMbuf *n = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
			if (n == NULL) {
				break;
			}
			n->m_type = m->m_type;
			n->m_data = (uint8_t *)n + 0x14;
			n->m_next = NULL;
			n->m_nextpkt = NULL;
			n->m_flags = 0;
			n->m_len = (len < 0xec) ? len : 0xec;
			m->m_next = n;
		}
		m = m->m_next;
	}

out:
	if ((m0->m_flags & M_PKTHDR) && (m0->pkthdr_len < totlen)) {
		m0->pkthdr_len = totlen;
	}
	set_k1(old_k1);
}

void sceNetIfhandle_lib_0FB8AE0D(void)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);
	g_mbuf_cfg0 = 0;
	g_mbuf_cfg1 = 0;
	g_mbuf_cfg2 = 0;
	g_mbuf_cfg3 = 0;
	set_k1(old_k1);
}

SceNetMbuf *sceNetIfhandle_lib_A493AA5F(int how, uint16_t type)
{
	(void)how;
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	SceNetMbuf *m = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (m != NULL) {
		m->m_type = type;
		m->m_data = (uint8_t *)m + 0x14;
		m->m_flags = 0;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
	}
	set_k1(old_k1);
	return m;
}

SceNetMbuf *sceNetIfhandle_lib_4CF15C43(int how, uint16_t type)
{
	(void)how;
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	SceNetMbuf *m = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (m != NULL) {
		m->m_type = type;
		m->m_data = (uint8_t *)m + 0x30;
		m->m_flags = M_PKTHDR;
		m->pkthdr_pad[0] = 0;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
	}
	set_k1(old_k1);
	return m;
}

static SceNetMbuf *m_copym0(SceNetMbuf *m, int off0, int len, int wait, int deep_ext, int copy_pkthdr_always)
{
	(void)wait;
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

	if (m == NULL || (int32_t)m < 0) {
		set_k1(old_k1);
		return NULL;
	}
	if ((len | off0) < 0) {
		Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf.c", "m_copym0");
		for (;;) {}
	}

	int off = off0;
	int copyhdr = 0;
	if ((off == 0 || copy_pkthdr_always != 0) && (m->m_flags & M_PKTHDR)) {
		copyhdr = 1;
	}

	while (off > 0) {
		if (m == NULL) {
			Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf.c", "m_copym0");
			for (;;) {}
		}
		if ((int32_t)m < 0) {
			set_k1(old_k1);
			return NULL;
		}
		if (off < m->m_len) {
			break;
		}
		off -= m->m_len;
		m = m->m_next;
	}

	SceNetMbuf *top = NULL;
	SceNetMbuf **mp = &top;
	while (len > 0) {
		if (m == NULL) {
			if (len != 1000000000) {
				Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf.c", "m_copym0");
				for (;;) {}
			}
			break;
		}
		if ((int32_t)m < 0) {
			set_k1(old_k1);
			return NULL;
		}

		SceNetMbuf *n = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (n != NULL) {
			n->m_type = m->m_type;
			n->m_data = (uint8_t *)n + 0x14;
			n->m_next = NULL;
			n->m_nextpkt = NULL;
			n->m_flags = 0;
		}
		*mp = n;
		if (n == NULL) {
			sceNetIfhandle_lib_F56FAC82(top);
			g_copym_drops++;
			set_k1(old_k1);
			return NULL;
		}

		if (copyhdr) {
			n->pkthdr_rcvif = m->pkthdr_rcvif;
			n->m_flags = m->m_flags & 0xfff6u;
			n->m_data = (uint8_t *)n + 0x30;
			n->pkthdr_len = m->pkthdr_len;
			for (int i = 0; i < 5; i++) {
				n->pkthdr_pad[i] = m->pkthdr_pad[i];
			}
			if (len == 1000000000) {
				n->pkthdr_len -= off0;
			} else {
				n->pkthdr_len = len;
			}
			copyhdr = 0;
		}

		int mlen = m->m_len - off;
		if (len < mlen) {
			mlen = len;
		}
		n->m_len = mlen;

		if (m->m_flags & M_EXT) {
			if (!deep_ext) {
				n->ext_buf = m->ext_buf;
				n->m_data = m->m_data + off;
				n->ext_free = m->ext_free;
				n->ext_arg = m->ext_arg;
				n->ext_size = m->ext_size;
				n->ext_type = m->ext_type;
				SceNetMbuf *nextref = m->ext_nextref;
				if ((int32_t)nextref < 0) {
					set_k1(old_k1);
					return NULL;
				}
				n->ext_nextref = nextref;
				n->ext_prevref = m;
				n->m_flags |= (m->m_flags & (M_EXT | M_RDONLY));
				nextref->ext_prevref = n;
				m->ext_nextref = n;
			} else {
				uint8_t *buf = (uint8_t *)sceNetIfhandle_lib_15CFE3C0(0x800);
				if (buf == NULL) {
					buf = (uint8_t *)sceNetIfhandle_lib_15CFE3C0(0x800);
				}
				n->ext_buf = buf;
				if (buf != NULL) {
					n->ext_size = 0x800;
					n->m_flags |= (M_EXT | M_RDONLY);
					n->m_data = buf;
					n->ext_free = NULL;
					n->ext_arg = NULL;
					n->ext_prevref = n;
					n->ext_nextref = n;
				}
				int space;
				if (n->m_flags & M_EXT) {
					space = (int)((n->ext_buf + n->ext_size) - n->m_data);
				} else {
					space = (int)(((uint8_t *)n + 0x100) - n->m_data);
				}
				if (space > len) {
					space = len;
				}
				int avail = m->m_len - off;
				if (space < avail) {
					avail = space;
				}
				n->m_len = avail;
				uint8_t *dst = n->m_data;
				uint8_t *src = m->m_data + off;
				if ((int32_t)dst < 0 || (int32_t)(dst + avail) < 0 ||
				    (int32_t)src < 0 || (int32_t)(src + avail) < 0) {
					set_k1(old_k1);
					return NULL;
				}
				memcpy(dst, src, avail);
			}
		} else {
			uint8_t *dst = n->m_data;
			uint8_t *src = m->m_data + off;
			if ((int32_t)dst < 0 || (int32_t)(dst + n->m_len) < 0 ||
			    (int32_t)src < 0 || (int32_t)(src + n->m_len) < 0) {
				set_k1(old_k1);
				return NULL;
			}
			memcpy(dst, src, n->m_len);
		}

		if (len != 1000000000) {
			len -= n->m_len;
		}
		off += n->m_len;
		if (off == m->m_len) {
			m = m->m_next;
			off = 0;
		}
		mp = &n->m_next;
	}

	if (top == NULL) {
		g_copym_drops++;
	}
	set_k1(old_k1);
	return top;
}

SceNetMbuf *sceNetIfhandle_lib_456E3146(SceNetMbuf *m, int off0, int len, int wait)
{
	return m_copym0(m, off0, len, wait, 0, 0);
}

SceNetMbuf *sceNetIfhandle_lib_E9BF5332(SceNetMbuf *m, int off0, int len, int wait)
{
	return m_copym0(m, off0, len, wait, 0, 1);
}

__attribute__((optimize("no-thread-jumps")))
int sceNetIfhandle_lib_5FB31C72(uint32_t *p0, uint32_t *p1, uint32_t *p2, uint32_t *p3)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if ((p0 != NULL && ((int32_t)(k1_shifted & (uint32_t)p0) < 0)) ||
	    (p1 != NULL && ((int32_t)(k1_shifted & (uint32_t)p1) < 0)) ||
	    (p2 != NULL && ((int32_t)(k1_shifted & (uint32_t)p2) < 0)) ||
	    (p3 != NULL && ((int32_t)(k1_shifted & (uint32_t)p3) < 0))) {
		set_k1(old_k1);
		return (int)0x80410003;
	}

	if (p0 != NULL) *p0 = g_mbuf_cfg0;
	if (p1 != NULL) *p1 = g_mbuf_cfg1;
	if (p2 != NULL) *p2 = g_mbuf_cfg2;
	if (p3 != NULL) *p3 = g_mbuf_cfg3;

	set_k1(old_k1);
	return 0;
}

int sceNetIfhandle_lib_62B20015(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);
	g_mbuf_cfg0 = v0;
	g_mbuf_cfg1 = v1;
	g_mbuf_cfg2 = v2;
	g_mbuf_cfg3 = v3;
	set_k1(old_k1);
	return 0;
}

SceNetMbuf *sceNetIfhandle_lib_E80F00A4(SceNetMbuf *m, int off, int len, int *offp)
{
	uint32_t old_k1 = get_k1();
	uint32_t k1_shifted = old_k1 << 11;
	set_k1(k1_shifted);

	if (m == NULL || (int32_t)m < 0 || (offp != NULL && ((int32_t)(k1_shifted & (uint32_t)offp) < 0))) {
		set_k1(old_k1);
		return NULL;
	}
	if (m == NULL) {
		Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf2.c", "sceNetMPulldown");
		for (;;) {}
	}
	if (len > 0x800) {
		sceNetIfhandle_lib_F56FAC82(m);
		set_k1(old_k1);
		return NULL;
	}

	SceNetMbuf *n = m;
	while (n != NULL && off > 0) {
		if ((int32_t)n < 0) {
			set_k1(old_k1);
			return NULL;
		}
		if (off < n->m_len) {
			break;
		}
		off -= n->m_len;
		n = n->m_next;
	}
	while (n != NULL && n->m_len == 0) {
		if ((int32_t)n < 0) {
			set_k1(old_k1);
			return NULL;
		}
		n = n->m_next;
	}
	if (n == NULL) {
		sceNetIfhandle_lib_F56FAC82(m);
		set_k1(old_k1);
		return NULL;
	}

	if ((off == 0 || offp != NULL) && len <= n->m_len - off) {
		goto ok;
	}

	int hlen = n->m_len - off;
	if (len < hlen) {
		SceNetMbuf *o = sceNetIfhandle_lib_456E3146(n, off, hlen, 1);
		if (o == NULL) {
			sceNetIfhandle_lib_F56FAC82(m);
			set_k1(old_k1);
			return NULL;
		}
		n->m_len = off;
		o->m_next = n->m_next;
		n->m_next = o;
		n = o;
		off = 0;
		goto ok;
	}

	int tlen = len - hlen;
	int tail_total = 0;
	for (SceNetMbuf *p = n->m_next; p != NULL; p = p->m_next) {
		if ((int32_t)p < 0) {
			set_k1(old_k1);
			return NULL;
		}
		tail_total += p->m_len;
	}
	if (hlen + tail_total < len) {
		sceNetIfhandle_lib_F56FAC82(m);
		set_k1(old_k1);
		return NULL;
	}

	int shared = 0;
	if (n->m_flags & M_EXT) {
		if (n->ext_free != NULL || n->ext_nextref != n) {
			shared = 1;
		}
	}

	if ((off == 0 || offp != NULL) && !shared) {
		int trailing;
		if (n->m_flags & M_EXT) {
			trailing = (int)((n->ext_buf + n->ext_size) - (n->m_data + n->m_len));
		} else {
			trailing = (int)(((uint8_t *)n + 0x100) - (n->m_data + n->m_len));
		}
		if (trailing >= tlen) {
			sceNetIfhandle_lib_9A6261EC(n->m_next, 0, tlen, n->m_data + n->m_len);
			n->m_len += tlen;
			sceNetIfhandle_lib_BFF3CEA5(n->m_next, tlen);
			goto ok;
		}
	}

	if ((off == 0 || offp != NULL) && !shared && n->m_next != NULL) {
		SceNetMbuf *next = n->m_next;
		int leading = 0;
		if ((next->m_flags & M_EXT) == 0) {
			if (next->m_flags & M_PKTHDR) {
				leading = (int)(next->m_data - ((uint8_t *)next + 0x30));
			} else {
				leading = (int)(next->m_data - ((uint8_t *)next + 0x14));
			}
			if (leading >= hlen) {
				if ((int32_t)next < 0) {
					set_k1(old_k1);
					return NULL;
				}
				next->m_data -= hlen;
				next->m_len += hlen;
				uint8_t *src = n->m_data + off;
				if ((int32_t)next->m_data < 0 || (int32_t)(next->m_data + hlen) < 0 ||
				    (int32_t)src < 0 || (int32_t)(src + hlen) < 0) {
					set_k1(old_k1);
					return NULL;
				}
				memcpy(next->m_data, src, hlen);
				n->m_len -= hlen;
				n = next;
				off = 0;
				goto ok;
			}
		}
	}

	SceNetMbuf *o = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (o == NULL) {
		sceNetIfhandle_lib_F56FAC82(m);
		set_k1(old_k1);
		return NULL;
	}
	o->m_type = m->m_type;
	o->m_data = (uint8_t *)o + 0x14;
	o->m_next = NULL;
	o->m_nextpkt = NULL;
	o->m_flags = 0;
	if ((uint32_t)len > 0xd0u) {
		uint8_t *buf = (uint8_t *)sceNetIfhandle_lib_15CFE3C0(0x800);
		if (buf == NULL) {
			buf = (uint8_t *)sceNetIfhandle_lib_15CFE3C0(0x800);
		}
		o->ext_buf = buf;
		if (buf != NULL) {
			o->m_data = buf;
			o->m_flags |= (M_EXT | M_RDONLY);
			o->ext_size = 0x800;
			o->ext_free = NULL;
			o->ext_arg = NULL;
			o->ext_prevref = o;
			o->ext_nextref = o;
		}
		if ((o->m_flags & M_EXT) == 0) {
			sceNetIfhandle_lib_F56FAC82(m);
			sceNetIfhandle_lib_F8825DC4(o);
			set_k1(old_k1);
			return NULL;
		}
	}

	o->m_len = hlen;
	uint8_t *src = n->m_data + off;
	if ((int32_t)o->m_data < 0 || (int32_t)(o->m_data + hlen) < 0 ||
	    (int32_t)src < 0 || (int32_t)(src + hlen) < 0) {
		set_k1(old_k1);
		return NULL;
	}
	memcpy(o->m_data, src, hlen);
	n->m_len -= hlen;
	sceNetIfhandle_lib_9A6261EC(n->m_next, 0, tlen, o->m_data + o->m_len);
	o->m_len += tlen;
	sceNetIfhandle_lib_BFF3CEA5(n->m_next, tlen);
	o->m_next = n->m_next;
	n->m_next = o;
	n = o;
	off = 0;

ok:
	if (offp != NULL) {
		*offp = off;
	}
	set_k1(old_k1);
	return n;
}

void sceNetIfhandle_lib_35FAB6A2(void)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);

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

	set_k1(old_k1);
}

void sceNetIfhandle_lib_0B258B5E(void)
{
	uint32_t old_k1 = get_k1();
	set_k1(old_k1 << 11);
	g_cur_game_wa = NULL;
	set_k1(old_k1);
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
