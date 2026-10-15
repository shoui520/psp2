/* ULT_QUEUE_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include <string.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalGetNumOfUltObjects(SceUID pid, int type);
extern void oalGetUltObjIds(SceUID pid, SceUInt32 *ids, SceUInt32 capacity,
	SceUInt32 skip, int type);
extern void oalGetUltQueueInfo(SceUID pid, SceUIntPtr address,
	CoredumpUltQueueInfo *info, SceUInt32 maxIds, SceUInt32 *ids,
	SceUInt32 *numSenders, SceUInt32 *numReceivers, SceUInt32 *numData);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);
extern int writerWritePaddingData(Writer *writer, SceSize size);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod ultQueueMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf ultQueueSegment;

static int _calcSize(SegIf *segment)
{
	CoredumpUltQueueInfo info;
	SceUInt32 count, numSenders, numReceivers, numData;
	SceUInt32 totalWaiting = 0, totalData = 0;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 64, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 4);
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	waitingIds = memMgrLocalAlloc(2048 * sizeof(*waitingIds));
	if (waitingIds == NULL) {
		memMgrLocalFree(ids);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	remaining = count;
	skip = 0;
	do {
		batch = remaining;
		batch = batch < 1024 ? batch : 1024;
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 4);
		for (i = 0; i < batch; i++) {
			oalGetUltQueueInfo(coredumpRun.pid, ids[i], &info, 2048,
				waitingIds, &numSenders, &numReceivers, &numData);
			totalWaiting += numSenders + numReceivers;
			totalData += (info.dataSize * numData + 3) & ~3u;
		}
		skip += 1024;
		remaining -= batch;
	} while (count > skip);
	memMgrLocalFree(waitingIds);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = totalData + 20 + (totalWaiting + count * 17 + 9) * 4;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceUInt32 count, numSenders, numReceivers, numData;
	CoredumpNoteHeader header;
	CoredumpUltQueueInfo info;
	SceUInt32 *ids, *waitingIds;
	SceUInt32 remaining, skip, batch, i;
	SceUInt32 j, numWaiting;
	SceSize dataWritten, padding;
	void *data;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 131, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfUltObjects(coredumpRun.pid, 4);
	ids = memMgrLocalAlloc(1024 * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	waitingIds = memMgrLocalAlloc(2048 * sizeof(*waitingIds));
	if (waitingIds == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out_ids;
	}
	utilsMakeNoteHeader(&header, "ULT_QUEUE_INFO", 0x1027, 7,
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
		oalGetUltObjIds(coredumpRun.pid, ids, batch, skip, 4);
		for (i = 0; i != batch; ++i) {
			oalGetUltQueueInfo(coredumpRun.pid, ids[i], &info,
				2048, waitingIds, &numSenders, &numReceivers,
				&numData);
			numWaiting = numReceivers + numSenders;
			result = writerWrite(currentWriter, &info, sizeof(info));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(info);
			result = writerWrite(currentWriter, waitingIds,
				numWaiting * sizeof(*waitingIds));
			if (result != 0)
				goto out;
			segment->writtenSize += numWaiting * sizeof(*waitingIds);
			if (info.dataSize == 0)
				continue;
			data = memMgrLocalAlloc(info.dataSize);
			dataWritten = 0;
			if (data != NULL) {
				for (j = 0; j < numData; ++j) {
					memset(data, 0, info.dataSize);
					ksceKernelCopyFromUserProc(coredumpRun.pid, data,
						(const void *)waitingIds[numWaiting + j],
						info.dataSize);
					result = writerWrite(currentWriter, data,
						info.dataSize);
					if (result != 0) {
						memMgrLocalFree(data);
						goto out;
					}
					dataWritten += info.dataSize;
					segment->writtenSize += info.dataSize;
				}
			} else {
				for (j = 0; j < numData; ++j) {
					result = writerWritePaddingData(currentWriter,
						info.dataSize);
					if (result != 0)
						goto out;
					dataWritten += info.dataSize;
					segment->writtenSize += info.dataSize;
				}
			}
			padding = ((dataWritten + 3) & ~3U) - dataWritten;
			memMgrLocalFree(data);
			result = writerWritePaddingData(currentWriter, padding);
			if (result != 0)
				goto out;
			segment->writtenSize += padding;
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
