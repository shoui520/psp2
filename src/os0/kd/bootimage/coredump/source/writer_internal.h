/* Complete private writer records shared by SceCoredump's writers. */

#pragma once

#include <sce/zlib.h>
#include "oal_internal.h"

typedef struct Writer Writer;

typedef struct WriterOperations {
	int (*writeDataToDest)(Writer *writer, const void *data, SceSize size,
		SceSize *written);
	int (*setDestOffset)(Writer *writer, SceOff offset);
	int (*getDestOffset)(Writer *writer, SceOff *offset);
	int (*isSetDestOffsetSupported)(Writer *writer);
} WriterOperations;

struct Writer {
	const WriterOperations *operations;
	void *buffer;
	SceSize bufferSize;
	SceSize bufferedSize;
};

typedef SceUID (*FileWriterOpenCallback)(void);

typedef struct FileWriter {
	Writer writer;
	SceUID fd;
	FileWriterOpenCallback openFile;
	SceSize reservedFileSize;
	SceSize writtenSize;
} FileWriter;

typedef struct CafWriterSegment {
	SceUInt64 segmentId;
	SceOff fileOffset;
	SceSize dataSize;
	SceUInt32 type;
	CoredumpCafIv iv;
	CoredumpCafSectionHash hash;
} CafWriterSegment;

typedef struct CafWriter {
	Writer writer;
	SceUID context;
	SceUInt32 type;
	Writer *destination;
	void *transformBuffer;
	SceSize transformBufferSize;
	SceUInt32 numSegments;
	SceUInt32 maxSegments;
	CafWriterSegment segments[2];
} CafWriter;

typedef struct GzipWriter {
	Writer writer;
	SceZlibStream stream;
	Writer *destination;
	void *outputBuffer;
	SceSize outputBufferSize;
	int (*getExitFlag)(SceSize processedSize);
} GzipWriter;

_Static_assert(sizeof(Writer) == 16, "writer size");
_Static_assert(sizeof(FileWriter) == 32, "file writer size");
_Static_assert(sizeof(CafWriterSegment) == 88, "CAF writer segment size");
_Static_assert(offsetof(CafWriter, segments) == 48, "CAF segments");
_Static_assert(sizeof(CafWriter) == 224, "CAF writer size");
_Static_assert(sizeof(GzipWriter) == 88, "gzip writer size");

int fileWriterInit(FileWriter *writer, FileWriterOpenCallback openFile,
	SceSize bufferSize, SceSize reservedFileSize);
int fileWriterFinal(FileWriter *writer);
int cafWriterInit(CafWriter *writer, Writer *destination, SceUInt32 type,
	SceSize bufferSize, SceSize transformBufferSize, SceUInt32 maxSegments);
int cafWriterBeginSegment(CafWriter *writer, SceUInt32 type);
int cafWriterFinal(CafWriter *writer);
int gzipWriterInit(GzipWriter *writer, Writer *destination,
	SceSize bufferSize, SceSize outputBufferSize,
	int (*getExitFlag)(SceSize processedSize));
int gzipWriterFinal(GzipWriter *writer);
int writerWrite(Writer *writer, const void *data, SceSize size);
int writerInit(Writer *writer, SceSize bufferSize);
int writerFinal(Writer *writer);
int writerFlush(Writer *writer);
int writerWritePaddingData(Writer *writer, SceSize size);
int writerSetDestOffset(Writer *writer, SceOff offset);
int writerGetDestOffset(Writer *writer, SceOff *offset);
