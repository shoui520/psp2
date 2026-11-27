/*
 * DTCPIP-SW SdiBuffer, SdiBufferCXD3705, and SdiBufferFactoryCXD3705.
 */

#include "sw_sdi.h"

#include <stdlib.h>
#include <string.h>

SdiBufferFactoryCXD3705 g_sdiBufferFactory;
uint32_t g_sdiTransferUnit = 0x8000u;

#define swIsTxStreamType(t) ( \
	(t) == 0x10040002u || (t) == 0x10080002u || (t) == 0x10100002u || (t) == 0x10200002u || \
	(t) == 0x10400002u || (t) == 0x10800002u || (t) == 0x11000002u || (t) == 0x12000002u || \
	(t) == 0x50040001u || (t) == 0x50080001u || (t) == 0x50100001u || (t) == 0x50200001u || \
	(t) == 0x50400001u || (t) == 0x50800001u || (t) == 0x51000001u || (t) == 0x52000001u)

#define swIsTxWithAuxType(t) ( \
	(t) == 0x10010002u || (t) == 0x20020228u || (t) == 0x20020231u)

#define swIsRxDirectType(t) ( \
	(t) == 0x90010004u || (t) == 0x90010008u || (t) == 0x90010010u || (t) == 0x90010020u || \
	(t) == 0x90010040u || (t) == 0x90010080u || (t) == 0x90010100u || (t) == 0x90010200u || \
	(t) == 0x50010004u || (t) == 0x50010008u || (t) == 0x50010010u || (t) == 0x50010020u || \
	(t) == 0x50010040u || (t) == 0x50010080u || (t) == 0x50010100u || (t) == 0x50010200u)

#define swIsRxWithAuxType(t) ( \
	(t) == 0x90010001u || (t) == 0xA0020228u || (t) == 0xA0020231u)

#define swIsAnyRxType(t) (swIsRxDirectType(t) || swIsRxWithAuxType(t))

#define swIsAnyTxType(t) (swIsTxStreamType(t) || swIsTxWithAuxType(t))

#define swIsRxDecryptType(t) ( \
	(t) == 0x90010004u || (t) == 0x90010008u || (t) == 0x90010010u || (t) == 0x90010020u || \
	(t) == 0x90010040u || (t) == 0x90010080u || (t) == 0x90010100u || (t) == 0x90010200u || \
	(t) == 0x90010001u || (t) == 0xA0020228u || (t) == 0xA0020231u)

/* ---- Scoped lock helper for SdiBuffer ---- */
class SdiBufferLock {
public:
	explicit SdiBufferLock(SdiBuffer *buf) : m_buf(buf) { m_buf->lockBuffer(); }
	~SdiBufferLock() { m_buf->unlockBuffer(); }

private:
	SdiBuffer *m_buf;
};

/* ---- SdiBuffer ---- */

SdiBuffer::SdiBuffer(void *extMem, uint32_t capacity, const SwString &label)
	: m_ownsMem(extMem == NULL ? 1 : 0),
	  m_mem(static_cast<uint8_t *>(extMem)),
	  m_capacity(capacity),
	  m_name("SdiBuffer("),
	  m_alignPad(0),
	  m_mutex(),
	  m_writePos(0),
	  m_readPos(0),
	  m_eof(0),
	  m_full(0),
	  m_readVec(NULL),
	  m_writeVec(NULL),
	  m_error(0)
{
	m_reserved5[0] = 0; m_reserved5[1] = 0; m_reserved5[2] = 0;
	m_reserved5a[0] = 0; m_reserved5a[1] = 0;

	if (label.empty())
		m_name.append("12:04:36");
	else
		m_name.append(label.c_str());
	m_name.append(") ");

	if (m_capacity == 0xFFFFFFFFu)
		--m_capacity;
	if (!m_mem) {
		m_mem = static_cast<uint8_t *>(malloc(m_capacity));
		if (!m_mem)
			m_capacity = 0;
	}

	m_readVec = new (std::nothrow) SdiBufferChunk[2];
	if (m_readVec)
		memset(m_readVec, 0, sizeof(SdiBufferChunk) * 2);
	m_writeVec = new (std::nothrow) SdiBufferChunk[2];
	if (m_writeVec)
		memset(m_writeVec, 0, sizeof(SdiBufferChunk) * 2);
}

SdiBuffer::~SdiBuffer()
{
	if (m_mem && m_ownsMem)
		free(m_mem);
	m_mem = NULL;
	delete[] m_readVec;
	m_readVec = NULL;
	delete[] m_writeVec;
	m_writeVec = NULL;
}

int SdiBuffer::lockBuffer()
{
	m_mutex.lock();
	return 1;
}

int SdiBuffer::unlockBuffer()
{
	m_mutex.unlock();
	return 1;
}

uint32_t SdiBuffer::availableUnlocked() const
{
	if (m_readPos == m_writePos)
		return m_full ? m_capacity : 0;
	if (m_readPos > m_writePos)
		return m_capacity - m_readPos + m_writePos;
	return m_writePos - m_readPos;
}

void SdiBuffer::setEndOfStream()
{
	SdiBufferLock lock(this);
	m_eof = 1;
}

int SdiBuffer::appendUnlocked(unsigned int size)
{
	uint32_t prev = m_writePos;
	m_writePos = prev + size;
	if (m_writePos >= m_capacity)
		m_writePos -= m_capacity;
	if (m_writePos <= m_capacity &&
	    (prev >= m_readPos || m_readPos >= m_writePos)) {
		if (m_writePos == m_capacity)
			m_writePos = 0;
		if (m_writePos == m_readPos && size != 0)
			m_full = 1;
		return 1;
	}
	m_writePos = prev;
	return 0;
}

int SdiBuffer::append(unsigned int size)
{
	SdiBufferLock lock(this);
	return appendUnlocked(size);
}

int SdiBuffer::consume(unsigned int size)
{
	SdiBufferLock lock(this);
	uint32_t prev = m_readPos;
	m_readPos = prev + size;
	if (m_readPos >= m_capacity)
		m_readPos -= m_capacity;
	if (m_readPos <= m_capacity &&
	    (prev >= m_writePos || m_writePos >= m_readPos)) {
		if (m_readPos == m_capacity)
			m_readPos = 0;
		if (m_full && size != 0)
			m_full = 0;
		return 1;
	}
	m_readPos = prev;
	return 0;
}

int SdiBuffer::available()
{
	SdiBufferLock lock(this);
	return (int)availableUnlocked();
}

int SdiBuffer::size()
{
	return (int)m_capacity;
}

int SdiBuffer::isEndOfStream()
{
	return m_eof;
}

int SdiBuffer::setName(const SwString &name)
{
	SdiBufferLock lock(this);
	m_name.assign("SdiBuffer(");
	if (name.empty())
		m_name.append("12:04:36");
	else
		m_name.append(name.c_str());
	m_name.append(")");
	return 1;
}

int SdiBuffer::reset()
{
	SdiBufferLock lock(this);
	m_readPos = 0;
	m_writePos = 0;
	m_full = 0;
	m_eof = 0;
	return 1;
}

int SdiBuffer::setSize(unsigned int newSize)
{
	SdiBufferLock lock(this);
	if (newSize == 0)
		return 0;
	if (newSize == m_capacity)
		return 1;
	if (availableUnlocked() > newSize)
		return 0;

	uint8_t *newMem = new (std::nothrow) uint8_t[newSize];
	if (!newMem)
		return 0;

	if (m_readPos == m_writePos) {
		if (m_full) {
			uint32_t tail = m_capacity - m_writePos;
			memcpy(newMem, m_mem + m_writePos, tail);
			memcpy(newMem + tail, m_mem, m_writePos);
			m_writePos = m_capacity;
		} else {
			m_writePos = 0;
		}
		m_readPos = 0;
		m_full = 0;
	} else if (m_readPos > m_writePos) {
		uint32_t tail = m_capacity - m_readPos;
		uint32_t head = m_writePos;
		memcpy(newMem, m_mem + m_readPos, tail);
		memcpy(newMem + tail, m_mem, head);
		m_readPos = 0;
		m_writePos = tail + head;
		if (newSize == m_writePos)
			m_full = 1;
	} else {
		memcpy(newMem, m_mem + m_readPos, m_writePos - m_readPos);
		m_writePos -= m_readPos;
		m_readPos = 0;
	}

	if (m_mem) {
		delete[] m_mem;
		m_mem = NULL;
	}
	m_mem = newMem;
	m_capacity = newSize;
	return 1;
}

int SdiBuffer::read(void *dst, unsigned int len)
{
	if (!dst)
		return -1;
	if (len == 0) {
		if (isEndOfStream() && available() == 0)
			return -1;
		return 0;
	}
	SdiBufferChunk *list = NULL;
	int count = getReadChunkList(&list);
	if (count == -1)
		return -1;
	unsigned int total = 0;
	for (int i = 0; total != len && i < count; ++i) {
		unsigned int take = list[i].size;
		if (len - total < take)
			take = len - total;
		memcpy(static_cast<uint8_t *>(dst) + total, list[i].ptr, take);
		total += take;
	}
	consume(total);
	return (int)total;
}

int SdiBuffer::write(const void *src, unsigned int len)
{
	if (!src)
		return -1;
	if (len == 0)
		return isEndOfStream() ? -1 : 0;
	SdiBufferChunk *list = NULL;
	int count = getWriteChunkList(&list);
	if (count == -1)
		return -1;
	unsigned int total = 0;
	for (int i = 0; total != len && i < count; ++i) {
		unsigned int put = list[i].size;
		if (len - total < put)
			put = len - total;
		memcpy(list[i].ptr, static_cast<const uint8_t *>(src) + total, put);
		total += put;
	}
	append(total);
	return (int)total;
}

int SdiBuffer::getWriteChunkList(SdiBufferChunk **outList)
{
	SdiBufferLock lock(this);
	if (!outList)
		return -1;
	*outList = NULL;
	if (!m_writeVec || !m_mem)
		return -1;
	if (isEndOfStream())
		return -1;

	*outList = m_writeVec;
	if (m_writePos >= m_readPos) {
		if (m_readPos == m_writePos && m_full)
			return 0;
		m_writeVec[0].ptr = m_mem + m_writePos;
		m_writeVec[0].size = m_capacity - m_writePos;
		m_writeVec[1].ptr = m_mem;
		m_writeVec[1].size = m_readPos;
		return (m_writeVec[1].size != 0) ? 2 : 1;
	}
	m_writeVec[0].ptr = m_mem + m_writePos;
	m_writeVec[0].size = m_readPos - m_writePos;
	return 1;
}

int SdiBuffer::getReadChunkList(SdiBufferChunk **outList)
{
	SdiBufferLock lock(this);
	if (!outList)
		return -1;
	*outList = NULL;
	if (!m_readVec || !m_mem)
		return -1;
	if (isEndOfStream() && availableUnlocked() == 0)
		return -1;

	*outList = m_readVec;
	if (m_readPos >= m_writePos) {
		if (m_writePos == m_readPos && !m_full)
			return 0;
		m_readVec[0].ptr = m_mem + m_readPos;
		m_readVec[0].size = m_capacity - m_readPos;
		m_readVec[1].ptr = m_mem;
		m_readVec[1].size = m_writePos;
		return (m_readVec[1].size != 0) ? 2 : 1;
	}
	m_readVec[0].ptr = m_mem + m_readPos;
	m_readVec[0].size = m_writePos - m_readPos;
	return 1;
}

int SdiBuffer::setError(int error)
{
	if (m_error != 0)
		return 0;
	m_error = error;
	return 1;
}

int SdiBuffer::getError() { return m_error; }
int SdiBuffer::setTimeout(int) { return 0; }
int SdiBuffer::isKeyExpiring() { return 0; }

/* ---- SdiBufferCXD3705 ---- */

void SdiBufferCXD3705::initFields(uint32_t chunkMax, uint32_t chunkAlign, uint32_t chunkOff)
{
	m_device = NULL;
	m_sessionId = 0;
	m_drvFd = -1;
	m_streamId = 0;
	m_reserved75[0] = 0; m_reserved75[1] = 0; m_reserved75[2] = 0;
	m_isReadSide = 0;
	m_dirFlag = 0xFF;
	m_bound = 0;
	m_reserved7e[0] = 0; m_reserved7e[1] = 0;
	m_streamDone = 0;
	m_lastPacketSent = 0;
	m_stoppedRx = 0;
	m_stoppedTx = 0;
	m_lastEmi = 4;
	m_streamCb = NULL;
	m_eventCb = NULL;
	m_lastIv = NULL;
	m_timeoutMs = 0;
	m_pendingHeaderBytes = 0;
	m_chunkMax = chunkMax;
	m_chunkAlign = chunkAlign;
	m_chunkOffset = (int32_t)chunkOff;
	m_hasChunkOffset = 0;
	m_reservedb1[0] = 0; m_reservedb1[1] = 0; m_reservedb1[2] = 0;
	m_reservedb4 = 0;
	m_reservedb8 = 0;
	m_scanCursor = 0;
	memset(m_hdrAccum, 0, sizeof(m_hdrAccum));
	m_hdrAccumLen = 0;
	m_retryLast = 0;
	m_reservedc6[0] = 0; m_reservedc6[1] = 0;
	m_lastCci = 255;
	m_lastCopyMode = 5;
	m_lastRetention = 0;
	m_reservedd1[0] = 0; m_reservedd1[1] = 0; m_reservedd1[2] = 0;
	m_lastStatus = 0;
	m_lastErrorFlags = 0;
	memset(m_lastCciInfo, 0, sizeof(m_lastCciInfo));
	m_params = NULL;
}

SdiBufferCXD3705::SdiBufferCXD3705(void *extMem, uint32_t capacity, const SwString &label)
	: SdiBuffer(extMem, capacity, label), m_chunks()
{
	initFields(0, 0, 0);
}

SdiBufferCXD3705::SdiBufferCXD3705(const uint32_t *chunkMax, const uint32_t *chunkAlign,
				   const uint32_t *chunkOff, void *extMem,
				   uint32_t capacity, const SwString &label)
	: SdiBuffer(extMem, capacity, label), m_chunks()
{
	initFields(chunkMax ? *chunkMax : 0,
		   chunkAlign ? *chunkAlign : 0,
		   chunkOff ? *chunkOff : 0);
}

SdiBufferCXD3705::~SdiBufferCXD3705()
{
	if (m_lastIv) {
		delete[] static_cast<uint8_t *>(m_lastIv);
		m_lastIv = NULL;
	}
	for (SwListIterator it = m_chunks.begin(); it != m_chunks.end(); ++it)
		delete SwPtrList<SdiBufferChunk>::deref(it);
	m_chunks.clear();
}

int SdiBufferCXD3705::reset()
{
	if (m_bound)
		return 0;
	return SdiBuffer::reset();
}

int SdiBufferCXD3705::setSize(unsigned int newSize)
{
	if (m_bound)
		return 0;
	return SdiBuffer::setSize(newSize);
}

int SdiBufferCXD3705::attachStream(int drvFd, int sessionId, uint8_t streamId,
				   int isReadSide, uint8_t dirFlag, void *streamCb,
				   void *eventCb, SdiStreamParams *params)
{
	SdiDevice *dev = NULL;
	int err = SdiDevice::getInstance(&dev);
	m_device = static_cast<SdiDeviceCXD3705 *>(dev);
	if (!m_device || err != 0)
		return 0;
	if (drvFd == -1)
		return 0;

	m_drvFd = drvFd;
	m_sessionId = sessionId;
	m_streamId = streamId;
	m_isReadSide = isReadSide;
	m_dirFlag = dirFlag;
	m_streamCb = streamCb;
	m_eventCb = eventCb;
	m_params = params;

	delete[] m_readVec;
	m_readVec = new (std::nothrow) SdiBufferChunk[10];
	m_writePos = 0;
	m_readPos = 0;
	return 1;
}

uint8_t *SdiBufferCXD3705::bindMemory(uint32_t *outSize)
{
	m_bound = 1;
	*outSize = m_capacity;
	if (m_chunkOffset >= 0) {
		m_scanCursor = (uint32_t)((uintptr_t)m_mem + (uint32_t)m_chunkOffset);
		m_hasChunkOffset = 1;
	} else {
		m_scanCursor = (uint32_t)(uintptr_t)m_mem;
		m_hasChunkOffset = 0;
	}
	return m_mem;
}

void SdiBufferCXD3705::setEndOfStream()
{
	if (m_isReadSide == 0) {
		if (!m_eof) {
			sendLastPacket();
			m_eof = 1;
		}
		SdiBuffer::setEndOfStream();
		return;
	}
	if (m_isReadSide == 1) {
		SdiBufferCXD3705 *aux = static_cast<SdiBufferCXD3705 *>(m_params->auxBuffer);
		if (aux && aux != this && !m_eof)
			aux->setEndOfStream();
		SdiBufferCXD3705 *mainBuf = static_cast<SdiBufferCXD3705 *>(m_params->mainBuffer);
		if (mainBuf && mainBuf != this && !m_eof)
			mainBuf->setEndOfStream();
		if (swIsTxStreamType(m_params->streamType)) {
			sendLastPacket();
			return;
		}
		for (;;) {
			SdiBufferChunk *list = NULL;
			int count = getReadChunkList(&list);
			if (count == -1)
				break;
			unsigned int sum = 0;
			for (int i = 0; i < count; ++i)
				sum += list[i].size;
			consume(sum);
		}
	}
	SdiBuffer::setEndOfStream();
}

int SdiBufferCXD3705::getNextChunk(void **outPtr)
{
	if (!outPtr)
		return -1;
	*outPtr = NULL;
	if (m_isReadSide != 0)
		return -1;
	SdiBufferChunk *list = NULL;
	int count = getWriteChunkList(&list);
	if (count == -1)
		return -1;
	if (count == 0)
		return 0;
	*outPtr = list[0].ptr;
	return (int)list[0].size;
}

int SdiBufferCXD3705::getWriteChunkList(SdiBufferChunk **outList)
{
	SdiBufferLock lock(this);
	if (!outList)
		return -1;
	*outList = NULL;
	if (m_isReadSide != 0)
		return -1;

	uint32_t info[2] = { 0, 0 };
	if (pollStream(info) != 3 && info[0] != 0 && info[1] != 0) {
		m_readPos = info[0] - (uint32_t)(uintptr_t)m_mem + info[1];
		if (m_readPos == m_capacity)
			m_readPos = 0;
		if (m_full)
			m_full = 0;
	}

	if (m_eof)
		return -1;

	*outList = m_writeVec;
	if (m_writePos == m_readPos) {
		if (m_full) {
			*outList = NULL;
			return 0;
		}
		m_writeVec[0].ptr = m_mem + m_writePos;
		m_writeVec[0].size = m_capacity - m_writePos;
		if (m_writePos != 0) {
			m_writeVec[1].ptr = m_mem;
			m_writeVec[1].size = m_writePos;
			return 2;
		}
		return 1;
	}
	if (m_writePos > m_readPos) {
		m_writeVec[0].ptr = m_mem + m_writePos;
		m_writeVec[0].size = m_capacity - m_writePos;
		if (m_readPos != 0) {
			m_writeVec[1].ptr = m_mem;
			m_writeVec[1].size = m_readPos;
			return 2;
		}
		return 1;
	}
	m_writeVec[0].ptr = m_mem + m_writePos;
	m_writeVec[0].size = m_readPos - m_writePos;
	return 1;
}

uint32_t SdiBufferCXD3705::scanPacketHeaders(uint32_t addr, uint32_t len, uint8_t *outPartial)
{
	*outPartial = 0;
	if (m_chunkMax == 0 || m_chunkAlign == 0)
		return len;
	uint32_t base = (uint32_t)(uintptr_t)m_mem;
	if (base > addr || len == 0)
		return 0;
	uint32_t end = base + m_capacity;
	uint32_t limit = addr + len;
	if (limit > end)
		return 0;

	uint32_t total = 0;
	while (limit >= m_scanCursor) {
		for (uint32_t i = 0; m_hdrAccumLen < 4 && limit > m_scanCursor + i; ++i)
			m_hdrAccum[m_hdrAccumLen++] = *(const uint8_t *)(uintptr_t)(m_scanCursor + i);
		if (m_hdrAccumLen >= 4) {
			uint32_t raw = 0;
			memcpy(&raw, m_hdrAccum, 4);
			total += swSwap32(raw) + m_chunkAlign;
			*outPartial = 0;
			m_hdrAccumLen = 0;
		}
		m_scanCursor += m_chunkMax;
		if (m_hasChunkOffset &&
		    limit >= m_scanCursor - (uint32_t)m_chunkOffset &&
		    limit <= m_scanCursor + 4) {
			*outPartial = 1;
		}
	}
	if (m_scanCursor > end) {
		m_scanCursor -= m_capacity;
		*outPartial = 0;
	}
	return total;
}

int SdiBufferCXD3705::append(unsigned int size)
{
	SdiBufferLock lock(this);
	if (m_isReadSide != 0)
		return 0;

	unsigned int commitLen = size;
	uint8_t partial = 1;
	if (m_chunkMax != 0 && m_chunkAlign != 0) {
		uint32_t base = (uint32_t)(uintptr_t)m_mem;
		if (m_writePos + size > m_capacity) {
			uint32_t wrap = m_writePos + size - m_capacity;
			m_pendingHeaderBytes += scanPacketHeaders(base + m_writePos,
								  m_capacity - m_writePos, &partial);
			m_pendingHeaderBytes += scanPacketHeaders(base, wrap, &partial);
		} else {
			m_pendingHeaderBytes += scanPacketHeaders(base + m_writePos, size, &partial);
		}
		if (!partial && m_pendingHeaderBytes < size)
			commitLen = m_pendingHeaderBytes;
		m_pendingHeaderBytes -= commitLen;
	}

	int ok = appendUnlocked(commitLen);
	if (commitLen != 0)
		updateStreamPosition();
	return ok;
}

int SdiBufferCXD3705::getNextReadChunk(void **outPtr)
{
	if (!outPtr)
		return -1;
	*outPtr = NULL;
	if (m_isReadSide != 1)
		return -1;
	SdiBufferChunk *list = NULL;
	int count = getReadChunkList(&list);
	if (count == -1)
		return -1;
	if (count == 0)
		return 0;
	*outPtr = list[0].ptr;
	return (int)list[0].size;
}

int SdiBufferCXD3705::getReadChunkList(SdiBufferChunk **outList)
{
	SdiBufferLock lock(this);
	if (!outList)
		return -1;
	*outList = NULL;
	if (m_isReadSide != 1)
		return -1;

	if (!m_stoppedRx) {
		if (m_params && m_params->auxBuffer && m_params->auxBuffer != this) {
			SdiBufferCXD3705 *aux = static_cast<SdiBufferCXD3705 *>(m_params->auxBuffer);
			if (m_isReadSide == 1 && aux->m_isReadSide == 0 && aux->m_eof == 1) {
				uint32_t info[2] = { 0, 0 };
				if (aux->pollStream(info) == 1 && info[1] == 0) {
					while (aux->stopStream() == 0)
						swYield();
					m_stoppedRx = 1;
				}
			}
		}
		if (m_params && m_params->mainBuffer && m_params->mainBuffer != this) {
			SdiBufferCXD3705 *mainBuf = static_cast<SdiBufferCXD3705 *>(m_params->mainBuffer);
			if (m_isReadSide == 1 && mainBuf->m_isReadSide == 0 &&
			    mainBuf->m_eof == 1 && mainBuf->available() == 0 &&
			    mainBuf->m_streamDone) {
				m_stoppedRx = (uint8_t)mainBuf->stopStream();
			}
		}
	}

	if (!isEndOfStream()) {
		SdiBufferChunk *chunk = new (std::nothrow) SdiBufferChunk;
		if (chunk) {
			chunk->ptr = NULL;
			chunk->size = 0;
			uint32_t info[2] = { 0, 0 };
			if (pollStream(info) != 3 && info[0] != 0 && info[1] != 0) {
				chunk->ptr = (void *)(uintptr_t)info[0];
				chunk->size = info[1];
				m_chunks.pushBackPtr(chunk);
			} else {
				delete chunk;
			}
		}
	}

	SwListIterator it = m_chunks.begin();
	if (it == m_chunks.end()) {
		if (!isEndOfStream()) {
			*outList = NULL;
			return 0;
		}
		return -1;
	}

	unsigned int count = 0;
	*outList = m_readVec;
	while (count < 10 && it != m_chunks.end()) {
		SdiBufferChunk *chunk = SwPtrList<SdiBufferChunk>::deref(it);
		if (chunk) {
			m_readVec[count].ptr = chunk->ptr;
			m_readVec[count].size = chunk->size;
			++count;
			++it;
		} else {
			it = m_chunks.erase(it);
		}
	}
	return (int)count;
}

int SdiBufferCXD3705::consume(unsigned int size)
{
	SdiBufferLock lock(this);
	if (m_isReadSide != 1)
		return 0;
	if (size == 0)
		return 1;

	uint32_t newReadPos = m_readPos;
	unsigned int remain = size;
	SwPtrList<SdiBufferChunk> backup;
	SwListIterator it = m_chunks.begin();

	while (remain != 0 && it != m_chunks.end()) {
		SdiBufferChunk *chunk = SwPtrList<SdiBufferChunk>::deref(it);
		if (!chunk) {
			it = m_chunks.erase(it);
			continue;
		}
		SdiBufferChunk *saved = new (std::nothrow) SdiBufferChunk;
		if (saved) {
			saved->ptr = chunk->ptr;
			saved->size = chunk->size;
			backup.pushFrontPtr(saved);
		}
		if (remain == chunk->size) {
			SwListIterator nextIt = m_chunks.erase(it);
			it = nextIt;
			if (it != m_chunks.end()) {
				SdiBufferChunk *nextChunk = SwPtrList<SdiBufferChunk>::deref(it);
				newReadPos = (uint32_t)((uintptr_t)nextChunk->ptr - (uintptr_t)m_mem);
				if (newReadPos > m_capacity)
					newReadPos = 0;
			} else {
				newReadPos = (uint32_t)((uintptr_t)chunk->ptr + chunk->size - (uintptr_t)m_mem);
			}
			remain = 0;
			delete chunk;
		} else if (remain > chunk->size) {
			remain -= chunk->size;
			it = m_chunks.erase(it);
			delete chunk;
		} else {
			chunk->ptr = static_cast<uint8_t *>(chunk->ptr) + remain;
			chunk->size -= remain;
			newReadPos = (uint32_t)((uintptr_t)chunk->ptr - (uintptr_t)m_mem);
			remain = 0;
		}
	}

	if (remain != 0) {
		for (SwListIterator bi = backup.begin(); bi != backup.end(); ++bi)
			m_chunks.pushFront(*bi);
		return 0;
	}

	for (SwListIterator bi = backup.begin(); bi != backup.end(); ++bi) {
		SdiBufferChunk *p = SwPtrList<SdiBufferChunk>::deref(bi);
		delete p;
	}
	backup.clear();

	m_readPos = newReadPos;
	if (m_readPos == m_capacity)
		m_readPos = 0;
	if (m_full)
		m_full = 0;
	if (!m_streamDone)
		updateStreamPosition();
	return 1;
}

int SdiBufferCXD3705::available()
{
	if (m_isReadSide != 0) {
		if (m_params && swIsTxStreamType(m_params->streamType)) {
			SdiBufferLock lock(this);
			uint32_t info[2] = { 0, 0 };
			if (pollStream(info) != 3 && info[0] != 0 && info[1] != 0) {
				SdiBufferChunk *chunk = new (std::nothrow) SdiBufferChunk;
				if (chunk) {
					chunk->ptr = (void *)(uintptr_t)info[0];
					chunk->size = info[1];
					m_chunks.pushBackPtr(chunk);
				}
			}
		}
	} else {
		SdiBufferLock lock(this);
		uint32_t info[2] = { 0, 0 };
		if (pollStream(info) != 3 && info[0] != 0 && info[1] != 0) {
			m_readPos = info[0] - (uint32_t)(uintptr_t)m_mem + info[1];
			if (m_readPos == m_capacity)
				m_readPos = 0;
			if (m_full)
				m_full = 0;
		}
	}
	return SdiBuffer::available();
}

int SdiBufferCXD3705::isEndOfStream()
{
	return m_streamDone != 0 ? 1 : 0;
}

int SdiBufferCXD3705::getLastIv(void *outIv8)
{
	if (!outIv8)
		return 5;
	if (!m_lastIv)
		return 4;
	memcpy(outIv8, m_lastIv, 8);
	return 0;
}

int SdiBufferCXD3705::setTimeout(int timeoutMs)
{
	if (timeoutMs < 0)
		return 5;
	m_timeoutMs = timeoutMs;
	return 0;
}

bool SdiBufferCXD3705::setChunkOffset(uint32_t off)
{
	if (m_bound)
		return false;
	m_chunkOffset = (int32_t)off;
	return true;
}

bool SdiBufferCXD3705::setChunkMax(uint32_t maxVal)
{
	if (m_bound)
		return false;
	m_chunkMax = maxVal;
	return true;
}

bool SdiBufferCXD3705::setChunkAlign(uint32_t alignVal)
{
	if (m_bound)
		return false;
	m_chunkAlign = alignVal;
	return true;
}

int SdiBufferCXD3705::isKeyExpiring()
{
	if (!m_device)
		return 0;
	return (m_device->getHwCapabilities() & 0x400000u) != 0 ? 1 : 0;
}

void SdiBufferCXD3705::updateStreamPosition()
{
	if (!m_device || m_streamDone)
		return;
	uint32_t pos = m_isReadSide ? m_readPos : m_writePos;
	if (pos == 0)
		pos = m_capacity;
	int err = m_device->updateStreamPos(m_drvFd, m_sessionId, m_streamId, &pos, m_dirFlag);
	if (err != 0) {
		m_eof = 1;
		setError(err);
		if (m_isReadSide == 1)
			m_readPos = m_writePos;
		else
			m_writePos = m_readPos;
		m_full = 0;
	}
}

int SdiBufferCXD3705::sendLastPacket()
{
	if (!m_device || m_lastPacketSent)
		return 0;
	if (m_device->setLastPacketIoctl(m_drvFd, m_sessionId, m_streamId) != 0) {
		m_retryLast = 1;
		return swYield();
	}
	m_lastPacketSent = 1;
	m_retryLast = 0;
	return 0;
}

int SdiBufferCXD3705::stopStream()
{
	if (!m_device)
		return 0;
	if (m_isReadSide == 0 && m_stoppedRx)
		return 1;
	uint32_t st = m_params ? m_params->streamType : 0;
	if (swIsTxStreamType(st) && m_stoppedTx)
		return 1;
	if ((st == 0x10010002u || st == 0x90010001u) && m_streamDone) {
		SdiBufferCXD3705 *aux = static_cast<SdiBufferCXD3705 *>(m_params->auxBuffer);
		SdiBufferCXD3705 *mainBuf = static_cast<SdiBufferCXD3705 *>(m_params->mainBuffer);
		if ((mainBuf && mainBuf->isStopped()) || (aux && aux->isStopped()))
			return 1;
	}
	if (m_retryLast) {
		sendLastPacket();
		if (m_retryLast)
			return 0;
	}
	int err = m_device->stopStreamIoctl(m_drvFd, m_sessionId, m_streamId, m_params);
	if (err == 9)
		return 0;
	if (err != 0) {
		m_eof = 1;
		return 1;
	}
	if (!swIsTxStreamType(st))
		m_eof = 1;
	if (swIsTxStreamType(st))
		m_stoppedTx = 1;
	if (swIsRxDirectType(st))
		m_stoppedRx = 1;
	if (st == 0x90010001u || st == 0x10010002u) {
		if (m_isReadSide == 1)
			m_stoppedTx = 1;
		else
			m_stoppedRx = 1;
	}
	return 1;
}

/* Callback dispatch helper interfaces matching CXD3705 notification slots */
struct SwCbRxNotify {
	virtual ~SwCbRxNotify() {}
	virtual void onNotifyBase(const char *tag, int *flags) = 0;
	virtual void onTxEvent() = 0;
	virtual void onEmiChange(uint32_t emi) = 0;
	virtual void onCciChange(uint32_t cci, uint32_t copyMode, uint8_t retention) = 0;
	virtual void onKeyExpired(int flag) = 0;
	virtual void onDiscard() = 0;
};

struct SwCbEventNotify {
	virtual ~SwCbEventNotify() {}
	virtual void onEventA() = 0;
	virtual void onEventB() = 0;
};

int SdiBufferCXD3705::pollStream(uint32_t outAddrLen[2])
{
	if (!m_device)
		return 3;
	outAddrLen[0] = 0;
	outAddrLen[1] = 0;
	if (m_streamDone)
		return 1;

	uint32_t st = m_params ? m_params->streamType : 0;
	if (swIsTxStreamType(st) && m_lastPacketSent && !m_stoppedTx)
		stopStream();

	int status = 0;
	int flags = 0;
	uint8_t cciInfo[48];
	memset(cciInfo, 0, sizeof(cciInfo));

	int err = m_device->getStreamStatus(m_drvFd, m_sessionId, m_streamId,
					    outAddrLen, cciInfo, &status, &flags,
					    m_dirFlag, &m_lastIv, m_timeoutMs);
	if (status != m_lastStatus || flags != m_lastErrorFlags) {
		m_lastStatus = status;
		m_lastErrorFlags = flags;
	}
	if (memcmp(cciInfo, m_lastCciInfo, sizeof(cciInfo)) != 0)
		memcpy(m_lastCciInfo, cciInfo, sizeof(cciInfo));

	if (err != 0) {
		status = 3;
		m_eof = 1;
		outAddrLen[0] = 0;
		outAddrLen[1] = 0;
		setError(1);
		return status;
	}

	if (m_retryLast)
		sendLastPacket();

	if (m_isReadSide == 1 && outAddrLen[0] != 0 && outAddrLen[1] != 0) {
		m_writePos = outAddrLen[1] + outAddrLen[0] - (uint32_t)(uintptr_t)m_mem;
		if (m_writePos == m_capacity)
			m_writePos = 0;
		if (m_writePos == m_readPos && outAddrLen[1] != 0)
			m_full = 1;
	}

	if ((status == 1 && outAddrLen[1] == 0) || status == 3) {
		m_streamDone = 1;
		m_eof = 1;
		return status;
	}

	void *rxCb = NULL;
	void *txCb = NULL;
	void *baseCb = NULL;
	if ((unsigned int)m_isReadSide <= 1u) {
		if (swIsAnyRxType(st) && m_streamCb) {
			rxCb = m_streamCb;
			intptr_t adj = *(*(intptr_t **)rxCb - 5);
			baseCb = (uint8_t *)rxCb + adj;
		} else if (swIsAnyTxType(st) && m_streamCb) {
			txCb = m_streamCb;
			intptr_t adj = *(*(intptr_t **)txCb - 5);
			baseCb = (uint8_t *)txCb + adj;
		}

		if (status == 2) {
			if ((flags & (int)0xC08DEC00u) != 0) {
				if (flags & 0x8000)       setError(11009);
				if (flags & 0x40000)      setError(1016);
				if (flags & 0x40000000)   setError(11011);
				if (flags < 0)            setError(11012);
				if (flags & 0x80000)      setError(11010);
				if (flags & 0x800000)     setError(1005);
				setError(11008);
				status = 3;

				while (stopStream() == 0) {
					if (swIsTxStreamType(st)) {
						bool updated = false;
						for (SwListIterator it = m_chunks.begin(); it != m_chunks.end(); ++it) {
							SdiBufferChunk *c = SwPtrList<SdiBufferChunk>::deref(it);
							if (c) {
								m_readPos = (uint32_t)((uintptr_t)c->ptr - (uintptr_t)m_mem + c->size);
								if (m_readPos == m_capacity)
									m_readPos = 0;
								m_full = 0;
								delete c;
								*it = 0;
								updated = true;
							}
						}
						m_chunks.clear();
						int st2 = pollStream(outAddrLen);
						if (st2 != 1 && st2 != 3) {
							if (outAddrLen[0] != 0 && outAddrLen[1] != 0) {
								m_readPos = m_writePos;
								m_full = 0;
								updated = true;
							}
							if (updated)
								updateStreamPosition();
						}
					}
				}

				do {
					swYield();
					if (m_device->getStreamStatus(m_drvFd, m_sessionId, m_streamId,
								      outAddrLen, cciInfo, &status, &flags,
								      m_dirFlag, &m_lastIv, m_timeoutMs) != 0) {
						status = 3;
						outAddrLen[1] = 0;
					}
					if (status != m_lastStatus || flags != m_lastErrorFlags) {
						m_lastStatus = status;
						m_lastErrorFlags = flags;
					}
					if (memcmp(cciInfo, m_lastCciInfo, sizeof(cciInfo)) != 0)
						memcpy(m_lastCciInfo, cciInfo, sizeof(cciInfo));
				} while (status == 0 || status == 2 || (status == 1 && outAddrLen[1] != 0));

				m_writePos = m_readPos;
				m_full = 0;
				m_eof = 1;
				m_streamDone = 1;
				m_stoppedTx = 1;
				m_stoppedRx = 1;
				m_lastPacketSent = 1;
				m_retryLast = 0;
			} else {
				if ((flags & 0x10) && txCb) {
					typedef void (*FnTx)(void *);
					(*(FnTx *)(*(uintptr_t *)txCb + 12))(txCb);
				}
				if ((flags & 0x3000160) && baseCb) {
					typedef void (*FnBase)(void *, const char *, int *);
					(*(FnBase *)(*(uintptr_t *)baseCb + 8))(baseCb, "CXD3705", &flags);
				}
				if ((flags & 0x200000) && m_eventCb) {
					typedef void (*FnEv)(void *);
					(*(FnEv *)(*(uintptr_t *)m_eventCb + 8))(m_eventCb);
				}
				if ((flags & 0x100000) && m_eventCb) {
					typedef void (*FnEv)(void *);
					(*(FnEv *)(*(uintptr_t *)m_eventCb + 12))(m_eventCb);
				}
				if ((flags & 0x8000000) && rxCb) {
					typedef void (*FnKey)(void *, int);
					(*(FnKey *)(*(uintptr_t *)rxCb + 24))(rxCb, 1);
				}
				if ((flags & 0x400000) && rxCb) {
					typedef void (*FnDisc)(void *);
					(*(FnDisc *)(*(uintptr_t *)rxCb + 28))(rxCb);
				}
			}
		}
	}

	if (!m_stoppedRx && swIsRxDecryptType(st) && m_dirFlag == 0 && rxCb) {
		uint8_t rawMode;
		uint8_t rawCci;
		if (m_params->sacSession != NULL) {
			memcpy(m_params->cciInfo, cciInfo, 48);
			rawMode = cciInfo[32];
			rawCci = cciInfo[33];
			m_lastRetention = (cciInfo[34] != 0) ? 1 : 0;
		} else {
			rawMode = cciInfo[0];
			rawCci = cciInfo[1];
			m_lastRetention = (cciInfo[2] != 0) ? 1 : 0;
		}

		uint32_t mappedCci = 255;
		switch (rawCci) {
		case 0:  mappedCci = 0; break;
		case 2:  mappedCci = 2; break;
		case 4:  mappedCci = 4; break;
		case 6:  mappedCci = 6; break;
		case 8:  mappedCci = 8; break;
		case 10: mappedCci = 10; break;
		case 12: mappedCci = 12; break;
		default: mappedCci = 255; break;
		}

		uint32_t mappedMode = 5;
		switch (rawMode) {
		case 0:  mappedMode = 0; break;
		case 2:  mappedMode = 1; break;
		case 4:
		case 6:  mappedMode = 2; break;
		case 8:
		case 10: mappedMode = 3; break;
		case 12: mappedMode = 4; break;
		default: mappedMode = 5; break;
		}

		if (mappedMode != m_lastCopyMode || mappedCci != m_lastCci) {
			m_lastCopyMode = mappedMode;
			m_lastCci = mappedCci;
			typedef void (*FnCci)(void *, uint32_t, uint32_t, uint32_t);
			(*(FnCci *)(*(uintptr_t *)rxCb + 20))(rxCb, m_lastCci, m_lastCopyMode, m_lastRetention);
		}

		uint32_t emi = 4;
		if (rawMode == 0 || rawMode == 2)
			emi = 0;
		else if (rawMode == 4)
			emi = 1;
		else if (rawMode == 8 || rawMode == 10)
			emi = 2;
		else if (rawMode == 6 || rawMode == 12)
			emi = 3;
		if (m_lastEmi != emi) {
			m_lastEmi = emi;
			typedef void (*FnEmi)(void *, uint32_t);
			(*(FnEmi *)(*(uintptr_t *)rxCb + 16))(rxCb, m_lastEmi);
		}
	}

	return status;
}

/* ---- SdiBufferFactoryCXD3705 ---- */

SdiBuffer *SdiBufferFactoryCXD3705::create(void *extMem, uint32_t capacity,
					   const SwString &label)
{
	return new (std::nothrow) SdiBufferCXD3705(extMem, capacity, label);
}

SdiBuffer *SdiBufferFactoryCXD3705::createWithChunks(const uint32_t *chunkMax,
						     const uint32_t *chunkAlign,
						     const uint32_t *chunkOff,
						     void *extMem, uint32_t capacity,
						     const SwString &label)
{
	uint32_t offCopy = chunkOff ? *chunkOff : 0;
	return new (std::nothrow) SdiBufferCXD3705(chunkMax, chunkAlign, &offCopy,
						   extMem, capacity, label);
}
