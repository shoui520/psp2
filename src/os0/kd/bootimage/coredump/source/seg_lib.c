/* LIBRARY_INFO note. */

#include <sce/debug.h>
#include <sce/error.h>
#include <psp2kern/kernel/modulemgr.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern int oalGetNumOfLibs(SceUID pid);
extern void oalGetLibIdList(SceUID pid, SceUID *ids, SceSize capacity);
extern int oalGetLibInfo(SceUID pid, SceUID libraryId, CoredumpLibraryInfo *info);
extern unsigned int oalGetLibNumFuncAndVar(SceUID pid,
	const CoredumpLibraryInfo *info);
extern void oalGetLibExportInfo(SceUID pid, SceUID libraryId,
	SceKernelModuleExportEntry *entries, SceSize capacity, SceSize skip);
extern unsigned int oalGetLibNumClient(SceUID pid,
	const CoredumpLibraryInfo *info);
extern void oalGetLibClientInfo(SceUID pid, SceUID libraryId,
	SceUID *moduleIds, SceSize capacity);
extern void oalGetLibName(SceUID pid, SceUID libraryId, char *name,
	SceSize *nameSize);
extern void *memMgrLocalAlloc(SceSize size);
extern void *memMgrLocalAllAlloc(SceSize *size);
extern int memMgrLocalFree(void *allocation);
extern int writerWritePaddingData(Writer *writer, SceSize size);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod libMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

static int _calcSize(SegIf *segment)
{
	SceSize nameSize;
	CoredumpLibraryInfo info;
	char name[256];
	SceUID *ids;
	SceUID *library;
	unsigned int count, i, namesSize, numClients, numExports;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 69, "_calcSize");
	segment->writtenSize = 0;
	count = oalGetNumOfLibs(coredumpRun.pid);
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	library = ids;
	namesSize = 0;
	numClients = 0;
	numExports = 0;
	i = 0;
	oalGetLibIdList(coredumpRun.pid, ids, count);
	do {
		++i;
		oalGetLibInfo(coredumpRun.pid, *library, &info);
		numExports += oalGetLibNumFuncAndVar(coredumpRun.pid, &info);
		numClients += oalGetLibNumClient(coredumpRun.pid, &info);
		oalGetLibName(coredumpRun.pid, *library++, name, &nameSize);
		namesSize += (nameSize + 3) & ~3u;
	} while (i != count);
	memMgrLocalFree(ids);
	segment->numPhdr = 1;
	segment->dataSize = namesSize + sizeof(CoredumpLibraryInfo) * (i + 1) +
		2156 + numExports * sizeof(SceKernelModuleExportEntry) +
		(i + numClients) * sizeof(SceUID);
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	unsigned int count;
	SceSize exportSize, nameSize;
	CoredumpLibraryInfo info;
	CoredumpNoteHeader header;
	char name[256];
	SceUID *ids, *clients;
	SceKernelModuleExportEntry *exports;
	unsigned int i, remaining, capacity, chunk, skip, numClients;
	SceSize padding;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 129, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	count = oalGetNumOfLibs(coredumpRun.pid);
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	utilsMakeNoteHeader(&header, "LIBRARY_INFO", 0x1006, 6,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(count);
	oalGetLibIdList(coredumpRun.pid, ids, count);
	for (i = 0; i < count; ++i) {
		oalGetLibInfo(coredumpRun.pid, ids[i], &info);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(info);
		remaining = oalGetLibNumFuncAndVar(coredumpRun.pid, &info);
		exportSize = 0;
		exports = memMgrLocalAllAlloc(&exportSize);
		if (exports == NULL) {
			result = SCE_ERROR_ERRNO_ENOMEM;
			goto out;
		}
		if (exportSize == 0) {
			result = SCE_ERROR_ERRNO_ENOMEM;
			goto freeExports;
		}
		capacity = exportSize / sizeof(*exports);
		skip = 0;
		while (remaining != 0) {
			chunk = remaining < capacity ? remaining : capacity;
			oalGetLibExportInfo(coredumpRun.pid, ids[i], exports,
				chunk, skip);
			result = writerWrite(currentWriter, exports,
				chunk * sizeof(*exports));
			if (result != 0)
				goto freeExports;
			remaining -= chunk;
			skip += chunk;
			segment->writtenSize += chunk * sizeof(*exports);
		}
		memMgrLocalFree(exports);
		numClients = oalGetLibNumClient(coredumpRun.pid, &info);
		clients = memMgrLocalAlloc(numClients * sizeof(*clients));
		if (clients == NULL) {
			result = SCE_ERROR_ERRNO_ENOMEM;
			goto out;
		}
		oalGetLibClientInfo(coredumpRun.pid, ids[i], clients, numClients);
		result = writerWrite(currentWriter, clients,
			numClients * sizeof(*clients));
		if (result != 0)
			goto freeClients;
		segment->writtenSize += numClients * sizeof(*clients);
		oalGetLibName(coredumpRun.pid, ids[i], name, &nameSize);
		result = writerWrite(currentWriter, &nameSize, sizeof(nameSize));
		if (result != 0)
			goto freeClients;
		segment->writtenSize += sizeof(nameSize);
		result = writerWrite(currentWriter, name, nameSize);
		if (result != 0)
			goto freeClients;
		padding = ((nameSize + 3) & ~3u) - nameSize;
		segment->writtenSize += nameSize;
		result = writerWritePaddingData(currentWriter, padding);
		if (result != 0)
			goto freeClients;
		segment->writtenSize += padding;
		memMgrLocalFree(clients);
	}
	if (segment->writtenSize < segment->dataSize) {
		writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		segment->writtenSize = segment->dataSize;
	}
	goto out;
freeExports:
	memMgrLocalFree(exports);
	goto out;
freeClients:
	memMgrLocalFree(clients);
out:
	memMgrLocalFree(ids);
	return result;
}
