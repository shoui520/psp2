/* SYSTEM_INFO2 screenshot-file payload. */

#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern char encryptedScreenshotFilePath[];
SceSize encryptedScreenshotSize;
extern int encryptedScreenshotInit(SegIf *segment);
extern int encryptedScreenshotCalcSize(SegIf *segment);
extern int oalFileReopen(const char *path, SceUID *fd);
extern int oalFileRead(SceUID fd, void *buffer, SceSize size);
extern SceOff oalFileSeek(SceUID fd, SceOff offset, int whence);
extern int oalFileClose(SceUID fd);
extern int writerWritePaddingData(Writer *writer, SceSize size);

static int _writeSegs(SegIf *segment);

const SegIfMethod encryptedScreenshotMethods = {
	encryptedScreenshotInit,
	encryptedScreenshotCalcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

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
