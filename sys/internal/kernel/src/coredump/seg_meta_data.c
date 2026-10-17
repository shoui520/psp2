/* META_DATA_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include <modulemgr/modulemgr_debugger.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern void *memMgrLocalAllAlloc(SceSize *size);
extern int memMgrLocalFree(void *allocation);
extern int writerWritePaddingData(Writer *writer, SceSize size);
extern int oalGetNumAndTotalSizeOfMetaData(SceUID pid, int *numMetaData,
	SceSize *totalSize, SceKernelModuleInfoForDebugger *modules,
	SceSize capacity);
extern int oalEachMetaDataInfo(SceUID pid,
	int (*callback)(CoredumpMetaDataInfo *info, void *metadata,
		SceSize paddingSize, void *context),
	SceKernelModuleInfoForDebugger *modules, SceSize capacity, void *context);
static int _writeCallback(CoredumpMetaDataInfo *info, void *metadata,
	SceSize paddingSize, void *context);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod metaDataMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf metaDataSegment;

static int _calcSize(SegIf *segment)
{
	SceKernelModuleInfoForDebugger *modules;
	SceSize capacity;
	int numMetaData;
	SceSize totalSize;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 62, "_calcSize");
	segment->writtenSize = 0;
	segment->numPhdr = 0;
	segment->dataSize = 0;
	modules = memMgrLocalAllAlloc(&capacity);
	if (modules != NULL) {
		if (oalGetNumAndTotalSizeOfMetaData(coredumpRun.pid,
			&numMetaData, &totalSize, modules, capacity) == 0 &&
			numMetaData != 0) {
			segment->numPhdr = 1;
			segment->dataSize = totalSize + 1060 +
				numMetaData * sizeof(CoredumpMetaDataInfo);
		}
		memMgrLocalFree(modules);
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceSize capacity;
	int numMetaData;
	SceSize totalSize;
	CoredumpNoteHeader header;
	SceKernelModuleInfoForDebugger *modules;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 111, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	modules = memMgrLocalAllAlloc(&capacity);
	if (modules == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	result = oalGetNumAndTotalSizeOfMetaData(coredumpRun.pid, &numMetaData,
		&totalSize, modules, capacity);
	if (result != 0 || numMetaData == 0)
		goto out;
	utilsMakeNoteHeader(&header, "META_DATA_INFO", 0x1019, 2,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &numMetaData, sizeof(numMetaData));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(numMetaData);
	result = oalEachMetaDataInfo(coredumpRun.pid, _writeCallback,
		modules, capacity, &segment->writtenSize);
	if (result != 0)
		goto out;
	if (segment->writtenSize < segment->dataSize) {
		writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		segment->writtenSize = segment->dataSize;
	}
out:
	memMgrLocalFree(modules);
	return result;
}

static int _writeCallback(CoredumpMetaDataInfo *info, void *metadata,
	SceSize paddingSize, void *context)
{
	SceSize *writtenSize = context;
	int result;

	SCE_DBG_ASSERT_FUNC(info != NULL, 190, "_writeCallback");
	SCE_DBG_ASSERT_FUNC(context != NULL, 191, "_writeCallback");
	result = writerWrite(currentWriter, info, sizeof(*info));
	if (result != 0)
		return result;
	*writtenSize += sizeof(*info);
	result = writerWrite(currentWriter, metadata, info->size);
	if (result != 0)
		return result;
	*writtenSize += info->size;
	result = writerWritePaddingData(currentWriter, paddingSize);
	if (result == 0)
		*writtenSize += paddingSize;
	return result;
}
