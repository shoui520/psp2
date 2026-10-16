/*
 * Cache helpers on ranges given by any address and size: the cache lines
 * (0x40 bytes) and L2 lines (0x100 bytes) that hold any of the range.
 */
#include "application.h"

#define LINE		0x40
#define L2_LINE		0x100

static uint32_t lineStart(uint32_t address, uint32_t line)
{
	return address & ~(line - 1);
}

static uint32_t lineEnd(uint32_t address, uint32_t size, uint32_t line)
{
	return (address + size + line - 1) & ~(line - 1);
}

/* dropped from this core's cache, flushed from the L2 */
void cacheInvalidateFlush(uint32_t address, uint32_t size)
{
	vnzCacheInvalidate(address, size);
	vnzL2Flush(address, size);
}

/* written back from this core's cache and dropped */
void cacheFlushRange(uint32_t address, uint32_t size)
{
	uint32_t start = lineStart(address, LINE);
	uint32_t length = lineEnd(address, size, LINE) - start;

	if (start && length) {
		vnzCacheFlush(start, length);
		mep_syncm();
	}
}

/* written back from both caches, the write seen through */
void cacheFlushWriteback(uint32_t address, uint32_t size)
{
	uint32_t start = lineStart(address, LINE);
	uint32_t length = lineEnd(address, size, LINE) - start;
	uint32_t l2Start = lineStart(address, L2_LINE);
	uint32_t l2Length = lineEnd(address, size, L2_LINE) - l2Start;

	if (start && length) {
		vnzCacheFlush(start, length);
		mep_syncm();
		vnzL2Writeback(l2Start, l2Length);
		(void)((volatile uint8_t *)(start + length))[-1];
	}
}

void cacheL2Writeback(uint32_t address, uint32_t size)
{
	uint32_t start = lineStart(address, L2_LINE);
	uint32_t length = lineEnd(address, size, L2_LINE) - start;

	if (start && length) {
		vnzL2Writeback(start, length);
		(void)((volatile uint8_t *)(start + length))[-1];
	}
}

/* dropped from this core's cache, written back first */
void cachePurgeRange(uint32_t address, uint32_t size)
{
	uint32_t start = lineStart(address, LINE);
	uint32_t length = lineEnd(address, size, LINE) - start;

	if (start && length)
		vnzCachePurge(start, length);
}

/* dropped from both caches, to read what another writer put there */
void cachePurgeInvalidate(uint32_t address, uint32_t size)
{
	uint32_t start = lineStart(address, LINE);
	uint32_t length = lineEnd(address, size, LINE) - start;
	uint32_t l2Start = lineStart(address, L2_LINE);
	uint32_t l2Length = lineEnd(address, size, L2_LINE) - l2Start;

	if (start && length) {
		vnzCachePurge(start, length);
		vnzL2Invalidate(l2Start, l2Length);
	}
}

void cacheL2Invalidate(uint32_t address, uint32_t size)
{
	uint32_t start = lineStart(address, L2_LINE);
	uint32_t length = lineEnd(address, size, L2_LINE) - start;

	if (start && length)
		vnzL2Invalidate(start, length);
}
