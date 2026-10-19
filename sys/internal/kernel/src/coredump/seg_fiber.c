/* FIBER_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern void *memMgrLocalAllAlloc(SceSize *size);
extern int memMgrLocalFree(void *allocation);
extern int oalGetNumOfFibers(SceUID pid);
extern int oalEachFiberInfo(SceUID pid,
	int (*callback)(CoredumpFiberInfo *info, void *context),
	SceUInt32 *ids, SceSize capacity, void *context);
static int _writeCallback(CoredumpFiberInfo *info, void *context);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod fiberMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf fiberSegment;

static int _calcSize(SegIf *segment)
{
	int count, numPhdr;
	SceSize dataSize;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 55, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfFibers(coredumpRun.pid);
	numPhdr = count;
	dataSize = count;
	if (count != 0) {
		dataSize = count * sizeof(CoredumpFiberInfo) + 36;
		numPhdr = 1;
	}
	segment->dataSize = dataSize;
	segment->numPhdr = numPhdr;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceSize capacity;
	int numFibers;
	CoredumpNoteHeader header;
	SceUInt32 *ids;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 81, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	ids = memMgrLocalAllAlloc(&capacity);
	if (ids == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto done;
	}
	utilsMakeNoteHeader(&header, "FIBER_INFO", 0x1017, 2,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	numFibers = oalGetNumOfFibers(coredumpRun.pid);
	result = writerWrite(currentWriter, &numFibers, sizeof(numFibers));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(numFibers);
	result = oalEachFiberInfo(coredumpRun.pid, _writeCallback,
		ids, capacity, &segment->writtenSize);
	if (result != 0)
		goto out;
	result = segWritePaddingData(segment, segment->writtenSize);
	segment->writtenSize = segment->dataSize;
out:
	memMgrLocalFree(ids);
done:
	return result;
}

static int _writeCallback(CoredumpFiberInfo *info, void *context)
{
	SceSize *writtenSize = context;
	SceSize previousSize = *writtenSize;
	int result;

	result = writerWrite(currentWriter, info, sizeof(*info));
	if (result == 0)
		*writtenSize = previousSize + sizeof(*info);
	return result;
}
