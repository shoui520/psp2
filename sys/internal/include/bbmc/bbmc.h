#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

typedef struct BbmcSubscribeFeatureIdParam {
	SceInt32 subscriberId;
	SceUID callbackId;
	SceUInt32 featureId;
	SceUInt32 eventMask;
} BbmcSubscribeFeatureIdParam;

typedef struct BbmcUpdatePackage {
	SceUInt32 chunkCount;
	SceUInt32 chunkIndex;
	void *buffer;
	SceSize size;
} BbmcUpdatePackage;

typedef struct BbmcDriverInfo {
	SceInt32 currentState;
	SceInt32 targetState;
	SceUInt32 firstBootReply;
	SceUInt32 secondBootReply;
} BbmcDriverInfo;

_Static_assert(sizeof(BbmcSubscribeFeatureIdParam) == 16,
	"BBMC subscription record size");
_Static_assert(offsetof(BbmcSubscribeFeatureIdParam, callbackId) == 4 &&
	offsetof(BbmcSubscribeFeatureIdParam, featureId) == 8 &&
	offsetof(BbmcSubscribeFeatureIdParam, eventMask) == 12,
	"BBMC subscription record layout");
_Static_assert(sizeof(BbmcUpdatePackage) == 16,
	"BBMC update package size");
_Static_assert(offsetof(BbmcUpdatePackage, chunkIndex) == 4 &&
	offsetof(BbmcUpdatePackage, buffer) == 8 &&
	offsetof(BbmcUpdatePackage, size) == 12,
	"BBMC update package layout");
_Static_assert(sizeof(BbmcDriverInfo) == 16, "BBMC driver information size");
_Static_assert(offsetof(BbmcDriverInfo, targetState) == 4 &&
	offsetof(BbmcDriverInfo, firstBootReply) == 8 &&
	offsetof(BbmcDriverInfo, secondBootReply) == 12,
	"BBMC driver information layout");

struct BbmcNetworkInfo;
struct BbmcUnblockPinParam;
struct BbmcStringPair;
struct BbmcStringSet;
struct BbmcStringPairList;

/* bbmc.c */
int SceBbmcForDriver_D98DB926(void);
int SceBbmcForDriver_D16D0845(void);
int sceBbmcGetNetworkInfoForDriver(struct BbmcNetworkInfo *info);
int sceBbmcSubscribeFeatureIdForDriver(BbmcSubscribeFeatureIdParam *parameters);
int sceBbmcUnsubscribeFeatureForDriver(int subscriberId);
int sceBbmcClearEventForDriver(int subscriberId, SceUInt32 eventMask);
int sceBbmcResumeSubscriberCallbackForDriver(int subscriberId);
int sceBbmcTurnOnForDriver(int subscriberId);
int sceBbmcTurnOffForDriver(int subscriberId);
int sceBbmcGetPinStatusForDriver(int subscriberId, SceUInt32 *status, SceUInt16 *retries);
int sceBbmcUnblockPinForDriver(int subscriberId, struct BbmcUnblockPinParam *parameters);
int sceBbmcVerifyPinForDriver(int subscriberId, const void *pin, SceSize length);
int sceBbmcSMSGetListStartForDriver(int subscriberId);
int sceBbmcSMSGetListDoneForDriver(int subscriberId, SceUInt32 *count, SceUInt32 *indices,
	SceUInt32 capacity);
int sceBbmcSMSReadStartForDriver(int subscriberId, SceUInt32 memoryIndex);
int sceBbmcSMSReadDoneForDriver(int subscriberId, void *data, SceUInt32 *format);
int sceBbmcSMSReadAckForDriver(int subscriberId, SceUInt32 memoryIndex, SceUInt32 deleteMessage);
int sceBbmcSMSSendForDriver(int subscriberId, const void *data, SceSize length, SceUInt32 format);
int sceBbmcGetDriverInfoForDriver(int subscriberId, BbmcDriverInfo *info);
int sceBbmcReserveOperationModeForDriver(int subscriberId, SceUInt32 operationMode);
int sceBbmcSetupGetValueStartForDriver(int subscriberId, SceUInt32 type);
int sceBbmcSetupGetValueDoneForDriver(int subscriberId, SceUInt32 type, void *output, SceSize size);
int sceBbmcSetupSetValueForDriver(int subscriberId, SceUInt32 type, const void *input,
	SceSize size);
int sceBbmcSetLPMModeForDriver(int subscriberId, int mode);
int sceBbmcDLQBackupEFSNVForDriver(int subscriberId, const struct BbmcStringPair *parameters);
int sceBbmcDLQRestoreEFSNVForDriver(int subscriberId, const struct BbmcStringPair *parameters);
int sceBbmcDLQDownloadSafeImageForDriver(int subscriberId, const struct BbmcStringSet *parameters);
int sceBbmcDLQDownloadSlotImageForDriver(int subscriberId,
	const struct BbmcStringPairList *parameters);
int sceBbmcDLGetSwdlErrorForDriver(int subscriberId, SceInt32 *error);
int sceBbmcDLQConfirmChangePartitionForDriver(int subscriberId);
int sceBbmcDLUpdaterStartForDriver(int subscriberId, BbmcUpdatePackage *package);
int sceBbmcDLUpdaterContinueForDriver(int subscriberId, BbmcUpdatePackage *package);
int sceBbmcDLUpdaterGetArchiveVersionForDriver(int subscriberId, SceUInt32 *version);
int sceBbmcDLUpdaterGetSequenceForDriver(int subscriberId, SceUInt32 *firstStatus,
	SceUInt32 *secondStatus, BbmcDriverInfo *info);
int sceBbmcDLUpdaterGetPkgIdForDriver(int subscriberId, SceUInt32 *pkgId);
/* not written in C yet: the prototype follows update_mgr's use (bbmc_status.c) */
int SceBbmcForDriver_4281E476(int subscriberId, uint32_t *status);

/* usb_extif.c */
int sceBbmcUsbExtIfStart(int mode);
int sceBbmcUsbExtIfStop(void);
int sceBbmcUsbExtIfRead(unsigned int channel, void *data, SceSize length);
int sceBbmcUsbExtIfWrite(unsigned int channel, const void *data, SceSize length);
