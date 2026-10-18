/* BUDGET_INFO note: every budget and its physical memory partitions. */

#include <sysmem/debug.h>
#include <kernel/error.h>
#include "writer_internal.h"
#include "seg_internal.h"
#include "oal_internal.h"

extern int oalIsSystemProgram(SceUID pid);
extern int oalGetNumOfBudgets(void);
extern void oalGetBudgetIdList(SceUID *ids, int capacity);
extern void oalGetBudgetInfo(SceUID budgetId, CoredumpBudgetInfo *info,
	CoredumpPhyMemPartInfo *parts, uint32_t *numParts);
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern Writer *currentWriter;

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod budgetMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf budgetSegment;

static int _calcSize(SegIf *segment)
{
	CoredumpBudgetInfo info;
	CoredumpPhyMemPartInfo parts[5];
	SceUInt32 count, totalParts, i;
	uint32_t numParts;
	SceUID *ids;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 58, "_calcSize");
	segment->writtenSize = 0;
	if (oalIsSystemProgram(coredumpRun.pid) == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	count = oalGetNumOfBudgets();
	if (count == 0) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	oalGetBudgetIdList(ids, count);
	totalParts = 0;
	for (i = 0; i < count; i++) {
		SceUID id = ids[i];

		numParts = 5;
		oalGetBudgetInfo(id, &info, parts, &numParts);
		totalParts += numParts;
	}
	segment->numPhdr = 1;
	segment->dataSize = count * sizeof(info) + 1092 +
		totalParts * sizeof(parts[0]);
	memMgrLocalFree(ids);
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	CoredumpNoteHeader header;
	CoredumpBudgetInfo info;
	CoredumpPhyMemPartInfo parts[5];
	SceUInt32 count, i, j;
	uint32_t numParts;
	SceUID *ids;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 112, "_writeSegs");
	if (segment->numPhdr == 0)
		return 0;
	count = oalGetNumOfBudgets();
	ids = memMgrLocalAlloc(count * sizeof(*ids));
	if (ids == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	utilsMakeNoteHeader(&header, "BUDGET_INFO", 0x101F, 6, segment->dataSize);
	result = writerWrite(currentWriter, &header, sizeof(header));
	if (result != 0)
		goto free;
	segment->writtenSize += sizeof(header);
	result = writerWrite(currentWriter, &count, sizeof(count));
	if (result != 0)
		goto free;
	segment->writtenSize += sizeof(count);
	oalGetBudgetIdList(ids, count);
	for (i = 0; i < count; i++) {
		SceUID id = ids[i];

		numParts = 5;
		oalGetBudgetInfo(id, &info, parts, &numParts);
		result = writerWrite(currentWriter, &info, sizeof(info));
		if (result != 0)
			goto free;
		segment->writtenSize += sizeof(info);
		for (j = 0; j < numParts; j++) {
			result = writerWrite(currentWriter, &parts[j], sizeof(parts[j]));
			if (result != 0)
				goto free;
			segment->writtenSize += sizeof(parts[j]);
		}
	}
	if (segment->dataSize > segment->writtenSize) {
		result = writerWritePaddingData(currentWriter,
			segment->dataSize - segment->writtenSize);
		if (result == 0)
			segment->writtenSize = segment->dataSize;
	}
free:
	memMgrLocalFree(ids);
	return result;
}
