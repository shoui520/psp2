/* SYSTEM_INFO2 screenshot-file payload. */

#include <string.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <appmgr/appmgr_debugger.h>
#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
char encryptedScreenshotFilePath[1330];
SceSize encryptedScreenshotSize;
extern int oalFileReopen(const char *path, SceUID *fd);
extern int oalFileRead(SceUID fd, void *buffer, SceSize size);
extern SceOff oalFileSeek(SceUID fd, SceOff offset, int whence);
extern int oalFileClose(SceUID fd);
extern int writerWritePaddingData(Writer *writer, SceSize size);
extern int oalGetFrameBufferDetails(SceUID pid, int index,
	unsigned int *vblankcount, unsigned int *width, unsigned int *height,
	unsigned int *pixelformat, void **base, unsigned int *pitch);
extern int oalFileRemove(const char *path);
extern int openDumpFileAtDestination(SceUID *fd, const char *filename,
	char *path);
extern int accumulateEncryptedScreenshotSize(SceSize size);
extern int cafWriterEndSegment(CafWriter *writer);

/* one frame buffer in the encrypted screenshot work file */
typedef struct EncryptedScreenshotFrame {
	SceUInt32 reserved;
	unsigned int vblankcount;
	unsigned int width;
	unsigned int height;
	unsigned int pixelformat;
} EncryptedScreenshotFrame;

_Static_assert(sizeof(EncryptedScreenshotFrame) == 20, "screenshot frame size");
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
	if (coredumpRun.trigger.dumpLevel == 15 && size > 80) {
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

static SceUID openEncryptedScreenshotFile(void)
{
	SceUID fd;
	int result;

	result = openDumpFileAtDestination(&fd, "psp2core-work.tmp",
		encryptedScreenshotFilePath);
	if (result >= 0)
		result = fd;
	return result;
}

/*
 * The process's and the shell's frame buffers, compressed into an encrypted
 * work file before the process is dumped; SYSTEM_INFO2 copies it.
 */
int writeFrameBuffers(SceUID pid)
{
	FileWriter file;
	GzipWriter gzip;
	void *bases[2];
	unsigned int pitches[2];
	SceUID pids[2];
	EncryptedScreenshotFrame frames[2];
	CafWriter caf;
	AppMgrAppList apps;
	unsigned char data[1024];
	SceSize totalSize, rowSize, size;
	unsigned int row, pitch;
	SceUID framePid;
	const char *rowAddress, *source;
	int i, result;

	encryptedScreenshotSize = 0;
	bases[0] = NULL;
	bases[1] = NULL;
	pitches[0] = 0;
	pitches[1] = 0;
	totalSize = 80;
	SceSysrootForKernel_2A03DFA1(&apps);
	pids[0] = pid;
	pids[1] = apps.device.shellPid;
	for (i = 0; i < 2; i++) {
		frames[i].reserved = 0;
		result = oalGetFrameBufferDetails(pids[i], i,
			&frames[i].vblankcount, &frames[i].width,
			&frames[i].height, &frames[i].pixelformat, &bases[i],
			&pitches[i]);
		if (result < 0)
			return result;
		totalSize += (frames[i].width * frames[i].height + 28) * 4;
	}
	memset(encryptedScreenshotFilePath, 0, sizeof(encryptedScreenshotFilePath));
	result = fileWriterInit(&file, openEncryptedScreenshotFile, 0x10000,
		totalSize);
	if (result != 0)
		return result;
	if (cafWriterInit(&caf, &file.writer, 5, 1024, 1024, 2) == 0) {
		for (i = 0; i < 2; i++) {
			if (cafWriterBeginSegment(&caf, 5) != 0)
				break;
			if (gzipWriterInit(&gzip, &caf.writer, 1024, 1024,
			    accumulateEncryptedScreenshotSize) != 0)
				goto endSegment;
			rowAddress = bases[i];
			framePid = pids[i];
			pitch = pitches[i];
			if (writerWrite(&gzip.writer, &frames[i], sizeof(frames[i])) >= 0) {
				for (row = 0; row < frames[i].height; row++) {
					source = rowAddress;
					rowSize = frames[i].width * 4;
					while (rowSize != 0) {
						size = rowSize < sizeof(data) ? rowSize : sizeof(data);
						if (ksceKernelCopyFromUserProc(framePid, data,
						    source, size) < 0)
							goto gzipFinal;
						rowSize -= size;
						source += size;
						if (writerWrite(&gzip.writer, data, size) < 0)
							goto gzipFinal;
					}
					rowAddress += pitch * 4;
				}
			}
gzipFinal:
			if (gzipWriterFinal(&gzip) != 0 ||
			    cafWriterEndSegment(&caf) < 0)
				goto endSegment;
		}
		goto cafFinal;
endSegment:
		cafWriterEndSegment(&caf);
cafFinal:
		cafWriterFinal(&caf);
	}
	result = fileWriterFinal(&file);
	if (result >= 0)
		encryptedScreenshotSize = result;
	else
		encryptedScreenshotSize = 0;
	return result;
}

int removeEncryptedScreenshot(void)
{
	int result;

	result = oalFileRemove(encryptedScreenshotFilePath);
	memset(encryptedScreenshotFilePath, 0, sizeof(encryptedScreenshotFilePath));
	return result;
}
