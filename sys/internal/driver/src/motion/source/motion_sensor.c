/* Register transfers for the directly attached motion sensor. */
#include "motion_command.h"
#include <psp2kern/kernel/threadmgr.h>
#include <sce/syscon.h>
#include <sce/threadmgr.h>
#include <sce/clockgen.h>
#include <string.h>

typedef struct MotionSensorFifoSample {
	uint8_t accel[3][2];
	uint8_t gyro[3][2];
} MotionSensorFifoSample;

_Static_assert(sizeof(MotionSensorFifoSample) == 12, "sensor FIFO sample size");

typedef struct MotionSensorRegisterSetting {
	uint8_t reg;
	uint8_t value;
} MotionSensorRegisterSetting;

typedef struct MotionSensorTransferContext {
	MotionSensorPacket request;
	MotionSensorPacket response;
	const uint8_t *sampleData;
	union {
		uint32_t status;
		struct {
			uint8_t deviceId;
			uint8_t flags;
			uint16_t reserved;
		} state;
	};
	MotionSensorStatistics statistics;
	uint32_t accelSum[3];
	uint32_t statusSum;
	uint32_t startTime;
	uint16_t sequence;
	uint16_t accelCount;
	uint16_t statusCount;
	uint16_t emptyCount;
} MotionSensorTransferContext;

typedef struct MotionSensorWorkerContext {
	MotionSensorStatistics sensorStatistics;
	MotionMagnStatistics magnStatistics;
	int thread;
	int event;
	uint8_t reserved[64];
	void (*completion)(int result, void *arg);
	SceMotionDevData *samples;
	MotionCalibrationBuffer *calibration;
	SceMotionDevDeviceInfo *deviceInfo;
	uint32_t mode;
	uint32_t command;
	uint32_t calibrationSlot;
	uint8_t magnPoll;
	uint8_t magnPending;
	uint8_t suspended;
	uint8_t reserved1;
} MotionSensorWorkerContext;

_Static_assert(sizeof(MotionSensorWorkerContext) == 176,
	"sensor worker context size");
_Static_assert(offsetof(MotionSensorWorkerContext, completion) == 144 &&
	offsetof(MotionSensorWorkerContext, calibrationSlot) == 168 &&
	offsetof(MotionSensorWorkerContext, suspended) == 174,
	"sensor worker member layout");
_Static_assert(offsetof(SceMotionDevDeviceInfo, direct.sensorStatus) == 14 &&
	offsetof(SceMotionDevDeviceInfo, direct.magnetometer) == 18,
	"direct sensor device info layout");

static const MotionSensorRegisterSetting motionSensorSettings[] = {
	{ .reg = 0x6a, .value = 0x10 },
	{ .reg = 0x1a, .value = 0x03 },
	{ .reg = 0x1b, .value = 0x18 },
	{ .reg = 0x1d, .value = 0x03 },
	{ .reg = 0x1c, .value = 0x08 },
	{ .reg = 0x19, .value = 0x01 },
	{ .reg = 0x6c, .value = 0x00 },
	{ .reg = 0x37, .value = 0x00 },
	{ .reg = 0x38, .value = 0x00 },
	{ .reg = 0x23, .value = 0x78 },
	{ .reg = 0x6a, .value = 0x55 },
	{ .reg = 0, .value = 0 },
};

static MotionSensorTransferContext motionSensorTransfer;
static MotionSensorWorkerContext motionSensorWorker;

_Static_assert(sizeof(MotionSensorRegisterSetting) == 2,
	"sensor register setting size");
_Static_assert(sizeof(MotionSensorStatistics) == 44, "sensor statistics size");
_Static_assert(sizeof(MotionSensorTransferContext) == 344,
	"sensor context size");
_Static_assert(offsetof(MotionSensorTransferContext, sampleData) == 264 &&
	offsetof(MotionSensorTransferContext, statistics) == 272 &&
	offsetof(MotionSensorTransferContext, accelSum) == 316 &&
	offsetof(MotionSensorTransferContext, sequence) == 336,
	"sensor context member layout");

int motionVerifySensorRegisters(void)
{
	const MotionSensorRegisterSetting *setting = &motionSensorSettings[1];
	int reg = 0x6a;
	int result;

	for (;;) {
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = reg | 0x80;
		result = motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		if (result < 0)
			return result;
		if (reg == 0x6a) {
			if (motionSensorTransfer.response.data[1] != 0x50)
				return -1;
		} else if (motionSensorTransfer.response.data[1] != setting[-1].value) {
			return -1;
		}
		reg = setting++->reg;
		if (reg == 0)
			break;
	}
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.size = 2;
	motionSensorTransfer.request.data[0] = 0xeb;
	result = motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	if (result >= 0 && motionSensorTransfer.response.data[1] != 0x41)
		return -1;
	return result;
}

int motionInitializeSensor(void)
{
	int result;

	memset(&motionSensorTransfer, 0, sizeof(motionSensorTransfer));
	motionSensorTransfer.sampleData = &motionSensorTransfer.response.data[1];
	motionSensorTransfer.state.flags = 1;
	sceKernelDelayThread(100000);
	if (motionSpiInitialize(3, 200, 128, 1) < 0) {
		motionSensorTransfer.state.flags |= 2;
		goto failed;
	}
	for (;;) {
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.data[1] = 1;
		motionSensorTransfer.request.data[0] = 0x6b;
		motionSensorTransfer.request.size = 2;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		sceKernelDelayThread(5000);
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.data[0] = 0xf5;
		motionSensorTransfer.request.size = 2;
		if (motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response) < 0)
			motionSensorTransfer.state.flags |= 2;
		motionSensorTransfer.state.deviceId = *motionSensorTransfer.sampleData;
		if (motionSensorTransfer.state.deviceId == 0xaf) {
			const MotionSensorRegisterSetting *setting = motionSensorSettings;
			uint8_t reg = 0x6a;

			for (;;) {
				uint8_t value = setting->value;

				memset(&motionSensorTransfer.request, 0,
					sizeof(motionSensorTransfer.request));
				memset(&motionSensorTransfer.response, 0,
					sizeof(motionSensorTransfer.response));
				motionSensorTransfer.request.data[1] = value;
				motionSensorTransfer.request.size = 2;
				motionSensorTransfer.request.data[0] = reg;
				if (motionSpiTransfer(&motionSensorTransfer.request,
					&motionSensorTransfer.response) < 0) {
					motionSensorTransfer.state.flags |= 2;
					break;
				}
				setting++;
				reg = setting->reg;
				if (reg == 0)
					break;
			}
			memset(&motionSensorTransfer.request, 0,
				sizeof(motionSensorTransfer.request));
			memset(&motionSensorTransfer.response, 0,
				sizeof(motionSensorTransfer.response));
			motionSensorTransfer.request.data[1] = 0x41;
			motionSensorTransfer.request.size = 2;
			motionSensorTransfer.request.data[0] = 0x6b;
			motionSpiTransfer(&motionSensorTransfer.request,
				&motionSensorTransfer.response);
			sceKernelDelayThread(5000);
			result = motionVerifySensorRegisters();
			if (result >= 0) {
				motionSensorTransfer.state.flags = 0;
				return result;
			}
			motionSensorTransfer.statistics.registerFailures++;
			motionSensorTransfer.state.flags |= 4;
			if (motionSensorTransfer.statistics.registerFailures > 1)
				goto failed;
		} else {
			motionSensorTransfer.statistics.identityFailures++;
			motionSensorTransfer.state.flags |= 0x80;
			if (motionSensorTransfer.statistics.identityFailures > 1)
				goto failed;
		}
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x6b;
		motionSensorTransfer.request.data[1] = 0x80;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		sceKernelDelayThread(100000);
	}
failed:
	motionSensorTransfer.state.flags &= 0xfe;
	return (int)0x80360006U;
}

int motionGetSensorStatus(uint32_t *status)
{
	if (status != NULL)
		*status = motionSensorTransfer.status;
	return 0;
}

int motionResetSensorFifo(void)
{
	motionSensorTransfer.accelCount = 0;
	motionSensorTransfer.accelSum[0] = 0;
	motionSensorTransfer.accelSum[1] = 0;
	motionSensorTransfer.accelSum[2] = 0;
	motionSensorTransfer.statusSum = 0;
	motionSensorTransfer.statusCount = 0;
	motionSensorTransfer.startTime = ksceKernelGetSystemTimeLow();
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.data[1] = 1;
	motionSensorTransfer.request.data[0] = 0x6b;
	motionSensorTransfer.request.size = 2;
	motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	sceKernelDelayThread(5000);
	sceKernelDelayThread(95000);
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.size = 2;
	motionSensorTransfer.request.data[0] = 0x23;
	motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.data[1] = 0x10;
	motionSensorTransfer.request.size = 2;
	motionSensorTransfer.request.data[0] = 0x6a;
	motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.data[1] = 0x14;
	motionSensorTransfer.request.size = 2;
	motionSensorTransfer.request.data[0] = 0x6a;
	motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.data[1] = 0x50;
	motionSensorTransfer.request.data[0] = 0x6a;
	motionSensorTransfer.request.size = 2;
	motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.size = 2;
	motionSensorTransfer.request.data[0] = 0x23;
	motionSensorTransfer.request.data[1] = 0x78;
	motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	return 0;
}

int motionStopSensor(void)
{
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.data[0] = 0x6b;
	motionSensorTransfer.request.data[1] = 0x41;
	motionSensorTransfer.request.size = 2;
	motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	sceKernelDelayThread(5000);
	return 0;
}

int motionSuspendSensor(void)
{
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.data[0] = 0x6b;
	motionSensorTransfer.request.data[1] = 0x41;
	motionSensorTransfer.request.size = 2;
	motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	sceKernelDelayThread(5000);
	motionSpiDeactivate();
	return motionSpiSuspend();
}

int motionResumeSensor(void)
{
	int result = motionSpiResume();

	if (result < 0)
		return result;
	motionSpiActivate();
	sceKernelDelayThread(100000);
	for (;;) {
		const MotionSensorRegisterSetting *setting = motionSensorSettings;
		uint8_t reg = 0x6a;

		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x6b;
		motionSensorTransfer.request.data[1] = 1;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		sceKernelDelayThread(5000);
		for (;;) {
			uint8_t value = setting->value;

			memset(&motionSensorTransfer.request, 0,
				sizeof(motionSensorTransfer.request));
			memset(&motionSensorTransfer.response, 0,
				sizeof(motionSensorTransfer.response));
			motionSensorTransfer.request.data[1] = value;
			motionSensorTransfer.request.size = 2;
			motionSensorTransfer.request.data[0] = reg;
			if (motionSpiTransfer(&motionSensorTransfer.request,
				&motionSensorTransfer.response) < 0)
				break;
			setting++;
			reg = setting->reg;
			if (reg == 0)
				break;
		}
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.data[1] = 0x41;
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x6b;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		sceKernelDelayThread(5000);
		result = motionVerifySensorRegisters();
		if (result >= 0)
			return result;
		if (++motionSensorTransfer.statistics.resumeFailures > 1)
			return 0;
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x6b;
		motionSensorTransfer.request.data[1] = 0x80;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		sceKernelDelayThread(100000);
	}
}

int motionReadSensorFifo(SceMotionDevData *data)
{
	const MotionSensorFifoSample *samplesStart;
	int result, fifoSize, count, extra;
	uint8_t samples;
	int index, consumedSamples = 0;

	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.size = 3;
	motionSensorTransfer.request.data[0] = 0xf2;
	result = motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	if (result < 0)
		return result;
	fifoSize = ((motionSensorTransfer.sampleData[0] << 8) |
		motionSensorTransfer.sampleData[1]) & 0x1fff;
	if (fifoSize == 0) {
		if ((uint16_t)(motionSensorTransfer.emptyCount + 1) <= 1)
			motionSensorTransfer.emptyCount++;
		else {
			motionSensorTransfer.emptyCount = 0;
			motionSensorTransfer.statistics.emptyPolls++;
		}
		return 0;
	}
	motionSensorTransfer.emptyCount = 0;
	if ((int16_t)fifoSize >= 512)
		motionSensorTransfer.statistics.fifoFull++;
	samples = (int16_t)fifoSize / 12;
	if (samples > 10) {
		motionSensorTransfer.statistics.fifoOverruns++;
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x23;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.data[1] = 0x10;
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x6a;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.data[1] = 0x14;
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x6a;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.data[1] = 0x50;
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x6a;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		memset(&motionSensorTransfer.request, 0,
			sizeof(motionSensorTransfer.request));
		memset(&motionSensorTransfer.response, 0,
			sizeof(motionSensorTransfer.response));
		motionSensorTransfer.request.data[1] = 0x78;
		motionSensorTransfer.request.size = 2;
		motionSensorTransfer.request.data[0] = 0x23;
		motionSpiTransfer(&motionSensorTransfer.request,
			&motionSensorTransfer.response);
		return 0;
	}
	memset(&motionSensorTransfer.request, 0,
		sizeof(motionSensorTransfer.request));
	memset(&motionSensorTransfer.response, 0,
		sizeof(motionSensorTransfer.response));
	motionSensorTransfer.request.data[0] = 0xf4;
	motionSensorTransfer.request.size = 12 * (int16_t)samples + 1;
	result = motionSpiTransfer(&motionSensorTransfer.request,
		&motionSensorTransfer.response);
	count = samples;
	extra = 0;
	if (samples > 6) {
		extra = samples - 6;
		count = 6;
		samples = 6;
	}
	if (motionSensorTransfer.startTime != 0) {
		uint32_t elapsed = ksceKernelGetSystemTimeLow() -
			motionSensorTransfer.startTime;

		motionSensorTransfer.startTime = 0;
		motionSensorTransfer.sequence += elapsed / 2000;
	}
	data->time = sceKernelGetSystemTimeWide();
	samplesStart = (const MotionSensorFifoSample *)motionSensorTransfer.sampleData;
	for (index = 0; index < count; index++) {
		const MotionSensorFifoSample *sample = &samplesStart[consumedSamples];
		SceMotionDevDataEntry *entry = &data->entry[index];
		uint16_t accel[3], gyro[3];

		if (extra != 0) {
			extra--;
			accel[0] = (uint16_t)(((int16_t)((sample[1].accel[0][0] << 8) |
				sample[1].accel[0][1]) +
				(int16_t)((sample[0].accel[0][0] << 8) |
				sample[0].accel[0][1])) / 2) ^ 0x8000;
			accel[1] = (uint16_t)(((int16_t)((sample[1].accel[1][0] << 8) |
				sample[1].accel[1][1]) +
				(int16_t)((sample[0].accel[1][0] << 8) |
				sample[0].accel[1][1])) / 2) ^ 0x8000;
			accel[2] = (uint16_t)(((int16_t)((sample[1].accel[2][0] << 8) |
				sample[1].accel[2][1]) +
				(int16_t)((sample[0].accel[2][0] << 8) |
				sample[0].accel[2][1])) / 2) ^ 0x8000;
			gyro[0] = (uint16_t)(((int16_t)((sample[1].gyro[0][0] << 8) |
				sample[1].gyro[0][1]) +
				(int16_t)((sample[0].gyro[0][0] << 8) |
				sample[0].gyro[0][1])) / 2) ^ 0x8000;
			gyro[1] = (uint16_t)(((int16_t)((sample[1].gyro[1][0] << 8) |
				sample[1].gyro[1][1]) +
				(int16_t)((sample[0].gyro[1][0] << 8) |
				sample[0].gyro[1][1])) / 2) ^ 0x8000;
			gyro[2] = (uint16_t)(((int16_t)((sample[1].gyro[2][0] << 8) |
				sample[1].gyro[2][1]) +
				(int16_t)((sample[0].gyro[2][0] << 8) |
				sample[0].gyro[2][1])) / 2) ^ 0x8000;
			consumedSamples++;
			entry->sequence = motionSensorTransfer.sequence++;
		} else {
			accel[0] = ((sample->accel[0][0] << 8) |
				sample->accel[0][1]) ^ 0x8000;
			accel[1] = ((sample->accel[1][0] << 8) |
				sample->accel[1][1]) ^ 0x8000;
			accel[2] = ((sample->accel[2][0] << 8) |
				sample->accel[2][1]) ^ 0x8000;
			gyro[0] = ((sample->gyro[0][0] << 8) |
				sample->gyro[0][1]) ^ 0x8000;
			gyro[1] = ((sample->gyro[1][0] << 8) |
				sample->gyro[1][1]) ^ 0x8000;
			gyro[2] = ((sample->gyro[2][0] << 8) |
				sample->gyro[2][1]) ^ 0x8000;
		}
		consumedSamples++;
		motionSensorTransfer.accelSum[0] += accel[0];
		motionSensorTransfer.accelSum[1] += accel[1];
		motionSensorTransfer.accelSum[2] += accel[2];
		motionSensorTransfer.accelCount++;
		if (motionSensorTransfer.accelCount > 4) {
			entry->accel[1] = motionSensorTransfer.accelSum[1] / 5;
			entry->flags |= MOTION_ENTRY_ACCEL;
			entry->accel[0] = motionSensorTransfer.accelSum[0] / 5;
			entry->accel[2] = motionSensorTransfer.accelSum[2] / 5;
			motionSensorTransfer.accelCount = 0;
			motionSensorTransfer.accelSum[2] = 0;
			motionSensorTransfer.accelSum[0] = 0;
			motionSensorTransfer.accelSum[1] = 0;
			motionSensorTransfer.statistics.accelSamples++;
		}
		motionSensorTransfer.statusSum += 18000;
		if (++motionSensorTransfer.statusCount >= 500) {
			entry->flags |= MOTION_ENTRY_STATUS;
			entry->status = motionSensorTransfer.statusSum / 500;
			motionSensorTransfer.statusCount = 0;
			motionSensorTransfer.statusSum = 0;
		}
		entry->gyro[0] = gyro[0];
		entry->gyro[1] = gyro[1];
		entry->gyro[2] = gyro[2];
		entry->flags |= MOTION_ENTRY_GYRO;
		entry->sequence = motionSensorTransfer.sequence++;
		motionSensorTransfer.statistics.samples++;
		entry->timestamp = 2 * entry->sequence;
	}
	data->count = samples;
	data->sampleFormat = 1;
	data->timestampWraps = 2;
	return result;
}

int motionGetSensorStatistics(MotionSensorStatistics *statistics)
{
	if (statistics != NULL)
		__builtin_memcpy(statistics, &motionSensorTransfer.statistics,
			sizeof(*statistics));
	return 0;
}

int motionSensorThread(SceSize args, void *argp)
{
	uint32_t events;
	int result;
	SceMotionDevCalibrationHeader header;

	(void)args;
	(void)argp;
	SceClockgenForDriver_66AC5580();
	SceSysconForDriver_063425AE(1);
	motionInitializeSensor();
	motionMagnInitialize();
	for (;;) {
		void (*completion)(int result, void *arg);

		do {
			result = ksceKernelWaitEventFlag(motionSensorWorker.event,
				0xff, 5, &events, NULL);
		} while (result < 0);
		completion = motionSensorWorker.completion;
		if (events & 1) {
			SceMotionDevDeviceInfo *info = motionSensorWorker.deviceInfo;

			memset(info, 0, sizeof(*info));
			info->firmwareVersion = 0x2500;
			info->hardwareInfo = 0x3000;
			info->protocolVersion = 0x1000;
			motionGetSensorStatus(&info->direct.sensorStatus);
			motionMagnGetDeviceInfo(&info->direct.magnetometer);
			result = 0;
			if (completion != NULL)
				completion(0, NULL);
		}
		if (events & 4) {
			uint32_t slot = motionSensorWorker.calibrationSlot;
			MotionCalibrationBuffer *buffer = motionSensorWorker.calibration;

			if (slot <= 1 && buffer != NULL) {
				buffer->size = 0;
				buffer->received = 0;
				memset(&header, 0, sizeof(header));
				result = SceSysconForDriver_76506A57(slot, 0,
					buffer->data, 12);
				if (result >= 0) {
					result = motionDecodeCalibrationHeader(buffer->data,
						12, &header);
					if (result >= 0) {
						int remaining = 20 * header.recordCount;
						uint32_t offset = 12;

						if ((uint32_t)(remaining + 12) > 512)
							result = (int)0x80360106U;
						else {
							buffer->size = remaining + 12;
							buffer->received = 12;
							while (remaining > 0) {
								int chunk = remaining >= 64 ? 64 : remaining;

								remaining -= chunk;
								result = SceSysconForDriver_76506A57(slot,
									offset, &buffer->data[offset], chunk);
								if (result < 0) {
									buffer->size = 0;
									break;
								}
								offset = buffer->received + chunk;
								buffer->received = offset;
							}
						}
					}
				}
			} else
				result = (int)0x80360001U;
			if (completion != NULL)
				completion(result, NULL);
		}
		if ((events & 2) && completion != NULL)
			completion(result, NULL);
		if (events & 8) {
			int start = 0;

			if (motionSensorWorker.mode <= 1) {
				start = motionResetSensorFifo();
				motionMagnStartSampling();
				motionSensorWorker.magnPoll = 0;
			}
			if (completion != NULL)
				completion(start, NULL);
		}
		if (events & 0x10) {
			int stop = 0;

			if (motionSensorWorker.mode <= 3 &&
				((1U << motionSensorWorker.mode) & 0xb)) {
				stop = motionStopSensor();
				motionMagnCancelMeasurement();
				motionSensorWorker.magnPending = 0;
			}
			if (completion != NULL)
				completion(stop, NULL);
		}
		if (events & 0x20) {
			result = 0;
			if (!motionSensorWorker.suspended) {
				result = motionMagnStop();
				if (result >= 0) {
					result = motionSuspendSensor();
					if (result >= 0) {
						result = SceSysconForDriver_063425AE(0);
						if (result >= 0)
							motionSensorWorker.suspended = 1;
					}
				}
			}
			if (completion != NULL)
				completion(result, NULL);
		}
		if (events & 0x40) {
			result = SceSysconForDriver_063425AE(1);
			if (result >= 0) {
				result = motionMagnReset();
				if (result >= 0) {
					result = motionResumeSensor();
					if (result >= 0)
						motionSensorWorker.suspended = 0;
				}
			}
			if (completion != NULL)
				completion(result, NULL);
		}
		if (events & 0x80) {
			SceMotionDevData *data = motionSensorWorker.samples;

			if (data == NULL) {
				result = (int)0x80360001U;
				goto samples_complete;
			}
			memset(data, 0, sizeof(*data));
			result = motionReadSensorFifo(data);
			if (result < 0)
				goto samples_complete;
			if (motionSensorWorker.mode == 0) {
				if (motionSensorWorker.magnPoll == 0) {
					if (motionSensorWorker.magnPending) {
						result = motionMagnReadSample(data);
						if (result < 0)
							goto samples_complete;
						motionSensorWorker.magnPending = 0;
					}
					result = motionMagnStartMeasurement();
					if (result < 0)
						goto samples_complete;
					motionSensorWorker.magnPending = 1;
				}
				if (++motionSensorWorker.magnPoll > 1)
					motionSensorWorker.magnPoll = 0;
			} else
				motionSensorWorker.magnPending = 0;
			if (motionSensorWorker.mode == 1)
				data->timestampWraps |= 0x10;
	samples_complete:
			if (completion != NULL)
				completion(result, NULL);
		}
	}
}

int motionGetDirectStatistics(void *statistics)
{
	if (statistics != NULL) {
		motionGetSensorStatistics(&motionSensorWorker.sensorStatistics);
		motionMagnGetStatistics(&motionSensorWorker.magnStatistics);
		memcpy(statistics, &motionSensorWorker, 72);
	}
	return 0;
}

int motionSuspendDirect(void)
{
	int suspended = motionSensorWorker.suspended;
	int result = 0;

	if (suspended == 0) {
		result = motionMagnStop();
		if (result >= 0) {
			result = motionSuspendSensor();
			if (result >= 0) {
				result = SceSysconForDriver_063425AE(suspended);
				if (result >= 0)
					motionSensorWorker.suspended = 1;
			}
		}
	}
	return result;
}

int motionInitializeDirect(void)
{
	SceKernelThreadOptParamV1 option;
	int result;

	memset(&motionSensorWorker, 0, sizeof(motionSensorWorker));
	motionSensorWorker.mode = 3;
	result = ksceKernelCreateEventFlag("SceMotionDev", 0x1000, 0, NULL);
	motionSensorWorker.event = result;
	if (result >= 0) {
		option.size = sizeof(option);
		option.attr = 0;
		result = ksceKernelCreateThread("SceMotionDev", motionSensorThread,
			42, 0x2000, 0x10000000, 0x80000,
			(const SceKernelThreadOptParam *)&option);
		motionSensorWorker.thread = result;
		if (result >= 0)
			result = ksceKernelStartThread(result, 0, NULL);
	}
	return result;
}

int motionQueueDirectDeviceInfo(SceMotionDevDeviceInfo *info, void (*completion)(int result, void *arg))
{
	if (completion == NULL)
		return (int)0x80360001U;
	motionSensorWorker.completion = completion;
	motionSensorWorker.deviceInfo = info;
	return ksceKernelSetEventFlag(motionSensorWorker.event, 1);
}

int motionQueueDirectCalibration(uint32_t slot, MotionCalibrationBuffer *buffer, void (*completion)(int result, void *arg))
{
	if (completion == NULL || buffer == NULL)
		return (int)0x80360001U;
	motionSensorWorker.completion = completion;
	motionSensorWorker.calibration = buffer;
	motionSensorWorker.calibrationSlot = slot;
	return ksceKernelSetEventFlag(motionSensorWorker.event, 4);
}

int motionQueueDirectCommand(uint32_t command, void (*completion)(int result, void *arg))
{
	if (completion == NULL)
		return (int)0x80360001U;
	motionSensorWorker.completion = completion;
	motionSensorWorker.command = command;
	return ksceKernelSetEventFlag(motionSensorWorker.event, 2);
}

int motionQueueDirectSamples(SceMotionDevData *data, uint32_t mode, void (*completion)(int result, void *arg))
{
	if (completion == NULL || data == NULL)
		return (int)0x80360001U;
	motionSensorWorker.completion = completion;
	motionSensorWorker.samples = data;
	motionSensorWorker.mode = mode;
	return ksceKernelSetEventFlag(motionSensorWorker.event, 128);
}

int motionQueueDirectStart(uint32_t mode, void (*completion)(int result, void *arg))
{
	if (completion == NULL)
		return (int)0x80360001U;
	motionSensorWorker.completion = completion;
	motionSensorWorker.mode = mode;
	return ksceKernelSetEventFlag(motionSensorWorker.event, 8);
}

int motionQueueDirectStop(void (*completion)(int result, void *arg))
{
	if (completion == NULL)
		return (int)0x80360001U;
	motionSensorWorker.completion = completion;
	motionSensorWorker.mode = 3;
	return ksceKernelSetEventFlag(motionSensorWorker.event, 16);
}

int motionQueueDirectSuspend(void (*completion)(int result, void *arg))
{
	if (completion == NULL)
		return (int)0x80360001U;
	motionSensorWorker.completion = completion;
	motionSensorWorker.mode = 3;
	return ksceKernelSetEventFlag(motionSensorWorker.event, 32);
}

int motionQueueDirectResume(void (*completion)(int result, void *arg))
{
	if (completion == NULL)
		return (int)0x80360001U;
	motionSensorWorker.completion = completion;
	return ksceKernelSetEventFlag(motionSensorWorker.event, 64);
}
