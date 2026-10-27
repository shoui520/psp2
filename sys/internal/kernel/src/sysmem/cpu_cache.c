/* CPU cache maintenance. */
#include <stdint.h>
#include <stddef.h>
#include <sysmem/barrier.h>
#include <sysmem/cp15.h>
#include <sysmem/cpu.h>

/* the Cortex-A9 L1 data cache: 32 KiB, 4 ways of 256 sets of 32-byte lines */
#define L1_CACHE_LINE_SHIFT	5
#define L1_CACHE_LINE		(1 << L1_CACHE_LINE_SHIFT)
#define L1_DCACHE_WAY_SIZE	0x2000		/* the set field ends here */
#define L1_DCACHE_WAY_STEP	0x40000000	/* the way field is bits 31:30 */

/* the lines a range touches, less one */
#define L1_LINES_AFTER_FIRST(address, size) \
	((((address) + (size) - 1) >> L1_CACHE_LINE_SHIFT) - ((address) >> L1_CACHE_LINE_SHIFT))

#if !SYSMEM_NSKBL || defined(SYSMEM_SKBL)
int SceCpuForKernel_15A48AB4(void)
{
	writeIciallu(0);
	dsb();
	isb();
	return 0;
}
#endif

int sceKernelL1IcacheInvalidateEntireAllCore(void)
{
	writeIcialluis(0);
	dsb();
	isb();
	return 0;
}

#if !SYSMEM_NSKBL
void SceCpuForKernel_7ED856FE(const void *address)
{
	writeDcimvac(address);
	dsb();
}

void SceCpuForKernel_74B7F445(const void *address)
{
	writeDccmvac(address);
	dsb();
}

void SceCpuForKernel_597426D2(const void *address)
{
	writeDccimvac(address);
	dsb();
}
#endif

void SceCpuForKernel_2E637B1D(const void *start, uint32_t size)
{
	uint32_t address = (uintptr_t)start;
	uint32_t remaining = L1_LINES_AFTER_FIRST(address, size);

	do {
		writeIcimvau(address);
		address += L1_CACHE_LINE;
	} while (remaining--);
	dsb();
}

void SceCpuForKernel_4236E06A(const void *start, uint32_t size)
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

void SceCpuForKernel_70350360(const void *start, uint32_t size)
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

void SceCpuForKernel_4F442396(const void *start, uint32_t size)
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

#if !SYSMEM_NSKBL || defined(SYSMEM_SKBL)
void SceCpuForKernel_F9B0B171(void)
{
	uint32_t setWay = 0;

	do {
		/* every way of the set, until the way field wraps */
		do {
			writeDcisw(setWay);
			setWay += L1_DCACHE_WAY_STEP;
		} while (setWay >= L1_DCACHE_WAY_STEP);
		setWay += L1_CACHE_LINE;
	} while (setWay < L1_DCACHE_WAY_SIZE);
	dsb();
}

void SceCpuForKernel_49180814(void)
{
	uint32_t setWay = 0;

	do {
		/* every way of the set, until the way field wraps */
		do {
			writeDccsw(setWay);
			setWay += L1_DCACHE_WAY_STEP;
		} while (setWay >= L1_DCACHE_WAY_STEP);
		setWay += L1_CACHE_LINE;
	} while (setWay < L1_DCACHE_WAY_SIZE);
	dsb();
}

void SceCpuForKernel_4BBA5C82(void)
{
	uint32_t setWay = 0;

	do {
		/* every way of the set, until the way field wraps */
		do {
			writeDccisw(setWay);
			setWay += L1_DCACHE_WAY_STEP;
		} while (setWay >= L1_DCACHE_WAY_STEP);
		setWay += L1_CACHE_LINE;
	} while (setWay < L1_DCACHE_WAY_SIZE);
	dsb();
}
#endif
