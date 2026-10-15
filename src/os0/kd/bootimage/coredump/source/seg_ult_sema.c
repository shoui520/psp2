/* ULT_SEMA_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalGetNumOfUltObjects(SceUID pid, int type);
extern void oalGetUltObjIds(SceUID pid, SceUInt32 *ids, SceUInt32 capacity,
	SceUInt32 skip, int type);
extern void oalGetUltSemaInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltSemaInfo *info, SceUInt32 *ids, SceUInt32 *numIds);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod ultSemaMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf ultSemaSegment;

static int _calcSize(SegIf *segment)
{
	CoredumpUltSemaInfo info;
	SceUInt32 count, numWaiting, totalWaiting = 0;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 57, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 2);
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
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 2);
		for (i = 0; i < batch; i++) {
			numWaiting = 1024;
			oalGetUltSemaInfo(coredumpRun.pid, ids[i], &info, waitingIds,
				&numWaiting);
			totalWaiting += numWaiting;
		}
		skip += 1024;
		remaining -= batch;
	} while (count > skip);
	memMgrLocalFree(waitingIds);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = (totalWaiting + count * 14 + 14) * 4;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceUInt32 count, numWaiting;
	CoredumpNoteHeader header;
	CoredumpUltSemaInfo info;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 118, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 2);
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	waitingIds = memMgrLocalAlloc(1024 * sizeof(*waitingIds));
	if (waitingIds == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out_ids;
	}
	utilsMakeNoteHeader(&header, "ULT_SEMA_INFO", 0x1023, 7,
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
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 2);
		for (i = 0; i != batch; ++i) {
			numWaiting = 1024;
			oalGetUltSemaInfo(coredumpRun.pid, ids[i], &info,
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
