#include "ifhandle_int.h"

uint32_t g_mbuf_cfg0;
uint32_t g_mbuf_cfg1;
uint32_t g_mbuf_cfg2;
uint32_t g_mbuf_cfg3;
uint32_t g_copym_drops;
uint32_t g_pullup_drops;

void sceNetIfhandle_lib_0FB8AE0D(void)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	g_mbuf_cfg0 = 0;
	g_mbuf_cfg1 = 0;
	g_mbuf_cfg2 = 0;
	g_mbuf_cfg3 = 0;
	pspSetK1(old_k1);
}

SceNetMbuf *sceNetIfhandle_lib_A493AA5F(int how, uint16_t type)
{
	(void)how;
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	SceNetMbuf *m = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (m != NULL) {
		m->m_type = type;
		m->m_data = MBUF_DAT(m);
		m->m_flags = 0;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
	}
	pspSetK1(old_k1);
	return m;
}

SceNetMbuf *sceNetIfhandle_lib_4CF15C43(int how, uint16_t type)
{
	(void)how;
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	SceNetMbuf *m = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (m != NULL) {
		m->m_type = type;
		m->m_data = MBUF_PKTDAT(m);
		m->m_flags = M_PKTHDR;
		m->pkthdr_pad[0] = 0;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
	}
	pspSetK1(old_k1);
	return m;
}

static SceNetMbuf *m_copym0(SceNetMbuf *m, int start_off, int len, int wait, int deep_ext, int copy_pkthdr_always)
{
	(void)wait;
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	if (m == NULL || (int32_t)m < 0) {
		pspSetK1(old_k1);
		return NULL;
	}
	if ((len | start_off) < 0) {
		Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf.c", "m_copym0");
		for (;;) {}
	}

	int off = start_off;
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
			pspSetK1(old_k1);
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
			pspSetK1(old_k1);
			return NULL;
		}

		SceNetMbuf *n = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (n != NULL) {
			n->m_type = m->m_type;
			n->m_data = MBUF_DAT(n);
			n->m_next = NULL;
			n->m_nextpkt = NULL;
			n->m_flags = 0;
		}
		*mp = n;
		if (n == NULL) {
			sceNetIfhandle_lib_F56FAC82(top);
			g_copym_drops++;
			pspSetK1(old_k1);
			return NULL;
		}

		if (copyhdr) {
			n->pkthdr_rcvif = m->pkthdr_rcvif;
			n->m_flags = m->m_flags & 0xfff6u;
			n->m_data = MBUF_PKTDAT(n);
			n->pkthdr_len = m->pkthdr_len;
			for (int i = 0; i < 5; i++) {
				n->pkthdr_pad[i] = m->pkthdr_pad[i];
			}
			if (len == 1000000000) {
				n->pkthdr_len -= start_off;
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
					pspSetK1(old_k1);
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
					space = (int)(MBUF_END(n) - n->m_data);
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
					pspSetK1(old_k1);
					return NULL;
				}
				memcpy(dst, src, avail);
			}
		} else {
			uint8_t *dst = n->m_data;
			uint8_t *src = m->m_data + off;
			if ((int32_t)dst < 0 || (int32_t)(dst + n->m_len) < 0 ||
			    (int32_t)src < 0 || (int32_t)(src + n->m_len) < 0) {
				pspSetK1(old_k1);
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
	pspSetK1(old_k1);
	return top;
}

SceNetMbuf *sceNetIfhandle_lib_456E3146(SceNetMbuf *m, int start_off, int len, int wait)
{
	return m_copym0(m, start_off, len, wait, 0, 0);
}

SceNetMbuf *sceNetIfhandle_lib_E9BF5332(SceNetMbuf *m, int start_off, int len, int wait)
{
	return m_copym0(m, start_off, len, wait, 0, 1);
}

__attribute__((optimize("no-thread-jumps")))
int sceNetIfhandle_lib_5FB31C72(uint32_t *outCfgA, uint32_t *outCfgB, uint32_t *outCfgC, uint32_t *outCfgD)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if ((outCfgA != NULL && ((int32_t)(k1_shifted & (uint32_t)outCfgA) < 0)) ||
	    (outCfgB != NULL && ((int32_t)(k1_shifted & (uint32_t)outCfgB) < 0)) ||
	    (outCfgC != NULL && ((int32_t)(k1_shifted & (uint32_t)outCfgC) < 0)) ||
	    (outCfgD != NULL && ((int32_t)(k1_shifted & (uint32_t)outCfgD) < 0))) {
		pspSetK1(old_k1);
		return (int)0x80410003;
	}

	if (outCfgA != NULL) *outCfgA = g_mbuf_cfg0;
	if (outCfgB != NULL) *outCfgB = g_mbuf_cfg1;
	if (outCfgC != NULL) *outCfgC = g_mbuf_cfg2;
	if (outCfgD != NULL) *outCfgD = g_mbuf_cfg3;

	pspSetK1(old_k1);
	return 0;
}

int sceNetIfhandle_lib_62B20015(uint32_t cfgA, uint32_t cfgB, uint32_t cfgC, uint32_t cfgD)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	g_mbuf_cfg0 = cfgA;
	g_mbuf_cfg1 = cfgB;
	g_mbuf_cfg2 = cfgC;
	g_mbuf_cfg3 = cfgD;
	pspSetK1(old_k1);
	return 0;
}

SceNetMbuf *sceNetIfhandle_lib_E80F00A4(SceNetMbuf *m, int off, int len, int *offp)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (m == NULL || (int32_t)m < 0 || (offp != NULL && ((int32_t)(k1_shifted & (uint32_t)offp) < 0))) {
		pspSetK1(old_k1);
		return NULL;
	}
	if (m == NULL) {
		Kprintf("%s:%s()\n", "../../../../../../../devkit/internal/driver/src/net/pspnet/lib/mbuf2.c", "sceNetMPulldown");
		for (;;) {}
	}
	if (len > 0x800) {
		sceNetIfhandle_lib_F56FAC82(m);
		pspSetK1(old_k1);
		return NULL;
	}

	SceNetMbuf *n = m;
	while (n != NULL && off > 0) {
		if ((int32_t)n < 0) {
			pspSetK1(old_k1);
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
			pspSetK1(old_k1);
			return NULL;
		}
		n = n->m_next;
	}
	if (n == NULL) {
		sceNetIfhandle_lib_F56FAC82(m);
		pspSetK1(old_k1);
		return NULL;
	}

	do {
		if ((off == 0 || offp != NULL) && len <= n->m_len - off) {
			break;
		}

		int hlen = n->m_len - off;
		if (len < hlen) {
			SceNetMbuf *o = sceNetIfhandle_lib_456E3146(n, off, hlen, 1);
			if (o == NULL) {
				sceNetIfhandle_lib_F56FAC82(m);
				pspSetK1(old_k1);
				return NULL;
			}
			n->m_len = off;
			o->m_next = n->m_next;
			n->m_next = o;
			n = o;
			off = 0;
			break;
		}

		int tlen = len - hlen;
		int tail_total = 0;
		for (SceNetMbuf *p = n->m_next; p != NULL; p = p->m_next) {
			if ((int32_t)p < 0) {
				pspSetK1(old_k1);
				return NULL;
			}
			tail_total += p->m_len;
		}
		if (hlen + tail_total < len) {
			sceNetIfhandle_lib_F56FAC82(m);
			pspSetK1(old_k1);
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
				trailing = (int)(MBUF_END(n) - (n->m_data + n->m_len));
			}
			if (trailing >= tlen) {
				sceNetIfhandle_lib_9A6261EC(n->m_next, 0, tlen, n->m_data + n->m_len);
				n->m_len += tlen;
				sceNetIfhandle_lib_BFF3CEA5(n->m_next, tlen);
				break;
			}
		}

		if ((off == 0 || offp != NULL) && !shared && n->m_next != NULL) {
			SceNetMbuf *next = n->m_next;
			int leading = 0;
			if ((next->m_flags & M_EXT) == 0) {
				if (next->m_flags & M_PKTHDR) {
					leading = (int)(next->m_data - MBUF_PKTDAT(next));
				} else {
					leading = (int)(next->m_data - MBUF_DAT(next));
				}
				if (leading >= hlen) {
					if ((int32_t)next < 0) {
						pspSetK1(old_k1);
						return NULL;
					}
					next->m_data -= hlen;
					next->m_len += hlen;
					uint8_t *src = n->m_data + off;
					if ((int32_t)next->m_data < 0 || (int32_t)(next->m_data + hlen) < 0 ||
					    (int32_t)src < 0 || (int32_t)(src + hlen) < 0) {
						pspSetK1(old_k1);
						return NULL;
					}
					memcpy(next->m_data, src, hlen);
					n->m_len -= hlen;
					n = next;
					off = 0;
					break;
				}
			}
		}

		SceNetMbuf *o = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (o == NULL) {
			sceNetIfhandle_lib_F56FAC82(m);
			pspSetK1(old_k1);
			return NULL;
		}
		o->m_type = m->m_type;
		o->m_data = MBUF_DAT(o);
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
				pspSetK1(old_k1);
				return NULL;
			}
		}

		o->m_len = hlen;
		uint8_t *src = n->m_data + off;
		if ((int32_t)o->m_data < 0 || (int32_t)(o->m_data + hlen) < 0 ||
		    (int32_t)src < 0 || (int32_t)(src + hlen) < 0) {
			pspSetK1(old_k1);
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
	} while (0);

	if (offp != NULL) {
		*offp = off;
	}
	pspSetK1(old_k1);
	return n;
}

