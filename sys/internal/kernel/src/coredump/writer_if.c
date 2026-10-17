/* Buffered writer interface. */

#include "writer_internal.h"
#include <sysmem/debug.h>
#include <string.h>

extern void *memMgrAlloc(SceSize size);
extern int memMgrFreeForWriter(void *allocation);

static const unsigned char writerPadding[8] = { 0 };

int writerInit(Writer *writer, SceSize bufferSize)
{
	SCE_DBG_ASSERT(writer != NULL && bufferSize != 0, 35);
	writer->bufferSize = bufferSize;
	writer->bufferedSize = 0;
	writer->buffer = memMgrAlloc(bufferSize);
	if (writer->buffer == NULL)
		return (int)0x8001000c;
	memset(writer->buffer, 0, writer->bufferSize);
	return 0;
}

int writerFinal(Writer *writer)
{
	SCE_DBG_ASSERT(writer != NULL, 68);
	if (writer->buffer != NULL)
		memMgrFreeForWriter(writer->buffer);
	memset(writer, 0, sizeof(*writer));
	return 0;
}

static inline int _repeatWriteToDest(Writer *writer, const void *data,
	SceSize size)
{
	SceSize offset = 0, written;
	int result;

	SCE_DBG_ASSERT_FUNC(writer != NULL, 239, "_repeatWriteToDest");
	while (size != 0) {
		result = writer->operations->writeDataToDest(writer,
			(const char *)data + offset, size, &written);
		if (result != 0)
			return result;
		if (written == 0)
			break;
		size -= written;
		offset += written;
	}
	return 0;
}

int writerWrite(Writer *writer, const void *data, SceSize size)
{
	char *buffer;
	SceSize capacity, bufferedSize, available, offset = 0;
	int result;

	SCE_DBG_ASSERT(writer != NULL, 90);
	buffer = writer->buffer;
	capacity = writer->bufferSize;
	bufferedSize = writer->bufferedSize;
	if (bufferedSize != 0) {
		available = capacity - bufferedSize;
		if (size <= available)
			goto append;
		if (available != 0)
			memcpy(buffer + bufferedSize, data, available);
		result = _repeatWriteToDest(writer, buffer, capacity);
		if (result != 0)
			return result;
		writer->bufferedSize = 0;
		offset = available;
		size -= available;
		bufferedSize = 0;
	}
	if (size > capacity)
		return _repeatWriteToDest(writer, (const char *)data + offset, size);
append:
	if (size != 0 && capacity - bufferedSize >= size) {
		memcpy(buffer + bufferedSize, (const char *)data + offset, size);
		writer->bufferedSize += size;
	}
	return 0;
}

int writerFlush(Writer *writer)
{
	int result;

	SCE_DBG_ASSERT(writer != NULL, 161);
	result = _repeatWriteToDest(writer, writer->buffer, writer->bufferedSize);
	if (result == 0)
		writer->bufferedSize = 0;
	return result;
}

int writerWritePaddingData(Writer *writer, SceSize size)
{
	SceSize writeSize;
	int result;

	SCE_DBG_ASSERT(writer != NULL, 180);
	while (size != 0) {
		writeSize = size < sizeof(writerPadding) ? size : sizeof(writerPadding);
		result = writerWrite(writer, writerPadding, writeSize);
		if (result != 0)
			return result;
		size -= writeSize;
	}
	return 0;
}

int writerSetDestOffset(Writer *writer, SceOff offset)
{
	SCE_DBG_ASSERT(writer != NULL, 204);
	return writer->operations->setDestOffset(writer, offset);
}

int writerGetDestOffset(Writer *writer, SceOff *offset)
{
	SCE_DBG_ASSERT(writer != NULL, 217);
	return writer->operations->getDestOffset(writer, offset);
}
