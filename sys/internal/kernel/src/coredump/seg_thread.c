/* THREAD_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern unsigned int oalGetNumOfThreads(SceUID pid);
extern void oalGetThreadIdList(SceUID pid, SceUID *ids, unsigned int capacity);
extern void oalGetThreadInfo(SceUID pid, SceUID threadId,
	const CoredumpThreadStopEvent *stopEvent, CoredumpThreadInfo *info);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod threadMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf threadSegment;

static int _calcSize(SegIf *segment)
{
	unsigned int count;
	SceUInt32 numPhdr;
	SceSize dataSize;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 53, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfThreads(coredumpRun.pid);
	numPhdr = count;
	dataSize = count;
	if (count != 0) {
		dataSize = count * sizeof(CoredumpThreadInfo) +
			sizeof(CoredumpNoteHeader) + sizeof(count);
		numPhdr = 1;
	}
	segment->dataSize = dataSize;
	segment->numPhdr = numPhdr;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	unsigned int count;
	CoredumpThreadStopEvent stopEvent;
	CoredumpNoteHeader header;
	CoredumpThreadInfo info;
	SceUID *threadIds;
	SceUInt32 stopReason;
	unsigned int i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 79, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfThreads(coredumpRun.pid);
	threadIds = memMgrLocalAlloc(count * sizeof(*threadIds));
	if (threadIds == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	utilsMakeNoteHeader(&header, "THREAD_INFO", 4099, 18,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetThreadIdList(coredumpRun.pid, threadIds, count);
	stopEvent.threadId = coredumpRun.trigger.crash_thid;
	stopEvent.reason = coredumpRun.trigger.triggerReason;
	for (i = 0; i < count; ++i) {
		oalGetThreadInfo(coredumpRun.pid, threadIds[i], &stopEvent, &info);
		__builtin_memcpy(&stopReason, &info.stopReason.reason,
			sizeof(stopReason));
		if (stopReason != 0)
			coredumpRun.stopReason = stopReason;
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(info);
	}
out:
	memMgrLocalFree(threadIds);
	return result;
}
