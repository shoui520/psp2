/* MESG_PIPE_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern unsigned int oalGetNumOfSyncObj(int type, SceUID pid);
extern void oalGetSyncObjIdList(int type, SceUID pid, SceUID *ids,
	SceSize capacity);
extern void oalGetMsgPipeInfo(SceUID pid, SceUID msgPipeId,
	CoredumpMsgPipeInfo *info);
extern void oalGetCommonEventInfo(SceUID pid, SceUID eventId,
	CoredumpCommonEventInfo *info);
extern void oalGetWaitingThreadList(SceUID pid, SceUID waitId,
	CoredumpWaitingThread *threads, SceSize capacity);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int segWritePaddingData(SegIf *segment, SceSize writtenSize);
extern int oalGetNumOfWaitingThreads(SceUID pid, SceUID waitId);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod msgPipeMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf msgPipeSegment;

static int _calcSize(SegIf *segment)
{
	SceUID *ids;
	unsigned int count, i, numWaiting = 0;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 55, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfSyncObj(7, coredumpRun.pid);
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetSyncObjIdList(7, coredumpRun.pid, ids, count);
	for (i = 0; i < count; i++)
		numWaiting += oalGetNumOfWaitingThreads(coredumpRun.pid, ids[i]);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = (numWaiting + count *
		(sizeof(CoredumpMsgPipeInfo) / sizeof(CoredumpWaitingThread))) *
		sizeof(CoredumpWaitingThread) + 76;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	unsigned int count;
	CoredumpNoteHeader header;
	CoredumpMsgPipeInfo info;
	CoredumpCommonEventInfo event;
	CoredumpWaitingThread *threads = NULL;
	SceUID *ids = NULL;
	unsigned int i, j, numWaiting, totalWaiting;
	SceSize writtenSize;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 102, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfSyncObj(7, coredumpRun.pid);
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
	utilsMakeNoteHeader(&header, "MESG_PIPE_INFO", 0x1010, 17,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetSyncObjIdList(7, coredumpRun.pid, ids, count);
	totalWaiting = 0;
	for (i = 0; i < count; ++i) {
		oalGetMsgPipeInfo(coredumpRun.pid, ids[i], &info);
		numWaiting = (unsigned int)info.numSendWaitThreads +
			(unsigned int)info.numReceiveWaitThreads;
		info.recordSize = 8 * (numWaiting + 10);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		if (numWaiting >= 320)
			numWaiting = 320;
		segment->writtenSize += sizeof(info);
		oalGetWaitingThreadList(coredumpRun.pid, ids[i], threads,
			numWaiting);
		for (j = 0; j != numWaiting; ++j) {
			result = writerWrite(currentWriter, &threads[j],
				sizeof(*threads));
			if (result != 0)
				goto out;
			segment->writtenSize += sizeof(*threads);
		}
		totalWaiting += numWaiting;
		oalGetCommonEventInfo(coredumpRun.pid, ids[i], &event);
		result = writerWrite(currentWriter, &event, sizeof(event));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(event);
	}
	writtenSize = 36 + count * (sizeof(info) + sizeof(event)) +
		totalWaiting * sizeof(*threads);
	result = segWritePaddingData(segment, writtenSize);
	segment->writtenSize += segment->dataSize - writtenSize;
out:
	if (threads != NULL)
		memMgrLocalFree(threads);
	if (ids != NULL)
		memMgrLocalFree(ids);
	return result;
}
