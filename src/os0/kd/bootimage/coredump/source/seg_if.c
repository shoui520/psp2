/* Segment interface dispatch and size queries. */

#include "seg_internal.h"
#include "writer_internal.h"
#include <elf.h>
#include <sce/debug.h>

extern Writer *currentWriter;

int segInit(SegIf *segment)
{
	SCE_DBG_ASSERT(segment != NULL, 28);
	return segment->methods->init(segment);
}

int segCalcSize(SegIf *segment)
{
	SCE_DBG_ASSERT(segment != NULL, 38);
	return segment->methods->calcSize(segment);
}

void segGetSize(SegIf *segment, SceUInt32 *numPhdr, SceSize *dataSize)
{
	SCE_DBG_ASSERT(segment != NULL && numPhdr != NULL && dataSize != NULL, 52);
	*numPhdr = segment->numPhdr;
	*dataSize = segment->dataSize;
}

int segWritePhdr(SegIf *segment, SceOff fileOffset)
{
	SCE_DBG_ASSERT(segment != NULL, 67);
	return segment->methods->writePhdr(segment, fileOffset);
}

int segWriteSegs(SegIf *segment)
{
	SCE_DBG_ASSERT(segment != NULL, 78);
	return segment->methods->writeSegs(segment);
}

int segInitBase(SegIf *segment)
{
	SCE_DBG_ASSERT(segment != NULL, 89);
	segment->numPhdr = 0;
	segment->dataSize = 0;
	return 0;
}

int segWritePtNoteHdr(SegIf *segment, SceOff fileOffset)
{
	Elf32_Phdr header;
	int result;

	SCE_DBG_ASSERT(segment != NULL, 106);
	result = segment->numPhdr;
	if (result != 0) {
		utilsElf32MakePtnote(&header, (SceUInt32)fileOffset,
			segment->dataSize);
		result = writerWrite(currentWriter, &header, sizeof(header));
	}
	return result;
}

int segWritePaddingData(SegIf *segment, SceSize writtenSize)
{
	int paddingSize;

	SCE_DBG_ASSERT(segment != NULL, 126);
	paddingSize = segment->dataSize - writtenSize;
	if (paddingSize <= 0)
		return 0;
	return writerWritePaddingData(currentWriter, paddingSize);
}
