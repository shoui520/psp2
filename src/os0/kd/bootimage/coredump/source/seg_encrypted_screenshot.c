/* SYSTEM_INFO2 screenshot-file payload. */

#include <sce/appmgr_debugger.h>
#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern char encryptedScreenshotFilePath[];
SceSize encryptedScreenshotSize;
extern int oalFileReopen(const char *path, SceUID *fd);
extern int oalFileRead(SceUID fd, void *buffer, SceSize size);
extern SceOff oalFileSeek(SceUID fd, SceOff offset, int whence);
extern int oalFileClose(SceUID fd);
extern int writerWritePaddingData(Writer *writer, SceSize size);
extern int oalGetFrameBufferInfo(SceUID pid, unsigned int index,
	unsigned int *vblankcount, unsigned int *width, unsigned int *height,
	unsigned int *pixelformat);

static int _init(SegIf *segment);
static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod encryptedScreenshotMethods = {
	_init,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf encryptedScreenshotSegment;

static int _init(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 51, "_init");
	return segInitBase(segment);
}

static int _calcSize(SegIf *segment)
{
	unsigned int vblankcount, width, height, pixelformat;
	AppMgrAppList apps;
	SceSize size;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 64, "_calcSize");
	segment->writtenSize = 0;
	height = 0;
	pixelformat = 0;
	vblankcount = 0;
	width = 0;
	if (oalGetFrameBufferInfo(coredumpRun.pid, 0, &vblankcount, &width,
				  &height, &pixelformat) < 0)
		size = 304;
	else
		size = height * width * 4 + 304;
	SceSysrootForKernel_2A03DFA1(&apps);
	if (oalGetFrameBufferInfo(apps.device.shellPid, 1, &vblankcount, &width,
				  &height, &pixelformat) >= 0)
		size += width * height * 4;
	if (coredumpRun.dumpLevel == 15 && size > 80) {
		segment->numPhdr = 1;
		segment->dataSize = size + 544;
	} else {
		segment->numPhdr = 0;
		segment->dataSize = 0;
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceUID fd;
	CoredumpNoteHeader header;
	unsigned char data[1024];
	SceSize remaining, size;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 113, "_writeSegs");
	result = segment->numPhdr;
	remaining = encryptedScreenshotSize;
	if (result == 0)
		return result;
	utilsMakeNoteHeader(&header, "SYSTEM_INFO2", 0x102d, 16,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(header);
	fd = 0;
	if (oalFileReopen(encryptedScreenshotFilePath, &fd) >= 0) {
		while (remaining != 0) {
			size = remaining < sizeof(data) ? remaining : sizeof(data);
			if (oalFileRead(fd, data, size) < 0)
				break;
			if (writerWrite(currentWriter, data, size) < 0)
				break;
			remaining -= size;
			segment->writtenSize += size;
		}
	}
	if (segment->writtenSize < segment->dataSize) {
		writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		segment->writtenSize = segment->dataSize;
	}
	oalFileSeek(fd, 0, 0);
	oalFileClose(fd);
	return 0;
}
