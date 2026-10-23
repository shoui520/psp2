/* Per-application device and audio arbitration state. */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <touch/touch.h>
#include <appmgr/appmgr_debugger.h>

typedef struct AppmgrAudioApp {
	int appId;
	char appName[32];
	SceUID processId;
	int isRunning;
	int btrmAcquired;
	int bgmPriority;
	int enableBgmSubPriority;
	int bgmSubPriority;
	SceChar8 proxyAppName[32];
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

/* What sceAppMgrGetAppMgrState copies out (_deviceGetAppMgrState returns it). */
typedef struct AppMgrState {
	int prioritizeSystemChat;
	int inputImposed;
	int btrmOwner;
	int systemChatDucking;		/* the BGM state's, copied when it is applied */
	int recommendedScreenOrientation;
	int ctrlpMode;
	SceUInt32 reserved[26];
} AppMgrState;

_Static_assert(sizeof(AppMgrState) == 0x80, "application manager state");

/*
 * The audio ports' owners and settings that dumpBgmState prints:
 * updateAudioArbitration2NoLock builds the next one and applies what changed.
 */
typedef struct AppmgrBgmState {
	SceUID soundExclusivePid;
	int soundExclusivePriority;
	SceUInt32 displayRequestEnabled;
	SceUInt32 displayRequestMode;
	int displayRequestBlanked;
	int soundExclusiveDisplayRequest;
	int shellMainPortEnabled;
	SceUID mainOutPortOwner;
	SceUID bgmOutPortOwner;
	int bgmOutPortOwnerPriority;
	int bgmOutPortOwnerSubPriority;
	SceUID voiceOutPortOwner;
	int voiceOutPortOwnerAppId;
	SceUID inputPortOwner;
	SceUInt32 otherPortVolume;
	int systemChatDucking;
} AppmgrBgmState;

_Static_assert(sizeof(AppmgrBgmState) == 0x40, "BGM state");

/* appmgr_device.c's state: initializeAppMgrDevice clears it whole. */
typedef struct AppmgrDevice {
	SceUID shellPid;
	int activeAppId;
	SceUID renderingPids[8];
	TouchProcessPriorities touchProcessPriorities;
	AppMgrState appMgrState;
	AppmgrBgmState bgmState;
	SceUID displayOwner[2];
	SceUID systemChatPid;
	SceUInt32 systemChatDuckingVolume;
	SceUID reserved;		/* set to -1 at initialization, never read */
	int audioPortsDisabled;
	int backRenderAppId;
	int bgmEnabled;
	AppmgrDeviceOwners deviceOwners;
	AppmgrAudioApp apps[20];
	SceUID audioUpdateMutex;
} AppmgrDevice;

_Static_assert(offsetof(AppmgrDevice, appMgrState) == 0x6c, "device AppMgrState");
_Static_assert(offsetof(AppmgrDevice, bgmState) == 0xec, "device BGM state");
_Static_assert(offsetof(AppmgrDevice, deviceOwners) == 0x14c, "device owners");
_Static_assert(offsetof(AppmgrDevice, apps) == 0x168, "device applications");
_Static_assert(sizeof(AppmgrDevice) == 0xa7c, "device state");

extern AppmgrDevice g_appmgrDevice;

/* What sceAppMgrGetCurrentBgmState2 copies out: the BGM port's owner. */
typedef struct AppmgrCurrentBgmState {
	SceUID bgmOutPortOwner;
	int priority;
	int subPriority;
	int appId;			/* -1 when nobody owns the port */
	SceUInt32 reserved[4];
} AppmgrCurrentBgmState;

_Static_assert(sizeof(AppmgrCurrentBgmState) == 32, "current BGM state");

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
void _deviceSetApp(int appId, const char *name, const char *proxyName);
void _deviceSetRunStatus(int appId, int isRunning);
void _deviceSetProcessId(int appId, SceUID pid);
int sceAppMgrGetCurrentBgmState(SceUID *currentBgmId, int *currentBgmState);
int sceAppMgrGetCurrentBgmState2(AppmgrCurrentBgmState *state);
void _deviceGetBgmProxyAppName(int appId, char *name);
void _deviceSetName(int appId, const char *name);
int appmgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled, SceUInt32 displayMode);
int appmgrReleaseSoundOutExclusive(void);
int appmgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayRequest);
int appmgrReleaseSoundOutExclusive2(void);
int appmgrAcquireSoundOutExclusive3(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions);
int appmgrReleaseSoundOutExclusive3(void);
int sceAppMgrSetBgmProxyApp(const char *appName);
int _sceAppMgrDeviceDeclareShell(const char *appName, int appId, SceUID pid);
int _updateDeviceArbitration2(void);
int initializeAppMgrDevice(void);
int sceAppMgrGetAppMgrState(AppMgrState *state);
AppMgrState *_deviceGetAppMgrState(void);
void _deviceSetAudioPortsDisabled(int disabled);
int sceAppMgrSetBackRenderPortOwner(int appId);
void _deviceRefreshDisplay(void);
int _deviceSetBgmEnabled(int enabled);
int _SetDisplayOwner(SceUInt32 plane, SceUID pid);
void _deviceRefreshAudioPort(SceUID pid);
void appmgrCaptureDeviceInfo(AppMgrDeviceInfo *info);
