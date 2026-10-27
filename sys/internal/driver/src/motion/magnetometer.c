/* Compass identification, factory adjustment and self-test. */
#include <string.h>
#include <threadmgr/threadmgr.h>
#include <lowio/gpio.h>
#include <lowio/i2c.h>
#include <motion/motion_dev.h>
#include "magnetometer.h"

#define MAGN_I2C_BUS	1
#define MAGN_I2C_ADDRESS	0x18
#define MAGN_RESET_PORT	28
#define MAGN_ERROR_INIT	((int)0x80360006U)

MotionMagnContext motionMagn;

int motionMagnInitialize(void)
{
	uint8_t adjustmentRegister[8];
	uint8_t statusRegister[7];
	uint8_t identityRegister;
	uint8_t command[2];
	uint8_t sample[10];
	int result, attempts;
	int x, y, z;

	__builtin_memset(&motionMagn, 0, sizeof(motionMagn));
	motionMagn.device.status = 1;
	SceGpioForDriver_372022A4(0, MAGN_RESET_PORT, 0);
	for (;;) {
		sceGpioPortClear(0, MAGN_RESET_PORT);
		sceKernelDelayThread(5);
		sceGpioPortSet(0, MAGN_RESET_PORT);
		sceKernelDelayThread(100);
		identityRegister = 0;
		result = sceI2cMasterTransmitReceive(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
			&identityRegister, 1, MAGN_I2C_ADDRESS,
			(uint8_t *)&motionMagn.device.deviceId, 2);
		if (result < 0) {
			motionMagn.statistics.configurationErrors++;
			motionMagn.device.status |= 2;
			if (motionMagn.statistics.configurationErrors <= 1)
				continue;
			goto initialization_error;
		}
		if (motionMagn.device.deviceId != 0x548) {
			motionMagn.statistics.identificationErrors++;
			motionMagn.device.status |= 0x80;
			if (motionMagn.statistics.identificationErrors <= 1)
				continue;
			goto initialization_error;
		}
		command[0] = 0x31;
		command[1] = 0x1f;
		result = sceI2cMasterTransmit(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
			command, 2);
		sceKernelDelayThread(100);
		if (result < 0)
			goto configuration_error;
		adjustmentRegister[0] = 0x60;
		result = sceI2cMasterTransmitReceive(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
			adjustmentRegister, 1, MAGN_I2C_ADDRESS,
			motionMagn.device.sensitivity, 3);
		if (result < 0)
			goto configuration_error;
		command[0] = 0x31;
		command[1] = 0;
		result = sceI2cMasterTransmit(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
			command, 2);
		sceKernelDelayThread(100);
		if (result >= 0)
			break;
	configuration_error:
		motionMagn.statistics.configurationErrors++;
		motionMagn.device.status |= 2;
		if (motionMagn.statistics.configurationErrors > 1)
			goto initialization_error;
	}
	__builtin_memset(sample, 0, sizeof(sample));
	command[0] = 0x31;
	command[1] = 0;
	sceI2cMasterTransmit(MAGN_I2C_BUS, MAGN_I2C_ADDRESS, command, 2);
	sceKernelDelayThread(100);
	command[0] = 0x31;
	command[1] = 0x10;
	sceI2cMasterTransmit(MAGN_I2C_BUS, MAGN_I2C_ADDRESS, command, 2);
	sceKernelDelayThread(100);
	sceKernelDelayThread(8000);
	attempts = 6;
	do {
		statusRegister[0] = 0x10;
		result = sceI2cMasterTransmitReceive(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
			statusRegister, 1, MAGN_I2C_ADDRESS, sample, 1);
		if (result < 0)
			goto read_error;
		if (--attempts == 0) {
			motionMagn.statistics.timeouts++;
			goto self_test_error;
		}
	} while (!(sample[0] & 1));
	command[0] = 0x11;
	result = sceI2cMasterTransmitReceive(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
		command, 1, MAGN_I2C_ADDRESS, sample, 8);
	if (result < 0)
		goto read_error;
	x = ((motionMagn.device.sensitivity[0] + 128) *
		(int16_t)(sample[0] | (sample[1] << 8))) >> 7;
	y = ((motionMagn.device.sensitivity[1] + 128) *
		(int16_t)(sample[2] | (sample[3] << 8))) >> 7;
	z = ((motionMagn.device.sensitivity[2] + 128) *
		(int16_t)(sample[4] | (sample[5] << 8))) >> 7;
	motionMagn.device.selfTest[0] = x;
	motionMagn.device.selfTest[1] = y;
	motionMagn.device.selfTest[2] = z;
	if ((uint16_t)(x + 30) <= 60 && (uint16_t)(y + 30) <= 60 &&
	    (uint16_t)(z + 400) <= 350) {
		motionMagn.device.status = 0;
		return result;
	}
	goto self_test_error;

read_error:
	motionMagn.statistics.readErrors++;
self_test_error:
	motionMagn.device.status |= 8;
initialization_error:
	motionMagn.device.status &= ~1;
	return MAGN_ERROR_INIT;
}

int motionMagnGetDeviceInfo(MotionMagnDeviceInfo *info)
{
	if (info != NULL)
		*info = motionMagn.device;
	return 0;
}

int motionMagnStartSampling(void)
{
	return 0;
}

int motionMagnCancelMeasurement(void)
{
	motionMagn.measuring = 0;
	return 0;
}

int motionMagnStop(void)
{
	motionMagn.measuring = 0;
	sceGpioPortClear(0, MAGN_RESET_PORT);
	return 0;
}

int motionMagnReset(void)
{
	SceGpioForDriver_372022A4(0, MAGN_RESET_PORT, 0);
	sceGpioPortClear(0, MAGN_RESET_PORT);
	sceKernelDelayThread(5);
	sceGpioPortSet(0, MAGN_RESET_PORT);
	sceKernelDelayThread(100);
	return 0;
}

int motionMagnStartMeasurement(void)
{
	uint8_t command[2];
	int result;

	command[0] = 0x31;
	command[1] = 1;
	result = sceI2cMasterTransmit(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
		command, 2);
	sceKernelDelayThread(100);
	if (result >= 0) {
		motionMagn.measuring = 1;
		return result;
	}
	sceGpioPortClear(0, MAGN_RESET_PORT);
	sceKernelDelayThread(5);
	sceGpioPortSet(0, MAGN_RESET_PORT);
	sceKernelDelayThread(100);
	return result;
}

int motionMagnReadSample(SceMotionDevData *data)
{
	uint8_t statusRegister[8];
	uint8_t dataRegister[8];
	uint8_t sample[10];
	SceMotionDevDataEntry *entry;
	int result, attempts, index;

	if (data == NULL)
		return (int)0x80360001U;
	__builtin_memset(sample, 0, sizeof(sample));
	result = motionMagn.measuring;
	if (result == 0)
		return result;
	motionMagn.measuring = 0;
	attempts = 6;
	do {
		statusRegister[0] = 0x10;
		result = sceI2cMasterTransmitReceive(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
			statusRegister, 1, MAGN_I2C_ADDRESS, sample, 1);
		if (result < 0) {
			motionMagn.statistics.readErrors++;
			goto status_error;
		}
		if (--attempts == 0) {
			motionMagn.statistics.timeouts++;
			goto status_error;
		}
	} while (!(sample[0] & 1));
	dataRegister[0] = 0x11;
	result = sceI2cMasterTransmitReceive(MAGN_I2C_BUS, MAGN_I2C_ADDRESS,
		dataRegister, 1, MAGN_I2C_ADDRESS, sample, 8);
	if (result < 0) {
		motionMagn.statistics.readErrors++;
		goto reset;
	}
	index = 0;
	if (data->entry[0].flags & 4)
		index = 0;
	else if (data->entry[1].flags & 4)
		index = 1;
	else if (data->entry[2].flags & 4)
		index = 2;
	else if (data->entry[3].flags & 4)
		index = 3;
	else if (data->entry[4].flags & 4)
		index = 4;
	else if (data->entry[5].flags & 4)
		index = 5;
	entry = &data->entry[index];
	entry->magnetometer[0] = 4 * (sample[0] | (sample[1] << 8));
	entry->magnetometer[1] = 4 * (sample[2] | (sample[3] << 8));
	entry->magnetometer[2] = 4 * (sample[4] | (sample[5] << 8));
	motionMagn.statistics.samples++;
	entry->flags |= 8;
	return result;

status_error:
	result = (int)0x80360107U;
reset:
	sceGpioPortClear(0, MAGN_RESET_PORT);
	sceKernelDelayThread(5);
	sceGpioPortSet(0, MAGN_RESET_PORT);
	sceKernelDelayThread(100);
	return result;
}

int motionMagnGetStatistics(MotionMagnStatistics *statistics)
{
	if (statistics != NULL) {
		statistics->samples = motionMagn.statistics.samples;
		statistics->configurationErrors =
			motionMagn.statistics.configurationErrors;
		statistics->identificationErrors =
			motionMagn.statistics.identificationErrors;
		statistics->readErrors = motionMagn.statistics.readErrors;
		statistics->timeouts = motionMagn.statistics.timeouts;
		statistics->diagnosticCount[0] = motionMagn.statistics.diagnosticCount[0];
		statistics->diagnosticCount[1] = motionMagn.statistics.diagnosticCount[1];
	}
	return 0;
}
