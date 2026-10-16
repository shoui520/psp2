/* GPU_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern SceSize oalGetGpuInfoSize(void);
extern void oalGetGpuInfo(void *buffer, SceSize size);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);

static int _init(SegIf *segment);
static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod gpuMethods = {
	_init,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

GpuSegIf gpuSegment;

static int _init(SegIf *segment)
{
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 49, "_init");
	result = segInitBase(segment);
	if (result == 0)
		((GpuSegIf *)segment)->gpuInfoSize = 0;
	return result;
}

static int _calcSize(SegIf *segment)
{
	SceSize size;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 65, "_calcSize");
	segment->writtenSize = 0;
	if (coredumpRun.trigger.triggerReason != 1)
		return 0;
	size = oalGetGpuInfoSize();
	if (size == 0)
		return 0;
	((GpuSegIf *)segment)->gpuInfoSize = size;
	segment->numPhdr = 1;
	segment->dataSize = size + sizeof(CoredumpNoteHeader);
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	GpuSegIf *gpu = (GpuSegIf *)segment;
	CoredumpNoteHeader header;
	void *buffer;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 88, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	utilsMakeNoteHeader(&header, "GPU_INFO", 0x2000, 5,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(header);
	buffer = memMgrLocalAlloc(gpu->gpuInfoSize);
	if (buffer == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetGpuInfo(buffer, gpu->gpuInfoSize);
	result = writerWrite(currentWriter, buffer, gpu->gpuInfoSize);
	if (result != 0)
		memMgrLocalFree(buffer);
	else
		segment->writtenSize += gpu->gpuInfoSize;
	return result;
}
