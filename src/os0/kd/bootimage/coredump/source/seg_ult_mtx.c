/* ULT_MUTEX_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalGetNumOfUltObjects(SceUID pid, int type);
extern void oalGetUltObjIds(SceUID pid, SceUInt32 *ids, SceUInt32 capacity,
	SceUInt32 skip, int type);
extern void oalGetUltMutexInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltMutexInfo *info, SceUInt32 *ids, SceUInt32 *numIds);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);
extern int ultMutexCalcSize(SegIf *segment);

static int _writeSegs(SegIf *segment);

const SegIfMethod ultMutexMethods = {
	segInitBase,
	ultMutexCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _writeSegs(SegIf *segment)
{
	SceUInt32 count, numWaiting;
	CoredumpNoteHeader header;
	CoredumpUltMutexInfo info;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 118, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 0);
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	waitingIds = memMgrLocalAlloc(1024 * sizeof(*waitingIds));
	if (waitingIds == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out_ids;
	}
	utilsMakeNoteHeader(&header, "ULT_MUTEX_INFO", 0x1024, 7,
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
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 0);
		for (i = 0; i != batch; ++i) {
			numWaiting = 1024;
			oalGetUltMutexInfo(coredumpRun.pid, ids[i], &info,
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
