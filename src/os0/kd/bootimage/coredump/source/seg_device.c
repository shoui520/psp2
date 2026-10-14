/* DEVICE_INFO note. */

#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern void oalGetDeviceInfo(CoredumpDeviceInfo *info, SceUID *renderingPids,
	SceSize *renderingCount, SceUID *touchPids, SceSize *touchCount);
extern int writerWritePaddingData(Writer *writer, SceSize size);
extern int deviceCalcSize(SegIf *segment);

static int _writeSegs(SegIf *segment);

const SegIfMethod deviceMethods = {
	segInitBase,
	deviceCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _writeSegs(SegIf *segment)
{
	SceSize renderingCount, touchCount;
	CoredumpDeviceInfo info;
	CoredumpNoteHeader header;
	SceUID renderingPids[16], touchPids[16];
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 106, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	renderingCount = 16;
	touchCount = 16;
	oalGetDeviceInfo(&info, renderingPids, &renderingCount, touchPids,
		&touchCount);
	utilsMakeNoteHeader(&header, "DEVICE_INFO", 0x1021, 6,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &info, sizeof(info));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(info);
	result = writerWrite(currentWriter, &renderingCount,
		sizeof(renderingCount));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(renderingCount);
	result = writerWrite(currentWriter, renderingPids,
		renderingCount * sizeof(*renderingPids));
	if (result != 0)
		return result;
	segment->writtenSize += renderingCount * sizeof(*renderingPids);
	result = writerWrite(currentWriter, &touchCount, sizeof(touchCount));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(touchCount);
	result = writerWrite(currentWriter, touchPids,
		touchCount * sizeof(*touchPids));
	if (result != 0)
		return result;
	segment->writtenSize += touchCount * sizeof(*touchPids);
	if (segment->writtenSize < segment->dataSize) {
		writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		segment->writtenSize = segment->dataSize;
	}
	return result;
}
