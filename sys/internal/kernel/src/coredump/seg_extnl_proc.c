/* EXTNL_PROC_INFO note: every other process with its path and unwind tables. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"
#include "oal_internal.h"

extern Writer *currentWriter;

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod extnlProcMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf extnlProcSegment;

static int _calcSize(SegIf *segment)
{
	char path[256];
	SceSize pathSize, totalPathSize = 0;
	SceUID *ids;
	unsigned int count, i;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 65, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfExtnlProcs(coredumpRun.pid);
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetExtnlProcIdList(coredumpRun.pid, ids, count);
	for (i = 0; i < count; i++) {
		oalGetProcessPath(ids[i], path, &pathSize);
		totalPathSize += (pathSize + 3) & ~3u;
	}
	segment->numPhdr = 1;
	segment->dataSize = totalPathSize + 108 * count + 312;
	memMgrLocalFree(ids);
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	CoredumpNoteHeader header;
	CoredumpExtnlProcInfo info;
	CoredumpArmExInfo armExInfo;
	char path[256];
	SceUInt32 count, i;
	SceSize padding;
	SceUID *ids;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 109, "_writeSegs");
	if (segment->numPhdr == 0)
		return 0;
	count = oalGetNumOfExtnlProcs(coredumpRun.pid);
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	utilsMakeNoteHeader(&header, "EXTNL_PROC_INFO", 0x101E, 6,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto free;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto free;
	segment->writtenSize += sizeof(count);
	oalGetExtnlProcIdList(coredumpRun.pid, ids, count);
	for (i = 0; i < count; i++) {
		oalGetExtnlProcInfo(coredumpRun.pid, ids[i], &info);
		oalGetProcessPath(ids[i], path, &info.pathSize);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto free;
		segment->writtenSize += sizeof(info);
		result = writerWrite(currentWriter, path, info.pathSize);
		if (result != 0)
			goto free;
		segment->writtenSize += info.pathSize;
		padding = ((info.pathSize + 3) & ~3u) - info.pathSize;
		result = writerWritePaddingData(currentWriter, padding);
		if (result != 0)
			goto free;
		segment->writtenSize += padding;
		oalGetProcessArmExInfo(ids[i], &armExInfo);
		result = writerWrite(currentWriter, &armExInfo, sizeof(armExInfo));
		if (result != 0)
			goto free;
		segment->writtenSize += sizeof(armExInfo);
	}
	if (segment->dataSize > segment->writtenSize) {
		writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		segment->writtenSize = segment->dataSize;
	}
free:
	memMgrLocalFree(ids);
	return result;
}
