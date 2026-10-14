/* ULT_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
typedef struct CoredumpUltWriteCounts {
	SceUInt32 numRuntimes;
	SceUInt32 numUlts;
} CoredumpUltWriteCounts;

extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);
extern int oalGetNumOfUltRuntimes(SceUID pid);
extern int oalEachUltRuntimeInfo(SceUID pid,
	int (*callback)(SceUInt32 runtimeId, CoredumpUltRuntimeInfo *info,
		void *context),
	SceUInt32 *ids, SceSize capacity, void *context);
extern int oalEachUltInfo(SceUID pid, SceUIntPtr runtimeId,
	int (*callback)(CoredumpUltInfo *info, void *context),
	SceUInt32 *ids, SceSize capacity, void *context);
extern void *memMgrLocalAllAlloc(SceSize *size);

static int _getSizeCallback(SceUInt32 runtimeId,
	CoredumpUltRuntimeInfo *info, void *context);
static int _writeUltRuntimeInfoCallback(SceUInt32 runtimeId,
	CoredumpUltRuntimeInfo *info, void *context);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod ultMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf ultSegment;

static int _calcSize(SegIf *segment)
{
	CoredumpUltWriteCounts counts;
	SceUInt32 *ids;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 72, "_calcSize");
	segment->writtenSize = 0;
	counts = (CoredumpUltWriteCounts){ 0 };
	ids = memMgrLocalAlloc(1024);
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	result = oalEachUltRuntimeInfo(coredumpRun.pid, _getSizeCallback,
		ids, 1024, &counts);
	if (result == 0) {
		if (counts.numRuntimes != 0) {
			segment->numPhdr = 1;
			segment->dataSize = counts.numUlts * sizeof(CoredumpUltInfo) +
				counts.numRuntimes * sizeof(CoredumpUltRuntimeInfo) + 36;
		} else {
			segment->numPhdr = 0;
			segment->dataSize = 0;
		}
	}
	memMgrLocalFree(ids);
	return result;
}

static int _writeSegs(SegIf *segment)
{
	int numRuntimes;
	CoredumpUltWriteCounts counts;
	SceSize writtenSize;
	CoredumpNoteHeader header;
	SceUInt32 *ids;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 127, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	counts = (CoredumpUltWriteCounts){ 0 };
	ids = memMgrLocalAlloc(1024);
	if (ids == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto done;
	}
	utilsMakeNoteHeader(&header, "ULT_INFO", 0x1018, 2,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	numRuntimes = oalGetNumOfUltRuntimes(coredumpRun.pid);
	result = writerWrite(currentWriter, &numRuntimes, sizeof(numRuntimes));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(numRuntimes);
	result = oalEachUltRuntimeInfo(coredumpRun.pid,
		_writeUltRuntimeInfoCallback, ids, 1024, &counts);
	if (result != 0)
		goto out;
	writtenSize = counts.numRuntimes * sizeof(CoredumpUltRuntimeInfo) +
		counts.numUlts * sizeof(CoredumpUltInfo);
	segment->writtenSize += writtenSize;
	writtenSize += 36;
	result = segWritePaddingData(segment, writtenSize);
	segment->writtenSize += segment->dataSize - writtenSize;
out:
	memMgrLocalFree(ids);
done:
	return result;
}

static int _getSizeCallback(SceUInt32 runtimeId,
	CoredumpUltRuntimeInfo *info, void *context)
{
	CoredumpUltWriteCounts *counts = context;

	SCE_DBG_ASSERT_FUNC(info != NULL, 200, "_getSizeCallback");
	SCE_DBG_ASSERT_FUNC(counts != NULL, 201, "_getSizeCallback");
	counts->numRuntimes++;
	counts->numUlts += info->numUsedUlts;
	return 0;
}

static int _writeUltInfoCallback(CoredumpUltInfo *info, void *context)
{
	CoredumpUltWriteCounts *counts = context;
	int result;

	SCE_DBG_ASSERT_FUNC(info != NULL, 264, "_writeUltInfoCallback");
	SCE_DBG_ASSERT_FUNC(counts != NULL, 265, "_writeUltInfoCallback");
	result = writerWrite(currentWriter, info, sizeof(*info));
	if (result == 0)
		counts->numUlts++;
	return result;
}

static int _writeUltRuntimeInfoCallback(SceUInt32 runtimeId,
	CoredumpUltRuntimeInfo *info, void *context)
{
	CoredumpUltWriteCounts *counts = context;
	SceSize size;
	void *ids;
	int result;

	SCE_DBG_ASSERT_FUNC(info != NULL, 216, "_writeUltRuntimeInfoCallback");
	SCE_DBG_ASSERT_FUNC(counts != NULL, 217, "_writeUltRuntimeInfoCallback");
	result = writerWrite(currentWriter, info, sizeof(*info));
	if (result != 0)
		return result;
	counts->numRuntimes++;
	ids = memMgrLocalAllAlloc(&size);
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	result = oalEachUltInfo(coredumpRun.pid, runtimeId, _writeUltInfoCallback,
		ids, size, counts);
	memMgrLocalFree(ids);
	return result;
}
