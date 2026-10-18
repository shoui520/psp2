/* Sensor command state and the bounded SPI wire buffers. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <motion/motion_dev.h>
#include <motion/motion_command.h>
#include "magnetometer.h"

typedef struct MotionSensorStatistics {
	uint32_t samples;
	uint32_t accelSamples;
	uint32_t identityFailures;
	uint32_t registerFailures;
	uint32_t resumeFailures;
	uint32_t fifoOverruns;
	uint32_t fifoFull;
	uint32_t emptyPolls;
	uint32_t reserved[3];
} MotionSensorStatistics;

typedef struct MotionDeviceStatistics {
	MotionSensorStatistics sensor;
	MotionMagnStatistics magnetometer;
} MotionDeviceStatistics;

_Static_assert(sizeof(MotionDeviceStatistics) == 72, "motion device statistics size");

typedef struct MotionSensorPacket {
	uint32_t size;
	uint8_t data[128];
} MotionSensorPacket;

/* Decoded calibration-file header and the bounded syscon read buffer. */

typedef struct MotionCalibrationBuffer {
	uint32_t size;
	uint32_t received;
	uint8_t data[512];
} MotionCalibrationBuffer;

typedef struct MotionCalibrationWireRecord {
	uint8_t kindChannel;
	uint8_t attributes[3];
	uint8_t parameterBytes[4][4];
} MotionCalibrationWireRecord;

typedef struct MotionWorkerContext {
	int thread;
	int event;
	uint32_t command;
	union {
		const MotionSensorPacket *packet;
		uint32_t calibration;
		int activeLow;
	} input;
	union {
		MotionSensorPacket *packet;
		MotionCalibrationBuffer *calibration;
	} output;
	uint32_t transferMode;
	uint32_t delay;
	void (*completion)(int result, void *arg);
	void *arg;
} MotionWorkerContext;

typedef struct MotionCommandContext {
	uint16_t busy;
	uint16_t retries;
	MotionSensorPacket request;
	MotionSensorPacket response;
	void (*completion)(int result, void *arg);
	uint32_t resetRequired;
	uint16_t pollCount;
	uint16_t reserved;
	union {
		void *output;
		uint32_t command;
		uint8_t resetCommand;
		const MotionSensorResponse *request;
		const MotionSensorPacket *packet;
	} destination;
	void *auxiliaryOutput;
} MotionCommandContext;

_Static_assert(sizeof(MotionSensorPacket) == 132, "sensor SPI buffer size");
_Static_assert(sizeof(MotionCalibrationBuffer) == 520, "calibration read buffer size");
_Static_assert(sizeof(MotionCalibrationWireRecord) == 20, "calibration wire record size");
_Static_assert(sizeof(MotionWorkerContext) == 36, "sensor worker state size");
_Static_assert(sizeof(MotionCommandContext) == 288, "sensor command state size");
_Static_assert(offsetof(MotionCommandContext, completion) == 268,
	"sensor command completion position");
_Static_assert(offsetof(MotionCommandContext, destination) == 280,
	"sensor command output position");

int motionParseResponse(const MotionSensorPacket *packet,
	MotionSensorResponse *responses, int capacity);
int motionDecodeSamples(const MotionSensorResponse *response,
	SceMotionDevData *data);
int motionEncodeRequest(MotionSensorPacket *packet,
	const MotionSensorResponse *responses, int count);
int motionDecodeCalibrationHeader(const uint8_t *data, int size,
	SceMotionDevCalibrationHeader *header);
int motionDecodeCalibrationRecords(const uint8_t *data, int size,
	const SceMotionDevCalibrationHeader *header, SceMotionDevCalibrationData *records,
	int capacity);
void motionInitReadBlockRequest(MotionSensorResponse *request,
	uint8_t block, uint8_t mode);
void motionInitDeviceInfoRequest(MotionSensorResponse *request);
void motionInitIdentityRequest(MotionSensorResponse *request);
void motionInitSensorControlRequest(MotionSensorResponse *request, int control);
void motionInitResetRequest(MotionSensorResponse *request, int resetCommand);
void motionInitSamplesRequest(MotionSensorResponse *request);
int motionSpiTransfer(const MotionSensorPacket *request,
	MotionSensorPacket *response);
int motionSpiInitialize(int clockDivider, uint16_t delay,
	uint8_t receiveThreshold, uint8_t mode);
int motionSpiSuspend(void);
int motionSpiResume(void);
int motionSpiDeactivate(void);
int motionSpiActivate(void);

int motionPowerOff(void);
int motionCommandInitialize(void);
int motionQueuePowerOff(void (*completion)(int result, void *arg), void *arg);
int motionQueuePowerOn(int activeLow, void (*completion)(int result, void *arg), void *arg);
int motionReadCalibrationBlock(uint32_t block, MotionCalibrationBuffer *buffer, void (*completion)(int result, void *arg), void *arg);
int motionRequestDeviceInfo(SceMotionDevDeviceInfo *info, void (*completion)(int result, void *arg), void *arg);
int motionSendSensorControl(int control, void (*completion)(int result, void *arg), void *arg);
int motionRequestReset(int resetCommand, void (*completion)(int result, void *arg), void *arg);
int motionRequestSensorReset(void (*completion)(int result, void *arg), void *arg);
int motionRequestSamples(SceMotionDevData *data, int resetCommand, void (*completion)(int result, void *arg), void *arg);
int motionQueueDirectDeviceInfo(SceMotionDevDeviceInfo *info, void (*completion)(int result, void *arg));
int motionQueueDirectCalibration(uint32_t slot, MotionCalibrationBuffer *buffer, void (*completion)(int result, void *arg));
int motionQueueDirectCommand(uint32_t command, void (*completion)(int result, void *arg));
int motionQueueDirectSamples(SceMotionDevData *data, uint32_t mode, void (*completion)(int result, void *arg));
int motionQueueDirectStart(uint32_t mode, void (*completion)(int result, void *arg));
int motionQueueDirectStop(void (*completion)(int result, void *arg));
int motionQueueDirectSuspend(void (*completion)(int result, void *arg));
int motionQueueDirectResume(void (*completion)(int result, void *arg));
int motionInitializeDirect(void);
int motionInitializeDevice(void);
int motionGetDirectStatistics(void *statistics);
int motionSuspendDirect(void);

int motionReadDeviceInfo(SceMotionDevDeviceInfo *info, void (*completion)(int result, void *arg), void *arg);
int motionRequestResponse(const MotionSensorResponse *request, MotionSensorResponse *response, void (*completion)(int result, void *arg), void *arg);
int motionRequestPacket(const MotionSensorPacket *request, MotionSensorPacket *response,
	void (*completion)(int result, void *arg), void *arg);
int motionSetActivePolarity(int activeLow);
int motionPowerOn(int activeLow);
int motionSuspendDevice(void);
int motionGetDeviceStatistics(void *statistics);
int motionReleaseReset(int pulse);
int motionReadDeviceSamples(SceMotionDevData *data, uint32_t mode, void (*completion)(int result, void *arg), void *arg);
int motionStartDevice(uint32_t mode, void (*completion)(int result, void *arg), void *arg);
int motionStopDevice(void (*completion)(int result, void *arg), void *arg);
int motionControlDevice(uint32_t control, void (*completion)(int result, void *arg), void *arg);

int motionReadDeviceCalibration(uint32_t slot, MotionCalibrationBuffer *buffer,
	void (*completion)(int result, void *arg), void *arg);
int motionRequestIdentity(SceMotionDevDeviceInfo *info,
	void (*completion)(int result, void *arg), void *arg);

int motionSuspendDeviceAsync(void (*completion)(int result, void *arg), void *arg);
int motionResumeDeviceAsync(int activeLow, void (*completion)(int result, void *arg), void *arg);
