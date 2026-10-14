/* FILE_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct FileSizeContext {
	SceUInt32 numFiles;
	SceSize pathsSize;
} FileSizeContext;

_Static_assert(sizeof(FileSizeContext) == 8, "file size context");

extern Writer *currentWriter;
extern void *memMgrLocalAllAlloc(SceSize *size);
extern int memMgrLocalFree(void *allocation);
extern int oalEachFileInfo(SceUID pid,
	int (*callback)(CoredumpFileInfo *info, SceSize resolvedPathSize,
		const char *resolvedPath, SceSize resolvedPathPadding,
		SceSize openPathSize, const char *openPath, SceSize openPathPadding,
		void *context),
	void *context, void *allocation, SceSize allocationSize);
static int _writeCallback(CoredumpFileInfo *info, SceSize resolvedPathSize,
	const char *resolvedPath, SceSize resolvedPathPadding,
	SceSize openPathSize, const char *openPath, SceSize openPathPadding,
	void *context);
extern int fileAccumulateSize(CoredumpFileInfo *info, SceSize resolvedPathSize,
	const char *resolvedPath, SceSize resolvedPathPadding,
	SceSize openPathSize, const char *openPath, SceSize openPathPadding,
	void *context);
extern int writerWritePaddingData(Writer *writer, SceSize size);

static int _writeSegs(SegIf *base);
static int _calcSize(SegIf *base);

const SegIfMethod fileMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

FileSegIf fileSegment;

static int _calcSize(SegIf *base)
{
	FileSegIf *segment = (FileSegIf *)base;
	SceSize capacity;
	FileSizeContext context;
	void *allocation;

	SCE_DBG_ASSERT_FUNC(base != NULL, 80, "_calcSize");
	base->writtenSize = 0;
	base->numPhdr = 0;
	base->dataSize = 0;
	allocation = memMgrLocalAllAlloc(&capacity);
	if (allocation != NULL) {
		context.numFiles = 0;
		context.pathsSize = 0;
		if (oalEachFileInfo(coredumpRun.pid, fileAccumulateSize,
		    &context, allocation, capacity) == 0 && context.numFiles != 0) {
			segment->numFiles = context.numFiles;
			base->numPhdr = 1;
			base->dataSize = context.pathsSize +
				sizeof(CoredumpNoteHeader) + sizeof(context.numFiles) +
				context.numFiles * sizeof(CoredumpFileInfo);
		}
		memMgrLocalFree(allocation);
	}
	return 0;
}

static int _writeSegs(SegIf *base)
{
	FileSegIf *segment = (FileSegIf *)base;
	SceSize capacity;
	CoredumpNoteHeader header;
	void *allocation;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 132, "_writeSegs");
	result = segment->base.numPhdr;
	if (result == 0)
		return result;
	segment->base.writtenSize = 0;
	allocation = memMgrLocalAllAlloc(&capacity);
	if (allocation == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	utilsMakeNoteHeader(&header, "FILE_INFO", 0x1009, 5,
		segment->base.dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->base.writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &segment->numFiles,
		sizeof(segment->numFiles));
	if (result != 0)
		goto out;
	segment->base.writtenSize += sizeof(segment->numFiles);
	result = oalEachFileInfo(coredumpRun.pid, _writeCallback,
		&segment->base.writtenSize, allocation, capacity);
out:
	memMgrLocalFree(allocation);
	return result;
}

static int _writeCallback(CoredumpFileInfo *info, SceSize resolvedPathSize,
	const char *resolvedPath, SceSize resolvedPathPadding,
	SceSize openPathSize, const char *openPath, SceSize openPathPadding,
	void *context)
{
	SceSize *writtenSize = context;
	int result;

	result = writerWrite(currentWriter, info, sizeof(*info));
	if (result != 0)
		return result;
	*writtenSize += sizeof(*info);
	result = writerWrite(currentWriter, &resolvedPathSize,
		sizeof(resolvedPathSize));
	if (result != 0)
		return result;
	*writtenSize += sizeof(resolvedPathSize);
	result = writerWrite(currentWriter, resolvedPath, resolvedPathSize);
	if (result != 0)
		return result;
	*writtenSize += resolvedPathSize;
	result = writerWritePaddingData(currentWriter, resolvedPathPadding);
	if (result != 0)
		return result;
	*writtenSize += resolvedPathPadding;
	result = writerWrite(currentWriter, &openPathSize, sizeof(openPathSize));
	if (result != 0)
		return result;
	*writtenSize += sizeof(openPathSize);
	result = writerWrite(currentWriter, openPath, openPathSize);
	if (result != 0)
		return result;
	*writtenSize += openPathSize;
	result = writerWritePaddingData(currentWriter, openPathPadding);
	if (result == 0)
		*writtenSize += openPathPadding;
	return result;
}
