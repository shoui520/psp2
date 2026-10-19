/* ULT_RWLOCK_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod ultRwlockMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf ultRwlockSegment;

static int _calcSize(SegIf *segment)
{
	CoredumpUltRwlockInfo info;
	SceUInt32 count, numReaders, numWriters, writerThreadId;
	SceUInt32 totalWaiting = 0;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 57, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 3);
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
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 3);
		for (i = 0; i < batch; i++) {
			oalGetUltRwlockInfo(coredumpRun.pid, ids[i], &info,
				&writerThreadId, 1024, waitingIds, &numReaders,
				&numWriters);
			totalWaiting += numWriters + numReaders;
		}
		skip += 1024;
		remaining -= batch;
	} while (count > skip);
	memMgrLocalFree(waitingIds);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = (totalWaiting + count * 17 + 15) * 4;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceUInt32 count, numReaders, numWriters, writerThreadId;
	CoredumpNoteHeader header;
	CoredumpUltRwlockInfo info;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;
	SceSize recordSize, waitingSize;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 123, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 3);
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	waitingIds = memMgrLocalAlloc(1024 * sizeof(*waitingIds));
	if (waitingIds == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out_ids;
	}
	utilsMakeNoteHeader(&header, "ULT_RWLOCK_INFO", 0x1028, 17,
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
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 3);
		for (i = 0; i != batch; ++i) {
			oalGetUltRwlockInfo(coredumpRun.pid, ids[i], &info,
				&writerThreadId, 1024, waitingIds, &numReaders,
				&numWriters);
			recordSize = (numWriters + numReaders + 17) * 4;
			info.recordSize = recordSize;
			result = writerWrite(currentWriter, &info, sizeof(info));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(info);
			waitingSize = recordSize - sizeof(info) - sizeof(writerThreadId);
			result = writerWrite(currentWriter, waitingIds,
				waitingSize);
			if (result != 0)
				goto out;
			segment->writtenSize += waitingSize;
			result = writerWrite(currentWriter, &writerThreadId,
				sizeof(writerThreadId));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(writerThreadId);
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
