/*
 * updater_core's API (Sony's updater_core.cpp, the first of the library's files in the 0.995
 * build's order): the library's state, its error log, and the sceUpdater* functions, each a
 * check of the state and the arguments in front of a method of the class that does the work.
 * The library's operators new and delete use the allocator sceUpdaterInitialize was given.
 */
#include <stdarg.h>
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "updater_memory.h"
#include "updater_progress.h"
#include "parallel_copy_file.h"
#include "partition_manager.h"
#include "pup_controller.h"
#include "segment_analysis.h"
#include "spkg_group_manager.h"
#include "update_utility.h"

#define UPDATER_FILE_ID		0xfce75563

#define UPDATER_LOG_LINE_SIZE	0x800
#define UPDATER_LOG_PATH_SIZE	256
#define UPDATER_LOG_FILE_NAME	"/updater.log"
#define UPDATER_LOG_FILE_MODE	0606

/* the library's state (the names ours) */
enum {
	UPDATER_STATE_NONE		= 0,
	UPDATER_STATE_INITIALIZED	= 1,
	UPDATER_STATE_FINALIZED		= 2,
	UPDATER_STATE_UPDATING		= 3,
	UPDATER_STATE_UPDATED		= 4,
	UPDATER_STATE_INSPECTING	= 5,
	UPDATER_STATE_INSPECTED		= 6,
};

/* the log's and the segment analysis's state */
enum {
	UPDATER_OPEN_NONE	= 0,
	UPDATER_OPEN_OPENED	= 1,
	UPDATER_OPEN_CLOSED	= 2,
};

/* UpdateUtility::PowerControl's operations */
#define UPDATER_POWER_LOCK	3
#define UPDATER_POWER_UNLOCK	4
#define UPDATER_POWER_FLAGS	2

static SpkgGroupManager *s_spkgGroupManager = NULL;
static int s_state = UPDATER_STATE_NONE;
static int s_powerLocked = 0;
static int s_logState = UPDATER_OPEN_NONE;
static SceUID s_logFd = -1;
static int s_segmentAnalysisState = UPDATER_OPEN_NONE;

SceUpdaterMemoryInfo g_updaterMemoryInfo;

int UpdaterLogPrintf(const char *format, ...)
{
	char line[UPDATER_LOG_LINE_SIZE];
	va_list arguments;
	int length;

	va_start(arguments, format);
	if (s_logFd < 0)
		return 0;
	length = sceClibVsnprintf(line, sizeof(line), format, arguments);
	if (sceIoWrite(s_logFd, line, length) < 0)
		return SCE_UPDATER_ERROR_LOG_WRITE;
	return 0;
}

extern "C" {

int sceUpdaterInitialize(const SceUpdaterMemoryInfo *memoryInfo)
{
	if (s_state == UPDATER_STATE_INITIALIZED || s_state == UPDATER_STATE_UPDATING ||
	    s_state == UPDATER_STATE_UPDATED || s_state == UPDATER_STATE_INSPECTING ||
	    s_state == UPDATER_STATE_INSPECTED) {
		UPDATER_ERROR_FMT(98, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	if (memoryInfo == NULL) {
		UPDATER_ERROR(103);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (memoryInfo->updaterCoreAlloc == NULL) {
		UPDATER_ERROR(107);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (memoryInfo->updaterCoreFree == NULL) {
		UPDATER_ERROR(111);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (memoryInfo->updaterCoreMemalign == NULL) {
		UPDATER_ERROR(115);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	g_updaterMemoryInfo = *memoryInfo;
	s_spkgGroupManager = new SpkgGroupManager(0);
	s_state = UPDATER_STATE_INITIALIZED;
	return 0;
}

int sceUpdaterFinalize(int reboot)
{
	UpdateUtility utility;
	int result;

	if (s_state == UPDATER_STATE_NONE || s_state == UPDATER_STATE_FINALIZED) {
		UPDATER_ERROR_FMT(147, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	result = s_spkgGroupManager->GetResult();
	s_spkgGroupManager->Terminate();
	delete s_spkgGroupManager;
	s_state = UPDATER_STATE_FINALIZED;
	return utility.PowerControl(reboot, result);
}

int sceUpdaterGetProgressRate(void)
{
	return s_spkgGroupManager->GetProgressRate();
}

int sceUpdaterUpdateAllSpackage(const char *fileName)
{
	int ret;

	if (s_state == UPDATER_STATE_NONE || s_state == UPDATER_STATE_INITIALIZED ||
	    s_state == UPDATER_STATE_FINALIZED || s_state == UPDATER_STATE_UPDATING ||
	    s_state == UPDATER_STATE_INSPECTING) {
		UPDATER_ERROR_FMT(196, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	if (fileName == NULL) {
		UPDATER_ERROR(201);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	s_state = UPDATER_STATE_UPDATING;
	ret = s_spkgGroupManager->UpdateAllSpkg(fileName);
	s_state = UPDATER_STATE_UPDATED;
	return ret;
}

int sceUpdaterInspectAllSpackage(const char *fileName)
{
	int ret;

	if (s_state == UPDATER_STATE_NONE || s_state == UPDATER_STATE_FINALIZED ||
	    s_state == UPDATER_STATE_UPDATING || s_state == UPDATER_STATE_INSPECTING) {
		UPDATER_ERROR_FMT(238, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	if (fileName == NULL) {
		UPDATER_ERROR(243);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	s_state = UPDATER_STATE_INSPECTING;
	ret = s_spkgGroupManager->InspectAllSpackage(fileName);
	s_state = UPDATER_STATE_INSPECTED;
	return ret;
}

int sceUpdaterGetSpackageHeader(void *header, int type)
{
	if (s_state == UPDATER_STATE_NONE || s_state == UPDATER_STATE_FINALIZED ||
	    s_state == UPDATER_STATE_UPDATING || s_state == UPDATER_STATE_INSPECTING) {
		UPDATER_ERROR_FMT(269, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	return s_spkgGroupManager->GetSpackageHeader(header, type);
}

int sceUpdaterGetProgress(SceUpdaterProgress *progress)
{
	if (s_state == UPDATER_STATE_NONE || s_state == UPDATER_STATE_INITIALIZED ||
	    s_state == UPDATER_STATE_FINALIZED || s_state == UPDATER_STATE_UPDATED ||
	    s_state == UPDATER_STATE_INSPECTED) {
		UPDATER_ERROR_FMT(293, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	if (progress == NULL) {
		UPDATER_ERROR(298);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return s_spkgGroupManager->GetProgress(progress);
}

int sceUpdaterGetEstimateProcessTime(const char *fileName, int *time)
{
	if (time == NULL) {
		UPDATER_ERROR(320);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return s_spkgGroupManager->GetEstimateProcessTime(fileName, time);
}

int sceUpdaterVerifyPup(const char *fileName)
{
	PupController pup;
	int ret;

	ret = UpdateUtility::CheckDeviceStatus();
	if (ret != 0) {
		UPDATER_ERROR_FMT(344, "%x", ret);
		return ret;
	}
	if (fileName == NULL) {
		UPDATER_ERROR(349);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.VerifyPup(fileName);
}

int sceUpdaterVerifyPupSegment(const char *fileName, SceUInt64 segmentId)
{
	PupController pup;

	if (fileName == NULL) {
		UPDATER_ERROR(373);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.VerifyPupSegment(fileName, segmentId);
}

int sceUpdaterParse(const char *fileName, SceUInt64 segmentId, void *pData, SceUInt64 size,
		    SceUInt64 offset)
{
	PupController pup;

	if (s_state == UPDATER_STATE_NONE || s_state == UPDATER_STATE_FINALIZED) {
		UPDATER_ERROR_FMT(403, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	if (fileName == NULL) {
		UPDATER_ERROR(408);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (pData == NULL) {
		UPDATER_ERROR(412);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.Parse(fileName, segmentId, pData, size, offset);
}

int sceUpdaterGetSegmentSize(const char *fileName, SceUInt64 segmentId, SceUInt64 *size)
{
	PupController pup;

	if (fileName == NULL) {
		UPDATER_ERROR(437);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (size == NULL) {
		UPDATER_ERROR(441);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.GetSegmentSize(fileName, segmentId, size);
}

int sceUpdaterIsApplicablePup(const char *fileName)
{
	PupController pup;
	int ret;

	ret = UpdateUtility::CheckDeviceStatus();
	if (ret != 0) {
		UPDATER_ERROR_FMT(465, "%x", ret);
		return ret;
	}
	if (fileName == NULL) {
		UPDATER_ERROR(470);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.IsApplicablePup(fileName);
}

int sceUpdaterCheckAcceptableSpkgType(const char *fileName, const int *types, int count,
				      int *result)
{
	if (s_state == UPDATER_STATE_NONE || s_state == UPDATER_STATE_FINALIZED) {
		UPDATER_ERROR_FMT(498, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	if (fileName == NULL) {
		UPDATER_ERROR(503);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (types == NULL) {
		UPDATER_ERROR(508);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (count <= 0) {
		UPDATER_ERROR_FMT(513, "%d", count);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (result == NULL) {
		UPDATER_ERROR(518);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return s_spkgGroupManager->CheckAcceptableSpkgType(fileName, types, count, result);
}

int sceUpdaterGetPupInformation(const char *fileName, SceUpdaterPupInformation *information)
{
	PupController pup;

	if (fileName == NULL) {
		UPDATER_ERROR(544);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (information == NULL) {
		UPDATER_ERROR(548);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.GetPupInformation(fileName, information);
}

int sceUpdaterNetworkPreCheckVersion(SceUInt32 version, int *result)
{
	PupController pup;

	if (result == NULL) {
		UPDATER_ERROR(572);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.NetworkPreCheckVersion(version, result);
}

int sceUpdaterNetworkCheckBaseVersion(SceUInt32 version, int *result)
{
	PupController pup;

	if (result == NULL) {
		UPDATER_ERROR(596);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.NetworkCheckBaseVersion(version, result);
}

int sceUpdaterCheckNewerThanSystemVersion(SceUInt32 version, int *result)
{
	PupController pup;

	if (result == NULL) {
		UPDATER_ERROR(620);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return pup.CheckNewerThanSystemVersion(version, result);
}

SceUInt64 sceUpdaterGetSegmentAnalysisSize(void)
{
	PupController pup;

	return pup.GetSegmentAnalysisSize();
}

int sceUpdaterCreateSegmentAnalysisHandler(SceUInt64 size,
					   SceUpdaterSegmentAnalysisHandler *handler)
{
	SegmentAnalysis *analysis;

	if (s_state == UPDATER_STATE_NONE || s_state == UPDATER_STATE_FINALIZED) {
		UPDATER_ERROR_FMT(663, "%d", s_state);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	analysis = new SegmentAnalysis();
	*handler = analysis;
	analysis->Initialize(size);
	s_segmentAnalysisState = UPDATER_OPEN_OPENED;
	return 0;
}

int sceUpdaterDestroySegmentAnalysisHandler(SceUpdaterSegmentAnalysisHandler handler)
{
	if (s_segmentAnalysisState != UPDATER_OPEN_OPENED) {
		UPDATER_ERROR_FMT(692, "%d", s_segmentAnalysisState);
		return SCE_UPDATER_ERROR_ANALYSIS_STATE;
	}
	if (handler == NULL) {
		UPDATER_ERROR(697);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	delete static_cast<SegmentAnalysis *>(handler);
	s_segmentAnalysisState = UPDATER_OPEN_NONE;
	return 0;
}

int sceUpdaterIsSegmentAvailable(SceUpdaterSegmentData *data, int *result)
{
	if (s_segmentAnalysisState != UPDATER_OPEN_OPENED) {
		UPDATER_ERROR_FMT(723, "%d", s_segmentAnalysisState);
		return SCE_UPDATER_ERROR_ANALYSIS_STATE;
	}
	if (data == NULL) {
		UPDATER_ERROR(728);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	if (result == NULL) {
		UPDATER_ERROR(732);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return static_cast<SegmentAnalysis *>(data->handler)->IsSegmentAvailable(data, result);
}

int sceUpdaterGetDeviceInfo(SceUpdaterDeviceInfo *deviceInfo)
{
	UpdateUtility utility;

	if (deviceInfo == NULL) {
		UPDATER_ERROR(755);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return utility.GetDeviceInfo(deviceInfo);
}

int sceUpdaterGetUpdateModeState(void)
{
	UpdateUtility utility;

	return utility.GetUpdateModeState();
}

int sceUpdaterGetUpdateMode(SceUInt8 *mode)
{
	UpdateUtility utility;

	if (mode == NULL) {
		UPDATER_ERROR(795);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return utility.GetUpdateMode(mode);
}

int sceUpdaterSetUpdateMode(SceUInt8 mode)
{
	UpdateUtility utility;

	return utility.SetUpdateMode(mode);
}

int sceUpdaterUpdateModeSetLastError(int error)
{
	UpdateUtility utility;

	return utility.UpdateModeSetLastError(error);
}

int sceUpdaterUpdateModeGetLastError(int *error)
{
	UpdateUtility utility;

	if (error == NULL) {
		UPDATER_ERROR(855);
		return SCE_UPDATER_ERROR_INVALID_ARGUMENT;
	}
	return utility.UpdateModeGetLastError(error);
}

const char *sceUpdaterGetWorkingDirectory(void)
{
	PartitionManager partition;

	return partition.GetWorkingDirectory();
}

int sceUpdaterLogStart(void)
{
	char path[UPDATER_LOG_PATH_SIZE];

	sceClibStrncpy(path, sceUpdaterGetWorkingDirectory(), sizeof(path));
	sceClibStrncat(path, UPDATER_LOG_FILE_NAME, sizeof(path));
	if (s_logState == UPDATER_OPEN_OPENED) {
		UPDATER_ERROR_FMT(884, "%d", s_logState);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	s_logState = UPDATER_OPEN_OPENED;
	s_logFd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
			    UPDATER_LOG_FILE_MODE);
	if (s_logFd < 0) {
		UPDATER_ERROR_FMT(894, "%s %o : %x", path, UPDATER_LOG_FILE_MODE, s_logFd);
		return SCE_UPDATER_ERROR_LOG_OPEN;
	}
	return 0;
}

int sceUpdaterLogEnd(void)
{
	int ret;

	if (s_logState == UPDATER_OPEN_NONE || s_logState == UPDATER_OPEN_CLOSED) {
		UPDATER_ERROR_FMT(915, "%d", s_logState);
		return SCE_UPDATER_ERROR_INVALID_STATE;
	}
	s_logState = UPDATER_OPEN_CLOSED;
	ret = sceIoClose(s_logFd);
	if (ret < 0) {
		UPDATER_ERROR_FMT(924, "%x", ret);
		return SCE_UPDATER_ERROR_LOG_CLOSE;
	}
	s_logFd = -1;
	return 0;
}

int sceUpdaterPartitionSetUp(void)
{
	PartitionManager partition;

	return partition.PartitionSetUp();
}

int sceUpdaterCleanUp(void)
{
	PartitionManager partition;

	return partition.CleanUp();
}

const char *sceUpdaterGetPupPath(void)
{
	PartitionManager partition;

	return partition.GetPupPath();
}

int sceUpdaterPowerLock(void)
{
	UpdateUtility utility;
	int ret;

	if (s_powerLocked == 1)
		return SCE_UPDATER_ERROR_POWER_STATE;
	ret = utility.PowerControl(UPDATER_POWER_LOCK, UPDATER_POWER_FLAGS);
	if (ret == 0)
		s_powerLocked = 1;
	return ret;
}

int sceUpdaterPowerUnlock(void)
{
	UpdateUtility utility;
	int ret;

	if (s_powerLocked == 0)
		return SCE_UPDATER_ERROR_POWER_STATE;
	ret = utility.PowerControl(UPDATER_POWER_UNLOCK, UPDATER_POWER_FLAGS);
	if (ret == 0)
		s_powerLocked = 0;
	return ret;
}

void sceUpdaterSetProgressCallback(SceUpdaterProgressCallback callback, SceUInt32 interval)
{
	g_updaterProgressCallback = callback;
	g_updaterProgressInterval = interval;
}

int sceUpdaterCopyFile(SceUpdaterCopyFileTarget *src, SceUpdaterCopyFileTarget *dst,
		       SceUpdaterCopyFileOption *option)
{
	ParallelCopyFile copy(src, dst, option);

	return copy.CopyFile();
}

}

void *operator new(size_t size)
{
	return g_updaterMemoryInfo.updaterCoreAlloc(size);
}

void *operator new[](size_t size)
{
	return g_updaterMemoryInfo.updaterCoreAlloc(size);
}

void operator delete(void *ptr)
{
	g_updaterMemoryInfo.updaterCoreFree(ptr);
}

void operator delete[](void *ptr)
{
	g_updaterMemoryInfo.updaterCoreFree(ptr);
}
