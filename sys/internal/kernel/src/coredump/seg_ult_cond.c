/* ULT_COND_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod ultCondMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf ultCondSegment;

static int _calcSize(SegIf *segment)
{
	CoredumpUltCondInfo info;
	SceUInt32 count, totalWaiting = 0;
	uint32_t numWaiting;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 57, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 1);
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	waitingIds = memMgrLocalAlloc(1024 * sizeof(*waitingIds));
	if (waitingIds == NULL) {
		memMgrLocalFree(ids);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	remaining = count;
	skip = 0;
	do {
		batch = remaining;
		batch = batch < 1024 ? batch : 1024;
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 1);
		for (i = 0; i < batch; i++) {
			numWaiting = 1024;
			oalGetUltCondInfo(coredumpRun.pid, ids[i], &info, waitingIds,
				&numWaiting);
			totalWaiting += numWaiting;
		}
		skip += 1024;
		remaining -= batch;
	} while (count > skip);
	memMgrLocalFree(waitingIds);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = (totalWaiting + count * 13 + 14) * 4;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceUInt32 count;
	uint32_t numWaiting;
	CoredumpNoteHeader header;
	CoredumpUltCondInfo info;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 118, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 1);
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	waitingIds = memMgrLocalAlloc(1024 * sizeof(*waitingIds));
	if (waitingIds == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out_ids;
	}
	utilsMakeNoteHeader(&header, "ULT_COND_INFO", 0x1029, 7,
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
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 1);
		for (i = 0; i != batch; ++i) {
			SceUInt32 id = ids[i];

			numWaiting = 1024;
			oalGetUltCondInfo(coredumpRun.pid, id, &info,
				waitingIds, &numWaiting);
			result = writerWrite(currentWriter, &info, sizeof(info));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(info);
			result = writerWrite(currentWriter, waitingIds,
				numWaiting * sizeof(*waitingIds));
			if (result != 0)
				goto out;
			segment->writtenSize += numWaiting * sizeof(*waitingIds);
		}
		remaining -= batch;
	}
	result = segWritePaddingData(segment, segment->writtenSize);
	segment->writtenSize = segment->dataSize;
out:
	memMgrLocalFree(waitingIds);
out_ids:
	memMgrLocalFree(ids);
	return result;
}
