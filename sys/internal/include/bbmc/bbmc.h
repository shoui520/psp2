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

/* name unknown: the network's last reject cause and reason, as the caller's
 * byte buffer receives them */
typedef struct __attribute__((packed)) BbmcRejectCause {
	SceUInt32 cause;
	SceUInt32 reason;
} BbmcRejectCause;

struct BbmcNetworkInfo;
struct BbmcNetworkScanInfo;
struct BbmcCallEndReason;
struct BbmcProactiveCmd;
struct BbmcTerminalResponse;
struct QmiCat2CommandOptions;
struct BbmcFileRefreshDetail;
struct BbmcSimFile;
struct BbmcSimContentInfo;
struct BbmcControlKeyParam;
struct QmiIndexedResponse;
struct BbmcModemConfigurationLeaf;
struct BbmcUnblockPinParam;
struct BbmcStringPair;
struct BbmcStringSet;
struct BbmcStringPairList;

/* bbmc.c */
int SceBbmcForDriver_D98DB926(void);
int SceBbmcForDriver_D16D0845(void);
int SceBbmcForDriver_EBC5D3DB(int subscriberId);
int SceBbmcForDriver_503C7E0B(int subscriberId, struct BbmcNetworkScanInfo *scan);
int SceBbmcForDriver_739274F3(int subscriberId);
int SceBbmcForDriver_75FA68D3(int subscriberId, SceInt32 attach);
int SceBbmcForDriver_3D27000A(int subscriberId);
int SceBbmcForDriver_BF550F4A(int subscriberId, struct BbmcCallEndReason *reason);
int SceBbmcForDriver_FB3DA35E(int subscriberId, SceUInt32 flags);
int SceBbmcForDriver_7EA964B6(int subscriberId, SceUInt32 flags);
int SceBbmcForDriver_BC3C8942(int subscriberId, SceUInt32 *flags);
int SceBbmcForDriver_E09808A2(int subscriberId, SceUInt32 *bearer);
int SceBbmcForDriver_A9CF664F(SceInt32 *transition);
int SceBbmcForDriver_399EF002(int subscriberId, struct BbmcProactiveCmd *command);
int SceBbmcForDriver_93E5345A(int subscriberId, const struct BbmcTerminalResponse *response);
int SceBbmcForDriver_F4C8482D(int subscriberId, const struct QmiCat2CommandOptions *options);
int SceBbmcForDriver_2930DB51(int subscriberId, struct BbmcFileRefreshDetail *detail);
int SceBbmcForDriver_009C73D2(int subscriberId, SceUInt32 response);
int SceBbmcForDriver_D288E771(int subscriberId, const struct BbmcSimFile *file);
int SceBbmcForDriver_24EE6E9C(int subscriberId, struct BbmcSimContentInfo *content);
int SceBbmcForDriver_683D19BF(int subscriberId, const struct BbmcControlKeyParam *param);
int SceBbmcForDriver_6AD4053A(int subscriberId, const struct BbmcControlKeyParam *param);
int SceBbmcForDriver_3D06FDB0(int subscriberId, SceUInt32 featureId, struct QmiIndexedResponse *status);
int SceBbmcForDriver_07ADDA90(int subscriberId, struct BbmcModemConfigurationLeaf *leaf);
int SceBbmcForDriver_FCF6A1EA(int subscriberId, SceUInt32 *version);
int SceBbmcForDriver_92D7C12C(int subscriberId, BbmcRejectCause *cause);
int SceBbmcForDriver_864E6CA0(int subscriberId, SceUInt32 *value);
int SceBbmcForDriver_B17A056F(int subscriberId, char *revision, SceSize size);
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
int SceBbmcForDriver_B7E650A9(int subscriberId);
int sceBbmcDLQConfirmChangePartitionForDriver(int subscriberId);
int sceBbmcDLUpdaterStartForDriver(int subscriberId, BbmcUpdatePackage *package);
int sceBbmcDLUpdaterContinueForDriver(int subscriberId, BbmcUpdatePackage *package);
int sceBbmcDLUpdaterGetArchiveVersionForDriver(int subscriberId, SceUInt32 *version);
int sceBbmcDLUpdaterGetSequenceForDriver(int subscriberId, SceUInt32 *firstStatus,
	SceUInt32 *secondStatus, BbmcDriverInfo *info);
int sceBbmcDLUpdaterGetPkgIdForDriver(int subscriberId, SceUInt32 *pkgId);
/* not written in C yet: the prototype follows update_mgr's use (bbmc_status.c) */
int SceBbmcForDriver_4281E476(int subscriberId, SceUInt32 *matches);

/* usb_extif.c */
int sceBbmcUsbExtIfStart(int mode);
int sceBbmcUsbExtIfStop(void);
int sceBbmcUsbExtIfRead(unsigned int channel, void *data, SceSize length);
int sceBbmcUsbExtIfWrite(unsigned int channel, const void *data, SceSize length);
