/* fileWriter destination operations. */

#include "writer_internal.h"
#include <sysmem/debug.h>
#include <iofilemgr/iofilemgr.h>

static int _writeDataToDest(Writer *writer, const void *data, SceSize size,
	SceSize *written);
static int _setDestOffset(Writer *writer, SceOff offset);
static int _getDestOffset(Writer *writer, SceOff *offset);
static int _isSetDestOffsetSupported(Writer *writer);

const WriterOperations fileWriterOperations = {
	.writeDataToDest = _writeDataToDest,
	.setDestOffset = _setDestOffset,
	.getDestOffset = _getDestOffset,
	.isSetDestOffsetSupported = _isSetDestOffsetSupported,
};

static int _isSetDestOffsetSupported(Writer *writer)
{
	SCE_DBG_ASSERT(writer != NULL, 225);
	return 1;
}

static int _writeDataToDest(Writer *base, const void *data, SceSize size,
	SceSize *written)
{
	FileWriter *writer = (FileWriter *)base;
	SceIoStat stat;
	SceUID fd;
	SceSize total = 0;
	int result;

	SCE_DBG_ASSERT(base != NULL, 110);
	if (writer->fd == 0) {
		fd = writer->openFile();
		if (fd < 0)
			return fd;
		writer->fd = fd;
		if (writer->reservedFileSize != 0) {
			stat.st_size = writer->reservedFileSize;
			sceIoChstatByFd(fd, &stat, 4);
			sceIoLseek(fd, 0, SCE_SEEK_SET);
		}
		oalFileSetIoCacheSize(writer->fd, 0x8000);
	}
	while (size != 0) {
		result = oalFileWrite(writer->fd, (const char *)data + total, size);
		if (result < 0)
			return result;
		size -= result;
		total += result;
		writer->writtenSize += result;
	}
	*written = total;
	return 0;
}

static int _getDestOffset(Writer *base, SceOff *offset)
{
	FileWriter *writer = (FileWriter *)base;
	SceOff position;
	int result;

	SCE_DBG_ASSERT(base != NULL, 190);
	if (writer->fd == 0) {
		*offset = base->bufferedSize;
		return 0;
	}
	result = writerFlush(base);
	if (result != 0)
		return result;
	position = oalFileSeek(writer->fd, 0, SCE_SEEK_CUR);
	if (position < 0)
		return (int)position;
	*offset = position;
	return 0;
}

static int _setDestOffset(Writer *base, SceOff offset)
{
	FileWriter *writer = (FileWriter *)base;
	SceOff position;
	int result;

	SCE_DBG_ASSERT(base != NULL, 162);
	result = writerFlush(base);
	if (result != 0)
		return result;
	position = oalFileSeek(writer->fd, offset, SCE_SEEK_SET);
	return position < 0 ? (int)position : 0;
}

int fileWriterInit(FileWriter *writer, FileWriterOpenCallback openFile,
	SceSize bufferSize, SceSize reservedFileSize)
{
	int result;

	SCE_DBG_ASSERT(writer != NULL, 58);
	result = writerInit(&writer->writer, bufferSize);
	if (result == 0) {
		writer->fd = 0;
		writer->openFile = openFile;
		writer->reservedFileSize = reservedFileSize;
		writer->writtenSize = 0;
		writer->writer.operations = &fileWriterOperations;
	}
	return result;
}

int fileWriterFinal(FileWriter *writer)
{
	SceIoStat stat;
	SceUID fd;
	SceSize writtenSize;

	SCE_DBG_ASSERT(writer != NULL, 79);
	writtenSize = writer->writtenSize;
	writerFlush(&writer->writer);
	if (writer->reservedFileSize != 0) {
		fd = writer->fd;
		stat.st_size = writer->writtenSize;
		sceIoChstatByFd(fd, &stat, 4);
		sceIoLseek(fd, 0, SCE_SEEK_SET);
	}
	writerFinal(&writer->writer);
	if (writer->fd != 0)
		oalFileClose(writer->fd);
	return writtenSize;
}
