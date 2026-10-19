/*
 * SceSysmem: cache maintenance over a virtual range, through both levels.
 * The L1 data cache is maintained by virtual address; the L2 (PL310) by
 * physical address, one page at a time. Uncached ranges are skipped when the
 * caller asks for it; the "ForL1WBWA" variants maintain L1 only.
 */

#include <sysmem/cp15.h>
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <fw.h>
#include <sysmem/debug.h>
#include <sysmem/cpu.h>

/* what the range functions do */
#define CACHE_RANGE_SKIP_UNCACHED	0x01	/* leave uncached ranges alone */
#define CACHE_RANGE_L1_ONLY		0x20	/* L1 is write-back write-allocate, no L2 */

/* the memory types SceCpuForKernel_87F7631D reports */
#define MMU_MEMORY_TYPE_UNCACHED_2	0x02
#define MMU_MEMORY_TYPE_UNCACHED_8	0x08
#define MMU_MEMORY_TYPE_UNCACHED_40	0x40
#define MMU_MEMORY_TYPE_UNCACHED_80	0x80
#define MMU_MEMORY_TYPE_CACHED_D0	0xd0

#define PAGE_SIZE			0x1000
#define PAGE_OFFSET(_a)			((_a) & (PAGE_SIZE - 1))

/* cpu_cache.c: the L2 controller */
void CpuL2InvalidateRangePA(uintptr_t pa, SceSize size);
void CpuL2CleanRangePA(uintptr_t pa, SceSize size);
void CpuL2CleanInvalidateRangePA(uintptr_t pa, SceSize size);

/*
 * The range functions take the exported name and the caller's return address
 * for the misalignment warning, which only debug builds print.
 */
#if FW_DEBUG
#define CACHE_CALLER_PARAMS		, const char *func, const void *lr
#define CACHE_CALLER(_func)		, (_func), __builtin_return_address(0)
#else
#define CACHE_CALLER_PARAMS
#define CACHE_CALLER(_func)
#endif

/* the preload engine (PLE) is usable when NSACR grants it */
static inline int CachePleEnabled(void)
{
	return (readNsacr() & NSACR_PLE) != 0;
}

/* name unknown: stops the PLE and waits until its channel is idle */
__attribute__((noinline))
void CachePleFlush(void)
{
	CP_WRITE(p15, 0, c11, c2, 1, 0);	/* stop the channel */
	while (readPleasr() & 1)
		CP_WRITE(p15, 0, c11, c3, 2, 0);	/* kill it */
}

SceBool sceKernelIsUncacheAddressInTmpFsGame(const void *address)
{
	int type = SceCpuForKernel_87F7631D((uintptr_t)address);

	switch (type) {
	case MMU_MEMORY_TYPE_UNCACHED_2:
	case MMU_MEMORY_TYPE_UNCACHED_8:
	case MMU_MEMORY_TYPE_UNCACHED_40:
	case MMU_MEMORY_TYPE_UNCACHED_80:
		return 1;
	case MMU_MEMORY_TYPE_CACHED_D0:
		return 0;
	default:
		SCE_DBG_PRINTF_FUNC(0xb, 88, "sceKernelIsUncacheAddressInTmpFsGame",
				    "Warning:sceKernelMMUGetMemoryType failed 0x%08x\n", type);
		return 0;
	}
}

/* the uncached windows of the kernel address space */
static inline int CacheIsUncachedAddress(const void *start)
{
	uintptr_t address = (uintptr_t)start;

	if (address - 0x18000000 < 0x08000000)
		return sceKernelIsUncacheAddressInTmpFsGame(start);
	return address - 0x20000000 < 0x20000000 || address - 0x60000000 < 0x21000000;
}

__attribute__((noinline))
void _sceKernelDcacheCleanInvalidateRangeCore(const void *start, SceSize size, int flags
					      CACHE_CALLER_PARAMS)
{
	uintptr_t address = (uintptr_t)start, va, pa;
	SceSize remaining, chunk, head;

	if (((uintptr_t)start | size) & 0x3f)
		SCE_DBG_PRINTF(3, 527, "%s:Warning: start=%p, size=0x%08x, lr=%p\n",
			       func, start, size, lr);
	if (size == 0)
		return;
	if ((flags & CACHE_RANGE_SKIP_UNCACHED) && CacheIsUncachedAddress(start))
		return;
	if (CachePleEnabled())
		CachePleFlush();
	SceCpuForKernel_4F442396(start, size);
	if (flags & CACHE_RANGE_L1_ONLY)
		return;

	va = address;
	chunk = PAGE_SIZE - PAGE_OFFSET(address);
	remaining = size;
	for (;;) {
		if (SceCpuForKernel_14E0B0C4(va, &pa) < 0)
			SCE_PANIC(566);
		if (chunk > remaining)
			chunk = remaining;
		CpuL2CleanInvalidateRangePA(pa, chunk);
		remaining -= chunk;
		if (remaining == 0)
			break;
		va += chunk;
		chunk = PAGE_SIZE;
	}
	CpuL2CacheSyncCore();

	/* lines only partly in the range may have been refilled from L2 meanwhile */
	head = address & 0x3f;
	if (head != 0) {
		address -= head;
		SceCpuForKernel_4F442396((const void *)address, 0x40);
		if (size < 0x40 - head)
			return;
		size = size - 0x40 + head;
		address += 0x40;
	}
	if (size & 0x3f) {
		size &= ~0x3f;
		SceCpuForKernel_4F442396((const void *)(address + size), 0x40);
		if (size == 0)
			return;
	}
	SceCpuForKernel_4F442396((const void *)address, size);
}

__attribute__((noinline))
void _sceKernelDcacheCleanRangeCore(const void *start, SceSize size, int flags
				    CACHE_CALLER_PARAMS)
{
	uintptr_t va = (uintptr_t)start, pa;
	SceSize remaining, chunk;

	if (((uintptr_t)start | size) & 0x1f)
		SCE_DBG_PRINTF(3, 359, "%s:Warning: start=%p, size=0x%08x, lr=%p\n",
			       func, start, size, lr);
	if (size == 0)
		return;
	if ((flags & CACHE_RANGE_SKIP_UNCACHED) && CacheIsUncachedAddress(start))
		return;
	SceCpuForKernel_70350360(start, size);
	if (flags & CACHE_RANGE_L1_ONLY)
		return;

	chunk = PAGE_SIZE - PAGE_OFFSET(va);
	remaining = size;
	for (;;) {
		if (SceCpuForKernel_14E0B0C4(va, &pa) < 0)
			SCE_PANIC(406);
		if (chunk > remaining)
			chunk = remaining;
		CpuL2CleanRangePA(pa, chunk);
		remaining -= chunk;
		if (remaining == 0)
			break;
		va += chunk;
		chunk = PAGE_SIZE;
	}
	CpuL2CacheSyncCore();
}

__attribute__((noinline))
void _sceKernelDcacheInvalidateRangeCore(const void *start, SceSize size, int flags
					 CACHE_CALLER_PARAMS)
{
	uintptr_t va = (uintptr_t)start, pa;
	SceSize remaining, chunk;

	if (((uintptr_t)start | size) & 0x3f) {
		SCE_DBG_PRINTF(3, 193, "%s:Error: start=%p, size=0x%08x, lr=%p\n",
			       func, start, size, lr);
		/* a partial line would lose its neighbours' data */
		if (((uintptr_t)start | size) & 0x1f)
			SCE_DBG_ASSERT_CHECK(0, 195);
	}
	if (size == 0)
		return;
	if ((flags & CACHE_RANGE_SKIP_UNCACHED) && CacheIsUncachedAddress(start))
		return;
	if (CachePleEnabled())
		CachePleFlush();
	SceCpuForKernel_4236E06A(start, size);
	if (flags & CACHE_RANGE_L1_ONLY)
		return;

	chunk = PAGE_SIZE - PAGE_OFFSET(va);
	remaining = size;
	for (;;) {
		if (SceCpuForKernel_14E0B0C4(va, &pa) < 0)
			SCE_PANIC(234);
		if (chunk > remaining)
			chunk = remaining;
		CpuL2InvalidateRangePA(pa, chunk);
		remaining -= chunk;
		if (remaining == 0)
			break;
		va += chunk;
		chunk = PAGE_SIZE;
	}
	CpuL2CacheSyncCore();
	SceCpuForKernel_4236E06A(start, size);
}

void sceKernelIcacheInvalidateRange(const void *start, SceSize size)
{
	uintptr_t va = (uintptr_t)start, pa;
	SceSize remaining, chunk;

	if (((uintptr_t)start | size) & 0x3f)
		SCE_DBG_PRINTF_FUNC(0xb, 101, "sceKernelIcacheInvalidateRange",
				    "Error: start=%p, size=0x%08x, lr=%p\n",
				    start, size, __builtin_return_address(0));
	if (size == 0)
		return;

	/* the instructions must reach memory before the I-cache refetches them */
	chunk = PAGE_SIZE - PAGE_OFFSET(va);
	remaining = size;
	for (;;) {
		if (SceCpuForKernel_14E0B0C4(va, &pa) < 0)
			SCE_PANIC_FUNC(124, "sceKernelIcacheInvalidateRange");
		if (chunk > remaining)
			chunk = remaining;
		CpuL2CleanInvalidateRangePA(pa, chunk);
		remaining -= chunk;
		if (remaining == 0)
			break;
		va += chunk;
		chunk = PAGE_SIZE;
	}
	CpuL2CacheSyncCore();
	SceCpuForKernel_2E637B1D(start, size);
}

void sceKernelDcacheInvalidateRangeForL1WBWA(const void *start, SceSize size)
{
	_sceKernelDcacheInvalidateRangeCore(start, size, CACHE_RANGE_L1_ONLY
		CACHE_CALLER("sceKernelDcacheInvalidateRangeForL1WBWA"));
}

void sceKernelDcacheInvalidateRange(const void *start, SceSize size)
{
	_sceKernelDcacheInvalidateRangeCore(start, size, CACHE_RANGE_SKIP_UNCACHED
		CACHE_CALLER("sceKernelDcacheInvalidateRange"));
}

/* name unknown: invalidates a range whose physical address the caller knows */
void CacheInvalidateRangeWithPA(const void *start, uintptr_t pa, SceSize size)
{
	SceSize remaining, chunk;

	if (size == 0)
		return;
	if (CachePleEnabled())
		CachePleFlush();
	chunk = PAGE_SIZE - PAGE_OFFSET((uintptr_t)start);
	remaining = size;
	SceCpuForKernel_4236E06A(start, size);
	for (;;) {
		if (chunk > remaining)
			chunk = remaining;
		CpuL2InvalidateRangePA(pa, chunk);
		pa += chunk;
		remaining -= chunk;
		if (remaining == 0)
			break;
		chunk = PAGE_SIZE;
	}
	CpuL2CacheSyncCore();
	SceCpuForKernel_4236E06A(start, size);
}

void sceKernelDcacheCleanRangeForL1WBWA(const void *start, SceSize size)
{
	_sceKernelDcacheCleanRangeCore(start, size, CACHE_RANGE_L1_ONLY
		CACHE_CALLER("sceKernelDcacheCleanRangeForL1WBWA"));
}

void sceKernelDcacheCleanRange(const void *start, SceSize size)
{
	_sceKernelDcacheCleanRangeCore(start, size, CACHE_RANGE_SKIP_UNCACHED
		CACHE_CALLER("sceKernelDcacheCleanRange"));
}

void SceCpuForKernel_A0A70166(void)
{
	if (CachePleEnabled())
		CachePleFlush();
}

void sceKernelDcacheCleanInvalidateRangeForL1WBWA(const void *start, SceSize size)
{
	_sceKernelDcacheCleanInvalidateRangeCore(start, size, CACHE_RANGE_L1_ONLY
		CACHE_CALLER("sceKernelDcacheCleanInvalidateRangeForL1WBWA"));
}

void sceKernelDcacheCleanInvalidateRange(const void *start, SceSize size)
{
	_sceKernelDcacheCleanInvalidateRangeCore(start, size, CACHE_RANGE_SKIP_UNCACHED
		CACHE_CALLER("sceKernelDcacheCleanInvalidateRange"));
}

/* vmaccess.S: the code whose faults are recovered */
extern const char VmaccessStart[], VmaccessEnd[];

void SceCpuForKernel_C32687D0(uintptr_t *start, uintptr_t *end)
{
	*start = (uintptr_t)VmaccessStart;
	*end = (uintptr_t)VmaccessEnd;
}
