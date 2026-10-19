/* SceCamera private configuration owners. */
#pragma once

#include <scetypes.h>
#include <camera.h>
#include <sysmem/spinlock.h>
#include <threadmgr/threadmgr.h>

typedef struct CameraSensorRegister {
	SceUInt16 address;
	SceUInt8 value;
	SceUInt8 mask;
} CameraSensorRegister;

typedef struct CameraAntiflickerState {
	int detected;
	int applied;
} CameraAntiflickerState;

typedef struct CameraConfig {
	SceSize sizeThis;
	SceUInt16 wFormat;
	SceUInt16 wResolution;
	SceUInt16 wFramerate;
	SceUInt16 wWidth;
	SceUInt16 wHeight;
	SceUInt16 wRange;
	SceUInt16 wPitch;
	SceUInt16 wZoom;
	SceUInt16 wBuffer;
	SceUInt16 reserved;
	SceUInt32 frameTimeout;
	int saturation;
	int brightness;
	int contrast;
	int sharpness;
	int reverse;
	int effect;
	int ev;
	int antiflicker;
	int iso;
	int gain;
	int whiteBalance;
	int backlight;
	int nightMode;
	int noiseReduction;
	int autoControlHold;
	int exposureCeiling;
	int imageQuality;
	SceSize sizeIBase;
	SceSize sizeUBase;
	SceSize sizeVBase;
} CameraConfig;

typedef struct CameraFrameBuffer {
	SceUInt32 paddrIBase;
	SceUInt32 paddrUBase;
	SceUInt32 paddrVBase;
	void *pvIBase;
	void *pvUBase;
	void *pvVBase;
} CameraFrameBuffer;

typedef struct CameraProcess {
	int started;
	int state;
	int activeStatus;
	int alternateFrameBuffers;
	int suspendState;
	SceUID processId;
	CameraConfig config;
	CameraFrameBuffer frameBuffer;
	int rangesLocked;
	SceUInt64 frame;
	SceUInt64 timestamp;
	SceUInt64 previousFrame;
	int requestedSuspendState;
	SceUInt16 wPriority;
	SceUInt16 reserved;
	union {
		SceUInt8 sensorRegisterValues[20];
		SceUInt32 sensorRegisterWords[5];
	};
	SceUInt8 reservedTail[44];
} CameraProcess;

typedef struct CameraAttributeOption {
	SceUInt16 wRange;
	SceUInt16 reserved;
	int deferRegisterUpdate;
	int updateDevice;
	CameraProcess *process;
} CameraAttributeOption;

_Static_assert(sizeof(CameraAttributeOption) == 16, "Camera attribute option size");

typedef struct CameraDevice {
	SceUID eventFlagId;
	SceUID mutexId;
	SceKernelEventFlagOptParam eventFlagOption;
	int devnum;
	int isUserThread;
	int sensorType;
	int (*initSensor)(int devnum);
	int (*resetSensor)(int devnum);
	int (*setFormat)(int devnum, int format);
	int (*setResolution)(int devnum, int resolution, int framerate);
	int (*setFramerate)(int devnum, int framerate, int resolution);
	int (*setRange)(int devnum, int range);
	int (*getAttribute)(int devnum, int attribute, int *value, int flags,
			    void *option);
	int (*setAttribute)(int devnum, int attribute, int value, int flags,
			    void *option);
	int (*writeRegister)(int devnum, SceUInt16 address, SceUInt8 value);
	int (*readRegister)(int devnum, SceUInt16 address, SceUInt8 *value);
	int (*writeRegisters)(int devnum, const void *registers, unsigned int count);
	int (*setPowerState)(int devnum, int enabled);
	int (*getCsiFormat)(int devnum, int framerate, int format);
	int (*getExposure)(int devnum, int framerate, unsigned int *exposure);
	int (*getRaw8Format)(int devnum, int format, unsigned int *value);
	int (*hasColorChange)(int devnum);
	int (*recoverSensor)(int devnum);
	int (*getSensorRegisters)(int devnum, SceUInt8 *values, int defaults);
	int (*setSensorRegisters)(int devnum, const SceUInt8 *values);
	SceUInt32 reservedCallbacks;
	SceUID activeProcessId;
	CameraConfig config;
	SceUInt64 frame;
	SceUInt64 timestamp;
	SceUInt64 lastExposureSampleTime;
	SceUInt64 previousReadFrame;
	SceUInt64 lastColorCountSampleTime;
	SceUInt32 exposureTime;
	SceUInt32 exposureGap;
	SceUInt32 raw8Format;
	SceUInt32 reservedExposure;
	CameraFrameBuffer cachedFrameBuffer;
	CameraFrameBuffer frameBuffer[3];
	SceUInt16 irqFrameIndex;
	SceUInt16 chromaAligned;
	SceUInt32 reservedFrame;
	int waitingForReverseFrame;
	int suspendState;
	int requestedSuspendState;
	SceUInt32 reservedSuspend;
	SceUInt64 irqFrameCounter;
	union {
		SceUInt8 sensorRegisterValues[20];
		SceUInt32 sensorRegisterWords[5];
	};
	SceUInt8 reservedSensor[44];
	int busy;
	int readInProgress;
	SceUID pendingProcessId;
	SceUInt32 reservedTail;
} CameraDevice;

typedef struct CameraDriver {
	int initialized;
	SceKernelSpinlock spinlock;
	SceUID mutexId;
	int processLocalStorageKey;
	int gpioPowerEnabled;
	SceUInt32 hardwareInfo;
	int busClockFast;
	int clockEnabled;
	int i2cEnabled;
	int suspendState;
	SceFVector3 location[2];
	SceUID eventFlagId;
	SceUID threadId;
	SceKernelEventFlagOptParam eventFlagOption;
	SceKernelThreadOptParamV1 threadOption;
	SceUInt32 reserved[2];
	SceUID pendingSuspendProcessId;
	SceUID activeProcessId;
	SceUID queuedActiveProcessId;
	int processSwitchInProgress;
	SceUID callbackId;
	int powerEnabled;
	int activeEnabled;
	int powerEventState;
	int activeEventState;
	CameraDevice device[2];
} CameraDriver;

typedef struct CameraProcessRegistry {
	unsigned int count;
	SceUID processId[16];
} CameraProcessRegistry;

extern CameraProcessRegistry cameraProcessRegistry;
extern CameraDriver cameraDriverStorage;
extern CameraDriver *cameraDriver;
extern void *cameraFrameBufferBase[2][3];
extern void *cameraAlternateFrameBufferBase[2][3];
extern SceUInt64 cameraLastFrameTimestamp[2];
extern int cameraAllowSimultaneousDevices;
extern int cameraMeasuredFramerate[2];
extern SceUInt64 cameraPreviousFrameTimestamp[2];
extern void *cameraSramFrameBufferBase[2][3];

/* camera.c */
/* name unknown; same user wrapper family as SceCameraDummy */
int cameraSetAlternateFrameBuffers(unsigned int mode);
int SceCameraForDriver_E487870A(int devnum, SceCameraInfo *pInfo);
int SceCameraForDriver_2D12FDF3(int devnum, SceCameraRead *read);
int SceCameraForDriver_EE3D4B1C(int devnum, SceFVector3 *pLocation);

/* camera_config.c */
SceUID cameraGetCallerProcessId(void);
CameraProcess *cameraGetProcess(int devnum, SceUID processId);
int cameraInitializeConfig(void);
int cameraInitializeProcess(SceUID processId);
int cameraRegisterProcess(SceUID processId);
int cameraUnregisterProcess(SceUID processId);
CameraProcess *cameraGetProcessStorage(SceUID processId, int create);
int sceCameraConfigOpen(int devnum, SceCameraInfo *pInfo);
int sceCameraConfigStartPid(int devnum, SceUID processId, int force);
int sceCameraConfigStopPid(int devnum, SceUID processId);
int cameraHasOpenProcess(int devnum);
int cameraHasStartedProcess(int devnum, int activeOnly);
int sceCameraConfigSetStartedPid(int devnum, SceUID processId, int started);
int sceCameraConfigSetActiveStatusPid(int devnum, SceUID processId, int status);
int sceCameraConfigClosePid(int devnum, SceUID processId);
int cameraResetConfig(void);
int sceCameraConfigRead(int devnum, SceCameraRead *pRead);
int sceCameraConfigAttribute(int devnum, int checkFormat);
int cameraCheckActiveProcess(SceUID processId, int devnum, int currentProcess);
int sceCameraConfigGetAttribute(int devnum, int attribute, int *pValue);
int sceCameraConfigSetAttribute(int devnum, int attribute, int value, int flags, int updateSensor);
int cameraGetProcessDeviceStatus(SceUID processId);
int cameraSetProcessState(SceUID processId, int devnum, int state);
int cameraHasOtherProcess(int devnum, SceUID processId);
int cameraIsProcessActive(int devnum, SceUID processId);
int cameraIsProcessStarted(int devnum, SceUID processId);
int cameraSetProcessSuspendState(int devnum, SceUID processId, int state);
int cameraGetProcessSuspendState(int devnum, SceUID processId, int *state);
int cameraSetProcessRequestedSuspendState(int devnum, SceUID processId, int state);
int cameraGetProcessRequestedSuspendState(int devnum, SceUID processId, int *state);
int cameraAdvanceProcessFrame(int devnum, SceUID processId);
int sceCameraConfigSetProcPrevFrame(int devnum, SceUID processId, SceUInt64 frame);
int cameraRestoreProcessSensorRegisters(int devnum);
SceUInt64 sceCameraConfigGetProcFrame(int devnum, SceUID processId);
SceUInt64 sceCameraConfigGetProcPrevFrame(int devnum, SceUID processId);
SceUInt64 sceCameraConfigGetProcTimestamp(int devnum, SceUID processId);
int cameraSetProcessAlternateFrameBuffers(int devnum, SceUID processId, int alternate);
int cameraGetProcessAlternateFrameBuffers(int devnum, SceUID processId, int *alternate);
int cameraSetProcessRangesLocked(int devnum, SceUID processId, int locked);

/* ov7735.c */
int cameraWriteRegisterOV7735(int devnum, SceUInt16 address, SceUInt8 value);
int cameraReadRegisterOV7735(int devnum, SceUInt16 address, SceUInt8 *value);
int cameraWriteRegistersOV7735(int devnum, const void *registers, unsigned int count);
int cameraProbeOV7735(int devnum);
int cameraSetControlModeOV7735(int devnum, int command, int mode);
int cameraGetProbeResultOV7735(int devnum, int command, SceUInt16 *result);
int cameraInitializeOV7735(int devnum);
int cameraResetOV7735(int devnum);
int cameraSetFormatOV7735(int devnum, int format);
int cameraSetFramerateOV7735(int devnum, int framerate, int resolution);
int cameraSetResolutionOV7735(int devnum, int resolution, int framerate);
int cameraSetRangeOV7735(int devnum, int range);
int cameraSetPowerStateOV7735(int devnum, int enabled);
int cameraGetAttributeOV7735(int devnum, int attribute, int *value, int flags, void *option);
int cameraGetCsiFormatOV7735(int devnum, int framerate, int format);
int sceCameraGetExposureTimeOV7735(int devnum, int framerate, unsigned int *exposure);
int cameraGetRaw8FormatOV7735(int devnum, int format, unsigned int *value);
int cameraHasColorChangeOV7735(int devnum);
int cameraSetAttributeOV7735(int devnum, int attribute, int value, int flags, void *argument);
int cameraRecoverOV7735(int devnum);
int cameraGetSensorRegistersOV7735(int devnum, SceUInt8 *values, int defaults);
int cameraSetSensorRegistersOV7735(int devnum, const SceUInt8 *values);
