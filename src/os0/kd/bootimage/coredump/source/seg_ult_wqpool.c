/* ULT_WQPOOL_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalGetNumOfUltObjects(SceUID pid, int type);
extern void oalGetUltObjIds(SceUID pid, SceUInt32 *ids, SceUInt32 capacity,
	SceUInt32 skip, int type);
extern void oalGetUltWaitingQueueResourcePoolInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltWaitingQueueResourcePoolInfo *info);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod ultWaitingQueuePoolMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf ultWaitingQueuePoolSegment;

static int _calcSize(SegIf *segment)
{
	unsigned int count;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 55, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 5);
	if (count != 0) {
		segment->numPhdr = 1;
		segment->dataSize = (count + 1) * 56;
	} else {
		segment->numPhdr = 0;
		segment->dataSize = 0;
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceUInt32 count;
	CoredumpNoteHeader header;
	CoredumpUltWaitingQueueResourcePoolInfo info;
	SceUInt32 *ids, *cursor;
	SceUInt32 remaining, skip, batch, i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 83, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 5);
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto done;
	}
	utilsMakeNoteHeader(&header, "ULT_WQPOOL_INFO", 0x1026, 7,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	remaining = count;
	for (skip = 0; skip < count; skip += 1024) {
		batch = remaining;
		if (batch >= 1024)
			batch = 1024;
		cursor = ids;
		i = 0;
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 5);
		for (; i != batch; ++i) {
			oalGetUltWaitingQueueResourcePoolInfo(coredumpRun.pid,
				*cursor++, &info);
			result = writerWrite(currentWriter, &info, sizeof(info));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(info);
		}
		remaining -= batch;
	}
	result = segWritePaddingData(segment, segment->writtenSize);
	segment->writtenSize = segment->dataSize;
out:
	memMgrLocalFree(ids);
done:
	return result;
}
