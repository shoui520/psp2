/*
 * Process memory segments: the full dump (every block of the user partitions) and
 * the mini dump (the unwind tables, the thread stacks and the pages around what the
 * aborted thread's registers and stack point at), one PT_LOAD program header each.
 */

#include <string.h>
#include <modulemgr/elf.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <modulemgr/modulemgr_debugger.h>
#include "writer_internal.h"
#include "seg_internal.h"

#define PROC_MEM_MAX_STACK_THREADS	500
#define PROC_MEM_MAX_ADDRESS_REGIONS	16

/* dumpLevel bits */
#define PROC_MEM_LEVEL_STACK		0x1
#define PROC_MEM_LEVEL_ARM_EX		0x2
#define PROC_MEM_LEVEL_MEM_BLOCKS	0x1ef0
#define PROC_MEM_LEVEL_FULL		7920

typedef struct ProcMemRegion {
	uintptr_t base;
	SceSize size;
} ProcMemRegion;

/* the aborted thread and the pages its registers and stack words point into */
typedef struct MemDumpContext {
	int hasAbortedThread;
	CoredumpThreadRegInfo registers;
	uintptr_t stackBase;
	SceSize stackSize;
	ProcMemRegion regions[PROC_MEM_MAX_ADDRESS_REGIONS];
	SceUInt32 numRegions;
	SceUInt32 maxRegions;
	SceSize regionSize;		/* pages kept around each address */
	SceUInt32 numMergedRegions;
	SceSize mergedDataSize;
} MemDumpContext;

_Static_assert(sizeof(MemDumpContext) == 0x218, "memory dump context size");

typedef struct ProcMemSegment {
	SegIf segment;
	ProcMemRegion *armExRegions;
	SceUInt32 numArmExRegions;
	SceSize armExDataSize;
	ProcMemRegion *stackRegions;
	SceUInt32 numStackRegions;
	SceSize stackDataSize;
	SceUInt32 numMemBlocks;
	SceUInt32 fileOffset;		/* of the next PT_LOAD data, while writing them */
	SceSize memBlockDataSize;
	SceSize writtenPhdrDataSize;
	SceSize writtenMemBlockSize;
	SceUInt32 dumpLevel;
	Elf32_Word phdrType;		/* PT_LOAD, or PT_NULL for the mini dump of a full run */
	MemDumpContext *context;
} ProcMemSegment;

_Static_assert(sizeof(ProcMemSegment) == 76, "process memory segment size");

/* _writeMemBlockSegmentCallback's context */
typedef struct ProcMemWriteContext {
	ProcMemSegment *segment;
	void *buffer;
	SceSize bufferSize;
} ProcMemWriteContext;

extern Writer *currentWriter;

static int _initAsFull(SegIf *segment);
static int _initAsMini(SegIf *segment);
static int _calcSize(SegIf *segment);
static int _writePhdr(SegIf *segment, SceOff fileOffset);
static int _writeSegs(SegIf *segment);

const SegIfMethod procMemFullMethods = {
	_initAsFull,
	_calcSize,
	_writePhdr,
	_writeSegs,
};

const SegIfMethod procMemMiniMethods = {
	_initAsMini,
	_calcSize,
	_writePhdr,
	_writeSegs,
};

static MemDumpContext memDumpContext;
ProcMemSegment procMemFullSegment;
ProcMemSegment procMemMiniSegment;

/* the segment being sized or written, for the callbacks */
static ProcMemSegment *currentSegment;
static ProcMemRegion armExRegions[512];
static ProcMemRegion stackRegions[256];

static int _writeRegion(uintptr_t address, SceSize size, void *buffer,
	SceSize bufferSize);
static int _calcSizeOfMemBlockCallback(CoredumpMemBlockInfo *info, void *context);
static int _writeMemBlockSegmentCallback(CoredumpMemBlockInfo *info,
	void *context);
static int _writeMemBlockPhdrCallback(CoredumpMemBlockInfo *info, void *context);
static int _getArmExInfoCallback(CoredumpModuleInfo *info,
	SceKernelSegmentInfoForDebugger *segments, unsigned int nSegments,
	SceKernelModuleExidxInfo *exidx, void *context);
static int _getMemBlockFromAddressOnUserRegisterOrStack(ProcMemSegment *segment);

static int _writeSegs(SegIf *base)
{
	ProcMemSegment *segment = (ProcMemSegment *)base;
	ProcMemWriteContext context;
	MemDumpContext *dump;
	MemBlockInfoEx *blocks;
	SceSize blocksSize;
	void *buffer;
	SceUInt32 i;
	int result;

	SCE_DBG_ASSERT_FUNC(base != NULL, 428, "_writeSegs");
	if (base->numPhdr == 0)
		return 0;
	currentSegment = segment;
	if (segment->dumpLevel == PROC_MEM_LEVEL_FULL)
		segment->phdrType = PT_LOAD;
	else if (coredumpRun.trigger.dumpLevel == PROC_MEM_LEVEL_FULL)
		segment->phdrType = PT_NULL;
	else
		segment->phdrType = PT_LOAD;
	base->writtenSize = 0;
	buffer = memMgrLocalAlloc(4096);
	if (buffer == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	for (i = 0; i < segment->numArmExRegions; i++) {
		result = _writeRegion(segment->armExRegions[i].base,
			segment->armExRegions[i].size, buffer, 4096);
		if (result != 0)
			goto free;
	}
	for (i = 0; i < segment->numStackRegions; i++) {
		result = _writeRegion(segment->stackRegions[i].base,
			segment->stackRegions[i].size, buffer, 4096);
		if (result != 0)
			goto free;
	}
	if (segment->numMemBlocks == 0) {
		memMgrLocalFree(buffer);
		return 0;
	}
	if ((segment->dumpLevel & PROC_MEM_LEVEL_MEM_BLOCKS) == 0) {
		dump = segment->context;
		SCE_DBG_ASSERT_FUNC(dump != NULL, 499, "_writeSegs");
		for (i = 0; i < dump->maxRegions; i++) {
			if (dump->regions[i].base == 0 || dump->regions[i].size == 0)
				continue;
			result = _writeRegion(dump->regions[i].base,
				dump->regions[i].size, buffer, 4096);
			if (result != 0)
				goto free;
		}
		memMgrLocalFree(buffer);
		return 0;
	}
	blocks = memMgrLocalAllAlloc(&blocksSize);
	if (blocks == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto free;
	}
	context.segment = segment;
	context.buffer = buffer;
	context.bufferSize = 4096;
	result = oalEachOfAllMemBlockInfo(coredumpRun.pid,
		_writeMemBlockSegmentCallback, &context, blocks, blocksSize);
	memMgrLocalFree(blocks);
free:
	memMgrLocalFree(buffer);
	return result;
}

static int _calcSize(SegIf *base)
{
	ProcMemSegment *segment = (ProcMemSegment *)base;
	SceKernelModuleInfoForDebugger *modules;
	MemBlockInfoEx *blocks;
	SceUInt32 level;
	SceUInt32 numThreads, i, k;
	SceSize size;
	SceUID *ids;
	int result;

	SCE_DBG_ASSERT_FUNC(base != NULL, 224, "_calcSize");
	level = segment->dumpLevel;
	currentSegment = segment;
	base->writtenSize = 0;
	if (level == PROC_MEM_LEVEL_FULL) {
		segment->phdrType = 1;
		if (coredumpRun.trigger.dumpLevel != PROC_MEM_LEVEL_FULL) {
			base->numPhdr = 0;
			base->dataSize = 0;
			return 0;
		}
	} else {
		segment->phdrType = coredumpRun.trigger.dumpLevel != PROC_MEM_LEVEL_FULL;
		if (level & PROC_MEM_LEVEL_ARM_EX) {
			/* _getArmExInfo inlined */
			SCE_DBG_ASSERT_FUNC(base != NULL, 530, "_getArmExInfo");
			modules = memMgrLocalAllAlloc(&size);
			if (modules == NULL)
				return SCE_ERROR_ERRNO_ENOMEM;
			result = oalEachModuleInfo(coredumpRun.pid,
				_getArmExInfoCallback, modules, size, segment);
			memMgrLocalFree(modules);
			if (result != 0)
				return result;
		}
		if (level & PROC_MEM_LEVEL_STACK) {
			/* _getStackInfo inlined */
			SCE_DBG_ASSERT_FUNC(base != NULL, 653, "_getStackInfo");
			numThreads = oalGetNumOfThreads(coredumpRun.pid);
			if (numThreads >= PROC_MEM_MAX_STACK_THREADS)
				numThreads = PROC_MEM_MAX_STACK_THREADS;
			ids = memMgrLocalAlloc(numThreads * sizeof(*ids));
			if (ids == NULL)
				return SCE_ERROR_ERRNO_ENOMEM;
			oalGetThreadIdList(coredumpRun.pid, ids, numThreads);
			for (i = 0, k = 0; i < numThreads; i++) {
				oalGetThreadTlsStack(coredumpRun.pid, ids[i],
					&segment->stackRegions[k].base,
					&segment->stackRegions[k].size);
				segment->numStackRegions++;
				segment->stackDataSize += segment->stackRegions[k].size;
				k++;
				if (oalProbeThreadInfo(coredumpRun.pid, ids[i]) != 0)
					continue;
				oalGetThreadStack(coredumpRun.pid, ids[i],
					&segment->stackRegions[k].base,
					&segment->stackRegions[k].size);
				segment->numStackRegions++;
				segment->stackDataSize += segment->stackRegions[k].size;
				k++;
			}
			memMgrLocalFree(ids);
		}
	}
	if (level & PROC_MEM_LEVEL_MEM_BLOCKS) {
		blocks = memMgrLocalAllAlloc(&size);
		if (blocks == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		result = oalEachOfAllMemBlockInfo(coredumpRun.pid,
			_calcSizeOfMemBlockCallback, segment, blocks, size);
		memMgrLocalFree(blocks);
		if (result != 0)
			return result;
	} else {
		result = _getMemBlockFromAddressOnUserRegisterOrStack(segment);
		if (result != 0)
			return result;
	}
	base->numPhdr = segment->numMemBlocks + segment->numStackRegions +
		segment->numArmExRegions;
	base->dataSize = segment->memBlockDataSize + segment->armExDataSize +
		segment->stackDataSize;
	return 0;
}

static int _writePhdr(SegIf *base, SceOff fileOffset)
{
	ProcMemSegment *segment = (ProcMemSegment *)base;
	MemDumpContext *dump;
	MemBlockInfoEx *blocks;
	ProcMemRegion *region;
	Elf32_Phdr phdr;
	SceSize size;
	SceUInt32 i;
	int result;

	SCE_DBG_ASSERT_FUNC(base != NULL && currentSegment != NULL, 321,
		"_writePhdr");
	if (base->numPhdr == 0)
		return 0;
	segment->fileOffset = fileOffset;
	currentSegment = segment;
	if (segment->dumpLevel == PROC_MEM_LEVEL_FULL)
		segment->phdrType = PT_LOAD;
	else if (coredumpRun.trigger.dumpLevel == PROC_MEM_LEVEL_FULL)
		segment->phdrType = PT_NULL;
	else
		segment->phdrType = PT_LOAD;
	for (i = 0; i < segment->numArmExRegions; i++) {
		region = &segment->armExRegions[i];
		utilsElf32MakePhent(&phdr, currentSegment->phdrType,
			segment->fileOffset, region->base, region->base,
			region->size, region->size, PF_R | PF_W | PF_X, 4);
		result = writerWrite(currentWriter, &phdr, sizeof(phdr));
		if (result != 0)
			return result;
		segment->fileOffset += region->size;
	}
	for (i = 0; i < segment->numStackRegions; i++) {
		region = &segment->stackRegions[i];
		utilsElf32MakePhent(&phdr, currentSegment->phdrType,
			segment->fileOffset, region->base, region->base,
			region->size, region->size, PF_R | PF_W | PF_X, 4);
		result = writerWrite(currentWriter, &phdr, sizeof(phdr));
		if (result != 0)
			return result;
		segment->fileOffset += region->size;
	}
	if (segment->numMemBlocks == 0)
		return 0;
	if (segment->dumpLevel & PROC_MEM_LEVEL_MEM_BLOCKS) {
		blocks = memMgrLocalAllAlloc(&size);
		if (blocks == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		result = oalEachOfAllMemBlockInfo(coredumpRun.pid,
			_writeMemBlockPhdrCallback, segment, blocks, size);
		memMgrLocalFree(blocks);
		return result;
	}
	dump = segment->context;
	SCE_DBG_ASSERT_FUNC(dump != NULL, 394, "_writePhdr");
	for (i = 0; i < dump->maxRegions; i++) {
		if (dump->regions[i].base == 0 || dump->regions[i].size == 0)
			continue;
		utilsElf32MakePhent(&phdr, currentSegment->phdrType,
			segment->fileOffset, dump->regions[i].base,
			dump->regions[i].base, dump->regions[i].size,
			dump->regions[i].size, PF_R | PF_W | PF_X, 4);
		result = writerWrite(currentWriter, &phdr, sizeof(phdr));
		if (result != 0)
			return result;
		segment->fileOffset += dump->regions[i].size;
	}
	return 0;
}

/* writes [address, address + size) through buffer; inaccessible pages as zeros */
/*
 * Sony's debug build keeps it out of line (4 callers here and the memblock
 * callback); the release builds, without the assert, inline it everywhere.
 */
#if FW_DEBUG
static __attribute__((noinline)) int _writeRegion(uintptr_t address,
#else
static int _writeRegion(uintptr_t address,
#endif
	SceSize size, void *buffer, SceSize bufferSize)
{
	SceSize length;
	int result;

	SCE_DBG_ASSERT_FUNC(buffer != NULL, 1120, "_writeRegion");
	while (size != 0) {
		length = size >= bufferSize ? bufferSize : size;
		if (oalIsAccessibleRange(coredumpRun.pid, (const void *)address,
			length) != 0) {
			oalCopyFromUserSpace(coredumpRun.pid, buffer,
				(const void *)address, length);
			result = writerWrite(currentWriter, buffer, length);
		} else {
			result = writerWritePaddingData(currentWriter, length);
		}
		if (result != 0)
			return result;
		size -= length;
		address += length;
		currentSegment->segment.writtenSize += length;
	}
	return 0;
}

static int _calcSizeOfMemBlockCallback(CoredumpMemBlockInfo *info, void *context)
{
	ProcMemSegment *segment = context;

	SCE_DBG_ASSERT_FUNC(info != NULL && segment != NULL &&
		currentSegment != NULL, 1035, "_calcSizeOfMemBlockCallback");
	if ((currentSegment->dumpLevel & oalGetPartitionOfMemBlock(info)) != 0) {
		segment->memBlockDataSize += info->size - info->headFree -
			info->tailFree;
		segment->numMemBlocks++;
	}
	return 0;
}

static int _writeMemBlockSegmentCallback(CoredumpMemBlockInfo *info,
	void *context)
{
	ProcMemWriteContext *write = context;
	SceSize size;
	int result;

	SCE_DBG_ASSERT_FUNC(info != NULL && write != NULL &&
		currentSegment != NULL, 1091, "_writeMemBlockSegmentCallback");
	result = currentSegment->dumpLevel & oalGetPartitionOfMemBlock(info);
	if (result == 0)
		return result;
	size = info->size - (info->headFree + info->tailFree);
	result = _writeRegion(info->vbase + info->headFree, size, write->buffer,
		write->bufferSize);
	if (result == 0)
		write->segment->writtenMemBlockSize += size;
	return result;
}

static int _writeMemBlockPhdrCallback(CoredumpMemBlockInfo *info, void *context)
{
	ProcMemSegment *segment = context;
	Elf32_Phdr phdr;
	SceSize size;
	int result;

	SCE_DBG_ASSERT_FUNC(info != NULL && segment != NULL &&
		currentSegment != NULL, 1057, "_writeMemBlockPhdrCallback");
	result = currentSegment->dumpLevel & oalGetPartitionOfMemBlock(info);
	if (result == 0)
		return result;
	size = info->size - (info->tailFree + info->headFree);
	utilsElf32MakePhent(&phdr, currentSegment->phdrType, segment->fileOffset,
		info->vbase + info->headFree, info->vbase + info->headFree, size,
		size, PF_R | PF_W | PF_X, 4);
	result = writerWrite(currentWriter, &phdr, sizeof(phdr));
	if (result == 0) {
		segment->fileOffset += size;
		segment->writtenPhdrDataSize += size;
	}
	return result;
}

static int _init(SegIf *base)
{
	ProcMemSegment *segment = (ProcMemSegment *)base;
	int result;

	SCE_DBG_ASSERT_FUNC(base != NULL, 164, "_init");
	result = segInitBase(base);
	if (result != 0)
		return result;
	segment->armExRegions = armExRegions;
	memset(armExRegions, 0, sizeof(armExRegions));
	segment->numArmExRegions = 0;
	segment->armExDataSize = 0;
	segment->stackRegions = stackRegions;
	memset(stackRegions, 0, sizeof(stackRegions));
	segment->numStackRegions = 0;
	segment->stackDataSize = 0;
	segment->numMemBlocks = 0;
	segment->fileOffset = 0;
	segment->memBlockDataSize = 0;
	segment->writtenPhdrDataSize = 0;
	segment->writtenMemBlockSize = 0;
	segment->context = &memDumpContext;
	return 0;
}

static int _initAsFull(SegIf *base)
{
	ProcMemSegment *segment = (ProcMemSegment *)base;

	SCE_DBG_ASSERT_FUNC(base != NULL, 191, "_initAsFull");
	_init(base);
	segment->dumpLevel = PROC_MEM_LEVEL_FULL;
	return 0;
}

static void _sInitMemDumpContext(MemDumpContext *context)
{
	CoredumpThreadStopEvent stopEvent;
	SceUID threadId;
	int i;

	SCE_DBG_ASSERT_FUNC(context != NULL, 822, "_sInitMemDumpContext");
	stopEvent.threadId = coredumpRun.trigger.crash_thid;
	stopEvent.reason = coredumpRun.trigger.triggerReason;
	threadId = oalGetAbortedThreadID(coredumpRun.pid, &stopEvent);
	if (threadId < 0) {
		context->hasAbortedThread = 0;
		return;
	}
	oalGetThreadRegInfo(coredumpRun.pid, threadId, &context->registers);
	oalGetThreadStack(coredumpRun.pid, threadId, &context->stackBase,
		&context->stackSize);
	context->hasAbortedThread = 1;
	for (i = 0; i < PROC_MEM_MAX_ADDRESS_REGIONS; i++) {
		context->regions[i].base = 0;
		context->regions[i].size = 0;
	}
	context->numRegions = 0;
	context->maxRegions = PROC_MEM_MAX_ADDRESS_REGIONS;
	context->regionSize = 0x2000;
}

static int _initAsMini(SegIf *base)
{
	ProcMemSegment *segment = (ProcMemSegment *)base;

	SCE_DBG_ASSERT_FUNC(base != NULL, 206, "_initAsMini");
	_init(base);
	_sInitMemDumpContext(&memDumpContext);
	segment->dumpLevel = 15;
	return 0;
}

static void _recordDumpRegion(ProcMemSegment *segment, uintptr_t base,
	SceSize size)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 580, "_recordDumpRegion");
	segment->armExRegions[segment->numArmExRegions].base = base;
	segment->armExRegions[segment->numArmExRegions].size = size;
	segment->numArmExRegions++;
	segment->armExDataSize += size;
}

/* the pages of the module's .ARM.exidx and .ARM.extab, merged when they meet */
static void _recordArmExInfo(SceKernelModuleExidxInfo *exidx,
	ProcMemSegment *segment)
{
	uintptr_t exidxFirst = 0, exidxLast = 0, extabFirst = 0, extabLast = 0;
	uintptr_t first, last;
	int hasExidx = 0, hasExtab = 0;

	SCE_DBG_ASSERT_FUNC(exidx != NULL && segment != NULL, 593,
		"_recordArmExInfo");
	if (exidx->exidxTop != NULL && exidx->exidxTop < exidx->exidxBtm) {
		exidxFirst = (uintptr_t)exidx->exidxTop >> 12;
		exidxLast = ((uintptr_t)exidx->exidxBtm - 1) >> 12;
		hasExidx = 1;
	}
	if (exidx->extabStart != NULL && exidx->extabStart < exidx->extabEnd) {
		extabFirst = (uintptr_t)exidx->extabStart >> 12;
		extabLast = ((uintptr_t)exidx->extabEnd - 1) >> 12;
		hasExtab = 1;
	}
	if (hasExidx && hasExtab) {
		if (extabFirst > exidxLast || extabLast < exidxFirst) {
			_recordDumpRegion(segment, exidxFirst << 12,
				((exidxLast + 1) << 12) - (exidxFirst << 12));
			_recordDumpRegion(segment, extabFirst << 12,
				((extabLast + 1) << 12) - (extabFirst << 12));
		} else {
			first = exidxFirst < extabFirst ? exidxFirst : extabFirst;
			last = exidxLast > extabLast ? exidxLast : extabLast;
			_recordDumpRegion(segment, first << 12,
				((last + 1) << 12) - (first << 12));
		}
	} else if (hasExidx) {
		_recordDumpRegion(segment, exidxFirst << 12,
			((exidxLast + 1) << 12) - (exidxFirst << 12));
	} else if (hasExtab) {
		_recordDumpRegion(segment, extabFirst << 12,
			((extabLast + 1) << 12) - (extabFirst << 12));
	}
}

static int _getArmExInfoCallback(CoredumpModuleInfo *info,
	SceKernelSegmentInfoForDebugger *segments, unsigned int nSegments,
	SceKernelModuleExidxInfo *exidx, void *context)
{
	SCE_DBG_ASSERT_FUNC(exidx != NULL && context != NULL, 570,
		"_getArmExInfoCallback");
	_recordArmExInfo(exidx, context);
	return 0;
}

static void _sPushMemBlockInfo(ProcMemSegment *segment, uintptr_t base,
	SceSize size)
{
	MemDumpContext *context;
	SceUInt32 i;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 887, "_sPushMemBlockInfo");
	context = segment->context;
	SCE_DBG_ASSERT_FUNC(context != NULL, 890, "_sPushMemBlockInfo");
	base &= ~0xfff;
	if (context->numRegions >= context->maxRegions)
		return;
	for (i = 0; i < context->numRegions; i++) {
		if (context->regions[i].base == base)
			return;
	}
	context->regions[context->numRegions].base = base;
	context->regions[context->numRegions].size = size & ~0xfff;
	context->numRegions++;
}

/* keep regionSize bytes of the block around address, when it is in the block */
static void _sPushIfInRange(ProcMemSegment *segment, uintptr_t address,
	uintptr_t blockBase, SceSize blockSize)
{
	MemDumpContext *context;
	uintptr_t start, end;
	SceSize size;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 865, "_sPushIfInRange");
	context = segment->context;
	SCE_DBG_ASSERT_FUNC(context != NULL, 868, "_sPushIfInRange");
	if (address < blockBase || address > blockBase + blockSize - 1)
		return;
	end = blockBase + blockSize;
	size = context->regionSize;
	start = ((address + 0x800) & ~0xfff) - size / 2;
	if (start < blockBase)
		start = blockBase;
	if (end <= start + size)
		size = end - start;
	_sPushMemBlockInfo(segment, start, size);
}

static void _sSortMemBlockInfo(ProcMemSegment *segment)
{
	MemDumpContext *context;
	ProcMemRegion swap;
	SceUInt32 i, j;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 915, "_sSortMemBlockInfo");
	context = segment->context;
	SCE_DBG_ASSERT_FUNC(context != NULL, 918, "_sSortMemBlockInfo");
	for (i = 0; i < context->numRegions; i++) {
		for (j = i; j < context->numRegions; j++) {
			if (context->regions[i].base > context->regions[j].base) {
				swap = context->regions[i];
				context->regions[i] = context->regions[j];
				context->regions[j] = swap;
			}
		}
	}
}

static void _sMargeMemBlockInfo(ProcMemSegment *segment)
{
	MemDumpContext *context;
	ProcMemRegion *region;
	uintptr_t base, end;
	SceUInt32 i;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 942, "_sMargeMemBlockInfo");
	context = segment->context;
	SCE_DBG_ASSERT_FUNC(context != NULL, 945, "_sMargeMemBlockInfo");
	context->numMergedRegions = 0;
	context->mergedDataSize = 0;
	if (context->numRegions == 0)
		return;
	_sSortMemBlockInfo(segment);
	for (i = 0; i < context->numRegions - 1; i++) {
		region = &context->regions[i];
		base = region->base;
		end = region[1].base + region[1].size;
		if (region[1].base <= base + region->size &&
		    base + region->size <= end) {
			region->base = 0;
			region->size = 0;
			region[1].base = base;
			region[1].size = end - base;
		}
	}
	for (i = 0; i < context->maxRegions; i++) {
		if (context->regions[i].size != 0) {
			context->numMergedRegions++;
			context->mergedDataSize += context->regions[i].size;
		}
	}
}

/*
 * The mini dump keeps the pages around every address the aborted thread's
 * registers and the top of its stack hold, in whichever block holds it.
 */
static int _getMemBlockFromAddressOnUserRegisterOrStack(ProcMemSegment *segment)
{
	MemDumpContext *context;
	CoredumpMemBlockInfo info;
	uint32_t stack[128];
	const uint32_t *registers;
	uintptr_t sp, stackEnd;
	SceUID *ids;
	SceUInt32 numBlocks, i;
	int k;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 739,
		"_getMemBlockFromAddressOnUserRegisterOrStack");
	context = segment->context;
	SCE_DBG_ASSERT_FUNC(context != NULL, 742,
		"_getMemBlockFromAddressOnUserRegisterOrStack");
	memset(stack, 0, sizeof(stack));
	sp = context->registers.registers.sp;
	stackEnd = context->stackBase + context->stackSize;
	if (sp <= stackEnd)
		oalCopyFromUserSpace(coredumpRun.pid, stack, (const void *)sp,
			stackEnd - sp >= sizeof(stack) ? sizeof(stack) :
			stackEnd - sp);
	numBlocks = oalGetNumOfMemBlocks(coredumpRun.pid);
	ids = memMgrLocalAlloc(numBlocks * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetMemBlockIdList(coredumpRun.pid, ids, numBlocks);
	registers = (const uint32_t *)&context->registers.registers;
	for (i = 0; i < numBlocks; i++) {
		oalGetMemBlockInfo(coredumpRun.pid, ids[i], &info);
		for (k = 0; k < 16; k++)
			_sPushIfInRange(segment, registers[k], info.vbase,
				info.size);
		for (k = 0; k < 128; k++)
			_sPushIfInRange(segment, stack[k], info.vbase, info.size);
	}
	_sMargeMemBlockInfo(segment);
	segment->numMemBlocks = context->numMergedRegions;
	segment->memBlockDataSize = context->mergedDataSize;
	memMgrLocalFree(ids);
	return 0;
}
