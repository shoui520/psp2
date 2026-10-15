/* cafWriter destination operations. */

#include "writer_internal.h"
#include <sce/debug.h>

extern int oalCafSegmentTransform(SceUID context, const void *source,
	void *destination, SceSize size);
extern void *memMgrAlloc(SceSize size);
extern int memMgrFreeForWriter(void *allocation);
extern SceUID oalCafContextCreate(void);
extern int oalCafContextDestroy(SceUID context);
extern int oalCafCreateIV(void *iv);
extern int oalCafSegmentInit(SceUID context, int type, const void *iv);
extern int oalCafSegmentFinal(SceUID context, CoredumpCafSectionHash *hash);
extern int oalCafHeaderInit(SceUID context, int type);
extern int oalCafHeaderTransform(SceUID context, const void *data, SceSize size);
extern int oalCafHeaderFinal(SceUID context, void *digest);
extern int oalCafMakeHeader(CoredumpCafHeader *header, SceUInt64 segmentCount,
	SceUInt64 headerSize, SceUInt64 dataSize, int type);
extern int oalCafMakeSegmentTable(CoredumpCafSegmentTable *table,
	SceUInt64 segmentId, SceUInt64 fileOffset, SceUInt32 segmentSize,
	SceUInt32 segmentIndex, int type, const CoredumpCafIv *iv);

static int _writeDataToDest(Writer *writer, const void *data, SceSize size,
	SceSize *written);
static int _setDestOffset(Writer *writer, SceOff offset);
static int _getDestOffset(Writer *writer, SceOff *offset);
static int _isSetDestOffsetSupported(Writer *writer);

const WriterOperations cafWriterOperations = {
	.writeDataToDest = _writeDataToDest,
	.setDestOffset = _setDestOffset,
	.getDestOffset = _getDestOffset,
	.isSetDestOffsetSupported = _isSetDestOffsetSupported,
};

static int _isSetDestOffsetSupported(Writer *writer)
{
	SCE_DBG_ASSERT(writer != NULL, 316);
	return 0;
}

static int _writeDataToDest(Writer *base, const void *data, SceSize size,
	SceSize *written)
{
	CafWriter *writer = (CafWriter *)base;
	Writer *destination;
	const char *source;
	SceSize total = 0, writeSize;
	int result;

	SCE_DBG_ASSERT(base != NULL, 239);
	destination = writer->destination;
	for (;;) {
		writeSize = writer->transformBufferSize;
		if (size <= writeSize)
			writeSize = size & ~15U;
		if (writeSize == 0)
			break;
		source = (const char *)data + total;
		total += writeSize;
		size -= writeSize;
		result = oalCafSegmentTransform(writer->context, source,
			writer->transformBuffer, writeSize);
		if (result != 0)
			return result;
		result = writerWrite(destination, writer->transformBuffer, writeSize);
		if (result != 0)
			return result;
	}
	*written = total;
	return 0;
}

static int _getDestOffset(Writer *writer, SceOff *offset)
{
	SCE_DBG_ASSERT(writer != NULL, 302);
	(void)offset;
	return (int)0x80010086;
}

static int _setDestOffset(Writer *writer, SceOff offset)
{
	SCE_DBG_ASSERT(writer != NULL, 287);
	(void)offset;
	return (int)0x80010086;
}

int cafWriterInit(CafWriter *writer, Writer *destination, SceUInt32 type,
	SceSize bufferSize, SceSize transformBufferSize, SceUInt32 maxSegments)
{
	SceUID context;
	int result;

	SCE_DBG_ASSERT(writer != NULL, 68);
	SCE_DBG_ASSERT(destination != NULL, 69);
	if (maxSegments - 1 > 1)
		return (int)0x80010016;
	result = writerInit(&writer->writer, bufferSize);
	if (result != 0)
		return result;
	writer->type = type;
	writer->destination = destination;
	writer->transformBufferSize = transformBufferSize;
	writer->writer.operations = &cafWriterOperations;
	writer->transformBuffer = memMgrAlloc(transformBufferSize);
	writer->numSegments = 0;
	writer->maxSegments = maxSegments;
	if (writer->transformBuffer != NULL) {
		context = oalCafContextCreate();
		if (context >= 0) {
			writer->context = context;
			SCE_DBG_ASSERT_FUNC(writer != NULL, 421, "_writeDummyHeader");
			result = writerWritePaddingData(&writer->writer,
				112 * writer->maxSegments + 80);
			if (result == 0)
				return result;
			oalCafContextDestroy(writer->context);
		}
		memMgrFreeForWriter(writer->transformBuffer);
	} else {
		result = (int)0x8001000c;
	}
	writerFinal(&writer->writer);
	return result;
}

int cafWriterBeginSegment(CafWriter *writer, SceUInt32 type)
{
	CafWriterSegment *segment;
	Writer *destination;
	int result;

	SCE_DBG_ASSERT(writer != NULL, 142);
	if (writer->numSegments >= writer->maxSegments)
		return (int)0x80010016;
	destination = writer->destination;
	segment = &writer->segments[writer->numSegments];
	segment->segmentId = writer->numSegments;
	result = writerFlush(&writer->writer);
	if (result != 0)
		return result;
	result = writerGetDestOffset(destination, &segment->fileOffset);
	if (result != 0)
		return result;
	segment->type = type;
	result = oalCafCreateIV(&segment->iv);
	if (result != 0)
		return result;
	return oalCafSegmentInit(writer->context, segment->type, &segment->iv);
}

int cafWriterEndSegment(CafWriter *writer)
{
	CafWriterSegment *segment;
	Writer *destination;
	SceUInt32 index;
	SceOff position;
	int result;

	SCE_DBG_ASSERT(writer != NULL, 168);
	destination = writer->destination;
	index = writer->numSegments;
	result = writerFlush(&writer->writer);
	if (result != 0)
		return result;
	result = writerGetDestOffset(destination, &position);
	if (result != 0)
		return result;
	segment = &writer->segments[index];
	if (position < segment->fileOffset)
		return (int)0x80010016;
	segment->dataSize = position - segment->fileOffset;
	result = oalCafSegmentFinal(writer->context, &segment->hash);
	if (result == 0)
		writer->numSegments++;
	return result;
}

int cafWriterGetNumSegments(CafWriter *writer)
{
	SCE_DBG_ASSERT(writer != NULL, 190);
	return writer->maxSegments;
}

static inline int _writeHeader(CafWriter *writer)
{
	CoredumpCafHeader header;
	union {
		CoredumpCafSegmentTable table;
		unsigned char digest[32];
	} output;
	CafWriterSegment *segment;
	Writer *destination;
	SceOff position;
	SceUInt32 index;
	int result;

	SCE_DBG_ASSERT_FUNC(writer != NULL, 329, "_writeHeader");
	if (writer->maxSegments != writer->numSegments)
		return (int)0x80010016;
	destination = writer->destination;
	position = 0;
	result = writerGetDestOffset(destination, &position);
	if (result != 0)
		return result;
	result = writerSetDestOffset(destination, 0);
	if (result != 0)
		return result;
	result = oalCafHeaderInit(writer->context, writer->type);
	if (result != 0)
		return result;
	result = oalCafMakeHeader(&header, writer->maxSegments,
		writer->segments[0].fileOffset,
		(SceUInt64)position - writer->segments[0].fileOffset, writer->type);
	if (result != 0)
		return result;
	result = writerWrite(destination, &header, sizeof(header));
	if (result != 0)
		return result;
	result = oalCafHeaderTransform(writer->context, &header, sizeof(header));
	if (result != 0)
		return result;
	for (index = 0; index < writer->maxSegments; index++) {
		segment = &writer->segments[index];
		result = oalCafMakeSegmentTable(&output.table, segment->segmentId,
			segment->fileOffset, segment->dataSize, 0, segment->type,
			&segment->iv);
		if (result != 0)
			return result;
		result = writerWrite(destination, &output.table, sizeof(output.table));
		if (result != 0)
			return result;
		result = oalCafHeaderTransform(writer->context, &output.table,
			sizeof(output.table));
		if (result != 0)
			return result;
	}
	for (index = 0; index < writer->maxSegments; index++) {
		segment = &writer->segments[index];
		segment->hash.sectionId = index;
		result = writerWrite(destination, &segment->hash, sizeof(segment->hash));
		if (result != 0)
			return result;
		result = oalCafHeaderTransform(writer->context, &segment->hash,
			sizeof(segment->hash));
		if (result != 0)
			return result;
	}
	result = oalCafHeaderFinal(writer->context, output.digest);
	if (result != 0)
		return result;
	return writerWrite(destination, output.digest, sizeof(output.digest));
}

int cafWriterFinal(CafWriter *writer)
{
	int result, finalResult;

	SCE_DBG_ASSERT(writer != NULL, 201);
	result = _writeHeader(writer);
	finalResult = oalCafContextDestroy(writer->context);
	if (finalResult != 0)
		result = finalResult;
	memMgrFreeForWriter(writer->transformBuffer);
	finalResult = writerFinal(&writer->writer);
	if (finalResult != 0)
		result = finalResult;
	return result;
}
