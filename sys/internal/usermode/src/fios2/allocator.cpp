/*
 * allocator.cpp: the allocator of the library's ops, file and directory handles and chunks
 * (fios2_internal.h). Sony's file name; the method names but AtomicToggleBits are ours.
 */
#include <algorithm>
#include <sce_atomic.h>
#include "fios2_internal.h"

/*
 * Lay the buffer out as units and the bitmap behind them, as many units as fit with the
 * bitmap padded to 8 bytes. maxCount bounds the units: it may be reached only by a layout
 * that uses the whole buffer.
 */
int Allocator::Init(SceFiosBuffer buffer, uint32_t maxCount, size_t unitSize, size_t alignment)
{
	uint32_t words, slack, count, bitmapSize, padding, previous;
	size_t dataSize;

	m_alignment = alignment;
	m_unitSize = SCE_FIOS_ALIGN_UP(unitSize, alignment);
	if ((uintptr_t)buffer.pPtr & (alignment - 1))
		return SCE_FIOS_ERROR_BAD_ALIGNMENT;
	if (buffer.pPtr == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (buffer.length < m_unitSize + 1)
		return SCE_FIOS_ERROR_BAD_SIZE;

	words = buffer.length & ~3;
	slack = buffer.length - words;
	count = (buffer.length / 4) * 32 / (m_unitSize * 8 + 1);
	if (count == 0)
		return SCE_FIOS_ERROR_BAD_SIZE;
	bitmapSize = (count + 7) / 8;
	dataSize = count * m_unitSize;
	padding = SCE_FIOS_ALIGN_UP(bitmapSize, 8) - bitmapSize;
	previous = 0;
	while (padding != 0 && previous != bitmapSize) {
		previous = bitmapSize;
		count = (words - padding) * 8 / (m_unitSize * 8 + 1);
		bitmapSize = (count + 7) / 8;
		dataSize = count * m_unitSize;
		padding = SCE_FIOS_ALIGN_UP(bitmapSize, 8) - bitmapSize;
	}
	while (dataSize + slack + padding + bitmapSize > buffer.length) {
		count--;
		bitmapSize = (count + 7) / 8;
		dataSize -= m_unitSize;
		padding = SCE_FIOS_ALIGN_UP(bitmapSize, 8) - bitmapSize;
	}
	if (count == 0)
		return SCE_FIOS_ERROR_BAD_SIZE;
	bitmapSize += padding;
	if (maxCount < count || (count == maxCount && dataSize + bitmapSize != buffer.length))
		return SCE_FIOS_ERROR_BAD_SIZE;

	m_allocated = 0;
	m_maxAllocated = 0;
	m_lastIndex = (uint32_t)-1;
	m_count = count;
	m_generation = 1;
	m_pBase = (uint8_t *)buffer.pPtr;
	m_pBitmap = (uint8_t *)buffer.pPtr + buffer.length - bitmapSize;
	sceClibMemset(buffer.pPtr, 0, buffer.length);
	m_initialized = true;
	GetLastIndex();
	return SCE_FIOS_OK;
}

void Allocator::Term()
{
	if (!m_initialized)
		return;
	GetLastIndex();
	if (m_allocated != 0)
		sceFiosPrintf("fios2 warning: terminated with %d bytes (%d units) of %ss still allocated\n",
			      m_allocated * m_unitSize, m_allocated, m_pName);
	m_initialized = false;
	if (m_pBase != NULL)
		sceClibMemset(m_pBase, 0, m_unitSize * m_count);
	if (m_pBitmap != NULL)
		sceClibMemset(m_pBitmap, 0, (m_count + 7) / 8);
	m_allocated = 0;
	m_lastIndex = (uint32_t)-1;
	m_count = 0;
	m_pBase = NULL;
	m_pBitmap = NULL;
}

/* the bit of unit index in the bitmap, most significant first */
static inline bool isBitSet(const uint8_t *pBitmap, uint32_t index)
{
	return (pBitmap[index >> 3] & (1 << (7 - (index & 7)))) != 0;
}

/* whether p is the start of count allocated units */
bool Allocator::IsAllocated(const void *p, uint32_t count) const
{
	uint32_t total = m_count;
	const uint8_t *pBase = m_pBase;
	size_t unitSize = m_unitSize;
	uint32_t index, end;

	if ((const uint8_t *)p < pBase)
		return false;
	if ((const uint8_t *)p > pBase + (total - count) * unitSize)
		return false;
	index = ((const uint8_t *)p - pBase) / unitSize;
	if ((const uint8_t *)p != pBase + index * unitSize)
		return false;
	if (!m_initialized)
		return false;
	if (count > total)
		return false;
	if (index >= total)
		return false;
	end = count + index;
	if (end > total)
		return false;
	for (; index < end; index++)
		if (!isBitSet(m_pBitmap, index))
			return false;
	return true;
}

/* a bitmap word as a number whose most significant bit is the first unit's */
static inline uint64_t bitmapWordToBits(uint64_t word)
{
	return (word >> 56) | ((word >> 40) & 0xff00) | ((word >> 24) & 0xff0000) |
	       ((word >> 8) & 0xff000000) | ((word << 8) & 0xff00000000ULL) |
	       ((word << 24) & 0xff0000000000ULL) | ((word << 40) & 0xff000000000000ULL) |
	       (word << 56);
}

/*
 * Set (or clear) the bits of count units from index, a 64-bit word at a time, each word
 * changed only if every one of its bits was clear (or set). When a bit is not, the words
 * already changed are changed back and false is returned.
 */

bool Allocator::AtomicToggleBits(uint32_t index, uint32_t count, int set)
{
	uint32_t first = index, end = index + count;
	uint32_t word;

	for (word = index & ~63; word < end; word += 64) {
		uint32_t firstBit = std::max(word, std::min(word + 63, index)) & 63;
		uint32_t lastBit = std::max(word, std::min(end - 1, word + 63)) & 63;
		volatile uint64_t *pWord = (volatile uint64_t *)(m_pBitmap + word / 8);
		uint64_t old, desired;

		do {
			uint64_t bits, expected;
			int64_t mask, expectedBit, desiredBit;

			old = *pWord;
			bits = bitmapWordToBits(old);
			expected = bits;
			desired = bits;
			mask = (int64_t)1 << (63 - firstBit);
			expectedBit = (int64_t)(int)!set << (63 - firstBit);
			desiredBit = (int64_t)(uint64_t)(set != 0) << (63 - firstBit);
			for (uint32_t i = firstBit; i <= lastBit; i++) {
				expected = (expected & ~mask) | expectedBit;
				desired = (desired & ~mask) | desiredBit;
				mask >>= 1;
				expectedBit >>= 1;
				desiredBit >>= 1;
			}
			if (bits != expected) {
				if (!set)
					sceFiosPrintf("FIOS: unexpected failure during deallocation in AtomicToggleBits\n");
				if (first < word)
					AtomicToggleBits(first, word - first, !set);
				return false;
			}
		} while (sceAtomicCompareAndSwap64Relaxed((volatile int64_t *)pWord, old,
							  bitmapWordToBits(desired)) != (int64_t)old);
	}
	return true;
}

/* count consecutive free units, searched from after the last allocation: the first's index */
int Allocator::Allocate(uint32_t count)
{
	uint32_t total, tried, index, end, i, allocated, max;
	uint8_t *p;

	if (!m_initialized)
		return -1;
	total = m_count;
	if (m_allocated + count > total)
		return -1;
	tried = 0;
	index = m_lastIndex + 1;
	if (total == 0)
		return -1;
	for (;;) {
		end = index + count;
		if (end > total) {
			m_generation++;
			index = 0;
			end = count;
			if (m_generation == 0x7fff)
				m_generation = 1;
		}
		for (i = index; i < end; i++)
			if (isBitSet(m_pBitmap, i))
				break;
		if (i < end) {
			tried += i - index;
			index = i;
		} else {
			p = m_pBase + index * m_unitSize;
			if (AtomicToggleBits(index, count, true))
				break;
		}
		tried++;
		index++;
		if (tried >= total)
			return -1;
	}
	allocated = sceAtomicAdd32Relaxed((volatile int32_t *)&m_allocated, count) + count;
	do {
		max = m_maxAllocated;
		if (allocated <= max)
			break;
	} while ((uint32_t)sceAtomicCompareAndSwap32Relaxed((volatile int32_t *)&m_maxAllocated,
							    max, allocated) != max);
	m_lastIndex = index + count - 1;
	sceClibMemset(p, 0, count * m_unitSize);
	return index;
}

void Allocator::Free(uint32_t index, uint32_t count)
{
	if (count > m_allocated)
		return;
	if (index >= m_count || index + count > m_count)
		return;
	if (AtomicToggleBits(index, count, false))
		sceAtomicAdd32Relaxed((volatile int32_t *)&m_allocated, -(int32_t)count);
}
