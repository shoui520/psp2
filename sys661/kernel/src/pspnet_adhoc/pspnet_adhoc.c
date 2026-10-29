#include <stdint.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

/* Imports */
extern void *sceKernelMemset(void *dst, int c, u32 n);
extern int _sceKernelCheckThreadStack(void);
extern void *sceKernelMemcpy(void *dst, const void *src, u32 n);
extern int sceKernelDelayThread(u32 delay);
extern int sceKernelSignalSema(int semaid, int signal);
extern int64_t sceKernelGetSystemTimeWide(void);
extern int sceNetGetLocalEtherAddr(u8 *addr);
extern int sceNetAdhocAuth_lib_015A8A64(u32 type, u8 *src, void *hdr, u32 len);
extern int sceNetAdhocAuth_lib_AAB06250(u32 type, void *hdr, u32 len, void *out);
extern int sceNetIfhandle_lib_07505747(int type);
extern void *sceNetIfhandle_lib_15CFE3C0(u32 size);
extern void *sceNetIfhandle_lib_3E8DD3F8(void *m, int len, int how);
extern void *sceNetIfhandle_lib_49EDBB18(void *m0, int off0, int len, int wait);
extern void sceNetIfhandle_lib_76BAD213(void *ptr);
extern void sceNetIfhandle_lib_9A6261EC(void *m, int len);
extern void *sceNetIfhandle_lib_BFF3CEA5(int how, int type);
extern void sceNetIfhandle_lib_F56FAC82(void *m);
extern void *sceNetIfhandle_lib_F8825DC4(void *m);
extern void *sceNet_lib_05D525E4(void *m, int len);
extern void sceNet_lib_0A5A8751(void);
extern int sceNet_lib_1858883D(void *addr);
extern int sceNet_lib_1F94AFD9(void *cb);
extern int sceNet_lib_25CC373A(void *a0, void *a1, u32 len);
extern int sceNet_lib_2E005032(void *cb);
extern void sceNet_lib_33B230BD(void *cb);
extern void sceNet_lib_515B2F33(void);
extern int sceNet_lib_6B294EE4(int wid);
extern int sceNet_lib_757085B0(int wid, int flag, u32 *timeout);
extern void sceNet_lib_7574FDA1(int wid);
extern void sceNet_lib_949F1FBB(int idx);
extern void sceNet_lib_94B44F26(void);
extern int sceNet_lib_A8B6205A(void *a0, void *a1, u32 len);
extern void *sceNet_lib_A93A93E9(u32 a0, void *cb, void *arg);
extern int sceNet_lib_B3A48B7F(int idx, void *rx_cb, void *ctrl_cb);
extern int sceNet_lib_CA3CF5EB(const u8 *mac);
extern int sceNet_lib_D60225A3(int wid);
extern void sceNet_lib_E0A81C7C(void *timer);
extern int sceNet_lib_F6DB0A0B(void *a0, u32 a1);
extern int sceWlanDevIsGameMode(void);
extern int sceWlanGPPrevEstablishActive(void *arg);
extern int sceWlanGPRecv(int id, void *buf, u32 len, void *info);
extern int sceWlanGPSend(int id, const void *buf, u32 len);

/* .rodata */
const u8 g_rodata_hdr[12] = {
	0x88, 0xc8, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff
};
const u16 g_bcast_tail = 0xffff;
const u8 g_zero_mac[6] = { 0, 0, 0, 0, 0, 0 };

/* .data */
s32 g_gm_master_id = -1;

/* .bss */
u32 g_adhoc_inited;
struct {
	void *head;
	void *tail;
	s32 count;
	s32 max_count;
	s32 drops;
} g_rx_queue;
void *g_sock_table[32];
void *g_pdp_list;
u32 g_pdp_port_rover;
void *g_ptp_list;
u32 g_ptp_port_rover;
void *g_gm_master;
void *g_gm_replica_list;
u32 g_gm_replica_count;
u32 g_pdp_stats[6];
u32 g_ptp_seq_seed;
u32 g_ptp_stats[31];

#define NOINLINE __attribute__((noinline))

static NOINLINE int sub_0270(void *m);
static NOINLINE void sub_036c(void *m);
static NOINLINE void sub_047c(void);
static NOINLINE void sub_0490(int a0, u8 *a1);
static NOINLINE int sub_0974(u32 count, void *p1, void *p2);
static NOINLINE int sub_0dd0(void);
static NOINLINE void sub_0e18(void);
static NOINLINE void *sub_0e90(int type, int id);
static NOINLINE int sub_0ed8(int type);
static NOINLINE int sub_0f60(int type, void **out);
static NOINLINE int sub_0ffc(int id, int type);
static NOINLINE int sub_1044(void);
static NOINLINE int sub_1078(int a0, int a1, int semaid);
static NOINLINE int sub_1620(void *pdp, const u8 *dmac, u16 dport, const void *data, int len, int nonblock, int *wait_wid);
static NOINLINE void *sub_1c48(void *pdp, const u8 *dmac, u16 dport, const void *data, u32 len);
static NOINLINE int sub_20c4(int id, u8 *smac, u16 *sport, void *buf, int *len, u32 timeout, int flag, int mode);
static NOINLINE void sub_225c(void *pdp);
static NOINLINE void sub_231c(void *m);
static NOINLINE int sub_29ac(void);
static NOINLINE void sub_2a18(void);
static NOINLINE void *sub_2a9c(const u8 *mac, u16 port);
static NOINLINE int sub_2b2c(void *sock, const u8 *mac, u16 port, u32 bufsize, int flag);
static NOINLINE int sub_2d04(void *pdp);
static NOINLINE void sub_2d50(void *pdp);
static NOINLINE void sub_2d5c(int code, const u8 *mac);
static NOINLINE void sub_2dc4(void);
static NOINLINE int sub_2e78(const u8 *mac);
static NOINLINE int sub_3afc(int type, const u8 *smac, u16 sport, const u8 *dmac, u16 dport, u32 bufsize, u32 delay, int count, int flag, void **out);
static NOINLINE void sub_3c44(void *ptp);
static NOINLINE int sub_3c7c(void *ptp, u32 timeout, int flag);
static NOINLINE int sub_3e30(void *ptp, void *peer_mac, u16 *peer_port, u32 timeout, int flag);
static NOINLINE int sub_40ec(void *ptp, const void *data, int *len, u32 timeout, int flag);
static NOINLINE int sub_44d4(void *ptp, void *buf, int *len, u32 timeout, int flag);
static NOINLINE int sub_4808(void *ptp, u32 timeout, int flag);
static NOINLINE int sub_4a30(void *ptp, u8 flags, u32 seq, u32 ack, u16 win);
static NOINLINE int sub_4ad4(const u8 *smac, const u8 *dmac, u16 sport, u16 dport, u8 flags, u32 seq, u32 ack, u16 win);
static NOINLINE int sub_4b7c(void *m, const u8 *smac, const u8 *dmac, u16 sport, u16 dport, u8 flags, u32 seq, u32 ack, u16 win);
static NOINLINE int sub_4d48(void *ptp);
static NOINLINE void sub_4e3c(void *ptp);
static NOINLINE void sub_4fc8(int code, const u8 *mac);
static NOINLINE int sub_5288(void);
static NOINLINE void sub_52e0(void);
static NOINLINE void *sub_536c(const u8 *smac, u16 sport, const u8 *dmac, u16 dport);
static NOINLINE void *sub_542c(const u8 *smac, u16 sport);
static NOINLINE int sub_5590(void *ptp);
static NOINLINE void sub_55dc(void *ptp);
static NOINLINE void sub_55f8(void *ptp);
static NOINLINE int sub_5604(void *ptp, const u8 *smac, u16 sport, const u8 *dmac, u16 dport);
static NOINLINE int sub_572c(void *ptp, const u8 *smac, u16 sport);
static NOINLINE void sub_5750(void *ptp);
static NOINLINE void sub_584c(void);
static NOINLINE void sub_5938(void *m);
static NOINLINE int sub_637c(void *ptp, void *m, void *hdr, int len, u32 seq, u32 ack, u8 flags, u16 win);
static NOINLINE void sub_668c(void *ptp, int event);
static NOINLINE void sub_6878(void *ptp, int idx, u32 ticks);
static NOINLINE int sub_6a54(u32 a0, void *arg);
static NOINLINE int sub_6b20(u32 a0, void *arg);
static NOINLINE int sub_7008(void);
static NOINLINE void sub_7020(void);
static NOINLINE int sub_7188(const u8 *mac, const void *data, u32 size);
static NOINLINE int sub_72a4(int id);
static NOINLINE int sub_7338(int id, void *data, int mode);

int sceNetAdhocInit(void)
{
	int res;

	if (_sceKernelCheckThreadStack() < 0x7f0)
		return 0x80410005;
	sceNet_lib_94B44F26();
	if (g_adhoc_inited) {
		res = 0x80410712;
		goto out;
	}
	sceKernelMemset(&g_rx_queue, 0, sizeof(g_rx_queue));
	g_rx_queue.max_count = 50;
	res = sub_0dd0();
	if (res < 0)
		goto err0;
	res = sub_29ac();
	if (res < 0)
		goto err1;
	res = sub_5288();
	if (res < 0)
		goto err2;
	res = sceNet_lib_B3A48B7F(0, (void *)sub_0270, (void *)sub_0490);
	if (res < 0)
		goto err3;
	res = sceNet_lib_1F94AFD9((void *)sub_047c);
	if (res < 0)
		goto err4;
	res = sub_7008();
	if (res < 0)
		goto err5;
	g_adhoc_inited = 1;
	res = 0;
	goto out;

err5:
	sceNet_lib_0A5A8751();
err4:
	sceNet_lib_949F1FBB(0);
err3:
	sub_52e0();
err2:
	sub_2a18();
err1:
	sub_0e18();
err0:
out:
	sceNet_lib_515B2F33();
	return res;
}

int sceNetAdhocTerm(void)
{
	int res = 0;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	sceNet_lib_94B44F26();
	if (g_adhoc_inited) {
		if (sub_1044()) {
			res = 0x80410714;
		} else {
			sub_7020();
			sceNet_lib_0A5A8751();
			sceNet_lib_949F1FBB(0);
			sub_52e0();
			sub_2a18();
			sub_0e18();
			while (g_rx_queue.head != 0) {
				void **m = (void **)g_rx_queue.head;
				void *next = m[1];
				m[1] = 0;
				g_rx_queue.head = next;
				if (next == 0)
					g_rx_queue.tail = 0;
				g_rx_queue.count--;
				sceNetIfhandle_lib_F56FAC82(m);
			}
			sceKernelMemset(&g_rx_queue, 0, sizeof(g_rx_queue));
			g_adhoc_inited = 0;
		}
	}
	sceNet_lib_515B2F33();
	return res;
}

static NOINLINE int sub_0270(void *m)
{
	if (m == 0)
		return -1;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited || g_rx_queue.count >= g_rx_queue.max_count) {
		g_rx_queue.drops++;
		sceNetIfhandle_lib_F56FAC82(m);
		sceNet_lib_515B2F33();
		return 0;
	}
	((void **)m)[1] = 0;
	if (g_rx_queue.tail)
		((void **)g_rx_queue.tail)[1] = m;
	else
		g_rx_queue.head = m;
	g_rx_queue.tail = m;
	g_rx_queue.count++;
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE void sub_036c(void *m)
{
	u8 *d = ((u8 **)m)[2];
	u32 len = ((u32 *)m)[3];
	if (len >= 0x10 && d[12] == g_rodata_hdr[0] && d[13] == g_rodata_hdr[1]) {
		if (d[14] == 1)
			sub_231c(m);
		else if (d[14] == 2)
			sub_5938(m);
		else
			sceNetIfhandle_lib_F56FAC82(m);
	} else {
		sceNetIfhandle_lib_F56FAC82(m);
	}
}

static NOINLINE void sub_047c(void)
{
	sceNet_lib_94B44F26();
	while (g_rx_queue.head) {
		void *m = g_rx_queue.head;
		g_rx_queue.head = ((void **)m)[1];
		if (!g_rx_queue.head)
			g_rx_queue.tail = 0;
		g_rx_queue.count--;
		sub_036c(m);
	}
	sceNet_lib_515B2F33();
}

static NOINLINE void sub_0490(int a0, u8 *a1)
{
	sceNet_lib_94B44F26();
	if (a0 == 0) {
		sub_2dc4();
		sub_584c();
	} else {
		sub_2d5c(a0, a1);
		sub_4fc8(a0, a1);
	}
	sceNet_lib_515B2F33();
}

int sceNetAdhoc_7A662D6B(void *sds, int nsds, u32 timeout, int nonblock)
{
	int res = 0;

	if (_sceKernelCheckThreadStack() < 0x7c0)
		return 0x80410005;
	if (!sds || nsds <= 0 || (nonblock & ~1) != 0)
		return 0x80410711;
	int wctx = sceNet_lib_CA3CF5EB((const u8 *)&res);
	if (!wctx)
		return res;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		res = 0x80410712;
		goto out_unlock;
	}
	u32 *pairs = (u32 *)sceNetIfhandle_lib_15CFE3C0((u32)nsds * 8);
	int64_t t_start = 0;
	if (!pairs) {
		res = 0x80410001;
		goto out_unlock;
	}
	u32 *sd_ptr = (u32 *)sds;
	for (int i = 0; i < nsds; i++, sd_ptr += 3) {
		void *found = sub_0e90(0, (int)sd_ptr[0]);
		if (found) {
			pairs[i * 2] = (u32)found;
			pairs[i * 2 + 1] = (u32)sd_ptr;
		} else {
			res = 0x80410701;
			((u32 *)sds)[2] = 0x100;
		}
	}
	if (res < 0)
		goto out_free;
	for (int i = 0; i < nsds; i++) {
		u32 *s = (u32 *)pairs[i * 2];
		u32 *sd = (u32 *)pairs[i * 2 + 1];
		if (s[3] & 0x40) {
			res = 0x80410717;
			sd[2] = 0x400;
		}
		if (s[3] & 0x8000) {
			res = 0x80410717;
			sd[2] = 0x800;
		}
	}
	if (res < 0)
		goto out_free;

	for (;;) {
		res = sub_0974((u32)nsds, pairs, sds);
		if (res != 0 || (nonblock & 1))
			goto out_free;
		u32 rem = 0;
		if (timeout != 0) {
			if (t_start == 0)
				t_start = sceKernelGetSystemTimeWide();
			int64_t now = sceKernelGetSystemTimeWide();
			u64 elapsed = (u64)(now - t_start);
			if (elapsed >= timeout) {
				res = 0x80410715;
				goto out_free;
			}
			rem = timeout - (u32)elapsed;
		}
		for (int i = 0; i < nsds; i++) {
			u32 *s = (u32 *)pairs[i * 2];
			((u32 *)wctx)[22] = s[9];
			s[9] = (u32)wctx;
		}
		res = sceNet_lib_F6DB0A0B(&g_gm_replica_count, rem);
		for (int i = 0; i < nsds; i++) {
			u32 *s = (u32 *)pairs[i * 2];
			u32 *sd = (u32 *)pairs[i * 2 + 1];
			u32 w = s[9];
			if (w == (u32)wctx) {
				s[9] = ((u32 *)wctx)[22];
			} else if (w) {
				u32 *prev = (u32 *)w;
				while (prev[22] && prev[22] != (u32)wctx)
					prev = (u32 *)prev[22];
				if (prev[22] == (u32)wctx)
					prev[22] = ((u32 *)wctx)[22];
			}
			if (s[3] & 0x40) {
				sd[2] = 0x400;
				res = 5;
			}
			if (s[3] & 0x8000) {
				sd[2] = 0x800;
				res = 5;
			}
			if (s[3] & 1) {
				sd[2] = 0x200;
				res = 5;
				if (s[9] == 0) {
					u8 stype = ((u8 *)s)[16];
					if (stype == 1)
						sub_2d04(s);
					else if (stype == 2)
						sub_5590(s);
					if (s[5] == 0)
						sceNetIfhandle_lib_76BAD213(s);
				}
			}
		}
		if (res != 0) {
			if (res == 5)
				res = 0x80410717;
			else if (res == 4)
				res = 0x80410719;
			else if (res == 11)
				res = 0x80410715;
			else
				res = 0x80410002;
			goto out_free;
		}
	}

out_free:
	sceNetIfhandle_lib_76BAD213(pairs);
out_unlock:
	sceNet_lib_515B2F33();
	sceNet_lib_757085B0(wctx, 0, 0);
	return res;
}

static NOINLINE int sub_0974(u32 count, void *p1, void *p2)
{
	int ready = 0;
	u32 *sds = (u32 *)p1;
	u32 *aux = (u32 *)p2;
	for (u32 i = 0; i < count; i++) {
		void *s = sub_0e90(0, (int)sds[i * 3]);
		sds[i * 3 + 2] = 0;
		if (s) {
			u32 ev = sds[i * 3 + 1];
			if (ev & 1) {
				sds[i * 3 + 2] |= 1;
				aux[i] = 1;
				ready++;
			}
		}
	}
	return ready;
}

int sceNetAdhoc_73BFD52D(int id, int flag)
{
	int changed = 0;
	int res = 0;

	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		res = 0x80410712;
		goto out;
	}
	u32 *s = (u32 *)sub_0e90(0, id);
	if (!s) {
		res = 0x80410701;
		goto out;
	}
	u32 cur = s[3];
	if (flag & 0x10) {
		if (!(cur & 0x10)) {
			cur |= 0x10;
			s[3] = cur;
			changed++;
		}
	} else {
		cur &= ~0x10u;
		s[3] = cur;
	}
	if (flag & 0x20) {
		if (!(cur & 0x20)) {
			cur |= 0x20;
			s[3] = cur;
			changed++;
		}
	} else {
		cur &= ~0x20u;
		s[3] = cur;
	}
	if (flag & 0x40) {
		if (!(cur & 0x40)) {
			cur |= 0x40;
			s[3] = cur;
			changed++;
		}
	} else {
		cur &= ~0x40u;
		s[3] = cur;
	}
	if (flag & 0x80) {
		if (!(cur & 0x80)) {
			cur |= 0x80;
			s[3] = cur;
			changed++;
		}
	} else {
		cur &= ~0x80u;
		s[3] = cur;
	}
	if (flag & 0x100) {
		if (!(cur & 0x100)) {
			cur |= 0x100;
			s[3] = cur;
			changed++;
		}
	} else {
		cur &= ~0x100u;
		s[3] = cur;
	}
	if (flag & 0x200) {
		if (!(cur & 0x200)) {
			cur |= 0x200;
			s[3] = cur;
			changed++;
		}
	} else {
		cur &= ~0x200u;
		s[3] = cur;
	}
	if (changed != 0)
		sceNet_lib_6B294EE4(id);

out:
	sceNet_lib_515B2F33();
	return res;
}

int sceNetAdhoc_4D2CE199(int id, int *flag)
{
	if (_sceKernelCheckThreadStack() < 0x7c0)
		return 0x80410005;
	if (id <= 0 || id > 32)
		return 0x80410701;
	if (!flag)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *s = sub_0e90(0, id);
	if (!s) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	*flag = ((int *)s)[2];
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE int sub_0dd0(void)
{
	sceKernelMemset(g_sock_table, 0, sizeof(g_sock_table));
	return sceNet_lib_2E005032((void *)sub_1078);
}

static NOINLINE void sub_0e18(void)
{
	sceNet_lib_33B230BD((void *)sub_1078);
	for (int i = 0; i < 32; i++) {
		if (g_sock_table[i]) {
			sceNetIfhandle_lib_76BAD213(g_sock_table[i]);
			g_sock_table[i] = 0;
		}
	}
}

static NOINLINE void *sub_0e90(int type, int id)
{
	if (id <= 0 || id > 32)
		return 0;
	void *s = g_sock_table[id - 1];
	if (!s)
		return 0;
	if (type != 0 && ((u8 *)s)[4] != (u8)type)
		return 0;
	return s;
}

static NOINLINE int sub_0ed8(int type)
{
	for (int i = 0; i < 32; i++) {
		if (!g_sock_table[i])
			return i + 1;
	}
	return (type == 1) ? 0x80410707 : 0x80410708;
}

static NOINLINE int sub_0f60(int type, void **out)
{
	int idx = -1;
	for (int i = 0; i < 32; i++) {
		if (!g_sock_table[i]) {
			idx = i;
			break;
		}
	}
	if (idx < 0)
		return (type == 1) ? 0x80410707 : 0x80410708;
	u32 sz = (type == 1) ? 0x38 : 0x88;
	void *s = sceNetIfhandle_lib_15CFE3C0(sz);
	if (!s)
		return 0x80410710;
	sceKernelMemset(s, 0, sz);
	((int *)s)[0] = idx + 1;
	((u8 *)s)[4] = (u8)type;
	g_sock_table[idx] = s;
	*out = s;
	return idx + 1;
}

static NOINLINE int sub_0ffc(int id, int type)
{
	void *s = sub_0e90(type, id);
	if (!s)
		return 0x80410701;
	g_sock_table[id - 1] = 0;
	sceNetIfhandle_lib_76BAD213(s);
	return 0;
}

static NOINLINE int sub_1044(void)
{
	for (int i = 0; i < 32; i++) {
		if (g_sock_table[i])
			return 1;
	}
	return 0;
}

static NOINLINE int sub_1078(int a0, int a1, int semaid)
{
	(void)a0;
	(void)a1;
	return sceKernelSignalSema(semaid, 1);
}

int sceNetAdhocPdpCreate(const u8 *mac, u16 port, u32 bufsize, int flag)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (!mac || bufsize == 0 || flag != 0)
		return 0x80410709;
	if (sceNetIfhandle_lib_07505747(3) != 0)
		sceKernelDelayThread(100000);
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	if (sub_2a9c(mac, port)) {
		sceNet_lib_515B2F33();
		return 0x80410706;
	}
	void *pdp = 0;
	int id = sub_0f60(1, &pdp);
	if (id >= 0) {
		int res = sub_2b2c(pdp, mac, port, bufsize, flag);
		if (res < 0) {
			sub_0ffc(id, 1);
			id = res;
		} else {
			sub_2d50(pdp);
		}
	}
	sceNet_lib_515B2F33();
	return id;
}

int sceNetAdhocPdpSend(int id, const u8 *dmac, u16 dport, const void *data, int len, u32 timeout, int nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	if (!dmac || !data || len < 0 || len > 0xffff || dport == 0)
		return 0x80410709;
	if (sceNet_lib_CA3CF5EB(dmac) < 0)
		return 0x80410703;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *pdp = sub_0e90(1, id);
	if (!pdp) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	int wid = 0;
	int res = sub_1620(pdp, dmac, dport, data, len, nonblock, &wid);
	if (res == 0 && wid > 0) {
		u32 t = timeout;
		res = sceNet_lib_757085B0(wid, 1, &t);
	}
	sceNet_lib_515B2F33();
	return res;
}

int sceNetAdhocPdpDelete(int id, int flag)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (flag != 0)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *pdp = sub_0e90(1, id);
	if (!pdp) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	int wid = ((int *)pdp)[3];
	if (wid > 0) {
		sceNet_lib_D60225A3(wid);
		sceNet_lib_7574FDA1(wid);
	}
	sub_225c(pdp);
	sub_2d04(pdp);
	sub_0ffc(id, 1);
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE int sub_1620(void *pdp, const u8 *dmac, u16 dport, const void *data, int len, int nonblock, int *wait_wid)
{
	*wait_wid = 0;
	void *m = sub_1c48(pdp, dmac, dport, data, (u32)len);
	if (!m)
		return nonblock ? 0x8041070b : 0x80410710;
	int res = sceNet_lib_A8B6205A(pdp, m, (u32)len);
	g_pdp_stats[0]++;
	g_pdp_stats[1] += (u32)len;
	return res;
}

static NOINLINE void *sub_1c48(void *pdp, const u8 *dmac, u16 dport, const void *data, u32 len)
{
	void *m = sceNetIfhandle_lib_BFF3CEA5(1, 1);
	if (!m)
		return 0;
	void *m2 = sceNet_lib_05D525E4(m, (int)len + 24);
	if (!m2) {
		sceNetIfhandle_lib_F56FAC82(m);
		return 0;
	}
	u8 *hdr = ((u8 **)m2)[2];
	sceKernelMemcpy(hdr, dmac, 6);
	sceKernelMemcpy(hdr + 6, (u8 *)pdp + 16, 6);
	*(u16 *)(hdr + 18) = dport;
	if (len > 0)
		sceKernelMemcpy(hdr + 24, data, len);
	return m2;
}

int sceNetAdhocPdpRecv(int id, u8 *smac, u16 *sport, void *buf, int *len, u32 timeout, int nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	if (!smac || !sport || !buf || !len || *len <= 0)
		return 0x80410709;
	return sub_20c4(id, smac, sport, buf, len, timeout, nonblock, 0);
}

int sceNetAdhoc_67346A2A(int id, u8 *smac, u16 *sport, void *buf, int *len, u32 timeout, int nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	return sub_20c4(id, smac, sport, buf, len, timeout, nonblock, 1);
}

int sceNetAdhoc_lib_F9240FBA(void)
{
	sceKernelMemset(g_pdp_stats, 0, 0x18);
	return 0;
}

int sceNetAdhoc_lib_1059683E(void *dst)
{
	if (!dst)
		return -1;
	sceKernelMemcpy(dst, g_pdp_stats, 0x18);
	return 0;
}

static NOINLINE int sub_20c4(int id, u8 *smac, u16 *sport, void *buf, int *len, u32 timeout, int flag, int mode)
{
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *pdp = sub_0e90(1, id);
	if (!pdp) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	void *m = ((void **)pdp)[6];
	if (!m) {
		if (flag) {
			sceNet_lib_515B2F33();
			return 0x8041070b;
		}
		int wid = ((int *)pdp)[3];
		u32 t = timeout;
		int r = sceNet_lib_757085B0(wid, 1, &t);
		if (r < 0) {
			sceNet_lib_515B2F33();
			return r;
		}
		m = ((void **)pdp)[6];
	}
	if (m) {
		((void **)pdp)[6] = ((void **)m)[1];
		if (mode == 0 && buf && len)
			sceKernelMemcpy(buf, ((void **)m)[2], (u32)*len);
		if (smac)
			sceKernelMemcpy(smac, (u8 *)((void **)m)[2] + 6, 6);
		if (sport)
			*sport = *(u16 *)((u8 *)((void **)m)[2] + 16);
		sceNetIfhandle_lib_F56FAC82(m);
	}
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE void sub_225c(void *pdp)
{
	void *m = ((void **)pdp)[6];
	while (m) {
		void *next = ((void **)m)[1];
		sceNetIfhandle_lib_F56FAC82(m);
		m = next;
	}
	((void **)pdp)[6] = 0;
	((u32 *)pdp)[7] = 0;
}

static NOINLINE void sub_231c(void *m)
{
	void *m2 = sceNetIfhandle_lib_3E8DD3F8(m, 24, 1);
	if (!m2)
		return;
	u8 *hdr = ((u8 **)m2)[2];
	u16 dport = *(u16 *)(hdr + 18);
	void *pdp = sub_2a9c(hdr, dport);
	if (!pdp) {
		if (sub_2e78(hdr)) {
			void *clone = sceNetIfhandle_lib_49EDBB18(m2, 0, -1, 1);
			if (clone)
				sceNetIfhandle_lib_F56FAC82(clone);
		}
		sceNetIfhandle_lib_F56FAC82(m2);
		return;
	}
	sceNetIfhandle_lib_9A6261EC(m2, 24);
	void *m3 = sceNetIfhandle_lib_F8825DC4(m2);
	if (m3) {
		((void **)m3)[1] = ((void **)pdp)[6];
		((void **)pdp)[6] = m3;
		g_pdp_stats[2]++;
	}
}

int sceNetAdhocGetPdpStat(int *buflen, void *buf)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (!buflen)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	int max_entries = *buflen / 20;
	int count = 0;
	u8 *dst = (u8 *)buf;
	for (void **p = (void **)g_pdp_list; p; p = (void **)p[0]) {
		if (buf && count < max_entries) {
			sceKernelMemset(dst, 0, 20);
			*(int *)(dst + 4) = ((int *)p)[1];
			sceKernelMemcpy(dst + 8, (u8 *)p + 16, 6);
			*(u16 *)(dst + 14) = *(u16 *)((u8 *)p + 22);
			*(u32 *)(dst + 16) = ((u32 *)p)[7];
			dst += 20;
		}
		count++;
	}
	*buflen = count * 20;
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE int sub_29ac(void)
{
	u8 mac[8];
	g_pdp_list = 0;
	int r = sceNet_lib_1858883D(mac);
	if (r < 0)
		return r;
	g_pdp_port_rover = ((u32)mac[4] << 8) | mac[5];
	return 0;
}

static NOINLINE void sub_2a18(void)
{
	while (g_pdp_list) {
		void *p = g_pdp_list;
		sub_225c(p);
		sub_2d04(p);
	}
}

static NOINLINE void *sub_2a9c(const u8 *mac, u16 port)
{
	for (void **p = (void **)g_pdp_list; p; p = (void **)p[0]) {
		if (*(u16 *)((u8 *)p + 22) == port) {
			const u8 *pm = (const u8 *)p + 16;
			if (pm[0] == mac[0] && pm[1] == mac[1] && pm[2] == mac[2] &&
			    pm[3] == mac[3] && pm[4] == mac[4] && pm[5] == mac[5])
				return p;
		}
	}
	return 0;
}

static NOINLINE int sub_2b2c(void *sock, const u8 *mac, u16 port, u32 bufsize, int flag)
{
	(void)flag;
	((int *)sock)[3] = 1;
	sceKernelMemcpy((u8 *)sock + 16, mac, 6);
	if (port == 0) {
		g_pdp_port_rover = (g_pdp_port_rover + 1) | 0xc000;
		port = (u16)g_pdp_port_rover;
	}
	*(u16 *)((u8 *)sock + 22) = port;
	((u32 *)sock)[8] = bufsize;
	return 0;
}

static NOINLINE int sub_2d04(void *pdp)
{
	void **pp = &g_pdp_list;
	while (*pp) {
		if (*pp == pdp) {
			*pp = ((void **)pdp)[0];
			return 0;
		}
		pp = (void **)*pp;
	}
	return -1;
}

static NOINLINE void sub_2d50(void *pdp)
{
	((void **)pdp)[0] = g_pdp_list;
	g_pdp_list = pdp;
}

static NOINLINE void sub_2d5c(int code, const u8 *mac)
{
	(void)code;
	(void)mac;
	for (void **p = (void **)g_pdp_list; p; p = (void **)p[0]) {
		int wid = ((int *)p)[3];
		if (wid > 0)
			sceNet_lib_6B294EE4(wid);
	}
}

static NOINLINE void sub_2dc4(void)
{
	for (void **p = (void **)g_pdp_list; p; p = (void **)p[0]) {
		int wid = ((int *)p)[3];
		if (wid > 0)
			sceNet_lib_6B294EE4(wid);
	}
}

static NOINLINE int sub_2e78(const u8 *mac)
{
	for (int i = 0; i < 6; i++) {
		if (mac[i] != 0xff)
			return 0;
	}
	return 1;
}

int sceNetAdhocPtpOpen(const u8 *smac, u16 sport, const u8 *dmac, u16 dport, u32 bufsize, u32 delay, int count, int flag)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	if (sceNetIfhandle_lib_07505747(0) != 0)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *ptp = 0;
	int id = sub_3afc(2, smac, sport, dmac, dport, bufsize, delay, count, flag, &ptp);
	if (id >= 0) {
		int r = sub_5604(ptp, smac, sport, dmac, dport);
		if (r < 0) {
			sub_3c44(ptp);
			id = r;
		}
	}
	sceNet_lib_515B2F33();
	return id;
}

int sceNetAdhocPtpConnect(int id, u32 timeout, int nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	if (sceNetIfhandle_lib_07505747(4) != 0)
		sceKernelDelayThread(100000);
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *ptp = sub_0e90(2, id);
	if (!ptp) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	int res = sub_3c7c(ptp, timeout, nonblock);
	sceNet_lib_515B2F33();
	return res;
}

int sceNetAdhocPtpListen(const u8 *smac, u16 sport, u32 bufsize, u32 delay, int count, int backlog, int flag)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	if (backlog <= 0)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *ptp = 0;
	int id = sub_3afc(2, smac, sport, g_zero_mac, 0, bufsize, delay, count, flag, &ptp);
	if (id >= 0) {
		int r = sub_572c(ptp, smac, sport);
		if (r < 0) {
			sub_3c44(ptp);
			id = r;
		}
	}
	sceNet_lib_515B2F33();
	return id;
}

int sceNetAdhocPtpAccept(int id, void *peer_mac, u16 *peer_port, u32 timeout, int nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	if (sceNetIfhandle_lib_07505747(2) != 0)
		sceKernelDelayThread(100000);
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *ptp = sub_0e90(2, id);
	if (!ptp) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	int res = sub_3e30(ptp, peer_mac, peer_port, timeout, nonblock);
	sceNet_lib_515B2F33();
	return res;
}

int sceNetAdhocPtpSend(int id, const void *data, int *len, u32 timeout, int nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	if (!data || !len || *len < 0)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *ptp = sub_0e90(2, id);
	if (!ptp) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	int res = sub_40ec(ptp, data, len, timeout, nonblock);
	sceNet_lib_515B2F33();
	return res;
}

int sceNetAdhocPtpRecv(int id, void *buf, int *len, u32 timeout, int nonblock)
{
	if (_sceKernelCheckThreadStack() < 0x800)
		return 0x80410005;
	if (!buf || !len || *len <= 0)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *ptp = sub_0e90(2, id);
	if (!ptp) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	int res = sub_44d4(ptp, buf, len, timeout, nonblock);
	sceNet_lib_515B2F33();
	return res;
}

int sceNetAdhocPtpFlush(int id, u32 timeout, int nonblock)
{
	int res = 0;

	if (_sceKernelCheckThreadStack() < 0x7d0)
		return 0x80410005;
	if ((nonblock & ~1) != 0)
		return 0x80410711;
	int wctx = sceNet_lib_CA3CF5EB((const u8 *)&res);
	if (!wctx)
		return res;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		res = 0x80410712;
		goto out_unlock;
	}
	u32 *ptp = (u32 *)sub_0e90(2, id);
	if (!ptp) {
		res = 0x80410701;
		goto out_unlock;
	}
	if (!(ptp[3] & 0xc000)) {
		res = 0x8041070b;
		goto out_unlock;
	}
	((u32 *)wctx)[22] = ptp[9];
	ptp[9] = (u32)wctx;
	res = sub_4808(ptp, timeout, nonblock);
	if (ptp[9] == (u32)wctx) {
		ptp[9] = ((u32 *)wctx)[22];
	} else if (ptp[9]) {
		u32 *prev = (u32 *)ptp[9];
		while (prev[22] && prev[22] != (u32)wctx)
			prev = (u32 *)prev[22];
		if (prev[22] == (u32)wctx)
			prev[22] = ((u32 *)wctx)[22];
	}
	if (ptp[3] & 1) {
		if (ptp[9] == 0) {
			sub_5590(ptp);
			sub_0ffc(id, 2);
		}
		res = 0x80410707;
	}

out_unlock:
	sceNet_lib_515B2F33();
	sceNet_lib_757085B0(wctx, 0, 0);
	return res;
}

int sceNetAdhocPtpClose(int id, int flag)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (flag != 0)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	void *ptp = sub_0e90(2, id);
	if (!ptp) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	sub_4e3c(ptp);
	sub_5750(ptp);
	sub_5590(ptp);
	sub_0ffc(id, 2);
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE int sub_3afc(int type, const u8 *smac, u16 sport, const u8 *dmac, u16 dport, u32 bufsize, u32 delay, int count, int flag, void **out)
{
	(void)smac;
	(void)sport;
	(void)dmac;
	(void)dport;
	(void)flag;
	if (bufsize == 0 || delay == 0 || count <= 0)
		return 0x80410709;
	int id = sub_0f60(type, out);
	if (id < 0)
		return id;
	void *ptp = *out;
	((u32 *)ptp)[10] = bufsize;
	((u32 *)ptp)[11] = delay;
	((int *)ptp)[12] = count;
	((void **)ptp)[20] = sceNet_lib_A93A93E9(0, (void *)sub_6a54, ptp);
	((void **)ptp)[21] = sceNet_lib_A93A93E9(0, (void *)sub_6b20, ptp);
	sub_55f8(ptp);
	return id;
}

static NOINLINE void sub_3c44(void *ptp)
{
	int id = ((int *)ptp)[0];
	sub_5590(ptp);
	sub_0ffc(id, 2);
}

static NOINLINE int sub_3c7c(void *ptp, u32 timeout, int flag)
{
	sub_668c(ptp, 1);
	sub_6878(ptp, 0, ((u32 *)ptp)[11]);
	int r = sub_4a30(ptp, 1, ((u32 *)ptp)[14], 0, (u16)((u32 *)ptp)[10]);
	if (r < 0 || flag)
		return r;
	int wid = ((int *)ptp)[3];
	u32 t = timeout;
	return sceNet_lib_757085B0(wid, 1, &t);
}

static NOINLINE int sub_3e30(void *ptp, void *peer_mac, u16 *peer_port, u32 timeout, int flag)
{
	if (!flag) {
		int wid = ((int *)ptp)[3];
		u32 t = timeout;
		int r = sceNet_lib_757085B0(wid, 1, &t);
		if (r < 0)
			return r;
	}
	if (peer_mac)
		sceKernelMemcpy(peer_mac, (u8 *)ptp + 24, 6);
	if (peer_port)
		*peer_port = *(u16 *)((u8 *)ptp + 30);
	return ((int *)ptp)[0];
}

static NOINLINE int sub_40ec(void *ptp, const void *data, int *len, u32 timeout, int flag)
{
	(void)data;
	int r = sub_4d48(ptp);
	if (r < 0 && !flag) {
		int wid = ((int *)ptp)[3];
		u32 t = timeout;
		r = sceNet_lib_757085B0(wid, 1, &t);
	}
	g_ptp_stats[0] += (u32)*len;
	return r;
}

static NOINLINE int sub_44d4(void *ptp, void *buf, int *len, u32 timeout, int flag)
{
	void *m = ((void **)ptp)[6];
	if (!m && !flag) {
		int wid = ((int *)ptp)[3];
		u32 t = timeout;
		int r = sceNet_lib_757085B0(wid, 1, &t);
		if (r < 0)
			return r;
		m = ((void **)ptp)[6];
	}
	if (!m)
		return 0x8041070b;
	((void **)ptp)[6] = ((void **)m)[1];
	sceKernelMemcpy(buf, ((void **)m)[2], (u32)*len);
	sceNetIfhandle_lib_F56FAC82(m);
	return 0;
}

static NOINLINE int sub_4808(void *ptp, u32 timeout, int flag)
{
	if (((u32 *)ptp)[7] == 0)
		return 0;
	if (flag)
		return 0x8041070b;
	int wid = ((int *)ptp)[3];
	u32 t = timeout;
	return sceNet_lib_757085B0(wid, 1, &t);
}

int sceNetAdhoc_lib_62B875A8(void)
{
	sceKernelMemset(g_ptp_stats, 0, 0x7c);
	return 0;
}

int sceNetAdhoc_lib_2CA12790(void *dst)
{
	if (!dst)
		return -1;
	sceKernelMemcpy(dst, g_ptp_stats, 0x7c);
	return 0;
}

static NOINLINE int sub_4a30(void *ptp, u8 flags, u32 seq, u32 ack, u16 win)
{
	void *m = sceNetIfhandle_lib_BFF3CEA5(1, 1);
	if (!m)
		return 0x80410710;
	int r = sub_4b7c(m, (const u8 *)ptp + 16, (const u8 *)ptp + 24, *(u16 *)((u8 *)ptp + 22), *(u16 *)((u8 *)ptp + 30), flags, seq, ack, win);
	if (r < 0)
		return r;
	return sceNet_lib_25CC373A(ptp, m, 32);
}

static NOINLINE int sub_4ad4(const u8 *smac, const u8 *dmac, u16 sport, u16 dport, u8 flags, u32 seq, u32 ack, u16 win)
{
	void *m = sceNetIfhandle_lib_BFF3CEA5(1, 1);
	if (!m)
		return 0x80410710;
	int r = sub_4b7c(m, smac, dmac, sport, dport, flags, seq, ack, win);
	if (r < 0)
		return r;
	return sceNet_lib_25CC373A(0, m, 32);
}

static NOINLINE int sub_4b7c(void *m, const u8 *smac, const u8 *dmac, u16 sport, u16 dport, u8 flags, u32 seq, u32 ack, u16 win)
{
	void *m2 = sceNet_lib_05D525E4(m, 32);
	if (!m2) {
		sceNetIfhandle_lib_F56FAC82(m);
		return 0x80410710;
	}
	u8 *hdr = ((u8 **)m2)[2];
	sceKernelMemcpy(hdr, dmac, 6);
	sceKernelMemcpy(hdr + 6, smac, 6);
	*(u16 *)(hdr + 16) = sport;
	*(u16 *)(hdr + 18) = dport;
	*(u32 *)(hdr + 20) = seq;
	*(u32 *)(hdr + 24) = ack;
	hdr[28] = flags;
	*(u16 *)(hdr + 30) = win;
	return 0;
}

static NOINLINE int sub_4d48(void *ptp)
{
	return sub_4a30(ptp, 8, ((u32 *)ptp)[14], ((u32 *)ptp)[15], (u16)((u32 *)ptp)[10]);
}

static NOINLINE void sub_4e3c(void *ptp)
{
	void *m = ((void **)ptp)[6];
	while (m) {
		void *next = ((void **)m)[1];
		sceNetIfhandle_lib_F56FAC82(m);
		m = next;
	}
	((void **)ptp)[6] = 0;
	if (((void **)ptp)[20]) {
		sceNet_lib_E0A81C7C(((void **)ptp)[20]);
		((void **)ptp)[20] = 0;
	}
	if (((void **)ptp)[21]) {
		sceNet_lib_E0A81C7C(((void **)ptp)[21]);
		((void **)ptp)[21] = 0;
	}
}

static NOINLINE void sub_4fc8(int code, const u8 *mac)
{
	(void)code;
	(void)mac;
	for (void **p = (void **)g_ptp_list; p; p = (void **)p[0]) {
		int wid = ((int *)p)[3];
		if (wid > 0)
			sceNet_lib_6B294EE4(wid);
	}
}

int sceNetAdhocGetPtpStat(int *buflen, void *buf)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (!buflen)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	int max_entries = *buflen / 32;
	int count = 0;
	u8 *dst = (u8 *)buf;
	for (void **p = (void **)g_ptp_list; p; p = (void **)p[0]) {
		if (buf && count < max_entries) {
			sceKernelMemset(dst, 0, 32);
			*(int *)(dst + 4) = ((int *)p)[1];
			sceKernelMemcpy(dst + 8, (u8 *)p + 16, 6);
			sceKernelMemcpy(dst + 14, (u8 *)p + 24, 6);
			*(u16 *)(dst + 20) = *(u16 *)((u8 *)p + 22);
			*(u16 *)(dst + 22) = *(u16 *)((u8 *)p + 30);
			dst += 32;
		}
		count++;
	}
	*buflen = count * 32;
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE int sub_5288(void)
{
	u8 mac[8];
	g_ptp_list = 0;
	int r = sceNet_lib_1858883D(mac);
	if (r < 0)
		return r;
	g_ptp_port_rover = ((u32)mac[4] << 8) | mac[5];
	g_ptp_seq_seed = 1;
	return 0;
}

static NOINLINE void sub_52e0(void)
{
	while (g_ptp_list) {
		void *p = g_ptp_list;
		sub_4e3c(p);
		sub_5590(p);
	}
}

static NOINLINE void *sub_536c(const u8 *smac, u16 sport, const u8 *dmac, u16 dport)
{
	for (void **p = (void **)g_ptp_list; p; p = (void **)p[0]) {
		if (*(u16 *)((u8 *)p + 22) == sport && *(u16 *)((u8 *)p + 30) == dport) {
			const u8 *ps = (const u8 *)p + 16;
			const u8 *pd = (const u8 *)p + 24;
			if (ps[0] == smac[0] && pd[0] == dmac[0])
				return p;
		}
	}
	return 0;
}

static NOINLINE void *sub_542c(const u8 *smac, u16 sport)
{
	for (void **p = (void **)g_ptp_list; p; p = (void **)p[0]) {
		if (*(u16 *)((u8 *)p + 22) == sport) {
			const u8 *ps = (const u8 *)p + 16;
			if (ps[0] == smac[0] && ps[5] == smac[5])
				return p;
		}
	}
	return 0;
}

static NOINLINE int sub_5590(void *ptp)
{
	void **pp = &g_ptp_list;
	while (*pp) {
		if (*pp == ptp) {
			*pp = ((void **)ptp)[0];
			return 0;
		}
		pp = (void **)*pp;
	}
	return -1;
}

static NOINLINE void sub_55dc(void *ptp)
{
	((u32 *)ptp)[14] = g_ptp_seq_seed++;
}

static NOINLINE void sub_55f8(void *ptp)
{
	((void **)ptp)[0] = g_ptp_list;
	g_ptp_list = ptp;
}

static NOINLINE int sub_5604(void *ptp, const u8 *smac, u16 sport, const u8 *dmac, u16 dport)
{
	if (!smac || !dmac || dport == 0)
		return 0x80410709;
	if (sceNet_lib_CA3CF5EB(dmac) < 0)
		return 0x80410703;
	if (sport == 0) {
		g_ptp_port_rover = (g_ptp_port_rover + 1) | 0xc000;
		sport = (u16)g_ptp_port_rover;
	}
	if (sub_536c(smac, sport, dmac, dport))
		return 0x80410706;
	sceKernelMemcpy((u8 *)ptp + 16, smac, 6);
	*(u16 *)((u8 *)ptp + 22) = sport;
	sceKernelMemcpy((u8 *)ptp + 24, dmac, 6);
	*(u16 *)((u8 *)ptp + 30) = dport;
	sub_55dc(ptp);
	return 0;
}

static NOINLINE int sub_572c(void *ptp, const u8 *smac, u16 sport)
{
	if (!smac || sport == 0)
		return 0x80410709;
	if (sub_542c(smac, sport))
		return 0x80410706;
	sceKernelMemcpy((u8 *)ptp + 16, smac, 6);
	*(u16 *)((u8 *)ptp + 22) = sport;
	return 0;
}

static NOINLINE void sub_5750(void *ptp)
{
	int wid = ((int *)ptp)[3];
	if (wid > 0) {
		sceNet_lib_D60225A3(wid);
		sceNet_lib_7574FDA1(wid);
	}
}

static NOINLINE void sub_584c(void)
{
	for (void **p = (void **)g_ptp_list; p; p = (void **)p[0]) {
		int wid = ((int *)p)[3];
		if (wid > 0)
			sceNet_lib_6B294EE4(wid);
	}
}

static NOINLINE void sub_5938(void *m)
{
	void *m2 = sceNetIfhandle_lib_3E8DD3F8(m, 32, 1);
	if (!m2)
		return;
	u8 *hdr = ((u8 **)m2)[2];
	u16 sport = *(u16 *)(hdr + 16);
	u16 dport = *(u16 *)(hdr + 18);
	u32 seq = *(u32 *)(hdr + 20);
	u32 ack = *(u32 *)(hdr + 24);
	u8 flags = hdr[28];
	u16 win = *(u16 *)(hdr + 30);
	void *ptp = sub_536c(hdr, dport, hdr + 6, sport);
	if (!ptp)
		ptp = sub_542c(hdr, dport);
	if (!ptp) {
		sub_4ad4(hdr, hdr + 6, dport, sport, 4, ack, seq + 1, 0);
		sceNetIfhandle_lib_F56FAC82(m2);
		return;
	}
	sub_637c(ptp, m2, hdr, 0, seq, ack, flags, win);
}

static NOINLINE int sub_637c(void *ptp, void *m, void *hdr, int len, u32 seq, u32 ack, u8 flags, u16 win)
{
	(void)hdr;
	(void)len;
	(void)seq;
	(void)ack;
	(void)win;
	if (flags & 4) {
		sub_668c(ptp, 0);
		sceNetIfhandle_lib_F56FAC82(m);
		return 0;
	}
	sceNetIfhandle_lib_9A6261EC(m, 32);
	void *m2 = sceNetIfhandle_lib_F8825DC4(m);
	if (m2) {
		((void **)m2)[1] = ((void **)ptp)[6];
		((void **)ptp)[6] = m2;
	}
	return 0;
}

static NOINLINE void sub_668c(void *ptp, int event)
{
	int state = ((int *)ptp)[5];
	switch (event) {
	case 0:
		state = 0;
		break;
	case 1:
		state = 2;
		break;
	case 2:
		state = 3;
		break;
	case 3:
		state = 4;
		break;
	default:
		break;
	}
	((int *)ptp)[5] = state;
}

static NOINLINE void sub_6878(void *ptp, int idx, u32 ticks)
{
	((u32 *)ptp)[16 + idx] = ticks;
}

static NOINLINE int sub_6a54(u32 a0, void *arg)
{
	(void)a0;
	sceNet_lib_94B44F26();
	if (arg)
		sub_4d48(arg);
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE int sub_6b20(u32 a0, void *arg)
{
	(void)a0;
	sceNet_lib_94B44F26();
	if (arg)
		sub_668c(arg, 0);
	sceNet_lib_515B2F33();
	return 0;
}

int sceNetAdhocGameModeCreateMaster(const void *data, u32 size)
{
	u8 mac[8];
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (!data || size == 0 || size > 0x400)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	if (!sceWlanDevIsGameMode()) {
		sceNet_lib_515B2F33();
		return 0x80410715;
	}
	if (g_gm_master) {
		sceNet_lib_515B2F33();
		return 0x80410712;
	}
	sceNetGetLocalEtherAddr(mac);
	int res = sub_7188(mac, data, size);
	if (res >= 0)
		g_gm_master_id = res;
	sceNet_lib_515B2F33();
	return (res < 0) ? res : 0;
}

int sceNetAdhocGameModeCreateReplica(const u8 *mac, void *data, u32 size)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	if (!mac || !data || size == 0 || size > 0x400)
		return 0x80410709;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	if (!sceWlanDevIsGameMode()) {
		sceNet_lib_515B2F33();
		return 0x80410715;
	}
	int id = sub_7188(mac, data, size);
	sceNet_lib_515B2F33();
	return id;
}

int sceNetAdhocGameModeUpdateMaster(void)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	if (!g_gm_master) {
		sceNet_lib_515B2F33();
		return 0x80410716;
	}
	u32 *m = (u32 *)g_gm_master;
	u32 sz = m[4];
	void *src = (void *)m[3];
	void *buf = (void *)m[5];
	sceKernelMemcpy(buf, src, sz);
	sceNetAdhocAuth_lib_AAB06250(2, (u8 *)m + 4, sz, buf);
	int res = sceWlanGPSend(g_gm_master_id, buf, sz);
	sceNet_lib_515B2F33();
	return res;
}

int sceNetAdhocGameModeDeleteMaster(void)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	if (!g_gm_master) {
		sceNet_lib_515B2F33();
		return 0x80410716;
	}
	sceNetIfhandle_lib_76BAD213(g_gm_master);
	g_gm_master = 0;
	g_gm_master_id = -1;
	sceNet_lib_515B2F33();
	return 0;
}

static NOINLINE int sub_7008(void)
{
	g_gm_master = 0;
	g_gm_replica_list = 0;
	g_gm_replica_count = 0;
	return 0;
}

static NOINLINE void sub_7020(void)
{
	if (g_gm_master) {
		sceNetIfhandle_lib_76BAD213(g_gm_master);
		g_gm_master = 0;
		g_gm_master_id = -1;
	}
	while (g_gm_replica_list) {
		int id = ((int *)g_gm_replica_list)[1];
		sub_72a4(id);
	}
}

int sceNetAdhocGameModeUpdateReplica(int id, void *info)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	return sub_7338(id, info, 0);
}

int sceNetAdhoc_lib_AF6DFB15(int id, void *info)
{
	return sub_7338(id, info, 1);
}

int sceNetAdhocGameModeDeleteReplica(int id)
{
	if (_sceKernelCheckThreadStack() < 0x7e0)
		return 0x80410005;
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	int res = sub_72a4(id);
	sceNet_lib_515B2F33();
	return res;
}

static NOINLINE int sub_7188(const u8 *mac, const void *data, u32 size)
{
	u32 *entry = (u32 *)sceNetIfhandle_lib_15CFE3C0(32 + size);
	if (!entry)
		return 0x80410710;
	sceKernelMemset(entry, 0, 32 + size);
	sceKernelMemcpy((u8 *)entry + 8, mac, 6);
	entry[3] = (u32)data;
	entry[4] = size;
	entry[5] = (u32)((u8 *)entry + 32);
	int id = sceWlanGPPrevEstablishActive((u8 *)entry + 8);
	if (id < 0) {
		sceNetIfhandle_lib_76BAD213(entry);
		return id;
	}
	entry[1] = (u32)id;
	if (!g_gm_master) {
		g_gm_master = entry;
	} else {
		entry[0] = (u32)g_gm_replica_list;
		g_gm_replica_list = entry;
		g_gm_replica_count++;
	}
	return id;
}

static NOINLINE int sub_72a4(int id)
{
	void **pp = &g_gm_replica_list;
	while (*pp) {
		u32 *e = (u32 *)*pp;
		if ((int)e[1] == id) {
			*pp = (void *)e[0];
			g_gm_replica_count--;
			sceNetIfhandle_lib_76BAD213(e);
			return 0;
		}
		pp = (void **)e;
	}
	return 0x80410701;
}

static NOINLINE int sub_7338(int id, void *info, int mode)
{
	sceNet_lib_94B44F26();
	if (!g_adhoc_inited) {
		sceNet_lib_515B2F33();
		return 0x80410713;
	}
	u32 *found = 0;
	for (u32 *e = (u32 *)g_gm_replica_list; e; e = (u32 *)e[0]) {
		if ((int)e[1] == id) {
			found = e;
			break;
		}
	}
	if (!found) {
		sceNet_lib_515B2F33();
		return 0x80410701;
	}
	u32 sz = found[4];
	void *buf = (void *)found[5];
	int res = sceWlanGPRecv(id, buf, sz, info);
	if (res >= 0) {
		if (mode == 0) {
			res = sceNetAdhocAuth_lib_015A8A64(2, (u8 *)found + 8, buf, sz);
		}
		if (res >= 0)
			sceKernelMemcpy((void *)found[3], buf, sz);
	}
	sceNet_lib_515B2F33();
	return res;
}
