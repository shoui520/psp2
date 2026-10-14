/* GPU_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern void oalGetGpuInfo(void *buffer, SceSize size);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int gpuInit(SegIf *segment);
extern int gpuCalcSize(SegIf *segment);

static int _writeSegs(SegIf *segment);

const SegIfMethod gpuMethods = {
	gpuInit,
	gpuCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

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
