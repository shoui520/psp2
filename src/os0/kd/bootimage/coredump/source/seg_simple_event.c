/* SMPL_EVENT_INFO note. */

#include <string.h>
#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern unsigned int oalGetNumOfSyncObj(int type, SceUID pid);
extern void oalGetSyncObjIdList(int type, SceUID pid, SceUID *ids,
	SceSize capacity);
extern void oalGetSimpleEventInfo(SceUID pid, SceUID eventId,
	CoredumpSimpleEventInfo *info);
extern void oalGetWaitingThreadList(SceUID pid, SceUID waitId,
	CoredumpWaitingThread *threads, SceSize capacity);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);
extern int simpleEventCalcSize(SegIf *segment);

static int _writeSegs(SegIf *segment);

const SegIfMethod simpleEventMethods = {
	segInitBase,
	simpleEventCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _writeSegs(SegIf *segment)
{
	unsigned int count;
	CoredumpNoteHeader header;
	CoredumpSimpleEventInfo info;
	CoredumpWaitingThread *threads = NULL;
	SceUID *ids = NULL, *cursor;
	unsigned int i, j, numWaiting, totalWaiting;
	SceSize writtenSize;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 84, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfSyncObj(11, coredumpRun.pid);
	threads = memMgrLocalAlloc(320 * sizeof(*threads));
	if (threads == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL) {
		result = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	utilsMakeNoteHeader(&header, "SMPL_EVENT_INFO", 0x1030, 17,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetSyncObjIdList(11, coredumpRun.pid, ids, count);
	totalWaiting = 0;
	cursor = ids;
	for (i = 0; i < count; ++i) {
		memset(&info, 0, sizeof(info));
		oalGetSimpleEventInfo(coredumpRun.pid, *cursor++, &info);
		info.recordSize = sizeof(info) + sizeof(*threads) * info.numWaitThreads;
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		numWaiting = (unsigned int)info.numWaitThreads;
		if (numWaiting >= 320)
			numWaiting = 320;
		segment->writtenSize += sizeof(info);
		oalGetWaitingThreadList(coredumpRun.pid, cursor[-1], threads,
			numWaiting);
		for (j = 0; j != numWaiting; ++j) {
			result = writerWrite(currentWriter, &threads[j],
				sizeof(*threads));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(*threads);
		}
		totalWaiting += numWaiting;
	}
	writtenSize = 36 + count * sizeof(info) + totalWaiting * sizeof(*threads);
	result = segWritePaddingData(segment, writtenSize);
	segment->writtenSize += segment->dataSize - writtenSize;
out:
	if (threads != NULL)
		memMgrLocalFree(threads);
	if (ids != NULL)
		memMgrLocalFree(ids);
	return result;
}
