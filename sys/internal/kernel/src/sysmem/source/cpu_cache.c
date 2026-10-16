/* CPU cache maintenance. */
#include <stdint.h>
#include <stddef.h>
#include <sce/barrier.h>
#include <sce/cp15.h>

/* the Cortex-A9 L1 data cache: 32 KiB, 4 ways of 256 sets of 32-byte lines */
#define L1_CACHE_LINE_SHIFT	5
#define L1_CACHE_LINE		(1 << L1_CACHE_LINE_SHIFT)
#define L1_DCACHE_WAY_SIZE	0x2000		/* the set field ends here */
#define L1_DCACHE_WAY_STEP	0x40000000	/* the way field is bits 31:30 */

/* the lines a range touches, less one */
#define L1_LINES_AFTER_FIRST(address, size) \
	((((address) + (size) - 1) >> L1_CACHE_LINE_SHIFT) - ((address) >> L1_CACHE_LINE_SHIFT))

int ksceKernelL1IcacheInvalidateEntire(void)
{
	writeIciallu(0);
	dsb();
	isb();
	return 0;
}

int ksceKernelL1IcacheInvalidateEntireAllCore(void)
{
	writeIcialluis(0);
	dsb();
	isb();
	return 0;
}

void ksceKernelL1DcacheInvalidate(const void *address)
{
	writeDcimvac(address);
	dsb();
}

void ksceKernelL1DcacheClean(const void *address)
{
	writeDccmvac(address);
	dsb();
}

void ksceKernelL1DcacheCleanInvalidate(const void *address)
{
	writeDccimvac(address);
	dsb();
}

void ksceKernelL1IcacheInvalidateRange(const void *start, uint32_t size)
{
	uint32_t address = (uintptr_t)start;
	uint32_t remaining = L1_LINES_AFTER_FIRST(address, size);

	do {
		writeIcimvau(address);
		address += L1_CACHE_LINE;
	} while (remaining--);
	dsb();
}

void ksceKernelL1DcacheInvalidateRange(const void *start, uint32_t size)
{
	uint32_t address = (uintptr_t)start;
	uint32_t remaining = L1_LINES_AFTER_FIRST(address, size);

	dsb();
	do {
		writeDcimvac(address);
		address += L1_CACHE_LINE;
	} while (remaining--);
	dsb();
}

void ksceKernelL1DcacheCleanRange(const void *start, uint32_t size)
{
	uint32_t address = (uintptr_t)start;
	uint32_t remaining = L1_LINES_AFTER_FIRST(address, size);

	dsb();
	do {
		writeDccmvac(address);
		address += L1_CACHE_LINE;
	} while (remaining--);
	dsb();
}

void ksceKernelL1DcacheCleanInvalidateRange(const void *start, uint32_t size)
{
	uint32_t address = (uintptr_t)start;
	uint32_t remaining = L1_LINES_AFTER_FIRST(address, size);

	dsb();
	do {
		writeDccimvac(address);
		address += L1_CACHE_LINE;
	} while (remaining--);
	dsb();
}

void ksceKernelL1DcacheInvalidateAll(void)
{
	uint32_t set, way;

	for (set = 0; set < L1_DCACHE_WAY_SIZE; set += L1_CACHE_LINE) {
		way = 0;
		do {
			writeDcisw(set | way);
			way += L1_DCACHE_WAY_STEP;
		} while (way);
	}
	dsb();
}

void ksceKernelL1DcacheCleanAll(void)
{
	uint32_t set, way;

	for (set = 0; set < L1_DCACHE_WAY_SIZE; set += L1_CACHE_LINE) {
		way = 0;
		do {
			writeDccsw(set | way);
			way += L1_DCACHE_WAY_STEP;
		} while (way);
	}
	dsb();
}

void ksceKernelL1DcacheCleanInvalidateAll(void)
{
	uint32_t set, way;

	for (set = 0; set < L1_DCACHE_WAY_SIZE; set += L1_CACHE_LINE) {
		way = 0;
		do {
			writeDccisw(set | way);
			way += L1_DCACHE_WAY_STEP;
		} while (way);
	}
	dsb();
}
