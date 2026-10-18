/* SceCamera private configuration owners. */
#pragma once

#include <psp2kern/types.h>
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
