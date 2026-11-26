/*
 * teleport/teleport_client_types.h: SceTeleportClient types and constants.
 */
#ifndef _SCE_TELEPORT_CLIENT_TYPES_H_
#define _SCE_TELEPORT_CLIENT_TYPES_H_

#include <scetypes.h>
#include <sceerror.h>
#include <net/ether.h>
#include <libnetctl.h>

typedef struct _SceTeleportClientInitializeInfo {
	SceVoid *(*_malloc)(SceUInt32 size);
	SceVoid (*_free)(void *ptr);
	SceUChar8 reserved[56];
} SceTeleportClientInitializeInfo;

typedef struct _SceTeleportDeviceInfo {
	SceChar8 ipAddress[SCE_NET_CTL_IPV4_ADDR_STR_LEN];
	SceUID innerData;
	SceChar8 deviceId[18];
	SceUChar8 reserved[22];
} SceTeleportDeviceInfo;

typedef struct _SceTeleportDeviceDescription {
	SceChar8 name[64];
	SceUInt32 systemVersion;
	SceUInt32 protocolVersion;
	SceChar8 innerData[32];
	SceUChar8 reserved[32];
} SceTeleportDeviceDescription;

typedef struct _SceTeleportRemoteAppInfo {
	SceChar8 name[256];
	SceChar8 appId[128];
	SceUInt32 appVersion;
	SceUChar8 reserved[64];
} SceTeleportRemoteAppInfo;

#define SCE_TELEPORT_CLIENT_RESPONSE_STATUS_DEFAULT		(0)
#define SCE_TELEPORT_CLIENT_RESPONSE_STATUS_ALREADY_LAUNCHED	(1)
#define SCE_TELEPORT_CLIENT_EXT_DATA_MAX_SIZE			128

typedef struct _SceTeleportLaunchResponse {
	SceChar8 appErrorHint[256];
	SceInt32 status;
	SceChar8 *extData;
	SceUInt32 extDataBufSize;
	SceUInt16 appPort;
	SceUChar8 reserved[86];
} SceTeleportLaunchResponse;

typedef struct _SceTeleportRemoteAppInfoNum {
	SceUInt16 num;
	SceUChar8 reserved[34];
} SceTeleportRemoteAppInfoNum;

#define SCE_TELEPORT_LAUNCH_OPTION_DEFAULT			0
#define SCE_TELEPORT_LAUNCH_OPTION_FORCE_LAUNCH			1
#define SCE_TELEPORT_LAUNCH_ARG_SIZE_MAX			952

typedef struct _SceTeleportRemoteAppArg {
	const SceChar8 *arg;
	SceChar8 passCode[4];
	SceUInt32 option;
	SceUChar8 reserved[48];
} SceTeleportRemoteAppArg;

typedef struct _SceTeleportWakeupInfo {
	SceUChar8 reserved[64];
} SceTeleportWakeupInfo;

typedef struct _SceTeleportCacheInfo {
	SceChar8 deviceName[64];
	SceChar8 passCode[4];
	SceBool initialTarget;
	SceChar8 deviceId[18];
	SceUChar8 reserved[42];
} SceTeleportCacheInfo;

#ifdef __cplusplus
typedef void (*SceTeleportClientGetDeviceInfoCallback)(const SceTeleportDeviceInfo &info);
#else
typedef void (*SceTeleportClientGetDeviceInfoCallback)(const SceTeleportDeviceInfo *info);
#endif

#endif /* _SCE_TELEPORT_CLIENT_TYPES_H_ */
