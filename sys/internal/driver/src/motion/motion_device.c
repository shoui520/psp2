/* Hardware selection between command-driven and directly attached sensors. */
#include "motion_command.h"
#include <syscon/syscon.h>

static uint32_t motionHardwareType;

int motionInitializeDevice(void)
{
	motionHardwareType = ksceSysconGetHardwareInfo() & 0xff0000;
	if (motionHardwareType == 0x820000)
		return motionInitializeDirect();
	return motionCommandInitialize();
}

int motionSuspendDeviceAsync(void (*completion)(int result, void *arg), void *arg)
{
	if (motionHardwareType == 0x820000)
		return motionQueueDirectSuspend(completion);
	return motionQueuePowerOff(completion, arg);
}

int motionResumeDeviceAsync(int activeLow, void (*completion)(int result, void *arg), void *arg)
{
	if (motionHardwareType == 0x820000)
		return motionQueueDirectResume(completion);
	return motionQueuePowerOn(activeLow, completion, arg);
}

int motionReadDeviceCalibration(uint32_t slot, MotionCalibrationBuffer *buffer, void (*completion)(int result, void *arg), void *arg)
{
	if (motionHardwareType == 0x820000)
		return motionQueueDirectCalibration(slot, buffer, completion);
	return motionReadCalibrationBlock(slot, buffer, completion, arg);
}

int motionReadDeviceInfo(SceMotionDevDeviceInfo *info, void (*completion)(int result, void *arg), void *arg)
{
	if (motionHardwareType == 0x820000)
		return motionQueueDirectDeviceInfo(info, completion);
	return motionRequestDeviceInfo(info, completion, arg);
}

int motionControlDevice(uint32_t control, void (*completion)(int result, void *arg), void *arg)
{
	if (motionHardwareType == 0x820000)
		return motionQueueDirectCommand(control, completion);
	return motionSendSensorControl(control, completion, arg);
}

int motionStartDevice(uint32_t mode, void (*completion)(int result, void *arg), void *arg)
{
	if (motionHardwareType == 0x820000)
		return motionQueueDirectStart(mode, completion);
	return motionRequestReset(mode, completion, arg);
}

int motionStopDevice(void (*completion)(int result, void *arg), void *arg)
{
	if (motionHardwareType == 0x820000)
		return motionQueueDirectStop(completion);
	return motionRequestSensorReset(completion, arg);
}

int motionReadDeviceSamples(SceMotionDevData *data, uint32_t mode, void (*completion)(int result, void *arg), void *arg)
{
	if (motionHardwareType == 0x820000)
		return motionQueueDirectSamples(data, mode, completion);
	return motionRequestSamples(data, mode, completion, arg);
}

int motionGetDeviceStatistics(void *statistics)
{
	if (motionHardwareType == 0x820000)
		return motionGetDirectStatistics(statistics);
	return 0;
}

int motionSuspendDevice(void)
{
	if (motionHardwareType == 0x820000)
		return motionSuspendDirect();
	return motionPowerOff();
}
