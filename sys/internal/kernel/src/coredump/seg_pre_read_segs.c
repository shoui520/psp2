/*
 * Pre-read segments: a writer that collects program headers and segment data
 * in a memory buffer, and a segment that writes them out of it. Nothing in
 * this build registers either: no segment table names the segment methods.
 */

#include <coredump/elf.h>
#include <string.h>
#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct PreReadSegsWriter {
	Writer writer;
	void *segsBuffer;
	SceSize segsBufferSize;
	SceSize segsSize;
	int overflow;
} PreReadSegsWriter;

_Static_assert(sizeof(PreReadSegsWriter) == 32, "pre-read writer size");

extern Writer *currentWriter;

static int _writeDataToDest(Writer *writer, const void *data, SceSize size,
	SceSize *written);
static int _setDestOffset(Writer *writer, SceOff offset);
static int _getDestOffset(Writer *writer, SceOff *offset);
static int _isSetDestOffsetSupported(Writer *writer);

const WriterOperations preReadSegsWriterOperations = {
	.writeDataToDest = _writeDataToDest,
	.setDestOffset = _setDestOffset,
	.getDestOffset = _getDestOffset,
	.isSetDestOffsetSupported = _isSetDestOffsetSupported,
};

SceUInt32 numPreReadPhdr;
PreReadSegsWriter *preReadWriter;

int _calcSize(SegIf *segment)
{
	SceUInt32 numPhdr;
	SceSize dataSize;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 42, "_calcSize");
	segment->writtenSize = 0;
	numPhdr = 0;
	dataSize = 0;
	if (preReadWriter != NULL) {
		numPhdr = numPreReadPhdr;
		dataSize = preReadWriter->segsSize - numPhdr * sizeof(Elf32_Phdr);
	}
	segment->dataSize = dataSize;
	segment->numPhdr = numPhdr;
	return 0;
}

int _writeSegs(SegIf *segment)
{
	const char *data;
	SceSize padding;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 63, "_writeSegs");
	result = 0;
	if (segment->numPhdr != 0) {
		if (preReadWriter != NULL) {
			data = (const char *)preReadWriter->segsBuffer +
				numPreReadPhdr * sizeof(Elf32_Phdr);
			if (preReadWriter->overflow == 0) {
				result = writerWrite(currentWriter, data,
					segment->dataSize);
				if (result == 0)
					segment->writtenSize = segment->dataSize;
			} else {
				result = writerWrite(currentWriter, data,
					preReadWriter->segsBufferSize);
				if (result == 0) {
					segment->writtenSize = preReadWriter->segsBufferSize;
					if (segment->writtenSize < segment->dataSize)
						padding = segment->dataSize - segment->writtenSize;
					else
						padding = 0;
					result = writerWritePaddingData(currentWriter, padding);
					if (result == 0)
						segment->writtenSize += padding;
				}
			}
		}
	}
	return result;
}

int _writePhdr(SegIf *segment, SceOff fileOffset)
{
	Elf32_Phdr phdr;
	SceUInt32 i;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 99, "_writePhdr");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	if (preReadWriter == NULL)
		return 0;
	for (i = 0; i < numPreReadPhdr; i++) {
		phdr = ((const Elf32_Phdr *)preReadWriter->segsBuffer)[i];
		if (phdr.p_filesz == 0)
			continue;
		phdr.p_offset += fileOffset;
		result = writerWrite(currentWriter, &phdr, sizeof(phdr));
		if (result != 0)
			return result;
	}
	return 0;
}

static int _writeDataToDest(Writer *base, const void *data, SceSize size,
	SceSize *written)
{
	PreReadSegsWriter *writer = (PreReadSegsWriter *)base;
	SceSize rest;

	SCE_DBG_ASSERT(base != NULL, 217);
	rest = writer->segsBufferSize - writer->segsSize;
	if (writer->segsBufferSize >= writer->segsSize && rest != 0) {
		if (rest < size) {
			memcpy((char *)writer->segsBuffer + writer->segsSize, data, rest);
			*written = size;
			writer->overflow = 1;
		} else {
			memcpy((char *)writer->segsBuffer + writer->segsSize, data, size);
			*written = size;
		}
	} else {
		*written = size;
		writer->overflow = 1;
	}
	writer->segsSize += size;
	return 0;
}

static int _setDestOffset(Writer *writer, SceOff offset)
{
	SCE_DBG_ASSERT(writer != NULL, 255);
	(void)offset;
	return (int)0x80010086;
}

static int _getDestOffset(Writer *writer, SceOff *offset)
{
	SCE_DBG_ASSERT(writer != NULL, 270);
	(void)offset;
	return (int)0x80010086;
}

static int _isSetDestOffsetSupported(Writer *writer)
{
	SCE_DBG_ASSERT(writer != NULL, 284);
	return 0;
}
