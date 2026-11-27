#include "ifhandle_int.h"

int g_vpl_uid = -1;
uint32_t g_vpl_max_used = 0xffffffffu;

int sceNetIfhandle_lib_0C391E9F(uint32_t partOrSize, uint32_t poolSize)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	int modid = sceKernelSearchModuleByName("sceNet_Library");
	uint32_t info[24];
	memset(info, 0, sizeof(info));
	info[0] = sizeof(info);
	int res = sceKernelQueryModuleInfo(modid, info);
	if (res >= 0) {
		int part = 2;
		uint32_t size = partOrSize;
		if ((info[16] & 0xffff0000u) != 0x01010000u) {
			part = (int)partOrSize;
			size = poolSize;
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

	pspSetK1(old_k1);
	return res;
}

int sceNetIfhandle_lib_29ED84C5(void)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	if (g_vpl_uid > 0) {
		sceKernelDeleteVpl(g_vpl_uid);
		g_vpl_uid = -1;
	}

	pspSetK1(old_k1);
	return 0;
}

int sceNetIfhandle_lib_4C2886CB(uint32_t *stat)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (stat == NULL || ((int32_t)(k1_shifted & (uint32_t)stat) < 0)) {
		pspSetK1(old_k1);
		return (int)0x80410103;
	}

	uint32_t vpl_info[13];
	vpl_info[0] = 0x34;
	int res = sceKernelReferVplStatus(g_vpl_uid, vpl_info);
	if (res < 0) {
		pspSetK1(old_k1);
		return res;
	}

	pspSetK1(old_k1);
	stat[0] = vpl_info[10];
	stat[1] = g_vpl_max_used;
	stat[2] = vpl_info[11];
	return 0;
}

void *sceNetIfhandle_lib_15CFE3C0(uint32_t size)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	uint32_t vpl_info[16];
	void *ptr;
	if (sceKernelTryAllocateVpl(g_vpl_uid, size, &ptr) < 0 || (int32_t)ptr < 0) {
		pspSetK1(old_k1);
		return NULL;
	}

	vpl_info[0] = 0x34;
	if (sceKernelReferVplStatus(g_vpl_uid, vpl_info) == 0) {
		uint32_t used = vpl_info[10] - vpl_info[11];
		if ((int32_t)used > (int32_t)g_vpl_max_used) {
			g_vpl_max_used = used;
		}
	}

	pspSetK1(old_k1);
	return ptr;
}

void sceNetIfhandle_lib_76BAD213(void *ptr)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (ptr == NULL || ((int32_t)(k1_shifted & (uint32_t)ptr) < 0) || (int32_t)ptr < 0) {
		pspSetK1(old_k1);
		return;
	}

	int res = sceKernelFreeVpl(g_vpl_uid, ptr);
	if (res < 0) {
		Kprintf("sceNetFree: invalid ptr 0x%p, ret = 0x%08x\n", ptr, res);
	}
	pspSetK1(old_k1);
}

SceNetMbuf *sceNetIfhandle_lib_59F0D619(int how, uint16_t type)
{
	(void)how;
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	SceNetMbuf *m = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (m == NULL) {
		pspSetK1(old_k1);
		return NULL;
	}

	uint8_t *dat = MBUF_DAT(m);
	m->m_type = type;
	m->m_next = NULL;
	m->m_nextpkt = NULL;
	m->m_data = dat;
	m->m_flags = 0;
	if ((int32_t)dat < 0) {
		pspSetK1(old_k1);
		return NULL;
	}
	memset(dat, 0, 0xec);
	pspSetK1(old_k1);
	return m;
}

SceNetMbuf *sceNetIfhandle_lib_F8825DC4(SceNetMbuf *m)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	if (m == NULL || (int32_t)m < 0) {
		pspSetK1(old_k1);
		return NULL;
	}

	uint16_t flags = m->m_flags;
	SceNetMbuf *nextref = m->ext_nextref;
	SceNetMbuf *prevref = m->ext_prevref;
	if (flags & M_EXT) {
		if ((int32_t)nextref < 0 || (int32_t)prevref < 0 || (int32_t)m->ext_buf < 0) {
			pspSetK1(old_k1);
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
	pspSetK1(old_k1);
	if ((int32_t)next < 0) {
		return NULL;
	}
	return next;
}

void sceNetIfhandle_lib_F56FAC82(SceNetMbuf *m)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	while (m != NULL) {
		if ((int32_t)m < 0) {
			pspSetK1(old_k1);
			return;
		}
		uint16_t flags = m->m_flags;
		SceNetMbuf *nextref = m->ext_nextref;
		SceNetMbuf *prevref = m->ext_prevref;
		if (flags & M_EXT) {
			if ((int32_t)nextref < 0 || (int32_t)prevref < 0 || (int32_t)m->ext_buf < 0) {
				pspSetK1(old_k1);
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

	pspSetK1(old_k1);
}

SceNetMbuf *sceNetIfhandle_lib_C3325FDC(SceNetMbuf *m, int len, int how)
{
	(void)how;
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	if (m == NULL || (int32_t)m < 0) {
		pspSetK1(old_k1);
		return NULL;
	}

	SceNetMbuf *n = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
	if (n == NULL) {
		sceNetIfhandle_lib_F56FAC82(m);
		pspSetK1(old_k1);
		return NULL;
	}

	n->m_flags = 0;
	n->m_data = MBUF_DAT(n);
	n->m_type = m->m_type;
	n->m_next = NULL;
	n->m_nextpkt = NULL;

	if (m->m_flags & M_PKTHDR) {
		n->m_flags = m->m_flags & 0xfff6u;
		n->m_data = MBUF_PKTDAT(n);
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
	pspSetK1(old_k1);
	return n;
}

void sceNetIfhandle_lib_9A6261EC(SceNetMbuf *m, int off, int len, void *cp)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (cp == NULL) {
		pspSetK1(old_k1);
		return;
	}
	uint32_t end = (uint32_t)cp + (uint32_t)len;
	if (((int32_t)(k1_shifted & ((uint32_t)cp | end | (uint32_t)len)) < 0) ||
	    (int32_t)cp < 0 || (int32_t)end < 0) {
		pspSetK1(old_k1);
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
			pspSetK1(old_k1);
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
			pspSetK1(old_k1);
			return;
		}
		int avail = m->m_len - off;
		int chunk = (avail < len) ? avail : len;
		uint8_t *src = m->m_data + off;
		len -= chunk;
		if ((int32_t)src < 0 || (int32_t)(src + chunk) < 0) {
			pspSetK1(old_k1);
			return;
		}
		memcpy(out, src, chunk);
		out += chunk;
		m = m->m_next;
		off = 0;
	}

	pspSetK1(old_k1);
}

void sceNetIfhandle_lib_3E8DD3F8(SceNetMbuf *m, SceNetMbuf *n)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	if (m == NULL || (int32_t)m < 0) {
		pspSetK1(old_k1);
		return;
	}

	while (m->m_next != NULL) {
		m = m->m_next;
		if ((int32_t)m < 0) {
			pspSetK1(old_k1);
			return;
		}
	}

	uint8_t *m_end = MBUF_END(m);
	while (n != NULL) {
		if ((int32_t)n < 0) {
			pspSetK1(old_k1);
			return;
		}
		if ((m->m_flags & M_EXT) || (m->m_data + m->m_len + n->m_len >= m_end)) {
			m->m_next = n;
			pspSetK1(old_k1);
			return;
		}
		uint8_t *dst = m->m_data + m->m_len;
		uint8_t *src = n->m_data;
		if ((int32_t)dst < 0 || (int32_t)(dst + n->m_len) < 0 ||
		    (int32_t)src < 0 || (int32_t)(src + n->m_len) < 0) {
			pspSetK1(old_k1);
			return;
		}
		memcpy(dst, src, n->m_len);
		m->m_len += n->m_len;
		n = sceNetIfhandle_lib_F8825DC4(n);
	}

	pspSetK1(old_k1);
}

void sceNetIfhandle_lib_BFF3CEA5(SceNetMbuf *mp, int req_len)
{
	uint32_t old_k1 = pspGetK1();
	int len = req_len;
	SceNetMbuf *m = mp;
	if (m == NULL || (int32_t)m < 0) {
		return;
	}

	if (len >= 0) {
		while (m != NULL && len > 0) {
			if ((int32_t)m < 0) {
				pspSetK1(old_k1);
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
		pspSetK1(old_k1);
	} else {
		len = -len;
		int count = 0;
		for (;;) {
			if ((int32_t)m < 0) {
				pspSetK1(old_k1);
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
			pspSetK1(old_k1);
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
				pspSetK1(old_k1);
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
					pspSetK1(old_k1);
					return;
				}
				m->m_len = 0;
			}
		}
		pspSetK1(old_k1);
	}
}

SceNetMbuf *sceNetIfhandle_lib_49EDBB18(SceNetMbuf *n, int len)
{
	uint32_t old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);

	if (n == NULL || (int32_t)n < 0) {
		pspSetK1(old_k1);
		return NULL;
	}

	SceNetMbuf *m;
	if ((n->m_flags & M_EXT) == 0 &&
	    (n->m_data + len < MBUF_END(n)) &&
	    n->m_next != NULL) {
		if (n->m_len >= len) {
			pspSetK1(old_k1);
			return n;
		}
		m = n;
		n = n->m_next;
		len -= m->m_len;
		if ((int32_t)n < 0) {
			pspSetK1(old_k1);
			return NULL;
		}
	} else {
		if ((uint32_t)len > 0xd0u) {
			sceNetIfhandle_lib_F56FAC82(n);
			g_pullup_drops++;
			pspSetK1(old_k1);
			return NULL;
		}
		m = (SceNetMbuf *)sceNetIfhandle_lib_15CFE3C0(0x100);
		if (m == NULL) {
			sceNetIfhandle_lib_F56FAC82(n);
			g_pullup_drops++;
			pspSetK1(old_k1);
			return NULL;
		}
		m->m_flags = 0;
		m->m_data = MBUF_DAT(m);
		m->m_type = n->m_type;
		m->m_next = NULL;
		m->m_nextpkt = NULL;
		m->m_len = 0;
		if (n->m_flags & M_PKTHDR) {
			m->m_flags = n->m_flags & 0xfff6u;
			m->m_data = MBUF_PKTDAT(m);
			m->pkthdr_rcvif = n->pkthdr_rcvif;
			m->pkthdr_len = n->pkthdr_len;
			n->m_flags &= ~M_PKTHDR;
			for (int i = 0; i < 5; i++) {
				m->pkthdr_pad[i] = n->pkthdr_pad[i];
			}
		}
	}

	int space = (int)(MBUF_END(m) - (m->m_data + m->m_len));
	do {
		if ((int32_t)n < 0) {
			pspSetK1(old_k1);
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
			pspSetK1(old_k1);
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
		pspSetK1(old_k1);
		return NULL;
	}
	m->m_next = n;
	pspSetK1(old_k1);
	return m;
}

void sceNetIfhandle_lib_1560F143(SceNetMbuf *head_mbuf, int off, int len, const void *cp)
{
	uint32_t old_k1 = pspGetK1();
	uint32_t k1_shifted = old_k1 << 11;
	pspSetK1(k1_shifted);

	if (cp == NULL) {
		pspSetK1(old_k1);
		return;
	}
	uint32_t end = (uint32_t)cp + (uint32_t)len;
	if (((int32_t)(k1_shifted & ((uint32_t)cp | end | (uint32_t)len)) < 0) ||
	    (int32_t)cp < 0 || (int32_t)end < 0 || head_mbuf == NULL || (int32_t)head_mbuf < 0) {
		pspSetK1(old_k1);
		return;
	}

	SceNetMbuf *m = head_mbuf;
	int totlen = 0;
	int alloc_failed = 0;
	while (off > m->m_len) {
		if ((int32_t)m < 0) {
			pspSetK1(old_k1);
			return;
		}
		off -= m->m_len;
		totlen += m->m_len;
		if (m->m_next == NULL) {
			SceNetMbuf *n = sceNetIfhandle_lib_59F0D619(1, m->m_type);
			if (n == NULL) {
				alloc_failed = 1;
				break;
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
	while (!alloc_failed && len > 0) {
		if ((int32_t)m < 0) {
			pspSetK1(old_k1);
			return;
		}
		int mlen = m->m_len - off;
		if (mlen > len) {
			mlen = len;
		}
		uint8_t *dst = m->m_data + off;
		if ((int32_t)dst < 0 || (int32_t)(dst + mlen) < 0 ||
		    (int32_t)src < 0 || (int32_t)(src + mlen) < 0) {
			pspSetK1(old_k1);
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
			n->m_data = MBUF_DAT(n);
			n->m_next = NULL;
			n->m_nextpkt = NULL;
			n->m_flags = 0;
			n->m_len = (len < 0xec) ? len : 0xec;
			m->m_next = n;
		}
		m = m->m_next;
	}

	if ((head_mbuf->m_flags & M_PKTHDR) && (head_mbuf->pkthdr_len < totlen)) {
		head_mbuf->pkthdr_len = totlen;
	}
	pspSetK1(old_k1);
}

