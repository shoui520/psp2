#ifndef UPDATE_MGR_INTERNAL_H
#define UPDATE_MGR_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <psp2kern/kernel/proc_event.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <motion/motion_dev.h>
#include <touch/touch.h>
#include <sdstor/mbr.h>
#include <threadmgr/threadmgr.h>
#include "pup.h"

typedef char SceUpdateMode;

/* Whole user request, including the uninterpreted words preserved on output. */
typedef struct SceKernelSpackageArgs {
	SceSize arg_size;
	uint32_t package_type;
	void *addr;
	SceSize size;
	uint32_t flags;
	uint32_t extension[2];
	uint32_t *seq_no;
	uint32_t *result;
	uint32_t *progress;
	uint32_t *written_rates;
} SceKernelSpackageArgs;

_Static_assert(sizeof(SceKernelSpackageArgs) == 44, "s-package user request size");
_Static_assert(offsetof(SceKernelSpackageArgs, flags) == 16 &&
	       offsetof(SceKernelSpackageArgs, seq_no) == 28,
	       "s-package user request layout");

typedef struct SceSblUsSpkgInfo {
	SceSize size;
	uint32_t version;
	uint8_t status[4];
	int reserved2;
} SceSblUsSpkgInfo;

_Static_assert(sizeof(SceSblUsSpkgInfo) == 16, "s-package information size");

typedef struct UpdateApplicableVersion {
	uint32_t version;
	uint32_t condition;
	uint32_t reserved[2];
} UpdateApplicableVersion;

_Static_assert(sizeof(UpdateApplicableVersion) == 16,
	       "applicable version information size");

typedef struct UpdateUtilNodeStatus {
	uint32_t requestId;
	uint32_t seq_no;
	uint32_t result;
	uint32_t progress;
	uint32_t written_rates;
	void *buffer;
} UpdateUtilNodeStatus;

_Static_assert(sizeof(UpdateUtilNodeStatus) == 24, "update node status size");

extern int updateInitializeAsync;
extern SceMotionDevDeviceInfo updateMotionDeviceInfo;
extern struct touchDeviceInfo updateTouchDeviceInfo[2];
extern uint16_t updateSysconTouchDeviceInfo[4];
extern int updateMotionPollCount;
extern int updateTouchPollCount;
extern int updateUseBatteryService;
extern int updateUseCpupService;
extern uint16_t updateMotionHardwareType;
extern int updateUseCpupExtendedService;
extern uint16_t updateMotionFirmwareVersion;
extern unsigned int updateCpupBoardVersion;
extern unsigned int updateCpupProtocolVersion;
extern int updateMotionConnected;
extern unsigned int updateBatteryDataFlashVersion;
extern int updateCpupConnected;
extern unsigned int updateBatteryFirmwareVersion;
extern unsigned int updateBatteryHardwareInfo;
extern int updateUseCpupPowerService;
extern int updateUseTouchService;
extern unsigned int pupFileBufferSize;
extern PupHeader *pupFileBuffer;
extern int pupSmId;
extern PupHeader pupFileHeader;
int SpkgInfoUtilGetNVSFlagStatus(uint32_t *status, uint32_t *flags);
int SpkgInfoUtilGetSNVSFlagStatus(uint32_t *status, uint32_t *flags,
				 int load_sm, int smid);
int SpkgInfoUtilInitForUpdater(uint32_t *status, uint32_t *flags);
int SpkgInfoUtilInitSystemSwVersion(void);
int SpkgInfoUtilGetSystemSwVersion(void);
int BicUtilIsDisconnected(void);
int BicUtilIsUnsupportedHardware(void);
int TouchUtilGetFirmwareVersion(unsigned int port, unsigned int *version);
int DeciUtilInformUpdateStarted(int number1, int number2,
			       const char *str, SceSize len);
int DeciUtilInformUpdateOngoing(int number1, int number2);
int DeciUtilInformUpdateFinished(int number, const char *str, SceSize len);
int DeciUtilSetSwInfoInt(const char *name, SceSize nameLength, uint32_t value);
int DeciUtilSetSwInfoStr(const char *name, SceSize nameLength,
			const char *value, SceSize valueLength);
int DeciUtilSetSwInfoBin(const char *name, SceSize nameLength,
			const void *value, SceSize valueLength);
int UpdateUtilWritePartition(const void *buffer, uint64_t size, uint64_t offset,
			     const SceMbr *mbr, uint8_t partition,
			     int nodeType, UpdateUtilNodeStatus *status);
int UpdateMgrWritePupSegment(SceUID heapId, SceUID fd,
			     const PupCachedHeader *pup, const SceMbr *mbr,
			     uint8_t partition, uint64_t segmentId);

int SpkgInfoUtilGetVersion(int packageType, int flags, unsigned int *version,
			  int loadSm, int existingSmId);
int SpkgInfoUtilSetVersion(int packageType, int flags, unsigned int version,
			  int loadSm, int existingSmId);
int UpdateUtilReleaseAllBuffers(int processId);
int UpdateUtilGetNodeStatus(int nodeType, UpdateUtilNodeStatus *status, int lock);
int UpdateMgrProcessCreate(SceUID pid, SceProcEventInvokeParam2 *param, int flags);
int UpdateMgrProcessExit(SceUID pid, SceProcEventInvokeParam1 *param, int flags);
int UpdateMgrProcessKill(SceUID pid, SceProcEventInvokeParam1 *param, int flags);
int UpdateMgrProcessStart(SceUID pid, int eventType,
			 SceProcEventInvokeParam1 *param, int flags);
int UpdateMgrRegisterEventHandlers(void);
int sceSblUpdateMgrInitPeripheralsPhase2(SceSize arglen, void *argp);
int invalidate_prev_system_info(SceSize arglen, void *argp);
unsigned int UpdateUtilInit(void);
int StorageUtilInit(void);
SceUID StorageUtileGetPartitionFd(uint32_t partition, uint8_t active);
int StorageUtilReadMbr(SceMbr *mbr);
int StorageUtilWriteMbr(const SceMbr *mbr);
int UpdateUtilGetHeapId(void);
int UpdateUtilCreatePacketPayloadPupWMWithBuffer(void **packet,
		SceKernelPAVector **dataVector, SceKernelPAVector **vectorVector,
		const PupHeaderDigest *headerDigest, void **buffer, SceSize size);
int UpdateUtilCreatePacketPayloadPupASWithBuffer(void **packet,
		SceKernelPAVector **dataVector, SceKernelPAVector **vectorVector,
		const PupHeaderDigest *headerDigest, void **buffer, SceSize size);
int UpdateUtilIoRead(SceUID fd, int64_t offset, uint64_t size, void *buffer);
int UpdateUtilSetupSm(unsigned int type, int *smId, int commandId);
int UpdateUtilStopSm(int *smId);
int UpdateUtilCreatePacketPayloadWithBuffer(void **packet,
		SceKernelPAVector **dataVector, SceKernelPAVector **vectorVector,
		void **buffer, SceSize size);
int SpkgInfoUtilGetHardwareIndex(int *index);
uint32_t SpkgInfoUtilGetHardwareSupportMask(unsigned int index);
int SpkgInfoUtilIsBelowMinimumVersion(int packageType, uint32_t version,
				    uint32_t currentVersion);
int UpdateMgrdoUpdate(int heapId, const char *path);
int sceSblUsPowerControlForKernel(int operation, int flags);
int UpdateUtilUA2KA(const void *userBuffer, SceUID pid, void **kernelBuffer);
int UpdateUtilKA2UA(const void *kernelBuffer, SceUID pid, void **userBuffer);
int UpdateUtilGetNodeStatusLocked(int nodeType, UpdateUtilNodeStatus *status);
int UpdateUtilAllocateBuffer(SceSize size, void **buffer, SceUID pid);
int UpdateUtilReleaseBuffer(void *buffer, SceUID pid);
int SpkgUtilGetStatus(int nodeType, uint32_t requestId, uint32_t *seq_no,
		     uint32_t *result, uint32_t *progress, uint32_t *written_rates);
int SpkgInfoUtilGetSpkgInfo(int packageType, SceSblUsSpkgInfo *info);
int SpkgUtilGetUpdateMode(SceUpdateMode *mode);
int SpkgUtilSetUpdateMode(SceUpdateMode mode);
const SceMbr *SpkgUtilGetMbr(void);
int SpkgUtilUpdateSpackageSync(SceUID fd, const SceMbr *mbr, int nodeType,
			      int packageType, const PupSegment *segment, int flags);
int PupUtilPupGetCachedHeader(PupCachedHeader *header);
int PupUtilPupFreeCachedHeader(void);
int UpdateUtilLockPupHeader(void);
int UpdateUtilUnlockPupHeader(void);
SceUID UpdateUtilCreateThread(const char *name, SceKernelThreadEntry entry,
		int priority, SceSize stackSize, unsigned int attr,
		int cpuAffinityMask, const SceKernelThreadOptParam *option);
int SpkgUtilPowerControl(int operation, int flags);
int SpkgInfoUtilGetApplicableVersion(int type, UpdateApplicableVersion *info);
int PupUtilVerifyPup(SceUID fd);
int sceSblUsVerifyPup(const char *path);
int sceSblUsVerifyPupHeader(const char *path);
int PupUtilVerifyPupHeader(SceUID fd, int load_sm, int resident_sm);
int sceSblUsVerifyPupSegment(const char *path, uint64_t segment_index);
int sceSblUsVerifyPupSegmentById(const char *path, uint64_t seg_id);
int sceSblUsVerifyPupWatermark(const char *path);
int sceSblUsVerifyPupAdditionalSign(const char *path);
int PupUtilVerifyPupWatermark(SceUID fd, int need_header_verify,
			     int load_sm, int resident_sm);
int PupUtilVerifyPupAdditionalSign(SceUID fd, int need_header_verify,
				  int load_sm, int resident_sm);
int PupUtilVerifyPupSegmentById(SceUID fd, uint64_t seg_id, int need_header_verify,
				int load_sm, int resident_sm);
int PupUtilVerifyPupSegment(SceUID fd, uint64_t segment_index, int need_header_verify,
			    int load_sm, int resident_sm);
int SpkgUtilUpdateSpackage(int packageType, void *buffer, SceSize size,
			  uint32_t flags, uint32_t *requestId);
int SpkgUtilInspectSpackage(int packageType, void *buffer, SceSize size,
			   uint32_t flags, uint32_t *requestId);
int SpkgUtilExtractSpackage(int packageType, void *buffer, SceSize size,
			   uint32_t flags, uint32_t *requestId);

extern int updateThreadArgument;
extern int updateUseMotionService;
extern int updateUseSysconConfigStorage;
extern uint32_t updateLastResult;
extern int updateUseSysconUpdateService;
extern int updateUseSysconNvsService;
extern int updateUseSysconPowerControl;
extern int updateInitialized;
extern int updateSysconDownloader;
extern int updateUseSysconSnvs;
extern unsigned int updateSysconFirmwareVersion;
extern unsigned int updateSysconHardwareInfo;
extern unsigned int updateSysconDownloaderVersion;

#endif
