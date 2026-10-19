/* Per-application device and audio arbitration state. */
#pragma once

#include <stddef.h>
#include <scetypes.h>

typedef struct AppmgrAudioApp {
	int appId;
	char appName[32];
	SceUID processId;
	int isRunning;
	int btrmAcquired;
	int bgmPriority;
	int enableBgmSubPriority;
	int bgmSubPriority;
	char proxyAppName[32];
	int soundExclusivePriority;
	SceUInt32 displayRequestEnabled;
	SceUInt32 displayRequestMode;
	int displayRequestBlanked;
	int soundExclusiveDisplayRequest;
	SceUInt32 audioPortOpenFlags;
} AppmgrAudioApp;

_Static_assert(sizeof(AppmgrAudioApp) == 116, "audio application size");
_Static_assert(offsetof(AppmgrAudioApp, processId) == 36, "audio process");
_Static_assert(offsetof(AppmgrAudioApp, btrmAcquired) == 44, "BTRM acquisition");
_Static_assert(offsetof(AppmgrAudioApp, proxyAppName) == 60, "BGM proxy name");
_Static_assert(offsetof(AppmgrAudioApp, soundExclusivePriority) == 92, "sound priority");
_Static_assert(offsetof(AppmgrAudioApp, audioPortOpenFlags) == 112, "audio ports");

typedef struct AppmgrDeviceOwners {
	SceUID shellPid;
	SceUID backRenderPid;
	SceUID renderingPid;
	SceUID inputPid;
	int inputImposed;
	int btrmOwnerAppId;
	int ctrlpMode;
} AppmgrDeviceOwners;

_Static_assert(sizeof(AppmgrDeviceOwners) == 28, "device arbitration owners");

/* Complete local copy of acquisition3's sixteen user argument bytes. */
typedef struct AppmgrSoundOutExclusiveArguments {
	int displayBlanked;
	SceUInt32 unsupportedOptions;
	SceUInt32 reserved[2];
} AppmgrSoundOutExclusiveArguments;

_Static_assert(sizeof(AppmgrSoundOutExclusiveArguments) == 16,
	"sound acquisition argument copy");

/* appmgr_device.c */
int dumpBgmState(void);
void _deviceSetRunStatus(int appId, int isRunning);
void _deviceGetBgmProxyAppName(int appId, char *name);
int appmgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode);
int appmgrReleaseSoundOutExclusive(void);
int appmgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayRequest);
int appmgrReleaseSoundOutExclusive2(void);
int appmgrAcquireSoundOutExclusive3(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions);
int appmgrReleaseSoundOutExclusive3(void);
int sceAppMgrSetBgmProxyApp(const char *appName);
void _deviceRefreshAudioPort(SceUID pid);
