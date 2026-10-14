#pragma once
#include <sce/hdmi.h>

#include <psp2kern/types.h>

typedef struct AvConfigScalingRatio {
	SceSize size;
	SceUInt32 ratio;
} AvConfigScalingRatio;

_Static_assert(sizeof(AvConfigScalingRatio) == 8, "HDMI scaling ratio size");

typedef struct AvConfigMonitorInfo {
	SceSize size;
	SceUInt8 productId[10];
	SceUInt8 blockCount;
	char monitorName[13];
	SceUInt8 reserved[3];
	SceUInt8 padding;
	struct {
		SceUInt32 supported;
		SceUInt32 reserved;
	} videoFormats[4];
} AvConfigMonitorInfo;

_Static_assert(sizeof(AvConfigMonitorInfo) == 64, "monitor information size");
_Static_assert(__builtin_offsetof(AvConfigMonitorInfo, videoFormats) == 32,
	       "monitor video formats");

typedef struct AvConfigAvlsTimer {
	SceKernelSpinlock lock;
	int busy;
	unsigned int elapsed;
	int tracking;
} AvConfigAvlsTimer;

_Static_assert(sizeof(AvConfigAvlsTimer) == 16, "AVLS timer size");

typedef struct AvConfigBtAudioDevice {
	SceUInt64 address;
	SceUInt32 profiles;
	SceUInt32 profile;
	SceUInt32 flags;
	SceUInt32 sampleRate;
} AvConfigBtAudioDevice;

typedef struct AvConfigBtAudioState {
	AvConfigBtAudioDevice active;
	AvConfigBtAudioDevice pending;
	int deviceChanged;
	int completionFlags;
	int operation;
	int disconnected;
	int buttonCompleted;
	int volumeNotification;
} AvConfigBtAudioState;

typedef struct AvConfigSettings {
	SceUInt32 connectedDevices;
	int audioInputActive;
	int btMic;
	int btAudioPolicy;
	int displayEnabled;
	int displayMode;
	int displayBlanked;
	int displayUpdatePending;
	int hdcpState;
	int volumeKeyCommand;
	int systemVolume;
	int btVolumeNotified;
	int btVolume;
	int shutterVolume;
	int masterVolume;
	SceUInt32 reserved[3];
	int suspendInterval;
	unsigned int brightness;
	int acOnline;
	int soundOutput;
	int speakerMute;
	int dimmingInterval;
	int colorSpaceMode;
} AvConfigSettings;

typedef struct AvConfigState {
	AvConfigBtAudioState bt;
	int btAudioRunning;
	SceUInt32 reserved;
	AvConfigSettings settings;
	SceUInt32 previousOutputDevices[2];
	SceUInt32 previousInputDevice;
	SceUInt32 outputDevices[2];
	SceUInt32 inputDevice;
	SceUInt32 previousRoutingChanges;
	SceUInt32 routingChanges;
	SceUID eventFlag;
	SceUID thread;
	SceUInt32 eventBits;
	int muted;
	int savedVolume;
	SceUID callback;
	SceUInt32 callbackMask;
	int hasVideoHardware;
	int avlsTimerEnabled;
} AvConfigState;

typedef struct AvConfigDisplayRequest {
	int active;
	int enabled;
	int mode;
	int blanked;
} AvConfigDisplayRequest;

typedef struct AvConfigConfiguration {
	AvConfigDisplayRequest displayRequests[16];
	int displayRequestCount;
	SceUInt32 reserved;
	AvConfigSettings settings;
	SceKernelSpinlock lock;
	SceUInt32 reservedTail;
} AvConfigConfiguration;

_Static_assert(sizeof(AvConfigBtAudioDevice) == 24, "BT audio device size");
_Static_assert(sizeof(AvConfigBtAudioState) == 72, "BT audio state size");
_Static_assert(sizeof(AvConfigSettings) == 100, "AV settings size");
_Static_assert(sizeof(AvConfigState) == 248, "AV state size");
_Static_assert(sizeof(AvConfigConfiguration) == 372, "AV configuration size");

extern AvConfigState avConfigState;
extern AvConfigConfiguration avConfigConfiguration;
extern int avConfigHdmiAudioMode;
extern int avConfigHdmiAvailable;
extern int avConfigHdmiPowered;
extern const char avConfigMasterVolumeCategory[];
extern const char avConfigMasterVolumeKey[];

int avConfigConvertBtVolume(unsigned int volume, unsigned int *converted);
int avConfigSetNotificationCallback(SceUID callback, SceUInt32 mask);
int avConfigNotifyCallback(int events);
int avConfigWriteMasterVolume(unsigned int volume);
int avConfigReadMasterVolume(int *volume);
int avConfigSetSuspendInterval(unsigned int interval);
int avConfigSetDimmingInterval(unsigned int interval);
int avConfigSetDisplayColorSpaceMode(int mode);
int avConfigSetRoutingVolume(int volume);
int avConfigApplyHdmiScalingRatio(const AvConfigScalingRatio *ratio);

int avConfigReadBtEvents(AvConfigState *state);
void avConfigUpdateBtVolume(int notified, unsigned int volume);
void avConfigReadBtDevice(AvConfigBtAudioState *bt, SceUInt64 address);
void avConfigSelectBtProfile(AvConfigBtAudioState *bt,
			   const AvConfigSettings *settings);
int avConfigStartBtAudio(AvConfigBtAudioState *bt);
int avConfigStopBtAudio(AvConfigBtAudioState *bt);
int avConfigSendBtVolume(SceUInt64 address, unsigned int volume);
int avConfigSendBtButton(AvConfigBtAudioState *bt, unsigned int button);
int avConfigSetEventFlag(unsigned int bits);
int avConfigClearCecInfo(void);
int avConfigGetCecInfo(CecStatistics *statistics);
int avConfigGetMonitorInfo(AvConfigMonitorInfo *info);
void avConfigGetVolumeControlState(int *mode, int *muted);
int avConfigHdmiUpdateMonitor(void);
int avConfigHdmiDisableHead(void);
int avConfigHdmiWaitHdcp(int *state);
int avConfigHdmiSetInitialOutput(void);
int avConfigStopBtSend(void);
int avConfigStopBtReceive(void);
int avConfigStartBtSend(SceUInt64 address, unsigned int flags);
int avConfigStartBtReceive(SceUInt64 address, unsigned int flags, int sampleRate);
int avConfigInitializeBtTransfer(void);
int avConfigGetHdmiAudioMode(void);
int avConfigGetHdmiPowerState(void);
int avConfigIsHdmiAvailable(void);
int avConfigHdmiConfigureCec(void);
int hdmiSetOutput(int vic, unsigned int rangeMode, int flags);
int avConfigSetHdmiScalingRatio(const AvConfigScalingRatio *ratio);
int avConfigGetHdmiScalingRatio(AvConfigScalingRatio *ratio);
