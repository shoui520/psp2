/* MODULE_INFO note. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include <modulemgr/modulemgr_debugger.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern void *memMgrLocalAllAlloc(SceSize *size);
extern int memMgrLocalFree(void *allocation);
extern int oalGetNumOfModulesAndSegments(SceUID pid, int *numModules,
	unsigned int *numSegments, SceKernelModuleInfoForDebugger *modules,
	SceSize capacity);
extern int oalEachModuleInfo(SceUID pid,
	int (*callback)(CoredumpModuleInfo *info,
		SceKernelSegmentInfoForDebugger *segments, unsigned int nSegments,
		SceKernelModuleExidxInfo *exidx, void *context),
	SceKernelModuleInfoForDebugger *modules, SceSize capacity, void *context);
static int _writeCallback(CoredumpModuleInfo *info,
	SceKernelSegmentInfoForDebugger *segments, unsigned int nSegments,
	SceKernelModuleExidxInfo *exidx, void *context);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod moduleMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf moduleSegment;

static int _calcSize(SegIf *segment)
{
	SceSize capacity;
	int numModules;
	unsigned int numSegments;
	SceKernelModuleInfoForDebugger *modules;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 67, "_calcSize");
	segment->writtenSize = 0;
	segment->numPhdr = 0;
	segment->dataSize = 0;
	modules = memMgrLocalAllAlloc(&capacity);
	if (modules != NULL) {
		if (oalGetNumOfModulesAndSegments(coredumpRun.pid, &numModules,
		    &numSegments, modules, capacity) == 0 && numModules != 0) {
			segment->numPhdr = 1;
			segment->dataSize = numModules *
				(sizeof(CoredumpModuleInfo) + sizeof(SceKernelModuleExidxInfo)) +
				numSegments * sizeof(SceKernelSegmentInfoForDebugger) + 396;
		}
		memMgrLocalFree(modules);
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	SceSize capacity;
	int numModules;
	unsigned int numSegments;
	CoredumpNoteHeader header;
	SceKernelModuleInfoForDebugger *modules;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 117, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	modules = memMgrLocalAllAlloc(&capacity);
	if (modules == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	result = oalGetNumOfModulesAndSegments(coredumpRun.pid, &numModules,
		&numSegments, modules, capacity);
	if (result != 0 || numModules == 0)
		goto out;
	utilsMakeNoteHeader(&header, "MODULE_INFO", 0x1005, 1,
		segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &numModules, sizeof(numModules));
	if (result != 0)
		goto out;
	segment->writtenSize += sizeof(numModules);
	result = oalEachModuleInfo(coredumpRun.pid, _writeCallback,
		modules, capacity, &segment->writtenSize);
	if (result != 0)
		goto out;
	if (segment->writtenSize < segment->dataSize) {
		writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		segment->writtenSize = segment->dataSize;
	}
out:
	memMgrLocalFree(modules);
	return result;
}

static int _writeCallback(CoredumpModuleInfo *info,
	SceKernelSegmentInfoForDebugger *segments, unsigned int nSegments,
	SceKernelModuleExidxInfo *exidx, void *context)
{
	SceSize *writtenSize = context;
	unsigned int index;
	int result;

	SCE_DBG_ASSERT_FUNC(info != NULL, 195, "_writeCallback");
	SCE_DBG_ASSERT_FUNC(context != NULL, 196, "_writeCallback");
	result = writerWrite(currentWriter, info, sizeof(*info));
	if (result != 0)
		return result;
	*writtenSize += sizeof(*info);
	for (index = 0; index != nSegments; ++index) {
		result = writerWrite(currentWriter, segments, sizeof(*segments));
		if (result != 0)
			return result;
		segments++;
		*writtenSize += sizeof(*segments);
	}
	result = writerWrite(currentWriter, exidx, sizeof(*exidx));
	if (result == 0)
		*writtenSize += sizeof(*exidx);
	return result;
}
