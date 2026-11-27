/*
 * SceLibMp4Recorder sample ring buffer and iterator for interleaved MP4 chunks.
 */
#include "mp4rec_internal.h"

SampleRingBuffer::SampleRingBuffer()
{
	m_reserved = 0;
	m_dataCapacity = 0;
	m_maxSamples = 0;
	m_dataBase = NULL;
	m_writePtr = NULL;
	m_spans = NULL;
	m_spanCount = 0;
	m_chunkCounts = NULL;
	m_chunkCount = 0;
	m_lastMarkedSample = 0;
	m_totalPushed = 0;
	m_prevMarkedTotal = 0;
	m_defaultLimit = 1000;
}

SampleRingBuffer::~SampleRingBuffer()
{
}

uint32_t SampleRingBuffer::CalcWorkMemSize(uint32_t dataSize, uint32_t maxSamples)
{
	return dataSize + 8U * maxSamples + 4U * maxSamples;
}

int32_t SampleRingBuffer::Term(MemAllocator *allocator)
{
	if (allocator == NULL || allocator->m_initialized == 0)
		return MP4REC_AVREC_ERR_INVALID_VALUE;

	if (m_dataBase != NULL) {
		allocator->Free(m_dataBase);
		m_dataBase = NULL;
	}
	if (m_spans != NULL) {
		allocator->Free(m_spans);
		m_spans = NULL;
	}
	if (m_chunkCounts != NULL) {
		allocator->Free(m_chunkCounts);
		m_chunkCounts = NULL;
	}
	return 0;
}

int32_t SampleRingBuffer::Init(uint32_t dataSize, uint32_t maxSamples, MemAllocator *allocator)
{
	if (m_dataBase != NULL || m_spans != NULL || m_chunkCounts != NULL)
		return MP4REC_AVREC_ERR_FATAL;
	if (dataSize == 0 || maxSamples == 0 || allocator == NULL || allocator->m_initialized == 0)
		return MP4REC_AVREC_ERR_INVALID_VALUE;

	m_dataBase = (uint8_t *)allocator->Malloc(dataSize);
	m_spans = (SampleSpan *)allocator->Malloc(maxSamples * sizeof(SampleSpan));
	m_chunkCounts = (uint32_t *)allocator->Malloc(maxSamples * sizeof(uint32_t));

	if (m_dataBase != NULL && m_spans != NULL && m_chunkCounts != NULL) {
		m_dataCapacity = dataSize;
		m_maxSamples = maxSamples;
		m_writePtr = m_dataBase;
		m_spanCount = 0;
		m_chunkCount = 0;
		m_lastMarkedSample = 0;
		m_totalPushed = 0;
		return 0;
	}

	Term(allocator);
	return MP4REC_AVREC_ERR_OUT_OF_MEMORY;
}

int32_t SampleRingBuffer::Push(const void *data, size_t size)
{
	if (m_dataBase == NULL || m_spans == NULL || m_chunkCounts == NULL)
		return MP4REC_AVREC_ERR_FATAL;
	if (data == NULL || size == 0)
		return MP4REC_AVREC_ERR_INVALID_VALUE;
	if (m_spanCount >= m_maxSamples)
		return MP4REC_AVREC_ERR_SAMPLE_NUM_FULL;

	uint8_t *dst;
	if (m_spanCount == 0) {
		m_writePtr = m_dataBase;
		dst = m_dataBase;
		if (size > m_dataCapacity)
			return MP4REC_AVREC_ERR_OUT_OF_MEMORY;
	} else {
		dst = m_writePtr;
		uint8_t *oldest = (uint8_t *)m_spans[0].data;
		if (dst + size > m_dataBase + m_dataCapacity) {
			if (oldest >= dst)
				return MP4REC_AVREC_ERR_BUFFER_FULL;
			m_writePtr = m_dataBase;
			dst = m_dataBase;
		}
		if (dst <= oldest && oldest < dst + size)
			return MP4REC_AVREC_ERR_BUFFER_FULL;
	}

	memcpy(dst, data, size);
	m_spans[m_spanCount].data = m_writePtr;
	m_spans[m_spanCount].size = (uint32_t)size;
	m_writePtr += size;
	m_spanCount++;
	m_totalPushed++;
	return 0;
}

uint32_t SampleRingBuffer::GetTotalPushed()
{
	return m_totalPushed;
}

int32_t SampleRingBuffer::MarkChunkAt(uint32_t sampleIndex)
{
	if (m_dataBase == NULL || m_spans == NULL || m_chunkCounts == NULL)
		return MP4REC_AVREC_ERR_FATAL;
	if (sampleIndex < m_lastMarkedSample)
		return MP4REC_AVREC_ERR_INVALID_VALUE;
	if (m_chunkCount >= m_maxSamples)
		return MP4REC_AVREC_ERR_OUT_OF_MEMORY;

	m_chunkCounts[m_chunkCount] = sampleIndex - m_lastMarkedSample;
	m_chunkCount++;
	m_lastMarkedSample = sampleIndex;
	m_prevMarkedTotal = m_totalPushed;
	return 0;
}

int32_t SampleRingBuffer::MarkChunkNow()
{
	return MarkChunkAt(m_totalPushed);
}

bool SampleRingBuffer::HasReadyChunk()
{
	if (m_chunkCount != 0)
		return m_spanCount >= m_chunkCounts[0];
	return false;
}

SampleIterator SampleRingBuffer::GetReadyChunkIterator()
{
	if (m_dataBase != NULL && m_spans != NULL && m_chunkCounts != NULL) {
		if (m_chunkCount != 0 && m_spanCount >= m_chunkCounts[0])
			return SampleIterator(m_spans, m_chunkCounts[0]);
	}
	return SampleIterator(NULL, 0);
}

int32_t SampleRingBuffer::ConsumeReadyChunk()
{
	if (m_dataBase == NULL || m_spans == NULL || m_chunkCounts == NULL)
		return MP4REC_AVREC_ERR_FATAL;
	if (m_chunkCount == 0)
		return 0;

	uint32_t take = m_chunkCounts[0];
	if (m_spanCount < take)
		return 0;

	if (take != 0) {
		uint32_t remain = m_spanCount - take;
		m_spanCount = remain;
		memmove(m_spans, &m_spans[take], remain * sizeof(SampleSpan));
	}
	m_chunkCount--;
	memmove(m_chunkCounts, &m_chunkCounts[1], m_chunkCount * sizeof(uint32_t));
	return 0;
}

SampleIterator SampleRingBuffer::GetAllSamplesIterator()
{
	if (m_dataBase != NULL && m_spans != NULL && m_chunkCounts != NULL)
		return SampleIterator(m_spans, m_spanCount);
	return SampleIterator(NULL, 0);
}

uint32_t SampleRingBuffer::GetFreeSpace()
{
	if (m_spanCount == 0)
		return m_dataCapacity;

	uint32_t writeAddr = (uint32_t)(uintptr_t)m_writePtr;
	uint32_t oldestAddr = (uint32_t)(uintptr_t)m_spans[0].data;
	uint32_t baseAddr = (uint32_t)(uintptr_t)m_dataBase;
	uint32_t endAddr = baseAddr + m_dataCapacity;

	if (writeAddr <= oldestAddr)
		return oldestAddr - writeAddr;

	uint32_t headSpace = oldestAddr - baseAddr;
	uint32_t tailSpace = endAddr - writeAddr;
	return (tailSpace > headSpace) ? tailSpace : headSpace;
}

SampleIterator::SampleIterator(SampleSpan *spans, uint32_t count)
{
	m_spans = spans;
	m_count = count;
	m_index = 0;
}

SampleIterator::~SampleIterator()
{
}

SampleSpan *SampleIterator::Next()
{
	if (m_index < m_count)
		return &m_spans[m_index++];
	return NULL;
}
