/* ULT_Q_POOL_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod ultQueueDataPoolMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf ultQueueDataPoolSegment;

static int _calcSize(SegIf *segment)
{
	unsigned int count;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 55, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 6);
	if (count != 0) {
		segment->numPhdr = 1;
		segment->dataSize = count * 64 + 56;
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
	CoredumpUltQueueDataResourcePoolInfo info;
	SceUInt32 *ids;
	SceUInt32 remaining, skip, batch, i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 83, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 6);
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto done;
	}
	utilsMakeNoteHeader(&header, "ULT_Q_POOL_INFO", 0x1025, 7,
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
		batch = batch < 1024 ? batch : 1024;
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 6);
		for (i = 0; i != batch; ++i) {
			oalGetUltQueueDataResourcePoolInfo(coredumpRun.pid,
				ids[i], &info);
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
