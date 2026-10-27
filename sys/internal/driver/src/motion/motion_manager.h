/* Motion device lifecycle, per-process readers and calibration state. */
#pragma once

#include "motion_command.h"
#include <motion/motion_dev.h>

/* The two calibration channel groups each contain two three-axis values. */
typedef struct SceMotionDevMagnCalibData {
	uint32_t generation;
	float parameters[2][2][3];
} SceMotionDevMagnCalibData;

typedef struct SceMotionDevDeviceLocation {
	float sensorPosition[2][3];
	uint8_t reserved[24];
} SceMotionDevDeviceLocation;

/* The hardware device has one reader per process. */
typedef struct MotionDeviceReader {
	SceUID pid;
	int reader;
	int stopped;
	int sampling;
	uint8_t reserved[3];
	uint8_t rebase;
	uint32_t lastTimestamp;
	uint32_t timestampOffset;
} MotionDeviceReader;

typedef struct MotionPacketCompletion {
	void (*completion)(int result, const MotionSensorPacket *request,
		MotionSensorPacket *response, void *arg);
	const MotionSensorPacket *request;
	MotionSensorPacket *response;
	void *arg;
} MotionPacketCompletion;

typedef struct MotionUpdateState {
	SceUID event;
	void (*completion)(int result, const MotionSensorResponse *request,
		MotionSensorResponse *response, void *arg);
	uint8_t reserved[132];
	const MotionSensorResponse *request;
	MotionSensorResponse *response;
	void *arg;
} MotionUpdateState;

_Static_assert(sizeof(MotionUpdateState) == 152, "motion update state size");
_Static_assert(offsetof(MotionUpdateState, request) == 140,
	"motion update request");

typedef struct MotionManagerState {
	int state;
	int savedState;
	uint32_t suspendState;
	int initState;
	int overrideSampling;
	SceMotionDevDeviceInfo deviceInfo;
	SceMotionDevCalibrationHeader calibrationHeaders[3];
	SceMotionDevCalibrationData *calibrationRecords[3];
	MotionRing *ring;
	int lock;
	uint32_t baryonVersion;
	uint32_t mode;
	SceUID timer;
	uint32_t period;
	int lastSequence;
	MotionDeviceReader processes[16];
	SceUID sysEventHandler;
	SceUID procEventHandler;
	SceMotionDevMagnCalibData factoryMagnCalibration;
	SceMotionDevMagnCalibData currentMagnCalibration;
	SceMotionDevGyroCalibData gyroCalibration;
	SceMotionDevAccCalibData accCalibration;
	SceMotionDevGyroBiasData gyroBias;
	GyroFilter filter;
	SceUID owner;
	uint8_t reserved[8];
	int samplingDisabled;
	uint32_t magnStabilityLevel;
	SceMotionDevDeviceLocation deviceLocation;
	int allowGyroControl;
} MotionManagerState;

_Static_assert(sizeof(SceMotionDevMagnCalibData) == 52,
	"magnetometer calibration size");
_Static_assert(sizeof(SceMotionDevDeviceLocation) == 48,
	"device location size");
_Static_assert(sizeof(MotionDeviceReader) == 28, "hardware reader size");
_Static_assert(sizeof(MotionPacketCompletion) == 16, "packet completion size");
_Static_assert(sizeof(MotionManagerState) == 0x474, "motion manager size");
_Static_assert(offsetof(MotionManagerState, ring) == 0x70, "manager ring");
_Static_assert(offsetof(MotionManagerState, processes) == 0x8c,
	"manager processes");
_Static_assert(offsetof(MotionManagerState, filter) == 0x398, "manager filter");
_Static_assert(offsetof(MotionManagerState, owner) == 0x42c, "manager owner");

void motionInitializeComplete(int result, void *arg);

int motionCreateHeap(void);
int motionInitializeManager(void *arg);

/* motion_manager.c */
int sceMotionDevGetCalibrationHeader(unsigned int block, SceMotionDevCalibrationHeader *header);
int sceMotionDevGetCalibrationData(unsigned int block, SceMotionDevCalibrationData *data,
	unsigned int count);
int motionRead(SceMotionDevData *data, int count, unsigned int *info);
int motionReadForMagnCalib(SceMotionDevData *data, int count);
int motionMagnSamplingStart(void);
int motionMagnSamplingStop(void);
int motionUpdateMagnCalibData(const SceMotionDevMagnCalibData *data, uint32_t *tag);
int motionGetCurrentMagnCalibData(SceMotionDevMagnCalibData *data);
int motionGetFactoryMagnCalibData(SceMotionDevMagnCalibData *data);
int motionUpdateMagnStabilityLevel(uint32_t level);
int motionGetCurrentMagnStabilityLevel(uint32_t *level);
int motionGetGyroCalibData(SceMotionDevGyroCalibData *data);
int motionGetAccCalibData(SceMotionDevAccCalibData *data);
int motionGetGyroBias(SceMotionDevGyroBiasData *bias);
int motionGetDeviceLocation(SceMotionDevDeviceLocation *location);
uint32_t motionGetMeasMode(uint8_t *mode);
int motionGetControllerType(int port, uint8_t *type);
int sceMotionDevGetEvaInfo(void *statistics);
