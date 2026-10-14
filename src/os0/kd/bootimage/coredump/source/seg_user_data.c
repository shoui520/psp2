/* USER_INFO note and fixed-capacity handler data. */

#include <string.h>
#include <sce/error.h>
#include <sce/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct CoredumpUserDataNote {
	CoredumpNoteHeader header;
	SceUInt32 reserved;
	SceSize usedSize;
} CoredumpUserDataNote;

_Static_assert(sizeof(CoredumpUserDataNote) == 40, "user data note size");

extern Writer *currentWriter;
extern int oalHasCoredumpHandler(SceUID pid);
extern CoredumpRunContext coredumpRun;
extern void *memMgrLocalAlloc(SceSize size);
extern int memMgrLocalFree(void *allocation);
extern int oalUnlinkNormalSyscall(SceUID pid);
extern SceUID oalCallCoredumpHandler(SceUID pid);
extern int oalWaitThreadEnd(SceUID threadId, SceUInt timeout);
extern int oalIsCoredumpHandlerAtSyscall(SceUID pid, SceUID threadId);
extern int oalDeleteThread(SceUID threadId);
extern int oalPrintf(const char *fmt, ...);
extern int userDataSetBuffer(void *buffer, SceSize capacity);
extern int userDataGetUsedSize(void);
extern int userDataClearBuffer(void);

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod userDataMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf userDataSegment;

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 49, "_calcSize");
	segment->writtenSize = 0;
	if (coredumpRun.triggerReason == -2 || coredumpRun.triggerReason == -1) {
		segment->numPhdr = 0;
		segment->dataSize = 0;
		return 0;
	}
	if (oalHasCoredumpHandler(coredumpRun.pid) != 0) {
		segment->numPhdr = 1;
		segment->dataSize = sizeof(CoredumpUserDataNote) + 0x4000;
	} else {
		segment->numPhdr = 0;
		segment->dataSize = 0;
	}
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	CoredumpUserDataNote note;
	void *buffer;
	SceUID threadId;
	int result, usedSize;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 84, "_writeSegs");
	result = segment->numPhdr;
	if (result == 0)
		return result;
	note = (CoredumpUserDataNote){0};
	buffer = memMgrLocalAlloc(16384);
	if (buffer == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	memset(buffer, 0, 16384);
	userDataSetBuffer(buffer, 16384);
	result = oalUnlinkNormalSyscall(coredumpRun.pid);
	if (result != 0)
		goto out;
	threadId = oalCallCoredumpHandler(coredumpRun.pid);
	if (threadId <= 0) {
		oalPrintf("Calling the coredump handler failed.\n");
	} else {
		result = oalWaitThreadEnd(threadId, 3000000);
		if (result != 0) {
			oalPrintf("Coredump handler timed out.\n");
			if (oalIsCoredumpHandlerAtSyscall(coredumpRun.pid,
				threadId))
				oalPrintf("You can not use system calls in the coredump handler.\n");
			oalDeleteThread(threadId);
		}
	}
	utilsMakeNoteHeader(&note.header, "USER_INFO", 0x1022, 6,
		segment->dataSize);
	usedSize = userDataGetUsedSize();
	userDataClearBuffer();
	if (usedSize >= 0) {
		note.usedSize = usedSize;
		result = writerWrite(currentWriter, &note, sizeof(note));
		if (result != 0)
			goto out;
		segment->writtenSize += sizeof(note);
		result = writerWrite(currentWriter, buffer, 16384);
		if (result == 0)
			segment->writtenSize += 16384;
	}
out:
	memMgrLocalFree(buffer);
	return result;
}
