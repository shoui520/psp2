/* APP_INFO note. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/debug.h>
#include "writer_internal.h"
#include "seg_internal.h"

typedef struct AppNoteInfo {
	SceUInt32 reservedHeader;
	char titleid[10];
	char appName[128];
	char appVersion[6];
	SceUInt32 sdkVersion;
	unsigned char reservedTail[104];
} AppNoteInfo;

typedef struct AppNote {
	CoredumpNoteHeader header;
	AppNoteInfo info;
} AppNote;

_Static_assert(sizeof(AppNoteInfo) == 256, "app note information size");
_Static_assert(sizeof(AppNote) == 288, "app note size");
_Static_assert(offsetof(AppNoteInfo, sdkVersion) == 148,
	"app note SDK version");

extern Writer *currentWriter;

static int _calcSize(SegIf *segment);
static int _writeSegs(SegIf *segment);

const SegIfMethod appMethods = {
	segInitBase,
	_calcSize,
	segWritePtNoteHdr,
	_writeSegs,
};

SegIf appSegment;

static int _calcSize(SegIf *segment)
{
	SCE_DBG_ASSERT_FUNC(segment != NULL, 54, "_calcSize");
	segment->numPhdr = 1;
	segment->dataSize = sizeof(AppNote);
	segment->writtenSize = 0;
	return 0;
}

static int _writeSegs(SegIf *segment)
{
	AppNote note;
	SceUID pid;
	int result;

	SCE_DBG_ASSERT_FUNC(segment != NULL, 70, "_writeSegs");
	result = segment->numPhdr;
	if (result != 0) {
		utilsMakeNoteHeader(&note.header, "APP_INFO", 4124, 6,
			segment->dataSize);
		pid = coredumpRun.pid;
		memset(&note.info, 0, sizeof(note.info));
		if (coredumpRun.trigger.titleid != NULL && coredumpRun.trigger.titleid_len != 0) {
#if FW_DEBUG
			__builtin___memcpy_chk(note.info.titleid,
				coredumpRun.trigger.titleid, coredumpRun.trigger.titleid_len,
				sizeof(note.info) - offsetof(AppNoteInfo, titleid));
#else
			memcpy(note.info.titleid, coredumpRun.trigger.titleid,
				coredumpRun.trigger.titleid_len);
#endif
		}
		if (coredumpRun.trigger.app_name != NULL && coredumpRun.trigger.app_name_len != 0) {
#if FW_DEBUG
			__builtin___memcpy_chk(note.info.appName,
				coredumpRun.trigger.app_name, coredumpRun.trigger.app_name_len,
				sizeof(note.info) - offsetof(AppNoteInfo, appName));
#else
			memcpy(note.info.appName, coredumpRun.trigger.app_name,
				coredumpRun.trigger.app_name_len);
#endif
		}
		snprintf(note.info.appVersion, sizeof(note.info.appVersion),
			"%02d.%02d", coredumpRun.trigger.appVersion / 100,
			coredumpRun.trigger.appVersion % 100);
		oalGetAppBuildSDKVersion(pid, &note.info.sdkVersion);
		result = writerWrite(currentWriter, &note, segment->dataSize);
		if (result == 0) {
			segment->writtenSize += segment->dataSize;
			return 0;
		}
	}
	return result;
}
