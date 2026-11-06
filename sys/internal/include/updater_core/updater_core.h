/*
 * The system updater's core library (Sony's internal/usermode/src/updater_core): reads and
 * verifies a PUP, extracts its segments and writes the system packages, through
 * SceSblSsUpdateMgr. cui_setupper, psp2swu and safemode link it.
 *
 * The functions are Sony's sceUpdater* API (their names from the 0.990 and 0.995 builds'
 * error messages; sceUpdaterGetProgressRate, GetEstimateProcessTime, GetSegmentAnalysisSize,
 * GetUpdateModeState, UpdateModeSetLastError, PowerLock, PowerUnlock and SetProgressCallback
 * are ours, as are the error codes' names).
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
#define SCE_UPDATER_ERROR_NO_MEMORY		0x80180001
#define SCE_UPDATER_ERROR_INVALID_STATE		0x80180002
#define SCE_UPDATER_ERROR_INVALID_ARGUMENT	0x80180006
#define SCE_UPDATER_ERROR_SEGMENT_NOT_FOUND	0x80180007
#define SCE_UPDATER_ERROR_ABORTED		0x80180008
#define SCE_UPDATER_ERROR_POWER_STATE		0x80180009
#define SCE_UPDATER_ERROR_ANALYSIS_STATE	0x8018000a
#define SCE_UPDATER_ERROR_CHANNEL_FULL		0x8018000b
#define SCE_UPDATER_ERROR_CHANNEL_EMPTY		0x8018000c
#define SCE_UPDATER_ERROR_LOG_OPEN		0x80180011
#define SCE_UPDATER_ERROR_PUP_OPEN		0x80180012
#define SCE_UPDATER_ERROR_LOG_CLOSE		0x80180021
#define SCE_UPDATER_ERROR_PUP_CLOSE		0x80180022
#define SCE_UPDATER_ERROR_PUP_READ		0x80180032
#define SCE_UPDATER_ERROR_LOG_WRITE		0x80180041
#define SCE_UPDATER_ERROR_PUP_SEEK		0x80180052
#define SCE_UPDATER_ERROR_PUP_NOT_FOUND		0x8018010a
#define SCE_UPDATER_ERROR_PUP_AUTH		0x8018010d
#define SCE_UPDATER_ERROR_PUP_NO_FILE		0x8018010f

/* the PUP segment that holds the updater (psp2swu.self) */
#define SCE_UPDATER_SEGMENT_ID_UPDATER	0x200

/* sceUpdaterGetUpdateMode / sceUpdaterSetUpdateMode */
#define SCE_UPDATER_UPDATE_MODE_NONE	0xff
#define SCE_UPDATER_UPDATE_MODE_UPDATE	0x30

/*
 * One side of sceUpdaterCopyFile: the file and how to reach it. A side whose open is NULL gets
 * the core's own, on the file system (sceIoOpen and the rest, arg pointing at the descriptor).
 */
typedef struct SceUpdaterCopyFileTarget {
	void *arg;
	const char *path;
	int (*open)(void *arg, const char *path);
	int (*close)(void *arg);
	SceInt32 (*getSize)(void *arg);
	SceSSize (*read)(void *arg, void *buf, SceSize size);
	SceSSize (*write)(void *arg, const void *buf, SceSize size);
} SceUpdaterCopyFileTarget;

/* called after each buffer is read: the file's size and the bytes read so far */
typedef void (*SceUpdaterCopyFileCallback)(void *arg, SceUInt64 fileSize, SceUInt64 copied);

/* a field left 0 takes its default (buffer 0x40000 bytes, no callback, the user priority) */
typedef struct SceUpdaterCopyFileOption {
	void *arg;
	SceInt32 bufferSize;
	SceUpdaterCopyFileCallback callback;
	SceInt32 threadPriority;
	SceInt32 cpuAffinityMask;
} SceUpdaterCopyFileOption;

/* a segment analysis (sceUpdaterCreateSegmentAnalysisHandler) */
typedef void *SceUpdaterSegmentAnalysisHandler;

/* a piece of a PUP segment that sceUpdaterIsSegmentAvailable looks at */
typedef struct SceUpdaterSegmentData {
	SceUpdaterSegmentAnalysisHandler handler;
	const void *data;
	SceUInt64 size;
} SceUpdaterSegmentData;

/* sceUpdaterIsSegmentAvailable's result (the names ours) */
#define SCE_UPDATER_SEGMENT_NOT_AVAILABLE	1
#define SCE_UPDATER_SEGMENT_AVAILABLE		2

/* what sceUpdaterGetPupInformation reads from a PUP's header */
typedef struct SceUpdaterPupInformation {
	SceUInt32 magicVersion;		/* the magic's last byte */
	SceUInt32 reserved;
	SceUInt64 version;		/* the system version it installs */
	SceUInt64 fileSize;		/* the header's offset and size of the file */
	SceUInt8 attribute[4];
	SceUInt32 platform;
	SceUInt32 formatPolicy;
	SceUInt32 supportList;
	SceUInt32 baseVersion;		/* the version a differential PUP applies to */
	SceUInt32 baseBuildNo;
} SceUpdaterPupInformation;

/* the console, for sceUpdaterGetDeviceInfo (the names ours) */
#define SCE_UPDATER_DEVICE_TYPE_DEVELOPMENT	1
#define SCE_UPDATER_DEVICE_TYPE_CEX		2

typedef struct SceUpdaterDeviceInfo {
	SceUInt32 systemVersion;	/* the installed system's */
	SceUInt32 reserved;
	SceUInt32 qaPupOnly;		/* only a QA-approved PUP may be installed */
	SceUInt32 deviceType;
} SceUpdaterDeviceInfo;

/* the update's progress (0x40 bytes), which sceUpdaterSetProgressCallback's callback gets */
typedef struct SceUpdaterProgress SceUpdaterProgress;
typedef void (*SceUpdaterProgressCallback)(int type, int reserved, SceUpdaterProgress progress);

int sceUpdaterInitialize(const SceUpdaterMemoryInfo *memoryInfo);
int sceUpdaterFinalize(int reboot);
int sceUpdaterGetProgressRate(void);
int sceUpdaterUpdateAllSpackage(const char *fileName);
int sceUpdaterInspectAllSpackage(const char *fileName);
int sceUpdaterGetSpackageHeader(void *header, int type);
int sceUpdaterGetProgress(SceUpdaterProgress *progress);
int sceUpdaterGetEstimateProcessTime(const char *fileName, int *time);
int sceUpdaterVerifyPup(const char *fileName);
int sceUpdaterVerifyPupSegment(const char *fileName, SceUInt64 segmentId);
int sceUpdaterParse(const char *fileName, SceUInt64 segmentId, void *pData, SceUInt64 size,
		    SceUInt64 offset);
int sceUpdaterGetSegmentSize(const char *fileName, SceUInt64 segmentId, SceUInt64 *size);
int sceUpdaterIsApplicablePup(const char *fileName);
int sceUpdaterCheckAcceptableSpkgType(const char *fileName, const int *types, int count,
				      int *result);
int sceUpdaterGetPupInformation(const char *fileName, SceUpdaterPupInformation *information);
int sceUpdaterNetworkPreCheckVersion(SceUInt32 version, int *result);
int sceUpdaterNetworkCheckBaseVersion(SceUInt32 version, int *result);
int sceUpdaterCheckNewerThanSystemVersion(SceUInt32 version, int *result);
SceUInt64 sceUpdaterGetSegmentAnalysisSize(void);
int sceUpdaterCreateSegmentAnalysisHandler(SceUInt64 segmentId,
					   SceUpdaterSegmentAnalysisHandler *handler);
int sceUpdaterDestroySegmentAnalysisHandler(SceUpdaterSegmentAnalysisHandler handler);
int sceUpdaterIsSegmentAvailable(SceUpdaterSegmentData *data, int *result);
int sceUpdaterGetDeviceInfo(SceUpdaterDeviceInfo *deviceInfo);
int sceUpdaterGetUpdateModeState(void);
int sceUpdaterGetUpdateMode(SceUInt8 *mode);
int sceUpdaterSetUpdateMode(SceUInt8 mode);
int sceUpdaterUpdateModeSetLastError(int error);
int sceUpdaterUpdateModeGetLastError(int *error);
const char *sceUpdaterGetWorkingDirectory(void);
int sceUpdaterLogStart(void);
int sceUpdaterLogEnd(void);
int sceUpdaterPartitionSetUp(void);
int sceUpdaterCleanUp(void);
const char *sceUpdaterGetPupPath(void);
int sceUpdaterPowerLock(void);
int sceUpdaterPowerUnlock(void);
void sceUpdaterSetProgressCallback(SceUpdaterProgressCallback callback, SceUInt32 interval);
int sceUpdaterCopyFile(SceUpdaterCopyFileTarget *src, SceUpdaterCopyFileTarget *dst,
		       SceUpdaterCopyFileOption *option);

#ifdef __cplusplus
}
#endif

#endif
