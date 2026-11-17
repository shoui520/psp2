/*
 * SceNpCommonPs4: a ring buffer over a block of memory (npext/memory/ring_buf.cpp), and the one that allocates its
 * block from a pool.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_ringBufVtable[];		/* 0x8102a8e0 */
extern const void *g_ringBufAllocVtable[];	/* 0x8102a8f0 */

struct NpCommonPs4RingBuf *SceNpCommonPs4_796C5CD5(struct NpCommonPs4RingBuf *ringBuf)
{
	ringBuf->vtable = NP_COMMON_PS4_VTABLE(g_ringBufVtable);
	ringBuf->buf = NULL;
	ringBuf->rp = NULL;
	ringBuf->wp = NULL;
	ringBuf->bufSize = 0;
	ringBuf->size = 0;
	return ringBuf;
}

struct NpCommonPs4RingBuf *SceNpCommonPs4_25595081(struct NpCommonPs4RingBuf *ringBuf)
{
	ringBuf->vtable = NP_COMMON_PS4_VTABLE(g_ringBufVtable);
	ringBuf->buf = NULL;
	ringBuf->rp = NULL;
	ringBuf->wp = NULL;
	ringBuf->bufSize = 0;
	ringBuf->size = 0;
	return ringBuf;
}

void SceNpCommonPs4_DF845A9C(void)
{
}

struct NpCommonPs4RingBuf *SceNpCommonPs4_D3879F93(struct NpCommonPs4RingBuf *ringBuf)
{
	ringBuf->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return ringBuf;
}

int SceNpCommonPs4_F2F0ECF5(struct NpCommonPs4RingBuf *ringBuf)
{
	ringBuf->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(ringBuf);
}

struct NpCommonPs4RingBuf *SceNpCommonPs4_D4B167A1(struct NpCommonPs4RingBuf *ringBuf)
{
	ringBuf->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return ringBuf;
}

int SceNpCommonPs4_221CC7EE(struct NpCommonPs4RingBuf *ringBuf, void *buf, SceSize bufSize)
{
	static const char functionName[] = "Init";

	NP_COMMON_PS4_ASSERT(ringBuf->buf == 0, functionName, "npext\\memory\\ring_buf.cpp", 58, "m_buf == 0");
	NP_COMMON_PS4_ASSERT(buf != 0, functionName, "npext\\memory\\ring_buf.cpp", 59, "buf != 0");
	NP_COMMON_PS4_ASSERT(bufSize != 0, functionName, "npext\\memory\\ring_buf.cpp", 60, "bufsize > 0");
	ringBuf->buf = (SceUInt8 *)buf;
	ringBuf->rp = (SceUInt8 *)buf;
	ringBuf->bufSize = bufSize;
	ringBuf->wp = (SceUInt8 *)buf;
	ringBuf->size = 0;
	return 0;
}

struct NpCommonPs4RingBuf *SceNpCommonPs4_34DAA7F1(struct NpCommonPs4RingBuf *ringBuf)
{
	ringBuf->buf = NULL;
	ringBuf->rp = NULL;
	ringBuf->wp = NULL;
	ringBuf->size = 0;
	ringBuf->bufSize = 0;
	return ringBuf;
}

struct NpCommonPs4RingBuf *SceNpCommonPs4_2221BD7C(struct NpCommonPs4RingBuf *ringBuf)
{
	ringBuf->buf = NULL;
	ringBuf->rp = NULL;
	ringBuf->wp = NULL;
	ringBuf->size = 0;
	ringBuf->bufSize = 0;
	return ringBuf;
}

struct NpCommonPs4RingBuf *SceNpCommonPs4_BE6CB06B(struct NpCommonPs4RingBuf *ringBuf)
{
	SceUInt8 *buf;

	buf = ringBuf->buf;
	ringBuf->rp = buf;
	ringBuf->wp = buf;
	ringBuf->size = 0;
	return ringBuf;
}

int SceNpCommonPs4_E67CA80C(struct NpCommonPs4RingBuf *ringBuf)
{
	return ringBuf->size == ringBuf->bufSize;
}

int SceNpCommonPs4_B599A560(struct NpCommonPs4RingBuf *ringBuf)
{
	return ringBuf->size == 0;
}

SceSize SceNpCommonPs4_B85224E9(struct NpCommonPs4RingBuf *ringBuf)
{
	return ringBuf->size;
}

SceSize SceNpCommonPs4_2A607C9A(struct NpCommonPs4RingBuf *ringBuf)
{
	return ringBuf->bufSize - ringBuf->size;
}

__attribute__((noinline)) void *SceNpCommonPs4_D7E49888(struct NpCommonPs4RingBuf *ringBuf, SceSize *pBlockSize)
{
	SceUInt8 *wp;
	SceUInt8 *rp;
	static const char functionName[] = "CheckoutForWrite";

	NP_COMMON_PS4_ASSERT(pBlockSize != 0, functionName, "npext\\memory\\ring_buf.cpp", 110, "blockSize != 0");
	if (ringBuf->size == ringBuf->bufSize)
		return NULL;
	wp = ringBuf->wp;
	rp = ringBuf->rp;
	if (rp == wp) {
		rp = ringBuf->buf;
		wp = rp;
		ringBuf->rp = rp;
		ringBuf->wp = rp;
	}
	if (rp <= wp)
		rp = ringBuf->buf + ringBuf->bufSize;
	*pBlockSize = rp - wp;
	return wp;
}

static __attribute__((always_inline)) SceSize CheckinForWrite(struct NpCommonPs4RingBuf *ringBuf, SceSize size)
{
	static const char functionName[] = "CheckinForWrite";

	ringBuf->wp += size;
	NP_COMMON_PS4_ASSERT(ringBuf->buf + ringBuf->bufSize >= ringBuf->wp, functionName, "npext\\memory\\ring_buf.cpp", 136,
		"m_buf + m_bufSize >= m_wp");
	if (ringBuf->buf + ringBuf->bufSize == ringBuf->wp)
		ringBuf->wp = ringBuf->buf;
	ringBuf->size += size;
	return ringBuf->size;
}

int SceNpCommonPs4_12337D98(struct NpCommonPs4RingBuf *ringBuf, SceSize size)
{
	return CheckinForWrite(ringBuf, size);
}

int SceNpCommonPs4_548EF805(struct NpCommonPs4RingBuf *ringBuf, const char *data, SceSize length)
{
	SceSize blockSize;
	SceSize remain;
	void *wptr;
	static const char functionName[] = "Write";

	if (length > ringBuf->bufSize - ringBuf->size)
		return -2141872630;
	wptr = SceNpCommonPs4_D7E49888(ringBuf, &blockSize);
	NP_COMMON_PS4_ASSERT(wptr != 0, functionName, "npext\\memory\\ring_buf.cpp", 161, "wptr != 0");
	if (length <= blockSize) {
		sceClibMemcpy_safe(wptr, data, length);
		CheckinForWrite(ringBuf, length);
	} else {
		sceClibMemcpy_safe(wptr, data, blockSize);
		CheckinForWrite(ringBuf, blockSize);
		remain = length - blockSize;
		wptr = SceNpCommonPs4_D7E49888(ringBuf, &blockSize);
		NP_COMMON_PS4_ASSERT(wptr != 0, functionName, "npext\\memory\\ring_buf.cpp", 179, "wptr != 0");
		NP_COMMON_PS4_ASSERT(remain <= blockSize, functionName, "npext\\memory\\ring_buf.cpp", 180, "remain <= wsize");
		sceClibMemcpy_safe(wptr, data + blockSize, remain);
		CheckinForWrite(ringBuf, remain);
	}
	return 0;
}

void *SceNpCommonPs4_D833372B(struct NpCommonPs4RingBuf *ringBuf, SceSize *pBlockSize)
{
	SceUInt8 *rp;
	static const char functionName[] = "CheckoutForRead";

	if (ringBuf->size == 0)
		return NULL;
	rp = ringBuf->rp;
	if (ringBuf->wp > rp) {
		*pBlockSize = ringBuf->wp - rp;
		if (*pBlockSize != ringBuf->size) {
			NP_COMMON_PS4_ASSERT(0, functionName, "npext\\memory\\ring_buf.cpp", 202, "*blockSize == m_size");
			return ringBuf->rp;
		}
	} else {
		*pBlockSize = ringBuf->buf + ringBuf->bufSize - rp;
	}
	return rp;
}

__attribute__((noinline)) SceUInt32 SceNpCommonPs4_7AF729FD(struct NpCommonPs4RingBuf *ringBuf, SceSize size)
{
	SceUInt8 *end;
	SceUInt8 *rp;
	static const char functionName[] = "CheckinForRead";

	end = ringBuf->buf + ringBuf->bufSize;
	rp = ringBuf->rp;
	if (end - rp <= size) {
		ringBuf->rp = (SceUInt8 *)(size - (ringBuf->bufSize - (SceSize)rp));
	} else {
		ringBuf->rp = rp + size;
		NP_COMMON_PS4_ASSERT(ringBuf->buf + ringBuf->bufSize >= ringBuf->rp, functionName, "npext\\memory\\ring_buf.cpp", 217,
			"((this)->m_buf+(this)->m_bufSize) >= m_rp");
	}
	ringBuf->size -= size;
	return ringBuf->size;
}

int SceNpCommonPs4_DC5607D0(struct NpCommonPs4RingBuf *ringBuf, char *dst, SceSize length)
{
	SceSize blockSize;
	void *rptr;
	static const char functionName[] = "Read";

	if (length == 0)
		return 0;
	if (ringBuf->size < length)
		return -2141872629;
	rptr = SceNpCommonPs4_D833372B(ringBuf, &blockSize);
	NP_COMMON_PS4_ASSERT(rptr != 0, functionName, "npext\\memory\\ring_buf.cpp", 248, "rptr != 0");
	if (blockSize < length) {
		if (dst != NULL) {
			sceClibMemcpy_safe(dst, rptr, blockSize);
			dst += blockSize;
		}
		length -= blockSize;
		SceNpCommonPs4_7AF729FD(ringBuf, blockSize);
		rptr = SceNpCommonPs4_D833372B(ringBuf, &blockSize);
		NP_COMMON_PS4_ASSERT(rptr != 0, functionName, "npext\\memory\\ring_buf.cpp", 270, "rptr != 0");
		NP_COMMON_PS4_ASSERT(length <= blockSize, functionName, "npext\\memory\\ring_buf.cpp", 271, "remain <= rsize");
		if (dst != NULL)
			sceClibMemcpy_safe(dst, rptr, length);
	} else {
		if (dst != NULL)
			sceClibMemcpy_safe(dst, rptr, length);
	}
	SceNpCommonPs4_7AF729FD(ringBuf, length);
	return 0;
}

int SceNpCommonPs4_AD24A470(struct NpCommonPs4RingBuf *ringBuf, SceSize offset, char *dst, SceSize length)
{
	SceUInt8 *cp;
	SceSize first;
	static const char functionName[] = "Peek";

	NP_COMMON_PS4_ASSERT(dst != 0, functionName, "npext\\memory\\ring_buf.cpp", 286, "buf != 0");
	NP_COMMON_PS4_ASSERT(length != 0, functionName, "npext\\memory\\ring_buf.cpp", 287, "size > 0");
	if (ringBuf->size <= offset || ringBuf->size < offset + length)
		return -2141872630;
	cp = ringBuf->rp + offset;
	if (cp >= ringBuf->buf + ringBuf->bufSize) {
		cp -= ringBuf->bufSize;
		NP_COMMON_PS4_ASSERT(cp >= ringBuf->buf && cp < ringBuf->buf + ringBuf->bufSize, functionName, "npext\\memory\\ring_buf.cpp", 316,
			"cp>=m_buf && cp<((this)->m_buf+(this)->m_bufSize)");
	} else {
		NP_COMMON_PS4_ASSERT(cp >= ringBuf->buf && cp < ringBuf->buf + ringBuf->bufSize, functionName, "npext\\memory\\ring_buf.cpp", 305,
			"cp>=m_buf && cp<((this)->m_buf+(this)->m_bufSize)");
		first = ringBuf->buf + ringBuf->bufSize - cp;
		NP_COMMON_PS4_ASSERT(length <= ringBuf->size - offset, functionName, "npext\\memory\\ring_buf.cpp", 307,
			"size <= m_size - offset");
		if (first <= length) {
			sceClibMemcpy_safe(dst, cp, first);
			sceClibMemcpy_safe(dst + first, ringBuf->buf, length - first);
			return 0;
		}
	}
	sceClibMemcpy_safe(dst, cp, length);
	return 0;
}

struct NpCommonPs4RingBufAlloc *SceNpCommonPs4_3245630F(struct NpCommonPs4RingBufAlloc *ringBuf, struct NpCommonPs4Allocator *pool)
{
	ringBuf->ringBuf.rp = NULL;
	ringBuf->ringBuf.wp = NULL;
	ringBuf->ringBuf.size = 0;
	ringBuf->ringBuf.vtable = NP_COMMON_PS4_VTABLE(g_ringBufAllocVtable);
	ringBuf->ringBuf.buf = NULL;
	ringBuf->pool = pool;
	ringBuf->ringBuf.bufSize = 0;
	return ringBuf;
}

struct NpCommonPs4RingBufAlloc *SceNpCommonPs4_7BFE6CC6(struct NpCommonPs4RingBufAlloc *ringBuf, struct NpCommonPs4Allocator *pool)
{
	ringBuf->ringBuf.rp = NULL;
	ringBuf->ringBuf.wp = NULL;
	ringBuf->ringBuf.size = 0;
	ringBuf->ringBuf.vtable = NP_COMMON_PS4_VTABLE(g_ringBufAllocVtable);
	ringBuf->ringBuf.buf = NULL;
	ringBuf->pool = pool;
	ringBuf->ringBuf.bufSize = 0;
	return ringBuf;
}

void SceNpCommonPs4_7F252C09(struct NpCommonPs4RingBufAlloc *ringBuf)
{
	SceNpCommonPs4_38CC9AF5(ringBuf->pool, ringBuf->ringBuf.buf);
}

struct NpCommonPs4RingBufAlloc *SceNpCommonPs4_A264C490(struct NpCommonPs4RingBufAlloc *ringBuf)
{
	struct NpCommonPs4Allocator *pool;
	SceUInt8 *buf;

	pool = ringBuf->pool;
	buf = ringBuf->ringBuf.buf;
	ringBuf->ringBuf.vtable = NP_COMMON_PS4_VTABLE(g_ringBufAllocVtable);
	SceNpCommonPs4_38CC9AF5(pool, buf);
	ringBuf->ringBuf.vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return ringBuf;
}

int SceNpCommonPs4_244620BD(struct NpCommonPs4RingBufAlloc *ringBuf)
{
	struct NpCommonPs4Allocator *pool;
	SceUInt8 *buf;

	pool = ringBuf->pool;
	buf = ringBuf->ringBuf.buf;
	ringBuf->ringBuf.vtable = NP_COMMON_PS4_VTABLE(g_ringBufAllocVtable);
	SceNpCommonPs4_38CC9AF5(pool, buf);
	ringBuf->ringBuf.vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(ringBuf);
}

struct NpCommonPs4RingBufAlloc *SceNpCommonPs4_EF7124FF(struct NpCommonPs4RingBufAlloc *ringBuf)
{
	struct NpCommonPs4Allocator *pool;
	SceUInt8 *buf;

	pool = ringBuf->pool;
	buf = ringBuf->ringBuf.buf;
	ringBuf->ringBuf.vtable = NP_COMMON_PS4_VTABLE(g_ringBufAllocVtable);
	SceNpCommonPs4_38CC9AF5(pool, buf);
	ringBuf->ringBuf.vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return ringBuf;
}

int SceNpCommonPs4_530564F5(struct NpCommonPs4RingBufAlloc *ringBuf, SceSize bufSize)
{
	void *buf;

	buf = SceNpCommonPs4_299ADFC3(ringBuf->pool, bufSize);
	if (buf == NULL)
		return ((int *)ringBuf->pool)[4];
	SceNpCommonPs4_221CC7EE(&ringBuf->ringBuf, buf, bufSize);
	return 0;
}

void SceNpCommonPs4_2EEC1D6E(void)
{
}

int SceNpCommonPs4_31D856EA(struct NpCommonPs4RingBufAlloc *ringBuf)
{
	SceNpCommonPs4_38CC9AF5(ringBuf->pool, ringBuf->ringBuf.buf);
	ringBuf->ringBuf.buf = NULL;
	ringBuf->ringBuf.rp = NULL;
	ringBuf->ringBuf.wp = NULL;
	ringBuf->ringBuf.size = 0;
	ringBuf->ringBuf.bufSize = 0;
	return 0;
}

int SceNpCommonPs4_E1F6AE0F(struct NpCommonPs4RingBufAlloc *ringBuf)
{
	return ringBuf->ringBuf.buf != NULL;
}

int SceNpCommonPs4_82693AD1(struct NpCommonPs4RingBufAlloc *ringBuf, SceSize extra)
{
	SceUInt8 *buf;
	SceUInt8 *rp;
	SceUInt8 *wp;
	SceUInt8 *newBuf;
	SceUInt8 *newRp;
	SceSize bufSize;

	newBuf = (SceUInt8 *)NpCommonPs4AllocatorReallocate(ringBuf->pool, ringBuf->ringBuf.buf, ringBuf->ringBuf.bufSize + extra);
	if (newBuf == NULL)
		return -2141872631;
	buf = ringBuf->ringBuf.buf;
	rp = ringBuf->ringBuf.rp;
	wp = ringBuf->ringBuf.wp;
	newRp = newBuf + (rp - buf);
	bufSize = ringBuf->ringBuf.bufSize;
	if (rp > wp) {
		sceClibMemmove(newRp + extra, newRp, buf + bufSize - rp);
		buf = ringBuf->ringBuf.buf;
		wp = ringBuf->ringBuf.wp;
		bufSize = ringBuf->ringBuf.bufSize;
		newRp = newBuf + (ringBuf->ringBuf.rp - buf) + extra;
	}
	ringBuf->ringBuf.rp = newRp;
	ringBuf->ringBuf.buf = newBuf;
	ringBuf->ringBuf.wp = newBuf + (wp - buf);
	ringBuf->ringBuf.bufSize = bufSize + extra;
	return 0;
}

const void *g_ringBufVtable[] = {		/* 0x8102a8e0 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_D3879F93,
	(const void *)SceNpCommonPs4_F2F0ECF5,
};

const void *g_ringBufAllocVtable[] = {		/* 0x8102a8f0 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_A264C490,
	(const void *)SceNpCommonPs4_244620BD,
};

}
