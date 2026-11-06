/*
 * The system updater's core library (Sony's internal/usermode/src/updater_core): reads and
 * verifies a PUP, extracts its segments and writes the system packages, through
 * SceSblSsUpdateMgr. cui_setupper, psp2swu and safemode link it.
 *
 * The functions are Sony's sceUpdater* API (their names from the 0.990 and 0.995 builds'
 * error messages); a function those builds do not name has a name of ours.
 */
#ifndef UPDATER_CORE_UPDATER_CORE_H
#define UPDATER_CORE_UPDATER_CORE_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the allocator the core uses for everything it allocates */
typedef struct SceUpdaterMemoryInfo {
	void *(*updaterCoreAlloc)(SceSize size);
	void (*updaterCoreFree)(void *ptr);
	void *(*updaterCoreMemalign)(SceSize align, SceSize size);
} SceUpdaterMemoryInfo;

/* error codes (the names ours) */
#define SCE_UPDATER_ERROR_INVALID_STATE		0x80180002
#define SCE_UPDATER_ERROR_INVALID_ARGUMENT	0x80180006
#define SCE_UPDATER_ERROR_PUP_NOT_FOUND		0x8018010a
#define SCE_UPDATER_ERROR_PUP_AUTH		0x8018010d
#define SCE_UPDATER_ERROR_PUP_NO_FILE		0x8018010f

/* the PUP segment that holds the updater (psp2swu.self) */
#define SCE_UPDATER_SEGMENT_ID_UPDATER	0x200

/* sceUpdaterGetUpdateMode / sceUpdaterSetUpdateMode */
#define SCE_UPDATER_UPDATE_MODE_NONE	0xff
#define SCE_UPDATER_UPDATE_MODE_UPDATE	0x30

/* one side of sceUpdaterCopyFile */
typedef struct SceUpdaterCopyFileTarget {
	SceUInt32 reserved0;
	const char *path;
	SceUInt32 reserved[5];
} SceUpdaterCopyFileTarget;

typedef void (*SceUpdaterCopyFileCallback)(void *arg, SceInt32 status, SceUInt64 offset,
					   SceUInt64 copied);

typedef struct SceUpdaterCopyFileOption {
	void *arg;
	SceSize bufferSize;
	SceUpdaterCopyFileCallback callback;
	SceUInt32 reserved[2];
} SceUpdaterCopyFileOption;

int sceUpdaterInitialize(const SceUpdaterMemoryInfo *memoryInfo);
int sceUpdaterFinalize(int reboot);
int sceUpdaterParse(const char *fileName, SceUInt64 segmentId, void *pData, SceUInt64 size,
		    SceUInt64 offset);
int sceUpdaterGetSegmentSize(const char *fileName, SceUInt64 segmentId, SceUInt64 *size);
int sceUpdaterIsApplicablePup(const char *fileName);
int sceUpdaterGetUpdateMode(SceUInt8 *mode);
int sceUpdaterSetUpdateMode(SceUInt8 mode);
const char *sceUpdaterGetWorkingDirectory(void);
int sceUpdaterPartitionSetUp(void);
int sceUpdaterCleanUp(void);
const char *sceUpdaterGetPupPath(void);
int sceUpdaterCopyFile(const SceUpdaterCopyFileTarget *src, const SceUpdaterCopyFileTarget *dst,
		       const SceUpdaterCopyFileOption *option);

#ifdef __cplusplus
}
#endif

#endif
