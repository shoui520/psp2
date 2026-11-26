/*
 * mbuf.c: NetBSD-derived packet buffer chain operations for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

int g_adhocMaxProtoHdr;
int g_adhocMbufPullupDrops;

static inline void mbufReleaseExt(AdhocMbuf *m)
{
	if (m->storage.mh.body.ext.extNextRef == m) {
		if (m->flags & PSPNET_ADHOC_M_CLUSTER)
			adhocFree(m->storage.mh.body.ext.extBuf);
		else
			adhocFree(m->storage.mh.body.ext.extBuf);
	} else {
		m->storage.mh.body.ext.extNextRef->storage.mh.body.ext.extPrevRef =
			m->storage.mh.body.ext.extPrevRef;
		m->storage.mh.body.ext.extPrevRef->storage.mh.body.ext.extNextRef =
			m->storage.mh.body.ext.extNextRef;
	}
	m->flags &= ~(PSPNET_ADHOC_M_EXT | PSPNET_ADHOC_M_CLUSTER);
	m->storage.mh.body.ext.extSize = 0;
}

__attribute__((noinline))
void adhocMbufFreeChain(AdhocMbuf *m)
{
	AdhocMbuf *next;

	if (m == NULL)
		return;
	do {
		if ((m->flags & PSPNET_ADHOC_M_PKTHDR) && m->storage.mh.pktHdr.aux != NULL) {
			adhocMbufFreeChain(m->storage.mh.pktHdr.aux);
			m->storage.mh.pktHdr.aux = NULL;
		}
		if (m->flags & PSPNET_ADHOC_M_EXT)
			mbufReleaseExt(m);
		next = m->next;
		adhocFree(m);
		m = next;
	} while (m != NULL);
}

__attribute__((noinline))
AdhocMbuf *adhocMbufFree(AdhocMbuf *m)
{
	AdhocMbuf *next;

	if ((m->flags & PSPNET_ADHOC_M_PKTHDR) && m->storage.mh.pktHdr.aux != NULL) {
		adhocMbufFreeChain(m->storage.mh.pktHdr.aux);
		m->storage.mh.pktHdr.aux = NULL;
	}
	if (m->flags & PSPNET_ADHOC_M_EXT)
		mbufReleaseExt(m);
	next = m->next;
	adhocFree(m);
	return next;
}

__attribute__((noinline))
void adhocMbufCopyData(const AdhocMbuf *m, int off, int len, void *cp)
{
	char *dst = (char *)cp;
	int count;

	while (off > 0) {
		if (off < m->len)
			break;
		off -= m->len;
		m = m->next;
	}
	while (len > 0) {
		count = m->len - off;
		if ((unsigned int)count >= (unsigned int)len)
			count = len;
		sceClibMemcpy_safe(dst, m->data + off, (SceSize)count);
		len -= count;
		dst += count;
		off = 0;
		m = m->next;
	}
}

__attribute__((noinline))
void adhocMbufCat(AdhocMbuf *m, AdhocMbuf *n)
{
	while (m->next != NULL)
		m = m->next;
	while (n != NULL) {
		if ((m->flags & PSPNET_ADHOC_M_EXT) ||
		    (m->data + m->len + n->len) >= &m->storage.dat[PSPNET_ADHOC_MLEN]) {
			m->next = n;
			return;
		}
		sceClibMemcpy_safe(m->data + m->len, n->data, (SceSize)n->len);
		m->len += n->len;
		n = adhocMbufFree(n);
	}
}

__attribute__((noinline))
void adhocMbufAdj(AdhocMbuf *mp, int reqLen)
{
	int len = reqLen;
	AdhocMbuf *m = mp;
	int count;

	if (m == NULL)
		return;
	if (len >= 0) {
		while (m != NULL && len > 0) {
			if (m->len <= len) {
				len -= m->len;
				m->len = 0;
				m = m->next;
			} else {
				m->len -= len;
				m->data += len;
				len = 0;
			}
		}
		if (mp->flags & PSPNET_ADHOC_M_PKTHDR)
			mp->storage.mh.pktHdr.len -= (reqLen - len);
	} else {
		len = -len;
		count = m->len;
		while (m->next != NULL) {
			m = m->next;
			count += m->len;
		}
		if (m->len >= len) {
			m->len -= len;
			if (mp->flags & PSPNET_ADHOC_M_PKTHDR)
				mp->storage.mh.pktHdr.len -= len;
			return;
		}
		count -= len;
		if (count < 0)
			count = 0;
		if (mp->flags & PSPNET_ADHOC_M_PKTHDR)
			mp->storage.mh.pktHdr.len = count;
		m = mp;
		for (;;) {
			if (m->len >= count) {
				m->len = count;
				break;
			}
			count -= m->len;
			m = m->next;
			if (m == NULL)
				break;
		}
		while ((m = m->next) != NULL)
			m->len = 0;
	}
}

__attribute__((noinline))
AdhocMbuf *adhocMbufPullup(AdhocMbuf *n, int len)
{
	AdhocMbuf *m;
	unsigned int count, space;
	int failed = 0;

	if ((n->flags & PSPNET_ADHOC_M_EXT) == 0 &&
	    n->data + len < &n->storage.dat[PSPNET_ADHOC_MLEN] &&
	    n->next != NULL) {
		if (n->len >= len)
			return n;
		m = n;
		n = n->next;
		len -= m->len;
	} else {
		if ((unsigned int)len > PSPNET_ADHOC_MHLEN ||
		    (m = (AdhocMbuf *)adhocMalloc(PSPNET_ADHOC_MSIZE)) == NULL) {
			failed = 1;
		} else {
			m->next = NULL;
			m->nextPkt = NULL;
			m->data = m->storage.dat;
			m->type = n->type;
			m->flags = 0;
			m->len = 0;
			if (n->flags & PSPNET_ADHOC_M_PKTHDR) {
				m->storage.mh.pktHdr = n->storage.mh.pktHdr;
				m->flags = (SceShort16)(n->flags & ~(PSPNET_ADHOC_M_EXT | PSPNET_ADHOC_M_CLUSTER));
				m->data = m->storage.mh.body.pktDat;
				n->flags &= ~PSPNET_ADHOC_M_PKTHDR;
			}
		}
	}
	if (!failed) {
		space = (unsigned int)(&m->storage.dat[PSPNET_ADHOC_MLEN] - (m->data + m->len));
		do {
			count = (unsigned int)len > (unsigned int)g_adhocMaxProtoHdr ?
				(unsigned int)len : (unsigned int)g_adhocMaxProtoHdr;
			if (count >= space)
				count = space;
			if (count > (unsigned int)n->len)
				count = (unsigned int)n->len;
			sceClibMemcpy_safe(m->data + m->len, n->data, count);
			len -= (int)count;
			m->len += (int)count;
			n->len -= (int)count;
			space -= count;
			if (n->len != 0)
				n->data += count;
			else
				n = adhocMbufFree(n);
		} while (len > 0 && n != NULL);
		if (len > 0) {
			adhocMbufFree(m);
			failed = 1;
		}
	}
	if (failed) {
		adhocMbufFreeChain(n);
		g_adhocMbufPullupDrops++;
		return NULL;
	}
	m->next = n;
	return m;
}

__attribute__((noinline))
void adhocMbufCopyBack(AdhocMbuf *head, int off, int len, const void *cp)
{
	const char *src = (const char *)cp;
	AdhocMbuf *m = head;
	AdhocMbuf *n;
	int mlen;
	int totlen = 0;
	int allocFailed = 0;

	if (head == NULL)
		return;
	while (off > (mlen = m->len)) {
		off -= mlen;
		totlen += mlen;
		if (m->next == NULL) {
			SceShort16 mtype = m->type;
			n = (AdhocMbuf *)adhocMalloc(PSPNET_ADHOC_MSIZE);
			if (n != NULL) {
				n->next = NULL;
				n->nextPkt = NULL;
				n->data = n->storage.dat;
				n->type = mtype;
				n->flags = 0;
				sceClibMemset(n->data, 0, PSPNET_ADHOC_MLEN);
			}
			if (n == NULL) {
				allocFailed = 1;
				break;
			}
			n->len = (unsigned int)(len + off) > PSPNET_ADHOC_MLEN ?
				 PSPNET_ADHOC_MLEN : (len + off);
			m->next = n;
		}
		m = m->next;
	}
	while (!allocFailed && len > 0) {
		mlen = m->len - off;
		if (mlen >= len)
			mlen = len;
		sceClibMemcpy_safe(m->data + off, src, (SceSize)mlen);
		src += mlen;
		len -= mlen;
		mlen += off;
		off = 0;
		totlen += mlen;
		if (len == 0)
			break;
		if (m->next == NULL) {
			SceShort16 mtype = m->type;
			n = (AdhocMbuf *)adhocMalloc(PSPNET_ADHOC_MSIZE);
			if (n == NULL)
				break;
			n->next = NULL;
			n->nextPkt = NULL;
			n->data = n->storage.dat;
			n->type = mtype;
			n->flags = 0;
			n->len = (unsigned int)len > PSPNET_ADHOC_MLEN ?
				 PSPNET_ADHOC_MLEN : len;
			m->next = n;
		}
		m = m->next;
	}
	if ((head->flags & PSPNET_ADHOC_M_PKTHDR) && head->storage.mh.pktHdr.len < totlen)
		head->storage.mh.pktHdr.len = totlen;
}
