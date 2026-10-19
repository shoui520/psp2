/* gzipWriter destination operations. */

#include "writer_internal.h"
#include <sysmem/debug.h>

extern int deflateInit2Padded_(SceZlibStream *stream, int level,
	int method, int windowBits, int memLevel, int strategy,
	const char *version, int streamSize);

static int _writeDataToDest(Writer *writer, const void *data, SceSize size,
	SceSize *written);
static int _setDestOffset(Writer *writer, SceOff offset);
static int _getDestOffset(Writer *writer, SceOff *offset);
static int _isSetDestOffsetSupported(Writer *writer);

const WriterOperations gzipWriterOperations = {
	.writeDataToDest = _writeDataToDest,
	.setDestOffset = _setDestOffset,
	.getDestOffset = _getDestOffset,
	.isSetDestOffsetSupported = _isSetDestOffsetSupported,
};

static int _isSetDestOffsetSupported(Writer *writer)
{
	SCE_DBG_ASSERT(writer != NULL, 336);
	return 0;
}

static int _writeDataToDest(Writer *base, const void *data, SceSize size,
	SceSize *written)
{
	GzipWriter *writer = (GzipWriter *)base;
	Writer *destination;
	int result;

	SCE_DBG_ASSERT(base != NULL, 235);
	writer->stream.next_in = data;
	writer->stream.avail_in = size;
	destination = writer->destination;
	while (writer->stream.avail_in != 0) {
		writer->stream.next_out = writer->outputBuffer;
		writer->stream.avail_out = writer->outputBufferSize;
		result = deflate(&writer->stream, 0);
		if (result != 0)
			return result;
		result = writerWrite(destination, writer->outputBuffer,
			writer->outputBufferSize - writer->stream.avail_out);
		if (result != 0)
			return result;
	}
	*written = size;
	if (writer->getExitFlag != NULL)
		return writer->getExitFlag(size);
	return 0;
}

static int _getDestOffset(Writer *writer, SceOff *offset)
{
	SCE_DBG_ASSERT(writer != NULL, 322);
	(void)offset;
	return (int)0x80010086;
}

static int _setDestOffset(Writer *writer, SceOff offset)
{
	SCE_DBG_ASSERT(writer != NULL, 307);
	(void)offset;
	return (int)0x80010086;
}

static void *zAlloc(void *opaque, SceUInt32 items, SceUInt32 size)
{
	(void)opaque;
	return memMgrAlloc(size * items);
}

static void zFree(void *opaque, void *allocation)
{
	(void)opaque;
	memMgrFreeForWriter(allocation);
}

int gzipWriterInit(GzipWriter *writer, Writer *destination,
	SceSize bufferSize, SceSize outputBufferSize,
	int (*getExitFlag)(SceSize processedSize))
{
	int result;

	SCE_DBG_ASSERT(writer != NULL, 75);
	result = writerInit(&writer->writer, bufferSize);
	if (result != 0)
		return result;
	writer->destination = destination;
	writer->outputBufferSize = outputBufferSize;
	writer->writer.operations = &gzipWriterOperations;
	writer->outputBuffer = memMgrAlloc(outputBufferSize);
	writer->getExitFlag = getExitFlag;
	if (writer->outputBuffer == NULL) {
		writerFinal(&writer->writer);
		return (int)0x8001000c;
	}
	writer->stream.opaque = NULL;
	writer->stream.next_in = NULL;
	writer->stream.next_out = NULL;
	writer->stream.avail_in = 0;
	writer->stream.avail_out = 0;
	writer->stream.zalloc = zAlloc;
	writer->stream.zfree = zFree;
	result = deflateInit2Padded_(&writer->stream, 1, 8, 24, 1, 0,
		"1.2.5", sizeof(writer->stream));
	if (result != 0) {
		memMgrFreeForWriter(writer->outputBuffer);
		writerFinal(&writer->writer);
		return (int)0x80010005;
	}
	return result;
}

int gzipWriterFinal(GzipWriter *writer)
{
	Writer *destination;
	SceSize bufferedSize;
	int result, finalResult, status;

	SCE_DBG_ASSERT(writer != NULL, 133);
	writer->stream.next_in = writer->writer.buffer;
	writer->stream.avail_in = writer->writer.bufferedSize;
	destination = writer->destination;
	for (;;) {
		writer->stream.next_out = writer->outputBuffer;
		writer->stream.avail_out = writer->outputBufferSize;
		status = deflate(&writer->stream, 4);
		if ((unsigned int)status > 1) {
			result = (int)0x80010005;
			break;
		}
		result = writerWrite(destination, writer->outputBuffer,
			writer->outputBufferSize - writer->stream.avail_out);
		if (result != 0 || status == 1)
			break;
	}
	bufferedSize = writer->writer.bufferedSize;
	writer->writer.bufferedSize = 0;
	deflateEnd(&writer->stream);
	memMgrFreeForWriter(writer->outputBuffer);
	finalResult = writerFinal(&writer->writer);
	if (finalResult != 0)
		return finalResult;
	if (writer->getExitFlag != NULL) {
		finalResult = writer->getExitFlag(bufferedSize);
		if (finalResult != 0)
			return finalResult;
	}
	return result;
}
