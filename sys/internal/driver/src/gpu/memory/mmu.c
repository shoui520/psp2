/* Native GPU page-table pool and entry-validation prerequisites. */
#include "../../gpu_es4/srvkm/gpu-mmu.h"
#include "../../gpu_es4/srvkm/gpu-blockheap.h"
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <lowio/pervasive.h>
#include <sysmem/sysclib.h>
#include <processmgr/processmgr.h>
#include <processmgr/process.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/memory/mmu.c"

/* Module-local declarations while the shared ABI inputs are frozen. Native
 * callers/callees consume two words for each query; no options argument.
 */
extern int CreateGpuHeap(unsigned base, unsigned size, unsigned lookupKey,
			 unsigned attributes, GpuHeap **output);
extern int DestroyGpuHeap(GpuHeap *heap);
int sceGpuHeapFreeByGpuVA(GpuHeap *heap, unsigned address,
		GpuHeapAllocation *allocation);
int FreeProcessGpuMemory(void *address);

int sceGpuMmuDestroyPageTable(GpuPageTable *table)
{
	GpuHeapAllocation allocation;
	int ret;

	if (!table)
		return (int)0x804c0000;
	SCE_DBG_ASSERT(table->pool != NULL, 329);
	SCE_DBG_ASSERT(table->pool->heap != NULL, 330);
	ret = sceGpuHeapFreeByGpuVA(table->pool->heap,
			(unsigned)table->entries, &allocation);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 334,
				"%s: Failed to free page table at %p with error %08x.\n",
				__func__, table->entries, ret);
		return ret;
	}
	FreeProcessGpuMemory(table);
	return 0;
}

/* ARM register shifts use the low byte and yield zero for shifts >= 32. */
static inline unsigned ShiftGpuPageOffset(unsigned value, unsigned shift)
{
	shift &= 0xff;
	return shift < 32 ? value >> shift : 0;
}

int sceGpuMmuCheckPageTableEntries(GpuPageTable *table, unsigned offset,
				 unsigned size, unsigned mask, unsigned expected)
{
	volatile GpuPageTable *pageTable = table;
	unsigned index, end, entry, shift;

	shift = pageTable->pageShift;
	index = ShiftGpuPageOffset(offset, shift);
	end = index + ShiftGpuPageOffset(size, shift);
	SCE_DBG_ASSERT(end <= pageTable->entryCount, 358);
	for (; index < end; index++) {
		entry = pageTable->entries[index];
		if (expected & 1 & mask) {
			if ((entry & mask) != expected)
				return (int)0x804c0003;
		} else if (entry) {
			return (int)0x804c0003;
		}
	}
	return 0;
}

/* Native private name unavailable. This walks 4-MiB directory regions. */
int CheckGpuMmuRange(GpuMmuContext *context, unsigned address, unsigned size,
		    unsigned mask, unsigned expected)
{
	volatile GpuMmuContext *mmu = context;
	unsigned index, offset, length;
	int ret;

	while (size) {
		index = address >> 22;
		offset = address & 0x3fffff;
		length = 0x400000 - offset;
		if (size <= length)
			length = size;
		if (mmu->pageDirectory[index]) {
			ret = sceGpuMmuCheckPageTableEntries(mmu->pageTables[index],
						 offset, length, mask, expected);
			if (ret < 0)
				return ret;
		} else if (expected & 1 & mask) {
			return (int)0x804c0002;
		}
		address += length;
		size -= length;
	}
	return 0;
}

int sceGpuMmuAllocatePageTablePool(GpuPageTablePool *pool, unsigned pid)
{
	SceKernelProcessInfo process;
	SceKernelBudgetInfo budget;
	SceKernelAllocMemBlockKernelOpt options;
	const char *name;
	unsigned size, type;
	void *base;
	int uid, ret;

	if (!pool)
		return (int)0x804c0000;
	if (pid == 0x10005) {
		size = 0x80000;
		name = "SceGpuPageTablePoolKernel";
	} else {
		memset(&process, 0, sizeof(process));
		process.size = sizeof(process);
		ret = sceKernelGetProcessInfo(pid, &process);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 159,
				"%s: Unable to get information for process %08x with error %08x.\n",
				__func__, pid, ret);
			return ret;
		}
		memset(&budget, 0, sizeof(budget));
		budget.size = sizeof(budget);
		ret = sceKernelGetBudgetInfo(process.budgetId, &budget);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 168,
				"%s: Unable to get budget information for process %08x budget %08x with error %08x.\n",
				__func__, pid, process.budgetId, ret);
			return ret;
		}
		/* Keep the native unsigned wrapping before the 64-MiB rounding. */
		size = ((budget.part[BUDGET_PART_MAIN].psize + 0x3ffffff) & 0xfc000000) >> 10;
		size += ((budget.part[BUDGET_PART_CDRAM].psize + 0x3ffffff) & 0xfc000000) >> 10;
		if (process.type == PROCESS_TYPE_MINI_APP) {
			size += ((budget.part[BUDGET_PART_CDIALOG].psize + 0x3ffffff) & 0xfc000000) >> 10;
			size += 0x60000;
			name = "SceGpuPageTablePoolMiniApp";
		} else {
			size += 0xa0000;
			name = "SceGpuPageTablePoolApp";
		}
	}
	SCE_DBG_ASSERT(1, 213);
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	if (pid == 0x10005) {
		type = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW;
	} else {
		options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID;
		options.pid = pid;
		type = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_UMAIN_NC_RW;
	}
	uid = SceSysmemForDriver_C94850C9(name, type, size, &options);
	if (uid < 0)
		return uid;
	ret = SceSysmemForDriver_A841EDDA(uid, &base);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 231,
			"%s: Failed to get address for memblock %08x with error %08x.\n",
			__func__, uid, ret);
		SceSysmemForDriver_009E1C61(uid);
		return ret;
	}
	ret = CreateGpuHeap((unsigned)base, size, 0, 0, &pool->heap);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 238, "%s: Failed to create heap with error %08x\n",
			       __func__, ret);
		SceSysmemForDriver_009E1C61(uid);
		return ret;
	}
	pool->uid = uid;
	return 0;
}

int FreeGpuPageTablePool(GpuPageTablePool *pool)
{
	volatile GpuPageTablePool *pageTablePool = pool;
	GpuHeap *heap;
	int uid;

	if (!pool)
		return (int)0x804c0000;
	heap = pageTablePool->heap;
	if (heap) {
		DestroyGpuHeap(heap);
		pageTablePool->heap = 0;
	}
	uid = pageTablePool->uid;
	if (uid != -1) {
		SceSysmemForDriver_009E1C61(uid);
		pageTablePool->uid = -1;
	}
	return 0;
}

/* SCE: choose a larger data-page size only when each physical discontinuity
 * is aligned to that size. Older SoC revisions retain the caller's defaults.
 */
int sceGpuMmuSelectPageTableSize(unsigned address, unsigned *directoryFlags,
		unsigned *pageShift, unsigned *entryCount, unsigned *tableSize)
{
	uintptr_t previous, physical;
	unsigned revision, cursor;
	unsigned can4M, can1M, can256K, can64K;
	int ret;

	revision = ScePervasiveForDriver_714EEFB7();
	if (!(revision & 0x1ff00) && (unsigned char)revision <= 0x32)
		return 0;
	ret = SceSysmemForDriver_8D160E65((void *)address, &previous);
	if (ret < 0)
		return ret;
	if (!(previous & 0x3fffff)) {
		can4M = can1M = can256K = can64K = 1;
	} else if (!(previous & 0xfffff)) {
		can4M = 0;
		can1M = can256K = can64K = 1;
	} else if (!(previous & 0x3ffff)) {
		can4M = can1M = 0;
		can256K = can64K = 1;
	} else if (!(previous & 0xffff)) {
		can4M = can1M = can256K = 0;
		can64K = 1;
	} else {
		if (previous & 0x3fff)
			return 0;
		can4M = can1M = can256K = can64K = 0;
	}
	for (cursor = address + 0x1000; cursor < address + 0x400000;
	     cursor += 0x1000) {
		ret = SceSysmemForDriver_8D160E65((void *)cursor, &physical);
		if (ret < 0)
			return ret;
		if (physical != previous + 0x1000) {
			can4M = (unsigned short)physical;
			if (physical & 0xfffff)
				can1M = 0;
			if (physical & 0x3ffff)
				can256K = 0;
			if (can4M) {
				can64K = physical & 0x3fff;
				if (can64K)
					return 0;
				can4M = 0;
			}
		}
		previous = physical;
	}
	if (can4M) {
		*pageShift = 22;
		*entryCount = 1;
		*directoryFlags = 11;
		*tableSize = 16;
	} else if (can1M) {
		*pageShift = 20;
		*entryCount = 4;
		*directoryFlags = 9;
		*tableSize = 16;
	} else if (can256K) {
		*pageShift = 18;
		*entryCount = 16;
		*directoryFlags = 7;
		*tableSize = 64;
	} else if (can64K) {
		*pageShift = 16;
		*entryCount = 64;
		*directoryFlags = 5;
		*tableSize = 256;
	} else {
		*pageShift = 14;
		*entryCount = 256;
		*directoryFlags = 3;
		*tableSize = 1024;
	}
	return 0;
}

void *AllocProcessGpuMemory(unsigned size);
int sceGpuHeapAlloc(GpuHeap *heap, unsigned size, unsigned alignment,
		unsigned allocationType, unsigned flags, unsigned blockGroupId,
		GpuHeapAllocation *output, unsigned boundary);
void RequestGpuSystemCacheInvalidate(void);
void RequestGpuPageTableCacheInvalidate(void);
void RequestGpuPageDirectoryCacheInvalidate(void);

/* SCE native mapping path. Sony inlines sceGpuMmuCreatePageTable and
 * sceGpuMmuSetPageTableEntries here; the debug records retain their names.
 */
int ksceGpuMmuMapMemory(GpuMmuContext *context, unsigned gpuAddress,
		unsigned backingAddress, unsigned size, unsigned permissions)
{
	GpuHeapAllocation allocation, released;
	GpuPageTable *table;
	unsigned directoryChanged = 0;
	unsigned directoryFlags, pageShift, entryCount, tableSize;
	unsigned directoryIndex, offset, length, start, end, index;
	unsigned entriesChanged, physical, entryFlags;
	int ret;

	if (!context || ((gpuAddress | backingAddress) & 0xfff) ||
	    (size & 0xfff))
		return (int)0x804c0000;
	ret = CheckGpuMmuRange(context, gpuAddress, size, 1, 0);
	if (ret < 0)
		return ret;
	entryFlags = permissions & 0x7fffffff;
	while (size) {
		offset = gpuAddress & 0x3fffff;
		length = 0x400000 - offset;
		directoryIndex = gpuAddress >> 22;
		if (length >= size)
			length = size;
		if (!context->pageDirectory[directoryIndex]) {
			tableSize = 4096;
			directoryFlags = 1;
			pageShift = 12;
			entryCount = 1024;
#if FW_DEBUG
			SceDebugForDriver_35A35322(context->pageTables[directoryIndex] == NULL,
					SCE_DBGINFO_FUNC(714, "sceGpuMmuMapMemory"),
					__builtin_return_address(0));
#endif
			if (length == 0x400000) {
				ret = sceGpuMmuSelectPageTableSize(backingAddress,
						&directoryFlags, &pageShift,
						&entryCount, &tableSize);
				if (ret < 0)
					return ret;
			}
#if FW_DEBUG
			SceDebugForDriver_35A35322(&context->pageTablePool != NULL,
					SCE_DBGINFO_FUNC(271, "sceGpuMmuCreatePageTable"),
					__builtin_return_address(0));
			SceDebugForDriver_35A35322(context->pageTablePool.heap != NULL,
					SCE_DBGINFO_FUNC(272, "sceGpuMmuCreatePageTable"),
					__builtin_return_address(0));
#endif
			if (!tableSize)
				return (int)0x804c0000;
			table = AllocProcessGpuMemory(sizeof(*table));
			if (!table)
				return (int)0x804c0001;
			memset(table, 0, sizeof(*table));
			ret = sceGpuHeapAlloc(context->pageTablePool.heap,
					tableSize, tableSize, 7, 0x200000, 0,
					&allocation, 0);
			if (ret < 0) {
#if FW_DEBUG
				SceDebugForDriver_D9703808(1, 0,
					SCE_DBGINFO_FUNC(285, "sceGpuMmuCreatePageTable"),
					"%s: Failed heap allocate a page table of size %08x with error %08x\n",
					"sceGpuMmuCreatePageTable", tableSize, ret);
#endif
				FreeProcessGpuMemory(table);
				return ret;
			}
			table->pool = &context->pageTablePool;
			table->entries = (unsigned *)allocation.gpuAddress;
			ret = SceSysmemForDriver_8D160E65(table->entries,
					&table->physicalAddress);
			if (ret < 0) {
#if FW_DEBUG
				SceDebugForDriver_D9703808(1, 0,
					SCE_DBGINFO_FUNC(297, "sceGpuMmuCreatePageTable"),
					"%s: Failed to get physical address for %p with error %08x.\n",
					"sceGpuMmuCreatePageTable", table->entries, ret);
#endif
				sceGpuHeapFreeByGpuVA(context->pageTablePool.heap,
						(unsigned)table->entries, &released);
				FreeProcessGpuMemory(table);
				return ret;
			}
			memset(table->entries, 0, tableSize);
			table->pageShift = pageShift;
			table->entryCount = entryCount;
			context->pageDirectory[directoryIndex] =
					table->physicalAddress | directoryFlags;
			context->pageTables[directoryIndex] = table;
			directoryChanged = 1;
		}
		table = context->pageTables[directoryIndex];
		if ((length | offset) & ((1U << table->pageShift) - 1))
			return (int)0x804c0000;
		start = offset >> table->pageShift;
		end = start + (length >> table->pageShift);
#if FW_DEBUG
		SceDebugForDriver_35A35322(end <= table->entryCount,
				SCE_DBGINFO_FUNC(442, "sceGpuMmuSetPageTableEntries"),
				__builtin_return_address(0));
#endif
		entriesChanged = 0;
		ret = 0;
		for (index = start; index < end; index++) {
#if FW_DEBUG
			SceDebugForDriver_35A35322(table->entries[index] == 0,
					SCE_DBGINFO_FUNC(456, "sceGpuMmuSetPageTableEntries"),
					__builtin_return_address(0));
#endif
			if (permissions & 0x80000000) {
				physical = backingAddress;
			} else {
				ret = SceSysmemForDriver_8D160E65((void *)backingAddress, &physical);
				if (ret < 0) {
					unsigned rollback;

					for (rollback = index - 1; rollback >= start;
					     rollback--)
						table->entries[rollback] = 0;
					break;
				}
			}
			table->entries[index] = physical | 1 | entryFlags;
			entriesChanged = 1;
			backingAddress += 1U << table->pageShift;
		}
		if (entriesChanged)
			RequestGpuPageTableCacheInvalidate();
		if (ret < 0)
			return ret;
		size -= length;
		gpuAddress += length;
	}
	RequestGpuSystemCacheInvalidate();
	if (directoryChanged)
		RequestGpuPageDirectoryCacheInvalidate();
	return 0;
}

int ksceGpuMmuUnmapMemory(GpuMmuContext *context, unsigned address,
		unsigned size)
{
	GpuPageTable *table;
	unsigned directoryChanged = 0;
	unsigned directoryIndex, offset, length, start, end, index;
	unsigned entriesChanged;
	int ret, clearResult;

	if (!context || (address & 0xfff) || (size & 0xfff))
		return (int)0x804c0000;
	ret = CheckGpuMmuRange(context, address, size, 1, 1);
	if (ret < 0)
		return ret;
	while (size) {
		directoryIndex = address >> 22;
		offset = address & 0x3fffff;
		length = 0x400000 - offset;
		table = context->pageTables[directoryIndex];
		SCE_DBG_ASSERT_FUNC(table != NULL, 824, "sceGpuMmuUnmapMemory");
		if (length >= size)
			length = size;
		SCE_DBG_ASSERT_FUNC(context->pageDirectory[directoryIndex] != 0, 830,
				    "sceGpuMmuUnmapMemory");
		if ((length | offset) & ((1U << table->pageShift) - 1)) {
			clearResult = (int)0x804c0000;
		} else {
			start = offset >> table->pageShift;
			end = start + (length >> table->pageShift);
#if FW_DEBUG
			SceDebugForDriver_35A35322(end <= table->entryCount,
					SCE_DBGINFO_FUNC(442, "sceGpuMmuSetPageTableEntries"),
					__builtin_return_address(0));
#endif
			entriesChanged = 0;
			for (index = start; index < end; index++) {
#if FW_DEBUG
				SceDebugForDriver_35A35322(table->entries[index] != 0,
					SCE_DBGINFO_FUNC(466, "sceGpuMmuSetPageTableEntries"),
					__builtin_return_address(0));
#endif
				table->entries[index] = 0;
				entriesChanged = 1;
			}
			if (entriesChanged)
				RequestGpuPageTableCacheInvalidate();
			clearResult = 0;
		}
		SCE_DBG_ASSERT_FUNC(clearResult == 0, 837, "sceGpuMmuUnmapMemory");
		if (!sceGpuMmuCheckPageTableEntries(table, 0, 0x400000, 1, 0)) {
			sceGpuMmuDestroyPageTable(table);
			directoryChanged = 1;
			context->pageTables[directoryIndex] = NULL;
			context->pageDirectory[directoryIndex] = 0;
		}
		address += length;
		size -= length;
	}
	if (directoryChanged)
		RequestGpuPageDirectoryCacheInvalidate();
	RequestGpuSystemCacheInvalidate();
	return 0;
}

int FreeGpuPageDirectory(GpuMmuContext *context);

int DestroyGpuMmuContext(GpuMmuContext *context)
{
	unsigned index;

	for (index = 0; index != 1024; index++) {
		if (context->pageTables[index]) {
			sceGpuMmuDestroyPageTable(context->pageTables[index]);
			context->pageTables[index] = NULL;
		}
	}
	FreeGpuPageDirectory(context);
	FreeGpuPageTablePool(&context->pageTablePool);
	return 0;
}

int sceGpuMmuShareRange(GpuMmuContext *destination, GpuMmuContext *source,
		unsigned address, unsigned size)
{
	unsigned index, count, sourceEntry, destinationEntry;
	unsigned directoryChanged = 0, mappingAdded = 0;

	if (destination->pageDirectory) {
		index = address >> 22;
		count = (((address + size + 0x3fffff) & ~0x3fffffU) -
			 (address & ~0x3fffffU)) >> 22;
		while (count) {
			SCE_DBG_ASSERT(destination->pageTables[index] == NULL, 935);
			sourceEntry = source->pageDirectory[index];
			destinationEntry = destination->pageDirectory[index];
			if (sourceEntry) {
				if (destinationEntry) {
					SCE_DBG_ASSERT(destinationEntry == sourceEntry, 961);
				} else {
					directoryChanged = 1;
					destination->pageDirectory[index] = sourceEntry;
					mappingAdded = 1;
				}
			} else if (destinationEntry) {
				destination->pageDirectory[index] = 0;
				directoryChanged = 1;
			}
			count--;
			index++;
		}
		if (directoryChanged)
			RequestGpuPageDirectoryCacheInvalidate();
		if (mappingAdded)
			RequestGpuSystemCacheInvalidate();
	}
	return 0;
}

int sceGpuMmuInit(void)
{
	return 0;
}

int CheckGpuMmuMapping(GpuMmuContext *context, unsigned address,
		unsigned size, unsigned flags)
{
	if (!context || (size & 0xfff))
		return (int)0x804c0000;
	return CheckGpuMmuRange(context, address, size, 7, flags | 1);
}
