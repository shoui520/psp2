/* GPU_ACT_INFO note. */

#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

extern Writer *currentWriter;
extern SceUInt32 oalGetGpuRecoveryCount(void);
extern SceSize oalGetGpuActivitySize(void);
extern void oalGetGpuActivity(void *buffer, SceSize size);

typedef struct CoredumpGpuActivityNote {
	CoredumpNoteHeader header;
	SceUInt32 recoveryCount;
	SceSize activitySize;
} CoredumpGpuActivityNote;

_Static_assert(sizeof(CoredumpGpuActivityNote) == 40,
	"GPU activity note size");

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod gpuActivityMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf gpuActivitySegment;

static int _calcSize(SegIf *segment)
{
	SceSize size;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 51, "_calcSize");
	segment->writtenSize = 0;
	size = oalGetGpuActivitySize();
	size = size < 4096 ? size : 4096;
	segment->numPhdr = 1;
	segment->dataSize = (size + sizeof(CoredumpGpuActivityNote) + 3) & ~3u;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	CoredumpGpuActivityNote note;
	unsigned char data[4096];
	SceSize size, padding;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 70, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	utilsMakeNoteHeader(&note.header, "GPU_ACT_INFO", 0x2001, 8,
		segment->dataSize);
	note.recoveryCount = oalGetGpuRecoveryCount();
	note.activitySize = oalGetGpuActivitySize();
	result = writerWrite(currentWriter, &note, sizeof(note));
	if (result != 0)
		return result;
	segment->writtenSize += sizeof(note);
	size = note.activitySize;
	if (size >= sizeof(data))
		size = sizeof(data);
	oalGetGpuActivity(data, size);
	result = writerWrite(currentWriter, data, size);
	if (result != 0)
		return result;
	segment->writtenSize += size;
	padding = ((segment->writtenSize + 3) & ~3U) - segment->writtenSize;
	result = writerWritePaddingData(currentWriter, padding);
	if (result == 0)
		segment->writtenSize += padding;
	return result;
}
