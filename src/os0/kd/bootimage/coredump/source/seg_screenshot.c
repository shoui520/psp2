/* SCREENSHOT_INFO note. */

#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int screenshotInit(SegIf *segment);
extern int screenshotCalcSize(SegIf *segment);
extern int oalGetFrameBufferInfo(SceUID pid, unsigned int index,
	unsigned int *vblankcount, unsigned int *width, unsigned int *height,
	unsigned int *pixelformat);
extern int oalGetFrameBuffer(SceUID pid, int index, void **base,
	unsigned int *pitch);

typedef struct CoredumpScreenshotNote {
	CoredumpNoteHeader header;
	SceUInt32 reserved;
	SceUInt32 vblankcount;
	SceUInt32 width;
	SceUInt32 height;
	SceUInt32 pixelformat;
} CoredumpScreenshotNote;

_Static_assert(sizeof(CoredumpScreenshotNote) == 52, "screenshot note size");

static int _writeSegs(SegIf *segment);

const SegIfMethod screenshotMethods = {
	screenshotInit,
	screenshotCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _writeSegs(SegIf *segment)
{
	void *base;
	unsigned int width, height, pitch, vblankcount, pixelformat;
	CoredumpScreenshotNote note;
	unsigned char data[256];
	unsigned char *source;
	SceUInt32 row;
	SceSize remaining, size;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 87, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	result = oalGetFrameBufferInfo(coredumpRun.pid, 0, &vblankcount,
		&width, &height, &pixelformat);
	if (result < 0)
		return result;
	utilsMakeNoteHeader(&note.header, "SCREENSHOT_INFO", 0x102b, 8,
		segment->dataSize);
	note.reserved = 0;
	note.vblankcount = vblankcount;
	note.width = width;
	note.height = height;
	note.pixelformat = pixelformat;
	result = writerWrite(currentWriter, &note, sizeof(note));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(note);
	result = oalGetFrameBuffer(coredumpRun.pid, 0, &base, &pitch);
	if (result < 0)
		return result;
	for (row = 0; row < height; ++row) {
		remaining = width * 4;
		source = (unsigned char *)base + row * pitch * 4;
		while (remaining != 0) {
			size = remaining < sizeof(data) ? remaining : sizeof(data);
			if (ksceKernelCopyFromUserProc(coredumpRun.pid,
				data, source, size) != 0)
				return -1;
			remaining -= size;
			result = writerWrite(currentWriter, data, size);
			source += size;
			if (result != 0)
				return result;
			segment->writtenSize += size;
		}
	}
	return 0;
}
